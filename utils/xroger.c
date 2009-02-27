/* xscreensaver, Copyright (c) 1991-1993 Jamie Zawinski <jwz@lucid.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include <X11/Xlib.h>

static void
crossbones (dpy, window, draw_gc, x, y, w, h)
     Display *dpy;
     Window window;
     GC draw_gc;
     int x, y, w, h;
{
  double xscale = w / 440.0;
  double yscale = h / 216.0;
  XPoint points [6];
  points [0].x = x + xscale * 20;
  points [0].y = y + yscale * 10;
  points [1].x = x + xscale * 120;
  points [1].y = y + yscale * 10;
  points [2].x = x + xscale * 243;
  points [2].y = y + yscale * 93;
  points [3].x = x + xscale * 57;
  points [3].y = y + yscale * 210;
  points [4].x = x + xscale * 20;
  points [4].y = y + yscale * 210;
  points [5].x = x + xscale * 175;
  points [5].y = y + yscale * 113;
  XFillPolygon (dpy, window, draw_gc, points, 6, Complex, CoordModeOrigin);
  points [0].x = x + xscale * 197;
  points [0].y = y + yscale * 127;
  points [1].x = x + xscale * 384;
  points [1].y = y + yscale * 10;
  points [2].x = x + xscale * 420;
  points [2].y = y + yscale * 10;
  points [3].x = x + xscale * 265;
  points [3].y = y + yscale * 108;
  points [4].x = x + xscale * 420;
  points [4].y = y + yscale * 210;
  points [5].x = x + xscale * 320;
  points [5].y = y + yscale * 210;
  XFillPolygon (dpy, window, draw_gc, points, 6, Complex, CoordModeOrigin);
}


void
skull (dpy, window, draw_gc, erase_gc, x, y, w, h)
     Display *dpy;
     Window window;
     GC draw_gc, erase_gc;
     int x, y, w, h;
{
  XPoint points [3];
  crossbones (dpy, window, draw_gc, x, y+h/2, w, h/2);
  x += w/100;
  y += h/15;
  XFillArc (dpy, window, draw_gc, (int) (x + (w * 0.3)), y, w/2, h/2,
	    -40*64, 260*64);
  XFillRectangle (dpy, window, draw_gc, (int) (x + (w * 0.35)), y + h/5,
		  (int) (w * 0.4), h/5);
  XFillRectangle (dpy, window, draw_gc, (int) (x + (w * 0.375)),
		  (int) (y + (h * 0.425)), w / 20, h / 20);
  XFillRectangle (dpy, window, draw_gc, (int) (x + (w * 0.495)),
		  (int) (y + (h * 0.425)), w / 20, h / 20);
  XFillRectangle (dpy, window, draw_gc, (int) (x + (w * 0.555)),
		  (int) (y + (h * 0.425)), w / 20, h / 20);
  XFillRectangle (dpy, window, draw_gc, (int) (x + (w * 0.675)),
		  (int) (y + (h * 0.425)), w / 20, h / 20);
  points [0].x = x + (w * 0.435);
  points [0].y = y + (h * 0.425);
  points [1].x = x + (w * 0.485);
  points [1].y = points [0].y;
  points [2].x = (points [0].x + points [1].x) / 2;
  points [2].y = points [0].y + h/10;
  XFillPolygon (dpy, window, draw_gc, points, 3, Complex, CoordModeOrigin);
  points [0].x = x + (w * 0.615);
  points [1].x = x + (w * 0.665);
  points [2].x = (points [0].x + points [1].x) / 2;
  XFillPolygon (dpy, window, draw_gc, points, 3, Complex, CoordModeOrigin);
  points [0].x = x + (w * 0.52);
  points [0].y = y + (h * 0.35);
  points [1].x = points [0].x - (w * 0.05);
  points [1].y = points [0].y;
  points [2].x = points [0].x;
  points [2].y = points [0].y - (w * 0.10);
  XFillPolygon (dpy, window, erase_gc, points, 3, Complex, CoordModeOrigin);
  points [0].x = x + (w * 0.57);
  points [1].x = x + (w * 0.62);
  points [2].x = points [0].x;
  XFillPolygon (dpy, window, erase_gc, points, 3, Complex, CoordModeOrigin);
  XFillArc (dpy, window, erase_gc, x + ((int) (w * 0.375)), y + h/7,
	    w/10, h/10, 0, 360*64);
  XFillArc (dpy, window, erase_gc, x + ((int) (w * 0.615)), y + h/7,
	    w/10, h/10, 0, 360*64);
}
