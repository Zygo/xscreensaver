#ifndef _RANDOM_H_
#define _RANDOM_H_

#if !defined( lint ) && !defined( SABER )
/* #ident	"@(#)random.h	4.14 99/06/17 xlockmore" */

#endif

/*-
 * Random stuff
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 17-06-99: Started log. :)
 */

/*** random number generator ***/
/* defaults */
#ifdef STANDALONE
/*-
 * Compatibility with the xlockmore RNG API
 * (note that the xlockmore hacks never expect negative numbers.)
 */
#define LRAND()     ((long) (random() & 0x7fffffff))
#define NRAND(n)    ((int) (LRAND() % (n)))
#define MAXRAND     (2147483648.0)	/* unsigned 1<<31 as a float */
#define SRAND(n)		/* already seeded by screenhack.c */
#else /* STANDALONE */
#ifdef HAVE_RAND48
#define SRAND srand48
#define LRAND lrand48
#define MAXRAND (2147483648.0)
#ifndef DECLARED_SRAND48
#ifdef __cplusplus
extern "C" {
#endif
#ifndef __GLIBC__
extern void srand48(long int);
extern long int lrand48(void);
#endif
#ifdef __cplusplus
}
#endif
#endif
#else /* HAVE_RAND48 */
#ifdef HAVE_RANDOM
#define SRAND srandom
#define LRAND random
#define MAXRAND (2147483648.0)
#ifdef __cplusplus
extern "C" {
#endif
extern void srandom(unsigned int);
extern long int random(void);
#ifdef __cplusplus
}
#endif
#else /* HAVE_RANDOM */
#ifdef HAVE_RAND
#define SRAND srand
#define LRAND rand
#ifdef __cplusplus
extern "C" {
#endif
extern void srand(unsigned int);
extern int rand(void);
#ifdef __cplusplus
}
#endif
#ifdef AIXV3
#define MAXRAND (2147483648.0)
#else
#define MAXRAND (32768.0)
#endif
#endif /* HAVE_RAND */
#endif /* HAVE_RANDOM */
#endif /* HAVE_RAND48 */

#ifndef SRAND
#ifdef __cplusplus
  extern "C" {
#endif
extern void SetRNG(long int s);
#ifdef __cplusplus
  }
#endif

#define SRAND(X) SetRNG((long) X)
#endif
#ifndef LRAND
#ifdef __cplusplus
  extern "C" {
#endif
extern long LongRNG(void);
#ifdef __cplusplus
  }
#endif

extern long LongRNG(void);
#define LRAND() LongRNG()
#endif
#ifndef MAXRAND
#define MAXRAND (2147483648.0)
#endif
#define NRAND(X) ((int)(LRAND()%(X)))
#endif /* STANDALONE */
#endif /* _RANDOM_H_ */
