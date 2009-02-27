/* -*- Mode: C; tab-width: 4 -*-
 * ifs --- Modified iterated functions system.
 */
#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)ifs.c	   4.02 97/04/01 xlockmore";
#endif

/* Copyright (c) 1997 by Massimino Pascal (Pascal.Massimon@ens.fr)
 *
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
 * 10-May-97: jwz@netscape.com: turned into a standalone program.
 *			  Made it render into an offscreen bitmap and then copy
 *			  that onto the screen, to reduce flicker.
 */

#ifdef STANDALONE
# define PROGCLASS					"IFS"
# define HACK_INIT					init_ifs
# define HACK_DRAW					draw_ifs
# define ifs_opts					xlockmore_opts
# define DEFAULTS	"*delay:		20000 \n"			\
					"*ncolors:		100   \n"
# define SMOOTH_COLORS
# include "xlockmore.h"				/* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"					/* from the xlockmore distribution */
#endif /* !STANDALONE */

ModeSpecOpt ifs_opts = {
  0, NULL, 0, NULL, NULL };

/*****************************************************/
/*****************************************************/

typedef float DBL;
typedef short int F_PT;

/* typedef float               F_PT; */

/*****************************************************/

#define FIX 12
#define UNIT   ( 1<<FIX )
#define MAX_SIMI  6

   /* settings for a PC 120Mhz... */
#define MAX_DEPTH_2  10
#define MAX_DEPTH_3  6
#define MAX_DEPTH_4  4
#define MAX_DEPTH_5  3

#define DBL_To_F_PT(x)  (F_PT)( (DBL)(UNIT)*(x) )

/*****************************************************/

static int  Max_Colors;
static ModeInfo *The_MI;
static F_PT Lx, Ly;
static int  D;
static Display *display;
static GC   gc;
static Window window;

/*****************************************************/

typedef struct Similitude_Struct SIMI;
typedef struct Fractal_Struct FRACTAL;

struct Similitude_Struct {

	DBL         c_x, c_y;
	DBL         r, r2, A, A2;
	F_PT        Ct, St, Ct2, St2;
	F_PT        Cx, Cy;
	F_PT        R, R2;
};

struct Fractal_Struct {

	int         Nb_Simi;
	SIMI        Components[5 * MAX_SIMI];
	int         Depth, Col;
	int         Count, Speed;
	int         Width, Height, Lx, Ly;
	DBL         r_mean, dr_mean, dr2_mean;
	int         Cur_Pt, Max_Pt;
	XPoint     *Buffer1, *Buffer2;
    Pixmap      dbuf;	/* jwz */
    GC          dbuf_gc;
};

static FRACTAL *Root = NULL, *Cur_F;
static XPoint *Buf;
static int  Cur_Pt;


/*****************************************************/
/*****************************************************/

static      DBL
Gauss_Rand(DBL c, DBL A, DBL S)
{
	DBL         y;

	y = (DBL) LRAND() / MAXRAND;
	y = A * (1.0 - exp(-y * y * S)) / (1.0 - exp(-S));
	if (NRAND(2))
		return (c + y);
	return (c - y);
}

static      DBL
Half_Gauss_Rand(DBL c, DBL A, DBL S)
{
	DBL         y;

	y = (DBL) LRAND() / MAXRAND;
	y = A * (1.0 - exp(-y * y * S)) / (1.0 - exp(-S));
	return (c + y);
}

static void
Random_Simis(FRACTAL * F, SIMI * Cur, int i)
{
	while (i--) {
		Cur->c_x = Gauss_Rand(0.0, .8, 4.0);
		Cur->c_y = Gauss_Rand(0.0, .8, 4.0);
		Cur->r = Gauss_Rand(F->r_mean, F->dr_mean, 3.0);
		Cur->r2 = Half_Gauss_Rand(0.0, F->dr2_mean, 2.0);
		Cur->A = Gauss_Rand(0.0, 360.0, 4.0) * (M_PI / 180.0);
		Cur->A2 = Gauss_Rand(0.0, 360.0, 4.0) * (M_PI / 180.0);
		Cur++;
	}
}

/***************************************************************/

void
init_ifs(ModeInfo * mi)
{
	int         i;
	FRACTAL    *Fractal;

	if (Root == NULL) {
		Root = (FRACTAL *) calloc(
				       MI_NUM_SCREENS(mi), sizeof (FRACTAL));
		if (Root == NULL)
			return;
	}
	Fractal = &Root[MI_SCREEN(mi)];

	if (Fractal->Max_Pt) {
		free(Fractal->Buffer1);
		free(Fractal->Buffer2);
	}
	i = (NRAND(4)) + 2;	/* Number of centers */
	switch (i) {
		case 3:
			Fractal->Depth = MAX_DEPTH_3;
			Fractal->r_mean = .6;
			Fractal->dr_mean = .4;
			Fractal->dr2_mean = .3;
			break;

		case 4:
			Fractal->Depth = MAX_DEPTH_4;
			Fractal->r_mean = .5;
			Fractal->dr_mean = .4;
			Fractal->dr2_mean = .3;
			break;

		case 5:
			Fractal->Depth = MAX_DEPTH_5;
			Fractal->r_mean = .5;
			Fractal->dr_mean = .4;
			Fractal->dr2_mean = .3;
			break;

		default:
		case 2:
			Fractal->Depth = MAX_DEPTH_2;
			Fractal->r_mean = .7;
			Fractal->dr_mean = .3;
			Fractal->dr2_mean = .4;
			break;
	}
	/* (void) fprintf( stderr, "N=%d\n", i ); */
	Fractal->Nb_Simi = i;
	Fractal->Max_Pt = Fractal->Nb_Simi - 1;
	for (i = 0; i <= Fractal->Depth + 2; ++i)
		Fractal->Max_Pt *= Fractal->Nb_Simi;

	Fractal->Buffer1 = (XPoint *) calloc(Fractal->Max_Pt, sizeof (XPoint));
	if (Fractal->Buffer1 == NULL)
		goto Abort;
	Fractal->Buffer2 = (XPoint *) calloc(Fractal->Max_Pt, sizeof (XPoint));
	if (Fractal->Buffer2 == NULL)
		goto Abort;

	Fractal->Speed = 6;
	Fractal->Width = MI_WIN_WIDTH(mi);
	Fractal->Height = MI_WIN_HEIGHT(mi);
	Fractal->Cur_Pt = 0;
	Fractal->Count = 0;
	Fractal->Lx = (Fractal->Width - 1) / 2;
	Fractal->Ly = (Fractal->Height - 1) / 2;
	Fractal->Col = NRAND(MI_NPIXELS(mi) - 1) + 1;

	Random_Simis(Fractal, Fractal->Components, 5 * MAX_SIMI);

	Fractal->dbuf = XCreatePixmap(MI_DISPLAY(mi), MI_WINDOW(mi),
								  Fractal->Width, Fractal->Height, 1);
	if (Fractal->dbuf)
	  {
		XGCValues gcv;
		gcv.foreground = 0;
		gcv.background = 0;
		gcv.function = GXcopy;
		Fractal->dbuf_gc = XCreateGC(MI_DISPLAY(mi), Fractal->dbuf,
									 GCForeground|GCBackground|GCFunction,
									 &gcv);
		XFillRectangle(MI_DISPLAY(mi), Fractal->dbuf,
					   Fractal->dbuf_gc, 0,0, Fractal->Width, Fractal->Height);

		XSetBackground(MI_DISPLAY(mi), MI_GC(mi), MI_WIN_BLACK_PIXEL(mi));
		XSetFunction(MI_DISPLAY(mi), MI_GC(mi), GXcopy);
	  }

	XClearWindow(MI_DISPLAY(mi), MI_WINDOW(mi));
	return;

      Abort:
	if (Fractal->Buffer1 != NULL)
		free(Fractal->Buffer1);
	if (Fractal->Buffer2 != NULL)
		free(Fractal->Buffer2);
	Fractal->Buffer1 = NULL;
	Fractal->Buffer2 = NULL;
	Fractal->Max_Pt = 0;
	return;
}


/***************************************************************/

#ifndef __GNUC__
# define inline /* */
#endif

static inline void
Transform(SIMI * Simi, F_PT xo, F_PT yo, F_PT * x, F_PT * y)
{
	F_PT        xx, yy;

	xo = xo - Simi->Cx;
	xo = (xo * Simi->R) / UNIT;
	yo = yo - Simi->Cy;
	yo = (yo * Simi->R) / UNIT;

	xx = xo - Simi->Cx;
	xx = (xx * Simi->R2) / UNIT;
	yy = -yo - Simi->Cy;
	yy = (yy * Simi->R2) / UNIT;

	*x = ((xo * Simi->Ct - yo * Simi->St + xx * Simi->Ct2 - yy * Simi->St2) / UNIT) + Simi->Cx;
	*y = ((xo * Simi->St + yo * Simi->Ct + xx * Simi->St2 + yy * Simi->Ct2) / UNIT) + Simi->Cy;
}

/***************************************************************/

static void
Trace(F_PT xo, F_PT yo)
{
	F_PT        x, y, i;
	SIMI       *Cur;

	Cur = Cur_F->Components;
	for (i = Cur_F->Nb_Simi; i; --i, Cur++) {
		Transform(Cur, xo, yo, &x, &y);
		Buf->x = Lx + (x * Lx / (UNIT * 2));
		Buf->y = Ly - (y * Ly / (UNIT * 2));
		Buf++;
		Cur_Pt++;

		if (D && ((x - xo) >> 4) && ((y - yo) >> 4)) {
			D--;
			Trace(x, y);
			D++;
		}
	}
}

static void
Draw_Fractal(FRACTAL * F)
{
	int         i, j;
	F_PT        x, y, xo, yo;
	SIMI       *Cur, *Simi;

	for (Cur = F->Components, i = F->Nb_Simi; i; --i, Cur++) {
		Cur->Cx = DBL_To_F_PT(Cur->c_x);
		Cur->Cy = DBL_To_F_PT(Cur->c_y);

		Cur->Ct = DBL_To_F_PT(cos(Cur->A));
		Cur->St = DBL_To_F_PT(sin(Cur->A));
		Cur->Ct2 = DBL_To_F_PT(cos(Cur->A2));
		Cur->St2 = DBL_To_F_PT(sin(Cur->A2));

		Cur->R = DBL_To_F_PT(Cur->r);
		Cur->R2 = DBL_To_F_PT(Cur->r2);
	}


	Cur_Pt = 0;
	Cur_F = F;
	Buf = F->Buffer2;
	Lx = F->Lx;
	Ly = F->Ly;
	D = F->Depth;
	for (Cur = F->Components, i = F->Nb_Simi; i; --i, Cur++) {
		xo = Cur->Cx;
		yo = Cur->Cy;
		for (Simi = F->Components, j = F->Nb_Simi; j; --j, Simi++) {
			if (Simi == Cur)
				continue;
			Transform(Simi, xo, yo, &x, &y);
			Trace(x, y);
		}
	}

	/* Erase previous */

	if (F->Cur_Pt) {
		XSetForeground(display, gc, MI_WIN_BLACK_PIXEL(The_MI));
		if (F->dbuf)	/* jwz */
		  {
			XSetForeground(display, F->dbuf_gc, 0);
/*		    XDrawPoints(display, F->dbuf, F->dbuf_gc, F->Buffer1, F->Cur_Pt,
					    CoordModeOrigin); */
		    XFillRectangle(display, F->dbuf, F->dbuf_gc, 0, 0,
						   F->Width, F->Height);
		  }
		else
		  XDrawPoints(display, window, gc, F->Buffer1, F->Cur_Pt,
					  CoordModeOrigin);
	}
	if (Max_Colors < 2)
		XSetForeground(display, gc, MI_WIN_WHITE_PIXEL(The_MI));
	else
		XSetForeground(display, gc, MI_PIXEL(The_MI, F->Col % Max_Colors));
	if (Cur_Pt) {
	  if (F->dbuf)
		{
		  XSetForeground(display, F->dbuf_gc, 1);
		  XDrawPoints(display, F->dbuf, F->dbuf_gc, F->Buffer2, Cur_Pt,
					  CoordModeOrigin);
		}
	  else
		XDrawPoints(display, window, gc, F->Buffer2, Cur_Pt, CoordModeOrigin);
	}

	if (F->dbuf)
	  XCopyPlane(display, F->dbuf, window, gc, 0,0,F->Width,F->Height,0,0, 1);

	F->Cur_Pt = Cur_Pt;
	Buf = F->Buffer1;
	F->Buffer1 = F->Buffer2;
	F->Buffer2 = Buf;
}


void
draw_ifs(ModeInfo * mi)
{
	int         i;
	FRACTAL    *F;
	DBL         u, uu, v, vv, u0, u1, u2, u3;
	SIMI       *S, *S1, *S2, *S3, *S4;

	The_MI = mi;
	display = MI_DISPLAY(mi);
	window = MI_WINDOW(mi);
	gc = MI_GC(mi);
	Max_Colors = MI_NPIXELS(mi);

	F = &Root[MI_SCREEN(mi)];

	u = (DBL) (F->Count) * (DBL) (F->Speed) / 1000.0;
	uu = u * u;
	v = 1.0 - u;
	vv = v * v;
	u0 = vv * v;
	u1 = 3.0 * vv * u;
	u2 = 3.0 * v * uu;
	u3 = u * uu;

	S = F->Components;
	S1 = S + F->Nb_Simi;
	S2 = S1 + F->Nb_Simi;
	S3 = S2 + F->Nb_Simi;
	S4 = S3 + F->Nb_Simi;

	for (i = F->Nb_Simi; i; --i, S++, S1++, S2++, S3++, S4++) {
		S->c_x = u0 * S1->c_x + u1 * S2->c_x + u2 * S3->c_x + u3 * S4->c_x;
		S->c_y = u0 * S1->c_y + u1 * S2->c_y + u2 * S3->c_y + u3 * S4->c_y;
		S->r = u0 * S1->r + u1 * S2->r + u2 * S3->r + u3 * S4->r;
		S->r2 = u0 * S1->r2 + u1 * S2->r2 + u2 * S3->r2 + u3 * S4->r2;
		S->A = u0 * S1->A + u1 * S2->A + u2 * S3->A + u3 * S4->A;
		S->A2 = u0 * S1->A2 + u1 * S2->A2 + u2 * S3->A2 + u3 * S4->A2;
	}

	Draw_Fractal(F);

	if (F->Count >= 1000 / F->Speed) {
		S = F->Components;
		S1 = S + F->Nb_Simi;
		S2 = S1 + F->Nb_Simi;
		S3 = S2 + F->Nb_Simi;
		S4 = S3 + F->Nb_Simi;

		for (i = F->Nb_Simi; i; --i, S++, S1++, S2++, S3++, S4++) {
			S2->c_x = 2.0 * S4->c_x - S3->c_x;
			S2->c_y = 2.0 * S4->c_y - S3->c_y;
			S2->r = 2.0 * S4->r - S3->r;
			S2->r2 = 2.0 * S4->r2 - S3->r2;
			S2->A = 2.0 * S4->A - S3->A;
			S2->A2 = 2.0 * S4->A2 - S3->A2;

			*S1 = *S4;
		}
		Random_Simis(F, F->Components + 3 * F->Nb_Simi, F->Nb_Simi);

		Random_Simis(F, F->Components + 4 * F->Nb_Simi, F->Nb_Simi);

		F->Count = 0;
	} else
		F->Count++;

	F->Col++;
}


/***************************************************************/

void
release_ifs(ModeInfo * mi)
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
}
