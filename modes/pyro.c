/* -*- Mode: C; tab-width: 4 -*- */
/* pyro --- fireworks */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)pyro.c	5.00 2000/11/01 xlockmore";

#endif

/*-
 * Copyright (c) 1991 by Patrick J. Naughton.
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
 * 01-Nov-2000: Allocation checks
 * 15-May-1997: jwz@jwz.org: turned into a standalone program.
 * 05-Sep-1996: Added 3d support Henrik Theiling <theiling@coli.uni-sb.de>
 * 16-Mar-1991: Written, received from David Brooks <brooks@osf.org>
 */

/*-
 * The physics of the rockets is a little bogus, but it looks OK.  Each is
 * given an initial velocity impetus.  They decelerate slightly (gravity
 * overcomes the rocket's impulse) and explode as the rocket's main fuse
 * gives out (we could add a ballistic stage, maybe).  The individual
 * stars fan out from the rocket, and they decelerate less quickly.
 * That's called bouyancy, but really it's again a visual preference.
 */

#ifdef STANDALONE
#define PROGCLASS "Pyro"
#define HACK_INIT init_pyro
#define HACK_DRAW draw_pyro
#define pyro_opts xlockmore_opts
#define DEFAULTS "*delay: 15000 \n" \
 "*count: 100 \n" \
 "*size: -3 \n" \
 "*ncolors: 200 \n" \
 "*use3d: False \n" \
 "*delta3d: 1.5 \n" \
 "*right3d: red \n" \
 "*left3d: blue \n" \
 "*both3d: magenta \n" \
 "*none3d: black \n"
#define UNIFORM_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_pyro

ModeSpecOpt pyro_opts =
{0, (XrmOptionDescRec *) NULL, 0, (argtype *) NULL, (OptionStruct *) NULL};

#ifdef USE_MODULES
ModStruct   pyro_description =
{"pyro", "init_pyro", "draw_pyro", "release_pyro",
 "refresh_pyro", "init_pyro", NULL, &pyro_opts,
 15000, 100, 1, -3, 64, 1.0, "",
 "Shows fireworks", 0, NULL};

#endif

#define ORANGE (MI_NPIXELS(mi) / 16)

#define MINROCKETS 1
#define MINSIZE 1

#define SILENT 0
#define REDGLARE 1
#define BURSTINGINAIR 2

#define CLOUD 0
#define DOUBLECLOUD 1
#define COLORCLOUD 2
/* Clearly other types and other fascinating visual effects could be added... */

/* P_xxx parameters represent the reciprocal of the probability... */
#define P_IGNITE 5000		/* ...of ignition per cycle */
#define P_DOUBLECLOUD 10	/* ...of an ignition being double */
#define P_COLORCLOUD 5		/* ...of an ignition being full color */
#define P_MULTI 75		/* ...of an ignition being several @ once */
#define P_FUSILLADE 250		/* ...of an ignition starting a fusillade */

#define ROCKETW 2		/* Dimensions of rocket */
#define ROCKETH 4
#define XVELFACTOR 0.0025	/* Max horizontal velocity / screen width */
#define ZVELFACTOR 0.0025
#define MINZVAL      250	/* absolute minimum of z values */
#define MINZVALSTART 300	/* range where they start */
#define MAXZVALSTART 1000
#define SCREENZ      400
/* the z-component still gets too small sometimes */
#define GETZDIFFAUX(z) (MI_DELTA3D(mi)*20.0*(1.0-(SCREENZ)/(z)))
#define GETZDIFF(z)    ((z)<MINZVAL?GETZDIFFAUX(MINZVAL):GETZDIFFAUX(z))
#define MINYVELFACTOR 0.016	/* Min vertical velocity / screen height */
#define MAXYVELFACTOR 0.018
#define GRAVFACTOR 0.0002	/* delta v / screen height */
#define MINFUSE 50		/* range of fuse lengths for rocket */
#define MAXFUSE 100

#define FUSILFACTOR 10		/* Generate fusillade by reducing P_IGNITE */
#define FUSILLEN 100		/* Length of fusillade, in ignitions */

#define SVELFACTOR 0.1		/* Max star velocity / yvel */
#define BOUYANCY 0.2		/* Reduction in grav deceleration for stars */
#define MAXSTARS 150		/* Number of stars issued from a shell */
#define MINSTARS 50
#define MINSFUSE 50		/* Range of fuse lengths for stars */
#define MAXSFUSE 100

#define INTRAND(min,max) (NRAND((max+1)-(min))+(min))
#define FLOATRAND(min,max) ((min)+((double) LRAND()/((double) MAXRAND))*((max)-(min)))
#define INCZ(val,add) if((val)+(add)>MINZVAL) val+=(add)
#define ADDZ(val,add) (((val)+(add)>MINZVAL)?(val)+(add):(val))
#define TWOPI (2.0*M_PI)

typedef struct {
	unsigned long color;
	float       sx, sy, sz;	/* Distance from notional center  */
	float       sxvel, syvel, szvel;	/* Relative to notional center */
} star;

typedef struct {
	int         state;
	int         shelltype;
	int         fuse;
	float       xvel, yvel, zvel;
	float       x, y, z;
	unsigned long color[2];
	int         nstars;
	XRectangle  Xpoints[2][MAXSTARS];
	XRectangle  Xpointsleft[2][MAXSTARS];
	star        stars[MAXSTARS];
} rocket;

typedef struct {
	int         p_ignite;
	unsigned long rockpixel;
	int         nflying, nrockets;
	int         fusilcount;
	int         width, height;
	int         lmargin, rmargin;
	int         star_size;
	float       minvelx, maxvelx;
	float       minvely, maxvely;
	float       minvelz, maxvelz;
	float       maxsvel;
	float       rockdecel, stardecel;
	rocket     *rockq;
} pyrostruct;

static void shootup(ModeInfo * mi, pyrostruct * pp, rocket * rp);
static void burst(ModeInfo * mi, pyrostruct * pp, rocket * rp);

static pyrostruct *pyros = NULL;
static int  orig_p_ignite;
static int  just_started = True;	/* Greet the user right away */

static void
ignite(ModeInfo * mi, pyrostruct * pp)
{
	rocket     *rp;
	int         multi, shelltype, nstars, fuse, pix;
	unsigned long color[2];
	float       xvel, yvel, x, zvel = 0.0, z = 0.0;

	x = NRAND(pp->width);
	xvel = FLOATRAND(-pp->maxvelx, pp->maxvelx);
/* All this to stop too many rockets going offscreen: */
	if ((x < pp->lmargin && xvel < 0.0) || (x > pp->rmargin && xvel > 0.0))
		xvel = -xvel;
	yvel = FLOATRAND(pp->minvely, pp->maxvely);
	if (MI_IS_USE3D(mi)) {
		z = FLOATRAND(MINZVALSTART, MAXZVALSTART);
		zvel = FLOATRAND(pp->minvelz, pp->maxvelz);
	}
	fuse = INTRAND(MINFUSE, MAXFUSE);
	nstars = INTRAND(MINSTARS, MAXSTARS);
	if (MI_NPIXELS(mi) > 2) {
		pix = NRAND(MI_NPIXELS(mi));
		color[0] = MI_PIXEL(mi, pix);
		color[1] = MI_PIXEL(mi,
			    ((pix + (MI_NPIXELS(mi) / 2)) % MI_NPIXELS(mi)));
	} else {
		color[0] = color[1] = MI_WHITE_PIXEL(mi);
	}

	multi = 1;
	if (NRAND(P_DOUBLECLOUD) == 0)
		shelltype = DOUBLECLOUD;
	else {
		shelltype = CLOUD;
		if (NRAND(P_MULTI) == 0)
			multi = INTRAND(5, 15);
	}
	if (NRAND(P_COLORCLOUD) == 0)
		shelltype |= COLORCLOUD;

	rp = pp->rockq;
	while (multi--) {
		if (pp->nflying >= pp->nrockets)
			return;
		while (rp->state != SILENT)
			rp++;
		pp->nflying++;
		rp->shelltype = shelltype;
		rp->state = REDGLARE;
		rp->color[0] = color[0];
		rp->color[1] = color[1];
		rp->xvel = xvel;
		rp->yvel = FLOATRAND(yvel * 0.97, yvel * 1.03);
		rp->fuse = INTRAND((fuse * 90) / 100, (fuse * 110) / 100);
		rp->x = x + FLOATRAND(multi * 7.6, multi * 8.4);
		rp->y = pp->height - 1;
		if (MI_IS_USE3D(mi)) {
			rp->zvel = FLOATRAND(zvel * 0.97, zvel * 1.03);
			rp->z = z;
		}
		rp->nstars = nstars;
	}
}

static void
animate(ModeInfo * mi, pyrostruct * pp, rocket * rp)
{
	int         starn, diff;
	float       r, theta;

	if (rp->state == REDGLARE) {
		shootup(mi, pp, rp);

/* Handle setup for explosion */
		if (rp->state == BURSTINGINAIR) {
			for (starn = 0; starn < rp->nstars; starn++) {
				rp->stars[starn].sx = rp->stars[starn].sy = 0.0;
				if (MI_IS_USE3D(mi)) {
					rp->stars[starn].sz = 0.0;
					diff = (int) GETZDIFF(rp->z);
					rp->Xpoints[0][starn].x = (int) rp->x + diff;
					rp->Xpointsleft[0][starn].x = (int) rp->x - diff;
					rp->Xpoints[0][starn].y =
						rp->Xpointsleft[0][starn].y = (int) rp->y;
					if (rp->shelltype & DOUBLECLOUD) {
						rp->Xpoints[1][starn].x = (int) rp->x + diff;
						rp->Xpointsleft[1][starn].x = (int) rp->x - diff;
						rp->Xpoints[1][starn].y =
							rp->Xpointsleft[1][starn].y = (int) rp->y;
					}
					/* this isn't really a 3d direction. it's */
					/* very much the same as in worm. */
					r = FLOATRAND(0.0, pp->maxsvel);
					theta = FLOATRAND(0.0, TWOPI);
					rp->stars[starn].sxvel = r * COSF(theta);
					rp->stars[starn].syvel = r * SINF(theta);
					theta = FLOATRAND(0.0, TWOPI);
					rp->stars[starn].szvel = FLOATRAND(0.0, pp->maxsvel) * SINF(theta);
				} else {
					rp->Xpoints[0][starn].x = (int) rp->x;
					rp->Xpoints[0][starn].y = (int) rp->y;
					if ((rp->shelltype & COLORCLOUD) && (MI_NPIXELS(mi) > 2)) {
						if (NRAND(6) < 1)
							rp->stars[starn].color = MI_WHITE_PIXEL(mi);
						else
							rp->stars[starn].color = MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));
					}
					if (rp->shelltype & DOUBLECLOUD) {
						rp->Xpoints[1][starn].x = (int) rp->x;
						rp->Xpoints[1][starn].y = (int) rp->y;
					}
					r = FLOATRAND(0.0, pp->maxsvel);
					theta = FLOATRAND(0.0, TWOPI);
					rp->stars[starn].sxvel = r * COSF(theta);
					rp->stars[starn].syvel = r * SINF(theta);
				}
/* This isn't accurate solid geometry, but it looks OK. */
			}
			rp->fuse = INTRAND(MINSFUSE, MAXSFUSE);
		}
	}
	if (rp->state == BURSTINGINAIR) {
		burst(mi, pp, rp);
	}
}

static void
shootup(ModeInfo * mi, pyrostruct * pp, rocket * rp)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	int         diff, h;

	if (MI_IS_INSTALL(mi) && MI_IS_USE3D(mi))
		XSetForeground(display, gc, MI_NONE_COLOR(mi));
	else
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
	if (MI_IS_USE3D(mi)) {
		diff = (int) GETZDIFF(rp->z);
		XFillRectangle(display, window, gc, (int) (rp->x) + diff, (int) (rp->y),
			       ROCKETW, ROCKETH + 3);
		XFillRectangle(display, window, gc, (int) (rp->x) - diff, (int) (rp->y),
			       ROCKETW, ROCKETH + 3);
	} else
		XFillRectangle(display, window, gc, (int) (rp->x), (int) (rp->y),
			       ROCKETW, ROCKETH + 3);

	if (rp->fuse-- <= 0) {
		rp->state = BURSTINGINAIR;
		return;
	}
	rp->x += rp->xvel;
	rp->y += rp->yvel;
	rp->yvel += pp->rockdecel;
	if (MI_IS_USE3D(mi)) {
		INCZ(rp->z, rp->zvel);
		diff = (int) GETZDIFF(rp->z);
		h = (int) (ROCKETH + NRAND(4));
		if (MI_IS_INSTALL(mi))
			XSetFunction(display, gc, GXor);
		XSetForeground(display, gc, MI_RIGHT_COLOR(mi));
		XFillRectangle(display, window, gc, (int) (rp->x) + diff, (int) (rp->y),
			       ROCKETW, h);
		XSetForeground(display, gc, MI_LEFT_COLOR(mi));
		XFillRectangle(display, window, gc, (int) (rp->x) - diff, (int) (rp->y),
			       ROCKETW, h);
		if (MI_IS_INSTALL(mi))
			XSetFunction(display, gc, GXcopy);
	} else {
		XSetForeground(display, gc, pp->rockpixel);
		XFillRectangle(display, window, gc, (int) (rp->x), (int) (rp->y),
			       ROCKETW, (int) (ROCKETH + NRAND(4)));
	}
}

static void
burst(ModeInfo * mi, pyrostruct * pp, rocket * rp)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	register int starn;
	register int nstars, stype;
	register float rx, ry, rz = 0.0, sd;	/* Help compiler optimize :-) */
	register float sx, sy, sz;
	int         diff;

	nstars = rp->nstars;
	stype = rp->shelltype;
	if (MI_IS_INSTALL(mi) && MI_IS_USE3D(mi))
		XSetForeground(display, gc, MI_NONE_COLOR(mi));
	else
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));

	XFillRectangles(display, window, gc, rp->Xpoints[0], nstars);
	if (stype & DOUBLECLOUD)
		XFillRectangles(display, window, gc, rp->Xpoints[1], nstars);
	if (MI_IS_USE3D(mi)) {
		XFillRectangles(display, window, gc, rp->Xpointsleft[0], nstars);
		if (stype & DOUBLECLOUD)
			XFillRectangles(display, window, gc, rp->Xpointsleft[1], nstars);
	}
	if (rp->fuse-- <= 0) {
		rp->state = SILENT;
		pp->nflying--;
		return;
	}
/* Stagger the stars' decay */
	if (rp->fuse <= 7) {
		if ((rp->nstars = nstars = nstars * 90 / 100) == 0)
			return;
	}
	rx = rp->x;
	ry = rp->y;
	if (MI_IS_USE3D(mi)) {
		rz = rp->z;
	}
	sd = pp->stardecel;
	for (starn = 0; starn < nstars; starn++) {
		sx = rp->stars[starn].sx += rp->stars[starn].sxvel;
		sy = rp->stars[starn].sy += rp->stars[starn].syvel;
		rp->stars[starn].syvel += sd;
		if (MI_IS_USE3D(mi)) {
			if (rz + rp->stars[starn].sz + rp->stars[starn].szvel > MINZVAL)
				rp->stars[starn].sz += rp->stars[starn].szvel;
			sz = rp->stars[starn].sz;
			diff = (int) GETZDIFF(rz + sz);
			rp->Xpoints[0][starn].x = (int) (rx + sx + diff);
			rp->Xpointsleft[0][starn].x = (int) (rx + sx - diff);
			rp->Xpoints[0][starn].y =
				rp->Xpointsleft[0][starn].y = (int) (ry + sy);
			if (stype & DOUBLECLOUD) {
				rp->Xpoints[1][starn].x = (int) (rx + 1.7 * sx) + diff;
				rp->Xpointsleft[1][starn].x = (int) (rx + 1.7 * sx) - diff;
				rp->Xpoints[1][starn].y =
					rp->Xpointsleft[1][starn].y = (int) (ry + 1.7 * sy);
			}
		} else {

			rp->Xpoints[0][starn].x = (int) (rx + sx);
			rp->Xpoints[0][starn].y = (int) (ry + sy);
			if (stype & DOUBLECLOUD) {
				rp->Xpoints[1][starn].x = (int) (rx + 1.7 * sx);
				rp->Xpoints[1][starn].y = (int) (ry + 1.7 * sy);
			}
		}
	}
	rp->x = rx + rp->xvel;
	rp->y = ry + rp->yvel;
	if (MI_IS_USE3D(mi)) {
		rp->z = ADDZ(rz, rp->zvel);
	}
	rp->yvel += sd;

	if (MI_IS_USE3D(mi)) {
		if (MI_IS_INSTALL(mi))
			XSetFunction(display, gc, GXor);
		XSetForeground(display, gc, MI_RIGHT_COLOR(mi));
		XFillRectangles(display, window, gc, rp->Xpoints[0], nstars);
		if (stype & DOUBLECLOUD) {
			XFillRectangles(display, window, gc, rp->Xpoints[1], nstars);
		}
		XSetForeground(display, gc, MI_LEFT_COLOR(mi));
		XFillRectangles(display, window, gc, rp->Xpointsleft[0], nstars);
		if (stype & DOUBLECLOUD) {
			XFillRectangles(display, window, gc, rp->Xpointsleft[1], nstars);
		}
		if (MI_IS_INSTALL(mi))
			XSetFunction(display, gc, GXcopy);
	} else {

		if ((stype & COLORCLOUD) && (MI_NPIXELS(mi) > 2)) {
			for (starn = 0; starn < nstars; starn++) {
				XSetForeground(display, gc, rp->stars[starn].color);
				XFillRectangle(display, window, gc,
					       rp->Xpoints[0][starn].x, rp->Xpoints[0][starn].y,
					       rp->Xpoints[0][starn].width, rp->Xpoints[0][starn].height);
			}
		} else {
			XSetForeground(display, gc, rp->color[0]);
			XFillRectangles(display, window, gc, rp->Xpoints[0], nstars);
		}
		if (stype & DOUBLECLOUD) {
			XSetForeground(display, gc, rp->color[1]);
			XFillRectangles(display, window, gc, rp->Xpoints[1], nstars);
		}
	}
}

void
init_pyro(ModeInfo * mi)
{
	int         rockn, starn;
	int         size = MI_SIZE(mi);
	rocket     *rp;
	pyrostruct *pp;

	if (pyros == NULL) {
		if ((pyros = (pyrostruct *) calloc(MI_NUM_SCREENS(mi),
					       sizeof (pyrostruct))) == NULL)
			return;
	}
	pp = &pyros[MI_SCREEN(mi)];

	pp->width = MI_WIDTH(mi);
	pp->height = MI_HEIGHT(mi);
	pp->lmargin = pp->width / 16;
	pp->rmargin = pp->width - pp->lmargin;

	pp->nrockets = MI_COUNT(mi);
	if (pp->nrockets < -MINROCKETS) {
		if (pp->rockq) {
			(void) free((void *) pp->rockq);
			pp->rockq = NULL;
		}
		pp->nrockets = NRAND(-pp->nrockets - MINROCKETS + 1) + MINROCKETS;
	} else if (pp->nrockets < MINROCKETS)
		pp->nrockets = MINROCKETS;
	if (size < -MINSIZE)
		pp->star_size = NRAND(MIN(-size, MAX(MINSIZE,
		  MIN(pp->width, pp->height) / 64)) - MINSIZE + 1) + MINSIZE;
	else if (size < MINSIZE) {
		if (!size)
			pp->star_size = MAX(MINSIZE, MIN(pp->width, pp->height) / 64);
		else
			pp->star_size = MINSIZE;
	} else
		pp->star_size = MIN(size, MAX(MINSIZE, MIN(pp->width, pp->height) / 64));
	orig_p_ignite = P_IGNITE / pp->nrockets;
	if (orig_p_ignite <= 0)
		orig_p_ignite = 1;
	pp->p_ignite = orig_p_ignite;

	if (!pp->rockq) {
		if ((pp->rockq = (rocket *) malloc(pp->nrockets *
				 sizeof (rocket))) == NULL) {
			return;
		}
	}
	pp->nflying = pp->fusilcount = 0;

	for (rockn = 0, rp = pp->rockq; rockn < pp->nrockets; rockn++, rp++) {
		rp->state = SILENT;
		for (starn = 0; starn < MAXSTARS; starn++) {
			rp->Xpoints[0][starn].width = rp->Xpoints[0][starn].height =
				rp->Xpoints[1][starn].width = rp->Xpoints[1][starn].height =
				pp->star_size;
			if (MI_IS_USE3D(mi)) {
				rp->Xpointsleft[0][starn].width = rp->Xpointsleft[0][starn].height =
					rp->Xpointsleft[1][starn].width = rp->Xpointsleft[1][starn].height =
					pp->star_size;
			}
		}
	}

	if (MI_NPIXELS(mi) > 3)
		pp->rockpixel = MI_PIXEL(mi, ORANGE);
	else
		pp->rockpixel = MI_WHITE_PIXEL(mi);

/* Geometry-dependent physical data: */
	pp->maxvelx = (float) (pp->width) * XVELFACTOR;
	pp->minvelx = -pp->maxvelx;
	pp->minvely = -(float) (pp->height) * MINYVELFACTOR;
	pp->maxvely = -(float) (pp->height) * MAXYVELFACTOR;
	if (MI_IS_USE3D(mi)) {
		pp->maxvelz = (float) (MAXZVALSTART - MINZVALSTART) * ZVELFACTOR;
		pp->minvelz = -pp->maxvelz;
	}
	pp->maxsvel = pp->minvely * SVELFACTOR;
	pp->rockdecel = (float) (pp->height) * GRAVFACTOR;
	pp->stardecel = pp->rockdecel * BOUYANCY;
	if (MI_IS_INSTALL(mi) && MI_IS_USE3D(mi)) {
		MI_CLEARWINDOWCOLOR(mi, MI_NONE_COLOR(mi));
	} else {
		MI_CLEARWINDOW(mi);
	}
}

/* ARGSUSED */
void
draw_pyro(ModeInfo * mi)
{
	rocket     *rp;
	int         rockn;
	pyrostruct *pp;

	if (pyros == NULL)
		return;
	pp = &pyros[MI_SCREEN(mi)];
	if (pp->rockq == NULL)
		return;

	MI_IS_DRAWN(mi) = True;
	if (just_started || (NRAND(pp->p_ignite) == 0)) {
		just_started = False;
		if (NRAND(P_FUSILLADE) == 0) {
			pp->p_ignite = orig_p_ignite / FUSILFACTOR;
			if (pp->p_ignite <= 0)
				pp->p_ignite = 1;
			pp->fusilcount = INTRAND(FUSILLEN * 9 / 10, FUSILLEN * 11 / 10);
		}
		ignite(mi, pp);
		if (pp->fusilcount > 0) {
			if (--pp->fusilcount == 0)
				pp->p_ignite = orig_p_ignite;
		}
	}
	for (rockn = pp->nflying, rp = pp->rockq; rockn > 0; rp++) {
		if (rp->state != SILENT) {
			animate(mi, pp, rp);
			rockn--;
		}
	}
}

void
release_pyro(ModeInfo * mi)
{
	if (pyros != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			pyrostruct *pp = &pyros[screen];

			if (pp->rockq != NULL) {
				(void) free((void *) pp->rockq);
			}
		}
		(void) free((void *) pyros);
		pyros = NULL;
	}
}

void
refresh_pyro(ModeInfo * mi)
{
	if (MI_IS_INSTALL(mi) && MI_IS_USE3D(mi)) {
		MI_CLEARWINDOWCOLOR(mi, MI_NONE_COLOR(mi));
	} else {
		MI_CLEARWINDOW(mi);
	}
}

#endif /* MODE_pyro */
