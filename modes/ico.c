/* -*- Mode: C; tab-width: 4 -*- */
/* ico --- bouncing polyhedra */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)ico.c	5.00 2000/11/01 xlockmore";

#endif
/*-
 * Copyright (c) 1987  X Consortium
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
 * 27-Mar-2000: Added double buffering for drawings
 * 10-May-1997: Compatible with xscreensaver
 * 25-Mar-1997: David Bagley <bagleyd@tux.org>
 *              Took ico from the X11R6 distribution.  Stripped out
 *              anything complicated... to be added back in later.
 *              added dodecahedron, tetrahedron, and star octahedron.
 * $XConsortium: ico.c,v 1.47 94/04/17 20:45:15 gildea Exp $
 */

/*-
original copyright
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.


Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/
/******************************************************************************
 * Description
 *	Display a wire-frame rotating icosahedron, with hidden lines removed
 *****************************************************************************/
/*-
 * Additions by jimmc@sci:
 *  faces and colors
 *  double buffering on the display
 *  additional polyhedra
 *
 * multi-thread version by Stephen Gildea, January 1992
 */

#ifdef STANDALONE
#define MODE_ico
#define PROGCLASS "Ico"
#define HACK_INIT init_ico
#define HACK_DRAW draw_ico
#define ico_opts xlockmore_opts
#define DEFAULTS "*delay: 100000 \n" \
 "*count: 0 \n" \
 "*cycles: 300 \n" \
 "*ncolors: 200 \n"
#define UNIFORM_COLORS
#define BRIGHT_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */
#include "automata.h"

#ifdef MODE_ico

#define DEF_FACES "True"
#define DEF_EDGES "True"
#define DEF_OPAQUE "True"

static Bool faces;
static Bool edges;
static Bool opaque;

static XrmOptionDescRec opts[] =
{
	{(char *) "-faces", (char *) ".ico.faces", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+faces", (char *) ".ico.faces", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-edges", (char *) ".ico.edges", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+edges", (char *) ".ico.edges", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-opaque", (char *) ".ico.opaque", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+opaque", (char *) ".ico.opaque", XrmoptionNoArg, (caddr_t) "off"}
};
static argtype vars[] =
{
	{(void *) & faces, (char *) "faces", (char *) "Faces", (char *) DEF_FACES, t_Bool},
	{(void *) & edges, (char *) "edges", (char *) "Edges", (char *) DEF_EDGES, t_Bool},
	{(void *) & opaque, (char *) "opaque", (char *) "Opaque", (char *) DEF_OPAQUE, t_Bool}
};
static OptionStruct desc[] =
{
	{(char *) "-/+faces", (char *) "turn on/off drawing of faces"},
	{(char *) "-/+edges", (char *) "turn on/off drawing of wireframe"},
	{(char *) "-/+opaque", (char *) "turn on/off drawing of bottom (unless faces true)"}
};

ModeSpecOpt ico_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   ico_description =
{"ico", "init_ico", "draw_ico", "release_ico",
 "refresh_ico", "change_ico", (char *) NULL, &ico_opts,
 200000, 0, 400, 0, 64, 1.0, "",
 "Shows a bouncing polyhedron", 0, NULL};

#endif

#define MINSIZE 5
#define DEFAULT_DELTAX 13
#define DEFAULT_DELTAY 9

/*-
 * This is the description of one polyhedron file
 */

#define MAXVERTS 120
	/* great rhombicosidodecahedron has 120 vertices */
#define MAXNV MAXVERTS
#define MAXFACES 30
	/* (hexakis icosahedron has 120 faces) */
#define MAXEDGES 180
	/* great rhombicosidodecahedron has 180 edges */
#define MAXEDGESPERPOLY 20

typedef struct {
	double      x, y, z;
} Point3D;

/* structure of the include files which define the polyhedra */
typedef struct {
#ifdef DEFUNCT
	char       *longname;	/* long name of object */
	char       *shortname;	/* short name of object */
	char       *dual;	/* long name of dual */
#endif
	int         numverts;	/* number of vertices */
	int         numedges;	/* number of edges */
	int         numfaces;	/* number of faces */
	Point3D     v[MAXVERTS];	/* the vertices */
	int         f[MAXEDGES * 2 + MAXFACES];		/* the faces */
} Polyinfo;

/*-
 *                              faces/edges/vert Vertex Config   Wythoff Symbol
 * tetrahedron                  4/6/4            {3,3,3}         3|2 3
 * cube                         6/12/8           {4,4,4}         3|2 4
 * octahedron                   8/12/6           {3,3,3,3}       4|2 3
 * dodecahedron                 12/30/12         {5,5,5}         3|2 5
 * icosahedron                  20/30/12         {3,3,3,3,3}     5|2 3
 *Nice additions would be the Kepler-Poinsot Solids:
 * small stellated dodecahedron 12/30/12      {5/2,5/2,5/2,5/2,5/2}   5|2 5/2
 * great stellated dodecahedron 12/30/20         {5/2,5/2,5/2}   3|2 5/2
 * great dodecahedron           12/30/12         {5,5,5,5,5}/2   5/2|2 5
 * great icosahedron            20/30/12         {3,3,3,3,3}/2   5/2|2 3
 */

static Polyinfo polygons[] =
{

/* objtetra - structure values for tetrahedron */
	{
#ifdef DEFUNCT
		"tetrahedron", "tetra",		/* long and short names */
		"tetrahedron",	/* long name of dual */
#endif
		4, 6, 4,	/* number of vertices, edges, and faces */
		{		/* vertices (x,y,z) */
			/* all points must be within radius 1 of the origin */
#define T 0.57735
			{T, T, T},
			{T, -T, -T},
			{-T, T, -T},
			{-T, -T, T},
#undef T
		},
		{		/* faces (numfaces + indexes into vertices) */
		/*  faces must be specified clockwise from the outside */
			3, 2, 1, 0,
			3, 1, 3, 0,
			3, 3, 2, 0,
			3, 2, 3, 1,
		}
	},

/* objcube - structure values for cube */

	{
#ifdef DEFUNCT
		"hexahedron", "cube",	/* long and short names */
		"octahedron",	/* long name of dual */
#endif
		8, 12, 6,	/* number of vertices, edges, and faces */
		{		/* vertices (x,y,z) */
			/* all points must be within radius 1 of the origin */
#define T 0.57735
			{T, T, T},
			{T, T, -T},
			{T, -T, -T},
			{T, -T, T},
			{-T, T, T},
			{-T, T, -T},
			{-T, -T, -T},
			{-T, -T, T},
#undef T
		},
		{		/* faces (numfaces + indexes into vertices) */
		/*  faces must be specified clockwise from the outside */
			4, 0, 1, 2, 3,
			4, 7, 6, 5, 4,
			4, 1, 0, 4, 5,
			4, 3, 2, 6, 7,
			4, 2, 1, 5, 6,
			4, 0, 3, 7, 4,
		}
	},

/* objocta - structure values for octahedron */

	{
#ifdef DEFUNCT
		"octahedron", "octa",	/* long and short names */
		"hexahedron",	/* long name of dual */
#endif
		6, 12, 8,	/* number of vertices, edges, and faces */
		{		/* vertices (x,y,z) */
			/* all points must be within radius 1 of the origin */
#define T 1.0
			{T, 0, 0},
			{-T, 0, 0},
			{0, T, 0},
			{0, -T, 0},
			{0, 0, T},
			{0, 0, -T},
#undef T
		},
		{		/* faces (numfaces + indexes into vertices) */
		/*  faces must be specified clockwise from the outside */
			3, 0, 4, 2,
			3, 0, 2, 5,
			3, 0, 5, 3,
			3, 0, 3, 4,
			3, 1, 2, 4,
			3, 1, 5, 2,
			3, 1, 3, 5,
			3, 1, 4, 3,
		}
	},

/* objdodec - structure values for dodecahedron */

	{
#ifdef DEFUNCT
		"dodecahedron", "dodeca",	/* long and short names */
		"icosahedron",	/* long name of dual */
#endif
		20, 30, 12,	/* number of vertices, edges, and faces */
		{		/* vertices (x,y,z) */
			/* all points must be within radius 1 of the origin */
			{0.000000, 0.309017, 0.809015},
			{0.000000, -0.309017, 0.809015},
			{0.000000, -0.309017, -0.809015},
			{0.000000, 0.309017, -0.809015},
			{0.809015, 0.000000, 0.309017},
			{-0.809015, 0.000000, 0.309017},
			{-0.809015, 0.000000, -0.309017},
			{0.809015, 0.000000, -0.309017},
			{0.309017, 0.809015, 0.000000},
			{-0.309017, 0.809015, 0.000000},
			{-0.309017, -0.809015, 0.000000},
			{0.309017, -0.809015, 0.000000},
			{0.500000, 0.500000, 0.500000},
			{-0.500000, 0.500000, 0.500000},
			{-0.500000, -0.500000, 0.500000},
			{0.500000, -0.500000, 0.500000},
			{0.500000, -0.500000, -0.500000},
			{0.500000, 0.500000, -0.500000},
			{-0.500000, 0.500000, -0.500000},
			{-0.500000, -0.500000, -0.500000},
		},
		{		/* faces (numfaces + indexes into vertices) */
		/*  faces must be specified clockwise from the outside */
			5, 12, 8, 17, 7, 4,
			5, 5, 6, 18, 9, 13,
			5, 14, 10, 19, 6, 5,
			5, 12, 4, 15, 1, 0,
			5, 13, 9, 8, 12, 0,
			5, 1, 14, 5, 13, 0,
			5, 16, 7, 17, 3, 2,
			5, 19, 10, 11, 16, 2,
			5, 3, 18, 6, 19, 2,
			5, 15, 11, 10, 14, 1,
			5, 3, 17, 8, 9, 18,
			5, 4, 7, 16, 11, 15,
		}
	},

/* objicosa - structure values for icosahedron */

	{
#ifdef DEFUNCT
		"icosahedron", "icosa",		/* long and short names */
		"dodecahedron",	/* long name of dual */
#endif
		12, 30, 20,	/* number of vertices, edges, and faces */
		{		/* vertices (x,y,z) */
			/* all points must be within radius 1 of the origin */
			{0.00000000, 0.00000000, -0.95105650},
			{0.00000000, 0.85065080, -0.42532537},
			{0.80901698, 0.26286556, -0.42532537},
			{0.50000000, -0.68819095, -0.42532537},
			{-0.50000000, -0.68819095, -0.42532537},
			{-0.80901698, 0.26286556, -0.42532537},
			{0.50000000, 0.68819095, 0.42532537},
			{0.80901698, -0.26286556, 0.42532537},
			{0.00000000, -0.85065080, 0.42532537},
			{-0.80901698, -0.26286556, 0.42532537},
			{-0.50000000, 0.68819095, 0.42532537},
			{0.00000000, 0.00000000, 0.95105650}
		},
		{		/* faces (numfaces + indexes into vertices) */
		/*  faces must be specified clockwise from the outside */
			3, 0, 2, 1,
			3, 0, 3, 2,
			3, 0, 4, 3,
			3, 0, 5, 4,
			3, 0, 1, 5,
			3, 1, 6, 10,
			3, 1, 2, 6,
			3, 2, 7, 6,
			3, 2, 3, 7,
			3, 3, 8, 7,
			3, 3, 4, 8,
			3, 4, 9, 8,
			3, 4, 5, 9,
			3, 5, 10, 9,
			3, 5, 1, 10,
			3, 10, 6, 11,
			3, 6, 7, 11,
			3, 7, 8, 11,
			3, 8, 9, 11,
			3, 9, 10, 11
		}
	},

/* objplane - structure values for plane */

	{
#ifdef DEFUNCT
		"plane", "plane",	/* long and short names */
		"plane",	/* long name of dual?? */
#endif
		4, 4, 1,	/* number of vertices, edges, and faces */
		{		/* vertices (x,y,z) */
			/* all points must be within radius 1 of the origin */
#define T 1.0
			{T, 0, 0},
			{-T, 0, 0},
			{0, T, 0},
			{0, -T, 0},
#undef T
		},
		{		/* faces (numfaces + indexes into vertices) */
		/*  faces must be specified clockwise from the outside */
			4, 0, 2, 1, 3,
		}
	},

/* objpyr - structure values for pyramid */

	{
#ifdef DEFUNCT
		"pyramid", "pyramid",	/* long and short names */
		"pyramid",	/* long name of dual */
#endif
		5, 8, 5,	/* number of vertices, edges, and faces */
		{		/* vertices (x,y,z) */
			/* all points must be within radius 1 of the origin */
#define T 1.0
			{T, 0, 0},
			{-T, 0, 0},
			{0, T, 0},
			{0, -T, 0},
			{0, 0, T},
		/* {  0,  0, -T }, */
#undef T
		},
		{		/* faces (numfaces + indexes into vertices) */
			/*  faces must be specified clockwise from the outside */
			3, 0, 4, 2,
			/* 3, 0, 2, 5, */
			/* 3, 0, 5, 3, */
			3, 0, 3, 4,
			3, 1, 2, 4,
			/* 3, 1, 5, 2, */
			/* 3, 1, 3, 5, */
			3, 1, 4, 3,
			4, 0, 2, 1, 3,
		}
	},

/* ico does not draw non-convex polyhedra well. */
/* objstar - structure values for octahedron star (stellated octahedron?) */
	{
#ifdef DEFUNCT
		"star", "star",	/* long and short names */
		"star",		/* long name of dual */
#endif
		8, 12, 8,	/* number of vertices, edges, and faces */
		{		/* vertices (x,y,z) */
			/* all points must be within radius 1 of the origin */
#define T 0.577
			{T, T, T},
			{T, -T, -T},
			{-T, T, -T},
			{-T, -T, T},
			{-T, -T, -T},
			{-T, T, T},
			{T, -T, T},
			{T, T, -T},
#undef T
		},
		{		/* faces (numfaces + indexes into vertices) */
		/*  faces must be specified clockwise from the outside */
			3, 2, 1, 0,
			3, 1, 3, 0,
			3, 3, 2, 0,
			3, 2, 3, 1,
			3, 6, 5, 4,
			3, 5, 7, 4,
			3, 7, 6, 4,
			3, 6, 7, 5,
		}
	},
  /* Needed 4 other 3-D stars */

};

static int  polysize = sizeof (polygons) / sizeof (polygons[0]);
#define POLYSIZE 5  /* Only the 5 Platonic solids work, why is that? */

#define POLYBITS(n,w,h)\
  if ((ip->pixmaps[ip->init_bits]=\
  XCreatePixmapFromBitmapData(display,window,(char *)n,w,h,1,0,1))==None){\
  free_ico(display,ip);return;} else {ip->init_bits++;}

typedef double Transform3D[4][4];

/* variables that need to be per-thread */

typedef struct {
	int         loopcount;
	int         object;
	int         width, height;
	int         linewidth;
	long        color;
	Polyinfo   *poly;
	int         polyW, polyH;
	int         currX, currY;
	int         prevX, prevY;
	int         polyDeltaX, polyDeltaY;
	int         deltaWidth, deltaHeight;
	Bool        faces, edges, opaque;
	char        drawnEdges[MAXNV][MAXNV];
	char        drawnPoints[MAXNV];
	int         xv_buffer;
	Transform3D xform;
	Point3D     xv[2][MAXNV];
	double      wo2, ho2;
	Pixmap      dbuf;
	GC          dbuf_gc;
	int         color_offset;
	int         init_bits;
	GC          stippledGC;
	Pixmap      pixmaps[NUMSTIPPLES - 1];
} icostruct;

static icostruct *icos = (icostruct *) NULL;

/*-
 * variables that are not set except maybe in initialization before
 * any additional threads are created
 */


static void
icoClearArea(ModeInfo * mi, int x, int y, int w, int h)
{
	Display    *display = MI_DISPLAY(mi);

#if 1
	/* my monochrome likes this better */
	XSetForeground(display, MI_GC(mi), MI_BLACK_PIXEL(mi));
	XFillRectangle(display, MI_WINDOW(mi), MI_GC(mi), x, y, w, h);
#else
	XClearArea(display, MI_WINDOW(mi), x, y, w, h, 0);
#endif
}

/******************************************************************************
 * Description
 *	Format a 4x4 identity matrix.
 *
 * Output
 *	*m		Formatted identity matrix
 *****************************************************************************/
static void
IdentMat(register Transform3D m)
{
	register int i;
	register int j;

	for (i = 3; i >= 0; --i) {
		for (j = 3; j >= 0; --j)
			m[i][j] = 0.0;
		m[i][i] = 1.0;
	}
}



/******************************************************************************
 * Description
 *	Format a matrix that will perform a rotation transformation
 *	about the specified axis.  The rotation angle is measured
 *	counterclockwise about the specified axis when looking
 *	at the origin from the positive axis.
 *
 * Input
 *	axis		Axis ('x', 'y', 'z') about which to perform rotation
 *	angle		Angle (in radians) of rotation
 *	A		Pointer to rotation matrix
 *
 * Output
 *	*m		Formatted rotation matrix
 *****************************************************************************/

static void
FormatRotateMat(char axis, double angle, register Transform3D m)
{
	double      s, c;

	IdentMat(m);

	s = sin(angle);
	c = cos(angle);

	switch (axis) {
		case 'x':
			m[1][1] = m[2][2] = c;
			m[1][2] = s;
			m[2][1] = -s;
			break;
		case 'y':
			m[0][0] = m[2][2] = c;
			m[2][0] = s;
			m[0][2] = -s;
			break;
		case 'z':
			m[0][0] = m[1][1] = c;
			m[0][1] = s;
			m[1][0] = -s;
			break;
	}
}


/******************************************************************************
 * Description
 *	Concatenate two 4-by-4 transformation matrices.
 *
 * Input
 *	l		multiplicand (left operand)
 *	r		multiplier (right operand)
 *
 * Output
 *	*m		Result matrix
 *****************************************************************************/

static void
ConcatMat(register Transform3D l, register Transform3D r,
	  register Transform3D m)
{
	register int i;
	register int j;

	for (i = 0; i < 4; ++i)
		for (j = 0; j < 4; ++j)
			m[i][j] = l[i][0] * r[0][j]
				+ l[i][1] * r[1][j]
				+ l[i][2] * r[2][j]
				+ l[i][3] * r[3][j];
}

/* Set up points, transforms, etc.  */

static void
initPoly(ModeInfo * mi, Polyinfo * poly, int polyW, int polyH, Bool init)
{
	icostruct  *ip = &icos[MI_SCREEN(mi)];
	Point3D    *vertices = poly->v;
	int         NV = poly->numverts;
	Transform3D r1;
	Transform3D r2;

#define ROLL_DEGREES 5
	if ((ip->polyDeltaX < 0 && ip->polyDeltaY < 0) ||
	    (ip->polyDeltaX > 0 && ip->polyDeltaY > 0)) {
		FormatRotateMat('x', ((ip->polyDeltaX > 0) ?
		  -ROLL_DEGREES : ROLL_DEGREES) * M_PI / 180.0, r1);
		FormatRotateMat('y', ((ip->polyDeltaY < 0) ?
		  -ROLL_DEGREES : ROLL_DEGREES) * M_PI / 180.0, r2);
	} else {
		FormatRotateMat('x', ((ip->polyDeltaX < 0) ?
		  -ROLL_DEGREES : ROLL_DEGREES) * M_PI / 180.0, r1);
		FormatRotateMat('y', ((ip->polyDeltaY > 0) ?
		  -ROLL_DEGREES : ROLL_DEGREES) * M_PI / 180.0, r2);
	}
	ConcatMat(r1, r2, ip->xform);
    if (init) {
		(void) memcpy((char *) ip->xv[0], (char *) vertices, NV * sizeof (Point3D));
		ip->xv_buffer = 0;
		ip->wo2 = polyW / 2.0;
		ip->ho2 = polyH / 2.0;
	}
}

/******************************************************************************
 * Description
 *	Perform a partial transform on non-homogeneous points.
 *	Given an array of non-homogeneous (3-coordinate) input points,
 *	this routine multiplies them by the 3-by-3 upper left submatrix
 *	of a standard 4-by-4 transform matrix.  The resulting non-homogeneous
 *	points are returned.
 *
 * Input
 *	n		number of points to transform
 *	m		4-by-4 transform matrix
 *	in		array of non-homogeneous input points
 *
 * Output
 *	*out		array of transformed non-homogeneous output points
 *****************************************************************************/

static void
PartialNonHomTransform(int n, register Transform3D m,
		       register Point3D * in, register Point3D * out)
{
	for (; n > 0; --n, ++in, ++out) {
		out->x = in->x * m[0][0] + in->y * m[1][0] + in->z * m[2][0];
		out->y = in->x * m[0][1] + in->y * m[1][1] + in->z * m[2][1];
		out->z = in->x * m[0][2] + in->y * m[1][2] + in->z * m[2][2];
	}
}


/******************************************************************************
 * Description
 *	Undraw previous polyhedron (by erasing its bounding box).
 *	Rotate and draw the new polyhedron.
 *
 * Input
 *	poly		the polyhedron to draw
 *	gc		X11 graphics context to be used for drawing
 *	currX, currY	position of upper left of bounding-box
 *	polyW, polyH	size of bounding-box
 *	prevX, prevY	position of previous bounding-box
 *****************************************************************************/

static void
drawPoly(ModeInfo * mi, Polyinfo * poly, GC gc,
	 int currX, int currY, int polyW, int polyH, int prevX, int prevY)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	icostruct  *ip = &icos[MI_SCREEN(mi)];
	int        *f = poly->f;
	int         NV = poly->numverts;
	int         NF = poly->numfaces;

	register int p0;
	register int p1;
	register XPoint *pv2;
	XSegment   *pe;
	XPoint   *pp;
	register Point3D *pxv;
	XPoint      v2[MAXNV];
	XPoint      pts[MAXNV];
	XSegment    edge_segs[MAXEDGES];
	register int i;
	int         j, k;
	register int *pf;
	int         facecolor;

	int         pcount = 0;
	double      pxvz;
	XPoint      ppts[MAXEDGESPERPOLY];
	Window      lwindow;
	GC          lgc;

	/* Switch double-buffer and rotate vertices */

	ip->xv_buffer = !ip->xv_buffer;
	PartialNonHomTransform(NV, ip->xform,
			       ip->xv[!ip->xv_buffer], ip->xv[ip->xv_buffer]);


	/* Convert 3D coordinates to 2D window coordinates: */

	pxv = ip->xv[ip->xv_buffer];
	pv2 = v2;
	for (i = NV - 1; i >= 0; --i) {
		pv2->x = (int) ((pxv->x + 1.0) * ip->wo2) + currX;
		pv2->y = (int) ((pxv->y + 1.0) * ip->ho2) + currY;
		++pxv;
		++pv2;
	}


	/* Accumulate edges to be drawn, eliminating duplicates for speed: */

	pxv = ip->xv[ip->xv_buffer];
	pv2 = v2;
	pf = f;
	pe = edge_segs;
	pp = pts;
	(void) memset(ip->drawnEdges, 0, sizeof (ip->drawnEdges));
	(void) memset(ip->drawnPoints, 0, sizeof (ip->drawnPoints));

	if (ip->dbuf) {
		XSetForeground(display, ip->dbuf_gc, MI_BLACK_PIXEL(mi));
		XFillRectangle(display, ip->dbuf, ip->dbuf_gc, 0, 0,
			polyW + ip->deltaWidth + ip->linewidth,
			polyH + ip->deltaHeight + ip->linewidth);
		lwindow = ip->dbuf;
		lgc = ip->dbuf_gc;
	} else {
		lwindow = window;
		lgc = gc;
		icoClearArea(mi, prevX, prevY, polyW + 1, polyH + 1);
	}
	for (i = NF - 1; i >= 0; --i, pf += pcount) {

		pcount = *pf++;	/* number of edges for this face */
		pxvz = 0.0;
		for (j = 0; j < pcount; j++) {
			p0 = pf[j];
			pxvz += pxv[p0].z;
		}

		/* If facet faces away from viewer, don't consider it: */
		if (pxvz < 0.0 && (ip->faces || ip->opaque))
			continue;

		if (ip->faces) {
			for (j = 0; j < pcount; j++) {
				p0 = pf[j];
				ppts[j].x = pv2[p0].x;
				ppts[j].y = pv2[p0].y;
				if (ip->dbuf) {
					ppts[j].x -= (currX - ip->deltaWidth / 2 - ip->linewidth / 2);
					ppts[j].y -= (currY - ip->deltaHeight / 2 - ip->linewidth / 2);
				}
			}
			if (MI_NPIXELS(mi) > 2) {
				facecolor = (i * MI_NPIXELS(mi) / NF + ip->color_offset) % MI_NPIXELS(mi);
				XSetForeground(display, lgc, MI_PIXEL(mi, facecolor));
			} else {
				XGCValues   gcv;
				facecolor = (i * (NUMSTIPPLES - 1) / NF + ip->color_offset) % (NUMSTIPPLES - 1);
				gcv.stipple = ip->pixmaps[facecolor];
				gcv.foreground = MI_WHITE_PIXEL(mi);
				gcv.background = MI_BLACK_PIXEL(mi);
				XChangeGC(MI_DISPLAY(mi), ip->stippledGC,
                          GCStipple | GCForeground | GCBackground, &gcv);
				lgc = ip->stippledGC;
				/* XSetForeground(display, lgc, MI_WHITE_PIXEL(mi)); */
			}
			XFillPolygon(display, lwindow, lgc,
				ppts, pcount, Convex, CoordModeOrigin);
		}
		if (ip->edges) {
			for (j = 0; j < pcount; j++) {
				if (j < pcount - 1)
					k = j + 1;
				else
					k = 0;
				p0 = pf[j];
				p1 = pf[k];
				if (!ip->drawnEdges[p0][p1]) {
					ip->drawnEdges[p0][p1] = 1;
					ip->drawnEdges[p1][p0] = 1;
					pe->x1 = pv2[p0].x;
					pe->y1 = pv2[p0].y;
					pe->x2 = pv2[p1].x;
					pe->y2 = pv2[p1].y;
					if (ip->dbuf) {
						pe->x1 -= (currX - ip->deltaWidth / 2 - ip->linewidth / 2);
						pe->y1 -= (currY - ip->deltaHeight / 2 - ip->linewidth / 2);
						pe->x2 -= (currX - ip->deltaWidth / 2 - ip->linewidth / 2);
						pe->y2 -= (currY - ip->deltaHeight / 2 - ip->linewidth / 2);
					}
					++pe;
				}
			}
		} else {
			for (j = 0; j < pcount; j++) {
				p0 = pf[j];
				if (!ip->drawnPoints[p0]) {
					ip->drawnPoints[p0] = 1;
					pp->x = pv2[p0].x;
					pp->y = pv2[p0].y;
					if (ip->dbuf) {
						pp->x -= (currX - ip->deltaWidth / 2 - ip->linewidth / 2);
						pp->y -= (currY - ip->deltaHeight / 2 - ip->linewidth / 2);
					}
					++pp;
				}
			}
		}
	}
	/* Erase previous, draw current icosahedrons; sync for smoothness. */

	if (ip->edges) {
		if (MI_NPIXELS(mi) <= 2)
			if (ip->faces) {
				XSetForeground(display, lgc, ip->color);
			} else {
				XSetForeground(display, lgc, MI_WHITE_PIXEL(mi));
			}
		else {
			ip->color = (ip->color + 1) % MI_NPIXELS(mi);
			XSetForeground(display, lgc, MI_PIXEL(mi, ip->color));
		}
		XSetLineAttributes(display, lgc, ip->linewidth,
                           LineSolid, CapRound, JoinRound);
		XDrawSegments(display, lwindow, lgc, edge_segs, pe - edge_segs);
		XSetLineAttributes(display, lgc, 1,
                           LineSolid, CapNotLast, JoinRound);
	} else if (!ip->faces) {
		if (MI_NPIXELS(mi) <= 2)
			XSetForeground(display, lgc, MI_WHITE_PIXEL(mi));
		else {
			ip->color = (ip->color + 1) % MI_NPIXELS(mi);
			XSetForeground(display, lgc, MI_PIXEL(mi, ip->color));
		}
#if 0
		/* just does not look that good */
		if (ip->linewidth <= 1)
			XDrawPoints(display, lwindow, lgc, pts, pp - pts, CoordModeOrigin);
		else
#endif
		{
			for (j = 0; j < pp-pts; j++)
			XFillArc(display, lwindow, lgc,
                         pts[j].x - ip->linewidth / 2, pts[j].y - ip->linewidth / 2, ip->linewidth, ip->linewidth, 0, 23040);

		}
	}
	if (ip->dbuf) {
		XCopyArea(display, ip->dbuf, window, gc, 0, 0,
			polyW + 1 + ip->deltaWidth + ip->linewidth,
			polyH + 1 + ip->deltaHeight + ip->linewidth,
			currX - ip->deltaWidth / 2 - ip->linewidth / 2,
			currY - ip->deltaHeight / 2 - ip->linewidth / 2);
	}
	XFlush(display);
}

static void
free_ico(Display *display, icostruct *ip)
{
	int shade;

	if (ip->stippledGC != None) {
		XFreeGC(display, ip->stippledGC);
		ip->stippledGC = None;
	}
	for (shade = 0; shade < ip->init_bits; shade++) {
		if (ip->pixmaps[shade] != None) {
			XFreePixmap(display, ip->pixmaps[shade]);
			ip->pixmaps[shade] = None;
		}
	}
	if (ip->dbuf != None) {
		XFreePixmap(display, ip->dbuf);
		ip->dbuf = None;
	}
	if (ip->dbuf_gc != None) {
		XFreeGC(display, ip->dbuf_gc);
		ip->dbuf_gc = None;
	}
}

void
init_ico(ModeInfo * mi)
{
	Display * display = MI_DISPLAY(mi);
	Window window = MI_WINDOW(mi);
	int         size = MI_SIZE(mi);
	icostruct  *ip;

	if (icos == NULL) {
		if ((icos = (icostruct *) calloc(MI_NUM_SCREENS(mi),
						 sizeof (icostruct))) == NULL)
			return;
	}
	ip = &icos[MI_SCREEN(mi)];

	ip->width = MI_WIDTH(mi);
	ip->height = MI_HEIGHT(mi);
	ip->linewidth = NRAND((ip->width + ip->height) / 200 + 1) + 2;

	if (MI_NPIXELS(mi) <= 2) {
		if (ip->stippledGC == None) {
			XGCValues   gcv;

			gcv.fill_style = FillOpaqueStippled;
			if ((ip->stippledGC = XCreateGC(display, window, GCFillStyle,
					 &gcv)) == NULL) {
				free_ico(display, ip);
				return;
			}
		}
		if (ip->init_bits == 0) {
			int i;

			for (i = 1; i < NUMSTIPPLES; i++) {
				POLYBITS(stipples[i], STIPPLESIZE, STIPPLESIZE);
			}
		}
	}

	if (MI_IS_FULLRANDOM(mi)) {
		ip->faces = (Bool) (LRAND() & 1);
		ip->edges = (Bool) (LRAND() & 1);
		ip->opaque = (!(NRAND(4) == 0));
	} else {
		ip->edges = edges;
		ip->faces = faces;
		ip->opaque = opaque;
	}

	if (size < -MINSIZE)
		ip->polyW = NRAND(MIN(-size, MAX(MINSIZE,
		   MIN(ip->width, ip->height) / 4)) - MINSIZE + 1) + MINSIZE;
	else if (size < MINSIZE) {
		if (!size)
			ip->polyW = MAX(MINSIZE, MIN(ip->width, ip->height) / 4);
		else
			ip->polyW = MINSIZE;
	} else
		ip->polyW = MIN(size, MAX(MINSIZE,
					  MIN(ip->width, ip->height) / 4));

	ip->polyH = ip->polyW;
	ip->polyDeltaX = ip->polyW / DEFAULT_DELTAY + 1;
	ip->polyDeltaY = ip->polyH / DEFAULT_DELTAX + 1;
	ip->currX = NRAND(ip->width - ip->polyW);
	ip->currY = NRAND(ip->height - ip->polyH);

	/* Bounce the box in the window */

	ip->deltaWidth = ip->polyDeltaX * 2;
	ip->deltaHeight = ip->polyDeltaY * 2;
	ip->polyDeltaX *= ((LRAND() & 1) ? 1 : -1);
	ip->polyDeltaY *= ((LRAND() & 1) ? 1 : -1);

	ip->loopcount = 0;

	ip->object = MI_COUNT(mi) - 1;
	if (ip->object < 0 || ip->object >= polysize) {
		/* avoid pyramid and star (drawing errors) count = 7 & 8
		   also  avoid plane (boring) count = 6
		   but allow direct access */
		ip->object = NRAND(POLYSIZE);
	}
	ip->poly = polygons + ip->object;
	if (MI_NPIXELS(mi) > 2)
		ip->color = NRAND(MI_NPIXELS(mi));
	else if (ip->faces && ip->edges)
		ip->color = (LRAND() & 1) ? MI_WHITE_PIXEL(mi) : MI_BLACK_PIXEL(mi);

	ip->color_offset = NRAND(MI_NPIXELS(mi));
#ifndef NO_DBUF
	if (ip->dbuf != None)
		XFreePixmap(display, ip->dbuf);
	ip->dbuf = XCreatePixmap(display, window,
		ip->polyW + ip->deltaWidth + ip->linewidth,
		ip->polyH + ip->deltaHeight + ip->linewidth,
	 	MI_DEPTH(mi));
	/* Allocation checked */
	if (ip->dbuf != None) {
		XGCValues   gcv;

		gcv.foreground = 0;
		gcv.background = 0;
		gcv.graphics_exposures = False;
		gcv.function = GXcopy;

		if (ip->dbuf_gc != None)
			XFreeGC(display, ip->dbuf_gc);
		if ((ip->dbuf_gc = XCreateGC(display, ip->dbuf,
				GCForeground | GCBackground | GCGraphicsExposures | GCFunction,
				&gcv)) == None) {
			XFreePixmap(display, ip->dbuf);
			ip->dbuf = None;
		} else {
			XFillRectangle(display, ip->dbuf, ip->dbuf_gc,
				0, 0, ip->polyW + ip->deltaWidth + ip->linewidth,
				ip->polyH + ip->deltaHeight + ip->linewidth);
			XSetBackground(display, MI_GC(mi), MI_BLACK_PIXEL(mi));
			XSetFunction(display, MI_GC(mi), GXcopy);
		}
	}
#endif

	MI_CLEARWINDOW(mi);

	/* don't want any exposure events from XCopyPlane */
	XSetGraphicsExposures(display, MI_GC(mi), False);


	initPoly(mi, ip->poly, ip->polyW, ip->polyH, True);
}

void
draw_ico(ModeInfo * mi)
{
	icostruct  *ip;

	if (icos == NULL)
		return;
	ip = &icos[MI_SCREEN(mi)];
	if ((MI_NPIXELS(mi) <= 2) && (ip->stippledGC == None))
		return;

	MI_IS_DRAWN(mi) = True;
	if (++ip->loopcount > MI_CYCLES(mi))
		init_ico(mi);

	ip->prevX = ip->currX;
	ip->prevY = ip->currY;

	ip->currX += ip->polyDeltaX;
	if (ip->currX < 0 || ip->currX + ip->polyW > ip->width) {

		ip->currX -= 2 * ip->polyDeltaX;
		ip->polyDeltaX = -ip->polyDeltaX;
		/* spin should change after hitting wall */
		initPoly(mi, ip->poly, ip->polyW, ip->polyH, False);
	}
	ip->currY += ip->polyDeltaY;
	if (ip->currY < 0 || ip->currY + ip->polyH > ip->height) {
		ip->currY -= 2 * ip->polyDeltaY;
		ip->polyDeltaY = -ip->polyDeltaY;
		/* spin should change after hitting wall */
		initPoly(mi, ip->poly, ip->polyW, ip->polyH, False);
	}
	drawPoly(mi, ip->poly, MI_GC(mi),
	   ip->currX, ip->currY, ip->polyW, ip->polyH, ip->prevX, ip->prevY);
}
void
refresh_ico(ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
}

void
release_ico(ModeInfo * mi)
{
	if (icos != NULL) {
		int screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_ico(MI_DISPLAY(mi), &icos[screen]);
		free(icos);
		icos = (icostruct *) NULL;
	}
}

void
change_ico(ModeInfo * mi)
{
	icostruct  *ip;

	if (icos == NULL)
		return;
	ip = &icos[MI_SCREEN(mi)];
	if (MI_NPIXELS(mi) <= 2 && ip->stippledGC == None)
		return;

	if (MI_COUNT(mi) <= 0 || MI_COUNT(mi) > POLYSIZE) {
		ip->object = (ip->object + 1) % (POLYSIZE);
		ip->poly = polygons + ip->object;
	}
	ip->loopcount = 0;

	MI_CLEARWINDOWCOLORMAPFAST(mi, MI_GC(mi), MI_BLACK_PIXEL(mi));

	initPoly(mi, ip->poly, ip->polyW, ip->polyH, True);
}

#endif /* MODE_ico */
