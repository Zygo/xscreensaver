#ifndef lint
static char sccsid[] = "@(#)spline.c 1.3 92/01/01 XLOCK";
#endif
/*-
 * spline.c - spline fun #3 
 *
 * Copyright (c) 1992 by Jef Poskanzer
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 2-Sep-93: xlock version: (David Bagley bagleyd@source.asset.com)
 * 1992:     X11 version (Jef Poskanzer jef@netcom.com || jef@well.sf.ca.us)
 * 
 */

/* original copyright
** xsplinefun.c - X11 version of spline fun #3
**
** Displays colorful moving splines in the X11 root window.
**
** Copyright (C) 1992 by Jef Poskanzer
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#include	"xlock.h"

#define MAXPOINTS 99
#define MAX_DELTA 3
#define MAX_SPLINES 2048

#define SPLINE_THRESH 5

typedef struct {
    int		width;
    int		height;
    int		color;
    int		points;
    int         generation;
    int x[MAXPOINTS], y[MAXPOINTS], dx[MAXPOINTS], dy[MAXPOINTS];
} splinestruct;

static splinestruct splines[MAXSCREENS];

static void move_splines();
static void XDrawSpline();

void
initspline(win)
    Window      win;
{
    int i;
    XWindowAttributes xgwa;
    splinestruct *sp = &splines[screen];

    XGetWindowAttributes(dsp, win, &xgwa);
    sp->width = xgwa.width;
    sp->height = xgwa.height;
    /* batchcount is the upper bound on the number of points */
    if (batchcount > MAXPOINTS || batchcount <= 3)
      batchcount = 5;
    /*sp->points = batchcount;*/
    sp->points = random() % (batchcount - 3) + 3;
    sp->generation = 0;

    XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
    XFillRectangle(dsp, win, Scr[screen].gc, 0, 0, sp->width, sp->height);

    /* Initialize points. */
    for ( i = 0; i < sp->points; ++i )
        {
        sp->x[i] = random() % sp->width;
        sp->y[i] = random() % sp->height;
        sp->dx[i] = random() % ( MAX_DELTA * 2 ) - MAX_DELTA;
        if ( sp->dx[i] <= 0 ) --sp->dx[i];
        sp->dy[i] = random() % ( MAX_DELTA * 2 ) - MAX_DELTA;
        if ( sp->dy[i] <= 0 ) --sp->dy[i];
        }
    
    sp->color = 0;
}


void
drawspline(win)
    Window      win;
{
    int i, t, px, py, zx, zy, nx, ny;
    splinestruct *sp = &splines[screen];

    /* Move the points. */
    for ( i = 0; i < sp->points; i++ )
        {
        for ( ; ; )
            {
            t = sp->x[i] + sp->dx[i];
            if ( t >= 0 && t < sp->width ) break;
            sp->dx[i] = random() % ( MAX_DELTA * 2 ) - MAX_DELTA;
            if ( sp->dx[i] <= 0 ) --sp->dx[i];
            }
        sp->x[i] = t;
        for ( ; ; )
            {
            t = sp->y[i] + sp->dy[i];
            if ( t >= 0 && t < sp->height ) break;
            sp->dy[i] = random() % ( MAX_DELTA * 2 ) - MAX_DELTA;
            if ( sp->dy[i] <= 0 ) --sp->dy[i];
            }
        sp->y[i] = t;
        }

    /* Draw the figure. */
    px = zx = ( sp->x[0] + sp->x[sp->points-1] ) / 2;
    py = zy = ( sp->y[0] + sp->y[sp->points-1] ) / 2;
    for ( i = 0; i < sp->points-1; ++i )
        {
        nx = ( sp->x[i+1] + sp->x[i] ) / 2;
        ny = ( sp->y[i+1] + sp->y[i] ) / 2;
        XDrawSpline( dsp, win, Scr[screen].gc,
	  px, py, sp->x[i], sp->y[i], nx, ny );
        px = nx;
        py = ny;
        }
       if (!mono && Scr[screen].npixels > 2) {
         XSetForeground(dsp, Scr[screen].gc, Scr[screen].pixels[sp->color]);
         if (++sp->color >= Scr[screen].npixels)
	     sp->color = 0;
       } else
         XSetForeground(dsp, Scr[screen].gc, WhitePixel(dsp, screen));

    XDrawSpline( dsp, win, Scr[screen].gc,
      px, py, sp->x[sp->points-1], sp->y[sp->points-1], zx, zy );

    if (++sp->generation > MAX_SPLINES)
      initspline(win);
}

/* X spline routine. */

#define abs(x) ((x) < 0 ? -(x) : (x))

static void
XDrawSpline(display, d, gc, x0, y0, x1, y1, x2, y2)
Display* display;
Drawable d;
GC gc;
int x0, y0, x1, y1, x2, y2;
    {
    register int xa, ya, xb, yb, xc, yc, xp, yp;

    xa = (x0 + x1) / 2;
    ya = (y0 + y1) / 2;
    xc = (x1 + x2) / 2;
    yc = (y1 + y2) / 2;
    xb = (xa + xc) / 2;
    yb = (ya + yc) / 2;

    xp = (x0 + xb) / 2;
    yp = (y0 + yb) / 2;
    if (abs(xa - xp) + abs(ya - yp) > SPLINE_THRESH)
        XDrawSpline(display, d, gc, x0, y0, xa, ya, xb, yb);
    else
        XDrawLine(display, d, gc, x0, y0, xb, yb);

    xp = (x2 + xb) / 2;
    yp = (y2 + yb) / 2;
    if (abs(xc - xp) + abs(yc - yp) > SPLINE_THRESH)
        XDrawSpline(display, d, gc, xb, yb, xc, yc, x2, y2);
    else
        XDrawLine(display, d, gc, xb, yb, x2, y2);
    }
