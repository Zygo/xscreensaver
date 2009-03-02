/* -*- Mode: C; tab-width: 4 -*- */
/* mandelbrot --- animated mandelbrot sets */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)mandelbrot.c 4.13 98/12/21 xlockmore";

#endif

#define USE_LOG

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
 * See  A.K. Dewdney's "Computer Recreations", Scientific American 
 * Magazine" Aug 1985 for more info.  Also A.K. Dewdney's "Computer
 * Recreations", Scientific American Magazine" Jul 1989, has some neat
 * extensions besides z^n + c (n small but >= 2) some of these are:
 *   z^z + z^n + c
 *   sin(z) + e^z + c 
 *   sin(z) + z^n + c
 * These were first explored by a colleague of Mandelbrot, Clifford A.
 * Pickover.  These would make nice additions to add.
 *
 * Revision History:
 * 24-Mar-99: DEM and Binary decomp options added by Tim.Auckland@Sun.COM
 *            Ideas from Peitgen & Saupe's "The Science of Fractal Images"
 * 20-Oct-97: Written by Dan Stromberg <strombrg@nis.acs.uci.edu>
 * 17-Nov-98: Many Changes by Stromberg, including selection of
 *            interesting subregions, more extreme color ranges,
 *            reduction of possible powers to smaller/more interesting
 *            range, elimination of some unused code, slower color cycling,
 *            longer period of color cycling after the drawing is complete.
 *            Hopefully the longer color cycling period will make the mode
 *            reasonable even after CPUs are so fast that the drawing
 *            interval goes by quickly
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
#include "vis.h"
#include "color.h"
#endif /* !STANDALONE */

#ifdef MODE_mandelbrot

#define MINPOWER 2
#define DEF_INCREMENT  "1.00"
#define DEF_BINARY     "False"
#define DEF_DEM        "False"
/* 4.0 is best for seeing if a point is inside the set, 13 is better if
** you want to get a pretty corona
*/
#define ESCAPE 13.0
#define FLOATRAND(min,max) ((min)+((double) LRAND()/((double) MAXRAND))*((max)-(min)))

	/* incr also would be nice as a parameter.  It controls how fast
	   the order is advanced.  Non-integral values are not true orders,
	   but it's a somewhat interesting function anyway
	 */
static float increment;
static Bool  binary;
static Bool  dem;

static XrmOptionDescRec opts[] =
{
     {"-increment", ".mandelbrot.increment", XrmoptionSepArg, (caddr_t) NULL},
     {"-binary", ".mandelbrot.binary", XrmoptionNoArg, (caddr_t) "on"},
     {"+binary", ".mandelbrot.binary", XrmoptionNoArg, (caddr_t) "off"},
     {"-dem", ".mandelbrot.dem", XrmoptionNoArg, (caddr_t) "on"},
     {"+dem", ".mandelbrot.dem", XrmoptionNoArg, (caddr_t) "off"}
};

static argtype vars[] =
{
 {(caddr_t *) & increment, "increment", "Increment", DEF_INCREMENT, t_Float},
 {(caddr_t *) & binary, "binary", "Binary", DEF_BINARY, t_Bool},
 {(caddr_t *) & dem, "dem", "Dem", DEF_DEM, t_Bool},
};
static OptionStruct desc[] =
{
	{"-increment value", "increasing orders"},
	{"-/+binary", "turn on/off Binary Decomposition colour modulation"},
	{"-/+dem", "turn on/off Distance Estimator Method (instead of escape time)"}
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

/* Plans for future additions?  a, z complex; x, y real
   sin(z) = (e^(i * z) - e^(-i * z)) / (2 * i)
   cos(z) = (e^(i * z) - e^(-i * z)) / 2
   a^z = e^(z * ln(a))
   e^z = e^x * (cos(y) + i * sin(y))
   ln(z) = ln ((x^2 + y^2)^(1/2)) + i * invtan(y/x)
 */

/* this is a true power function. */
static void
ipow(complex * a, int n)
{


	switch (n) {
		case 1:
			return;
		case 2:
			mult(a, *a);
			return;
		default:
			{
				complex     a2;
				int         t2;

				a2 = *a;	/* Not very efficient to use:  mult(a, ipow(&a2, n-1)); */
				t2 = n / 2;
				ipow(&a2, t2);
				mult(&a2, a2);
				if (t2 * 2 != n)	/* if n is odd */
					mult(&a2, *a);
				*a = a2;
			}
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
	complex		extreme_ul;
	complex		extreme_lr;
	complex		ul;
	complex		lr;
	int         screen_width;
	int			screen_height;
	int			reptop;
	Bool		dem;
	Bool		binary;
} mandelstruct;

static mandelstruct *mandels = NULL;

/* do the iterations
 * if binary is true, check halfplane of last iteration.
 * if demrange is non zero, estimate lower bound of dist(c, M)
 * Loosely based on  Peitgen & Saupe's "The Science of Fractal Images" 
 */
static int
reps(complex c, double p, int r, Bool binary, double demrange)
{
	int         rep;
	int         escaped = 0;
	complex     t;
	int escape = (demrange == 0) ? ESCAPE :
		ESCAPE*ESCAPE*ESCAPE*ESCAPE; /* 2 more iterations */
	complex     t1;
	complex     dt;

	t = c;
	dt.real = 1; dt.imag = 0;
	for (rep = 0; rep < r; rep++) {
	    t1 = t;
		ipow(&t, p);
		add(&t, c);
		if (t.real * t.real + t.imag * t.imag >= escape) {
			escaped = 1;
			break;
		}
		if (demrange){
			/* compute dt/dc 
			 *               p-1
			 * dt    =  p * t  * dt + 1
			 *   k+1         k     k
			 */
			dt.real *= p; dt.imag *= p;
			if(p > 2) ipow(&t1, p - 1);
			mult(&dt, t1);
			dt.real += 1;
			if (dt.real * dt.real + dt.imag * dt.imag >= 1e300) {
				escaped = 2;
				break;
			}
		}
	}
	if (escaped) {
		if(demrange) {
			double mt = sqrt(t1.real * t1.real + t1.imag * t1.imag);
			 /* distance estimate */
			double dist = 0.5 * mt * log(mt) /
				sqrt(dt.real * dt.real + dt.imag * dt.imag);
			rep = (int) 1 + 10*r*dist/demrange; /* scale for viewing */
			if(rep > r-1) rep = r-1; /* chop into color range */
		} else {
			if(binary && t.imag > 0)
				rep = (r + rep / 2) % r; /* binary decomp */
		}
		return rep;
	} else
		return r;
}

static void
Select(
	/* input variables first */
	complex *extreme_ul,complex *extreme_lr,
	int width,int height,
	int power,int top,
	/* output variables follow */
	complex *selected_ul,complex *selected_lr)
	{
	double precision;
	double s2;
	int inside;
	int uninteresting;
	int found;
	int tries;
	found = 0;
	while (!found)
		{
		/* select a precision - be careful with this */
		precision = pow(2.0,FLOATRAND(-9.0,-18.0));
		/* (void) printf("precision is %f\n",precision); */
		for (tries=0;tries<10000&&!found;tries++)
			{
			/* it eventually turned out that this inner loop doesn't always
			** terminate - so we just try 10000 times, and if we don't get
			** anything interesting, we pick a new precision
			*/
			complex temp;
			int sample_step = 4;
			int row,column;
			inside = 0;
			uninteresting = 0;
			/* pick a random point in the allowable range */
			temp.real = FLOATRAND(extreme_ul->real,extreme_lr->real);
			temp.imag = FLOATRAND(extreme_ul->imag,extreme_lr->imag);
			/* find upper left and lower right points */
			selected_ul->real = temp.real - precision * width / 2;
			selected_lr->real = temp.real + precision * width / 2;
			selected_ul->imag = temp.imag - precision * height / 2;
			selected_lr->imag = temp.imag + precision * height / 2;
			/* sample the results we'd get from this choice, accept or reject
			** accordingly
			*/
			for (row=0; row<sample_step; row++)
				{
				for (column=0; column<sample_step; column++)
					{
					int r;
					temp.imag = selected_ul->imag +
						(selected_ul->imag - selected_lr->imag) * 
						(((double)row)/sample_step);
					temp.real = selected_ul->real + 
						(selected_ul->real - selected_lr->real) * 
						(((double)column)/sample_step);
					r = reps(temp,power,top,0,0);
					/* Here, we just want to see if the point is in the set,
					** not if we can make something pretty
					*/
					if (r == top)
						{
						inside++;
						}
					if (r < 2)
						{
						uninteresting++;
						}
					}
				}
			s2 = sample_step*sample_step;
			/* more than 10 percent, but less than 60 percent inside the set */
			if (inside >= ceil(s2/10.0) && inside <= s2*6.0/10.0 &&
				uninteresting <= s2/10.0)
				{
				/* this one looks interesting */
				found = 1;
				}
			/* else
			*** this does not look like a real good combination, so back
			*** up to the top of the loop to try another possibility
			 */
			}
		}
	}

void
init_mandelbrot(ModeInfo * mi)
{
	mandelstruct *mp;
	int         preserve;
	Bool        truecolor;
	unsigned long redmask, greenmask, bluemask;
	Window      window = MI_WINDOW(mi);
	Display    *display = MI_DISPLAY(mi);

	if (mandels == NULL) {
		if ((mandels = (mandelstruct *) calloc(MI_NUM_SCREENS(mi),
					     sizeof (mandelstruct))) == NULL)
			return;
	}
	mp = &mandels[MI_SCREEN(mi)];


	mp->power = NRAND(3) + MINPOWER;

	mp->column = 0;
	mp->counter = 0;

	MI_CLEARWINDOW(mi);

	mp->screen_width = MI_WIDTH(mi);
	mp->screen_height = MI_HEIGHT(mi);

	if (MI_IS_FULLRANDOM(mi)) {
	  switch(NRAND(3)){
	  case 0: mp->binary = 1; mp->dem = 0; break;
	  case 1: mp->binary = 0; mp->dem = 1; break;
	  default:mp->binary = 0; mp->dem = 0; break;
	  }
	} else {
	  mp->binary = binary;
	  mp->dem = dem;
	}

	mp->reptop = 300;

	/* these could be tuned a little closer, but the selection
	** process throws out the chaf anyway, it just takes slightly
	** longer
	*/
	mp->extreme_ul.real = -3.0;
	mp->extreme_ul.imag = -3.0;
	mp->extreme_lr.real = 3.0;
	mp->extreme_lr.imag = 3.0;

#ifndef STANDALONE
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
#define DENOM 4
		/* color in the bottom part */
		mp->bottom.red = NRAND(RANGE / DENOM);
		mp->bottom.blue = NRAND(RANGE / DENOM);
		mp->bottom.green = NRAND(RANGE / DENOM);
		/* color in the top part */
		mp->top.red = RANGE - NRAND(((DENOM-1)*RANGE) / DENOM);
		mp->top.blue = RANGE - NRAND(((DENOM-1)*RANGE) / DENOM);
		mp->top.green = RANGE - NRAND(((DENOM-1)*RANGE) / DENOM);

		/* allocate colormap, if needed */
		if (mp->colors == NULL)
			mp->colors = (XColor *) malloc(sizeof (XColor) * mp->ncolors);
		if (mp->cmap == None)
			mp->cmap = XCreateColormap(display, window,
						   MI_VISUAL(mi), AllocAll);
		mp->usable_colors = mp->ncolors - preserve;
	}
	Select(&mp->extreme_ul,&mp->extreme_lr,
		mp->screen_width,mp->screen_height,
		mp->power,mp->reptop,
		&mp->ul,&mp->lr);

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
draw_mandelbrot(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	mandelstruct *mp = &mandels[MI_SCREEN(mi)];
	int         h;
	complex     c;
#if defined(USE_LOG)
	double      log_top;
#endif

	double      demrange;
	unsigned int i;
	int         j, k;

#if defined(USE_LOG)
	log_top = log((float) mp->reptop);
#endif

	MI_IS_DRAWN(mi) = True;

#ifndef STANDALONE
	if (mp->counter % 15 == 0) {
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
#if defined(OLD_COLOR)
					/* this is known to work well enough. I just wanted
					** to explore some other possibilities
					*/
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
#else
					mp->colors[i].pixel = i;
					mp->colors[i].red = pick_rgb(mp->bottom.red,mp->top.red,
						j,mp->ncolors);
					mp->colors[i].green = pick_rgb(mp->bottom.green,mp->top.green,
						j,mp->ncolors);
					mp->colors[i].blue = pick_rgb(mp->bottom.blue,mp->top.blue,
						j,mp->ncolors);
					mp->colors[i].flags = DoRed | DoGreen | DoBlue;
					j = (j + 1) % mp->ncolors;
					k++;
#endif
				}
			}
			/* make the entire mandelbrot move forward */
			XStoreColors(display, mp->cmap, mp->colors, mp->ncolors);
			setColormap(display, window, mp->cmap, MI_IS_INWINDOW(mi));
		}
	}
#endif /* !STANDALONE */
	/* so we iterate columns beyond the width of the physical screen, so that
	** we just wait around and show what we've done
	*/
	if (mp->column >= 3 * mp->screen_width) {
		/* reset to left edge of screen, bump power */
		mp->column = 0;
		mp->power = NRAND(3) + MINPOWER;
		/* select a new region! */
		Select(&mp->extreme_ul,&mp->extreme_lr,
			mp->screen_width,mp->screen_height,
			mp->power,mp->reptop,
			&mp->ul,&mp->lr);
	} else if (mp->column >= mp->screen_width) {
		/* delay a while */
		mp->column++;
		mp->counter++;
		return;
	}
	/* demrange is used to give some idea of scale */
	demrange = mp->dem ? fabs(mp->ul.real - mp->lr.real) / 2 : 0;
	for (h = 0; h < mp->screen_height; h++) {
		unsigned int color;
		int         result;

		/* c.real = 1.3 - (double) mp->column / mp->screen_width * 3.4; */
		/* c.imag = -1.6 + (double) h / mp->screen_height * 3.2; */
		c.real = mp->ul.real + 
			(mp->ul.real-mp->lr.real)*(((double)(mp->column))/mp->screen_width);
		c.imag = mp->ul.imag + 
			(mp->ul.imag - mp->lr.imag)*(((double) h) / mp->screen_height);
		result = reps(c, mp->power, mp->reptop, mp->binary, demrange);
		if (result == 0 || result == mp->reptop)
			XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		else {
#if defined(USE_LOG)
			color=(unsigned int) (MI_NPIXELS(mi) * log((float) result) / log_top);
#else
			color=(unsigned int) ((MI_NPIXELS(mi) * (float)result) / mp->reptop);
#endif
#ifndef STANDALONE
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
#endif /* !STANDALONE */
		}
		/* we no longer have vertical symmetry - so we compute all points
		** and don't draw with redundancy
		*/
		XDrawPoint(display, window, gc, mp->column, h);
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

#endif /* MODE_mandelbrot */
