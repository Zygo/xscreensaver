#ifndef lint
static char sccsid[] = "@(#)sphere.c 1.3 88/02/26 XLOCK";
#endif

/*-
 * sphere.c - Draw a bunch of shaded spheres for xlock,
 * the X Window System lockscreen.
 *
 * Copyright (c) 1988 by Sun Microsystems
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 2-Sep-93: xlock version (David Bagley bagleyd@source.asset.com)
 * 1988: Revised to use SunView canvas instead of gfxsw Sun Microsystems
 * 1982: Orignal Algorithm  Tom Duff  Lucasfilm Ltd.
 */

/* original copyright
 ******************************************************************************
 Copyright 1988 by Sun Microsystems, Inc. Mountain View, CA.

 All Rights Reserved

 Permission to use, copy, modify, and distribute this software and its
 documentation for any purpose and without fee is hereby granted,
 provided that the above copyright notice appear in all copies and that
 both that copyright notice and this permission notice appear in
 supporting documentation, and that the names of Sun or MIT not be
 used in advertising or publicity pertaining to distribution of the
 software without specific prior written permission. Sun and M.I.T.
 make no representations about the suitability of this software for
 any purpose. It is provided "as is" without any express or implied warranty.

 SUN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE. IN NO EVENT SHALL SUN BE LIABLE FOR ANY SPECIAL, INDIRECT
 OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
 OR PERFORMANCE OF THIS SOFTWARE.
 *****************************************************************************/

#include <math.h>
#include "xlock.h"
/*
 * (NX, NY, NZ) is the light source vector -- length should be
 * 100
 */
#define NX 48
#define NY (-36)
#define NZ 80
#define NR 100
#define MIN(a,b) (((a)<(b))?(a):(b))
#define SQRT(a) ((int)sqrt((double)(a)))

typedef struct {
    int      width, height;
    int      radius;
    int      x0;		/* x center */
    int      y0;		/* y center */
    int      color;
    int      x;
    int      maxy;
} spherestruct;

static spherestruct spheres[MAXSCREENS];

void
initsphere(win)
    Window        win;
{
    spherestruct *ss = &spheres[screen];
    XWindowAttributes xwa;

    XGetWindowAttributes(dsp, win, &xwa);
    ss->width = xwa.width;
    ss->height = xwa.height;

    XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
    XFillRectangle(dsp, win, Scr[screen].gc, 0, 0, ss->width, ss->height);

    ss->x = ss->radius = 0;
}

void
drawsphere(win)
    Window        win;
{
    spherestruct *ss = &spheres[screen];
    register      y;

    if (ss->x >= ss->radius) {
	ss->radius = random() % (MIN(ss->width / 2, ss->height / 2) - 1) + 1;
	ss->x0 = random() % ss->width;
	ss->y0 = random() % ss->height;
	ss->x = -ss->radius;

        if (Scr[screen].npixels > 2)
            ss->color = random() % Scr[screen].npixels;
    }
    ss->maxy = SQRT(ss->radius * ss->radius - ss->x * ss->x);
    XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
    XDrawLine(dsp, win, Scr[screen].gc,
        ss->x0 + ss->x, ss->y0 - ss->maxy,
        ss->x0 + ss->x, ss->y0 + ss->maxy);
    if (!mono && Scr[screen].npixels > 2)
        XSetForeground(dsp, Scr[screen].gc, Scr[screen].pixels[ss->color]);
    else
        XSetForeground(dsp, Scr[screen].gc, WhitePixel(dsp, screen));
    for (y = -ss->maxy; y <= ss->maxy; y++)
        if ((random() % (ss->radius * NR)) <=
                (NX * ss->x + NY * y + NZ *
		 SQRT(ss->radius * ss->radius - ss->x * ss->x - y * y)))
            XDrawPoint(dsp, win, Scr[screen].gc,
                ss->x + ss->x0, y + ss->y0);
    ss->x++;
}

