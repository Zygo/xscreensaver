/* -*- mode: c; tab-width: 4; fill-column: 128 -*- */
/* vi: set ts=4 tw=128: */

/*
aligned_malloc.c, Copyright (c) 2014 Dave Odell <dmo2118@gmail.com>

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.  No representations are made about the suitability of this
software for any purpose.  It is provided "as is" without express or
implied warranty.
*/

#if HAVE_CONFIG_H
#	include "config.h"
#endif

#include "aligned_malloc.h"

#include <stddef.h>
#include <stdlib.h>

#include <assert.h>
#include <errno.h>

/* Might be changed by thread_util.c:threads_available()
 */
unsigned int aligned_malloc_default_alignment = sizeof(void *);


#if HAVE_UNISTD_H
#	include <unistd.h>
#endif

#if defined __MACH__ && defined __APPLE__ /* OS X, iOS */
#	include <sys/sysctl.h>
#	include <inttypes.h>
#endif

#define IS_POWER_OF_2(x) ((x) > 0 && !((x) & ((x) - 1)))

/*
   arraysize(a). Also known as countof(x), XtNumber(x), NELEMS(x), LEN(x),
   NUMOF(x), ARRAY_SIZE(x), etc., since the fine folks behind C never got
   around to including this incredibly useful macro in the standard library,
   which is where it belongs.

   Much of the code here assumes that multiple processors in a system all use
   the same cache line size...which might be wrong on occasion.
*/

#define arraysize(a) (sizeof(a) / sizeof(*(a)))
#define arrayend(a) ((a) + arraysize(a))

/*
These numbers are from:
- Linux: arch/(arch name)/include/asm/cache.h, note
  L1_CACHE_BYTES/L1_CACHE_SHIFT/SMP_CACHE_BYTES.
- FreeBSD: sys/(sys name)/include/param.h, note
  CACHE_LINE_SHIFT/CACHE_LINE_SIZE.

Preprocessor symbols come from:
- TARGET_CPU_CPP_BUILTINS() in the GNU C preprocessor
  <http://code.ohloh.net/?s=%22TARGET_CPU_CPP_BUILTINS%22&fp=304413>
- http://predef.sourceforge.net/
*/

/*
Several architectures need preprocessor symbols.

Qualcomm Hexagon: 1 << 5
Imagination Technologies META: 1 << 6
OpenRISC: 16 (Linux has the cache line size as a todo.)
Unicore: 1 << 5
*/

#if defined __MACH__ && defined __APPLE__ /* OS X, iOS */
#	include <TargetConditionals.h> /* For TARGET_OS_IPHONE. */
#	ifdef TARGET_OS_IPHONE
#		define _CACHE_LINE_SIZE 64
#	endif
#endif

#if defined __FreeBSD__ && !defined _CACHE_LINE_SIZE
#	include <machine/param.h>
#	ifdef CACHE_LINE_SIZE
#		define _CACHE_LINE_SIZE CACHE_LINE_SIZE
#	endif
#endif

#if !defined _CACHE_LINE_SIZE
#	if defined __alpha || defined __alpha__
/* DEC Alpha */
#		define _CACHE_LINE_SIZE 64 /* EV6 and above. EV4 and EV5 use 32 bytes. */
#	elif defined __arm__
/* ARM architecture */
#		define _CACHE_LINE_SIZE (1 << 6)
#	elif defined __AVR || defined __AVR__
/* Atmel AVR32 */
#		define _CACHE_LINE_SIZE (1 << 5)
#	elif defined __bfin || defined __BFIN__
/* Analog Devices Blackfin */
#		define _CACHE_LINE_SIZE (1 << 5)
#	elif defined _TMS320C6X || defined __TMS320C6X__
/* Texas Instruments TMS320C6x */
#		define _CACHE_LINE_SIZE (1 << 7) /* From L2. L1 data cache line is 1 << 6. */
#	elif defined __cris
/* Axis Communications ETRAX CRIS */
#		define _CACHE_LINE_SIZE 32
#	elif defined __ia64__ || defined _IA64
/* Intel Itanium */
#		define _CACHE_LINE_SIZE (1 << 7)
#	elif defined __M32R__ || defined __m32r__
/* Mitsubishi/Renesas M32R */
#		define _CACHE_LINE_SIZE (1 << 4)
#	elif defined __m68k__ || defined M68000 || defined __MC68K__
/* Motorola 68000 */
#		define _CACHE_LINE_SIZE (1 << 4)
#	elif defined __MICROBLAZE__ || defined __microblaze__
/* Xilinx MicroBlaze */
#		define _CACHE_LINE_SIZE (1 << 5)
#	elif defined __mips__ || defined __mips || defined __MIPS__
/* MIPS */
#		define _CACHE_LINE_SIZE (1 << 6)
#	elif defined __mn10300__ || defined __MN10300__
/* Matsushita/Panasonic MN103 */
#		define _CACHE_LINE_SIZE 32 /* MN103E010 has 16 bytes. */
#	elif defined __hppa || defined __hppa__
/* Hewlett-Packard PA-RISC */
#		define _CACHE_LINE_SIZE 64 /* PA-RISC 2.0 uses 64 bytes, PA-RISC 1.1 uses 32. */
#	elif defined __powerpc || defined _ARCH_PPC
/* Power Architecture (a.k.a. PowerPC) */
#		define _CACHE_LINE_SIZE (1 << 7) /* Linux has a list of PPC models with associated L1_CACHE_SHIFT values. */
#	elif defined __s390__ || defined __370__ || defined __zarch__ || defined __SYSC_ZARCH__
/* IBM System/390 */
#		define _CACHE_LINE_SIZE 256
#	elif defined SUNPLUS || defined __SCORE__ || defined __score__
/* Sunplus S+core */
#		define _CACHE_LINE_SIZE (1 << 4)
#	elif defined __sh__
/* Hitachi SuperH */
#		define _CACHE_LINE_SIZE (1 << 5) /* SH3 and earlier used 1 << 4. */
#	elif defined __sparc__ || defined __sparc
/* SPARC */
#		define _CACHE_LINE_SIZE (1 << 7) /* Linux and FreeBSD disagree as to what this should be. */
#	elif defined __tile__
/* Tilera TILE series */
#		define _CACHE_LINE_SIZE (1 << 6) /* TILEPro uses different sizes for L1 and L2. */
#	elif defined __i386 || defined __x86_64
/* x86(-64) */
#		define _CACHE_LINE_SIZE (1 << 7)
#	elif defined __xtensa__ || defined __XTENSA__
/* Cadence Design Systems/Tensilica Xtensa */
#		define _CACHE_LINE_SIZE (1 << 5) /* 1 << 4 on some models. */
#	endif
#endif /* !defined _CACHE_LINE_SIZE */

#if defined __NetBSD__ && !defined _CACHE_LINE_SIZE
/*
NetBSD defines COHERENCY_UNIT to be 32 on MIPS, and 64 for all other platforms -- which is wrong. Still, this is what the kernel
uses; if this value didn't work, the system wouldn't run.
*/
#	include <sys/param.h>
#		ifdef COHERENCY_UNIT
#		define _CACHE_LINE_SIZE COHERENCY_UNIT
#	endif
#endif

#ifndef _CACHE_LINE_SIZE
#	define _CACHE_LINE_SIZE 256 /* Fallback cache line size. */
#endif

static unsigned _get_cache_line_size(void)
{
	/*
	The general idea:
	- Try to get the actual cache line size from the operating system.
	  - In the interest of keeping things simple, this only checks with
        glibc and OS X.
	    - A few other methods that could be added:
	      - Query x86 CPUs directly with the CPUID instruction.
	      - Query various ELF systems through the auxillary vector.
            (Power, Alpha, SuperH)
	      - Query Linux through
            /sys/devices/system/cpu/cpu?/cache/index?/coherency_line_size
            (x86 only, AFAIK)
	      - Query Linux through cache_alignment in /proc/cpuinfo
	      - Query Solaris through PICL.
	- If that fails, return a value appropriate for the current CPU
      architecture.
	- Otherwise, return a sufficiently large number.
	*/

	/*
	sysconf(3) is not a syscall, it's a glibc call that, for cache line sizes,
	uses CPUID on x86 and returns 0 on other platforms. If it were to work on
	most other platforms, it would have to get cache information from the
	kernel, since that information is usually made available by the processor
	only in privileged mode.
	https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/unix/sysv/linux/i386/sysconf.c;hb=HEAD
	*/

	/* uClibc, newlib, dietlibc, musl, Bionic do not have this. */

#	if HAVE_UNISTD_H && ( \
	defined _SC_LEVEL1_DCACHE_LINESIZE || \
	defined _SC_LEVEL2_CACHE_LINESIZE || \
	defined _SC_LEVEL3_CACHE_LINESIZE || \
	defined _SC_LEVEL4_CACHE_LINESIZE)
	{
		static const int names[] =
		{
#		ifdef _SC_LEVEL1_DCACHE_LINESIZE
			_SC_LEVEL1_DCACHE_LINESIZE,
#		endif
#		ifdef _SC_LEVEL2_CACHE_LINESIZE
			_SC_LEVEL2_CACHE_LINESIZE,
#		endif
#		ifdef _SC_LEVEL3_CACHE_LINESIZE
			_SC_LEVEL3_CACHE_LINESIZE,
#		endif
#		ifdef  _SC_LEVEL4_CACHE_LINESIZE
			_SC_LEVEL4_CACHE_LINESIZE
#		endif
		};

		const int *name;
		long result = 0;

		for(name = names; name != arrayend(names); ++name)
		{
			long sysconf_result = sysconf(*name); /* Can return -1 or 0 on
                                                     failure. */

			if(sysconf_result > result)
				result = sysconf_result;
		}

		if(result)
			return result;

		/* Currently, this fails for every platform that isn't x86. Perhaps
           future versions will support other processors? */
	}
#	endif

#	if defined __MACH__ && defined __APPLE__
	{
		uint32_t result; /* sysctl.h says that hw.cachelinesize is a
                            CTLTYPE_INT. */
		size_t size = sizeof(result);
		static const int name[] = {CTL_HW, HW_CACHELINE};

		if(!sysctl((int *)name, 2, &result, &size, NULL, 0)) /* (int *) is for OS X. */
		{
			assert(size == sizeof(result));
			return result;
		};
	}
#	endif

	/* Guess based on the CPU type. */
	return _CACHE_LINE_SIZE;
}

unsigned get_cache_line_size(void)
{
	unsigned result = _get_cache_line_size();
	assert(result >= sizeof(void *));
	assert(IS_POWER_OF_2(result));
	return result;
}

/* aligned_alloc() (C11) or posix_memalign() (POSIX) are other possibilities
   for aligned_malloc().
 */

int aligned_malloc(void **ptr, unsigned alignment, size_t size)
{
	void *block_start;
	ptrdiff_t align1;

    if (alignment == 0)
      alignment = aligned_malloc_default_alignment;

	assert(alignment && !(alignment & (alignment - 1))); /* alignment must be a power of two. */

    align1 = alignment - 1;

	size += sizeof(void *) + align1;
	block_start = malloc(size);
	if(!block_start)
		return ENOMEM;
	*ptr = (void *)(((ptrdiff_t)block_start + sizeof(void *) + align1) & ~align1);
	((void **)(*ptr))[-1] = block_start;
	return 0;
}

void aligned_free(void *ptr)
{
	if(ptr)
		free(((void **)(ptr))[-1]);
}
