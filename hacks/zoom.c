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

struct state {
  Display *dpy;
  Window window;
  Screen *screen;

  int sizex, sizey;

  int delay;
  int duration;
  int pixwidth, pixheight, pixspacex, pixspacey, lensoffsetx, lensoffsety;
  Bool lenses;

  GC window_gc;

  XImage *orig_map;
  Pixmap pm;

  int tlx, tly, s;

  int sinusoid_offset;

  time_t start_time;
  async_load_state *img_loader;
};


static long currentTimeInMs(struct state *st)
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

static void *
zoom_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;
  XWindowAttributes xgwa;
  Colormap cmap;
  unsigned long bg;
  long gcflags;
  int nblocksx, nblocksy;

  st->dpy = dpy;
  st->window = window;
  XGetWindowAttributes(st->dpy, st->window, &xgwa);
  st->screen = xgwa.screen;
  st->sizex = xgwa.width;
  st->sizey = xgwa.height;
  cmap = xgwa.colormap;
  bg = get_pixel_resource(st->dpy, cmap, "background", "Background");

  st->delay = get_integer_resource(st->dpy, "delay", "Integer");
  if (st->delay < 1)
    st->delay = 1;
  st->duration = get_integer_resource (st->dpy, "duration", "Seconds");
  if (st->duration < 1)
    st->duration = 1;
  st->pixwidth = get_integer_resource(st->dpy, "pixwidth", "Integer");
  if (st->pixwidth < 1)
    st->pixwidth = 1;
  st->pixheight = get_integer_resource(st->dpy, "pixheight", "Integer");
  if (st->pixheight < 1)
    st->pixheight = 1;
  st->pixspacex = get_integer_resource(st->dpy, "pixspacex", "Integer");
  if (st->pixspacex < 0)
    st->pixspacex = 0;
  st->pixspacey = get_integer_resource(st->dpy, "pixspacey", "Integer");
  if (st->pixspacey < 0)
    st->pixspacey = 0;
  st->lenses = get_boolean_resource(st->dpy, "lenses", "Boolean");
  st->lensoffsetx = get_integer_resource(st->dpy, "lensoffsetx", "Integer");
  st->lensoffsetx = MAX(0, MIN(st->pixwidth, st->lensoffsetx));
  st->lensoffsety = get_integer_resource(st->dpy, "lensoffsety", "Integer");
  st->lensoffsety = MAX(0, MIN(st->pixwidth, st->lensoffsety));

  gcv.function = GXcopy;
  gcv.subwindow_mode = IncludeInferiors;
  gcflags = GCForeground|GCFunction;
  gcv.foreground = bg;
  if (!st->lenses && use_subwindow_mode_p(xgwa.screen, st->window))       /* see grabscreen.c */
    gcflags |= GCSubwindowMode;
  st->window_gc = XCreateGC(st->dpy, st->window, gcflags, &gcv);


  st->orig_map = NULL;
  st->pm = XCreatePixmap(st->dpy, st->window, st->sizex, st->sizey, xgwa.depth);

  XFillRectangle(st->dpy, st->window, st->window_gc, 0, 0, st->sizex, st->sizey);
  XSetWindowBackground(st->dpy, st->window, bg);

  st->start_time = time ((time_t) 0);
  st->img_loader = load_image_async_simple (0, xgwa.screen, st->window,
                                            st->pm, 0, 0);

  /* We might have needed this to grab the image, but if we leave this set
     to GCSubwindowMode, then we'll *draw* right over subwindows too. */
  XSetSubwindowMode (st->dpy, st->window_gc, ClipByChildren);


  nblocksx = (int)ceil((double)st->sizex / (double)(st->pixwidth + st->pixspacex));
  nblocksy = (int)ceil((double)st->sizey / (double)(st->pixheight + st->pixspacey));
  if (st->lenses)
    st->s = MAX((nblocksx - 1) * st->lensoffsetx + st->pixwidth, 
	    (nblocksy - 1) * st->lensoffsety + st->pixheight) * 2;
  else
    st->s = MAX(nblocksx, nblocksy) * 2;

  st->sinusoid_offset = random();

  return st;
}

static unsigned long
zoom_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  unsigned x, y, i, j;

  long now;

  if (st->img_loader)   /* still loading */
    {
      st->img_loader = load_image_async_simple (st->img_loader, 0, 0, 0, 0, 0);
      if (! st->img_loader) {  /* just finished */
        XClearWindow (st->dpy, st->window);
        st->start_time = time ((time_t) 0);
        if (!st->lenses) {
          st->orig_map = XGetImage(st->dpy, st->pm, 0, 0, st->sizex, st->sizey, ~0L, ZPixmap);
/*          XFreePixmap(st->dpy, st->pm);
          st->pm = 0;*/
        }
      }
      return st->delay;
    }

  if (!st->img_loader &&
      st->start_time + st->duration < time ((time_t) 0)) {
    st->img_loader = load_image_async_simple (0, st->screen, st->window,
                                              st->pm, 0, 0);
    return st->delay;
  }

#define nrnd(x) (random() % (x))

  now = currentTimeInMs(st);
  now += st->sinusoid_offset;  /* don't run multiple screens in lock-step */

  /* find new x,y */
  st->tlx = ((1. + sin(((double)now) / X_PERIOD * 2. * M_PI))/2.0)
    * (st->sizex - st->s/2) /* -s/4 */ + MINX;
  st->tly = ((1. + sin(((double)now) / Y_PERIOD * 2. * M_PI))/2.0)
    * (st->sizey - st->s/2) /* -s/4 */ + MINY;

  if (st->lenses) {
    for (x = i = 0; x < st->sizex; x += (st->pixwidth + st->pixspacex), ++i)
      for (y = j = 0; y < st->sizey; y += (st->pixheight + st->pixspacey), ++j) {
	XCopyArea(st->dpy, st->pm /* src */, st->window /* dest */, st->window_gc,
		  st->tlx + i * st->lensoffsetx /* src_x */, 
		  st->tly + j * st->lensoffsety /* src_y */,
		  st->pixwidth, st->pixheight,
		  x /* dest_x */, y /* dest_y */);
      }
  } else {
    for (x = i = 0; x < st->sizex; x += (st->pixwidth + st->pixspacex), ++i)
      for (y = j = 0; y < st->sizey; y += (st->pixheight + st->pixspacey), ++j) {
	XSetForeground(st->dpy, st->window_gc, XGetPixel(st->orig_map, st->tlx+i, st->tly+j));
	XFillRectangle(st->dpy, st->window, st->window_gc, 
		       i * (st->pixwidth + st->pixspacex),
		       j * (st->pixheight + st->pixspacey), st->pixwidth, st->pixheight);
      }
  }

  return st->delay;
}

static void
zoom_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
zoom_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (screenhack_event_helper (dpy, window, event))
    {
      st->start_time = 0;
      return True;
    }
  return False;
}

static void
zoom_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  XFreeGC (st->dpy, st->window_gc);
  if (st->orig_map) XDestroyImage (st->orig_map);
  if (st->pm) XFreePixmap (st->dpy, st->pm);
  free (st);
}


static const char *zoom_defaults[] = {
  "*dontClearRoot: True",
  ".foreground: white",
  ".background: #111111",
  "*fpsSolid:	true",
#ifdef __sgi /* really, HAVE_READ_DISPLAY_EXTENSION */
  "*visualID: Best",
#endif
  "*lenses:      true",
  "*delay:       10000",
  "*duration:    120",
  "*pixwidth:    10",
  "*pixheight:   10",
  "*pixspacex:   2",
  "*pixspacey:   2",
  "*lensoffsetx: 5",
  "*lensoffsety: 5",
#ifdef USE_IPHONE
  "*ignoreRotation: True",
  "*rotateImages:   True",
#endif
  0
};

static XrmOptionDescRec zoom_options[] = {
  { "-lenses", ".lenses", XrmoptionNoArg, "true" },
  { "-no-lenses", ".lenses", XrmoptionNoArg, "false" },
  { "-delay", ".delay", XrmoptionSepArg, 0 },
  { "-duration",  ".duration", XrmoptionSepArg, 0 },
  { "-pixwidth", ".pixwidth", XrmoptionSepArg, 0 },
  { "-pixheight", ".pixheight", XrmoptionSepArg, 0 },
  { "-pixspacex", ".pixspacex", XrmoptionSepArg, 0 },
  { "-pixspacey", ".pixspacey", XrmoptionSepArg, 0 },
  { "-lensoffsetx", ".lensoffsetx", XrmoptionSepArg, 0 },
  { "-lensoffsety", ".lensoffsety", XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Zoom", zoom)
