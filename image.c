#ifndef lint
static char sccsid[] = "@(#)image.c	1.7 91/05/24 XLOCK";
#endif
/*-
 * image.c - image bouncer for xlock, the X Window System lockscreen.
 *
 * Copyright (c) 1991 by Patrick J. Naughton.
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 29-Jul-90: Written.
 */

#include "xlock.h"
#include "sunlogo.bit"

static XImage logo = {
    0, 0,			/* width, height */
    0, XYBitmap, 0,		/* xoffset, format, data */
    LSBFirst, 8,		/* byte-order, bitmap-unit */
    LSBFirst, 8, 1		/* bitmap-bit-order, bitmap-pad, depth */
};

#define MAXICONS 256

typedef struct {
    int         x;
    int         y;
}           point;

typedef struct {
    int         width;
    int         height;
    int         nrows;
    int         ncols;
    int         xb;
    int         yb;
    int         iconmode;
    int         iconcount;
    point       icons[MAXICONS];
    long        startTime;
}           imagestruct;

static imagestruct ims[MAXSCREENS];

void
drawimage(win)
    Window      win;
{
    imagestruct *ip = &ims[screen];
    int         i;

    XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
    for (i = 0; i < ip->iconcount; i++) {
	if (!ip->iconmode)
	    XFillRectangle(dsp, win, Scr[screen].gc,
			   ip->xb + sunlogo_width * ip->icons[i].x,
			   ip->yb + sunlogo_height * ip->icons[i].y,
			   sunlogo_width, sunlogo_height);

	ip->icons[i].x = random() % ip->ncols;
	ip->icons[i].y = random() % ip->nrows;
    }
    if (Scr[screen].npixels == 2)
	XSetForeground(dsp, Scr[screen].gc, WhitePixel(dsp, screen));
    for (i = 0; i < ip->iconcount; i++) {
	if (Scr[screen].npixels > 2)
	    XSetForeground(dsp, Scr[screen].gc,
			 Scr[screen].pixels[random() % Scr[screen].npixels]);

	XPutImage(dsp, win, Scr[screen].gc, &logo,
		  0, 0,
		  ip->xb + sunlogo_width * ip->icons[i].x,
		  ip->yb + sunlogo_height * ip->icons[i].y,
		  sunlogo_width, sunlogo_height);
    }
}

void
initimage(win)
    Window      win;
{
    XWindowAttributes xgwa;
    imagestruct *ip = &ims[screen];

    ip->startTime = seconds();

    logo.data = (char *) sunlogo_bits;
    logo.width = sunlogo_width;
    logo.height = sunlogo_height;
    logo.bytes_per_line = (sunlogo_width + 7) / 8;

    XGetWindowAttributes(dsp, win, &xgwa);
    ip->width = xgwa.width;
    ip->height = xgwa.height;
    ip->ncols = ip->width / sunlogo_width;
    ip->nrows = ip->height / sunlogo_height;
    ip->iconmode = (ip->ncols < 2 || ip->nrows < 2);
    if (ip->iconmode) {
	ip->xb = 0;
	ip->yb = 0;
	ip->iconcount = 1;	/* icon mode */
    } else {
	ip->xb = (ip->width - sunlogo_width * ip->ncols) / 2;
	ip->yb = (ip->height - sunlogo_height * ip->nrows) / 2;
	ip->iconcount = batchcount;
    }
    XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
    XFillRectangle(dsp, win, Scr[screen].gc, 0, 0, ip->width, ip->height);
}
