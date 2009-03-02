/* -*- Mode: C; tab-width: 4 -*- */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)sierpinski.c	4.00 97/01/01 xlockmore";

#endif

/*-
 * sierpinski.c - Sierpinski triangle fractal for xlock,
 *   the X Window System lockscreen.
 *
 * See xlock.c for copying information.
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
 * 20-May-97: Changed the name tri to sierpinski for more compatiblity
 * 10-May-97: Jamie Zawinski <jwz@netscape.com> compatible with xscreensaver
 * 05-Sep-96: Desmond Daignault Datatimes Incorporated
 *            <tekdd@dtol.datatimes.com> .
 */

#ifdef STANDALONE
#define PROGCLASS            "Sierpinski"
#define HACK_INIT            init_sierpinski
#define HACK_DRAW            draw_sierpinski
#define DEF_DELAY            400000
#define DEF_BATCHCOUNT       2000
#define DEF_CYCLES           100
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
ModeSpecOpt sierpinski_opts =
{0, NULL, 0, NULL, NULL};

#endif /* STANDALONE */

typedef struct {
	int         width, height;
	int         time;
	int         px, py;
	int         total_npoints;
	int         npoints[3];
	unsigned long colors[3];
	XPoint     *pointBuffer[3];
	XPoint      vertex[3];
} sierpinskistruct;

static sierpinskistruct *tris = NULL;

static void
startover(ModeInfo * mi)
{
	int         j;
	sierpinskistruct *sp = &tris[MI_SCREEN(mi)];

	if (MI_NPIXELS(mi) > 2) {
		sp->colors[0] = (NRAND(MI_NPIXELS(mi)));
		sp->colors[1] = (sp->colors[0] + MI_NPIXELS(mi) / 7 +
			     NRAND(2 * MI_NPIXELS(mi) / 7)) % MI_NPIXELS(mi);
		sp->colors[2] = (sp->colors[0] + 4 * MI_NPIXELS(mi) / 7 +
			     NRAND(2 * MI_NPIXELS(mi) / 7)) % MI_NPIXELS(mi);
	}
	for (j = 0; j < 3; j++) {
		sp->vertex[j].x = NRAND(sp->width);
		sp->vertex[j].y = NRAND(sp->height);
	}
	sp->px = NRAND(sp->width);
	sp->py = NRAND(sp->height);
	sp->time = 0;
	XClearWindow(MI_DISPLAY(mi), MI_WINDOW(mi));
}

void
init_sierpinski(ModeInfo * mi)
{
	sierpinskistruct *sp;
	int         i;

	if (tris == NULL) {
		if ((tris = (sierpinskistruct *) calloc(MI_NUM_SCREENS(mi),
					 sizeof (sierpinskistruct))) == NULL)
			return;
	}
	sp = &tris[MI_SCREEN(mi)];

	sp->width = MI_WIN_WIDTH(mi);
	sp->height = MI_WIN_HEIGHT(mi);

	sp->total_npoints = MI_BATCHCOUNT(mi);
	if (sp->total_npoints < 1)
		sp->total_npoints = 1;
	for (i = 0; i < 3; i++) {
		if (!sp->pointBuffer[i])
			sp->pointBuffer[i] = (XPoint *) malloc(sp->total_npoints *
							    sizeof (XPoint));
	}
	startover(mi);
}

void
draw_sierpinski(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	sierpinskistruct *sp = &tris[MI_SCREEN(mi)];
	XPoint     *xp[3];
	int         i = 0, v;

	if (MI_NPIXELS(mi) <= 2)
		XSetForeground(display, gc, MI_WIN_WHITE_PIXEL(mi));
	for (i = 0; i < 3; i++)
		xp[i] = sp->pointBuffer[i];
	for (i = 0; i < sp->total_npoints; i++) {
		v = NRAND(3);
		sp->px = (sp->px + sp->vertex[v].x) / 2;
		sp->py = (sp->py + sp->vertex[v].y) / 2;
		xp[v]->x = sp->px;
		xp[v]->y = sp->py;
		xp[v]++;
		sp->npoints[v]++;
	}
	for (i = 0; i < 3; i++) {
		if (MI_NPIXELS(mi) > 2)
			XSetForeground(display, gc, MI_PIXEL(mi, sp->colors[i]));
		XDrawPoints(display, MI_WINDOW(mi), gc, sp->pointBuffer[i], sp->npoints[i],
			    CoordModeOrigin);
		sp->npoints[i] = 0;
	}
	if (++sp->time >= MI_CYCLES(mi))
		startover(mi);
}

void
release_sierpinski(ModeInfo * mi)
{
	if (tris != NULL) {
		int         screen, i;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			for (i = 0; i < 3; i++)
				if (tris[screen].pointBuffer[i] != NULL) {
					(void) free((void *) tris[screen].pointBuffer[i]);
				}
		}
		(void) free((void *) tris);
		tris = NULL;
	}
}

void
refresh_sierpinski(ModeInfo * mi)
{
	/* Do nothing, it will refresh by itself */
}
