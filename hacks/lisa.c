/* -*- Mode: C; tab-width: 4 -*- */
/* lisa --- animated full-loop lisajous figures */

#if 0
static const char sccsid[] = "@(#)lisa.c	5.00 2000/11/01 xlockmore";
#endif

/*-
 * Copyright (c) 1997 by Caleb Cullen.
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
 *
 * The inspiration for this program, Lasp, was written by Adam B. Roach
 * in 1990, assisted by me, Caleb Cullen.  It was written first in C, then
 * in assembly, and used pre-calculated data tables to graph lisajous
 * figures on 386 machines and lower.  This version bears only superficial
 * resemblances to the original Lasp.
 *
 * The `lissie' module's source code was studied as an example of how
 * to incorporate a new module into xlock.  Resemblances to it are
 * expected, but not intended to be plaigiaristic.
 */

#ifdef STANDALONE
#define MODE_lisa
#define PROGCLASS "Lisa"
#define HACK_INIT init_lisa
#define HACK_DRAW draw_lisa
#define lisa_opts xlockmore_opts
#define DEFAULTS "*delay: 25000 \n" \
 "*count: 1 \n" \
 "*cycles: 256 \n" \
 "*size: -1 \n" \
 "*ncolors: 200 \n"
#define UNIFORM_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */

#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */

#endif /* STANDALONE */

#ifdef MODE_lisa

#define  DEF_ADDITIVE     "True"

static Bool additive;

static XrmOptionDescRec opts[] =
{
	{"-additive", ".lisa.additive", XrmoptionNoArg, "True"},
	{"+additive", ".lisa.additive", XrmoptionNoArg, "False"}
};

static argtype vars[] =
{
	{&additive, "additive", "Additive", DEF_ADDITIVE, t_Bool}
};

static OptionStruct desc[] =
{
	{"-/+additive", "turn on/off additive functions mode"}
};

ModeSpecOpt lisa_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   lisa_description =
{"lisa", "init_lisa", "draw_lisa", "release_lisa",
 "refresh_lisa", "change_lisa", (char *) NULL, &lisa_opts,
 25000, 1, 256, -1, 64, 1.0, "",
 "Shows animated lisajous loops", 0, NULL};

#endif

#define  DRAWLINES    1
#define  TWOLOOPS     1
#define  XVMAX        10	/* Maximum velocities */
#define  YVMAX        10
#define  LISAMAXFUNCS 2
#define  NUMSTDFUNCS  10
#define  MAXCYCLES    3
#define  MINLISAS     1
#define  lisasetcolor() \
if (MI_NPIXELS(mi) > 2) { \
  XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_PIXEL(mi, loop->color)); \
  if (++(loop->color) >= (unsigned) MI_NPIXELS(mi)) { loop->color=0; } \
  } else { XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_WHITE_PIXEL(mi)); }
#define getRadius(context) \
  ((context->width > context->height)?context->height:context->width) * 3 / 8
#define checkRadius(loop, context) \
  if ((context->height / 2 > MI_SIZE(mi)) && (context->width / 2 > MI_SIZE(mi))) \
      loop->radius = MI_SIZE(mi); \
  if ((loop->radius < 0) || \
      (loop->radius > loop->center.x) || \
      (loop->radius > loop->center.y)) loop->radius = getRadius(context)


typedef struct lisafunc_struct {
	double      xcoeff[2], ycoeff[2];
	int         nx, ny;
	int         index;
} lisafuncs;

typedef struct lisa_struct {
	unsigned long color;
	int         radius, dx, dy, nsteps, nfuncs, melting;
	double      pistep, phi, theta;
	XPoint      center, *lastpoint;
	lisafuncs  *function[LISAMAXFUNCS];
	int         linewidth;
} lisas;

typedef struct lisacontext_struct {
	lisas      *lisajous;
	int         width, height, nlisajous, loopcount;
	int         maxcycles;
	Bool        painted;
} lisacons;

static lisacons *Lisa = (lisacons *) NULL;

static lisafuncs Function[NUMSTDFUNCS] =
{
	{
		{1.0, 2.0},
		{1.0, 2.0}, 2, 2, 0},
	{
		{1.0, 2.0},
		{1.0, 1.0}, 2, 2, 1},
	{
		{1.0, 3.0},
		{1.0, 2.0}, 2, 2, 2},
	{
		{1.0, 3.0},
		{1.0, 3.0}, 2, 2, 3},
	{
		{2.0, 4.0},
		{1.0, 2.0}, 2, 2, 4},
	{
		{1.0, 4.0},
		{1.0, 3.0}, 2, 2, 5},
	{
		{1.0, 4.0},
		{1.0, 4.0}, 2, 2, 6},
	{
		{1.0, 5.0},
		{1.0, 5.0}, 2, 2, 7},
	{
		{2.0, 5.0},
		{2.0, 5.0}, 2, 2, 8},
	{
		{1.0, 0.0},
		{1.0, 0.0}, 1, 1, 9}
};

static void
free_lisa(lisacons *lc)
{
	while (lc->lisajous) {
		int    lctr;

		for (lctr = 0; lctr < lc->nlisajous; lctr++) {
			(void) free((void *) lc->lisajous[lctr].lastpoint);
		}
		(void) free((void *) lc->lisajous);
		lc->lisajous = (lisas *) NULL;
	}
}

static Bool
drawlisa(ModeInfo * mi, lisas * loop)
{
	XPoint     *np;
	XPoint     *lp = loop->lastpoint;
	lisacons   *lc = &Lisa[MI_SCREEN(mi)];
	lisafuncs **lf = loop->function;
	int         phase = lc->loopcount % loop->nsteps;
	int         pctr, fctr, xctr, yctr;
	double      xprod, yprod, xsum, ysum;

	/* Allocate the np array */
	if ((np = (XPoint *) calloc(loop->nsteps, sizeof (XPoint))) == NULL) {
		free_lisa(lc);
		return False;
	}

	/* Update the center */
	loop->center.x += loop->dx;
	loop->center.y += loop->dy;
	checkRadius(loop, lc);
	if ((loop->center.x - loop->radius) <= 0) {
		loop->center.x = loop->radius;
		loop->dx = NRAND(XVMAX);
	} else if ((loop->center.x + loop->radius) >= lc->width) {
		loop->center.x = lc->width - loop->radius;
		loop->dx = -NRAND(XVMAX);
	};
	if ((loop->center.y - loop->radius) <= 0) {
		loop->center.y = loop->radius;
		loop->dy = NRAND(YVMAX);
	} else if ((loop->center.y + loop->radius) >= lc->height) {
		loop->center.y = lc->height - loop->radius;
		loop->dy = -NRAND(YVMAX);
	};

	/* Now draw the points, and erase the ones from the last cycle */

	for (pctr = 0; pctr < loop->nsteps; pctr++) {
		fctr = loop->nfuncs;
		loop->phi = (double) (pctr - phase) * loop->pistep;
		loop->theta = (double) (pctr + phase) * loop->pistep;
		xsum = ysum = 0;
		while (fctr--) {
			xctr = lf[fctr]->nx;
			yctr = lf[fctr]->ny;
			if (additive) {
				xprod = yprod = 0.0;
				while (xctr--)
					xprod += sin(lf[fctr]->xcoeff[xctr] * loop->theta);
				while (yctr--)
					yprod += sin(lf[fctr]->ycoeff[yctr] * loop->phi);
				if (loop->melting) {
					if (fctr) {
						xsum += xprod * (double) (loop->nsteps - loop->melting) /
							(double) loop->nsteps;
						ysum += yprod * (double) (loop->nsteps - loop->melting) /
							(double) loop->nsteps;
					} else {
						xsum += xprod * (double) loop->melting / (double) loop->nsteps;
						ysum += yprod * (double) loop->melting / (double) loop->nsteps;
					}
				} else {
					xsum = xprod;
					ysum = yprod;
				}
				if (!fctr) {
					xsum = xsum * (double) loop->radius / (double) lf[fctr]->nx;
					ysum = ysum * (double) loop->radius / (double) lf[fctr]->ny;
				}
			} else {
				if (loop->melting) {
					if (fctr) {
						yprod = xprod = (double) loop->radius *
							(double) (loop->nsteps - loop->melting) /
							(double) (loop->nsteps);
					} else {
						yprod = xprod = (double) loop->radius *
							(double) (loop->melting) / (double) (loop->nsteps);
					}
				} else {
					xprod = yprod = (double) loop->radius;
				}
				while (xctr--)
					xprod *= sin(lf[fctr]->xcoeff[xctr] * loop->theta);
				while (yctr--)
					yprod *= sin(lf[fctr]->ycoeff[yctr] * loop->phi);
				xsum += xprod;
				ysum += yprod;
			}
		}
		if ((loop->nfuncs > 1) && (!loop->melting)) {
			xsum /= (double) loop->nfuncs;
			ysum /= (double) loop->nfuncs;
		}
		xsum += (double) loop->center.x;
		ysum += (double) loop->center.y;

		np[pctr].x = (int) ceil(xsum);
		np[pctr].y = (int) ceil(ysum);
	}
	if (loop->melting) {
		if (!--loop->melting) {
			loop->nfuncs = 1;
			loop->function[0] = loop->function[1];
		}
	}
	for (pctr = 0; pctr < loop->nsteps; pctr++) {

#if defined DRAWLINES
		XSetLineAttributes(MI_DISPLAY(mi), MI_GC(mi), loop->linewidth,
				   LineSolid, CapProjecting, JoinMiter);
		/* erase the last cycle's point */
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_BLACK_PIXEL(mi));
		XDrawLine(MI_DISPLAY(mi), MI_WINDOW(mi),
			  MI_GC(mi), lp[pctr].x, lp[pctr].y,
			  lp[(pctr + 1) % loop->nsteps].x,
			  lp[(pctr + 1) % loop->nsteps].y);

		/* Set the new color */
		lisasetcolor();

		/* plot this cycle's point */
		XDrawLine(MI_DISPLAY(mi), MI_WINDOW(mi),
			  MI_GC(mi), np[pctr].x, np[pctr].y,
			  np[(pctr + 1) % loop->nsteps].x,
			  np[(pctr + 1) % loop->nsteps].y);
		XSetLineAttributes(MI_DISPLAY(mi), MI_GC(mi), 1,
				   LineSolid, CapProjecting, JoinMiter);
#else
		/* erase the last cycle's point */
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_BLACK_PIXEL(mi));
		XDrawPoint(MI_DISPLAY(mi), MI_WINDOW(mi),
			   MI_GC(mi), lp[pctr].x, lp[pctr].y);

		/* Set the new color */
		lisasetcolor();

		/* plot this cycle's point */
		XDrawPoint(MI_DISPLAY(mi), MI_WINDOW(mi),
			   MI_GC(mi), np[pctr].x, np[pctr].y);
#endif
	}
	(void) free((void *) lp);
	loop->lastpoint = np;
	return True;
}

static Bool 
initlisa(ModeInfo * mi, lisas * loop)
{
	lisacons   *lc = &Lisa[MI_SCREEN(mi)];
	lisafuncs **lf = loop->function;
	XPoint     *lp;
	int         phase, pctr, fctr, xctr, yctr;
	double      xprod, yprod, xsum, ysum;

	if (MI_NPIXELS(mi) > 2) {
		loop->color = 0;
	} else
		loop->color = MI_WHITE_PIXEL(mi);
	loop->nsteps = MI_CYCLES(mi);
	if (loop->nsteps == 0)
		loop->nsteps = 1;
	lc->maxcycles = (MAXCYCLES * loop->nsteps) - 1;
	loop->melting = 0;
	loop->nfuncs = 1;
	loop->pistep = 2.0 * M_PI / (double) loop->nsteps;
	loop->center.x = lc->width / 2;
	loop->center.y = lc->height / 2;
	loop->radius = (int) MI_SIZE(mi);
	checkRadius(loop, lc);
	loop->dx = NRAND(XVMAX);
	loop->dy = NRAND(YVMAX);
	loop->dx++;
	loop->dy++;
	lf[0] = &Function[lc->loopcount % NUMSTDFUNCS];
	if ((lp = loop->lastpoint = (XPoint *)
	     calloc(loop->nsteps, sizeof (XPoint))) == NULL) {
		free_lisa(lc);
		return False;
	}
	phase = lc->loopcount % loop->nsteps;

	for (pctr = 0; pctr < loop->nsteps; pctr++) {
		loop->phi = (double) (pctr - phase) * loop->pistep;
		loop->theta = (double) (pctr + phase) * loop->pistep;
		fctr = loop->nfuncs;
		xsum = ysum = 0.0;
		while (fctr--) {
			xprod = yprod = (double) loop->radius;
			xctr = lf[fctr]->nx;
			yctr = lf[fctr]->ny;
			while (xctr--)
				xprod *= sin(lf[fctr]->xcoeff[xctr] * loop->theta);
			while (yctr--)
				yprod *= sin(lf[fctr]->ycoeff[yctr] * loop->phi);
			xsum += xprod;
			ysum += yprod;
		}
		if (loop->nfuncs > 1) {
			xsum /= 2.0;
			ysum /= 2.0;
		}
		xsum += (double) loop->center.x;
		ysum += (double) loop->center.y;

		lp[pctr].x = (int) ceil(xsum);
		lp[pctr].y = (int) ceil(ysum);
	}
#if defined DRAWLINES
	{
		loop->linewidth = -8;	/* #### make this a resource */

		if (loop->linewidth == 0)
			loop->linewidth = 1;
		if (loop->linewidth < 0)
			loop->linewidth = NRAND(-loop->linewidth) + 1;
		XSetLineAttributes(MI_DISPLAY(mi), MI_GC(mi), loop->linewidth,
				   LineSolid, CapProjecting, JoinMiter);
	}
#endif
	for (pctr = 0; pctr < loop->nsteps; pctr++) {
		/* Set the color */
		lisasetcolor();
#if defined DRAWLINES
		XDrawLine(MI_DISPLAY(mi), MI_WINDOW(mi),
			  MI_GC(mi), lp[pctr].x, lp[pctr].y,
			  lp[(pctr + 1) % loop->nsteps].x,
			  lp[(pctr + 1) % loop->nsteps].y);
#else
		XDrawPoint(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi),
			   lp[pctr].x, lp[pctr].y);
#endif
	}
#if defined DRAWLINES
	XSetLineAttributes(MI_DISPLAY(mi), MI_GC(mi), 1,
			   LineSolid, CapProjecting, JoinMiter);
#endif
	return True;
}

static void
refreshlisa(ModeInfo * mi)
{
	lisacons   *lc = &Lisa[MI_SCREEN(mi)];
	int         lctr;

	for (lctr = 0; lctr < lc->nlisajous; lctr++) {
		if (!drawlisa(mi, &lc->lisajous[lctr]))
			return;
	}
}

void
refresh_lisa(ModeInfo * mi)
{
	lisacons   *lc;

	if (Lisa == NULL)
		return;
	lc = &Lisa[MI_SCREEN(mi)];
	if (lc->lisajous == NULL)
		return;

	if (lc->painted) {
		lc->painted = False;
		MI_CLEARWINDOW(mi);
		refreshlisa(mi);
	}
}

void
change_lisa(ModeInfo * mi)
{
	lisas      *loop;
	int         lctr;
	lisacons   *lc;

	if (Lisa == NULL)
		return;
	lc = &Lisa[MI_SCREEN(mi)];
	if (lc->lisajous == NULL)
		return;

	lc->loopcount = 0;
	for (lctr = 0; lctr < lc->nlisajous; lctr++) {
		loop = &lc->lisajous[lctr];
		loop->function[1] = &Function[(loop->function[0]->index + 1) %
					      NUMSTDFUNCS];
		loop->melting = loop->nsteps - 1;
		loop->nfuncs = 2;
	}
}

void
init_lisa(ModeInfo * mi)
{
	int         lctr;
	lisacons   *lc;

	if (Lisa == NULL) {
		if ((Lisa = (lisacons *) calloc(MI_NUM_SCREENS(mi),
				 sizeof (lisacons))) == NULL)
			return;
	}
	lc = &Lisa[MI_SCREEN(mi)];
	lc->width = MI_WIDTH(mi);
	lc->height = MI_HEIGHT(mi);
	lc->loopcount = 0;
	lc->nlisajous = MI_COUNT(mi);
	if (lc->nlisajous <= 0)
		lc->nlisajous = 1;
	MI_CLEARWINDOW(mi);
	lc->painted = False;

	if (lc->lisajous == NULL) {
		if ((lc->lisajous = (lisas *) calloc(lc->nlisajous,
				sizeof (lisas))) == NULL)
			return;
		for (lctr = 0; lctr < lc->nlisajous; lctr++) {
			if (!initlisa(mi, &lc->lisajous[lctr]))
				return;
			lc->loopcount++;
		}
	} else {
		refreshlisa(mi);
	}
}

void
draw_lisa(ModeInfo * mi)
{
	lisacons   *lc;

	if (Lisa == NULL)
		return;
	lc = &Lisa[MI_SCREEN(mi)];
	if (lc->lisajous == NULL)
		return;

	MI_IS_DRAWN(mi) = True;
	lc->painted = True;
	if (++lc->loopcount > lc->maxcycles) {
		change_lisa(mi);
	}
	refreshlisa(mi);
}

void
release_lisa(ModeInfo * mi)
{
	if (Lisa) {
		int    screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_lisa(&Lisa[screen]);
		(void) free(Lisa);
		Lisa = (lisacons *) NULL;
	}
}

#endif /* MODE_lisa */
