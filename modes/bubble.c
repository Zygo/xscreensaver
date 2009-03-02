/* -*- Mode: C; tab-width: 4 -*- */
/* bubble --- simple exploding bubbles */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)bubble.c	4.07 98/01/08 xlockmore";

#endif

/*-
 * Copyright (c) 1998 by Charles Vidal <vidalc@club-internet.fr>
 *         http://www.chez.com/vidalc
 *         and David Bagley
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
 * 10-Jan-98: Written.
 */

#ifdef STANDALONE
#define PROGCLASS "Bubble"
#define HACK_INIT init_bubble
#define HACK_DRAW draw_bubble
#define bubble_opts xlockmore_opts
#define DEFAULTS "*delay: 100000 \n" \
 "*count: 25 \n" \
 "*size: 100 \n" \
 "*ncolors: 200 \n" \
 "*fullrandom: True \n"
#define UNIFORM_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#define DEF_BOIL  "False"
static Bool boil;

static XrmOptionDescRec opts[] =
{
	{"-boil", ".bubble.boil", XrmoptionNoArg, (caddr_t) "on"},
	{"+boil", ".bubble.boil", XrmoptionNoArg, (caddr_t) "off"}
};
static argtype vars[] =
{
	{(caddr_t *) & boil, "boil", "Boil", DEF_BOIL, t_Bool}
};
static OptionStruct desc[] =
{
	{"-/+boil", "turn on/off boil"}
};

ModeSpecOpt bubble_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   bubble_description =
{"bubble", "init_bubble", "draw_bubble", "release_bubble",
 "refresh_bubble", "init_bubble", NULL, &bubble_opts,
 100000, 25, 1, 100, 64, 0.6, "",
 "Shows popping bubbles", 0, NULL};

#endif

typedef struct {
	int         x, y, life;
} bubbletype;

#define MINSIZE 20
#define MINBUBBLES 1

typedef struct {
	int         direction;
	int         color;
	int         width, height;
	int         nbubbles;
	Bool        boil;
	bubbletype *bubble;
	int         d;
	Pixmap      dbuf;
	GC          dbuf_gc;
} bubblestruct;

static bubblestruct *bubbles = NULL;

static void
updateBubble(ModeInfo * mi, int i)
{
	bubblestruct *bp = &bubbles[MI_SCREEN(mi)];
	int         x, diameter, x4, y4, diameter4;

	if (bp->bubble[i].life + 1 > bp->d - NRAND(16) ||
	    bp->bubble[i].y < 0) {
		bp->bubble[i].life = 0;
		return;
	}
	++bp->bubble[i].life;
	diameter = bp->bubble[i].life;
	x = bp->bubble[i].x;
	if (bp->boil) {
		bp->bubble[i].y -= diameter / 2;
		x += (int) (cos((double) (diameter +
		       bp->bubble[i].x) / (M_PI / 5.0)) * (double) diameter);
	}
	/* SunOS 4.1.X xnews server may crash without this */
	if (diameter < 4) {
		XFillRectangle(MI_DISPLAY(mi), bp->dbuf, bp->dbuf_gc,
			    x - diameter / 2, bp->bubble[i].y - diameter / 2,
			       diameter, diameter);
	} else {
		XDrawArc(MI_DISPLAY(mi), bp->dbuf, bp->dbuf_gc,
			 x - diameter / 2, bp->bubble[i].y - diameter / 2,
			 diameter, diameter, 0, 23040);
	}
	diameter4 = diameter / 4;
	if (diameter4 > 0) {
		x4 = x - diameter4 / 2 +
			((bp->direction / 2) ? (-1) : 1) * diameter / 6;
		y4 = bp->bubble[i].y - diameter4 / 2 +
			((bp->direction % 2) ? 1 : (-1)) * diameter / 6;
		/* SunOS 4.1.X xnews server may crash without this */
		if (diameter4 < 4) {
			XFillRectangle(MI_DISPLAY(mi), bp->dbuf, bp->dbuf_gc,
				       x4, y4, diameter4, diameter4);
		} else {
			XFillArc(MI_DISPLAY(mi), bp->dbuf, bp->dbuf_gc,
				 x4, y4, diameter4, diameter4, 0, 23040);
		}
	}
}

static void
changeBubble(ModeInfo * mi)
{
	bubblestruct *bp = &bubbles[MI_SCREEN(mi)];
	int         i;

	for (i = 0; i < bp->nbubbles; i++) {
		if (bp->bubble[i].life != 0)
			updateBubble(mi, i);
	}
	i = NRAND(bp->nbubbles);
	if (bp->bubble[i].life == 0) {
		bp->bubble[i].x = NRAND(bp->width);
		if (bp->boil)
			bp->bubble[i].y = bp->height -
				((bp->height >= 16) ? NRAND(bp->height / 16) : 0);
		else
			bp->bubble[i].y = NRAND(bp->height);
		updateBubble(mi, i);
	}
}

void
init_bubble(ModeInfo * mi)
{
	bubblestruct *bp;
	int         size = MI_SIZE(mi);
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	XGCValues   gcv;

	if (bubbles == NULL) {
		if ((bubbles = (bubblestruct *) calloc(MI_NUM_SCREENS(mi),
					     sizeof (bubblestruct))) == NULL)
			return;
	}
	bp = &bubbles[MI_SCREEN(mi)];

	bp->width = MI_WIDTH(mi);
	bp->height = MI_HEIGHT(mi);
	bp->direction = NRAND(4);
	if (MI_IS_FULLRANDOM(mi))
		bp->boil = (Bool) (LRAND() & 1);
	else
		bp->boil = boil;

	if (size < -MINSIZE)
		bp->d = NRAND(MIN(-size, MAX(MINSIZE,
		   MIN(bp->width, bp->height) / 2)) - MINSIZE + 1) + MINSIZE;
	else if (size < MINSIZE) {
		if (!size)
			bp->d = MAX(MINSIZE, MIN(bp->width, bp->height) / 2);
		else
			bp->d = MINSIZE;
	} else
		bp->d = MIN(size, MAX(MINSIZE,
				      MIN(bp->width, bp->height) / 2));
	bp->nbubbles = MI_COUNT(mi);
	if (bp->nbubbles < -MINBUBBLES) {
		bp->nbubbles = NRAND(-bp->nbubbles - MINBUBBLES + 1) + MINBUBBLES;
	} else if (bp->nbubbles < MINBUBBLES)
		bp->nbubbles = MINBUBBLES;
	if (bp->bubble != NULL)
		(void) free((void *) bp->bubble);
	bp->bubble = (bubbletype *) calloc(bp->nbubbles, sizeof (bubbletype));

	if (MI_NPIXELS(mi) > 2)
		bp->color = NRAND(MI_NPIXELS(mi));

	if (bp->dbuf)
		XFreePixmap(display, bp->dbuf);
	bp->dbuf = XCreatePixmap(display, window, bp->width, bp->height, 1);
	/* Do not want any exposure events from XCopyPlane */
	XSetGraphicsExposures(display, MI_GC(mi), False);

	gcv.foreground = 1;
	gcv.background = 0;
	gcv.function = GXcopy;
	gcv.graphics_exposures = False;
	gcv.line_width = 2;
	if (bp->dbuf_gc)
		XFreeGC(display, bp->dbuf_gc);
	bp->dbuf_gc = XCreateGC(display, bp->dbuf,
	       GCForeground | GCBackground | GCLineWidth | GCFunction, &gcv);

	MI_CLEARWINDOW(mi);
}

void
draw_bubble(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	bubblestruct *bp = &bubbles[MI_SCREEN(mi)];

	if (MI_NPIXELS(mi) <= 2)
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
	else {
		bp->color = (bp->color + 1) % MI_NPIXELS(mi);
		XSetForeground(display, gc, MI_PIXEL(mi, bp->color));
	}
	if (bp->dbuf) {
		XSetForeground(display, bp->dbuf_gc, 0);
		XFillRectangle(display, bp->dbuf, bp->dbuf_gc,
			       0, 0, bp->width, bp->height);
		XSetForeground(display, bp->dbuf_gc, 1);
		changeBubble(mi);
		XCopyPlane(display, bp->dbuf, window, gc,
			   0, 0, bp->width, bp->height, 0, 0, 1);
	}
}

void
release_bubble(ModeInfo * mi)
{
	if (bubbles != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			bubblestruct *bp = &bubbles[screen];

			if (bp->dbuf)
				XFreePixmap(MI_DISPLAY(mi), bp->dbuf);
			if (bp->dbuf_gc)
				XFreeGC(MI_DISPLAY(mi), bp->dbuf_gc);
			if (bp->bubble != NULL)
				(void) free((void *) bp->bubble);
		}
		(void) free((void *) bubbles);
		bubbles = NULL;
	}
}

void
refresh_bubble(ModeInfo * mi)
{
	/* Do nothing, it will refresh by itself */
}
