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

char *progclass = "Munch";

char *defaults [] = {
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

XrmOptionDescRec options [] = {
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


static GC gc;
/* only configurable things right now */
static int delay, hold, clear, logminwidth, shiftk, xor;

static void munchOnce (Display* dpy, Window w,
		       int width, /* pixels */
		       int atX, int atY, /* pixels */
		       int kX, int kT, int kY, /* pixels */
		       int grav /* 0 or not */) {

    int x, y, t;
    static Colormap cmap;
    XWindowAttributes xgwa;
    XColor fgc;
    int drawX, drawY;

    /*
    fprintf(stderr,"Doing width %d at %d %d shift %d %d %d grav %d\n",
	    width, atX, atY, kX, kT, kY, grav);
	    */
    
    XGetWindowAttributes (dpy, w, &xgwa);

    if (!mono_p) {
	/* XXX there are probably bugs with this. */
	cmap = xgwa.colormap;
   
	fgc.red = random() % 65535;
	fgc.green = random() % 65535;
	fgc.blue = random() % 65535;
    
	if (XAllocColor(dpy, cmap, &fgc)) {
	    XSetForeground(dpy, gc, fgc.pixel);
	}
    }
    
    /* Finally draw this munching square. */
    for(t = 0; t < width; t++) {
	for(x = 0; x < width; x++) {
	    /* figure out the next point */
	    y = ((x ^ ((t + kT) % width)) + kY) % width;

	    drawX = ((x + kX) % width) + atX;
	    drawY = (grav ? y + atY : atY + width - 1 - y);

	    /* used to be bugs where it would draw partially offscreen.
	       while that might be a pretty feature, I didn't want it to do
	       that yet.  if these trigger, please let me know.
	       */
/*	    assert(drawX >= 0 && drawX < xgwa.width);
	    assert(drawY >= 0 && drawY < xgwa.height);
*/

	    XDrawPoint(dpy, w, gc, drawX, drawY);
	    /* XXX may want to change this to XDrawPoints,
	       but it's fast enough without it for the moment. */
	    
	}
	
	/* we've drawn one pass' worth of points.  let the server catch up
	   or this won't be interesting to watch at all.  we call this here
	   in the hopes of having one whole sequence of dots appear at the
	   same time (one for each value of x, surprisingly enough)
	   */
	XSync(dpy, False);
        screenhack_handle_events (dpy);
	if (delay) usleep(delay);
    }
}

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

/* This parses arguments, initializes the window, etc., and finally starts
 * calling munchOnce ad infinitum.
 */
void
screenhack (dpy, w) Display *dpy; Window w;
{
    int logmaxwidth;
    int maxwidth;
    XWindowAttributes xgwa;
    Colormap cmap;
    XGCValues gcv;
    int n = 0;  /* number of squares before we have to clear */
    int randflags;
    int thiswidth;
    
    /* get the dimensions of the window */
    XGetWindowAttributes (dpy, w, &xgwa);
    
    /* We need a square; limit on screen size? */
    /* we want a power of 2 for the width or the munch doesn't fill up.
     */
    logmaxwidth = (int)
	dumb_log_2(xgwa.height < xgwa.width ? xgwa.height : xgwa.width);

    maxwidth = 1 << logmaxwidth;

    if (logmaxwidth < logminwidth) {
	/* off-by-one error here?  Anyone running on < 640x480? */
	fprintf(stderr, "munch: screen too small; use -logminwidth\n");
	fprintf(stderr, "\t(width is %d; log is %d; log must be at least "
		"%d)\n", 
		(xgwa.height < xgwa.width ? xgwa.height : xgwa.width),
		logmaxwidth, logminwidth);
	exit(0);
    }
    
    /* create the gc */
    cmap = xgwa.colormap;
    gcv.foreground= get_pixel_resource("foreground","Foreground",
				       dpy, cmap);
    gcv.background= get_pixel_resource("background","Background",
				       dpy, cmap);
    
    gc = XCreateGC(dpy, w, GCForeground|GCBackground, &gcv);
    
    delay = get_integer_resource ("delay", "Integer");
    if (delay < 0) delay = 0;
    
    hold = get_integer_resource ("hold", "Integer");
    if (hold < 0) hold = 0;
    
    clear = get_integer_resource ("clear", "Integer");
    if (clear < 0) clear = 0;

    logminwidth = get_integer_resource ("logminwidth", "Integer");
    if (logminwidth < 2) logminwidth = 2;

    shiftk = get_boolean_resource("shift", "Boolean");

    xor = get_boolean_resource("xor", "Boolean");

    /* always draw xor on mono. */
    if (mono_p || xor) {
	XSetFunction(dpy, gc, GXxor);
    }

    for(;;) {
	/* saves some calls to random.  big deal */
	randflags = random();

	/* choose size -- power of two */
	thiswidth = 1 << (logminwidth +
			  (random() % (1 + logmaxwidth - logminwidth)));

	munchOnce(dpy, w,
		  thiswidth, /* Width, in pixels */
		  
		  /* draw at this location */
		  (random() % (xgwa.width <= thiswidth ? 1
			       : xgwa.width - thiswidth)),
		  (random() % (xgwa.height <= thiswidth ? 1
			       : xgwa.width - thiswidth)),
		  
		  /* wrap-around by these values; no need to %
		     as we end up doing that later anyway*/
		  ((shiftk && (randflags & SHIFT_KX))
		   ? (random() % thiswidth) : 0),
		  ((shiftk && (randflags & SHIFT_KT))
		   ? (random() % thiswidth) : 0),
		  ((shiftk && (randflags & SHIFT_KY))
		   ? (random() % thiswidth) : 0),
		  
		  /* set the gravity of the munch, or rather,
		     which direction we draw stuff in. */
		  (randflags & GRAV)
		  );
	
        screenhack_handle_events (dpy);
	if (hold) usleep(hold);
	
	if (clear && ++n >= clear) {
	    XClearWindow(dpy, w);
	    n = 0;
	}
    }
}
