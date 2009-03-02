
/* -*- Mode: C; tab-width: 4 -*- */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)bug.c	4.03 97/05/10 xlockmore";

#endif

/*-
 * bug.c - Michael Palmiter's simulated evolution for xlock,
 *           the X Window System lockscreen.
 *
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
#define PROGCLASS            "Bug"
#define HACK_INIT            init_bug
#define HACK_DRAW            draw_bug
#define DEF_DELAY            75000
#define DEF_BATCHCOUNT       10
#define DEF_CYCLES           32767
#define DEF_SIZE             -4
#define DEF_NCOLORS          200
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
ModeSpecOpt bug_opts =
{0, NULL, 0, NULL, NULL};

#endif /* STANDALONE */

#define BACTERIA 0
#define BUG 1
#define ORIENTS 6

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

/* Relative bug moves */
/* Forward, Right, Hard Right, Reverse, Hard Left, Left */

/* Compass bug moves */
#define NE 0
#define E 1
#define SE 2
#define SW 3
#define W 4
#define NW 5

/* Bug data */
typedef struct {
	char        direction;
	int         age, energy;
	unsigned long color;
	int         col, row;
	int         gene[ORIENTS];
	double      gene_prob[ORIENTS];
} bugstruct;

/* Doublely linked list */
typedef struct _BugList {
	bugstruct   info;
	struct _BugList *previous, *next;
} BugList;

typedef struct {
	int         initialized;
	int         n;		/* Number of bugs */
	int         eden;	/* Does the garden exist? */
	int         generation;
	int         nhcols, nhrows;
	int         nccols, ncrows;
	int         width, height;
	int         edenwidth, edenheight;
	int         edenstartx, edenstarty;
	int         xs, ys, xb, yb;
	int         redrawing, redrawpos;
	BugList    *currbug, *babybug, *lastbug, *firstbug, *lasttemp, *firsttemp;
	BugList   **arr;	/* 0=empty or pts to a bug */
	char       *bacteria;	/* 0=empty or food */
} bugfarmstruct;

static double genexp[MAXGENE - MINGENE + 1];
static bugfarmstruct *bugfarms = NULL;

#if 0
static void
cart2hex(cx, cy, hx, hy)
	int         cx, cy, *hx, *hy;
{
	*hy = cy / 2;
	*hx = (cx + 1) / 2 - 1;
}

#endif

static void
hex2cart(int hx, int hy, int *cx, int *cy)
{
	*cy = hy * 2 + 1;
	*cx = 2 * hx + 1 + !(hy & 1);
}

/* hexagon compass move */
static char
hexcmove(bugfarmstruct * bp, int hx, int hy, int dir, int *nhx, int *nhy)
{
	switch (dir) {
		case NE:
			*nhy = hy - 1;
			*nhx = hx + (int) !(hy & 1);
			return (*nhy >= 0 && *nhx < bp->nhcols - !(*nhy & 1));
		case E:
			*nhy = hy;
			*nhx = hx + 1;
			return (*nhx < bp->nhcols - !(*nhy & 1));
		case SE:
			*nhy = hy + 1;
			*nhx = hx + (int) !(hy & 1);
			return (*nhy < bp->nhrows && *nhx < bp->nhcols - !(*nhy & 1));
		case SW:
			*nhy = hy + 1;
			*nhx = hx - (int) (hy & 1);
			return (*nhy < bp->nhrows && *nhx >= 0);
		case W:
			*nhy = hy;
			*nhx = hx - 1;
			return (*nhx >= 0);
		case NW:
			*nhy = hy - 1;
			*nhx = hx - (int) (hy & 1);
			return (*nhy >= 0 && *nhx >= 0);
		default:
			return 0;
	}
}

static int
neighbor(bugfarmstruct * bp, int hcol, int hrow)
{
	int         colrow = hcol + hrow * bp->nhcols;

	if ((hrow > 0) && (bp->arr[colrow - bp->nhcols]))
		return True;
	if ((hrow < bp->nhrows - 1) && bp->arr[colrow + bp->nhcols])
		return True;
	if (hcol > 0) {
		if (bp->arr[colrow - 1])
			return True;
		if (hrow & 1) {
			if ((hrow > 0) && bp->arr[colrow - 1 - bp->nhcols])
				return True;
			if ((hrow < bp->nhrows - 1) && bp->arr[colrow - 1 + bp->nhcols])
				return True;
		}
	}
	if (hcol < bp->nhcols - 1) {
		if (bp->arr[colrow + 1])
			return True;
		if (!(hrow & 1)) {
			if ((hrow > 0) && bp->arr[colrow + 1 - bp->nhcols])
				return True;
			if ((hrow < bp->nhrows - 1) && bp->arr[colrow + 1 + bp->nhcols])
				return True;
		}
	}
	return False;
}

static void
drawabacterium(ModeInfo * mi, int hcol, int hrow, char color)
{
	bugfarmstruct *bp = &bugfarms[MI_SCREEN(mi)];
	int         ccol, crow;

	if (color)
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_WIN_WHITE_PIXEL(mi));
	else
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_WIN_BLACK_PIXEL(mi));
	hex2cart(hcol, hrow, &ccol, &crow);
	XFillRectangle(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi),
		       bp->xb + bp->xs * ccol, bp->yb + bp->ys * crow,
		       bp->xs, bp->ys);
}

static void
drawabug(ModeInfo * mi, int hcol, int hrow, unsigned long color)
{
	bugfarmstruct *bp = &bugfarms[MI_SCREEN(mi)];
	int         ccol, crow;

	if (MI_NPIXELS(mi) > 2)
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_PIXEL(mi, color));
	else
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_WIN_WHITE_PIXEL(mi));
	hex2cart(hcol, hrow, &ccol, &crow);
	XFillRectangle(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi),
		  bp->xb + bp->xs * (ccol - 1), bp->yb + bp->ys * (crow - 1),
		       3 * bp->xs, 3 * bp->ys);
}


static void
eraseabug(ModeInfo * mi, int hcol, int hrow)
{
	bugfarmstruct *bp = &bugfarms[MI_SCREEN(mi)];
	int         ccol, crow;

	XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_WIN_BLACK_PIXEL(mi));
	hex2cart(hcol, hrow, &ccol, &crow);
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
	bp->arr[ptr->info.col + ptr->info.row * bp->nhcols] = 0;
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
		/*bp->arr[bp->currbug->info.col + bp->currbug->info.row * bp->nhcols] = 0; */
		(void) free((void *) bp->currbug);
	}

	while (bp->lasttemp->previous != bp->firsttemp) {
		bp->currbug = bp->lasttemp->previous;
		bp->currbug->previous->next = bp->lasttemp;
		bp->lasttemp->previous = bp->currbug->previous;
		/*bp->arr[bp->currbug->info.col + bp->currbug->info.row * bp->nhcols] = 0; */
		(void) free((void *) bp->currbug);
	}
}

static int
dirbug(bugstruct * info)
{
	double      sum = 0.0, prob;
	int         i;

	prob = (double) LRAND() / MAXRAND;
	for (i = 0; i < ORIENTS; i++) {
		sum += info->gene_prob[i];
		if (prob < sum)
			return i;
	}
	return ORIENTS - 1;	/* Could miss due to rounding */
}

static void
mutatebug(bugstruct * info)
{
	double      sum = 0.0;
	int         gene;

	gene = NRAND(ORIENTS);
	if (LRAND() & 1) {
		if (info->gene[gene] == MAXGENE)
			return;
		info->gene[gene]++;
	} else {
		if (info->gene[gene] == MINGENE)
			return;
		info->gene[gene]--;
	}
	for (gene = 0; gene < ORIENTS; gene++)
		sum += genexp[info->gene[gene] + MAXGENE];
	for (gene = 0; gene < ORIENTS; gene++)
		info->gene_prob[gene] = genexp[info->gene[gene] + MAXGENE] / sum;
}

static void
makebacteria(ModeInfo * mi,
	     int n, int startx, int starty, int width, int height)
{
	int         i = 0, j = 0, hcol, hrow, colrow;
	bugfarmstruct *bp = &bugfarms[MI_SCREEN(mi)];

	/* Make bacteria but if can't, don't loop forever */
	while (i < n && j < 2 * n) {
		hrow = NRAND(height) + starty;
		if (width - (!(hrow & 1)) > 0)
			hcol = NRAND(width - (!(hrow & 1))) + startx;
		else
			hcol = startx;
		colrow = hcol + hrow * bp->nhcols;
		j++;
		if (!bp->arr[colrow] && !bp->bacteria[colrow]) {
			i++;
			bp->bacteria[colrow] = True;
			drawabacterium(mi, hcol, hrow, True);
		}
	}
}

static void
redrawbacteria(ModeInfo * mi, int colrow)
{
	int         hcol, hrow;
	bugfarmstruct *bp = &bugfarms[MI_SCREEN(mi)];

	if (!bp->bacteria[colrow])
		return;

	hrow = colrow / bp->nhcols;
	hcol = colrow % bp->nhcols;
	drawabacterium(mi, hcol, hrow, True);
}

void
init_bug(ModeInfo * mi)
{
	bugfarmstruct *bp;
	int         size = MI_SIZE(mi);
	int         i = 0, j = 0, hcol, hrow, gene, colrow;
	double      sum;
	bugstruct   info;

	if (bugfarms == NULL) {
		if ((bugfarms = (bugfarmstruct *) calloc(MI_NUM_SCREENS(mi),
					    sizeof (bugfarmstruct))) == NULL)
			return;
	}
	bp = &bugfarms[MI_SCREEN(mi)];
	bp->redrawing = 0;
	bp->generation = 0;
	if (!bp->initialized) {	/* Genesis */
		bp->initialized = 1;
		/* Set up what will be a 'triply' linked list.
		   doubly linked list, doubly linked to an array */
		init_buglist(bp);
		genexp[MAXGENE] = 1;
		for (i = 1; i <= MAXGENE; i++) {
			genexp[MAXGENE + i] = genexp[MAXGENE + i - 1] * M_E;
			genexp[MAXGENE - i] = 1.0 / genexp[MAXGENE + i];
		}
	} else			/* Apocolypse */
		flush_buglist(bp);

	bp->width = MI_WIN_WIDTH(mi);
	bp->height = MI_WIN_HEIGHT(mi);
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
	bp->nccols = MAX(bp->width / bp->xs - 1, 2);
	bp->ncrows = MAX(bp->height / bp->ys, 2);
	bp->nhcols = bp->nccols / 2;
	bp->nhrows = bp->ncrows / 2;
	bp->nhrows -= !(bp->nhrows & 1);	/* Must be odd */
	bp->xb = (bp->width - bp->xs * bp->nccols) / 2;
	bp->yb = (bp->height - bp->ys * bp->ncrows) / 2;
	if (bp->arr != NULL)
		(void) free((void *) bp->arr);
	bp->arr = (BugList **) calloc(bp->nhcols * bp->nhrows, sizeof (BugList *));
	if (bp->bacteria != NULL)
		(void) free((void *) bp->bacteria);
	bp->bacteria = (char *) calloc(bp->nhcols * bp->nhrows, sizeof (char));

	bp->edenwidth = bp->nhcols / 4;
	bp->edenheight = bp->nhrows / 4;
	if (bp->edenheight)
		bp->edenheight -= !(bp->edenheight & 1);	/* Make sure its odd */
	bp->eden = (bp->edenwidth > 1 && bp->edenheight > 1);
	if (bp->eden) {
		bp->edenstartx = NRAND(bp->nhcols - bp->edenwidth);
		bp->edenstarty = NRAND(bp->nhrows - bp->edenheight);
		if (bp->edenstarty & 1)
			bp->edenstarty++;
	}
	/* Play G-d with these numbers */
	bp->n = MI_BATCHCOUNT(mi);
	if (bp->n < 0)
		bp->n = NRAND(-bp->n) + 1;

	XClearWindow(MI_DISPLAY(mi), MI_WINDOW(mi));

	/* Make bugs but if can't, don't loop forever */
	i = 0, j = 0;
	while (i < bp->n && j < 2 * bp->n) {
		hrow = NRAND(bp->nhrows);
		if (bp->nhcols - (!(hrow & 1)) > 0)
			hcol = NRAND(bp->nhcols - (!(hrow & 1)));
		else
			hcol = 0;
		colrow = hcol + hrow * bp->nhcols;
		j++;
		if (!bp->arr[colrow] && !neighbor(bp, hcol, hrow)) {
			i++;
			info.age = 0;
			info.energy = INITENERGY;
			info.direction = NRAND(ORIENTS);
			for (gene = 0; gene < ORIENTS; gene++)
#if 0				/* Some creationalism, may create gliders or twirlers */
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
			for (gene = 0; gene < ORIENTS; gene++)
				sum += genexp[info.gene[gene] + MAXGENE];
			for (gene = 0; gene < ORIENTS; gene++)
				info.gene_prob[gene] = genexp[info.gene[gene] + MAXGENE] / sum;
			if (MI_NPIXELS(mi) > 2)
				info.color = NRAND(MI_NPIXELS(mi));
			else
				info.color = 0;
			info.col = hcol;
			info.row = hrow;
			addto_buglist(bp, info);
			bp->arr[colrow] = bp->currbug;
			drawabug(mi, hcol, hrow, bp->currbug->info.color);
		}
	}
	makebacteria(mi, bp->nhcols * bp->nhrows / 2, 0, 0, bp->nhcols, bp->nhrows);
	if (bp->eden)
		makebacteria(mi, bp->edenwidth * bp->edenheight / 2,
			     bp->edenstartx, bp->edenstarty, bp->edenwidth, bp->edenheight);
}

#define ENOUGH 16
void
draw_bug(ModeInfo * mi)
{
	bugfarmstruct *bp = &bugfarms[MI_SCREEN(mi)];
	int         hcol, hrow, nhcol, nhrow, colrow, ncolrow, absdir, tryit;

	bp->currbug = bp->firstbug->next;
	while (bp->currbug != bp->lastbug) {
		hcol = bp->currbug->info.col;
		hrow = bp->currbug->info.row;
		colrow = hcol + hrow * bp->nhcols;
		if (bp->currbug->info.energy-- < 0) {	/* Time to die, Bug */
			/* back up one or else in void */
			bp->currbug = bp->currbug->previous;
			removefrom_buglist(bp, bp->arr[colrow]);
			bp->arr[colrow] = 0;
			eraseabug(mi, hcol, hrow);
			bp->n--;
		} else {	/* try to move */
			bp->arr[colrow] = 0;	/* Don't want neighbor to detect itself */
			tryit = 0;
			do {
				if (tryit++ > ENOUGH) {
					break;
				}
				absdir = (bp->currbug->info.direction +
				       dirbug(&bp->currbug->info)) % ORIENTS;
			} while (!hexcmove(bp, hcol, hrow, absdir, &nhcol, &nhrow) ||
				 neighbor(bp, nhcol, nhrow));
			bp->currbug->info.age++;
			bp->currbug->info.energy--;
			if (tryit <= ENOUGH) {
				ncolrow = nhcol + nhrow * bp->nhcols;
				if (bp->bacteria[ncolrow]) {
					bp->currbug->info.energy += BACTERIAENERGY;
					bp->bacteria[ncolrow] = 0;
					if (bp->currbug->info.energy > MAXENERGY)
						bp->currbug->info.energy = MAXENERGY;
				}
				bp->currbug->info.col = nhcol;
				bp->currbug->info.row = nhrow;
				bp->currbug->info.direction = absdir;
				bp->arr[ncolrow] = bp->currbug;
				if (bp->currbug->info.energy > STRONG &&
				    bp->currbug->info.age > MATURE) {	/* breed */
					drawabug(mi, nhcol, nhrow, bp->currbug->info.color);
					cutfrom_buglist(bp);	/* This rotates out who goes first */
					bp->babybug->info.age = 0;
					bp->babybug->info.energy = INITENERGY;
					dupin_buglist(bp);
					mutatebug(&bp->babybug->previous->info);
					mutatebug(&bp->babybug->info);
					bp->arr[colrow] = bp->babybug;
					bp->babybug->info.col = hcol;
					bp->babybug->info.row = hrow;
					bp->n++;
				} else {
					eraseabug(mi, hcol, hrow);
					drawabug(mi, nhcol, nhrow, bp->currbug->info.color);
				}
			} else
				bp->arr[colrow] = bp->currbug;
		}
		bp->currbug = bp->currbug->next;
	}
	reattach_buglist(bp);
	makebacteria(mi, FOODPERCYCLE, 0, 0, bp->nhcols, bp->nhrows);
	if (bp->eden)
		makebacteria(mi, FOODPERCYCLE,
			     bp->edenstartx, bp->edenstarty, bp->edenwidth, bp->edenheight);
	if (!bp->n || bp->generation >= MI_CYCLES(mi))
		init_bug(mi);
	bp->generation++;
	if (bp->redrawing) {
		int         i;

		for (i = 0; i < REDRAWSTEP; i++) {
			if (bp->bacteria[bp->redrawpos])
				redrawbacteria(mi, bp->redrawpos);
			if (++(bp->redrawpos) >= bp->nhcols * bp->nhrows) {
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

	bp->redrawing = 1;
	bp->redrawpos = 0;
}
