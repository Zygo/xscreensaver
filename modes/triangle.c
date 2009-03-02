/* -*- Mode: C; tab-width: 4 -*- */
/* triangle --- create a triangle-mountain */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)triangle.c	4.07 97/11/24 xlockmore";

#endif

/*-
 * Copyright (c) 1995 by Tobias Gloth
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
 * 22-Dec-97: Removed MI_PAUSE since it does not work on multiscreens.
 * 10-May-97: Compatible with xscreensaver
 * 10-Mar-96: re-arranged and re-formatted the code for appearance and
 *            to make common subroutines.  Simplified.
 *	          Ron Hitchens <ron@idiom.com>
 * 07-Mar-96: Removed internal delay code, set MI_PAUSE(mi) for inter-scene
 *            delays.  No other delays are needed here.
 *            Made pause time sensitive to value of cycles (in 10ths of a
 *            second).  Removed (hopefully) all references to globals.
 *	          Ron Hitchens <ron@idiom.com>
 * 27-Feb-96: Undid the changes listed below.  Added ModeInfo argument.
 *	          Implemented delay between scenes using the MI_PAUSE(mi)
 *            scheme.  Ron Hitchens <ron@idiom.com>
 * 27-Dec-95: Ron Hitchens <ron@idiom.com>
 *            Modified logic of draw_triangle() to provide a delay
 *            (sensitive to the value of cycles) between each iteration.
 *            Because this mode is so compute intensive, when the new
 *            event loop adjusted the delay to compensate, this mode had
 *            almost no delay time left.  This change pauses between each
 *            new landscape, but could still be done better (it is not
 *            sensitive to input events while drawing, for example).
 * 03-Nov-95: Many changes (hopefully some good ones) by David Bagley
 * 01-Oct-95: Written by Tobias Gloth
 */

#ifdef STANDALONE
#define PROGCLASS "Triangle"
#define HACK_INIT init_triangle
#define HACK_DRAW draw_triangle
#define triangle_opts xlockmore_opts
#define DEFAULTS "*delay: 10000 \n" \
 "*ncolors: 128 \n " \
 "*wireframe: False \n" \
 "*fullrandom: False \n"
#define SMOOTH_COLORS
			      /* #define UNIFORM_COLORS *//* To get blue water uncomment, but ... */
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

ModeSpecOpt triangle_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   triangle_description =
{"triangle", "init_triangle", "draw_triangle", "release_triangle",
 "refresh_triangle", "init_triangle", NULL, &triangle_opts,
 10000, 1, 1, 1, 64, 1.0, "",
 "Shows a triangle mountain range", 0, NULL};

#endif

#define MAX_STEPS 8
#define MAX_SIZE  (1<<MAX_STEPS)
#define MAX_LEVELS 1000

#define DELTA  0.4
#define LEFT   (-0.25)
#define RIGHT  1.25
#define TOP    0.3
#define BOTTOM 1.0
#define BLUE   (45 * MI_NPIXELS(mi) / 64)	/* Just the right shade of blue */

#define BACKFACE_REMOVAL

#define DISPLACE(h,d) ((h)/2+LRAND()/(MAXRAND/(2*(d)+1))-d)

typedef struct {
	int         width;
	int         height;
	int         size;
	int         steps;
	int         stage;
	int         busyLoop;
	int         fast;
	int         i;
	int         j;
	int         d;
	short       level[MAX_LEVELS];
	int         xpos[2 * MAX_SIZE + 1];
	int         ypos[MAX_SIZE + 1];
	short       H[(MAX_SIZE + 1) * (MAX_SIZE + 2) / 2];
	short      *h[MAX_SIZE + 1];
	short       delta[MAX_STEPS];
	Bool        wireframe;
	Bool        joke;
} trianglestruct;

static trianglestruct *triangles = NULL;

static
void
draw_atriangle(ModeInfo * mi, XPoint * p, int y_0, int y_1, int y_2, double dinv)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	trianglestruct *tp = &triangles[MI_SCREEN(mi)];

	if (MI_NPIXELS(mi) > 2) {	/* color */
		int         dmax, dmin;
		long        color;

		dmin = MIN(y_0, y_1);
		dmin = MIN(dmin, y_2);
		dmax = MAX(y_0, y_1);
		dmax = MAX(dmax, y_2);

		if (dmax == 0) {
			color = BLUE;
		} else {
			color = MI_NPIXELS(mi) -
				(int) ((double) MI_NPIXELS(mi) / M_PI_2 * atan(dinv * (dmax - dmin)));
		}

		XSetForeground(display, gc, MI_PIXEL(mi, color % MI_NPIXELS(mi)));
		if (tp->joke)
			if ((Bool) (LRAND() & 1)) {
				XDrawLines(display, window, gc, p, 4, CoordModeOrigin);
			} else {
				XFillPolygon(display, window, gc, p, 3, Convex, CoordModeOrigin);
		} else if (tp->wireframe) {
			XDrawLines(display, window, gc, p, 4, CoordModeOrigin);
		} else {
			XFillPolygon(display, window, gc, p, 3, Convex, CoordModeOrigin);
		}
	} else {
		/* mono */
#ifdef BACKFACE_REMOVAL
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		XFillPolygon(display, window, gc, p, 3, Convex, CoordModeOrigin);
#endif
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
		XDrawLines(display, window, gc, p, 4, CoordModeOrigin);
	}
}

static
void
calc_points1(trianglestruct * tp, int d, int *y0_p, int *y1_p, int *y2_p, XPoint * p)
{
	*y0_p = tp->level[MAX(tp->h[tp->i][tp->j], 0)];
	*y1_p = tp->level[MAX(tp->h[tp->i + d][tp->j], 0)];
	*y2_p = tp->level[MAX(tp->h[tp->i][tp->j + d], 0)];

	p[0].x = tp->xpos[2 * tp->i + tp->j];
	p[1].x = tp->xpos[2 * (tp->i + d) + tp->j];
	p[2].x = tp->xpos[2 * tp->i + (tp->j + d)];

	p[0].y = tp->ypos[tp->j] - *y0_p;
	p[1].y = tp->ypos[tp->j] - *y1_p;
	p[2].y = tp->ypos[tp->j + d] - *y2_p;

	p[3] = p[0];
}

static
void
calc_points2(trianglestruct * tp, int d, int *y0_p, int *y1_p, int *y2_p, XPoint * p)
{
	*y0_p = tp->level[MAX(tp->h[tp->i + d][tp->j], 0)];
	*y1_p = tp->level[MAX(tp->h[tp->i + d][tp->j + d], 0)];
	*y2_p = tp->level[MAX(tp->h[tp->i][tp->j + d], 0)];

	p[0].x = tp->xpos[2 * (tp->i + d) + tp->j];
	p[1].x = tp->xpos[2 * (tp->i + d) + (tp->j + d)];
	p[2].x = tp->xpos[2 * tp->i + (tp->j + d)];

	p[0].y = tp->ypos[tp->j] - *y0_p;
	p[1].y = tp->ypos[tp->j + d] - *y1_p;
	p[2].y = tp->ypos[tp->j + d] - *y2_p;

	p[3] = p[0];
}


static
void
draw_mesh(ModeInfo * mi, trianglestruct * tp, int d, int count)
{
	XPoint      p[4];
	int         first = 1;
	int         y_0, y_1, y_2;
	double      dinv = 0.2 / d;

	if ((tp->j == 0) && (tp->i == 0)) {
		MI_CLEARWINDOW(mi);
	}
	for (; (tp->j < tp->size) && (count > 0); tp->j += ((count) ? d : 0)) {
		for (tp->i = (first) ? tp->i : 0, first = 0;
		     (tp->i < MAX_SIZE - tp->j) && (count > 0);
		     tp->i += d, count--) {
			if (tp->i + tp->j < tp->size) {
				calc_points1(tp, d, &y_0, &y_1, &y_2, p);
				draw_atriangle(mi, p, y_0, y_1, y_2, dinv);
			}
			if (tp->i + tp->j + d < tp->size) {
				calc_points2(tp, d, &y_0, &y_1, &y_2, p);
				draw_atriangle(mi, p, y_0, y_1, y_2, dinv);
			}
		}
	}

	if (tp->j == tp->size) {
		tp->busyLoop = 1;
	}
}

void
init_triangle(ModeInfo * mi)
{
	trianglestruct *tp;
	short      *tmp;
	int         i, dim, one;

	if (triangles == NULL) {
		if ((triangles = (trianglestruct *) calloc(MI_NUM_SCREENS(mi),
					   sizeof (trianglestruct))) == NULL)
			return;
	}
	tp = &triangles[MI_SCREEN(mi)];

	tp->width = MI_WIDTH(mi);
	tp->height = MI_HEIGHT(mi);
	tp->busyLoop = -1;
	tp->fast = 2;
	if (MI_IS_FULLRANDOM(mi)) {
		tp->joke = (Bool) (NRAND(10) == 0);
		tp->wireframe = (Bool) (LRAND() & 1);
	} else
		tp->wireframe = MI_IS_WIREFRAME(mi);

	MI_CLEARWINDOW(mi);

	tp->steps = MAX_STEPS;
	do {
		tp->size = 1 << --tp->steps;
	} while (tp->size * 5 > tp->width);
	tmp = tp->H;
	for (i = 0; i < tp->size + 1; i++) {
		tp->h[i] = tmp;
		tmp += (tp->size) + 1 - i;
	}

	tp->stage = -1;
	dim = MIN(tp->width, tp->height);

	for (i = 0; i < 2 * tp->size + 1; i++) {
		tp->xpos[i] = (short) ((((double) i)
			 / ((double) (2 * tp->size)) * (RIGHT - LEFT) + LEFT)
				       * dim) + (tp->width - dim) / 2;
	}

	for (i = 0; i < (tp->size + 1); i++) {
		tp->ypos[i] = (short) ((((double) i)
			 / ((double) tp->size) * (BOTTOM - TOP) + TOP) * dim)
			+ (tp->height - dim) / 2;
	}

	for (i = 0; i < tp->steps; i++) {
		tp->delta[i] = ((short) (DELTA * dim)) >> i;
	}

	one = tp->delta[0];

	if (one > 0)
		for (i = 0; i < MAX_LEVELS; i++) {
			tp->level[i] = (i * i) / one;
		}
}

void
draw_triangle(ModeInfo * mi)
{
	trianglestruct *tp = &triangles[MI_SCREEN(mi)];
	int         d, d2, i, j, delta;

	if (tp->busyLoop > 0) {
		if (tp->busyLoop >= 100)
			tp->busyLoop = -1;
		else
			tp->busyLoop++;
		return;
	}
	if (!tp->busyLoop) {
		draw_mesh(mi, tp, tp->d / 2, MAX_SIZE / tp->d);
		return;
	}
	if (tp->delta[0] > 0) {
		if (!(++tp->stage)) {
			tp->h[0][0] = (short int) MAX(0, DISPLACE(0, tp->delta[0]));
			tp->h[tp->size][0] = (short int) MAX(0, DISPLACE(0, tp->delta[0]));
			tp->h[0][tp->size] = (short int) MAX(0, DISPLACE(0, tp->delta[0]));
		} else {
			d = 2 << (tp->steps - tp->stage);
			d2 = d / 2;
			delta = tp->delta[tp->stage - 1];

			for (i = 0; i < tp->size; i += d) {
				for (j = 0; j < (tp->size - i); j += d) {
					tp->h[i + d2][j] = (short int) DISPLACE(tp->h[i][j] +
						     tp->h[i + d][j], delta);
					tp->h[i][j + d2] = (short int) DISPLACE(tp->h[i][j] +
						     tp->h[i][j + d], delta);
					tp->h[i + d2][j + d2] = (short int) DISPLACE(tp->h[i + d][j] +
						     tp->h[i][j + d], delta);
				}

				tp->busyLoop = 0;
				tp->i = 0;
				tp->j = 0;
				tp->d = d;
			}
		}
	}
	if (tp->stage == tp->steps) {
#ifdef STANDALONE
    erase_full_window(MI_DISPLAY(mi), MI_WINDOW(mi));
#endif
		init_triangle(mi);
	}
}

void
release_triangle(ModeInfo * mi)
{
	if (triangles != NULL) {
		(void) free((void *) triangles);
		triangles = NULL;
	}
}

void
refresh_triangle(ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
}
