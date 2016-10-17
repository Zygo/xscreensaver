/* xscreensaver, Copyright (c) 1997-2008 Jamie Zawinski <jwz@jwz.org>
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
  Bool additive_p;
  Bool cmap_p;
  int delay;
};


static struct blob *
make_blob (Display *dpy, int maxx, int maxy, int size)
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

  b->torque       = get_float_resource (dpy, "torque", "Torque");
  b->elasticity   = SCALE * get_float_resource (dpy, "elasticity", "Elasticity");
  b->max_velocity = SCALE * get_float_resource (dpy, "maxVelocity", "MaxVelocity");

  b->x = RAND(maxx);
  b->y = RAND(maxy);

  b->dx = RAND(b->max_velocity) * RANDSIGN();
  b->dy = RAND(b->max_velocity) * RANDSIGN();
  b->th = frand(M_PI+M_PI) * RANDSIGN();
  b->npoints = (random() % 5) + 5;

  b->spline = make_spline (b->npoints);
  b->r = (long *) malloc (sizeof(*b->r) * b->npoints);
  for (i = 0; i < b->npoints; i++)
    b->r[i] = (long) ((random() % mid) + (mid/2)) * RANDSIGN();
  return b;
}

static void
free_blob(struct blob *blob)
{
  free_spline(blob->spline);
  free(blob->r);
  free(blob);
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
  for (i = 0; i < layer->nblobs; i++){
    int j = blob_max - blob_min;
    layer->blobs[i] = make_blob (dpy, width, height,
				 (j ? random() % j : 0) + blob_min);
  }

  layer->pixmap = XCreatePixmap (dpy, window, width, height, 1);
  layer->gc = XCreateGC (dpy, layer->pixmap, 0, &gcv);

# ifdef HAVE_JWXYZ
  jwxyz_XSetAlphaAllowed (dpy, layer->gc, True);
# endif /* HAVE_JWXYZ */

  return layer;
}

static void
free_layer(struct layer *layer, Display *dpy)
{
  int i;
  for (i = 0; i < layer->nblobs; i++){
    free_blob(layer->blobs[i]);
  }
  free(layer->blobs);
  XFreeGC(dpy, layer->gc);
  free(layer);
}


#ifndef HAVE_JWXYZ
static void
draw_layer_plane (Display *dpy, struct layer *layer, int width, int height)
{
  int i;
  for (i = 0; i < layer->nblobs; i++)
    {
      throb_blob (layer->blobs[i]);
      move_blob (layer->blobs[i], width, height);
      draw_blob (dpy, layer->pixmap, layer->gc, layer->blobs[i], True);
    }
}
#endif /* !HAVE_JWXYZ */


static void
draw_layer_blobs (Display *dpy, Drawable drawable, GC gc,
		  struct layer *layer, int width, int height,
		  Bool fill_p)
{
  int i;
  for (i = 0; i < layer->nblobs; i++)
    {
      draw_blob (dpy, drawable, gc, layer->blobs[i], fill_p);
      throb_blob (layer->blobs[i]);
      move_blob (layer->blobs[i], width, height);
    }
}


static struct goop *
make_goop (Screen *screen, Visual *visual, Window window, Colormap cmap,
	   int width, int height, long depth)
{
  Display *dpy = DisplayOfScreen (screen);
  int i;
  struct goop *goop = (struct goop *) calloc(1, sizeof(*goop));
  XGCValues gcv;
  int nblobs = get_integer_resource (dpy, "count", "Count");

  unsigned long *plane_masks = 0;
# ifndef HAVE_JWXYZ
  unsigned long base_pixel = 0;
# endif
  char *s;

  s = get_string_resource (dpy, "mode", "Mode");
  goop->mode = transparent;
  if (!s || !*s || !strcasecmp (s, "transparent"))
    ;
  else if (!strcasecmp (s, "opaque"))
    goop->mode = opaque;
  else if (!strcasecmp (s, "xor"))
    goop->mode = xor;
  else
    fprintf (stderr, "%s: bogus mode: \"%s\"\n", progname, s);
  free(s);

  goop->delay = get_integer_resource (dpy, "delay", "Integer");

  goop->width = width;
  goop->height = height;

  goop->nlayers = get_integer_resource (dpy, "planes", "Planes");
  if (goop->nlayers <= 0)
    goop->nlayers = (random() % (depth-2)) + 2;
  if (! goop->layers)
    goop->layers = (struct layer **) 
      malloc(sizeof(*goop->layers)*goop->nlayers);

  goop->additive_p = get_boolean_resource (dpy, "additive", "Additive");
  goop->cmap_p = has_writable_cells (screen, visual);

  if (mono_p && goop->mode == transparent)
    goop->mode = opaque;

# ifndef HAVE_JWXYZ
  /* Try to allocate some color planes before committing to nlayers.
   */
  if (goop->mode == transparent)
    {
      int nplanes = goop->nlayers;
      allocate_alpha_colors (screen, visual, cmap,
                             &nplanes, goop->additive_p, &plane_masks,
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
# endif /* !HAVE_JWXYZ */

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

# ifndef HAVE_JWXYZ
  if (goop->mode == transparent && plane_masks)
    {
      for (i = 0; i < goop->nlayers; i++)
	goop->layers[i]->pixel = base_pixel | plane_masks[i];
      goop->background = base_pixel;
    }
# endif /* !HAVE_JWXYZ */

  if (plane_masks)
    free (plane_masks);

# ifndef HAVE_JWXYZ
  if (goop->mode != transparent)
# endif /* !HAVE_JWXYZ */
    {
      XColor color;
      color.flags = DoRed|DoGreen|DoBlue;

      goop->background =
	get_pixel_resource (dpy,cmap, "background", "Background");

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
# ifdef HAVE_JWXYZ
          if (goop->mode == transparent)
            {
              /* give a non-opaque alpha to the color */
              unsigned long pixel = goop->layers[i]->pixel;
              unsigned long amask = BlackPixelOfScreen (screen);
              unsigned long a = (0xBBBBBBBB & amask);
              pixel = (pixel & (~amask)) | a;
              goop->layers[i]->pixel = pixel;
            }
# endif /* HAVE_JWXYZ */
	}
    }

  goop->pixmap = XCreatePixmap (dpy, window, width, height,
				(goop->mode == xor ? 1L : depth));

  gcv.background = goop->background;
  gcv.foreground = get_pixel_resource (dpy, cmap, "foreground", "Foreground");
  gcv.line_width = get_integer_resource (dpy, "thickness","Thickness");
  goop->pixmap_gc = XCreateGC (dpy, goop->pixmap, GCLineWidth, &gcv);
  goop->window_gc = XCreateGC (dpy, window, GCForeground|GCBackground, &gcv);

# ifdef HAVE_JWXYZ
  jwxyz_XSetAlphaAllowed (dpy, goop->pixmap_gc, True);
# endif /* HAVE_JWXYZ */
  
  return goop;
}

/* Well, the naming of this function is
   confusing with goop_free()... */
static void
free_goop (struct goop *goop, Display *dpy)
{
  int i;
  for (i = 0; i < goop->nlayers; i++){
    struct layer * layer = goop->layers[i];
    free_layer(layer, dpy);
  }
  free(goop->layers);
  XFreeGC(dpy, goop->pixmap_gc);
  XFreeGC(dpy, goop->window_gc);
}

static void *
goop_init (Display *dpy, Window window)
{
  XWindowAttributes xgwa;
  XGetWindowAttributes (dpy, window, &xgwa);
  return make_goop (xgwa.screen, xgwa.visual, window, xgwa.colormap,
		    xgwa.width, xgwa.height, xgwa.depth);
}

static unsigned long
goop_draw (Display *dpy, Window window, void *closure)
{
  struct goop *goop = (struct goop *) closure;
  int i;

  switch (goop->mode)
    {
# ifndef HAVE_JWXYZ
    case transparent:

      for (i = 0; i < goop->nlayers; i++)
	draw_layer_plane (dpy, goop->layers[i], goop->width, goop->height);

      XSetForeground (dpy, goop->pixmap_gc, goop->background);
      XSetFunction (dpy, goop->pixmap_gc, GXcopy);
      XSetPlaneMask (dpy, goop->pixmap_gc, AllPlanes);
      XFillRectangle (dpy, goop->pixmap, goop->pixmap_gc, 0, 0,
		      goop->width, goop->height);

      XSetForeground (dpy, goop->pixmap_gc, ~0L);

      if (!goop->cmap_p && !goop->additive_p)
        {
          int j;
          for (i = 0; i < goop->nlayers; i++)
            for (j = 0; j < goop->layers[i]->nblobs; j++)
              draw_blob (dpy, goop->pixmap, goop->pixmap_gc,
                         goop->layers[i]->blobs[j], True);
          XSetFunction (dpy, goop->pixmap_gc, GXclear);
        }

      for (i = 0; i < goop->nlayers; i++)
	{
	  XSetPlaneMask (dpy, goop->pixmap_gc, goop->layers[i]->pixel);
	  draw_layer_blobs (dpy, goop->pixmap, goop->pixmap_gc,
			    goop->layers[i], goop->width, goop->height,
			    True);
	}
      XCopyArea (dpy, goop->pixmap, window, goop->window_gc, 0, 0,
		 goop->width, goop->height, 0, 0);
      break;
#endif /* !HAVE_JWXYZ */

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

# ifdef HAVE_JWXYZ
    case transparent:
# endif
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
  return goop->delay;
}

static void
goop_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct goop *goop = (struct goop *) closure;

  if (w != goop->width || h != goop->height)
    {
      struct goop *goop2 = goop_init (dpy, window);
      free_goop(goop, dpy);
      memcpy (goop, goop2, sizeof(*goop));
      free(goop2);
    }
}

static Bool
goop_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
goop_free (Display *dpy, Window window, void *closure)
{
  struct goop *goop = (struct goop *) closure;
  free_goop(goop, dpy);
  free(goop);
}




static const char *goop_defaults [] = {
  ".background:		black",
  ".foreground:		yellow",
  "*delay:		12000",
  "*additive:		true",
  "*mode:		transparent",
  "*count:		1",
  "*planes:		12",
  "*thickness:		5",
  "*torque:		0.0075",
  "*elasticity:		0.9",
  "*maxVelocity:	0.5",
#ifdef HAVE_MOBILE
  "*ignoreRotation:     True",
#endif
  0
};

static XrmOptionDescRec goop_options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-count",		".count",	XrmoptionSepArg, 0 },
  { "-planes",		".planes",	XrmoptionSepArg, 0 },
  { "-mode",		".mode",	XrmoptionSepArg, 0 },
  { "-xor",		".mode",	XrmoptionNoArg, "xor" },
  { "-transparent",	".mode",	XrmoptionNoArg, "transparent" },
  { "-opaque",		".mode",	XrmoptionNoArg, "opaque" },
  { "-additive",	".additive",	XrmoptionNoArg, "True" },
  { "-subtractive",	".additive",	XrmoptionNoArg, "false" },
  { "-thickness",	".thickness",	XrmoptionSepArg, 0 },
  { "-torque",		".torque",	XrmoptionSepArg, 0 },
  { "-elasticity",	".elasticity",	XrmoptionSepArg, 0 },
  { "-max-velocity",	".maxVelocity",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Goop", goop)
