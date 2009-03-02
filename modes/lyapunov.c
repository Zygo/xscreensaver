/* -*- Mode: C; tab-width: 4 -*- */
/* lyapunov --- animated lyapunov space */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)lyapunov.c 4.13 98/12/21 xlockmore";

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
 * 17-Dec-98: Used as basis mandelbrot.c
 */


#ifdef STANDALONE
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
#include <X11/Xutil.h>
#else /* !STANDALONE */
#include "xlock.h"		/* from the xlockmore distribution */
#include "vis.h"
#include "color.h"
#endif /* !STANDALONE */

#ifdef MODE_lyapunov

#define MINPOWER 2
#define DEF_INCREMENT  "1.00"
/* 4.0 is best for seeing if a point is inside the set, 13 is better if
** you want to get a pretty corona
*/

#define FLOATRAND(min,max) ((min)+((double) LRAND()/((double) MAXRAND))*((max)-(min)))

ModeSpecOpt lyapunov_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   lyapunov_description =
{"lyapunov", "init_lyapunov", "draw_lyapunov", "release_lyapunov",
 NULL, "init_lyapunov", NULL, &lyapunov_opts,
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
	XColor      fgcol, bgcol;	/* foreground and background colour specs */
	Bool        fixed_colormap;
	int         ncolors;
	XColor      top;
	XColor      bottom;
	XColor     *colors;
	Colormap    cmap;
	int         usable_colors;
	unsigned int cur_color;
	int         screen_width;
	int			screen_height;
	float			reptop;
  unsigned long periodic_forcing;
  int firstone;
} lyapstruct;

static lyapstruct *lyaps = NULL;
#define OFFSET 3.0  /* Slight changes here may have unexpected results! */

static double
reps(unsigned long periodic_forcing, int firstone, int iter, double a, double b)
{
	int         forcing_index = firstone, n;
  double      x = 0.5, total = 0.0, gamma;

  a += OFFSET, b += OFFSET;
	for (n = 1; n <= iter; n++) {
 		gamma = (periodic_forcing & (1 << forcing_index)) ? b : a;
		if (forcing_index == 0)
			forcing_index = firstone;
		else
			forcing_index--;
		x = gamma * x * (1.0 - x);
		/* Sci. Am. article says this should be a table */
		total += (log(ABS(gamma * (1.0 - 2.0 * x))) / M_LN2);
#ifdef DEBUG
		if (debug)
    /*if (!(n % 1000)) */
    (void) printf("gamma %g, a %g,  b %g, result %g, n %d, total %g, x %g, index %d\n",
			 gamma, a, b, total / n, n, total, x, forcing_index);
#endif
	}
	return (total / ((double) iter));
}

void
init_lyapunov(ModeInfo * mi)
{
	lyapstruct *lp;
	int         preserve;
	Bool        truecolor;
	unsigned long redmask, greenmask, bluemask;
	Window      window = MI_WINDOW(mi);
	Display    *display = MI_DISPLAY(mi);
	int i = NRAND(MAXPOWER);
  unsigned long power2;

	if (lyaps == NULL) {
		if ((lyaps = (lyapstruct *) calloc(MI_NUM_SCREENS(mi),
					     sizeof (lyapstruct))) == NULL)
			return;
	}
	lp = &lyaps[MI_SCREEN(mi)];

#ifdef DEBUG
	debug = MI_IS_DEBUG(mi);
#endif
	lp->column = 0;

	MI_CLEARWINDOW(mi);

	lp->screen_width = MI_WIDTH(mi);
	lp->screen_height = MI_HEIGHT(mi);

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

#ifndef STANDALONE
	lp->fixed_colormap = !setupColormap(mi,
		&(lp->ncolors), &truecolor, &redmask, &greenmask, &bluemask);
	if (!lp->fixed_colormap) {
		preserve = preserveColors(MI_BLACK_PIXEL(mi), MI_WHITE_PIXEL(mi),
					  MI_BG_PIXEL(mi), MI_FG_PIXEL(mi));
		lp->bgcol.pixel = MI_BG_PIXEL(mi);
		lp->fgcol.pixel = MI_FG_PIXEL(mi);
		XQueryColor(display, MI_COLORMAP(mi), &(lp->bgcol));
		XQueryColor(display, MI_COLORMAP(mi), &(lp->fgcol));

#define RANGE 65536
#define DENOM 4
		/* color in the bottom part */
		lp->bottom.red = NRAND(RANGE / DENOM);
		lp->bottom.blue = NRAND(RANGE / DENOM);
		lp->bottom.green = NRAND(RANGE / DENOM);
		/* color in the top part */
		lp->top.red = RANGE - NRAND(((DENOM-1)*RANGE) / DENOM);
		lp->top.blue = RANGE - NRAND(((DENOM-1)*RANGE) / DENOM);
		lp->top.green = RANGE - NRAND(((DENOM-1)*RANGE) / DENOM);

		/* allocate colormap, if needed */
		if (lp->colors == NULL)
			lp->colors = (XColor *) malloc(sizeof (XColor) * lp->ncolors);
		if (lp->cmap == None)
			lp->cmap = XCreateColormap(display, window,
						   MI_VISUAL(mi), AllocAll);
		lp->usable_colors = lp->ncolors - preserve;
	}

#endif /* !STANDALONE */
}

#if !defined(OLD_COLOR)
static unsigned short
pick_rgb(unsigned short bottom,unsigned short top,
	int color_ind,int ncolors)
{
	/* por == part of range */
	double por;

	if (color_ind<ncolors/2) {
		/* going up */
		por = ((float)color_ind*2) / ncolors;
	} else {
		/* going down */
		por = ((float)(color_ind-ncolors/2)*2) / ncolors;
	}
	return ((unsigned short) (bottom + (top - bottom)*por));
}
#endif

void
draw_lyapunov(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	lyapstruct *lp = &lyaps[MI_SCREEN(mi)];
	int         h;

	unsigned int i;
	int         j, k;

	MI_IS_DRAWN(mi) = True;

#ifndef STANDALONE
	if (lp->column % 15 == 0) {
		/* advance "drawing" color */
		lp->cur_color = (lp->cur_color + 1) % lp->ncolors;
		if (!lp->fixed_colormap && lp->usable_colors > 2) {
			while (lp->cur_color == MI_WHITE_PIXEL(mi) ||
			       lp->cur_color == MI_BLACK_PIXEL(mi) ||
			       lp->cur_color == MI_BG_PIXEL(mi) ||
			       lp->cur_color == MI_FG_PIXEL(mi)) {
				lp->cur_color = (lp->cur_color + 1) % lp->ncolors;
			}
		}
		/* advance colormap */
		if (!lp->fixed_colormap && lp->usable_colors > 2) {
			for (i = 0, j = lp->cur_color, k = 0;
			     i < (unsigned int) lp->ncolors; i++) {
				if (i == MI_WHITE_PIXEL(mi)) {
					lp->colors[i].pixel = i;
					lp->colors[i].red = 65535;
					lp->colors[i].blue = 65535;
					lp->colors[i].green = 65535;
					lp->colors[i].flags = DoRed | DoGreen | DoBlue;
				} else if (i == MI_BLACK_PIXEL(mi)) {
					lp->colors[i].pixel = i;
					lp->colors[i].red = 0;
					lp->colors[i].blue = 0;
					lp->colors[i].green = 0;
					lp->colors[i].flags = DoRed | DoGreen | DoBlue;
				} else if (i == MI_BG_PIXEL(mi)) {
					lp->colors[i] = lp->bgcol;
				} else if (i == MI_FG_PIXEL(mi)) {
					lp->colors[i] = lp->fgcol;
				} else {
#if defined(OLD_COLOR)
					/* this is known to work well enough. I just wanted
					** to explore some other possibilities
					*/
					double      range;
					lp->colors[i].pixel = i;
					range = ((double) (lp->top.red - lp->bottom.red)) /
						lp->ncolors;
					lp->colors[i].red = (short unsigned int) (range * j +
							     lp->bottom.red);
					range = ((double) (lp->top.green - lp->bottom.green)) /
						lp->ncolors;
					lp->colors[i].green = (short unsigned int) (range * j +
							   lp->bottom.green);
					range = ((double) (lp->top.blue - lp->bottom.blue)) /
						lp->ncolors;
					lp->colors[i].blue = (short unsigned int) (range * j +
							    lp->bottom.blue);
					lp->colors[i].flags = DoRed | DoGreen | DoBlue;
					j = (j + 1) % lp->ncolors;
					k++;
#else
					lp->colors[i].pixel = i;
					lp->colors[i].red = pick_rgb(lp->bottom.red,lp->top.red,
						j,lp->ncolors);
					lp->colors[i].green = pick_rgb(lp->bottom.green,lp->top.green,
						j,lp->ncolors);
					lp->colors[i].blue = pick_rgb(lp->bottom.blue,lp->top.blue,
						j,lp->ncolors);
					lp->colors[i].flags = DoRed | DoGreen | DoBlue;
					j = (j + 1) % lp->ncolors;
					k++;
#endif
				}
			}
			/* make the entire lyapunov move forward */
			XStoreColors(display, lp->cmap, lp->colors, lp->ncolors);
			setColormap(display, window, lp->cmap, MI_IS_INWINDOW(mi));
		}
	}
#endif /* !STANDALONE */
	/* so we iterate columns beyond the width of the physical screen, so that
	** we just wait around and show what we've done
	*/
	if (lp->column >= 4 * lp->screen_width) {
		/* reset to left edge of screen */
		init_lyapunov(mi);
		return;
		/* select a new region! */
	} else if (lp->column >= lp->screen_width) {
		/* delay a while */
		lp->column++;
		return;
	}
	for (h = 0; h < lp->screen_height; h++) {
		unsigned int color;
		double  result;

#define MULT 1.0
		result = reps(lp->periodic_forcing, lp->firstone, MI_COUNT(mi),
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
#ifndef STANDALONE
			if (!lp->fixed_colormap && lp->usable_colors > 2) {
				while (color == MI_WHITE_PIXEL(mi) ||
				       color == MI_BLACK_PIXEL(mi) ||
				       color == MI_BG_PIXEL(mi) ||
				       color == MI_FG_PIXEL(mi)) {
					color = (color + 1) % lp->ncolors;
				}
				XSetForeground(display, gc, color);
			} else
				XSetForeground(display, gc, MI_PIXEL(mi, color));
#endif /* !STANDALONE */
		}
		/* we no longer have vertical symmetry - so we colpute all points
		** and don't draw with redundancy
		*/
		XDrawPoint(display, window, gc, lp->column, h);
	}
	lp->column++;
}

void
release_lyapunov(ModeInfo * mi)
{
	if (lyaps != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			lyapstruct *lp = &lyaps[screen];

			if (lp->cmap != None)
				XFreeColormap(MI_DISPLAY(mi), lp->cmap);
			if (lp->colors != NULL)
				XFree((caddr_t) lp->colors);
		}
		(void) free((void *) lyaps);
		lyaps = NULL;
	}
}

#endif /* MODE_lyapunov */
