/* -*- Mode: C; tab-width: 4 -*-
   Ported from xlockmore 4.03a12 to be a standalone program and thus usable
   with xscreensaver by Jamie Zawinski <jwz@jwz.org> on 15-May-97.

   Original copyright notice from xlock.c:

    * Copyright (c) 1988-91 by Patrick J. Naughton.
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
 */

#if 0
static const char sccsid[] = "@(#)bouboule.c	4.00 97/01/01 xlockmore";
#endif

/*-
 * bouboule.c (bouboule mode for xlockmore)
 *
 * Sort of starfield for xlockmore. I found that making a starfield for
 * a 3D engine and thought it could be a nice lock mode. For a real starfield,
 * I only scale the sort of sphere you see to the whole sky and clip the stars
 * to the camera screen.
 *
 *   Code Copyright 1996 by Jeremie PETIT (jeremie_petit@geocities.com)
 *
 *   Use: batchcount is the number of stars.
 *        cycles is the maximum size for a star
 *
 * 15-May-97: jwz@jwz.org: turned into a standalone program.
 * 04-Sep-96: Added 3d support (Henrik Theiling, theiling@coli-uni-sb.de)
 * 20-Feb-96: Added tests so that already malloced objects are not
 *            malloced twice, thanks to the report from <mccomb@interport.net>
 * 01-Feb-96: Patched by Jouk Jansen <joukj@alpha.chem.uva.nl> for VMS
 *            Patched by <bagleyd@bigfoot.com> for TrueColor displays
 * 30-Jan-96: Wrote all that I wanted to.
 *
 * DONE: Build up a XArc list and Draw everything once with XFillArcs
 *       That idea came from looking at swarm code.
 * DONE: Add an old arcs list for erasing.
 * DONE: Make center of starfield SinVariable.
 * DONE: Add some random in the sinvary() function.
 * DONE: check time for erasing the stars with the two methods and use the
 *       better one. Note that sometimes the time difference between
 *       beginning of erasing and its end is negative! I check this, and
 *       do not use this result when it occurs. If all values are negative,
 *       the erasing will continue being done in the currently tested mode.
 * DONE: Allow stars size customization.
 * DONE: Make sizey be no less than half sizex or no bigger than twice sizex.
 *
 * IDEA: A simple check can be performed to know which stars are "behind"
 *       and which are "in front". So is possible to very simply change
 *       the drawing mode for these two sorts of stars. BUT: this would lead
 *       to a rewrite of the XArc list code because drawing should be done
 *       in two steps: "behind" stars then "in front" stars. Also, what could
 *       be the difference between the rendering of these two types of stars?
 * IDEA: Calculate the distance of each star to the "viewer" and render the
 *       star accordingly to this distance. Same remarks as for previous
 *       ideas can be pointed out. This would even lead to reget the old stars
 *       drawing code, that has been replaced by the XFillArcs. On another
 *       hand, this would allow particular stars (own color, shape...), as
 *       far as they would be individually drawn. One should be careful to
 *       draw them according to their distance, that is not drawing a far
 *       star after a close one.
 */

#ifdef STANDALONE
# define DEFAULTS	"*count:		100     \n"			\
					"*size:			15      \n"			\
					"*delay:		20000   \n"			\
					"*ncolors:		64      \n"			\
					"*use3d:		False   \n"			\
					"*delta3d:		1.5		\n"			\
					"*right3d:		red		\n"			\
					"*left3d:		blue	\n"			\
					"*both3d:		magenta	\n"			\
					"*none3d:		black	\n"			\
					"*fpsSolid:		true	\n"			\
					"*ignoreRotation: True  \n"

# define SMOOTH_COLORS
# define bouboule_handle_event 0
# include "xlockmore.h"				/* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"					/* from the xlockmore distribution */
# define ENTRYPOINT /**/
#endif /* !STANDALONE */

ENTRYPOINT ModeSpecOpt bouboule_opts = {
  0, NULL, 0, NULL, NULL };

#define USEOLDXARCS  1		/* If 1, we use old xarcs list for erasing.
				   * else we just roughly erase the window.
				   * This mainly depends on the number of stars,
				   * because when they are many, it is faster to
				   * erase the whole window than to erase each star
				 */

#if HAVE_GETTIMEOFDAY
#define ADAPT_ERASE  1		/* If 1, then we try ADAPT_CHECKS black XFillArcs,
				   * and after, ADAPT_CHECKS XFillRectangle.
				   * We check which method seems better, knowing that
				   * XFillArcs is generally visually better. So we
				   * consider that XFillArcs is still better if its time
				   * is about XFillRectangle * ADAPT_ARC_PREFERED
				   * We need gettimeofday
				   * for this... Does it exist on other systems ? Do we
				   * have to use another function for others ?
				   * This value overrides USEOLDXARCS.
				 */

#ifdef USE_XVMSUTILS
# if 0
#  include "../xvmsutils/unix_time.h"
# else
#  include <X11/unix_time.h>
# endif
#endif

#include <sys/time.h>

#define ADAPT_CHECKS 50
#define ADAPT_ARC_PREFERED 150	/* Maybe the value that is the most important
				   * for adapting to a system */
#endif

#define dtor(x)    (((x) * M_PI) / 180.0)	/* Degrees to radians */

#define MINSTARS      1
#define MINSIZE       1
/* jwz: I think slower color changes look better */
#define COLOR_CHANGES 50		/* How often we change colors (1 = always)
				   * This value should be tuned accordingly to
				   * the number of stars */
#define MAX_SIZEX_SIZEY 2.	/* This controls whether the sphere can be very
				   * very large and have a small height (or the
				   * opposite) or no. */

#define THETACANRAND  80	/* percentage of changes for the speed of
				   * change of the 3 theta values */
#define SIZECANRAND   80	/* percentage of changes for the speed of
				   * change of the sizex and sizey values */
#define POSCANRAND    80	/* percentage of changes for the speed of
				   * change of the x and y values */
/* Note that these XXXCANRAND values can be 0, that is no rand acceleration *
   variation. */

#define VARRANDALPHA (NRAND((int) (M_PI * 1000.0))/1000.0)
#define VARRANDSTEP  (M_PI/(NRAND(100)+100.0))
#define VARRANDMIN   (-70.0)
#define VARRANDMAX   70.0

#define MINZVAL   100		/* stars can come this close */
#define SCREENZ  2000		/* this is where the screen is */
#define MAXZVAL 10000		/* stars can go this far away */

#define GETZDIFF(z) ((MI_DELTA3D(mi))*20.0*(1.0-(SCREENZ)/(z+1000)))
#define MAXDIFF  MAX(-GETZDIFF(MINZVAL),GETZDIFF(MAXZVAL))

/* These values are the variation parameters of the acceleration variation *
   of the SinVariables that are randomized. */

/******************************/
typedef struct SinVariableStruct
/******************************/
{
	double      alpha;	/* 
				 * Alpha is the current state of the sinvariable
				 * alpha should be initialized to a value between
				 * 0.0 and 2 * M_PI
				 */
	double      step;	/*
				 * Speed of evolution of alpha. It should be a reasonable
				 * fraction of 2 * M_PI. This value directly influence
				 * the variable speed of variation.
				 */
	double      minimum;	/* Minimum value for the variable */
	double      maximum;	/* Maximum value for the variable */
	double      value;	/* Current value */
	int         mayrand;	/* Flag for knowing whether some randomization can be
				 * applied to the variable */
	struct SinVariableStruct *varrand;	/* Evolving Variable: the variation of
						   * alpha */
} SinVariable;

/***********************/
typedef struct StarStruct
/***********************/
{
	double      x, y, z;	/* Position of the star */
	short       size;	/* Try to guess */
} Star;

/****************************/
typedef struct StarFieldStruct
/****************************/
{
	short       width, height;	/* width and height of the starfield window */
	short       max_star_size;	/* Maximum radius for stars. stars radius will
					 * vary from 1 to MAX_STAR_SIZE */
	SinVariable x;		/* Evolving variables:               */
	SinVariable y;		/* Center of the field on the screen */
	SinVariable z;
	SinVariable sizex;	/* Evolving variable: half width  of the field */
	SinVariable sizey;	/* Evolving variable: half height of the field */
	SinVariable thetax;	/* Evolving Variables:              */
	SinVariable thetay;	/* rotation angles of the starfield */
	SinVariable thetaz;	/* around x, y and z local axis     */
	Star       *star;	/* List of stars */
	XArc       *xarc;	/* Current List of arcs */
	XArc       *xarcleft;	/* additional list for the left arcs */
#if ((USEOLDXARCS == 1) || (ADAPT_ERASE == 1))
	XArc       *oldxarc;	/* Old list of arcs */
	XArc       *oldxarcleft;
#endif
	unsigned long color;	/* Current color of the starfield */
	int         colorp;	/* Pointer to color of the starfield */
	int         NbStars;	/* Number of stars */
	short       colorchange;	/* Counter for the color change */
#if (ADAPT_ERASE == 1)
	short       hasbeenchecked;
	long        rect_time;
	long        xarc_time;
#endif
} StarField;

static StarField *starfield = NULL;

/*********/
static void
sinvary(SinVariable * v)
/*********/

{
	v->value = v->minimum +
		(v->maximum - v->minimum) * (sin(v->alpha) + 1.0) / 2.0;

	if (v->mayrand == 0)
		v->alpha += v->step;
	else {
		int         vaval = NRAND(100);

		if (vaval <= v->mayrand)
			sinvary(v->varrand);
		v->alpha += (100.0 + (v->varrand->value)) * v->step / 100.0;
	}

	if (v->alpha > 2 * M_PI)
		v->alpha -= 2 * M_PI;
}

/*************************************************/
static void
sininit(SinVariable * v,
	double alpha, double step, double minimum, double maximum,
	short int mayrand)
{
	v->alpha = alpha;
	v->step = step;
	v->minimum = minimum;
	v->maximum = maximum;
	v->mayrand = mayrand;
	if (mayrand != 0) {
		if (v->varrand == NULL)
			v->varrand = (SinVariable *) calloc(1, sizeof (SinVariable));
		sininit(v->varrand,
			VARRANDALPHA,
			VARRANDSTEP,
			VARRANDMIN,
			VARRANDMAX,
			0);
		sinvary(v->varrand);
	}
	/* We calculate the values at least once for initialization */
	sinvary(v);
}

static void
sinfree(SinVariable * point)
{
	SinVariable *temp, *next;

	next = point->varrand;
	while (next) {
		temp = next;
		next = temp->varrand;
		(void) free((void *) temp);
	}
}


/***************/
ENTRYPOINT void
init_bouboule(ModeInfo * mi)
/***************/

/*-
 *  The stars init part was first inspirated from the net3d game starfield
 * code.  But net3d starfield is not really 3d starfield, and I needed real 3d,
 * so only remains the net3d starfield initialization main idea, that is
 * the stars distribution on a sphere (theta and omega computing)
 */
{
	StarField  *sp;
	int         size = MI_SIZE(mi);
	int         i;
	double      theta, omega;

	if (starfield == NULL) {
		if ((starfield = (StarField *) calloc(MI_NUM_SCREENS(mi),
						sizeof (StarField))) == NULL)
			return;
	}
	sp = &starfield[MI_SCREEN(mi)];

	sp->width = MI_WIN_WIDTH(mi);
	sp->height = MI_WIN_HEIGHT(mi);

	/* use the right `black' pixel values: */
	if (MI_WIN_IS_INSTALL(mi) && MI_WIN_IS_USE3D(mi)) {
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_NONE_COLOR(mi));
		XFillRectangle(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi),
			       0, 0, sp->width, sp->height);
	} else
		XClearWindow(MI_DISPLAY(mi), MI_WINDOW(mi));

	if (size < -MINSIZE)
		sp->max_star_size = NRAND(-size - MINSIZE + 1) + MINSIZE;
	else if (size < MINSIZE)
		sp->max_star_size = MINSIZE;
	else
		sp->max_star_size = size;

	sp->NbStars = MI_BATCHCOUNT(mi);
	if (sp->NbStars < -MINSTARS) {
		if (sp->star) {
			(void) free((void *) sp->star);
			sp->star = NULL;
		}
		if (sp->xarc) {
			(void) free((void *) sp->xarc);
			sp->xarc = NULL;
		}
		if (sp->xarcleft) {
			(void) free((void *) sp->xarcleft);
			sp->xarcleft = NULL;
		}
#if ((USEOLDXARCS == 1) || (ADAPT_ERASE == 1))
		if (sp->oldxarc) {
			(void) free((void *) sp->oldxarc);
			sp->oldxarc = NULL;
		}
		if (sp->oldxarcleft) {
			(void) free((void *) sp->oldxarcleft);
			sp->oldxarcleft = NULL;
		}
#endif
		sp->NbStars = NRAND(-sp->NbStars - MINSTARS + 1) + MINSTARS;
	} else if (sp->NbStars < MINSTARS)
		sp->NbStars = MINSTARS;

	/* We get memory for lists of objects */
	if (sp->star == NULL)
		sp->star = (Star *) malloc(sp->NbStars * sizeof (Star));
	if (sp->xarc == NULL)
		sp->xarc = (XArc *) malloc(sp->NbStars * sizeof (XArc));
	if (MI_WIN_IS_USE3D(mi) && sp->xarcleft == NULL)
		sp->xarcleft = (XArc *) malloc(sp->NbStars * sizeof (XArc));
#if ((USEOLDXARCS == 1) || (ADAPT_ERASE == 1))
	if (sp->oldxarc == NULL)
		sp->oldxarc = (XArc *) malloc(sp->NbStars * sizeof (XArc));
	if (MI_WIN_IS_USE3D(mi) && sp->oldxarcleft == NULL)
		sp->oldxarcleft = (XArc *) malloc(sp->NbStars * sizeof (XArc));
#endif

	{
		/* We initialize evolving variables */
		sininit(&sp->x,
			NRAND(3142) / 1000.0, M_PI / (NRAND(100) + 100.0),
			((double) sp->width) / 4.0,
			3.0 * ((double) sp->width) / 4.0,
			POSCANRAND);
		sininit(&sp->y,
			NRAND(3142) / 1000.0, M_PI / (NRAND(100) + 100.0),
			((double) sp->height) / 4.0,
			3.0 * ((double) sp->height) / 4.0,
			POSCANRAND);

		/* for z, we have to ensure that the bouboule does not get behind */
		/* the eyes of the viewer.  His/Her eyes are at 0.  Because the */
		/* bouboule uses the x-radius for the z-radius, too, we have to */
		/* use the x-values. */
		sininit(&sp->z,
			NRAND(3142) / 1000.0, M_PI / (NRAND(100) + 100.0),
			((double) sp->width / 2.0 + MINZVAL),
			((double) sp->width / 2.0 + MAXZVAL),
			POSCANRAND);


		sininit(&sp->sizex,
			NRAND(3142) / 1000.0, M_PI / (NRAND(100) + 100.0),
			MIN(((double) sp->width) - sp->x.value,
			    sp->x.value) / 5.0,
			MIN(((double) sp->width) - sp->x.value,
			    sp->x.value),
			SIZECANRAND);

		sininit(&sp->sizey,
			NRAND(3142) / 1000.0, M_PI / (NRAND(100) + 100.0),
			MAX(sp->sizex.value / MAX_SIZEX_SIZEY,
			    sp->sizey.maximum / 5.0),
			MIN(sp->sizex.value * MAX_SIZEX_SIZEY,
			    MIN(((double) sp->height) -
				sp->y.value,
				sp->y.value)),
			SIZECANRAND);

		sininit(&sp->thetax,
			NRAND(3142) / 1000.0, M_PI / (NRAND(200) + 200.0),
			-M_PI, M_PI,
			THETACANRAND);
		sininit(&sp->thetay,
			NRAND(3142) / 1000.0, M_PI / (NRAND(200) + 200.0),
			-M_PI, M_PI,
			THETACANRAND);
		sininit(&sp->thetaz,
			NRAND(3142) / 1000.0, M_PI / (NRAND(400) + 400.0),
			-M_PI, M_PI,
			THETACANRAND);
	}
	for (i = 0; i < sp->NbStars; i++) {
		Star       *star;
		XArc       *arc = NULL, *arcleft = NULL;
#if ((USEOLDXARCS == 1) || (ADAPT_ERASE == 1))
	XArc       *oarc = NULL, *oarcleft = NULL;
#endif

		star = &(sp->star[i]);
		arc = &(sp->xarc[i]);
		if (MI_WIN_IS_USE3D(mi))
			arcleft = &(sp->xarcleft[i]);
#if ((USEOLDXARCS == 1) || (ADAPT_ERASE == 1))
		oarc = &(sp->oldxarc[i]);
		if (MI_WIN_IS_USE3D(mi))
			oarcleft = &(sp->oldxarcleft[i]);
#endif
		/* Elevation and bearing of the star */
		theta = dtor((NRAND(1800)) / 10.0 - 90.0);
		omega = dtor((NRAND(3600)) / 10.0 - 180.0);

		/* Stars coordinates in a 3D space */
		star->x = cos(theta) * sin(omega);
		star->y = sin(omega) * sin(theta);
		star->z = cos(omega);

		/* We set the stars size */
		star->size = NRAND(2 * sp->max_star_size);
		if (star->size < sp->max_star_size)
			star->size = 0;
		else
			star->size -= sp->max_star_size;

		/* We set default values for the XArc lists elements */
		arc->x = arc->y = 0;
		if (MI_WIN_IS_USE3D(mi)) {
			arcleft->x = arcleft->y = 0;
		}
#if ((USEOLDXARCS == 1) || (ADAPT_ERASE == 1))
		oarc->x = oarc->y = 0;
		if (MI_WIN_IS_USE3D(mi)) {
			oarcleft->x = oarcleft->y = 0;
		}
#endif
		arc->width = 2 + star->size;
		arc->height = 2 + star->size;
		if (MI_WIN_IS_USE3D(mi)) {
			arcleft->width = 2 + star->size;
			arcleft->height = 2 + star->size;
		}
#if ((USEOLDXARCS == 1) || (ADAPT_ERASE == 1))
		oarc->width = 2 + star->size;
		oarc->height = 2 + star->size;
		if (MI_WIN_IS_USE3D(mi)) {
			oarcleft->width = 2 + star->size;
			oarcleft->height = 2 + star->size;
		}
#endif

		arc->angle1 = 0;
		arc->angle2 = 360 * 64;
		if (MI_WIN_IS_USE3D(mi)) {
			arcleft->angle1 = 0;
			arcleft->angle2 = 360 * 64;
		}
#if ((USEOLDXARCS == 1) || (ADAPT_ERASE == 1))
		oarc->angle1 = 0;
		oarc->angle2 = 360 * 64;	/* ie. we draw whole disks:
						 * from 0 to 360 degrees */
		if (MI_WIN_IS_USE3D(mi)) {
			oarcleft->angle1 = 0;
			oarcleft->angle2 = 360 * 64;
		}
#endif
	}

	if (MI_NPIXELS(mi) > 2)
		sp->colorp = NRAND(MI_NPIXELS(mi));
	/* We set up the starfield color */
	if (!MI_WIN_IS_USE3D(mi) && MI_NPIXELS(mi) > 2)
		sp->color = MI_PIXEL(mi, sp->colorp);
	else
		sp->color = MI_WIN_WHITE_PIXEL(mi);

#if (ADAPT_ERASE == 1)
	/* We initialize the adaptation code for screen erasing */
	sp->hasbeenchecked = ADAPT_CHECKS * 2;
	sp->rect_time = 0;
	sp->xarc_time = 0;
#endif
}

/****************/
ENTRYPOINT void
draw_bouboule(ModeInfo * mi)
/****************/

{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	StarField  *sp = &starfield[MI_SCREEN(mi)];
	int         i, diff = 0;
	double      CX, CY, CZ, SX, SY, SZ;
	Star       *star;
	XArc       *arc = NULL, *arcleft = NULL;

#ifdef HAVE_COCOA	/* Don't second-guess Quartz's double-buffering */
    XClearWindow(MI_DISPLAY(mi), MI_WINDOW(mi));
#endif

#if (ADAPT_ERASE == 1)
	struct timeval tv1;
	struct timeval tv2;

#endif

#if ((USEOLDXARCS == 0) || (ADAPT_ERASE == 1))
	short       x_1, y_1, x_2, y_2;

	/* bounding rectangle around the old starfield,
	 * for erasing with the smallest rectangle
	 * instead of filling the whole screen */
	int         maxdiff = 0;	/* maximal distance between left and right */

	/* star in 3d mode, otherwise 0 */
#endif

#if ((USEOLDXARCS == 0) || (ADAPT_ERASE == 1))
	if (MI_WIN_IS_USE3D(mi)) {
		maxdiff = (int) MAXDIFF;
	}
	x_1 = (int) sp->x.value - (int) sp->sizex.value -
		sp->max_star_size - maxdiff;
	y_1 = (int) sp->y.value - (int) sp->sizey.value -
		sp->max_star_size;
	x_2 = 2 * ((int) sp->sizex.value + sp->max_star_size + maxdiff);
	y_2 = 2 * ((int) sp->sizey.value + sp->max_star_size);
#endif
	/* We make variables vary. */
	sinvary(&sp->thetax);
	sinvary(&sp->thetay);
	sinvary(&sp->thetaz);

	sinvary(&sp->x);
	sinvary(&sp->y);
	if (MI_WIN_IS_USE3D(mi))
		sinvary(&sp->z);

	/* A little trick to prevent the bouboule from being
	 * bigger than the screen */
	sp->sizex.maximum =
		MIN(((double) sp->width) - sp->x.value,
		    sp->x.value);
	sp->sizex.minimum = sp->sizex.maximum / 3.0;

	/* Another trick to make the ball not too flat */
	sp->sizey.minimum =
		MAX(sp->sizex.value / MAX_SIZEX_SIZEY,
		    sp->sizey.maximum / 3.0);
	sp->sizey.maximum =
		MIN(sp->sizex.value * MAX_SIZEX_SIZEY,
		    MIN(((double) sp->height) - sp->y.value,
			sp->y.value));

	sinvary(&sp->sizex);
	sinvary(&sp->sizey);

	/*
	 * We calculate the rotation matrix values. We just make the
	 * rotation on the fly, without using a matrix.
	 * Star positions are recorded as unit vectors pointing in various
	 * directions. We just make them all rotate.
	 */
	CX = cos(sp->thetax.value);
	SX = sin(sp->thetax.value);
	CY = cos(sp->thetay.value);
	SY = sin(sp->thetay.value);
	CZ = cos(sp->thetaz.value);
	SZ = sin(sp->thetaz.value);

	for (i = 0; i < sp->NbStars; i++) {
		star = &(sp->star[i]);
		arc = &(sp->xarc[i]);
		if (MI_WIN_IS_USE3D(mi)) {
			arcleft = &(sp->xarcleft[i]);
			/* to help the eyes, the starfield is always as wide as */
			/* deep, so .sizex.value can be used. */
			diff = (int) GETZDIFF(sp->sizex.value *
				      ((SY * CX) * star->x + (SX) * star->y +
				       (CX * CY) * star->z) + sp->z.value);
		}
		arc->x = (short) ((sp->sizex.value *
				   ((CY * CZ - SX * SY * SZ) * star->x +
				    (-CX * SZ) * star->y +
				    (SY * CZ + SZ * SX * CY) * star->z) +
				   sp->x.value));
		arc->y = (short) ((sp->sizey.value *
				   ((CY * SZ + SX * SY * CZ) * star->x +
				    (CX * CZ) * star->y +
				    (SY * SZ - SX * CY * CZ) * star->z) +
				   sp->y.value));

		if (MI_WIN_IS_USE3D(mi)) {
			arcleft->x = (short) ((sp->sizex.value *
					((CY * CZ - SX * SY * SZ) * star->x +
					 (-CX * SZ) * star->y +
					 (SY * CZ + SZ * SX * CY) * star->z) +
					       sp->x.value));
			arcleft->y = (short) ((sp->sizey.value *
					((CY * SZ + SX * SY * CZ) * star->x +
					 (CX * CZ) * star->y +
					 (SY * SZ - SX * CY * CZ) * star->z) +
					       sp->y.value));
			arc->x += diff;
			arcleft->x -= diff;
		}
		if (star->size != 0) {
			arc->x -= star->size;
			arc->y -= star->size;
			if (MI_WIN_IS_USE3D(mi)) {
				arcleft->x -= star->size;
				arcleft->y -= star->size;
			}
		}
	}

	/* First, we erase the previous starfield */
	if (MI_WIN_IS_INSTALL(mi) && MI_WIN_IS_USE3D(mi))
		XSetForeground(display, gc, MI_NONE_COLOR(mi));
	else
		XSetForeground(display, gc, MI_WIN_BLACK_PIXEL(mi));

#if (ADAPT_ERASE == 1)
	if (sp->hasbeenchecked == 0) {
		/* We just calculate which method is the faster and eventually free
		 * the oldxarc list */
		if (sp->xarc_time >
		    ADAPT_ARC_PREFERED * sp->rect_time) {
			sp->hasbeenchecked = -2;	/* XFillRectangle mode */
			(void) free((void *) sp->oldxarc);
			sp->oldxarc = NULL;
			if (MI_WIN_IS_USE3D(mi)) {
				(void) free((void *) sp->oldxarcleft);
			  sp->oldxarcleft = NULL;
			}
		} else {
			sp->hasbeenchecked = -1;	/* XFillArcs mode */
		}
	}
	if (sp->hasbeenchecked == -2) {
		/* Erasing is done with XFillRectangle */
		XFillRectangle(display, window, gc,
			       x_1, y_1, x_2, y_2);
	} else if (sp->hasbeenchecked == -1) {
		/* Erasing is done with XFillArcs */
		XFillArcs(display, window, gc,
			  sp->oldxarc, sp->NbStars);
		if (MI_WIN_IS_USE3D(mi))
			XFillArcs(display, window, gc,
				  sp->oldxarcleft, sp->NbStars);
	} else {
		long        usec;

		if (sp->hasbeenchecked > ADAPT_CHECKS) {
#ifdef GETTIMEOFDAY_TWO_ARGS
			(void) gettimeofday(&tv1, NULL);
#else
			(void) gettimeofday(&tv1);
#endif
			XFillRectangle(display, window, gc,
				       x_1, y_1, x_2, y_2);
#ifdef GETTIMEOFDAY_TWO_ARGS
			(void) gettimeofday(&tv2, NULL);
#else
			(void) gettimeofday(&tv2);
#endif
			usec = (tv2.tv_sec - tv1.tv_sec) * 1000000;
			if (usec + tv2.tv_usec - tv1.tv_usec > 0) {
				sp->rect_time += usec + tv2.tv_usec - tv1.tv_usec;
				sp->hasbeenchecked--;
			}
		} else {
#ifdef GETTIMEOFDAY_TWO_ARGS
			(void) gettimeofday(&tv1, NULL);
#else
			(void) gettimeofday(&tv1);
#endif
			XFillArcs(display, window, gc,
				  sp->oldxarc, sp->NbStars);
			if (MI_WIN_IS_USE3D(mi))
				XFillArcs(display, window, gc,
					  sp->oldxarcleft, sp->NbStars);
#ifdef GETTIMEOFDAY_TWO_ARGS
			(void) gettimeofday(&tv2, NULL);
#else
			(void) gettimeofday(&tv2);
#endif
			usec = (tv2.tv_sec - tv1.tv_sec) * 1000000;
			if (usec + tv2.tv_usec - tv1.tv_usec > 0) {
				sp->xarc_time += usec + tv2.tv_usec - tv1.tv_usec;
				sp->hasbeenchecked--;
			}
		}
	}
#else
#if (USEOLDXARCS == 1)
	XFillArcs(display, window, gc,
		  sp->oldxarc, sp->NbStars);
	if (MI_WIN_IS_USE3D(mi))
		XFillArcs(display, window, gc,
			  sp->oldxarcleft, sp->NbStars);
#else
	XFillRectangle(display, window, gc,
		       x_1, y_1, x_2, y_2);
#endif
#endif

	/* Then we draw the new one */
	if (MI_WIN_IS_USE3D(mi)) {
		if (MI_WIN_IS_INSTALL(mi))
			XSetFunction(display, gc, GXor);
		XSetForeground(display, gc, MI_RIGHT_COLOR(mi));
		XFillArcs(display, window, gc, sp->xarc, sp->NbStars);
		XSetForeground(display, gc, MI_LEFT_COLOR(mi));
		XFillArcs(display, window, gc, sp->xarcleft, sp->NbStars);
		if (MI_WIN_IS_INSTALL(mi))
			XSetFunction(display, gc, GXcopy);
	} else {
		XSetForeground(display, gc, sp->color);
		XFillArcs(display, window, gc, sp->xarc, sp->NbStars);
	}

#if ((USEOLDXARCS == 1) || (ADAPT_ERASE == 1))
#if (ADAPT_ERASE == 1)
	if (sp->hasbeenchecked >= -1) {
		arc = sp->xarc;
		sp->xarc = sp->oldxarc;
		sp->oldxarc = arc;
		if (MI_WIN_IS_USE3D(mi)) {
			arcleft = sp->xarcleft;
			sp->xarcleft = sp->oldxarcleft;
			sp->oldxarcleft = arcleft;
		}
	}
#else
	arc = sp->xarc;
	sp->xarc = sp->oldxarc;
	sp->oldxarc = arc;
	if (MI_WIN_IS_USE3D(mi)) {
		arcleft = sp->xarcleft;
		sp->xarcleft = sp->oldxarcleft;
		sp->oldxarcleft = arcleft;
	}
#endif
#endif

	/* We set up the color for the next drawing */
	if (!MI_WIN_IS_USE3D(mi) && MI_NPIXELS(mi) > 2 &&
	    (++sp->colorchange >= COLOR_CHANGES)) {
		sp->colorchange = 0;
		if (++sp->colorp >= MI_NPIXELS(mi))
			sp->colorp = 0;
		sp->color = MI_PIXEL(mi, sp->colorp);
	}
}

ENTRYPOINT void
release_bouboule(ModeInfo * mi)
{
	if (starfield != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			StarField  *sp = &starfield[screen];

			if (sp->star)
				(void) free((void *) sp->star);
			if (sp->xarc)
				(void) free((void *) sp->xarc);
			if (sp->xarcleft)
				(void) free((void *) sp->xarcleft);
#if ((USEOLDXARCS == 1) || (ADAPT_ERASE == 1))
			if (sp->oldxarc)
				(void) free((void *) sp->oldxarc);
			if (sp->oldxarcleft)
				(void) free((void *) sp->oldxarcleft);
#endif
			sinfree(&(sp->x));
			sinfree(&(sp->y));
			sinfree(&(sp->z));
			sinfree(&(sp->sizex));
			sinfree(&(sp->sizey));
			sinfree(&(sp->thetax));
			sinfree(&(sp->thetay));
			sinfree(&(sp->thetaz));
		}
		(void) free((void *) starfield);
		starfield = NULL;
	}
}

ENTRYPOINT void
reshape_bouboule(ModeInfo * mi, int width, int height)
{
	StarField  *sp = &starfield[MI_SCREEN(mi)];
	sp->width  = width;
	sp->height = height;
    sininit(&sp->x,
        sp->x.alpha, sp->x.step,
        ((double) sp->width) / 4.0,
        3.0 * ((double) sp->width) / 4.0,
        POSCANRAND);
    sininit(&sp->y,
        sp->y.alpha, sp->y.step,
        ((double) sp->height) / 4.0,
        3.0 * ((double) sp->height) / 4.0,
        POSCANRAND);
}

ENTRYPOINT void
refresh_bouboule(ModeInfo * mi)
{
	/* Do nothing, it will refresh by itself */
}

XSCREENSAVER_MODULE ("Bouboule", bouboule)
