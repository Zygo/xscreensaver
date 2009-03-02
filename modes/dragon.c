/* -*- Mode: C; tab-width: 4 -*- */
/* dragon --- Oskar van Deventer's Hexagonal Dragons Maze */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)dragon.c	5.01 2001/03/07 xlockmore";

#endif

/*-
 * Copyright (c) 2001 by David Bagley.
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
 * From "Fractal Garden Maze" by Oskar van Deventer, Cubism For Fun
 * Issue 54 February 2001 p16, 17.  The maze itself is called a "Hexagonal
 * Dragons Maze", named after its similarity to the Dragon Curve (see
 * turtle.c).
 *
 * Revision History:
 * 07-Mar-2001: Started writing, based on demon mode
 */

/*-
  Grid     Number of Neighbors
  ----     ------------------
  Hexagon  6
*/

#ifdef STANDALONE
#define PROGCLASS "Dragon"
#define HACK_INIT init_dragon
#define HACK_DRAW draw_dragon
#define dragon_opts xlockmore_opts
#define DEFAULTS "*delay: 2000000 \n" \
 "*cycles: 16 \n" \
 "*size: -24 \n" \
 "*ncolors: 64 \n" \
 "*neighbors: 6 \n"
#define UNIFORM_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */

#endif /* STANDALONE */
#include "automata.h"

#ifdef MODE_dragon

ModeSpecOpt dragon_opts =
{0, (XrmOptionDescRec *) NULL, 0, (argtype *) NULL, (OptionStruct *) NULL};

#ifdef USE_MODULES
ModStruct   dragon_description =
{"dragon", "init_dragon", "draw_dragon", "release_dragon",
 "refresh_dragon", "init_dragon", NULL, &dragon_opts,
 2000000, 1, 16, -24, 64, 1.0, "",
 "Shows Deventer's Hexagonal Dragons Maze", 0, NULL};

#endif

#include "bitmaps/gray1.xbm"

#define STATES 3
#define LISTS 2
#define REDRAWSTEP 2000		/* How many cells to draw per cycle */
#define MINGRIDSIZE 16 
#define MINSIZE 4

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
	int         addlist;
	int         redrawing, redrawpos;
	int        *ncells;
	unsigned long color;
	CellList  **cellList;
	unsigned char *oldcell;
	GC          stippledGC;
	Pixmap      graypix;
	XPoint      hexagon[6];
} dragonstruct;

static dragonstruct *dragons = NULL;

static void
drawcell(ModeInfo * mi, int col, int row, unsigned char state)
{
	dragonstruct *dp = &dragons[MI_SCREEN(mi)];
	Display    *display = MI_DISPLAY(mi);
	GC          gc;

	if (!state) {
		XSetForeground(display, MI_GC(mi), MI_WHITE_PIXEL(mi));
		gc = MI_GC(mi);
	} else if (state == 1) {
		XSetForeground(display, MI_GC(mi), MI_BLACK_PIXEL(mi));
		gc = MI_GC(mi);
	} else if (MI_NPIXELS(mi) > 2) {
		XSetForeground(display, MI_GC(mi), dp->color);
		gc = MI_GC(mi);
	} else {
		XGCValues   gcv;

		gcv.stipple = dp->graypix;
		gcv.foreground = MI_WHITE_PIXEL(mi);
		gcv.background = MI_BLACK_PIXEL(mi);
		XChangeGC(display, dp->stippledGC,
			  GCStipple | GCForeground | GCBackground, &gcv);
		gc = dp->stippledGC;
	}
	{
		int         ccol = 2 * col + !(row & 1), crow = 2 * row;

		dp->hexagon[0].x = dp->xb + ccol * dp->xs;
		dp->hexagon[0].y = dp->yb + crow * dp->ys;
		if (dp->xs == 1 && dp->ys == 1)
			XDrawPoint(display, MI_WINDOW(mi),
				       gc, dp->hexagon[0].x, dp->hexagon[0].y);
		else
			XFillPolygon(display, MI_WINDOW(mi), gc,
			    dp->hexagon, 6, Convex, CoordModePrevious);
	}
}

static Bool
addtolist(ModeInfo * mi, int col, int row)
{
	dragonstruct *dp = &dragons[MI_SCREEN(mi)];
	CellList   *current;

	current = dp->cellList[dp->addlist];
	if ((dp->cellList[dp->addlist] = (CellList *)
		malloc(sizeof (CellList))) == NULL) {
		return False;
	}
	dp->cellList[dp->addlist]->pt.x = col;
	dp->cellList[dp->addlist]->pt.y = row;
	dp->cellList[dp->addlist]->next = current;
	dp->ncells[dp->addlist]++;
	return True;
}

#ifdef DEBUG
static void
print_state(ModeInfo * mi, int state)
{
	dragonstruct *dp = &dragons[MI_SCREEN(mi)];
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
free_alist(dragonstruct * dp, int list)
{
	CellList   *current;

	while (dp->cellList[list]) {
		current = dp->cellList[list];
		dp->cellList[list] = dp->cellList[list]->next;
		(void) free((void *) current);
	}
	dp->cellList[list] = NULL;
	if (dp->ncells != NULL)
		dp->ncells[list] = 0;
}


static void
free_list(dragonstruct * dp)
{
	int         list;

	for (list = 0; list < LISTS; list++)
		free_alist(dp, list);
	(void) free((void *) dp->cellList);
	dp->cellList = NULL;
	if (dp->ncells != NULL) {
		(void) free((void *) dp->ncells);
		dp->ncells = NULL;
	}
}

static void
free_struct(dragonstruct * dp)
{
	if (dp->cellList != NULL) {
		free_list(dp);
	}
	if (dp->oldcell != NULL) {
		(void) free((void *) dp->oldcell);
		dp->oldcell = NULL;
	}
}

static void
free_dragon(Display *display, dragonstruct *dp)
{
	if (dp->stippledGC != None) {
		XFreeGC(display, dp->stippledGC);
		dp->stippledGC = None;
	}
	if (dp->graypix != None) {
		XFreePixmap(display, dp->graypix);
		dp->graypix = None;
	}
	free_struct(dp);
}

#if 0
static Bool
draw_state(ModeInfo * mi, int state)
{
	dragonstruct *dp = &dragons[MI_SCREEN(mi)];
	Display    *display = MI_DISPLAY(mi);
	GC          gc;
	CellList   *current;

	if (!state) {
		XSetForeground(display, MI_GC(mi), MI_WHITE_PIXEL(mi));
		gc = MI_GC(mi);
	} else if (state == 1) {
		XSetForeground(display, MI_GC(mi), MI_BLACK_PIXEL(mi));
		gc = MI_GC(mi);
	} else if (MI_NPIXELS(mi) > 2) {
		XSetForeground(display, MI_GC(mi), dp->color);
		gc = MI_GC(mi);
	} else {
		XGCValues   gcv;

		gcv.stipple = dp->graypix;
		gcv.foreground = MI_WHITE_PIXEL(mi);
		gcv.background = MI_BLACK_PIXEL(mi);
		XChangeGC(display, dp->stippledGC,
			  GCStipple | GCForeground | GCBackground, &gcv);
		gc = dp->stippledGC;
	}
	{	/* Draw right away, slow */
		current = dp->cellList[state];
		while (current) {
			int         col, row, ccol, crow;

			col = current->pt.x;
			row = current->pt.y;
			ccol = 2 * col + !(row & 1), crow = 2 * row;
			dp->hexagon[0].x = dp->xb + ccol * dp->xs;
			dp->hexagon[0].y = dp->yb + crow * dp->ys;
			if (dp->xs == 1 && dp->ys == 1)
				XDrawPoint(display, MI_WINDOW(mi),
					       gc, dp->hexagon[0].x, dp->hexagon[0].y);
			else
				XFillPolygon(display, MI_WINDOW(mi), gc,
					     dp->hexagon, 6, Convex, CoordModePrevious);
			current = current->next;
		}
	}
	XFlush(MI_DISPLAY(mi));
	return True;
}
#endif

static Bool
SetSoup(ModeInfo * mi)
{
	dragonstruct *dp = &dragons[MI_SCREEN(mi)];
	int         row, col, mrow = 0;

	for (row = dp->nrows - 1; row >= 0; --row) {
		for (col = dp->ncols - 1; col >= 0; --col) {
			if (!addtolist(mi, col, row))
				return False;
		}
		mrow += dp->ncols;
	}
	dp->addlist = !dp->addlist;
	return True;
}

void
init_dragon(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         size = MI_SIZE(mi);
	dragonstruct *dp;

	if (dragons == NULL) {
		if ((dragons = (dragonstruct *) calloc(MI_NUM_SCREENS(mi),
					      sizeof (dragonstruct))) == NULL)
			return;
	}
	dp = &dragons[MI_SCREEN(mi)];

	dp->generation = 0;
	dp->redrawing = 0;
	if (MI_NPIXELS(mi) <= 2) {
		if (dp->stippledGC == None) {
			XGCValues   gcv;

			gcv.fill_style = FillOpaqueStippled;
			if ((dp->stippledGC = XCreateGC(display, window,
				 GCFillStyle, &gcv)) == None) {
				free_dragon(display, dp);
				return;
			}
		}
		if (dp->graypix == None) {
			if ((dp->graypix = XCreateBitmapFromData(display, window,
				(char *) gray1_bits, gray1_width, gray1_height)) == None) {
				free_dragon(display, dp);
				return;
			}
		}
	}
	free_struct(dp);
	if (MI_NPIXELS(mi) > 2)
		dp->color = MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));

	if ((dp->cellList = (CellList **) calloc(STATES,
			sizeof (CellList *))) == NULL) {
		free_dragon(display, dp);
		return;
	}
	if ((dp->ncells = (int *) calloc(STATES, sizeof (int))) == NULL) {
		free_dragon(display, dp);
		return;
	}

	dp->addlist = 0;

	dp->width = MI_WIDTH(mi);
	dp->height = MI_HEIGHT(mi);

	{
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
		dp->yb = (dp->height - dp->ys * (ncrows / 2) * 2) / 2 + dp->ys - 2;
		for (i = 0; i < 6; i++) {
			dp->hexagon[i].x = dp->xs * hexagonUnit[i].x;
			dp->hexagon[i].y = ((dp->ys + 1) * hexagonUnit[i].y / 2) * 4 / 3;
		}
	}

	MI_CLEARWINDOW(mi);

	if ((dp->oldcell = (unsigned char *)
		calloc(dp->ncols * dp->nrows,
			sizeof (unsigned char))) == NULL) {
		free_dragon(display, dp);
		return;
	}
	if (!SetSoup(mi)) {
		free_dragon(display, dp);
		return;
	}
}

void
draw_dragon(ModeInfo * mi)
{
	int white, black;
	int         choose_layer, factor, orient, l;
	dragonstruct *dp;
	CellList *locallist;
	Bool detour = False;

	if (dragons == NULL)
		return;
	dp = &dragons[MI_SCREEN(mi)];
	if (dp->cellList == NULL)
		return;

	MI_IS_DRAWN(mi) = True;
	choose_layer= NRAND(6);
	if (dp->ncells[!dp->addlist] == 1) {
		/* Since the maze is infinite, it may not get to this last
		 * spot for a while.  Force it to do it right away so it
		 * does not appear to be stuck. */
		detour = True; 
		white = black = 0; /* not used but make -Wall happy */
	} else {
		white = (choose_layer / 2);
		black = (choose_layer % 2) ?
			((white + 2) % 3) : ((white + 1) % 3);
		/* gray = (choose_layer % 2) ?
			((white + 1) % 3) : ((white + 2) % 3); */
	} 
	locallist = dp->cellList[!dp->addlist];
	orient = dp->generation % 2;
	factor = 1;
	for (l = 0; l < dp->generation / 2; l++) {
		factor *= 3;
	}
	if (!locallist && dp->generation >= MI_CYCLES(mi)) {
		init_dragon(mi);
		return;
	}	

	while (locallist) {
		int i, j, k;

		i = locallist->pt.x;
		j = locallist->pt.y;
		if (orient) {
			k = (j / factor) % 3;
		} else {
		 	if (j % 2) {
				/* Had trouble with this line... */
				k = ((i + factor / 2) / factor + 1) % 3;
			} else {
				k = (i / factor) % 3;
			}
		}
		if (detour) {
			k = (LRAND() & 1) + 1;
			dp->oldcell[j * dp->ncols + i] = k;
			drawcell(mi, i, j, k);
		} if (white == k) {
			dp->oldcell[j * dp->ncols + i] = 0;
			drawcell(mi, i, j, 0);
			if (!addtolist(mi, i, j)) {
				free_dragon(MI_DISPLAY(mi), dp);
				return;
			}
		} else if (black == k) {
			dp->oldcell[j * dp->ncols + i] = 1;
			drawcell(mi, i, j, 1);
		} else /* if (gray == k) */ {
			dp->oldcell[j * dp->ncols + i] = 2;
			drawcell(mi, i, j, 2);
		}
		dp->cellList[!dp->addlist] = dp->cellList[!dp->addlist]->next;
		(void) free((void *) locallist);
		dp->ncells[!dp->addlist]--;
		locallist = dp->cellList[!dp->addlist];
		if ((dp->cellList[!dp->addlist] == NULL) &&
		    (dp->cellList[dp->addlist] == NULL))
			dp->generation = 0;
	}
	dp->addlist = !dp->addlist;
	if (dp->redrawing) {
		int i;

		for (i = 0; i < REDRAWSTEP; i++) {
			if (dp->oldcell[dp->redrawpos] != 1) {
				drawcell(mi, dp->redrawpos % dp->ncols, dp->redrawpos / dp->ncols,
					 dp->oldcell[dp->redrawpos]);
			}
			if (++(dp->redrawpos) >= dp->ncols * dp->nrows) {
				dp->redrawing = 0;
				break;
			}
		}
	}
	dp->generation++;
}
void
release_dragon(ModeInfo * mi)
{
	if (dragons != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_dragon(MI_DISPLAY(mi), &dragons[screen]);
		(void) free((void *) dragons);
		dragons = NULL;
	}
}

void
refresh_dragon(ModeInfo * mi)
{
	dragonstruct *dp;

	if (dragons == NULL)
		return;
	dp = &dragons[MI_SCREEN(mi)];

	dp->redrawing = 1;
	dp->redrawpos = 0;
}

#endif /* MODE_dragon */
