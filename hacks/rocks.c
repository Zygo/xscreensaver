/* xscreensaver, Copyright (c) 1992 Jamie Zawinski <jwz@mcom.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* Flying through an asteroid field.  Based on TI Explorer Lisp code by 
   John Nguyen <johnn@hx.lcs.mit.edu>
 */

#include "screenhack.h"
#include <stdio.h>
#include <math.h>
#if __STDC__
#include <math.h>	/* for M_PI */
#endif

#define MIN_DEPTH 2		/* rocks disappar when they get this close */
#define MAX_DEPTH 60		/* this is where rocks appear */
#define MAX_WIDTH 100		/* how big (in pixels) rocks are at depth 1 */
#define DEPTH_SCALE 100		/* how many ticks there are between depths */
#define SIN_RESOLUTION 1000

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
static GC draw_gc, erase_gc;
static Bool rotate_p ;

static int speed;

struct rock {
  int real_size;
  int r;
  int theta;
  int depth;
  int size, x, y;
};

static struct rock *rocks;
static int nrocks;
static Pixmap pixmaps [MAX_WIDTH];
static int delay;

static void rock_reset(), rock_tick(), rock_compute(), rock_draw();
static void init_pixmaps(), init_rocks(), tick_rocks(), rocks_once();


static void
rock_reset (rock)
     struct rock *rock;
{
  rock->real_size = MAX_WIDTH;
  rock->r = (SIN_RESOLUTION * 0.7) + (random () % (30 * SIN_RESOLUTION));
  rock->theta = random () % SIN_RESOLUTION;
  rock->depth = MAX_DEPTH * DEPTH_SCALE;
  rock_compute (rock);
  rock_draw (rock, True);
}

static void
rock_tick (rock, d)
     struct rock *rock;
     int d;
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
rock_compute (rock)
     struct rock *rock;
{
  double factor = depths [rock->depth];
  rock->size = (int) ((rock->real_size * factor) + 0.5);
  rock->x = midx + (coss [rock->theta] * rock->r * factor);
  rock->y = midy + (sins [rock->theta] * rock->r * factor);
}

static void
rock_draw (rock, draw_p)
     struct rock *rock;
     Bool draw_p;
{
  GC gc = draw_p ? draw_gc : erase_gc;
  if (rock->x <= 0 || rock->y <= 0 || rock->x >= width || rock->y >= height)
    {
      /* this means that if a rock were to go off the screen at 12:00, but
	 would have been visible at 3:00, it won't come back once the observer
	 rotates around so that the rock would have been visible again.
	 Oh well.
       */
      rock->depth = 0;
      return;
    }
  if (rock->size <= 1)
    XDrawPoint (dpy, window, gc, rock->x, rock->y);
  else if (rock->size <= 3 || !draw_p)
    XFillRectangle (dpy, window, gc,
		    rock->x - rock->size/2, rock->y - rock->size/2,
		    rock->size, rock->size);
  else if (rock->size < MAX_WIDTH)
    XCopyPlane (dpy, pixmaps [rock->size], window, gc,
		0, 0, rock->size, rock->size,
		rock->x - rock->size/2, rock->y - rock->size/2,
		1);
  else
    abort ();
}


static void
init_pixmaps (dpy, window)
     Display *dpy;
     Window window;
{
  int i;
  XGCValues gcv;
  GC fg_gc = 0, bg_gc;
  pixmaps [0] = pixmaps [1] = 0;
  for (i = MIN_DEPTH; i < MAX_WIDTH; i++)
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


static void
init_rocks (d, w)
     Display *d;
     Window w;
{
  int i;
  XGCValues gcv;
  Colormap cmap;
  XWindowAttributes xgwa;
  unsigned int fg, bg;
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
  fg = get_pixel_resource ("foreground", "Foreground", dpy, cmap);
  bg = get_pixel_resource ("background", "Background", dpy, cmap);
  gcv.foreground = fg;
  gcv.background = bg;
  draw_gc = XCreateGC (dpy, window, GCForeground|GCBackground, &gcv);
  gcv.foreground = bg;
  gcv.background = fg;
  erase_gc = XCreateGC (dpy, window, GCForeground|GCBackground, &gcv);

  for (i = 0; i < SIN_RESOLUTION; i++)
    {
      sins [i] = sin ((((double) i) / (SIN_RESOLUTION / 2)) * M_PI);
      coss [i] = cos ((((double) i) / (SIN_RESOLUTION / 2)) * M_PI);
    }
  /* we actually only need i/speed of these, but wtf */
  for (i = 1; i < (sizeof (depths) / sizeof (depths[0])); i++)
    depths [i] = atan (((double) 0.5) / (((double) i) / DEPTH_SCALE));
  depths [0] = M_PI/2; /* avoid division by 0 */

  nrocks = get_integer_resource ("count", "Count");
  if (nrocks < 1) nrocks = 1;
  rocks = (struct rock *) calloc (nrocks, sizeof (struct rock));
  init_pixmaps (dpy, window);
  XClearWindow (dpy, window);
}


static void
tick_rocks (d)
     int d;
{
  int i;
  for (i = 0; i < nrocks; i++)
    rock_tick (&rocks [i], d);
}

static void
rocks_once ()
{
  static int current_delta = 0;	/* observer Z rotation */
  static int window_tick = 50;

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

  if ((random () % 50) == 0)
    {
      int d = current_delta;
      int new_delta = ((random () % 11) - 5);
      if ((random () % 10) == 0)
	new_delta *= 5;

      while (d == current_delta)
	{
	  int i;
	  for (i = 0; i < 3; i++)
	    tick_rocks (d);
	  if (current_delta < new_delta) d++;
	  else d--;
	}
      current_delta = new_delta;
    }
  tick_rocks (current_delta);
}


char *progclass = "Rocks";

char *defaults [] = {
  "Rocks.background:	black",		/* to placate SGI */
  "Rocks.foreground:	white",
  "*count:	100",
  "*delay:	50000",
  "*speed:	100",
  "*rotate:     true",
  0
};

XrmOptionDescRec options [] = {
  { "-count",		".count",	XrmoptionSepArg, 0 },
  { "-norotate",        ".rotate",      XrmoptionNoArg,  "false" },
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-speed",		".speed",	XrmoptionSepArg, 0 }
};
int options_size = (sizeof (options) / sizeof (options[0]));

void
screenhack (dpy, window)
     Display *dpy;
     Window window;
{
  init_rocks (dpy, window);
  while (1)
    {
      rocks_once ();
      XSync (dpy, True);
      if (delay) usleep (delay);
    }
}
