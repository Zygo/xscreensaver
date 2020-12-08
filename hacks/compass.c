/* xscreensaver, Copyright (c) 1999-2018 Jamie Zawinski <jwz@jwz.org>
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
#include "screenhack.h"

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
# include "xdbe.h"
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

#define countof(x) (sizeof(x)/sizeof(*(x)))
#define ABS(x) ((x)<0?-(x):(x))
#define MAX(x,y) ((x)<(y)?(y):(x))
#define MIN(x,y) ((x)>(y)?(y):(x))
#define RAND(n) ((long) ((random() & 0x7fffffff) % ((long) (n))))
#define RANDSIGN() ((random() & 1) ? 1 : -1)

struct state {
  Display *dpy;
  Window window;

  int delay;
  Bool dbuf;
  struct disc *discs[4];
  int x, y, size, size2;
  GC ptr_gc;
  GC erase_gc;
  XWindowAttributes xgwa;
  Pixmap b, ba, bb;	/* double-buffer to reduce flicker */
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  XdbeBackBuffer backb;
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
};



struct disc {
  int theta;		/* 0 - 360*64 */
  int velocity;
  int acceleration;
  int limit;
  GC gc;
  void (*draw) (struct state *, Drawable, struct disc *,
                int x, int y, int radius);
};


static void
draw_letters (struct state *st, Drawable d, struct disc *disc,
              int x, int y, int radius)
{
  XPoint points[50];
  double th2 = 2 * M_PI * (disc->theta / ((double) 360*64));
  double th;

  /* W */

  th = th2;

  points[0].x = x + radius * 0.8 * cos(th - 0.07);
  points[0].y = y + radius * 0.8 * sin(th - 0.07);

  points[1].x = x + radius * 0.7 * cos(th - 0.05);
  points[1].y = y + radius * 0.7 * sin(th - 0.05);

  points[2].x = x + radius * 0.78 * cos(th);
  points[2].y = y + radius * 0.78 * sin(th);

  points[3].x = x + radius * 0.7 * cos(th + 0.05);
  points[3].y = y + radius * 0.7 * sin(th + 0.05);

  points[4].x = x + radius * 0.8 * cos(th + 0.07);
  points[4].y = y + radius * 0.8 * sin(th + 0.07);

  XDrawLines (st->dpy, d, disc->gc, points, 5, CoordModeOrigin);

  /* 30 (1) */

  th = th2 + (2 * M_PI * 0.08333);

  points[0].x = x + radius * 0.78 * cos(th - 0.13);
  points[0].y = y + radius * 0.78 * sin(th - 0.13);

  points[1].x = x + radius * 0.8  * cos(th - 0.08);
  points[1].y = y + radius * 0.8  * sin(th - 0.08);

  points[2].x = x + radius * 0.78 * cos(th - 0.03);
  points[2].y = y + radius * 0.78 * sin(th - 0.03);

  points[3].x = x + radius * 0.76 * cos(th - 0.03);
  points[3].y = y + radius * 0.76 * sin(th - 0.03);

  points[4].x = x + radius * 0.75 * cos(th - 0.08);
  points[4].y = y + radius * 0.75 * sin(th - 0.08);

  points[5].x = x + radius * 0.74 * cos(th - 0.03);
  points[5].y = y + radius * 0.74 * sin(th - 0.03);

  points[6].x = x + radius * 0.72 * cos(th - 0.03);
  points[6].y = y + radius * 0.72 * sin(th - 0.03);

  points[7].x = x + radius * 0.7  * cos(th - 0.08);
  points[7].y = y + radius * 0.7  * sin(th - 0.08);

  points[8].x = x + radius * 0.72  * cos(th - 0.13);
  points[8].y = y + radius * 0.72  * sin(th - 0.13);

  XDrawLines (st->dpy, d, disc->gc, points, 9, CoordModeOrigin);

  /* 30 (2) */

  points[0].x = x + radius * 0.78 * cos(th + 0.03);
  points[0].y = y + radius * 0.78 * sin(th + 0.03);

  points[1].x = x + radius * 0.8  * cos(th + 0.08);
  points[1].y = y + radius * 0.8  * sin(th + 0.08);

  points[2].x = x + radius * 0.78 * cos(th + 0.13);
  points[2].y = y + radius * 0.78 * sin(th + 0.13);

  points[3].x = x + radius * 0.72 * cos(th + 0.13);
  points[3].y = y + radius * 0.72 * sin(th + 0.13);

  points[4].x = x + radius * 0.7  * cos(th + 0.08);
  points[4].y = y + radius * 0.7  * sin(th + 0.08);

  points[5].x = x + radius * 0.72  * cos(th + 0.03);
  points[5].y = y + radius * 0.72  * sin(th + 0.03);

  points[6] = points[0];

  XDrawLines (st->dpy, d, disc->gc, points, 7, CoordModeOrigin);

  /* 33 (1) */

  th = th2 + (2 * M_PI * 0.16666);

  points[0].x = x + radius * 0.78 * cos(th - 0.13);
  points[0].y = y + radius * 0.78 * sin(th - 0.13);

  points[1].x = x + radius * 0.8  * cos(th - 0.08);
  points[1].y = y + radius * 0.8  * sin(th - 0.08);

  points[2].x = x + radius * 0.78 * cos(th - 0.03);
  points[2].y = y + radius * 0.78 * sin(th - 0.03);

  points[3].x = x + radius * 0.76 * cos(th - 0.03);
  points[3].y = y + radius * 0.76 * sin(th - 0.03);

  points[4].x = x + radius * 0.75 * cos(th - 0.08);
  points[4].y = y + radius * 0.75 * sin(th - 0.08);

  points[5].x = x + radius * 0.74 * cos(th - 0.03);
  points[5].y = y + radius * 0.74 * sin(th - 0.03);

  points[6].x = x + radius * 0.72 * cos(th - 0.03);
  points[6].y = y + radius * 0.72 * sin(th - 0.03);

  points[7].x = x + radius * 0.7  * cos(th - 0.08);
  points[7].y = y + radius * 0.7  * sin(th - 0.08);

  points[8].x = x + radius * 0.72  * cos(th - 0.13);
  points[8].y = y + radius * 0.72  * sin(th - 0.13);

  XDrawLines (st->dpy, d, disc->gc, points, 9, CoordModeOrigin);

  /* 33 (2) */

  points[0].x = x + radius * 0.78 * cos(th + 0.03);
  points[0].y = y + radius * 0.78 * sin(th + 0.03);

  points[1].x = x + radius * 0.8  * cos(th + 0.08);
  points[1].y = y + radius * 0.8  * sin(th + 0.08);

  points[2].x = x + radius * 0.78 * cos(th + 0.13);
  points[2].y = y + radius * 0.78 * sin(th + 0.13);

  points[3].x = x + radius * 0.76 * cos(th + 0.13);
  points[3].y = y + radius * 0.76 * sin(th + 0.13);

  points[4].x = x + radius * 0.75 * cos(th + 0.08);
  points[4].y = y + radius * 0.75 * sin(th + 0.08);

  points[5].x = x + radius * 0.74 * cos(th + 0.13);
  points[5].y = y + radius * 0.74 * sin(th + 0.13);

  points[6].x = x + radius * 0.72 * cos(th + 0.13);
  points[6].y = y + radius * 0.72 * sin(th + 0.13);

  points[7].x = x + radius * 0.7  * cos(th + 0.08);
  points[7].y = y + radius * 0.7  * sin(th + 0.08);

  points[8].x = x + radius * 0.72  * cos(th + 0.03);
  points[8].y = y + radius * 0.72  * sin(th + 0.03);

  XDrawLines (st->dpy, d, disc->gc, points, 9, CoordModeOrigin);

  /* N */

  th = th2 + (2 * M_PI * 0.25);

  points[0].x = x + radius * 0.7 * cos(th - 0.05);
  points[0].y = y + radius * 0.7 * sin(th - 0.05);

  points[1].x = x + radius * 0.8 * cos(th - 0.05);
  points[1].y = y + radius * 0.8 * sin(th - 0.05);

  points[2].x = x + radius * 0.7 * cos(th + 0.05);
  points[2].y = y + radius * 0.7 * sin(th + 0.05);

  points[3].x = x + radius * 0.8 * cos(th + 0.05);
  points[3].y = y + radius * 0.8 * sin(th + 0.05);

  XDrawLines (st->dpy, d, disc->gc, points, 4, CoordModeOrigin);

  /* 3 */

  th = th2 + (2 * M_PI * 0.33333);

  points[0].x = x + radius * 0.78 * cos(th - 0.05);
  points[0].y = y + radius * 0.78 * sin(th - 0.05);

  points[1].x = x + radius * 0.8  * cos(th);
  points[1].y = y + radius * 0.8  * sin(th);

  points[2].x = x + radius * 0.78 * cos(th + 0.05);
  points[2].y = y + radius * 0.78 * sin(th + 0.05);

  points[3].x = x + radius * 0.76 * cos(th + 0.05);
  points[3].y = y + radius * 0.76 * sin(th + 0.05);

  points[4].x = x + radius * 0.75 * cos(th);
  points[4].y = y + radius * 0.75 * sin(th);

  points[5].x = x + radius * 0.74 * cos(th + 0.05);
  points[5].y = y + radius * 0.74 * sin(th + 0.05);

  points[6].x = x + radius * 0.72 * cos(th + 0.05);
  points[6].y = y + radius * 0.72 * sin(th + 0.05);

  points[7].x = x + radius * 0.7  * cos(th);
  points[7].y = y + radius * 0.7  * sin(th);

  points[8].x = x + radius * 0.72  * cos(th - 0.05);
  points[8].y = y + radius * 0.72  * sin(th - 0.05);

  XDrawLines (st->dpy, d, disc->gc, points, 9, CoordModeOrigin);

  /* 6 */

  th = th2 + (2 * M_PI * 0.41666);

  points[0].x = x + radius * 0.78 * cos(th + 0.05);
  points[0].y = y + radius * 0.78 * sin(th + 0.05);

  points[1].x = x + radius * 0.8  * cos(th);
  points[1].y = y + radius * 0.8  * sin(th);

  points[2].x = x + radius * 0.78 * cos(th - 0.05);
  points[2].y = y + radius * 0.78 * sin(th - 0.05);

  points[3].x = x + radius * 0.72 * cos(th - 0.05);
  points[3].y = y + radius * 0.72 * sin(th - 0.05);

  points[4].x = x + radius * 0.7  * cos(th);
  points[4].y = y + radius * 0.7  * sin(th);

  points[5].x = x + radius * 0.72 * cos(th + 0.05);
  points[5].y = y + radius * 0.72 * sin(th + 0.05);

  points[6].x = x + radius * 0.74 * cos(th + 0.05);
  points[6].y = y + radius * 0.74 * sin(th + 0.05);

  points[7].x = x + radius * 0.76 * cos(th);
  points[7].y = y + radius * 0.76 * sin(th);

  points[8].x = x + radius * 0.74 * cos(th - 0.05);
  points[8].y = y + radius * 0.74 * sin(th - 0.05);

  XDrawLines (st->dpy, d, disc->gc, points, 9, CoordModeOrigin);


  /* E */

  th = th2 + (2 * M_PI * 0.5);

  points[0].x = x + radius * 0.8 * cos(th + 0.05);
  points[0].y = y + radius * 0.8 * sin(th + 0.05);

  points[1].x = x + radius * 0.8 * cos(th - 0.05);
  points[1].y = y + radius * 0.8 * sin(th - 0.05);

  points[2].x = x + radius * 0.75 * cos(th - 0.05);
  points[2].y = y + radius * 0.75 * sin(th - 0.05);

  points[3].x = x + radius * 0.75 * cos(th + 0.025);
  points[3].y = y + radius * 0.75 * sin(th + 0.025);

  points[4].x = x + radius * 0.75 * cos(th - 0.05);
  points[4].y = y + radius * 0.75 * sin(th - 0.05);

  points[5].x = x + radius * 0.7 * cos(th - 0.05);
  points[5].y = y + radius * 0.7 * sin(th - 0.05);

  points[6].x = x + radius * 0.7 * cos(th + 0.05);
  points[6].y = y + radius * 0.7 * sin(th + 0.05);

  XDrawLines (st->dpy, d, disc->gc, points, 7, CoordModeOrigin);

  /* 12 (1) */

  th = th2 + (2 * M_PI * 0.58333);

  points[0].x = x + radius * 0.77 * cos(th - 0.06);
  points[0].y = y + radius * 0.77 * sin(th - 0.06);

  points[1].x = x + radius * 0.8  * cos(th - 0.03);
  points[1].y = y + radius * 0.8  * sin(th - 0.03);

  points[2].x = x + radius * 0.7  * cos(th - 0.03);
  points[2].y = y + radius * 0.7  * sin(th - 0.03);

  XDrawLines (st->dpy, d, disc->gc, points, 3, CoordModeOrigin);

  /* 12 (2) */

  points[0].x = x + radius * 0.78 * cos(th + 0.02);
  points[0].y = y + radius * 0.78 * sin(th + 0.02);

  points[1].x = x + radius * 0.8  * cos(th + 0.07);
  points[1].y = y + radius * 0.8  * sin(th + 0.07);

  points[2].x = x + radius * 0.78 * cos(th + 0.11);
  points[2].y = y + radius * 0.78 * sin(th + 0.11);

  points[3].x = x + radius * 0.76 * cos(th + 0.11);
  points[3].y = y + radius * 0.76 * sin(th + 0.11);

  points[4].x = x + radius * 0.74 * cos(th + 0.02);
  points[4].y = y + radius * 0.74 * sin(th + 0.02);

  points[5].x = x + radius * 0.71 * cos(th + 0.03);
  points[5].y = y + radius * 0.71 * sin(th + 0.03);

  points[6].x = x + radius * 0.7  * cos(th + 0.03);
  points[6].y = y + radius * 0.7  * sin(th + 0.03);

  points[7].x = x + radius * 0.7  * cos(th + 0.13);
  points[7].y = y + radius * 0.7  * sin(th + 0.13);

  XDrawLines (st->dpy, d, disc->gc, points, 8, CoordModeOrigin);

  /* 15 (1) */

  th = th2 + (2 * M_PI * 0.66666);

  points[0].x = x + radius * 0.77 * cos(th - 0.06);
  points[0].y = y + radius * 0.77 * sin(th - 0.06);

  points[1].x = x + radius * 0.8  * cos(th - 0.03);
  points[1].y = y + radius * 0.8  * sin(th - 0.03);

  points[2].x = x + radius * 0.7  * cos(th - 0.03);
  points[2].y = y + radius * 0.7  * sin(th - 0.03);

  XDrawLines (st->dpy, d, disc->gc, points, 3, CoordModeOrigin);

  /* 15 (2) */

  points[0].x = x + radius * 0.8  * cos(th + 0.11);
  points[0].y = y + radius * 0.8  * sin(th + 0.11);

  points[1].x = x + radius * 0.8  * cos(th + 0.02);
  points[1].y = y + radius * 0.8  * sin(th + 0.02);

  points[2].x = x + radius * 0.76 * cos(th + 0.02);
  points[2].y = y + radius * 0.76 * sin(th + 0.02);

  points[3].x = x + radius * 0.77 * cos(th + 0.06);
  points[3].y = y + radius * 0.77 * sin(th + 0.06);

  points[4].x = x + radius * 0.76 * cos(th + 0.10);
  points[4].y = y + radius * 0.76 * sin(th + 0.10);

  points[5].x = x + radius * 0.73 * cos(th + 0.11);
  points[5].y = y + radius * 0.73 * sin(th + 0.11);

  points[6].x = x + radius * 0.72 * cos(th + 0.10);
  points[6].y = y + radius * 0.72 * sin(th + 0.10);

  points[7].x = x + radius * 0.7  * cos(th + 0.06);
  points[7].y = y + radius * 0.7  * sin(th + 0.06);

  points[8].x = x + radius * 0.72 * cos(th + 0.02);
  points[8].y = y + radius * 0.72 * sin(th + 0.02);

  XDrawLines (st->dpy, d, disc->gc, points, 9, CoordModeOrigin);

  /* S */

  th = th2 + (2 * M_PI * 0.75);

  points[0].x = x + radius * 0.78 * cos(th + 0.05);
  points[0].y = y + radius * 0.78 * sin(th + 0.05);

  points[1].x = x + radius * 0.8  * cos(th);
  points[1].y = y + radius * 0.8  * sin(th);

  points[2].x = x + radius * 0.78 * cos(th - 0.05);
  points[2].y = y + radius * 0.78 * sin(th - 0.05);

  points[3].x = x + radius * 0.76 * cos(th - 0.05);
  points[3].y = y + radius * 0.76 * sin(th - 0.05);

  points[4].x = x + radius * 0.74 * cos(th + 0.05);
  points[4].y = y + radius * 0.74 * sin(th + 0.05);

  points[5].x = x + radius * 0.72 * cos(th + 0.05);
  points[5].y = y + radius * 0.72 * sin(th + 0.05);

  points[6].x = x + radius * 0.7  * cos(th);
  points[6].y = y + radius * 0.7  * sin(th);

  points[7].x = x + radius * 0.72  * cos(th - 0.05);
  points[7].y = y + radius * 0.72  * sin(th - 0.05);

  XDrawLines (st->dpy, d, disc->gc, points, 8, CoordModeOrigin);

  /* 21 (1) */

  th = th2 + (2 * M_PI * 0.83333);

  points[0].x = x + radius * 0.78 * cos(th - 0.13);
  points[0].y = y + radius * 0.78 * sin(th - 0.13);

  points[1].x = x + radius * 0.8  * cos(th - 0.08);
  points[1].y = y + radius * 0.8  * sin(th - 0.08);

  points[2].x = x + radius * 0.78 * cos(th - 0.03);
  points[2].y = y + radius * 0.78 * sin(th - 0.03);

  points[3].x = x + radius * 0.76 * cos(th - 0.03);
  points[3].y = y + radius * 0.76 * sin(th - 0.03);

  points[4].x = x + radius * 0.74 * cos(th - 0.12);
  points[4].y = y + radius * 0.74 * sin(th - 0.12);

  points[5].x = x + radius * 0.71 * cos(th - 0.13);
  points[5].y = y + radius * 0.71 * sin(th - 0.13);

  points[6].x = x + radius * 0.7  * cos(th - 0.13);
  points[6].y = y + radius * 0.7  * sin(th - 0.13);

  points[7].x = x + radius * 0.7  * cos(th - 0.02);
  points[7].y = y + radius * 0.7  * sin(th - 0.02);

  XDrawLines (st->dpy, d, disc->gc, points, 8, CoordModeOrigin);

  /* 21 (2) */

  points[0].x = x + radius * 0.77 * cos(th + 0.03);
  points[0].y = y + radius * 0.77 * sin(th + 0.03);

  points[1].x = x + radius * 0.8  * cos(th + 0.06);
  points[1].y = y + radius * 0.8  * sin(th + 0.06);

  points[2].x = x + radius * 0.7  * cos(th + 0.06);
  points[2].y = y + radius * 0.7  * sin(th + 0.06);

  XDrawLines (st->dpy, d, disc->gc, points, 3, CoordModeOrigin);

  /* 24 (1) */

  th = th2 + (2 * M_PI * 0.91666);

  points[0].x = x + radius * 0.78 * cos(th - 0.13);
  points[0].y = y + radius * 0.78 * sin(th - 0.13);

  points[1].x = x + radius * 0.8  * cos(th - 0.08);
  points[1].y = y + radius * 0.8  * sin(th - 0.08);

  points[2].x = x + radius * 0.78 * cos(th - 0.03);
  points[2].y = y + radius * 0.78 * sin(th - 0.03);

  points[3].x = x + radius * 0.76 * cos(th - 0.03);
  points[3].y = y + radius * 0.76 * sin(th - 0.03);

  points[4].x = x + radius * 0.74 * cos(th - 0.12);
  points[4].y = y + radius * 0.74 * sin(th - 0.12);

  points[5].x = x + radius * 0.71 * cos(th - 0.13);
  points[5].y = y + radius * 0.71 * sin(th - 0.13);

  points[6].x = x + radius * 0.7  * cos(th - 0.13);
  points[6].y = y + radius * 0.7  * sin(th - 0.13);

  points[7].x = x + radius * 0.7  * cos(th - 0.02);
  points[7].y = y + radius * 0.7  * sin(th - 0.02);

  XDrawLines (st->dpy, d, disc->gc, points, 8, CoordModeOrigin);

  /* 24 (2) */

  points[0].x = x + radius * 0.69 * cos(th + 0.09);
  points[0].y = y + radius * 0.69 * sin(th + 0.09);

  points[1].x = x + radius * 0.8  * cos(th + 0.09);
  points[1].y = y + radius * 0.8  * sin(th + 0.09);

  points[2].x = x + radius * 0.72 * cos(th + 0.01);
  points[2].y = y + radius * 0.72 * sin(th + 0.01);

  points[3].x = x + radius * 0.72 * cos(th + 0.13);
  points[3].y = y + radius * 0.72 * sin(th + 0.13);

  XDrawLines (st->dpy, d, disc->gc, points, 4, CoordModeOrigin);
}


static void
draw_ticks (struct state *st, Drawable d, struct disc *disc,
            int x, int y, int radius)
{
  XSegment segs[72];
  int i;
  double tick = (M_PI * 2) / 72;

  for (i = 0; i < 72; i++)
    {
      int radius2 = radius;
      double th = (i * tick) + (2 * M_PI * (disc->theta / ((double) 360*64)));

      if (i % 6)
        radius2 -= radius / 16;
      else
        radius2 -= radius / 8;

      segs[i].x1 = x + radius  * cos(th);
      segs[i].y1 = y + radius  * sin(th);
      segs[i].x2 = x + radius2 * cos(th);
      segs[i].y2 = y + radius2 * sin(th);
    }
  XDrawSegments (st->dpy, d, disc->gc, segs, countof(segs));

  draw_letters (st, d, disc, x, y, radius);
}


static void
draw_thin_arrow (struct state *st, Drawable d, struct disc *disc,
                 int x, int y, int radius)
{
  XPoint points[3];
  double th;
  int radius2;
  double tick = ((M_PI * 2) / 72) * 2;

  radius *= 0.9;
  radius2 = radius - (radius / 8) * 3;

  th = 2 * M_PI * (disc->theta / ((double) 360*64));

  points[0].x = x + radius * cos(th);		/* tip */
  points[0].y = y + radius * sin(th);

  points[1].x = x + radius2 * cos(th - tick);	/* tip left */
  points[1].y = y + radius2 * sin(th - tick);

  points[2].x = x + radius2 * cos(th + tick);	/* tip right */
  points[2].y = y + radius2 * sin(th + tick);

  XDrawLine (st->dpy, d, disc->gc,
             (int) (x + radius2 * cos(th)),
             (int) (y + radius2 * sin(th)),
             (int) (x + -radius * cos(th)),
             (int) (y + -radius * sin(th)));

  XFillPolygon (st->dpy, d, disc->gc, points, 3, Convex, CoordModeOrigin);
}


static void
draw_thick_arrow (struct state *st, Drawable d, struct disc *disc,
                  int x, int y, int radius)
{
  XPoint points[10];
  double th;
  int radius2, radius3;
  double tick = ((M_PI * 2) / 72) * 2;

  radius *= 0.9;
  radius2 = radius - (radius / 8) * 3;
  radius3 = radius - (radius / 8) * 2;
  th = 2 * M_PI * (disc->theta / ((double) 360*64));

  points[0].x = x + radius * cos(th);		/* tip */
  points[0].y = y + radius * sin(th);

  points[1].x = x + radius2 * cos(th - tick);	/* tip left */
  points[1].y = y + radius2 * sin(th - tick);

  points[2].x = x + radius2 * cos(th + tick);	/* tip right */
  points[2].y = y + radius2 * sin(th + tick);

  points[3] = points[0];

  XDrawLines (st->dpy, d, disc->gc, points, 4, CoordModeOrigin);

  points[0].x = x + radius2 * cos(th - tick/2);	 /* top left */
  points[0].y = y + radius2 * sin(th - tick/2);

  points[1].x = x + -radius2 * cos(th + tick/2); /* bottom left */
  points[1].y = y + -radius2 * sin(th + tick/2);

  points[2].x = x + -radius3 * cos(th);		 /* bottom */
  points[2].y = y + -radius3 * sin(th);

  points[3].x = x + -radius * cos(th);		 /* bottom spike */
  points[3].y = y + -radius * sin(th);

  points[4] = points[2];			 /* return */

  points[5].x = x + -radius2 * cos(th - tick/2); /* bottom right */
  points[5].y = y + -radius2 * sin(th - tick/2);

  points[6].x = x + radius2 * cos(th + tick/2);  /* top right */
  points[6].y = y + radius2 * sin(th + tick/2);

  XDrawLines (st->dpy, d, disc->gc, points, 7, CoordModeOrigin);
}



static void
roll_disc (struct disc *disc)
{
  double th = disc->theta;
  if (th < 0)
    th = -(th + disc->velocity);
  else
    th = (th + disc->velocity);

  if (th > (360*64))
    th -= (360*64);
  else if (th < 0)
    th += (360*64);

  disc->theta = (disc->theta > 0 ? th : -th);

  disc->velocity += disc->acceleration;

  if (disc->velocity > disc->limit || 
      disc->velocity < -disc->limit)
    disc->acceleration = -disc->acceleration;

  /* Alter direction of rotational acceleration randomly. */
  if (! (random() % 120))
    disc->acceleration = -disc->acceleration;

  /* Change acceleration very occasionally. */
  if (! (random() % 200))
    {
      if (random() & 1)
	disc->acceleration *= 1.2;
      else
	disc->acceleration *= 0.8;
    }
}


static void
init_spin (struct disc *disc)
{
  disc->limit = 5*64;
  disc->theta = RAND(360*64);
  disc->velocity = RAND(16) * RANDSIGN();
  disc->acceleration = RAND(16) * RANDSIGN();
}


static void
draw_compass (struct state *st)
{
  int i = 0;
  while (st->discs[i])
    {
      st->discs[i]->draw (st, st->b, st->discs[i], st->x, st->y, st->size);
      roll_disc (st->discs[i]);
      i++;
    }
}

static void
draw_pointer (struct state *st)
{
  int radius = st->size;
  GC dot_gc = st->discs[0]->gc;
  XPoint points[3];
  int size = radius * 0.1;

  /* top */

  points[0].x = st->x - size;
  points[0].y = st->y - radius - size;

  points[1].x = st->x + size;
  points[1].y = st->y - radius - size;

  points[2].x = st->x;
  points[2].y = st->y - radius;
  
  XFillPolygon (st->dpy, st->b, st->ptr_gc, points, 3, Convex, CoordModeOrigin);

  /* top right */

  points[0].x = st->x - (radius * 0.85);
  points[0].y = st->y - (radius * 0.8);

  points[1].x = st->x - (radius * 1.1);
  points[1].y = st->y - (radius * 0.55);

  points[2].x = st->x - (radius * 0.6);
  points[2].y = st->y - (radius * 0.65);
  
  XFillPolygon (st->dpy, st->b, st->ptr_gc, points, 3, Convex, CoordModeOrigin);

  /* left */

  points[0].x = st->x - (radius * 1.05);
  points[0].y = st->y;

  points[1].x = st->x - (radius * 1.1);
  points[1].y = st->y - (radius * 0.025);

  points[2].x = st->x - (radius * 1.1);
  points[2].y = st->y + (radius * 0.025);
  
  XFillPolygon (st->dpy, st->b, dot_gc, points, 3, Convex, CoordModeOrigin);

  /* right */

  points[0].x = st->x + (radius * 1.05);
  points[0].y = st->y;

  points[1].x = st->x + (radius * 1.1);
  points[1].y = st->y - (radius * 0.025);

  points[2].x = st->x + (radius * 1.1);
  points[2].y = st->y + (radius * 0.025);
  
  XFillPolygon (st->dpy, st->b, dot_gc, points, 3, Convex, CoordModeOrigin);

  /* bottom */

  points[0].x = st->x;
  points[0].y = st->y + (radius * 1.05);

  points[1].x = st->x - (radius * 0.025);
  points[1].y = st->y + (radius * 1.1);

  points[2].x = st->x + (radius * 0.025);
  points[2].y = st->y + (radius * 1.1);
  
  XFillPolygon (st->dpy, st->b, dot_gc, points, 3, Convex, CoordModeOrigin);

  /* bottom left */

  points[0].x = st->x + (radius * 0.74);
  points[0].y = st->y + (radius * 0.74);

  points[1].x = st->x + (radius * 0.78);
  points[1].y = st->y + (radius * 0.75);

  points[2].x = st->x + (radius * 0.75);
  points[2].y = st->y + (radius * 0.78);
  
  XFillPolygon (st->dpy, st->b, dot_gc, points, 3, Convex, CoordModeOrigin);

  /* top left */

  points[0].x = st->x + (radius * 0.74);
  points[0].y = st->y - (radius * 0.74);

  points[1].x = st->x + (radius * 0.78);
  points[1].y = st->y - (radius * 0.75);

  points[2].x = st->x + (radius * 0.75);
  points[2].y = st->y - (radius * 0.78);
  
  XFillPolygon (st->dpy, st->b, dot_gc, points, 3, Convex, CoordModeOrigin);

  /* bottom right */

  points[0].x = st->x - (radius * 0.74);
  points[0].y = st->y + (radius * 0.74);

  points[1].x = st->x - (radius * 0.78);
  points[1].y = st->y + (radius * 0.75);

  points[2].x = st->x - (radius * 0.75);
  points[2].y = st->y + (radius * 0.78);
  
  XFillPolygon (st->dpy, st->b, dot_gc, points, 3, Convex, CoordModeOrigin);
}


static void *
compass_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;
  st->dpy = dpy;
  st->window = window;
  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  st->dbuf = get_boolean_resource (st->dpy, "doubleBuffer", "Boolean");

# ifdef HAVE_JWXYZ	/* Don't second-guess Quartz's double-buffering */
  st->dbuf = False;
# endif

  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
  st->size2 = MIN(st->xgwa.width, st->xgwa.height);

  if (st->xgwa.width > st->xgwa.height * 5 ||  /* goofy aspect ratio */
      st->xgwa.height > st->xgwa.width * 5)
    st->size2 = MAX(st->xgwa.width, st->xgwa.height);

  {
    int max = 600;
    if (st->xgwa.width > 2560) max *= 2;  /* Retina displays */
    if (st->size2 > max) st->size2 = max;
  }

  st->size = (st->size2 / 2) * 0.8;

  st->x = st->xgwa.width/2;
  st->y = st->xgwa.height/2;

  if (st->dbuf)
    {
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
      st->b = st->backb = xdbe_get_backbuffer (st->dpy, st->window, XdbeUndefined);
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

      if (!st->b)
        {
          st->x = st->size2/2;
          st->y = st->size2/2;
          st->ba = XCreatePixmap (st->dpy, st->window, st->size2, st->size2, st->xgwa.depth);
          st->bb = XCreatePixmap (st->dpy, st->window, st->size2, st->size2, st->xgwa.depth);
          st->b = st->ba;
        }
    }
  else
    {
      st->b = st->window;
    }

  st->discs[0] = (struct disc *) calloc (1, sizeof (struct disc));
  st->discs[1] = (struct disc *) calloc (1, sizeof (struct disc));
  st->discs[2] = (struct disc *) calloc (1, sizeof (struct disc));
  st->discs[3] = 0;

  gcv.foreground = get_pixel_resource (st->dpy, st->xgwa.colormap,
                                       "foreground", "Foreground");
  gcv.line_width = MAX(2, (st->size/60));
  gcv.join_style = JoinBevel;
  st->discs[0]->draw = draw_ticks;
  st->discs[0]->gc = XCreateGC (st->dpy, st->b, GCForeground|GCLineWidth|GCJoinStyle,
                            &gcv);
  init_spin (st->discs[0]);

  gcv.foreground = get_pixel_resource (st->dpy, st->xgwa.colormap,
                                       "arrow2Foreground", "Foreground");
  gcv.line_width = MAX(4, (st->size / 30));
  st->discs[1]->draw = draw_thick_arrow;
  st->discs[1]->gc = XCreateGC (st->dpy, st->b, GCForeground|GCLineWidth, &gcv);
  init_spin (st->discs[1]);

  gcv.foreground = get_pixel_resource (st->dpy, st->xgwa.colormap,
                                       "arrow1Foreground", "Foreground");
  gcv.line_width = MAX(4, (st->size / 30));
  st->discs[2]->draw = draw_thin_arrow;
  st->discs[2]->gc = XCreateGC (st->dpy, st->b, GCForeground|GCLineWidth, &gcv);
  init_spin (st->discs[2]);

  gcv.foreground = get_pixel_resource (st->dpy, st->xgwa.colormap,
                                       "pointerForeground", "Foreground");
  st->ptr_gc = XCreateGC (st->dpy, st->b, GCForeground|GCLineWidth, &gcv);

  gcv.foreground = get_pixel_resource (st->dpy, st->xgwa.colormap,
                                       "background", "Background");
  st->erase_gc = XCreateGC (st->dpy, st->b, GCForeground, &gcv);

  if (st->ba) XFillRectangle (st->dpy, st->ba, st->erase_gc, 0, 0, st->size2, st->size2);
  if (st->bb) XFillRectangle (st->dpy, st->bb, st->erase_gc, 0, 0, st->size2, st->size2);

  return st;
}

static unsigned long
compass_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  XFillRectangle (st->dpy, st->b, st->erase_gc, 0, 0, st->xgwa.width, st->xgwa.height);

  draw_compass (st);
  draw_pointer (st);

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  if (st->backb)
    {
      XdbeSwapInfo info[1];
      info[0].swap_window = st->window;
      info[0].swap_action = XdbeUndefined;
      XdbeSwapBuffers (st->dpy, info, 1);
    }
  else
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
    if (st->dbuf)
      {
        XCopyArea (st->dpy, st->b, st->window, st->erase_gc, 0, 0,
                   st->size2, st->size2,
                   st->xgwa.width/2 - st->x,
                   st->xgwa.height/2 - st->y);
        st->b = (st->b == st->ba ? st->bb : st->ba);
      }

  return st->delay;
}

static void
compass_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
  st->size2 = MIN(st->xgwa.width, st->xgwa.height);
  st->x = st->xgwa.width/2;
  st->y = st->xgwa.height/2;
}

static Bool
compass_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
compass_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int i;
  XFreeGC (dpy, st->ptr_gc);
  XFreeGC (dpy, st->erase_gc);
  for (i = 0; i < 4; i++)
    if (st->discs[i]) {
      XFreeGC (dpy, st->discs[i]->gc);
      free (st->discs[i]);
    }
  if (st->ba) XFreePixmap (dpy, st->ba);
  if (st->bb) XFreePixmap (dpy, st->bb);
  free (st);
}



static const char *compass_defaults [] = {
  ".background:		#000000",
  ".foreground:		#DDFFFF",
  "*arrow1Foreground:	#FFF66A",
  "*arrow2Foreground:	#F7D64A",
  "*pointerForeground:	#FF0000",
  "*delay:		20000",
  "*doubleBuffer:	True",
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  "*useDBE:		True",
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
  0
};

static XrmOptionDescRec compass_options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-db",		".doubleBuffer", XrmoptionNoArg,  "True" },
  { "-no-db",		".doubleBuffer", XrmoptionNoArg,  "False" },
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("Compass", compass)
