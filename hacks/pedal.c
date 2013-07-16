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
#include "erase.h"

/* If MAXLINES is too big, we might not be able to get it
 * to the X server in the 2byte length field. Must be less
 * than 16k
 */
#define MAXLINES (16 * 1024)
#define MAXPOINTS MAXLINES

/* 
 * If the pedal has only this many lines, it must be ugly and we dont
 * want to see it.
 */
#define MINLINES 7


struct state {
  Display *dpy;
  Window window;

  XPoint *points;

  int sizex, sizey;
  int delay;
  int maxlines;
  GC gc;
  XColor foreground, background;
  Colormap cmap;

  eraser_state *eraser;
  int erase_p;
};


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
compute_pedal(struct state *st, XPoint *points, int maxpoints)
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
    XPoint *pp = st->points;
    int count;
    int numpoints;

    /* Just to make sure that this division is not done inside the loop */
    int h_width = st->sizex / 2, h_height = st->sizey / 2 ;

    for (;;) {
	d = rand_range (MINLINES, st->maxlines);

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

static void *
pedal_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;
  XWindowAttributes xgwa;

  st->dpy = dpy;
  st->window = window;

  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  if (st->delay < 0) st->delay = 0;

  st->maxlines = get_integer_resource (st->dpy, "maxlines", "Integer");
  if (st->maxlines < MINLINES) st->maxlines = MINLINES;
  else if (st->maxlines > MAXLINES) st->maxlines = MAXLINES;

  st->points = (XPoint *)malloc(sizeof(XPoint) * st->maxlines);

  XGetWindowAttributes (st->dpy, st->window, &xgwa);
  st->sizex = xgwa.width;
  st->sizey = xgwa.height;

  st->cmap = xgwa.colormap;

  gcv.function = GXcopy;
  gcv.foreground = get_pixel_resource (st->dpy, st->cmap, "foreground", "Foreground");
  gcv.background = get_pixel_resource (st->dpy, st->cmap, "background", "Background");
  st->gc = XCreateGC (st->dpy, st->window, GCForeground | GCBackground |GCFunction, &gcv);

  return st;
}

/*
 *    Since the XFillPolygon doesn't require that the last
 *    point == first point, the number of points is the same
 *    as the number of lines.  We just let XFillPolygon supply
 *    the line from the last point to the first point.
 *
 */
static unsigned long
pedal_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int numpoints;
  int erase_delay = 10000;
  int long_delay = 1000000 * st->delay;

  if (st->erase_p || st->eraser) {
    st->eraser = erase_window (dpy, window, st->eraser);
    st->erase_p = 0;
    return (st->eraser ? erase_delay : 1000000);
  }

  numpoints = compute_pedal(st, st->points, st->maxlines);

  /* Pick a new foreground color (added by jwz) */
  if (! mono_p)
    {
      XColor color;
      hsv_to_rgb (random()%360, 1.0, 1.0,
                  &color.red, &color.green, &color.blue);
      if (XAllocColor (st->dpy, st->cmap, &color))
        {
          XSetForeground (st->dpy, st->gc, color.pixel);
          XFreeColors (st->dpy, st->cmap, &st->foreground.pixel, 1, 0);
          st->foreground.red = color.red;
          st->foreground.green = color.green;
          st->foreground.blue = color.blue;
          st->foreground.pixel = color.pixel;
        }
    }

  XFillPolygon (st->dpy, st->window, st->gc, st->points, numpoints,
                Complex, CoordModeOrigin);

  st->erase_p = 1;
  return long_delay;
}

static void
pedal_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  st->sizex = w;
  st->sizey = h;
}

static Bool
pedal_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
pedal_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}




/*
 * If we are trying to save the screen, the background
 * should be dark.
 */
static const char *pedal_defaults [] = {
  ".background:			black",
  ".foreground:			white",
  "*fpsSolid:			true",
  "*delay:			5",
  "*maxlines:			1000",
#ifdef USE_IPHONE
  "*ignoreRotation:             True",
#endif
  0
};

static XrmOptionDescRec pedal_options [] = {
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-maxlines",	".maxlines",		XrmoptionSepArg, 0 },
  { "-foreground",      ".foreground",          XrmoptionSepArg, 0 },
  { "-background",      ".background",          XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Pedal", pedal)
