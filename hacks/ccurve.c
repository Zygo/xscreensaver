/* ccurve, Copyright (c) 1998, 1999
 *  Rick Campbell <rick@campbellcentral.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 */

/* Draw self-similar linear fractals including the classic ``C Curve''
 * 
 * 16 Aug 1999  Rick Campbell <rick@campbellcentral.org>
 *      Eliminated sub-windows-with-backing-store-double-buffering crap in
 *      favor of drawing the new image in a pixmap and then splatting that on
 *      the window.
 *
 * 19 Dec 1998  Rick Campbell <rick@campbellcentral.org>
 *      Original version.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "screenhack.h"
#include "colors.h"
#include "erase.h"

#define SQRT3 (1.73205080756887729353)
#define MAXIMUM_COLOR_COUNT (256)
#define EPSILON (1e-5)

typedef struct Position_struct
{
    double x;
    double y;
}
Position;

typedef struct Segment_struct
{
    double angle;
    double length;
}
Segment;

struct state {
  Display *dpy;
  Window window;

  int                  color_count;
  int                  color_index;
  Colormap             color_map;
  XColor               colors [MAXIMUM_COLOR_COUNT];
  int                  line_count;
  int                  maximum_lines;
  double               plot_maximum_x;
  double               plot_maximum_y;
  double               plot_minimum_x;
  double               plot_minimum_y;
  int                  total_lines;

  unsigned long int    background;
  GC                   context;
  Pixmap               pixmap;
  int                  width;
  int                  height;
  float                delay;
  float                delay2;

  int    draw_index;

  int draw_iterations;
  double draw_maximum_x;
  double draw_maximum_y;
  double draw_minimum_x;
  double draw_minimum_y;
  int draw_segment_count;
  Segment* draw_segments;
  double draw_x1;
  double draw_y1;
  double draw_x2;
  double draw_y2;
};




/* normalize alters the sequence to go from (0,0) to (1,0) */
static void
normalized_plot (int       segment_count,
		 Segment*  segments,
		 Position* points)
{
    double   angle = 0.0;
    double   cosine = 0.0;
    int      index = 0;
    double   length = 0.0;
    double   sine = 0.0;
    double   x = 0.0;
    double   y = 0.0;

    for (index = 0; index < segment_count; ++index)
    {
	Segment* segment = segments + index;
	double length = segment->length;
	double angle = segment->angle;

	x += length * cos (angle);
	y += length * sin (angle);
	points [index].x = x;
	points [index].y = y;
    }
    angle = -(atan2 (y, x));
    cosine = cos (angle);
    sine = sin (angle);
    length = sqrt ((x * x) + (y * y));
    /* rotate and scale */
    for (index = 0; index < segment_count; ++index)
    {
	double temp_x = points [index].x;
	double temp_y = points [index].y;
	points [index].x = ((temp_x * cosine) + (temp_y * (-sine))) / length;
	points [index].y = ((temp_x * sine) + (temp_y * cosine)) / length;
    }
}

static void
copy_points (int       segment_count,
	     Position* source,
	     Position* target)
{
    int      index = 0;

    for (index = 0; index < segment_count; ++index)
    {
	target [index] = source [index];
    }
}

static void
realign (double    x1,
	 double    y1,
	 double    x2,
	 double    y2,
	 int       segment_count,
	 Position* points)
{
    double angle = 0.0;
    double cosine = 0.0;
    double delta_x = 0.0;
    double delta_y = 0.0;
    int    index = 0;
    double length = 0.0;
    double sine = 0.0;

    delta_x = x2 - x1;
    delta_y = y2 - y1;
    angle = atan2 (delta_y, delta_x);
    cosine = cos (angle);
    sine = sin (angle);
    length = sqrt ((delta_x * delta_x) + (delta_y * delta_y));
    /* rotate, scale, then shift */
    for (index = 0; index < segment_count; ++index)
    {
	double temp_x = points [index].x;
	double temp_y = points [index].y;
	points [index].x
	    = (length * ((temp_x * cosine) + (temp_y * (-sine)))) + x1;
	points [index].y
	    = (length * ((temp_x * sine) + (temp_y * cosine))) + y1;
    }
}

static Bool
self_similar_normalized (struct state *st,
			 int       iterations,
			 double    x1,
			 double    y1,
			 double    x2,
			 double    y2,
			 double    maximum_x,
			 double    maximum_y,
			 double    minimum_x,
			 double    minimum_y,
			 int       segment_count,
			 Position* points)
{
    if (iterations == 0)
    {
	double delta_x = maximum_x - minimum_x;
	double delta_y = maximum_y - minimum_y;
	st->color_index = (int)(((double)(st->line_count * st->color_count))
			    / ((double)st->total_lines));
	++st->line_count;
	XSetForeground (st->dpy, st->context, st->colors [st->color_index].pixel);
	if (st->plot_maximum_x < x1) st->plot_maximum_x = x1;
	if (st->plot_maximum_x < x2) st->plot_maximum_x = x2;
	if (st->plot_maximum_y < y1) st->plot_maximum_y = y1;
	if (st->plot_maximum_y < y2) st->plot_maximum_y = y2;
	if (st->plot_minimum_x > x1) st->plot_minimum_x = x1;
	if (st->plot_minimum_x > x2) st->plot_minimum_x = x2;
	if (st->plot_minimum_y > y1) st->plot_minimum_y = y1;
	if (st->plot_minimum_y > y2) st->plot_minimum_y = y2;
	XDrawLine (st->dpy, st->pixmap, st->context,
		   (int)(((x1 - minimum_x) / delta_x) * st->width),
		   (int)(((maximum_y - y1) / delta_y) * st->height),
		   (int)(((x2 - minimum_x) / delta_x) * st->width),
		   (int)(((maximum_y - y2) / delta_y) * st->height));
    }
    else
    {
	int       index = 0;
	double    next_x = 0.0;
	double    next_y = 0.0;
	Position* replacement = (Position*)NULL;
	double    x = 0.0;
	double    y = 0.0;

	replacement = (Position*)(malloc (segment_count * sizeof (*replacement)));
	copy_points (segment_count, points, replacement);
	assert (fabs ((replacement [segment_count - 1].x) - 1.0) < EPSILON);
	assert (fabs (replacement [segment_count - 1].y) < EPSILON);
	realign (x1, y1, x2, y2, segment_count, replacement);
        /* jwz: I don't understand what these assertions are supposed to
           be detecting, but let's just bail on the fractal instead of
           crashing. */
/*	assert (fabs (x2 - (replacement [segment_count - 1].x)) < EPSILON);
	assert (fabs (y2 - (replacement [segment_count - 1].y)) < EPSILON);*/
        if (fabs (x2 - (replacement [segment_count - 1].x)) >= EPSILON ||
            fabs (y2 - (replacement [segment_count - 1].y)) >= EPSILON) {
          free (replacement);
          return False;
        }
	x = x1;
	y = y1;
	for (index = 0; index < segment_count; ++index)
	{
	    next_x = replacement [index].x;
	    next_y = replacement [index].y;
	    if (!self_similar_normalized (st, 
				     iterations - 1, x, y, next_x, next_y,
				     maximum_x, maximum_y,
				     minimum_x, minimum_y,
				     segment_count, points)) {
              free(replacement);
              return False;
            }
	    x = next_x;
	    y = next_y;
	}
        free(replacement);
    }
    return True;
}

static void
self_similar (struct state *st,
	      Pixmap   pixmap,
	      GC       context,
	      int      width,
	      int      height,
	      int      iterations,
	      double   x1,
	      double   y1,
	      double   x2,
	      double   y2,
	      double   maximum_x,
	      double   maximum_y,
	      double   minimum_x,
	      double   minimum_y,
	      int      segment_count,
	      Segment* segments)
{
    Position* points = (Position*)NULL;

    points = (Position*)(malloc (segment_count * sizeof (Position)));
    normalized_plot (segment_count, segments, points);
    assert (fabs ((points [segment_count - 1].x) - 1.0) < EPSILON);
    assert (fabs (points [segment_count - 1].y) < EPSILON);
    self_similar_normalized (st, iterations,
			     x1, y1, x2, y2,
			     maximum_x, maximum_y,
			     minimum_x, minimum_y,
			     segment_count, points);
    free((void*)points);
}

static
double
random_double (double base,
	       double limit,
	       double epsilon)
{
    double       range = 0.0;
    unsigned int steps = 0;

    assert (base < limit);
    assert (epsilon > 0.0);
    range = limit - base;
    steps = (unsigned int)(floor (range / epsilon));
    return base + ((random () % steps) * epsilon);
}

static void
select_2_pattern (Segment* segments)
{
    if ((random () % 2) == 0)
    {
	if ((random () % 2) == 0)
	{
	    segments [0].angle  = -M_PI_4;
	    segments [0].length = M_SQRT2;
	    segments [1].angle  = M_PI_4;
	    segments [1].length = M_SQRT2;
	}
	else
	{
	    segments [0].angle  = M_PI_4;
	    segments [0].length = M_SQRT2;
	    segments [1].angle  = -M_PI_4;
	    segments [1].length = M_SQRT2;
	}
    }
    else
    {
	segments [0].angle
	    = random_double (M_PI / 6.0, M_PI / 3.0, M_PI / 180.0);
	segments [0].length = random_double (0.25, 0.67, 0.001);
	if ((random () % 2) == 0)
	{
	    segments [1].angle = -(segments [0].angle);
	    segments [1].length = segments [0].length;
	}
	else
	{
	    segments [1].angle = random_double ((-M_PI) / 3.0,
						(-M_PI) / 6.0,
						M_PI / 180.0);
	    segments [1].length = random_double (0.25, 0.67, 0.001);
	}	
    }
}

static void
select_3_pattern (Segment* segments)
{
    switch (random () % 5)
    {
     case 0:
	if ((random () % 2) == 0)
	{
	    segments [0].angle  = M_PI_4;
	    segments [0].length = M_SQRT2 / 4.0;
	    segments [1].angle  = -M_PI_4;
	    segments [1].length = M_SQRT2 / 2.0;
	    segments [2].angle  = M_PI_4;
	    segments [2].length = M_SQRT2 / 4.0;
	}
	else
	{
	    segments [0].angle  = -M_PI_4;
	    segments [0].length = M_SQRT2 / 4.0;
	    segments [1].angle  = M_PI_4;
	    segments [1].length = M_SQRT2 / 2.0;
	    segments [2].angle  = -M_PI_4;
	    segments [2].length = M_SQRT2 / 4.0;
	}
	break;
     case 1:
	if ((random () % 2) == 0)
	{
	    segments [0].angle  = M_PI / 6.0;
	    segments [0].length = 1.0;
	    segments [1].angle  = -M_PI_2;
	    segments [1].length = 1.0;
	    segments [2].angle  = M_PI / 6.0;
	    segments [2].length = 1.0;
	}
	else
	{
	    segments [0].angle  = -M_PI / 6.0;
	    segments [0].length = 1.0;
	    segments [1].angle  = M_PI_2;
	    segments [1].length = 1.0;
	    segments [2].angle  = -M_PI / 6.0;
	    segments [2].length = 1.0;
	}
	break;
     case 2:
     case 3:
     case 4:
	segments [0].angle
	    = random_double (M_PI / 6.0, M_PI / 3.0, M_PI / 180.0);
	segments [0].length = random_double (0.25, 0.67, 0.001);
	segments [1].angle
	    = random_double (-M_PI / 3.0, -M_PI / 6.0, M_PI / 180.0);
	segments [1].length = random_double (0.25, 0.67, 0.001);
	if ((random () % 3) == 0)
	{
	    if ((random () % 2) == 0)
	    {
		segments [2].angle = segments [0].angle;
	    }
	    else
	    {
		segments [2].angle = -(segments [0].angle);
	    }
	    segments [2].length = segments [0].length;
	}
	else
	{
	    segments [2].angle
		= random_double (-M_PI / 3.0, -M_PI / 6.0, M_PI / 180.0);
	    segments [2].length = random_double (0.25, 0.67, 0.001);
	}
	break;
    }
}

static void
select_4_pattern (Segment* segments)
{
    switch (random () % 9)
    {
     case 0:
	if ((random () % 2) == 0)
	{
	    double length = random_double (0.25, 0.50, 0.001);

	    segments [0].angle  = 0.0;
	    segments [0].length = 0.5;
	    segments [1].angle  = M_PI_2;
	    segments [1].length = length;
	    segments [2].angle  = -M_PI_2;
	    segments [2].length = length;
	    segments [3].angle  = 0.0;
	    segments [3].length = 0.5;
	}
	else
	{
	    double length = random_double (0.25, 0.50, 0.001);

	    segments [0].angle  = 0.0;
	    segments [0].length = 0.5;
	    segments [1].angle  = -M_PI_2;
	    segments [1].length = length;
	    segments [2].angle  = M_PI_2;
	    segments [2].length = length;
	    segments [3].angle  = 0.0;
	    segments [3].length = 0.5;
	}
	break;
     case 1:
	if ((random () % 2) == 0)
	{
	    segments [0].angle  = 0.0;
	    segments [0].length = 0.5;
	    segments [1].angle  = M_PI_2;
	    segments [1].length = 0.45;
	    segments [2].angle  = -M_PI_2;
	    segments [2].length = 0.45;
	    segments [3].angle  = 0.0;
	    segments [3].length = 0.5;
	}
	else
	{
	    segments [0].angle  = 0.0;
	    segments [0].length = 0.5;
	    segments [1].angle  = -M_PI_2;
	    segments [1].length = 0.45;
	    segments [2].angle  = M_PI_2;
	    segments [2].length = 0.45;
	    segments [3].angle  = 0.0;
	    segments [3].length = 0.5;
	}
	break;
     case 2:
	if ((random () % 2) == 0)
	{
	    segments [0].angle  = 0.0;
	    segments [0].length = 1.0;
	    segments [1].angle  = (5.0 * M_PI) / 12.0;
	    segments [1].length = 1.2;
	    segments [2].angle  = (-5.0 * M_PI) / 12.0;
	    segments [2].length = 1.2;
	    segments [3].angle  = 0.0;
	    segments [3].length = 1.0;
	}
	else
	{
	    segments [0].angle  = 0.0;
	    segments [0].length = 1.0;
	    segments [1].angle  = (-5.0 * M_PI) / 12.0;
	    segments [1].length = 1.2;
	    segments [2].angle  = (5.0 * M_PI) / 12.0;
	    segments [2].length = 1.2;
	    segments [3].angle  = 0.0;
	    segments [3].length = 1.0;
	}
	break;
     case 3:
	if ((random () % 2) == 0)
	{
	    double angle
		= random_double (M_PI / 4.0,
				 M_PI_2,
				 M_PI / 180.0);

	    segments [0].angle  = 0.0;
	    segments [0].length = 1.0;
	    segments [1].angle  = angle;
	    segments [1].length = 1.2;
	    segments [2].angle  = (-angle);
	    segments [2].length = 1.2;
	    segments [3].angle  = 0.0;
	    segments [3].length = 1.0;
	}
	else
	{
	    double angle
		= random_double (M_PI / 4.0,
				 M_PI_2,
				 M_PI / 180.0);

	    segments [0].angle  = 0.0;
	    segments [0].length = 1.0;
	    segments [1].angle  = (-angle);
	    segments [1].length = 1.2;
	    segments [2].angle  = angle;
	    segments [2].length = 1.2;
	    segments [3].angle  = 0.0;
	    segments [3].length = 1.0;
	}
	break;
     case 4:
	if ((random () % 2) == 0)
	{
	    double angle
		= random_double (M_PI / 4.0,
				 M_PI_2,
				 M_PI / 180.0);

	    segments [0].angle  = 0.0;
	    segments [0].length = 1.0;
	    segments [1].angle  = angle;
	    segments [1].length = 1.2;
	    segments [2].angle  = (-angle);
	    segments [2].length = 1.2;
	    segments [3].angle  = 0.0;
	    segments [3].length = 1.0;
	}
	else
	{
	    double angle
		= random_double (M_PI / 4.0,
				 M_PI_2,
				 M_PI / 180.0);

	    segments [0].angle  = 0.0;
	    segments [0].length = 1.0;
	    segments [1].angle  = (-angle);
	    segments [1].length = 1.2;
	    segments [2].angle  = angle;
	    segments [2].length = 1.2;
	    segments [3].angle  = 0.0;
	    segments [3].length = 1.0;
	}
	break;
     case 5:
	if ((random () % 2) == 0)
	{
	    double angle
		= random_double (M_PI / 4.0,
				 M_PI_2,
				 M_PI / 180.0);
	    double length = random_double (0.25, 0.50, 0.001);

	    segments [0].angle  = 0.0;
	    segments [0].length = 1.0;
	    segments [1].angle  = angle;
	    segments [1].length = length;
	    segments [2].angle  = (-angle);
	    segments [2].length = length;
	    segments [3].angle  = 0.0;
	    segments [3].length = 1.0;
	}
	else
	{
	    double angle
		= random_double (M_PI / 4.0,
				 M_PI_2,
				 M_PI / 180.0);
	    double length = random_double (0.25, 0.50, 0.001);

	    segments [0].angle  = 0.0;
	    segments [0].length = 1.0;
	    segments [1].angle  = (-angle);
	    segments [1].length = length;
	    segments [2].angle  = angle;
	    segments [2].length = length;
	    segments [3].angle  = 0.0;
	    segments [3].length = 1.0;
	}
	break;
     case 6:
     case 7:
     case 8:
	segments [0].angle
	    = random_double (M_PI / 12.0, (11.0 * M_PI) / 12.0, 0.001);
	segments [0].length = random_double (0.25, 0.50, 0.001);
	segments [1].angle
	    = random_double (M_PI / 12.0, (11.0 * M_PI) / 12.0, 0.001);
	segments [1].length = random_double (0.25, 0.50, 0.001);
	if ((random () % 3) == 0)
	{
	    segments [2].angle
		= random_double (M_PI / 12.0, (11.0 * M_PI) / 12.0, 0.001);
	    segments [2].length = random_double (0.25, 0.50, 0.001);
	    segments [3].angle
		= random_double (M_PI / 12.0, (11.0 * M_PI) / 12.0, 0.001);
	    segments [3].length = random_double (0.25, 0.50, 0.001);
	}
	else
	{
	    if ((random () % 2) == 0)
	    {
		segments [2].angle = -(segments [1].angle);
		segments [2].length = segments [1].length;
		segments [3].angle = -(segments [0].angle);
		segments [3].length = segments [0].length;
	    }
	    else
	    {
		segments [2].angle = segments [1].angle;
		segments [2].length = segments [1].length;
		segments [3].angle = segments [0].angle;
		segments [3].length = segments [0].length;
	    }
	}
	break;
    }
}

static void
select_pattern (int      segment_count,
		Segment* segments)
{
    switch (segment_count)
    {
     case 2:
	select_2_pattern (segments);
	break;
     case 3:
	select_3_pattern (segments);
	break;
     case 4:
	select_4_pattern (segments);
	break;
     default:
	fprintf (stderr, "\nBad segment count, must be 2, 3, or 4.\n");
	exit (1);
    }
}

#define Y_START (0.5)

static void *
ccurve_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
    unsigned long int    black           = 0;
    int                  depth           = 0;
    XWindowAttributes    hack_attributes;
    XGCValues            values;
    unsigned long int    white           = 0;

    st->dpy = dpy;
    st->window = window;

    st->delay = get_float_resource (st->dpy, "delay", "Integer");
    st->delay2 = get_float_resource (st->dpy, "pause", "Integer");
    st->maximum_lines = get_integer_resource (st->dpy, "limit", "Integer");
    black = BlackPixel (st->dpy, DefaultScreen (st->dpy));
    white = WhitePixel (st->dpy, DefaultScreen (st->dpy));
    st->background = black;
    XGetWindowAttributes (st->dpy, st->window, &hack_attributes);
    st->width = hack_attributes.width;
    st->height = hack_attributes.height;
    depth = hack_attributes.depth;
    st->color_map = hack_attributes.colormap;
    st->pixmap = XCreatePixmap (st->dpy, st->window, st->width, st->height, depth);
    values.foreground = white;
    values.background = black;
    st->context = XCreateGC (st->dpy, st->window, GCForeground | GCBackground,
			 &values);
    st->color_count = MAXIMUM_COLOR_COUNT;
    make_color_loop (hack_attributes.screen, hack_attributes.visual,
                     st->color_map,
		     0,   1, 1,
		     120, 1, 1,
		     240, 1, 1,
		     st->colors, &st->color_count, True, False);
    if (st->color_count <= 0)
    {
        st->color_count = 1;
        st->colors [0].red = st->colors [0].green = st->colors [0].blue = 0xFFFF;
        XAllocColor (st->dpy, st->color_map, &st->colors [0]);
    }

    st->draw_maximum_x =  1.20;
    st->draw_maximum_y =  0.525;
    st->draw_minimum_x = -0.20;
    st->draw_minimum_y = -0.525;
    st->draw_x2 = 1.0;

    return st;
}

static unsigned long
ccurve_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
	static const int lengths [] = { 4, 4, 4, 4, 4, 3, 3, 3, 2 };

        if (st->draw_index == 0)
          {
	st->draw_segment_count
	    = lengths [random () % (sizeof (lengths) / sizeof (int))];
        if (st->draw_segments) free (st->draw_segments);
	st->draw_segments
	    = (Segment*)(malloc ((st->draw_segment_count) * sizeof (Segment)));
	select_pattern (st->draw_segment_count, st->draw_segments);
	st->draw_iterations = floor (log (st->maximum_lines)
				/ log (((double)(st->draw_segment_count))));
	if ((random () % 3) != 0)
	{
	    double factor = 0.45;
	    st->draw_x1 += random_double (-factor, factor, 0.001);
	    st->draw_y1 += random_double (-factor, factor, 0.001);
	    st->draw_x2 += random_double (-factor, factor, 0.001);
	    st->draw_y2 += random_double (-factor, factor, 0.001);
	}
/* 	background = (random () % 2) ? black : white; */

        }

	/* for (st->draw_index = 0; st->draw_index < st->draw_iterations; ++st->draw_index) */
	{
	    double delta_x = 0.0;
	    double delta_y = 0.0;

	    XSetForeground (st->dpy, st->context, st->background);
	    XFillRectangle (st->dpy, st->pixmap, st->context, 0, 0, st->width, st->height);
	    st->line_count = 0;
	    st->total_lines = (int)(pow ((double)(st->draw_segment_count),
				     (double)st->draw_index));
	    st->plot_maximum_x = -1000.00;
	    st->plot_maximum_y = -1000.00;
	    st->plot_minimum_x =  1000.00;
	    st->plot_minimum_y =  1000.00;
	    self_similar (st, st->pixmap, st->context, st->width, st->height, st->draw_index,
			  st->draw_x1, st->draw_y1, st->draw_x2, st->draw_y2,
			  st->draw_maximum_x,
			  st->draw_maximum_y,
			  st->draw_minimum_x,
			  st->draw_minimum_y,
			  st->draw_segment_count, st->draw_segments);
	    delta_x = st->plot_maximum_x - st->plot_minimum_x;
	    delta_y = st->plot_maximum_y - st->plot_minimum_y;
	    st->draw_maximum_x = st->plot_maximum_x + (delta_x * 0.2);
	    st->draw_maximum_y = st->plot_maximum_y + (delta_y * 0.2);
	    st->draw_minimum_x = st->plot_minimum_x - (delta_x * 0.2);
	    st->draw_minimum_y = st->plot_minimum_y - (delta_y * 0.2);
	    delta_x = st->draw_maximum_x - st->draw_minimum_x;
	    delta_y = st->draw_maximum_y - st->draw_minimum_y;
	    if ((delta_y / delta_x) > (((double)st->height) / ((double)st->width)))
	    {
		double new_delta_x
		    = (delta_y * ((double)st->width)) / ((double)st->height);
		st->draw_minimum_x -= (new_delta_x - delta_x) / 2.0;
		st->draw_maximum_x += (new_delta_x - delta_x) / 2.0;
	    }
	    else
	    {
		double new_delta_y
		    = (delta_x * ((double)st->height)) / ((double)st->width);
		st->draw_minimum_y -= (new_delta_y - delta_y) / 2.0;
		st->draw_maximum_y += (new_delta_y - delta_y) / 2.0;
	    }
	    XCopyArea (st->dpy, st->pixmap, st->window, st->context, 0, 0, st->width, st->height,
		       0, 0);
	}
        st->draw_index++;
        /* #### mi->recursion_depth = st->draw_index; */

        if (st->draw_index >= st->draw_iterations)
          {
            st->draw_index = 0;
            free((void*)st->draw_segments);
            st->draw_segments = 0;
            return (int) (1000000 * st->delay);
          }
        else
          return (int) (1000000 * st->delay2);
}

static void
ccurve_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  XWindowAttributes xgwa;
  st->width = w;
  st->height = h;
  XGetWindowAttributes (st->dpy, st->window, &xgwa);
  XFreePixmap (dpy, st->pixmap);
  st->pixmap = XCreatePixmap (st->dpy, st->window, st->width, st->height,
                              xgwa.depth);
}

static Bool
ccurve_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (screenhack_event_helper (dpy, window, event))
    {
      st->draw_index = 0;
      return True;
    }
  return False;
}

static void
ccurve_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  XFreeGC (dpy, st->context);
  if (st->draw_segments) free (st->draw_segments);
  free (st);
}


static const char *ccurve_defaults [] =
{
    ".background:  black",
    ".foreground:  white",
    ".delay:      3",
    ".pause:      0.4",
    ".limit: 200000",
#ifdef HAVE_MOBILE
    "*ignoreRotation: True",
#endif
    0
};

static XrmOptionDescRec ccurve_options [] =
{
    { "-delay", ".delay", XrmoptionSepArg, 0 },
    { "-pause", ".pause", XrmoptionSepArg, 0 },
    { "-limit", ".limit", XrmoptionSepArg, 0 },
    { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("CCurve", ccurve)
