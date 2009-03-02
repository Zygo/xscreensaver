/* -*- Mode: C; tab-width: 4 -*- */
/* wire --- logical circuits based on simple state-changes (wireworld) */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)wire.c	4.07 97/11/24 xlockmore";

#endif

/*-
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
 * 05-Dec-97: neighbors option added.
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
#define PROGCLASS "Wire"
#define HACK_INIT init_wire
#define HACK_DRAW draw_wire
#define wire_opts xlockmore_opts
#define DEFAULTS "*delay: 500000 \n" \
 "*count: 1000 \n" \
 "*cycles: 150 \n" \
 "*size: -8 \n" \
 "*ncolors: 64 \n" \
 "*neighbors: 0 \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */
#include "automata.h"

#ifdef MODE_wire

/*-
 * neighbors of 0 randomizes it between 3, 4, 6, 8, 9, and 12.
 */
#define DEF_NEIGHBORS  "0"      /* choose random value */

static int  neighbors;

static XrmOptionDescRec opts[] =
{
	{"-neighbors", ".wire.neighbors", XrmoptionSepArg, (caddr_t) NULL}
};

static argtype vars[] =
{
	{(caddr_t *) & neighbors, "neighbors", "Neighbors", DEF_NEIGHBORS, t_Int
}
};
static OptionStruct desc[] =
{
	{"-neighbors num", "squares 4 or 8, hexagons 6, triangles 3, 9 or 12"}
};

ModeSpecOpt wire_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   wire_description =
{"wire", "init_wire", "draw_wire", "release_wire",
 "refresh_wire", "init_wire", NULL, &wire_opts,
 500000, 1000, 150, -8, 64, 1.0, "",
 "Shows a random circuit with 2 electrons", 0, NULL};

#endif

#define WIREBITS(n,w,h)\
  wp->pixmaps[wp->init_bits++]=\
  XCreatePixmapFromBitmapData(display,window,(char *)n,w,h,1,0,1)

#define COLORS 4
#define MINWIRES 32
#define MINGRIDSIZE 24
#define MINSIZE 3
#define ANGLES 360
#define NEIGHBORKINDS 6

#define SPACE 0
#define WIRE 1			/* Normal wire */
#define HEAD 2			/* electron head */
#define TAIL 3			/* electron tail */

#define REDRAWSTEP 2000		/* How much wire to draw per cycle */

/* Singly linked list */
typedef struct _CellList {
	XPoint      pt;
	struct _CellList *next;
} CellList;

typedef struct {
	int         init_bits;
	int         neighbors;
	int         generation;
	int         xs, ys;
	int         xb, yb;
	int         nrows, ncols;
	int         bnrows, bncols;
	int         mincol, minrow, maxcol, maxrow;
	int         width, height;
	int         redrawing, redrawpos;
	unsigned char *oldcells, *newcells;
	int         ncells[COLORS - 1];
	CellList   *cellList[COLORS - 1];
	unsigned char colors[COLORS - 1];
	GC          stippledGC;
	Pixmap      pixmaps[COLORS - 1];
	int         prob_array[12];
	union {
		XPoint      hexagon[6];
		XPoint      triangle[2][3];
	} shape;
} circuitstruct;

static char plots[NEIGHBORKINDS] =
{3, 4, 6, 8, 9, 12};		/* Neighborhoods */

static circuitstruct *circuits = NULL;

static void
position_of_neighbor(circuitstruct * wp, int dir, int *pcol, int *prow)
{
	int         col = *pcol, row = *prow;

	/* NO WRAPING */

	if (wp->neighbors == 6) {
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
	} else if (wp->neighbors == 4 || wp->neighbors == 8) {
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
		if ((col + row) % 2) {	/* right */
			switch (dir) {
				case 0:
					col = col - 1;
					break;
				case 30:
				case 40:
					col = col - 1;
					row = row - 1;
					break;
				case 60:
					col = col - 1;
					row = row - 2;
					break;
				case 80:
				case 90:
					row = row - 2;
					break;
				case 120:
					row = row - 1;
					break;
				case 150:
				case 160:
					col = col + 1;
					row = row - 1;
					break;
				case 180:
					col = col + 1;
					break;
				case 200:
				case 210:
					col = col + 1;
					row = row + 1;
					break;
				case 240:
					row = row + 1;
					break;
				case 270:
				case 280:
					row = row + 2;
					break;
				case 300:
					col = col - 1;
					row = row + 2;
					break;
				case 320:
				case 330:
					col = col - 1;
					row = row + 1;
					break;
				default:
					(void) fprintf(stderr, "wrong direction %d\n", dir);
			}
		} else {	/* left */
			switch (dir) {
				case 0:
					col = col + 1;
					break;
				case 30:
				case 40:
					col = col + 1;
					row = row + 1;
					break;
				case 60:
					col = col + 1;
					row = row + 2;
					break;
				case 80:
				case 90:
					row = row + 2;
					break;
				case 120:
					row = row + 1;
					break;
				case 150:
				case 160:
					col = col - 1;
					row = row + 1;
					break;
				case 180:
					col = col - 1;
					break;
				case 200:
				case 210:
					col = col - 1;
					row = row - 1;
					break;
				case 240:
					row = row - 1;
					break;
				case 270:
				case 280:
					row = row - 2;
					break;
				case 300:
					col = col + 1;
					row = row - 2;
					break;
				case 320:
				case 330:
					col = col + 1;
					row = row - 1;
					break;
				default:
					(void) fprintf(stderr, "wrong direction %d\n", dir);
			}
		}
	}
	*pcol = col;
	*prow = row;
}

static      Bool
withinBounds(circuitstruct * wp, int col, int row)
{
	return (row >= 2 && row < wp->bnrows - 2 &&
		col >= 2 && col < wp->bncols - 2 - (wp->neighbors == 6 && !(row % 2)));
}

static void
fillcell(ModeInfo * mi, GC gc, int col, int row)
{
	circuitstruct *wp = &circuits[MI_SCREEN(mi)];

	if (wp->neighbors == 6) {
		int         ccol = 2 * col + !(row & 1), crow = 2 * row;

		wp->shape.hexagon[0].x = wp->xb + ccol * wp->xs;
		wp->shape.hexagon[0].y = wp->yb + crow * wp->ys;
		if (wp->xs == 1 && wp->ys == 1)
			XDrawPoint(MI_DISPLAY(mi), MI_WINDOW(mi), gc,
			wp->shape.hexagon[0].x, wp->shape.hexagon[0].y);
		else
			XFillPolygon(MI_DISPLAY(mi), MI_WINDOW(mi), gc,
			    wp->shape.hexagon, 6, Convex, CoordModePrevious);
	} else if (wp->neighbors == 4 || wp->neighbors == 8) {
		XFillRectangle(MI_DISPLAY(mi), MI_WINDOW(mi), gc,
		wp->xb + wp->xs * col, wp->yb + wp->ys * row,
		wp->xs - (wp->xs > 3), wp->ys - (wp->ys > 3));
	} else {		/* TRI */
		int         orient = (col + row) % 2;	/* O left 1 right */

		wp->shape.triangle[orient][0].x = wp->xb + col * wp->xs;
		wp->shape.triangle[orient][0].y = wp->yb + row * wp->ys;
		if (wp->xs <= 3 || wp->ys <= 3)
			XDrawPoint(MI_DISPLAY(mi), MI_WINDOW(mi), gc,
			((orient) ? -1 : 1) + wp->shape.triangle[orient][0].x,
				       wp->shape.triangle[orient][0].y);
		else {
			if (orient)
				wp->shape.triangle[orient][0].x += (wp->xs / 2 - 1);
			else
				wp->shape.triangle[orient][0].x -= (wp->xs / 2 - 1);
			XFillPolygon(MI_DISPLAY(mi), MI_WINDOW(mi), gc,
				     wp->shape.triangle[orient], 3, Convex, CoordModePrevious);
		}
	}
}

static void
drawcell(ModeInfo * mi, int col, int row, unsigned char state)
{
	circuitstruct *wp = &circuits[MI_SCREEN(mi)];
	GC          gc;

	if (MI_NPIXELS(mi) > 2) {
		gc = MI_GC(mi);
		XSetForeground(MI_DISPLAY(mi), gc, MI_PIXEL(mi, wp->colors[state]));
	} else {
		XGCValues   gcv;

		gcv.stipple = wp->pixmaps[state];
		gcv.foreground = MI_WHITE_PIXEL(mi);
		gcv.background = MI_BLACK_PIXEL(mi);
		XChangeGC(MI_DISPLAY(mi), wp->stippledGC,
			  GCStipple | GCForeground | GCBackground, &gcv);
		gc = wp->stippledGC;
	}
	fillcell(mi, gc, col, row);
}

#if 0
static void
drawcell_notused(ModeInfo * mi, int col, int row, unsigned char state)
{
	circuitstruct *wp = &circuits[MI_SCREEN(mi)];
	XGCValues   gcv;
	GC          gc;

	if (MI_NPIXELS(mi) > 2) {
		gc = MI_GC(mi);
		XSetForeground(MI_DISPLAY(mi), gc, MI_PIXEL(mi, wp->colors[state]));
	} else {
		gcv.stipple = wp->pixmaps[state];
		gcv.foreground = MI_WHITE_PIXEL(mi);
		gcv.background = MI_BLACK_PIXEL(mi);
		XChangeGC(MI_DISPLAY(mi), wp->stippledGC,
			  GCStipple | GCForeground | GCBackground, &gcv);
		gc = wp->stippledGC;
	}
	XFillRectangle(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi),
	       wp->xb + wp->xs * col, wp->yb + wp->ys * row,
		wp->xs - (wp->xs > 3), wp->ys - (wp->ys > 3));
}
#endif

static void
addtolist(ModeInfo * mi, int col, int row, unsigned char state)
{
	circuitstruct *wp = &circuits[MI_SCREEN(mi)];
	CellList   *current = wp->cellList[state];

	wp->cellList[state] = NULL;
	if ((wp->cellList[state] = (CellList *) malloc(sizeof (CellList))) == NULL) {
		wp->cellList[state] = current;
		return;
	}
	wp->cellList[state]->pt.x = col;
	wp->cellList[state]->pt.y = row;
	wp->cellList[state]->next = current;
	wp->ncells[state]++;
}

#ifdef DEBUG
static void
print_state(ModeInfo * mi, int state)
{
	circuitstruct *wp = &circuits[MI_SCREEN(mi)];
	CellList   *locallist = wp->cellList[state];
	int         i = 0;

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
	CellList   *current;

	while (wp->cellList[state]) {
		current = wp->cellList[state];
		wp->cellList[state] = wp->cellList[state]->next;
		(void) free((void *) current);
	}
	wp->ncells[state] = 0;
}

static void
draw_state(ModeInfo * mi, int state)
{
	circuitstruct *wp = &circuits[MI_SCREEN(mi)];
	GC          gc;
	XGCValues   gcv;
	CellList   *current = wp->cellList[state];

	if (MI_NPIXELS(mi) > 2) {
		gc = MI_GC(mi);
		XSetForeground(MI_DISPLAY(mi), gc, MI_PIXEL(mi, wp->colors[state]));
	} else {
		gcv.stipple = wp->pixmaps[state];
		gcv.foreground = MI_WHITE_PIXEL(mi);
		gcv.background = MI_BLACK_PIXEL(mi);
		XChangeGC(MI_DISPLAY(mi), wp->stippledGC,
			  GCStipple | GCForeground | GCBackground, &gcv);
		gc = wp->stippledGC;
	}

	if (wp->neighbors == 6) {	/* Draw right away, slow */
		while (current) {
			int         col, row, ccol, crow;

			col = current->pt.x;
			row = current->pt.y;
			ccol = 2 * col + !(row & 1), crow = 2 * row;
			wp->shape.hexagon[0].x = wp->xb + ccol * wp->xs;
			wp->shape.hexagon[0].y = wp->yb + crow * wp->ys;
			if (wp->xs == 1 && wp->ys == 1)
				XDrawPoint(MI_DISPLAY(mi), MI_WINDOW(mi),
					       gc, wp->shape.hexagon[0].x, wp->shape.hexagon[0].y);
			else
				XFillPolygon(MI_DISPLAY(mi), MI_WINDOW(mi), gc,
					     wp->shape.hexagon, 6, Convex, CoordModePrevious);
			current = current->next;
		}
	} else if (wp->neighbors == 4 || wp->neighbors == 8) {
		XRectangle *rects = NULL;
		/* Take advantage of XFillRectangles */
		int         nrects = 0;

		/* Create Rectangle list from part of the cellList */
		if ((rects = (XRectangle *) malloc(wp->ncells[state] * sizeof (XRectangle))) == NULL) {
			return;
    }

		while (current) {
			rects[nrects].x = wp->xb + current->pt.x * wp->xs;
			rects[nrects].y = wp->yb + current->pt.y * wp->ys;
			rects[nrects].width = wp->xs - (wp->xs > 3);
			rects[nrects].height = wp->ys - (wp->ys > 3);
			current = current->next;
			nrects++;
		}
		/* Finally get to draw */
		XFillRectangles(MI_DISPLAY(mi), MI_WINDOW(mi), gc, rects, nrects);
		/* Free up rects list and the appropriate part of the cellList */
		(void) free((void *) rects);
	} else {		/* TRI */
		while (current) {
			int         col, row, orient;

			col = current->pt.x;
			row = current->pt.y;
			orient = (col + row) % 2;	/* O left 1 right */
			wp->shape.triangle[orient][0].x = wp->xb + col * wp->xs;
			wp->shape.triangle[orient][0].y = wp->yb + row * wp->ys;
			if (wp->xs <= 3 || wp->ys <= 3)
				XDrawPoint(MI_DISPLAY(mi), MI_WINDOW(mi), gc,
					       ((orient) ? -1 : 1) + wp->shape.triangle[orient][0].x,
				      wp->shape.triangle[orient][0].y);
			else {
				if (orient)
					wp->shape.triangle[orient][0].x += (wp->xs / 2 - 1);
				else
					wp->shape.triangle[orient][0].x -= (wp->xs / 2 - 1);
				XFillPolygon(MI_DISPLAY(mi), MI_WINDOW(mi), gc,
					     wp->shape.triangle[orient], 3, Convex, CoordModePrevious);
			}
			current = current->next;
		}
	}
	free_state(wp, state);
	XFlush(MI_DISPLAY(mi));
}

#if 0
static void
RandomSoup(circuitstruct * wp)
{
	int         i, j;

	for (j = 2; j < wp->bnrows - 2; j++)
		for (i = 2; i < wp->bncols - 2; i++) {
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
	wp->mincol = col - 1, wp->minrow = row - 2;
	wp->maxcol = col + 1, wp->maxrow = row + 2;
	dir = NRAND(wp->neighbors) * ANGLES / wp->neighbors;
	*(wp->newcells + col + row * wp->bncols) = HEAD;
	while (++count < n) {
		prob = NRAND(wp->prob_array[wp->neighbors - 1]);
		i = 0;
		while (prob > wp->prob_array[i])
			i++;
		dir = ((dir * wp->neighbors / ANGLES + i) %
		       wp->neighbors) * ANGLES / wp->neighbors;
		nextcol = col;
		nextrow = row;
		position_of_neighbor(wp, dir, &nextcol, &nextrow);
		if (withinBounds(wp, nextcol, nextrow)) {
			col = nextcol;
			row = nextrow;
			if (col == wp->mincol && col > 2)
				wp->mincol--;
			if (row == wp->minrow && row > 2)
				wp->minrow--;
			else if (row == wp->minrow - 1 && row > 3)
				wp->minrow -= 2;
			if (col == wp->maxcol && col < wp->bncols - 3)
				wp->maxcol++;
			if (row == wp->maxrow && row < wp->bnrows - 3)
				wp->maxrow++;
			else if (row == wp->maxrow + 1 && row < wp->bnrows - 4)
				wp->maxrow += 2;

			if (!*(wp->newcells + col + row * wp->bncols))
				*(wp->newcells + col + row * wp->bncols) = WIRE;
		} else {
			if (wp->neighbors == 3)
				break;	/* There is no reverse step */
			dir = ((dir * wp->neighbors / ANGLES + wp->neighbors / 2) %
			       wp->neighbors) * ANGLES / wp->neighbors;
		}
	}
	*(wp->newcells + col + row * wp->bncols) = HEAD;
}

static void
do_gen(circuitstruct * wp)
{
	int         i, j, k;
	unsigned char *z;
	int         count;

#define LOC(X, Y) (*(wp->oldcells + (X) + ((Y) * wp->bncols)))
#define ADD(X, Y) if (LOC((X), (Y)) == HEAD) count++

	for (j = wp->minrow; j <= wp->maxrow; j++) {
		for (i = wp->mincol; i <= wp->maxcol; i++) {
			z = wp->newcells + i + j * wp->bncols;
			switch (LOC(i, j)) {
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
					for (k = 0; k < wp->neighbors; k++) {
						int         newi = i, newj = j;

						position_of_neighbor(wp, k * ANGLES / wp->neighbors, &newi, &newj);
						ADD(newi, newj);
					}
					if (count == 1 || count == 2)
						*z = HEAD;
					else
						*z = WIRE;
					break;
				default:
					{
						(void) fprintf(stderr,
							       "bad internal character %d at %d,%d\n",
						      (int) LOC(i, j), i, j);
					}
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
release_wire(ModeInfo * mi)
{
	if (circuits != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			circuitstruct *wp = &circuits[screen];
			int         shade;

			for (shade = 0; shade < wp->init_bits; shade++)
				if (wp->pixmaps[shade] != None)
					XFreePixmap(MI_DISPLAY(mi), wp->pixmaps[shade]);
			if (wp->stippledGC != None)
				XFreeGC(MI_DISPLAY(mi), wp->stippledGC);
			if (wp->oldcells != NULL)
				(void) free((void *) wp->oldcells);
			if (wp->newcells != NULL)
				(void) free((void *) wp->newcells);
			free_list(wp);
		}
		(void) free((void *) circuits);
		circuits = NULL;
	}
}

void
init_wire(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         i, size = MI_SIZE(mi), n;
	circuitstruct *wp;
	XGCValues   gcv;

	if (circuits == NULL) {
		if ((circuits = (circuitstruct *) calloc(MI_NUM_SCREENS(mi),
					    sizeof (circuitstruct))) == NULL)
			return;
	}
	wp = &circuits[MI_SCREEN(mi)];

	wp->redrawing = 0;

	if ((MI_NPIXELS(mi) <= 2) && (wp->init_bits == 0)) {
		if (wp->stippledGC == None) {
			gcv.fill_style = FillOpaqueStippled;
			wp->stippledGC = XCreateGC(display, window, GCFillStyle, &gcv);
			if (wp->stippledGC == None) {
				release_wire(mi);
				return;
			}
		}
		WIREBITS(stipples[NUMSTIPPLES - 1], STIPPLESIZE, STIPPLESIZE);
		WIREBITS(stipples[NUMSTIPPLES - 3], STIPPLESIZE, STIPPLESIZE);
		WIREBITS(stipples[2], STIPPLESIZE, STIPPLESIZE);
		if (wp->pixmaps[2] == None) {
			release_wire(mi);
			return;
		}
	}
	if (MI_NPIXELS(mi) > 2) {
		wp->colors[0] = (NRAND(MI_NPIXELS(mi)));
		wp->colors[1] = (wp->colors[0] + MI_NPIXELS(mi) / 6 +
			     NRAND(MI_NPIXELS(mi) / 4 + 1)) % MI_NPIXELS(mi);
		wp->colors[2] = (wp->colors[1] + MI_NPIXELS(mi) / 6 +
			     NRAND(MI_NPIXELS(mi) / 4 + 1)) % MI_NPIXELS(mi);
	}
	free_list(wp);
	wp->generation = 0;
	wp->width = MI_WIDTH(mi);
	wp->height = MI_HEIGHT(mi);

	for (i = 0; i < NEIGHBORKINDS; i++) {
		if (neighbors == plots[i]) {
			wp->neighbors = plots[i];
			break;
		}
		if (i == NEIGHBORKINDS - 1) {
			i = NRAND(NEIGHBORKINDS - 3) + 1;	/* Skip triangular ones */
			wp->neighbors = plots[i];
			break;
		}
	}

	wp->prob_array[wp->neighbors - 1] = 100;
	if (wp->neighbors == 3) {
		wp->prob_array[1] = 67;
		wp->prob_array[0] = 33;
	} else {
		int         incr = 24 / wp->neighbors;

		for (i = wp->neighbors - 2; i >= 0; i--) {
			wp->prob_array[i] = wp->prob_array[i + 1] - incr -
				incr * ((i + 1) != wp->neighbors / 2);
		}
	}

	if (wp->neighbors == 6) {
		int         nccols, ncrows;

		if (wp->width < 4)
			wp->width = 4;
		if (wp->height < 4)
			wp->height = 4;
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
		nccols = MAX(wp->width / wp->xs - 2, 2);
		ncrows = MAX(wp->height / wp->ys - 1, 2);
		wp->ncols = nccols / 2;
		wp->nrows = ncrows / 2;
		wp->nrows -= !(wp->nrows & 1);	/* Must be odd */
		wp->xb = (wp->width - wp->xs * nccols) / 2 + wp->xs;
		wp->yb = (wp->height - wp->ys * ncrows) / 2 + wp->ys;
		for (i = 0; i < 6; i++) {
			wp->shape.hexagon[i].x = (wp->xs - 1) * hexagonUnit[i].x;
			wp->shape.hexagon[i].y = ((wp->ys - 1) * hexagonUnit[i].y / 2) * 4 / 3;
		}
	} else if (wp->neighbors == 4 || wp->neighbors == 8) {
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
	} else {		/* TRI */
		int         orient;

		if (wp->width < 4)
			wp->width = 4;
		if (wp->height < 2)
			wp->height = 2;
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
		wp->xs = (int) (1.52 * wp->ys);
		wp->ncols = (MAX(wp->width / wp->xs - 1, 4) / 2) * 2;
		wp->nrows = (MAX(wp->height / wp->ys - 1, 2) / 2) * 2 - 1;
		wp->xb = (wp->width - wp->xs * wp->ncols) / 2 + wp->xs / 2;
		wp->yb = (wp->height - wp->ys * wp->nrows) / 2 + wp->ys;
		for (orient = 0; orient < 2; orient++) {
			for (i = 0; i < 3; i++) {
				wp->shape.triangle[orient][i].x =
					(wp->xs - 2) * triangleUnit[orient][i].x;
				wp->shape.triangle[orient][i].y =
					(wp->ys - 2) * triangleUnit[orient][i].y;
			}
		}
	}

	/*
	 * I am being a bit naughty here wasting a little bit of memory
	 * but it will give me a real headache to figure out the logic
	 * and to refigure the mappings to save a few bytes
	 * ncols should only need a border of 2 and nrows should only need
	 * a border of 4 when in the neighbors = 9 or 12
	 */
	wp->bncols = wp->ncols + 4;
	wp->bnrows = wp->nrows + 4;

	MI_CLEARWINDOW(mi);

	if (wp->oldcells != NULL) {
		(void) free((void *) wp->oldcells);
		wp->oldcells = NULL;
	}
	if ((wp->oldcells = (unsigned char *) calloc(wp->bncols * wp->bnrows, sizeof (unsigned char))) == NULL) {
		release_wire(mi);
		return;
	}

	if (wp->newcells != NULL) {
		(void) free((void *) wp->newcells);
		wp->newcells = NULL;
	}
	if ((wp->newcells = (unsigned char *) calloc(wp->bncols * wp->bnrows, sizeof (unsigned char))) == NULL) {
		release_wire(mi);
		return;
	}

	n = MI_COUNT(mi);
	i = (1 + (wp->neighbors == 6)) * wp->ncols * wp->nrows / 4;
	if (n < -MINWIRES && i > MINWIRES) {
		n = NRAND(MIN(-n, i) - MINWIRES + 1) + MINWIRES;
	} else if (n < MINWIRES) {
		n = MINWIRES;
	} else if (n > i) {
		n = MAX(MINWIRES, i);
	}
	create_path(wp, n);
}

void
draw_wire(ModeInfo * mi)
{
	circuitstruct *wp = &circuits[MI_SCREEN(mi)];
	int         offset, i, j;
	unsigned char *z, *znew;

	if (circuits == NULL) {
		init_wire(mi);
		return;
	}
	MI_IS_DRAWN(mi) = True;

	/* wires do not grow so min max stuff does not change */
	for (j = wp->minrow; j <= wp->maxrow; j++) {
		for (i = wp->mincol; i <= wp->maxcol; i++) {
			offset = j * wp->bncols + i;
			z = wp->oldcells + offset;
			znew = wp->newcells + offset;
			if (*z != *znew) {	/* Counting on once a space always a space */
				*z = *znew;
				addtolist(mi, i - 2, j - 2, *znew - 1);
			}
		}
	}
	for (i = 0; i < COLORS - 1; i++)
		draw_state(mi, i);
	if (++wp->generation > MI_CYCLES(mi)) {
		init_wire(mi);
		return;
	} else
		do_gen(wp);

	if (wp->redrawing) {
		for (i = 0; i < REDRAWSTEP; i++) {
			if ((*(wp->oldcells + wp->redrawpos))) {
				drawcell(mi, wp->redrawpos % wp->bncols - 2,
					 wp->redrawpos / wp->bncols - 2, *(wp->oldcells + wp->redrawpos) - 1);
			}
			if (++(wp->redrawpos) >= wp->bncols * (wp->bnrows - 2)) {
				wp->redrawing = 0;
				break;
			}
		}
	}
}

void
refresh_wire(ModeInfo * mi)
{
	circuitstruct *wp = &circuits[MI_SCREEN(mi)];

	if (circuits == NULL) {
		init_wire(mi);
		return;
	}
	MI_CLEARWINDOW(mi);
	wp->redrawing = 1;
	wp->redrawpos = 2 * wp->ncols + 2;
}

#endif /* MODE_wire */
