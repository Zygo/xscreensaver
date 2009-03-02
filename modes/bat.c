/* -*- Mode: C; tab-width: 4 -*- */
/* bat --- bouncing bats */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)bat.c	4.07 97/11/24 xlockmore";

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
 * 18-Sep-95: 5 bats now in color <patol@info.isbiel.ch>
 * 20-Sep-94: 5 bats instead of bouncing balls, based on bounce.c
 *            <patol@info.isbiel.ch>
 * 2-Sep-93: bounce version David Bagley <bagleyd@bigfoot.com>
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

#ifdef STANDALONE
#define PROGCLASS "Bat"
#define HACK_INIT init_bat
#define HACK_DRAW draw_bat
#define bat_opts xlockmore_opts
#define DEFAULTS "*delay: 100000 \n" \
 "*count: -8 \n" \
 "*size: 0 \n" \
 "*ncolors: 200 \n" \
 "*verbose: False \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#include "vis.h"
#include "color.h"
#endif /* STANDALONE */
#include "iostuff.h"

#ifdef MODE_bat

ModeSpecOpt bat_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   bat_description =
{"bat", "init_bat", "draw_bat", "release_bat",
 "refresh_bat", "init_bat", NULL, &bat_opts,
 100000, -8, 1, 0, 64, 1.0, "",
 "Shows bouncing flying bats", 0, NULL};

#endif

#if defined( USE_XPM ) || defined( USE_XPMINC )
#if USE_XPMINC
#include <xpm.h>
#else
#include <X11/xpm.h>		/* Normal spot */
#endif
#include "pixmaps/bat-0.xpm"
#include "pixmaps/bat-1.xpm"
#include "pixmaps/bat-2.xpm"
#include "pixmaps/bat-3.xpm"
#include "pixmaps/bat-4.xpm"
#endif

#include "bitmaps/bat-0.xbm"
#include "bitmaps/bat-1.xbm"
#include "bitmaps/bat-2.xbm"
#include "bitmaps/bat-3.xbm"
#include "bitmaps/bat-4.xbm"

#define MAX_STRENGTH 24
#define FRICTION 15
#define PENETRATION 0.4
#define SLIPAGE 4
#define TIME 32
#define MINBATS 1
#define MINSIZE 1
#define MINGRIDSIZE 3

#define ORIENTS 8
#define ORIENTCYCLE 32
#define CCW 1
#define CW (ORIENTS-1)
#define DIR(x)	(((x)>=0)?CCW:CW)
#define SIGN(x)	(((x)>=0)?1:-1)

static XImage bimages[] =
{
	{0, 0, 0, XYBitmap, 0, LSBFirst, 8, LSBFirst, 8, 1},
	{0, 0, 0, XYBitmap, 0, LSBFirst, 8, LSBFirst, 8, 1},
	{0, 0, 0, XYBitmap, 0, LSBFirst, 8, LSBFirst, 8, 1},
	{0, 0, 0, XYBitmap, 0, LSBFirst, 8, LSBFirst, 8, 1},
	{0, 0, 0, XYBitmap, 0, LSBFirst, 8, LSBFirst, 8, 1}
};

typedef struct {
	int         x, y, xlast, ylast;
	int         spincount, spindelay, spindir, orient;
	int         vx, vy, vang;
	int         graphics_format;
	unsigned long color;
} batstruct;

typedef struct {
	int         width, height;
	int         nbats;
	int         xs, ys;
	int         avgsize;
	int         restartnum;
	int         graphics_format;
	int         pixelmode;
	GC          backGC;
	Colormap    cmap;
	unsigned long black;
	batstruct  *bats;
	XImage     *images[ORIENTS / 2 + 1];
} bouncestruct;

static bouncestruct *bounces = NULL;

static void checkCollision(bouncestruct * bp, int a_bat);
static void drawabat(ModeInfo * mi, batstruct * bat);
static void movebat(bouncestruct * bp, batstruct * bat);
static void flapbat(batstruct * bat, int dir, int *vel, int avgsize);
static int  collide(bouncestruct * bp, int a_bat);

static unsigned char *bits[] =
{
	bat0_bits, bat1_bits, bat2_bits, bat3_bits, bat4_bits
};

#if defined( USE_XPM ) || defined( USE_XPMINC )
static char **pixs[] =
{
	bat0, bat1, bat2, bat3, bat4
};

#endif

static void
checkCollision(bouncestruct * bp, int a_bat)
{
	int         i, amount, spin, d, size;
	double      x, y;

	for (i = 0; i < bp->nbats; i++) {
		if (i != a_bat) {
			x = (double) (bp->bats[i].x - bp->bats[a_bat].x);
			y = (double) (bp->bats[i].y - bp->bats[a_bat].y);
			d = (int) sqrt(x * x + y * y);
			size = bp->avgsize;
			if (d > 0 && d < size) {
				amount = size - d;
				if (amount > PENETRATION * size)
					amount = (int) (PENETRATION * size);
				bp->bats[i].vx += (int) ((double) amount * x / d);
				bp->bats[i].vy += (int) ((double) amount * y / d);
				bp->bats[i].vx -= bp->bats[i].vx / FRICTION;
				bp->bats[i].vy -= bp->bats[i].vy / FRICTION;
				bp->bats[a_bat].vx -= (int) ((double) amount * x / d);
				bp->bats[a_bat].vy -= (int) ((double) amount * y / d);
				bp->bats[a_bat].vx -= bp->bats[a_bat].vx / FRICTION;
				bp->bats[a_bat].vy -= bp->bats[a_bat].vy / FRICTION;
				spin = (bp->bats[i].vang - bp->bats[a_bat].vang) /
					(2 * size * SLIPAGE);
				bp->bats[i].vang -= spin;
				bp->bats[a_bat].vang += spin;
				bp->bats[i].spindir = DIR(bp->bats[i].vang);
				bp->bats[a_bat].spindir = DIR(bp->bats[a_bat].vang);
				if (!bp->bats[i].vang) {
					bp->bats[i].spindelay = 1;
					bp->bats[i].spindir = 0;
				} else
					bp->bats[i].spindelay = (int) ((double) M_PI *
								       bp->avgsize / (ABS(bp->bats[i].vang))) + 1;
				if (!bp->bats[a_bat].vang) {
					bp->bats[a_bat].spindelay = 1;
					bp->bats[a_bat].spindir = 0;
				} else
					bp->bats[a_bat].spindelay = (int) ((double) M_PI *
									   bp->avgsize / (ABS(bp->bats[a_bat].vang))) + 1;
				return;
			}
		}
	}
}

static void
drawabat(ModeInfo * mi, batstruct * bat)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	bouncestruct *bp = &bounces[MI_SCREEN(mi)];

	if (bp->pixelmode) {
		if (bat->xlast != -1) {
			XSetForeground(display, bp->backGC, bp->black);
			XFillRectangle(display, window, bp->backGC,
				     bat->xlast, bat->ylast, bp->xs, bp->ys);
		}
		XSetForeground(display, bp->backGC, bat->color);
		XFillRectangle(display, window, bp->backGC,
			       bat->x, bat->y, bp->xs, bp->ys);
	} else {
		XSetForeground(display, bp->backGC, bat->color);
/* PURIFY 4.0.1 on SunOS4 and on Solaris 2 reports a 15985 byte memory leak on
   * the next line. */
		(void) XPutImage(display, window, bp->backGC,
				 bp->images[(bat->orient > ORIENTS / 2) ?
					ORIENTS - bat->orient : bat->orient],
				 0, 0, bat->x, bat->y, bp->xs, bp->ys);
		if (bat->xlast != -1) {
			XSetForeground(display, bp->backGC, bp->black);
			ERASE_IMAGE(display, window, bp->backGC,
				    bat->x, bat->y, bat->xlast, bat->ylast, bp->xs, bp->ys);
		}
	}
}

static void
movebat(bouncestruct * bp, batstruct * bat)
{
	bat->xlast = bat->x;
	bat->ylast = bat->y;
	bat->x += bat->vx;
	if (bat->x > (bp->width - bp->xs)) {
		/* Bounce off the right edge */
		bat->x = 2 * (bp->width - bp->xs) - bat->x;
		bat->vx = -bat->vx + bat->vx / FRICTION;
		flapbat(bat, 1, &bat->vy, bp->avgsize);
	} else if (bat->x < 0) {
		/* Bounce off the left edge */
		bat->x = -bat->x;
		bat->vx = -bat->vx + bat->vx / FRICTION;
		flapbat(bat, -1, &bat->vy, bp->avgsize);
	}
	bat->vy++;
	bat->y += bat->vy;
	if (bat->y >= (bp->height + bp->ys)) {	/* Don't see bat bounce */
		/* Bounce off the bottom edge */
		bat->y = (bp->height - bp->ys);
		bat->vy = -bat->vy + bat->vy / FRICTION;
		flapbat(bat, -1, &bat->vx, bp->avgsize);
	}			/* else if (bat->y < 0) { */
	/* Bounce off the top edge */
	/*bat->y = -bat->y;
	   bat->vy = -bat->vy + bat->vy / FRICTION;
	   flapbat(bat, 1, &bat->vx, bp->avgsize);
	   } */
	if (bat->spindir) {
		bat->spincount--;
		if (!bat->spincount) {
			bat->orient = (bat->spindir + bat->orient) % ORIENTS;
			bat->spincount = bat->spindelay;
		}
	}
}

static void
flapbat(batstruct * bat, int dir, int *vel, int avgsize)
{
	*vel -= (int) ((*vel + SIGN(*vel * dir) *
		 bat->spindelay * ORIENTCYCLE / (M_PI * avgsize)) / SLIPAGE);
	if (*vel) {
		bat->spindir = DIR(*vel * dir);
		bat->vang = *vel * ORIENTCYCLE;
		bat->spindelay = (int) ((double) M_PI * avgsize / (ABS(bat->vang))) + 1;
	} else
		bat->spindir = 0;
}

static int
collide(bouncestruct * bp, int a_bat)
{
	int         i, d, x, y;

	for (i = 0; i < a_bat; i++) {
		x = (bp->bats[i].x - bp->bats[a_bat].x);
		y = (bp->bats[i].y - bp->bats[a_bat].y);
		d = (int) sqrt((double) (x * x + y * y));
		if (d < bp->avgsize)
			return i;
	}
	return i;
}

static void
init_stuff(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	bouncestruct *bp = &bounces[MI_SCREEN(mi)];

	if (!bp->cmap && !bp->pixelmode) {
		int         i;

#if defined( USE_XPM ) || defined( USE_XPMINC )
		int         total = 0;
		XpmAttributes attrib;

#ifndef STANDALONE
		if (!fixedColors(mi)) {
			bp->cmap = XCreateColormap(display, window, MI_VISUAL(mi), AllocNone);
			attrib.colormap = bp->cmap;
			reserveColors(mi, bp->cmap, &bp->black);
		} else
#endif /* STANDALONE */
			attrib.colormap = MI_COLORMAP(mi);

		attrib.visual = MI_VISUAL(mi);
		attrib.depth = MI_DEPTH(mi);
		attrib.valuemask = XpmVisual | XpmColormap | XpmDepth;

		if (bp->graphics_format == IS_NONE
#ifndef USE_MONOXPM
		    && MI_NPIXELS(mi) > 2
#endif
			) {
			for (i = 0; i <= ORIENTS / 2; i++)
				if (XpmSuccess != XpmCreateImageFromData(display, pixs[i],
				&(bp->images[i]), (XImage **) NULL, &attrib))
					break;
			bp->graphics_format = IS_XPM;
			total = i;
			if (total <= ORIENTS / 2) {	/* All or nothing */
				bp->graphics_format = IS_XBM;
				if (MI_IS_VERBOSE(mi))
					(void) fprintf(stderr, "Full color images could not be loaded.\n");
				for (i = 0; i < total; i++) {
					(void) XDestroyImage(bp->images[i]);
					bp->images[i] = None;
				}
				bp->images[total] = None;
				if (bp->cmap != None) {
					XFreeColormap(display, bp->cmap);
					bp->cmap = None;
				}
			}
		}
		if (total <= ORIENTS / 2)
#endif

		{
			if (!bimages[0].data)	/* Only need to do this once */
				for (i = 0; i <= ORIENTS / 2; i++) {
					bimages[i].data = (char *) bits[i];
					bimages[i].width = bat0_width;
					bimages[i].height = bat0_height;
					bimages[i].bytes_per_line = (bat0_width + 7) / 8;
				}
			for (i = 0; i <= ORIENTS / 2; i++)
				bp->images[i] = &(bimages[i]);
		}
	}
	if (bp->cmap != None) {

#ifndef STANDALONE
		setColormap(display, window, bp->cmap, MI_IS_INWINDOW(mi));
#endif
		if (bp->backGC == None) {
			XGCValues   xgcv;

			xgcv.background = bp->black;
			bp->backGC = XCreateGC(display, window, GCBackground, &xgcv);
		}
	} else {
		bp->black = MI_BLACK_PIXEL(mi);
		bp->backGC = MI_GC(mi);
	}

}

#if defined( USE_XPM ) || defined( USE_XPMINC )
static void
free_stuff(Display * display, bouncestruct * bp)
{
	int         i;

	if (bp->graphics_format == IS_XPM) {
		for (i = 0; i <= ORIENTS / 2; i++) {
			if (bp->images[i]) {
				(void) XDestroyImage(bp->images[i]);
				bp->images[i] = None;
			}
		}
		bp->graphics_format = IS_NONE;
	}
	if (bp->cmap != None) {
		XFreeColormap(display, bp->cmap);
		if (bp->backGC != None) {
			XFreeGC(display, bp->backGC);
			bp->backGC = None;
		}
		bp->cmap = None;
	} else
		bp->backGC = None;
}
#endif

void
init_bat(ModeInfo * mi)
{
	bouncestruct *bp;
	int         size = MI_SIZE(mi);
	int         i, tryagain = 0;

	if (bounces == NULL) {
		if ((bounces = (bouncestruct *) calloc(MI_NUM_SCREENS(mi),
					     sizeof (bouncestruct))) == NULL)
			return;
	}
	bp = &bounces[MI_SCREEN(mi)];

#if defined( USE_XPM ) || defined( USE_XPMINC )
	free_stuff(MI_DISPLAY(mi), bp);
#endif

	bp->width = MI_WIDTH(mi);
	bp->height = MI_HEIGHT(mi);
	if (bp->width < 2)
		bp->width = 2;
	if (bp->height < 2)
		bp->height = 2;
	bp->restartnum = TIME;

	bp->nbats = MI_COUNT(mi);
	if (bp->nbats < -MINBATS) {
		/* if bp->nbats is random ... the size can change */
		if (bp->bats != NULL) {
			(void) free((void *) bp->bats);
			bp->bats = NULL;
		}
		bp->nbats = NRAND(-bp->nbats - MINBATS + 1) + MINBATS;
	} else if (bp->nbats < MINBATS)
		bp->nbats = MINBATS;
	if (!bp->bats)
		bp->bats = (batstruct *) malloc(bp->nbats * sizeof (batstruct));
	if (size == 0 ||
	    MINGRIDSIZE * size > bp->width / 2 || MINGRIDSIZE * size > bp->height) {
		if (bp->width > MINGRIDSIZE * bat0_width &&
		    bp->height > MINGRIDSIZE * bat0_height) {
			bp->pixelmode = False;
			bp->xs = bat0_width;
			bp->ys = bat0_height;
		} else {
			bp->pixelmode = True;
			bp->ys = MAX(MINSIZE, MIN(bp->width / 2, bp->height) / MINGRIDSIZE);
			bp->xs = 2 * bp->ys;
		}
	} else {
		bp->pixelmode = True;
		if (size < -MINSIZE)
			bp->ys = NRAND(MIN(-size, MAX(MINSIZE, MIN(bp->width / 2, bp->height) /
				      MINGRIDSIZE)) - MINSIZE + 1) + MINSIZE;
		else if (size < MINSIZE)
			bp->ys = MINSIZE;
		else
			bp->ys = MIN(size, MAX(MINSIZE, MIN(bp->width / 2, bp->height) /
					       MINGRIDSIZE));
		bp->xs = 2 * bp->ys;
	}
	bp->avgsize = (bp->xs + bp->ys) / 2;

	init_stuff(mi);

	i = 0;
	while (i < bp->nbats) {
		bp->bats[i].vx = ((LRAND() & 1) ? -1 : 1) * (NRAND(MAX_STRENGTH) + 1);
		bp->bats[i].x = (bp->bats[i].vx >= 0) ? 0 : bp->width - bp->xs;
		bp->bats[i].y = NRAND(bp->height / 2);
		if (i == collide(bp, i) || tryagain >= 8) {
			if (MI_NPIXELS(mi) > 2)
				bp->bats[i].color = MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));
			else
				bp->bats[i].color = MI_WHITE_PIXEL(mi);
			bp->bats[i].xlast = -1;
			bp->bats[i].ylast = 0;
			bp->bats[i].spincount = 1;
			bp->bats[i].spindelay = 1;
			bp->bats[i].vy = ((LRAND() & 1) ? -1 : 1) * NRAND(MAX_STRENGTH);
			bp->bats[i].spindir = 0;
			bp->bats[i].vang = 0;
			bp->bats[i].orient = NRAND(ORIENTS);
			i++;
		} else
			tryagain++;
	}
	MI_CLEARWINDOWCOLORMAP(mi, bp->backGC, bp->black);
}

void
draw_bat(ModeInfo * mi)
{
	bouncestruct *bp = &bounces[MI_SCREEN(mi)];
	int         i;

	MI_IS_DRAWN(mi) = True;

	for (i = 0; i < bp->nbats; i++) {
		drawabat(mi, &bp->bats[i]);
		movebat(bp, &bp->bats[i]);
	}
	for (i = 0; i < bp->nbats; i++)
		checkCollision(bp, i);
	if (!NRAND(TIME))	/* Put some randomness into the time */
		bp->restartnum--;
	if (!bp->restartnum)
		init_bat(mi);
}

void
release_bat(ModeInfo * mi)
{
	if (bounces != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			bouncestruct *bp = &bounces[screen];

#if defined( USE_XPM ) || defined( USE_XPMINC )
			free_stuff(MI_DISPLAY(mi), bp);
#endif

			if (bp->bats != NULL)
				(void) free((void *) bp->bats);
		}
		(void) free((void *) bounces);
		bounces = NULL;
	}
}

void
refresh_bat(ModeInfo * mi)
{
	bouncestruct *bp = &bounces[MI_SCREEN(mi)];

	MI_CLEARWINDOWCOLORMAP(mi, bp->backGC, bp->black);
}

#endif /* MODE_bat */
