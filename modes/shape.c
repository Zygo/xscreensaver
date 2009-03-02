
/* -*- Mode: C; tab-width: 4 -*- */
/* shape --- basic in your face shapes */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)shape.c	4.04 97/07/28 xlockmore";

#endif

/*-
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
 * Revision History:
 * 10-May-97: Compatible with xscreensaver
 * 03-Nov-95: formerly rect.c
 * 11-Aug-95: slight change to initialization of pixmaps
 * 27-Jun-95: added ellipses
 * 27-Feb-95: patch for VMS
 * 29-Sep-94: multidisplay bug fix <epstein_caleb@jpmorgan.com>
 * 15-Jul-94: xlock version David Bagley <bagleyd@bigfoot.com>
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
#define PROGCLASS "Shape"
#define HACK_INIT init_shape
#define HACK_DRAW draw_shape
#define shape_opts xlockmore_opts
#define DEFAULTS "*delay: 10000 \n" \
 "*count: 100 \n" \
 "*cycles: 256 \n" \
 "*ncolors: 200 \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

ModeSpecOpt shape_opts =
{0, NULL, 0, NULL, NULL};

#define NBITS 12

#include "bitmaps/stipple.xbm"
#include "bitmaps/cross_weave.xbm"
#include "bitmaps/dimple1.xbm"
#include "bitmaps/dimple3.xbm"
#include "bitmaps/flipped_gray.xbm"
#include "bitmaps/gray1.xbm"
#include "bitmaps/gray3.xbm"
#include "bitmaps/hlines2.xbm"
#include "bitmaps/light_gray.xbm"
#include "bitmaps/root_weave.xbm"
#include "bitmaps/vlines2.xbm"
#include "bitmaps/vlines3.xbm"
#define SHAPEBITS(n,w,h)\
  sp->pixmaps[sp->init_bits++]=\
		XCreateBitmapFromData(display,window,(char *)n,w,h)

typedef struct {
	int         width;
	int         height;
	int         borderx;
	int         bordery;
	int         time;
	int         x, y, w, h;
	GC          stippledgc;
	int         init_bits;
	Pixmap      pixmaps[NBITS];
} shapestruct;


static shapestruct *shapes = NULL;

void
init_shape(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	shapestruct *sp;

	if (shapes == NULL) {
		if ((shapes = (shapestruct *) calloc(MI_NUM_SCREENS(mi),
					      sizeof (shapestruct))) == NULL)
			return;
	}
	sp = &shapes[MI_SCREEN(mi)];

	sp->width = MI_WIN_WIDTH(mi);
	sp->height = MI_WIN_HEIGHT(mi);
	sp->borderx = sp->width / 20;
	sp->bordery = sp->height / 20;
	sp->time = 0;

	if (!sp->stippledgc) {
		XGCValues   gcv;

		gcv.foreground = MI_WIN_WHITE_PIXEL(mi);
		gcv.background = MI_WIN_BLACK_PIXEL(mi);
		if ((sp->stippledgc = XCreateGC(display, window,
				 GCForeground | GCBackground, &gcv)) == None)
			return;
		XSetFillStyle(display, sp->stippledgc, FillOpaqueStippled);
	}
	if (!sp->init_bits) {
		SHAPEBITS(cross_weave_bits, cross_weave_width, cross_weave_height);
		SHAPEBITS(dimple1_bits, dimple1_width, dimple1_height);
		SHAPEBITS(dimple3_bits, dimple3_width, dimple3_height);
		SHAPEBITS(flipped_gray_bits, flipped_gray_width, flipped_gray_height);
		SHAPEBITS(gray1_bits, gray1_width, gray1_height);
		SHAPEBITS(gray3_bits, gray3_width, gray3_height);
		SHAPEBITS(vlines3_bits, vlines3_width, vlines3_height);
		SHAPEBITS(stipple_bits, stipple_width, stipple_height);
		SHAPEBITS(hlines2_bits, hlines2_width, hlines2_height);
		SHAPEBITS(light_gray_bits, light_gray_width, light_gray_height);
		SHAPEBITS(root_weave_bits, root_weave_width, root_weave_height);
		SHAPEBITS(vlines2_bits, vlines2_width, vlines2_height);
	}
	XClearWindow(display, window);
}

void
draw_shape(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	shapestruct *sp = &shapes[MI_SCREEN(mi)];
	XGCValues   gcv;

	if (!sp->init_bits)
		return;
	gcv.stipple = sp->pixmaps[NRAND(sp->init_bits)];
	gcv.foreground = (MI_NPIXELS(mi) > 2) ?
		MI_PIXEL(mi, NRAND(MI_NPIXELS(mi))) : MI_WIN_WHITE_PIXEL(mi);
	gcv.background = (MI_NPIXELS(mi) > 2) ?
		MI_PIXEL(mi, NRAND(MI_NPIXELS(mi))) : MI_WIN_BLACK_PIXEL(mi);
	XChangeGC(display, sp->stippledgc, GCStipple | GCForeground | GCBackground,
		  &gcv);
	if (NRAND(3)) {
		sp->w = sp->borderx + NRAND(sp->width - 2 * sp->borderx) *
			NRAND(sp->width) / sp->width;
		sp->h = sp->bordery + NRAND(sp->height - 2 * sp->bordery) *
			NRAND(sp->height) / sp->height;
		sp->x = NRAND(sp->width - sp->w);
		sp->y = NRAND(sp->height - sp->h);
		if (LRAND() & 1)
			XFillRectangle(display, window, sp->stippledgc,
				       sp->x, sp->y, sp->w, sp->h);
		else
			XFillArc(display, window, sp->stippledgc,
				 sp->x, sp->y, sp->w, sp->h, 0, 23040);
	} else {
		XPoint      triangleList[3];

		triangleList[0].x = sp->borderx + NRAND(sp->width - 2 * sp->borderx);
		triangleList[0].y = sp->bordery + NRAND(sp->height - 2 * sp->bordery);
		triangleList[1].x = sp->borderx + NRAND(sp->width - 2 * sp->borderx);
		triangleList[1].y = sp->bordery + NRAND(sp->height - 2 * sp->bordery);
		triangleList[2].x = sp->borderx + NRAND(sp->width - 2 * sp->borderx);
		triangleList[2].y = sp->bordery + NRAND(sp->height - 2 * sp->bordery);
		XFillPolygon(display, window, sp->stippledgc, triangleList, 3,
			     Convex, CoordModeOrigin);
	}


	if (++sp->time > MI_CYCLES(mi))
		init_shape(mi);
}

void
release_shape(ModeInfo * mi)
{
	int         bits;

	if (shapes != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			shapestruct *sp = &shapes[screen];

			if (sp != NULL)
				XFreeGC(MI_DISPLAY(mi), sp->stippledgc);
			for (bits = 0; bits < sp->init_bits; bits++)
				XFreePixmap(MI_DISPLAY(mi), sp->pixmaps[bits]);
		}
		(void) free((void *) shapes);
		shapes = NULL;
	}
}

void
refresh_shape(ModeInfo * mi)
{
	/* Do nothing, it will refresh by itself, or pretty damn close */
}
