#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)erase.c	4.12 98/08/13 xlockmore";

#endif

/*-
 * erase.c: Erase the screen in various more or less interesting ways.
 *
 * (c) 1997 by Johannes Keukelaar <johannes@nada.kth.se>
 *
 * Revision History:
 *   1997      : original version by Johannes Keukelaar <johannes@nada.kth.se>
 *   13-8-1998 : changed to be used with xlockmore by Jouk Jansen
 *               <joukj@hrem.stm.tudelft.nl>
 *
 * Permission to use in any way granted. Provided "as is" without expressed
 * or implied warranty. NO WARRANTY, NO EXPRESSION OF SUITABILITY FOR ANY
 * PURPOSE. (I.e.: Use in any way, but at your own risk!)
 */

#include "xlock.h"

#undef countof
#define countof(x) (sizeof(x)/sizeof(*(x)))

extern int  erasedelay, erasemode;
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
	Bool        horiz_p = LRAND() & 1;
	int         max = (horiz_p ? height : width);
	int        *lines = (int *) calloc(max, sizeof (*lines));
	int         i;

#if HAVE_GETTIMEOFDAY
	struct timeval tp;
	int         interval, t_prev;

	GETTIMEOFDAY(&tp);
	t_prev = tp.tv_usec;
#endif
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
			XDrawLine(dpy, window, gc, 0, lines[i], width, lines[i]);
		else
			XDrawLine(dpy, window, gc, lines[i], 0, lines[i], height);

		XSync(dpy, False);
		if (delay > 0 && ((i % granularity) == 0))
#if HAVE_GETTIMEOFDAY
		{
			GETTIMEOFDAY(&tp);
			interval = tp.tv_usec - t_prev;
			if (interval <= 0)
				interval = interval + 1000000;
			interval = delay * granularity - interval;
			if (interval > 0)
				usleep(interval);
			GETTIMEOFDAY(&tp);
			t_prev = tp.tv_usec;
		}
#else
			usleep(delay * granularity);
#endif
	}
	(void) free((void *) lines);
}

static void
random_squares(Display * dpy, Window window, GC gc,
	       int width, int height, int delay, int granularity)
{
	int         randsize = MAX(1, MIN(width, height) / (16 + NRAND(32)));
	int         max = (height / randsize + 1) * (width / randsize + 1);
	int        *squares = (int *) calloc(max, sizeof (*squares));
	int         i;
	int         columns = width / randsize + 1;  /* Add an extra for roundoff */

#if HAVE_GETTIMEOFDAY
	struct timeval tp;
	int         interval, t_prev;

	GETTIMEOFDAY(&tp);
	t_prev = tp.tv_usec;
#endif
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
						 (squares[i] / columns) * randsize,
			       randsize, randsize);

		XSync(dpy, False);
		if (delay > 0 && ((i % granularity) == 0))
#if HAVE_GETTIMEOFDAY
		{
			GETTIMEOFDAY(&tp);
			interval = tp.tv_usec - t_prev;
			if (interval <= 0)
				interval = interval + 1000000;
			interval = delay * granularity - interval;
			if (interval > 0)
				usleep(interval);
			GETTIMEOFDAY(&tp);
			t_prev = tp.tv_usec;
		}
#else
			usleep(delay * granularity);
#endif
	}
	(void) free((void *) squares);
}

static void
venetian(Display * dpy, Window window, GC gc,
	 int width, int height, int delay, int granularity)
{
	Bool        horiz_p = LRAND() & 1;
	Bool        flip_p = LRAND() & 1;
	int         max = (horiz_p ? height : width);
	int        *lines = (int *) calloc(max, sizeof (*lines));
	int         i, j;

#if HAVE_GETTIMEOFDAY
	struct timeval tp;
	int         interval, t_prev;

	GETTIMEOFDAY(&tp);
	t_prev = tp.tv_usec;
#endif

	granularity /= 6;

	j = 0;
	for (i = 0; i < max * 2; i++) {
		int         line = ((i / 16) * 16) - ((i % 16) * 15);

		if (line >= 0 && line < max)
			lines[j++] = (flip_p ? max - line : line);
	}

	for (i = 0; i < max; i++) {
		if (horiz_p)
			XDrawLine(dpy, window, gc, 0, lines[i], width, lines[i]);
		else
			XDrawLine(dpy, window, gc, lines[i], 0, lines[i], height);

		XSync(dpy, False);
		if (delay > 0 && ((i % granularity) == 0))
#if HAVE_GETTIMEOFDAY
		{
			GETTIMEOFDAY(&tp);
			interval = tp.tv_usec - t_prev;
			if (interval <= 0)
				interval = interval + 1000000;
			interval = delay * granularity - interval;
			if (interval > 0)
				usleep(interval);
			GETTIMEOFDAY(&tp);
			t_prev = tp.tv_usec;
		}
#else
			usleep(delay * granularity);
#endif
	}
	(void) free((void *) lines);
}


static void
triple_wipe(Display * dpy, Window window, GC gc,
	    int width, int height, int delay, int granularity)
{
	Bool        flip_x = LRAND() & 1;
	Bool        flip_y = LRAND() & 1;
	int         max = width + (height / 2);
	int        *lines = (int *) calloc(max, sizeof (int));
	int         i;

#if HAVE_GETTIMEOFDAY
	struct timeval tp;
	int         interval, t_prev;

	GETTIMEOFDAY(&tp);
	t_prev = tp.tv_usec;
#endif

	for (i = 0; i < width / 2; i++)
		lines[i] = i * 2 + height;
	for (i = 0; i < height / 2; i++)
		lines[i + width / 2] = i * 2;
	for (i = 0; i < width / 2; i++)
		lines[i + width / 2 + height / 2] = width - i * 2 - (width % 2 ? 0 : 1) + height;

	granularity /= 6;

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
		if (delay > 0 && ((i % granularity) == 0))
#if HAVE_GETTIMEOFDAY
		{
			GETTIMEOFDAY(&tp);
			interval = tp.tv_usec - t_prev;
			if (interval <= 0)
				interval = interval + 1000000;
			interval = delay * granularity - interval;
			if (interval > 0)
				usleep(interval);
			GETTIMEOFDAY(&tp);
			t_prev = tp.tv_usec;
		}
#else
			usleep(delay * granularity);
#endif
	}
	(void) free((void *) lines);
}


static void
quad_wipe(Display * dpy, Window window, GC gc,
	  int width, int height, int delay, int granularity)
{
	Bool        flip_x = LRAND() & 1;
	Bool        flip_y = LRAND() & 1;
	int         max = width + height;
	int        *lines = (int *) calloc(max, sizeof (int));
	int         i;

#if HAVE_GETTIMEOFDAY
	struct timeval tp;
	int         interval, t_prev;

	GETTIMEOFDAY(&tp);
	t_prev = tp.tv_usec;
#endif

	granularity /= 3;

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
		if (delay > 0 && ((i % granularity) == 0))
#if HAVE_GETTIMEOFDAY
		{
			GETTIMEOFDAY(&tp);
			interval = tp.tv_usec - t_prev;
			if (interval <= 0)
				interval = interval + 1000000;
			interval = delay * granularity - interval;
			if (interval > 0)
				usleep(interval);
			GETTIMEOFDAY(&tp);
			t_prev = tp.tv_usec;
		}
#else
			usleep(delay * granularity);
#endif
	}
	(void) free((void *) lines);
}



static void
circle_wipe(Display * dpy, Window window, GC gc,
	    int width, int height, int delay, int granularity)
{
	int         full = 360 * 64;
	int         inc = full / 64;
	int         start = NRAND(full);
	int         rad = (width > height ? width : height);
	int         i;

#if HAVE_GETTIMEOFDAY
	struct timeval tp;
	int         interval, t_prev;

	GETTIMEOFDAY(&tp);
	t_prev = tp.tv_usec;
#endif

	if (LRAND() & 1)
		inc = -inc;
	for (i = (inc > 0 ? 0 : full);
	     (inc > 0 ? i < full : i > 0);
	     i += inc) {
		XFillArc(dpy, window, gc,
		     (width / 2) - rad, (height / 2) - rad, rad * 2, rad * 2,
			 (i + start) % full, inc);
		XFlush(dpy);
#if HAVE_GETTIMEOFDAY
		{
			GETTIMEOFDAY(&tp);
			interval = tp.tv_usec - t_prev;
			if (interval <= 0)
				interval = interval + 1000000;
			interval = delay * granularity - interval;
			if (interval > 0)
				usleep(interval);
			GETTIMEOFDAY(&tp);
			t_prev = tp.tv_usec;
		}
#else
		usleep(delay * granularity);
#endif
	}
}


static void
three_circle_wipe(Display * dpy, Window window, GC gc,
		  int width, int height, int delay, int granularity)
{
	int         i;
	int         full = 360 * 64;
	int         q = full / 6;
	int         q2 = q * 2;
	int         inc = full / 240;
	int         start = NRAND(q);
	int         rad = (width > height ? width : height);

#if HAVE_GETTIMEOFDAY
	struct timeval tp;
	int         interval, t_prev;

	GETTIMEOFDAY(&tp);
	t_prev = tp.tv_usec;
#endif

	for (i = 0; i < q; i += inc) {
		XFillArc(dpy, window, gc, (width / 2) - rad, (height / 2) - rad, rad * 2, rad * 2,
			 (start + i) % full, inc);
		XFillArc(dpy, window, gc, (width / 2) - rad, (height / 2) - rad, rad * 2, rad * 2,
			 (start - i) % full, -inc);

		XFillArc(dpy, window, gc, (width / 2) - rad, (height / 2) - rad, rad * 2, rad * 2,
			 (start + q2 + i) % full, inc);
		XFillArc(dpy, window, gc, (width / 2) - rad, (height / 2) - rad, rad * 2, rad * 2,
			 (start + q2 - i) % full, -inc);

		XFillArc(dpy, window, gc, (width / 2) - rad, (height / 2) - rad, rad * 2, rad * 2,
			 (start + q2 + q2 + i) % full, inc);
		XFillArc(dpy, window, gc, (width / 2) - rad, (height / 2) - rad, rad * 2, rad * 2,
			 (start + q2 + q2 - i) % full, -inc);

		XSync(dpy, False);
#if HAVE_GETTIMEOFDAY
		{
			GETTIMEOFDAY(&tp);
			interval = tp.tv_usec - t_prev;
			if (interval <= 0)
				interval = interval + 1000000;
			interval = delay * granularity - interval;
			if (interval > 0)
				usleep(interval);
			GETTIMEOFDAY(&tp);
			t_prev = tp.tv_usec;
		}
#else
		usleep(delay * granularity);
#endif
	}
}


static void
squaretate(Display * dpy, Window window, GC gc,
	   int width, int height, int delay, int granularity)
{
#ifdef FAST_CPU
	int         steps = (((width > height ? width : width) * 2) / granularity);

#else
	int         steps = (((width > height ? width : width)) / (4 * granularity));

#endif
	int         i;
	Bool        flip = LRAND() & 1;

#if HAVE_GETTIMEOFDAY
	struct timeval tp;
	int         interval, t_prev;

	GETTIMEOFDAY(&tp);
	t_prev = tp.tv_usec;
#endif

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
		if (delay > 0)
#if HAVE_GETTIMEOFDAY
		{
			GETTIMEOFDAY(&tp);
			interval = tp.tv_usec - t_prev;
			if (interval <= 0)
				interval = interval + 1000000;
			interval = delay * granularity - interval;
			if (interval > 0)
				usleep(interval);
			GETTIMEOFDAY(&tp);
			t_prev = tp.tv_usec;
		}
#else
			usleep(delay * granularity);
#endif
	}
#undef DRAW
}

static struct eraser_names {
	Eraser      mode;
	char       *name;
} erasers[] = {

	{
		random_lines, "random_lines"
	},
	{
		random_squares, "random_squares"
	},
	{
		venetian, "venetian"
	},
	{
		triple_wipe, "triple_wipe"
	},
	{
		quad_wipe, "quad_wipe"
	},
	{
		circle_wipe, "circle_wipe"
	},
	{
		three_circle_wipe, "three_circle_wipe"
	},
	{
		squaretate, "squaretate"
	},
	{
		no_fade, "no_fade"
	}
};


static void
erase_window(ModeInfo * mi, GC erase_gc, unsigned long pixel, int mode,
	     int delay)
{
	int         granularity = 25;

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
