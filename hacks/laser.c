/* -*- Mode: C; tab-width: 4 -*- */
/* laser --- spinning lasers */

#if 0
static const char sccsid[] = "@(#)laser.c	5.00 2000/11/01 xlockmore";
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
 * 1995: Written.
 */

#ifdef STANDALONE
# define MODE_laser
# define DEFAULTS	"*delay: 40000 \n" \
					"*count: 10 \n" \
					"*cycles: 200 \n" \
					"*ncolors: 64 \n" \
					"*fpsSolid: true \n" \

# define BRIGHT_COLORS
# define reshape_laser 0
# define laser_handle_event 0
# include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
# include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_laser

ENTRYPOINT ModeSpecOpt laser_opts =
{0, (XrmOptionDescRec *) NULL, 0, (argtype *) NULL, (OptionStruct *) NULL};

#ifdef USE_MODULES
ModStruct   laser_description =
{"laser", "init_laser", "draw_laser", "release_laser",
 "refresh_laser", "init_laser", (char *) NULL, &laser_opts,
 20000, -10, 200, 1, 64, 1.0, "",
 "Shows spinning lasers", 0, NULL};

#endif

#define MINREDRAW 3		/* Number of redrawn on each frame */
#define MAXREDRAW 8

#define MINLASER  1		/* Laser number */

#define MINWIDTH  2		/* Laser ray width range */
#define MAXWIDTH 40

#define MINSPEED  2		/* Speed range */
#define MAXSPEED 17

#define MINDIST  10		/* Minimal distance from edges */

#define COLORSTEP 2		/* Laser color step */

#define RANGE_RAND(min,max) (int) ((min) + LRAND() % ((max) - (min)))

typedef enum {
	TOP, RIGHT, BOTTOM, LEFT
} border;

typedef struct {
	int         bx;		/* border x */
	int         by;		/* border y */
	border      bn;		/* active border */
	int         dir;	/* direction */
	int         speed;	/* laser velocity from MINSPEED to MAXSPEED */
	int         sx[MAXWIDTH];	/* x stack */
	int         sy[MAXWIDTH];	/* x stack */
	XGCValues   gcv;	/* for color */
} laserstruct;

typedef struct {
	int         width;
	int         height;
	int         cx;		/* center x */
	int         cy;		/* center y */
	int         lw;		/* laser width */
	int         ln;		/* laser number */
	int         lr;		/* laser redraw */
	int         sw;		/* stack width */
	int         so;		/* stack offset */
	int         time;	/* up time */
	GC          stippledGC;
	XGCValues   gcv_black;	/* for black color */
	laserstruct *laser;
} lasersstruct;

static lasersstruct *lasers = (lasersstruct *) NULL;

static void
free_laser(Display *display, lasersstruct *lp)
{
	if (lp->laser != NULL) {
		(void) free((void *) lp->laser);
		lp->laser = (laserstruct *) NULL;
	}
	if (lp->stippledGC != None) {
		XFreeGC(display, lp->stippledGC);
		lp->stippledGC = None;
	}
}

ENTRYPOINT void
init_laser(ModeInfo * mi)
{
	Display *display = MI_DISPLAY(mi);
	int         i, c = 0;
	lasersstruct *lp;

	if (lasers == NULL) {
		if ((lasers = (lasersstruct *) calloc(MI_NUM_SCREENS(mi),
					     sizeof (lasersstruct))) == NULL)
			return;
	}
	lp = &lasers[MI_SCREEN(mi)];

	lp->width = MI_WIDTH(mi);
	lp->height = MI_HEIGHT(mi);
	lp->time = 0;

	lp->ln = MI_COUNT(mi);
	if (lp->ln < -MINLASER) {
		/* if lp->ln is random ... the size can change */
		if (lp->laser != NULL) {
			(void) free((void *) lp->laser);
			lp->laser = (laserstruct *) NULL;
		}
		lp->ln = NRAND(-lp->ln - MINLASER + 1) + MINLASER;
	} else if (lp->ln < MINLASER)
		lp->ln = MINLASER;

	if (lp->laser == NULL) {
		if ((lp->laser = (laserstruct *) malloc(lp->ln *
				sizeof (laserstruct))) == NULL) {
			free_laser(display, lp);
			return;
		}
	}
	if (lp->stippledGC == None) {
		XGCValues   gcv;

		gcv.foreground = MI_WHITE_PIXEL(mi);
		gcv.background = MI_BLACK_PIXEL(mi);
		lp->gcv_black.foreground = MI_BLACK_PIXEL(mi);
		if ((lp->stippledGC = XCreateGC(display, MI_WINDOW(mi),
				GCForeground | GCBackground, &gcv)) == None) {
			free_laser(display, lp);
			return;
		}
# ifdef HAVE_JWXYZ
    jwxyz_XSetAntiAliasing (MI_DISPLAY(mi), lp->stippledGC, False);
# endif
	}
	MI_CLEARWINDOW(mi);

	if (MINDIST < lp->width - MINDIST)
		lp->cx = RANGE_RAND(MINDIST, lp->width - MINDIST);
	else
		lp->cx = RANGE_RAND(0, lp->width);
	if (MINDIST < lp->height - MINDIST)
		lp->cy = RANGE_RAND(MINDIST, lp->height - MINDIST);
	else
		lp->cy = RANGE_RAND(0, lp->height);
	lp->lw = RANGE_RAND(MINWIDTH, MAXWIDTH);
	lp->lr = RANGE_RAND(MINREDRAW, MAXREDRAW);
	lp->sw = 0;
	lp->so = 0;

	if (MI_NPIXELS(mi) > 2)
		c = NRAND(MI_NPIXELS(mi));

	for (i = 0; i < lp->ln; i++) {
		laserstruct *l = &lp->laser[i];

		l->bn = (border) NRAND(4);

		switch (l->bn) {
			case TOP:
				l->bx = NRAND(lp->width);
				l->by = 0;
				break;
			case RIGHT:
				l->bx = lp->width;
				l->by = NRAND(lp->height);
				break;
			case BOTTOM:
				l->bx = NRAND(lp->width);
				l->by = lp->height;
				break;
			case LEFT:
				l->bx = 0;
				l->by = NRAND(lp->height);
		}

		l->dir = (int) (LRAND() & 1);
		l->speed = ((RANGE_RAND(MINSPEED, MAXSPEED) * lp->width) / 1000) + 1;
		if (MI_NPIXELS(mi) > 2) {
			l->gcv.foreground = MI_PIXEL(mi, c);
			c = (c + COLORSTEP) % MI_NPIXELS(mi);
		} else
			l->gcv.foreground = MI_WHITE_PIXEL(mi);
	}
}

static void
draw_laser_once(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	lasersstruct *lp = &lasers[MI_SCREEN(mi)];
	int         i;

	for (i = 0; i < lp->ln; i++) {
		laserstruct *l = &lp->laser[i];

		if (lp->sw >= lp->lw) {
			XChangeGC(display, lp->stippledGC, GCForeground, &(lp->gcv_black));
			XDrawLine(display, MI_WINDOW(mi), lp->stippledGC,
				  lp->cx, lp->cy,
				  l->sx[lp->so], l->sy[lp->so]);
		}
		if (l->dir) {
			switch (l->bn) {
				case TOP:
					l->bx -= l->speed;
					if (l->bx < 0) {
						l->by = -l->bx;
						l->bx = 0;
						l->bn = LEFT;
					}
					break;
				case RIGHT:
					l->by -= l->speed;
					if (l->by < 0) {
						l->bx = lp->width + l->by;
						l->by = 0;
						l->bn = TOP;
					}
					break;
				case BOTTOM:
					l->bx += l->speed;
					if (l->bx >= lp->width) {
						l->by = lp->height - l->bx % lp->width;
						l->bx = lp->width;
						l->bn = RIGHT;
					}
					break;
				case LEFT:
					l->by += l->speed;
					if (l->by >= lp->height) {
						l->bx = l->by % lp->height;
						l->by = lp->height;
						l->bn = BOTTOM;
					}
			}
		} else {
			switch (l->bn) {
				case TOP:
					l->bx += l->speed;
					if (l->bx >= lp->width) {
						l->by = l->bx % lp->width;
						l->bx = lp->width;
						l->bn = RIGHT;
					}
					break;
				case RIGHT:
					l->by += l->speed;
					if (l->by >= lp->height) {
						l->bx = lp->width - l->by % lp->height;
						l->by = lp->height;
						l->bn = BOTTOM;
					}
					break;
				case BOTTOM:
					l->bx -= l->speed;
					if (l->bx < 0) {
						l->by = lp->height + l->bx;
						l->bx = 0;
						l->bn = LEFT;
					}
					break;
				case LEFT:
					l->by -= l->speed;
					if (l->by < 0) {
						l->bx = -l->bx;
						l->by = 0;
						l->bn = TOP;
					}
			}
		}

		XChangeGC(display, lp->stippledGC, GCForeground, &l->gcv);
		XDrawLine(display, MI_WINDOW(mi), lp->stippledGC,
			  lp->cx, lp->cy, l->bx, l->by);

		l->sx[lp->so] = l->bx;
		l->sy[lp->so] = l->by;

	}

	if (lp->sw < lp->lw)
		++lp->sw;

	lp->so = (lp->so + 1) % lp->lw;
}

ENTRYPOINT void
draw_laser(ModeInfo * mi)
{
	int         i;
	lasersstruct *lp;

	if (lasers == NULL)
		return;
	lp = &lasers[MI_SCREEN(mi)];
	if (lp->laser == NULL)
		return;

	MI_IS_DRAWN(mi) = True;
	for (i = 0; i < lp->lr; i++)
		draw_laser_once(mi);

	if (++lp->time > MI_CYCLES(mi))
		init_laser(mi);
}

ENTRYPOINT void
release_laser(ModeInfo * mi)
{
	if (lasers != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_laser(MI_DISPLAY(mi), &lasers[screen]);
		(void) free((void *) lasers);
		lasers = (lasersstruct *) NULL;
	}
}

ENTRYPOINT void
refresh_laser(ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
}

XSCREENSAVER_MODULE ("Laser", laser)

#endif /* MODE_laser */
