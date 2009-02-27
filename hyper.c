#ifndef lint
static char sccsid[] = "@(#)hyper.c	2.7 95/02/21 xlockmore";
#endif
/*
 * This code derived from TI Explorer Lisp code by Joe Keane, Fritz Mueller,
 * and Jamie Zawinski.
 *
 * Copyright (c) 1992 by Jamie Zawinski
 *
 * 2-Sep-93: xlock version (David Bagley bagleyd@source.asset.com)
 * 1992:     xscreensaver version (Jamie Zawinski jwz@lucid.com)
*/

/* original copyright
 * Copyright (c) 1992 by Jamie Zawinski
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#include <math.h>
#include "xlock.h"

/* Note on port:
  This is not as efficient as it was, but maybe its more readable.
  I tried to replace the "##" stuff with:
#define IDENT(X)        X
#if defined (__STDC__) || defined (AIXV3)
#define CAT(X,Y)        X##Y
#else
#define CAT(X,Y)        IDENT(X)Y
#endif
  A X##Y##Z could then be replaced with CAT(CAT(X,Y),Z).
  Becuase of this and the fact I am using hyperstruct to keep track of
  global variables, the code got really ugly, so instead of using "CAT",
  it just uses arrays.*/

#define COMBOS 6
#define XY 0
#define XZ 1
#define YZ 2
#define XW 3
#define YW 4
#define ZW 5
#define DIMS 4
#define POINTS (DIMS*DIMS)
#define COLORS 8
#define TIMEOUT 300

typedef struct {
  int old_x, old_y;
  int new_x, new_y;
  int same_p;
} state;

typedef struct {
    int	width;
    int	height;
    int observer_z;
    int x_offset, y_offset;
    int unit_pixels;
    double cos_array[COMBOS], sin_array[COMBOS];
    double vars[DIMS][DIMS];
    state points[POINTS];
    int colors[COLORS];
    GC xor_GC;
    long startTime;
} hyperstruct;

static hyperstruct hypers[MAXSCREENS];

static void
move_line (win, state0, state1, color)
     Window win;
     state state0, state1;
     int color;
{
      hyperstruct *hp = &hypers[screen];

  if (state0.same_p && state1.same_p)
    return;
  if (mono || Scr[screen].npixels <= COLORS)
  {
      XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
      XDrawLine (dsp, win, Scr[screen].gc,
		 state0.old_x, state0.old_y, state1.old_x, state1.old_y);
      XSetForeground(dsp, Scr[screen].gc, WhitePixel(dsp, screen));
      XDrawLine (dsp, win, Scr[screen].gc,
		 state0.new_x, state0.new_y, state1.new_x, state1.new_y);
  }
  else
  {
      XSegment segments[2];
      XSetForeground(dsp, hp->xor_GC, Scr[screen].pixels[hp->colors[color]]);
      segments[0].x1 = state0.old_x; segments[0].y1 = state0.old_y;
      segments[0].x2 = state1.old_x; segments[0].y2 = state1.old_y;
      segments[1].x1 = state0.new_x; segments[1].y1 = state0.new_y;
      segments[1].x2 = state1.new_x; segments[1].y2 = state1.new_y;
      XDrawSegments (dsp, win, hp->xor_GC, segments, 2);
  }
}

void
inithyper(win)
    Window      win;
{
    XWindowAttributes xgwa;
    XGCValues gcv;
    hyperstruct *hp = &hypers[screen];
    int i, j;
    Screen *scr;
    double combos[COMBOS];

    (void) XGetWindowAttributes(dsp, win, &xgwa);
    scr = ScreenOfDisplay(dsp, screen);
    hp->width = xgwa.width;
    hp->height = xgwa.height;
    hp->unit_pixels = xgwa.width < xgwa.height ? xgwa.width : xgwa.height;
    hp->x_offset = xgwa.width / 2;
    hp->y_offset = xgwa.height / 2;

    XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
    XFillRectangle(dsp, win, Scr[screen].gc, 0, 0, hp->width, hp->height);
  
  combos[0] = 0.0;
  combos[1] = 0.01;
  combos[2] = 0.005;
  combos[3] = 0.01;
  combos[4] = 0.0;
  combos[5] = 0.0;
  for (i = 0; i < COMBOS; i++)
  {
    hp->cos_array[i] = cos(combos[i]);
    hp->sin_array[i] = sin(combos[i]);
  }
  for (i = 0; i < DIMS; i++)
    for (j = 0; j < DIMS; j++)
      if (i == j)
	hp->vars[i][j] = 1.0;
      else
	hp->vars[i][j] = 0.0;
  (void) memset(hp->points, 0, sizeof(hp->points));
  hp->observer_z = 5;

      gcv.function = GXxor;
      gcv.foreground = WhitePixelOfScreen(scr) ^ BlackPixelOfScreen(scr);
      hp->xor_GC = XCreateGC (dsp, win, GCForeground|GCFunction, &gcv);
  for (i = 0; i < COLORS; i++)
    hp->colors[i] = RAND() % Scr[screen].npixels;
  hp->startTime = seconds();
}


static int left_part[COMBOS] = {0, 0, 1, 0, 1, 2};
static int right_part[COMBOS] = {1, 2, 2, 3, 3, 3};
#define NOTSIGN(i) (((i)==0)?(-1):(1))

void
drawhyper(win)
    Window      win;
{
    hyperstruct *hp = &hypers[screen];

  double temp_mult, tmp0, tmp1;
  int i, j;
  int sign[DIMS];
      for (i = 0; i < POINTS; i++) {
	for (j = 0; j < DIMS; j++)
  	  sign[j] = NOTSIGN(i & (1<<(DIMS-1-j)));
        temp_mult = (hp->unit_pixels /
	  (((sign[0]*hp->vars[0][2]) + (sign[1]*hp->vars[1][2]) +
	    (sign[2]*hp->vars[2][2]) + (sign[3]*hp->vars[3][2]) +
	    (sign[0]*hp->vars[0][3]) + (sign[1]*hp->vars[1][3]) +
	    (sign[2]*hp->vars[2][3]) + (sign[3]*hp->vars[3][3])) -
	    hp->observer_z));
        hp->points[i].old_x = hp->points[i].new_x;
        hp->points[i].old_y = hp->points[i].new_y;
        hp->points[i].new_x =
	  ((((sign[0]*hp->vars[0][0]) + (sign[1]*hp->vars[1][0]) +
  	     (sign[2]*hp->vars[2][0]) + (sign[3]*hp->vars[3][0])) *
	    temp_mult) + hp->x_offset);
        hp->points[i].new_y =
	  ((((sign[0]*hp->vars[0][1]) + (sign[1]*hp->vars[1][1]) +
	     (sign[2]*hp->vars[2][1]) + (sign[3]*hp->vars[3][1])) *
	    temp_mult) + hp->y_offset);
        hp->points[i].same_p =
	  (hp->points[i].old_x == hp->points[i].new_x &&
	   hp->points[i].old_y == hp->points[i].new_y);
      }

      move_line (win, hp->points[0], hp->points[1], 0);
      move_line (win, hp->points[0], hp->points[2], 0);
      move_line (win, hp->points[2], hp->points[3], 0);
      move_line (win, hp->points[1], hp->points[3], 0);

      move_line (win, hp->points[8], hp->points[9], 1);
      move_line (win, hp->points[8], hp->points[10], 1);
      move_line (win, hp->points[10], hp->points[11], 1);
      move_line (win, hp->points[9], hp->points[11], 1);

      move_line (win, hp->points[4], hp->points[5], 2);
      move_line (win, hp->points[4], hp->points[6], 2);
      move_line (win, hp->points[6], hp->points[7], 2);
      move_line (win, hp->points[5], hp->points[7], 2);

      move_line (win, hp->points[3], hp->points[7], 3);
      move_line (win, hp->points[3], hp->points[11], 3);
      move_line (win, hp->points[11], hp->points[15], 3);
      move_line (win, hp->points[7], hp->points[15], 3);

      move_line (win, hp->points[0], hp->points[4], 4);
      move_line (win, hp->points[0], hp->points[8], 4);
      move_line (win, hp->points[4], hp->points[12], 4);
      move_line (win, hp->points[8], hp->points[12], 4);

      move_line (win, hp->points[1], hp->points[5], 5);
      move_line (win, hp->points[1], hp->points[9], 5);
      move_line (win, hp->points[9], hp->points[13], 5);
      move_line (win, hp->points[5], hp->points[13], 5);

      move_line (win, hp->points[2], hp->points[6], 6);
      move_line (win, hp->points[2], hp->points[10], 6);
      move_line (win, hp->points[10], hp->points[14], 6);
      move_line (win, hp->points[6], hp->points[14], 6);
      
      move_line (win, hp->points[12], hp->points[13], 7);
      move_line (win, hp->points[12], hp->points[14], 7);
      move_line (win, hp->points[14], hp->points[15], 7);
      move_line (win, hp->points[13], hp->points[15], 7);

      for (i = 0; i < COMBOS; i++)
        if (hp->sin_array[i] != 0)
          for (j = 0; j < DIMS; j++)
          {
            tmp0 = ((hp->vars[j][left_part[i]] * hp->cos_array[i]) +
  		  (hp->vars[j][right_part[i]] * hp->sin_array[i]));
            tmp1 = ((hp->vars[j][right_part[i]] * hp->cos_array[i]) -
	  	  (hp->vars[j][left_part[i]] * hp->sin_array[i]));
            hp->vars[j][left_part[i]] = tmp0;
	    hp->vars[j][right_part[i]] = tmp1;
          }
      if (seconds() - hp->startTime > TIMEOUT)
        inithyper(win);
}
