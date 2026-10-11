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

 * 20 May 2003 -- now includes color cycling and a man page.

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

struct field {
    unsigned int height;
    unsigned int width;
    unsigned int max_age;
    unsigned int cell_size;
    unsigned char *cells;
    unsigned char *new_cells;
};

struct state {
  Display *dpy;
  Window window;

#ifdef TIME_ME
  time_t start_time;
#endif

  unsigned int cycles;
  unsigned int colorindex;  /* which color in the colormap are we on */
  unsigned int colortimer;  /* when this reaches 0, cycle to next color */

  int cycle_delay;
  int cycle_colors;
  int ncolors;
  int density;

  GC fgc, bgc;
  XGCValues gcv;
  XWindowAttributes xgwa;
  XColor *colors;

  struct field *field;

  XPoint fg_points[MAX_WIDTH];
  XPoint bg_points[MAX_WIDTH];
};


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

static struct field *
init_field(struct state *st)
{
    struct field *f = xrealloc(NULL, sizeof(struct field));
    f->height = 0;
    f->width = 0;
    f->cell_size = get_integer_resource(st->dpy, "cellSize", "Integer");
    f->max_age = get_integer_resource(st->dpy, "maxAge", "Integer");

    if (f->max_age > 255) {
      fprintf (stderr, "%s: max-age must be < 256 (not %d)\n", progname,
               f->max_age);
      exit (1);
    }

    f->cells = NULL;
    f->new_cells = NULL;
    return f;
}

static void 
resize_field(struct field * f, unsigned int w, unsigned int h)
{
    int s = w * h * sizeof(unsigned char);
    f->width = w;
    f->height = h;

    f->cells = xrealloc(f->cells, s);
    f->new_cells = xrealloc(f->new_cells, s);
    memset(f->cells, 0, s);
    memset(f->new_cells, 0, s);
}

static inline unsigned char 
*cell_at(struct field * f, unsigned int x, unsigned int y)
{
    return (f->cells + x * sizeof(unsigned char) + 
                       y * f->width * sizeof(unsigned char));
}

static inline unsigned char 
*new_cell_at(struct field * f, unsigned int x, unsigned int y)
{
    return (f->new_cells + x * sizeof(unsigned char) + 
                           y * f->width * sizeof(unsigned char));
}

static void
draw_field(struct state *st, struct field * f)
{
    unsigned int x, y;
    unsigned int rx, ry = 0;	/* random amount to offset the dot */
    unsigned int size = 1 << f->cell_size;
    unsigned int mask = size - 1;
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
		st->fg_points[fg_count].x = (short) x *size - rx - 1;
		st->fg_points[fg_count].y = (short) y *size - ry - 1;
		fg_count++;
	    } else {
		st->bg_points[bg_count].x = (short) x *size - rx - 1;
		st->bg_points[bg_count].y = (short) y *size - ry - 1;
		bg_count++;
	    }
	}
	XDrawPoints(st->dpy, st->window, st->fgc, st->fg_points, fg_count,
		    CoordModeOrigin);
	XDrawPoints(st->dpy, st->window, st->bgc, st->bg_points, bg_count,
		    CoordModeOrigin);
    }
}

static inline unsigned int 
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

static inline unsigned int 
is_alive(struct field * f, unsigned int x, unsigned int y)
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

static unsigned int 
do_tick(struct field * f)
{
    unsigned int x, y;
    unsigned int count = 0;
    for (x = 1; x < f->width - 1; x++) {
	for (y = 1; y < f->height - 1; y++) {
	    count += *new_cell_at(f, x, y) = is_alive(f, x, y);
	}
    }
    memcpy(f->cells, f->new_cells, f->width * f->height *
           sizeof(unsigned char));
    return count;
}


static unsigned int 
random_cell(unsigned int p)
{
    int r = random() & 0xff;

    if (r < p) {
	return (1);
    } else {
	return (0);
    }
}

static void 
populate_field(struct field * f, unsigned int p)
{
    unsigned int x, y;

    for (x = 0; x < f->width; x++) {
	for (y = 0; y < f->height; y++) {
	    *cell_at(f, x, y) = random_cell(p);
	}
    }
}

static void 
populate_edges(struct field * f, unsigned int p)
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

static void *
cloudlife_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
    Bool tmp = True;

    st->dpy = dpy;
    st->window = window;
    st->field = init_field(st);

#ifdef TIME_ME
    st->start_time = time(NULL);
#endif

    st->cycle_delay = get_integer_resource(st->dpy, "cycleDelay", "Integer");
    st->cycle_colors = get_integer_resource(st->dpy, "cycleColors", "Integer");
    st->ncolors = get_integer_resource(st->dpy, "ncolors", "Integer");
    st->density = (get_integer_resource(st->dpy, "initialDensity", "Integer") 
                  % 100 * 256)/100;

    XGetWindowAttributes(st->dpy, st->window, &st->xgwa);

    if (st->cycle_colors) {
        st->colors = (XColor *) xrealloc(st->colors, sizeof(XColor) * (st->ncolors+1));
        make_smooth_colormap (st->xgwa.screen, st->xgwa.visual,
                              st->xgwa.colormap, st->colors, &st->ncolors,
                              True, &tmp, True);
    }

    st->gcv.foreground = get_pixel_resource(st->dpy, st->xgwa.colormap,
                                        "foreground", "Foreground");
    st->fgc = XCreateGC(st->dpy, st->window, GCForeground, &st->gcv);

    st->gcv.foreground = get_pixel_resource(st->dpy, st->xgwa.colormap,
                                        "background", "Background");
    st->bgc = XCreateGC(st->dpy, st->window, GCForeground, &st->gcv);

    return st;
}

static unsigned long
cloudlife_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;

  if (st->cycle_colors) {
    if (st->colortimer == 0) {
      st->colortimer = st->cycle_colors;
      if( st->colorindex == 0 ) 
        st->colorindex = st->ncolors;
      st->colorindex--;
      XSetForeground(st->dpy, st->fgc, st->colors[st->colorindex].pixel);
    }
    st->colortimer--;
  } 

  XGetWindowAttributes(st->dpy, st->window, &st->xgwa);
  if (st->field->height != st->xgwa.height / (1 << st->field->cell_size) + 2 ||
      st->field->width != st->xgwa.width / (1 << st->field->cell_size) + 2) {

    resize_field(st->field, st->xgwa.width / (1 << st->field->cell_size) + 2,
                 st->xgwa.height / (1 << st->field->cell_size) + 2);
    populate_field(st->field, st->density);
  }

  draw_field(st, st->field);

  if (do_tick(st->field) < (st->field->height + st->field->width) / 4) {
    populate_field(st->field, st->density);
  }

  if (st->cycles % (st->field->max_age /2) == 0) {
    populate_edges(st->field, st->density);
    do_tick(st->field);
    populate_edges(st->field, 0);
  }

  st->cycles++;

#ifdef TIME_ME
  if (st->cycles % st->field->max_age == 0) {
    printf("%g s.\n",
           ((time(NULL) - st->start_time) * 1000.0) / st->cycles);
  }
#endif

  return (st->cycle_delay);
}


static void
cloudlife_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
cloudlife_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (screenhack_event_helper (dpy, window, event))
    {
      XClearWindow (dpy, window);
      st->cycles = 0;
      if (st->field) {
        free (st->field->cells);
        free (st->field->new_cells);
        free (st->field);
      }
      st->field = init_field(st);
      return True;
    }
  return False;
}

static void
cloudlife_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st->field->cells);
  free (st->field->new_cells);
  free (st->field);
  free (st->colors);
  XFreeGC (dpy, st->fgc);
  XFreeGC (dpy, st->bgc);
  free (st);
}


static const char *cloudlife_defaults[] = {
    ".background:	black",
    ".foreground:	blue",
    "*fpsSolid: 	true",
    "*cycleDelay:	25000",
    "*cycleColors:      2",
    "*ncolors:          64",
    "*maxAge:		64",
    "*initialDensity:	30",
    "*cellSize:		3",
#ifdef HAVE_MOBILE
    "*ignoreRotation:   True",
#endif
    0
};

static XrmOptionDescRec cloudlife_options[] = {
    {"-background", ".background", XrmoptionSepArg, 0},
    {"-foreground", ".foreground", XrmoptionSepArg, 0},
    {"-cycle-delay", ".cycleDelay", XrmoptionSepArg, 0},
    {"-cycle-colors", ".cycleColors", XrmoptionSepArg, 0},
    {"-ncolors", ".ncolors", XrmoptionSepArg, 0},
    {"-cell-size", ".cellSize", XrmoptionSepArg, 0},
    {"-initial-density", ".initialDensity", XrmoptionSepArg, 0},
    {"-max-age", ".maxAge", XrmoptionSepArg, 0},
    {0, 0, 0, 0}
};


XSCREENSAVER_MODULE ("CloudLife", cloudlife)
