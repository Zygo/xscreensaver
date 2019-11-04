/*
 * spotlight - an xscreensaver module
 * Copyright (c) 1999, 2001 Rick Schultz <rick.schultz@gmail.com>
 *
 * loosely based on the BackSpace module "StefView" by Darcy Brockbank
 */

/* modified from a module from the xscreensaver distribution */

/*
 * xscreensaver, Copyright (c) 1992-2006 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */


/* #define DEBUG */
#include <math.h>
#include <limits.h>
#include "screenhack.h"

#define X_PERIOD 15000.0
#define Y_PERIOD 12000.0

struct state {
  Display *dpy;
  Window window;
  Screen *screen;

  int sizex, sizey; /* screen size */
  int delay;
  int duration;
  time_t start_time;
  int first_time;
  GC window_gc;
#ifdef DEBUG
  GC white_gc;
#endif
  GC buffer_gc;     /* draw in buffer, then flush to screen
                       to avoid flicker */
  int radius;       /* radius of spotlight in pixels */

  Pixmap pm;        /* pixmap grabbed from screen */
  Pixmap buffer;    /* pixmap for the buffer */

  int x, y, s;      /* x & y coords of buffer (upper left corner) */
  /* s is the width of the buffer */

  int off;	/* random offset from currentTimeInMs(), so that
                   two concurrent copies of spotlight have different
                   behavior. */

  int oldx, oldy, max_x_speed, max_y_speed;
  /* used to keep the new buffer position
     over the old spotlight image to make sure 
     the old image is completely erased */

  Bool first_p;
  async_load_state *img_loader;
  XRectangle geom;
};


/* The path the spotlight follows around the screen is sinusoidal.
   This function is fed to sin() to get the x & y coords */
static long
currentTimeInMs(struct state *st)
{
  struct timeval curTime;
  unsigned long ret_unsigned;
#ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone tz = {0,0};
  gettimeofday(&curTime, &tz);
#else
  gettimeofday(&curTime);
#endif
  ret_unsigned = curTime.tv_sec *1000U + curTime.tv_usec / 1000;
  return (ret_unsigned <= LONG_MAX) ? ret_unsigned : -1 - (long)(ULONG_MAX - ret_unsigned);
}


static void *
spotlight_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;
  XWindowAttributes xgwa;
  long gcflags;
  Colormap cmap;
  unsigned long bg;
  GC clip_gc;
  Pixmap clip_pm;

  st->dpy = dpy;
  st->window = window;
  st->first_p = True;

  XGetWindowAttributes (st->dpy, st->window, &xgwa);
  st->screen = xgwa.screen;
  st->sizex = xgwa.width;
  st->sizey = xgwa.height;
  cmap = xgwa.colormap;
  bg = get_pixel_resource (st->dpy, cmap, "background", "Background");

  /* read parameters, keep em sane */
  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  if (st->delay < 1) st->delay = 1;
  st->duration = get_integer_resource (st->dpy, "duration", "Seconds");
  if (st->duration < 1) st->duration = 1;
  st->radius = get_integer_resource (st->dpy, "radius", "Integer");
  if (st->radius < 0) st->radius = 125;

  if (xgwa.width > 2560) st->radius *= 2;  /* Retina displays */

  /* Don't let the spotlight be bigger than the window */
  while (st->radius > xgwa.width * 0.45)
    st->radius /= 2;
  while (st->radius > xgwa.height * 0.45)
    st->radius /= 2;

  if (st->radius < 4)
    st->radius = 4;

  /* do the dance */
  gcv.function = GXcopy;
  gcv.subwindow_mode = IncludeInferiors;
  gcflags = GCForeground | GCFunction;
  gcv.foreground = bg;

#ifdef NOPE
  if (use_subwindow_mode_p(xgwa.screen, st->window)) /* see grabscreen.c */
    gcflags |= GCSubwindowMode;
#endif
  st->window_gc = XCreateGC(st->dpy, st->window, gcflags, &gcv);

  st->pm = XCreatePixmap(st->dpy, st->window, st->sizex, st->sizey, xgwa.depth);
  XClearWindow(st->dpy, st->window);

  st->first_time = 1;

  /* create buffer to reduce flicker */
#ifdef HAVE_JWXYZ	/* Don't second-guess Quartz's double-buffering */
  st->buffer = 0;
#else
  st->buffer = XCreatePixmap(st->dpy, st->window, st->sizex, st->sizey, xgwa.depth);
#endif

  st->buffer_gc = XCreateGC(st->dpy, (st->buffer ? st->buffer : window), gcflags, &gcv);
  if (st->buffer)
    XFillRectangle(st->dpy, st->buffer, st->buffer_gc, 0, 0, st->sizex, st->sizey);

  /* create clip mask (so it's a circle, not a square) */
  clip_pm = XCreatePixmap(st->dpy, st->window, st->radius*4, st->radius*4, 1);
  st->img_loader = load_image_async_simple (0, xgwa.screen, st->window, st->pm,
                                            0, &st->geom);
  st->start_time = time ((time_t *) 0);

  gcv.foreground = 0L;
  clip_gc = XCreateGC(st->dpy, clip_pm, gcflags, &gcv);
  XFillRectangle(st->dpy, clip_pm, clip_gc, 0, 0, st->radius*4, st->radius*4);

  XSetForeground(st->dpy, clip_gc, 1L);
  XFillArc(st->dpy, clip_pm, clip_gc, st->radius , st->radius,
	   st->radius*2, st->radius*2, 0, 360*64);

  /* set buffer's clip mask to the one we just made */
  XSetClipMask(st->dpy, st->buffer_gc, clip_pm);

  /* free everything */
  XFreeGC(st->dpy, clip_gc);
  XFreePixmap(st->dpy, clip_pm);

  /* avoid remants */
  st->max_x_speed = st->max_y_speed = st->radius;
  
  st->off = random();

  /* blank out screen */
  XFillRectangle(st->dpy, st->window, st->window_gc, 0, 0, st->sizex, st->sizey);

  return st;
}


/*
 * perform one iteration
 */
static void
onestep (struct state *st, Bool first_p)
{
  long now;
  unsigned long now_unsigned;

  if (st->img_loader)   /* still loading */
    {
      st->img_loader = load_image_async_simple (st->img_loader, 0, 0, 0, 0, 
                                                &st->geom);
      if (! st->img_loader) {  /* just finished */
        st->start_time = time ((time_t *) 0);
      }
      return;
    }

  if (!st->img_loader &&
      st->start_time + st->duration < time ((time_t *) 0)) {
    st->img_loader = load_image_async_simple (0, st->screen, st->window,
                                              st->pm, 0, &st->geom);
    return;
  }

#define nrnd(x) (random() % (x))

  st->oldx = st->x;
  st->oldy = st->y;

  st->s = st->radius *4 ;   /* s = width of buffer */

  now_unsigned = (unsigned long) currentTimeInMs(st) + st->off;
  now = (now_unsigned <= LONG_MAX) ? now_unsigned : -1 - (long)(ULONG_MAX - now_unsigned);

  /* find new x,y */
  st->x = st->geom.x +
    ((1 + sin(((double)now) / X_PERIOD * 2. * M_PI))/2.0) 
    * (st->geom.width - st->s/2) -st->s/4;
  st->y = st->geom.y +
    ((1 + sin(((double)now) / Y_PERIOD * 2. * M_PI))/2.0) 
    * (st->geom.height - st->s/2) -st->s/4;
    
  if (!st->first_p)
    {
      /* limit change in x and y to buffer width */
      if ( st->x < (st->oldx - st->max_x_speed) ) st->x = st->oldx - st->max_x_speed;
      if ( st->x > (st->oldx + st->max_x_speed) ) st->x = st->oldx + st->max_x_speed;
      if ( st->y < (st->oldy - st->max_y_speed) ) st->y = st->oldy - st->max_y_speed;
      if ( st->y > (st->oldy + st->max_y_speed) ) st->y = st->oldy + st->max_y_speed;
    }

  if (! st->buffer)
    {
      XClearWindow (st->dpy, st->window);
      XSetClipOrigin(st->dpy, st->buffer_gc, st->x,st->y);
      XCopyArea(st->dpy, st->pm, st->window, st->buffer_gc, st->x, st->y, st->s, st->s, st->x, st->y);
    }
  else
    {
      /* clear buffer */
      XFillRectangle(st->dpy, st->buffer, st->buffer_gc, st->x, st->y, st->s, st->s);

      /* copy area of screen image (pm) to buffer
         Clip to a circle */
      XSetClipOrigin(st->dpy, st->buffer_gc, st->x,st->y);
      XCopyArea(st->dpy, st->pm, st->buffer, st->buffer_gc, st->x, st->y, st->s, st->s, st->x, st->y);

      if (st->first_time) {
        /* blank out screen */
        XFillRectangle(st->dpy, st->window, st->window_gc, 0, 0, st->sizex, st->sizey);
        st->first_time = 0;
      }

      /* copy buffer to screen (window) */
      XCopyArea(st->dpy, st->buffer, st->window, st->window_gc, st->x , st->y, st->s, st->s, st->x, st->y);

# if 0
      XSetForeground (st->dpy, st->window_gc,
                      WhitePixel (st->dpy, DefaultScreen (st->dpy)));
      XDrawRectangle(st->dpy, st->window, st->window_gc,
                     st->geom.x, st->geom.y, st->geom.width, st->geom.height);
# endif
    }

#ifdef DEBUG
  /* draw a box around the buffer */
  XDrawRectangle(st->dpy, st->window, st->white_gc, st->x , st->y, st->s, st->s);
#endif

}


static unsigned long
spotlight_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  onestep(st, st->first_p);
  st->first_p = False;
  return st->delay;
}
  
static void
spotlight_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
spotlight_event (Display *dpy, Window window, void *closure, XEvent *event)
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
spotlight_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  XFreeGC (dpy, st->window_gc);
  XFreeGC (dpy, st->buffer_gc);
  if (st->pm) XFreePixmap (dpy, st->pm);
  if (st->buffer) XFreePixmap (dpy, st->buffer);
  free (st);
}




static const char *spotlight_defaults [] = {
  ".background:			black",
  ".foreground:			white",
  "*dontClearRoot:		True",
  "*fpsSolid:			true",

#ifdef __sgi	/* really, HAVE_READ_DISPLAY_EXTENSION */
  "*visualID:			Best",
#endif

  "*delay:			10000",
  "*duration:			120",
  "*radius:			125",
#ifdef HAVE_MOBILE
  "*ignoreRotation:             True",
  "*rotateImages:               True",
#endif
  0
};

static XrmOptionDescRec spotlight_options [] = {
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-duration",	".duration",		XrmoptionSepArg, 0 },
  { "-radius",		".radius",		XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Spotlight", spotlight)
