/* xscreensaver, Copyright (c) 1999 Jamie Zawinski <jwz@jwz.org>
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

struct disc {
  int theta;		/* 0 - 360*64 */
  int velocity;
  int acceleration;
  int limit;
  GC gc;
  void (*draw) (Display *, Drawable, struct disc *,
                int x, int y, int radius);
};


static void
draw_letters (Display *dpy, Drawable d, struct disc *disc,
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

  XDrawLines (dpy, d, disc->gc, points, 5, CoordModeOrigin);

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

  XDrawLines (dpy, d, disc->gc, points, 9, CoordModeOrigin);

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

  XDrawLines (dpy, d, disc->gc, points, 7, CoordModeOrigin);

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

  XDrawLines (dpy, d, disc->gc, points, 9, CoordModeOrigin);

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

  XDrawLines (dpy, d, disc->gc, points, 9, CoordModeOrigin);

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

  XDrawLines (dpy, d, disc->gc, points, 4, CoordModeOrigin);

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

  XDrawLines (dpy, d, disc->gc, points, 9, CoordModeOrigin);

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

  XDrawLines (dpy, d, disc->gc, points, 9, CoordModeOrigin);


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

  XDrawLines (dpy, d, disc->gc, points, 7, CoordModeOrigin);

  /* 12 (1) */

  th = th2 + (2 * M_PI * 0.58333);

  points[0].x = x + radius * 0.77 * cos(th - 0.06);
  points[0].y = y + radius * 0.77 * sin(th - 0.06);

  points[1].x = x + radius * 0.8  * cos(th - 0.03);
  points[1].y = y + radius * 0.8  * sin(th - 0.03);

  points[2].x = x + radius * 0.7  * cos(th - 0.03);
  points[2].y = y + radius * 0.7  * sin(th - 0.03);

  XDrawLines (dpy, d, disc->gc, points, 3, CoordModeOrigin);

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

  XDrawLines (dpy, d, disc->gc, points, 8, CoordModeOrigin);

  /* 15 (1) */

  th = th2 + (2 * M_PI * 0.66666);

  points[0].x = x + radius * 0.77 * cos(th - 0.06);
  points[0].y = y + radius * 0.77 * sin(th - 0.06);

  points[1].x = x + radius * 0.8  * cos(th - 0.03);
  points[1].y = y + radius * 0.8  * sin(th - 0.03);

  points[2].x = x + radius * 0.7  * cos(th - 0.03);
  points[2].y = y + radius * 0.7  * sin(th - 0.03);

  XDrawLines (dpy, d, disc->gc, points, 3, CoordModeOrigin);

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

  XDrawLines (dpy, d, disc->gc, points, 9, CoordModeOrigin);

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

  XDrawLines (dpy, d, disc->gc, points, 8, CoordModeOrigin);

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

  XDrawLines (dpy, d, disc->gc, points, 8, CoordModeOrigin);

  /* 21 (2) */

  points[0].x = x + radius * 0.77 * cos(th + 0.03);
  points[0].y = y + radius * 0.77 * sin(th + 0.03);

  points[1].x = x + radius * 0.8  * cos(th + 0.06);
  points[1].y = y + radius * 0.8  * sin(th + 0.06);

  points[2].x = x + radius * 0.7  * cos(th + 0.06);
  points[2].y = y + radius * 0.7  * sin(th + 0.06);

  XDrawLines (dpy, d, disc->gc, points, 3, CoordModeOrigin);

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

  XDrawLines (dpy, d, disc->gc, points, 8, CoordModeOrigin);

  /* 24 (2) */

  points[0].x = x + radius * 0.69 * cos(th + 0.09);
  points[0].y = y + radius * 0.69 * sin(th + 0.09);

  points[1].x = x + radius * 0.8  * cos(th + 0.09);
  points[1].y = y + radius * 0.8  * sin(th + 0.09);

  points[2].x = x + radius * 0.72 * cos(th + 0.01);
  points[2].y = y + radius * 0.72 * sin(th + 0.01);

  points[3].x = x + radius * 0.72 * cos(th + 0.13);
  points[3].y = y + radius * 0.72 * sin(th + 0.13);

  XDrawLines (dpy, d, disc->gc, points, 4, CoordModeOrigin);
}


static void
draw_ticks (Display *dpy, Drawable d, struct disc *disc,
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
  XDrawSegments (dpy, d, disc->gc, segs, countof(segs));

  draw_letters (dpy, d, disc, x, y, radius);
}


static void
draw_thin_arrow (Display *dpy, Drawable d, struct disc *disc,
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

  XDrawLine (dpy, d, disc->gc,
             (int) (x + radius2 * cos(th)),
             (int) (y + radius2 * sin(th)),
             (int) (x + -radius * cos(th)),
             (int) (y + -radius * sin(th)));

  XFillPolygon (dpy, d, disc->gc, points, 3, Convex, CoordModeOrigin);
}


static void
draw_thick_arrow (Display *dpy, Drawable d, struct disc *disc,
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

  XDrawLines (dpy, d, disc->gc, points, 4, CoordModeOrigin);

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

  XDrawLines (dpy, d, disc->gc, points, 7, CoordModeOrigin);
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
draw_compass (Display *dpy, Drawable d, struct disc **discs,
              int x, int y, int radius)
{
  int i = 0;
  while (discs[i])
    {
      discs[i]->draw (dpy, d, discs[i], x, y, radius);
      roll_disc (discs[i]);
      i++;
    }
}

static void
draw_pointer (Display *dpy, Drawable d, GC ptr_gc, GC dot_gc,
              int x, int y, int radius)
{
  XPoint points[3];
  int size = radius * 0.1;

  /* top */

  points[0].x = x - size;
  points[0].y = y - radius - size;

  points[1].x = x + size;
  points[1].y = y - radius - size;

  points[2].x = x;
  points[2].y = y - radius;
  
  XFillPolygon (dpy, d, ptr_gc, points, 3, Convex, CoordModeOrigin);

  /* top right */

  points[0].x = x - (radius * 0.85);
  points[0].y = y - (radius * 0.8);

  points[1].x = x - (radius * 1.1);
  points[1].y = y - (radius * 0.55);

  points[2].x = x - (radius * 0.6);
  points[2].y = y - (radius * 0.65);
  
  XFillPolygon (dpy, d, ptr_gc, points, 3, Convex, CoordModeOrigin);

  /* left */

  points[0].x = x - (radius * 1.05);
  points[0].y = y;

  points[1].x = x - (radius * 1.1);
  points[1].y = y - (radius * 0.025);

  points[2].x = x - (radius * 1.1);
  points[2].y = y + (radius * 0.025);
  
  XFillPolygon (dpy, d, dot_gc, points, 3, Convex, CoordModeOrigin);

  /* right */

  points[0].x = x + (radius * 1.05);
  points[0].y = y;

  points[1].x = x + (radius * 1.1);
  points[1].y = y - (radius * 0.025);

  points[2].x = x + (radius * 1.1);
  points[2].y = y + (radius * 0.025);
  
  XFillPolygon (dpy, d, dot_gc, points, 3, Convex, CoordModeOrigin);

  /* bottom */

  points[0].x = x;
  points[0].y = y + (radius * 1.05);

  points[1].x = x - (radius * 0.025);
  points[1].y = y + (radius * 1.1);

  points[2].x = x + (radius * 0.025);
  points[2].y = y + (radius * 1.1);
  
  XFillPolygon (dpy, d, dot_gc, points, 3, Convex, CoordModeOrigin);

  /* bottom left */

  points[0].x = x + (radius * 0.74);
  points[0].y = y + (radius * 0.74);

  points[1].x = x + (radius * 0.78);
  points[1].y = y + (radius * 0.75);

  points[2].x = x + (radius * 0.75);
  points[2].y = y + (radius * 0.78);
  
  XFillPolygon (dpy, d, dot_gc, points, 3, Convex, CoordModeOrigin);

  /* top left */

  points[0].x = x + (radius * 0.74);
  points[0].y = y - (radius * 0.74);

  points[1].x = x + (radius * 0.78);
  points[1].y = y - (radius * 0.75);

  points[2].x = x + (radius * 0.75);
  points[2].y = y - (radius * 0.78);
  
  XFillPolygon (dpy, d, dot_gc, points, 3, Convex, CoordModeOrigin);

  /* bottom right */

  points[0].x = x - (radius * 0.74);
  points[0].y = y + (radius * 0.74);

  points[1].x = x - (radius * 0.78);
  points[1].y = y + (radius * 0.75);

  points[2].x = x - (radius * 0.75);
  points[2].y = y + (radius * 0.78);
  
  XFillPolygon (dpy, d, dot_gc, points, 3, Convex, CoordModeOrigin);
}


char *progclass = "Compass";

char *defaults [] = {
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

XrmOptionDescRec options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-db",		".doubleBuffer", XrmoptionNoArg,  "True" },
  { "-no-db",		".doubleBuffer", XrmoptionNoArg,  "False" },
  { 0, 0, 0, 0 }
};

void
screenhack (Display *dpy, Window window)
{
  XGCValues gcv;
  int delay = get_integer_resource ("delay", "Integer");
  Bool dbuf = get_boolean_resource ("doubleBuffer", "Boolean");
  struct disc *discs[4];
  int x, y, size, size2;
  GC ptr_gc;
  GC erase_gc = 0;
  XWindowAttributes xgwa;
  Pixmap b=0, ba=0, bb=0;	/* double-buffer to reduce flicker */
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
  XdbeBackBuffer backb = 0;
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

  XGetWindowAttributes (dpy, window, &xgwa);
  size2 = MIN(xgwa.width, xgwa.height);

  if (size2 > 600) size2 = 600;

  size = (size2 / 2) * 0.8;

  x = xgwa.width/2;
  y = xgwa.height/2;

  if (dbuf)
    {
#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
      b = backb = xdbe_get_backbuffer (dpy, window, XdbeUndefined);
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

      if (!b)
        {
          x = size2/2;
          y = size2/2;
          ba = XCreatePixmap (dpy, window, size2, size2, xgwa.depth);
          bb = XCreatePixmap (dpy, window, size2, size2, xgwa.depth);
          b = ba;
        }
    }
  else
    {
      b = window;
    }

  discs[0] = (struct disc *) calloc (1, sizeof (struct disc));
  discs[1] = (struct disc *) calloc (1, sizeof (struct disc));
  discs[2] = (struct disc *) calloc (1, sizeof (struct disc));
  discs[3] = 0;

  gcv.foreground = get_pixel_resource ("foreground", "Foreground",
                                       dpy, xgwa.colormap);
  gcv.line_width = MAX(2, (size/60));
  gcv.join_style = JoinBevel;
  discs[0]->draw = draw_ticks;
  discs[0]->gc = XCreateGC (dpy, b, GCForeground|GCLineWidth|GCJoinStyle,
                            &gcv);
  init_spin (discs[0]);

  gcv.foreground = get_pixel_resource ("arrow2Foreground", "Foreground",
                                       dpy, xgwa.colormap);
  gcv.line_width = MAX(4, (size / 30));
  discs[1]->draw = draw_thick_arrow;
  discs[1]->gc = XCreateGC (dpy, b, GCForeground|GCLineWidth, &gcv);
  init_spin (discs[1]);

  gcv.foreground = get_pixel_resource ("arrow1Foreground", "Foreground",
                                       dpy, xgwa.colormap);
  gcv.line_width = MAX(4, (size / 30));
  discs[2]->draw = draw_thin_arrow;
  discs[2]->gc = XCreateGC (dpy, b, GCForeground|GCLineWidth, &gcv);
  init_spin (discs[2]);

  gcv.foreground = get_pixel_resource ("pointerForeground", "Foreground",
                                       dpy, xgwa.colormap);
  ptr_gc = XCreateGC (dpy, b, GCForeground|GCLineWidth, &gcv);

  gcv.foreground = get_pixel_resource ("background", "Background",
                                       dpy, xgwa.colormap);
  erase_gc = XCreateGC (dpy, b, GCForeground, &gcv);

  if (ba) XFillRectangle (dpy, ba, erase_gc, 0, 0, size2, size2);
  if (bb) XFillRectangle (dpy, bb, erase_gc, 0, 0, size2, size2);

  while (1)
    {
      XFillRectangle (dpy, b, erase_gc, 0, 0, xgwa.width, xgwa.height);

      draw_compass (dpy, b, discs, x, y, size);
      draw_pointer (dpy, b, ptr_gc, discs[0]->gc, x, y, size);

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
      if (backb)
        {
          XdbeSwapInfo info[1];
          info[0].swap_window = window;
          info[0].swap_action = XdbeUndefined;
          XdbeSwapBuffers (dpy, info, 1);
        }
      else
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
      if (dbuf)
        {
          XCopyArea (dpy, b, window, erase_gc, 0, 0,
                     size2, size2,
                     xgwa.width/2 - x,
                     xgwa.height/2 - y);
          b = (b == ba ? bb : ba);
        }

      XSync (dpy, False);
      screenhack_handle_events (dpy);
      if (delay)
        usleep (delay);
    }
}
