/* xscreensaver, Copyright (c) 1993-2014 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This file was ported from xlock for use in xscreensaver (and standalone)
 * by jwz on 18-Oct-93.  (And again, 11-May-97.)  Original copyright reads:
 *
 *   static char sccsid[] = "@(#)flame.c 1.4 91/09/27 XLOCK";
 *
 * flame.c - recursive fractal cosmic flames.
 *
 * Copyright (c) 1991 by Patrick J. Naughton.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * Comments and additions should be sent to the author:
 *
 *		       naughton@eng.sun.com
 *
 *		       Patrick J. Naughton
 *		       MS 21-14
 *		       Sun Laboritories, Inc.
 *		       2550 Garcia Ave
 *		       Mountain View, CA  94043
 *
 * Revision History:
 * 01-Jun-95: This should look more like the original with some updates by
 *            Scott Draves.
 * 27-Jun-91: vary number of functions used.
 * 24-Jun-91: fixed portability problem with integer mod (%).
 * 06-Jun-91: Written. (received from Scott Draves, spot@cs.cmu.edu).
 */

#include <math.h>
#include "screenhack.h"

#include <signal.h>		/* so we can ignore SIGFPE */

#define POINT_BUFFER_SIZE 10
#define MAXLEV 4
#define MAXKINDS  10

struct state {
  Display *dpy;
  Window window;

  double f[2][3][MAXLEV];	/* three non-homogeneous transforms */
  int max_total;
  int max_levels;
  int max_points;
  int cur_level;
  int variation;
  int snum;
  int anum;
  int num_points;
  int total_points;
  int pixcol;
  int ncolors;
  XColor *colors;
  XPoint points [POINT_BUFFER_SIZE];
  GC gc;

  int delay, delay2;
  int width, height;

  short lasthalf;

  int flame_alt;
  int do_reset;
};


static short
halfrandom (struct state *st, int mv)
{
  unsigned long r;

  if (st->lasthalf)
    {
      r = st->lasthalf;
      st->lasthalf = 0;
    }
  else
    {
      r = random ();
      st->lasthalf = r >> 16;
    }
  return (r % mv);
}

static void *
flame_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;
  XWindowAttributes xgwa;
  Colormap cmap;

  st->dpy = dpy;
  st->window = window;

#if defined(SIGFPE) && defined(SIG_IGN)
  /* No doubt a better fix would be to track down where the NaN is coming
     from, and code around that; but this should do.  Apparently most systems
     (Linux, Solaris, Irix, ...) ignore FPE by default -- but FreeBSD dumps
     core by default. */
  signal (SIGFPE, SIG_IGN);
#endif

  XGetWindowAttributes (st->dpy, st->window, &xgwa);
  st->width = xgwa.width;
  st->height = xgwa.height;
  cmap = xgwa.colormap;

  st->max_points = get_integer_resource (st->dpy, "iterations", "Integer");
  if (st->max_points <= 0) st->max_points = 100;

  st->max_levels = st->max_points;

  st->max_total = get_integer_resource (st->dpy, "points", "Integer");
  if (st->max_total <= 0) st->max_total = 10000;

  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  if (st->delay < 0) st->delay = 0;
  st->delay2 = get_integer_resource (st->dpy, "delay2", "Integer");
  if (st->delay2 < 0) st->delay2 = 0;

  st->variation = random() % MAXKINDS;

  if (mono_p)
    st->ncolors = 0;
  else
    {
      st->ncolors = get_integer_resource (st->dpy, "colors", "Integer");
      if (st->ncolors <= 0) st->ncolors = 128;
      st->colors = (XColor *) malloc ((st->ncolors+1) * sizeof (*st->colors));
      make_smooth_colormap (xgwa.screen, xgwa.visual, xgwa.colormap,
                            st->colors, &st->ncolors,
			    True, 0, True);
      if (st->ncolors <= 2)
	mono_p = True, st->ncolors = 0;
    }

  gcv.foreground = get_pixel_resource (st->dpy, cmap, "foreground", "Foreground");
  gcv.background = get_pixel_resource (st->dpy, cmap, "background", "Background");

  if (! mono_p)
    {
      st->pixcol = halfrandom (st, st->ncolors);
      gcv.foreground = (st->colors [st->pixcol].pixel);
    }

  st->gc = XCreateGC (st->dpy, st->window, GCForeground | GCBackground, &gcv);
  return st;
}

static int
recurse (struct state *st, double x, double y, int l, Display *dpy, Window win)
{
  int i;
  double nx, ny;

  if (l == st->max_levels)
    {
      st->total_points++;
      if (st->total_points > st->max_total) /* how long each fractal runs */
	return 0;

      if (x > -1.0 && x < 1.0 && y > -1.0 && y < 1.0)
	{
	  st->points[st->num_points].x = (int) ((st->width / 2) * (x + 1.0));
	  st->points[st->num_points].y = (int) ((st->height / 2) * (y + 1.0));
	  st->num_points++;
	  if (st->num_points >= POINT_BUFFER_SIZE)
	    {
	      XDrawPoints (st->dpy, win, st->gc, st->points, st->num_points, CoordModeOrigin);
	      st->num_points = 0;
	    }
	}
    }
  else
    {
      for (i = 0; i < st->snum; i++)
	{

	  /* Scale back when values get very large. Spot sez:
	     "I think this happens on HPUX.  I think it's non-IEEE
	     to generate an exception instead of a silent NaN."
	   */
	  if ((fabs(x) > 1.0E5) || (fabs(y) > 1.0E5))
	    x = x / y;

	  nx = st->f[0][0][i] * x + st->f[0][1][i] * y + st->f[0][2][i];
	  ny = st->f[1][0][i] * x + st->f[1][1][i] * y + st->f[1][2][i];
	  if (i < st->anum)
	    {
	      switch (st->variation)
		{
		case 0:	/* sinusoidal */
		  nx = sin(nx);
		  ny = sin(ny);
		  break;
		case 1:	/* complex */
		  {
		    double r2 = nx * nx + ny * ny + 1e-6;
		    nx = nx / r2;
		    ny = ny / r2;
		  }
		  break;
		case 2:	/* bent */
		  if (nx < 0.0)
		    nx = nx * 2.0;
		  if (ny < 0.0)
		    ny = ny / 2.0;
		  break;
		case 3:	/* swirl */
		  {
		    double r = (nx * nx + ny * ny);	/* times k here is fun */
		    double c1 = sin(r);
		    double c2 = cos(r);
		    double t = nx;

		    if (nx > 1e4 || nx < -1e4 || ny > 1e4 || ny < -1e4)
		      ny = 1e4;
		    else
		      ny = c2 * t + c1 * ny;
		    nx = c1 * nx - c2 * ny;
		  }
		  break;
		case 4:	/* horseshoe */
		  {
		    double r, c1, c2, t;

		    /* Avoid atan2: DOMAIN error message */
		    if (nx == 0.0 && ny == 0.0)
		      r = 0.0;
		    else
		      r = atan2(nx, ny);      /* times k here is fun */
		    c1 = sin(r);
		    c2 = cos(r);
		    t = nx;

		    nx = c1 * nx - c2 * ny;
		    ny = c2 * t + c1 * ny;
		  }
		  break;
		case 5:	/* drape */
		  {
		    double t;

		    /* Avoid atan2: DOMAIN error message */
		    if (nx == 0.0 && ny == 0.0)
		      t = 0.0;
		    else
		      t = atan2(nx, ny) / M_PI;

		    if (nx > 1e4 || nx < -1e4 || ny > 1e4 || ny < -1e4)
		      ny = 1e4;
		    else
		      ny = sqrt(nx * nx + ny * ny) - 1.0;
		    nx = t;
		  }
		  break;
		case 6:	/* broken */
		  if (nx > 1.0)
		    nx = nx - 1.0;
		  if (nx < -1.0)
		    nx = nx + 1.0;
		  if (ny > 1.0)
		    ny = ny - 1.0;
		  if (ny < -1.0)
		    ny = ny + 1.0;
		  break;
		case 7:	/* spherical */
		  {
		    double r = 0.5 + sqrt(nx * nx + ny * ny + 1e-6);

		    nx = nx / r;
		    ny = ny / r;
		  }
		  break;
		case 8:	/*  */
		  nx = atan(nx) / M_PI_2;
		  ny = atan(ny) / M_PI_2;
		  break;
/* #if 0 */  /* core dumps on some machines, why not all? */
		case 9:	/* complex sine */
		  {
		    double u = nx;
		    double v = ny;
		    double ev = exp(v);
		    double emv = exp(-v);

		    nx = (ev + emv) * sin(u) / 2.0;
		    ny = (ev - emv) * cos(u) / 2.0;
		  }
		  break;
		case 10:	/* polynomial */
		  if (nx < 0)
		    nx = -nx * nx;
		  else
		    nx = nx * nx;
		  if (ny < 0)
		    ny = -ny * ny;
		  else
		    ny = ny * ny;
		  break;
/* #endif */
		default:
		  nx = sin(nx);
		  ny = sin(ny);
		}
	    }
	  if (!recurse (st, nx, ny, l + 1, st->dpy, win))
	    return 0;
	}
    }
  return 1;
}

static unsigned long
flame_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int i, j, k;
  unsigned long this_delay = st->delay;

  if (st->do_reset)
    {
      st->do_reset = 0;
      XClearWindow (st->dpy, st->window);
    }

  if (!(st->cur_level++ % st->max_levels))
    {
      st->do_reset = 1;
      this_delay = st->delay2;
      st->flame_alt = !st->flame_alt;
      st->variation = random() % MAXKINDS;
    }
  else
    {
      if (st->ncolors > 2)
	{
	  XSetForeground (st->dpy, st->gc, st->colors [st->pixcol].pixel);
	  if (--st->pixcol < 0)
	    st->pixcol = st->ncolors - 1;
	}
    }

  /* number of functions */
  st->snum = 2 + (st->cur_level % (MAXLEV - 1));

  /* how many of them are of alternate form */
  if (st->flame_alt)
    st->anum = 0;
  else
    st->anum = halfrandom (st, st->snum) + 2;

  /* 6 coefs per function */
  for (k = 0; k < st->snum; k++)
    {
      for (i = 0; i < 2; i++)
	for (j = 0; j < 3; j++)
	  st->f[i][j][k] = ((double) (random() & 1023) / 512.0 - 1.0);
    }
  st->num_points = 0;
  st->total_points = 0;
  recurse (st, 0.0, 0.0, 0, st->dpy, st->window);
  XDrawPoints (st->dpy, st->window, st->gc, st->points, st->num_points, CoordModeOrigin);

  return this_delay;
}


#if defined(__hpux) && defined(PLOSS)
/* I don't understand why this is necessary, but I'm told that this program
   does nothing at all on HP-sUX without it.

   I'm further told that HPUX 11.0 doesn't define PLOSS, and works ok without
   this section.  Go figure.
 */
#undef random
#undef srandom
#include <math.h>
int matherr(x)
   register struct exception *x;
{
  if (x->type == PLOSS) return 1;
  else return 0;
}
#endif /* __hpux */



static const char *flame_defaults [] = {
  ".background:	black",
  ".foreground:	white",
  "*fpsSolid:	true",
  "*colors:	64",
  "*iterations:	25",
  "*delay:	50000",
  "*delay2:	2000000",
  "*points:	10000",
#ifdef HAVE_MOBILE
  "*ignoreRotation: True",
#endif
  0
};

static XrmOptionDescRec flame_options [] = {
  { "-colors",		".colors",	XrmoptionSepArg, 0 },
  { "-iterations",	".iterations",	XrmoptionSepArg, 0 },
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-delay2",		".delay2",	XrmoptionSepArg, 0 },
  { "-points",		".points",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

static void
flame_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  st->width = w;
  st->height = h;
}

static Bool
flame_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (screenhack_event_helper (dpy, window, event))
    {
      st->do_reset = 1;
      return True;
    }
  return False;
}

static void
flame_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  XFreeGC (dpy, st->gc);
  free (st->colors);
  free (st);
}

XSCREENSAVER_MODULE ("Flame", flame)

