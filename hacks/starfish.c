/* xscreensaver, Copyright (c) 1997 Jamie Zawinski <jwz@netscape.com>
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
#include <sys/time.h> /* for gettimeofday() */
#include "screenhack.h"
#include "spline.h"


static Colormap cmap;
static Bool cycle_p;
static XColor *colors;
static int ncolors;
static int fg_index;
static GC gc;

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

static struct starfish *
make_starfish (int maxx, int maxy, int size)
{
  struct starfish *s = (struct starfish *) calloc(1, sizeof(*s));
  int i;
  int mid;

  s->blob_p = get_boolean_resource ("blob", "Blob");
  s->elasticity = SCALE * get_float_resource ("thickness", "Thickness");

  if (s->elasticity == 0)
    /* bell curve from 0-15, avg 7.5 */
    s->elasticity = RAND(5*SCALE) + RAND(5*SCALE) + RAND(5*SCALE);

  s->rotv = get_float_resource ("rotation", "Rotation");
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
    static char skips[] = { 2, 2, 2, 2,
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
    static char sizes[] = { 3, 3, 3, 3, 3,
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
draw_starfish (Display *dpy, Drawable drawable, GC gc, struct starfish *s,
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
	XClearWindow (dpy, drawable);
      XFillPolygon (dpy, drawable, gc, points, i+j, Complex, CoordModeOrigin);
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
	XDrawLine (dpy, drawable, gc, s->x/SCALE, s->y/SCALE,
		   s->spline->control_x[i], s->spline->control_y[i]);
    }
#endif
}


static struct starfish *
make_window_starfish (Display *dpy, Window window)
{
  XWindowAttributes xgwa;
  int size;
  Bool blob_p = get_boolean_resource ("blob", "Blob");
  XGetWindowAttributes (dpy, window, &xgwa);
  size = (xgwa.width < xgwa.height ? xgwa.width : xgwa.height);
  if (blob_p) size /= 2;
  else size *= 1.3;
  return make_starfish (xgwa.width, xgwa.height, size);
}


static struct starfish *
init_starfish (Display *dpy, Window window)
{
  static Bool first_time = True;
  XGCValues gcv;
  XWindowAttributes xgwa;
  Bool blob_p = get_boolean_resource ("blob", "Blob");
  XGetWindowAttributes (dpy, window, &xgwa);

  cmap = xgwa.colormap;
  cycle_p = get_boolean_resource ("cycle", "Cycle");

  if (!first_time)
    {
      if (colors && ncolors)
	free_colors (dpy, cmap, colors, ncolors);
      if (colors)
	free (colors);
      colors = 0;
    }

  ncolors = get_integer_resource ("colors", "Colors");
  if (ncolors < 2) ncolors = 2;
  if (ncolors <= 2) mono_p = True;

  if (mono_p)
    colors = 0;
  else
    colors = (XColor *) malloc(sizeof(*colors) * (ncolors+1));

  if (mono_p || blob_p)
    cycle_p = False;

  if (mono_p)
    ;
  else if (random() % 3)
    make_smooth_colormap (dpy, xgwa.visual, cmap, colors, &ncolors,
			  True, &cycle_p, True);
  else
    make_uniform_colormap (dpy, xgwa.visual, cmap, colors, &ncolors,
			   True, &cycle_p, True);

  if (ncolors < 2) ncolors = 2;
  if (ncolors <= 2) mono_p = True;

  if (mono_p) cycle_p = False;

  fg_index = 0;

  if (!mono_p && !blob_p)
    {
      gcv.foreground = colors[fg_index].pixel;
      XSetWindowBackground (dpy, window, gcv.foreground);
    }

  if (first_time)
    {
      XClearWindow (dpy, window);
      first_time = False;
    }

  gcv.fill_rule = EvenOddRule;
  gc = XCreateGC (dpy, window, GCForeground | GCFillRule, &gcv);

  return make_window_starfish (dpy, window);
}



static void
run_starfish (Display *dpy, Window window, struct starfish *s)
{
  throb_starfish (s);
  spin_starfish (s);
  draw_starfish (dpy, window, gc, s, False);

  if (mono_p)
    {
      static Bool init = False;
      static unsigned long black, white;
      if (!init)
	{
	  black = get_pixel_resource ("background", "Background", dpy, cmap);
	  white = get_pixel_resource ("foreground", "Foreground", dpy, cmap);
	  init = True;
	  fg_index = white;
	  XSetForeground (dpy, gc, fg_index);
	}
      else if (!s->blob_p)
	{
	  fg_index = (fg_index == black ? white : black);
	  XSetForeground (dpy, gc, fg_index);
	}
    }
  else
    {
      fg_index = (fg_index + 1) % ncolors;
      XSetForeground (dpy, gc, colors [fg_index].pixel);
    }
}




char *progclass = "Starfish";

char *defaults [] = {
  ".background:		black",
  ".foreground:		white",
  "*delay:		10000",
  "*cycleDelay:		100000",
  "*thickness:		0",		/* pixels, 0 = random */
  "*rotation:		-1",		/* degrees, -1 = "random" */
  "*colors:		200",
  "*cycle:		true",
  "*duration:		30",
  "*delay2:		5",
  "*blob:		false",
  0
};

XrmOptionDescRec options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-delay2",		".delay2",	XrmoptionSepArg, 0 },
  { "-cycle-delay",	".cycleDelay",	XrmoptionSepArg, 0 },
  { "-thickness",	".thickness",	XrmoptionSepArg, 0 },
  { "-rotation",	".rotation",	XrmoptionSepArg, 0 },
  { "-colors",		".colors",	XrmoptionSepArg, 0 },
  { "-cycle",		".cycle",	XrmoptionNoArg, "True" },
  { "-no-cycle",	".cycle",	XrmoptionNoArg, "False" },
  { "-duration",	".duration",	XrmoptionSepArg, 0 },
  { "-blob",		".blob",	XrmoptionNoArg, "True" },
  { "-no-blob",		".blob",	XrmoptionNoArg, "False" },
  { 0, 0, 0, 0 }
};

void
screenhack (Display *dpy, Window window)
{
  struct starfish *s = init_starfish (dpy, window);
  int delay = get_integer_resource ("delay", "Delay");
  int delay2 = get_integer_resource ("delay2", "Delay") * 1000000;
  int cycle_delay = get_integer_resource ("cycleDelay", "Delay");
  int duration = get_seconds_resource ("duration", "Seconds");
  Bool blob_p = get_boolean_resource ("blob", "Blob");
  time_t start = time ((time_t) 0);
  time_t now;
  int direction = (random() % 1) ? 1 : -1;

  if (blob_p)
    delay *= 3;

  while (1)
    {
      run_starfish (dpy, window, s);
      XSync (dpy, True);

      if (cycle_p && cycle_delay)
	{
	  if (cycle_delay <= delay)
	    {
	      int i = 0;
	      while (i < delay)
		{
		  rotate_colors (dpy, cmap, colors, ncolors, direction);
		  usleep(cycle_delay);
		  i += cycle_delay;
		}
	    }
	  else
	    {
	      static long tick = 0;
	      if (tick >= cycle_delay)
		{
		  rotate_colors (dpy, cmap, colors, ncolors, direction);
		  tick = 0;
		}
	      if (delay)
		usleep(delay);
	      tick += delay;
	    }

	  if (! (random() % 1000))
	    direction = -direction;
	}
      else if (delay)
	usleep (delay);

      if (duration > 0)
	{
	  now = time ((time_t) 0);
	  if (start + duration < now)
	    {
	      start = now;

	      free_starfish (s);

	      if (delay2 && !blob_p && cycle_p)
		{
		  int i = 0;
		  while (i < delay2)
		    {
		      rotate_colors (dpy, cmap, colors, ncolors, direction);
		      usleep(cycle_delay);
		      i += cycle_delay;
		    }
		}

	      /* Every now and then, pick new colors; otherwise, just build
		 a new starfish with the current colors. */
	      if (! (random () % 10))
		s = init_starfish (dpy, window);
	      else
		s = make_window_starfish(dpy, window);
	    }
	}
    }
}
