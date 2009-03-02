/* -*- Mode: C; tab-width: 4 -*- */
/* strange --- strange attractors */

#if !defined( lint ) && !defined( SABER )
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
 * 01-Nov-2000: Allocation checks
 * 10-May-1997: jwz@jwz.org: turned into a standalone program.
 *              Made it render into an offscreen bitmap and then copy
 *              that onto the screen, to reduce flicker.
 *
 * strange attractors are not so hard to find...
 */

#ifdef STANDALONE
#define MODE_strange
#define PROGCLASS "Strange"
#define HACK_INIT init_strange
#define HACK_DRAW draw_strange
#define strange_opts xlockmore_opts
#define DEFAULTS "*delay: 2000 \n" \
 "*ncolors: 100 \n"
#define SMOOTH_COLORS
#include "xlockmore.h"		/* from the xscreensaver distribution */
#else /* !STANDALONE */
#include "xlock.h"		/* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef MODE_strange

ModeSpecOpt strange_opts =
{0, (XrmOptionDescRec *) NULL, 0, (argtype *) NULL, (OptionStruct *) NULL};

#ifdef USE_MODULES
ModStruct   strange_description =
{"strange", "init_strange", "draw_strange", "release_strange",
 "init_strange", "init_strange", (char *) NULL, &strange_opts,
 1000, 1, 1, 1, 64, 1.0, "",
 "Shows strange attractors", 0, NULL};

#endif

typedef float DBL;
typedef int PRM;

#define UNIT (1<<12)
#define UNIT2 (1<<14)
/* #define UNIT2 (3140*UNIT/1000) */

#define SKIP_FIRST      100
#define MAX_POINTS      5500
#define DBL_To_PRM(x)  (PRM)( (DBL)(UNIT)*(x) )


#define DO_FOLD(a) (a)<0 ? -A->Fold[ (-(a))&(UNIT2-1) ] : A->Fold[ (a)&(UNIT2-1) ]

#if 0
#define DO_FOLD(a) (a)<-UNIT2 ? -A->Fold[(-(a))%UNIT2] : (a)<0 ? -A->Fold[ -(a) ] :\
(a)>UNIT2 ? A->Fold[ (a)%UNIT2 ] : A->Fold[ (a) ]
#define DO_FOLD(a) DBL_To_PRM( sin( (DBL)(a)/UNIT ) )
#define DO_FOLD(a) (a)<0 ? DBL_To_PRM( exp( 16.0*(a)/UNIT2 ) )-1.0 : \
DBL_To_PRM( 1.0-exp( -16.0*(a)/UNIT2 ) )
#endif

/******************************************************************/

#define MAX_PRM 3*5

typedef struct _ATTRACTOR {
	DBL         Prm1[MAX_PRM], Prm2[MAX_PRM];
	PRM         Prm[MAX_PRM], *Fold;
	void        (*Iterate) (struct _ATTRACTOR *, PRM, PRM, PRM *, PRM *);
	XPoint     *Buffer1, *Buffer2;
	int         Cur_Pt, Max_Pt;
	int         Col, Count, Speed;
	int         Width, Height;
	Pixmap      dbuf;	/* jwz */
	GC          dbuf_gc;
} ATTRACTOR;

static ATTRACTOR *Root = (ATTRACTOR *) NULL; 

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

static      DBL
Gauss_Rand(DBL c, DBL A, DBL S)
{
	DBL         y;

	y = (DBL) LRAND() / MAXRAND;
	y = A * (1.0 - exp(-y * y * S)) / (1.0 - exp(-S));
	if (NRAND(2))
		return (c + y);
	else
		return (c - y);
}

static void
Random_Prm(DBL * Prm)
{
	int         i;

	for (i = 0; i < MAX_PRM; ++i)
		Prm[i] = Gauss_Rand(Mid_Prm[i], Amp_Prm[i], 4.0);
}

/***************************************************************/

   /* 2 examples of non-linear map */

static void
Iterate_X2(ATTRACTOR * A, PRM x, PRM y, PRM * xo, PRM * yo)
{
	PRM         xx, yy, xy, x2y, y2x, Tmp;

	xx = (x * x) / UNIT;
	x2y = (xx * y) / UNIT;
	yy = (y * y) / UNIT;
	y2x = (yy * x) / UNIT;
	xy = (x * y) / UNIT;

	Tmp = A->Prm[1] * xx + A->Prm[2] * xy + A->Prm[3] * yy + A->Prm[4] * x2y;
	Tmp = A->Prm[0] - y + (Tmp / UNIT);
	*xo = DO_FOLD(Tmp);
	Tmp = A->Prm[6] * xx + A->Prm[7] * xy + A->Prm[8] * yy + A->Prm[9] * y2x;
	Tmp = A->Prm[5] + x + (Tmp / UNIT);
	*yo = DO_FOLD(Tmp);
}

static void
Iterate_X3(ATTRACTOR * A, PRM x, PRM y, PRM * xo, PRM * yo)
{
	PRM         xx, yy, xy, x2y, y2x, Tmp_x, Tmp_y, Tmp_z;

	xx = (x * x) / UNIT;
	x2y = (xx * y) / UNIT;
	yy = (y * y) / UNIT;
	y2x = (yy * x) / UNIT;
	xy = (x * y) / UNIT;

	Tmp_x = A->Prm[1] * xx + A->Prm[2] * xy + A->Prm[3] * yy + A->Prm[4] * x2y;
	Tmp_x = A->Prm[0] - y + (Tmp_x / UNIT);
	Tmp_x = DO_FOLD(Tmp_x);

	Tmp_y = A->Prm[6] * xx + A->Prm[7] * xy + A->Prm[8] * yy + A->Prm[9] * y2x;
	Tmp_y = A->Prm[5] + x + (Tmp_y / UNIT);

	Tmp_y = DO_FOLD(Tmp_y);

	Tmp_z = A->Prm[11] * xx + A->Prm[12] * xy + A->Prm[13] * yy + A->Prm[14] * y2x;
	Tmp_z = A->Prm[10] + x + (Tmp_z / UNIT);
	Tmp_z = UNIT + Tmp_z * Tmp_z / UNIT;

	*xo = (Tmp_x * UNIT) / Tmp_z;
	*yo = (Tmp_y * UNIT) / Tmp_z;
}

static void (*Funcs[2]) (ATTRACTOR *, PRM, PRM, PRM *, PRM *) = {
	Iterate_X2, Iterate_X3
};

/***************************************************************/

static void
free_strange(Display *display, ATTRACTOR *A)
{
	if (A->Buffer1 != NULL) {
		(void) free((void *) A->Buffer1);
		A->Buffer1 = (XPoint *) NULL;
	}
	if (A->Buffer2 != NULL) {
		(void) free((void *) A->Buffer2);
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
		(void) free((void *) A->Fold);
		A->Fold = (PRM *) NULL;
	}
}

void
draw_strange(ModeInfo * mi)
{
	int         i, j, n, Cur_Pt;
	PRM         x, y, xo, yo;
	DBL         u;
	XPoint     *Buf;
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	DBL         Lx, Ly;
	void        (*Iterate) (ATTRACTOR *, PRM, PRM, PRM *, PRM *);
	PRM         xmin, xmax, ymin, ymax;
	ATTRACTOR  *A;

	if (Root == NULL)
		return;
	A = &Root[MI_SCREEN(mi)];
	if (A->Fold == NULL)
		return;

	Cur_Pt = A->Cur_Pt;
	Iterate = A->Iterate;

	u = (DBL) (A->Count) / 1000.0;
	for (j = MAX_PRM - 1; j >= 0; --j)
		A->Prm[j] = DBL_To_PRM((1.0 - u) * A->Prm1[j] + u * A->Prm2[j]);

	x = y = DBL_To_PRM(.0);
	for (n = SKIP_FIRST; n; --n) {
		(*Iterate) (A, x, y, &xo, &yo);
		x = xo + NRAND(8) - 4;
		y = yo + NRAND(8) - 4;
	}

	xmax = 0;
	xmin = UNIT * 4;
	ymax = 0;
	ymin = UNIT * 4;
	A->Cur_Pt = 0;
	Buf = A->Buffer2;
	Lx = (DBL) A->Width / UNIT / 2.2;
	Ly = (DBL) A->Height / UNIT / 2.2;
	for (n = A->Max_Pt; n; --n) {
		(*Iterate) (A, x, y, &xo, &yo);
		Buf->x = (int) (Lx * (x + DBL_To_PRM(1.1)));
		Buf->y = (int) (Ly * (DBL_To_PRM(1.1) - y));
		/* (void) fprintf( stderr, "X,Y: %d %d    ", Buf->x, Buf->y ); */
		Buf++;
		A->Cur_Pt++;
		if (xo > xmax)
			xmax = xo;
		else if (xo < xmin)
			xmin = xo;
		if (yo > ymax)
			ymax = yo;
		else if (yo < ymin)
			ymin = yo;
		x = xo + NRAND(8) - 4;
		y = yo + NRAND(8) - 4;
	}

	MI_IS_DRAWN(mi) = True;

	XSetForeground(display, gc, MI_BLACK_PIXEL(mi));

	if (A->dbuf != None) {		/* jwz */
		XSetForeground(display, A->dbuf_gc, 0);
/* XDrawPoints(display, A->dbuf, A->dbuf_gc, A->Buffer1,
   Cur_Pt,CoordModeOrigin); */
		XFillRectangle(display, A->dbuf, A->dbuf_gc, 0, 0, A->Width, A->Height);
	} else
		XDrawPoints(display, window, gc, A->Buffer1, Cur_Pt, CoordModeOrigin);

	if (MI_NPIXELS(mi) < 2)
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
	else
		XSetForeground(display, gc, MI_PIXEL(mi, A->Col % MI_NPIXELS(mi)));

	if (A->dbuf != None) {
		XSetForeground(display, A->dbuf_gc, 1);
		XDrawPoints(display, A->dbuf, A->dbuf_gc, A->Buffer2, A->Cur_Pt,
			    CoordModeOrigin);
		XCopyPlane(display, A->dbuf, window, gc, 0, 0, A->Width, A->Height, 0, 0, 1);
	} else
		XDrawPoints(display, window, gc, A->Buffer2, A->Cur_Pt, CoordModeOrigin);

	Buf = A->Buffer1;
	A->Buffer1 = A->Buffer2;
	A->Buffer2 = Buf;

	if ((xmax - xmin < DBL_To_PRM(.2)) && (ymax - ymin < DBL_To_PRM(.2)))
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
}


/***************************************************************/

void
init_strange(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	ATTRACTOR  *Attractor;

	if (Root == NULL) {
		if ((Root = (ATTRACTOR *) calloc(MI_NUM_SCREENS(mi),
				sizeof (ATTRACTOR))) == NULL)
			return;
	}
	Attractor = &Root[MI_SCREEN(mi)];

	if (Attractor->Fold == NULL) {
		int         i;

		if ((Attractor->Fold = (PRM *) calloc(UNIT2 + 1,
				sizeof (PRM))) == NULL) {
			free_strange(display, Attractor);
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
	if (Attractor->Buffer1 == NULL)
		if ((Attractor->Buffer1 = (XPoint *) calloc(MAX_POINTS,
				sizeof (XPoint))) == NULL) {
			free_strange(display, Attractor);
			return;
		}
	if (Attractor->Buffer2 == NULL)
		if ((Attractor->Buffer2 = (XPoint *) calloc(MAX_POINTS,
				sizeof (XPoint))) == NULL) {
			free_strange(display, Attractor);
			return;
		}
	Attractor->Max_Pt = MAX_POINTS;

	Attractor->Width = MI_WIDTH(mi);
	Attractor->Height = MI_HEIGHT(mi);
	Attractor->Cur_Pt = 0;
	Attractor->Count = 0;
	Attractor->Col = NRAND(MI_NPIXELS(mi));
	Attractor->Speed = 4;

	Attractor->Iterate = Funcs[NRAND(2)];
	Random_Prm(Attractor->Prm1);
	Random_Prm(Attractor->Prm2);
#ifndef NO_DBUF
	if (Attractor->dbuf != None)
		XFreePixmap(display, Attractor->dbuf);
	Attractor->dbuf = XCreatePixmap(display, window,
	     Attractor->Width, Attractor->Height, 1);
	/* Allocation checked */
	if (Attractor->dbuf != None) {
		XGCValues   gcv;

		gcv.foreground = 0;
		gcv.background = 0;
		gcv.graphics_exposures = False;
		gcv.function = GXcopy;

		if (Attractor->dbuf_gc != None)
			XFreeGC(display, Attractor->dbuf_gc);

		if ((Attractor->dbuf_gc = XCreateGC(display, Attractor->dbuf,
				GCForeground | GCBackground | GCGraphicsExposures | GCFunction,
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

	MI_CLEARWINDOW(mi);

	/* Do not want any exposure events from XCopyPlane */
	XSetGraphicsExposures(display, MI_GC(mi), False);
}

/***************************************************************/

void
release_strange(ModeInfo * mi)
{
	if (Root != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); ++screen)
			free_strange(MI_DISPLAY(mi), &Root[screen]);
		(void) free((void *) Root);
		Root = (ATTRACTOR *) NULL; 
	}
}

#endif /* MODE_strange */
