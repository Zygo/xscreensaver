/*
 * pedal
 *
 * Based on a program for some old PDP-11 Graphics Display Processors
 * at CMU.
 *
 * X version by
 *
 *  Dale Moore  <Dale.Moore@cs.cmu.edu>
 *  24-Jun-1994
 *
 *  Copyright (c) 1994, by Carnegie Mellon University.  Permission to use,
 *  copy, modify, distribute, and sell this software and its documentation
 *  for any purpose is hereby granted without fee, provided fnord that the
 *  above copyright notice appear in all copies and that both that copyright
 *  notice and this permission notice appear in supporting documentation.
 *  No representations are made about the  suitability of fnord this software
 *  for any purpose.  It is provided "as is" without express or implied
 *  warranty.
 */

#include <math.h>
#include <stdlib.h>
#include "screenhack.h"

/* If MAXLINES is too big, we might not be able to get it
 * to the X server in the 2byte length field. Must be less
 * than 16k
 */
#define MAXLINES (16 * 1024)
#define MAXPOINTS MAXLINES
XPoint *points;

/* 
 * If the pedal has only this many lines, it must be ugly and we dont
 * want to see it.
 */
#define MINLINES 7

static int sizex, sizey;
static int delay;
static int fadedelay;
static int maxlines;
static GC gc;
static XColor foreground, background;
static Colormap cmap;

static Bool fade_p;


/*
 * Routine (Macro actually)
 *   mysin
 * Description:
 *   Assume that degrees is .. oh 360... meaning that
 *   there are 360 degress in a circle.  Then this function
 *   would return the sin of the angle in degrees.  But lets
 *   say that they are really big degrees, with 4 big degrees
 *   the same as one regular degree.  Then this routine
 *   would be called mysin(t, 90) and would return sin(t degrees * 4)
 */
#define mysin(t, degrees) sin(t * 2 * M_PI / (degrees))
#define mycos(t, degrees) cos(t * 2 * M_PI / (degrees))

/*
 * Macro:
 *   rand_range
 * Description:
 *   Return a random number between a inclusive  and b exclusive.
 *    rand (3, 6) returns 3 or 4 or 5, but not 6.
 */
#define rand_range(a, b) (a + random() % (b - a))


static int
gcd(int m, int n) /* Greatest Common Divisor (also Greates common factor). */
{
    int r;

    for (;;) {
        r = m % n;
        if (r == 0) return (n);
        m = n;
        n = r;
    }
}

static int numlines (int a, int b, int d)
/*
 * Description:
 *
 *      Given parameters a and b, how many lines will we have to draw?
 *
 * Algorithm:
 *
 *      This algorithm assumes that r = sin (theta * a), where we
 *      evaluate theta on multiples of b.
 *
 *      LCM (i, j) = i * j / GCD (i, j);
 *
 *      So, at LCM (b, 360) we start over again.  But since we
 *      got to LCM (b, 360) by steps of b, the number of lines is
 *      LCM (b, 360) / b.
 *
 *      If a is odd, then at 180 we cross over and start the
 *      negative.  Someone should write up an elegant way of proving
 *      this.  Why?  Because I'm not convinced of it myself. 
 *
 */
{
#define odd(x) (x & 1)
#define even(x) (!odd(x))
    if ( odd(a) && odd(b) && even(d)) d /= 2;
    return  (d / gcd (d, b));
#undef odd
}

static int
compute_pedal(XPoint *points, int maxpoints)
/*
 * Description:
 *
 *    Basically, it's combination spirograph and string art.
 *    Instead of doing lines, we just use a complex polygon,
 *    and use an even/odd rule for filling in between.
 *
 *    The spirograph, in mathematical terms is a polar
 *    plot of the form r = sin (theta * c);
 *    The string art of this is that we evaluate that
 *    function only on certain multiples of theta.  That is
 *    we let theta advance in some random increment.  And then
 *    we draw a straight line between those two adjacent points.
 *
 *    Eventually, the lines will start repeating themselves
 *    if we've evaluated theta on some rational portion of the
 *    whole.
 *
 *    The number of lines generated is limited to the
 *    ratio of the increment we put on theta to the whole.
 *    If we say that there are 360 degrees in a circle, then we
 *    will never have more than 360 lines.   
 *
 * Return:
 *
 *    The number of points.
 *
 */
{
    int a, b, d;  /* These describe a unique pedal */

    double r;
    int theta = 0;
    XPoint *pp = points;
    int count;
    int numpoints;

    /* Just to make sure that this division is not done inside the loop */
    int h_width = sizex / 2, h_height = sizey / 2 ;

    for (;;) {
	d = rand_range (MINLINES, maxlines);

	a = rand_range (1, d);
	b = rand_range (1, d);
	numpoints = numlines(a, b, d);
	if (numpoints > MINLINES) break;
    }

    /* it might be nice to try to move as much sin and cos computing
     * (or at least the argument computing) out of the loop.
     */
    for (count = numpoints; count-- ; )
    {
        r = mysin (theta * a, d);

        /* Convert from polar to cartesian coordinates */
	/* We could round the results, but coercing seems just fine */
        pp->x = mysin (theta, d) * r * h_width  + h_width;
        pp->y = mycos (theta, d) * r * h_height + h_height;

        /* Advance index into array */
        pp++;

        /* Advance theta */
        theta += b;
        theta %= d;
    }

    return(numpoints);
}

static void
init_pedal (Display *dpy, Window window)
{
  XGCValues gcv;
  XWindowAttributes xgwa;

  fade_p = !mono_p;

  delay = get_integer_resource ("delay", "Integer");
  if (delay < 0) delay = 0;

  fadedelay = get_integer_resource ("fadedelay", "Integer");
  if (fadedelay < 0) fadedelay = 0;

  maxlines = get_integer_resource ("maxlines", "Integer");
  if (maxlines < MINLINES) maxlines = MINLINES;
  else if (maxlines > MAXLINES) maxlines = MAXLINES;

  points = (XPoint *)malloc(sizeof(XPoint) * maxlines);

  XGetWindowAttributes (dpy, window, &xgwa);
  sizex = xgwa.width;
  sizey = xgwa.height;

  if ((xgwa.visual->class != GrayScale) && (xgwa.visual->class != PseudoColor))
    fade_p = False;

  cmap = xgwa.colormap;

  gcv.function = GXcopy;
  gcv.foreground = get_pixel_resource ("foreground", "Foreground", dpy, cmap);
  gcv.background = get_pixel_resource ("background", "Background", dpy, cmap);
  gc = XCreateGC (dpy, window, GCForeground | GCBackground |GCFunction, &gcv);

  if (fade_p)
  {
      int status;
      foreground.pixel = gcv.foreground;
      XQueryColor (dpy, cmap, &foreground);

      status = XAllocColorCells (
			dpy,
			cmap,
			0,
			NULL,
			0,
			&foreground.pixel,
			1);
      if (status)
      {
	  XStoreColor ( dpy, cmap, &foreground);
	  XSetForeground (dpy, gc, foreground.pixel);

	  background.pixel = gcv.background;
	  XQueryColor (dpy, cmap, &background);
      }
      else
      {
	  /* If we cant allocate a color cell, then just forget the
           * whole fade business.
           */
	  fade_p = False;
      }
  }
}

static void
fade_foreground (Display *dpy, Colormap cmap,
		 XColor from, XColor to, int steps)
/*
 * This routine assumes that we have a writeable colormap.
 * That means that the default colormap is not full, and that
 * the visual class is PseudoColor or GrayScale.
 */
{
    int i;
    XColor inbetween;
    int udelay = fadedelay / (steps + 1);

    inbetween = foreground;
    for (i = 0; i <= steps; i++ )
    {
      inbetween.red   = from.red   + (to.red   - from.red)   * i / steps ;
      inbetween.green = from.green + (to.green - from.green) * i / steps ;
      inbetween.blue  = from.blue  + (to.blue  - from.blue)  * i / steps ;
      XStoreColor (dpy, cmap, &inbetween);
      /* If we don't sync, these can bunch up */
      XSync(dpy, 0);
      usleep(udelay);
    }
}

static void
pedal (Display *dpy, Window window)
/*
 *    Since the XFillPolygon doesn't require that the last
 *    point == first point, the number of points is the same
 *    as the number of lines.  We just let XFillPolygon supply
 *    the line from the last point to the first point.
 *
 */
{
   int numpoints;

   numpoints = compute_pedal(points, maxlines);

   /* Fade out, make foreground the same as background */
   if (fade_p)
     fade_foreground (dpy, cmap, foreground, background, 32);

    /* Clear the window of previous garbage */
    XClearWindow (dpy, window);

    XFillPolygon (
                dpy,
                window,
                gc,
                points,
                numpoints,
                Complex,
                CoordModeOrigin);

   /* Pick a new foreground color (added by jwz) */
   if (! mono_p)
     {
       XColor color;
       hsv_to_rgb (random()%360, 1.0, 1.0,
		   &color.red, &color.green, &color.blue);
       XSync(dpy, 0);
       if (fade_p)
	 {
	   foreground.red = color.red;
	   foreground.green = color.green;
	   foreground.blue = color.blue;
	   /* don't do this here -- let fade_foreground() bring it up! */
	   /* XStoreColor (dpy, cmap, &foreground); */
	 }
       else if (XAllocColor (dpy, cmap, &color))
	 {
	   XSetForeground (dpy, gc, color.pixel);
	   XFreeColors (dpy, cmap, &foreground.pixel, 1, 0);
	   foreground.red = color.red;
	   foreground.green = color.green;
	   foreground.blue = color.blue;
	   foreground.pixel = color.pixel;
	 }
       XSync(dpy, 0);
     }

    /* Fade in by bringing the foreground back from background */
    if (fade_p)
       fade_foreground (dpy, cmap, background, foreground, 32);
}


char *progclass = "Pedal";

/*
 * If we are trying to save the screen, the background
 * should be dark.
 */
char *defaults [] = {
  ".background:			black",
  ".foreground:			white",
  "*delay:			5",
  "*fadedelay:			200000",
  "*maxlines:			1000",
  0
};

XrmOptionDescRec options [] = {
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-fadedelay",	".fadedelay",		XrmoptionSepArg, 0 },
  { "-maxlines",	".maxlines",		XrmoptionSepArg, 0 },
  { "-foreground",      ".foreground",          XrmoptionSepArg, 0 },
  { "-background",      ".background",          XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

void
screenhack (Display *dpy, Window window)
{
    init_pedal (dpy, window);
    for (;;) {
	pedal (dpy, window);
	XSync(dpy, 0);
	if (delay) sleep (delay);
    }
}
