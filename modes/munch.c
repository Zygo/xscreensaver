/* -*- Mode: C; tab-width: 4 -*- */
/* munch --- munching squares */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)munch.c 4.07 97/11/24 xlockmore";

#endif

/*-
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
 * Tim Showalter <tjs@andrew.cmu.edu>
 * 
 * Copyright 1997, Tim Showalter
 * Permission is granted to copy, modify, and use this as long
 * as this notice remains intact.  No warranties are expressed or implied.
 * CMU Sucks.
 * 
 * Some code stolen from / This is meant to work with
 * xscreensaver, Copyright (c) 1992, 1995, 1996
 * Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/*-
 * Munching Squares is this simplistic, silly screen hack (according to
 * HAKMEM, discovered by Jackson Wright in 1962) where you take Y = X XOR T
 * and graph it over and over.  According to HAKMEM, it  takes 5 instructions
 * of PDP-1 assembly.  This is a little more complicated than that, mostly X's 
 *  fault, but it does some other random things.
 * http://www.inwap.com/pdp10/hbaker/hakmem/hacks.html#item146
 */

#ifdef STANDALONE
#define PROGCLASS "Munch"
#define HACK_INIT init_munch
#define HACK_DRAW draw_munch
#define munch_opts xlockmore_opts
#define DEFAULTS "*delay: 5000 \n" \
 "*cycles: 7 \n"
#include "xlockmore.h"		/* from the xscreensaver distribution */
#else /* !STANDALONE */
#include "xlock.h"		/* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef MODE_munch

ModeSpecOpt munch_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   munch_description =
{"munch", "init_munch", "draw_munch", "release_munch",
 "init_munch", "init_munch", NULL, &munch_opts,
 5000, 1, 7, 1, 64, 1.0, "",
 "Shows munching squares", 0, NULL};

#endif

#if 0
#include <assert.h>
#endif

/* flags for random things.  Must be < log2(random's maximum), incidentially. */
#define SHIFT_KX (0x01)
#define SHIFT_KT (0x02)
#define SHIFT_KY (0x04)
#define GRAV     (0x08)

typedef struct {
	int         width, height;
	int         logminwidth;
	int         logmaxwidth;
	GC          gc;
	int         thiswidth;
	int         t;
	int         atX, atY;
	int         kX, kT, kY;
	int         grav;
} munchstruct;

static munchstruct *munches = NULL;

static void
munchBit(ModeInfo * mi, int width,	/* pixels */
	 int atX, int atY,	/* pixels */
	 int kX, int kT, int kY,	/* pixels */
	 int grav /* 0 or not */ )
{
	munchstruct *mp = &munches[MI_SCREEN(mi)];

	int         x, y;
	int         drawX, drawY;

#if 0
	(void) fprintf(stderr, "Doing width %d at %d %d shift %d %d %d grav %d\n",
		       width, atX, atY, kX, kT, kY, grav);
#endif

	for (x = 0; x < width; x++) {
		/* figure out the next point */
		y = ((x ^ ((mp->t + kT) % width)) + kY) % width;

		drawX = ((x + kX) % width) + atX;
		drawY = (grav ? y + atY : atY + width - 1 - y);

		/* used to be bugs where it would draw partially offscreen.
		   while that might be a pretty feature, I didn't want it to do
		   that yet.  if these trigger, please let me know.
		 */
#if 0
		assert(drawX >= 0 && drawX < MI_WIDTH(mi));
		assert(drawY >= 0 && drawY < MI_HEIGHT(mi));
#endif

		XDrawPoint(MI_DISPLAY(mi), MI_WINDOW(mi), mp->gc, drawX, drawY);
		/* XXX may want to change this to XDrawPoints,
		   but it's fast enough without it for the moment. */

	}
}

/* 
 * dumb way to get # of digits in number.  Probably faster than actually
 * doing a log and a division, maybe.
 */
static int
dumb_log_2(int k)
{
	int         r = -1;

	while (k > 0) {
		k >>= 1;
		r++;
	}
	return r;
}

void
init_munch(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	munchstruct *mp;

	if (munches == NULL) {
		if ((munches = (munchstruct *) calloc(MI_NUM_SCREENS(mi),
					      sizeof (munchstruct))) == NULL)
			return;
	}
	mp = &munches[MI_SCREEN(mi)];

	if (!mp->gc) {
		if ((mp->gc = XCreateGC(display, MI_WINDOW(mi),
			     (unsigned long) 0, (XGCValues *) NULL)) == None)
			return;
	}
	mp->width = MI_WIDTH(mi);
	mp->height = MI_HEIGHT(mi);

	/* We need a square; limit on screen size? */
	/* we want a power of 2 for the width or the munch doesn't fill up. */
	mp->logmaxwidth = (int)
		dumb_log_2((mp->height < mp->width) ? mp->height : mp->width);

	XSetFunction(display, mp->gc, GXxor);

	mp->logminwidth = MI_CYCLES(mi);
	if (mp->logminwidth < 2 || MI_IS_ICONIC(mi))
		mp->logminwidth = 2;

	if (mp->logmaxwidth < mp->logminwidth)
		mp->logmaxwidth = mp->logminwidth;

	mp->t = 0;

	MI_CLEARWINDOW(mi);
}

void
draw_munch(ModeInfo * mi)
{
	munchstruct *mp = &munches[MI_SCREEN(mi)];

	MI_IS_DRAWN(mi) = True;

	if (!mp->t) {		/* New one */
		int         randflags = (int) LRAND();

		/* choose size -- power of two */
		mp->thiswidth = (int) (1 << (mp->logminwidth +
		       (LRAND() % (1 + mp->logmaxwidth - mp->logminwidth))));

		if (MI_NPIXELS(mi) > 2)
			XSetForeground(MI_DISPLAY(mi), mp->gc,
				       MI_PIXEL(mi, NRAND(MI_NPIXELS(mi))));
		else		/* Xor'red so WHITE may not be appropriate */
			XSetForeground(MI_DISPLAY(mi), mp->gc, 1);

		mp->atX = (int) (LRAND() %
				 ((mp->width <= mp->thiswidth) ? 1 : mp->width - mp->thiswidth));
		mp->atY = (int) (LRAND() %
				 ((mp->height <= mp->thiswidth) ? 1 : mp->height - mp->thiswidth));

		/* wrap-around by these values; no need to %
		   as we end up doing that later anyway */
		mp->kX = (int) ((randflags & SHIFT_KX) ? LRAND() % mp->thiswidth : 0);
		mp->kT = (int) ((randflags & SHIFT_KT) ? LRAND() % mp->thiswidth : 0);
		mp->kY = (int) ((randflags & SHIFT_KY) ? LRAND() % mp->thiswidth : 0);

		/* set the gravity of the munch, or rather,
		   which direction we draw stuff in. */
		mp->grav = randflags & GRAV;
	}
	/* Finally draw this munching square. */
	munchBit(mi,
		 mp->thiswidth,	/* Width, in pixels */
	/* draw at this location */
		 mp->atX, mp->atY, mp->kX, mp->kT, mp->kY, mp->grav);

	mp->t++;
	if (mp->t == mp->thiswidth)
		mp->t = 0;
}

void
release_munch(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);

	MI_CLEARWINDOW(mi);

	if (munches != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			munchstruct *mp = &munches[screen];

			if (mp->gc != NULL)
				XFreeGC(display, mp->gc);
		}

		(void) free((void *) munches);
		munches = NULL;
	}
}

#endif /* MODE_munch */
