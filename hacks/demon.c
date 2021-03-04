/* -*- Mode: C; tab-width: 4 -*- */
/* demon --- David Griffeath's cellular automata */

#if 0
static const char sccsid[] = "@(#)demon.c	5.00 2000/11/01 xlockmore";
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
 * 01-Nov-2000: Allocation checks
 * 10-May-1997: Compatible with xscreensaver
 * 16-Apr-1997: -neighbors 3, 9 (not sound mathematically), 12, and 8 added
 * 30-May-1996: Ron Hitchens <ron@idiom.com>
 *            Fixed memory management that caused leaks
 * 14-Apr-1996: -neighbors 6 runtime-time option added
 * 21-Aug-1995: Coded from A.K. Dewdney's "Computer Recreations", Scientific
 *              American Magazine" Aug 1989 pp 102-105.  Also very similar
 *              to hodgepodge machine described in A.K. Dewdney's "Computer
 *              Recreations", Scientific American Magazine" Aug 1988
 *              pp 104-107.  Also used life.c as a guide.
 */

/*-
 * A cellular universe of 4 phases debris, droplets, defects, and demons.
 */

/*-
  Grid     Number of Neighbors
  ----     ------------------
  Square   4 or 8
  Hexagon  6
  Triangle 3, 9, or 12
*/

#ifndef HAVE_JWXYZ
# define DO_STIPPLE
#endif

#ifdef STANDALONE
# define MODE_demon
# define DEFAULTS	"*delay:   50000 \n" \
					"*count:   0     \n" \
					"*cycles:  1000  \n" \
					"*size:    -30   \n" \
					"*ncolors: 64    \n" \
					"*fpsSolid: true    \n" \
				    "*ignoreRotation: True  \n" \

# define UNIFORM_COLORS
# define release_demon 0
# define reshape_demon 0
# define demon_handle_event 0
# include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
# include "xlock.h"		/* in xlockmore distribution */
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
	{"-neighbors", ".demon.neighbors", XrmoptionSepArg, 0}
};

static argtype vars[] =
{
	{&neighbors, "neighbors", "Neighbors", DEF_NEIGHBORS, t_Int}
};
static OptionStruct desc[] =
{
	{"-neighbors num", "squares 4 or 8, hexagons 6, triangles 3, 9 or 12"}
};

ENTRYPOINT ModeSpecOpt demon_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   demon_description =
{"demon", "init_demon", "draw_demon", (char *) NULL,
 "refresh_demon", "init_demon", "free_demon", &demon_opts,
 50000, 0, 1000, -7, 64, 1.0, "",
 "Shows Griffeath's cellular automata", 0, NULL};

#endif

#define DEMONBITS(n,w,h)\
  if ((dp->pixmaps[dp->init_bits]=\
  XCreatePixmapFromBitmapData(MI_DISPLAY(mi),window,(char *)n,w,h,1,0,1))==None){\
  free_demon(mi); return;} else {dp->init_bits++;}

#define REDRAWSTEP 2000		/* How many cells to draw per cycle */
#define MINSTATES 2
#define MINGRIDSIZE 5
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

static demonstruct *demons = (demonstruct *) NULL;

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
#ifdef DO_STIPPLE
		gcv.stipple = dp->pixmaps[(state - 1) % (NUMSTIPPLES - 1)];
#endif /* DO_STIPPLE */
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

static Bool
addtolist(ModeInfo * mi, int col, int row, unsigned char state)
{
	demonstruct *dp = &demons[MI_SCREEN(mi)];
	CellList   *current;

	current = dp->cellList[state];
	if ((dp->cellList[state] = (CellList *)
		malloc(sizeof (CellList))) == NULL) {
		return False;
	}
	dp->cellList[state]->pt.x = col;
	dp->cellList[state]->pt.y = row;
	dp->cellList[state]->next = current;
	dp->ncells[state]++;
	return True;
}


static void
free_state(demonstruct * dp, int state)
{
	CellList   *current;

	while (dp->cellList[state]) {
		current = dp->cellList[state];
		dp->cellList[state] = dp->cellList[state]->next;
		(void) free((void *) current);
	}
	dp->cellList[state] = (CellList *) NULL;
	if (dp->ncells != NULL)
		dp->ncells[state] = 0;
}


static void
free_list(demonstruct * dp)
{
	int         state;

	for (state = 0; state < dp->states; state++)
		free_state(dp, state);
	(void) free((void *) dp->cellList);
	dp->cellList = (CellList **) NULL;
}

static void
free_struct(demonstruct * dp)
{
	if (dp->cellList != NULL) {
		free_list(dp);
	}
	if (dp->ncells != NULL) {
		(void) free((void *) dp->ncells);
		dp->ncells = (int *) NULL;
	}
	if (dp->oldcell != NULL) {
		(void) free((void *) dp->oldcell);
		dp->oldcell = (unsigned char *) NULL;
	}
	if (dp->newcell != NULL) {
		(void) free((void *) dp->newcell);
		dp->newcell = (unsigned char *) NULL;
	}
}

ENTRYPOINT void
free_demon(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	demonstruct *dp = &demons[MI_SCREEN(mi)];
	int         shade;

	if (dp->stippledGC != None) {
		XFreeGC(display, dp->stippledGC);
		dp->stippledGC = None;
	}
	for (shade = 0; shade < dp->init_bits; shade++) {
		XFreePixmap(display, dp->pixmaps[shade]);
	}
	dp->init_bits = 0;
	free_struct(dp);
}

static Bool
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

#ifdef DO_STIPPLE
		gcv.stipple = dp->pixmaps[(state - 1) % (NUMSTIPPLES - 1)];
#endif /* DO_STIPPLE */
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
		if ((rects = (XRectangle *) malloc(dp->ncells[state] *
			 sizeof (XRectangle))) == NULL) {
			return False;
		}
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
	return True;
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
			if (!addtolist(mi, col, row, dp->oldcell[col + mrow]))
				return; /* sparse soup */
		}
		mrow += dp->ncols;
	}
}

ENTRYPOINT void
init_demon (ModeInfo * mi)
{
	int         size = MI_SIZE(mi), nk;
	demonstruct *dp;

	MI_INIT (mi, demons);
	dp = &demons[MI_SCREEN(mi)];

    if (MI_WIDTH(mi) < 100 || MI_HEIGHT(mi) < 100)  /* tiny window */
      size = MIN(MI_WIDTH(mi), MI_HEIGHT(mi));

	dp->generation = 0;
	dp->redrawing = 0;
#ifdef DO_STIPPLE
	if (MI_NPIXELS(mi) < NUMSTIPPLES) {
          Window window = MI_WINDOW(mi);
          if (dp->stippledGC == None) {
			XGCValues   gcv;

			gcv.fill_style = FillOpaqueStippled;
			if ((dp->stippledGC = XCreateGC(MI_DISPLAY(mi), window,
				 GCFillStyle, &gcv)) == None) {
				free_demon(mi);
				return;
			}
		}
		if (dp->init_bits == 0) {
			int         i;

			for (i = 1; i < NUMSTIPPLES; i++) {
				DEMONBITS(stipples[i], STIPPLESIZE, STIPPLESIZE);
			}
		}
	}
#endif /* DO_STIPPLE */
	free_struct(dp);

#ifdef HAVE_JWXYZ
    jwxyz_XSetAntiAliasing (MI_DISPLAY(mi), MI_GC(mi), False);
#endif

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
	if ((dp->cellList = (CellList **) calloc(dp->states,
		sizeof (CellList *))) == NULL) {
		free_demon(mi);
		return;
	}
	if ((dp->ncells = (int *) calloc(dp->states, sizeof (int))) == NULL) {
		free_demon(mi);
		return;
	}

	dp->state = 0;

	dp->width = MI_WIDTH(mi);
	dp->height = MI_HEIGHT(mi);

	if (dp->neighbors == 6) {
		int         nccols, ncrows, i;

		if (dp->width < 8)
			dp->width = 8;
		if (dp->height < 8)
			dp->height = 8;
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
		ncrows = MAX(dp->height / dp->ys - 1, 4);
		dp->ncols = nccols / 2;
		dp->nrows = 2 * (ncrows / 4);
		dp->xb = (dp->width - dp->xs * nccols) / 2 + dp->xs / 2;
		dp->yb = (dp->height - dp->ys * (ncrows / 2) * 2) / 2 + dp->ys - 2;
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

	if ((dp->oldcell = (unsigned char *)
		malloc(dp->ncols * dp->nrows * sizeof (unsigned char))) == NULL) {
		free_demon(mi);
		return;
	}

	if ((dp->newcell = (unsigned char *)
		malloc(dp->ncols * dp->nrows * sizeof (unsigned char))) == NULL) {
		free_demon(mi);
		return;
	}

	RandomSoup(mi);
}

ENTRYPOINT void
draw_demon (ModeInfo * mi)
{
	int         i, j, k, l, mj = 0, ml;
	demonstruct *dp;

	if (demons == NULL)
		return;
	dp = &demons[MI_SCREEN(mi)];
	if (dp->cellList == NULL)
		return;

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
					/*l = j;*/
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
						ml = mj;
						if (dp->oldcell[k + ml] ==
						    (int) (dp->oldcell[i + mj] + 1) % dp->states)
							dp->newcell[i + mj] = dp->oldcell[k + ml];
					} else {	/* left */
						/* E */
						k = (i + 1 == dp->ncols) ? 0 : i + 1;
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
								/*l = j;*/
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
								/*l = j;*/
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
					if (!addtolist(mi, i, j, dp->oldcell[i + mj])) {
						free_demon(mi);
						return;
					}
				}
			mj += dp->ncols;
		}
		if (++dp->generation > MI_CYCLES(mi))
			init_demon(mi);
		dp->state = 0;
	} else {
		if (dp->ncells[dp->state])
			if (!draw_state(mi, dp->state)) {
				free_demon(mi);
				return;
			}
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

#ifndef STANDALONE
ENTRYPOINT void
refresh_demon (ModeInfo * mi)
{
	demonstruct *dp;

	if (demons == NULL)
		return;
	dp = &demons[MI_SCREEN(mi)];

	dp->redrawing = 1;
	dp->redrawpos = 0;
}
#endif

XSCREENSAVER_MODULE ("Demon", demon)

#endif /* MODE_demon */
