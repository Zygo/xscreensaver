#ifndef lint
static char sccsid[] = "@(#)life.c	23.6 91/05/24 XLOCK";
#endif
/*-
 * life.c - Conway's game of Life for xlock, the X Window System lockscreen.
 *
 * Copyright (c) 1991 by Patrick J. Naughton.
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 24-May-91: Added wraparound code from johnson@bugs.comm.mot.com.
 *	      Made old cells stay blue.
 *	      Made batchcount control the number of generations till restart.
 * 29-Jul-90: support for multiple screens.
 * 07-Feb-90: remove bogus semi-colon after #include line.
 * 15-Dec-89: Fix for proper skipping of {White,Black}Pixel() in colors.
 * 08-Oct-89: Moved seconds() to an extern.
 * 20-Sep-89: Written (life algorithm courtesy of Jim Graham, flar@sun.com).
 */

#include "xlock.h"
#include "lifeicon.bit"

static XImage logo = {
    0, 0,			/* width, height */
    0, XYBitmap, 0,		/* xoffset, format, data */
    LSBFirst, 8,		/* byte-order, bitmap-unit */
    LSBFirst, 8, 1		/* bitmap-bit-order, bitmap-pad, depth */
};
#define min(a, b) ((a)<(b)?(a):(b))
#define	MAXROWS	155
#define MAXCOLS	144
#define TIMEOUT 30

typedef struct {
    int         pixelmode;
    int         xs;
    int         ys;
    int         xb;
    int         yb;
    int         generation;
    long        shooterTime;
    int         nrows;
    int         ncols;
    int         width;
    int         height;
    unsigned char buffer[(MAXROWS + 2) * (MAXCOLS + 2) + 2];
    unsigned char tempbuf[MAXCOLS * 2];
    unsigned char lastbuf[MAXCOLS];
    unsigned char agebuf[(MAXROWS + 2) * (MAXCOLS + 2)];
}           lifestruct;

static lifestruct lifes[MAXSCREENS];
static int  icon_width, icon_height;

/* Buffer stores the data for each cell. Each cell is stored as
 * 8 bits representing the presence of a critter in each of it's
 * surrounding 8 cells. There is an empty row and column around
 * the whole array to allow stores without bounds checking as well
 * as an extra row at the end for the fetches into tempbuf.
 *
 * Tempbuf stores the data for the next two rows so that we know
 * the state of those critter before he was modified by the fate
 * of the critters that have already been processed.
 *
 * Agebuf stores the age of each critter.
 */

#define	UPLT	0x01
#define UP	0x02
#define UPRT	0x04
#define LT	0x08
#define RT	0x10
#define DNLT	0x20
#define DN	0x40
#define DNRT	0x80

/* Fates is a lookup table for the fate of a critter. The 256
 * entries represent the 256 possible combinations of the 8
 * neighbor cells. Each entry is one of BIRTH (create a cell
 * or leave one alive), SAME (leave the cell alive or dead),
 * or DEATH (kill anything in the cell).
 */
#define BIRTH	0
#define SAME	1
#define DEATH	2
static unsigned char fates[256];
static int  initialized = 0;

static int  patterns[][128] = {
    {				/* EIGHT */
	-3, -3, -2, -3, -1, -3,
	-3, -2, -2, -2, -1, -2,
	-3, -1, -2, -1, -1, -1,
	0, 0, 1, 0, 2, 0,
	0, 1, 1, 1, 2, 1,
	0, 2, 1, 2, 2, 2,
	99
    },
    {				/* PULSAR */
	1, 1, 2, 1, 3, 1, 4, 1, 5, 1,
	1, 2, 5, 2,
	99
    },
    {				/* BARBER */
	-7, -7, -6, -7,
	-7, -6, -5, -6,
	-5, -4, -3, -4,
	-3, -2, -1, -2,
	-1, 0, 1, 0,
	1, 2, 3, 2,
	3, 4, 5, 4,
	4, 5, 5, 5,
	99
    },
    {				/* HERTZ */
	-2, -6, -1, -6,
	-2, -5, -1, -5,
	-7, -3, -6, -3, -2, -3, -1, -3, 0, -3, 1, -3, 5, -3, 6, -3,
	-7, -2, -5, -2, -3, -2, 2, -2, 4, -2, 6, -2,
	-5, -1, -3, -1, -2, -1, 2, -1, 4, -1,
	-7, 0, -5, 0, -3, 0, 2, 0, 4, 0, 6, 0,
	-7, 1, -6, 1, -2, 1, -1, 1, 0, 1, 1, 1, 5, 1, 6, 1,
	-2, 3, -1, 3,
	-2, 4, -1, 4,
	99
    },
    {				/* TUMBLER */
	-6, -6, -5, -6, 6, -6, 7, -6,
	-6, -5, -5, -5, 6, -5, 7, -5,
	-5, 5, 6, 5,
	-7, 6, -5, 6, 6, 6, 8, 6,
	-7, 7, -5, 7, 6, 7, 8, 7,
	-7, 8, -6, 8, 7, 8, 8, 8,
	99
    },
    {				/* PERIOD4 */
	-5, -8, -4, -8,
	-7, -7, -5, -7,
	-8, -6, -2, -6,
	-7, -5, -3, -5, -2, -5,
	-5, -3, -3, -3,
	-4, -2,
	99
    },
    {				/* PERIOD5 */
	-5, -8, -4, -8,
	-6, -7, -3, -7,
	-7, -6, -2, -6,
	-8, -5, -1, -5,
	-8, -4, -1, -4,
	-7, -3, -2, -3,
	-6, -2, -3, -2,
	-5, -1, -4, -1,
	99
    },
    {				/* PERIOD6 */
	-4, -8, -3, -8,
	-8, -7, -7, -7, -5, -7,
	-8, -6, -7, -6, -4, -6, -1, -6,
	-3, -5, -1, -5,
	-2, -4,
	-3, -2, -2, -2,
	-3, -1, -2, -1,
	99
    },
    {				/* PINWHEEL */
	-4, -8, -3, -8,
	-4, -7, -3, -7,
	-4, -5, -3, -5, -2, -5, -1, -5,
	-5, -4, -3, -4, 0, -4, 2, -4, 3, -4,
	-5, -3, -1, -3, 0, -3, 2, -3, 3, -3,
	-8, -2, -7, -2, -5, -2, -2, -2, 0, -2,
	-8, -1, -7, -1, -5, -1, 0, -1,
	-4, 0, -3, 0, -2, 0, -1, 0,
	-2, 2, -1, 2,
	-2, 3, -1, 3,
	99
    },
    {				/* ] */
	-1, -1, 0, -1, 1, -1,
	0, 0, 1, 0,
	-1, 1, 0, 1, 1, 1,
	99
    },
    {				/* cc: */
	-3, -1, -2, -1, -1, -1, 1, -1, 2, -1, 3, -1,
	-3, 0, -2, 0, 1, 0, 2, 0,
	-3, 1, -2, 1, -1, 1, 1, 1, 2, 1, 3, 1,
	99
    },
    {				/* DOLBY */
	-3, -1, -2, -1, -1, -1, 1, -1, 2, -1, 3, -1,
	-3, 0, -2, 0, 2, 0, 3, 0,
	-3, 1, -2, 1, -1, 1, 1, 1, 2, 1, 3, 1,
	99
    },
    {				/* HORIZON */
	-15, 0, -14, 0, -13, 0, -12, 0, -11, 0,
	-10, 0, -9, 0, -8, 0, -7, 0, -6, 0,
	-5, 0, -4, 0, -3, 0, -2, 0, -1, 0,
	4, 0, 3, 0, 2, 0, 1, 0, 0, 0,
	9, 0, 8, 0, 7, 0, 6, 0, 5, 0,
	14, 0, 13, 0, 12, 0, 11, 0, 10, 0,
	99
    },
    {				/* SHEAR */
	-7, -2, -6, -2, -5, -2, -4, -2, -3, -2,
	-2, -2, -1, -2, 0, -2, 1, -2, 2, -2,
	-5, -1, -4, -1, -3, -1, -2, -1, -1, -1,
	0, -1, 1, -1, 2, -1, 3, -1, 4, -1,
	-3, 0, -2, 0, -1, 0, 0, 0, 1, 0,
	2, 0, 3, 0, 4, 0, 5, 0, 6, 0,
	-10, 1, -9, 1, -8, 1, -7, 1, -6, 1,
	-5, 1, -4, 1, -3, 1, -2, 1, -1, 1,
	-10, 2, -9, 2, -8, 2, -7, 2, -6, 2,
	-5, 2, -4, 2, -3, 2, -2, 2, -1, 2,
	99
    },
    {				/* VERTIGO */
	0, -7,
	0, -6,
	0, -5,
	0, -4,
	0, -3,
	0, -2,
	0, -1,
	0, 0,
	0, 7,
	0, 6,
	0, 5,
	0, 4,
	0, 3,
	0, 2,
	0, 1,
	99
    },
    {				/* CROSSBAR */
	-5, 0, -4, 0, -3, 0, -2, 0, -1, 0, 4, 0, 3, 0, 2, 0, 1, 0, 0, 0,
	99
    },
    {				/* GOALPOSTS */
	-8, -7, 8, -7,
	-8, -6, 8, -6,
	-8, -5, 8, -5,
	-8, -4, 8, -4,
	-8, -3, 8, -3,
	-8, -2, 8, -2,
	-8, -1, 8, -1,
	-8, 0, 8, 0,
	-8, 1, 8, 1,
	-8, 2, 8, 2,
	-8, 3, 8, 3,
	-8, 4, 8, 4,
	-8, 5, 8, 5,
	-8, 6, 8, 6,
	-8, 7, 8, 7,
	99
    },
    {				/* \ */
	-8, -8, -7, -8,
	-7, -7, -6, -7,
	-6, -6, -5, -6,
	-5, -5, -4, -5,
	-4, -4, -3, -4,
	-3, -3, -2, -3,
	-2, -2, -1, -2,
	-1, -1, 0, -1,
	0, 0, 1, 0,
	1, 1, 2, 1,
	2, 2, 3, 2,
	3, 3, 4, 3,
	4, 4, 5, 4,
	5, 5, 6, 5,
	6, 6, 7, 6,
	7, 7, 8, 7,
	99
    },
    {				/* LABYRINTH */
	-4, -4, -3, -4, -2, -4, -1, -4, 0, -4, 1, -4, 2, -4, 3, -4, 4, -4,
	-4, -3, 0, -3, 4, -3,
	-4, -2, -2, -2, -1, -2, 0, -2, 1, -2, 2, -2, 4, -2,
	-4, -1, -2, -1, 2, -1, 4, -1,
	-4, 0, -2, 0, -1, 0, 0, 0, 1, 0, 2, 0, 4, 0,
	-4, 1, -2, 1, 2, 1, 4, 1,
	-4, 2, -2, 2, -1, 2, 0, 2, 1, 2, 2, 2, 4, 2,
	-4, 3, 0, 3, 4, 3,
	-4, 4, -3, 4, -2, 4, -1, 4, 0, 4, 1, 4, 2, 4, 3, 4, 4, 4,
	99
    }
};

#define NPATS	(sizeof patterns / sizeof patterns[0])


static void
drawcell(win, row, col)
    Window      win;
    int         row, col;
{
    lifestruct *lp = &lifes[screen];

    XSetForeground(dsp, Scr[screen].gc, WhitePixel(dsp, screen));
    if (!mono && Scr[screen].npixels > 2) {
	unsigned char *loc = lp->buffer + ((row + 1) * (lp->ncols + 2)) + col + 1;
	unsigned char *ageptr = lp->agebuf + (loc - lp->buffer);
	unsigned char age = *ageptr;

	/* if we aren't up to blue yet, then keep aging the cell. */
	if (age < Scr[screen].npixels * 0.7)
	    ++age;

	XSetForeground(dsp, Scr[screen].gc, Scr[screen].pixels[age]);
	*ageptr = age;
    }
    if (lp->pixelmode)
	XFillRectangle(dsp, win, Scr[screen].gc,
	       lp->xb + lp->xs * col, lp->yb + lp->ys * row, lp->xs, lp->ys);
    else
	XPutImage(dsp, win, Scr[screen].gc, &logo,
		  0, 0, lp->xb + lp->xs * col, lp->yb + lp->ys * row,
		  icon_width, icon_height);
}


static void
erasecell(win, row, col)
    Window      win;
    int         row, col;
{
    lifestruct *lp = &lifes[screen];
    XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
    XFillRectangle(dsp, win, Scr[screen].gc,
	       lp->xb + lp->xs * col, lp->yb + lp->ys * row, lp->xs, lp->ys);
}


static void
spawn(loc)
    unsigned char *loc;
{
    lifestruct *lp = &lifes[screen];
    unsigned char *ulloc, *ucloc, *urloc, *clloc, *crloc, *llloc, *lcloc, *lrloc,
               *arloc;
    int         off, row, col, lastrow;

    lastrow = (lp->nrows) * (lp->ncols + 2);
    off = loc - lp->buffer;
    col = off % (lp->ncols + 2);
    row = (off - col) / (lp->ncols + 2);
    ulloc = loc - lp->ncols - 3;
    ucloc = loc - lp->ncols - 2;
    urloc = loc - lp->ncols - 1;
    clloc = loc - 1;
    crloc = loc + 1;
    arloc = loc + 1;
    llloc = loc + lp->ncols + 1;
    lcloc = loc + lp->ncols + 2;
    lrloc = loc + lp->ncols + 3;
    if (row == 1) {
	ulloc += lastrow;
	ucloc += lastrow;
	urloc += lastrow;
    }
    if (row == lp->nrows) {
	llloc -= lastrow;
	lcloc -= lastrow;
	lrloc -= lastrow;
    }
    if (col == 1) {
	ulloc += lp->ncols;
	clloc += lp->ncols;
	llloc += lp->ncols;
    }
    if (col == lp->ncols) {
	urloc -= lp->ncols;
	crloc -= lp->ncols;
	lrloc -= lp->ncols;
    }
    *ulloc |= UPLT;
    *ucloc |= UP;
    *urloc |= UPRT;
    *clloc |= LT;
    *crloc |= RT;
    *arloc |= RT;
    *llloc |= DNLT;
    *lcloc |= DN;
    *lrloc |= DNRT;

    *(lp->agebuf + (loc - lp->buffer)) = 0;
}


static void
kill(loc)
    unsigned char *loc;
{
    lifestruct *lp = &lifes[screen];

    unsigned char *ulloc, *ucloc, *urloc, *clloc, *crloc, *llloc, *lcloc,
               *lrloc, *arloc;
    int         off, row, col, lastrow;

    lastrow = (lp->nrows) * (lp->ncols + 2);
    off = loc - lp->buffer;
    row = off / (lp->ncols + 2);
    col = off % (lp->ncols + 2);
    row = (off - col) / (lp->ncols + 2);
    ulloc = loc - lp->ncols - 3;
    ucloc = loc - lp->ncols - 2;
    urloc = loc - lp->ncols - 1;
    clloc = loc - 1;
    crloc = loc + 1;
    arloc = loc + 1;
    llloc = loc + lp->ncols + 1;
    lcloc = loc + lp->ncols + 2;
    lrloc = loc + lp->ncols + 3;
    if (row == 1) {
	ulloc += lastrow;
	ucloc += lastrow;
	urloc += lastrow;
    }
    if (row == lp->nrows) {
	llloc -= lastrow;
	lcloc -= lastrow;
	lrloc -= lastrow;
    }
    if (col == 1) {
	ulloc += lp->ncols;
	clloc += lp->ncols;
	llloc += lp->ncols;
    }
    if (col == lp->ncols) {
	urloc -= lp->ncols;
	crloc -= lp->ncols;
	lrloc -= lp->ncols;
    }
    *ulloc &= ~UPLT;
    *ucloc &= ~UP;
    *urloc &= ~UPRT;
    *clloc &= ~LT;
    *crloc &= ~RT;
    *arloc &= ~RT;
    *llloc &= ~DNLT;
    *lcloc &= ~DN;
    *lrloc &= ~DNRT;
}


static void
setcell(win, row, col)
    Window      win;
    int         row;
    int         col;
{
    lifestruct *lp = &lifes[screen];
    unsigned char *loc;

    loc = lp->buffer + ((row + 1) * (lp->ncols + 2)) + col + 1;
    spawn(loc);
    drawcell(win, row, col);
}


static void
init_fates()
{
    int         i, bits, neighbors;

    for (i = 0; i < 256; i++) {
	neighbors = 0;
	for (bits = i; bits; bits &= (bits - 1))
	    neighbors++;
	if (neighbors == 3)
	    fates[i] = BIRTH;
	else if (neighbors == 2)
	    fates[i] = SAME;
	else
	    fates[i] = DEATH;
    }
}


void
initlife(win)
    Window      win;
{
    int         row, col;
    int        *patptr;
    XWindowAttributes xgwa;
    lifestruct *lp = &lifes[screen];

    lp->generation = 0;
    lp->shooterTime = seconds();
    icon_width = lifeicon_width;
    icon_height = lifeicon_height;

    if (!initialized) {
	initialized = 1;
	init_fates();
	logo.data = (char *) lifeicon_bits;
	logo.width = icon_width;
	logo.height = icon_height;
	logo.bytes_per_line = (icon_width + 7) / 8;
    }
    XGetWindowAttributes(dsp, win, &xgwa);
    lp->width = xgwa.width;
    lp->height = xgwa.height;
    lp->pixelmode = (lp->width < 4 * icon_width);
    if (lp->pixelmode) {
	lp->ncols = 32;
	lp->nrows = 32;
    } else {
	lp->ncols = min(lp->width / icon_width, MAXCOLS);
	lp->nrows = min(lp->height / icon_height, MAXROWS);
    }
    lp->xs = lp->width / lp->ncols;
    lp->ys = lp->height / lp->nrows;
    lp->xb = (lp->width - lp->xs * lp->ncols) / 2;
    lp->yb = (lp->height - lp->ys * lp->nrows) / 2;

    XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
    XFillRectangle(dsp, win, Scr[screen].gc, 0, 0, lp->width, lp->height);

    bzero(lp->buffer, sizeof(lp->buffer));
    patptr = &patterns[random() % NPATS][0];
    while ((col = *patptr++) != 99) {
	row = *patptr++;
	col += lp->ncols / 2;
	row += lp->nrows / 2;
	setcell(win, row, col);
    }
}


void
drawlife(win)
    Window      win;
{
    unsigned char *loc, *temploc, *lastloc;
    int         row, col;
    unsigned char fate;
    lifestruct *lp = &lifes[screen];

    loc = lp->buffer + lp->ncols + 2 + 1;
    temploc = lp->tempbuf;
    /* copy the first 2 rows to the tempbuf */
    bcopy(loc, temploc, lp->ncols);
    bcopy(loc + lp->ncols + 2, temploc + lp->ncols, lp->ncols);

    lastloc = lp->lastbuf;
    /* copy the last row to another buffer for wraparound */
    bcopy(loc + ((lp->nrows - 1) * (lp->ncols + 2)), lastloc, lp->ncols);

    for (row = 0; row < lp->nrows; ++row) {
	for (col = 0; col < lp->ncols; ++col) {
	    fate = fates[*temploc];
	    *temploc = (row == (lp->nrows - 3)) ?
		*(lastloc + col) :
		*(loc + (lp->ncols + 2) * 2);
	    switch (fate) {
	    case BIRTH:
		if (!(*(loc + 1) & RT)) {
		    spawn(loc);
		}
		/* NO BREAK */
	    case SAME:
		if (*(loc + 1) & RT) {
		    drawcell(win, row, col);
		}
		break;
	    case DEATH:
		if (*(loc + 1) & RT) {
		    kill(loc);
		    erasecell(win, row, col);
		}
		break;
	    }
	    loc++;
	    temploc++;
	}
	loc += 2;
	if (temploc >= lp->tempbuf + lp->ncols * 2)
	    temploc = lp->tempbuf;
    }

    if (++lp->generation > batchcount)
	initlife(win);

    /*
     * generate a randomized shooter aimed roughly toward the center of the
     * screen after timeout.
     */

    if (seconds() - lp->shooterTime > TIMEOUT) {
	int         hsp = random() % (lp->ncols - 5) + 3;
	int         vsp = random() % (lp->nrows - 5) + 3;
	int         hoff = 1;
	int         voff = 1;
	if (vsp > lp->nrows / 2)
	    voff = -1;
	if (hsp > lp->ncols / 2)
	    hoff = -1;
	setcell(win, vsp + 0 * voff, hsp + 2 * hoff);
	setcell(win, vsp + 1 * voff, hsp + 2 * hoff);
	setcell(win, vsp + 2 * voff, hsp + 2 * hoff);
	setcell(win, vsp + 2 * voff, hsp + 1 * hoff);
	setcell(win, vsp + 1 * voff, hsp + 0 * hoff);
	lp->shooterTime = seconds();
    }
}
