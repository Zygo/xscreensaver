/* -*- Mode: C; tab-width: 4 -*- */
/* loop --- Chris Langton's self-producing loops */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)loop.c	4.07 97/11/24 xlockmore";

#endif

/*-
 * Copyright (c) 1996 by David Bagley.
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
 * 15-Nov-95: Coded from Chris Langton's Self-Reproduction in Cellular
 *            Automata Physica 10D 135-144 1984
 *            also used wire.c as a guide.
 */

/*-
 * From Steven Levy's Artificial Life
 * Chris Langton's cellular automata "loops" reproduce in the spirit of life.
 * Beginning from a single organism, the loops from a colony.  As the loops
 * on the outer fringes reproduce, the inner loops -- blocked by their
 * daughters -- can no longer produce offspring.  These dead progenitors
 * provide a base for future generations' expansion, much like the formation
 * of a coral reef.  This self-organizing behavior emerges spontaneously,
 * from the bottom up -- a key characteristic of artificial life.
 */

/*-
   Don't Panic  --  When the artificial life tries to leave its petri
   dish (ie. the screen) it will (usually) die...
   The loops are short of "real" life because a general purpose Turing
   machine is not contained in the loop.  This is a simplification of
   von Neumann and Codd's self-producing Turing machine.
 */

#ifdef STANDALONE
#define PROGCLASS "loop"
#define HACK_INIT init_loop
#define HACK_DRAW draw_loop
#define loop_opts xlockmore_opts
#define DEFAULTS "*delay: 100000 \n" \
 "*cycles: 1600 \n" \
 "*size: -12 \n" \
 "*ncolors: 15 \n"
#define SPREAD_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

ModeSpecOpt loop_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   loop_description =
{"loop", "init_loop", "draw_loop", "release_loop",
 "refresh_loop", "init_loop", NULL, &loop_opts,
 100000, 1, 1600, -12, 64, 1.0, "",
 "Shows Langton's self-producing loops", 0, NULL};

#endif

#define LOOPBITS(n,w,h)\
  lp->pixmaps[lp->init_bits++]=\
  XCreatePixmapFromBitmapData(display,window,(char *)n,w,h,1,0,1)

#define COLORS 8
#define REALCOLORS (COLORS-2)
#define MINLOOPS 1
#define REDRAWSTEP 2000		/* How many cells to draw per cycle */
#define ADAM_LOOPX 16
#define ADAM_LOOPY 10
#define MINGRIDSIZE (3*ADAM_LOOPX)
#define MINSIZE 1

/* Singly linked list */
typedef struct _RectList {
	XPoint      pt;
	struct _RectList *next;
} RectList;

typedef struct {
	int         init_bits;
	int         generation;
	int         xs, ys;
	int         xb, yb;
	int         nrows, ncols;
	int         bnrows, bncols;
	int         mincol, minrow, maxcol, maxrow;
	int         width, height;
	int         redrawing, redrawpos;
	unsigned char *newcells, *oldcells;
	unsigned int colors[COLORS];
	int         nrects[COLORS];
	RectList   *rectlist[COLORS];
	GC          stippledGC;
	Pixmap      pixmaps[COLORS];
} loopstruct;

static loopstruct *loops = NULL;

#define TRANSITION(TT,V) V=TT&7;TT>>=3
#define TABLE(T,R,B,L) (table[((T)<<9)|((R)<<6)|((B)<<3)|(L)])
#ifdef RAND_RULES		/* Hack, see below */
#define TABLE_IN(C,T,R,B,L,I) (TABLE(T,R,B,L)&=~(7<<((C)*3)));\
(TABLE(T,R,B,L)|=((I)<<((C)*3)))
#else
#define TABLE_IN(C,T,R,B,L,I) (TABLE(T,R,B,L)|=((I)<<((C)*3)))
#endif
#define TABLE_OUT(C,T,R,B,L) ((TABLE(T,R,B,L)>>((C)*3))&7)
static unsigned int *table = NULL;	/* 8*8*8*8 = 2^12 = 2^3^4 = 4K */

static unsigned int transition_table[] =
{				/* Octal */
/* CTRBL->I CTRBL->I CTRBL->I CTRBL->I CTRBL->I */
	0000000, 0025271, 0113221, 0202422, 0301021,
	0000012, 0100011, 0122244, 0202452, 0301220,
	0000020, 0100061, 0122277, 0202520, 0302511,
	0000030, 0100077, 0122434, 0202552, 0401120,
	0000050, 0100111, 0122547, 0202622, 0401220,
	0000063, 0100121, 0123244, 0202722, 0401250,
	0000071, 0100211, 0123277, 0203122, 0402120,
	0000112, 0100244, 0124255, 0203216, 0402221,
	0000122, 0100277, 0124267, 0203226, 0402326,
	0000132, 0100511, 0125275, 0203422, 0402520,
	0000212, 0101011, 0200012, 0204222, 0403221,
	0000220, 0101111, 0200022, 0205122, 0500022,
	0000230, 0101244, 0200042, 0205212, 0500215,
	0000262, 0101277, 0200071, 0205222, 0500225,
	0000272, 0102026, 0200122, 0205521, 0500232,
	0000320, 0102121, 0200152, 0205725, 0500272,
	0000525, 0102211, 0200212, 0206222, 0500520,
	0000622, 0102244, 0200222, 0206722, 0502022,
	0000722, 0102263, 0200232, 0207122, 0502122,
	0001022, 0102277, 0200242, 0207222, 0502152,
	0001120, 0102327, 0200250, 0207422, 0502220,
	0002020, 0102424, 0200262, 0207722, 0502244,
	0002030, 0102626, 0200272, 0211222, 0502722,
	0002050, 0102644, 0200326, 0211261, 0512122,
	0002125, 0102677, 0200423, 0212222, 0512220,
	0002220, 0102710, 0200517, 0212242, 0512422,
	0002322, 0102727, 0200522, 0212262, 0512722,
	0005222, 0105427, 0200575, 0212272, 0600011,
	0012321, 0111121, 0200722, 0214222, 0600021,
	0012421, 0111221, 0201022, 0215222, 0602120,
	0012525, 0111244, 0201122, 0216222, 0612125,
	0012621, 0111251, 0201222, 0217222, 0612131,
	0012721, 0111261, 0201422, 0222272, 0612225,
	0012751, 0111277, 0201722, 0222442, 0700077,
	0014221, 0111522, 0202022, 0222462, 0701120,
	0014321, 0112121, 0202032, 0222762, 0701220,
	0014421, 0112221, 0202052, 0222772, 0701250,
	0014721, 0112244, 0202073, 0300013, 0702120,
	0016251, 0112251, 0202122, 0300022, 0702221,
	0017221, 0112277, 0202152, 0300041, 0702251,
	0017255, 0112321, 0202212, 0300076, 0702321,
	0017521, 0112424, 0202222, 0300123, 0702525,
	0017621, 0112621, 0202272, 0300421, 0702720,
	0017721, 0112727, 0202321, 0300622
};


/*-
Neighborhoods are read as follows (rotations are not listed):
    T
  L C R  ==>  I
    B
 */

static unsigned char self_reproducing_loop[ADAM_LOOPY][ADAM_LOOPX] =
{
	{0, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0},
	{2, 1, 7, 0, 1, 4, 0, 1, 4, 2, 0, 0, 0, 0, 0},
	{2, 0, 2, 2, 2, 2, 2, 2, 0, 2, 0, 0, 0, 0, 0},
	{2, 7, 2, 0, 0, 0, 0, 2, 1, 2, 0, 0, 0, 0, 0},
	{2, 1, 2, 0, 0, 0, 0, 2, 1, 2, 0, 0, 0, 0, 0},
	{2, 0, 2, 0, 0, 0, 0, 2, 1, 2, 0, 0, 0, 0, 0},
	{2, 7, 2, 0, 0, 0, 0, 2, 1, 2, 0, 0, 0, 0, 0},
	{2, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 0},
	{2, 0, 7, 1, 0, 7, 1, 0, 7, 1, 1, 1, 1, 1, 2},
	{0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0}
};

static void
addtolist(ModeInfo * mi, int col, int row, unsigned char state)
{
	loopstruct *lp = &loops[MI_SCREEN(mi)];
	RectList   *current;

	current = lp->rectlist[state];
	lp->rectlist[state] = (RectList *) malloc(sizeof (RectList));
	lp->rectlist[state]->pt.x = col;
	lp->rectlist[state]->pt.y = row;
	lp->rectlist[state]->next = current;
	lp->nrects[state]++;
}

#ifdef DEBUG
static void
print_state(ModeInfo * mi, int state)
{
	loopstruct *lp = &loops[MI_SCREEN(mi)];
	RectList   *locallist;
	int         i = 0;

	locallist = lp->rectlist[state];
	(void) printf("state %d\n", state);
	while (locallist) {
		(void) printf("%d x %d, y %d\n", i,
			      locallist->pt.x, locallist->pt.y);
		locallist = locallist->next;
		i++;
	}
}

#endif

static void
free_state(loopstruct * lp, int state)
{
	RectList   *current;

	while (lp->rectlist[state]) {
		current = lp->rectlist[state];
		lp->rectlist[state] = lp->rectlist[state]->next;
		(void) free((void *) current);
	}
	lp->rectlist[state] = NULL;
	lp->nrects[state] = 0;
}

static void
drawcell(ModeInfo * mi, int col, int row, int state)
{
	loopstruct *lp = &loops[MI_SCREEN(mi)];
	XGCValues   gcv;
	GC          gc;

	if (MI_NPIXELS(mi) >= COLORS) {
		gc = MI_GC(mi);
		XSetForeground(MI_DISPLAY(mi), gc, lp->colors[state]);
	} else {
		gcv.stipple = lp->pixmaps[state];
		gcv.foreground = MI_WHITE_PIXEL(mi);
		gcv.background = MI_BLACK_PIXEL(mi);
		XChangeGC(MI_DISPLAY(mi), lp->stippledGC,
			  GCStipple | GCForeground | GCBackground, &gcv);
		gc = lp->stippledGC;
	}
	XFillRectangle(MI_DISPLAY(mi), MI_WINDOW(mi), gc,
	       lp->xb + lp->xs * col, lp->yb + lp->ys * row, lp->xs, lp->ys);
}

static void
draw_state(ModeInfo * mi, int state)
{
	loopstruct *lp = &loops[MI_SCREEN(mi)];
	GC          gc;
	XRectangle *rects;
	XGCValues   gcv;
	RectList   *current;

	if (MI_NPIXELS(mi) >= COLORS) {
		gc = MI_GC(mi);
		XSetForeground(MI_DISPLAY(mi), gc, lp->colors[state]);
	} else {
		gcv.stipple = lp->pixmaps[state];
		gcv.foreground = MI_WHITE_PIXEL(mi);
		gcv.background = MI_BLACK_PIXEL(mi);
		XChangeGC(MI_DISPLAY(mi), lp->stippledGC,
			  GCStipple | GCForeground | GCBackground, &gcv);
		gc = lp->stippledGC;
	}

	{
		/* Take advantage of XFillRectangles */
		int         nrects = 0;

		/* Create Rectangle list from part of the rectlist */
		rects = (XRectangle *) malloc(lp->nrects[state] * sizeof (XRectangle));
		current = lp->rectlist[state];
		while (current) {
			rects[nrects].x = lp->xb + current->pt.x * lp->xs;
			rects[nrects].y = lp->yb + current->pt.y * lp->ys;
			rects[nrects].width = lp->xs;
			rects[nrects].height = lp->ys;
			current = current->next;
			nrects++;
		}
		/* Finally get to draw */
		XFillRectangles(MI_DISPLAY(mi), MI_WINDOW(mi), gc, rects, nrects);
		/* Free up rects list and the appropriate part of the rectlist */
		(void) free((void *) rects);
	}
	free_state(lp, state);
	XFlush(MI_DISPLAY(mi));
}

static void
init_table(void)
{
	if (table == NULL) {
		unsigned int tt, c, t, r, b, l, i;
		int         j, size_transition_table = sizeof (transition_table) /
		sizeof (unsigned int);

		if ((table = (unsigned int *) calloc(8 * 8 * 8 * 8,
					     sizeof (unsigned int))) == NULL)
			            return;

#ifdef RAND_RULES
		/* Here I was interested to see what happens when it hits a wall....
		   Rules not normally used take over... */
		{
			int         k;

			for (j = 0; j < 8 * 8 * 8 * 8; j++) {
				for (k = 0; k < 8; k++)
					table[j] |= (unsigned int) ((unsigned int) (NRAND(8)) << (k * 3));
			}
		}
#endif
		for (j = 0; j < size_transition_table; j++) {
			tt = transition_table[j];
			TRANSITION(tt, i);
			TRANSITION(tt, l);
			TRANSITION(tt, b);
			TRANSITION(tt, r);
			TRANSITION(tt, t);
			TRANSITION(tt, c);
			TABLE_IN(c, t, r, b, l, i);
			TABLE_IN(c, r, b, l, t, i);
			TABLE_IN(c, b, l, t, r, i);
			TABLE_IN(c, l, t, r, b, i);
		}
	}
}

static void
init_adam(loopstruct * lp)
{
	XPoint      start, dirx, diry;
	int         i, j;

	switch (NRAND(4)) {
		case 0:
			start.x = (lp->bncols - ADAM_LOOPX) / 2;
			start.y = (lp->bnrows - ADAM_LOOPY) / 2;
			dirx.x = 1, dirx.y = 0;
			diry.x = 0, diry.y = 1;
			lp->mincol = start.x, lp->minrow = start.y;
			lp->maxcol = start.x + ADAM_LOOPX, lp->maxrow = start.y + ADAM_LOOPY;
			break;
		case 1:
			start.x = (lp->bncols + ADAM_LOOPY) / 2;
			start.y = (lp->bnrows - ADAM_LOOPX) / 2;
			dirx.x = 0, dirx.y = 1;
			diry.x = -1, diry.y = 0;
			lp->mincol = start.x - ADAM_LOOPY, lp->minrow = start.y;
			lp->maxcol = start.x, lp->maxrow = start.y + ADAM_LOOPX;
			break;
		case 2:
			start.x = (lp->bncols + ADAM_LOOPX) / 2;
			start.y = (lp->bnrows + ADAM_LOOPY) / 2;
			dirx.x = -1, dirx.y = 0;
			diry.x = 0, diry.y = -1;
			lp->mincol = start.x - ADAM_LOOPX, lp->minrow = start.y - ADAM_LOOPY;
			lp->maxcol = start.x, lp->maxrow = start.y;
			break;
		case 3:
			start.x = (lp->bncols - ADAM_LOOPY) / 2;
			start.y = (lp->bnrows + ADAM_LOOPX) / 2;
			dirx.x = 0, dirx.y = -1;
			diry.x = 1, diry.y = 0;
			lp->mincol = start.x, lp->minrow = start.y - ADAM_LOOPX;
			lp->maxcol = start.x + ADAM_LOOPY, lp->maxrow = start.y;
			break;
	}
	for (j = 0; j < ADAM_LOOPY; j++)
		for (i = 0; i < ADAM_LOOPX; i++)
			lp->newcells[(start.y + dirx.y * i + diry.y * j) * lp->bncols +
				     start.x + dirx.x * i + diry.x * j] =
				self_reproducing_loop[j][i];
}

static void
do_gen(loopstruct * lp)
{
	int         i, j;
	unsigned char *z;
	unsigned int c, t, r, b, l;

	for (j = lp->minrow; j <= lp->maxrow; j++) {
		for (i = lp->mincol; i <= lp->maxcol; i++) {
			z = lp->newcells + i + j * lp->bncols;
			c = *(lp->oldcells + i + j * lp->bncols);
			t = *(lp->oldcells + i + (j - 1) * lp->bncols);
			r = *(lp->oldcells + i + 1 + j * lp->bncols);
			b = *(lp->oldcells + i + (j + 1) * lp->bncols);
			l = *(lp->oldcells + i - 1 + j * lp->bncols);
			*z = TABLE_OUT(c, t, r, b, l);
		}
	}
}

static void
free_list(loopstruct * lp)
{
	int         state;

	for (state = 0; state < COLORS; state++)
		free_state(lp, state);
}

void
init_loop(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         size = MI_SIZE(mi);
	loopstruct *lp;
	XGCValues   gcv;

	if (loops == NULL) {
		if ((loops = (loopstruct *) calloc(MI_NUM_SCREENS(mi),
					       sizeof (loopstruct))) == NULL)
			return;
	}
	lp = &loops[MI_SCREEN(mi)];
	lp->redrawing = 0;
	if (MI_NPIXELS(mi) < COLORS) {
		if (lp->stippledGC == None) {
			gcv.fill_style = FillOpaqueStippled;
			lp->stippledGC = XCreateGC(display, window, GCFillStyle, &gcv);
		}
		if (lp->init_bits == 0) {
			LOOPBITS(stipples[0], STIPPLESIZE, STIPPLESIZE);
			LOOPBITS(stipples[2], STIPPLESIZE, STIPPLESIZE);
			LOOPBITS(stipples[3], STIPPLESIZE, STIPPLESIZE);
			LOOPBITS(stipples[4], STIPPLESIZE, STIPPLESIZE);
			LOOPBITS(stipples[6], STIPPLESIZE, STIPPLESIZE);
			LOOPBITS(stipples[7], STIPPLESIZE, STIPPLESIZE);
			LOOPBITS(stipples[8], STIPPLESIZE, STIPPLESIZE);
			LOOPBITS(stipples[10], STIPPLESIZE, STIPPLESIZE);
		}
	}
	if (MI_NPIXELS(mi) >= COLORS) {
		/* Maybe these colors should be randomized */
		lp->colors[0] = MI_BLACK_PIXEL(mi);
		lp->colors[1] = MI_PIXEL(mi, 0);	/* RED */
		lp->colors[5] = MI_PIXEL(mi, MI_NPIXELS(mi) / REALCOLORS);	/* YELLOW */
		lp->colors[4] = MI_PIXEL(mi, 2 * MI_NPIXELS(mi) / REALCOLORS);	/* GREEN */
		lp->colors[6] = MI_PIXEL(mi, 3 * MI_NPIXELS(mi) / REALCOLORS);	/* CYAN */
		lp->colors[2] = MI_PIXEL(mi, 4 * MI_NPIXELS(mi) / REALCOLORS);	/* BLUE */
		lp->colors[3] = MI_PIXEL(mi, 5 * MI_NPIXELS(mi) / REALCOLORS);	/* MAGENTA */
		lp->colors[7] = MI_WHITE_PIXEL(mi);
	}
	free_list(lp);
	lp->generation = 0;

	lp->width = MI_WIN_WIDTH(mi);
	lp->height = MI_WIN_HEIGHT(mi);

	if (size < -MINSIZE)
		lp->ys = NRAND(MIN(-size, MAX(MINSIZE, MIN(lp->width, lp->height) /
				      MINGRIDSIZE)) - MINSIZE + 1) + MINSIZE;
	else if (size < MINSIZE) {
		if (!size)
			lp->ys = MAX(MINSIZE, MIN(lp->width, lp->height) / MINGRIDSIZE);
		else
			lp->ys = MINSIZE;
	} else
		lp->ys = MIN(size, MAX(MINSIZE, MIN(lp->width, lp->height) /
				       MINGRIDSIZE));
	lp->xs = lp->ys;
	lp->ncols = MAX(lp->width / lp->xs, ADAM_LOOPX + 1);
	lp->nrows = MAX(lp->height / lp->ys, ADAM_LOOPX + 1);
	lp->xb = (lp->width - lp->xs * lp->ncols) / 2;
	lp->yb = (lp->height - lp->ys * lp->nrows) / 2;

	lp->bncols = lp->ncols + 2;
	lp->bnrows = lp->nrows + 2;

	MI_CLEARWINDOW(mi);

	if (lp->oldcells != NULL)
		(void) free((void *) lp->oldcells);
	lp->oldcells = (unsigned char *)
		calloc(lp->bncols * lp->bnrows, sizeof (unsigned char));

	if (lp->newcells != NULL)
		(void) free((void *) lp->newcells);
	lp->newcells = (unsigned char *)
		calloc(lp->bncols * lp->bnrows, sizeof (unsigned char));

	init_table();
	init_adam(lp);
}

void
draw_loop(ModeInfo * mi)
{
	loopstruct *lp = &loops[MI_SCREEN(mi)];
	int         offset, i, j, life = 0;
	unsigned char *z, *znew;

	for (j = lp->minrow; j <= lp->maxrow; j++) {
		for (i = lp->mincol; i <= lp->maxcol; i++) {
			offset = j * lp->bncols + i;
			z = lp->oldcells + offset;
			znew = lp->newcells + offset;
			if (*z != *znew) {
				*z = *znew;
				addtolist(mi, i - 1, j - 1, *znew);
				life = 1;
				if (i == lp->mincol && i > 1)
					lp->mincol--;
				if (j == lp->minrow && j > 1)
					lp->minrow--;
				if (i == lp->maxcol && i < lp->bncols - 2)
					lp->maxcol++;
				if (j == lp->maxrow && j < lp->bnrows - 2)
					lp->maxrow++;
			}
		}
	}
	for (i = 0; i < COLORS; i++)
		draw_state(mi, i);
	if (++lp->generation > MI_CYCLES(mi) || !life)
		init_loop(mi);
	else
		do_gen(lp);

	if (lp->redrawing) {
		for (i = 0; i < REDRAWSTEP; i++) {
			if ((*(lp->oldcells + lp->redrawpos))) {
				drawcell(mi, lp->redrawpos % lp->bncols - 1,
					 lp->redrawpos / lp->bncols - 1, *(lp->oldcells + lp->redrawpos));
			}
			if (++(lp->redrawpos) >= lp->bncols * (lp->bnrows - 1)) {
				lp->redrawing = 0;
				break;
			}
		}
	}
}

void
release_loop(ModeInfo * mi)
{
	if (loops != NULL) {
		int         screen, shade;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			loopstruct *lp = &loops[screen];

			free_list(lp);
			for (shade = 0; shade < lp->init_bits; shade++)
				XFreePixmap(MI_DISPLAY(mi), lp->pixmaps[shade]);
			if (lp->stippledGC != None)
				XFreeGC(MI_DISPLAY(mi), lp->stippledGC);
			if (lp->oldcells != NULL)
				(void) free((void *) lp->oldcells);
			if (lp->newcells != NULL)
				(void) free((void *) lp->newcells);
		}
		(void) free((void *) loops);
		loops = NULL;
	}
	if (table != NULL) {
		(void) free((void *) table);
		table = NULL;
	}
}

void
refresh_loop(ModeInfo * mi)
{
	loopstruct *lp = &loops[MI_SCREEN(mi)];

	lp->redrawing = 1;
	lp->redrawpos = lp->bncols + 1;
}
