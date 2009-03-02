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

#include "screenhack.h"
#include <stdio.h>

struct qpoint {
  int x, y;
  int dx, dy;
};

struct qline {
  struct qpoint p1, p2;
  XColor color;
  Bool dead;
};

struct qix {
  int id;
  int fp;
  int nlines;
  struct qline *lines;
};

static GC draw_gc, erase_gc;
static unsigned int default_fg_pixel;
static int maxx, maxy, max_spread, max_size, color_shift;
static Bool random_p, solid_p, xor_p, transparent_p;
static int delay;
static int count;
static Colormap cmap;
static unsigned long base_pixel;

static GC *gcs[2];

static void
get_geom (dpy, window)
     Display *dpy;
     Window window;
{
  XWindowAttributes xgwa;
  XGetWindowAttributes (dpy, window, &xgwa);
  maxx = xgwa.width;
  maxy = xgwa.height;
}

static struct qix *
init_one_qix (dpy, window, nlines)
     Display *dpy;
     Window window;
     int nlines;
{
  int i;
  struct qix *qix = (struct qix *) calloc (1, sizeof (struct qix));
  qix->nlines = nlines;
  qix->lines = (struct qline *) calloc (qix->nlines, sizeof (struct qline));
  
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
  qix->lines[0].p1.x = random () % maxx;
  qix->lines[0].p1.y = random () % maxy;
  if (max_size == 0)
    {
      qix->lines[0].p2.x = random () % maxx;
      qix->lines[0].p2.y = random () % maxy;
    }
  else
    {
      qix->lines[0].p2.x = qix->lines[0].p1.x + (random () % (max_size/2));
      qix->lines[0].p2.y = qix->lines[0].p1.y + (random () % (max_size/2));
      if (qix->lines[0].p2.x > maxx) qix->lines[0].p2.x = maxx;
      if (qix->lines[0].p2.y > maxy) qix->lines[0].p2.y = maxy;
    }
  qix->lines[0].p1.dx = (random () % (max_spread + 1)) - (max_spread / 2);
  qix->lines[0].p1.dy = (random () % (max_spread + 1)) - (max_spread / 2);
  qix->lines[0].p2.dx = (random () % (max_spread + 1)) - (max_spread / 2);
  qix->lines[0].p2.dy = (random () % (max_spread + 1)) - (max_spread / 2);
  qix->lines[0].dead = True;
  for (i = 1; i < qix->nlines; i++)
    {
      qix->lines[i] = qix->lines[0];
      if (!mono_p && !transparent_p)
	if (!XAllocColor (dpy, cmap, &qix->lines[i].color))
	  abort ();
    }
  return qix;
}

/* I don't believe this fucking language doesn't have builtin exponentiation.
   I further can't believe that the fucking ^ character means fucking XOR!! */
static int i_exp(i,j)
     int i, j;
{
  int k = 1;
  while (j--) k *= i;
  return k;
}


static void
merge_colors (argc, argv, into_color, mask, increment_p)
     int argc;
     XColor **argv;
     XColor *into_color;
     int mask;
     Bool increment_p;
{
  int j;
  *into_color = *argv [0];
  into_color->pixel |= mask;
#define SHORT_INC(x,y) (x = ((((x)+(y)) > 0xFFFF) ? 0xFFFF : ((x)+(y))))
#define SHORT_DEC(x,y) (x = ((((x)-(y)) < 0)      ? 0      : ((x)-(y))))
  for (j = 1; j < argc; j++)
    if (increment_p)
      {
	SHORT_INC (into_color->red,   argv[j]->red);
	SHORT_INC (into_color->green, argv[j]->green);
	SHORT_INC (into_color->blue,  argv[j]->blue);
      }
    else
      {
	SHORT_DEC (into_color->red,   argv[j]->red);
	SHORT_DEC (into_color->green, argv[j]->green);
	SHORT_DEC (into_color->blue,  argv[j]->blue);
      }
#undef SHORT_INC
#undef SHORT_DEC
}

/* fill in all the permutations of colors that XAllocColorCells() has 
   allocated for us.  Thanks Ron, you're an additive kind of guy. */
static void
permute_colors (pcolors, colors, count, plane_masks, increment_p)
     XColor *pcolors, *colors;
     int count;
     unsigned long *plane_masks;
     Bool increment_p;
{
  int out = 0;
  int max = i_exp (2, count);
  if (count > 31) abort ();
  for (out = 1; out < max; out++)
    {
      XColor *argv [32];
      int this_mask = 0;
      int argc = 0;
      int bit;
      for (bit = 0; bit < 32; bit++)
	if (out & (1<<bit))
	  {
	    argv [argc++] = &pcolors [bit];
	    this_mask |= plane_masks [bit];
	  }
      merge_colors (argc, argv, &colors [out-1], this_mask, increment_p);
    }
}


static struct qix **
init_qix (dpy, window)
     Display *dpy;
     Window window;
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
  get_geom (dpy, window);
  max_spread = get_integer_resource ("spread", "Integer");
  if (max_spread <= 0) max_spread = 10;
  max_size = get_integer_resource ("size", "Integer");
  if (max_size < 0) max_size = 0;
  random_p = get_boolean_resource ("random", "Boolean");
  solid_p = get_boolean_resource ("solid", "Boolean");
  xor_p = get_boolean_resource ("xor", "Boolean");
  transparent_p = get_boolean_resource ("transparent", "Boolean");
  delay = get_integer_resource ("delay", "Integer");
  color_shift = get_integer_resource ("colorShift", "Integer");
  if (color_shift < 0 || color_shift >= 360) color_shift = 5;
  if (delay < 0) delay = 0;

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
      Bool increment_p = get_boolean_resource ("additive", "Boolean");
      unsigned long plane_masks [32];
      XColor *pcolors, *colors;
      int nplanes = count;
      int i, total_colors;

      /* permutations would be harder if the number of planes didn't fit
	 in an int.  Who has >32-bit displays anyway... */
      if (nplanes > 31) nplanes = 31;

      while (nplanes > 1 &&
	     !XAllocColorCells (dpy, cmap, False, plane_masks, nplanes,
				&base_pixel, 1))
	nplanes--;

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
      total_colors = i_exp (2, count);
      pcolors = (XColor *) calloc (count, sizeof (XColor));
      colors = (XColor *) calloc (total_colors, sizeof (XColor));
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

	  /* pick the "primary" (not in that sense) colors.
	     If we are in subtractive mode, pick higher intensities. */
	  hsv_to_rgb (random () % 360, frand (1.0),
		      frand (0.5) + (increment_p ? 0.2 : 0.5),
		      &pcolors[i].red, &pcolors[i].green, &pcolors[i].blue);

	  pcolors [i].flags = DoRed | DoGreen | DoBlue;
	  pcolors [i].pixel = base_pixel | plane_masks [i];
	}
      permute_colors (pcolors, colors, count, plane_masks, increment_p);
      /* clone the default background of the window into our "base" pixel */
      colors [total_colors - 1].pixel =
	get_pixel_resource ("background", "Background", dpy, cmap);
      XQueryColor (dpy, cmap, &colors [total_colors - 1]);
      colors [total_colors - 1].pixel = base_pixel;
      XStoreColors (dpy, cmap, colors, total_colors);
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
      qixes [count] = init_one_qix (dpy, window, nlines);
      qixes [count]->id = count;
    }
  return qixes;
}

static void
free_qline (dpy, window, cmap, qline, prev, qix)
     Display *dpy;
     Window window;
     Colormap cmap;
     struct qline *qline, *prev;
     struct qix *qix;
{
  if (qline->dead || !prev)
    ;
  else if (solid_p)
    {
      XPoint points [4];
      points [0].x = qline->p1.x; points [0].y = qline->p1.y;
      points [1].x = qline->p2.x; points [1].y = qline->p2.y;
      points [2].x = prev->p2.x; points [2].y = prev->p2.y;
      points [3].x = prev->p1.x; points [3].y = prev->p1.y;
      XFillPolygon (dpy, window, (transparent_p ? gcs[1][qix->id] : erase_gc),
		    points, 4, Complex, CoordModeOrigin);
    }
  else
    XDrawLine (dpy, window, (transparent_p ? gcs[1][qix->id] : erase_gc),
	       qline->p1.x, qline->p1.y, qline->p2.x, qline->p2.y);

  if (!mono_p && !transparent_p)
    XFreeColors (dpy, cmap, &qline->color.pixel, 1, 0);

  qline->dead = True;
}

static void
add_qline (dpy, window, cmap, qline, prev_qline, qix)
     Display *dpy;
     Window window;
     Colormap cmap;
     struct qline *qline, *prev_qline;
     struct qix *qix;
{
  *qline = *prev_qline;

#define wiggle(point,delta,max)						\
  if (random_p) delta += (random () % 3) - 1;				\
  if (delta > max_spread) delta = max_spread;				\
  else if (delta < -max_spread) delta = -max_spread;			\
  point += delta;							\
  if (point < 0) point = 0, delta = -delta, point += delta<<1;		\
  else if (point > max) point = max, delta = -delta, point += delta<<1;
  
  wiggle (qline->p1.x, qline->p1.dx, maxx);
  wiggle (qline->p1.y, qline->p1.dy, maxy);
  wiggle (qline->p2.x, qline->p2.dx, maxx);
  wiggle (qline->p2.y, qline->p2.dy, maxy);

  if (max_size)
    {
      if (qline->p1.x - qline->p2.x > max_size)
	qline->p1.x = qline->p2.x + max_size
	  - (random_p ? random() % max_spread : 0);
      else if (qline->p2.x - qline->p1.x > max_size)
	qline->p2.x = qline->p1.x + max_size
	  - (random_p ? random() % max_spread : 0);
      if (qline->p1.y - qline->p2.y > max_size)
	qline->p1.y = qline->p2.y + max_size
	  - (random_p ? random() % max_spread : 0);
      else if (qline->p2.y - qline->p1.y > max_size)
	qline->p2.y = qline->p1.y + max_size
	  - (random_p ? random() % max_spread : 0);
    }

  if (!mono_p && !transparent_p)
    {
      XColor desired;
      cycle_hue (&qline->color, color_shift);
      qline->color.flags = DoRed | DoGreen | DoBlue;
      desired = qline->color;
      if (XAllocColor (dpy, cmap, &qline->color))
	{
	  /* XAllocColor returns the actual RGB that the hardware let us
	     allocate.  Restore the requested values into the XColor struct
	     so that limited-resolution hardware doesn't cause cycle_hue to
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
    XDrawLine (dpy, window, (transparent_p ? gcs[0][qix->id] : draw_gc),
	       qline->p1.x, qline->p1.y, qline->p2.x, qline->p2.y);
  else if (!prev_qline->dead)
    {
      XPoint points [4];
      points [0].x = qline->p1.x; points [0].y = qline->p1.y;
      points [1].x = qline->p2.x; points [1].y = qline->p2.y;
      points [2].x = prev_qline->p2.x; points [2].y = prev_qline->p2.y;
      points [3].x = prev_qline->p1.x; points [3].y = prev_qline->p1.y;
      XFillPolygon (dpy, window, (transparent_p ? gcs[0][qix->id] : draw_gc),
		    points, 4, Complex, CoordModeOrigin);
    }

  qline->dead = False;
}

static void
qix1 (dpy, window, qix)
     Display *dpy;
     Window window;
     struct qix *qix;
{
  int ofp = qix->fp - 1;
  static int gtick = 0;
  if (gtick++ == 500)
    get_geom (dpy, window), gtick = 0;
  if (ofp < 0) ofp = qix->nlines - 1;
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
  "*spread:	8",
  "*size:	0",
  "*colorShift:	3",
  "*solid:	false",
  "*delay:	10000",
  "*random:	true",
  "*xor:	false",
  "*transparent:false",
  "*additive:	true",
  0
};

XrmOptionDescRec options [] = {
  { "-count",		".count",	XrmoptionSepArg, 0 },
  { "-segments",	".segments",	XrmoptionSepArg, 0 },
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
  { "-additive",	".additive",	XrmoptionNoArg, "true" },
  { "-subtractive",	".additive",	XrmoptionNoArg, "false" },
};
int options_size = (sizeof (options) / sizeof (options[0]));

void
screenhack (dpy, window)
     Display *dpy;
     Window window;
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
