/* -*- Mode: C; tab-width: 4 -*- */
/* bounce --- bouncing footballs */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)bounce.c	4.07 97/11/24 xlockmore";

#endif

/*-
 * Copyright (c) 1988 by Sun Microsystems
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
 * 10-May-97: Compatible with xscreensaver
 * 01-Apr-97: Curtis Larsen <larsen@rtp3.med.utah.edu>
 * The modification is only for the inroot mode.  It causes the balls to see
 * children of the root window and bounce off of the sides of them.  New
 * windows are only recognized after every init_bounce, because fvwm did not
 * like xlock setting SubstructureNotifyMask on root.  I did not fix the
 * initial placement of balls yet, so they can start out underneath windows.
 * 18-Sep-95: tinkered with it to look like bat.c .
 * 15-Jul-94: cleaned up in time for the final match.
 * 4-Apr-94: spinning multiple ball version
 *             (I got a lot of help from with the physics of ball to ball
 *             collision looking at the source of xpool from Ismail ARIT
 *             <iarit@tara.mines.colorado.edu>
 * 22-Mar-94: got rid of flashing problem by only erasing parts of the image
 *             that will not be in the next image.
 * 2-Sep-93: xlock version David Bagley <bagleyd@tux.org>
 * 1986: Sun Microsystems
 */

/*-
 * original copyright
 * **************************************************************************
 * Copyright 1988 by Sun Microsystems, Inc. Mountain View, CA.
 *
 * All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Sun or MIT not be used in advertising
 * or publicity pertaining to distribution of the software without specific
 * prior written permission. Sun and M.I.T. make no representations about the
 * suitability of this software for any purpose. It is provided "as is"
 * without any express or implied warranty.
 *
 * SUN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * IN NO EVENT SHALL SUN BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * ***************************************************************************
 */

/*-
 * Open for improvement:
 *  include different balls (size and mass)
 *    how about be real crazy and put in an American/Australian football?
 *  should only have 1 bitmap for ball, the others should be generated
 *    as 90 degree rotations.
 *  multiscreen interaction
 */

#ifdef STANDALONE
#define PROGCLASS "Bounce"
#define HACK_INIT init_bounce
#define HACK_DRAW draw_bounce
#define bounce_opts xlockmore_opts
#define DEFAULTS "*delay: 5000 \n" \
 "*count: -10 \n" \
 "*size: 0 \n" \
 "*ncolors: 200 \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */

#endif /* STANDALONE */

#ifdef MODE_bounce

ModeSpecOpt bounce_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   bounce_description =
{"bounce", "init_bounce", "draw_bounce", "release_bounce",
 "refresh_bounce", "init_bounce", NULL, &bounce_opts,
 5000, -10, 1, 0, 64, 1.0, "",
 "Shows bouncing footballs", 0, NULL};

#endif

#include "bitmaps/bounce-0.xbm"
#include "bitmaps/bounce-1.xbm"
#include "bitmaps/bounce-2.xbm"
#include "bitmaps/bounce-3.xbm"
#include "bitmaps/bounce-mask.xbm"
#define BALLBITS(n,w,h)\
  bp->pixmaps[bp->init_orients++]=\
		XCreateBitmapFromData(display,window,(char *)n,w,h)

#define MAX_STRENGTH 24
#define FRICTION 24
#define PENETRATION 0.3
#define SLIPAGE 4
#define TIME 32
#define MINBALLS 1
#define MINSIZE 1
#define MINGRIDSIZE 6

#define ORIENTS 4
#define ORIENTCYCLE 16
#define CCW 1
#define CW (ORIENTS-1)
#define DIR(x)	(((x)>=0)?CCW:CW)
#define SIGN(x)	(((x)>=0)?1:-1)

typedef struct {
	int         x, y;
	int         width, height;
} ballwindow;

typedef struct {
	int         x, y, xlast, ylast, orientlast;
	int         spincount, spindelay, spindir, orient;
	int         vx, vy, vang;
	unsigned long color;
} ballstruct;

typedef struct {
	int         width, height;
	int         nballs;
	int         xs, ys, avgsize;
	int         restartnum;
	int         pixelmode;
	ballstruct *balls;
	GC          stippledGC;
	Pixmap      pixmaps[ORIENTS + 1];
	int         init_orients;
	int         nwindow;
	ballwindow *windows;
} bouncestruct;

static bouncestruct *bounces = NULL;

static void
checkCollision(bouncestruct * bp, int aball)
{
	int         i, amount, spin, d, size;
	double      x, y;

	for (i = 0; i < bp->nballs; i++) {
		if (i != aball) {
			x = (double) (bp->balls[i].x - bp->balls[aball].x);
			y = (double) (bp->balls[i].y - bp->balls[aball].y);
			d = (int) sqrt(x * x + y * y);
			size = bp->avgsize;
			if (d > 0 && d < size) {
				amount = size - d;
				if (amount > PENETRATION * size)
					amount = (int) (PENETRATION * size);
				bp->balls[i].vx += (int) ((double) amount * x / d);
				bp->balls[i].vy += (int) ((double) amount * y / d);
				bp->balls[i].vx -= bp->balls[i].vx / FRICTION;
				bp->balls[i].vy -= bp->balls[i].vy / FRICTION;
				bp->balls[aball].vx -= (int) ((double) amount * x / d);
				bp->balls[aball].vy -= (int) ((double) amount * y / d);
				bp->balls[aball].vx -= bp->balls[aball].vx / FRICTION;
				bp->balls[aball].vy -= bp->balls[aball].vy / FRICTION;
				spin = (bp->balls[i].vang - bp->balls[aball].vang) /
					(2 * size * SLIPAGE);
				bp->balls[i].vang -= spin;
				bp->balls[aball].vang += spin;
				bp->balls[i].spindir = DIR(bp->balls[i].vang);
				bp->balls[aball].spindir = DIR(bp->balls[aball].vang);
				if (!bp->balls[i].vang) {
					bp->balls[i].spindelay = 1;
					bp->balls[i].spindir = 0;
				} else
					bp->balls[i].spindelay = (int) ((double) M_PI *
									bp->avgsize / (ABS(bp->balls[i].vang))) + 1;
				if (!bp->balls[aball].vang) {
					bp->balls[aball].spindelay = 1;
					bp->balls[aball].spindir = 0;
				} else
					bp->balls[aball].spindelay = (int) ((double) M_PI *
									    bp->avgsize / (ABS(bp->balls[aball].vang))) + 1;
				return;
			}
		}
	}
}

static void
drawball(ModeInfo * mi, ballstruct * ball)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	bouncestruct *bp = &bounces[MI_SCREEN(mi)];

	if (ball->xlast != -1) {
		XSetForeground(display, bp->stippledGC, MI_BLACK_PIXEL(mi));
		XSetStipple(display, bp->stippledGC,
			bp->pixmaps[(bp->pixelmode) ? 0 : ORIENTS]);
		XSetFillStyle(display, bp->stippledGC, FillStippled);
		XSetTSOrigin(display, bp->stippledGC, ball->xlast, ball->ylast);
#ifdef FLASH
		XFillRectangle(display, window, bp->stippledGC,
			ball->xlast, ball->ylast, bp->xs, bp->ys);
#else
		ERASE_IMAGE(display, window, bp->stippledGC, ball->x, ball->y,
			ball->xlast, ball->ylast, bp->xs, bp->ys);
#endif
	}
	XSetTSOrigin(display, bp->stippledGC, ball->x, ball->y);
	XSetForeground(display, bp->stippledGC, ball->color);
	XSetStipple(display, bp->stippledGC,
		bp->pixmaps[(bp->pixelmode) ? 0 : ball->orient]);
#ifdef FLASH
	XSetFillStyle(display, bp->stippledGC, FillStippled);
#else
	XSetFillStyle(display, bp->stippledGC, FillOpaqueStippled);
#endif
	XFillRectangle(display, window, bp->stippledGC,
		ball->x, ball->y, bp->xs, bp->ys);
	XFlush(display);
}

static void
spinball(ballstruct * ball, int dir, int *vel, int avgsize)
{
	*vel -= (int) ((*vel + SIGN(*vel * dir) *
		ball->spindelay * ORIENTCYCLE / (M_PI * avgsize)) / SLIPAGE);
	if (*vel) {
		ball->spindir = DIR(*vel * dir);
		ball->vang = *vel * ORIENTCYCLE;
		ball->spindelay = (int) ((double) M_PI * avgsize / (ABS(ball->vang))) + 1;
	} else
		ball->spindir = 0;
}

#define BETWEEN(x, xmin, xmax) (((x) >= (xmin)) && ((x) <= (xmax)))
static void
hit_left_wall(ModeInfo * mi, ballstruct * ball,
	      int ytop, int height, int x, int side)
{
	bouncestruct *bp = &bounces[MI_SCREEN(mi)];

	if ((ball->x <= x) && ((ball->xlast >= x) || side) &&
	    BETWEEN(ball->y, ytop - bp->ys, ytop + height)) {
		/* Bounce off the wall to the left of the ball */

		ball->x = 2 * x - ball->x;
		ball->vx = (ball->vx - (ball->vx * FRICTION)) / FRICTION;
		spinball(ball, -1, &ball->vy, bp->avgsize);
	}
}

static void
hit_right_wall(ModeInfo * mi, ballstruct * ball,
	       int ytop, int height, int x, int side)
{
	bouncestruct *bp = &bounces[MI_SCREEN(mi)];

	x -= bp->xs;		/* account for ball width */

	if ((ball->x >= x) && ((ball->xlast <= x) || side) &&
	    BETWEEN(ball->y, ytop - bp->ys, ytop + height)) {
		/* Bounce off the wall to the right of the ball */

		ball->x = 2 * x - ball->x;
		ball->vx = (ball->vx - (ball->vx * FRICTION)) / FRICTION;
		spinball(ball, 1, &ball->vy, bp->avgsize);
	}
}

static void
hit_top_wall(bouncestruct * bp, ballstruct * ball, int xleft, int width, int y, int side)
{
	if ((ball->y <= y) && ((ball->ylast >= y) || side) &&
	    BETWEEN(ball->x, xleft - bp->xs, xleft + width)) {	/* Bounce off the wall to the top of the ball */
		ball->y = 2 * y - ball->y;
		/* HACK to make it look better for iconified mode */
		if (y == 0) {
			ball->vy = 0;
		} else {
			ball->vy = (ball->vy - (FRICTION * ball->vy)) / FRICTION;
		}
		spinball(ball, 1, &ball->vx, bp->avgsize);
	}
}


static void
hit_bottom_wall(bouncestruct * bp, ballstruct * ball, int xleft, int width, int y, int side)
{
	y -= bp->ys;		/* account for ball height */

	if ((ball->y >= y) && ((ball->ylast <= y) || side) &&
	    BETWEEN(ball->x, xleft - bp->xs, xleft + width)) {	/* Bounce off the wall to the bottom of the ball */
		ball->y = y;
		ball->vy = (ball->vy - (FRICTION * ball->vy)) / FRICTION;
		spinball(ball, -1, &ball->vx, bp->avgsize);
	}
}

static void
moveball(ModeInfo * mi, ballstruct * ball)
{
	bouncestruct *bp = &bounces[MI_SCREEN(mi)];
	int         i;
	ballwindow *win;

	ball->xlast = ball->x;
	ball->ylast = ball->y;
	ball->orientlast = ball->orient;
	ball->x += ball->vx;

	for (i = 0; i < bp->nwindow; i++) {
		win = &bp->windows[i];

		hit_left_wall(mi, ball, win->y, win->height, win->x + win->width, 0);
		hit_right_wall(mi, ball, win->y, win->height, win->x, 0);
	}
	hit_right_wall(mi, ball, 0, bp->height, bp->width, 1);
	hit_left_wall(mi, ball, 0, bp->height, 0, 1);

	ball->vy++;
	ball->y += ball->vy;

	for (i = 0; i < bp->nwindow; i++) {
		win = &bp->windows[i];

		hit_top_wall(bp, ball, win->x, win->width, win->y + win->height, 0);
		hit_bottom_wall(bp, ball, win->x, win->width, win->y, 0);
	}
	hit_top_wall(bp, ball, 0, bp->width, 0, 1);
	hit_bottom_wall(bp, ball, 0, bp->width, bp->height, 1);

	if (ball->spindir) {
		ball->spincount--;
		if (!ball->spincount) {
			ball->orient = (ball->spindir + ball->orient) % ORIENTS;
			ball->spincount = ball->spindelay;
		}
	}
}

static int
collide(bouncestruct * bp, int aball)
{
	int         i, d, x, y;

	for (i = 0; i < aball; i++) {
		x = (bp->balls[i].x - bp->balls[aball].x);
		y = (bp->balls[i].y - bp->balls[aball].y);
		d = (int) sqrt((double) (x * x + y * y));
		if (d < bp->avgsize)
			return i;
	}
	return i;
}

static void
bounce_windows(ModeInfo * mi, bouncestruct * bs)
{
	Window      root, parent, *children;
	unsigned int nchildren;
	int         i;
	int         n;

	if (!MI_IS_INROOT(mi)) {
		bs->nwindow = 0;
		return;
	}
	if (XQueryTree(MI_DISPLAY(mi), MI_WINDOW(mi),
		       &root, &parent, &children, &nchildren) == 0) {	/* failure */
		bs->nwindow = 0;
		return;
	}
	bs->nwindow = nchildren;
	if (bs->windows != NULL) {
		(void) free((void *) bs->windows);
	}
	bs->windows = (ballwindow *) malloc(bs->nwindow * sizeof (ballwindow));
	for (n = 0, i = 0; i < bs->nwindow; i++) {
		XWindowAttributes att;

/*-
   May give
   X Error of failed request: BadWindow (invalid Window parameter)
   Major opcode of failed request:  3 (X_GetWindowAttributes)
 */
		if (XGetWindowAttributes(MI_DISPLAY(mi), children[i], &att) == 0) {	/* failure */
			bs->nwindow = 0;
			return;
		}
		if ((att.x < 0) || (att.x > bs->width) ||
		    (att.y < 0) || (att.y > bs->height) ||
#if defined(__cplusplus) || defined(c_plusplus)
		    (att.c_class != InputOutput) ||
#else
		    (att.class != InputOutput) ||
#endif
		    (att.map_state != IsViewable)) {
			continue;
		}
		bs->windows[n].x = att.x;
		bs->windows[n].y = att.y;
		bs->windows[n].width = att.width;
		bs->windows[n].height = att.height;
		n++;
	}
	bs->nwindow = n;
	XFree((caddr_t) children);
	return;
}

static void
freePixmaps(ModeInfo * mi)
{
	bouncestruct *bp = &bounces[MI_SCREEN(mi)];
	Display    *display = MI_DISPLAY(mi);
	int bits;

	for (bits = 0; bits < bp->init_orients; bits++) {
		XFreePixmap(display, bp->pixmaps[bits]);
		bp->pixmaps[bits] = None;
	}
	bp->init_orients = 0;
}

void
init_bounce(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         size = MI_SIZE(mi);
	bouncestruct *bp;
	XGCValues   gcv;
	int         i, tryagain = 0;

	if (bounces == NULL) {
		if ((bounces = (bouncestruct *) calloc(MI_NUM_SCREENS(mi),
					     sizeof (bouncestruct))) == NULL)
			return;
	}
	bp = &bounces[MI_SCREEN(mi)];

	bp->width = MI_WIDTH(mi);
	bp->height = MI_HEIGHT(mi);
	if (bp->width < 2)
		bp->width = 2;
	if (bp->height < 2)
		bp->height = 2;
	bp->restartnum = TIME;
	bounce_windows(mi, bp);
	bp->nballs = MI_COUNT(mi);
	if (bp->nballs < -MINBALLS) {
		/* if bp->nballs is random ... the size can change */
		if (bp->balls != NULL) {
			(void) free((void *) bp->balls);
			bp->balls = NULL;
		}
		bp->nballs = NRAND(-bp->nballs - MINBALLS + 1) + MINBALLS;
	} else if (bp->nballs < MINBALLS)
		bp->nballs = MINBALLS;
	if (!bp->balls)
		bp->balls = (ballstruct *) malloc(bp->nballs * sizeof (ballstruct));
	if (size == 0 ||
	 MINGRIDSIZE * size > bp->width || MINGRIDSIZE * size > bp->height) {
		if (bp->width > MINGRIDSIZE * bounce0_width &&
		    bp->height > MINGRIDSIZE * bounce0_height) {
			bp->pixelmode = False;
			bp->xs = bounce0_width;
			bp->ys = bounce0_height;
		} else {
			bp->pixelmode = True;
			bp->ys = MAX(MINSIZE, MIN(bp->width, bp->height) / MINGRIDSIZE);
			bp->xs = bp->ys;
		}
	} else {
		bp->pixelmode = True;
		if (size < -MINSIZE)
			bp->ys = NRAND(MIN(-size, MAX(MINSIZE, MIN(bp->width, bp->height) /
				      MINGRIDSIZE)) - MINSIZE + 1) + MINSIZE;
		else if (size < MINSIZE)
			bp->ys = MINSIZE;
		else
			bp->ys = MIN(size, MAX(MINSIZE, MIN(bp->width, bp->height) /
					       MINGRIDSIZE));
		bp->xs = bp->ys;
	}
	freePixmaps(mi);
	if (!bp->stippledGC) {
		gcv.foreground = MI_BLACK_PIXEL(mi);
		gcv.background = MI_BLACK_PIXEL(mi);
		if ((bp->stippledGC = XCreateGC(display, window,
			 GCForeground | GCBackground, &gcv)) == None)
			return;
	}
	if (bp->pixelmode) {
		GC fg_gc, bg_gc;

  		bp->pixmaps[0] = XCreatePixmap(display, window,
			bp->xs, bp->ys, 1);
		bp->init_orients = 1;
		gcv.foreground = 0;
		gcv.background = 1;
		bg_gc = XCreateGC(display, bp->pixmaps[0],
				 GCForeground | GCBackground, &gcv);
		gcv.foreground = 1;
		gcv.background = 0;
		fg_gc = XCreateGC(display, bp->pixmaps[0],
				 GCForeground | GCBackground, &gcv);
		XFillRectangle(display, bp->pixmaps[0], bg_gc,
			0, 0, bp->xs, bp->ys);
		XFillArc(display, bp->pixmaps[0], fg_gc,
			0, 0, bp->xs, bp->ys, 0, 23040);
		XFreeGC(display, bg_gc);
		XFreeGC(display, fg_gc);
	} else {
		BALLBITS(bounce0_bits, bounce0_width, bounce0_height);
		BALLBITS(bounce1_bits, bounce1_width, bounce1_height);
		BALLBITS(bounce2_bits, bounce2_width, bounce2_height);
		BALLBITS(bounce3_bits, bounce3_width, bounce3_height);
		BALLBITS(bouncemask_bits, bouncemask_width, bouncemask_height);
	}
	bp->avgsize = (bp->xs + bp->ys) / 2;
	i = 0;
	while (i < bp->nballs) {
		bp->balls[i].vx = ((LRAND() & 1) ? -1 : 1) * (NRAND(MAX_STRENGTH) + 1);
		bp->balls[i].x = (bp->balls[i].vx >= 0) ? 0 : bp->width - bp->xs;
		bp->balls[i].y = NRAND(bp->height / 2);
		if (i == collide(bp, i) || tryagain >= 8) {
			if (MI_NPIXELS(mi) > 2)
				bp->balls[i].color =
					MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));
			else
				bp->balls[i].color = MI_WHITE_PIXEL(mi);
			bp->balls[i].xlast = -1;
			bp->balls[i].ylast = 0;
			bp->balls[i].orientlast = 0;
			bp->balls[i].spincount = 1;
			bp->balls[i].spindelay = 1;
			bp->balls[i].vy = ((LRAND() & 1) ? -1 : 1) * NRAND(MAX_STRENGTH);
			bp->balls[i].spindir = 0;
			bp->balls[i].vang = 0;
			bp->balls[i].orient = NRAND(ORIENTS);
			i++;
		} else
			tryagain++;
	}
	MI_CLEARWINDOW(mi);
}

void
draw_bounce(ModeInfo * mi)
{
	bouncestruct *bp = &bounces[MI_SCREEN(mi)];
	int         i;

	MI_IS_DRAWN(mi) = True;

	for (i = 0; i < bp->nballs; i++) {
		drawball(mi, &bp->balls[i]);
		moveball(mi, &bp->balls[i]);
	}
	for (i = 0; i < bp->nballs; i++)
		checkCollision(bp, i);
	if (!NRAND(TIME))	/* Put some randomness into the time */
		bp->restartnum--;
	if (!bp->restartnum)
		init_bounce(mi);
}

void
release_bounce(ModeInfo * mi)
{
	if (bounces != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			bouncestruct *bp = &bounces[screen];

			if (bp->balls != NULL)
				(void) free((void *) bp->balls);
			if (bp->stippledGC != NULL) {
				XFreeGC(MI_DISPLAY(mi), bp->stippledGC);
			}
			freePixmaps(mi);
		}
		(void) free((void *) bounces);
		bounces = NULL;
	}
}

void
refresh_bounce(ModeInfo * mi)
{
	bouncestruct *bp = &bounces[MI_SCREEN(mi)];

	MI_CLEARWINDOW(mi);
	bounce_windows(mi, bp);
}

#endif /* MODE_bounce */
