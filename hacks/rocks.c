/* xscreensaver, Copyright (c) 1992, 1995, 1996, 1997
 *  Jamie Zawinski <jwz@netscape.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* 18-Sep-97: Johannes Keukelaar <johannes@nada.kth.se>: Added some color.
 * Using -mono gives the old behaviour.  (Modified by jwz.)
 */
/*   Flying through an asteroid field.  Based on TI Explorer Lisp code by 
   John Nguyen <johnn@hx.lcs.mit.edu>
 */

#include <stdio.h>
#include <math.h>
#include "screenhack.h"

#define MIN_ROCKS 1
#define MIN_DEPTH 2		/* rocks disappear when they get this close */
#define MAX_DEPTH 60		/* this is where rocks appear */
#define MIN_SIZE 3		/* how small where pixmaps are not used */
#define MAX_SIZE 200		/* how big (in pixels) rocks are at depth 1 */
#define DEPTH_SCALE 100		/* how many ticks there are between depths */
#define SIN_RESOLUTION 1000

#define MAX_DEP 0.3		/* how far the displacement can be (percent) */
#define DIRECTION_CHANGE_RATE 60
#define MAX_DEP_SPEED 5		/* Maximum speed for movement */
#define MOVE_STYLE 0		/* Only 0 and 1. Distinguishes the fact that
				   these are the rocks that are moving (1) 
				   or the rocks source (0). */

/* there's not much point in the above being user-customizable, but those
   numbers might want to be tweaked for displays with an order of magnitude
   higher resolution or compute power.
 */

static double sins [SIN_RESOLUTION];
static double coss [SIN_RESOLUTION];
static double depths [(MAX_DEPTH + 1) * DEPTH_SCALE];

static Display *dpy;
static Window window;
static int width, height, midx, midy;
static int dep_x, dep_y;
static int ncolors;
static XColor *colors;
static float max_dep;
static GC erase_gc;
static GC *draw_gcs;
static Bool rotate_p;
static Bool move_p;
static int speed;
static Bool threed;
static GC threed_left_gc, threed_right_gc;
static double threed_delta;

#define GETZDIFF(z) \
        (threed_delta * 40.0 * \
	 (1.0 - ((MAX_DEPTH * DEPTH_SCALE / 2) / \
		 ((z) + 20.0 * DEPTH_SCALE))))

struct rock {
  int real_size;
  int r;
  int theta;
  int depth;
  int size, x, y;
  int diff;
  int color;
};

static struct rock *rocks;
static int nrocks;
static Pixmap pixmaps [MAX_SIZE];
static int delay;

static void rock_compute (struct rock *);
static void rock_draw (struct rock *, Bool draw_p);

static void
rock_reset (struct rock *rock)
{
  rock->real_size = MAX_SIZE;
  rock->r = (SIN_RESOLUTION * 0.7) + (random () % (30 * SIN_RESOLUTION));
  rock->theta = random () % SIN_RESOLUTION;
  rock->depth = MAX_DEPTH * DEPTH_SCALE;
  rock->color = random() % ncolors;
  rock_compute (rock);
  rock_draw (rock, True);
}

static void
rock_tick (struct rock *rock, int d)
{
  if (rock->depth > 0)
    {
      rock_draw (rock, False);
      rock->depth -= speed;
      if (rotate_p)
        {
	  rock->theta = (rock->theta + d) % SIN_RESOLUTION;
        }
      while (rock->theta < 0)
	rock->theta += SIN_RESOLUTION;
      if (rock->depth < (MIN_DEPTH * DEPTH_SCALE))
	rock->depth = 0;
      else
	{
	  rock_compute (rock);
	  rock_draw (rock, True);
	}
    }
  else if ((random () % 40) == 0)
    rock_reset (rock);
}

static void
rock_compute (struct rock *rock)
{
  double factor = depths [rock->depth];
  double rsize = rock->real_size * factor;

  rock->size = (int) (rsize + 0.5);
  rock->diff = (int) GETZDIFF(rock->depth);
  rock->x = midx + (coss [rock->theta] * rock->r * factor);
  rock->y = midy + (sins [rock->theta] * rock->r * factor);

  if (move_p)
    {
      double move_factor = (((double) MOVE_STYLE) -
			    (((double) rock->depth) /
			     (((double) (MAX_DEPTH + 1)) *
			      ((double) DEPTH_SCALE))));
      /* move_factor is 0 when the rock is close, 1 when far */
      rock->x += (((double) dep_x) * move_factor);
      rock->y += (((double) dep_y) * move_factor);
    }
}

static void
rock_draw (rock, draw_p)
     struct rock *rock;
     Bool draw_p;
{
  GC gc = (draw_p 
	   ? (threed ? erase_gc : draw_gcs[rock->color])
	   : erase_gc);

  if (rock->x <= 0 || rock->y <= 0 || rock->x >= width || rock->y >= height)
    {
      /* this means that if a rock were to go off the screen at 12:00, but
	 would have been visible at 3:00, it won't come back once the observer
	 rotates around so that the rock would have been visible again.
	 Oh well.
       */
      if (!move_p)
	rock->depth = 0;
      return;
    }
  if (rock->size <= 1)
    {
      if (threed)
	{
	  if (draw_p) gc = threed_left_gc;
	  XDrawPoint (dpy, window, gc, rock->x - rock->diff, rock->y);
	  if (draw_p) gc = threed_right_gc;
	  XDrawPoint (dpy, window, gc, rock->x + rock->diff, rock->y);
	}
      else
	{
	  XDrawPoint (dpy, window, gc, rock->x, rock->y);
	}
    }
  else if (rock->size <= MIN_SIZE || !draw_p)
    {
      if (threed)
	{
	  if (draw_p) gc = threed_left_gc;
	  XFillRectangle(dpy, window, gc,
			 rock->x - rock->size / 2 - rock->diff,
			 rock->y - rock->size / 2,
			 rock->size, rock->size);
	  if (draw_p) gc = threed_right_gc;
	  XFillRectangle(dpy, window, gc,
			 rock->x - rock->size / 2 + rock->diff,
			 rock->y - rock->size / 2,
			 rock->size, rock->size);
	}
      else
	{
	  XFillRectangle (dpy, window, gc,
			  rock->x - rock->size/2, rock->y - rock->size/2,
			  rock->size, rock->size);
	}
    }
  else if (rock->size < MAX_SIZE)
    {
      if (threed)
	{
	  gc = threed_left_gc;
	  XCopyPlane(dpy, pixmaps[rock->size], window, gc,
		     0, 0, rock->size, rock->size,
		     rock->x - rock->size / 2 - rock->diff,
		     rock->y - rock->size / 2, 1L);
	  gc = threed_right_gc;
	  XCopyPlane(dpy, pixmaps[rock->size], window, gc,
		     0, 0, rock->size, rock->size,
		     rock->x - rock->size / 2 + rock->diff,
		     rock->y - rock->size / 2, 1L);
	}
      else
	{
	  XCopyPlane (dpy, pixmaps [rock->size], window, gc,
		      0, 0, rock->size, rock->size,
		      rock->x - rock->size/2, rock->y - rock->size/2,
		      1L);
	}
    }
}


static void
init_pixmaps (Display *dpy, Window window)
{
  int i;
  XGCValues gcv;
  GC fg_gc = 0, bg_gc = 0;
  pixmaps [0] = pixmaps [1] = 0;
  for (i = MIN_DEPTH; i < MAX_SIZE; i++)
    {
      int w = (1+(i/32))<<5; /* server might be faster if word-aligned */
      int h = i;
      Pixmap p = XCreatePixmap (dpy, window, w, h, 1);
      XPoint points [7];
      pixmaps [i] = p;
      if (! p)
	{
	  fprintf (stderr, "%s: couldn't allocate pixmaps", progname);
	  exit (1);
	}
      if (! fg_gc)
	{	/* must use drawable of pixmap, not window (fmh) */
	  gcv.foreground = 1;
	  fg_gc = XCreateGC (dpy, p, GCForeground, &gcv);
	  gcv.foreground = 0;
	  bg_gc = XCreateGC (dpy, p, GCForeground, &gcv);
	}
      XFillRectangle (dpy, p, bg_gc, 0, 0, w, h);
      points [0].x = i * 0.15; points [0].y = i * 0.85;
      points [1].x = i * 0.00; points [1].y = i * 0.20;
      points [2].x = i * 0.30; points [2].y = i * 0.00;
      points [3].x = i * 0.40; points [3].y = i * 0.10;
      points [4].x = i * 0.90; points [4].y = i * 0.10;
      points [5].x = i * 1.00; points [5].y = i * 0.55;
      points [6].x = i * 0.45; points [6].y = i * 1.00;
      XFillPolygon (dpy, p, fg_gc, points, 7, Nonconvex, CoordModeOrigin);
    }
  XFreeGC (dpy, fg_gc);
  XFreeGC (dpy, bg_gc);
}


static int
compute_move(int axe)				/* 0 for x, 1 for y */
{
  static int current_dep[2] = {0, 0};
  static int speed[2] = {0, 0};
  static short direction[2] = {0, 0};
  static int limit[2] = {0, 0};
  int change = 0;

  limit[0] = midx;
  limit[1] = midy;

  current_dep[axe] += speed[axe];	/* We adjust the displacement */

  if (current_dep[axe] > (int) (limit[axe] * max_dep))
    {
      if (current_dep[axe] > limit[axe])
	current_dep[axe] = limit[axe];
      direction[axe] = -1;
    }			/* This is when we reach the upper screen limit */
  if (current_dep[axe] < (int) (-limit[axe] * max_dep))
    {
      if (current_dep[axe] < -limit[axe])
	current_dep[axe] = -limit[axe];
      direction[axe] = 1;
    }			/* This is when we reach the lower screen limit */
  if (direction[axe] == 1)	/* We adjust the speed */
    speed[axe] += 1;
  else if (direction[axe] == -1)
    speed[axe] -= 1;

  if (speed[axe] > MAX_DEP_SPEED)
    speed[axe] = MAX_DEP_SPEED;
  else if (speed[axe] < -MAX_DEP_SPEED)
    speed[axe] = -MAX_DEP_SPEED;

  if (move_p && !(random() % DIRECTION_CHANGE_RATE))
    {
      /* We change direction */
      change = random() & 1;
      if (change != 1)
	{
	  if (direction[axe] == 0)
	    direction[axe] = change - 1;	/* 0 becomes either 1 or -1 */
	  else
	    direction[axe] = 0;			/* -1 or 1 become 0 */
	}
    }
  return (current_dep[axe]);
}

static void
tick_rocks (int d)
{
  int i;

  if (move_p)
    {
      dep_x = compute_move(0);
      dep_y = compute_move(1);
    }

  for (i = 0; i < nrocks; i++)
    rock_tick (&rocks [i], d);
}


static void
rocks_once (void)
{
  static int current_delta = 0;	/* observer Z rotation */
  static int window_tick = 50;
	static int  new_delta = 0;
	static int  dchange_tick = 0;

  if (window_tick++ == 50)
    {
      XWindowAttributes xgwa;
      XGetWindowAttributes (dpy, window, &xgwa);
      window_tick = 0;
      width = xgwa.width;
      height = xgwa.height;
      midx = width/2;
      midy = height/2;
    }

  if (current_delta != new_delta)
    {
      if (dchange_tick++ == 5)
	{
	  dchange_tick = 0;
	  if (current_delta < new_delta)
	    current_delta++;
	  else
	    current_delta--;
	}
    }
  else
    {
      if (! (random() % 50))
	{
	  new_delta = ((random() % 11) - 5);
	  if (! (random() % 10))
	    new_delta *= 5;
	}
    }
  tick_rocks (current_delta);
}

static void
init_rocks (Display *d, Window w)
{
  int i;
  XGCValues gcv;
  Colormap cmap;
  XWindowAttributes xgwa;
  unsigned int bg;
  dpy = d;
  window = w;
  XGetWindowAttributes (dpy, window, &xgwa);
  cmap = xgwa.colormap;
  delay = get_integer_resource ("delay", "Integer");
  if (delay < 0) delay = 0;
  speed = get_integer_resource ("speed", "Integer");
  if (speed < 1) speed = 1;
  if (speed > 100) speed = 100;
  rotate_p = get_boolean_resource ("rotate", "Boolean");
  move_p = get_boolean_resource ("move", "Boolean");
  if (mono_p)
    ncolors = 2;
  else
    ncolors = get_integer_resource ("colors", "Colors");

  if (ncolors < 2)
    {
      ncolors = 2;
      mono_p = True;
  }

  colors = (XColor *) malloc(ncolors * sizeof(*colors));
  draw_gcs = (GC *) malloc(ncolors * sizeof(*draw_gcs));

  bg = get_pixel_resource ("background", "Background", dpy, cmap);
  colors[0].pixel = bg;
  colors[0].flags = DoRed|DoGreen|DoBlue;
  XQueryColor(dpy, cmap, &colors[0]);

  ncolors--;
  make_random_colormap(dpy, xgwa.visual, cmap, colors+1, &ncolors, True,
		       True, 0, True);
  ncolors++;

  if (ncolors < 2)
    {
      ncolors = 2;
      mono_p = True;
  }

  if (mono_p)
    {
      unsigned int fg = get_pixel_resource("foreground", "Foreground",
					   dpy, cmap);
      colors[1].pixel = fg;
      colors[1].flags = DoRed|DoGreen|DoBlue;
      XQueryColor(dpy, cmap, &colors[1]);
      gcv.foreground = fg;
      gcv.background = bg;
      draw_gcs[0] = XCreateGC (dpy, window, GCForeground|GCBackground, &gcv);
      draw_gcs[1] = draw_gcs[0];
    }
  else
    for( i = 0; i < ncolors; i++ )
      {
	gcv.foreground = colors[i].pixel;
	gcv.background = bg;
	draw_gcs[i] = XCreateGC (dpy, window, GCForeground|GCBackground, &gcv);
      }

  gcv.foreground = bg;
  erase_gc = XCreateGC (dpy, window, GCForeground|GCBackground, &gcv);

  max_dep = (move_p ? MAX_DEP : 0);

  for (i = 0; i < SIN_RESOLUTION; i++)
    {
      sins [i] = sin ((((double) i) / (SIN_RESOLUTION / 2)) * M_PI);
      coss [i] = cos ((((double) i) / (SIN_RESOLUTION / 2)) * M_PI);
    }
  /* we actually only need i/speed of these, but wtf */
  for (i = 1; i < (sizeof (depths) / sizeof (depths[0])); i++)
    depths [i] = atan (((double) 0.5) / (((double) i) / DEPTH_SCALE));
  depths [0] = M_PI/2; /* avoid division by 0 */

  threed = get_boolean_resource("use3d", "Boolean");
  if (threed)
    {
      gcv.background = bg;
      gcv.foreground = get_pixel_resource ("left3d", "Foreground", dpy, cmap);
      threed_left_gc = XCreateGC (dpy, window, GCForeground|GCBackground,&gcv);
      gcv.foreground = get_pixel_resource ("right3d", "Foreground", dpy, cmap);
      threed_right_gc = XCreateGC (dpy, window,GCForeground|GCBackground,&gcv);
      threed_delta = get_float_resource("delta3d", "Integer");
    }

  /* don't want any exposure events from XCopyPlane */
  for( i = 0; i < ncolors; i++)
    XSetGraphicsExposures (dpy, draw_gcs[i], False);
  XSetGraphicsExposures (dpy, erase_gc, False);

  nrocks = get_integer_resource ("count", "Count");
  if (nrocks < 1) nrocks = 1;
  rocks = (struct rock *) calloc (nrocks, sizeof (struct rock));
  init_pixmaps (dpy, window);
  XClearWindow (dpy, window);
}



char *progclass = "Rocks";

char *defaults [] = {
  ".background:	Black",
  ".foreground:	#E9967A",
  "*colors:	5",
  "*count:	100",
  "*delay:	50000",
  "*speed:	100",
  "*rotate:     true",
  "*move:       true",
  "*use3d:      False",
  "*left3d:	Blue",
  "*right3d:	Red",
  "*delta3d:	1.5",
  0
};

XrmOptionDescRec options [] = {
  { "-count",		".count",	XrmoptionSepArg, 0 },
  { "-rotate",          ".rotate",      XrmoptionNoArg,  "true" },
  { "-norotate",        ".rotate",      XrmoptionNoArg,  "false" },
  { "-move",            ".move",        XrmoptionNoArg,  "true" },
  { "-nomove",          ".move",        XrmoptionNoArg,  "false" },
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-speed",		".speed",	XrmoptionSepArg, 0 },
  {"-3d",		".use3d",	XrmoptionNoArg, "True"},
  {"-no-3d",		".use3d",	XrmoptionNoArg, "False"},
  {"-left3d",		".left3d",	XrmoptionSepArg, 0 },
  {"-right3d",		".right3d",	XrmoptionSepArg, 0 },
  {"-delta3d",		".delta3d",	XrmoptionSepArg, 0 },
  { "-colors",		".colors",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

void
screenhack (Display *dpy, Window window)
{
  init_rocks (dpy, window);
  while (1)
    {
      rocks_once ();
      XSync (dpy, True);
      if (delay) usleep (delay);
    }
}
