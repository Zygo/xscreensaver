/* -*- Mode: C; tab-width: 4 -*- */
/* eyes --- follow the bouncing Grelb */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)eyes.c	4.07 97/11/24 xlockmore";

#endif

/*-
 * Copyright 1996 by Ron Hitchens <ron@idiom.com>
 *
 * Adapted from the ubiquitous xeyes demo supplied by MIT with the
 * X Window System.
 * That code is Copyright 1991 Massachusetts Institute of Technology.
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
 * 04-Sep-97: Added interactive frobbing with mouse (copied from julia.c)
 * 10-May-97: Compatible with xscreensaver
 * 18-Mar-96: Changes for new hook calling conventions.  Keep per-screen
 *            state information.  Remove global accesses.
 * 21-Feb-96: Recoded to keep an off-screen image for each pair of eyes,
 *            and to only paint the changed parts to the screen.
 *            Allow the Grelb to enter from either side.
 * 18-Feb-96: Got the code into mostly working condition.
 * 15-Feb-96: Had a brainwave, started hacking the xeyes code.
 *
 * Put "random rotations and horizontal shifts in.
 * At first try make the shifts "random" with a weighting scheme favouring
 * shifts towards the most empty region.
 */

#ifdef STANDALONE
#define PROGCLASS "Eyes"
#define HACK_INIT init_eyes
#define HACK_DRAW draw_eyes
#define eyes_opts xlockmore_opts
#define DEFAULTS "*delay: 20000 \n" \
 "*count: -8 \n" \
 "*cycles: 5 \n" \
 "*ncolors: 200 \n" \
 "*bitmap: \n" \
 "*trackmouse: False \n"
#include "xlockmore.h"		/* in xscreensaver distribution */

#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */

#endif /* STANDALONE */
#include "iostuff.h"

#ifdef MODE_eyes

#define DEF_TRACKMOUSE  "False"

static Bool trackmouse;

static XrmOptionDescRec opts[] =
{
	{(char *) "-trackmouse", (char *) ".eyes.trackmouse", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+trackmouse", (char *) ".eyes.trackmouse", XrmoptionNoArg, (caddr_t) "off"}
};

static argtype vars[] =
{
	{(caddr_t *) & trackmouse, (char *) "trackmouse", (char *) "TrackMouse", (char *) DEF_TRACKMOUSE, t_Bool}
};

static OptionStruct desc[] =
{
	{(char *) "-/+trackmouse", (char *) "turn on/off the tracking of the mouse"}
};

ModeSpecOpt eyes_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   eyes_description =
{"eyes", "init_eyes", "draw_eyes", "release_eyes",
 "refresh_eyes", "init_eyes", NULL, &eyes_opts,
 20000, -8, 5, 1, 64, 1.0, "",
 "Shows eyes following a bouncing grelb", 0, NULL};

#endif

/* definitions for the xlock version of xeyes */
#define MAX_EYES		200	/* limit or uses too much memory */
#define MIN_EYE_SIZE		50	/* smallest size eyes can be */
#define FLY_ICON_SIZE		5	/* the size of a fly when iconic */
#define FLY_MAX_SPEED		10	/* max (slowest) delay between steps */
#define FLY_SIDE_LEFT		0	/* enter and leave by left side */
#define FLY_SIDE_RIGHT		1	/* enter and leave by right side */
#define LIFE_MIN		100	/* shortest life, cycles  */
#define LIFE_RANGE		1000	/* range of possible lifetimes */
#define MAX_CYCLES		10	/* max value of cycles */
#define FRICTION		24	/* affects bounciness */

/* definitions from the original MIT xeyes code */
#define NUM_EYES		2
#define EYE_X(n)		((n) * 2.0)
#define EYE_Y(n)		(0.0)
#define EYE_OFFSET		(0.1)	/* padding between eyes */
#define EYE_THICK		(0.175)		/* thickness of eye rim */
#define BALL_WIDTH		(0.3)
#define BALL_PAD		(0.05)
#define EYE_WIDTH		(2.0 - (EYE_THICK + EYE_OFFSET) * 2)
#define EYE_HEIGHT		EYE_WIDTH
#define EYE_HWIDTH		(EYE_WIDTH / 2.0)
#define EYE_HHEIGHT		(EYE_HEIGHT / 2.0)
#define BALL_HEIGHT		BALL_WIDTH
#define BALL_DIST		((EYE_WIDTH - BALL_WIDTH) / 2.0 - BALL_PAD)
#define W_MIN_X			(-1.0 + EYE_OFFSET)
#define W_MAX_X			(3.0 - EYE_OFFSET)
#define W_MIN_Y			(-1.0 + EYE_OFFSET)
#define W_MAX_Y			(1.0 - EYE_OFFSET)

/* ---------------------------------------------------------------------- */

/* definitions of matrix math code used by xeyes */

#define TPointEqual(a, b)	((a).x == (b).x && (a).y == (b).y)
#define XPointEqual(a, b)	((a).x == (b).x && (a).y == (b).y)

typedef struct _transform {
	double      mx, bx;
	double      my, by;
} Transform;

typedef struct _TPoint {
	double      x, y;
} TPoint;

#define Xx(x,y,t)		((int)((t)->mx * (x) + (t)->bx + 0.5))
#define Xy(x,y,t)		((int)((t)->my * (y) + (t)->by + 0.5))
#define Xwidth(w,h,t)		((int)((t)->mx * (w) + 0.5))
#define Xheight(w,h,t)		((int)((t)->my * (h) + 0.5))
#define Tx(x,y,t)		((((double) (x)) - (t)->bx) / (t)->mx)
#define Ty(x,y,t)		((((double) (y)) - (t)->by) / (t)->my)
#define Twidth(w,h,t)		(((double) (w)) / (t)->mx)
#define Theight(w,h,t)		(((double) (h)) / (t)->my)

/* ---------------------------------------------------------------------- */

/* aliases for vars defined in the bitmap file */
#define FLY_WIDTH		image_width
#define FLY_HEIGHT		image_height
#define FLY_BITS		image_bits

#include "eyes.xbm"
#ifdef XBM_GRELB
#include "eyes2.xbm"
#define FLY2_WIDTH   image2_width
#define FLY2_HEIGHT  image2_height
#define FLY2_BITS    image2_bits
#endif

typedef struct {		/* info about a "fly" */
	int         x, y;
	int         oldx, oldy;
	int         width, height;
	int         vx, vy;
	int         side;
	unsigned long pixel;
	int         zero_y;
} Fly;

typedef struct {		/* info about a pair of eyes */
	int         x, y;
	int         width;
	int         height;
	int         rectw, recth;
	int         painted;
	unsigned long time_to_die;
	unsigned long eyelid_pixel, eyeball_pixel, pupil_pixel;
	Pixmap      pixmap;
	XRectangle  bbox;
	Transform   transform;
	TPoint      pupil[2];
	TPoint      last_pupil[2];
} Eyes;

typedef struct {		/* per-screen info */
	Pixmap      flypix;
	int         flywidth, flyheight;
#ifdef XBM_GRELB
	Pixmap      fly2pix;
	int         fly2width, fly2height;
#endif
	int         graphics_format;
	GC          eyeGC;
	GC          flyGC;
	int         num_eyes;
	Eyes       *eyes;
	Fly         fly;
	Cursor      cursor;
	unsigned long time;
} EyeScrInfo;

/* ---------------------------------------------------------------------- */

static EyeScrInfo *eye_info = NULL;

static int  stuff_alloced = False;	/* is stuff here alloced */

/* ---------------------------------------------------------------------- */

/*-
 *    Fill an arc, using a tranformation matrix.  Lifted from xeyes.
 *      The code to return the bounding box is a local addition.
 */

static void
TFillArc(register Display * dpy, Drawable d, GC gc, Transform * t, double x, double y, double width, double height, int angle1, int angle2, XRectangle * rect)
{
	int         xx, xy, xw, xh;

	xx = Xx(x, y, t);
	xy = Xy(x, y, t);
	xw = Xwidth(width, height, t);
	xh = Xheight(width, height, t);
	if (xw < 0) {
		xx += xw;
		xw = -xw;
	}
	if (xh < 0) {
		xy += xh;
		xh = -xh;
	}
	XFillArc(dpy, d, gc, xx, xy, xw, xh, angle1, angle2);

	if (rect != NULL) {
		rect->x = xx;
		rect->y = xy;
		rect->width = xw;
		rect->height = xh;
	}
}


/*-
 *    Set a tranform matrix from the given arguments.  Lifted from xeyes.
 */

static void
SetTransform(Transform * t, int xx1, int xx2, int xy1, int xy2, double tx1, double tx2, double ty1, double ty2)
{
	t->mx = ((double) xx2 - xx1) / (tx2 - tx1);
	t->bx = ((double) xx1) - t->mx * tx1;
	t->my = ((double) xy2 - xy1) / (ty2 - ty1);
	t->by = ((double) xy1) - t->my * ty1;
}

/* ---------------------------------------------------------------------- */

/*-
 *    Given two rectangles, return the rectangle which encloses both.
 *      Used to clculate "damage" when the pupil moves, to minimize the
 *      number of pixels which must be copied out to the screen.
 */

static void
join_rects(XRectangle * r1, XRectangle * r2, XRectangle * ret)
{
	XRectangle  tmp;
	int         n1, n2;

	/* find min x and min y */
	tmp.x = (r1->x <= r2->x) ? r1->x : r2->x;
	tmp.y = (r1->y <= r2->y) ? r1->y : r2->y;
	/* find max x, plus one (just past the right side) */
	n1 = r1->x + r1->width;
	n2 = r2->x + r2->width;
	/* compute width, relative to min x (left side) */
	tmp.width = ((n1 > n2) ? n1 : n2) - tmp.x;
	/* same for y */
	n1 = r1->y + r1->height;
	n2 = r2->y + r2->height;
	tmp.height = ((n1 > n2) ? n1 : n2) - tmp.y;
	*ret = tmp;		/* copy out result rectangle */
}

/* ---------------------------------------------------------------------- */

/*-
 *    Do the math to figure out where the pupil should be drawn.
 *      This code lifted intact from the xeyes widget.
 */

#if defined( SVR4 ) || defined( SYSV ) && defined( SYSV386 )
extern double hypot(double, double);

#endif

static  void
computePupil(int num, TPoint mouse, TPoint *ret)
{
	double      cx, cy;
	double      dist;
	double      angle;
	double      x, y;
	double      h;
	double      dx, dy;
	double      cosa, sina;

	dx = mouse.x - EYE_X(num);
	dy = mouse.y - EYE_Y(num);
	if (dx == 0 && dy == 0) {
		cx = EYE_X(num);
		cy = EYE_Y(num);
	} else {
		/* Avoid atan2: DOMAIN error message */
		if (dx == 0.0 && dy == 0.0)
			angle = 0.0;
		else
			angle = atan2((double) dy, (double) dx);
		cosa = cos(angle);
		sina = sin(angle);
		h = hypot(EYE_HHEIGHT * cosa, EYE_HWIDTH * sina);
		x = (EYE_HWIDTH * EYE_HHEIGHT) * cosa / h;
		y = (EYE_HWIDTH * EYE_HHEIGHT) * sina / h;
		dist = BALL_DIST * hypot(x, y);
		if (dist > hypot((double) dx, (double) dy)) {
			cx = dx + EYE_X(num);
			cy = dy + EYE_Y(num);
		} else {
			cx = dist * cosa + EYE_X(num);
			cy = dist * sina + EYE_Y(num);
		}
	}
	(*ret).x = cx;
	(*ret).y = cy;
}

/* ---------------------------------------------------------------------- */

/*-
 *    Create the eye image, using the data in the structure pointed
 *      to by "e", in the Drawable "d".  The "full" flag indicates
 *      whether to create the full eye image, or to just paint the
 *      pupil in a new position.
 */

static void
make_eye(ModeInfo * mi, Drawable d, Eyes * e, int n, int full)
{
	EyeScrInfo *ep = &eye_info[MI_SCREEN(mi)];
	Display    *display = MI_DISPLAY(mi);
	GC          gc = ep->eyeGC;
	XRectangle *bbox = &e->bbox;
	XRectangle  tmp1, tmp2;

	if (full) {
		/* draw the outer (eyelid) oval */
		XSetForeground(display, gc, e->eyelid_pixel);
		TFillArc(display, d, gc, &e->transform,
			 EYE_X(n) - EYE_HWIDTH - EYE_THICK,
			 EYE_Y(n) - EYE_HHEIGHT - EYE_THICK,
			 EYE_WIDTH + EYE_THICK * 2.0,
			 EYE_HEIGHT + EYE_THICK * 2.0,
			 90 * 64, 360 * 64, &tmp1);

		/* draw the inner (eyeball) oval */
		XSetForeground(display, gc, e->eyeball_pixel);
		TFillArc(display, d, gc, &e->transform,
			 EYE_X(n) - EYE_HWIDTH, EYE_Y(n) - EYE_HHEIGHT,
			 EYE_WIDTH, EYE_HEIGHT, 90 * 64, 360 * 64, &tmp2);

		join_rects(&tmp1, &tmp2, &tmp1);

		/* draw the pupil on top of the eyeball oval */
		XSetForeground(display, gc, e->pupil_pixel);
		TFillArc(display, d, gc, &e->transform,
			 e->pupil[n].x - BALL_WIDTH / 2.0,
			 e->pupil[n].y - BALL_HEIGHT / 2.0,
			 BALL_WIDTH, BALL_HEIGHT, 90 * 64, 360 * 64, &tmp2);

		join_rects(&tmp1, &tmp2, bbox);
	} else {
		/* undraw the pupil */
		XSetForeground(display, gc, e->eyeball_pixel);
		TFillArc(display, d, gc, &e->transform,
			 e->last_pupil[n].x - BALL_WIDTH / 2.0,
			 e->last_pupil[n].y - BALL_HEIGHT / 2.0,
			 BALL_WIDTH, BALL_HEIGHT, 90 * 64, 360 * 64, &tmp1);

		/* draw the pupil on top of the eyeball oval */
		XSetForeground(display, gc, e->pupil_pixel);
		TFillArc(display, d, gc, &e->transform,
			 e->pupil[n].x - BALL_WIDTH / 2.0,
			 e->pupil[n].y - BALL_HEIGHT / 2.0,
			 BALL_WIDTH, BALL_HEIGHT, 90 * 64, 360 * 64, &tmp2);

		join_rects(&tmp1, &tmp2, bbox);
	}
}

/* ---------------------------------------------------------------------- */

/*-
 *    Check to see if the flyer touches this pair of eyes.
 */

static      Bool
fly_touches(Fly * f, Eyes * e, int old)
{
	int         x = (old) ? f->oldx : f->x;
	int         y = (old) ? f->oldy : f->y;

	if ((x + f->width) <= e->x)
		return (False);
	if (x >= (e->x + e->width))
		return (False);
	if ((y + f->height) <= e->y)
		return (False);
	if (y >= (e->y + e->height))
		return (False);
	return (True);
}

static      Bool
fly_touches_eye(Fly * f, Eyes * e)
{
	if (fly_touches(f, e, True) || fly_touches(f, e, False)) {
		return (True);
	}
	return (False);
}

/*-
 *    Check to see if two pairs of eyes overlap.
 */

static      Bool
eyes_overlap(Eyes * e1, Eyes * e2)
{
	if ((e1->x + e1->width) < e2->x)
		return (False);
	if (e1->x >= (e2->x + e2->width))
		return (False);
	if ((e1->y + e1->height) < e2->y)
		return (False);
	if (e1->y >= (e2->y + e2->height))
		return (False);
	return (True);
}

/* ---------------------------------------------------------------------- */

/*-
 *    Initialize the flyer.  Called when the window changes, and
 *      whenever she bounces off the screen.
 *      In the first version, the eyes followed a "fly", which was
 *      just a flickering spot that moved at random.  That didn't
 *      work so well.  It was replaced with a bouncing gelb, but the
 *      name "fly" has yet to be purged.
 */

static void
init_fly(ModeInfo * mi, Fly * f)
{
	EyeScrInfo *ep = &eye_info[MI_SCREEN(mi)];
	int         win_width = MI_WIDTH(mi);
	int         win_height = MI_HEIGHT(mi);

	(void) memset((char *) f, 0, sizeof (Fly));	/* clear everything to zero */

	f->side = FLY_SIDE_LEFT;

	if (MI_IS_ICONIC(mi)) {
		/* image is just a dot when iconic */
		f->width = f->height = FLY_ICON_SIZE;
		f->vx = NRAND(4) + 1;	/* slower when iconic */
	} else {
		f->width = ep->flywidth;
		f->height = ep->flyheight;
		f->vx = NRAND(15) + 1;	/* random horiz velocity */
	}

	f->y = NRAND(win_height);
	if (f->y > (win_height / 2)) {
		f->side = FLY_SIDE_RIGHT;	/* change to right side */
		f->y -= win_height / 2;		/* start in top half */
		f->x = win_width - f->width;	/* move to right of screen */
		f->vx = -(f->vx);	/* flip direction */
	}
	f->oldx = -(f->width);	/* prevent undraw 1st time */

	if (MI_NPIXELS(mi) <= 2) {
		f->pixel = MI_WHITE_PIXEL(mi);	/* always white when mono */
	} else {
		f->pixel = MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));
	}
}

/*-
 *    Unpaint the flyer by painting the image in black.
 */

static void
unpaint_fly(ModeInfo * mi, Fly * f)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);

	if (MI_IS_ICONIC(mi)) {
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		XFillArc(display, window, gc, f->oldx, f->oldy,
			 f->width, f->height, 90 * 64, 360 * 64);
	} else {
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
#ifdef FLASH
		XFillRectangle(display, window, gc,
			       f->oldx, f->oldy, f->width, f->height);
#else
		ERASE_IMAGE(display, window, gc, f->x, f->y,
			    f->oldx, f->oldy, f->width, f->height);
#endif
	}
}

/*-
 *    Paint the bouncing grelb on the screen.  If not in iconic
 *      mode, unpaint the previous image.  When iconic, the fly
 *      doesn't need to be undrawn, because it will always be on top
 *      of the eyes, which are repainted before the fly is painted.
 */

static void
paint_fly(ModeInfo * mi, Fly * f)
{
	EyeScrInfo *ep = &eye_info[MI_SCREEN(mi)];
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         x = f->x, y = f->y;

	if (MI_IS_ICONIC(mi)) {
		/* don't need to unpaint when iconic
		 * ep->flyGC has stipple set, don't use when iconic
		 */
		XSetForeground(display, MI_GC(mi), f->pixel);
		XFillArc(display, window, MI_GC(mi), x, y,
			 f->width, f->height, 90 * 64, 360 * 64);
	} else {
		unpaint_fly(mi, f);
		XSetForeground(display, ep->flyGC, f->pixel);
#ifdef XBM_GRELB
		if (ep->fly2pix != None) {
			XSetStipple(display, ep->flyGC, (f->vy <= 0) ? ep->flypix : ep->fly2pix);
		} else
#endif
			XSetStipple(display, ep->flyGC, ep->flypix);
		XSetTSOrigin(display, ep->flyGC, x, y);
#ifdef FLASH
		XSetFillStyle(display, ep->flyGC, FillStippled);
#else
		XSetFillStyle(display, ep->flyGC, FillOpaqueStippled);
#endif
		XFillRectangle(display, window, ep->flyGC,
			       x, y, f->width, f->height);
		XFlush(display);
	}
}

/*-
 *    Compute the new position of the fly.  The bouncy-boinginess
 *      algorithm is borrowed from the "bounce" (soccer ball) mode.
 */

static void
move_fly(ModeInfo * mi, Fly * f)
{
	int         win_width = MI_WIDTH(mi);
	int         win_height = MI_HEIGHT(mi);
	int         left = (f->side == FLY_SIDE_LEFT) ? -(f->width) : 0;
	int         right = (f->side == FLY_SIDE_RIGHT) ? win_width :
	win_width - f->width;
	Bool        track_p = trackmouse;
	int         cx, cy;


	f->oldx = f->x;		/* remember position before moving, */
	f->oldy = f->y;		/* for unpainting previous image */

	if (track_p) {
		Window      r, c;
		int         rx, ry;
		unsigned int m;

		(void) XQueryPointer(MI_DISPLAY(mi), MI_WINDOW(mi),
				     &r, &c, &rx, &ry, &cx, &cy, &m);
		if (cx <= 0 || cy <= 0 ||
		    cx >= MI_WIDTH(mi) - f->width - 1 ||
		    cy >= MI_HEIGHT(mi) - f->width - 1) {
			track_p = False;
		}
	}
	if (track_p) {
		f->x = cx;
		f->y = cy;
		return;
	}
	f->x += f->vx;		/* apply x velocity */

	if (f->x > right) {
		if (f->side == FLY_SIDE_RIGHT) {
			unpaint_fly(mi, f);	/* went off the edge, reset */
			init_fly(mi, f);
		} else {
			/* Bounce off the right edge */
			f->x = 2 * (win_width - f->width) - f->x;
			f->vx = -f->vx + f->vx / FRICTION;
		}
	} else if (f->x < left) {
		if (f->side == FLY_SIDE_LEFT) {
			unpaint_fly(mi, f);	/* went off the edge, reset */
			init_fly(mi, f);
		} else {
			/* Bounce off the left edge */
			f->x = -f->x;
			f->vx = -f->vx + f->vx / FRICTION;
		}
	}
	f->vy++;		/* gravity, accelerate in y direction */
	f->y += f->vy;		/* apply y velocity */

	if (f->y >= (win_height - f->height)) {
		/* Bounce off the bottom edge */
		f->y = (win_height - f->height);
		f->vy = -f->vy + f->vy / FRICTION;
		/* every once in a while, go apeshit to clean "high lurkers" */
		if (NRAND(50) == 0) {
			f->vy *= 4;
		}
	} else if (f->y < 0) {
		/* Bounce off the top edge */
		f->y = -f->y;
		f->vy = -f->vy + f->vy / FRICTION;
	}
	/* if he settles to the bottom, move him off quick */
	if (abs(f->vy) < 2) {
		if ((f->zero_y++) > 10) {
			f->vx += (f->side == FLY_SIDE_LEFT) ? -1 : 1;
		}
	} else {
		f->zero_y = 0;	/* still bouncing */
	}
}

/* ---------------------------------------------------------------------- */

/*-
 *    Initialize one pair of eyes
 */

static void
create_eyes(ModeInfo * mi, Eyes * e, Eyes * eyes, int num_eyes)
{
	EyeScrInfo *ep = &eye_info[MI_SCREEN(mi)];
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         win_width = MI_WIDTH(mi);
	int         win_height = MI_HEIGHT(mi);
	unsigned long black_pixel = MI_BLACK_PIXEL(mi);
	unsigned long white_pixel = MI_WHITE_PIXEL(mi);
	Bool        iconic = MI_IS_ICONIC(mi);
	Pixmap      pix = e->pixmap;	/* preserve pixmap handle */
	int         w = e->width;	/* remember last w/h */
	int         h = e->height;
	int         npixels = MI_NPIXELS(mi);	/* num colors in colormap */
	int         cycs = MI_CYCLES(mi);	/* affects eye lifetime */
	int         maxw = win_width / 2;	/* widest eyes can be */
	int         color, lid_color;
	int         i;

	(void) memset((char *) e, 0, sizeof (Eyes));	/* wipe everything */
	e->pixmap = pix;	/* remember Pixmap handle */

	/* sanity check the cycles value */
	if (cycs < 1)
		cycs = 1;
	if (cycs > MAX_CYCLES)
		cycs = MAX_CYCLES;
	e->time_to_die = (unsigned long) LIFE_MIN + NRAND(LIFE_RANGE);
	e->time_to_die *= (unsigned long) cycs;		/* multiply life by cycles */
	e->time_to_die += ep->time;

	e->pupil_pixel = black_pixel;	/* pupil is always black */

	if (MI_NPIXELS(mi) <= 2) {
		/* TODO: stipple the eyelid? */
		e->eyelid_pixel = black_pixel;
		e->eyeball_pixel = white_pixel;
	} else {
		lid_color = color = NRAND(npixels);
		e->eyelid_pixel = MI_PIXEL(mi, lid_color);

		while ((color = NRAND(npixels + 5)) == lid_color) {
			/* empty */
		}
		if (color >= npixels) {
			/* give white a little better chance */
			e->eyeball_pixel = white_pixel;
		} else {
			e->eyeball_pixel = MI_PIXEL(mi, color);
		}
	}

	if (iconic) {
		/* only one pair of eyes, fills entire window */
		e->width = win_width;
		e->height = win_height;
	} else {
		if (maxw - MIN_EYE_SIZE > MIN_EYE_SIZE)
			e->width = NRAND(maxw - MIN_EYE_SIZE) + MIN_EYE_SIZE;
		else
			e->width = NRAND(MIN_EYE_SIZE) + MIN_EYE_SIZE;
		e->x = (win_width - e->width > 0) ? NRAND(win_width - e->width) : 0;
		e->height = NRAND(e->width * 3 / 4) + (e->width / 4);
		e->y = (win_height - e->height > 0) ? NRAND(win_height - e->height) : 0;

		/* check for overlap with other eyes */
		for (i = 0; i < num_eyes; i++) {
			if (&eyes[i] == e) {	/* that's me */
				continue;
			}
			if (eyes_overlap(e, &eyes[i])) {
				/* collision, force retry on next cycle */
				e->time_to_die = 0;
				break;
			}
		}
	}

	/* If the Pixmap is smaller than the new size, make it bigger */
	if ((e->width > w) || (e->height > h)) {
		if (e->pixmap != None) {
			XFreePixmap(display, e->pixmap);
		}
		e->pixmap = XCreatePixmap(display, window,
					  e->width, e->height, MI_DEPTH(mi));
		if (e->pixmap == None) {
			e->width = e->height = 0;
			return;
		}
	}
	/* Set the transformation matrix for this set of eyes
	 * If iconic, make the eyes image one pixel shorter and
	 * skinnier, they seem to fit in the icon box better that way.
	 */
	SetTransform(&e->transform, 0, (iconic) ? e->width - 1 : e->width,
		     (iconic) ? e->height - 1 : e->height, 0,
		     W_MIN_X, W_MAX_X, W_MIN_Y, W_MAX_Y);

	/* clear the offscreen pixmap to background color */
	XSetForeground(display, ep->eyeGC, black_pixel);
	XFillRectangle(display, e->pixmap, ep->eyeGC, 0, 0, e->width, e->height);

	/* make the full eye images in the offscreen Pixmap */
	make_eye(mi, e->pixmap, e, 0, True);
	make_eye(mi, e->pixmap, e, 1, True);
}


/*-
 *    Paint an eye pair onto the screen.
 *      This is normally only the change rectangles for each pupil,
 *      unless in iconic mode, which always repaints the full image.
 */

static void
paint_eyes(ModeInfo * mi, Eyes * e, Fly * f, Eyes * eyes, int num_eyes)
{
	EyeScrInfo *ep = &eye_info[MI_SCREEN(mi)];
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = ep->eyeGC;
	Bool        iconic = MI_IS_ICONIC(mi);
	int         focusx = (f->x + (f->width / 2)) - e->x;
	int         focusy = (f->y + (f->height / 2)) - e->y;
	Pixmap      pix = e->pixmap;
	TPoint      point;
	int         i;

	if (pix == None) {
		e->time_to_die = 0;	/* "should not happen" */
	}
	if (ep->time >= e->time_to_die) {
		/* Sorry Bud, your time is up */
		if (e->painted) {
			/* only unpaint it if previously painted */
			XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
			XFillRectangle(display, window, gc, e->x, e->y, e->width, e->height);
		}
		/* randomly place the eyes elsewhere */
		create_eyes(mi, e, eyes, num_eyes);
		pix = e->pixmap;	/* pixmap may have changed */
	}
	/* If the bouncer would intersect this pair of eyes, force the
	 * eyes to move.  This simplifies the code, because we do not
	 * have to deal with drawing the bouncer on top of the eyes.
	 * When trying to do so, there was too much annoying flashing
	 * and ghost images from the undraw.  I decided to observe the
	 * KISS principle and keep it simple.  I think the effect is
	 * better also.
	 * We must draw the flyer on the eyes when iconic, but that is
	 * easy because the eyes repaint the whole box each time.
	 */
	if ((!iconic) && (fly_touches_eye(f, e))) {
		e->time_to_die = 0;
	}
	if (e->time_to_die == 0) {
		return;		/* collides with something */
	}
	/* set the point to look at and compute the pupil position  */
	point.x = Tx(focusx, focusy, &e->transform);
	point.y = Ty(focusx, focusy, &e->transform);
	computePupil(0, point, &(e->pupil[0]));
	computePupil(1, point, &(e->pupil[1]));

	if (e->painted) {
		/* if still looking at the same point, do nothing further */
		if (TPointEqual(e->pupil[0], e->last_pupil[0]) &&
		    TPointEqual(e->pupil[1], e->last_pupil[1])) {
			return;
		}
	}
	for (i = 0; i < 2; i++) {
		/* update the eye, calculates the changed rectangle */
		make_eye(mi, pix, e, i, False);

		/* Only blit the change if the full image has been painted */
		if (e->painted) {
			/* copy the changed rectangle out to the screen */
			XCopyArea(display, pix, window, gc,
				  e->bbox.x, e->bbox.y,
				  (int) e->bbox.width, (int) e->bbox.height,
				  e->x + e->bbox.x, e->y + e->bbox.y);
		}
		/* remember where we're looking, for the next time around */
		e->last_pupil[i] = e->pupil[i];
	}

	/* always do full paint when iconic, eliminates need to track fly */
	if (iconic || (!e->painted)) {
		XCopyArea(display, pix, window, gc, 0, 0,
			  e->width, e->height, e->x, e->y);
	}
	/* when iconic, pretend to never paint, causes full paint each time */
	if (!iconic) {
		e->painted++;	/* note that a paint has been done */
	}
}

/* ---------------------------------------------------------------------- */

/*-
 *    Initialization that only needs to be done once.  If the
 *      release hook is called, this stuff may be freed and this
 *      function will have to allocate it again next time the
 *      init hook is called.
 */

static void
one_time_init(ModeInfo * mi)
{
	if (stuff_alloced) {
		return;		/* only once, dude */
	}
	if (eye_info == NULL) {
		if ((eye_info = (EyeScrInfo *) calloc(MI_NUM_SCREENS(mi),
					       sizeof (EyeScrInfo))) == NULL)
			return;
	}
	stuff_alloced = True;
}

static void
free_eyes(Display * display, EyeScrInfo * ep)
{
	int         en;

	if (ep->eyes) {
		for (en = 0; en < ep->num_eyes; en++)
			if (ep->eyes[en].pixmap != None)
				XFreePixmap(display, ep->eyes[en].pixmap);
		(void) free((void *) ep->eyes);
		ep->eyes = NULL;
	}
}

/*-
 *    Initialize them eyes.  Called each time the window changes.
 */

void
init_eyes(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	EyeScrInfo *ep;
	int         i;

	one_time_init(mi);

	if (!stuff_alloced) {
		/* if an alloc failed in one_time_init(), quit */
		return;
	}
	ep = &eye_info[MI_SCREEN(mi)];

	if (ep->flypix == None) {
		getPixmap(mi, window, FLY_WIDTH, FLY_HEIGHT, FLY_BITS,
			  &(ep->flywidth), &(ep->flyheight), &(ep->flypix),
			  &(ep->graphics_format));
		if (ep->flypix == None) {
			return;
		}
#ifdef XBM_GRELB
		if (ep->graphics_format == IS_XBM) {
			ep->graphics_format =0;
			getPixmap(mi, window,
			  FLY2_WIDTH, FLY2_HEIGHT, FLY2_BITS,
			  &(ep->fly2width), &(ep->fly2height), &(ep->fly2pix),
			  &(ep->graphics_format));
		}
#endif
	}
	if (ep->flyGC == None) {
		XGCValues   gcv;

		gcv.foreground = MI_BLACK_PIXEL(mi);
		gcv.background = MI_BLACK_PIXEL(mi);
		if ((ep->flyGC = XCreateGC(display, window,
				 GCForeground | GCBackground, &gcv)) == None)
			return;
	}
	if (ep->eyeGC == None) {
		if ((ep->eyeGC = XCreateGC(display, window,
			   (unsigned long) 0, (XGCValues *) NULL)) == None) {
			return;
		}
	}
	ep->time = 0;
	/* don't want any exposure events from XCopyArea */
	XSetGraphicsExposures(display, ep->eyeGC, False);

	free_eyes(display, ep);
	if (MI_IS_ICONIC(mi))
		ep->num_eyes = 1;
	else {
		ep->num_eyes = MI_COUNT(mi);
		/* MAX_EYES is used or one may quickly run out of memory */
		if (ep->num_eyes > MAX_EYES)
			ep->num_eyes = MAX_EYES;
		if (ep->num_eyes < 0) {
			if (ep->num_eyes < -MAX_EYES)
				ep->num_eyes = NRAND(MAX_EYES) + 1;
			else
				ep->num_eyes = NRAND(-ep->num_eyes) + 1;	/* Add 1 so its not too boring */
		}
	}
	if (!ep->eyes)
		ep->eyes = (Eyes *) calloc(ep->num_eyes, sizeof (Eyes));

	for (i = 0; i < ep->num_eyes; i++) {	/* place each eye pair */
		/* don't assume None == 0 */
		ep->eyes[i].pixmap = None;
		create_eyes(mi, &(ep->eyes[i]), ep->eyes, ep->num_eyes);
	}

	init_fly(mi, &(ep->fly));	/* init the bouncer */

	if (trackmouse && !ep->cursor) {	/* Create an invisible cursor */
		Pixmap      bit;
		XColor      black;

		black.red = 0;
		black.green = 0;
		black.blue = 0;
		black.flags = DoRed | DoGreen | DoBlue;
		bit = XCreatePixmapFromBitmapData(display, window, "\000", 1, 1,
						  MI_BLACK_PIXEL(mi),
						  MI_BLACK_PIXEL(mi), 1);
		ep->cursor = XCreatePixmapCursor(display, bit, bit, &black, &black,
						 0, 0);
		XFreePixmap(display, bit);
	}
	XDefineCursor(display, window, ep->cursor);

	MI_CLEARWINDOW(mi);
}

/* ---------------------------------------------------------------------- */

/*-
 *    Called by the mainline code periodically to update the display.
 */

void
draw_eyes(ModeInfo * mi)
{
	EyeScrInfo *ep = &eye_info[MI_SCREEN(mi)];
	int         i;

	MI_IS_DRAWN(mi) = True;

	if (!stuff_alloced) {
		/* if startup init didn't finish, do nothing */
		return;
	}
	move_fly(mi, &(ep->fly));
	ep->time++;
	for (i = 0; i < ep->num_eyes; i++) {
		paint_eyes(mi, &(ep->eyes[i]), &(ep->fly), ep->eyes, ep->num_eyes);
	}

	paint_fly(mi, &(ep->fly));
}

/* ---------------------------------------------------------------------- */

/*-
 *    The display is being taken away from us.  Free up malloc'ed
 *      memory and X resources that we've alloc'ed.  Only called
 *      once, we must zap everything for every screen.
 */

void
release_eyes(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);

	if (eye_info != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			EyeScrInfo *ep = &eye_info[screen];

			if (ep->flyGC != None)
				XFreeGC(display, ep->flyGC);
			if (ep->eyeGC != None)
				XFreeGC(display, ep->eyeGC);
			if (ep->flypix != None)
				XFreePixmap(display, ep->flypix);
#ifdef XBM_GRELB
			if (ep->fly2pix != None)
				XFreePixmap(display, ep->fly2pix);
#endif
			free_eyes(display, ep);
			if (ep->cursor)
				XFreeCursor(display, ep->cursor);
		}
		(void) free((void *) eye_info);
	}
	eye_info = NULL;
	stuff_alloced = False;
}

/* ---------------------------------------------------------------------- */

/*-
 *    Called when the mainline xlock code notices possible window
 *      damage.  This hook should take steps to repaint the entire
 *      window (no specific damage area information is provided).
 */

void
refresh_eyes(ModeInfo * mi)
{
	int         screen = MI_SCREEN(mi);
	int         i;

	if (!stuff_alloced) {
		return;		/* paranoia */
	}
	MI_CLEARWINDOW(mi);

	/* simply flag all the eyes as not painted, will repaint next time */
	for (i = 0; i < eye_info[screen].num_eyes; i++) {
		eye_info[screen].eyes[i].painted = False;
	}
}

#endif /* MODE_eyes */
