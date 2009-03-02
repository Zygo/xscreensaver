/* -*- Mode: C; tab-width: 4 -*- */
/* flow --- flow of strange bees */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)flow.c 4.10 98/04/24 xlockmore";

#endif

/*-
 * Copyright (c) 1996 by Tim Auckland <Tim.Auckland@Sun.COM>
 * Portions added by Stephen Davies are Copyright (c) 2000 Stephen Davies
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
 * 21-Feb-00: Major hackage by Chalky (Stephen Davies, chalky@null.net)
 *            Forced perspective mode, added 3d box around attractor which
 *            involved coding 3d-planar-clipping against the view-frustrum
 *            thingy. Also made view alternate between piggybacking on a 'bee'
 *            to zooming around outside the attractor. Most bees slow down and
 *            stop, to make the structure of the attractor more obvious.
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
					"*count:		500 \n" \
					"*cycles:		3000 \n" \
					"*ncolors:		200 \n" \
	"*rotate:         True \n" \
	"*ride:           True \n" \
	"*zoom:           True \n" \
	"*allow2d:        True \n" \
	"*box:            True \n" \
	"*slow:           True \n" \
	"*freeze:         True \n"
# define SMOOTH_COLORS
# include "xlockmore.h"		/* in xscreensaver distribution */
# include "erase.h"

#else /* STANDALONE */
# include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

XrmOptionDescRec flow_options[];
ModeSpecOpt flow_opts = { 7, flow_options, 0, NULL, NULL };

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
/*#define SCALE_Y(A) (sp->height/2+sp->height/sp->size*(A))*/
#define SCALE_Y(A) (sp->height/2+sp->width/sp->size*(A))

/* Mode of operation. Rotate, ride and zoom are mutually exclusive */
typedef enum {
	FLOW_ROTATE = 1, /* Rotate around attractor */
	FLOW_RIDE = 2,   /* Ride a trained bee */
	FLOW_ZOOM = 4,   /* Zoom in and out */
	FLOW_2D = 8,     /* Allow 2D attractors */
	FLOW_BOX = 16,    /* Compute a box around the attractor */
	FLOW_SLOW = 32,   /* Some bees are slower (and have antifreeze) */
	FLOW_FREEZE = 64, /* Freeze some of the bees in action */
} FlowMode;

#define FLOW_DEFAULT (FLOW_ROTATE|FLOW_RIDE|FLOW_ZOOM|FLOW_2D|\
		FLOW_BOX|FLOW_SLOW|FLOW_FREEZE)

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
	double		slow;
	double		slow_view;
	dvector     centre;		/* centre */
	dvector		range;
	struct {
		double  depth;
		double  height;
	}           view;
	dvector		circle[2]; /* POV that circles around the scene */
	dvector    *p[2];   /* bee positions x[time][bee#] */
	struct {
		double  theta;
		double  dtheta;
		double  phi;
		double  dphi;
	}           tumble;
	dvector  (*ODE) (Par par, double x, double y, double z);
	Par         par;
	FlowMode		mode; /* Mode of operation */
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

void init_clip(flowstruct *sp);

void
init_flow(ModeInfo * mi)
{
	flowstruct *sp;
	int         b;
	double      beemult = 1;
	static int  allocated = 0;

	if (flows == NULL) {
		if ((flows = (flowstruct *) calloc(MI_NUM_SCREENS(mi),
					       sizeof (flowstruct))) == NULL)
			return;
	}
	sp = &flows[MI_SCREEN(mi)];

	sp->count = 0;
	sp->slow = 0.999;
	sp->slow_view = 0.90;

	sp->width = MI_WIDTH(mi);
	sp->height = MI_HEIGHT(mi);

	sp->tumble.theta = balance_rand(M_PI);
	sp->tumble.phi = balance_rand(M_PI);
	sp->tumble.dtheta = 0.002;
	sp->tumble.dphi = 0.001;
	sp->view.height = 0;
	sp->view.depth = 0; /* no perspective view */
	sp->mode = 0;
   if (get_boolean_resource ("rotate", "Boolean")) sp->mode |= FLOW_ROTATE;
   if (get_boolean_resource ("ride", "Boolean")) sp->mode |= FLOW_RIDE;
   if (get_boolean_resource ("zoom", "Boolean")) sp->mode |= FLOW_ZOOM;
   if (get_boolean_resource ("allow2d", "Boolean")) sp->mode |= FLOW_2D;
   if (get_boolean_resource ("slow", "Boolean")) sp->mode |= FLOW_SLOW;
   if (get_boolean_resource ("freeze", "Boolean")) sp->mode |= FLOW_FREEZE;
   if (get_boolean_resource ("box", "Boolean")) sp->mode |= FLOW_BOX;

	b = (sp->mode & FLOW_2D) ? 5 : 3;
	b = NRAND(b);

	/* If more than one of rotate, ride and zoom are set, choose one */
	if (b < 3) {
		int num = 0, modes[3];

		if (sp->mode & FLOW_ROTATE) modes[num++] = FLOW_ROTATE;
		if (sp->mode & FLOW_RIDE) modes[num++] = FLOW_RIDE;
		if (sp->mode & FLOW_ZOOM) modes[num++] = FLOW_ZOOM;

		sp->mode &= ~(FLOW_ROTATE | FLOW_RIDE | FLOW_ZOOM);

		if (num) sp->mode |= modes[ NRAND(num) ];
		else sp->mode |= FLOW_ZOOM;
	}
	
	switch (b) {
	case 0:
		sp->view.depth = 10;
		sp->view.height = 0.2;
		beemult = 3;
		sp->ODE = Lorentz;
		sp->step = 0.02;
		sp->size = 60;
		sp->centre.x = 0;
		sp->centre.y = 0;
		sp->centre.z = 24;
		sp->range.x = 5;
		sp->range.y = 5;
		sp->range.z = 1;
		sp->par.a = 10 + balance_rand(5);
		sp->par.b = 28 + balance_rand(5);
		sp->par.c = 2 + balance_rand(1);
		break;
	case 1:
		sp->view.depth = 10;
		sp->view.height = 0.1;
		beemult = 4;
		sp->ODE = Rossler;
		sp->step = 0.05;
		sp->size = 24;
		sp->centre.x = 0;
		sp->centre.y = 0;
		sp->centre.z = 3;
		sp->range.x = 4;
		sp->range.y = 4;
		sp->range.z = 7;
		sp->par.a = 2 + balance_rand(1);
		sp->par.b = 0.2 + balance_rand(0.1);
		sp->par.c = 0.2 + balance_rand(0.1);
		break;
	case 2:
		sp->view.depth = 10;
		sp->view.height = 0.1;
		beemult = 3;
		sp->ODE = RosslerCone;
		sp->step = 0.05;
		sp->size = 24;
		sp->centre.x = 0;
		sp->centre.y = 0;
		sp->centre.z = 3;
		sp->range.x = 4;
		sp->range.y = 4;
		sp->range.z = 4;
		sp->par.a = 2;
		sp->par.b = 0.2;
		sp->par.c = 0.25 + balance_rand(0.09);
		break;
	case 3:
		sp->ODE = Birkhoff;
		sp->step = 0.04;
		sp->size = 2.6;
		sp->centre.x = 0;
		sp->centre.y = 0;
		sp->centre.z = 0;
		sp->range.x = 3;
		sp->range.y = 4;
		sp->range.z = 0;
		sp->par.a = 10 + balance_rand(5);
		sp->par.b = 0.35 + balance_rand(0.25);
		sp->par.c = 1.57;
		sp->tumble.theta = 0;
		sp->tumble.phi = 0;
		sp->tumble.dtheta = 0;
		sp->tumble.dphi = 0;
		break;
	case 4:
	default:
		sp->ODE = Duffing;
		sp->step = 0.02;
		sp->size = 30;
		sp->centre.x = 0;
		sp->centre.y = 0;
		sp->centre.z = 0;
		sp->range.x = 20;
		sp->range.y = 20;
		sp->range.z = 0;
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

	sp->view.depth *= 4;

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
		allocated = sp->beecount;
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
		X(1, b) = X(0, b) = balance_rand(sp->range.x);
		Y(1, b) = Y(0, b) = balance_rand(sp->range.y);
		Z(1, b) = Z(0, b) = balance_rand(sp->range.z);
	}

	init_clip(sp);

}

/* Clipping planes */
#define PLANES 5
static double plane_orig[][2][3] = {
	/* X goes into screen, Y goes right, Z goes down(up?) */
	/* {Normal}, {Point} */
	{ {1.0, 0, 0}, {0.01, 0, 0} },
	{ {1.0, 1.0, 0.0}, {0, 0, 0} },
	{ {1.0,-1.0, 0.0}, {0, 0, 0} },
	{ {1.0, 0.0, 1.0}, {0, 0, 0} },
	{ {1.0, 0.0,-1.0}, {0, 0, 0} }
};
static double plane[PLANES][2][3];
static double plane_d[PLANES];

#define BOX_P 32
#define BOX_L 36
#define MIN_BOX (3)
#define MAX_BOX (MIN_BOX + BOX_L)
/* Points that make up the box (normalized coordinates) */
static double box_orig[][3] = {
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

/* Container for scaled box points */
static double box[BOX_P][3];

/* Lines connecting the box dots */
static double lines[][2] = {
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
	
/* Boundaries of bees */
double xmin, xmax;
double ymin, ymax;
double zmin, zmax;

void init_clip(flowstruct *sp)
{
	int i;

	/* Scale the planes to the screen. I had to invert the projection
	 * algorithms so that when projected they would be right at the edge of the
	 * screen. */

    /* #### jwz: I'm not really sure what it means when sp->view.depth is 0
       in here -- what's the right thing to do? */

	double width = (sp->view.depth
                    ? sp->size/sp->view.depth/2
                    : 1);
	double height = (sp->view.depth
                     ? (sp->size/sp->view.depth/2*
                        sp->view.height/sp->view.height)
                     : 1);
	for (i = 0; i < PLANES; i++) {
		/* Copy orig planes into planes, expanding <-> clippings */
		plane[i][0][0] = plane_orig[i][0][0];
		plane[i][0][1] = plane_orig[i][0][1] / width;
		plane[i][0][2] = plane_orig[i][0][2] / height;
		plane[i][1][0] = plane_orig[i][1][0];
		plane[i][1][1] = plane_orig[i][1][1];
		plane[i][1][2] = plane_orig[i][1][2];
		
		/* Calculate the 'd' part of 'ax + by + cz = d' */
		plane_d[i] = - plane[i][0][0] * plane[i][1][0];
		plane_d[i] -= plane[i][0][1] * plane[i][1][1];
		plane_d[i] -= plane[i][0][2] * plane[i][1][2];
	}
	xmin = X(0, i); xmax = X(0,i);
	ymin = Y(0, i); ymax = Y(0,i);
	zmin = Z(0, i); zmax = Z(0,i);
}

/* Scale the box defined above to fit around all points */
void create_box(flowstruct *sp)
{
	int i = MAX_BOX;
	double xmid, ymid, zmid;
	double xsize, ysize, zsize;
	double size;

	/* Count every 5th point for speed.. */
	for (; i < sp->beecount; i += 5) {
		if ( X(0,i) < xmin ) xmin = X(0, i);
		else if ( X(0,i) > xmax ) xmax = X(0, i);
		if ( Y(0,i) < ymin ) ymin = Y(0, i);
		else if ( Y(0,i) > ymax ) ymax = Y(0, i);
		if ( Z(0,i) < zmin ) zmin = Z(0, i);
		else if ( Z(0,i) > zmax ) zmax = Z(0, i);
	}
	xmid = (xmax+xmin)/2;
	ymid = (ymax+ymin)/2;
	zmid = (zmax+zmin)/2;
	xsize = xmax - xmin;
	ysize = ymax - ymin;
	zsize = zmax - zmin;
	size = xsize;
	if (ysize> size) size = ysize;
	if (zsize > size) size = zsize;
	size /= 2;

	/* Scale box */
	for (i = 0; i < BOX_P; i++) {
		box[i][0] = box_orig[i][0] * size + xmid;
		box[i][1] = box_orig[i][1] * size + ymid;
		box[i][2] = box_orig[i][2] * size + zmid;
	}

}

/* Returns true if point is infront of the plane (rather than behind) */
int infront_of(double x, double y, double z, int i)
{
	double sum = plane[i][0][0]*x + plane[i][0][1]*y + plane[i][0][2]*z + plane_d[i];
	return sum >= 0.0;
}

/* Returns true if line was behind a clip plane, or clips the line */
int clip(double *x1, double *y1, double *z1, double *x2, double *y2, double *z2)
{
	int i;
	for (i = 0; i < PLANES; i++) {
		double t;
		double x, y, z; /* Intersection point */
		double dx, dy, dz; /* line delta */
		int front1, front2;
		front1 = infront_of(*x1, *y1, *z1, i);
		front2 = infront_of(*x2, *y2, *z2, i);
		if (!front1 && !front2) return 1;
		if (front1 && front2) continue;

		dx = *x2 - *x1;
		dy = *y2 - *y1;
		dz = *z2 - *z1;

		/* Find t in line equation */
		t = ( plane_d[i] - 
				plane[i][0][0]*(*x1) - plane[i][0][1]*(*y1) - plane[i][0][2]*(*z1) ) 
				/ 
			( plane[i][0][0]*dx + plane[i][0][1]*dy + plane[i][0][2]*dz );

		x = *x1 + dx * t;
		y = *y1 + dy * t;
		z = *z1 + dz * t;
		/* Make point that was behind to be the intersect */
		if (front2) {
			*x1 = x;
			*y1 = y;
			*z1 = z;
		} else {
			*x2 = x;
			*y2 = y;
			*z2 = z;
		}
	}
	return 0;
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
	int			new_view = 0;
	double      M[3][3]; /* transformation matrix */
	double		step_view = sp->step;
	double		step_bees = sp->step;
	double		step_slow = sp->step;
	double		pp, pc;

	create_box(sp);

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

	/* Calculate circling POV */
	sp->circle[1] = sp->circle[0];
	sp->circle[0].x = sp->size * 2 * sin(sp->count / 40.0) * (0.6 + 0.4 *cos(sp->count / 100.0));
	sp->circle[0].y = sp->size * 2 * cos(sp->count / 40.0) * (0.6 + 0.4 *cos(sp->count / 100.0));
	sp->circle[0].z = sp->size * 2 * sin(sp->count / 421.0);

	if (sp->mode & FLOW_ROTATE)
		pp = 0;
	else if (sp->mode & FLOW_RIDE)
		pp = 1;
	else /* ZOOM */
		/* Bistable oscillator to switch between the trained bee and the circler */
		pp = -sin(sin(sin(cos(sp->count / 150.0)*M_PI/2)*M_PI/2)*M_PI/2) *0.5 + 0.5;
	pc = 1 - pp;


	/* Slow down or speed up the bees / view: */

	/* exponentially accelerate towards zero */
	sp->slow = sp->slow * 1.005 - 0.005; 
	if (sp->slow < 0) sp->slow = 0;

	sp->slow_view = sp->slow_view * 1.005 - 0.005;
	if (sp->slow_view < 0) sp->slow_view = 0;

	/* View speeds up, slow bees slow to half speed, and other bees will
	 * actually stop */
	step_view = step_view * (1.01 - sp->slow_view * sp->slow_view) * 0.2;
	step_slow = step_slow * (sp->slow + 0.5) / 2;
	if (sp->mode & FLOW_SLOW)
		step_bees = step_bees * sp->slow;
	else
		step_bees = step_slow;

	/* <=- Bees -=> */
	for (b = 0; b < sp->beecount; b++) {
		/* Calc if this bee is slow. Note normal bees are exempt from
		 * calculations once they slow to half speed, so that they remain as
		 * frozen lines rather than barely-visible points */
		int slow = ((b & 0x7) == 0);
		if ( !(sp->mode & FLOW_FREEZE) ) slow = 1;
		/* Age the arrays. */
		if (b < 2 || sp->slow > 0.5 || slow) {
			X(1, b) = X(0, b);
			Y(1, b) = Y(0, b);
			Z(1, b) = Z(0, b);

			/* 2nd order Kunge Kutta */
			{
				dvector     k1, k2;
				double		step;

				if (b == 0 || b == 1) {
					step = step_view;
				} else if (slow) {
					step = step_slow;
				} else {
					step = step_bees;
				}
				k1 = sp->ODE(sp->par, X(1, b), Y(1, b), Z(1, b));
				k1.x *= step;
				k1.y *= step;
				k1.z *= step;
				k2 = sp->ODE(sp->par, X(1, b) + k1.x, Y(1, b) + k1.y, Z(1, b) + k1.z);
				k2.x *= step;
				k2.y *= step;
				k2.z *= step;
				X(0, b) = X(1, b) + (k1.x + k2.x) / 2.0;
				Y(0, b) = Y(1, b) + (k1.y + k2.y) / 2.0;
				Z(0, b) = Z(1, b) + (k1.z + k2.z) / 2.0;
			}
		}


		/* Colour according to bee */
		col = b % (MI_NPIXELS(mi) - 1);
		ix = col * sp->beecount + sp->cnsegs[col];

		/* Fill the segment lists. */

		if(sp->view.depth) { /* perspective view has special points */
			if(b==0){ /* point of view */
				sp->centre.x = X(0, b) * pp + sp->circle[0].x * pc;
				sp->centre.y = Y(0, b) * pp + sp->circle[0].y * pc;
				sp->centre.z = Z(0, b) * pp + sp->circle[0].z * pc;
				/*printf("center: (%3.3f,%3.3f,%3.3f)\n",sp->centre.x, sp->centre.y, sp->centre.z);*/
			}else if(b==1){ /* neighbour: used to compute local axes */
				double x[3], p[3], x2=0, xp=0, C[3][3];
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

				/* Look at trained bee into C matrix */
				/* forward */				
				x[0] = 0 - sp->circle[0].x;
				x[1] = 0 - sp->circle[0].y;
				x[2] = 0 - sp->circle[0].z;
			
				/* neighbour */
				p[0] = sp->circle[0].x - sp->circle[1].x;
				p[1] = sp->circle[0].y - sp->circle[1].y;
				p[2] = sp->circle[0].z - sp->circle[1].z;

				for(i=0; i<3; i++){
					x2+= x[i]*x[i];    /* X . X */
					xp+= x[i]*p[i];    /* X . P */
					C[0][i] = x[i];    /* X */
				}

				for(i=0; i<3; i++)               /* (X x P) x X */
					C[1][i] = x2*p[i] - xp*x[i]; /* == (X . X) P - (X . P) X */
				
				C[2][0] =  x[1]*p[2] - x[2]*p[1]; /* X x P */
				C[2][1] = -x[0]*p[2] + x[2]*p[0];
				C[2][2] =  x[0]*p[1] - x[1]*p[0];

				/* normalise axes */
				for(j=0; j<3; j++){
					double A=0;
					for(i=0; i<3; i++) A+=C[j][i]*C[j][i]; /* sum squares */
					A=sqrt(A);
					for(i=0; i<3; i++) C[j][i]/=A;
				}

				/* Interpolate between Center and Trained Bee matrices */
				/* This isn't very accurate and leads to weird transformations
				 * (shearing, etc), but it works. Besides, sometimes they look
				 * cool :) */
				pp = pp * pp; /* Don't follow bee's direction until very close */
				pc = 1 - pp;
				for (i = 0; i < 3; i++)
					for (j = 0; j < 3; j++)
						M[i][j] = M[i][j] * pp + C[i][j] * pc;
				

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
			/* Draw a box... */
			}
		}
			if (b >= MIN_BOX && b < MAX_BOX) {
				int p1 = lines[b-MIN_BOX][0];
				int p2 = lines[b-MIN_BOX][1];
				X(0, b) = box[p1][0]; Y(0, b) = box[p1][1]; Z(0, b) = box[p1][2];
				X(1, b) = box[p2][0]; Y(1, b) = box[p2][1]; Z(1, b) = box[p2][2];
			}
		
		
#if 0	/* Original code */
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
#else	
		/* Chalky's code w/ clipping */
		if (b < ((sp->mode & FLOW_BOX) ? 2 : MAX_BOX))
			continue;
		do {
			double x1=X(0,b)-sp->centre.x;
			double y1=Y(0,b)-sp->centre.y;
			double z1=Z(0,b)-sp->centre.z;
			double X1=M[0][0]*x1 + M[0][1]*y1 + M[0][2]*z1;
			double Y1=M[1][0]*x1 + M[1][1]*y1 + M[1][2]*z1;
			double Z1=M[2][0]*x1 + M[2][1]*y1 + M[2][2]*z1+sp->view.height;
			double absx1, absy1;				
			double x2=X(1,b)-sp->centre.x;
			double y2=Y(1,b)-sp->centre.y;
			double z2=Z(1,b)-sp->centre.z;
			double X2=M[0][0]*x2 + M[0][1]*y2 + M[0][2]*z2;
			double Y2=M[1][0]*x2 + M[1][1]*y2 + M[1][2]*z2;
			double Z2=M[2][0]*x2 + M[2][1]*y2 + M[2][2]*z2+sp->view.height;
			double absx2, absy2;				
			if(sp->view.depth){
				/* Need clipping if: is part of box, or close to viewer */
				if ( (b >= MIN_BOX && b < MAX_BOX) || X1 <= 0.1 || X2 < 0.1) 
					if (clip(&X1, &Y1, &Z1, &X2, &Y2, &Z2))
						break;
				if (X1 <= 0 || X2 <= 0) break;
				absx1=SCALE_X(sp->view.depth*Y1/X1);
				absy1=SCALE_Y(sp->view.depth*Z1/X1);
				if(absx1 < -sp->width || absx1 > 2*sp->width ||
				   absy1 < -sp->height || absy1 > 2*sp->height)
					break;
				absx2=SCALE_X(sp->view.depth*Y2/X2);
				absy2=SCALE_Y(sp->view.depth*Z2/X2);
				if(absx2 < -sp->width || absx2 > 2*sp->width ||
				   absy2 < -sp->height || absy2 > 2*sp->height)
					break;
			}else{
				absx1=SCALE_X(X1);
				absy1=SCALE_Y(Y1);
				absx2=SCALE_X(X2);
				absy2=SCALE_Y(Y2);
			}

			sp->csegs[ix].x1 = (short) absx1;
			sp->csegs[ix].y1 = (short) absy1;
			sp->csegs[ix].x2 = (short) absx2;
			sp->csegs[ix].y2 = (short) absy2;

			sp->cnsegs[col]++;
		} while (0);
#endif
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

	if (sp->count % (MI_CYCLES(mi)/4) == 0) { /* pick a new view */
		new_view = 0; /* change to 1 .. */
	}

	if (X(0, 0) < xmin*2 || X(0, 0) > xmax*2) new_view = 1;
	if (Y(0, 0) < ymin*2 || Y(0, 0) > ymax*2) new_view = 1;
	if (Z(0, 0) < zmin*2 || Z(0, 0) > zmax*2) new_view = 1;

	if (new_view) {
		for (b = 0; b < 2; b++) {
			X(1, b) = X(0, b) = balance_rand(sp->range.x*4);
			Y(1, b) = Y(0, b) = balance_rand(sp->range.y*4);
			Z(1, b) = Z(0, b) = balance_rand(sp->range.z*4);
		}
		sp->slow_view = 0.90;
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

XrmOptionDescRec flow_options[] =
{
	{"-rotate",  ".rotate", XrmoptionSepArg, 0},
	{"-ride",  ".ride", XrmoptionSepArg, 0},
	{"-zoom",  ".zoom", XrmoptionSepArg, 0},
	{"-box",  ".box", XrmoptionSepArg, 0},
	{"-slow",  ".slow", XrmoptionSepArg, 0},
	{"-freeze",  ".freeze", XrmoptionSepArg, 0},
	{"-allow2d",  ".allow2d", XrmoptionSepArg, 0},
  { 0, 0, 0, 0 }
};

/*
char*        defaults[] =
{
	"*rotate:         True",
	"*ride:           True",
	"*zoom:           True",
	"*allow2d:        True",
	"*box:            True",
	"*slow:           True",
	"*freeze:         True",
  0
};
	*/
