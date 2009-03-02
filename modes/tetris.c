/* -*- Mode: C; tab-width: 4 -*- */
/* tetris --- autoplaying tetris game */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)tetris.c	5.01 2000/12/19 xlockmore";

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
 *       on tetris)
 *
 * Revision History:
 * 01-Dec-2000: Lifted code from polyominoes for variable size pieces
 *              Copyright (c) 2000 by Stephen Montgomery-Smith
 * 01-Nov-2000: Allocation checks
 * 02-Aug-2000: alwelltris trackmouse implemented
 * 27-Jul-2000: alwelltris all but trackmouse
 * 22-Jun-2000: trackmouse added, row clearing fixed, bonus pieces,
 * 01-Nov-1998: altetris additions by David Bagley
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
#define DEF_PLAIN "False"
#define DEF_TRACKMOUSE  "False"
#define DEF_WELL "False"

static Bool bonus;
static Bool cycle_p;
static Bool plain;
static Bool trackmouse;
static Bool well;

#define INTRAND(min,max) (NRAND((max+1)-(min))+(min))

static XrmOptionDescRec opts[] =
{
  {(char *) "-bonus", (char *) ".tetris.bonus", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+bonus", (char *) ".tetris.bonus", XrmoptionNoArg, (caddr_t) "off"},
  {(char *) "-cycle", (char *) ".tetris.cycle", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+cycle", (char *) ".tetris.cycle", XrmoptionNoArg, (caddr_t) "off"},
  {(char *) "-plain", (char *) ".tetris.plain", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+plain", (char *) ".tetris.plain", XrmoptionNoArg, (caddr_t) "off"},
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
  {(caddr_t *) & plain, (char *) "plain", (char *) "Plain", (char *) DEF_PLAIN, t_Bool},
#ifndef DISABLE_INTERACTIVE
  {(caddr_t *) & trackmouse, (char *) "trackmouse", (char *) "TrackMouse", (char *) DEF_TRACKMOUSE, t_Bool},
#endif
  {(caddr_t *) & well, (char *) "well", (char *) "Well", (char *) DEF_WELL, t_Bool}
};
static OptionStruct desc[] =
{
  {(char *) "-/+bonus", (char *) "turn on/off larger bonus pieces"},
  {(char *) "-/+cycle", (char *) "turn on/off colour cycling"},
  {(char *) "-/+plain", (char *) "turn on/off plain pieces"},
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
 600000, -40, 200, -100, 64, 1.0, "",
 "Shows an autoplaying tetris game", 0, NULL};

#endif

#include "bitmaps/gray1.xbm"

#define MINGRIDSIZE 10 
#define MAXPIXELSIZE 12
#define MINSIZE 6 
#define MAX_SIDES 4

#define NUM_FLASHES 16
#define BITMAPS 256

#define DELTA 1
#define WELL_WIDTH 8
#define WELL_DEPTH 12
#define WELL_PERIMETER (MAX_SIDES*WELL_WIDTH)
#define BASE_WIDTH (WELL_WIDTH*tp->xs+DELTA)
#define ENDPT (3*BASE_WIDTH)

#define THRESHOLD(x) ((x+1)*10)
#define EMPTY (-1)

typedef enum {FALL, DROP, MOVE_LEFT, MOVE_RIGHT, ROTATE, REFLECT} move_t;

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
  Bool   cycle_p, mono_p, no_colors, use3D;
  unsigned long blackpixel, whitepixel, fg, bg;
  int   direction;
  unsigned long colour;
  int   p_type;
  int   sidemoves;

  int   xb, yb;
  int   xs, ys;
  int   width, height;
  int   pixelmode;
  XImage *images[BITMAPS];
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
  Pixmap graypix;

  ModeInfo *mi;
} trisstruct;

#define ARR(i,j) (((i)<0||(i)>=tp->curPolyomino.size||\
(j)<0||(j)>=tp->curPolyomino.size)?-2:\
(polytris[(int) tp->bonusNow].polyomino[tp->curPolyomino.polyomino_number].shape[(j) * tp->curPolyomino.size + (i)]>0))

#define ROUND8(n) ((((n)+7)/8)*8)

/* Defines to index the bitmaps.  A set bit indicates that an edge or
   corner is required. */
#define LEFT 1 /* (1<<0) */
#define DOWN 2 /* (1<<1) */
#define RIGHT 4 /* (1<<2) */
#define UP 8 /* (1<<3) */
#define LEFT_DOWN 16 /* (1<<4) */
#define RIGHT_DOWN 32 /* (1<<5) */
#define RIGHT_UP 64 /* (1<<6) */
#define LEFT_UP 128 /* (1<<7) */
#define IS_LEFT(n) ((n)&LEFT)
#define IS_DOWN(n) ((n)&DOWN)
#define IS_RIGHT(n) ((n)&RIGHT)
#define IS_UP(n) ((n)&UP)
#define IS_LEFT_DOWN(n) ((n)&LEFT_DOWN)
#define IS_RIGHT_DOWN(n) ((n)&RIGHT_DOWN)
#define IS_RIGHT_UP(n) ((n)&RIGHT_UP)
#define IS_LEFT_UP(n) ((n)&LEFT_UP)

#define CHECKLEFT(x) ((x)|LEFT)
#define CHECKDOWN(x) ((x)|DOWN)
#define CHECKRIGHT(x) ((x)|RIGHT)
#define CHECKUP(x) ((x)|UP)

/* Defines to access the bitmaps. */
#define BITNO(x,y) ((x)+(y)*ROUND8(tp->ys))
#define SETBIT(x,y) {data[BITNO(x,y)/8] |= 1<<(BITNO(x,y)%8);}
#define RESBIT(x,y) {data[BITNO(x,y)/8] &= ~(1<<(BITNO(x,y)%8));}
#define TWOTHIRDSBIT(x,y) {if ((x+y-1)%3) SETBIT(x,y) else RESBIT(x,y)}
#define HALFBIT(x,y) {if ((x-y)%2) SETBIT(x,y) else RESBIT(x,y)}
#define THIRDBIT(x,y) {if (!((x-y-1)%3)) SETBIT(x,y) else RESBIT(x,y)}
#define THREEQUARTERSBIT(x,y) \
  {if ((y%2)||((x+2+y/2+1)%2)) SETBIT(x,y) else RESBIT(x,y)}
#define NOTHALFBIT(x,y) {if ((x-y)%2) RESBIT(x,y) else SETBIT(x,y)}

/* Parameters for bitmaps. */
#define G ((tp->ys/45)+1)         /* 1/2 of gap between pentominoes. */
#define T ((tp->ys<=12)?1:(G*2))  /* Thickness of walls of pentominoes. */
#define R ((tp->ys<=12)?1:(G*6))  /* Amount of rounding. */
#define RT ((tp->ys<=12)?1:(G*3)) /* Thickness of wall of rounded parts. 
                                  Here 3 is an approximation to 2 sqrt(2).  */
#define RR 0   /* Roof ridge thickness */

/* A list of those bitmaps we need to create to display any polyomino. */
static int images_needed[] =
{
/* This needed for mononimo*/
 LEFT|RIGHT|UP|DOWN|LEFT_UP|LEFT_DOWN|RIGHT_UP|RIGHT_DOWN,

 LEFT_UP|LEFT_DOWN|RIGHT_UP|RIGHT_DOWN,

 LEFT|RIGHT_UP|RIGHT_DOWN,
 RIGHT|LEFT_UP|LEFT_DOWN,
 UP|LEFT_DOWN|RIGHT_DOWN,
 DOWN|LEFT_UP|RIGHT_UP,
 LEFT|RIGHT_UP,
 RIGHT|LEFT_UP,
 UP|LEFT_DOWN,
 DOWN|LEFT_UP,
 LEFT|RIGHT_DOWN,
 RIGHT|LEFT_DOWN,
 UP|RIGHT_DOWN,
 DOWN|RIGHT_UP,

#if 0
/* These needed for hexonimoes*/
 LEFT,
 RIGHT,
 UP,
 DOWN,
 LEFT_DOWN|RIGHT_UP|RIGHT_DOWN,
 LEFT_UP|RIGHT_UP|RIGHT_DOWN,
 LEFT_UP|LEFT_DOWN|RIGHT_DOWN,
 LEFT_UP|LEFT_DOWN|RIGHT_UP,
#endif

 LEFT|UP|RIGHT_DOWN,
 LEFT|DOWN|RIGHT_UP,
 RIGHT|UP|LEFT_DOWN,
 RIGHT|DOWN|LEFT_UP,
 LEFT|UP,
 LEFT|DOWN,
 RIGHT|UP,
 RIGHT|DOWN,

 LEFT|RIGHT,
 UP|DOWN,

 RIGHT|UP|DOWN,
 LEFT|UP|DOWN,
 LEFT|RIGHT|DOWN,
 LEFT|RIGHT|UP,

 -1
};

static trisstruct *triss = NULL;

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

static int
image_needed(int n) {
  int i;

  for (i=0;images_needed[i]!=-1;i++)
    if (n == images_needed[i])
      return 1;
  return 0;
}

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

static Bool
readPolyominoes(void)
{
  int   size, n, index = 0, sum = 0;
  int   counter, start_counter;
  int   polyomino, kinds, i, j, bonustype;

  for (bonustype = 0; bonustype < 2; bonustype++) {
    counter = 0, start_counter = 0;
    polytris[bonustype].mode.number = start_ominoes[bonustype];
    for (kinds = 0; kinds < area_kinds[bonustype]; kinds++) {
      size = ominos[index++];
      if (size > omino_squares[bonustype]) {
        (void) printf("tetris corruption: size %d\n", size);
        return False;
      }
      n = ominos[index++];
      if (sum > max_ominoes[bonustype]) {
        (void) printf("tetris corruption: n %d\n", n);
        return False;
      }
      for (polyomino = 0; polyomino < n; polyomino++) {
        if (polyomino + counter < max_ominoes[bonustype]) {
          sum = polyomino + counter;
        } else {
          (void) printf("tetris corruption: bonustype %d, polyomino %d, counter %d\n",
            bonustype, polyomino, counter);
          return False;
        }
        if (polyomino != ominos[index]) {
          (void) printf("tetris corruption: polyomino %d, ominos[index] %d\n",
            polyomino, ominos[index]);
          return False;
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
            return False;
          }
        }
        polytris[bonustype].polyomino[sum].size = size;
        if ((polytris[bonustype].polyomino[sum].shape = (int *) malloc(size *
            size * sizeof(int))) == NULL) {
          (void) printf("tetris out of memory\n");
          return False;
	}
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
  return True;
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
setColour(ModeInfo * mi, int cid /* , int exor */) 
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

  setColour(mi, cid /* , 0 */);
  if (tp->pixelmode)
    XFillRectangle(display, window, tp->gc,
      tp->xb + col * tp->xs, tp->yb + row * tp->ys, tp->xs, tp->ys);
  else {
    if (pmid < 0 || pmid >= 256)
      (void) printf("pmid = %d\n", pmid);
/*-
 * PURIFY on SunOS4 and on Solaris 2 reports a 120 byte memory leak on
 * the next line */
    (void) XPutImage(display, window, tp->gc, tp->images[pmid], 0, 0,
      tp->xb + col * tp->xs, tp->yb + row * tp->ys,
      tp->xs, tp->ys);
  }
}

static void
xorSquare(ModeInfo * mi, int i, int j)
{
  Display    *display = MI_DISPLAY(mi);
  Window      window = MI_WINDOW(mi);
  trisstruct *tp = &triss[MI_SCREEN(mi)];

  XSetFunction(display, tp->gc, GXxor);
  XSetBackground(display, tp->gc, 0);
  XSetForeground(display, tp->gc, 0xFFFFFF);
  XFillRectangle(display, window, tp->gc,
    tp->xb + i * tp->xs, tp->yb + j * tp->ys,
    tp->xs, tp->ys);
  XSetFunction(display, tp->gc, GXcopy);
  XSetBackground(display, tp->gc, tp->blackpixel);
  XSetForeground(display, tp->gc, tp->whitepixel);
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
  {
    XSetFillStyle(display, gc, FillOpaqueStippled);
    XSetStipple(display, gc, tp->graypix);
  }
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
  {
    XSetFillStyle(display, gc, FillSolid);
  }
}

static void
clearTrapazoid(ModeInfo * mi, int i, int j)
{
  Display    *display = MI_DISPLAY(mi);
  trisstruct *tp = &triss[MI_SCREEN(mi)];

  XSetForeground(display, tp->gc, tp->blackpixel);
  drawWallSquare(mi, tp->gc, i, j); /* pmid not used yet */
}

static void
drawTrapazoid(ModeInfo * mi, /* int pmid, */ int cid, int i, int j)
{
  trisstruct *tp = &triss[MI_SCREEN(mi)];

  setColour(mi, cid /* , 0 */ );
  drawWallSquare(mi, tp->gc, i, j); /* pmid not used yet */
}

static void
freezeTrapazoid(ModeInfo * mi, int i, int j)
{
  Display    *display = MI_DISPLAY(mi);
  trisstruct *tp = &triss[MI_SCREEN(mi)];

  XSetFunction(display, tp->gc, GXxor);
  XSetBackground(display, tp->gc, 0);
  XSetForeground(display, tp->gc, 0xFFFFFF);
  drawWallSquare(mi, tp->gc, i, j); /* pmid not used yet */
  XSetFunction(display, tp->gc, GXcopy);
  XSetBackground(display, tp->gc, tp->blackpixel);
  XSetForeground(display, tp->gc, tp->whitepixel);
}

static int
checkWall(int pmid, int base_i, int base_j)
{
  if (pmid < 0)
    return pmid;
  /* Check if next to well wall */
  if (base_i == 0)
    pmid = CHECKLEFT((unsigned int) pmid);
  if (base_j == 0)
    pmid = CHECKUP((unsigned int) pmid);
  if (base_i == WELL_WIDTH - 1)
    pmid = CHECKRIGHT((unsigned int) pmid);
  if (base_j == WELL_WIDTH - 1)
    pmid = CHECKDOWN((unsigned int) pmid);
  return pmid;
}

static int
checkSides(int pmid, int i)
{
  if (pmid < 0)
    return pmid;
  if (i == 0 || i == 3 * WELL_WIDTH - 1)
    pmid = CHECKLEFT((unsigned int) pmid);
  else if (i == WELL_WIDTH - 1 || i == 2 * WELL_WIDTH)
    pmid = CHECKRIGHT((unsigned int) pmid);
  else if (i == WELL_WIDTH || i == 4 * WELL_WIDTH - 1)
    pmid = CHECKUP((unsigned int) pmid);
  else if (i == 2 * WELL_WIDTH - 1 || i == 3 * WELL_WIDTH)
    pmid = CHECKDOWN((unsigned int) pmid);
  return pmid;
}

static int
checkBottom(int pmid, int i, int j)
{
  if (pmid < 0 || j != WELL_DEPTH - 1)
    return pmid;
  switch (i / WELL_WIDTH) {
  case 0:
    return CHECKDOWN((unsigned int) pmid);
  case 1:
    return CHECKLEFT((unsigned int) pmid);
  case 2:
    return CHECKUP((unsigned int) pmid);
  case 3:
    return CHECKRIGHT((unsigned int) pmid);
  }
  return EMPTY;
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
              tp->base[base_y][base_x].pmid =
                checkWall(tp->base[base_y][base_x].pmid, base_x, base_y);
              tp->wall[l][k].pmid = EMPTY;
              clearTrapazoid(mi, k, l);
              drawSquare(mi, tp->base[base_y][base_x].pmid,
                tp->base[base_y][base_x].cid, base_x, base_y);
            } else {
              tp->wall[l + j][k] = tp->wall[l][k];
              tp->wall[l + j][k].pmid =
                checkBottom(tp->wall[l + j][k].pmid, k, l + j);
              tp->wall[l][k].pmid = EMPTY;
              clearTrapazoid(mi, k, l);
              drawTrapazoid(mi, /* tp->wall[l][k].pmid, */ tp->wall[l][k].cid,
                k, l + j);
            }
            XFlush(display);
          }
    }
  }
}

static void
freezeWall(ModeInfo * mi, int w)
{
  int i, j;

  for (j = 0; j < WELL_DEPTH; j++)
    for (i = 0; i < WELL_WIDTH; i++) {
      freezeTrapazoid(mi, w * WELL_WIDTH + i, j);
    }
  XFlush(MI_DISPLAY(mi));
}

static Bool
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

static void
checkFreeze(ModeInfo *mi)
{
  trisstruct *tp = &triss[MI_SCREEN(mi)];
  int w;

  for (w = 0; w < MAX_SIDES; w++) {
    if (!tp->frozen_wall[w]) {
      dropWall(mi, w);
    } else {
      tp->frozen_wall[w]--;
      if (!tp->frozen_wall[w]) {
        freezeWall(mi, w);
        dropWall(mi, w);
      }
    }
  }
}

static int
curPixmap(trisstruct *tp, int i, int j)
{
  int pmid = 0;

  if (ARR(i,j) != ARR(i-1,j))
    pmid |= LEFT;
  if (ARR(i,j) != ARR(i,j+1))
    pmid |= DOWN;
  if (ARR(i,j) != ARR(i+1,j))
    pmid |= RIGHT;
  if (ARR(i,j) != ARR(i,j-1))
    pmid |= UP;
  if (ARR(i,j) != ARR(i-1,j+1))
    pmid |= LEFT_DOWN;
  if (ARR(i,j) != ARR(i+1,j+1))
    pmid |= RIGHT_DOWN;
  if (ARR(i,j) != ARR(i+1,j-1))
    pmid |= RIGHT_UP;
  if (ARR(i,j) != ARR(i-1,j-1))
    pmid |= LEFT_UP;
  return pmid;
}

static int
rotateIndex(int pmid, int i)
{
  int wallRotation, pm1, pm0;

  if (pmid == EMPTY)
    return EMPTY;
  wallRotation = (MAX_SIDES - i / WELL_WIDTH) % MAX_SIDES;
  pm1 = (0xF0 & pmid) << wallRotation;
  pm0 = (0xF & pmid) << wallRotation;
  pm1 = (0xF0 & pm1) | ((pm1 & 0xF00) >> MAX_SIDES);
  pm0 = (0xF & pm0) | ((pm0 & 0xF0) >> MAX_SIDES);
  return (pm1 | pm0);
}

static void
drawBox(ModeInfo * mi, int pmid, int cid, int i, int j)
{
  if (j < WELL_DEPTH) {
    drawTrapazoid(mi, /* pmid, */ cid, i, j);
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
  XFillRectangle(display, window, tp->gc,
    tp->xb + col * tp->xs, tp->yb + row * tp->ys, tp->xs, tp->ys);
}

static void
drawPolyomino(ModeInfo * mi)
{
  trisstruct *tp = &triss[MI_SCREEN(mi)];
  int   i, j;

  if (tp->well) {
    int xi, yj, base_i, base_j;
    int temp_base[WELL_WIDTH][WELL_WIDTH];

    /* Initialize */
    for (i = 0; i < WELL_WIDTH; i++)
      for (j = 0; j < WELL_WIDTH; j++)
        temp_base[j][i] = 0;
    /* Find pieces and overlap them if necessary */
    for (i = 0; i < tp->curPolyomino.size; i++) {
      for (j = 0; j < tp->curPolyomino.size; j++) {
        if (polytris[(int) tp->bonusNow].polyomino
                    [tp->curPolyomino.polyomino_number].shape
                    [j * tp->curPolyomino.size + i]) {
          int xi, yj;

          xi = (tp->curPolyomino.xpos + i) % WELL_PERIMETER;
          yj = tp->curPolyomino.ypos + j;
          if (yj >= WELL_DEPTH) {
            wall_to_base(&base_i, &base_j, xi, yj);
            xi = checkWall(rotateIndex(curPixmap(tp, i, j), xi),
              base_i, base_j);
            if (!temp_base[base_j][base_i])
              temp_base[base_j][base_i] = xi;
            else
              temp_base[base_j][base_i] &= xi;
          }
        }
      }
      /* Now draw */
      for (i = 0; i < tp->curPolyomino.size; i++)
        for (j = 0; j < tp->curPolyomino.size; j++)
          if (polytris[(int) tp->bonusNow].polyomino
                      [tp->curPolyomino.polyomino_number].shape
                      [j * tp->curPolyomino.size + i]) {
            xi = (tp->curPolyomino.xpos + i) % WELL_PERIMETER;
            yj = tp->curPolyomino.ypos + j;
            if (yj >= WELL_DEPTH) {
              wall_to_base(&base_i, &base_j, xi, yj);
              if (temp_base[base_j][base_i]) {
                drawSquare(mi, temp_base[base_j][base_i],
                  tp->curPolyomino.color_number, base_i, base_j);
                temp_base[base_j][base_i] = 0;
              }
            } else if (yj >= 0) {
              drawTrapazoid(mi, /* tp->curPolyomino.pmid, */
                tp->curPolyomino.color_number, xi, yj);
            }
          }
    }
  } else {
    for (i = 0; i < tp->curPolyomino.size; i++) {
      for (j = 0; j < tp->curPolyomino.size; j++) {
        if (polytris[(int) tp->bonusNow].polyomino
                    [tp->curPolyomino.polyomino_number].shape
                    [j * tp->curPolyomino.size + i]) {
          drawSquare(mi, curPixmap(tp, i, j),
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

  if (tp->well) {
    int xi, yj, base_i, base_j;
    int temp_base[WELL_WIDTH][WELL_WIDTH];

    /* Initialize */
    for (i = 0; i < WELL_WIDTH; i++)
      for (j = 0; j < WELL_WIDTH; j++)
        temp_base[j][i] = 0;
    /* Find pieces and overlap them if necessary */
    for (i = 0; i < tp->curPolyomino.size; i++)
      for (j = 0; j < tp->curPolyomino.size; j++)
        if (polytris[(int) tp->bonusNow].polyomino
                    [tp->curPolyomino.polyomino_number].shape
                    [j * tp->curPolyomino.size + i]) {
          xi = (tp->curPolyomino.xpos + i) % WELL_PERIMETER;
          yj = tp->curPolyomino.ypos + j;
          if (yj >= WELL_DEPTH) {
            wall_to_base(&base_i, &base_j, xi, yj);
            xi = checkWall(rotateIndex(curPixmap(tp, i, j), xi),
              base_i, base_j);
            if (!temp_base[base_j][base_i])
              temp_base[base_j][base_i] = xi;
            else
              temp_base[base_j][base_i] &= xi;
           }
          }
    /* Now draw */
    for (i = 0; i < tp->curPolyomino.size; i++)
      for (j = 0; j < tp->curPolyomino.size; j++)
        if (polytris[(int) tp->bonusNow].polyomino
                    [tp->curPolyomino.polyomino_number].shape
                    [j * tp->curPolyomino.size + i]) {
          xi = (tp->curPolyomino.xpos + i) % WELL_PERIMETER;
          yj = tp->curPolyomino.ypos + j;
          if (yj >= WELL_DEPTH) {
            wall_to_base(&base_i, &base_j, xi, yj);
            if (temp_base[base_j][base_i] > 0) {
              drawSquare(mi, temp_base[base_j][base_i],
                tp->curPolyomino.color_number, base_i, base_j);
               /* negate for erase */
               temp_base[base_j][base_i] = -temp_base[base_j][base_i];
             }
           } else if (yj >= 0) {
              drawTrapazoid(mi, /* tp->curPolyomino.pmid, */
                tp->curPolyomino.color_number, xi, yj);
            }
          }
    /* Erase */
    for (i = 0; i < old->size; i++)
      for (j = 0; j < old->size; j++) {
        xi = (old->xpos + i) % WELL_PERIMETER;
        yj = old->ypos + j;
        ox = (xi - tp->curPolyomino.xpos + WELL_PERIMETER) % WELL_PERIMETER;
        oy = yj - tp->curPolyomino.ypos;
        if (yj >= WELL_DEPTH) {
          wall_to_base(&base_i, &base_j, xi, yj);
          if (polytris[(int) tp->bonusNow].polyomino
                      [old->polyomino_number].shape
                      [j * old->size + i] &&
            ((ox >= tp->curPolyomino.size) || (oy < 0) ||
             (oy >= tp->curPolyomino.size) || !temp_base[base_j][base_i]))
            clearSquare(mi, base_i, base_j);
        } else if (yj >= 0) {
           if (polytris[(int) tp->bonusNow].polyomino
                       [old->polyomino_number].shape
                       [j * old->size + i] &&
             ((ox >= tp->curPolyomino.size) || (oy < 0) ||
              (oy >= tp->curPolyomino.size) || !polytris
		[(int) tp->bonusNow].polyomino
                [tp->curPolyomino.polyomino_number].shape
                [oy * tp->curPolyomino.size + ox]))
              clearTrapazoid(mi, xi, yj);
          }
        }
  } else {
    for (i = 0; i < tp->curPolyomino.size; i++)
      for (j = 0; j < tp->curPolyomino.size; j++)
        if ((tp->curPolyomino.ypos + j >= 0) &&
          polytris[(int) tp->bonusNow].polyomino
            [tp->curPolyomino.polyomino_number].shape[j * tp->curPolyomino.size + i]) {
          drawSquare(mi, curPixmap(tp, i, j),
            tp->curPolyomino.color_number,
            tp->curPolyomino.xpos + i, tp->curPolyomino.ypos + j);
        }
    for (i = 0; i < tp->curPolyomino.size; i++)
      for (j = 0; j < tp->curPolyomino.size; j++) {
        ox = old->xpos + i - tp->curPolyomino.xpos;
        oy = old->ypos + j - tp->curPolyomino.ypos;
        if (polytris[(int) tp->bonusNow].polyomino[old->polyomino_number].shape[j * tp->curPolyomino.size + i] &&
          ((ox < 0) || (ox >= tp->curPolyomino.size) ||
          (oy < 0) || (oy >= tp->curPolyomino.size) ||
          !polytris[(int) tp->bonusNow].polyomino
            [tp->curPolyomino.polyomino_number].shape[oy * tp->curPolyomino.size + ox])) {
          clearSquare(mi, (old->xpos + i), (old->ypos + j));
        }
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
  if (tp->well) {
    int w;

    /* pick a free spot on the rim, not all frozen else this is an infinite
       loop */
    /* tp->curPolyomino.xpos = (WELL_WIDTH - tp->curPolyomino.size) / 2; */
    tp->curPolyomino.xpos = NRAND(WELL_WIDTH - tp->curPolyomino.size + 1);
    do {
      w = NRAND(MAX_SIDES);
    } while (tp->frozen_wall[w]);
    tp->curPolyomino.xpos += w * WELL_WIDTH;
  } else {
    tp->curPolyomino.xpos = NRAND(tp->ncols - tp->curPolyomino.size + 1);
    /* tp->curPolyomino.xpos = (tp->ncols - tp->curPolyomino.size) / 2; */
  }
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
          /* Set pieces and overlap them if necessary */
          int xi, yj;

          xi = (x + i) % WELL_PERIMETER;
          yj = y + j;
          if (yj >= WELL_DEPTH) {
            int base_i, base_j;

            wall_to_base(&base_i, &base_j, xi, yj);
            xi = checkWall(rotateIndex(curPixmap(tp, i, j), xi),
              base_i, base_j);
            if (!tp->base[base_j][base_i].pmid)
              tp->base[base_j][base_i].pmid = xi;
            else
              tp->base[base_j][base_i].pmid &= xi;
            tp->base[base_j][base_i].cid = tp->curPolyomino.color_number;
          } else {
            tp->wall[yj][xi].pmid =
              checkSides(checkBottom(rotateIndex(curPixmap(tp, i,
              j), xi), xi, yj), xi);
            tp->wall[yj][xi].cid = tp->curPolyomino.color_number;
            tp->frozen_wall[xi / WELL_WIDTH] = MAX_SIDES;
          }
        } else {
          pos = (y + j) * tp->ncols + x + i;
          tp->field[pos].pmid = curPixmap(tp, i, j);
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
        if ((tp->curPolyomino.ypos + j >= WELL_DEPTH) &&
            polytris[(int) tp->bonusNow].polyomino
              [tp->curPolyomino.polyomino_number].shape[j * tp->curPolyomino.size + i])
          return True;
  return False;
}

static Bool
wallChange(ModeInfo *mi, thing_t oldthing, thing_t newthing)
{
  trisstruct *tp = &triss[MI_SCREEN(mi)];
  int             w = -1, i, j;
  int             x = tp->curPolyomino.xpos;

  for (i = 0; i < oldthing.size; i++)
    for (j = 0; j < oldthing.size; j++)
  if (polytris[(int) tp->bonusNow].polyomino[oldthing.polyomino_number].shape[j * tp->curPolyomino.size + i]) {
    if (w == -1)
      w = ((x + i) % WELL_PERIMETER) / WELL_WIDTH;
    else if (w != ((x + i) % WELL_PERIMETER) / WELL_WIDTH)
      return True;
  }
  for (i = 0; i < newthing.size; i++)
    for (j = 0; j < newthing.size; j++)
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

    case MOVE_LEFT:
      if (tp->well) {
        tp->curPolyomino.xpos = (tp->curPolyomino.xpos + WELL_PERIMETER - 1) % WELL_PERIMETER;
        if (atBaseFully(mi) || (atBasePartially(mi) && wallChange(mi, old, tp->curPolyomino)))
          skip = True;
      } else
        tp->curPolyomino.xpos--;
      break;

    case MOVE_RIGHT:
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

static void
fillLines(ModeInfo *mi)
{
  Display    *display = MI_DISPLAY(mi);
  trisstruct *tp = &triss[MI_SCREEN(mi)];
  int             i, j;

  XFlush(display);
  if (tp->well) {
    for (j = 0; j < WELL_DEPTH + WELL_WIDTH; j++)
      for (i = 0; i < WELL_PERIMETER; i++) {
         drawBox(mi, BITMAPS - 1, 
           LRAND() % polytris[(int) tp->bonusNow].mode.number,
                 i, j);
         XFlush(display);
      }
  } else {
    for (j = 0; j <= tp->nrows / 2; j++) {
      for (i = 0; i < tp->ncols; i++) {
         drawSquare(mi, BITMAPS - 1,
           LRAND() % polytris[(int) tp->bonusNow].mode.number,
                 i, j);
         drawSquare(mi, BITMAPS - 1,
           LRAND() % polytris[(int) tp->bonusNow].mode.number,
                 i, tp->nrows - j - 1);
      }
      XFlush(display);
    }
  }
}

static void
free_images(trisstruct *tp)
{
  int n;

  if (tp->images[BITMAPS - 1] != None) {
    (void) XDestroyImage(tp->images[BITMAPS - 1]);
    tp->images[BITMAPS - 1] = None;
  }
  for (n = 0; n < BITMAPS - 1; n++) {
    /* Don't bother to free duplicates */
    if (IS_UP(n) && IS_DOWN(n) && IS_LEFT(n) && IS_RIGHT(n))
      tp->images[n] = None;
    else if (IS_LEFT_UP(n) && (IS_LEFT(n) || IS_UP(n)))
      tp->images[n] = None;
    else if (IS_LEFT_DOWN(n) && (IS_LEFT(n) || IS_DOWN(n)))
      tp->images[n] = None;
    else if (IS_RIGHT_UP(n) && (IS_RIGHT(n) || IS_UP(n)))
      tp->images[n] = None;
    else if (IS_RIGHT_DOWN(n) && (IS_RIGHT(n) || IS_DOWN(n)))
      tp->images[n] = None;
    else if (tp->images[n] != None) {
      (void) XDestroyImage(tp->images[n]);
      tp->images[n] = None;
    }
  }
}

static void
free_tetris(Display *display, trisstruct *tp)
{
  ModeInfo *mi = tp->mi;

  if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
    MI_WHITE_PIXEL(mi) = tp->whitepixel;
    MI_BLACK_PIXEL(mi) = tp->blackpixel;
#ifndef STANDALONE
    MI_FG_PIXEL(mi) = tp->fg;
    MI_BG_PIXEL(mi) = tp->bg;
#endif
    if (tp->colors != NULL) {
      if (tp->ncolors && !tp->no_colors)
        free_colors(display, tp->cmap, tp->colors, tp->ncolors);
      (void) free((void *) tp->colors);
      tp->colors = NULL;
    }
    if (tp->cmap != None) {
      XFreeColormap(display, tp->cmap);
      tp->cmap = None;
    }
  }
  if (tp->gc != NULL) {
     XFreeGC(display, tp->gc);
     tp->gc = NULL;
  }
  if (tp->graypix != None) {
    XFreePixmap(display, tp->graypix);
    tp->graypix = None;
  }
  if (tp->field != NULL) {
     (void) free((void *) tp->field);
     tp->field = NULL;
  }
  free_images(tp);
}

static int
checkLines(ModeInfo *mi)
{
  trisstruct *tp = &triss[MI_SCREEN(mi)];
  Display    *display = MI_DISPLAY(mi);
  Window      window = MI_WINDOW(mi);

  if (tp->well) {
    int             lSet[2][WELL_WIDTH], nset[2];
    int             i, j, k, *tmp;

    nset[0] = nset[1] = 0;

    /* Find filled rows */
    for (j = 0; j < WELL_WIDTH; j++) {
      lSet[0][j] = 0;
      for (i = 0; i < WELL_WIDTH; i++)
        if (tp->base[j][i].pmid >= 0)
          lSet[0][j] ++;
      if (lSet[0][j] == WELL_WIDTH)
        nset[0] ++;
    }
    /* Find filled columns */
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
        for (k = 0; k < ((NUM_FLASHES / (nset[0] + nset[1])) / 2) * 2; k++) {
            for (j = 0; j < WELL_WIDTH; j++) {
                if (lSet[0][j] == WELL_WIDTH)
                  for (i = 0; i < WELL_WIDTH; i++)
                    xorSquare(mi, i, j);
                if (lSet[1][j] == WELL_WIDTH)
                  for (i = 0; i < WELL_WIDTH; i++)
                    xorSquare(mi, j, i);
            }
            XFlush(display);
        }

        for (j = 0; j < WELL_WIDTH; j++)
          for (i = 0; i < WELL_WIDTH; i++)
            temp_base[j][i] = tp->base[j][i];
        if (nset[0]) {
          for (j = 0; j < WELL_WIDTH / 2; j++)
            if (lSet[0][j] == WELL_WIDTH)
              for (i = 0; i < WELL_WIDTH; i++) {
                for (k = j; k > 0; k--)
                  temp_base[k][i] = temp_base[k - 1][i];
                temp_base[0][i].pmid = EMPTY;
                if (j > 0) {
                  tmp = &temp_base[j][i].pmid;
                  if (*tmp >= 0)
                     *tmp = CHECKDOWN((unsigned int) *tmp);
                }
                tmp = &temp_base[j + 1][i].pmid;
                if (*tmp >= 0)
                   *tmp = CHECKUP((unsigned int) *tmp);
              }
          for (j = WELL_WIDTH - 1; j >= WELL_WIDTH / 2; j--)
            if (lSet[0][j] == WELL_WIDTH)
              for (i = 0; i < WELL_WIDTH; i++) {
                for (k = j; k < WELL_WIDTH - 1; k++)
                  temp_base[k][i] = temp_base[k + 1][i];
                temp_base[WELL_WIDTH - 1][i].pmid = EMPTY;
                if (j < WELL_WIDTH - 1) {
                  tmp = &temp_base[j][i].pmid;
                  if (*tmp >= 0)
                     *tmp = CHECKUP((unsigned int) *tmp);
                }
                tmp = &temp_base[j - 1][i].pmid;
                if (*tmp >= 0)
                   *tmp = CHECKDOWN((unsigned int) *tmp);
              }
        }
        if (nset[1]) {
          for (i = 0; i < WELL_WIDTH / 2; i++)
            if (lSet[1][i] == WELL_WIDTH)
              for (j = 0; j < WELL_WIDTH; j++) {
                for (k = i; k > 0; k--)
                  temp_base[j][k] = temp_base[j][k - 1];
                temp_base[j][0].pmid = EMPTY;
                if (i > 0) {
                  tmp = &temp_base[j][i].pmid;
                  if (*tmp >= 0)
                     *tmp = CHECKRIGHT((unsigned int) *tmp);
                }
                tmp = &temp_base[j][i + 1].pmid;
                if (*tmp >= 0)
                   *tmp = CHECKLEFT((unsigned int) *tmp);
              }
          for (i = WELL_WIDTH - 1; i >= WELL_WIDTH / 2; i--)
            if (lSet[1][i] == WELL_WIDTH)
              for (j = 0; j < WELL_WIDTH; j++) {
                for (k = i; k < WELL_WIDTH - 1; k++)
                  temp_base[j][k] = temp_base[j][k + 1];
                temp_base[j][WELL_WIDTH - 1].pmid = EMPTY;
                if (i < WELL_WIDTH - 1) {
                  tmp = &temp_base[j][i].pmid;
                  if (*tmp >= 0)
                     *tmp = CHECKLEFT((unsigned int) *tmp);
                }
                tmp = &temp_base[j][i - 1].pmid;
                if (*tmp >= 0)
                   *tmp = CHECKRIGHT((unsigned int) *tmp);
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
            }
       XFlush(display);
    }

    return (nset[0] + nset[1]);
  } else {
    int      *lSet, nset = 0;
    int       i, j, y;

    if ((lSet = (int *) calloc(tp->nrows, sizeof (int))) == NULL) {
        free_tetris(display, tp);
        return -1; /* error */
    }
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
            for (i = 0; i < tp->ncols; i++)
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
          XSetForeground(display, tp->gc, tp->blackpixel);
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

static Bool
create_an_image(ModeInfo *mi, trisstruct *tp, int n)
{
  int x, y;
  unsigned char * data;

  if ((data = (unsigned char *) malloc(sizeof(char)*(tp->ys*ROUND8(tp->ys)/8))) == NULL) {
    return False;  /* no biggie, will not use images */
  }

  for (y=0;y<tp->ys;y++) for (x=0;x<tp->ys;x++) {
     if (!tp->use3D) {
#ifdef SMALL_BELLYBUTTON 
        if (x >= tp->ys / 2 && x <= tp->ys / 2 + 1 &&
            y >= tp->ys / 2 && y <= tp->ys / 2 + 1)
          NOTHALFBIT(x,y)
        else
#endif
          HALFBIT(x,y)
      } else if ((x>=y && x<=tp->ys-y-1 && IS_UP(n))
          || (x<=y && x<=tp->ys-y-1 && y<tp->ys/2 && !IS_LEFT(n))
          || (x>=y && x>=tp->ys-y-1 && y<tp->ys/2 && !IS_RIGHT(n)))
        SETBIT(x,y)
      else if ((x<=y && x<=tp->ys-y-1 && IS_LEFT(n))
          || (x>=y && x<=tp->ys-y-1 && x<tp->ys/2 && !IS_UP(n))
          || (x<=y && x>=tp->ys-y-1 && x<tp->ys/2 && !IS_DOWN(n)))
        TWOTHIRDSBIT(x,y)
      else if ((x>=y && x>=tp->ys-y-1 && IS_RIGHT(n))
          || (x>=y && x<=tp->ys-y-1 && x>=tp->ys/2 && !IS_UP(n))
          || (x<=y && x>=tp->ys-y-1 && x>=tp->ys/2 && !IS_DOWN(n)))
        HALFBIT(x,y)
      else if ((x<=y && x>=tp->ys-y-1 && IS_DOWN(n))
          || (x<=y && x<=tp->ys-y-1 && y>=tp->ys/2 && !IS_LEFT(n))
          || (x>=y && x>=tp->ys-y-1 && y>=tp->ys/2 && !IS_RIGHT(n)))
        THIRDBIT(x,y)
  }

  if (IS_LEFT(n))
    for (y=0;y<tp->ys;y++) for (x=G;x<G+T;x++)
      SETBIT(x,y)
  if (IS_RIGHT(n))
    for (y=0;y<tp->ys;y++) for (x=G;x<G+T;x++)
      SETBIT(tp->ys-1-x,y)
  if (IS_UP(n))
    for (x=0;x<tp->ys;x++) for (y=G;y<G+T;y++)
      SETBIT(x,y)
  if (IS_DOWN(n))
    for (x=0;x<tp->ys;x++) for (y=G;y<G+T;y++)
      SETBIT(x,tp->ys-1-y)
  if (IS_LEFT(n))
    for (y=0;y<tp->ys;y++) for (x=0;x<G;x++)
      RESBIT(x,y)
  if (IS_RIGHT(n))
    for (y=0;y<tp->ys;y++) for (x=0;x<G;x++)
      RESBIT(tp->ys-1-x,y)
  if (IS_UP(n))
    for (x=0;x<tp->ys;x++) for (y=0;y<G;y++)
      RESBIT(x,y)
  if (IS_DOWN(n))
    for (x=0;x<tp->ys;x++) for (y=0;y<G;y++)
      RESBIT(x,tp->ys-1-y)

  if (IS_LEFT(n) && IS_UP(n))
    for (x=G;x<=G+R;x++)
      for (y=G;y<=R+2*G-x;y++) {
        if (x+y>R+2*G-RT)
          SETBIT(x,y)
        else
          RESBIT(x,y)
      }
  if (IS_LEFT(n) && IS_DOWN(n))
    for (x=G;x<=G+R;x++)
      for (y=G;y<=R+2*G-x;y++) {
        if (x+y>R+2*G-RT)
          SETBIT(x,tp->ys-1-y)
        else
          RESBIT(x,tp->ys-1-y)
      }
  if (IS_RIGHT(n) && IS_UP(n))
    for (x=G;x<=G+R;x++)
      for (y=G;y<=R+2*G-x;y++) {
        if (x+y>R+2*G-RT)
          SETBIT(tp->ys-1-x,y)
        else
          RESBIT(tp->ys-1-x,y)
      }
  if (IS_RIGHT(n) && IS_DOWN(n))
    for (x=G;x<=G+R;x++)
      for (y=G;y<=R+2*G-x;y++) {
        if (x+y>R+2*G-RT)
          SETBIT(tp->ys-1-x,tp->ys-1-y)
        else
          RESBIT(tp->ys-1-x,tp->ys-1-y)
      }

  if (!IS_LEFT(n) && !IS_UP(n) && IS_LEFT_UP(n)) {
    for (x=0;x<G;x++) for(y=0;y<G;y++)
      RESBIT(x,y)
    for (x=G;x<G+T;x++) for(y=0;y<G;y++)
      SETBIT(x,y)
    for (x=0;x<G+T;x++) for(y=G;y<G+T;y++)
      SETBIT(x,y)
  }
  if (!IS_LEFT(n) && !IS_DOWN(n) && IS_LEFT_DOWN(n)) {
    for (x=0;x<G;x++) for(y=0;y<G;y++)
      RESBIT(x,tp->ys-1-y)
    for (x=G;x<G+T;x++) for(y=0;y<G;y++)
      SETBIT(x,tp->ys-1-y)
    for (x=0;x<G+T;x++) for(y=G;y<G+T;y++)
      SETBIT(x,tp->ys-1-y)
  }
  if (!IS_RIGHT(n) && !IS_UP(n) && IS_RIGHT_UP(n)) {
    for (x=0;x<G;x++) for(y=0;y<G;y++)
      RESBIT(tp->ys-1-x,y)
    for (x=G;x<G+T;x++) for(y=0;y<G;y++)
      SETBIT(tp->ys-1-x,y)
    for (x=0;x<G+T;x++) for(y=G;y<G+T;y++)
      SETBIT(tp->ys-1-x,y)
  }
  if (!IS_RIGHT(n) && !IS_DOWN(n) && IS_RIGHT_DOWN(n)) {
    for (x=0;x<G;x++) for(y=0;y<G;y++)
      RESBIT(tp->ys-1-x,tp->ys-1-y)
    for (x=G;x<G+T;x++) for(y=0;y<G;y++)
      SETBIT(tp->ys-1-x,tp->ys-1-y)
    for (x=0;x<G+T;x++) for(y=G;y<G+T;y++)
      SETBIT(tp->ys-1-x,tp->ys-1-y)
  }
#ifdef LARGE_BELLYBUTTON 
  if (!tp->use3D) {
    if (!IS_LEFT(n) && !IS_UP(n) && !IS_LEFT_UP(n))
      for (x=0;x<G+T;x++) for(y=0;y<G+T;y++)
        SETBIT(x,y)
    if (!IS_LEFT(n) && !IS_DOWN(n) && !IS_LEFT_DOWN(n))
      for (x=0;x<G+T;x++) for(y=tp->ys-G-T;y<tp->ys;y++)
        SETBIT(x,y)
    if (!IS_RIGHT(n) && !IS_UP(n) && !IS_RIGHT_UP(n))
      for (x=tp->ys-G-T;x<tp->ys;x++) for(y=0;y<G+T;y++)
        SETBIT(x,y)
    if (!IS_RIGHT(n) && !IS_DOWN(n) && !IS_RIGHT_DOWN(n))
      for (x=tp->ys-G-T;x<tp->ys;x++) for(y=tp->ys-G-T;y<tp->ys;y++)
        SETBIT(x,y)
  } else
#else
  if (tp->use3D)
#endif
  {
    if (!IS_LEFT(n) && !IS_UP(n) && !IS_LEFT_UP(n))
      for (x=0;x<tp->ys/2-RR;x++) for(y=0;y<tp->ys/2-RR;y++)
        THREEQUARTERSBIT(x,y)
    if (!IS_LEFT(n) && !IS_DOWN(n) && !IS_LEFT_DOWN(n))
      for (x=0;x<tp->ys/2-RR;x++) for(y=tp->ys/2+RR;y<tp->ys;y++)
        THREEQUARTERSBIT(x,y)
    if (!IS_RIGHT(n) && !IS_UP(n) && !IS_RIGHT_UP(n))
      for (x=tp->ys/2+RR;x<tp->ys;x++) for(y=0;y<tp->ys/2-RR;y++)
        THREEQUARTERSBIT(x,y)
    if (!IS_RIGHT(n) && !IS_DOWN(n) && !IS_RIGHT_DOWN(n))
      for (x=tp->ys/2+RR;x<tp->ys;x++) for(y=tp->ys/2+RR;y<tp->ys;y++)
        THREEQUARTERSBIT(x,y)
  }

  if ((tp->images[n] = XCreateImage(MI_DISPLAY(mi), MI_VISUAL(mi),
           1, XYBitmap, 0, (char *) data, tp->xs, tp->ys, 8, 0)) == None) {
     (void) free((void *) data);
     return False;
  }
  tp->images[n]->byte_order = MSBFirst;
  tp->images[n]->bitmap_unit = 8;
  tp->images[n]->bitmap_bit_order = LSBFirst;
  return True;
}

static Bool
create_images(ModeInfo *mi, trisstruct *tp)
{
  int n;

  (void) create_an_image(mi, tp, BITMAPS - 1);
  for (n = 0; n < BITMAPS - 1; n++) {
    /* Avoid duplication of identical images. */
    if (IS_UP(n) && IS_DOWN(n) && IS_LEFT(n) && IS_RIGHT(n))
      tp->images[n] = tp->images[BITMAPS - 1];
    else if (IS_LEFT_UP(n) && (IS_LEFT(n) || IS_UP(n)))
      tp->images[n] = tp->images[n & ~LEFT_UP];
    else if (IS_LEFT_DOWN(n) && (IS_LEFT(n) || IS_DOWN(n)))
      tp->images[n] = tp->images[n & ~LEFT_DOWN];
    else if (IS_RIGHT_UP(n) && (IS_RIGHT(n) || IS_UP(n)))
      tp->images[n] = tp->images[n & ~RIGHT_UP];
    else if (IS_RIGHT_DOWN(n) && (IS_RIGHT(n) || IS_DOWN(n)))
      tp->images[n] = tp->images[n & ~RIGHT_DOWN];
    else if (image_needed(n)) {
      if (!create_an_image(mi, tp, n))
        return False;
    }
  }
  return True;
}

void
refresh_tetris(ModeInfo * mi)
{
  trisstruct *tp;

  if (triss == NULL)
    return;
  tp = &triss[MI_SCREEN(mi)];
  if (!tp->well && tp->field == NULL)
    return;

  if (!tp->painted)
    return;
  MI_CLEARWINDOW(mi);
  if (tp->well) {
    int i, j;

    for (i = 0; i < WELL_PERIMETER; i++) {
      for (j = 0; j < WELL_DEPTH; j++) {
        if (tp->wall[j][i].pmid >= 0) {
          drawTrapazoid(mi, /*  tp->wall[j][i].pmid, */ tp->wall[j][i].cid, i, j);
        }
        if (tp->frozen_wall[i / WELL_WIDTH]) {
          freezeTrapazoid(mi, i, j);
        }
      }
    }
    for (i = 0; i < WELL_WIDTH; i++)
      for (j = 0; j < WELL_WIDTH; j++)
        if (tp->base[j][i].pmid >= 0)
          drawSquare(mi, tp->base[j][i].pmid, tp->base[j][i].cid, i, j);
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
  int bonustype, number;

  if (triss != NULL) {
    int   screen;

    for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
       free_tetris(MI_DISPLAY(mi), &triss[screen]);
    (void) free((void *) triss);
    triss = NULL;
    for (bonustype = 0; bonustype < 2; bonustype++) {
      if (polytris[bonustype].polyomino != NULL) {
        for (number = 0; number < max_ominoes[bonustype]; number++) {
          if (polytris[bonustype].polyomino[number].shape != NULL)
            (void) free((void *) polytris[bonustype].polyomino[number].shape);
        }
        (void) free((void *) polytris[bonustype].polyomino);
        polytris[bonustype].polyomino = NULL;
      }
      if (polytris[bonustype].mode.start != NULL) {
        (void) free((void *) polytris[bonustype].mode.start);
        polytris[bonustype].mode.start = NULL;
      }
    }
  }
}

void
change_tetris(ModeInfo * mi) {
  trisstruct *tp;

  if (triss == NULL)
    return;
  tp = &triss[MI_SCREEN(mi)];
  if (!tp->well && tp->field == NULL)
    return;

  tryMove(mi, ROTATE);
}

void
init_tetris(ModeInfo * mi) {
  Display *   display = MI_DISPLAY(mi);
  Window      window = MI_WINDOW(mi);
  int size = MI_SIZE(mi);
  int   i, j, bonustype;
  trisstruct *tp;

  if (triss == NULL) {
    if ((triss = (trisstruct *) calloc(MI_NUM_SCREENS(mi),
               sizeof (trisstruct))) == NULL)
      return;
    for (bonustype = 0; bonustype < 2; bonustype++) {
      if (((polytris[bonustype].mode.start = (int *) malloc(start_ominoes[bonustype] *
            sizeof(int))) == NULL) ||
          ((polytris[bonustype].polyomino = (Polyominoes *) malloc(max_ominoes[bonustype] *
            sizeof(Polyominoes))) == NULL)) {
        release_tetris(mi);
        return;
      }
    }
    if (!readPolyominoes()) {
      release_tetris(mi);
      return;
    }
#ifdef DEBUG
    writePolyominoes(tp);
#endif
  }
   tp = &triss[MI_SCREEN(mi)];
  tp->mi = mi;

  if (MI_IS_FULLRANDOM(mi)) {
    tp->bonus = (Bool) (LRAND() & 1);
    tp->well = !(NRAND(3));
    tp->use3D = (Bool) (NRAND(4));
  } else {
    tp->bonus = bonus;
    tp->well = well;
    tp->use3D = !plain;
  }

  if (tp->gc == None) {
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
      if ((tp->cmap = XCreateColormap(display, window,
          MI_VISUAL(mi), AllocNone)) == None) {
        free_tetris(display, tp);
        return;
      }
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
      tp->colors = NULL;
      tp->ncolors = 0;
    }
    if ((tp->gc = XCreateGC(display, MI_WINDOW(mi),
        (unsigned long) 0, (XGCValues *) NULL)) == None) {
      free_tetris(display, tp);
      return;
    }
#if 0
    {
      XGCValues       gcv;
      gcv.function = GXxor;
      gcv.foreground = tp->fg;
      gcv.background = tp->bg;
    }
#endif
  }
  MI_CLEARWINDOW(mi);
  tp->painted = False;

  tp->width = MI_WIDTH(mi);
  tp->height = MI_HEIGHT(mi);

  free_images(tp);
  if (tp->width < 2 + 2 * tp->well)
    tp->width = 2 + 2 * tp->well;
  if (tp->height < 2 + 2 * tp->well)
    tp->height = 2 + 2 * tp->well;
  if (size >= MAXPIXELSIZE  &&
        (MINGRIDSIZE * size < tp->width && MINGRIDSIZE * size < tp->height)) {
      if (!tp->well) {
        tp->xs = size;
        tp->ys = size;
      } else {
        tp->xs = tp->ys = MAX(MINSIZE, MIN(tp->width, tp->height) /
            ((tp->well) ? 3 * MINGRIDSIZE : MINGRIDSIZE));
      }
  } else {
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
  if (tp->ys >= MAXPIXELSIZE) {
    tp->xs = tp->ys = (tp->ys / 12) * 12;
    if (!create_images(mi, tp)) {
      free_images(tp);
      tp->pixelmode = True;
    } else
      tp->pixelmode = False;
  } else
    tp->pixelmode = True;
  if (tp->well) {
    if ((tp->graypix = XCreateBitmapFromData(display, window,
         (char *) gray1_bits, gray1_width, gray1_height)) == None) {
      free_tetris(display, tp);
      return;
    }
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
        tp->base[j][i].cid = BITMAPS - 1;
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
    if (tp->field != NULL)
      (void) free((void *) tp->field);
    if ((tp->field = (fieldstruct *) malloc(tp->ncols * tp->nrows *
        sizeof (fieldstruct))) == NULL) {
      free_tetris(display, tp);
      return;
    }
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
    if (tp->colors != NULL) {
      if (tp->ncolors && !tp->no_colors)
        free_colors(display, tp->cmap, tp->colors, tp->ncolors);
      (void) free((void *) tp->colors);
      tp->colors = NULL;
    }
    tp->ncolors = MI_NCOLORS(mi);
    if (tp->ncolors < 2)
      tp->ncolors = 2;
    if (tp->ncolors <= 2)
      tp->mono_p = True;
    else
      tp->mono_p = False;

    if (tp->mono_p)
      tp->colors = NULL;
    else
      if ((tp->colors = (XColor *) malloc(sizeof (*tp->colors) *
          (tp->ncolors + 1))) == NULL) {
        free_tetris(display, tp);
        return;
      }
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
            display, MI_WINDOW(mi),
#else
            mi,
#endif
              tp->cmap, tp->colors, &tp->ncolors,
              True, True, &tp->cycle_p);
      else if (!(LRAND() % 2))
        make_uniform_colormap(
#if STANDALONE
            display, MI_WINDOW(mi),
#else
            mi,
#endif
                  tp->cmap, tp->colors, &tp->ncolors,
                  True, &tp->cycle_p);
      else
        make_smooth_colormap(
#if STANDALONE
            display, MI_WINDOW(mi),
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
              wall = 2.0 * (atan2((double) cy, (double) cx) + 3 * M_PI / 4) / M_PI;
            if (wall < 0.0)
              wall += 4.0;
            mousepos = (int) (wall * WELL_WIDTH);
            ominopos = tp->curPolyomino.xpos;
            diff = mousepos - ominopos;
            if (diff < 0)
              diff += WELL_WIDTH * MAX_SIDES;
            if (diff > WELL_WIDTH * (MAX_SIDES / 2))
              move = MOVE_LEFT;
            else if (diff > 0)
              move = MOVE_RIGHT;
          }
        } else {
          cx = (cx - tp->xb) / tp->xs;
          cy = (cy - tp->yb) / tp->ys;
          if (!(cx < 0 || cy < 1 || cx >= tp->ncols || cy > tp->nrows)) {
            if (cx > tp->curPolyomino.xpos +
  polytris[(int) tp->bonusNow].polyomino[tp->curPolyomino.polyomino_number].leadingEmptyWidth) {
              move = MOVE_RIGHT;
            } else if (cx < tp->curPolyomino.xpos +
  polytris[(int) tp->bonusNow].polyomino[tp->curPolyomino.polyomino_number].leadingEmptyWidth) {
              move = MOVE_LEFT;
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
        move = MOVE_LEFT;
      else
        move = MOVE_RIGHT;
    }
  }
  if ((move == DROP) || ((move == FALL) && atBottom(mi))) {
    putBox(mi);
    {
      int       lines;

      if ((lines = checkLines(mi)) < 0) {
        return False;
      }
      tp->rows += lines;
      if (tp->rows > THRESHOLD(tp->level)) {
        tp->level++;
        if (tp->bonus) {
          tp->bonusNow = True; /* No good deed goes unpunished */
        }
      }
    }
    XFlush(display);
    if (tp->well) {
      if (allFrozen(mi)) {
        gameOver(mi);
        init_tetris(mi);
        return False;
      }
    }
    newPolyomino(mi);
    if (tp->well) {
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
draw_tetris(ModeInfo * mi)
{
  Display *   display = MI_DISPLAY(mi);
  trisstruct *tp;

  if (triss == NULL)
    return;
  tp = &triss[MI_SCREEN(mi)];
  if (!tp->well && tp->field == NULL)
    return;

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
