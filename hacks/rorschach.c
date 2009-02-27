/* xscreensaver, Copyright (c) 1992, 1996 Jamie Zawinski <jwz@netscape.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * 19971004: Johannes Keukelaar <johannes@nada.kth.se>: Use helix screen
 *           eraser.
 */

#include "screenhack.h"
#include "erase.h"

static GC draw_gc, erase_gc;
static unsigned int default_fg_pixel;
static int iterations, offset;
static Bool xsym, ysym;
static int erase_speed, sleep_time, erase_mode;

void erase_window (Display *dpy, Window win, GC gc, int width, int height,
		   int mode, int delay);

static void
init_rorschach (Display *dpy, Window window)
{
  XGCValues gcv;
  Colormap cmap;
  XWindowAttributes xgwa;
  XGetWindowAttributes (dpy, window, &xgwa);
  cmap = xgwa.colormap;
  gcv.foreground = default_fg_pixel =
    get_pixel_resource ("foreground", "Foreground", dpy, cmap);
  draw_gc = XCreateGC (dpy, window, GCForeground, &gcv);
  gcv.foreground = get_pixel_resource ("background", "Background", dpy, cmap);
  erase_gc = XCreateGC (dpy, window, GCForeground, &gcv);
  iterations = get_integer_resource ("iterations", "Integer");
  offset = get_integer_resource ("offset", "Integer");
  if (offset <= 0) offset = 3;
  if (iterations < 10) iterations = 10;
  xsym = get_boolean_resource ("xsymmetry", "Symmetry");
  ysym = get_boolean_resource ("ysymmetry", "Symmetry");
}

static void
hurm (Display *dpy, Window window)
{
  Colormap cmap;
  XWindowAttributes xgwa;
  int xlim, ylim, x, y, i, got_color = 0;
  XPoint points [4];
  XColor color;
  XClearWindow (dpy, window);
  XGetWindowAttributes (dpy, window, &xgwa);
  xlim = xgwa.width;
  ylim = xgwa.height;
  cmap = xgwa.colormap;

  if (! mono_p)
    hsv_to_rgb (random()%360, 1.0, 1.0, &color.red, &color.green, &color.blue);
  if ((!mono_p) && (got_color = XAllocColor (dpy, cmap, &color)))
    XSetForeground (dpy, draw_gc, color.pixel);
  else
    XSetForeground (dpy, draw_gc, default_fg_pixel);

  x = xlim/2;
  y = ylim/2;
  for (i = 0; i < iterations; i++)
    {
      int j = 0;
      x += ((random () % (1 + (offset << 1))) - offset);
      y += ((random () % (1 + (offset << 1))) - offset);
      points [j].x = x;
      points [j].y = y;
      j++;
      if (xsym)
	{
	  points [j].x = xlim - x;
	  points [j].y = y;
	  j++;
	}
      if (ysym)
	{
	  points [j].x = x;
	  points [j].y = ylim - y;
	  j++;
	}
      if (xsym && ysym)
	{
	  points [j].x = xlim - x;
	  points [j].y = ylim - y;
	  j++;
	}
      XDrawPoints (dpy, window, draw_gc, points, j, CoordModeOrigin);
      XSync (dpy, True);
    }
  sleep ( sleep_time );

  erase_window(dpy, window, erase_gc, xlim, ylim, erase_mode, erase_speed);

  XClearWindow (dpy, window);
  if (got_color) XFreeColors (dpy, cmap, &color.pixel, 1, 0);
  XSync (dpy, True);
  sleep (1);
}


char *progclass = "Rorschach";

char *defaults [] = {
  "Rorschach.background:	black",		/* to placate SGI */
  "Rorschach.foreground:	white",
  "*xsymmetry:	true",
  "*ysymmetry:	false",
  "*iterations:	4000",
  "*offset:	4",
  "Rorschach.eraseSpeed: 400",
  "Rorschach.delay: 5",
  "Rorschach.eraseMode: -1",
  0
};

XrmOptionDescRec options [] = {
  { "-iterations",	".iterations",	XrmoptionSepArg, 0 },
  { "-offset",		".offset",	XrmoptionSepArg, 0 },
  { "-xsymmetry",	".xsymmetry",	XrmoptionNoArg, "true" },
  { "-ysymmetry",	".ysymmetry",	XrmoptionNoArg, "true" },
  { "-erase-speed",	".eraseSpeed",		XrmoptionSepArg, 0 },
  { "-delay",           ".delay",               XrmoptionSepArg, 0 },
  { "-erase-mode",      ".eraseMode",           XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

void
screenhack (Display *dpy, Window window)
{
  erase_speed = get_integer_resource("eraseSpeed", "Integer");
  sleep_time = get_integer_resource("delay", "Integer");
  erase_mode = get_integer_resource("eraseMode", "Integer");
  init_rorschach (dpy, window);
  while (1)
    hurm (dpy, window);
}
