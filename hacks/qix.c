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

#include "screenhack.h"
#include "alpha.h"
#include <stdio.h>

#define MAXPOLY	16
#define SCALE	6

struct qpoint {
  int x, y;
  int dx, dy;
};

struct qline {
  struct qpoint *p;
  XColor color;
  Bool dead;
};

struct qix {
  int id;
  int fp;
  int nlines;
  int npoly;
  struct qline *lines;
};

struct state {
  Display *dpy;
  Window window;

  GC draw_gc, erase_gc;
  unsigned int default_fg_pixel;
  long maxx, maxy, max_spread, max_size;
  int color_shift;
  Bool random_p, solid_p, xor_p, transparent_p, gravity_p;
  int delay;
  int count;
  Colormap cmap;
  int npoly;
  Bool additive_p;
  Bool cmap_p;

  GC *gcs[2];

  int gtick;

  struct qix **qixes;
};


static void
get_geom (struct state *st)
{
  XWindowAttributes xgwa;
  XGetWindowAttributes (st->dpy, st->window, &xgwa);
  st->maxx = ((long)(xgwa.width+1)<<SCALE)  - 1;
  st->maxy = ((long)(xgwa.height+1)<<SCALE) - 1;
}

static struct qix *
init_one_qix (struct state *st, int nlines)
{
  int i, j;
  struct qix *qix = (struct qix *) calloc (1, sizeof (struct qix));
  qix->nlines = nlines;
  qix->lines = (struct qline *) calloc (qix->nlines, sizeof (struct qline));
  qix->npoly = st->npoly;
  for (i = 0; i < qix->nlines; i++)
    qix->lines[i].p = (struct qpoint *)
      calloc(qix->npoly, sizeof(struct qpoint));

# ifndef HAVE_COCOA
  if (!mono_p && !st->transparent_p)
# endif
    {
      hsv_to_rgb (random () % 360, frand (1.0), frand (0.5) + 0.5,
		  &qix->lines[0].color.red, &qix->lines[0].color.green,
		  &qix->lines[0].color.blue);
      if (!XAllocColor (st->dpy, st->cmap, &qix->lines[0].color))
	{
	  qix->lines[0].color.pixel = st->default_fg_pixel;
	  XQueryColor (st->dpy, st->cmap, &qix->lines[0].color);
	  if (!XAllocColor (st->dpy, st->cmap, &qix->lines[0].color))
	    abort ();
	}
    }

  if (st->max_size == 0)
    {
      for (i = 0; i < qix->npoly; i++)
	{
	  qix->lines[0].p[i].x = random () % st->maxx;
	  qix->lines[0].p[i].y = random () % st->maxy;
	}
    }
  else
    {
      /*assert(qix->npoly == 2);*/
      qix->lines[0].p[0].x = random () % st->maxx;
      qix->lines[0].p[0].y = random () % st->maxy;
      qix->lines[0].p[1].x = qix->lines[0].p[0].x + (random () % (st->max_size/2));
      qix->lines[0].p[1].y = qix->lines[0].p[0].y + (random () % (st->max_size/2));
      if (qix->lines[0].p[1].x > st->maxx) qix->lines[0].p[1].x = st->maxx;
      if (qix->lines[0].p[1].y > st->maxy) qix->lines[0].p[1].y = st->maxy;
    }

  for (i = 0; i < qix->npoly; i++)
    {
      qix->lines[0].p[i].dx = (random () % (st->max_spread + 1)) - (st->max_spread /2);
      qix->lines[0].p[i].dy = (random () % (st->max_spread + 1)) - (st->max_spread /2);
    }
  qix->lines[0].dead = True;

  for (i = 1; i < qix->nlines; i++)
    {
      for(j=0; j<qix->npoly; j++)
	qix->lines[i].p[j] = qix->lines[0].p[j];
      qix->lines[i].color = qix->lines[0].color;
      qix->lines[i].dead = qix->lines[0].dead;
  
# ifndef HAVE_COCOA
      if (!mono_p && !st->transparent_p)
# endif
	if (!XAllocColor (st->dpy, st->cmap, &qix->lines[i].color))
	  abort ();
    }
  return qix;
}




static void *
qix_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  int nlines;
  XGCValues gcv;
  XWindowAttributes xgwa;
  st->dpy = dpy;
  st->window = window;
  XGetWindowAttributes (st->dpy, st->window, &xgwa);
  st->cmap = xgwa.colormap;
  st->count = get_integer_resource (st->dpy, "count", "Integer");
  if (st->count <= 0) st->count = 1;
  nlines = get_integer_resource (st->dpy, "segments", "Integer");
  if (nlines <= 0) nlines = 20;
  st->npoly = get_integer_resource(st->dpy, "poly", "Integer");
  if (st->npoly <= 2) st->npoly = 2;
  if (st->npoly > MAXPOLY) st->npoly = MAXPOLY;
  get_geom (st);
  st->max_spread = get_integer_resource (st->dpy, "spread", "Integer");
  if (st->max_spread <= 0) st->max_spread = 10;
  st->max_spread <<= SCALE;
  st->max_size = get_integer_resource (st->dpy, "size", "Integer");
  if (st->max_size < 0) st->max_size = 0;
  st->max_size <<= SCALE;
  st->random_p = get_boolean_resource (st->dpy, "random", "Boolean");
  st->solid_p = get_boolean_resource (st->dpy, "solid", "Boolean");
  st->xor_p = get_boolean_resource (st->dpy, "xor", "Boolean");
  st->transparent_p = get_boolean_resource (st->dpy, "transparent", "Boolean");
  st->gravity_p = get_boolean_resource(st->dpy, "gravity", "Boolean");
  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  st->color_shift = get_integer_resource (st->dpy, "colorShift", "Integer");
  if (st->color_shift < 0 || st->color_shift >= 360) st->color_shift = 5;
  if (st->delay < 0) st->delay = 0;

  /* Clear up ambiguities regarding npoly */
  if (st->solid_p) 
    {
      if (st->npoly != 2)
	fprintf(stderr, "%s: Can't have -solid and -poly; using -poly 2\n",
		progname);
      st->npoly = 2;
    }      
  if (st->npoly > 2)
    {
      if (st->max_size)
	fprintf(stderr, "%s: Can't have -poly and -size; using -size 0\n",
		progname);
      st->max_size = 0;
    }

  if (st->count == 1 && st->transparent_p)
    st->transparent_p = False; /* it's a no-op */

  if (st->transparent_p && CellsOfScreen (DefaultScreenOfDisplay (st->dpy)) <= 2)
    {
      fprintf (stderr, "%s: -transparent only works on color displays.\n",
	       progname);
      st->transparent_p = False;
    }

  if (st->xor_p && !st->transparent_p)
    mono_p = True;

  st->gcs[0] = st->gcs[1] = 0;
  gcv.foreground = st->default_fg_pixel =
    get_pixel_resource (st->dpy, st->cmap, "foreground", "Foreground");

  st->additive_p = get_boolean_resource (st->dpy, "additive", "Boolean");
  st->cmap_p = has_writable_cells (xgwa.screen, xgwa.visual);

# ifndef HAVE_COCOA
  if (st->transparent_p)
    {
      unsigned long *plane_masks = 0;
      unsigned long base_pixel;
      int nplanes = st->count;
      int i;

      allocate_alpha_colors (xgwa.screen, xgwa.visual, st->cmap,
                             &nplanes, st->additive_p, &plane_masks,
			     &base_pixel);

      if (nplanes <= 1)
	{
	  fprintf (stderr,
         "%s: couldn't allocate any color planes; turning -transparent off.\n",
		   progname);
	  st->transparent_p = False;
	  if (st->xor_p)
	    goto NON_TRANSPARENT_XOR;
	  else
	    goto NON_TRANSPARENT;
	}
      else if (nplanes != st->count)
	{
	  fprintf (stderr,
		   "%s: only allocated %d color planes (instead of %d).\n",
		   progname, nplanes, st->count);
	  st->count = nplanes;
	}

      st->gcs[0] = (GC *) malloc (st->count * sizeof (GC));
      st->gcs[1] = (st->xor_p
                    ? st->gcs[0]
                    : (GC *) malloc (st->count * sizeof (GC)));

      for (i = 0; i < st->count; i++)
	{
	  gcv.plane_mask = plane_masks [i];
	  gcv.foreground = ~0;

/*  argh, I'm not sure how to make "-subtractive" work in truecolor...
          if (!cmap_p && !additive_p)
            gcv.function = GXclear;
 */

	  if (st->xor_p)
	    {
	      gcv.function = GXxor;
	      st->gcs [0][i] = XCreateGC (st->dpy, st->window,
                                          GCForeground|GCFunction|GCPlaneMask,
                                          &gcv);
	    }
	  else
	    {
	      st->gcs [0][i] = XCreateGC (st->dpy, st->window, 
                                          GCForeground|GCPlaneMask,
                                          &gcv);
	      gcv.foreground = 0;
	      st->gcs [1][i] = XCreateGC (st->dpy, st->window, 
                                          GCForeground|GCPlaneMask,
                                          &gcv);
# ifdef HAVE_COCOA
           /* jwxyz_XSetAntiAliasing (st->dpy, st->gcs [0][i], False);
              jwxyz_XSetAntiAliasing (st->dpy, st->gcs [1][i], False); */
              if (st->transparent_p)
                {
                  jwxyz_XSetAlphaAllowed (dpy, st->gcs [0][i], True);
                  jwxyz_XSetAlphaAllowed (dpy, st->gcs [1][i], True);
                }
# endif /* HAVE_COCOA */
	    }
	}

      if (plane_masks)
        free (plane_masks);

      XSetWindowBackground (st->dpy, st->window, base_pixel);
      XClearWindow (st->dpy, st->window);
    }
  else
#endif /* !HAVE_COCOA */
  if (st->xor_p)
    {
#ifndef HAVE_COCOA
    NON_TRANSPARENT_XOR:
#endif
      gcv.function = GXxor;
      gcv.foreground =
	(st->default_fg_pixel /* ^ get_pixel_resource (st->dpy, st->cmap,
                                                "background", "Background")*/);
      st->draw_gc = st->erase_gc = XCreateGC(st->dpy,st->window,GCForeground|GCFunction,&gcv);
    }
  else
    {
#ifndef HAVE_COCOA
    NON_TRANSPARENT:
#endif
      st->draw_gc = XCreateGC (st->dpy, st->window, GCForeground, &gcv);
      gcv.foreground = get_pixel_resource (st->dpy, st->cmap,
                                           "background", "Background");
      st->erase_gc = XCreateGC (st->dpy, st->window, GCForeground, &gcv);
    }

#ifdef HAVE_COCOA
  if (st->transparent_p)
    jwxyz_XSetAlphaAllowed (dpy, st->draw_gc, True);
#endif

  st->qixes = (struct qix **) malloc ((st->count + 1) * sizeof (struct qix *));
  st->qixes [st->count] = 0;
  while (st->count--)
    {
      st->qixes [st->count] = init_one_qix (st, nlines);
      st->qixes [st->count]->id = st->count;
    }

# ifdef HAVE_COCOA
  /* line-mode leaves turds without this. */
  jwxyz_XSetAntiAliasing (st->dpy, st->erase_gc, False);
  jwxyz_XSetAntiAliasing (st->dpy, st->draw_gc,  False);
# endif

  return st;
}

static void
free_qline (struct state *st,
	    struct qline *qline,
	    struct qline *prev,
	    struct qix *qix)
{
  int i;
  if (qline->dead || !prev)
    ;
  else if (st->solid_p)
    {
      XPoint points [4];
      /*assert(qix->npoly == 2);*/
      points [0].x = qline->p[0].x >> SCALE; 
      points [0].y = qline->p[0].y >> SCALE;
      points [1].x = qline->p[1].x >> SCALE;
      points [1].y = qline->p[1].y >> SCALE;
      points [2].x = prev->p[1].x >> SCALE;
      points [2].y = prev->p[1].y >> SCALE;
      points [3].x = prev->p[0].x >> SCALE;
      points [3].y = prev->p[0].y >> SCALE;
      XFillPolygon (st->dpy, st->window,
                    (st->transparent_p && st->gcs[1]
                     ? st->gcs[1][qix->id]
                     : st->erase_gc),
		    points, 4, Complex, CoordModeOrigin);
    }
  else
    {
      /*  XDrawLine (dpy, window, (transparent_p ? gcs[1][qix->id] : erase_gc),
	             qline->p1.x, qline->p1.y, qline->p2.x, qline->p2.y);*/
      XPoint points[MAXPOLY+1];
      for(i = 0; i < qix->npoly; i++)
	{
	  points[i].x = qline->p[i].x >> SCALE;
	  points[i].y = qline->p[i].y >> SCALE;
	}
      points[qix->npoly] = points[0];
      XDrawLines(st->dpy, st->window,
                 (st->transparent_p && st->gcs[1] 
                  ? st->gcs[1][qix->id]
                  : st->erase_gc),
		 points, qix->npoly+1, CoordModeOrigin);
    }

  if (!mono_p && !st->transparent_p)
    XFreeColors (st->dpy, st->cmap, &qline->color.pixel, 1, 0);

  qline->dead = True;
}

static void
add_qline (struct state *st,
	   struct qline *qline,
	   struct qline *prev_qline,
	   struct qix *qix)
{
  int i;

  for(i=0; i<qix->npoly; i++)
    qline->p[i] = prev_qline->p[i];
  qline->color = prev_qline->color;
  qline->dead = prev_qline->dead;

#define wiggle(point,delta,max)						\
  if (st->random_p) delta += (random () % (1 << (SCALE+1))) - (1 << SCALE);	\
  if (delta > st->max_spread) delta = st->max_spread;				\
  else if (delta < -st->max_spread) delta = -st->max_spread;			\
  point += delta;							\
  if (point < 0) point = 0, delta = -delta, point += delta<<1;		\
  else if (point > max) point = max, delta = -delta, point += delta<<1;

  if (st->gravity_p)
    for(i=0; i<qix->npoly; i++)
      qline->p[i].dy += 3;

  for (i = 0; i < qix->npoly; i++)
    {
      wiggle (qline->p[i].x, qline->p[i].dx, st->maxx);
      wiggle (qline->p[i].y, qline->p[i].dy, st->maxy);
    }

  if (st->max_size)
    {
      /*assert(qix->npoly == 2);*/
      if (qline->p[0].x - qline->p[1].x > st->max_size)
	qline->p[0].x = qline->p[1].x + st->max_size
	  - (st->random_p ? random() % st->max_spread : 0);
      else if (qline->p[1].x - qline->p[0].x > st->max_size)
	qline->p[1].x = qline->p[0].x + st->max_size
	  - (st->random_p ? random() % st->max_spread : 0);
      if (qline->p[0].y - qline->p[1].y > st->max_size)
	qline->p[0].y = qline->p[1].y + st->max_size
	  - (st->random_p ? random() % st->max_spread : 0);
      else if (qline->p[1].y - qline->p[0].y > st->max_size)
	qline->p[1].y = qline->p[0].y + st->max_size
	  - (st->random_p ? random() % st->max_spread : 0);
    }

#ifndef HAVE_COCOA
  if (!mono_p && !st->transparent_p)
#endif
    {
      XColor desired;

      int h;
      double s, v;
      rgb_to_hsv (qline->color.red, qline->color.green, qline->color.blue,
		  &h, &s, &v);
      h = (h + st->color_shift) % 360;
      hsv_to_rgb (h, s, v,
		  &qline->color.red, &qline->color.green, &qline->color.blue);

      qline->color.flags = DoRed | DoGreen | DoBlue;
      desired = qline->color;
      if (XAllocColor (st->dpy, st->cmap, &qline->color))
	{
	  /* XAllocColor returns the actual RGB that the hardware let us
	     allocate.  Restore the requested values into the XColor struct
	     so that limited-resolution hardware doesn't cause the cycle to
	     get "stuck". */
	  qline->color.red = desired.red;
	  qline->color.green = desired.green;
	  qline->color.blue = desired.blue;
	}
      else
	{
	  qline->color = prev_qline->color;
	  if (!XAllocColor (st->dpy, st->cmap, &qline->color))
	    abort (); /* same color should work */
	}

# ifdef HAVE_COCOA
          if (st->transparent_p)
            {
              /* give a non-opaque alpha to the color */
              unsigned long pixel = qline->color.pixel;
              unsigned long amask = BlackPixelOfScreen (0);
              unsigned long a = (0xBBBBBBBB & amask);
              pixel = (pixel & (~amask)) | a;
              qline->color.pixel = pixel;
            }
# endif /* HAVE_COCOA */

      XSetForeground (st->dpy, st->draw_gc, qline->color.pixel);
    }
  if (! st->solid_p)
    {
      /*  XDrawLine (dpy, window, (transparent_p ? gcs[0][qix->id] : draw_gc),
	             qline->p1.x, qline->p1.y, qline->p2.x, qline->p2.y);*/
      XPoint points[MAXPOLY+1];
      for (i = 0; i < qix->npoly; i++)
	{
	  points[i].x = qline->p[i].x >> SCALE;
	  points[i].y = qline->p[i].y >> SCALE;
	}
      points[qix->npoly] = points[0];
      XDrawLines(st->dpy, st->window, 
                 (st->transparent_p && st->gcs[0]
                  ? st->gcs[0][qix->id]
                  : st->draw_gc),
		 points, qix->npoly+1, CoordModeOrigin);
    }
  else if (!prev_qline->dead)
    {
      XPoint points [4];
      points [0].x = qline->p[0].x >> SCALE;
      points [0].y = qline->p[0].y >> SCALE;
      points [1].x = qline->p[1].x >> SCALE;
      points [1].y = qline->p[1].y >> SCALE;
      points [2].x = prev_qline->p[1].x >> SCALE;
      points [2].y = prev_qline->p[1].y >> SCALE;
      points [3].x = prev_qline->p[0].x >> SCALE;
      points [3].y = prev_qline->p[0].y >> SCALE;
      XFillPolygon (st->dpy, st->window,
                    (st->transparent_p && st->gcs[0]
                     ? st->gcs[0][qix->id]
                     : st->draw_gc),
		    points, 4, Complex, CoordModeOrigin);
    }

  qline->dead = False;
}

static void
qix1 (struct state *st, struct qix *qix)
{
  int ofp = qix->fp - 1;
  if (ofp < 0) ofp = qix->nlines - 1;
  if (st->gtick++ == 500)
    get_geom (st), st->gtick = 0;
  free_qline (st, &qix->lines [qix->fp],
	      &qix->lines[(qix->fp + 1) % qix->nlines], qix);
  add_qline (st, &qix->lines[qix->fp], &qix->lines[ofp], qix);
  if ((++qix->fp) >= qix->nlines)
    qix->fp = 0;
}


static unsigned long
qix_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  struct qix **q1 = st->qixes;
  struct qix **qn;
  for (qn = q1; *qn; qn++)
    qix1 (st, *qn);
  return st->delay;
}

static void
qix_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  get_geom (st);
}

static Bool
qix_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
qix_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  if (st->gcs[0])
    free (st->gcs[0]);
  if (st->gcs[1] && st->gcs[0] != st->gcs[1])
    free (st->gcs[1]);
  free (st->qixes);
  free (st);
}


static const char *qix_defaults [] = {
  ".background:	black",
  ".foreground:	white",
  "*fpsSolid:	true",
  "*count:	4",
  "*segments:	250",
  "*poly:	2",
  "*spread:	8",
  "*size:	200",
  "*colorShift:	3",
  "*solid:	true",
  "*delay:	10000",
  "*random:	false",
  "*xor:	false",
  "*transparent:true",
  "*gravity:	false",
  "*additive:	true",
  0
};

static XrmOptionDescRec qix_options [] = {
  { "-count",		".count",	XrmoptionSepArg, 0 },
  { "-segments",	".segments",	XrmoptionSepArg, 0 },
  { "-poly",		".poly",	XrmoptionSepArg, 0 },
  { "-spread",		".spread",	XrmoptionSepArg, 0 },
  { "-size",		".size",	XrmoptionSepArg, 0 },
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-color-shift",	".colorShift",	XrmoptionSepArg, 0 },
  { "-random",		".random",	XrmoptionNoArg, "true" },
  { "-linear",		".random",	XrmoptionNoArg, "false" },
  { "-solid",		".solid",	XrmoptionNoArg, "true" },
  { "-hollow",		".solid",	XrmoptionNoArg, "false" },
  { "-xor",		".xor",		XrmoptionNoArg, "true" },
  { "-no-xor",		".xor",		XrmoptionNoArg, "false" },
  { "-transparent",	".transparent",	XrmoptionNoArg, "true" },
  { "-non-transparent",	".transparent",	XrmoptionNoArg, "false" },
  { "-gravity",		".gravity",	XrmoptionNoArg, "true" },
  { "-no-gravity",	".gravity",	XrmoptionNoArg, "false" },
  { "-additive",	".additive",	XrmoptionNoArg, "true" },
  { "-subtractive",	".additive",	XrmoptionNoArg, "false" },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Qix", qix)
