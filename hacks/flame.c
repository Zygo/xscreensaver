/* xscreensaver, Copyright (c) 1993, 1995, 1996, 1998
 *  Jamie Zawinski <jwz@jwz.org>
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

static double f[2][3][MAXLEV];	/* three non-homogeneous transforms */
static int max_total;
static int max_levels;
static int max_points;
static int cur_level;
static int variation;
static int snum;
static int anum;
static int num_points;
static int total_points;
static int pixcol;
static int ncolors;
static XColor *colors;
static XPoint points [POINT_BUFFER_SIZE];
static GC gc;

static int delay, delay2;
static int width, height;

static short
halfrandom (int mv)
{
  static short lasthalf = 0;
  unsigned long r;

  if (lasthalf)
    {
      r = lasthalf;
      lasthalf = 0;
    }
  else
    {
      r = random ();
      lasthalf = r >> 16;
    }
  return (r % mv);
}

static void
init_flame (Display *dpy, Window window)
{
  XGCValues gcv;
  XWindowAttributes xgwa;
  Colormap cmap;

#if defined(SIGFPE) && defined(SIG_IGN)
  /* No doubt a better fix would be to track down where the NaN is coming
     from, and code around that; but this should do.  Apparently most systems
     (Linux, Solaris, Irix, ...) ignore FPE by default -- but FreeBSD dumps
     core by default. */
  signal (SIGFPE, SIG_IGN);
#endif

  XGetWindowAttributes (dpy, window, &xgwa);
  width = xgwa.width;
  height = xgwa.height;
  cmap = xgwa.colormap;

  max_points = get_integer_resource ("iterations", "Integer");
  if (max_points <= 0) max_points = 100;

  max_levels = max_points;

  max_total = get_integer_resource ("points", "Integer");
  if (max_total <= 0) max_total = 10000;

  delay = get_integer_resource ("delay", "Integer");
  if (delay < 0) delay = 0;
  delay2 = get_integer_resource ("delay2", "Integer");
  if (delay2 < 0) delay2 = 0;

  variation = random() % MAXKINDS;

  if (mono_p)
    ncolors = 0;
  else
    {
      ncolors = get_integer_resource ("colors", "Integer");
      if (ncolors <= 0) ncolors = 128;
      colors = (XColor *) malloc ((ncolors+1) * sizeof (*colors));
      make_smooth_colormap (dpy, xgwa.visual, xgwa.colormap, colors, &ncolors,
			    True, 0, True);
      if (ncolors <= 2)
	mono_p = True, ncolors = 0;
    }

  gcv.foreground = get_pixel_resource ("foreground", "Foreground", dpy, cmap);
  gcv.background = get_pixel_resource ("background", "Background", dpy, cmap);

  if (! mono_p)
    {
      pixcol = halfrandom (ncolors);
      gcv.foreground = (colors [pixcol].pixel);
    }

  gc = XCreateGC (dpy, window, GCForeground | GCBackground, &gcv);
}

static int
recurse (double x, double y, int l, Display *dpy, Window win)
{
  int xp, yp, i;
  double nx, ny;

  if (l == max_levels)
    {
      total_points++;
      if (total_points > max_total) /* how long each fractal runs */
	return 0;

      if (x > -1.0 && x < 1.0 && y > -1.0 && y < 1.0)
	{
	  xp = points[num_points].x = (int) ((width / 2) * (x + 1.0));
	  yp = points[num_points].y = (int) ((height / 2) * (y + 1.0));
	  num_points++;
	  if (num_points >= POINT_BUFFER_SIZE)
	    {
	      XDrawPoints (dpy, win, gc, points, num_points, CoordModeOrigin);
	      num_points = 0;
	      /* if (delay) usleep (delay); */
	      /* XSync (dpy, False); */
	    }
	}
    }
  else
    {
      for (i = 0; i < snum; i++)
	{

	  /* Scale back when values get very large. Spot sez:
	     "I think this happens on HPUX.  I think it's non-IEEE
	     to generate an exception instead of a silent NaN."
	   */
	  if ((abs(x) > 1.0E5) || (abs(y) > 1.0E5))
	    x = x / y;

	  nx = f[0][0][i] * x + f[0][1][i] * y + f[0][2][i];
	  ny = f[1][0][i] * x + f[1][1][i] * y + f[1][2][i];
	  if (i < anum)
	    {
	      switch (variation)
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
	  if (!recurse (nx, ny, l + 1, dpy, win))
	    return 0;
	}
    }
  return 1;
}


static void
flame (Display *dpy, Window window)
{
  int i, j, k;
  static int alt = 0;

  if (!(cur_level++ % max_levels))
    {
      if (delay2) usleep (delay2);
      XClearWindow (dpy, window);
      alt = !alt;

      variation = random() % MAXKINDS;
    }
  else
    {
      if (ncolors > 2)
	{
	  XSetForeground (dpy, gc, colors [pixcol].pixel);
	  if (--pixcol < 0)
	    pixcol = ncolors - 1;
	}
    }

  /* number of functions */
  snum = 2 + (cur_level % (MAXLEV - 1));

  /* how many of them are of alternate form */
  if (alt)
    anum = 0;
  else
    anum = halfrandom (snum) + 2;

  /* 6 coefs per function */
  for (k = 0; k < snum; k++)
    {
      for (i = 0; i < 2; i++)
	for (j = 0; j < 3; j++)
	  f[i][j][k] = ((double) (random() & 1023) / 512.0 - 1.0);
    }
  num_points = 0;
  total_points = 0;
  (void) recurse (0.0, 0.0, 0, dpy, window);
  XDrawPoints (dpy, window, gc, points, num_points, CoordModeOrigin);
  XSync (dpy, False);
  if (delay) usleep (delay);
}


#ifdef __hpux
/* I don't understand why this is necessary, but I'm told that this program
   does nothing at all on HP-sUX without it.
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



char *progclass = "Flame";

char *defaults [] = {
  ".background:	black",
  ".foreground:	white",
  "*colors:	64",
  "*iterations:	25",
  "*delay:	50000",
  "*delay2:	2000000",
  "*points:	10000",
  0
};

XrmOptionDescRec options [] = {
  { "-colors",		".colors",	XrmoptionSepArg, 0 },
  { "-iterations",	".iterations",	XrmoptionSepArg, 0 },
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-delay2",		".delay2",	XrmoptionSepArg, 0 },
  { "-points",		".points",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

void
screenhack (Display *dpy, Window window)
{
  init_flame (dpy, window);
  while (1)
    {
      flame (dpy, window);
      screenhack_handle_events (dpy);
    }
}
