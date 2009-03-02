/* -*- Mode: C; tab-width: 4 -*- */
/* tube --- animated tube */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)tube.c 4.07 97/11/24 xlockmore";

#endif

/*-
 * Copyright (c) 1997 Dan Stromberg <strombrg@nis.acs.uci.edu>
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
 * 4-Mar-97: Memory leak fix by Tom Schmidt <tschmidt@micron.com>
 * 7-Feb-97: Written by Dan Stromberg <strombrg@nis.acs.uci.edu>
 */


#ifdef STANDALONE
#define PROGCLASS "Tube"
#define HACK_INIT init_tube
#define HACK_DRAW draw_tube
#define tube_opts xlockmore_opts
#define DEFAULTS "*delay: 25000 \n" \
 "*cycles: 20000 \n" \
 "*size: -200 \n" \
 "*ncolors: 200 \n"
#define SMOOTH_COLORS
#define WRITABLE_COLORS
#include "xlockmore.h"		/* from the xscreensaver distribution */
#include <X11/Xutil.h>
#else /* !STANDALONE */
#include "xlock.h"		/* from the xlockmore distribution */
#endif /* !STANDALONE */

ModeSpecOpt tube_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   tube_description =
{"tube", "init_tube", "draw_tube", "release_tube",
 NULL, "init_tube", NULL, &tube_opts,
 25000, -9, 20000, -200, 64, 1.0, "",
 "Shows an animated tube", 0, NULL};

#endif

#define MINSIZE 3
#define MINSHAPE 1

typedef struct {
	int         dx1, dy1;
	int         x1, y1;
	int         width, height, average, linewidth;
	int         shape;
	XPoint     *pts, *proto_pts;
	unsigned int cur_color;
	Colormap    cmap;
	XColor     *colors;
	XColor      top;
	XColor      bottom;
	XColor      bgcol, fgcol;	/* foreground and background colour specs */
	int         counter;
	int         ncolors;
	int         usable_colors;
	Bool        fixed_colormap;
} tubestruct;

static tubestruct *tubes = NULL;

void
init_tube(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	tubestruct *tp;
	int         screen_width, screen_height, preserve;
	int         size = MI_SIZE(mi);
	Bool        truecolor;
	int         star = 1, lw;
	unsigned long redmask, greenmask, bluemask;

	if (tubes == NULL) {
		if ((tubes = (tubestruct *) calloc(MI_NUM_SCREENS(mi),
					       sizeof (tubestruct))) == NULL)
			return;
	}
	tp = &tubes[MI_SCREEN(mi)];

	screen_width = MI_WIN_WIDTH(mi);
	screen_height = MI_WIN_HEIGHT(mi);

	if (size < -MINSIZE) {
		tp->width = NRAND(MIN(-size, MAX(MINSIZE, screen_width / 2)) -
				  MINSIZE + 1) + MINSIZE;
		tp->height = NRAND(MIN(-size, MAX(MINSIZE, screen_height / 2)) -
				   MINSIZE + 1) + MINSIZE;
	} else if (size < MINSIZE) {
		if (!size) {
			tp->width = MAX(MINSIZE, screen_width / 2);
			tp->height = MAX(MINSIZE, screen_height / 2);
		} else {
			tp->width = MINSIZE;
			tp->height = MINSIZE;
		}
	} else {
		tp->width = MIN(size, MAX(MINSIZE, screen_width / 2));
		tp->height = MIN(size, MAX(MINSIZE, screen_height / 2));
	}
	tp->average = (tp->width + tp->height) / 2;

	tp->dx1 = NRAND(2);
	if (tp->dx1 == 0)
		tp->dx1 = -1;
	tp->dy1 = NRAND(2);
	if (tp->dy1 == 0)
		tp->dy1 = -1;
	if (tp->pts) {
		(void) free((void *) tp->pts);
		tp->pts = NULL;
	}
	if (tp->proto_pts) {
		(void) free((void *) tp->proto_pts);
		tp->proto_pts = NULL;
	}
	tp->shape = MI_BATCHCOUNT(mi);
	if (tp->shape < -MINSHAPE) {
		tp->shape = NRAND(-tp->shape - MINSHAPE + 1) + MINSHAPE;
	} else if (tp->shape < MINSHAPE)
		tp->shape = MINSHAPE;

	if (tp->shape >= 3) {
		int         i;
		float       start;

		tp->width = tp->height = tp->average;
		tp->pts = (XPoint *) malloc((tp->shape + 1) * sizeof (XPoint));
		tp->proto_pts = (XPoint *) malloc((tp->shape + 1) * sizeof (XPoint));
		start = (float) NRAND(360);
		do {
			star = NRAND(tp->shape / 2) + 1;	/* Not always a star but thats ok. */
		} while ((star != 1) && (!(tp->shape % star)));
		for (i = 0; i < tp->shape; i++) {
			tp->proto_pts[i].x = tp->average / 2 +
				(int) (cos((double) start * 2.0 * M_PI / 360.0) * tp->average / 2.0);
			tp->proto_pts[i].y = tp->average / 2 +
				(int) (sin((double) start * 2.0 * M_PI / 360.0) * tp->average / 2.0);
			start += star * 360 / tp->shape;
		}
		tp->proto_pts[tp->shape] = tp->proto_pts[0];
	}
	tp->linewidth = NRAND(tp->average / 6 + 1) + 1;
	if (star > 1)		/* Make the width less */
		tp->linewidth = NRAND(tp->linewidth / 2 + 1) + 1;
	lw = tp->linewidth / 2;
	tp->x1 = NRAND(screen_width - tp->width - 2 * lw) + lw;
	tp->y1 = NRAND(screen_height - tp->height - 2 * lw) + lw;

	MI_CLEARWINDOW(mi);

	tp->counter = 0;

	tp->fixed_colormap = !setupColormap(mi,
		&(tp->ncolors), &truecolor, &redmask, &greenmask, &bluemask);
	if (!tp->fixed_colormap) {
		preserve = preserveColors(MI_BLACK_PIXEL(mi), MI_WHITE_PIXEL(mi),
					  MI_BG_PIXEL(mi), MI_FG_PIXEL(mi));
		tp->bgcol.pixel = MI_BG_PIXEL(mi);
		tp->fgcol.pixel = MI_FG_PIXEL(mi);
		XQueryColor(display, MI_COLORMAP(mi), &(tp->bgcol));
		XQueryColor(display, MI_COLORMAP(mi), &(tp->fgcol));

#define RANGE 65536
#define DENOM 2
		/* color in the bottom part */
		tp->bottom.red = NRAND(RANGE / DENOM);
		tp->bottom.blue = NRAND(RANGE / DENOM);
		tp->bottom.green = NRAND(RANGE / DENOM);
		/* color in the top part */
		tp->top.red = RANGE - NRAND(RANGE / DENOM);
		tp->top.blue = RANGE - NRAND(RANGE / DENOM);
		tp->top.green = RANGE - NRAND(RANGE / DENOM);

		/* allocate colormap, if needed */
		if (tp->colors == NULL)
			tp->colors = (XColor *) malloc(sizeof (XColor) * tp->ncolors);
		if (tp->cmap == None)
			tp->cmap = XCreateColormap(display, window,
						   MI_VISUAL(mi), AllocAll);
		tp->usable_colors = tp->ncolors - preserve;
	}
	tp->cur_color = NRAND(tp->ncolors);
}

void
draw_tube(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	tubestruct *tp = &tubes[MI_SCREEN(mi)];
	unsigned int i, j;
	int         k, lw = tp->linewidth / 2;

	/* advance drawing color */
	tp->cur_color = (tp->cur_color + 1) % tp->ncolors;
	if (!tp->fixed_colormap && tp->usable_colors > 2) {
		while (tp->cur_color == MI_WHITE_PIXEL(mi) ||
		       tp->cur_color == MI_BLACK_PIXEL(mi) ||
		       tp->cur_color == MI_BG_PIXEL(mi) ||
		       tp->cur_color == MI_FG_PIXEL(mi)) {
			tp->cur_color = (tp->cur_color + 1) % tp->ncolors;
		}
		XSetForeground(display, gc, tp->cur_color);
	} else
		XSetForeground(display, gc, MI_PIXEL(mi, tp->cur_color));

	/* move rectangle forward, horiz */
	tp->x1 += tp->dx1;
	if (tp->x1 < lw) {
		tp->x1 = lw;
		tp->dx1 = -tp->dx1;
	}
	if (tp->x1 + tp->width + lw >= MI_WIN_WIDTH(mi)) {
		tp->x1 = MI_WIN_WIDTH(mi) - tp->width - 1 - lw;
		tp->dx1 = -tp->dx1;
	}
	/* move rectange forward, vert */
	tp->y1 += tp->dy1;
	if (tp->y1 < lw) {
		tp->y1 = lw;
		tp->dy1 = -tp->dy1;
	}
	if (tp->y1 + tp->height + lw >= MI_WIN_HEIGHT(mi)) {
		tp->y1 = MI_WIN_HEIGHT(mi) - tp->height - 1 - lw;
		tp->dy1 = -tp->dy1;
	}
	XSetLineAttributes(display, gc, tp->linewidth + (tp->shape >= 2),
			   LineSolid, CapNotLast, JoinRound);
	if (tp->shape < 2)
		XDrawRectangle(display, window, gc,
			       tp->x1, tp->y1, tp->width, tp->height);
	else if (tp->shape == 2)
		XDrawArc(display, window, gc,
			 tp->x1, tp->y1, tp->width, tp->height, 0, 23040);
	else {
		for (i = 0; i <= (unsigned int) tp->shape; i++) {
			tp->pts[i].x = tp->x1 + tp->proto_pts[i].x;
			tp->pts[i].y = tp->y1 + tp->proto_pts[i].y;
		}
		XDrawLines(display, window, gc, tp->pts, tp->shape + 1,
			   CoordModeOrigin);
	}
	XSetLineAttributes(display, gc, 1, LineSolid, CapNotLast, JoinRound);

	/* advance colormap */
	if (!tp->fixed_colormap && tp->usable_colors > 2) {
		for (i = 0, j = tp->cur_color, k = 0;
		     i < (unsigned int) tp->ncolors; i++) {
			if (i == MI_WHITE_PIXEL(mi)) {
				tp->colors[i].pixel = i;
				tp->colors[i].red = 65535;
				tp->colors[i].blue = 65535;
				tp->colors[i].green = 65535;
				tp->colors[i].flags = DoRed | DoGreen | DoBlue;
			} else if (i == MI_BLACK_PIXEL(mi)) {
				tp->colors[i].pixel = i;
				tp->colors[i].red = 0;
				tp->colors[i].blue = 0;
				tp->colors[i].green = 0;
				tp->colors[i].flags = DoRed | DoGreen | DoBlue;
			} else if (i == MI_BG_PIXEL(mi)) {
				tp->colors[i] = tp->bgcol;
			} else if (i == MI_FG_PIXEL(mi)) {
				tp->colors[i] = tp->fgcol;
			} else {
				double      range;

				tp->colors[i].pixel = i;
				range = ((double) (tp->top.red - tp->bottom.red)) /
					tp->ncolors;
				tp->colors[i].red = (short unsigned int) (range * j +
							     tp->bottom.red);
				range = ((double) (tp->top.green - tp->bottom.green)) /
					tp->ncolors;
				tp->colors[i].green = (short unsigned int) (range * j +
							   tp->bottom.green);
				range = ((double) (tp->top.blue - tp->bottom.blue)) /
					tp->ncolors;
				tp->colors[i].blue = (short unsigned int) (range * j +
							    tp->bottom.blue);
				tp->colors[i].flags = DoRed | DoGreen | DoBlue;
				j = (j + 1) % tp->ncolors;
				k++;
			}
		}
		/* make the entire tube move forward */
		XStoreColors(display, tp->cmap, tp->colors, tp->ncolors);
		setColormap(display, window, tp->cmap, MI_WIN_IS_INWINDOW(mi));
	}
	tp->counter++;
	if (tp->counter > MI_CYCLES(mi)) {
		init_tube(mi);
	}
}

void
release_tube(ModeInfo * mi)
{
	if (tubes != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			tubestruct *tp = &tubes[screen];

			if (tp->pts)
				(void) free((void *) tp->pts);
			if (tp->proto_pts)
				(void) free((void *) tp->proto_pts);
			if (tp->cmap != None)
				XFreeColormap(MI_DISPLAY(mi), tp->cmap);
			if (tp->colors != NULL)
				XFree((caddr_t) tp->colors);
		}
		(void) free((void *) tubes);
		tubes = NULL;
	}
}
