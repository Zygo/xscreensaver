/* -*- Mode: C; tab-width: 4 -*- */
/* apollonian --- Apollonian Circles */

#if 0
static const char sccsid[] = "@(#)apollonian.c	5.02 2001/07/01 xlockmore";
#endif

/*-
 * Copyright (c) 2000, 2001 by Allan R. Wilks <allan@research.att.com>.
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
 * radius r = 1 / c (curvature)
 *
 * Descartes Circle Theorem: (a, b, c, d are curvatures of tangential circles)
 * Let a, b, c, d be the curvatures of for mutually (externally) tangent
 * circles in the plane.  Then
 * a^2 + b^2 + c^2 + d^2 = (a + b + c + d)^2 / 2
 *
 * Complex Descartes Theorem:  If the oriented curvatues and (complex) centers
 * of an oriented Descrates configuration in the plane are a, b, c, d and
 * w, x, y, z respectively, then
 * a^2*w^2 + b^2*x^2 + c^2*y^2 + d^2*z^2 = (aw + bx + cy + dz)^2 / 2
 * In addition these quantities satisfy
 * a^2*w + b^2*x + c^2*y + d^2*z = (aw + bx + cy + dz)(a + b + c + d) /  2
 *
 * Enumerate root integer Descartes quadruples (a,b,c,d) satisfying the
 * Descartes condition:
 *      2(a^2+b^2+c^2+d^2) = (a+b+c+d)^2
 * i.e., quadruples for which no application of the "pollinate" operator
 *      z <- 2(a+b+c+d) - 3*z,
 * where z is in {a,b,c,d}, gives a quad of strictly smaller sum.  This
 * is equivalent to the condition:
 *      sum(a,b,c,d) >= 2*max(a,b,c,d)
 * which, because of the Descartes condition, is equivalent to
 *      sum(a^2,b^2,c^2,d^2) >= 2*max(a,b,c,d)^2
 *
 *
 * Todo:
 * Add a small font
 *
 * Revision History:
 * 25-Jun-2001: Converted from C and Postscript code by David Bagley 
 *              Original code by Allan R. Wilks <allan@research.att.com>.
 *
 * From Circle Math Science News April 21, 2001 VOL. 254-255
 * http://www.sciencenews.org/20010421/toc.asp
 * Apollonian Circle Packings Assorted papers from Ronald L Graham,
 * Jeffrey Lagarias, Colin Mallows, Allan Wilks, Catherine Yan
 *      http://front.math.ucdavis.edu/math.NT/0009113
 *      http://front.math.ucdavis.edu/math.MG/0101066
 *      http://front.math.ucdavis.edu/math.MG/0010298
 *      http://front.math.ucdavis.edu/math.MG/0010302
 *      http://front.math.ucdavis.edu/math.MG/0010324
 */

#ifdef STANDALONE
# define MODE_apollonian
# define DEFAULTS	"*delay:   1000000 \n" \
					"*count:   64      \n" \
					"*cycles:  20      \n" \
					"*ncolors: 64      \n" \
					"*font:    fixed" "\n" \
					"*fpsTop: true     \n" \
					"*fpsSolid: true   \n" \
					"*ignoreRotation: True" \

# define refresh_apollonian 0
# include "xlockmore.h"		/* in xscreensaver distribution */
# include "erase.h"
#else /* STANDALONE */
# include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_apollonian

#define DEF_ALTGEOM  "True"
#define DEF_LABEL  "True"

static Bool altgeom;
static Bool label;

static XrmOptionDescRec opts[] =
{
	{"-altgeom", ".apollonian.altgeom", XrmoptionNoArg, "on"},
	{"+altgeom", ".apollonian.altgeom", XrmoptionNoArg, "off"},
	{"-label", ".apollonian.label", XrmoptionNoArg, "on"},
	{"+label", ".apollonian.label", XrmoptionNoArg, "off"},
};
static argtype vars[] =
{
	{&altgeom, "altgeom", "AltGeom", DEF_ALTGEOM, t_Bool},
	{&label,   "label",   "Label",   DEF_LABEL,   t_Bool},
};
static OptionStruct desc[] =
{
        {"-/+altgeom", "turn on/off alternate geometries (off euclidean space, on includes spherical and hyperbolic)"},
        {"-/+label", "turn on/off alternate space and number labeling"},
};

ENTRYPOINT ModeSpecOpt apollonian_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef DOFONT
extern XFontStruct *getFont(Display * display);
#endif

#ifdef USE_MODULES
ModStruct   apollonian_description =
{"apollonian", "init_apollonian", "draw_apollonian", "release_apollonian",
 "init_apollonian", "init_apollonian", (char *) NULL, &apollonian_opts,
 1000000, 64, 20, 1, 64, 1.0, "",
 "Shows Apollonian Circles", 0, NULL};

#endif

typedef struct {
	int a, b, c, d;
} apollonian_quadruple;

typedef struct {
        double e;       /* euclidean bend */
        double s;       /* spherical bend */
        double h;       /* hyperbolic bend */
        double x, y;    /* euclidean bend times euclidean position */
} circle;
enum space {
  euclidean = 0, spherical, hyperbolic
};

static const char * space_string[] = {
  "euclidean",
  "spherical",
  "hyperbolic"
};

/*
Generate Apollonian packing starting with a quadruple of circles.
The four input lines each contain the 5-tuple (e,s,h,x,y) representing
the circle with radius 1/e and center (x/e,y/e).  The s and h is propagated
like e, x and y, but can differ from e so as to represent different
geometries, spherical and hyperbolic, respectively.  The "standard" picture,
for example (-1, 2, 2, 3), can be labeled for the three geometries.
Origins of circles z1, z2, z3, z4
a * z1 = 0
b * z2 = (a+b)/a
c * z3 = (q123 + a * i)^2/(a*(a+b)) where q123 = sqrt(a*b+a*c+b*c)
d * z4 = (q124 + a * i)^2/(a*(a+b)) where q124 = q123 - a - b
If (e,x,y) represents the Euclidean circle (1/e,x/e,y/e) (so that e is
the label in the standard picture) then the "spherical label" is
(e^2+x^2+y^2-1)/(2*e) (an integer!)  and the "hyperbolic label", is 
calulated by h + s = e.
*/
static circle examples[][4] = {
{ /* double semi-bounded */
	{ 0, 0, 0,   0,  1},
	{ 0, 0, 0,   0, -1},
	{ 1, 1, 1,  -1,  0},
	{ 1, 1, 1,   1,  0}
},
#if 0
{ /* standard */
	{-1, 0, -1,   0,  0},
	{ 2, 1,  1,   1,  0},
	{ 2, 1,  1,  -1,  0},
	{ 3, 2,  1,   0,  2}
},
{ /* next simplest */
	{-2, -1, -1,   0.0,  0},
	{ 3,  2,  1,   0.5,  0},
	{ 6,  3,  3,  -2.0,  0},
	{ 7,  4,  3,  -1.5,  2}
},
{ /*  */
	{-3, -2, -1,         0.0,  0},
	{ 4,  3,  1,   1.0 / 3.0,  0},
	{12,  7,  5,        -3.0,  0},
	{13,  8,  5,  -8.0 / 3.0,  2}
},
{ /* Mickey */
	{-3, -2, -1,         0.0,  0},
	{ 5,  4,  1,   2.0 / 3.0,  0},
	{ 8,  5,  3,  -4.0 / 3.0, -1},
	{ 8,  5,  3,  -4.0 / 3.0,  1}
},
{ /*  */
	{-4, -3, -1,   0.00,  0},
	{ 5,  4,  1,   0.25,  0},
	{20, 13,  7,  -4.00,  0},
	{21, 14,  7,  -3.75,  2}
},
{ /* Mickey2 */
	{-4, -2, -2,    0.0,  0},
	{ 8,  4,  4,    1.0,  0},
	{ 9,  5,  4,  -0.75, -1},
	{ 9,  5,  4,  -0.75,  1}
},
{ /* Mickey3 */
	{-5,  -4, -1,   0.0,  0},
	{ 7,   6,  1,   0.4,  0},
	{18,  13,  5,  -2.4, -1},
	{18,  13,  5,  -2.4,  1}
},
{ /*  */
	{-6, -5, -1,          0.0,  0},
	{ 7,  6,  1,    1.0 / 6.0,  0},
	{42, 31, 11,         -6.0,  0},
	{43, 32, 11,  -35.0 / 6.0,  2}
},
{ /*  */
	{-6, -3, -3,         0.0,  0},
	{10,  5,  5,   2.0 / 3.0,  0},
	{15,  8,  7,        -1.5,  0},
	{19, 10,  9,  -5.0 / 6.0,  2}
},
{ /* asymmetric */
	{-6, -5, -1,           0.0,  0.0},
	{11, 10,  1,     5.0 / 6.0,  0.0},
	{14, 11,  3,  -16.0 / 15.0, -0.8},
	{15, 12,  3,          -0.9,  1.2} 
},
#endif
/* Non integer stuff */
#define DELTA 2.154700538 /* ((3+2*sqrt(3))/3) */
{ /* 3 fold symmetric bounded (x, y calculated later) */
	{   -1,    -1,    -1,   0.0,  0.0},
	{DELTA, DELTA, DELTA,   1.0,  0.0},
	{DELTA, DELTA, DELTA,   1.0, -1.0},
	{DELTA, DELTA, DELTA,  -1.0,  1.0} 
},
{ /* semi-bounded (x, y calculated later) */
#define ALPHA 2.618033989 /* ((3+sqrt(5))/2) */
	{              1.0,               1.0,               1.0,   0,  0},
	{              0.0,               0.0,               0.0,   0, -1},
	{1.0/(ALPHA*ALPHA), 1.0/(ALPHA*ALPHA), 1.0/(ALPHA*ALPHA),  -1,  0},
	{        1.0/ALPHA,         1.0/ALPHA,         1.0/ALPHA,  -1,  0}
},
{ /* unbounded (x, y calculated later) */
/* #define PHI 1.618033989 *//* ((1+sqrt(5))/2) */
#define BETA 2.890053638 /* (PHI+sqrt(PHI)) */
	{                 1.0,                  1.0,                  1.0,  0,  0},
	{1.0/(BETA*BETA*BETA), 1.0/(BETA*BETA*BETA), 1.0/(BETA*BETA*BETA),  1,  0},
	{     1.0/(BETA*BETA),      1.0/(BETA*BETA),      1.0/(BETA*BETA),  1,  0},
	{            1.0/BETA,             1.0/BETA,             1.0/BETA,  1,  0}
}
};

#define PREDEF_CIRCLE_GAMES  (sizeof (examples) / (4 * sizeof (circle)))

#if 0
Euclidean
0, 0, 1, 1
-1, 2, 2, 3
-2, 3, 6, 7
-3, 5, 8, 8
-4, 8, 9, 9
-3, 4, 12, 13
-6, 11, 14, 15
 -5, 7, 18, 18
 -6, 10, 15, 19
 -7, 12, 17, 20
 -4, 5, 20, 21
 -9, 18, 19, 22
 -8, 13, 21, 24
Spherical
0, 1, 1, 2
 -1, 2, 3, 4
 -2, 4, 5, 5
 -2, 3, 7, 8
Hyperbolic
-1, 1, 1, 1
 0, 0, 1, 3
 -2, 3, 5, 6
 -3, 6, 6, 7
#endif

typedef struct {
	int         size;
	XPoint      offset;
	int         geometry;
	circle      c1, c2, c3, c4;
	int         color_offset;
	int         count;
	Bool        label, altgeom;
	apollonian_quadruple  *quad;
#ifdef DOFONT
	XFontStruct *font;
#endif
	int         time;
	int         game;
#ifdef STANDALONE
  eraser_state *eraser;
#endif
} apollonianstruct;

static apollonianstruct *apollonians = (apollonianstruct *) NULL;

#define FONT_HEIGHT 19
#define FONT_WIDTH 15
#define FONT_LENGTH 20
#define MAX_CHAR 10
#define K       2.15470053837925152902  /* 1+2/sqrt(3) */
#define MAXBEND 100 /* Do not want configurable by user since it will take too
	much time if increased. */

static int
gcd(int a, int b)
{
	int r;

	while (b) {
		r = a % b;
		a = b;
		b = r;
       	}
	return a;
}

static int
isqrt(int n)
{
	int y;

	if (n < 0)
		return -1;
	y = (int) (sqrt((double) n) + 0.5);
	return ((n == y*y) ? y : -1);
}

static void
dquad(int n, apollonian_quadruple *quad)
{
	int a, b, c, d;
	int counter = 0, B, C;
	
	for (a = 0; a < MAXBEND; a++) {
		B = (int) (K * a);
		for (b = a + 1; b <= B; b++) {
			C = (int) (((a + b) * (a + b)) / (4.0 * (b - a)));
			for (c = b; c <= C; c++) {
				d = isqrt(b*c-a*(b+c));
				if (d >= 0 && (gcd(a,gcd(b,c)) <= 1)) {
 					quad[counter].a = -a;
 					quad[counter].b = b;
 					quad[counter].c = c;
					quad[counter].d = -a+b+c-2*d;
					if (++counter >= n) {
						return;
					}
				}
			}
		}
	}
	(void) printf("found only %d below maximum bend of %d\n",
		counter, MAXBEND);
	for (; counter < n; counter++) {
 		quad[counter].a = -1; 
 		quad[counter].b = 2;
 		quad[counter].c = 2;
		quad[counter].d = 3;
	}
	return;
}

/*
 * Given a Descartes quadruple of bends (a,b,c,d), with a<0, find a
 * quadruple of circles, represented by (bend,bend*x,bend*y), such
 * that the circles have the given bends and the bends times the
 * centers are integers.
 *
 * This just performs an exaustive search, assuming that the outer
 * circle has center in the unit square.
 *
 * It is always sufficient to look in {(x,y):0<=y<=x<=1/2} for the
 * center of the outer circle, but this may not lead to a packing
 * that can be labelled with integer spherical and hyperbolic labels.
 * To effect the smaller search, replace FOR(a) with
 *
 *      for (pa = ea/2; pa <= 0; pa++) for (qa = pa; qa <= 0; qa++)
 */

#define For(v,l,h)	for (v = l; v <= h; v++)
#define FOR(z)		For(p##z,lop##z,hip##z) For(q##z,loq##z,hiq##z)
#define H(z)		((e##z*e##z+p##z*p##z+q##z*q##z)%2)
#define UNIT(z)		((abs(e##z)-1)*(abs(e##z)-1) >= p##z*p##z+q##z*q##z)
#define T(z,w)		is_tangent(e##z,p##z,q##z,e##w,p##w,q##w)
#define LO(r,z)		lo##r##z = iceil(e##z*(r##a+1),ea)-1
#define HI(r,z)		hi##r##z = iflor(e##z*(r##a-1),ea)-1
#define B(z)		LO(p,z); HI(p,z); LO(q,z); HI(q,z)

static int
is_quad(int a, int b, int c, int d)
{
	int s;

	s = a+b+c+d;
	return 2*(a*a+b*b+c*c+d*d) == s*s;
}

static Bool
is_tangent(int e1, int p1, int q1, int e2, int p2, int q2)
{
	int dx, dy, s;

	dx = p1*e2 - p2*e1;
	dy = q1*e2 - q2*e1;
	s = e1 + e2;
	return dx*dx + dy*dy == s*s;
}

static int
iflor(int a, int b)
{
	int q;

	if (b == 0) {
		(void) printf("iflor: b = 0\n");
		return 0;
	}
	if (a%b == 0)
		return a/b;
	q = abs(a)/abs(b);
	return ((a<0)^(b<0)) ? -q-1 : q;
}

static int
iceil(int a, int b)
{
	int q;

	if (b == 0) {
		(void) printf("iceil: b = 0\n");
		return 0;
	}
	if (a%b == 0)
		return a/b;
	q = abs(a)/abs(b);
	return ((a<0)^(b<0)) ? -q : 1+q;
}

static double
geom(int geometry, int e, int p, int q)
{
	int g = (geometry == spherical) ? -1 :
		(geometry == hyperbolic) ? 1 : 0;

	if (g)
		return (e*e + (1.0 - p*p - q*q) * g) / (2.0*e);
	(void) printf("geom: g = 0\n");
	return e;
}

static void
cquad(circle *c1, circle *c2, circle *c3, circle *c4)
{
	int ea, eb, ec, ed;
	int pa, pb, pc, pd;
	int qa, qb, qc, qd;
	int lopa, lopb, lopc, lopd;
	int hipa, hipb, hipc, hipd;
	int loqa, loqb, loqc, loqd;
	int hiqa, hiqb, hiqc, hiqd;

	ea = (int) c1->e;
	eb = (int) c2->e;
	ec = (int) c3->e;
	ed = (int) c4->e;
	if (ea >= 0)
		(void) printf("ea = %d\n", ea);
	if (!is_quad(ea,eb,ec,ed))
		(void) printf("Error not quad %d %d %d %d\n", ea, eb, ec, ed);
	lopa = loqa = ea;
	hipa = hiqa = 0;
	FOR(a) {
		B(b); B(c); B(d);
		if (H(a) && UNIT(a)) FOR(b) {
			if (H(b) && T(a,b)) FOR(c) {
				if (H(c) && T(a,c) && T(b,c)) FOR(d) {
				  if (H(d) && T(a,d) && T(b,d) && T(c,d)) {
				    c1->s = geom(spherical, ea, pa, qa);
				    c1->h = geom(hyperbolic, ea, pa, qa);
				    c2->s = geom(spherical, eb, pb, qb);
				    c2->h = geom(hyperbolic, eb, pb, qb);
				    c3->s = geom(spherical, ec, pc, qc);
				    c3->h = geom(hyperbolic, ec, pc, qc);
				    c4->s = geom(spherical, ed, pd, qd);
				    c4->h = geom(hyperbolic, ed, pd, qd);
				  }
				}
			}
		}
	}
}

static void
p(ModeInfo *mi, circle c)
{
	apollonianstruct *cp = &apollonians[MI_SCREEN(mi)];
	char string[10];
	double g, e;
	int g_width;

#ifdef DEBUG
	(void) printf("c.e=%g c.s=%g c.h=%g  c.x=%g c.y=%g\n",
		c.e, c.s, c.h, c.x, c.y);
#endif
	g = (cp->geometry == spherical) ? c.s : (cp->geometry == hyperbolic) ?
		c.h : c.e;
	if (c.e < 0.0) {
		if (g < 0.0)
			g = -g;
		if (MI_NPIXELS(mi) <= 2)
			XSetForeground(MI_DISPLAY(mi), MI_GC(mi),
				MI_WHITE_PIXEL(mi));
		else
			XSetForeground(MI_DISPLAY(mi), MI_GC(mi),
				MI_PIXEL(mi, ((int) ((g + cp->color_offset) *
					g)) % MI_NPIXELS(mi)));
		XDrawArc(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi),
			((int) (cp->size * (-cp->c1.e) * (c.x - 1.0) /
				(-2.0 * c.e) + cp->size / 2.0 + cp->offset.x)),
			((int) (cp->size * (-cp->c1.e) * (c.y - 1.0) /
				(-2.0 * c.e) + cp->size / 2.0 + cp->offset.y)),
			(int) (cp->c1.e * cp->size / c.e),
			(int) (cp->c1.e * cp->size / c.e), 0, 23040);
		if (!cp->label) {
#ifdef DEBUG
			(void) printf("%g\n", -g);
#endif
			return;
		}
		(void) sprintf(string, "%g", (g == 0.0) ? 0 : -g);
		if (cp->size >= 10 * FONT_WIDTH) {
		  /* hard code these to corners */
		  XDrawString(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi),
			((int) (cp->size * c.x / (2.0 * c.e))) + cp->offset.x,
			((int) (cp->size * c.y / (2.0 * c.e))) + FONT_HEIGHT,
			string, (g == 0.0) ? 1 : ((g < 10.0) ? 2 :
				((g < 100.0) ? 3 : 4)));
		}
		if (cp->altgeom && MI_HEIGHT(mi) >= 30 * FONT_WIDTH) {
		  XDrawString(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi),
			((int) (cp->size * c.x / (2.0 * c.e) + cp->offset.x)),
			((int) (cp->size * c.y / (2.0 * c.e) + MI_HEIGHT(mi) -
			FONT_HEIGHT / 2)), (char *) space_string[cp->geometry],
			strlen(space_string[cp->geometry]));
		}
		return;
	}
	if (MI_NPIXELS(mi) <= 2)
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_WHITE_PIXEL(mi));
	else
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi),
			MI_PIXEL(mi, ((int) ((g + cp->color_offset) * g)) %
				MI_NPIXELS(mi)));
	if (c.e == 0.0) {
		if (c.x == 0.0 && c.y != 0.0) {
			XDrawLine(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi),
				0, (int) ((c.y + 1.0) * cp->size / 2.0 + cp->offset.y),
				MI_WIDTH(mi),
				(int) ((c.y + 1.0) * cp->size / 2.0 + cp->offset.y));
		} else if (c.y == 0.0 && c.x != 0.0) {
			XDrawLine(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi),
				(int) ((c.x + 1.0) * cp->size / 2.0 + cp->offset.x), 0,
				(int) ((c.x + 1.0) * cp->size / 2.0 + cp->offset.x),
				MI_HEIGHT(mi));
		}
		return;
	}
	e = (cp->c1.e >= 0.0) ? 1.0 : -cp->c1.e;
	XFillArc(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi),
		((int) (cp->size * e * (c.x - 1.0) / (2.0 * c.e) +
			cp->size / 2.0 + cp->offset.x)),
		((int) (cp->size * e * (c.y - 1.0) / (2.0 * c.e) +
			cp->size / 2.0 + cp->offset.y)),
		(int) (e * cp->size / c.e), (int) (e * cp->size / c.e),
		0, 23040);
	if (!cp->label) {
#ifdef DEBUG
		(void) printf("%g\n", g);
#endif
		return;
	}
	if (MI_NPIXELS(mi) <= 2)
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_BLACK_PIXEL(mi));
	else
		XSetForeground(MI_DISPLAY(mi), MI_GC(mi),
			MI_PIXEL(mi, ((int) ((g + cp->color_offset) * g) +
				MI_NPIXELS(mi) / 2) % MI_NPIXELS(mi)));
	g_width = (g < 10.0) ? 1: ((g < 100.0) ? 2 : 3);
	if (c.e < e * cp->size / (FONT_LENGTH + 5 * g_width) && g < 1000.0) {
		(void) sprintf(string, "%g", g);
		XDrawString(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi),
			((int) (cp->size * e * c.x / (2.0 * c.e) +
				cp->size / 2.0 + cp->offset.x)) -
				g_width * FONT_WIDTH / 2,
			((int) (cp->size * e * c.y / (2.0 * c.e) +
				cp->size / 2.0 + cp->offset.y)) +
				FONT_HEIGHT / 2,
			string, g_width);
        }
}

#define BIG 7
static void
f(ModeInfo *mi, circle c1, circle c2, circle c3, circle c4, int depth)
{
	apollonianstruct *cp = &apollonians[MI_SCREEN(mi)];
	int e = (int) ((cp->c1.e >= 0.0) ? 1.0 : -cp->c1.e);
        circle c;

	if (depth > mi->recursion_depth) mi->recursion_depth = depth;

        c.e = 2*(c1.e+c2.e+c3.e) - c4.e;
        c.s = 2*(c1.s+c2.s+c3.s) - c4.s;
        c.h = 2*(c1.h+c2.h+c3.h) - c4.h;
        c.x = 2*(c1.x+c2.x+c3.x) - c4.x;
        c.y = 2*(c1.y+c2.y+c3.y) - c4.y;
        if (c.e == 0 ||
            c.e > cp->size * e || c.x / c.e > BIG || c.y / c.e > BIG ||
            c.x / c.e < -BIG || c.y / c.e < -BIG)
                return;
        p(mi, c);
        f(mi, c2, c3, c, c1, depth+1);
        f(mi, c1, c3, c, c2, depth+1);
        f(mi, c1, c2, c, c3, depth+1);
}

ENTRYPOINT void
free_apollonian (Display *display, apollonianstruct *cp)
{
	if (cp->quad != NULL) {
		(void) free((void *) cp->quad);
		cp->quad = (apollonian_quadruple *) NULL;
	}
#ifdef DOFONT
	if (cp->gc != None) {
		XFreeGC(display, cp->gc);
		cp->gc = None;
	}
	if (cp->font != None) {
		XFreeFont(display, cp->font);
		cp->font = None;
	}
#endif
}

#ifndef DEBUG
static void
randomize_c(int randomize, circle * c)
{
  if (randomize / 2) {
    double temp;

    temp = c->x;
    c->x = c->y;
    c->y = temp;
  }
  if (randomize % 2) {
    c->x = -c->x;
    c->y = -c->y;
  }
}
#endif

ENTRYPOINT void
init_apollonian (ModeInfo * mi)
{
	apollonianstruct *cp;
	int i;

	if (apollonians == NULL) {
		if ((apollonians = (apollonianstruct *) calloc(MI_NUM_SCREENS(mi),
					     sizeof (apollonianstruct))) == NULL)
			return;
	}
	cp = &apollonians[MI_SCREEN(mi)];

	cp->size = MAX(MIN(MI_WIDTH(mi), MI_HEIGHT(mi)) - 1, 1);
	cp->offset.x = (MI_WIDTH(mi) - cp->size) / 2;
	cp->offset.y = (MI_HEIGHT(mi) - cp->size) / 2;
	cp->color_offset = NRAND(MI_NPIXELS(mi));

#ifdef DOFONT
	if (cp->font == None) {
		if ((cp->font = getFont(MI_DISPLAY(mi))) == None)
			return False;
	}
#endif
	cp->label = label;
	cp->altgeom = cp->label && altgeom;

	if (cp->quad == NULL) {
		cp->count = ABS(MI_COUNT(mi));
		if ((cp->quad = (apollonian_quadruple *) malloc(cp->count *
			sizeof (apollonian_quadruple))) == NULL) {
			return;
		}
		dquad(cp->count, cp->quad);
	}
	cp->game = NRAND(PREDEF_CIRCLE_GAMES + cp->count);
	cp->geometry = (cp->game && cp->altgeom) ? NRAND(3) : 0;

	if (cp->game < PREDEF_CIRCLE_GAMES) {	
		cp->c1 = examples[cp->game][0];
		cp->c2 = examples[cp->game][1];
		cp->c3 = examples[cp->game][2];
		cp->c4 = examples[cp->game][3];
		/* do not label non int */
		cp->label = cp->label && (cp->c4.e == (int) cp->c4.e);
	} else { /* uses results of dquad, all int */
		i = cp->game - PREDEF_CIRCLE_GAMES;
		cp->c1.e = cp->quad[i].a;
		cp->c2.e = cp->quad[i].b;
		cp->c3.e = cp->quad[i].c;
		cp->c4.e = cp->quad[i].d;
		if (cp->geometry)
			cquad(&(cp->c1), &(cp->c2), &(cp->c3), &(cp->c4));
	}
	cp->time = 0;
#ifndef STANDALONE
	MI_CLEARWINDOW(mi);
#endif
	if (cp->game != 0) {
		double q123;

		if (cp->c1.e == 0.0 || cp->c1.e == -cp->c2.e)
			return;
		cp->c1.x = 0.0;
		cp->c1.y = 0.0;
		cp->c2.x = -(cp->c1.e + cp->c2.e) / cp->c1.e;
		cp->c2.y = 0;
		q123 = sqrt(cp->c1.e * cp->c2.e + cp->c1.e * cp->c3.e +
			cp->c2.e * cp->c3.e);
#ifdef DEBUG
		(void) printf("q123 = %g, ", q123);
#endif
		cp->c3.x = (cp->c1.e * cp->c1.e - q123 * q123) / (cp->c1.e *
			(cp->c1.e + cp->c2.e));
		cp->c3.y = -2.0 * q123 / (cp->c1.e + cp->c2.e);
		q123 = -cp->c1.e - cp->c2.e + q123;
		cp->c4.x = (cp->c1.e * cp->c1.e - q123 * q123) / (cp->c1.e *
			(cp->c1.e + cp->c2.e));
		cp->c4.y = -2.0 * q123 / (cp->c1.e + cp->c2.e);
#ifdef DEBUG
		(void) printf("q124 = %g\n", q123);
		(void) printf("%g %g %g %g %g %g %g %g\n",
			cp->c1.x, cp->c1.y, cp->c2.x, cp->c2.y,
			cp->c3.x, cp->c3.y, cp->c4.x, cp->c4.y);
#endif
	}
#ifndef DEBUG
	if (LRAND() & 1) {
		cp->c3.y = -cp->c3.y;
		cp->c4.y = -cp->c4.y;
	}
	i = NRAND(4);
	randomize_c(i, &(cp->c1));
	randomize_c(i, &(cp->c2));
	randomize_c(i, &(cp->c3));
	randomize_c(i, &(cp->c4));
#endif 

    mi->recursion_depth = -1;
}

ENTRYPOINT void
draw_apollonian (ModeInfo * mi)
{
	apollonianstruct *cp;

	if (apollonians == NULL)
		return;
	cp = &apollonians[MI_SCREEN(mi)];

#ifdef STANDALONE
    if (cp->eraser) {
      cp->eraser = erase_window (MI_DISPLAY(mi), MI_WINDOW(mi), cp->eraser);
      return;
    }
#endif

	MI_IS_DRAWN(mi) = True;

	if (cp->time < 5) {
		switch (cp->time) {
		case 0:
			p(mi, cp->c1);
			p(mi, cp->c2);
			p(mi, cp->c3);
			p(mi, cp->c4);
			break;
		case 1:
			f(mi, cp->c1, cp->c2, cp->c3, cp->c4, 0);
			break;
		case 2:
			f(mi, cp->c1, cp->c2, cp->c4, cp->c3, 0);
			break;
		case 3:
			f(mi, cp->c1, cp->c3, cp->c4, cp->c2, 0);
			break;
		case 4:
			f(mi, cp->c2, cp->c3, cp->c4, cp->c1, 0);
		}
	}
	if (++cp->time > MI_CYCLES(mi))
      {
#ifdef STANDALONE
        cp->eraser = erase_window (MI_DISPLAY(mi), MI_WINDOW(mi), cp->eraser);
#endif /* STANDALONE */
		init_apollonian(mi);
      }
}

ENTRYPOINT void
reshape_apollonian(ModeInfo * mi, int width, int height)
{
  XClearWindow (MI_DISPLAY (mi), MI_WINDOW(mi));
  init_apollonian (mi);
}

ENTRYPOINT Bool
apollonian_handle_event (ModeInfo *mi, XEvent *event)
{
  if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      reshape_apollonian (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
      return True;
    }
  return False;
}

ENTRYPOINT void
release_apollonian (ModeInfo * mi)
{
	if (apollonians != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_apollonian(MI_DISPLAY(mi), &apollonians[screen]);
		(void) free((void *) apollonians);
		apollonians = (apollonianstruct *) NULL;
	}
}

XSCREENSAVER_MODULE ("Apollonian", apollonian)

#endif /* MODE_apollonian */
