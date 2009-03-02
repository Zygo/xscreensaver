/* -*- Mode: C; tab-width: 4 -*- */
/* mountain -- square grid mountains */

#if 0
static const char sccsid[] = "@(#)mountain.c	5.00 2000/11/01 xlockmore";
#endif

/*-
 * Copyright (c) 1995 Pascal Pensa <pensa@aurora.unice.fr>
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
 * 01-Nov-2000: Allocation checks
 * 10-May-1997: Compatible with xscreensaver
 * 1995: Written
 */

#ifdef STANDALONE
#define MODE_mountain
#define PROGCLASS "Mountain"
#define HACK_INIT init_mountain
#define HACK_DRAW draw_mountain
#define mountain_opts xlockmore_opts
#define DEFAULTS "*delay: 1000 \n" \
 "*count: 30 \n" \
 "*cycles: 4000 \n" \
 "*ncolors: 64 \n"
#define SMOOTH_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_mountain

ModeSpecOpt mountain_opts =
{0, (XrmOptionDescRec *) NULL, 0, (argtype *) NULL, (OptionStruct *) NULL};

#ifdef USE_MODULES
ModStruct   mountain_description =
{"mountain", "init_mountain", "draw_mountain", "release_mountain",
 "refresh_mountain", "init_mountain", (char *) NULL, &mountain_opts,
 1000, 30, 4000, 1, 64, 1.0, "",
 "Shows Papo's mountain range", 0, NULL};

#endif

/* ~ 5000 Max mountain height (1000 - 10000) */
#define MAXHEIGHT  (3 * (mp->width + mp->height))

#define WORLDWIDTH 50		/* World size x * y */

#define RANGE_RAND(min,max) ((min) + NRAND((max) - (min)))

typedef struct {
	int         pixelmode;
	int         width;
	int         height;
	int         x, y;
	int         offset;
	int         stage;
	int         h[WORLDWIDTH][WORLDWIDTH];
	long        time;	/* up time */
	Bool        wireframe;
	Bool        joke;
	GC          stippledGC;
} mountainstruct;

static mountainstruct *mountains = (mountainstruct *) NULL;

static void
spread(int  (*m)[50], int x, int y)
{
	int         x2, y2;
	int         h = m[x][y];

	for (y2 = y - 1; y2 <= y + 1; y2++)
		for (x2 = x - 1; x2 <= x + 1; x2++)
			if (x2 >= 0 && y2 >= 0 && x2 < WORLDWIDTH && y2 < WORLDWIDTH)
				m[x2][y2] = (m[x2][y2] + h) / 2;
}

static void
drawamountain(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	mountainstruct *mp = &mountains[MI_SCREEN(mi)];
	int         x2, y2, x3, y3, y4, y5, c = 0;
	XPoint      p[5];

	if (MI_NPIXELS(mi) > 2) {
		c = (mp->h[mp->x][mp->y] + mp->h[mp->x + 1][mp->y] +
		  mp->h[mp->x][mp->y + 1] + mp->h[mp->x + 1][mp->y + 1]) / 4;
		c = (c / 10 + mp->offset) % MI_NPIXELS(mi);
	}
	x2 = mp->x * (2 * mp->width) / (3 * WORLDWIDTH);
	y2 = mp->y * (2 * mp->height) / (3 * WORLDWIDTH);
	p[0].x = (x2 - y2 / 2) + (mp->width / 4);
	p[0].y = (y2 - mp->h[mp->x][mp->y]) + mp->height / 4;

	x3 = (mp->x + 1) * (2 * mp->width) / (3 * WORLDWIDTH);
	y3 = mp->y * (2 * mp->height) / (3 * WORLDWIDTH);
	p[1].x = (x3 - y3 / 2) + (mp->width / 4);
	p[1].y = (y3 - mp->h[mp->x + 1][mp->y]) + mp->height / 4;

	y4 = (mp->y + 1) * (2 * mp->height) / (3 * WORLDWIDTH);
	p[2].x = (x3 - y4 / 2) + (mp->width / 4);
	p[2].y = (y4 - mp->h[mp->x + 1][mp->y + 1]) + mp->height / 4;

	y5 = (mp->y + 1) * (2 * mp->height) / (3 * WORLDWIDTH);
	p[3].x = (x2 - y5 / 2) + (mp->width / 4);
	p[3].y = (y5 - mp->h[mp->x][mp->y + 1]) + mp->height / 4;

	p[4].x = p[0].x;
	p[4].y = p[0].y;

	if (MI_NPIXELS(mi) > 2)
		XSetForeground(display, gc, MI_PIXEL(mi, c));
	else
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));

	if (mp->joke) {
		if ((Bool) (LRAND() & 1))
			XDrawLines(display, window, gc, p, 5, CoordModeOrigin);
		else {
			XFillPolygon(display, window, gc, p, 4, Complex, CoordModeOrigin);
			if (!mp->pixelmode) {
				XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
				XDrawLines(display, window, gc, p, 5, CoordModeOrigin);
			}
		}
	} else {
		if (mp->wireframe) {
			XDrawLines(display, window, gc, p, 5, CoordModeOrigin);
		} else {
			XFillPolygon(display, window, gc, p, 4, Complex, CoordModeOrigin);

			if (!mp->pixelmode) {
				XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
				XDrawLines(display, window, gc, p, 5, CoordModeOrigin);
			}
		}
	}
	mp->x++;
	if (mp->x == WORLDWIDTH - 1) {
		mp->y++;
		mp->x = 0;
	}
	if (mp->y == WORLDWIDTH - 1)
		mp->stage++;
}

void
init_mountain(ModeInfo * mi)
{
	int         i, j, x, y;
	XGCValues   gcv;
	mountainstruct *mp;

	if (mountains == NULL) {
		if ((mountains = (mountainstruct *) calloc(MI_NUM_SCREENS(mi),
					   sizeof (mountainstruct))) == NULL)
			return;
	}
	mp = &mountains[MI_SCREEN(mi)];

	mp->width = MI_WIDTH(mi);
	mp->height = MI_HEIGHT(mi);
	mp->pixelmode = (mp->width + mp->height < 200);
	mp->stage = 0;
	mp->time = 0;
	mp->x = mp->y = 0;
	if (MI_IS_FULLRANDOM(mi)) {
		mp->joke = (Bool) (NRAND(10) == 0);
		mp->wireframe = (Bool) (LRAND() & 1);
	} else {
		mp->joke = False;
		mp->wireframe = MI_IS_WIREFRAME(mi);
	}

	if (mp->stippledGC == None) {
		gcv.foreground = MI_WHITE_PIXEL(mi);
		gcv.background = MI_BLACK_PIXEL(mi);
		if ((mp->stippledGC = XCreateGC(MI_DISPLAY(mi), MI_WINDOW(mi),
					  GCForeground | GCBackground, &gcv)) == None)
			return;
	}
	MI_CLEARWINDOW(mi);

	for (y = 0; y < (int) WORLDWIDTH; y++)
		for (x = 0; x < (int) WORLDWIDTH; x++)
			mp->h[x][y] = 0;

	j = MI_COUNT(mi);
	if (j < 0)
		j = NRAND(-j) + 1;
	for (i = 0; i < j; i++)
		mp->h[RANGE_RAND(1, WORLDWIDTH - 1)][RANGE_RAND(1, WORLDWIDTH - 1)] =
			NRAND(MAXHEIGHT);

	for (y = 0; y < WORLDWIDTH; y++)
		for (x = 0; x < WORLDWIDTH; x++)
			spread(mp->h, x, y);

	for (y = 0; y < WORLDWIDTH; y++)
		for (x = 0; x < WORLDWIDTH; x++) {
			mp->h[x][y] = mp->h[x][y] + NRAND(10) - 5;
			if (mp->h[x][y] < 10)
				mp->h[x][y] = 0;
		}

	if (MI_NPIXELS(mi) > 2)
		mp->offset = NRAND(MI_NPIXELS(mi));
	else
		mp->offset = 0;
}

void
draw_mountain(ModeInfo * mi)
{
	mountainstruct *mp;

	if (mountains == NULL)
			return;
	mp = &mountains[MI_SCREEN(mi)];
	if (mp->stippledGC == NULL)
			return;

	MI_IS_DRAWN(mi) = True;

	switch (mp->stage) {
		case 0:
			drawamountain(mi);
			break;
		case 1:
			if (++mp->time > MI_CYCLES(mi))
				mp->stage++;
			break;
		case 2:
			init_mountain(mi);
			break;
	}
}

void
release_mountain(ModeInfo * mi)
{
	if (mountains != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			mountainstruct *mp = &mountains[screen];

			if (mp->stippledGC)
				XFreeGC(MI_DISPLAY(mi), mp->stippledGC);
		}
		(void) free((void *) mountains);
		mountains = (mountainstruct *) NULL;
	}
}

void
refresh_mountain(ModeInfo * mi)
{
	mountainstruct *mp;

	if (mountains == NULL)
			return;
	mp = &mountains[MI_SCREEN(mi)];

	MI_CLEARWINDOW(mi);
	mp->x = 0;
	mp->y = 0;
}

#endif /* MODE_mountain */
