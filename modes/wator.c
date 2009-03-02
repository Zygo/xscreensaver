/* -*- Mode: C; tab-width: 4 -*- */
/* wator --- Dewdney's Wa-Tor, water torus simulation */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)wator.c	4.07 97/11/24 xlockmore";

#endif

/*-
 * Copyright (c) 1994 by David Bagley.
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
 * 29-Aug-95: Efficiency improvements.
 * 12-Dec-94: Coded from A.K. Dewdney's "The Armchair Universe, Computer
 *            Recreations from the Pages of Scientific American Magazine"
 *            W.H. Freedman and Company, New York, 1988  (Dec 1984 and
 *            June 1985) also used life.c as a guide.
 */

#ifdef STANDALONE
#define PROGCLASS "Wator"
#define HACK_INIT init_wator
#define HACK_DRAW draw_wator
#define wator_opts xlockmore_opts
#define DEFAULTS "*delay: 750000 \n" \
 "*cycles: 32767 \n" \
 "*size: 0 \n" \
 "*ncolors: 200 \n" \
 "*neighbors: 0 \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */
#include "automata.h"

#ifdef MODE_wator

/*-
 * neighbors of 0 randomizes it between 3, 4, 6, 8, 9, and 12.
 */
#define DEF_NEIGHBORS  "0"      /* choose random value */

static int  neighbors;

static XrmOptionDescRec opts[] =
{
	{(char *) "-neighbors", (char *) ".wator.neighbors", XrmoptionSepArg, (caddr_t) NULL}
};

static argtype vars[] =
{
	{(caddr_t *) & neighbors, (char *) "neighbors", (char *) "Neighbors", (char *) DEF_NEIGHBORS, t_Int
}
};

static OptionStruct desc[] =
{
	{(char *) "-neighbors num", (char *) "squares 4 or 8, hexagons 6, triangles 3, 9 or 12"}
};

ModeSpecOpt wator_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   wator_description =
{"wator", "init_wator", "draw_wator", "release_wator",
 "refresh_wator", "init_wator", NULL, &wator_opts,
 750000, 1, 32767, 0, 64, 1.0, "",
 "Shows Dewdney's Water-Torus planet of fish and sharks", 0, NULL};

#endif

#include "bitmaps/fish-0.xbm"
#include "bitmaps/fish-1.xbm"
#include "bitmaps/fish-2.xbm"
#include "bitmaps/fish-3.xbm"
#include "bitmaps/fish-4.xbm"
#include "bitmaps/fish-5.xbm"
#include "bitmaps/fish-6.xbm"
#include "bitmaps/fish-7.xbm"
#include "bitmaps/shark-0.xbm"
#include "bitmaps/shark-1.xbm"
#include "bitmaps/shark-2.xbm"
#include "bitmaps/shark-3.xbm"
#include "bitmaps/shark-4.xbm"
#include "bitmaps/shark-5.xbm"
#include "bitmaps/shark-6.xbm"
#include "bitmaps/shark-7.xbm"

#define FISH 0
#define SHARK 1
#define KINDS 2
#define ORIENTS 4
#define REFLECTS 2
#define BITMAPS (ORIENTS*REFLECTS*KINDS)
#define KINDBITMAPS (ORIENTS*REFLECTS)
#define MINGRIDSIZE 10		/* It is possible for the fish to take over with 3 */
#define MINSIZE 4
#define NEIGHBORKINDS 6

static XImage logo[BITMAPS] =
{
	{0, 0, 0, XYBitmap, (char *) fish0_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{0, 0, 0, XYBitmap, (char *) fish1_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{0, 0, 0, XYBitmap, (char *) fish2_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{0, 0, 0, XYBitmap, (char *) fish3_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{0, 0, 0, XYBitmap, (char *) fish4_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{0, 0, 0, XYBitmap, (char *) fish5_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{0, 0, 0, XYBitmap, (char *) fish6_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{0, 0, 0, XYBitmap, (char *) fish7_bits, LSBFirst, 8, LSBFirst, 8, 1},
      {0, 0, 0, XYBitmap, (char *) shark0_bits, LSBFirst, 8, LSBFirst, 8, 1},
      {0, 0, 0, XYBitmap, (char *) shark1_bits, LSBFirst, 8, LSBFirst, 8, 1},
      {0, 0, 0, XYBitmap, (char *) shark2_bits, LSBFirst, 8, LSBFirst, 8, 1},
      {0, 0, 0, XYBitmap, (char *) shark3_bits, LSBFirst, 8, LSBFirst, 8, 1},
      {0, 0, 0, XYBitmap, (char *) shark4_bits, LSBFirst, 8, LSBFirst, 8, 1},
      {0, 0, 0, XYBitmap, (char *) shark5_bits, LSBFirst, 8, LSBFirst, 8, 1},
      {0, 0, 0, XYBitmap, (char *) shark6_bits, LSBFirst, 8, LSBFirst, 8, 1},
      {0, 0, 0, XYBitmap, (char *) shark7_bits, LSBFirst, 8, LSBFirst, 8, 1},
};

/* Fish and shark data */
typedef struct {
	char        kind, age, food, direction;
	unsigned long color;
	int         col, row;
} cellstruct;

/* Doublely linked list */
typedef struct _CellList {
	cellstruct  info;
	struct _CellList *previous, *next;
} CellList;

typedef struct {
	Bool        painted;
	int         nkind[KINDS];	/* Number of fish and sharks */
	int         breed[KINDS];	/* Breeding time of fish and sharks */
	int         sstarve;	/* Time the sharks starve if they dont find a fish */
	int         kind;	/* Currently working on fish or sharks? */
	int         xs, ys;	/* Size of fish and sharks */
	int         xb, yb;	/* Bitmap offset for fish and sharks */
	int         pixelmode;
	int         generation;
	int         ncols, nrows, positions;
	int         width, height;
	CellList   *currkind, *babykind, *lastkind[KINDS + 1], *firstkind[KINDS + 1];
	CellList  **arr;	/* 0=empty or pts to a fish or shark */
	int         neighbors;
	union {
		XPoint      hexagon[6];
		XPoint      triangle[2][3];
	} shape;
} watorstruct;

static char plots[NEIGHBORKINDS] =
{
	3, 4, 6, 8, 9, 12	/* Neighborhoods */
};

static watorstruct *wators = NULL;
static int  icon_width, icon_height;

#if 0
/*-
 * shape either a bitmap or 0 for circle and 1 for polygon
 * (if triangle shape:  -1, 0, 2 or 3 to differentiate left and right)
 */
drawshape(ModeInfo * mi, int x, int y, int sizex, int sizey,
	  int sides, int shape)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);

	if (sides == 4 && sizex == 0 && sizey == 0) {
		(void) XPutImage(display, window, gc, &logo[shape], 0, 0,
		x - icon_width, y - icon_height / 2, icon_width, icon_height);
	} else if (sizex < 3 || sizey < 3 || (sides == 4 && shape == 1)) {
		XFillRectangle(display, window, gc,
			x - sizex / 2, y - sizey / 2,
			sizex - (sizey > 3), sizey - (sizey > 3));
	} else if (sides == 4 && shape == 0) {
	} else if (sides == 6 && shape == 1) {
	} else if (sides == 6 && shape == 0) {
	} else if (sides == 3 && shape == 1) {
	} else if (sides == 3 && shape == 2) {
	} else if (sides == 3 && shape == -1) {
	} else if (sides == 3 && shape == 0) {
	}
}
#endif

static void
drawcell(ModeInfo * mi, int col, int row, unsigned long color, int bitmap,
	 Bool alive)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	watorstruct *wp = &wators[MI_SCREEN(mi)];
	unsigned long colour;

	if (!alive)
		colour = MI_BLACK_PIXEL(mi);
	else if (MI_NPIXELS(mi) > 2)
		colour = MI_PIXEL(mi, color);
	else
		colour = MI_WHITE_PIXEL(mi);
	XSetForeground(display, gc, colour);
	if (wp->neighbors == 6) {
		int         ccol = 2 * col + !(row & 1), crow = 2 * row;

		wp->shape.hexagon[0].x = wp->xb + ccol * wp->xs;
		wp->shape.hexagon[0].y = wp->yb + crow * wp->ys;
		if (wp->xs == 1 && wp->ys == 1)
			XDrawPoint(display, window, gc,
			wp->shape.hexagon[0].x, wp->shape.hexagon[0].y);
		else if (bitmap >= KINDBITMAPS || !alive)
			XFillPolygon(display, window, gc,
			    wp->shape.hexagon, 6, Convex, CoordModePrevious);
		else {
			XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
			XFillPolygon(display, window, gc,
			    wp->shape.hexagon, 6, Convex, CoordModePrevious);
			XSetForeground(display, gc, colour);
			if (wp->xs <= 6 || wp->ys <= 2)
				XFillRectangle(display, window, gc,
				     wp->shape.hexagon[0].x - 3 * wp->xs / 4,
				     wp->shape.hexagon[0].y + wp->ys / 4, wp->xs, wp->ys);
			else
				XFillArc(display, window, gc,
				     wp->xb + wp->xs * ccol - 3 * wp->xs / 4,
					 wp->yb + wp->ys * crow + wp->ys / 4,
					 2 * wp->xs - 6, 2 * wp->ys - 2,
					 0, 23040);
		}
	} else if (wp->neighbors == 4 || wp->neighbors == 8) {
		if (wp->pixelmode) {
			if (bitmap >= KINDBITMAPS || (wp->xs <= 2 || wp->ys <= 2) || !alive)
				XFillRectangle(display, window, gc,
				wp->xb + wp->xs * col, wp->yb + wp->ys * row,
				wp->xs - (wp->xs > 3), wp->ys - (wp->ys > 3));
			else {
				XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
				XFillRectangle(display, window, gc,
				wp->xb + wp->xs * col, wp->yb + wp->ys * row,
					       wp->xs, wp->ys);
				XSetForeground(display, gc, colour);
				XFillArc(display, window, gc,
					 wp->xb + wp->xs * col, wp->yb + wp->ys * row,
					 wp->xs - 1, wp->ys - 1,
					 0, 23040);
			}
		} else
			(void) XPutImage(display, window, gc,
					 &logo[bitmap], 0, 0,
				wp->xb + wp->xs * col, wp->yb + wp->ys * row,
					 icon_width, icon_height);
	} else {		/* TRI */
		int         orient = (col + row) % 2;	/* O left 1 right */

		wp->shape.triangle[orient][0].x = wp->xb + col * wp->xs;
		wp->shape.triangle[orient][0].y = wp->yb + row * wp->ys;
		if (wp->xs <= 3 || wp->ys <= 3)
			XDrawPoint(display, window, gc,
			((orient) ? -1 : 1) + wp->shape.triangle[orient][0].x,
				       wp->shape.triangle[orient][0].y);
		else {
			if (orient)
				wp->shape.triangle[orient][0].x += (wp->xs / 2 - 1);
			else
				wp->shape.triangle[orient][0].x -= (wp->xs / 2 - 1);
			if (bitmap >= KINDBITMAPS || !alive)
				XFillPolygon(display, window, gc,
					     wp->shape.triangle[orient], 3, Convex, CoordModePrevious);
			else {
				XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
				XFillPolygon(display, window, gc,
					     wp->shape.triangle[orient], 3, Convex, CoordModePrevious);
				XSetForeground(display, gc, colour);
				XFillArc(display, window, gc,
				     wp->xb + wp->xs * col - 4 * wp->xs / 5 +
				    ((orient) ? wp->xs / 3 : 3 * wp->xs / 5),
					 wp->yb + wp->ys * row - wp->ys / 2 + 1, wp->ys - 3, wp->ys - 3,
					 0, 23040);
			}
		}
	}
}

static void
init_kindlist(watorstruct * wp, int kind)
{
	/* Waste some space at the beginning and end of list
	   so we do not have to complicated checks against falling off the ends. */
	wp->lastkind[kind] = (CellList *) malloc(sizeof (CellList));
	wp->firstkind[kind] = (CellList *) malloc(sizeof (CellList));
	wp->firstkind[kind]->previous = wp->lastkind[kind]->next = NULL;
	wp->firstkind[kind]->next = wp->lastkind[kind]->previous = NULL;
	wp->firstkind[kind]->next = wp->lastkind[kind];
	wp->lastkind[kind]->previous = wp->firstkind[kind];
}

static void
addto_kindlist(watorstruct * wp, int kind, cellstruct info)
{
	wp->currkind = (CellList *) malloc(sizeof (CellList));
	wp->lastkind[kind]->previous->next = wp->currkind;
	wp->currkind->previous = wp->lastkind[kind]->previous;
	wp->currkind->next = wp->lastkind[kind];
	wp->lastkind[kind]->previous = wp->currkind;
	wp->currkind->info = info;
}

static void
removefrom_kindlist(watorstruct * wp, CellList * ptr)
{
	ptr->previous->next = ptr->next;
	ptr->next->previous = ptr->previous;
	wp->arr[ptr->info.col + ptr->info.row * wp->ncols] = 0;
	(void) free((void *) ptr);
}

static void
dupin_kindlist(watorstruct * wp)
{
	CellList   *temp;

	temp = (CellList *) malloc(sizeof (CellList));
	temp->previous = wp->babykind;
	temp->next = wp->babykind->next;
	wp->babykind->next = temp;
	temp->next->previous = temp;
	temp->info = wp->babykind->info;
	wp->babykind = temp;
}

/*-
 * new fish at end of list, this rotates who goes first, young fish go last
 * this most likely will not change the feel to any real degree
 */
static void
cutfrom_kindlist(watorstruct * wp)
{
	wp->babykind = wp->currkind;
	wp->currkind = wp->currkind->previous;
	wp->currkind->next = wp->babykind->next;
	wp->babykind->next->previous = wp->currkind;
	wp->babykind->next = wp->lastkind[KINDS];
	wp->babykind->previous = wp->lastkind[KINDS]->previous;
	wp->babykind->previous->next = wp->babykind;
	wp->babykind->next->previous = wp->babykind;
}

static void
reattach_kindlist(watorstruct * wp, int kind)
{
	wp->currkind = wp->lastkind[kind]->previous;
	wp->currkind->next = wp->firstkind[KINDS]->next;
	wp->currkind->next->previous = wp->currkind;
	wp->lastkind[kind]->previous = wp->lastkind[KINDS]->previous;
	wp->lastkind[KINDS]->previous->next = wp->lastkind[kind];
	wp->lastkind[KINDS]->previous = wp->firstkind[KINDS];
	wp->firstkind[KINDS]->next = wp->lastkind[KINDS];
}

static void
flush_kindlist(watorstruct * wp, int kind)
{
	while (wp->lastkind[kind]->previous != wp->firstkind[kind]) {
		wp->currkind = wp->lastkind[kind]->previous;
		wp->currkind->previous->next = wp->lastkind[kind];
		wp->lastkind[kind]->previous = wp->currkind->previous;
		/* wp->arr[wp->currkind->info.col + wp->currkind->info.row * wp->ncols] = 0; */
		(void) free((void *) wp->currkind);
	}
}

static int
neighbor_position(watorstruct * wp, int col, int row, int dir)
{
	if (wp->neighbors == 6) {
		switch (dir) {
			case 0:
				col = (col + 1 == wp->ncols) ? 0 : col + 1;
				break;
			case 60:
				if (!(row & 1))
					col = (col + 1 == wp->ncols) ? 0 : col + 1;
				row = (!row) ? wp->nrows - 1 : row - 1;
				break;
			case 120:
				if (row & 1)
					col = (!col) ? wp->ncols - 1 : col - 1;
				row = (!row) ? wp->nrows - 1 : row - 1;
				break;
			case 180:
				col = (!col) ? wp->ncols - 1 : col - 1;
				break;
			case 240:
				if (row & 1)
					col = (!col) ? wp->ncols - 1 : col - 1;
				row = (row + 1 == wp->nrows) ? 0 : row + 1;
				break;
			case 300:
				if (!(row & 1))
					col = (col + 1 == wp->ncols) ? 0 : col + 1;
				row = (row + 1 == wp->nrows) ? 0 : row + 1;
				break;
			default:
				(void) fprintf(stderr, "wrong direction %d\n", dir);
		}
	} else if (wp->neighbors == 4 || wp->neighbors == 8) {
		switch (dir) {
			case 0:
				col = (col + 1 == wp->ncols) ? 0 : col + 1;
				break;
			case 45:
				col = (col + 1 == wp->ncols) ? 0 : col + 1;
				row = (!row) ? wp->nrows - 1 : row - 1;
				break;
			case 90:
				row = (!row) ? wp->nrows - 1 : row - 1;
				break;
			case 135:
				col = (!col) ? wp->ncols - 1 : col - 1;
				row = (!row) ? wp->nrows - 1 : row - 1;
				break;
			case 180:
				col = (!col) ? wp->ncols - 1 : col - 1;
				break;
			case 225:
				col = (!col) ? wp->ncols - 1 : col - 1;
				row = (row + 1 == wp->nrows) ? 0 : row + 1;
				break;
			case 270:
				row = (row + 1 == wp->nrows) ? 0 : row + 1;
				break;
			case 315:
				col = (col + 1 == wp->ncols) ? 0 : col + 1;
				row = (row + 1 == wp->nrows) ? 0 : row + 1;
				break;
			default:
				(void) fprintf(stderr, "wrong direction %d\n", dir);
		}
	} else {		/* TRI */
		if ((col + row) % 2) {	/* right */
			switch (dir) {
				case 0:
					col = (!col) ? wp->ncols - 1 : col - 1;
					break;
				case 30:
				case 40:
					col = (!col) ? wp->ncols - 1 : col - 1;
					row = (row + 1 == wp->nrows) ? 0 : row + 1;
					break;
				case 60:
					col = (!col) ? wp->ncols - 1 : col - 1;
					if (row + 1 == wp->nrows)
						row = 1;
					else if (row + 2 == wp->nrows)
						row = 0;
					else
						row = row + 2;
					break;
				case 80:
				case 90:
					if (row + 1 == wp->nrows)
						row = 1;
					else if (row + 2 == wp->nrows)
						row = 0;
					else
						row = row + 2;
					break;
				case 120:
					row = (row + 1 == wp->nrows) ? 0 : row + 1;
					break;
				case 150:
				case 160:
					col = (col + 1 == wp->ncols) ? 0 : col + 1;
					row = (row + 1 == wp->nrows) ? 0 : row + 1;
					break;
				case 180:
					col = (col + 1 == wp->ncols) ? 0 : col + 1;
					break;
				case 200:
				case 210:
					col = (col + 1 == wp->ncols) ? 0 : col + 1;
					row = (!row) ? wp->nrows - 1 : row - 1;
					break;
				case 240:
					row = (!row) ? wp->nrows - 1 : row - 1;
					break;
				case 270:
				case 280:
					if (!row)
						row = wp->nrows - 2;
					else if (!(row - 1))
						row = wp->nrows - 1;
					else
						row = row - 2;

					break;
				case 300:
					col = (!col) ? wp->ncols - 1 : col - 1;
					if (!row)
						row = wp->nrows - 2;
					else if (!(row - 1))
						row = wp->nrows - 1;
					else
						row = row - 2;
					break;
				case 320:
				case 330:
					col = (!col) ? wp->ncols - 1 : col - 1;
					row = (!row) ? wp->nrows - 1 : row - 1;
					break;
				default:
					(void) fprintf(stderr, "wrong direction %d\n", dir);
			}
		} else {	/* left */
			switch (dir) {
				case 0:
					col = (col + 1 == wp->ncols) ? 0 : col + 1;
					break;
				case 30:
				case 40:
					col = (col + 1 == wp->ncols) ? 0 : col + 1;
					row = (!row) ? wp->nrows - 1 : row - 1;
					break;
				case 60:
					col = (col + 1 == wp->ncols) ? 0 : col + 1;
					if (!row)
						row = wp->nrows - 2;
					else if (row == 1)
						row = wp->nrows - 1;
					else
						row = row - 2;
					break;
				case 80:
				case 90:
					if (!row)
						row = wp->nrows - 2;
					else if (row == 1)
						row = wp->nrows - 1;
					else
						row = row - 2;
					break;
				case 120:
					row = (!row) ? wp->nrows - 1 : row - 1;
					break;
				case 150:
				case 160:
					col = (!col) ? wp->ncols - 1 : col - 1;
					row = (!row) ? wp->nrows - 1 : row - 1;
					break;
				case 180:
					col = (!col) ? wp->ncols - 1 : col - 1;
					break;
				case 200:
				case 210:
					col = (!col) ? wp->ncols - 1 : col - 1;
					row = (row + 1 == wp->nrows) ? 0 : row + 1;
					break;
				case 240:
					row = (row + 1 == wp->nrows) ? 0 : row + 1;
					break;
				case 270:
				case 280:
					if (row + 1 == wp->nrows)
						row = 1;
					else if (row + 2 == wp->nrows)
						row = 0;
					else
						row = row + 2;
					break;
				case 300:
					col = (col + 1 == wp->ncols) ? 0 : col + 1;
					if (row + 1 == wp->nrows)
						row = 1;
					else if (row + 2 == wp->nrows)
						row = 0;
					else
						row = row + 2;
					break;
				case 320:
				case 330:
					col = (col + 1 == wp->ncols) ? 0 : col + 1;
					row = (row + 1 == wp->nrows) ? 0 : row + 1;
					break;
				default:
					(void) fprintf(stderr, "wrong direction %d\n", dir);
			}
		}
	}
	return row * wp->ncols + col;
}

void
init_wator(ModeInfo * mi)
{
	int         size = MI_SIZE(mi);
	watorstruct *wp;
	int         i, col, row, colrow, kind;
	cellstruct  info;

	if (wators == NULL) {
		if ((wators = (watorstruct *) calloc(MI_NUM_SCREENS(mi),
					      sizeof (watorstruct))) == NULL)
			return;
	}
	wp = &wators[MI_SCREEN(mi)];

	wp->generation = 0;
	if (!wp->firstkind[0]) {	/* Genesis */
		icon_width = fish0_width;
		icon_height = fish0_height;
		/* Set up what will be a 'triply' linked list.
		   doubly linked list, doubly linked to an array */
		for (kind = FISH; kind <= KINDS; kind++)
			init_kindlist(wp, kind);
		for (i = 0; i < BITMAPS; i++) {
			logo[i].width = icon_width;
			logo[i].height = icon_height;
			logo[i].bytes_per_line = (icon_width + 7) / 8;
		}
	} else			/* Exterminate all  */
		for (i = FISH; i <= KINDS; i++)
			flush_kindlist(wp, i);

	wp->width = MI_WIDTH(mi);
	wp->height = MI_HEIGHT(mi);
	if (wp->width < 2)
		wp->width = 2;
	if (wp->height < 2)
		wp->height = 2;

	for (i = 0; i < NEIGHBORKINDS; i++) {
		if (neighbors == plots[i]) {
			wp->neighbors = neighbors;
			break;
		}
		if (i == NEIGHBORKINDS - 1) {
#if 0
			wp->neighbors = plots[NRAND(NEIGHBORKINDS)];
			wp->neighbors = (LRAND() & 1) ? 4 : 8;
#else
			wp->neighbors = 4;
#endif
			break;
		}
	}

	if (wp->neighbors == 6) {
		int         nccols, ncrows, sides;

		if (wp->width < 2)
			wp->width = 2;
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
		wp->pixelmode = True;
		nccols = MAX(wp->width / wp->xs - 2, 2);
		ncrows = MAX(wp->height / wp->ys - 1, 2);
		wp->ncols = nccols / 2;
		wp->nrows = 2 * (ncrows / 4);
		wp->xb = (wp->width - wp->xs * nccols) / 2 + wp->xs / 2;
		wp->yb = (wp->height - wp->ys * (ncrows / 2) * 2) / 2 + wp->ys;
		for (sides = 0; sides < 6; sides++) {
			wp->shape.hexagon[sides].x = (wp->xs - 1) * hexagonUnit[sides].x;
			wp->shape.hexagon[sides].y =
				((wp->ys - 1) * hexagonUnit[sides].y / 2) * 4 / 3;
		}
	} else if (wp->neighbors == 4 || wp->neighbors == 8) {
		if (wp->width < 2)
			wp->width = 2;
		if (wp->height < 2)
			wp->height = 2;
		if (size == 0 ||
		    MINGRIDSIZE * size > wp->width || MINGRIDSIZE * size > wp->height) {
			if (wp->width > MINGRIDSIZE * icon_width &&
			    wp->height > MINGRIDSIZE * icon_height) {
				wp->pixelmode = False;
				wp->xs = icon_width;
				wp->ys = icon_height;
			} else {
				wp->pixelmode = True;
				wp->xs = wp->ys = MAX(MINSIZE, MIN(wp->width, wp->height) /
						      MINGRIDSIZE);
			}
		} else {
			wp->pixelmode = True;
			if (size < -MINSIZE)
				wp->ys = NRAND(MIN(-size, MAX(MINSIZE, MIN(wp->width, wp->height) /
				      MINGRIDSIZE)) - MINSIZE + 1) + MINSIZE;
			else if (size < MINSIZE)
				wp->ys = MINSIZE;
			else
				wp->ys = MIN(size, MAX(MINSIZE, MIN(wp->width, wp->height) /
						       MINGRIDSIZE));
			wp->xs = wp->ys;
		}
		wp->ncols = MAX(wp->width / wp->xs, 2);
		wp->nrows = MAX(wp->height / wp->ys, 2);
		wp->xb = (wp->width - wp->xs * wp->ncols) / 2;
		wp->yb = (wp->height - wp->ys * wp->nrows) / 2;
	} else {		/* TRI */
		int         orient, sides;

		if (wp->width < 2)
			wp->width = 2;
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
		wp->pixelmode = True;
		wp->ncols = (MAX(wp->width / wp->xs - 1, 2) / 2) * 2;
		wp->nrows = (MAX(wp->height / wp->ys - 1, 2) / 2) * 2;
		wp->xb = (wp->width - wp->xs * wp->ncols) / 2 + wp->xs / 2;
		wp->yb = (wp->height - wp->ys * wp->nrows) / 2 + wp->ys / 2;
		for (orient = 0; orient < 2; orient++) {
			for (sides = 0; sides < 3; sides++) {
				wp->shape.triangle[orient][sides].x =
					(wp->xs - 2) * triangleUnit[orient][sides].x;
				wp->shape.triangle[orient][sides].y =
					(wp->ys - 2) * triangleUnit[orient][sides].y;
			}
		}
	}

	wp->positions = wp->ncols * wp->nrows;

	if (wp->arr != NULL)
		(void) free((void *) wp->arr);
	wp->arr = (CellList **) calloc(wp->positions, sizeof (CellList *));

	/* Play G-d with these numbers */
	wp->nkind[FISH] = wp->positions / 3;
	wp->nkind[SHARK] = wp->nkind[FISH] / 10;
	wp->kind = FISH;
	if (!wp->nkind[SHARK])
		wp->nkind[SHARK] = 1;
	wp->breed[FISH] = MI_COUNT(mi);
	wp->breed[SHARK] = 10;
	if (wp->breed[FISH] < 1)
		wp->breed[FISH] = 1;
	else if (wp->breed[FISH] > wp->breed[SHARK])
		wp->breed[FISH] = 4;
	wp->sstarve = 3;

	MI_CLEARWINDOW(mi);
	wp->painted = False;

	for (kind = FISH; kind <= SHARK; kind++) {
		i = 0;
		while (i < wp->nkind[kind]) {
			col = NRAND(wp->ncols);
			row = NRAND(wp->nrows);
			colrow = col + row * wp->ncols;
			if (!wp->arr[colrow]) {
				i++;
				info.kind = kind;
				info.age = NRAND(wp->breed[kind]);
				info.food = NRAND(wp->sstarve);
				info.direction = NRAND(KINDBITMAPS) + kind * KINDBITMAPS;
				if (MI_NPIXELS(mi) > 2)
					info.color = NRAND(MI_NPIXELS(mi));
				else
					info.color = 0;
				info.col = col;
				info.row = row;
				addto_kindlist(wp, kind, info);
				wp->arr[colrow] = wp->currkind;
				drawcell(mi, col, row,
					 wp->currkind->info.color, wp->currkind->info.direction, True);
			}
		}
	}
}

void
draw_wator(ModeInfo * mi)
{
	watorstruct *wp = &wators[MI_SCREEN(mi)];
	int         col, row;
	int         colrow, cr, position;
	int         i, numok;

	struct {
		int         pos, dir;
	} acell[12];


	MI_IS_DRAWN(mi) = True;

	wp->painted = True;
	/* Alternate updates, fish and sharks live out of phase with each other */
	wp->kind = (wp->kind + 1) % KINDS;
	{
		wp->currkind = wp->firstkind[wp->kind]->next;

		while (wp->currkind != wp->lastkind[wp->kind]) {
			col = wp->currkind->info.col;
			row = wp->currkind->info.row;
			colrow = col + row * wp->ncols;
			numok = 0;
			if (wp->kind == SHARK) {	/* Scan for fish */
				for (i = 0; i < wp->neighbors; i++) {
					position = neighbor_position(wp, col, row, i * 360 / wp->neighbors);
					if (wp->arr[position] && wp->arr[position]->info.kind == FISH) {
						acell[numok].pos = position;
						acell[numok++].dir = i;
					}
				}
				if (numok) {	/* No thanks, I'm a vegetarian */
					i = NRAND(numok);
					wp->nkind[FISH]--;
					cr = acell[i].pos;
					removefrom_kindlist(wp, wp->arr[cr]);
					wp->arr[cr] = wp->currkind;
					if (wp->neighbors == 4) {
						wp->currkind->info.direction = (5 - acell[i].dir) % ORIENTS +
							((NRAND(REFLECTS)) ? 0 : ORIENTS) + wp->kind * KINDBITMAPS;
					} else if (wp->neighbors == 8) {
						wp->currkind->info.direction = (char) (5 - (acell[i].dir / 2 +
											    ((acell[i].dir % 2) ? LRAND() & 1 : 0))) % ORIENTS +
							((NRAND(REFLECTS)) ? 0 : ORIENTS) + wp->kind * KINDBITMAPS;
					} else
						wp->currkind->info.direction = wp->kind * KINDBITMAPS;
					wp->currkind->info.col = acell[i].pos % wp->ncols;
					wp->currkind->info.row = acell[i].pos / wp->ncols;
					wp->currkind->info.food = wp->sstarve;
					drawcell(mi, wp->currkind->info.col, wp->currkind->info.row,
						 wp->currkind->info.color, wp->currkind->info.direction, True);
					if (++(wp->currkind->info.age) >= wp->breed[wp->kind]) {	/* breed */
						cutfrom_kindlist(wp);	/* This rotates out who goes first */
						wp->babykind->info.age = 0;
						dupin_kindlist(wp);
						wp->arr[colrow] = wp->babykind;
						wp->babykind->info.col = col;
						wp->babykind->info.row = row;
						wp->babykind->info.age = -1;	/* Make one a little younger */
#if 0
						if (MI_NPIXELS(mi) > 2 && (LRAND() & 1))
							/* A color mutation */
							if (++(wp->babykind->info.color) >= MI_NPIXELS(mi))
								wp->babykind->info.color = 0;
#endif
						wp->nkind[wp->kind]++;
					} else {
						wp->arr[colrow] = 0;
						drawcell(mi, col, row, 0, 0, False);
					}
				} else {
					if (wp->currkind->info.food-- < 0) {	/* Time to die, Jaws */
						/* back up one or else in void */
						wp->currkind = wp->currkind->previous;
						removefrom_kindlist(wp, wp->arr[colrow]);
						wp->arr[colrow] = 0;
						drawcell(mi, col, row, 0, 0, False);
						wp->nkind[wp->kind]--;
						numok = -1;	/* Want to escape from next if */
					}
				}
			}
			if (!numok) {	/* Fish or shark search for a place to go */
				for (i = 0; i < wp->neighbors; i++) {
					position = neighbor_position(wp, col, row, i * 360 / wp->neighbors);
					if (!wp->arr[position]) {	/* Found an empty spot */
						acell[numok].pos = position;
						acell[numok++].dir = i;
					}
				}
				if (numok) {	/* Found a place to go */
					i = NRAND(numok);
					wp->arr[acell[i].pos] = wp->currkind;
					if (wp->neighbors == 4) {
						wp->currkind->info.direction = (5 - acell[i].dir) % ORIENTS +
							((NRAND(REFLECTS)) ? 0 : ORIENTS) + wp->kind * KINDBITMAPS;
					} else if (wp->neighbors == 8) {
						wp->currkind->info.direction = (char) (5 - (acell[i].dir / 2 +
											    ((acell[i].dir % 2) ? LRAND() & 1 : 0))) % ORIENTS +
							((NRAND(REFLECTS)) ? 0 : ORIENTS) + wp->kind * KINDBITMAPS;
					} else
						wp->currkind->info.direction = wp->kind * KINDBITMAPS;
					wp->currkind->info.col = acell[i].pos % wp->ncols;
					wp->currkind->info.row = acell[i].pos / wp->ncols;
					drawcell(mi,
						 wp->currkind->info.col, wp->currkind->info.row,
						 wp->currkind->info.color, wp->currkind->info.direction, True);
					if (++(wp->currkind->info.age) >= wp->breed[wp->kind]) {	/* breed */
						cutfrom_kindlist(wp);	/* This rotates out who goes first */
						wp->babykind->info.age = 0;
						dupin_kindlist(wp);
						wp->arr[colrow] = wp->babykind;
						wp->babykind->info.col = col;
						wp->babykind->info.row = row;
						wp->babykind->info.age = -1;	/* Make one a little younger */
						wp->nkind[wp->kind]++;
					} else {
						wp->arr[colrow] = 0;
						drawcell(mi, col, row, 0, 0, False);
					}
				} else {
					/* I'll just sit here and wave my tail so you know I am alive */
					wp->currkind->info.direction =
						(wp->currkind->info.direction + ORIENTS) % KINDBITMAPS +
						wp->kind * KINDBITMAPS;
					drawcell(mi, col, row, wp->currkind->info.color,
					 wp->currkind->info.direction, True);
				}
			}
			wp->currkind = wp->currkind->next;
		}
		reattach_kindlist(wp, wp->kind);
	}

	if ((wp->nkind[FISH] >= wp->positions) ||
	    (!wp->nkind[FISH] && !wp->nkind[SHARK]) ||
	    wp->generation >= MI_CYCLES(mi)) {
		init_wator(mi);
	}
	if (wp->kind == SHARK)
		wp->generation++;
}

void
release_wator(ModeInfo * mi)
{
	if (wators != NULL) {
		int         screen, kind;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			watorstruct *wp = &wators[screen];

			for (kind = 0; kind <= KINDS; kind++) {
				if (wp->firstkind[0])
					flush_kindlist(wp, kind);
				if (wp->lastkind[kind])
					(void) free((void *) wp->lastkind[kind]);
				if (wp->firstkind[kind])
					(void) free((void *) wp->firstkind[kind]);
			}
			if (wp->arr != NULL)
				(void) free((void *) wp->arr);
		}
		(void) free((void *) wators);
		wators = NULL;
	}
}

void
refresh_wator(ModeInfo * mi)
{
	watorstruct *wp = &wators[MI_SCREEN(mi)];

	if (wp->painted) {
		MI_CLEARWINDOW(mi);
		wp->painted = False;
	}
}

#endif /* MODE_wator */
