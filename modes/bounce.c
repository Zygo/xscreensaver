/* -*- Mode: C; tab-width: 4 -*- */
/* bounce --- bouncing footballs */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)bounce.c	5.00 2000/11/01 xlockmore";

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
 * 01-Nov-2000: Allocation checks
 * 10-May-1997: Compatible with xscreensaver
 * 01-Apr-1997: Curtis Larsen <larsen@rtp3.med.utah.edu>
 *              The modification is only for the inroot option.  It causes
 *              the balls to see  children of the root window and bounce
 *              off of the sides of them.  New windows are only recognized
 *              after every init_bounce, because fvwm did not like xlock
 *              setting SubstructureNotifyMask on root.  I did not fix the
 *              initial placement of balls yet, so they can start out
 *              underneath windows.
 * 18-Sep-1995: tinkered with it to look like bat.c .
 * 15-Jul-1994: cleaned up in time for the final match.
 * 04-Apr-1994: spinning multiple ball version
 *              (I got a lot of help from with the physics of ball to ball
 *              collision looking at the source of xpool from Ismail ARIT
 *              <iarit@tara.mines.colorado.edu>
 * 22-Mar-1994: got rid of flashing problem by only erasing parts of the
 *              image that will not be covered up by the next image.
 * 02-Sep-1993: xlock version David Bagley <bagleyd@tux.org>
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
#include "vis.h"
#include "color.h"
#endif /* STANDALONE */
#include "iostuff.h"

#ifdef MODE_bounce

ModeSpecOpt bounce_opts =
{0, (XrmOptionDescRec *) NULL, 0, (argtype *) NULL, (OptionStruct *) NULL};

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
  if ((bp->pixmaps[bp->init_orients]=\
  XCreateBitmapFromData(display,window,(char *)n,w,h))==None){\
  free_bounce(display,bp); return False;} else {bp->init_orients++;}

/* aliases for vars defined in the bitmap file */
#define BOUNCE_WIDTH     image_width
#define BOUNCE_HEIGHT    image_height
#define BOUNCE_BITS      image_bits

#include "bounce.xbm"

#if defined( USE_XPM ) || defined( USE_XPMINC )
#define BOUNCE_NAME      image_name
#include "bounce.xpm"
#define DEFAULT_XPM 1
#endif

#define MAX_STRENGTH 24
#define FRICTION 24
#define PENETRATION 0.3
#define SLIPAGE 4
#define TIME 32
#define MINBALLS 1
#define MINSIZE 1
#define MINGRIDSIZE 5

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
	int         graphics_format;
	GC          backGC;
	XImage     *logo;
	unsigned long black;
	Colormap    cmap;
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
		if (bp->logo && !bp->pixelmode) {
			XSetForeground(display, bp->backGC, bp->black);
#ifdef FLASH
			XFillRectangle(display, window, bp->backGC,
				ball->xlast, ball->ylast, bp->xs, bp->ys);
#else
			ERASE_IMAGE(display, window, bp->backGC,
				ball->x, ball->y, ball->xlast, ball->ylast,
				bp->xs, bp->ys);
#endif
		} else {
			XSetForeground(display, bp->stippledGC, MI_BLACK_PIXEL(mi));
			XSetStipple(display, bp->stippledGC,
				bp->pixmaps[(bp->pixelmode) ? 0 : ORIENTS]);
			XSetFillStyle(display, bp->stippledGC, FillStippled);
			XSetTSOrigin(display, bp->stippledGC,
				ball->xlast, ball->ylast);
#ifdef FLASH
			XFillRectangle(display, window, bp->stippledGC,
				ball->xlast, ball->ylast, bp->xs, bp->ys);
#else
			ERASE_IMAGE(display, window, bp->stippledGC,
				ball->x, ball->y, ball->xlast, ball->ylast,
				bp->xs, bp->ys);
#endif
		}
	}
	if (bp->logo && !bp->pixelmode) {
	        XSetForeground(display, bp->backGC, ball->color);
                if (bp->logo)
                        (void) XPutImage(display, window, bp->backGC, bp->logo,
                                 0, 0, ball->x, ball->y, bp->xs, bp->ys);
	} else {
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
	}
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
bounce_windows(ModeInfo * mi, bouncestruct * bp)
{
	Window      root, parent, *children;
	unsigned int nchildren;
	int         i;
	int         n;

	if (!MI_IS_INROOT(mi)) {
		bp->nwindow = 0;
		return;
	}
	if (XQueryTree(MI_DISPLAY(mi), MI_WINDOW(mi),
		       &root, &parent, &children, &nchildren) == 0) {	/* failure */
		bp->nwindow = 0;
		return;
	}
	bp->nwindow = nchildren;
	if (bp->windows != NULL)
		(void) free((void *) bp->windows);
	if ((bp->windows = (ballwindow *) malloc(bp->nwindow *
			sizeof (ballwindow))) == NULL) {
		XFree((caddr_t) children);
		bp->nwindow = 0;
		return; 
	}
	for (n = 0, i = 0; i < bp->nwindow; i++) {
		XWindowAttributes att;

/*-
   May give
   X Error of failed request: BadWindow (invalid Window parameter)
   Major opcode of failed request:  3 (X_GetWindowAttributes)
 */
		if (XGetWindowAttributes(MI_DISPLAY(mi), children[i], &att) == 0) {	/* failure */
			XFree((caddr_t) children);
			bp->nwindow = 0;
			(void) free((void *) bp->windows);
			bp->windows = NULL;
			return;
		}
		if ((att.x < 0) || (att.x > bp->width) ||
		    (att.y < 0) || (att.y > bp->height) ||
#if defined(__cplusplus) || defined(c_plusplus)
		    (att.c_class != InputOutput) ||
#else
		    (att.class != InputOutput) ||
#endif
		    (att.map_state != IsViewable)) {
			continue;
		}
		bp->windows[n].x = att.x;
		bp->windows[n].y = att.y;
		bp->windows[n].width = att.width;
		bp->windows[n].height = att.height;
		n++;
	}
	bp->nwindow = n;
	XFree((caddr_t) children);
	return;
}

static void
free_stuff(Display * display, bouncestruct * bp)
{
	int bits;

	for (bits = 0; bits < bp->init_orients; bits++) {
		if (bp->pixmaps[bits] != None) {
			XFreePixmap(display, bp->pixmaps[bits]);
			bp->pixmaps[bits] = None;
		}
	}
	bp->init_orients = 0;
	if (bp->cmap != None) {
		XFreeColormap(display, bp->cmap);
		if (bp->backGC != None) {
			XFreeGC(display, bp->backGC);
			bp->backGC = None;
		}
		bp->cmap = None;
	} else
		bp->backGC = None;
	if (bp->logo != None) {
		destroyImage(&bp->logo, &bp->graphics_format);
		bp->logo = None;
	}
}

static void
free_bounce(Display *display, bouncestruct *bp)
{
	if (bp->balls != NULL) {
		(void) free((void *) bp->balls);
		bp->balls = NULL;
	}
	free_stuff(display, bp);
	if (bp->stippledGC != None) {
		XFreeGC(display, bp->stippledGC);
		bp->stippledGC = None;
	}
	if (bp->windows != NULL) {
		(void) free((void *) bp->windows);
		bp->windows = NULL;
	}

}
static Bool
init_stuff(ModeInfo * mi)
{
        Display    *display = MI_DISPLAY(mi);
        Window      window = MI_WINDOW(mi);
        bouncestruct *bp = &bounces[MI_SCREEN(mi)];
	XGCValues   gcv;

        if (MI_BITMAP(mi) && strlen(MI_BITMAP(mi))) {
                if (bp->logo == None) {
                  getImage(mi, &bp->logo, BOUNCE_WIDTH, BOUNCE_HEIGHT, BOUNCE_BITS,
#if defined( USE_XPM ) || defined( USE_XPMINC )
                         DEFAULT_XPM, BOUNCE_NAME,
#endif
                         &bp->graphics_format, &bp->cmap, &bp->black);
                	if (bp->logo == None) {
				free_bounce(display, bp);
				return False;
			}
		}
	} else {
		BALLBITS(bounce0_bits, bounce0_width, bounce0_height);
		BALLBITS(bounce1_bits, bounce1_width, bounce1_height);
		BALLBITS(bounce2_bits, bounce2_width, bounce2_height);
		BALLBITS(bounce3_bits, bounce3_width, bounce3_height);
		BALLBITS(bouncemask_bits, bouncemask_width, bouncemask_height);
	}
        if (bp->cmap != None) {

#ifndef STANDALONE
                setColormap(display, window, bp->cmap, MI_IS_INWINDOW(mi));
#endif
                if (bp->backGC == None) {
                        gcv.background = bp->black;
                        if ((bp->backGC = XCreateGC(display, window,
					GCBackground, &gcv)) == None) {
				free_bounce(display, bp);
				return False;
                	}
                }
        } else {
                bp->black = MI_BLACK_PIXEL(mi);
                bp->backGC = MI_GC(mi);
        }
	return True;
}

void
init_bounce(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         size = MI_SIZE(mi);
	bouncestruct *bp;
	int         i, tryagain = 0;
	XGCValues   gcv;

	if (bounces == NULL) {
		if ((bounces = (bouncestruct *) calloc(MI_NUM_SCREENS(mi),
					     sizeof (bouncestruct))) == NULL)
			return;
	}
	bp = &bounces[MI_SCREEN(mi)];

	free_stuff(display, bp);
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
	if (bp->balls == NULL) {
		if ((bp->balls = (ballstruct *) malloc(bp->nballs *
				sizeof (ballstruct))) == NULL) {
			free_bounce(display, bp);
			return;
		}
	}
	if (!init_stuff(mi))
		return;
	if (bp->stippledGC == None) {
		gcv.foreground = MI_BLACK_PIXEL(mi);
		gcv.background = MI_BLACK_PIXEL(mi);
		if ((bp->stippledGC = XCreateGC(display, window,
			 GCForeground | GCBackground, &gcv)) == None) {
			free_bounce(display, bp);
			return;
		}
	}
	if (size == 0 ||
	 MINGRIDSIZE * size > bp->width || MINGRIDSIZE * size > bp->height) {
		if (bp->logo) {
                        bp->xs = bp->logo->width;
                        bp->ys = bp->logo->height;
                } else {
                        bp->xs = bounce0_width;
                        bp->ys = bounce0_height;
                }
		if (bp->width > MINGRIDSIZE * bp->xs &&
		    bp->height > MINGRIDSIZE * bp->ys) {
			bp->pixelmode = False;
		} else {
			bp->pixelmode = True;
			bp->ys = MAX(MINSIZE, MIN(bp->width, bp->height) / MINGRIDSIZE);
			bp->xs = bp->ys;
			free_stuff(display, bp);
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
	if (bp->pixelmode) {
		GC fg_gc, bg_gc;

  		if ((bp->pixmaps[0] = XCreatePixmap(display, window,
				bp->xs, bp->ys, 1)) == None) {
			free_bounce(display, bp);
			return;
		}
		bp->init_orients = 1;
		gcv.foreground = 0;
		gcv.background = 1;
		if ((bg_gc = XCreateGC(display, bp->pixmaps[0],
				 GCForeground | GCBackground, &gcv)) == None) {
			free_bounce(display, bp);
			return;
		}
		gcv.foreground = 1;
		gcv.background = 0;
		if ((fg_gc = XCreateGC(display, bp->pixmaps[0],
				 GCForeground | GCBackground, &gcv)) == None) {
			XFreeGC(display, bg_gc);
			free_bounce(display, bp);
			return;
		}
		XFillRectangle(display, bp->pixmaps[0], bg_gc,
			0, 0, bp->xs, bp->ys);
		XFillArc(display, bp->pixmaps[0], fg_gc,
			0, 0, bp->xs, bp->ys, 0, 23040);
		XFreeGC(display, bg_gc);
		XFreeGC(display, fg_gc);
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
	int         i;
	bouncestruct *bp;

	if (bounces == NULL)
		return;
	bp = &bounces[MI_SCREEN(mi)];
	if (bp->balls == NULL)
		return;

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

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_bounce(MI_DISPLAY(mi), &bounces[screen]);
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
