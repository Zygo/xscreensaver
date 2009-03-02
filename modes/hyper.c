/* -*- Mode: C; tab-width: 4 -*- */
/* hyper --- spinning tesseract */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)hyper.c	4.07 97/11/24 xlockmore";

#endif

/*-
 * This code derived from TI Explorer Lisp code by Joe Keane, Fritz Mueller,
 * and Jamie Zawinski.
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
 * Revision History:
 * 10-May-97: Compatible with xscreensaver
 * 02-Sep-93: xlock version (David Bagley <bagleyd@bigfoot.com>)
 * 1992:     xscreensaver version (Jamie Zawinski <jwz@netscape.com>)
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
#define PROGCLASS "Hyper"
#define HACK_INIT init_hyper
#define HACK_DRAW draw_hyper
#define hyper_opts xlockmore_opts
#define DEFAULTS "*delay: 20000 \n" \
 "*cycles: 5000 \n" \
 "*ncolors: 200 \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

ModeSpecOpt hyper_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   hyper_description =
{"hyper", "init_hyper", "draw_hyper", "release_hyper",
 "refresh_hyper", "init_hyper", NULL, &hyper_opts,
 20000, 1, 5000, 1, 64, 1.0, "",
 "Shows a spinning tesseract in 4D space", 0, NULL};

#endif

#define COMBOS 6
#define XY 0
#define XZ 1
#define YZ 2
#define XW 3
#define YW 4
#define ZW 5
#define DIMS 4
#define POINTS (DIMS*DIMS)
#define COLORS 8
#define NOTSIGN(i) (((i)==0)?(-1):(1))

static int  left_part[COMBOS] =
{0, 0, 1, 0, 1, 2};
static int  right_part[COMBOS] =
{1, 2, 2, 3, 3, 3};

typedef struct {
	int         old_x, old_y;
	int         new_x, new_y;
	int         same_p;
} state;

typedef struct {
	int         width;
	int         height;
	int         observer_z;
	int         x_offset, y_offset;
	int         unit_pixels;
	double      vars[DIMS][DIMS];
	state       points[POINTS];
	int         colors[COLORS];
	GC          xorGC;
	int         count;
	Bool        redrawing;
} hyperstruct;

static double cos_array[COMBOS], sin_array[COMBOS];
static hyperstruct *hypers = NULL;

static void
move_line(ModeInfo * mi, state state0, state state1, int color)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	hyperstruct *hp = &hypers[MI_SCREEN(mi)];

	if (MI_NPIXELS(mi) <= 2) {
		if (state0.same_p && state1.same_p)
			return;
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		XDrawLine(display, window, gc,
		     state0.old_x, state0.old_y, state1.old_x, state1.old_y);
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
		XDrawLine(display, window, gc,
		     state0.new_x, state0.new_y, state1.new_x, state1.new_y);
	} else {
		XSegment    segments[2];

		if (!hp->redrawing && state0.same_p && state1.same_p)
			return;
		XSetForeground(display, hp->xorGC, MI_PIXEL(mi, hp->colors[color]));
		segments[0].x1 = state0.old_x;
		segments[0].y1 = state0.old_y;
		segments[0].x2 = state1.old_x;
		segments[0].y2 = state1.old_y;
		segments[1].x1 = state0.new_x;
		segments[1].y1 = state0.new_y;
		segments[1].x2 = state1.new_x;
		segments[1].y2 = state1.new_y;
		if (hp->redrawing)
			XDrawSegments(display, window, hp->xorGC, &segments[1], 1);
		else
			XDrawSegments(display, window, hp->xorGC, segments, 2);
	}
}

void
init_hyper(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	hyperstruct *hp;
	XGCValues   gcv;
	int         i, j;
	double      combos[COMBOS];
	static int  first = 1;

	if (hypers == NULL) {
		if ((hypers = (hyperstruct *) calloc(MI_NUM_SCREENS(mi),
					      sizeof (hyperstruct))) == NULL)
			return;
	}
	hp = &hypers[MI_SCREEN(mi)];

	hp->redrawing = False;
	hp->width = MI_WIDTH(mi);
	hp->height = MI_HEIGHT(mi);
	hp->unit_pixels = hp->width < hp->height ? hp->width : hp->height;
	hp->x_offset = hp->width / 2;
	hp->y_offset = hp->height / 2;

	MI_CLEARWINDOW(mi);

	if (first) {
		combos[0] = 0.0;
		combos[1] = 0.01;
		combos[2] = 0.005;
		combos[3] = 0.01;
		combos[4] = 0.0;
		combos[5] = 0.0;
		for (i = 0; i < COMBOS; i++) {
			cos_array[i] = cos(combos[i]);
			sin_array[i] = sin(combos[i]);
		}
	}
	for (i = 0; i < DIMS; i++)
		for (j = 0; j < DIMS; j++)
			if (i == j)
				hp->vars[i][j] = 1.0;
			else
				hp->vars[i][j] = 0.0;
	(void) memset((char *) hp->points, 0, sizeof (hp->points));
	hp->observer_z = 5;

	if (!hp->xorGC) {
		gcv.function = GXxor;
		gcv.foreground = MI_WHITE_PIXEL(mi) ^ MI_BLACK_PIXEL(mi);
		hp->xorGC = XCreateGC(display, MI_WINDOW(mi), GCForeground | GCFunction,
				      &gcv);
	}
	if (MI_NPIXELS(mi) > 2)
		for (i = 0; i < COLORS; i++)
			hp->colors[i] = NRAND(MI_NPIXELS(mi));
	hp->count = 0;
}

void
draw_hyper(ModeInfo * mi)
{
	hyperstruct *hp = &hypers[MI_SCREEN(mi)];
	double      temp_mult, tmp0, tmp1;
	int         i, j;
	int         sign[DIMS];

	for (i = 0; i < POINTS; i++) {
		for (j = 0; j < DIMS; j++)
			sign[j] = NOTSIGN(i & (1 << (DIMS - 1 - j)));
		temp_mult = (hp->unit_pixels /
		  (((sign[0] * hp->vars[0][2]) + (sign[1] * hp->vars[1][2]) +
		    (sign[2] * hp->vars[2][2]) + (sign[3] * hp->vars[3][2]) +
		    (sign[0] * hp->vars[0][3]) + (sign[1] * hp->vars[1][3]) +
		    (sign[2] * hp->vars[2][3]) + (sign[3] * hp->vars[3][3])) -
		   hp->observer_z));
		hp->points[i].old_x = hp->points[i].new_x;
		hp->points[i].old_y = hp->points[i].new_y;
		hp->points[i].new_x = (int)
			((((sign[0] * hp->vars[0][0]) + (sign[1] * hp->vars[1][0]) +
		   (sign[2] * hp->vars[2][0]) + (sign[3] * hp->vars[3][0])) *
			  temp_mult) + hp->x_offset);
		hp->points[i].new_y = (int)
			((((sign[0] * hp->vars[0][1]) + (sign[1] * hp->vars[1][1]) +
		   (sign[2] * hp->vars[2][1]) + (sign[3] * hp->vars[3][1])) *
			  temp_mult) + hp->y_offset);
		hp->points[i].same_p =
			(hp->points[i].old_x == hp->points[i].new_x &&
			 hp->points[i].old_y == hp->points[i].new_y);
	}

	move_line(mi, hp->points[0], hp->points[1], 0);
	move_line(mi, hp->points[0], hp->points[2], 0);
	move_line(mi, hp->points[2], hp->points[3], 0);
	move_line(mi, hp->points[1], hp->points[3], 0);

	move_line(mi, hp->points[8], hp->points[9], 1);
	move_line(mi, hp->points[8], hp->points[10], 1);
	move_line(mi, hp->points[10], hp->points[11], 1);
	move_line(mi, hp->points[9], hp->points[11], 1);

	move_line(mi, hp->points[4], hp->points[5], 2);
	move_line(mi, hp->points[4], hp->points[6], 2);
	move_line(mi, hp->points[6], hp->points[7], 2);
	move_line(mi, hp->points[5], hp->points[7], 2);

	move_line(mi, hp->points[3], hp->points[7], 3);
	move_line(mi, hp->points[3], hp->points[11], 3);
	move_line(mi, hp->points[11], hp->points[15], 3);
	move_line(mi, hp->points[7], hp->points[15], 3);

	move_line(mi, hp->points[0], hp->points[4], 4);
	move_line(mi, hp->points[0], hp->points[8], 4);
	move_line(mi, hp->points[4], hp->points[12], 4);
	move_line(mi, hp->points[8], hp->points[12], 4);

	move_line(mi, hp->points[1], hp->points[5], 5);
	move_line(mi, hp->points[1], hp->points[9], 5);
	move_line(mi, hp->points[9], hp->points[13], 5);
	move_line(mi, hp->points[5], hp->points[13], 5);

	move_line(mi, hp->points[2], hp->points[6], 6);
	move_line(mi, hp->points[2], hp->points[10], 6);
	move_line(mi, hp->points[10], hp->points[14], 6);
	move_line(mi, hp->points[6], hp->points[14], 6);

	move_line(mi, hp->points[12], hp->points[13], 7);
	move_line(mi, hp->points[12], hp->points[14], 7);
	move_line(mi, hp->points[14], hp->points[15], 7);
	move_line(mi, hp->points[13], hp->points[15], 7);

	hp->redrawing = False;
	for (i = 0; i < COMBOS; i++)
		if (sin_array[i] != 0)
			for (j = 0; j < DIMS; j++) {
				tmp0 = ((hp->vars[j][left_part[i]] * cos_array[i]) +
				(hp->vars[j][right_part[i]] * sin_array[i]));
				tmp1 = ((hp->vars[j][right_part[i]] * cos_array[i]) -
				 (hp->vars[j][left_part[i]] * sin_array[i]));
				hp->vars[j][left_part[i]] = tmp0;
				hp->vars[j][right_part[i]] = tmp1;
			}
	if (++hp->count > MI_CYCLES(mi))
		init_hyper(mi);
}

void
release_hyper(ModeInfo * mi)
{
	if (hypers != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			hyperstruct *hp = &hypers[screen];

			if (hp->xorGC != NULL)
				XFreeGC(MI_DISPLAY(mi), hp->xorGC);
		}
		(void) free((void *) hypers);
		hypers = NULL;
	}
}

void
refresh_hyper(ModeInfo * mi)
{
	hyperstruct *hp = &hypers[MI_SCREEN(mi)];

	MI_CLEARWINDOW(mi);

	hp->redrawing = True;
}
