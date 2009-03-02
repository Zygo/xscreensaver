/* xscreensaver, Copyright (c) 1992, 1995, 1996, 1997, 1998, 2001
 *  Jamie Zawinski <jwz@jwz.org>
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

       The simulation started out as a purely accurate gravitational
       simulation, but, with constant simulation step size, I quickly
       realized the field being simulated while grossly gravitational
       was, in fact, non-conservative.  It also had the rather annoying
       behavior of dealing very badly with colliding orbs.  Therefore,
       I implemented a negative-gravity region (with two thresholds; as
       I read your code, you only implemented one) to prevent orbs from
       every coming too close together, and added a viscosity factor if
       the speed of any orb got too fast.  This provides a nice stable
       system with interesting behavior.

       I had experimented with a number of fields including the van der
       Waals force (very interesting orbiting behavior) and 1/r^3
       gravity (not as interesting as 1/r^2).  An even normal viscosity
       (rather than the thresholded version to bleed excess energy) is
       also not interesting.  The 1/r^2, -1/r^2, -10/r^2 thresholds
       proved not only robust but also interesting -- the orbs never
       collided and the threshold viscosity fixed the
       non-conservational problem.

   Philip sez:
       > An even normal viscosity (rather than the thresholded version to
       > bleed excess energy) is also not interesting.

       unless you make about 200 points.... set the viscosity to about .8
       and drag the mouse through it.   it makes a nice wave which travels
       through the field.

   And (always the troublemaker) Joe Keane <jgk@jgk.org> sez:

       Despite what John sez, the field being simulated is always
       conservative.  The real problem is that it uses a simple hack,
       computing acceleration *based only on the starting position*,
       instead of a real differential equation solver.  Thus you'll
       always have energy coming out of nowhere, although it's most
       blatant when balls get close together.  If it were done right,
       you wouldn't need viscosity or artificial limits on how close
       the balls can get.

   Matt <straitm@carleton.edu> sez:

       Added a switch to remove the walls.

       Added a switch to make the threshold viscosity optional.  If
       nomaxspeed is specified, then balls going really fast do not
       recieve special treatment.

       I've made tail mode prettier by eliminating the first erase line
       that drew from the upper left corner to the starting position of
       each point.

       Made the balls in modes other than "balls" bounce exactly at the
       walls.  (Because the graphics for different modes are drawn
       differently with respect to the "actual" position of the point,
       they used to be able to run somewhat past the walls, or bounce
       before hitting them.)

       Added an option to output each ball's speed in the form of a bar
       graph drawn on the same window as the balls.  If only x or y is
       selected, they will be represented on the appropriate axis down
       the center of the window.  If both are selected, they will both
       be displayed along the diagonal such that the x and y bars for
       each point start at the same place.  If speed is selected, the
       speed will be displayed down the left side.  */

#include <stdio.h>
#include <math.h>
#include "screenhack.h"
#include "spline.h"

struct ball {
  double x, y;
  double vx, vy;
  double dx, dy;
  double mass;
  int size;
  int pixel_index;
  int hue;
};

static struct ball *balls;
static double *x_vels;
static double *y_vels;
static double *speeds;
static int npoints;
static int threshold;
static int delay;
static int global_size;
static int segments;
static Bool glow_p;
static Bool orbit_p;
static Bool walls_p;
static Bool maxspeed_p;
static Bool cbounce_p;
static XPoint *point_stack;
static int point_stack_size, point_stack_fp;
static XColor *colors;
static int ncolors;
static int fg_index;
static int color_shift;
Bool no_erase_yet; /* for tail mode fix */

/*flip mods for mouse interaction*/
static Bool mouse_p;
int mouse_x, mouse_y, mouse_mass, root_x, root_y;
static double viscosity;

static enum object_mode {
  ball_mode, line_mode, polygon_mode, spline_mode, spline_filled_mode,
  tail_mode
} mode;

static enum graph_mode {
  graph_none, graph_x, graph_y, graph_both, graph_speed
} graph_mode;

static GC draw_gc, erase_gc;

/* The normal (and max) width for a graph bar */
#define BAR_SIZE 11
#define MAX_SIZE 16
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

static void
init_balls (Display *dpy, Window window)
{
  int i;
  XWindowAttributes xgwa;
  XGCValues gcv;
  int xlim, ylim, midx, midy, r, vx, vy;
  double th;
  Colormap cmap;
  char *mode_str, *graph_mode_str;

  XGetWindowAttributes (dpy, window, &xgwa);
  xlim = xgwa.width;
  ylim = xgwa.height;
  cmap = xgwa.colormap;
  midx = xlim/2;
  midy = ylim/2;
  walls_p = get_boolean_resource ("walls", "Boolean");

  /* if there aren't walls, don't set a limit on the radius */
  r = get_integer_resource ("radius", "Integer");
  if (r <= 0 || (r > min (xlim/2, ylim/2) && walls_p) )
    r = min (xlim/2, ylim/2) - 50;

  vx = get_integer_resource ("vx", "Integer");
  vy = get_integer_resource ("vy", "Integer");

  npoints = get_integer_resource ("points", "Integer");
  if (npoints < 1)
    npoints = 3 + (random () % 5);
  balls = (struct ball *) malloc (npoints * sizeof (struct ball));

  no_erase_yet = 1; /* for tail mode fix */

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

  maxspeed_p = get_boolean_resource ("maxspeed", "Boolean");

  cbounce_p = get_boolean_resource ("cbounce", "Boolean");

  color_shift = get_integer_resource ("colorShift", "Integer");
  if (color_shift <= 0) color_shift = 5;

  /*flip mods for mouse interaction*/
  mouse_p = get_boolean_resource ("mouse", "Boolean");
  mouse_mass = get_integer_resource ("mouseSize", "Integer");
  mouse_mass =  mouse_mass *  mouse_mass *10;

  viscosity = get_float_resource ("viscosity", "Float");

  mode_str = get_string_resource ("mode", "Mode");
  if (! mode_str) mode = ball_mode;
  else if (!strcmp (mode_str, "balls")) 	mode = ball_mode;
  else if (!strcmp (mode_str, "lines")) 	mode = line_mode;
  else if (!strcmp (mode_str, "polygons")) 	mode = polygon_mode;
  else if (!strcmp (mode_str, "tails")) 	mode = tail_mode;
  else if (!strcmp (mode_str, "splines")) 	mode = spline_mode;
  else if (!strcmp (mode_str, "filled-splines"))mode = spline_filled_mode;
  else {
    fprintf (stderr,
	     "%s: mode must be balls, lines, tails, polygons, splines, or\n\
	filled-splines, not \"%s\"\n",
	     progname, mode_str);
    exit (1);
  }

  graph_mode_str = get_string_resource ("graphmode", "Mode");
  if (! graph_mode_str) graph_mode = graph_none;
  else if (!strcmp (graph_mode_str, "x")) 	graph_mode = graph_x;
  else if (!strcmp (graph_mode_str, "y")) 	graph_mode = graph_y;
  else if (!strcmp (graph_mode_str, "both")) 	graph_mode = graph_both;
  else if (!strcmp (graph_mode_str, "speed")) 	graph_mode = graph_speed;
  else if (!strcmp (graph_mode_str, "none")) 	graph_mode = graph_none;
  else {
    fprintf (stderr,
	 "%s: graphmode must be speed, x, y, both, or none, not \"%s\"\n",
	 progname, graph_mode_str);
    exit (1);
  }

  /* only allocate memory if it is needed */
  if(graph_mode != graph_none)
  {
    if(graph_mode == graph_x || graph_mode == graph_both)
      x_vels = (double *) malloc (npoints * sizeof (double));
    if(graph_mode == graph_y || graph_mode == graph_both)
      y_vels = (double *) malloc (npoints * sizeof (double));
    if(graph_mode == graph_speed)
      speeds = (double *) malloc (npoints * sizeof (double));
  }

  if (mode != ball_mode && mode != tail_mode) glow_p = False;
  
  if (mode == polygon_mode && npoints < 3)
    mode = line_mode;

  ncolors = get_integer_resource ("colors", "Colors");
  if (ncolors < 2) ncolors = 2;
  if (ncolors <= 2) mono_p = True;
  colors = 0;

  if (!mono_p)
    {
      fg_index = 0;
      switch (mode)
	{
	case ball_mode:
	  if (glow_p)
	    {
	      int H = random() % 360;
	      double S1 = 0.25;
	      double S2 = 1.00;
	      double V = frand(0.25) + 0.75;
	      colors = (XColor *) malloc(sizeof(*colors) * (ncolors+1));
	      make_color_ramp (dpy, cmap, H, S1, V, H, S2, V, colors, &ncolors,
			       False, True, False);
	    }
	  else
	    {
	      ncolors = npoints;
	      colors = (XColor *) malloc(sizeof(*colors) * (ncolors+1));
	      make_random_colormap (dpy, xgwa.visual, cmap, colors, &ncolors,
				    True, True, False, True);
	    }
	  break;
	case line_mode:
	case polygon_mode:
	case spline_mode:
	case spline_filled_mode:
	case tail_mode:
	  colors = (XColor *) malloc(sizeof(*colors) * (ncolors+1));
	  make_smooth_colormap (dpy, xgwa.visual, cmap, colors, &ncolors,
				True, False, True);
	  break;
	default:
	  abort ();
	}
    }

  if (!mono_p && ncolors <= 2)
    {
      if (colors) free (colors);
      colors = 0;
      mono_p = True;
    }

  if (mode != ball_mode)
    {
      int size = (segments ? segments : 1);
      point_stack_size = size * (npoints + 1);
      point_stack = (XPoint *) calloc (point_stack_size, sizeof (XPoint));
      point_stack_fp = 0;
    }

  gcv.line_width = (mode == tail_mode
		    ? (global_size ? global_size : (MAX_SIZE * 2 / 3))
		    : 1);
  gcv.cap_style = (mode == tail_mode ? CapRound : CapButt);

  if (mono_p)
    gcv.foreground = get_pixel_resource("foreground", "Foreground", dpy, cmap);
  else
    gcv.foreground = colors[fg_index].pixel;
  draw_gc = XCreateGC (dpy, window, GCForeground|GCLineWidth|GCCapStyle, &gcv);

  gcv.foreground = get_pixel_resource("background", "Background", dpy, cmap);
  erase_gc = XCreateGC (dpy, window, GCForeground|GCLineWidth|GCCapStyle,&gcv);


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
      if (mono_p || mode != ball_mode)
	balls [i].pixel_index = -1;
      else if (glow_p)
	balls [i].pixel_index = 0;
      else
	balls [i].pixel_index = random() % ncolors;
    }

  /*  This lets modes where the points don't really have any size use the whole
      window.  Otherwise, since the points still have a positive size
      assigned to them, they will be bounced somewhat early.  Mass and size are
      seperate, so this shouldn't cause problems.  It's a bit kludgy, tho.
  */
  if(mode == line_mode || mode == spline_mode || 
     mode == spline_filled_mode || mode == polygon_mode)
    {
	for(i = 1; i < npoints; i++)
	  {
		balls[i].size = 0;
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
compute_force (int i, double *dx_ret, double *dy_ret)
{
  int j;
  double x_dist, y_dist, dist, dist2;
  *dx_ret = 0;
  *dy_ret = 0;
  for (j = 0; j < npoints; j++)
    {
      if (i == j) continue;
      x_dist = balls [j].x - balls [i].x;
      y_dist = balls [j].y - balls [i].y;
      dist2 = (x_dist * x_dist) + (y_dist * y_dist);
      dist = sqrt (dist2);
	      
      if (dist > 0.1) /* the balls are not overlapping */
	{
	  double new_acc = ((balls[j].mass / dist2) *
			    ((dist < threshold) ? -1.0 : 1.0));
	  double new_acc_dist = new_acc / dist;
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
	  double new_acc = ((mouse_mass / dist2) *
			    ((dist < threshold) ? -1.0 : 1.0));
	  double new_acc_dist = new_acc / dist;
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


/* Draws meters along the diagonal for the x velocity */
static void 
draw_meter_x(Display *dpy, Window window, GC draw_gc,
             struct ball *balls, int i, int alone) 
{
  XWindowAttributes xgwa;
  int x1,x2,y,w1,w2,h;
  XGetWindowAttributes (dpy, window, &xgwa);

  /* set the width of the bars to use */
  if(xgwa.height < BAR_SIZE*npoints)
    {
      y = i*(xgwa.height/npoints);
      h = (xgwa.height/npoints) - 2;
    }
  else
    {
      y = BAR_SIZE*i;
      h = BAR_SIZE - 2;
    }
  
  if(alone)
    {
      x1 = xgwa.width/2;
      x2 = x1;
    }
  else
    {
      x1 = i*(h+2);
      if(x1 < i) 
        x1 = i;
      x2 = x1;
    }

  if(y<1) y=i;  
  if(h<1) h=1;

  w1 = (int)(20*x_vels[i]);
  w2 = (int)(20*balls[i].vx);
  x_vels[i] = balls[i].vx; 

  if (w1<0) {
    w1=-w1;
    x1=x1-w1;
  }
  if (w2<0) {
    w2=-w2;
    x2=x2-w2;
  }
  XDrawRectangle(dpy,window,erase_gc,x1+(h+2)/2,y,w1,h);
  XDrawRectangle(dpy,window,draw_gc,x2+(h+2)/2,y,w2,h);
}

/* Draws meters along the diagonal for the y velocity.
   Is there some way to make draw_meter_x and draw_meter_y 
   one function instead of two without making them completely unreadable?
*/
static void 
draw_meter_y (Display *dpy, Window window, GC draw_gc,
              struct ball *balls, int i, int alone) 
{
  XWindowAttributes xgwa;
  int y1,y2,x,h1,h2,w;
  XGetWindowAttributes (dpy, window, &xgwa);

  if(xgwa.height < BAR_SIZE*npoints){  /*needs to be height still */
    x = i*(xgwa.height/npoints);
    w = (xgwa.height/npoints) - 2;
  }
  else{
    x = BAR_SIZE*i;
    w = BAR_SIZE - 2;
  }

  if(alone)
    {
      y1 = xgwa.height/2;
      y2 = y1;
    }
  else
    {
      y1 = i*(w+2);
      if(y1 < i)
        y1 = i;
      y2 = y1;
    }

  if(x < 1) x = i;  
  if(w < 1) w = 1;

  h1 = (int)(20*y_vels[i]);
  h2 = (int)(20*balls[i].vy);
  y_vels[i] = balls[i].vy; 

  if (h1<0) {
    h1=-h1;
    y1=y1-h1;
  }
  if (h2<0) {
    h2=-h2;
    y2=y2-h2;
  }
  XDrawRectangle(dpy,window,erase_gc,x,y1+(w+2)/2,w,h1);
  XDrawRectangle(dpy,window,draw_gc,x,y2+(w+2)/2,w,h2);
}


/* Draws meters of the total speed of the balls */
static void
draw_meter_speed (Display *dpy, Window window, GC draw_gc, 
                  struct ball *balls, int i) 
{
  XWindowAttributes xgwa;
  int y,x1,x2,h,w1,w2;
  XGetWindowAttributes (dpy, window, &xgwa);

  if(xgwa.height < BAR_SIZE*npoints)
    {
      y = i*(xgwa.height/npoints);
      h = (xgwa.height/npoints) - 2;
    }
  else{
    y = BAR_SIZE*i;
    h = BAR_SIZE - 2;
  }

  x1 = 0;
  x2 = x1;

  if(y < 1) y = i;  
  if(h < 1) h = 1;

  w1 = (int)(5*speeds[i]);
  w2 = (int)(5*(balls[i].vy*balls[i].vy+balls[i].vx*balls[i].vx));
  speeds[i] =    balls[i].vy*balls[i].vy+balls[i].vx*balls[i].vx;

  XDrawRectangle(dpy,window,erase_gc,x1,y,w1,h);
  XDrawRectangle(dpy,window,draw_gc, x2,y,w2,h);
}

static void
run_balls (Display *dpy, Window window, int total_ticks)
{
  int last_point_stack_fp = point_stack_fp;
  static int tick = 500, xlim, ylim;
  static Colormap cmap;
  
  Window root1, child1;  /*flip mods for mouse interaction*/
  unsigned int mask;

  int i, radius = global_size/2;
  if(global_size == 0)
    radius = (MAX_SIZE / 3);

  if(graph_mode != graph_none)
    {
      if(graph_mode == graph_both)
	{
          for(i = 0; i < npoints; i++)
            {
              draw_meter_x(dpy,window,draw_gc, balls, i, 0);
              draw_meter_y(dpy,window,draw_gc, balls, i, 0);
            }
	}
      else if(graph_mode == graph_x)
	{
          for(i = 0; i < npoints; i++)
            {
              draw_meter_x(dpy,window,draw_gc, balls, i, 1);
            }
	}
      else if(graph_mode == graph_y)
	{
          for(i = 0; i < npoints; i++)
            {
              draw_meter_y(dpy,window,draw_gc, balls, i, 1);
            }
	}
      else if(graph_mode == graph_speed)
	{
          for(i = 0; i < npoints; i++)
            {
              draw_meter_speed(dpy,window,draw_gc, balls, i);
            }
	}

    }

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
      double old_x = balls[i].x;
      double old_y = balls[i].y;
      double new_x, new_y;
      int size = balls[i].size;
      balls[i].vx += balls[i].dx;
      balls[i].vy += balls[i].dy;

      /* "don't let them get too fast: impose a terminal velocity
         (actually, make the medium have friction)"
	 Well, what this first step really does is give the medium a 
	 viscosity of .9 for balls going over the speed limit.  Anyway, 
	 this is now optional
      */
      if (balls[i].vx > 10 && maxspeed_p)
	{
	  balls[i].vx *= 0.9;
	  balls[i].dx = 0;
	}
      else if (viscosity != 1)
	{
	  balls[i].vx *= viscosity;
	}

      if (balls[i].vy > 10 && maxspeed_p)
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


      /* bounce off the walls if desired
	 note: a ball is actually its upper left corner */
      if(walls_p)
 	{
	  if(cbounce_p)  /* with correct bouncing */
	    {
              /* so long as it's out of range, keep bouncing */
	
              while( (balls[i].x >= (xlim - balls[i].size)) ||
                     (balls[i].y >= (ylim - balls[i].size)) ||
                     (balls[i].x <= 0) ||
                     (balls[i].y <= 0) )
                {
                  if (balls[i].x >= (xlim - balls[i].size))
                    {
                      balls[i].x = (2*(xlim - balls[i].size) - balls[i].x);
                      balls[i].vx = -balls[i].vx;
                    }
                  if (balls[i].y >= (ylim - balls[i].size))
                    {
                      balls[i].y = (2*(ylim - balls[i].size) - balls[i].y);
                      balls[i].vy = -balls[i].vy;
                    }
                  if (balls[i].x <= 0)
                    {
                      balls[i].x = -balls[i].x;
		      balls[i].vx = -balls[i].vx;
                    }
                  if (balls[i].y <= 0)
                    {
                      balls[i].y = -balls[i].y;
		      balls[i].vy = -balls[i].vy;
                    }
                }
            }
          else  /* with old bouncing */
            {
              if (balls[i].x >= (xlim - balls[i].size))
                {
                  balls[i].x = (xlim - balls[i].size - 1);
                  if (balls[i].vx > 0) /* why is this check here? */
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
            }
        }
      new_x = balls[i].x;
      new_y = balls[i].y;

      if (!mono_p)
	{
	  if (mode == ball_mode)
	    {
	      if (glow_p)
		{
		  /* make color saturation be related to particle
		     acceleration. */
		  double limit = 0.5;
		  double s, fraction;
		  double vx = balls [i].dx;
		  double vy = balls [i].dy;
		  if (vx < 0) vx = -vx;
		  if (vy < 0) vy = -vy;
		  fraction = vx + vy;
		  if (fraction > limit) fraction = limit;

		  s = 1 - (fraction / limit);
		  balls[i].pixel_index = (ncolors * s);
		}
	      XSetForeground (dpy, draw_gc,
			      colors[balls[i].pixel_index].pixel);
	    }
	}

      if (mode == ball_mode)
	{
	  XFillArc (dpy, window, erase_gc, (int) old_x, (int) old_y,
		    size, size, 0, 360*64);
	  XFillArc (dpy, window, draw_gc,  (int) new_x, (int) new_y,
		    size, size, 0, 360*64);
	}
      else
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
	  static int tick = 0;
	  if (tick++ == color_shift)
	    {
	      tick = 0;
	      fg_index = (fg_index + 1) % ncolors;
	      XSetForeground (dpy, draw_gc, colors[fg_index].pixel);
	    }
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
	for (i = 0; i < npoints; i++)
	  {
	    int index = point_stack_fp + i;
	    int next_index = (index + (npoints + 1)) % point_stack_size;
            if(no_erase_yet == 1)
	      {
		if(total_ticks >= segments)
                  {
                    no_erase_yet = 0;
                    XDrawLine (dpy, window, erase_gc,
			       point_stack [index].x + radius,
			       point_stack [index].y + radius,
			       point_stack [next_index].x + radius,
			       point_stack [next_index].y + radius);
                  }
	      }
	    else
	      {
	    	XDrawLine (dpy, window, erase_gc,
                           point_stack [index].x + radius,
                           point_stack [index].y + radius,
                           point_stack [next_index].x + radius,
                           point_stack [next_index].y + radius);
	      }
	    index = last_point_stack_fp + i;
	    next_index = (index - (npoints + 1)) % point_stack_size;
	    if (next_index < 0) next_index += point_stack_size;
	    if (point_stack [next_index].x == 0 &&
		point_stack [next_index].y == 0)
	      continue;
	    XDrawLine (dpy, window, draw_gc,
		       point_stack [index].x + radius,
		       point_stack [index].y + radius,
		       point_stack [next_index].x + radius,
		       point_stack [next_index].y + radius);
	  }
      }
      break;
    case spline_mode:
    case spline_filled_mode:
      {
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

  XSync (dpy, False);
}


char *progclass = "Attraction";

char *defaults [] = {
  ".background:	black",
  ".foreground:	white",
  "*mode:	balls",
  "*graphmode:  none",
  "*points:	0",
  "*size:	0",
  "*colors:	200",
  "*threshold:	100",
  "*delay:	10000",
  "*glow:	false",
  "*mouseSize:	10",
  "*walls:	true",
  "*maxspeed:	true",
  "*cbounce:	true",
  "*mouse:	false",
  "*viscosity:	1",
  "*orbit:	false",
  "*colorShift:	3",
  "*segments:	500",
  "*vMult:	0.9",
  0
};

XrmOptionDescRec options [] = {
  { "-mode",		".mode",	XrmoptionSepArg, 0 },
  { "-graphmode",	".graphmode",   XrmoptionSepArg, 0 },
  { "-colors",		".colors",	XrmoptionSepArg, 0 },
  { "-points",		".points",	XrmoptionSepArg, 0 },
  { "-color-shift",	".colorShift",	XrmoptionSepArg, 0 },
  { "-threshold",	".threshold",	XrmoptionSepArg, 0 },
  { "-segments",	".segments",	XrmoptionSepArg, 0 },
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-size",		".size",	XrmoptionSepArg, 0 },
  { "-radius",		".radius",	XrmoptionSepArg, 0 },
  { "-vx",		".vx",		XrmoptionSepArg, 0 },
  { "-vy",		".vy",		XrmoptionSepArg, 0 },
  { "-vmult",		".vMult",	XrmoptionSepArg, 0 },
  { "-mouse-size",	".mouseSize",	XrmoptionSepArg, 0 },
  { "-viscosity",	".viscosity",	XrmoptionSepArg, 0 },
  { "-mouse",		".mouse",	XrmoptionNoArg, "true" },
  { "-nomouse",		".mouse",	XrmoptionNoArg, "false" },
  { "-glow",		".glow",	XrmoptionNoArg, "true" },
  { "-noglow",		".glow",	XrmoptionNoArg, "false" },
  { "-orbit",		".orbit",	XrmoptionNoArg, "true" },
  { "-nowalls",		".walls", 	XrmoptionNoArg, "false" },
  { "-walls",		".walls",	XrmoptionNoArg, "true" },
  { "-nomaxspeed",	".maxspeed",	XrmoptionNoArg, "false" },
  { "-maxspeed",	".maxspeed",	XrmoptionNoArg, "true" },
  { "-correct-bounce",	".cbounce",	XrmoptionNoArg, "false" },
  { "-fast-bounce",	".cbounce",	XrmoptionNoArg, "true" },
  { 0, 0, 0, 0 }
};

void
screenhack (Display *dpy, Window window)
{
  /* for tail mode fix */
  int total_ticks = 0;

  init_balls (dpy, window);
  while (1)
    {
      total_ticks++;
      run_balls (dpy, window, total_ticks);
      screenhack_handle_events (dpy);
      if (delay)
        usleep (delay);
    }
}
