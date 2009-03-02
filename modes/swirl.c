/* -*- Mode: C; tab-width: 4 -*- */
/* swirl --- swirly patterns */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)swirl.c	4.07 97/11/24 xlockmore";

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
 * 13-May-97: jwz@jwz.org: turned into a standalone program.
 * 21-Apr-95: improved startup time for TrueColour displays
 *            (limited to 16bpp to save memory) S.Early <sde1000@cam.ac.uk>
 * 09-Jan-95: fixed colour maps (more colourful) and the image now spirals
 *            outwards from the centre with a fixed number of points drawn
 *            every iteration. Thanks to M.Dobie <mrd@ecs.soton.ac.uk>.
 * 1994:      written.   Copyright (c) 1994 M.Dobie <mrd@ecs.soton.ac.uk>
 *            based on original code by R.Taylor
 */

#ifdef STANDALONE
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
#include <X11/Xutil.h>
#else /* !STANDALONE */
#include "xlock.h"		/* from the xlockmore distribution */
#include "vis.h"
#include "color.h"
#endif /* !STANDALONE */

#ifdef MODE_swirl

ModeSpecOpt swirl_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   swirl_description =
{"swirl", "init_swirl", "draw_swirl", "release_swirl",
 "refresh_swirl", "init_swirl", NULL, &swirl_opts,
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

/* a colour specification */
typedef struct Colour {
	unsigned short r, g, b;
} COLOUR   , *COLOUR_P;

/* drawing direction */
typedef enum {
	DRAW_RIGHT, DRAW_DOWN, DRAW_LEFT, DRAW_UP
} DIR_T;

/****************************************************************/

/* data associated with a swirl window */
typedef struct swirl_data {
	/* window paramaters */
	Window      win;	/* the window */
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
	DIR_T       direction;	/* current direction */
	int         dir_todo, dir_done;		/* how many points in current direction? */
	int         batch_todo, batch_done;	/* how many points in this batch */
	Bool        started, drawing;	/* are we drawing? */

	/* image stuff */
	unsigned char *image;	/* image data */
	XImage     *ximage;

	/* colours stuff */
	int         colours;	/* how many colours possible */
	int         dcolours;	/* how many colours for shading */
	Colormap    cmap;	/* colour map for the window */
	XColor     *rgb_values;	/* colour definitions array */
	Bool        off_screen;
#ifndef STANDALONE
	Bool        fixed_colourmap;	/* fixed colourmap? */
	int         current_map;	/* current colour map, 0..dcolours-1 */
	unsigned long black, white, bg, fg;	/* black and white pixel values */
	int         shift;	/* colourmap shift */
	int         dshift;	/* colourmap shift while drawing */
	XColor      bgcol, fgcol;	/* foreground and background colour specs */
#endif				/* !STANDALONE */
} SWIRL    , *SWIRL_P;

#ifndef STANDALONE
#define SWIRLCOLOURS 13
/* basic colours */
static COLOUR basic_colours[SWIRLCOLOURS];

#endif /* !STANDALONE */

/* an array of swirls for each screen */
static SWIRL_P swirls = NULL;

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

/*-
   initialise_swirl

   Initialise all the swirl data

   -      swirl is the swirl data */

static void
initialise_swirl(SWIRL_P swirl)
{
	swirl->width = 0;	/* width and height of window */
	swirl->height = 0;
	swirl->depth = 1;
	swirl->rdepth = 1;
	swirl->visual = NULL;
	swirl->resolution = MIN_RES + 1;	/* current resolution */
	swirl->max_resolution = MAX_RES;	/* maximum resolution */
	swirl->n_knots = 0;	/* number of knots */
	swirl->knot_type = ALL;	/* general type of knots */
	swirl->two_plane = False;	/* two plane mode? */
	swirl->first_plane = False;	/* doing first plane? */
	swirl->start_again = -1;	/* restart counter */

	/* drawing parameters */
	swirl->x = 0;
	swirl->y = 0;
	swirl->started = False;
	swirl->drawing = False;

	/* image stuff */
	swirl->image = NULL;	/* image data */
	swirl->ximage = NULL;

	/* colours stuff */
	swirl->colours = 0;	/* how many colours possible */
	swirl->dcolours = 0;	/* how many colours for shading */
	swirl->cmap = (Colormap) NULL;
	swirl->rgb_values = NULL;	/* colour definitions array */
}

/****************************************************************/

/*-
 * initialise_image
 *
 * Initialise the image for drawing to
 *
 * -      swirl is the swirl data
 */
static void
initialise_image(Display * dpy, SWIRL_P swirl)
{
	unsigned int pad;
	int         bytes_per_line;
	int         image_depth = swirl->rdepth;
	int         data_depth = image_depth;

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
	if (swirl->ximage != NULL)
		(void) XDestroyImage(swirl->ximage);

	/* how many bytes per line? (bits rounded up to pad) */
	bytes_per_line = ((swirl->width * data_depth + pad - 1) / pad) * (pad / 8);

	/* allocate space for the image */
	swirl->image = (unsigned char *) calloc(bytes_per_line * swirl->height, 1);

	/* create an ximage with this */
	swirl->ximage = XCreateImage(dpy, swirl->visual, image_depth, ZPixmap,
				     0, (char *) swirl->image, swirl->width,
				     swirl->height, pad, bytes_per_line);
}

/****************************************************************/
#ifndef STANDALONE

/*-
 * initialise_colours
 *
 * Initialise the list of colours from which the colourmaps are derived
 *
 * -      colours is the array to initialise
 * -      saturation is the saturation value to use 0->grey,
 *            1.0->full saturation
 */
static void
initialise_colours(COLOUR * colours, float saturate)
{
	int         i;

	/* start off fully saturated, medium and bright colours */
	colours[0].r = 0xA000;
	colours[0].g = 0x0000;
	colours[0].b = 0x0000;
	colours[1].r = 0xD000;
	colours[1].g = 0x0000;
	colours[1].b = 0x0000;
	colours[2].r = 0x0000;
	colours[2].g = 0x6000;
	colours[2].b = 0x0000;
	colours[3].r = 0x0000;
	colours[3].g = 0x9000;
	colours[3].b = 0x0000;
	colours[4].r = 0x0000;
	colours[4].g = 0x0000;
	colours[4].b = 0xC000;
	colours[5].r = 0x0000;
	colours[5].g = 0x0000;
	colours[5].b = 0xF000;
	colours[6].r = 0xA000;
	colours[6].g = 0x6000;
	colours[6].b = 0x0000;
	colours[7].r = 0xD000;
	colours[7].g = 0x9000;
	colours[7].b = 0x0000;
	colours[8].r = 0xA000;
	colours[8].g = 0x0000;
	colours[8].b = 0xC000;
	colours[9].r = 0xD000;
	colours[9].g = 0x0000;
	colours[9].b = 0xF000;
	colours[10].r = 0x0000;
	colours[10].g = 0x6000;
	colours[10].b = 0xC000;
	colours[11].r = 0x0000;
	colours[11].g = 0x9000;
	colours[11].b = 0xF000;
	colours[12].r = 0xA000;
	colours[12].g = 0xA000;
	colours[12].b = 0xA000;

	/* add white for low saturation */
	for (i = 0; i < SWIRLCOLOURS - 1; i++) {
		unsigned short max_rg, max;

		/* what is the max intensity for this colour? */
		max_rg = (colours[i].r > colours[i].g) ? colours[i].r : colours[i].g;
		max = (max_rg > colours[i].b) ? max_rg : colours[i].b;

		/* bring elements up to max as saturation approaches 0.0 */
		colours[i].r += (unsigned short) ((float) (1.0 - saturate) *
					       ((float) max - colours[i].r));
		colours[i].g += (unsigned short) ((float) (1.0 - saturate) *
					       ((float) max - colours[i].g));
		colours[i].b += (unsigned short) ((float) (1.0 - saturate) *
					       ((float) max - colours[i].b));
	}
}

/****************************************************************/

/*-
 * set_black_and_white
 *
 * Set the entries for foreground & background pixels and
 * WhitePixel & BlackPixel in an array of colour specifications.
 *
 * -      swirl is the swirl data
 * -      values is the array of specifications 
 */
static void
set_black_and_white(SWIRL_P swirl, XColor * values)
{
	unsigned long white, black;

	/* where is black and white? */
	white = swirl->white;
	black = swirl->black;

	/* set black and white up */
	values[white].flags = DoRed | DoGreen | DoBlue;
	values[white].pixel = white;
	values[white].red = 0xFFFF;
	values[white].green = 0xFFFF;
	values[white].blue = 0xFFFF;
	values[black].flags = DoRed | DoGreen | DoBlue;
	values[black].pixel = black;
	values[black].red = 0;
	values[black].green = 0;
	values[black].blue = 0;

	/* copy the colour specs from the original entries */
	values[swirl->bg] = swirl->bgcol;
	values[swirl->fg] = swirl->fgcol;
}

/****************************************************************/

/*-
 * set_colour
 *
 * Set an entry in an array of XColor specifications. The given entry will be
 * set to the given colour. If the entry corresponds to the foreground,
 * background, WhitePixel, or BlackPixel it is ignored and the given colour
 * is is put in the next entry.
 *
 * Therefore, the given colour may be placed up to four places after the
 * specified entry in the array, if foreground, background, white, or black
 * intervene.
 *
 * -      swirl is the swirl data
 * -      value points to a pointer to the array entry. It gets updated to
 *            point to the next free entry.
 * -      pixel points to the current pixel number. It gets updated.
 * -      c points to the colour to add
 */
static void
set_colour(SWIRL_P swirl, XColor ** value, unsigned long *pixel, COLOUR_P c)
{
	Bool        done;
	unsigned long black, white, bg, fg;

	/* where are black, white, background, foreground? */
	black = swirl->black;
	white = swirl->white;
	bg = swirl->bg;
	fg = swirl->fg;

	/* haven't set it yet */
	done = False;

	/* try and set the colour */
	while (!done) {
		(**value).flags = DoRed | DoGreen | DoBlue;
		(**value).pixel = *pixel;

		/* white, black, bg, fg, or a colour? */
		if ((*pixel != black) && (*pixel != white) &&
		    (*pixel != bg) && (*pixel != fg)) {
			(**value).red = c->r;
			(**value).green = c->g;
			(**value).blue = c->b;

			/* now we've done it */
			done = True;
		}
		/* next pixel */
		(*value)++;
		(*pixel)++;
	}
}

/****************************************************************/

/*-
 * get_colour
 *
 * Get an entry from an array of XColor specifications. The next colour from
 * the array will be returned. Foreground, background, WhitePixel, or
 * BlackPixel will be ignored.
 *
 * -      swirl is the swirl data
 * -      value points the array entry. It is updated to point to the entry
 *            following the one returned.
 * -      c is set to the colour found
 */
static void
get_colour(SWIRL_P swirl, XColor ** value, COLOUR_P c)
{
	Bool        done;
	unsigned long black, white, bg, fg;

	/* where is white and black? */
	black = swirl->black;
	white = swirl->white;
	bg = swirl->bg;
	fg = swirl->fg;

	/* haven't set it yet */
	done = False;

	/* try and set the colour */
	while (!done) {
		/* black, white or a colour? */
		if (((*value)->pixel != black) && ((*value)->pixel != white) &&
		    ((*value)->pixel != bg) && ((*value)->pixel != fg)) {
			c->r = (*value)->red;
			c->g = (*value)->green;
			c->b = (*value)->blue;

			/* now we've done it */
			done = True;
		}
		/* next value */
		(*value)++;
	}
}

/****************************************************************/

/*-
 *  interpolate
 *
 * Generate n colours between c1 and c2.  n XColors at *value are set up with
 * ascending pixel values.
 *
 * If the pixel range includes BlackPixel or WhitePixel they are set to black
 * and white respectively but otherwise ignored. Therefore, up to n+2 colours
 * may actually be set by this function.
 *
 * -      swirl is the swirl data
 * -      values points a pointer to an array of XColors to update
 * -      pixel points to the pixel number to start at
 * -      k n is the number of colours to generate
 * -      c1, c2 are the colours to interpolate between
 */
static void
interpolate(SWIRL_P swirl, XColor ** values, unsigned long *pixel, int n, COLOUR_P c1, COLOUR_P c2)
{
	int         i, r, g, b;
	COLOUR      c;
	unsigned short maxv;

	/* maximum value */
	maxv = (255 << 8);

	for (i = 0; i < n / 2 && (int) *pixel < swirl->colours; i++) {
		/* work out the colour */
		r = c1->r + 2 * i * ((int) c2->r) / n;
		c.r = (r > (int) maxv) ? maxv : r;
		g = c1->g + 2 * i * ((int) c2->g) / n;
		c.g = (g > (int) maxv) ? maxv : g;
		b = c1->b + 2 * i * ((int) c2->b) / n;
		c.b = (b > (int) maxv) ? maxv : b;

		/* set it up */
		set_colour(swirl, values, pixel, &c);
	}
	for (i = n / 2; i >= 0 && (int) *pixel < swirl->colours; i--) {
		r = c2->r + 2 * i * ((int) c1->r) / n;
		c.r = (r > (int) maxv) ? maxv : r;
		g = c2->g + 2 * i * ((int) c1->g) / n;
		c.g = (g > (int) maxv) ? maxv : g;
		b = c2->b + 2 * i * ((int) c1->b) / n;
		c.b = (b > (int) maxv) ? maxv : b;

		/* set it up */
		set_colour(swirl, values, pixel, &c);
	}
}

/****************************************************************/

/*-
 * basic_map
 *
 * Generate a `random' closed loop colourmap that occupies the whole colour
 * map.
 *
 * -      swirl is the swirl data
 * -      values is the array of colour definitions to set up
 */
static void
basic_map(SWIRL_P swirl, XColor * values)
{
	COLOUR      c[3];
	int         i;
	unsigned short r1, g1, b1, r2, g2, b2, r3, g3, b3;
	int         L1, L2, L3, L;
	unsigned long pixel;
	XColor     *value;

	/* start at the beginning of the colour map */
	pixel = 0;
	value = values;

	/* choose 3 different basic colours at random */
	for (i = 0; i < 3;) {
		int         j;
		Bool        same;

		/* choose colour i */
		c[i] = basic_colours[random_no(SWIRLCOLOURS - 1)];

		/* assume different */
		same = False;

		/* different from the rest? */
		for (j = 0; j < i; j++)
			if ((c[i].r == c[j].r) &&
			    (c[i].g == c[j].g) &&
			    (c[i].b == c[j].b))
				same = True;

		/* ready for the next colour? */
		if (!same)
			i++;
	}

	/* extract components into variables */
	r1 = c[0].r;
	g1 = c[0].g;
	b1 = c[0].b;
	r2 = c[1].r;
	g2 = c[1].g;
	b2 = c[1].b;
	r3 = c[2].r;
	g3 = c[2].g;
	b3 = c[2].b;

	/* work out the lengths of each side of the triangle */
	L1 = (int) sqrt((((double) r1 - r2) * ((double) r1 - r2) +
			 ((double) g1 - g2) * ((double) g1 - g2) +
			 ((double) b1 - b2) * ((double) b1 - b2)));

	L2 = (int) sqrt((((double) r3 - r2) * ((double) r3 - r2) +
			 ((double) g3 - g2) * ((double) g3 - g2) +
			 ((double) b3 - b2) * ((double) b3 - b2)));

	L3 = (int) sqrt((((double) r1 - r3) * ((double) r1 - r3) +
			 ((double) g1 - g3) * ((double) g1 - g3) +
			 ((double) b1 - b3) * ((double) b1 - b3)));

	L = L1 + L2 + L3;

	/* allocate colours in proportion to the lengths of the sides */
	interpolate(swirl, &value, &pixel,
		    (int) ((double) swirl->dcolours * ((double) L1 / (double) L)) + 1, c, c + 1);
	interpolate(swirl, &value, &pixel,
		    (int) ((double) swirl->dcolours * ((double) L2 / (double) L)) + 1, c + 1, c + 2);
	interpolate(swirl, &value, &pixel,
		    (int) ((double) swirl->dcolours * ((double) L3 / (double) L)) + 1, c + 2, c);

	/* fill up any remaining slots (due to rounding) */
	while ((int) pixel < swirl->colours) {
		/* repeat the last colour */
		set_colour(swirl, &value, &pixel, c);
	}

	/* ensure black and white are correct */
	if (!swirl->fixed_colourmap)
		set_black_and_white(swirl, values);
}

/****************************************************************/

/*-
 * pre_rotate
 *
 * Generate pre-rotated versions of the colour specifications
 *
 * -      swirl is the swirl data
 * -      values is an array of colour specifications
 */
static void
pre_rotate(SWIRL_P swirl, XColor * values)
{
	int         i, j;
	XColor     *src, *dest;
	int         dcolours;
	unsigned long pixel;

	/* how many colours to display? */
	dcolours = swirl->dcolours;

	/* start at the first map */
	src = values;
	dest = values + swirl->colours;

	/* generate dcolours-1 rotated maps */
	for (i = 0; i < dcolours - 1; i++) {
		COLOUR      first;

		/* start at the first pixel */
		pixel = 0;

		/* remember the first one and skip it */
		get_colour(swirl, &src, &first);

		/* put a rotated version of src at dest */
		for (j = 0; j < dcolours - 1; j++) {
			COLOUR      c;

			/* get the source colour */
			get_colour(swirl, &src, &c);

			/* set the colour */
			set_colour(swirl, &dest, &pixel, &c);
		}

		/* put the first one at the end */
		set_colour(swirl, &dest, &pixel, &first);

		/* NB: src and dest should now be ready for the next table */

		/* ensure black and white are properly set */
		set_black_and_white(swirl, src);
	}
}

/****************************************************************/

/*-
 * create_colourmap
 *
 * Create a read/write colourmap to use
 *
 * -      swirl is the swirl data
 */

static void
create_colourmap(ModeInfo * mi, SWIRL_P swirl)
{
	Display    *display = MI_DISPLAY(mi);
	int         preserve = 0;
	int         n_rotations;
	int         i;
	Bool        truecolor;
	unsigned long redmask, greenmask, bluemask;

	swirl->fixed_colourmap = !setupColormap(mi, &(swirl->colours),
				&truecolor, &redmask, &greenmask, &bluemask);

	/* how many colours should we animate? */

	if (MI_NPIXELS(mi) < 2)
		return;

	/* how fast to shift the colourmap? */
	swirl->shift = (swirl->colours > 64) ? swirl->colours / 64 : 1;
	swirl->dshift = (swirl->shift > 1) ? swirl->shift * 2 : 1;

	/* create a colour map */
	if (!swirl->fixed_colourmap) {
		swirl->current_map = 0;		/* current colour map, 0..dcolours-1 */

		/* set up fg fb colour specs */
		swirl->black = MI_BLACK_PIXEL(mi);
		swirl->white = MI_WHITE_PIXEL(mi);
		swirl->bg = MI_BG_PIXEL(mi);
		swirl->fg = MI_FG_PIXEL(mi);
		preserve = preserveColors(swirl->black, swirl->white,
					  swirl->bg, swirl->fg);
		swirl->bgcol.pixel = swirl->bg;
		swirl->fgcol.pixel = swirl->fg;
		XQueryColor(display, MI_COLORMAP(mi), &(swirl->bgcol));
		XQueryColor(display, MI_COLORMAP(mi), &(swirl->fgcol));
		if (swirl->cmap) {
			XFreeColormap(display, swirl->cmap);
		}
		swirl->cmap =
			XCreateColormap(display, swirl->win, swirl->visual, AllocAll);
	}
	swirl->dcolours = (swirl->colours > preserve + 1) ?
		swirl->colours - preserve : swirl->colours;
	/* how may colour map rotations are there? */
	n_rotations = (swirl->fixed_colourmap) ? 1 : swirl->dcolours;

	/* allocate space for colour definitions (if not already there) */
	if (swirl->rgb_values != NULL)
		XFree((caddr_t) swirl->rgb_values);
	swirl->rgb_values = (XColor *) calloc((swirl->colours + 3) * n_rotations,
					      sizeof (XColor));

	/* select a set of colours for the colour map */
	basic_map(swirl, swirl->rgb_values);

	/* are we rotating them? */
	if (!swirl->fixed_colourmap) {
		/* generate rotations of the colour maps */
		pre_rotate(swirl, swirl->rgb_values);

		/* store the colours in the colour map */
		XStoreColors(display, swirl->cmap, swirl->rgb_values, swirl->colours);
	} else {
		if (truecolor) {
			int         rsh, gsh, bsh;
			unsigned long int t;

			t = redmask;
			for (i = 0; (int) t > 0; i++, t >>= 1);
			rsh = 16 - i;
			t = greenmask;
			for (i = 0; (int) t > 0; i++, t >>= 1);
			gsh = 16 - i;
			t = bluemask;
			for (i = 0; (int) t > 0; i++, t >>= 1);
			bsh = 16 - i;
			for (i = 0; i < swirl->colours; i++)
				swirl->rgb_values[i].pixel =
					((rsh > 0 ? (swirl->rgb_values[i].red) >> rsh :
					  (swirl->rgb_values[i].red) << (-rsh)) & redmask) |
					((gsh > 0 ? (swirl->rgb_values[i].green) >> gsh :
					  (swirl->rgb_values[i].green) << (-gsh)) & greenmask) |
					((bsh > 0 ? (swirl->rgb_values[i].blue) >> bsh :
					  (swirl->rgb_values[i].blue) << (-bsh)) & bluemask);
		} else {
			/* lookup the colours in the fixed colour map */
			for (i = 0; i < swirl->colours; i++)
				(void) XAllocColor(display, MI_COLORMAP(mi),
						   &(swirl->rgb_values[i]));
		}
	}
}

/****************************************************************/

/*-
 * install_map
 *
 * Install a new set of colours into the colour map
 *
 * -      dpy is the display
 * -      swirl is the swirl data
 * -      shift is the amount to rotate the colour map by
 */
static void
install_map(Display * dpy, SWIRL_P swirl, int shift)
{
	if (!swirl->fixed_colourmap) {
		/* shift the colour map */
		swirl->current_map = (swirl->current_map + shift) %
			swirl->dcolours;

		/* store it */
		XStoreColors(dpy, swirl->cmap,
			     swirl->rgb_values +
			     swirl->current_map * swirl->colours,
			     swirl->colours);
	}
}

#endif /* !STANDALONE */
/****************************************************************/

/*-
 * create_knots
 *
 * Initialise the array of knot
 *
 * swirl is the swirl data
 */
static void
create_knots(SWIRL_P swirl)
{
	int         k;
	Bool        orbit, wheel, picasso, ray, hook;
	KNOT_P      knot;

	/* create array for knots */
	if (swirl->knots)
		(void) free((void *) swirl->knots);
	swirl->knots = (KNOT_P) calloc(swirl->n_knots, sizeof (KNOT));

	/* no knots yet */
	orbit = wheel = picasso = ray = hook = False;

	/* what types do we have? */
	if ((int) swirl->knot_type & (int) ALL) {
		orbit = wheel = ray = hook = True;
	} else {
		if ((int) swirl->knot_type & (int) ORBIT)
			orbit = True;
		if ((int) swirl->knot_type & (int) WHEEL)
			wheel = True;
		if ((int) swirl->knot_type & (int) PICASSO)
			picasso = True;
		if ((int) swirl->knot_type & (int) RAY)
			ray = True;
		if ((int) swirl->knot_type & (int) HOOK)
			hook = True;
	}

	/* initialise each knot */
	knot = swirl->knots;
	for (k = 0; k < swirl->n_knots; k++) {
		/* position */
		knot->x = random_no((unsigned int) swirl->width);
		knot->y = random_no((unsigned int) swirl->height);

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
		if (swirl->two_plane) {
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
do_point(SWIRL_P swirl, int i, int j)
{
	int         tT, k, add, value;
	unsigned long colour_value;
	double      dx, dy, theta, dist;
	int         dcolours, qcolours;
	double      rads;
	KNOT_P      knot;

	/* how many colours? */
	dcolours = swirl->dcolours;
	qcolours = dcolours / 4;

	/* colour step round a circle */
	rads = (double) dcolours / (2.0 * M_PI);

	/* start at zero */
	value = 0;

	/* go through all the knots */
	knot = swirl->knots;
	for (k = 0; k < swirl->n_knots; k++) {
		dx = i - knot->x;
		dy = j - knot->y;

		/* in two_plane mode get the appropriate knot type */
		if (swirl->two_plane)
			tT = (int) ((swirl->first_plane) ? knot->t : knot->T);
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
					add = (int) (dcolours / (1.0 + 0.01 * abs(knot->m) * dist));
					break;
				case WHEEL:
					/* Avoid atan2: DOMAIN error message */
					if (dy == 0.0 && dx == 0.0)
						theta = 1.0;
					else
						theta = (atan2(dy, dx) + M_PI) / M_PI;
					if (theta < 1.0)
						add = (int) (dcolours * theta +
						  sin(0.1 * knot->m * dist) *
						qcolours * exp(-0.01 * dist));
					else
						add = (int) (dcolours * (theta - 1.0) +
						  sin(0.1 * knot->m * dist) *
						qcolours * exp(-0.01 * dist));
					break;
				case PICASSO:
					add = (int) (dcolours *
					  fabs(cos(0.002 * knot->m * dist)));
					break;
				case RAY:
					/* Avoid atan2: DOMAIN error message */
					if (dy == 0.0 && dx == 0.0)
						add = 0;
					else
						add = (int) (dcolours * fabs(sin(2.0 * atan2(dy, dx))));

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
		/* for a +ve mass add on the contribution else take it off */
		if (knot->m > 0)
			value += add;
		else
			value -= add;

		/* next knot */
		knot++;
	}

	/* toggle plane */
	swirl->first_plane = (!swirl->first_plane);

	/* make sure we handle -ve values properly */
	if (value >= 0)
		colour_value = (value % dcolours) + 2;
	else
		colour_value = dcolours - (abs((int) value) % (dcolours - 1));

#ifndef STANDALONE
	/* if bg and fg are 0 and 1 we should be OK, but just in case */
	while ((dcolours > 2) &&
	       (((colour_value % swirl->colours) == swirl->black) ||
		((colour_value % swirl->colours) == swirl->white) ||
		((colour_value % swirl->colours) == swirl->bg) ||
		((colour_value % swirl->colours) == swirl->fg))) {
		colour_value++;
	}
#endif /* !STANDALONE */

	/* definitely make sure it is in range */
	colour_value = colour_value % swirl->colours;

	/* lookup the pixel value if necessary */
#ifndef STANDALONE
	if (swirl->fixed_colourmap && swirl->dcolours > 2)
#endif
		colour_value = swirl->rgb_values[colour_value].pixel;

	/* return it */
	return colour_value;
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
draw_point(ModeInfo * mi, SWIRL_P swirl)
{
	int         r;
	int         x, y;

	/* get current point coordinates and resolution */
	x = swirl->x;
	y = swirl->y;
	r = swirl->r;

	/* check we are within the window */
	if ((x < 0) || (x > swirl->width - r) || (y < 0) || (y > swirl->height - r))
		return;

	/* what style are we drawing? */
	if (swirl->two_plane) {
		int         r2;

		/* halve the block size */
		r2 = r / 2;

		/* interleave blocks at half r */
		draw_block(swirl->ximage, x, y, r2, do_point(swirl, x, y));
		draw_block(swirl->ximage, x + r2, y, r2, do_point(swirl, x + r2, y));
		draw_block(swirl->ximage, x + r2, y + r2, r2, do_point(swirl,
							    x + r2, y + r2));
		draw_block(swirl->ximage, x, y + r2, r2, do_point(swirl, x, y + r2));
	} else
		draw_block(swirl->ximage, x, y, r, do_point(swirl, x, y));

	/* update the screen */
/*-
 * PURIFY 4.0.1 on SunOS4 and on Solaris 2 reports a 256 byte memory leak on
 * the next line. */
	(void) XPutImage(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi), swirl->ximage,
			 x, y, x, y, r, r);
}

/****************************************************************/

/*-
 * next_point  Move to the next point in the spiral pattern
 *  -    swirl is the swirl
 *  -    win is the window to update
 */
static void
next_point(SWIRL_P swirl)
{
	/* more to do in this direction? */
	if (swirl->dir_done < swirl->dir_todo) {
		/* move in the current direction */
		switch (swirl->direction) {
			case DRAW_RIGHT:
				swirl->x += swirl->r;
				break;
			case DRAW_DOWN:
				swirl->y += swirl->r;
				break;
			case DRAW_LEFT:
				swirl->x -= swirl->r;
				break;
			case DRAW_UP:
				swirl->y -= swirl->r;
				break;
		}

		/* done another point */
		swirl->dir_done++;
	} else {
		/* none drawn yet */
		swirl->dir_done = 0;

		/* change direction - check and record if off screen */
		switch (swirl->direction) {
			case DRAW_RIGHT:
				swirl->direction = DRAW_DOWN;
				if (swirl->x > swirl->width - swirl->r) {
					/* skip these points */
					swirl->dir_done = swirl->dir_todo;
					swirl->y += (swirl->dir_todo * swirl->r);

					/* check for finish */
					if (swirl->off_screen)
						swirl->drawing = False;
					swirl->off_screen = True;
				} else
					swirl->off_screen = False;
				break;
			case DRAW_DOWN:
				swirl->direction = DRAW_LEFT;
				swirl->dir_todo++;
				if (swirl->y > swirl->height - swirl->r) {
					/* skip these points */
					swirl->dir_done = swirl->dir_todo;
					swirl->x -= (swirl->dir_todo * swirl->r);

					/* check for finish */
					if (swirl->off_screen)
						swirl->drawing = False;
					swirl->off_screen = True;
				} else
					swirl->off_screen = False;
				break;
			case DRAW_LEFT:
				swirl->direction = DRAW_UP;
				if (swirl->x < 0) {
					/* skip these points */
					swirl->dir_done = swirl->dir_todo;
					swirl->y -= (swirl->dir_todo * swirl->r);

					/* check for finish */
					if (swirl->off_screen)
						swirl->drawing = False;
					swirl->off_screen = True;
				} else
					swirl->off_screen = False;
				break;
			case DRAW_UP:
				swirl->direction = DRAW_RIGHT;
				swirl->dir_todo++;
				if (swirl->y < 0) {
					/* skip these points */
					swirl->dir_done = swirl->dir_todo;
					swirl->x += (swirl->dir_todo * swirl->r);

					/* check for finish */
					if (swirl->off_screen)
						swirl->drawing = False;
					swirl->off_screen = True;
				} else
					swirl->off_screen = False;
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
	SWIRL_P     swirl;

	/* does the swirls array exist? */
	if (swirls == NULL) {
		int         i;

		/* allocate an array, one entry for each screen */
		swirls = (SWIRL_P) calloc(ScreenCount(display), sizeof (SWIRL));

		/* initialise them all */
		for (i = 0; i < ScreenCount(display); i++)
			initialise_swirl(&swirls[i]);
	}
	/* get a pointer to this swirl */
	swirl = &(swirls[MI_SCREEN(mi)]);

	/* get window parameters */
	swirl->win = window;
	swirl->width = MI_WIDTH(mi);
	swirl->height = MI_HEIGHT(mi);
	swirl->depth = MI_DEPTH(mi);
	swirl->rdepth = swirl->depth;
	swirl->visual = MI_VISUAL(mi);

	if (swirl->depth > 16)
		swirl->depth = 16;

	/* initialise image for speeding up drawing */
	initialise_image(display, swirl);

	MI_CLEARWINDOW(mi);

#ifdef STANDALONE

	swirl->rgb_values = mi->colors;
	swirl->colours = mi->npixels;
	swirl->dcolours = swirl->colours;
/* swirl->fixed_colourmap = !mi->writable_p; */

#else /* !STANDALONE */
	/* initialise the colours from which the colourmap is derived */
	initialise_colours(basic_colours, MI_SATURATION(mi));

	/* set up the colour map */
	create_colourmap(mi, swirl);

	/* attach the colour map to the window (if we have one) */
	if (!swirl->fixed_colourmap) {
#if 1
		setColormap(display, window, swirl->cmap, MI_IS_INWINDOW(mi));
#else
		XSetWindowColormap(display, window, swirl->cmap);
		XSetWMColormapWindows(display, window, &window, 1);
		XInstallColormap(display, swirl->cmap);
#endif
	}
#endif /* !STANDALONE */

	/* resolution starts off chunky */
	swirl->resolution = MIN_RES + 1;

	/* calculate the pixel step for this resulution */
	swirl->r = (1 << (swirl->resolution - 1));

	/* how many knots? */
	swirl->n_knots = random_no((unsigned int) MI_COUNT(mi) / 2) +
		MI_COUNT(mi) + 1;

	/* what type of knots? */
	swirl->knot_type = ALL;	/* for now */

	/* use two_plane mode occaisionally */
	if (random_no(100) <= TWO_PLANE_PCNT) {
		swirl->two_plane = swirl->first_plane = True;
		swirl->max_resolution = 2;
	} else
		swirl->two_plane = False;

	/* fix the knot values */
	create_knots(swirl);

	/* we are off */
	swirl->started = True;
	swirl->drawing = False;
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
	SWIRL_P     swirl = &(swirls[MI_SCREEN(mi)]);

	MI_IS_DRAWN(mi) = True;

	/* are we going? */
	if (swirl->started) {
		/* in the middle of drawing? */
		if (swirl->drawing) {
#ifdef STANDALONE
			if (mi->writable_p)
				rotate_colors(MI_DISPLAY(mi), MI_COLORMAP(mi),
				       swirl->rgb_values, swirl->colours, 1);
#else /* !STANDALONE */
			/* rotate the colours */
			install_map(MI_DISPLAY(mi), swirl, swirl->dshift);
#endif /* !STANDALONE */

			/* draw a batch of points */
			swirl->batch_todo = BATCH_DRAW;
			while ((swirl->batch_todo > 0) && swirl->drawing) {
				/* draw a point */
				draw_point(mi, swirl);

				/* move to the next point */
				next_point(swirl);

				/* done a point */
				swirl->batch_todo--;
			}
		} else {
#ifdef STANDALONE
			if (mi->writable_p)
				rotate_colors(MI_DISPLAY(mi), MI_COLORMAP(mi),
				       swirl->rgb_values, swirl->colours, 1);
#else /* !STANDALONE */
			/* rotate the colours */
			install_map(MI_DISPLAY(mi), swirl, swirl->shift);
#endif /* !STANDALONE */

			/* time for a higher resolution? */
			if (swirl->resolution > swirl->max_resolution) {
				/* move to higher resolution */
				swirl->resolution--;

				/* calculate the pixel step for this resulution */
				swirl->r = (1 << (swirl->resolution - 1));

				/* start drawing again */
				swirl->drawing = True;

				/* start in the middle of the screen */
				swirl->x = (swirl->width - swirl->r) / 2;
				swirl->y = (swirl->height - swirl->r) / 2;

				/* initialise spiral drawing parameters */
				swirl->direction = DRAW_RIGHT;
				swirl->dir_todo = 1;
				swirl->dir_done = 0;
			} else {
				/* all done, decide when to restart */
				if (swirl->start_again == -1) {
					/* start the counter */
					swirl->start_again = RESTART;
				} else if (swirl->start_again == 0) {
					/* reset the counter */
					swirl->start_again = -1;

#ifdef STANDALONE
					/* Pick a new colormap! */
					MI_CLEARWINDOW(mi);
					free_colors(MI_DISPLAY(mi), MI_COLORMAP(mi),
						    mi->colors, mi->npixels);
					make_smooth_colormap(MI_DISPLAY(mi),
							     MI_VISUAL(mi),
							     MI_COLORMAP(mi),
					      mi->colors, &mi->npixels, True,
							     &mi->writable_p);
					swirl->colours = mi->npixels;
#endif /* STANDALONE */

					/* start again */
					init_swirl(mi);
				} else
					/* decrement the counter */
					swirl->start_again--;
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
		int         i;

		/* free them all */
		for (i = 0; i < MI_NUM_SCREENS(mi); i++) {
			SWIRL_P     swirl = &(swirls[i]);

			if (swirl->cmap != None) {
				XFreeColormap(MI_DISPLAY(mi), swirl->cmap);
			}
			if (swirl->rgb_values != NULL)
				XFree((caddr_t) swirl->rgb_values);
			if (swirl->ximage != NULL)
				(void) XDestroyImage(swirl->ximage);
			if (swirl->knots)
				(void) free((void *) swirl->knots);
		}
		/* deallocate an array, one entry for each screen */
		(void) free((void *) swirls);
		swirls = NULL;
	}
}

/****************************************************************/

void
refresh_swirl(ModeInfo * mi)
{
	SWIRL_P     swirl = &(swirls[MI_SCREEN(mi)]);

	if (swirl->started) {
		MI_CLEARWINDOW(mi);
		if (swirl->drawing)
			swirl->resolution = swirl->resolution + 1;
		swirl->drawing = False;
	}
}

#endif /* MODE_swirl */
