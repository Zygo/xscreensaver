/* -*- Mode: C; tab-width: 4 -*- */
/* scooter -- a journey through space tunnel and stars */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)scooter.c	5.01 2001/03/02 xlockmore";

#endif

/*
 *  scooter.c
 *
 *  Copyright (c) 2001 Sven Thoennissen <posse@gmx.net>
 *
 *  This program is based on the original "scooter", a blanker module from the
 *  Nightshift screensaver which is part of EGS (Enhanced Graphics System) on
 *  the Amiga computer. EGS has been developed by Ingenieurbüro Helfrich.
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
 */

#ifdef STANDALONE
#define PROGCLASS "Scooter"
#define HACK_INIT init_scooter
#define HACK_DRAW draw_scooter
#define scooter_opts xlockmore_opts
#define DEFAULTS "*delay: 20000 \n" \
 "*count: 24 \n" \
 "*cycles: 5 \n" \
 "*size: 100 \n" \
 "*ncolors: 200 \n" \
 "*fullrandom: True \n" \
 "*verbose: False \n"
#include "xlockmore.h"          /* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"              /* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_scooter

ModeSpecOpt scooter_opts = {0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct scooter_description =
{"scooter", "init_scooter", "draw_scooter", "release_scooter",
 "refresh_scooter", "change_scooter", NULL, &scooter_opts,
 20000, 24, 5, 100, 64, 1.0, "",
 "Shows a journey through space tunnel and stars", 0, NULL};
/*
 *  count = number of doors
 *  cycles = speed (see MIN/MAX_SPEED below)
 *  size = number of stars
 *
 */
#endif

struct Vec3D {
	int x, y, z;
};

struct Angle3D {
	int x, y, z;
};

struct ColorRGB {
	int r, g, b;
};

struct Rect {
	XPoint lefttop, rightbottom;
};

struct Door {
	struct Vec3D coords[4]; /* lefttop, righttop, rightbottom, leftbottom */
	int zelement;
	unsigned long color;
	char freecolor;
	char pad;
};

struct Star {
	int xpos, ypos;
	int width, height;
	int zelement;
	short draw;
};

/* define this to see a pixel for each zelement */
/*
#define _DRAW_ZELEMENTS
*/

struct ZElement {
	struct Vec3D pos;
	struct Angle3D angle;
};

static struct Star *stars = NULL;
static struct Door *doors = NULL;
static struct ZElement *zelements = NULL;
static int doorcount, ztotal, speed, zelements_per_door, zelement_distance, spectator_zelement,
	projnorm_z, rotationDuration, rotationStep, starcount, z_maxdepth;

static struct Angle3D currentRotation, rotationDelta;

/* doors color cycling stuff */
static struct ColorRGB begincolor, endcolor;
static int colorcount, colorsteps;

/* scale all stars and doors to window dimensions */
static float aspect_scale;

static char halt_scooter;


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

static float *sintable = NULL;


static void initdoorcolors();
static void nextdoorcolor( struct ModeInfo_s *mi, struct Door *door );


void init_scooter( struct ModeInfo_s *mi )
{
int i;

	release_scooter(mi);

	doorcount = MAX(MI_COUNT(mi),MIN_DOORS);
	speed = MI_CYCLES(mi);
	starcount = MI_SIZE(mi);

	if( speed < MIN_SPEED ) speed = MIN_SPEED;
	if( speed > MAX_SPEED ) speed = MAX_SPEED;
	zelements_per_door = 60;
	zelement_distance = 300;
	ztotal = doorcount * zelements_per_door;
	z_maxdepth = ztotal * zelement_distance;
	if( starcount > ztotal ) starcount = ztotal;

	halt_scooter = 0;

	initdoorcolors();

	if( (doors = (struct Door *) malloc(sizeof(struct Door) * doorcount)) ) {

		if( (zelements = (struct ZElement *) malloc(sizeof(struct ZElement) * ztotal)) ) {

			for( i=0; i<doorcount; i++ ) {
				doors[i].zelement = (zelements_per_door * (i + 1)) - 1;
				doors[i].freecolor = 0;
				nextdoorcolor(mi,&doors[i]);
			}

			for( i=0; i<ztotal; i++ ) {
				zelements[i].angle.x = 0;
				zelements[i].angle.y = 0;
				zelements[i].angle.z = 0;
			}
		}
	}

	if( (sintable = (float *) malloc(sizeof(float) * SINUSTABLE_SIZE)) ) {
		for( i=0; i<SINUSTABLE_SIZE; i++ )
			sintable[i] = SINF(M_PI*2/SINUSTABLE_SIZE*i);
	}

	if( (stars = (struct Star *) malloc(sizeof(struct Star) * starcount)) ) {
		for( i=0; i<starcount; i++ ) {
			stars[i].zelement = ztotal * i / starcount;
			stars[i].draw = 0;
		}
	}

	projnorm_z = 50 * 240;
	spectator_zelement = zelements_per_door;

	currentRotation.x = 0;
	currentRotation.y = 0;
	currentRotation.z = 0;

	rotationDelta.x = 0;
	rotationDelta.y = 0;
	rotationDelta.z = 0;

	rotationDuration = 1;
	rotationStep = 0;
}

static void cleardoors( struct ModeInfo_s *mi )
{
	MI_CLEARWINDOW(mi);
}

static inline float projection( int zval )
{
	if ( zval > 0 )
		return (projnorm_z / (PROJECTION_DEGREE * zval));
	else
		return (projnorm_z * pow(1.22,-(zval/200*PROJECTION_DEGREE)));
/* this is another formula. it is not limited to z>0 but it pulls too strong towards the screen center */
/*      return (projnorm_z * pow(1.22,-(zval/200*PROJ_CURVEDNESS)))*/
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

static void rotate_3d( struct Vec3D *src, struct Vec3D *dest, struct Angle3D *angle )
{
struct Vec3D tmp;
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
	dest->z = tmp.z * cosa - tmp.y * sina;
	dest->y = tmp.z * sina + tmp.y * cosa;

	/* Y axis */
	tmp.z = dest->z;
	tmp.x = src->x;
	dest->z = tmp.z * cosb - tmp.x * sinb;
	dest->x = tmp.z * sinb + tmp.x * cosb;

	/* Z axis */
	tmp.x = dest->x;
	tmp.y = dest->y;
	dest->x = tmp.x * cosc - tmp.y * sinc;
	dest->y = tmp.x * sinc + tmp.y * cosc;
}

static void randomcolor( struct ColorRGB *col )
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

static void initdoorcolors()
{
	/* prepare initial values for nextdoorcolor() */

	randomcolor(&endcolor);

	colorcount = 0;
	colorsteps = 0;
}

static void nextdoorcolor( struct ModeInfo_s *mi, struct Door *door )
{
Display *display = MI_DISPLAY(mi);
XColor xcol;

/* uncomment this to color the doors from xlock's palette (created with saturation value) */
#if 0
	if (MI_NPIXELS(mi) > 2) {
		if( ++colorcount >= MI_NPIXELS(mi) )
			colorcount = 0;
		door->color = MI_PIXEL(mi,colorcount);
	} else
		door->color = MI_WHITE_PIXEL(mi);
	return;
#endif
	if( door->freecolor ) {
		XFreeColors(display, MI_COLORMAP(mi), &(door->color), 1, 0);
		door->freecolor = 0;
	}
	if (MI_NPIXELS(mi) <= 2) {
		door->color = MI_WHITE_PIXEL(mi);
		return;
	}

	if( colorcount >= colorsteps ) {

		/* init next color ramp */

		colorcount = 0;
		colorsteps = 8 + NRAND(32);
		begincolor = endcolor;
		randomcolor(&endcolor);
	}

	/* compute next color values */

	xcol.red   = begincolor.r + ((endcolor.r - begincolor.r) * colorcount / colorsteps);
	xcol.green = begincolor.g + ((endcolor.g - begincolor.g) * colorcount / colorsteps);
	xcol.blue  = begincolor.b + ((endcolor.b - begincolor.b) * colorcount / colorsteps);
	xcol.pixel = 0;
	xcol.flags = DoRed | DoGreen | DoBlue;

	colorcount++;

	if( !XAllocColor(display, MI_COLORMAP(mi), &xcol) ) {
		 /* fail safe */
		door->color = MI_WHITE_PIXEL(mi);
		door->freecolor = 0;
	} else {
		door->color = xcol.pixel;
		door->freecolor = 1;
	}
}

static void calc_new_element( struct ModeInfo_s *mi )
{
float rot = SIN((SINUSTABLE_SIZE/2)*rotationStep/rotationDuration);

	/* change current rotation 3D angle */

	if( rotationStep++ >= rotationDuration ) {

		int fps = 1000000/MI_DELAY(mi); /* frames per second as timebase */

 		/*  one rotation interval takes 10-30 seconds at speed 1.
		 */
 		rotationDuration = 10*fps + NRAND(20*fps);

		/* -DOOR_CURVEDNESS <= delta <= +DOOR_CURVEDNESS */
		rotationDelta.x = NRAND(DOOR_CURVEDNESS*2+1) - DOOR_CURVEDNESS;
		rotationDelta.y = NRAND(DOOR_CURVEDNESS*2+1) - DOOR_CURVEDNESS;
		rotationDelta.z = NRAND(DOOR_CURVEDNESS*2+1) - DOOR_CURVEDNESS;

		rotationStep = 0;
	}

	currentRotation.x += rot * rotationDelta.x;
	currentRotation.y += rot * rotationDelta.y;
	currentRotation.z += rot * rotationDelta.z;

	currentRotation.x &= SINUSTABLE_MASK;
	currentRotation.y &= SINUSTABLE_MASK;
	currentRotation.z &= SINUSTABLE_MASK;
}

static void shift_elements( struct ModeInfo_s *mi )
{
int i, iprev;
struct Vec3D tmpvec;
struct Angle3D tmpangle;

	/* shift angles from zelements */

	for( i=speed; i<ztotal; i++ ) {
		zelements[i-speed].angle = zelements[i].angle;
	}
	for( i=ztotal-speed; i<ztotal; i++ ) {
		calc_new_element(mi);
		zelements[i].angle = currentRotation;
	}

	/* calculate new 3D-coords from ALL zelements */

	zelements[spectator_zelement].pos.x = 0;
	zelements[spectator_zelement].pos.y = 0;
	zelements[spectator_zelement].pos.z = zelement_distance * spectator_zelement;

	for( i=spectator_zelement-1; i>=0; --i ) {
		iprev = i + 1;

		tmpvec.x = 0;
		tmpvec.y = 0;
		tmpvec.z = - zelement_distance;
		tmpangle.x = zelements[i].angle.x - zelements[spectator_zelement].angle.x;
		tmpangle.y = zelements[i].angle.y - zelements[spectator_zelement].angle.y;
		tmpangle.z = zelements[i].angle.z - zelements[spectator_zelement].angle.z;
		rotate_3d( &tmpvec, &(zelements[i].pos), &tmpangle );
		zelements[i].pos.x += zelements[iprev].pos.x;
		zelements[i].pos.y += zelements[iprev].pos.y;
		zelements[i].pos.z += zelements[iprev].pos.z;
	}

	for( i=spectator_zelement+1; i<ztotal; i++ ) {
		iprev = i - 1;

		tmpvec.x = 0;
		tmpvec.y = 0;
		tmpvec.z = zelement_distance;
		tmpangle.x = zelements[i].angle.x - zelements[spectator_zelement].angle.x;
		tmpangle.y = zelements[i].angle.y - zelements[spectator_zelement].angle.y;
		tmpangle.z = zelements[i].angle.z - zelements[spectator_zelement].angle.z;
		rotate_3d( &tmpvec, &(zelements[i].pos), &tmpangle );
		zelements[i].pos.x += zelements[iprev].pos.x;
		zelements[i].pos.y += zelements[iprev].pos.y;
		zelements[i].pos.z += zelements[iprev].pos.z;
	}


	/* shift doors and wrap around */

	for( i=0; i<doorcount; i++ ) {
		if( (doors[i].zelement -= speed) < 0 ) {
			doors[i].zelement += ztotal;
			nextdoorcolor(mi,&doors[i]);
		}
	}

	/* shift stars */

	for( i=0; i<starcount; i++ ) {
		if( (stars[i].zelement -= speed) < 0 ) {
			int rnd;

			stars[i].zelement += ztotal;
			stars[i].draw = 1;

			/* make sure new stars are outside doors */

			rnd = NRAND(2*(STAR_MAX_X - STAR_MIN_X)) - (STAR_MAX_X - STAR_MIN_X);
			stars[i].xpos = rnd + (STAR_MIN_X * SGN(rnd));

			rnd = NRAND(2*(STAR_MAX_Y - STAR_MIN_Y)) - (STAR_MAX_Y - STAR_MIN_Y);
			stars[i].ypos = rnd + (STAR_MIN_Y * SGN(rnd));

			rnd = NRAND(STAR_SIZE_MAX - STAR_SIZE_MIN) + STAR_SIZE_MIN;
			stars[i].width  = rnd;
			stars[i].height = rnd * 3 / 4;
		}
	}
}

static void door_3d( struct Door *door )
{
struct ZElement *ze = &zelements[door->zelement];
struct Vec3D src;
struct Angle3D tmpangle;

	tmpangle.x = ze->angle.x - zelements[spectator_zelement].angle.x;
	tmpangle.y = ze->angle.y - zelements[spectator_zelement].angle.y;
	tmpangle.z = ze->angle.z - zelements[spectator_zelement].angle.z;

	/* calculate 3d coords of all 4 edges */

	src.x = -DOOR_WIDTH/2;
	src.y = DOOR_HEIGHT/2;
	src.z = 0;
	rotate_3d( &src, &(door->coords[0]), &tmpangle );
	door->coords[0].x += ze->pos.x;
	door->coords[0].y += ze->pos.y;
	door->coords[0].z += ze->pos.z;

	src.x = DOOR_WIDTH/2;
	rotate_3d( &src, &(door->coords[1]), &tmpangle );
	door->coords[1].x += ze->pos.x;
	door->coords[1].y += ze->pos.y;
	door->coords[1].z += ze->pos.z;

	src.y = -DOOR_HEIGHT/2;
	rotate_3d( &src, &(door->coords[2]), &tmpangle );
	door->coords[2].x += ze->pos.x;
	door->coords[2].y += ze->pos.y;
	door->coords[2].z += ze->pos.z;

	src.x = -DOOR_WIDTH/2;
	rotate_3d( &src, &(door->coords[3]), &tmpangle );
	door->coords[3].x += ze->pos.x;
	door->coords[3].y += ze->pos.y;
	door->coords[3].z += ze->pos.z;
}

/*
 *  clip the line p1-p2 at the given rectangle
 *
 */
static int clipline( XPoint *p1, XPoint *p2, struct Rect *rect )
{
XPoint new1, new2, tmp;
float m;

	new1 = *p1;
	new2 = *p2;

	/* entire line may not need clipping */

	if( ((new1.x >= rect->lefttop.x) && (new1.x <= rect->rightbottom.x))
	 || ((new1.y >= rect->lefttop.y) && (new1.y <= rect->rightbottom.y))
	 || ((new2.x >= rect->lefttop.x) && (new2.x <= rect->rightbottom.x))
	 || ((new2.y >= rect->lefttop.y) && (new2.y <= rect->rightbottom.y))
	 ) return 1;


	/* first: clip y dimension */

	/* p1 is above p2 */
	if( new1.y > new2.y ) {
		tmp = new1;
		new1 = new2;
		new2 = tmp;
	}

	/* line could be totally out of view */
	if( (new2.y < rect->lefttop.y) || (new1.y > rect->rightbottom.y) ) return 0;

	m = ( new2.x == new1.x ) ? 0 : ((float)(new2.y - new1.y) / (new2.x - new1.x));

	if( new1.y < rect->lefttop.y ) {
		if( m ) new1.x += (rect->lefttop.y - new1.y)/m;
		new1.y = rect->lefttop.y;
	}
	if( new2.y > rect->rightbottom.y ) {
		if( m ) new2.x -= (new2.y - rect->rightbottom.y)/m;
		new2.y = rect->rightbottom.y;
	}


	/* clip x dimension */

	/* p1 is left to p2 */
	if( new1.x > new2.x ) {
		tmp = new1;
		new1 = new2;
		new2 = tmp;
	}

	if( (new2.x < rect->lefttop.x) || (new1.x > rect->rightbottom.x) ) return 0;

	m = ( new2.x == new1.x ) ? 0 : ((float)(new2.y - new1.y) / (new2.x - new1.x));

	if( new1.x < rect->lefttop.x ) {
		new1.y += (rect->lefttop.y - new1.y)*m;
		new1.x = rect->lefttop.x;
	}
	if( new2.y > rect->rightbottom.y ) {
		new2.y -= (new2.y - rect->rightbottom.y)*m;
		new2.x = rect->rightbottom.x;
	}


	/* push values */

	*p1 = new1;
	*p2 = new2;

	return 1;
}

static void drawdoors( struct ModeInfo_s *mi )
{
Display *display = MI_DISPLAY(mi);
Window window = MI_WINDOW(mi);
GC gc = MI_GC(mi);
int width = MI_WIDTH(mi), height = MI_HEIGHT(mi),
    midx = width/2, midy = height/2;
int i, j;
struct Rect rect = { {0,0}, {0,0} };

        rect.rightbottom.x = width - 1;
        rect.rightbottom.y = height - 1;
	XSetLineAttributes( display, gc, 2, LineSolid, CapNotLast, JoinRound );

#ifdef _DRAW_ZELEMENTS
        XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
        for( i=spectator_zelement; i<ztotal; i++ ) {
                register float proj;
                XPoint p;

                proj = projection(zelements[i].pos.z) * aspect_scale;
                p.x = midx + (zelements[i].pos.x * proj / SPACE_XY_FACTOR);
                p.y = midy - (zelements[i].pos.y * proj / SPACE_XY_FACTOR);
                XDrawPoint( display, window, gc, p.x, p.y );
        }
#endif

	for( i=0; i<doorcount; i++ ) {

		register float proj;
		XPoint lines[4], clip1, clip2;

		door_3d(&doors[i]);

		for( j=0; j<4; j++ ) {
			if( doors[i].coords[j].z < 0 ) break;

			proj = projection(doors[i].coords[j].z) * aspect_scale;
			lines[j].x = midx + (doors[i].coords[j].x * proj / SPACE_XY_FACTOR);
			lines[j].y = midy - (doors[i].coords[j].y * proj / SPACE_XY_FACTOR);
		}
		if( j<4 ) continue;

		XSetForeground(display, gc, doors[i].color);

		for( j=0; j<4; j++ ) {
			clip1 = lines[j];
			clip2 = lines[(j+1)%4];
			if( clipline(&clip1, &clip2, &rect) )
				XDrawLine( display, window, gc, clip1.x, clip1.y, clip2.x, clip2.y );
		}
	}
}

static void drawstars( struct ModeInfo_s *mi )
{
Display *display = MI_DISPLAY(mi);
Window window = MI_WINDOW(mi);
GC gc = MI_GC(mi);
int width = MI_WIDTH(mi), height = MI_HEIGHT(mi),
    midx = width/2, midy = height/2;
int i;

	for( i=0; i<starcount; i++ ) {

		float proj;
		struct ZElement *ze = &zelements[stars[i].zelement];
		struct Vec3D tmpvec, coords;
		struct Angle3D tmpangle;
		XPoint lefttop, rightbottom;

		if( !stars[i].draw ) continue;


		/* rotate star around its z-element, then add its position */

		tmpangle.x = ze->angle.x - zelements[spectator_zelement].angle.x;
		tmpangle.y = ze->angle.y - zelements[spectator_zelement].angle.y;
		tmpangle.z = ze->angle.z - zelements[spectator_zelement].angle.z;

		tmpvec.x = stars[i].xpos;
		tmpvec.y = stars[i].ypos;
		tmpvec.z = 0;
		rotate_3d( &tmpvec, &coords, &tmpangle );
		coords.x += ze->pos.x;
		coords.y += ze->pos.y;
		coords.z += ze->pos.z;

		if( coords.z < 0 ) continue;


		/* projection and clipping (trivial for a rectangle) */

		proj = projection(coords.z) * aspect_scale;

		lefttop.x = midx + ((coords.x - stars[i].width/2)  * proj / SPACE_XY_FACTOR);
		lefttop.y = midy - ((coords.y + stars[i].height/2) * proj / SPACE_XY_FACTOR);
		if( lefttop.x < 0 ) lefttop.x = 0;
		else if( lefttop.x >= width ) continue;
		if( lefttop.y < 0 ) lefttop.y = 0;
		else if( lefttop.y >= height ) continue;

		rightbottom.x = midx + ((coords.x + stars[i].width/2)  * proj / SPACE_XY_FACTOR);
		rightbottom.y = midy - ((coords.y - stars[i].height/2) * proj / SPACE_XY_FACTOR);
		if( rightbottom.x < 0 ) continue;
		else if( rightbottom.x >= width ) rightbottom.x = width - 1;
		if( rightbottom.y < 0 ) continue;
		else if( rightbottom.y >= height ) rightbottom.y = height - 1;


		/* in white color, small stars look darker than big stars */

		XSetForeground(display, gc, MI_WHITE_PIXEL(mi) );

		if( (lefttop.x == rightbottom.x) && (lefttop.y == rightbottom.y) ) {
			/* star is exactly 1 pixel */
			XDrawPoint( display, window, gc, lefttop.x, lefttop.y );
		} else if( (rightbottom.x - lefttop.x) + (rightbottom.y - lefttop.y) == 1 ) {
			/* star is 2 pixels wide or high */
			XDrawPoint( display, window, gc, lefttop.x,     lefttop.y );
			XDrawPoint( display, window, gc, rightbottom.x, rightbottom.y );
		} else if( (rightbottom.x - lefttop.x == 1) && (rightbottom.y - lefttop.y == 1) ) {

			/*  star is exactly 2x2 pixels.
			 *  a 2x2 rectangle should be drawn faster by plotting all 4 pixels
			 *  than by filling a rectangle (is this really so under X ?)
			 */

			XDrawPoint( display, window, gc, lefttop.x,     lefttop.y );
			XDrawPoint( display, window, gc, rightbottom.x, lefttop.y );
			XDrawPoint( display, window, gc, lefttop.x,     rightbottom.y );
			XDrawPoint( display, window, gc, rightbottom.x, rightbottom.y );
		} else {
			XFillRectangle(display, window, gc, lefttop.x, lefttop.y,
				rightbottom.x - lefttop.x, rightbottom.y - lefttop.y );
		}
	}
}

void draw_scooter( struct ModeInfo_s *mi )
{
	if( !doors || !zelements || !sintable || !stars || halt_scooter ) return;

	cleardoors(mi);

	shift_elements(mi);

	/* With these scale factors, all doors are sized correctly for any window dimension.
	 * If aspect ratio is not 4:3, the smaller part of the window is used, e.g.:
	 *      window = 1000x600
	 *      => door scale factor is like in a 800x600 window (not 1000x750)
	 */

	if( (float)MI_WIDTH(mi)/MI_HEIGHT(mi) >= (float)ASPECT_SCREENWIDTH/ASPECT_SCREENHEIGHT ) {
		/* window is wider than or equal 4:3 */
		aspect_scale = (float)MI_HEIGHT(mi) / ASPECT_SCREENHEIGHT;
	} else {
		/* window is higher than 4:3 */
		aspect_scale = (float)MI_WIDTH(mi) / ASPECT_SCREENWIDTH;
	}

	drawstars(mi);

	drawdoors(mi);
}

void release_scooter( struct ModeInfo_s *mi )
{
	if( doors ) {
		free(doors);
		doors = NULL;
	}
	if( stars ) {
		free(stars);
		stars = NULL;
	}
	if( zelements ) {
		free(zelements);
		zelements = NULL;
	}
	if( sintable ) {
		free(sintable);
		sintable = NULL;
	}
}

void refresh_scooter( struct ModeInfo_s *mi )
{
	MI_CLEARWINDOW(mi);
}

void change_scooter( struct ModeInfo_s *mi )
{
	halt_scooter = 1 - halt_scooter;
}

#endif /* MODE_scooter */
