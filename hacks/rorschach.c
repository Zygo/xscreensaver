/* xscreensaver, Copyright (c) 1992-2014 Jamie Zawinski <jwz@jwz.org>
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

struct state {
  GC draw_gc;
  unsigned int default_fg_pixel;
  int iterations, offset;
  Bool xsym, ysym;
  int sleep_time;
  int xlim, ylim;
  int scale;
  XColor color;
  int current_x, current_y, remaining_iterations;
  eraser_state *eraser;
};


static void *
rorschach_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;
  Colormap cmap;
  XWindowAttributes xgwa;
  XGetWindowAttributes (dpy, window, &xgwa);

  st->scale = 1;
  if (xgwa.width > 2560 || xgwa.height > 2560)
    st->scale *= 3;  /* Retina displays */


  cmap = xgwa.colormap;
  gcv.foreground = st->default_fg_pixel =
    get_pixel_resource (dpy, cmap, "foreground", "Foreground");
  st->draw_gc = XCreateGC (dpy, window, GCForeground, &gcv);
  gcv.foreground = get_pixel_resource (dpy, cmap, "background", "Background");
  st->iterations = get_integer_resource (dpy, "iterations", "Integer");
  st->offset = get_integer_resource (dpy, "offset", "Integer");
  if (st->offset <= 0) st->offset = 3;
  if (st->iterations < 10) st->iterations = 10;
  st->sleep_time = get_integer_resource (dpy, "delay", "Delay");
  st->xsym = get_boolean_resource (dpy, "xsymmetry", "Symmetry");
  st->ysym = get_boolean_resource (dpy, "ysymmetry", "Symmetry");
  st->remaining_iterations = -1;
  st->color.pixel = 0;
  return st;
}

static void
rorschach_reshape (Display *dpy, Window window, void *closure,
                   unsigned int width, unsigned int height)
{
  struct state *st = (struct state *) closure;
  st->xlim = width;
  st->ylim = height;
}


static void
rorschach_draw_start (Display *dpy, Window window, struct state *st)
{
  Colormap cmap;
  XWindowAttributes xgwa;

  XGetWindowAttributes (dpy, window, &xgwa);
  st->xlim = xgwa.width;
  st->ylim = xgwa.height;
  cmap = xgwa.colormap;

  if (st->color.pixel) XFreeColors (dpy, cmap, &st->color.pixel, 1, 0);

  if (! mono_p)
    hsv_to_rgb (random()%360, 1.0, 1.0, &st->color.red, &st->color.green, &st->color.blue);
  if ((!mono_p) && XAllocColor (dpy, cmap, &st->color))
    XSetForeground (dpy, st->draw_gc, st->color.pixel);
  else
    XSetForeground (dpy, st->draw_gc, st->default_fg_pixel);

  st->current_x = st->xlim/2;
  st->current_y = st->ylim/2;
  st->remaining_iterations = st->iterations;
}


static void
rorschach_draw_step (Display *dpy, Window window, struct state *st)
{
# define ITER_CHUNK 300
  XRectangle points [4 * ITER_CHUNK];
  int x = st->current_x;
  int y = st->current_y;
  int i, j = 0;

  int this_iterations = ITER_CHUNK;
  if (this_iterations > st->remaining_iterations)
    this_iterations = st->remaining_iterations;

  for (i = 0; i < this_iterations; i++)
    {
      x += ((random () % (1 + (st->offset << 1))) - st->offset);
      y += ((random () % (1 + (st->offset << 1))) - st->offset);
      points [j].x = x;
      points [j].y = y;
      points[j].width = points[j].height = st->scale;
      j++;
      if (st->xsym)
	{
	  points [j].x = st->xlim - x;
	  points [j].y = y;
          points[j].width = points[j].height = st->scale;
	  j++;
	}
      if (st->ysym)
	{
	  points [j].x = x;
	  points [j].y = st->ylim - y;
          points[j].width = points[j].height = st->scale;
	  j++;
	}
      if (st->xsym && st->ysym)
	{
	  points [j].x = st->xlim - x;
	  points [j].y = st->ylim - y;
          points[j].width = points[j].height = st->scale;
	  j++;
	}
    }
  XFillRectangles (dpy, window, st->draw_gc, points, j);
  st->remaining_iterations -= this_iterations;
  if (st->remaining_iterations < 0) st->remaining_iterations = 0;
  st->current_x = x;
  st->current_y = y;
}


static unsigned long
rorschach_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  unsigned long delay = 20000;

  if (st->eraser) {
    st->eraser = erase_window (dpy, window, st->eraser);
    goto END;
  }

  if (st->remaining_iterations > 0)
    {
      rorschach_draw_step (dpy, window, st);
      if (st->remaining_iterations == 0)
        delay = st->sleep_time * 1000000;
    }
  else
    {
      if (st->remaining_iterations == 0)
        st->eraser = erase_window (dpy, window, st->eraser);

      rorschach_draw_start (dpy, window, st);
    }
 END:
  return delay;
}


static Bool
rorschach_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (screenhack_event_helper (dpy, window, event))
    {
      st->remaining_iterations = 0;
      return True;
    }
  return False;
}

static void
rorschach_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  XFreeGC (dpy, st->draw_gc);
  free (st);
}


static const char *rorschach_defaults [] = {
  ".background:	black",
  ".foreground:	white",
  "*fpsSolid:	true",
  "*xsymmetry:	true",
  "*ysymmetry:	false",
  "*iterations:	4000",
  "*offset:	7",
  "*delay:	5",
#ifdef HAVE_MOBILE
  "*ignoreRotation: True",
#endif
  0
};

static XrmOptionDescRec rorschach_options [] = {
  { "-iterations",	".iterations",	XrmoptionSepArg, 0 },
  { "-offset",		".offset",	XrmoptionSepArg, 0 },
  { "-xsymmetry",	".xsymmetry",	XrmoptionNoArg, "true" },
  { "-ysymmetry",	".ysymmetry",	XrmoptionNoArg, "true" },
  { "-no-xsymmetry",	".xsymmetry",	XrmoptionNoArg, "false" },
  { "-no-ysymmetry",	".ysymmetry",	XrmoptionNoArg, "false" },
  { "-delay",           ".delay",               XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Rorschach", rorschach)
