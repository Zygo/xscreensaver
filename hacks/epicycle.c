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


/* Name of the Screensaver hack */
char *progclass="Epicycle";

/* Some of these resource values here are hand-tuned to give a
 * pleasing variety of interesting shapes.  These are not the only
 * good settings, but you may find you need to change some as a group
 * to get pleasing figures.
 */
char *defaults [] = {
  ".background:	black",
  ".foreground:	white",
  "*colors:	100",
  "*color0:	red",
  "*delay:	1000",
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
  0
};

/* options passed to this program */
XrmOptionDescRec options [] = {
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


static Display *dpy;
static Window window;
static GC color0;
static int width, height;
static int x_offset, y_offset;
static int unit_pixels;
static unsigned long bg;
static Colormap cmap;
static int restart = 0;
static int stop = 0;
static double wdot_max;
static XColor *colors = NULL;
static int ncolors = 2;
static int color_shift_pos=0;	/* how far we are towards that. */
static double colour_cycle_rate = 1.0;
static int harmonics = 8;
static double divisorPoisson = 0.4;
static double sizeFactorMin = 1.05;
static double sizeFactorMax = 2.05;
static int minCircles;
static int maxCircles;

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
random_radius(double scale)	
{
  long r;

  r = frand(scale) * unit_pixels/2; /* for frand() see utils/yarandom.h */
  if (r < MIN_RADIUS)
    r = MIN_RADIUS;
  return r;
}


static long
random_divisor(void)
{
  int divisor = 1;
  int sign;

  while (frand(1.0) < divisorPoisson && divisor <= harmonics)
    {
      ++divisor;
    }
  sign = (frand(1.0) < 0.5) ? +1 : -1;
  return sign * divisor;
}


static void
oom(void)
{
  fprintf(stderr, "Failed to allocate memory!\n");
  exit(-1);
}


/* Construct a circle or die.
 */
Circle *
new_circle(double scale)
{
  Circle *p = malloc(sizeof(Circle));
  
  p->radius = random_radius(scale);
  p->w = p->initial_w = 0.0;
  p->divisor = random_divisor();
  p->wdot = wdot_max / p->divisor;
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

Circle *
new_circle_chain(void)
{
  Circle *head;
  double scale = 1.0, factor;
  int n;

  /* Parent circles are larger than their children by a factor of at
   * least FACTOR_MIN and at most FACTOR_MAX.
   */
  factor = sizeFactorMin + frand(sizeFactorMax - sizeFactorMin);
  
  /* There are between minCircles and maxCircles in each figure.
   */
  if (maxCircles == minCircles)
    n = minCircles;            /* Avoid division by zero. */
  else
    n = minCircles + random() % (maxCircles - minCircles);
  
  head = NULL;
  while (n--)
    {
      Circle *p = new_circle(scale);
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
new_body(void)
{
  Body *p = malloc(sizeof(Body));
  if (NULL == p)
    oom();
  p->epicycles = new_circle_chain();
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
draw_body(Body *pb, GC gc)
{
  XDrawLine(dpy, window, gc, pb->old_x, pb->old_y, pb->x, pb->y);
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
colour_init(XWindowAttributes *pxgwa)
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
  if (colors)
    {
      free_colors(dpy, cmap, colors, ncolors);
      colors = 0;
      ncolors = 0;
    }
	
  ncolors = get_integer_resource ("colors", "Colors");
  if (0 == ncolors)		/* English spelling? */
    ncolors = get_integer_resource ("colours", "Colors");
  
  if (ncolors < 2)
    ncolors = 2;
  if (ncolors <= 2)
    mono_p = True;
  colors = 0;

  if (!mono_p)
    {
      colors = (XColor *) malloc(sizeof(*colors) * (ncolors+1));
      if (!colors)
	oom();
	  
      make_smooth_colormap (dpy, pxgwa->visual, cmap, colors, &ncolors,
			    True, /* allocate */
			    False, /* not writable */
			    True); /* verbose (complain about failure) */
      if (ncolors <= 2)
	{
	  if (colors)
	    free (colors);
	  colors = 0;
	  mono_p = True;
	}
    }

  
  bg = get_pixel_resource ("background", "Background", dpy, cmap);

  /* Set the line width
   */
  gcv.line_width = get_integer_resource ("lineWidth", "Integer");
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
  if (mono_p)
    fg = get_pixel_resource ("foreground", "Foreground", dpy, cmap);
  else
    fg = bg ^ get_pixel_resource (("color0"), "Foreground", dpy, cmap); 
  gcv.foreground = fg;
  valuemask |= GCForeground;

  /* Actually create the GC.
   */
  color0 = XCreateGC (dpy, window, valuemask, &gcv);
  
  return retval;
}


/* check_events(); originally from XScreensaver: hacks/maze.c,
 * but now quite heavily modified.
 *
 * Reaction to events:-
 *
 * Mouse 1 -- new figure }
 *       2 -- new figure }-- ignored when running on root window.
 *       3 -- exit       }
 *
 * Window resized or exposed -- new figure.
 * Window iconised -- wait until it's re-mapped, then start a new figure.
 */
static int
check_events (void)                        /* X event handler [ rhess ] */
{
  XEvent e;
  int unmapped = 0;
	
  while (unmapped || XPending(dpy))
    {
      XNextEvent(dpy, &e);
		
      switch (e.type)
	{
	case ButtonPress:
	  switch (e.xbutton.button)
	    {
	    case 3:
	      exit (0);
	      break;
				
	    case 2:
	    case 1:
	    default:
	      restart = 1 ;
	      stop = 0 ;
	      break;
	    }
	  break;
			
	case ConfigureNotify:
	  restart = 1;
	  break;
			
	case UnmapNotify:
	  printf("unmapped!\n");
	  unmapped = 1;
	  restart = 1;			/* restart with new fig. when re-mapped. */
	  break;
			
	case Expose:		
	  if (0 == e.xexpose.count)
	    {
				/* We can get several expose events in the queue.
				 * Only the last one has a zero count.  We eat
				 * events in this function so as to avoid restarting
				 * the screensaver many times in quick succession.
				 */
	      restart = 1;
	    }
	  /* If we had been unmapped and are now waiting to be re-mapped,
	   * indicate that we condition we are waiting for is now met.
	   */
	  if (unmapped)
	    printf("re-mapped!\n");
	  unmapped = 0;
	  break;

        default:
          screenhack_handle_event(dpy, &e);
          break;
	}
		
      /* If we're unmapped, don't return to the caller.  This
       * prevents us wasting CPU, calculating new positions for
       * things that will never be plotted.   This is a real CPU
       * saver.
       */
      if (!unmapped)
	return 1;
    }
  return 0;
}


static void
setup(void)
{
  XWindowAttributes xgwa;
  int root;
  
  XGetWindowAttributes (dpy, window, &xgwa);
  cmap = xgwa.colormap;

  width = xgwa.width;
  height = xgwa.height;
  x_offset = width / 2;
  y_offset = height / 2;
  unit_pixels = width < height ? width : height;

  {
    static Bool done = False;
    if (!done)
      {
	colour_init(&xgwa);
	done = True;
      }
  }
  
  root = get_boolean_resource("root", "Boolean");
  
  if (root)
    {
      XSelectInput(dpy, window, ExposureMask);
    }
  else
    {
      XGetWindowAttributes (dpy, window, &xgwa);
      XSelectInput (dpy, window,
                    xgwa.your_event_mask | ExposureMask | ButtonPressMask);
    }
  
}
static void
color_step(Body *pb, double frac)
{
  if (!mono_p)
    {
      int newshift = ncolors * fmod(frac * colour_cycle_rate, 1.0);
      if (newshift != color_shift_pos)
	{
	  pb->current_color = newshift;
	  XSetForeground (dpy, color0, colors[pb->current_color].pixel);
	  color_shift_pos = newshift;
	}
    }
}


long
distance(long x1, long y1, long x2, long y2)
{
  long dx, dy;

  dx = x2 - x1;
  dy = y2 - y1;
  return dx*dx + dy*dy;
}

#if 0
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
		    double xtime, double step,
		    int *x_max, int *y_max,
		    int *x_min, int *y_min)
{
  double t;
  
  move_body(pb, 0.0); /* move once to avoid initial line from origin */
  *x_min = *x_max = pb->x;
  *y_min = *y_max = pb->y;
  
  for (t=0.0; t<xtime; t += step)
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

static void rescale_circles(Body *pb,
			    int x_max, int y_max,
			    int x_min, int y_min)
{
  double xscale, yscale, scale;
  double xm, ym;
  
  x_max -= x_offset;
  x_min -= x_offset;
  y_max -= y_offset;
  y_min -= y_offset;

  x_max = i_max(x_max, -x_min);
  y_max = i_max(y_max, -y_min);


  xm = width / 2.0;
  ym = height / 2.0;
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
random_wdot_max(void)
{
  /* Maximum and minimum values for the choice of wdot_max.  Possible
   * epicycle speeds vary from wdot_max to (wdot_max * harmonics).
   */
  double minspeed, maxspeed;
  minspeed = get_float_resource("minSpeed", "Double");
  maxspeed = get_float_resource("maxSpeed", "Double");
  return harmonics * (minspeed + FULLCIRCLE * frand(maxspeed-minspeed));
}

/* this is the function called for your screensaver */
/*GLOBAL*/ void
screenhack(Display *disp, Window win)
{
  Body *pb = NULL;
  long l;
  double t, timestep, circle, xtime, timestep_coarse;
  int delay;
  int uncleared = 1;
  int xmax, xmin, ymax, ymin;
  int holdtime = get_integer_resource ("holdtime", "Integer");

  dpy = disp;
  window = win;

  circle = FULLCIRCLE;
  
  XClearWindow(dpy, window);
  uncleared = 0;
  
  delay = get_integer_resource ("delay", "Integer");
  harmonics = get_integer_resource("harmonics", "Integer");
  divisorPoisson = get_float_resource("divisorPoisson", "Double");
  
  timestep = get_float_resource("timestep", "Double");
  timestep_coarse = timestep *
    get_float_resource("timestepCoarseFactor", "Double");
  
  sizeFactorMin = get_float_resource("sizeFactorMin", "Double");
  sizeFactorMax = get_float_resource("sizeFactorMax", "Double");

  minCircles = get_integer_resource ("minCircles", "Integer");
  maxCircles = get_integer_resource ("maxCircles", "Integer");

  xtime = 0; /* is this right? */
  while (0 == stop)
    {
      setup(); /* do this inside the loop to cope with any window resizing */
      restart = 0;

      /* Flush any outstanding events; this has the side effect of
       * reducing the number of "false restarts"; resdtarts caused by
       * one event (e.g. ConfigureNotify) followed by another
       * (e.g. Expose).
       */
      XSync(dpy, True);
	  
      wdot_max = random_wdot_max();
	  
      if (pb)
	{
	  delete_body(pb);
	  pb = NULL;
	}
      pb = new_body();
      pb->x_origin = pb->x = x_offset;
      pb->y_origin = pb->y = y_offset;
	  
      
      if (uncleared)
	{
	  erase_full_window(dpy, window);
	  uncleared = 0;
	}

      fflush(stdout);
      precalculate_figure(pb, xtime, timestep_coarse,
			  &xmax, &ymax, &xmin, &ymin);

      rescale_circles(pb, xmax, ymax, xmin, ymin);
      
      move_body(pb, 0.0); /* move once to avoid initial line from origin */
      move_body(pb, 0.0); /* move once to avoid initial line from origin */

      
      t = 0.0;			/* start at time zero. */

      l = compute_divisor_lcm(pb->epicycles);
      
      colour_cycle_rate = fabs(l);
      
      xtime = fabs(l * circle / wdot_max);

      if (colors)				/* (colors==NULL) if mono_p */
	XSetForeground (dpy, color0, colors[pb->current_color].pixel);

      while (0 == restart)
	{
	  color_step(pb, t/xtime );
	  draw_body(pb, color0);
	  uncleared = 1;

	  
	  /* Check if the figure is complete...*/
	  if (t > xtime)
	    {
	      XSync (dpy, False);

              check_events();
	      if (holdtime)
		sleep(holdtime); /* show complete figure for a bit. */

	      restart = 1;	/* begin new figure. */
	    }
	  
	  
	  check_events();
	  if (delay)
	    usleep (delay);
	  
	  t += timestep;
	  move_body(pb, t);
	  check_events();
	}
    }
}

