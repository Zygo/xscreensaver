/* -*- Mode: C; tab-width: 4 -*- */
/* sierpinski --- Sierpinski's triangle fractal */

#if 0
static const char sccsid[] = "@(#)sierpinski.c	5.00 2000/11/01 xlockmore";
#endif

/*-
 * Copyright (c) 1996 by Desmond Daignault
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
 * Dots initially appear where they "should not".  Later they get
 * "focused".  This is correct behavior.
 *
 * Revision History:
 * 01-Nov-2000: Allocation checks
 * 18-Sep-1997: 3D version Antti Kuntsi <kuntsi@iki.fi>.
 * 20-May-1997: Changed the name tri to sierpinski for more compatiblity
 * 10-May-1997: Jamie Zawinski <jwz@jwz.org> compatible with xscreensaver
 * 05-Sep-1996: Desmond Daignault Datatimes Incorporated
 *            <tekdd@dtol.datatimes.com> .
 */

#ifdef STANDALONE
# define MODE_sierpinski
# define DEFAULTS	"*delay: 400000 \n" \
					"*count: 2000 \n" \
					"*cycles: 100 \n" \
					"*ncolors: 64 \n" \
					"*fpsSolid: true \n" \
					"*ignoreRotation: True \n" \
				    "*lowrez: True \n" \

# define BRIGHT_COLORS
# define release_sierpinski 0
# define reshape_sierpinski 0
# define sierpinski_handle_event 0
# include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
# include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_sierpinski

ENTRYPOINT ModeSpecOpt sierpinski_opts =
{0, (XrmOptionDescRec *) NULL, 0, (argtype *) NULL, (OptionStruct *) NULL};

#ifdef USE_MODULES
ModStruct   sierpinski_description =
{"sierpinski", "init_sierpinski", "draw_sierpinski", (char *) NULL,
 "refresh_sierpinski", "init_sierpinski", "free_sierpinski", &sierpinski_opts,
 400000, 2000, 100, 1, 64, 1.0, "",
 "Shows Sierpinski's triangle", 0, NULL};

#endif

#define MAXCORNERS 4

typedef struct {
	int         width, height;
	int         time;
	int         px, py;
	int         total_npoints;
	int         corners;
	int         npoints[MAXCORNERS];
	unsigned long colors[MAXCORNERS];
	XPoint     *pointBuffer[MAXCORNERS];
	XPoint      vertex[MAXCORNERS];
} sierpinskistruct;

static sierpinskistruct *tris = (sierpinskistruct *) NULL;

static void
startover(ModeInfo * mi)
{
	int         j;
	sierpinskistruct *sp = &tris[MI_SCREEN(mi)];

	if (MI_NPIXELS(mi) > 2) {
		if (sp->corners == 3) {
			sp->colors[0] = (NRAND(MI_NPIXELS(mi)));
			sp->colors[1] = (sp->colors[0] + MI_NPIXELS(mi) / 7 +
			 NRAND(2 * MI_NPIXELS(mi) / 7 + 1)) % MI_NPIXELS(mi);
			sp->colors[2] = (sp->colors[0] + 4 * MI_NPIXELS(mi) / 7 +
			 NRAND(2 * MI_NPIXELS(mi) / 7 + 1)) % MI_NPIXELS(mi);
		} else if (sp->corners == 4) {
			sp->colors[0] = (NRAND(MI_NPIXELS(mi)));
			sp->colors[1] = (sp->colors[0] + MI_NPIXELS(mi) / 7 +
			     NRAND(MI_NPIXELS(mi) / 7 + 1)) % MI_NPIXELS(mi);
			sp->colors[2] = (sp->colors[0] + 3 * MI_NPIXELS(mi) / 7 +
			     NRAND(MI_NPIXELS(mi) / 7 + 1)) % MI_NPIXELS(mi);
			sp->colors[3] = (sp->colors[0] + 5 * MI_NPIXELS(mi) / 7 +
			     NRAND(MI_NPIXELS(mi) / 7 + 1)) % MI_NPIXELS(mi);
		} else {
			(void) fprintf(stderr, "colors not set for %d corners\n", sp->corners);
		}
	}
	for (j = 0; j < sp->corners; j++) {
		sp->vertex[j].x = NRAND(sp->width);
		sp->vertex[j].y = NRAND(sp->height);
	}
	sp->px = NRAND(sp->width);
	sp->py = NRAND(sp->height);
	sp->time = 0;

	MI_CLEARWINDOW(mi);
}

ENTRYPOINT void
free_sierpinski(ModeInfo * mi)
{
	sierpinskistruct *sp = &tris[MI_SCREEN(mi)];
	int corner;

	for (corner = 0; corner < MAXCORNERS; corner++)
		if (sp->pointBuffer[corner] != NULL) {
			(void) free((void *) sp->pointBuffer[corner]);
			sp->pointBuffer[corner] = (XPoint *) NULL;
		}
}

ENTRYPOINT void
init_sierpinski(ModeInfo * mi)
{
	int         i;
	sierpinskistruct *sp;

	MI_INIT (mi, tris);
	sp = &tris[MI_SCREEN(mi)];

	sp->width = MI_WIDTH(mi);
	sp->height = MI_HEIGHT(mi);

	sp->total_npoints = MI_COUNT(mi);
	if (sp->total_npoints < 1)
		sp->total_npoints = 1;
	sp->corners = MI_SIZE(mi);
	if (sp->corners < 3 || sp->corners > 4) {
		sp->corners = (int) (LRAND() & 1) + 3;
	}
	for (i = 0; i < sp->corners; i++) {
		if (!sp->pointBuffer[i])
			if ((sp->pointBuffer[i] = (XPoint *) malloc(sp->total_npoints *
					sizeof (XPoint))) == NULL) {
				free_sierpinski(mi);
				return;
			}
	}
	startover(mi);
}

ENTRYPOINT void
draw_sierpinski(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	XPoint     *xp[MAXCORNERS];
	int         i, v;
	sierpinskistruct *sp;

	if (tris == NULL)
		return;
	sp = &tris[MI_SCREEN(mi)];
	if (sp->pointBuffer[0] == NULL)
		return;

	MI_IS_DRAWN(mi) = True;
	if (MI_NPIXELS(mi) <= 2)
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
	for (i = 0; i < sp->corners; i++)
		xp[i] = sp->pointBuffer[i];
	for (i = 0; i < sp->total_npoints; i++) {
		v = NRAND(sp->corners);
		sp->px = (sp->px + sp->vertex[v].x) / 2;
		sp->py = (sp->py + sp->vertex[v].y) / 2;
		xp[v]->x = sp->px;
		xp[v]->y = sp->py;
		xp[v]++;
		sp->npoints[v]++;
	}
	for (i = 0; i < sp->corners; i++) {
		if (MI_NPIXELS(mi) > 2)
			XSetForeground(display, gc, MI_PIXEL(mi, sp->colors[i]));
		XDrawPoints(display, MI_WINDOW(mi), gc, sp->pointBuffer[i], sp->npoints[i],
			    CoordModeOrigin);
		sp->npoints[i] = 0;
	}
	if (++sp->time >= MI_CYCLES(mi))
		startover(mi);
}

#ifndef STANDALONE
ENTRYPOINT void
refresh_sierpinski(ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
}
#endif

XSCREENSAVER_MODULE ("Sierpinski", sierpinski)

#endif /* MODE_sierpinski */
