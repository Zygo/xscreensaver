/* -*- Mode: C; tab-width: 4 -*- */
/* scooter -- a journey through space tunnel and stars */

/*
 *  scooter.c
 *
 *  Copyright (c) 2001 Sven Thoennissen <posse@gmx.net>
 *
 *  This program is based on the original "scooter", a blanker module from the
 *  Nightshift screensaver which is part of EGS (Enhanced Graphics System) on
 *  the Amiga computer. EGS has been developed by VIONA Development.
 *
 *
 *  (now the obligatory stuff)
 *
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 *
 *  This file is provided AS IS with no warranties of any kind.  The author
 *  shall have no liability with respect to the infringement of copyrights,
 *  trade secrets or any patents by this file or any part thereof.  In no
 *  event will the author be liable for any lost revenue or profits or
 *  other special, indirect and consequential damages.
 *
 *	Changes:
 *
 *	??/??/2001 (Sven Thoennissen <posse@gmx.net>):    Initial release
 *	05/08/2019 (EoflaOE <eoflaoevicecity@gmail.com>): Ported to XScreenSaver
 */

#ifdef STANDALONE
#define MODE_scooter
#define PROGCLASS "Scooter"
#define HACK_INIT init_scooter
#define HACK_DRAW draw_scooter
#define scooter_opts xlockmore_opts
#define DEFAULTS 	"*delay: 20000 \n" \
 					"*count: 24 \n" \
 					"*cycles: 5 \n" \
 					"*size: 100 \n" \
 					"*ncolors: 200 \n" \
 					"*fullrandom: True \n" \
 					"*verbose: False \n" \
					"*fpsSolid: true \n" \

/*				    "*lowrez: True \n" */

# define reshape_scooter 0
# define scooter_handle_event 0
#include "xlockmore.h"          /* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"              /* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_scooter

ModeSpecOpt scooter_opts =
{0, (XrmOptionDescRec *) NULL, 0, (argtype *) NULL, (OptionStruct *) NULL};

#ifdef USE_MODULES
ModStruct scooter_description =
{"scooter", "init_scooter", "draw_scooter", "release_scooter",
 "refresh_scooter", "change_scooter", (char *) NULL, &scooter_opts,
 20000, 24, 5, 100, 64, 1.0, "",
 "Shows a journey through space tunnel and stars", 0, NULL};
/*
 *  count = number of doors
 *  cycles = speed (see MIN/MAX_SPEED below)
 *  size = number of stars
 *
 */
#endif

typedef struct {
	int x, y, z;
} Vec3D;

typedef struct {
	int x, y, z;
} Angle3D;

typedef struct {
	int r, g, b;
} ColorRGB;

typedef struct {
	XPoint lefttop, rightbottom;
} Rect;

typedef struct {
	Vec3D coords[4]; /* lefttop, righttop, rightbottom, leftbottom */
	int zelement;
	unsigned long color;
	char freecolor;
	char pad;
} Door;

typedef struct {
	int xpos, ypos;
	int width, height;
	int zelement;
	short draw;
} Star;

/* define this to see a pixel for each zelement */
/*
#define _DRAW_ZELEMENTS
*/

typedef struct {
	Vec3D pos;
	Angle3D angle;
} ZElement;

typedef struct {
	Star *stars;
	Door *doors;
	ZElement *zelements;
	int doorcount, ztotal, speed;
	int zelements_per_door, zelement_distance, spectator_zelement;
	int projnorm_z, rotationDuration, rotationStep, starcount;
	Angle3D currentRotation, rotationDelta;

	/* doors color cycling stuff */
	ColorRGB begincolor, endcolor;
	int colorcount, colorsteps;

	/* scale all stars and doors to window dimensions */
	float aspect_scale;

	Bool halt_scooter;
    int pscale;
} scooterstruct;

static scooterstruct *scooters = (scooterstruct *) NULL;

#define MIN_DOORS 4

#define MIN_SPEED 1
#define MAX_SPEED 10

#define SPACE_XY_FACTOR 10

#define DOOR_WIDTH  (600*SPACE_XY_FACTOR)
#define DOOR_HEIGHT (400*SPACE_XY_FACTOR)

/* stars distance from doors center */
#define STAR_MIN_X  (1000*SPACE_XY_FACTOR)
#define STAR_MIN_Y  (750*SPACE_XY_FACTOR)
#define STAR_MAX_X  (10000*SPACE_XY_FACTOR)
#define STAR_MAX_Y  (7500*SPACE_XY_FACTOR)

/* star size (random) */
#define STAR_SIZE_MIN (2*SPACE_XY_FACTOR)
#define STAR_SIZE_MAX (64*SPACE_XY_FACTOR)

/* greater values make scooter run harder curves, smaller values produce calm curves */
#define DOOR_CURVEDNESS 14

/* 3d->2d projection (greater values create more fish-eye effect) */
#define PROJECTION_DEGREE 2.4

/*  this is my resolution at which scooter is in its original size, producing a 4:3 aspect ratio.
 *  all variables in this module are adjusted for this screen size; if scooter is run
 *  in windows with different size, it knows how to rescale itself.
 */
#define ASPECT_SCREENWIDTH 1152
#define ASPECT_SCREENHEIGHT 864

/* we define our own sin/cos macros to be faaast ;-) (good old Amiga times) */
#define SINUSTABLE_SIZE 0x8000
#define SINUSTABLE_MASK 0x7fff
#define SIN(a) sintable[a & SINUSTABLE_MASK]
#define COS(a) sintable[(a+(SINUSTABLE_SIZE/4)) & SINUSTABLE_MASK]

/* signum function */
#define SGN(a) (a < 0 ? -1 : 1)

static float *sintable = (float *) NULL;

static void
randomcolor(ColorRGB *col)
{
	unsigned long n;

/*	col->r = NRAND(65536);
	col->g = NRAND(65536);
	col->b = NRAND(65536);
*/
/*	col->r = LRAND() & 0xffff;
	col->g = LRAND() & 0xffff;
	col->b = LRAND() & 0xffff;
*/
	/* this seems best */
	n = NRAND(0x1000000);
	col->r = (n>>16)<<8;
	col->g = ((n>>8) & 0xff)<<8;
	col->b = (n & 0xff)<<8;
}

static void
initdoorcolors(scooterstruct *sp)
{
	/* prepare initial values for nextdoorcolor() */

	randomcolor(&sp->endcolor);

	sp->colorcount = 0;
	sp->colorsteps = 0;
}

static void nextdoorcolor(ModeInfo *mi, Door *door)
{
	scooterstruct *sp = &scooters[MI_SCREEN(mi)];
	Display *display = MI_DISPLAY(mi);
	XColor xcol;

/* comment this to color the doors from xlock's palette (created with saturation value) */
#if 1
	if (door->freecolor) {
		XFreeColors(display, MI_COLORMAP(mi), &(door->color), 1, 0);
		door->freecolor = 0;
	}
	if (MI_NPIXELS(mi) <= 2) {
		door->color = MI_WHITE_PIXEL(mi);
		return;
	}

	if (sp->colorcount >= sp->colorsteps) {

		/* init next color ramp */

		sp->colorcount = 0;
		sp->colorsteps = 8 + NRAND(32);
		sp->begincolor = sp->endcolor;
		randomcolor(&sp->endcolor);
	}

	/* compute next color values */

	xcol.red   = sp->begincolor.r + ((sp-> endcolor.r - sp->begincolor.r) *
		sp->colorcount / sp->colorsteps);
	xcol.green = sp->begincolor.g + ((sp-> endcolor.g - sp->begincolor.g) *
		sp->colorcount / sp->colorsteps);
	xcol.blue  = sp->begincolor.b + ((sp-> endcolor.b - sp->begincolor.b) *
		sp->colorcount / sp->colorsteps);
	xcol.pixel = 0;
	xcol.flags = DoRed | DoGreen | DoBlue;

	sp->colorcount++;

	if (!XAllocColor(display, MI_COLORMAP(mi), &xcol)) {
		 /* fail safe */
		door->color = MI_WHITE_PIXEL(mi);
		door->freecolor = 0;
	} else {
		door->color = xcol.pixel;
		door->freecolor = 1;
	}
#else
	if (MI_NPIXELS(mi) > 2) {
		if (++colorcount >= MI_NPIXELS(mi))
			colorcount = 0;
		door->color = MI_PIXEL(mi,colorcount);
	} else
		door->color = MI_WHITE_PIXEL(mi);
	return;
#endif
}

static void
free_scooter(scooterstruct *sp)
{
	if (sp->doors != NULL) {
		free(sp->doors);
		sp->doors = (Door *) NULL;
	}
	if (sp->stars != NULL) {
		free(sp->stars);
		sp->stars = (Star *) NULL;
	}
	if (sp->zelements != NULL) {
		free(sp->zelements);
		sp->zelements = (ZElement *) NULL;
	}
}

static void release_scooter(ModeInfo *mi)
{
    if (scooters != NULL) {
        int screen;

        for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
             free_scooter(&scooters[screen]);
        free(scooters);
        scooters = (scooterstruct *) NULL;
    }
	if (sintable != NULL) {
		free(sintable);
		sintable = (float *) NULL;
	}
}

static void init_scooter(ModeInfo *mi)
{
	int i;
	scooterstruct *sp;

	if (scooters == NULL) {
		if ((scooters = (scooterstruct *) calloc(MI_NUM_SCREENS(mi),
				sizeof (scooterstruct))) == NULL)
			return;
	}
	sp = &scooters[MI_SCREEN(mi)];
	sp->doorcount = MAX(MI_COUNT(mi),MIN_DOORS);
	sp->speed = MI_CYCLES(mi);
	sp->starcount = MI_SIZE(mi);
	if (sp->starcount < 1)
		sp->starcount = 1;
	if (sp->speed < MIN_SPEED)
		sp->speed = MIN_SPEED;
	if (sp->speed > MAX_SPEED)
		sp->speed = MAX_SPEED;
	sp->zelements_per_door = 60;
	sp->zelement_distance = 300;
	sp->ztotal = sp->doorcount * sp->zelements_per_door;
	/* sp->z_maxdepth = sp->ztotal * sp->zelement_distance; */
	if (sp->starcount > sp->ztotal)
		sp->starcount = sp->ztotal;

	sp->halt_scooter = False;

	initdoorcolors(sp);
	free_scooter(sp);
	if (sintable == NULL) {
		if ((sintable = (float *) malloc(sizeof(float) *
			SINUSTABLE_SIZE)) == NULL) {
			release_scooter(mi);
			return;
		}
	}
	for (i = 0; i < SINUSTABLE_SIZE; i++) {
		sintable[i] = SINF(M_PI*2/SINUSTABLE_SIZE*i);
	}

	if ((sp->doors = (Door *) malloc(sizeof(Door) *
			sp->doorcount)) == NULL) {
		return;
	}

	if ((sp->zelements = (ZElement *) malloc(sizeof(ZElement) *
		 sp->ztotal)) == NULL) {
		free_scooter(sp);
		return;
	}
	for (i = 0; i <  sp->doorcount; i++) {
		sp->doors[i].zelement =
			(sp->zelements_per_door * (i + 1)) - 1;
		sp->doors[i].freecolor = 0;
		nextdoorcolor(mi, &sp->doors[i]);
	}

	for (i = 0; i < sp->ztotal; i++) {
		sp->zelements[i].angle.x = 0;
		sp->zelements[i].angle.y = 0;
		sp->zelements[i].angle.z = 0;
	}

	if ((sp->stars = (Star *) malloc(sizeof(Star) *
			sp->starcount)) == NULL) {
		free_scooter(sp);
		return;
	}
	for (i = 0; i < sp->starcount; i++) {
		sp->stars[i].zelement = sp->ztotal * i / sp->starcount;
		sp->stars[i].draw = 0;
	}

	sp->projnorm_z = 50 * 240;
	sp->spectator_zelement = sp->zelements_per_door;

	sp->currentRotation.x = 0;
	sp->currentRotation.y = 0;
	sp->currentRotation.z = 0;

	sp->rotationDelta.x = 0;
	sp->rotationDelta.y = 0;
	sp->rotationDelta.z = 0;

	sp->rotationDuration = 1;
	sp->rotationStep = 0;

    sp->pscale = 1;
    if (mi->xgwa.width > 2560 || mi->xgwa.height > 2560)
      sp->pscale = 2;  /* Retina displays */
}

static void cleardoors(ModeInfo *mi)
{
	/* MI_CLEARWINDOW(mi);*/
	XClearWindow(MI_DISPLAY(mi), MI_WINDOW(mi));
}

/* Should be taken care of already... but just in case */
#if !defined( __GNUC__ ) && !defined(__cplusplus) && !defined(c_plusplus)
#undef inline
#define inline                  /* */
#endif
static inline float
projection (scooterstruct *sp, int zval)
{
	return (sp->projnorm_z / (PROJECTION_DEGREE * zval));
/* this is another formula. it is not limited to z>0 but it pulls too strong towards the screen center */
/*      return (sp->projnorm_z * pow(1.22,-(zval/200*PROJ_CURVEDNESS)))*/
}

/*

 y

 ^
 |      z
 |    .
 |   /
 |  /
 | /
 |/
-+------------> x
/|

  rotation angles:
	  a = alpha (x-rotation), b = beta (y-rotation), c = gamma (z-rotation)

  x-axis rotation:
	(  z  )'  =  ( cos(a) -sin(a) ) (  z  )
	(  y  )      ( sin(a)  cos(a) ) (  y  )

  y-axis rotation:
	(  z  )'  =  ( cos(b) -sin(b) ) (  z  )
	(  x  )      ( sin(b)  cos(b) ) (  x  )

  z-axis rotation:
	(  x  )'  =  ( cos(c) -sin(c) ) (  x  )
	(  y  )      ( sin(c)  cos(c) ) (  y  )
*/

static void
rotate_3d(Vec3D *src, Vec3D *dest, Angle3D *angle)
{
	Vec3D tmp;
	float	cosa = COS(angle->x),
		cosb = COS(angle->y),
		cosc = COS(angle->z),
		sina = SIN(angle->x),
		sinb = SIN(angle->y),
		sinc = SIN(angle->z);

	/* rotate around X, Y and Z axis (see formulae above) */

	/* X axis */
	tmp.z = src->z;
	tmp.y = src->y;
	dest->z = (int) (tmp.z * cosa - tmp.y * sina);
	dest->y = (int) (tmp.z * sina + tmp.y * cosa);

	/* Y axis */
	tmp.z = dest->z;
	tmp.x = src->x;
	dest->z = (int) (tmp.z * cosb - tmp.x * sinb);
	dest->x = (int) (tmp.z * sinb + tmp.x * cosb);

	/* Z axis */
	tmp.x = dest->x;
	tmp.y = dest->y;
	dest->x = (int) (tmp.x * cosc - tmp.y * sinc);
	dest->y = (int) (tmp.x * sinc + tmp.y * cosc);
}

static void calc_new_element(ModeInfo *mi)
{
	scooterstruct *sp = &scooters[MI_SCREEN(mi)];
	float rot = SIN((SINUSTABLE_SIZE/2)*
		sp->rotationStep/sp->rotationDuration);

	/* change current rotation 3D angle */

	if (sp->rotationStep++ >= sp->rotationDuration) {

		int fps = 1000000/MI_DELAY(mi); /* frames per second as timebase */

 		/*  one rotation interval takes 10-30 seconds at speed 1.
		 */
 		sp->rotationDuration = 10*fps + NRAND(20*fps);

		/* -DOOR_CURVEDNESS <= delta <= +DOOR_CURVEDNESS */
		sp->rotationDelta.x =
			NRAND(DOOR_CURVEDNESS*2+1) - DOOR_CURVEDNESS;
		sp->rotationDelta.y =
			NRAND(DOOR_CURVEDNESS*2+1) - DOOR_CURVEDNESS;
		sp->rotationDelta.z =
			NRAND(DOOR_CURVEDNESS*2+1) - DOOR_CURVEDNESS;

		sp->rotationStep = 0;
	}

	sp->currentRotation.x += (int) (rot * sp->rotationDelta.x);
	sp->currentRotation.y += (int) (rot * sp->rotationDelta.y);
	sp->currentRotation.z += (int) (rot * sp->rotationDelta.z);

	sp->currentRotation.x &= SINUSTABLE_MASK;
	sp->currentRotation.y &= SINUSTABLE_MASK;
	sp->currentRotation.z &= SINUSTABLE_MASK;
}

static void shift_elements(ModeInfo *mi)
{
	scooterstruct *sp = &scooters[MI_SCREEN(mi)];
	int i, iprev;
	Vec3D tmpvec;
	Angle3D tmpangle;

	/* shift angles from zelements */

	for (i = sp->speed; i < sp->ztotal; i++) {
		sp->zelements[i - sp->speed].angle = sp->zelements[i].angle;
	}
	for (i = sp->ztotal - sp->speed; i < sp->ztotal; i++) {
		calc_new_element(mi);
		sp->zelements[i].angle = sp->currentRotation;
	}

	/* calculate new 3D-coords from ALL zelements */

	sp->zelements[sp->spectator_zelement].pos.x = 0;
	sp->zelements[sp->spectator_zelement].pos.y = 0;
	sp->zelements[sp->spectator_zelement].pos.z =
		sp->zelement_distance * sp->spectator_zelement;

	for (i = sp->spectator_zelement - 1; i >= 0; --i) {
		iprev = i + 1;

		tmpvec.x = 0;
		tmpvec.y = 0;
		tmpvec.z = - sp->zelement_distance;
		tmpangle.x = sp->zelements[i].angle.x -
			sp->zelements[sp->spectator_zelement].angle.x;
		tmpangle.y = sp->zelements[i].angle.y -
			sp->zelements[sp->spectator_zelement].angle.y;
		tmpangle.z = sp->zelements[i].angle.z -
			sp->zelements[sp->spectator_zelement].angle.z;
		rotate_3d(&tmpvec, &(sp->zelements[i].pos), &tmpangle);
		sp->zelements[i].pos.x += sp->zelements[iprev].pos.x;
		sp->zelements[i].pos.y += sp->zelements[iprev].pos.y;
		sp->zelements[i].pos.z += sp->zelements[iprev].pos.z;
	}

	for (i = sp->spectator_zelement + 1; i < sp->ztotal; i++) {
		iprev = i - 1;

		tmpvec.x = 0;
		tmpvec.y = 0;
		tmpvec.z = sp->zelement_distance;
		tmpangle.x = sp->zelements[i].angle.x -
			sp->zelements[sp->spectator_zelement].angle.x;
		tmpangle.y = sp->zelements[i].angle.y -
			sp->zelements[sp->spectator_zelement].angle.y;
		tmpangle.z = sp->zelements[i].angle.z -
			sp->zelements[sp->spectator_zelement].angle.z;
		rotate_3d(&tmpvec, &(sp->zelements[i].pos), &tmpangle);
		sp->zelements[i].pos.x += sp->zelements[iprev].pos.x;
		sp->zelements[i].pos.y += sp->zelements[iprev].pos.y;
		sp->zelements[i].pos.z += sp->zelements[iprev].pos.z;
	}


	/* shift doors and wrap around */

	for (i = 0; i < sp->doorcount; i++) {
		if ((sp->doors[i].zelement -= sp->speed) < 0) {
			sp->doors[i].zelement += sp->ztotal;
			nextdoorcolor(mi,&sp->doors[i]);
		}
	}

	/* shift stars */

	for (i = 0; i < sp->starcount; i++) {
		if ((sp->stars[i].zelement -= sp->speed) < 0) {
			int rnd;

			sp->stars[i].zelement += sp->ztotal;
			sp->stars[i].draw = 1;

			/* make sure new stars are outside doors */

			rnd = NRAND(2*(STAR_MAX_X - STAR_MIN_X)) -
				(STAR_MAX_X - STAR_MIN_X);
			sp->stars[i].xpos = rnd + (STAR_MIN_X * SGN(rnd));

			rnd = NRAND(2*(STAR_MAX_Y - STAR_MIN_Y)) -
				(STAR_MAX_Y - STAR_MIN_Y);
			sp->stars[i].ypos = rnd + (STAR_MIN_Y * SGN(rnd));

			rnd = NRAND(STAR_SIZE_MAX - STAR_SIZE_MIN) +
				STAR_SIZE_MIN;
			sp->stars[i].width  = rnd;
			sp->stars[i].height = rnd * 3 / 4;
		}
	}
}

static void door_3d(scooterstruct *sp, Door *door)
{
	ZElement *ze = &sp->zelements[door->zelement];
	Vec3D src;
	Angle3D tmpangle;

	tmpangle.x = ze->angle.x -
		sp->zelements[sp->spectator_zelement].angle.x;
	tmpangle.y = ze->angle.y -
		sp->zelements[sp->spectator_zelement].angle.y;
	tmpangle.z = ze->angle.z -
		sp->zelements[sp->spectator_zelement].angle.z;

	/* calculate 3d coords of all 4 edges */

	src.x = -DOOR_WIDTH/2;
	src.y = DOOR_HEIGHT/2;
	src.z = 0;
	rotate_3d(&src, &(door->coords[0]), &tmpangle);
	door->coords[0].x += ze->pos.x;
	door->coords[0].y += ze->pos.y;
	door->coords[0].z += ze->pos.z;

	src.x = DOOR_WIDTH/2;
	rotate_3d(&src, &(door->coords[1]), &tmpangle);
	door->coords[1].x += ze->pos.x;
	door->coords[1].y += ze->pos.y;
	door->coords[1].z += ze->pos.z;

	src.y = -DOOR_HEIGHT/2;
	rotate_3d(&src, &(door->coords[2]), &tmpangle);
	door->coords[2].x += ze->pos.x;
	door->coords[2].y += ze->pos.y;
	door->coords[2].z += ze->pos.z;

	src.x = -DOOR_WIDTH/2;
	rotate_3d(&src, &(door->coords[3]), &tmpangle);
	door->coords[3].x += ze->pos.x;
	door->coords[3].y += ze->pos.y;
	door->coords[3].z += ze->pos.z;
}

/*
 *  clip the line p1-p2 at the given rectangle
 *
 */
static int clipline(XPoint *p1, XPoint *p2, Rect *rect)
{
	XPoint new1, new2, tmp;
	float m;

	new1 = *p1;
	new2 = *p2;

	/* entire line may not need clipping */

	if (((new1.x >= rect->lefttop.x) && (new1.x <= rect->rightbottom.x))
	 || ((new1.y >= rect->lefttop.y) && (new1.y <= rect->rightbottom.y))
	 || ((new2.x >= rect->lefttop.x) && (new2.x <= rect->rightbottom.x))
	 || ((new2.y >= rect->lefttop.y) && (new2.y <= rect->rightbottom.y)))
		return 1;


	/* first: clip y dimension */

	/* p1 is above p2 */
	if (new1.y > new2.y) {
		tmp = new1;
		new1 = new2;
		new2 = tmp;
	}

	/* line could be totally out of view */
	if ((new2.y < rect->lefttop.y) || (new1.y > rect->rightbottom.y))
		return 0;

	m = (new2.x == new1.x) ? 0 :
		((float)(new2.y - new1.y) / (new2.x - new1.x));

	if (new1.y < rect->lefttop.y) {
		if (m)
			new1.x += (int) ((rect->lefttop.y - new1.y)/m);
		new1.y = rect->lefttop.y;
	}
	if (new2.y > rect->rightbottom.y) {
		if (m)
			new2.x -= (int) ((new2.y - rect->rightbottom.y)/m);
		new2.y = rect->rightbottom.y;
	}


	/* clip x dimension */

	/* p1 is left to p2 */
	if (new1.x > new2.x) {
		tmp = new1;
		new1 = new2;
		new2 = tmp;
	}

	if ((new2.x < rect->lefttop.x) || (new1.x > rect->rightbottom.x))
		return 0;

	m = (new2.x == new1.x) ? 0 :
		((float)(new2.y - new1.y) / (new2.x - new1.x));

	if (new1.x < rect->lefttop.x) {
		new1.y += (int) ((rect->lefttop.y - new1.y)*m);
		new1.x = rect->lefttop.x;
	}
	if (new2.y > rect->rightbottom.y) {
		new2.y -= (int) ((new2.y - rect->rightbottom.y)*m);
		new2.x = rect->rightbottom.x;
	}


	/* push values */

	*p1 = new1;
	*p2 = new2;

	return 1;
}

static void drawdoors(ModeInfo *mi)
{
	scooterstruct *sp = &scooters[MI_SCREEN(mi)];
	Display *display = MI_DISPLAY(mi);
	Window window = MI_WINDOW(mi);
	GC gc = MI_GC(mi);
	int width = MI_WIDTH(mi), height = MI_HEIGHT(mi),
		midx = width/2, midy = height/2;
	int i, j;
	Rect rect = { {0,0}, {0,0} };

        rect.rightbottom.x = width - 1;
        rect.rightbottom.y = height - 1;
	XSetLineAttributes(display, gc, 2*sp->pscale,
                       LineSolid, CapNotLast, JoinRound);

#ifdef _DRAW_ZELEMENTS
        XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
        for (i= sp->spectator_zelement; i<ztotal; i++) {
                register float proj;
                XPoint p;

                proj = projection(sp, zelements[i].pos.z) * sp->aspect_scale;
                p.x = midx + (sp->zelements[i].pos.x * proj / SPACE_XY_FACTOR);
                p.y = midy - (sp->zelements[i].pos.y * proj / SPACE_XY_FACTOR);
                XFillRectangle(display, window, gc, p.x, p.y,
                               st->pscale, st->pscale);
        }
#endif

	for (i = 0; i < sp->doorcount; i++) {

		register float proj;
		XPoint lines[4], clip1, clip2;

		door_3d(sp, &sp->doors[i]);

		for (j=0; j<4; j++) {
			if (sp->doors[i].coords[j].z <= 0) break;

			proj = projection(sp, sp->doors[i].coords[j].z) * sp->aspect_scale;
			lines[j].x = midx + (int) (sp->doors[i].coords[j].x *
				proj / SPACE_XY_FACTOR);
			lines[j].y = midy - (int) (sp->doors[i].coords[j].y *
				proj / SPACE_XY_FACTOR);
		}
		if (j<4) continue;

		XSetForeground(display, gc, sp->doors[i].color);

		for (j=0; j<4; j++) {
			clip1 = lines[j];
			clip2 = lines[(j+1)%4];
			if (clipline(&clip1, &clip2, &rect))
				XDrawLine(display, window, gc, clip1.x, clip1.y, clip2.x, clip2.y);
		}
	}
}

ENTRYPOINT void drawstars(ModeInfo *mi)
{
	scooterstruct *sp = &scooters[MI_SCREEN(mi)];
	Display *display = MI_DISPLAY(mi);
	Window window = MI_WINDOW(mi);
	GC gc = MI_GC(mi);
	int width = MI_WIDTH(mi), height = MI_HEIGHT(mi),
		midx = width/2, midy = height/2;
	int i;

	for (i = 0; i < sp->starcount; i++) {

		float proj;
		ZElement *ze = &sp->zelements[sp->stars[i].zelement];
		Vec3D tmpvec, coords;
		Angle3D tmpangle;
		XPoint lefttop, rightbottom;

		if (!sp->stars[i].draw) continue;


		/* rotate star around its z-element, then add its position */

		tmpangle.x = ze->angle.x -
			sp->zelements[sp->spectator_zelement].angle.x;
		tmpangle.y = ze->angle.y -
			sp->zelements[sp->spectator_zelement].angle.y;
		tmpangle.z = ze->angle.z -
			sp->zelements[sp->spectator_zelement].angle.z;

		tmpvec.x = sp->stars[i].xpos;
		tmpvec.y = sp->stars[i].ypos;
		tmpvec.z = 0;
		rotate_3d(&tmpvec, &coords, &tmpangle);
		coords.x += ze->pos.x;
		coords.y += ze->pos.y;
		coords.z += ze->pos.z;

		if (coords.z <= 0) continue;


		/* projection and clipping (trivial for a rectangle) */

		proj = projection(sp, coords.z) * sp->aspect_scale;

		lefttop.x = midx + (int) ((coords.x - sp->stars[i].width/2) *
			proj / SPACE_XY_FACTOR);
		lefttop.y = midy - (int) ((coords.y + sp->stars[i].height/2) *
			proj / SPACE_XY_FACTOR);
		if (lefttop.x < 0)
			lefttop.x = 0;
		else if (lefttop.x >= width)
			continue;
		if (lefttop.y < 0)
			lefttop.y = 0;
		else if (lefttop.y >= height)
			continue;

		rightbottom.x = midx + (int) ((coords.x + sp->stars[i].width/2) *
			 proj / SPACE_XY_FACTOR);
		rightbottom.y = midy - (int) ((coords.y - sp->stars[i].height/2) *
			 proj / SPACE_XY_FACTOR);
		if (rightbottom.x < 0)
			continue;
		else if (rightbottom.x >= width)
			rightbottom.x = width - 1;
		if (rightbottom.y < 0)
			continue;
		else if (rightbottom.y >= height)
			rightbottom.y = height - 1;


		/* in white color, small stars look darker than big stars */

		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));

#if 0
		if ((lefttop.x == rightbottom.x) &&
				(lefttop.y == rightbottom.y)) {
			/* star is exactly 1 pixel */
			XDrawPoint(display, window, gc, lefttop.x, lefttop.y);
		} else if ((rightbottom.x - lefttop.x) +
				(rightbottom.y - lefttop.y) == 1) {
			/* star is 2 pixels wide or high */
			XDrawPoint(display, window, gc,
				lefttop.x,     lefttop.y);
			XDrawPoint(display, window, gc,
				rightbottom.x, rightbottom.y);
		} else if ((rightbottom.x - lefttop.x == 1) &&
				(rightbottom.y - lefttop.y == 1)) {

			/*  star is exactly 2x2 pixels.
			 *  a 2x2 rectangle should be drawn faster by plotting all 4 pixels
			 *  than by filling a rectangle (is this really so under X ?)
			 */

			XDrawPoint(display, window, gc,
				lefttop.x,     lefttop.y);
			XDrawPoint(display, window, gc,
				rightbottom.x, lefttop.y);
			XDrawPoint(display, window, gc,
				lefttop.x,     rightbottom.y);
			XDrawPoint(display, window, gc,
				rightbottom.x, rightbottom.y);
		} else {
			XFillRectangle(display, window, gc,
				lefttop.x, lefttop.y,
				rightbottom.x - lefttop.x,
				rightbottom.y - lefttop.y);
		}
# else
        XFillArc (display, window, gc,
                  lefttop.x, lefttop.y,
                  rightbottom.x - lefttop.x,
                  rightbottom.y - lefttop.y,
                  0, 360 * 64);
# endif
	}
}

ENTRYPOINT void draw_scooter(ModeInfo *mi)
{
	scooterstruct *sp;

	if (scooters == NULL)
		return;
	sp = &scooters[MI_SCREEN(mi)];
	if (sp->doors == NULL)
		return;

	cleardoors(mi);

	shift_elements(mi);

	/* With these scale factors, all doors are sized correctly for any window dimension.
	 * If aspect ratio is not 4:3, the smaller part of the window is used, e.g.:
	 *      window = 1000x600
	 *      => door scale factor is like in a 800x600 window (not 1000x750)
	 */

	if ((float)MI_WIDTH(mi)/MI_HEIGHT(mi) >=
			(float)ASPECT_SCREENWIDTH/ASPECT_SCREENHEIGHT) {
		/* window is wider than or equal 4:3 */
		sp->aspect_scale = (float)MI_HEIGHT(mi) / ASPECT_SCREENHEIGHT;
	} else {
		/* window is higher than 4:3 */
		sp->aspect_scale = (float)MI_WIDTH(mi) / ASPECT_SCREENWIDTH;
	}

	drawstars(mi);

	drawdoors(mi);
}

#if 0
ENTRYPOINT void refresh_scooter(ModeInfo *mi)
{
	/* MI_CLEARWINDOW(mi); */
	XClearWindow(MI_DISPLAY(mi), MI_WINDOW(mi));
}

ENTRYPOINT void change_scooter(ModeInfo *mi)
{
	scooterstruct *sp;

	if (scooters == NULL)
		return;
	sp = &scooters[MI_SCREEN(mi)];

	sp->halt_scooter = !sp->halt_scooter;
}
#endif

#define free_scooter 0
XSCREENSAVER_MODULE ("Scooter", scooter)

#endif /* MODE_scooter */
