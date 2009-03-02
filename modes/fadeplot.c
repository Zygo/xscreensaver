/* -*- Mode: C; tab-width: 4 -*- */
/* fadeplot --- a fading plot of sine squared */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)fadeplot.c	5.00 2000/11/01 xlockmore";

#endif

/*-
 * Some easy plotting stuff, by Bas van Gaalen, Holland, PD
 *
 * Copyright (c) 1996 by Charles Vidal
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
 * 10-May-1997: Compatible with screensaver
 * 1996: Written by Charles Vidal based on work by Bas van Gaalen
 */

#ifdef STANDALONE
#define PROGCLASS "Fadeplot"
#define HACK_INIT init_fadeplot
#define HACK_DRAW draw_fadeplot
#define fadeplot_opts xlockmore_opts
#define DEFAULTS "*delay: 30000 \n" \
 "*count: 10 \n" \
 "*cycles: 1500 \n" \
 "*ncolors: 64 \n"
#define BRIGHT_COLORS
#define UNIFORM_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */

#endif /* STANDALONE */

#ifdef MODE_fadeplot

ModeSpecOpt fadeplot_opts =
{0, (XrmOptionDescRec *) NULL, 0, (argtype *) NULL, (OptionStruct *) NULL};

#ifdef USE_MODULES
ModStruct   fadeplot_description =
{"fadeplot", "init_fadeplot", "draw_fadeplot", "release_fadeplot",
 "refresh_fadeplot", "init_fadeplot", NULL, &fadeplot_opts,
 30000, 10, 1500, 1, 64, 0.6, "",
 "Shows a fading plot of sine squared", 0, NULL};

#endif

#define MINSTEPS 1

typedef struct {
	XPoint      speed, step, factor, st;
	int         temps, maxpts, nbstep;
	int         min;
	int         width, height;
	int         pix;
	int         angles;
	int        *stab;
	XPoint     *pts;
} fadeplotstruct;

static fadeplotstruct *fadeplots = NULL;

static void
free_fadeplot(fadeplotstruct *fp)
{
	if (fp->pts != NULL) {
		(void) free((void *) fp->pts);
		fp->pts = NULL;
	}
	if (fp->stab != NULL) {
		(void) free((void *) fp->stab);
		fp->stab = NULL;
	}
}

static Bool
initSintab(ModeInfo * mi)
{
	fadeplotstruct *fp = &fadeplots[MI_SCREEN(mi)];
	int         i;
	float       x;

	fp->angles = NRAND(950) + 250;
	if ((fp->stab = (int *) malloc(fp->angles * sizeof (int))) == NULL) {
		free_fadeplot(fp);
		return False;
	}
	for (i = 0; i < fp->angles; i++) {
		x = SINF(2.0 * M_PI * i / fp->angles);
		fp->stab[i] = (int) (x * ABS(x) * fp->min) + fp->min;
	}
	return True;
}

void
init_fadeplot(ModeInfo * mi)
{
	fadeplotstruct *fp;

	if (fadeplots == NULL) {
		if ((fadeplots = (fadeplotstruct *) calloc(MI_NUM_SCREENS(mi),
					   sizeof (fadeplotstruct))) == NULL)
			return;
	}
	fp = &fadeplots[MI_SCREEN(mi)];

	fp->width = MI_WIDTH(mi);
	fp->height = MI_HEIGHT(mi);
	fp->min = MAX(MIN(fp->width, fp->height) / 2, 1);

	fp->speed.x = 8;
	fp->speed.y = 10;
	fp->step.x = 1;
	fp->step.y = 1;
	fp->temps = 0;
	fp->factor.x = MAX(fp->width / (2 * fp->min), 1);
	fp->factor.y = MAX(fp->height / (2 * fp->min), 1);

	fp->nbstep = MI_COUNT(mi);
	if (fp->nbstep < -MINSTEPS) {
		fp->nbstep = NRAND(-fp->nbstep - MINSTEPS + 1) + MINSTEPS;
	} else if (fp->nbstep < MINSTEPS)
		fp->nbstep = MINSTEPS;

	fp->maxpts = MI_CYCLES(mi);
	if (fp->maxpts < 1)
		fp->maxpts = 1;

	if (fp->pts == NULL) {
		if ((fp->pts = (XPoint *) calloc(fp->maxpts, sizeof (XPoint))) ==
				 NULL) {
			free_fadeplot(fp);
			return;
		}
	}
	if (MI_NPIXELS(mi) > 2)
		fp->pix = NRAND(MI_NPIXELS(mi));

	if (fp->stab != NULL)
		(void) free((void *) fp->stab);
	if (!initSintab(mi))
		return;
	MI_CLEARWINDOW(mi);
}

void
draw_fadeplot(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	int         i, j, temp;
	fadeplotstruct *fp;

	if (fadeplots == NULL)
		return;
	fp = &fadeplots[MI_SCREEN(mi)];
	if (fp->stab == NULL)
		return;

	MI_IS_DRAWN(mi) = True;
	XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
	XDrawPoints(display, window, gc, fp->pts, fp->maxpts, CoordModeOrigin);

	if (MI_NPIXELS(mi) > 2) {
		XSetForeground(display, gc, MI_PIXEL(mi, fp->pix));
		if (++fp->pix >= MI_NPIXELS(mi))
			fp->pix = 0;
	} else
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));

	temp = 0;
	for (j = 0; j < fp->nbstep; j++) {
		for (i = 0; i < fp->maxpts / fp->nbstep; i++) {
			fp->pts[temp].x =
				fp->stab[(fp->st.x + fp->speed.x * j + i * fp->step.x) % fp->angles] *
				fp->factor.x + fp->width / 2 - fp->min;
			fp->pts[temp].y =
				fp->stab[(fp->st.y + fp->speed.y * j + i * fp->step.y) % fp->angles] *
				fp->factor.y + fp->height / 2 - fp->min;
			temp++;
		}
	}
	XDrawPoints(display, window, gc, fp->pts, temp, CoordModeOrigin);
	XFlush(display);
	fp->st.x = (fp->st.x + fp->speed.x) % fp->angles;
	fp->st.y = (fp->st.y + fp->speed.y) % fp->angles;
	fp->temps++;
	if ((fp->temps % (fp->angles / 2)) == 0) {
		fp->temps = fp->temps % fp->angles * 5;
		if ((fp->temps % (fp->angles)) == 0)
			fp->speed.y = (fp->speed.y++) % 30 + 1;
		if ((fp->temps % (fp->angles * 2)) == 0)
			fp->speed.x = (fp->speed.x) % 20;
		if ((fp->temps % (fp->angles * 3)) == 0)
			fp->step.y = (fp->step.y++) % 2 + 1;

		MI_CLEARWINDOW(mi);
	}
}
void
refresh_fadeplot(ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
}

void
release_fadeplot(ModeInfo * mi)
{
	if (fadeplots != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_fadeplot(&fadeplots[screen]);
		(void) free((void *) fadeplots);
		fadeplots = NULL;
	}
}

#endif /* MODE_fadeplot */
