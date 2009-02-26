/* -*- Mode: C; tab-width: 4 -*- */
/* flow --- flow of strange bees */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)flow.c 4.10 98/04/24 xlockmore";

#endif

/*-
 * Copyright (c) 1996 by Tim Auckland <Tim.Auckland@Sun.COM>
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
 * "flow" shows a variety of continuous phase-space flows around strange
 * attractors.  It includes the well-known Lorentz mask (the "Butterfly"
 * of chaos fame), two forms of Rossler's "Folded Band" and a Poincare'
 * section of the "Bagel".
 *
 * Revision History:
 * 09-Apr-97: Ported to xlockmore-4
 * 18-Jul-96: Adapted from swarm.c Copyright (c) 1991 by Patrick J. Naughton.
 * 31-Aug-90: Adapted from xswarm by Jeff Butterworth. (butterwo@ncsc.org)
 */

#ifdef STANDALONE
# define PROGCLASS	"Flow"
# define HACK_INIT	init_flow
# define HACK_DRAW	draw_flow
# define flow_opts	xlockmore_opts
# define DEFAULTS	"*delay:		1000 \n" \
					"*count:		1024 \n" \
					"*cycles:		3000 \n" \
					"*ncolors:		200 \n"
# define SMOOTH_COLORS
# include "xlockmore.h"		/* in xscreensaver distribution */
# include "erase.h"

#else /* STANDALONE */
# include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

ModeSpecOpt flow_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   flow_description =
{"flow", "init_flow", "draw_flow", "release_flow",
 "refresh_flow", "init_flow", NULL, &flow_opts,
 1000, 1024, 3000, 1, 64, 1.0, "",
 "Shows dynamic strange attractors", 0, NULL};

#endif

#define TIMES	2		/* number of time positions recorded */

typedef struct {
	double      x;
	double      y;
	double      z;
} dvector;

typedef struct {
	double      a, b, c;
} Par;

/* Macros */
#define X(t,b)	(sp->p[(t)*sp->beecount+(b)].x)
#define Y(t,b)	(sp->p[(t)*sp->beecount+(b)].y)
#define Z(t,b)	(sp->p[(t)*sp->beecount+(b)].z)
#define balance_rand(v)	((LRAND()/MAXRAND*(v))-((v)/2))		/* random number around 0 */

typedef struct {
	int         pix;
	int         width;
	int         height;
	int         count;
	double      size;

	int         beecount;	/* number of bees */
	XSegment   *csegs;	/* bee lines */
	int        *cnsegs;
	XSegment   *old_segs;	/* old bee lines */
	double      step;
	dvector     c;		/* centre */
	dvector    *p;		/* bee positions x[time][bee#] */
	double     *t;
	double      theta;
	double      dtheta;
	double      phi;
	double      dphi;
	void        (*ODE) (Par par,
						double *dx, double *dy, double *dz,
						double x,   double y,   double z,
						double t);
	Par         par;
} flowstruct;

static flowstruct *flows = NULL;

static void
Lorentz(Par par,
		double *dx, double *dy, double *dz,
		double x,   double y,   double z,
		double t)
{
	*dx = par.a * (y - x);
	*dy = x * (par.b - z) - y;
	*dz = x * y - par.c * z;
}

static void
Rossler(Par par,
		double *dx, double *dy, double *dz,
		double x,   double y,   double z,
		double t)
{
	*dx = -(y + par.a * z);
	*dy = x + y * par.b;
	*dz = par.c + z * (x - 5.7);
}

static void
RosslerCone(Par par,
			double *dx, double *dy, double *dz,
			double x,   double y,   double z,
			double t)
{
	*dx = -(y + par.a * z);
	*dy = x + y * par.b - z * z * par.c;
	*dz = 0.2 + z * (x - 5.7);
}

static void
Bagel(Par par,
	  double *dx, double *dy, double *dz,
	  double x,   double y,   double z,
	  double t)
{
	*dx = -y + par.b * sin(par.c * t);
	*dy = 0.7 * x + par.a * y * (0.1 - x * x);
	*dz = 0;
}

void
init_flow(ModeInfo * mi)
{
	flowstruct *sp;
	int         b;
	dvector     range;

	if (flows == NULL) {
		if ((flows = (flowstruct *) calloc(MI_NUM_SCREENS(mi),
					       sizeof (flowstruct))) == NULL)
			return;
	}
	sp = &flows[MI_SCREEN(mi)];

	sp->beecount = MI_COUNT(mi);
	if (sp->beecount < 0) {
		/* if sp->beecount is random ... the size can change */
		if (sp->csegs != NULL) {
			(void) free((void *) sp->csegs);
			sp->csegs = NULL;
		}
		if (sp->cnsegs != NULL) {
			(void) free((void *) sp->cnsegs);
			sp->cnsegs = NULL;
		}
		if (sp->old_segs != NULL) {
			(void) free((void *) sp->old_segs);
			sp->old_segs = NULL;
		}
		if (sp->p != NULL) {
			(void) free((void *) sp->p);
			sp->p = NULL;
		}
		if (sp->t != NULL) {
			(void) free((void *) sp->t);
			sp->t = NULL;
		}
		sp->beecount = NRAND(-sp->beecount) + 1;	/* Add 1 so its not too boring */
	}
	sp->count = 0;

	sp->width = MI_WIDTH(mi);
	sp->height = MI_HEIGHT(mi);

	sp->theta = balance_rand(M_PI);
	sp->phi = balance_rand(M_PI);
	sp->dtheta = 0.002;
	sp->dphi = 0.001;
	switch (NRAND(4)) {
		case 0:
			sp->ODE = Lorentz;
			sp->step = 0.02;
			sp->size = 60;
			sp->c.x = 0;
			sp->c.y = 0;
			sp->c.z = 24;
			range.x = 5;
			range.y = 5;
			range.z = 1;
			sp->par.a = 10 + balance_rand(5);
			sp->par.b = 28 + balance_rand(5);
			sp->par.c = 2 + balance_rand(1);
			break;
		case 1:
			sp->ODE = Rossler;
			sp->step = 0.05;
			sp->size = 24;
			sp->c.x = 0;
			sp->c.y = 0;
			sp->c.z = 3;
			range.x = 4;
			range.y = 4;
			range.z = 7;
			sp->par.a = 2 + balance_rand(1);
			sp->par.b = 0.2 + balance_rand(0.1);
			sp->par.c = 0.2 + balance_rand(0.1);
			break;
		case 2:
			sp->ODE = RosslerCone;
			sp->step = 0.05;
			sp->size = 24;
			sp->c.x = 0;
			sp->c.y = 0;
			sp->c.z = 3;
			range.x = 4;
			range.y = 4;
			range.z = 4;
			sp->par.a = 2;
			sp->par.b = 0.2;
			sp->par.c = 0.25 + balance_rand(0.09);
			break;
		case 3:
		default:
			sp->ODE = Bagel;
			sp->step = 0.04;
			sp->size = 2.6;
			sp->c.x = 0 /*-1.0*/ ;
			sp->c.y = 0;
			sp->c.z = 0;
			range.x = 3;
			range.y = 4;
			range.z = 0;
			sp->par.a = 10 + balance_rand(5);
			sp->par.b = 0.35 + balance_rand(0.25);
			sp->par.c = 1.57;
			sp->theta = 0;
			sp->phi = 0;
			sp->dtheta = 0 /*sp->par.c*sp->step */ ;
			sp->dphi = 0;
			break;
	}

	/* Clear the background. */
	MI_CLEARWINDOW(mi);

	/* Allocate memory. */

	if (!sp->csegs) {
		sp->csegs = (XSegment *) malloc(sizeof (XSegment) * sp->beecount
						* MI_NPIXELS(mi));
		sp->cnsegs = (int *) malloc(sizeof (int) * MI_NPIXELS(mi));

		sp->old_segs = (XSegment *) malloc(sizeof (XSegment) * sp->beecount);
		sp->p = (dvector *) malloc(sizeof (dvector) * sp->beecount * TIMES);
		sp->t = (double *) malloc(sizeof (double) * sp->beecount);
	}
	/* Initialize point positions, velocities, etc. */

	/* bees */
	for (b = 0; b < sp->beecount; b++) {
		X(0, b) = balance_rand(range.x);
		X(1, b) = X(0, b);
		Y(0, b) = balance_rand(range.y);
		Y(1, b) = Y(0, b);
		Z(0, b) = balance_rand(range.z);
		Z(1, b) = Z(0, b);
		sp->t[b] = 0;
	}
}


void
draw_flow(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	flowstruct *sp = &flows[MI_SCREEN(mi)];
	int         b, c;
	int         col, ix;
	double      sint, cost, sinp, cosp;

	sp->theta += sp->dtheta;
	sp->phi += sp->dphi;
	sint = sin(sp->theta);
	cost = cos(sp->theta);
	sinp = sin(sp->phi);
	cosp = cos(sp->phi);
	for (col = 0; col < MI_NPIXELS(mi); col++)
		sp->cnsegs[col] = 0;

	/* <=- Bees -=> */
	for (b = 0; b < sp->beecount; b++) {
		/* Age the arrays. */
		X(1, b) = X(0, b);
		Y(1, b) = Y(0, b);
		Z(1, b) = Z(0, b);

		/* 2nd order Kunge Kutta */
		{
			double      k1x, k1y, k1z;
			double      k2x, k2y, k2z;

			sp->t[b] += sp->step;	/* tick */
			sp->ODE(sp->par, &k1x, &k1y, &k1z,
				X(1, b), Y(1, b), Z(1, b), sp->t[b]);
			k1x *= sp->step;
			k1y *= sp->step;
			k1z *= sp->step;
			sp->ODE(sp->par, &k2x, &k2y, &k2z,
				X(1, b) + k1x, Y(1, b) + k1y, Z(1, b) + k1z, sp->t[b]);
			k2x *= sp->step;
			k2y *= sp->step;
			k2z *= sp->step;
			X(0, b) = X(1, b) + (k1x + k2x) / 2.0;
			Y(0, b) = Y(1, b) + (k1y + k2y) / 2.0;
			Z(0, b) = Z(1, b) + (k1z + k2z) / 2.0;
		}

		/* Fill the segment lists. */


		/* Tumble */
#define DISPLAYX(A) (sp->width/2+sp->width/sp->size* \
		     ((X((A),b)-sp->c.x)*cost \
		      -(Y((A),b)-sp->c.y)*sint*cosp \
		      +(Z((A),b)-sp->c.z)*sint*sinp))
#define DISPLAYY(A) (sp->height/2-sp->height/sp->size* \
		     ((X((A),b)-sp->c.x)*sint \
		      +(Y((A),b)-sp->c.y)*cost*cosp \
		      -(Z((A),b)-sp->c.z)*cost*sinp))

		/* Colour according to bee */
		col = b % (MI_NPIXELS(mi) - 1);

		ix = col * sp->beecount + sp->cnsegs[col];
		sp->csegs[ix].x1 = DISPLAYX(0);
		sp->csegs[ix].y1 = DISPLAYY(0);
		sp->csegs[ix].x2 = DISPLAYX(1);
		sp->csegs[ix].y2 = DISPLAYY(1);
		sp->cnsegs[col]++;
	}
	if (sp->count) {
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		XDrawSegments(display, window, gc, sp->old_segs, sp->beecount);
	}
	XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
	if (MI_NPIXELS(mi) > 2) {
		for (col = 0; col < MI_NPIXELS(mi); col++) {
			if (sp->cnsegs[col] > 0) {
				XSetForeground(display, gc, MI_PIXEL(mi, col));
				XDrawSegments(display, window, gc,
					      sp->csegs + col * sp->beecount,
					      sp->cnsegs[col]);
			}
		}
	} else {
	  /* mono */
	  XSetForeground(display, gc, MI_PIXEL(mi, 1));
	  XDrawSegments(display, window, gc,
					sp->csegs + col * sp->beecount,
					sp->cnsegs[col]);
	}
	for (col = 0, c = 0; col < MI_NPIXELS(mi); col++)
		for (b = 0; b < sp->cnsegs[col]; b++) {
			XSegment    s = (sp->csegs + col * sp->beecount)[b];

			sp->old_segs[c].x1 = s.x1;
			sp->old_segs[c].y1 = s.y1;
			sp->old_segs[c].x2 = s.x2;
			sp->old_segs[c].y2 = s.y2;
			c++;
		}
	if (++sp->count > MI_CYCLES(mi)) {
		init_flow(mi);
	}
}

void
release_flow(ModeInfo * mi)
{
	if (flows != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			flowstruct *sp = &flows[screen];

			if (sp->csegs != NULL)
				(void) free((void *) sp->csegs);
			if (sp->cnsegs != NULL)
				(void) free((void *) sp->cnsegs);
			if (sp->old_segs != NULL)
				(void) free((void *) sp->old_segs);
			if (sp->p != NULL)
				(void) free((void *) sp->p);
			if (sp->t != NULL)
				(void) free((void *) sp->t);
		}
		(void) free((void *) flows);
		flows = NULL;
	}
}

void
refresh_flow(ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
}
