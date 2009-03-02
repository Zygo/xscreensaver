/* -*- Mode: C; tab-width: 4 -*- */
/* starfish ---  */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)starfish.c	4.11 98/06/11 xlockmore";

#endif

/* xscreensaver, Copyright (c) 1997 Jamie Zawinski <jwz@jwz.org>

 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * Revision History:
 * 24-Feb-98:  xlockmore version by Jouk Jansen <joukj@hrem.stm.tudelft.nl>
 * 1997 :    original xscreensaver version by Jamie Zawinski <jwz@jwz.org>
 */

#ifdef STANDALONE
#define PROGCLASS "Starfish"
#define HACK_INIT init_starfish
#define HACK_DRAW draw_starfish
#define starfish_opts xlockmore_opts
#define DEFAULTS "*delay: 2000 \n" \
 "*cycles: 1000 \n" \
 "*ncolors: 200 \n" \
 "*fullrandom: True \n" \
 "*verbose: False \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#include "color.h"
#endif /* STANDALONE */

#ifdef MODE_starfish

#include "spline.h"

#define DEF_ROTV  "-1"
#define DEF_THICKNESS  "-20"
#define DEF_CYCLE "True"
#define DEF_BLOB "False"
#define DEF_CYCLESPEED "3"

static int  cyclespeed;
static float rotv, thickness;

static Bool blob, cycle_p;

static XrmOptionDescRec opts[] =
{
     {"-cyclespeed", "starfish.cyclespeed", XrmoptionSepArg, (caddr_t) NULL},
	{"-rotation", "starfish.rotation", XrmoptionSepArg, (caddr_t) NULL},
	{"-thickness", "starfish.thickness", XrmoptionSepArg, (caddr_t) NULL},
	{"-blob", ".starfish.blob", XrmoptionNoArg, (caddr_t) "on"},
	{"+blob", ".starfish.blob", XrmoptionNoArg, (caddr_t) "off"},
	{"-cycle", ".starfish.cycle", XrmoptionNoArg, (caddr_t) "on"},
	{"+cycle", ".starfish.cycle", XrmoptionNoArg, (caddr_t) "off"}
};

static argtype vars[] =
{
{(caddr_t *) & cyclespeed, "cyclespeed", "CycleSpeed", DEF_CYCLESPEED, t_Int},
	{(caddr_t *) & rotv, "rotation", "Rotation", DEF_ROTV, t_Float},
 {(caddr_t *) & thickness, "thickness", "Thickness", DEF_THICKNESS, t_Float},
	{(caddr_t *) & blob, "blob", "Blob", DEF_BLOB, t_Bool},
	{(caddr_t *) & cycle_p, "cycle", "Cycle", DEF_CYCLE, t_Bool}
};

static OptionStruct desc[] =
{
	{"-cyclespeed num", "Cycling speed"},
	{"-rotation num", "Rotation velocity"},
	{"-thickness num", "Thickness"},
	{"-/+blob", "turn on/off blob"},
	{"-/+cycle", "turn on/off cycle"}
};

ModeSpecOpt starfish_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   starfish_description =
{"starfish", "init_starfish", "draw_starfish", "release_starfish",
 "refresh_starfish", "init_starfish", NULL, &starfish_opts,
 2000, 1, 1000, 1, 64, 1.0, "",
 "Shows starfish", 0, NULL};

#endif

#define SCALE        1000	/* fixed-point math, for sub-pixel motion */

#define RAND(n) ((long) ((LRAND() & 0x7fffffff) % ((long) (n))))
#define RANDSIGN() ((LRAND() & 1) ? 1 : -1)

enum starfish_mode {
	pulse,
	zoom
};


typedef struct {
	enum starfish_mode mode;
	Bool        blob_p;
	int         skip;
	long        x, y;	/* position of midpoint */
	double      th;		/* angle of rotation */
	double      rotv;	/* rotational velocity */
	double      rota;	/* rotational acceleration */
	double      elasticity;	/* how fast it deforms: radial velocity */
	double      rot_max;
	long        min_r, max_r;	/* radius range */
	int         npoints;	/* control points */
	long       *r;		/* radii */
	spline     *splines;
	XPoint     *prev;
	int         n_prev;
	GC          gc;
	Colormap    cmap;
	XColor     *colors;
	int         ncolors;
	Bool        cycle_p, mono_p, no_colors;
	unsigned long fg_index;
	int         size, winwidth, winheight, stage, counter;
	float       thickness;
	unsigned long blackpixel, whitepixel, fg, bg;
	int         direction;
} starfishstruct;

static starfishstruct *starfishes = NULL;

static void
make_starfish(ModeInfo * mi)
{
	starfishstruct *sp = &starfishes[MI_SCREEN(mi)];
	int         i;

	sp->elasticity = SCALE * sp->thickness;

	if (sp->elasticity == 0.0)
		/* bell curve from 0-15, avg 7.5 */
		sp->elasticity = RAND(5 * SCALE) + RAND(5 * SCALE) + RAND(5 * SCALE);

	if (sp->rotv == -1)
		/* bell curve from 0-12 degrees, avg 6 */
		sp->rotv = (double) 4.0 *((double) LRAND() / (double) MAXRAND +
			                (double) LRAND() / (double) MAXRAND +
			                (double) LRAND() / (double) MAXRAND);

	sp->rotv /= 360;	/* convert degrees to ratio */

	if (sp->blob_p) {
		sp->elasticity *= 3.0;
		sp->rotv *= 3;
	}
	sp->rot_max = sp->rotv * 2;
	sp->rota = 0.0004 + 0.0002 * (double) LRAND() / (double) MAXRAND;


	if (NRAND(20) == 5)
		sp->size = (int) (sp->size *
			(0.35 * ((double) LRAND() / (double) MAXRAND +
				 (double) LRAND() / (double) MAXRAND) + 0.3));

	{
		static char skips[] =
		{2, 2, 2, 2, 3, 3, 3, 6, 6, 12};

		sp->skip = skips[LRAND() % sizeof (skips)];
	}

	if (!(LRAND() % (sp->skip == 2 ? 3 : 12)))
		sp->mode = zoom;
	else
		sp->mode = pulse;

	sp->winwidth *= SCALE;
	sp->winheight *= SCALE;
	sp->size *= SCALE;

	sp->max_r = sp->size;
	sp->min_r = 0;

	if (sp->min_r < (5 * SCALE))
		sp->min_r = (5 * SCALE);
	/* mid = ((sp->min_r + sp->max_r) / 2); */

	sp->x = sp->winwidth / 2;
	sp->y = sp->winheight / 2;

	sp->th = 2.0 * M_PI * ((double) LRAND() / (double) MAXRAND) * RANDSIGN();

	{
		static char sizes[] =
		{3, 3, 3, 3, 3,
		 4, 4, 4, 4, 5, 5, 5, 5, 5, 5,
		 8, 8, 8, 10, 35};
		int         nsizes = sizeof (sizes);

		if (sp->skip > 3)
			nsizes -= 4;
		sp->npoints = sp->skip * sizes[LRAND() % nsizes];
	}

	if (sp->splines)
		free_spline(sp->splines);
	sp->splines = make_spline(sp->npoints);

	if (sp->r)
		(void) free((void *) sp->r);
	sp->r = (long *) malloc(sizeof (*sp->r) * sp->npoints);

	for (i = 0; i < sp->npoints; i++)
		sp->r[i] = ((i % sp->skip) == 0) ? 0 : sp->size;
}


void
release_starfish(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);

	if (starfishes != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			starfishstruct *sp = &starfishes[screen];

			if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
				MI_WHITE_PIXEL(mi) = sp->whitepixel;
				MI_BLACK_PIXEL(mi) = sp->blackpixel;
#ifndef STANDALONE
				MI_FG_PIXEL(mi) = sp->fg;
				MI_BG_PIXEL(mi) = sp->bg;
#endif
				if (sp->colors && sp->ncolors && !sp->no_colors)
					free_colors(display, sp->cmap, sp->colors, sp->ncolors);
				if (sp->colors)
					(void) free((void *) sp->colors);
				if (sp->cmap)
				  XFreeColormap(display, sp->cmap);
			}
			if (sp->gc != NULL)
				XFreeGC(display, sp->gc);
			if (sp->splines)
				free_spline(sp->splines);
			if (sp->r)
				(void) free((void *) sp->r);
			if (sp->prev)
				(void) free((void *) sp->prev);
		}
		(void) free((void *) starfishes);
		starfishes = NULL;
	}
}


static void
throb_starfish(starfishstruct * sp)
{
	int         i;
	double      frac = ((M_PI + M_PI) / sp->npoints);

	for (i = 0; i < sp->npoints; i++) {
		double      r = sp->r[i];
		double      ra = (r > 0.0 ? r : -r);
		double      th = (sp->th > 0 ? sp->th : -sp->th);
		double      x, y;
		double      elasticity = sp->elasticity;

		/* place control points evenly around perimiter, shifted by theta */
		x = sp->x + (ra * cos(i * frac + th));
		y = sp->y + (ra * sin(i * frac + th));

		sp->splines->control_x[i] = x / SCALE;
		sp->splines->control_y[i] = y / SCALE;

		if (sp->mode == zoom && ((i % sp->skip) == 0))
			continue;

		/* Slow down near the end points: move fastest in the middle. */
		{
			double      ratio = ra / (double) (sp->max_r - sp->min_r);

			if (ratio > 0.5)
				ratio = 1 - ratio;	/* flip */
			ratio *= 2.0;	/* normalize */
			ratio = (ratio * 0.9) + 0.1;	/* fudge */
			elasticity = elasticity * ratio;
		}


		/* Increase/decrease radius by elasticity */
		ra += (r >= 0.0 ? elasticity : -elasticity);
		if ((i % sp->skip) == 0)
			ra += (elasticity / 2.0);

		r = ra * (r >= 0.0 ? 1.0 : -1.0);

		/* If we've reached the end (too long or too short) reverse direction. */
		if ((ra > sp->max_r && r >= 0.0) ||
		    (ra < sp->min_r && r < 0.0))
			r = -r;

		sp->r[i] = (long) r;
	}
}


static void
spin_starfish(starfishstruct * sp)
{
	double      th = sp->th;

	if (th < 0)
		th = -(th + sp->rotv);
	else
		th += sp->rotv;

	if (th > (M_PI + M_PI))
		th -= (M_PI + M_PI);
	else if (th < 0)
		th += (M_PI + M_PI);

	sp->th = (sp->th > 0 ? th : -th);

	sp->rotv += sp->rota;

	if (sp->rotv > sp->rot_max ||
	    sp->rotv < -sp->rot_max) {
		sp->rota = -sp->rota;
	}
	/* If it stops, start it going in the other direction. */
	else if (sp->rotv < 0) {
		if (LRAND() & 1) {
			/* keep going in the same direction */
			sp->rotv = 0;
			if (sp->rota < 0)
				sp->rota = -sp->rota;
		} else {
			/* reverse gears */
			sp->rotv = -sp->rotv;
			sp->rota = -sp->rota;
			sp->th = -sp->th;
		}
	}
	/* Alter direction of rotational acceleration randomly. */
	if (!(LRAND() % 120))
		sp->rota = -sp->rota;

	/* Change acceleration very occasionally. */
	if (!(LRAND() % 200)) {
		if (LRAND() & 1)
			sp->rota *= 1.2;
		else
			sp->rota *= 0.8;
	}
}


static void
draw1_starfish(Display * display, Drawable drawable, GC gc, starfishstruct * sp)
{
	compute_closed_spline(sp->splines);
	if (sp->prev) {
		XPoint     *points = (XPoint *)
		malloc(sizeof (XPoint) * (sp->n_prev + sp->splines->n_points + 2));
		int         i = sp->splines->n_points;
		int         j = sp->n_prev;

		(void) memcpy((char *) points, (char *) sp->splines->points,
			      (i * sizeof (*points)));
		(void) memcpy((char *) (points + i), (char *) sp->prev,
			      (j * sizeof (*points)));

		if (sp->blob_p)
			XClearWindow(display, drawable);
		XFillPolygon(display, drawable, gc, points, i + j,
			     Complex, CoordModeOrigin);
		(void) free((void *) points);

		(void) free((void *) sp->prev);
		sp->prev = NULL;
	}
	sp->prev = (XPoint *) malloc(sp->splines->n_points * sizeof (XPoint));
	(void) memcpy((char *) sp->prev, (char *) sp->splines->points,
		      sp->splines->n_points * sizeof (XPoint));
	sp->n_prev = sp->splines->n_points;

#ifdef DEBUG
	if (sp->blob_p) {
		int         i;

		for (i = 0; i < sp->npoints; i++)
			XDrawLine(display, drawable, gc, sp->x / SCALE, sp->y / SCALE,
			sp->splines->control_x[i], sp->splines->control_y[i]);
	}
#endif
}


void
init_starfish(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	XGCValues   gcv;
	starfishstruct *sp;

/* initialize */
	if (starfishes == NULL) {

		if ((starfishes = (starfishstruct *) calloc(MI_NUM_SCREENS(mi),
					   sizeof (starfishstruct))) == NULL)
			return;
	}
	sp = &starfishes[MI_SCREEN(mi)];

	if (MI_IS_FULLRANDOM(mi)) {
		if (NRAND(10) == 9)
			sp->blob_p = True;
		else
			sp->blob_p = False;
		if (NRAND(8) == 7)
			sp->cycle_p = False;
		else
			sp->cycle_p = True;
	} else {
		sp->blob_p = blob;
		sp->cycle_p = cycle_p;
	}
	sp->rotv = (double) rotv;
	sp->direction = (LRAND() & 1) ? 1 : -1;
	if (thickness < 0)
		sp->thickness = -thickness * (float) LRAND() / (float) MAXRAND;
	else
		sp->thickness = thickness;
	if (sp->blob_p)
		sp->fg_index = MI_BLACK_PIXEL(mi);
	else
		sp->fg_index = MI_WHITE_PIXEL(mi);

	if (!sp->gc) {
		if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
			XColor      color;

#ifndef STANDALONE
			extern char *background;
			extern char *foreground;

			sp->fg = MI_FG_PIXEL(mi);
			sp->bg = MI_BG_PIXEL(mi);
#endif
			sp->blackpixel = MI_BLACK_PIXEL(mi);
			sp->whitepixel = MI_WHITE_PIXEL(mi);
			sp->cmap =	/*MI_COLORMAP(mi); */
				XCreateColormap(display, window, MI_VISUAL(mi), AllocNone);
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
			sp->colors = 0;
			sp->ncolors = 0;
		}
		gcv.foreground = sp->fg_index;
		gcv.fill_rule = EvenOddRule;
		sp->gc = XCreateGC(display, window, GCForeground | GCFillRule, &gcv);
	}
	if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
/* Set up colour map */
		if (sp->colors && sp->ncolors && !sp->no_colors)
			free_colors(display, sp->cmap, sp->colors, sp->ncolors);
		if (sp->colors)
			(void) free((void *) sp->colors);
		sp->colors = 0;
		sp->ncolors = MI_NCOLORS(mi);
		if (sp->ncolors < 2)
			sp->ncolors = 2;
		if (sp->ncolors <= 2)
			sp->mono_p = True;
		else
			sp->mono_p = False;

		if (sp->mono_p)
			sp->colors = 0;
		else
			sp->colors = (XColor *) malloc(sizeof (*sp->colors) * (sp->ncolors + 1));

		if (sp->mono_p || sp->blob_p)
			sp->cycle_p = False;
		if (!sp->mono_p) {
			if (LRAND() % 3)
				make_smooth_colormap(mi, sp->cmap, sp->colors, &sp->ncolors,
						     True, &sp->cycle_p);
			else
				make_uniform_colormap(mi, sp->cmap, sp->colors, &sp->ncolors,
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

		sp->fg_index = 0;

/*  if (!sp->mono_p && !sp->blob_p)
   {
   gcv.foreground = sp->colors[sp->fg_index].pixel;
   XSetWindowBackground (display, window, gcv.foreground);
   } */

	} else
		sp->fg_index = NRAND(MI_NPIXELS(mi));

	sp->size = (MI_WIDTH(mi) < MI_HEIGHT(mi) ? MI_WIDTH(mi) : MI_HEIGHT(mi));
	if (sp->blob_p)
		sp->size /= 2;
	else
		sp->size = (int) (sp->size * 1.3);
	sp->winwidth = MI_WIDTH(mi);
	sp->winheight = MI_HEIGHT(mi);
	MI_CLEARWINDOW(mi);
	if (sp->prev) {
		(void) free((void *) sp->prev);
		sp->prev = NULL;
	}
	sp->stage = 0;
	sp->counter = 0;
	make_starfish(mi);
}

static void
run_starfish(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	starfishstruct *sp = &starfishes[MI_SCREEN(mi)];

	throb_starfish(sp);
	spin_starfish(sp);
	draw1_starfish(display, window, sp->gc, sp);

	if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
		if (sp->mono_p) {
			if (!sp->blob_p) {
				sp->fg_index = (sp->fg_index == MI_BLACK_PIXEL(mi) ?
				    MI_BLACK_PIXEL(mi) : MI_BLACK_PIXEL(mi));
			}
			XSetForeground(display, sp->gc, sp->fg_index);
		} else {
			sp->fg_index = (sp->fg_index + 1) % sp->ncolors;
			XSetForeground(display, sp->gc, sp->colors[sp->fg_index].pixel);
		}
	} else {
		if (MI_NPIXELS(mi) > 2)
			XSetForeground(display, sp->gc, MI_PIXEL(mi, sp->fg_index));
		else if (sp->fg_index)
			XSetForeground(display, sp->gc, MI_BLACK_PIXEL(mi));
		else
			XSetForeground(display, sp->gc, MI_WHITE_PIXEL(mi));
		if (++sp->fg_index >= (unsigned int) MI_NPIXELS(mi))
			sp->fg_index = 0;
	}
}

void
draw_starfish(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	starfishstruct *sp = &starfishes[MI_SCREEN(mi)];

	if (sp->no_colors) {
		init_starfish(mi);
		return;
	}
	MI_IS_DRAWN(mi) = True;

	if (!sp->stage)
		run_starfish(mi);
	else if (sp->cycle_p) {
		rotate_colors(display, sp->cmap, sp->colors, sp->ncolors, sp->direction);

		if (!NRAND(512))
			sp->direction = -sp->direction;
	}
	sp->stage++;

	if (sp->stage > (2 * sp->blob_p + 1) * cyclespeed) {
		sp->stage = 0;
		sp->counter++;
	}
	if (sp->counter > MI_CYCLES(mi)) {
		/* Every now and then, restart the mode */
		init_starfish(mi);
	}
}
void
refresh_starfish(ModeInfo * mi)
{
	init_starfish(mi);
}

#endif /* MODE_starfish */
