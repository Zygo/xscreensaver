#ifndef lint
static char sccsid[] = "@(#)image.c	2.7 95/02/21 xlockmore";
#endif
/*-
 * image.c - image bouncer for xlock, the X Window System lockscreen.
 *
 * Copyright (c) 1991 by Patrick J. Naughton.
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * Changes of David Bagley <bagleyd@source.asset.com>
 * 07-Dec-94: Icons are now better centered if do not exactly fill an area.
 *
 * Changes of Patrick J. Naughton
 * 29-Jul-90: Written.
 */

#include "xlock.h"
#include "image.xbm"

static XImage logo = {
    0, 0,			/* width, height */
    0, XYBitmap, 0,		/* xoffset, format, data */
    LSBFirst, 8,		/* byte-order, bitmap-unit */
    LSBFirst, 8, 1		/* bitmap-bit-order, bitmap-pad, depth */
};

#define MAXICONS 64

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
    int         xoff;
    int         yoff;
    int         iconmode;
    int         iconcount;
    point       icons[MAXICONS];
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
			   ip->xb + image_width * ip->icons[i].x + ip->xoff,
			   ip->yb + image_height * ip->icons[i].y + ip->yoff,
			   image_width, image_height);

	ip->icons[i].x = RAND() % ip->ncols;
	ip->icons[i].y = RAND() % ip->nrows;
    }
    if (mono || Scr[screen].npixels <= 2)
	XSetForeground(dsp, Scr[screen].gc, WhitePixel(dsp, screen));
    for (i = 0; i < ip->iconcount; i++) {
	if (!mono && Scr[screen].npixels > 2)
	    XSetForeground(dsp, Scr[screen].gc,
			 Scr[screen].pixels[RAND() % Scr[screen].npixels]);

	XPutImage(dsp, win, Scr[screen].gc, &logo,
		  0, 0,
		  ip->xb + image_width * ip->icons[i].x + ip->xoff,
		  ip->yb + image_height * ip->icons[i].y + ip-> yoff,
		  image_width, image_height);
    }
}

void
initimage(win)
    Window      win;
{
    XWindowAttributes xgwa;
    imagestruct *ip = &ims[screen];

    logo.data = (char *) image_bits;
    logo.width = image_width;
    logo.height = image_height;
    logo.bytes_per_line = (image_width + 7) / 8;

    (void) XGetWindowAttributes(dsp, win, &xgwa);
    ip->width = xgwa.width;
    ip->height = xgwa.height;
    ip->ncols = ip->width / image_width;
    ip->nrows = ip->height / image_height;
    ip->xoff = (ip->width - ip->ncols * image_width) / 2;
    ip->yoff = (ip->height - ip->nrows * image_height) / 2;
    ip->iconmode = (ip->ncols < 2 || ip->nrows < 2);
    if (ip->iconmode) {
	ip->xb = 0;
	ip->yb = 0;
	ip->iconcount = 1;	/* icon mode */
    } else {
	ip->xb = (ip->width - image_width * ip->ncols) / 2;
	ip->yb = (ip->height - image_height * ip->nrows) / 2;
	ip->iconcount = batchcount;
	if (ip->iconcount > MAXICONS)
	  ip->iconcount = 16;
    }
    XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
    XFillRectangle(dsp, win, Scr[screen].gc, 0, 0, ip->width, ip->height);
}
