/* -*- Mode: C; tab-width: 4; c-basic-offset: 4 -*- */
/* mandelbrot --- animated mandelbrot sets */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)mandelbrot.c	5.09 2003/06/30 xlockmore";

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
 *   z^z + z^n + c  <-- pow
 *   sin(z) + z^n + c  <-- sin
 *   sin(z) + e^z + c
 * These were first explored by a colleague of Mandelbrot, Clifford A.
 * Pickover.  These would make nice additions to add.
 *
 * Revision History:
 * 03-Jul-2003: Added pow and sin options
 * 30-Jun-2003: Changed writeable mode to be more consistent with
 *              xscreensaver's starfish
 * 01-Nov-2000: Allocation checks
 * 24-Mar-1999: DEM and Binary decomp options added by Tim Auckland
 *              <tda10.geo@yahoo.com>.  Ideas from Peitgen & Saupe's
 *              "The Science of Fractal Images"
 * 17-Nov-1998: Many Changes by Stromberg, including selection of
 *              interesting subregions, more extreme color ranges,
 *              reduction of possible powers to smaller/more interesting
 *              range, elimination of some unused code, slower color cycling,
 *              longer period of color cycling after the drawing is complete.
 *              Hopefully the longer color cycling period will make the mode
 *              reasonable even after CPUs are so fast that the drawing
 *              interval goes by quickly
 * 20-Oct-1997: Written by Dan Stromberg <strombrg@nis.acs.uci.edu>
 */


#ifdef STANDALONE
#define MODE_mandelbrot
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
#else /* !STANDALONE */
#include "xlock.h"		/* from the xlockmore distribution */
#include "color.h"
#endif /* !STANDALONE */

#ifdef MODE_mandelbrot

#define MINPOWER 2
#define DEF_INCREMENT  "1.00"
#define DEF_BINARY     "False"
#define DEF_DEM        "False"
#define DEF_LYAP       "False"
#define DEF_ALPHA      "False"
#define DEF_INDEX      "False"
#define DEF_POW        "False"
#define DEF_SIN        "False"
#define DEF_CYCLE "True"

/* 4.0 is best for seeing if a point is inside the set, 13 is better if
** you want to get a pretty corona
*/
#define ESCAPE 13.0
#define FLOATRAND(min,max) ((min)+((double) LRAND()/((double) MAXRAND))*((max)-(min)))

typedef enum {
	NONE,
	LYAPUNOV,
	ALPHA,
	INDEX,
	interior_size,
} interior_t;

	/* incr also would be nice as a parameter.  It controls how fast
	   the order is advanced.  Non-integral values are not true orders,
	   but it's a somewhat interesting function anyway
	 */
static float increment;
static Bool  binary_p;
static Bool  dem_p;
static Bool  lyap_p;
static Bool  alpha_p;
static Bool  index_p;
static Bool  pow_p;
static Bool  sin_p;
static Bool  cycle_p;

static XrmOptionDescRec opts[] =
{
  {(char *) "-increment", (char *) ".mandelbrot.increment", XrmoptionSepArg, (caddr_t) NULL},
  {(char *) "-binary", (char *) ".mandelbrot.binary", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+binary", (char *) ".mandelbrot.binary", XrmoptionNoArg, (caddr_t) "off"},
  {(char *) "-dem", (char *) ".mandelbrot.dem", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+dem", (char *) ".mandelbrot.dem", XrmoptionNoArg, (caddr_t) "off"},
  {(char *) "-lyap", (char *) ".mandelbrot.lyap", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+lyap", (char *) ".mandelbrot.lyap", XrmoptionNoArg, (caddr_t) "off"},
  {(char *) "-alpha", (char *) ".mandelbrot.alpha", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+alpha", (char *) ".mandelbrot.alpha", XrmoptionNoArg, (caddr_t) "off"},
  {(char *) "-index", (char *) ".mandelbrot.index", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+index", (char *) ".mandelbrot.index", XrmoptionNoArg, (caddr_t) "off"},
  {(char *) "-pow", (char *) ".mandelbrot.pow", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+pow", (char *) ".mandelbrot.pow", XrmoptionNoArg, (caddr_t) "off"},
  {(char *) "-sin", (char *) ".mandelbrot.sin", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+sin", (char *) ".mandelbrot.sin", XrmoptionNoArg, (caddr_t) "off"},
  {(char *) "-cycle", (char *) ".mandelbrot.cycle", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+cycle", (char *) ".mandelbrot.cycle", XrmoptionNoArg, (caddr_t) "off"}
};

static argtype vars[] =
{
  {(void *) & increment, (char *) "increment", (char *) "Increment", (char *) DEF_INCREMENT, t_Float},
  {(void *) & binary_p, (char *) "binary", (char *) "Binary", (char *) DEF_BINARY, t_Bool},
  {(void *) & dem_p, (char *) "dem", (char *) "Dem", (char *) DEF_DEM, t_Bool},
  {(void *) & lyap_p, (char *) "lyap", (char *) "Lyap", (char *) DEF_LYAP, t_Bool},
  {(void *) & alpha_p, (char *) "alpha", (char *) "Alpha", (char *) DEF_ALPHA, t_Bool},
  {(void *) & index_p, (char *) "index", (char *) "Index", (char *) DEF_INDEX, t_Bool},
  {(void *) & pow_p, (char *) "pow", (char *) "Pow", (char *) DEF_POW, t_Bool},
  {(void *) & sin_p, (char *) "sin", (char *) "Sin", (char *) DEF_SIN, t_Bool},
  {(void *) & cycle_p, (char *) "cycle", (char *) "Cycle", (char *) DEF_CYCLE, t_Bool}
};
static OptionStruct desc[] =
{
  {(char *) "-increment value", (char *) "increasing orders"},
  {(char *) "-/+binary", (char *) "turn on/off Binary Decomposition colour modulation"},
  {(char *) "-/+dem", (char *) "turn on/off Distance Estimator Method (instead of escape time)"},
  {(char *) "-/+lyap", (char *) "render interior with Lyapunov measure"},
  {(char *) "-/+alpha", (char *) "render interior with Alpha level sets"},
  {(char *) "-/+index", (char *) "render interior with Alpha indexes"},
  {(char *) "-/+pow", (char *) "turn on/off adding z^z"},
  {(char *) "-/+sin", (char *) "turn on/off adding sin(z)"},
  {(char *) "-/+cycle", (char *) "turn on/off colour cycling"}
};

ModeSpecOpt mandelbrot_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   mandelbrot_description =
{"mandelbrot", "init_mandelbrot", "draw_mandelbrot", "release_mandelbrot",
 (char *) NULL, "init_mandelbrot", (char *) NULL, &mandelbrot_opts,
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

static void
cln(complex * a)
{
	/* ln(z) = ln ((x^2 + y^2)^(1/2)) + i * invtan(y/x) */
	double      tr, ti;

	tr = sqrt(a->real * a->real + a->imag * a->imag);
	ti = (a->real == 0.0 && a->imag == 0.0) ? 0.0 :
		atan2(a->imag, a->real);
	a->real = tr;
	a->imag = ti;
}

static void
complex_exp(complex * a)
{
	/* e^z = e^x * (cos(y) + i * sin(y)) */
	double      tr;

	tr = exp(a->real);
	a->real = tr * cos(a->imag);
	a->imag = tr * sin(a->imag);
}

static complex
complex_pow(complex a, complex b)
{
	/* a^z = e^(z * ln(a)) */

	complex c, d;

	c = a;
	d = b;
	cln(&c);
	mult(&d, c);
	complex_exp(&d);
	return d;
}

static complex
complex_sin(complex a)
{
	/* sin(z) = -i * (e^(i * z) - e^(-i * z)) / 2 */
	/* cos(z) = (e^(i * z) - e^(-i * z)) / 2 */
	complex c, d, i;

	c = a;
	d = a;
	i.real = 0; i.imag = 1;

	/* inefficient but I just want to see the picture */
	mult(&c, i);
	i.imag = -i.imag;
	mult(&d, i);
	complex_exp(&c);
	complex_exp(&d);
	d.real = -d.real;
	d.imag = -d.imag;
	add(&c, d);
	c.real = c.real / 2;
	c.imag = c.imag / 2;
	mult(&c, i);
	return c;
}

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
	Bool        backwards;
	int         ncolors;
	unsigned int cur_color;
	GC          gc;
	Colormap    cmap;
	unsigned long blackpixel, whitepixel, fg, bg;
	int direction;
	XColor     *colors;
	complex     extreme_ul;
	complex	    extreme_lr;
	complex	    ul;
	complex     lr;
	int         screen_width;
	int         screen_height;
	int         reptop;
	Bool        dem, pow, sin, cycle_p, mono_p, no_colors;
	Bool        binary;
    interior_t  interior;
	ModeInfo   *mi;
} mandelstruct;

static mandelstruct *mandels = (mandelstruct *) NULL;

/* do the iterations
 * if binary is true, check halfplane of last iteration.
 * if demrange is non zero, estimate lower bound of dist(c, M)
 *
 * DEM - Distance Estimator Method
 *      Based on Peitgen & Saupe's "The Science of Fractal Images"
 *
 * ALPHA - level sets of closest return.
 * INDEX - index of ALPHA.
 *      Based on Peitgen & Richter's "The Beauty of Fractals" (Fig 33,34)
 *
 * LYAPUNOV - lyapunov exponent estimate
 *      Based on an idea by Jan Thor
 *
 */
static int
reps(complex c, double p, int r, Bool binary, interior_t interior, double demrange, Bool zpow, Bool zsin)
{
	int         rep;
	int         escaped = 0;
	complex     t;
	int escape = (int) ((demrange == 0) ? ESCAPE :
		ESCAPE*ESCAPE*ESCAPE*ESCAPE); /* 2 more iterations */
	complex     t1;
	complex     dt;
	double      L = 0.0;
	double      l2;
	double      dl2;
	double      alpha2 = ESCAPE;
	int         index = 0;
#if defined(USE_LOG)
	double      log_top = log(r);
#endif

	t = c;
	dt.real = 1; dt.imag = 0;
	for (rep = 0; rep < r; rep++) {
	    t1 = t;
		ipow(&t, (int) p);
		add(&t, c);
		if (zpow)
			add(&t, complex_pow(t1, t1));
		if (zsin)
			add(&t, complex_sin(t1));
		l2 = t.real * t.real + t.imag * t.imag;
		if (l2 <= alpha2) { 
			alpha2 = l2;
			index = rep;
		}
		if (l2 >= escape) {
			escaped = 1;
			break;
			} else if (interior == LYAPUNOV) {
		    /* Crude estimate of Lyapunov exponent.  The stronger the
			   attractor, the more negative the exponent will be. */
			/*                     n=N
			L = lim        1/N *  Sum    log(abs(dx(n+1)/dx(n)))/ln(2)
                   N->inf          n=1     
			*/
		    L += log(sqrt(l2));
		}
		if (demrange){
			/* compute dt/dc
			 *               p-1
			 * dt    =  p * t  * dt + 1
			 *   k+1         k     k
			 */
			/* Note this is incorrect for zpow or zsin, but a correct
			   implementation is too slow to be useful. */
			dt.real *= p; dt.imag *= p;
			if(p > 2) ipow(&t1, (int) (p - 1));
			mult(&dt, t1);
			dt.real += 1;
			dl2 = dt.real * dt.real + dt.imag * dt.imag;			
			if (dl2 >= 1e300) {
				escaped = 2;
				break;
			}
		}
	}
	if (escaped) {
		if(demrange) {
			double mt = sqrt(t1.real * t1.real + t1.imag * t1.imag);
			 /* distance estimate */
			double dist = 0.5 * mt * log(mt) / sqrt(dl2);
			/* scale for viewing.  Allow black when showing interior. */
			rep = (int) (((interior > NONE)?0:1) + 10*r*dist/demrange);
			if(rep > r-1) rep = r-1; /* chop into color range */
		}
		if(binary && t.imag > 0)
		    rep = (r + rep / 2) % r; /* binary decomp */
#ifdef USE_LOG
		if ( rep > 0 )
		    rep = r * log(rep)/log_top; /* Log Scale */
#endif
		return rep;
	} else if (interior == LYAPUNOV) {
		return -(int)(L/M_LN2) % r;
	} else if (interior == INDEX) {
		return 1 + index;
	} else if (interior == ALPHA) {
		return r * sqrt(alpha2);
	} else
		return r;
}

static void
Select(/* input variables first */
  complex *extreme_ul, complex *extreme_lr,
  int width, int height, int power, int top,
  Bool zpow, Bool zsin,
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
	while (!found) {
		/* select a precision - be careful with this */
		precision = pow(2.0,FLOATRAND(-9.0,-18.0));
		/* (void) printf("precision is %f\n",precision); */
		for (tries=0;tries<10000&&!found;tries++) {
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
			for (row=0; row<sample_step; row++) {
				for (column=0; column<sample_step; column++) {
					int r;
					temp.imag = selected_ul->imag +
						(selected_ul->imag - selected_lr->imag) *
						(((double)row)/sample_step);
					temp.real = selected_ul->real +
						(selected_ul->real - selected_lr->real) *
						(((double)column)/sample_step);
					r = reps(temp,(double) power,top,0,0,0.0, zpow, zsin);
					/* Here, we just want to see if the point is in the set,
					** not if we can make something pretty
					*/
					if (r == top) {
						inside++;
					}
					if (r < 2) {
						uninteresting++;
					}
				}
			}
			s2 = sample_step*sample_step;
			/* more than 10 percent, but less than 60 percent inside the set */
			if (inside >= ceil(s2/10.0) && inside <= s2*6.0/10.0 &&
				uninteresting <= s2/10.0) {
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


static void
free_mandelbrot(Display *display, mandelstruct *mp)
{
	ModeInfo *mi = mp->mi;

	if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
		MI_WHITE_PIXEL(mi) = mp->whitepixel;
		MI_BLACK_PIXEL(mi) = mp->blackpixel;
#ifndef STANDALONE
		MI_FG_PIXEL(mi) = mp->fg;
		MI_BG_PIXEL(mi) = mp->bg;
#endif
		if (mp->colors != NULL) {
			if (mp->ncolors && !mp->no_colors)
				free_colors(display, mp->cmap, mp->colors,
					mp->ncolors);
				free(mp->colors);
				mp->colors = (XColor *) NULL;
		}
		if (mp->cmap != None) {
			XFreeColormap(display, mp->cmap);
			mp->cmap = None;
		}
	}
	if (mp->gc != None) {
		XFreeGC(display, mp->gc);
		mp->gc = None;
	}
}

#ifndef STANDALONE
extern char *background;
extern char *foreground;
#endif

void
init_mandelbrot(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	mandelstruct *mp;

	if (mandels == NULL) {
		if ((mandels = (mandelstruct *) calloc(MI_NUM_SCREENS(mi),
					     sizeof (mandelstruct))) == NULL)
			return;
	}
	mp = &mandels[MI_SCREEN(mi)];
	mp->mi = mi;

	mp->screen_width = MI_WIDTH(mi);
	mp->screen_height = MI_HEIGHT(mi);
	mp->backwards = (Bool) (LRAND() & 1);
	if (mp->backwards)
		mp->column = mp->screen_width - 1;
	else
		mp->column = 0;
	mp->power = NRAND(3) + MINPOWER;
	mp->counter = 0;

	MI_CLEARWINDOW(mi);

	if (MI_IS_FULLRANDOM(mi)) {
		mp->binary = (Bool) (LRAND() & 1);
		mp->dem = (Bool) (LRAND() & 1);
		mp->interior = NRAND(interior_size);
#if 0
	/* too slow */
	  mp->pow = (NRAND(5) == 0);
	  mp->sin = (NRAND(5) == 0);
#endif
	} else {
	  mp->binary = binary_p;
	  mp->dem = dem_p;
	  if (index_p) {
		  mp->interior = INDEX;
	  } else if(alpha_p) {
		  mp->interior = ALPHA;
	  } else if(lyap_p) {
		  mp->interior = LYAPUNOV;
	  } else {
		  mp->interior = NONE;
	  }
	  mp->pow = pow_p;
	  mp->sin = sin_p;
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

	if (!mp->gc) {
		if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
			XColor      color;

#ifndef STANDALONE
			mp->fg = MI_FG_PIXEL(mi);
			mp->bg = MI_BG_PIXEL(mi);
#endif
			mp->blackpixel = MI_BLACK_PIXEL(mi);
			mp->whitepixel = MI_WHITE_PIXEL(mi);
			if ((mp->cmap = XCreateColormap(display, window,
					MI_VISUAL(mi), AllocNone)) == None) {
				free_mandelbrot(display, mp);
				return;
			}
			XSetWindowColormap(display, window, mp->cmap);
			(void) XParseColor(display, mp->cmap, "black", &color);
			(void) XAllocColor(display, mp->cmap, &color);
			MI_BLACK_PIXEL(mi) = color.pixel;
			(void) XParseColor(display, mp->cmap, "white", &color);
			(void) XAllocColor(display, mp->cmap, &color);
			MI_WHITE_PIXEL(mi) = color.pixel;
#ifndef STANDALONE
			(void) XParseColor(display, mp->cmap, background, &color);
			(void) XAllocColor(display, mp->cmap, &color);
			MI_BG_PIXEL(mi) = color.pixel;
			(void) XParseColor(display, mp->cmap, foreground, &color);
			(void) XAllocColor(display, mp->cmap, &color);
			MI_FG_PIXEL(mi) = color.pixel;
#endif
			mp->colors = (XColor *) NULL;
			mp->ncolors = 0;
		}
		if ((mp->gc = XCreateGC(display, MI_WINDOW(mi),
			     (unsigned long) 0, (XGCValues *) NULL)) == None) {
			free_mandelbrot(display, mp);
			return;
		}
	}
	MI_CLEARWINDOW(mi);

  /* Set up colour map */
  mp->direction = (LRAND() & 1) ? 1 : -1;
  if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
    if (mp->colors != NULL) {
      if (mp->ncolors && !mp->no_colors)
        free_colors(display, mp->cmap, mp->colors, mp->ncolors);
      free(mp->colors);
      mp->colors = (XColor *) NULL;
    }
    mp->ncolors = MI_NCOLORS(mi);
    if (mp->ncolors < 2)
      mp->ncolors = 2;
    if (mp->ncolors <= 2)
      mp->mono_p = True;
    else
      mp->mono_p = False;

    if (mp->mono_p)
      mp->colors = (XColor *) NULL;
    else
      if ((mp->colors = (XColor *) malloc(sizeof (*mp->colors) *
          (mp->ncolors + 1))) == NULL) {
        free_mandelbrot(display, mp);
        return;
      }
    mp->cycle_p = has_writable_cells(mi);
    if (mp->cycle_p) {
      if (MI_IS_FULLRANDOM(mi)) {
        if (!NRAND(8))
          mp->cycle_p = False;
        else
          mp->cycle_p = True;
      } else {
        mp->cycle_p = cycle_p;
      }
    }
    if (!mp->mono_p) {
      if (!(LRAND() % 10))
        make_random_colormap(
#if STANDALONE
            display, MI_WINDOW(mi),
#else
            mi,
#endif
              mp->cmap, mp->colors, &mp->ncolors,
              True, True, &mp->cycle_p);
      else if (!(LRAND() % 2))
        make_uniform_colormap(
#if STANDALONE
            display, MI_WINDOW(mi),
#else
            mi,
#endif
                  mp->cmap, mp->colors, &mp->ncolors,
                  True, &mp->cycle_p);
      else
        make_smooth_colormap(
#if STANDALONE
            display, MI_WINDOW(mi),
#else
            mi,
#endif
                 mp->cmap, mp->colors, &mp->ncolors,
                 True, &mp->cycle_p);
    }
    XInstallColormap(display, mp->cmap);
    if (mp->ncolors < 2) {
      mp->ncolors = 2;
      mp->no_colors = True;
    } else
      mp->no_colors = False;
    if (mp->ncolors <= 2)
      mp->mono_p = True;

    if (mp->mono_p)
      mp->cycle_p = False;

  }
  if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
                if (mp->mono_p) {
			mp->cur_color = MI_BLACK_PIXEL(mi);
		}
  }
	Select(&mp->extreme_ul,&mp->extreme_lr,
		mp->screen_width,mp->screen_height,
		(int) mp->power,mp->reptop, mp->pow, mp->sin,
		&mp->ul,&mp->lr);
}

void
draw_mandelbrot(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         h;
	complex     c;
	double      demrange;
	mandelstruct *mp;

	if (mandels == NULL)
		return;
	mp = &mandels[MI_SCREEN(mi)];

	MI_IS_DRAWN(mi) = True;
        if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
                if (mp->mono_p) {
                        XSetForeground(display, mp->gc, mp->cur_color);
                } else {
                        mp->cur_color = (mp->cur_color + 1) % mp->ncolors;
                        XSetForeground(display, mp->gc, mp->colors[mp->cur_color].pixel);
                }
        } else {
                if (MI_NPIXELS(mi) > 2)
                        XSetForeground(display, mp->gc, MI_PIXEL(mi, mp->cur_color));
                else if (mp->cur_color)
                        XSetForeground(display, mp->gc, MI_BLACK_PIXEL(mi));
                else
                        XSetForeground(display, mp->gc, MI_WHITE_PIXEL(mi));
                if (++mp->cur_color >= (unsigned int) MI_NPIXELS(mi))
                        mp->cur_color = 0;
        }

  /* Rotate colours */
  if (mp->cycle_p) {
    rotate_colors(display, mp->cmap, mp->colors, mp->ncolors,
      mp->direction);
    if (!(LRAND() % 1000))
      mp->direction = -mp->direction;
  }
	/* so we iterate columns beyond the width of the physical screen, so that
	** we just wait around and show what we've done
	*/
	if ((!mp->backwards && (mp->column >= 3 * mp->screen_width)) ||
	    (mp->backwards && (mp->column < -2 * mp->screen_width))) {
		/* reset to left edge of screen, bump power */
		mp->backwards = (Bool) (LRAND() & 1);
		if (mp->backwards)
			mp->column = mp->screen_width - 1;
		else
			mp->column = 0;
		mp->power = NRAND(3) + MINPOWER;
		/* select a new region! */
		Select(&mp->extreme_ul,&mp->extreme_lr,
			mp->screen_width,mp->screen_height,
			(int) mp->power,mp->reptop, mp->pow, mp->sin,
			&mp->ul,&mp->lr);
	} else if (mp->column >= mp->screen_width || mp->column < 0) {
		/* delay a while */
		if (mp->backwards)
			mp->column--;
		else
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
		result = reps(c, mp->power, mp->reptop, mp->binary, mp->interior, demrange, mp->pow, mp->sin);
		if (result < 0 || result >= mp->reptop)
			XSetForeground(display, mp->gc, MI_BLACK_PIXEL(mi));
		else {
			color=(unsigned int) ((MI_NPIXELS(mi) * (float)result) / mp->reptop);
			XSetForeground(display, mp->gc, MI_PIXEL(mi, color));
		}
		/* we no longer have vertical symmetry - so we compute all points
		** and don't draw with redundancy
		*/
		XDrawPoint(display, window, mp->gc, mp->column, h);
	}
	if (mp->backwards)
		mp->column--;
	else
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

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_mandelbrot(MI_DISPLAY(mi), &mandels[screen]);
		free(mandels);
		mandels = (mandelstruct *) NULL;
	}
}

#endif /* MODE_mandelbrot */
