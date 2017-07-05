/* -*- Mode: C; tab-width: 4; c-basic-offset: 4 -*- */
/* flow --- flow of strange bees */

#if 0
static const char sccsid[] = "@(#)flow.c	5.00 2000/11/01 xlockmore";
#endif

/*-
 * Copyright (c) 1996 by Tim Auckland <tda10.geo@yahoo.com>
 * Incorporating some code from Stephen Davies Copyright (c) 2000
 *
 * Search code based on techniques described in "Strange Attractors:
 * Creating Patterns in Chaos" by Julien C. Sprott
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
 * sections of the "Birkhoff Bagel" and Duffing's forced occilator.  "flow"
 * can now discover new attractors.
 *
 * Revision History:
 *
 * 29-Oct-2004: [TDA] Discover Attractors unknown to science.
 *   Replace 2D rendering of Periodic Attractors with a 3D
 *   'interrupted' rendering.  Replace "-/+allow2d" with "-/+periodic"
 *   Replace all ODE formulae with completely generic forms.
 *   Add '-search' option to perform background high-speed discovery
 *   for completely new attractors without impacting rendering
 *   performance.
 *   Use gaussian distribution for initial point positions and for
 *   parameter search.
 *   Add "+dbuf" option to allow Double-Buffering to be turned off on
 *   slow X servers.
 *   Remove redundant '-zoom' option.  Now automatically zooms if both
 *   rotation and riding are permitted.
 *   Replace dynamic bounding box with static one pre-calculated
 *   during discovery phase.
 *   Simplify and fix bounding box clipping code.  Should now be safe
 *   to run without double buffer on all XFree86 servers if desired.
 * 12-Oct-2004: [TDA] Merge Xscreensaver and Xlockmore branches
 *   Added Chalky's orbital camera, but made the zooming work by
 *   flying the camera rather than interpolating the view transforms.
 *   Added Chalky's Bounding Box, but time-averaged the boundaries to
 *   let the lost bees escape.
 *   Added Chalky's 'view-frustrum' clipping, but only applying it to
 *   the Bounding Box.  Trails make clipping less useful.
 *   Added Chalky's "-slow" and "-freeze" options for compatibility,
 *   but haven't implemented the features, since the results are ugly
 *   and make no mathematical contribution.
 *   Added Double-Buffering as a work-around for a persistent XFree86
 *   bug that left debris on the screen.
 * 21-Mar-2003: [TDA] Trails added (XLockmore branch)
 * 01-Nov-2000: [TDA] Allocation checks (XLockmore branch)
 * 21-Feb-2000: [Chalky] Major hackage (Stephen Davies, chalky@null.net)
 *   (Xscreensaver branch)
 *   Forced perspective mode, added 3d box around attractor which
 *   involved coding 3d-planar-clipping against the view-frustrum
 *   thingy. Also made view alternate between piggybacking on a 'bee'
 *   to zooming around outside the attractor. Most bees slow down and
 *   stop, to make the structure of the attractor more obvious.
* 28-Jan-1999: [TDA] Catch 'lost' bees in flow.c and disable them.
 *   (XLockmore branch)
 *   I chose to disable them rather than reinitialise them because
 *   reinitialising can produce fake attractors.
 *   This has allowed me to relax some of the parameters and initial
 *   conditions slightly to catch some of the more extreme cases.  As a
 *   result you may see some bees fly away at the start - these are the ones
 *   that 'missed' the attractor.  If the bee with the camera should fly
 *   away the mode will restart  :-)
 * 31-Nov-1998: [TDA] Added Duffing  (what a strange day that was :) DAB)
 *   Duffing's forced oscillator has been added to the formula list and
 *   the parameters section has been updated to display it in Poincare'
 *   section.
 * 30-Nov-1998: [TDA] Added travelling perspective option
 *   A more exciting point-of-view has been added to all autonomous flows.
 *   This views the flow as seen by a particle moving with the flow.  In the
 *   metaphor of the original code, I've attached a camera to one of the
 *   trained bees!
 * 30-Nov-1998: [TDA] Much code cleanup.
 * 09-Apr-1997: [TDA] Ported to xlockmore-4
 * 18-Jul-1996: Adapted from swarm.c Copyright (c) 1991 by Patrick J. Naughton.
 * 31-Aug-1990: Adapted from xswarm by Jeff Butterworth. (butterwo@ncsc.org).
 */

#ifdef STANDALONE
# define MODE_flow
# define DEFAULTS   "*delay:       10000 \n" \
					"*count:       3000  \n" \
					"*size:        -10   \n" \
					"*cycles:      10000 \n" \
					"*ncolors:     200   \n"

# define release_flow 0
# include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
# include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_flow

#define DEF_ROTATE   "TRUE"
#define DEF_RIDE     "TRUE"
#define DEF_BOX      "TRUE"
#define DEF_PERIODIC "TRUE"
#define DEF_SEARCH   "TRUE"
#define DEF_DBUF     "TRUE"

static Bool rotatep;
static Bool ridep;
static Bool boxp;
static Bool periodicp;
static Bool searchp;
static Bool dbufp;

static XrmOptionDescRec opts[] = {
	{"-rotate",   ".flow.rotate",   XrmoptionNoArg, "on"},
	{"+rotate",   ".flow.rotate",   XrmoptionNoArg, "off"},
	{"-ride",     ".flow.ride",     XrmoptionNoArg, "on"},
	{"+ride",     ".flow.ride",     XrmoptionNoArg, "off"},
	{"-box",      ".flow.box",      XrmoptionNoArg, "on"},
	{"+box",      ".flow.box",      XrmoptionNoArg, "off"},
	{"-periodic", ".flow.periodic", XrmoptionNoArg, "on"},
	{"+periodic", ".flow.periodic", XrmoptionNoArg, "off"},
	{"-search",   ".flow.search",   XrmoptionNoArg, "on"},
	{"+search",   ".flow.search",   XrmoptionNoArg, "off"},
	{"-dbuf",     ".flow.dbuf",     XrmoptionNoArg, "on"},
	{"+dbuf",     ".flow.dbuf",     XrmoptionNoArg, "off"},
};

static argtype vars[] = {
    {&rotatep,   "rotate",   "Rotate",   DEF_ROTATE,   t_Bool},
    {&ridep,     "ride",     "Ride",     DEF_RIDE,     t_Bool},
    {&boxp,      "box",      "Box",      DEF_BOX,      t_Bool},
    {&periodicp, "periodic", "Periodic", DEF_PERIODIC, t_Bool}, 
    {&searchp,   "search",   "Search",   DEF_SEARCH,   t_Bool}, 
    {&dbufp,     "dbuf",     "Dbuf",     DEF_DBUF,     t_Bool}, 
};

static OptionStruct desc[] = {
    {"-/+rotate",   "turn on/off rotating around attractor."},
    {"-/+ride",     "turn on/off ride in the flow."},
    {"-/+box",      "turn on/off bounding box."},
    {"-/+periodic", "turn on/off periodic attractors."},
    {"-/+search",   "turn on/off search for new attractors."},
    {"-/+dbuf",     "turn on/off double buffering."},
};

ENTRYPOINT ModeSpecOpt flow_opts =
{sizeof opts / sizeof opts[0], opts,
 sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   flow_description = {
	"flow", "init_flow", "draw_flow", "release_flow",
	"refresh_flow", "init_flow", NULL, &flow_opts,
	1000, 1024, 10000, -10, 200, 1.0, "",
	"Shows dynamic strange attractors", 0, NULL
};

#endif

typedef struct { double x, y, z; } dvector;

#define N_PARS 20 /* Enough for Full Cubic or Periodic Cubic */
typedef dvector Par[N_PARS];
enum { /* Name the parameter indices to make it easier to write
		  standard examples */
	C,
	X,XX,XXX,XXY,XXZ,XY,XYY,XYZ,XZ,XZZ,
	Y,YY,YYY,YYZ,YZ,YZZ,
	Z,ZZ,ZZZ,
	SINY = XY /* OK to overlap in this case */
};

/* Camera target [TDA] */
typedef enum {
	ORBIT = 0,
	BEE = 1
} Chaseto;

/* Macros */
#define IX(C) ((C) * segindex + sp->cnsegs[(C)])
#define B(t,b)	(sp->p + (t) + (b) * sp->taillen)
#define X(t,b)	(B((t),(b))->x)
#define Y(t,b)	(B((t),(b))->y)
#define Z(t,b)	(B((t),(b))->z)
#define balance_rand(v)	((LRAND()/MAXRAND*(v))-((v)/2))	/* random around 0 */
#define LOST_IN_SPACE 2000.0
#define INITIALSTEP 0.04
#define EYEHEIGHT   0.005
#define MINTRAIL 2
#define BOX_L 36

/* Points that make up the box (normalized coordinates) */
static const double box[][3] = {
	{1,1,1},   /* 0 */
	{1,1,-1},  /* 1 */
	{1,-1,-1}, /* 2 */
	{1,-1,1},  /* 3 */
	{-1,1,1},  /* 4 */
	{-1,1,-1}, /* 5 */
	{-1,-1,-1},/* 6 */
	{-1,-1,1}, /* 7 */
	{1, .8, .8},
	{1, .8,-.8},
	{1,-.8,-.8},
	{1,-.8, .8},
	{ .8,1, .8},
	{ .8,1,-.8},
	{-.8,1,-.8},
	{-.8,1, .8},
	{ .8, .8,1},
	{ .8,-.8,1},
	{-.8,-.8,1},
	{-.8, .8,1},
	{-1, .8, .8},
	{-1, .8,-.8},
	{-1,-.8,-.8},
	{-1,-.8, .8},
	{ .8,-1, .8},
	{ .8,-1,-.8},
	{-.8,-1,-.8},
	{-.8,-1, .8},
	{ .8, .8,-1},
	{ .8,-.8,-1},
	{-.8,-.8,-1},
	{-.8, .8,-1}
};

/* Lines connecting the box dots */
static const double lines[][2] = {
	{0,1}, {1,2}, {2,3}, {3,0}, /* box */
	{4,5}, {5,6}, {6,7}, {7,4},
	{0,4}, {1,5}, {2,6}, {3,7},
	{4+4,5+4}, {5+4,6+4}, {6+4,7+4}, {7+4,4+4},
	{4+8,5+8}, {5+8,6+8}, {6+8,7+8}, {7+8,4+8},
	{4+12,5+12}, {5+12,6+12}, {6+12,7+12}, {7+12,4+12},
	{4+16,5+16}, {5+16,6+16}, {6+16,7+16}, {7+16,4+16},
	{4+20,5+20}, {5+20,6+20}, {6+20,7+20}, {7+20,4+20},
	{4+24,5+24}, {5+24,6+24}, {6+24,7+24}, {7+24,4+24},
};

typedef struct {
	/* Variables used in rendering */
	dvector     cam[3]; /* camera flight path */
	int         chasetime;
	Chaseto		chaseto;
	Pixmap      buffer; /* Double Buffer */
	dvector		circle[2]; /* POV that circles around the scene */
	dvector     centre;		/* centre */
	int         beecount;	/* number of bees */
	XSegment   *csegs;	    /* bee lines */
	int        *cnsegs;
	XSegment   *old_segs;	/* old bee lines */
	int         nold_segs;
	int         taillen;

	/* Variables common to iterators */
	dvector  (*ODE) (Par par, double x, double y, double z);
	dvector  range; /* Initial conditions */
	double   yperiod; /* ODE's where Y is periodic. */

	/* Variables used in iterating main flow */
	Par         par;
	dvector    *p;   /* bee positions x[time][bee#] */
	int         count;
	double      lyap;
	double      size;
	dvector     mid; /* Effective bounding box */
	double      step;
	
	/* second set of variables, used for parallel search */
	Par         par2;
	dvector     p2[2];
	int         count2;
	double      lyap2;
	double      size2;
	dvector     mid2;
	double      step2;
	
} flowstruct;

static flowstruct *flows = (flowstruct *) NULL;

/*
 * Private functions
 */


/* ODE functions */

/* Generic 3D Cubic Polynomial.  Includes all the Quadratics (Lorentz,
   Rossler) and much more! */

/* I considered offering a seperate 'Quadratic' option, since Cubic is
   clearly overkill for the standard examples, but the performance
   difference is too small to measure.  The compute time is entirely
   dominated by the XDrawSegments calls anyway. [TDA] */
static dvector
Cubic(Par a, double x, double y, double z)
{
	dvector d;
	d.x = a[C].x + a[X].x*x + a[XX].x*x*x + a[XXX].x*x*x*x + a[XXY].x*x*x*y +
		a[XXZ].x*x*x*z + a[XY].x*x*y + a[XYY].x*x*y*y + a[XYZ].x*x*y*z +
		a[XZ].x*x*z + a[XZZ].x*x*z*z + a[Y].x*y + a[YY].x*y*y +
		a[YYY].x*y*y*y + a[YYZ].x*y*y*z + a[YZ].x*y*z + a[YZZ].x*y*z*z +
		a[Z].x*z + a[ZZ].x*z*z + a[ZZZ].x*z*z*z;

	d.y = a[C].y + a[X].y*x + a[XX].y*x*x + a[XXX].y*x*x*x + a[XXY].y*x*x*y +
		a[XXZ].y*x*x*z + a[XY].y*x*y + a[XYY].y*x*y*y + a[XYZ].y*x*y*z +
		a[XZ].y*x*z + a[XZZ].y*x*z*z + a[Y].y*y + a[YY].y*y*y +
		a[YYY].y*y*y*y + a[YYZ].y*y*y*z + a[YZ].y*y*z + a[YZZ].y*y*z*z +
		a[Z].y*z + a[ZZ].y*z*z + a[ZZZ].y*z*z*z;

	d.z = a[C].z + a[X].z*x + a[XX].z*x*x + a[XXX].z*x*x*x + a[XXY].z*x*x*y +
		a[XXZ].z*x*x*z + a[XY].z*x*y + a[XYY].z*x*y*y + a[XYZ].z*x*y*z +
		a[XZ].z*x*z + a[XZZ].z*x*z*z + a[Y].z*y + a[YY].z*y*y +
		a[YYY].z*y*y*y + a[YYZ].z*y*y*z + a[YZ].z*y*z + a[YZZ].z*y*z*z +
		a[Z].z*z + a[ZZ].z*z*z + a[ZZZ].z*z*z*z;

	return d;
}

/* 3D Cubic in (x,z) with periodic sinusoidal forcing term in x.  y is
   the independent periodic (time) axis.  This includes Birkhoff's
   Bagel and Duffing's Attractor */
static dvector
Periodic(Par a, double x, double y, double z)
{
	dvector d;

	d.x = a[C].x + a[X].x*x + a[XX].x*x*x + a[XXX].x*x*x*x +
		a[XXZ].x*x*x*z + a[XZ].x*x*z + a[XZZ].x*x*z*z + a[Z].x*z +
		a[ZZ].x*z*z + a[ZZZ].x*z*z*z + a[SINY].x*sin(y);

	d.y = a[C].y;

	d.z = a[C].z + a[X].z*x + a[XX].z*x*x + a[XXX].z*x*x*x +
		a[XXZ].z*x*x*z + a[XZ].z*x*z + a[XZZ].z*x*z*z + a[Z].z*z +
		a[ZZ].z*z*z + a[ZZZ].z*z*z*z;

	return d;
}

/* Numerical integration of the ODE using 2nd order Runge Kutta.
   Returns length^2 of the update, so that we can detect if the step
   size needs reducing. */
static double
Iterate(dvector *p, dvector(*ODE)(Par par, double x, double y, double z),
		Par par, double step)
{ 
	dvector     k1, k2, k3;
			
	k1 = ODE(par, p->x, p->y, p->z);
	k1.x *= step;
	k1.y *= step;
	k1.z *= step;
	k2 = ODE(par, p->x + k1.x, p->y + k1.y, p->z + k1.z);
	k2.x *= step;
	k2.y *= step;
	k2.z *= step;
	k3.x = (k1.x + k2.x) / 2.0;
	k3.y = (k1.y + k2.y) / 2.0;
	k3.z = (k1.z + k2.z) / 2.0;

	p->x += k3.x;
	p->y += k3.y;
	p->z += k3.z;

	return k3.x*k3.x + k3.y*k3.y + k3.z*k3.z;
}

/* Memory functions */

#define deallocate(p,t) if (p!=NULL) {free(p); p=(t*)NULL; }
#define allocate(p,t,s) if ((p=(t*)malloc(sizeof(t)*s))==NULL)\
{free_flow(mi);return;}

static void
free_flow(ModeInfo * mi)
{
	flowstruct *sp = &flows[MI_SCREEN(mi)];
	deallocate(sp->csegs, XSegment);
	deallocate(sp->cnsegs, int);
	deallocate(sp->old_segs, XSegment);
	deallocate(sp->p, dvector);
}

/* Generate Gaussian random number: mean 0, "amplitude" A (actually
   A is 3*standard deviation). */

/* Note this generates a pair of gaussian variables, so it saves one
   to give out next time it's called */
static double
Gauss_Rand(double A)
{
	static double d;
	static Bool ready = 0;
	if(ready) {
		ready = 0;
		return A/3 * d;
	} else {
		double x, y, w;		
		do {
			x = 2.0 * (double)LRAND() / MAXRAND - 1.0;
			y = 2.0 * (double)LRAND() / MAXRAND - 1.0;
			w = x*x + y*y;
		} while(w >= 1.0);

		w = sqrt((-2 * log(w))/w);
		ready = 1;		
		d =          x * w;
		return A/3 * y * w;
	}
}

/* Attempt to discover new atractors by sending a pair of bees on a
   fast trip through the new flow and computing their Lyapunov
   exponent.  Returns False if the bees fly away.

   If the bees stay bounded, the new bounds and the Lyapunov exponent
   are stored in sp and the function returns True.

   Repeat invocations continue the flow and improve the accuracy of
   the bounds and the Lyapunov exponent.  Set sp->count2 to zero to
   start a new test.

   Acts on alternate variable set, so that it can be run in parallel
   with the main flow */

static Bool
discover(ModeInfo * mi)
{
	flowstruct *sp;
	double l = 0;
	dvector dl;
	dvector max, min;
	double dl2, df, rs, lsum = 0, s, maxv2 = 0, v2;

	int N, i, nl = 0;

	if (flows == NULL)
		return 0;
	sp = &flows[MI_SCREEN(mi)];

	if(sp->count2 == 0) {
		/* initial conditions */
		sp->p2[0].x = Gauss_Rand(sp->range.x);
		sp->p2[0].y = (sp->yperiod > 0)?
			balance_rand(sp->range.y) : Gauss_Rand(sp->range.y);
		sp->p2[0].z = Gauss_Rand(sp->range.z);
		
		/* 1000 steps to find an attractor */
		/* Most cases explode out here */
		for(N=0; N < 1000; N++){
			Iterate(sp->p2, sp->ODE, sp->par2, sp->step2);
			if(sp->yperiod > 0 && sp->p2[0].y > sp->yperiod)
				sp->p2[0].y -= sp->yperiod;
			if(fabs(sp->p2[0].x) > LOST_IN_SPACE ||
			   fabs(sp->p2[0].y) > LOST_IN_SPACE ||
			   fabs(sp->p2[0].z) > LOST_IN_SPACE) {
				return 0;
			}
			sp->count2++;
		}
		/* Small perturbation */
		sp->p2[1].x = sp->p2[0].x + 0.000001;
		sp->p2[1].y = sp->p2[0].y;
		sp->p2[1].z = sp->p2[0].z;
	}

	/* Reset bounding box */
	max.x = min.x = sp->p2[0].x;
	max.y = min.y = sp->p2[0].y;
	max.z = min.z = sp->p2[0].z;

	/* Compute Lyapunov Exponent */

	/* (Technically, we're only estimating the largest Lyapunov
	   Exponent, but that's all we need to know to determine if we
	   have a strange attractor.) [TDA] */

	/* Fly two bees close together */
	for(N=0; N < 5000; N++){
		for(i=0; i< 2; i++) {			
			v2 = Iterate(sp->p2+i, sp->ODE, sp->par2, sp->step2);
			if(sp->yperiod > 0 && sp->p2[i].y > sp->yperiod)
				sp->p2[i].y -= sp->yperiod;
			
			if(fabs(sp->p2[i].x) > LOST_IN_SPACE ||
			   fabs(sp->p2[i].y) > LOST_IN_SPACE ||
			   fabs(sp->p2[i].z) > LOST_IN_SPACE) {
				return 0;
			}
			if(v2 > maxv2) maxv2 = v2; /* Track max v^2 */
		}

		/* find bounding box */
		if ( sp->p2[0].x < min.x )      min.x = sp->p2[0].x;
		else if ( sp->p2[0].x > max.x ) max.x = sp->p2[0].x;
		if ( sp->p2[0].y < min.y )      min.y = sp->p2[0].y;
		else if ( sp->p2[0].y > max.y ) max.y = sp->p2[0].y;
		if ( sp->p2[0].z < min.z )      min.z = sp->p2[0].z;
		else if ( sp->p2[0].z > max.z ) max.z = sp->p2[0].z;

		/* Measure how much we have to pull the two bees to prevent
		   them diverging. */
		dl.x = sp->p2[1].x - sp->p2[0].x;
		dl.y = sp->p2[1].y - sp->p2[0].y;
		dl.z = sp->p2[1].z - sp->p2[0].z;
		
		dl2 = dl.x*dl.x + dl.y*dl.y + dl.z*dl.z;
		if(dl2 > 0) {
			df = 1e12 * dl2;
			rs = 1/sqrt(df);
			sp->p2[1].x = sp->p2[0].x + rs * dl.x;
			sp->p2[1].y = sp->p2[0].y + rs * dl.y;
			sp->p2[1].z = sp->p2[0].z + rs * dl.z;
			lsum = lsum + log(df);
			nl = nl + 1;
			l = M_LOG2E / 2 * lsum / nl / sp->step2;
		}
		sp->count2++;
	}
	/* Anything that didn't explode has a finite attractor */
	/* If Lyapunov is negative then it probably hit a fixed point or a
     * limit cycle.  Positive Lyapunov indicates a strange attractor. */

	sp->lyap2 = l;

	sp->size2 = max.x - min.x;
	s = max.y - min.y;
	if(s > sp->size2) sp->size2 = s;
	s = max.z - min.z;
	if(s > sp->size2) sp->size2 = s;

	sp->mid2.x = (max.x + min.x) / 2;
	sp->mid2.y = (max.y + min.y) / 2;
	sp->mid2.z = (max.z + min.z) / 2;

	if(sqrt(maxv2) > sp->size2 * 0.2) {
		/* Flowing too fast, reduce step size.  This
		   helps to eliminate high-speed limit cycles,
		   which can show +ve Lyapunov due to integration
		   inaccuracy. */		
		sp->step2 /= 2;		
	}
	return 1;
}

/* Sets up initial conditions for a flow without all the extra baggage
   that goes with init_flow */
static void
restart_flow(ModeInfo * mi)
{
	flowstruct *sp;
	int         b;

	if (flows == NULL)
		return;
	sp = &flows[MI_SCREEN(mi)];
	sp->count = 0;

	/* Re-Initialize point positions, velocities, etc. */
	for (b = 0; b < sp->beecount; b++) {
		X(0, b) = Gauss_Rand(sp->range.x);
		Y(0, b) = (sp->yperiod > 0)?
			balance_rand(sp->range.y) : Gauss_Rand(sp->range.y);
		Z(0, b) = Gauss_Rand(sp->range.z);
	}
}

/* Returns true if line was behind a clip plane, or it clips the line */
/* nx,ny,nz is the normal to the plane.   d is the distance from the origin */
/* s and e are the end points of the line to be clipped */
static int
clip(double nx, double ny, double nz, double d, dvector *s, dvector *e)
{
	int front1, front2;
	dvector w, p;
	double t;

	front1 = (nx*s->x + ny*s->y + nz*s->z >= -d);
	front2 = (nx*e->x + ny*e->y + nz*e->z >= -d);
	if (!front1 && !front2) return 1;
	if (front1 && front2) return 0;	
	w.x = e->x - s->x;
	w.y = e->y - s->y;
	w.z = e->z - s->z;
	
	/* Find t in line equation */
	t = ( -d - nx*s->x - ny*s->y - nz*s->z) / ( nx*w.x + ny*w.y + nz*w.z);
	
	p.x = s->x + w.x * t;
	p.y = s->y + w.y * t;
	p.z = s->z + w.z * t;
	
	/* Move clipped point to the intersection */
	if (front2) {
		*s = p;
	} else {
		*e = p;
	}
	return 0;
}

/* 
 * Public functions
 */

ENTRYPOINT void
init_flow (ModeInfo * mi)
{
	flowstruct *sp;
	char       *name;
	
	MI_INIT (mi, flows, free_flow);
	sp = &flows[MI_SCREEN(mi)];

	sp->count2 = 0;

	sp->taillen = MI_SIZE(mi);
	if (sp->taillen < -MINTRAIL) {
		/* Change by sqrt so it seems more variable */
		sp->taillen = NRAND((int)sqrt((double) (-sp->taillen - MINTRAIL + 1)));
		sp->taillen = sp->taillen * sp->taillen + MINTRAIL;
	} else if (sp->taillen < MINTRAIL) {
		sp->taillen = MINTRAIL;
	}

	if(!rotatep && !ridep) rotatep = True; /* We need at least one viewpoint */

	/* Start camera at Orbit or Bee */
	if(rotatep) {
		sp->chaseto = ORBIT;
	} else {
		sp->chaseto = BEE;
	}
	sp->chasetime = 1; /* Go directly to target */

	sp->lyap = 0;
	sp->yperiod = 0;
	sp->step2 = INITIALSTEP;

	/* Zero parameter set */
	memset(sp->par2, 0, N_PARS * sizeof(dvector));

	/* Set up standard examples */
	switch (NRAND((periodicp) ? 5 : 3)) {
	case 0:
		/*
		  x' = a(y - x)
		  y' = x(b - z) - y
		  z' = xy - cz
		 */
		name = "Lorentz";
		sp->par2[Y].x = 10 + balance_rand(5*0); /* a */
		sp->par2[X].x = - sp->par2[Y].x;        /* -a */
		sp->par2[X].y = 28 + balance_rand(5*0); /* b */
		sp->par2[XZ].y = -1;
		sp->par2[Y].y = -1;
		sp->par2[XY].z = 1;
		sp->par2[Z].z = - 2 + balance_rand(1*0); /* -c */		
		break;
	case 1:
		/*
		  x' = -(y + az)
		  y' = x + by
		  z' = c + z(x - 5.7)
		 */
		name = "Rossler";
		sp->par2[Y].x = -1;
		sp->par2[Z].x = -2 + balance_rand(1); /* a */
		sp->par2[X].y = 1;
		sp->par2[Y].y = 0.2 + balance_rand(0.1); /* b */
		sp->par2[C].z = 0.2 + balance_rand(0.1); /* c */
		sp->par2[XZ].z = 1;
		sp->par2[Z].z = -5.7;
		break;
	case 2: 
		/*
		  x' = -(y + az)
		  y' = x + by - cz^2
		  z' = 0.2 + z(x - 5.7)
		 */
		name = "RosslerCone";
		sp->par2[Y].x = -1;
		sp->par2[Z].x = -2; /* a */
		sp->par2[X].y = 1;
		sp->par2[Y].y = 0.2; /* b */
		sp->par2[ZZ].y = -0.331 + balance_rand(0.01); /* c */
		sp->par2[C].z = 0.2;
		sp->par2[XZ].z = 1;
		sp->par2[Z].z = -5.7;
		break;
	case 3:
		/*
		  x' = -z + b sin(y)
		  y' = c
		  z' = 0.7x + az(0.1 - x^2) 
		 */
		name = "Birkhoff";
		sp->par2[Z].x = -1;
		sp->par2[SINY].x = 0.35 + balance_rand(0.25); /* b */
		sp->par2[C].y = 1.57; /* c */
		sp->par2[X].z = 0.7;
		sp->par2[Z].z = 1 + balance_rand(0.5); /* a/10 */
		sp->par2[XXZ].z = -10 * sp->par2[Z].z; /* -a */
		sp->yperiod = 2 * M_PI;
		break;
	default:
		/*
		  x' = -ax - z/2 - z^3/8 + b sin(y)
		  y' = c
		  z' = 2x
		 */
		name = "Duffing";
		sp->par2[X].x = -0.2 + balance_rand(0.1); /* a */
		sp->par2[Z].x = -0.5;
		sp->par2[ZZZ].x = -0.125;
		sp->par2[SINY].x = 27.0 + balance_rand(3.0); /* b */
		sp->par2[C].y = 1.33; /* c */
		sp->par2[X].z = 2;
		sp->yperiod = 2 * M_PI;
		break;

	}

	sp->range.x = 5;
	sp->range.z = 5;

	if(sp->yperiod > 0) {
		sp->ODE = Periodic;
		/* periodic flows show either uniform distribution or a
           snapshot on the 'time' axis */
		sp->range.y = NRAND(2)? sp->yperiod : 0;
	} else {
		sp->range.y = 5;
		sp->ODE = Cubic;
	}

	/* Run discoverer to set up bounding box, etc.  Lyapunov will
	   probably be innaccurate, since we're only running it once, but
	   we're using known strange attractors so it should be ok. */
	discover(mi);
	if(MI_IS_VERBOSE(mi))
		fprintf(stdout,
				"flow: Lyapunov exponent: %g, step: %g, size: %g (%s)\n",
				sp->lyap2, sp->step2, sp->size2, name);
	/* Install new params */
	sp->lyap = sp->lyap2;
	sp->size = sp->size2;
	sp->mid = sp->mid2;
	sp->step = sp->step2;
	memcpy(sp->par, sp->par2, sizeof(sp->par2));

	sp->count2 = 0; /* Reset search */

	sp->beecount = MI_COUNT(mi);
	if (!sp->beecount) {
		sp->beecount = 1; /* The camera requires 1 or more */
	} else if (sp->beecount < 0) {	/* random variations */
		sp->beecount = NRAND(-sp->beecount) + 1; /* Minimum 1 */
	}

# ifdef HAVE_JWXYZ	/* Don't second-guess Quartz's double-buffering */
  dbufp = False;
# endif

	if(dbufp) { /* Set up double buffer */
		if (sp->buffer != None)
			XFreePixmap(MI_DISPLAY(mi), sp->buffer);
		sp->buffer = XCreatePixmap(MI_DISPLAY(mi), MI_WINDOW(mi),
								 MI_WIDTH(mi), MI_HEIGHT(mi), MI_DEPTH(mi));
	} else {
		sp->buffer = MI_WINDOW(mi);
	}
	/* no "NoExpose" events from XCopyArea wanted */
	XSetGraphicsExposures(MI_DISPLAY(mi), MI_GC(mi), False);

	/* Make sure we're using 'thin' lines */
	XSetLineAttributes(MI_DISPLAY(mi), MI_GC(mi), 0, LineSolid, CapNotLast,
					   JoinMiter);

	/* Clear the background (may be slow depending on user prefs). */
	MI_CLEARWINDOW(mi);

	/* Allocate memory. */
	if (sp->csegs == NULL) {
		allocate(sp->csegs, XSegment,
				 (sp->beecount + BOX_L) * MI_NPIXELS(mi) * sp->taillen);
		allocate(sp->cnsegs, int, MI_NPIXELS(mi));
		allocate(sp->old_segs, XSegment, (sp->beecount + BOX_L) * sp->taillen);
		allocate(sp->p, dvector, sp->beecount * sp->taillen);
	}

	/* Initialize point positions, velocities, etc. */
	restart_flow(mi);

	/* Set up camera tail */
	X(1, 0) = sp->cam[1].x = 0;
	Y(1, 0) = sp->cam[1].y = 0;
	Z(1, 0) = sp->cam[1].z = 0;
}

ENTRYPOINT void
draw_flow (ModeInfo * mi)
{
	int         b, i;
	int         col, begin, end;
	double      M[3][3]; /* transformation matrix */
	flowstruct *sp = NULL;
	int         swarm = 0;
	int         segindex;

	if (flows == NULL)
		return;
	sp = &flows[MI_SCREEN(mi)];
	if (sp->csegs == NULL)
		return;

#ifdef HAVE_JWXYZ	/* Don't second-guess Quartz's double-buffering */
    XClearWindow (MI_DISPLAY(mi), MI_WINDOW(mi));
#endif

	/* multiplier for indexing segment arrays.  Used in IX macro, etc. */
	segindex = (sp->beecount + BOX_L) * sp->taillen;

	if(searchp){
		if(sp->count2 == 0) { /* start new search */
			sp->step2 = INITIALSTEP;
			 /* Pick random parameters.  Actual range is irrelevant
				since parameter scale determines flow speed but not
				structure. */
			for(i=0; i< N_PARS; i++) {
				sp->par2[i].x = Gauss_Rand(1.0);
				sp->par2[i].y = Gauss_Rand(1.0);
				sp->par2[i].z = Gauss_Rand(1.0);
			}
		}
		if(!discover(mi)) { /* Flow exploded, reset. */
			sp->count2 = 0;
		} else {
			if(sp->lyap2 < 0) {
				sp->count2 = 0; /* Attractor found, but it's not strange */
			}else if(sp->count2 > 1000000) { /* This one will do */
				sp->count2 = 0; /* Reset search */
				if(MI_IS_VERBOSE(mi))
					fprintf(stdout,
							"flow: Lyapunov exponent: %g, step: %g, size: %g (unnamed)\n",
							sp->lyap2, sp->step2, sp->size2);
				/* Install new params */
				sp->lyap = sp->lyap2;
				sp->size = sp->size2;
				sp->mid = sp->mid2;
				sp->step = sp->step2;
				memcpy(sp->par, sp->par2, sizeof(sp->par2));

				/* If we're allowed to zoom out, do so now, so that we
				   get a look at the new attractor. */
				if(sp->chaseto == BEE && rotatep) {
					sp->chaseto = ORBIT;
					sp->chasetime = 100;
				}
				/* Reset initial conditions, so we don't get
				   misleading artifacts in the particle density. */
				restart_flow(mi);
			}
		}
	}
	
	/* Reset segment buffers */
	for (col = 0; col < MI_NPIXELS(mi); col++)
		sp->cnsegs[col] = 0;

	MI_IS_DRAWN(mi) = True;

	/* Calculate circling POV [Chalky]*/
	sp->circle[1] = sp->circle[0];
	sp->circle[0].x = sp->size * 2 * sin(sp->count / 100.0) *
		(-0.6 + 0.4 *cos(sp->count / 500.0)) + sp->mid.x;
	sp->circle[0].y = sp->size * 2 * cos(sp->count / 100.0) *
		(0.6 + 0.4 *cos(sp->count / 500.0)) + sp->mid.y;
	sp->circle[0].z = sp->size * 2 * sin(sp->count / 421.0) + sp->mid.z;

	/* Timed chase instead of Chalkie's Bistable oscillator [TDA] */
	if(rotatep && ridep) {
		if(sp->chaseto == BEE && NRAND(1000) == 0){
			sp->chaseto = ORBIT;
			sp->chasetime = 100;
		}else if(NRAND(4000) == 0){
			sp->chaseto = BEE;
			sp->chasetime = 100;
		}
	}

	/* Set up orientation matrix */
	{
		double x[3], p[3], x2=0, xp=0;
		int j;
		
		/* Chasetime is here to guarantee the camera makes it all the
		   way to the target in a finite number of steps. */
		if(sp->chasetime > 1)
			sp->chasetime--;
		
		if(sp->chaseto == BEE){
			/* Camera Head targets bee 0 */
			sp->cam[0].x += (X(0, 0) - sp->cam[0].x)/sp->chasetime;
			sp->cam[0].y += (Y(0, 0) - sp->cam[0].y)/sp->chasetime;
			sp->cam[0].z += (Z(0, 0) - sp->cam[0].z)/sp->chasetime;
			
			/* Camera Tail targets previous position of bee 0 */
			sp->cam[1].x += (X(1, 0) - sp->cam[1].x)/sp->chasetime;
			sp->cam[1].y += (Y(1, 0) - sp->cam[1].y)/sp->chasetime;
			sp->cam[1].z += (Z(1, 0) - sp->cam[1].z)/sp->chasetime;
			
			/* Camera Wing targets bee 1 */
			sp->cam[2].x += (X(0, 1) - sp->cam[2].x)/sp->chasetime;
			sp->cam[2].y += (Y(0, 1) - sp->cam[2].y)/sp->chasetime;
			sp->cam[2].z += (Z(0, 1) - sp->cam[2].z)/sp->chasetime;
		} else {
			/* Camera Head targets Orbiter */
			sp->cam[0].x += (sp->circle[0].x - sp->cam[0].x)/sp->chasetime;
			sp->cam[0].y += (sp->circle[0].y - sp->cam[0].y)/sp->chasetime;
			sp->cam[0].z += (sp->circle[0].z - sp->cam[0].z)/sp->chasetime;
			
			/* Camera Tail targets diametrically opposite the middle
			   of the bounding box from the Orbiter */
			sp->cam[1].x += 
				(2*sp->circle[0].x - sp->mid.x - sp->cam[1].x)/sp->chasetime;
			sp->cam[1].y +=
				(2*sp->circle[0].y - sp->mid.y - sp->cam[1].y)/sp->chasetime;
			sp->cam[1].z +=
				(2*sp->circle[0].z - sp->mid.z - sp->cam[1].z)/sp->chasetime;
			/* Camera Wing targets previous position of Orbiter */
			sp->cam[2].x += (sp->circle[1].x - sp->cam[2].x)/sp->chasetime;
			sp->cam[2].y += (sp->circle[1].y - sp->cam[2].y)/sp->chasetime;
			sp->cam[2].z += (sp->circle[1].z - sp->cam[2].z)/sp->chasetime;
		}
		
		/* Viewpoint from Tail of camera */
		sp->centre.x=sp->cam[1].x;
		sp->centre.y=sp->cam[1].y;
		sp->centre.z=sp->cam[1].z;
		
		/* forward vector */
		x[0] = sp->cam[0].x - sp->cam[1].x;
		x[1] = sp->cam[0].y - sp->cam[1].y;
		x[2] = sp->cam[0].z - sp->cam[1].z;
		
		/* side */
		p[0] = sp->cam[2].x - sp->cam[1].x;
		p[1] = sp->cam[2].y - sp->cam[1].y;
		p[2] = sp->cam[2].z - sp->cam[1].z;


		/* So long as X and P don't collide, these can be used to form
		   three mutually othogonal axes: X, (X x P) x X and X x P.
		   After being normalised to unit length, these form the
		   Orientation Matrix. */
		
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
			if(A>0)
				for(i=0; i<3; i++) M[j][i]/=A;
		}

		if(sp->chaseto == BEE) {
			X(0, 1)=X(0, 0)+M[1][0]*sp->step; /* adjust neighbour */
			Y(0, 1)=Y(0, 0)+M[1][1]*sp->step;
			Z(0, 1)=Z(0, 0)+M[1][2]*sp->step;
		}
	}

	/* <=- Bounding Box -=> */
	if(boxp) {
		for (b = 0; b < BOX_L; b++) {

			/* Chalky's clipping code, Only used for the box */
			/* clipping trails is slow and of little benefit. [TDA] */
			int p1 = lines[b][0];
			int p2 = lines[b][1];
			dvector A1, A2;
			double x1=box[p1][0]* sp->size/2 + sp->mid.x - sp->centre.x;
			double y1=box[p1][1]* sp->size/2 + sp->mid.y - sp->centre.y;
			double z1=box[p1][2]* sp->size/2 + sp->mid.z - sp->centre.z;
			double x2=box[p2][0]* sp->size/2 + sp->mid.x - sp->centre.x;
			double y2=box[p2][1]* sp->size/2 + sp->mid.y - sp->centre.y;
			double z2=box[p2][2]* sp->size/2 + sp->mid.z - sp->centre.z;
			
			A1.x=M[0][0]*x1 + M[0][1]*y1 + M[0][2]*z1;
			A1.y=M[1][0]*x1 + M[1][1]*y1 + M[1][2]*z1;
			A1.z=M[2][0]*x1 + M[2][1]*y1 + M[2][2]*z1 + EYEHEIGHT * sp->size;
			A2.x=M[0][0]*x2 + M[0][1]*y2 + M[0][2]*z2;
			A2.y=M[1][0]*x2 + M[1][1]*y2 + M[1][2]*z2;
			A2.z=M[2][0]*x2 + M[2][1]*y2 + M[2][2]*z2 + EYEHEIGHT * sp->size;

			/* Clip in 3D before projecting down to 2D.  A 2D clip
			   after projection wouldn't be able to handle lines that
			   cross x=0 */
			if (clip(1, 0, 0,-1, &A1, &A2) || /* Screen */
				clip(1, 2, 0, 0, &A1, &A2) || /* Left */
				clip(1,-2, 0, 0, &A1, &A2) || /* Right */
				clip(1,0, 2.0*MI_WIDTH(mi)/MI_HEIGHT(mi), 0, &A1, &A2)||/*UP*/
				clip(1,0,-2.0*MI_WIDTH(mi)/MI_HEIGHT(mi), 0, &A1, &A2))/*Down*/
				continue;

			/* Colour according to bee */
			col = b % (MI_NPIXELS(mi) - 1);
			
			sp->csegs[IX(col)].x1 = MI_WIDTH(mi)/2 + MI_WIDTH(mi) * A1.y/A1.x;
			sp->csegs[IX(col)].y1 = MI_HEIGHT(mi)/2 + MI_WIDTH(mi) * A1.z/A1.x;
			sp->csegs[IX(col)].x2 = MI_WIDTH(mi)/2 + MI_WIDTH(mi) * A2.y/A2.x;
			sp->csegs[IX(col)].y2 = MI_HEIGHT(mi)/2 + MI_WIDTH(mi) * A2.z/A2.x;
			sp->cnsegs[col]++;
		}
	}		

	/* <=- Bees -=> */
	for (b = 0; b < sp->beecount; b++) {
		if(fabs(X(0, b)) > LOST_IN_SPACE ||
		   fabs(Y(0, b)) > LOST_IN_SPACE ||
		   fabs(Z(0, b)) > LOST_IN_SPACE){
			if(sp->chaseto == BEE && b == 0){
				/* Lost camera bee.  Need to replace it since
				   rerunning init_flow could lose us a hard-won new
				   attractor.  Try moving it very close to a random
				   other bee.  This way we have a good chance of being
				   close to the attractor and not forming a false
				   artifact.  If we've lost many bees this may need to
				   be repeated. */
				/* Don't worry about camera wingbee.  It stays close
				   to the main camera bee no matter what happens. */
				int newb = 1 + NRAND(sp->beecount - 1);
				X(0, 0) = X(0, newb) + 0.001;
				Y(0, 0) = Y(0, newb);
				Z(0, 0) = Z(0, newb);
				if(MI_IS_VERBOSE(mi))
					fprintf(stdout,
							"flow: resetting lost camera near bee %d\n",
							newb);
			}
			continue;
		}

		/* Age the tail.  It's critical this be fast since
		   beecount*taillen can be large. */
		memmove(B(1, b), B(0, b), (sp->taillen - 1) * sizeof(dvector));

		Iterate(B(0,b), sp->ODE, sp->par, sp->step);

		/* Don't show wingbee since he's not quite in the flow. */
		if(sp->chaseto == BEE && b == 1) continue;

		/* Colour according to bee */
		col = b % (MI_NPIXELS(mi) - 1);
		
		/* Fill the segment lists. */
		
		begin = 0; /* begin new trail */
		end = MIN(sp->taillen, sp->count); /* short trails at first */
		for(i=0; i < end; i++){
			double x = X(i,b)-sp->centre.x;
			double y = Y(i,b)*(sp->yperiod < 0? (sp->size/sp->yperiod) :1)
				-sp->centre.y;
			double z = Z(i,b)-sp->centre.z;
			double XM=M[0][0]*x + M[0][1]*y + M[0][2]*z;
			double YM=M[1][0]*x + M[1][1]*y + M[1][2]*z;
			double ZM=M[2][0]*x + M[2][1]*y + M[2][2]*z + EYEHEIGHT * sp->size;
			short absx, absy;
						
			swarm++; /* count the remaining bees */
			if(sp->yperiod > 0 && Y(i,b) > sp->yperiod){
				int j;
				Y(i,b) -= sp->yperiod;
				/* hide tail to prevent streaks in Y.  Streaks in X,Z
				   are ok, they help to outline the Poincare'
				   slice. */
				for(j = i; j < end; j++) Y(j,b) = Y(i,b);
				/*begin = i + 1;*/
				break;
			}
			
			if(XM <= 0){ /* off screen - new trail */
				begin = i + 1;
				continue;
			}
			absx = MI_WIDTH(mi)/2 + MI_WIDTH(mi) * YM/XM;
			absy = MI_HEIGHT(mi)/2 + MI_WIDTH(mi) * ZM/XM;
			/* Performance bottleneck */
			if(absx <= 0 || absx >= MI_WIDTH(mi) ||
			   absy <= 0 || absy >= MI_HEIGHT(mi)) {
				/* off screen - new trail */
				begin = i + 1;
				continue;
			}
			if(i > begin) {  /* complete previous segment */
				sp->csegs[IX(col)].x2 = absx;
				sp->csegs[IX(col)].y2 = absy;
				sp->cnsegs[col]++;
			}
			
			if(i < end -1){  /* start new segment */
				sp->csegs[IX(col)].x1 = absx;
				sp->csegs[IX(col)].y1 = absy;
			}
		}
	}

	 /* Erase */
	XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_BLACK_PIXEL(mi));
	if (dbufp) { /* In Double Buffer case, prepare off-screen copy */
		/* For slow systems, this can be the single biggest bottleneck
		   in the program.  These systems may be better of not using
		   the double buffer. */ 
		XFillRectangle(MI_DISPLAY(mi), sp->buffer, MI_GC(mi), 0, 0,
					   MI_WIDTH(mi), MI_HEIGHT(mi));
	} else { /* Otherwise, erase previous segment list directly */
		XDrawSegments(MI_DISPLAY(mi), sp->buffer, MI_GC(mi),
					  sp->old_segs, sp->nold_segs);
	}

	/* Render */
	if (MI_NPIXELS(mi) > 2){ /* colour */
		int mn  = 0;
		for (col = 0; col < MI_NPIXELS(mi) - 1; col++)
			if (sp->cnsegs[col] > 0) {
				if(sp->cnsegs[col] > mn) mn = sp->cnsegs[col];
				XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_PIXEL(mi, col+1));
				/* This is usually the biggest bottleneck on most
				   systems.  The maths load is insignificant compared
				   to this.  */
				XDrawSegments(MI_DISPLAY(mi), sp->buffer, MI_GC(mi),
							  sp->csegs + col * segindex, sp->cnsegs[col]);
			}
	} else { /* mono handled seperately since xlockmore uses '1' for
				mono white! */
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_WHITE_PIXEL(mi));
		XDrawSegments(MI_DISPLAY(mi), sp->buffer, MI_GC(mi),
					  sp->csegs, sp->cnsegs[0]);
	}
	if (dbufp) { /* In Double Buffer case, this updates the screen */
		XCopyArea(MI_DISPLAY(mi), sp->buffer, MI_WINDOW(mi), MI_GC(mi), 0, 0,
				  MI_WIDTH(mi), MI_HEIGHT(mi), 0, 0);
	} else { /* Otherwise, screen is already updated.  Copy segments
				to erase-list to be erased directly next time. */
		int c = 0;
		for (col = 0; col < MI_NPIXELS(mi) - 1; col++) {
			memcpy(sp->old_segs + c, sp->csegs + col * segindex,
				   sp->cnsegs[col] * sizeof(XSegment));
			c += sp->cnsegs[col];
		}
		sp->nold_segs = c;
	}

	if(sp->count > 1 && swarm == 0) { /* all gone */
		if(MI_IS_VERBOSE(mi))
			fprintf(stdout, "flow: all gone at %d\n", sp->count);
		init_flow(mi);
	}

	if(sp->count++ > MI_CYCLES(mi)){ /* Time's up.  If we haven't
										found anything new by now we
										should pick a new standard
										flow */
		init_flow(mi);
	}
}

ENTRYPOINT void
reshape_flow(ModeInfo * mi, int width, int height)
{
  init_flow (mi);
}


ENTRYPOINT void
refresh_flow (ModeInfo * mi)
{
	if(!dbufp) MI_CLEARWINDOW(mi);
}

ENTRYPOINT Bool
flow_handle_event (ModeInfo *mi, XEvent *event)
{
  if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      init_flow (mi);
      return True;
    }
  return False;
}


XSCREENSAVER_MODULE ("Flow", flow)

#endif /* MODE_flow */
