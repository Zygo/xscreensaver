/* xscreensaver, Copyright (c) 1997-2008 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include <math.h>
#include "screenhack.h"
#include "spline.h"

#define SCALE        1000	/* fixed-point math, for sub-pixel motion */


#define RAND(n) ((long) ((random() & 0x7fffffff) % ((long) (n))))
#define RANDSIGN() ((random() & 1) ? 1 : -1)

enum starfish_mode {
  pulse,
  zoom
};
  
struct starfish {
  enum starfish_mode mode;
  Bool blob_p;
  int skip;
  long x, y;		/* position of midpoint */
  double th;		/* angle of rotation */
  double rotv;		/* rotational velocity */
  double rota;		/* rotational acceleration */
  long elasticity;	/* how fast it deforms: radial velocity */
  double rot_max;
  long min_r, max_r;	/* radius range */
  int npoints;		/* control points */
  long *r;		/* radii */
  spline *spline;
  XPoint *prev;
  int n_prev;
};


struct state {
  Display *dpy;
  Window window;

  Colormap cmap;
  XColor *colors;
  int ncolors;
  int fg_index;
  GC gc;

  int delay, delay2, duration, direction, blob_p;

  Bool done_once;
  Bool initted;
  unsigned long black, white;

  long tick;
  time_t start_time;

  struct starfish *starfish;
};


static struct starfish *
make_starfish (struct state *st, int maxx, int maxy, int size)
{
  struct starfish *s = (struct starfish *) calloc(1, sizeof(*s));
  int i;
  int mid;

  s->blob_p = st->blob_p;
  s->elasticity = SCALE * get_float_resource (st->dpy, "thickness", "Thickness");

  if (s->elasticity == 0)
    /* bell curve from 0-15, avg 7.5 */
    s->elasticity = RAND(5*SCALE) + RAND(5*SCALE) + RAND(5*SCALE);

  s->rotv = get_float_resource (st->dpy, "rotation", "Rotation");
  if (s->rotv == -1)
    /* bell curve from 0-12 degrees, avg 6 */
    s->rotv = frand(4) + frand(4) + frand(4);

  s->rotv /= 360;  /* convert degrees to ratio */

  if (s->blob_p)
    {
      s->elasticity *= 3;
      s->rotv *= 3;
    }

  s->rot_max = s->rotv * 2;
  s->rota = 0.0004 + frand(0.0002);


  if (! (random() % 20))
    size *= frand(0.35) + frand(0.35) + 0.3;

  {
    static const char skips[] = { 2, 2, 2, 2,
                                  3, 3, 3,
                                  6, 6,
                                  12 };
    s->skip = skips[random() % sizeof(skips)];
  }

  if (! (random() % (s->skip == 2 ? 3 : 12)))
    s->mode = zoom;
  else
    s->mode = pulse;

  maxx *= SCALE;
  maxy *= SCALE;
  size *= SCALE;

  s->max_r = size;
  s->min_r = 0;

  if (s->min_r < (5*SCALE)) s->min_r = (5*SCALE);
  mid = ((s->min_r + s->max_r) / 2);

  s->x = maxx/2;
  s->y = maxy/2;

  s->th = frand(M_PI+M_PI) * RANDSIGN();

  {
    static const char sizes[] = { 3, 3, 3, 3, 3,
                                  4, 4, 4, 4,
                                  5, 5, 5, 5, 5, 5,
                                  8, 8, 8,
                                  10,
                                  35 };
    int nsizes = sizeof(sizes);
    if (s->skip > 3)
      nsizes -= 4;
    s->npoints = s->skip * sizes[random() % nsizes];
  }

  s->spline = make_spline (s->npoints);
  s->r = (long *) malloc (sizeof(*s->r) * s->npoints);

  for (i = 0; i < s->npoints; i++)
    s->r[i] = ((i % s->skip) == 0) ? 0 : size;

  return s;
}


static void
free_starfish (struct starfish *s)
{
  if (s->r) free (s->r);
  if (s->prev) free (s->prev);
  if (s->spline)
    {
      if (s->spline->control_x) free (s->spline->control_x);
      if (s->spline->control_y) free (s->spline->control_y);
      if (s->spline->points) free (s->spline->points);
      free (s->spline);
    }
  free (s);
}


static void 
throb_starfish (struct starfish *s)
{
  int i;
  double frac = ((M_PI+M_PI) / s->npoints);

  for (i = 0; i < s->npoints; i++)
    {
      long r = s->r[i];
      long ra = (r > 0 ? r : -r);
      double th = (s->th > 0 ? s->th : -s->th);
      long x, y;
      long elasticity = s->elasticity;

      /* place control points evenly around perimiter, shifted by theta */
      x = s->x + ra * cos (i * frac + th);
      y = s->y + ra * sin (i * frac + th);

      s->spline->control_x[i] = x / SCALE;
      s->spline->control_y[i] = y / SCALE;

      if (s->mode == zoom && ((i % s->skip) == 0))
	continue;

      /* Slow down near the end points: move fastest in the middle. */
      {
	double ratio = (double)ra / (double)(s->max_r - s->min_r);
	if (ratio > 0.5) ratio = 1-ratio;	/* flip */
	ratio *= 2;				/* normalize */
	ratio = (ratio * 0.9) + 0.1;		/* fudge */
	elasticity *= ratio;
      }


      /* Increase/decrease radius by elasticity */
      ra += (r >= 0 ? elasticity : -elasticity);
      if ((i % s->skip) == 0)
	ra += (elasticity / 2);

      r = ra * (r >= 0 ? 1 : -1);

      /* If we've reached the end (too long or too short) reverse direction. */
      if ((ra > s->max_r && r >= 0) ||
	  (ra < s->min_r && r < 0))
	r = -r;

      s->r[i] = r;
    }
}


static void
spin_starfish (struct starfish *s)
{
  double th = s->th;
  if (th < 0)
    th = -(th + s->rotv);
  else
    th += s->rotv;

  if (th > (M_PI+M_PI))
    th -= (M_PI+M_PI);
  else if (th < 0)
    th += (M_PI+M_PI);

  s->th = (s->th > 0 ? th : -th);

  s->rotv += s->rota;

  if (s->rotv > s->rot_max || 
      s->rotv < -s->rot_max)
    {
      s->rota = -s->rota;
    }
  /* If it stops, start it going in the other direction. */
  else if (s->rotv < 0)
    {
      if (random() & 1)
	{
	  /* keep going in the same direction */
	  s->rotv = 0;
	  if (s->rota < 0)
	    s->rota = -s->rota;
	}
      else
	{
	  /* reverse gears */
	  s->rotv = -s->rotv;
	  s->rota = -s->rota;
	  s->th = -s->th;
	}
    }


  /* Alter direction of rotational acceleration randomly. */
  if (! (random() % 120))
    s->rota = -s->rota;

  /* Change acceleration very occasionally. */
  if (! (random() % 200))
    {
      if (random() & 1)
	s->rota *= 1.2;
      else
	s->rota *= 0.8;
    }
}


static void
draw_starfish (struct state *st, Drawable drawable, GC this_gc, struct starfish *s,
	   Bool fill_p)
{
  compute_closed_spline (s->spline);
  if (s->prev)
    {
      XPoint *points = (XPoint *)
	malloc (sizeof(XPoint) * (s->n_prev + s->spline->n_points + 2));
      int i = s->spline->n_points;
      int j = s->n_prev;
      memcpy (points, s->spline->points, (i * sizeof(*points)));
      memcpy (points+i, s->prev, (j * sizeof(*points)));

      if (s->blob_p)
	XClearWindow (st->dpy, drawable);
      XFillPolygon (st->dpy, drawable, this_gc, points, i+j, Complex, CoordModeOrigin);
      free (points);

      free (s->prev);
      s->prev = 0;
    }

  s->prev = (XPoint *) malloc (s->spline->n_points * sizeof(XPoint));
  memcpy (s->prev, s->spline->points, s->spline->n_points * sizeof(XPoint));
  s->n_prev = s->spline->n_points;

#ifdef DEBUG
  if (s->blob_p)
    {
      int i;
      for (i = 0; i < s->npoints; i++)
	XDrawLine (st->dpy, drawable, this_gc, s->x/SCALE, s->y/SCALE,
		   s->spline->control_x[i], s->spline->control_y[i]);
    }
#endif
}


static struct starfish *
make_window_starfish (struct state *st)
{
  XWindowAttributes xgwa;
  int size;
  XGetWindowAttributes (st->dpy, st->window, &xgwa);
  size = (xgwa.width < xgwa.height ? xgwa.width : xgwa.height);
  if (st->blob_p) size /= 2;
  else size *= 1.3;
  return make_starfish (st, xgwa.width, xgwa.height, size);
}


static struct starfish *
reset_starfish (struct state *st)
{
  XGCValues gcv;
  unsigned int flags = 0;
  XWindowAttributes xgwa;
  XGetWindowAttributes (st->dpy, st->window, &xgwa);

  st->cmap = xgwa.colormap;

  if (st->done_once)
    {
      if (st->colors && st->ncolors)
	free_colors (st->dpy, st->cmap, st->colors, st->ncolors);
      if (st->colors)
	free (st->colors);
      st->colors = 0;
      XFreeGC (st->dpy, st->gc);
      st->gc = 0;
    }

  st->ncolors = get_integer_resource (st->dpy, "colors", "Colors");
  if (st->ncolors < 2) st->ncolors = 2;
  if (st->ncolors <= 2) mono_p = True;

  if (mono_p)
    st->colors = 0;
  else
    st->colors = (XColor *) malloc(sizeof(*st->colors) * (st->ncolors+1));

  if (mono_p)
    ;
  else if (random() % 3)
    make_smooth_colormap (st->dpy, xgwa.visual, st->cmap, st->colors, &st->ncolors,
			  True, 0, True);
  else
    make_uniform_colormap (st->dpy, xgwa.visual, st->cmap, st->colors, &st->ncolors,
			   True, 0, True);

  if (st->ncolors < 2) st->ncolors = 2;
  if (st->ncolors <= 2) mono_p = True;

  st->fg_index = 0;

  if (!mono_p && !st->blob_p)
    {
      flags |= GCForeground;
      gcv.foreground = st->colors[st->fg_index].pixel;
      XSetWindowBackground (st->dpy, st->window, gcv.foreground);
    }

  if (!st->done_once)
    {
      XClearWindow (st->dpy, st->window);
      st->done_once = 1;
    }

  flags |= GCFillRule;
  gcv.fill_rule = EvenOddRule;
  st->gc = XCreateGC (st->dpy, st->window, flags, &gcv);

  return make_window_starfish (st);
}



static void
run_starfish (struct state *st, struct starfish *s)
{
  throb_starfish (s);
  spin_starfish (s);
  draw_starfish (st, st->window, st->gc, s, False);

  if (mono_p)
    {
      if (!st->initted)
	{
	  st->black = get_pixel_resource (st->dpy, st->cmap, "background", "Background");
	  st->white = get_pixel_resource (st->dpy, st->cmap, "foreground", "Foreground");
	  st->initted = True;
	  st->fg_index = st->white;
	  XSetForeground (st->dpy, st->gc, st->fg_index);
	}
      else if (!s->blob_p)
	{
	  st->fg_index = (st->fg_index == st->black ? st->white : st->black);
	  XSetForeground (st->dpy, st->gc, st->fg_index);
	}
    }
  else
    {
      st->fg_index = (st->fg_index + 1) % st->ncolors;
      XSetForeground (st->dpy, st->gc, st->colors [st->fg_index].pixel);
    }
}


static void *
starfish_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  char *s;
  st->dpy = dpy;
  st->window = window;
  st->delay = get_integer_resource (st->dpy, "delay", "Delay");
  st->delay2 = get_integer_resource (st->dpy, "delay2", "Delay") * 1000000;
/*  st->duration = get_seconds_resource (st->dpy, "duration", "Seconds");*/
  st->duration = get_integer_resource (st->dpy, "duration", "Seconds");
  st->direction = (random() & 1) ? 1 : -1;

  s = get_string_resource (st->dpy, "mode", "Mode");
  if (s && !strcasecmp (s, "blob"))
    st->blob_p = True;
  else if (s && !strcasecmp (s, "zoom"))
    st->blob_p = False;
  else if (!s || !*s || !strcasecmp (s, "random"))
    st->blob_p = !(random() % 3);
  else
    fprintf (stderr, "%s: mode must be blob, zoom, or random", progname);

  if (st->blob_p)
    st->delay *= 3;

  st->starfish = reset_starfish (st);
  return st;
}

static unsigned long
starfish_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  struct starfish *s = st->starfish;
  time_t now;

  run_starfish (st, s);

  if (st->duration > 0)
    {
      if (st->start_time == 0)
        st->start_time = time ((time_t) 0);
      now = time ((time_t) 0);
      if (st->start_time + st->duration < now)
        {
          st->start_time = now;

          free_starfish (s);

          /* Every now and then, pick new colors; otherwise, just build
             a new starfish with the current colors. */
          if (! (random () % 10))
            s = reset_starfish (st);
          else
            s = make_window_starfish (st);

	  st->starfish = s;
        }
    }

  return st->delay;
}

static void
starfish_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  free_starfish (st->starfish);
  st->starfish = 0;
  st->starfish = reset_starfish (st);
}

static Bool
starfish_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
starfish_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}




static const char *starfish_defaults [] = {
  ".background:		black",
  ".foreground:		white",
  "*fpsSolid:		true",
  "*delay:		10000",
  "*thickness:		0",		/* pixels, 0 = random */
  "*rotation:		-1",		/* degrees, -1 = "random" */
  "*colors:		200",
  "*duration:		30",
  "*delay2:		5",
  "*mode:		random",
  0
};

static XrmOptionDescRec starfish_options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-delay2",		".delay2",	XrmoptionSepArg, 0 },
  { "-thickness",	".thickness",	XrmoptionSepArg, 0 },
  { "-rotation",	".rotation",	XrmoptionSepArg, 0 },
  { "-colors",		".colors",	XrmoptionSepArg, 0 },
  { "-duration",	".duration",	XrmoptionSepArg, 0 },
  { "-mode",		".mode",	XrmoptionSepArg, 0 },
  { "-blob",		".mode",	XrmoptionNoArg, "blob" },
  { "-zoom",		".mode",	XrmoptionNoArg, "zoom" },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Starfish", starfish)
