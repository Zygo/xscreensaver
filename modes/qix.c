/* -*- Mode: C; tab-width: 4 -*- */
/* qix --- vector swirl */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)qix.c	4.07 97/11/24 xlockmore";

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
 * 03-Mar-98: Combined with Darrick Brown's "geometry".
 * 10-May-97: Compatible with xscreensaver
 * 29-Jul-90: support for multiple screens.
 *	      made check_bounds_?() a macro.
 *	      fixed initial parameter setup.
 * 15-Dec-89: Fix for proper skipping of {White,Black}Pixel() in colors.
 * 08-Oct-89: Fixed bug in memory allocation in init_qix().
 *	      Moved seconds() to an extern.
 * 23-Sep-89: Switch to random() and fixed bug w/ less than 4 lines.
 * 20-Sep-89: Lint.
 * 24-Mar-89: Written.
 */

#ifdef STANDALONE
#define PROGCLASS "Qix"
#define HACK_INIT init_qix
#define HACK_DRAW draw_qix
#define qix_opts xlockmore_opts
#define DEFAULTS "*delay: 30000 \n" \
 "*count: -5 \n" \
 "*cycles: 32 \n" \
 "*ncolors: 200 \n" \
 "*fullrandom: True \n"
#define SMOOTH_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_qix

#define DEF_COMPLETE  "False"
#define DEF_KALEID  "False"

static Bool complete;
static Bool kaleid;

static XrmOptionDescRec opts[] =
{
	{(char *) "-complete", (char *) ".qix.complete", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+complete", (char *) ".qix.complete", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-kaleid", (char *) ".qix.kaleid", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+kaleid", (char *) ".qix.kaleid", XrmoptionNoArg, (caddr_t) "off"}
};
static argtype vars[] =
{
      {(caddr_t *) & complete, (char *) "complete", (char *) "Complete", (char *) DEF_COMPLETE, t_Bool},
	{(caddr_t *) & kaleid, (char *) "kaleid", (char *) "Kaleid", (char *) DEF_KALEID, t_Bool}
};
static OptionStruct desc[] =
{
	{(char *) "-/+complete", (char *) "turn on/off complete morphing graph"},
	{(char *) "-/+kaleid", (char *) "turn on/off complete kaleidoscope"}
};

ModeSpecOpt qix_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   qix_description =
{"qix", "init_qix", "draw_qix", "release_qix",
 "refresh_qix", "init_qix", NULL, &qix_opts,
 30000, -5, 32, 1, 64, 1.0, "",
 "Shows spinning lines a la Qix(tm)", 0, NULL};

#endif

/* How many segments to draw per cycle when redrawing */
#define REDRAWSTEP 3
#define MINPOINTS 2
/*-
 * remember when complete: the number of lines to be drawn is 1+2+3+...+N or
 * N(N+1)/2
 */

/* I forget now, did Newton discover this as a boy? */
#define NEWT(n) (n*(n+1)/2)

typedef struct {
	int         pix;
	int         first, last;
	XPoint     *delta, *position;
	int         npoints;
	int         offset;
	int         max_delta;
	int         width, height, mid, midx, midy;
	int         nlines, linecount;
	int         redrawing, redrawpos;
	XPoint     *lineq;
	Bool        complete;
	Bool        kaleid;
} qixstruct;

static qixstruct *qixs = NULL;

#define check_bounds(qp, val, del, min, max)				\
{								\
    if ((val) < (min)) {						\
	*(del) = NRAND((qp)->max_delta) + (qp)->offset;	\
    } else if ((val) >= (max)) {					\
	*(del) = -(NRAND((qp)->max_delta)) - (qp)->offset;	\
    }								\
}

void
init_qix(ModeInfo * mi)
{
	qixstruct  *qp;
	int         i;

	if (qixs == NULL) {
		if ((qixs = (qixstruct *) calloc(MI_NUM_SCREENS(mi),
						 sizeof (qixstruct))) == NULL)
			return;
	}
	qp = &qixs[MI_SCREEN(mi)];

	qp->width = MI_WIDTH(mi);
	qp->height = MI_HEIGHT(mi);
	qp->mid = MAX(qp->width, qp->height) / 2;
	qp->midx = qp->width / 2;
	qp->midy = qp->height / 2;
	qp->max_delta = 16;
	qp->redrawing = 0;
	qp->linecount = 0;

	if (qp->width < 100) {	/* icon window */
		qp->max_delta /= 4;
	}
	qp->offset = qp->max_delta / 3;
	qp->last = 0;
	if (MI_NPIXELS(mi) > 2)
		qp->pix = NRAND(MI_NPIXELS(mi));

	qp->npoints = MI_COUNT(mi);
	if (qp->npoints < -MINPOINTS)
		qp->npoints = NRAND(qp->npoints - MINPOINTS + 1) + MINPOINTS;
	/* Absolute minimum */
	if (qp->npoints < MINPOINTS)
		qp->npoints = MINPOINTS;

	if (MI_IS_FULLRANDOM(mi))
		qp->complete = (Bool) (LRAND() & 1);
	else
		qp->complete = complete;

	if (qp->complete) {
		qp->kaleid = False;
	} else {
		if (MI_IS_FULLRANDOM(mi))
			qp->kaleid = (Bool) (LRAND() & 1);
		else
			qp->kaleid = kaleid;
	}
	if (qp->delta)
		(void) free((void *) qp->delta);
	if (qp->position)
		(void) free((void *) qp->position);

	qp->delta = (XPoint *) malloc(qp->npoints * sizeof (XPoint));
	qp->position = (XPoint *) malloc(qp->npoints * sizeof (XPoint));
	for (i = 0; i < qp->npoints; i++) {
		qp->delta[i].x = NRAND(qp->max_delta) + qp->offset;
		qp->delta[i].y = NRAND(qp->max_delta) + qp->offset;
		qp->position[i].x = NRAND(qp->width);
		qp->position[i].y = NRAND(qp->height);
	}

	qp->nlines = (qp->npoints == 2) ? (MI_CYCLES(mi) + 1) * 2 :
		((MI_CYCLES(mi) + qp->npoints) / qp->npoints) * qp->npoints;
	if (qp->complete) {
		qp->max_delta /= 4;
		qp->nlines = qp->npoints;
	}
	if (qp->kaleid) {
		qp->nlines = MI_CYCLES(mi) * 16;	/* Fudge it so its compatible */
	}
	if (qp->lineq) {
		(void) free((void *) qp->lineq);
		qp->lineq = NULL;
	}
	if (!qp->kaleid) {
		qp->lineq = (XPoint *) malloc(qp->nlines * sizeof (XPoint));
		for (i = 0; i < qp->nlines; i++)
			qp->lineq[i].x = qp->lineq[i].y = -1;	/* move initial point off screen */
	}
	MI_CLEARWINDOW(mi);
}

void
draw_qix(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	qixstruct  *qp = &qixs[MI_SCREEN(mi)];
	int         i, j, k, l;

	qp->first = (qp->last + qp->npoints) % qp->nlines;

	MI_IS_DRAWN(mi) = True;

	for (i = 0; i < qp->npoints; i++) {
		qp->position[i].x += qp->delta[i].x;
		qp->position[i].y += qp->delta[i].y;
		if (NRAND(20) < 1) {
			check_bounds(qp, qp->position[i].x, &qp->delta[i].x,
				     qp->width / 3, 2 * qp->width / 3);
		} else {
			check_bounds(qp, qp->position[i].x, &qp->delta[i].x, 0, qp->width);
		}
		if (NRAND(20) < 1) {
			check_bounds(qp, qp->position[i].y, &qp->delta[i].y,
				     qp->height / 3, 2 * qp->height / 3);
		} else {
			check_bounds(qp, qp->position[i].y, &qp->delta[i].y, 0, qp->height);
		}
	}
	XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
	if (qp->complete && qp->npoints > 3) {
		for (i = 0; i < qp->npoints - 1; i++)
			for (j = i + 1; j < qp->npoints; j++)
				XDrawLine(display, MI_WINDOW(mi), gc,
					  qp->lineq[qp->first + i].x, qp->lineq[qp->first + i].y,
					  qp->lineq[qp->first + j].x, qp->lineq[qp->first + j].y);
	} else if (!qp->kaleid) {
		for (i = 1; i < qp->npoints + ((qp->npoints == 2) ? -1 : 0); i++)
			XDrawLine(display, MI_WINDOW(mi), gc,
				  qp->lineq[qp->first + i - 1].x, qp->lineq[qp->first + i - 1].y,
				  qp->lineq[qp->first + i].x, qp->lineq[qp->first + i].y);
		XDrawLine(display, MI_WINDOW(mi), gc,
			  qp->lineq[qp->first].x, qp->lineq[qp->first].y,
			  qp->lineq[qp->first + qp->npoints - 1].x,
			  qp->lineq[qp->first + qp->npoints - 1].y);
	}
	if (MI_NPIXELS(mi) > 2) {
		XSetForeground(display, gc, MI_PIXEL(mi, qp->pix));
		if (++qp->pix >= MI_NPIXELS(mi))
			qp->pix = 0;
	} else
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));

	if (qp->complete && qp->npoints > 3) {
		for (i = 0; i < qp->npoints - 1; i++)
			for (j = i + 1; j < qp->npoints; j++)
				XDrawLine(display, MI_WINDOW(mi), gc,
					qp->position[i].x, qp->position[i].y,
				       qp->position[j].x, qp->position[j].y);
	} else if (qp->kaleid) {
		for (i = 0; i < qp->npoints; i++) {
			XSegment    segs[8];
			XPoint      small[2];

			j = (i + 1) % qp->npoints;
			small[0].x = ABS(qp->position[i].x - qp->mid);
			small[0].y = ABS(qp->position[i].y - qp->mid);
			small[1].x = ABS(qp->position[j].x - qp->mid);
			small[1].y = ABS(qp->position[j].y - qp->mid);
			segs[0].x1 = qp->midx + small[0].x;
			segs[0].y1 = qp->midy + small[0].y;
			segs[0].x2 = qp->midx + small[1].x;
			segs[0].y2 = qp->midy + small[1].y;
			segs[1].x1 = qp->midx - small[0].x;
			segs[1].y1 = qp->midy + small[0].y;
			segs[1].x2 = qp->midx - small[1].x;
			segs[1].y2 = qp->midy + small[1].y;
			segs[2].x1 = qp->midx - small[0].x;
			segs[2].y1 = qp->midy - small[0].y;
			segs[2].x2 = qp->midx - small[1].x;
			segs[2].y2 = qp->midy - small[1].y;
			segs[3].x1 = qp->midx + small[0].x;
			segs[3].y1 = qp->midy - small[0].y;
			segs[3].x2 = qp->midx + small[1].x;
			segs[3].y2 = qp->midy - small[1].y;
			segs[4].x1 = qp->midx + small[0].y;
			segs[4].y1 = qp->midy + small[0].x;
			segs[4].x2 = qp->midx + small[1].y;
			segs[4].y2 = qp->midy + small[1].x;
			segs[5].x1 = qp->midx + small[0].y;
			segs[5].y1 = qp->midy - small[0].x;
			segs[5].x2 = qp->midx + small[1].y;
			segs[5].y2 = qp->midy - small[1].x;
			segs[6].x1 = qp->midx - small[0].y;
			segs[6].y1 = qp->midy - small[0].x;
			segs[6].x2 = qp->midx - small[1].y;
			segs[6].y2 = qp->midy - small[1].x;
			segs[7].x1 = qp->midx - small[0].y;
			segs[7].y1 = qp->midy + small[0].x;
			segs[7].x2 = qp->midx - small[1].y;
			segs[7].y2 = qp->midy + small[1].x;
			XDrawSegments(display, MI_WINDOW(mi), gc, segs, 8);
			if (qp->npoints == 2)	/* do not repeat drawing the same line */
				break;
		}
		if (++qp->linecount >= qp->nlines) {
			qp->linecount = 0;
			MI_CLEARWINDOW(mi);
		}
	} else {
		for (i = 1; i < qp->npoints + ((qp->npoints == 2) ? -1 : 0); i++)
			XDrawLine(display, MI_WINDOW(mi), gc,
				qp->position[i - 1].x, qp->position[i - 1].y,
				  qp->position[i].x, qp->position[i].y);
		XDrawLine(display, MI_WINDOW(mi), gc, qp->position[0].x, qp->position[0].y,
			  qp->position[qp->npoints - 1].x, qp->position[qp->npoints - 1].y);
	}

	if (!qp->kaleid)
		for (i = 0; i < qp->npoints; i++) {
			qp->lineq[qp->last].x = qp->position[i].x;
			qp->lineq[qp->last].y = qp->position[i].y;
			qp->last++;
			if (qp->last >= qp->nlines)
				qp->last = 0;
		}
	if (qp->redrawing) {
		for (i = 0; i < REDRAWSTEP; i++) {
			j = (qp->first - qp->redrawpos + qp->nlines - qp->npoints) % qp->nlines;
			if (qp->complete && qp->npoints > 3) {
				for (k = 0; k < qp->npoints - 1; k++)
					for (l = k + 1; l < qp->npoints; l++)
						XDrawLine(display, MI_WINDOW(mi), gc,
							  qp->lineq[j + k].x, qp->lineq[j + k].y,
							  qp->lineq[j + l].x, qp->lineq[j + l].y);
			} else if (!qp->kaleid) {
				for (k = 1; k < qp->npoints + ((qp->npoints == 2) ? -1 : 0); k++)
					XDrawLine(display, MI_WINDOW(mi), gc,
						  qp->lineq[j + k - 1].x, qp->lineq[j + k - 1].y,
						  qp->lineq[j + k].x, qp->lineq[j + k].y);
				XDrawLine(display, MI_WINDOW(mi), gc,
					  qp->lineq[j].x, qp->lineq[j].y,
					  qp->lineq[j + qp->npoints - 1].x, qp->lineq[j + qp->npoints - 1].y);
			}
			qp->redrawpos += qp->npoints;
			if (qp->redrawpos >= qp->nlines - qp->npoints) {
				qp->redrawing = 0;
				break;
			}
		}
	}
}

void
release_qix(ModeInfo * mi)
{
	if (qixs != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			qixstruct  *qp = &qixs[screen];

			if (qp->lineq)
				(void) free((void *) qp->lineq);
			if (qp->delta)
				(void) free((void *) qp->delta);
			if (qp->position)
				(void) free((void *) qp->position);
		}
		(void) free((void *) qixs);
		qixs = NULL;
	}
}

void
refresh_qix(ModeInfo * mi)
{
	qixstruct  *qp = &qixs[MI_SCREEN(mi)];

	MI_CLEARWINDOW(mi);
	qp->redrawing = 1;
	qp->redrawpos = 0;
}

#endif /* MODE_qix */
