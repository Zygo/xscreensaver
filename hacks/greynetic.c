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

#include "screenhack.h"

#define NBITS 12

#include <X11/bitmaps/stipple>
#include <X11/bitmaps/cross_weave>
#include <X11/bitmaps/dimple1>
#include <X11/bitmaps/dimple3>
#include <X11/bitmaps/flipped_gray>
#include <X11/bitmaps/gray1>
#include <X11/bitmaps/gray3>
#include <X11/bitmaps/hlines2>
#include <X11/bitmaps/light_gray>
#include <X11/bitmaps/root_weave>
#include <X11/bitmaps/vlines2>
#include <X11/bitmaps/vlines3>

static Pixmap pixmaps [NBITS];
static GC gc;
static int delay;
static unsigned long fg, bg, pixels [512];
static int npixels;

static void
init_greynetic (dpy, window)
     Display *dpy;
     Window window;
{
  int i;
  XGCValues gcv;
  XWindowAttributes xgwa;
  Colormap cmap;
  XGetWindowAttributes (dpy, window, &xgwa);
  cmap = xgwa.colormap;
  npixels = 0;
  gcv.foreground= fg= get_pixel_resource("foreground","Foreground", dpy, cmap);
  gcv.background= bg= get_pixel_resource("background","Background", dpy, cmap);
  gcv.fill_style= FillOpaqueStippled;
  gc = XCreateGC (dpy, window, GCForeground|GCBackground|GCFillStyle, &gcv);

  delay = get_integer_resource ("delay", "Integer");
  if (delay < 0) delay = 0;

  i = 0;
#define BITS(n,w,h) \
  pixmaps [i++] = XCreatePixmapFromBitmapData (dpy, window, n, w, h, 1, 0, 1)

  BITS (stipple_bits, stipple_width, stipple_height);
  BITS (cross_weave_bits, cross_weave_width, cross_weave_height);
  BITS (dimple1_bits, dimple1_width, dimple1_height);
  BITS (dimple3_bits, dimple3_width, dimple3_height);
  BITS (flipped_gray_bits, flipped_gray_width, flipped_gray_height);
  BITS (gray1_bits, gray1_width, gray1_height);
  BITS (gray3_bits, gray3_width, gray3_height);
  BITS (hlines2_bits, hlines2_width, hlines2_height);
  BITS (light_gray_bits, light_gray_width, light_gray_height);
  BITS (root_weave_bits, root_weave_width, root_weave_height);
  BITS (vlines2_bits, vlines2_width, vlines2_height);
  BITS (vlines3_bits, vlines3_width, vlines3_height);
}

static void
greynetic (dpy, window)
     Display *dpy;
     Window window;
{
  static int tick = 500, xlim, ylim;
  static Colormap cmap;
  int x, y, w, h, i;
  XGCValues gcv;
  if (tick++ == 500)
    {
      XWindowAttributes xgwa;
      XGetWindowAttributes (dpy, window, &xgwa);
      tick = 0;
      xlim = xgwa.width;
      ylim = xgwa.height;
      cmap = xgwa.colormap;
    }
  for (i = 0; i < 10; i++) /* minimize area, but don't try too hard */
    {
      w = 50 + random () % (xlim - 50);
      h = 50 + random () % (ylim - 50);
      if (w + h < xlim && w + h < ylim)
	break;
    }
  x = random () % (xlim - w);
  y = random () % (ylim - h);
  gcv.stipple = pixmaps [random () % NBITS];
  if (mono_p)
    {
      if (random () % 1)
	gcv.foreground = fg, gcv.background = bg;
      else
	gcv.foreground = bg, gcv.background = fg;
    }
  else
    {
      XColor fgc, bgc;
      if (npixels == sizeof (pixels) / sizeof (unsigned long))
	goto REUSE;
      fgc.flags = bgc.flags = DoRed|DoGreen|DoBlue;
      fgc.red = random ();
      fgc.green = random ();
      fgc.blue = random ();
      bgc.red = random ();
      bgc.green = random ();
      bgc.blue = random ();
      if (! XAllocColor (dpy, cmap, &fgc))
	goto REUSE;
      pixels [npixels++] = fgc.pixel;
      gcv.foreground = fgc.pixel;
      if (! XAllocColor (dpy, cmap, &bgc))
	goto REUSE;
      pixels [npixels++] = bgc.pixel;
      gcv.background = bgc.pixel;
      goto DONE;
    REUSE:
      gcv.foreground = pixels [random () % npixels];
      gcv.background = pixels [random () % npixels];
    DONE:
      ;
    }
  XChangeGC (dpy, gc, GCStipple|GCForeground|GCBackground, &gcv);
  XFillRectangle (dpy, window, gc, x, y, w, h);
  XSync (dpy, True);
}


char *progclass = "Greynetic";

char *defaults [] = {
  "Greynetic.background:	black",		/* to placate SGI */
  "Greynetic.foreground:	white",
  "*delay:	0",
  0
};

XrmOptionDescRec options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 }
};
int options_size = (sizeof (options) / sizeof (options[0]));

void
screenhack (dpy, window)
     Display *dpy;
     Window window;
{
  init_greynetic (dpy, window);
  while (1)
    {
      greynetic (dpy, window);
      if (delay) usleep (delay);
    }
}
