/* -*- Mode: C; tab-width: 4 -*- */
/* thornbird --- continuously varying Thornbird set */

#if 0
static const char sccsid[] = "@(#)thornbird.c	5.00 2000/11/01 xlockmore";
#endif

/*-
 * Copyright (c) 1996 by Tim Auckland <tda10.geo@yahoo.com>
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
 * continuously varying the three free parameters.
 *
 * Revision History:
 * 01-Nov-2000: Allocation checks
 * 04-Jun-1999: 3D tumble added by Tim Auckland
 * 31-Jul-1997: Adapted from discrete.c Copyright (c) 1996 by Tim Auckland
 */

#ifdef STANDALONE
# define MODE_thornbird
#define DEFAULTS	"*delay:    10000 \n" \
					"*count:    100   \n" \
					 "*cycles:  400   \n" \
					 "*ncolors: 64    \n" \
					 "*fpsSolid: true    \n" \
					"*ignoreRotation: True \n" \

# define BRIGHT_COLORS
# define thornbird_handle_event 0
# include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
# include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_thornbird

ENTRYPOINT ModeSpecOpt thornbird_opts =
{0, (XrmOptionDescRec *) NULL, 0, (argtype *) NULL, (OptionStruct *) NULL};

#ifdef USE_MODULES
ModStruct   thornbird_description =
{"thornbird", "init_thornbird", "draw_thornbird", "release_thornbird",
 "refresh_thornbird", "init_thornbird", (char *) NULL, &thornbird_opts,
 1000, 800, 16, 1, 64, 1.0, "",
 "Shows an animated Bird in a Thorn Bush fractal map", 0, NULL};

#endif

#define balance_rand(v)	((LRAND()/MAXRAND*(v))-((v)/2))	/* random around 0 */

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
    struct {
	  double  f1;
	  double  f2;
	}           liss;
    struct {
	  double  theta;
	  double  dtheta;
	  double  phi;
	  double  dphi;
	}           tumble;
    int         inc;
	int         pix;
	int         count;
	int         nbuffers;
	XPoint    **pointBuffer;	/* pointer for XDrawPoints */
} thornbirdstruct;

static thornbirdstruct *thornbirds = (thornbirdstruct *) NULL;

static void
free_thornbird(thornbirdstruct *hp)
{
	if (hp->pointBuffer != NULL) {
		int         buffer;

		for (buffer = 0; buffer < hp->nbuffers; buffer++)
			if (hp->pointBuffer[buffer] != NULL)
				(void) free((void *) hp->pointBuffer[buffer]);
		(void) free((void *) hp->pointBuffer);
		hp->pointBuffer = (XPoint **) NULL;
	}
}

ENTRYPOINT void
init_thornbird (ModeInfo * mi)
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
		if ((hp->pointBuffer = (XPoint **) calloc(MI_CYCLES(mi),
				sizeof (XPoint *))) == NULL) {
			free_thornbird(hp);
			return;
		}

	if (hp->pointBuffer[0] == NULL)
		if ((hp->pointBuffer[0] = (XPoint *) malloc(MI_COUNT(mi) *
				sizeof (XPoint))) == NULL) {
			free_thornbird(hp);
			return;
		}

	/* select frequencies for parameter variation */
	hp->liss.f1 = LRAND() % 5000;
	hp->liss.f2 = LRAND() % 2000;

	/* choose random 3D tumbling */
	hp->tumble.theta = 0;
	hp->tumble.phi = 0;
	hp->tumble.dtheta = balance_rand(0.001);
	hp->tumble.dphi = balance_rand(0.005);

	/* Clear the background. */
	MI_CLEARWINDOW(mi);

	hp->count = 0;
}


ENTRYPOINT void
draw_thornbird(ModeInfo * mi)
{
	Display    *dsp = MI_DISPLAY(mi);
	Window      win = MI_WINDOW(mi);
	double      oldj, oldi;
	int         batchcount = MI_COUNT(mi);
	int         k;
	XPoint     *xp;
	GC          gc = MI_GC(mi);
	int         erase;
	int         current;

	double      sint, cost, sinp, cosp;
	thornbirdstruct *hp;

	if (thornbirds == NULL)
		return;
	hp = &thornbirds[MI_SCREEN(mi)];
	if (hp->pointBuffer == NULL)
		return;

	erase = (hp->inc + 1) % MI_CYCLES(mi);
	current = hp->inc % MI_CYCLES(mi);
	k = batchcount;


	xp = hp->pointBuffer[current];

	/* vary papameters */
	hp->a = 1.99 + (0.4 * sin(hp->inc / hp->liss.f1) +
					0.05 * cos(hp->inc / hp->liss.f2));
	hp->c = 0.80 + (0.15 * cos(hp->inc / hp->liss.f1) +
					0.05 * sin(hp->inc / hp->liss.f2));

	/* vary view */
	hp->tumble.theta += hp->tumble.dtheta;
	hp->tumble.phi += hp->tumble.dphi;
	sint = sin(hp->tumble.theta);
	cost = cos(hp->tumble.theta);
	sinp = sin(hp->tumble.phi);
	cosp = cos(hp->tumble.phi);

	while (k--) {
		oldj = hp->j;
		oldi = hp->i;

		hp->j = oldi;
		hp->i = (1 - hp->c) * cos(M_PI * hp->a * oldj) + hp->c * hp->b;
		hp->b = oldj;

		xp->x = (short)
		  (hp->maxx / 2 * (1
						   + sint*hp->j + cost*cosp*hp->i - cost*sinp*hp->b));
		xp->y = (short)
		  (hp->maxy / 2 * (1
						   - cost*hp->j + sint*cosp*hp->i - sint*sinp*hp->b));
		xp++;
	}

	MI_IS_DRAWN(mi) = True;

	if (hp->pointBuffer[erase] == NULL) {
		if ((hp->pointBuffer[erase] = (XPoint *) malloc(MI_COUNT(mi) *
				sizeof (XPoint))) == NULL) {
			free_thornbird(hp);
			return;
		}
	} else {
		XSetForeground(dsp, gc, MI_BLACK_PIXEL(mi));
		XDrawPoints(dsp, win, gc, hp->pointBuffer[erase],
			    batchcount, CoordModeOrigin);
	}
	if (MI_NPIXELS(mi) > 2) {
		XSetForeground(dsp, gc, MI_PIXEL(mi, hp->pix));
#if 0
		if (erase == 0) /* change colours after "cycles" cycles */
#else
        if (!((hp->inc + 1) % (1 + (MI_CYCLES(mi) / 3)))) /* jwz: sooner */
#endif
			if (++hp->pix >= MI_NPIXELS(mi))
				hp->pix = 0;
	} else
		XSetForeground(dsp, gc, MI_WHITE_PIXEL(mi));

	XDrawPoints(dsp, win, gc, hp->pointBuffer[current],
		    batchcount, CoordModeOrigin);
	hp->inc++;
}

ENTRYPOINT void
reshape_thornbird(ModeInfo * mi, int width, int height)
{
  XClearWindow (MI_DISPLAY (mi), MI_WINDOW(mi));
  init_thornbird (mi);
}

ENTRYPOINT void
release_thornbird(ModeInfo * mi)
{
	if (thornbirds != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_thornbird(&thornbirds[screen]);
		(void) free((void *) thornbirds);
		thornbirds = (thornbirdstruct *) NULL;
	}
}

ENTRYPOINT void
refresh_thornbird (ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
}


XSCREENSAVER_MODULE ("Thornbird", thornbird)

#endif /* MODE_thornbird */
