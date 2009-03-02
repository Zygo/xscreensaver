/* -*- Mode: C; tab-width: 4 -*- */
/* hop --- real plane fractals */

#if 0
static const char sccsid[] = "@(#)hop.c	5.00 2000/11/01 xlockmore";
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
 * Changes in xlockmore distribution
 * 01-Nov-2000: Allocation checks
 * 24-Jun-1997: EJK and RR functions stolen from xmartin2.2
 *              Ed Kubaitis <ejk@ux2.cso.uiuc.edu> ejk functions and xmartin
 *              Renaldo Recuerdo rr function, generalized exponent version
 *              of the Barry Martin's square root function
 * 10-May-1997: Compatible with xscreensaver
 * 27-Jul-1995: added Peter de Jong's hop from Scientific American
 *              July 87 p. 111.  Sometimes they are amazing but there are a
 *              few duds (I did not see a pattern in the parameters).
 * 29-Mar-1995: changed name from hopalong to hop
 * 09-Dec-1994: added Barry Martin's sine hop
 * Changes in original xlock
 * 29-Oct-1990: fix bad (int) cast.
 * 29-Jul-1990: support for multiple screens.
 * 08-Jul-1990: new timing and colors and new algorithm for fractals.
 * 15-Dec-1989: Fix for proper skipping of {White,Black}Pixel() in colors.
 * 08-Oct-1989: Fixed long standing typo bug in RandomInitHop();
 *	            Fixed bug in memory allocation in init_hop();
 *	            Moved seconds() to an extern.
 *	            Got rid of the % mod since .mod is slow on a sparc.
 * 20-Sep-1989: Lint.
 * 31-Aug-1988: Forked from xlock.c for modularity.
 * 23-Mar-1988: Coded HOPALONG routines from Scientific American Sept. 86 p. 14.
 *              Hopalong was attributed to Barry Martin of Aston University
 *              (Birmingham, England)
 */


#ifdef STANDALONE
#define MODE_hop
#define DEFAULTS	"*delay: 10000 \n" \
					"*count: 1000 \n" \
					"*cycles: 2500 \n" \
					"*ncolors: 200 \n" \
					"*fpsSolid: true \n" \

# define SMOOTH_COLORS
# define reshape_hop 0
# define hop_handle_event 0
# include "xlockmore.h"		/* in xscreensaver distribution */
# include "erase.h"
#else /* STANDALONE */
# include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_hop

#define DEF_MARTIN "False"
#define DEF_POPCORN "False"
#define DEF_EJK1 "False"
#define DEF_EJK2 "False"
#define DEF_EJK3 "False"
#define DEF_EJK4 "False"
#define DEF_EJK5 "False"
#define DEF_EJK6 "False"
#define DEF_RR "False"
#define DEF_JONG "False"
#define DEF_SINE "False"

static Bool martin;
static Bool popcorn;
static Bool ejk1;
static Bool ejk2;
static Bool ejk3;
static Bool ejk4;
static Bool ejk5;
static Bool ejk6;
static Bool rr;
static Bool jong;
static Bool sine;

static XrmOptionDescRec opts[] =
{
	{"-martin", ".hop.martin", XrmoptionNoArg, "on"},
	{"+martin", ".hop.martin", XrmoptionNoArg, "off"},
	{"-popcorn", ".hop.popcorn", XrmoptionNoArg, "on"},
	{"+popcorn", ".hop.popcorn", XrmoptionNoArg, "off"},
	{"-ejk1", ".hop.ejk1", XrmoptionNoArg, "on"},
	{"+ejk1", ".hop.ejk1", XrmoptionNoArg, "off"},
	{"-ejk2", ".hop.ejk2", XrmoptionNoArg, "on"},
	{"+ejk2", ".hop.ejk2", XrmoptionNoArg, "off"},
	{"-ejk3", ".hop.ejk3", XrmoptionNoArg, "on"},
	{"+ejk3", ".hop.ejk3", XrmoptionNoArg, "off"},
	{"-ejk4", ".hop.ejk4", XrmoptionNoArg, "on"},
	{"+ejk4", ".hop.ejk4", XrmoptionNoArg, "off"},
	{"-ejk5", ".hop.ejk5", XrmoptionNoArg, "on"},
	{"+ejk5", ".hop.ejk5", XrmoptionNoArg, "off"},
	{"-ejk6", ".hop.ejk6", XrmoptionNoArg, "on"},
	{"+ejk6", ".hop.ejk6", XrmoptionNoArg, "off"},
	{"-rr", ".hop.rr", XrmoptionNoArg, "on"},
	{"+rr", ".hop.rr", XrmoptionNoArg, "off"},
	{"-jong", ".hop.jong", XrmoptionNoArg, "on"},
	{"+jong", ".hop.jong", XrmoptionNoArg, "off"},
	{"-sine", ".hop.sine", XrmoptionNoArg, "on"},
	{"+sine", ".hop.sine", XrmoptionNoArg, "off"}
};
static argtype vars[] =
{
	{&martin,  "martin",  "Martin",  DEF_MARTIN,  t_Bool},
	{&popcorn, "popcorn", "Popcorn", DEF_POPCORN, t_Bool},
	{&ejk1, "ejk1", "EJK1", DEF_EJK1, t_Bool},
	{&ejk2, "ejk2", "EJK2", DEF_EJK2, t_Bool},
	{&ejk3, "ejk3", "EJK3", DEF_EJK3, t_Bool},
	{&ejk4, "ejk4", "EJK4", DEF_EJK4, t_Bool},
	{&ejk5, "ejk5", "EJK5", DEF_EJK5, t_Bool},
	{&ejk6, "ejk6", "EJK6", DEF_EJK6, t_Bool},
	{&rr,   "rr",   "RR",   DEF_RR,   t_Bool},
	{&jong, "jong", "Jong", DEF_JONG, t_Bool},
	{&sine, "sine", "Sine", DEF_SINE, t_Bool}
};
static OptionStruct desc[] =
{
	{"-/+martin", "turn on/off sqrt format"},
	{"-/+popcorn", "turn on/off Clifford A. Pickover's popcorn format"},
	{"-/+ejk1", "turn on/off ejk1 format"},
	{"-/+ejk2", "turn on/off ejk2 format"},
	{"-/+ejk3", "turn on/off ejk3 format"},
	{"-/+ejk4", "turn on/off ejk4 format"},
	{"-/+ejk5", "turn on/off ejk5 format"},
	{"-/+ejk6", "turn on/off ejk6 format"},
	{"-/+rr",   "turn on/off rr format"},
	{"-/+jong", "turn on/off jong format"},
	{"-/+sine", "turn on/off sine format"}
};

ENTRYPOINT ModeSpecOpt hop_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   hop_description =
{"hop", "init_hop", "draw_hop", "release_hop",
 "refresh_hop", "init_hop", (char *) NULL, &hop_opts,
 10000, 1000, 2500, 1, 64, 1.0, "",
 "Shows real plane iterated fractals", 0, NULL};

#endif

#define MARTIN 0
#define POPCORN 7
#define SINE 8
#define EJK1 1
#define EJK2 2
#define EJK3 9
#define EJK4 3
#define EJK5 4
#define EJK6 10
#define RR 5
#define JONG 6
#ifdef OFFENDING
#define OPS 8			/* 8, 9, 10 might be too close to a swastika for some... */
#else
#define OPS 11
#endif

typedef struct {
	int         centerx, centery;	/* center of the screen */
	double      a, b, c, d;
	double      i, j;	/* hopalong parameters */
	int         inc;
	int         pix;
	int         op;
	int         count;
	int         bufsize;
	XPoint     *pointBuffer;	/* pointer for XDrawPoints */
#ifdef STANDALONE
  eraser_state *eraser;
#endif
} hopstruct;

static hopstruct *hops = (hopstruct *) NULL;

ENTRYPOINT void
init_hop(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	double      range;
	hopstruct  *hp;

	if (hops == NULL) {
		if ((hops = (hopstruct *) calloc(MI_NUM_SCREENS(mi),
						 sizeof (hopstruct))) == NULL)
			return;
	}
	hp = &hops[MI_SCREEN(mi)];

	hp->centerx = MI_WIDTH(mi) / 2;
	hp->centery = MI_HEIGHT(mi) / 2;
	/* Make the other operations less common since they are less interesting */
	if (MI_IS_FULLRANDOM(mi)) {
		hp->op = NRAND(OPS);
	} else {
		if (martin)
			hp->op = MARTIN;
		else if (popcorn)
			hp->op = POPCORN;
		else if (ejk1)
			hp->op = EJK1;
		else if (ejk2)
			hp->op = EJK2;
		else if (ejk3)
			hp->op = EJK3;
		else if (ejk4)
			hp->op = EJK4;
		else if (ejk5)
			hp->op = EJK5;
		else if (ejk6)
			hp->op = EJK6;
		else if (rr)
			hp->op = RR;
		else if (jong)
			hp->op = JONG;
		else if (sine)
			hp->op = SINE;
		else
			hp->op = NRAND(OPS);
	}

	range = sqrt((double) hp->centerx * hp->centerx +
	     (double) hp->centery * hp->centery) / (1.0 + LRAND() / MAXRAND);
	hp->i = hp->j = 0.0;
	hp->inc = (int) ((LRAND() / MAXRAND) * 200) - 100;
#undef XMARTIN
	switch (hp->op) {
		case MARTIN:
#ifdef XMARTIN
			hp->a = (LRAND() / MAXRAND) * 1500.0 + 40.0;
			hp->b = (LRAND() / MAXRAND) * 17.0 + 3.0;
			hp->c = (LRAND() / MAXRAND) * 3000.0 + 100.0;
#else
			hp->a = ((LRAND() / MAXRAND) * 2.0 - 1.0) * range / 20.0;
			hp->b = ((LRAND() / MAXRAND) * 2.0 - 1.0) * range / 20.0;
			if (LRAND() & 1)
				hp->c = ((LRAND() / MAXRAND) * 2.0 - 1.0) * range / 20.0;
			else
				hp->c = 0.0;
#endif
			if (MI_IS_VERBOSE(mi))
				(void) fprintf(stdout, "sqrt a=%g, b=%g, c=%g\n", hp->a, hp->b, hp->c);
			break;
		case EJK1:
#ifdef XMARTIN
			hp->a = (LRAND() / MAXRAND) * 500.0;
			hp->c = (LRAND() / MAXRAND) * 100.0 + 10.0;
#else
			hp->a = ((LRAND() / MAXRAND) * 2.0 - 1.0) * range / 30.0;
			hp->c = ((LRAND() / MAXRAND) * 2.0 - 1.0) * range / 40.0;
#endif
			hp->b = (LRAND() / MAXRAND) * 0.4;
			if (MI_IS_VERBOSE(mi))
				(void) fprintf(stdout, "ejk1 a=%g, b=%g, c=%g\n", hp->a, hp->b, hp->c);
			break;
		case EJK2:
#ifdef XMARTIN
			hp->a = (LRAND() / MAXRAND) * 500.0;
#else
			hp->a = ((LRAND() / MAXRAND) * 2.0 - 1.0) * range / 30.0;
#endif
			hp->b = pow(10.0, 6.0 + (LRAND() / MAXRAND) * 24.0);
			if (LRAND() & 1)
				hp->b = -hp->b;
			hp->c = pow(10.0, (LRAND() / MAXRAND) * 9.0);
			if (LRAND() & 1)
				hp->c = -hp->c;
			if (MI_IS_VERBOSE(mi))
				(void) fprintf(stdout, "ejk2 a=%g, b=%g, c=%g\n", hp->a, hp->b, hp->c);
			break;
		case EJK3:
#ifdef XMARTIN
			hp->a = (LRAND() / MAXRAND) * 500.0;
			hp->c = (LRAND() / MAXRAND) * 80.0 + 30.0;
#else
			hp->a = ((LRAND() / MAXRAND) * 2.0 - 1.0) * range / 30.0;
			hp->c = ((LRAND() / MAXRAND) * 2.0 - 1.0) * range / 70.0;
#endif
			hp->b = (LRAND() / MAXRAND) * 0.35 + 0.5;
			if (MI_IS_VERBOSE(mi))
				(void) fprintf(stdout, "ejk3 a=%g, b=%g, c=%g\n", hp->a, hp->b, hp->c);
			break;
		case EJK4:
#ifdef XMARTIN
			hp->a = (LRAND() / MAXRAND) * 1000.0;
			hp->c = (LRAND() / MAXRAND) * 40.0 + 30.0;
#else
			hp->a = ((LRAND() / MAXRAND) * 2.0 - 1.0) * range / 2.0;
			hp->c = ((LRAND() / MAXRAND) * 2.0 - 1.0) * range / 200.0;
#endif
			hp->b = (LRAND() / MAXRAND) * 9.0 + 1.0;
			if (MI_IS_VERBOSE(mi))
				(void) fprintf(stdout, "ejk4 a=%g, b=%g, c=%g\n", hp->a, hp->b, hp->c);
			break;
		case EJK5:
#ifdef XMARTIN
			hp->a = (LRAND() / MAXRAND) * 600.0;
			hp->c = (LRAND() / MAXRAND) * 90.0 + 20.0;
#else
			hp->a = ((LRAND() / MAXRAND) * 2.0 - 1.0) * range / 2.0;
			hp->c = ((LRAND() / MAXRAND) * 2.0 - 1.0) * range / 200.0;
#endif
			hp->b = (LRAND() / MAXRAND) * 0.3 + 0.1;
			if (MI_IS_VERBOSE(mi))
				(void) fprintf(stdout, "ejk5 a=%g, b=%g, c=%g\n", hp->a, hp->b, hp->c);
			break;
		case EJK6:
#ifdef XMARTIN
			hp->a = (LRAND() / MAXRAND) * 100.0 + 550.0;
#else
			hp->a = ((LRAND() / MAXRAND) * 2.0 - 1.0) * range / 30.0;
#endif
			hp->b = (LRAND() / MAXRAND) + 0.5;
			if (MI_IS_VERBOSE(mi))
				(void) fprintf(stdout, "ejk6 a=%g, b=%g\n", hp->a, hp->b);
			break;
		case RR:
#ifdef XMARTIN
			hp->a = (LRAND() / MAXRAND) * 100.0;
			hp->b = (LRAND() / MAXRAND) * 20.0;
			hp->c = (LRAND() / MAXRAND) * 200.0;
#else
			hp->a = ((LRAND() / MAXRAND) * 2.0 - 1.0) * range / 40.0;
			hp->b = ((LRAND() / MAXRAND) * 2.0 - 1.0) * range / 200.0;
			hp->c = ((LRAND() / MAXRAND) * 2.0 - 1.0) * range / 20.0;
#endif
			hp->d = (LRAND() / MAXRAND) * 0.9;
			if (MI_IS_VERBOSE(mi))
				(void) fprintf(stdout, "rr a=%g, b=%g, c=%g, d=%g\n",
					       hp->a, hp->b, hp->c, hp->d);
			break;
		case POPCORN:
			hp->a = 0.0;
			hp->b = 0.0;
			hp->c = ((LRAND() / MAXRAND) * 2.0 - 1.0) * 0.24 + 0.25;
			hp->inc = 100;
			if (MI_IS_VERBOSE(mi))
				(void) fprintf(stdout, "popcorn a=%g, b=%g, c=%g, d=%g\n",
					       hp->a, hp->b, hp->c, hp->d);
			break;
		case JONG:
			hp->a = ((LRAND() / MAXRAND) * 2.0 - 1.0) * M_PI;
			hp->b = ((LRAND() / MAXRAND) * 2.0 - 1.0) * M_PI;
			hp->c = ((LRAND() / MAXRAND) * 2.0 - 1.0) * M_PI;
			hp->d = ((LRAND() / MAXRAND) * 2.0 - 1.0) * M_PI;
			if (MI_IS_VERBOSE(mi))
				(void) fprintf(stdout, "jong a=%g, b=%g, c=%g, d=%g\n",
					       hp->a, hp->b, hp->c, hp->d);
			break;
		case SINE:	/* MARTIN2 */
#ifdef XMARTIN
			hp->a = M_PI + ((LRAND() / MAXRAND) * 2.0 - 1.0) * 0.07;
#else
			hp->a = M_PI + ((LRAND() / MAXRAND) * 2.0 - 1.0) * 0.7;
#endif
			if (MI_IS_VERBOSE(mi))
				(void) fprintf(stdout, "sine a=%g\n", hp->a);
			break;
	}
	if (MI_NPIXELS(mi) > 2)
		hp->pix = NRAND(MI_NPIXELS(mi));
	hp->bufsize = MI_COUNT(mi);

	if (hp->pointBuffer == NULL) {
		if ((hp->pointBuffer = (XPoint *) malloc(hp->bufsize *
				sizeof (XPoint))) == NULL)
			return;
	}

#ifndef STANDALONE
	MI_CLEARWINDOW(mi);
#endif

	XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
	hp->count = 0;
}


ENTRYPOINT void
draw_hop(ModeInfo * mi)
{
	double      oldj, oldi;
	XPoint     *xp;
	int         k;
	hopstruct  *hp;

	if (hops == NULL)
		return;
	hp = &hops[MI_SCREEN(mi)];

#ifdef STANDALONE
    if (hp->eraser) {
      hp->eraser = erase_window (MI_DISPLAY(mi), MI_WINDOW(mi), hp->eraser);
      return;
    }
#endif


	if (hp->pointBuffer == NULL)
		return;
	xp = hp->pointBuffer;
	k = hp->bufsize;

	MI_IS_DRAWN(mi) = True;
	hp->inc++;
	if (MI_NPIXELS(mi) > 2) {
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_PIXEL(mi, hp->pix));
		if (++hp->pix >= MI_NPIXELS(mi))
			hp->pix = 0;
	}
	while (k--) {
		oldj = hp->j;
		switch (hp->op) {
			case MARTIN:	/* SQRT, MARTIN1 */
				oldi = hp->i + hp->inc;
				hp->j = hp->a - hp->i;
				hp->i = oldj + ((hp->i < 0)
					   ? sqrt(fabs(hp->b * oldi - hp->c))
					: -sqrt(fabs(hp->b * oldi - hp->c)));
				xp->x = hp->centerx + (int) (hp->i + hp->j);
				xp->y = hp->centery - (int) (hp->i - hp->j);
				break;
			case EJK1:
				oldi = hp->i + hp->inc;
				hp->j = hp->a - hp->i;
				hp->i = oldj - ((hp->i > 0) ? (hp->b * oldi - hp->c) :
						-(hp->b * oldi - hp->c));
				xp->x = hp->centerx + (int) (hp->i + hp->j);
				xp->y = hp->centery - (int) (hp->i - hp->j);
				break;
			case EJK2:
				oldi = hp->i + hp->inc;
				hp->j = hp->a - hp->i;
				hp->i = oldj - ((hp->i < 0) ? log(fabs(hp->b * oldi - hp->c)) :
					   -log(fabs(hp->b * oldi - hp->c)));
				xp->x = hp->centerx + (int) (hp->i + hp->j);
				xp->y = hp->centery - (int) (hp->i - hp->j);
				break;
			case EJK3:
				oldi = hp->i + hp->inc;
				hp->j = hp->a - hp->i;
				hp->i = oldj - ((hp->i > 0) ? sin(hp->b * oldi) - hp->c :
						-sin(hp->b * oldi) - hp->c);
				xp->x = hp->centerx + (int) (hp->i + hp->j);
				xp->y = hp->centery - (int) (hp->i - hp->j);
				break;
			case EJK4:
				oldi = hp->i + hp->inc;
				hp->j = hp->a - hp->i;
				hp->i = oldj - ((hp->i > 0) ? sin(hp->b * oldi) - hp->c :
					  -sqrt(fabs(hp->b * oldi - hp->c)));
				xp->x = hp->centerx + (int) (hp->i + hp->j);
				xp->y = hp->centery - (int) (hp->i - hp->j);
				break;
			case EJK5:
				oldi = hp->i + hp->inc;
				hp->j = hp->a - hp->i;
				hp->i = oldj - ((hp->i > 0) ? sin(hp->b * oldi) - hp->c :
						-(hp->b * oldi - hp->c));
				xp->x = hp->centerx + (int) (hp->i + hp->j);
				xp->y = hp->centery - (int) (hp->i - hp->j);
				break;
			case EJK6:
				oldi = hp->i + hp->inc;
				hp->j = hp->a - hp->i;
				hp->i = oldj - asin((hp->b * oldi) - (long) (hp->b * oldi));
				xp->x = hp->centerx + (int) (hp->i + hp->j);
				xp->y = hp->centery - (int) (hp->i - hp->j);
				break;
			case RR:	/* RR1 */
				oldi = hp->i + hp->inc;
				hp->j = hp->a - hp->i;
				hp->i = oldj - ((hp->i < 0) ? -pow(fabs(hp->b * oldi - hp->c), hp->d) :
				     pow(fabs(hp->b * oldi - hp->c), hp->d));
				xp->x = hp->centerx + (int) (hp->i + hp->j);
				xp->y = hp->centery - (int) (hp->i - hp->j);
				break;
			case POPCORN:
#define HVAL 0.05
#define INCVAL 50
				{
					double      tempi, tempj;

					if (hp->inc >= 100)
						hp->inc = 0;
					if (hp->inc == 0) {
						if (hp->a++ >= INCVAL) {
							hp->a = 0;
							if (hp->b++ >= INCVAL)
								hp->b = 0;
						}
						hp->i = (-hp->c * INCVAL / 2 + hp->c * hp->a) * M_PI / 180.0;
						hp->j = (-hp->c * INCVAL / 2 + hp->c * hp->b) * M_PI / 180.0;
					}
					tempi = hp->i - HVAL * sin(hp->j + tan(3.0 * hp->j));
					tempj = hp->j - HVAL * sin(hp->i + tan(3.0 * hp->i));
					xp->x = hp->centerx + (int) (MI_WIDTH(mi) / 40 * tempi);
					xp->y = hp->centery + (int) (MI_HEIGHT(mi) / 40 * tempj);
					hp->i = tempi;
					hp->j = tempj;
				}
				break;
			case JONG:
				if (hp->centerx > 0)
					oldi = hp->i + 4 * hp->inc / hp->centerx;
				else
					oldi = hp->i;
				hp->j = sin(hp->c * hp->i) - cos(hp->d * hp->j);
				hp->i = sin(hp->a * oldj) - cos(hp->b * oldi);
				xp->x = hp->centerx + (int) (hp->centerx * (hp->i + hp->j) / 4.0);
				xp->y = hp->centery - (int) (hp->centery * (hp->i - hp->j) / 4.0);
				break;
			case SINE:	/* MARTIN2 */
				oldi = hp->i + hp->inc;
				hp->j = hp->a - hp->i;
				hp->i = oldj - sin(oldi);
				xp->x = hp->centerx + (int) (hp->i + hp->j);
				xp->y = hp->centery - (int) (hp->i - hp->j);
				break;
		}
		xp++;
	}
	XDrawPoints(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi),
		    hp->pointBuffer, hp->bufsize, CoordModeOrigin);
	if (++hp->count > MI_CYCLES(mi)) {
#ifdef STANDALONE
      hp->eraser = erase_window (MI_DISPLAY(mi), MI_WINDOW(mi), hp->eraser);
#endif /* STANDALONE */
		init_hop(mi);
	}
}

ENTRYPOINT void
release_hop(ModeInfo * mi)
{
	if (hops != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			hopstruct  *hp = &hops[screen];

			if (hp->pointBuffer != NULL)
				(void) free((void *) hp->pointBuffer);
		}
		(void) free((void *) hops);
		hops = (hopstruct *) NULL;
	}
}

ENTRYPOINT void
refresh_hop(ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
}

XSCREENSAVER_MODULE_2 ("Hopalong", hopalong, hop)

#endif /* MODE_hop */
