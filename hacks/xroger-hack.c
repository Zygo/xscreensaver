/* xscreensaver, Copyright (c) 1991-1994 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include "screenhack.h"

char *progclass = "XRoger";

char *defaults [] = {
  ".background:		black",
  ".foreground:		red",
  "*delay:		5",
  0
};

XrmOptionDescRec options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

extern void skull (Display *, Window, GC, GC, int, int, int, int);

void
screenhack (dpy, window)
     Display *dpy;
     Window window;
{
  double delta = 0.005;
  XGCValues gcv;
  Colormap cmap;
  GC draw_gc, erase_gc;
  unsigned int fg;
  XColor color, color2, color3;
  int delay = get_integer_resource ("delay", "Integer");
  XWindowAttributes xgwa;
  XGetWindowAttributes (dpy, window, &xgwa);
  cmap = xgwa.colormap;
  gcv.foreground = get_pixel_resource ("background", "Background", dpy, cmap);
  erase_gc = XCreateGC (dpy, window, GCForeground, &gcv);
  fg = get_pixel_resource ("foreground", "Foreground", dpy, cmap);
  if (fg == gcv.foreground)
    fg = ((gcv.foreground == WhitePixel (dpy, DefaultScreen (dpy)))
	  ? BlackPixel (dpy, DefaultScreen (dpy))
	  : WhitePixel (dpy, DefaultScreen (dpy)));
  gcv.foreground = fg;
  draw_gc = XCreateGC (dpy, window, GCForeground, &gcv);
  color.pixel = gcv.foreground;
  XQueryColor (dpy, cmap, &color);
  while (1)
    {
      int w, h, ww, hh, x, y;
      time_t start_time;
      XGetWindowAttributes (dpy, window, &xgwa);
      w = xgwa.width;
      h = xgwa.height;

      ww = 100 + random () % (w - 100);
      hh = 100 + random () % (h - 100);
      if (ww < 10) ww = 50;
      if (hh < 10) hh = 50;
      if (ww < hh) hh = ww;
      else ww = hh;
      x = random () % (w - ww);
      y = random () % (h - hh);
      XClearWindow (dpy, window);


      skull (dpy, window, draw_gc, erase_gc, x, y, ww, hh);
      XSync (dpy, False);
      screenhack_handle_events (dpy);
      start_time = time ((time_t *) 0);
      if (mono_p)
	sleep (delay);
      else
	while (start_time + delay > time ((time_t *) 0))
	  {
	    int H;
	    double S, V;
	    color2 = color;
	    rgb_to_hsv (color2.red, color2.green, color2.blue, &H, &S, &V);
	    V += delta;
	    if (V >= 1.0) V = 1.0, delta = -delta;
	    if (V <= 0.6) V = 0.7, delta = -delta;
	    hsv_to_rgb (H, S, V, &color2.red, &color2.green, &color2.blue);
	    color3 = color2;
	    if (XAllocColor (dpy, cmap, &color3))
	      {
		XSetForeground (dpy, draw_gc, color3.pixel);
		color2.pixel = color3.pixel;
		XFreeColors (dpy, cmap, &color.pixel, 1, 0);
	      }
	    color = color2;
	    usleep (20000);
	  }
    }
}
