
/* -*- Mode: C; tab-width: 4 -*- */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)image.c	4.03 97/05/10 xlockmore";

#endif

/*-
 * image.c - image bouncer for xlock, the X Window System lockscreen.
 *
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
 * 10-May-97: Compatible with xscreensaver
 * 03-Nov-95: Patched to add an arbitrary xpm file. 
 * 21-Sep-95: Patch if xpm fails to load <Markus.Zellner@anu.edu.au>.
 * 17-Jun-95: Pixmap stuff of Skip_Burrell@sterling.com added.
 * 07-Dec-94: Icons are now better centered if do not exactly fill an area.
 * 29-Jul-90: Written.
 */

#ifdef STANDALONE
#define PROGCLASS            "Image"
#define HACK_INIT            init_image
#define HACK_DRAW            draw_image
#define DEF_DELAY            2000000
#define DEF_BATCHCOUNT       -10
#define DEF_NCOLORS          200
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
ModeSpecOpt image_opts =
{0, NULL, 0, NULL, NULL};

#endif /* STANDALONE */

/* aliases for vars defined in the bitmap file */
#define IMAGE_WIDTH	image_width
#define IMAGE_HEIGHT	image_height
#define IMAGE_BITS	image_bits

#include "image.xbm"

#if defined( USE_XPM ) || defined( USE_XPMINC )
#define IMAGE_NAME	image_name
#include "image.xpm"
#endif

#define MINICONS 1

extern void getImage(ModeInfo * mi, XImage ** logo,
		     int width, int height, unsigned char *bits,
#if defined( USE_XPM ) || defined( USE_XPMINC )
		     char **name,
#endif
		     int *graphics_format, Colormap * newcolormap,
		     unsigned long *blackpix, unsigned long *whitepix);
extern void destroyImage(XImage ** logo, int *graphics_format);

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
	Colormap    cm;
	unsigned long black, white;
} imagestruct;

static imagestruct *ims = NULL;

void
init_image(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	imagestruct *ip;
	int         i;

	if (ims == NULL) {
		if ((ims = (imagestruct *) calloc(MI_NUM_SCREENS(mi),
					      sizeof (imagestruct))) == NULL)
			return;
	}
	ip = &ims[MI_SCREEN(mi)];

	if (!ip->logo)
		getImage(mi, &ip->logo, IMAGE_WIDTH, IMAGE_HEIGHT, IMAGE_BITS,
#if defined( USE_XPM ) || defined( USE_XPMINC )
			 IMAGE_NAME,
#endif
			 &ip->graphics_format, &ip->cm,
			 &ip->black, &ip->white);
	if (ip->cm != None) {
		if (ip->backGC == None) {
			XGCValues   xgcv;

			setColormap(display, window, ip->cm, MI_WIN_IS_INWINDOW(mi));
			xgcv.background = ip->black;
			ip->backGC = XCreateGC(display, window, GCBackground, &xgcv);
		}
	} else {
		ip->black = MI_WIN_BLACK_PIXEL(mi);
		ip->backGC = MI_GC(mi);
	}
	ip->width = MI_WIN_WIDTH(mi);
	ip->height = MI_WIN_HEIGHT(mi);
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
	ip->iconcount = MI_BATCHCOUNT(mi);
	if (ip->iconcount < -MINICONS)
		ip->iconcount = NRAND(-ip->iconcount - MINICONS + 1) + MINICONS;
	else if (ip->iconcount < MINICONS)
		ip->iconcount = MINICONS;
	if (ip->iconcount > ip->ncols * ip->nrows)
		ip->iconcount = ip->ncols * ip->nrows;
	if (ip->icons != NULL)
		(void) free((void *) ip->icons);
	ip->icons = (imagetype *) malloc(ip->iconcount * sizeof (imagetype));
	for (i = 0; i < ip->iconcount; i++)
		ip->icons[i].x = -1;
	XSetForeground(display, ip->backGC, ip->black);
	XFillRectangle(display, window, ip->backGC,
		       0, 0, ip->width, ip->height);
}

void
draw_image(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	imagestruct *ip = &ims[MI_SCREEN(mi)];
	int         i;

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
	refresh_image(mi);
}

void
release_image(ModeInfo * mi)
{
	if (ims != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			imagestruct *ip = &ims[screen];

			if (ip->icons != NULL)
				(void) free((void *) ip->icons);
			if (ip->cm != None) {
				if (ip->backGC != None)
					XFreeGC(MI_DISPLAY(mi), ip->backGC);
				XFreeColormap(MI_DISPLAY(mi), ip->cm);
			}
			destroyImage(&ip->logo, &ip->graphics_format);
		}
		(void) free((void *) ims);
		ims = NULL;
	}
}

void
refresh_image(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	imagestruct *ip = &ims[MI_SCREEN(mi)];
	int         i;

	if (MI_NPIXELS(mi) <= 2)
		XSetForeground(display, ip->backGC, MI_WIN_WHITE_PIXEL(mi));
	for (i = 0; i < ip->iconcount; i++) {
		if (ip->logo != NULL && ip->icons[i].x >= 0) {
			if (MI_NPIXELS(mi) > 2 && ip->graphics_format < IS_XPM)
				XSetForeground(display, ip->backGC, MI_PIXEL(mi, ip->icons[i].color));
			XPutImage(display, window, ip->backGC, ip->logo, 0, 0,
			ip->logo->width * ip->icons[i].x + ip->image_offset.x,
				  ip->logo->height * ip->icons[i].y + ip->image_offset.y,
				  ip->logo->width, ip->logo->height);
		}
	}
}
