/* ya_random -- Yet Another Random Number Generator.

   The unportable mess that is rand(), random(), drand48() and friends led me
   to ask Phil Karlton <karlton@netscape.com> what the Right Thing to Do was.
   He responded with this.  It is non-cryptographically secure, reasonably
   random (more so than anything that is in any C library), and very fast.

   I don't understand how it works at all, but he says "look at Knuth,
   Vol. 2 (original edition), page 26, Algorithm A.  In this case n=55,
   k=20 and m=2^32."

   So there you have it.
 */

#ifndef VMS
#include <unistd.h>   /* for getpid() */
#include <sys/time.h> /* for gettimeofday() */
#else
#include unixlib       /* for getpid() */
#include "unix_time.h" /* for gettimeofday() */
#endif


/* The following 'random' numbers are taken from CRC, 18th Edition, page 622.
   Each array element was taken from the corresponding line in the table,
   except that a[0] was from line 100. 8s and 9s in the table were simply
   skipped. The high order digit was taken mod 4.
 */
#define VectorSize 55
static unsigned int a[VectorSize] = {
 035340171546, 010401501101, 022364657325, 024130436022, 002167303062, /*  5 */
 037570375137, 037210607110, 016272055420, 023011770546, 017143426366, /* 10 */
 014753657433, 021657231332, 023553406142, 004236526362, 010365611275, /* 14 */
 007117336710, 011051276551, 002362132524, 001011540233, 012162531646, /* 20 */
 007056762337, 006631245521, 014164542224, 032633236305, 023342700176, /* 25 */
 002433062234, 015257225043, 026762051606, 000742573230, 005366042132, /* 30 */
 012126416411, 000520471171, 000725646277, 020116577576, 025765742604, /* 35 */
 007633473735, 015674255275, 017555634041, 006503154145, 021576344247, /* 40 */
 014577627653, 002707523333, 034146376720, 030060227734, 013765414060, /* 45 */
 036072251540, 007255221037, 024364674123, 006200353166, 010126373326, /* 50 */
 015664104320, 016401041535, 016215305520, 033115351014, 017411670323  /* 55 */
};

static int i1, i2;

/* i1 & i2 must be between 0 and Vectorsize -1 (Patrick Moreau - Dec 1995) */

unsigned int ya_random()
{
  register int ret = a[i1] + a[i2];
  a[i1] = ret;
  i1 = i1 >= (VectorSize -1) ? 0 : i1 + 1;
  i2 = i2 >= (VectorSize -1) ? 0 : i2 + 1;
  return ret;
}

void ya_rand_init(seed)
   register unsigned int seed;
{
  int i;
  if (seed == 0)
    {
      struct timeval tp;
      struct timezone tzp;
      gettimeofday(&tp, &tzp);
      /* ignore overflow */
      seed = (999*tp.tv_sec) + (1001*tp.tv_usec) + (1003 * getpid());
    }

  a[0] += seed;
  for (i = 1; i < (VectorSize-1); i++)
    {
      seed = a[i-1]*1001 + seed*999;
      a[i] += seed;
    }

  i1 = a[0] % (VectorSize -1);
  i2 = (i1 + 024) % (VectorSize -1);
}
