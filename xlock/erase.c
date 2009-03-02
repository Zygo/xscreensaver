#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)erase.c	5.03 2001/10/18 xlockmore";

#endif

/*-
 * erase.c: Erase the screen in various more or less interesting ways.
 *
 * (c) 1997 by Johannes Keukelaar <johannes@nada.kth.se>
 *
 * Revision History:
 * 23-Oct-2001: slide_lines added features by Jouk Jansen:
 *                horizontal & vertical slicing
 *                random slection of slice thickness
 * 18-Oct-2001: slide_lines & losira added
 * 01-Nov-2000: Allocation checks
 * 17-May-1999: changed timing by Jouk Jansen
 * 13-Aug-1998: changed to be used with xlockmore by Jouk Jansen
 *              <joukj@hrem.stm.tudelft.nl>
 * 1997: original version by Johannes Keukelaar <johannes@nada.kth.se>
 *
 * Permission to use in any way granted. Provided "as is" without expressed
 * or implied warranty. NO WARRANTY, NO EXPRESSION OF SUITABILITY FOR ANY
 * PURPOSE. (I.e.: Use in any way, but at your own risk!)
 */

#include "xlock.h"

#undef countof
#define countof(x) (sizeof(x)/sizeof(*(x)))

extern int erasedelay, erasetime;
extern void erasemodefromname(char *name, Bool verbose);
static int advanceErase();

typedef void (*Eraser) (Display * dpy, Window window, GC gc,
			int width, int height, int delay, int granularity);

static void
no_fade(Display * dpy, Window window, GC gc,
	int width, int height, int delay, int granularity)
{
}

static void
random_lines(Display * dpy, Window window, GC gc,
	     int width, int height, int delay, int granularity)
{
#define ERASEMODE "random_lines"
	Bool        horiz_p = (Bool) (LRAND() & 1);
	int         max = (horiz_p ? height : width);
	int        *lines;
	int         i;
	int         actual_delay = delay / max;

#include "erase_init.h"

	if ((lines = (int *) calloc(max, sizeof (int))) == NULL) {
		XDrawRectangle(dpy, window, gc, 0, 0, width, height);
		return;
	}
	for (i = 0; i < max; i++)
		lines[i] = i;

	for (i = 0; i < max; i++) {
		int         t, r;

		t = lines[i];
		r = NRAND(max);
		lines[i] = lines[r];
		lines[r] = t;
	}

	for (i = 0; i < max; i++) {
		if (horiz_p)
			XDrawLine(dpy, window, gc, 0, lines[i], width,
				  lines[i]);
		else
			XDrawLine(dpy, window, gc, lines[i], 0, lines[i],
				  height);

		XSync(dpy, False);

#define LOOPVAR i
#include "erase.h"
#undef LOOPVAR

	}
	free(lines);
#include "erase_debug.h"
}
#undef ERASEMODE

static void
random_squares(Display * dpy, Window window, GC gc,
	       int width, int height, int delay, int granularity)
{
#define ERASEMODE "random_squares"
	int         randsize = MAX(1, MIN(width, height) / (16 + NRAND(32)));
	int         max = (height / randsize + 1) * (width / randsize + 1);
	int        *squares;
	int         i;
	int         columns = width / randsize + 1;  /* Add an extra for roundoff */
	int         actual_delay = delay / max;

#include "erase_init.h"

	if ((squares = (int *) calloc(max, sizeof (int))) == NULL) {
		XDrawRectangle(dpy, window, gc, 0, 0, width, height);
		return;
	}
	for (i = 0; i < max; i++)
		squares[i] = i;

	for (i = 0; i < max; i++) {
		int         t, r;

		t = squares[i];
		r = NRAND(max);
		squares[i] = squares[r];
		squares[r] = t;
	}

	for (i = 0; i < max; i++) {
		XFillRectangle(dpy, window, gc,
			       (squares[i] % columns) * randsize,
						 (squares[i] / columns) *
			       randsize, randsize, randsize);

		XSync(dpy, False);

#define LOOPVAR i
#include "erase.h"
#undef LOOPVAR

	}
	free(squares);
#include "erase_debug.h"
}
#undef ERASEMODE

static void
venetian(Display * dpy, Window window, GC gc,
	 int width, int height, int delay, int granularity)
{
#define ERASEMODE "venetian"
	Bool        horiz_p = (Bool) (LRAND() & 1);
	Bool        flip_p = (Bool) (LRAND() & 1);
	int         max = (horiz_p ? height : width);
	int        *lines;
	int         i, j;
	int         actual_delay = delay / max;

#include "erase_init.h"

/*	granularity /= 6;*/

	if ((lines = (int *) calloc(max, sizeof (int))) == NULL) {
		XDrawRectangle(dpy, window, gc, 0, 0, width, height);
		return;
	}
	j = 0;
	for (i = 0; i < max * 2; i++) {
		int         line = ((i / 16) * 16) - ((i % 16) * 15);

		if (line >= 0 && line < max)
			lines[j++] = (flip_p ? max - line : line);
	}

	for (i = 0; i < max; i++) {
		if (horiz_p)
			XDrawLine(dpy, window, gc, 0, lines[i], width,
				  lines[i]);
		else
			XDrawLine(dpy, window, gc, lines[i], 0, lines[i],
				  height);

		XSync(dpy, False);

#define LOOPVAR i
#include "erase.h"
#undef LOOPVAR

	}
	free(lines);
#include "erase_debug.h"
}
#undef ERASEMODE

static void
triple_wipe(Display * dpy, Window window, GC gc,
	    int width, int height, int delay, int granularity)
{
#define ERASEMODE "triple_wipe"
	Bool        flip_x = (Bool) (LRAND() & 1);
	Bool        flip_y = (Bool) (LRAND() & 1);
	int         max = width + (height / 2);
	int        *lines;
	int         i;
	int         actual_delay = delay / max;

#include "erase_init.h"

	if ((lines = (int *) calloc(max, sizeof (int))) == NULL) {
		XDrawRectangle(dpy, window, gc, 0, 0, width, height);
		return;
	}
	for (i = 0; i < width / 2; i++)
		lines[i] = i * 2 + height;
	for (i = 0; i < height / 2; i++)
		lines[i + width / 2] = i * 2;
	for (i = 0; i < width / 2; i++)
		lines[i + width / 2 + height / 2] = width - i * 2 - (width % 2 ?
0 : 1) + height;

/*	granularity /= 6;*/

	for (i = 0; i < max; i++) {
		int         x, y, x2, y2;

		if (lines[i] < height)
			x = 0, y = lines[i], x2 = width, y2 = y;
		else
			x = lines[i] - height, y = 0, x2 = x, y2 = height;

		if (flip_x)
			x = width - x - 1, x2 = width - x2 - 1;
		if (flip_y)
			y = height - y - 1, y2 = height - y2 - 1;

		XDrawLine(dpy, window, gc, x, y, x2, y2);
		XSync(dpy, False);

#define LOOPVAR i
#include "erase.h"
#undef LOOPVAR

	}
	free(lines);
#include "erase_debug.h"
}
#undef ERASEMODE

static void
quad_wipe(Display * dpy, Window window, GC gc,
	  int width, int height, int delay, int granularity)
{
#define ERASEMODE "quad_wipe"
	Bool        flip_x = (Bool) (LRAND() & 1);
	Bool        flip_y = (Bool) (LRAND() & 1);
	int         max = width + height;
	int        *lines;
	int         i;
	int         actual_delay = delay / max;

#include "erase_init.h"

	if ((lines = (int *) calloc(max, sizeof (int))) == NULL) {
		XDrawRectangle(dpy, window, gc, 0, 0, width, height);
		return;
	}
/*	granularity /= 3;*/

	for (i = 0; i < max / 4; i++) {
		lines[i * 4] = i * 2;
		lines[i * 4 + 1] = height - i * 2 - (height % 2 ? 0 : 1);
		lines[i * 4 + 2] = height + i * 2;
		lines[i * 4 + 3] = height + width - i * 2 - (width % 2 ? 0 : 1);
	}

	for (i = 0; i < max; i++) {
		int         x, y, x2, y2;

		if (lines[i] < height)
			x = 0, y = lines[i], x2 = width, y2 = y;
		else
			x = lines[i] - height, y = 0, x2 = x, y2 = height;

		if (flip_x)
			x = width - x, x2 = width - x2;
		if (flip_y)
			y = height - y, y2 = height - y2;

		XDrawLine(dpy, window, gc, x, y, x2, y2);
		XSync(dpy, False);

#define LOOPVAR i
#include "erase.h"
#undef LOOPVAR

	}
	free(lines);
#include "erase_debug.h"
}
#undef ERASEMODE

static void
circle_wipe(Display * dpy, Window window, GC gc,
	    int width, int height, int delay, int granularity)
{
#define ERASEMODE "circle_wipe"
	int         full = 360 * 64;
	int         inc = 360;
	int         start = NRAND(full);
	int         rad = (width > height ? width : height);
	int         i;
	int         actual_delay = delay * inc / full;

#include "erase_init.h"

	if (LRAND() & 1)
		inc = -inc;
	for (i = (inc > 0 ? 0 : full);
	     (inc > 0 ? i < full : i > 0);
	     i += inc) {
		XFillArc(dpy, window, gc,
		     (width / 2) - rad, (height / 2) - rad, rad * 2, rad * 2,
			 (i + start) % full, inc);
		XFlush(dpy);

#define LOOPVAR i
#include "erase.h"
#undef LOOPVAR

	}
#include "erase_debug.h"
}
#undef ERASEMODE

static void
three_circle_wipe(Display * dpy, Window window, GC gc,
		  int width, int height, int delay, int granularity)
{
#define ERASEMODE "three_circle_wipe"
	int         i;
	int         full = 360 * 64;
	int         q = full / 6;
	int         q2 = q * 2;
	int         inc = full / 240;
	int         start = NRAND(q);
	int         rad = (width > height ? width : height);
	int         actual_delay = delay * inc / q;

#include "erase_init.h"

	for (i = 0; i < q; i += inc) {
		XFillArc(dpy, window, gc, (width / 2) - rad, (height / 2) - rad,
rad * 2, rad * 2,
			 (start + i) % full, inc);
		XFillArc(dpy, window, gc, (width / 2) - rad, (height / 2) - rad,
rad * 2, rad * 2,
			 (start - i) % full, -inc);

		XFillArc(dpy, window, gc, (width / 2) - rad, (height / 2) - rad,
rad * 2, rad * 2,
			 (start + q2 + i) % full, inc);
		XFillArc(dpy, window, gc, (width / 2) - rad, (height / 2) - rad,
rad * 2, rad * 2,
			 (start + q2 - i) % full, -inc);

		XFillArc(dpy, window, gc, (width / 2) - rad, (height / 2) - rad,
rad * 2, rad * 2,
			 (start + q2 + q2 + i) % full, inc);
		XFillArc(dpy, window, gc, (width / 2) - rad, (height / 2) - rad,
rad * 2, rad * 2,
			 (start + q2 + q2 - i) % full, -inc);

		XSync(dpy, False);

#define LOOPVAR i
#include "erase.h"
#undef LOOPVAR

	}
#include "erase_debug.h"
}
#undef ERASEMODE

static void
squaretate(Display * dpy, Window window, GC gc,
	   int width, int height, int delay, int granularity)
{
#define ERASEMODE "squaretate"
#ifdef FAST_CPU
	int         steps = (((width > height ? width : height) * 2) /
granularity + 1);

#else
	int         steps = (((width > height ? width : height)) / (8 *
granularity) + 1);

#endif
	int         i;
	Bool        flip = (Bool) (LRAND() & 1);
	int         actual_delay = delay / steps;

#include "erase_init.h"

#define DRAW() \
      if (flip) { \
	points[0].x = width-points[0].x; \
	points[1].x = width-points[1].x; \
        points[2].x = width-points[2].x; } \
      XFillPolygon (dpy, window, gc, points, 3, Convex, CoordModeOrigin)

	for (i = 0; i < steps; i++) {
		XPoint      points[3];

		points[0].x = 0;
		points[0].y = 0;
		points[1].x = width;
		points[1].y = 0;
		points[2].x = 0;
		points[2].y = points[0].y + ((i * height) / steps);
		DRAW();

		points[0].x = 0;
		points[0].y = 0;
		points[1].x = 0;
		points[1].y = height;
		points[2].x = ((i * width) / steps);
		points[2].y = height;
		DRAW();

		points[0].x = width;
		points[0].y = height;
		points[1].x = 0;
		points[1].y = height;
		points[2].x = width;
		points[2].y = height - ((i * height) / steps);
		DRAW();

		points[0].x = width;
		points[0].y = height;
		points[1].x = width;
		points[1].y = 0;
		points[2].x = width - ((i * width) / steps);
		points[2].y = 0;
		DRAW();

		XSync(dpy, True);

#define LOOPVAR i
#include "erase.h"
#undef LOOPVAR

	}
#undef DRAW
#include "erase_debug.h"
}
#undef ERASEMODE

static void
fizzle (Display *dpy, Window window, GC gc,
	    int width, int height, int delay, int granularity)
{
#define ERASEMODE "fizzle"
  /* These dimensions must be prime numbers.  They should be roughly the
     square root of the width and height. */
# define BX 31
# define BY 31
# define SIZE (BX*BY)

	int array[SIZE];
	int i, j;
	XPoint *skews;
	int nx, ny;
	int actual_delay = delay / SIZE;

#include "erase_init.h"

	/* Distribute the numbers [0,SIZE) randomly in the array */
	{
		int indices[SIZE];

		for (i = 0; i < SIZE; i++) {
			array[i] = -1;
			indices[i] = i;
		}

		for (i = 0; i < SIZE; i++) {
			j = (int) (LRAND() % (SIZE - i));
			array[indices[j]] = i;
			indices[j] = indices[SIZE - i - 1];
		}
	}

	/* nx, ny are the number of cells across and down, rounded up */
	nx = width  / BX + (0 == (width % BX) ? 0 : 1);
	ny = height / BY + (0 == (height % BY) ? 0 : 1);
	if ((skews = (XPoint *) malloc(nx * ny * sizeof (XPoint))) == NULL) {
		XDrawRectangle(dpy, window, gc, 0, 0, width, height);
		return;
	}
	for (i = 0; i < nx; i++) {
		for (j = 0; j < ny; j++) {
			skews[j * nx + i].x = (short) (LRAND() % BX);
			skews[j * nx + i].y = (short) (LRAND() % BY);
		}
	}

# define SKEWX(cx, cy) (skews[cy*nx + cx].x)
# define SKEWY(cx, cy) (skews[cy*nx + cx].y)

	for (i = 0; i < SIZE; i++) {
		int x = array[i] % BX;
		int y = array[i] / BX;
		int iy, cy;

		for (iy = 0, cy = 0; iy < height; iy += BY, cy++) {
			int ix, cx;

			for (ix = 0, cx = 0; ix < width; ix += BX, cx++) {
				int xx = ix + (SKEWX(cx, cy) + x*((cx%(BX-1))+1))%BX;
				int yy = iy + (SKEWY(cx, cy) + y*((cy%(BY-1))+1))%BY;

				XDrawPoint(dpy, window, gc, xx, yy);
			}
		}

		if ((BX-1) == (i%BX)) {
			XSync (dpy, False);
		}

#define LOOPVAR i
# include "erase.h"
#undef LOOPVAR
	}

# undef SKEWX
# undef SKEWY

	free(skews);

# undef BX
# undef BY
# undef SIZE
#include "erase_debug.h"
}

/* from Rick Campbell <rick@campbellcentral.org> */
#undef ERASEMODE
static void
spiral (Display *display, Window window, GC context,
        int width, int height, int delay, int granularity)
{
#define ERASEMODE "spiral"
# define SPIRAL_ERASE_PI_2 (M_PI + M_PI)
# define SPIRAL_ERASE_LOOP_COUNT (100)
# define SPIRAL_ERASE_ARC_COUNT (360.0)
# define SPIRAL_ERASE_ANGLE_INCREMENT (SPIRAL_ERASE_PI_2 /     \
SPIRAL_ERASE_ARC_COUNT)
# define SPIRAL_ERASE_DELAY (0)

	double angle;
	int arc_limit;
	int arc_max_limit;
	int length_step;
	XPoint points [3];
	int actual_delay;

#include "erase_init.h"

  angle = 0.0;
  arc_max_limit = (int) (ceil (sqrt((double) (width * width) + (height * height)))
                         / 2.0);
  length_step = ((arc_max_limit + SPIRAL_ERASE_LOOP_COUNT - 1) /
                 SPIRAL_ERASE_LOOP_COUNT);
  arc_max_limit += length_step;
  points [0].x = width / 2;
  points [0].y = height / 2;
  points [1].x = points [0].x + length_step;
  points [1].y = points [0].y;
  points [2].x = points [1].x;
  points [2].y = points [1].y;

	actual_delay = delay * length_step / arc_max_limit;

  for (arc_limit = length_step;
       arc_limit < arc_max_limit;
       arc_limit += length_step)
    {
      int arc_length = length_step;
      int length_base = arc_limit;
      for (angle = 0.0; angle < SPIRAL_ERASE_PI_2;
           angle += SPIRAL_ERASE_ANGLE_INCREMENT)
        {
          arc_length = length_base + (int) ((length_step * angle) /
                                      SPIRAL_ERASE_PI_2);
          points [1].x = points [2].x;
          points [1].y = points [2].y;
          points [2].x = points [0].x + (int)(cos (angle) * arc_length);
          points [2].y = points [0].y + (int)(sin (angle) * arc_length);
          XFillPolygon (display, window, context, points, 3, Convex,
                        CoordModeOrigin);
        }

#define LOOPVAR arc_limit
#include "erase.h"
#undef LOOPVAR

#if (SPIRAL_ERASE_DELAY != 0)
          (void) usleep (SPIRAL_ERASE_DELAY);
# endif /* (SPIRAL_ERASE_DELAY != 0) */
    }
# undef SPIRAL_ERASE_DELAY
# undef SPIRAL_ERASE_ANGLE_INCREMENT
# undef SPIRAL_ERASE_ARC_COUNT
# undef SPIRAL_ERASE_LOOP_COUNT
# undef SPIRAL_ERASE_PI_2
#include "erase_debug.h"
}

/* I first saw something like this, albeit in reverse, in an early Tetris
   implementation for the Mac.
    -- Torbjörn Andersson <torbjorn@dev.eurotime.se>
 */
#undef ERASEMODE
static void
slide_lines (Display *display, Window window, GC context,
        int width, int height, int delay, int granularity)
{
#define ERASEMODE "slide_lines"
   Bool horiz_p = (Bool) (LRAND() & 1);
   int max = width;
   int dy;
   int i;
   int actual_delay = delay / max;
   int tick_init = NRAND( 2 );
   int int1,int2;

#include "erase_init.h"
   if ( horiz_p )
     {
	max = width;
     }
   else
     {
	max = height;
	height = width;
	width = max;
     }
   int1 = height - 10;
   int2 = NRAND( int1 );
   dy = 10 + ( int2 * int2 ) / int1;

   XSetGraphicsExposures(display , context , False );
   for ( i = 0; i < max; i++ )
     {
	int tick = tick_init;
	int y;
	int from1 = i - 1;
	int to1 = i;
	int w = width - to1;
        int from2 = width - i + 1 - w;
        int to2 = width - i - w;

        for (y = 0; y < height; y += dy)
            {
              if ( ++tick & 1)
                {
		   if ( horiz_p  )
		     {
			XCopyArea (display, window, window, context, from1, y,
				   w, dy, to1, y);
			XFillRectangle (display, window, context, from1, y,
					to1-from1, dy);
		     }
		   else
		     {
			XCopyArea (display, window, window, context, y , from1,
				   dy, w, y, to1);
			XFillRectangle (display, window, context, y, from1,
					dy, to1-from1);
		     }
                }
              else
                {
		   if ( horiz_p )
		     {
			XCopyArea (display, window, window, context, from2, y,
				   w, dy, to2, y);
			XFillRectangle (display, window, context, from2+w, y,
					to2-from2, dy);
		     }
		   else
		     {
			XCopyArea (display, window, window, context, y, from2,
				   dy, w, y, to2);
			XFillRectangle (display, window, context, y, from2+w,
					dy, to2-from2);
		     }
                }
            }
	XSync( display , False );
#define LOOPVAR i
#include "erase.h"
#undef LOOPVAR
     }
#include "erase_debug.h"
}

/* from Frederick Roeber <roeber@xigo.com> */
#undef ERASEMODE
static void
losira (Display *dpy, Window window, GC gc, int width, int height, int delay,
	int granularity)
{
#define ERASEMODE "losira"
  XGCValues gcv;
  XWindowAttributes wa;
  GC white_gc;
  XArc arc[2][8];
  double xx[8], yy[8], dx[8], dy[8];
  int i;
  int max = width/2;
  int max_off = MAX(1, max / 12);
  int actual_delay = (int) (0.55 * delay / max);

#include "erase_init.h"

  (void) XGetWindowAttributes(dpy, window, &wa);
  gcv.foreground = XWhitePixelOfScreen(wa.screen);
  white_gc = XCreateGC(dpy, window, GCForeground, &gcv);

  XSetGraphicsExposures(dpy , gc , False );
  XSetGraphicsExposures(dpy , white_gc , False );

  /* Squeeze in from the sides */
  for ( i=0; i<max; i++ )
     {
        int off = ( i * max_off ) / max;
	int from1 = i - 1;
        int to1 = i;
        int w = max - to1 - off/2 + 1;
        int from2 = max+(to1-from1)+off/2;
        int to2 = max+off/2;

          if (w < 0)
            break;

          XCopyArea (dpy, window, window, gc, from1, 0, w, height, to1, 0);
          XCopyArea (dpy, window, window, gc, from2, 0, w, height, to2, 0);
          XFillRectangle (dpy, window, gc, from1, 0, (to1-from1), height);
          XFillRectangle (dpy, window, gc, to2+w, 0, from2+w,     height);
          XFillRectangle (dpy, window, white_gc, max-off/2, 0, off, height);
          XSync(dpy, False);
#define LOOPVAR i
#include "erase.h"
#undef LOOPVAR
     }


  XFillRectangle(dpy, window, white_gc, max-max_off/2, 0, max_off, height);

  /* Cap the top and bottom of the line */
  XFillRectangle(dpy, window, gc, max-max_off/2, 0, max_off, max_off/2);
  XFillRectangle(dpy, window, gc, max-max_off/2, height-max_off/2,
                 max_off, max_off/2);
  XFillArc(dpy, window, white_gc, max-max_off/2-1, 0,
           max_off-1, max_off-1, 0, 180*64);
  XFillArc(dpy, window, white_gc, max-max_off/2-1, height-max_off,
           max_off-1, max_off-1,
           180*64, 360*64);

  XFillRectangle(dpy, window, gc, 0,               0, max-max_off/2, height);
  XFillRectangle(dpy, window, gc, max+max_off/2-1, 0, max-max_off/2, height);
  XSync(dpy, False);

  /* Collapse vertically */

  max = height/2;
  actual_delay = (int) (0.30 * delay / max);
  for ( i=0; i<max; i++ )
     {
	int x = (width-max_off)/2;
        int w = max_off;
        int off =  ( i * max_off ) / max;
        int from1 = i - 1;
        int to1 = i;
        int h = max - to1 - off/2;
        int from2 = max+(to1-from1)+off/2;
        int to2 = max+off/2;

        if (h < max_off/2)
            break;

          XCopyArea (dpy, window, window, gc, x, from1, w, h, x, to1);
          XCopyArea (dpy, window, window, gc, x, from2, w, h, x, to2);
          XFillRectangle(dpy, window, gc, x, from1, w, (to1 - from1));
          XFillRectangle(dpy, window, gc, x, to2+h, w, (to2 - from2));
          XSync(dpy, False);
#define LOOPVAR i
#include "erase.h"
#undef LOOPVAR
     }

  /* "This is Sci-Fi" */
  for( i = 0; i < 8; i++ ) {
    arc[0][i].width = arc[0][i].height = max_off;
    arc[1][i].width = arc[1][i].height = max_off;
    arc[0][i].x = arc[1][i].x = width/2;
    arc[0][i].y = arc[1][i].y = height/2;
    xx[i] = (double)(width/2)  - max_off/2;
    yy[i] = (double)(height/2) - max_off/2;
  }

  arc[0][0].angle1 = arc[1][0].angle1 =   0*64; arc[0][0].angle2 = arc[1][0].angle2 =  45*64;
  arc[0][1].angle1 = arc[1][1].angle1 =  45*64; arc[0][1].angle2 = arc[1][1].angle2 =  45*64;
  arc[0][2].angle1 = arc[1][2].angle1 =  90*64; arc[0][2].angle2 = arc[1][2].angle2 =  45*64;
  arc[0][3].angle1 = arc[1][3].angle1 = 135*64; arc[0][3].angle2 = arc[1][3].angle2 =  45*64;
  arc[0][4].angle1 = arc[1][4].angle1 = 180*64; arc[0][4].angle2 = arc[1][4].angle2 =  45*64;
  arc[0][5].angle1 = arc[1][5].angle1 = 225*64; arc[0][5].angle2 = arc[1][5].angle2 =  45*64;
  arc[0][6].angle1 = arc[1][6].angle1 = 270*64; arc[0][6].angle2 = arc[1][6].angle2 =  45*64;
  arc[0][7].angle1 = arc[1][7].angle1 = 315*64; arc[0][7].angle2 = arc[1][7].angle2 =  45*64;

  for( i = 0; i < 8; i++ ) {
    dx[i] =  cos((i*45 + 22.5)/360 * 2*M_PI);
    dy[i] = -sin((i*45 + 22.5)/360 * 2*M_PI);
  }

  gcv.line_width = 3;
  XChangeGC(dpy, gc, GCLineWidth, &gcv);

  XClearWindow (dpy, window);
  XFillArc(dpy, window, white_gc,
           width/2-max_off/2-1, height/2-max_off/2-1,
           max_off-1, max_off-1,
           0, 360*64);
  XDrawLine(dpy, window, gc, 0, height/2-1, width, height/2-1);
  XDrawLine(dpy, window, gc, width/2-1, 0, width/2-1, height);
  XDrawLine(dpy, window, gc, width/2-1-max_off, height/2-1-max_off,
            width/2+max_off, height/2+max_off);
  XDrawLine(dpy, window, gc, width/2+max_off, height/2-1-max_off,
            width/2-1-max_off, height/2+max_off);

  XSync(dpy, False);


  /* Fan out */
  actual_delay = (int) (0.15 * delay / max_off);
  for ( i=0; i<max_off; i++ )
     {
	int j;
        for (j = 0; j < 8; j++)
            {
              xx[j] += 2*dx[j];
              yy[j] += 2*dy[j];
              arc[(i+1)%2][j].x = (short) xx[j];
              arc[(i+1)%2][j].y = (short) yy[j];
            }

          XFillRectangle (dpy, window, gc,
                          (width-max_off*5)/2, (height-max_off*5)/2,
                          max_off*5, max_off*5);
          XFillArcs(dpy, window, white_gc, arc[(i+1)%2], 8);
          XSync(dpy, False);
#define LOOPVAR i
#include "erase.h"
#undef LOOPVAR
     }
  XFreeGC(dpy, white_gc);
  gcv.line_width = 1;
  XChangeGC(dpy, gc, GCLineWidth, &gcv);
}

#undef ERASEMODE

static struct eraser_names {
	Eraser      mode;
	char       *name;
} erasers[] = {

	{
		random_lines, (char *) "random_lines"
	},
	{
		random_squares, (char *) "random_squares"
	},
	{
		venetian, (char *) "venetian"
	},
	{
		triple_wipe, (char *) "triple_wipe"
	},
	{
		quad_wipe, (char *) "quad_wipe"
	},
	{
		circle_wipe, (char *) "circle_wipe"
	},
	{
		three_circle_wipe, (char *) "three_circle_wipe"
	},
	{
		squaretate, (char *) "squaretate"
	},
	{
		fizzle, (char *) "fizzle"
	},
	{
		spiral, (char *) "spiral"
	},
	{
		slide_lines, (char *) "slide_lines"
	},
	{
		losira, (char *) "losira"
	},
	{
		no_fade, (char *) "no_fade"
	}
};


static void
erase_window(ModeInfo * mi, GC erase_gc, unsigned long pixel, int delay)
{
	int         granularity = 1, mode;

	XSetForeground(MI_DISPLAY(mi), erase_gc, pixel);
	if (MI_IS_DRAWN(mi) && delay > 0) {
#if 0
		if (mode < 0 || mode >= (int) countof(erasers))
			mode = NRAND(countof(erasers));
#else
		mode = advanceErase();
	        if (MI_IS_VERBOSE(mi))
			(void) fprintf(stdout, "erasemode %d: %s\n", mode,
				erasers[mode].name);
#endif
		(*(erasers[mode].mode)) (MI_DISPLAY(mi), MI_WINDOW(mi),
			erase_gc, MI_WIDTH(mi), MI_HEIGHT(mi),
			delay, granularity);
		MI_IS_DRAWN(mi) = False;
	}
	XFillRectangle(MI_DISPLAY(mi), MI_WINDOW(mi), erase_gc, 0, 0,
		  (unsigned int) MI_WIDTH(mi), (unsigned int) MI_HEIGHT(mi));
}

void
erase_full_window(ModeInfo * mi, GC erase_gc, unsigned long pixel)
{
	erase_window(mi, erase_gc, pixel, erasedelay);
}

#if 0
int
erasemodefromname(char *name, Bool verbose)
{
	int         a;
	char       *s1, *s2;
	int         eraseMode = -1;

	for (a = 0; a < (int) countof(erasers); a++) {
		for (s1 = erasers[a].name, s2 = name; *s1 && *s2; s1++, s2++)
			if ((isupper((int) *s1) ? tolower((int) *s1) : *s1) !=
			    (isupper((int) *s2) ? tolower((int) *s2) : *s2))
				break;

		if ((*s1 == '\0') || (*s2 == '\0')) {

			if (eraseMode != -1) {
				(void) fprintf(stderr,
					       "%s does not uniquely describe an erase mode (set to random)\n"
					       ,name);
				return (-1);
			}
			eraseMode = a;
		}
	}
	if (verbose)
		(void) fprintf(stderr, "Using erase mode %s\n", name);
	return (eraseMode);
}
#else

static int  currentEraseMode = -1;
/* static int  previousEraseMode = -1; */
static int *eraseModes;
static int  nEraseModes;
#ifdef DEBUG
static Bool sequential = True;
#else
static Bool sequential = False;
#endif

static int
pickMode(void)
{
	static int *eraseModeIndexes = (int *) NULL;
	static int  eraseModeCount = 0;
	static int  lastEraseMode = -1, lastIndex = -1;
	int         eraseMode, i;

	if (eraseModeIndexes == NULL) {
		if ((eraseModeIndexes = (int *) calloc(nEraseModes,
				sizeof (int))) == NULL) {
			if (sequential)
				return eraseModes[0];
			else
				return eraseModes[NRAND(nEraseModes)];
		}
	}
	if (eraseModeCount == 0) {
		for (i = 0; i < nEraseModes; i++) {
			eraseModeIndexes[i] = eraseModes[i];
		}
		eraseModeCount = nEraseModes;
	}
	if (eraseModeCount == 1) {
		/* only one left, let's use that one */
		lastIndex = -1;
		return (lastEraseMode = eraseModeIndexes[--eraseModeCount]);
	} else {
		/* pick a random slot in the list, check for last */
		if (sequential) {
			lastIndex = i = (lastIndex + 1) % nEraseModes;
		} else
			while (eraseModeIndexes[i = NRAND(eraseModeCount)] == lastEraseMode);
	}

	eraseMode = eraseModeIndexes[i];	/* copy out chosen eraseMode */
	/* move eraseMode at end of list to slot vacated by chosen eraseMode, dec count */
	eraseModeIndexes[i] = eraseModeIndexes[--eraseModeCount];
	return (lastEraseMode = eraseMode);	/* remember last eraseMode picked */
}

static char *
strpmtok(int *sign, char *str)
{
	static int  nextsign = 0;
	static char *loc;
	char       *p, *r;

	if (str)
		loc = str;
	if (nextsign) {
		*sign = nextsign;
		nextsign = 0;
	}
	p = loc - 1;
	for (;;) {
		switch (*++p) {
			case '+':
				*sign = 1;
				continue;
			case '-':
				*sign = -1;
				continue;
			case ' ':
			case ',':
			case '\t':
			case '\n':
				continue;
			case 0:
				loc = p;
				return (char *) NULL;
		}
		break;
	}
	r = p;

	for (;;) {
		switch (*++p) {
			case '+':
				nextsign = 1;
				break;
			case '-':
				nextsign = -1;
				break;
			case ' ':
			case ',':
			case '\t':
			case '\n':
			case 0:
				break;
			default:
				continue;
		}
		break;
	}
	if (*p) {
		*p = 0;
		loc = p + 1;
	} else
		loc = p;

	return r;
}

static Bool
parseEraseModeList(char * eraseModeList, Bool verbose)
{
	int         i, sign = 1;
	char       *p;

	if ((eraseModes = (int *) calloc(countof(erasers), sizeof (int))) == NULL) {
			return False;
	}
	p = strpmtok(&sign, (eraseModeList) ? eraseModeList : (char *) "");

	while (p) {
		if (!strcmp(p, "all")) {
			for (i = 0; i < (int) countof(erasers); i++) {
				eraseModes[i] = (sign > 0);
			}
		} else {
			for (i = 0; i < (int) countof(erasers); i++)
				if (!strcmp(p, erasers[i].name))
					break;
			if (i < (int) countof(erasers))
				eraseModes[i] = (sign > 0);
			else
				(void) fprintf(stderr, "unrecognized eraseMode \"%s\"\n", p);
		}
		p = strpmtok(&sign, (char *) NULL);
	}

	nEraseModes = 0;
	for (i = 0; i < (int) countof(erasers); i++)
		if (eraseModes[i])
			eraseModes[nEraseModes++] = i;
	if (!nEraseModes) {		/* empty list */
		for (i = 0; i < (int) countof(erasers); i++) {
			eraseModes[i] = i;
		}
		nEraseModes = (int) countof(erasers);
	}
	if (verbose) {
		(void) fprintf(stderr, "%d eraseMode%s: ", nEraseModes, ((nEraseModes == 1) ? "" : "s"));
		for (i = 0; i < nEraseModes; i++)
			(void) fprintf(stderr, "%s ", erasers[eraseModes[i]].name);
		(void) fprintf(stderr, "\n");
	}
	return True;
}

void
erasemodefromname(char *name, Bool verbose)
{
	if (currentEraseMode < 0) {
		if (!parseEraseModeList(name, verbose))
			return;
		currentEraseMode = pickMode();
	}
}


static int
advanceErase()
{
	if (currentEraseMode < 0)
		return 0;

	/* previousEraseMode = currentEraseMode; */
	currentEraseMode = pickMode();
	return currentEraseMode;
}
#endif
