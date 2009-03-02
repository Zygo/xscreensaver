/* -*- Mode: C; tab-width: 4 -*- */
/* discrete --- chaotic mappings */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)discrete.c	5.00 2000/11/01 xlockmore";

#endif

/*-
 * Copyright (c) 1996 by Tim Auckland <Tim.Auckland@Procket.com>
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
 * "discrete" shows a number of fractals based on the "discrete map"
 * type of dynamical systems.  They include a different way of looking
 * at the HOPALONG system, an inverse julia-set iteration, the "Standard
 * Map" and the "Bird in a Thornbush" fractal.
 *
 * Revision History:
 * 01-Nov-2000: Allocation checks
 * 31-Jul-1997: Ported to xlockmore-4
 * 08-Aug-1996: Adapted from hop.c Copyright (c) 1991 by Patrick J. Naughton.
 */

#ifdef STANDALONE
#define PROGCLASS "Discrete"
#define HACK_INIT init_discrete
#define HACK_DRAW draw_discrete
#define discrete_opts xlockmore_opts
#define DEFAULTS "*delay: 1000 \n" \
 "*count: 4096 \n" \
 "*cycles: 2500 \n" \
 "*ncolors: 100 \n"
#define SMOOTH_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_discrete

ModeSpecOpt discrete_opts =
{0, (XrmOptionDescRec *) NULL, 0, (argtype *) NULL, (OptionStruct *) NULL};

#ifdef USE_MODULES
ModStruct   discrete_description =
{"discrete", "init_discrete", "draw_discrete", "release_discrete",
 "refresh_discrete", "init_discrete", NULL, &discrete_opts,
 1000, 4096, 2500, 1, 64, 1.0, "",
 "Shows various discrete maps", 0, NULL};

#endif

enum ftypes {
	SQRT, BIRDIE, STANDARD, TRIG, CUBIC, HENON, AILUJ, HSHOE, DELOG
};

/*#define TEST STANDARD */

#define BIASES 18
static enum ftypes bias[BIASES] =
{
	STANDARD, STANDARD, STANDARD, STANDARD,
	SQRT, SQRT, SQRT, SQRT,
	BIRDIE, BIRDIE, BIRDIE,
	AILUJ, AILUJ, AILUJ,
	TRIG, TRIG,
	CUBIC,
	HENON,
};

typedef struct {
	int         maxx;
	int         maxy;	/* max of the screen */
	double      a;
	double      b;
	double      c;
	double      d;
	double      e;
	double      i;
	double      j;		/* discrete parameters */
	double      ic;
	double      jc;
	double      is;
	double      js;
	int         inc;
	int         pix;
	enum ftypes op;
	int         count;
	XPoint     *pointBuffer;	/* pointer for XDrawPoints */
} discretestruct;

static discretestruct *discretes = NULL;

void
init_discrete(ModeInfo * mi)
{
	double      range;
	discretestruct *hp;

	if (discretes == NULL) {
		if ((discretes =
		     (discretestruct *) calloc(MI_NUM_SCREENS(mi),
					   sizeof (discretestruct))) == NULL)
			return;
	}
	hp = &discretes[MI_SCREEN(mi)];

	hp->maxx = MI_WIDTH(mi);
	hp->maxy = MI_HEIGHT(mi);
#ifdef TEST
	hp->op = TEST;
#else
	hp->op = bias[LRAND() % BIASES];
#endif
	switch (hp->op) {
		case HSHOE:
			hp->ic = 0;
			hp->jc = 0;
			hp->is = hp->maxx / (4);
			hp->js = hp->maxy / (4);
			hp->a = 0.5;
			hp->b = 0.5;
			hp->c = 0.2;
			hp->d = -1.25;
			hp->e = 1;
			hp->i = hp->j = 0.0;
			break;
		case DELOG:
			hp->ic = 0.5;
			hp->jc = 0.3;
			hp->is = hp->maxx / 1.5;
			hp->js = hp->maxy / 1.5;
			hp->a = 2.176399;
			hp->i = hp->j = 0.01;
			break;
		case HENON:
			hp->jc = ((LRAND() / MAXRAND) * 2.0 - 1.0) * 0.4;
			hp->ic = 1.3 * (1 - (hp->jc * hp->jc) / (0.4 * 0.4));
			hp->is = hp->maxx;
			hp->js = hp->maxy * 1.5;
			hp->a = 1;
			hp->b = 1.4;
			hp->c = 0.3;
			hp->i = hp->j = 0;
			break;
		case SQRT:
			hp->ic = 0;
			hp->jc = 0;
			hp->is = 1;
			hp->js = 1;
			range = sqrt((double) hp->maxx * 2 * hp->maxx * 2 +
				     (double) hp->maxy * 2 * hp->maxy * 2) /
				(10.0 + LRAND() % 10);

			hp->a = (LRAND() / MAXRAND) * range - range / 2.0;
			hp->b = (LRAND() / MAXRAND) * range - range / 2.0;
			hp->c = (LRAND() / MAXRAND) * range - range / 2.0;
			if (!(LRAND() % 2))
				hp->c = 0.0;
			hp->i = hp->j = 0.0;
			break;
		case STANDARD:
			hp->ic = M_PI;
			hp->jc = M_PI;
			hp->is = hp->maxx / (M_PI * 2);
			hp->js = hp->maxy / (M_PI * 2);
			hp->a = 0;	/* decay */
			hp->b = (LRAND() / MAXRAND) * 2.0;
			hp->c = 0;
			hp->i = M_PI;
			hp->j = M_PI;
			break;
		case BIRDIE:
			hp->ic = 0;
			hp->jc = 0;
			hp->is = hp->maxx / 2;
			hp->js = hp->maxy / 2;
			hp->a = 1.99 + ((LRAND() / MAXRAND) * 2.0 - 1.0) * 0.2;
			hp->b = 0;
			hp->c = 0.8 + ((LRAND() / MAXRAND) * 2.0 - 1.0) * 0.1;
			hp->i = hp->j = 0;
			break;
		case TRIG:
			hp->a = 5;
			hp->b = 0.5 + ((LRAND() / MAXRAND) * 2.0 - 1.0) * 0.3;
			hp->ic = hp->a;
			hp->jc = 0;
			hp->is = hp->maxx / (hp->b * 20);
			hp->js = hp->maxy / (hp->b * 20);
			hp->i = hp->j = 0;
			break;
		case CUBIC:
			hp->a = 2.77;
			hp->b = 0.1 + ((LRAND() / MAXRAND) * 2.0 - 1.0) * 0.1;
			hp->ic = 0;
			hp->jc = 0;
			hp->is = hp->maxx / 4;
			hp->js = hp->maxy / 4;
			hp->i = hp->j = 0.1;
			break;
		case AILUJ:
			{
				int         i;
				double      x, y, xn, yn;

				hp->ic = 0;
				hp->jc = 0;
				hp->is = hp->maxx / 4;
				hp->js = hp->maxx / 4;
				do {
					hp->a = ((LRAND() / MAXRAND) * 2.0 - 1.0) * 1.5 - 0.5;
					hp->b = ((LRAND() / MAXRAND) * 2.0 - 1.0) * 1.5;
					x = y = 0;
#define MAXITER 10
					for (i = 0; i < MAXITER && x * x + y * y < 13; i++) {	/* 'Brot calc */
						xn = x * x - y * y + hp->a;
						yn = 2 * x * y + hp->b;
						x = xn;
						y = yn;
					}
				} while (i < MAXITER);	/* wait for a connected set */
				hp->i = hp->j = 0.1;
				break;
			}
	}
	hp->pix = 0;
	hp->inc = 0;

	if (hp->pointBuffer == NULL) {
		hp->pointBuffer = (XPoint *) malloc(sizeof (XPoint) * MI_COUNT(mi));
		/* if fails will check later */
	}

	/* Clear the background. */
	MI_CLEARWINDOW(mi);

	XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_WHITE_PIXEL(mi));
	hp->count = 0;
}


void
draw_discrete(ModeInfo * mi)
{
	Display    *dsp = MI_DISPLAY(mi);
	Window      win = MI_WINDOW(mi);
	double      oldj, oldi;
	int         count = MI_COUNT(mi);
	int         cycles = MI_CYCLES(mi);
	int         k;
	XPoint     *xp;
	GC          gc = MI_GC(mi);
	discretestruct *hp;

	if (discretes == NULL)
		return;
	hp = &discretes[MI_SCREEN(mi)];
	if (hp->pointBuffer == NULL)
		return;

	k = count;
	xp = hp->pointBuffer;

	hp->inc++;

	MI_IS_DRAWN(mi) = True;

	if (MI_NPIXELS(mi) > 2) {
		XSetForeground(dsp, gc, MI_PIXEL(mi, hp->pix));
		if (++hp->pix >= MI_NPIXELS(mi))
			hp->pix = 0;
	}
	while (k--) {
		oldj = hp->j;
		oldi = hp->i;
		switch (hp->op) {
			case HSHOE:
				{
					int         i;

#if 0
					if (!k) {
						XSetForeground(dsp, gc, MI_BLACK_PIXEL(mi));
						XFillRectangle(dsp, win, gc, 0, 0, hp->maxx, hp->maxy);
						XSetForeground(dsp, gc, MI_PIXEL(mi, hp->pix));
					} else
#endif
#define HD
#ifdef HD
					if (k < count / 4) {
						hp->i = ((double) k / count) * 8 - 1;
						hp->j = 1;
					} else if (k < count / 2) {
						hp->i = 1;
						hp->j = 3 - ((double) k / count) * 8;
					} else if (k < 3 * count / 4) {
						hp->i = 5 - ((double) k / count) * 8;
						hp->j = -1;
					} else {
						hp->i = -1;
						hp->j = ((double) k / count) * 8 - 7;
					}
					for (i = 1; i < (hp->inc % 15); i++) {
						oldj = hp->j;
						oldi = hp->i;
#endif
						hp->i = (hp->a * oldi + hp->b) * oldj;
						hp->j = (hp->e - hp->d + hp->c * oldi) * oldj * oldj - hp->c * oldi + hp->d;
#ifdef HD
					}
#endif
					break;
				}
			case DELOG:
				hp->j = oldi;
				hp->i = hp->a * oldi * (1 - oldj);
				break;
			case HENON:
				hp->i = oldj + hp->a - hp->b * oldi * oldi;
				hp->j = hp->c * oldi;
				break;
			case SQRT:
				if (k) {
					hp->j = hp->a + hp->i;
					hp->i = -oldj + (hp->i < 0
					? sqrt(fabs(hp->b * (hp->i - hp->c)))
							 : -sqrt(fabs(hp->b * (hp->i - hp->c))));
				} else {
					static int  s = 1;

					hp->i = s * hp->inc * hp->maxx / cycles / 2;
					hp->j = hp->a + hp->i;
					s = -s;
				}
				break;
			case STANDARD:
				if (k) {
					hp->j = (1 - hp->a) * oldj + hp->b * sin(oldi) + hp->a * hp->c;
					hp->j = fmod(hp->j + 2 * M_PI, 2 * M_PI);
					hp->i = oldi + hp->j;
					hp->i = fmod(hp->i + 2 * M_PI, 2 * M_PI);
				} else {
					static int  s = 1;

					hp->j = M_PI + fmod(s * hp->inc * 2 * M_PI / (cycles - 0.5), M_PI);
					hp->i = M_PI;
					s = -s;
				}
				break;
			case BIRDIE:
				hp->j = oldi;
				hp->i = (1 - hp->c) * cos(M_PI * hp->a * oldj) + hp->c * hp->b;
				hp->b = oldj;
				break;
			case TRIG:
				{
					double      r2 = oldi * oldi + oldj * oldj;

					hp->i = hp->a + hp->b * (oldi * cos(r2) - oldj * sin(r2));
					hp->j = hp->b * (oldj * cos(r2) + oldi * sin(r2));
				}
				break;
			case CUBIC:
				hp->i = oldj;
				hp->j = hp->a * oldj - oldj * oldj * oldj - hp->b * oldi;
				break;
			case AILUJ:
				hp->i = ((LRAND() < MAXRAND / 2) ? -1 : 1) *
					sqrt(((oldi - hp->a) +
					      sqrt((oldi - hp->a) * (oldi - hp->a) + (oldj - hp->b) * (oldj - hp->b))) / 2);
				if (hp->i < 0.00000001 && hp->i > -0.00000001)
					hp->i = (hp->i > 0.0) ? 0.00000001 : -0.00000001;
				hp->j = (oldj - hp->b) / (2 * hp->i);
				break;
		}
		xp->x = hp->maxx / 2 + (int) ((hp->i - hp->ic) * hp->is);
		xp->y = hp->maxy / 2 - (int) ((hp->j - hp->jc) * hp->js);
		xp++;
	}
	XDrawPoints(dsp, win, gc, hp->pointBuffer, count, CoordModeOrigin);
	if (++hp->count > cycles) {
		init_discrete(mi);
	}
}

void
release_discrete(ModeInfo * mi)
{
	if (discretes != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			discretestruct *hp = &discretes[screen];

			if (hp->pointBuffer != NULL) {
				(void) free((void *) hp->pointBuffer);
				/* hp->pointBuffer = NULL; */
			}
		}
		(void) free((void *) discretes);
		discretes = NULL;
	}
}

void
refresh_discrete(ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
}

#endif /* MODE_discrete */
