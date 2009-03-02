/* -*- Mode: C; tab-width: 4 -*- */
/* shape --- basic in your face shapes */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)shape.c	4.07 97/11/24 xlockmore";

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
 * 12-Mar-98: Got the idea for shadow from looking at xscreensaver web page.
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

#ifndef STIPPLING
/*-
 * For Linux (others, Intel based?) this could be deadly for the client.
 *   If you have 15/16 bit displays it may want a hard reboot after
 *   1 minute.  Not true on all systems... may be a XFree86 based problem.
 *   Also what is it with those corruption lines in some of the shapes?
 * 3 degrees for STIPPLING ...
 *     0: absolutely not
 *     1: if user insists (default for Linux)
 *     2: permit (also no warning message) (default for other OS's)
 */
#ifdef __linux__
#if 0
#define STIPPLING 0
#else
#define STIPPLING 1
#endif
#else
#define STIPPLING 2
#endif
#endif

/*-
 * "shade" is used over "shadow" because of possible confusion with
 * password shadowing
 */

#define DEF_SHADE  "True"
#define DEF_BORDER  "False"
#if (STIPPLING > 1)
#define DEF_STIPPLE  "True"
#else
#define DEF_STIPPLE  "False"
#endif

static Bool shade;
static Bool border;
static Bool stipple;

static XrmOptionDescRec opts[] =
{
	{"-shade", ".shape.shade", XrmoptionNoArg, (caddr_t) "on"},
	{"+shade", ".shape.shade", XrmoptionNoArg, (caddr_t) "off"},
	{"-border", ".shape.border", XrmoptionNoArg, (caddr_t) "on"},
	{"+border", ".shape.border", XrmoptionNoArg, (caddr_t) "off"},
	{"-stipple", ".shape.stipple", XrmoptionNoArg, (caddr_t) "on"},
	{"+stipple", ".shape.stipple", XrmoptionNoArg, (caddr_t) "off"}
};
static argtype vars[] =
{
	{(caddr_t *) & shade, "shade", "Shade", DEF_SHADE, t_Bool},
	{(caddr_t *) & border, "border", "Border", DEF_BORDER, t_Bool},
	{(caddr_t *) & stipple, "stipple", "Stipple", DEF_STIPPLE, t_Bool}
};
static OptionStruct desc[] =
{
	{"-/+shade", "turn on/off shape's shadows"},
	{"-/+border", "turn on/off shape's borders"},
	{"-/+stipple", "turn on/off shape's stippling"}
};

ModeSpecOpt shape_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   shape_description =
{"shape", "init_shape", "draw_shape", "release_shape",
 "refresh_shape", "init_shape", NULL, &shape_opts,
 10000, 100, 256, 1, 64, 1.0, "",
 "Shows overlaying rectangles, ellipses, and triangles", 0, NULL};

#endif

#define MAX_SHADE_OFFSET 32
#define MIN_SHADE_OFFSET 2

#define NBITS 12

#include "bitmaps/gray1.xbm"
#include "bitmaps/gray3.xbm"
#include "bitmaps/stipple.xbm"
#include "bitmaps/cross_weave.xbm"
#include "bitmaps/dimple1.xbm"
#include "bitmaps/dimple3.xbm"
#include "bitmaps/flipped_gray.xbm"
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
	Bool        shade;
	Bool        border;
	Bool        stipple;
	XPoint      shade_offset;
	int         init_bits;
	Pixmap      pixmaps[NBITS];
} shapestruct;

static shapestruct *shapes = NULL;

void
init_shape(ModeInfo * mi)
{
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

	if (MI_WIN_IS_FULLRANDOM(mi))
		sp->shade = (Bool) (LRAND() & 1);
	else
		sp->shade = shade;

	if (shade) {
		sp->shade_offset.x = (NRAND(MAX_SHADE_OFFSET - MIN_SHADE_OFFSET) +
				MIN_SHADE_OFFSET) * ((LRAND() & 1) ? 1 : -1);
		sp->shade_offset.y = (NRAND(MAX_SHADE_OFFSET - MIN_SHADE_OFFSET) +
				MIN_SHADE_OFFSET) * ((LRAND() & 1) ? 1 : -1);
	}
	if (MI_WIN_IS_FULLRANDOM(mi))
		sp->border = (Bool) (LRAND() & 1);
	else
		sp->border = border;

#if (STIPPLING > 1)
	if (MI_WIN_IS_FULLRANDOM(mi))
		sp->stipple = (Bool) (LRAND() & 1);
	else
		sp->stipple = stipple;
#else
#if (STIPPLING < 1)
	sp->stipple = False;
#else
	sp->stipple = stipple;	/* OK if you insist... could be dangerous */
#endif
#endif
	if (sp->stipple && !sp->init_bits) {
		Display    *display = MI_DISPLAY(mi);
		Window      window = MI_WINDOW(mi);

#if defined( __linux__ ) && ( STIPPLING < 2 )
		if ((MI_WIN_IS_VERBOSE(mi) || MI_WIN_IS_DEBUG(mi)) &&
		    (MI_DEPTH(mi) == 15 || MI_DEPTH(mi) == 16))
			(void) fprintf(stderr, "This may cause a LOCKUP in seconds!!!\n");
#endif
		SHAPEBITS(gray1_bits, gray1_width, gray1_height);
		SHAPEBITS(gray3_bits, gray3_width, gray3_height);
		SHAPEBITS(cross_weave_bits, cross_weave_width, cross_weave_height);
		SHAPEBITS(dimple1_bits, dimple1_width, dimple1_height);
		SHAPEBITS(dimple3_bits, dimple3_width, dimple3_height);
		SHAPEBITS(flipped_gray_bits, flipped_gray_width, flipped_gray_height);
		SHAPEBITS(vlines3_bits, vlines3_width, vlines3_height);
		SHAPEBITS(stipple_bits, stipple_width, stipple_height);
		SHAPEBITS(hlines2_bits, hlines2_width, hlines2_height);
		SHAPEBITS(light_gray_bits, light_gray_width, light_gray_height);
		SHAPEBITS(root_weave_bits, root_weave_width, root_weave_height);
		SHAPEBITS(vlines2_bits, vlines2_width, vlines2_height);
	}
	MI_CLEARWINDOW(mi);
}

void
draw_shape(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	shapestruct *sp = &shapes[MI_SCREEN(mi)];
	int         shape, i;
	XPoint      shade_offset;

	shade_offset.x = shade_offset.y = 0;
	if (sp->stipple)
		if (!sp->init_bits)
			return;

	if (sp->stipple) {
		XSetBackground(display, gc, (MI_NPIXELS(mi) > 2) ?
		   MI_PIXEL(mi, NRAND(MI_NPIXELS(mi))) : MI_BLACK_PIXEL(mi));
	}
	if (sp->border) {
		XSetLineAttributes(display, gc, 2, LineSolid, CapNotLast, JoinRound);
	}
	shape = NRAND(3);
	if (sp->shade) {
		i = MAX(ABS(sp->shade_offset.x), ABS(sp->shade_offset.y));
		shade_offset.x = ((float) NRAND(i) + 2.0) / (1.0 + i) * sp->shade_offset.x;
		shade_offset.y = ((float) NRAND(i) + 2.0) / (1.0 + i) * sp->shade_offset.y;
		/* This is over-simplistic... it casts the same length shadow over
		 * different depth objects.
		 */
	}
	if (shape == 2) {
		XPoint      triangleList[4];
		XPoint      triangleShade[3];

		triangleList[0].x = sp->borderx + NRAND(sp->width - 2 * sp->borderx);
		triangleList[0].y = sp->bordery + NRAND(sp->height - 2 * sp->bordery);
		triangleList[1].x = sp->borderx + NRAND(sp->width - 2 * sp->borderx);
		triangleList[1].y = sp->bordery + NRAND(sp->height - 2 * sp->bordery);
		triangleList[2].x = sp->borderx + NRAND(sp->width - 2 * sp->borderx);
		triangleList[2].y = sp->bordery + NRAND(sp->height - 2 * sp->bordery);
		triangleList[3].x = triangleList[0].x;
		triangleList[3].y = triangleList[0].y;
		if (sp->shade) {
			for (i = 0; i < 3; i++) {
				triangleShade[i].x = triangleList[i].x + shade_offset.x;
				triangleShade[i].y = triangleList[i].y + shade_offset.y;
			}
			if (sp->stipple) {
				XSetStipple(display, gc, sp->pixmaps[0]);
				XSetFillStyle(display, gc, FillStippled);
			}
			XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
			XFillPolygon(display, window, gc, triangleShade, 3,
				     Convex, CoordModeOrigin);
		}
		if (sp->stipple) {
			XSetStipple(display, gc, sp->pixmaps[NRAND(sp->init_bits)]);
			XSetFillStyle(display, gc, FillOpaqueStippled);
		}
		XSetForeground(display, gc, (MI_NPIXELS(mi) > 2) ?
		   MI_PIXEL(mi, NRAND(MI_NPIXELS(mi))) : MI_WHITE_PIXEL(mi));
		XFillPolygon(display, window, gc, triangleList, 3,
			     Convex, CoordModeOrigin);
		if (sp->border) {
			XSetForeground(display, gc, (MI_NPIXELS(mi) > 2) ?
				       MI_PIXEL(mi, NRAND(MI_NPIXELS(mi))) : MI_BLACK_PIXEL(mi));
			XDrawLines(display, window, gc, triangleList, 4, CoordModeOrigin);
		}
	} else {
		sp->w = sp->borderx + NRAND(sp->width - 2 * sp->borderx) *
			NRAND(sp->width) / sp->width;
		sp->h = sp->bordery + NRAND(sp->height - 2 * sp->bordery) *
			NRAND(sp->height) / sp->height;
		sp->x = NRAND(sp->width - sp->w);
		sp->y = NRAND(sp->height - sp->h);
		if (shape) {
			if (sp->shade) {
				if (sp->stipple) {
					XSetStipple(display, gc, sp->pixmaps[0]);
					XSetFillStyle(display, gc, FillStippled);
				}
				XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
				XFillArc(display, window, gc,
					 sp->x + shade_offset.x, sp->y + shade_offset.y,
					 sp->w, sp->h, 0, 23040);
			}
			if (sp->stipple) {
				XSetStipple(display, gc, sp->pixmaps[NRAND(sp->init_bits)]);
				XSetFillStyle(display, gc, FillOpaqueStippled);
			}
			XSetForeground(display, gc, (MI_NPIXELS(mi) > 2) ?
				       MI_PIXEL(mi, NRAND(MI_NPIXELS(mi))) : MI_WHITE_PIXEL(mi));
			XFillArc(display, window, gc,
				 sp->x, sp->y, sp->w, sp->h, 0, 23040);
			if (sp->border) {
				XSetForeground(display, gc, (MI_NPIXELS(mi) > 2) ?
					       MI_PIXEL(mi, NRAND(MI_NPIXELS(mi))) : MI_BLACK_PIXEL(mi));
				XDrawArc(display, window, gc,
				       sp->x, sp->y, sp->w, sp->h, 0, 23040);
			}
		} else {
			if (sp->shade) {
				if (sp->stipple) {
					XSetStipple(display, gc, sp->pixmaps[0]);
					XSetFillStyle(display, gc, FillStippled);
				}
				XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
				XFillRectangle(display, window, gc,
					       sp->x + shade_offset.x, sp->y + shade_offset.y, sp->w, sp->h);
			}
			if (sp->stipple) {
				XSetStipple(display, gc, sp->pixmaps[NRAND(sp->init_bits)]);
				XSetFillStyle(display, gc, FillOpaqueStippled);
			}
			XSetForeground(display, gc, (MI_NPIXELS(mi) > 2) ?
				       MI_PIXEL(mi, NRAND(MI_NPIXELS(mi))) : MI_WHITE_PIXEL(mi));
			XFillRectangle(display, window, gc,
				       sp->x, sp->y, sp->w, sp->h);
			if (sp->border) {
				XSetForeground(display, gc, (MI_NPIXELS(mi) > 2) ?
					       MI_PIXEL(mi, NRAND(MI_NPIXELS(mi))) : MI_BLACK_PIXEL(mi));
				XDrawRectangle(display, window, gc,
					       sp->x, sp->y, sp->w, sp->h);
			}
		}
	}
	if (sp->border)
		XSetLineAttributes(display, gc, 1, LineSolid, CapNotLast, JoinRound);
	if (sp->stipple)
		XSetFillStyle(display, gc, FillSolid);
	XFlush(display);
	if (++sp->time > MI_CYCLES(mi))
		init_shape(mi);
}

void
release_shape(ModeInfo * mi)
{
	if (shapes != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			shapestruct *sp = &shapes[screen];
			int         bits;

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
	/* Do nothing, it will refresh by itself */
}
