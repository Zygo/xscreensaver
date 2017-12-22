/* xscreensaver, Copyright (c) 1992-2013 Jamie Zawinski <jwz@jwz.org>
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
   a Lispm screensaver by John Pezaris <pz@mit.edu>.  Viscosity added by
   Philip Edward Cutone, III <pc2d+@andrew.cmu.edu>.

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

/* The normal (and max) width for a graph bar */
#define BAR_SIZE 11
#define MAX_SIZE 16
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))


enum object_mode {
  ball_mode, line_mode, polygon_mode, spline_mode, spline_filled_mode,
  tail_mode
};

enum graph_mode {
  graph_none, graph_x, graph_y, graph_both, graph_speed
};

struct ball {
  double x, y;
  double vx, vy;
  double dx, dy;
  double mass;
  int size;
  int pixel_index;
  int hue;
};

struct state {
  struct ball *balls;
  double *x_vels;
  double *y_vels;
  double *speeds;
  int npoints;
  int threshold;
  int delay;
  int global_size;
  int segments;
  Bool glow_p;
  Bool orbit_p;
  Bool walls_p;
  Bool maxspeed_p;
  Bool cbounce_p;
  XPoint *point_stack;
  int point_stack_size, point_stack_fp;
  XColor *colors;
  int ncolors;
  int fg_index;
  int color_shift;
  int xlim, ylim;
  Bool no_erase_yet; /* for tail mode fix */
  double viscosity;

  int mouse_ball;	/* index of ball being dragged, or 0 if none. */
  unsigned long mouse_pixel;

  enum object_mode mode;
  enum graph_mode graph_mode;

  GC draw_gc, erase_gc;

  int total_ticks;
  int color_tick;
  spline *spl;
};


static void *
attraction_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  int i;
  XWindowAttributes xgwa;
  XGCValues gcv;
  int midx, midy, r, vx, vy;
  double th;
  Colormap cmap;
  char *mode_str, *graph_mode_str;
  double size_scale;

  XGetWindowAttributes (dpy, window, &xgwa);
  st->xlim = xgwa.width;
  st->ylim = xgwa.height;
  cmap = xgwa.colormap;
  midx = st->xlim/2;
  midy = st->ylim/2;
  st->walls_p = get_boolean_resource (dpy, "walls", "Boolean");

  /* if there aren't walls, don't set a limit on the radius */
  r = get_integer_resource (dpy, "radius", "Integer");
  if (r <= 0 || (r > min (st->xlim/2, st->ylim/2) && st->walls_p) )
    r = min (st->xlim/2, st->ylim/2) - 50;

  vx = get_integer_resource (dpy, "vx", "Integer");
  vy = get_integer_resource (dpy, "vy", "Integer");

  st->npoints = get_integer_resource (dpy, "points", "Integer");
  if (st->npoints < 1)
    st->npoints = 3 + (random () % 5);
  st->balls = (struct ball *) malloc (st->npoints * sizeof (struct ball));

  st->no_erase_yet = 1; /* for tail mode fix */

  st->segments = get_integer_resource (dpy, "segments", "Integer");
  if (st->segments < 0) st->segments = 1;

  st->threshold = get_integer_resource (dpy, "threshold", "Integer");
  if (st->threshold < 0) st->threshold = 0;

  st->delay = get_integer_resource (dpy, "delay", "Integer");
  if (st->delay < 0) st->delay = 0;

  st->global_size = get_integer_resource (dpy, "size", "Integer");
  if (st->global_size < 0) st->global_size = 0;

  st->glow_p = get_boolean_resource (dpy, "glow", "Boolean");

  st->orbit_p = get_boolean_resource (dpy, "orbit", "Boolean");

  st->maxspeed_p = get_boolean_resource (dpy, "maxspeed", "Boolean");

  st->cbounce_p = get_boolean_resource (dpy, "cbounce", "Boolean");

  st->color_shift = get_integer_resource (dpy, "colorShift", "Integer");
  if (st->color_shift <= 0) st->color_shift = 5;

  st->viscosity = get_float_resource (dpy, "viscosity", "Float");

  mode_str = get_string_resource (dpy, "mode", "Mode");
  if (! mode_str) st->mode = ball_mode;
  else if (!strcmp (mode_str, "balls")) 	st->mode = ball_mode;
  else if (!strcmp (mode_str, "lines")) 	st->mode = line_mode;
  else if (!strcmp (mode_str, "polygons")) 	st->mode = polygon_mode;
  else if (!strcmp (mode_str, "tails")) 	st->mode = tail_mode;
  else if (!strcmp (mode_str, "splines")) 	st->mode = spline_mode;
  else if (!strcmp (mode_str, "filled-splines"))st->mode = spline_filled_mode;
  else {
    fprintf (stderr,
	     "%s: mode must be balls, lines, tails, polygons, splines, or\n\
	filled-splines, not \"%s\"\n",
	     progname, mode_str);
    exit (1);
  }

  graph_mode_str = get_string_resource (dpy, "graphmode", "Mode");
  if (! graph_mode_str) st->graph_mode = graph_none;
  else if (!strcmp (graph_mode_str, "x")) 	st->graph_mode = graph_x;
  else if (!strcmp (graph_mode_str, "y")) 	st->graph_mode = graph_y;
  else if (!strcmp (graph_mode_str, "both")) 	st->graph_mode = graph_both;
  else if (!strcmp (graph_mode_str, "speed")) 	st->graph_mode = graph_speed;
  else if (!strcmp (graph_mode_str, "none")) 	st->graph_mode = graph_none;
  else {
    fprintf (stderr,
	 "%s: graphmode must be speed, x, y, both, or none, not \"%s\"\n",
	 progname, graph_mode_str);
    exit (1);
  }

  /* only allocate memory if it is needed */
  if(st->graph_mode != graph_none)
  {
    if(st->graph_mode == graph_x || st->graph_mode == graph_both)
      st->x_vels = (double *) malloc (st->npoints * sizeof (double));
    if(st->graph_mode == graph_y || st->graph_mode == graph_both)
      st->y_vels = (double *) malloc (st->npoints * sizeof (double));
    if(st->graph_mode == graph_speed)
      st->speeds = (double *) malloc (st->npoints * sizeof (double));
  }

  if (st->mode != ball_mode && st->mode != tail_mode) st->glow_p = False;
  
  if (st->mode == polygon_mode && st->npoints < 3)
    st->mode = line_mode;

  st->ncolors = get_integer_resource (dpy, "colors", "Colors");
  if (st->ncolors < 2) st->ncolors = 2;
  if (st->ncolors <= 2) mono_p = True;
  st->colors = 0;

  if (!mono_p)
    {
      st->fg_index = 0;
      switch (st->mode)
	{
	case ball_mode:
	  if (st->glow_p)
	    {
	      int H = random() % 360;
	      double S1 = 0.25;
	      double S2 = 1.00;
	      double V = frand(0.25) + 0.75;
	      st->colors = (XColor *) malloc(sizeof(*st->colors) * (st->ncolors+1));
	      make_color_ramp (xgwa.screen, xgwa.visual, cmap,
                               H, S1, V, H, S2, V, st->colors, &st->ncolors,
			       False, True, False);
	    }
	  else
	    {
	      st->ncolors = st->npoints;
	      st->colors = (XColor *) malloc(sizeof(*st->colors) * (st->ncolors+1));
	      make_random_colormap (xgwa.screen, xgwa.visual, cmap, 
                                    st->colors, &st->ncolors,
				    True, True, False, True);
	    }
	  break;
	case line_mode:
	case polygon_mode:
	case spline_mode:
	case spline_filled_mode:
	case tail_mode:
	  st->colors = (XColor *) malloc(sizeof(*st->colors) * (st->ncolors+1));
	  make_smooth_colormap (xgwa.screen, xgwa.visual, cmap,
                                st->colors, &st->ncolors,
				True, False, True);
	  break;
	default:
	  abort ();
	}
    }

  if (!mono_p && st->ncolors <= 2)
    {
      if (st->colors) free (st->colors);
      st->colors = 0;
      mono_p = True;
    }

  st->mouse_pixel =
    get_pixel_resource (dpy, cmap, "mouseForeground", "MouseForeground");
  st->mouse_ball = -1;

  if (st->mode != ball_mode)
    {
      int size = (st->segments ? st->segments : 1);
      st->point_stack_size = size * (st->npoints + 1);
      st->point_stack = (XPoint *) calloc (st->point_stack_size, sizeof (XPoint));
      st->point_stack_fp = 0;
    }

  gcv.line_width = (st->mode == tail_mode
		    ? (st->global_size ? st->global_size : (MAX_SIZE * 2 / 3))
		    : 1);
  gcv.cap_style = (st->mode == tail_mode ? CapRound : CapButt);

  if (mono_p)
    gcv.foreground = get_pixel_resource(dpy, cmap, "foreground", "Foreground");
  else
    gcv.foreground = st->colors[st->fg_index].pixel;
  st->draw_gc = XCreateGC (dpy, window, GCForeground|GCLineWidth|GCCapStyle, &gcv);

  gcv.foreground = get_pixel_resource(dpy, cmap, "background", "Background");
  st->erase_gc = XCreateGC (dpy, window, GCForeground|GCLineWidth|GCCapStyle,&gcv);


#ifdef HAVE_JWXYZ
  jwxyz_XSetAntiAliasing (dpy, st->draw_gc,  False);
  jwxyz_XSetAntiAliasing (dpy, st->erase_gc, False);
#endif

  size_scale = 3;
  if (xgwa.width < 100 || xgwa.height < 100)  /* tiny windows */
    size_scale = 0.75;

  /* let's make the balls bigger by default */
#define rand_size() (size_scale * (8 + (random () % 7)))

  if (st->orbit_p && !st->global_size)
    /* To orbit, all objects must be the same mass, or the math gets
       really hairy... */
    st->global_size = rand_size ();

 RETRY_NO_ORBIT:
  th = frand (M_PI+M_PI);
  for (i = 0; i < st->npoints; i++)
    {
      int new_size = (st->global_size ? st->global_size : rand_size ());
      st->balls [i].dx = 0;
      st->balls [i].dy = 0;
      st->balls [i].size = new_size;
      st->balls [i].mass = (new_size * new_size * 10);
      st->balls [i].x = midx + r * cos (i * ((M_PI+M_PI) / st->npoints) + th);
      st->balls [i].y = midy + r * sin (i * ((M_PI+M_PI) / st->npoints) + th);
      if (! st->orbit_p)
	{
	  st->balls [i].vx = vx ? vx : ((6.0 - (random () % 11)) / 8.0);
	  st->balls [i].vy = vy ? vy : ((6.0 - (random () % 11)) / 8.0);
	}
      if (mono_p || st->mode != ball_mode)
	st->balls [i].pixel_index = -1;
      else if (st->glow_p)
	st->balls [i].pixel_index = 0;
      else
	st->balls [i].pixel_index = random() % st->ncolors;
    }

  /*  This lets modes where the points don't really have any size use the whole
      window.  Otherwise, since the points still have a positive size
      assigned to them, they will be bounced somewhat early.  Mass and size are
      seperate, so this shouldn't cause problems.  It's a bit kludgy, tho.
  */
  if(st->mode == line_mode || st->mode == spline_mode || 
     st->mode == spline_filled_mode || st->mode == polygon_mode)
    {
	for(i = 1; i < st->npoints; i++)
	  {
		st->balls[i].size = 0;
          }
     }
    
  if (st->orbit_p)
    {
      double a = 0;
      double v;
      double v_mult = get_float_resource (dpy, "vMult", "Float");
      if (v_mult == 0.0) v_mult = 1.0;

      for (i = 1; i < st->npoints; i++)
	{
          double _2ipi_n = (2 * i * M_PI / st->npoints);
	  double x = r * cos (_2ipi_n);
	  double y = r * sin (_2ipi_n);
	  double distx = r - x;
	  double dist2 = (distx * distx) + (y * y);
          double dist = sqrt (dist2);
          double a1 = ((st->balls[i].mass / dist2) *
                       ((dist < st->threshold) ? -1.0 : 1.0) *
                       (distx / dist));
	  a += a1;
	}
      if (a < 0.0)
	{
          /* "domain error: forces on balls too great" */
	  fprintf (stderr, "%s: window too small for these orbit settings.\n",
		   progname);
          st->orbit_p = False;
          goto RETRY_NO_ORBIT;
	}
      v = sqrt (a * r) * v_mult;
      for (i = 0; i < st->npoints; i++)
	{
	  double k = ((2 * i * M_PI / st->npoints) + th);
	  st->balls [i].vx = -v * sin (k);
	  st->balls [i].vy =  v * cos (k);
	}
    }

  if (mono_p) st->glow_p = False;

  XClearWindow (dpy, window);
  return st;
}

static void
compute_force (struct state *st, int i, double *dx_ret, double *dy_ret)
{
  int j;
  double x_dist, y_dist, dist, dist2;
  *dx_ret = 0;
  *dy_ret = 0;
  for (j = 0; j < st->npoints; j++)
    {
      if (i == j) continue;
      x_dist = st->balls [j].x - st->balls [i].x;
      y_dist = st->balls [j].y - st->balls [i].y;
      dist2 = (x_dist * x_dist) + (y_dist * y_dist);
      dist = sqrt (dist2);
	      
      if (dist > 0.1) /* the balls are not overlapping */
	{
	  double new_acc = ((st->balls[j].mass / dist2) *
			    ((dist < st->threshold) ? -1.0 : 1.0));
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
draw_meter_x(Display *dpy, Window window, struct state *st, int i, int alone) 
{
  XWindowAttributes xgwa;
  int x1,x2,y,w1,w2,h;
  XGetWindowAttributes (dpy, window, &xgwa);

  /* set the width of the bars to use */
  if(xgwa.height < BAR_SIZE*st->npoints)
    {
      y = i*(xgwa.height/st->npoints);
      h = (xgwa.height/st->npoints) - 2;
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

  w1 = (int)(20*st->x_vels[i]);
  w2 = (int)(20*st->balls[i].vx);
  st->x_vels[i] = st->balls[i].vx; 

  if (w1<0) {
    w1=-w1;
    x1=x1-w1;
  }
  if (w2<0) {
    w2=-w2;
    x2=x2-w2;
  }
  XDrawRectangle(dpy,window,st->erase_gc,x1+(h+2)/2,y,w1,h);
  XDrawRectangle(dpy,window,st->draw_gc,x2+(h+2)/2,y,w2,h);
}

/* Draws meters along the diagonal for the y velocity.
   Is there some way to make draw_meter_x and draw_meter_y 
   one function instead of two without making them completely unreadable?
*/
static void 
draw_meter_y (Display *dpy, Window window, struct state *st, int i, int alone) 
{
  XWindowAttributes xgwa;
  int y1,y2,x,h1,h2,w;
  XGetWindowAttributes (dpy, window, &xgwa);

  if(xgwa.height < BAR_SIZE*st->npoints){  /*needs to be height still */
    x = i*(xgwa.height/st->npoints);
    w = (xgwa.height/st->npoints) - 2;
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

  h1 = (int)(20*st->y_vels[i]);
  h2 = (int)(20*st->balls[i].vy);
  st->y_vels[i] = st->balls[i].vy; 

  if (h1<0) {
    h1=-h1;
    y1=y1-h1;
  }
  if (h2<0) {
    h2=-h2;
    y2=y2-h2;
  }
  XDrawRectangle(dpy,window,st->erase_gc,x,y1+(w+2)/2,w,h1);
  XDrawRectangle(dpy,window,st->draw_gc,x,y2+(w+2)/2,w,h2);
}


/* Draws meters of the total speed of the balls */
static void
draw_meter_speed (Display *dpy, Window window, struct state *st, int i) 
{
  XWindowAttributes xgwa;
  int y,x1,x2,h,w1,w2;
  XGetWindowAttributes (dpy, window, &xgwa);

  if(xgwa.height < BAR_SIZE*st->npoints)
    {
      y = i*(xgwa.height/st->npoints);
      h = (xgwa.height/st->npoints) - 2;
    }
  else{
    y = BAR_SIZE*i;
    h = BAR_SIZE - 2;
  }

  x1 = 0;
  x2 = x1;

  if(y < 1) y = i;  
  if(h < 1) h = 1;

  w1 = (int)(5*st->speeds[i]);
  w2 = (int)(5*(st->balls[i].vy*st->balls[i].vy+st->balls[i].vx*st->balls[i].vx));
  st->speeds[i] =    st->balls[i].vy*st->balls[i].vy+st->balls[i].vx*st->balls[i].vx;

  XDrawRectangle(dpy,window,st->erase_gc,x1,y,w1,h);
  XDrawRectangle(dpy,window,st->draw_gc, x2,y,w2,h);
}

/* Returns the position of the mouse relative to the root window.
 */
static void
query_mouse (Display *dpy, Window win, int *x, int *y)
{
  Window root1, child1;
  int mouse_x, mouse_y, root_x, root_y;
  unsigned int mask;
  if (XQueryPointer (dpy, win, &root1, &child1,
                     &root_x, &root_y, &mouse_x, &mouse_y, &mask))
    {
      *x = mouse_x;
      *y = mouse_y;
    }
  else
    {
      *x = -9999;
      *y = -9999;
    }
}

static unsigned long
attraction_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int last_point_stack_fp = st->point_stack_fp;
  
  int i, radius = st->global_size/2;

  st->total_ticks++;

  if(st->global_size == 0)
    radius = (MAX_SIZE / 3);

  if(st->graph_mode != graph_none)
    {
      if(st->graph_mode == graph_both)
	{
          for(i = 0; i < st->npoints; i++)
            {
              draw_meter_x(dpy, window, st, i, 0);
              draw_meter_y(dpy, window, st, i, 0);
            }
	}
      else if(st->graph_mode == graph_x)
	{
          for(i = 0; i < st->npoints; i++)
            {
              draw_meter_x(dpy, window, st, i, 1);
            }
	}
      else if(st->graph_mode == graph_y)
	{
          for(i = 0; i < st->npoints; i++)
            {
              draw_meter_y(dpy, window, st, i, 1);
            }
	}
      else if(st->graph_mode == graph_speed)
	{
          for(i = 0; i < st->npoints; i++)
            {
              draw_meter_speed(dpy, window, st, i);
            }
	}

    }

  /* compute the force of attraction/repulsion among all balls */
  for (i = 0; i < st->npoints; i++)
    compute_force (st, i, &st->balls[i].dx, &st->balls[i].dy);

  /* move the balls according to the forces now in effect */
  for (i = 0; i < st->npoints; i++)
    {
      double old_x = st->balls[i].x;
      double old_y = st->balls[i].y;
      double new_x, new_y;
      int size = st->balls[i].size;

      st->balls[i].vx += st->balls[i].dx;
      st->balls[i].vy += st->balls[i].dy;

      /* "don't let them get too fast: impose a terminal velocity
         (actually, make the medium have friction)"
	 Well, what this first step really does is give the medium a 
	 viscosity of .9 for balls going over the speed limit.  Anyway, 
	 this is now optional
      */
      if (fabs(st->balls[i].vx) > 10 && st->maxspeed_p)
	{
	  st->balls[i].vx *= 0.9;
	  st->balls[i].dx = 0;
	}
      if (st->viscosity != 1)
	{
	  st->balls[i].vx *= st->viscosity;
	}

      if (fabs(st->balls[i].vy) > 10 && st->maxspeed_p)
	{
	  st->balls[i].vy *= 0.9;
	  st->balls[i].dy = 0;
	}
      if (st->viscosity != 1)
	{
	  st->balls[i].vy *= st->viscosity;
	}

      st->balls[i].x += st->balls[i].vx;
      st->balls[i].y += st->balls[i].vy;


      /* bounce off the walls if desired
	 note: a ball is actually its upper left corner */
      if(st->walls_p)
 	{
	  if(st->cbounce_p)  /* with correct bouncing */
	    {
              /* so long as it's out of range, keep bouncing */
	      /* limit the maximum number to bounce to 4.*/
	      int bounce_allowed = 4;
	
              while( bounce_allowed && (
		     (st->balls[i].x >= (st->xlim - st->balls[i].size)) ||
                     (st->balls[i].y >= (st->ylim - st->balls[i].size)) ||
                     (st->balls[i].x <= 0) ||
                     (st->balls[i].y <= 0) )
		     )
                {
                  bounce_allowed--;
                  if (st->balls[i].x >= (st->xlim - st->balls[i].size))
                    {
                      st->balls[i].x = (2*(st->xlim - st->balls[i].size) - st->balls[i].x);
                      st->balls[i].vx = -st->balls[i].vx;
                    }
                  if (st->balls[i].y >= (st->ylim - st->balls[i].size))
                    {
                      st->balls[i].y = (2*(st->ylim - st->balls[i].size) - st->balls[i].y);
                      st->balls[i].vy = -st->balls[i].vy;
                    }
                  if (st->balls[i].x <= 0)
                    {
                      st->balls[i].x = -st->balls[i].x;
		      st->balls[i].vx = -st->balls[i].vx;
                    }
                  if (st->balls[i].y <= 0)
                    {
                      st->balls[i].y = -st->balls[i].y;
		      st->balls[i].vy = -st->balls[i].vy;
                    }
                }
            }
          else  /* with old bouncing */
            {
              if (st->balls[i].x >= (st->xlim - st->balls[i].size))
                {
                  st->balls[i].x = (st->xlim - st->balls[i].size - 1);
                  if (st->balls[i].vx > 0) /* why is this check here? */
                    st->balls[i].vx = -st->balls[i].vx;
                }
              if (st->balls[i].y >= (st->ylim - st->balls[i].size))
                {
                  st->balls[i].y = (st->ylim - st->balls[i].size - 1);
                  if (st->balls[i].vy > 0)
                    st->balls[i].vy = -st->balls[i].vy;
                }
              if (st->balls[i].x <= 0)
                {
                  st->balls[i].x = 0;
                  if (st->balls[i].vx < 0)
                    st->balls[i].vx = -st->balls[i].vx;
                }
              if (st->balls[i].y <= 0)
                {
                  st->balls[i].y = 0;
                  if (st->balls[i].vy < 0)
                    st->balls[i].vy = -st->balls[i].vy;
                }
            }
        }

      if (i == st->mouse_ball)
        {
          int x, y;
          query_mouse (dpy, window, &x, &y);
	  if (st->mode == ball_mode)
            {
              x -= st->balls[i].size / 2;
              y -= st->balls[i].size / 2;
            }

          st->balls[i].x = x;
          st->balls[i].y = y;
        }

      new_x = st->balls[i].x;
      new_y = st->balls[i].y;

      if (!mono_p)
	{
	  if (st->mode == ball_mode)
	    {
	      if (st->glow_p)
		{
		  /* make color saturation be related to particle
		     acceleration. */
		  double limit = 0.5;
		  double s, fraction;
		  double vx = st->balls [i].dx;
		  double vy = st->balls [i].dy;
		  if (vx < 0) vx = -vx;
		  if (vy < 0) vy = -vy;
		  fraction = vx + vy;
		  if (fraction > limit) fraction = limit;

		  s = 1 - (fraction / limit);
		  st->balls[i].pixel_index = (st->ncolors * s);
		}
	      XSetForeground (dpy, st->draw_gc,
                              (i == st->mouse_ball
                               ? st->mouse_pixel
                               : st->colors[st->balls[i].pixel_index].pixel));
	    }
	}

      if (st->mode == ball_mode)
	{
	  XFillArc (dpy, window, st->erase_gc, (int) old_x, (int) old_y,
		    size, size, 0, 360*64);
	  XFillArc (dpy, window, st->draw_gc,  (int) new_x, (int) new_y,
		    size, size, 0, 360*64);
	}
      else
	{
	  st->point_stack [st->point_stack_fp].x = new_x;
	  st->point_stack [st->point_stack_fp].y = new_y;
	  st->point_stack_fp++;
	}
    }

  /* draw the lines or polygons after computing all points */
  if (st->mode != ball_mode)
    {
      st->point_stack [st->point_stack_fp].x = st->balls [0].x; /* close the polygon */
      st->point_stack [st->point_stack_fp].y = st->balls [0].y;
      st->point_stack_fp++;
      if (st->point_stack_fp == st->point_stack_size)
	st->point_stack_fp = 0;
      else if (st->point_stack_fp > st->point_stack_size) /* better be aligned */
	abort ();
      if (!mono_p)
	{
	  if (st->color_tick++ == st->color_shift)
	    {
	      st->color_tick = 0;
	      st->fg_index = (st->fg_index + 1) % st->ncolors;
	      XSetForeground (dpy, st->draw_gc, st->colors[st->fg_index].pixel);
	    }
	}
    }

  switch (st->mode)
    {
    case ball_mode:
      break;
    case line_mode:
      if (st->segments > 0)
	XDrawLines (dpy, window, st->erase_gc, st->point_stack + st->point_stack_fp,
		    st->npoints + 1, CoordModeOrigin);
      XDrawLines (dpy, window, st->draw_gc, st->point_stack + last_point_stack_fp,
		  st->npoints + 1, CoordModeOrigin);
      break;
    case polygon_mode:
      if (st->segments > 0)
	XFillPolygon (dpy, window, st->erase_gc, st->point_stack + st->point_stack_fp,
		      st->npoints + 1, (st->npoints == 3 ? Convex : Complex),
		      CoordModeOrigin);
      XFillPolygon (dpy, window, st->draw_gc, st->point_stack + last_point_stack_fp,
		    st->npoints + 1, (st->npoints == 3 ? Convex : Complex),
		    CoordModeOrigin);
      break;
    case tail_mode:
      {
	for (i = 0; i < st->npoints; i++)
	  {
	    int index = st->point_stack_fp + i;
	    int next_index = (index + (st->npoints + 1)) % st->point_stack_size;
            if(st->no_erase_yet == 1)
	      {
		if(st->total_ticks >= st->segments)
                  {
                    st->no_erase_yet = 0;
                    XDrawLine (dpy, window, st->erase_gc,
			       st->point_stack [index].x + radius,
			       st->point_stack [index].y + radius,
			       st->point_stack [next_index].x + radius,
			       st->point_stack [next_index].y + radius);
                  }
	      }
	    else
	      {
	    	XDrawLine (dpy, window, st->erase_gc,
                           st->point_stack [index].x + radius,
                           st->point_stack [index].y + radius,
                           st->point_stack [next_index].x + radius,
                           st->point_stack [next_index].y + radius);
	      }
	    index = last_point_stack_fp + i;
	    next_index = (index - (st->npoints + 1)) % st->point_stack_size;
	    if (next_index < 0) next_index += st->point_stack_size;
	    if (st->point_stack [next_index].x == 0 &&
		st->point_stack [next_index].y == 0)
	      continue;
	    XDrawLine (dpy, window, st->draw_gc,
		       st->point_stack [index].x + radius,
		       st->point_stack [index].y + radius,
		       st->point_stack [next_index].x + radius,
		       st->point_stack [next_index].y + radius);
	  }
      }
      break;
    case spline_mode:
    case spline_filled_mode:
      {
	if (! st->spl) st->spl = make_spline (st->npoints);
	if (st->segments > 0)
	  {
	    for (i = 0; i < st->npoints; i++)
	      {
		st->spl->control_x [i] = st->point_stack [st->point_stack_fp + i].x;
		st->spl->control_y [i] = st->point_stack [st->point_stack_fp + i].y;
	      }
	    compute_closed_spline (st->spl);
	    if (st->mode == spline_filled_mode)
	      XFillPolygon (dpy, window, st->erase_gc, st->spl->points, st->spl->n_points,
			    (st->spl->n_points == 3 ? Convex : Complex),
			    CoordModeOrigin);
	    else
	      XDrawLines (dpy, window, st->erase_gc, st->spl->points, st->spl->n_points,
			  CoordModeOrigin);
	  }
	for (i = 0; i < st->npoints; i++)
	  {
	    st->spl->control_x [i] = st->point_stack [last_point_stack_fp + i].x;
	    st->spl->control_y [i] = st->point_stack [last_point_stack_fp + i].y;
	  }
	compute_closed_spline (st->spl);
	if (st->mode == spline_filled_mode)
	  XFillPolygon (dpy, window, st->draw_gc, st->spl->points, st->spl->n_points,
			(st->spl->n_points == 3 ? Convex : Complex),
			CoordModeOrigin);
	else
	  XDrawLines (dpy, window, st->draw_gc, st->spl->points, st->spl->n_points,
		      CoordModeOrigin);
      }
      break;
    default:
      abort ();
    }

  return st->delay;
}

static void
attraction_reshape (Display *dpy, Window window, void *closure, 
                    unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  st->xlim = w;
  st->ylim = h;
}

static Bool
attraction_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;

  if (event->xany.type == ButtonPress)
    {
      int i;
      if (st->mouse_ball != -1)  /* second down-click?  drop the ball. */
        {
          st->mouse_ball = -1;
          return True;
        }
      else
        {
          /* When trying to pick up a ball, first look for a click directly
             inside the ball; but if we don't find it, expand the radius
             outward until we find something nearby.
           */
          int x = event->xbutton.x;
          int y = event->xbutton.y;
          float max = 10 * (st->global_size ? st->global_size : MAX_SIZE);
          float step = max / 100;
          float r2;
          for (r2 = step; r2 < max; r2 += step)
            {
              for (i = 0; i < st->npoints; i++)
                {
                  float d = ((st->balls[i].x - x) * (st->balls[i].x - x) +
                             (st->balls[i].y - y) * (st->balls[i].y - y));
                  float r = st->balls[i].size;
                  if (r2 > r) r = r2;
                  if (d < r*r)
                    {
                      st->mouse_ball = i;
                      return True;
                    }
                }
            }
        }
      return True;
    }
  else if (event->xany.type == ButtonRelease)   /* drop the ball */
    {
      st->mouse_ball = -1;
      return True;
    }



  return False;
}

static void
attraction_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;

  if (st->balls)	free (st->balls);
  if (st->x_vels)	free (st->x_vels);
  if (st->y_vels)	free (st->y_vels);
  if (st->speeds)	free (st->speeds);
  if (st->point_stack)	free (st->point_stack);
  if (st->colors)	free (st->colors);
  if (st->spl)		free_spline (st->spl);

  free (st);
}


static const char *attraction_defaults [] = {
  ".background:	black",
  ".foreground:	white",
  "*fpsSolid:	true",
  "*mode:	balls",
  "*graphmode:  none",
  "*points:	0",
  "*size:	0",
  "*colors:	200",
  "*threshold:	200",
  "*delay:	10000",
  "*glow:	false",
  "*walls:	true",
  "*maxspeed:	true",
  "*cbounce:	true",
  "*viscosity:	1.0",
  "*orbit:	false",
  "*colorShift:	3",
  "*segments:	500",
  "*vMult:	0.9",
  "*radius:	0",
  "*vx:		0",
  "*vy:		0",
  "*mouseForeground: white",
#ifdef HAVE_MOBILE
  "*ignoreRotation: True",
#endif
  0
};

static XrmOptionDescRec attraction_options [] = {
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
  { "-viscosity",	".viscosity",	XrmoptionSepArg, 0 },
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


XSCREENSAVER_MODULE ("Attraction", attraction)
