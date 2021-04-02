/* xscreensaver, Copyright (c) 1997-2013 Jamie Zawinski <jwz@jwz.org>
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
 *
 * Changes by Lars Huttar, http://www.huttar.net:
 * - allow use of golden ratio for dividing rectangles instead of 1/2.
 * - allow smooth colors instead of random
 * - added line thickness setting
 * - added "Mondrian" mode
 * Other ideas:
 * - allow recomputing the colormap on each new frame (especially useful
 *   when ncolors is low)
 */

#include "screenhack.h"
#include <stdio.h>

struct state {
  XColor colors[255];
  int ncolors;
  int max_depth;
  int min_height;
  int min_width;
  int line_width;
  int old_line_width;
  Bool goldenRatio;
  Bool mondrian;
  Bool smoothColors;

  int delay;
  XWindowAttributes xgwa;
  GC fgc, bgc;
  int current_color;
};

/* Golden Ratio
 *   Suppose you're dividing a rectangle of length A+B
 *   into two parts, of length A and B respectively. You want the ratio of
 *   A to B to be the same as the ratio of the whole (A+B) to A. The golden
 *   ratio (phi) is that ratio. Supposed to be visually pleasing. */
#define PHI 1.61803
#define PHI1 (1.0/PHI)
#define PHI2 (1.0 - PHI1)

/* copied from make_random_colormap in colors.c */
static void
make_mondrian_colormap (Screen *screen, Visual *visual, Colormap cmap,
		      XColor *colors, int *ncolorsP,
		      Bool allocate_p,
		      Bool *writable_pP,
		      Bool verbose_p)
{
  Display *dpy = DisplayOfScreen (screen);
  Bool wanted_writable = (allocate_p && writable_pP && *writable_pP);
  int ncolors = 8;
  int i;

  if (*ncolorsP <= 0) return;

  /* If this visual doesn't support writable cells, don't bother trying. */
  if (wanted_writable && !has_writable_cells(screen, visual))
    *writable_pP = False;

  for (i = 0; i < ncolors; i++)
  {
      colors[i].flags = DoRed|DoGreen|DoBlue;
      colors[i].red = 0;
      colors[i].green = 0;
      colors[i].blue = 0;

      switch(i) {
	  case 0: case 1: case 2: case 3: case 7: /* white */
	      colors[i].red = 0xE800;
	      colors[i].green = 0xE800;
	      colors[i].blue  = 0xE800;
	      break;
	  case 4:
	      colors[i].red = 0xCFFF; break; /* red */
	  case 5:
	      colors[i].red = 0x2000;
	      colors[i].blue = 0xCFFF; break; /* blue */
	  case 6:
	      colors[i].red = 0xDFFF; /* yellow */
	      colors[i].green = 0xCFFF; break;
      }
  }

  if (!allocate_p)
    return;

 RETRY_NON_WRITABLE:
  if (writable_pP && *writable_pP)
    {
      unsigned long *pixels = (unsigned long *)
	malloc(sizeof(*pixels) * (ncolors + 1));

      allocate_writable_colors (screen, cmap, pixels, &ncolors);
      if (ncolors > 0)
	for (i = 0; i < ncolors; i++)
	  colors[i].pixel = pixels[i];
      free (pixels);
      if (ncolors > 0)
	XStoreColors (dpy, cmap, colors, ncolors);
    }
  else
    {
      for (i = 0; i < ncolors; i++)
	{
	  XColor color;
	  color = colors[i];
	  if (!XAllocColor (dpy, cmap, &color))
	    break;
	  colors[i].pixel = color.pixel;
	}
      ncolors = i;
    }

  /* If we tried for writable cells and got none, try for non-writable. */
  if (allocate_p && ncolors == 0 && writable_pP && *writable_pP)
    {
      ncolors = *ncolorsP;
      *writable_pP = False;
      goto RETRY_NON_WRITABLE;
    }

#if 0
  /* I don't think we need to bother copying or linking to the complain
     function. */
  if (verbose_p)
    complain(*ncolorsP, ncolors, wanted_writable,
	     wanted_writable && *writable_pP);
#endif

  *ncolorsP = ncolors;
}

static void
mondrian_set_sizes (struct state *st, int w, int h)
{
    if (w > h) {
	st->line_width = w/50;
	st->min_height = st->min_width = w/8;
    } else {
	st->line_width = h/50;
	st->min_height = st->min_width = h/8;
    }
}
 
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
      if ((st->goldenRatio || st->mondrian) ? (w > h) : (random() & 1))
        { /* Divide the rectangle side-by-side */
          int wnew = (st->goldenRatio ? (w * (random() & 1 ? PHI1 : PHI2)) : w/2);
	  deco (dpy, window, st, x, y, wnew, h, depth+1);
	  deco (dpy, window, st, x+wnew, y, w-wnew, h, depth+1);
	}
      else
        { /* Divide the rectangle top-to-bottom */
          int hnew = (st->goldenRatio ? (h * (random() & 1 ? PHI1 : PHI2)) : h/2);
	  deco (dpy, window, st, x, y, w, hnew, depth+1);
	  deco (dpy, window, st, x, y+hnew, w, h-hnew, depth+1);
	}
    }
}

static void *
deco_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;

  st->delay = get_integer_resource (dpy, "delay", "Integer");

  st->smoothColors = get_boolean_resource(dpy, "smoothColors", "Boolean");
  st->old_line_width = 1;

  st->goldenRatio = get_boolean_resource (dpy, "goldenRatio", "Boolean");

  st->max_depth = get_integer_resource (dpy, "maxDepth", "Integer");
  if (st->max_depth < 1) st->max_depth = 1;
  else if (st->max_depth > 1000) st->max_depth = 1000;

  st->min_width = get_integer_resource (dpy, "minWidth", "Integer");
  if (st->min_width < 2) st->min_width = 2;
  st->min_height = get_integer_resource (dpy, "minHeight", "Integer");
  if (st->min_height < 2) st->min_height = 2;

  st->line_width = get_integer_resource (dpy, "lineWidth", "Integer");

  XGetWindowAttributes (dpy, window, &st->xgwa);

  st->ncolors = get_integer_resource (dpy, "ncolors", "Integer");

  gcv.foreground = get_pixel_resource(dpy, st->xgwa.colormap,
                                      "foreground", "Foreground");
  st->fgc = XCreateGC (dpy, window, GCForeground, &gcv);

  gcv.foreground = get_pixel_resource(dpy, st->xgwa.colormap,
                                      "background", "Background");
  st->bgc = XCreateGC (dpy, window, GCForeground, &gcv);

  if (st->ncolors <= 2)
    mono_p = True;

  if (!mono_p)
    {
      GC tmp = st->fgc;
      st->fgc = st->bgc;
      st->bgc = tmp;
    }

  st->mondrian = get_boolean_resource(dpy, "mondrian", "Boolean");
  if (st->mondrian) {
      /* Mondrian, if true, overrides several other options. */
      mondrian_set_sizes(st, st->xgwa.width, st->xgwa.height);

      /** set up red-yellow-blue-black-white colormap and fgc **/
      make_mondrian_colormap(st->xgwa.screen, st->xgwa.visual,
                             st->xgwa.colormap,
			     st->colors, &st->ncolors, True, 0, True);

      /** put white in several cells **/
      /** set min-height and min-width to about 10% of total w/h **/
  }
  else if (st->smoothColors)
      make_smooth_colormap (st->xgwa.screen, st->xgwa.visual,
                            st->xgwa.colormap,
			    st->colors, &st->ncolors, True, 0, True);
  else
      make_random_colormap (st->xgwa.screen, st->xgwa.visual,
                            st->xgwa.colormap,
			    st->colors, &st->ncolors, False, True, 0, True);

  gcv.line_width = st->old_line_width = st->line_width;
  XChangeGC(dpy, st->fgc, GCLineWidth, &gcv);

  return st;
}

static unsigned long
deco_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  XFillRectangle (dpy, window, st->bgc, 0, 0, st->xgwa.width, st->xgwa.height);
  if (st->mondrian) {
      mondrian_set_sizes(st, st->xgwa.width, st->xgwa.height);
      if (st->line_width != st->old_line_width) {
	  XSetLineAttributes(dpy, st->fgc, st->line_width,
			     LineSolid, CapButt, JoinBevel);
	  st->old_line_width = st->line_width;
      }
  }
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
  XFreeGC (dpy, st->fgc);
  XFreeGC (dpy, st->bgc);
  free (st);
}


static const char *deco_defaults [] = {
  ".background:		black",
  ".foreground:		white",
  ".lowrez:		true",
  "*maxDepth:		12",
  "*minWidth:		20",
  "*minHeight:		20",
  "*lineWidth:          1",
  "*delay:		5",
  "*ncolors:		64",
  "*goldenRatio:        False",
  "*smoothColors:       False",
  "*mondrian:           False",
#ifdef HAVE_MOBILE
  "*ignoreRotation:     True",
#endif
  0
};

static XrmOptionDescRec deco_options [] = {
  { "-max-depth",	".maxDepth",	XrmoptionSepArg, 0 },
  { "-min-width",	".minWidth",	XrmoptionSepArg, 0 },
  { "-min-height",	".minHeight",	XrmoptionSepArg, 0 },
  { "-line-width",	".lineWidth",	XrmoptionSepArg, 0 },
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-ncolors",		".ncolors",	XrmoptionSepArg, 0 },
  { "-golden-ratio",    ".goldenRatio", XrmoptionNoArg, "True" },
  { "-no-golden-ratio", ".goldenRatio", XrmoptionNoArg, "False" },
  { "-smooth-colors",   ".smoothColors",XrmoptionNoArg, "True" },
  { "-no-smooth-colors",".smoothColors",XrmoptionNoArg, "False" },
  { "-mondrian",        ".mondrian",    XrmoptionNoArg, "True" },
  { "-no-mondrian",     ".mondrian",    XrmoptionNoArg, "False" },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Deco", deco)
