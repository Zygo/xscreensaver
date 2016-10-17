/* xscreensaver, Copyright (c) 1993-2013 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* I wanted to lay down new circles with TV:ALU-ADD instead of TV:ALU-XOR,
   but X doesn't support arithmetic combinations of pixmaps!!  What losers.
   I suppose I could crank out the 2's compliment math by hand, but that's
   a real drag...

   This would probably look good with shapes other than circles as well.

 */

#include "screenhack.h"
#include <stdio.h>

struct circle {
  int x, y, radius;
  int increment;
  int dx, dy;
};

static enum color_mode {
  seuss_mode, ramp_mode, random_mode
} cmode;


struct state {
  Display *dpy;
  Window window;

  struct circle *circles;
  int count, global_count;
  Pixmap pixmap, buffer;
  int width, height, global_inc;
  int delay, delay2;
  unsigned long fg_pixel, bg_pixel;
  GC draw_gc, erase_gc, copy_gc, merge_gc;
  Bool anim_p;
  Colormap cmap;

  int ncolors;
  XColor *colors;
  int fg_index;
  int bg_index;

  int iterations;
  Bool done_once;
  Bool done_once_no_really;
  int clear_tick;
  struct timeval then;
};

#define min(x,y) ((x)<(y)?(x):(y))
#define max(x,y) ((x)>(y)?(x):(y))

static void
init_circles_1 (struct state *st)
{
  int i;
  st->count = (st->global_count ? st->global_count
	   : (3 + (random () % max (1, (min (st->width, st->height) / 50)))
	        + (random () % max (1, (min (st->width, st->height) / 50)))));
  st->circles = (struct circle *) malloc (st->count * sizeof (struct circle));
  for (i = 0; i < st->count; i++)
    {
      st->circles [i].x = 10 + random () % (st->width - 20);
      st->circles [i].y = 10 + random () % (st->height - 20);
      if (st->global_inc)
      st->circles [i].increment = st->global_inc;
      else
	{ /* prefer smaller increments to larger ones */
	  int j = 8;
	  int inc = ((random()%j) + (random()%j) + (random()%j)) - ((j*3)/2);
	  if (inc < 0) inc = -inc + 3;
	  st->circles [i].increment = inc + 3;
	}
      st->circles [i].radius = random () % st->circles [i].increment;
      st->circles [i].dx = ((random () % 3) - 1) * (1 + random () % 5);
      st->circles [i].dy = ((random () % 3) - 1) * (1 + random () % 5);
    }
}

static void *
halo_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;
  XWindowAttributes xgwa;
  char *mode_str = 0;
  st->dpy = dpy;
  st->window = window;
  XGetWindowAttributes (st->dpy, st->window, &xgwa);
  st->cmap = xgwa.colormap;
  st->global_count = get_integer_resource (st->dpy, "count", "Integer");
  if (st->global_count < 0) st->global_count = 0;
  st->global_inc = get_integer_resource (st->dpy, "increment", "Integer");
  if (st->global_inc < 0) st->global_inc = 0;
  st->anim_p = get_boolean_resource (st->dpy, "animate", "Boolean");
  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  st->delay2 = get_integer_resource (st->dpy, "delay2", "Integer") * 1000000;
  mode_str = get_string_resource (st->dpy, "colorMode", "ColorMode");
  if (! mode_str) cmode = random_mode;
  else if (!strcmp (mode_str, "seuss"))  cmode = seuss_mode;
  else if (!strcmp (mode_str, "ramp"))   cmode = ramp_mode;
  else if (!strcmp (mode_str, "random")) cmode = random_mode;
  else {
    fprintf (stderr,
	     "%s: colorMode must be seuss, ramp, or random, not \"%s\"\n",
	     progname, mode_str);
    exit (1);
  }

  if (mono_p) cmode = seuss_mode;
  if (cmode == random_mode)
    cmode = ((random()&3) == 1) ? ramp_mode : seuss_mode;

  if (cmode == ramp_mode)
    st->anim_p = False;    /* This combo doesn't work right... */

  st->ncolors = get_integer_resource (st->dpy, "colors", "Colors");
  if (st->ncolors < 2) st->ncolors = 2;
  if (st->ncolors <= 2) mono_p = True;

  if (mono_p)
    st->colors  = 0;
  else
    st->colors = (XColor *) malloc(sizeof(*st->colors) * (st->ncolors+1));


  if (mono_p)
    ;
  else if (random() % (cmode == seuss_mode ? 2 : 10))
    make_uniform_colormap (xgwa.screen, xgwa.visual, st->cmap,
                           st->colors, &st->ncolors,
			   True, 0, True);
  else
    make_smooth_colormap (xgwa.screen, xgwa.visual, st->cmap,
                          st->colors, &st->ncolors,
			  True, 0, True);

  if (st->ncolors <= 2) mono_p = True;
  if (mono_p) cmode = seuss_mode;

  if (mono_p)
    {
      st->fg_pixel = get_pixel_resource (st->dpy, st->cmap, "foreground", "Foreground");
      st->bg_pixel = get_pixel_resource (st->dpy, st->cmap, "background", "Background");
    }
  else
    {
      st->fg_index = 0;
      st->bg_index = st->ncolors / 4;
      if (st->fg_index == st->bg_index) st->bg_index++;
      st->fg_pixel = st->colors[st->fg_index].pixel;
      st->bg_pixel = st->colors[st->bg_index].pixel;
    }

  st->width = max (50, xgwa.width);
  st->height = max (50, xgwa.height);

#ifdef DEBUG
  st->width/=2; st->height/=2;
#endif

  st->pixmap = XCreatePixmap (st->dpy, st->window, st->width, st->height, 1);
  if (cmode == seuss_mode)
    st->buffer = XCreatePixmap (st->dpy, st->window, st->width, st->height, 1);
  else
    st->buffer = 0;

  gcv.foreground = 1;
  gcv.background = 0;
  st->draw_gc = XCreateGC (st->dpy, st->pixmap, GCForeground | GCBackground, &gcv);
  gcv.foreground = 0;
  st->erase_gc = XCreateGC (st->dpy, st->pixmap, GCForeground, &gcv);
  gcv.foreground = st->fg_pixel;
  gcv.background = st->bg_pixel;
  st->copy_gc = XCreateGC (st->dpy, st->window, GCForeground | GCBackground, &gcv);

#ifdef HAVE_JWXYZ
  jwxyz_XSetAntiAliasing (dpy, st->draw_gc,  False);
  jwxyz_XSetAntiAliasing (dpy, st->erase_gc, False);
  jwxyz_XSetAntiAliasing (dpy, st->copy_gc,  False);
#endif

  if (cmode == seuss_mode)
    {
      gcv.foreground = 1;
      gcv.background = 0;
      gcv.function = GXxor;
      st->merge_gc = XCreateGC (st->dpy, st->pixmap,
			    GCForeground | GCBackground | GCFunction, &gcv);
    }
  else
    {
      gcv.foreground = st->fg_pixel;
      gcv.background = st->bg_pixel;
      gcv.function = GXcopy;
      st->merge_gc = XCreateGC (st->dpy, st->window,
			    GCForeground | GCBackground | GCFunction, &gcv);
    }

  init_circles_1 (st);
  XClearWindow (st->dpy, st->window);
  if (st->buffer) XFillRectangle (st->dpy, st->buffer, st->erase_gc, 0, 0, st->width, st->height);
  return st;
}

static unsigned long
halo_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int i;
  Bool done = False;
  Bool inhibit_sleep = False;
  int this_delay = st->delay;

  XFillRectangle (st->dpy, st->pixmap, st->erase_gc, 0, 0, st->width, st->height);
  for (i = 0; i < st->count; i++)
    {
      int radius = st->circles [i].radius;
      int inc = st->circles [i].increment;

      if (! (st->iterations & 1))  /* never stop on an odd number of iterations */
	;
      else if (radius == 0)	/* eschew inf */
	;
      else if (radius < 0)	/* stop when the circles are points */
	done = True;
      else			/* stop when the circles fill the st->window */
	{
	  /* Probably there's a simpler way to ask the musical question,
	     "is this square completely enclosed by this circle," but I've
	     forgotten too much trig to know it...  (That's not really the
	     right question anyway, but the right question is too hard.) */
	  double x1 = ((double) (-st->circles [i].x)) / ((double) radius);
	  double y1 = ((double) (-st->circles [i].y)) / ((double) radius);
	  double x2 = ((double) (st->width - st->circles [i].x)) / ((double) radius);
	  double y2 = ((double) (st->height - st->circles [i].y)) / ((double) radius);
	  x1 *= x1; x2 *= x2; y1 *= y1; y2 *= y2;
	  if ((x1 + y1) < 1 && (x2 + y2) < 1 && (x1 + y2) < 1 && (x2 + y1) < 1)
	    done = True;
	}

      if (radius > 0 &&
	  (cmode == seuss_mode ||	/* drawing all circles, or */
	   st->circles [0].increment < 0))	/* on the way back in */
	{
	  XFillArc (st->dpy,
		    (cmode == seuss_mode ? st->pixmap : st->window),
		    (cmode == seuss_mode ? st->draw_gc : st->merge_gc),
		    st->circles [i].x - radius, st->circles [i].y - radius,
		    radius * 2, radius * 2, 0, 360*64);
	}
      st->circles [i].radius += inc;
    }

  if (st->anim_p && !st->done_once)
    inhibit_sleep = !done;

  if (done)
    {
      if (st->anim_p)
	{
	  st->done_once = True;
	  for (i = 0; i < st->count; i++)
	    {
	      st->circles [i].x += st->circles [i].dx;
	      st->circles [i].y += st->circles [i].dy;
	      st->circles [i].radius %= st->circles [i].increment;
	      if (st->circles [i].x < 0 || st->circles [i].x >= st->width)
		{
		  st->circles [i].dx = -st->circles [i].dx;
		  st->circles [i].x += (2 * st->circles [i].dx);
		}
	      if (st->circles [i].y < 0 || st->circles [i].y >= st->height)
		{
		  st->circles [i].dy = -st->circles [i].dy;
		  st->circles [i].y += (2 * st->circles [i].dy);
		}
	    }
	}
      else if (st->circles [0].increment < 0)
	{
	  /* We've zoomed out and the screen is blank -- re-pick the
	     center points, and shift the st->colors.
	   */
	  free (st->circles);
	  init_circles_1 (st);
	  if (! mono_p)
	    {
	      st->fg_index = (st->fg_index + 1) % st->ncolors;
	      st->bg_index = (st->fg_index + (st->ncolors/2)) % st->ncolors;
	      XSetForeground (st->dpy, st->copy_gc, st->colors [st->fg_index].pixel);
	      XSetBackground (st->dpy, st->copy_gc, st->colors [st->bg_index].pixel);
	    }
	}
      /* Sometimes go out from the inside instead of the outside */
      else if (st->clear_tick == 0 && ((random () % 3) == 0))
	{
	  st->iterations = 0; /* ick */
	  for (i = 0; i < st->count; i++)
	    st->circles [i].radius %= st->circles [i].increment;

          st->clear_tick = ((random() % 8) + 4) | 1;   /* must be odd */
	}
      else
	{
	  for (i = 0; i < st->count; i++)
	    {
	      st->circles [i].increment = -st->circles [i].increment;
	      st->circles [i].radius += (2 * st->circles [i].increment);
	    }
	}
    }

  if (st->buffer)
    XCopyPlane (st->dpy, st->pixmap, st->buffer, st->merge_gc, 0, 0, st->width, st->height, 0, 0, 1);
  else if (cmode != seuss_mode)
    {

      if (!mono_p)
	{
	  st->fg_index++;
	  st->bg_index++;
	  if (st->fg_index >= st->ncolors) st->fg_index = 0;
	  if (st->bg_index >= st->ncolors) st->bg_index = 0;
	  XSetForeground (st->dpy, st->merge_gc, st->colors [st->fg_index].pixel);
	}

      if (st->circles [0].increment >= 0)
	inhibit_sleep = True;
      else if (done && cmode == seuss_mode)
	XFillRectangle (st->dpy, st->window, st->merge_gc, 0, 0, st->width, st->height);
    }
  else
    XCopyPlane (st->dpy, st->pixmap, st->window, st->merge_gc, 0, 0, st->width, st->height, 0, 0, 1);

  /* st->buffer is only used in seuss-mode or anim-mode */
  if (st->buffer && (st->anim_p
		 ? (done || (!st->done_once && (st->iterations & 1)))
		 : (st->iterations & 1)))
    {
      XCopyPlane (st->dpy, st->buffer, st->window, st->copy_gc, 0, 0, st->width, st->height, 0, 0, 1);
      if (st->anim_p && done)
	XFillRectangle (st->dpy, st->buffer, st->erase_gc, 0, 0, st->width, st->height);
    }

#ifdef DEBUG
  XCopyPlane (st->dpy, st->pixmap, st->window, st->copy_gc, 0,0,st->width,st->height,st->width,st->height, 1);
  if (st->buffer)
    XCopyPlane (st->dpy, st->buffer, st->window, st->copy_gc, 0,0,st->width,st->height,0,st->height, 1);
#endif

  if (done)
    st->iterations = 0;
  else
    st->iterations++;

  if (st->delay && !inhibit_sleep)
    {
      int d = st->delay;

      if (cmode == seuss_mode && st->anim_p)
        this_delay = d/100;
      else
        this_delay = d;

      if (done)
	st->done_once_no_really = True;
    }

  if (done && st->clear_tick > 0)
    {
      st->clear_tick--;
      if (!st->clear_tick)
        {
          XClearWindow (st->dpy, st->window);
          if (st->buffer) XFillRectangle (st->dpy, st->buffer, st->erase_gc, 0,0,st->width,st->height);
        }
    }

  if (inhibit_sleep) this_delay = 0;

  return this_delay;
}



static const char *halo_defaults [] = {
  ".background:		black",
  ".foreground:		white",
  "*colorMode:		random",
  "*colors:		100",
  "*count:		0",
  "*delay:		100000",
  "*delay2:		20",
  "*increment:		0",
  "*animate:		False",
#ifdef HAVE_MOBILE
  "*ignoreRotation:     True",
#endif
  0
};

static XrmOptionDescRec halo_options [] = {
  { "-count",		".count",	XrmoptionSepArg, 0 },
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-animate",		".animate",	XrmoptionNoArg, "True" },
  { "-mode",		".colorMode",	XrmoptionSepArg, 0 },
  { "-colors",		".colors",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

static void
halo_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
halo_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
halo_free (Display *dpy, Window window, void *closure)
{
}

XSCREENSAVER_MODULE ("Halo", halo)
