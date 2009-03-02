/* -*- Mode: C; tab-width: 4 -*- */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)turtle.c	4.03 97/05/10 xlockmore";

#endif

/*-
 * turtle.c - fractal curves xlock, the X Window System lockscreen.
 *
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
 * 01-Dec-96: Not too proud how I hacked in 2 more curves
 * 30-Sep-96: started with Hilbert curve, David Bagley
 * From Fractal Programming in C by Roger T. Stevens
 */

#ifdef STANDALONE
#define PROGCLASS            "Turtle"
#define HACK_INIT            init_turtle
#define HACK_DRAW            draw_turtle
#define DEF_DELAY            1000000
#define DEF_CYCLES           20
#define DEF_NCOLORS          64
#include "xlockmore.h"    /* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"    /* in xlockmore distribution */
ModeSpecOpt turtle_opts =
{0, NULL, 0, NULL, NULL};
 
#endif /* STANDALONE */

#define HILBERT 0		/* Not exactly a turtle algorthm... p199 */
#define CESARO_VAR 1		/* Telephone curve p168 */
#define HARTER_HEIGHTWAY 2	/* Dragon curve p292 */
#define TURTLE_CURVES 3

#define POINT(x_1,y_1,x_2,y_2) (((x_2)==(x_1))?(((y_2)>(y_1))?90.0:270.0):\
  ((x_2)>(x_1))?(atan(((y_2)-(y_1))/((x_2)-(x_1)))*(180.0/M_PI)):\
  (atan(((y_2)-(y_1))/((x_2)-(x_1)))*(180.0/M_PI)+180.0))
#define TURN(theta, angle) ((theta)+=(angle))
#define STEP(x, y, r, theta) ((x)+=(r)*cos((theta)*M_PI/180.0)),\
  ((y)+=(r)*sin((theta)*M_PI/180.0))

typedef struct {
	double      r, theta, x, y;
} turtletype;

typedef struct {
	double      x, y;
} complextype;

typedef struct {
	int         width, height;
	int         time;
	int         level;
	int         curve;
	int         r, maxlevel, min, dir;
	XPoint      pt1, pt2;
	XPoint      start;
	turtletype  turtle;
	int         sign;
} turtlestruct;

static turtlestruct *turtles = NULL;

static void
generate_hilbert(ModeInfo * mi, int r1, int r2)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	turtlestruct *tp = &turtles[MI_SCREEN(mi)];

	tp->level--;

	if (tp->level > 0)
		generate_hilbert(mi, r2, r1);

	tp->pt2.x += r1;
	tp->pt2.y += r2;
	XDrawLine(display, window, gc,
		  tp->pt1.x, tp->pt1.y, tp->pt2.x, tp->pt2.y);
	tp->pt1.x = tp->pt2.x;
	tp->pt1.y = tp->pt2.y;

	if (tp->level > 0)
		generate_hilbert(mi, r1, r2);

	tp->pt2.x += r2;
	tp->pt2.y += r1;
	XDrawLine(display, window, gc,
		  tp->pt1.x, tp->pt1.y, tp->pt2.x, tp->pt2.y);
	tp->pt1.x = tp->pt2.x;
	tp->pt1.y = tp->pt2.y;

	if (tp->level > 0)
		generate_hilbert(mi, r1, r2);

	tp->pt2.x -= r1;
	tp->pt2.y -= r2;
	XDrawLine(display, window, gc,
		  tp->pt1.x, tp->pt1.y, tp->pt2.x, tp->pt2.y);
	tp->pt1.x = tp->pt2.x;
	tp->pt1.y = tp->pt2.y;

	if (tp->level > 0)
		generate_hilbert(mi, -r2, -r1);

	tp->level++;
}

static void
generate_cesarovar(ModeInfo * mi, double pt1x, double pt1y,
		   double pt2x, double pt2y, int level, int sign)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	turtlestruct *tp = &turtles[MI_SCREEN(mi)];
	complextype points[4];

	level--;

	tp->turtle.r = sqrt((double) ((pt2x - pt1x) * (pt2x - pt1x) +
				      (pt2y - pt1y) * (pt2y - pt1y))) / 2.0;
	points[0].x = pt1x;
	points[0].y = pt1y;
	points[2].x = pt2x;
	points[2].y = pt2y;
	tp->turtle.theta = POINT(pt1x, pt1y, pt2x, pt2y);
	tp->turtle.x = pt1x;
	tp->turtle.y = pt1y;
	STEP(tp->turtle.x, tp->turtle.y, tp->turtle.r, tp->turtle.theta);
	points[3].x = tp->turtle.x;
	points[3].y = tp->turtle.y;
	TURN(tp->turtle.theta, 90.0 * (double) sign);
	STEP(tp->turtle.x, tp->turtle.y, tp->turtle.r, tp->turtle.theta);
	points[1].x = tp->turtle.x;
	points[1].y = tp->turtle.y;
	sign = -1;
	if (level > 0) {
		int         j;

		for (j = 0; j < 2; j++) {
			pt1x = points[j].x;
			pt2x = points[j + 1].x;
			pt1y = points[j].y;
			pt2y = points[j + 1].y;
			generate_cesarovar(mi, pt1x, pt1y, pt2x, pt2y, level, sign);
		}
	} else {
		XDrawLine(display, window, gc,
			  (int) points[0].x + tp->start.x, (int) points[0].y + tp->start.y,
			  (int) points[2].x + tp->start.x, (int) points[2].y + tp->start.y);
		XDrawLine(display, window, gc,
			  (int) points[1].x + tp->start.x, (int) points[1].y + tp->start.y,
			  (int) points[3].x + tp->start.x, (int) points[3].y + tp->start.y);
	}
}

static void
generate_harter_heightway(ModeInfo * mi, double pt1x, double pt1y,
			  double pt2x, double pt2y, int level, int sign)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	turtlestruct *tp = &turtles[MI_SCREEN(mi)];
	complextype points[4];
	int         sign2 = -1;

	level--;

	tp->turtle.r = sqrt((double) ((pt2x - pt1x) * (pt2x - pt1x) +
				      (pt2y - pt1y) * (pt2y - pt1y)) / 2.0);
	points[0].x = pt1x;
	points[0].y = pt1y;
	points[2].x = pt2x;
	points[2].y = pt2y;
	tp->turtle.theta = POINT(pt1x, pt1y, pt2x, pt2y);
	tp->turtle.x = pt1x;
	tp->turtle.y = pt1y;
	TURN(tp->turtle.theta, 45.0 * (double) sign);
	STEP(tp->turtle.x, tp->turtle.y, tp->turtle.r, tp->turtle.theta);
	points[1].x = tp->turtle.x;
	points[1].y = tp->turtle.y;
	if (level > 0) {
		int         j;

		for (j = 0; j < 2; j++) {
			pt1x = points[j].x;
			pt2x = points[j + 1].x;
			pt1y = points[j].y;
			pt2y = points[j + 1].y;
			generate_harter_heightway(mi, pt1x, pt1y, pt2x, pt2y, level, sign2);
			sign2 *= -1;
		}
	} else {
		XDrawLine(display, window, gc,
			  (int) points[0].x + tp->start.x, (int) points[0].y + tp->start.y,
			  (int) points[1].x + tp->start.x, (int) points[1].y + tp->start.y);
		XDrawLine(display, window, gc,
			  (int) points[1].x + tp->start.x, (int) points[1].y + tp->start.y,
			  (int) points[2].x + tp->start.x, (int) points[2].y + tp->start.y);
	}
}


void
init_turtle(ModeInfo * mi)
{
	turtlestruct *tp;
	int         i;

	if (turtles == NULL) {
		if ((turtles = (turtlestruct *) calloc(MI_NUM_SCREENS(mi),
					     sizeof (turtlestruct))) == NULL)
			return;
	}
	tp = &turtles[MI_SCREEN(mi)];

	tp->width = MI_WIN_WIDTH(mi);
	tp->height = MI_WIN_HEIGHT(mi);
	tp->min = MIN(tp->width, tp->height);
	tp->maxlevel = 0;
	i = tp->min;
	do {
		tp->r = i;
		tp->maxlevel++;
		i = (tp->min - 1) / (1 << tp->maxlevel);
	} while (i > 1);
	tp->maxlevel--;
	tp->r = tp->min = (tp->r << tp->maxlevel);

	tp->curve = NRAND(TURTLE_CURVES);
	switch (tp->curve) {
		case HILBERT:
			tp->start.x = NRAND(tp->width - tp->min);
			tp->start.y = NRAND(tp->height - tp->min);
			break;
		case CESARO_VAR:
			tp->r <<= 6;
			tp->min = 3 * tp->min / 4;
			tp->start.x = tp->width / 2;
			tp->start.y = tp->height / 2;
			switch (NRAND(4)) {
				case 0:
					tp->pt1.x = -tp->min / 2;
					tp->pt1.y = 0;
					tp->pt2.x = tp->min / 2;
					tp->pt2.y = 0;
					tp->start.y -= tp->min / 6;
					break;
				case 1:
					tp->pt1.x = 0;
					tp->pt1.y = -tp->min / 2;
					tp->pt2.x = 0;
					tp->pt2.y = tp->min / 2;
					tp->start.x += tp->min / 6;
					break;
				case 2:
					tp->pt1.x = tp->min / 2;
					tp->pt1.y = 0;
					tp->pt2.x = -tp->min / 2;
					tp->pt2.y = 0;
					tp->start.y += tp->min / 6;
					break;
				case 3:
					tp->pt1.x = 0;
					tp->pt1.y = tp->min / 2;
					tp->pt2.x = 0;
					tp->pt2.y = -tp->min / 2;
					tp->start.x -= tp->min / 6;
					break;
			}
			break;
		case HARTER_HEIGHTWAY:
			tp->r <<= 6;
			tp->min = 3 * tp->min / 4;
			tp->start.x = tp->width / 2;
			tp->start.y = tp->height / 2;
			switch (NRAND(4)) {
				case 0:
					tp->pt1.x = -tp->min / 2;
					tp->pt1.y = -tp->min / 12;
					tp->pt2.x = tp->min / 2;
					tp->pt2.y = -tp->min / 12;
					break;
				case 1:
					tp->pt1.x = tp->min / 12;
					tp->pt1.y = -tp->min / 2;
					tp->pt2.x = tp->min / 12;
					tp->pt2.y = tp->min / 2;
					break;
				case 2:
					tp->pt1.x = tp->min / 2;
					tp->pt1.y = tp->min / 12;
					tp->pt2.x = -tp->min / 2;
					tp->pt2.y = tp->min / 12;
					break;
				case 3:
					tp->pt1.x = -tp->min / 12;
					tp->pt1.y = tp->min / 2;
					tp->pt2.x = -tp->min / 12;
					tp->pt2.y = -tp->min / 2;
					break;
			}
	}
	tp->level = 0;
	tp->sign = 1;
	tp->dir = NRAND(4);
	if (MI_NPIXELS(mi) <= 2)
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_WIN_WHITE_PIXEL(mi));
	tp->time = 0;
	XClearWindow(MI_DISPLAY(mi), MI_WINDOW(mi));
}

void
draw_turtle(ModeInfo * mi)
{
	turtlestruct *tp = &turtles[MI_SCREEN(mi)];

	if (++tp->time > MI_CYCLES(mi))
		init_turtle(mi);

	if (MI_NPIXELS(mi) > 2)
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi),
			       MI_PIXEL(mi, NRAND(MI_NPIXELS(mi))));

	tp->r = tp->r >> 1;
	tp->level++;
	if (tp->r > 1)
		switch (tp->curve) {
			case HILBERT:
				switch (tp->dir) {
					case 0:
						tp->pt1.x = tp->pt2.x = tp->start.x + tp->r / 2;
						tp->pt1.y = tp->pt2.y = tp->start.y + tp->r / 2;
						generate_hilbert(mi, 0, tp->r);
						break;
					case 1:
						tp->pt1.x = tp->pt2.x = tp->start.x + tp->min - tp->r / 2;
						tp->pt1.y = tp->pt2.y = tp->start.y + tp->min - tp->r / 2;
						generate_hilbert(mi, 0, -tp->r);
						break;
					case 2:
						tp->pt1.x = tp->pt2.x = tp->start.x + tp->min - tp->r / 2;
						tp->pt1.y = tp->pt2.y = tp->start.y + tp->min - tp->r / 2;
						generate_hilbert(mi, -tp->r, 0);
						break;
					case 3:
						tp->pt1.x = tp->pt2.x = tp->start.x + tp->r / 2;
						tp->pt1.y = tp->pt2.y = tp->start.y + tp->r / 2;
						generate_hilbert(mi, tp->r, 0);
				}
				break;
			case CESARO_VAR:
				generate_cesarovar(mi, tp->pt1.x, tp->pt1.y, tp->pt2.x, tp->pt2.y,
						   tp->level, tp->sign);
				break;
			case HARTER_HEIGHTWAY:
				generate_harter_heightway(mi, tp->pt1.x, tp->pt1.y,
				  tp->pt2.x, tp->pt2.y, tp->level, tp->sign);
				break;
		}
}

void
release_turtle(ModeInfo * mi)
{
	if (turtles != NULL) {
		(void) free((void *) turtles);
		turtles = NULL;
	}
}
