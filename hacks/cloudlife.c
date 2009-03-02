/* cloudlife by Don Marti <dmarti@zgp.org>
 *
 * Based on Conway's Life, but with one rule change to make it a better
 * screensaver: cells have a max age.
 *
 * When a cell exceeds the max age, it counts as 3 for populating the next
 * generation.  This makes long-lived formations explode instead of just 
 * sitting there burning a hole in your screen.
 *
 * Cloudlife only draws one pixel of each cell per tick, whether the cell is
 * alive or dead.  So gliders look like little comets.

 * Based on several examples from the hacks directory of: 
 
 * xscreensaver, Copyright (c) 1997, 1998, 2002 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include "screenhack.h"
#include <stdio.h>
#include <sys/time.h>

#ifndef MAX_WIDTH
#include <limits.h>
#define MAX_WIDTH SHRT_MAX
#endif

#ifdef TIME_ME
#include <time.h>
#endif

/* this program goes faster if some functions are inline.  The following is
 * borrowed from ifs.c */
#if !defined( __GNUC__ ) && !defined(__cplusplus) && !defined(c_plusplus)
#undef inline
#define inline			/* */
#endif

typedef struct {
    unsigned int height;
    unsigned int width;
    unsigned int max_age;
    unsigned int cell_size;
    unsigned char *cells;
    unsigned char *new_cells;
} field;

static void 
*xrealloc(void *p, size_t size)
{
    void *ret;
    if ((ret = realloc(p, size)) == NULL) {
	fprintf(stderr, "%s: out of memory\n", progname);
	exit(1);
    }
    return ret;
}

field 
*init_field(void)
{
    field *f = xrealloc(NULL, sizeof(field));
    f->height = 0;
    f->width = 0;
    f->cell_size = get_integer_resource("cellSize", "Integer");
    f->max_age = get_integer_resource("maxAge", "Integer");
    f->cells = NULL;
    f->new_cells = NULL;
    return f;
}

void 
resize_field(field * f, unsigned int w, unsigned int h)
{
    f->width = w;
    f->height = h;

    f->cells = xrealloc(f->cells,
			w * sizeof(unsigned char) * 
                        h * sizeof(unsigned char));
    f->new_cells =
	xrealloc(f->new_cells,
		 w * sizeof(unsigned char) * h * sizeof(unsigned char));
}

inline unsigned char 
*cell_at(field * f, unsigned int x, unsigned int y)
{
    return (f->cells + x * sizeof(unsigned char) + 
                       y * f->width * sizeof(unsigned char));
}

inline unsigned char 
*new_cell_at(field * f, unsigned int x, unsigned int y)
{
    return (f->new_cells + x * sizeof(unsigned char) + 
                           y * f->width * sizeof(unsigned char));
}

static void
draw_field(Display * dpy,
	   Window window, Colormap cmap, GC fgc, GC bgc, field * f)
{
    unsigned int x, y;
    unsigned int rx, ry = 0;	/* random amount to offset the dot */
    unsigned int size = 1 << f->cell_size;
    unsigned int mask = size - 1;
    static XPoint fg_points[MAX_WIDTH];
    static XPoint bg_points[MAX_WIDTH];
    unsigned int fg_count, bg_count;

    /* columns 0 and width-1 are off screen and not drawn. */
    for (y = 1; y < f->height - 1; y++) {
	fg_count = 0;
	bg_count = 0;

	/* rows 0 and height-1 are off screen and not drawn. */
	for (x = 1; x < f->width - 1; x++) {
	    rx = random();
	    ry = rx >> f->cell_size;
	    rx &= mask;
	    ry &= mask;

	    if (*cell_at(f, x, y)) {
		fg_points[fg_count].x = (short) x *size - rx - 1;
		fg_points[fg_count].y = (short) y *size - ry - 1;
		fg_count++;
	    } else {
		bg_points[bg_count].x = (short) x *size - rx - 1;
		bg_points[bg_count].y = (short) y *size - ry - 1;
		bg_count++;
	    }
	}
	XDrawPoints(dpy, window, fgc, fg_points, fg_count,
		    CoordModeOrigin);
	XDrawPoints(dpy, window, bgc, bg_points, bg_count,
		    CoordModeOrigin);
    }
}

inline unsigned int 
cell_value(unsigned char c, unsigned int age)
{
    if (!c) {
	return 0;
    } else if (c > age) {
	return (3);
    } else {
	return (1);
    }
}

inline unsigned int 
is_alive(field * f, unsigned int x, unsigned int y)
{
    unsigned int count;
    unsigned int i, j;
    unsigned char *p;

    count = 0;

    for (i = x - 1; i <= x + 1; i++) {
	for (j = y - 1; j <= y + 1; j++) {
	    if (y != j || x != i) {
		count += cell_value(*cell_at(f, i, j), f->max_age);
	    }
	}
    }

    p = cell_at(f, x, y);
    if (*p) {
	if (count == 2 || count == 3) {
	    return ((*p) + 1);
	} else {
	    return (0);
	}
    } else {
	if (count == 3) {
	    return (1);
	} else {
	    return (0);
	}
    }
}

unsigned int 
do_tick(field * f)
{
    unsigned int x, y;
    unsigned int count = 0;
    for (x = 1; x < f->width - 1; x++) {
	for (y = 1; y < f->height - 1; y++) {
	    count += *new_cell_at(f, x, y) = is_alive(f, x, y);
	}
    }
    memcpy(f->cells, f->new_cells, f->width * sizeof(unsigned char) *
	   f->height * sizeof(unsigned char));
    return count;
}


unsigned int 
random_cell(unsigned int p)
{
    int r = random() & 0xff;

    if (r < p) {
	return (1);
    } else {
	return (0);
    }
}

void 
populate_field(field * f, unsigned int p)
{
    unsigned int x, y;

    for (x = 0; x < f->width; x++) {
	for (y = 0; y < f->height; y++) {
	    *cell_at(f, x, y) = random_cell(p);
	}
    }
}

void 
populate_edges(field * f, unsigned int p)
{
    unsigned int i;

    for (i = f->width; i--;) {
	*cell_at(f, i, 0) = random_cell(p);
	*cell_at(f, i, f->height - 1) = random_cell(p);
    }

    for (i = f->height; i--;) {
	*cell_at(f, f->width - 1, i) = random_cell(p);
	*cell_at(f, 0, i) = random_cell(p);
    }
}


char *progclass = "Cloudlife";

char *defaults[] = {
    ".background:	black",
    ".foreground:	blue",
    "*cycleDelay:	25000",
    "*maxAge:		64",
    "*initialDensity:	160",
    "*cellSize:		3",
    0
};

XrmOptionDescRec options[] = {
    {"-background", ".background", XrmoptionSepArg, 0},
    {"-foreground", ".foreground", XrmoptionSepArg, 0},
    {"-cycle-delay", ".cycleDelay", XrmoptionSepArg, 0},
    {"-cell-size", ".cellSize", XrmoptionSepArg, 0},
    {"-initial-density", ".initialDensity", XrmoptionSepArg, 0},
    {"-max-age", ".maxAge", XrmoptionSepArg, 0},
    {0, 0, 0, 0}
};

void screenhack(Display * dpy, Window window)
{
    field *f = init_field();

#ifdef TIME_ME
    time_t start_time = time(NULL);
#endif

    unsigned int cycles = 0;

    GC fgc, bgc;
    XGCValues gcv;
    XWindowAttributes xgwa;

    unsigned int cycle_delay = (unsigned int)
	get_integer_resource("cycleDelay", "Integer");
    unsigned int density = (unsigned int)
	get_integer_resource("initialDensity", "Integer") & 0xff;
    XGetWindowAttributes(dpy, window, &xgwa);

    gcv.foreground = get_pixel_resource("foreground", "Foreground",
					dpy, xgwa.colormap);
    fgc = XCreateGC(dpy, window, GCForeground, &gcv);

    gcv.foreground = get_pixel_resource("background", "Background",
					dpy, xgwa.colormap);
    bgc = XCreateGC(dpy, window, GCForeground, &gcv);

    while (1) {
	XGetWindowAttributes(dpy, window, &xgwa);
	if (f->height != xgwa.height / (1 << f->cell_size) + 2 ||
	    f->width != xgwa.width / (1 << f->cell_size) + 2) {

	    resize_field(f, xgwa.width / (1 << f->cell_size) + 2,
			 xgwa.height / (1 << f->cell_size) + 2);
	    populate_field(f, density);
	}

	screenhack_handle_events(dpy);

	draw_field(dpy, window, xgwa.colormap, fgc, bgc, f);

	if (do_tick(f) < (f->height + f->width) / 4) {
	    populate_field(f, density);
	}

	if (cycles % (f->max_age /2) == 0) {
	    populate_edges(f, density);
	    do_tick(f);
	    populate_edges(f, 0);
	}

	XSync(dpy, False);

	cycles++;

	if (cycle_delay)
	    usleep(cycle_delay);

#ifdef TIME_ME
	if (cycles % f->max_age == 0) {
	    printf("%g s.\n",
		   ((time(NULL) - start_time) * 1000.0) / cycles);
	}
#endif
    }
}
