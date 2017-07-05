/* -*- Mode: C; tab-width: 4 -*- */
/*-
 * braid --- random braids around a circle and then changes the color in
 *           a rotational pattern
 */

#if 0
static const char sccsid[] = "@(#)braid.c	5.00 2000/11/01 xlockmore";
#endif

/*-
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
 * 01-Nov-2000: Allocation checks
 * 10-May-1997: Jamie Zawinski <jwz@jwz.org> compatible with xscreensaver
 * 01-Sep-1995: color knotted components differently, J. Neil.
 * 29-Aug-1995: Written.  John Neil <neil@math.idbsu.edu>
 */

#ifdef STANDALONE
# define MODE_braid
# define DEFAULTS  "*delay: 1000 \n" \
				   "*count: 15 \n" \
				   "*cycles: 100 \n" \
				   "*size: -7 \n" \
				   "*ncolors: 64 \n" \
				   "*fpsSolid: true \n" \
				   "*ignoreRotation: True" \

# define UNIFORM_COLORS
# define release_braid 0
# include "xlockmore.h"
# include "erase.h"
#else /* STANDALONE */
# include "xlock.h"
# define ENTRYPOINT /**/
#endif /* STANDALONE */

#ifdef MODE_braid

ENTRYPOINT ModeSpecOpt braid_opts = {0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   braid_description =
{"braid", "init_braid", "draw_braid", (char *) NULL,
 "refresh_braid", "init_braid", (char *) NULL, &braid_opts,
 1000, 15, 100, 1, 64, 1.0, "",
 "Shows random braids and knots", 0, NULL};

#endif

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
	int         linewidth;
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
#ifdef STANDALONE
  eraser_state *eraser;
#endif
} braidtype;

static braidtype *braids = (braidtype *) NULL;

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

ENTRYPOINT void
init_braid(ModeInfo * mi)
{
	braidtype  *braid;
	int         used[MAXSTRANDS];
	int         i, count, comp, c;
	float       min_length;

	MI_INIT (mi, braids, 0);
	braid = &braids[MI_SCREEN(mi)];

	braid->center_x = MI_WIDTH(mi) / 2;
	braid->center_y = MI_HEIGHT(mi) / 2;
	braid->age = 0;

	/* jwz: go in the other direction sometimes. */
	braid->color_direction = ((LRAND() & 1) ? 1 : -1);

#ifndef STANDALONE
	MI_CLEARWINDOW(mi);
#endif

	min_length = (braid->center_x > braid->center_y) ?
		braid->center_y : braid->center_x;
	braid->min_radius = min_length * 0.30;
	braid->max_radius = min_length * 0.90;

	if (MI_COUNT(mi) < MINSTRANDS)
		braid->nstrands = MINSTRANDS;
	else
		braid->nstrands = INTRAND(MINSTRANDS,
				       MAX(MIN(MIN(MAXSTRANDS, MI_COUNT(mi)),
					       (int) ((braid->max_radius - braid->min_radius) / 5.0)), MINSTRANDS));
	braid->braidlength = INTRAND(MINLENGTH, MIN(MAXLENGTH -1, braid->nstrands * 6));

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

	braid->linewidth = MI_SIZE(mi);

	if (braid->linewidth < 0)
		braid->linewidth = NRAND(-braid->linewidth) + 1;
	if (braid->linewidth * braid->linewidth * 8 > MIN(MI_WIDTH(mi), MI_HEIGHT(mi)))
       braid->linewidth = MIN(1, (int) sqrt((double) MIN(MI_WIDTH(mi), MI_HEIGHT(mi)) / 8));
	for (i = 0; i < braid->nstrands; i++)
		if (!(braid->components[i] & 1))
			braid->components[i] *= -1;
}

ENTRYPOINT void
draw_braid(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         num_points = 500;
	float       t_inc;
	float       theta, psi;
	float       t, r_diff;
	int         i, s;
	float       x_1, y_1, x_2, y_2, r1, r2;
	float       color, color_use = 0.0, color_inc;
	braidtype  *braid;

	if (braids == NULL)
		return;
	braid = &braids[MI_SCREEN(mi)];

#ifdef STANDALONE
    if (braid->eraser) {
      braid->eraser = erase_window (MI_DISPLAY(mi), MI_WINDOW(mi), braid->eraser);
      if (!braid->eraser)
        init_braid(mi);
      return;
    }
#endif

	MI_IS_DRAWN(mi) = True;
	XSetLineAttributes(display, MI_GC(mi), braid->linewidth,
			   LineSolid,
			   (braid->linewidth <= 3 ? CapButt : CapRound),
			   JoinMiter);

	theta = (2.0 * M_PI) / (float) (braid->braidlength);
	t_inc = (2.0 * M_PI) / (float) num_points;
	color_inc = (float) MI_NPIXELS(mi) * braid->color_direction /
		(float) num_points;
	braid->startcolor += SPINRATE * color_inc;
	if (((int) braid->startcolor) >= MI_NPIXELS(mi))
		braid->startcolor = 0.0;

	r_diff = (braid->max_radius - braid->min_radius) / (float) (braid->nstrands);

	color = braid->startcolor;
	psi = 0.0;
	for (i = 0; i < braid->braidlength; i++) {
		psi += theta;
		for (t = 0.0; t < theta; t += t_inc) {
#ifdef COLORROUND
			color += color_inc;
			if (((int) color) >= MI_NPIXELS(mi))
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
						while (((int) color_use) >= MI_NPIXELS(mi))
							color_use -= (float) MI_NPIXELS(mi);
						while (((int) color_use) < 0)
							color_use += (float) MI_NPIXELS(mi);
					}
#endif
#ifdef COLORROUND
					if (MI_NPIXELS(mi) > 2) {
						color_use += SPINRATE * color_inc;
						while (((int) color_use) >= MI_NPIXELS(mi))
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
							XSetForeground(display, MI_GC(mi), MI_WHITE_PIXEL(mi));

						XDrawLine(display, window, MI_GC(mi),
							  (int) (x_1), (int) (y_1), (int) (x_2), (int) (y_2));
					}
#ifdef COLORCOMP
					if (MI_NPIXELS(mi) > 2) {
						color_use = color + SPINRATE *
							braid->components[applywordbackto(braid, s + 1, i)] +
							(psi + t) / 2.0 / M_PI * (float) MI_NPIXELS(mi);
						while (((int) color_use) >= MI_NPIXELS(mi))
							color_use -= (float) MI_NPIXELS(mi);
						while (((int) color_use) < 0)
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
							XSetForeground(display, MI_GC(mi), MI_WHITE_PIXEL(mi));

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
						while (((int) color_use) >= MI_NPIXELS(mi))
							color_use -= (float) MI_NPIXELS(mi);
						while (((int) color_use) < 0)
							color_use += (float) MI_NPIXELS(mi);
					}
#endif
#ifdef COLORROUND
					if (MI_NPIXELS(mi) > 2) {
						color_use += SPINRATE * color_inc;
						while (((int) color_use) >= MI_NPIXELS(mi))
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
						XSetForeground(display, MI_GC(mi), MI_WHITE_PIXEL(mi));

					XDrawLine(display, window, MI_GC(mi),
						  (int) (x_1), (int) (y_1), (int) (x_2), (int) (y_2));
				}
			}
		}
	}
	XSetLineAttributes(display, MI_GC(mi), 1, LineSolid, CapNotLast, JoinRound);

	if (++braid->age > MI_CYCLES(mi)) {
#ifdef STANDALONE
      braid->eraser = erase_window (MI_DISPLAY(mi), MI_WINDOW(mi), braid->eraser);
#else
		init_braid(mi);
#endif
	}
}

ENTRYPOINT void
reshape_braid(ModeInfo * mi, int width, int height)
{
  XClearWindow (MI_DISPLAY (mi), MI_WINDOW(mi));
  init_braid (mi);
}

ENTRYPOINT Bool
braid_handle_event (ModeInfo *mi, XEvent *event)
{
  if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      reshape_braid (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
      return True;
    }

  return False;
}

ENTRYPOINT void
refresh_braid(ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
}

XSCREENSAVER_MODULE ("Braid", braid)

#endif /* MODE_braid */
