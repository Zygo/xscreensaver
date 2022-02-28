/* -*- Mode: C; tab-width: 4 -*- */
/* sphere --- a bunch of shaded spheres */

#if 0
static const char sccsid[] = "@(#)sphere.c	5.00 2000/11/01 xlockmore";
#endif

/*-
 * Copyright (c) 1988 by Sun Microsystems
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
 * 30-May-1997: <jwz@jwz.org> made it go vertically as well as horizontally.
 * 27-May-1997: <jwz@jwz.org> turned into a standalone program.
 * 02-Sep-1993: xlock version David Bagley <bagleyd@tux.org>
 * 1988: Revised to use SunView canvas instead of gfxsw Sun Microsystems
 * 1982: Orignal Algorithm Tom Duff Lucasfilm Ltd.
 */

/*-
 * original copyright
 * **************************************************************************
 * Copyright 1988 by Sun Microsystems, Inc. Mountain View, CA.
 *
 * All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Sun or MIT not be used in advertising
 * or publicity pertaining to distribution of the software without specific
 * prior written permission. Sun and M.I.T. make no representations about the
 * suitability of this software for any purpose. It is provided "as is"
 * without any express or implied warranty.
 *
 * SUN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * IN NO EVENT SHALL SUN BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * ***************************************************************************
 */

#ifdef STANDALONE
# define MODE_sphere
#define DEFAULTS	"*delay: 20000 \n" \
					"*cycles: 20 \n" \
					"*size: 0 \n" \
					"*ncolors: 64 \n" \
					"*fpsSolid: true \n" \

# define BRIGHT_COLORS
# define release_sphere 0
# define reshape_sphere 0
# define sphere_handle_event 0
# include "xlockmore.h"		/* from the xscreensaver distribution */
#else /* !STANDALONE */
# include "xlock.h"		/* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef MODE_sphere

ENTRYPOINT ModeSpecOpt sphere_opts =
{0, (XrmOptionDescRec *) NULL, 0, (argtype *) NULL, (OptionStruct *) NULL};

#ifdef USE_MODULES
ModStruct   sphere_description =
{"sphere", "init_sphere", "draw_sphere", (char *) NULL,
 "refresh_sphere", "init_sphere", "free_sphere", &sphere_opts,
 5000, 1, 20, 0, 64, 1.0, "",
 "Shows a bunch of shaded spheres", 0, NULL};

#endif

/*-
 * (NX, NY, NZ) is the light source vector -- length should be 100
 */
#define NX 48
#define NY (-36)
#define NZ 80
#define NR 100
#define SQRT(a) ((int)sqrt((double)(a)))

typedef struct {
	int         width, height;
	int         radius;
	int         x0;		/* x center */
	int         y0;		/* y center */
	int         color;
	int         x, y;
	int         dirx, diry;
	int         shadowx, shadowy;
	int         maxx, maxy;
	XPoint     *points;
} spherestruct;

static spherestruct *spheres = (spherestruct *) NULL;

ENTRYPOINT void
init_sphere(ModeInfo * mi)
{
	spherestruct *sp;

	MI_INIT (mi, spheres);
	sp = &spheres[MI_SCREEN(mi)];

	if (sp->points != NULL) {
		(void) free((void *) sp->points);
		sp->points = (XPoint *) NULL;
	}
	sp->width = MAX(MI_WIDTH(mi), 4);
	sp->height = MAX(MI_HEIGHT(mi), 4);
	if ((sp->points = (XPoint *) malloc(MIN(sp->width, sp->height) *
			 sizeof (XPoint))) == NULL) {
		return;
	}

	MI_CLEARWINDOW(mi);

	sp->dirx = 1;
	sp->x = sp->radius;
	sp->shadowx = (LRAND() & 1) ? 1 : -1;
	sp->shadowy = (LRAND() & 1) ? 1 : -1;
}

ENTRYPOINT void
draw_sphere(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	int         sqrd, nd;
	register int minx = 0, maxx = 0, miny = 0, maxy = 0, npts = 0;
	spherestruct *sp;

	if (spheres == NULL)
		return;
	sp = &spheres[MI_SCREEN(mi)];
	if (sp->points == NULL)
		return;

	MI_IS_DRAWN(mi) = True;
	if ((sp->dirx && ABS(sp->x) >= sp->radius) ||
	    (sp->diry && ABS(sp->y) >= sp->radius)) {
		sp->radius = NRAND(MIN(sp->width / 2, sp->height / 2) - 1) + 1;

		if (LRAND() & 1) {
			sp->dirx = (int) (LRAND() & 1) * 2 - 1;
			sp->diry = 0;
		} else {
			sp->dirx = 0;
			sp->diry = (int) (LRAND() & 1) * 2 - 1;
		}
		sp->x0 = NRAND(sp->width);
		sp->y0 = NRAND(sp->height);

		sp->x = -sp->radius * sp->dirx;
		sp->y = -sp->radius * sp->diry;

		if (MI_NPIXELS(mi) > 2)
			sp->color = NRAND(MI_NPIXELS(mi));
	}
	if (sp->dirx == 1) {
		if (sp->x0 + sp->x < 0)
			sp->x = -sp->x0;
	} else if (sp->dirx == -1) {
		if (sp->x0 + sp->x >= sp->width)
			sp->x = sp->width - sp->x0 - 1;
	}
	if (sp->diry == 1) {
		if (sp->y0 + sp->y < 0)
			sp->y = -sp->y0;
	} else if (sp->diry == -1) {
		if (sp->y0 + sp->y >= sp->height)
			sp->y = sp->height - sp->y0 - 1;
	}
	if (sp->dirx) {
		sp->maxy = SQRT(sp->radius * sp->radius - sp->x * sp->x);
		miny = -sp->maxy;
		if (sp->y0 - sp->maxy < 0)
			miny = -sp->y0;
		maxy = sp->maxy;
	}
	if (sp->diry) {
		sp->maxx = SQRT(sp->radius * sp->radius - sp->y * sp->y);
		minx = -sp->maxx;
		if (sp->x0 - sp->maxx < 0)
			minx = -sp->x0;
		maxx = sp->maxx;
	}
	if (sp->dirx) {
		if (sp->y0 + sp->maxy >= sp->height)
			maxy = sp->height - sp->y0;
	}
	if (sp->diry) {
		if (sp->x0 + sp->maxx >= sp->width)
			maxx = sp->width - sp->x0;
	}
	XSetForeground(display, gc, MI_BLACK_PIXEL(mi));

	if (sp->dirx)
		XDrawLine(display, MI_WINDOW(mi), gc,
		sp->x0 + sp->x, sp->y0 + miny, sp->x0 + sp->x, sp->y0 + maxy);
	if (sp->diry)
		XDrawLine(display, MI_WINDOW(mi), gc,
		sp->x0 + minx, sp->y0 + sp->y, sp->x0 + maxx, sp->y0 + sp->y);

	if (MI_NPIXELS(mi) > 2)
		XSetForeground(display, gc, MI_PIXEL(mi, sp->color));
	else
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));

	if (sp->dirx) {
		sqrd = sp->radius * sp->radius - sp->x * sp->x;
		nd = NX * sp->shadowx * sp->x;
		for (sp->y = miny; sp->y <= maxy; sp->y++)
			if ((NRAND(sp->radius * NR)) <= nd + NY * sp->shadowy * sp->y +
			    NZ * SQRT(sqrd - sp->y * sp->y)) {
				sp->points[npts].x = sp->x + sp->x0;
				sp->points[npts].y = sp->y + sp->y0;
				npts++;
			}
	}
	if (sp->diry) {
		sqrd = sp->radius * sp->radius - sp->y * sp->y;
		nd = NY * sp->shadowy * sp->y;
		for (sp->x = minx; sp->x <= maxx; sp->x++)
			if ((NRAND(sp->radius * NR)) <= NX * sp->shadowx * sp->x + nd +
			    NZ * SQRT(sqrd - sp->x * sp->x)) {
				sp->points[npts].x = sp->x + sp->x0;
				sp->points[npts].y = sp->y + sp->y0;
				npts++;
			}
	}
	XDrawPoints(display, MI_WINDOW(mi), gc, sp->points, npts, CoordModeOrigin);
	if (sp->dirx == 1) {
		sp->x++;
		if (sp->x0 + sp->x >= sp->width)
			sp->x = sp->radius;
	} else if (sp->dirx == -1) {
		sp->x--;
		if (sp->x0 + sp->x < 0)
			sp->x = -sp->radius;
	}
	if (sp->diry == 1) {
		sp->y++;
		if (sp->y0 + sp->y >= sp->height)
			sp->y = sp->radius;
	} else if (sp->diry == -1) {
		sp->y--;
		if (sp->y0 + sp->y < 0)
			sp->y = -sp->radius;
	}
}

ENTRYPOINT void
free_sphere(ModeInfo * mi)
{
	spherestruct *sp;

	if (spheres == NULL)
		return;
	sp = &spheres[MI_SCREEN(mi)];

	if (sp->points) {
		(void) free((void *) sp->points);
		/* sp->points = NULL; */
	}
}

#ifndef STANDALONE
ENTRYPOINT void
refresh_sphere(ModeInfo * mi)
{
	spherestruct *sp;

	if (spheres == NULL)
		return;
	sp = &spheres[MI_SCREEN(mi)];

	MI_CLEARWINDOW(mi);

	sp->x = -sp->radius;
}
#endif

XSCREENSAVER_MODULE ("Sphere", sphere)

#endif /* MODE_sphere */
