/* -*- Mode: C; tab-width: 4 -*- */
/* lyapunov --- animated lyapunov space */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)lyapunov.c	5.09 2003/06/30 xlockmore";

#endif

/*-
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
 * Pictures discovered by Mario Markus and Benno Hess while trying to
 * simulate how enzymes break down carbohydrates.  By adjusting a pair of
 * parameter's they found they could make the simulated enzymes behave in
 * either a orderly or chaotic manner.  Images are based on a formula
 * named after the Russian mathematician Aleksandr M. Lyapunov.
 * See  A.K. Dewdney's "Computer Recreations", Scientific American
 * Magazine" Sep 1991 for more info.
 *
 * Revision History:
 * 30-Jun-2003: Changed writeable mode to be more consistent with
 *              xscreensaver's starfish
 * 01-Nov-2000: Allocation checks
 * 17-Dec-1998: Used mandelbrot.c as basis
 */


#ifdef STANDALONE
#define MODE_lyapunov
#define PROGCLASS "Lyapunov"
#define HACK_INIT init_lyapunov
#define HACK_DRAW draw_lyapunov
#define lyapunov_opts xlockmore_opts
#define DEFAULTS "*delay: 25000 \n" \
 "*count: 600 \n" \
 "*cycles: 20000 \n" \
 "*ncolors: 200 \n"
#define SMOOTH_COLORS
#define WRITABLE_COLORS
#include "xlockmore.h"		/* from the xscreensaver distribution */
#else /* !STANDALONE */
#include "xlock.h"		/* from the xlockmore distribution */
#include "color.h"
#endif /* !STANDALONE */

#ifdef MODE_lyapunov

#define MINPOWER 2
#define DEF_INCREMENT  "1.00"
/* 4.0 is best for seeing if a point is inside the set, 13 is better if
 * you want to get a pretty corona
 */

#define FLOATRAND(min,max) ((min)+((double) LRAND()/((double) MAXRAND))*((max)-(min)))

#define DEF_CYCLE "True"

static Bool cycle_p;

static XrmOptionDescRec opts[] =
{
  {(char *) "-cycle", (char *) ".lyapunov.cycle", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+cycle", (char *) ".lyapunov.cycle", XrmoptionNoArg, (caddr_t) "off"}
};

static argtype vars[] =
{
  {(void *) & cycle_p, (char *) "cycle", (char *) "Cycle", (char *) DEF_CYCLE
, t_Bool}
};
static OptionStruct desc[] =
{
  {(char *) "-/+cycle", (char *) "turn on/off colour cycling"}
};

ModeSpecOpt lyapunov_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   lyapunov_description =
{"lyapunov", "init_lyapunov", "draw_lyapunov", "release_lyapunov",
 (char *) NULL, "init_lyapunov", (char *) NULL, &lyapunov_opts,
 25000, 600, 20000, 1, 64, 1.0, "",
 "Shows lyapunov space", 0, NULL};

#endif

#define ROUND_FLOAT(x,a) ((float) ((int) ((x) / (a) + 0.5)) * (a))
#define MAXPOWER 14 /* (32 - 2) for 32 bit unsigned long */

#ifdef DEBUG
static int debug;
#endif

typedef struct {
	int         column;
	Bool        backwards, cycle_p, mono_p, no_colors;
	int         ncolors;
	unsigned int cur_color;
	GC          gc;
	Colormap    cmap;
	unsigned long blackpixel, whitepixel, fg, bg;
	int direction;
	XColor     *colors;
	int         screen_width;
	int         screen_height;
	float       reptop;
	unsigned long periodic_forcing;
	int firstone;
	ModeInfo   *mi;
	unsigned long seq;
	int          na, nb, hbit;
	int          iter_local;
	double       norm;
} lyapstruct;

static lyapstruct *lyaps = (lyapstruct *) NULL;
#define OFFSET 3.0  /* Slight changes here may have unexpected results! */

static double
reps(lyapstruct * lp, unsigned long periodic_forcing, int firstone, int iter, double a, double b)
{
	#define ABS_LESS(v, limit) ((-(limit) < (v)) && ((v) < (limit)))
	#define ABS_MORE(v, limit) (((v) < -(limit)) || ((limit) < (v)))

	#define ta_max 512
	double  ta[ta_max];
	int     ta_idx = 0;
	int     i, j = firstone;
	double  f = 0.5, t = 1.0, r;
	double  log_b;

	a += OFFSET;
	b += OFFSET; log_b = log(b);

	/* update cached values */
	if (lp->seq != periodic_forcing || lp->hbit != firstone)
	{
		lp->seq = periodic_forcing;
		lp->hbit = firstone;

		for (i=lp->hbit, lp->nb=0; i>=0; i--)
      			if (lp->seq & (1 << i))
				lp->nb++;
		lp->na = lp->hbit - lp->nb;
	}

  	if (iter != lp->iter_local)
	{
		lp->iter_local = iter;
		lp->norm = 1.0 / (log(2.0) * iter);
	}

	for (i = 0; i < iter; i++) {
		r = (lp->seq & (1 << j)) ? b : a;
		if (! j--)
			j = lp->hbit;

		f *= r*(1-f);
		t *= 1-2*f;

		if (ta_idx < ta_max)
		{
			if (ABS_LESS(t, 1e-50)) { ta[ta_idx++] = t; t = 1.0; }
			if (ABS_MORE(t, 1e+50)) { ta[ta_idx++] = t; t = 1.0; }
		}
	}

	/* post process */
	r = log(ABS(t)) + lp->na*log(ABS(a)) + lp->nb*log_b;
	for (i=0; i<ta_idx; i++)
        	r += log(ABS(ta[i]));

      	return r < 0 ? (r * lp->norm) : r;
}

static void
free_lyapunov(Display *display, lyapstruct *lp)
{
	ModeInfo *mi = lp->mi;

	if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
		MI_WHITE_PIXEL(mi) = lp->whitepixel;
		MI_BLACK_PIXEL(mi) = lp->blackpixel;
#ifndef STANDALONE
		MI_FG_PIXEL(mi) = lp->fg;
		MI_BG_PIXEL(mi) = lp->bg;
#endif
		if (lp->colors != NULL) {
			if (lp->ncolors && !lp->no_colors)
				free_colors(display, lp->cmap, lp->colors,
					lp->ncolors);
				free(lp->colors);
				lp->colors = (XColor *) NULL;
		}
		if (lp->cmap != None) {
			XFreeColormap(display, lp->cmap);
			lp->cmap = None;
		}
	}
	if (lp->gc != None) {
		XFreeGC(display, lp->gc);
		lp->gc = None;
	}
}

#ifndef STANDALONE
extern char *background;
extern char *foreground;
#endif

void
init_lyapunov(ModeInfo * mi)
{
	Window      window = MI_WINDOW(mi);
	Display    *display = MI_DISPLAY(mi);
	int i = NRAND(MAXPOWER);
	unsigned long power2;
	lyapstruct *lp;

	if (lyaps == NULL) {
		if ((lyaps = (lyapstruct *) calloc(MI_NUM_SCREENS(mi),
					     sizeof (lyapstruct))) == NULL)
			return;
	}
	lp = &lyaps[MI_SCREEN(mi)];
	lp->mi = mi;

#ifdef DEBUG
	debug = MI_IS_DEBUG(mi);
#endif
	lp->screen_width = MI_WIDTH(mi);
	lp->screen_height = MI_HEIGHT(mi);
	lp->backwards = (Bool) (LRAND() & 1);
	if (lp->backwards)
		lp->column = lp->screen_width - 1;
	else
		lp->column = 0;

	MI_CLEARWINDOW(mi);

	lp->reptop = 0.6;

	/* To force a number, say <i = 2;> has  i + 2 (or 4) binary digits */
  power2 = 1 << (i + 1);
  /* Do not want numbers which in binary are all 1's. */
  lp->periodic_forcing = NRAND(power2 - 1) + power2;
	lp->firstone = 1;
	for (i = 32 - 2; i >= 1; i--)
		if (lp->periodic_forcing & (1 << i)) {
			lp->firstone = i;
			break;
		}

  if (MI_IS_VERBOSE(mi)) {
		(void) printf("0x%lx forcing number", lp->periodic_forcing);
		switch (lp->periodic_forcing) {
			case 0x2:
				(void) printf(", swallow\n");
				break;
      case 0x34:
				(void) printf(", jellyfish\n");
				break;
			case 0xFC0: /* Yeah, I think it should be "city" too, but its not. */
				(void) printf(", zircon zity\n");
				break;
			default:
				(void) printf("\n");
				break;
		}
	}

	if (!lp->gc) {
		if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
			XColor      color;

#ifndef STANDALONE
			lp->fg = MI_FG_PIXEL(mi);
			lp->bg = MI_BG_PIXEL(mi);
#endif
			lp->blackpixel = MI_BLACK_PIXEL(mi);
			lp->whitepixel = MI_WHITE_PIXEL(mi);
			if ((lp->cmap = XCreateColormap(display, window,
					MI_VISUAL(mi), AllocNone)) == None) {
				free_lyapunov(display, lp);
				return;
			}
			XSetWindowColormap(display, window, lp->cmap);
			(void) XParseColor(display, lp->cmap, "black", &color);
			(void) XAllocColor(display, lp->cmap, &color);
			MI_BLACK_PIXEL(mi) = color.pixel;
			(void) XParseColor(display, lp->cmap, "white", &color);
			(void) XAllocColor(display, lp->cmap, &color);
			MI_WHITE_PIXEL(mi) = color.pixel;
#ifndef STANDALONE
			(void) XParseColor(display, lp->cmap, background, &color);
			(void) XAllocColor(display, lp->cmap, &color);
			MI_BG_PIXEL(mi) = color.pixel;
			(void) XParseColor(display, lp->cmap, foreground, &color);
			(void) XAllocColor(display, lp->cmap, &color);
			MI_FG_PIXEL(mi) = color.pixel;
#endif
			lp->colors = (XColor *) NULL;
			lp->ncolors = 0;
		}
		if ((lp->gc = XCreateGC(display, MI_WINDOW(mi),
			     (unsigned long) 0, (XGCValues *) NULL)) == None) {
			free_lyapunov(display, lp);
			return;
		}
	}
	MI_CLEARWINDOW(mi);

  /* Set up colour map */
  lp->direction = (LRAND() & 1) ? 1 : -1;
  if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
    if (lp->colors != NULL) {
      if (lp->ncolors && !lp->no_colors)
        free_colors(display, lp->cmap, lp->colors, lp->ncolors);
      free(lp->colors);
      lp->colors = (XColor *) NULL;
    }
    lp->ncolors = MI_NCOLORS(mi);
    if (lp->ncolors < 2)
      lp->ncolors = 2;
    if (lp->ncolors <= 2)
      lp->mono_p = True;
    else
      lp->mono_p = False;

    if (lp->mono_p)
      lp->colors = (XColor *) NULL;
    else
      if ((lp->colors = (XColor *) malloc(sizeof (*lp->colors) *
          (lp->ncolors + 1))) == NULL) {
        free_lyapunov(display, lp);
        return;
      }
    lp->cycle_p = has_writable_cells(mi);
    if (lp->cycle_p) {
      if (MI_IS_FULLRANDOM(mi)) {
        if (!NRAND(8))
          lp->cycle_p = False;
        else
          lp->cycle_p = True;
      } else {
        lp->cycle_p = cycle_p;
      }
    }
    if (!lp->mono_p) {
      if (!(LRAND() % 10))
        make_random_colormap(
#if STANDALONE
            display, MI_WINDOW(mi),
#else
            mi,
#endif
              lp->cmap, lp->colors, &lp->ncolors,
              True, True, &lp->cycle_p);
      else if (!(LRAND() % 2))
        make_uniform_colormap(
#if STANDALONE
            display, MI_WINDOW(mi),
#else
            mi,
#endif
                  lp->cmap, lp->colors, &lp->ncolors,
                  True, &lp->cycle_p);
      else
        make_smooth_colormap(
#if STANDALONE
            display, MI_WINDOW(mi),
#else
            mi,
#endif
                 lp->cmap, lp->colors, &lp->ncolors,
                 True, &lp->cycle_p);
    }
    XInstallColormap(display, lp->cmap);
    if (lp->ncolors < 2) {
      lp->ncolors = 2;
      lp->no_colors = True;
    } else
      lp->no_colors = False;
    if (lp->ncolors <= 2)
      lp->mono_p = True;

    if (lp->mono_p)
      lp->cycle_p = False;

  }
  if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
                if (lp->mono_p) {
			lp->cur_color = MI_BLACK_PIXEL(mi);
		}
  }
	lp->seq = (unsigned long) -1;
	lp->hbit = -1;
	lp->iter_local = -1;
}

void
draw_lyapunov(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	int         h;
	lyapstruct *lp;

	if (lyaps == NULL)
		return;
	lp = &lyaps[MI_SCREEN(mi)];

	MI_IS_DRAWN(mi) = True;

        if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
                if (lp->mono_p) {
                        XSetForeground(display, lp->gc, lp->cur_color);
                } else {
                        lp->cur_color = (lp->cur_color + 1) % lp->ncolors;
                        XSetForeground(display, lp->gc, lp->colors[lp->cur_color].pixel);
                }
        } else {
                if (MI_NPIXELS(mi) > 2)
                        XSetForeground(display, lp->gc, MI_PIXEL(mi, lp->cur_color));
                else if (lp->cur_color)
                        XSetForeground(display, lp->gc, MI_BLACK_PIXEL(mi));
                else
                        XSetForeground(display, lp->gc, MI_WHITE_PIXEL(mi));
                if (++lp->cur_color >= (unsigned int) MI_NPIXELS(mi))
                        lp->cur_color = 0;
        }

  /* Rotate colours */
  if (lp->cycle_p) {
    rotate_colors(display, lp->cmap, lp->colors, lp->ncolors,
      lp->direction);
    if (!(LRAND() % 1000))
      lp->direction = -lp->direction;
  }
	/* so we iterate columns beyond the width of the physical screen,
        ** so that we just wait around and show what we've done
	*/
	if ((!lp->backwards && (lp->column >= 4 * lp->screen_width)) ||
	    (lp->backwards && (lp->column < -3 * lp->screen_width))) {
		/* reset to left edge of screen */
		init_lyapunov(mi);
		return;
		/* select a new region! */
	} else if (lp->column >= lp->screen_width || lp->column < 0) {
		/* delay a while */
		if (lp->backwards)
			lp->column--;
		else
			lp->column++;
		return;
	}
	for (h = 0; h < lp->screen_height; h++) {
		unsigned int color;
		double  result;

#define MULT 1.0
		result = reps(lp, lp->periodic_forcing, lp->firstone, MI_COUNT(mi),
			((double) lp->column) / (MULT * lp->screen_width),
			((double) h) / (MULT * lp->screen_height));
#ifdef DEBUG
		if (debug)
		(void) printf("a %g, b %g, result %g\n",
			 ((double) lp->column) / lp->screen_width,
			 ((double) h) / lp->screen_height, result);
#endif
		if (result > 0.0)
			XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		else {
			color=(unsigned int) ((MI_NPIXELS(mi) * (float)-result) / lp->reptop);
			color = color % MI_NPIXELS(mi);
			XSetForeground(display, gc, MI_PIXEL(mi, color));
		}
		/* we no longer have vertical symmetry - so we compute all points
		** and do not draw with redundancy
		*/
		XDrawPoint(display, window, gc, lp->column, h);
	}
	if (lp->backwards)
		lp->column--;
	else
		lp->column++;
}

void
release_lyapunov(ModeInfo * mi)
{
	if (lyaps != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_lyapunov(MI_DISPLAY(mi), &lyaps[screen]);
		free(lyaps);
		lyaps = (lyapstruct *) NULL;
	}
}

#endif /* MODE_lyapunov */
