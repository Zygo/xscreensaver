/* -*- Mode: C; tab-width: 4 -*- */
/* ant1d --- */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)ant1d.c	4.15 99/09/09 xlockmore";

#endif

/*-
 * Copyright (c) 1999 by David Bagley.
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
 * 09-Sep-99: written, used life1d.c as a basis, a 9/9/99 bug
 *
 */

#ifdef STANDALONE
#define PROGCLASS "Ant1D"
#define HACK_INIT init_ant1d
#define HACK_DRAW draw_ant1d
#define ant1d_opts xlockmore_opts
#define DEFAULTS "*delay: 10000 \n" \
 "*count: -5 \n" \
 "*cycles: 1200 \n" \
 "*size: -15 \n" \
 "*ncolors: 64 \n" \
 "*fullrandom: True \n" \
 "*verbose: False \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */
#include "automata.h"

#ifdef MODE_ant1d

#define DEF_EYES  "False"
#define DEF_SOUP  "False"

static Bool eyes;
static Bool soup;

static XrmOptionDescRec opts[] =
{
	{"-eyes", ".ant1d.eyes", XrmoptionNoArg, (caddr_t) "on"},
	{"+eyes", ".ant1d.eyes", XrmoptionNoArg, (caddr_t) "off"},
	{"-soup", ".ant1d.soup", XrmoptionNoArg, (caddr_t) "on"},
	{"+soup", ".ant1d.soup", XrmoptionNoArg, (caddr_t) "off"}
};
static argtype vars[] =
{
	{(caddr_t *) & eyes, "eyes", "Eyes", DEF_EYES, t_Bool},
	{(caddr_t *) & soup, "soup", "Soup", DEF_SOUP, t_Bool},
};
static OptionStruct desc[] =
{
	{"-/+eyes", "turn on/off eyes"},
	{"-/+soup", "turn on/off random soup"},
};

ModeSpecOpt ant1d_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   ant1d_description =
{"ant1d", "init_ant1d", "draw_ant1d", "release_ant1d",
 "refresh_ant1d", "init_ant1d", NULL, &ant1d_opts,
 10000, -3, 1000, -12, 64, 1.0, "",
 "Shows Turing machines on a tape", 0, NULL};

#endif

#define ANT1DBITS(n,w,h)\
  ap->pixmaps[ap->init_bits++]=\
  XCreatePixmapFromBitmapData(display,window,(char *)n,w,h,1,0,1)

#define STATES 2
#define MINANTS 1
#define REDRAWSTEP 2000		/* How much tape to draw per cycle */
#define MINGRIDSIZE 24
#define MINSIZE 2		/* 3 may be better here */
#define MINRANDOMSIZE 5

typedef struct {
	unsigned char color;
	short       direction;
	unsigned char next;
} statestruct;

typedef struct {
	int         col, row;
	short       direction;
	unsigned char state;
} antstruct;

typedef struct {
	int         painted;
	int         screen_generation, row;
	int         xs, ys, xb, yb;
	int         nrows, ncols;
	int         width, height;
	unsigned char ncolors, nstates;
	int         n;
	int         redrawing, redrawpos;
	int         eyes, soup;
	statestruct machine[NUMSTIPPLES * STATES];
	unsigned char *tape;
	antstruct  *ants;
	int         init_bits;
	unsigned char colors[NUMSTIPPLES - 1];
	GC          stippledGC;
	Pixmap      pixmaps[NUMSTIPPLES - 1];
	int         busyLoop;
	unsigned char *buffer;
} antfarm1dstruct;

/* Relative ant moves */
#define FS 0			/* Step */
#define TBS 1			/* Turn back, then step */
#define SF 2			/* Step */
#define STB 3			/* Step then turn back */

static antfarm1dstruct *antfarm1ds = NULL;

static unsigned char tables[][3 * NUMSTIPPLES * STATES + 2] =
{
  /* Here just so you can figure out notation */
	{			/* ZigZag */
		2, 1,
		1, TBS, 0, 0, STB, 0
	},
	{
		4, 1,
		1, TBS, 0, 2, FS, 0, 3, STB, 0, 0, SF, 0
	},
	{
		2, 2,
		1, TBS, 0, 0, FS, 1,
		1, STB, 0, 1, SF, 0
	},
	{
		2, 2,
		1, STB, 0, 0, FS, 1,
		0, TBS, 0, 1, SF, 0
	},
};

#define NTABLES   (sizeof tables / sizeof tables[0])

static void
position_of_neighbor(antfarm1dstruct * ap, int dir, int *pcol)
{
	int         col = *pcol;

	switch (dir) {
		case 0:
			col = (col + 1 == ap->ncols) ? 0 : col + 1;
			break;
		case 1:
			col = (!col) ? ap->ncols - 1 : col - 1;
			break;
		default:
			(void) fprintf(stderr, "wrong direction %d\n", dir);
	}
	*pcol = col;
}
static void
fillcell(ModeInfo * mi, GC gc, int col, int row)
{
	antfarm1dstruct *ap = &antfarm1ds[MI_SCREEN(mi)];

	XFillRectangle(MI_DISPLAY(mi), MI_WINDOW(mi), gc,
		       ap->xb + ap->xs * col, ap->yb + ap->ys * row,
		       ap->xs - (ap->xs > 3), ap->ys - (ap->ys > 3));
}

static void
drawcell(ModeInfo * mi, int col, int row, unsigned char color)
{
	antfarm1dstruct *ap = &antfarm1ds[MI_SCREEN(mi)];
	GC          gc;

	if (!color) {
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_BLACK_PIXEL(mi));
		gc = MI_GC(mi);
	} else if (MI_NPIXELS(mi) > 2) {
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi),
			       MI_PIXEL(mi, ap->colors[color - 1]));
		gc = MI_GC(mi);
	} else {
		XGCValues   gcv;

		gcv.stipple = ap->pixmaps[color - 1];
		gcv.foreground = MI_WHITE_PIXEL(mi);
		gcv.background = MI_BLACK_PIXEL(mi);
		XChangeGC(MI_DISPLAY(mi), ap->stippledGC,
			  GCStipple | GCForeground | GCBackground, &gcv);
		gc = ap->stippledGC;
	}
	fillcell(mi, gc, col, row);
}

static void
draw_anant(ModeInfo * mi, int direction, int col, int row)
{
	antfarm1dstruct *ap = &antfarm1ds[MI_SCREEN(mi)];
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);

	XSetForeground(display, MI_GC(mi), MI_WHITE_PIXEL(mi));
	fillcell(mi, MI_GC(mi), col, row);
	if (ap->eyes && ap->xs > 3 && ap->ys > 3) {	/* Draw Eyes */

		XSetForeground(display, MI_GC(mi), MI_BLACK_PIXEL(mi));
		switch (direction) {
			case 0:
				XDrawPoint(display, window, MI_GC(mi),
					   ap->xb + ap->xs * (col + 1) - 3,
				     ap->yb + ap->ys * row + ap->ys / 2 - 2);
				XDrawPoint(display, window, MI_GC(mi),
					   ap->xb + ap->xs * (col + 1) - 3,
					 ap->yb + ap->ys * row + ap->ys / 2);
				break;
			case 1:
				XDrawPoint(display, window, MI_GC(mi),
					   ap->xb + ap->xs * col + 1,
				     ap->yb + ap->ys * row + ap->ys / 2 - 2);
				XDrawPoint(display, window, MI_GC(mi),
					   ap->xb + ap->xs * col + 1,
					 ap->yb + ap->ys * row + ap->ys / 2);
				break;
			default:
				(void) fprintf(stderr, "wrong eyes direction %d\n", direction);
		}
	}
}

static void
RandomSoup(antfarm1dstruct * ap, int v)
{
	int         col;

	v /= 2;
	if (v < 1)
		v = 1;
	for (col = ap->ncols / 2 - v; col < ap->ncols / 2 + v; ++col)
		if (col >= 0 && col < ap->ncols)
			ap->tape[col] = (unsigned char) NRAND(ap->ncolors - 1) + 1;
}

static void
Rainbow(antfarm1dstruct * ap, int v)
{
	int         col;

	v /= 2;
	if (v < 1)
		v = 1;
	for (col = ap->ncols / 2 - v; col < ap->ncols / 2 + v; ++col)
		if (col >= 0 && col < ap->ncols && abs(col - ap->ncols) < ap->ncolors)
			ap->tape[col] = (unsigned char) (col - ap->ncols);
}

static void
getTable(ModeInfo * mi, int i)
{
	antfarm1dstruct *ap = &antfarm1ds[MI_SCREEN(mi)];
	int         j, total;
	unsigned char *patptr;

	patptr = &tables[i][0];
	ap->ncolors = *patptr++;
	ap->nstates = *patptr++;
	total = ap->ncolors * ap->nstates;
	if (MI_IS_VERBOSE(mi))
		(void) fprintf(stdout,
			 "ants %d, table number %d, colors %d, states %d\n ",
			       ap->n, i, ap->ncolors, ap->nstates);
	for (j = 0; j < total; j++) {
		ap->machine[j].color = *patptr++;
		ap->machine[j].direction = *patptr++;
		ap->machine[j].next = *patptr++;
	}
}

static void
getTurk(ModeInfo * mi, int i)
{
	antfarm1dstruct *ap = &antfarm1ds[MI_SCREEN(mi)];
	int         power2, j, number, total;

	/* To force a number, say <i = 2;> has  i + 2 (or 4) binary digits */
	power2 = 1 << (i + 1);
	/* Do not want numbers which in binary are all 1's. */
	number = NRAND(power2 - 1) + power2;
	/* To force a particular number, say <number = 10;> */

	ap->ncolors = i + 2;
	ap->nstates = 1;
	total = ap->ncolors * ap->nstates;
	for (j = 0; j < total; j++) {
		ap->machine[j].color = (j + 1) % total;
		ap->machine[j].direction = (power2 & number) ? FS : TBS;
		ap->machine[j].next = 0;
		power2 >>= 1;
	}
	if (MI_IS_VERBOSE(mi))
		(void) fprintf(stdout,
			       "ants %d, Turk's number %d, colors %d\n",
			       ap->n, number, ap->ncolors);
}

void
init_ant1d(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         size = MI_SIZE(mi);
	antfarm1dstruct *ap;
	int         col, dir;
	int         i;

	if (antfarm1ds == NULL) {
		if ((antfarm1ds = (antfarm1dstruct *) calloc(MI_NUM_SCREENS(mi),
					  sizeof (antfarm1dstruct))) == NULL)
			return;
	}
	ap = &antfarm1ds[MI_SCREEN(mi)];
	ap->row = 0;
	ap->redrawing = 0;
	if (MI_NPIXELS(mi) <= 2) {
		if (ap->stippledGC == None) {
			XGCValues   gcv;

			gcv.fill_style = FillOpaqueStippled;
			ap->stippledGC = XCreateGC(display, window, GCFillStyle, &gcv);
		}
		if (ap->init_bits == 0) {
			for (i = 1; i < NUMSTIPPLES ; i++)
				ANT1DBITS(stipples[i], STIPPLESIZE, STIPPLESIZE);
		}
	}
	ap->screen_generation = 0;
	ap->n = MI_COUNT(mi);
	if (ap->n < -MINANTS) {
		/* if ap->n is random ... the size can change */
		if (ap->ants != NULL) {
			(void) free((void *) ap->ants);
			ap->ants = NULL;
		}
		ap->n = NRAND(-ap->n - MINANTS + 1) + MINANTS;
	} else if (ap->n < MINANTS)
		ap->n = MINANTS;

	ap->width = MI_WIDTH(mi);
	ap->height = MI_HEIGHT(mi);

	if (size < -MINSIZE) {
		ap->ys = NRAND(MIN(-size, MAX(MINSIZE, MIN(ap->width, ap->height) /
				      MINGRIDSIZE)) - MINSIZE + 1) + MINSIZE;
		if (ap->ys < MINRANDOMSIZE)
			ap->ys = MIN(MINRANDOMSIZE,
				     MAX(MINSIZE, MIN(ap->width, ap->height) / MINGRIDSIZE));
	} else if (size < MINSIZE) {
		if (!size)
			ap->ys = MAX(MINSIZE, MIN(ap->width, ap->height) / MINGRIDSIZE);
		else
			ap->ys = MINSIZE;
	} else
		ap->ys = MIN(size, MAX(MINSIZE, MIN(ap->width, ap->height) /
				       MINGRIDSIZE));
	ap->xs = ap->ys;
	ap->ncols = MAX(ap->width / ap->xs, 2);
	ap->nrows = MAX(ap->height / ap->ys, 2);
	ap->xb = (ap->width - ap->xs * ap->ncols) / 2;
	ap->yb = (ap->height - ap->ys * ap->nrows) / 2;

	XSetLineAttributes(display, MI_GC(mi), 1, LineSolid, CapNotLast, JoinMiter);
	MI_CLEARWINDOW(mi);
	ap->painted = False;

	if (MI_IS_FULLRANDOM(mi)) {
		ap->eyes = (Bool) (LRAND() & 1);
		ap->soup = (Bool) (LRAND() & 1);
	} else {
		ap->eyes = eyes;
		ap->soup = soup;
	}
	ap->eyes = (ap->eyes && ap->xs > 3 && ap->ys > 3);
	/* Exclude odd # of neighbors, stepping forward not defined */
	if (!NRAND(NUMSTIPPLES)) {
		getTable(mi, (int) (NRAND(NTABLES)));
	} else
		getTurk(mi, (int) (NRAND(NUMSTIPPLES - 1)));
	if (MI_NPIXELS(mi) > 2)
		for (i = 0; i < (int) ap->ncolors - 1; i++)
			ap->colors[i] = (unsigned char) (NRAND(MI_NPIXELS(mi)) +
			     i * MI_NPIXELS(mi)) / ((int) (ap->ncolors - 1));
	if (ap->ants == NULL)
		ap->ants = (antstruct *) malloc(ap->n * sizeof (antstruct));
	if (ap->tape != NULL)
		(void) free((void *) ap->tape);
	ap->tape = (unsigned char *) calloc(ap->ncols, sizeof (unsigned char));

	if (ap->soup)
		RandomSoup(ap, ap->ncols / 4);
	else
		Rainbow(ap, ap->ncols / 4);
	if (ap->buffer != NULL)
		(void) free((void *) ap->buffer);
	ap->buffer = (unsigned char *) calloc(ap->ncols * ap->nrows,
					      sizeof (unsigned char));

	ap->busyLoop = 0;

	col = ap->ncols / 2;
	dir = LRAND() & 1;
	/* Have them all start in the same spot, why not? */
	for (i = 0; i < ap->n; i++) {
		ap->ants[i].col = col;
		ap->ants[i].row = ap->row;
		ap->ants[i].direction = dir;
		ap->ants[i].state = 0;
	}
	draw_anant(mi, dir, col, ap->row);
}

void
draw_ant1d(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	antstruct  *anant;
	statestruct *status;
	int         i, state_pos, tape_pos, col;
	unsigned char color;
	short       chg_dir, old_dir;
	antfarm1dstruct *ap;

	if (antfarm1ds == NULL)
		return;
	ap = &antfarm1ds[MI_SCREEN(mi)];

	MI_IS_DRAWN(mi) = True;
	ap->painted = True;

	if (ap->busyLoop) {
		if (ap->busyLoop >= 250)
			ap->busyLoop = 0;
		else
			ap->busyLoop++;
		return;
	}
	if (ap->row == 0) {
		if (ap->screen_generation > MI_CYCLES(mi))
			init_ant1d(mi);
		for (col = 0; col < ap->ncols; col++) {
			drawcell(mi, col, ap->row, ap->tape[col]);
		}
		(void) memcpy((char *) ap->buffer, (char *) ap->tape, ap->ncols);
	} else {
		for (col = 0; col < ap->ncols; col++) {
			drawcell(mi, col, ap->row, ap->tape[col]);
		}
		for (i = 0; i < ap->n; i++) {
			anant = &ap->ants[i];
			tape_pos = anant->col;
			color = ap->tape[tape_pos];	/* read tape */
			state_pos = color + anant->state * ap->ncolors;
			status = &(ap->machine[state_pos]);
			drawcell(mi, anant->col, ap->row, status->color);
			ap->tape[tape_pos] = status->color;	/* write on tape */

			/* Find direction of Ants. */
			/* Translate relative direction to actual direction */
			old_dir = anant->direction;
			chg_dir = (4 - status->direction) % 2;
			anant->direction = (chg_dir + old_dir) % 2;
			anant->state = status->next;

			old_dir = ((status->direction < 2) ? anant->direction : old_dir);
			position_of_neighbor(ap, old_dir, &(anant->col));
			draw_anant(mi, anant->direction, anant->col, ap->row);
		}
		(void) memcpy((char *) (ap->buffer + ap->row * ap->ncols),
			      (char *) ap->tape, ap->ncols);
		if (++ap->screen_generation > MI_CYCLES(mi)) {
			init_ant1d(mi);
		}
	}
	ap->row++;
	if (ap->row >= ap->nrows) {
		ap->screen_generation++;
		ap->busyLoop = 1;
		ap->row = 0;
	}
	if (ap->redrawing) {
		for (i = 0; i < REDRAWSTEP; i++) {
			if (ap->buffer[ap->redrawpos]) {
				drawcell(mi, ap->redrawpos % ap->ncols, ap->redrawpos / ap->ncols,
					 ap->buffer[ap->redrawpos]);
			}
			if (++(ap->redrawpos) >= ap->ncols * ap->nrows) {
				ap->redrawing = 0;
				break;
			}
		}
	}
}

void
release_ant1d(ModeInfo * mi)
{
	if (antfarm1ds != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			antfarm1dstruct *ap = &antfarm1ds[screen];
			int         shade;

			if (ap->stippledGC != None) {
				XFreeGC(MI_DISPLAY(mi), ap->stippledGC);
			}
			for (shade = 0; shade < ap->init_bits; shade++)
				XFreePixmap(MI_DISPLAY(mi), ap->pixmaps[shade]);
			if (ap->tape != NULL)
				(void) free((void *) ap->tape);
			if (ap->buffer != NULL)
				(void) free((void *) ap->buffer);
			if (ap->ants != NULL)
				(void) free((void *) ap->ants);
		}
		(void) free((void *) antfarm1ds);
		antfarm1ds = NULL;
	}
}

void
refresh_ant1d(ModeInfo * mi)
{
	antfarm1dstruct *ap;

	if (antfarm1ds == NULL)
		return;
	ap = &antfarm1ds[MI_SCREEN(mi)];

	if (ap->painted) {
		MI_CLEARWINDOW(mi);
		ap->redrawing = 1;
		ap->redrawpos = 0;
	}
}

#endif /* MODE_ant1d */
