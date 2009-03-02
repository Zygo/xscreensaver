
/* -*- Mode: C; tab-width: 4 -*- */
/* hyperer --- spinning hypercubes, not just tesseracts any more */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)hyperer.c 4.11 98/05/27 xlockmore";

#endif

/*-
 * hyperer.c (nee multidico)
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
 * hyperer (then called multidico) was originally written for
 * xscreensaver in 1992.  Unfortunately it didn't make it into that
 * code because Jamie Zawinski was implementing his own tesseract and
 * preferred his version (optimized for speed) to my version
 * (with support for more than 4 dimesnions).
 *
 * In 1998 I finally got around to porting it to xlockmore and sending
 * it off.
 *
 * (The implementation is independent of jwz's---I started with ico.)
 */

#ifdef STANDALONE
#define PROGCLASS "Hyperer"
#define HACK_INIT init_hyperer
#define HACK_DRAW draw_hyperer
#define HACK_CHANGE change_hyperer
#define hyperer_opts xlockmore_opts
#define DEFAULTS "*delay: 100000 \n" \
	"*count: -6 \n" \
	"*cycles: 300 \n" \
	"*ncolors: 200 \n" \
	"*spinDelay:	2 \n" \
	"*colorDimension: 3 \n" \
	"*showAxes:	false \n" \
	"*randomStart:	false \n" \
	"*debug:	false \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

static Bool random_start;
static Bool show_axes;
static int  spin_delay;
static int  color_dimension;
static int  group_dimension;
static int  color_groups;

extern XFontStruct *getFont();

static XrmOptionDescRec opts[] =
{
	{"-color-dimension", ".hyperer.colorDimension", XrmoptionSepArg, 0},
	{"-group-dimension", ".hyperer.groupDimension", XrmoptionSepArg, 0},
	{"-spin-delay", ".hyperer.spinDelay", XrmoptionSepArg, 0},
	{"-show-axes", ".hyperer.showAxes", XrmoptionNoArg, "true"},
	{"+show-axes", ".hyperer.showAxes", XrmoptionNoArg, "false"},
	{"+random-start", ".hyperer.randomStart", XrmoptionNoArg, "false"},
	{"-random-start", ".hyperer.randomStart", XrmoptionNoArg, "true"},
	{"+color-groups", ".hyperer.colorGroups", XrmoptionNoArg, "false"},
	{"-color-groups", ".hyperer.colorGroups", XrmoptionNoArg, "true"},
};
static argtype vars[] =
{
  {(caddr_t *) & random_start, "randomStart", "RandomStart", "True", t_Bool},
	{(caddr_t *) & show_axes, "showAxes", "ShowAxes", "True", t_Bool},
	{(caddr_t *) & color_dimension, "colorDimension", "ColorDimension", "3", t_Int},
	{(caddr_t *) & group_dimension, "groupDimension", "GroupDimension", "0", t_Int},
	{(caddr_t *) & spin_delay, "spinDelay", "SpinDelay", "2", t_Int},
 {(caddr_t *) & color_groups, "colorGroups", "ColorGroups", "False", t_Bool},
};
static OptionStruct desc[] =
{
{"-color-dimension num", "color sub-cubes of this many dimensions the same"},
	{"-group-dimension num", "aggregate this many dimensions as a single color (use +color-groups as a simpler interface)"},
     {"-/+color-groups", "turn off/on coloring sub-groups of the hypercube"},
	{"-spin-delay num", "delay in seconds before chaning spin speed"},
	{"-/+show-axes", "turn off/on showing the axes"},
	{"-/+random-start", "turn off/on begining with random rotations"},
};

ModeSpecOpt hyperer_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

typedef struct {
	double      x, y;
} dpoint;

typedef double vector;		/* Was an array of size (MAX_D + 1) */
typedef double matrix;		/* Was a double array of size (MAX_D + 1) ^ 2 */

#define MIN_D 2
#define MAX_D 10		/* Memory use goes up exponentially */

typedef struct {
	int         from, to;
	long        color;
} line_segment;


typedef struct hyperer {
	GC          gc;
	XFontStruct *font;

	int         show_axes_p;

	int         maxx, maxy;
	int         delay;
	int         spinDelay;

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

	matrix     *Tall;

	int         num_points;
	vector     *points;
	int         num_lines;
	line_segment *lines;

	int         point_set;	/* which bank of points are we using */
	XPoint     *xpoints[2];

	int         num_axis_points;

	/* Formerly an array of (MAX_D + 1) */
	int        *axis_points;

	/*
	 * Inter-step state:
	 */
	int         translated;
	int         this_set;

	int         group_dimension;
} hypererstruct;


static hypererstruct *hyperers = NULL;

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
		};
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
	};
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

static long
color_from_point(ModeInfo * mi, hypererstruct * hp, vector * from, vector * to)
{
	int         n = 0, i, same_in_low_bits = 1;
	long        color;

	for (i = 0; i < hp->num_d; i++) {
		n = (n << 1) + (to[i] ? 1 : 0);
		if (i < hp->group_dimension && from[i] != to[i])
			same_in_low_bits = 0;
	};
	if (same_in_low_bits) {
		/* toss some number of dimensions */
		n &= ~((1 << color_dimension) - 1);
		/* scale it across the points */
		color = MI_PIXEL(mi, (int) ((n * MI_NPIXELS(mi)) / hp->num_points));
		/* printf ("color %d (%x)\n", n, color); */
	} else {
		color = MI_WHITE_PIXEL(mi);
		/* printf ("color white\n"); */
	};
	return color;
}

static void
figure_points(ModeInfo * mi, hypererstruct * hp)
{
	int         i, j, k, n, d;
	int        *c;

	/*
	 * First, figure out the points.
	 */
	hp->num_points = figure_num_points(hp->num_d);
	/* assert(num_points <= max_points); */
	hp->points = allocarray(vector, hp->num_points * hp->num_mat);
	hp->xpoints[0] = allocarray(XPoint, hp->num_points);
	hp->xpoints[1] = allocarray(XPoint, hp->num_points);
	c = allocarray(int, hp->num_points);	/* will be lost, sigh */

	ZeroMultiCounter(c, hp->num_d);
	n = 0;
	do {
		for (i = 0; i < hp->num_d; i++)
			hp->points[n * hp->num_mat + i] = c[i];
		n++;
	} while (IncMultiCounter(c, hp->num_d));
	(void) free((void *) c);
#if 0
	assert(hp->num_points == n);
#endif
	/*
	 * Next connect them.
	 * We could do this more intelligently, but why bother?
	 *
	 * Connect points that differ by only one coordinate.
	 */
	hp->num_lines = figure_num_lines(hp->num_d);
	/* assert(hp->num_lines <= hp->max_lines); */
	hp->lines = (line_segment *) malloc(hp->num_lines * sizeof (line_segment));
	for (n = i = 0; i < hp->num_points; i++)
		for (j = i + 1; j < hp->num_points; j++) {
			for (d = k = 0; k < hp->num_d; k++)
				if (hp->points[i * hp->num_mat + k] != hp->points[j * hp->num_mat + k])
					d++;
			if (d == 1) {
				hp->lines[n].from = i;
				hp->lines[n].to = j;
				/* printf ("from %x to %x ", i, j); */
				hp->lines[n].color = color_from_point(mi, hp,
								      &hp->points[i * hp->num_mat], &hp->points[j * hp->num_mat]);
				n++;
			};
		};
#if 0
	assert(hp->num_lines == n);
#endif

	/*
	 * Now determine the planes of rotation.
	 */
	hp->num_planes = figure_num_planes(hp->num_d);
#if 0
	assert(hp->num_planes <= max_planes);
#endif

	hp->axis_points = allocarray(int, hp->num_d + 1);

#if 1
	hp->rotations = allocarray(double, hp->num_planes);
	hp->d_rotations = callocarray(double, hp->num_planes);
	hp->dd_rotations = callocarray(double, hp->num_planes);
	hp->cdd_rotations = callocarray(int, hp->num_planes);

	hp->Trotations = allocarray(matrix, hp->num_planes * hp->num_matmat);
	hp->Tall = allocarray(matrix, hp->num_matmat);

	hp->rotation_planes = allocarray(XPoint, hp->num_planes);

#else
#define max_planes 40
	hp->rotations = allocarray(double, max_planes);
	hp->d_rotations = allocarray(double, max_planes);
	hp->dd_rotations = allocarray(double, max_planes);
	hp->cdd_rotations = allocarray(int, max_planes);

	hp->Trotations = allocarray(matrix, max_planes);

	hp->rotation_planes = allocarray(XPoint, max_planes);
#endif

	for (n = i = 0; i < hp->num_d; i++)
		for (j = i + 1; j < hp->num_d; j++) {
			hp->rotation_planes[n].x = i;
			hp->rotation_planes[n].y = j;
			n++;
		};
#if 0
	assert(hp->num_planes == n);
#endif
	/*
	 * Potential random initial rotations.
	 */
#define FRAC (1024*16)
	if (random_start) {
		for (i = 0; i < hp->num_planes; i++)
			hp->rotations[i] = 2.0 * NRAND(FRAC) * M_PI / FRAC;
	};
}


static void
figure_axis_points(hypererstruct * hp)
{
	int         i, j, num_set;

	hp->show_axes_p = show_axes;
	for (hp->num_axis_points = i = 0; i < hp->num_points; i++) {
		for (num_set = j = 0; j < hp->num_d; j++)
			if (hp->points[i * hp->num_mat + j] != 0.0)
				num_set++;
		if (num_set <= 1)
			hp->axis_points[hp->num_axis_points++] = i;
	};
}

static void
free_hyperer(hypererstruct * hp)
{
	if (hp->axis_points) {
		(void) free((void *) hp->axis_points);
		hp->axis_points = NULL;
	}
	if (hp->points) {
		(void) free((void *) hp->points);
		hp->points = NULL;
	}
	if (hp->lines) {
		XFree(hp->lines);
		hp->lines = NULL;
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
	if (hp->Tall) {
		(void) free((void *) hp->Tall);
		hp->Tall = NULL;
	}
	if (hp->xpoints[0]) {
		XFree(hp->xpoints[0]);
		hp->xpoints[0] = NULL;
	}
	if (hp->xpoints[1]) {
		XFree(hp->xpoints[1]);
		hp->xpoints[1] = NULL;
	}
}

static void
init_x_stuff(ModeInfo * mi, hypererstruct * hp)
{
	XGCValues   gcv;
	int         options;

	hp->maxx = MI_WIDTH(mi);
	hp->maxy = MI_HEIGHT(mi);

#if 0
	hp->delay = get_integer_resource("delay", "Integer");
	if (hp->delay < 0)
		hp->delay = 0;
#endif /* 0 */


	hp->spinDelay = 10 * spin_delay;
	if (hp->spinDelay < 0)
		hp->spinDelay = 0;

	free_hyperer(hp);
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

	if (hp->font == None)
		hp->font = getFont(MI_DISPLAY(mi));

	if (hp->gc == None) {
		options = GCForeground | GCBackground | GCFont;
		gcv.foreground = MI_WHITE_PIXEL(mi);
		gcv.background = MI_BLACK_PIXEL(mi);
		gcv.font = hp->font->fid;
		hp->gc = XCreateGC(MI_DISPLAY(mi), MI_WINDOW(mi), options, &gcv);
	}
}

void
init_hyperer(ModeInfo * mi)
{
	hypererstruct *hp;
	int         i;

	if (hyperers == NULL) {
		if ((hyperers = (hypererstruct *) calloc(MI_NUM_SCREENS(mi),
					    sizeof (hypererstruct))) == NULL)
			return;
	}
	hp = &hyperers[MI_SCREEN(mi)];

	init_x_stuff(mi, hp);
	MI_CLEARWINDOW(mi);

	hp->group_dimension = group_dimension;
	/* options hacking
	 * color groups superceeds the more complex color-same-dimensions
	 */
	if (color_groups)
		hp->group_dimension = hp->num_d - color_dimension;

	figure_points(mi, hp);

	/*
	 * Fix the d+1 coord of all points.
	 */
	for (i = 0; i < hp->num_points; i++)
		hp->points[i * hp->num_mat + hp->num_d] = 1;

	figure_axis_points(hp);

	hp->this_set = hp->translated = 0;
}


/*
 * Flickering is a problem here since we erase the whole
 * thing and draw the whole thning.
 * #define'ing XOR_HACK draws the lines with xor which helps a lot
 * (it's what hyper, the 4-d hypercube does), but it doesn't work
 * for us because frequently when we have no rotation in one 
 * (of our *many*) axes then we end up XOR'ing twice => nothing.
 * 
 */
static void
draw_hyperer_step(ModeInfo * mi, hypererstruct * hp, int draw_p, int set)
{
	Display    *display = MI_DISPLAY(mi);
	int         i;
	int         from, to;
	char        tmps[80];
	unsigned int color;

/* #define XOR_HACK */
#ifdef XOR_HACK
	int         other_set = !set;

#endif /* XOR_HACK */

	if (draw_p) {
		color = MI_WHITE_PIXEL(mi);
	} else {
		color = MI_BLACK_PIXEL(mi);
	};
	XSetForeground(display, hp->gc, color);

#ifdef XOR_HACK
	XSetFunction(display, hp->gc, (draw_p == 2 ? GXxor : GXcopy));
#endif /* XOR_HACK */

	for (i = 0; i < hp->num_lines; i++) {
		from = hp->lines[i].from;
		to = hp->lines[i].to;
		if (draw_p && MI_NPIXELS(mi) > 2)
			XSetForeground(display, hp->gc, hp->lines[i].color);
		XDrawLine(display, MI_WINDOW(mi), hp->gc,
			  hp->xpoints[set][from].x, hp->xpoints[set][from].y,
			  hp->xpoints[set][to].x, hp->xpoints[set][to].y);
#ifdef XOR_HACK
		if (draw_p == 2)
			XDrawLine(display, MI_WINDOW(mi), hp->gc,
				  hp->xpoints[other_set][from].x, hp->xpoints[other_set][from].y,
				  hp->xpoints[other_set][to].x, hp->xpoints[other_set][to].y);
#endif /* XOR_HACK */
	};

	if (hp->show_axes_p) {
		XSetForeground(display, hp->gc, color);
		for (i = 0; i < hp->num_axis_points; i++) {
			sprintf(tmps, "%d", i);
			to = hp->axis_points[i];
			XDrawString(display, MI_WINDOW(mi), hp->gc,
			      hp->xpoints[set][to].x, hp->xpoints[set][to].y,
				    tmps, strlen(tmps));
		};
	};
}

static void
move_hyperer(ModeInfo * mi, hypererstruct * hp)
{
	int         i;
	int         stationary = 0;

/* NEEDSWORK: These should be resources */
#define default_cdd_rotation 10
#define default_dd_rotation (M_PI/1024.0)
	int         axis, max_axis;
	int         faster;

	if (hp->spinDelay-- <= 0) {

		hp->spinDelay = 10 * spin_delay;
		/*
		 * Change rotation?
		 *
		 * 66% chance if stationary, 33% if not.
		 */
		stationary = 1;
		for (i = 0; i < hp->num_planes; i++)
			if (hp->d_rotations[i] != 0.0 || hp->cdd_rotations[i]) {
				stationary = 0;
				break;
			};
		if (NRAND(3) < 1 + stationary) {
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
			if (faster || hp->dd_rotations[axis] != 0.0 || stationary)
				hp->dd_rotations[axis] = default_dd_rotation *
					(hp->dd_rotations[axis] >= 0.0 ? 1 : -1) *
					(faster ? 1 : -1);
			if (MI_IS_DEBUG(mi))
				fprintf(stderr,
					"axis %d made %s\n", axis, faster ? "faster" : "slower");
		};
	};

	/*
	 * Rotate.
	 */
	for (i = 0; i < hp->num_planes; i++) {
		if (hp->cdd_rotations[i]) {
			hp->cdd_rotations[i]--;
			hp->d_rotations[i] += hp->dd_rotations[i];
		};
		hp->rotations[i] += hp->d_rotations[i];
	};
}

#define NEED_SINCOS
#ifdef NEED_SINCOS
static void
sincos(double a, double *s, double *c)
{
	*s = sin(a);
	*c = cos(a);
}

#endif /* NEED_SINCOS */

static void
calc_transformation(hypererstruct * hp)
{
	double      cosa, sina;
	int         p1, p2;
	matrix     *Ttmp;
	matrix     *Tpre, *Tuser, *Tpretranspose, *Tscale, *Tposttranspose;
	int         i;
	dpoint      scale, range, offset;
	double      scale_used;

	Ttmp = allocarray(matrix, hp->num_matmat);
	Tpre = allocarray(matrix, hp->num_matmat);
	Tuser = allocarray(matrix, hp->num_matmat);
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
	};


	/*
	 * Figure the rotation.
	 */
	MatIdent(Tuser, hp->num_mat);
	for (i = 0; i < hp->num_planes; i++) {
		p1 = hp->rotation_planes[i].x;
		p2 = hp->rotation_planes[i].y;
		sincos(hp->rotations[i], &sina, &cosa);
		MatIdent(&hp->Trotations[i * hp->num_matmat], hp->num_mat);
		hp->Trotations[i * hp->num_matmat + p1 * hp->num_mat + p1] =
			hp->Trotations[i * hp->num_matmat + p2 * hp->num_mat + p2] = cosa;
		hp->Trotations[i * hp->num_matmat + p1 * hp->num_mat + p2] = sina;
		hp->Trotations[i * hp->num_matmat + p2 * hp->num_mat + p1] = -sina;
		MatMult(&hp->Trotations[i * hp->num_matmat], Tuser, Ttmp, hp->num_mat);
		MatCopy(Ttmp, Tuser, hp->num_mat);
	};

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
	range.x = range.y = sqrt((double) hp->num_d);
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

	/*
	 * Put it all together.
	 */
	MatMult(Tuser, Tpre, Ttmp, hp->num_mat);
	MatMult(Tscale, Ttmp, hp->Tall, hp->num_mat);

	(void) free((void *) Ttmp);
	(void) free((void *) Tpre);
	(void) free((void *) Tuser);
	(void) free((void *) Tpretranspose);
	(void) free((void *) Tscale);
	(void) free((void *) Tposttranspose);
}


static void
translate_point(hypererstruct * hp, vector * real, XPoint * screen_image)
{
	vector     *image;

	image = allocarray(vector, hp->num_mat);

	MatVecMult(hp->Tall, real, image, hp->num_mat);
	screen_image->x = (short) image[0];
	screen_image->y = (short) image[1];

	(void) free((void *) image);
}

static void
translate_points(hypererstruct * hp, int set)
{
	int         i;

	calc_transformation(hp);
	for (i = 0; i < hp->num_points; i++) {
		translate_point(hp, &hp->points[i * hp->num_mat], &hp->xpoints[set][i]);
	};
}



void
draw_hyperer(ModeInfo * mi)
{
	hypererstruct *hp = &hyperers[MI_SCREEN(mi)];
	int         next_set = !hp->this_set;

	if (!hp->translated) {
		/* First time through */
		translate_points(hp, next_set);
		translate_points(hp, hp->this_set);
		hp->translated = 1;
	};

#ifdef XOR_HACK
	draw_hyperer_step(mi, hp, 2, next_set);
#else /* ! XOR_HACK */
	draw_hyperer_step(mi, hp, 0, hp->this_set);
	draw_hyperer_step(mi, hp, 1, next_set);
#endif /* XOR_HACK */

	/* Set up next place */
	move_hyperer(mi, hp);
	translate_points(hp, hp->this_set);
	hp->this_set = next_set;
}

void
refresh_hyperer(ModeInfo * mi)
{
	hypererstruct *hp = &hyperers[MI_SCREEN(mi)];

	MI_CLEARWINDOW(mi);
	if (hp->translated)
		draw_hyperer_step(mi, hp, 1, hp->this_set);
}

void
change_hyperer(ModeInfo * mi)
{
	hypererstruct *hp = &hyperers[MI_SCREEN(mi)];

	/* make it change */
	hp->spinDelay = 0;
}

void
release_hyperer(ModeInfo * mi)
{
	if (hyperers != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			hypererstruct *hp = &hyperers[screen];

			if (hp->gc != NULL)
				XFreeGC(MI_DISPLAY(mi), hp->gc);
			if (hp->font)
				XFreeFont(MI_DISPLAY(mi), hp->font);
			free_hyperer(hp);
		}
		(void) free((void *) hyperers);
		hyperers = NULL;
	}
}
