/* -*- Mode: C; tab-width: 4 -*- */
/* mandelbrot --- animated mandelbrot sets */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)mandelbrot.c 4.07 97/11/24 xlockmore";

#endif

/*-
 * Copyright (c) 1997 Dan Stromberg <strombrg@nis.acs.uci.edu>
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
 * 20-Oct-97: Written by Dan Stromberg <strombrg@nis.acs.uci.edu>
 */


#ifdef STANDALONE
#define PROGCLASS "Mandelbrot"
#define HACK_INIT init_mandelbrot
#define HACK_DRAW draw_mandelbrot
#define mandelbrot_opts xlockmore_opts
#define DEFAULTS "*delay: 25000 \n" \
 "*count: -8 \n" \
 "*cycles: 20000 \n" \
 "*ncolors: 200 \n"
#define SMOOTH_COLORS
#define WRITABLE_COLORS
#include "xlockmore.h"		/* from the xscreensaver distribution */
#include <X11/Xutil.h>
#else /* !STANDALONE */
#include "xlock.h"		/* from the xlockmore distribution */
#endif /* !STANDALONE */

#define MINPOWER 1
#define DEF_INCREMENT  "1.00"

	/* incr also would be nice as a parameter.  It controls how fast
	   the order is advanced.  Non-integral values are not true orders,
	   but it's a somewhat interesting function anyway
	 */
static float increment;

static XrmOptionDescRec opts[] =
{
     {"-increment", ".mandelbrot.increment", XrmoptionSepArg, (caddr_t) NULL}
};

static argtype vars[] =
{
 {(caddr_t *) & increment, "increment", "Increment", DEF_INCREMENT, t_Float},
};
static OptionStruct desc[] =
{
	{"-increment value", "increasing orders"}
};

ModeSpecOpt mandelbrot_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   mandelbrot_description =
{"mandelbrot", "init_mandelbrot", "draw_mandelbrot", "release_mandelbrot",
 NULL, "init_mandelbrot", NULL, &mandelbrot_opts,
 25000, -8, 20000, 1, 64, 1.0, "",
 "Shows mandelbrot sets", 0, NULL};

#endif

#define ROUND_FLOAT(x,a) ((float) ((int) ((x) / (a) + 0.5)) * (a))

typedef struct {
	double      real, imag;
} complex;

static void
add(complex * a, complex b)
{
	a->real = a->real + b.real;
	a->imag = a->imag + b.imag;
}

static void
mult(complex * a, complex b)
{
	double      tr, ti;

	/* a.real*b.real + i*a.real*b.imag + i*a.imag*b.real + i^2*a.imag*b.imag */
	tr = a->real * b.real - a->imag * b.imag;
	ti = a->real * b.imag + a->imag * b.real;
	a->real = tr;
	a->imag = ti;
}

/* this IS a true power function. */
static void
ipow(complex * a, int n)
{
	int         t2;
	complex     a2;

	switch (n) {
		case 1:
			return;
		case 2:
			mult(a, *a);
			return;
		default:
			t2 = n / 2;
			a2 = *a;
			ipow(&a2, t2);
			mult(&a2, a2);
			if (t2 * 2 != n)	/* if n is odd */
				mult(&a2, *a);
			*a = a2;
			;;
	}
}

/*-
 * This is not a true power function, but it's an acceptable approximation.
 * Ok, actually, it yields a bit of visible distortion.
 */
static void
rpow(complex * a, double n)
{
	int         low, high;

	low = (int) floor(n);
	high = (int) ceil(n);
	if (low == high) {
		ipow(a, (int) n);
	} else {
		complex     x, y;
		double      frac;

		frac = n - low;
		/* x = a^low */
		x = *a;
		ipow(&x, low);
		/* y = a^low+1 */
		y = x;
		mult(&y, *a);
		a->real = x.real * frac + y.real * (1 - frac);
		a->imag = x.imag * frac + y.imag * (1 - frac);
	}
}

typedef struct {
	int         counter;
	double      power;
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
} mandelstruct;

static mandelstruct *mandels = NULL;

void
init_mandelbrot(ModeInfo * mi)
{
	mandelstruct *mp;
	int         preserve;
	Bool        truecolor;
	unsigned long redmask, greenmask, bluemask;
	Window      window = MI_WINDOW(mi);
	Display    *display = MI_DISPLAY(mi);
	int         power;

	if (mandels == NULL) {
		if ((mandels = (mandelstruct *) calloc(MI_NUM_SCREENS(mi),
					     sizeof (mandelstruct))) == NULL)
			return;
	}
	mp = &mandels[MI_SCREEN(mi)];


	power = MI_BATCHCOUNT(mi);
	if (power < -MINPOWER) {
		int         temp;

		do {
			temp = NRAND(-power - MINPOWER + 1) + MINPOWER;
		} while (temp == mp->power);
		mp->power = temp;
	} else if (power < MINPOWER)
		mp->power = MINPOWER;
	else
		mp->power = power;

	mp->column = 0;
	mp->counter = 0;

	MI_CLEARWINDOW(mi);

	mp->fixed_colormap = !setupColormap(mi,
		&(mp->ncolors), &truecolor, &redmask, &greenmask, &bluemask);
	if (!mp->fixed_colormap) {
		preserve = preserveColors(MI_BLACK_PIXEL(mi), MI_WHITE_PIXEL(mi),
					  MI_BG_PIXEL(mi), MI_FG_PIXEL(mi));
		mp->bgcol.pixel = MI_BG_PIXEL(mi);
		mp->fgcol.pixel = MI_FG_PIXEL(mi);
		XQueryColor(display, MI_COLORMAP(mi), &(mp->bgcol));
		XQueryColor(display, MI_COLORMAP(mi), &(mp->fgcol));

#define RANGE 65536
#define DENOM 2
		/* color in the bottom part */
		mp->bottom.red = NRAND(RANGE / DENOM);
		mp->bottom.blue = NRAND(RANGE / DENOM);
		mp->bottom.green = NRAND(RANGE / DENOM);
		/* color in the top part */
		mp->top.red = RANGE - NRAND(RANGE / DENOM);
		mp->top.blue = RANGE - NRAND(RANGE / DENOM);
		mp->top.green = RANGE - NRAND(RANGE / DENOM);

		/* allocate colormap, if needed */
		if (mp->colors == NULL)
			mp->colors = (XColor *) malloc(sizeof (XColor) * mp->ncolors);
		if (mp->cmap == None)
			mp->cmap = XCreateColormap(display, window,
						   MI_VISUAL(mi), AllocAll);
		mp->usable_colors = mp->ncolors - preserve;
	}
}

static int
reps(complex c, double p, int r)
{
	int         rep;
	int         escaped = 0;
	complex     t;

	t = c;
	for (rep = 0; rep < r; rep++) {
		rpow(&t, p);
		add(&t, c);
		if (t.real * t.real + t.imag * t.imag >= 4.0) {
			escaped = 1;
			break;
		}
	}
	if (escaped)
		return rep;
	else
		return r;
}

void
draw_mandelbrot(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	mandelstruct *mp = &mandels[MI_SCREEN(mi)];
	int         screen_width, screen_height;
	int         h;
	complex     c;
	double      log_top;

	/* it'd be nice if top were parameterized.  It really impacts how fast
	   this all runs, but lower values "look sloppy".
	 */
	int         top = 300;

	unsigned int i;
	int         j, k;

	log_top = log((float) top);
	screen_width = MI_WIN_WIDTH(mi);
	screen_height = ((MI_WIN_HEIGHT(mi) + 1) >> 1) << 1;	/* screen_height even */

	if (mp->counter % 5 == 0) {
		/* advance "drawing" color */
		mp->cur_color = (mp->cur_color + 1) % mp->ncolors;
		if (!mp->fixed_colormap && mp->usable_colors > 2) {
			while (mp->cur_color == MI_WHITE_PIXEL(mi) ||
			       mp->cur_color == MI_BLACK_PIXEL(mi) ||
			       mp->cur_color == MI_BG_PIXEL(mi) ||
			       mp->cur_color == MI_FG_PIXEL(mi)) {
				mp->cur_color = (mp->cur_color + 1) % mp->ncolors;
			}
		}
		/* advance colormap */
		if (!mp->fixed_colormap && mp->usable_colors > 2) {
			for (i = 0, j = mp->cur_color, k = 0;
			     i < (unsigned int) mp->ncolors; i++) {
				if (i == MI_WHITE_PIXEL(mi)) {
					mp->colors[i].pixel = i;
					mp->colors[i].red = 65535;
					mp->colors[i].blue = 65535;
					mp->colors[i].green = 65535;
					mp->colors[i].flags = DoRed | DoGreen | DoBlue;
				} else if (i == MI_BLACK_PIXEL(mi)) {
					mp->colors[i].pixel = i;
					mp->colors[i].red = 0;
					mp->colors[i].blue = 0;
					mp->colors[i].green = 0;
					mp->colors[i].flags = DoRed | DoGreen | DoBlue;
				} else if (i == MI_BG_PIXEL(mi)) {
					mp->colors[i] = mp->bgcol;
				} else if (i == MI_FG_PIXEL(mi)) {
					mp->colors[i] = mp->fgcol;
				} else {
					double      range;

					mp->colors[i].pixel = i;
					range = ((double) (mp->top.red - mp->bottom.red)) /
						mp->ncolors;
					mp->colors[i].red = (short unsigned int) (range * j +
							     mp->bottom.red);
					range = ((double) (mp->top.green - mp->bottom.green)) /
						mp->ncolors;
					mp->colors[i].green = (short unsigned int) (range * j +
							   mp->bottom.green);
					range = ((double) (mp->top.blue - mp->bottom.blue)) /
						mp->ncolors;
					mp->colors[i].blue = (short unsigned int) (range * j +
							    mp->bottom.blue);
					mp->colors[i].flags = DoRed | DoGreen | DoBlue;
					j = (j + 1) % mp->ncolors;
					k++;
				}
			}
			/* make the entire tube move forward */
			XStoreColors(display, mp->cmap, mp->colors, mp->ncolors);
			setColormap(display, window, mp->cmap, MI_WIN_IS_INWINDOW(mi));
		}
	}
	if (mp->column >= 3 * screen_width / 2) {
		/* reset to left edge of screen, bump power */
		mp->column = 0;
		if (MI_BATCHCOUNT(mi) >= -MINPOWER) {	/* Then do not randomize */
			mp->power += increment;
			/* round to third digit after decimal, in case of floating point
			   inaccuracy.  Assumption: no one wants to increment this by
			   less than 0.001 at a time
			 */
			mp->power = ROUND_FLOAT(mp->power, 0.001);
		} else {
			int         temp;

			do {
				temp = NRAND(-MI_BATCHCOUNT(mi) - MINPOWER + 1) + MINPOWER;
			} while (temp == mp->power);
			mp->power = temp;
		}
	} else if (mp->column >= screen_width) {
		/* delay a while */
		mp->column++;
		mp->counter++;
		return;
	}
	/* assumes screen height is even */
	for (h = 0; h < screen_height / 2; h++) {
#if defined(ALT_POW)
		int         first;
		int         second;
		double      frac;
		int         high, low;


#endif
		unsigned int color;
		int         result;

		/* c.real = -2.1 + (double) mp->column / screen_width * 3.4; */
		c.real = 1.3 - (double) mp->column / screen_width * 3.4;
		c.imag = -1.6 + (double) h / screen_height * 3.2;
#if defined(ALT_POW)
		high = ceil(mp->power);
		low = floor(mp->power);
		frac = mp->power - low;

		first = reps(c, low, top);
		if (low == high)
			result = first;
		else {
			second = reps(c, high, top);
			result = first * (1.0 - frac) + second * frac;
		}
#else
		result = reps(c, mp->power, top);
#endif
		if (result == 0 || result == top)
			XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		else {
			color = (unsigned int) (MI_NPIXELS(mi) * log((float) result) / log_top);
			if (!mp->fixed_colormap && mp->usable_colors > 2) {
				while (color == MI_WHITE_PIXEL(mi) ||
				       color == MI_BLACK_PIXEL(mi) ||
				       color == MI_BG_PIXEL(mi) ||
				       color == MI_FG_PIXEL(mi)) {
					color = (color + 1) % mp->ncolors;
				}
				XSetForeground(display, gc, color);
			} else
				XSetForeground(display, gc, MI_PIXEL(mi, color));
		}
		/* take advantage of vertical symmetry */
		XDrawPoint(display, window, gc, mp->column, h);
		XDrawPoint(display, window, gc, mp->column, screen_height - h - 1);
	}
	mp->column++;
	mp->counter++;
	if (mp->counter > MI_CYCLES(mi)) {
		init_mandelbrot(mi);
	}
}

void
release_mandelbrot(ModeInfo * mi)
{
	if (mandels != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			mandelstruct *mp = &mandels[screen];

			if (mp->cmap != None)
				XFreeColormap(MI_DISPLAY(mi), mp->cmap);
			if (mp->colors != NULL)
				XFree((caddr_t) mp->colors);
		}
		(void) free((void *) mandels);
		mandels = NULL;
	}
}
