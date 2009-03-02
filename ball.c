
/* -*- Mode: C; tab-width: 4 -*- */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)ball.c	4.03 97/05/10 xlockmore";

#endif

/*-
 * ball.c - bouncing balls for xlock the X Window System lockscreen.
 *
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
 * 10-May-97: Compatible with xscreensaver
 */

/*-
 * Bouncing balls kind of mode.
 * Random drawing functions for the balls and the trail
 * left by the balls.  This one looks pretty cool
 * if good functions are chosen.
 */

#ifdef STANDALONE
#define PROGCLASS            "Ball"
#define HACK_INIT            init_ball
#define HACK_DRAW            draw_ball
#define DEF_DELAY            10000
#define DEF_BATCHCOUNT       10
#define DEF_CYCLES           20
#define DEF_SIZE             -100
#define DEF_NCOLORS          200
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
ModeSpecOpt ball_opts =
{0, NULL, 0, NULL, NULL};

#endif /* STANDALONE */

#define NONE	0		/* not in a window */
#define V	1		/* vertical */
#define H	2		/* horizontal */
#define B	3		/* ball */

#define MINBALLS 1
#define MINSIZE 2
#define MINGRIDSIZE 4

#define DEFNO	6
#define SPEED	156
#define SQLIMIT	(SPEED*SPEED/(30*30))	/* square of lower speed limit */
#define RATE    600

typedef struct {
	int         x, y;	/* x and y coords */
	int         dx, dy;	/* x and y velocity */
	int         rad;
	int         bounce;
	int         dyold;
	int         started;
	int         def;
	GC          GcF, GcB;
} balltype;

typedef struct {
	int         init;
	balltype   *bt;
	int         rad;
	int         size;
	int         width, height;
	int         bounce;
	int         nballs;
	int         dispx, dispy;
} ballstruct;

static ballstruct *balls = NULL;

static void
collided(ModeInfo * mi, int i, int n, int *dx, int *dy, int td)
{
	ballstruct *bp = &balls[MI_SCREEN(mi)];
	balltype   *bti = &bp->bt[i];
	balltype   *btn = &bp->bt[n];
	int         rx1, ry1, rx2, ry2;
	int         Vx1, Vy1, Vx2, Vy2;
	int         NVx1, NVy1, NVx2, NVy2;

	float       Ux1, Uy1, Ux2, Uy2;
	float       mag1, mag2, imp;

	rx1 = bti->x;
	ry1 = bti->y;
	Vx1 = bti->dx;
	Vy1 = bti->dy;

	rx2 = btn->x;
	ry2 = btn->y;
	Vx2 = btn->dx;
	Vy2 = btn->dy;

	Ux1 = rx1 - rx2;
	Uy1 = ry1 - ry2;
	mag1 = sqrt(((Ux1 * Ux1) + (Uy1 * Uy1)));
	Ux1 = Ux1 / mag1;
	Uy1 = Uy1 / mag1;

	Ux2 = rx2 - rx1;
	Uy2 = ry2 - ry1;
	mag2 = sqrt(((Ux2 * Ux2) + (Uy2 * Uy2)));
	Ux2 = Ux2 / mag2;
	Uy2 = Uy2 / mag2;

	imp = ((Vx1 * Ux2) + (Vy1 * Uy2)) + ((Vx2 * Ux1) + (Vy2 * Uy1));

	NVx1 = Vx1 + (int) (imp * Ux1);
	NVy1 = Vy1 + (int) (imp * Uy1);

	NVx2 = Vx2 + (int) (imp * Ux2);
	NVy2 = Vy2 + (int) (imp * Uy2);

	bti->dx = NVx1;
	bti->dy = NVy1;

	btn->dx = NVx2;
	btn->dy = NVy2;

	XFillArc(MI_DISPLAY(mi), MI_WINDOW(mi), btn->GcB,
		 btn->x - (btn->rad / 2), btn->y - (btn->rad / 2),
		 btn->rad, btn->rad, 0, 360 * 64);
	if (bp->dispx > 100) {
		*dx = (td * btn->dx) / RATE;
		*dy = (td * btn->dy) / RATE;
	} else {
		*dx = (td * btn->dx) / 150;
		*dy = (td * btn->dy) / 150;
	}

	btn->x += (*dx / 2);
	btn->y += (*dy / 2);
	XFillArc(MI_DISPLAY(mi), MI_WINDOW(mi), btn->GcF,
		 btn->x - (btn->rad / 2), btn->y - (btn->rad / 2),
		 btn->rad, btn->rad, 0, 360 * 64);

	if (bp->dispx > 100) {
		*dx = (td * bti->dx) / RATE;
		*dy = (td * bti->dy) / RATE;
	} else {
		*dx = (td * bti->dx) / 150;
		*dy = (td * bti->dy) / 150;
	}

	bti->x += (*dx / 2);
	bti->y += (*dy / 2);
}

static int
inwin(ballstruct * bp, int x, int y, int *n, int rad)
{
	int         i, diffx, diffy;

	if ((x < 0) || (x > bp->dispx)) {
		return (V);
	}
	if ((y < 0) || (y > bp->dispy)) {
		return (H);
	}
	if (bp->dispx > 100) {
		for (i = 0; i < bp->nballs; i++) {
			if ((i == (*n)) || (!bp->bt[i].def))
				continue;
			diffx = (bp->bt[i].x - x);
			diffy = (bp->bt[i].y - y);
			if ((diffx * diffx + diffy * diffy) <
			    (((rad / 2) * (rad / 2) * 2) +
			  ((bp->bt[i].rad / 2) * (bp->bt[i].rad / 2) * 2))) {
				(*n) = i;
				return (B);
			}
		}
	}
	return (NONE);
}

static void
randomball(ModeInfo * mi, int i)
{
	ballstruct *bp = &balls[MI_SCREEN(mi)];
	balltype   *bti = &bp->bt[i];
	Display    *display = MI_DISPLAY(mi);
	int         x, y, bn;
	int         dum;
	int         attempts;
	unsigned long randbg = 1;

	attempts = 0;
	if (bp->bounce == -2)
		bn = 30 + NRAND(69L);
	else
		bn = bp->bounce;

	if (bn > 100)
		bn = 100;
	if (bn < 0)
		bn = 0;
	bn = (0 - bn);

	if (bp->dispx > 100) {
		bti->dx = NRAND(2L * SPEED) + SPEED;
		bti->dy = NRAND(2L * SPEED) + (SPEED / 2);
	} else {
		bti->dx = NRAND(4L * SPEED) + (SPEED / 20);
		bti->dy = NRAND(2L * SPEED) + (SPEED / 40);
	}

	switch (NRAND(9L) % 2) {
		case 0:
			break;
		case 1:
			bti->dx = (0 - bti->dx);
			break;
	}

	bti->bounce = bn;
	bti->dyold = 0;
	bti->rad = bp->rad;	/* Pretty lame... should be different sizes */

	do {
		dum = i;
		x = NRAND((long) bp->dispx);
		y = 0;
		attempts++;
		if (attempts > 5) {
			bti->def = 0;
			return;
		}
	}

	while ((inwin(bp, x, y, &dum, bti->rad) != NONE) ||
	       (inwin(bp, bti->dx + x, bti->dy + y, &dum, bti->rad) != NONE));

	bti->def = 1;
	bti->x = x;
	bti->y = y;

	/* set background color for ball */

	if (MI_NPIXELS(mi) > 2) {
		randbg = MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));
	} else {
		randbg = (unsigned long) MI_WIN_BLACK_PIXEL(mi);
	}
	XSetForeground(display, bti->GcB, randbg);

	/* set foreground color for ball */

	if (MI_NPIXELS(mi) > 2) {
		randbg = MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));
	} else {
		randbg = (unsigned long) MI_WIN_WHITE_PIXEL(mi);
	}
	XSetForeground(display, bti->GcF, randbg);

	XFillArc(display, MI_WINDOW(mi),
		 bti->GcB, bti->x - (bti->rad / 2), bti->y - (bti->rad / 2),
		 bti->rad, bti->rad, 0, 360 * 64);
}

void
init_ball(ModeInfo * mi)
{
	ballstruct *bp;
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         GcLp, i;
	int         size = MI_SIZE(mi);

	if (balls == NULL) {
		if ((balls = (ballstruct *) calloc(MI_NUM_SCREENS(mi),
					       sizeof (ballstruct))) == NULL)
			return;
	}
	bp = &balls[MI_SCREEN(mi)];

	bp->bounce = 85;
	bp->width = MI_WIN_WIDTH(mi);
	bp->height = MI_WIN_HEIGHT(mi);

	bp->nballs = MI_BATCHCOUNT(mi);
	if (bp->nballs < -MINBALLS) {
		/* if bp->nballs is random ... the size can change */
		if (bp->bt != NULL) {
			(void) free((void *) bp->bt);
			bp->bt = NULL;
		}
		bp->nballs = NRAND(-bp->nballs - MINBALLS + 1) + MINBALLS;
	} else if (bp->nballs < MINBALLS)
		bp->nballs = MINBALLS;
	if (bp->bt == NULL) {
		bp->bt = (balltype *) calloc(bp->nballs, sizeof (balltype));
	}
	if (size == 0 ||
	 MINGRIDSIZE * size > bp->width || MINGRIDSIZE * size > bp->height) {
		bp->rad = MAX(MINSIZE, MIN(bp->width, bp->height) / MINGRIDSIZE);
	} else {
		if (size < -MINSIZE)
			bp->rad = NRAND(MIN(-size, MAX(MINSIZE, MIN(bp->width, bp->height) /
				      MINGRIDSIZE)) - MINSIZE + 1) + MINSIZE;
		else if (size < MINSIZE)
			bp->rad = MINSIZE;
		else
			bp->rad = MIN(size, MAX(MINSIZE, MIN(bp->width, bp->height) /
						MINGRIDSIZE));
	}

	/* clearballs */
	XClearWindow(display, window);
	XFlush(display);
	if (bp->nballs <= 0)
		bp->nballs = 1;
	if (!bp->init) {
		for (GcLp = 0; GcLp < bp->nballs; GcLp++) {
			bp->bt[GcLp].GcB = XCreateGC(display, window, 0L, (XGCValues *) 0);
			bp->bt[GcLp].GcF = XCreateGC(display, window, 0L, (XGCValues *) 0);
		}
		bp->init = 1;
	}
	for (GcLp = 0; GcLp < bp->nballs; GcLp++) {
		XSetFunction(display, bp->bt[GcLp].GcB, NRAND(16L));
		XSetFunction(display, bp->bt[GcLp].GcF, NRAND(16L));
	}

	bp->dispx = MI_WIN_WIDTH(mi);
	bp->dispy = MI_WIN_HEIGHT(mi);

	XFlush(display);
	for (i = 0; i < bp->nballs; i++) {
		randomball(mi, i);
	}
}

void
draw_ball(ModeInfo * mi)
{
	ballstruct *bp = &balls[MI_SCREEN(mi)];
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         i, n;
	int         td;
	int         dx, dy;
	int         redo;

	td = 10;

	for (i = 0; i < bp->nballs; i++) {
		if (!bp->bt[i].def)
			randomball(mi, i);
	}

	for (i = 0; i < bp->nballs; i++) {
		if (!bp->bt[i].def) {
			continue;
		}
		XFillArc(display, window, bp->bt[i].GcB,
			 bp->bt[i].x - (bp->bt[i].rad / 2), bp->bt[i].y - (bp->bt[i].rad / 2),
			 bp->bt[i].rad, bp->bt[i].rad, 0, 360 * 64);
		redo = 0;
		if (((bp->bt[i].dx * bp->bt[i].dx + bp->bt[i].dy * bp->bt[i].dy) <
		     SQLIMIT) && (bp->bt[i].y >= (bp->dispy - 3))) {
			redo = 25;
		}
		do {
			if (bp->dispx > 100) {
				dx = (td * bp->bt[i].dx) / RATE;
				dy = (td * bp->bt[i].dy) / RATE;
			} else {
				dx = (td * bp->bt[i].dx) / 150;
				dy = (td * bp->bt[i].dy) / 150;
			}

			if (redo > 5) {
				redo = 0;
				randomball(mi, i);
				if (!bp->bt[i].def)
					continue;
				XFillArc(display, window, bp->bt[i].GcF,
					 bp->bt[i].x - (bp->bt[i].rad / 2),
					 bp->bt[i].y - (bp->bt[i].rad / 2),
				  bp->bt[i].rad, bp->bt[i].rad, 0, 360 * 64);
			}
			n = i;
			switch (inwin(bp, dx + bp->bt[i].x, dy + bp->bt[i].y, &n,
				      bp->bt[i].rad)) {
				case NONE:
					bp->bt[i].x += dx;
					bp->bt[i].y += dy;
					redo = 0;
					break;
				case V:
					bp->bt[i].dx = (int) (((float) bp->bt[i].bounce *
					(float) bp->bt[i].dx) / (float) 100);
					redo++;
					break;
				case H:
					bp->bt[i].dy = (int) (((float) bp->bt[i].bounce *
					(float) bp->bt[i].dy) / (float) 100);
					if (bp->bt[i].bounce != 100) {
						if ((bp->bt[i].y >= (bp->dispy - 3)) && (bp->bt[i].dy > -250) &&
						    (bp->bt[i].dy < 0)) {
							redo = 15;
						}
						if ((bp->bt[i].y >= (bp->dispy - 3)) &&
						    (bp->bt[i].dy == bp->bt[i].dyold)) {
							redo = 10;
						}
						bp->bt[i].dyold = bp->bt[i].dy;
					}
					redo++;
					break;
				case B:
					if (redo > 5) {
						if (bp->bt[i].y >= (bp->dispy - 3)) {
							randomball(mi, i);
							redo = 0;
						} else if (bp->bt[n].y >= (bp->dispy - 3)) {
							randomball(mi, n);
							redo = 0;
						} else
							redo = 0;
					} else {
						collided(mi, i, n, &dx, &dy, td);
						redo = 0;
					}
					break;
			}
		}
		while (redo);
		bp->bt[i].dy += td;

		if (bp->bt[i].def)
			XFillArc(display, window, bp->bt[i].GcF,
				 bp->bt[i].x - (bp->bt[i].rad / 2), bp->bt[i].y - (bp->bt[i].rad / 2),
				 bp->bt[i].rad, bp->bt[i].rad, 0, 360 * 64);
	}

	XFlush(display);
}

void
release_ball(ModeInfo * mi)
{
	if (balls != NULL) {
		int         screen, i;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			ballstruct *bp = &balls[screen];

			if (bp->init)
				for (i = 0; i < bp->nballs; i++) {
					XFreeGC(MI_DISPLAY(mi), bp->bt[i].GcF);
					XFreeGC(MI_DISPLAY(mi), bp->bt[i].GcB);
				}
			if (bp->bt != NULL)
				(void) free((void *) bp->bt);

		}
		(void) free((void *) balls);
		balls = NULL;
	}
}

void
refresh_ball(ModeInfo * mi)
{
	XClearWindow(MI_DISPLAY(mi), MI_WINDOW(mi));
}
