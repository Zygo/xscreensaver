/*
 *  Copyright (C) 2000 James Macnicol
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
#include <X11/Xutil.h>
#include <sys/time.h>

#ifndef MIN
#define MIN(a, b) (((a) < (b))?(a):(b))
#endif

#ifndef MAX
#define MAX(a, b) (((a) > (b))?(a):(b))
#endif

#define MINX 0.0
#define MINY 0.0
/* This should be *way* slower than the spotlight hack was */
#define X_PERIOD 45000.0
#define Y_PERIOD 36000.0

static int sizex, sizey;

static int delay;
static int pixwidth, pixheight, pixspacex, pixspacey, lensoffsetx, lensoffsety;
static Bool lenses;

static GC window_gc;

static XImage *orig_map;
static Pixmap pm;

static int tlx, tly, s;

static long currentTimeInMs(void)
{ 
  struct timeval curTime;
#ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone tz = {0,0};
  gettimeofday(&curTime, &tz);
#else
  gettimeofday(&curTime);
#endif
  return curTime.tv_sec*1000 + curTime.tv_usec/1000.0;
}

static void init_hack(Display *dpy, Window window)
{
  XGCValues gcv;
  XWindowAttributes xgwa;
  Colormap cmap;
  unsigned long fg, bg;
  long gcflags;
  int nblocksx, nblocksy;

  XGetWindowAttributes(dpy, window, &xgwa);
  sizex = xgwa.width;
  sizey = xgwa.height;
  cmap = xgwa.colormap;
  fg = get_pixel_resource("foreground", "Foreground", dpy, cmap);
  bg = get_pixel_resource("background", "Background", dpy, cmap);

  delay = get_integer_resource("delay", "Integer");
  if (delay < 1)
    delay = 1;
  pixwidth = get_integer_resource("pixwidth", "Integer");
  if (pixwidth < 1)
    pixwidth = 1;
  pixheight = get_integer_resource("pixheight", "Integer");
  if (pixheight < 1)
    pixheight = 1;
  pixspacex = get_integer_resource("pixspacex", "Integer");
  if (pixspacex < 0)
    pixspacex = 0;
  pixspacey = get_integer_resource("pixspacey", "Integer");
  if (pixspacey < 0)
    pixspacey = 0;
  lenses = get_boolean_resource("lenses", "Boolean");
  lensoffsetx = get_integer_resource("lensoffsetx", "Integer");
  lensoffsetx = MAX(0, MIN(pixwidth, lensoffsetx));
  lensoffsety = get_integer_resource("lensoffsety", "Integer");
  lensoffsety = MAX(0, MIN(pixwidth, lensoffsety));

  gcv.function = GXcopy;
  gcv.subwindow_mode = IncludeInferiors;
  gcflags = GCForeground|GCFunction;
  gcv.foreground = bg;
  if (!lenses && use_subwindow_mode_p(xgwa.screen, window))       /* see grabscreen.c */
    gcflags |= GCSubwindowMode;
  window_gc = XCreateGC(dpy, window, gcflags, &gcv);


  orig_map = NULL;
  pm = XCreatePixmap(dpy, window, sizex, sizey, xgwa.depth);
  load_random_image (xgwa.screen, window, pm);

  if (!lenses) {
    orig_map = XGetImage(dpy, pm, 0, 0, sizex, sizey, ~0L, ZPixmap);
    XFreePixmap(dpy, pm);
    pm = 0;
  }

  /* We might have needed this to grab the image, but if we leave this set
     to GCSubwindowMode, then we'll *draw* right over subwindows too. */
  XSetSubwindowMode (dpy, window_gc, ClipByChildren);


  XFillRectangle(dpy, window, window_gc, 0, 0, sizex, sizey);
  XSetWindowBackground(dpy, window, bg);

  nblocksx = (int)ceil((double)sizex / (double)(pixwidth + pixspacex));
  nblocksy = (int)ceil((double)sizey / (double)(pixheight + pixspacey));
  if (lenses)
    s = MAX((nblocksx - 1) * lensoffsetx + pixwidth, 
	    (nblocksy - 1) * lensoffsety + pixheight) * 2;
  else
    s = MAX(nblocksx, nblocksy) * 2;
}

static void onestep(Display *dpy, Window window)
{
  unsigned x, y, i, j;

  long now;

#define nrnd(x) (random() % (x))

  now = currentTimeInMs();

  /* find new x,y */
  tlx = ((1. + sin(((float)now) / X_PERIOD * 2. * M_PI))/2.0)
    * (sizex - s/2) /* -s/4 */ + MINX;
  tly = ((1. + sin(((float)now) / Y_PERIOD * 2. * M_PI))/2.0)
    * (sizey - s/2) /* -s/4 */ + MINY;

  if (lenses) {
    for (x = i = 0; x < sizex; x += (pixwidth + pixspacex), ++i)
      for (y = j = 0; y < sizey; y += (pixheight + pixspacey), ++j) {
	XCopyArea(dpy, pm /* src */, window /* dest */, window_gc,
		  tlx + i * lensoffsetx /* src_x */, 
		  tly + j * lensoffsety /* src_y */,
		  pixwidth, pixheight,
		  x /* dest_x */, y /* dest_y */);
      }
  } else {
    for (x = i = 0; x < sizex; x += (pixwidth + pixspacex), ++i)
      for (y = j = 0; y < sizey; y += (pixheight + pixspacey), ++j) {
	XSetForeground(dpy, window_gc, XGetPixel(orig_map, tlx+i, tly+j));
	XFillRectangle(dpy, window, window_gc, 
		       i * (pixwidth + pixspacex),
		       j * (pixheight + pixspacey), pixwidth, pixheight);
      }
  }
}

char *progclass = "Zoom";

char *defaults[] = {
  "*dontClearRoot: True",
#ifdef __sgi /* really, HAVE_READ_DISPLAY_EXTENSION */
  "*visualID: Best",
#endif
  "*lenses:      false",
  "*delay:       10000",
  "*pixwidth:    10",
  "*pixheight:   10",
  "*pixspacex:   2",
  "*pixspacey:   2",
  "*lensoffsetx: 5",
  "*lensoffsety: 5",
  0
};

XrmOptionDescRec options[] = {
  { "-lenses", ".lenses", XrmoptionNoArg, "true" },
  { "-delay", ".delay", XrmoptionSepArg, 0 },
  { "-pixwidth", ".pixwidth", XrmoptionSepArg, 0 },
  { "-pixheight", ".pixheight", XrmoptionSepArg, 0 },
  { "-pixspacex", ".pixspacex", XrmoptionSepArg, 0 },
  { "-pixspacey", ".pixspacey", XrmoptionSepArg, 0 },
  { "-lensoffsetx", ".lensoffsetx", XrmoptionSepArg, 0 },
  { "-lensoffsety", ".lensoffsety", XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

void screenhack(Display *dpy, Window window)
{
  init_hack(dpy, window);
  while (1) {
    onestep(dpy, window);
    XSync(dpy, False);
    if (delay)
      usleep(delay);
    screenhack_handle_events(dpy);
  }
}
