#ifndef lint
static char sccsid[] = "@(#)hopalong.c	23.9 91/05/24 XLOCK";
#endif
/*-
 * hopalong.c - Real Plane Fractals for xlock, the X Window System lockscreen.
 *
 * Copyright (c) 1991 by Patrick J. Naughton.
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 29-Oct-90: fix bad (int) cast.
 * 29-Jul-90: support for multiple screens.
 * 08-Jul-90: new timing and colors and new algorithm for fractals.
 * 15-Dec-89: Fix for proper skipping of {White,Black}Pixel() in colors.
 * 08-Oct-89: Fixed long standing typo bug in RandomInitHop();
 *	      Fixed bug in memory allocation in inithop();
 *	      Moved seconds() to an extern.
 *	      Got rid of the % mod since .mod is slow on a sparc.
 * 20-Sep-89: Lint.
 * 31-Aug-88: Forked from xlock.c for modularity.
 * 23-Mar-88: Coded HOPALONG routines from Scientific American Sept. 86 p. 14.
 */

#include "xlock.h"
#include <math.h>

typedef struct {
    int         centerx;
    int         centery;	/* center of the screen */
    double      a;
    double      b;
    double      c;
    double      i;
    double      j;		/* hopalong parameters */
    int         inc;
    int         pix;
    long        startTime;
}           hopstruct;

static hopstruct hops[MAXSCREENS];
static XPoint *pointBuffer = 0;	/* pointer for XDrawPoints */

#define TIMEOUT 30

void
inithop(win)
    Window      win;
{
    double      range;
    XWindowAttributes xgwa;
    hopstruct  *hp = &hops[screen];


    XGetWindowAttributes(dsp, win, &xgwa);
    hp->centerx = xgwa.width / 2;
    hp->centery = xgwa.height / 2;
    range = sqrt((double) hp->centerx * hp->centerx +
		 (double) hp->centery * hp->centery) /
	(10.0 + random() % 10);

    hp->pix = 0;
    hp->inc = (int) ((random() / MAXRAND) * 200) - 100;
    hp->a = (random() / MAXRAND) * range - range / 2.0;
    hp->b = (random() / MAXRAND) * range - range / 2.0;
    hp->c = (random() / MAXRAND) * range - range / 2.0;
    if (!(random() % 2))
	hp->c = 0.0;

    hp->i = hp->j = 0.0;

    if (!pointBuffer)
	pointBuffer = (XPoint *) malloc(batchcount * sizeof(XPoint));

    XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
    XFillRectangle(dsp, win, Scr[screen].gc, 0, 0,
		   hp->centerx * 2, hp->centery * 2);
    XSetForeground(dsp, Scr[screen].gc, WhitePixel(dsp, screen));
    hp->startTime = seconds();
}


void
drawhop(win)
    Window      win;
{
    double      oldj;
    int         k = batchcount;
    XPoint     *xp = pointBuffer;
    hopstruct  *hp = &hops[screen];

    hp->inc++;
    if (!mono && Scr[screen].npixels > 2) {
	XSetForeground(dsp, Scr[screen].gc, Scr[screen].pixels[hp->pix]);
	if (++hp->pix >= Scr[screen].npixels)
	    hp->pix = 0;
    }
    while (k--) {
	oldj = hp->j;
	hp->j = hp->a - hp->i;
	hp->i = oldj + (hp->i < 0
			? sqrt(fabs(hp->b * (hp->i + hp->inc) - hp->c))
			: -sqrt(fabs(hp->b * (hp->i + hp->inc) - hp->c)));
	xp->x = hp->centerx + (int) (hp->i + hp->j);
	xp->y = hp->centery - (int) (hp->i - hp->j);
	xp++;
    }
    XDrawPoints(dsp, win, Scr[screen].gc,
		pointBuffer, batchcount, CoordModeOrigin);
    if (seconds() - hp->startTime > TIMEOUT)
	inithop(win);
}
