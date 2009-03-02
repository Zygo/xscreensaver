/* -*- Mode: C; tab-width: 4 -*- */
/* swirl --- swirly patterns */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)swirl.c	5.09 2003/06/30 xlockmore";

#endif

/*-
 * Copyright (c) 1994 M.Dobie <mrd@ecs.soton.ac.uk>
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * Revision History:
 * 30-Jun-2003: Changed writeable mode to be more consistent with
 *              xscreensaver's starfish, looks like I am not going to
 *              port the variable speed colormapping when done drawing.
 * 01-Nov-2000: Allocation checks
 * 13-May-1997: jwz@jwz.org: turned into a standalone program.
 * 21-Apr-1995: improved startup time for TrueColour displays
 *              (limited to 16bpp to save memory) S.Early <sde1000@cam.ac.uk>
 * 09-Jan-1995: fixed colour maps (more colourful) and the image now spirals
 *              outwards from the centre with a fixed number of points drawn
 *              every iteration. Thanks to M.Dobie <mrd@ecs.soton.ac.uk>.
 * 1994: written.   Copyright (c) 1994 M.Dobie <mrd@ecs.soton.ac.uk>
 *       based on original code by R.Taylor
 */

#ifdef STANDALONE
#define MODE_swirl
#define PROGCLASS "Swirl"
#define HACK_INIT init_swirl
#define HACK_DRAW draw_swirl
#define swirl_opts xlockmore_opts
#define DEFAULTS "*delay: 5000 \n" \
 "*count: 5 \n" \
 "*ncolors: 200 \n"
#define SMOOTH_COLORS
#define WRITABLE_COLORS
#include "xlockmore.h"		/* from the xscreensaver distribution */
#else /* !STANDALONE */
#include "xlock.h"		/* from the xlockmore distribution */
#include "color.h"
#endif /* !STANDALONE */

#ifdef MODE_swirl

#define DEF_CYCLE "True"

static Bool cycle_p;

static XrmOptionDescRec opts[] =
{
  {(char *) "-cycle", (char *) ".swirl.cycle", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+cycle", (char *) ".swirl.cycle", XrmoptionNoArg, (caddr_t) "off"}
};

static argtype vars[] =
{
  {(void *) & cycle_p, (char *) "cycle", (char *) "Cycle", (char *) DEF_CYCLE, t_Bool}
};

static OptionStruct desc[] =
{
  {(char *) "-/+cycle", (char *) "turn on/off colour cycling"}
};

ModeSpecOpt swirl_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   swirl_description =
{"swirl", "init_swirl", "draw_swirl", "release_swirl",
 "refresh_swirl", "init_swirl", (char *) NULL, &swirl_opts,
 5000, 5, 1, 1, 64, 1.0, "",
 "Shows animated swirling patterns", 0, NULL};

#endif

#include <time.h>

#define MASS            4	/* maximum mass of a knot */
#define MIN_RES         5	/* minimim resolution (>= MIN_RES) */
#define MAX_RES         1	/* maximum resolution (>0) */
#define TWO_PLANE_PCNT  30	/* probability for two plane mode (0-100) */
#define RESTART         2500	/* number of cycles before restart */
#define BATCH_DRAW      100	/* points to draw per iteration */

/* knot types */
typedef enum {
	NONE = 0,
	ORBIT = (1 << 0),
	WHEEL = (1 << 1),
	PICASSO = (1 << 2),
	RAY = (1 << 3),
	HOOK = (1 << 4),
	ALL = (1 << 5)
} KNOT_T;

/* a knot */
typedef struct Knot {
	int         x, y;	/* position */
	int         m;		/* mass */
	KNOT_T      t;		/* type in the first (or only) plane */
	KNOT_T      T;		/* type in second plane if there is one */
	int         M;		/* mass in second plane if there is one */
} KNOT     , *KNOT_P;

/* drawing direction */
typedef enum {
	DRAW_RIGHT, DRAW_DOWN, DRAW_LEFT, DRAW_UP
} DIR_T;

/****************************************************************/

/* data associated with a swirl window */
typedef struct swirl_data {
	/* window paramaters */
	int         width, height;	/* window size */
	int         depth;	/* depth */
	int         rdepth;	/* real depth (for XImage) */
	Visual     *visual;	/* visual */

	/* swirl drawing parameters */
	int         n_knots;	/* number of knots */
	KNOT_P      knots;	/* knot details */
	KNOT_T      knot_type;	/* general type of knots */
	int         resolution;	/* drawing resolution, 1..5 */
	int         max_resolution;	/* maximum resolution, MAX_RES */
	int         r;		/* pixel step */
	Bool        two_plane;	/* two plane mode? */
	Bool        first_plane;	/* doing first plane? */
	int         start_again;	/* when to restart */

	/* spiral drawing parameters */
	int         x, y;	/* current point */
	DIR_T       dir;	/* current direction */
	int         dir_todo, dir_done;		/* how many points in current direction? */
	int         batch_todo, batch_done;	/* how many points in this batch */
	Bool        started, drawing;	/* are we drawing? */
	Bool        off_screen;

	/* image stuff */
	XImage     *ximage;

	/* colours stuff */
	int         shift;	/* colourmap shift */
	int         dshift;	/* colourmap shift while drawing */
	unsigned int cur_color;
	GC          gc;
	Colormap    cmap;
	unsigned long blackpixel, whitepixel, fg, bg;
	int direction;
	XColor     *colors;
	int         ncolors;
	Bool        cycle_p, mono_p, no_colors, reverse;
	ModeInfo   *mi;
} swirlstruct;

/* an array of swirls for each screen */
static swirlstruct *swirls = (swirlstruct *) NULL;

/*-
   random_no

   Return a random integer between 0 and n inclusive

   -      n is the maximum number

   Returns a random integer */

static int
random_no(unsigned int n)
{
	return ((int) ((n + 1) * (double) LRAND() / MAXRAND));
}

/****************************************************************/

static void
free_swirl(Display *display, swirlstruct *sp)
{
  ModeInfo *mi = sp->mi;

	if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
		MI_WHITE_PIXEL(mi) = sp->whitepixel;
		MI_BLACK_PIXEL(mi) = sp->blackpixel;
#ifndef STANDALONE
		MI_FG_PIXEL(mi) = sp->fg;
		MI_BG_PIXEL(mi) = sp->bg;
#endif
		if (sp->colors != NULL) {
			if (sp->ncolors && !sp->no_colors)
				free_colors(display, sp->cmap, sp->colors,
					sp->ncolors);
			free(sp->colors);
			sp->colors = (XColor *) NULL;
		}
		if (sp->cmap != None) {
			XFreeColormap(display, sp->cmap);
			sp->cmap = None;
		}
	}
	if (sp->gc != None) {
		XFreeGC(display, sp->gc);
		sp->gc = None;
	}
	if (sp->ximage != None) {
		(void) XDestroyImage(sp->ximage);
		sp->ximage = None;
	}
	if (sp->knots != NULL) {
		free(sp->knots);
		sp->knots = (KNOT_P) NULL;
	}
}

#ifndef STANDALONE
extern char *background;
extern char *foreground;
#endif

/*-
   initialise_swirl

   Initialise all the swirl data

   -      swirl is the swirl data */

static void
initialise_swirl(swirlstruct *sp)
{
	sp->width = 0;	/* width and height of window */
	sp->height = 0;
	sp->depth = 1;
	sp->rdepth = 1;
	sp->visual = (Visual *) NULL;
	sp->resolution = MIN_RES + 1;	/* current resolution */
	sp->max_resolution = MAX_RES;	/* maximum resolution */
	sp->n_knots = 0;	/* number of knots */
	sp->knot_type = ALL;	/* general type of knots */
	sp->two_plane = False;	/* two plane mode? */
	sp->first_plane = False;	/* doing first plane? */
	sp->start_again = -1;	/* restart counter */

	/* drawing parameters */
	sp->x = 0;
	sp->y = 0;
	sp->started = False;
	sp->drawing = False;

	/* image stuff */
	sp->ximage = None;
}

/****************************************************************/

/*-
 * initialise_image
 *
 * Initialise the image for drawing to
 *
 * -      swirl is the swirl data
 */
static Bool
initialise_image(Display * dpy, swirlstruct *sp)
{
	unsigned int pad;
	int         bytes_per_line;
	int         image_depth = sp->rdepth;
	int         data_depth = image_depth;
	unsigned char *image;	/* image data */

	/* On SGIs at least, using an XImage of depth 24 on a Visual of depth 24
	 *  requires the XImage data to use 32 bits per pixel.  I don't understand
	 *  how one is supposed to determine this -- maybe XListPixmapFormats?
	 *  But on systems that don't work this way, allocating 32 bpp instead of
	 *  24 will be wasteful but non-fatal.  -- jwz, 16-May-97.
	 */
	if (data_depth >= 24 && data_depth < 32)
		data_depth = 32;

	/* get the bitmap pad */
	pad = BitmapPad(dpy);
	/* destroy the old image (destroy XImage and data) */
	if (sp->ximage != NULL)
		(void) XDestroyImage(sp->ximage);

	/* how many bytes per line? (bits rounded up to pad) */
	bytes_per_line = ((sp->width * data_depth + pad - 1) / pad) * (pad / 8);

	/* allocate space for the image */
	if ((image = (unsigned char *) calloc(bytes_per_line *
			sp->height, 1)) == NULL) {
		return False;
	}

	/* create an ximage with this */
	if ((sp->ximage = XCreateImage(dpy, sp->visual, image_depth, ZPixmap,
				     0, (char *) image, sp->width,
				     sp->height, pad, bytes_per_line)) == None) {
		free(image);
		return False;
	}
	return True;
}

/*-
 * create_knots
 *
 * Initialise the array of knot
 *
 * swirl is the swirl data
 */
static Bool
create_knots(swirlstruct *sp)
{
	int         k;
	Bool        orbit, wheel, picasso, ray, hook;
	KNOT_P      knot;

	/* create array for knots */
	if (sp->knots)
		free(sp->knots);
	if ((sp->knots = (KNOT_P) calloc(sp->n_knots,
			sizeof (KNOT))) == NULL) {
		return False;
	}

	/* no knots yet */
	orbit = wheel = picasso = ray = hook = False;

	/* what types do we have? */
	if ((int) sp->knot_type & (int) ALL) {
		orbit = wheel = ray = hook = True;
	} else {
		if ((int) sp->knot_type & (int) ORBIT)
			orbit = True;
		if ((int) sp->knot_type & (int) WHEEL)
			wheel = True;
		if ((int) sp->knot_type & (int) PICASSO)
			picasso = True;
		if ((int) sp->knot_type & (int) RAY)
			ray = True;
		if ((int) sp->knot_type & (int) HOOK)
			hook = True;
	}

	/* initialise each knot */
	knot = sp->knots;
	for (k = 0; k < sp->n_knots; k++) {
		/* position */
		knot->x = random_no((unsigned int) sp->width);
		knot->y = random_no((unsigned int) sp->height);

		/* mass */
		knot->m = random_no(MASS) + 1;

		/* can be negative */
		if (random_no(100) > 50)
			knot->m *= -1;

		/* type */
		knot->t = NONE;
		while (knot->t == NONE) {
			/* choose a random one from the types available */
			switch (random_no(4)) {
				case 0:
					if (orbit)
						knot->t = ORBIT;
					break;
				case 1:
					if (wheel)
						knot->t = WHEEL;
					break;
				case 2:
					if (picasso)
						knot->t = PICASSO;
					break;
				case 3:
					if (ray)
						knot->t = RAY;
					break;
				case 4:
					if (hook)
						knot->t = HOOK;
					break;
			}
		}

		/* if two planes, do same for second plane */
		if (sp->two_plane) {
			knot->T = NONE;
			while (knot->T == NONE || knot->T == knot->t) {
				/* choose a different type */
				switch (random_no(4)) {
					case 0:
						if (orbit)
							knot->T = ORBIT;
						break;
					case 1:
						if (wheel)
							knot->T = WHEEL;
						break;
					case 2:
						if (picasso)
							knot->T = PICASSO;
						break;
					case 3:
						if (ray)
							knot->T = RAY;
						break;
					case 4:
						if (hook)
							knot->T = HOOK;
						break;
				}
			}
		}
		/* next knot */
		knot++;
	}
	return True;
}

/****************************************************************/

/*-
 * do_point
 *
 * Work out the pixel value at i, j. Ensure it does not clash with BlackPixel
 * or WhitePixel.
 *
 * -      swirl is the swirl data
 * -      i, j is the point to calculate
 *
 * Returns the value of the point
 */
static unsigned long
do_point(swirlstruct *sp, int i, int j)
{
	int         tT, k, add, value;
	unsigned long colour_value;
	double      dx, dy, theta, dist;
	int         ncolours, qcolours;
	double      rads;
	KNOT_P      knot;
	ModeInfo   *mi = sp->mi;

	/* how many colours? */
	ncolours = sp->ncolors;
	qcolours = ncolours / 4;

	/* colour step round a circle */
	rads = (double) ncolours / (2.0 * M_PI);

	/* start at zero */
	value = 0;

	/* go through all the knots */
	knot = sp->knots;
	for (k = 0; k < sp->n_knots; k++) {
		dx = i - knot->x;
		dy = j - knot->y;

		/* in two_plane mode get the appropriate knot type */
		if (sp->two_plane)
			tT = (int) ((sp->first_plane) ? knot->t : knot->T);
		else
			tT = (int) knot->t;

		/* distance from knot */
		dist = sqrt(dx * dx + dy * dy);

		/* nothing to add at first */
		add = 0;

		/* work out the contribution (if close enough) */
		if (dist > 0.1)
			switch (tT) {
				case ORBIT:
					add = (int) (ncolours / (1.0 + 0.01 * abs(knot->m) * dist));
					break;
				case WHEEL:
					/* Avoid atan2: DOMAIN error message */
					if (dy == 0.0 && dx == 0.0)
						theta = 1.0;
					else
						theta = (atan2(dy, dx) + M_PI) / M_PI;
					if (theta < 1.0)
						add = (int) (ncolours * theta +
						  sin(0.1 * knot->m * dist) *
						qcolours * exp(-0.01 * dist));
					else
						add = (int) (ncolours * (theta - 1.0) +
						  sin(0.1 * knot->m * dist) *
						qcolours * exp(-0.01 * dist));
					break;
				case PICASSO:
					add = (int) (ncolours *
					  fabs(cos(0.002 * knot->m * dist)));
					break;
				case RAY:
					/* Avoid atan2: DOMAIN error message */
					if (dy == 0.0 && dx == 0.0)
						add = 0;
					else
						add = (int) (ncolours * fabs(sin(2.0 * atan2(dy, dx))));

					break;
				case HOOK:
					/* Avoid atan2: DOMAIN error message */
					if (dy == 0.0 && dx == 0.0)
						add = (int) (0.05 * (abs(knot->m) - 1) * dist);
					else
						add = (int) (rads * atan2(dy, dx) +
							     0.05 * (abs(knot->m) - 1) * dist);
					break;
			}
		/* for a positive mass add on the contribution else take it off */
		if (knot->m > 0)
			value += add;
		else
			value -= add;

		/* next knot */
		knot++;
	}

	/* toggle plane */
	sp->first_plane = (!sp->first_plane);

	if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
		if (ncolours < 2) {
			ncolours = 2;
		}
		/* make sure we handle negative values properly */
		if (value >= 0)
			colour_value = (value % ncolours) + 2;
		else
			colour_value = ncolours - (abs((int) value) % (ncolours - 1));

		/* definitely make sure it is in range */
		colour_value = colour_value % ncolours;

                if (sp->mono_p) {
                        return colour_value;
                } else {
                        return sp->colors[colour_value].pixel;
                }
        } else {
		if (value >= 0)
			colour_value = (value % MI_NPIXELS(mi)) + 2;
		else
			colour_value = MI_NPIXELS(mi) - (abs((int) value) % (MI_NPIXELS(mi) - 1));
		/* definitely make sure it is in range */
		colour_value = colour_value % MI_NPIXELS(mi);
                if (MI_NPIXELS(mi) > 2) {
                        return MI_PIXEL(mi, colour_value);
		} else if (colour_value % 2)
                        return MI_BLACK_PIXEL(mi);
                else
                        return MI_WHITE_PIXEL(mi);
        }
}

/****************************************************************/

/*-
 * draw_block
 *
 * Draw a square block of points with the same value.
 *
 * -      ximage is the XImage to draw on.
 * -      x, y is the top left corner
 * -      s is the length of each side
 * -      v is the value
 */
static void
draw_block(XImage * ximage, int x, int y, int s, unsigned long v)
{
	int         a, b;

	for (a = 0; a < s; a++)
		for (b = 0; b < s; b++) {
			XPutPixel(ximage, x + b, y + a, v);
		}
}

/****************************************************************/

/*-
 * draw_point  Draw the current point in a swirl pattern onto the XImage
 *
 * -    swirl is the swirl
 * -    win is the window to update
 */
static void
draw_point(ModeInfo * mi, swirlstruct *sp)
{
	int         r;
	int         x, y;

	/* get current point coordinates and resolution */
	x = sp->x;
	y = sp->y;
	r = sp->r;

	/* check we are within the window */
	if ((x < 0) || (x > sp->width - r) || (y < 0) || (y > sp->height - r))
		return;

	/* what style are we drawing? */
	if (sp->two_plane) {
		int         r2;

		/* halve the block size */
		r2 = r / 2;

		/* interleave blocks at half r */
		draw_block(sp->ximage, x, y, r2, do_point(sp, x, y));
		draw_block(sp->ximage, x + r2, y, r2, do_point(sp, x + r2, y));
		draw_block(sp->ximage, x + r2, y + r2, r2, do_point(sp,
							    x + r2, y + r2));
		draw_block(sp->ximage, x, y + r2, r2, do_point(sp, x, y + r2));
	} else
		draw_block(sp->ximage, x, y, r, do_point(sp, x, y));

	/* update the screen */
/*-
 * PURIFY 4.0.1 on SunOS4 and on Solaris 2 reports a 256 byte memory leak on
 * the next line. */
	(void) XPutImage(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi), sp->ximage,
			 x, y, x, y, r, r);
}

/****************************************************************/

/*-
 * next_point  Move to the next point in the spiral pattern
 *  -    swirl is the swirl
 *  -    win is the window to update
 */
static void
next_point(swirlstruct *sp)
{
	/* more to do in this direction? */
	if (sp->dir_done < sp->dir_todo) {
		/* move in the current direction */
		switch (sp->dir) {
			case DRAW_RIGHT:
				sp->x += sp->r;
				break;
			case DRAW_DOWN:
				sp->y += sp->r;
				break;
			case DRAW_LEFT:
				sp->x -= sp->r;
				break;
			case DRAW_UP:
				sp->y -= sp->r;
				break;
		}

		/* done another point */
		sp->dir_done++;
	} else {
		/* none drawn yet */
		sp->dir_done = 0;

		/* change direction - check and record if off screen */
		switch (sp->dir) {
			case DRAW_RIGHT:
				sp->dir = DRAW_DOWN;
				if (sp->x > sp->width - sp->r) {
					/* skip these points */
					sp->dir_done = sp->dir_todo;
					sp->y += (sp->dir_todo * sp->r);

					/* check for finish */
					if (sp->off_screen)
						sp->drawing = False;
					sp->off_screen = True;
				} else
					sp->off_screen = False;
				break;
			case DRAW_DOWN:
				sp->dir = DRAW_LEFT;
				sp->dir_todo++;
				if (sp->y > sp->height - sp->r) {
					/* skip these points */
					sp->dir_done = sp->dir_todo;
					sp->x -= (sp->dir_todo * sp->r);

					/* check for finish */
					if (sp->off_screen)
						sp->drawing = False;
					sp->off_screen = True;
				} else
					sp->off_screen = False;
				break;
			case DRAW_LEFT:
				sp->dir = DRAW_UP;
				if (sp->x < 0) {
					/* skip these points */
					sp->dir_done = sp->dir_todo;
					sp->y -= (sp->dir_todo * sp->r);

					/* check for finish */
					if (sp->off_screen)
						sp->drawing = False;
					sp->off_screen = True;
				} else
					sp->off_screen = False;
				break;
			case DRAW_UP:
				sp->dir = DRAW_RIGHT;
				sp->dir_todo++;
				if (sp->y < 0) {
					/* skip these points */
					sp->dir_done = sp->dir_todo;
					sp->x += (sp->dir_todo * sp->r);

					/* check for finish */
					if (sp->off_screen)
						sp->drawing = False;
					sp->off_screen = True;
				} else
					sp->off_screen = False;
				break;
		}
	}
}

/****************************************************************/

/*-
 * init_swirl
 *
 * Initialise things for swirling
 *
 * -      win is the window to draw in
 */
void
init_swirl(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	swirlstruct *sp;

	/* does the swirls array exist? */
	if (swirls == NULL) {
		int         i;

		/* allocate an array, one entry for each screen */
		if ((swirls = (swirlstruct *) calloc(MI_NUM_SCREENS(mi),
				sizeof (swirlstruct))) == NULL)
			return;

		/* initialise them all */
		for (i = 0; i < MI_NUM_SCREENS(mi); i++)
			initialise_swirl(&swirls[i]);
	}
	/* get a pointer to this swirl */
	sp = &(swirls[MI_SCREEN(mi)]);
	sp->mi = mi;

	/* get window parameters */
	sp->width = MI_WIDTH(mi);
	sp->height = MI_HEIGHT(mi);
	sp->depth = MI_DEPTH(mi);
	sp->rdepth = sp->depth;
	sp->visual = MI_VISUAL(mi);

	if (sp->depth > 16)
		sp->depth = 16;

	/* initialise image for speeding up drawing */
	if (!initialise_image(display, sp)) {
		free_swirl(display, sp);
		return;
	}
	MI_CLEARWINDOW(mi);

	if (!sp->gc) {
		if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
			XColor      color;

#ifndef STANDALONE
			sp->fg = MI_FG_PIXEL(mi);
			sp->bg = MI_BG_PIXEL(mi);
#endif
			sp->blackpixel = MI_BLACK_PIXEL(mi);
			sp->whitepixel = MI_WHITE_PIXEL(mi);
			if ((sp->cmap = XCreateColormap(display, window,
					MI_VISUAL(mi), AllocNone)) == None) {
				free_swirl(display, sp);
				return;
			}
			XSetWindowColormap(display, window, sp->cmap);
			(void) XParseColor(display, sp->cmap, "black", &color);
			(void) XAllocColor(display, sp->cmap, &color);
			MI_BLACK_PIXEL(mi) = color.pixel;
			(void) XParseColor(display, sp->cmap, "white", &color);
			(void) XAllocColor(display, sp->cmap, &color);
			MI_WHITE_PIXEL(mi) = color.pixel;
#ifndef STANDALONE
			(void) XParseColor(display, sp->cmap, background, &color);
			(void) XAllocColor(display, sp->cmap, &color);
			MI_BG_PIXEL(mi) = color.pixel;
			(void) XParseColor(display, sp->cmap, foreground, &color);
			(void) XAllocColor(display, sp->cmap, &color);
			MI_FG_PIXEL(mi) = color.pixel;
#endif
			sp->colors = (XColor *) NULL;
			sp->ncolors = 0;
		}
		if ((sp->gc = XCreateGC(display, MI_WINDOW(mi),
			     (unsigned long) 0, (XGCValues *) NULL)) == None) {
			free_swirl(display, sp);
			return;
		}
	}
	MI_CLEARWINDOW(mi);

  /* Set up colour map */
  sp->direction = (LRAND() & 1) ? 1 : -1;
  if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
    if (sp->colors != NULL) {
      if (sp->ncolors && !sp->no_colors)
        free_colors(display, sp->cmap, sp->colors, sp->ncolors);
      free(sp->colors);
      sp->colors = (XColor *) NULL;
    }
    sp->ncolors = MI_NCOLORS(mi);
    if (sp->ncolors < 2)
      sp->ncolors = 2;
    if (sp->ncolors <= 2)
      sp->mono_p = True;
    else
      sp->mono_p = False;

    if (sp->mono_p)
      sp->colors = (XColor *) NULL;
    else
      if ((sp->colors = (XColor *) malloc(sizeof (*sp->colors) *
          (sp->ncolors + 1))) == NULL) {
        free_swirl(display, sp);
        return;
      }
    sp->cycle_p = has_writable_cells(mi);
    if (sp->cycle_p) {
      if (MI_IS_FULLRANDOM(mi)) {
        if (!NRAND(8))
          sp->cycle_p = False;
        else
          sp->cycle_p = True;
      } else {
        sp->cycle_p = cycle_p;
      }
    }
    if (!sp->mono_p) {
      if (!(LRAND() % 10))
        make_random_colormap(
#if STANDALONE
            display, MI_WINDOW(mi),
#else
            mi,
#endif
              sp->cmap, sp->colors, &sp->ncolors,
              True, True, &sp->cycle_p);
      else if (!(LRAND() % 2))
        make_uniform_colormap(
#if STANDALONE
            display, MI_WINDOW(mi),
#else
            mi,
#endif
                  sp->cmap, sp->colors, &sp->ncolors,
                  True, &sp->cycle_p);
      else
        make_smooth_colormap(
#if STANDALONE
            display, MI_WINDOW(mi),
#else
            mi,
#endif
                 sp->cmap, sp->colors, &sp->ncolors,
                 True, &sp->cycle_p);
    }
    XInstallColormap(display, sp->cmap);
    if (sp->ncolors < 2) {
      sp->ncolors = 2;
      sp->no_colors = True;
    } else
      sp->no_colors = False;
    if (sp->ncolors <= 2)
      sp->mono_p = True;

    if (sp->mono_p)
      sp->cycle_p = False;

  }
  if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
                if (sp->mono_p) {
			sp->cur_color = MI_BLACK_PIXEL(mi);
		}
  }

	/* resolution starts off chunky */
	sp->resolution = MIN_RES + 1;

	/* calculate the pixel step for this resulution */
	sp->r = (1 << (sp->resolution - 1));

	/* how many knots? */
	sp->n_knots = random_no((unsigned int) MI_COUNT(mi) / 2) +
		MI_COUNT(mi) + 1;

	/* what type of knots? */
	sp->knot_type = ALL;	/* for now */

	/* use two_plane mode occaisionally */
	if (random_no(100) <= TWO_PLANE_PCNT) {
		sp->two_plane = sp->first_plane = True;
		sp->max_resolution = 2;
	} else
		sp->two_plane = False;

	/* fix the knot values */
	if (!create_knots(sp)) {
		free_swirl(display, sp);
		return;
	}

	/* we are off */
	sp->started = True;
	sp->drawing = False;
}

/****************************************************************/

/*-
 * draw_swirl
 *
 * Draw one iteration of swirling
 *
 * -      win is the window to draw in
 */
void
draw_swirl(ModeInfo * mi)
{
	swirlstruct *sp;

	if (swirls == NULL)
		return;
	sp = &(swirls[MI_SCREEN(mi)]);
	if (sp->knots == NULL)
		return;

	MI_IS_DRAWN(mi) = True;

	/* are we going? */
	if (sp->started) {
		/* in the middle of drawing? */
		if (sp->drawing) {
		  if(sp->cycle_p) {
		 rotate_colors(MI_DISPLAY(mi), sp->cmap, sp->colors, sp->ncolors, sp->direction);
		    if (!(LRAND() % 1000))
      sp->direction = -sp->direction;
                 }
			/* draw a batch of points */
			sp->batch_todo = BATCH_DRAW;
			while ((sp->batch_todo > 0) && sp->drawing) {
				/* draw a point */
				draw_point(mi, sp);

				/* move to the next point */
				next_point(sp);

				/* done a point */
				sp->batch_todo--;
			}
		} else {
		  if(sp->cycle_p) {
		 rotate_colors(MI_DISPLAY(mi), sp->cmap, sp->colors, sp->ncolors, sp->direction);
		    if (!(LRAND() % 1000))
      sp->direction = -sp->direction;
                 }

			/* time for a higher resolution? */
			if (sp->resolution > sp->max_resolution) {
				/* move to higher resolution */
				sp->resolution--;

				/* calculate the pixel step for this resulution */
				sp->r = (1 << (sp->resolution - 1));

				/* start drawing again */
				sp->drawing = True;

				/* start in the middle of the screen */
				sp->x = (sp->width - sp->r) / 2;
				sp->y = (sp->height - sp->r) / 2;

				/* initialise spiral drawing parameters */
				sp->dir = DRAW_RIGHT;
				sp->dir_todo = 1;
				sp->dir_done = 0;
			} else {
				/* all done, decide when to restart */
				if (sp->start_again == -1) {
					/* start the counter */
					sp->start_again = RESTART;
				} else if (sp->start_again == 0) {
					/* reset the counter */
					sp->start_again = -1;

					/* start again */
					init_swirl(mi);
				} else
					/* decrement the counter */
					sp->start_again--;
			}
		}
	}
}

/****************************************************************/

void
release_swirl(ModeInfo * mi)
{
	/* does the swirls array exist? */
	if (swirls != NULL) {
		int         screen;

		/* free them all */
		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_swirl(MI_DISPLAY(mi), &swirls[screen]);
		/* deallocate an array, one entry for each screen */
		free(swirls);
		swirls = (swirlstruct *) NULL;
	}
}

/****************************************************************/

void
refresh_swirl(ModeInfo * mi)
{
	swirlstruct *sp;

	if (swirls == NULL)
		return;
	sp = &swirls[MI_SCREEN(mi)];

	if (sp->started) {
		MI_CLEARWINDOW(mi);
		if (sp->drawing)
			sp->resolution = sp->resolution + 1;
		sp->drawing = False;
	}
}

#endif /* MODE_swirl */
