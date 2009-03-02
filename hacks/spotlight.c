/*
 * spotlight - an xscreensaver module
 * Copyright (c) 1999, 2001 Rick Schultz <rick@skapunx.net>
 *
 * loosely based on the BackSpace module "StefView" by Darcy Brockbank
 */

/* modified from a module from the xscreensaver distribution */

/*
 * xscreensaver, Copyright (c) 1992, 1993, 1994, 1996, 1997, 1998
 * Jamie Zawinski <jwz@jwz.org>
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
#include "screenhack.h"
#include <X11/Xutil.h>
#include <sys/time.h>

#define MINX 0.0
#define MINY 0.0
#define X_PERIOD 15000.0
#define Y_PERIOD 12000.0

static int sizex, sizey; /* screen size */
static int delay;        /* in case it's too fast... */
static GC window_gc;
#ifdef DEBUG
static GC white_gc;
#endif
static GC buffer_gc;     /* draw in buffer, then flush to screen
			    to avoid flicker */
static int radius;       /* radius of spotlight in pixels */

static Pixmap pm;        /* pixmap grabbed from screen */
static Pixmap clip_pm;   /* pixmap for clipping (spotlight shape) */
static Pixmap buffer;    /* pixmap for the buffer */

static GC clip_gc;       /* GC for the clip pixmap */

static int x, y, s;      /* x & y coords of buffer (upper left corner) */
                         /* s is the width of the buffer */

static int oldx, oldy, max_x_speed, max_y_speed;
                         /* used to keep the new buffer position
			    over the old spotlight image to make sure 
			    the old image is completely erased */

/* The path the spotlight follows around the screen is sinusoidal.
   This function is fed to sin() to get the x & y coords */
static long
currentTimeInMs(void)
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


static void
init_hack (Display *dpy, Window window)
{
  XGCValues gcv;
  XWindowAttributes xgwa;
  long gcflags;
  Colormap cmap;
  unsigned long fg, bg;

  XGetWindowAttributes (dpy, window, &xgwa);
  sizex = xgwa.width;
  sizey = xgwa.height;
  cmap = xgwa.colormap;
  fg = get_pixel_resource ("foreground", "Foreground", dpy, cmap);
  bg = get_pixel_resource ("background", "Background", dpy, cmap);

  /* read parameters, keep em sane */
  delay = get_integer_resource ("delay", "Integer");
  if (delay < 1) delay = 1;
  radius = get_integer_resource ("radius", "Integer");
  if (radius < 0) radius = 125;

  /* Don't let the spotlight be bigger than 1/4 of the window */
  if (radius > xgwa.width  / 4) radius = xgwa.width  / 4;
  if (radius > xgwa.height / 4) radius = xgwa.height / 4;

  /* do the dance */
  gcv.function = GXcopy;
  gcv.subwindow_mode = IncludeInferiors;
  gcflags = GCForeground | GCFunction;
  gcv.foreground = bg;

#ifdef NOPE
  if (use_subwindow_mode_p(xgwa.screen, window)) /* see grabscreen.c */
    gcflags |= GCSubwindowMode;
#endif
  window_gc = XCreateGC(dpy, window, gcflags, &gcv);

  /* grab screen to pixmap */
  pm = XCreatePixmap(dpy, window, sizex, sizey, xgwa.depth);
  load_random_image (xgwa.screen, window, pm, NULL);
  XClearWindow(dpy, window);
  XFlush (dpy);

  /* create buffer to reduce flicker */
  buffer = XCreatePixmap(dpy, window, sizex, sizey, xgwa.depth);
  buffer_gc = XCreateGC(dpy, buffer, gcflags, &gcv);
  XFillRectangle(dpy, buffer, buffer_gc, 0, 0, sizex, sizey);

  /* blank out screen */
  XFillRectangle(dpy, window, window_gc, 0, 0, sizex, sizey);
  XSetWindowBackground (dpy, window, bg);

  /* create clip mask (so it's a circle, not a square) */
  clip_pm = XCreatePixmap(dpy, window, radius*4, radius*4, 1);

  gcv.foreground = 0L;
  clip_gc = XCreateGC(dpy, clip_pm, gcflags, &gcv);
  XFillRectangle(dpy, clip_pm, clip_gc, 0, 0, radius*4, radius*4);

  XSetForeground(dpy, clip_gc, 1L);
  XFillArc(dpy, clip_pm, clip_gc, radius , radius,
	   radius*2, radius*2, 0, 360*64);

  /* set buffer's clip mask to the one we just made */
  XSetClipMask(dpy, buffer_gc, clip_pm);

  /* free everything */
  XFreeGC(dpy, clip_gc);
  XFreePixmap(dpy, clip_pm);

  /* avoid remants */
  max_x_speed = max_y_speed = radius;

#ifdef DEBUG
  /* create GC with white fg */
  gcv.foreground = fg;
  white_gc = XCreateGC(dpy, window, gcflags, &gcv);
#endif
  
  /* initialize x and y to avoid initial `jump' across screen */
  x = ((1 + sin(((float)currentTimeInMs()) / X_PERIOD * 2. * M_PI))/2.0) 
    * (sizex - s/2) -s/4  + MINX;
  y = ((1 + sin(((float)currentTimeInMs()) / Y_PERIOD * 2. * M_PI))/2.0) 
    * (sizey - s/2) -s/4  + MINY;

}


/*
 * perform one iteration
 */
static void
onestep (Display *dpy, Window window)
{
    long now;

    /* clear buffer */
    XFillRectangle(dpy, buffer, buffer_gc, x, y, s, s);

    
#define nrnd(x) (random() % (x))

    oldx = x;
    oldy = y;

    s = radius *4 ;   /* s = width of buffer */

    now = currentTimeInMs();

    /* find new x,y */
    x = ((1 + sin(((float)now) / X_PERIOD * 2. * M_PI))/2.0) 
      * (sizex - s/2) -s/4  + MINX;
    y = ((1 + sin(((float)now) / Y_PERIOD * 2. * M_PI))/2.0) 
      * (sizey - s/2) -s/4  + MINY;
    
    /* limit change in x and y to buffer width */
    if ( x < (oldx - max_x_speed) ) x = oldx - max_x_speed;
    if ( x > (oldx + max_x_speed) ) x = oldx + max_x_speed;
    if ( y < (oldy - max_y_speed) ) y = oldy - max_y_speed;
    if ( y > (oldy + max_y_speed) ) y = oldy + max_y_speed;

    /* copy area of screen image (pm) to buffer
       Clip to a circle */
    XSetClipOrigin(dpy, buffer_gc, x,y);
    XCopyArea(dpy, pm, buffer, buffer_gc, x, y, s, s, x, y);
    /* copy buffer to screen (window) */
    XCopyArea(dpy, buffer, window, window_gc, x , y, s, s, x, y);

#ifdef DEBUG
    /* draw a box around the buffer */
    XDrawLine(dpy, window, white_gc, x, y, x+s, y);
    XDrawLine(dpy, window, white_gc, x, y, x, y+s);
    XDrawLine(dpy, window, white_gc, x+s, y, x+s, y+s);
    XDrawLine(dpy, window, white_gc, x, y+s, x+s, y+s);
#endif

}


char *progclass = "Spotlight";

char *defaults [] = {
  ".background:			black",
  ".foreground:			white",
  "*dontClearRoot:		True",

#ifdef __sgi	/* really, HAVE_READ_DISPLAY_EXTENSION */
  "*visualID:			Best",
#endif

  "*delay:			10000",
  "*radius:			125",
  0
};

XrmOptionDescRec options [] = {
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-radius",		".radius",		XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

void
screenhack (Display *dpy, Window window)
{
  init_hack (dpy, window);
  while (1) {
    onestep(dpy, window);
    XSync(dpy, False);
    if (delay) usleep (delay);
    screenhack_handle_events (dpy);
  }
}
  
