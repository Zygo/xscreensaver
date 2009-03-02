#ifndef _RANDOM_H_
#define _RANDOM_H_
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
#if HAVE_RAND48
#define SRAND srand48
#define LRAND lrand48
#define MAXRAND (2147483648.0)
#else /* HAVE_RAND48 */
#if HAVE_RANDOM
#define SRAND srand48
#define LRAND lrand48
#define MAXRAND (2147483648.0)
#else /* HAVE_RANDOM */
#if HAVE_RAND
#define SRAND srand48
#define LRAND lrand48
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
extern long LongRNG(void);

#define LRAND() LongRNG()
#endif
#ifndef MAXRAND
#define MAXRAND (2147483648.0)
#endif

#define NRAND(X) ((int)(LRAND()%(X)))
#endif /* STANDALONE */
#endif /* _RANDOM_H_ */
