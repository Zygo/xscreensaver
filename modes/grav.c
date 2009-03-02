/* -*- Mode: C; tab-width: 4 -*- */
/* grav --- planets spinning around a pulsar */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)grav.c	5.00 2000/11/01 xlockmore";

#endif

/*-
 * Copyright (c) 1993 by Greg Boewring <gb@pobox.com>
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
 * 11-Jul-1994: color version
 * 06-Oct-1993: Written by Greg Bowering <gb@pobox.com>
 */

#ifdef STANDALONE
#define PROGCLASS "Grav"
#define HACK_INIT init_grav
#define HACK_DRAW draw_grav
#define grav_opts xlockmore_opts
#define DEFAULTS "*delay: 10000 \n" \
 "*count: -12 \n" \
 "*ncolors: 64 \n"
#define BRIGHT_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */

#endif /* STANDALONE */

#ifdef MODE_grav

#define DEF_DECAY "False"	/* Damping for decaying orbits */
#define DEF_TRAIL "False"	/* For trails (works good in mono only) */

static Bool decay;
static Bool trail;

static XrmOptionDescRec opts[] =
{
	{(char *) "-decay", (char *) ".grav.decay", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+decay", (char *) ".grav.decay", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-trail", (char *) ".grav.trail", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+trail", (char *) ".grav.trail", XrmoptionNoArg, (caddr_t) "off"}
};
static argtype vars[] =
{
	{(caddr_t *) & decay, (char *) "decay", (char *) "Decay", (char *) DEF_DECAY, t_Bool},
	{(caddr_t *) & trail, (char *) "trail", (char *) "Trail", (char *) DEF_TRAIL, t_Bool}
};
static OptionStruct desc[] =
{
	{(char *) "-/+decay", (char *) "turn on/off decaying orbits"},
	{(char *) "-/+trail", (char *) "turn on/off trail dots"}
};

ModeSpecOpt grav_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   grav_description =
{"grav", "init_grav", "draw_grav", "release_grav",
 "refresh_grav", "init_grav", NULL, &grav_opts,
 10000, -12, 1, 1, 64, 1.0, "",
 "Shows orbiting planets", 0, NULL};

#endif

#define GRAV			-0.02	/* Gravitational constant */
#define DIST			16.0
#define COLLIDE			0.0001
#define ALMOST			15.99
#define HALF			0.5
/* #define INTRINSIC_RADIUS     200.0 */
#define INTRINSIC_RADIUS	((float) (gp->height/5))
#define STARRADIUS		(unsigned int)(gp->height/(2*DIST))
#define AVG_RADIUS		(INTRINSIC_RADIUS/DIST)
#define RADIUS			(unsigned int)(INTRINSIC_RADIUS/(POS(Z)+DIST))

#define XR			HALF*ALMOST
#define YR			HALF*ALMOST
#define ZR			HALF*ALMOST

#define VR			0.04

#define DIMENSIONS		3
#define X			0
#define Y			1
#define Z			2

#define DAMP			0.999999
#define MaxA			0.1	/* Maximum acceleration (w/ damping) */

#define POS(c) planet->P[c]
#define VEL(c) planet->V[c]
#define ACC(c) planet->A[c]

#define Planet(x,y)\
  if ((x) >= 0 && (y) >= 0 && (x) <= gp->width && (y) <= gp->height) {\
    if (planet->ri < 2)\
     XDrawPoint(display, window, gc, (x), (y));\
    else\
     XFillArc(display, window, gc,\
      (x) - planet->ri / 2, (y) - planet->ri / 2, planet->ri, planet->ri,\
      0, 23040);\
   }

#define FLOATRAND(min,max)	((min)+(LRAND()/MAXRAND)*((max)-(min)))

typedef struct {
	double      P[DIMENSIONS], V[DIMENSIONS], A[DIMENSIONS];
	int         xi, yi, ri;
	unsigned long colors;
} planetstruct;

typedef struct {
	int         width, height;
	int         x, y, sr, nplanets;
	unsigned long starcolor;
	planetstruct *planets;
} gravstruct;

static gravstruct *gravs = NULL;

static void
init_planet(ModeInfo * mi, planetstruct * planet)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	gravstruct *gp = &gravs[MI_SCREEN(mi)];

	if (MI_NPIXELS(mi) > 2)
		planet->colors = MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));
	else
		planet->colors = MI_WHITE_PIXEL(mi);
	/* Initialize positions */
	POS(X) = FLOATRAND(-XR, XR);
	POS(Y) = FLOATRAND(-YR, YR);
	POS(Z) = FLOATRAND(-ZR, ZR);

	if (POS(Z) > -ALMOST) {
		planet->xi = (int)
			((double) gp->width * (HALF + POS(X) / (POS(Z) + DIST)));
		planet->yi = (int)
			((double) gp->height * (HALF + POS(Y) / (POS(Z) + DIST)));
	} else
		planet->xi = planet->yi = -1;
	planet->ri = RADIUS;

	/* Initialize velocities */
	VEL(X) = FLOATRAND(-VR, VR);
	VEL(Y) = FLOATRAND(-VR, VR);
	VEL(Z) = FLOATRAND(-VR, VR);

	/* Draw planets */
	Planet(planet->xi, planet->yi);
}

static void
draw_planet(ModeInfo * mi, planetstruct * planet)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	gravstruct *gp = &gravs[MI_SCREEN(mi)];
	double      D;		/* A distance variable to work with */
	register unsigned char cmpt;

	D = POS(X) * POS(X) + POS(Y) * POS(Y) + POS(Z) * POS(Z);
	if (D < COLLIDE)
		D = COLLIDE;
	D = sqrt(D);
	D = D * D * D;
	for (cmpt = X; cmpt < DIMENSIONS; cmpt++) {
		ACC(cmpt) = POS(cmpt) * GRAV / D;
		if (decay) {
			if (ACC(cmpt) > MaxA)
				ACC(cmpt) = MaxA;
			else if (ACC(cmpt) < -MaxA)
				ACC(cmpt) = -MaxA;
			VEL(cmpt) = VEL(cmpt) + ACC(cmpt);
			VEL(cmpt) *= DAMP;
		} else {
			/* update velocity */
			VEL(cmpt) = VEL(cmpt) + ACC(cmpt);
		}
		/* update position */
		POS(cmpt) = POS(cmpt) + VEL(cmpt);
	}

	gp->x = planet->xi;
	gp->y = planet->yi;

	if (POS(Z) > -ALMOST) {
		planet->xi = (int)
			((double) gp->width * (HALF + POS(X) / (POS(Z) + DIST)));
		planet->yi = (int)
			((double) gp->height * (HALF + POS(Y) / (POS(Z) + DIST)));
	} else
		planet->xi = planet->yi = -1;

	/* Mask */
	XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
	Planet(gp->x, gp->y);
	if (trail) {
		XSetForeground(display, gc, planet->colors);
		XDrawPoint(display, MI_WINDOW(mi), gc, gp->x, gp->y);
	}
	/* Move */
	gp->x = planet->xi;
	gp->y = planet->yi;
	planet->ri = RADIUS;

	/* Redraw */
	XSetForeground(display, gc, planet->colors);
	Planet(gp->x, gp->y);
}

void
init_grav(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	unsigned char ball;
	gravstruct *gp;

	if (gravs == NULL) {
		if ((gravs = (gravstruct *) calloc(MI_NUM_SCREENS(mi),
					       sizeof (gravstruct))) == NULL)
			return;
	}
	gp = &gravs[MI_SCREEN(mi)];

	gp->width = MI_WIDTH(mi);
	gp->height = MI_HEIGHT(mi);

	gp->sr = STARRADIUS;

	gp->nplanets = MI_COUNT(mi);
	if (gp->nplanets < 0) {
		if (gp->planets) {
			(void) free((void *) gp->planets);
			gp->planets = NULL;
		}
		gp->nplanets = NRAND(-gp->nplanets) + 1;	/* Add 1 so its not too boring */
	}
	if (gp->planets == NULL) {
		if ((gp->planets = (planetstruct *) calloc(gp->nplanets,
				sizeof (planetstruct))) == NULL)
			return;
	}

	MI_CLEARWINDOW(mi);

	if (MI_NPIXELS(mi) > 2)
		gp->starcolor = MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));
	else
		gp->starcolor = MI_WHITE_PIXEL(mi);
	for (ball = 0; ball < (unsigned char) gp->nplanets; ball++)
		init_planet(mi, &gp->planets[ball]);

	/* Draw centrepoint */
	XDrawArc(display, MI_WINDOW(mi), gc,
		 gp->width / 2 - gp->sr / 2, gp->height / 2 - gp->sr / 2, gp->sr, gp->sr,
		 0, 23040);
}

void
draw_grav(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	register unsigned char ball;
	gravstruct *gp;

	if (gravs == NULL)
			return;
	gp = &gravs[MI_SCREEN(mi)];
	if (gp->planets == NULL)
		return;

	MI_IS_DRAWN(mi) = True;
	/* Mask centrepoint */
	XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
	XDrawArc(display, window, gc,
		 gp->width / 2 - gp->sr / 2, gp->height / 2 - gp->sr / 2, gp->sr, gp->sr,
		 0, 23040);

	/* Resize centrepoint */
	switch (NRAND(4)) {
		case 0:
			if (gp->sr < (int) STARRADIUS)
				gp->sr++;
			break;
		case 1:
			if (gp->sr > 2)
				gp->sr--;
	}

	/* Draw centrepoint */
	XSetForeground(display, gc, gp->starcolor);
	XDrawArc(display, window, gc,
		 gp->width / 2 - gp->sr / 2, gp->height / 2 - gp->sr / 2, gp->sr, gp->sr,
		 0, 23040);

	for (ball = 0; ball < (unsigned char) gp->nplanets; ball++)
		draw_planet(mi, &gp->planets[ball]);
}

void
release_grav(ModeInfo * mi)
{
	if (gravs != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			gravstruct *gp = &gravs[screen];

			if (gp->planets)
				(void) free((void *) gp->planets);
		}
		(void) free((void *) gravs);
		gravs = NULL;
	}
}

void
refresh_grav(ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
}

#endif /* MODE_grav */
