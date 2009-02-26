/* xscreensaver, Copyright (c) 1997 Jamie Zawinski <jwz@netscape.com>
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
#include <X11/Xutil.h>
#include <stdio.h>

static int ncolors;
static XColor *colors = 0;
static int fg_pixel, bg_pixel;
static Pixmap p0 = 0, p1 = 0, p2 = 0, p3 = 0;
static GC copy_gc = 0, erase_gc = 0, window_gc = 0;
static int width, height, size;
static int x1, x2, y1, y2, x3, y3;
static int dx1, dx2, dx3, dy1, dy2, dy3;
static int othickness, thickness;
static Bool do_three;

static void
init_moire2 (Display *dpy, Window window)
{
  XWindowAttributes xgwa;
  XGetWindowAttributes (dpy, window, &xgwa);

  othickness = get_integer_resource("thickness", "Thickness");

  if (mono_p)
    ncolors = 2;
  else
    ncolors = get_integer_resource ("colors", "Colors");
  if (ncolors < 2) ncolors = 2;
  if (ncolors <= 2) mono_p = True;

  if (mono_p)
    colors = 0;
  else
    colors = (XColor *) malloc(sizeof(*colors) * (ncolors+1));

  if (mono_p)
    ;
  else
    make_smooth_colormap (dpy, xgwa.visual, xgwa.colormap, colors, &ncolors,
			  True, 0, True);

  bg_pixel = get_pixel_resource("background", "Background", dpy,
				xgwa.colormap);
  fg_pixel = get_pixel_resource("foreground", "Foreground", dpy,
				xgwa.colormap);
}


static void
reset_moire2 (Display *dpy, Window window)
{
  GC gc;
  XWindowAttributes xgwa;
  XGCValues gcv;
  Bool xor;
  XGetWindowAttributes (dpy, window, &xgwa);

  do_three = (0 == (random() % 3));

  width = xgwa.width;
  height = xgwa.height;
  size = width > height ? width : height;

  if (p0) XFreePixmap(dpy, p0);
  if (p1) XFreePixmap(dpy, p1);
  if (p2) XFreePixmap(dpy, p2);
  if (p3) XFreePixmap(dpy, p3);

  p0 = XCreatePixmap(dpy, window, width, height, 1);
  p1 = XCreatePixmap(dpy, window, width*2, height*2, 1);
  p2 = XCreatePixmap(dpy, window, width*2, height*2, 1);
  if (do_three)
    p3 = XCreatePixmap(dpy, window, width*2, height*2, 1);
  else
    p3 = 0;

  thickness = (othickness > 0 ? othickness : (1 + (random() % 4)));

  gcv.foreground = 0;
  gcv.line_width = (thickness == 1 ? 0 : thickness);
  gc = XCreateGC (dpy, p1, GCForeground|GCLineWidth, &gcv);

  XFillRectangle(dpy, p1, gc, 0, 0, width*2, height*2);
  XFillRectangle(dpy, p2, gc, 0, 0, width*2, height*2);
  if (do_three)
    XFillRectangle(dpy, p3, gc, 0, 0, width*2, height*2);

  XSetForeground(dpy, gc, 1);

  xor = (do_three || (thickness == 1) || (random() & 1));

  {
    int i, ii, maxx, maxy;

#define FROB(P) do { \
    maxx = (size*4); \
    maxy = (size*4); \
    if (0 == (random() % 5)) { \
	float f = 1.0 + frand(0.05); \
	if (random() & 1) maxx *= f; \
	else maxy *= f; \
      } \
    ii = (thickness + 1 + (xor ? 0 : 1) + (random() % (4 * thickness)));  \
    for (i = 0; i < (size*2); i += ii)  \
      XDrawArc(dpy, (P), gc, i-size, i-size, maxx-i-i, maxy-i-i, 0, 360*64); \
    if (0 == (random() % 5)) \
      { \
	XSetFunction(dpy, gc, GXxor); \
	XFillRectangle(dpy, (P), gc, 0, 0, width*2, height*2); \
	XSetFunction(dpy, gc, GXcopy); \
      } \
    } while(0)

    FROB(p1);
    FROB(p2);
    if (do_three)
      FROB(p3);
#undef FROB
  }

  XFreeGC(dpy, gc);

  if (copy_gc) XFreeGC(dpy, copy_gc);
  gcv.function = (xor ? GXxor : GXor);
  gcv.foreground = 1;
  gcv.background = 0;

  copy_gc = XCreateGC (dpy, p0, GCFunction|GCForeground|GCBackground, &gcv);

  gcv.foreground = 0;
  if (erase_gc) XFreeGC(dpy, erase_gc);
  erase_gc = XCreateGC (dpy, p0, GCForeground, &gcv);

  gcv.foreground = fg_pixel;
  gcv.background = bg_pixel;
  if (window_gc) XFreeGC(dpy, window_gc);
  window_gc = XCreateGC (dpy, window, GCForeground|GCBackground, &gcv);

#define FROB(N,DN,MAX) \
  N = (MAX/2) + (random() % MAX); \
  DN = ((1 + (random() % (7*thickness))) * ((random() & 1) ? 1 : -1))

  FROB(x1,dx1,width);
  FROB(x2,dx2,width);
  FROB(x3,dx3,width);
  FROB(y1,dy1,height);
  FROB(y2,dy2,height);
  FROB(y3,dy3,height);
#undef FROB
}



static void
moire2 (Display *dpy, Window window)
{
#define FROB(N,DN,MAX) \
  N += DN; \
  if (N <= 0) N = 0, DN = -DN; \
  else if (N >= MAX) N = MAX, DN = -DN; \
  else if (0 == (random() % 100)) DN = -DN; \
  else if (0 == (random() % 50)) \
    DN += (DN <= -20 ? 1 : (DN >= 20 ? -1 : ((random() & 1) ? 1 : -1)))

  FROB(x1,dx1,width);
  FROB(x2,dx2,width);
  FROB(x3,dx3,width);
  FROB(y1,dy1,height);
  FROB(y2,dy2,height);
  FROB(y3,dy3,height);
#undef FROB

  XFillRectangle(dpy, p0, erase_gc, 0, 0, width, height);
  XCopyArea(dpy, p1, p0, copy_gc, x1, y1, width, height, 0, 0);
  XCopyArea(dpy, p2, p0, copy_gc, x2, y2, width, height, 0, 0);
  if (do_three)
    XCopyArea(dpy, p3, p0, copy_gc, x3, y3, width, height, 0, 0);

  XSync(dpy, False);
  XCopyPlane(dpy, p0, window, window_gc, 0, 0, width, height, 0, 0, 1L);
  XSync(dpy, False);

#if 0
  XCopyPlane(dpy, p1, window, window_gc, (width*2)/3, (height*2)/3,
	     width/2, height/2,
	     0, height/2, 1L);
  XCopyPlane(dpy, p2, window, window_gc, (width*2)/3, (height*2)/3,
	     width/2, height/2,
	     width/2, height/2, 1L);
#endif
}




char *progclass = "Moire2";

char *defaults [] = {
  ".background:		black",
  ".foreground:		white",
  "*delay:		50000",
  "*thickness:		0",
  "*colors:		150",
  "*colorShift:		5",
  0
};

XrmOptionDescRec options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-ncolors",		".colors",	XrmoptionSepArg, 0 },
  { "-thickness",	".thickness",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

void
screenhack (Display *dpy, Window window)
{
  int delay = get_integer_resource ("delay", "Integer");
  int color_shift = get_integer_resource ("colorShift", "Integer");
  int pix = 0;
  Bool flip_a, flip_b;

  if (color_shift <= 0) color_shift = 1;
  init_moire2 (dpy, window);
  while (1)
    {
      int iterations = 30 + (random() % 70) + (random() % 70);
      reset_moire2 (dpy, window);

      flip_a = mono_p ? False : (random() & 1);
      flip_b = mono_p ? False : (random() & 1);

      if (flip_b)
	{
	  XSetForeground(dpy, window_gc, bg_pixel);
	  XSetBackground(dpy, window_gc, fg_pixel);
	}
      else
	{
	  XSetForeground(dpy, window_gc, fg_pixel);
	  XSetBackground(dpy, window_gc, bg_pixel);
	}

      while (--iterations > 0)
	{
	  int i;

	  if (!mono_p)
	    {
	      pix++;
	      pix = pix % ncolors;

	      if (flip_a)
		XSetBackground(dpy, window_gc, colors[pix].pixel);
	      else
		XSetForeground(dpy, window_gc, colors[pix].pixel);
	    }

	  for (i = 0; i < color_shift; i++)
	    {
	      moire2 (dpy, window);
	      if (delay)
		usleep(delay);
	    }
	}
    }
}
