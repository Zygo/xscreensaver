/* xscreensaver, Copyright (c) 1992, 1995 Jamie Zawinski <jwz@netscape.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* Simulation of a pair of quasi-gravitational fields, maybe sorta kinda
   a little like the strong and weak electromagnetic forces.  Derived from
   a Lispm screensaver by John Pezaris <pz@mit.edu>.  Mouse control and
   viscosity added by "Philip Edward Cutone, III" <pc2d+@andrew.cmu.edu>.

   John sez:

   The simulation started out as a purely accurate gravitational simulation,
   but, with constant simulation step size, I quickly realized the field being
   simulated while grossly gravitational was, in fact, non-conservative.  It
   also had the rather annoying behavior of dealing very badly with colliding
   orbs.  Therefore, I implemented a negative-gravity region (with two
   thresholds; as I read your code, you only implemented one) to prevent orbs
   from every coming too close together, and added a viscosity factor if the
   speed of any orb got too fast.  This provides a nice stable system with
   interesting behavior.

   I had experimented with a number of fields including the van der Waals
   force (very interesting orbiting behavior) and 1/r^3 gravity (not as
   interesting as 1/r^2).  An even normal viscosity (rather than the
   thresholded version to bleed excess energy) is also not interesting.
   The 1/r^2, -1/r^2, -10/r^2 thresholds proved not only robust but also
   interesting -- the orbs never collided and the threshold viscosity fixed
   the non-conservational problem.

   Philip sez:
   > An even normal viscosity (rather than the thresholded version to
   > bleed excess energy) is also not interesting.

   unless you make about 200 points.... set the viscosity to about .8
   and drag the mouse through it.   it makes a nice wave which travels
   through the field.
 */

#include <stdio.h>
#include <math.h>
#include "screenhack.h"
#include "spline.h"

struct ball {
  float x, y;
  float vx, vy;
  float dx, dy;
  float mass;
  int size;
  XColor color;
  int hue;
};

static unsigned int default_fg_pixel;
static struct ball *balls;
static int npoints;
static int threshold;
static int delay;
static int global_size;
static int segments;
static Bool glow_p;
static Bool orbit_p;
static XPoint *point_stack;
static int point_stack_size, point_stack_fp, pixel_stack_fp, pixel_stack_size;
static unsigned long *pixel_stack;
static unsigned int color_shift;

/*flip mods for mouse interaction*/
static Bool mouse_p;
int mouse_x, mouse_y, mouse_mass, root_x, root_y;
static float viscosity;

static enum object_mode {
  ball_mode, line_mode, polygon_mode, spline_mode, spline_filled_mode,
  tail_mode
} mode;

static enum color_mode {
  cycle_mode, random_mode
} cmode;

static GC draw_gc, erase_gc;

#define MAX_SIZE 16

#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

static void
init_balls (dpy, window)
     Display *dpy;
     Window window;
{
  int i;
  XWindowAttributes xgwa;
  XGCValues gcv;
  int xlim, ylim, midx, midy, r, vx, vy;
  double th;
  Colormap cmap;
  char *mode_str;
  XGetWindowAttributes (dpy, window, &xgwa);
  xlim = xgwa.width;
  ylim = xgwa.height;
  cmap = xgwa.colormap;
  midx = xlim/2;
  midy = ylim/2;
  r = get_integer_resource ("radius", "Integer");
  if (r <= 0 || r > min (xlim/2, ylim/2))
    r = min (xlim/2, ylim/2) - 50;
  vx = get_integer_resource ("vx", "Integer");
  vy = get_integer_resource ("vy", "Integer");
  npoints = get_integer_resource ("points", "Integer");
  if (npoints < 1)
    npoints = 3 + (random () % 5);
  balls = (struct ball *) malloc (npoints * sizeof (struct ball));
  segments = get_integer_resource ("segments", "Integer");
  if (segments < 0) segments = 1;
  threshold = get_integer_resource ("threshold", "Integer");
  if (threshold < 0) threshold = 0;
  delay = get_integer_resource ("delay", "Integer");
    if (delay < 0) delay = 0;
  global_size = get_integer_resource ("size", "Integer");
  if (global_size < 0) global_size = 0;
  glow_p = get_boolean_resource ("glow", "Boolean");
  orbit_p = get_boolean_resource ("orbit", "Boolean");
  color_shift = get_integer_resource ("colorShift", "Integer");
  if (color_shift >= 360) color_shift = 5;

  /*flip mods for mouse interaction*/
  mouse_p = get_boolean_resource ("mouse", "Boolean");
  mouse_mass = get_integer_resource ("mouseSize", "Integer");
  mouse_mass =  mouse_mass *  mouse_mass *10;

  viscosity = get_float_resource ("viscosity", "Float");

  mode_str = get_string_resource ("mode", "Mode");
  if (! mode_str) mode = ball_mode;
  else if (!strcmp (mode_str, "balls")) mode = ball_mode;
  else if (!strcmp (mode_str, "lines")) mode = line_mode;
  else if (!strcmp (mode_str, "polygons")) mode = polygon_mode;
  else if (!strcmp (mode_str, "tails")) mode = tail_mode;
  else if (!strcmp (mode_str, "splines")) mode = spline_mode;
  else if (!strcmp (mode_str, "filled-splines")) mode = spline_filled_mode;
  else {
    fprintf (stderr,
	     "%s: mode must be balls, lines, tails, polygons, splines, or\n\
	filled-splines, not \"%s\"\n",
	     progname, mode_str);
    exit (1);
  }

  mode_str = get_string_resource ("colorMode", "ColorMode");
  if (! mode_str) cmode = cycle_mode;
  else if (!strcmp (mode_str, "cycle")) cmode = cycle_mode;
  else if (!strcmp (mode_str, "random")) cmode = random_mode;
  else {
    fprintf (stderr, "%s: colorMode must be cycle or random, not \"%s\"\n",
	     progname, mode_str);
    exit (1);
  }

  if (mode != ball_mode && mode != tail_mode) glow_p = False;
  
  if (mode == polygon_mode && npoints < 3)
    mode = line_mode;

  if (mode != ball_mode)
    {
      int size = (segments ? segments : 1);
      point_stack_size = size * (npoints + 1);
      point_stack = (XPoint *) calloc (point_stack_size, sizeof (XPoint));
      point_stack_fp = 0;
      if (segments > 0)
	pixel_stack_size = segments;
      else
	pixel_stack_size = (360 / color_shift);
      pixel_stack = (unsigned long *)
	calloc (pixel_stack_size, sizeof (unsigned int));
      pixel_stack_fp = 0;
    }

  gcv.line_width = (mode == tail_mode
		    ? (global_size ? global_size : (MAX_SIZE * 2 / 3))
		    : 1);
  gcv.cap_style = (mode == tail_mode ? CapRound : CapButt);

  gcv.foreground = default_fg_pixel =
    get_pixel_resource ("foreground", "Foreground", dpy, cmap);
  draw_gc = XCreateGC (dpy, window, GCForeground|GCLineWidth|GCCapStyle, &gcv);
  gcv.foreground = get_pixel_resource ("background", "Background", dpy, cmap);
  erase_gc = XCreateGC (dpy, window, GCForeground|GCLineWidth|GCCapStyle,&gcv);

  if (!mono_p && mode != ball_mode)
    for (i = 0; i < pixel_stack_size; i++)
      {
	XColor color;
	color.pixel = default_fg_pixel;
	XQueryColor (dpy, cmap, &color);
	if (!XAllocColor (dpy, cmap, &color)) abort ();
	pixel_stack [i] = color.pixel;
      }

#define rand_size() min (MAX_SIZE, 8 + (random () % (MAX_SIZE - 9)))

  if (orbit_p && !global_size)
    /* To orbit, all objects must be the same mass, or the math gets
       really hairy... */
    global_size = rand_size ();

  th = frand (M_PI+M_PI);
  for (i = 0; i < npoints; i++)
    {
      int new_size = (global_size ? global_size : rand_size ());
      balls [i].dx = 0;
      balls [i].dy = 0;
      balls [i].size = new_size;
      balls [i].mass = (new_size * new_size * 10);
      balls [i].x = midx + r * cos (i * ((M_PI+M_PI) / npoints) + th);
      balls [i].y = midy + r * sin (i * ((M_PI+M_PI) / npoints) + th);
      if (! orbit_p)
	{
	  balls [i].vx = vx ? vx : ((6.0 - (random () % 11)) / 8.0);
	  balls [i].vy = vy ? vy : ((6.0 - (random () % 11)) / 8.0);
	}
      balls [i].color.pixel = default_fg_pixel;
      balls [i].color.flags = DoRed | DoGreen | DoBlue;
      if (!mono_p)
	{
	  if (i != 0 && (glow_p || mode != ball_mode))
	    balls [i].hue = balls [0].hue;
	  else
	    balls [i].hue = random () % 360;
	  hsv_to_rgb (balls [i].hue, 1.0, 1.0,
		      &balls [i].color.red, &balls [i].color.green,
		      &balls [i].color.blue);
	  if (!XAllocColor (dpy, cmap, &balls [i].color))
	    mono_p = True; /* just give up */
	}
    }

  if (orbit_p)
    {
      double a = 0;
      double v;
      double v_mult = get_float_resource ("vMult", "Float");
      if (v_mult == 0.0) v_mult = 1.0;

      for (i = 1; i < npoints; i++)
	{
          double _2ipi_n = (2 * i * M_PI / npoints);
	  double x = r * cos (_2ipi_n);
	  double y = r * sin (_2ipi_n);
	  double distx = r - x;
	  double dist2 = (distx * distx) + (y * y);
          double dist = sqrt (dist2);
          double a1 = ((balls[i].mass / dist2) *
                       ((dist < threshold) ? -1.0 : 1.0) *
                       (distx / dist));
	  a += a1;
	}
      if (a < 0.0)
	{
	  fprintf (stderr, "%s: domain error: forces on balls too great\n",
		   progname);
	  exit (-1);
	}
      v = sqrt (a * r) * v_mult;
      for (i = 0; i < npoints; i++)
	{
	  double k = ((2 * i * M_PI / npoints) + th);
	  balls [i].vx = -v * sin (k);
	  balls [i].vy =  v * cos (k);
	}
    }

  if (mono_p) glow_p = False;
  XClearWindow (dpy, window);
}

static void
compute_force (i, dx_ret, dy_ret)
     int i;
     float *dx_ret, *dy_ret;
{
  int j;
  float x_dist, y_dist, dist, dist2;
  *dx_ret = 0;
  *dy_ret = 0;
  for (j = 0; j < npoints; j++)
    {
      float x_dist, y_dist, dist, dist2;

      if (i == j) continue;
      x_dist = balls [j].x - balls [i].x;
      y_dist = balls [j].y - balls [i].y;
      dist2 = (x_dist * x_dist) + (y_dist * y_dist);
      dist = sqrt (dist2);
	      
      if (dist > 0.1) /* the balls are not overlapping */
	{
	  float new_acc = ((balls[j].mass / dist2) *
			   ((dist < threshold) ? -1.0 : 1.0));
	  float new_acc_dist = new_acc / dist;
	  *dx_ret += new_acc_dist * x_dist;
	  *dy_ret += new_acc_dist * y_dist;
	}
      else
	{		/* the balls are overlapping; move randomly */
	  *dx_ret += (frand (10.0) - 5.0);
	  *dy_ret += (frand (10.0) - 5.0);
	}
    }

  if (mouse_p)
    {
      x_dist = mouse_x - balls [i].x;
      y_dist = mouse_y - balls [i].y;
      dist2 = (x_dist * x_dist) + (y_dist * y_dist);
      dist = sqrt (dist2);
	
      if (dist > 0.1) /* the balls are not overlapping */
	{
	  float new_acc = ((mouse_mass / dist2) *
			   ((dist < threshold) ? -1.0 : 1.0));
	  float new_acc_dist = new_acc / dist;
	  *dx_ret += new_acc_dist * x_dist;
	  *dy_ret += new_acc_dist * y_dist;
	}
      else
	{		/* the balls are overlapping; move randomly */
	  *dx_ret += (frand (10.0) - 5.0);
	  *dy_ret += (frand (10.0) - 5.0);
	}
    }
}

static void
run_balls (dpy, window)
     Display *dpy;
     Window window;
{
  int last_point_stack_fp = point_stack_fp;
  static int tick = 500, xlim, ylim;
  static Colormap cmap;
  int i;

  /*flip mods for mouse interaction*/
  Window  root1, child1;
  int mask;
  if (mouse_p)
    {
      XQueryPointer(dpy, window, &root1, &child1,
		    &root_x, &root_y, &mouse_x, &mouse_y, &mask);
    }

  if (tick++ == 500)
    {
      XWindowAttributes xgwa;
      XGetWindowAttributes (dpy, window, &xgwa);
      tick = 0;
      xlim = xgwa.width;
      ylim = xgwa.height;
      cmap = xgwa.colormap;
    }

  /* compute the force of attraction/repulsion among all balls */
  for (i = 0; i < npoints; i++)
    compute_force (i, &balls[i].dx, &balls[i].dy);

  /* move the balls according to the forces now in effect */
  for (i = 0; i < npoints; i++)
    {
      float old_x = balls[i].x;
      float old_y = balls[i].y;
      float new_x, new_y;
      int size = balls[i].size;
      balls[i].vx += balls[i].dx;
      balls[i].vy += balls[i].dy;

      /* don't let them get too fast: impose a terminal velocity
         (actually, make the medium have friction) */
      if (balls[i].vx > 10)
	{
	  balls[i].vx *= 0.9;
	  balls[i].dx = 0;
	}
      else if (viscosity != 1)
	{
	  balls[i].vx *= viscosity;
	}

      if (balls[i].vy > 10)
	{
	  balls[i].vy *= 0.9;
	  balls[i].dy = 0;
	}
      else if (viscosity != 1)
	{
	  balls[i].vy *= viscosity;
	}

      balls[i].x += balls[i].vx;
      balls[i].y += balls[i].vy;

      /* bounce off the walls */
      if (balls[i].x >= (xlim - balls[i].size))
	{
	  balls[i].x = (xlim - balls[i].size - 1);
	  if (balls[i].vx > 0)
	    balls[i].vx = -balls[i].vx;
	}
      if (balls[i].y >= (ylim - balls[i].size))
	{
	  balls[i].y = (ylim - balls[i].size - 1);
	  if (balls[i].vy > 0)
	    balls[i].vy = -balls[i].vy;
	}
      if (balls[i].x <= 0)
	{
	  balls[i].x = 0;
	  if (balls[i].vx < 0)
	    balls[i].vx = -balls[i].vx;
	}
      if (balls[i].y <= 0)
	{
	  balls[i].y = 0;
	  if (balls[i].vy < 0)
	    balls[i].vy = -balls[i].vy;
	}

      new_x = balls[i].x;
      new_y = balls[i].y;

      /* make color saturation be related to particle acceleration. */
      if (glow_p)
	{
	  float limit = 0.5;
	  double s, v, fraction;
	  float vx = balls [i].dx;
	  float vy = balls [i].dy;
	  XColor new_color;
	  if (vx < 0) vx = -vx;
	  if (vy < 0) vy = -vy;
	  fraction = vx + vy;
	  if (fraction > limit) fraction = limit;

	  s = 1 - (fraction / limit);
	  v = 1.0;

	  s = (s * 0.75) + 0.25;

	  hsv_to_rgb (balls [i].hue, s, v, 
		      &new_color.red, &new_color.green, &new_color.blue);
	  if (XAllocColor (dpy, cmap, &new_color))
	    {
	      XFreeColors (dpy, cmap, &balls [i].color.pixel, 1, 0);
	      balls [i].color = new_color;
	    }
	}

      if (mode == ball_mode)
	{
	  if (!mono_p)
	    XSetForeground (dpy, draw_gc, balls [i].color.pixel);
	  XFillArc (dpy, window, erase_gc, (int) old_x, (int) old_y,
		    size, size, 0, 360*64);
	  XFillArc (dpy, window, draw_gc,  (int) new_x, (int) new_y,
		    size, size, 0, 360*64);
	}
      if (mode != ball_mode)
	{
	  point_stack [point_stack_fp].x = new_x;
	  point_stack [point_stack_fp].y = new_y;
	  point_stack_fp++;
	}
    }

  /* draw the lines or polygons after computing all points */
  if (mode != ball_mode)
    {
      point_stack [point_stack_fp].x = balls [0].x; /* close the polygon */
      point_stack [point_stack_fp].y = balls [0].y;
      point_stack_fp++;
      if (point_stack_fp == point_stack_size)
	point_stack_fp = 0;
      else if (point_stack_fp > point_stack_size) /* better be aligned */
	abort ();
      if (!mono_p)
	{
	  XColor color2, desired;
	  color2 = balls [0].color;
	  switch (cmode)
	    {
	    case cycle_mode:
	      cycle_hue (&color2, color_shift);
	      break;
	    case random_mode:
	      color2.red =   random () % 65535;
	      color2.green = random () % 65535;
	      color2.blue =  random () % 65535;
	      break;
	    default:
	      abort ();
	    }
	      
	  desired = color2;
	  if (XAllocColor (dpy, cmap, &color2))
	    {
	      /* XAllocColor returns the actual RGB that the hardware let us
		 allocate.  Restore the requested values into the XColor struct
		 so that limited-resolution hardware doesn't cause cycle_hue to
		 get "stuck". */
	      color2.red = desired.red;
	      color2.green = desired.green;
	      color2.blue = desired.blue;
	    }
	  else
	    {
	      color2 = balls [0].color;
	      if (!XAllocColor (dpy, cmap, &balls [0].color))
		abort ();
	    }
	  pixel_stack [pixel_stack_fp++] = balls [0].color.pixel;
	  if (pixel_stack_fp >= pixel_stack_size)
	    pixel_stack_fp = 0;
	  XFreeColors (dpy, cmap, pixel_stack + pixel_stack_fp, 1, 0);
	  balls [0].color = color2;
	  XSetForeground (dpy, draw_gc, balls [0].color.pixel);
	}
    }

  switch (mode)
    {
    case ball_mode:
      break;
    case line_mode:
      if (segments > 0)
	XDrawLines (dpy, window, erase_gc, point_stack + point_stack_fp,
		    npoints + 1, CoordModeOrigin);
      XDrawLines (dpy, window, draw_gc, point_stack + last_point_stack_fp,
		  npoints + 1, CoordModeOrigin);
      break;
    case polygon_mode:
      if (segments > 0)
	XFillPolygon (dpy, window, erase_gc, point_stack + point_stack_fp,
		      npoints + 1, (npoints == 3 ? Convex : Complex),
		      CoordModeOrigin);
      XFillPolygon (dpy, window, draw_gc, point_stack + last_point_stack_fp,
		    npoints + 1, (npoints == 3 ? Convex : Complex),
		    CoordModeOrigin);
      break;
    case tail_mode:
      {
	int i;
	for (i = 0; i < npoints; i++)
	  {
	    int index = point_stack_fp + i;
	    int next_index = (index + (npoints + 1)) % point_stack_size;
	    XDrawLine (dpy, window, erase_gc,
		       point_stack [index].x,
		       point_stack [index].y,
		       point_stack [next_index].x,
		       point_stack [next_index].y);

	    index = last_point_stack_fp + i;
	    next_index = (index - (npoints + 1)) % point_stack_size;
	    if (next_index < 0) next_index += point_stack_size;
	    if (point_stack [next_index].x == 0 &&
		point_stack [next_index].y == 0)
	      continue;
	    XDrawLine (dpy, window, draw_gc,
		       point_stack [index].x,
		       point_stack [index].y,
		       point_stack [next_index].x,
		       point_stack [next_index].y);
	  }
      }
      break;
    case spline_mode:
    case spline_filled_mode:
      {
	int i;
	static spline *s = 0;
	if (! s) s = make_spline (npoints);
	if (segments > 0)
	  {
	    for (i = 0; i < npoints; i++)
	      {
		s->control_x [i] = point_stack [point_stack_fp + i].x;
		s->control_y [i] = point_stack [point_stack_fp + i].y;
	      }
	    compute_closed_spline (s);
	    if (mode == spline_filled_mode)
	      XFillPolygon (dpy, window, erase_gc, s->points, s->n_points,
			    (s->n_points == 3 ? Convex : Complex),
			    CoordModeOrigin);
	    else
	      XDrawLines (dpy, window, erase_gc, s->points, s->n_points,
			  CoordModeOrigin);
	  }
	for (i = 0; i < npoints; i++)
	  {
	    s->control_x [i] = point_stack [last_point_stack_fp + i].x;
	    s->control_y [i] = point_stack [last_point_stack_fp + i].y;
	  }
	compute_closed_spline (s);
	if (mode == spline_filled_mode)
	  XFillPolygon (dpy, window, draw_gc, s->points, s->n_points,
			(s->n_points == 3 ? Convex : Complex),
			CoordModeOrigin);
	else
	  XDrawLines (dpy, window, draw_gc, s->points, s->n_points,
		      CoordModeOrigin);
      }
      break;
    default:
      abort ();
    }

  XSync (dpy, True);
}


char *progclass = "Attraction";

char *defaults [] = {
  "Attraction.background:	black",		/* to placate SGI */
  "Attraction.foreground:	white",
  "*mode:	balls",
  "*points:	0",
  "*size:	0",
  "*threshold:	100",
  "*delay:	10000",
  "*glow:	false",
  "*mouseSize:	10",
  "*mouse:	false",
  "*viscosity:	1",
  "*orbit:	false",
  "*colorShift:	3",
  "*segments:	100",
  0
};

XrmOptionDescRec options [] = {
  { "-mode",		".mode",	XrmoptionSepArg, 0 },
  { "-points",		".points",	XrmoptionSepArg, 0 },
  { "-threshold",	".threshold",	XrmoptionSepArg, 0 },
  { "-segments",	".segments",	XrmoptionSepArg, 0 },
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-size",		".size",	XrmoptionSepArg, 0 },
  { "-color-mode",	".colorMode",	XrmoptionSepArg, 0 },
  { "-color-shift",	".colorShift",	XrmoptionSepArg, 0 },
  { "-radius",		".radius",	XrmoptionSepArg, 0 },
  { "-vx",		".vx",		XrmoptionSepArg, 0 },
  { "-vy",		".vy",		XrmoptionSepArg, 0 },
  { "-vmult",		".vMult",	XrmoptionSepArg, 0 },
  { "-mouse-size",	".mouseSize",	XrmoptionSepArg, 0 },
  { "-mouse",		".mouse",	XrmoptionNoArg, "true" },
  { "-nomouse",		".mouse",	XrmoptionNoArg, "false" },
  { "-viscosity",	".viscosity",	XrmoptionSepArg, 0 },
  { "-glow",		".glow",	XrmoptionNoArg, "true" },
  { "-noglow",		".glow",	XrmoptionNoArg, "false" },
  { "-orbit",		".orbit",	XrmoptionNoArg, "true" }
};
int options_size = (sizeof (options) / sizeof (options[0]));

void
screenhack (dpy, window)
     Display *dpy;
     Window window;
{
  init_balls (dpy, window);
  while (1)
    {
      run_balls (dpy, window);
      if (delay) usleep (delay);
    }
}
