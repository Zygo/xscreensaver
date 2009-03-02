
/* -*- Mode: C; tab-width: 4 -*- */
/* vines --- vine fractals */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)vines.c	4.04 97/07/28 xlockmore";

#endif

/*-
 * Copyright (c) 1997 by Tracy Camp campt@hurrah.com
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
 * If you make a modification I would of course appreciate a copy.
 *
 * Revision History:
 * 11-Jul-97: David Hansen <dhansen@metapath.com>
 *            Changed names to vines and modified draw loop
 *            to honor batchcount so vines can be grown or
 *            plotted.
 * 10-May-97: Compatible with xscreensaver
 * 21-Mar-97: David Hansen <dhansen@metapath.com>
 *            Updated mode to draw complete patterns on every
 *            iteration instead of growing the vine.  Also made
 *            adjustments to randomization and changed variable
 *            names to make logic easier to follow.
 */

/*- 
 * This was modifed from a 'screen saver' that a friend and I
 * wrote on our TI-8x calculators in high school physics one day
 * Basically another geometric pattern generator, this ones claim
 * to fame is a pseudo-fractal looking vine like pattern that creates
 * nifty whorls and loops.
 */

#ifdef STANDALONE
#define PROGCLASS "Vines"
#define HACK_INIT init_vines
#define HACK_DRAW draw_vines
#define vines_opts xlockmore_opts
#define DEFAULTS "*delay: 200000 \n" \
 "*count: 0 \n" \
 "*ncolors: 200 \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

ModeSpecOpt vines_opts =
{0, NULL, 0, NULL, NULL};

typedef struct {
	int         a;
	int         x1;
	int         y1;
	int         x2;
	int         y2;
	int         i;
	int         length;
	int         iterations;
	int         constant;
	int         ang;
	int         centerx;
	int         centery;
} vinestruct;

static vinestruct *vines = NULL;

void
refresh_vines(ModeInfo * mi)
{
}				/* refresh_vines */

void
init_vines(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	vinestruct *fp;

	if (vines == NULL) {
		if ((vines = (vinestruct *) calloc(MI_NUM_SCREENS(mi), sizeof (vinestruct))) == NULL) {
			return;
		}
	}
	fp = &vines[MI_SCREEN(mi)];

	fp->i = 0;
	fp->length = 0;
	fp->iterations = 30 + NRAND(100);

	XClearWindow(display, MI_WINDOW(mi));
}				/* init_vines */

void
draw_vines(ModeInfo * mi)
{
	vinestruct *fp = &vines[MI_SCREEN(mi)];
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	int         count;

	if (fp->i >= fp->length) {
		if (--(fp->iterations) == 0)
			init_vines(mi);

		fp->centerx = NRAND(MI_WIN_WIDTH(mi));
		fp->centery = NRAND(MI_WIN_HEIGHT(mi));

		fp->ang = 60 + NRAND(720);
		fp->length = 100 + NRAND(3000);
		fp->constant = fp->length * (10 + NRAND(10));

		fp->i = 0;
		fp->a = 0;
		fp->x1 = 0;
		fp->y1 = 0;
		fp->x2 = 1;
		fp->y2 = 0;

		if (MI_NPIXELS(mi) > 2)
			XSetForeground(display, gc, MI_PIXEL(mi, NRAND(MI_NPIXELS(mi))));
		else
			XSetForeground(display, gc, MI_WIN_WHITE_PIXEL(mi));
	}
	count = fp->i + MI_BATCHCOUNT(mi);
	if ((count <= fp->i) || (count > fp->length))
		count = fp->length;

	while (fp->i < count) {
		XDrawLine(display, MI_WINDOW(mi), gc,
			  fp->centerx + (fp->x1 / fp->constant),
			  fp->centery - (fp->y1 / fp->constant),
			  fp->centerx + (fp->x2 / fp->constant),
			  fp->centery - (fp->y2 / fp->constant));

		fp->a += (fp->ang * fp->i);

		fp->x1 = fp->x2;
		fp->y1 = fp->y2;

		fp->x2 += (int) (fp->i * ((cos(fp->a) * 360) / (2 * M_PI)));
		fp->y2 += (int) (fp->i * ((sin(fp->a) * 360) / (2 * M_PI)));
		fp->i++;
	}
}				/* draw_vines */

void
release_vines(ModeInfo * mi)
{
	if (vines != NULL) {
		(void) free((void *) vines);
		vines = NULL;
	}
}				/* release_vines */
