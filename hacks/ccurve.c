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

static int                  color_count = 0;
static int                  color_index = 0;
static Colormap             color_map = (Colormap)NULL;
static XColor               colors [MAXIMUM_COLOR_COUNT];
static int                  line_count = 0;
static int                  maximum_lines = 0;
static double               plot_maximum_x = -1000.00;
static double               plot_maximum_y = -1000.00;
static double               plot_minimum_x =  1000.00;
static double               plot_minimum_y =  1000.00;
static int                  total_lines = 0;

/* normalize alters the sequence to go from (0,0) to (1,0) */
static
void
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

static
void
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

static
void
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

static
void
self_similar_normalized (Display*  display,
			 Pixmap    pixmap,
			 GC	   context,
			 int       width,
			 int       height,
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
	color_index = (int)(((double)(line_count * color_count))
			    / ((double)total_lines));
	++line_count;
	XSetForeground (display, context, colors [color_index].pixel);
	if (plot_maximum_x < x1) plot_maximum_x = x1;
	if (plot_maximum_x < x2) plot_maximum_x = x2;
	if (plot_maximum_y < y1) plot_maximum_y = y1;
	if (plot_maximum_y < y2) plot_maximum_y = y2;
	if (plot_minimum_x > x1) plot_minimum_x = x1;
	if (plot_minimum_x > x2) plot_minimum_x = x2;
	if (plot_minimum_y > y1) plot_minimum_y = y1;
	if (plot_minimum_y > y2) plot_minimum_y = y2;
	XDrawLine (display, pixmap, context,
		   (int)(((x1 - minimum_x) / delta_x) * width),
		   (int)(((maximum_y - y1) / delta_y) * height),
		   (int)(((x2 - minimum_x) / delta_x) * width),
		   (int)(((maximum_y - y2) / delta_y) * height));
    }
    else
    {
	int       index = 0;
	double    next_x = 0.0;
	double    next_y = 0.0;
	Position* replacement = (Position*)NULL;
	double    x = 0.0;
	double    y = 0.0;

	replacement = (Position*)(malloc (segment_count * sizeof (Segment)));
	copy_points (segment_count, points, replacement);
	assert (fabs ((replacement [segment_count - 1].x) - 1.0) < EPSILON);
	assert (fabs (replacement [segment_count - 1].y) < EPSILON);
	realign (x1, y1, x2, y2, segment_count, replacement);
	assert (fabs (x2 - (replacement [segment_count - 1].x)) < EPSILON);
	assert (fabs (y2 - (replacement [segment_count - 1].y)) < EPSILON);
	x = x1;
	y = y1;
	for (index = 0; index < segment_count; ++index)
	{
	    next_x = replacement [index].x;
	    next_y = replacement [index].y;
	    self_similar_normalized (display, pixmap, context, width, height,
				     iterations - 1, x, y, next_x, next_y,
				     maximum_x, maximum_y,
				     minimum_x, minimum_y,
				     segment_count, points);
	    x = next_x;
	    y = next_y;
	}
        free((void*)replacement);
    }
}

static
void
self_similar (Display* display,
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
    self_similar_normalized (display, pixmap, context,
			     width, height, iterations,
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

static
void
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

static
void
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

static
void
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

char *progclass = "Ccurve";

char *defaults [] =
{
    ".delay:      1",
    ".pause:      3",
    ".limit: 200000",
    0
};

XrmOptionDescRec options [] =
{
    { "-delay", ".delay", XrmoptionSepArg, 0 },
    { "-pause", ".pause", XrmoptionSepArg, 0 },
    { "-limit", ".limit", XrmoptionSepArg, 0 },
    { 0, 0, 0, 0 }
};

#define Y_START (0.5)

void
screenhack (Display* display,
	    Window   window)
{
    unsigned long int    background      = 0;
    unsigned long int    black           = 0;
    GC                   context;
    int                  delay           = 0;
    int                  depth           = 0;
    Pixmap               pixmap           = (Pixmap)NULL;
    XWindowAttributes    hack_attributes;
    int                  height          = 0;
    int                  iterations      = 0;
    int                  pause           = 0;
    XGCValues            values;
    unsigned long int    white           = 0;
    int                  width           = 0;

    delay = get_integer_resource ("delay", "Integer");
    pause = get_integer_resource ("pause", "Integer");
    maximum_lines = get_integer_resource ("limit", "Integer");
    black = BlackPixel (display, DefaultScreen (display));
    white = WhitePixel (display, DefaultScreen (display));
    background = black;
    XGetWindowAttributes (display, window, &hack_attributes);
    width = hack_attributes.width;
    height = hack_attributes.height;
    depth = hack_attributes.depth;
    color_map = hack_attributes.colormap;
    pixmap = XCreatePixmap (display, window, width, height, depth);
    values.foreground = white;
    values.background = black;
    context = XCreateGC (display, window, GCForeground | GCBackground,
			 &values);
    color_count = MAXIMUM_COLOR_COUNT;
    make_color_loop (display, color_map,
		     0,   1, 1,
		     120, 1, 1,
		     240, 1, 1,
		     colors, &color_count, True, False);
    if (color_count <= 0)
    {
        color_count = 1;
        colors [0].red = colors [0].green = colors [0].blue = 0xFFFF;
        XAllocColor (display, color_map, &colors [0]);
    }

    while (1)
    {
	int    index = 0;
	double maximum_x =  1.20;
	double maximum_y =  0.525;
	double minimum_x = -0.20;
	double minimum_y = -0.525;
	static int lengths [] = { 4, 4, 4, 4, 4, 3, 3, 3, 2 };
	int segment_count = 0;
	Segment* segments = (Segment*)NULL;
	double x1 = 0.0;
	double y1 = 0.0;
	double x2 = 1.0;
	double y2 = 0.0;

	segment_count
	    = lengths [random () % (sizeof (lengths) / sizeof (int))];
	segments
	    = (Segment*)(malloc ((segment_count) * sizeof (Segment)));
	select_pattern (segment_count, segments);
	iterations = floor (log (maximum_lines)
				/ log (((double)(segment_count))));
	if ((random () % 3) != 0)
	{
	    double factor = 0.45;
	    x1 += random_double (-factor, factor, 0.001);
	    y1 += random_double (-factor, factor, 0.001);
	    x2 += random_double (-factor, factor, 0.001);
	    y2 += random_double (-factor, factor, 0.001);
	}
/* 	background = (random () % 2) ? black : white; */
	for (index = 0; index < iterations; ++index)
	{
	    double delta_x = 0.0;
	    double delta_y = 0.0;

	    XSetForeground (display, context, background);
	    XFillRectangle (display, pixmap, context, 0, 0, width, height);
	    line_count = 0;
	    total_lines = (int)(pow ((double)(segment_count),
				     (double)index));
	    plot_maximum_x = -1000.00;
	    plot_maximum_y = -1000.00;
	    plot_minimum_x =  1000.00;
	    plot_minimum_y =  1000.00;
	    self_similar (display, pixmap, context, width, height, index,
			  x1, y1, x2, y2,
			  maximum_x,
			  maximum_y,
			  minimum_x,
			  minimum_y,
			  segment_count, segments);
	    delta_x = plot_maximum_x - plot_minimum_x;
	    delta_y = plot_maximum_y - plot_minimum_y;
	    maximum_x = plot_maximum_x + (delta_x * 0.2);
	    maximum_y = plot_maximum_y + (delta_y * 0.2);
	    minimum_x = plot_minimum_x - (delta_x * 0.2);
	    minimum_y = plot_minimum_y - (delta_y * 0.2);
	    delta_x = maximum_x - minimum_x;
	    delta_y = maximum_y - minimum_y;
	    if ((delta_y / delta_x) > (((double)height) / ((double)width)))
	    {
		double new_delta_x
		    = (delta_y * ((double)width)) / ((double)height);
		minimum_x -= (new_delta_x - delta_x) / 2.0;
		maximum_x += (new_delta_x - delta_x) / 2.0;
	    }
	    else
	    {
		double new_delta_y
		    = (delta_x * ((double)height)) / ((double)width);
		minimum_y -= (new_delta_y - delta_y) / 2.0;
		maximum_y += (new_delta_y - delta_y) / 2.0;
	    }
	    XCopyArea (display, pixmap, window, context, 0, 0, width, height,
		       0, 0);
	    if (delay) sleep (delay);
	    screenhack_handle_events (display);
	}
        free((void*)segments);
	if (pause) sleep (pause);
	erase_full_window (display, window);
    }
}
