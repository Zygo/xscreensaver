/* xscreensaver, Copyright (c) 1997 Jamie Zawinski <jwz@jwz.org>
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
#include "alpha.h"


/* This is pretty compute-intensive, probably due to the large number of
   polygon fills.  I tried introducing a scaling factor to make the spline
   code emit fewer line segments, but that made the edges very rough.
   However, tuning *maxVelocity, *elasticity and *delay can result in much
   smoother looking animation.  I tuned these for a 1280x1024 Indy display,
   but I don't know whether these values will be reasonable for a slower
   machine...

   The more planes the better -- SGIs have a 12-bit pseudocolor display
   (4096 colormap cells) which is mostly useless, except for this program,
   where it means you can have 11 or 12 mutually-transparent objects instead
   of only 7 or 8.  But, if you are using the 12-bit visual, you should crank
   down the velocity and elasticity, or server slowness will cause the
   animation to look jerky (yes, it's sad but true, SGI's X server is
   perceptibly slower when using plane masks on a 12-bit visual than on an
   8-bit visual.)  Using -max-velocity 0.5 -elasticity 0.9 seems to work ok
   on my Indy R5k with visual 0x27 and the bottom-of-the-line 24-bit graphics
   board.

   It might look better if each blob had an outline, which was a *slightly*
   darker color than the center, to give them a bit more definition -- but
   that would mean using two planes per blob.  (Or maybe allocating the
   outline colors outside of the plane-space?  Then the outlines wouldn't be
   transparent, but maybe that wouldn't be so noticeable?)

   Oh, for an alpha channel... maybe I should rewrite this in GL.  Then the
   blobs could have thickness, and curved edges with specular reflections...
 */


#define SCALE       10000  /* fixed-point math, for sub-pixel motion */
#define DEF_COUNT   12	   /* When planes and count are 0, how many blobs. */


#define RAND(n) ((long) ((random() & 0x7fffffff) % ((long) (n))))
#define RANDSIGN() ((random() & 1) ? 1 : -1)

struct blob {
  long x, y;		/* position of midpoint */
  long dx, dy;		/* velocity and direction */
  double torque;	/* rotational speed */
  double th;		/* angle of rotation */
  long elasticity;	/* how fast they deform */
  long max_velocity;	/* speed limit */
  long min_r, max_r;	/* radius range */
  int npoints;		/* control points */
  long *r;		/* radii */
  spline *spline;
};

struct layer {
  int nblobs;
  struct blob **blobs;
  Pixmap pixmap;
  unsigned long pixel;
  GC gc;
};

enum goop_mode {
  transparent,
  opaque,
  xor,
  outline
};

struct goop {
  enum goop_mode mode;
  int width, height;
  int nlayers;
  struct layer **layers;
  unsigned long background;
  Pixmap pixmap;
  GC pixmap_gc;
  GC window_gc;
};


static struct blob *
make_blob (int maxx, int maxy, int size)
{
  struct blob *b = (struct blob *) calloc(1, sizeof(*b));
  int i;
  int mid;

  maxx *= SCALE;
  maxy *= SCALE;
  size *= SCALE;

  b->max_r = size/2;
  b->min_r = size/10;

  if (b->min_r < (5*SCALE)) b->min_r = (5*SCALE);
  mid = ((b->min_r + b->max_r) / 2);

  b->torque       = get_float_resource ("torque", "Torque");
  b->elasticity   = SCALE * get_float_resource ("elasticity", "Elasticity");
  b->max_velocity = SCALE * get_float_resource ("maxVelocity", "MaxVelocity");

  b->x = RAND(maxx);
  b->y = RAND(maxy);

  b->dx = RAND(b->max_velocity) * RANDSIGN();
  b->dy = RAND(b->max_velocity) * RANDSIGN();
  b->th = frand(M_PI+M_PI) * RANDSIGN();
  b->npoints = (random() % 5) + 5;

  b->spline = make_spline (b->npoints);
  b->r = (long *) malloc (sizeof(*b->r) * b->npoints);
  for (i = 0; i < b->npoints; i++)
    b->r[i] = ((random() % mid) + (mid/2)) * RANDSIGN();
  return b;
}

static void 
throb_blob (struct blob *b)
{
  int i;
  double frac = ((M_PI+M_PI) / b->npoints);

  for (i = 0; i < b->npoints; i++)
    {
      long r = b->r[i];
      long ra = (r > 0 ? r : -r);
      double th = (b->th > 0 ? b->th : -b->th);
      long x, y;

      /* place control points evenly around perimiter, shifted by theta */
      x = b->x + ra * cos (i * frac + th);
      y = b->y + ra * sin (i * frac + th);

      b->spline->control_x[i] = x / SCALE;
      b->spline->control_y[i] = y / SCALE;

      /* alter the radius by a random amount, in the direction in which
	 it had been going (the sign of the radius indicates direction.) */
      ra += (RAND(b->elasticity) * (r > 0 ? 1 : -1));
      r = ra * (r >= 0 ? 1 : -1);

      /* If we've reached the end (too long or too short) reverse direction. */
      if ((ra > b->max_r && r >= 0) ||
	  (ra < b->min_r && r < 0))
	r = -r;
      /* And reverse direction in mid-course once every 50 times. */
      else if (! (random() % 50))
	r = -r;

      b->r[i] = r;
    }
}

static void
move_blob (struct blob *b, int maxx, int maxy)
{
  maxx *= SCALE;
  maxy *= SCALE;

  b->x += b->dx;
  b->y += b->dy;

  /* If we've reached the edge of the box, reverse direction. */
  if ((b->x > maxx && b->dx >= 0) ||
      (b->x < 0    && b->dx < 0))
    {
      b->dx = -b->dx;
    }
  if ((b->y > maxy && b->dy >= 0) ||
      (b->y < 0    && b->dy < 0))
    {
      b->dy = -b->dy;
    }

  /* Alter velocity randomly. */
  if (! (random() % 10))
    {
      b->dx += (RAND(b->max_velocity/2) * RANDSIGN());
      b->dy += (RAND(b->max_velocity/2) * RANDSIGN());

      /* Throttle velocity */
      if (b->dx > b->max_velocity || b->dx < -b->max_velocity)
	b->dx /= 2;
      if (b->dy > b->max_velocity || b->dy < -b->max_velocity)
	b->dy /= 2;
    }

  {
    double th = b->th;
    double d = (b->torque == 0 ? 0 : frand(b->torque));
    if (th < 0)
      th = -(th + d);
    else
      th += d;

    if (th > (M_PI+M_PI))
      th -= (M_PI+M_PI);
    else if (th < 0)
      th += (M_PI+M_PI);

    b->th = (b->th > 0 ? th : -th);
  }

  /* Alter direction of rotation randomly. */
  if (! (random() % 100))
    b->th *= -1;
}

static void
draw_blob (Display *dpy, Drawable drawable, GC gc, struct blob *b,
	   Bool fill_p)
{
  compute_closed_spline (b->spline);
#ifdef DEBUG
  {
    int i;
    for (i = 0; i < b->npoints; i++)
      XDrawLine (dpy, drawable, gc, b->x/SCALE, b->y/SCALE,
		 b->spline->control_x[i], b->spline->control_y[i]);
  }
#else
  if (fill_p)
    XFillPolygon (dpy, drawable, gc, b->spline->points, b->spline->n_points,
		  Nonconvex, CoordModeOrigin);
  else
#endif
    XDrawLines (dpy, drawable, gc, b->spline->points, b->spline->n_points,
		CoordModeOrigin);
}


static struct layer *
make_layer (Display *dpy, Window window, int width, int height, int nblobs)
{
  int i;
  struct layer *layer = (struct layer *) calloc(1, sizeof(*layer));
  int blob_min, blob_max;
  XGCValues gcv;
  layer->nblobs = nblobs;

  layer->blobs = (struct blob **) malloc(sizeof(*layer->blobs)*layer->nblobs);

  blob_max = (width < height ? width : height) / 2;
  blob_min = (blob_max * 2) / 3;
  for (i = 0; i < layer->nblobs; i++)
    layer->blobs[i] = make_blob (width, height,
				 (random() % (blob_max-blob_min)) + blob_min);

  layer->pixmap = XCreatePixmap (dpy, window, width, height, 1);
  layer->gc = XCreateGC (dpy, layer->pixmap, 0, &gcv);

  return layer;
}

static void
draw_layer_plane (Display *dpy, struct layer *layer, int width, int height)
{
  int i;
  XSetForeground (dpy, layer->gc, 1L);
  XFillRectangle (dpy, layer->pixmap, layer->gc, 0, 0, width, height);
  XSetForeground (dpy, layer->gc, 0L);
  for (i = 0; i < layer->nblobs; i++)
    {
      throb_blob (layer->blobs[i]);
      move_blob (layer->blobs[i], width, height);
      draw_blob (dpy, layer->pixmap, layer->gc, layer->blobs[i], True);
    }
}


static void
draw_layer_blobs (Display *dpy, Drawable drawable, GC gc,
		  struct layer *layer, int width, int height,
		  Bool fill_p)
{
  int i;
  for (i = 0; i < layer->nblobs; i++)
    {
      throb_blob (layer->blobs[i]);
      move_blob (layer->blobs[i], width, height);
      draw_blob (dpy, drawable, gc, layer->blobs[i], fill_p);
    }
}


static struct goop *
make_goop (Display *dpy, Window window, Colormap cmap,
	   int width, int height, long depth)
{
  int i;
  struct goop *goop = (struct goop *) calloc(1, sizeof(*goop));
  XGCValues gcv;
  int nblobs = get_integer_resource ("count", "Count");

  unsigned long *plane_masks = 0;
  unsigned long base_pixel = 0;

  goop->mode = (get_boolean_resource("xor", "Xor")
		? xor
		: (get_boolean_resource("transparent", "Transparent")
		   ? transparent
		   : opaque));

  goop->width = width;
  goop->height = height;


  goop->nlayers = get_integer_resource ("planes", "Planes");
  if (goop->nlayers <= 0)
    goop->nlayers = (random() % (depth-2)) + 2;
  goop->layers = (struct layer **) malloc(sizeof(*goop->layers)*goop->nlayers);


  if (mono_p && goop->mode == transparent)
    goop->mode = opaque;

  /* Try to allocate some color planes before committing to nlayers.
   */
  if (goop->mode == transparent)
    {
      Bool additive_p = get_boolean_resource ("additive", "Additive");
      int nplanes = goop->nlayers;
      allocate_alpha_colors (dpy, cmap, &nplanes, additive_p, &plane_masks,
			     &base_pixel);
      if (nplanes > 1)
	goop->nlayers = nplanes;
      else
	{
	  fprintf (stderr,
         "%s: couldn't allocate any color planes; turning transparency off.\n",
		   progname);
	  goop->mode = opaque;
	}
    }

  {
    int lblobs[32];
    int total = DEF_COUNT;
    memset (lblobs, 0, sizeof(lblobs));
    if (nblobs <= 0)
      while (total)
	for (i = 0; total && i < goop->nlayers; i++)
	  lblobs[i]++, total--;
    for (i = 0; i < goop->nlayers; i++)
      goop->layers[i] = make_layer (dpy, window, width, height, 
				    (nblobs > 0 ? nblobs : lblobs[i]));
  }

  if (goop->mode == transparent && plane_masks)
    {
      for (i = 0; i < goop->nlayers; i++)
	goop->layers[i]->pixel = base_pixel | plane_masks[i];
      goop->background = base_pixel;
    }
  if (plane_masks)
    free (plane_masks);

  if (goop->mode != transparent)
    {
      XColor color;
      color.flags = DoRed|DoGreen|DoBlue;

      goop->background =
	get_pixel_resource ("background", "Background", dpy,cmap);

      for (i = 0; i < goop->nlayers; i++)
	{
	  int H = random() % 360;			   /* range 0-360    */
	  double S = ((double) (random()%70) + 30)/100.0;  /* range 30%-100% */
	  double V = ((double) (random()%34) + 66)/100.0;  /* range 66%-100% */
	  hsv_to_rgb (H, S, V, &color.red, &color.green, &color.blue);
	  if (XAllocColor (dpy, cmap, &color))
	    goop->layers[i]->pixel = color.pixel;
	  else
	    goop->layers[i]->pixel =
	      WhitePixelOfScreen(DefaultScreenOfDisplay(dpy));
	}
    }

  goop->pixmap = XCreatePixmap (dpy, window, width, height,
				(goop->mode == xor ? 1L : depth));

  gcv.background = goop->background;
  gcv.foreground = get_pixel_resource ("foreground", "Foreground", dpy, cmap);
  gcv.line_width = get_integer_resource ("thickness","Thickness");
  goop->pixmap_gc = XCreateGC (dpy, goop->pixmap, GCLineWidth, &gcv);
  goop->window_gc = XCreateGC (dpy, window, GCForeground|GCBackground, &gcv);

  return goop;
}

static struct goop *
init_goop (Display *dpy, Window window)
{
  XWindowAttributes xgwa;
  XGetWindowAttributes (dpy, window, &xgwa);

  return make_goop (dpy, window, xgwa.colormap,
		    xgwa.width, xgwa.height, xgwa.depth);
}

static void
run_goop (Display *dpy, Window window, struct goop *goop)
{
  int i;

  switch (goop->mode)
    {
    case transparent:

      for (i = 0; i < goop->nlayers; i++)
	draw_layer_plane (dpy, goop->layers[i], goop->width, goop->height);

      XSetForeground (dpy, goop->pixmap_gc, goop->background);
      XSetPlaneMask (dpy, goop->pixmap_gc, AllPlanes);
      XFillRectangle (dpy, goop->pixmap, goop->pixmap_gc, 0, 0,
		      goop->width, goop->height);
      XSetForeground (dpy, goop->pixmap_gc, ~0L);
      for (i = 0; i < goop->nlayers; i++)
	{
	  XSetPlaneMask (dpy, goop->pixmap_gc, goop->layers[i]->pixel);
/*
	  XSetForeground (dpy, goop->pixmap_gc, ~0L);
	  XFillRectangle (dpy, goop->pixmap, goop->pixmap_gc, 0, 0,
			  goop->width, goop->height);
	  XSetForeground (dpy, goop->pixmap_gc, 0L);
 */
	  draw_layer_blobs (dpy, goop->pixmap, goop->pixmap_gc,
			    goop->layers[i], goop->width, goop->height,
			    True);
	}
      XCopyArea (dpy, goop->pixmap, window, goop->window_gc, 0, 0,
		 goop->width, goop->height, 0, 0);
      break;

    case xor:
      XSetFunction (dpy, goop->pixmap_gc, GXcopy);
      XSetForeground (dpy, goop->pixmap_gc, 0);
      XFillRectangle (dpy, goop->pixmap, goop->pixmap_gc, 0, 0,
		      goop->width, goop->height);
      XSetFunction (dpy, goop->pixmap_gc, GXxor);
      XSetForeground (dpy, goop->pixmap_gc, 1);
      for (i = 0; i < goop->nlayers; i++)
	draw_layer_blobs (dpy, goop->pixmap, goop->pixmap_gc,
			  goop->layers[i], goop->width, goop->height,
			  (goop->mode != outline));
      XCopyPlane (dpy, goop->pixmap, window, goop->window_gc, 0, 0,
		  goop->width, goop->height, 0, 0, 1L);
      break;

    case opaque:
    case outline:
      XSetForeground (dpy, goop->pixmap_gc, goop->background);
      XFillRectangle (dpy, goop->pixmap, goop->pixmap_gc, 0, 0,
		      goop->width, goop->height);
      for (i = 0; i < goop->nlayers; i++)
	{
	  XSetForeground (dpy, goop->pixmap_gc, goop->layers[i]->pixel);
	  draw_layer_blobs (dpy, goop->pixmap, goop->pixmap_gc,
			    goop->layers[i], goop->width, goop->height,
			    (goop->mode != outline));
	}
      XCopyArea (dpy, goop->pixmap, window, goop->window_gc, 0, 0,
		 goop->width, goop->height, 0, 0);
      break;

    default:
      abort ();
      break;
    }
}


char *progclass = "Goop";

char *defaults [] = {
  ".background:		black",
  ".foreground:		white",
  "*delay:		12000",
  "*transparent:	true",
  "*additive:		true",
  "*xor:		false",
  "*count:		0",
  "*planes:		0",
  "*thickness:		5",
  "*torque:		0.0075",
  "*elasticity:		1.8",
  "*maxVelocity:	1.2",
  0
};

XrmOptionDescRec options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-count",		".count",	XrmoptionSepArg, 0 },
  { "-planes",		".planes",	XrmoptionSepArg, 0 },
  { "-transparent",	".transparent",	XrmoptionNoArg, "True" },
  { "-non-transparent",	".transparent",	XrmoptionNoArg, "False" },
  { "-additive",	".additive",	XrmoptionNoArg, "True" },
  { "-subtractive",	".additive",	XrmoptionNoArg, "false" },
  { "-xor",		".xor",		XrmoptionNoArg, "true" },
  { "-no-xor",		".xor",		XrmoptionNoArg, "false" },
  { "-thickness",	".thickness",	XrmoptionSepArg, 0 },
  { "-torque",		".torque",	XrmoptionSepArg, 0 },
  { "-elasticity",	".elasticity",	XrmoptionSepArg, 0 },
  { "-max-velocity",	".maxVelocity",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

void
screenhack (Display *dpy, Window window)
{
  struct goop *g = init_goop (dpy, window);
  int delay = get_integer_resource ("delay", "Integer");
  while (1)
    {
      run_goop (dpy, window, g);
      XSync (dpy, True);
      if (delay) usleep (delay);
    }
}
