/* -*- Mode: C; tab-width: 4 -*- */
/* drift --- drifting recursive fractal cosmic flames */

#if 0
static const char sccsid[] = "@(#)drift.c	5.00 2000/11/01 xlockmore";
#endif

/*-
 * Copyright (c) 1991 by Patrick J. Naughton.
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
 * 01-Nov-2000: Allocation checks
 * 10-May-1997: Jamie Zawinski <jwz@jwz.org> compatible with xscreensaver
 * 01-Jan-1997: Moved new flame to drift.  Compile time options now run time.
 * 01-Jun-1995: Updated by Scott Draves.
 * 27-Jun-1991: vary number of functions used.
 * 24-Jun-1991: fixed portability problem with integer mod (%).
 * 06-Jun-1991: Written, received from Scott Draves <spot@cs.cmu.edu>
 */

#ifdef STANDALONE
# define MODE_drift
# define DEFAULTS "*delay: 10000 \n" \
				  "*count: 30 \n" \
				  "*ncolors: 200 \n" \
				  "*fpsSolid: true \n" \
				  "*ignoreRotation: True \n" \

# define SMOOTH_COLORS
# define release_drift 0
# define reshape_drift 0
# define drift_handle_event 0
# include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
# define ENTRYPOINT /**/
# include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_drift

#define DEF_GROW "False"	/* Grow fractals instead of animating one at a time,
				   would then be like flame */
#define DEF_LISS "False"	/* if this is defined then instead of a point
				   bouncing around in a high dimensional sphere, we
				   use lissojous figures.  Only makes sense if
				   grow is false. */

static Bool grow;
static Bool liss;

static XrmOptionDescRec opts[] =
{
	{"-grow", ".drift.grow", XrmoptionNoArg, "on"},
	{"+grow", ".drift.grow", XrmoptionNoArg, "off"},
	{"-liss", ".drift.trail", XrmoptionNoArg, "on"},
	{"+liss", ".drift.trail", XrmoptionNoArg, "off"}
};
static argtype vars[] =
{
	{&grow, "grow", "Grow", DEF_GROW, t_Bool},
	{&liss, "trail", "Trail", DEF_LISS, t_Bool}
};
static OptionStruct desc[] =
{
	{"-/+grow", "turn on/off growing fractals, else they are animated"},
	{"-/+liss", "turn on/off using lissojous figures to get points"}
};

ENTRYPOINT ModeSpecOpt drift_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   drift_description =
{"drift", "init_drift", "draw_drift", (char *) NULL,
 "refresh_drift", "init_drift", "free_drift", &drift_opts,
 10000, 30, 1, 1, 64, 1.0, "",
 "Shows cosmic drifting flame fractals", 0, NULL};

#endif

#define MAXBATCH1	200	/* mono */
#define MAXBATCH2	20	/* color */
#define FUSE		10	/* discard this many initial iterations */
#define NMAJORVARS	7
#define MAXLEV 10

typedef struct {
	/* shape of current flame */
	int         nxforms;
	double      f[2][3][MAXLEV];	/* a bunch of non-homogeneous xforms */
	int         variation[10];	/* for each xform */

	/* Animation */
	double      df[2][3][MAXLEV];

	/* high-level control */
	int         mode;	/* 0->slow/single 1->fast/many */
	int         nfractals;	/* draw this many fractals */
	int         major_variation;
	int         fractal_len;	/* pts/fractal */
	int         color;
	int         rainbow;	/* more than one color per fractal
				   1-> computed by adding dimension to fractal */

	int         width, height;	/* of window */
	int         timer;

	/* draw info about current flame */
	int         fuse;	/* iterate this many before drawing */
	int         total_points;	/* draw this many pts before fractal ends */
	int         npoints;	/* how many we've computed but not drawn */
	XPoint      pts[MAXBATCH1];	/* here they are */
	unsigned long pixcol;
	/* when drawing in color, we have a buffer per color */
	int        *ncpoints;
	XPoint     *cpts;

	double      x, y, c;
	int         liss_time;
	Bool        grow, liss;

	short       lasthalf;
	long        saved_random_bits;
	int         nbits;

  int erase_countdown;
} driftstruct;

static driftstruct *drifts = (driftstruct *) NULL;

static short
halfrandom(driftstruct * dp, int mv)
{
	unsigned long r;

	if (dp->lasthalf) {
		r = dp->lasthalf;
		dp->lasthalf = 0;
	} else {
		r = LRAND();
		dp->lasthalf = (short) (r >> 16);
	}
	r = r % mv;
	return r;
}

static int
frandom(driftstruct * dp, int n)
{
	int         result;

	if (3 > dp->nbits) {
		dp->saved_random_bits = LRAND();
		dp->nbits = 31;
	}
	switch (n) {
		case 2:
			result = (int) (dp->saved_random_bits & 1);
			dp->saved_random_bits >>= 1;
			dp->nbits -= 1;
			return result;

		case 3:
			result = (int) (dp->saved_random_bits & 3);
			dp->saved_random_bits >>= 2;
			dp->nbits -= 2;
			if (3 == result)
				return frandom(dp, 3);
			return result;

		case 4:
			result = (int) (dp->saved_random_bits & 3);
			dp->saved_random_bits >>= 2;
			dp->nbits -= 2;
			return result;

		case 5:
			result = (int) (dp->saved_random_bits & 7);
			dp->saved_random_bits >>= 3;
			dp->nbits -= 3;
			if (4 < result)
				return frandom(dp, 5);
			return result;
		default:
			(void) fprintf(stderr, "bad arg to frandom\n");
	}
	return 0;
}

#define DISTRIB_A (halfrandom(dp, 7000) + 9000)
#define DISTRIB_B ((frandom(dp, 3) + 1) * (frandom(dp, 3) + 1) * 120000)
#define LEN(x) (sizeof(x)/sizeof((x)[0]))

static void
initmode(ModeInfo * mi, int mode)
{
	driftstruct *dp = &drifts[MI_SCREEN(mi)];

#define VARIATION_LEN 14

	dp->mode = mode;

	dp->major_variation = halfrandom(dp, VARIATION_LEN);
	/*  0, 0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6, 6 */
	dp->major_variation = ((dp->major_variation >= VARIATION_LEN >> 1) &&
			       (dp->major_variation < VARIATION_LEN - 1)) ?
		(dp->major_variation + 1) >> 1 : dp->major_variation >> 1;

	if (dp->grow) {
		dp->rainbow = 0;
		if (mode) {
			if (!dp->color || halfrandom(dp, 8)) {
				dp->nfractals = halfrandom(dp, 30) + 5;
				dp->fractal_len = DISTRIB_A;
			} else {
				dp->nfractals = halfrandom(dp, 5) + 5;
				dp->fractal_len = DISTRIB_B;
			}
		} else {
			dp->rainbow = dp->color;
			dp->nfractals = 1;
			dp->fractal_len = DISTRIB_B;
		}
	} else {
		dp->nfractals = 1;
		dp->rainbow = dp->color;
		dp->fractal_len = 2000000;
	}
	dp->fractal_len = (dp->fractal_len * MI_COUNT(mi)) / 20;

	MI_CLEARWINDOW(mi);
}

static void
pick_df_coefs(ModeInfo * mi)
{
	driftstruct *dp = &drifts[MI_SCREEN(mi)];
	int         i, j, k;
	double      r;

	for (i = 0; i < dp->nxforms; i++) {

		r = 1e-6;
		for (j = 0; j < 2; j++)
			for (k = 0; k < 3; k++) {
				dp->df[j][k][i] = ((double) halfrandom(dp, 1000) / 500.0 - 1.0);
				r += dp->df[j][k][i] * dp->df[j][k][i];
			}
		r = (3 + halfrandom(dp, 5)) * 0.01 / sqrt(r);
		for (j = 0; j < 2; j++)
			for (k = 0; k < 3; k++)
				dp->df[j][k][i] *= r;
	}
}

ENTRYPOINT void
free_drift(ModeInfo * mi)
{
	driftstruct *dp = &drifts[MI_SCREEN(mi)];
	if (dp->ncpoints != NULL) {
		(void) free((void *) dp->ncpoints);
		dp->ncpoints = (int *) NULL;
	}
	if (dp->cpts != NULL) {
		(void) free((void *) dp->cpts);
		dp->cpts = (XPoint *) NULL;
	}
}

static void
initfractal(ModeInfo * mi)
{
	driftstruct *dp = &drifts[MI_SCREEN(mi)];
	int         i, j, k;

#define XFORM_LEN 9

	dp->fuse = FUSE;
	dp->total_points = 0;

	if (!dp->ncpoints) {
		if ((dp->ncpoints = (int *) malloc(sizeof (int) * MI_NCOLORS(mi))) ==
			NULL) {
			free_drift(mi);
			return;
		}
	}
	if (!dp->cpts) {
		if ((dp->cpts = (XPoint *) malloc(MAXBATCH2 * sizeof (XPoint) *
			 MI_NCOLORS(mi))) == NULL) {
			free_drift(mi);
			return;
		}
	}

	if (dp->rainbow)
		for (i = 0; i < MI_NPIXELS(mi); i++)
			dp->ncpoints[i] = 0;
	else
		dp->npoints = 0;
	dp->nxforms = halfrandom(dp, XFORM_LEN);
	/* 2, 2, 2, 3, 3, 3, 4, 4, 5 */
	dp->nxforms = (dp->nxforms >= XFORM_LEN - 1) + dp->nxforms / 3 + 2;

	dp->c = dp->x = dp->y = 0.0;
	if (dp->liss && !halfrandom(dp, 10)) {
		dp->liss_time = 0;
	}
	if (!dp->grow)
		pick_df_coefs(mi);
	for (i = 0; i < dp->nxforms; i++) {
		if (NMAJORVARS == dp->major_variation)
			dp->variation[i] = halfrandom(dp, NMAJORVARS);
		else
			dp->variation[i] = dp->major_variation;
		for (j = 0; j < 2; j++)
			for (k = 0; k < 3; k++) {
				if (dp->liss)
					dp->f[j][k][i] = sin(dp->liss_time * dp->df[j][k][i]);
				else
					dp->f[j][k][i] = ((double) halfrandom(dp, 1000) / 500.0 - 1.0);
			}
	}
	if (dp->color)
		dp->pixcol = MI_PIXEL(mi, halfrandom(dp, MI_NPIXELS(mi)));
	else
		dp->pixcol = MI_WHITE_PIXEL(mi);

}


ENTRYPOINT void
init_drift(ModeInfo * mi)
{
	driftstruct *dp;

	MI_INIT (mi, drifts);
	dp = &drifts[MI_SCREEN(mi)];

	dp->width = MI_WIDTH(mi);
	dp->height = MI_HEIGHT(mi);
	dp->color = MI_NPIXELS(mi) > 2;

	if (MI_IS_FULLRANDOM(mi)) {
		if (NRAND(3) == 0)
			dp->grow = True;
		else {
			dp->grow = False;
			dp->liss = (Bool) (LRAND() & 1);
		}
	} else {
		dp->grow = grow;
		if (dp->grow)
			dp->liss = False;
		else
			dp->liss = liss;
	}
	initmode(mi, 1);
	initfractal(mi);
}

static void
iter(driftstruct * dp)
{
	int         i = frandom(dp, dp->nxforms);
	double      nx, ny, nc;


	if (i)
		nc = (dp->c + 1.0) / 2.0;
	else
		nc = dp->c / 2.0;

	nx = dp->f[0][0][i] * dp->x + dp->f[0][1][i] * dp->y + dp->f[0][2][i];
	ny = dp->f[1][0][i] * dp->x + dp->f[1][1][i] * dp->y + dp->f[1][2][i];


	switch (dp->variation[i]) {
		case 1:
			/* sinusoidal */
			nx = sin(nx);
			ny = sin(ny);
			break;
		case 2:
			{
				/* complex */
				double      r2 = nx * nx + ny * ny + 1e-6;

				nx = nx / r2;
				ny = ny / r2;
				break;
			}
		case 3:
			/* bent */
			if (nx < 0.0)
				nx = nx * 2.0;
			if (ny < 0.0)
				ny = ny / 2.0;
			break;
		case 4:
			{
				/* swirl */

				double      r = (nx * nx + ny * ny);	/* times k here is fun */
				double      c1 = sin(r);
				double      c2 = cos(r);
				double      t = nx;

				if (nx > 1e4 || nx < -1e4 || ny > 1e4 || ny < -1e4)
					ny = 1e4;
				else
					ny = c2 * t + c1 * ny;
				nx = c1 * nx - c2 * ny;
				break;
			}
		case 5:
			{
				/* horseshoe */
				double      r, c1, c2, t;

				/* Avoid atan2: DOMAIN error message */
				if (nx == 0.0 && ny == 0.0)
					r = 0.0;
				else
					r = atan2(nx, ny);	/* times k here is fun */
				c1 = sin(r);
				c2 = cos(r);
				t = nx;

				nx = c1 * nx - c2 * ny;
				ny = c2 * t + c1 * ny;
				break;
			}
		case 6:
			{
				/* drape */
				double      t;

				/* Avoid atan2: DOMAIN error message */
				if (nx == 0.0 && ny == 0.0)
					t = 0.0;
				else
					t = atan2(nx, ny) / M_PI;

				if (nx > 1e4 || nx < -1e4 || ny > 1e4 || ny < -1e4)
					ny = 1e4;
				else
					ny = sqrt(nx * nx + ny * ny) - 1.0;
				nx = t;
				break;
			}
	}

#if 0
	/* here are some others */
	{
		/* broken */
		if (nx > 1.0)
			nx = nx - 1.0;
		if (nx < -1.0)
			nx = nx + 1.0;
		if (ny > 1.0)
			ny = ny - 1.0;
		if (ny < -1.0)
			ny = ny + 1.0;
		break;
	}
	{
		/* complex sine */
		double      u = nx, v = ny;
		double      ev = exp(v);
		double      emv = exp(-v);

		nx = (ev + emv) * sin(u) / 2.0;
		ny = (ev - emv) * cos(u) / 2.0;
	}
	{

		/* polynomial */
		if (nx < 0)
			nx = -nx * nx;
		else
			nx = nx * nx;

		if (ny < 0)
			ny = -ny * ny;
		else
			ny = ny * ny;
	}
	{
		/* spherical */
		double      r = 0.5 + sqrt(nx * nx + ny * ny + 1e-6);

		nx = nx / r;
		ny = ny / r;
	}
	{
		nx = atan(nx) / M_PI_2
			ny = atan(ny) / M_PI_2
	}
#endif

	/* how to check nan too?  some machines don't have finite().
	   don't need to check ny, it'll propogate */
	if (nx > 1e4 || nx < -1e4) {
		nx = halfrandom(dp, 1000) / 500.0 - 1.0;
		ny = halfrandom(dp, 1000) / 500.0 - 1.0;
		dp->fuse = FUSE;
	}
	dp->x = nx;
	dp->y = ny;
	dp->c = nc;

}

static void
draw(ModeInfo * mi, driftstruct * dp, Drawable d)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	double      x = dp->x;
	double      y = dp->y;
	int         fixed_x, fixed_y, npix, c, n;

	if (dp->fuse) {
		dp->fuse--;
		return;
	}
	if (!(x > -1.0 && x < 1.0 && y > -1.0 && y < 1.0))
		return;

	fixed_x = (int) ((dp->width / 2) * (x + 1.0));
	fixed_y = (int) ((dp->height / 2) * (y + 1.0));

	if (!dp->rainbow) {

		dp->pts[dp->npoints].x = fixed_x;
		dp->pts[dp->npoints].y = fixed_y;
		dp->npoints++;
		if (dp->npoints == MAXBATCH1) {
			XSetForeground(display, gc, dp->pixcol);
			XDrawPoints(display, d, gc, dp->pts, dp->npoints, CoordModeOrigin);
			dp->npoints = 0;
		}
	} else {

		npix = MI_NPIXELS(mi);
		c = (int) (dp->c * npix);

		if (c < 0)
			c = 0;
		if (c >= npix)
			c = npix - 1;
		n = dp->ncpoints[c];
		dp->cpts[c * MAXBATCH2 + n].x = fixed_x;
		dp->cpts[c * MAXBATCH2 + n].y = fixed_y;
		if (++dp->ncpoints[c] == MAXBATCH2) {
			XSetForeground(display, gc, MI_PIXEL(mi, c));
			XDrawPoints(display, d, gc, &(dp->cpts[c * MAXBATCH2]),
				    dp->ncpoints[c], CoordModeOrigin);
			dp->ncpoints[c] = 0;
		}
	}
}

static void
draw_flush(ModeInfo * mi, driftstruct * dp, Drawable d)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);

	if (dp->rainbow) {
		int         npix = MI_NPIXELS(mi);
		int         i;

		for (i = 0; i < npix; i++) {
			if (dp->ncpoints[i]) {
				XSetForeground(display, gc, MI_PIXEL(mi, i));
				XDrawPoints(display, d, gc, &(dp->cpts[i * MAXBATCH2]),
					    dp->ncpoints[i], CoordModeOrigin);
				dp->ncpoints[i] = 0;
			}
		}
	} else {
		if (dp->npoints)
			XSetForeground(display, gc, dp->pixcol);
		XDrawPoints(display, d, gc, dp->pts,
			    dp->npoints, CoordModeOrigin);
		dp->npoints = 0;
	}
}


ENTRYPOINT void
draw_drift(ModeInfo * mi)
{
	Window      window = MI_WINDOW(mi);
	driftstruct *dp;

	if (drifts == NULL)
		return;
	dp = &drifts[MI_SCREEN(mi)];
	if (dp->ncpoints == NULL)
		return;

    if (dp->erase_countdown) {
      if (!--dp->erase_countdown) {
        initmode(mi, frandom(dp, 2));
        initfractal(mi);
      }
      return;
    }

	MI_IS_DRAWN(mi) = True;
	dp->timer = 3000;
	while (dp->timer) {
		iter(dp);
		draw(mi, dp, window);
		if (dp->total_points++ > dp->fractal_len) {
			draw_flush(mi, dp, window);
			if (0 == --dp->nfractals) {
              dp->erase_countdown = 4 * 1000000 / MI_PAUSE(mi);
				return;
			}
			initfractal(mi);
		}
		dp->timer--;
	}
	if (!dp->grow) {
		int         i, j, k;

		draw_flush(mi, dp, window);
		if (dp->liss)
			dp->liss_time++;
		for (i = 0; i < dp->nxforms; i++)
			for (j = 0; j < 2; j++)
				for (k = 0; k < 3; k++) {
					if (dp->liss)
						dp->f[j][k][i] = sin(dp->liss_time * dp->df[j][k][i]);
					else {
						double      t = dp->f[j][k][i] += dp->df[j][k][i];

						if (t < -1.0 || 1.0 < t)
							dp->df[j][k][i] *= -1.0;
					}
				}
	}
}

#ifndef STANDALONE
ENTRYPOINT void
refresh_drift(ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
}
#endif

XSCREENSAVER_MODULE ("Drift", drift)

#endif /* MODE_drift */
