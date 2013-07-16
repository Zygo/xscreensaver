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
* 01-Nov-2000: Allocation checks
* 10-May-1997: jwz@jwz.org: turned into a standalone program.
*              Made it render into an offscreen bitmap and then copy
*              that onto the screen, to reduce flicker.
*
* strange attractors are not so hard to find...
*/

/* TODO: Bring over tweaks from 3.x version.
* For a good idea of what's missing, diff strange.c.20081107-good2 against strange.c-3.29
* We forked from the 3.29 series at 20081107, so anything added since then may be missing.
*/

#ifdef STANDALONE
# define MODE_strange
# define DEFAULTS	"*delay: 10000 \n" \
					"*ncolors: 100 \n" \
					"*fpsSolid: True \n" \
					"*ignoreRotation: True \n" \

# define SMOOTH_COLORS
# define refresh_strange 0
# define strange_handle_event 0
# include "xlockmore.h"		/* from the xscreensaver distribution */
#else /* !STANDALONE */
# include "xlock.h"		/* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef MODE_strange
#define DEF_CURVE  "10"
#define DEF_POINTS "5500"

/*static int curve;*/
static int points;

static XrmOptionDescRec opts[] =
{
/*		{"-curve",  ".strange.curve",  XrmoptionSepArg, 0}, */
		{"-points", ".strange.points", XrmoptionSepArg, 0},
};
static argtype vars[] =
{
/*		{&curve,  "curve",  "Curve",  DEF_CURVE,  t_Int},*/
		{&points, "points", "Points", DEF_POINTS, t_Int},
};
static OptionStruct desc[] =
{
/*		{"-curve", "set the curve factor of the attractors"},*/
		{"-points", "change the number of points/iterations each frame"},
};
ENTRYPOINT ModeSpecOpt strange_opts =
{sizeof opts / sizeof opts[0], opts,
sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   strange_description =
{"strange", "init_strange", "draw_strange", "release_strange",
"init_strange", "init_strange", (char *) NULL, &strange_opts,
1000, 1, 1, 1, 64, 1.0, "",
"Shows strange attractors", 0, NULL};
#endif

#ifdef HAVE_COCOA
# define NO_DBUF
#endif

typedef float DBL;
typedef int PRM;

#define UNIT (1<<12)
#define UNIT2 (1<<14)
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
#define useAccumulator (Root->Max_Pt > 6000)
#define ACC_GAMMA 10.0
#define NUM_COLS 150
/* Extra options: */
#define VARY_SPEED_TO_AVOID_BOREDOM
#define POINTS_HISTORY
#define MERGE_FRAMES 3

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
	#ifdef useAccumulator
		int **accMap;
	#endif
} ATTRACTOR;

static ATTRACTOR *Root = (ATTRACTOR *) NULL; 

#ifdef useAccumulator
static XColor* cols;
#endif

#ifdef POINTS_HISTORY
static int numOldPoints;
static int* oldPointsX;
static int* oldPointsY;
static int oldPointsIndex;
static int startedClearing;
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

ENTRYPOINT void
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

	u = (DBL) (A->Count) / 40000.0;
	for (j = MAX_PRM - 1; j >= 0; --j)
		A->Prm[j] = DBL_To_PRM((1.0 - u) * A->Prm1[j] + u * A->Prm2[j]);

	/* We collect the accumulation of the orbits in the 2d int array field. */
#ifndef POINTS_HISTORY
	#ifdef useAccumulator
		if (useAccumulator) {
			for (i=0;i<A->Width;i++) {
				for (j=0;j<A->Height;j++) {
					A->accMap[i][j] = 0;

				}
			}
		}
	#endif
#endif

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
		#ifdef useAccumulator
		if (useAccumulator) {
			int mx,my;
			mx = (short) ( A->Width*0.1 + A->Width*0.8 * (xo - xmin) / (xmax - xmin) );
			my = (short) ( A->Width*0.1 + (A->Height - A->Width*0.2) * (yo - ymin) / (ymax - ymin) );
			if (mx>=0 && my>=0 && mx<A->Width && my<A->Height) {
				A->accMap[mx][my]++;
			}
#ifdef POINTS_HISTORY
		/* #define clearOldPoint(i) { if (startedClearing) { field[oldPoints[i].x][oldPoints[i].y]--; } }
		#define saveUnplot(X,Y) { clearOldPoint(oldPointsIndex) oldPoints[oldPointsIndex].x = X; oldPoints[oldPointsIndex].y = Y; oldPointsIndex = (oldPointsIndex + 1) % numOldPoints; if (oldPointsIndex==0) { startedClearing=1; } }
		saveUnplot(mx,my) */
		if (startedClearing) {
			int oldX = oldPointsX[oldPointsIndex];
			int oldY = oldPointsY[oldPointsIndex];
			if (oldX>=0 && oldY>=0 && oldX<A->Width && oldY<A->Height) {
				A->accMap[oldX][oldY]--;
			}
		}
		oldPointsX[oldPointsIndex] = mx;
		oldPointsY[oldPointsIndex] = my;
		oldPointsIndex = (oldPointsIndex + 1) % numOldPoints;
		if (oldPointsIndex==0) { startedClearing=1; }
#endif
		} else {
		#endif
			Buf->x = (int) (Lx * (x + DBL_To_PRM(1.1)));
			Buf->y = (int) (Ly * (DBL_To_PRM(1.1) - y));
			Buf++;
			A->Cur_Pt++;
		#ifdef useAccumulator
		}
		#endif
		/* (void) fprintf( stderr, "X,Y: %d %d    ", Buf->x, Buf->y ); */
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

	#ifdef useAccumulator
	if (useAccumulator) {
		float colorScale;
		int col;
		#ifdef VARY_SPEED_TO_AVOID_BOREDOM
		int pixelCount = 0;
		#endif
		colorScale = (A->Width*A->Height/640.0/480.0*800000.0/(float)A->Max_Pt*(float)NUM_COLS/256);
		if (A->dbuf != None) {
			XSetForeground(display, A->dbuf_gc, 0);
			XFillRectangle(display, A->dbuf, A->dbuf_gc, 0, 0, A->Width, A->Height);
		} else {
			XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
			XFillRectangle(display, window, gc, 0, 0, A->Width, A->Height);
		}
		for (i=0;i<A->Width;i++) {
			for (j=0;j<A->Height;j++) {
				if (A->accMap[i][j]>0) {
					col = (float)A->accMap[i][j] * colorScale;
					if (col>NUM_COLS-1) {
						col = NUM_COLS-1;
					}
					#ifdef VARY_SPEED_TO_AVOID_BOREDOM
					if (col>0) {
						if (col<NUM_COLS-1)  /* we don't count maxxed out pixels */
							pixelCount++;
					}
					#endif
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
			}
		}
		if (A->dbuf != None) {
			XCopyArea(display, A->dbuf, window, gc, 0, 0, A->Width, A->Height, 0, 0);
		}
		#ifdef VARY_SPEED_TO_AVOID_BOREDOM
			/* Increaase the rate of change of the parameters if the attractor has become visually boring. */
			if ((xmax - xmin < DBL_To_PRM(.2)) && (ymax - ymin < DBL_To_PRM(.2))) {
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

	#ifdef useAccumulator
	}
	#endif

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
    mi->recursion_depth = A->Count;
}


/***************************************************************/

ENTRYPOINT void
init_strange(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
#ifndef NO_DBUF
	GC          gc = MI_GC(mi);
#endif
	ATTRACTOR  *Attractor;

#ifdef POINTS_HISTORY
	startedClearing=0;
	oldPointsIndex=0;
#endif

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

	Attractor->Max_Pt = points;

	if (Attractor->Buffer1 == NULL)
		if ((Attractor->Buffer1 = (XPoint *) calloc(Attractor->Max_Pt,
				sizeof (XPoint))) == NULL) {
			free_strange(display, Attractor);
			return;
		}
	if (Attractor->Buffer2 == NULL)
		if ((Attractor->Buffer2 = (XPoint *) calloc(Attractor->Max_Pt,
				sizeof (XPoint))) == NULL) {
			free_strange(display, Attractor);
			return;
		}

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
#ifdef useAccumulator
#define colorDepth ( useAccumulator ? MI_DEPTH(mi) : 1 )
#else
#define colorDepth 1
#endif
	Attractor->dbuf = XCreatePixmap(display, window,
	     Attractor->Width, Attractor->Height, colorDepth);
	/* Allocation checked */
	if (Attractor->dbuf != None) {
		XGCValues   gcv;

		gcv.foreground = 0;
		gcv.background = 0;
#ifndef HAVE_COCOA
		gcv.graphics_exposures = False;
#endif /* HAVE_COCOA */
		gcv.function = GXcopy;

		if (Attractor->dbuf_gc != None)
			XFreeGC(display, Attractor->dbuf_gc);

		if ((Attractor->dbuf_gc = XCreateGC(display, Attractor->dbuf,
#ifndef HAVE_COCOA
				GCGraphicsExposures |
#endif /* HAVE_COCOA */
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
		XWindowAttributes xgwa;
		int i,j;
		XGetWindowAttributes (display, window, &xgwa);
		/* cmap = xgwa.colormap; */
		/* cmap = XCreateColormap(display, window, MI_VISUAL(mi), AllocAll); */
		Attractor->accMap = (int**)calloc(Attractor->Width,sizeof(int*));
		for (i=0;i<Attractor->Width;i++) {
			Attractor->accMap[i] = (int*)calloc(Attractor->Height,sizeof(int));
			for (j=0;j<Attractor->Height;j++) {
				Attractor->accMap[i][j] = 0;
			}
		}
#ifdef POINTS_HISTORY
		numOldPoints = A->Max_Pt * MERGE_FRAMES;
		oldPointsX = (int*)calloc(numOldPoints,sizeof(int));
		oldPointsY = (int*)calloc(numOldPoints,sizeof(int));
#endif
		cols = (XColor*)calloc(NUM_COLS,sizeof(XColor));
		for (i=0;i<NUM_COLS;i++) {
			float li;
			#define MINBLUE 1
			#define FULLBLUE 128
			li = MINBLUE + (255.0-MINBLUE) * log(1.0 + ACC_GAMMA*(float)i/NUM_COLS) / log(1.0 + ACC_GAMMA);
			if (li<FULLBLUE) {
				cols[i].red = 0;
				cols[i].green = 0;
				cols[i].blue = 65536*li/FULLBLUE;
			} else {
				cols[i].red = 65536*(li-FULLBLUE)/(256-FULLBLUE);
				cols[i].green = 65536*(li-FULLBLUE)/(256-FULLBLUE);
				cols[i].blue = 65535;
			}
			XAllocColor (display, xgwa.colormap, &cols[i]);
			/*
			if (!XAllocColor(MI_DISPLAY(mi), cmap, &cols[i])) {
			if (!XAllocColor(display, cmap, &cols[i])) {
				cols[i].pixel = WhitePixel (display, DefaultScreen (display));
				cols[i].red = cols[i].green = cols[i].blue = 0xFFFF;
			}
			*/
		}
		/*
		XSetWindowColormap(display, window, cmap);
		(void) XSetWMColormapWindows(display, window, &window, 1);
		XInstallColormap(display, cmap);
		XStoreColors(display, cmap, cols, 256);
		*/
	}
	#undef A
#endif
	MI_CLEARWINDOW(mi);

	/* Do not want any exposure events from XCopyPlane */
	XSetGraphicsExposures(display, MI_GC(mi), False);
}

ENTRYPOINT void
reshape_strange(ModeInfo * mi, int width, int height)
{
  XClearWindow (MI_DISPLAY (mi), MI_WINDOW(mi));
  init_strange (mi);
}

/***************************************************************/

ENTRYPOINT void
release_strange(ModeInfo * mi)
{
	if (Root != NULL) {
		int         screen;

#ifdef useAccumulator
		int i;
		(void) free((void *) cols);
		for (i=0;i<Root->Width;i++) {
			(void) free((void *) Root->accMap[i]);
		}
		(void) free((void *) Root->accMap);
#endif
#ifdef POINTS_HISTORY
		free(oldPointsX);
		free(oldPointsY);
#endif
		for (screen = 0; screen < MI_NUM_SCREENS(mi); ++screen)
			free_strange(MI_DISPLAY(mi), &Root[screen]);
		(void) free((void *) Root);
		Root = (ATTRACTOR *) NULL; 
	}
}

XSCREENSAVER_MODULE ("Strange", strange)

#endif /* MODE_strange */
