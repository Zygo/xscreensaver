/* xscreensaver, Copyright (c) 1997, 1998, 2002, 2006
 *  Jamie Zawinski <jwz@jwz.org>
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

struct state {
  XColor colors[255];
  int ncolors;
  int max_depth;
  int min_height;
  int min_width;
  int delay;
  XWindowAttributes xgwa;
  GC fgc, bgc;
  int current_color;
};

static void
deco (Display *dpy, Window window, struct state *st,
      int x, int y, int w, int h, int depth)
{
  if (((random() % st->max_depth) < depth) || (w < st->min_width) || (h < st->min_height))
    {
      if (!mono_p)
	{
	  if (++st->current_color >= st->ncolors)
	    st->current_color = 0;
	  XSetForeground(dpy, st->bgc, st->colors[st->current_color].pixel);
	}
      XFillRectangle (dpy, window, st->bgc, x, y, w, h);
      XDrawRectangle (dpy, window, st->fgc, x, y, w, h);
    }
  else
    {
      if (random() & 1)
	{
	  deco (dpy, window, st, x, y, w/2, h, depth+1);
	  deco (dpy, window, st, x+w/2, y, w/2, h, depth+1);
	}
      else
	{
	  deco (dpy, window, st, x, y, w, h/2, depth+1);
	  deco (dpy, window, st, x, y+h/2, w, h/2, depth+1);
	}
    }
}

static void *
deco_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;

  st->delay = get_integer_resource (dpy, "delay", "Integer");

  st->max_depth = get_integer_resource (dpy, "maxDepth", "Integer");
  if (st->max_depth < 1) st->max_depth = 1;
  else if (st->max_depth > 1000) st->max_depth = 1000;

  st->min_width = get_integer_resource (dpy, "minWidth", "Integer");
  if (st->min_width < 2) st->min_width = 2;
  st->min_height = get_integer_resource (dpy, "minHeight", "Integer");
  if (st->min_height < 2) st->min_height = 2;

  XGetWindowAttributes (dpy, window, &st->xgwa);

  gcv.foreground = get_pixel_resource(dpy, st->xgwa.colormap,
                                      "foreground", "Foreground");
  st->fgc = XCreateGC (dpy, window, GCForeground, &gcv);

  gcv.foreground = get_pixel_resource(dpy, st->xgwa.colormap,
                                      "background", "Background");
  st->bgc = XCreateGC (dpy, window, GCForeground, &gcv);

  st->ncolors = get_integer_resource (dpy, "ncolors", "Integer");

  make_random_colormap (dpy, st->xgwa.visual, st->xgwa.colormap, st->colors, &st->ncolors,
			False, True, 0, True);

  if (st->ncolors <= 2)
    mono_p = True;

  if (!mono_p)
    {
      GC tmp = st->fgc;
      st->fgc = st->bgc;
      st->bgc = tmp;
    }

  return st;
}

static unsigned long
deco_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  XFillRectangle (dpy, window, st->bgc, 0, 0, st->xgwa.width, st->xgwa.height);
  deco (dpy, window, st, 0, 0, st->xgwa.width, st->xgwa.height, 0);
  return 1000000 * st->delay;
}

static void
deco_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  st->xgwa.width = w;
  st->xgwa.height = h;
}

static Bool
deco_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
deco_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}


static const char *deco_defaults [] = {
  ".background:		black",
  ".foreground:		white",
  "*maxDepth:		12",
  "*minWidth:		20",
  "*minHeight:		20",
  "*delay:		5",
  "*ncolors:		64",
  0
};

static XrmOptionDescRec deco_options [] = {
  { "-max-depth",	".maxDepth",	XrmoptionSepArg, 0 },
  { "-min-width",	".minWidth",	XrmoptionSepArg, 0 },
  { "-min-height",	".minHeight",	XrmoptionSepArg, 0 },
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-ncolors",		".ncolors",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Deco", deco)
