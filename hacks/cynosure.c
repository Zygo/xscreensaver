/* cynosure --- draw some rectangles
 *
 * 01-aug-96: written in Java by ozymandias G desiderata <ogd@organic.com>
 * 25-dec-97: ported to C and XScreenSaver by Jamie Zawinski <jwz@jwz.org>
 *
 * Original version:
 *   http://www.organic.com/staff/ogd/java/cynosure.html
 *   http://www.organic.com/staff/ogd/java/source/cynosure/Cynosure-java.txt
 *
 * Original comments and copyright:
 *
 *   Cynosure.java
 *   A Java implementation of Stephen Linhart's Cynosure screen-saver as a
 *   drop-in class.
 *
 *   Header: /home/ogd/lib/cvs/aoaioxxysz/graphics/Cynosure.java,v 1.2 1996/08/02 02:41:21 ogd Exp 
 *
 *   ozymandias G desiderata <ogd@organic.com>
 *   Thu Aug  1 1996
 *
 *   COPYRIGHT NOTICE
 *
 *   Copyright 1996 ozymandias G desiderata. Title, ownership rights, and
 *   intellectual property rights in and to this software remain with
 *   ozymandias G desiderata. This software may be copied, modified,
 *   or used as long as this copyright is retained. Use this code at your
 *   own risk.
 *
 *   Revision: 1.2 
 *
 *   Log: Cynosure.java,v 
 *   Revision 1.2  1996/08/02 02:41:21  ogd
 *   Added a few more comments, fixed messed-up header.
 *
 *   Revision 1.1.1.1  1996/08/02 02:30:45  ogd
 *   First version
 */

#include "screenhack.h"

/* #define DO_STIPPLE */

struct state {
  Display *dpy;
  Window window;

  XColor *colors;
  int ncolors;

#ifndef DO_STIPPLE
  XColor *colors2;
  int ncolors2;
#endif

  int fg_pixel, bg_pixel;
  GC fg_gc, bg_gc, shadow_gc;

  int curColor;
  int curBase;	/* color progression */
  int   shadowWidth;
  int   elevation;	/* offset of dropshadow */
  int   sway;	/* time until base color changed */
  int   timeLeft;	/* until base color used */
  int   tweak;	/* amount of color variance */
  int gridSize;
  int iterations, i, delay;
  XWindowAttributes xgwa;
};


/**
 * The smallest size for an individual cell.
 **/
#define MINCELLSIZE 16

/**
 * The narrowest a rectangle can be.
 **/
#define MINRECTSIZE 6

/**
 * Every so often genNewColor() generates a completely random
 * color. This variable sets how frequently that happens. It's
 * currently set to happen 1% of the time.
 *
 * @see #genNewColor
 **/
#define THRESHOLD 100 /*0.01*/

static void paint(struct state *st);
static int genNewColor(struct state *st);
static int genConstrainedColor(struct state *st, int base, int tweak);
static int c_tweak(struct state *st, int base, int tweak);


static void *
cynosure_init (Display *d, Window w)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;

  st->dpy = d;
  st->window = w;

  st->curColor    = 0;
  st->curBase     = st->curColor;
  st->shadowWidth = get_integer_resource (st->dpy, "shadowWidth", "Integer");
  st->elevation   = get_integer_resource (st->dpy, "elevation", "Integer");
  st->sway        = get_integer_resource (st->dpy, "sway", "Integer");
  st->tweak       = get_integer_resource (st->dpy, "tweak", "Integer");
  st->gridSize    = get_integer_resource (st->dpy, "gridSize", "Integer");
  st->timeLeft    = 0;

  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);

  st->ncolors = get_integer_resource (st->dpy, "colors", "Colors");
  if (st->ncolors < 2) st->ncolors = 2;
  if (st->ncolors <= 2) mono_p = True;

  if (mono_p)
    st->colors = 0;
  else
    st->colors = (XColor *) malloc(sizeof(*st->colors) * (st->ncolors+1));

  if (mono_p)
    ;
  else {
    make_smooth_colormap (st->xgwa.screen, st->xgwa.visual, st->xgwa.colormap,
                          st->colors, &st->ncolors,
			  True, 0, True);
    if (st->ncolors <= 2) {
      mono_p = True;
      st->ncolors = 2;
      if (st->colors) free(st->colors);
      st->colors = 0;
    }
  }

  st->bg_pixel = get_pixel_resource(st->dpy,
				st->xgwa.colormap, "background", "Background");
  st->fg_pixel = get_pixel_resource(st->dpy,
				st->xgwa.colormap, "foreground", "Foreground");

  gcv.foreground = st->fg_pixel;
  st->fg_gc = XCreateGC(st->dpy, st->window, GCForeground, &gcv);
  gcv.foreground = st->bg_pixel;
  st->bg_gc = XCreateGC(st->dpy, st->window, GCForeground, &gcv);

#ifdef DO_STIPPLE
  gcv.fill_style = FillStippled;
  gcv.stipple = XCreateBitmapFromData(st->dpy, st->window, "\125\252", 8, 2);
  st->shadow_gc = XCreateGC(st->dpy, st->window, GCForeground|GCFillStyle|GCStipple, &gcv);
  XFreePixmap(st->dpy, gcv.stipple);

#else /* !DO_STIPPLE */
  st->shadow_gc = XCreateGC(st->dpy, st->window, GCForeground, &gcv);

#  ifdef HAVE_COCOA /* allow non-opaque alpha components in pixel values */
  jwxyz_XSetAlphaAllowed (st->dpy, st->shadow_gc, True);
#  endif

  if (st->colors)
    {
      int i;
      st->ncolors2 = st->ncolors;
      st->colors2 = (XColor *) malloc(sizeof(*st->colors2) * (st->ncolors2+1));

      for (i = 0; i < st->ncolors2; i++)
        {
#  ifdef HAVE_COCOA
          /* give a non-opaque alpha to the shadow colors */
          unsigned long pixel = st->colors[i].pixel;
          unsigned long amask = BlackPixelOfScreen (st->xgwa.screen);
          unsigned long a = (0x77777777 & amask);
          pixel = (pixel & (~amask)) | a;
          st->colors2[i].pixel = pixel;
#  else /* !HAVE_COCOA */
          int h;
          double s, v;
          rgb_to_hsv (st->colors[i].red,
                      st->colors[i].green,
                      st->colors[i].blue,
                      &h, &s, &v);
          v *= 0.4;
          hsv_to_rgb (h, s, v,
                      &st->colors2[i].red,
                      &st->colors2[i].green,
                      &st->colors2[i].blue);
          st->colors2[i].pixel = st->colors[i].pixel;
          XAllocColor (st->dpy, st->xgwa.colormap, &st->colors2[i]);
#  endif /* !HAVE_COCOA */
        }
    }
# endif /* !DO_STIPPLE */

  st->delay = get_integer_resource (st->dpy, "delay", "Delay");
  st->iterations = get_integer_resource (st->dpy, "iterations", "Iterations");

  return st;
}

static unsigned long
cynosure_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  if (st->iterations > 0 && ++st->i >= st->iterations)
    {
      st->i = 0;
      if (!mono_p)
        XSetWindowBackground(st->dpy, st->window,
                             st->colors[random() % st->ncolors].pixel);
      XClearWindow(st->dpy, st->window);
    }
  paint(st);

  return st->delay;
}


/**
 * paint adds a new layer of multicolored rectangles within a grid of
 * randomly generated size. Each row of rectangles is the same color,
 * but colors vary slightly from row to row. Each rectangle is placed
 * within a regularly-sized cell, but each rectangle is sized and
 * placed randomly within that cell.
 *
 * @param g      the Graphics coordinate in which to draw
 * @see #genNewColor
 **/
static void paint(struct state *st)
{
    int i;
    int cellsWide, cellsHigh, cellWidth, cellHeight;
    int width = st->xgwa.width;
    int height = st->xgwa.height;

    /* How many cells wide the grid is (equal to gridSize +/- (gridSize / 2))
     */
    cellsWide  = c_tweak(st, st->gridSize, st->gridSize / 2);
    /* How many cells high the grid is (equal to gridSize +/- (gridSize / 2))
     */
    cellsHigh  = c_tweak(st, st->gridSize, st->gridSize / 2);
    /* How wide each cell in the grid is */
    cellWidth  = width  / cellsWide;
    /* How tall each cell in the grid is */
    cellHeight = height / cellsHigh;

    /* Ensure that each cell is above a certain minimum size */

    if (cellWidth < MINCELLSIZE) {
      cellWidth  = MINCELLSIZE;
      cellsWide  = width / cellWidth;
    }

    if (cellHeight < MINCELLSIZE) {
      cellHeight = MINCELLSIZE;
      cellsHigh  = width / cellWidth;
    }

    /* fill the grid with randomly-generated cells */
    for(i = 0; i < cellsHigh; i++) {
      int j;

      /* Each row is a different color, randomly generated (but constrained) */
      if (!mono_p)
	{
	  int c = genNewColor(st);
	  XSetForeground(st->dpy, st->fg_gc, st->colors[c].pixel);
# ifndef DO_STIPPLE
          if (st->colors2)
            XSetForeground(st->dpy, st->shadow_gc, st->colors2[c].pixel);
# endif
	}

      for(j = 0; j < cellsWide; j++) {
	int curWidth, curHeight, curX, curY;

        /* Generate a random height for a rectangle and make sure that */
        /* it's above a certain minimum size */
        curHeight = random() % (cellHeight - st->shadowWidth);
        if (curHeight < MINRECTSIZE)
          curHeight = MINRECTSIZE;
        /* Generate a random width for a rectangle and make sure that
           it's above a certain minimum size */
        curWidth  = random() % (cellWidth  - st->shadowWidth);
        if (curWidth < MINRECTSIZE)
          curWidth = MINRECTSIZE;
        /* Figure out a random place to locate the rectangle within the
           cell */
        curY      = (i * cellHeight) + (random() % ((cellHeight - curHeight) -
						    st->shadowWidth));
        curX      = (j * cellWidth) +  (random() % ((cellWidth  - curWidth) -
						    st->shadowWidth));

        /* Draw the shadow */
	if (st->elevation > 0)
	  XFillRectangle(st->dpy, st->window, st->shadow_gc,
			 curX + st->elevation, curY + st->elevation,
			 curWidth, curHeight);

        /* Draw the edge */
	if (st->shadowWidth > 0)
	  XFillRectangle(st->dpy, st->window, st->bg_gc,
			 curX + st->shadowWidth, curY + st->shadowWidth,
			 curWidth, curHeight);

	XFillRectangle(st->dpy, st->window, st->fg_gc, curX, curY, curWidth, curHeight);

        /* Draw a 1-pixel black border around the rectangle */
	XDrawRectangle(st->dpy, st->window, st->bg_gc, curX, curY, curWidth, curHeight);
      }

    }
}


/**
 * genNewColor returns a new color, gradually mutating the colors and
 * occasionally returning a totally random color, just for variety.
 *
 * @return the new color
 **/
static int genNewColor(struct state *st)
{
    /* These lines handle "sway", or the gradual random changing of */
    /* colors. After genNewColor() has been called a given number of */
    /* times (specified by a random permutation of the tweak variable), */
    /* take whatever color has been most recently randomly generated and */
    /* make it the new base color. */
    if (st->timeLeft == 0) {
      st->timeLeft = c_tweak(st, st->sway, st->sway / 3);
      st->curColor = st->curBase;
    } else {
      st->timeLeft--;
    }
     
    /* If a randomly generated number is less than the threshold value,
       produce a "sport" color value that is completely unrelated to the 
       current palette. */
    if (0 == (random() % THRESHOLD)) {
      return (random() % st->ncolors);
    } else {
      st->curBase = genConstrainedColor(st, st->curColor, st->tweak);
      return st->curBase;
    }

}

/**
 * genConstrainedColor creates a random new color within a certain
 * range of an existing color. Right now this works with RGB color
 * values, but a future version of the program will most likely use HSV
 * colors, which should generate a more pleasing progression of values.
 *
 * @param base  the color on which the new color will be based
 * @param tweak the amount that the new color can be tweaked
 * @return a new constrained color
 * @see #genNewColor
 **/
static int genConstrainedColor(struct state *st, int base, int tweak) 
{
    int i = 1 + (random() % st->tweak);
    if (random() & 1)
      i = -i;
    i = (base + i) % st->ncolors;
    while (i < 0)
      i += st->ncolors;
    return i;
}

/**
 * Utility function to generate a tweaked color value
 *
 * @param  base   the byte value on which the color is based
 * @param  tweak  the amount the value will be skewed
 * @see    #tweak
 * @return the tweaked byte
 **/
static int c_tweak(struct state *st, int base, int tweak) 
{
    int ranTweak = (random() % (2 * tweak));
    int n = (base + (ranTweak - tweak));
    if (n < 0) n = -n;
    return (n < 255 ? n : 255);
}

static void
cynosure_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  st->xgwa.width = w;
  st->xgwa.height = h;
}

static Bool
cynosure_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (screenhack_event_helper (dpy, window, event))
    {
      st->i = st->iterations;
      return True;
    }
  return False;
}

static void
cynosure_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}


static const char *cynosure_defaults [] = {
  ".background:		black",
  ".foreground:		white",
  "*fpsSolid:		true",
  "*delay:		500000",
  "*colors:		128",
  "*iterations:		100",
  "*shadowWidth:	2",
  "*elevation:		5",
  "*sway:		30",
  "*tweak:		20",
  "*gridSize:		12",
#ifdef USE_IPHONE
  "*ignoreRotation:     True",
#endif
  0
};

static XrmOptionDescRec cynosure_options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-ncolors",		".colors",	XrmoptionSepArg, 0 },
  { "-iterations",	".iterations",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("Cynosure", cynosure)
