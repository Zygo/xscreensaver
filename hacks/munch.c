/* munch.c
 * A munching squares implementation for X
 * Tim Showalter <tjs@andrew.cmu.edu>
 * 
 * Copyright 1997, Tim Showalter
 * Permission is granted to copy, modify, and use this as long
 * as this notice remains intact.  No warranties are expressed or implied.
 * CMU Sucks.
 * 
 * Some code stolen from / This is meant to work with
 * xscreensaver, Copyright (c) 1992, 1995, 1996
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

/* Munching Squares is this simplistic, silly screen hack (according
   to HAKMEM, discovered by Jackson Wright in 1962) where you take
   Y = X XOR T and graph it over and over.  According to HAKMEM, it 
   takes 5 instructions of PDP-1 assembly.  This is a little more
   complicated than that, mostly X's fault, but it does some other
   random things.

   http://www.inwap.com/pdp10/hbaker/hakmem/hacks.html#item146
 */

#include <math.h>
/*#include <assert.h>*/
#include "screenhack.h"

/* flags for random things.  Must be < log2(random's maximum), incidentially.
 */
#define SHIFT_KX (0x01)
#define SHIFT_KT (0x02)
#define SHIFT_KY (0x04)
#define GRAV     (0x08)


struct state {
  Display *dpy;
  Window window;

  GC gc;
  int delay, hold, clear, logminwidth, shiftk, xor;

  int logmaxwidth;
  int maxwidth;
  int randflags;
  int thiswidth;
  XWindowAttributes xgwa;

  int munch_t;

  int reset;
  int atX, atY, kX, kT, kY, grav;
  int square_count;
};


static int munchOnce (struct state *st, int width)
{

    if (st->munch_t == 0) {
      /*
        fprintf(stderr,"Doing width %d at %d %d shift %d %d %d grav %d\n",
        width, atX, atY, kX, kT, kY, grav);
      */
    
      if (!mono_p) {
        XColor fgc;
	fgc.red = random() % 65535;
	fgc.green = random() % 65535;
	fgc.blue = random() % 65535;
    
	if (XAllocColor(st->dpy, st->xgwa.colormap, &fgc)) {
          XSetForeground(st->dpy, st->gc, fgc.pixel);
	}
      }
    }
    
    /* Finally draw this munching square. */
    /* for(munch_t = 0; munch_t < width; munch_t++) */
    {
      int x;
      for(x = 0; x < width; x++) {
        /* figure out the next point */
        int y = ((x ^ ((st->munch_t + st->kT) % width)) + st->kY) % width;
        int drawX = ((x + st->kX) % width) + st->atX;
        int drawY = (st->grav ? y + st->atY : st->atY + width - 1 - y);

        /* used to be bugs where it would draw partially offscreen.
           while that might be a pretty feature, I didn'munch_t want it to do
           that yet.  if these trigger, please let me know.
        */
        /*	    assert(drawX >= 0 && drawX < xgwa.width);
          assert(drawY >= 0 && drawY < xgwa.height);
        */

        XDrawPoint(st->dpy, st->window, st->gc, drawX, drawY);
        /* XXX may want to change this to XDrawPoints,
           but it's fast enough without it for the moment. */
	    
      }
    }

    st->munch_t++;
    if (st->munch_t >= width) {
      st->munch_t = 0;
      return 1;
    }

    return 0;
}

/*
 * dumb way to get # of digits in number.  Probably faster than actually
 * doing a log and a division, maybe.
 */
static int dumb_log_2(int k)
{
    int r = -1;
    while (k > 0) {
	k >>= 1; r++;
    }
    return r;
}

static void *
munch_init (Display *dpy, Window w)
{
    struct state *st = (struct state *) calloc (1, sizeof(*st));
    XGCValues gcv;
    
    st->dpy = dpy;
    st->window = w;

    /* get the dimensions of the window */
    XGetWindowAttributes (st->dpy, w, &st->xgwa);
    
    /* We need a square; limit on screen size? */
    /* we want a power of 2 for the width or the munch doesn't fill up.
     */
    st->logmaxwidth = (int)
	dumb_log_2(st->xgwa.height < st->xgwa.width ? st->xgwa.height : st->xgwa.width);

    st->maxwidth = 1 << st->logmaxwidth;

    if (st->logmaxwidth < st->logminwidth) {
	/* off-by-one error here?  Anyone running on < 640x480? */
	fprintf(stderr, "munch: screen too small; use -logminwidth\n");
	fprintf(stderr, "\t(width is %d; log is %d; log must be at least "
		"%d)\n", 
		(st->xgwa.height < st->xgwa.width ? st->xgwa.height : st->xgwa.width),
		st->logmaxwidth, st->logminwidth);
	exit(0);
    }
    
    /* create the gc */
    gcv.foreground= get_pixel_resource(st->dpy, st->xgwa.colormap,
                                       "foreground","Foreground");
    gcv.background= get_pixel_resource(st->dpy, st->xgwa.colormap,
                                       "background","Background");
    
    st->gc = XCreateGC(st->dpy, w, GCForeground|GCBackground, &gcv);
    
    st->delay = get_integer_resource (st->dpy, "delay", "Integer");
    if (st->delay < 0) st->delay = 0;
    
    st->hold = get_integer_resource (st->dpy, "hold", "Integer");
    if (st->hold < 0) st->hold = 0;
    
    st->clear = get_integer_resource (st->dpy, "clear", "Integer");
    if (st->clear < 0) st->clear = 0;

    st->logminwidth = get_integer_resource (st->dpy, "logminwidth", "Integer");
    if (st->logminwidth < 2) st->logminwidth = 2;

    st->shiftk = get_boolean_resource(st->dpy, "shift", "Boolean");

    st->xor = get_boolean_resource(st->dpy, "xor", "Boolean");

    /* always draw xor on mono. */
    if (mono_p || st->xor) {
	XSetFunction(st->dpy, st->gc, GXxor);
    }

    st->reset = 1;

    return st;
}

static unsigned long
munch_draw (Display *dpy, Window w, void *closure)
{
  struct state *st = (struct state *) closure;
  int this_delay = st->delay;

  if (st->reset)
    {
      st->reset = 0;

      this_delay = st->hold;
      /* saves some calls to random.  big deal */
      st->randflags = random();

      /* choose size -- power of two */
      st->thiswidth = 1 << (st->logminwidth +
                        (random() % (1 + st->logmaxwidth - st->logminwidth)));

      if (st->clear && ++st->square_count >= st->clear) {
        XClearWindow(st->dpy, w);
        st->square_count = 0;
      }

      /* draw at this location */
      st->atX = (random() % (st->xgwa.width <= st->thiswidth ? 1
                         : st->xgwa.width - st->thiswidth));
      st->atY = (random() % (st->xgwa.height <= st->thiswidth ? 1
                         : st->xgwa.width - st->thiswidth));
		  
      /* wrap-around by these values; no need to %
         as we end up doing that later anyway*/
      st->kX = ((st->shiftk && (st->randflags & SHIFT_KX))
            ? (random() % st->thiswidth) : 0);
      st->kT = ((st->shiftk && (st->randflags & SHIFT_KT))
            ? (random() % st->thiswidth) : 0);
      st->kY = ((st->shiftk && (st->randflags & SHIFT_KY))
            ? (random() % st->thiswidth) : 0);
		  
      /* set the gravity of the munch, or rather,
         which direction we draw stuff in. */
      st->grav = (st->randflags & GRAV);
    }

  if (munchOnce (st, st->thiswidth))
    st->reset = 1;

/*  printf("%d\n",this_delay);*/
  return this_delay;
}

static void
munch_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  st->xgwa.width = w;
  st->xgwa.height = h;
  st->logmaxwidth = (int)
    dumb_log_2(st->xgwa.height < st->xgwa.width ? st->xgwa.height : st->xgwa.width);
  st->maxwidth = 1 << st->logmaxwidth;
}

static Bool
munch_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
munch_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}


static const char *munch_defaults [] = {
    ".background:	black",
    ".foreground:	white",
    "*delay:	        5000",
    "*hold:		100000",
    "*clear:		50",
    "*logminwidth:	7",
    "*shift:		True",
    "*xor:		True",
    0
};

static XrmOptionDescRec munch_options [] = {
    { "-delay",		".delay",	XrmoptionSepArg, 0 },
    { "-hold",		".hold",	XrmoptionSepArg, 0 },
    { "-clear",         ".clear",       XrmoptionSepArg, "true" },
    { "-shift",         ".shift",       XrmoptionNoArg, "true" },
    { "-no-shift",	".shift",       XrmoptionNoArg, "false" },
    { "-logminwidth",	".logminwidth", XrmoptionSepArg, 0 },
    { "-xor",           ".xor",		XrmoptionNoArg, "true" },
    { "-no-xor",	".xor",		XrmoptionNoArg, "false" },
    { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Munch", munch)
