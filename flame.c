#ifndef lint
static char sccsid[] = "@(#)flame.c 1.4 91/09/27 XLOCK";
#endif
/*-
 * flame.c - recursive fractal cosmic flames.
 *
 * Copyright (c) 1991 by Patrick J. Naughton.
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 27-Jun-91: vary number of functions used.
 * 24-Jun-91: fixed portability problem with integer mod (%).
 * 06-Jun-91: Written. (received from Scott Graves, spot@cs.cmu.edu).
 */

#include "xlock.h"
#include <math.h>

#define MAXTOTAL	10000
#define MAXBATCH	10
#define MAXLEV		4

typedef struct {
    double      f[2][3][MAXLEV];/* three non-homogeneous transforms */
    int         max_levels;
    int         cur_level;
    int         snum;
    int         anum;
    int         width, height;
    int         num_points;
    int         total_points;
    int         pixcol;
    Window      win;
    XPoint      pts[MAXBATCH];
}           flamestruct;

static flamestruct flames[MAXSCREENS];

static short
halfrandom(mv)
    int         mv;
{
    static short lasthalf = 0;
    unsigned long r;

    if (lasthalf) {
	r = lasthalf;
	lasthalf = 0;
    } else {
	r = random();
	lasthalf = r >> 16;
    }
    return r % mv;
}

void
initflame(win)
    Window      win;
{
    flamestruct *fs = &flames[screen];
    XWindowAttributes xwa;

    srandom(time((long *) 0));

    XGetWindowAttributes(dsp, win, &xwa);
    fs->width = xwa.width;
    fs->height = xwa.height;

    fs->max_levels = batchcount;
    fs->win = win;

    XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
    XFillRectangle(dsp, win, Scr[screen].gc, 0, 0, fs->width, fs->height);

    if (Scr[screen].npixels > 2) {
	fs->pixcol = halfrandom(Scr[screen].npixels);
	XSetForeground(dsp, Scr[screen].gc, Scr[screen].pixels[fs->pixcol]);
    } else {
	XSetForeground(dsp, Scr[screen].gc, WhitePixel(dsp, screen));
    }
}

static      Bool
recurse(fs, x, y, l)
    flamestruct *fs;
    register double x, y;
    register int l;
{
    int         xp, yp, i;
    double      nx, ny;

    if (l == fs->max_levels) {
	fs->total_points++;
	if (fs->total_points > MAXTOTAL)	/* how long each fractal runs */
	    return False;

	if (x > -1.0 && x < 1.0 && y > -1.0 && y < 1.0) {
	    xp = fs->pts[fs->num_points].x = (int) ((fs->width / 2)
						    * (x + 1.0));
	    yp = fs->pts[fs->num_points].y = (int) ((fs->height / 2)
						    * (y + 1.0));
	    fs->num_points++;
	    if (fs->num_points > MAXBATCH) {	/* point buffer size */
		XDrawPoints(dsp, fs->win, Scr[screen].gc, fs->pts,
			    fs->num_points, CoordModeOrigin);
		fs->num_points = 0;
	    }
	}
    } else {
	for (i = 0; i < fs->snum; i++) {
	    nx = fs->f[0][0][i] * x + fs->f[0][1][i] * y + fs->f[0][2][i];
	    ny = fs->f[1][0][i] * x + fs->f[1][1][i] * y + fs->f[1][2][i];
	    if (i < fs->anum) {
		nx = sin(nx);
		ny = sin(ny);
	    }
	    if (!recurse(fs, nx, ny, l + 1))
		return False;
	}
    }
    return True;
}


void
drawflame(win)
    Window      win;
{
    flamestruct *fs = &flames[screen];

    int         i, j, k;
    static      alt = 0;

    if (!(fs->cur_level++ % fs->max_levels)) {
	XClearWindow(dsp, fs->win);
	alt = !alt;
    } else {
	if (Scr[screen].npixels > 2) {
	    XSetForeground(dsp, Scr[screen].gc,
			   Scr[screen].pixels[fs->pixcol]);
	    if (--fs->pixcol < 0)
		fs->pixcol = Scr[screen].npixels - 1;
	}
    }

    /* number of functions */
    fs->snum = 2 + (fs->cur_level % (MAXLEV - 1));

    /* how many of them are of alternate form */
    if (alt)
	fs->anum = 0;
    else
	fs->anum = halfrandom(fs->snum) + 2;

    /* 6 coefs per function */
    for (k = 0; k < fs->snum; k++) {
	for (i = 0; i < 2; i++)
	    for (j = 0; j < 3; j++)
		fs->f[i][j][k] = ((double) (random() & 1023) / 512.0 - 1.0);
    }
    fs->num_points = 0;
    fs->total_points = 0;
    (void) recurse(fs, 0.0, 0.0, 0);
    XDrawPoints(dsp, win, Scr[screen].gc,
		fs->pts, fs->num_points, CoordModeOrigin);
}
