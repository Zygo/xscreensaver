/* -*- Mode: C; tab-width: 4 -*-
 * strange --- Strange attractors are not so hard to find...
 */
#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)strange.c	   4.02 97/04/01 xlockmore";
#endif

/* Copyright (c) 1997 by Massimino Pascal (Pascal.Massimon@ens.fr)
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
 * 30-Jul-98: sineswiper@resonatorsoft.com: added curve factor (discovered
 *         while experimenting with the Gauss_Rand function).
 * 10-May-97: jwz@jwz.org: turned into a standalone program.
 *			  Made it render into an offscreen bitmap and then copy
 *			  that onto the screen, to reduce flicker.
 */

#ifdef STANDALONE
# define PROGCLASS					"Strange"
# define HACK_INIT					init_strange
# define HACK_DRAW					draw_strange
# define strange_opts				xlockmore_opts
# define DEFAULTS	"*delay:		2000  \n"			\
					"*ncolors:		100   \n"
# define SMOOTH_COLORS
# include "xlockmore.h"				/* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"					/* from the xlockmore distribution */
#endif /* !STANDALONE */

/*****************************************************/
/*****************************************************/

typedef float DBL;
typedef int PRM;

#define UNIT (1<<12)
#define UNIT2 (1<<14)
/* #define UNIT2 (3140*UNIT/1000) */

#define SKIP_FIRST      100
#define MAX_POINTS      5500
#define DBL_To_PRM(x)  (PRM)( (DBL)(UNIT)*(x) )


#define DO_FOLD(a) (a)<0 ? -Fold[ (-(a))&(UNIT2-1) ] : Fold[ (a)&(UNIT2-1) ]

/* 
   #define DO_FOLD(a) (a)<-UNIT2 ? -Fold[(-(a))%UNIT2] : (a)<0 ? -Fold[ -(a) ] 

   :  \ (a)>UNIT2 ? Fold[ (a)%UNIT2 ] : Fold[ (a) ] */
/* #define DO_FOLD(a) DBL_To_PRM( sin( (DBL)(a)/UNIT ) ) */
/* 
   #define DO_FOLD(a) (a)<0 ? DBL_To_PRM( exp( 16.0*(a)/UNIT2 ) )-1.0 : \
   DBL_To_PRM( 1.0-exp( -16.0*(a)/UNIT2 ) ) */

/******************************************************************/

#define MAX_PRM 3*5

typedef struct {
	DBL         Prm1[MAX_PRM], Prm2[MAX_PRM];
	void        (*Iterate) (PRM, PRM, PRM *, PRM *);
	XPoint     *Buffer1, *Buffer2;
	int         Cur_Pt, Max_Pt;
	int         Col, Count, Speed;
	int         Width, Height;
    Pixmap      dbuf;	/* jwz */
    GC          dbuf_gc;
} ATTRACTOR;

static ATTRACTOR *Root;
static PRM  xmin, xmax, ymin, ymax;
static PRM  Prm[MAX_PRM];
static PRM *Fold = NULL;

static int curve;

static XrmOptionDescRec opts[] =
{
	{"-curve", ".strange.curve", XrmoptionSepArg, (caddr_t) "10"},
};
static OptionStruct desc[] =
{
	{"-curve", "set the curve factor of the attractors"},
};

ModeSpecOpt strange_opts = { 1, opts, 0, NULL, desc };

/******************************************************************/
/******************************************************************/

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
	DBL         y,z;

	y = (DBL) LRAND() / MAXRAND;
	z = curve / 10;
  	y = A * (z - exp(-y * y * S)) / (z - exp(-S));
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
Iterate_X2(PRM x, PRM y, PRM * xo, PRM * yo)
{
	PRM         xx, yy, xy, x2y, y2x, Tmp;

	xx = (x * x) / UNIT;
	x2y = (xx * y) / UNIT;
	yy = (y * y) / UNIT;
	y2x = (yy * x) / UNIT;
	xy = (x * y) / UNIT;

	Tmp = Prm[1] * xx + Prm[2] * xy + Prm[3] * yy + Prm[4] * x2y;
	Tmp = Prm[0] - y + (Tmp / UNIT);
	*xo = DO_FOLD(Tmp);
	Tmp = Prm[6] * xx + Prm[7] * xy + Prm[8] * yy + Prm[9] * y2x;
	Tmp = Prm[5] + x + (Tmp / UNIT);
	*yo = DO_FOLD(Tmp);
}

static void
Iterate_X3(PRM x, PRM y, PRM * xo, PRM * yo)
{
	PRM         xx, yy, xy, x2y, y2x, Tmp_x, Tmp_y, Tmp_z;

	xx = (x * x) / UNIT;
	x2y = (xx * y) / UNIT;
	yy = (y * y) / UNIT;
	y2x = (yy * x) / UNIT;
	xy = (x * y) / UNIT;

	Tmp_x = Prm[1] * xx + Prm[2] * xy + Prm[3] * yy + Prm[4] * x2y;
	Tmp_x = Prm[0] - y + (Tmp_x / UNIT);
	Tmp_x = DO_FOLD(Tmp_x);

	Tmp_y = Prm[6] * xx + Prm[7] * xy + Prm[8] * yy + Prm[9] * y2x;
	Tmp_y = Prm[5] + x + (Tmp_y / UNIT);

	Tmp_y = DO_FOLD(Tmp_y);

	Tmp_z = Prm[11] * xx + Prm[12] * xy + Prm[13] * yy + Prm[14] * y2x;
	Tmp_z = Prm[10] + x + (Tmp_z / UNIT);
	Tmp_z = UNIT + Tmp_z * Tmp_z / UNIT;

	*xo = (Tmp_x * UNIT) / Tmp_z;
	*yo = (Tmp_y * UNIT) / Tmp_z;
}

static void (*Funcs[2]) (PRM, PRM, PRM *, PRM *) = {
	Iterate_X2, Iterate_X3
};

/***************************************************************/

void
draw_strange(ModeInfo * mi)
{
	int         i, j, n, Max_Colors, Cur_Pt;
	PRM         x, y, xo, yo;
	DBL         u;
	ATTRACTOR  *A;
	XPoint     *Buf;
	Display    *display;
	GC          gc;
	Window      window;
	DBL         Lx, Ly;
	void        (*Iterate) (PRM, PRM, PRM *, PRM *);

	display = MI_DISPLAY(mi);
	window = MI_WINDOW(mi);
	gc = MI_GC(mi);
	Max_Colors = MI_NPIXELS(mi);

	A = &Root[MI_SCREEN(mi)];

	Cur_Pt = A->Cur_Pt;
	Iterate = A->Iterate;

	u = (DBL) (A->Count) / 1000.0;
	for (j = MAX_PRM - 1; j >= 0; --j)
		Prm[j] = DBL_To_PRM((1.0 - u) * A->Prm1[j] + u * A->Prm2[j]);

	x = y = DBL_To_PRM(.0);
	for (n = SKIP_FIRST; n; --n) {
		(*Iterate) (x, y, &xo, &yo);
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
		(*Iterate) (x, y, &xo, &yo);
		Buf->x = (short) (Lx * (x + DBL_To_PRM(1.1)));
		Buf->y = (short) (Ly * (DBL_To_PRM(1.1) - y));
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

	XSetForeground(display, gc, MI_WIN_BLACK_PIXEL(mi));

	if (A->dbuf)	/* jwz */
	  {
		XSetForeground(display, A->dbuf_gc, 0);
/*	    XDrawPoints(display, A->dbuf, A->dbuf_gc, A->Buffer1,
				    Cur_Pt,CoordModeOrigin);*/
		XFillRectangle(display, A->dbuf, A->dbuf_gc, 0,0, A->Width, A->Height);
	  }
	else
	  XDrawPoints(display, window, gc, A->Buffer1, Cur_Pt, CoordModeOrigin);

	if (Max_Colors < 2)
		XSetForeground(display, gc, MI_WIN_WHITE_PIXEL(mi));
	else
		XSetForeground(display, gc, MI_PIXEL(mi, A->Col % Max_Colors));

	if (A->dbuf)
	  {
		XSetForeground(display, A->dbuf_gc, 1);
		XDrawPoints(display, A->dbuf, A->dbuf_gc, A->Buffer2, A->Cur_Pt,
					CoordModeOrigin);
	  }
	else
	  XDrawPoints(display, window, gc, A->Buffer2, A->Cur_Pt, CoordModeOrigin);

	if (A->dbuf)
	  XCopyPlane(display, A->dbuf, window, gc, 0,0,A->Width,A->Height,0,0, 1);

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
	ATTRACTOR  *Attractor;

	curve = get_integer_resource ("curve", "Integer");
	if (curve <= 0) curve = 10;

	if (Root == NULL) {
		Root = (ATTRACTOR *) calloc(
				     MI_NUM_SCREENS(mi), sizeof (ATTRACTOR));
		if (Root == NULL)
			return;
	}
	if (Fold == NULL) {
		int         i;

		Fold = (PRM *) calloc(UNIT2 + 1, sizeof (PRM));
		if (Fold == NULL)
			return;
		for (i = 0; i <= UNIT2; ++i) {
			DBL         x;

			/* x = ( DBL )(i)/UNIT2; */
			/* x = sin( M_PI/2.0*x ); */
			/* x = sqrt( x ); */
			/* x = x*x; */
			/* x = x*(1.0-x)*4.0; */
			x = (DBL) (i) / UNIT;
			x = sin(x);
			Fold[i] = DBL_To_PRM(x);
		}
	}
	Attractor = &Root[MI_SCREEN(mi)];

	Attractor->Buffer1 = (XPoint *) calloc(MAX_POINTS, sizeof (XPoint));
	if (Attractor->Buffer1 == NULL)
		goto Abort;
	Attractor->Buffer2 = (XPoint *) calloc(MAX_POINTS, sizeof (XPoint));
	if (Attractor->Buffer2 == NULL)
		goto Abort;
	Attractor->Max_Pt = MAX_POINTS;

	Attractor->Width = MI_WIN_WIDTH(mi);
	Attractor->Height = MI_WIN_HEIGHT(mi);
	Attractor->Cur_Pt = 0;
	Attractor->Count = 0;
	Attractor->Col = NRAND(MI_NPIXELS(mi));
	Attractor->Speed = 4;

	Attractor->Iterate = Funcs[NRAND(2)];
	Random_Prm(Attractor->Prm1);
	Random_Prm(Attractor->Prm2);

	Attractor->dbuf = XCreatePixmap(MI_DISPLAY(mi), MI_WINDOW(mi),
									Attractor->Width, Attractor->Height, 1);
	if (Attractor->dbuf)
	  {
		XGCValues gcv;
		gcv.foreground = 0;
		gcv.background = 0;
		gcv.function = GXcopy;
		Attractor->dbuf_gc = XCreateGC(MI_DISPLAY(mi), Attractor->dbuf,
									   GCForeground|GCBackground|GCFunction,
									   &gcv);
		XFillRectangle(MI_DISPLAY(mi), Attractor->dbuf,
					   Attractor->dbuf_gc, 0,0, Attractor->Width,
					   Attractor->Height);
		XSetBackground(MI_DISPLAY(mi), MI_GC(mi), MI_WIN_BLACK_PIXEL(mi));
		XSetFunction(MI_DISPLAY(mi), MI_GC(mi), GXcopy);
	  }

	XClearWindow(MI_DISPLAY(mi), MI_WINDOW(mi));
	return;

      Abort:
	if (Attractor->Buffer1 != NULL)
		free(Attractor->Buffer1);
	if (Attractor->Buffer2 != NULL)
		free(Attractor->Buffer2);
	Attractor->Buffer1 = NULL;
	Attractor->Buffer2 = NULL;
	Attractor->Cur_Pt = 0;
	return;
}

/***************************************************************/

void
release_strange(ModeInfo * mi)
{
	int         i;

	if (Root == NULL)
		return;

	for (i = 0; i < MI_NUM_SCREENS(mi); ++i) {
		if (Root[i].Buffer1 != NULL)
			free(Root[i].Buffer1);
		if (Root[i].Buffer2 != NULL)
			free(Root[i].Buffer2);
		if (Root[i].dbuf)
		    XFreePixmap(MI_DISPLAY(mi), Root[i].dbuf);
		if (Root[i].dbuf_gc)
		    XFreeGC(MI_DISPLAY(mi), Root[i].dbuf_gc);
	}
	free(Root);
	Root = NULL;
	if (Fold != NULL)
		free(Fold);
	Fold = NULL;
}
