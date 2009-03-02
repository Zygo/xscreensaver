/* -*- Mode: C; tab-width: 4 -*- */
/* hyper --- spinning hypercubes, not just for tesseracts any more */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)hyper.c 4.12 98/08/04 xlockmore";

#endif

/*-
 * hyper.c (nee multidico)
 * Copyright (C) 1992,1998 John Heidemann <johnh@isi.edu>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 ***
 *
 * hyper (then called multidico) was originally written for
 * xscreensaver in 1992.  Unfortunately it didn't make it into that
 * code because Jamie Zawinski was implementing his own tesseract and
 * preferred his version (optimized for speed) to my version
 * (with support for more than 4 dimesnions).
 *
 * In 1998 I finally got around to porting it to xlockmore and sending
 * it off.
 *
 * (The implementation is independent of jwz's---I started with ico.)
 *
 * Editor's Note... (DAB)
 * Removed all original Color stuff.  Replaced it so it could do xoring
 * effectively....  Took move_line from jwz's version.... to make it legal...
 *
 * Copyright (c) 1992 by Jamie Zawinski
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
 * Check out A.K. Dewdney's "Computer Recreations", Scientific American
 * Magazine" Apr 1986 pp 14-25 for more info.
 * Idea on 3d taken from there but does not work yet.  Also a small number of
 * random chosen planes drawn may be nice.
 *
 * Revision History:
 * 04-Aug-98: Added "3d" support from Scientific American
 */

#ifdef STANDALONE
#define PROGCLASS "Hyper"
#define HACK_INIT init_hyper
#define HACK_DRAW draw_hyper
#define HACK_CHANGE change_hyper
#define hyper_opts xlockmore_opts
#define DEFAULTS "*delay: 100000 \n" \
 "*count: -6 \n" \
 "*cycles: 300 \n" \
 "*ncolors: 200 \n" \
 "*use3d: False \n" \
 "*delta3d: 1.5 \n" \
 "*right3d: red \n" \
 "*left3d: blue \n" \
 "*both3d: magenta \n" \
 "*none3d: black \n" \
 "*spinDelay:	2 \n" \
 "*showAxes:	false \n" \
 "*randomStart:	false \n" \
 "*debug:	false \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_hyper

static Bool random_start;
static Bool show_axes;
static Bool show_planes;
static int  spin_delay;

extern XFontStruct *getFont(Display * display);

static XrmOptionDescRec opts[] =
{
	{"-randomstart", ".hyper.randomStart", XrmoptionNoArg, "on"},
	{"+randomstart", ".hyper.randomStart", XrmoptionNoArg, "off"},
	{"-showaxes", ".hyper.showAxes", XrmoptionNoArg, "on"},
	{"+showaxes", ".hyper.showAxes", XrmoptionNoArg, "off"},
	{"-showplanes", ".hyper.showPlanes", XrmoptionNoArg, "on"},
	{"+showplanes", ".hyper.showPlanes", XrmoptionNoArg, "off"},
	{"-spindelay", ".hyper.spinDelay", XrmoptionSepArg, 0},
};
static argtype vars[] =
{
  {(caddr_t *) & random_start, "randomStart", "RandomStart", "True", t_Bool},
	{(caddr_t *) & show_axes, "showAxes", "ShowAxes", "True", t_Bool},
	{(caddr_t *) & show_planes, "showPlanes", "ShowPlanes", "False", t_Bool},
	{(caddr_t *) & spin_delay, "spinDelay", "SpinDelay", "2", t_Int},
};
static OptionStruct desc[] =
{
	{"-/+randomstart", "turn on/off begining with random rotations"},
	{"-/+showaxes", "turn on/off showing the axes"},
	{"-/+showplanes", "turn on/off showing the planes"},
	{"-spindelay num", "delay in seconds before chaning spin speed"},
};

ModeSpecOpt hyper_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   hyper_description =
{"hyper", "init_hyper", "draw_hyper", "release_hyper",
 "refresh_hyper", "change_hyper", NULL, &hyper_opts,
 100000, -6, 300, 1, 64, 1.0, "",
 "Shows spinning n-dimensional hypercubes", 0, NULL};

#endif

#if DEBUG
#include <assert.h>
#endif

typedef struct {
	double      x, y;
} dpoint;

typedef double vector;		/* Was an array of size (MAX_D + 1) */
typedef double matrix;		/* Was a double array of size (MAX_D + 1) ^ 2 */

#define MIN_D 2
#define MAX_D 10		/* Memory use goes up exponentially */

/* Normally DELTA3D is not in degrees, but it works here, so why fight it? */
#define DELDEG (MI_DELTA3D(mi)*M_PI/180.0)

typedef struct {
	int         from, to;
	long        color;
} line_segment;

typedef struct {
	int         a, b, c, d;
	long        color;
} plane_section;


typedef struct hyper {
	GC          gc;
	Bool        redrawing;
	Bool        painted;
	XFontStruct *font;

	int         show_axes;
	int         show_planes;

	int         maxx, maxy;
	int         delay;
	int         spinDelay;
	Bool        normxor;

	/*
	 * Data storage.
	 */
	int         num_d;	/* number of dimensions */
	int         num_mat;
	int         num_matmat;

	/* there are C(num_d,2) planes */
	int         num_planes;	/* #define max_planes 40 */
	XPoint     *rotation_planes;

	/* Formerly max_planes arrays */
	double     *rotations;	/* where we are in each dimension */
	double     *d_rotations;	/* change in rotation */
	double     *dd_rotations;	/* change in change in rotation */
	int        *cdd_rotations;	/* how many turns to apply dd_rotations */
	matrix     *Trotations;
	matrix     *Trotationsleft;

	matrix     *Tall;
	matrix     *Tallleft;

	int         num_points;
	vector     *points;
	vector     *pointsleft;
	int         num_lines;
	line_segment *lines;
	plane_section *planes;

	int         point_set;	/* which bank of points are we using */
	XPoint     *xpoints[2];
	XPoint     *xpointsleft[2];

	int         num_axis_points;

	/* Formerly an array of (MAX_D + 1) */
	int        *axis_points;

	/*
	 * Inter-step state:
	 */
	int         this_set;
	Bool        stationary;
} hyperstruct;


static hyperstruct *hypers = NULL;

#define allocarray(ELTYPE,N) ((ELTYPE*)malloc((N)*sizeof(ELTYPE)))
#define callocarray(ELTYPE,N) ((ELTYPE*)calloc(N,sizeof(ELTYPE)))

/*
 * Matrix handling & 3d transformation routines
 */

static void
MatMult(matrix * a, matrix * b, matrix * c, int n)
	/* c = a * b  for n x n matricies */
{
	register int i, j, k;
	double      temp;

	/* Strassen' method... what's that? */
	for (i = 0; i < n; i++)	/* go through a's rows */
		for (j = 0; j < n; j++) {	/* go through b's columns */
			temp = 0.0;
			for (k = 0; k < n; k++)
				temp += a[i * n + k] * b[k * n + j];
			c[i * n + j] = temp;
		}
}

static void
MatVecMult(matrix * a, vector * b, vector * c, int n)
	/* c = a * b  for a n x n, b&c 1 x n */
{
	register int i, k;
	double      temp;

	for (i = 0; i < n; i++) {	/* go through a's rows */
		temp = 0.0;
		for (k = 0; k < n; k++)
			temp += a[i * n + k] * b[k];
		c[i] = temp;
	}
}

static void
MatCopy(matrix * a, matrix * b, int n)
	/* b <- a */
{
	register int i, j;

	for (i = 0; i < n; i++)
		for (j = 0; j < n; j++)
			b[i * n + j] = a[i * n + j];
}

static void
MatZero(matrix * a, int n)
	/* a = 0 */
{
	register int i, j;

	for (i = 0; i < n; i++)
		for (j = 0; j < n; j++)
			a[i * n + j] = 0.0;
}

static void
MatIdent(matrix * a, int n)
	/* a = I */
{
	register int i;

	MatZero(a, n);
	for (i = 0; i < n; i++)
		a[i * n + i] = 1.0;
}

static void
ZeroMultiCounter(int *c, int n)
{
	int         i;

	for (i = 0; i < n; i++)
		c[i] = 0;
}


static int
RealIncMultiCounter(int *c, int n, int place)
{
#define BASE 2
	if (place >= n)
		return 0;
	else if (++c[place] >= BASE) {
		c[place] = 0;
		return RealIncMultiCounter(c, n, place + 1);
	} else
		return 1;
}


static int
IncMultiCounter(int *c, int n)
{
	return RealIncMultiCounter(c, n, 0);
}


static int
figure_num_points(int d)
{
	return 1 << d;		/* (int)pow(2.0, (double)d); */
}

static int
figure_num_lines(int d)
{
	return d * figure_num_points(d - 1);
}

static int
figure_num_planes(int d)
{
	return ((d - 1) * d) / 2;
}

static void
figure_points(ModeInfo * mi)
{
	hyperstruct *hp = &hypers[MI_SCREEN(mi)];
	int         i, j, k, n, d, pix = 0;
	int        *c;

	/*
	 * First, figure out the points.
	 */
	hp->num_points = figure_num_points(hp->num_d);
#if DEBUG
	assert(hp->num_points <= hp->max_points);
#endif
	hp->points = allocarray(vector, hp->num_points * hp->num_mat);
	hp->xpoints[0] = allocarray(XPoint, hp->num_points);
	hp->xpoints[1] = allocarray(XPoint, hp->num_points);
	if (MI_IS_USE3D(mi)) {
		hp->pointsleft = allocarray(vector, hp->num_points * hp->num_mat);
		hp->xpointsleft[0] = allocarray(XPoint, hp->num_points);
		hp->xpointsleft[1] = allocarray(XPoint, hp->num_points);
	}
	c = allocarray(int, hp->num_points);	/* will be lost, sigh */

	ZeroMultiCounter(c, hp->num_d);
	n = 0;
	do {
		for (i = 0; i < hp->num_d; i++) {
			hp->points[n * hp->num_mat + i] = c[i];
			if (MI_IS_USE3D(mi))
				hp->pointsleft[n * hp->num_mat + i] = c[i];
		}
		n++;
	} while (IncMultiCounter(c, hp->num_d));
	(void) free((void *) c);
#if DEBUG
	assert(hp->num_points == n);
#endif
	/*
	 * Next connect them.
	 * We could do this more intelligently, but why bother?
	 *
	 * Connect points that differ by only one coordinate.
	 */
	if (MI_NPIXELS(mi) > 2)
		pix = NRAND(MI_NPIXELS(mi));

	hp->num_lines = figure_num_lines(hp->num_d);
#if DEBUG
	assert(hp->num_lines <= hp->max_lines);
#endif
	hp->lines = (line_segment *) malloc(hp->num_lines * sizeof (line_segment));
	for (n = i = 0; i < hp->num_points; i++) {
		for (j = i + 1; j < hp->num_points; j++) {
			for (d = k = 0; k < hp->num_d; k++) {
				if (hp->points[i * hp->num_mat + k] != hp->points[j * hp->num_mat + k])
					d++;
			}
			if (d == 1) {
				hp->lines[n].from = i;
				hp->lines[n].to = j;
				/* (void) printf ("from %x to %x ", i, j); */
				if (MI_NPIXELS(mi) > 2) {
					hp->lines[n].color = MI_PIXEL(mi, pix);
					if (++pix >= MI_NPIXELS(mi))
						pix = 0;
				} else
					hp->lines[n].color = MI_WHITE_PIXEL(mi);
				n++;
			}
		}
	}
#if DEBUG
	assert(hp->num_lines == n);
#endif

	/*
	 * Now determine the planes of rotation.
	 */
	hp->num_planes = figure_num_planes(hp->num_d);
#if DEBUG
	assert(hp->num_planes <= max_planes);
#endif
	hp->show_planes = show_planes;

	if (hp->show_planes) {
		hp->planes = (plane_section *)
				malloc(hp->num_planes * sizeof (plane_section));

		/* Keeping it simple and just drawing planes that touch the
		 * axes.  Still not the simple, have to figure out which pt c is
	 	 * furthest away and draw it first... yuck.
 		 */

		for (n = i = 0; i < hp->num_d; i++) {
			for (j = i + 1; j < hp->num_d; j++) {
				hp->planes[n].a = 0;	
				hp->planes[n].b = 1 << i;	
				hp->planes[n].d = 1 << j;	
				hp->planes[n].c = hp->planes[n].b + hp->planes[n].d;	
				/*(void) printf ("a %d, b %d, c %d, d %d\n",
					0, 1 << i, (1 << i) + (1 << j), 1 << j);*/
				if (MI_NPIXELS(mi) > 2) {
					hp->planes[n].color = MI_PIXEL(mi, pix);
					if (++pix >= MI_NPIXELS(mi))
						pix = 0;
				} else
					hp->planes[n].color = MI_WHITE_PIXEL(mi);
				n++;
			}
		}
	}

	hp->axis_points = allocarray(int, hp->num_d + 1);

	hp->rotations = allocarray(double, hp->num_planes);
	hp->d_rotations = callocarray(double, hp->num_planes);
	hp->dd_rotations = callocarray(double, hp->num_planes);
	hp->cdd_rotations = callocarray(int, hp->num_planes);

	hp->Trotations = allocarray(matrix, hp->num_planes * hp->num_matmat);
	hp->Tall = allocarray(matrix, hp->num_matmat);

	if (MI_IS_USE3D(mi)) {
		hp->Trotationsleft = allocarray(matrix, hp->num_planes * hp->num_matmat);
		hp->Tallleft = allocarray(matrix, hp->num_matmat);
	}
	hp->rotation_planes = allocarray(XPoint, hp->num_planes);

	for (n = i = 0; i < hp->num_d; i++)
		for (j = i + 1; j < hp->num_d; j++) {
			hp->rotation_planes[n].x = i;
			hp->rotation_planes[n].y = j;
			n++;
		}
#if DEBUG
	assert(hp->num_planes == n);
#endif
	/*
	 * Potential random initial rotations.
	 */
#define FRAC (1024*16)
	if (random_start) {
		for (i = 0; i < hp->num_planes; i++)
			hp->rotations[i] = 2.0 * NRAND(FRAC) * M_PI / FRAC;
	}
}

static void
figure_axis_points(hyperstruct * hp)
{
	int         i, j, num_set;

	hp->show_axes = show_axes;
	for (hp->num_axis_points = i = 0; i < hp->num_points; i++) {
		for (num_set = j = 0; j < hp->num_d; j++)
			if (hp->points[i * hp->num_mat + j] != 0.0)
				num_set++;
		if (num_set <= 1)
			hp->axis_points[hp->num_axis_points++] = i;
	}
}

static void
free_hyper(hyperstruct * hp)
{
	if (hp->axis_points) {
		(void) free((void *) hp->axis_points);
		hp->axis_points = NULL;
	}
	if (hp->points) {
		(void) free((void *) hp->points);
		hp->points = NULL;
	}
	if (hp->pointsleft) {
		(void) free((void *) hp->pointsleft);
		hp->pointsleft = NULL;
	}
	if (hp->lines) {
		XFree(hp->lines);
		hp->lines = NULL;
	}
	if (hp->planes) {
		XFree(hp->planes);
		hp->planes = NULL;
	}
	if (hp->rotation_planes) {
		XFree(hp->rotation_planes);
		hp->rotation_planes = NULL;
	}
	if (hp->rotations) {
		(void) free((void *) hp->rotations);
		hp->rotations = NULL;
	}
	if (hp->d_rotations) {
		(void) free((void *) hp->d_rotations);
		hp->d_rotations = NULL;
	}
	if (hp->dd_rotations) {
		(void) free((void *) hp->dd_rotations);
		hp->dd_rotations = NULL;
	}
	if (hp->cdd_rotations) {
		(void) free((void *) hp->cdd_rotations);
		hp->cdd_rotations = NULL;
	}
	if (hp->Trotations) {
		(void) free((void *) hp->Trotations);
		hp->Trotations = NULL;
	}
	if (hp->Trotationsleft) {
		(void) free((void *) hp->Trotationsleft);
		hp->Trotationsleft = NULL;
	}
	if (hp->Tall) {
		(void) free((void *) hp->Tall);
		hp->Tall = NULL;
	}
	if (hp->Tallleft) {
		(void) free((void *) hp->Tallleft);
		hp->Tallleft = NULL;
	}
	if (hp->xpoints[0]) {
		XFree(hp->xpoints[0]);
		hp->xpoints[0] = NULL;
	}
	if (hp->xpoints[1]) {
		XFree(hp->xpoints[1]);
		hp->xpoints[1] = NULL;
	}
	if (hp->xpointsleft[0]) {
		XFree(hp->xpointsleft[0]);
		hp->xpointsleft[0] = NULL;
	}
	if (hp->xpointsleft[1]) {
		XFree(hp->xpointsleft[1]);
		hp->xpointsleft[1] = NULL;
	}
}

static void
init_x_stuff(ModeInfo * mi)
{
	hyperstruct *hp = &hypers[MI_SCREEN(mi)];
	XGCValues   gcv;

	hp->maxx = MI_WIDTH(mi);
	hp->maxy = MI_HEIGHT(mi);


	hp->spinDelay = 10 * spin_delay;
	if (hp->spinDelay < 0)
		hp->spinDelay = 0;

	free_hyper(hp);
	hp->num_d = MI_COUNT(mi);
	if (hp->num_d < -MAX_D)
		hp->num_d = -MAX_D;
	if (hp->num_d > MAX_D)
		hp->num_d = MAX_D;
	else if (hp->num_d < -MIN_D) {
		hp->num_d = NRAND(-hp->num_d - MIN_D + 1) + MIN_D;
	} else if (hp->num_d < MIN_D)
		hp->num_d = MIN_D;

	hp->num_mat = hp->num_d + 1;
	hp->num_matmat = hp->num_mat * hp->num_mat;

	if (!hp->font)
		hp->font = getFont(MI_DISPLAY(mi));

	if (MI_NPIXELS(mi) <= 2 || MI_IS_USE3D(mi))
		hp->normxor = False;
	if (!hp->gc && (MI_NPIXELS(mi) <= 2 || !MI_IS_USE3D(mi))) {
		long        options;

		if (hp->normxor) {
			options = GCForeground | GCFont | GCFunction;
			gcv.foreground = MI_WHITE_PIXEL(mi) ^ MI_BLACK_PIXEL(mi);
			gcv.function = GXxor;
		} else {
			options = GCForeground | GCBackground | GCFont;
			gcv.foreground = MI_WHITE_PIXEL(mi);
			gcv.background = MI_BLACK_PIXEL(mi);
		}
		gcv.font = hp->font->fid;
		hp->gc = XCreateGC(MI_DISPLAY(mi), MI_WINDOW(mi), options, &gcv);
	}
}

static void
move_line(ModeInfo * mi, int from, int to, int set, long color)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	hyperstruct *hp = &hypers[MI_SCREEN(mi)];

	if (MI_NPIXELS(mi) <= 2 || !MI_IS_USE3D(mi))
		gc = hp->gc;
	if (hp->normxor) {
		XSetForeground(display, gc, color);
		if (!hp->redrawing)
			XDrawLine(display, window, gc,
			hp->xpoints[!set][from].x, hp->xpoints[!set][from].y,
			   hp->xpoints[!set][to].x, hp->xpoints[!set][to].y);
		XDrawLine(display, window, gc,
			  hp->xpoints[set][from].x, hp->xpoints[set][from].y,
			  hp->xpoints[set][to].x, hp->xpoints[set][to].y);
	} else {
		if (!hp->redrawing) {
			if (MI_IS_INSTALL(mi) && MI_IS_USE3D(mi))
				XSetForeground(display, gc, MI_NONE_COLOR(mi));
			else
				XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
			XDrawLine(display, window, gc,
			  hp->xpoints[set][from].x, hp->xpoints[set][from].y,
			     hp->xpoints[set][to].x, hp->xpoints[set][to].y);
			if (MI_IS_USE3D(mi))
				XDrawLine(display, window, gc,
					  hp->xpointsleft[set][from].x, hp->xpointsleft[set][from].y,
					  hp->xpointsleft[set][to].x, hp->xpointsleft[set][to].y);
		}
		if (MI_IS_USE3D(mi)) {
			if (MI_IS_INSTALL(mi)) {
				XSetFunction(display, gc, GXor);
			}
			XSetForeground(display, gc, MI_LEFT_COLOR(mi));
		} else if (MI_NPIXELS(mi) <= 2)
			XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
		else
			XSetForeground(display, gc, color);
		XDrawLine(display, window, gc,
			hp->xpoints[!set][from].x, hp->xpoints[!set][from].y,
			  hp->xpoints[!set][to].x, hp->xpoints[!set][to].y);
		if (MI_IS_USE3D(mi)) {
			XSetForeground(display, gc, MI_RIGHT_COLOR(mi));
			XDrawLine(display, window, gc,
				  hp->xpointsleft[!set][from].x, hp->xpointsleft[!set][from].y,
				  hp->xpointsleft[!set][to].x, hp->xpointsleft[!set][to].y);
			if (MI_IS_INSTALL(mi)) {
				XSetFunction(display, gc, GXcopy);
			}
		}
	}
}

static void
move_plane(ModeInfo * mi, int a, int b, int c, int d, int set, long color)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	hyperstruct *hp = &hypers[MI_SCREEN(mi)];
  XPoint rect[4];

	if (MI_NPIXELS(mi) <= 2 || !MI_IS_USE3D(mi))
		gc = hp->gc;
	if (hp->normxor) {
		XSetForeground(display, gc, color);
		if (!hp->redrawing) {
			rect[0].x = hp->xpoints[!set][a].x;
			rect[0].y = hp->xpoints[!set][a].y;
			rect[1].x = hp->xpoints[!set][b].x;
			rect[1].y = hp->xpoints[!set][b].y;
			rect[2].x = hp->xpoints[!set][c].x;
			rect[2].y = hp->xpoints[!set][c].y;
			rect[3].x = hp->xpoints[!set][d].x;
			rect[3].y = hp->xpoints[!set][d].y;
			XFillPolygon(display, window, gc, rect, 4, Convex, CoordModeOrigin);
		}
		rect[0].x = hp->xpoints[set][a].x;
		rect[0].y = hp->xpoints[set][a].y;
		rect[1].x = hp->xpoints[set][b].x;
		rect[1].y = hp->xpoints[set][b].y;
		rect[2].x = hp->xpoints[set][c].x;
		rect[2].y = hp->xpoints[set][c].y;
		rect[3].x = hp->xpoints[set][d].x;
		rect[3].y = hp->xpoints[set][d].y;
		XFillPolygon(display, window, gc, rect, 4, Convex, CoordModeOrigin);
	} else {
		if (!hp->redrawing) {
			if (MI_IS_INSTALL(mi) && MI_IS_USE3D(mi))
				XSetForeground(display, gc, MI_NONE_COLOR(mi));
			else
				XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
			rect[0].x = hp->xpoints[set][a].x;
			rect[0].y = hp->xpoints[set][a].y;
			rect[1].x = hp->xpoints[set][b].x;
			rect[1].y = hp->xpoints[set][b].y;
			rect[2].x = hp->xpoints[set][c].x;
			rect[2].y = hp->xpoints[set][c].y;
			rect[3].x = hp->xpoints[set][d].x;
			rect[3].y = hp->xpoints[set][d].y;
			XFillPolygon(display, window, gc, rect, 4, Convex, CoordModeOrigin);
			if (MI_IS_USE3D(mi)) {
				rect[0].x = hp->xpointsleft[set][a].x;
				rect[0].y = hp->xpointsleft[set][a].y;
				rect[1].x = hp->xpointsleft[set][b].x;
				rect[1].y = hp->xpointsleft[set][b].y;
				rect[2].x = hp->xpointsleft[set][c].x;
				rect[2].y = hp->xpointsleft[set][c].y;
				rect[3].x = hp->xpointsleft[set][d].x;
				rect[3].y = hp->xpointsleft[set][d].y;
				XFillPolygon(display, window, gc, rect, 4, Convex, CoordModeOrigin);
			}
		}
		if (MI_IS_USE3D(mi)) {
			if (MI_IS_INSTALL(mi)) {
				XSetFunction(display, gc, GXor);
			}
			XSetForeground(display, gc, MI_LEFT_COLOR(mi));
		} else if (MI_NPIXELS(mi) <= 2)
			XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
		else
			XSetForeground(display, gc, color);
		rect[0].x = hp->xpoints[!set][a].x;
		rect[0].y = hp->xpoints[!set][a].y;
		rect[1].x = hp->xpoints[!set][b].x;
		rect[1].y = hp->xpoints[!set][b].y;
		rect[2].x = hp->xpoints[!set][c].x;
		rect[2].y = hp->xpoints[!set][c].y;
		rect[3].x = hp->xpoints[!set][d].x;
		rect[3].y = hp->xpoints[!set][d].y;
		XFillPolygon(display, window, gc, rect, 4, Convex, CoordModeOrigin);
		if (MI_IS_USE3D(mi)) {
			XSetForeground(display, gc, MI_RIGHT_COLOR(mi));
			rect[0].x = hp->xpointsleft[!set][a].x;
			rect[0].y = hp->xpointsleft[!set][a].y;
			rect[1].x = hp->xpointsleft[!set][b].x;
			rect[1].y = hp->xpointsleft[!set][b].y;
			rect[2].x = hp->xpointsleft[!set][c].x;
			rect[2].y = hp->xpointsleft[!set][c].y;
			rect[3].x = hp->xpointsleft[!set][d].x;
			rect[3].y = hp->xpointsleft[!set][d].y;
			XFillPolygon(display, window, gc, rect, 4, Convex, CoordModeOrigin);
			if (MI_IS_INSTALL(mi)) {
				XSetFunction(display, gc, GXcopy);
			}
		}
	}
}

static void
move_number(ModeInfo * mi, int axis, char *string, int set, long color)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	hyperstruct *hp = &hypers[MI_SCREEN(mi)];

	if (MI_NPIXELS(mi) <= 2 || !MI_IS_USE3D(mi))
		gc = hp->gc;
	if (hp->normxor) {
		XSetForeground(display, gc, color);
		if (!hp->redrawing)
			XDrawString(display, window, gc,
			hp->xpoints[!set][axis].x, hp->xpoints[!set][axis].y,
				    string, strlen(string));
		XDrawString(display, window, gc,
			  hp->xpoints[set][axis].x, hp->xpoints[set][axis].y,
			    string, strlen(string));
	} else {
		if (!hp->redrawing) {
			if (MI_IS_INSTALL(mi) && MI_IS_USE3D(mi))
				XSetForeground(display, gc, MI_NONE_COLOR(mi));
			else
				XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
			XDrawString(display, window, gc,
			  hp->xpoints[set][axis].x, hp->xpoints[set][axis].y,
				    string, strlen(string));
			if (MI_IS_USE3D(mi))
				XDrawString(display, window, gc,
					    hp->xpointsleft[set][axis].x, hp->xpointsleft[set][axis].y,
					    string, strlen(string));
		}
		if (MI_IS_USE3D(mi)) {
			if (MI_IS_INSTALL(mi)) {
				XSetFunction(display, gc, GXor);
			}
			XSetForeground(display, gc, MI_LEFT_COLOR(mi));
		} else if (MI_NPIXELS(mi) <= 2)
			XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
		else
			XSetForeground(display, gc, color);
		XDrawString(display, window, gc,
			hp->xpoints[!set][axis].x, hp->xpoints[!set][axis].y,
			    string, strlen(string));
		if (MI_IS_USE3D(mi)) {
			XSetForeground(display, gc, MI_RIGHT_COLOR(mi));
			XDrawString(display, window, gc,
				    hp->xpointsleft[!set][axis].x, hp->xpointsleft[!set][axis].y,
				    string, strlen(string));
			if (MI_IS_INSTALL(mi)) {
				XSetFunction(display, gc, GXcopy);
			}
		}
	}
}

static void
draw_hyper_step(ModeInfo * mi, int set)
{
	hyperstruct *hp = &hypers[MI_SCREEN(mi)];
	int         i;
	char        tmps[3];

	MI_IS_DRAWN(mi) = True;

	if (!hp->stationary || hp->redrawing) {
		for (i = 0; i < hp->num_lines; i++) {
			move_line(mi, hp->lines[i].from, hp->lines[i].to,
				set, hp->lines[i].color);
		}
		if (hp->show_planes) {
			for (i = 0; i < hp->num_planes; i++) {
				move_plane(mi, hp->planes[i].a, hp->planes[i].b, hp->planes[i].c,
					hp->planes[i].d, set, hp->planes[i].color);
			}
		}
		if (hp->show_axes) {
			for (i = 0; i < hp->num_axis_points; i++) {
				(void) sprintf(tmps, "%d", i);
				move_number(mi, hp->axis_points[i], tmps, set, MI_WHITE_PIXEL(mi));
			}
		}
	}
}

static void
move_hyper(ModeInfo * mi)
{
	hyperstruct *hp = &hypers[MI_SCREEN(mi)];
	int         i;

/* NEEDSWORK: These should be resources */
#define default_cdd_rotation 10
#define default_dd_rotation (M_PI/1024.0)
	int         axis, max_axis;
	int         faster;

	hp->stationary = False;
	if (hp->spinDelay-- <= 0) {

		hp->spinDelay = 10 * spin_delay;
		/*
		 * Change rotation?
		 *
		 * 66% chance if stationary, 33% if not.
		 */
		hp->stationary = True;
		for (i = 0; i < hp->num_planes; i++)
			if (hp->d_rotations[i] != 0.0 || hp->cdd_rotations[i]) {
				hp->stationary = False;
				break;
			}
		if (NRAND(3) < 1 + hp->stationary) {
			/* Change!  But what? */
			max_axis = hp->num_planes;

			if (max_axis > hp->num_planes || max_axis < 0)
				max_axis = hp->num_planes;
			axis = NRAND(max_axis);

			/*
			 * And how much?  33% chance faster, 66% slower.
			 * If stopped, slower doesn't start it moving
			 * unless we're stationary.
			 */
			hp->cdd_rotations[axis] = default_cdd_rotation +
				NRAND(7) - 3;
			faster = (NRAND(3) < 1);
			if (faster || hp->dd_rotations[axis] != 0.0 || hp->stationary)
				hp->dd_rotations[axis] = default_dd_rotation *
					(hp->dd_rotations[axis] >= 0.0 ? 1 : -1) *
					(faster ? 1 : -1);
			if (MI_IS_DEBUG(mi))
				(void) fprintf(stderr,
					       "axis %d made %s\n", axis, faster ? "faster" : "slower");
		}
	}
	/*
	 * Rotate.
	 */
	for (i = 0; i < hp->num_planes; i++) {
		if (hp->cdd_rotations[i]) {
			hp->cdd_rotations[i]--;
			hp->d_rotations[i] += hp->dd_rotations[i];
		}
		hp->rotations[i] += hp->d_rotations[i];
	}
}

static void
calc_transformation(ModeInfo * mi)
{
	hyperstruct *hp = &hypers[MI_SCREEN(mi)];
	double      cosa, sina;
	double      cosb = 0.0, sinb = 0.0;
	int         p1, p2;
	matrix     *Ttmp;
	matrix     *Tpre, *Tuser, *Tuserleft = NULL;
	matrix     *Tpretranspose, *Tscale, *Tposttranspose;
	int         i;
	dpoint      scale, range, offset;
	double      scale_used;

	Ttmp = allocarray(matrix, hp->num_matmat);
	Tpre = allocarray(matrix, hp->num_matmat);
	Tuser = allocarray(matrix, hp->num_matmat);
	if (MI_IS_USE3D(mi)) {
		Tuserleft = allocarray(matrix, hp->num_matmat);
	}
	Tpretranspose = allocarray(matrix, hp->num_matmat);
	Tscale = allocarray(matrix, hp->num_matmat);
	Tposttranspose = allocarray(matrix, hp->num_matmat);

	/*
	 * Adjust the data.
	 * Since the data goes from 0->1 on each axis,
	 * we shift it by -0.5 here to center the figure.
	 */
	MatIdent(Tpre, hp->num_mat);
	for (i = 0; i < hp->num_d; i++) {
		Tpre[i * hp->num_mat + hp->num_d] = -0.5;
	}


	/*
	 * Figure the rotation.
	 */
	MatIdent(Tuser, hp->num_mat);
	if (MI_IS_USE3D(mi)) {
		MatIdent(Tuserleft, hp->num_mat);
	}
	for (i = 0; i < hp->num_planes; i++) {
		p1 = hp->rotation_planes[i].x;
		p2 = hp->rotation_planes[i].y;
		if (MI_IS_USE3D(mi)) {
			sinb = sin(hp->rotations[i] - DELDEG);
			cosb = cos(hp->rotations[i] - DELDEG);
			sina = sin(hp->rotations[i] + DELDEG);
			cosa = cos(hp->rotations[i] + DELDEG);
		} else {
			sina = sin(hp->rotations[i]);
			cosa = cos(hp->rotations[i]);
		}

		MatIdent(&hp->Trotations[i * hp->num_matmat], hp->num_mat);
		hp->Trotations[i * hp->num_matmat + p1 * hp->num_mat + p1] =
			hp->Trotations[i * hp->num_matmat + p2 * hp->num_mat + p2] = cosa;
		hp->Trotations[i * hp->num_matmat + p1 * hp->num_mat + p2] = sina;
		hp->Trotations[i * hp->num_matmat + p2 * hp->num_mat + p1] = -sina;
		MatMult(&hp->Trotations[i * hp->num_matmat], Tuser, Ttmp, hp->num_mat);
		MatCopy(Ttmp, Tuser, hp->num_mat);

		if (MI_IS_USE3D(mi)) {
			MatIdent(&hp->Trotationsleft[i * hp->num_matmat], hp->num_mat);
			hp->Trotationsleft[i * hp->num_matmat + p1 * hp->num_mat + p1] =
				hp->Trotationsleft[i * hp->num_matmat + p2 * hp->num_mat + p2] = cosb;
			hp->Trotationsleft[i * hp->num_matmat + p1 * hp->num_mat + p2] = sinb;
			hp->Trotationsleft[i * hp->num_matmat + p2 * hp->num_mat + p1] = -sinb;
			MatMult(&hp->Trotationsleft[i * hp->num_matmat], Tuserleft, Ttmp, hp->num_mat);
			MatCopy(Ttmp, Tuserleft, hp->num_mat);
		}
	}

/* Now calculate the scaling matrix */
#if 1
	/*
	 * Calculate the best scale of the two axes.
	 * Multiply by 0.9 to use 90% of the display.
	 * Divide by the sqrt(2.0) because it's bigest when
	 * rotated by 45 degrees.
	 *
	 * This principle generalizes to sqrt(hp->num_d).
	 */
#define border_width (0.05)
	range.x = sqrt((double) hp->num_d);
	range.y = range.x;
	scale.x = (1.0 - 2 * border_width) * hp->maxx / range.x;
	scale.y = (1.0 - 2 * border_width) * hp->maxy / range.y;
	scale_used = ((scale.x < scale.y) ? scale.x : scale.y);
	offset.x = hp->maxx / 2.0;
	offset.y = hp->maxy / 2.0;

	/*
	 * Setup & compute the matricies
	 */
	MatIdent(Tpretranspose, hp->num_mat);
	Tpretranspose[0 * hp->num_mat + hp->num_d] = 0;
	Tpretranspose[1 * hp->num_mat + hp->num_d] = 0;

	MatIdent(Tscale, hp->num_mat);
	Tscale[0 * hp->num_mat + 0] = scale_used;
	Tscale[1 * hp->num_mat + 1] = -scale_used;

	MatIdent(Tposttranspose, hp->num_mat);
	Tposttranspose[0 * hp->num_mat + hp->num_d] = offset.x;
	Tposttranspose[1 * hp->num_mat + hp->num_d] = offset.y;

	MatMult(Tscale, Tpretranspose, Ttmp, hp->num_mat);
	MatMult(Tposttranspose, Ttmp, Tscale, hp->num_mat);
#else
	MatIdent(Tscale, hp->num_mat);
#endif
	(void) free((void *) Tpretranspose);
	(void) free((void *) Tposttranspose);

	/*
	 * Put it all together.
	 */
	MatMult(Tuser, Tpre, Ttmp, hp->num_mat);
	MatMult(Tscale, Ttmp, hp->Tall, hp->num_mat);
	(void) free((void *) Tuser);
	if (MI_IS_USE3D(mi)) {
		MatMult(Tuserleft, Tpre, Ttmp, hp->num_mat);
		MatMult(Tscale, Ttmp, hp->Tallleft, hp->num_mat);
		(void) free((void *) Tuserleft);
	}
	(void) free((void *) Tpre);
	(void) free((void *) Ttmp);
	(void) free((void *) Tscale);
}


static void
translate_point(hyperstruct * hp, matrix * Tall, vector * real, XPoint * screen_image)
{
	vector     *image;

	image = allocarray(vector, hp->num_mat);

	MatVecMult(Tall, real, image, hp->num_mat);
	screen_image->x = (short) image[0];
	screen_image->y = (short) image[1];

	(void) free((void *) image);
}

static void
translate_points(ModeInfo * mi, int set)
{
	hyperstruct *hp = &hypers[MI_SCREEN(mi)];
	int         i;

	calc_transformation(mi);
	for (i = 0; i < hp->num_points; i++) {
		translate_point(hp, hp->Tall, &hp->points[i * hp->num_mat], &hp->xpoints[set][i]);
		if (MI_IS_USE3D(mi))
			translate_point(hp, hp->Tallleft, &hp->pointsleft[i * hp->num_mat], &hp->xpointsleft[set][i]);
	}
}

void
refresh_hyper(ModeInfo * mi)
{
	hyperstruct *hp = &hypers[MI_SCREEN(mi)];

	if (!hp->painted) {
		MI_CLEARWINDOW(mi);
		hp->redrawing = True;
		draw_hyper_step(mi, hp->this_set);
		hp->redrawing = False;
		hp->painted = True;
	}
}

void
init_hyper(ModeInfo * mi)
{
	hyperstruct *hp;
	int         i;

	if (hypers == NULL) {
		if ((hypers = (hyperstruct *) calloc(MI_NUM_SCREENS(mi),
					      sizeof (hyperstruct))) == NULL)
			return;
	}
	hp = &hypers[MI_SCREEN(mi)];

	init_x_stuff(mi);

	figure_points(mi);

	/*
	 * Fix the d+1 coord of all points.
	 */
	for (i = 0; i < hp->num_points; i++) {
		hp->points[i * hp->num_mat + hp->num_d] = 1;
		if (MI_IS_USE3D(mi))
			hp->pointsleft[i * hp->num_mat + hp->num_d] = 1;
	}

	figure_axis_points(hp);

	hp->this_set = 0;
	translate_points(mi, !hp->this_set);
	translate_points(mi, hp->this_set);
	refresh_hyper(mi);
	hp->painted = True;
	hp->stationary = True;
}


void
draw_hyper(ModeInfo * mi)
{
	hyperstruct *hp = &hypers[MI_SCREEN(mi)];

	hp->painted = False;
	draw_hyper_step(mi, hp->this_set);

	/* Set up next place */
	move_hyper(mi);
	translate_points(mi, hp->this_set);
	if (!hp->stationary)
		hp->this_set = !hp->this_set;
}

void
change_hyper(ModeInfo * mi)
{
	hyperstruct *hp = &hypers[MI_SCREEN(mi)];

	/* make it change */
	hp->spinDelay = 0;
}

void
release_hyper(ModeInfo * mi)
{
	if (hypers != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			hyperstruct *hp = &hypers[screen];

			if (hp->gc != NULL)
				XFreeGC(MI_DISPLAY(mi), hp->gc);
			if (hp->font)
				XFreeFont(MI_DISPLAY(mi), hp->font);
			free_hyper(hp);
		}
		(void) free((void *) hypers);
		hypers = NULL;
	}
}

#endif /* MODE_hyper */
