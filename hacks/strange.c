/* -*- Mode: C; tab-width: 4 -*- */
/* strange --- strange attractors */

#if 0
static const char sccsid[] = "@(#)strange.c	5.00 2000/11/01 xlockmore";
#endif

/*-
* Copyright (c) 1997 by Massimino Pascal <Pascal.Massimon@ens.fr>
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
* 10-Apr-2017: dmo2118@gmail.com: Enhancements for accumulator mode:
*              Performance tuning, varying colors, fixed wobbliness.
*              New options: point size, zoom, brightness, motion blur.
* 08-Apr-2017: dmo2118@gmail.com: Merged with current xlockmore strange.c.
*              Also resurrected the -curve parameter from XScreenSaver 2.31.
*              Multi-monitor fixes.
*              More allocation checks.
* 13-Apr-2010: Added useAccumulator, VARY_SPEED_TO_AVOID_BOREDOM.
* 22-Dec-2004: TDA: Replace Gauss_Rand with a real Gaussian.
* 01-Nov-2000: Allocation checks
* 30-Jul-1998: sineswiper@resonatorsoft.com: added curve factor (discovered
*              while experimenting with the Gauss_Rand function).
* 10-May-1997: jwz AT jwz.org: turned into a standalone program.
*              Made it render into an offscreen bitmap and then copy
*              that onto the screen, to reduce flicker.
*
* strange attractors are not so hard to find...
*/

/* Be sure to try:
* ./strange -points 500000 -delay 0 -point-size 3
* ./strange -points 500000 -delay 0 -point-size 2 -curve 5 -zoom 1.5 -brightness 0.75
*/

/* TODO: Can anything be done about the overflow with -point-size 32? */

#ifdef STANDALONE
# define MODE_strange
# define DEFAULTS	"*delay: 10000 \n" \
					"*ncolors: 100 \n" \
					"*fpsSolid: True \n" \
					"*ignoreRotation: True \n" \
					"*useSHM: True \n" \
					"*useThreads: True \n" \

/*				    "*lowrez: True \n" \ */

# define SMOOTH_COLORS
# define release_strange 0
# define reshape_strange 0
# define strange_handle_event 0
# include "xlockmore.h"		/* from the xscreensaver distribution */
#else /* !STANDALONE */
# include "xlock.h"		/* from the xlockmore distribution */
# define ENTRYPOINT
#endif /* !STANDALONE */

#include <errno.h>
#include "pow2.h"
#include "thread_util.h"
#include "xshm.h"

#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif

#ifdef MODE_strange
#define DEF_CURVE       "10"
#define DEF_POINTS      "5500"
#define DEF_POINT_SIZE  "1"
#define DEF_ZOOM        "0.9" /* approx. 1 / 1.1 */
#define DEF_BRIGHTNESS  "1.0"
#define DEF_MOTION_BLUR "3.0" /* Formerly MERGE_FRAMES, but it's IIR now. */

static int curve;
static int points;
static int pointSize;
static float zoom;
static float brightness;
static float motionBlur;

static XrmOptionDescRec opts[] =
{
		{"-curve",        ".strange.curve",      XrmoptionSepArg, 0},
		{"-points",       ".strange.points",     XrmoptionSepArg, 0},
		{"-point-size",   ".strange.pointSize",  XrmoptionSepArg, 0},
		{"-zoom",         ".strange.zoom",       XrmoptionSepArg, 0},
		{"-brightness",   ".strange.brightness", XrmoptionSepArg, 0},
		{"-motion-blur",  ".strange.motionBlur", XrmoptionSepArg, 0},
		THREAD_OPTIONS
};
static argtype vars[] =
{
		{&curve,      "curve",      "Curve",      DEF_CURVE,       t_Int},
		{&points,     "points",     "Points",     DEF_POINTS,      t_Int},
		{&pointSize,  "pointSize",  "PointSize",  DEF_POINT_SIZE,  t_Int},
		{&zoom,       "zoom",       "Zoom",       DEF_ZOOM,        t_Float},
		{&brightness, "brightness", "Brightness", DEF_BRIGHTNESS,  t_Float},
		{&motionBlur, "motionBlur", "MotionBlur", DEF_MOTION_BLUR, t_Float},
};
static OptionStruct desc[] =
{
		{"-curve", "set the curve factor of the attractors"},
		{"-points", "change the number of points/iterations each frame"},
		{"-point-size", "change the size of individual points"},
		{"-zoom", "zoom in or out"},
		{"-brightness", "adjust the brightness for accumulator mode"},
		{"-motion-blur", "adds motion blur"},
};
ENTRYPOINT ModeSpecOpt strange_opts =
{sizeof opts / sizeof opts[0], opts,
sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   strange_description =
{"strange", "init_strange", "draw_strange", (char *) NULL,
"init_strange", "init_strange", "free_strange", &strange_opts,
10000, 1, 1, 1, 64, 1.0, "",
"Shows strange attractors", 0, NULL};
#endif

#ifdef HAVE_JWXYZ
# define NO_DBUF
#endif

typedef float DBL;
typedef int PRM;

#define UNIT_BITS 12
#define UNIT (1<<UNIT_BITS)
#define UNIT2 (1<<14)
#define COLOR_BITS 16
/* #define UNIT2 (3140*UNIT/1000) */

#define SKIP_FIRST      100
#define DBL_To_PRM(x)  (PRM)( (DBL)(UNIT)*(x) )


#define DO_FOLD(a) (a)<0 ? -A->Fold[ (-(a))&(UNIT2-1) ] : A->Fold[ (a)&(UNIT2-1) ]

#if 0
#define DO_FOLD(a) (a)<-UNIT2 ? -A->Fold[(-(a))%UNIT2] : (a)<0 ? -A->Fold[ -(a) ] :\
(a)>UNIT2 ? A->Fold[ (a)%UNIT2 ] : A->Fold[ (a) ]
#define DO_FOLD(a) DBL_To_PRM( sin( (DBL)(a)/UNIT ) )
#define DO_FOLD(a) (a)<0 ? DBL_To_PRM( exp( 16.0*(a)/UNIT2 ) )-1.0 : \
DBL_To_PRM( 1.0-exp( -16.0*(a)/UNIT2 ) )
#endif

/* useAccumulator performs two functions:
* If it is defined, then support for the accumulator will be compiled.
* It is also the condition for which the accumulator renderer will engage.
*/
#define useAccumulator (A->Max_Pt > 6000)
#define ACC_GAMMA 10.0
#define DEF_NUM_COLS 150
/* Extra options: */
#define VARY_SPEED_TO_AVOID_BOREDOM
/*#define AUTO_ZOOM*/ /* Works funny, but try it with -curve 5. */

/******************************************************************/

#define MAX_PRM 3*5

#if defined(__BIGGEST_ALIGNMENT__) \
	&& (defined(__GNUC__) \
    && (__GNUC__ == 4 && __GNUC_MINOR__ >= 7 || __GNUC__ >= 5) \
	|| defined(__clang__))
# define ALIGNED __attribute__((aligned(__BIGGEST_ALIGNMENT__)))
# define ALIGN_HINT(ptr) __builtin_assume_aligned((ptr), __BIGGEST_ALIGNMENT__)
#else
# define ALIGNED
# define ALIGN_HINT(ptr) (ptr)
# ifndef __BIGGEST_ALIGNMENT__
#  define __BIGGEST_ALIGNMENT__ (sizeof(void *))
# endif
#endif


#ifdef HAVE_INTTYPES_H
typedef uint16_t ALIGNED PIXEL0;
typedef uint32_t PIXEL0X;
typedef uint32_t ALIGNED PIXEL1;
#else
typedef unsigned short ALIGNED PIXEL0;
typedef unsigned long PIXEL0X;
typedef unsigned long ALIGNED PIXEL1;
#endif

static const union {
#ifdef HAVE_INTTYPES_H
	uint16_t signature;
	uint8_t bytes[2];
#else
	unsigned short signature;
	unsigned char bytes[2];
#endif
} byte_order_union = {MSBFirst};

#define LOCAL_BYTE_ORDER byte_order_union.bytes[1]

typedef struct _ATTRACTOR {
	DBL         Prm1[MAX_PRM], Prm2[MAX_PRM];
	PRM         Prm[MAX_PRM], *Fold;
	void        (*Iterate) (const struct _ATTRACTOR *, PRM, PRM, PRM *, PRM *);
	void       *Buffer1, *Buffer2; /* Either XPoint or XRectangle. */
	int         Cur_Pt, Max_Pt;
	int         Col, Count, Speed;
	int         Width, Height;
	Pixmap      dbuf;	/* jwz */
	GC          dbuf_gc;
	#ifdef useAccumulator
		int visualClass;
		size_t alignedWidth;
		unsigned long rMask, gMask, bMask;
		unsigned rShift, gShift, bShift;
		PIXEL0 blurFac; /* == 0 when no motion blur is taking place. */
		double colorFac;
		XColor *palette;
		unsigned numCols;
		unsigned long *cols;
		XImage *accImage;
		XShmSegmentInfo shmInfo;
		struct _THREAD **threads;
		struct threadpool pool;
	#endif
} ATTRACTOR;

static ATTRACTOR *Root = (ATTRACTOR *) NULL;

#ifdef useAccumulator
typedef struct _THREAD {
	const ATTRACTOR *Attractor;
	unsigned long Rnd;
	size_t y0, y1, y2;

	PIXEL0 **accMap;
	PIXEL0 *bloomRows;
	PIXEL1 *colorRow;
	PIXEL0 *motionBlur;

	PRM xmin, xmax, ymin, ymax;
	unsigned pixelCount;
} THREAD;
#endif

static DBL  Amp_Prm[MAX_PRM] =
{
	1.0, 3.5, 3.5, 2.5, 4.7,
	1.0, 3.5, 3.6, 2.5, 4.7,
	1.0, 1.5, 2.2, 2.1, 3.5
};
static DBL  Mid_Prm[MAX_PRM] =
{
	0.0, 1.5, 0.0, .5, 1.5,
	0.0, 1.5, 0.0, .5, 1.5,
	0.0, 1.5, -1.0, -.5, 2.5,
};

#if 0

static inline uint64_t frq(void)
{
	return 1000000;
}

static inline uint64_t tick(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * frq() + tv.tv_usec;
}

#endif

static      DBL
Old_Gauss_Rand(DBL c, DBL A, DBL S)
{
	DBL         y,z;

	y = (DBL) LRAND() / MAXRAND;
	z = curve / 10;
  	y = A * (z - exp(-y * y * S)) / (z - exp(-S));
	if (NRAND(2))
		return (c + y);
	else
		return (c - y);
}

#if 0
/* dmo2118: seems to be responsible for lots of boring-looking rings */

/* I don't know that it makes a difference, but this one actually *is*
   a Gaussian.  [TDA] */

/* Generate Gaussian random number: mean c, "amplitude" A (actually
   A is 3*standard deviation).  'S' is unused.  */

/* Note this generates a pair of gaussian variables, so it saves one
   to give out next time it's called */

static double
Gauss_Rand(DBL c, DBL A, DBL S)
{
	static double d;
	static Bool ready = 0;
	if(ready) {
		ready = 0;
		return c + A/3 * d;
	} else {
		double x, y, w;
		do {
			x = 2.0 * (double)LRAND() / MAXRAND - 1.0;
			y = 2.0 * (double)LRAND() / MAXRAND - 1.0;
			w = x*x + y*y;
		} while(w >= 1.0);

		w = sqrt((-2 * log(w))/w);
		ready = 1;
		d =          x * w;
		return c + A/3 * y * w;
	}
}
#endif

static void
Random_Prm(DBL * Prm)
{
	int         i;

	for (i = 0; i < MAX_PRM; ++i) {
#if 0
		if (curve == 10)
			Prm[i] = Gauss_Rand (Mid_Prm[i], Amp_Prm[i], 4.0);
		else
#endif
			Prm[i] = Old_Gauss_Rand (Mid_Prm[i], Amp_Prm[i], 4.0);
	}
}

/***************************************************************/

  /* 2 examples of non-linear map */

static void
Iterate_X2(const ATTRACTOR * A, PRM x, PRM y, PRM * xo, PRM * yo)
{
	PRM         xx, yy, xy, x2y, y2x, Tmp;

	xx = (x * x) >> UNIT_BITS;
	x2y = (xx * y) >> UNIT_BITS;
	yy = (y * y) >> UNIT_BITS;
	y2x = (yy * x) >> UNIT_BITS;
	xy = (x * y) >> UNIT_BITS;

	Tmp = A->Prm[1] * xx + A->Prm[2] * xy + A->Prm[3] * yy + A->Prm[4] * x2y;
	Tmp = A->Prm[0] - y + (Tmp >> UNIT_BITS);
	*xo = DO_FOLD(Tmp);
	Tmp = A->Prm[6] * xx + A->Prm[7] * xy + A->Prm[8] * yy + A->Prm[9] * y2x;
	Tmp = A->Prm[5] + x + (Tmp >> UNIT_BITS);
	*yo = DO_FOLD(Tmp);
}

static void
Iterate_X3(const ATTRACTOR * A, PRM x, PRM y, PRM * xo, PRM * yo)
{
	PRM         xx, yy, xy, x2y, y2x, Tmp_x, Tmp_y, Tmp_z;

	xx = (x * x) >> UNIT_BITS;
	x2y = (xx * y) >> UNIT_BITS;
	yy = (y * y) >> UNIT_BITS;
	y2x = (yy * x) >> UNIT_BITS;
	xy = (x * y) >> UNIT_BITS;

	Tmp_x = A->Prm[1] * xx + A->Prm[2] * xy + A->Prm[3] * yy + A->Prm[4] * x2y;
	Tmp_x = A->Prm[0] - y + (Tmp_x >> UNIT_BITS);
	Tmp_x = DO_FOLD(Tmp_x);

	Tmp_y = A->Prm[6] * xx + A->Prm[7] * xy + A->Prm[8] * yy + A->Prm[9] * y2x;
	Tmp_y = A->Prm[5] + x + (Tmp_y >> UNIT_BITS);

	Tmp_y = DO_FOLD(Tmp_y);

	Tmp_z = A->Prm[11] * xx + A->Prm[12] * xy + A->Prm[13] * yy + A->Prm[14] * y2x;
	Tmp_z = A->Prm[10] + x + (Tmp_z >> UNIT_BITS);
	Tmp_z = UNIT + ((Tmp_z * Tmp_z) >> UNIT_BITS);

	/* Can happen with -curve 9. */
	if (!Tmp_z)
		Tmp_z = 1;

#ifdef HAVE_INTTYPES_H
	{
		uint64_t Tmp_z1 = (1 << 30) / Tmp_z;
		*xo = (Tmp_x * Tmp_z1) >> (30 - UNIT_BITS);
		*yo = (Tmp_y * Tmp_z1) >> (30 - UNIT_BITS);
	}
#else
	*xo = (Tmp_x * UNIT) / Tmp_z;
	*yo = (Tmp_y * UNIT) / Tmp_z;
#endif
}

static void (*Funcs[2]) (const ATTRACTOR *, PRM, PRM, PRM *, PRM *) = {
	Iterate_X2, Iterate_X3
};

/***************************************************************/

ENTRYPOINT void
free_strange(ModeInfo *mi)
{
	Display *display = MI_DISPLAY(mi);
	ATTRACTOR  *A = &Root[MI_SCREEN(mi)];

	if (A->Buffer1 != NULL) {
		free(A->Buffer1);
		A->Buffer1 = (XPoint *) NULL;
	}
	if (A->Buffer2 != NULL) {
		free(A->Buffer2);
		A->Buffer2 = (XPoint *) NULL;
	}
	if (A->dbuf) {
		XFreePixmap(display, A->dbuf);
		A->dbuf = None;
	}
	if (A->dbuf_gc) {
		XFreeGC(display, A->dbuf_gc);
		A->dbuf_gc = None;
	}
	if (A->Fold != NULL) {
		free(A->Fold);
		A->Fold = (PRM *) NULL;
	}

#ifdef useAccumulator
	if (useAccumulator) {
		if (A->pool.count) {
			threadpool_destroy (&A->pool);
			A->pool.count = 0;
		}

		free (A->threads);
		A->threads = NULL;

		if (A->accImage) {
			destroy_xshm_image (display, A->accImage, &A->shmInfo);
			A->accImage = NULL;
		}

		free (A->palette);
		A->palette = NULL;

		if (A->cols) {
			if (A->visualClass != TrueColor && A->numCols > 2)
				XFreeColors (display, MI_COLORMAP(mi), A->cols, A->numCols, 0);
			free (A->cols);
			A->cols = NULL;
		}
	}
#endif
}

/* NRAND() is also in use; making three PRNGs in total here. */

/* ISO/IEC 9899 suggests this one. Doesn't require 64-bit math. */
#define GOODRND(seed) ((seed) = (((seed) * 1103515245 + 12345) & 0x7fffffff))
#define GOODRND_BITS 31

/* Extremely cheap entropy: this is often a single LEA instruction. */
#define CHEAPRND(seed) ((seed) = (seed) * 5)

static void
init_draw (const ATTRACTOR *A, PRM *x, PRM *y,
           PRM *xmin, PRM *ymin, PRM *xmax, PRM *ymax, unsigned long *rnd)
{
	int         n;
	PRM         xo, yo;

	*xmin = UNIT;
	*ymin = UNIT;
	*xmax = -UNIT;
	*ymax = -UNIT;

	*x = *y = DBL_To_PRM(.0);
	for (n = SKIP_FIRST; n; --n) {
		(*A->Iterate) (A, *x, *y, &xo, &yo);

#ifdef AUTO_ZOOM
		if (xo > *xmax)
			*xmax = xo;
		if (xo < *xmin)
			*xmin = xo;
		if (yo > *ymax)
			*ymax = yo;
		if (yo < *ymin)
			*ymin = yo;
#endif

		/* Can't use NRAND(), because that modifies global state in a
		 * thread-unsafe way.
		 */
		*x = xo + (GOODRND(*rnd) >> (GOODRND_BITS - 3)) - 4;
		*y = yo + (GOODRND(*rnd) >> (GOODRND_BITS - 3)) - 4;
	}
}

static void
recalc_scale (const ATTRACTOR *A, PRM xmin, PRM ymin, PRM xmax, PRM ymax,
              DBL *Lx, DBL *Ly, PRM *mx, PRM *my)
{
#ifndef AUTO_ZOOM
	xmin = -UNIT;
	ymin = -UNIT;
	xmax = UNIT;
	ymax = UNIT;
#endif

	*Lx = zoom * (DBL) A->Width / (xmax - xmin);
	*Ly = -zoom * (DBL) A->Height / (ymax - ymin);
	*mx = A->Width/2 - (xmax + xmin) * *Lx / 2;
	*my = A->Height/2 - (ymax + ymin) * *Ly / 2;
}

#ifdef useAccumulator

static void
thread_destroy (void *Self_Raw)
{
	THREAD     *T = (THREAD *)Self_Raw;

	aligned_free (T->accMap[0]);
	(void) free((void *) T->accMap);
	aligned_free (T->bloomRows);
	aligned_free (T->colorRow);
	aligned_free (T->motionBlur);
}

static int
thread_create (void *Self_Raw, struct threadpool *pool, unsigned id)
{
	THREAD     *T = (THREAD *)Self_Raw;
	int         i;
	const ATTRACTOR *A = GET_PARENT_OBJ(ATTRACTOR, pool, pool);

	memset (T, 0, sizeof(*T));

	T->Attractor = A;
	A->threads[id] = T;

	T->Rnd = random();

	/* The gap between y0 and y1 is to preheat the box blur. */
	T->y1 = A->Height * id / pool->count;
	T->y2 = A->Height * (id + 1) / pool->count;
	T->y0 = T->y1 < pointSize ? 0 : T->y1 - pointSize;

	T->accMap = (PIXEL0**)calloc(A->Height,sizeof(PIXEL0*));
	if (!T->accMap) {
		thread_destroy (T);
		return ENOMEM;
	}
	T->accMap[0] = NULL;
	if (aligned_malloc ((void **)&T->accMap[0], __BIGGEST_ALIGNMENT__,
		A->alignedWidth * A->Height * sizeof(PIXEL0))) {
		thread_destroy (T);
		return ENOMEM;
	}
	for (i=0;i<A->Height;i++)
		T->accMap[i] = T->accMap[0] + A->alignedWidth * i;

	if (aligned_malloc ((void **)&T->bloomRows, __BIGGEST_ALIGNMENT__,
		A->alignedWidth * (pointSize + 2) * sizeof(*T->bloomRows))) {
		thread_destroy (T);
		return ENOMEM;
	}
	if (aligned_malloc ((void **)&T->colorRow, __BIGGEST_ALIGNMENT__,
		A->alignedWidth * sizeof(*T->colorRow))) {
		thread_destroy (T);
		return ENOMEM;
	}
	if (A->blurFac) {
		if (aligned_malloc ((void **)&T->motionBlur, __BIGGEST_ALIGNMENT__,
			A->alignedWidth * (T->y2 - T->y1) * sizeof(*T->motionBlur))) {
			thread_destroy (T);
			return ENOMEM;
		}

		memset (T->motionBlur, 0, A->alignedWidth * (T->y2 - T->y1) * sizeof(*T->motionBlur));
	}

	return 0;
}

static void
points_thread (void *Self_Raw)
{
	/* Restricts viewable area to 2^(32-L_Bits). */
	const unsigned L_Bits = 19; /* Max. image size: 8192x8192. */

	THREAD     *T = (THREAD *)Self_Raw;
	const ATTRACTOR *A = T->Attractor;
	int         n;
	PRM         x, y, xo, yo;
	DBL         Lx, Ly;
	PRM         iLx, iLy, cx, cy;
	void        (*Iterate) (const ATTRACTOR *, PRM, PRM, PRM *, PRM *);
	unsigned    Rnd;
	PRM         xmax, xmin, ymax, ymin;

	Iterate = A->Iterate;

	if (useAccumulator) {
		memset (T->accMap[0], 0, sizeof(PIXEL0) * A->alignedWidth * A->Height);
	}

	/* Using CHEAPRND() by itself occasionally gets stuck at 0 mod 8, so seed it
	 * from GOODRND().
	 */
	init_draw (A, &x, &y, &xmin, &ymin, &xmax, &ymax, &T->Rnd);
	recalc_scale (A, xmin, ymin, xmax, ymax, &Lx, &Ly, &cx, &cy);

	Rnd = GOODRND(T->Rnd);

	iLx = Lx * (1 << L_Bits);
	iLy = Ly * (1 << L_Bits);
	if (!iLx) /* Can happen with small windows. */
		iLx = 1;
	if (!iLy)
		iLy = 1;

	for (n = T->Attractor->Max_Pt / A->pool.count; n; --n) {
		unsigned mx,my;
		(*Iterate) (T->Attractor, x, y, &xo, &yo);
		mx = ((iLx * x) >> L_Bits) + cx;
		my = ((iLy * y) >> L_Bits) + cy;
		/* Fun trick: making m[x|y] unsigned means we can skip mx<0 && my<0. */
		if (mx<A->Width && my<A->Height)
			T->accMap[my][mx]++;

		#ifdef AUTO_ZOOM
		if (xo > xmax)
			xmax = xo;
		if (xo < xmin)
			xmin = xo;
		if (yo > ymax)
			ymax = yo;
		if (yo < ymin)
			ymin = yo;

		if (!(n & 0xfff)) {
			recalc_scale (A, xmin, ymin, xmax, ymax, &Lx, &Ly, &cx, &cy);
		}
		#endif

		/* Skimp on the randomness. */
		x = xo + (CHEAPRND(Rnd) >> (sizeof(Rnd) * 8 - 3)) - 4;
		y = yo + (CHEAPRND(Rnd) >> (sizeof(Rnd) * 8 - 3)) - 4;
	}
}

static void
rasterize_thread (void *Self_Raw)
{
	THREAD     *T = (THREAD *)Self_Raw;
	const ATTRACTOR *A = T->Attractor;
	unsigned    i, j, k;
	PRM         xmax = 0, xmin = A->Width, ymax = 0, ymin = A->Height;
	unsigned long colorScale =
		(double)A->Width * A->Height
		* (1 << COLOR_BITS) * brightness
		* A->colorFac
		* (zoom * zoom) / (0.9 * 0.9)
		/ 640.0 / 480.0
		/ (pointSize * pointSize)
		* 800000.0
		/ (float)A->Max_Pt
		* (float)A->numCols/256;
	#ifdef VARY_SPEED_TO_AVOID_BOREDOM
	unsigned    pixelCount = 0;
	#endif
	PIXEL0     *motionBlurRow = T->motionBlur;
	void       *outRow = (char *)A->accImage->data
	                     + A->accImage->bytes_per_line * T->y1;

	/* Clang needs these for loop-vectorizing; A->Width doesn't work. */
	unsigned    w = A->Width, aw = A->alignedWidth;

	if (A->numCols == 2) /* Brighter for monochrome. */
		colorScale *= 4;

	/* bloomRows: row ring buffer, bloom accumulator, in that order. */
	memset (T->bloomRows, 0, (pointSize + 1) * aw * sizeof(*T->bloomRows));

	/* This code is highly amenable to auto-vectorization; on GCC use -O3 to get
	 * that. On x86-32, also add -msse2.
	 */
	for (j=T->y0;j<T->y2;j++) {
		Bool has_col = False;
		int accum = 0;

		PIXEL0 *bloomRow =
			ALIGN_HINT(T->bloomRows + A->alignedWidth * (j % pointSize));
		PIXEL0 *accumRow =
			ALIGN_HINT(T->bloomRows + A->alignedWidth * pointSize);
		PIXEL1 *colRow = T->colorRow;

		/* Moderately fast bloom.  */

		for (i=0;i<aw;i++) {
			accumRow[i] -= bloomRow[i];
			bloomRow[i] = 0;
		}

		for (k=0;k<A->pool.count;k++) {
			const PIXEL0 *inRow = ALIGN_HINT(A->threads[k]->accMap[j]);
			for (i=0;i<aw;i++) {
				/* Lots of last-level cache misses. Such is life. */
				bloomRow[i] += inRow[i];
			}
		}

		/* Hardware prefetching works better going forwards than going backwards.
		 * Since this blurs/blooms/convolves in-place, it expands points to the
		 * right instead of to the left.
		 */
		for (i=0;i<pointSize-1;i++)
			accum += bloomRow[i];

		for (i=0;i<aw;i++) {
			PIXEL0 oldBloom = bloomRow[i];

			/* alignedWidth has extra padding for this one line. */
			accum += bloomRow[i+pointSize-1];

			bloomRow[i] = accum;
			accumRow[i] += accum;
			accum -= oldBloom;
		}

		if (j>=T->y1) {
			PIXEL0 *accumRow1 = A->blurFac ? motionBlurRow : accumRow;
			PIXEL0 blurFac = A->blurFac;

			if (blurFac) {
				/* TODO: Do I vectorize OK? */
				if (blurFac == 0x8000) {
					/* Optimization for the default. */
					for (i=0;i<aw;i++)
						motionBlurRow[i] = (motionBlurRow[i] >> 1) + accumRow[i];
				} else {
					for (i=0;i<aw;i++)
						motionBlurRow[i] = (PIXEL0)(((PIXEL0X)motionBlurRow[i] * blurFac) >> 16) + accumRow[i];
				}

				motionBlurRow += aw;
			}

			for (i=0;i<aw;i++) {
				unsigned col = (accumRow1[i] * colorScale) >> COLOR_BITS;
				if (col>A->numCols-1) {
					col = A->numCols-1;
				}
				#ifdef VARY_SPEED_TO_AVOID_BOREDOM
				if (col>0) {
					if (col<A->numCols-1)  /* we don't count maxxed out pixels */
						pixelCount++;
					if (i > xmax)
						xmax = i;
					if (i < xmin)
						xmin = i;
					has_col = True;
				}
				#endif
				colRow[i] = A->cols[col];
			}

			#ifdef VARY_SPEED_TO_AVOID_BOREDOM
			if (has_col) {
				if (j > ymax)
					ymax = j;
				if (j < ymin)
					ymin = j;
			}
			#endif

#if 0
			for (i=0;i<aw;i++) {
				if (MI_NPIXELS(mi) < 2)
					XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
				else
					/*XSetForeground(display, gc, MI_PIXEL(mi, A->Col % MI_NPIXELS(mi)));*/
					XSetForeground(display, gc, cols[col].pixel);

				if (A->dbuf != None) {
					XSetForeground(display, A->dbuf_gc, cols[col].pixel);
					XDrawPoint(display, A->dbuf, A->dbuf_gc, i, j);
				} else {
					XSetForeground(display, gc, cols[col].pixel);
					XDrawPoint(display, window, gc, i, j);
				}
			}
#endif

			if (A->accImage->bits_per_pixel==32 &&
				A->accImage->byte_order==LOCAL_BYTE_ORDER) {
				for (i=0;i<aw;i++)
					((uint32_t *)outRow)[i] = colRow[i];
			} else if (A->accImage->bits_per_pixel==16 &&
				A->accImage->byte_order==LOCAL_BYTE_ORDER) {
				for (i=0;i<aw;i++)
					((uint16_t *)outRow)[i] = colRow[i];
			} else if (A->accImage->bits_per_pixel==8) {
				for (i=0;i<aw;i++)
					((uint8_t *)outRow)[i] = colRow[i];
			} else {
				for (i=0;i<w;i++)
					XPutPixel (A->accImage, i, j, colRow[i]);
			}

			outRow = (char *)outRow + A->accImage->bytes_per_line;
		}
	}

	/*
	uint64_t dt = 0;
	uint64_t t0 = tick();
	dt += tick() - t0;

	printf("B %g\t(%d,%d)\t%dx%d\t%d\n", 1000.0 * dt / frq(),
		xmin, ymin, xmax - xmin, ymax - ymin, pixelCount);
	*/

	T->xmax = xmax;
	T->xmin = xmin;
	T->ymax = ymax;
	T->ymin = ymin;
	T->pixelCount = pixelCount;
}

static void
ramp_color (const XColor *color_in, XColor *color_out, unsigned i, unsigned n)
{
	float li;
	#define MINBLUE 1
	#define FULLBLUE 128
	#define LOW_COLOR(c) ((c)*li/FULLBLUE)
	#define HIGH_COLOR(c) ((65535-(c))*(li-FULLBLUE)/(256-FULLBLUE)+(c))
	li = MINBLUE
		+ (255.0-MINBLUE) * log(1.0 + ACC_GAMMA*(float)i/n)
		/ log(1.0 + ACC_GAMMA);
	if (li<FULLBLUE) {
		color_out->red = LOW_COLOR(color_in->red);
		color_out->green = LOW_COLOR(color_in->green);
		color_out->blue = LOW_COLOR(color_in->blue);
	} else {
		color_out->red = HIGH_COLOR(color_in->red);
		color_out->green = HIGH_COLOR(color_in->green);
		color_out->blue = HIGH_COLOR(color_in->blue);
	}
}

#endif

static void
draw_points (Display *display, Drawable d, GC gc, const void *Buf,
             unsigned Count)
{
	if (pointSize == 1)
		XDrawPoints(display, d, gc, (XPoint *)Buf, Count, CoordModeOrigin);
	else
		XFillRectangles(display, d, gc, (XRectangle *)Buf, Count);
}

ENTRYPOINT void
draw_strange(ModeInfo * mi)
{
	int         i, j, n, Cur_Pt;
	PRM         x, y, xo, yo;
	DBL         u;
	void       *Buf;
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	DBL         Lx, Ly;
	void        (*Iterate) (const ATTRACTOR *, PRM, PRM, PRM *, PRM *);
	PRM         xmin, xmax, ymin, ymax;
	ATTRACTOR  *A;
	unsigned long Rnd = random();
	int         cx, cy;

	if (Root == NULL)
		return;
	A = &Root[MI_SCREEN(mi)];
	if (A->Fold == NULL)
		return;

	Cur_Pt = A->Cur_Pt;
	Iterate = A->Iterate;

	u = (DBL) (A->Count) / 40000.0;
	for (j = MAX_PRM - 1; j >= 0; --j)
		A->Prm[j] = DBL_To_PRM((1.0 - u) * A->Prm1[j] + u * A->Prm2[j]);

	/* We collect the accumulation of the orbits in the 2d int array field. */

	init_draw (A, &x, &y, &xmin, &ymin, &xmax, &ymax, &Rnd);
	recalc_scale (A, xmin, ymin, xmax, ymax, &Lx, &Ly, &cx, &cy);

	A->Cur_Pt = 0;

	xmax = 0;
	xmin = A->Width;
	ymax = 0;
	ymin = A->Height;

	MI_IS_DRAWN(mi) = True;

	#ifdef useAccumulator
	if (useAccumulator) {
		int pixelCount = 0;

		threadpool_run (&A->pool, points_thread);

		if (A->visualClass == TrueColor) {
			XColor *src_color = &A->palette[A->Col % MI_NPIXELS(mi)];

			for (i=0;i<A->numCols;i++) {
				XColor color;
				ramp_color (src_color, &color, i, A->numCols);
				A->cols[i] =
					((((unsigned long)color.red   << 16) >> A->rShift) & A->rMask) |
					((((unsigned long)color.green << 16) >> A->gShift) & A->gMask) |
					((((unsigned long)color.blue  << 16) >> A->bShift) & A->bMask);
			}
		}
		threadpool_wait (&A->pool);

		threadpool_run(&A->pool, rasterize_thread);
		threadpool_wait(&A->pool);

		for (i=0; i!=A->pool.count; ++i) {
			THREAD *T = A->threads[i];
			if (T->xmax > xmax)
				xmax = T->xmax;
			if (T->xmin < xmin)
				xmin = T->xmin;
			if (T->ymax > ymax)
				ymax = T->ymax;
			if (T->ymin < ymin)
				ymin = T->ymin;
			pixelCount += T->pixelCount;
		}

		put_xshm_image (display, A->dbuf != None ? A->dbuf : window,
		                A->dbuf != None ? A->dbuf_gc : gc, A->accImage,
		                0, 0, 0, 0, A->accImage->width, A->accImage->height,
		                &A->shmInfo);

		if (A->dbuf != None) {
			XCopyArea(display, A->dbuf, window, gc, 0, 0, A->Width, A->Height, 0, 0);
		}
		#ifdef VARY_SPEED_TO_AVOID_BOREDOM
			/* Increase the rate of change of the parameters if the attractor has become visually boring. */
			if ((xmax - xmin < Lx * DBL_To_PRM(.2)) && (ymax - ymin < Ly * DBL_To_PRM(.2))) {
				A->Speed *= 1.25;
			} else if (pixelCount>0 && pixelCount<A->Width*A->Height/1000) {
				A->Speed *= 1.25;  /* A->Count = 1000; */
			} else {
				A->Speed = 4; /* reset to normal/default */
			}
			if (A->Speed > 32)
				A->Speed = 32;
			A->Count += A->Speed;
			if (A->Count >= 1000) {
				for (i = MAX_PRM - 1; i >= 0; --i)
					A->Prm1[i] = A->Prm2[i];
				Random_Prm(A->Prm2);
				A->Count = 0;
			}
		#endif
	} else {
	#endif
	for (n = 0; n != A->Max_Pt; ++n) {
		int x1, y1;
		(*Iterate) (A, x, y, &xo, &yo);
		x1 = (int) (Lx * x) + cx;
		y1 = (int) (Ly * y) + cy;
		if (pointSize == 1) {
			XPoint *Buf = &((XPoint *)A->Buffer2)[n];
			Buf->x = x1;
			Buf->y = y1;
		} else {
			XRectangle *Buf = &((XRectangle *)A->Buffer2)[n];
			/* Position matches bloom in accumulator mode. */
			Buf->x = x1 - pointSize + 1;
			Buf->y = y1;
			Buf->width = pointSize;
			Buf->height = pointSize;
		}

		if (x1 > xmax)
			xmax = x1;
		if (x1 < xmin)
			xmin = x1;
		if (y1 > ymax)
			ymax = y1;
		if (y1 < ymin)
			ymin = y1;

		A->Cur_Pt++;
		/* (void) fprintf( stderr, "X,Y: %d %d    ", Buf->x, Buf->y ); */
		x = xo + NRAND(8) - 4;
		y = yo + NRAND(8) - 4;
	}

	XSetForeground(display, gc, MI_BLACK_PIXEL(mi));

	if (A->dbuf != None) {		/* jwz */
		XSetForeground(display, A->dbuf_gc, 0);
/* XDrawPoints(display, A->dbuf, A->dbuf_gc, A->Buffer1,
  Cur_Pt,CoordModeOrigin); */
		XFillRectangle(display, A->dbuf, A->dbuf_gc, 0, 0, A->Width, A->Height);
	} else {
		draw_points(display, window, gc, A->Buffer1, Cur_Pt);
	}

	if (MI_NPIXELS(mi) <= 2)
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
	else
		XSetForeground(display, gc, MI_PIXEL(mi, A->Col % MI_NPIXELS(mi)));

	if (A->dbuf != None) {
		XSetForeground(display, A->dbuf_gc, 1);
		draw_points(display, A->dbuf, A->dbuf_gc, A->Buffer2, A->Cur_Pt);
		XCopyPlane(display, A->dbuf, window, gc, 0, 0, A->Width, A->Height, 0, 0, 1);
	} else
		draw_points(display, window, gc, A->Buffer2, A->Cur_Pt);

	#ifdef useAccumulator
	}
	#endif

	Buf = A->Buffer1;
	A->Buffer1 = A->Buffer2;
	A->Buffer2 = Buf;

	if ((xmax - xmin < Lx * DBL_To_PRM(.2)) && (ymax - ymin < Ly * DBL_To_PRM(.2)))
		A->Count += 4 * A->Speed;
	else
		A->Count += A->Speed;
	if (A->Count >= 1000) {
		for (i = MAX_PRM - 1; i >= 0; --i)
			A->Prm1[i] = A->Prm2[i];
		Random_Prm(A->Prm2);
		A->Count = 0;
	}
	A->Col++;
#ifdef STANDALONE
    mi->recursion_depth = A->Count;
#endif
}


/***************************************************************/

ENTRYPOINT void
init_strange(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
#ifndef NO_DBUF
	GC          gc = MI_GC(mi);
#endif
	ATTRACTOR  *Attractor;
	size_t      pointStructSize;

    if (MI_WIDTH(mi) > 2560 || MI_HEIGHT(mi) > 2560)
      pointSize *= 3;  /* Retina displays */

    pointStructSize =
		pointSize == 1 ? sizeof (XPoint) : sizeof (XRectangle);

	if (curve <= 0) curve = 10;

	MI_INIT (mi, Root);
	Attractor = &Root[MI_SCREEN(mi)];

	if (Attractor->Fold == NULL) {
		int         i;

		if ((Attractor->Fold = (PRM *) calloc(UNIT2 + 1,
				sizeof (PRM))) == NULL) {
			free_strange(mi);
			return;
		}
		for (i = 0; i <= UNIT2; ++i) {
			DBL         x;

			/* x = ( DBL )(i)/UNIT2; */
			/* x = sin( M_PI/2.0*x ); */
			/* x = sqrt( x ); */
			/* x = x*x; */
			/* x = x*(1.0-x)*4.0; */
			x = (DBL) (i) / UNIT;
			x = sin(x);
			Attractor->Fold[i] = DBL_To_PRM(x);
		}
	}

	Attractor->Max_Pt = points;

	if (Attractor->Buffer1 == NULL)
		if ((Attractor->Buffer1 = calloc(Attractor->Max_Pt,
				pointStructSize)) == NULL) {
			free_strange(mi);
			return;
		}
	if (Attractor->Buffer2 == NULL)
		if ((Attractor->Buffer2 = calloc(Attractor->Max_Pt,
				pointStructSize)) == NULL) {
			free_strange(mi);
			return;
		}

	Attractor->Width = MI_WIDTH(mi);
	Attractor->Height = MI_HEIGHT(mi);
	Attractor->Cur_Pt = 0;
	Attractor->Count = 0;
	Attractor->Col = NRAND(MI_NPIXELS(mi));
	Attractor->Speed = 4;

	Attractor->Iterate = Funcs[NRAND(2)];
	if (curve < 10) /* Avoid boring Iterate_X2. */
		Attractor->Iterate = Iterate_X3;

	Random_Prm(Attractor->Prm1);
	Random_Prm(Attractor->Prm2);
#ifndef NO_DBUF
	if (Attractor->dbuf != None)
		XFreePixmap(display, Attractor->dbuf);
	#ifdef useAccumulator
	#define A Attractor
	if (useAccumulator)
	{
		Attractor->dbuf = None;
	}
	else
	#undef A
	#endif
	{
		Attractor->dbuf = XCreatePixmap (display, MI_WINDOW(mi),
			Attractor->Width, Attractor->Height,
			/* useAccumulator ? MI_DEPTH(mi) : */ 1);
	}
	/* Allocation checked */
	if (Attractor->dbuf != None) {
		XGCValues   gcv;

		gcv.foreground = 0;
		gcv.background = 0;
#ifndef HAVE_JWXYZ
		gcv.graphics_exposures = False;
#endif /* HAVE_JWXYZ */
		gcv.function = GXcopy;

		if (Attractor->dbuf_gc != None)
			XFreeGC(display, Attractor->dbuf_gc);

		if ((Attractor->dbuf_gc = XCreateGC(display, Attractor->dbuf,
#ifndef HAVE_JWXYZ
				GCGraphicsExposures |
#endif /* HAVE_JWXYZ */
                               GCFunction | GCForeground | GCBackground,
				&gcv)) == None) {
			XFreePixmap(display, Attractor->dbuf);
			Attractor->dbuf = None;
		} else {
			XFillRectangle(display, Attractor->dbuf, Attractor->dbuf_gc,
				0, 0, Attractor->Width, Attractor->Height);
			XSetBackground(display, gc, MI_BLACK_PIXEL(mi));
			XSetFunction(display, gc, GXcopy);
		}
	}
#endif


#ifdef useAccumulator
	#define A Attractor
	if (useAccumulator) {
		static const struct threadpool_class threadClass = {
			sizeof(THREAD),
			thread_create,
			thread_destroy
		};
		Screen *screen = MI_SCREENPTR(mi);
		int i;
		unsigned maxThreads, threadCount;
		unsigned bpp = visual_pixmap_depth (screen, MI_VISUAL(mi));
		size_t threadAlign1 = 8 * thread_memory_alignment(display) - 1;
		if (A->cols) {
			if (A->visualClass != TrueColor && A->numCols > 2)
				XFreeColors (display, MI_COLORMAP(mi), A->cols, A->numCols, 0);
			free (A->cols);
		}
		if (MI_NPIXELS(mi) <= 2) {
			A->numCols = 2;
			A->visualClass = StaticColor;
		} else {
			A->numCols = DEF_NUM_COLS;
			A->visualClass = visual_class(screen, MI_VISUAL(mi));
		}

		A->cols = calloc (A->numCols,sizeof(*A->cols));
		if (!A->cols) {
			free_strange(mi);
			return;
		}

		if (A->visualClass == TrueColor) {
			/* Rebuilds ramp every frame. No need for XAllocColor. */
			/* TODO: This could also include PseudoColor. */
			visual_rgb_masks (screen, MI_VISUAL(mi),
				&A->rMask, &A->gMask, &A->bMask);
			A->rShift = 31 - i_log2 (A->rMask);
			A->gShift = 31 - i_log2 (A->gMask);
			A->bShift = 31 - i_log2 (A->bMask);

			free (A->palette);
			A->palette = malloc(MI_NPIXELS(mi) * sizeof(XColor));
			if (!A->palette) {
				free_strange (mi);
				return;
			}

			for (i=0;i<MI_NPIXELS(mi);i++)
				A->palette[i].pixel = MI_PIXEL(mi,i);

			XQueryColors (display, MI_COLORMAP(mi), A->palette, MI_NPIXELS(mi));
		} else if (A->numCols == 2) {
			A->cols[0] = MI_BLACK_PIXEL (mi);
			A->cols[1] = MI_WHITE_PIXEL (mi);
		} else {
			/* No changing colors, unfortunately. */
			XColor color;

			color.pixel = MI_PIXEL(mi,NRAND(MI_NPIXELS(mi)));
			XQueryColor (display, MI_COLORMAP(mi), &color);

			for (;;) {
				for (i=0;i<A->numCols;i++) {
					XColor out_color;
					ramp_color (&color, &out_color, i, A->numCols);
					if (!XAllocColor (display, MI_COLORMAP(mi), &out_color))
						break;
					A->cols[i] = out_color.pixel;
					/*
					if (!XAllocColor(MI_DISPLAY(mi), cmap, &cols[i])) {
					if (!XAllocColor(display, cmap, &cols[i])) {
						cols[i].pixel = WhitePixel (display, DefaultScreen (display));
						cols[i].red = cols[i].green = cols[i].blue = 0xFFFF;
					}
					*/
				}

				if (i==A->numCols)
					break;

				XFreeColors (display, MI_COLORMAP(mi), A->cols, i, 0);
				A->numCols = A->numCols * 11 / 12;
				if (A->numCols < 2) {
					A->numCols = 0;
					free_strange (mi);
					abort();
					return;
				}
			}
		}

		/* Add slack for horizontal blur, then round up to the platform's SIMD
		 * alignment.
		 */
		A->alignedWidth =
			(((A->Width + pointSize) * sizeof(PIXEL0) + (__BIGGEST_ALIGNMENT__ - 1)) &
				~(__BIGGEST_ALIGNMENT__ - 1)) / sizeof(PIXEL0);

		if (A->accImage)
			destroy_xshm_image (display, A->accImage, &A->shmInfo);
		A->accImage = create_xshm_image(display, MI_VISUAL(mi),
			MI_DEPTH(mi), ZPixmap, &A->shmInfo,
			((A->alignedWidth * bpp + threadAlign1) & ~threadAlign1) / bpp,
			A->Height);

		A->blurFac = (PIXEL0)(65536 * (motionBlur - 1) / (motionBlur + 1));
		A->colorFac = 2 / (motionBlur + 1);

		/* Don't overdose on threads. */
		threadCount = hardware_concurrency (display);
		maxThreads = A->Height / (pointSize * 4);
		if (maxThreads < 1)
			maxThreads = 1;
		if (threadCount > maxThreads)
			threadCount = maxThreads;

		if (A->threads)
			free (A->threads);
		A->threads = malloc (threadCount * sizeof(*A->threads));
		if (!A->threads) {
			free_strange (mi);
			return;
		}

		if (A->pool.count)
			threadpool_destroy (&A->pool);
		if (threadpool_create (&A->pool, &threadClass, display, threadCount)) {
			A->pool.count = 0;
			free_strange (mi);
			return;
		}
	}
	#undef A
#endif
	MI_CLEARWINDOW(mi);

	/* Do not want any exposure events from XCopyPlane */
	XSetGraphicsExposures(display, MI_GC(mi), False);
}

/***************************************************************/

#ifdef STANDALONE
XSCREENSAVER_MODULE ("Strange", strange)
#endif

#endif /* MODE_strange */
