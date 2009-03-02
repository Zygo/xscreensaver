/* -*- Mode: C; tab-width: 4 -*- */
/* thornbird --- continuously varying Thornbird set */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)discrete.c 4.10 98/04/24 xlockmore";

#endif

/*-
 * Copyright (c) 1996 by Tim Auckland <Tim.Auckland@Sun.COM>
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
 * "thornbird" shows a view of the "Bird in a Thornbush" fractal,
 * continuously varying the three free paramaters.
 *
 * Revision History:
 * 31-Jul-97: Adapted from discrete.c Copyright (c) 1996 by Tim Auckland
 */

#ifdef STANDALONE
#define PROGCLASS "Thornbird"
#define HACK_INIT init_thornbird
#define HACK_DRAW draw_thornbird
#define thornbird_opts xlockmore_opts
#define DEFAULTS "*delay: 1000 \n" \
 "*count: 800 \n" \
 "*cycles: 16 \n" \
 "*ncolors: 100 \n"
#define SMOOTH_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_thornbird

ModeSpecOpt thornbird_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   thornbird_description =
{"thornbird", "init_thornbird", "draw_thornbird", "release_thornbird",
 "refresh_thornbird", NULL, NULL, &thornbird_opts,
 1000, 800, 16, 1, 64, 1.0, "",
 "Shows an animated Bird in a Thorn Bush fractal map", 0, NULL};

#endif

typedef struct {
	int         maxx;
	int         maxy;	/* max of the screen */
	double      a;
	double      b;
	double      c;
	double      d;
	double      e;
	double      i;
	double      j;		/* thornbird parameters */
	int         inc;
	int         pix;
	int         count;
	int         nbuffers;
	XPoint    **pointBuffer;	/* pointer for XDrawPoints */
} thornbirdstruct;

static thornbirdstruct *thornbirds = NULL;

void
init_thornbird(ModeInfo * mi)
{
	thornbirdstruct *hp;

	if (thornbirds == NULL) {
		if ((thornbirds =
		     (thornbirdstruct *) calloc(MI_NUM_SCREENS(mi),
					  sizeof (thornbirdstruct))) == NULL)
			return;
	}
	hp = &thornbirds[MI_SCREEN(mi)];


	hp->maxx = MI_WIDTH(mi);
	hp->maxy = MI_HEIGHT(mi);

	hp->b = 0.1;
	hp->i = hp->j = 0.1;

	hp->pix = 0;
	hp->inc = 0;

	hp->nbuffers = MI_CYCLES(mi);

	if (hp->pointBuffer == NULL)
		hp->pointBuffer = (XPoint **) calloc(MI_CYCLES(mi), sizeof (XPoint *));

	if (hp->pointBuffer[0] == NULL)
		hp->pointBuffer[0] = (XPoint *) malloc(MI_COUNT(mi) * sizeof (XPoint));

	/* Clear the background. */
	MI_CLEARWINDOW(mi);

	hp->count = 0;
}


void
draw_thornbird(ModeInfo * mi)
{
	Display    *dsp = MI_DISPLAY(mi);
	Window      win = MI_WINDOW(mi);
	double      oldj, oldi;
	int         batchcount = MI_COUNT(mi);

	int         k;
	XPoint     *xp;
	GC          gc = MI_GC(mi);
	thornbirdstruct *hp = &thornbirds[MI_SCREEN(mi)];

	int         erase = (hp->inc + 1) % MI_CYCLES(mi);
	int         current = hp->inc % MI_CYCLES(mi);

	k = batchcount;


	xp = hp->pointBuffer[current];

	hp->a = 1.99 + 0.4 * sin((hp->inc / 2700.0)) * sin(hp->inc / 1700.0);
	hp->c = 0.80 + 0.15 * cos((hp->inc / 2700.0)) * cos(hp->inc / 1700.0);
#if 1
	hp->c += 0.05 * cos(hp->inc / 600.0);
	hp->a += 0.05 * sin(hp->inc / 600.0);
#endif
#if 0
	hp->a = 1.99 + ((LRAND() / MAXRAND) * 2.0 - 1.0) * 0.2;
	hp->c = 0.8 + ((LRAND() / MAXRAND) * 2.0 - 1.0) * 0.1;
#endif

	while (k--) {
		oldj = hp->j;
		oldi = hp->i;

		hp->j = oldi;
		hp->i = (1 - hp->c) * cos(M_PI * hp->a * oldj) + hp->c * hp->b;
		hp->b = oldj;

		xp->x = (short) (hp->maxx / 2 * (1 + hp->i));
		xp->y = (short) (hp->maxy / 2 * (1 - hp->j));
		xp++;
	}

	MI_IS_DRAWN(mi) = True;

	if (hp->pointBuffer[erase] == NULL) {
		hp->pointBuffer[erase] =
			(XPoint *) malloc(MI_COUNT(mi) * sizeof (XPoint));
	} else {
		XSetForeground(dsp, gc, MI_BLACK_PIXEL(mi));
		XDrawPoints(dsp, win, gc, hp->pointBuffer[erase],
			    batchcount, CoordModeOrigin);
	}
	if (MI_NPIXELS(mi) > 2) {
		XSetForeground(dsp, gc, MI_PIXEL(mi, hp->pix));
		if (erase % 5 == 0)
			if (++hp->pix >= MI_NPIXELS(mi))
				hp->pix = 0;
	}
	XDrawPoints(dsp, win, gc, hp->pointBuffer[current],
		    batchcount, CoordModeOrigin);
	hp->inc++;

}

void
release_thornbird(ModeInfo * mi)
{
	if (thornbirds != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			thornbirdstruct *hp = &thornbirds[screen];

			if (hp->pointBuffer != NULL) {
				int         i;

				for (i = 0; i < hp->nbuffers; i++)
					if (hp->pointBuffer[i] != NULL)
						(void) free((void *) hp->pointBuffer[i]);
				(void) free((void *) hp->pointBuffer);
			}
		}
		(void) free((void *) thornbirds);
		thornbirds = NULL;
	}
}

void
refresh_thornbird(ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
}

#endif /* MODE_thornbird */
