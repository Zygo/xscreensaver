/* -*- Mode: C; tab-width: 4 -*- */
/* pacman --- Mr. Pacman and his ghost friends */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)pacman.c	4.07 97/11/24 xlockmore";

#endif

/*-
 * Copyright (c) 1995 by Heath Rice <rice@asl.dl.nec.com>.
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
 * 04-Jun-97: Compatible with xscreensaver
 *
 */

/*-
 * [Pacman eats screen.  Ghosts put screen back.]
 * Pacman eats ghosts when he encounters them.
 * After all ghosts are eaten, pacman continues
 * eating the screen until all of it is gone. Then
 * it starts over.
 * Food dots should be added.  Restart when done eating.
 */

#ifdef STANDALONE
#define PROGCLASS "Pacman"
#define HACK_INIT init_pacman
#define HACK_DRAW draw_pacman
#define pacman_opts xlockmore_opts
#define DEFAULTS "*delay: 100000 \n" \
 "*count: 10 \n" \
 "*size: 0 \n" \
 "*ncolors: 6 \n" \
 "*bitmap: \n"
#define UNIFORM_COLORS
#define BRIGHT_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */
#include "iostuff.h"

#ifdef MODE_pacman

ModeSpecOpt pacman_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   pacman_description =
{"pacman", "init_pacman", "draw_pacman", "release_pacman",
 "refresh_pacman", "init_pacman", NULL, &pacman_opts,
 100000, 10, 1, 0, 64, 1.0, "",
 "Shows Pacman(tm)", 0, NULL};

#endif

/* aliases for vars defined in the bitmap file */
#define CELL_WIDTH   image_width
#define CELL_HEIGHT    image_height
#define CELL_BITS    image_bits

#include "ghost.xbm"

#define MINGHOSTS 1
#define MAXMOUTH 11
#define MINGRIDSIZE 4
#define MINSIZE 1
#define NOWHERE 255

#define NONE 0x0000
#define LT   0x1000
#define RT   0x0001
#define RB   0x0010
#define LB   0x0100
#define ALL  0x1111

#define YELLOW (MI_NPIXELS(mi) / 6)
#define GREEN (23 * MI_NPIXELS(mi) / 64)
#define BLUE (45 * MI_NPIXELS(mi) / 64)

typedef struct {
	int         col, row;
	int         nextbox, lastbox, nextcol, nextrow;
	int         dead;
	int         mouthstage, mouthdirection;
	/*int         color; */
} beingstruct;

typedef struct {
	int         pixelmode;
	int         width, height;
	int         nrows, ncols;
	int         xs, ys, xb, yb;
	int         incx, incy;
	GC          stippledGC;
	Pixmap      ghostPixmap;
	int         graphics_format;
	beingstruct pacman;
	beingstruct *ghosts;
	int         nghosts;
#ifdef DEFUNCT
	unsigned int *eaten;
#endif
	Pixmap      pacmanPixmap[4][MAXMOUTH];
} pacmangamestruct;

static pacmangamestruct *pacmangames = NULL;

#if DEFUNCT
static void
clearcorners(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	Window      window = MI_WINDOW(mi);
	pacmangamestruct *pp = &pacmangames[MI_SCREEN(mi)];

	if (MI_NPIXELS(mi) > 2)
		XSetForeground(display, gc, MI_PIXEL(mi, GREEN));
	else
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
	if ((pp->pacman.nextrow == 0) && (pp->pacman.nextcol == 0)) {
		XFillRectangle(display, window, gc, 0, 0,
		((pp->width / pp->ncols) / 2), ((pp->width / pp->ncols) / 2));
		pp->eaten[(pp->pacman.nextcol * pp->nrows) + pp->pacman.nextrow] |=
			RT | LT | RB | LB;
	} else if ((pp->pacman.nextrow == pp->nrows - 1) &&
		   (pp->pacman.nextcol == 0)) {
		XFillRectangle(display, window, gc,
		     0, (pp->nrows * pp->ys) - ((pp->width / pp->ncols) / 2),
		((pp->width / pp->ncols) / 2), ((pp->width / pp->ncols) / 2));
		pp->eaten[(pp->pacman.nextcol * pp->nrows) + pp->pacman.nextrow] |=
			RT | LT | RB | LB;
	} else if ((pp->pacman.nextrow == 0) &&
		   (pp->pacman.nextcol == pp->ncols - 1)) {
		XFillRectangle(display, window, gc,
		     (pp->ncols * pp->xs) - ((pp->width / pp->ncols) / 2), 0,
		((pp->width / pp->ncols) / 2), ((pp->width / pp->ncols) / 2));
		pp->eaten[(pp->pacman.nextcol * pp->nrows) + pp->pacman.nextrow] |=
			RT | LT | RB | LB;
	} else if ((pp->pacman.nextrow == pp->nrows - 1) &&
		   (pp->pacman.nextcol == pp->ncols - 1)) {
		XFillRectangle(display, window, gc,
			(pp->ncols * pp->xs) - ((pp->width / pp->ncols) / 2),
			(pp->nrows * pp->ys) - ((pp->width / pp->ncols) / 2),
		((pp->width / pp->ncols) / 2), ((pp->width / pp->ncols) / 2));
		pp->eaten[(pp->pacman.nextcol * pp->nrows) + pp->pacman.nextrow] |=
			RT | LT | RB | LB;
	}
}
#endif
static void
repopulate(ModeInfo * mi)
{
	pacmangamestruct *pp = &pacmangames[MI_SCREEN(mi)];
	int         ghost;

#if DEFUNCT
	if (pp->eaten)
		(void) free((void *) pp->eaten);
	pp->eaten = (unsigned int *) calloc((pp->nrows * pp->ncols),
					    sizeof (unsigned int));

#endif

	MI_CLEARWINDOW(mi);

	for (ghost = 0; ghost < pp->nghosts; ghost++) {
		do {
			pp->ghosts[ghost].col = NRAND(pp->ncols);
			pp->ghosts[ghost].row = NRAND(pp->nrows);
			if ((pp->ghosts[ghost].row + pp->ghosts[ghost].col) % 2 !=
			    (pp->pacman.row + pp->pacman.col) % 2) {
				if (pp->ghosts[ghost].col != 0)
					pp->ghosts[ghost].col--;
				else
					pp->ghosts[ghost].col++;
			}
		}
		while ((pp->ghosts[ghost].col == pp->pacman.col) &&
		       (pp->ghosts[ghost].row == pp->pacman.row));
		pp->ghosts[ghost].dead = 0;
		pp->ghosts[ghost].lastbox = -1;

	}
}

static void
movepac(ModeInfo * mi)
{
	typedef struct {
		int         cfactor, rfactor;
		int         cf, rf;
		int         oldcf, oldrf;
	} being;
	being      *g, p;

	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	pacmangamestruct *pp = &pacmangames[MI_SCREEN(mi)];
	int         ghost, alldead, dir;
	XPoint      delta;

	if (pp->pacman.nextcol > pp->pacman.col) {
		p.cfactor = 1;
#if DEFUNCT
		if (pp->eaten) {
			pp->eaten[(pp->pacman.col * pp->nrows) + pp->pacman.row] |=
				RT | RB;
			pp->eaten[((pp->pacman.col + 1) * pp->nrows) + pp->pacman.row] |=
				LT | LB;
		}
#endif
	} else if (pp->pacman.col > pp->pacman.nextcol) {
		p.cfactor = -1;
#if DEFUNCT
		if (pp->eaten) {
			pp->eaten[(pp->pacman.col * pp->nrows) + pp->pacman.row] |=
				LT | LB;
			pp->eaten[((pp->pacman.col - 1) * pp->nrows) + pp->pacman.row] |=
				RT | RB;
		}
#endif
	} else {
		p.cfactor = 0;
	}

	if (pp->pacman.nextrow > pp->pacman.row) {
		p.rfactor = 1;
#if DEFUNCT
		if (pp->eaten) {
			pp->eaten[(pp->pacman.col * pp->nrows) + pp->pacman.row] |=
				RB | LB;
			pp->eaten[(pp->pacman.col * pp->nrows) + (pp->pacman.row + 1)] |=
				RT | LT;
		}
#endif
	} else if (pp->pacman.row > pp->pacman.nextrow) {
		p.rfactor = -1;
#if DEFUNCT
		if (pp->eaten) {
			pp->eaten[(pp->pacman.col * pp->nrows) + pp->pacman.row] |=
				RT | LT;
			pp->eaten[(pp->pacman.col * pp->nrows) + (pp->pacman.row - 1)] |=
				RB | LB;
		}
#endif
	} else {
		p.rfactor = 0;
	}

	p.oldcf = pp->pacman.col * pp->xs + pp->xb;
	p.oldrf = pp->pacman.row * pp->ys + pp->yb;
	g = (being *) malloc(pp->nghosts * sizeof (being));

	for (ghost = 0; ghost < pp->nghosts; ghost++) {
		if (pp->ghosts[ghost].dead == 0) {
#if DEFUNCT
			pp->eaten[(pp->ghosts[ghost].col * pp->nrows) +
				  pp->ghosts[ghost].row] = NONE;
#endif
			if (pp->ghosts[ghost].nextcol > pp->ghosts[ghost].col) {
				g[ghost].cfactor = 1;
			} else if (pp->ghosts[ghost].col > pp->ghosts[ghost].nextcol) {
				g[ghost].cfactor = -1;
			} else {
				g[ghost].cfactor = 0;
			}
			if (pp->ghosts[ghost].nextrow > pp->ghosts[ghost].row) {
				g[ghost].rfactor = 1;
			} else if (pp->ghosts[ghost].row > pp->ghosts[ghost].nextrow) {
				g[ghost].rfactor = -1;
			} else {
				g[ghost].rfactor = 0;
			}

			g[ghost].oldcf = pp->ghosts[ghost].col * pp->xs +
				pp->xb;
			g[ghost].oldrf = pp->ghosts[ghost].row * pp->ys +
				pp->yb;
		}
	}
	for (delta.x = pp->incx, delta.y = pp->incy;
	     (delta.x < pp->xs + pp->incx) || (delta.y < pp->ys + pp->incy);
	     delta.x += pp->incx, delta.y += pp->incy) {
		if (delta.x > pp->xs)
			delta.x = pp->xs;
		if (delta.y > pp->ys)
			delta.y = pp->ys;
		p.cf = pp->pacman.col * pp->xs + delta.x * p.cfactor + pp->xb;
		p.rf = pp->pacman.row * pp->ys + delta.y * p.rfactor + pp->yb;

		dir = (ABS(p.cfactor) * (2 - p.cfactor) +
		       ABS(p.rfactor) * (1 + p.rfactor)) % 4;
		XSetForeground(display, pp->stippledGC, MI_BLACK_PIXEL(mi));
#ifdef FLASH
		XFillRectangle(display, window, pp->stippledGC,
			     p.oldcf, p.oldrf, pp->xs, pp->ys);
#else
		ERASE_IMAGE(display, window, pp->stippledGC, p.cf, p.rf,
			    p.oldcf, p.oldrf, pp->xs, pp->ys);
#endif
		XSetTSOrigin(display, pp->stippledGC, p.cf, p.rf);
		if (MI_NPIXELS(mi) > 2)
			XSetForeground(display, pp->stippledGC, MI_PIXEL(mi, YELLOW));
		else
			XSetForeground(display, pp->stippledGC, MI_WHITE_PIXEL(mi));

		XSetStipple(display, pp->stippledGC,
			    pp->pacmanPixmap[dir][pp->pacman.mouthstage]);
#ifdef FLASH
		XSetFillStyle(display, pp->stippledGC, FillStippled);
#else
		XSetFillStyle(display, pp->stippledGC, FillOpaqueStippled);
#endif
		XFillRectangle(display, window, pp->stippledGC,
			       p.cf, p.rf, pp->xs, pp->ys);
		pp->pacman.mouthstage += pp->pacman.mouthdirection;
		if ((pp->pacman.mouthstage >= MAXMOUTH) ||
		    (pp->pacman.mouthstage < 0)) {
			pp->pacman.mouthdirection *= -1;
			pp->pacman.mouthstage += pp->pacman.mouthdirection * 2;
		}
		p.oldcf = p.cf;
		p.oldrf = p.rf;

		for (ghost = 0; ghost < pp->nghosts; ghost++) {
			if (!pp->ghosts[ghost].dead) {
				g[ghost].cf = pp->ghosts[ghost].col * pp->xs +
					delta.x * g[ghost].cfactor + pp->xb;
				g[ghost].rf = pp->ghosts[ghost].row * pp->ys +
					delta.y * g[ghost].rfactor + pp->yb;
				XSetForeground(display, pp->stippledGC, MI_BLACK_PIXEL(mi));
#ifdef FLASH
				XFillRectangle(display, window, pp->stippledGC,
				    g[ghost].oldcf, g[ghost].oldrf, pp->xs, pp->ys);
#else
				ERASE_IMAGE(display, window, pp->stippledGC,
					g[ghost].cf, g[ghost].rf,
					g[ghost].oldcf, g[ghost].oldrf, pp->xs, pp->ys);
#endif
				XSetTSOrigin(display, pp->stippledGC,
					g[ghost].cf, g[ghost].rf);
				if (MI_NPIXELS(mi) > 2)
					XSetForeground(display, pp->stippledGC, MI_PIXEL(mi, BLUE));
				else
					XSetForeground(display, pp->stippledGC, MI_WHITE_PIXEL(mi));
				XSetStipple(display, pp->stippledGC, pp->ghostPixmap);

#ifdef FLASH
				XSetFillStyle(display, pp->stippledGC, FillStippled);
#else
				XSetFillStyle(display, pp->stippledGC, FillOpaqueStippled);
#endif
				XFillRectangle(display, window, pp->stippledGC,
			       g[ghost].cf, g[ghost].rf, pp->xs, pp->ys);
				XFlush(display);
				g[ghost].oldcf = g[ghost].cf;
				g[ghost].oldrf = g[ghost].rf;
			}
		}
		XFlush(display);
	}

#if 0
	clearcorners(mi);
#endif
	(void) free((void *) g);

	alldead = 1;
	for (ghost = 0; ghost < pp->nghosts; ghost++) {
		if (pp->ghosts[ghost].dead == 0) {
#if 0
			if ((pp->ghosts[ghost].nextrow >= pp->pacman.nextrow - 1) &&
			    (pp->ghosts[ghost].nextrow <= pp->pacman.nextrow + 1) &&
			    (pp->ghosts[ghost].nextcol >= pp->pacman.nextcol - 1) &&
			    (pp->ghosts[ghost].nextcol <= pp->pacman.nextcol + 1)) {
				(void) printf("%d %d\n", pp->ghosts[ghost].nextrow,
					      pp->ghosts[ghost].nextcol);
			}
#endif
			if (((pp->ghosts[ghost].nextrow == pp->pacman.nextrow) &&
			(pp->ghosts[ghost].nextcol == pp->pacman.nextcol)) ||
			    ((pp->ghosts[ghost].nextrow == pp->pacman.row) &&
			     (pp->ghosts[ghost].nextcol == pp->pacman.col) &&
			     (pp->ghosts[ghost].row == pp->pacman.nextrow) &&
			     (pp->ghosts[ghost].col == pp->pacman.nextcol))) {
				pp->ghosts[ghost].dead = 1;
				XSetForeground(display, pp->stippledGC, MI_BLACK_PIXEL(mi));
				/*XFillRectangle(display, window, gc,
				   pp->ghosts[ghost].col * pp->xs + pp->xb,
				   pp->ghosts[ghost].row * pp->ys + pp->yb,
				   pp->xs, pp->ys); */
				XFillRectangle(display, window, pp->stippledGC,
				 pp->ghosts[ghost].nextcol * pp->xs + pp->xb,
				 pp->ghosts[ghost].nextrow * pp->ys + pp->yb,
					       pp->xs, pp->ys);
			} else {
				pp->ghosts[ghost].row = pp->ghosts[ghost].nextrow;
				pp->ghosts[ghost].col = pp->ghosts[ghost].nextcol;
				alldead = 0;
			}
		}
	}
	pp->pacman.row = pp->pacman.nextrow;
	pp->pacman.col = pp->pacman.nextcol;

#if DEFUNCT
	if (alldead && pp->eaten) {
		for (ghost = 0; ghost < (pp->nrows * pp->ncols); ghost++)
			if (pp->eaten[ghost] != ALL)
				break;
		if (ghost == pp->nrows * pp->ncols)
			repopulate(mi);
	}
#else
	if (alldead) {
		repopulate(mi);
	}
#endif
}

void
init_pacman(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         size = MI_SIZE(mi);
	pacmangamestruct *pp;
	XGCValues   gcv;
	int         ghost, dir, mouth;
	GC          fg_gc, bg_gc;

	if (pacmangames == NULL) {
		if ((pacmangames = (pacmangamestruct *) calloc(MI_NUM_SCREENS(mi),
					 sizeof (pacmangamestruct))) == NULL)
			return;
	}
	pp = &pacmangames[MI_SCREEN(mi)];
	pp->width = MI_WIDTH(mi);
	pp->height = MI_HEIGHT(mi);
	if (pp->ghostPixmap != None) {
		XFreePixmap(display, pp->ghostPixmap);
		pp->ghostPixmap = None;
	    pp->graphics_format = IS_NONE;
	}
	if (size == 0 ||
		MINGRIDSIZE * size > pp->width ||
		MINGRIDSIZE * size > pp->height) {
	    int         ghostWidth, ghostHeight;

		ghostWidth = CELL_WIDTH;
		ghostHeight = CELL_HEIGHT;
		getPixmap(mi, window, CELL_WIDTH, CELL_HEIGHT, CELL_BITS,
			&ghostWidth, &ghostHeight, &(pp->ghostPixmap),
			&(pp->graphics_format));
		pp->xs = ghostWidth;
		pp->ys = ghostHeight;
		if (pp->width > MINGRIDSIZE * pp->xs &&
            pp->height > MINGRIDSIZE * pp->ys) {
            pp->pixelmode = False;
        } else {
			if (pp->ghostPixmap != None) {
				XFreePixmap(display, pp->ghostPixmap);
				pp->ghostPixmap = None;
			    pp->graphics_format = IS_NONE;
			}
            pp->pixelmode = True;
            pp->ys = MAX(MINSIZE, MIN(pp->width, pp->height) / MINGRIDSIZE);
            pp->xs = pp->ys;
        }
	} else {
		pp->pixelmode = True;
		if (size < -MINSIZE)
			pp->ys = NRAND(MIN(-size, MAX(MINSIZE, MIN(pp->width, pp->height) /
				      MINGRIDSIZE)) - MINSIZE + 1) + MINSIZE;
		else if (size < MINSIZE)
			pp->ys = MINSIZE;
		else
			pp->ys = MIN(size, MAX(MINSIZE, MIN(pp->width, pp->height) /
					       MINGRIDSIZE));
		pp->xs = pp->ys;
	}
	pp->incx = pp->xs / 10 + 1;
	pp->incy = pp->ys / 10 + 1;
	pp->ncols = MAX(pp->width / pp->xs, 2);
	pp->nrows = MAX(pp->height / pp->ys, 2);
	pp->xb = pp->width - pp->ncols * pp->xs;
	pp->yb = pp->height - pp->nrows * pp->ys;

	if (pp->pixelmode) {
        pp->ghostPixmap = XCreatePixmap(display, window,
            pp->xs, pp->ys, 1);
        gcv.foreground = 0;
        gcv.background = 1;
        bg_gc = XCreateGC(display, pp->ghostPixmap,
			GCForeground | GCBackground, &gcv);
        gcv.foreground = 1;
        gcv.background = 0;
        fg_gc = XCreateGC(display, pp->ghostPixmap,
			GCForeground | GCBackground, &gcv);
        XFillRectangle(display, pp->ghostPixmap, bg_gc,
            0, 0, pp->xs, pp->ys);
		XFillArc(display, pp->ghostPixmap, fg_gc,
			0, 0, pp->xs, pp->ys, 0, 11520);
		XFillRectangle(display, pp->ghostPixmap, fg_gc,
			0, pp->ys / 2, pp->xs, pp->ys / 2);
        XFreeGC(display, bg_gc);
        XFreeGC(display, fg_gc);
    }
	if (!pp->stippledGC) {
		gcv.foreground = MI_BLACK_PIXEL(mi);
		gcv.background = MI_BLACK_PIXEL(mi);
		if ((pp->stippledGC = XCreateGC(display, window,
				GCForeground | GCBackground, &gcv)) == None)
			return;
	}
	if (pp->pacmanPixmap[0][0] != None)
		for (dir = 0; dir < 4; dir++)
			for (mouth = 0; mouth < MAXMOUTH; mouth++)
				XFreePixmap(display, pp->pacmanPixmap[dir][mouth]);

	for (dir = 0; dir < 4; dir++)
		for (mouth = 0; mouth < MAXMOUTH; mouth++) {
			pp->pacmanPixmap[dir][mouth] = XCreatePixmap(display, MI_WINDOW(mi),
							  pp->xs, pp->ys, 1);
			gcv.foreground = 1;
        	gcv.background = 0;
			fg_gc = XCreateGC(display, pp->pacmanPixmap[dir][mouth],
				GCForeground | GCBackground, &gcv);
			gcv.foreground = 0;
        	gcv.background = 0;
			bg_gc = XCreateGC(display, pp->pacmanPixmap[dir][mouth],
				GCForeground | GCBackground, &gcv);
			XFillRectangle(display, pp->pacmanPixmap[dir][mouth], bg_gc,
				0, 0, pp->xs, pp->ys);
			if (pp->xs == 1 && pp->ys == 1)
				XFillRectangle(display, pp->pacmanPixmap[dir][mouth], fg_gc,
					0, 0, pp->xs, pp->ys);
			else
				XFillArc(display, pp->pacmanPixmap[dir][mouth], fg_gc,
					0, 0, pp->xs, pp->ys,
					((90 - dir * 90) + mouth * 5) * 64,
					(360 + (-2 * mouth * 5)) * 64);
			XFreeGC(display, fg_gc);
			XFreeGC(display, bg_gc);
		}
	pp->pacman.lastbox = -1;
	pp->pacman.mouthdirection = 1;

	pp->nghosts = MI_COUNT(mi);
	if (pp->nghosts < -MINGHOSTS) {
		/* if pp->nghosts is random ... the size can change */
		if (pp->ghosts != NULL) {
			(void) free((void *) pp->ghosts);
			pp->ghosts = NULL;
		}
		pp->nghosts = NRAND(-pp->nghosts - MINGHOSTS + 1) + MINGHOSTS;
	} else if (pp->nghosts < MINGHOSTS)
		pp->nghosts = MINGHOSTS;

	if (!pp->ghosts)
		pp->ghosts = (beingstruct *) calloc(pp->nghosts, sizeof (beingstruct));
	for (ghost = 0; ghost < pp->nghosts; ghost++)
		pp->ghosts[ghost].nextbox = NOWHERE;

	pp->pacman.row = NRAND(pp->nrows);
	pp->pacman.col = NRAND(pp->ncols);

	MI_CLEARWINDOW(mi);

	pp->pacman.mouthstage = MAXMOUTH - 1;
	for (ghost = 0; ghost < pp->nghosts; ghost++) {
		do {
			pp->ghosts[ghost].col = NRAND(pp->ncols);
			pp->ghosts[ghost].row = NRAND(pp->nrows);
		}
		while ((pp->ghosts[ghost].col == pp->pacman.col) &&
		       (pp->ghosts[ghost].row == pp->pacman.row));
		pp->ghosts[ghost].dead = 0;
		pp->ghosts[ghost].lastbox = -1;

	}
#if DEFUNCT
	if (pp->eaten)
		(void) free((void *) pp->eaten);
	pp->eaten = (unsigned int *) malloc((pp->nrows * pp->ncols) *
					    sizeof (unsigned int));

	if (pp->eaten)
		for (ghost = 0; ghost < (pp->nrows * pp->ncols); ghost++)
			pp->eaten[ghost] = NONE;
#endif
}

void
draw_pacman(ModeInfo * mi)
{
	pacmangamestruct *pp = &pacmangames[MI_SCREEN(mi)];
	int         g;

	MI_IS_DRAWN(mi) = True;

	do {
		if (NRAND(3) == 2)
			pp->pacman.nextbox = NRAND(5);

		switch (pp->pacman.nextbox) {
			case 0:
				if ((pp->pacman.row == 0) || (pp->pacman.lastbox == 2))
					pp->pacman.nextbox = NOWHERE;
				else {
					pp->pacman.nextrow = pp->pacman.row - 1;
					pp->pacman.nextcol = pp->pacman.col;
				}
				break;

			case 1:
				if ((pp->pacman.col == pp->ncols - 1) ||
				    (pp->pacman.lastbox == 3))
					pp->pacman.nextbox = NOWHERE;
				else {
					pp->pacman.nextrow = pp->pacman.row;
					pp->pacman.nextcol = pp->pacman.col + 1;
				}
				break;

			case 2:
				if ((pp->pacman.row == pp->nrows - 1) ||
				    (pp->pacman.lastbox == 0))
					pp->pacman.nextbox = NOWHERE;
				else {
					pp->pacman.nextrow = pp->pacman.row + 1;
					pp->pacman.nextcol = pp->pacman.col;
				}
				break;

			case 3:
				if ((pp->pacman.col == 0) || (pp->pacman.lastbox == 1))
					pp->pacman.nextbox = NOWHERE;
				else {
					pp->pacman.nextrow = pp->pacman.row;
					pp->pacman.nextcol = pp->pacman.col - 1;
				}
				break;

			default:
				pp->pacman.nextbox = NOWHERE;
				break;
		}
	}
	while (pp->pacman.nextbox == NOWHERE);


	for (g = 0; g < pp->nghosts; g++) {
		if (pp->ghosts[g].dead == 0) {
			do {
				if (NRAND(3) == 2)
					pp->ghosts[g].nextbox = NRAND(5);

				switch (pp->ghosts[g].nextbox) {
					case 0:
						if ((pp->ghosts[g].row == 0) || (pp->ghosts[g].lastbox == 2))
							pp->ghosts[g].nextbox = NOWHERE;
						else {
							pp->ghosts[g].nextrow = pp->ghosts[g].row - 1;
							pp->ghosts[g].nextcol = pp->ghosts[g].col;
						}
						break;

					case 1:
						if ((pp->ghosts[g].col == pp->ncols - 1) ||
						(pp->ghosts[g].lastbox == 3))
							pp->ghosts[g].nextbox = NOWHERE;
						else {
							pp->ghosts[g].nextrow = pp->ghosts[g].row;
							pp->ghosts[g].nextcol = pp->ghosts[g].col + 1;
						}
						break;

					case 2:
						if ((pp->ghosts[g].row == pp->nrows - 1) ||
						(pp->ghosts[g].lastbox == 0))
							pp->ghosts[g].nextbox = NOWHERE;
						else {
							pp->ghosts[g].nextrow = pp->ghosts[g].row + 1;
							pp->ghosts[g].nextcol = pp->ghosts[g].col;
						}
						break;

					case 3:
						if ((pp->ghosts[g].col == 0) || (pp->ghosts[g].lastbox == 1))
							pp->ghosts[g].nextbox = NOWHERE;
						else {
							pp->ghosts[g].nextrow = pp->ghosts[g].row;
							pp->ghosts[g].nextcol = pp->ghosts[g].col - 1;
						}
						break;

					default:
						pp->ghosts[g].nextbox = NOWHERE;
						break;
				}
			}
			while (pp->ghosts[g].nextbox == NOWHERE);
			pp->ghosts[g].lastbox = pp->ghosts[g].nextbox;
		}
	}
	if (pp->pacman.lastbox != pp->pacman.nextbox)
		pp->pacman.mouthstage = 0;
	pp->pacman.lastbox = pp->pacman.nextbox;
	movepac(mi);

}

void
release_pacman(ModeInfo * mi)
{
	if (pacmangames != NULL) {
		int         screen, dir, mouth;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			pacmangamestruct *pp = &pacmangames[screen];
			Display    *display = MI_DISPLAY(mi);

			if (pp->ghosts != NULL)
				(void) free((void *) pp->ghosts);
#ifdef DEFUNCT
			if (pp->eaten != NULL)
				(void) free((void *) pp->eaten);
#endif
			if (pp->stippledGC != NULL) {
				XFreeGC(display, pp->stippledGC);
			}
			if (pp->ghostPixmap != None)
				XFreePixmap(display, pp->ghostPixmap);
			if (pp->pacmanPixmap[0][0] != None)
				for (dir = 0; dir < 4; dir++)
					for (mouth = 0; mouth < MAXMOUTH; mouth++)
						XFreePixmap(display, pp->pacmanPixmap[dir][mouth]);
		}
		(void) free((void *) pacmangames);
		pacmangames = NULL;
	}
}

void
refresh_pacman(ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
}

#endif /* MODE_pacman */
