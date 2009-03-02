/* -*- Mode: C; tab-width: 4 -*- */
/* rotor --- a swirly rotor */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)rotor.c	4.07 97/11/24 xlockmore";

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
 * 10-May-97: Compatible with xscreensaver
 * 08-Mar-95: CAT stuff for ## was tripping up some C compilers.  Removed.
 * 01-Dec-93: added patch for AIXV3 from Tom McConnell
 *            <tmcconne@sedona.intel.com>
 * 11-Nov-90: put into xlock by Steve Zellers <zellers@sun.com>
 * 16-Oct-90: Received from Tom Lawrence (tcl@cs.brown.edu: 'flight' simulator)
 */

#ifdef STANDALONE
#define PROGCLASS "Rotor"
#define HACK_INIT init_rotor
#define HACK_DRAW draw_rotor
#define rotor_opts xlockmore_opts
#define DEFAULTS "*delay: 500 \n" \
 "*count: 4 \n" \
 "*cycles: 100 \n" \
 "*size: -6 \n" \
 "*ncolors: 200 \n"
#define SMOOTH_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_rotor

ModeSpecOpt rotor_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   rotor_description =
{"rotor", "init_rotor", "draw_rotor", "release_rotor",
 "refresh_rotor", "init_rotor", NULL, &rotor_opts,
 500, 4, 100, -6, 64, 0.3, "",
 "Shows Tom's Roto-Rooter", 0, NULL};

#endif

/*-
 * A 'batchcount' of 3 or 4 works best!
 */

#define MAXANGLE	3000.0	/* irrectangular  (was 10000.0) */

/* How many segments to draw per cycle when redrawing */
#define REDRAWSTEP 3

typedef struct {
	float       angle;
	float       radius;
	float       start_radius;
	float       end_radius;
	float       radius_drift_max;
	float       radius_drift_now;

	float       ratio;
	float       start_ratio;
	float       end_ratio;
	float       ratio_drift_max;
	float       ratio_drift_now;
} elem;

typedef struct {
	int         pix;
	int         lastx, lasty;
	int         num, rotor, prev;
	int         nsave;
	float       angle;
	int         centerx, centery;
	int         prevcenterx, prevcentery;
	unsigned char firsttime;
	unsigned char iconifiedscreen;	/* for iconified view */
	unsigned char forward;
	unsigned char unused;
	elem       *elements;
	XPoint     *save;
	int         redrawing, redrawpos;
	int         linewidth;
} rotorstruct;

static rotorstruct *rotors = NULL;

void
init_rotor(ModeInfo * mi)
{
	rotorstruct *rp;
	int         x;
	elem       *pelem;
	unsigned char wasiconified;

	if (rotors == NULL) {
		if ((rotors = (rotorstruct *) calloc(MI_NUM_SCREENS(mi),
					      sizeof (rotorstruct))) == NULL)
			return;
	}
	rp = &rotors[MI_SCREEN(mi)];

	rp->prevcenterx = rp->centerx;
	rp->prevcentery = rp->centery;

	rp->centerx = MI_WIDTH(mi) / 2;
	rp->centery = MI_HEIGHT(mi) / 2;

	rp->redrawing = 0;
	/*
	 * sometimes, you go into iconified view, only to see a really whizzy pattern
	 * that you would like to look more closely at. Normally, clicking in the
	 * icon reinitializes everything - but I don't, cuz I'm that kind of guy.
	 * HENCE, the wasiconified stuff you see here.
	 */

	wasiconified = rp->iconifiedscreen;
	rp->iconifiedscreen = MI_IS_ICONIC(mi);

	if (wasiconified && !rp->iconifiedscreen)
		rp->firsttime = True;
	else {

		/* This is a fudge is needed since prevcenter may not be set when it comes
		   from the the random mode and return is pressed (and its not the first
		   mode that was running). This assumes that the size of the lock screen
		   window / size of the icon window = 12 */
		if (!rp->prevcenterx)
			rp->prevcenterx = rp->centerx * 12;
		if (!rp->prevcentery)
			rp->prevcentery = rp->centery * 12;

		rp->num = MI_COUNT(mi);
		if (rp->num < 0) {
			rp->num = NRAND(-rp->num) + 1;
			if (rp->elements != NULL) {
				(void) free((void *) rp->elements);
				rp->elements = NULL;
			}
		}
		if (rp->elements == NULL)
			rp->elements = (elem *) calloc(rp->num, sizeof (elem));
		rp->nsave = MI_CYCLES(mi);
		if (rp->nsave <= 1)
			rp->nsave = 2;
		if (rp->save == NULL)
			rp->save = (XPoint *) malloc(rp->nsave * sizeof (XPoint));
		for (x = 0; x < rp->nsave; x++) {
			rp->save[x].x = rp->centerx;
			rp->save[x].y = rp->centery;
		}

		pelem = rp->elements;

		for (x = rp->num; --x >= 0; pelem++) {
			pelem->radius_drift_max = 1.0;
			pelem->radius_drift_now = 1.0;

			pelem->end_radius = 100.0;

			pelem->ratio_drift_max = 1.0;
			pelem->ratio_drift_now = 1.0;
			pelem->end_ratio = 10.0;
		}
		if (MI_NPIXELS(mi) > 2)
			rp->pix = NRAND(MI_NPIXELS(mi));

		rp->rotor = 0;
		rp->prev = 1;
		rp->lastx = rp->centerx;
		rp->lasty = rp->centery;
		rp->angle = (float) NRAND((long) MAXANGLE) / 3.0;
		rp->forward = rp->firsttime = True;
	}
	rp->linewidth = MI_SIZE(mi);

	if (rp->linewidth == 0)
		rp->linewidth = 1;
	if (rp->linewidth < 0)
		rp->linewidth = NRAND(-rp->linewidth) + 1;

	MI_CLEARWINDOW(mi);
}

void
draw_rotor(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	register rotorstruct *rp = &rotors[MI_SCREEN(mi)];
	register elem *pelem;
	int         thisx, thisy;
	int         i;
	int         x_1, y_1, x_2, y_2;

	MI_IS_DRAWN(mi) = True;

	if (!rp->iconifiedscreen) {
		thisx = rp->centerx;
		thisy = rp->centery;
	} else {
		thisx = rp->prevcenterx;
		thisy = rp->prevcentery;
	}
	XSetLineAttributes(MI_DISPLAY(mi), MI_GC(mi), rp->linewidth,
			   LineSolid, CapButt, JoinMiter);
	for (i = rp->num, pelem = rp->elements; --i >= 0; pelem++) {
		if (pelem->radius_drift_max <= pelem->radius_drift_now) {
			pelem->start_radius = pelem->end_radius;
			pelem->end_radius = (float) NRAND(40000) / 100.0 - 200.0;
			pelem->radius_drift_max = (float) NRAND(100000) + 10000.0;
			pelem->radius_drift_now = 0.0;
		}
		if (pelem->ratio_drift_max <= pelem->ratio_drift_now) {
			pelem->start_ratio = pelem->end_ratio;
			pelem->end_ratio = (float) NRAND(2000) / 100.0 - 10.0;
			pelem->ratio_drift_max = (float) NRAND(100000) + 10000.0;
			pelem->ratio_drift_now = 0.0;
		}
		pelem->ratio = pelem->start_ratio +
			(pelem->end_ratio - pelem->start_ratio) /
			pelem->ratio_drift_max * pelem->ratio_drift_now;
		pelem->angle = rp->angle * pelem->ratio;
		pelem->radius = pelem->start_radius +
			(pelem->end_radius - pelem->start_radius) /
			pelem->radius_drift_max * pelem->radius_drift_now;

		thisx += (int) (COSF(pelem->angle) * pelem->radius);
		thisy += (int) (SINF(pelem->angle) * pelem->radius);

		pelem->ratio_drift_now += 1.0;
		pelem->radius_drift_now += 1.0;
	}
	if (rp->firsttime)
		rp->firsttime = False;
	else {
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));

		x_1 = (int) rp->save[rp->rotor].x;
		y_1 = (int) rp->save[rp->rotor].y;
		x_2 = (int) rp->save[rp->prev].x;
		y_2 = (int) rp->save[rp->prev].y;

		if (rp->iconifiedscreen) {
			x_1 = x_1 * rp->centerx / rp->prevcenterx;
			x_2 = x_2 * rp->centerx / rp->prevcenterx;
			y_1 = y_1 * rp->centery / rp->prevcentery;
			y_2 = y_2 * rp->centery / rp->prevcentery;
		}
		XDrawLine(display, MI_WINDOW(mi), gc, x_1, y_1, x_2, y_2);

		if (MI_NPIXELS(mi) > 2) {
			XSetForeground(display, gc, MI_PIXEL(mi, rp->pix));
			if (++rp->pix >= MI_NPIXELS(mi))
				rp->pix = 0;
		} else
			XSetForeground(display, gc, MI_WHITE_PIXEL(mi));

		x_1 = rp->lastx;
		y_1 = rp->lasty;
		x_2 = thisx;
		y_2 = thisy;

		if (rp->iconifiedscreen) {
			x_1 = x_1 * rp->centerx / rp->prevcenterx;
			x_2 = x_2 * rp->centerx / rp->prevcenterx;
			y_1 = y_1 * rp->centery / rp->prevcentery;
			y_2 = y_2 * rp->centery / rp->prevcentery;
		}
		XDrawLine(display, MI_WINDOW(mi), gc, x_1, y_1, x_2, y_2);
	}
	rp->save[rp->rotor].x = rp->lastx = thisx;
	rp->save[rp->rotor].y = rp->lasty = thisy;

	++rp->rotor;
	rp->rotor %= rp->nsave;
	++rp->prev;
	rp->prev %= rp->nsave;
	if (rp->forward) {
		rp->angle += 0.01;
		if (rp->angle >= MAXANGLE) {
			rp->angle = MAXANGLE;
			rp->forward = False;
		}
	} else {
		rp->angle -= 0.1;
		if (rp->angle <= 0) {
			rp->angle = 0.0;
			rp->forward = True;
		}
	}
	if (rp->redrawing) {
		int         j;

		for (i = 0; i < REDRAWSTEP; i++) {
			j = (rp->rotor - rp->redrawpos + rp->nsave) % rp->nsave;

			x_1 = (int) rp->save[j].x;
			y_1 = (int) rp->save[j].y;
			x_2 = (int) rp->save[(j - 1 + rp->nsave) % rp->nsave].x;
			y_2 = (int) rp->save[(j - 1 + rp->nsave) % rp->nsave].y;

			if (rp->iconifiedscreen) {
				x_1 = x_1 * rp->centerx / rp->prevcenterx;
				x_2 = x_2 * rp->centerx / rp->prevcenterx;
				y_1 = y_1 * rp->centery / rp->prevcentery;
				y_2 = y_2 * rp->centery / rp->prevcentery;
			}
			XDrawLine(display, MI_WINDOW(mi), gc, x_1, y_1, x_2, y_2);

			if (++(rp->redrawpos) >= rp->nsave) {
				rp->redrawing = 0;
				break;
			}
		}
	}
	XSetLineAttributes(MI_DISPLAY(mi), MI_GC(mi), 1,
			   LineSolid, CapButt, JoinMiter);
}

void
release_rotor(ModeInfo * mi)
{
	if (rotors != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			rotorstruct *rp = &rotors[screen];

			if (rp->elements != NULL)
				(void) free((void *) rp->elements);
			if (rp->save != NULL)
				(void) free((void *) rp->save);
		}
		(void) free((void *) rotors);
		rotors = NULL;
	}
}

void
refresh_rotor(ModeInfo * mi)
{
	rotorstruct *rp = &rotors[MI_SCREEN(mi)];

	MI_CLEARWINDOW(mi);
	rp->redrawing = 1;
	rp->redrawpos = 1;
}

#endif /* MODE_rotor */
