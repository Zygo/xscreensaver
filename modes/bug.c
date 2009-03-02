/* -*- Mode: C; tab-width: 4 -*- */
/* bug --- Michael Palmiter's simulated evolution */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)bug.c	4.07 97/11/24 xlockmore";

#endif

/*-
 * Copyright (c) 1995 by David Bagley.
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
 * 24-Aug-95: Coded from A.K. Dewdney's, "Computer Recreations", Scientific
 *            American Magazine" May 1989 pp138-141 and Sept 1989 p 183.
 *            also used wator.c as a guide.
 */

/*-
 * Bugs learn to hunt bacteria (or die) in the Garden of Eden and outside.
 * They start as jitterbugs and "evolve" to move straight (in the Garden
 *  they may evolve to twirl around).
 */

#ifdef STANDALONE
#define PROGCLASS "Bug"
#define HACK_INIT init_bug
#define HACK_DRAW draw_bug
#define bug_opts xlockmore_opts
#define DEFAULTS "*delay: 75000 \n" \
 "*count: 10 \n" \
 "*cycles: 32767 \n" \
 "*size: -4 \n" \
 "*ncolors: 64 \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */

#endif /* STANDALONE */
#include "automata.h"

#ifdef MODE_bug

#define DEF_NEIGHBORS  "0"      /* choose random value */
#define DEF_EYES  "False"

static int  neighbors;
static Bool eyes;

static XrmOptionDescRec opts[] =
{
	{"-neighbors", ".bug.neighbors", XrmoptionSepArg, (caddr_t) NULL},
	{"-eyes", ".bug.eyes", XrmoptionNoArg, (caddr_t) "on"},
	{"+eyes", ".bug.eyes", XrmoptionNoArg, (caddr_t) "off"},
};
static argtype vars[] =
{
	{(caddr_t *) & neighbors, "neighbors", "Neighbors", DEF_NEIGHBORS, t_Int},
	{(caddr_t *) & eyes, "eyes", "Eyes", DEF_EYES, t_Bool},
};
static OptionStruct desc[] =
{
	{"-neighbors num", "squares 4 or 8, hexagons 6"},
	{"-/+eyes", "turn on/off eyes"}
};

ModeSpecOpt bug_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};


#ifdef USE_MODULES
ModStruct   bug_description =
{"bug", "init_bug", "draw_bug", "release_bug",
 "refresh_bug", "init_bug", NULL, &bug_opts,
 75000, 10, 32767, -4, 64, 1.0, "",
 "Shows Palmiter's bug evolution and garden of Eden", 0, NULL};

#endif

#if DEBUG
#include <assert.h>
#endif

#define BUGBITS(n,w,h)\
  bp->pixmaps[bp->init_bits++]=\
  XCreatePixmapFromBitmapData(display,window,(char *)n,w,h,1,0,1)

#define BACTERIA 0
#define BUG 1

#define MAXENERGY 1500		/* Max energy storage */
#define INITENERGY 400		/* Initial energy */
#define FOODPERCYCLE 1
#define BACTERIAENERGY 40
#define STRONG 1000		/* Min energy to breed */
#define MATURE 800		/* Breeding time of bugs */
#define MAXGENE 6
#define MINGENE (-MAXGENE)
#define REDRAWSTEP 2000		/* How many bacteria to draw per cycle */
#define MINGRIDSIZE 24
#define MINSIZE 1
#define ANGLES 360
#define MAXNEIGHBORS 12

/* Relative hex bug moves */
/* Forward, Right, Hard Right, Reverse, Hard Left, Left */

/* Bug data */
typedef struct {
	char        direction;
	int         age, energy;
	unsigned int color;
	int         col, row;
	int         gene[MAXNEIGHBORS];
	double      gene_prob[MAXNEIGHBORS];
#if EXTRA_GENES
	int         lgene[MAXNEIGHBORS];
	double      lgene_prob[MAXNEIGHBORS];
#endif
} bugstruct;

/* Doublely linked list */
typedef struct _BugList {
	bugstruct   info;
	struct _BugList *previous, *next;
} BugList;

typedef struct {
	Bool        painted;
	int         neighbors;
	int         nbugs;	/* Number of bugs */
	int         eden;	/* Does the garden exist? */
	int         generation;
	int         ncols, nrows;
	int         width, height;
	int         edenwidth, edenheight;
	int         edenstartx, edenstarty;
	int         xs, ys, xb, yb;
	int         redrawing, redrawpos;
	int         eyes;
	BugList    *currbug, *babybug, *lastbug, *firstbug, *lasttemp, *firsttemp;
	BugList   **arr;	/* 0=empty or pts to a bug */
	char       *bacteria;	/* 0=empty or food */
	int         init_bits;
	GC          stippledGC;
	Pixmap      pixmaps[NUMSTIPPLES - 1];
	union {
		XPoint      hexagon[6];
		XPoint      triangle[2][3];
	} shape;
} bugfarmstruct;

static double genexp[MAXGENE - MINGENE + 1];

static char plots[] =
{3, 4, 6, 8,
#ifdef NUMBER_9
 9,
#endif
 12};		/* Neighborhoods */

#define NEIGHBORKINDS (long) (sizeof plots / sizeof *plots)

static bugfarmstruct *bugfarms = NULL;

#if 0
static void
position_of_neighbor(bugfarmstruct * bp, int dir, int *pcol, int *prow)
{
	int         col = *pcol, row = *prow;

	/* NO WRAPING */

	if (bp->neighbors == 6) {
		switch (dir) {
			case 0:
				col = col + 1;
				break;
			case 60:
				if (!(row & 1))
					col = col + 1;
				row = row - 1;
				break;
			case 120:
				if (row & 1)
					col = col - 1;
				row = row - 1;
				break;
			case 180:
				col = col - 1;
				break;
			case 240:
				if (row & 1)
					col = col - 1;
				row = row + 1;
				break;
			case 300:
				if (!(row & 1))
					col = col + 1;
				row = row + 1;
				break;
			default:
				(void) fprintf(stderr, "wrong direction %d\n", dir);
		}
	} else if (bp->neighbors == 4 || bp->neighbors == 8) {
		switch (dir) {
			case 0:
				col = col + 1;
				break;
			case 45:
				col = col + 1;
				row = row - 1;
				break;
			case 90:
				row = row - 1;
				break;
			case 135:
				col = col - 1;
				row = row - 1;
				break;
			case 180:
				col = col - 1;
				break;
			case 225:
				col = col - 1;
				row = row + 1;
				break;
			case 270:
				row = row + 1;
				break;
			case 315:
				col = col + 1;
				row = row + 1;
				break;
			default:
				(void) fprintf(stderr, "wrong direction %d\n", dir);
		}
	} else {		/* TRI */
		switch (dir) {
			case 0:
				col = col + 1;
				break;
			case 20:
			case 30:
			case 40:
				col = col + 1;
				row = row - 1;
				break;
			case 60:
				if ((col + row) % 2) {	/* right */
					row = row - 1;
				} else { /* left */
					col = col + 1;
					row = row - 2;
				}
				break;
			case 80:
			case 90:
			case 100:
				row = row - 2;
				break;
			case 120:
				if ((col + row) % 2) {	/* right */
					col = col - 1;
					row = row - 2;
				} else { /* left */
					row = row - 1;
				}
				break;
			case 140:
			case 150:
			case 160:
				col = col - 1;
				row = row - 1;
				break;
			case 180:
				col = col - 1;
				break;
			case 200:
			case 210:
			case 220:
				col = col - 1;
				row = row + 1;
				break;
			case 240:
				if ((col + row) % 2) {	/* right */
					col = col - 1;
					row = row + 2;
				} else { /* left */
					row = row + 1;
				}
				break;
			case 260:
			case 270:
			case 280:
				row = row + 2;
				break;
			case 300:
				if ((col + row) % 2) {	/* right */
					row = row + 1;
				} else { /* left */
					col = col + 1;
					row = row + 2;
				}
				break;
			case 320:
			case 330:
			case 340:
				col = col + 1;
				row = row + 1;
				break;
			default:
				(void) fprintf(stderr, "wrong direction %d\n", dir);
		}
	}
	*pcol = col;
	*prow = row;
}

#endif

#if 0
static void
screen2grid(bugfarmstruct *bp, int sx, int sy, int x, int y)
{
	*y = sy / 2;
	if (bp->neighbors == 6) {
		*x = (sx + 1) / 2 - 1;
	} else if (bp->neighbors == 4 || bp->neighbors == 8) {
		*x = sx / 2;
	} else {		/* TRI */
		/* Wrong but... */
		*x = sx / 3;
	}
}

#endif

static void
grid2screen(bugfarmstruct *bp, int x, int y, int *sx, int *sy)
{
	*sy = y * 2 + 1;
	if (bp->neighbors == 6) {
		*sx = 2 * x + 1 + !(y & 1);
	} else if (bp->neighbors == 4 || bp->neighbors == 8) {
		*sx = x * 2 + 1;
	} else {		/* TRI */
		*sx = x * 3 + 1 + ((y & 1) ? (x & 1) : !(x & 1));
	}
}

static char
dirmove(bugfarmstruct * bp, int x, int y, int dir, int *nx, int *ny)
{
	if (bp->neighbors == 6) {
		switch (dir) {
			case 0:
				*ny = y;
				*nx = x + 1;
				return (*nx < bp->ncols - !(*ny & 1));
			case 60:
				*ny = y - 1;
				*nx = x + (int) !(y & 1);
				return (*ny >= 0 && *nx < bp->ncols - !(*ny & 1));
			case 120:
				*ny = y - 1;
				*nx = x - (int) (y & 1);
				return (*ny >= 0 && *nx >= 0);
			case 180:
				*ny = y;
				*nx = x - 1;
				return (*nx >= 0);
			case 240:
				*ny = y + 1;
				*nx = x - (int) (y & 1);
				return (*ny < bp->nrows && *nx >= 0);
			case 300:
				*ny = y + 1;
				*nx = x + (int) !(y & 1);
				return (*ny < bp->nrows && *nx < bp->ncols - !(*ny & 1));
			default:
				(void) fprintf(stderr, "wrong direction %d\n", dir);
		}
	} else if (bp->neighbors == 4 || bp->neighbors == 8) {
		switch (dir) {
			case 0:
				*ny = y;
				*nx = x + 1;
				return (*nx < bp->ncols);
			case 45:
				*ny = y - 1;
				*nx = x + 1;
				return (*ny >= 0 && *nx < bp->ncols);
			case 90:
				*ny = y - 1;
				*nx = x;
				return (*ny >= 0);
			case 135:
				*ny = y - 1;
				*nx = x - 1;
				return (*ny >= 0 && *nx >= 0);
			case 180:
				*ny = y;
				*nx = x - 1;
				return (*nx >= 0);
			case 225:
				*ny = y + 1;
				*nx = x - 1;
				return (*ny < bp->nrows && *nx >= 0);
			case 270:
				*ny = y + 1;
				*nx = x;
				return (*ny < bp->nrows);
			case 315:
				*ny = y + 1;
				*nx = x + 1;
				return (*ny < bp->nrows && *nx < bp->ncols);
			default:
				(void) fprintf(stderr, "wrong direction %d\n", dir);
		}
	} else {		/* TRI */
		switch (dir) {
			case 0:
				*nx = x + 1;
				*ny = y;
				return (*nx < bp->ncols);
			case 20:
			case 30:
			case 40:
				*nx = x + 1;
				*ny = y - 1;
				return (*nx < bp->ncols && *ny >= 0);
			case 60:
				if ((x + y) % 2) {  /* right */
					*nx = x;
					*ny = y - 1;
					return (*ny >= 0);
				} else { /* left */
					*nx = x + 1;
					*ny = y - 2;
					return (*nx < bp->ncols && *ny >= 0);
				}
			case 80:
			case 90:
			case 100:
				*nx = x;
				*ny = y - 2;
				return (*ny >= 0);
			case 120:
				if ((x + y) % 2) {  /* right */
					*nx = x - 1;
					*ny = y - 2;
					return (*nx >= 0 && *ny >= 0);
				} else { /* left */
					*nx = x;
					*ny = y - 1;
					return (*ny >= 0);
				}
			case 140:
			case 150:
			case 160:
				*nx = x - 1;
				*ny = y - 1;
				return (*nx >= 0 && *ny >= 0);
			case 180:
				*nx = x - 1;
				*ny = y;
				return (*nx >= 0);
			case 200:
			case 210:
			case 220:
				*nx = x - 1;
				*ny = y + 1;
				return (*nx >= 0 && *ny < bp->nrows);
			case 240:
				if ((x + y) % 2) {  /* right */
					*nx = x - 1;
					*ny = y + 2;
					return (*nx >= 0 && *ny < bp->nrows);
				} else { /* left */
					*nx = x;
					*ny = y + 1;
					return (*ny < bp->nrows);
				}
			case 260:
			case 270:
			case 280:
				*nx = x;
				*ny = y + 2;
				return (*ny < bp->nrows);
			case 300:
				if ((x + y) % 2) {  /* right */
					*nx = x;
					*ny = y + 1;
					return (*ny < bp->nrows);
				} else { /* left */
					*nx = x + 1;
					*ny = y + 2;
					return (*nx < bp->ncols && *ny < bp->nrows);
				}
			case 320:
			case 330:
			case 340:
				*nx = x + 1;
				*ny = y + 1;
				return (*nx < bp->ncols && *ny < bp->nrows);
			default:
				(void) fprintf(stderr, "wrong direction %d\n", dir);
		}
	}
	*nx = -1;
	*ny = -1;
	return 0;
}

/* This keeps bugs from walking over each other and escaping off the screen */
static int
has_neighbor(bugfarmstruct * bp, int col, int row)
{
	int         colrow = col + row * bp->ncols;

	if (bp->neighbors == 6) {
		if ((row > 0) && bp->arr[colrow - bp->ncols])
			return True;
		if ((row < bp->nrows - 1) && bp->arr[colrow + bp->ncols])
			return True;
		if (col > 0) {
			if (bp->arr[colrow - 1])
				return True;
			if (row & 1) {
				if ((row > 0) && bp->arr[colrow - 1 - bp->ncols])
					return True;
				if ((row < bp->nrows - 1) && bp->arr[colrow - 1 + bp->ncols])
					return True;
			}
		}
		if (col < bp->ncols - 1) {
			if (bp->arr[colrow + 1])
				return True;
			if (!(row & 1)) {
				if ((row > 0) && bp->arr[colrow + 1 - bp->ncols])
					return True;
				if ((row < bp->nrows - 1) && bp->arr[colrow + 1 + bp->ncols])
					return True;
			}
		}
	} else if (bp->neighbors == 4 || bp->neighbors == 8) {
		/* Same for both because the bug square takes up the space regardless */
		if ((row > 0) && bp->arr[colrow - bp->ncols])
			return True;
		if ((row < bp->nrows - 1) && bp->arr[colrow + bp->ncols])
			return True;
		if ((col > 0) && bp->arr[colrow - 1])
			return True;
		if ((col < bp->ncols - 1) && bp->arr[colrow + 1])
			return True;
		if (row > 0 && col > 0 && bp->arr[colrow - bp->ncols - 1])
			return True;
		if (row > 0 && col < bp->ncols - 1 && bp->arr[colrow - bp->ncols + 1])
			return True;
		if (row < bp->nrows - 1 && col > 0 && bp->arr[colrow + bp->ncols - 1])
			return True;
		if (row < bp->nrows - 1 && col < bp->ncols - 1 && bp->arr[colrow + bp->ncols + 1])
			return True;
	} else {		/* TRI */
		/* Same for all three neighbor kinds. Only care about 3
		   adjacent spots because that is all the squares will
		   overlap.*/
		if (((row & 1) && (col & 1)) || ((!(row & 1)) && (!(col & 1)))) {
			if ((col < bp->ncols - 1) && bp->arr[colrow + 1])
				return True;
		} else {
			if ((col > 0) && bp->arr[colrow - 1])
				return True;
		}
		if ((row > 0) && bp->arr[colrow - bp->ncols])
			return True;
		if ((row < bp->nrows - 1) && bp->arr[colrow + bp->ncols])
			return True;
	}
	return False;
}

static void
drawabacterium(ModeInfo * mi, int col, int row, unsigned char color)
{
	bugfarmstruct *bp = &bugfarms[MI_SCREEN(mi)];
	int         ccol, crow;

	if (color)
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_WHITE_PIXEL(mi));
	else
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_BLACK_PIXEL(mi));
	grid2screen(bp, col, row, &ccol, &crow);
	XFillRectangle(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi),
	       bp->xb + bp->xs * ccol, bp->yb + bp->ys * crow,
	       bp->xs, bp->ys);
}

static void
drawabug(ModeInfo * mi, int col, int row, unsigned int color, int direction)
{
	bugfarmstruct *bp = &bugfarms[MI_SCREEN(mi)];
	Display *display = MI_DISPLAY(mi);
	Window window = MI_WINDOW(mi);
	int         ccol, crow;
	GC          gc;

	if (MI_NPIXELS(mi) > 3) {
		XSetForeground(display, MI_GC(mi), MI_PIXEL(mi, color));
		gc = MI_GC(mi);
	} else {
		XGCValues   gcv;

		gcv.stipple = bp->pixmaps[color];
		gcv.foreground = MI_WHITE_PIXEL(mi);
		gcv.background = MI_BLACK_PIXEL(mi);
		XChangeGC(MI_DISPLAY(mi), bp->stippledGC,
			  GCStipple | GCForeground | GCBackground, &gcv);
		gc = bp->stippledGC;
	}

	grid2screen(bp, col, row, &ccol, &crow);
	ccol = bp->xb + bp->xs * (ccol - 1);
	crow = bp->yb + bp->ys * (crow - 1);
	XFillRectangle(display, window, gc,
		ccol, crow, 3 * bp->xs, 3 * bp->ys);

	if (bp->eyes && bp->xs >= 1 && bp->ys >= 1) { /* Draw Eyes */
		XSetForeground(display, MI_GC(mi), MI_BLACK_PIXEL(mi));
		switch (direction) {
			case 0:
				XDrawPoint(display, window, MI_GC(mi),
					ccol + 3 * bp->xs - 2,
					crow + 3 * bp->ys / 2 - 1);
				XDrawPoint(display, window, MI_GC(mi),
					ccol + 3 * bp->xs - 2,
					crow + 3 * bp->ys / 2 + 1);
					break;
			case 20:
			case 30:
			case 40:
			case 45:
			case 60:
				XDrawPoint(display, window, MI_GC(mi),
					ccol + 3 * bp->xs - 3,
					crow + 1);
				XDrawPoint(display, window, MI_GC(mi),
					ccol + 3 * bp->xs - 2,
					crow + 2);
				break;
			case 80:
			case 90:
			case 100:
				XDrawPoint(display, window, MI_GC(mi),
					ccol + 3 * bp->xs / 2 - 1,
					crow + 1);
				XDrawPoint(display, window, MI_GC(mi),
					ccol + 3 * bp->xs / 2 + 1,
					crow + 1);
				break;
			case 120:
			case 135:
			case 140:
			case 150:
			case 160:
				XDrawPoint(display, window, MI_GC(mi),
					ccol + 2,
					crow + 1);
				XDrawPoint(display, window, MI_GC(mi),
					ccol + 1,
						crow + 2);
				break;
			case 180:
				XDrawPoint(display, window, MI_GC(mi),
					ccol + 1,
					crow + 3 * bp->ys / 2 - 1);
				XDrawPoint(display, window, MI_GC(mi),
					ccol + 1,
					crow + 3 * bp->ys / 2 + 1);
				break;
			case 200:
			case 210:
			case 220:
			case 225:
			case 240:
				XDrawPoint(display, window, MI_GC(mi),
					ccol + 2,
					crow + 3 * bp->ys - 2);
				XDrawPoint(display, window, MI_GC(mi),
					ccol + 1,
					crow + 3 * bp->ys - 3);
				break;
			case 260:
			case 270:
			case 280:
				XDrawPoint(display, window, MI_GC(mi),
					ccol + 3 * bp->xs / 2 - 1,
					crow + 3 * bp->ys - 2);
				XDrawPoint(display, window, MI_GC(mi),
					ccol + 3 * bp->xs / 2 + 1,
					crow + 3 * bp->ys - 2);
				break;
			case 300:
			case 315:
			case 320:
			case 330:
			case 340:
				XDrawPoint(display, window, MI_GC(mi),
					ccol + 3 * bp->xs - 3,
					crow + 3 * bp->ys - 2);
				XDrawPoint(display, window, MI_GC(mi),
					ccol + 3 * bp->xs - 2,
					crow + 3 * bp->ys - 3);
				break;
			default:
				(void) fprintf(stderr, "wrong direction %d\n", direction);
		}
	}
}

static void
eraseabug(ModeInfo * mi, int col, int row)
{
	bugfarmstruct *bp = &bugfarms[MI_SCREEN(mi)];
	int         ccol, crow;

	XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_BLACK_PIXEL(mi));
	grid2screen(bp, col, row, &ccol, &crow);
	XFillRectangle(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi),
		bp->xb + bp->xs * (ccol - 1), bp->yb + bp->ys * (crow - 1),
		3 * bp->xs, 3 * bp->ys);
}

static void
init_buglist(bugfarmstruct * bp)
{
	/* Waste some space at the beginning and end of list
	   so we do not have to complicated checks against falling off the ends. */
	bp->lastbug = (BugList *) malloc(sizeof (BugList));
	bp->firstbug = (BugList *) malloc(sizeof (BugList));
	bp->firstbug->previous = bp->lastbug->next = NULL;
	bp->firstbug->next = bp->lastbug->previous = NULL;
	bp->firstbug->next = bp->lastbug;
	bp->lastbug->previous = bp->firstbug;

	bp->lasttemp = (BugList *) malloc(sizeof (BugList));
	bp->firsttemp = (BugList *) malloc(sizeof (BugList));
	bp->firsttemp->previous = bp->lasttemp->next = NULL;
	bp->firsttemp->next = bp->lasttemp->previous = NULL;
	bp->firsttemp->next = bp->lasttemp;
	bp->lasttemp->previous = bp->firsttemp;
}

static void
addto_buglist(bugfarmstruct * bp, bugstruct info)
{
	bp->currbug = (BugList *) malloc(sizeof (BugList));
	bp->lastbug->previous->next = bp->currbug;
	bp->currbug->previous = bp->lastbug->previous;
	bp->currbug->next = bp->lastbug;
	bp->lastbug->previous = bp->currbug;
	bp->currbug->info = info;
}

static void
removefrom_buglist(bugfarmstruct * bp, BugList * ptr)
{
	ptr->previous->next = ptr->next;
	ptr->next->previous = ptr->previous;
	bp->arr[ptr->info.col + ptr->info.row * bp->ncols] = 0;
	(void) free((void *) ptr);
}

static void
dupin_buglist(bugfarmstruct * bp)
{
	BugList    *temp;

	temp = (BugList *) malloc(sizeof (BugList));
	temp->previous = bp->babybug;
	temp->next = bp->babybug->next;
	bp->babybug->next = temp;
	temp->next->previous = temp;
	temp->info = bp->babybug->info;
	bp->babybug = temp;
}

/*-
 * new bug at end of list, this rotates who goes first, young bugs go last
 * this most likely will not change the feel to any real degree
 */
static void
cutfrom_buglist(bugfarmstruct * bp)
{
	bp->babybug = bp->currbug;
	bp->currbug = bp->currbug->previous;
	bp->currbug->next = bp->babybug->next;
	bp->babybug->next->previous = bp->currbug;
	bp->babybug->next = bp->lasttemp;
	bp->babybug->previous = bp->lasttemp->previous;
	bp->babybug->previous->next = bp->babybug;
	bp->babybug->next->previous = bp->babybug;
}

static void
reattach_buglist(bugfarmstruct * bp)
{
	bp->currbug = bp->lastbug->previous;
	bp->currbug->next = bp->firsttemp->next;
	bp->currbug->next->previous = bp->currbug;
	bp->lastbug->previous = bp->lasttemp->previous;
	bp->lasttemp->previous->next = bp->lastbug;
	bp->lasttemp->previous = bp->firsttemp;
	bp->firsttemp->next = bp->lasttemp;
}

static void
flush_buglist(bugfarmstruct * bp)
{
	while (bp->lastbug->previous != bp->firstbug) {
		bp->currbug = bp->lastbug->previous;
		bp->currbug->previous->next = bp->lastbug;
		bp->lastbug->previous = bp->currbug->previous;
		/*bp->arr[bp->currbug->info.col + bp->currbug->info.row * bp->ncols] = 0; */
		(void) free((void *) bp->currbug);
	}

	while (bp->lasttemp->previous != bp->firsttemp) {
		bp->currbug = bp->lasttemp->previous;
		bp->currbug->previous->next = bp->lasttemp;
		bp->lasttemp->previous = bp->currbug->previous;
		/*bp->arr[bp->currbug->info.col + bp->currbug->info.row * bp->ncols] = 0; */
		(void) free((void *) bp->currbug);
	}
}

static int
dirbug(bugstruct * info, int local_neighbors)
{
	double      sum = 0.0, prob;
	int         i;

	prob = (double) LRAND() / MAXRAND;
#if EXTRA_GENES
	if ((local_neighbors % 2) && !((info->col + info->row) & 1)) {
		for (i = 0; i < local_neighbors; i++) {
			sum += info->lgene_prob[i];
			if (prob < sum)
				return i;
		}
	} else
#endif
	{
		for (i = 0; i < local_neighbors; i++) {
			sum += info->gene_prob[i];
			if (prob < sum)
				return i;
		}
	}
	return local_neighbors - 1;	/* Could miss due to rounding */
}

static void
mutatebug(bugstruct * info, int local_neighbors)
{
	double      sum = 0.0;
	int         gene;

	gene = NRAND(local_neighbors);
	if (LRAND() & 1) {
		if (info->gene[gene] == MAXGENE)
			return;
		info->gene[gene]++;
	} else {
		if (info->gene[gene] == MINGENE)
			return;
		info->gene[gene]--;
	}
	for (gene = 0; gene < local_neighbors; gene++)
		sum += genexp[info->gene[gene] + MAXGENE];
	for (gene = 0; gene < local_neighbors; gene++)
		info->gene_prob[gene] = genexp[info->gene[gene] + MAXGENE] / sum;
#if EXTRA_GENES
	if (local_neighbors % 2) {
		sum = 0.0;
		gene = NRAND(local_neighbors);
		if (LRAND() & 1) {
			if (info->lgene[gene] == MAXGENE)
				return;
			info->lgene[gene]++;
		} else {
			if (info->lgene[gene] == MINGENE)
				return;
			info->lgene[gene]--;
		}
		for (gene = 0; gene < local_neighbors; gene++)
			sum += genexp[info->lgene[gene] + MAXGENE];
		for (gene = 0; gene < local_neighbors; gene++)
			info->lgene_prob[gene] = genexp[info->lgene[gene] + MAXGENE] / sum;
	}
#endif
}

static void
makebacteria(ModeInfo * mi,
	     int n, int startx, int starty, int width, int height, Bool draw)
{
	int         nbacteria = 0, ntries = 0, col = 0, row, colrow;
	bugfarmstruct *bp = &bugfarms[MI_SCREEN(mi)];

	/* Make bacteria but if can not, exit */
	while (nbacteria < n && ntries < 2 * n) {
		row = NRAND(height) + starty;
		if (bp->neighbors == 6) {
			if (width - (!(row & 1)) > 0)
				col = NRAND(width - (!(row & 1))) + startx;
			else
				col = startx;
		} else { /* RECT or TRI */
			col = NRAND(width) + startx;
		}
		colrow = col + row * bp->ncols;
		ntries++;
		if (!bp->arr[colrow] && !bp->bacteria[colrow]) {
			nbacteria++;
			bp->bacteria[colrow] = True;
			drawabacterium(mi, col, row, draw);
		}
	}
}

static void
redrawbacteria(ModeInfo * mi, int colrow)
{
	int         col, row;
	bugfarmstruct *bp = &bugfarms[MI_SCREEN(mi)];

	if (!bp->bacteria[colrow])
		return;

	row = colrow / bp->ncols;
	col = colrow % bp->ncols;
	drawabacterium(mi, col, row, True);
}

void
init_bug(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         size = MI_SIZE(mi);
	XGCValues   gcv;
	bugfarmstruct *bp;
	int         nbugs = 0, ntries = 0, col = 0, row, gene, colrow, i;
	int         nccols, ncrows;
	double      sum;
	bugstruct   info;

	if (bugfarms == NULL) {
		if ((bugfarms = (bugfarmstruct *) calloc(MI_NUM_SCREENS(mi),
					    sizeof (bugfarmstruct))) == NULL)
			return;
	}
	bp = &bugfarms[MI_SCREEN(mi)];
	if (MI_NPIXELS(mi) <= 3) {
		if (bp->stippledGC == None) {
			gcv.fill_style = FillOpaqueStippled;
			bp->stippledGC = XCreateGC(display, window, GCFillStyle, &gcv);
		}
		if (bp->init_bits == 0) {
			for (i = 1; i < NUMSTIPPLES; i++)
				BUGBITS(stipples[i], STIPPLESIZE, STIPPLESIZE);
		}
	}
	bp->generation = 0;
	if (!bp->firstbug) {	/* Genesis */
		/* Set up what will be a 'triply' linked list.
		   doubly linked list, doubly linked to an array */
		init_buglist(bp);
		genexp[MAXGENE] = 1;
		for (i = 1; i <= MAXGENE; i++) {
			genexp[MAXGENE + i] = genexp[MAXGENE + i - 1] * M_E;
			genexp[MAXGENE - i] = 1.0 / genexp[MAXGENE + i];
		}
	} else			/* Apocolypse now */
		flush_buglist(bp);

	for (i = 0; i < NEIGHBORKINDS; i++) {
		if (neighbors == plots[i]) {
			bp->neighbors = plots[i];
			break;
		}
		if (i == NEIGHBORKINDS - 1) {
			bp->neighbors = plots[NRAND(NEIGHBORKINDS)];
			break;
		}
	}

	bp->width = MI_WIDTH(mi);
	bp->height = MI_HEIGHT(mi);
	if (bp->width < 4)
		bp->width = 4;
	if (bp->height < 4)
		bp->height = 4;

	if (size < -MINSIZE)
		bp->ys = NRAND(MIN(-size, MAX(MINSIZE, MIN(bp->width, bp->height) /
				      MINGRIDSIZE)) - MINSIZE + 1) + MINSIZE;
	else if (size < MINSIZE) {
		if (!size)
			bp->ys = MAX(MINSIZE, MIN(bp->width, bp->height) / MINGRIDSIZE);
		else
			bp->ys = MINSIZE;
	} else
		bp->ys = MIN(size, MAX(MINSIZE, MIN(bp->width, bp->height) /
				       MINGRIDSIZE));
	bp->xs = bp->ys;
	nccols = MAX(bp->width / bp->xs - 2, 2);
	ncrows = MAX(bp->height / bp->ys - 1, 2);
	bp->nrows = ncrows / 2;
	if ((bp->neighbors % 2) || bp->neighbors == 12) {
		bp->ncols = nccols / 3;
		if (bp->ncols & 1)
			bp->ncols--;	/* Must be odd for no wrap */
		if (!(bp->nrows & 1))
			bp->nrows--;	/* Must be even for no wrap */
	} else
		bp->ncols = nccols / 2;
	if (bp->neighbors == 6 && !(bp->nrows & 1))
		bp->nrows--;	/* Must be odd for no wrap */
	bp->xb = (bp->width - bp->xs * nccols) / 2;
	bp->yb = (bp->height - bp->ys * ncrows) / 2;
	if (bp->arr != NULL)
		(void) free((void *) bp->arr);
	bp->arr = (BugList **) calloc(bp->ncols * bp->nrows, sizeof (BugList *));
	if (bp->bacteria != NULL)
		(void) free((void *) bp->bacteria);
	bp->bacteria = (char *) calloc(bp->ncols * bp->nrows, sizeof (char));

	bp->edenheight = bp->nrows / 4;
	bp->edenwidth = bp->ncols / 4;
	if ((bp->neighbors % 2) || bp->neighbors == 12) {
		if (bp->edenwidth & 1)
			bp->edenwidth--;
		if (!(bp->edenheight & 1))
			bp->edenheight--;
	} else if (bp->neighbors == 6 && bp->edenheight && !(bp->edenheight & 1))
		bp->edenheight--;	/* Make sure its odd */
	bp->eden = (bp->edenwidth > 1 && bp->edenheight > 1);
	if (bp->eden) {
		bp->edenstartx = NRAND(bp->ncols - bp->edenwidth);
		bp->edenstarty = NRAND(bp->nrows - bp->edenheight);
		if ((bp->neighbors % 2) || bp->neighbors == 12) {
			if (bp->edenstartx & 1) {
				bp->edenstartx--;
				bp->eden = (bp->edenwidth > 1);
			}
			if (bp->edenstarty & 1) {
				bp->edenstarty--;
				bp->eden = (bp->edenheight > 1);
			}
		} else if (bp->neighbors == 6 && (bp->edenstarty & 1)) {
			bp->edenstarty--;
			bp->eden = (bp->edenheight > 1);
		}
	}
	/* Play G-d with these numbers... the genes */
	nbugs = MI_COUNT(mi);
	if (nbugs < 0)
		nbugs = NRAND(-nbugs) + 1;
	bp->nbugs = 0;

	MI_CLEARWINDOW(mi);
	bp->painted = False;

	if (MI_IS_FULLRANDOM(mi)) {
		bp->eyes = (Bool) (LRAND() & 1);
	} else {
		bp->eyes = eyes;
	}
	bp->eyes = (bp->eyes && bp->xs > 1 && bp->ys > 1);

	/* Try to make bugs, but if can not, exit */
	while (bp->nbugs < nbugs && ntries < 2 * nbugs) {
		row = NRAND(bp->nrows);
		if (bp->neighbors == 6) {
			if (bp->ncols - (!(row & 1)) > 0)
				col = NRAND(bp->ncols - (!(row & 1)));
			else
				col = 0;
		} else { /* RECT or TRI */
			col = NRAND(bp->ncols);
		}
		colrow = col + row * bp->ncols;
		ntries++;
		if (!bp->arr[colrow] && !has_neighbor(bp, col, row)) {
			bp->nbugs++;
			info.age = 0;
			info.energy = INITENERGY;
			info.direction = NRAND(bp->neighbors);
			for (gene = 0; gene < bp->neighbors; gene++)
#if 0
				/* Some creationalism, may create gliders or twirlers */
				do {
					double      temp = (double) LRAND() / MAXRAND;

					info.gene[gene] = ((int) (1.0 / (1.0 - temp * temp)) - 1) *
						((LRAND() & 1) ? -1 : 1);
				} while (info.gene[gene] > MAXGENE / 2 ||
					 info.gene[gene] < MINGENE / 2);
#else /* Jitterbugs, evolve or die */
				info.gene[gene] = 0;
#endif
			sum = 0;
			for (gene = 0; gene < bp->neighbors; gene++)
				sum += genexp[info.gene[gene] + MAXGENE];
			for (gene = 0; gene < bp->neighbors; gene++)
				info.gene_prob[gene] = genexp[info.gene[gene] + MAXGENE] / sum;
#if EXTRA_GENES
			if (bp->neighbors % 2) {
				for (gene = 0; gene < bp->neighbors; gene++)
#if 0
				/* Some creationalism, may create gliders or twirlers */
					do {
						double      temp = (double) LRAND() / MAXRAND;

						info.lgene[gene] = ((int) (1.0 / (1.0 - temp * temp)) - 1) *
							((LRAND() & 1) ? -1 : 1);
					} while (info.lgene[gene] > MAXGENE / 2 ||
						 info.lgene[gene] < MINGENE / 2);
#else /* Jitterbugs, evolve or die */
					info.lgene[gene] = 0;
#endif
				sum = 0;
				for (gene = 0; gene < bp->neighbors; gene++)
					sum += genexp[info.lgene[gene] + MAXGENE];
				for (gene = 0; gene < bp->neighbors; gene++)
					info.lgene_prob[gene] = genexp[info.lgene[gene] + MAXGENE] / sum;
			}
#endif
			if (MI_NPIXELS(mi) > 3)
				info.color = NRAND(MI_NPIXELS(mi));
			else
				info.color = NRAND(NUMSTIPPLES - 1);
			info.col = col;
			info.row = row;
			addto_buglist(bp, info);
			bp->arr[colrow] = bp->currbug;
			drawabug(mi, col, row, bp->currbug->info.color, (int) bp->currbug->info.direction * ANGLES / bp->neighbors);
		}
	}
	makebacteria(mi, bp->ncols * bp->nrows / 2, 0, 0, bp->ncols, bp->nrows,
		     False);
	if (bp->eden)
		makebacteria(mi, bp->edenwidth * bp->edenheight / 2,
		bp->edenstartx, bp->edenstarty, bp->edenwidth, bp->edenheight, False);
	bp->redrawing = 1;
	bp->redrawpos = 0;
}

#define ENOUGH 16
void
draw_bug(ModeInfo * mi)
{
	bugfarmstruct *bp = &bugfarms[MI_SCREEN(mi)];
	int         col, row, ncol = -1, nrow = -1, colrow, ncolrow;
	int         absdir = 0, tryit, dir;

	MI_IS_DRAWN(mi) = True;

	bp->painted = True;
	bp->currbug = bp->firstbug->next;
	while (bp->currbug != bp->lastbug) {
		col = bp->currbug->info.col;
		row = bp->currbug->info.row;
		colrow = col + row * bp->ncols;
		if (bp->currbug->info.energy-- < 0) {	/* Time to die, Bug */
#if DEBUG
			assert(bp->currbug == bp->arr[colrow]);
#endif
			/* back up one or else in void */
			bp->currbug = bp->currbug->previous;
			removefrom_buglist(bp, bp->arr[colrow]);
#if DEBUG
			assert(bp->arr[colrow] == 0);
#endif
			bp->arr[colrow] = 0;
			eraseabug(mi, col, row);
			bp->nbugs--;
		} else {	/* try to move */
			bp->arr[colrow] = 0;	/* Don't want neighbor to detect itself */
			tryit = 0;
			do {
				if (tryit++ > ENOUGH) {
					break;
				}
				absdir = (bp->currbug->info.direction +
					  dirbug(&bp->currbug->info, bp->neighbors)) % bp->neighbors;
				dir = absdir * ANGLES / bp->neighbors;
				if (bp->neighbors % 2) {
					if ((col + row) & 1)
						dir = (3 * ANGLES / 2 - dir) % ANGLES;
					else
						dir = (ANGLES - dir) % ANGLES;
				}
			} while (!dirmove(bp, col, row, dir,
			  &ncol, &nrow) || has_neighbor(bp, ncol, nrow) ||
			  bp->arr[ncol + nrow * bp->ncols]);
			bp->currbug->info.age++;
			bp->currbug->info.energy--;
			if (tryit <= ENOUGH) {
				ncolrow = ncol + nrow * bp->ncols;
				if (bp->bacteria[ncolrow]) {
					bp->currbug->info.energy += BACTERIAENERGY;
					bp->bacteria[ncolrow] = 0;
					if (bp->currbug->info.energy > MAXENERGY)
						bp->currbug->info.energy = MAXENERGY;
				}
				bp->currbug->info.col = ncol;
				bp->currbug->info.row = nrow;
				bp->currbug->info.direction = absdir;
#if DEBUG
				assert(bp->arr[ncolrow] == 0);
#endif
				bp->arr[ncolrow] = bp->currbug;
				if (bp->currbug->info.energy > STRONG &&
				    bp->currbug->info.age > MATURE) {	/* breed */
					drawabug(mi, ncol, nrow, bp->currbug->info.color, (int) bp->currbug->info.direction * ANGLES / bp->neighbors);
					cutfrom_buglist(bp);	/* This rotates out who goes first */
					bp->babybug->info.age = 0;
					bp->babybug->info.energy = INITENERGY;
					dupin_buglist(bp);
					mutatebug(&bp->babybug->previous->info, bp->neighbors);
					mutatebug(&bp->babybug->info, bp->neighbors);
					bp->arr[colrow] = bp->babybug;
					bp->babybug->info.col = col;
					bp->babybug->info.row = row;
					bp->nbugs++;
				} else {
					eraseabug(mi, col, row);
					drawabug(mi, ncol, nrow, bp->currbug->info.color, (int) bp->currbug->info.direction * ANGLES / bp->neighbors);
				}
			} else
				bp->arr[colrow] = bp->currbug;
		}
		bp->currbug = bp->currbug->next;
	}
	reattach_buglist(bp);
	makebacteria(mi, FOODPERCYCLE, 0, 0, bp->ncols, bp->nrows, True);
	if (bp->eden)
		makebacteria(mi, FOODPERCYCLE,
			     bp->edenstartx, bp->edenstarty, bp->edenwidth, bp->edenheight, True);
	if (!bp->nbugs || bp->generation >= MI_CYCLES(mi))
		init_bug(mi);
	bp->generation++;
	if (bp->redrawing) {
		int         i;

		for (i = 0; i < REDRAWSTEP; i++) {
			if (bp->bacteria[bp->redrawpos])
				redrawbacteria(mi, bp->redrawpos);
			if (++(bp->redrawpos) >= bp->ncols * bp->nrows) {
				bp->redrawing = 0;
				break;
			}
		}
	}
}

void
release_bug(ModeInfo * mi)
{
	if (bugfarms != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			bugfarmstruct *bp = &bugfarms[screen];
			int         shade;

			if (bp->stippledGC != None) {
				XFreeGC(MI_DISPLAY(mi), bp->stippledGC);
			}
			for (shade = 0; shade < bp->init_bits; shade++) {
				if (bp->pixmaps[shade])
					XFreePixmap(MI_DISPLAY(mi), bp->pixmaps[shade]);
			}
			if (bp->firstbug)
				flush_buglist(bp);
			(void) free((void *) bp->lastbug);
			(void) free((void *) bp->firstbug);
			(void) free((void *) bp->lasttemp);
			(void) free((void *) bp->firsttemp);
			if (bp->arr != NULL)
				(void) free((void *) bp->arr);
			if (bp->bacteria != NULL)
				(void) free((void *) bp->bacteria);
		}
		(void) free((void *) bugfarms);
		bugfarms = NULL;
	}
}

void
refresh_bug(ModeInfo * mi)
{
	bugfarmstruct *bp = &bugfarms[MI_SCREEN(mi)];

	if (bp->painted) {
		MI_CLEARWINDOW(mi);
		bp->painted = False;
		bp->redrawing = 1;
		bp->redrawpos = 0;
	}
}

#endif /* MODE_bug */
