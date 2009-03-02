#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)random.c	4.10 98/04/23 xlockmore";

#endif

/*-
 * random.c - various utilities for random numbers
 *
 * Copyright (c) 1998 by David Bagley
 *
 * Revision History:
 *
 * Changes maintained by David Bagley <bagleyd@bigfoot.com>
 * 23-Apr-96: Separated out of util.c
 *
 */


/*-
 * Dr. Park's algorithm published in the Oct. '88 ACM  "Random Number
 * Generators: Good Ones Are Hard To Find" His version available at
 * ftp://cs.wm.edu/pub/rngs.tar Present form by many authors.
 */

static int  Seed = 1;		/* This is required to be 32 bits long */

/*-
 *      Given an integer, this routine initializes the RNG seed.
 */
void
SetRNG(long int s)
{
	Seed = (int) s;
}

/*-
 *      Returns an integer between 0 and 2147483647, inclusive.
 */
long
LongRNG(void)
{
	if ((Seed = Seed % 44488 * 48271 - Seed / 44488 * 3399) < 0)
		Seed += 2147483647;
	return (long) (Seed - 1);
}
