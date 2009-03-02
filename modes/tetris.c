/* -*- Mode: C; tab-width: 4 -*- */
/* tetris --- autoplaying tetris game */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)tetris.c  4.13 98/10/01 xlockmore";

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
 * Todo list:
 *  -Add option for the level of thinking the computer should perform
 *     (i.e. dummy = do nothing ..... genius = do always the best move
 *     possible)
 *  -Get welltris option to work better.
 *     draw more sophisticated squares (i.e. something like the bitmaps
 *       on tetris
 *
 * Revision History:
 * 02-Aug-2000: alwelltris trackmouse implemented
 * 27-Jul-2000: alwelltris all but trackmouse
 * 22-Jun-2000: trackmouse added, row clearing fixed, bonus pieces,
 * 01-Nov-1998: altetris additions
 * 01-Oct-1998: Created
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
 "*trackmouse: False \n" \
 "*verbose: False \n"
#include "xlockmore.h"    /* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"    /* in xlockmore distribution */
#include "color.h"
#endif /* STANDALONE */

#ifdef MODE_tetris

#define DEF_BONUS "False"
#define DEF_CYCLE "True"
#define DEF_TRACKMOUSE  "False"
#define DEF_WELL "False"

static Bool bonus;
static Bool cycle_p;
static Bool trackmouse;
static Bool well;

#define INTRAND(min,max) (NRAND((max+1)-(min))+(min))

static XrmOptionDescRec opts[] =
{
  {(char *) "-bonus", (char *) ".tetris.bonus", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+bonus", (char *) ".tetris.bonus", XrmoptionNoArg, (caddr_t) "off"},
  {(char *) "-cycle", (char *) ".tetris.cycle", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+cycle", (char *) ".tetris.cycle", XrmoptionNoArg, (caddr_t) "off"},
#ifndef DISABLE_INTERACTIVE
  {(char *) "-trackmouse", (char *) ".tetris.trackmouse", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+trackmouse", (char *) ".tetris.trackmouse", XrmoptionNoArg, (caddr_t) "off"},
#endif
  {(char *) "-well", (char *) ".tetris.well", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+well", (char *) ".tetris.well", XrmoptionNoArg, (caddr_t) "off"}
};

static argtype vars[] =
{
  {(caddr_t *) & bonus, (char *) "bonus", (char *) "Bonus", (char *) DEF_BONUS, t_Bool},
  {(caddr_t *) & cycle_p, (char *) "cycle", (char *) "Cycle", (char *) DEF_CYCLE, t_Bool},
#ifndef DISABLE_INTERACTIVE
  {(caddr_t *) & trackmouse, (char *) "trackmouse", (char *) "TrackMouse", (char *) DEF_TRACKMOUSE, t_Bool},
#endif
  {(caddr_t *) & well, (char *) "well", (char *) "Well", (char *) DEF_WELL, t_Bool}
};
static OptionStruct desc[] =
{
  {(char *) "-/+bonus", (char *) "turn on/off larger bonus pieces"},
  {(char *) "-/+cycle", (char *) "turn on/off colour cycling"},
#ifndef DISABLE_INTERACTIVE
  {(char *) "-/+trackmouse", (char *) "turn on/off the tracking of the mouse"},
#endif
  {(char *) "-/+well", (char *) "turn on/off the welltris mode"}
};

ModeSpecOpt tetris_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   tetris_description =
{"tetris", "init_tetris", "draw_tetris", "release_tetris",
 "refresh_tetris", "change_tetris", NULL, &tetris_opts,
 600000, -40, 200, 0, 64, 1.0, "",
 "Shows an autoplaying tetris game", 0, NULL};

#endif

#if !defined(BM_3D) && !defined(BM_PLAIN)
#define BM_3D
#endif
#ifdef BM_3D
#include  "bitmaps/rot00.xbm"
#include  "bitmaps/rot01.xbm"
#include  "bitmaps/rot02.xbm"
#include  "bitmaps/rot03.xbm"
#include  "bitmaps/rot04.xbm"
#include  "bitmaps/rot05.xbm"
#include  "bitmaps/rot06.xbm"
#include  "bitmaps/rot07.xbm"
#include  "bitmaps/rot08.xbm"
#include  "bitmaps/rot09.xbm"
#include  "bitmaps/rot10.xbm"
#include  "bitmaps/rot11.xbm"
#include  "bitmaps/rot12.xbm"
#include  "bitmaps/rot13.xbm"
#include  "bitmaps/rot14.xbm"
#include  "bitmaps/rot15.xbm"
#else
#ifdef BM_PLAIN
#include  "bitmaps/rop00.xbm"
#include  "bitmaps/rop01.xbm"
#include  "bitmaps/rop02.xbm"
#include  "bitmaps/rop03.xbm"
#include  "bitmaps/rop04.xbm"
#include  "bitmaps/rop05.xbm"
#include  "bitmaps/rop06.xbm"
#include  "bitmaps/rop07.xbm"
#include  "bitmaps/rop08.xbm"
#include  "bitmaps/rop09.xbm"
#include  "bitmaps/rop10.xbm"
#include  "bitmaps/rop11.xbm"
#include  "bitmaps/rop12.xbm"
#include  "bitmaps/rop13.xbm"
#include  "bitmaps/rop14.xbm"
#include  "bitmaps/rop15.xbm"
#endif
#endif
#define MINGRIDSIZE 10 
#define MINSIZE 2
#define MAX_SIDES 4

#define NUM_FLASHES 16
#define BITMAPS 16

#define DELTA 1
#define WELL_WIDTH 8
#define WELL_DEPTH 12
#define WELL_PERIMETER (MAX_SIDES*WELL_WIDTH)
#define BASE_WIDTH (WELL_WIDTH*tp->xs+DELTA)
#define ENDPT (3*BASE_WIDTH)


#define THRESHOLD(x) ((x+1)*10)
#define CHECKUP(x) ((x)%8)
#define CHECKDOWN(x) (((x)/4)*4+(x)%2)
#define EMPTY (-1)

static XImage logo[BITMAPS] =
{
#ifdef BM_3D
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
#else
#ifdef BM_PLAIN
  {0, 0, 0, XYBitmap, (char *) rop00_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) rop01_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) rop02_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) rop03_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) rop04_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) rop05_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) rop06_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) rop07_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) rop08_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) rop09_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) rop10_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) rop11_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) rop12_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) rop13_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) rop14_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) rop15_bits, LSBFirst, 8, LSBFirst, 8, 1}
#endif
#endif
};

typedef enum {FALL, DROP, LEFT, RIGHT, ROTATE, REFLECT} move_t;

typedef struct {
  int  squares, polyomino_number;
  int  xpos, ypos;
  int  size, color_number;
  int  random_rotation;
  int  random_reflection;  /* not used */
  long  random_number;
} thing_t;

typedef struct {
  int  rotation;
  int  reflection;  /* not used */
  int  leadingEmptyWidth, leadingEmptyHeight;
  int  *shape;
  int  size;
} Polyominoes;

typedef struct {
  int  number;
  int  *start;
} Mode;

typedef struct {
  Polyominoes *polyomino;
  Mode  mode;
} Polytris;

static Polytris polytris[2];

typedef struct {
  int pmid, cid;
} fieldstruct;

typedef struct {
  Bool   painted;
  int   object, orient;
  GC   gc;
  Colormap cmap;
  Bool   cycle_p, mono_p, no_colors;
  unsigned long blackpixel, whitepixel, fg, bg;
  int   direction;
  unsigned long colour;
  int   p_type;
  int   sidemoves;

  int   xb, yb;
  int   xs, ys;
  int   width, height;
  int   pixelmode;
  XColor     *colors;
  int   ncolors;
  int   ncols, nrows;
  int   rows, level;
  Bool    bonus, bonusNow;
  thing_t     curPolyomino;

  /* tetris */
  fieldstruct *field;

  /* welltris */
  Bool well;
  int frozen_wall[MAX_SIDES];
  fieldstruct wall[WELL_DEPTH+WELL_WIDTH][WELL_PERIMETER];
  fieldstruct base[WELL_WIDTH][WELL_WIDTH];

} trisstruct;

static trisstruct *triss = NULL;
static int icon_width, icon_height;

/* Setup: Normal, Bonus */
static int area_kinds[] = {3, 3};
static int omino_squares[] = {4, 5};
static int start_ominoes[] = {7, 18};
static int max_ominoes[] = {19, 63};

static int
ominos[] =
{
  /* Side of smallest square that contains shape */
  /* Number of orientations */
  /* Position, Next Postion, (not used yet),
     number_down, used_for_start?
     Polyomino (side^2 values) */

  /* Normal pieces  4 squares */
  2,
  1,
  0, 0, 0,   2,   6, 3,  12, 9,

  3,
  16,
  0, 1, 4,     2,   0, 0, 0,  6, 5, 1,   8, 0, 0,
  1, 2, 7,     0,   0, 2, 0,  0, 10, 0,  0, 12, 1,
  2, 3, 6,     0,   0, 0, 0,  0, 0, 2,   4, 5, 9,
  3, 0, 5,     0,   0, 4, 3,  0, 0, 10,  0, 0, 8,
  4, 5, 0,     1,   0, 0, 0,  4, 5, 3,   0, 0, 8,
  5, 6, 3,     0,   0, 6, 1,  0, 10, 0,  0, 8, 0,
  6, 7, 2,     0,   0, 0, 0,  2, 0, 0,   12, 5, 1,
  7, 4, 1,     0,   0, 0, 2,  0, 0, 10,  0, 4, 9,
  8, 9, 8,     2,   0, 0, 0,  4, 7, 1,   0, 8, 0,
  9, 10, 11,   0,   0, 2, 0,  0, 14, 1,  0, 8, 0,
  10, 11, 10,  0,   0, 0, 0,  0, 2, 0,   4, 13, 1,
  11, 8, 9,    0,   0, 0, 2,  0, 4, 11,  0, 0, 8,
  12, 13, 14,  2,   0, 0, 0,  4, 3, 0,   0, 12, 1,
  13, 12, 15,  0,   0, 0, 2,  0, 6, 9,   0, 8, 0,
  14, 15, 12,  1,   0, 0, 0,  0, 6, 1,   4, 9, 0,
  15, 14, 13,  0,   0, 2, 0,  0, 12, 3,  0, 0, 8,

  4,
  2,
  0, 1, 0,  2,   0, 0, 0, 0,  0, 0, 0, 0,   4, 5, 5, 1,   0, 0, 0, 0,
  1, 0, 1,  0,   0, 0, 2, 0,  0, 0, 10, 0,  0, 0, 10, 0,  0, 0, 8, 0,

  /* Bonus pieces  5 squares */
  3,
  37,
  0, 1, 0,     2,   0, 0, 0,  6, 5, 3,    8, 0, 8,
  1, 2, 3,     0,   0, 6, 1,  0, 10, 0,   0, 12, 1,
  2, 3, 2,     0,   0, 0, 0,  2, 0, 2,    12, 5, 9,
  3, 0, 1,     0,   0, 4, 3,  0, 0, 10,   0, 4, 9,
  4, 5, 8,     2,   0, 0, 0,  6, 7, 1,    12, 9, 0,
  5, 6, 11,    0,   0, 2, 0,  0, 14, 3,   0, 12, 9,
  6, 7, 10,    0,   0, 0, 0,  0, 6, 3,    4, 13, 9,
  7, 4, 9,     0,   0, 6, 3,  0, 12, 11,  0, 0, 8,
  8, 9, 4,     1,   0, 0, 0,  4, 7, 3,    0, 12, 9,
  9, 10, 7,    0,   0, 6, 3,  0, 14, 9,   0, 8, 0,
  10, 11, 6,   0,   0, 0, 0,  6, 3, 0,    12, 13, 1,
  11, 8, 5,    0,   0, 0, 2,  0, 6, 11,   0, 12, 9,
  12, 13, 12,  2,   4, 7, 1,  0, 10, 0,   0, 8, 0,
  13, 14, 15,  0,   2, 0, 0,  14, 5, 1,   8, 0, 0,
  14, 15, 14,  0,   0, 2, 0,  0, 10, 0,   4, 13, 1,
  15, 12, 13,  0,   0, 0, 2,  4,  5, 11,  0, 0, 8,
  16, 17, 19,  2,   6, 5, 1,  10, 0, 0,   8, 0, 0,
  17, 18, 18,  0,   2, 0, 0,  10, 0, 0,   12, 5, 1,
  18, 19, 17,  0,   0, 0, 2,  0, 0, 10,   4, 5, 9,
  19, 16, 16,  0,   4, 5, 3,  0, 0, 10,   0, 0, 8,
  20, 21, 22,  2,   2, 0, 0,  12, 5, 3,   0, 0, 8,
  21, 20, 23,  0,   0, 6, 1,  0, 10, 0,   4, 9, 0,
  22, 23, 20,  1,   0, 0, 2,  6, 5, 9,    8, 0, 0,
  23, 22, 21,  0,   4, 3, 0,  0, 10, 0,   0, 12, 1,
  24, 25, 25,  2,   4, 3, 0,  0, 12, 3,   0, 0, 8,
  25, 26, 24,  0,   0, 6, 1,  6, 9, 0,    8, 0, 0,
  26, 27, 27,  0,   2, 0, 0,  12, 3, 0,   0, 12, 1,
  27, 24, 26,  0,   0, 0, 2,  0, 6, 9,    4, 9, 0,
  28, 29, 32,  2,   0, 2, 0,  6, 13, 1,   8, 0, 0,
  29, 30, 35,  0,   0, 2, 0,  4, 11, 0,   0, 12, 1,
  30, 31, 34,  0,   0, 0, 2,  4, 7, 9,    0, 8, 0,
  31, 28, 33,  0,   4, 3, 0,  0, 14, 1,   0, 8, 0,
  32, 33, 28,  1,   0, 2, 0,  4, 13, 3,   0, 0, 8,
  33, 34, 31,  0,   0, 6, 1,  4, 11, 0,   0, 8, 0,
  34, 35, 30,  0,   2, 0, 0,  12, 7, 1,   0, 8, 0,
  35, 32, 29,  0,   0, 2, 0,  0, 14, 1,   4, 9, 0,
  36, 36, 36,  2,   0, 2, 0,  4, 15, 1,   0, 8, 0,

  4,
  24,
  0, 1, 4,     2,   0, 0, 0, 0,  6, 5, 5, 1,   8, 0, 0, 0,   0, 0, 0, 0,
  1, 2, 7,     0,   0, 2, 0, 0,  0, 10, 0, 0,  0, 10, 0, 0,  0, 12, 1, 0,
  2, 3, 6,     0,   0, 0, 0, 0,  0, 0, 0, 2,   4, 5, 5, 9,   0, 0, 0, 0,
  3, 0, 5,     0,   0, 4, 3, 0,  0, 0, 10, 0,  0, 0, 10, 0,  0, 0, 8, 0,
  4, 5, 0,     1,   0, 0, 0, 0,  4, 5, 5, 3,   0, 0, 0, 8,   0, 0, 0, 0,
  5, 6, 3,     0,   0, 6, 1, 0,  0, 10, 0, 0,  0, 10, 0, 0,  0, 8, 0, 0,
  6, 7, 2,     0,   0, 0, 0, 0,  2, 0, 0, 0,   12, 5, 5, 1,  0, 0, 0, 0,
  7, 4, 1,     0,   0, 0, 2, 0,  0, 0, 10, 0,  0, 0, 10, 0,  0, 4, 9, 0,
  8, 9, 12,    2,   0, 0, 0, 0,  4, 7, 5, 1,   0, 8, 0, 0,   0, 0, 0, 0,
  9, 10, 15,   0,   0, 2, 0, 0,  0, 10, 0, 0,  0, 14, 1, 0,  0, 8, 0, 0,
  10, 11, 14,  0,   0, 0, 0, 0,  0, 0, 2, 0,   4, 5, 13, 1,  0, 0, 0, 0,
  11, 8, 13,   0,   0, 0, 2, 0,  0, 4, 11, 0,  0, 0, 10, 0,  0, 0, 8, 0,
  12, 13, 8,   1,   0, 0, 0, 0,  4, 5, 7, 1,   0, 0, 8, 0,   0, 0, 0, 0,
  13, 14, 11,  0,   0, 2, 0, 0,  0, 14, 1, 0,  0, 10, 0, 0,  0, 8, 0, 0,
  14, 15, 10,  0,   0, 0, 0, 0,  0, 2, 0, 0,   4, 13, 5, 1,  0, 0, 0, 0,
  15, 12, 9,   0,   0, 0, 2, 0,  0, 0, 10, 0,  0, 4, 11, 0,  0, 0, 8, 0,
  16, 17, 20,  2,   0, 0, 0, 0,  4, 5, 3, 0,   0, 0, 12, 1,  0, 0, 0, 0,
  17, 18, 23,  0,   0, 0, 2, 0,  0, 6, 9, 0,   0, 10, 0, 0,  0, 8, 0, 0,
  18, 19, 22,  0,   0, 0, 0, 0,  4, 3, 0, 0,   0, 12, 5, 1,  0, 0, 0, 0,
  19, 16, 21,  0,   0, 0, 2, 0,  0, 0, 10, 0,  0, 6, 9, 0,   0, 8, 0, 0,
  20, 21, 16,  1,   0, 0, 0, 0,  0, 6, 5, 1,   4, 9, 0, 0,   0, 0, 0, 0,
  21, 22, 19,  0,   0, 2, 0, 0,  0, 10, 0, 0,  0, 12, 3, 0,  0, 0, 8, 0,
  22, 23, 18,  0,   0, 0, 0, 0,  0, 0, 6, 1,   4, 5, 9, 0,   0, 0, 0, 0,
  23, 20, 17,  0,   0, 2, 0, 0,  0, 12, 3, 0,  0, 0, 10, 0,  0, 0, 8, 0,

  5,
  2,
  0, 1, 0,  2,   0, 0, 0, 0, 0,  0, 0, 0, 0, 0,   4, 5, 5, 5, 1,   0, 0, 0, 0, 0,   0, 0, 0, 0, 0,
  1, 0, 1,  0,   0, 0, 2, 0, 0,  0, 0, 10, 0, 0,  0, 0, 10, 0, 0,  0, 0, 10, 0, 0,  0, 0, 8, 0, 0,
};

static void 
leadingEmptyWidth(Polyominoes * polyo)
{
  int i, j;

  for (i = 0; i < polyo->size; i++) {
    for (j = 0; j < polyo->size; j++) {
      if (polyo->shape[j * polyo->size + i]) {
        polyo->leadingEmptyWidth = i;
        return;
      }
    }
  }
  polyo->leadingEmptyWidth = polyo->size;
}

static void
leadingEmptyHeight(Polyominoes * polyo)
{
  int i, j;

  for (j = 0; j < polyo->size; j++) {
    for (i = 0; i < polyo->size; i++) {
      if (polyo->shape[j * polyo->size + i]) {
        polyo->leadingEmptyHeight = j;
        return;
      }
    }
  }
  polyo->leadingEmptyHeight = polyo->size;
}

static void
readPolyominoes(trisstruct * tp)
{
  int   size = 0, n = 0, index = 0, sum = 0;
  int   counter, start_counter;
  int   polyomino, kinds, i, j, bonustype;

  for (bonustype = 0; bonustype < 2; bonustype++) {
    counter = 0, start_counter = 0;
    polytris[bonustype].mode.number = start_ominoes[bonustype];
    for (kinds = 0; kinds < area_kinds[bonustype]; kinds++) {
      size = ominos[index++];
      if (size > omino_squares[bonustype]) {
        (void) printf("tetris corruption: size %d\n", size);
        return;
      }
      n = ominos[index++];
      if (sum > max_ominoes[bonustype]) {
        (void) printf("tetris corruption: n %d\n", n);
        return;
      }
      for (polyomino = 0; polyomino < n; polyomino++) {
        if (polyomino + counter < max_ominoes[bonustype]) {
          sum = polyomino + counter;
        } else {
          (void) printf("tetris corruption: bonustype %d, polyomino %d, counter %d\n",
            bonustype, polyomino, counter);
          return;
        }
        if (polyomino != ominos[index]) {
          (void) printf("tetris corruption: polyomino %d, ominos[index] %d\n",
            polyomino, ominos[index]);
          return;
        }
        index++;  /* This is only there to "read" it */
        polytris[bonustype].polyomino[sum].rotation =
          ominos[index++] + counter;
        polytris[bonustype].polyomino[sum].reflection =
          ominos[index++] + counter; /* not used */

        if (ominos[index++]) {
          if (start_counter < start_ominoes[bonustype]) {
            polytris[bonustype].mode.start[start_counter++] = sum;
          } else {
            (void) printf("tetris corruption: bonustype %d, polyomino %d, start_counter %d\n",
              bonustype, polyomino, start_counter);
            return;
          }
        }
        polytris[bonustype].polyomino[sum].size = size;
        polytris[bonustype].polyomino[sum].shape = (int *) malloc(size *
    size * sizeof(int));
        for (j = 0; j < size; j++) {
          for (i = 0; i < size; i++) {
            polytris[bonustype].polyomino[sum].shape[j * size + i] =
        ominos[index++];
          }
        }
        leadingEmptyWidth(&(polytris[bonustype].polyomino[sum]));
        leadingEmptyHeight(&(polytris[bonustype].polyomino[sum]));
      }
      counter += n;
    }
  }
}

#ifdef DEBUG
static void
writePolyominoes(trisstruct * tp)
{
  int   size = 0;
  int   polyomino, i, j, bonustype;

  for (bonustype = 0; bonustype < 2; bonustype++) {
    for (polyomino = 0; polyomino < max_ominoes[bonustype]; polyomino++) {
      (void) printf("polyomino %d, rotation %d, reflection %d, size %d\n",
        polyomino, polytris[bonustype].polyomino[polyomino].rotation,
        polytris[bonustype].polyomino[polyomino].reflection,
        polytris[bonustype].polyomino[polyomino].size);
      size = polytris[bonustype].polyomino[polyomino].size;
      for (j = 0; j < size; j++) {
        (void) printf(" ");
        for (i = 0; i < size; i++) {
          (void) printf(" %d", polytris[bonustype].polyomino[polyomino].shape[j * size + i]);
        }
      }
      (void) printf("\n");
    }
    (void) printf("Starts:");
    for (polyomino = 0; polyomino < start_ominoes[bonustype]; polyomino++) {
      (void) printf(" %d", polytris[bonustype].mode.start[polyomino]);
    }
    (void) printf("\n");
  }
}
#endif

static void
setColour(ModeInfo * mi, int cid, int exor) 
{
  Display    *display = MI_DISPLAY(mi);
  trisstruct *tp = &triss[MI_SCREEN(mi)];
  long colour;

  if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
    if (tp->ncolors >= start_ominoes[(int) tp->bonusNow] + 2)
      colour = cid + 2;
    else
      colour = 1; /* Just in case */
    XSetBackground(display, tp->gc, tp->blackpixel);
    XSetForeground(display, tp->gc, tp->colors[colour].pixel);
  } else {
    if (MI_NPIXELS(mi) > start_ominoes[(int) tp->bonusNow])
      colour = MI_PIXEL(mi, cid * MI_NPIXELS(mi) / start_ominoes[(int) tp->bonusNow]);
    else
      colour = MI_WHITE_PIXEL(mi);
    XSetForeground(display, tp->gc, colour);
    XSetBackground(display, tp->gc, MI_BLACK_PIXEL(mi));
  }
}

static void
drawSquare(ModeInfo * mi, int pmid, int cid, int col, int row)
{
  Display    *display = MI_DISPLAY(mi);
  Window      window = MI_WINDOW(mi);
  trisstruct *tp = &triss[MI_SCREEN(mi)];
  long colour;

  setColour(mi, cid, 0);
  if (tp->well)
    XFillRectangle(display, window, tp->gc,
      tp->xb + col * tp->xs + 1, tp->yb + row * tp->ys + 1, tp->xs - 2, tp->ys - 2);
  else if (tp->pixelmode)
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
xorSquare(ModeInfo * mi, int i, int j)
{
  Display    *display = MI_DISPLAY(mi);
  Window      window = MI_WINDOW(mi);
  trisstruct *tp = &triss[MI_SCREEN(mi)];

  XSetFunction(display, tp->gc, GXxor);
  if (tp->well)
    XFillRectangle(display, window, tp->gc,
      tp->xb + i * tp->xs + 1, tp->yb + j * tp->ys + 1,
      tp->xs - 2, tp->ys - 2);
  else
    XFillRectangle(display, window, tp->gc,
      tp->xb, tp->yb + j * tp->ys,
      tp->xs * tp->ncols, tp->ys);
  XSetFunction(display, tp->gc, GXcopy);
}

static void
XFillTrapazoid(Display *display, Window window, GC gc,
  int ulx, int uly, int base_x, int base_y, int lrx, int lry,
  int plateau_x, int plateau_y)
{
  XPoint trapazoid_list[MAX_SIDES];

  trapazoid_list[0].x = ulx;
  trapazoid_list[0].y = uly;
  trapazoid_list[1].x = base_x;
  trapazoid_list[1].y = base_y;
  trapazoid_list[2].x = lrx - base_x - ulx;
  trapazoid_list[2].y = lry - base_y - uly;
  trapazoid_list[3].x = -plateau_x;
  trapazoid_list[3].y = -plateau_y;
  XFillPolygon(display, window, gc, trapazoid_list, MAX_SIDES,
    Convex, CoordModePrevious);
}

static void
drawWallSquare(ModeInfo * mi, GC gc, int i, int j)
{
  Display    *display = MI_DISPLAY(mi);
  Window      window = MI_WINDOW(mi);
  trisstruct *tp = &triss[MI_SCREEN(mi)];
  int ulx, uly, lrx, lry, base_x, plateau_x, w, offsetx, offsety;

  offsetx = tp->width / 2 - 3 * (WELL_WIDTH * tp->xs) / 2 - DELTA;
  offsety = tp->height / 2 - 3 * (WELL_WIDTH * tp->xs) / 2 - DELTA;
  w = i / WELL_WIDTH;
  i -= (w * WELL_WIDTH);
  ulx = 3 * BASE_WIDTH / 2 + (i - (WELL_WIDTH / 2)) *
    (2 * tp->xs * (WELL_DEPTH - j) / WELL_DEPTH + tp->xs) +
    2 * DELTA;
  uly = j * WELL_WIDTH * tp->xs / WELL_DEPTH + 2 * DELTA;
  lrx = 3 * BASE_WIDTH / 2 + (i + 1 - (WELL_WIDTH / 2)) *
    (2 * tp->xs * (WELL_DEPTH - j - 1) / WELL_DEPTH + tp->xs) +
    DELTA * (i / 4) - 3 * DELTA;
  lry = (j + 1) * WELL_WIDTH * tp->xs / WELL_DEPTH - DELTA;
  base_x = tp->xs + (WELL_DEPTH - j) * 2 * tp->xs / WELL_DEPTH -
    5 * DELTA;
  plateau_x = tp->xs + (WELL_DEPTH - j - 1) * 2 *
    tp->xs / WELL_DEPTH - 5 * DELTA;
  switch (w) {
  case 0:
    XFillTrapazoid(display, window, gc,
      ulx + offsetx, uly + offsety, base_x, 0,
      lrx + offsetx, lry + offsety, plateau_x, 0);
    break;
  case 1:
    XFillTrapazoid(display, window, gc,
      ENDPT - uly + offsetx, ulx + offsety, 0, base_x,
      ENDPT - lry + offsetx, lrx + offsety, 0, plateau_x);
    break;
  case 2:
    XFillTrapazoid(display, window, gc,
      ENDPT - 1 - ulx + offsetx, ENDPT - uly + offsety, -base_x, 0,
      ENDPT - 1 - lrx + offsetx, ENDPT - lry + offsety, -plateau_x, 0);
    break;
  case 3:
    XFillTrapazoid(display, window, gc,
      uly + offsetx, ENDPT - 1 - ulx + offsety, 0, -base_x,
      lry + offsetx, ENDPT - 1 - lrx + offsety, 0, -plateau_x);
    break;
  default:
    (void) fprintf (stderr, "trapazoid kinds range 0-3, got %d.\n", w);
  }
}

static void
clearTrapazoid(ModeInfo * mi, int i, int j)
{
  Display    *display = MI_DISPLAY(mi);
  Window      window = MI_WINDOW(mi);
  trisstruct *tp = &triss[MI_SCREEN(mi)];

  XSetForeground(display, tp->gc, tp->blackpixel);
  drawWallSquare(mi, tp->gc, i, j); /* pmid not used yet */
}

static void
drawTrapazoid(ModeInfo * mi, int pmid, int cid, int i, int j)
{
  trisstruct *tp = &triss[MI_SCREEN(mi)];

  setColour(mi, cid, 0);
  drawWallSquare(mi, tp->gc, i, j); /* pmid not used yet */
}

static void
xorTrapazoid(ModeInfo * mi, int pmid, int cid, int i, int j)
{
  trisstruct *tp = &triss[MI_SCREEN(mi)];

  setColour(mi, cid, 1);
  XSetFunction(MI_DISPLAY(mi), tp->gc, GXxor);
  drawWallSquare(mi, tp->gc, i, j); /* pmid not used yet */
  XSetFunction(MI_DISPLAY(mi), tp->gc, GXcopy);
}

static void
freezeTrapazoid(ModeInfo * mi, int i, int j)
{
  trisstruct *tp = &triss[MI_SCREEN(mi)];

  setColour(mi, -1, 1);
  XSetFunction(MI_DISPLAY(mi), tp->gc, GXxor);
  drawWallSquare(mi, tp->gc, i, j); /* pmid not used yet */
  XSetFunction(MI_DISPLAY(mi), tp->gc, GXcopy);
}

/* Wall number ... imagine a 4 hour clock...
     0
    3+1
     2
 */
static void
wall_to_base(int *base_x, int *base_y, int wall_x, int wall_y)
{
  switch (wall_x / WELL_WIDTH)
  {
  case 0:
    *base_x = wall_x;
    *base_y = wall_y - WELL_DEPTH;
    break;
  case 1:
    *base_x = WELL_WIDTH - 1 + WELL_DEPTH - wall_y;
    *base_y = wall_x - WELL_WIDTH;
    break;
  case 2:
    *base_x = WELL_PERIMETER - 1 - WELL_WIDTH - wall_x;
    *base_y = WELL_WIDTH - 1 + WELL_DEPTH - wall_y;
    break;
  case 3:
    *base_x = wall_y - WELL_DEPTH;
    *base_y = WELL_PERIMETER - 1 - wall_x;
    break;
  default:
   (void) fprintf (stderr, "wall_to_base kinds range 0-3, got %d.\n",
               wall_x / WELL_WIDTH);
  }
}

static void
dropWall(ModeInfo *mi, int w)
{
  Display    *display = MI_DISPLAY(mi);
  Window      window = MI_WINDOW(mi);
  GC          gc = MI_GC(mi);
  trisstruct *tp = &triss[MI_SCREEN(mi)];
  int i, j, k, l, lines, base_x, base_y;
  Bool fits;

  lines = 0;
  for (j = WELL_DEPTH - 1; j >= 0 && lines == 0; j--)
    for (i = 0; i < WELL_WIDTH; i++)
      if (tp->wall[j][w * WELL_WIDTH + i].pmid != EMPTY)
        lines = WELL_DEPTH - j;
  if (lines > 0) {
    fits = True;
    j = 0;
    while (j < (WELL_WIDTH / 2 + lines - 1) && fits) {
      j++;
      for (l = WELL_DEPTH - j; l < WELL_DEPTH; l++)
        for (i = 0; i < WELL_WIDTH; i++)
          if (tp->wall[l][w * WELL_WIDTH + i].pmid != EMPTY) {
            wall_to_base(&base_x, &base_y,
              w * WELL_WIDTH + i, l + j);
            if (tp->base[base_y][base_x].pmid != EMPTY)
              fits = False;
          }
    }
    if (!fits)
      j--;
    if (j > 0)
    {
      for (l = WELL_DEPTH - 1; l >= 0; l--)
        for (i = 0; i < WELL_WIDTH; i++)
          if (tp->wall[l][w * WELL_WIDTH + i].pmid != EMPTY) {
            k = w * WELL_WIDTH + i;
            if (l + j >= WELL_DEPTH) {
              wall_to_base(&base_x, &base_y, k, l + j);
              tp->base[base_y][base_x] = tp->wall[l][k];
              tp->wall[l][k].pmid = EMPTY;
              clearTrapazoid(mi, k, l);
              drawSquare(mi, tp->wall[l][k].pmid, tp->wall[l][k].cid, base_x, base_y);
            } else {
              tp->wall[l + j][k] = tp->wall[l][k];
              tp->wall[l][k].pmid = EMPTY;
              clearTrapazoid(mi, k, l);
              drawTrapazoid(mi, tp->wall[l][k].pmid, tp->wall[l][k].cid, k, l + j);
            }
            XFlush(display);
          }
    }
  }
}

static void
freezeWall(ModeInfo * mi, int w)
{
  trisstruct *tp = &triss[MI_SCREEN(mi)];
  int i, j;

  for (j = 0; j < WELL_DEPTH; j++)
    for (i = 0; i < WELL_WIDTH; i++) {
      freezeTrapazoid(mi, w * WELL_WIDTH + i, j);
    }
  XFlush(MI_DISPLAY(mi));
}

Bool
allFrozen(ModeInfo * mi)
{
  trisstruct *tp = &triss[MI_SCREEN(mi)];
  int w;

  for (w = 0; w < MAX_SIDES; w++)
    if (tp->frozen_wall[w] == MAX_SIDES) {
      freezeWall(mi, w);
      XFlush(MI_DISPLAY(mi));
    }
  for (w = 0; w < MAX_SIDES; w++)
    if (!tp->frozen_wall[w])
      return False;
  return True;
}

void
checkFreeze(ModeInfo *mi)
{
  trisstruct *tp = &triss[MI_SCREEN(mi)];
  int w;

  for (w = 0; w < MAX_SIDES; w++) {
    if (tp->frozen_wall[w] == 0) {
      dropWall(mi, w);
    } else {
      tp->frozen_wall[w]--;
      if (tp->frozen_wall[w] == 0) {
        freezeWall(mi, w);
        dropWall(mi, w);
      }
    }
  }
  /* pick a free spot on the rim,  not all walls frozen or else
     this is an infinite loop */
  tp->curPolyomino.xpos = NRAND(WELL_WIDTH - tp->curPolyomino.size + 1);
  do {
    w = NRAND(MAX_SIDES);
  } while (tp->frozen_wall[w]);
  tp->curPolyomino.xpos += w * WELL_WIDTH;
}

static void
drawBox(ModeInfo * mi, int pmid, int cid, int i, int j)
{
  if (j < WELL_DEPTH) {
    drawTrapazoid(mi, pmid, cid, i, j);
  } else {
    int base_i, base_j;

    wall_to_base(&base_i, &base_j, i, j);
    drawSquare(mi, pmid, cid, base_i, base_j);
  }
}

static void
clearSquare(ModeInfo * mi, int col, int row)
{
  Display    *display = MI_DISPLAY(mi);
  Window      window = MI_WINDOW(mi);
  trisstruct *tp = &triss[MI_SCREEN(mi)];

  XSetForeground(display, tp->gc, tp->blackpixel);
  if (tp->well)
    XFillRectangle(display, window, tp->gc,
      tp->xb + col * tp->xs + 1, tp->yb + row * tp->ys + 1,
      tp->xs - 2, tp->ys - 2);
  else
    XFillRectangle(display, window, tp->gc,
      tp->xb + col * tp->xs, tp->yb + row * tp->ys, tp->xs, tp->ys);
}

static void
clearBox(ModeInfo * mi, int i, int j)
{
  if (j < WELL_DEPTH) {
    clearTrapazoid(mi, i, j);
  } else {
    int base_i, base_j;

    wall_to_base(&base_i, &base_j, i, j);
    clearSquare(mi, base_i, base_j);
  }
}

static void
drawPolyomino(ModeInfo * mi)
{
  trisstruct *tp = &triss[MI_SCREEN(mi)];
  int   i, j;

  for (i = 0; i < tp->curPolyomino.size; i++) {
    for (j = 0; j < tp->curPolyomino.size; j++) {
      if (polytris[(int) tp->bonusNow].polyomino[tp->curPolyomino.polyomino_number].shape[j * tp->curPolyomino.size + i]) {
        if (tp->well) {
          int xi, yj;

          xi = (tp->curPolyomino.xpos + i) % WELL_PERIMETER;
          yj = tp->curPolyomino.ypos + j;
          drawBox(mi, polytris[(int) tp->bonusNow].polyomino
            [tp->curPolyomino.polyomino_number].shape[j * tp->curPolyomino.size + i],
            tp->curPolyomino.color_number, xi, yj);
        } else {
          drawSquare(mi, polytris[(int) tp->bonusNow].polyomino
            [tp->curPolyomino.polyomino_number].shape[j * tp->curPolyomino.size + i],
            tp->curPolyomino.color_number,
            tp->curPolyomino.xpos + i, tp->curPolyomino.ypos + j);
        }
      }
    }
  }
}

static void
drawPolyominoDiff(ModeInfo *mi, thing_t * old)
{
  trisstruct *tp = &triss[MI_SCREEN(mi)];
  int   i, j, ox, oy;

  for (i = 0; i < tp->curPolyomino.size; i++)
    for (j = 0; j < tp->curPolyomino.size; j++)
      if ((tp->curPolyomino.ypos + j >= 0) &&
        polytris[(int) tp->bonusNow].polyomino
          [tp->curPolyomino.polyomino_number].shape[j * tp->curPolyomino.size + i])
        if (tp->well) {
          int xi, yj;

          xi = (tp->curPolyomino.xpos + i) % WELL_PERIMETER;
          yj = tp->curPolyomino.ypos + j;
          drawBox(mi, polytris[(int) tp->bonusNow].polyomino
            [tp->curPolyomino.polyomino_number].shape[j * tp->curPolyomino.size + i],
            tp->curPolyomino.color_number, xi, yj);
        } else {
          drawSquare(mi, polytris[(int) tp->bonusNow].polyomino
            [tp->curPolyomino.polyomino_number].shape[j * tp->curPolyomino.size + i],
            tp->curPolyomino.color_number, tp->curPolyomino.xpos + i, tp->curPolyomino.ypos + j);
        }

  for (i = 0; i < tp->curPolyomino.size; i++)
    for (j = 0; j < tp->curPolyomino.size; j++) {
      ox = old->xpos + i - tp->curPolyomino.xpos;
      oy = old->ypos + j - tp->curPolyomino.ypos;
      if (polytris[(int) tp->bonusNow].polyomino[old->polyomino_number].shape[j * tp->curPolyomino.size + i] &&
        ((ox < 0) || (ox >= tp->curPolyomino.size) ||
        (oy < 0) || (oy >= tp->curPolyomino.size) ||
        !polytris[(int) tp->bonusNow].polyomino
          [tp->curPolyomino.polyomino_number].shape[oy * tp->curPolyomino.size + ox]))
        if (tp->well) {
          int xi, yj;

          xi = (old->xpos + i) % WELL_PERIMETER;
          yj = old->ypos + j;
          clearBox(mi, xi, yj);
        } else {
          clearSquare(mi, (old->xpos + i), (old->ypos + j));
        }
    }
}

static void
redoNext(ModeInfo * mi)
{
  trisstruct *tp = &triss[MI_SCREEN(mi)];
  int next_start, i;

  next_start = (int) (tp->curPolyomino.random_number % polytris[(int) tp->bonusNow].mode.number);
  tp->curPolyomino.color_number = next_start;

  tp->curPolyomino.polyomino_number = polytris[(int) tp->bonusNow].mode.start[next_start];
  for (i = 0; i < tp->curPolyomino.random_rotation; i++)
    tp->curPolyomino.polyomino_number = polytris[(int) tp->bonusNow].polyomino
      [tp->curPolyomino.polyomino_number].rotation;
  tp->curPolyomino.size = polytris[(int) tp->bonusNow].polyomino[tp->curPolyomino.polyomino_number].size;
  tp->curPolyomino.xpos = NRAND(tp->ncols - tp->curPolyomino.size + 1);
  /* tp->curPolyomino.xpos = (tp->ncols - tp->curPolyomino.size) / 2; */
  tp->curPolyomino.ypos = -polytris[(int) tp->bonusNow].polyomino
    [tp->curPolyomino.polyomino_number].leadingEmptyHeight;
}

static void
newPolyomino(ModeInfo * mi) {
  trisstruct *tp = &triss[MI_SCREEN(mi)];

  tp->curPolyomino.random_number = LRAND();
  tp->curPolyomino.random_rotation = NRAND(MAX_SIDES);
#if 0
  tp->curPolyomino.random_reflection = NRAND(2);  /* Future? */
#endif
  if (tp->bonus) {
    if (trackmouse) {
      tp->bonusNow = 0;
    } else {
      /* Unlikely to finish rows, so show bonus pieces this way */
      tp->bonusNow = !NRAND(16);
    }
  }
  redoNext(mi);
}

static void
putBox(ModeInfo * mi)
{
  trisstruct *tp = &triss[MI_SCREEN(mi)];
  int   i, j;
  int   x = tp->curPolyomino.xpos, y = tp->curPolyomino.ypos, pos;

  for (i = 0; i < tp->curPolyomino.size; i++)
    for (j = 0; j < tp->curPolyomino.size; j++)
      if ((y + j >= 0) && polytris[(int) tp->bonusNow].polyomino
        [tp->curPolyomino.polyomino_number].shape[j * tp->curPolyomino.size + i]) {
        if (tp->well) {
          int xi, yj;

          xi = (x + i) % WELL_PERIMETER;
          yj = y + j;
          if (yj < WELL_DEPTH) {
            tp->wall[yj][xi].pmid = polytris[(int) tp->bonusNow].polyomino
              [tp->curPolyomino.polyomino_number].shape[j * tp->curPolyomino.size + i];
            tp->wall[yj][xi].cid = tp->curPolyomino.color_number;
            tp->frozen_wall[xi / WELL_WIDTH] = MAX_SIDES;
          } else {
            int base_i, base_j;

            wall_to_base(&base_i, &base_j, xi, yj);
            tp->base[base_j][base_i].pmid = polytris[(int) tp->bonusNow].polyomino
              [tp->curPolyomino.polyomino_number].shape[j * tp->curPolyomino.size + i];
            tp->base[base_j][base_i].cid = tp->curPolyomino.color_number;
          }
        } else {
          pos = (y + j) * tp->ncols + x + i;
          tp->field[pos].pmid = polytris[(int) tp->bonusNow].polyomino
            [tp->curPolyomino.polyomino_number].shape[j * tp->curPolyomino.size + i] % BITMAPS;
          tp->field[pos].cid = tp->curPolyomino.color_number;
        }
      }
}

static Bool
overlapping(ModeInfo * mi)
{
  trisstruct *tp = &triss[MI_SCREEN(mi)];
  int       i, j;
  int       x = tp->curPolyomino.xpos, y = tp->curPolyomino.ypos;
  Bool gradualAppear = tp->well;

  if (tp->well) {
    for (i = 0; i < tp->curPolyomino.size; i++)
      for (j = 0; j < tp->curPolyomino.size; j++)
        if (polytris[(int) tp->bonusNow].polyomino
                [tp->curPolyomino.polyomino_number].shape[j * tp->curPolyomino.size + i]) {
          int xi, yj;

          xi = (x + i) % WELL_PERIMETER;
          yj = y + j;
          if (yj < WELL_DEPTH) {
            if (tp->frozen_wall[xi / WELL_WIDTH])
              return True;
            if (gradualAppear) {
/* This method one can turn polyomino to an area above of screen, also
   part of the polyomino may not be visible initially */
              if ((yj >= 0) && (tp->wall[yj][xi].pmid >= 0))
                return True;
            } else {
/* This method does not allow turning polyomino to an area above screen */
              if ((yj < 0) || (tp->wall[yj][xi].pmid >= 0))
                return True;
            }
          }
          else if (yj < WELL_DEPTH + WELL_WIDTH) {
            int base_i, base_j;

            wall_to_base(&base_i, &base_j, xi, yj);
            if (tp->base[base_j][base_i].pmid >= 0)
              return True;
          }
          else
            return True;
        }
  } else {
    for (i = 0; i < tp->curPolyomino.size; i++)
      for (j = 0; j < tp->curPolyomino.size; j++)
        if (polytris[(int) tp->bonusNow].polyomino
            [tp->curPolyomino.polyomino_number].shape[j * tp->curPolyomino.size + i]) {
          if ((y + j >= tp->nrows) || (x + i < 0) || (x + i >= tp->ncols))
            return True;
          if (gradualAppear) {
/* This method one can turn polyomino to an area above of screen, also
   part of the polyomino may not be visible initially */
            if ((y + j >= 0) &&
                (tp->field[(y + j) * tp->ncols + x + i].pmid >= 0))
              return True;
          } else {
/* This method does not allow turning polyomino to an area above screen */
            if ((y + j < 0) ||
                (tp->field[(y + j) * tp->ncols + x + i].pmid >= 0))
              return True;
          }
        }
  }
  return False;
}

static Bool
atBottom(ModeInfo * mi)
{
  trisstruct *tp = &triss[MI_SCREEN(mi)];
  int       i, j;
  int       x = tp->curPolyomino.xpos, y = tp->curPolyomino.ypos;

  if (tp->well) {
    for (i = 0; i < tp->curPolyomino.size; i++)
      for (j = 0; j < tp->curPolyomino.size; j++)
        if (polytris[(int) tp->bonusNow].polyomino
            [tp->curPolyomino.polyomino_number].shape[j * tp->curPolyomino.size + i]) {
          int xi, yj;

          xi = (x + i) % WELL_PERIMETER;
          yj = y + j;
          if (yj < -1)
            return False;
          if (yj < WELL_DEPTH - 1) {
            if (tp->frozen_wall[xi / WELL_WIDTH])
              return True;
            if (tp->wall[yj + 1][xi].pmid >= 0)
              return True;
          } else if (yj < WELL_DEPTH + WELL_WIDTH - 1) {
            int base_i, base_j;

            wall_to_base(&base_i, &base_j, xi, yj + 1);
            if (tp->base[base_j][base_i].pmid >= 0)
              return True;
          } else
            return True;
        }
  } else {
    for (i = 0; i < tp->curPolyomino.size; i++)
      for (j = 0; j < tp->curPolyomino.size; j++)
        if ((y + j >= -1) && polytris[(int) tp->bonusNow].polyomino
          [tp->curPolyomino.polyomino_number].shape[j * tp->curPolyomino.size + i])
          if ((y + j >= tp->nrows - 1) || (x + i < 0) || (x + i >= tp->ncols) ||
              (tp->field[(y + j + 1) * tp->ncols + x + i].pmid >= 0))
            return True;
  }
  return False;
}

static Bool
atBaseFully(ModeInfo * mi)
{
  trisstruct *tp = &triss[MI_SCREEN(mi)];
  int i, j;

  for (i = 0; i < tp->curPolyomino.size; i++)
    for (j = 0; j < tp->curPolyomino.size; j++)
      if (tp->curPolyomino.ypos + j < WELL_DEPTH &&
          polytris[(int) tp->bonusNow].polyomino
           [tp->curPolyomino.polyomino_number].shape[j * tp->curPolyomino.size + i])
    return False;
  return True;
}

static Bool
atBasePartially(ModeInfo * mi)
{
  trisstruct *tp = &triss[MI_SCREEN(mi)];
  int i, j;

  for (i = 0; i < tp->curPolyomino.size; i++)
    for (j = 0; j < tp->curPolyomino.size; j++)
        if (tp->curPolyomino.ypos + j >= WELL_DEPTH &&
            polytris[(int) tp->bonusNow].polyomino
              [tp->curPolyomino.polyomino_number].shape[j * tp->curPolyomino.size + i])
          return True;
     return False;
}

static Bool
wallChange(ModeInfo * mi, thing_t old, thing_t new)
{
  trisstruct *tp = &triss[MI_SCREEN(mi)];
  int             w = -1, i, j;
  int             x = tp->curPolyomino.xpos;

  for (i = 0; i < old.size; i++)
    for (j = 0; j < old.size; j++)
  if (polytris[(int) tp->bonusNow].polyomino[old.polyomino_number].shape[j * tp->curPolyomino.size + i]) {
    if (w == -1)
      w = ((x + i) % WELL_PERIMETER) / WELL_WIDTH;
    else if (w != ((x + i) % WELL_PERIMETER) / WELL_WIDTH)
      return True;
  }
  for (i = 0; i < new.size; i++)
    for (j = 0; j < new.size; j++)
      if (polytris[(int) tp->bonusNow].polyomino
                [tp->curPolyomino.polyomino_number].shape[j * tp->curPolyomino.size + i]) {
          if (w != ((x + i) % WELL_PERIMETER) / WELL_WIDTH)
            return True;
      }
  return False;
}

static void
tryMove(ModeInfo * mi, move_t move)
{
  trisstruct *tp = &triss[MI_SCREEN(mi)];
  thing_t   old;
  int       i;
  int       skip = False;
  int       cw = False; /* I am not sure if this is the original */

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
      if (tp->well) {
        tp->curPolyomino.xpos = (tp->curPolyomino.xpos + WELL_PERIMETER - 1) % WELL_PERIMETER;
        if (atBaseFully(mi) || (atBasePartially(mi) && wallChange(mi, old, tp->curPolyomino)))
          skip = True;
      } else
        tp->curPolyomino.xpos--;
      break;

    case RIGHT:
      if (tp->well) {
        tp->curPolyomino.xpos = (tp->curPolyomino.xpos + WELL_PERIMETER + 1) % WELL_PERIMETER;
        if (atBaseFully(mi) || (atBasePartially(mi) && wallChange(mi, old, tp->curPolyomino)))
          skip = True;
      } else
        tp->curPolyomino.xpos++;
      break;

    case ROTATE:
        if (cw)
          for (i = 0; i < MAX_SIDES - 1; i++)
            tp->curPolyomino.polyomino_number = polytris[(int) tp->bonusNow].polyomino
              [tp->curPolyomino.polyomino_number].rotation;
        else /* ccw */
          tp->curPolyomino.polyomino_number = polytris[(int) tp->bonusNow].polyomino
            [tp->curPolyomino.polyomino_number].rotation;
      tp->curPolyomino.xpos = old.xpos;
      tp->curPolyomino.ypos = old.ypos;
      if (tp->well && atBasePartially(mi) && wallChange(mi, old, tp->curPolyomino))
          skip = True;
      break;

    case REFLECT: /* reflect on y axis */
      tp->curPolyomino.polyomino_number = polytris[(int) tp->bonusNow].polyomino
        [tp->curPolyomino.polyomino_number].reflection;
      tp->curPolyomino.xpos = old.xpos;
      tp->curPolyomino.ypos = old.ypos;
      if (tp->well && atBasePartially(mi) && wallChange(mi, old, tp->curPolyomino))
          skip = True;
      break;
  }

  if (!skip && !overlapping(mi)) {
    drawPolyominoDiff(mi, &old);
  } else {
    tp->curPolyomino = old;
    tp->sidemoves = tp->ncols / 2;
  }
}

void
fillLines(ModeInfo *mi)
{
  Display    *display = MI_DISPLAY(mi);
  trisstruct *tp = &triss[MI_SCREEN(mi)];
  int             i, j;

  XFlush(display);
  if (tp->well) {
    for (j = 0; j < WELL_DEPTH + WELL_WIDTH; j++)
      for (i = 0; i < WELL_PERIMETER; i++) {
         drawBox(mi, 0,
           LRAND() % polytris[(int) tp->bonusNow].mode.number,
                 i, j);
         XFlush(display);
      }
  } else {
    for (j = 0; j <= tp->nrows / 2; j++) {
      for (i = 0; i < tp->ncols; i++) {
         drawSquare(mi, 0,
           LRAND() % polytris[(int) tp->bonusNow].mode.number,
                 i, j);
         drawSquare(mi, 0,
           LRAND() % polytris[(int) tp->bonusNow].mode.number,
                 i, tp->nrows - j - 1);
      }
      XFlush(display);
    }
  }
}

static int
checkLines(ModeInfo *mi)
{
  trisstruct *tp = &triss[MI_SCREEN(mi)];
  Display    *display = MI_DISPLAY(mi);
  Window      window = MI_WINDOW(mi);

  if (tp->well) {
    int             lSet[2][WELL_WIDTH], nset[2];
    int             i, j, k;

    nset[0] = nset[1] = 0;

    for (j = 0; j < WELL_WIDTH; j++) {
      lSet[0][j] = 0;
      for (i = 0; i < WELL_WIDTH; i++)
        if (tp->base[j][i].pmid >= 0)
          lSet[0][j] ++;
      if (lSet[0][j] == WELL_WIDTH)
        nset[0] ++;
    }
    for (i = 0; i < WELL_WIDTH; i++) {
      lSet[1][i] = 0;
      for (j = 0; j < WELL_WIDTH; j++)
        if (tp->base[j][i].pmid >= 0)
          lSet[1][i] ++;
      if (lSet[1][i] == WELL_WIDTH)
        nset[1] ++;
    }

    if (nset[0] || nset[1]) {
        fieldstruct temp_base[WELL_WIDTH][WELL_WIDTH];
        for (k = 0; k < ((NUM_FLASHES / (nset[0] + nset[1])) % 2) * 2; k++) {
            for (j = 0; j < WELL_WIDTH; j++) {
                if (lSet[0][j] == WELL_WIDTH)
                  for (i = 0; i < WELL_WIDTH; i++)
                    xorSquare(mi, i, j);
                if (lSet[1][i] == WELL_WIDTH)
                  for (i = 0; i < WELL_WIDTH; i++)
                    xorSquare(mi, j, i);
            }
            XFlush(display);
        }

        for (j = 0; j < WELL_WIDTH; j++)
          for (i = 0; i < WELL_WIDTH; i++)
            temp_base[j][i] = tp->base[j][i];
        if (nset[0] > 0)
          for (j = 0; j < WELL_WIDTH; j++)
            if (lSet[0][j] == WELL_WIDTH) {
              if (j < WELL_WIDTH / 2)
                for (i = 0; i < WELL_WIDTH; i++) {
                  for (k = j; k > 0; k--)
                    temp_base[k][i] = temp_base[k - 1][i];
                  temp_base[0][i].pmid = EMPTY;
                }
              else
                for (i = 0; i < WELL_WIDTH; i++) {
                  for (k = j; k < WELL_WIDTH - 1; k++)
                    temp_base[k][i] = temp_base[k + 1][i];
                  temp_base[WELL_WIDTH - 1][i].pmid = EMPTY;
                }
            }
        if (nset[1] > 0)
          for (i = 0; i < WELL_WIDTH; i++)
            if (lSet[1][i] == WELL_WIDTH) {
              if (i < WELL_WIDTH / 2)
                for (j = 0; j < WELL_WIDTH; j++) {
                  for (k = i; k > 0; k--)
                    temp_base[j][k] = temp_base[j][k - 1];
                  temp_base[j][0].pmid = EMPTY;
                }
              else
                for (j = 0; j < WELL_WIDTH; j++) {
                  for (k = i; k < WELL_WIDTH - 1; k++)
                    temp_base[j][k] = temp_base[j][k + 1];
                  temp_base[j][WELL_WIDTH - 1].pmid = EMPTY;
                }
            }
       for (j = 0; j < WELL_WIDTH; j++)
         for (i = 0; i < WELL_WIDTH; i++)
           if ((temp_base[j][i].cid != tp->base[j][i].cid) ||
               (temp_base[j][i].pmid != tp->base[j][i].pmid))
           {
             tp->base[j][i] = temp_base[j][i];
             if (tp->base[j][i].pmid >= 0)
               drawSquare(mi, tp->base[j][i].pmid, tp->base[j][i].cid, i, j);
             else
               clearSquare(mi, i, j);
             XFlush(display);
           }

      XFlush(display);
    }

    return (nset[0] + nset[1]);
  } else {
    int      *lSet, nset = 0;
    int       i, j, y;

    lSet = (int *) calloc(tp->nrows, sizeof (int));
    for (j = 0; j < tp->nrows; j++) {
      for (i = 0; i < tp->ncols; i++)
        if (tp->field[j * tp-> ncols + i].pmid >= 0)
          lSet[j]++;
      if (lSet[j] == tp->ncols)
        nset++;
    }

    if (nset) {
      for (i = 0; i < ((NUM_FLASHES / nset) % 2) * 2; i++) {
        for (j = 0; j < tp->nrows; j++) {
          if (lSet[j] == tp->ncols)
            xorSquare(mi, i, j);
        }
        XFlush(display);
      }

      for (j = tp->nrows - 1; j >= 0; j--) {
        if (lSet[j] == tp->ncols) {
          for (y = j; y > 0; y--)
            for (i = 0; i < tp->ncols; i++)
          tp->field[y * tp->ncols + i] =
             tp->field[(y - 1) * tp->ncols + i];
          for (i = 0; i < tp->ncols; i++)
            tp->field[i].pmid = EMPTY;

          XCopyArea(display, window, window, tp->gc,
            tp->xb, tp->yb,
            tp->xs * tp->ncols, j * tp->ys,
            tp->xb, tp->yb + tp->ys);
          XSetBackground(display, tp->gc,
            MI_BLACK_PIXEL(mi));
          XFillRectangle(display, window, tp->gc,
            tp->xb, tp->yb, tp->xs * tp->ncols, tp->ys);

          for (i = j; i > 0; i--)
            lSet[i] = lSet[i - 1];
          lSet[0] = 0;

          if (j > 0)
            for (i = 0; i < tp->ncols; i++) {
              int       tmp = tp->field[j * tp->ncols + i].pmid;

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
              int       tmp = tp->field[j * tp->ncols + i].pmid;

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
    }

    if (lSet)
      (void) free((void *) lSet);
    return nset;
  }
}

static void
gameOver(ModeInfo *mi)
{
  /* maybe more stuff later */
  fillLines(mi);
}

void
refresh_tetris(ModeInfo * mi)
{
  trisstruct *tp = &triss[MI_SCREEN(mi)];

  if (!tp->painted)
    return;
  MI_CLEARWINDOW(mi);
  if (tp->well) {
    int i, j;

    for (i = 0; i < WELL_PERIMETER; i++) {
      for (j = 0; j < WELL_DEPTH; j++) {
        if (tp->wall[j][i].pmid >= 0) {
          drawTrapazoid(mi,  tp->wall[j][i].pmid, tp->wall[j][i].cid, i, j);
        }
        if (tp->frozen_wall[i / WELL_WIDTH]) {
          freezeTrapazoid(mi, i, j);
        }
      }
    }
    for (i = 0; i < WELL_WIDTH; i++)
      for (j = 0; j < WELL_WIDTH; j++)
        if (tp->base[j][i].pmid >= 0)
          drawSquare(mi,  tp->base[j][i].pmid, tp->base[j][i].cid, i, j);
  } else {
    int col, row, pos;

    for (col = 0; col < tp->ncols; col++)
      for (row = 0; row < tp->nrows; row++) {
        pos = row * tp->ncols + col;
#if DEBUG 
        (void) printf("pmid %d, cid %d, %d %d\n",
          tp->field[pos].pmid, tp->field[pos].cid, col, row);
#endif
        if (tp->field[pos].pmid != EMPTY)
          drawSquare(mi, tp->field[pos].pmid, tp->field[pos].cid, col, row);
    }
  }
  drawPolyomino(mi);
}

void
release_tetris(ModeInfo * mi) {
  Display *   display = MI_DISPLAY(mi);
  int bonustype, number;

  if (triss != NULL) {
    int   screen;

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
    for (bonustype = 0; bonustype < 2; bonustype++) {
      for (number = 0; number < max_ominoes[bonustype]; number++) {
        if (polytris[bonustype].polyomino[number].shape)
          (void) free((void *) polytris[bonustype].polyomino[number].shape);
      }
      (void) free((void *) polytris[bonustype].polyomino);
      (void) free((void *) polytris[bonustype].mode.start);
    }
  }
}

void
change_tetris(ModeInfo * mi) {
  tryMove(mi, ROTATE);
}

void
init_tetris(ModeInfo * mi) {
  Display *   display = MI_DISPLAY(mi);
  Window      window = MI_WINDOW(mi);
  XGCValues       gcv;
  trisstruct *tp;
  int size = MI_SIZE(mi);
  int   i, j, first = 0, bonustype;

  if (triss == NULL) {
    if ((triss = (trisstruct *) calloc(MI_NUM_SCREENS(mi),
               sizeof (trisstruct))) == NULL)
      return;
    for (bonustype = 0; bonustype < 2; bonustype++) {
      polytris[bonustype].mode.start = (int *) malloc(start_ominoes[bonustype] * sizeof(int));
      polytris[bonustype].polyomino = (Polyominoes *) malloc(max_ominoes[bonustype] * sizeof(Polyominoes));
    }
    tp = &triss[MI_SCREEN(mi)];
    readPolyominoes(tp);
#ifdef DEBUG
    writePolyominoes(tp);
#endif
  } else
    tp = &triss[MI_SCREEN(mi)];

  if (MI_IS_FULLRANDOM(mi)) {
    tp->bonus = (Bool) (LRAND() & 1);
    tp->well = (Bool) (LRAND() & 1);
  } else {
    tp->bonus = bonus;
    tp->well = well;
  }

#define UNDER_CONSTRUCTION
#ifndef UNDER_CONSTRUCTION
  if (trackmouse)
    tp->well = False;
#endif 
  if (!tp->gc) {
#ifdef BM_3D
    icon_width = rot00_width;
    icon_height = rot00_height;
#else
#ifdef BM_PLAIN
    icon_width = rop00_width;
    icon_height = rop00_height;
#endif
#endif
    for (i = 0; i < BITMAPS; i++) {
      logo[i].width = icon_width;
      logo[i].height = icon_height;
      logo[i].bytes_per_line = (icon_width + 7) / 8;
    }

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
           (unsigned long) 0, (XGCValues *) NULL)) == None) {
      return;
    }
    gcv.function = GXxor;
    gcv.foreground = tp->fg;
    gcv.background = tp->bg;
  }
  MI_CLEARWINDOW(mi);
  tp->painted = False;

  tp->width = MI_WIDTH(mi);
  tp->height = MI_HEIGHT(mi);

  if (tp->width < 2 + 2 * tp->well)
    tp->width = 2 + 2 * tp->well;
  if (tp->height < 2 + 2 * tp->well)
    tp->height = 2 + 2 * tp->well;
  if (size == 0 ||
    MINGRIDSIZE * size > tp->width || MINGRIDSIZE * size > tp->height) {
      if (!tp->well && tp->width > MINGRIDSIZE * icon_width &&
        tp->height > MINGRIDSIZE * icon_height) {
        tp->pixelmode = False;
        tp->xs = icon_width;
        tp->ys = icon_height;
      } else {
        tp->pixelmode = True;
        tp->xs = tp->ys = MAX(MINSIZE, MIN(tp->width, tp->height) /
            ((tp->well) ? 3 * MINGRIDSIZE : MINGRIDSIZE));
      }
  } else {
    tp->pixelmode = True;
    if (size < -MINSIZE)
      tp->ys = NRAND(MIN(-size, MAX(MINSIZE, MIN(tp->width, tp->height) /
        ((tp->well) ? 3 * MINGRIDSIZE : MINGRIDSIZE))) - MINSIZE + 1) + MINSIZE;
    else if (size < MINSIZE)
      tp->ys = MINSIZE;
    else
      tp->ys = MIN(size, MAX(MINSIZE, MIN(tp->width, tp->height) /
        ((tp->well) ? 3 * MINGRIDSIZE : MINGRIDSIZE)));
    tp->xs = tp->ys;
  }

  if (tp->well) {
    tp->ncols = MAX_SIDES * WELL_WIDTH;
    tp->nrows = WELL_DEPTH;
    tp->xb = tp->width / 2 - tp->xs * WELL_WIDTH / 2;
    tp->yb = tp->height / 2 - tp->ys * WELL_WIDTH / 2;
    for (i = 0; i < WELL_PERIMETER; i++) {
      for (j = 0; j < WELL_DEPTH + WELL_WIDTH; j++) {
        tp->wall[j][i].pmid = EMPTY;
        tp->wall[j][i].cid = 0;
      }
    }
    for (i = 0; i < WELL_WIDTH; i++) {
      for (j = 0; j < WELL_WIDTH; j++) {
        tp->base[j][i].pmid = EMPTY;
        tp->base[j][i].cid = 0;
      }
    }
    for (i = 0; i < MAX_SIDES; i++) {
      tp->frozen_wall[i] = 0;
    }
  } else {
    tp->ncols = MAX(tp->width / tp->xs, 2);
    tp->nrows = MAX(tp->height / tp->ys, 2);
    tp->xb = (tp->width - tp->xs * tp->ncols) / 2;
    tp->yb = (tp->height - tp->ys * tp->nrows) / 2;
    if (tp->field)
      (void) free((void *) tp->field);
    tp->field = (fieldstruct *) malloc(tp->ncols * tp->nrows * sizeof (fieldstruct));
    for (i = 0; i < tp->ncols * tp->nrows; i++) {
      tp->field[i].pmid = EMPTY;
      tp->field[i].cid = 0;
    }
  }
  tp->rows = 0;
  tp->level = 0;
  tp->bonusNow = False;

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

static Bool
moveOne(ModeInfo *mi, move_t move)
{
  Display    *display = MI_DISPLAY(mi);
  trisstruct *tp = &triss[MI_SCREEN(mi)];

  if (move == FALL) {
    if (trackmouse) {
      tp->sidemoves = tp->sidemoves + 1;
      if (tp->sidemoves < tp->ncols / 2) {
        Window      r, c;
        int         rx, ry, cx, cy;
        unsigned int m;

        (void) XQueryPointer(MI_DISPLAY(mi), MI_WINDOW(mi),
          &r, &c, &rx, &ry, &cx, &cy, &m);
        if (tp->well) {
          cx = cx - tp->width / 2;
          cy = cy - tp->height / 2;
          if (ABS(cx) <= tp->xs * tp->ncols / 2 ||
              ABS(cy) <= tp->ys * tp->nrows / 2) {
            int ominopos, mousepos, diff;
            double wall;

            /* Avoid atan2: DOMAIN error message */
            if (cy == 0 && cx == 0)
              wall = 0.0;
            else
              wall = 2.0 * (atan2(cy, cx) + 3 * M_PI / 4) / M_PI;
            if (wall < 0.0)
              wall += 4.0;
            mousepos = (int) (wall * WELL_WIDTH);
            ominopos = tp->curPolyomino.xpos;
            diff = mousepos - ominopos;
            if (diff < 0)
              diff += WELL_WIDTH * MAX_SIDES;
            if (diff > WELL_WIDTH * (MAX_SIDES / 2))
              move = LEFT;
            else if (diff > 0)
              move = RIGHT;
          }
        } else {
          cx = (cx - tp->xb) / tp->xs;
          cy = (cy - tp->yb) / tp->ys;
          if (!(cx < 0 || cy < 1 || cx >= tp->ncols || cy > tp->nrows)) {
            if (cx > tp->curPolyomino.xpos +
  polytris[(int) tp->bonusNow].polyomino[tp->curPolyomino.polyomino_number].leadingEmptyWidth) {
              move = RIGHT;
            } else if (cx < tp->curPolyomino.xpos +
  polytris[(int) tp->bonusNow].polyomino[tp->curPolyomino.polyomino_number].leadingEmptyWidth) {
              move = LEFT;
            }
          }
        }
      } else {
        tp->sidemoves = 0;
      }
    } else if (!NRAND(4)) {
      if (LRAND() & 1)
        move = ROTATE;
      else if (LRAND() & 1)
        move = LEFT;
      else
        move = RIGHT;
    }
  }
  if ((move == DROP) || ((move == FALL) && atBottom(mi))) {
    putBox(mi);
    {
      int       lines;

      lines = checkLines(mi);
      tp->rows += lines;
      if (tp->rows > THRESHOLD(tp->level)) {
        tp->level++;
        if (tp->bonus) {
          tp->bonusNow = True; /* No good deed goes unpunished */
        }
      }
    }
    newPolyomino(mi);
    XFlush(display);
    if (tp->well) {
      if (allFrozen(mi)) {
        gameOver(mi);
        init_tetris(mi);
        return False;
      }
      checkFreeze(mi);
    }
    if (overlapping(mi)) {
      gameOver(mi);
      init_tetris(mi);
      return False;
    }
    drawPolyomino(mi);
    return True;
  } else {
    tryMove(mi, move);
    if (tp->rows > THRESHOLD(tp->level)) {
      tp->level++;
      if (tp->bonus) {
        tp->bonusNow = True;
      }
    }
    return False;
  }
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

#endif /* MODE_tetris */
