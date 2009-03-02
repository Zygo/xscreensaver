/* -*- Mode: C; tab-width: 4 -*- */
/* coral --- a coral reef */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)coral.c 4.07 97/11/24 xlockmore";

#endif
/* 
 * Copyright (c) 1997 by "Frederick G.M. Roeber" <roeber@netscape.com>
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
 * 29-Oct-97: xlock version (David Bagley <bagleyd@bigfoot.com>)
 * 15-Jul-97: xscreensaver version Frederick G.M. Roeber <roeber@netscape.com>
 */

/*-
 * original copyright
 * Copyright (c) 1997 by "Frederick G.M. Roeber" <roeber@netscape.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifdef STANDALONE
#define PROGCLASS "Coral"
#define HACK_INIT init_coral
#define HACK_DRAW draw_coral
#define coral_opts xlockmore_opts
#define DEFAULTS "*delay: 60000 \n" \
 "*batchcount: -3 \n" \
 "*size: 35 \n" \
 "*ncolors: 200 \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

ModeSpecOpt coral_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   coral_description =
{"coral", "init_coral", "draw_coral", "release_coral",
 "init_coral", "init_coral", NULL, &coral_opts,
 60000, -3, 1, 35, 64, 0.6, "",
 "Shows a coral reef", 0, NULL};

#endif

#define MINSIZE 1
#define MINSEEDS 1
#define MAXSIZE 99
#define MAXPOINTS 200
#define COLORTHRESH 512

typedef struct {
	unsigned int default_fg_pixel;

	XPoint     *walkers;
	XPoint     *pointbuf;
	unsigned int *reef;
	int         nwalkers;
	int         width, widthb;
	int         height;
	int         colorindex;
	int         density;
	int         seeds;
} coralstruct;

static coralstruct *reefs = NULL;

#define getdot(x,y) (cp->reef[(y*cp->widthb)+(x>>5)] &  (1<<(x & 31)))
/* Avoid array bounds reads and writes */
#define setdot(x,y) (cp->reef[((y*cp->widthb)+(x>>5) >= 0 ? (y*cp->widthb)+(x>>5) : 0)] |= (1<<(x & 31)))

/*-
 * returns 2 bits of randomness (conserving calls to LRAND()).
 * This speeds things up a little, but not a lot (5-10% or so.)
 */
static int
rand_2(void)
{
	static int  i = 0;
	static int  r = 0;

	if (i) {
		i--;
	} else {
		i = 15;
		r = LRAND();
	}

	{
		register int j = (r & 3);

		r = r >> 2;
		return j;
	}
}


void
init_coral(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	coralstruct *cp;
	int         size = MI_SIZE(mi);

	int         i;

	if (reefs == NULL) {
		if ((reefs = (coralstruct *) calloc(MI_NUM_SCREENS(mi),
					      sizeof (coralstruct))) == NULL)
			return;
	}
	cp = &reefs[MI_SCREEN(mi)];


	cp->width = MAX(MI_WIDTH(mi), 2);
	cp->height = MAX(MI_HEIGHT(mi), 2);

	cp->widthb = ((cp->width + 31) >> 5);

	if (cp->reef) {
		(void) free((void *) cp->reef);
		cp->reef = NULL;
	}
	cp->reef = (unsigned int *) calloc((cp->widthb + 1) * cp->height,
					   sizeof (unsigned int));

	if (!cp->reef)
		return;

	if (size < -MINSIZE)
		cp->density = NRAND(MIN(MAXSIZE, -size) - MINSIZE + 1) + MINSIZE;
	else if (size < MINSIZE)
		cp->density = MINSIZE;
	else
		cp->density = MIN(MAXSIZE, size);

	cp->nwalkers = (cp->width * cp->height * cp->density) / 100;
	if (cp->walkers) {
		(void) free((void *) cp->walkers);
		cp->walkers = NULL;
	}
	cp->walkers = (XPoint *) calloc(cp->nwalkers, sizeof (XPoint));
	if (!cp->walkers)
		return;

	cp->seeds = MI_COUNT(mi);
	if (cp->seeds < -MINSEEDS)
		cp->seeds = NRAND(-cp->seeds - MINSEEDS + 1) + MINSEEDS;
	else if (cp->seeds < MINSEEDS)
		cp->seeds = MINSEEDS;

	MI_CLEARWINDOW(mi);

	if (MI_NPIXELS(mi) > 2) {
		cp->colorindex = NRAND(MI_NPIXELS(mi) * COLORTHRESH);
		XSetForeground(display, gc, MI_PIXEL(mi, cp->colorindex / COLORTHRESH));
	} else
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));

	for (i = 0; i < cp->seeds; i++) {
		int         x, y;

		do {
			x = NRAND(cp->width);
			y = NRAND(cp->height);
		} while (getdot(x, y));

		setdot((x - 1), (y - 1));
		setdot(x, (y - 1));
		setdot((x + 1), (y - 1));
		setdot((x - 1), y);
		setdot(x, y);
		setdot((x + 1), y);
		setdot((x - 1), (y + 1));
		setdot(x, (y + 1));
		setdot((x + 1), (y + 1));
		XDrawPoint(display, window, gc, x, y);
	}

	for (i = 0; i < cp->nwalkers; i++) {
		cp->walkers[i].x = NRAND(cp->width - 2) + 1;
		cp->walkers[i].y = NRAND(cp->height - 2) + 1;
	}
	if (cp->pointbuf) {
		(void) free((void *) cp->pointbuf);
		cp->pointbuf = NULL;
	}
	cp->pointbuf = (XPoint *) calloc((MAXPOINTS + 2), sizeof (XPoint));
	if (!cp->pointbuf)
		return;
}


void
draw_coral(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	coralstruct *cp = &reefs[MI_SCREEN(mi)];

	int         npoints = 0;


	int         i;

	for (i = 0; i < cp->nwalkers; i++) {
		int         x = cp->walkers[i].x;
		int         y = cp->walkers[i].y;

		if (getdot(x, y)) {

			/* XDrawPoint(display, window, gc, x, y); */
			cp->pointbuf[npoints].x = x;
			cp->pointbuf[npoints].y = y;
			npoints++;

			/* Mark the surrounding area as "sticky" */
			setdot((x - 1), (y - 1));
			setdot(x, (y - 1));
			setdot((x + 1), (y - 1));
			setdot((x - 1), y);
			setdot((x + 1), y);
			setdot((x - 1), (y + 1));
			setdot(x, (y + 1));
			setdot((x + 1), (y + 1));
			cp->nwalkers--;
			cp->walkers[i].x = cp->walkers[cp->nwalkers].x;
			cp->walkers[i].y = cp->walkers[cp->nwalkers].y;

			if (0 == cp->nwalkers || npoints >= MAXPOINTS) {
				XDrawPoints(display, window, gc, cp->pointbuf, npoints,
					    CoordModeOrigin);
				npoints = 0;
			}
			if (MI_NPIXELS(mi) > 2) {
				XSetForeground(display, gc, MI_PIXEL(mi, cp->colorindex / COLORTHRESH));
				if (++cp->colorindex >= MI_NPIXELS(mi) * COLORTHRESH)
					cp->colorindex = 0;
			} else
				XSetForeground(display, gc, MI_WHITE_PIXEL(mi));

			if (0 == cp->nwalkers) {
				if (cp->pointbuf)
					(void) free((void *) cp->pointbuf);
				cp->pointbuf = 0;
				init_coral(mi);
				return;
			}
		} else {
			/* move it a notch */
			switch (rand_2()) {
				case 0:
					if (1 == x)
						continue;
					cp->walkers[i].x--;
					break;
				case 1:
					if (cp->width - 2 == x)
						continue;
					cp->walkers[i].x++;
					break;
				case 2:
					if (1 == y)
						continue;
					cp->walkers[i].y--;
					break;
				default:	/* case 3: */
					if (cp->height - 2 == y)
						continue;
					cp->walkers[i].y++;
					break;
			}
		}
	}

	if (npoints > 0) {
		XDrawPoints(display, window, gc, cp->pointbuf, npoints,
			    CoordModeOrigin);
		npoints = 0;
	}
}

void
release_coral(ModeInfo * mi)
{
	if (reefs != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			coralstruct *cp = &reefs[screen];

			if (cp->reef != NULL)
				(void) free((void *) cp->reef);
			if (cp->pointbuf != NULL)
				(void) free((void *) cp->pointbuf);
			if (cp->walkers != NULL)
				(void) free((void *) cp->walkers);
		}
		(void) free((void *) reefs);
		reefs = NULL;
	}
}
