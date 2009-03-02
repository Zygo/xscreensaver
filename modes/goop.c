/* -*- Mode: C; tab-width: 4 -*- */
/* goop --- goop from a lava lamp */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)goop.c  4.10 98/03/24 xlockmore";

#endif

/*-
 * Copyright (c) 1997 by Jamie Zawinski
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
 * 24-Mar-98: xlock version David Bagley <bagleyd@tux.org>
 * 1997:      xscreensaver version Jamie Zawinski <jwz@jwz.org>
 */

/*-
 * original copyright
 * xscreensaver, Copyright (c) 1997 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

/*-
 * This is pretty compute-intensive, probably due to the large number of
 * polygon fills.  I tried introducing a scaling factor to make the spline
 * code emit fewer line segments, but that made the edges very rough.
 * However, tuning *maxVelocity, *elasticity and *delay can result in much
 * smoother looking animation.  I tuned these for a 1280x1024 Indy display,
 * but I don't know whether these values will be reasonable for a slower
 * machine...
 *
 * The more planes the better -- SGIs have a 12-bit pseudocolor display
 * (4096 colormap cells) which is mostly useless, except for this program,
 * where it means you can have 11 or 12 mutually-transparent objects instead
 * of only 7 or 8.  But, if you are using the 12-bit visual, you should crank
 * down the velocity and elasticity, or server slowness will cause the
 * animation to look jerky (yes, it's sad but true, SGI's X server is
 * perceptibly slower when using plane masks on a 12-bit visual than on an
 * 8-bit visual.)  Using -max-velocity 0.5 -elasticity 0.9 seems to work ok
 * on my Indy R5k with visual 0x27 and the bottom-of-the-line 24-bit graphics
 * board.
 *
 * It might look better if each blob had an outline, which was a *slightly*
 * darker color than the center, to give them a bit more definition -- but
 * that would mean using two planes per blob.  (Or maybe allocating the
 * outline colors outside of the plane-space?  Then the outlines wouldn't be
 * transparent, but maybe that wouldn't be so noticeable?)
 *
 * Oh, for an alpha channel... maybe I should rewrite this in GL.  Then the
 * blobs could have thickness, and curved edges with specular reflections...
 */

#ifdef STANDALONE
#define PROGCLASS "Goop"
#define HACK_INIT init_goop
#define HACK_DRAW draw_goop
#define goop_opts xlockmore_opts
#define DEFAULTS "*delay: 40000 \n" \
 "*count: 100 \n"
/*-  Come back to this.
  "*delay:		12000",
"*transparent:	true",
"*additive:		true",
"*xor:		false",
"*count:		0",
"*planes:		0",
"*thickness:		5",
"*torque:		0.0075",
"*elasticity:		1.8",
"*maxVelocity:	1.2",
 */
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */

#endif /* STANDALONE */
#include <spline.h>

#ifdef MODE_goop

ModeSpecOpt goop_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   goop_description =
{"goop", "init_goop", "draw_goop", "release_goop",
 "init_goop", "init_goop", NULL, &goop_opts,
 10000, -12, 1, 1, 64, 1.0, "",
 "Shows goop from a lava lamp", 0, NULL};

#endif


#define SCALE       10000	/* fixed-point math, for sub-pixel motion */
#define DEF_COUNT   12		/* When planes and count are 0, how many blobs. */


#define RANDSIGN() ((LRAND() & 1) ? 1 : -1)

typedef struct {
	long        x, y;	/* position of midpoint */
	long        dx, dy;	/* velocity and direction */
	double      torque;	/* rotational speed */
	double      th;		/* angle of rotation */
	long        elasticity;	/* how fast they deform */
	long        max_velocity;	/* speed limit */
	long        min_r, max_r;	/* radius range */
	int         npoints;	/* control points */
	long       *r;		/* radii */
	spline     *splines;
} blob;

typedef struct {
	int         nblobs;	/* number of blops per plane */
	blob       *blobs;
	Pixmap      pixmap;
	unsigned long pixel;
	GC          gc;
} layer;

enum goop_mode {
	transparent,
	opaque,
	xor,
	outline
};

typedef struct {
	enum goop_mode mode;
	int         width, height;
	int         nlayers;
	layer      *layers;
	unsigned long background;
	Pixmap      pixmap;
	GC          pixmap_gc;
} goopstruct;

static goopstruct *goops = NULL;

static void
make_blob(blob * b, int maxx, int maxy, int size)
{
	int         i;
	long        mid;

	maxx *= SCALE;
	maxy *= SCALE;
	size *= SCALE;

	b->max_r = size / 2;
	b->min_r = size / 10;

	if (b->min_r < (5 * SCALE))
		b->min_r = (5 * SCALE);
	mid = ((b->min_r + b->max_r) / 2);

	b->torque = 0.0075;	/* torque init */
	b->elasticity = (long) (SCALE * 1.8);	/* elasticity init */
	b->max_velocity = (long) (SCALE * 1.2);		/* max_velocity init */

	b->x = NRAND(maxx);
	b->y = NRAND(maxy);

	b->dx = NRAND(b->max_velocity) * RANDSIGN();
	b->dy = NRAND(b->max_velocity) * RANDSIGN();
	b->th = (2.0 * M_PI) * LRAND() / MAXRAND * RANDSIGN();
	b->npoints = (int) (LRAND() % 5) + 5;

	b->splines = make_spline(b->npoints);
	b->r = (long *) malloc(sizeof (*b->r) * b->npoints);
	for (i = 0; i < b->npoints; i++)
		b->r[i] = ((LRAND() % mid) + (mid / 2)) * RANDSIGN();
}

static void
throb_blob(blob * b)
{
	int         i;
	double      frac = ((M_PI + M_PI) / b->npoints);

	for (i = 0; i < b->npoints; i++) {
		long        r = b->r[i];
		long        ra = (r > 0 ? r : -r);
		double      th = (b->th > 0 ? b->th : -b->th);
		long        x, y;

		/* place control points evenly around perimiter, shifted by theta */
		x = b->x + (long) (ra * cos(i * frac + th));
		y = b->y + (long) (ra * sin(i * frac + th));

		b->splines->control_x[i] = x / SCALE;
		b->splines->control_y[i] = y / SCALE;

		/* alter the radius by a random amount, in the direction in which
		   it had been going (the sign of the radius indicates direction.) */
		ra += (NRAND(b->elasticity) * (r > 0 ? 1 : -1));
		r = ra * (r >= 0 ? 1 : -1);

		/* If we've reached the end (too long or too short) reverse direction. */
		if ((ra > b->max_r && r >= 0) ||
		    (ra < b->min_r && r < 0))
			r = -r;
		/* And reverse direction in mid-course once every 50 times. */
		else if (!(LRAND() % 50))
			r = -r;

		b->r[i] = r;
	}
}

static void
move_blob(blob * b, int maxx, int maxy)
{
	maxx *= SCALE;
	maxy *= SCALE;

	b->x += b->dx;
	b->y += b->dy;

	/* If we've reached the edge of the box, reverse direction. */
	if ((b->x > maxx && b->dx >= 0) ||
	    (b->x < 0 && b->dx < 0)) {
		b->dx = -b->dx;
	}
	if ((b->y > maxy && b->dy >= 0) ||
	    (b->y < 0 && b->dy < 0)) {
		b->dy = -b->dy;
	}
	/* Alter velocity randomly. */
	if (!(LRAND() % 10)) {
		b->dx += (NRAND(b->max_velocity / 2) * RANDSIGN());
		b->dy += (NRAND(b->max_velocity / 2) * RANDSIGN());

		/* Throttle velocity */
		if (b->dx > b->max_velocity || b->dx < -b->max_velocity)
			b->dx /= 2;
		if (b->dy > b->max_velocity || b->dy < -b->max_velocity)
			b->dy /= 2;
	} {
		double      th = b->th;
		double      d = (b->torque == 0 ? 0 : (b->torque) * LRAND() / MAXRAND);

		if (th < 0)
			th = -(th + d);
		else
			th += d;

		if (th > (M_PI + M_PI))
			th -= (M_PI + M_PI);
		else if (th < 0)
			th += (M_PI + M_PI);

		b->th = (b->th > 0 ? th : -th);
	}

	/* Alter direction of rotation randomly. */
	if (!(LRAND() % 100))
		b->th *= -1;
}


static void
draw_blob(Display * display, Drawable drawable, GC gc, blob * b,
	  Bool fill_p)
{
	compute_closed_spline(b->splines);
#ifdef DEBUG
	{
		int         i;

		for (i = 0; i < b->npoints; i++)
			XDrawLine(display, drawable, gc, b->x / SCALE, b->y / SCALE,
			 b->splines->control_x[i], b->splines->control_y[i]);
	}
#else
	if (fill_p)
		XFillPolygon(display, drawable, gc, b->splines->points, b->splines->n_points,
			     Nonconvex, CoordModeOrigin);
	else
#endif
		XDrawLines(display, drawable, gc, b->splines->points, b->splines->n_points,
			   CoordModeOrigin);
}

static void
make_layer(ModeInfo * mi, layer * l, int nblobs)
{
	int         i;
	int         blob_min, blob_max;
	XGCValues   gcv;
	int         width = MI_WIDTH(mi), height = MI_HEIGHT(mi);

	l->nblobs = nblobs;

	l->blobs = (blob *) calloc(l->nblobs, sizeof (blob));

	blob_max = (width < height ? width : height) / 2;
	blob_min = (blob_max * 2) / 3;
	for (i = 0; i < l->nblobs; i++)
		make_blob(&(l->blobs[i]), width, height,
			  (int) (LRAND() % (blob_max - blob_min)) + blob_min);

	l->pixmap = XCreatePixmap(MI_DISPLAY(mi), MI_WINDOW(mi), width, height, 1);
	l->gc = XCreateGC(MI_DISPLAY(mi), l->pixmap, 0, &gcv);
}

static void
draw_layer_plane(Display * display, layer * layer_plane, int width, int height)
{
	int         i;

	XSetForeground(display, layer_plane->gc, 1L);
	XFillRectangle(display, layer_plane->pixmap, layer_plane->gc,
     0, 0, width, height);
	XSetForeground(display, layer_plane->gc, 0L);
	for (i = 0; i < layer_plane->nblobs; i++) {
		throb_blob(&(layer_plane->blobs[i]));
		move_blob(&(layer_plane->blobs[i]), width, height);
		draw_blob(display, layer_plane->pixmap, layer_plane->gc,
       &(layer_plane->blobs[i]), True);
	}
}


static void
draw_layer_blobs(Display * display, Drawable drawable, GC gc,
		 layer * layer_plane, int width, int height,
		 Bool fill_p)
{
	int         i;

	for (i = 0; i < layer_plane->nblobs; i++) {
		throb_blob(&(layer_plane->blobs[i]));
		move_blob(&(layer_plane->blobs[i]), width, height);
		draw_blob(display, drawable, gc, &(layer_plane->blobs[i]), fill_p);
	}
}

static void
free_goop(Display * display, goopstruct * gp)
{
	int         l;

	if (gp->layers) {
		for (l = 0; l < gp->nlayers; l++) {
			if (gp->layers[l].blobs) {
				int         b;

				for (b = 0; b < gp->layers[l].nblobs; b++) {
					if (gp->layers[l].blobs[b].r)
						(void) free((void *) gp->layers[l].blobs[b].r);
					free_spline(gp->layers[l].blobs[b].splines);
				}
				(void) free((void *) gp->layers[l].blobs);
			}
			if (gp->layers[l].gc != None)
				XFreeGC(display, gp->layers[l].gc);
			if (gp->layers[l].pixmap != None)
				XFreePixmap(display, gp->layers[l].pixmap);
		}
		if (gp->pixmap_gc != None) {
			XFreeGC(display, gp->pixmap_gc);
			gp->pixmap_gc = None;
		}
		if (gp->pixmap != None) {
			XFreePixmap(display, gp->pixmap);
			gp->pixmap = None;
		}
		(void) free((void *) gp->layers);
		gp->layers = NULL;
	}
}

void
init_goop(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	goopstruct *gp;
	int         i;
	XGCValues   gcv;
	int         nblobs;

	unsigned long *plane_masks = 0;
	unsigned long base_pixel = 0;

	if (goops == NULL) {
		if ((goops = (goopstruct *) calloc(MI_NUM_SCREENS(mi),
					       sizeof (goopstruct))) == NULL)
			return;
	}
	gp = &goops[MI_SCREEN(mi)];


	gp->mode = (False /* xor init */ ? xor
		    : (True /* transparent init */ ? transparent : opaque));

	gp->width = MI_WIDTH(mi);
	gp->height = MI_HEIGHT(mi);

	free_goop(display, gp);

	gp->nlayers = 0;	/* planes init */
	if (gp->nlayers <= 0)
		gp->nlayers = (int) (LRAND() % (MI_DEPTH(mi) - 2)) + 2;
	gp->layers = (layer *) calloc(gp->nlayers, sizeof (layer));


	if ((MI_NPIXELS(mi) < 2) && gp->mode == transparent)
		gp->mode = opaque;

	/* Try to allocate some color planes before committing to nlayers.
	 */
#if 0
	if (gp->mode == transparent) {
		Bool        additive_p = True;	/* additive init */
		int         nplanes = gp->nlayers;

		/* allocate_alpha_colors (display, MI_COLORMAP(mi), &nplanes, additive_p, &plane_masks,
		   &base_pixel); *//* COME BACK */
		if (nplanes > 1)
			gp->nlayers = nplanes;
		else {
			(void) fprintf(stderr,
				       "could not allocate any color planes; turning transparency off.\n");
			gp->mode = opaque;
		}
	}
#else
	if (gp->mode == transparent)
		gp->mode = opaque;
#endif

	nblobs = MI_COUNT(mi);
	if (nblobs < 0) {
		nblobs = NRAND(-nblobs) + 1;	/* Add 1 so its not too boring */
	} {
		int         lblobs[32];
		int         total = DEF_COUNT;

		/* What does this do? DAB */
		if (nblobs <= 0)
			while (total)
				for (i = 0; total && i < gp->nlayers; i++)
					lblobs[i]++, total--;
		for (i = 0; i < gp->nlayers; i++)
			make_layer(mi, &(gp->layers[i]),
				   (nblobs > 0 ? nblobs : lblobs[i]));
	}

	if (gp->mode == transparent && plane_masks) {
		for (i = 0; i < gp->nlayers; i++)
			gp->layers[i].pixel = base_pixel | plane_masks[i];
		gp->background = base_pixel;
	}
	if (plane_masks)
		(void) free((void *) plane_masks);

	if (gp->mode != transparent) {
		gp->background = 0;	/* init */

		for (i = 0; i < gp->nlayers; i++) {
			if (MI_NPIXELS(mi) > 2)
				gp->layers[i].pixel = MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));
			else
				gp->layers[i].pixel = MI_WHITE_PIXEL(mi);
		}
	}
	gp->pixmap = XCreatePixmap(display, window, MI_WIDTH(mi), MI_HEIGHT(mi),
				   (gp->mode == xor ? 1 : MI_DEPTH(mi)));

	gcv.background = gp->background;
	gcv.foreground = 255;	/* init */
	gcv.line_width = 5;	/* thickness init */
	gp->pixmap_gc = XCreateGC(display, gp->pixmap, GCLineWidth, &gcv);
	MI_CLEARWINDOW(mi);
}

void
draw_goop(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	goopstruct *gp = &goops[MI_SCREEN(mi)];
	int         i;

	MI_IS_DRAWN(mi) = True;

	switch (gp->mode) {
		case transparent:

			for (i = 0; i < gp->nlayers; i++)
				draw_layer_plane(display, &(gp->layers[i]), gp->width, gp->height);

			XSetForeground(display, gp->pixmap_gc, gp->background);
			XSetPlaneMask(display, gp->pixmap_gc, AllPlanes);
			XFillRectangle(display, gp->pixmap, gp->pixmap_gc, 0, 0,
				       gp->width, gp->height);
			XSetForeground(display, gp->pixmap_gc, ~0L);
			for (i = 0; i < gp->nlayers; i++) {
				XSetPlaneMask(display, gp->pixmap_gc, gp->layers[i].pixel);
/*
   XSetForeground (display, gp->pixmap_gc, ~0L);
   XFillRectangle (display, gp->pixmap, gp->pixmap_gc, 0, 0,
   gp->width, gp->height);
   XSetForeground (display, gp->pixmap_gc, 0L);
 */
				draw_layer_blobs(display, gp->pixmap, gp->pixmap_gc,
				     &(gp->layers[i]), gp->width, gp->height,
						 True);
			}
			XCopyArea(display, gp->pixmap, window, MI_GC(mi), 0, 0,
				  gp->width, gp->height, 0, 0);
			break;

		case xor:
			XSetFunction(display, gp->pixmap_gc, GXcopy);
			XSetForeground(display, gp->pixmap_gc, 0);
			XFillRectangle(display, gp->pixmap, gp->pixmap_gc, 0, 0,
				       gp->width, gp->height);
			XSetFunction(display, gp->pixmap_gc, GXxor);
			XSetForeground(display, gp->pixmap_gc, 1);
			for (i = 0; i < gp->nlayers; i++)
				draw_layer_blobs(display, gp->pixmap, gp->pixmap_gc,
				     &(gp->layers[i]), gp->width, gp->height,
						 (gp->mode != outline));
			XCopyPlane(display, gp->pixmap, window, MI_GC(mi), 0, 0,
				   gp->width, gp->height, 0, 0, 1L);
			break;

		case opaque:
		case outline:
			XSetForeground(display, gp->pixmap_gc, MI_BLACK_PIXEL(mi));
			XFillRectangle(display, gp->pixmap, gp->pixmap_gc, 0, 0,
				       gp->width, gp->height);
			for (i = 0; i < gp->nlayers; i++) {
				XSetForeground(display, gp->pixmap_gc, gp->layers[i].pixel);
				draw_layer_blobs(display, gp->pixmap, gp->pixmap_gc,
				     &(gp->layers[i]), gp->width, gp->height,
						 (gp->mode != outline));
			}
			XCopyArea(display, gp->pixmap, window, MI_GC(mi), 0, 0,
				  gp->width, gp->height, 0, 0);
			break;

		default:
			abort();
			break;
	}
}

void
release_goop(ModeInfo * mi)
{
	if (goops != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			goopstruct *gp = &goops[screen];

			free_goop(MI_DISPLAY(mi), gp);
		}
		(void) free((void *) goops);
		goops = NULL;
	}
}

#endif /* MODE_goop */
