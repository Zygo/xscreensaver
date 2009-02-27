/* -*- Mode: C; tab-width: 4 -*-
 * braid --- draws random color-cyling rotating braids around a circle.
 */
#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)braid.c	4.00 97/01/01 xlockmore";
#endif

/*
 * Copyright (c) 1995 by John Neil.
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
 * 10-May-97: jwz@netscape.com: turned into a standalone program.
 * 01-Sep-95: color knotted components differently, J. Neil.
 * 29-Aug-95: Written.  John Neil <neil@math.idbsu.edu>
 */

#ifdef STANDALONE
# define PROGCLASS					"Braid"
# define HACK_INIT					init_braid
# define HACK_DRAW					draw_braid
# define braid_opts					xlockmore_opts
# define DEFAULTS	"*count:		15    \n"			\
					"*cycles:		100   \n"			\
					"*delay:		1000  \n"			\
					"*ncolors:		64    \n"
# define UNIFORM_COLORS
# include "xlockmore.h"				/* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"					/* from the xlockmore distribution */
#endif /* !STANDALONE */

ModeSpecOpt braid_opts = {
  0, NULL, 0, NULL, NULL };


#if defined( COLORROUND ) && defined( COLORCOMP )
#undef COLORROUND
#undef COLORCOMP
#endif

#if !defined( COLORROUND ) && !defined( COLORCOMP )
#if 0
/* to color in a circular pattern use COLORROUND */
#define COLORROUND
#else
/* to color by component use COLORCOMP */
#define COLORCOMP
#endif
#endif

#define MAXLENGTH  50		/* the maximum length of a braid word */
#define MINLENGTH  8		/* the minimum length of a braid word */
#define MAXSTRANDS  15		/* the maximum number of strands in the braid */
#define MINSTRANDS  3		/* the minimum number of strands in the braid */
#define SPINRATE  12.0		/* the rate at which the colors spin */

#define INTRAND(min,max) (NRAND((max+1)-(min))+(min))
#define FLOATRAND(min,max) ((min)+((double) LRAND()/((double) MAXRAND))*((max)-(min)))

typedef struct {
	int         braidword[MAXLENGTH];
	int         components[MAXSTRANDS];
	int         startcomp[MAXLENGTH][MAXSTRANDS];
	int         nstrands;
	int         braidlength;
	float       startcolor;
	int         center_x;
	int         center_y;
	float       min_radius;
	float       max_radius;
	float       top, bottom, left, right;
	int         age;
    int         color_direction;
} braidtype;

static braidtype *braids = NULL;

static int
applyword(braidtype * braid, int string, int position)
{
	int         i, c;

	c = string;
	for (i = position; i < braid->braidlength; i++) {
		if (c == ABS(braid->braidword[i]))
			c--;
		else if (c == ABS(braid->braidword[i]) - 1)
			c++;
	}
	for (i = 0; i < position; i++) {
		if (c == ABS(braid->braidword[i]))
			c--;
		else if (c == ABS(braid->braidword[i]) - 1)
			c++;
	}
	return c;
}

#if 0
static int
applywordto(braidtype * braid, int string, int position)
{
	int         i, c;

	c = string;
	for (i = 0; i < position; i++) {
		if (c == ABS(braid->braidword[i])) {
			c--;
		} else if (c == ABS(braid->braidword[i]) - 1) {
			c++;
		}
	}
	return c;
}
#endif

static int
applywordbackto(braidtype * braid, int string, int position)
{
	int         i, c;

	c = string;
	for (i = position - 1; i >= 0; i--) {
		if (c == ABS(braid->braidword[i])) {
			c--;
		} else if (c == ABS(braid->braidword[i]) - 1) {
			c++;
		}
	}
	return c;
}

void
init_braid(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	braidtype  *braid;
	int         used[MAXSTRANDS];
	int         i, count, comp, c;
	float       min_length;

	if (braids == NULL) {
		if ((braids = (braidtype *) calloc(MI_NUM_SCREENS(mi),
						sizeof (braidtype))) == NULL)
			return;
	}
	braid = &braids[MI_SCREEN(mi)];

	braid->center_x = MI_WIN_WIDTH(mi) / 2;
	braid->center_y = MI_WIN_HEIGHT(mi) / 2;
	braid->age = 0;

	/* jwz: go in the other direction sometimes. */
	braid->color_direction = ((LRAND() & 1) ? 1 : -1);

	XClearWindow(display, MI_WINDOW(mi));

	min_length = (braid->center_x > braid->center_y) ?
		braid->center_y : braid->center_x;
	braid->min_radius = min_length * 0.30;
	braid->max_radius = min_length * 0.90;

	if (MI_BATCHCOUNT(mi) < MINSTRANDS)
		braid->nstrands = MINSTRANDS;
	else
		braid->nstrands = INTRAND(MINSTRANDS,
				  MAX(MIN(MIN(MAXSTRANDS, MI_BATCHCOUNT(mi)),
					  (int) ((braid->max_radius - braid->min_radius) / 5.0)), MINSTRANDS));
	braid->braidlength = INTRAND(MINLENGTH, MIN(MAXLENGTH, braid->nstrands * 6));

	for (i = 0; i < braid->braidlength; i++) {
		braid->braidword[i] =
			INTRAND(1, braid->nstrands - 1) * (INTRAND(1, 2) * 2 - 3);
		if (i > 0)
			while (braid->braidword[i] == -braid->braidword[i - 1])
				braid->braidword[i] = INTRAND(1, braid->nstrands - 1) * (INTRAND(1, 2) * 2 - 3);
	}

	while (braid->braidword[0] == -braid->braidword[braid->braidlength - 1])
		braid->braidword[braid->braidlength - 1] =
			INTRAND(1, braid->nstrands - 1) * (INTRAND(1, 2) * 2 - 3);

	do {
		(void) memset((char *) used, 0, sizeof (used));
		count = 0;
		for (i = 0; i < braid->braidlength; i++)
			used[ABS(braid->braidword[i])]++;
		for (i = 0; i < braid->nstrands; i++)
			count += (used[i] > 0) ? 1 : 0;
		if (count < braid->nstrands - 1) {
			braid->braidword[braid->braidlength] =
				INTRAND(1, braid->nstrands - 1) * (INTRAND(1, 2) * 2 - 3);
			while (braid->braidword[braid->braidlength] ==
			       -braid->braidword[braid->braidlength - 1] &&
			       braid->braidword[0] == -braid->braidword[braid->braidlength])
				braid->braidword[braid->braidlength] =
					INTRAND(1, braid->nstrands - 1) * (INTRAND(1, 2) * 2 - 3);
			braid->braidlength++;
		}
	} while (count < braid->nstrands - 1 && braid->braidlength < MAXLENGTH);

	braid->startcolor = (MI_NPIXELS(mi) > 2) ?
		(float) NRAND(MI_NPIXELS(mi)) : 0.0;
	/* XSetLineAttributes (display, MI_GC(mi), 2, LineSolid, CapRound, 
	   JoinRound); */

	(void) memset((char *) braid->components, 0, sizeof (braid->components));
	c = 1;
	comp = 0;
	braid->components[0] = 1;
	do {
		i = comp;
		do {
			i = applyword(braid, i, 0);
			braid->components[i] = braid->components[comp];
		} while (i != comp);
		count = 0;
		for (i = 0; i < braid->nstrands; i++)
			if (braid->components[i] == 0)
				count++;
		if (count > 0) {
			for (comp = 0; braid->components[comp] != 0; comp++);
			braid->components[comp] = ++c;
		}
	} while (count > 0);

	for (i = 0; i < braid->nstrands; i++)
		if (!(braid->components[i] & 1))
			braid->components[i] *= -1;
}

void
draw_braid(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	braidtype  *braid = &braids[MI_SCREEN(mi)];
	float       num_points, t_inc;
	float       theta, psi;
	float       t, r_diff;
	int         i, s;
	float       x_1, y_1, x_2, y_2, r1, r2;
	float       color, color_use = 0.0, color_inc;

	num_points = 500.0;
	theta = (2.0 * M_PI) / (float) (braid->braidlength);
	t_inc = (2.0 * M_PI) / num_points;
	color_inc = (float) MI_NPIXELS(mi) / num_points;
	color_inc *= braid->color_direction;

	braid->startcolor += SPINRATE * color_inc;
	if (braid->startcolor >= MI_NPIXELS(mi))
		braid->startcolor = 0.0;

	r_diff = (braid->max_radius - braid->min_radius) / (float) (braid->nstrands);

	color = braid->startcolor;
	psi = 0.0;
	for (i = 0; i < braid->braidlength; i++) {
		psi += theta;
		for (t = 0.0; t < theta; t += t_inc) {
#ifdef COLORROUND
			color += color_inc;
			if (color >= (float) (MI_NPIXELS(mi)))
				color = 0.0;
			color_use = color;
#endif
			for (s = 0; s < braid->nstrands; s++) {
				if (ABS(braid->braidword[i]) == s)
					continue;
				if (ABS(braid->braidword[i]) - 1 == s) {
					/* crosSINFg */
#ifdef COLORCOMP
					if (MI_NPIXELS(mi) > 2) {
						color_use = color + SPINRATE *
							braid->components[applywordbackto(braid, s, i)] +
							(psi + t) / 2.0 / M_PI * (float) MI_NPIXELS(mi);
						while (color_use >= (float) MI_NPIXELS(mi))
							color_use -= (float) MI_NPIXELS(mi);
						while (color_use < 0.0)
							color_use += (float) MI_NPIXELS(mi);
					}
#endif
#ifdef COLORROUND
					if (MI_NPIXELS(mi) > 2) {
						color_use += SPINRATE * color_inc;
						while (color_use >= (float) (MI_NPIXELS(mi)))
							color_use -= (float) MI_NPIXELS(mi);
					}
#endif
					r1 = braid->min_radius + r_diff * (float) (s);
					r2 = braid->min_radius + r_diff * (float) (s + 1);
					if (braid->braidword[i] > 0 ||
					    (FABSF(t - theta / 2.0) > theta / 7.0)) {
						x_1 = ((0.5 * (1.0 + SINF(t / theta * M_PI - M_PI_2)) * r2 +
							0.5 * (1.0 + SINF((theta - t) / theta * M_PI - M_PI_2)) * r1)) *
							COSF(t + psi) + braid->center_x;
						y_1 = ((0.5 * (1.0 + SINF(t / theta * M_PI - M_PI_2)) * r2 +
							0.5 * (1.0 + SINF((theta - t) / theta * M_PI - M_PI_2)) * r1)) *
							SINF(t + psi) + braid->center_y;
						x_2 = ((0.5 * (1.0 + SINF((t + t_inc) / theta * M_PI - M_PI_2)) * r2 +
							0.5 * (1.0 + SINF((theta - t - t_inc) / theta * M_PI - M_PI_2)) * r1)) *
							COSF(t + t_inc + psi) + braid->center_x;
						y_2 = ((0.5 * (1.0 + SINF((t + t_inc) / theta * M_PI - M_PI_2)) * r2 +
							0.5 * (1.0 + SINF((theta - t - t_inc) / theta * M_PI - M_PI_2)) * r1)) *
							SINF(t + t_inc + psi) + braid->center_y;
						if (MI_NPIXELS(mi) > 2)
							XSetForeground(display, MI_GC(mi), MI_PIXEL(mi, (int) color_use));
						else
							XSetForeground(display, MI_GC(mi), MI_WIN_WHITE_PIXEL(mi));

						XDrawLine(display, window, MI_GC(mi),
							  (int) (x_1), (int) (y_1), (int) (x_2), (int) (y_2));
					}
#ifdef COLORCOMP
					if (MI_NPIXELS(mi) > 2) {
						color_use = color + SPINRATE *
							braid->components[applywordbackto(braid, s + 1, i)] +
							(psi + t) / 2.0 / M_PI * (float) MI_NPIXELS(mi);
						while (color_use >= (float) MI_NPIXELS(mi))
							color_use -= (float) MI_NPIXELS(mi);
						while (color_use < 0.0)
							color_use += (float) MI_NPIXELS(mi);
					}
#endif
					if (braid->braidword[i] < 0 ||
					    (FABSF(t - theta / 2.0) > theta / 7.0)) {
						x_1 = ((0.5 * (1.0 + SINF(t / theta * M_PI - M_PI_2)) * r1 +
							0.5 * (1.0 + SINF((theta - t) / theta * M_PI - M_PI_2)) * r2)) *
							COSF(t + psi) + braid->center_x;
						y_1 = ((0.5 * (1.0 + SINF(t / theta * M_PI - M_PI_2)) * r1 +
							0.5 * (1.0 + SINF((theta - t) / theta * M_PI - M_PI_2)) * r2)) *
							SINF(t + psi) + braid->center_y;
						x_2 = ((0.5 * (1.0 + SINF((t + t_inc) / theta * M_PI - M_PI_2)) * r1 +
							0.5 * (1.0 + SINF((theta - t - t_inc) / theta * M_PI - M_PI_2)) * r2)) *
							COSF(t + t_inc + psi) + braid->center_x;
						y_2 = ((0.5 * (1.0 + SINF((t + t_inc) / theta * M_PI - M_PI_2)) * r1 +
							0.5 * (1.0 + SINF((theta - t - t_inc) / theta * M_PI - M_PI_2)) * r2)) *
							SINF(t + t_inc + psi) + braid->center_y;
						if (MI_NPIXELS(mi) > 2)
							XSetForeground(display, MI_GC(mi), MI_PIXEL(mi, (int) color_use));
						else
							XSetForeground(display, MI_GC(mi), MI_WIN_WHITE_PIXEL(mi));

						XDrawLine(display, window, MI_GC(mi),
							  (int) (x_1), (int) (y_1), (int) (x_2), (int) (y_2));
					}
				} else {
					/* no crosSINFg */
#ifdef COLORCOMP
					if (MI_NPIXELS(mi) > 2) {
						color_use = color + SPINRATE *
							braid->components[applywordbackto(braid, s, i)] +
							(psi + t) / 2.0 / M_PI * (float) MI_NPIXELS(mi);
						while (color_use >= (float) MI_NPIXELS(mi))
							color_use -= (float) MI_NPIXELS(mi);
						while (color_use < 0.0)
							color_use += (float) MI_NPIXELS(mi);
					}
#endif
#ifdef COLORROUND
					if (MI_NPIXELS(mi) > 2) {
						color_use += SPINRATE * color_inc;
						while (color_use >= (float) MI_NPIXELS(mi))
							color_use -= (float) MI_NPIXELS(mi);
					}
#endif
					r1 = braid->min_radius + r_diff * (float) (s);
					x_1 = r1 * COSF(t + psi) + braid->center_x;
					y_1 = r1 * SINF(t + psi) + braid->center_y;
					x_2 = r1 * COSF(t + t_inc + psi) + braid->center_x;
					y_2 = r1 * SINF(t + t_inc + psi) + braid->center_y;
					if (MI_NPIXELS(mi) > 2)
						XSetForeground(display, MI_GC(mi), MI_PIXEL(mi, (int) color_use));
					else
						XSetForeground(display, MI_GC(mi), MI_WIN_WHITE_PIXEL(mi));

					XDrawLine(display, window, MI_GC(mi),
						  (int) (x_1), (int) (y_1), (int) (x_2), (int) (y_2));
				}
			}
		}
	}

	if (++braid->age > MI_CYCLES(mi))
		init_braid(mi);
}

void
release_braid(ModeInfo * mi)
{
	if (braids != NULL) {
		(void) free((void *) braids);
		braids = NULL;
	}
}

void
refresh_braid(ModeInfo * mi)
{
	/* Do nothing, it will refresh by itself */
}
