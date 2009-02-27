#ifndef lint
static char sccsid[] = "@(#)hsbramp.c	23.5 91/05/24 XLOCK";
#endif
/*-
 * hsbramp.c - Create an HSB ramp.
 *
 * Copyright (c) 1991 by Patrick J. Naughton.
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * Changes of David Bagley <bagleyd@source.asset.com>
 * 22-Jun-94: Modified for VMS
 *            <Anthony.D.Clarke@Support.Hatfield.Raytheon.bae.eurokom.ie>
 * Changes of Patrick J. Naughton
 * 29-Jul-90: renamed hsbramp.c from HSBmap.c
 *	      minor optimizations.
 * 01-Sep-88: Written.
 */

#include <sys/types.h>
#ifdef VMS
typedef unsigned char u_char;
#endif
#include <math.h>

void
hsb2rgb(H, S, B, r, g, b)
    double      H,
                S,
                B;
    u_char     *r,
               *g,
               *b;
{
    int         i;
    double      f;
    double      bb;
    u_char      p;
    u_char      q;
    u_char      t;

    H -= floor(H);		/* remove anything over 1 */
    H *= 6.0;
    i = floor(H);		/* 0..5 */
    f = H - (float) i;		/* f = fractional part of H */
    bb = 255.0 * B;
    p = (u_char) (bb * (1.0 - S));
    q = (u_char) (bb * (1.0 - (S * f)));
    t = (u_char) (bb * (1.0 - (S * (1.0 - f))));
    switch (i) {
    case 0:
	*r = (u_char) bb;
	*g = t;
	*b = p;
	break;
    case 1:
	*r = q;
	*g = (u_char) bb;
	*b = p;
	break;
    case 2:
	*r = p;
	*g = (u_char) bb;
	*b = t;
	break;
    case 3:
	*r = p;
	*g = q;
	*b = (u_char) bb;
	break;
    case 4:
	*r = t;
	*g = p;
	*b = (u_char) bb;
	break;
    case 5:
	*r = (u_char) bb;
	*g = p;
	*b = q;
	break;
    }
}


/*
 * Input is two points in HSB color space and a count
 * of how many discreet rgb space values the caller wants.
 *
 * Output is that many rgb triples which describe a linear
 * interpolate ramp between the two input colors.
 */

void
hsbramp(h1, s1, b1, h2, s2, b2, count, red, green, blue)
    double      h1,
                s1,
                b1,
                h2,
                s2,
                b2;
    int         count;

    u_char     *red,
               *green,
               *blue;
{
    double      dh;
    double      ds;
    double      db;

    dh = (h2 - h1) / count;
    ds = (s2 - s1) / count;
    db = (b2 - b1) / count;
    while (count--) {
	hsb2rgb(h1, s1, b1, red++, green++, blue++);
	h1 += dh;
	s1 += ds;
	b1 += db;
    }
}
