/* epicycle --- The motion of a body with epicycles, as in the pre-Copernican
 * cosmologies.
 *
 * Copyright (c) 1998  James Youngman <jay@gnu.org>
 * 
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* Standard C headers; screenhack.h assumes that these have already
 * been included if required -- for example, it defines M_PI if not
 * already defined.
 */
#include <float.h>
#include <math.h>


#include "screenhack.h"
#include "erase.h"

/* MIT-SHM headers omitted; this screenhack doesn't use it */



/*********************************************************/
/******************** MAGIC CONSTANTS ********************/
/*********************************************************/
#define MIN_RADIUS (5)		/* smallest allowable circle radius */
#define FILL_PROPORTION (0.9)	/* proportion of screen to fill by scaling. */
/*********************************************************/
/***************** END OF MAGIC CONSTANTS ****************/
/*********************************************************/



#define FULLCIRCLE (2.0 * M_PI)	/* radians in a circle. */


/* Some of these resource values here are hand-tuned to give a
 * pleasing variety of interesting shapes.  These are not the only
 * good settings, but you may find you need to change some as a group
 * to get pleasing figures.
 */
static const char *epicycle_defaults [] = {
  ".background:	black",
  ".foreground:	white",
  "*fpsSolid:	true",
  "*colors:	100",
  "*color0:	red",
  "*delay:	20000",
  "*holdtime:	2",
  "*lineWidth:	4",
  "*minCircles:  2",
  "*maxCircles:  10",
  "*minSpeed:	0.003",
  "*maxSpeed:	0.005",
  "*harmonics:	8",
  "*timestep:	1.0",
  "*timestepCoarseFactor: 1.0", /* no option for this resource. */
  "*divisorPoisson: 0.4",
  "*sizeFactorMin: 1.05",
  "*sizeFactorMax: 2.05",
#ifdef USE_IPHONE
  "*ignoreRotation: True",
#endif
  0
};

/* options passed to this program */
static XrmOptionDescRec epicycle_options [] = {
  { "-color0",		".color0",	  XrmoptionSepArg, 0 },
  { "-colors",		".colors",	  XrmoptionSepArg, 0 },
  { "-colours",		".colors",	  XrmoptionSepArg, 0 },
  { "-foreground",	".foreground",	  XrmoptionSepArg, 0 },
  { "-delay",		".delay",	  XrmoptionSepArg, 0 },
  { "-holdtime",	".holdtime",	  XrmoptionSepArg, 0 },
  { "-linewidth",	".lineWidth",	  XrmoptionSepArg, 0 },
  { "-min_circles",	".minCircles",	  XrmoptionSepArg, 0 },
  { "-max_circles",	".maxCircles",	  XrmoptionSepArg, 0 },
  { "-min_speed",	".minSpeed",	  XrmoptionSepArg, 0 },
  { "-max_speed",	".maxSpeed",	  XrmoptionSepArg, 0 },
  { "-harmonics",	".harmonics",	  XrmoptionSepArg, 0 },
  { "-timestep",	".timestep",	  XrmoptionSepArg, 0 },
  { "-divisor_poisson",".divisorPoisson",XrmoptionSepArg, 0 },
  { "-size_factor_min", ".sizeFactorMin", XrmoptionSepArg, 0 },
  { "-size_factor_max", ".sizeFactorMax", XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};


/* Each circle is centred on a point on the rim of another circle.
 */
struct tagCircle
{
  long radius;			/* in pixels */
  double w;			/* position (radians ccw from x-axis) */
  double initial_w;		/* starting position */
  double wdot;			/* rotation rate (change in w per iteration) */
  int    divisor;

  struct tagCircle *pchild;
};
typedef struct tagCircle Circle;


struct tagBody			/* a body that moves on a system of circles. */
{
  int x_origin, y_origin;
  int x, y;
  int old_x, old_y;
  int current_color;		/* pixel index into colors[] */
  Circle *epicycles;		/* system of circles on which it moves. */
  struct tagBody *next;		/* next in list. */
};
typedef struct tagBody Body;


struct state {
   Display *dpy;
   Window window;
   GC color0;
   int width, height;
   int x_offset, y_offset;
   int unit_pixels;
   unsigned long bg;
   Colormap cmap;
   int restart;
   double wdot_max;
   XColor *colors;
   int ncolors;
   int color_shift_pos;	/* how far we are towards that. */
   double colour_cycle_rate;
   int harmonics;
   double divisorPoisson;
   double sizeFactorMin;
   double sizeFactorMax;
   int minCircles;
   int maxCircles;

   Bool done;

   long L;
   double T, timestep, circle, timestep_coarse;
   int delay;
   int uncleared;
   int holdtime;
   int xmax, xmin, ymax, ymin;
   Body *pb0;
   double xtime;
   eraser_state *eraser;
};



/* Determine the GCD of two numbers using Euclid's method.  The other
 * possible algorighm is Stein's method, but it's probably only going
 * to be much faster on machines with no divide instruction, like the
 * ARM and the Z80.  The former is very fast anyway and the latter
 * probably won't run X clients; in any case, this calculation is not
 * the bulk of the computational expense of the program.  I originally
 * tried using Stein's method, but I wanted to remove the gotos.  Not
 * wanting to introduce possible bugs, I plumped for Euclid's method
 * instead.  Lastly, Euclid's algorithm is preferred to the
 * generalisation for N inputs.
 *
 * See Knuth, section 4.5.2.
 */
static int
gcd(int u, int v)		/* Euclid's Method */
{
  /* If either operand of % is negative, the sign of the result is
   * implementation-defined.  See section 6.3.5 "Multiplicative
   * Operators" of the ANSI C Standard (page 46 [LEFT HAND PAGE!] of
   * "Annotated C Standard", Osborne, ISBN 0-07-881952-0).
   */
  if (u < 0) u = -u;
  if (v < 0) v = -v;
  
  while (0 != v)
    {
      int r;
      r = u % v;
      u = v;
      v = r;
    }
  return u;
}

/* Determine the Lowest Common Multiple of two integers, using
 * Euclid's Proposition 34, as explained in Knuth's The Art of
 * Computer Programming, Vol 2, section 4.5.2.
 */
static int
lcm(int u, int v)
{
  return u / gcd(u,v) * v;
}

static long 
random_radius(struct state *st, double scale)	
{
  long r;

  r = frand(scale) * st->unit_pixels/2; /* for frand() see utils/yarandom.h */
  if (r < MIN_RADIUS)
    r = MIN_RADIUS;
  return r;
}


static long
random_divisor(struct state *st)
{
  int divisor = 1;
  int sign;

  while (frand(1.0) < st->divisorPoisson && divisor <= st->harmonics)
    {
      ++divisor;
    }
  sign = (frand(1.0) < 0.5) ? +1 : -1;
  return sign * divisor;
}


/* Construct a circle or die.
 */
static Circle *
new_circle(struct state *st, double scale)
{
  Circle *p = malloc(sizeof(Circle));
  
  p->radius = random_radius(st, scale);
  p->w = p->initial_w = 0.0;
  p->divisor = random_divisor(st);
  p->wdot = st->wdot_max / p->divisor;
  p->pchild = NULL;
  
  return p;
}

static void delete_circle(Circle *p)
{
  free(p);
}

static void 
delete_circle_chain(Circle *p)
{
  while (p)
    {
      Circle *q = p->pchild;
      delete_circle(p);
      p = q;
    }
}

static Circle *
new_circle_chain(struct state *st)
{
  Circle *head;
  double scale = 1.0, factor;
  int n;

  /* Parent circles are larger than their children by a factor of at
   * least FACTOR_MIN and at most FACTOR_MAX.
   */
  factor = st->sizeFactorMin + frand(st->sizeFactorMax - st->sizeFactorMin);
  
  /* There are between minCircles and maxCircles in each figure.
   */
  if (st->maxCircles == st->minCircles)
    n = st->minCircles;            /* Avoid division by zero. */
  else
    n = st->minCircles + random() % (st->maxCircles - st->minCircles);
  
  head = NULL;
  while (n--)
    {
      Circle *p = new_circle(st, scale);
      p->pchild = head;
      head = p;

      scale /= factor;
    }
  return head;
}

static void
assign_random_common_w(Circle *p)
{
  double w_common = frand(FULLCIRCLE);	/* anywhere on the circle */
  while (p)
    {
      p->initial_w = w_common;
      p = p->pchild;
    }
}

static Body *
new_body(struct state *st)
{
  Body *p = malloc(sizeof(Body));
  if (!p) abort();
  p->epicycles = new_circle_chain(st);
  p->current_color = 0;		/* ?? start them all on different colors? */
  p->next = NULL;
  p->x = p->y = 0;
  p->old_x = p->old_y = 0;
  p->x_origin = p->y_origin = 0;

  /* Start all the epicycles at the same w value to make it easier to
   * figure out at what T value the cycle is closed.   We don't just fix
   * the initial W value because that makes all the patterns tend to 
   * be symmetrical about the X axis.
   */
  assign_random_common_w(p->epicycles);
  return p;
}

static void
delete_body(Body *p)
{
  delete_circle_chain(p->epicycles);
  free(p);
}


static void 
draw_body(struct state *st, Body *pb, GC gc)
{
  XDrawLine(st->dpy, st->window, gc, pb->old_x, pb->old_y, pb->x, pb->y);
}

static long
compute_divisor_lcm(Circle *p)
{
  long l = 1;
  
  while (p)
    {
      l = lcm(l, p->divisor);
      p = p->pchild;
    }
  return l;
}

	      
/* move_body()
 *
 * Calculate the position for the body at time T.  We work in double 
 * rather than int to avoid the cumulative errors that would be caused
 * by the rounding implicit in an assignment to int.
 */
static void
move_body(Body *pb, double t)
{
  Circle *p;
  double x, y;

  pb->old_x = pb->x;
  pb->old_y = pb->y;
  
  x = pb->x_origin;
  y = pb->y_origin;
  
  for (p=pb->epicycles; NULL != p; p=p->pchild)
    {
      /* angular pos = initial_pos + time * angular speed */
      /* but this is an angular position, so modulo FULLCIRCLE. */
      p->w = fmod(p->initial_w + (t * p->wdot), FULLCIRCLE);
      
      x += (p->radius * cos(p->w));
      y += (p->radius * sin(p->w));
    }
  
  pb->x = (int)x;
  pb->y = (int)y;
}

static int
colour_init(struct state *st, XWindowAttributes *pxgwa)
{
  XGCValues gcv;

#if 0
  int H = random() % 360;	/* colour choice from attraction.c. */
  double S1 = 0.25;
  double S2 = 1.00;
  double V = frand(0.25) + 0.75;
  int line_width = 0;
#endif

  int retval = 1;
  unsigned long valuemask = 0L;
  unsigned long fg;
  
  /* Free any already allocated colors...
   */
  if (st->colors)
    {
      free_colors(pxgwa->screen, st->cmap, st->colors, st->ncolors);
      st->colors = 0;
      st->ncolors = 0;
    }
	
  st->ncolors = get_integer_resource (st->dpy, "colors", "Colors");
  if (0 == st->ncolors)		/* English spelling? */
    st->ncolors = get_integer_resource (st->dpy, "colours", "Colors");
  
  if (st->ncolors < 2)
    st->ncolors = 2;
  if (st->ncolors <= 2)
    mono_p = True;
  st->colors = 0;

  if (!mono_p)
    {
      st->colors = (XColor *) malloc(sizeof(*st->colors) * (st->ncolors+1));
      if (!st->colors) abort();
	  
      make_smooth_colormap (pxgwa->screen, pxgwa->visual, st->cmap,
                            st->colors, &st->ncolors,
			    True, /* allocate */
			    False, /* not writable */
			    True); /* verbose (complain about failure) */
      if (st->ncolors <= 2)
	{
	  if (st->colors)
	    free (st->colors);
	  st->colors = 0;
	  mono_p = True;
	}
    }

  
  st->bg = get_pixel_resource (st->dpy, st->cmap, "background", "Background");

  /* Set the line width
   */
  gcv.line_width = get_integer_resource (st->dpy, "lineWidth", "Integer");
  if (gcv.line_width)
    {
      valuemask |= GCLineWidth;

      gcv.join_style = JoinRound;
      gcv.cap_style = CapRound;
	  
      valuemask |= (GCCapStyle | GCJoinStyle);
    }
  

  /* Set the drawing function.
   */
  gcv.function = GXcopy;
  valuemask |= GCFunction;
  
  /* Set the foreground.
   */
/*  if (mono_p)*/
    fg = get_pixel_resource (st->dpy, st->cmap, "foreground", "Foreground");
/* WTF?
else
    fg = st->bg ^ get_pixel_resource (st->dpy, st->cmap, ("color0"), "Foreground"); 
*/
  gcv.foreground = fg;
  valuemask |= GCForeground;

  /* Actually create the GC.
   */
  st->color0 = XCreateGC (st->dpy, st->window, valuemask, &gcv);
  
  return retval;
}




static void
setup(struct state *st)
{
  XWindowAttributes xgwa;
  
  XGetWindowAttributes (st->dpy, st->window, &xgwa);
  st->cmap = xgwa.colormap;

  st->width = xgwa.width;
  st->height = xgwa.height;
  st->x_offset = st->width / 2;
  st->y_offset = st->height / 2;
  st->unit_pixels = st->width < st->height ? st->width : st->height;

  {
    if (!st->done)
      {
	colour_init(st, &xgwa);
	st->done = True;
      }
  }
}


static void
color_step(struct state *st, Body *pb, double frac)
{
  if (!mono_p)
    {
      int newshift = st->ncolors * fmod(frac * st->colour_cycle_rate, 1.0);
      if (newshift != st->color_shift_pos)
	{
	  pb->current_color = newshift;
	  XSetForeground (st->dpy, st->color0, st->colors[pb->current_color].pixel);
	  st->color_shift_pos = newshift;
	}
    }
}


#if 0
static long
distance(long x1, long y1, long x2, long y2)
{
  long dx, dy;

  dx = x2 - x1;
  dy = y2 - y1;
  return dx*dx + dy*dy;
}

static int poisson_irand(double p)
{
  int r = 1;
  while (fabs(frand(1.0)) < p)
    ++r;
  return r < 1 ? 1 : r;
}
#endif

static void
precalculate_figure(Body *pb,
		    double this_xtime, double step,
		    int *x_max, int *y_max,
		    int *x_min, int *y_min)
{
  double t;
  
  move_body(pb, 0.0); /* move once to avoid initial line from origin */
  *x_min = *x_max = pb->x;
  *y_min = *y_max = pb->y;
  
  for (t=0.0; t<this_xtime; t += step)
    {
      move_body(pb, t); /* move once to avoid initial line from origin */
      if (pb->x > *x_max)
	*x_max = pb->x;
      if (pb->x < *x_min)
	*x_min = pb->x;
      if (pb->y > *y_max)
	*y_max = pb->y;
      if (pb->y < *y_min)
	*y_min = pb->y;
    }
}

static int i_max(int a, int b)
{
  return (a>b) ? a : b;
}

static void rescale_circles(struct state *st, Body *pb,
			    int x_max, int y_max,
			    int x_min, int y_min)
{
  double xscale, yscale, scale;
  double xm, ym;
  
  x_max -= st->x_offset;
  x_min -= st->x_offset;
  y_max -= st->y_offset;
  y_min -= st->y_offset;

  x_max = i_max(x_max, -x_min);
  y_max = i_max(y_max, -y_min);


  xm = st->width / 2.0;
  ym = st->height / 2.0;
  if (x_max > xm)
    xscale = xm / x_max;
  else
    xscale = 1.0;
  if (y_max > ym)
    yscale = ym / y_max;
  else
    yscale = 1.0;

  if (xscale < yscale)		/* wider than tall */
    scale = xscale;		/* ensure width fits onscreen */
  else
    scale = yscale;		/* ensure height fits onscreen */


  scale *= FILL_PROPORTION;	/* only fill FILL_PROPORTION of screen */
  if (scale < 1.0)		/* only reduce, don't enlarge. */
    {
      Circle *p;
      for (p=pb->epicycles; p; p=p->pchild)
	{
	  p->radius *= scale;
	}
    }
  else
    {
      printf("enlarge by x%.2f skipped...\n", scale);
    }
}


/* angular speeds of the circles are harmonics of a fundamental
 * value.  That should please the Pythagoreans among you... :-)
 */
static double 
random_wdot_max(struct state *st)
{
  /* Maximum and minimum values for the choice of wdot_max.  Possible
   * epicycle speeds vary from wdot_max to (wdot_max * harmonics).
   */
  double minspeed, maxspeed;
  minspeed = get_float_resource(st->dpy, "minSpeed", "Double");
  maxspeed = get_float_resource(st->dpy, "maxSpeed", "Double");
  return st->harmonics * (minspeed + FULLCIRCLE * frand(maxspeed-minspeed));
}


static void *
epicycle_init (Display *disp, Window win)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  st->dpy = disp;
  st->window = win;

  st->holdtime = get_integer_resource (st->dpy, "holdtime", "Integer");

  st->circle = FULLCIRCLE;
  
  XClearWindow(st->dpy, st->window);
  st->uncleared = 0;
  st->restart = 1;
  
  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  st->harmonics = get_integer_resource(st->dpy, "harmonics", "Integer");
  st->divisorPoisson = get_float_resource(st->dpy, "divisorPoisson", "Double");
  
  st->timestep = get_float_resource(st->dpy, "timestep", "Double");
  st->timestep_coarse = st->timestep *
    get_float_resource(st->dpy, "timestepCoarseFactor", "Double");
  
  st->sizeFactorMin = get_float_resource(st->dpy, "sizeFactorMin", "Double");
  st->sizeFactorMax = get_float_resource(st->dpy, "sizeFactorMax", "Double");

  st->minCircles = get_integer_resource (st->dpy, "minCircles", "Integer");
  st->maxCircles = get_integer_resource (st->dpy, "maxCircles", "Integer");

  st->xtime = 0; /* is this right? */

  return st;
}

static unsigned long
epicycle_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int this_delay = st->delay;

  if (st->eraser) {
    st->eraser = erase_window (st->dpy, st->window, st->eraser);
    return 10000;
  }

  if (st->restart)
    {
      setup(st);
      st->restart = 0;

      /* Flush any outstanding events; this has the side effect of
       * reducing the number of "false restarts"; resdtarts caused by
       * one event (e.g. ConfigureNotify) followed by another
       * (e.g. Expose).
       */
	  
      st->wdot_max = random_wdot_max(st);
	  
      if (st->pb0)
        {
          delete_body(st->pb0);
          st->pb0 = NULL;
        }
      st->pb0 = new_body(st);
      st->pb0->x_origin = st->pb0->x = st->x_offset;
      st->pb0->y_origin = st->pb0->y = st->y_offset;

      if (st->uncleared)
        {
          st->eraser = erase_window (st->dpy, st->window, st->eraser);
          st->uncleared = 0;
        }

      precalculate_figure(st->pb0, st->xtime, st->timestep_coarse,
                          &st->xmax, &st->ymax, &st->xmin, &st->ymin);

      rescale_circles(st, st->pb0, st->xmax, st->ymax, st->xmin, st->ymin);
      
      move_body(st->pb0, 0.0); /* move once to avoid initial line from origin */
      move_body(st->pb0, 0.0); /* move once to avoid initial line from origin */

      
      st->T = 0.0;			/* start at time zero. */

      st->L = compute_divisor_lcm(st->pb0->epicycles);
      
      st->colour_cycle_rate = fabs(st->L);
      
      st->xtime = fabs(st->L * st->circle / st->wdot_max);

      if (st->colors)				/* (colors==NULL) if mono_p */
        XSetForeground (st->dpy, st->color0, st->colors[st->pb0->current_color].pixel);
    }


  color_step(st, st->pb0, st->T/st->xtime );
  draw_body(st, st->pb0, st->color0);
  st->uncleared = 1;

	  
  /* Check if the figure is complete...*/
  if (st->T > st->xtime)
    {
      this_delay = st->holdtime * 1000000;
      st->restart = 1;	/* begin new figure. */
    }
	  


  st->T += st->timestep;
  move_body(st->pb0, st->T);

  return this_delay;
}

static void
epicycle_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  st->restart = 1;
}

static Bool
epicycle_event (Display *dpy, Window window, void *closure, XEvent *e)
{
  struct state *st = (struct state *) closure;
  if (e->type == ButtonPress)
    {
      st->restart = 1;
      return True;
    }

  return False;
}

static void
epicycle_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}

XSCREENSAVER_MODULE ("Epicycle", epicycle)
