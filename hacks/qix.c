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

#include "screenhack.h"
#include "alpha.h"
#include <stdio.h>

#define MAXPOLY	16
#define SCALE	6

struct qpoint {
  long x, y;
  long dx, dy;
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

static GC draw_gc, erase_gc;
static unsigned int default_fg_pixel;
static long maxx, maxy, max_spread, max_size;
static int color_shift;
static Bool random_p, solid_p, xor_p, transparent_p, gravity_p;
static int delay;
static int count;
static Colormap cmap;
static int npoly;

static GC *gcs[2];

static void
get_geom (Display *dpy, Window window)
{
  XWindowAttributes xgwa;
  XGetWindowAttributes (dpy, window, &xgwa);
  maxx = ((long)(xgwa.width+1)<<SCALE)  - 1;
  maxy = ((long)(xgwa.height+1)<<SCALE) - 1;
}

static struct qix *
init_one_qix (Display *dpy, Window window, int nlines, int npoly)
{
  int i, j;
  struct qix *qix = (struct qix *) calloc (1, sizeof (struct qix));
  qix->nlines = nlines;
  qix->lines = (struct qline *) calloc (qix->nlines, sizeof (struct qline));
  qix->npoly = npoly;
  for (i = 0; i < qix->nlines; i++)
    qix->lines[i].p = (struct qpoint *)
      calloc(qix->npoly, sizeof(struct qpoint));

  if (!mono_p && !transparent_p)
    {
      hsv_to_rgb (random () % 360, frand (1.0), frand (0.5) + 0.5,
		  &qix->lines[0].color.red, &qix->lines[0].color.green,
		  &qix->lines[0].color.blue);
      if (!XAllocColor (dpy, cmap, &qix->lines[0].color))
	{
	  qix->lines[0].color.pixel = default_fg_pixel;
	  XQueryColor (dpy, cmap, &qix->lines[0].color);
	  if (!XAllocColor (dpy, cmap, &qix->lines[0].color))
	    abort ();
	}
    }

  if (max_size == 0)
    {
      for (i = 0; i < qix->npoly; i++)
	{
	  qix->lines[0].p[i].x = random () % maxx;
	  qix->lines[0].p[i].y = random () % maxy;
	}
    }
  else
    {
      /*assert(qix->npoly == 2);*/
      qix->lines[0].p[0].x = random () % maxx;
      qix->lines[0].p[0].y = random () % maxy;
      qix->lines[0].p[1].x = qix->lines[0].p[0].x + (random () % (max_size/2));
      qix->lines[0].p[1].y = qix->lines[0].p[0].y + (random () % (max_size/2));
      if (qix->lines[0].p[1].x > maxx) qix->lines[0].p[1].x = maxx;
      if (qix->lines[0].p[1].y > maxy) qix->lines[0].p[1].y = maxy;
    }

  for (i = 0; i < qix->npoly; i++)
    {
      qix->lines[0].p[i].dx = (random () % (max_spread + 1)) - (max_spread /2);
      qix->lines[0].p[i].dy = (random () % (max_spread + 1)) - (max_spread /2);
    }
  qix->lines[0].dead = True;

  for (i = 1; i < qix->nlines; i++)
    {
      for(j=0; j<qix->npoly; j++)
	qix->lines[i].p[j] = qix->lines[0].p[j];
      qix->lines[i].color = qix->lines[0].color;
      qix->lines[i].dead = qix->lines[0].dead;
  
      if (!mono_p && !transparent_p)
	if (!XAllocColor (dpy, cmap, &qix->lines[i].color))
	  abort ();
    }
  return qix;
}




static struct qix **
init_qix (Display *dpy, Window window)
{
  int nlines;
  struct qix **qixes;
  XGCValues gcv;
  XWindowAttributes xgwa;
  XGetWindowAttributes (dpy, window, &xgwa);
  cmap = xgwa.colormap;
  count = get_integer_resource ("count", "Integer");
  if (count <= 0) count = 1;
  nlines = get_integer_resource ("segments", "Integer");
  if (nlines <= 0) nlines = 20;
  npoly = get_integer_resource("poly", "Integer");
  if (npoly <= 2) npoly = 2;
  if (npoly > MAXPOLY) npoly = MAXPOLY;
  get_geom (dpy, window);
  max_spread = get_integer_resource ("spread", "Integer");
  if (max_spread <= 0) max_spread = 10;
  max_spread <<= SCALE;
  max_size = get_integer_resource ("size", "Integer");
  if (max_size < 0) max_size = 0;
  max_size <<= SCALE;
  random_p = get_boolean_resource ("random", "Boolean");
  solid_p = get_boolean_resource ("solid", "Boolean");
  xor_p = get_boolean_resource ("xor", "Boolean");
  transparent_p = get_boolean_resource ("transparent", "Boolean");
  gravity_p = get_boolean_resource("gravity", "Boolean");
  delay = get_integer_resource ("delay", "Integer");
  color_shift = get_integer_resource ("colorShift", "Integer");
  if (color_shift < 0 || color_shift >= 360) color_shift = 5;
  if (delay < 0) delay = 0;

  /* Clear up ambiguities regarding npoly */
  if (solid_p) 
    {
      if (npoly != 2)
	fprintf(stderr, "%s: Can't have -solid and -poly; using -poly 2\n",
		progname);
      npoly = 2;
    }      
  if (npoly > 2)
    {
      if (max_size)
	fprintf(stderr, "%s: Can't have -poly and -size; using -size 0\n",
		progname);
      max_size = 0;
    }

  if (count == 1 && transparent_p)
    transparent_p = False; /* it's a no-op */

  if (transparent_p && CellsOfScreen (DefaultScreenOfDisplay (dpy)) <= 2)
    {
      fprintf (stderr, "%s: -transparent only works on color displays.\n",
	       progname);
      transparent_p = False;
    }

  if (xor_p && !transparent_p)
    mono_p = True;

  gcs[0] = gcs[1] = 0;
  gcv.foreground = default_fg_pixel =
    get_pixel_resource ("foreground", "Foreground", dpy, cmap);

  if (transparent_p)
    {
      Bool additive_p = get_boolean_resource ("additive", "Boolean");
      unsigned long *plane_masks = 0;
      unsigned long base_pixel;
      int nplanes = count;
      int i;

      allocate_alpha_colors (dpy, cmap, &nplanes, additive_p, &plane_masks,
			     &base_pixel);

      if (nplanes <= 1)
	{
	  fprintf (stderr,
         "%s: couldn't allocate any color planes; turning -transparent off.\n",
		   progname);
	  transparent_p = False;
	  if (xor_p)
	    goto NON_TRANSPARENT_XOR;
	  else
	    goto NON_TRANSPARENT;
	}
      else if (nplanes != count)
	{
	  fprintf (stderr,
		   "%s: only allocated %d color planes (instead of %d).\n",
		   progname, nplanes, count);
	  count = nplanes;
	}

      gcs[0] = (GC *) malloc (count * sizeof (GC));
      gcs[1] = xor_p ? gcs[0] : (GC *) malloc (count * sizeof (GC));


      for (i = 0; i < count; i++)
	{
	  gcv.plane_mask = plane_masks [i];
	  gcv.foreground = ~0;
	  if (xor_p)
	    {
	      gcv.function = GXxor;
	      gcs [0][i] = XCreateGC (dpy, window,
				      GCForeground|GCFunction|GCPlaneMask,
				      &gcv);
	    }
	  else
	    {
	      gcs [0][i] = XCreateGC (dpy, window, GCForeground|GCPlaneMask,
				      &gcv);
	      gcv.foreground = 0;
	      gcs [1][i] = XCreateGC (dpy, window, GCForeground|GCPlaneMask,
				      &gcv);
	    }
	}

      XSetWindowBackground (dpy, window, base_pixel);
      XClearWindow (dpy, window);
    }
  else if (xor_p)
    {
    NON_TRANSPARENT_XOR:
      gcv.function = GXxor;
      gcv.foreground =
	(default_fg_pixel ^ get_pixel_resource ("background", "Background",
						dpy, cmap));
      draw_gc = erase_gc = XCreateGC(dpy,window,GCForeground|GCFunction,&gcv);
    }
  else
    {
    NON_TRANSPARENT:
      draw_gc = XCreateGC (dpy, window, GCForeground, &gcv);
      gcv.foreground = get_pixel_resource ("background", "Background",
					   dpy, cmap);
      erase_gc = XCreateGC (dpy, window, GCForeground, &gcv);
    }

  qixes = (struct qix **) malloc ((count + 1) * sizeof (struct qix *));
  qixes [count] = 0;
  while (count--)
    {
      qixes [count] = init_one_qix (dpy, window, nlines, npoly);
      qixes [count]->id = count;
    }
  return qixes;
}

static void
free_qline (Display *dpy, Window window, Colormap cmap,
	    struct qline *qline,
	    struct qline *prev,
	    struct qix *qix)
{
  int i;
  if (qline->dead || !prev)
    ;
  else if (solid_p)
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
      XFillPolygon (dpy, window, (transparent_p ? gcs[1][qix->id] : erase_gc),
		    points, 4, Complex, CoordModeOrigin);
    }
  else
    {
      /*  XDrawLine (dpy, window, (transparent_p ? gcs[1][qix->id] : erase_gc),
	             qline->p1.x, qline->p1.y, qline->p2.x, qline->p2.y);*/
      static XPoint points[MAXPOLY+1];
      for(i = 0; i < qix->npoly; i++)
	{
	  points[i].x = qline->p[i].x >> SCALE;
	  points[i].y = qline->p[i].y >> SCALE;
	}
      points[qix->npoly] = points[0];
      XDrawLines(dpy, window, (transparent_p ? gcs[1][qix->id] : erase_gc),
		 points, qix->npoly+1, CoordModeOrigin);
    }

  if (!mono_p && !transparent_p)
    XFreeColors (dpy, cmap, &qline->color.pixel, 1, 0);

  qline->dead = True;
}

static void
add_qline (Display *dpy, Window window, Colormap cmap,
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
  if (random_p) delta += (random () % (1 << (SCALE+1))) - (1 << SCALE);	\
  if (delta > max_spread) delta = max_spread;				\
  else if (delta < -max_spread) delta = -max_spread;			\
  point += delta;							\
  if (point < 0) point = 0, delta = -delta, point += delta<<1;		\
  else if (point > max) point = max, delta = -delta, point += delta<<1;

  if (gravity_p)
    for(i=0; i<qix->npoly; i++)
      qline->p[i].dy += 3;

  for (i = 0; i < qix->npoly; i++)
    {
      wiggle (qline->p[i].x, qline->p[i].dx, maxx);
      wiggle (qline->p[i].y, qline->p[i].dy, maxy);
    }

  if (max_size)
    {
      /*assert(qix->npoly == 2);*/
      if (qline->p[0].x - qline->p[1].x > max_size)
	qline->p[0].x = qline->p[1].x + max_size
	  - (random_p ? random() % max_spread : 0);
      else if (qline->p[1].x - qline->p[0].x > max_size)
	qline->p[1].x = qline->p[0].x + max_size
	  - (random_p ? random() % max_spread : 0);
      if (qline->p[0].y - qline->p[1].y > max_size)
	qline->p[0].y = qline->p[1].y + max_size
	  - (random_p ? random() % max_spread : 0);
      else if (qline->p[1].y - qline->p[0].y > max_size)
	qline->p[1].y = qline->p[0].y + max_size
	  - (random_p ? random() % max_spread : 0);
    }

  if (!mono_p && !transparent_p)
    {
      XColor desired;

      int h;
      double s, v;
      rgb_to_hsv (qline->color.red, qline->color.green, qline->color.blue,
		  &h, &s, &v);
      h = (h + color_shift) % 360;
      hsv_to_rgb (h, s, v,
		  &qline->color.red, &qline->color.green, &qline->color.blue);

      qline->color.flags = DoRed | DoGreen | DoBlue;
      desired = qline->color;
      if (XAllocColor (dpy, cmap, &qline->color))
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
	  if (!XAllocColor (dpy, cmap, &qline->color))
	    abort (); /* same color should work */
	}
      XSetForeground (dpy, draw_gc, qline->color.pixel);
    }
  if (! solid_p)
    {
      /*  XDrawLine (dpy, window, (transparent_p ? gcs[0][qix->id] : draw_gc),
	             qline->p1.x, qline->p1.y, qline->p2.x, qline->p2.y);*/
      static XPoint points[MAXPOLY+1];
      for (i = 0; i < qix->npoly; i++)
	{
	  points[i].x = qline->p[i].x >> SCALE;
	  points[i].y = qline->p[i].y >> SCALE;
	}
      points[qix->npoly] = points[0];
      XDrawLines(dpy, window, (transparent_p ? gcs[0][qix->id] : draw_gc),
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
      XFillPolygon (dpy, window, (transparent_p ? gcs[0][qix->id] : draw_gc),
		    points, 4, Complex, CoordModeOrigin);
    }

  qline->dead = False;
}

static void
qix1 (Display *dpy, Window window, struct qix *qix)
{
  int ofp = qix->fp - 1;
  static int gtick = 0;
  if (ofp < 0) ofp = qix->nlines - 1;
  if (gtick++ == 500)
    get_geom (dpy, window), gtick = 0;
  free_qline (dpy, window, cmap, &qix->lines [qix->fp],
	      &qix->lines[(qix->fp + 1) % qix->nlines], qix);
  add_qline (dpy, window, cmap, &qix->lines[qix->fp], &qix->lines[ofp], qix);
  if ((++qix->fp) >= qix->nlines)
    qix->fp = 0;
}


char *progclass = "Qix";

char *defaults [] = {
  "Qix.background:	black",		/* to placate SGI */
  "Qix.foreground:	white",
  "*count:	1",
  "*segments:	50",
  "*poly:	2",
  "*spread:	8",
  "*size:	0",
  "*colorShift:	3",
  "*solid:	false",
  "*delay:	10000",
  "*random:	true",
  "*xor:	false",
  "*transparent:false",
  "*gravity:	false",
  "*additive:	true",
  0
};

XrmOptionDescRec options [] = {
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

void
screenhack (Display *dpy, Window window)
{
  struct qix **q1 = init_qix (dpy, window);
  struct qix **qn;
  while (1)
    for (qn = q1; *qn; qn++)
      {
	qix1 (dpy, window, *qn);
	XSync (dpy, True);
	if (delay) usleep (delay);
      }
}
