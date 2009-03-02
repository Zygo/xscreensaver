/* -*- Mode: C; tab-width: 4 -*- */
/* star --- flying through an asteroid field */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)star.c	4.07 97/11/24 xlockmore";

#endif

/*-
 * Based on TI Explorer Lisp code by John Nguyen <johnn@hx.lcs.mit.edu>
 * Copyright (c) 1992 by Jamie Zawinski
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
 * 10-Oct-96: Renamed from rock.  Added trek and rock options.
 *            Combined with features from star by Heath Rice
 *            <rice@asl.dl.nec.com>.
 *            The Enterprise flys by from a few different views.
 *            Also have one view of a Romulan ship. 
 *  7-Sep-96:  Fixed problems with 3d mode <theiling@coli.uni-sb.de>
 * 8-May-96: Blue on left instead of green for 3d.  It seems more common
 *           than green.  Use "-left3d Green" if you have the other kind.
 * 17-Jan-96: 3D mode for star thanks to <theiling@coli.uni-sb.de>.
 *            Get out your 3D glasses, Red on right and Blue on left.
 * 14-Apr-95: Jeremie PETIT <petit@aurora.unice.fr> added a "move" feature.
 * 2-Sep-93: xlock version David Bagley <bagleyd@bigfoot.com>
 * 1992:     xscreensaver version Jamie Zawinski <jwz@netscape.com>
 */

/*-
 * original copyright
 * Copyright (c) 1992 by Jamie Zawinski
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#ifdef STANDALONE
#define PROGCLASS "Star"
#define HACK_INIT init_star
#define HACK_DRAW draw_star
#define star_opts xlockmore_opts
#define DEFAULTS "*delay: 40000 \n" \
 "*count: 100 \n" \
 "*size: 100 \n" \
 "*ncolors: 200 \n" \
 "*use3d: False \n" \
 "*delta3d: 1.5 \n" \
 "*right3d: red \n" \
 "*left3d: blue \n" \
 "*both3d: magenta \n" \
 "*none3d: black \n"
#define BRIGHT_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */

#endif /* STANDALONE */

#define DEF_TREK  "50"
#define DEF_ROCK  "False"
#define DEF_STRAIGHT  "False"

static int  trek;
static Bool rock;
static Bool straight;

static XrmOptionDescRec opts[] =
{
	{"-trek", ".star.trek", XrmoptionSepArg, (caddr_t) NULL},
	{"-rock", ".star.rock", XrmoptionNoArg, (caddr_t) "on"},
	{"+rock", ".star.rock", XrmoptionNoArg, (caddr_t) "off"},
	{"-straight", ".star.straight", XrmoptionNoArg, (caddr_t) "on"},
	{"+straight", ".star.straight", XrmoptionNoArg, (caddr_t) "off"}
};

static argtype vars[] =
{
	{(caddr_t *) & trek, "trek", "Trek", DEF_TREK, t_Int},
	{(caddr_t *) & rock, "rock", "Rock", DEF_ROCK, t_Bool},
	{(caddr_t *) & straight, "straight", "Straight", DEF_STRAIGHT, t_Bool}
};

static OptionStruct desc[] =
{
	{"-trek num", "chance of a Star Trek encounter"},
	{"-/+rock", "turn on/off rocks"},
	{"-/+straight", "turn on/off spin and shifting origin"}
};

ModeSpecOpt star_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   star_description =
{"star", "init_star", "draw_star", "release_star",
 "refresh_star", "init_star", NULL, &star_opts,
 40000, 100, 1, 100, 64, 0.3, "",
 "Shows a star field with a twist", 0, NULL};

#endif

typedef struct _BitmapType {
	int         direction, width, height;
} BitmapType;

#include "bitmaps/trek-0.xbm"	/* Enterprise SOUTH EAST */
#include "bitmaps/trek-1.xbm"	/* Enterprise EAST */
#ifdef ROMULAN			/* Bitmap looks messed up to me. */
#include "bitmaps/trek-2.xbm"	/* Romulan (cloaked?) EAST */
#define TREKIES 3
#else
#define TREKIES 2
#endif

static BitmapType trekie[] =
{
	{3, trek0_width, trek0_height},
	{2, trek1_width, trek1_height}
#ifdef ROMULAN
	,
	{2, trek2_width, trek2_height}
#endif
};

/*-
 * For 3d effect get some 3D glasses, right lens red, and left lens blue.
 * Too bad monitors do not emit polarized light.
 */

#define MIN_STARS 1
#define MIN_DEPTH 2		/* stars disappear when they get this close */
#define MAX_DEPTH 60		/* this is where stars appear */
#define MINSIZE 3		/* how small where pixmaps are not used */
#define MAXSIZE 200		/* how big (in pixels) stars are at depth 1 */
#define DEPTH_SCALE 100		/* how many ticks there are between depths */
#define RESOLUTION 1000
#define MAX_DEP 0.3		/* how far the displacement can be (percents) */
#define DIRECTION_CHANGE_RATE 60
#define MAX_DEP_SPEED 5		/* Maximum speed for movement */
#define MOVE_STYLE 0		/* Only 0 and 1. Distinguishes the fact that
				   these are the stars that are moving (1) 
				   or the stars source (0). */

#define GETZDIFF(z) \
        (MI_DELTA3D(mi)*40.0*(1.0-((MAX_DEPTH*DEPTH_SCALE/2)/(z+20.0*DEPTH_SCALE))))
 /* the compiler needs to optimize the calculations here */

/*-
  there's not much point in the above being user-customizable, but those
  numbers might want to be tweaked for displays with an order of magnitude
  higher resolution or compute power.
 */

#define TREKBITS(n,w,h)\
  sp->trekPixmaps[sp->init_treks++]=\
    XCreateBitmapFromData(display,window,(char *)n,w,h)


typedef struct {
	int         real_size;
	int         r;
	unsigned long color;
	int         theta;
	int         depth;
	int         size, x, y;
	int         diff;
} astar;

typedef struct {
	XPoint      loc, delta, size;
} trekstruct;

typedef struct {
	int         current_delta;	/* observer Z rotation */
	int         new_delta;
	int         dchange_tick;
	int         width, height;
	int         midx, midy;
	int         current_dep[2];
	int         speed_dep[2];
	short       direction[2];
	int         rotate_p, speed, nstars;
	float       max_dep;
	int         move_p;
	int         dep_x, dep_y;
	int         max_star_size;
	astar      *astars;
	Pixmap     *pixmaps;
	Pixmap      trekPixmaps[TREKIES];
	int         init_treks;
	int         current_trek;
	GC          stippledGC;
	trekstruct  trek;
} starstruct;

static float cos_array[RESOLUTION], sin_array[RESOLUTION];
static float depths[(MAX_DEPTH + 1) * DEPTH_SCALE];

static starstruct *stars = NULL;

static void star_draw(ModeInfo * mi, astar * astars, int draw_p);
static int  compute_move(starstruct * sp, int axe);

static void
star_compute(ModeInfo * mi, astar * astars)
{
	starstruct *sp = &stars[MI_SCREEN(mi)];
	double      factor = depths[astars->depth];
	double      rsize = astars->real_size * factor;

	astars->size = (int) (rsize + 0.5);
	astars->diff = (int) (int) GETZDIFF(astars->depth);
	astars->x = sp->midx + (int) (cos_array[astars->theta] * astars->r * factor);
	astars->y = sp->midy + (int) (sin_array[astars->theta] * astars->r * factor);
	if (sp->move_p) {
		double      move_factor = (double) (MOVE_STYLE - (double) astars->depth /
			  (double) ((MAX_DEPTH + 1) * (double) DEPTH_SCALE));

		/* move_factor is 0 when the star is close, 1 when far */
		astars->x += (int) ((double) sp->dep_x * move_factor);
		astars->y += (int) ((double) sp->dep_y * move_factor);
	}
}

static void
star_reset(ModeInfo * mi, astar * astars)
{
	starstruct *sp = &stars[MI_SCREEN(mi)];

	astars->real_size = sp->max_star_size;
	astars->r = (int) (RESOLUTION * 0.7 + NRAND(30 * RESOLUTION));
	astars->theta = NRAND(RESOLUTION);
	astars->depth = MAX_DEPTH * DEPTH_SCALE;
	if (MI_NPIXELS(mi) > 2)
		astars->color = MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));
	else
		astars->color = MI_WHITE_PIXEL(mi);
	star_compute(mi, astars);
	star_draw(mi, astars, True);
}

static void
star_tick(ModeInfo * mi, astar * astars, int d)
{
	starstruct *sp = &stars[MI_SCREEN(mi)];

	if (astars->depth > 0) {
		star_draw(mi, astars, False);
		astars->depth -= sp->speed;
		if (sp->rotate_p)
			astars->theta = (astars->theta + d) % RESOLUTION;
		while (astars->theta < 0)
			astars->theta += RESOLUTION;
		if (astars->depth < (MIN_DEPTH * DEPTH_SCALE))
			astars->depth = 0;
		else {
			star_compute(mi, astars);
			star_draw(mi, astars, True);
		}
	} else if (!NRAND(40))
		star_reset(mi, astars);
}

static void
move_trek(starstruct * sp, int direction, int width, int height)
{
	switch (direction) {	/* Format: 0 = N, 1 = NE, etc */
		case 2:	/* EAST */
			sp->trek.loc.x = sp->width;
			sp->trek.loc.y = NRAND(sp->height);
			sp->trek.delta.x = -(NRAND(3) + 1);
			sp->trek.delta.y = NRAND(7) - 4;
			sp->trek.size.x = width;
			sp->trek.size.y = height;
			break;
		case 3:	/* SOUTH EAST */
			if (LRAND() & 1) {	/* Top to Right */
				sp->trek.loc.x = NRAND(sp->width);
				sp->trek.loc.y = -height;
			} else {	/* Left to Bottom */
				sp->trek.loc.x = -width;
				sp->trek.loc.y = NRAND(sp->height);
			}
			sp->trek.delta.x = NRAND(3) + 1;
			sp->trek.delta.y = NRAND(3) + 1;
			sp->trek.size.x = width;
			sp->trek.size.y = height;
			break;
		case 4:	/* SOUTH */
			sp->trek.loc.x = NRAND(sp->width);
			sp->trek.loc.y = sp->height;
			sp->trek.delta.x = NRAND(7) - 4;
			sp->trek.delta.y = -(NRAND(3) + 1);
			sp->trek.size.x = width;
			sp->trek.size.y = height;
			break;
		default:
			(void) printf("not implemented for direction %d", direction);
			break;
	}
}

static void
draw_trek(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	starstruct *sp = &stars[MI_SCREEN(mi)];
	int         new_one = 0;

	if (TREKIES == sp->current_trek)
		if (NRAND(10000) < trek) {
			sp->current_trek = NRAND(TREKIES);
			new_one = 1;
			move_trek(sp, trekie[sp->current_trek].direction,
				  trekie[sp->current_trek].width, trekie[sp->current_trek].height);
		}
	if (TREKIES != sp->current_trek) {
		sp->trek.loc.x += sp->trek.delta.x;
		sp->trek.loc.y += sp->trek.delta.y;

		if ((sp->trek.loc.x < -sp->trek.size.x) ||
		    (sp->trek.loc.y < -sp->trek.size.y) ||
		    (sp->trek.loc.y > sp->height) ||
		    (sp->trek.loc.x > sp->width)) {
			sp->current_trek = TREKIES;
			XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
			XFillRectangle(display, window, gc,
				       sp->trek.loc.x - sp->trek.delta.x,
				       sp->trek.loc.y - sp->trek.delta.y,
				       sp->trek.size.x, sp->trek.size.y);
		} else {
			XSetForeground(display, sp->stippledGC, MI_WHITE_PIXEL(mi));
			XSetTSOrigin(display, sp->stippledGC, sp->trek.loc.x, sp->trek.loc.y);
			XSetStipple(display, sp->stippledGC, sp->trekPixmaps[sp->current_trek]);
			XSetFillStyle(display, sp->stippledGC, FillOpaqueStippled);
			XFillRectangle(display, window, sp->stippledGC,
				       sp->trek.loc.x, sp->trek.loc.y,
				       sp->trek.size.x, sp->trek.size.y);

			if (!new_one) {
				XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
				ERASE_IMAGE(display, window, gc,
					    sp->trek.loc.x, sp->trek.loc.y,
					 (sp->trek.loc.x - sp->trek.delta.x),
					 (sp->trek.loc.y - sp->trek.delta.y),
					    sp->trek.size.x, sp->trek.size.y);

			}
		}
	}
}

static void
star_draw(ModeInfo * mi, astar * astars, int draw_p)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	starstruct *sp = &stars[MI_SCREEN(mi)];

	if (draw_p) {
		if (MI_IS_USE3D(mi)) {
			if (MI_IS_INSTALL(mi))
				XSetForeground(MI_DISPLAY(mi), gc, MI_NONE_COLOR(mi));
			else
				XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		} else
			XSetForeground(display, gc, astars->color);
	} else
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
	if (astars->x <= 0 || astars->y <= 0 ||
	    astars->x >= sp->width || astars->y >= sp->height) {
		/* This means that if a star were to go off the screen at 12:00, but
		   would have been visible at 3:00, it won't come back once the observer
		   rotates around so that the star would have been visible again.
		   Oh well.
		 */
		if (!sp->move_p)
			astars->depth = 0;
		return;
	}
	if (astars->size <= 1) {
		if (MI_IS_USE3D(mi)) {
			if (draw_p) {
				if (MI_IS_INSTALL(mi))
					XSetFunction(display, gc, GXor);
				XSetForeground(display, gc, MI_LEFT_COLOR(mi));
			}
			XDrawPoint(display, window, gc, astars->x - astars->diff, astars->y);
			if (draw_p)
				XSetForeground(display, gc, MI_RIGHT_COLOR(mi));
			XDrawPoint(display, window, gc, astars->x + astars->diff, astars->y);
			if (draw_p && MI_IS_INSTALL(mi))
				XSetFunction(display, gc, GXcopy);
		} else
			XDrawPoint(display, window, gc, astars->x, astars->y);
	} else if (astars->size <= MINSIZE || !draw_p) {
		if (MI_IS_USE3D(mi)) {
			if (draw_p) {
				if (MI_IS_INSTALL(mi))
					XSetFunction(display, gc, GXor);
				XSetForeground(display, gc, MI_LEFT_COLOR(mi));
			}
			XFillRectangle(display, window, gc,
				 astars->x - astars->size / 2 - astars->diff,
				       astars->y - astars->size / 2,
				       astars->size, astars->size);
			if (draw_p)
				XSetForeground(display, gc, MI_RIGHT_COLOR(mi));
			XFillRectangle(display, window, gc,
				 astars->x - astars->size / 2 + astars->diff,
				       astars->y - astars->size / 2,
				       astars->size, astars->size);
			if (draw_p && MI_IS_INSTALL(mi))
				XSetFunction(display, gc, GXcopy);
		} else
			XFillRectangle(display, window, gc,
				       astars->x - astars->size / 2, astars->y - astars->size / 2,
				       astars->size, astars->size);
	} else if (astars->size < sp->max_star_size) {
		if (MI_IS_USE3D(mi)) {
			if (MI_IS_INSTALL(mi))
				XSetFunction(display, gc, GXor);
			XSetForeground(display, gc, MI_LEFT_COLOR(mi));
			XCopyPlane(display, sp->pixmaps[astars->size - MINSIZE], window, gc,
				   0, 0, astars->size, astars->size,
				 astars->x - astars->size / 2 - astars->diff,
				   astars->y - astars->size / 2,
				   1L);
			XSetForeground(display, gc, MI_RIGHT_COLOR(mi));
			XCopyPlane(display, sp->pixmaps[astars->size - MINSIZE], window, gc,
				   0, 0, astars->size, astars->size,
				 astars->x - astars->size / 2 + astars->diff,
				   astars->y - astars->size / 2,
				   1L);
			if (MI_IS_INSTALL(mi))
				XSetFunction(display, gc, GXcopy);
		} else
			XCopyPlane(display, sp->pixmaps[astars->size - MINSIZE], window, gc,
				   0, 0, astars->size, astars->size,
				   astars->x - astars->size / 2, astars->y - astars->size / 2,
				   1L);
	}
}

static void
init_pixmaps(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	starstruct *sp = &stars[MI_SCREEN(mi)];
	int         size = MI_SIZE(mi);
	int         i;
	XGCValues   gcv;
	GC          fg_gc = 0, bg_gc = 0;

	if (size < -MINSIZE)
		sp->max_star_size = NRAND(-size - MINSIZE + 1) + MINSIZE;
	else if (size < MINSIZE)
		sp->max_star_size = MINSIZE;
	else
		sp->max_star_size = size;
	if (sp->max_star_size > MAXSIZE)
		sp->max_star_size = MAXSIZE;
	sp->pixmaps = (Pixmap *) calloc(sp->max_star_size, sizeof (Pixmap));
	for (i = 0; i < sp->max_star_size; i++) {
		int         h = i + MINSIZE;
		Pixmap      p;
		XPoint      points[7];

		if (rock)
			p = XCreatePixmap(display, window, h, h, 1);
		else		/* Dunno why this is required */
			p = XCreatePixmap(display, window, 2 * h, 2 * h, 1);
		sp->pixmaps[i] = p;
		if (!p) {
			(void) fprintf(stderr, "Could not allocate pixmaps, in star\n");
			return;
		}
		if (!fg_gc) {	/* must use drawable of pixmap, not window (fmh) */
			gcv.foreground = 1;
			gcv.background = 0;
			fg_gc = XCreateGC(display, p, GCForeground | GCBackground, &gcv);
		}
		if (!bg_gc) {	/* must use drawable of pixmap, not window (fmh) */
			gcv.foreground = 0;
			gcv.background = 1;
			bg_gc = XCreateGC(display, p, GCForeground | GCBackground, &gcv);
		}
		XFillRectangle(display, p, bg_gc, 0, 0, h, h);
		if (rock) {
			points[0].x = (int) ((double) h * 0.15);
			points[0].y = (int) ((double) h * 0.85);
			points[1].x = (int) ((double) h * 0.00);
			points[1].y = (int) ((double) h * 0.20);
			points[2].x = (int) ((double) h * 0.30);
			points[2].y = (int) ((double) h * 0.00);
			points[3].x = (int) ((double) h * 0.40);
			points[3].y = (int) ((double) h * 0.10);
			points[4].x = (int) ((double) h * 0.90);
			points[4].y = (int) ((double) h * 0.10);
			points[5].x = (int) ((double) h * 1.00);
			points[5].y = (int) ((double) h * 0.55);
			points[6].x = (int) ((double) h * 0.45);
			points[6].y = (int) ((double) h * 1.00);
			XFillPolygon(display, p, fg_gc, points, 7, Nonconvex, CoordModeOrigin);
		} else {
			XFillArc(display, p, fg_gc, 0, 0, h, h, 0, 23040);
		}
	}
	XFreeGC(display, fg_gc);
	XFreeGC(display, bg_gc);
	if (!sp->stippledGC) {
		gcv.foreground = MI_BLACK_PIXEL(mi);
		gcv.background = MI_BLACK_PIXEL(mi);
		if ((sp->stippledGC = XCreateGC(display, window,
				 GCForeground | GCBackground, &gcv)) == None)
			return;
	}
	if (!sp->init_treks && trek) {
/* PURIFY 4.0.1 on SunOS4 reports a 3264 byte memory leak on the next line. *
   PURIFY 4.0.1 on Solaris 2 does not report this memory leak. */
		TREKBITS(trek0_bits, trek0_width, trek0_height);
		TREKBITS(trek1_bits, trek1_width, trek1_height);
#ifdef ROMULAN
		TREKBITS(trek2_bits, trek2_width, trek2_height);
#endif
	}
}

static void
tick_stars(ModeInfo * mi, int d)
{
	starstruct *sp = &stars[MI_SCREEN(mi)];
	int         i;

	if (sp->move_p) {
		sp->dep_x = compute_move(sp, 0);
		sp->dep_y = compute_move(sp, 1);
	}
	for (i = 0; i < sp->nstars; i++)
		star_tick(mi, &sp->astars[i], d);
}

static int
compute_move(starstruct * sp, int axe)
				/* 0 for x, 1 for y */
{
	int         limit[2];
	int         change = 0;

	limit[0] = sp->midx;
	limit[1] = sp->midy;

	sp->current_dep[axe] += sp->speed_dep[axe];	/* We adjust the displacement */

	if (sp->current_dep[axe] > (int) (limit[axe] * sp->max_dep)) {
		if (sp->current_dep[axe] > limit[axe])
			sp->current_dep[axe] = limit[axe];
		sp->direction[axe] = -1;
	}			/* This is when we reach the upper screen limit */
	if (sp->current_dep[axe] < (int) (-limit[axe] * sp->max_dep)) {
		if (sp->current_dep[axe] < -limit[axe])
			sp->current_dep[axe] = -limit[axe];
		sp->direction[axe] = 1;
	}			/* This is when we reach the lower screen limit */
	if (sp->direction[axe] == 1)	/* We adjust the sp->speed_dep */
		sp->speed_dep[axe] += 1;
	else if (sp->direction[axe] == -1)
		sp->speed_dep[axe] -= 1;

	if (sp->speed_dep[axe] > MAX_DEP_SPEED)
		sp->speed_dep[axe] = MAX_DEP_SPEED;
	else if (sp->speed_dep[axe] < -MAX_DEP_SPEED)
		sp->speed_dep[axe] = -MAX_DEP_SPEED;

	if (!straight && !NRAND(DIRECTION_CHANGE_RATE)) {
		/* We change direction */
		change = LRAND() & 1;
		if (change != 1) {
			if (sp->direction[axe] == 0)
				sp->direction[axe] = change - 1;	/* 0 becomes either 1 or -1 */
			else
				sp->direction[axe] = 0;		/* -1 or 1 become 0 */
		}
	}
	return (sp->current_dep[axe]);
}

void
init_star(ModeInfo * mi)
{
	starstruct *sp;
	int         i;
	static int  first = 1;

	if (stars == NULL) {
		if ((stars = (starstruct *) calloc(MI_NUM_SCREENS(mi),
					       sizeof (starstruct))) == NULL)
			return;
	}
	sp = &stars[MI_SCREEN(mi)];

	sp->width = MI_WIDTH(mi);
	sp->height = MI_HEIGHT(mi);
	sp->midx = sp->width / 2;
	sp->midy = sp->height / 2;
	sp->speed = 100;
	sp->rotate_p = !straight;
	sp->max_dep = (straight) ? 0 : MAX_DEP;
	sp->move_p = True;
	sp->dep_x = 0;
	sp->dep_y = 0;
	sp->current_trek = TREKIES;
	sp->nstars = MI_COUNT(mi);
	if (sp->nstars < -MIN_STARS) {
		if (sp->astars) {
			(void) free((void *) sp->astars);
			sp->astars = NULL;
		}
		sp->nstars = NRAND(-sp->nstars - MIN_STARS + 1) + MIN_STARS;
	} else if (sp->nstars < MIN_STARS)
		sp->nstars = MIN_STARS;
	if (sp->speed < 1)
		sp->speed = 1;
	if (sp->speed > 100)
		sp->speed = 100;

	if (first) {
		first = 0;
		for (i = 0; i < RESOLUTION; i++) {
			sin_array[i] = SINF((((float) i) / (RESOLUTION / 2.0)) * M_PI);
			cos_array[i] = COSF((((float) i) / (RESOLUTION / 2.0)) * M_PI);
		}
		/* We actually only need i/speed of these. Oh well. */
		for (i = 1; i < (int) (sizeof (depths) / sizeof (depths[0])); i++)
			depths[i] = (float) atan(((double) 0.5) / (((double) i) / DEPTH_SCALE));
		depths[0] = M_PI_2;	/* avoid division by 0 */
	}
	if (!sp->astars)
		sp->astars = (astar *) calloc(sp->nstars, sizeof (astar));
	if (!sp->pixmaps)
		init_pixmaps(mi);

	/* don't want any exposure events from XCopyPlane */
	XSetGraphicsExposures(MI_DISPLAY(mi), MI_GC(mi), False);
	if (MI_IS_INSTALL(mi) && MI_IS_USE3D(mi)) {
		MI_CLEARWINDOWCOLOR(mi, MI_NONE_COLOR(mi));
	} else {
		MI_CLEARWINDOW(mi);
	}
}

void
draw_star(ModeInfo * mi)
{
	starstruct *sp = &stars[MI_SCREEN(mi)];

	if (sp->current_delta != sp->new_delta) {
		if (sp->dchange_tick++ == 5) {
			sp->dchange_tick = 0;
			if (sp->current_delta < sp->new_delta)
				sp->current_delta++;
			else
				sp->current_delta--;
		}
	} else {
		if (!NRAND(50)) {
			sp->new_delta = (NRAND(11) - 5);
			if (!NRAND(10))
				sp->new_delta *= 5;
		}
	}
	tick_stars(mi, sp->current_delta);
	draw_trek(mi);
}

void
release_star(ModeInfo * mi)
{
	if (stars != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			Display    *display = MI_DISPLAY(mi);
			starstruct *sp = &stars[screen];
			int         i;

			if (sp->astars != NULL)
				(void) free((void *) sp->astars);
			if (sp->pixmaps != NULL) {
				for (i = 0; i < sp->max_star_size; i++)
					XFreePixmap(display, sp->pixmaps[i]);
				(void) free((void *) sp->pixmaps);
			}
			if (sp->stippledGC != NULL)
				XFreeGC(display, sp->stippledGC);
			for (i = 0; i < sp->init_treks; i++)
				XFreePixmap(display, sp->trekPixmaps[i]);
		}
		(void) free((void *) stars);
		stars = NULL;
	}
}

void
refresh_star(ModeInfo * mi)
{
	if (MI_IS_INSTALL(mi) && MI_IS_USE3D(mi)) {
		MI_CLEARWINDOWCOLOR(mi, MI_NONE_COLOR(mi));
	} else {
		MI_CLEARWINDOW(mi);
	}
}
