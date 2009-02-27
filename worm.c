#ifndef lint
static char sccsid[] = "%Z%%M% %I% %E% XLOCK";
#endif
/*-
 * worm.c - draw wiggly worms.
 *
 * Copyright (c) 1991 by Patrick J. Naughton.
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 27-Sep-91: got rid of all malloc calls since there were no calls to free().
 * 25-Sep-91: Integrated into X11R5 contrib xlock.
 *
 * Adapted from a concept in the Dec 87 issue of Scientific American.
 *
 * SunView version: Brad Taylor (brad@sun.com)
 * X11 version: Dave Lemke (lemke@ncd.com)
 * xlock version: Boris Putanec (bp@cs.brown.edu)
 *
 * This code is a static memory pig... like almost 200K... but as contributed
 * it leaked at a massive rate, so I made everything static up front... feel
 * free to contribute the proper memory management code.
 * 
 */

#include	"xlock.h"
#include	<math.h>

#define MAXCOLORS 64
#define MAXWORMS 64
#define CIRCSIZE 2
#define MAXWORMLEN 50

#define PI 3.14159265358979323844
#define SEGMENTS  36
static int  sintab[SEGMENTS];
static int  costab[SEGMENTS];
static int  init_table = 0;

typedef struct {
    int         xcirc[MAXWORMLEN];
    int         ycirc[MAXWORMLEN];
    int         dir;
    int         tail;
    int         x;
    int         y;
}           wormstuff;

typedef struct {
    int         xsize;
    int         ysize;
    int         wormlength;
    int         monopix;
    int         nc;
    int         nw;
    wormstuff   worm[MAXWORMS];
    XRectangle  rects[MAXCOLORS][MAXWORMS];
    int         size[MAXCOLORS];
}           wormstruct;

static wormstruct worms[MAXSCREENS];

#ifdef hpux
int
rint(x)
    double      x;
{
    if (x<0) return(-floor(fabs(x)+0.5));
        else return(floor(fabs(x)+0.5));
}
#endif

int
round(x)
    float       x;
{
    return ((int) rint((double) x));
}


void
worm_doit(win, wp, which, color)
    Window      win;
    wormstruct *wp;
    int         which;
    unsigned long color;
{
    wormstuff  *ws = &wp->worm[which];
    int         x, y;

    ws->tail++;
    if (ws->tail == wp->wormlength)
	ws->tail = 0;

    x = ws->xcirc[ws->tail];
    y = ws->ycirc[ws->tail];
    XClearArea(dsp, win, x, y, CIRCSIZE, CIRCSIZE, False);

    if (random() & 1) {
	ws->dir = (ws->dir + 1) % SEGMENTS;
    } else {
	ws->dir = (ws->dir + SEGMENTS - 1) % SEGMENTS;
    }

    x = (ws->x + costab[ws->dir] + wp->xsize) % wp->xsize;
    y = (ws->y + sintab[ws->dir] + wp->ysize) % wp->ysize;

    ws->xcirc[ws->tail] = x;
    ws->ycirc[ws->tail] = y;
    ws->x = x;
    ws->y = y;

    wp->rects[color][wp->size[color]].x = x;
    wp->rects[color][wp->size[color]].y = y;
    wp->size[color]++;
}


void
initworm(win)
    Window      win;
{
    int         i, j;
    wormstruct *wp = &worms[screen];
    XWindowAttributes xwa;

    wp->nc = Scr[screen].npixels;
    if (wp->nc > MAXCOLORS)
	wp->nc = MAXCOLORS;

    wp->nw = batchcount;
    if (wp->nw > MAXWORMS)
	wp->nw = MAXWORMS;

    if (!init_table) {
	init_table = 1;
	for (i = 0; i < SEGMENTS; i++) {
	    sintab[i] = round(CIRCSIZE * sin(i * 2 * PI / SEGMENTS));
	    costab[i] = round(CIRCSIZE * cos(i * 2 * PI / SEGMENTS));
	}
    }
    XGetWindowAttributes(dsp, win, &xwa);
    wp->xsize = xwa.width;
    wp->ysize = xwa.height;

    if (xwa.width < 100) {
	wp->monopix = BlackPixel(dsp, screen);
	wp->wormlength = MAXWORMLEN / 10;
    } else {
	wp->monopix = WhitePixel(dsp, screen);
	wp->wormlength = MAXWORMLEN;
    }

    for (i = 0; i < wp->nc; i++) {
	for (j = 0; j < wp->nw / wp->nc + 1; j++) {
	    wp->rects[i][j].width = CIRCSIZE;
	    wp->rects[i][j].height = CIRCSIZE;
	}
    }
    bzero(wp->size, wp->nc * sizeof(int));

    for (i = 0; i < wp->nw; i++) {
	for (j = 0; j < wp->wormlength; j++) {
	    wp->worm[i].xcirc[j] = wp->xsize / 2;
	    wp->worm[i].ycirc[j] = wp->ysize / 2;
	}
	wp->worm[i].dir = (unsigned) random() % SEGMENTS;
	wp->worm[i].tail = 0;
	wp->worm[i].x = wp->xsize / 2;
	wp->worm[i].y = wp->ysize / 2;
    }

    XClearWindow(dsp, win);
}


void
drawworm(win)
    Window      win;
{
    int         i;
    wormstruct *wp = &worms[screen];
    unsigned int wcolor;
    static unsigned int chromo = 0;

    bzero(wp->size, wp->nc * sizeof(int));

    for (i = 0; i < wp->nw; i++) {
	if (!mono && wp->nc > 2) {
	    wcolor = (i + chromo) % wp->nc;

	    worm_doit(win, wp, i, wcolor);
	} else
	    worm_doit(win, wp, i, 0);
    }

    if (!mono && wp->nc > 2) {
	for (i = 0; i < wp->nc; i++) {
	    XSetForeground(dsp, Scr[screen].gc, Scr[screen].pixels[i]);
	    XFillRectangles(dsp, win, Scr[screen].gc, wp->rects[i],
			    wp->size[i]);
	}
    } else {
	XSetForeground(dsp, Scr[screen].gc, wp->monopix);
	XFillRectangles(dsp, win, Scr[screen].gc, wp->rects[0],
			wp->size[0]);
    }

    if (++chromo == wp->nc)
	chromo = 0;
}
