/* xscreensaver, Copyright (c) 1992-2008 Jamie Zawinski <jwz@jwz.org>
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

struct state {
  Display *dpy;
  Window window;

  double sins [SIN_RESOLUTION];
  double coss [SIN_RESOLUTION];
  double depths [(MAX_DEPTH + 1) * DEPTH_SCALE];

  int width, height, midx, midy;
  int dep_x, dep_y;
  int ncolors;
  XColor *colors;
  float max_dep;
  GC erase_gc;
  GC *draw_gcs;
  Bool rotate_p;
  Bool move_p;
  int speed;
  Bool threed;
  GC threed_left_gc, threed_right_gc;
  double threed_delta;

  struct rock *rocks;
  int nrocks;
  Pixmap pixmaps [MAX_SIZE];
  int delay;

  int move_current_dep[2];
  int move_speed[2];
  short move_direction[2];
  int move_limit[2];

  int current_delta;	/* observer Z rotation */
  int  new_delta;
  int  dchange_tick;
};



#define GETZDIFF(z) \
        (st->threed_delta * 40.0 * \
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

static void rock_compute (struct state *, struct rock *);
static void rock_draw (struct state *, struct rock *, Bool draw_p);

static void
rock_reset (struct state *st, struct rock *rock)
{
  rock->real_size = MAX_SIZE;
  rock->r = (SIN_RESOLUTION * 0.7) + (random () % (30 * SIN_RESOLUTION));
  rock->theta = random () % SIN_RESOLUTION;
  rock->depth = MAX_DEPTH * DEPTH_SCALE;
  rock->color = random() % st->ncolors;
  rock_compute (st, rock);
  rock_draw (st, rock, True);
}

static void
rock_tick (struct state *st, struct rock *rock, int d)
{
  if (rock->depth > 0)
    {
      rock_draw (st, rock, False);
      rock->depth -= st->speed;
      if (st->rotate_p)
        {
	  rock->theta = (rock->theta + d) % SIN_RESOLUTION;
        }
      while (rock->theta < 0)
	rock->theta += SIN_RESOLUTION;
      if (rock->depth < (MIN_DEPTH * DEPTH_SCALE))
	rock->depth = 0;
      else
	{
	  rock_compute (st, rock);
	  rock_draw (st, rock, True);
	}
    }
  else if ((random () % 40) == 0)
    rock_reset (st, rock);
}

static void
rock_compute (struct state *st, struct rock *rock)
{
  double factor = st->depths [rock->depth];
  double rsize = rock->real_size * factor;

  rock->size = (int) (rsize + 0.5);
  rock->diff = (int) GETZDIFF(rock->depth);
  rock->x = st->midx + (st->coss [rock->theta] * rock->r * factor);
  rock->y = st->midy + (st->sins [rock->theta] * rock->r * factor);

  if (st->move_p)
    {
      double move_factor = (((double) MOVE_STYLE) -
			    (((double) rock->depth) /
			     (((double) (MAX_DEPTH + 1)) *
			      ((double) DEPTH_SCALE))));
      /* move_factor is 0 when the rock is close, 1 when far */
      rock->x += (((double) st->dep_x) * move_factor);
      rock->y += (((double) st->dep_y) * move_factor);
    }
}

static void
rock_draw (struct state *st, struct rock *rock, Bool draw_p)
{
  GC gc = (draw_p 
	   ? (st->threed ? st->erase_gc : st->draw_gcs[rock->color])
	   : st->erase_gc);

  if (rock->x <= 0 || rock->y <= 0 || rock->x >= st->width || rock->y >= st->height)
    {
      /* this means that if a rock were to go off the screen at 12:00, but
	 would have been visible at 3:00, it won't come back once the observer
	 rotates around so that the rock would have been visible again.
	 Oh well.
       */
      if (!st->move_p)
	rock->depth = 0;
      return;
    }
  if (rock->size <= 1)
    {
      if (st->threed)
	{
	  if (draw_p) gc = st->threed_left_gc;
	  XDrawPoint (st->dpy, st->window, gc, rock->x - rock->diff, rock->y);
	  if (draw_p) gc = st->threed_right_gc;
	  XDrawPoint (st->dpy, st->window, gc, rock->x + rock->diff, rock->y);
	}
      else
	{
	  XDrawPoint (st->dpy, st->window, gc, rock->x, rock->y);
	}
    }
  else if (rock->size <= MIN_SIZE || !draw_p)
    {
      if (st->threed)
	{
	  if (draw_p) gc = st->threed_left_gc;
	  XFillRectangle(st->dpy, st->window, gc,
			 rock->x - rock->size / 2 - rock->diff,
			 rock->y - rock->size / 2,
			 rock->size, rock->size);
	  if (draw_p) gc = st->threed_right_gc;
	  XFillRectangle(st->dpy, st->window, gc,
			 rock->x - rock->size / 2 + rock->diff,
			 rock->y - rock->size / 2,
			 rock->size, rock->size);
	}
      else
	{
	  XFillRectangle (st->dpy, st->window, gc,
			  rock->x - rock->size/2, rock->y - rock->size/2,
			  rock->size, rock->size);
	}
    }
  else if (rock->size < MAX_SIZE)
    {
      if (st->threed)
	{
	  gc = st->threed_left_gc;
	  XCopyPlane(st->dpy, st->pixmaps[rock->size], st->window, gc,
		     0, 0, rock->size, rock->size,
		     rock->x - rock->size / 2 - rock->diff,
		     rock->y - rock->size / 2, 1L);
	  gc = st->threed_right_gc;
	  XCopyPlane(st->dpy, st->pixmaps[rock->size], st->window, gc,
		     0, 0, rock->size, rock->size,
		     rock->x - rock->size / 2 + rock->diff,
		     rock->y - rock->size / 2, 1L);
	}
      else
	{
	  XCopyPlane (st->dpy, st->pixmaps [rock->size], st->window, gc,
		      0, 0, rock->size, rock->size,
		      rock->x - rock->size/2, rock->y - rock->size/2,
		      1L);
	}
    }
}


static void
init_pixmaps (struct state *st)
{
  int i;
  XGCValues gcv;
  GC fg_gc = 0, bg_gc = 0;
  st->pixmaps [0] = st->pixmaps [1] = 0;
  for (i = MIN_DEPTH; i < MAX_SIZE; i++)
    {
      int w = (1+(i/32))<<5; /* server might be faster if word-aligned */
      int h = i;
      Pixmap p = XCreatePixmap (st->dpy, st->window, w, h, 1);
      XPoint points [7];
      st->pixmaps [i] = p;
      if (! p)
	{
	  fprintf (stderr, "%s: couldn't allocate pixmaps", progname);
	  exit (1);
	}
      if (! fg_gc)
	{	/* must use drawable of pixmap, not window (fmh) */
	  gcv.foreground = 1;
	  fg_gc = XCreateGC (st->dpy, p, GCForeground, &gcv);
	  gcv.foreground = 0;
	  bg_gc = XCreateGC (st->dpy, p, GCForeground, &gcv);
	}
      XFillRectangle (st->dpy, p, bg_gc, 0, 0, w, h);
      points [0].x = i * 0.15; points [0].y = i * 0.85;
      points [1].x = i * 0.00; points [1].y = i * 0.20;
      points [2].x = i * 0.30; points [2].y = i * 0.00;
      points [3].x = i * 0.40; points [3].y = i * 0.10;
      points [4].x = i * 0.90; points [4].y = i * 0.10;
      points [5].x = i * 1.00; points [5].y = i * 0.55;
      points [6].x = i * 0.45; points [6].y = i * 1.00;
      XFillPolygon (st->dpy, p, fg_gc, points, 7, Nonconvex, CoordModeOrigin);
    }
  XFreeGC (st->dpy, fg_gc);
  XFreeGC (st->dpy, bg_gc);
}


static int
compute_move(struct state *st, int axe)			/* 0 for x, 1 for y */
{
  int change = 0;

  st->move_limit[0] = st->midx;
  st->move_limit[1] = st->midy;

  st->move_current_dep[axe] += st->move_speed[axe];	/* We adjust the displacement */

  if (st->move_current_dep[axe] > (int) (st->move_limit[axe] * st->max_dep))
    {
      if (st->move_current_dep[axe] > st->move_limit[axe])
	st->move_current_dep[axe] = st->move_limit[axe];
      st->move_direction[axe] = -1;
    }			/* This is when we reach the upper screen limit */
  if (st->move_current_dep[axe] < (int) (-st->move_limit[axe] * st->max_dep))
    {
      if (st->move_current_dep[axe] < -st->move_limit[axe])
	st->move_current_dep[axe] = -st->move_limit[axe];
      st->move_direction[axe] = 1;
    }			/* This is when we reach the lower screen limit */
  if (st->move_direction[axe] == 1)	/* We adjust the speed */
    st->move_speed[axe] += 1;
  else if (st->move_direction[axe] == -1)
    st->move_speed[axe] -= 1;

  if (st->move_speed[axe] > MAX_DEP_SPEED)
    st->move_speed[axe] = MAX_DEP_SPEED;
  else if (st->move_speed[axe] < -MAX_DEP_SPEED)
    st->move_speed[axe] = -MAX_DEP_SPEED;

  if (st->move_p && !(random() % DIRECTION_CHANGE_RATE))
    {
      /* We change direction */
      change = random() & 1;
      if (change != 1)
	{
	  if (st->move_direction[axe] == 0)
	    st->move_direction[axe] = change - 1;	/* 0 becomes either 1 or -1 */
	  else
	    st->move_direction[axe] = 0;			/* -1 or 1 become 0 */
	}
    }
  return (st->move_current_dep[axe]);
}

static void
tick_rocks (struct state *st, int d)
{
  int i;

  if (st->move_p)
    {
      st->dep_x = compute_move(st, 0);
      st->dep_y = compute_move(st, 1);
    }

  for (i = 0; i < st->nrocks; i++)
    rock_tick (st, &st->rocks [i], d);
}


static unsigned long
rocks_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  if (st->current_delta != st->new_delta)
    {
      if (st->dchange_tick++ == 5)
	{
	  st->dchange_tick = 0;
	  if (st->current_delta < st->new_delta)
	    st->current_delta++;
	  else
	    st->current_delta--;
	}
    }
  else
    {
      if (! (random() % 50))
	{
	  st->new_delta = ((random() % 11) - 5);
	  if (! (random() % 10))
	    st->new_delta *= 5;
	}
    }
  tick_rocks (st, st->current_delta);

  return st->delay;
}

static void *
rocks_init (Display *d, Window w)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  int i;
  XGCValues gcv;
  Colormap cmap;
  XWindowAttributes xgwa;
  unsigned int bg;
  st->dpy = d;
  st->window = w;
  XGetWindowAttributes (st->dpy, st->window, &xgwa);

  st->width = xgwa.width;
  st->height = xgwa.height;
  st->midx = st->width/2;
  st->midy = st->height/2;

  cmap = xgwa.colormap;
  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  if (st->delay < 0) st->delay = 0;
  st->speed = get_integer_resource (st->dpy, "speed", "Integer");
  if (st->speed < 1) st->speed = 1;
  if (st->speed > 100) st->speed = 100;
  st->rotate_p = get_boolean_resource (st->dpy, "rotate", "Boolean");
  st->move_p = get_boolean_resource (st->dpy, "move", "Boolean");
  if (mono_p)
    st->ncolors = 2;
  else
    st->ncolors = get_integer_resource (st->dpy, "colors", "Colors");

  if (st->ncolors < 2)
    {
      st->ncolors = 2;
      mono_p = True;
  }

  st->colors = (XColor *) malloc(st->ncolors * sizeof(*st->colors));
  st->draw_gcs = (GC *) malloc(st->ncolors * sizeof(*st->draw_gcs));

  bg = get_pixel_resource (st->dpy, cmap, "background", "Background");
  st->colors[0].pixel = bg;
  st->colors[0].flags = DoRed|DoGreen|DoBlue;
  XQueryColor(st->dpy, cmap, &st->colors[0]);

  st->ncolors--;
  make_random_colormap(st->dpy, xgwa.visual, cmap, st->colors+1, &st->ncolors, True,
		       True, 0, True);
  st->ncolors++;

  if (st->ncolors < 2)
    {
      st->ncolors = 2;
      mono_p = True;
  }

  if (mono_p)
    {
      unsigned int fg = get_pixel_resource(st->dpy, cmap, "foreground", "Foreground");
      st->colors[1].pixel = fg;
      st->colors[1].flags = DoRed|DoGreen|DoBlue;
      XQueryColor(st->dpy, cmap, &st->colors[1]);
      gcv.foreground = fg;
      gcv.background = bg;
      st->draw_gcs[0] = XCreateGC (st->dpy, st->window, GCForeground|GCBackground, &gcv);
      st->draw_gcs[1] = st->draw_gcs[0];
    }
  else
    for( i = 0; i < st->ncolors; i++ )
      {
	gcv.foreground = st->colors[i].pixel;
	gcv.background = bg;
	st->draw_gcs[i] = XCreateGC (st->dpy, st->window, GCForeground|GCBackground, &gcv);
      }

  gcv.foreground = bg;
  st->erase_gc = XCreateGC (st->dpy, st->window, GCForeground|GCBackground, &gcv);

  st->max_dep = (st->move_p ? MAX_DEP : 0);

  for (i = 0; i < SIN_RESOLUTION; i++)
    {
      st->sins [i] = sin ((((double) i) / (SIN_RESOLUTION / 2)) * M_PI);
      st->coss [i] = cos ((((double) i) / (SIN_RESOLUTION / 2)) * M_PI);
    }
  /* we actually only need i/speed of these, but wtf */
  for (i = 1; i < (sizeof (st->depths) / sizeof (st->depths[0])); i++)
    st->depths [i] = atan (((double) 0.5) / (((double) i) / DEPTH_SCALE));
  st->depths [0] = M_PI/2; /* avoid division by 0 */

  st->threed = get_boolean_resource(st->dpy, "use3d", "Boolean");
  if (st->threed)
    {
      gcv.background = bg;
      gcv.foreground = get_pixel_resource (st->dpy, cmap, "left3d", "Foreground");
      st->threed_left_gc = XCreateGC (st->dpy, st->window, GCForeground|GCBackground,&gcv);
      gcv.foreground = get_pixel_resource (st->dpy, cmap, "right3d", "Foreground");
      st->threed_right_gc = XCreateGC (st->dpy, st->window,GCForeground|GCBackground,&gcv);
      st->threed_delta = get_float_resource(st->dpy, "delta3d", "Integer");
    }

  /* don't want any exposure events from XCopyPlane */
  for( i = 0; i < st->ncolors; i++)
    XSetGraphicsExposures (st->dpy, st->draw_gcs[i], False);
  XSetGraphicsExposures (st->dpy, st->erase_gc, False);

  st->nrocks = get_integer_resource (st->dpy, "count", "Count");
  if (st->nrocks < 1) st->nrocks = 1;
  st->rocks = (struct rock *) calloc (st->nrocks, sizeof (struct rock));
  init_pixmaps (st);
  XClearWindow (st->dpy, st->window);
  return st;
}

static void
rocks_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  st->width = w;
  st->height = h;
  st->midx = st->width/2;
  st->midy = st->height/2;
}

static Bool
rocks_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
rocks_free (Display *dpy, Window window, void *closure)
{
}



static const char *rocks_defaults [] = {
  ".background:	Black",
  ".foreground:	#E9967A",
  "*fpsSolid:	true",
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

static XrmOptionDescRec rocks_options [] = {
  { "-count",		".count",	XrmoptionSepArg, 0 },
  { "-rotate",          ".rotate",      XrmoptionNoArg,  "true" },
  { "-no-rotate",        ".rotate",      XrmoptionNoArg,  "false" },
  { "-move",            ".move",        XrmoptionNoArg,  "true" },
  { "-no-move",          ".move",        XrmoptionNoArg,  "false" },
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

XSCREENSAVER_MODULE ("Rocks", rocks)
