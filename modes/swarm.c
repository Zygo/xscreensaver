/* -*- Mode: C; tab-width: 4 -*- */
/* swarm --- swarm of bees */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)swarm.c	5.00 2000/11/01 xlockmore";

#endif

/*-
 * Copyright (c) 1991 by Patrick J. Naughton.
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
 * 13-Jul-2000: Bee trails implemented by Juan Heguiabehere <jheguia@usa.net>
 * 10-May-1997: Compatible with xscreensaver
 * 31-Aug-1990: Adapted from xswarm by Jeff Butterworth <butterwo@ncsc.org>
 */

#ifdef STANDALONE
#define PROGCLASS "Swarm"
#define HACK_INIT init_swarm
#define HACK_DRAW draw_swarm
#define swarm_opts xlockmore_opts
#define DEFAULTS "*delay: 15000 \n" \
 "*count: -100 \n" \
 "*size: -100 \n" \
 "*trackmouse: False \n"
#define BRIGHT_COLORS
#define SMOOTH_COLORS
#include "xlockmore.h"		/* from the xscreensaver distribution */
#include <X11/Xutil.h>
#else /* !STANDALONE */
#include "xlock.h"		/* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef MODE_swarm

#define DEF_TRACKMOUSE  "False"

static Bool trackmouse;

static XrmOptionDescRec opts[] =
{
        {(char *) "-trackmouse", (char *) ".swarm.trackmouse", XrmoptionNoArg, (caddr_t) "on"},
        {(char *) "+trackmouse", (char *) ".swarm.trackmouse", XrmoptionNoArg, (caddr_t) "off"}
};

static argtype vars[] =
{
        {(caddr_t *) & trackmouse, (char *) "trackmouse", (char *) "TrackMouse", (char *) DEF_TRACKMOUSE, t_Bool}
};

static OptionStruct desc[] =
{
        {(char *) "-/+trackmouse", (char *) "turn on/off the tracking of the mouse"}
};

ModeSpecOpt swarm_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};


#ifdef USE_MODULES
ModStruct   swarm_description =
{"swarm", "init_swarm", "draw_swarm", "release_swarm",
 "refresh_swarm", "init_swarm", NULL, &swarm_opts,
 15000, -100, 1, -100, 64, 1.0, "",
 "Shows a swarm of bees following a wasp", 0, NULL};

#endif

#define MINBEES	1		/* min number of bees */
#define MINTRAIL 3		/* min number of time positions recorded */
#define BEEACC	2		/* acceleration of bees */
#define WASPACC 5		/* maximum acceleration of wasp */
#define BEEVEL	17		/* maximum bee velocity */
#define WASPVEL 15		/* maximum wasp velocity */

/* Macros */
#define X(t,b)	(sp->x[((t)*sp->beecount+(b))])
#define Y(t,b)	(sp->y[((t)*sp->beecount+(b))])
#define balance_rand(v)	((NRAND(v))-((v-1)/2))	/* random number around 0, input odd */

typedef struct {
	int         pix;
	int         width;
	int         height;
	int         border;	/* wasp won't go closer than this to the edge */
	int         beecount;	/* number of bees */
	XSegment   *segs;	/* bee lines */
	XSegment   *old_segs;	/* old bee lines */
	float      *x, *y;	/* bee positions x[time][bee#] */
	float      *xv, *yv;	/* bee velocities xv[bee#] */
	short       wx[3];
	short       wy[3];
	short       wxv;
	short       wyv;
	short       tick;
	short       rolloverflag;
	short       taillen;
	Cursor      cursor;
} swarmstruct;

static swarmstruct *swarms = NULL;

static void
free_segs(swarmstruct *sp)
{
	if (sp->segs != NULL) {
		(void) free((void *) sp->segs);
		sp->segs = NULL;
	}
	if (sp->old_segs != NULL) {
		(void) free((void *) sp->old_segs);
		sp->old_segs = NULL;
	}
}

static void
free_bees(swarmstruct *sp)
{
	free_segs(sp);
	if (sp->x != NULL) {
		(void) free((void *) sp->x);
		sp->x = NULL;
	}
	if (sp->y != NULL) {
		(void) free((void *) sp->y);
		sp->y = NULL;
	}
	if (sp->xv != NULL) {
		(void) free((void *) sp->xv);
		sp->xv = NULL;
	}
	if (sp->yv != NULL) {
		(void) free((void *) sp->yv);
		sp->yv = NULL;
	}
}

static void
free_swarm(Display *display, swarmstruct *sp)
{
	free_bees(sp);
	if (sp->cursor != None) {
		XFreeCursor(display, sp->cursor);
		sp->cursor = None;
	}
}

void
init_swarm(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         b, t;
	swarmstruct *sp;

	if (swarms == NULL) {
		if ((swarms = (swarmstruct *) calloc(MI_NUM_SCREENS(mi),
				sizeof (swarmstruct))) == NULL)
			return;
	}
	sp = &swarms[MI_SCREEN(mi)];

	sp->beecount = MI_COUNT(mi);
	if (sp->beecount < -MINBEES) {
		sp->beecount = NRAND(-sp->beecount - MINBEES + 1) + MINBEES;
		/* if sp->beecount is random ... the size can change */
		free_bees(sp);
	} else if (sp->beecount < MINBEES)
		sp->beecount = MINBEES;
	sp->taillen = MI_SIZE(mi);
	if (sp->taillen < -MINTRAIL) {
		/* Change by sqr so its seems more variable */
		sp->taillen = NRAND((int) sqrt((double) (-sp->taillen - MINTRAIL + 1)));
		sp->taillen = sp->taillen * sp->taillen + MINTRAIL;
		free_segs(sp);
	} else if (sp->taillen < MINTRAIL)
		sp->taillen = MINTRAIL;
	sp->width = MI_WIDTH(mi);
	sp->height = MI_HEIGHT(mi);
	sp->border = (sp->width + sp->height) / 50;
	sp->tick = 0;
	sp->rolloverflag = 0;

	if (trackmouse && !sp->cursor) {	/* Create an invisible cursor */
		Pixmap      bit;
		XColor      black;

		black.red = 0;
		black.green = 0;
		black.blue = 0;
		black.flags = DoRed | DoGreen | DoBlue;
		if ((bit = XCreatePixmapFromBitmapData(display, window,
				(char *) "\000", 1, 1, MI_BLACK_PIXEL(mi),
				MI_BLACK_PIXEL(mi), 1)) == None) {
			free_bees(sp); /* Do not need to free cursor */
			return;
		}

		if ((sp->cursor = XCreatePixmapCursor(display, bit, bit,
				 &black, &black, 0, 0)) == None) {
			XFreePixmap(display, bit);
			free_bees(sp);
			return;
		}
		XFreePixmap(display, bit);
	}
	XDefineCursor(display, window, sp->cursor);

	MI_CLEARWINDOW(mi);

	/* Allocate memory. */

	if (sp->segs == NULL) {
		if (((sp->segs = (XSegment *) malloc(sizeof (XSegment) *
			sp->beecount)) == NULL) ||
		    ((sp->old_segs = (XSegment *) malloc(sizeof (XSegment) *
			sp->beecount)) == NULL) ||
		    ((sp->x = (float *) malloc(sizeof (float) * sp->beecount *
			sp->taillen)) == NULL) ||
		    ((sp->y = (float *) malloc(sizeof (float) * sp->beecount *
			sp->taillen)) == NULL) ||
		    ((sp->xv = (float *) malloc(sizeof (float) *
			sp->beecount)) == NULL) ||
		    ((sp->yv = (float *) malloc(sizeof (float) *
			sp->beecount)) == NULL)) {
			free_swarm(display, sp);
			return;
		}
	}
	/* Initialize point positions, velocities, etc. */
	if (MI_NPIXELS(mi) > 2)
		sp->pix = NRAND(MI_NPIXELS(mi));

	/* wasp */
	sp->wx[0] = sp->border + NRAND(sp->width - 2 * sp->border);
	sp->wy[0] = sp->border + NRAND(sp->height - 2 * sp->border);
	sp->wx[1] = sp->wx[0];
	sp->wy[1] = sp->wy[0];
	sp->wxv = 0;
	sp->wyv = 0;

	/* bees */
	for (b = 0; b < sp->beecount; b++) {
		X(0, b) = NRAND(sp->width);
		Y(0, b) = NRAND(sp->height);
		for (t = 1; t < sp->taillen; t++) {
			X(t, b) = X(0, b);
			Y(t, b) = Y(0, b);
		}
		sp->xv[b] = balance_rand(7);
		sp->yv[b] = balance_rand(7);
	}
}

void
draw_swarm(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	int         b, newlimit;
	Bool        track_p = trackmouse;
	int         cx, cy;
	short       prev;
	float       speed;
	swarmstruct *sp;

	if (swarms == NULL)
		return;
	sp = &swarms[MI_SCREEN(mi)];
	if (sp->segs == NULL)
		return;

	MI_IS_DRAWN(mi) = True;
	if (track_p) {
		Window      r, c;
		int         rx, ry;
		unsigned int m;

		(void) XQueryPointer(display, window,
				     &r, &c, &rx, &ry, &cx, &cy, &m);
		if (cx <= sp->border || cy <= sp->border ||
		    cx >= MI_WIDTH(mi) - 1 - sp->border ||
		    cy >= MI_HEIGHT(mi) - 1 - sp->border)
			track_p = False;
	}
	/* <=- Wasp -=> */
	/* Age the arrays. */
	sp->wx[2] = sp->wx[1];
	sp->wx[1] = sp->wx[0];
	sp->wy[2] = sp->wy[1];
	sp->wy[1] = sp->wy[0];
	if (track_p) {
		sp->wx[0] = cx;
		sp->wy[0] = cy;
	} else {

		/* Accelerate */
		sp->wxv += balance_rand(WASPACC);
		sp->wyv += balance_rand(WASPACC);

		/* Speed Limit Checks */
		speed = sqrt((double) sp->wxv * sp->wxv + sp->wyv * sp->wyv);
		if (speed > WASPVEL) {
			newlimit = (int) ((NRAND(WASPVEL) + WASPVEL / 2) / speed);
			sp->wxv *= newlimit;
			sp->wyv *= newlimit;
		}
		/* Move */
		sp->wx[0] = sp->wx[1] + sp->wxv;
		sp->wy[0] = sp->wy[1] + sp->wyv;

		/* Bounce Checks */
		if ((sp->wx[0] < sp->border) || (sp->wx[0] > sp->width - sp->border - 1)) {
			sp->wxv = -sp->wxv;
			sp->wx[0] += sp->wxv;
		}
		if ((sp->wy[0] < sp->border) || (sp->wy[0] > sp->height - sp->border - 1)) {
			sp->wyv = -sp->wyv;
			sp->wy[0] += sp->wyv;
		}
		/* Don't let things settle down. */
		sp->xv[NRAND(sp->beecount)] += balance_rand(3);
		sp->yv[NRAND(sp->beecount)] += balance_rand(3);
	}
	/* <=- Bees -=> */
	sp->tick = ++(sp->tick) % (sp->taillen);
	prev = (sp->tick) ? sp->tick - 1 : sp->taillen - 1; 
	if (sp->tick == sp->taillen - 1)
		sp->rolloverflag = 1;
	for (b = 0; b < sp->beecount; b++) {
		int         distance, dx, dy;

		/* Accelerate */
		dx = (int) (sp->wx[1] - X(prev, b));
		dy = (int) (sp->wy[1] - Y(prev, b));
		distance = (int) sqrt((double) dx * dx + dy * dy);
		if (distance == 0)
			distance = 1;
		sp->xv[b] += (dx * BEEACC) / distance;
		sp->yv[b] += (dy * BEEACC) / distance;

		/* Speed Limit Checks */
		speed = sqrt(sp->xv[b] * sp->xv[b] + sp->yv[b] * sp->yv[b]);
		if (speed > BEEVEL) {
			newlimit = (int) ((NRAND(BEEVEL) + BEEVEL / 2) / speed);
			sp->xv[b] *= newlimit;
			sp->yv[b] *= newlimit;
		}
		/* Move */
		X(sp->tick, b) = X(prev, b) + sp->xv[b];
		Y(sp->tick, b) = Y(prev, b) + sp->yv[b];
 
		/* Fill the segment lists. */
		sp->segs[b].x1 = (short) X(sp->tick, b);
		sp->segs[b].y1 = (short) Y(sp->tick, b);
		sp->segs[b].x2 = (short) X(prev, b);
		sp->segs[b].y2 = (short) Y(prev, b);
		sp->old_segs[b].x1 = (short) X(((sp->tick+2)%sp->taillen), b);
		sp->old_segs[b].y1 = (short) Y(((sp->tick+2)%sp->taillen), b);
		sp->old_segs[b].x2 = (short) X(((sp->tick+1)%sp->taillen), b);
		sp->old_segs[b].y2 = (short) Y(((sp->tick+1)%sp->taillen), b);
	}

	XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
	XDrawLine(display, window, gc,
		  sp->wx[1], sp->wy[1], sp->wx[2], sp->wy[2]);
	if (sp->rolloverflag) {
		XDrawSegments(display, window, gc, sp->old_segs, sp->beecount);
	}
	XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
	XDrawLine(display, window, gc,
		  sp->wx[0], sp->wy[0], sp->wx[1], sp->wy[1]);
	if (MI_NPIXELS(mi) > 2) {
		XSetForeground(display, gc, MI_PIXEL(mi, sp->pix));
		if (++sp->pix >= MI_NPIXELS(mi))
			sp->pix = 0;
	}
	XDrawSegments(display, window, gc, sp->segs, sp->beecount);
}

void
release_swarm(ModeInfo * mi)
{
	if (swarms != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_swarm(MI_DISPLAY(mi), &swarms[screen]);
		(void) free((void *) swarms);
		swarms = NULL;
	}
}

void
refresh_swarm(ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
}

#endif /* MODE_swarm */
