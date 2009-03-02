
/* -*- Mode: C; tab-width: 4 -*- */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)helix.c	4.03 97/05/10 xlockmore";

#endif

/*-
 * helix.c - string art for xlock, the X Window System lockscreen.
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
 * 06-Apr-97: new ellipse code from Dan Stromberg <strombrg@nis.acs.uci.edu>
 * 11-Aug-95: found some typos, looks more interesting now
 * 08-Aug-95: speed up thanks to Heath A. Kehoe <hakehoe@icaen.uiowa.edu>
 * 17-Jun-95: removed sleep statements
 * 2-Sep-93: xlock version David Bagley <bagleyd@bigfoot.com>
 * 1992:     xscreensaver version Jamie Zawinski <jwz@netscape.com>
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
#define PROGCLASS            "Helix"
#define HACK_INIT            init_helix
#define HACK_DRAW            draw_helix
#define DEF_DELAY            25000
#define DEF_CYCLES           100
#define DEF_NCOLORS          200
#define SPREAD_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */

#endif /* STANDALONE */

#define ANGLES 360
#define DEF_ELLIPSE "False"

static Bool ellipse;

#ifndef STANDALONE
static XrmOptionDescRec opts[] =
{
	{"-ellipse", ".helix.ellipse", XrmoptionNoArg, (caddr_t) "on"},
	{"+ellipse", ".helix.ellipse", XrmoptionNoArg, (caddr_t) "off"}
};
static argtype vars[] =
{
	{(caddr_t *) & ellipse, "ellipse", "Ellipse", DEF_ELLIPSE, t_Bool}
};
static OptionStruct desc[] =
{
	{"-/+ellipse", "turn on/off ellipse format"}
};

ModeSpecOpt helix_opts =
{2, opts, 1, vars, desc};

#endif /* STANDALONE */

static double cos_array[ANGLES], sin_array[ANGLES];
typedef struct {
	int         width;
	int         height;
	int         xmid, ymid;
	int         color;
	int         time;
	int         radius1, radius2, d_angle, factor1, factor2, factor3,
	            factor4;
	int         redraw;

	Bool        ellipse;
	int         d_angle_offset, dir;
	int         offset;
	int         density;
	int         count;
} helixstruct;

static helixstruct *helixes = NULL;

static int
gcd(int a, int b)
{
	while (b > 0) {
		int         tmp;

		tmp = a % b;
		a = b;
		b = tmp;
	}
	return (a < 0 ? -a : a);
}

static void
helix(ModeInfo * mi, int radius1, int radius2, int d_angle,
      int factor1, int factor2, int factor3, int factor4)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	helixstruct *hp = &helixes[MI_SCREEN(mi)];
	int         x_1, y_1, x_2, y_2, angle, limit;
	int         i;

	x_1 = hp->xmid;
	y_1 = hp->ymid + radius2;
	x_2 = hp->xmid;
	y_2 = hp->ymid + radius1;
	angle = 0;
	limit = 1 + (ANGLES / gcd(ANGLES, d_angle));

	for (i = 0; i < limit; i++) {
		int         tmp;

#define pmod(x,y) (tmp=((x)%(y)),(tmp>=0?tmp:tmp+y))
		x_1 = hp->xmid + (int) (((double) radius1) *
				 sin_array[pmod((angle * factor1), ANGLES)]);
		y_1 = hp->ymid + (int) (((double) radius2) *
				 cos_array[pmod((angle * factor2), ANGLES)]);
		if (MI_NPIXELS(mi) > 2) {
			XSetForeground(display, gc, MI_PIXEL(mi, hp->color));
			if (++hp->color >= MI_NPIXELS(mi))
				hp->color = 0;
		}
		XDrawLine(display, window, gc, x_1, y_1, x_2, y_2);
		x_2 = hp->xmid + (int) (((double) radius2) *
				 sin_array[pmod((angle * factor3), ANGLES)]);
		y_2 = hp->ymid + (int) (((double) radius1) *
				 cos_array[pmod((angle * factor4), ANGLES)]);
		if (MI_NPIXELS(mi) > 2) {
			XSetForeground(display, gc, MI_PIXEL(mi, hp->color));
			if (++hp->color >= MI_NPIXELS(mi))
				hp->color = 0;
		}
		XDrawLine(display, window, gc, x_1, y_1, x_2, y_2);
		angle += d_angle;
	}
}

static void
trig(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	helixstruct *hp = &helixes[MI_SCREEN(mi)];
	int         x1, y1, x2, y2;
	int         tmp, angle;


#define pmod(x,y) (tmp=((x)%(y)),(tmp>=0?tmp:tmp+y))

	while (ABS(hp->d_angle) <= ANGLES) {
		angle = hp->d_angle + hp->d_angle_offset;
		x1 = (int) (sin_array[pmod(angle * hp->factor1, ANGLES)] * hp->xmid) +
			hp->xmid;
		y1 = (int) (cos_array[pmod(angle * hp->factor1, ANGLES)] * hp->ymid) +
			hp->ymid;
		x2 = (int) (sin_array[pmod(angle * hp->factor2 + hp->offset, ANGLES)] *
			    hp->xmid) + hp->xmid;
		y2 = (int) (cos_array[pmod(angle * hp->factor2 + hp->offset, ANGLES)] *
			    hp->ymid) + hp->ymid;
		XDrawLine(display, MI_WINDOW(mi), gc, x1, y1, x2, y2);
		if (MI_NPIXELS(mi) > 2) {
			XSetForeground(display, gc, MI_PIXEL(mi, hp->color));
			if (++hp->color >= MI_NPIXELS(mi))
				hp->color = 0;
		}
		tmp = (int) ANGLES / (2 * hp->density * hp->factor1 * hp->factor2);
		if (tmp == 0)	/* Do not want it getting stuck... */
			tmp = 1;	/* Would not need if floating point */
		hp->d_angle += hp->dir * tmp;
	}
}

static void
random_helix(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	helixstruct *hp = &helixes[MI_SCREEN(mi)];
	int         radius;
	double      divisor;

	radius = MIN(hp->xmid, hp->ymid);

	hp->d_angle = 0;
	hp->factor1 = 2;
	hp->factor2 = 2;
	hp->factor3 = 2;
	hp->factor4 = 2;

	divisor = ((LRAND() / MAXRAND * 3.0 + 1) * (((LRAND() & 1) * 2) - 1));

	if ((LRAND() & 1) == 0) {
		hp->radius1 = radius;
		hp->radius2 = (int) ((double) radius / divisor);
	} else {
		hp->radius2 = radius;
		hp->radius1 = (int) ((double) radius / divisor);
	}

	while (gcd(ANGLES, hp->d_angle) >= 2)
		hp->d_angle = NRAND(ANGLES);

#define random_factor()				\
  (((NRAND(7)) ? ((LRAND() & 1) + 1) : 3)	* (((LRAND() & 1) * 2) - 1))

	while (gcd(gcd(gcd(hp->factor1, hp->factor2), hp->factor3), hp->factor4)
	       != 1) {
		hp->factor1 = random_factor();
		hp->factor2 = random_factor();
		hp->factor3 = random_factor();
		hp->factor4 = random_factor();
	}
	if (MI_NPIXELS(mi) > 2) {
		XSetForeground(display, gc, MI_PIXEL(mi, hp->color));
		if (++hp->color >= MI_NPIXELS(mi))
			hp->color = 0;
	} else
		XSetForeground(display, gc, MI_WIN_WHITE_PIXEL(mi));

	helix(mi, hp->radius1, hp->radius2, hp->d_angle,
	      hp->factor1, hp->factor2, hp->factor3, hp->factor4);
}

static void
random_trig(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	helixstruct *hp = &helixes[MI_SCREEN(mi)];

	hp->d_angle = 0;
	hp->factor1 = NRAND(8) + 1;
	do
		hp->factor2 = NRAND(8) + 1;
	while (hp->factor1 == hp->factor2);
	hp->dir = (LRAND() & 1) ? 1 : -1;
	hp->d_angle_offset = NRAND(ANGLES);
	hp->offset = (NRAND(ANGLES / 4 - 1) + 1) / 4;
	hp->density = 1 << (NRAND(4) + 4);	/* Higher density, higher ANGLES */
	if (MI_NPIXELS(mi) > 2) {
		XSetForeground(display, gc, MI_PIXEL(mi, hp->color));
		if (++hp->color >= MI_NPIXELS(mi))
			hp->color = 0;
	} else
		XSetForeground(display, gc, MI_WIN_WHITE_PIXEL(mi));
	trig(mi);
}

void
init_helix(ModeInfo * mi)
{
	helixstruct *hp;
	int         i;
	static int  first = 1;

	if (helixes == NULL) {
		if ((helixes = (helixstruct *) calloc(MI_NUM_SCREENS(mi),
					      sizeof (helixstruct))) == NULL)
			return;
	}
	hp = &helixes[MI_SCREEN(mi)];

	if (first) {
		first = 0;
		for (i = 0; i < ANGLES; i++) {
			cos_array[i] = cos((((double) i) / (double) (ANGLES / 2)) * M_PI);
			sin_array[i] = sin((((double) i) / (double) (ANGLES / 2)) * M_PI);;
		}
	}
	hp->ellipse = ellipse;
	if (MI_WIN_IS_FULLRANDOM(mi))
		hp->ellipse = (Bool) (!NRAND(5));	/* 1:5 chance of running ellipse stuff */

	hp->width = MI_WIN_WIDTH(mi);
	hp->height = MI_WIN_HEIGHT(mi);
	hp->xmid = hp->width / 2;
	hp->ymid = hp->height / 2;
	hp->redraw = 0;
	XClearWindow(MI_DISPLAY(mi), MI_WINDOW(mi));

	if (MI_NPIXELS(mi) > 2)
		hp->color = NRAND(MI_NPIXELS(mi));
	hp->time = 0;

	if (hp->ellipse) {
		random_trig(mi);
	} else
		random_helix(mi);
}

void
draw_helix(ModeInfo * mi)
{
	helixstruct *hp = &helixes[MI_SCREEN(mi)];

	if (++hp->time > MI_CYCLES(mi))
		init_helix(mi);
	if (hp->redraw) {
		if (hp->ellipse) {
			trig(mi);
		} else {
			helix(mi, hp->radius1, hp->radius2, hp->d_angle,
			 hp->factor1, hp->factor2, hp->factor3, hp->factor4);
		}
		hp->redraw = 0;
	}
}

void
release_helix(ModeInfo * mi)
{
	if (helixes != NULL) {
		(void) free((void *) helixes);
		helixes = NULL;
	}
}

void
refresh_helix(ModeInfo * mi)
{
	helixstruct *hp = &helixes[MI_SCREEN(mi)];

	hp->redraw = 1;
}
