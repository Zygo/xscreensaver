/* xscreensaver, Copyright (c) 1997, 1998, 2003 by Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __YARANDOM_H__
#define __YARANDOM_H__

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif

#undef random
#undef rand
#undef drand48
#undef srandom
#undef srand
#undef frand
#undef sranddev
#undef srandomdev
#undef arc4random
#undef arc4random_addrandom
#undef arc4random_buf
#undef arc4random_stir
#undef arc4random_uniform
#undef erand48
#undef jrand48
#undef lcong48
#undef lrand48
#undef mrand48
#undef nrand48
#undef seed48
#undef srand48
#undef rand_r
#undef RAND_MAX

#ifdef VMS
# include "vms-gtod.h"
#endif

extern unsigned int ya_random (void);
extern void ya_rand_init (unsigned int);

#define random()   ya_random()
#define RAND_MAX   0xFFFFFFFF

/*#define srandom(i) ya_rand_init(0)*/

/* Define these away to keep people from using the wrong APIs in xscreensaver.
 */
#define rand          __ERROR_use_random_not_rand_in_xscreensaver__
#define drand48       __ERROR_use_frand_not_drand48_in_xscreensaver__
#define srandom       __ERROR_do_not_call_srandom_in_xscreensaver__
#define srand         __ERROR_do_not_call_srand_in_xscreensaver__
#define sranddev      __ERROR_do_not_call_sranddev_in_xscreensaver__
#define ya_rand_init  __ERROR_do_not_call_ya_rand_init_in_xscreensaver__
#define srandomdev    __ERROR_do_not_call_srandomdev_in_xscreensaver__
#define arc4random    __ERROR_do_not_call_arc4random_in_xscreensaver__
#define arc4random_addrandom __ERROR_do_not_call_arc4random_in_xscreensaver__
#define arc4random_buf       __ERROR_do_not_call_arc4random_in_xscreensaver__
#define arc4random_stir      __ERROR_do_not_call_arc4random_in_xscreensaver__
#define arc4random_uniform   __ERROR_do_not_call_arc4random_in_xscreensaver__
#define erand48    __ERROR_do_not_call_erand48_in_xscreensaver__
#define jrand48    __ERROR_do_not_call_jrand48_in_xscreensaver__
#define lcong48    __ERROR_do_not_call_lcong48_in_xscreensaver__
#define lrand48    __ERROR_do_not_call_lrand48_in_xscreensaver__
#define mrand48    __ERROR_do_not_call_mrand48_in_xscreensaver__
#define nrand48    __ERROR_do_not_call_nrand48_in_xscreensaver__
#define seed48     __ERROR_do_not_call_seed48_in_xscreensaver__
#define srand48    __ERROR_do_not_call_srand48_in_xscreensaver__
#define rand_r     __ERROR_do_not_call_rand_r_in_xscreensaver__


#if defined (__GNUC__) && (__GNUC__ >= 2)
 /* Implement frand using GCC's statement-expression extension. */

# define frand(f)							\
  __extension__								\
  ({ double tmp = ((((double) random()) * ((double) (f))) /		\
		   ((double) ((unsigned int)~0)));			\
     tmp < 0 ? (-tmp) : tmp; })

#else /* not GCC2 - implement frand using a global variable.*/

static double _frand_tmp_;
# define frand(f)							\
  (_frand_tmp_ = ((((double) random()) * ((double) (f))) /		\
		  ((double) ((unsigned int)~0))),			\
   _frand_tmp_ < 0 ? (-_frand_tmp_) : _frand_tmp_)

#endif /* not GCC2 */

/* Compatibility with the xlockmore RNG API
   (note that the xlockmore hacks never expect negative numbers.)
 */
#define LRAND()         ((long) (random() & 0x7fffffff))

/* The first NRAND(n) is much faster, when uint64_t is available to allow it.
 * Especially on ARM and other processors without built-in division.
 *
 * n must be greater than zero.
 *
 * The division by RAND_MAX+1 should optimize down to a bit shift.
 *
 * Although the result here is never negative, this needs to return signed
 * integers: A binary operator in C requires operands have the same type, and
 * if one side is signed and the other is unsigned, but they're both the same
 * size, then the signed operand is cast to unsigned, and the result is
 * similarly unsigned. This can potentially cause problems when idioms like
 * "NRAND(n) * RANDSIGN()" (where RANDSIGN() returns either -1 or 1) is used
 * to get random numbers from the range (-n,n).
 */
#ifdef HAVE_INTTYPES_H
# define NRAND(n)       ((int32_t) ((uint64_t) random() * (uint32_t) (n) / \
                                    ((uint64_t) RAND_MAX + 1)))
#else
# define NRAND(n)       ((int) (LRAND() % (n)))
#endif

#define MAXRAND         (2147483648.0) /* unsigned 1<<31 as a float */
#define SRAND(n)        /* already seeded by screenhack.c */

#endif /* __YARANDOM_H__ */
