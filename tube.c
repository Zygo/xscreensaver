/* -*- Mode: C; tab-width: 4 -*- */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)tube.c 4.03 97/05/10 xlockmore";

#endif

/*-
 * tube.c - animated tube for xlock, the X Window System lockscreen
 *
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
#define PROGCLASS      "Tube"
#define HACK_INIT      init_tube
#define HACK_DRAW      draw_tube
#define DEF_DELAY      25000
#define DEF_CYCLES     20000
#define DEF_SIZE       -200
#define DEF_NCOLORS    200
#define SMOOTH_COLORS
#define WRITABLE_COLORS
#include "xlockmore.h"    /* from the xscreensaver distribution */
#include <X11/Xutil.h>
#else /* !STANDALONE */
#include "xlock.h"    /* from the xlockmore distribution */
ModeSpecOpt tube_opts =
{0, NULL, 0, NULL, NULL};
 
#endif /* !STANDALONE */

#define MINSIZE 3

typedef struct {
	int         dx1, dy1;
	int         x1, y1;
	int         width;
	int         height;
	unsigned int cur_color;
	Colormap    cmap;
	XColor     *colors;
	XColor      top;
	XColor      bottom;
	unsigned int fg, bg;
	XColor      fgcol, bgcol;	/* foreground and background colour specs */
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
	tubestruct *tp;
	int         screen_width, screen_height, preserve;
	int         size = MI_SIZE(mi);
	Bool        truecolor;
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

	tp->dx1 = NRAND(2);
	if (tp->dx1 == 0)
		tp->dx1 = -1;
	tp->dy1 = NRAND(2);
	if (tp->dy1 == 0)
		tp->dy1 = -1;
	tp->x1 = NRAND(screen_width - tp->width);
	tp->y1 = NRAND(screen_height - tp->height);
	XClearWindow(display, MI_WINDOW(mi));
	tp->counter = 0;

	tp->fg = MI_FG_COLOR(mi);
	tp->bg = MI_BG_COLOR(mi);
	tp->fgcol.pixel = tp->fg;
	tp->bgcol.pixel = tp->bg;
	preserve = preserveColors(tp->fg, tp->bg,
			     MI_WIN_WHITE_PIXEL(mi), MI_WIN_BLACK_PIXEL(mi));
	XQueryColor(display, MI_WIN_COLORMAP(mi), &(tp->fgcol));
	XQueryColor(display, MI_WIN_COLORMAP(mi), &(tp->bgcol));

	tp->fixed_colormap = !setupColormap(mi,
		&(tp->ncolors), &truecolor, &redmask, &greenmask, &bluemask);
	if (!tp->fixed_colormap) {
		/* color in "the bottom third */
		tp->bottom.red = NRAND(65536 / 3);
		tp->bottom.blue = NRAND(65536 / 3);
		tp->bottom.green = NRAND(65536 / 3);
		/* color in "the top third */
		tp->top.red = NRAND(65536 / 3) + 65536 * 2 / 3;
		tp->top.blue = NRAND(65536 / 3) + 65536 * 2 / 3;
		tp->top.green = NRAND(65536 / 3) + 65536 * 2 / 3;

		/* allocate colormap, if needed */
		if (tp->colors == NULL)
			tp->colors = (XColor *) malloc(sizeof (XColor) * tp->ncolors);
		if (tp->cmap == None)
			tp->cmap = XCreateColormap(display,
				     MI_WINDOW(mi), MI_VISUAL(mi), AllocAll);
	}
	tp->usable_colors = tp->ncolors - preserve;
}

void
draw_tube(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	tubestruct *tp = &tubes[MI_SCREEN(mi)];
	unsigned int i, j;
	int         k;

	/* advance drawing color */
	tp->cur_color = (tp->cur_color + 1) % MI_NPIXELS(mi);
	if (!tp->fixed_colormap && tp->usable_colors > 2) {
		while (tp->cur_color == MI_WIN_WHITE_PIXEL(mi) ||
		       tp->cur_color == MI_WIN_BLACK_PIXEL(mi) ||
		       tp->cur_color == MI_FG_COLOR(mi) ||
		       tp->cur_color == MI_BG_COLOR(mi)) {
			tp->cur_color = (tp->cur_color + 1) % MI_NPIXELS(mi);
		}
	}
	XSetForeground(display, gc, tp->cur_color);

	/* move rectangle forward, horiz */
	tp->x1 += tp->dx1;
	if (tp->x1 < 0) {
		tp->x1 = 0;
		tp->dx1 = -tp->dx1;
	}
	if (tp->x1 + tp->width >= MI_WIN_WIDTH(mi)) {
		tp->x1 = MI_WIN_WIDTH(mi) - tp->width - 1;
		tp->dx1 = -tp->dx1;
	}
	/* move rectange forward, vert */
	tp->y1 += tp->dy1;
	if (tp->y1 < 0) {
		tp->y1 = 0;
		tp->dy1 = -tp->dy1;
	}
	if (tp->y1 + tp->height >= MI_WIN_HEIGHT(mi)) {
		tp->y1 = MI_WIN_HEIGHT(mi) - tp->height - 1;
		tp->dy1 = -tp->dy1;
	}
	/* draw the rectangle */
	XDrawLine(display, MI_WINDOW(mi), gc,
		  tp->x1, tp->y1,
		  tp->x1, tp->y1 + tp->height);
	XDrawLine(display, MI_WINDOW(mi), gc,
		  tp->x1, tp->y1 + tp->height,
		  tp->x1 + tp->width, tp->y1 + tp->height);
	XDrawLine(display, MI_WINDOW(mi), gc,
		  tp->x1 + tp->width, tp->y1 + tp->height,
		  tp->x1 + tp->width, tp->y1);
	XDrawLine(display, MI_WINDOW(mi), gc,
		  tp->x1 + tp->width, tp->y1,
		  tp->x1, tp->y1);

	/* advance colormap */
	if (!tp->fixed_colormap && tp->usable_colors > 2) {
		for (i = 0, j = tp->cur_color, k = 0;
		     i < tp->ncolors; i++) {
			if (i == MI_WIN_WHITE_PIXEL(mi)) {
				tp->colors[i].pixel = i;
				tp->colors[i].red = 65535;
				tp->colors[i].blue = 65535;
				tp->colors[i].green = 65535;
				tp->colors[i].flags = DoRed | DoGreen | DoBlue;
			} else if (i == MI_WIN_BLACK_PIXEL(mi)) {
				tp->colors[i].pixel = i;
				tp->colors[i].red = 0;
				tp->colors[i].blue = 0;
				tp->colors[i].green = 0;
				tp->colors[i].flags = DoRed | DoGreen | DoBlue;
			} else if (i == tp->fg) {
				tp->colors[i] = tp->fgcol;
			} else if (i == tp->bg) {
				tp->colors[i] = tp->bgcol;
			} else {
				double      range;

				tp->colors[i].pixel = i;
				range = ((double) (tp->top.red - tp->bottom.red)) /
					MI_NPIXELS(mi);
				tp->colors[i].red = (short unsigned int) (range * j +
							     tp->bottom.red);
				range = ((double) (tp->top.green - tp->bottom.green)) /
					MI_NPIXELS(mi);
				tp->colors[i].green = (short unsigned int) (range * j +
							   tp->bottom.green);
				range = ((double) (tp->top.blue - tp->bottom.blue)) /
					MI_NPIXELS(mi);
				tp->colors[i].blue = (short unsigned int) (range * j +
							    tp->bottom.blue);
				tp->colors[i].flags = DoRed | DoGreen | DoBlue;
				j = (j + 1) % MI_NPIXELS(mi);
				k++;
			}
		}
		/* make the entire tube move forward */
		XStoreColors(display, tp->cmap, tp->colors, MI_NPIXELS(mi));
		setColormap(display, MI_WINDOW(mi), tp->cmap, MI_WIN_IS_INWINDOW(mi));
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

			if (tp->cmap != None)
				XFreeColormap(MI_DISPLAY(mi), tp->cmap);
			if (tp->colors != NULL)
				XFree((void *) tp->colors);
		}
		(void) free((void *) tubes);
		tubes = NULL;
	}
}
