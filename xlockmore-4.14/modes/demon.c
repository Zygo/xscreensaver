/* -*- Mode: C; tab-width: 4 -*- */
/* demon --- David Griffeath's cellular automata */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)demon.c	4.07 97/11/24 xlockmore";

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
 * 16-Apr-97: -neighbors 3, 9 (not sound mathematically), 12, and 8 added
 * 30-May-96: Ron Hitchens <ron@idiom.com>
 *            Fixed memory management that caused leaks
 * 14-Apr-96: -neighbors 6 runtime-time option added
 * 21-Aug-95: Coded from A.K. Dewdney's "Computer Recreations", Scientific
 *            American Magazine" Aug 1989 pp 102-105.  Also very similar to
 *            hodgepodge machine described in A.K. Dewdney's "Computer
 *            Recreations", Scientific American Magazine" Aug 1988 pp 104-107.
 *            also used life.c as a guide.
 */

/*-
 * A cellular universe of 4 phases debris, droplets, defects, and demons.
 */

/*-
  Grid     Number of Neigbors
  ----     ------------------
  Square   4 or 8
  Hexagon  6
  Triangle 3, 9, or 12
*/

#ifdef STANDALONE
#define PROGCLASS "Demon"
#define HACK_INIT init_demon
#define HACK_DRAW draw_demon
#define demon_opts xlockmore_opts
#define DEFAULTS "*delay: 50000 \n" \
 "*count: 0 \n" \
 "*cycles: 1000 \n" \
 "*size: -7 \n" \
 "*ncolors: 64 \n" \
 "*neighbors: 0 \n"
#define UNIFORM_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */

#endif /* STANDALONE */
#include "automata.h"

#ifdef MODE_demon

/*-
 * neighbors of 0 randomizes it between 3, 4, 6, 8, 9, and 12.
 */
#define DEF_NEIGHBORS  "0"      /* choose random value */

static int  neighbors;

static XrmOptionDescRec opts[] =
{
	{"-neighbors", ".demon.neighbors", XrmoptionSepArg, (caddr_t) NULL}
};

static argtype vars[] =
{
	{(caddr_t *) & neighbors, "neighbors", "Neighbors", DEF_NEIGHBORS, t_Int}
};
static OptionStruct desc[] =
{
	{"-neighbors num", "squares 4 or 8, hexagons 6, triangles 3, 9 or 12"}
};

ModeSpecOpt demon_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   demon_description =
{"demon", "init_demon", "draw_demon", "release_demon",
 "refresh_demon", "init_demon", NULL, &demon_opts,
 50000, 0, 1000, -7, 64, 1.0, "",
 "Shows Griffeath's cellular automata", 0, NULL};

#endif

#define DEMONBITS(n,w,h)\
  dp->pixmaps[dp->init_bits++]=\
  XCreatePixmapFromBitmapData(display,window,(char *)n,w,h,1,0,1)

#define REDRAWSTEP 2000		/* How many cells to draw per cycle */
#define MINSTATES 2
#define MINGRIDSIZE 24
#define MINSIZE 4
#define NEIGHBORKINDS 6

/* Singly linked list */
typedef struct _CellList {
	XPoint      pt;
	struct _CellList *next;
} CellList;

typedef struct {
	int         generation;
	int         xs, ys;
	int         xb, yb;
	int         nrows, ncols;
	int         width, height;
	int         states;
	int         state;
	int         redrawing, redrawpos;
	int        *ncells;
	CellList  **cellList;
	unsigned char *oldcell, *newcell;
	int         neighbors;
	int         init_bits;
	GC          stippledGC;
	Pixmap      pixmaps[NUMSTIPPLES - 1];
	union {
		XPoint      hexagon[6];
		XPoint      triangle[2][3];
	} shape;
} demonstruct;

static char plots[2][NEIGHBORKINDS] =
{
	{3, 4, 6, 8, 9, 12},	/* Neighborhoods */
	{12, 16, 18, 20, 22, 24}	/* Number of states */
};

static demonstruct *demons = NULL;

static void
drawcell(ModeInfo * mi, int col, int row, unsigned char state)
{
	demonstruct *dp = &demons[MI_SCREEN(mi)];
	GC          gc;

	if (!state) {
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_BLACK_PIXEL(mi));
		gc = MI_GC(mi);
	} else if (MI_NPIXELS(mi) >= NUMSTIPPLES) {
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi),
			   MI_PIXEL(mi, (((int) state - 1) * MI_NPIXELS(mi) /
					 (dp->states - 1)) % MI_NPIXELS(mi)));
		gc = MI_GC(mi);
	} else {
		XGCValues   gcv;

		gcv.stipple = dp->pixmaps[(state - 1) % (NUMSTIPPLES - 1)];
		gcv.foreground = MI_WHITE_PIXEL(mi);
		gcv.background = MI_BLACK_PIXEL(mi);
		XChangeGC(MI_DISPLAY(mi), dp->stippledGC,
			  GCStipple | GCForeground | GCBackground, &gcv);
		gc = dp->stippledGC;
	}
	if (dp->neighbors == 6) {
		int         ccol = 2 * col + !(row & 1), crow = 2 * row;

		dp->shape.hexagon[0].x = dp->xb + ccol * dp->xs;
		dp->shape.hexagon[0].y = dp->yb + crow * dp->ys;
		if (dp->xs == 1 && dp->ys == 1)
			XDrawPoint(MI_DISPLAY(mi), MI_WINDOW(mi),
				       gc, dp->shape.hexagon[0].x, dp->shape.hexagon[0].y);
		else
			XFillPolygon(MI_DISPLAY(mi), MI_WINDOW(mi), gc,
			    dp->shape.hexagon, 6, Convex, CoordModePrevious);
	} else if (dp->neighbors == 4 || dp->neighbors == 8) {
		XFillRectangle(MI_DISPLAY(mi), MI_WINDOW(mi), gc,
		dp->xb + dp->xs * col, dp->yb + dp->ys * row,
		dp->xs - (dp->xs > 3), dp->ys - (dp->ys > 3));
	} else {		/* TRI */
		int         orient = (col + row) % 2;	/* O left 1 right */

		dp->shape.triangle[orient][0].x = dp->xb + col * dp->xs;
		dp->shape.triangle[orient][0].y = dp->yb + row * dp->ys;
		if (dp->xs <= 3 || dp->ys <= 3)
			XDrawPoint(MI_DISPLAY(mi), MI_WINDOW(mi), gc,
			((orient) ? -1 : 1) + dp->shape.triangle[orient][0].x,
				       dp->shape.triangle[orient][0].y);
		else {
			if (orient)
				dp->shape.triangle[orient][0].x += (dp->xs / 2 - 1);
			else
				dp->shape.triangle[orient][0].x -= (dp->xs / 2 - 1);
			XFillPolygon(MI_DISPLAY(mi), MI_WINDOW(mi), gc,
				     dp->shape.triangle[orient], 3, Convex, CoordModePrevious);

		}
	}
}

static void
addtolist(ModeInfo * mi, int col, int row, unsigned char state)
{
	demonstruct *dp = &demons[MI_SCREEN(mi)];
	CellList   *current;

	current = dp->cellList[state];
	dp->cellList[state] = (CellList *) malloc(sizeof (CellList));
	dp->cellList[state]->pt.x = col;
	dp->cellList[state]->pt.y = row;
	dp->cellList[state]->next = current;
	dp->ncells[state]++;
}

#ifdef DEBUG
static void
print_state(ModeInfo * mi, int state)
{
	demonstruct *dp = &demons[MI_SCREEN(mi)];
	CellList   *locallist;
	int         i = 0;

	locallist = dp->cellList[state];
	(void) printf("state %d\n", state);
	while (locallist) {
		(void) printf("%d	x %d, y %d\n", i,
			      locallist->pt.x, locallist->pt.y);
		locallist = locallist->next;
		i++;
	}
}

#endif

static void
free_state(demonstruct * dp, int state)
{
	CellList   *current;

	while (dp->cellList[state]) {
		current = dp->cellList[state];
		dp->cellList[state] = dp->cellList[state]->next;
		(void) free((void *) current);
	}
	dp->cellList[state] = NULL;
	if (dp->ncells)
		dp->ncells[state] = 0;
}


static void
free_list(demonstruct * dp)
{
	int         state;

	for (state = 0; state < dp->states; state++)
		free_state(dp, state);
	(void) free((void *) dp->cellList);
	dp->cellList = NULL;
}

static void
free_struct(demonstruct * dp)
{
	if (dp->cellList != NULL) {
		free_list(dp);
	}
	if (dp->ncells != NULL) {
		(void) free((void *) dp->ncells);
		dp->ncells = NULL;
	}
	if (dp->oldcell != NULL) {
		(void) free((void *) dp->oldcell);
		dp->oldcell = NULL;
	}
	if (dp->newcell != NULL) {
		(void) free((void *) dp->newcell);
		dp->newcell = NULL;
	}
}

static void
draw_state(ModeInfo * mi, int state)
{
	demonstruct *dp = &demons[MI_SCREEN(mi)];
	GC          gc;
	XRectangle *rects;
	CellList   *current;

	if (!state) {
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_BLACK_PIXEL(mi));
		gc = MI_GC(mi);
	} else if (MI_NPIXELS(mi) >= NUMSTIPPLES) {
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi),
			   MI_PIXEL(mi, (((int) state - 1) * MI_NPIXELS(mi) /
					 (dp->states - 1)) % MI_NPIXELS(mi)));
		gc = MI_GC(mi);
	} else {
		XGCValues   gcv;

		gcv.stipple = dp->pixmaps[(state - 1) % (NUMSTIPPLES - 1)];
		gcv.foreground = MI_WHITE_PIXEL(mi);
		gcv.background = MI_BLACK_PIXEL(mi);
		XChangeGC(MI_DISPLAY(mi), dp->stippledGC,
			  GCStipple | GCForeground | GCBackground, &gcv);
		gc = dp->stippledGC;
	}
	if (dp->neighbors == 6) {	/* Draw right away, slow */
		current = dp->cellList[state];
		while (current) {
			int         col, row, ccol, crow;

			col = current->pt.x;
			row = current->pt.y;
			ccol = 2 * col + !(row & 1), crow = 2 * row;
			dp->shape.hexagon[0].x = dp->xb + ccol * dp->xs;
			dp->shape.hexagon[0].y = dp->yb + crow * dp->ys;
			if (dp->xs == 1 && dp->ys == 1)
				XDrawPoint(MI_DISPLAY(mi), MI_WINDOW(mi),
					       gc, dp->shape.hexagon[0].x, dp->shape.hexagon[0].y);
			else
				XFillPolygon(MI_DISPLAY(mi), MI_WINDOW(mi), gc,
					     dp->shape.hexagon, 6, Convex, CoordModePrevious);
			current = current->next;
		}
	} else if (dp->neighbors == 4 || dp->neighbors == 8) {
		/* Take advantage of XDrawRectangles */
		int         ncells = 0;

		/* Create Rectangle list from part of the cellList */
		rects = (XRectangle *) malloc(dp->ncells[state] * sizeof (XRectangle));
		current = dp->cellList[state];
		while (current) {
			rects[ncells].x = dp->xb + current->pt.x * dp->xs;
			rects[ncells].y = dp->yb + current->pt.y * dp->ys;
			rects[ncells].width = dp->xs - (dp->xs > 3);
			rects[ncells].height = dp->ys - (dp->ys > 3);
			current = current->next;
			ncells++;
		}
		/* Finally get to draw */
		XFillRectangles(MI_DISPLAY(mi), MI_WINDOW(mi), gc, rects, ncells);
		/* Free up rects list and the appropriate part of the cellList */
		(void) free((void *) rects);
	} else {		/* TRI */
		current = dp->cellList[state];
		while (current) {
			int         col, row, orient;

			col = current->pt.x;
			row = current->pt.y;
			orient = (col + row) % 2;	/* O left 1 right */
			dp->shape.triangle[orient][0].x = dp->xb + col * dp->xs;
			dp->shape.triangle[orient][0].y = dp->yb + row * dp->ys;
			if (dp->xs <= 3 || dp->ys <= 3)
				XDrawPoint(MI_DISPLAY(mi), MI_WINDOW(mi), gc,
					       ((orient) ? -1 : 1) + dp->shape.triangle[orient][0].x,
				      dp->shape.triangle[orient][0].y);
			else {
				if (orient)
					dp->shape.triangle[orient][0].x += (dp->xs / 2 - 1);
				else
					dp->shape.triangle[orient][0].x -= (dp->xs / 2 - 1);
				XFillPolygon(MI_DISPLAY(mi), MI_WINDOW(mi), gc,
					     dp->shape.triangle[orient], 3, Convex, CoordModePrevious);
			}
			current = current->next;
		}
	}
	free_state(dp, state);
	XFlush(MI_DISPLAY(mi));
}

static void
RandomSoup(ModeInfo * mi)
{
	demonstruct *dp = &demons[MI_SCREEN(mi)];
	int         row, col, mrow = 0;

	for (row = 0; row < dp->nrows; ++row) {
		for (col = 0; col < dp->ncols; ++col) {
			dp->oldcell[col + mrow] =
				(unsigned char) LRAND() % ((unsigned char) dp->states);
			addtolist(mi, col, row, dp->oldcell[col + mrow]);
		}
		mrow += dp->ncols;
	}
}

void
init_demon(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         size = MI_SIZE(mi), nk;
	demonstruct *dp;

	if (demons == NULL) {
		if ((demons = (demonstruct *) calloc(MI_NUM_SCREENS(mi),
					      sizeof (demonstruct))) == NULL)
			return;
	}
	dp = &demons[MI_SCREEN(mi)];
	dp->generation = 0;
	dp->redrawing = 0;
	if (MI_NPIXELS(mi) < NUMSTIPPLES) {
		if (dp->stippledGC == None) {
			XGCValues   gcv;

			gcv.fill_style = FillOpaqueStippled;
			dp->stippledGC = XCreateGC(display, window, GCFillStyle, &gcv);
		}
		if (dp->init_bits == 0) {
			int         i;

			for (i = 1; i < NUMSTIPPLES; i++)
				DEMONBITS(stipples[i], STIPPLESIZE, STIPPLESIZE);
		}
	}
	free_struct(dp);

	for (nk = 0; nk < NEIGHBORKINDS; nk++) {
		if (neighbors == plots[0][nk]) {
			dp->neighbors = plots[0][nk];
			break;
		}
		if (nk == NEIGHBORKINDS - 1) {
			nk = NRAND(NEIGHBORKINDS);
			dp->neighbors = plots[0][nk];
			break;
		}
	}

	dp->states = MI_COUNT(mi);
	if (dp->states < -MINSTATES)
		dp->states = NRAND(-dp->states - MINSTATES + 1) + MINSTATES;
	else if (dp->states < MINSTATES)
		dp->states = plots[1][nk];
	dp->cellList = (CellList **) calloc(dp->states, sizeof (CellList *));
	dp->ncells = (int *) calloc(dp->states, sizeof (int));

	dp->state = 0;

	dp->width = MI_WIDTH(mi);
	dp->height = MI_HEIGHT(mi);

	if (dp->neighbors == 6) {
		int         nccols, ncrows, i;

		if (dp->width < 2)
			dp->width = 2;
		if (dp->height < 4)
			dp->height = 4;
		if (size < -MINSIZE)
			dp->ys = NRAND(MIN(-size, MAX(MINSIZE, MIN(dp->width, dp->height) /
				      MINGRIDSIZE)) - MINSIZE + 1) + MINSIZE;
		else if (size < MINSIZE) {
			if (!size)
				dp->ys = MAX(MINSIZE, MIN(dp->width, dp->height) / MINGRIDSIZE);
			else
				dp->ys = MINSIZE;
		} else
			dp->ys = MIN(size, MAX(MINSIZE, MIN(dp->width, dp->height) /
					       MINGRIDSIZE));
		dp->xs = dp->ys;
		nccols = MAX(dp->width / dp->xs - 2, 2);
		ncrows = MAX(dp->height / dp->ys - 1, 2);
		dp->ncols = nccols / 2;
		dp->nrows = 2 * (ncrows / 4);
		dp->xb = (dp->width - dp->xs * nccols) / 2 + dp->xs / 2;
		dp->yb = (dp->height - dp->ys * (ncrows / 2) * 2) / 2 + dp->ys;
		for (i = 0; i < 6; i++) {
			dp->shape.hexagon[i].x = (dp->xs - 1) * hexagonUnit[i].x;
			dp->shape.hexagon[i].y = ((dp->ys - 1) * hexagonUnit[i].y / 2) * 4 / 3;
		}
	} else if (dp->neighbors == 4 || dp->neighbors == 8) {
		if (size < -MINSIZE)
			dp->ys = NRAND(MIN(-size, MAX(MINSIZE, MIN(dp->width, dp->height) /
				      MINGRIDSIZE)) - MINSIZE + 1) + MINSIZE;
		else if (size < MINSIZE) {
			if (!size)
				dp->ys = MAX(MINSIZE, MIN(dp->width, dp->height) / MINGRIDSIZE);
			else
				dp->ys = MINSIZE;
		} else
			dp->ys = MIN(size, MAX(MINSIZE, MIN(dp->width, dp->height) /
					       MINGRIDSIZE));
		dp->xs = dp->ys;
		dp->ncols = MAX(dp->width / dp->xs, 2);
		dp->nrows = MAX(dp->height / dp->ys, 2);
		dp->xb = (dp->width - dp->xs * dp->ncols) / 2;
		dp->yb = (dp->height - dp->ys * dp->nrows) / 2;
	} else {		/* TRI */
		int         orient, i;

		if (dp->width < 2)
			dp->width = 2;
		if (dp->height < 2)
			dp->height = 2;
		if (size < -MINSIZE)
			dp->ys = NRAND(MIN(-size, MAX(MINSIZE, MIN(dp->width, dp->height) /
				      MINGRIDSIZE)) - MINSIZE + 1) + MINSIZE;
		else if (size < MINSIZE) {
			if (!size)
				dp->ys = MAX(MINSIZE, MIN(dp->width, dp->height) / MINGRIDSIZE);
			else
				dp->ys = MINSIZE;
		} else
			dp->ys = MIN(size, MAX(MINSIZE, MIN(dp->width, dp->height) /
					       MINGRIDSIZE));
		dp->xs = (int) (1.52 * dp->ys);
		dp->ncols = (MAX(dp->width / dp->xs - 1, 2) / 2) * 2;
		dp->nrows = (MAX(dp->height / dp->ys - 1, 2) / 2) * 2;
		dp->xb = (dp->width - dp->xs * dp->ncols) / 2 + dp->xs / 2;
		dp->yb = (dp->height - dp->ys * dp->nrows) / 2 + dp->ys / 2;
		for (orient = 0; orient < 2; orient++) {
			for (i = 0; i < 3; i++) {
				dp->shape.triangle[orient][i].x =
					(dp->xs - 2) * triangleUnit[orient][i].x;
				dp->shape.triangle[orient][i].y =
					(dp->ys - 2) * triangleUnit[orient][i].y;
			}
		}
	}

	MI_CLEARWINDOW(mi);

	dp->oldcell = (unsigned char *)
		malloc(dp->ncols * dp->nrows * sizeof (unsigned char));

	dp->newcell = (unsigned char *)
		malloc(dp->ncols * dp->nrows * sizeof (unsigned char));

	RandomSoup(mi);
}

void
draw_demon(ModeInfo * mi)
{
	demonstruct *dp = &demons[MI_SCREEN(mi)];
	int         i, j, k, l, mj = 0, ml;

	MI_IS_DRAWN(mi) = True;

	if (dp->state >= dp->states) {
		(void) memcpy((char *) dp->newcell, (char *) dp->oldcell,
			      dp->ncols * dp->nrows * sizeof (unsigned char));

		if (dp->neighbors == 6) {
			for (j = 0; j < dp->nrows; j++) {
				for (i = 0; i < dp->ncols; i++) {
					/* NE */
					if (!(j & 1))
						k = (i + 1 == dp->ncols) ? 0 : i + 1;
					else
						k = i;
					l = (!j) ? dp->nrows - 1 : j - 1;
					ml = l * dp->ncols;
					if (dp->oldcell[k + ml] ==
					    (int) (dp->oldcell[i + mj] + 1) % dp->states)
						dp->newcell[i + mj] = dp->oldcell[k + ml];
					/* E */
					k = (i + 1 == dp->ncols) ? 0 : i + 1;
					l = j;
					ml = mj;
					if (dp->oldcell[k + ml] ==
					    (int) (dp->oldcell[i + mj] + 1) % dp->states)
						dp->newcell[i + mj] = dp->oldcell[k + ml];
					/* SE */
					if (!(j & 1))
						k = (i + 1 == dp->ncols) ? 0 : i + 1;
					else
						k = i;
					l = (j + 1 == dp->nrows) ? 0 : j + 1;
					ml = l * dp->ncols;
					if (dp->oldcell[k + ml] ==
					    (int) (dp->oldcell[i + mj] + 1) % dp->states)
						dp->newcell[i + mj] = dp->oldcell[k + ml];
					/* SW */
					if (j & 1)
						k = (!i) ? dp->ncols - 1 : i - 1;
					else
						k = i;
					l = (j + 1 == dp->nrows) ? 0 : j + 1;
					ml = l * dp->ncols;
					if (dp->oldcell[k + ml] ==
					    (int) (dp->oldcell[i + mj] + 1) % dp->states)
						dp->newcell[i + mj] = dp->oldcell[k + ml];
					/* W */
					k = (!i) ? dp->ncols - 1 : i - 1;
					l = j;
					ml = mj;
					if (dp->oldcell[k + ml] ==
					    (int) (dp->oldcell[i + mj] + 1) % dp->states)
						dp->newcell[i + mj] = dp->oldcell[k + ml];
					/* NW */
					if (j & 1)
						k = (!i) ? dp->ncols - 1 : i - 1;
					else
						k = i;
					l = (!j) ? dp->nrows - 1 : j - 1;
					ml = l * dp->ncols;
					if (dp->oldcell[k + ml] ==
					    (int) (dp->oldcell[i + mj] + 1) % dp->states)
						dp->newcell[i + mj] = dp->oldcell[k + ml];
				}
				mj += dp->ncols;
			}
		} else if (dp->neighbors == 4 || dp->neighbors == 8) {
			for (j = 0; j < dp->nrows; j++) {
				for (i = 0; i < dp->ncols; i++) {
					/* N */
					k = i;
					l = (!j) ? dp->nrows - 1 : j - 1;
					ml = l * dp->ncols;
					if (dp->oldcell[k + ml] ==
					    (int) (dp->oldcell[i + mj] + 1) % dp->states)
						dp->newcell[i + mj] = dp->oldcell[k + ml];
					/* E */
					k = (i + 1 == dp->ncols) ? 0 : i + 1;
					l = j;
					ml = mj;
					if (dp->oldcell[k + ml] ==
					    (int) (dp->oldcell[i + mj] + 1) % dp->states)
						dp->newcell[i + mj] = dp->oldcell[k + ml];
					/* S */
					k = i;
					l = (j + 1 == dp->nrows) ? 0 : j + 1;
					ml = l * dp->ncols;
					if (dp->oldcell[k + ml] ==
					    (int) (dp->oldcell[i + mj] + 1) % dp->states)
						dp->newcell[i + mj] = dp->oldcell[k + ml];
					/* W */
					k = (!i) ? dp->ncols - 1 : i - 1;
					l = j;
					ml = mj;
					if (dp->oldcell[k + ml] ==
					    (int) (dp->oldcell[i + mj] + 1) % dp->states)
						dp->newcell[i + mj] = dp->oldcell[k + ml];
				}
				mj += dp->ncols;
			}
			if (dp->neighbors == 8) {
				mj = 0;
				for (j = 0; j < dp->nrows; j++) {
					for (i = 0; i < dp->ncols; i++) {
						/* NE */
						k = (i + 1 == dp->ncols) ? 0 : i + 1;
						l = (!j) ? dp->nrows - 1 : j - 1;
						ml = l * dp->ncols;
						if (dp->oldcell[k + ml] ==
						    (int) (dp->oldcell[i + mj] + 1) % dp->states)
							dp->newcell[i + mj] = dp->oldcell[k + ml];
						/* SE */
						k = (i + 1 == dp->ncols) ? 0 : i + 1;
						l = (j + 1 == dp->nrows) ? 0 : j + 1;
						ml = l * dp->ncols;
						if (dp->oldcell[k + ml] ==
						    (int) (dp->oldcell[i + mj] + 1) % dp->states)
							dp->newcell[i + mj] = dp->oldcell[k + ml];
						/* SW */
						k = (!i) ? dp->ncols - 1 : i - 1;
						l = (j + 1 == dp->nrows) ? 0 : j + 1;
						ml = l * dp->ncols;
						if (dp->oldcell[k + ml] ==
						    (int) (dp->oldcell[i + mj] + 1) % dp->states)
							dp->newcell[i + mj] = dp->oldcell[k + ml];
						/* NW */
						k = (!i) ? dp->ncols - 1 : i - 1;
						l = (!j) ? dp->nrows - 1 : j - 1;
						ml = l * dp->ncols;
						if (dp->oldcell[k + ml] ==
						    (int) (dp->oldcell[i + mj] + 1) % dp->states)
							dp->newcell[i + mj] = dp->oldcell[k + ml];
					}
					mj += dp->ncols;
				}
			}
		} else if (dp->neighbors == 3 || dp->neighbors == 9 ||
			   dp->neighbors == 12) {
			for (j = 0; j < dp->nrows; j++) {
				for (i = 0; i < dp->ncols; i++) {
					if ((i + j) % 2) {	/* right */
						/* W */
						k = (!i) ? dp->ncols - 1 : i - 1;
						l = j;
						ml = mj;
						if (dp->oldcell[k + ml] ==
						    (int) (dp->oldcell[i + mj] + 1) % dp->states)
							dp->newcell[i + mj] = dp->oldcell[k + ml];
					} else {	/* left */
						/* E */
						k = (i + 1 == dp->ncols) ? 0 : i + 1;
						l = j;
						ml = mj;
						if (dp->oldcell[k + ml] ==
						    (int) (dp->oldcell[i + mj] + 1) % dp->states)
							dp->newcell[i + mj] = dp->oldcell[k + ml];
					}
					/* N */
					k = i;
					l = (!j) ? dp->nrows - 1 : j - 1;
					ml = l * dp->ncols;
					if (dp->oldcell[k + ml] ==
					    (int) (dp->oldcell[i + mj] + 1) % dp->states)
						dp->newcell[i + mj] = dp->oldcell[k + ml];
					/* S */
					k = i;
					l = (j + 1 == dp->nrows) ? 0 : j + 1;
					ml = l * dp->ncols;
					if (dp->oldcell[k + ml] ==
					    (int) (dp->oldcell[i + mj] + 1) % dp->states)
						dp->newcell[i + mj] = dp->oldcell[k + ml];
				}
				mj += dp->ncols;
			}
			if (dp->neighbors == 9 || dp->neighbors == 12) {
				mj = 0;
				for (j = 0; j < dp->nrows; j++) {
					for (i = 0; i < dp->ncols; i++) {
						/* NN */
						k = i;
						if (!j)
							l = dp->nrows - 2;
						else if (!(j - 1))
							l = dp->nrows - 1;
						else
							l = j - 2;
						ml = l * dp->ncols;
						if (dp->oldcell[k + ml] ==
						    (int) (dp->oldcell[i + mj] + 1) % dp->states)
							dp->newcell[i + mj] = dp->oldcell[k + ml];
						/* SS */
						k = i;
						if (j + 1 == dp->nrows)
							l = 1;
						else if (j + 2 == dp->nrows)
							l = 0;
						else
							l = j + 2;
						ml = l * dp->ncols;
						if (dp->oldcell[k + ml] ==
						    (int) (dp->oldcell[i + mj] + 1) % dp->states)
							dp->newcell[i + mj] = dp->oldcell[k + ml];
						/* NW */
						k = (!i) ? dp->ncols - 1 : i - 1;
						l = (!j) ? dp->nrows - 1 : j - 1;
						ml = l * dp->ncols;
						if (dp->oldcell[k + ml] ==
						    (int) (dp->oldcell[i + mj] + 1) % dp->states)
							dp->newcell[i + mj] = dp->oldcell[k + ml];
						/* NE */
						k = (i + 1 == dp->ncols) ? 0 : i + 1;
						l = (!j) ? dp->nrows - 1 : j - 1;
						ml = l * dp->ncols;
						if (dp->oldcell[k + ml] ==
						    (int) (dp->oldcell[i + mj] + 1) % dp->states)
							dp->newcell[i + mj] = dp->oldcell[k + ml];
						/* SW */
						k = (!i) ? dp->ncols - 1 : i - 1;
						l = (j + 1 == dp->nrows) ? 0 : j + 1;
						ml = l * dp->ncols;
						if (dp->oldcell[k + ml] ==
						    (int) (dp->oldcell[i + mj] + 1) % dp->states)
							dp->newcell[i + mj] = dp->oldcell[k + ml];
						/* SE */
						k = (i + 1 == dp->ncols) ? 0 : i + 1;
						l = (j + 1 == dp->nrows) ? 0 : j + 1;
						ml = l * dp->ncols;
						if (dp->oldcell[k + ml] ==
						    (int) (dp->oldcell[i + mj] + 1) % dp->states)
							dp->newcell[i + mj] = dp->oldcell[k + ml];
					}
					mj += dp->ncols;
				}
				if (dp->neighbors == 12) {
					mj = 0;
					for (j = 0; j < dp->nrows; j++) {
						for (i = 0; i < dp->ncols; i++) {
							if ((i + j) % 2) {	/* right */
								/* NNW */
								k = (!i) ? dp->ncols - 1 : i - 1;
								if (!j)
									l = dp->nrows - 2;
								else if (!(j - 1))
									l = dp->nrows - 1;
								else
									l = j - 2;
								ml = l * dp->ncols;
								if (dp->oldcell[k + ml] ==
								    (int) (dp->oldcell[i + mj] + 1) % dp->states)
									dp->newcell[i + mj] = dp->oldcell[k + ml];
								/* SSW */
								k = (!i) ? dp->ncols - 1 : i - 1;
								if (j + 1 == dp->nrows)
									l = 1;
								else if (j + 2 == dp->nrows)
									l = 0;
								else
									l = j + 2;
								ml = l * dp->ncols;
								if (dp->oldcell[k + ml] ==
								    (int) (dp->oldcell[i + mj] + 1) % dp->states)
									dp->newcell[i + mj] = dp->oldcell[k + ml];
								/* EE */
								k = (i + 1 == dp->ncols) ? 0 : i + 1;
								l = j;
								ml = mj;
								if (dp->oldcell[k + ml] ==
								    (int) (dp->oldcell[i + mj] + 1) % dp->states)
									dp->newcell[i + mj] = dp->oldcell[k + ml];
							} else {	/* left */
								/* NNE */
								k = (i + 1 == dp->ncols) ? 0 : i + 1;
								if (!j)
									l = dp->nrows - 2;
								else if (!(j - 1))
									l = dp->nrows - 1;
								else
									l = j - 2;
								ml = l * dp->ncols;
								if (dp->oldcell[k + ml] ==
								    (int) (dp->oldcell[i + mj] + 1) % dp->states)
									dp->newcell[i + mj] = dp->oldcell[k + ml];
								/* SSE */
								k = (i + 1 == dp->ncols) ? 0 : i + 1;
								if (j + 1 == dp->nrows)
									l = 1;
								else if (j + 2 == dp->nrows)
									l = 0;
								else
									l = j + 2;
								ml = l * dp->ncols;
								if (dp->oldcell[k + ml] ==
								    (int) (dp->oldcell[i + mj] + 1) % dp->states)
									dp->newcell[i + mj] = dp->oldcell[k + ml];
								/* WW */
								k = (!i) ? dp->ncols - 1 : i - 1;
								l = j;
								ml = mj;
								if (dp->oldcell[k + ml] ==
								    (int) (dp->oldcell[i + mj] + 1) % dp->states)
									dp->newcell[i + mj] = dp->oldcell[k + ml];
							}
						}
						mj += dp->ncols;
					}
				}
			}
		}
		mj = 0;
		for (j = 0; j < dp->nrows; j++) {
			for (i = 0; i < dp->ncols; i++)
				if (dp->oldcell[i + mj] != dp->newcell[i + mj]) {
					dp->oldcell[i + mj] = dp->newcell[i + mj];
					addtolist(mi, i, j, dp->oldcell[i + mj]);
				}
			mj += dp->ncols;
		}
		if (++dp->generation > MI_CYCLES(mi))
			init_demon(mi);
		dp->state = 0;
	} else {
		if (dp->ncells[dp->state])
			draw_state(mi, dp->state);
		dp->state++;
	}
	if (dp->redrawing) {
		for (i = 0; i < REDRAWSTEP; i++) {
			if (dp->oldcell[dp->redrawpos]) {
				drawcell(mi, dp->redrawpos % dp->ncols, dp->redrawpos / dp->ncols,
					 dp->oldcell[dp->redrawpos]);
			}
			if (++(dp->redrawpos) >= dp->ncols * dp->nrows) {
				dp->redrawing = 0;
				break;
			}
		}
	}
}
void
release_demon(ModeInfo * mi)
{
	if (demons != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			demonstruct *dp = &demons[screen];
			int         shade;

			if (dp->stippledGC != None) {
				XFreeGC(MI_DISPLAY(mi), dp->stippledGC);
			}
			for (shade = 0; shade < dp->init_bits; shade++)
				XFreePixmap(MI_DISPLAY(mi), dp->pixmaps[shade]);
			free_struct(dp);
		}
		(void) free((void *) demons);
		demons = NULL;
	}
} void

refresh_demon(ModeInfo * mi)
{
	demonstruct *dp = &demons[MI_SCREEN(mi)];

	dp->redrawing = 1;
	dp->redrawpos = 0;
}

#endif /* MODE_demon */
