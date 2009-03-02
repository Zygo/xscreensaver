/* -*- Mode: C; tab-width: 4 -*- */
/* tetris --- autoplaying tetris game */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)tetris.c	4.13 98/10/01 xlockmore";

#endif

/*-
 * Copyright (c) 1998 by Jouk Jansen <joukj@hrem.stm.tudelft.nl>
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
 * The author should like to be notified if changes have been made to the
 * routine.  Response will only be guaranteed when a VMS version of the
 * program is available.
 *
 * An autoplaying tetris mode for xlockmore
 * David Bagley changed much of it to look more like altetris code...
 * which a significant portion belongs to Q. Alex Zhao.
 *
 * Copyright (c) 1992 - 95    Q. Alex Zhao, azhao@cc.gatech.edu
 *
 *     All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of the author not be
 * used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 *
 * This program is distributed in the hope that it will be "playable",
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Todo list consists of (in this order):
 *  -Removal of "full" lines
 *  -Adding rotation/shifting
 *  -Add option for the level of thinking the computer should perform
 *     (i.e. dummy = do nothing ..... genius = do always the best move
 *     possible)
 *
 * Revision History:
 * 01-Oct-98: Created
 */

#ifdef STANDALONE
#define PROGCLASS "Tetris"
#define HACK_INIT init_tetris
#define HACK_DRAW draw_tetris
#define tetris_opts xlockmore_opts
#define DEFAULTS "*delay: 600000 \n" \
 "*count: -500 \n" \
 "*cycles: 200 \n" \
 "*size: 0 \n" \
 "*ncolors: 200 \n" \
 "*fullrandom: True \n" \
 "*verbose: False \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#include "color.h"
#endif /* STANDALONE */

#ifdef MODE_tetris

#define DEF_CYCLE "True"

static Bool cycle_p;

#define INTRAND(min,max) (NRAND((max+1)-(min))+(min))

static XrmOptionDescRec opts[] =
{
	{"-cycle", ".tetris.cycle", XrmoptionNoArg, (caddr_t) "on"},
	{"+cycle", ".tetris.cycle", XrmoptionNoArg, (caddr_t) "off"}
};

static argtype vars[] =
{
	{(caddr_t *) & cycle_p, "cycle", "Cycle", DEF_CYCLE, t_Bool}
};
static OptionStruct desc[] =
{
	{"-/+cycle", "turn on/off colour cycling"}
};

ModeSpecOpt tetris_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   tetris_description =
{"tetris", "init_tetris", "draw_tetris", "release_tetris",
 "refresh_tetris", "init_tetris", NULL, &tetris_opts,
 600000, -40, 200, 0, 64, 1.0, "",
 "Shows an autoplaying tetris game", 0, NULL};

#endif

#include	"bitmaps/rot00.xbm"
#include	"bitmaps/rot01.xbm"
#include	"bitmaps/rot02.xbm"
#include	"bitmaps/rot03.xbm"
#include	"bitmaps/rot04.xbm"
#include	"bitmaps/rot05.xbm"
#include	"bitmaps/rot06.xbm"
#include	"bitmaps/rot07.xbm"
#include	"bitmaps/rot08.xbm"
#include	"bitmaps/rot09.xbm"
#include	"bitmaps/rot10.xbm"
#include	"bitmaps/rot11.xbm"
#include	"bitmaps/rot12.xbm"
#include	"bitmaps/rot13.xbm"
#include	"bitmaps/rot14.xbm"
#include	"bitmaps/rot15.xbm"

#define MINGRIDSIZE 8
#define MINSIZE 2
#define NONE 0
#define MAX_SIDES 4
#define MAX_KINDS 3
#define MAX_SQUARES 4
#define MAX_START_POLYOMINOES 7
#define MAX_POLYOMINOES 19
#define NUM_FLASHES     16
#define BITMAPS     16

#define THRESHOLD(x)    ((x+1)*10)
#define CHECKUP(x)      ((x)%8)
#define CHECKDOWN(x)    (((x)/4)*4+(x)%2)

static XImage logo[BITMAPS] =
{
	{0, 0, 0, XYBitmap, (char *) rot00_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{0, 0, 0, XYBitmap, (char *) rot01_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{0, 0, 0, XYBitmap, (char *) rot02_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{0, 0, 0, XYBitmap, (char *) rot03_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{0, 0, 0, XYBitmap, (char *) rot04_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{0, 0, 0, XYBitmap, (char *) rot05_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{0, 0, 0, XYBitmap, (char *) rot06_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{0, 0, 0, XYBitmap, (char *) rot07_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{0, 0, 0, XYBitmap, (char *) rot08_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{0, 0, 0, XYBitmap, (char *) rot09_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{0, 0, 0, XYBitmap, (char *) rot10_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{0, 0, 0, XYBitmap, (char *) rot11_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{0, 0, 0, XYBitmap, (char *) rot12_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{0, 0, 0, XYBitmap, (char *) rot13_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{0, 0, 0, XYBitmap, (char *) rot14_bits, LSBFirst, 8, LSBFirst, 8, 1},
	{0, 0, 0, XYBitmap, (char *) rot15_bits, LSBFirst, 8, LSBFirst, 8, 1}
};

typedef enum {FALL, DROP, LEFT, RIGHT, ROTATE, REFLECT} move_t;

typedef struct {
	int	 squares, polyomino_number;
	int	 xpos, ypos;
	int	 size, color_number;
	int	 random_rotation;
	int	 bonus, random_reflection;	/* not really used yet */
	long	random_number;
} thing_t;
typedef struct {
	int	 rotation;
	int	 reflection;	/* not really used yet */
	int	 start_height;
	int	 shape[MAX_SQUARES][MAX_SQUARES];
	int	 size;
} Polyominoes;
typedef struct {
	int	 number;
	int	 start[MAX_START_POLYOMINOES];
} Mode;
typedef struct {
	Polyominoes polyomino[MAX_POLYOMINOES];
	Mode	mode;
} Polytris;
typedef struct {
  int pmid, cid;
} fieldstruct;

typedef struct {
	Bool	 painted;
	int	 object, orient;
	GC	 gc;
	Colormap cmap;
	Bool	 cycle_p, mono_p, no_colors;
	unsigned long blackpixel, whitepixel, fg, bg;
	int	 direction;
	unsigned long colour;
	int	 p_type;

	fieldstruct *field;
	int	 xb, yb;
	int	 xs, ys;
	int	 width, height;
	int	 pixelmode;
	XColor     *colors;
	int	 ncolors;
	int	 ncols, nrows;
	Polytris    tris;
	thing_t     curPolyomino;
} trisstruct;

static trisstruct *triss = NULL;
static int icon_width, icon_height;

void init_tetris(ModeInfo * mi);

static int
ominos[] =
{
	/* Side of smallest square that contains shape */
	/* Number of orientations */
	/* Position, Next Postion, (not used yet),
	   number_down, used_for_start?
	   Polyomino (side^2 values) */
	2,
	1,
	0, 0, 0,   0, 2,   6, 3,  12, 9,

	3,
	16,
	0, 1, 4,     1, 2,   0, 0, 0,  6, 5, 1,   8, 0, 0,
	1, 2, 7,     0, 0,   0, 2, 0,  0, 10, 0,  0, 12, 1,
	2, 3, 6,     1, 0,   0, 0, 0,  0, 0, 2,   4, 5, 9,
	3, 0, 5,     0, 0,   0, 4, 3,  0, 0, 10,  0, 0, 8,
	4, 5, 0,     1, 1,   0, 0, 0,  4, 5, 3,   0, 0, 8,
	5, 6, 3,     0, 0,   0, 6, 1,  0, 10, 0,  0, 8, 0,
	6, 7, 2,     1, 0,   0, 0, 0,  2, 0, 0,   12, 5, 1,
	7, 4, 1,     0, 0,   0, 0, 2,  0, 0, 10,  0, 4, 9,
	8, 9, 8,     1, 2,   0, 0, 0,  4, 7, 1,   0, 8, 0,
	9, 10, 11,   0, 0,   0, 2, 0,  0, 14, 1,  0, 8, 0,
	10, 11, 10,  1, 0,   0, 0, 0,  0, 2, 0,   4, 13, 1,
	11, 8, 9,    0, 0,   0, 0, 2,  0, 4, 11,  0, 0, 8,
	12, 13, 14,  1, 2,   0, 0, 0,  4, 3, 0,   0, 12, 1,
	13, 12, 15,  0, 0,   0, 0, 2,  0, 6, 9,   0, 8, 0,
	14, 15, 12,  1, 1,   0, 0, 0,  0, 6, 1,   4, 9, 0,
	15, 14, 13,  0, 0,   0, 2, 0,  0, 12, 3,  0, 0, 8,

	4,
	2,
	0, 1, 0,  2, 2,   0, 0, 0, 0,  0, 0, 0, 0,   4, 5, 5, 1,   0, 0, 0, 0,
	1, 0, 1,  0, 0,   0, 0, 2, 0,  0, 0, 10, 0,  0, 0, 10, 0,  0, 0, 8, 0
};

static void
readPolyominoes(trisstruct * tp)
{
	int	 size = 0, n = 0, index = 0, sum = 0;
	int	 counter = 0, start_counter = 0;
	int	 polyomino, kinds, i, j;

	tp->tris.mode.number = MAX_START_POLYOMINOES;
	for (kinds = 0; kinds < MAX_KINDS; kinds++) {
		size = ominos[index++];
		n = ominos[index++];
		for (polyomino = 0; polyomino < n; polyomino++) {
			sum = polyomino + counter;
			if (polyomino != ominos[index])
				(void) printf("polyomino %d, ominos[index] %d\n", polyomino, ominos[index]);
			index++;	/* This is only there to "read" input file  */
			tp->tris.polyomino[sum].rotation = ominos[index++] + counter;
			tp->tris.polyomino[sum].reflection = ominos[index++] + counter; /* not used */

			tp->tris.polyomino[sum].start_height = ominos[index++];
			if (ominos[index++] != NONE) {
				i = start_counter;
				tp->tris.mode.start[i] = sum;
				start_counter++;
			}
			tp->tris.polyomino[sum].size = size;
			for (j = 0; j < size; j++)
				for (i = 0; i < size; i++) {
					tp->tris.polyomino[sum].shape[j][i] = ominos[index++];
				}
		}
		counter += n;
	}
}

static void
drawSquare(ModeInfo * mi, int pmid, int cid, int col, int row)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	trisstruct *tp = &triss[MI_SCREEN(mi)];
	long colour;

	if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
		if (tp->ncolors >= MAX_START_POLYOMINOES + 2)
			colour = cid + 2;
		else
			colour = 1;	/* Just in case */
		XSetBackground(display, tp->gc, tp->blackpixel);
		XSetForeground(display, tp->gc, tp->colors[colour].pixel);
	} else {
		if (MI_NPIXELS(mi) >  MAX_START_POLYOMINOES)
			colour = MI_PIXEL(mi, cid * MI_NPIXELS(mi) / MAX_START_POLYOMINOES);
		else
			colour = MI_WHITE_PIXEL(mi);
		XSetForeground(display, tp->gc, colour);
		XSetBackground(display, tp->gc, MI_BLACK_PIXEL(mi));
	}
  if (tp->pixelmode)
		XFillRectangle(display, window, tp->gc,
			tp->xb + col * tp->xs, tp->yb + row * tp->ys, tp->xs, tp->ys);
	else
/*-
 * PURIFY on SunOS4 and on Solaris 2 reports a 120 byte memory leak on
 * the next line */
    (void) XPutImage(display, window, tp->gc, &logo[pmid], 0, 0,
			tp->xb + icon_width * col, tp->yb + icon_height * row,
			icon_width, icon_height);
}

static void
clearSquare(ModeInfo * mi, int col, int row)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	trisstruct *tp = &triss[MI_SCREEN(mi)];

	XSetForeground(display, tp->gc, tp->blackpixel);
	XFillRectangle(display, window, tp->gc,
		tp->xb + col * tp->xs, tp->yb + row * tp->ys, tp->xs, tp->ys);
}

static void
drawPolyomino(ModeInfo * mi)
{
	trisstruct *tp = &triss[MI_SCREEN(mi)];
	int	 i, j;

	for (i = 0; i < tp->curPolyomino.size; i++) {
		for (j = 0; j < tp->curPolyomino.size; j++) {
			if (tp->tris.polyomino[tp->curPolyomino.polyomino_number].shape[j][i]) {
				drawSquare(mi, tp->tris.polyomino
						[tp->curPolyomino.polyomino_number].shape[j][i],
					tp->curPolyomino.color_number,
					tp->curPolyomino.xpos + i, tp->curPolyomino.ypos + j);
			}
		}
	}
}

static void
drawPolyominoDiff(ModeInfo *mi, thing_t * old)
{
	trisstruct *tp = &triss[MI_SCREEN(mi)];
	int	 i, j, ox, oy;

	for (i = 0; i < tp->curPolyomino.size; i++)
		for (j = 0; j < tp->curPolyomino.size; j++)
	if ((tp->curPolyomino.ypos + j >= 0) && tp->tris.polyomino
	      [tp->curPolyomino.polyomino_number].shape[j][i])
	  drawSquare(mi, tp->tris.polyomino
			 [tp->curPolyomino.polyomino_number].shape[j][i],
	    tp->curPolyomino.color_number, tp->curPolyomino.xpos + i, tp->curPolyomino.ypos + j);

	for (i = 0; i < tp->curPolyomino.size; i++)
		for (j = 0; j < tp->curPolyomino.size; j++) {
			ox = old->xpos + i - tp->curPolyomino.xpos;
			oy = old->ypos + j - tp->curPolyomino.ypos;
			if (tp->tris.polyomino[old->polyomino_number].shape[j][i] &&
				((ox < 0) || (ox >= tp->curPolyomino.size) ||
				(oy < 0) || (oy >= tp->curPolyomino.size) ||
				!tp->tris.polyomino
					[tp->curPolyomino.polyomino_number].shape[oy][ox]))
				clearSquare(mi, (old->xpos + i), (old->ypos + j));
		}
}

static void
redoNext(ModeInfo * mi)
{
	trisstruct *tp = &triss[MI_SCREEN(mi)];
	int next_start, i;

	next_start = tp->curPolyomino.random_number % tp->tris.mode.number;
	tp->curPolyomino.color_number = next_start;

	tp->curPolyomino.polyomino_number = tp->tris.mode.start[next_start];
	for (i = 0; i < tp->curPolyomino.random_rotation; i++)
		tp->curPolyomino.polyomino_number = tp->tris.polyomino
			[tp->curPolyomino.polyomino_number].rotation;
	tp->curPolyomino.size = tp->tris.polyomino[tp->curPolyomino.polyomino_number].size;
	tp->curPolyomino.xpos = NRAND(tp->ncols - tp->curPolyomino.size + 1);
	/* tp->curPolyomino.xpos = (tp->ncols - tp->curPolyomino.size) / 2; */
	tp->curPolyomino.ypos = -tp->tris.polyomino
		[tp->curPolyomino.polyomino_number].start_height;
}

static void
newPolyomino(ModeInfo * mi) {
	trisstruct *tp = &triss[MI_SCREEN(mi)];

	tp->curPolyomino.random_number = LRAND();
	tp->curPolyomino.random_rotation = NRAND(MAX_SIDES);
	/* tp->curPolyomino.random_reflection = NRAND(2); */  /* Future? */
	/* tp->curPolyomino.bonus = bonusNow; */ /* Future? */

	redoNext(mi);
}

static void
putBox(ModeInfo * mi)
{
	trisstruct *tp = &triss[MI_SCREEN(mi)];
	int	 i, j;
	int	 x = tp->curPolyomino.xpos, y = tp->curPolyomino.ypos, pos;

	for (i = 0; i < tp->curPolyomino.size; i++)
		for (j = 0; j < tp->curPolyomino.size; j++)
			if ((y + j >= 0) && tp->tris.polyomino
				[tp->curPolyomino.polyomino_number].shape[j][i]) {
				pos = (y + j) * tp->ncols + x + i;
				tp->field[pos].pmid = tp->tris.polyomino
					[tp->curPolyomino.polyomino_number].shape[j][i] % BITMAPS;
				tp->field[pos].cid = tp->curPolyomino.color_number;
			}
}

static Bool
overlapping(ModeInfo * mi)
{
	trisstruct *tp = &triss[MI_SCREEN(mi)];
	int	     i, j;
	int	     x = tp->curPolyomino.xpos, y = tp->curPolyomino.ypos;
        int gradualAppear = False;

	for (i = 0; i < tp->curPolyomino.size; i++)
		for (j = 0; j < tp->curPolyomino.size; j++)
			if (tp->tris.polyomino[tp->curPolyomino.polyomino_number].shape[j][i]) {
				if ((y + j >= tp->nrows) || (x + i < 0) || (x + i >= tp->ncols))
					return True;
if (gradualAppear) {
/* This method one can turn polyomino to an area above of screen, also
   part of the polyomino may not be visible initially */
            if ((y + j >= 0) && (tp->field[(y + j) * tp->ncols + x + i].pmid >= 0))
              return True;
          } else {
/* This method does not allow turning polyomino to an area above screen */
            if ((y + j < 0) || (tp->field[(y + j) * tp->ncols + x + i].pmid >= 0))
              return True;
          }

			}
	return False;
}

static Bool
atBottom(ModeInfo * mi)
{
	trisstruct *tp = &triss[MI_SCREEN(mi)];
	int	     i, j;
	int	     x = tp->curPolyomino.xpos, y = tp->curPolyomino.ypos;

	for (i = 0; i < tp->curPolyomino.size; i++)
		for (j = 0; j < tp->curPolyomino.size; j++)
			if ((y + j >= -1) && tp->tris.polyomino
				[tp->curPolyomino.polyomino_number].shape[j][i])
				if ((y + j >= tp->nrows - 1) || (x + i < 0) || (x + i >= tp->ncols) ||
					 (tp->field[(y + j + 1) * tp->ncols + x + i].pmid >= 0))
					return True;
	return False;
}

static void
tryMove(ModeInfo * mi, move_t move)
{
	trisstruct *tp = &triss[MI_SCREEN(mi)];
	thing_t	 old;
	int	     i;
	int	     cw = False; /* I am not sure if this is the original */

	old = tp->curPolyomino;

	switch (move) {
		case FALL:
			tp->curPolyomino.ypos++;
			break;

		case DROP:
			do { /* so fast you can not see it ;) */
				tp->curPolyomino.ypos ++;
			} while (!overlapping(mi));
			tp->curPolyomino.ypos--;
			break;

		case LEFT:
			tp->curPolyomino.xpos--;
			break;

		case RIGHT:
			tp->curPolyomino.xpos++;
			break;

		case ROTATE:
	  		if (cw)
				for (i = 0; i < MAX_SIDES - 1; i++)
	 				tp->curPolyomino.polyomino_number = tp->tris.polyomino
						[tp->curPolyomino.polyomino_number].rotation;
	  		else /* ccw */
				tp->curPolyomino.polyomino_number = tp->tris.polyomino
					[tp->curPolyomino.polyomino_number].rotation;
			tp->curPolyomino.xpos = old.xpos;
			tp->curPolyomino.ypos = old.ypos;
			break;

		case REFLECT: /* reflect on y axis */
			tp->curPolyomino.polyomino_number = tp->tris.polyomino
				[tp->curPolyomino.polyomino_number].reflection;
			tp->curPolyomino.xpos = old.xpos;
			tp->curPolyomino.ypos = old.ypos;
			break;
	}

	if (!overlapping(mi))
		drawPolyominoDiff(mi, &old);
	else
		tp->curPolyomino = old;
}

static int
checkLines(ModeInfo *mi)
{
	trisstruct *tp = &triss[MI_SCREEN(mi)];
	Display    *display = MI_DISPLAY(mi);
	int	    *lSet, nset = 0;
	int	     i, j, y;


	lSet = (int *) calloc(tp->nrows, sizeof (int));
	for (j = 0; j < tp->nrows; j++) {
		for (i = 0; i < tp->ncols; i++)
			if (tp->field[j * tp-> ncols + i].pmid >= 0)
				lSet[j]++;
		if (lSet[j] == tp->ncols)
			nset++;
	}

	if (nset) {
#ifdef UNDER_CONSTRUCTION
		for (i = 0; i < ((NUM_FLASHES / nset) % 2) * 2; i++) {
			for (j = 0; j < tp->nrows; j++) {
				if (lSet[j] == tp->ncols)
					XFillRectangle(display, blockWin, xorGC,
					  0, j * BOXSIZE, frameW, BOXSIZE);
			}
			XFlush(display);
		}
#endif

		for (j = tp->nrows - 1; j >= 0; j--) {
			if (lSet[j] == tp->ncols) {
				for (y = j; y > 0; y--)
			   	 for (i = 0; i < tp->ncols; i++)
					tp->field[y * tp->ncols + i] =
					   tp->field[(y - 1) * tp->ncols + i];
				for (i = 0; i < tp->ncols; i++)
					tp->field[i].pmid = -1;

#ifdef UNDER_CONSTRUCTION
				XCopyArea(display, blockWin, blockWin, tinyGC,
				  0, 0, frameW, j * BOXSIZE, 0, BOXSIZE);

				XFillRectangle(display, blockWin, revGC,
				  0, 0, frameW, BOXSIZE);
#endif

				for (i = j; i > 0; i--)
					lSet[i] = lSet[i-1];
				lSet[0] = 0;

				if (j > 0)
					for (i = 0; i < tp->ncols; i++) {
						int	     tmp = tp->field[j * tp->ncols + i].pmid;

						if ((tmp >= 0) && (tmp != CHECKDOWN(tmp))) {
							tp->field[j * tp->ncols + i].pmid = CHECKDOWN(tmp);
							 	drawSquare(mi,
					 		  tp->field[j * tp->ncols + i].pmid, tp->field[j * tp->ncols + i].cid,
							  i, j);
						}
			   		}

				j++;

				if (j < tp->nrows)
					for (i = 0; i < tp->ncols; i++) {
						int	     tmp = tp->field[j * tp->ncols + i].pmid;

						if ((tmp >= 0) && (tmp != CHECKUP(tmp))) {
							tp->field[j * tp->ncols + i].pmid = CHECKUP(tmp);
							drawSquare(mi,
							  tp->field[j * tp->ncols + i].pmid, tp->field[j * tp->ncols + i].cid,
							  i, j);
						}
					}

				XFlush(display);
			}
		}

		XSync(display, False);
	}

	if (lSet)
		(void) free((void *) lSet);


	return nset;
}

static Bool
moveOne(ModeInfo *mi, move_t move)
{
	Display    *display = MI_DISPLAY(mi);

    if ((move == DROP) || ((move == FALL) && atBottom(mi))) {
	putBox(mi);
#ifdef UNDER_CONSTRUCTION
	{
		int	     lines;

		lines = checkLines(mi);
		rows += lines;
		if (rows > THRESHOLD(level)) {
		    level++;
		    if (bonus) /* No good deed goes unpunished */
	     		bonusNow = True;
		}
	}
#else
	(void) checkLines(mi);
#endif
	newPolyomino(mi);
	XSync(display, True);	/* discard all events */
	if (overlapping(mi)) {
#ifdef UNDER_CONSTRUCTION
		gameOver();
#endif
		init_tetris(mi);
		return False;
	}

	drawPolyomino(mi);
	return True;
    } else {
	tryMove(mi, move);
#ifdef UNDER_CONSTRUCTION
	if (rows > THRESHOLD(level)) {
	    level++;
	    if (bonus)
	      bonusNow = True;
	}
#endif
	return False;
    }
}

void
refresh_tetris(ModeInfo * mi)
{
	trisstruct *tp = &triss[MI_SCREEN(mi)];
	int col, row, pos;

	if (!tp->painted)
		return;
	MI_CLEARWINDOW(mi);
	for (col = 0; col < tp->ncols; col++)
		for (row = 0; row < tp->nrows; row++) {
			pos = row * tp->ncols + col;
#if 0
			(void) printf("pmid %d, cid %d, %d %d\n",
				tp->field[pos].pmid, tp->field[pos].cid, col, row);
#endif
			if (tp->field[pos].pmid != -1)
				drawSquare(mi, tp->field[pos].pmid, tp->field[pos].cid, col, row);
		}
	drawPolyomino(mi);
}

void
draw_tetris(ModeInfo * mi) {
	Display *   display = MI_DISPLAY(mi);
	trisstruct *tp = &triss[MI_SCREEN(mi)];

	if (tp->no_colors) {
		init_tetris(mi);
		return;
	}
	tp->painted = True;
	MI_IS_DRAWN(mi) = True;

	/* Rotate colours */
	if (tp->cycle_p) {
		rotate_colors(display, tp->cmap, tp->colors, tp->ncolors,
			      tp->direction);
		if (!(LRAND() % 1000))
			tp->direction = -tp->direction;
	}

	(void) moveOne(mi, FALL);
}

void
release_tetris(ModeInfo * mi) {
	Display *   display = MI_DISPLAY(mi);

	if (triss != NULL) {
		int	 screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			trisstruct *tp = &triss[screen];

			if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
				MI_WHITE_PIXEL(mi) = tp->whitepixel;
				MI_BLACK_PIXEL(mi) = tp->blackpixel;
#ifndef STANDALONE
				MI_FG_PIXEL(mi) = tp->fg;
				MI_BG_PIXEL(mi) = tp->bg;
#endif
				if (tp->colors && tp->ncolors && !tp->no_colors)
					free_colors(display, tp->cmap, tp->colors, tp->ncolors);
				if (tp->colors)
					(void) free((void *) tp->colors);
				if (tp->cmap)
				  XFreeColormap(display, tp->cmap);
			}
			if (tp->gc != NULL)
				XFreeGC(display, tp->gc);
			if (tp->field)
				(void) free((void *) tp->field);

		}
		(void) free((void *) triss);
		triss = NULL;
	}
}

void
init_tetris(ModeInfo * mi) {
	Display *   display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	trisstruct *tp;
	int size = MI_SIZE(mi);
	int	 i;

	if (triss == NULL) {
		if ((triss = (trisstruct *) calloc(MI_NUM_SCREENS(mi),
					     sizeof (trisstruct))) == NULL)
			return;
	}
	tp = &triss[MI_SCREEN(mi)];

	if (!tp->gc) {
		icon_width = rot00_width;
		icon_height = rot00_height;
		for (i = 0; i < BITMAPS; i++) {
			logo[i].width = icon_width;
			logo[i].height = icon_height;
			logo[i].bytes_per_line = (icon_width + 7) / 8;
		}

		readPolyominoes(tp);
		tp->blackpixel = MI_BLACK_PIXEL(mi);
		if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
			XColor      color;

#ifndef STANDALONE
			extern char *background;
			extern char *foreground;

			tp->fg = MI_FG_PIXEL(mi);
			tp->bg = MI_BG_PIXEL(mi);
#endif
			tp->whitepixel = MI_WHITE_PIXEL(mi);
			tp->cmap = XCreateColormap(display, window,
						    MI_VISUAL(mi), AllocNone);
			XSetWindowColormap(display, window, tp->cmap);
			(void) XParseColor(display, tp->cmap, "black", &color);
			(void) XAllocColor(display, tp->cmap, &color);
			MI_BLACK_PIXEL(mi) = color.pixel;
			(void) XParseColor(display, tp->cmap, "white", &color);
			(void) XAllocColor(display, tp->cmap, &color);
			MI_WHITE_PIXEL(mi) = color.pixel;
#ifndef STANDALONE
			(void) XParseColor(display, tp->cmap, background, &color);
			(void) XAllocColor(display, tp->cmap, &color);
			MI_BG_PIXEL(mi) = color.pixel;
			(void) XParseColor(display, tp->cmap, foreground, &color);
			(void) XAllocColor(display, tp->cmap, &color);
			MI_FG_PIXEL(mi) = color.pixel;
#endif
			tp->colors = 0;
			tp->ncolors = 0;
		}
		if ((tp->gc = XCreateGC(display, MI_WINDOW(mi),
			     (unsigned long) 0, (XGCValues *) NULL)) == None)
			return;
	}
	MI_CLEARWINDOW(mi);
	tp->painted = False;

	tp->width = MI_WIDTH(mi);
	tp->height = MI_HEIGHT(mi);

	if (tp->width < 2)
		tp->width = 2;
	if (tp->height < 2)
		tp->height = 2;
	if (size == 0 ||
		MINGRIDSIZE * size > tp->width || MINGRIDSIZE * size > tp->height) {
			if (tp->width > MINGRIDSIZE * icon_width &&
				tp->height > MINGRIDSIZE * icon_height) {
				tp->pixelmode = False;
				tp->xs = icon_width;
				tp->ys = icon_height;
      } else {
				tp->pixelmode = True;
				tp->xs = tp->ys = MAX(MINSIZE, MIN(tp->width, tp->height) /
					MINGRIDSIZE);
			}
	} else {
		tp->pixelmode = True;
		if (size < -MINSIZE)
			tp->ys = NRAND(MIN(-size, MAX(MINSIZE, MIN(tp->width, tp->height) /
				MINGRIDSIZE)) - MINSIZE + 1) + MINSIZE;
		else if (size < MINSIZE)
			tp->ys = MINSIZE;
		else
			tp->ys = MIN(size, MAX(MINSIZE, MIN(tp->width, tp->height) /
				MINGRIDSIZE));
		tp->xs = tp->ys;
	}
	tp->ncols = MAX(tp->width / tp->xs, 2);
	tp->nrows = MAX(tp->height / tp->ys, 2);
	tp->xb = (tp->width - tp->xs * tp->ncols) / 2;
	tp->yb = (tp->height - tp->ys * tp->nrows) / 2;

	/* Set up tetris data */

	if (tp->field)
		(void) free((void *) tp->field);
	tp->field = (fieldstruct *) malloc(tp->ncols * tp->nrows * sizeof (fieldstruct));
	for (i = 0; i < tp->ncols * tp->nrows; i++) {
		tp->field[i].pmid = -1;
		tp->field[i].cid = 0;
	}

	/* Set up colour map */
	tp->direction = (LRAND() & 1) ? 1 : -1;
	if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
		if (tp->colors && tp->ncolors && !tp->no_colors)
			free_colors(display, tp->cmap, tp->colors, tp->ncolors);
		if (tp->colors)
			(void) free((void *) tp->colors);
		tp->colors = 0;
		tp->ncolors = MI_NCOLORS(mi);
		if (tp->ncolors < 2)
			tp->ncolors = 2;
		if (tp->ncolors <= 2)
			tp->mono_p = True;
		else
			tp->mono_p = False;

		if (tp->mono_p)
			tp->colors = 0;
		else
			tp->colors = (XColor *) malloc(sizeof (*tp->colors) * (tp->ncolors + 1));
		tp->cycle_p = has_writable_cells(mi);
		if (tp->cycle_p) {
			if (MI_IS_FULLRANDOM(mi)) {
				if (!NRAND(8))
					tp->cycle_p = False;
				else
					tp->cycle_p = True;
			} else {
				tp->cycle_p = cycle_p;
			}
		}
		if (!tp->mono_p) {
			if (!(LRAND() % 10))
				make_random_colormap(
#if STANDALONE
            MI_DISPLAY(mi), MI_WINDOW(mi),
#else
            mi,
#endif
              tp->cmap, tp->colors, &tp->ncolors,
						  True, True, &tp->cycle_p);
			else if (!(LRAND() % 2))
				make_uniform_colormap(
#if STANDALONE
            MI_DISPLAY(mi), MI_WINDOW(mi),
#else
            mi,
#endif
                  tp->cmap, tp->colors, &tp->ncolors,
						      True, &tp->cycle_p);
			else
				make_smooth_colormap(
#if STANDALONE
            MI_DISPLAY(mi), MI_WINDOW(mi),
#else
            mi,
#endif
                 tp->cmap, tp->colors, &tp->ncolors,
						     True, &tp->cycle_p);
		}
		XInstallColormap(display, tp->cmap);
		if (tp->ncolors < 2) {
			tp->ncolors = 2;
			tp->no_colors = True;
		} else
			tp->no_colors = False;
		if (tp->ncolors <= 2)
			tp->mono_p = True;

		if (tp->mono_p)
			tp->cycle_p = False;

	}
	/* initialize object */
	newPolyomino(mi);
}

#endif /* MODE_tetris */
