/* wander, by Rick Campbell <rick@campbellcentral.org>, 19 December 1998.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include <stdio.h>

#include "screenhack.h"
#include "colors.h"
#include "erase.h"

#define MAXIMUM_COLOR_COUNT (256)

static unsigned int advance     = 0;
static Bool         circles     = 0;
static Colormap     color_map   = (Colormap)NULL;
static int          color_count = 0;
static int          color_index = 0;
static XColor       colors      [MAXIMUM_COLOR_COUNT];
static GC           context     = (GC)NULL;
static unsigned int density     = 0;
static int          depth       = 0;
static int          height      = 0;
static unsigned int length 	= 0;
static unsigned int reset 	= 0;
static unsigned int size 	= 0;
static int          width       = 0;

static void
init_wander (Display *display, Window window)
{
    XGCValues values;
    XWindowAttributes attributes;

    XClearWindow (display, window);
    XGetWindowAttributes (display, window, &attributes);
    width = attributes.width;
    height = attributes.height;
    depth = attributes.depth;
    color_map = attributes.colormap;
    if (color_count)
    {
        free_colors (display, color_map, colors, color_count);
        color_count = 0;
    }
    context = XCreateGC (display, window, GCForeground, &values);
    color_count = MAXIMUM_COLOR_COUNT;
    make_color_loop (display, color_map,
                    0,   1, 1,
                    120, 1, 1,
                    240, 1, 1,
                    colors, &color_count, True, False);
    if (color_count <= 0)
    {
        color_count = 2;
        colors [0].red = colors [0].green = colors [0].blue = 0;
        colors [1].red = colors [1].green = colors [1].blue = 0xFFFF;
        XAllocColor (display, color_map, &colors [0]);
        XAllocColor (display, color_map, &colors [1]);
    }
    color_index = random () % color_count;
    
    advance = get_integer_resource ("advance", "Integer");
    density = get_integer_resource ("density", "Integer");
    if (density < 1) density = 1;
    reset = get_integer_resource ("reset", "Integer");
    if (reset < 100) reset = 100;
    circles = get_boolean_resource ("circles", "Boolean");
    size = get_integer_resource ("size", "Integer");
    if (size < 1) size = 1;
    width = width / size;
    height = height / size;
    length = get_integer_resource ("length", "Integer");
    if (length < 1) length = 1;
    XSetForeground (display, context, colors [color_index].pixel);
}


static void
wander (Display *display, Window window)
{
    int x = random () % width;
    int y = random () % height;
    int last_x = x;
    int last_y = y;
    int width_1 = width - 1;
    int height_1 = height - 1;
    int length_limit = length;
    int reset_limit = reset;
    int color_index = random () % color_count;
    unsigned long color = colors [random () % color_count].pixel;
    Pixmap pixmap = XCreatePixmap (display, DefaultRootWindow (display), size,
				   size, depth);
    XSetForeground (display, context,
		    BlackPixel (display, DefaultScreen (display)));
    XFillRectangle (display, pixmap, context, 0, 0,
		    width * size, height * size);
    XSetForeground (display, context, color);
    XFillArc (display, pixmap, context, 0, 0, size, size, 0, 360*64);

    while (1)
    {
        if (random () % density)
        {
            x = last_x;
            y = last_y;
        }
        else
        {
            last_x = x;
            last_y = y;
            x = (x + width_1  + (random () % 3)) % width;
            y = (y + height_1 + (random () % 3)) % height;
        }
        if ((random () % length_limit) == 0)
        {
            if (advance == 0)
            {
                color_index = random () % color_count;
            }
            else
            {
                color_index = (color_index + advance) % color_count;
            }
            color = colors [color_index].pixel;
            XSetForeground (display, context, color);
            if (circles)
            {
                XFillArc (display, pixmap, context,
			  0, 0, size, size, 0, 360 * 64);
            }
        }
        if ((random () % reset_limit) == 0)
        {
            erase_full_window (display, window);
            color = colors [random () % color_count].pixel;
            x = random () % width;
            y = random () % height;
            last_x = x;
            last_y = y;
            if (circles)
            {
                XFillArc (display, pixmap, context, 0, 0, size, size, 0, 360*64);
            }
        }
        if (size == 1)
        {
            XDrawPoint (display, window, context, x, y);
        }
        else
        {
            if (circles)
            {
                XCopyArea (display, pixmap, window, context, 0, 0, size, size,
                           x * size, y * size);
            }
            else
            {
                XFillRectangle (display, window, context, x * size, y * size,
                               size, size);
            }
        }
        screenhack_handle_events (display);
    }
}

char *progclass = "Wander";

char *defaults [] =
{
    ".advance:    1",
    ".density:    2",
    ".length:     25000",
    ".delay:      1",
    ".reset:      2500000",
    ".circles:    False",
    ".size:       1",
    0
};

XrmOptionDescRec options [] =
{
    { "-advance", ".advance", XrmoptionSepArg, 0 },
    { "-circles", ".circles",   XrmoptionNoArg, "True" },
    { "-no-circles",".circles", XrmoptionNoArg, "False" },
    { "-density", ".density", XrmoptionSepArg, 0 },
    { "-length",  ".length",  XrmoptionSepArg, 0 },
    { "-delay",   ".delay",   XrmoptionSepArg, 0 },
    { "-reset",   ".reset",   XrmoptionSepArg, 0 },
    { "-size",    ".size",    XrmoptionSepArg, 0 },
    { 0, 0, 0, 0 }
};

void
screenhack (display, window)
    Display *display;
    Window window;
{
    int delay = get_integer_resource ("delay", "Integer");
    while (1)
    {
        init_wander (display, window);
        wander (display, window);
        screenhack_handle_events (display);
        if (delay) sleep (delay);
        erase_full_window (display, window);
    }
}
