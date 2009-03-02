/* xscreensaver, Copyright (c) 1997 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Concept snarfed from Michael D. Bayne in
 * http://www.go2net.com/internet/deep/1997/04/16/body.html
 */

#include "screenhack.h"
#include <stdio.h>

static XColor colors[255];
static int ncolors = 0;
static int max_depth = 0;
static int min_height = 0;
static int min_width = 0;

static void
deco (Display *dpy,
      Window window,
      Colormap cmap,
      GC fgc, GC bgc,
      int x, int y, int w, int h, int depth)
{
  if (((random() % max_depth) < depth) || (w < min_width) || (h < min_height))
    {
      if (!mono_p)
	{
	  static int current_color = 0;
	  if (++current_color >= ncolors)
	    current_color = 0;
	  XSetForeground(dpy, bgc, colors[current_color].pixel);
	}
      XFillRectangle (dpy, window, bgc, x, y, w, h);
      XDrawRectangle (dpy, window, fgc, x, y, w, h);
    }
  else
    {
      if (random() & 1)
	{
	  deco (dpy, window, cmap, fgc, bgc, x, y, w/2, h, depth+1);
	  deco (dpy, window, cmap, fgc, bgc, x+w/2, y, w/2, h, depth+1);
	}
      else
	{
	  deco (dpy, window, cmap, fgc, bgc, x, y, w, h/2, depth+1);
	  deco (dpy, window, cmap, fgc, bgc, x, y+h/2, w, h/2, depth+1);
	}
    }
}


char *progclass = "Deco";

char *defaults [] = {
  ".background:		black",
  ".foreground:		white",
  "*maxDepth:		12",
  "*minWidth:		20",
  "*minHeight:		20",
  "*cycle:		False",
  "*delay:		5",
  "*cycleDelay:		1000000",
  "*ncolors:		64",
  0
};

XrmOptionDescRec options [] = {
  { "-max-depth",	".maxDepth",	XrmoptionSepArg, 0 },
  { "-min-width",	".minWidth",	XrmoptionSepArg, 0 },
  { "-min-height",	".minHeight",	XrmoptionSepArg, 0 },
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-ncolors",		".ncolors",	XrmoptionSepArg, 0 },
  { "-cycle",		".cycle",	XrmoptionNoArg, "True" },
  { "-no-cycle",	".cycle",	XrmoptionNoArg, "False" },
  { "-cycle-delay",	".cycleDelay",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

void
screenhack (Display *dpy, Window window)
{
  GC fgc, bgc;
  XGCValues gcv;
  XWindowAttributes xgwa;
  int delay = get_integer_resource ("delay", "Integer");
  int cycle_delay = get_integer_resource ("cycleDelay", "Integer");
  Bool writable = get_boolean_resource ("cycle", "Boolean");

  max_depth = get_integer_resource ("maxDepth", "Integer");
  if (max_depth < 1) max_depth = 1;
  else if (max_depth > 1000) max_depth = 1000;

  min_width = get_integer_resource ("minWidth", "Integer");
  if (min_width < 2) min_width = 2;
  min_height = get_integer_resource ("minHeight", "Integer");
  if (min_height < 2) min_height = 2;

  XGetWindowAttributes (dpy, window, &xgwa);

  gcv.foreground = get_pixel_resource("foreground", "Foreground",
				      dpy, xgwa.colormap);
  fgc = XCreateGC (dpy, window, GCForeground, &gcv);

  gcv.foreground = get_pixel_resource("background", "Background",
				      dpy, xgwa.colormap);
  bgc = XCreateGC (dpy, window, GCForeground, &gcv);

  ncolors = get_integer_resource ("ncolors", "Integer");

  make_random_colormap (dpy, xgwa.visual, xgwa.colormap, colors, &ncolors,
			False, True, &writable, True);

  if (ncolors <= 2)
    mono_p = True;

  if (!mono_p)
    {
      GC tmp = fgc;
      fgc = bgc;
      bgc = tmp;
    }

  while (1)
    {
      XGetWindowAttributes (dpy, window, &xgwa);
      XFillRectangle(dpy, window, bgc, 0, 0, xgwa.width, xgwa.height);
      deco (dpy, window, xgwa.colormap, fgc, bgc,
	    0, 0, xgwa.width, xgwa.height, 0);
      XSync (dpy, True);

      if (!delay) continue;
      if (!writable)
	sleep (delay);
      else
	{
	  time_t start = time((time_t) 0);
	  while (start - delay < time((time_t) 0))
	    {
	      rotate_colors (dpy, xgwa.colormap, colors, ncolors, 1);
	      if (cycle_delay)
		usleep (cycle_delay);
	    }
	}
    }
}
