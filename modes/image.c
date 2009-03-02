/* -*- Mode: C; tab-width: 4 -*- */
/* image --- image bouncer */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)image.c	5.00 2000/11/01 xlockmore";

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
 * 10-May-1997: Compatible with xscreensaver
 * 03-Nov-1995: Patched to add an arbitrary xpm file.
 * 21-Sep-1995: Patch if xpm fails to load <Markus.Zellner@anu.edu.au>.
 * 17-Jun-1995: Pixmap stuff of Skip_Burrell@sterling.com added.
 * 07-Dec-1994: Icons are now better centered if do not exactly fill an area.
 * 29-Jul-1990: Written.
 */

#ifdef STANDALONE
#define PROGCLASS "Image"
#define HACK_INIT init_image
#define HACK_DRAW draw_image
#define image_opts xlockmore_opts
#define DEFAULTS "*delay: 2000000 \n" \
 "*count: -10 \n" \
 "*ncolors: 200 \n" \
 "*bitmap: \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#include "color.h"
#endif /* STANDALONE */
#include "iostuff.h"

#ifdef MODE_image

ModeSpecOpt image_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   image_description =
{"image", "init_image", "draw_image", "release_image",
 "refresh_image", "init_image", NULL, &image_opts,
 2000000, -10, 1, 1, 64, 1.0, "",
 "Shows randomly appearing logos", 0, NULL};

#endif

/* aliases for vars defined in the bitmap file */
#define IMAGE_WIDTH	image_width
#define IMAGE_HEIGHT	image_height
#define IMAGE_BITS	image_bits

#include "image.xbm"

#if defined( USE_XPM ) || defined( USE_XPMINC )
#define IMAGE_NAME	image_name
#include "image.xpm"
#define DEFAULT_XPM 1
#endif

#define MINICONS 1

typedef struct {
	int         x, y;
	int         color;
} imagetype;

typedef struct {
	int         width, height;
	int         nrows, ncols;
	XPoint      image_offset;
	int         iconcount;
	imagetype  *icons;
	int         graphics_format;
	GC          backGC;
	XImage     *logo;
	Colormap    cmap;
	unsigned long black;
} imagestruct;

static imagestruct *ims = NULL;

static void
free_stuff(Display * display, imagestruct * ip)
{
	if (ip->cmap != None) {
		XFreeColormap(display, ip->cmap);
		if (ip->backGC != None) {
			XFreeGC(display, ip->backGC);
			ip->backGC = None;
		}
		ip->cmap = None;
	} else
		ip->backGC = None;
}

static void
free_image(Display * display, imagestruct * ip)
{
	if (ip->icons != NULL) {
		(void) free((void *) ip->icons);
		ip->icons = NULL;
	}
	free_stuff(display, ip);
	if (ip->logo != None) {
		destroyImage(&ip->logo, &ip->graphics_format);
		ip->logo = None;
	}
}

static Bool
init_stuff(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	imagestruct *ip = &ims[MI_SCREEN(mi)];

	if (ip->logo == None) {
		getImage(mi, &ip->logo, IMAGE_WIDTH, IMAGE_HEIGHT, IMAGE_BITS,
#if defined( USE_XPM ) || defined( USE_XPMINC )
			 DEFAULT_XPM, IMAGE_NAME,
#endif
			 &ip->graphics_format, &ip->cmap, &ip->black);
		if (ip->logo == None) {
			free_image(display, ip);
			return False;
		}
	}
#ifndef STANDALONE
	if (ip->cmap != None) {
		setColormap(display, window, ip->cmap, MI_IS_INWINDOW(mi));
		if (ip->backGC == None) {
			XGCValues   xgcv;

			xgcv.background = ip->black;
			if ((ip->backGC = XCreateGC(display, window, GCBackground,
					&xgcv)) == None) {
				free_image(display, ip);
				return False;
			}
		}
	} else
#endif /* STANDALONE */
	{
		ip->black = MI_BLACK_PIXEL(mi);
		ip->backGC = MI_GC(mi);
	}
	return True;
}

static void
drawimages(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	imagestruct *ip = &ims[MI_SCREEN(mi)];
	int         i;


	MI_CLEARWINDOWCOLORMAPFAST(mi, ip->backGC, ip->black);
	if (MI_NPIXELS(mi) <= 2)
		XSetForeground(display, ip->backGC, MI_WHITE_PIXEL(mi));
	for (i = 0; i < ip->iconcount; i++) {
		if (ip->logo != NULL && ip->icons[i].x >= 0) {
			if (MI_NPIXELS(mi) > 2 && ip->graphics_format < IS_XPM)
				XSetForeground(display, ip->backGC, MI_PIXEL(mi, ip->icons[i].color));
			(void) XPutImage(display, window, ip->backGC, ip->logo, 0, 0,
			ip->logo->width * ip->icons[i].x + ip->image_offset.x,
					 ip->logo->height * ip->icons[i].y + ip->image_offset.y,
					 ip->logo->width, ip->logo->height);
		}
	}
}

void
init_image(ModeInfo * mi)
{
	imagestruct *ip;
	int         i;

	if (ims == NULL) {
		if ((ims = (imagestruct *) calloc(MI_NUM_SCREENS(mi),
					      sizeof (imagestruct))) == NULL)
			return;
	}
	ip = &ims[MI_SCREEN(mi)];

	if (!init_stuff(mi))
		return;

	ip->width = MI_WIDTH(mi);
	ip->height = MI_HEIGHT(mi);
	if (ip->width > ip->logo->width)
		ip->ncols = ip->width / ip->logo->width;
	else
		ip->ncols = 1;
	if (ip->height > ip->logo->height)
		ip->nrows = ip->height / ip->logo->height;
	else
		ip->nrows = 1;
	ip->image_offset.x = (ip->width - ip->ncols * ip->logo->width) / 2;
	ip->image_offset.y = (ip->height - ip->nrows * ip->logo->height) / 2;
	ip->iconcount = MI_COUNT(mi);
	if (ip->iconcount < -MINICONS)
		ip->iconcount = NRAND(-ip->iconcount - MINICONS + 1) + MINICONS;
	else if (ip->iconcount < MINICONS)
		ip->iconcount = MINICONS;
	if (ip->iconcount > ip->ncols * ip->nrows)
		ip->iconcount = ip->ncols * ip->nrows;
	if (ip->icons != NULL)
		(void) free((void *) ip->icons);
	if ((ip->icons = (imagetype *) malloc(ip->iconcount *
			sizeof (imagetype))) == NULL) {
		free_image(MI_DISPLAY(mi), ip);
		return;
	}
	for (i = 0; i < ip->iconcount; i++)
		ip->icons[i].x = -1;

	MI_CLEARWINDOWCOLORMAP(mi, ip->backGC, ip->black);
}

void
draw_image(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         i;
	imagestruct *ip;

	if (ims == NULL)
		return;
	ip = &ims[MI_SCREEN(mi)];
	if (ip->icons == NULL)
		return;
	
	MI_IS_DRAWN(mi) = True;
	XSetForeground(display, ip->backGC, ip->black);
	for (i = 0; i < ip->iconcount; i++) {
		if ((ip->ncols * ip->nrows > ip->iconcount) && ip->icons[i].x >= 0)
			XFillRectangle(display, window, ip->backGC,
			ip->logo->width * ip->icons[i].x + ip->image_offset.x,
				       ip->logo->height * ip->icons[i].y + ip->image_offset.y,
				       ip->logo->width, ip->logo->height);
		ip->icons[i].x = NRAND(ip->ncols);
		ip->icons[i].y = NRAND(ip->nrows);
		ip->icons[i].color = NRAND(MI_NPIXELS(mi));
	}
	drawimages(mi);
}

void
release_image(ModeInfo * mi)
{
	if (ims != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_image(MI_DISPLAY(mi), &ims[screen]);
		(void) free((void *) ims);
		ims = NULL;
	}
}

void
refresh_image(ModeInfo * mi)
{
#if defined( USE_XPM ) || defined( USE_XPMINC )
	imagestruct *ip;

	if (ims == NULL)
		return;
	ip = &ims[MI_SCREEN(mi)];
	if (ip->icons == NULL)
		return;

	if (ip->graphics_format >= IS_XPM) {
		/* This is needed when another program changes the colormap. */
		free_image(MI_DISPLAY(mi), ip);
		init_image(mi);
		return;
	}
#endif
	drawimages(mi);
}

#endif /* MODE_image */
