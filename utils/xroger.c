/* xscreensaver, Copyright (c) 1991-1998 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include "utils.h"

static void
crossbones (Display *dpy, Window window, GC draw_gc,
	    int x, int y, int w, int h)
{
  double xscale = w / 440.0;
  double yscale = h / 216.0;
  XPoint points [6];
  points[0].x = x + xscale * 20;  points[0].y = y + yscale * 10;
  points[1].x = x + xscale * 120; points[1].y = y + yscale * 10;
  points[2].x = x + xscale * 243; points[2].y = y + yscale * 93;
  points[3].x = x + xscale * 57;  points[3].y = y + yscale * 210;
  points[4].x = x + xscale * 20;  points[4].y = y + yscale * 210;
  points[5].x = x + xscale * 175; points[5].y = y + yscale * 113;
  XFillPolygon (dpy, window, draw_gc, points, 6, Complex, CoordModeOrigin);
  points[0].x = x + xscale * 202; points[0].y = y + yscale * 132;
  points[1].x = x + xscale * 384; points[1].y = y + yscale * 10;
  points[2].x = x + xscale * 420; points[2].y = y + yscale * 10;
  points[3].x = x + xscale * 270; points[3].y = y + yscale * 113;
  points[4].x = x + xscale * 420; points[4].y = y + yscale * 210;
  points[5].x = x + xscale * 320; points[5].y = y + yscale * 210;
  XFillPolygon (dpy, window, draw_gc, points, 6, Complex, CoordModeOrigin);
}


#include "spline.h"

void
skull (Display *dpy, Window window, GC draw_gc, GC erase_gc,
       int x, int y, int w, int h)
{
  spline s;
  float w100, h100;
  XPoint points [20];
  double sx[20], sy[20];
  int i;

  memset(&s, 0, sizeof(s));
  s.control_x = sx;
  s.control_y = sy;

  y -= (w * 0.025);

  crossbones (dpy, window, draw_gc, x, y+(h/2), w, (h / 3));

  x += (w * 0.27);
  y += (h * 0.25);
  w *= 0.6;
  h *= 0.6;

  w100 = w / 100.0;
  h100 = h / 100.0;

  points[ 0].x = x + (0   * w100); points[ 0].y = y + (10 * h100);
  points[ 1].x = x + (10  * w100); points[ 1].y = y + (0  * h100);
  points[ 2].x = x + (90  * w100); points[ 2].y = y + (0  * h100);
  points[ 3].x = x + (100 * w100); points[ 3].y = y + (10 * h100);
  points[ 4].x = x + (100 * w100); points[ 4].y = y + (30 * h100);
  points[ 5].x = x + (90  * w100); points[ 5].y = y + (40 * h100);
  points[ 6].x = x + (70  * w100); points[ 6].y = y + (40 * h100);
  points[ 7].x = x + (70  * w100); points[ 7].y = y + (50 * h100);
  points[ 8].x = x + (30  * w100); points[ 8].y = y + (50 * h100);
  points[ 9].x = x + (30  * w100); points[ 9].y = y + (40 * h100);
  points[10].x = x + (10  * w100); points[10].y = y + (40 * h100);
  points[11].x = x + (0   * w100); points[11].y = y + (30 * h100);

  for (i = 0; i < 12; i++)
    sx[i] = points[i].x, sy[i] = points[i].y;
  s.n_controls = i;
  s.allocated_points = i;
  s.points = (XPoint *) calloc (i, sizeof (*s.points));
  compute_closed_spline(&s);

  XFillPolygon (dpy, window, draw_gc, points+6, 4, Complex, CoordModeOrigin);
  XFillPolygon (dpy, window, draw_gc, s.points, s.n_points,
		Complex, CoordModeOrigin);

  points[0].x = x + (20  * w100); points[0].y = y + (18 * h100);
  points[1].x = x + (25  * w100); points[1].y = y + (15 * h100);
  points[2].x = x + (43  * w100); points[2].y = y + (15 * h100);
  points[3].x = x + (45  * w100); points[3].y = y + (17 * h100);
  points[4].x = x + (45  * w100); points[4].y = y + (25 * h100);
  points[5].x = x + (40  * w100); points[5].y = y + (30 * h100);
  points[6].x = x + (30  * w100); points[6].y = y + (30 * h100);
  points[7].x = x + (20  * w100); points[7].y = y + (23 * h100);
  for (i = 0; i < 8; i++)
    sx[i] = points[i].x, sy[i] = points[i].y;
  s.n_controls = i;
  compute_closed_spline(&s);
  XFillPolygon (dpy, window, erase_gc, s.points, s.n_points,
		Complex, CoordModeOrigin);

  points[0].x = x + (80  * w100); points[0].y = y + (18 * h100);
  points[1].x = x + (75  * w100); points[1].y = y + (15 * h100);
  points[2].x = x + (57  * w100); points[2].y = y + (15 * h100);
  points[3].x = x + (55  * w100); points[3].y = y + (17 * h100);
  points[4].x = x + (55  * w100); points[4].y = y + (25 * h100);
  points[5].x = x + (60  * w100); points[5].y = y + (30 * h100);
  points[6].x = x + (70  * w100); points[6].y = y + (30 * h100);
  points[7].x = x + (80  * w100); points[7].y = y + (23 * h100);
  for (i = 0; i < 8; i++)
    sx[i] = points[i].x, sy[i] = points[i].y;
  s.n_controls = i;
  compute_closed_spline(&s);
  XFillPolygon (dpy, window, erase_gc, s.points, s.n_points,
		Complex, CoordModeOrigin);

  points[ 0].x = x + (48  * w100); points[ 0].y = y + (30 * h100);
  points[ 1].x = x + (52  * w100); points[ 1].y = y + (30 * h100);
  points[ 2].x = x + (56  * w100); points[ 2].y = y + (42 * h100);
  points[ 3].x = x + (52  * w100); points[ 3].y = y + (45 * h100);
  points[ 4].x = x + (48  * w100); points[ 4].y = y + (45 * h100);
  points[ 5].x = x + (44  * w100); points[ 5].y = y + (42 * h100);
  for (i = 0; i < 6; i++)
    sx[i] = points[i].x, sy[i] = points[i].y;
  s.n_controls = i;
  compute_closed_spline(&s);
  XFillPolygon (dpy, window, erase_gc, s.points, s.n_points,
		Complex, CoordModeOrigin);

  free(s.points);
}
