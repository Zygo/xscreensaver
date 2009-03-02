/* -*- Mode: C; tab-width: 4 -*- */
/* hop --- real plane fractals */

#if !defined( lint ) && !defined( SABER )
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
#define PROGCLASS "Hop"
#define HACK_INIT init_hop
#define HACK_DRAW draw_hop
#define hop_opts xlockmore_opts
#define DEFAULTS "*delay: 10000 \n" \
 "*count: 1000 \n" \
 "*cycles: 2500 \n" \
 "*ncolors: 200 \n" \
 "*fullrandom: True \n" \
 "*verbose: False \n"
#define SMOOTH_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */

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
	{(char *) "-martin", (char *) ".hop.martin", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+martin", (char *) ".hop.martin", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-popcorn", (char *) ".hop.popcorn", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+popcorn", (char *) ".hop.popcorn", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-ejk1", (char *) ".hop.ejk1", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+ejk1", (char *) ".hop.ejk1", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-ejk2", (char *) ".hop.ejk2", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+ejk2", (char *) ".hop.ejk2", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-ejk3", (char *) ".hop.ejk3", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+ejk3", (char *) ".hop.ejk3", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-ejk4", (char *) ".hop.ejk4", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+ejk4", (char *) ".hop.ejk4", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-ejk5", (char *) ".hop.ejk5", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+ejk5", (char *) ".hop.ejk5", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-ejk6", (char *) ".hop.ejk6", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+ejk6", (char *) ".hop.ejk6", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-rr", (char *) ".hop.rr", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+rr", (char *) ".hop.rr", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-jong", (char *) ".hop.jong", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+jong", (char *) ".hop.jong", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-sine", (char *) ".hop.sine", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+sine", (char *) ".hop.sine", XrmoptionNoArg, (caddr_t) "off"}
};
static argtype vars[] =
{
	{(void *) & martin, (char *) "martin", (char *) "Martin", (char *) DEF_MARTIN, t_Bool},
	{(void *) & popcorn, (char *) "popcorn", (char *) "Popcorn", (char *) DEF_POPCORN, t_Bool},
	{(void *) & ejk1, (char *) "ejk1", (char *) "EJK1", (char *) DEF_EJK1, t_Bool},
	{(void *) & ejk2, (char *) "ejk2", (char *) "EJK2", (char *) DEF_EJK2, t_Bool},
	{(void *) & ejk3, (char *) "ejk3", (char *) "EJK3", (char *) DEF_EJK3, t_Bool},
	{(void *) & ejk4, (char *) "ejk4", (char *) "EJK4", (char *) DEF_EJK4, t_Bool},
	{(void *) & ejk5, (char *) "ejk5", (char *) "EJK5", (char *) DEF_EJK5, t_Bool},
	{(void *) & ejk6, (char *) "ejk6", (char *) "EJK6", (char *) DEF_EJK6, t_Bool},
	{(void *) & rr, (char *) "rr", (char *) "RR", (char *) DEF_RR, t_Bool},
	{(void *) & jong, (char *) "jong", (char *) "Jong", (char *) DEF_JONG, t_Bool},
	{(void *) & sine, (char *) "sine", (char *) "Sine", (char *) DEF_SINE, t_Bool}
};
static OptionStruct desc[] =
{
	{(char *) "-/+martin", (char *) "turn on/off sqrt format"},
	{(char *) "-/+popcorn", (char *) "turn on/off Clifford A. Pickover's popcorn format"},
	{(char *) "-/+ejk1", (char *) "turn on/off ejk1 format"},
	{(char *) "-/+ejk2", (char *) "turn on/off ejk2 format"},
	{(char *) "-/+ejk3", (char *) "turn on/off ejk3 format"},
	{(char *) "-/+ejk4", (char *) "turn on/off ejk4 format"},
	{(char *) "-/+ejk5", (char *) "turn on/off ejk5 format"},
	{(char *) "-/+ejk6", (char *) "turn on/off ejk6 format"},
	{(char *) "-/+rr", (char *) "turn on/off rr format"},
	{(char *) "-/+jong", (char *) "turn on/off jong format"},
	{(char *) "-/+sine", (char *) "turn on/off sine format"}
};

ModeSpecOpt hop_opts =
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
} hopstruct;

static hopstruct *hops = (hopstruct *) NULL;

void
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
	if (hp->bufsize < 1)
		hp->bufsize = 1;
	if (hp->pointBuffer == NULL) {
		if ((hp->pointBuffer = (XPoint *) malloc(hp->bufsize *
				sizeof (XPoint))) == NULL)
			return;
	}

	MI_CLEARWINDOW(mi);

	XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
	hp->count = 0;
}


void
draw_hop(ModeInfo * mi)
{
	double      oldj, oldi;
	XPoint     *xp;
	int         k;
	hopstruct  *hp;

	if (hops == NULL)
		return;
	hp = &hops[MI_SCREEN(mi)];
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
		init_hop(mi);
	}
}

void
release_hop(ModeInfo * mi)
{
	if (hops != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			hopstruct  *hp = &hops[screen];

			if (hp->pointBuffer != NULL)
				free(hp->pointBuffer);
		}
		free(hops);
		hops = (hopstruct *) NULL;
	}
}

void
refresh_hop(ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
}

#endif /* MODE_hop */
