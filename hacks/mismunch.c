/* mismunch.c
 * Munch Errors
 * Copyright (c) 2004 Steven Hazel <sah@thalassocracy.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * Based on munch.c
 * A munching squares implementation for X
 * Copyright 1997, Tim Showalter <tjs@andrew.cmu.edu>
 *
 * Some code stolen from / This is meant to work with
 * xscreensaver, Copyright (c) 1992, 1995, 1996 Jamie Zawinski <jwz@jwz.org>
 *
 */

#include <math.h>
#include "screenhack.h"

char *progclass = "Mismunch";

char *defaults [] = {
  ".background:       black",
  ".foreground:       white",
  "*delay:            2500",
  "*simul:            5",
  "*clear:            65",
  "*xor:              True",
  0
};

XrmOptionDescRec options [] = {
  { "-delay",         ".delay",       XrmoptionSepArg,  0 },
  { "-simul",         ".simul",       XrmoptionSepArg,  0 },
  { "-clear",         ".clear",       XrmoptionSepArg, "true" },
  { "-xor",           ".xor",         XrmoptionNoArg,  "true" },
  { "-no-xor",        ".xor",         XrmoptionNoArg,  "false" },
  { 0, 0, 0, 0 }
};


static GC gc;
static int delay, simul, clear, xor;
static int logminwidth, logmaxwidth;
static int restart, window_width, window_height;

typedef struct _muncher {
  int width;
  int atX, atY;
  int kX, kT, kY;
  int grav;
  XColor fgc;
  int yshadow, xshadow;
  int x, y, t;
  int doom;
  int done;
} muncher;


/*
 * dumb way to get # of digits in number.  Probably faster than actually
 * doing a log and a division, maybe.
 */
static int dumb_log_2(int k) {
  int r = -1;
  while (k > 0) {
    k >>= 1; r++;
  }
  return r;
}


void calc_logwidths (void) {
  /* Choose a range of square sizes based on the window size.  We want
     a power of 2 for the square width or the munch doesn't fill up.
     Also, if a square doesn't fit inside an area 20% smaller than the
     window, it's too big.  Mismunched squares that big make things
     look too noisy. */

  if (window_height < window_width) {
    logmaxwidth = (int)dumb_log_2(window_height * 0.8);
  } else {
    logmaxwidth = (int)dumb_log_2(window_width * 0.8);
  }

  if (logmaxwidth < 2) {
    logmaxwidth = 2;
  }

  /* we always want three sizes of squares */
  logminwidth = logmaxwidth - 2;

  if (logminwidth < 2) {
    logminwidth = 2;
  }
}


void mismunch_handle_events (Display *dpy) {
  XEvent e;

  while (XPending(dpy)) {
    XNextEvent(dpy, &e);

    switch (e.type) {
      case ConfigureNotify:
        if (e.xconfigure.width != window_width ||
            e.xconfigure.height != window_height) {
          window_width = e.xconfigure.width;
          window_height = e.xconfigure.height;
          calc_logwidths();
          restart = 1;
        }
        break;

      default:
        screenhack_handle_event(dpy, &e);
    }
  }
}


static muncher *make_muncher (Display *dpy, Window w) {
  int logwidth;
  XWindowAttributes xgwa;
  muncher *m = (muncher *) malloc(sizeof(muncher));

  XGetWindowAttributes(dpy, w, &xgwa);

  /* choose size -- power of two */
  logwidth = (logminwidth +
              (random() % (1 + logmaxwidth - logminwidth)));

  m->width = 1 << logwidth;

  /* draw at this location */
  m->atX = (random() % (xgwa.width <= m->width ? 1
                        : xgwa.width - m->width));
  m->atY = (random() % (xgwa.height <= m->width ? 1
                        : xgwa.width - m->width));

  /* wrap-around by these values; no need to % as we end up doing that
     later anyway */
  m->kX = ((random() % 2)
           ? (random() % m->width) : 0);
  m->kT = ((random() % 2)
           ? (random() % m->width) : 0);
  m->kY = ((random() % 2)
           ? (random() % m->width) : 0);

  /* set the gravity of the munch, or rather, which direction we draw
     stuff in. */
  m->grav = random() % 2;

  /* I like this color scheme better than random colors. */
  switch (random() % 4) {
    case 0:
      m->fgc.red = random() % 65536;
      m->fgc.blue = random() % 32768;
      m->fgc.green = random() % 16384;
      break;

    case 1:
      m->fgc.red = 0;
      m->fgc.blue = random() % 65536;
      m->fgc.green = random() % 16384;
      break;

    case 2:
      m->fgc.red = random() % 8192;
      m->fgc.blue = random() % 8192;
      m->fgc.green = random() % 49152;
      break;

    case 3:
      m->fgc.red = random() % 65536;
      m->fgc.green = m->fgc.red;
      m->fgc.blue = m->fgc.red;
      break;
  }

  /* Sometimes draw a mostly-overlapping copy of the square.  This
     generates all kinds of neat blocky graphics when drawing in xor
     mode. */
  if (random() % 4) {
    m->xshadow = 0;
    m->yshadow = 0;
  } else {
    m->xshadow = (random() % (m->width/3)) - (m->width/6);
    m->yshadow = (random() % (m->width/3)) - (m->width/6);
  }

  /* Start with a random y value -- this sort of controls the type of
     deformities seen in the squares. */
  m->y = random() % 256;

  m->t = 0;

  /*
    Doom each square to be aborted at some random point.
    (When doom == (width - 1), the entire square will be drawn.)
  */
  m->doom = random() % m->width;

  m->done = 0;

  return m;
}


static void munch (Display *dpy, Window w, muncher *m) {
  int drawX, drawY;
  XWindowAttributes xgwa;
  static Colormap cmap;

  if (m->done) {
    return;
  }

  XGetWindowAttributes(dpy, w, &xgwa);

  if (!mono_p) {
    /* XXX there are probably bugs with this. */
    cmap = xgwa.colormap;

    if (XAllocColor(dpy, cmap, &m->fgc)) {
      XSetForeground(dpy, gc, m->fgc.pixel);
    }
  }

  /* Finally draw this pass of the munching error. */

  for(m->x = 0; m->x < m->width; m->x++) {
    /* figure out the next point */

    /*
      The ordinary Munching Squares calculation is:
      m->y = ((m->x ^ ((m->t + m->kT) % m->width)) + m->kY) % m->width;

      We create some feedback by plugging in y in place of x, and
      make a couple of values negative so that some parts of some
      squares get drawn in the wrong place.
    */
    m->y = ((-m->y ^ ((-m->t + m->kT) % m->width)) + m->kY) % m->width;

    drawX = ((m->x + m->kX) % m->width) + m->atX;
    drawY = (m->grav ? m->y + m->atY : m->atY + m->width - 1 - m->y);

    XDrawPoint(dpy, w, gc, drawX, drawY);
    if ((m->xshadow != 0) || (m->yshadow != 0)) {
      /* draw the corresponding shadow point */
      XDrawPoint(dpy, w, gc, drawX + m->xshadow, drawY + m->yshadow);
    }
    /* XXX may want to change this to XDrawPoints,
       but it's fast enough without it for the moment. */

  }

  /*
    we've drawn one pass' worth of points.  let the server catch up or
    this won't be interesting to watch at all.  we call this here in
    the hopes of having one whole sequence of dots appear at the same
    time (one for each value of x, surprisingly enough)
  */
  XSync(dpy, False);
  mismunch_handle_events(dpy);

  if (delay) usleep(delay);

  m->t++;
  if (m->t > m->doom) {
    m->done = 1;
    return;
  }

  return;
}


void screenhack (Display *dpy, Window w)
{
  XWindowAttributes xgwa;
  Colormap cmap;
  XGCValues gcv;
  int n = 0;  /* number of squares before we have to clear */
  muncher **munchers;
  int i;

  restart = 0;

  /* get the dimensions of the window */
  XGetWindowAttributes(dpy, w, &xgwa);

  /* create the gc */
  cmap = xgwa.colormap;
  gcv.foreground= get_pixel_resource("foreground","Foreground",
                                     dpy, cmap);
  gcv.background= get_pixel_resource("background","Background",
                                     dpy, cmap);

  gc = XCreateGC(dpy, w, GCForeground|GCBackground, &gcv);

  delay = get_integer_resource("delay", "Integer");
  if (delay < 0) delay = 0;

  simul = get_integer_resource("simul", "Integer");
  if (simul < 1) simul = 1;

  clear = get_integer_resource("clear", "Integer");
  if (clear < 0) clear = 0;

  xor = get_boolean_resource("xor", "Boolean");

  window_width = xgwa.width;
  window_height = xgwa.height;

  calc_logwidths();

  /* always draw xor on mono. */
  if (mono_p || xor) {
    XSetFunction(dpy, gc, GXxor);
  }

  munchers = (muncher **) calloc(simul, sizeof(muncher *));
  for (i = 0; i < simul; i++) {
    munchers[i] = make_muncher(dpy, w);
  }

  for(;;) {
    for (i = 0; i < simul; i++) {
      munch(dpy, w, munchers[i]);

      if (munchers[i]->done) {
        n++;

        free(munchers[i]);
        munchers[i] = make_muncher(dpy, w);

        mismunch_handle_events(dpy);
      }
    }

    if (restart || (clear && n >= clear)) {
      for (i = 0; i < simul; i++) {
        free(munchers[i]);
        munchers[i] = make_muncher(dpy, w);
      }

      XClearWindow(dpy, w);
      n = 0;
      restart = 0;
    }

    XSync(dpy, False);
  }
}
