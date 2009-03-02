/* -*- Mode: C; tab-width: 4 -*- */
/* daisy --- flowers in a field */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)daisy.c	4.07 97/11/24 xlockmore";

#endif

/*-
 * Copyright (c) 1996 by David Bagley.
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
 * 07-Aug-96: written.  Initially copied forest.c and made continual
 *            refinements, pyro was helpful too.  Based on a program
 *            I saw on a PC.
 *
 */

#ifdef STANDALONE
#define PROGCLASS "Daisy"
#define HACK_INIT init_daisy
#define HACK_DRAW draw_daisy
#define daisy_opts xlockmore_opts
#define DEFAULTS "*delay: 100000 \n" \
 "*count: 300 \n" \
 "*cycles: 350 \n" \
 "*ncolors: 200 \n"
#define BRIGHT_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */

#endif /* STANDALONE */

#define DEF_GARDEN  "False"

static Bool garden;

static XrmOptionDescRec opts[] =
{
	{"-garden", ".daisy.garden", XrmoptionNoArg, (caddr_t) "on"},
	{"+garden", ".daisy.garden", XrmoptionNoArg, (caddr_t) "off"}
};
static argtype vars[] =
{
	{(caddr_t *) & garden, "garden", "Garden", DEF_GARDEN, t_Bool}
};
static OptionStruct desc[] =
{
	{"-/+garden", "turn on/off garden"}
};

ModeSpecOpt daisy_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   daisy_description =
{"daisy", "init_daisy", "draw_daisy", "release_daisy",
 "refresh_daisy", "init_daisy", NULL, &daisy_opts,
 100000, 300, 350, 1, 64, 1.0, "",
 "Shows a meadow of daisies", 0, NULL};

#endif

#define GREEN	(23 * MI_NPIXELS(mi) / 64)
#define NOTGREEN	(7 * MI_NPIXELS(mi) / 64)
#define MINDAISIES   1

#define DROOP 20		/* Percentage x with relation to y */
#define MINHEIGHT  20		/* Daisy height range */
#define MAXHEIGHT  40

typedef struct {
	int         width;
	int         height;
	int         time;	/* up time */
	int         ndaisies;
	int         meadow_y;
	Bool        garden;
	float       step;
} daisystruct;

static daisystruct *daisies = NULL;

/* always green, straight for now, parabolic later */
static void
drawstem(ModeInfo * mi, XPoint start, XPoint stop)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);

	if (MI_NPIXELS(mi) > 2)
		XSetForeground(display, gc, MI_PIXEL(mi, GREEN));
	else
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
#if 1
	XDrawLine(display, window, gc, start.x, start.y, stop.x, stop.y);
#else
	XDrawArc(display, window, gc, stop.x, stop.y,
		 NRAND(50) + 30, start.y - stop.y + 1, 90 * 64, 170 * 64);
#endif
}

/* not green */
static unsigned long
drawpetals(ModeInfo * mi, XPoint center,
	   int size, int circles, int delta, int offset, int petals)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	unsigned long colour = 0;
	float       start_angle, inc_angle;
	XPoint      newcenter;
	int         petal, inc;
	float       sine, cosine;

	if (MI_NPIXELS(mi) > GREEN + NOTGREEN) {
		do {
			colour = NRAND(MI_NPIXELS(mi));
		} while (colour >= GREEN - NOTGREEN && colour <= GREEN + NOTGREEN);
	}
	start_angle = NRAND(360) * M_PI / 180;
	inc_angle = 2 * M_PI / petals;
	XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
	if (size > 2)
		XFillArc(display, window, gc,
			 center.x - size / 2, center.y - size / 2, size, size,
			 0, 23040);
	for (petal = 0; petal < petals; petal++) {
		sine = SINF(start_angle + petal * inc_angle);
		cosine = COSF(start_angle + petal * inc_angle);
		if (size > 2)
			if (MI_NPIXELS(mi) <= 2) {
				XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
				for (inc = 0; inc < circles; inc++) {
					newcenter.x = center.x + (int) (sine * (offset + delta * inc));
					newcenter.y = center.y + (int) (cosine * (offset + delta * inc));
					XDrawArc(display, window, gc,
						 newcenter.x - size / 2, newcenter.y - size / 2, size, size,
						 0, 23040);
				}
			}
		if (MI_NPIXELS(mi) > 2 && MI_NPIXELS(mi) > GREEN + NOTGREEN)
			XSetForeground(display, gc, MI_PIXEL(mi, colour));
		else
			XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
		for (inc = 0; inc < circles; inc++) {
			newcenter.x = center.x + (int) (sine * (offset + delta * inc));
			newcenter.y = center.y + (int) (cosine * (offset + delta * inc));
			if (size < 2)
				XDrawPoint(display, window, gc, newcenter.x, newcenter.y);
			else
				XFillArc(display, window, gc,
					 newcenter.x - size / 2, newcenter.y - size / 2, size, size,
					 0, 23040);
		}
	}
	return colour;
}

/* not green */
static void
drawcenter(ModeInfo * mi, XPoint center, int size, unsigned long petalcolour)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	unsigned long colour;

	if (MI_NPIXELS(mi) > GREEN + NOTGREEN) {
		do {
			/* Insure good contrast */
			colour = (NRAND(2 * MI_NPIXELS(mi) / 3) + petalcolour +
				  MI_NPIXELS(mi) / 6) % MI_NPIXELS(mi);
		} while (colour >= GREEN - NOTGREEN && colour <= GREEN + NOTGREEN);
		XSetForeground(display, gc, MI_PIXEL(mi, colour));
	} else
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
	if (size < 2)
		XDrawPoint(display, window, gc, center.x, center.y);
	else
		XFillArc(display, window, gc,
			 center.x - size / 2, center.y - size / 2, size, size, 0, 23040);
}

static void
drawdaisy(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	daisystruct *dp = &daisies[MI_SCREEN(mi)];
	XPoint      stem_start, stem_stop;
	int         height, droop;
	unsigned long colour;

	/* Care more about the flower being on the screen rather than the stem */
#ifdef LINEAR
	stem_stop.y = dp->meadow_y + dp->time * dp->step;
	height = (NRAND(MAXHEIGHT - MINHEIGHT + 1) + MINHEIGHT) * dp->height / 300 *
		2 * (dp->time + 20) / (dp->ndaisies + 20);
#else
	stem_stop.y = dp->meadow_y + (int) ((float) dp->time * dp->time *
					    dp->step / dp->ndaisies);
	height = (NRAND(MAXHEIGHT - MINHEIGHT + 1) + MINHEIGHT) * dp->height / 300 *
		2 * (dp->time * dp->time + 1) / (dp->ndaisies * dp->ndaisies + 1);
#endif
	stem_start.y = stem_stop.y + height;
	if (dp->garden) {
		stem_stop.x = ((LRAND() & 1) ? 1 : -1) *
			NRAND(dp->width / 4 * (dp->time + 1) / (dp->ndaisies + 20) +
			      dp->width / 4) + dp->width / 2;
	} else {
		stem_stop.x = NRAND(dp->width);
	}
	/* Give about droop left or right with 25% randomness */
	droop = ((LRAND() & 1) ? 1 : -1) * DROOP * (NRAND(50) + 75) / 100;
	stem_start.x = stem_stop.x + droop * height / 100;
	XSetLineAttributes(display, gc, height / 24 + 1,
			   LineSolid, CapNotLast, JoinRound);
	drawstem(mi, stem_start, stem_stop);
	XSetLineAttributes(display, gc, 1, LineSolid, CapNotLast, JoinRound);
	colour = drawpetals(mi, stem_stop, height / 6, 5, height / 32 + 1,
			    height / 7, NRAND(6) + 6);
	drawcenter(mi, stem_stop, height / 7, colour);
}


void
init_daisy(ModeInfo * mi)
{
	daisystruct *dp;

	if (daisies == NULL) {
		if ((daisies = (daisystruct *) calloc(MI_NUM_SCREENS(mi),
					      sizeof (daisystruct))) == NULL)
			return;
	}
	dp = &daisies[MI_SCREEN(mi)];

	dp->width = MI_WIN_WIDTH(mi);
	dp->height = MI_WIN_HEIGHT(mi);
	dp->time = 0;
	if (MI_WIN_IS_FULLRANDOM(mi))
		dp->garden = (Bool) (LRAND() & 1);
	else
		dp->garden = garden;

	dp->ndaisies = MI_BATCHCOUNT(mi);
	if (dp->ndaisies < -MINDAISIES)
		dp->ndaisies = NRAND(-dp->ndaisies - MINDAISIES + 1) + MINDAISIES;
	else if (dp->ndaisies < MINDAISIES)
		dp->ndaisies = MINDAISIES;
	dp->meadow_y = dp->height / 5;
	dp->step = (float) (dp->height - 2 * dp->meadow_y) / (dp->ndaisies + 1.0);
	MI_CLEARWINDOW(mi);
}

void
draw_daisy(ModeInfo * mi)
{
	daisystruct *dp = &daisies[MI_SCREEN(mi)];

	if (dp->time < dp->ndaisies)
		drawdaisy(mi);
	if (++dp->time > MI_CYCLES(mi))
		init_daisy(mi);
}

void
release_daisy(ModeInfo * mi)
{
	if (daisies != NULL) {
		(void) free((void *) daisies);
		daisies = NULL;
	}
}

void
refresh_daisy(ModeInfo * mi)
{
	daisystruct *dp = &daisies[MI_SCREEN(mi)];

	if (dp->time < dp->ndaisies) {
		MI_CLEARWINDOW(mi);
	} else {
		init_daisy(mi);
	}
}
