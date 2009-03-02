/* -*- Mode: C; tab-width: 4 -*- */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)wire.c	4.03 97/05/10 xlockmore";

#endif

/*-
 * wire.c - logical circuits based on simple state-changes (wireworld)
 *           for the X Window System lockscreen.
 *
 * Copyright (c) 1996 by David Bagley.
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
 * 10-May-97: Compatible with xscreensaver
 * 14-Jun-96: Coded from A.K. Dewdney's "Computer Recreations", Scientific
 *            American Magazine" Jan 1990 pp 146-148.  Used ant.c as an
 *            example.  do_gen() based on code by Kevin Dahlhausen
 *            <ap096@po.cwru.edu> and Stefan Strack
 *            <stst@vuse.vanderbilt.edu>.
 */

/*-
 * OR gate is protected by diodes
 *           XX     XX
 * Input ->XXX XX XX XXX<- Input
 *           XX  X  XX
 *               X
 *               X
 *               |
 *               V
 *             Output
 *
 *   ->XXXoOXXX-> Electron moving in a wire
 *
 * memory element, about to forget 1 and remember 0
 *         Remember 0
 *             o
 *            X O XX XX   Memory Loop
 *            X  XX X XXXX
 *            X   XX XX   X oOX-> 1 Output
 * Inputs ->XX        X    X
 *                X   X XX o
 * Inputs ->XXXXXXX XX X XO Memory of 1
 *                XX    XX
 *         Remember 1
 */

#ifdef STANDALONE
#define PROGCLASS            "Wire"
#define HACK_INIT            init_wire
#define HACK_DRAW            draw_wire
#define DEF_DELAY            500000
#define DEF_BATCHCOUNT       1000
#define DEF_CYCLES           150
#define DEF_SIZE             -8
#define DEF_NCOLORS          64
#include "xlockmore.h"    /* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"    /* in xlockmore distribution */
ModeSpecOpt wire_opts =
{0, NULL, 0, NULL, NULL};
 
#endif /* STANDALONE */

#define WIREBITS(n,w,h)\
  wp->pixmaps[wp->init_bits++]=\
  XCreatePixmapFromBitmapData(display,window,(char *)n,w,h,1,0,1)

#define COLORS 4
#define MINWIRES 1
#define PATTERNSIZE 8
#define MINGRIDSIZE 24
#define MINSIZE 1

static unsigned char patterns[COLORS - 1][PATTERNSIZE] =
{
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},	/* black */
	{0x00, 0x66, 0x66, 0x00, 0x00, 0x66, 0x66, 0x00},	/* spots */
	{0xff, 0x99, 0x99, 0xff, 0xff, 0x99, 0x99, 0xff}	/* spots */
};

#define PATHDIRS 4
static int  prob_array[PATHDIRS] =
{75, 85, 90, 100};

#define SPACE 0
#define WIRE 1			/* Normal wire */
#define HEAD 2			/* electron head */
#define TAIL 3			/* electron tail */

#define REDRAWSTEP 2000		/* How much wire to draw per cycle */

/* Singly linked list */
typedef struct _RectList {
	XPoint      pt;
	struct _RectList *next;
} RectList;

typedef struct {
	int         init_bits;
	int         generation;
	int         xs, ys;
	int         xb, yb;
	int         nrows, ncols;
	int         bnrows, bncols;
	int         mincol, minrow, maxcol, maxrow;
	int         width, height;
	int         n;
	int         redrawing, redrawpos;
	unsigned char *oldcells, *newcells;
	unsigned char colors[COLORS - 1];
	int         nrects[COLORS - 1];
	RectList   *rectlist[COLORS - 1];
	GC          stippledGC;
	Pixmap      pixmaps[COLORS - 1];
} circuitstruct;

static circuitstruct *circuits = NULL;

static void
addtolist(ModeInfo * mi, int col, int row, unsigned char state)
{
	circuitstruct *wp = &circuits[MI_SCREEN(mi)];
	RectList   *current;

	current = wp->rectlist[state];
	wp->rectlist[state] = (RectList *) malloc(sizeof (RectList));
	wp->rectlist[state]->pt.x = col;
	wp->rectlist[state]->pt.y = row;
	wp->rectlist[state]->next = current;
	wp->nrects[state]++;
}

#ifdef DEBUG
static void
print_state(ModeInfo * mi, int state)
{
	circuitstruct *wp = &circuits[MI_SCREEN(mi)];
	RectList   *locallist;
	int         i = 0;

	locallist = wp->rectlist[state];
	(void) printf("state %d\n", state);
	while (locallist) {
		(void) printf("%d x %d, y %d\n", i,
			      locallist->pt.x, locallist->pt.y);
		locallist = locallist->next;
		i++;
	}
}

#endif

static void
free_state(circuitstruct * wp, int state)
{
	RectList   *current;

	while (wp->rectlist[state]) {
		current = wp->rectlist[state];
		wp->rectlist[state] = wp->rectlist[state]->next;
		(void) free((void *) current);
	}
	wp->rectlist[state] = NULL;
	wp->nrects[state] = 0;
}

static void
drawcell(ModeInfo * mi, int col, int row, unsigned char state)
{
	circuitstruct *wp = &circuits[MI_SCREEN(mi)];
	XGCValues   gcv;
	GC          gc;

	if (MI_NPIXELS(mi) > 2) {
		gc = MI_GC(mi);
		XSetForeground(MI_DISPLAY(mi), gc, MI_PIXEL(mi, wp->colors[state]));
	} else {
		gcv.stipple = wp->pixmaps[state];
		gcv.foreground = MI_WIN_WHITE_PIXEL(mi);
		gcv.background = MI_WIN_BLACK_PIXEL(mi);
		XChangeGC(MI_DISPLAY(mi), wp->stippledGC,
			  GCStipple | GCForeground | GCBackground, &gcv);
		gc = wp->stippledGC;
	}
	XFillRectangle(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi),
	       wp->xb + wp->xs * col, wp->yb + wp->ys * row, wp->xs, wp->ys);
}

static void
draw_state(ModeInfo * mi, int state)
{
	circuitstruct *wp = &circuits[MI_SCREEN(mi)];
	GC          gc;
	XRectangle *rects;
	XGCValues   gcv;
	RectList   *current;

	if (MI_NPIXELS(mi) > 2) {
		gc = MI_GC(mi);
		XSetForeground(MI_DISPLAY(mi), gc, MI_PIXEL(mi, wp->colors[state]));
	} else {
		gcv.stipple = wp->pixmaps[state];
		gcv.foreground = MI_WIN_WHITE_PIXEL(mi);
		gcv.background = MI_WIN_BLACK_PIXEL(mi);
		XChangeGC(MI_DISPLAY(mi), wp->stippledGC,
			  GCStipple | GCForeground | GCBackground, &gcv);
		gc = wp->stippledGC;
	}

	{
		/* Take advantage of XFillRectangles */
		int         nrects = 0;

		/* Create Rectangle list from part of the rectlist */
		rects = (XRectangle *) malloc(wp->nrects[state] * sizeof (XRectangle));
		current = wp->rectlist[state];
		while (current) {
			rects[nrects].x = wp->xb + current->pt.x * wp->xs;
			rects[nrects].y = wp->yb + current->pt.y * wp->ys;
			rects[nrects].width = wp->xs;
			rects[nrects].height = wp->ys;
			current = current->next;
			nrects++;
		}
		/* Finally get to draw */
		XFillRectangles(MI_DISPLAY(mi), MI_WINDOW(mi), gc, rects, nrects);
		/* Free up rects list and the appropriate part of the rectlist */
		(void) free((void *) rects);
	}
	free_state(wp, state);
	XFlush(MI_DISPLAY(mi));
}

#if 0
static void
RandomSoup(circuitstruct * wp)
{
	int         i, j;

	for (j = 1; j < wp->bnrows - 1; j++)
		for (i = 1; i < wp->bncols - 1; i++) {
			*(wp->newcells + i + j * wp->bncols) =
				(NRAND(100) > wp->n) ? SPACE : (NRAND(4)) ? WIRE : (NRAND(2)) ?
				HEAD : TAIL;
		}
}

#endif


static void
create_path(circuitstruct * wp, int n)
{
	int         col, row;
	int         count = 0;
	int         dir, prob;
	int         nextcol = 0, nextrow = 0, i;

#ifdef RANDOMSTART
	/* Path usually "mushed" in a corner */
	col = NRAND(wp->ncols) + 1;
	row = NRAND(wp->nrows) + 1;
#else
	/* Start from center */
	col = wp->ncols / 2;
	row = wp->nrows / 2;
#endif
	wp->mincol = col - 1, wp->minrow = row - 1;
	wp->maxcol = col + 1, wp->maxrow = row + 1;
	dir = NRAND(PATHDIRS);
	*(wp->newcells + col + row * wp->bncols) = HEAD;
	while (++count < n) {
		prob = NRAND(prob_array[PATHDIRS - 1]);
		i = 0;
		while (prob > prob_array[i])
			i++;
		dir = (dir + i) % PATHDIRS;
		switch (dir) {
			case 0:
				nextcol = col;
				nextrow = row - 1;
				break;
			case 1:
				nextcol = col + 1;
				nextrow = row;
				if (!NRAND(10))
					nextrow += ((LRAND() & 1) ? -1 : 1);
				break;
			case 2:
				nextcol = col;
				nextrow = row + 1;
				break;
			case 3:
				nextcol = col - 1;
				nextrow = row;
				if (!NRAND(10))
					nextrow += ((LRAND() & 1) ? -1 : 1);
				break;
		}
		if (nextrow > 0 && nextrow < wp->bnrows - 1 &&
		    nextcol > 0 && nextcol < wp->bncols - 1) {
			col = nextcol;
			row = nextrow;
			if (col == wp->mincol && col > 1)
				wp->mincol--;
			if (row == wp->minrow && row > 1)
				wp->minrow--;
			if (col == wp->maxcol && col < wp->bncols - 2)
				wp->maxcol++;
			if (row == wp->maxrow && row < wp->bnrows - 2)
				wp->maxrow++;

			if (!*(wp->newcells + col + row * wp->bncols))
				*(wp->newcells + col + row * wp->bncols) = WIRE;
		} else
			dir = (dir + PATHDIRS / 2) % PATHDIRS;
	}
	*(wp->newcells + col + row * wp->bncols) = HEAD;
}

static void
do_gen(circuitstruct * wp)
{
	int         i, j;
	unsigned char *z;
	int         count;

#define loc(X, Y) (*(wp->oldcells + (X) + ((Y) * wp->bncols)))
#define add(X, Y) if (loc(X, Y) == HEAD) count++

	for (j = wp->minrow; j <= wp->maxrow; j++) {
		for (i = wp->mincol; i <= wp->maxcol; i++) {
			z = wp->newcells + i + j * wp->bncols;
			switch (loc(i, j)) {
				case SPACE:
					*z = SPACE;
					break;
				case TAIL:
					*z = WIRE;
					break;
				case HEAD:
					*z = TAIL;
					break;
				case WIRE:
					count = 0;
					add(i - 1, j);
					add(i + 1, j);
					add(i - 1, j - 1);
					add(i, j - 1);
					add(i + 1, j - 1);
					add(i - 1, j + 1);
					add(i, j + 1);
					add(i + 1, j + 1);
					if (count == 1 || count == 2)
						*z = HEAD;
					else
						*z = WIRE;
					break;
				default:
					(void) fprintf(stderr,
						       "oops, bad internal character %d at %d,%d\n",
						       (int) loc(i, j), i, j);
					exit(1);
			}
		}
	}
}

static void
free_list(circuitstruct * wp)
{
	int         state;

	for (state = 0; state < COLORS - 1; state++)
		free_state(wp, state);
}

void
init_wire(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         size = MI_SIZE(mi);
	circuitstruct *wp;
	XGCValues   gcv;
	int         i;

	if (circuits == NULL) {
		if ((circuits = (circuitstruct *) calloc(MI_NUM_SCREENS(mi),
					    sizeof (circuitstruct))) == NULL)
			return;
	}
	wp = &circuits[MI_SCREEN(mi)];
	wp->redrawing = 0;
	if ((MI_NPIXELS(mi) <= 2) && (wp->init_bits == 0)) {
		gcv.fill_style = FillOpaqueStippled;
		wp->stippledGC = XCreateGC(display, window, GCFillStyle, &gcv);
		for (i = 0; i < COLORS - 1; i++)
			WIREBITS(patterns[i], PATTERNSIZE, PATTERNSIZE);
	}
	if (MI_NPIXELS(mi) > 2) {
		wp->colors[0] = (NRAND(MI_NPIXELS(mi)));
		wp->colors[1] = (wp->colors[0] + MI_NPIXELS(mi) / 6 +
				 NRAND(MI_NPIXELS(mi) / 4)) % MI_NPIXELS(mi);
		wp->colors[2] = (wp->colors[1] + MI_NPIXELS(mi) / 6 +
				 NRAND(MI_NPIXELS(mi) / 4)) % MI_NPIXELS(mi);
	}
	free_list(wp);
	wp->generation = 0;
	wp->n = MI_BATCHCOUNT(mi);
	if (wp->n < -MINWIRES) {
		wp->n = NRAND(-wp->n - MINWIRES + 1) + MINWIRES;
	} else if (wp->n < MINWIRES)
		wp->n = MINWIRES;

	wp->width = MI_WIN_WIDTH(mi);
	wp->height = MI_WIN_HEIGHT(mi);

	if (size < -MINSIZE)
		wp->ys = NRAND(MIN(-size, MAX(MINSIZE, MIN(wp->width, wp->height) /
				      MINGRIDSIZE)) - MINSIZE + 1) + MINSIZE;
	else if (size < MINSIZE) {
		if (!size)
			wp->ys = MAX(MINSIZE, MIN(wp->width, wp->height) / MINGRIDSIZE);
		else
			wp->ys = MINSIZE;
	} else
		wp->ys = MIN(size, MAX(MINSIZE, MIN(wp->width, wp->height) /
				       MINGRIDSIZE));
	wp->xs = wp->ys;
	wp->ncols = MAX(wp->width / wp->xs, 2);
	wp->nrows = MAX(wp->height / wp->ys, 2);
	wp->xb = (wp->width - wp->xs * wp->ncols) / 2;
	wp->yb = (wp->height - wp->ys * wp->nrows) / 2;

	wp->bncols = wp->ncols + 2;
	wp->bnrows = wp->nrows + 2;
	XClearWindow(display, MI_WINDOW(mi));

	if (wp->oldcells != NULL)
		(void) free((void *) wp->oldcells);
	wp->oldcells = (unsigned char *)
		calloc(wp->bncols * wp->bnrows, sizeof (unsigned char));

	if (wp->newcells != NULL)
		(void) free((void *) wp->newcells);
	wp->newcells = (unsigned char *)
		calloc(wp->bncols * wp->bnrows, sizeof (unsigned char));

	create_path(wp, wp->n);
}

void
draw_wire(ModeInfo * mi)
{
	circuitstruct *wp = &circuits[MI_SCREEN(mi)];
	int         offset, i, j;
	unsigned char *z, *znew;

	/* wires do not grow so min max stuff does not change */
	for (j = wp->minrow; j <= wp->maxrow; j++) {
		for (i = wp->mincol; i <= wp->maxcol; i++) {
			offset = j * wp->bncols + i;
			z = wp->oldcells + offset;
			znew = wp->newcells + offset;
			if (*z != *znew) {	/* Counting on once a space always a space */
				*z = *znew;
				addtolist(mi, i - 1, j - 1, *znew - 1);
			}
		}
	}
	for (i = 0; i < COLORS - 1; i++)
		draw_state(mi, i);
	if (++wp->generation > MI_CYCLES(mi))
		init_wire(mi);
	else
		do_gen(wp);

	if (wp->redrawing) {
		for (i = 0; i < REDRAWSTEP; i++) {
			if ((*(wp->oldcells + wp->redrawpos))) {
				drawcell(mi, wp->redrawpos % wp->bncols - 1,
					 wp->redrawpos / wp->bncols - 1, *(wp->oldcells + wp->redrawpos) - 1);
			}
			if (++(wp->redrawpos) >= wp->bncols * (wp->bnrows - 1)) {
				wp->redrawing = 0;
				break;
			}
		}
	}
}

void
release_wire(ModeInfo * mi)
{
	if (circuits != NULL) {
		int         screen, shade;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			circuitstruct *wp = &circuits[screen];

			free_list(wp);
			if (wp->stippledGC != NULL)
				XFreeGC(MI_DISPLAY(mi), wp->stippledGC);
			if (wp->init_bits != 0)
				for (shade = 0; shade < COLORS - 1; shade++)
					XFreePixmap(MI_DISPLAY(mi), wp->pixmaps[shade]);
			if (wp->oldcells != NULL)
				(void) free((void *) wp->oldcells);
			if (wp->newcells != NULL)
				(void) free((void *) wp->newcells);
		}
		(void) free((void *) circuits);
		circuits = NULL;
	}
}

void
refresh_wire(ModeInfo * mi)
{
	circuitstruct *wp = &circuits[MI_SCREEN(mi)];

	wp->redrawing = 1;
	wp->redrawpos = wp->ncols + 1;
}
