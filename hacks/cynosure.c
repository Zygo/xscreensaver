/* cynosure --- draw some rectangles
 *
 * 01-aug-96: written in Java by ozymandias G desiderata <ogd@organic.com>
 * 25-dec-97: ported to C and XScreenSaver by Jamie Zawinski <jwz@netscape.com>
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
static Display *dpy;
static Window window;
static XColor *colors;
static int ncolors;
static int fg_pixel, bg_pixel;
static GC fg_gc, bg_gc, shadow_gc;

static void paint(void);
static int genNewColor(void);
static int genConstrainedColor(int base, int tweak);
static int c_tweak(int base, int tweak);

/**
 * The current color that is being tweaked to create the
 * rectangles.
 **/
static int curColor;

/**
 * A variable used for the progression of the colors (yes, I know
 * that's a lame explanation, but if your read the source, it should
 * become obvious what I'm doing with this variable).
 **/
static int curBase;

/**
 * The width of the right and bottom edges of the rectangles.
 **/
static int   shadowWidth;

/* The offset of the dropshadow beneath the rectangles. */
static int   elevation;

/**
 * The approximate amount of time that will elapse before the base
 * color is permanently changed.
 *
 * @see #tweak
 **/
static int   sway;

/**
 * The counter of time left until the base color value used. This class
 * variable is necessary because Java doesn't support static method
 * variables (grr grr).
 **/
static int   timeLeft;

/**
 * The amount by which the color of the polygons drawn will vary.
 *
 * @see #sway;
 **/
static int   tweak;

/**
 * The smallest size for an individual cell.
 **/
#define MINCELLSIZE 16

/**
 * The narrowest a rectangle can be.
 **/
#define MINRECTSIZE 6

/**
 * The size of the grid that the rectangles are placed within.
 **/
static int gridSize;

/**
 * Every so often genNewColor() generates a completely random
 * color. This variable sets how frequently that happens. It's
 * currently set to happen 1% of the time.
 *
 * @see #genNewColor
 **/
#define THRESHOLD 100 /*0.01*/


char *progclass = "Cynosure";
char *defaults [] = {
  "*background:		black",
  "*foreground:		white",
  "*delay:		500000",
  "*colors:		128",
  "*iterations:		100",
  "*shadowWidth:	2",
  "*elevation:		5",
  "*sway:		30",
  "*tweak:		20",
  "*gridSize:		12",
  0
};

XrmOptionDescRec options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-ncolors",		".colors",	XrmoptionSepArg, 0 },
  { "-iterations",	".iterations",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};


void screenhack(Display *d, Window w) 
{
  XWindowAttributes xgwa;
  XGCValues gcv;
  int delay;
  int i, iterations;

  dpy = d;
  window = w;

  curColor    = 0;
  curBase     = curColor;
  shadowWidth = get_integer_resource ("shadowWidth", "Integer");
  elevation   = get_integer_resource ("elevation", "Integer");
  sway        = get_integer_resource ("sway", "Integer");
  tweak       = get_integer_resource ("tweak", "Integer");
  gridSize    = get_integer_resource ("gridSize", "Integer");
  timeLeft    = 0;

  XGetWindowAttributes (dpy, window, &xgwa);

  ncolors = get_integer_resource ("colors", "Colors");
  if (ncolors < 2) ncolors = 2;
  if (ncolors <= 2) mono_p = True;

  if (mono_p)
    colors = 0;
  else
    colors = (XColor *) malloc(sizeof(*colors) * (ncolors+1));

  if (mono_p)
    ;
  else {
    make_smooth_colormap (dpy, xgwa.visual, xgwa.colormap, colors, &ncolors,
			  True, 0, True);
    if (ncolors <= 2) {
      mono_p = True;
      ncolors = 2;
      if (colors) free(colors);
      colors = 0;
    }
  }

  bg_pixel = get_pixel_resource("background", "Background", dpy,
				xgwa.colormap);
  fg_pixel = get_pixel_resource("foreground", "Foreground", dpy,
				xgwa.colormap);

  gcv.foreground = fg_pixel;
  fg_gc = XCreateGC(dpy, window, GCForeground, &gcv);
  gcv.foreground = bg_pixel;
  bg_gc = XCreateGC(dpy, window, GCForeground, &gcv);

  gcv.fill_style = FillStippled;
  gcv.stipple = XCreateBitmapFromData(dpy, window, "\125\252", 8, 2);
  shadow_gc = XCreateGC(dpy, window, GCForeground|GCFillStyle|GCStipple, &gcv);
  XFreePixmap(dpy, gcv.stipple);

  delay = get_integer_resource ("delay", "Delay");
  iterations = get_integer_resource ("iterations", "Iterations");

  i = 0;
  while (1)
    {
      if (iterations > 0 && ++i >= iterations)
	{
	  i = 0;
	  if (!mono_p)
	    XSetWindowBackground(dpy, window,
				 colors[random() % ncolors].pixel);
	  XClearWindow(dpy, window);
	}
      paint();
      XSync(dpy, False);
      if (delay)
	usleep(delay);
    }
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
static void paint(void)
{
    int i;
    int cellsWide, cellsHigh, cellWidth, cellHeight;
    static int width, height;
    static int size_check = 1;

    if (--size_check <= 0)
      {
	XWindowAttributes xgwa;
	XGetWindowAttributes (dpy, window, &xgwa);
	width = xgwa.width;
	height = xgwa.height;
	size_check = 1000;
      }

    /* How many cells wide the grid is (equal to gridSize +/- (gridSize / 2))
     */
    cellsWide  = c_tweak(gridSize, gridSize / 2);
    /* How many cells high the grid is (equal to gridSize +/- (gridSize / 2))
     */
    cellsHigh  = c_tweak(gridSize, gridSize / 2);
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
	  int c = genNewColor();
	  XSetForeground(dpy, fg_gc, colors[c].pixel);
	}

      for(j = 0; j < cellsWide; j++) {
	int curWidth, curHeight, curX, curY;

        /* Generate a random height for a rectangle and make sure that */
        /* it's above a certain minimum size */
        curHeight = random() % (cellHeight - shadowWidth);
        if (curHeight < MINRECTSIZE)
          curHeight = MINRECTSIZE;
        /* Generate a random width for a rectangle and make sure that
           it's above a certain minimum size */
        curWidth  = random() % (cellWidth  - shadowWidth);
        if (curWidth < MINRECTSIZE)
          curWidth = MINRECTSIZE;
        /* Figure out a random place to locate the rectangle within the
           cell */
        curY      = (i * cellHeight) + (random() % ((cellHeight - curHeight) -
						    shadowWidth));
        curX      = (j * cellWidth) +  (random() % ((cellWidth  - curWidth) -
						    shadowWidth));

        /* Draw the shadow */
	if (elevation > 0)
	  XFillRectangle(dpy, window, shadow_gc,
			 curX + elevation, curY + elevation,
			 curWidth, curHeight);

        /* Draw the edge */
	if (shadowWidth > 0)
	  XFillRectangle(dpy, window, bg_gc,
			 curX + shadowWidth, curY + shadowWidth,
			 curWidth, curHeight);

	XFillRectangle(dpy, window, fg_gc, curX, curY, curWidth, curHeight);

        /* Draw a 1-pixel black border around the rectangle */
	XDrawRectangle(dpy, window, bg_gc, curX, curY, curWidth, curHeight);
      }

    }
}


/**
 * genNewColor returns a new color, gradually mutating the colors and
 * occasionally returning a totally random color, just for variety.
 *
 * @return the new color
 **/
static int genNewColor(void)
{
    /* These lines handle "sway", or the gradual random changing of */
    /* colors. After genNewColor() has been called a given number of */
    /* times (specified by a random permutation of the tweak variable), */
    /* take whatever color has been most recently randomly generated and */
    /* make it the new base color. */
    if (timeLeft == 0) {
      timeLeft = c_tweak(sway, sway / 3);
      curColor = curBase;
    } else {
      timeLeft--;
    }
     
    /* If a randomly generated number is less than the threshold value,
       produce a "sport" color value that is completely unrelated to the 
       current palette. */
    if (0 == (random() % THRESHOLD)) {
      return (random() % ncolors);
    } else {
      curBase = genConstrainedColor(curColor, tweak);
      return curBase;
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
static int genConstrainedColor(int base, int tweak) 
{
    int i = 1 + (random() % tweak);
    if (random() & 1)
      i = -i;
    i = (base + i) % ncolors;
    while (i < 0)
      i += ncolors;
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
static int c_tweak(int base, int tweak) 
{
    int ranTweak = (random() % (2 * tweak));
    int n = (base + (ranTweak - tweak));
    if (n < 0) n = -n;
    return (n < 255 ? n : 255);
}
