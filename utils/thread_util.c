/* -*- mode: c; tab-width: 4; fill-column: 78 -*- */
/* vi: set ts=4 tw=78: */

/*
thread_util.c, Copyright (c) 2014 Dave Odell <dmo2118@gmail.com>

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.  No representations are made about the suitability of this
software for any purpose.  It is provided "as is" without express or
implied warranty.
*/

#include "thread_util.h"

#include "aligned_malloc.h"
#include "resources.h"

#if HAVE_CONFIG_H
#	include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h> /* Only used by thread_memory_alignment(). */
#include <string.h>

#if HAVE_ALLOCA_H
#	include <alloca.h>
#endif

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

#if HAVE_PTHREAD

#	if !HAVE_UNISTD_H
#		error unistd.h must be present whenever pthread.h is.
#	endif

#	if defined __MACH__ && defined __APPLE__ /* OS X, iOS */
#		include <TargetConditionals.h> /* For TARGET_OS_IPHONE. */
#		ifdef TARGET_OS_IPHONE
#			define _CACHE_LINE_SIZE 64
#		endif
#	endif

#	if defined __FreeBSD__ && !defined _CACHE_LINE_SIZE
#		include <machine/param.h>
#		ifdef CACHE_LINE_SIZE
#			define _CACHE_LINE_SIZE CACHE_LINE_SIZE
#		endif
#	endif

#	if !defined _CACHE_LINE_SIZE
#		if defined __alpha || defined __alpha__
/* DEC Alpha */
#			define _CACHE_LINE_SIZE 64 /* EV6 and above. EV4 and EV5 use 32 bytes. */
#		elif defined __arm__
/* ARM architecture */
#			define _CACHE_LINE_SIZE (1 << 6)
#		elif defined __AVR || defined __AVR__
/* Atmel AVR32 */
#			define _CACHE_LINE_SIZE (1 << 5)
#		elif defined __bfin || defined __BFIN__
/* Analog Devices Blackfin */
#			define _CACHE_LINE_SIZE (1 << 5)
#		elif defined _TMS320C6X || defined __TMS320C6X__
/* Texas Instruments TMS320C6x */
#			define _CACHE_LINE_SIZE (1 << 7) /* From L2. L1 data cache line is 1 << 6. */
#		elif defined __cris
/* Axis Communications ETRAX CRIS */
#			define _CACHE_LINE_SIZE 32
#		elif defined __ia64__ || defined _IA64
/* Intel Itanium */
#			define _CACHE_LINE_SIZE (1 << 7)
#		elif defined __M32R__ || defined __m32r__
/* Mitsubishi/Renesas M32R */
#			define _CACHE_LINE_SIZE (1 << 4)
#		elif defined __m68k__ || defined M68000 || defined __MC68K__
/* Motorola 68000 */
#			define _CACHE_LINE_SIZE (1 << 4)
#		elif defined __MICROBLAZE__ || defined __microblaze__
/* Xilinx MicroBlaze */
#			define _CACHE_LINE_SIZE (1 << 5)
#		elif defined __mips__ || defined __mips || defined __MIPS__
/* MIPS */
#			define _CACHE_LINE_SIZE (1 << 6)
#		elif defined __mn10300__ || defined __MN10300__
/* Matsushita/Panasonic MN103 */
#			define _CACHE_LINE_SIZE 32 /* MN103E010 has 16 bytes. */
#		elif defined __hppa || defined __hppa__
/* Hewlett-Packard PA-RISC */
#			define _CACHE_LINE_SIZE 64 /* PA-RISC 2.0 uses 64 bytes, PA-RISC 1.1 uses 32. */
#		elif defined __powerpc || defined _ARCH_PPC
/* Power Architecture (a.k.a. PowerPC) */
#			define _CACHE_LINE_SIZE (1 << 7) /* Linux has a list of PPC models with associated L1_CACHE_SHIFT values. */
#		elif defined __s390__ || defined __370__ || defined __zarch__ || defined __SYSC_ZARCH__
/* IBM System/390 */
#			define _CACHE_LINE_SIZE 256
#		elif defined SUNPLUS || defined __SCORE__ || defined __score__
/* Sunplus S+core */
#			define _CACHE_LINE_SIZE (1 << 4)
#		elif defined __sh__
/* Hitachi SuperH */
#			define _CACHE_LINE_SIZE (1 << 5) /* SH3 and earlier used 1 << 4. */
#		elif defined __sparc__ || defined __sparc
/* SPARC */
#			define _CACHE_LINE_SIZE (1 << 7) /* Linux and FreeBSD disagree as to what this should be. */
#		elif defined __tile__
/* Tilera TILE series */
#			define _CACHE_LINE_SIZE (1 << 6) /* TILEPro uses different sizes for L1 and L2. */
#		elif defined __i386 || defined __x86_64
/* x86(-64) */
#			define _CACHE_LINE_SIZE (1 << 7)
#		elif defined __xtensa__ || defined __XTENSA__
/* Cadence Design Systems/Tensilica Xtensa */
#			define _CACHE_LINE_SIZE (1 << 5) /* 1 << 4 on some models. */
#		endif
#	endif /* !defined _CACHE_LINE_SIZE */

#	if defined __NetBSD__ && !defined _CACHE_LINE_SIZE
/*
NetBSD defines COHERENCY_UNIT to be 32 on MIPS, and 64 for all other platforms -- which is wrong. Still, this is what the kernel
uses; if this value didn't work, the system wouldn't run.
*/
#		include <sys/param.h>
#			ifdef COHERENCY_UNIT
#			define _CACHE_LINE_SIZE COHERENCY_UNIT
#		endif
#	endif

#	ifndef _CACHE_LINE_SIZE
#		define _CACHE_LINE_SIZE 256 /* Fallback cache line size. */
#	endif

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

const pthread_mutex_t mutex_initializer =
#	if defined _GNU_SOURCE && !defined NDEBUG
	PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
#	else
	PTHREAD_MUTEX_INITIALIZER
#	endif
	;

const pthread_cond_t cond_initializer = PTHREAD_COND_INITIALIZER;

static int _has_pthread = 0; /* Initialize when needed. */
static int _cache_line_size = sizeof(void *);

/* This is actually the init function for various things in here. */
int threads_available(Display *dpy)
{
/*	This is maybe not thread-safe, but: this should -- and generally will --
	be called before the program launches its second thread. */

	if(!_has_pthread)
	{
#	if _POSIX_THREADS
		_has_pthread = _POSIX_THREADS;
#	else
		_has_pthread = sysconf(_SC_THREADS);
#	endif

		if(_has_pthread >= 0)
		{
			if(get_boolean_resource(dpy, "useThreads", "Boolean"))
			{
				_cache_line_size = _get_cache_line_size();
				assert(_cache_line_size >= sizeof(void *));
				assert(IS_POWER_OF_2(_cache_line_size));
			}
			else
			{
				_has_pthread = -1;
			}
		}
	}

	return _has_pthread;
}

#endif /* HAVE_PTHREAD */

/*
   hardware_concurrency() -

   Various platforms offer various statistics that look like they should be
   useful: sysconf(_SC_NPROCESSORS_ONLN) (i.e. the number of 'online'
   processors) in particular is available on many Unixes, and is frequently
   used for functions like hardware_concurrency(). But 'online' is somewhat
   ambiguous; it can mean:

  1. The number of CPU cores that are not (temporarily) asleep. (e.g. Android
     can sometimes put cores to sleep if they aren't being used, and this is
     reflected in _SC_NPROCESSORS_ONLN.)

  2. The maximum number of CPU cores that can be provided to this application,
     as currently set by the system administrator.  (2) is the one that
     hardware_concurrency() ultimately needs.
*/

/*
   Shamelessly plagarized from Boost.Thread and Stack Overflow
   <http://stackoverflow.com/q/150355>.  GNU libstdc++ has some of this too,
   see thread::hardware_concurrency() in thread.cc.
   http://gcc.gnu.org/viewcvs/gcc/trunk/libstdc%2B%2B-v3/src/c%2B%2B11/thread.cc?view=markup

   This might not work right on less common systems for various reasons.
*/

#if HAVE_PTHREAD
#	if defined __APPLE__ && defined __MACH__ || \
		defined __FreeBSD__ || \
		defined __OpenBSD__ || \
		defined __NetBSD__ || \
		defined __DragonFly__ || \
		defined __minix

/*
   BSD Unixes use sysctl(3) for this.
   Some BSDs also support sysconf(3) for this, but in each case this was added
   after sysctl(3).
   Linux: sysctl is present, but strongly deprecated.
   Minix uses the NetBSD userspace, so it has both this and sysconf(3).
   QNX: sysctl is present for kern.* and net.*, but it doesn't say anything
   about hw.*
*/

/* __APPLE__ without __MACH__ is OS 9 or earlier. __APPLE__ with __MACH__ is OS X. */

/*
The usual thing to do here is for sysctl(3) to call __sysctl(2).
  http://cvsweb.netbsd.org/bsdweb.cgi/src/lib/libc/gen/sysctl.c?only_with_tag=HEAD
  http://svnweb.freebsd.org/base/head/lib/libc/gen/sysctl.c?view=markup
*/

/*
   OS X: Xcode Instruments (as of Xcode 4; Apple likes to move things like
   this around) can disable CPUs as a debugging tool.
   Instruments -> Preferences... (Command-,) -> General -> Active Processor Cores
   FreeBSD, OpenBSD: It doesn't look like CPUs can be disabled.
   NetBSD: CPUs can be disabled manually through cpuctl(8).
*/

#		include <stddef.h>

/* FreeBSD: sys/sysctl.h needs sys/types.h, but the one doesn't bring the
   other in automatically. */
#		include <sys/types.h>
#		include <sys/sysctl.h>

static unsigned _hardware_concurrency(void)
{
	int count;
	size_t size = sizeof(count);

#		if defined __APPLE__ && defined __MACH__
	/* Apple sez: sysctl("hw.logicalcpu") is affected by the "current power
       management mode", so use hw.logicalcpu_max. */
	/* https://developer.apple.com/library/mac/#documentation/Darwin/Reference/ManPages/man3/sysctl.3.html */
	if(!sysctlbyname("hw.logicalcpu_max", &count, &size, NULL, 0)) /* Preferred on more recent Darwin. */
	{
		assert(size == sizeof(count));
		return count;
	}
#		endif

#		if defined HW_NCPUONLINE
	/* NetBSD has this. */
	{
		static const int name[] = {CTL_HW, HW_NCPUONLINE};
		if(!sysctl(name, 2, &count, &size, NULL, 0))
		{
			assert(size == sizeof(count));
			return count;
		}
	}
#		endif

	{
		static const int name[] = {CTL_HW, HW_NCPU};
		if(!sysctl((int *)name, 2, &count, &size, NULL, 0)) /* (int *) is for OS X. */
		{
			assert(size == sizeof(count));
			return count;
		}
	}

	return 1;
}

#	elif HAVE_UNISTD_H && defined _SC_NPROCESSORS_ONLN

/*
Supported by:
Linux 2.0 was the first version to provide SMP support via clone(2).
  (e)glibc on Linux provides this, which in turn uses get_nprocs().
  get_nprocs in turn uses /sys/devices/system/cpu/online, /proc/stat, or /proc/cpuinfo, whichever's available.
  https://sourceware.org/git/?p=glibc.git;a=blob;f=posix/sysconf.c;hb=HEAD
  https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/unix/sysv/linux/getsysstats.c;hb=HEAD
  Linux usually isn't configured to auto-enable/disable cores.
SunOS (Solaris), sometime between 4.1.3 and 5.5.1.
  This includes all open source derivatives of 5.10. (Illumos, OpenIndiana)
  sysconf(_SC_NPROCESSORS_ONLN) call _sysconfig(2).
  Not sure if CPU power management (enabled by default, see cpupm and
  cpu_deep_idle in power.conf(4)) affects this.
  psradm(1M) can bring up/down CPU cores, which affects
  sysconf(_SC_NPROCESSORS_ONLN).
  http://src.illumos.org/source/xref/illumos-gate/usr/src/lib/libc/port/gen/sysconf.c
  Minix 3.2, at the latest. (This is the first version to support SMP.)
  AIX 7.1, probably earlier.

Also:
Mac OS X apparently has this on 10.5+.
FreeBSD 5.0, NetBSD 5.0 also have this. They both call sysctl(3).
  http://svnweb.freebsd.org/base/head/lib/libc/gen/sysconf.c?view=markup
  http://cvsweb.netbsd.org/bsdweb.cgi/src/lib/libc/gen/sysconf.c?only_with_tag=HEAD

QNX has sysconf(3), but it doesn't have _SC_NPROCESSORS_*.
*/

static unsigned _hardware_concurrency(void)
{
	long count = sysconf(_SC_NPROCESSORS_ONLN);
	return count > 0 ? count : 1;
}

#	else

static unsigned _hardware_concurrency(void)
{
	return 1; /* Fallback for unknown systems. */
}

#	endif
#endif

unsigned hardware_concurrency(Display *dpy)
{
#if HAVE_PTHREAD
	if(threads_available(dpy) >= 0)
		return _hardware_concurrency();
#endif
	return 1;
}

/* thread_memory_alignment() - */

unsigned thread_memory_alignment(Display *dpy)
{
	(void)threads_available(dpy);
#if HAVE_PTHREAD
	return _cache_line_size;
#else
	return sizeof(void *);
#endif
}

/* Thread pool - */

static unsigned _threadpool_count_serial(struct threadpool *self)
{
#if HAVE_PTHREAD
	assert(_has_pthread);
	if(_has_pthread >= 0)
		return self->count ? 1 : 0;
#endif
	return self->count;
}

static void _serial_destroy(struct threadpool *self)
{
	void *thread = self->serial_threads;
	unsigned i, count = _threadpool_count_serial(self);

	for(i = 0; i != count; ++i)
	{
		self->thread_destroy(thread);
		thread = (char *)thread + self->thread_size;
	}

	free(self->serial_threads);
}

#if HAVE_PTHREAD

static void _parallel_abort(struct threadpool *self)
{
	assert(self->count > 1);
	self->count = self->parallel_unfinished + 1 /* The '+ 1' should technically be _threadpool_count_serial(self). */;
	PTHREAD_VERIFY(pthread_cond_broadcast(&self->cond));
}

struct _parallel_startup_type
{
	struct threadpool *parent;
	int (*thread_create)(void *self, struct threadpool *pool, unsigned id);
	int last_errno;
};

static unsigned _threadpool_count_parallel(struct threadpool *self)
{
	assert(_has_pthread);
	assert(self->count >= 1);
	return self->count - 1 /* The '- 1' should technically be _threadpool_count_serial(self). */;
}

static void *_start_routine(void *startup_raw);

/* Tricky lock sequence: _add_next_thread unlocks on error. */
static void _add_next_thread(struct _parallel_startup_type *self)
{
	assert(!self->last_errno);

	if(self->parent->parallel_unfinished == _threadpool_count_parallel(self->parent))
	{
		PTHREAD_VERIFY(pthread_cond_broadcast(&self->parent->cond));
	}
	else
	{
		pthread_t *thread = self->parent->parallel_threads + self->parent->parallel_unfinished;
		self->last_errno = pthread_create(thread, NULL, _start_routine, self);
		if(self->last_errno)
			_parallel_abort(self->parent);
	}
}

static void *_thread_free_and_unlock(struct threadpool *self, void *thread)
{
	PTHREAD_VERIFY(pthread_mutex_unlock(&self->mutex));
#	if !HAVE_ALLOCA
	thread_free(thread);
#	endif
	return NULL;
}

static void *_thread_destroy_and_unlock(struct threadpool *self, void *thread)
{
	self->thread_destroy(thread);
	return _thread_free_and_unlock(self, thread);
}

/* At one point, one of the threads refused to destroy itself at the end. Why?! And why won't it happen again? */

static void *_start_routine(void *startup_raw)
{
	struct _parallel_startup_type *startup = (struct _parallel_startup_type *)startup_raw;

	struct threadpool *parent = startup->parent;

	void *thread;

	PTHREAD_VERIFY(pthread_mutex_lock(&parent->mutex));
	++parent->parallel_unfinished;

#	if HAVE_ALLOCA
/*	Ideally, the thread object goes on the thread's stack. This guarantees no false sharing with other threads, and in a NUMA
	configuration, ensures that the thread object is using memory from the right node. */
	thread = alloca(parent->thread_size);
#	else
	startup->last_errno = thread_malloc(&thread, NULL, parent->thread_size);
	if(startup->last_errno)
	{
		_parallel_abort(parent);
		PTHREAD_VERIFY(pthread_mutex_unlock(&parent->mutex));
		return NULL;
	}
#	endif

/*	Setting thread affinity for threads running in lock-step can cause delays
	and jumpiness.  Ideally, there would be some way to recommend (but not
	require) that a thread run on a certain core/set of cores. */

/*	Neither Linux nor libnuma seem to support the concept of a preferred/ideal
  	CPU for a thread/process. */

/*	Untested. */
/*	{
		cpu_set_t cpu_set;
		CPU_ZERO(&cpu_set);
		CPU_SET(&cpu_set, &parent._threads_unfinished);
		pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpu_set);
	} */

	startup->last_errno = startup->thread_create(thread, parent, parent->parallel_unfinished);
	if(startup->last_errno)
	{
		_parallel_abort(parent);
		return _thread_free_and_unlock(parent, thread); /* Tail calls make everything better. */
	}

	assert(!startup->last_errno);
	_add_next_thread(startup); /* Calls _parallel_abort() on failure. */
	if(startup->last_errno)
		return _thread_destroy_and_unlock(parent, thread);

	for(;;)
	{
		for(;;)
		{
			/*
			This must come before the '.threads' check, otherwise if
			threadpool_destroy is called immediately after a run starts, then
			it's possible that not all threads would be launched for the final
			run. This can cause deadlock in conjunction with things like
			barriers.
			*/
			if(parent->parallel_pending)
				break; /* Start a run. */

			if(!parent->parallel_threads)
				return _thread_destroy_and_unlock(parent, thread); /* Threads are shutting down. */

			PTHREAD_VERIFY(pthread_cond_wait(&parent->cond, &parent->mutex));
		}

		--parent->parallel_pending;
		if(!parent->parallel_pending)
			PTHREAD_VERIFY(pthread_cond_broadcast(&parent->cond));
			/* All threads have started processing, other threads can finish. */

		PTHREAD_VERIFY(pthread_mutex_unlock(&parent->mutex));

		parent->thread_run(thread);

		PTHREAD_VERIFY(pthread_mutex_lock(&parent->mutex));
#	if 0
		if(!parent->parallel_threads) /* I don't think this is necessary anymore. */
			break;
#	endif
		/* Don't loop around until all other threads have begun processing. */

		/* I suspect it doesn't matter whether this comes before or after the threads_unfinished check. */
		while(parent->parallel_pending)
			PTHREAD_VERIFY(pthread_cond_wait(&parent->cond, &parent->mutex));

		--parent->parallel_unfinished;
		if(!parent->parallel_unfinished)
			PTHREAD_VERIFY(pthread_cond_broadcast(&parent->cond)); /* All threads done for now. */
	}

	/* return _thread_destroy_and_unlock(parent, thread); */
}

static void _unlock_and_destroy(struct threadpool *self)
{
	pthread_t *threads;

	threads = self->parallel_threads;
	self->parallel_threads = NULL;

	if(threads)
		PTHREAD_VERIFY(pthread_cond_broadcast(&self->cond));

	PTHREAD_VERIFY(pthread_mutex_unlock(&self->mutex));

	if(threads)
	{
		unsigned i, count = _threadpool_count_parallel(self);
		for(i = 0; i != count; ++i)
			PTHREAD_VERIFY(pthread_join(threads[i], NULL));

		free(threads);
		PTHREAD_VERIFY(pthread_cond_destroy(&self->cond));
		PTHREAD_VERIFY(pthread_mutex_destroy(&self->mutex));
	}

	_serial_destroy(self);
}

#endif /* HAVE_PTHREAD */

int threadpool_create(struct threadpool *self, const struct threadpool_class *cls, Display *dpy, unsigned count)
{
	(void)threads_available(dpy);

	self->count = count;

/*	If threads are not present, run each "thread" in sequence on the calling
	thread. Otherwise, only run the first thread on the main thread. */

	assert(cls);

	self->thread_size = cls->size;
	self->thread_destroy = cls->destroy;

	{
		void *thread;
		unsigned i, count_serial = _threadpool_count_serial(self);

		if(count_serial)
		{
			thread = malloc(cls->size * count_serial);
			if(!thread)
				return ENOMEM;
		}
		else
		{
			/* Might as well skip the malloc. */
			thread = NULL;
		}

		self->serial_threads = thread;

		for(i = 0; i != count_serial; ++i)
		{
			int error = cls->create(thread, self, i);
			if(error)
			{
				self->count = i;
				_serial_destroy(self);
				return error;
			}

			thread = (char *)thread + self->thread_size;
		}
	}

#if HAVE_PTHREAD
	assert(_has_pthread); /* _has_pthread should be either -1 or >0. */
	if(_has_pthread >= 0)
	{
		unsigned count_parallel = _threadpool_count_parallel(self);
		self->mutex = mutex_initializer;
		self->cond = cond_initializer;
		self->parallel_pending = 0;
		self->parallel_unfinished = 0;
		if(!count_parallel)
		{
			self->parallel_threads = NULL;
			return 0;
		}

		self->parallel_threads = malloc(sizeof(pthread_t) * count_parallel);
		if(!self->parallel_threads)
			return ENOMEM;

		{
			struct _parallel_startup_type startup;
			startup.parent = self;
			startup.thread_create = cls->create;
			startup.last_errno = 0;

			PTHREAD_VERIFY(pthread_mutex_lock(&self->mutex));
			_add_next_thread(&startup);

			if(!startup.last_errno)
			{
				while(self->parallel_unfinished != count_parallel && self->parallel_threads)
					PTHREAD_VERIFY(pthread_cond_wait(&self->cond, &self->mutex));
			}

			/* This must come after the if(!startup.last_errno). */
			if(startup.last_errno)
			{
				_unlock_and_destroy(self);
			}
			else
			{
				self->parallel_unfinished = 0;
				PTHREAD_VERIFY(pthread_mutex_unlock(&self->mutex));
			}

			return startup.last_errno;
		}
	}
#endif

	return 0;
}

void threadpool_destroy(struct threadpool *self)
{
#if HAVE_PTHREAD
	if(_has_pthread >= 0)
	{
		PTHREAD_VERIFY(pthread_mutex_lock(&self->mutex));
		_unlock_and_destroy(self);
		return;
	}
#endif

	_serial_destroy(self);
}

void threadpool_run(struct threadpool *self, void (*func)(void *))
{
#if HAVE_PTHREAD
	if(_has_pthread >= 0)
	{
		unsigned count = _threadpool_count_parallel(self);
		PTHREAD_VERIFY(pthread_mutex_lock(&self->mutex));

		/* Do not call threadpool_run() twice without a threadpool_wait() in the middle. */
		assert(!self->parallel_pending);
		assert(!self->parallel_unfinished);

		self->parallel_pending = count;
		self->parallel_unfinished = count;
		self->thread_run = func;
		PTHREAD_VERIFY(pthread_cond_broadcast(&self->cond));
		PTHREAD_VERIFY(pthread_mutex_unlock(&self->mutex));
	}
#endif

	/* It's perfectly valid to move this to the beginning of threadpool_wait(). */
	{
		void *thread = self->serial_threads;
		unsigned i, count = _threadpool_count_serial(self);
		for(i = 0; i != count; ++i)
		{
			func(thread);
			thread = (char *)thread + self->thread_size;
		}
	}
}

void threadpool_wait(struct threadpool *self)
{
#if HAVE_PTHREAD
	if(_has_pthread >= 0)
	{
		PTHREAD_VERIFY(pthread_mutex_lock(&self->mutex));
		while(self->parallel_unfinished)
			PTHREAD_VERIFY(pthread_cond_wait(&self->cond, &self->mutex));
		PTHREAD_VERIFY(pthread_mutex_unlock(&self->mutex));
	}
#endif
}

/* io_thread - */

#if HAVE_PTHREAD
/* Without threads at compile time, there's only stubs in thread_util.h. */

#	define VERSION_CHECK(cc_major, cc_minor, req_major, req_minor) \
	((cc_major) > (req_major) || \
	(cc_major) == (req_major) && (cc_minor) >= (req_minor))

#	if defined(__GNUC__) && (__GNUC__ > 4 || __GNUC__ == 4 && __GNUC_MINOR__ >= 7) || \
	defined(__clang__) && \
		(!defined(__apple_build_version__) && VERSION_CHECK(__clang_major__, __clang_minor__, 3, 1) || \
		  defined(__apple_build_version__) && VERSION_CHECK(__clang_major__, __clang_minor__, 3, 1)) || \
	defined(__ICC) && __ICC >= 1400

/*
   Clang 3.0 has a partial implementation of GNU atomics; 3.1 rounds it out.
   http://llvm.org/viewvc/llvm-project/cfe/tags/RELEASE_30/final/include/clang/Basic/Builtins.def?view=markup
   http://llvm.org/viewvc/llvm-project/cfe/tags/RELEASE_31/final/include/clang/Basic/Builtins.def?view=markup

   Apple changes the Clang version to track Xcode versions; use
   __apple_build_version__ to distinguish between the two.

   Xcode 4.3 uses Apple LLVM 3.1, which corresponds to Clang 3.1.
   https://en.wikipedia.org/wiki/Xcode

   Earlier versions of Intel C++ may also support these intrinsics.
 */

#define _status_load(status) (__atomic_load_n((status), __ATOMIC_SEQ_CST))
#define _status_exchange(obj, desired) (__atomic_exchange_n((obj), (desired), __ATOMIC_SEQ_CST))

/* C11 atomics are around the corner, but they're not here yet for many
   systems. (Including mine.) */
/*
#elif __STDC_VERSION__ >= 201112l && !defined __STDC_NO_ATOMICS__

#include <stdatomic.h>

#define _status_load(status) (atomic_load((status)))
#define _status_exchange(obj, desired) (atomic_exchange((obj), (desired)))
*/

/* Solaris profiles atomic ops on at least Solaris 10. See atomic_swap(3C) and
   membar_ops(3C). This would probably also need a snippet in configure.in.
   http://graegert.com/programming/using-atomic-operations-in-c-on-solaris-10
*/

#	else

/* No atomic variables, so here's some ugly mutex-based code instead. */

/* Nothing ever destroys this mutex. */
pthread_mutex_t _global_mutex = PTHREAD_MUTEX_INITIALIZER;

#define _lock()	PTHREAD_VERIFY(pthread_mutex_lock(&_global_mutex))
#define _unlock() PTHREAD_VERIFY(pthread_mutex_unlock(&_global_mutex))

static enum _io_thread_status _status_load(enum _io_thread_status *status)
{
	enum _io_thread_status result;
	_lock();
	result = *status;
	_unlock();
	return result;
}

static enum _io_thread_status _status_exchange(enum _io_thread_status *obj, enum _io_thread_status desired)
{
	enum _io_thread_status result;
	_lock();
	result = *obj;
	*obj = desired;
	_unlock();
	return result;
}

#	endif

void *io_thread_create(struct io_thread *self, void *parent, void *(*start_routine)(void *), Display *dpy, unsigned stacksize)
{
	if(threads_available(dpy) >= 0)
	{
		int error;
		pthread_attr_t attr;
		pthread_attr_t *attr_ptr = NULL;

		if(stacksize)
		{
			attr_ptr = &attr;
			if(pthread_attr_init(&attr))
				return NULL;
#   if defined _POSIX_SOURCE || defined _POSIX_C_SOURCE || defined _XOPEN_SOURCE
			/* PTHREAD_STACK_MIN needs the above test. */
			assert(stacksize >= PTHREAD_STACK_MIN);
#   endif
			PTHREAD_VERIFY(pthread_attr_setstacksize(&attr, stacksize));
		}

		/* This doesn't need to be an atomic store, since pthread_create(3)
		   "synchronizes memory with respect to other threads".
		   http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap04.html#tag_04_11 */
		self->status = _io_thread_working;

		error = pthread_create(&self->thread, attr_ptr, start_routine, parent);
		assert(!error || error == EAGAIN);
		if(error)
			parent = NULL;

		if(attr_ptr)
			PTHREAD_VERIFY(pthread_attr_destroy(attr_ptr));

		return parent;
	}

	return NULL;
}

int io_thread_return(struct io_thread *self)
{
	if(_has_pthread >= 0)
	{
		enum _io_thread_status old_status = _status_exchange(&self->status, _io_thread_done);
		assert(old_status == _io_thread_working ||
		       old_status == _io_thread_cancelled);
		return old_status != _io_thread_working;
	}

	return 0;
}

int io_thread_is_done(struct io_thread *self)
{
	if(_has_pthread >= 0)
	{
		int result = _status_load(&self->status);
		assert(result != _io_thread_cancelled);
		return result;
	}
	return 1;
}

int io_thread_cancel(struct io_thread *self)
{
	if(_has_pthread >= 0)
	{
		enum _io_thread_status old_status =
			_status_exchange(&self->status, _io_thread_cancelled);
		assert(old_status == _io_thread_working ||
		       old_status == _io_thread_done);

		PTHREAD_VERIFY(pthread_detach(self->thread));
		return old_status != _io_thread_working;
	}

	return 0;
}

void io_thread_finish(struct io_thread *self)
{
	if(_has_pthread >= 0)
	{
#	ifndef NDEBUG
		enum _io_thread_status status = _status_load(&self->status);
		assert(status == _io_thread_working ||
		       status == _io_thread_done);
#	endif
		PTHREAD_VERIFY(pthread_join(self->thread, NULL));
		assert(_status_load(&self->status) == _io_thread_done);
	}
}

#endif /* HAVE_PTHREAD */
