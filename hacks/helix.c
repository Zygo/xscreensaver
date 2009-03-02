/* xscreensaver, Copyright (c) 1992 Jamie Zawinski <jwz@mcom.com>
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
#include <math.h>
#ifdef VMS
#define M_PI 3.14159265358979323846
#endif
#if __STDC__
#include <math.h>	/* for M_PI */
#endif

static double sins [360];
static double coss [360];

static GC draw_gc, erase_gc;
static unsigned int default_fg_pixel;

static void
init_helix (dpy, window)
     Display *dpy;
     Window window;
{
  int i;
  XGCValues gcv;
  XWindowAttributes xgwa;
  Colormap cmap;
  XGetWindowAttributes (dpy, window, &xgwa);
  cmap = xgwa.colormap;
  gcv.foreground = default_fg_pixel =
    get_pixel_resource ("foreground", "Foreground", dpy, cmap);
  draw_gc = XCreateGC (dpy, window, GCForeground, &gcv);
  gcv.foreground = get_pixel_resource ("background", "Background", dpy, cmap);
  erase_gc = XCreateGC (dpy, window, GCForeground, &gcv);

  for (i = 0; i < 360; i++)
    {
      sins [i] = sin ((((double) i) / 180.0) * M_PI);
      coss [i] = cos ((((double) i) / 180.0) * M_PI);
    }
}

static int
gcd (a, b)
     int a, b;
{
  while (b > 0)
    {
      int tmp;
      tmp = a % b;
      a = b;
      b = tmp;
    }
  return (a < 0 ? -a : a);
}

static void
helix (dpy, window,
       radius1, radius2, d_angle,
       factor1, factor2, factor3, factor4)
     Display *dpy;
     Window window;
     int radius1, radius2, d_angle;
     int factor1, factor2, factor3, factor4;
{
  XWindowAttributes xgwa;
  int width, height;
  int xmid, ymid;
  int x1, y1, x2, y2, angle, limit;
  int i;

  XClearWindow (dpy, window);
  XGetWindowAttributes (dpy, window, &xgwa);
  width = xgwa.width;
  height = xgwa.height;

  xmid = width / 2;
  ymid = height / 2;
  x1 = xmid;
  y1 = ymid + radius2;
  x2 = xmid;
  y2 = ymid + radius1;
  angle = 0;
  limit = 1 + (360 / gcd (360, d_angle));
  
  for (i = 0; i < limit; i++)
    {
      int tmp;
#define pmod(x,y) (tmp = (x % y), (tmp >= 0 ? tmp : tmp + y))
      x1 = xmid + (((double) radius1) * sins [pmod ((angle * factor1), 360)]);
      y1 = ymid + (((double) radius2) * coss [pmod ((angle * factor2), 360)]);
      XDrawLine (dpy, window, draw_gc, x1, y1, x2, y2);
      x2 = xmid + (((double) radius2) * sins [pmod ((angle * factor3), 360)]);
      y2 = ymid + (((double) radius1) * coss [pmod ((angle * factor4), 360)]);
      XDrawLine (dpy, window, draw_gc, x1, y1, x2, y2);
      angle += d_angle;
      XFlush (dpy);
    }
}

#define min(a,b) ((a)<(b)?(a):(b))

static void
random_helix (dpy, window)
     Display *dpy;
     Window window;
{
  Colormap cmap;
  int width, height;
  int radius, radius1, radius2, d_angle, factor1, factor2, factor3, factor4;
  double divisor;
  XColor color;
  int i, got_color = 0;
  XWindowAttributes xgwa;
  XGetWindowAttributes (dpy, window, &xgwa);
  width = xgwa.width;
  height = xgwa.height;
  cmap = xgwa.colormap;

  radius = min (width, height) / 2;

  d_angle = 0;
  factor1 = 2;
  factor2 = 2;
  factor3 = 2;
  factor4 = 2;

  divisor = ((frand (3.0) + 1) * (((random() & 1) * 2) - 1));

  if ((random () & 1) == 0)
    {
      radius1 = radius;
      radius2 = radius / divisor;
    }
  else
    {
      radius2 = radius;
      radius1 = radius / divisor;
    }

  while (gcd (360, d_angle) >= 2)
    d_angle = random () % 360;

#define random_factor()				\
  (((random() % 7) ? ((random() & 1) + 1) : 3)	\
   * (((random() & 1) * 2) - 1))

  while (gcd (gcd (gcd (factor1, factor2), factor3), factor4) != 1)
    {
      factor1 = random_factor ();
      factor2 = random_factor ();
      factor3 = random_factor ();
      factor4 = random_factor ();
    }

  if (mono_p)
    XSetForeground (dpy, draw_gc, default_fg_pixel);
  else
    {
      hsv_to_rgb (random () % 360, frand (1.0), frand (0.5) + 0.5,
		  &color.red, &color.green, &color.blue);
      if ((got_color = XAllocColor (dpy, cmap, &color)))
	XSetForeground (dpy, draw_gc, color.pixel);
      else
	XSetForeground (dpy, draw_gc, default_fg_pixel);
    }
  helix (dpy, window, radius1, radius2, d_angle,
	 factor1, factor2, factor3, factor4);

  XSync (dpy, True);
  sleep (5);

  for (i = 0; i < height; i++)
    {
      int y = (random () % height);
      XDrawLine (dpy, window, erase_gc, 0, y, width, y);
      XFlush (dpy);
      if ((i % 50) == 0)
	usleep (10000);
    }
  XClearWindow (dpy, window);
  if (got_color) XFreeColors (dpy, cmap, &color.pixel, 1, 0);
  XSync (dpy, True);
  sleep (1);
}


char *progclass = "Helix";

char *defaults [] = {
  "Helix.background: black",		/* to placate SGI */
  0
};

XrmOptionDescRec options [] = { 0 };
int options_size = 0;

void
screenhack (dpy, window)
     Display *dpy;
     Window window;
{
  init_helix (dpy, window);
  while (1)
    random_helix (dpy, window);
}
