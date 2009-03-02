/* -*- Mode: C; tab-width: 4 -*- */
/* lissie --- the Lissajous worm */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)lissie.c	5.00 2000/11/01 xlockmore";

#endif

/*-
 * lissie.c - The Lissajous worm for xlock, the X Window System
 *               lockscreen.
 *
 * Copyright (c) 1996 by Alexander Jolk <ub9x@rz.uni-karlsruhe.de>
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
 * 18-Aug-1996: added refresh-hook.
 * 01-May-1996: written.
 */

#ifdef STANDALONE
#define PROGCLASS "Lissie"
#define HACK_INIT init_lissie
#define HACK_DRAW draw_lissie
#define lissie_opts xlockmore_opts
#define DEFAULTS "*delay: 10000 \n" \
 "*count: 1 \n" \
 "*cycles: 20000 \n" \
 "*size: -200 \n" \
 "*ncolors: 200 \n"
#define SMOOTH_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_lissie

ModeSpecOpt lissie_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   lissie_description =
{"lissie", "init_lissie", "draw_lissie", "release_lissie",
 "refresh_lissie", "init_lissie", NULL, &lissie_opts,
 10000, 1, 2000, -200, 64, 0.6, "",
 "Shows lissajous worms", 0, NULL};

#endif

#define MINSIZE 1

#define Lissie(n)\
	if (lissie->loc[(n)].x > 0 && lissie->loc[(n)].y > 0 &&\
		lissie->loc[(n)].x <= lp->width && lissie->loc[(n)].y <= lp->height) {\
		if (lissie->ri < 2)\
			XDrawPoint(display, MI_WINDOW(mi),\
				gc, lissie->loc[(n)].x, lissie->loc[(n)].y);\
		else\
			XDrawArc(display, MI_WINDOW(mi), gc,\
				lissie->loc[(n)].x - lissie->ri / 2,\
				lissie->loc[(n)].y - lissie->ri / 2,\
				lissie->ri, lissie->ri, 0, 23040);\
	}

#define FLOATRAND(min,max)	((min)+(LRAND()/MAXRAND)*((max)-(min)))
#define INTRAND(min,max)     ((min)+NRAND((max)-(min)+1))

#define MINDT  0.01
#define MAXDT  0.15

#define MAXLISSIELEN  100
#define MINLISSIELEN  10
#define MINLISSIES 1

/* How many segments to draw per cycle when redrawing */
#define REDRAWSTEP 3

typedef struct {
	double      tx, ty, dtx, dty;
	int         xi, yi, ri, rx, ry, len, pos;
	int         redrawing, redrawpos;
	XPoint      loc[MAXLISSIELEN];
	unsigned long color;
} lissiestruct;

typedef struct {
	Bool        painted;
	int         width, height;
	int         nlissies;
	lissiestruct *lissie;
	int         loopcount;
} lissstruct;

static lissstruct *lisses = NULL;


static void
drawlissie(ModeInfo * mi, lissiestruct * lissie)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	lissstruct *lp = &lisses[MI_SCREEN(mi)];
	int         p = (++lissie->pos) % MAXLISSIELEN;
	int         oldp = (lissie->pos - lissie->len + MAXLISSIELEN) % MAXLISSIELEN;

	/* Let time go by ... */
	lissie->tx += lissie->dtx;
	lissie->ty += lissie->dty;
	if (lissie->tx > 2 * M_PI)
		lissie->tx -= 2 * M_PI;
	if (lissie->ty > 2 * M_PI)
		lissie->ty -= 2 * M_PI;

	/* vary both (x/y) speeds by max. 1% */
	lissie->dtx *= FLOATRAND(0.99, 1.01);
	lissie->dty *= FLOATRAND(0.99, 1.01);
	if (lissie->dtx < MINDT)
		lissie->dtx = MINDT;
	else if (lissie->dtx > MAXDT)
		lissie->dtx = MAXDT;
	if (lissie->dty < MINDT)
		lissie->dty = MINDT;
	else if (lissie->dty > MAXDT)
		lissie->dty = MAXDT;

	lissie->loc[p].x = lissie->xi + (int) (sin(lissie->tx) * lissie->rx);
	lissie->loc[p].y = lissie->yi + (int) (sin(lissie->ty) * lissie->ry);

	/* Mask */
	XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
	Lissie(oldp);

	/* Redraw */
	if (MI_NPIXELS(mi) > 2) {
		XSetForeground(display, gc, MI_PIXEL(mi, lissie->color));
		if (++lissie->color >= (unsigned) MI_NPIXELS(mi))
			lissie->color = 0;
	} else
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
	Lissie(p);
	if (lissie->redrawing) {
		int         i;

		lissie->redrawpos++;
		/* This compensates for the changed p
		   since the last callback. */

		for (i = 0; i < REDRAWSTEP; i++) {
			Lissie((p - lissie->redrawpos + MAXLISSIELEN) % MAXLISSIELEN);
			if (++(lissie->redrawpos) >= lissie->len) {
				lissie->redrawing = 0;
				break;
			}
		}
	}
}

static void
initlissie(ModeInfo * mi, lissiestruct * lissie)
{
	lissstruct *lp = &lisses[MI_SCREEN(mi)];
	int         size = MI_SIZE(mi);
	int         i;

	if (MI_NPIXELS(mi) > 2)
		lissie->color = NRAND(MI_NPIXELS(mi));
	else
		lissie->color = MI_WHITE_PIXEL(mi);
	/* Initialize parameters */
	if (size < -MINSIZE)
		lissie->ri = NRAND(MIN(-size, MAX(MINSIZE,
		   MIN(lp->width, lp->height) / 4)) - MINSIZE + 1) + MINSIZE;
	else if (size < MINSIZE) {
		if (!size)
			lissie->ri = MAX(MINSIZE, MIN(lp->width, lp->height) / 4);
		else
			lissie->ri = MINSIZE;
	} else
		lissie->ri = MIN(size, MAX(MINSIZE, MIN(lp->width, lp->height) / 4));
	lissie->xi = INTRAND(lp->width / 4 + lissie->ri,
			     lp->width * 3 / 4 - lissie->ri);
	lissie->yi = INTRAND(lp->height / 4 + lissie->ri,
			     lp->height * 3 / 4 - lissie->ri);
	lissie->rx = INTRAND(lp->width / 4,
		   MIN(lp->width - lissie->xi, lissie->xi)) - 2 * lissie->ri;
	lissie->ry = INTRAND(lp->height / 4,
		  MIN(lp->height - lissie->yi, lissie->yi)) - 2 * lissie->ri;
	lissie->len = INTRAND(MINLISSIELEN, MAXLISSIELEN - 1);
	lissie->pos = 0;

	lissie->redrawing = 0;

	lissie->tx = FLOATRAND(0, 2 * M_PI);
	lissie->ty = FLOATRAND(0, 2 * M_PI);
	lissie->dtx = FLOATRAND(MINDT, MAXDT);
	lissie->dty = FLOATRAND(MINDT, MAXDT);

	for (i = 0; i < MAXLISSIELEN; i++)
		lissie->loc[i].x = lissie->loc[i].y = 0;
	/* Draw lissie */
	drawlissie(mi, lissie);
}

void
init_lissie(ModeInfo * mi)
{
	lissstruct *lp;
	unsigned char ball;

	if (lisses == NULL) {
		if ((lisses = (lissstruct *) calloc(MI_NUM_SCREENS(mi),
					       sizeof (lissstruct))) == NULL)
			return;
	}
	lp = &lisses[MI_SCREEN(mi)];

	lp->width = MI_WIDTH(mi);
	lp->height = MI_HEIGHT(mi);

	lp->nlissies = MI_COUNT(mi);
	if (lp->nlissies < -MINLISSIES) {
		if (lp->lissie) {
			(void) free((void *) lp->lissie);
			lp->lissie = NULL;
		}
		lp->nlissies = NRAND(-lp->nlissies - MINLISSIES + 1) + MINLISSIES;
	} else if (lp->nlissies < MINLISSIES)
		lp->nlissies = MINLISSIES;

	lp->loopcount = 0;

	if (lp->lissie == NULL)
		if ((lp->lissie = (lissiestruct *) calloc(lp->nlissies,
				sizeof (lissiestruct))) == NULL)
			return;

	MI_CLEARWINDOW(mi);
	lp->painted = False;

	for (ball = 0; ball < (unsigned char) lp->nlissies; ball++)
		initlissie(mi, &lp->lissie[ball]);

}

void
draw_lissie(ModeInfo * mi)
{
	register unsigned char ball;
	lissstruct *lp;

	if (lisses == NULL)
		return;
	lp = &lisses[MI_SCREEN(mi)];
	if (lp->lissie == NULL)
		return;

	MI_IS_DRAWN(mi) = True;

	if (++lp->loopcount > MI_CYCLES(mi)) {
		init_lissie(mi);
	} else {
		lp->painted = True;
		for (ball = 0; ball < (unsigned char) lp->nlissies; ball++)
			drawlissie(mi, &lp->lissie[ball]);
	}
}

void
release_lissie(ModeInfo * mi)
{
	if (lisses != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			lissstruct *lp = &lisses[screen];

			if (lp->lissie != NULL) {
				(void) free((void *) lp->lissie);
				/* lp->lissie = NULL; */
			}
		}
		(void) free((void *) lisses);
		lisses = NULL;
	}
}

void
refresh_lissie(ModeInfo * mi)
{
	int         i;
	lissstruct *lp;

	if (lisses == NULL)
		return;
	lp = &lisses[MI_SCREEN(mi)];
	if (lp->lissie == NULL)
		return;

	if (lp->painted) {
		MI_CLEARWINDOW(mi);
		for (i = 0; i < lp->nlissies; i++) {
			lp->lissie[i].redrawing = 1;
			lp->lissie[i].redrawpos = 0;
		}
	}
}

#endif /* MODE_lissie */
