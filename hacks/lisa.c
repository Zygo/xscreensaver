/* -*- Mode: C; tab-width: 4 -*- */
/* lisa --- animated full-loop lissajous figures */

#if 0
static const char sccsid[] = "@(#)lisa.c	5.00 2000/11/01 xlockmore";
#endif

/*-
 * Copyright (c) 1997, 2006 by Caleb Cullen.
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
 * 23-Feb-2006: fixed color-cycling issues
 * 01-Nov-2000: Allocation checks
 * 10-May-1997: Compatible with xscreensaver
 *
 * The inspiration for this program, Lasp, was written by Adam B. Roach
 * in 1990, assisted by me, Caleb Cullen.  It was written first in C, then
 * in assembly, and used pre-calculated data tables to graph lissajous
 * figures on 386 machines and lower.  This version bears only superficial
 * resemblances to the original Lasp.
 *
 * The `lissie' module's source code was studied as an example of how
 * to incorporate a new module into xlock.  Resemblances to it are
 * expected, but not intended to be plaigiaristic.
 *
 * February, 2006: 21st Century Update for Lisa
 * + fixed color-mapping: the 'beginning' of the loop always uses the
 *     same (starting) pixel value, causing the loop's coloration to
 *     appear solid rather than flickering as in the previous version
 * + all lines/points in a single color are drawn at once using XDrawLines()
 *     or XDrawPoints(); the artifacting evident in the previous version
 *     has been masked by the use of CapNotLast to separate individual drawn
 *     areas with intentional "whitespace" (typically black)
 * + added many new elements to the Function[] array
 * + randomized selection of next function
 * + introduced concept of "rarely-chosen" functions
 * + cleaned up code somewhat, standardized capitalization, commented all
 *     #directives with block labels
 */

#ifdef STANDALONE
# define MODE_lisa
# define DEFAULTS	"*delay: 17000 \n" \
					"*count: 1 \n" \
					"*cycles: 768 \n" \
					"*size: 500 \n" \
					"*ncolors: 64 \n" \
					"*fpsSolid: true \n" \

# define UNIFORM_COLORS
# define reshape_lisa 0
# define lisa_handle_event 0
# include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"          /* in xlockmore distribution */

#endif /* STANDALONE */

#ifdef MODE_lisa

#define  DEF_ADDITIVE     "True"

static Bool additive;

static XrmOptionDescRec opts[] =
{
	{"-additive", ".lisa.additive", XrmoptionNoArg, "True"},
	{"+additive", ".lisa.additive", XrmoptionNoArg, "False"}
};

static argtype vars[] =
{
	{&additive, "additive", "Additive", DEF_ADDITIVE, t_Bool}
};

static OptionStruct desc[] =
{
	{"-/+additive", "turn on/off additive functions mode"}
};

ENTRYPOINT ModeSpecOpt lisa_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   lisa_description =
{"lisa", "init_lisa", "draw_lisa", "release_lisa",
 "refresh_lisa", "change_lisa", (char *) NULL, &lisa_opts,
 17000, 1, 768, -1, 64, 1.0, "",
 "Shows animated lissajous figures", 0, NULL};

#endif

#define  DRAWLINES    1
/* #define  FOLLOW_FUNC_ORDER 1 */
#define  TWOLOOPS     1
#define  XVMAX        10	/* Maximum velocities */
#define  YVMAX        10
#define  LISAMAXFUNCS 2
#define  NUMSTDFUNCS  28
#define  RAREFUNCMIN  25
#define  RAREFUNCODDS 4     /* 1:n chance a rare function will be re-randomized */
#define  MAXCYCLES    3
#define  MINLISAS     1
#define  STARTCOLOR   0
#define  STARTFUNC    24    /* if negative, is upper-bound on randomization */
#define  LINEWIDTH    -8    /* if negative, is upper-bound on randomization */
#define  LINESTYLE    LineSolid  /* an insane man might have fun with this :) */
#define  LINECAP      CapNotLast /* anything else looks pretty crappy */
#define  LINEJOIN     JoinBevel  /* this ought to be fastest */
#define  SET_COLOR() \
         if (MI_NPIXELS(mi) > 2) { \
           XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_PIXEL(mi, loop->color)); \
         if (loop->cstep \
             && pctr % loop->cstep == 0 \
             && ++(loop->color) >= (unsigned) MI_NPIXELS(mi)) \
               { loop->color=STARTCOLOR; } \
         } else { XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_WHITE_PIXEL(mi)); } 
#define  GET_RADIUS(context) \
         ((context->width > context->height)?context->height:context->width) * 3 / 8
#define  CHECK_RADIUS(loop, context) \
         if ((context->height / 2 > MI_SIZE(mi)) && (context->width / 2 > MI_SIZE(mi))) \
           loop->radius = MI_SIZE(mi); \
         if ((loop->radius < 0) || \
           (loop->radius > loop->center.x) || \
           (loop->radius > loop->center.y)) loop->radius = GET_RADIUS(context)
#define  PRINT_FUNC(funcptr) \
		 printf("new function -- #%d:\n\tx = sin(%gs) * sin(%gs)\n\ty = sin(%gt) * sin(%gt)\n", \
			    funcptr->index, \
			    funcptr->xcoeff[0], funcptr->xcoeff[1], \
			    funcptr->ycoeff[0], funcptr->ycoeff[1])


typedef struct lisafunc_struct {
	double      xcoeff[2], ycoeff[2];
	int         nx, ny;
	int         index;
} lisafuncs;

typedef struct lisa_struct {
    unsigned long color;
    int         radius, dx, dy, nsteps, nfuncs, melting, cstep;
    double      pistep, phi, theta;
    XPoint      center, *lastpoint;
    lisafuncs  *function[LISAMAXFUNCS];
    int         linewidth;
} lisas;

typedef struct lisacontext_struct {
	lisas      *lissajous;
	int         width, height, nlissajous, loopcount;
	int         maxcycles;
	Bool        painted;
} lisacons;

static lisacons *Lisa = (lisacons *) NULL;

static lisafuncs Function[NUMSTDFUNCS] =
  {
	{
	  {1.0, 2.0},
	  {1.0, 2.0}, 2, 2, 0},
	{
	  {1.0, 2.0},
	  {1.0, 1.0}, 2, 2, 1},
	{
	  {1.0, 3.0},
	  {1.0, 2.0}, 2, 2, 2},
	{
	  {1.0, 3.0},
	  {1.0, 3.0}, 2, 2, 3},
	{
	  {2.0, 4.0},
	  {1.0, 2.0}, 2, 2, 4},
	{
	  {1.0, 4.0},
	  {1.0, 3.0}, 2, 2, 5},
	{
	  {1.0, 4.0},
	  {1.0, 4.0}, 2, 2, 6},
	{
	  {1.0, 5.0},
	  {1.0, 5.0}, 2, 2, 7},
	{
	  {2.0, 5.0},
	  {2.0, 5.0}, 2, 2, 8},
	{
	  {1.0, 2.0},
	  {2.0, 5.0}, 2, 2, 9},
	{
	  {1.0, 2.0},
	  {3.0, 5.0}, 2, 2, 10},
	{
	  {1.0, 2.0},
	  {2.0, 3.0}, 2, 2, 11},
	{
	  {1.0, 3.0},
	  {2.0, 3.0}, 2, 2, 12},
	{
	  {2.0, 3.0},
	  {1.0, 3.0}, 2, 2, 13},
	{
	  {2.0, 4.0},
	  {1.0, 3.0}, 2, 2, 14},
	{
	  {1.0, 4.0},
	  {2.0, 3.0}, 2, 2, 15},
	{
	  {2.0, 4.0},
	  {2.0, 3.0}, 2, 2, 16},
	{
	  {1.0, 5.0},
	  {2.0, 3.0}, 2, 2, 17},
	{
	  {2.0, 5.0},
	  {2.0, 3.0}, 2, 2, 18},	
	{
	  {1.0, 5.0},
	  {2.0, 5.0}, 2, 2, 19},
	{
	  {1.0, 3.0},
	  {2.0, 7.0}, 2, 2, 20},
	{
	  {2.0, 3.0},
	  {5.0, 7.0}, 2, 2, 21},
	{
	  {1.0, 2.0},
	  {3.0, 7.0}, 2, 2, 22},
	{
	  {2.0, 5.0},
	  {5.0, 7.0}, 2, 2, 23},
	{
	  {5.0, 7.0},
	  {5.0, 7.0}, 2, 2, 24},
	{                          /* functions past here are 'rare' and won't  */
	  {2.0, 7.0},              /* show up as often.  tweak the #defines above */
	  {1.0, 7.0}, 2, 2, 25},   /* to see them more frequently */
	{
	  {2.0, 9.0},
	  {1.0, 7.0}, 2, 2, 26},
	{
	  {5.0, 11.0},
	  {2.0, 9.0}, 2, 2, 27}
};

int xMaxLines;

static void
free_lisa(lisacons *lc)
{
	while (lc->lissajous) {
		int    lctr;

		for (lctr = 0; lctr < lc->nlissajous; lctr++) {
			(void) free((void *) lc->lissajous[lctr].lastpoint);
		}
		(void) free((void *) lc->lissajous);
		lc->lissajous = (lisas *) NULL;
	}
}

static Bool
drawlisa(ModeInfo * mi, lisas * loop)
{
	XPoint     *np;
	XPoint     *lp = loop->lastpoint;
	lisacons   *lc = &Lisa[MI_SCREEN(mi)];
	lisafuncs **lf = loop->function;
	int         phase = lc->loopcount % loop->nsteps;
	int         pctr, fctr, xctr, yctr, extra_points;
	double      xprod, yprod, xsum, ysum;

    /* why carry this around in the struct when we can calculate it on demand? */
    extra_points = loop->cstep - (loop->nsteps % loop->cstep);

	/* Allocate the np (new point) array (with padding) */
	if ((np = (XPoint *) calloc(loop->nsteps+extra_points, sizeof (XPoint))) == NULL) {
		free_lisa(lc);
		return False;
	}

	/* Update the center */
	loop->center.x += loop->dx;
	loop->center.y += loop->dy;
	CHECK_RADIUS(loop, lc);

  /* check for overlaps -- where the figure might go off the screen */

	if ((loop->center.x - loop->radius) <= 0) {
		loop->center.x = loop->radius;
		loop->dx = NRAND(XVMAX);
	} else if ((loop->center.x + loop->radius) >= lc->width) {
		loop->center.x = lc->width - loop->radius;
		loop->dx = -NRAND(XVMAX);
	};
	if ((loop->center.y - loop->radius) <= 0) {
		loop->center.y = loop->radius;
		loop->dy = NRAND(YVMAX);
	} else if ((loop->center.y + loop->radius) >= lc->height) {
		loop->center.y = lc->height - loop->radius;
		loop->dy = -NRAND(YVMAX);
	};

	/* Now draw the points, and erase the ones from the last cycle */

	for (pctr = 0; pctr < loop->nsteps; pctr++) {
		fctr = loop->nfuncs;
		loop->phi = (double) (pctr - phase) * loop->pistep;
		loop->theta = (double) (pctr + phase) * loop->pistep;
		xsum = ysum = 0;
		while (fctr--) {
			xctr = lf[fctr]->nx;
			yctr = lf[fctr]->ny;
			if (additive) {
				xprod = yprod = 0.0;
				while (xctr--)
					xprod += sin(lf[fctr]->xcoeff[xctr] * loop->theta);
				while (yctr--)
					yprod += sin(lf[fctr]->ycoeff[yctr] * loop->phi);
				if (loop->melting) {
					if (fctr) {
						xsum += xprod * (double) (loop->nsteps - loop->melting) /
							(double) loop->nsteps;
						ysum += yprod * (double) (loop->nsteps - loop->melting) /
							(double) loop->nsteps;
					} else {
						xsum += xprod * (double) loop->melting / (double) loop->nsteps;
						ysum += yprod * (double) loop->melting / (double) loop->nsteps;
					}
				} else {
					xsum = xprod;
					ysum = yprod;
				}
				if (!fctr) {
					xsum = xsum * (double) loop->radius / (double) lf[fctr]->nx;
					ysum = ysum * (double) loop->radius / (double) lf[fctr]->ny;
				}
			} else {
				if (loop->melting) {
					if (fctr) {
						yprod = xprod = (double) loop->radius *
							(double) (loop->nsteps - loop->melting) /
							(double) (loop->nsteps);
					} else {
						yprod = xprod = (double) loop->radius *
							(double) (loop->melting) / (double) (loop->nsteps);
					}
				} else {
					xprod = yprod = (double) loop->radius;
				}
				while (xctr--)
					xprod *= sin(lf[fctr]->xcoeff[xctr] * loop->theta);
				while (yctr--)
					yprod *= sin(lf[fctr]->ycoeff[yctr] * loop->phi);
				xsum += xprod;
				ysum += yprod;
			}
		}
		if ((loop->nfuncs > 1) && (!loop->melting)) {
			xsum /= (double) loop->nfuncs;
			ysum /= (double) loop->nfuncs;
		}
		xsum += (double) loop->center.x;
		ysum += (double) loop->center.y;

		np[pctr].x = (int) ceil(xsum);
		np[pctr].y = (int) ceil(ysum);
	}
    /* fill in extra points */
    for (pctr=loop->nsteps; pctr < loop->nsteps+extra_points; pctr++) {
  	  np[pctr].x = np[pctr - loop->nsteps].x;
  	  np[pctr].y = np[pctr - loop->nsteps].y;
    }
	if (loop->melting) {
		if (!--loop->melting) {
			loop->nfuncs = 1;
			loop->function[0] = loop->function[1];
		}
	}

    /* reset starting color each time to prevent ass-like appearance */	
    loop->color = STARTCOLOR;

  if (loop->cstep < xMaxLines) {
	/*	printf("Drawing dashes\n"); */
	for (pctr = 0; pctr < loop->nsteps; pctr+=loop->cstep) {
#if defined DRAWLINES
		XSetLineAttributes(MI_DISPLAY(mi), MI_GC(mi), loop->linewidth,
				   LINESTYLE, LINECAP, LINEJOIN);
		/* erase the last cycle's point */
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_BLACK_PIXEL(mi));
		XDrawLines(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi), &lp[pctr], 
				   loop->cstep, CoordModeOrigin);

		/* Set the new color */
		SET_COLOR();

		/* plot this cycle's point */
		XDrawLines(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi), 
				   &np[pctr], loop->cstep, CoordModeOrigin);
		XSetLineAttributes(MI_DISPLAY(mi), MI_GC(mi), 1,
						   LINESTYLE, LINECAP, LINEJOIN);
#else
		/* erase the last cycle's point */
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_BLACK_PIXEL(mi));
		XDrawPoints(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi),
					&lp[pctr], loop->cstep, CoordModeOrigin);

		/* Set the new color */
		SET_COLOR();

		/* plot this cycle's point */
		XDrawPoints(MI_DISPLAY(mi), MI_WINDOW(mi),
					MI_GC(mi), &np[pctr], loop->cstep, CoordModeOrigin);
#endif
	}
  } else { /* on my system, cstep is larger than 65532/2 if we get here */
	for (pctr = 0; pctr < loop->nsteps; pctr++) {
#if defined DRAWLINES
	  XSetLineAttributes(MI_DISPLAY(mi), MI_GC(mi), loop->linewidth,
						 LINESTYLE, LINECAP, LINEJOIN);
	  /* erase the last cycle's point */
	  XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_BLACK_PIXEL(mi));
	  XDrawLine(MI_DISPLAY(mi), MI_WINDOW(mi),
				MI_GC(mi), lp[pctr].x, lp[pctr].y,
				lp[(pctr + 1) % loop->nsteps].x,
				lp[(pctr + 1) % loop->nsteps].y);
	  
	  /* Set the new color */
	  SET_COLOR();
	  
	  /* plot this cycle's point */
	  XDrawLine(MI_DISPLAY(mi), MI_WINDOW(mi),
				MI_GC(mi), np[pctr].x, np[pctr].y,
				np[(pctr + 1) % loop->nsteps].x,
				np[(pctr + 1) % loop->nsteps].y);
	  XSetLineAttributes(MI_DISPLAY(mi), MI_GC(mi), 1,
						 LINESTYLE, LINECAP, LINEJOIN);
#else  /* DRAWLINES */
	  /* erase the last cycle's point */
	  XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_BLACK_PIXEL(mi));
	  XDrawPoint(MI_DISPLAY(mi), MI_WINDOW(mi),
				 MI_GC(mi), lp[pctr].x, lp[pctr].y);
	  
	  /* Set the new color */
	  SET_COLOR();
	  
	  /* plot this cycle's point */
	  XDrawPoint(MI_DISPLAY(mi), MI_WINDOW(mi),
				 MI_GC(mi), np[pctr].x, np[pctr].y);
#endif  /* DRAWLINES */
	}
  }
	(void) free((void *) lp);
	loop->lastpoint = np;
	return True;
}

static Bool 
initlisa(ModeInfo * mi, lisas * loop)
{
  lisacons   *lc = &Lisa[MI_SCREEN(mi)];
  lisafuncs **lf = loop->function;
  XPoint     *lp;
  int         phase, pctr, fctr, xctr, yctr, extra_points;
  double      xprod, yprod, xsum, ysum;
  
  xMaxLines = (XMaxRequestSize(MI_DISPLAY(mi))-3)/2;
  /*  printf("Got xMaxLines = %d\n", xMaxLines); */
  loop->nsteps = MI_CYCLES(mi);
  if (loop->nsteps == 0)
	loop->nsteps = 1;
  if (MI_NPIXELS(mi) > 2) {
	loop->color = STARTCOLOR;
	loop->cstep = (loop->nsteps > MI_NPIXELS(mi)) ? loop->nsteps / MI_NPIXELS(mi) : 1;
  } else {
	loop->color = MI_WHITE_PIXEL(mi);
	loop->cstep = 0;
  }
  extra_points = loop->cstep - (loop->nsteps % loop->cstep);
  lc->maxcycles = (MAXCYCLES * loop->nsteps) - 1;
  loop->cstep = ( loop->nsteps > MI_NPIXELS(mi) ) ? loop->nsteps / MI_NPIXELS(mi) : 1;
  /*  printf("Got cstep = %d\n", loop->cstep); */
  loop->melting = 0;
  loop->nfuncs = 1;
  loop->pistep = 2.0 * M_PI / (double) loop->nsteps;
  loop->center.x = lc->width / 2;
  loop->center.y = lc->height / 2;
  loop->radius = (int) MI_SIZE(mi);
  CHECK_RADIUS(loop, lc);
  loop->dx = NRAND(XVMAX);
  loop->dy = NRAND(YVMAX);
  loop->dx++;
  loop->dy++;
#if defined STARTFUNC
  lf[0] = &Function[STARTFUNC];
#else  /* STARTFUNC */
  lf[0] = &Function[NRAND(NUMSTDFUNCS)];
#endif  /* STARTFUNC */	
  
  if ((lp = loop->lastpoint = (XPoint *)
	   calloc(loop->nsteps+extra_points, sizeof (XPoint))) == NULL) {
	free_lisa(lc);
	return False;
  }
  phase = lc->loopcount % loop->nsteps;
  
#if defined DEBUG
  printf( "nsteps = %d\tcstep = %d\tmrs = %d\textra_points = %d\n", 
		  loop->nsteps, loop->cstep, xMaxLines, extra_points );
  PRINT_FUNC(lf[0]);
#endif  /* DEBUG */
  
  for (pctr = 0; pctr < loop->nsteps; pctr++) {
	loop->phi = (double) (pctr - phase) * loop->pistep;
	loop->theta = (double) (pctr + phase) * loop->pistep;
	fctr = loop->nfuncs;
	xsum = ysum = 0.0;
	while (fctr--) {
	  xprod = yprod = (double) loop->radius;
	  xctr = lf[fctr]->nx;
	  yctr = lf[fctr]->ny;
	  while (xctr--)
		xprod *= sin(lf[fctr]->xcoeff[xctr] * loop->theta);
	  while (yctr--)
		yprod *= sin(lf[fctr]->ycoeff[yctr] * loop->phi);
	  xsum += xprod;
	  ysum += yprod;
	}
	if (loop->nfuncs > 1) {
	  xsum /= 2.0;
	  ysum /= 2.0;
	}
	xsum += (double) loop->center.x;
	ysum += (double) loop->center.y;
	
	lp[pctr].x = (int) ceil(xsum);
	lp[pctr].y = (int) ceil(ysum);
  }
  /* this fills in the extra points, so we can use segment-drawing calls */
  for (pctr = loop->nsteps; pctr < loop->nsteps + extra_points; pctr++) {
	lp[pctr].x=lp[pctr - loop->nsteps].x;
	lp[pctr].y=lp[pctr - loop->nsteps].y;
  }
#if defined DRAWLINES
  loop->linewidth = LINEWIDTH;	/* #### make this a resource */
  
  if (loop->linewidth == 0)
	loop->linewidth = 1;
  if (loop->linewidth < 0)
	loop->linewidth = NRAND(-loop->linewidth) + 1;
  XSetLineAttributes(MI_DISPLAY(mi), MI_GC(mi), loop->linewidth,
					 LINESTYLE, LINECAP, LINEJOIN);
#endif  /* DRAWLINES */
  
  if ( loop->cstep < xMaxLines ) { 
	/* we can send each color segment in a single request 
	 * because the max request length is long enough 
	 * and because we have padded out the array to have extra elements 
	 * to support calls which would otherwise fall off the end*/
	for (pctr = 0; pctr < loop->nsteps; pctr+=loop->cstep) {
	  /* Set the color */
	  SET_COLOR();
	  
#if defined DRAWLINES
	  XDrawLines(MI_DISPLAY(mi), MI_WINDOW(mi),
				 MI_GC(mi), &lp[pctr], loop->cstep, CoordModeOrigin );
#else  /* DRAWLINES */
	  XDrawPoints(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi),
				  &lp[pctr], loop->cstep, CoordModeOrigin );
#endif  /* DRAWLINES */
	}
  } else { /* do it one by one as before */
	for (pctr = 0; pctr < loop->nsteps; pctr++ ) {
	  SET_COLOR();
	  
#if defined DRAWLINES
	  XDrawLine(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi),
				lp[pctr].x, lp[pctr].y,
				lp[pctr+1 % loop->nsteps].x, lp[pctr+1 % loop->nsteps].y);
#else  /* DRAWLINES */
	  XDrawPoint(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi),
				 lp[pctr].x, lp[pctr].y);
#endif  /* DRAWLINES */
	}
  }
  
#if defined DRAWLINES
  XSetLineAttributes(MI_DISPLAY(mi), MI_GC(mi), 1,
					 LINESTYLE, LINECAP, LINEJOIN);
#endif  /* DRAWLINES */
  return True;
}

static void
refreshlisa(ModeInfo * mi)
{
	lisacons   *lc = &Lisa[MI_SCREEN(mi)];
	int         lctr;

	for (lctr = 0; lctr < lc->nlissajous; lctr++) {
		if (!drawlisa(mi, &lc->lissajous[lctr]))
			return;
	}
}

ENTRYPOINT void
refresh_lisa(ModeInfo * mi)
{
	lisacons   *lc;

	if (Lisa == NULL)
		return;
	lc = &Lisa[MI_SCREEN(mi)];
	if (lc->lissajous == NULL)
		return;

	if (lc->painted) {
		lc->painted = False;
		MI_CLEARWINDOW(mi);
		refreshlisa(mi);
	}
}

static void
change_lisa(ModeInfo * mi)
{
	lisas      *loop;
	int         lctr, newfunc;
	lisacons   *lc;

	if (Lisa == NULL)
		return;
	lc = &Lisa[MI_SCREEN(mi)];
	if (lc->lissajous == NULL)
		return;

	lc->loopcount = 0;
	for (lctr = 0; lctr < lc->nlissajous; lctr++) {
		loop = &lc->lissajous[lctr];       /* count through the loops we're drawing */
	newfunc = NRAND(NUMSTDFUNCS);     /* choose a new function at random */
#if defined FOLLOW_FUNC_ORDER
	loop->function[1] =
	  &Function[(loop->function[0]->index + 1) % NUMSTDFUNCS];
#else  /* FOLLOW_FUNC_ORDER */
	if (newfunc == loop->function[0]->index) {
	  ++newfunc;
	  newfunc %= NUMSTDFUNCS;       /* take the next if we got the one we have */
	}
	if (newfunc >= RAREFUNCMIN \
		&& !(random() % RAREFUNCODDS) \
		&& (newfunc = NRAND(NUMSTDFUNCS)) == loop->function[0]->index) {
	  ++newfunc;
	  newfunc %= NUMSTDFUNCS;
	}
	loop->function[1] =               /* set 2nd function pointer on the loop */
	  &Function[newfunc];             /* to the new function we just chose */
#endif  /* FOLLOW_FUNC_ORDER */		
#if defined DEBUG
	PRINT_FUNC(loop->function[1]);
#endif /* DEBUG */
	loop->melting = loop->nsteps - 1; /* melt the two functions together */ 
	loop->nfuncs = 2;                 /* simultaneously for a full cycle */  
  }
}

ENTRYPOINT void
init_lisa (ModeInfo * mi)
{
	int         lctr;
	lisacons   *lc;

	if (Lisa == NULL) {
		if ((Lisa = (lisacons *) calloc(MI_NUM_SCREENS(mi),
				 sizeof (lisacons))) == NULL)
			return;
	}
	lc = &Lisa[MI_SCREEN(mi)];
	lc->width = MI_WIDTH(mi);
	lc->height = MI_HEIGHT(mi);
	lc->loopcount = 0;
	lc->nlissajous = MI_COUNT(mi);
	if (lc->nlissajous <= 0)
		lc->nlissajous = 1;
	MI_CLEARWINDOW(mi);
	lc->painted = False;

	if (lc->lissajous == NULL) {
		if ((lc->lissajous = (lisas *) calloc(lc->nlissajous,
				sizeof (lisas))) == NULL)
			return;
		for (lctr = 0; lctr < lc->nlissajous; lctr++) {
			if (!initlisa(mi, &lc->lissajous[lctr]))
				return;
			lc->loopcount++;
		}
	} else {
		refreshlisa(mi);
	}
}

ENTRYPOINT void
draw_lisa (ModeInfo * mi)
{
	lisacons   *lc;

	if (Lisa == NULL)
		return;
	lc = &Lisa[MI_SCREEN(mi)];
	if (lc->lissajous == NULL)
		return;

#ifdef HAVE_JWXYZ	/* Don't second-guess Quartz's double-buffering */
    XClearWindow (MI_DISPLAY(mi), MI_WINDOW(mi));
#endif

	MI_IS_DRAWN(mi) = True;
	lc->painted = True;
	if (++lc->loopcount > lc->maxcycles) {
		change_lisa(mi);
	}
	refreshlisa(mi);
}

ENTRYPOINT void
release_lisa (ModeInfo * mi)
{
	if (Lisa) {
		int    screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_lisa(&Lisa[screen]);
		(void) free(Lisa);
		Lisa = (lisacons *) NULL;
	}
}

XSCREENSAVER_MODULE ("Lisa", lisa)

#endif /* MODE_lisa */
