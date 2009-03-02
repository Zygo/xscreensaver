/* -*- Mode: C; tab-width: 4 -*- */
/* clock --- displays an analog clock */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)clock.c	4.07 97/11/24 xlockmore";

#endif

/*-
 * Copyright (c) 1995 by Jeremie PETIT <petit@aurora.unice.fr>
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
 * The design was highly inspirated from 'oclock' but not the code,
 * which is all mine.
 * 
 * Revision History:
 * 10-May-97: Compatible with xscreensaver
 * 24-Dec-95: Ron Hitchens <ron@idiom.com> cycles range and defaults
 *            changed.
 * 01-Dec-95: Clock size changes each time it is displayed.
 * 01-Jun-95: Written.
 */

#ifdef STANDALONE
#define PROGCLASS "Clock"
#define HACK_INIT init_clock
#define HACK_DRAW draw_clock
#define clock_opts xlockmore_opts
#define DEFAULTS "*delay: 100000 \n" \
 "*count: -16 \n" \
 "*cycles: 200 \n" \
 "*size: -200 \n" \
 "*ncolors: 200 \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */

#endif /* STANDALONE */

ModeSpecOpt clock_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   clock_description =
{"clock", "init_clock", "draw_clock", "release_clock",
 "refresh_clock", "init_clock", NULL, &clock_opts,
 100000, -16, 200, -200, 64, 1.0, "",
 "Shows Packard's clock", 0, NULL};

#endif

#ifdef TM_IN_SYS_TIME
#include <sys/time.h>
#else
#include <time.h>
#endif

#define MIN_CYCLES     50	/* shortest possible time before moving the clock */
#define MAX_CYCLES   1000	/* longest possible time before moving the clock */
#define CLOCK_BORDER    9	/* Percentage of the clock size for the border */
#define HOURS_SIZE     50	/* Percentage of the clock size for hours length */
#define MINUTES_SIZE   75	/* Idem for minutes */
#define SECONDS_SIZE   80	/* Idem for seconds */
#define HOURS_WIDTH    10	/* Percentage of the clock size for hours width */
#define MINUTES_WIDTH   5	/* Idem for minutes */
#define SECONDS_WIDTH   2	/* Idem for seconds */
#define JEWEL_POSITION 90	/* The relative position of the jewel */
#define JEWEL_SIZE      9	/* The relative size of the jewel */
#define MINCLOCKS       1
#define MINSIZE        16	/* size, minimum */

typedef struct {
	int         size;	/* length of the hand */
	int         width;	/* width of the hand */
	unsigned long color;	/* color of the hand */
} hand;

typedef struct {
	hand        hours;	/* the hours hand */
	hand        minutes;	/* the minutes hand */
	hand        seconds;	/* the seconds hand */
	XPoint      pos;
	int         size;
	int         radius;
	int         boxsize;
	int         jewel_size;
	int         jewel_position;
	int         border_width;
	unsigned long border_color;
	unsigned long jewel_color;
	struct tm   dd_time;
} oclock;

typedef struct {
	oclock     *oclocks;
	time_t      d_time;
	int         clock_count;
	int         width, height;
	int         nclocks;
	int         currentclocks;
	int         redrawing;
} clockstruct;

static clockstruct *clocks = NULL;

static int
myrand(int minimum, int maximum)
{
	return ((int) (minimum + (LRAND() / MAXRAND) * (maximum + 1 - minimum)));
}

static double
HourToAngle(oclock * oclockp)
{
	return ((M_PI * ((double) oclockp->dd_time.tm_hour +
			 ((double) oclockp->dd_time.tm_min / 60) +
		   ((double) oclockp->dd_time.tm_sec / 3600))) / 6 - M_PI_2);
}

static double
MinutesToAngle(oclock * oclockp)
{
	return ((M_PI * ((double) oclockp->dd_time.tm_min +
		    ((double) oclockp->dd_time.tm_sec / 60))) / 30 - M_PI_2);
}

static double
SecondsToAngle(oclock * oclockp)
{
	return ((M_PI * ((double) oclockp->dd_time.tm_sec) / 30) - M_PI_2);
}

static void
DrawDisk(Display * display, Window window, GC gc,
	 int center_x, int center_y, int diameter)
{
	int         radius = diameter / 2;

	if (diameter > 1)
		XFillArc(display, window, gc,
			 center_x - radius, center_y - radius,
			 diameter, diameter,
			 0, 23040);
	else
		XDrawPoint(display, window, gc, center_x, center_y);
}

static void
DrawBorder(ModeInfo * mi, int clck, short int mode)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	clockstruct *cp = &clocks[MI_SCREEN(mi)];


	if (mode == 0)
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
	else
		XSetForeground(display, gc, cp->oclocks[clck].border_color);

	XSetLineAttributes(display, gc, cp->oclocks[clck].border_width,
			   LineSolid, CapNotLast, JoinMiter);
	XDrawArc(display, MI_WINDOW(mi), gc,
		 cp->oclocks[clck].pos.x - cp->oclocks[clck].size / 2,
		 cp->oclocks[clck].pos.y - cp->oclocks[clck].size / 2,
		 cp->oclocks[clck].size, cp->oclocks[clck].size,
		 0, 23040);
}

static void
DrawJewel(ModeInfo * mi, int clck, short int mode)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	clockstruct *cp = &clocks[MI_SCREEN(mi)];

	XSetLineAttributes(display, gc, 1, LineSolid, CapNotLast, JoinMiter);
	if (mode == 0)
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
	else
		XSetForeground(display, gc, cp->oclocks[clck].jewel_color);
	DrawDisk(display, MI_WINDOW(mi), gc,
		 cp->oclocks[clck].pos.x,
		 cp->oclocks[clck].pos.y - cp->oclocks[clck].jewel_position,
		 cp->oclocks[clck].jewel_size);
}

static void
DrawHand(ModeInfo * mi, int clck, hand * h, double angle, short int mode)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	clockstruct *cp = &clocks[MI_SCREEN(mi)];
	int         cosi, sinj;

	/* mode: 0 for erasing, anything else for drawing */
	if (mode == 0)
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
	else
		XSetForeground(display, gc, h->color);
	XSetLineAttributes(display, gc, h->width, LineSolid, CapNotLast, JoinMiter);
	DrawDisk(display, window, gc,
		 cp->oclocks[clck].pos.x, cp->oclocks[clck].pos.y, h->width);
	cosi = (int) (cos(angle) * (double) (h->size));
	sinj = (int) (sin(angle) * (double) (h->size));
	XDrawLine(display, window, gc,
		  cp->oclocks[clck].pos.x, cp->oclocks[clck].pos.y,
	     cp->oclocks[clck].pos.x + cosi, cp->oclocks[clck].pos.y + sinj);
	DrawDisk(display, window, gc,
	      cp->oclocks[clck].pos.x + cosi, cp->oclocks[clck].pos.y + sinj,
		 h->width);
}

static void
real_draw_clock(ModeInfo * mi, int clck, short int mode)
{
	clockstruct *cp = &clocks[MI_SCREEN(mi)];

	DrawBorder(mi, clck, mode);
	DrawJewel(mi, clck, mode);
	DrawHand(mi, clck, &cp->oclocks[clck].hours,
		 HourToAngle(&cp->oclocks[clck]), mode);
	DrawHand(mi, clck, &cp->oclocks[clck].minutes,
		 MinutesToAngle(&cp->oclocks[clck]), mode);
	DrawHand(mi, clck, &cp->oclocks[clck].seconds,
		 SecondsToAngle(&cp->oclocks[clck]), mode);
}

static int
collide(clockstruct * cp, int clck)
{
	int         i, d, x, y;

	for (i = 0; i < clck; i++) {
		x = (cp->oclocks[i].pos.x - cp->oclocks[clck].pos.x);
		y = (cp->oclocks[i].pos.y - cp->oclocks[clck].pos.y);
		d = (int) sqrt((double) (x * x + y * y));
		if (d < (cp->oclocks[i].size + cp->oclocks[i].border_width +
		cp->oclocks[clck].size + cp->oclocks[clck].border_width) / 2)
			return i;
	}
	return clck;
}

static int
new_clock_state(ModeInfo * mi, int clck)
{
	clockstruct *cp = &clocks[MI_SCREEN(mi)];
	int         size = MI_SIZE(mi);
	int         tryit;

	for (tryit = 0; tryit < 8; tryit++) {
		/* We change the clock size */
		if (size < -MINSIZE)
			cp->oclocks[clck].size = NRAND(MIN(-size, MAX(MINSIZE,
			MIN(cp->width, cp->height))) - MINSIZE + 1) + MINSIZE;
		else if (size < MINSIZE) {
			if (!size)
				cp->oclocks[clck].size = MAX(MINSIZE, MIN(cp->width, cp->height));
			else
				cp->oclocks[clck].size = MINSIZE;
		} else
			cp->oclocks[clck].size = MIN(size, MAX(MINSIZE,
						MIN(cp->width, cp->height)));

		/* We must compute new attributes for the clock because its size changes */
		cp->oclocks[clck].hours.size = (cp->oclocks[clck].size * HOURS_SIZE) / 200;
		cp->oclocks[clck].minutes.size =
			(cp->oclocks[clck].size * MINUTES_SIZE) / 200;
		cp->oclocks[clck].seconds.size =
			(cp->oclocks[clck].size * SECONDS_SIZE) / 200;
		cp->oclocks[clck].jewel_size = (cp->oclocks[clck].size * JEWEL_SIZE) / 200;

		cp->oclocks[clck].border_width =
			(cp->oclocks[clck].size * CLOCK_BORDER) / 200;
		cp->oclocks[clck].hours.width = (cp->oclocks[clck].size * HOURS_WIDTH) / 200;
		cp->oclocks[clck].minutes.width =
			(cp->oclocks[clck].size * MINUTES_WIDTH) / 200;
		cp->oclocks[clck].seconds.width =
			(cp->oclocks[clck].size * SECONDS_WIDTH) / 200;

		cp->oclocks[clck].jewel_position =
			(cp->oclocks[clck].size * JEWEL_POSITION) / 200;

		cp->oclocks[clck].radius =
			(cp->oclocks[clck].size / 2) + cp->oclocks[clck].border_width + 1;

		/* Here we set new values for the next clock to be displayed */
		if (MI_NPIXELS(mi) > 2) {
			/* New colors */
			cp->oclocks[clck].border_color = MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));
			cp->oclocks[clck].jewel_color = MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));
			cp->oclocks[clck].hours.color = MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));
			cp->oclocks[clck].minutes.color = MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));
			cp->oclocks[clck].seconds.color = MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));
		}
		/* A new position for the clock */
		cp->oclocks[clck].pos.x = myrand(cp->oclocks[clck].radius,
				       cp->width - cp->oclocks[clck].radius);
		cp->oclocks[clck].pos.y = myrand(cp->oclocks[clck].radius,
				      cp->height - cp->oclocks[clck].radius);
		if (clck == collide(cp, clck))
			return True;
	}
	cp->currentclocks = clck;
	return False;
}

static void
update_clock(ModeInfo * mi, int clck)
{
	clockstruct *cp = &clocks[MI_SCREEN(mi)];

	DrawHand(mi, clck, &cp->oclocks[clck].hours,
		 HourToAngle(&cp->oclocks[clck]), 0);
	DrawHand(mi, clck, &cp->oclocks[clck].minutes,
		 MinutesToAngle(&cp->oclocks[clck]), 0);
	DrawHand(mi, clck, &cp->oclocks[clck].seconds,
		 SecondsToAngle(&cp->oclocks[clck]), 0);
	cp->d_time = time((time_t *) 0);
	(void) memcpy((char *) &cp->oclocks[clck].dd_time,
		      (char *) localtime(&cp->d_time),
		      sizeof (cp->oclocks[clck].dd_time));
	DrawHand(mi, clck, &cp->oclocks[clck].hours,
		 HourToAngle(&cp->oclocks[clck]), 1);
	DrawHand(mi, clck, &cp->oclocks[clck].minutes,
		 MinutesToAngle(&cp->oclocks[clck]), 1);
	DrawHand(mi, clck, &cp->oclocks[clck].seconds,
		 SecondsToAngle(&cp->oclocks[clck]), 1);
}

void
init_clock(ModeInfo * mi)
{
	clockstruct *cp;
	int         clck;

	if (clocks == NULL) {
		if ((clocks = (clockstruct *) calloc(MI_NUM_SCREENS(mi),
					      sizeof (clockstruct))) == NULL)
			return;
	}
	cp = &clocks[MI_SCREEN(mi)];


	cp->redrawing = 0;
	cp->width = MI_WIDTH(mi);
	cp->height = MI_HEIGHT(mi);

	MI_CLEARWINDOW(mi);

	cp->nclocks = MI_COUNT(mi);
	if (cp->nclocks < -MINCLOCKS) {
		/* if cp->nclocks is random ... the size can change */
		if (cp->oclocks != NULL) {
			(void) free((void *) cp->oclocks);
			cp->oclocks = NULL;
		}
		cp->nclocks = NRAND(-cp->nclocks - MINCLOCKS + 1) + MINCLOCKS;
	} else if (cp->nclocks < MINCLOCKS)
		cp->nclocks = MINCLOCKS;
	if (cp->oclocks == NULL)
		cp->oclocks = (oclock *) malloc(cp->nclocks * sizeof (oclock));

	cp->clock_count = MI_CYCLES(mi);
	if (cp->clock_count < MIN_CYCLES)
		cp->clock_count = MIN_CYCLES;
	if (cp->clock_count > MAX_CYCLES)
		cp->clock_count = MAX_CYCLES;

	for (clck = 0; clck < cp->nclocks; clck++) {
		/* By default, we set the clock's colors to white */
		cp->oclocks[clck].border_color = MI_WHITE_PIXEL(mi);
		cp->oclocks[clck].jewel_color = MI_WHITE_PIXEL(mi);
		cp->oclocks[clck].hours.color = MI_WHITE_PIXEL(mi);
		cp->oclocks[clck].minutes.color = MI_WHITE_PIXEL(mi);
		cp->oclocks[clck].seconds.color = MI_WHITE_PIXEL(mi);
		cp->d_time = time((time_t *) 0);
	}
	for (clck = 0; clck < cp->nclocks; clck++) {
		(void) memcpy((char *) &cp->oclocks[clck].dd_time,
			      (char *) localtime(&cp->d_time),
			      sizeof (cp->oclocks[clck].dd_time));
	}
	cp->currentclocks = cp->nclocks;
	for (clck = 0; clck < cp->currentclocks; clck++)
		if (!new_clock_state(mi, clck))
			break;
}

void
draw_clock(ModeInfo * mi)
{
	clockstruct *cp = &clocks[MI_SCREEN(mi)];
	int         clck;

	if (++cp->clock_count >= MI_CYCLES(mi)) {
		cp->clock_count = 0;
		for (clck = 0; clck < cp->currentclocks; clck++)
			real_draw_clock(mi, clck, 0);
		cp->currentclocks = cp->nclocks;
		for (clck = 0; clck < cp->currentclocks; clck++)
			if (!new_clock_state(mi, clck))
				break;
		for (clck = 0; clck < cp->currentclocks; clck++)
			real_draw_clock(mi, clck, 1);
		cp->redrawing = 0;
	} else if (cp->d_time != time((time_t *) 0))
		for (clck = 0; clck < cp->currentclocks; clck++)
			update_clock(mi, clck);
	if (cp->redrawing) {
		for (clck = 0; clck < cp->currentclocks; clck++)
			real_draw_clock(mi, clck, 1);
		cp->redrawing = 0;
	}
}

void
release_clock(ModeInfo * mi)
{
	if (clocks != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			clockstruct *cp = &clocks[screen];

			if (cp->oclocks != NULL)
				(void) free((void *) cp->oclocks);
		}
		(void) free((void *) clocks);
		clocks = NULL;
	}
}

void
refresh_clock(ModeInfo * mi)
{
	clockstruct *cp = &clocks[MI_SCREEN(mi)];

	MI_CLEARWINDOW(mi);
	cp->redrawing = 1;
}
