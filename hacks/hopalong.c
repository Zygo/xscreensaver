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

/* This file was ported from xlock for use in xscreensaver (and standalone)
 * by jwz on 12-Aug-92.  Original copyright reads:
 *
 * hopalong.c - Real Plane Fractals for xlock, the X Window System lockscreen.
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
 * 29-Oct-90: fix bad (int) cast.
 * 29-Jul-90: support for multiple screens.
 * 08-Jul-90: new timing and colors and new algorithm for fractals.
 * 15-Dec-89: Fix for proper skipping of {White,Black}Pixel() in colors.
 * 08-Oct-89: Fixed long standing typo bug in RandomInitHop();
 *	      Fixed bug in memory allocation in inithop();
 *	      Moved seconds() to an extern.
 *	      Got rid of the % mod since .mod is slow on a sparc.
 * 20-Sep-89: Lint.
 * 31-Aug-88: Forked from xlock.c for modularity.
 * 23-Mar-88: Coded HOPALONG routines from Scientific American Sept. 86 p. 14.
 */

#include "screenhack.h"
#include <math.h>

static GC gc;
static int batchcount = 1000;

static unsigned int *pixels = 0, fg_pixel, bg_pixel;
static int npixels;
static unsigned int delay;
static int timeout;

typedef struct {
    int         centerx;
    int         centery;	/* center of the screen */
    double      a;
    double      b;
    double      c;
    double      i;
    double      j;		/* hopalong parameters */
    int         inc;
    int         pix;
    long        startTime;
}           hopstruct;

static hopstruct hop;
static XPoint *pointBuffer = 0;	/* pointer for XDrawPoints */

static void
inithop(dsp,win)
     Display *dsp;
    Window      win;
{
    double      range;
    XWindowAttributes xgwa;
    hopstruct  *hp = &hop;
    XGCValues gcv;
    Colormap cmap;
    XGetWindowAttributes (dsp, win, &xgwa);
    cmap = xgwa.colormap;

    if (! pixels)
      {
	XColor color;
	int i = get_integer_resource ("ncolors", "Integer");
	int shift;
	if (i <= 2) i = 2, mono_p = True;
	shift = 360 / i;
	pixels = (unsigned int *) calloc (i, sizeof (unsigned int));
	fg_pixel = get_pixel_resource ("foreground", "Foreground", dsp, cmap);
	bg_pixel = get_pixel_resource ("background", "Background", dsp, cmap);
	if (! mono_p)
	  {
	    hsv_to_rgb (random () % 360, 1.0, 1.0, 
			&color.red, &color.green, &color.blue);
	    for (npixels = 0; npixels < i; npixels++)
	      {
		if (! XAllocColor (dsp, cmap, &color))
		  break;
		pixels[npixels] = color.pixel;
		cycle_hue (&color, shift);
	      }
	  }
	timeout = get_integer_resource ("timeout", "Seconds");
	if (timeout <= 0) timeout = 30;
	delay = get_integer_resource ("delay", "Usecs");

	gcv.foreground = fg_pixel;
	gc = XCreateGC (dsp, win, GCForeground, &gcv);
      }

    XClearWindow (dsp, win);

    hp->centerx = xgwa.width / 2;
    hp->centery = xgwa.height / 2;
    range = sqrt((double) hp->centerx * hp->centerx +
		 (double) hp->centery * hp->centery) /
	(10.0 + random() % 10);

    hp->pix = 0;
#define frand0() (((double) random()) / ((unsigned int) (~0)))
    hp->inc = (int) (frand0() * 200) - 100;
    hp->a = frand0() * range - range / 2.0;
    hp->b = frand0() * range - range / 2.0;
    hp->c = frand0() * range - range / 2.0;
    if (!(random() % 2))
	hp->c = 0.0;

    hp->i = hp->j = 0.0;

    if (!pointBuffer)
	pointBuffer = (XPoint *) malloc(batchcount * sizeof(XPoint));

    XSetForeground(dsp, gc, bg_pixel);
    XFillRectangle(dsp, win, gc, 0, 0,
		   hp->centerx * 2, hp->centery * 2);
    XSetForeground(dsp, gc, fg_pixel);
    hp->startTime = time ((time_t *) 0);
}


static void
drawhop(dsp,win)
     Display *dsp;
    Window      win;
{
    double      oldj;
    int         k = batchcount;
    XPoint     *xp = pointBuffer;
    hopstruct  *hp = &hop;

    hp->inc++;
    if (! mono_p) {
	XSetForeground(dsp, gc, pixels[hp->pix]);
	if (++hp->pix >= npixels)
	    hp->pix = 0;
    }
    while (k--) {
	oldj = hp->j;
	hp->j = hp->a - hp->i;
	hp->i = oldj + (hp->i < 0
			? sqrt(fabs(hp->b * (hp->i + hp->inc) - hp->c))
			: -sqrt(fabs(hp->b * (hp->i + hp->inc) - hp->c)));
	xp->x = hp->centerx + (int) (hp->i + hp->j);
	xp->y = hp->centery - (int) (hp->i - hp->j);
	xp++;
    }
    XDrawPoints(dsp, win, gc,
		pointBuffer, batchcount, CoordModeOrigin);
    XSync (dsp, True);
    if ((time ((time_t *) 0) - hp->startTime) > timeout)
      {
	int i;
	XSetForeground(dsp, gc, bg_pixel);
	for (i = 0; i < hp->centery; i++)
	  {
	    int y = (random () % (hp->centery << 1));
	    XDrawLine (dsp, win, gc, 0, y, hp->centerx << 1, y);
	    XFlush (dsp);
	    if ((i % 50) == 0)
	      usleep (10000);
	  }
	XClearWindow (dsp, win);
	XFlush (dsp);
	sleep (1);
	inithop(dsp,win);
      }
}


char *progclass = "Hopalong";

char *defaults [] = {
  "Hopalong.background:	black",		/* to placate SGI */
  "Hopalong.foreground:	white",
  "*count:	1000",
  "*ncolors:	100",
  "*timeout:	20",
  "*delay:	0",
  0
};

XrmOptionDescRec options [] = {
  { "-count",		".count",	XrmoptionSepArg, 0 },
  { "-ncolors",		".ncolors",	XrmoptionSepArg, 0 },
  { "-timeout",		".timeout",	XrmoptionSepArg, 0 },
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
};
int options_size = (sizeof (options) / sizeof (options[0]));

void
screenhack (dpy, window)
     Display *dpy;
     Window window;
{
  inithop (dpy, window);
  while (1)
    {
      drawhop (dpy, window);
      XSync (dpy, True);
      if (delay) usleep (delay);
    }
}
