/* xscreensaver, Copyright (c) 1993, 1995 Jamie Zawinski <jwz@netscape.com>
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
 * by jwz on 18-Oct-93.  Original copyright reads:
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
 * 27-Jun-91: vary number of functions used.
 * 24-Jun-91: fixed portability problem with integer mod (%).
 * 06-Jun-91: Written. (received from Scott Graves, spot@cs.cmu.edu).
 */

#include "screenhack.h"

#define POINT_BUFFER_SIZE 10
#define MAXLEV 4

static double f[2][3][MAXLEV];	/* three non-homogeneous transforms */
static int max_total;
static int max_levels;
static int max_points;
static int cur_level;
static int snum;
static int anum;
static int num_points;
static int total_points;
static int pixcol;
static int npixels;
static unsigned long *pixels;
static XPoint points [POINT_BUFFER_SIZE];
static GC gc;

static int delay, delay2;
static int width, height;

static short
halfrandom (mv)
     int mv;
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
init_flame (dpy, window)
     Display *dpy;
     Window window;
{
  XGCValues gcv;
  XWindowAttributes xgwa;
  Colormap cmap;
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

  if (mono_p)
    npixels = 0;
  else
    {
      int i = get_integer_resource ("ncolors", "Integer");
      double saturation = 1.0;
      double value = 1.0;
      XColor color;
      if (i <= 0) i = 128;

      pixels = (unsigned long *) malloc ((i+1) * sizeof (*pixels));
      for (npixels = 0; npixels < i; npixels++)
	{
	  hsv_to_rgb ((360*npixels)/i, saturation, value,
		      &color.red, &color.green, &color.blue);
	  if (! XAllocColor (dpy, cmap, &color))
	    break;
	  pixels [npixels] = color.pixel;
	}
    }

  gcv.foreground = get_pixel_resource ("foreground", "Foreground", dpy, cmap);
  gcv.background = get_pixel_resource ("background", "Background", dpy, cmap);

  if (! mono_p)
    {
      pixcol = halfrandom (npixels);
      gcv.foreground = (pixels [pixcol]);
    }

  gc = XCreateGC (dpy, window, GCForeground | GCBackground, &gcv);
}

static int
recurse (x, y, l, dpy, win)
     register double x, y;
     register int l;
     Display *dpy;
     Window win;
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
	      /* XSync (dpy, True); */
	    }
	}
    }
  else
    {
      for (i = 0; i < snum; i++)
	{
	  nx = f[0][0][i] * x + f[0][1][i] * y + f[0][2][i];
	  ny = f[1][0][i] * x + f[1][1][i] * y + f[1][2][i];
	  if (i < anum)
	    {
	      nx = sin(nx);
	      ny = sin(ny);
	    }
	  if (!recurse (nx, ny, l + 1, dpy, win))
	    return 0;
	}
    }
  return 1;
}


static void
flame (dpy, window)
     Display *dpy;
     Window window;
{
  int i, j, k;
  static int alt = 0;

  if (!(cur_level++ % max_levels))
    {
      if (delay2) usleep (delay2);
      XClearWindow (dpy, window);
      alt = !alt;
    }
  else
    {
      if (npixels > 2)
	{
	  XSetForeground (dpy, gc, pixels [pixcol]);
	  if (--pixcol < 0)
	    pixcol = npixels - 1;
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
  XSync (dpy, True);
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
  "Flame.background:	black",		/* to placate SGI */
  "Flame.foreground:	white",
  "*colors:	128",
  "*iterations:	25",
  "*delay:	50000",
  "*delay2:	2000000",
  "*points:	10000",
  0
};

XrmOptionDescRec options [] = {
  { "-ncolors",		".colors",	XrmoptionSepArg, 0 },
  { "-iterations",	".iterations",	XrmoptionSepArg, 0 },
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-delay2",		".delay2",	XrmoptionSepArg, 0 },
  { "-points",		".points",	XrmoptionSepArg, 0 }
};
int options_size = (sizeof (options) / sizeof (options[0]));

void
screenhack (dpy, window)
     Display *dpy;
     Window window;
{
  init_flame (dpy, window);
  while (1)
    flame (dpy, window);
}
