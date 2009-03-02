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
 * of chaos fame), two forms of Rossler's "Folded Band" and Poincare'
 * sections of the "Birkhoff Bagel" and Duffing's forced occilator.
 *
 * Revision History:
 * 31-Nov-98: [TDA] Added Duffing  (what a strange day that was :) DAB)
 *   Duffing's forced oscillator has been added to the formula list and
 *   the parameters section has been updated to display it in Poincare'
 *   section.
 * 30-Nov-98: [TDA] Added travelling perspective option
 *   A more exciting point-of-view has been added to all autonomous flows.
 *   This views the flow as seen by a particle moving with the flow.  In the
 *   metaphor of the original code, I've attached a camera to one of the
 *   trained bees!
 * 30-Nov-98: [TDA] Much code cleanup.
 * 09-Apr-97: [TDA] Ported to xlockmore-4
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

ModeSpecOpt flow_opts = { 0, NULL, 0, NULL, NULL };

#ifdef USE_MODULES
ModStruct   flow_description = {
	"flow", "init_flow", "draw_flow", "release_flow",
	"refresh_flow", "init_flow", NULL, &flow_opts,
	1000, 1024, 3000, 1, 64, 1.0, "",
	"Shows dynamic strange attractors", 0, NULL
};

#endif

typedef struct {
	double      x;
	double      y;
	double      z;
} dvector;

typedef struct {
	double      a, b, c;
} Par;

/* Macros */
#define X(t,b)	(sp->p[t][b].x)
#define Y(t,b)	(sp->p[t][b].y)
#define Z(t,b)	(sp->p[t][b].z)
#define balance_rand(v)	((LRAND()/MAXRAND*(v))-((v)/2))	/* random around 0 */
#define SCALE_X(A) (sp->width/2+sp->width/sp->size*(A))
#define SCALE_Y(A) (sp->height/2+sp->height/sp->size*(A))

typedef struct {
	int         width;
	int         height;
	int         count;
	double      size;
	
	int         beecount;	/* number of bees */
	XSegment   *csegs;	    /* bee lines */
	int        *cnsegs;
	XSegment   *old_segs;	/* old bee lines */
	int         nold_segs;
	double      step;
	dvector     centre;		/* centre */
	struct {
		double  depth;
		double  height;
	}           view;
	dvector    *p[2];   /* bee positions x[time][bee#] */
	struct {
		double  theta;
		double  dtheta;
		double  phi;
		double  dphi;
	}           tumble;
	dvector  (*ODE) (Par par, double x, double y, double z);
	Par         par;
} flowstruct;

static flowstruct *flows = NULL;

static dvector
Lorentz(Par par, double x, double y, double z)
{
	dvector d;

	d.x = par.a * (y - x);
	d.y = x * (par.b - z) - y;
	d.z = x * y - par.c * z;
	return d;
}

static dvector
Rossler(Par par, double x, double y, double z)
{
	dvector d;

	d.x = -(y + par.a * z);
	d.y = x + y * par.b;
	d.z = par.c + z * (x - 5.7);
	return d;
}

static dvector
RosslerCone(Par par, double x, double y, double z)
{
	dvector d;

	d.x = -(y + par.a * z);
	d.y = x + y * par.b - z * z * par.c;
	d.z = 0.2 + z * (x - 5.7);
	return d;
}

static dvector
Birkhoff(Par par, double x, double y, double z)
{
	dvector d;

	d.x = -y + par.b * sin(z);
	d.y = 0.7 * x + par.a * y * (0.1 - x * x);
	d.z = par.c;
	return d;
}

static dvector
Duffing(Par par, double x, double y, double z)
{
	dvector d;

	d.x = -par.a * x - y/2 - y * y * y/8 + par.b * cos(z);
	d.y = 2*x;
	d.z = par.c;
	return d;
}

void
init_flow(ModeInfo * mi)
{
	flowstruct *sp;
	int         b;
	double      beemult = 1;
	dvector     range;
	static int  allocated = 0;

	if (flows == NULL) {
		if ((flows = (flowstruct *) calloc(MI_NUM_SCREENS(mi),
					       sizeof (flowstruct))) == NULL)
			return;
	}
	sp = &flows[MI_SCREEN(mi)];

	sp->count = 0;

	sp->width = MI_WIDTH(mi);
	sp->height = MI_HEIGHT(mi);

	sp->tumble.theta = balance_rand(M_PI);
	sp->tumble.phi = balance_rand(M_PI);
	sp->tumble.dtheta = 0.002;
	sp->tumble.dphi = 0.001;
	sp->view.height = 0;
	sp->view.depth = 0; /* no perspective view */

	switch (NRAND(8)) {
	case 0:
		sp->view.depth = 10;
		sp->view.height = 0.2;
		beemult = 3;
	case 1:
		sp->ODE = Lorentz;
		sp->step = 0.02;
		sp->size = 60;
		sp->centre.x = 0;
		sp->centre.y = 0;
		sp->centre.z = 24;
		range.x = 5;
		range.y = 5;
		range.z = 1;
		sp->par.a = 10 + balance_rand(5);
		sp->par.b = 28 + balance_rand(5);
		sp->par.c = 2 + balance_rand(1);
		break;
	case 2:
		sp->view.depth = 10;
		sp->view.height = 0.1;
		beemult = 4;
	case 3:
		sp->ODE = Rossler;
		sp->step = 0.05;
		sp->size = 24;
		sp->centre.x = 0;
		sp->centre.y = 0;
		sp->centre.z = 3;
		range.x = 4;
		range.y = 4;
		range.z = 7;
		sp->par.a = 2 + balance_rand(1);
		sp->par.b = 0.2 + balance_rand(0.1);
		sp->par.c = 0.2 + balance_rand(0.1);
		break;
	case 4:
		sp->view.depth = 10;
		sp->view.height = 0.1;
		beemult = 3;
	case 5:
		sp->ODE = RosslerCone;
		sp->step = 0.05;
		sp->size = 24;
		sp->centre.x = 0;
		sp->centre.y = 0;
		sp->centre.z = 3;
		range.x = 4;
		range.y = 4;
		range.z = 4;
		sp->par.a = 2;
		sp->par.b = 0.2;
		sp->par.c = 0.25 + balance_rand(0.09);
		break;
	case 6:
		sp->ODE = Birkhoff;
		sp->step = 0.04;
		sp->size = 2.6;
		sp->centre.x = 0;
		sp->centre.y = 0;
		sp->centre.z = 0;
		range.x = 3;
		range.y = 4;
		range.z = 0;
		sp->par.a = 10 + balance_rand(5);
		sp->par.b = 0.35 + balance_rand(0.25);
		sp->par.c = 1.57;
		sp->tumble.theta = 0;
		sp->tumble.phi = 0;
		sp->tumble.dtheta = 0;
		sp->tumble.dphi = 0;
		break;
	case 7:
	default:
		sp->ODE = Duffing;
		sp->step = 0.02;
		sp->size = 30;
		sp->centre.x = 0;
		sp->centre.y = 0;
		sp->centre.z = 0;
		range.x = 20;
		range.y = 20;
		range.z = 0;
		sp->par.a = 0.2 + balance_rand(0.1);
		sp->par.b = 27.0 + balance_rand(3.0);
		sp->par.c = 1.33;
		sp->tumble.theta = 0;
		sp->tumble.phi = 0;
		sp->tumble.dtheta = -NRAND(2)*sp->par.c*sp->step;
		sp->tumble.dphi = 0;
		beemult = 0.5;
		break;
	}

	sp->beecount = beemult * MI_COUNT(mi);
	if (sp->beecount < 0)	/* random variations */	
		sp->beecount = NRAND(-sp->beecount) + 1; /* Minimum 1 */

	/* Clear the background. */
	MI_CLEARWINDOW(mi);

	if(!allocated || sp->beecount != allocated){ /* reallocate */
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
		if (sp->p[0] != NULL) {
			(void) free((void *) sp->p[0]);
			sp->p[0] = NULL;
		}
		if (sp->p[1] != NULL) {
			(void) free((void *) sp->p[1]);
			sp->p[1] = NULL;
		}
	}

	/* Allocate memory. */

	if (!sp->csegs) {
		sp->csegs = (XSegment *) malloc(sizeof (XSegment) * sp->beecount
						* MI_NPIXELS(mi));
		sp->cnsegs = (int *) malloc(sizeof (int) * MI_NPIXELS(mi));

		sp->old_segs = (XSegment *) malloc(sizeof (XSegment) * sp->beecount);
		sp->p[0] = (dvector *) malloc(sizeof (dvector) * sp->beecount);
		sp->p[1] = (dvector *) malloc(sizeof (dvector) * sp->beecount);
	}

	/* Initialize point positions, velocities, etc. */

	for (b = 0; b < sp->beecount; b++) {
		X(1, b) = X(0, b) = balance_rand(range.x);
		Y(1, b) = Y(0, b) = balance_rand(range.y);
		Z(1, b) = Z(0, b) = balance_rand(range.z);
	}
}

void
draw_flow(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	flowstruct *sp = &flows[MI_SCREEN(mi)];
	int         b, c, i;
	int         col, ix;
	double      M[3][3]; /* transformation matrix */

	if(!sp->view.depth){ /* simple 3D tumble */
		double      sint, cost, sinp, cosp;
		sp->tumble.theta += sp->tumble.dtheta;
		sp->tumble.phi += sp->tumble.dphi;
		sint = sin(sp->tumble.theta);
		cost = cos(sp->tumble.theta);
		sinp = sin(sp->tumble.phi);
		cosp = cos(sp->tumble.phi);
		M[0][0]= cost; M[0][1]=-sint*cosp; M[0][2]= sint*sinp;
		M[1][0]= sint; M[1][1]= cost*cosp; M[1][2]=-cost*sinp;
		M[2][0]= 0;    M[2][1]= 0;         M[2][2]= 1;
	} else { /* initialize matrix */
		M[0][0]= 0; M[0][1]= 0; M[0][2]= 0;
		M[1][0]= 0; M[1][1]= 0; M[1][2]= 0;
		M[2][0]= 0; M[2][1]= 0; M[2][2]= 0;

	}

	for (col = 0; col < MI_NPIXELS(mi); col++)
		sp->cnsegs[col] = 0;

	MI_IS_DRAWN(mi) = True;

	/* <=- Bees -=> */
	for (b = 0; b < sp->beecount; b++) {
		/* Age the arrays. */
		X(1, b) = X(0, b);
		Y(1, b) = Y(0, b);
		Z(1, b) = Z(0, b);

		/* 2nd order Kunge Kutta */
		{
			dvector     k1, k2;

			k1 = sp->ODE(sp->par, X(1, b), Y(1, b), Z(1, b));
			k1.x *= sp->step;
			k1.y *= sp->step;
			k1.z *= sp->step;
			k2 = sp->ODE(sp->par, X(1, b) + k1.x, Y(1, b) + k1.y, Z(1, b) + k1.z);
			k2.x *= sp->step;
			k2.y *= sp->step;
			k2.z *= sp->step;
			X(0, b) = X(1, b) + (k1.x + k2.x) / 2.0;
			Y(0, b) = Y(1, b) + (k1.y + k2.y) / 2.0;
			Z(0, b) = Z(1, b) + (k1.z + k2.z) / 2.0;
		}

		/* Colour according to bee */
		col = b % (MI_NPIXELS(mi) - 1);
		ix = col * sp->beecount + sp->cnsegs[col];

		/* Fill the segment lists. */

		if(sp->view.depth) { /* perspective view has special points */
			if(b==0){ /* point of view */
				sp->centre.x=X(0, b);
				sp->centre.y=Y(0, b);
				sp->centre.z=Z(0, b);
			}else if(b==1){ /* neighbour: used to compute local axes */
				double x[3], p[3], x2=0, xp=0;
				int j;

				/* forward */				
				x[0] = X(0, 0) - X(1, 0);
				x[1] = Y(0, 0) - Y(1, 0);
				x[2] = Z(0, 0) - Z(1, 0);
			
				/* neighbour */
				p[0] = X(0, 1) - X(1, 0);
				p[1] = Y(0, 1) - Y(1, 0);
				p[2] = Z(0, 1) - Z(1, 0);

				for(i=0; i<3; i++){
					x2+= x[i]*x[i];    /* X . X */
					xp+= x[i]*p[i];    /* X . P */
					M[0][i] = x[i];    /* X */
				}

				for(i=0; i<3; i++)               /* (X x P) x X */
					M[1][i] = x2*p[i] - xp*x[i]; /* == (X . X) P - (X . P) X */
				
				M[2][0] =  x[1]*p[2] - x[2]*p[1]; /* X x P */
				M[2][1] = -x[0]*p[2] + x[2]*p[0];
				M[2][2] =  x[0]*p[1] - x[1]*p[0];

				/* normalise axes */
				for(j=0; j<3; j++){
					double A=0;
					for(i=0; i<3; i++) A+=M[j][i]*M[j][i]; /* sum squares */
					A=sqrt(A);
					for(i=0; i<3; i++) M[j][i]/=A;
				}

				X(0, 1)=X(0, 0)+M[1][0]; /* adjust neighbour */
				Y(0, 1)=Y(0, 0)+M[1][1];
				Z(0, 1)=Z(0, 0)+M[1][2];

#if 0  /* display local axes for testing */
				X(1, b)=X(0, 0);
				Y(1, b)=Y(0, 0);
				Z(1, b)=Z(0, 0);
			}else if(b==2){
				X(0, b)=X(0, 0)+0.5*M[0][0];
				Y(0, b)=Y(0, 0)+0.5*M[0][1];
				Z(0, b)=Z(0, 0)+0.5*M[0][2];
				X(1, b)=X(0, 0);
				Y(1, b)=Y(0, 0);
				Z(1, b)=Z(0, 0);
			}else if(b==3){
				X(0, b)=X(0, 0)+1.5*M[2][0];
				Y(0, b)=Y(0, 0)+1.5*M[2][1];
				Z(0, b)=Z(0, 0)+1.5*M[2][2];
				X(1, b)=X(0, 0);
				Y(1, b)=Y(0, 0);
				Z(1, b)=Z(0, 0);
#endif
			}
		}
		
		for(i=0; i<2; i++){
			double x=X(i,b)-sp->centre.x;
			double y=Y(i,b)-sp->centre.y;
			double z=Z(i,b)-sp->centre.z;
			double X=M[0][0]*x + M[0][1]*y + M[0][2]*z;
			double Y=M[1][0]*x + M[1][1]*y + M[1][2]*z;
			double Z=M[2][0]*x + M[2][1]*y + M[2][2]*z+sp->view.height;
			double absx, absy;				
			if(sp->view.depth){
				if(X <= 0) break;
				absx=SCALE_X(sp->view.depth*Y/X);
				absy=SCALE_Y(sp->view.depth*Z/X);
				if(absx < -sp->width || absx > 2*sp->width ||
				   absy < -sp->height || absy > 2*sp->height)
					break;
			}else{
				absx=SCALE_X(X);
				absy=SCALE_Y(Y);
			}
			if(i){
				sp->csegs[ix].x1 = (short) absx;
				sp->csegs[ix].y1 = (short) absy;
			}else{
				sp->csegs[ix].x2 = (short) absx;
				sp->csegs[ix].y2 = (short) absy;
			}
		}
		if(i == 2) /* both assigned */
			sp->cnsegs[col]++;
    }
	if (sp->count) { /* erase */
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		XDrawSegments(display, window, gc, sp->old_segs, sp->nold_segs);
	}

	if (MI_NPIXELS(mi) > 2){ /* render colour */
		for (col = 0; col < MI_NPIXELS(mi); col++)
			if (sp->cnsegs[col] > 0) {
				XSetForeground(display, gc, MI_PIXEL(mi, col));
				XDrawSegments(display, window, gc,
					      sp->csegs + col * sp->beecount, sp->cnsegs[col]);
			}
	} else { 		/* render mono */
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
		XDrawSegments(display, window, gc,
					  sp->csegs + col * sp->beecount, sp->cnsegs[col]);
	}

	/* Copy to erase-list */
	for (col = 0, c = 0; col < MI_NPIXELS(mi); col++)
		for (b = 0; b < sp->cnsegs[col]; b++)
			sp->old_segs[c++] = (sp->csegs + col * sp->beecount)[b];
	sp->nold_segs = c;

	if (++sp->count > MI_CYCLES(mi)) /* pick a new flow */
		init_flow(mi);
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
			if (sp->p[0] != NULL)
				(void) free((void *) sp->p[0]);
			if (sp->p[1] != NULL)
				(void) free((void *) sp->p[1]);
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
