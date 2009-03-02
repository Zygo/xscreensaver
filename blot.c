
/* -*- Mode: C; tab-width: 4 -*- */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)blot.c	4.03 97/05/10 xlockmore";

#endif

/*-
 * Rorschach's ink blot test
 *
 * Copyright (c) 1992 by Jamie Zawinski
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * 10-May-97: Compatible with xscreensaver
 * 05-Jan-95: patch for Dual-Headed machines from Greg Onufer
 *            <Greg.Onufer@Eng.Sun.COM>
 * 07-Dec-94: now randomly has xsym, ysym, or both.
 * 02-Sep-93: xlock version David Bagley <bagleyd@bigfoot.com>
 * 1992:      xscreensaver version Jamie Zawinski <jwz@netscape.com>
 */

/*-
 * original copyright
 * Copyright (c) 1992 by Jamie Zawinski
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this 
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#ifdef STANDALONE
#define PROGCLASS            "Blot"
#define HACK_INIT            init_blot
#define HACK_DRAW            draw_blot
#define DEF_DELAY            200000
#define DEF_BATCHCOUNT       6
#define DEF_CYCLES           30
#define DEF_NCOLORS  200
#define BRIGHT_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
ModeSpecOpt blot_opts =
{0, NULL, 0, NULL, NULL};

#endif /* STANDALONE */

typedef struct {
	int         width;
	int         height;
	int         xmid, ymid;
	int         offset;
	int         xsym, ysym;
	unsigned long size;
	int         pix;
	int         count;
	XPoint     *pointBuffer;
	unsigned long pointBufferSize;
} blotstruct;

static blotstruct *blots = NULL;

void
init_blot(ModeInfo * mi)
{
	blotstruct *bp;
	Display    *display = MI_DISPLAY(mi);

	if (blots == NULL) {
		if ((blots = (blotstruct *) calloc(MI_NUM_SCREENS(mi),
					       sizeof (blotstruct))) == NULL)
			return;
	}
	bp = &blots[MI_SCREEN(mi)];

	bp->width = MI_WIN_WIDTH(mi);
	bp->height = MI_WIN_HEIGHT(mi);
	bp->xmid = bp->width / 2;
	bp->ymid = bp->height / 2;

	bp->offset = 4;
	bp->ysym = LRAND() & 1;
	bp->xsym = (bp->ysym) ? LRAND() & 1 : 1;
	if (MI_NPIXELS(mi) > 2)
		bp->pix = NRAND(MI_NPIXELS(mi));
	if (bp->offset <= 0)
		bp->offset = 3;
	if (MI_BATCHCOUNT(mi) < 0)
		bp->size = NRAND(-MI_BATCHCOUNT(mi) + 1);
	else
		bp->size = MI_BATCHCOUNT(mi);

	/* Fudge the size so it takes up the whole screen */
	bp->size *= (unsigned long) (bp->width / 32 + 1) * (bp->height / 32 + 1);
	if (!bp->pointBuffer || bp->pointBufferSize < bp->size * sizeof (XPoint)) {
		if (bp->pointBuffer != NULL)
			(void) free((void *) bp->pointBuffer);
		bp->pointBufferSize = bp->size * sizeof (XPoint);
		bp->pointBuffer = (XPoint *) malloc(bp->pointBufferSize);
	}
	XClearWindow(display, MI_WINDOW(mi));
	XSetForeground(display, MI_GC(mi), MI_WIN_WHITE_PIXEL(mi));
	bp->count = 0;
}

void
draw_blot(ModeInfo * mi)
{
	blotstruct *bp = &blots[MI_SCREEN(mi)];
	XPoint     *xp = bp->pointBuffer;
	int         x, y;
	unsigned long k;

	if (MI_NPIXELS(mi) > 2) {
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_PIXEL(mi, bp->pix));
		if (++bp->pix >= MI_NPIXELS(mi))
			bp->pix = 0;
	}
	x = bp->xmid;
	y = bp->ymid;
	k = bp->size;
	while (k >= 4) {
		x += (NRAND(1 + (bp->offset << 1)) - bp->offset);
		y += (NRAND(1 + (bp->offset << 1)) - bp->offset);
		k--;
		xp->x = x;
		xp->y = y;
		xp++;
		if (bp->xsym) {
			k--;
			xp->x = bp->width - x;
			xp->y = y;
			xp++;
		}
		if (bp->ysym) {
			k--;
			xp->x = x;
			xp->y = bp->height - y;
			xp++;
		}
		if (bp->xsym && bp->ysym) {
			k--;
			xp->x = bp->width - x;
			xp->y = bp->height - y;
			xp++;
		}
	}
	XDrawPoints(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi),
		    bp->pointBuffer, (bp->size - k), CoordModeOrigin);
	if (++bp->count > MI_CYCLES(mi))
		init_blot(mi);
}


void
release_blot(ModeInfo * mi)
{
	if (blots != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			if (blots[screen].pointBuffer != NULL) {
				(void) free((void *) blots[screen].pointBuffer);
			}
		}
		(void) free((void *) blots);
		blots = NULL;
	}
}

void
refresh_blot(ModeInfo * mi)
{
	XClearWindow(MI_DISPLAY(mi), MI_WINDOW(mi));
}
