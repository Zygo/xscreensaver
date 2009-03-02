#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)erase.c	5.00 2000/11/01 xlockmore";

#endif

/*-
 * erase.c: Erase the screen in various more or less interesting ways.
 *
 * (c) 1997 by Johannes Keukelaar <johannes@nada.kth.se>
 *
 * Revision History:
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

extern int  erasedelay, erasemode, erasetime;
extern int erasemodefromname(char *name);

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
	(void) free((void *) lines);
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
	(void) free((void *) squares);
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
	(void) free((void *) lines);
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
	(void) free((void *) lines);
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
	(void) free((void *) lines);
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
granularity);

#else
	int         steps = (((width > height ? width : height)) / (8 *
granularity));

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

	(void) free((void *) skews);

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
		fizzle , (char *) "fizzle"
	},
	{
		spiral , (char *) "spiral"
	},
	{
		no_fade, (char *) "no_fade"
	}
};


static void
erase_window(ModeInfo * mi, GC erase_gc, unsigned long pixel, int mode,
	     int delay)
{
	int         granularity = 1;

	XSetForeground(MI_DISPLAY(mi), erase_gc, pixel);
	if (MI_IS_DRAWN(mi) && delay > 0) {
		if (mode < 0 || mode >= (int) countof(erasers))
			mode = NRAND(countof(erasers));
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
	erase_window(mi, erase_gc, pixel, erasemode, erasedelay);
}

int
erasemodefromname(char *name)
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
	return (eraseMode);
}
