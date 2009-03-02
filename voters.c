/* -*- Mode: C; tab-width: 4 -*- */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)voters.c	4.03 97/06/10 xlockmore";

#endif

/*-
 * voters.c - Dewdney's Voters, voting simulation for xlock,
 *            the X Window System lockscreen.
 *
 * Copyright (c) 1997 by David Bagley.
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
 * 10-Jun-97: Coded from A.K. Dewdney's "The Armchair Universe, Computer
 *            Recreations from the Pages of Scientific American Magazine"
 *            W.H. Freedman and Company, New York, 1988  (Apr 1985)
 *            Used wator.c and demon.c as a guide.
 */

#ifdef STANDALONE
#define PROGCLASS            "Voters"
#define HACK_INIT            init_voters
#define HACK_DRAW            draw_voters
#define DEF_DELAY            750000
#define DEF_CYCLES           32767
#define DEF_SIZE             0
#define DEF_NCOLORS          200
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
ModeSpecOpt voters_opts =
{0, NULL, 0, NULL, NULL};

#endif /* STANDALONE */

extern int  neighbors;

/*-
 * From far left to right, at least in the currently in the US.  By the way, I
 * consider myself to be a proud bleeding heart liberal democrat, in
 * case anyone wants to know....  Please, no fascist "improvements".  :)
 */

#include "bitmaps/sickle.xbm"
#include "bitmaps/donkey.xbm"
#include "bitmaps/elephant.xbm"

#define MINPARTIES 2
#define BITMAPS 3
#define MINGRIDSIZE 10
#define MINSIZE 4
#define FACTOR 10
#define NEIGHBORKINDS 6

static XImage logo[BITMAPS] =
{
      {0, 0, 0, XYBitmap, (char *) sickle_bits, LSBFirst, 8, LSBFirst, 8, 1},
      {0, 0, 0, XYBitmap, (char *) donkey_bits, LSBFirst, 8, LSBFirst, 8, 1},
    {0, 0, 0, XYBitmap, (char *) elephant_bits, LSBFirst, 8, LSBFirst, 8, 1},
};

/* Voter data */
typedef struct {
	char        kind;
	int         age;
	int         col, row;
} cellstruct;

/* Doublely linked list */
typedef struct _CellList {
	cellstruct  info;
	struct _CellList *previous, *next;
} CellList;

typedef struct {
	int         initialized;
	int         party;	/* Currently working on donkey, elephant, or sickle? */
	int         xs, ys;	/* Size of fish and sharks */
	int         xb, yb;	/* Bitmap offset for fish and sharks */
	int         nparties;	/* 2 parties or 3 */
	int         number_in_party[BITMAPS];	/* Good to know when one party rules */
	int         pixelmode;
	int         generation;
	int         ncols, nrows;
	int         npositions;
	int         width, height;
	CellList   *last, *first;
	char       *arr;
	int         neighbors;
	XPoint      hexagonList[6];
	XPoint      triangleList[2][3];
} voterstruct;

static int  initVal[NEIGHBORKINDS] =
{
	3, 4, 6, 8, 9, 12	/* Neighborhoods */
};

static voterstruct *voters = NULL;
static int  icon_width, icon_height;


static void
drawcell(ModeInfo * mi, int col, int row, unsigned long color, int bitmap,
	 Bool firstChange)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	voterstruct *vp = &voters[MI_SCREEN(mi)];
	unsigned long colour = (MI_NPIXELS(mi) > 2) ?
	MI_PIXEL(mi, color) : MI_WIN_WHITE_PIXEL(mi);

	XSetForeground(display, gc, colour);
	if (vp->neighbors == 6) {
		int         ccol = 2 * col + !(row & 1), crow = 2 * row;

		vp->hexagonList[0].x = vp->xb + ccol * vp->xs;
		vp->hexagonList[0].y = vp->yb + crow * vp->ys;
		if (vp->xs == 1 && vp->ys == 1)
			XFillRectangle(display, window, gc,
			   vp->hexagonList[0].x, vp->hexagonList[0].y, 1, 1);
		else if (bitmap == BITMAPS - 1)
			XFillPolygon(display, window, gc,
			      vp->hexagonList, 6, Convex, CoordModePrevious);
		else {
			if (firstChange) {
				XSetForeground(display, gc, MI_WIN_BLACK_PIXEL(mi));
				XFillPolygon(display, window, gc,
			      vp->hexagonList, 6, Convex, CoordModePrevious);
				XSetForeground(display, gc, colour);
			}
		  if (vp->xs <= 6 || vp->ys <= 2)
			XFillRectangle(display, window, gc,
			   vp->hexagonList[0].x  - 3 * vp->xs / 4,
         vp->hexagonList[0].y + vp->ys / 4, vp->xs, vp->ys);
      else
			XFillArc(display, window, gc,
				 vp->xb + vp->xs * ccol - 3 * vp->xs / 4,
				 vp->yb + vp->ys * crow + vp->ys / 4,
				 2 * vp->xs - 6, 2 * vp->ys - 2,
				 0, 23040);
		}
	} else if (vp->neighbors == 4 || vp->neighbors == 8) {
		if (vp->pixelmode) {
			if (bitmap == BITMAPS - 1 || (vp->xs == 1 && vp->ys == 1))
				XFillRectangle(display, window, gc,
					       vp->xb + vp->xs * col, vp->yb + vp->ys * row,
				         vp->xs, vp->ys);
			else {
				if (firstChange) {
					XSetForeground(display, gc, MI_WIN_BLACK_PIXEL(mi));
					XFillRectangle(display, window, gc,
						  vp->xb + vp->xs * col, vp->yb + vp->ys * row,
					    vp->xs, vp->ys);
					XSetForeground(display, gc, colour);
				}
				XFillArc(display, window, gc,
					 vp->xb + vp->xs * col, vp->yb + vp->ys * row, vp->xs, vp->ys,
					 0, 23040);
			}
		} else
			XPutImage(display, window, gc,
				  &logo[bitmap], 0, 0,
				vp->xb + vp->xs * col, vp->yb + vp->ys * row,
				  icon_width, icon_height);
	} else {		/* TRI */
		int         orient = (col + row) % 2;	/* O left 1 right */

		vp->triangleList[orient][0].x = vp->xb + col * vp->xs;
		vp->triangleList[orient][0].y = vp->yb + row * vp->ys;
		if (vp->xs <= 3 || vp->ys <= 3)
			XFillRectangle(display, window, gc,
			 ((orient) ? -1 : 1) + vp->triangleList[orient][0].x,
				       vp->triangleList[orient][0].y, 1, 1);
		else {
			if (orient)
				vp->triangleList[orient][0].x += (vp->xs / 2 - 1);
			else
				vp->triangleList[orient][0].x -= (vp->xs / 2 - 1);
			if (bitmap == BITMAPS - 1)
				XFillPolygon(display, window, gc,
					     vp->triangleList[orient], 3, Convex, CoordModePrevious);
			else {
				if (firstChange) {
					XSetForeground(display, gc, MI_WIN_BLACK_PIXEL(mi));
					XFillPolygon(display, window, gc,
						     vp->triangleList[orient], 3, Convex, CoordModePrevious);
					XSetForeground(display, gc, colour);
				}
				XFillArc(display, window, gc,
					 vp->xb + vp->xs * col - 4 * vp->xs / 5  +
						 ((orient) ? vp->xs / 3 : 3 * vp->xs / 5),
					 vp->yb + vp->ys * row - vp->ys / 2 + 1, vp->ys - 3, vp->ys - 3,
					 0, 23040);
			}
		}
	}
}

static void
init_list(voterstruct * vp)
{
	/* Waste some space at the beginning and end of list
	   so we do not have to complicated checks against falling off the ends. */
	vp->last = (CellList *) malloc(sizeof (CellList));
	vp->first = (CellList *) malloc(sizeof (CellList));
	vp->first->previous = vp->last->next = NULL;
	vp->first->next = vp->last->previous = NULL;
	vp->first->next = vp->last;
	vp->last->previous = vp->first;
}

static void
addto_list(voterstruct * vp, cellstruct info)
{
	CellList   *curr;

	curr = (CellList *) malloc(sizeof (CellList));
	vp->last->previous->next = curr;
	curr->previous = vp->last->previous;
	curr->next = vp->last;
	vp->last->previous = curr;
	curr->info = info;
}

static void
removefrom_list(CellList * ptr)
{
	ptr->previous->next = ptr->next;
	ptr->next->previous = ptr->previous;
	(void) free((void *) ptr);
}

static void
flush_list(voterstruct * vp)
{
	CellList   *curr;

	while (vp->last->previous != vp->first) {
		curr = vp->last->previous;
		curr->previous->next = vp->last;
		vp->last->previous = curr->previous;
		(void) free((void *) curr);
	}
}

static char
neighbors_opinion(voterstruct * vp, int col, int row, int dir)
{
	if (vp->neighbors == 6) {
		switch (dir) {
			case 0:
				col = (col + 1 == vp->ncols) ? 0 : col + 1;
				break;
			case 60:
				if (!(row & 1))
					col = (col + 1 == vp->ncols) ? 0 : col + 1;
				row = (!row) ? vp->nrows - 1 : row - 1;
				break;
			case 120:
				if (row & 1)
					col = (!col) ? vp->ncols - 1 : col - 1;
				row = (!row) ? vp->nrows - 1 : row - 1;
				break;
			case 180:
				col = (!col) ? vp->ncols - 1 : col - 1;
				break;
			case 240:
				if (row & 1)
					col = (!col) ? vp->ncols - 1 : col - 1;
				row = (row + 1 == vp->nrows) ? 0 : row + 1;
				break;
			case 300:
				if (!(row & 1))
					col = (col + 1 == vp->ncols) ? 0 : col + 1;
				row = (row + 1 == vp->nrows) ? 0 : row + 1;
				break;
			default:
				(void) fprintf(stderr, "wrong direction %d\n", dir);
		}
	} else if (vp->neighbors == 4 || vp->neighbors == 8) {
		switch (dir) {
			case 0:
				col = (col + 1 == vp->ncols) ? 0 : col + 1;
				break;
			case 45:
				col = (col + 1 == vp->ncols) ? 0 : col + 1;
				row = (!row) ? vp->nrows - 1 : row - 1;
				break;
			case 90:
				row = (!row) ? vp->nrows - 1 : row - 1;
				break;
			case 135:
				col = (!col) ? vp->ncols - 1 : col - 1;
				row = (!row) ? vp->nrows - 1 : row - 1;
				break;
			case 180:
				col = (!col) ? vp->ncols - 1 : col - 1;
				break;
			case 225:
				col = (!col) ? vp->ncols - 1 : col - 1;
				row = (row + 1 == vp->nrows) ? 0 : row + 1;
				break;
			case 270:
				row = (row + 1 == vp->nrows) ? 0 : row + 1;
				break;
			case 315:
				col = (col + 1 == vp->ncols) ? 0 : col + 1;
				row = (row + 1 == vp->nrows) ? 0 : row + 1;
				break;
			default:
				(void) fprintf(stderr, "wrong direction %d\n", dir);
		}
	} else {		/* TRI */
    if ((col + row) % 2) {  /* right */
      switch (dir) {
        case 0:
          col = (!col) ? vp->ncols - 1 : col - 1;
          break;
        case 30:
        case 40:
          col = (!col) ? vp->ncols - 1 : col - 1;
          row = (row + 1 == vp->nrows) ? 0 : row + 1;
          break;
        case 60:
          col = (!col) ? vp->ncols - 1 : col - 1;
          if (row + 1 == vp->nrows)
            row = 1;
          else if (row + 2 == vp->nrows)
            row = 0;
          else
            row = row + 2;
          break;
        case 80:
        case 90:
          if (row + 1 == vp->nrows)
            row = 1;
          else if (row + 2 == vp->nrows)
            row = 0;
          else
            row = row + 2;
          break;
        case 120:
          row = (row + 1 == vp->nrows) ? 0 : row + 1;
          break;
        case 150:
        case 160:
          col = (col + 1 == vp->ncols) ? 0 : col + 1;
          row = (row + 1 == vp->nrows) ? 0 : row + 1;
          break;
        case 180:
          col = (col + 1 == vp->ncols) ? 0 : col + 1;
          break;
        case 200:
        case 210:
          col = (col + 1 == vp->ncols) ? 0 : col + 1;
          row = (!row) ? vp->nrows - 1 : row - 1;
          break;
        case 240:
          row = (!row) ? vp->nrows - 1 : row - 1;
          break;
        case 270:
        case 280:
          if (!row)
            row = vp->nrows - 2;
          else if (!(row - 1))
            row = vp->nrows - 1;
          else
            row = row - 2;

          break;
        case 300:
          col = (!col) ? vp->ncols - 1 : col - 1;
          if (!row)
            row = vp->nrows - 2;
          else if (!(row - 1))
            row = vp->nrows - 1;
          else
            row = row - 2;
          break;
        case 320:
        case 330:
          col = (!col) ? vp->ncols - 1 : col - 1;
          row = (!row) ? vp->nrows - 1 : row - 1;
          break;
        default:
          (void) fprintf(stderr, "wrong direction %d\n", dir);
      }
    } else {  /* left */
      switch (dir) {
        case 0:
          col = (col + 1 == vp->ncols) ? 0 : col + 1;
          break;
        case 30:
        case 40:
          col = (col + 1 == vp->ncols) ? 0 : col + 1;
          row = (!row) ? vp->nrows - 1 : row - 1;
          break;
        case 60:
          col = (col + 1 == vp->ncols) ? 0 : col + 1;
          if (!row)
            row = vp->nrows - 2;
          else if (row == 1)
            row = vp->nrows - 1;
          else
            row = row - 2;
          break;
        case 80:
        case 90:
          if (!row)
            row = vp->nrows - 2;
          else if (row == 1)
            row = vp->nrows - 1;
          else
            row = row - 2;
          break;
        case 120:
          row = (!row) ? vp->nrows - 1 : row - 1;
          break;
        case 150:
        case 160:
          col = (!col) ? vp->ncols - 1 : col - 1;
          row = (!row) ? vp->nrows - 1 : row - 1;
          break;
        case 180:
          col = (!col) ? vp->ncols - 1 : col - 1;
          break;
        case 200:
        case 210:
          col = (!col) ? vp->ncols - 1 : col - 1;
          row = (row + 1 == vp->nrows) ? 0 : row + 1;
          break;
        case 240:
          row = (row + 1 == vp->nrows) ? 0 : row + 1;
          break;
        case 270:
        case 280:
          if (row + 1 == vp->nrows)
            row = 1;
          else if (row + 2 == vp->nrows)
            row = 0;
          else
            row = row + 2;
          break;
        case 300:
          col = (col + 1 == vp->ncols) ? 0 : col + 1;
          if (row + 1 == vp->nrows)
            row = 1;
          else if (row + 2 == vp->nrows)
            row = 0;
          else
            row = row + 2;
          break;
        case 320:
        case 330:
          col = (col + 1 == vp->ncols) ? 0 : col + 1;
          row = (row + 1 == vp->nrows) ? 0 : row + 1;
          break;
        default:
          (void) fprintf(stderr, "wrong direction %d\n", dir);
      }
    }
	}

	return vp->arr[row * vp->ncols + col];
}


static void
advanceColors(ModeInfo * mi, int col, int row)
{
	voterstruct *vp = &voters[MI_SCREEN(mi)];
	CellList   *curr;

	curr = vp->first->next;
	while (curr != vp->last) {
		if (curr->info.col == col && curr->info.row == row) {
			curr = curr->next;
			removefrom_list(curr->previous);
		} else {
			if (curr->info.age > 0)
				curr->info.age--;
			else if (curr->info.age < 0)
				curr->info.age++;
			drawcell(mi, curr->info.col, curr->info.row,
				 (MI_NPIXELS(mi) + curr->info.age / FACTOR +
				  (MI_NPIXELS(mi) * curr->info.kind / BITMAPS)) % MI_NPIXELS(mi),
				 curr->info.kind, False);
			if (curr->info.age == 0) {
				curr = curr->next;
				removefrom_list(curr->previous);
			} else
				curr = curr->next;
		}
	}
}

void
init_voters(ModeInfo * mi)
{
	int         size = MI_SIZE(mi);
	voterstruct *vp;
	int         i, col, row, colrow;

	if (voters == NULL) {
		if ((voters = (voterstruct *) calloc(MI_NUM_SCREENS(mi),
					      sizeof (voterstruct))) == NULL)
			return;
	}
	vp = &voters[MI_SCREEN(mi)];

	vp->generation = 0;
	if (!vp->initialized) {	/* Genesis of democracy */
		icon_width = donkey_width;
		icon_height = donkey_height;
		vp->initialized = 1;
		init_list(vp);
		for (i = 0; i < BITMAPS; i++) {
			logo[i].width = icon_width;
			logo[i].height = icon_height;
			logo[i].bytes_per_line = (icon_width + 7) / 8;
		}
	} else			/* Exterminate all free thinking individuals */
		flush_list(vp);

	vp->width = MI_WIN_WIDTH(mi);
	vp->height = MI_WIN_HEIGHT(mi);

	for (i = 0; i < NEIGHBORKINDS; i++) {
		if (neighbors == initVal[i]) {
			vp->neighbors = neighbors;
			break;
		}
		if (i == NEIGHBORKINDS - 1) {
#if 0
			vp->neighbors = initVal[NRAND(NEIGHBORKINDS)];
			vp->neighbors = (LRAND() & 1) ? 4 : 8;
#else
			vp->neighbors = 8;
#endif
			break;
		}
	}

	if (vp->neighbors == 6) {
		int         nccols, ncrows, i;

		if (vp->width < 2)
			vp->width = 2;
		if (vp->height < 4)
			vp->height = 4;
		if (size < -MINSIZE)
			vp->ys = NRAND(MIN(-size, MAX(MINSIZE, MIN(vp->width, vp->height) /
				      MINGRIDSIZE)) - MINSIZE + 1) + MINSIZE;
		else if (size < MINSIZE) {
			if (!size)
				vp->ys = MAX(MINSIZE, MIN(vp->width, vp->height) / MINGRIDSIZE);
			else
				vp->ys = MINSIZE;
		} else
			vp->ys = MIN(size, MAX(MINSIZE, MIN(vp->width, vp->height) /
					       MINGRIDSIZE));
		vp->xs = vp->ys;
		vp->pixelmode = True;
		nccols = MAX(vp->width / vp->xs - 2, 2);
		ncrows = MAX(vp->height / vp->ys - 1, 2);
		vp->ncols = nccols / 2;
		vp->nrows = 2 * (ncrows / 4);
		vp->xb = (vp->width - vp->xs * nccols) / 2 + vp->xs / 2;
		vp->yb = (vp->height - vp->ys * (ncrows / 2) * 2) / 2 + vp->ys;
		for (i = 0; i < 6; i++) {
			vp->hexagonList[i].x = (vp->xs - 1) * hexagonUnit[i].x;
			vp->hexagonList[i].y = ((vp->ys - 1) * hexagonUnit[i].y / 2) * 4 / 3;
		}
	} else if (vp->neighbors == 4 || vp->neighbors == 8) {
		if (vp->width < 2)
			vp->width = 2;
		if (vp->height < 2)
			vp->height = 2;
		if (size == 0 ||
		    MINGRIDSIZE * size > vp->width || MINGRIDSIZE * size > vp->height) {
			if (vp->width > MINGRIDSIZE * icon_width &&
			    vp->height > MINGRIDSIZE * icon_height) {
				vp->pixelmode = False;
				vp->xs = icon_width;
				vp->ys = icon_height;
			} else {
				vp->pixelmode = True;
				vp->xs = vp->ys = MAX(MINSIZE, MIN(vp->width, vp->height) /
						      MINGRIDSIZE);
			}
		} else {
			vp->pixelmode = True;
			if (size < -MINSIZE)
				vp->ys = NRAND(MIN(-size, MAX(MINSIZE, MIN(vp->width, vp->height) /
				      MINGRIDSIZE)) - MINSIZE + 1) + MINSIZE;
			else if (size < MINSIZE)
				vp->ys = MINSIZE;
			else
				vp->ys = MIN(size, MAX(MINSIZE, MIN(vp->width, vp->height) /
						       MINGRIDSIZE));
			vp->xs = vp->ys;
		}
		vp->ncols = MAX(vp->width / vp->xs, 2);
		vp->nrows = MAX(vp->height / vp->ys, 2);
		vp->xb = (vp->width - vp->xs * vp->ncols) / 2;
		vp->yb = (vp->height - vp->ys * vp->nrows) / 2;
	} else {		/* TRI */
		int         orient, i;

		if (vp->width < 2)
			vp->width = 2;
		if (vp->height < 2)
			vp->height = 2;
		if (size < -MINSIZE)
			vp->ys = NRAND(MIN(-size, MAX(MINSIZE, MIN(vp->width, vp->height) /
				      MINGRIDSIZE)) - MINSIZE + 1) + MINSIZE;
		else if (size < MINSIZE) {
			if (!size)
				vp->ys = MAX(MINSIZE, MIN(vp->width, vp->height) / MINGRIDSIZE);
			else
				vp->ys = MINSIZE;
		} else
			vp->ys = MIN(size, MAX(MINSIZE, MIN(vp->width, vp->height) /
					       MINGRIDSIZE));
		vp->xs = (int) (1.52 * vp->ys);
		vp->pixelmode = True;
		vp->ncols = (MAX(vp->width / vp->xs - 1, 2) / 2) * 2;
		vp->nrows = (MAX(vp->height / vp->ys - 1, 2) / 2) * 2;
		vp->xb = (vp->width - vp->xs * vp->ncols) / 2 + vp->xs / 2;
		vp->yb = (vp->height - vp->ys * vp->nrows) / 2 + vp->ys / 2;
		for (orient = 0; orient < 2; orient++) {
			for (i = 0; i < 3; i++) {
				vp->triangleList[orient][i].x =
					(vp->xs - 2) * triangleUnit[orient][i].x;
				vp->triangleList[orient][i].y =
					(vp->ys - 2) * triangleUnit[orient][i].y;
			}
		}
	}

	vp->npositions = vp->ncols * vp->nrows;
	if (vp->arr != NULL)
		(void) free((void *) vp->arr);
	vp->arr = (char *) calloc(vp->npositions, sizeof (char));

	/* Play G-d with these numbers */
	vp->nparties = MI_BATCHCOUNT(mi);
	if (vp->nparties < MINPARTIES || vp->nparties > BITMAPS)
		vp->nparties = NRAND(BITMAPS - MINPARTIES + 1) + MINPARTIES;
	if (vp->pixelmode)
		vp->nparties = 2;

	XClearWindow(MI_DISPLAY(mi), MI_WINDOW(mi));

	for (i = 0; i < BITMAPS; i++)
		vp->number_in_party[i] = 0;

	for (row = 0; row < vp->nrows; row++)
		for (col = 0; col < vp->ncols; col++) {
			colrow = col + row * vp->ncols;
			i = NRAND(vp->nparties) + (vp->nparties == 2);
			vp->arr[colrow] = (char) i;
			drawcell(mi, col, row, (unsigned long) (MI_NPIXELS(mi) * i / BITMAPS),
				 i, False);
			vp->number_in_party[i]++;
		}
}

void
draw_voters(ModeInfo * mi)
{
	voterstruct *vp = &voters[MI_SCREEN(mi)];
	int         i, spineless_dude, neighbor_direction;
	int         spineless_col, spineless_row;
	int         new_opinion, old_opinion;
	cellstruct  info;

	for (i = 0; i < BITMAPS; i++)
		if (vp->number_in_party[i] == vp->npositions) {	/* The End of the WORLD */
			init_voters(mi);	/* Create a more interesting planet */
		}
	spineless_dude = NRAND(vp->npositions);
	neighbor_direction = NRAND(vp->neighbors) * 360 / vp->neighbors;
	spineless_col = spineless_dude % vp->ncols;
	spineless_row = spineless_dude / vp->ncols;
	old_opinion = vp->arr[spineless_dude];
	new_opinion = neighbors_opinion(vp, spineless_col, spineless_row,
					neighbor_direction);
	if (old_opinion != new_opinion) {
		vp->number_in_party[old_opinion]--;
		vp->number_in_party[new_opinion]++;
		vp->arr[spineless_dude] = new_opinion;
		info.kind = new_opinion;
		info.age = (old_opinion - new_opinion);
		if (info.age == 2)
			info.age = -1;
		if (info.age == -2)
			info.age = 1;
		info.age *= (FACTOR * MI_NPIXELS(mi)) / 3;
		info.col = spineless_col;
		info.row = spineless_row;
		if (MI_NPIXELS(mi) > 2) {
			advanceColors(mi, spineless_col, spineless_row);
			addto_list(vp, info);
		}
		drawcell(mi, spineless_col, spineless_row,
			 (MI_NPIXELS(mi) + info.age / FACTOR +
		  (MI_NPIXELS(mi) * new_opinion / BITMAPS)) % MI_NPIXELS(mi),
			 new_opinion, True);
	} else if (MI_NPIXELS(mi) > 2)
		advanceColors(mi, -1, -1);
	vp->generation++;
	for (i = 0; i < BITMAPS; i++)
		if (vp->number_in_party[i] == vp->npositions) {	/* The End of the WORLD */
      refresh_voters(mi);
			MI_PAUSE(mi) = 1000000;
		}
}

void
release_voters(ModeInfo * mi)
{
	if (voters != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			voterstruct *vp = &voters[screen];

			flush_list(vp);
			(void) free((void *) vp->last);
			(void) free((void *) vp->first);
			if (vp->arr != NULL)
				(void) free((void *) vp->arr);
		}
		(void) free((void *) voters);
		voters = NULL;
	}
}

void
refresh_voters(ModeInfo * mi)
{
	voterstruct *vp = &voters[MI_SCREEN(mi)];
	int         col, row, colrow;

	for (row = 0; row < vp->nrows; row++)
		for (col = 0; col < vp->ncols; col++) {
			colrow = col + row * vp->ncols;
			/* Draw all old, will get corrected soon if wrong... */
			drawcell(mi, col, row,
				 (unsigned long) (MI_NPIXELS(mi) * vp->arr[colrow] / BITMAPS),
				 vp->arr[colrow], False);
		}
}
