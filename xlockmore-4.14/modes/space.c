/* -*- Mode: C; tab-width: 4 -*- */
/* space --- A journey into deep space */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)space.c  4.07 97/11/24 xlockmore";

#endif

/*-
 * Copyright (c) 1998 by Vincent Caron [Vincent.Caron@ecl1999.ec-lyon.fr]
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

#ifdef STANDALONE
#define PROGCLASS "Space"
#define HACK_INIT init_space
#define HACK_DRAW draw_space
#define space_opts xlockmore_opts
#define DEFAULTS "*delay: 10000 \n" \
 "*count: 100 \n" \
 "*size: 15 \n" \
 "*ncolors: 64 \n" \
 "*use3d: False \n" \
 "*delta3d: 1.5 \n" \
 "*right3d: red \n" \
 "*left3d: blue \n" \
 "*both3d: magenta \n" \
 "*none3d: black \n"
#define SMOOTH_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_space

ModeSpecOpt space_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   space_description =
{"space", "init_space", "draw_space", "release_space",
 "refresh_space", "init_space", NULL, &space_opts,
 10000, 100, 1, 15, 64, 1.0, "",
 "a journey into deep space", 0, NULL};

#endif

#define X_LIMIT 400		/* space coords clipping */
#define Y_LIMIT 300
#define Z_MAX   450
#define Z_MIN   (-330)
#define Z_L2    200		/* far-distance level */
#define Z_L3    (-150)		/* close-distance level */
#define DIST    400		/* observer-starfield distance */
#define TRANS_X_MAX 3		/* cinetic parameters auto-change : */
#define TRANS_Y_MAX 3		/*   defines limits for cinetic parameters randomizer */
#define TRANS_Z_MAX 7		/*   idem  */
#define ROT_X_MAX 60		/*   idem */
#define ROT_Y_MAX 80		/*   idem */
#define ROT_Z_MAX 50		/*   idem */
#define TIME_MIN 500		/*   idem */
#define TIME_AMP 800		/*   idem */
#define DEGREE 0.01/100		/* custom angle unit :) */

typedef struct {
	int         ok;		/* starfield is properly set */
	int         nb;		/* stars number */
	int         originX, originY;	/* screen width/2,height/2 */
	double      zoom;	/* adapt to screen dimensions */
	double     *starX;	/* star buffer */
	double     *starY;	/*   holds X,Y,Z coords */
	double     *starZ;
	int         ddxn, ddyn, ddzn, daxn, dayn, dazn;		/* counters */
	double      dx, ddx, dy, ddy, dz, ddz, ax, dax, ay, day, az, daz;	/* cinetic params */
	int         pixel_nb;	/* number of XPoints in last used pixel buffer */
	XPoint     *stars1;	/* pixel buffer 1 */
	XPoint     *stars1copy;	/*   left eye or 2D mode */
	XPoint     *stars1a;	/*   two buffers in stars1a,stars1b */
	XPoint     *stars1b;
	XPoint     *stars2;	/* pixel buffer 2 */
	XPoint     *stars2copy;	/*   right eye (3D mode only) */
	XPoint     *stars2a;	/*   two buffers in stars2a,stars2b */
	XPoint     *stars2b;
} SpaceStruct;

static SpaceStruct *spaces = NULL;

void
init_space(ModeInfo * mi)
{
	SpaceStruct *sp;
	int         i;
	double      originX, originY, zoom;

	/* allocate a SpaceStruct for every screen */
	if (spaces == NULL) {
		if ((spaces = (SpaceStruct *) calloc(MI_NUM_SCREENS(mi), sizeof (SpaceStruct))) == NULL)
			return;
	}
	sp = &spaces[MI_SCREEN(mi)];
	/* star density is linked to screen surface */
	sp->nb = MI_COUNT(mi);
	originX = sp->originX = MI_WIDTH(mi) / 2;
	originY = sp->originY = MI_HEIGHT(mi) / 2;
	zoom = sp->zoom = (double) MI_WIDTH(mi) * 0.54 + 40;
	sp->ok = 0;

	/* allocate stars buffers for current screen */
	if ((sp->starX = (double *) calloc(sp->nb, sizeof (double))) == NULL)
		            return;
	if ((sp->starY = (double *) calloc(sp->nb, sizeof (double))) == NULL)
		            return;
	if ((sp->starZ = (double *) calloc(sp->nb, sizeof (double))) == NULL)
		            return;

	/* allocate pixels buffers for current screen */
	if ((sp->stars1a = (XPoint *) calloc(9 * sp->nb, sizeof (XPoint))) == NULL)
		return;
	if ((sp->stars1b = (XPoint *) calloc(9 * sp->nb, sizeof (XPoint))) == NULL)
		return;
	if (MI_IS_USE3D(mi)) {
		if ((sp->stars2a = (XPoint *) calloc(9 * sp->nb, sizeof (XPoint))) == NULL)
			return;
		if ((sp->stars2b = (XPoint *) calloc(9 * sp->nb, sizeof (XPoint))) == NULL)
			return;
	}
	sp->ok = 1;		/* mem allocations succeeded */
	sp->pixel_nb = 0;
	sp->stars1 = sp->stars1a;
	sp->stars1copy = sp->stars1b;
	sp->stars2 = sp->stars2a;
	sp->stars2copy = sp->stars2b;

	/* place stars randomly */
	for (i = 0; i < sp->nb; i++) {
		sp->starX[i] = ((double) NRAND(10000) / 5000 - 1) * X_LIMIT;
		sp->starY[i] = ((double) NRAND(10000) / 5000 - 1) * Y_LIMIT;
		sp->starZ[i] = (double) NRAND(10000) / 10000 * (Z_MAX - Z_MIN) + Z_MIN;
	}
	sp->dx = 0;
	sp->ddxn = 0;
	sp->dy = 0;
	sp->ddyn = 0;
	sp->dz = 3;
	sp->ddzn = 0;
	sp->ax = 0;
	sp->daxn = 0;
	sp->ay = 0;
	sp->dayn = 0;
	sp->az = 0;
	sp->dazn = 0;

	/* clear screen */
	MI_CLEARWINDOW(mi);
}

void
draw_space(ModeInfo * mi)
{
	int         i, n, originX, originY, x, x2 = 0, y, IS_SMALL;
	double      _x, _y, _z, cosX, sinX, cosY, sinY, cosZ, sinZ, k, z,
	            zoom;

	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	int         is3D = MI_IS_USE3D(mi);
	double      delta = MI_DELTA3D(mi) * 8;
	SpaceStruct *sp;

	if (spaces == NULL)
		return;
	sp = &spaces[MI_SCREEN(mi)];
	if (!sp->ok)
		return;
	originX = sp->originX;
	originY = sp->originY;
	zoom = sp->zoom;
	IS_SMALL = ((originX * originY) < (160 * 100));

	/* get cos & sin of rotations */
	cosX = COSF(sp->ax);
	sinX = SINF(sp->ax);
	cosY = COSF(sp->ay);
	sinY = SINF(sp->ay);
	cosZ = COSF(sp->az);
	sinZ = SINF(sp->az);

	/* move stars ! */
	for (i = 0; i < sp->nb; i++) {
		_x = sp->starX[i];
		_y = sp->starY[i];
		_z = sp->starZ[i];

		/* X,Y & Z axis rotations */
		k = _y * cosX + _z * sinX;
		_z = _z * cosX - _y * sinX;
		_y = k;
		k = _x * cosY + _z * sinY;
		_z = _z * cosY - _x * sinY;
		_x = k;
		k = _x * cosZ + _y * sinZ;
		_y = _y * cosZ - _x * sinZ;
		_x = k;

		/* translations + space boundary overflow */
		_x += sp->dx;
		if (_x < (-X_LIMIT))
			_x = X_LIMIT;
		else if (_x > X_LIMIT)
			_x = -X_LIMIT;
		_y += sp->dy;
		if (_y < (-Y_LIMIT))
			_y = Y_LIMIT;
		else if (_y > Y_LIMIT)
			_y = -Y_LIMIT;
		_z -= sp->dz;
		if (_z < Z_MIN)
			_z = Z_MAX;
		else if (_z > Z_MAX)
			_z = Z_MIN;

		sp->starX[i] = _x;
		sp->starY[i] = _y;
		sp->starZ[i] = _z;
	}

	/* update translation parameters */
	if (sp->ddxn == 0) {
		k = (double) NRAND(TRANS_X_MAX * 2) - TRANS_X_MAX;
		sp->ddxn = (int) NRAND(TIME_AMP) + TIME_MIN;
		sp->ddx = (k - sp->dx) / sp->ddxn;
	} else {
		sp->dx += sp->ddx;
		sp->ddxn--;
	}
	if (sp->ddyn == 0) {
		k = (double) NRAND(TRANS_Y_MAX * 2) - TRANS_Y_MAX;
		sp->ddyn = (int) NRAND(TIME_AMP) + TIME_MIN;
		sp->ddy = (k - sp->dy) / sp->ddyn;
	} else {
		sp->dy += sp->ddy;
		sp->ddyn--;
	}
	if (sp->ddzn == 0) {
		k = (double) NRAND(TRANS_Z_MAX * 2) - TRANS_Z_MAX;
		sp->ddzn = (int) NRAND(TIME_AMP) + TIME_MIN;
		sp->ddz = (k - sp->dz) / sp->ddzn;
	} else {
		sp->dz += sp->ddz;
		sp->ddzn--;
	}

	/* update rotation parameters */
	if (sp->daxn == 0) {
		k = (double) (NRAND(ROT_X_MAX * 2) - ROT_X_MAX) * DEGREE;
		sp->daxn = (int) NRAND(TIME_AMP) + TIME_MIN;
		sp->dax = (k - sp->ax) / sp->daxn;
	} else {
		sp->ax += sp->dax;
		sp->daxn--;
	}
	if (sp->dayn == 0) {
		k = (double) (NRAND(ROT_Y_MAX * 2) - ROT_Y_MAX) * DEGREE;
		sp->dayn = (int) NRAND(TIME_AMP) + TIME_MIN;
		sp->day = (k - sp->ay) / sp->dayn;
	} else {
		sp->ay += sp->day;
		sp->dayn--;
	}
	if (sp->dazn == 0) {
		k = (double) (NRAND(ROT_Z_MAX * 2) - ROT_Z_MAX) * DEGREE;
		sp->dazn = (int) NRAND(TIME_AMP) + TIME_MIN;
		sp->daz = (k - sp->az) / sp->dazn;
	} else {
		sp->az += sp->daz;
		sp->dazn--;
	}

	/* project stars and create corresponding pixels */
	n = 0;
	for (i = 0; i < sp->nb; i++) {
		z = sp->starZ[i];
		k = zoom / (z + DIST);
		y = sp->stars1[n].y = originY - (int) (k * sp->starY[i]);
		if (is3D) {
			x = sp->stars1[n].x = originX + (int) (k * (sp->starX[i] - delta));
			x2 = sp->stars2[n].x = originX + (int) (k * (sp->starX[i] + delta));
			sp->stars2[n].y = y;
		} else
			x = sp->stars1[n].x = originX + (int) (k * sp->starX[i]);
		n++;
		if (z < Z_L2) {
			/* not to close but closer : use 4 more pixels for this star */
			sp->stars1[n].x = x + 1;
			sp->stars1[n].y = y;
			sp->stars1[n + 1].x = x - 1;
			sp->stars1[n + 1].y = y;
			sp->stars1[n + 2].x = x;
			sp->stars1[n + 2].y = y + 1;
			sp->stars1[n + 3].x = x;
			sp->stars1[n + 3].y = y - 1;
			if (is3D) {
				sp->stars2[n].x = x2 + 1;
				sp->stars2[n].y = y;
				sp->stars2[n + 1].x = x2 - 1;
				sp->stars2[n + 1].y = y;
				sp->stars2[n + 2].x = x2;
				sp->stars2[n + 2].y = y + 1;
				sp->stars2[n + 3].x = x2;
				sp->stars2[n + 3].y = y - 1;
			}
			n += 4;
		}
		if ((z < Z_L3) && (!IS_SMALL)) {
			/* very close : use again 4 more pixels (makes 9 for this star) */
			sp->stars1[n].x = x - 1;
			sp->stars1[n].y = y + 1;
			sp->stars1[n + 1].x = x - 1;
			sp->stars1[n + 1].y = y - 1;
			sp->stars1[n + 2].x = x + 1;
			sp->stars1[n + 2].y = y + 1;
			sp->stars1[n + 3].x = x + 1;
			sp->stars1[n + 3].y = y - 1;
			if (is3D) {
				sp->stars2[n].x = x2 - 1;
				sp->stars2[n].y = y + 1;
				sp->stars2[n + 1].x = x2 - 1;
				sp->stars2[n + 1].y = y - 1;
				sp->stars2[n + 2].x = x2 + 1;
				sp->stars2[n + 2].y = y + 1;
				sp->stars2[n + 3].x = x2 + 1;
				sp->stars2[n + 3].y = y - 1;
			}
			n += 4;
		}
	}

	/* erase pixels with previous pixel buffer */
	if (MI_IS_INSTALL(mi) && is3D)
		XSetForeground(display, gc, MI_NONE_COLOR(mi));
	else
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
	XDrawPoints(display, window, gc, sp->stars1copy, sp->pixel_nb, CoordModeOrigin);
	if (is3D)
		XDrawPoints(display, window, gc, sp->stars2copy, sp->pixel_nb, CoordModeOrigin);

	/* draw pixels */
	sp->pixel_nb = n;
	if (is3D) {
		if (MI_IS_INSTALL(mi))
			XSetFunction(display, gc, GXor);
		XSetForeground(display, gc, MI_LEFT_COLOR(mi));
		XDrawPoints(display, window, gc, sp->stars1, sp->pixel_nb, CoordModeOrigin);
		XSetForeground(display, gc, MI_RIGHT_COLOR(mi));
		XDrawPoints(display, window, gc, sp->stars2, sp->pixel_nb, CoordModeOrigin);
		if (MI_IS_INSTALL(mi))
			XSetFunction(display, gc, GXcopy);
	} else {
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
		XDrawPoints(display, window, gc, sp->stars1, sp->pixel_nb, CoordModeOrigin);
	}
	XFlush(display);

	/* swap pixel buffers a & b */
	if (sp->stars1 == sp->stars1a) {
		sp->stars1 = sp->stars1b;
		sp->stars1copy = sp->stars1a;
	} else {
		sp->stars1 = sp->stars1a;
		sp->stars1copy = sp->stars1b;
	}
	if (sp->stars2 == sp->stars2a) {
		sp->stars2 = sp->stars2b;
		sp->stars2copy = sp->stars2a;
	} else {
		sp->stars2 = sp->stars2a;
		sp->stars2copy = sp->stars2b;
	}
}

void
refresh_space(ModeInfo * mi)
{
	/* Do nothing, it will refresh by itself */
}

void
release_space(ModeInfo * mi)
{
	int         i;
	SpaceStruct *sp;

	if (spaces != NULL) {
		for (i = 0; i < MI_NUM_SCREENS(mi); i++) {
			sp = &spaces[i];
			(void) free((void *) sp->starX);
			(void) free((void *) sp->starY);
			(void) free((void *) sp->starZ);
			(void) free((void *) sp->stars1a);
			(void) free((void *) sp->stars1b);
			if (MI_IS_USE3D(mi)) {
				(void) free((void *) sp->stars2a);
				(void) free((void *) sp->stars2b);
			}
		}
		(void) free((void *) spaces);
		spaces = NULL;
	}
}

#endif /* MODE_space */
