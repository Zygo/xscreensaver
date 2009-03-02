/*****************************************************************************
 * #ident "Id: main.c,v 3.27 2002-01-06 16:23:01+02 rl Exp "
 * kaleido
 *
 *	Kaleidoscopic construction of uniform polyhedra
 *	Copyright (c) 1991-2002 Dr. Zvi Har'El <rl@math.technion.ac.il>
 *
 *	Redistribution and use in source and binary forms, with or without
 *	modification, are permitted provided that the following conditions
 *	are met:
 *
 *	1. Redistributions of source code must retain the above copyright
 *	   notice, this list of conditions and the following disclaimer.
 *
 *	2. Redistributions in binary form must reproduce the above copyright
 *	   notice, this list of conditions and the following disclaimer in
 *	   the documentation and/or other materials provided with the
 *	   distribution.
 *
 *	3. The end-user documentation included with the redistribution,
 *	   if any, must include the following acknowledgment:
 *		"This product includes software developed by
 *		 Dr. Zvi Har'El (http://www.math.technion.ac.il/~rl/)."
 *	   Alternately, this acknowledgment may appear in the software itself,
 *	   if and wherever such third-party acknowledgments normally appear.
 *
 *	This software is provided 'as-is', without any express or implied
 *	warranty.  In no event will the author be held liable for any
 *	damages arising from the use of this software.
 *
 *	Author:
 *		Dr. Zvi Har'El,
 *		Deptartment of Mathematics,
 *		Technion, Israel Institue of Technology,
 *		Haifa 32000, Israel.
 *		E-Mail: rl@math.technion.ac.il
 *
 *	ftp://ftp.math.technion.ac.il/kaleido/
 *	http://www.mathconsult.ch/showroom/unipoly/
 *
 *	Adapted for xscreensaver by Jamie Zawinski <jwz@jwz.org> 25-Apr-2004
 *
 *****************************************************************************
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "polyhedra.h"

extern const char *progname;

#ifndef MAXLONG
#define MAXLONG 0x7FFFFFFF
#endif
#ifndef MAXDIGITS
#define MAXDIGITS 10 /* (int)log10((double)MAXLONG) + 1 */
#endif

#ifndef DBL_EPSILON
#define DBL_EPSILON 2.2204460492503131e-16
#endif
#define BIG_EPSILON 3e-2
#define AZ M_PI/7  /* axis azimuth */
#define EL M_PI/17 /* axis elevation */

#define Err(x) do {\
	fprintf (stderr, "%s: %s\n", progname, (x)); \
	exit (1); \
    } while(0)

#define Free(lvalue) do {\
	if (lvalue) {\
	    free((char*) lvalue);\
	    lvalue=0;\
	}\
    } while(0)

#define Matfree(lvalue,n) do {\
	if (lvalue) \
	    matfree((char*) lvalue, n);\
	lvalue=0;\
    } while(0)

#define Malloc(lvalue,n,type) do {\
	if (!(lvalue = (type*) malloc((n) * sizeof(type)))) \
	    Err("out of memory");\
    } while(0)

#define Realloc(lvalue,n,type) do {\
	if (!(lvalue = (type*) realloc(lvalue, (n) * sizeof(type)))) \
	    Err("out of memory");\
    } while(0)

#define Calloc(lvalue,n,type) do {\
	if (!(lvalue = (type*) calloc(n, sizeof(type))))\
	    Err("out of memory");\
    } while(0)

#define Matalloc(lvalue,n,m,type) do {\
	if (!(lvalue = (type**) matalloc(n, (m) * sizeof(type))))\
	    Err("out of memory");\
    } while(0)

#define Sprintfrac(lvalue,x) do {\
	if (!(lvalue=sprintfrac(x)))\
	    return 0;\
    } while(0)

#define numerator(x) (frac(x), frax.n)
#define denominator(x) (frac(x), frax.d)
#define compl(x) (frac(x), (double) frax.n / (frax.n-frax.d))

typedef struct {
    double x, y, z;
} Vector;

typedef struct {
  /* NOTE: some of the int's can be replaced by short's, char's,
     or even bit fields, at the expense of readability!!!*/
  int index; /* index to the standard list, the array uniform[] */
  int N; /* number of faces types (atmost 5)*/
  int M; /* vertex valency  (may be big for dihedral polyhedra) */
  int V; /* vertex count */
  int E; /* edge count */
  int F; /* face count */
  int D; /* density */
  int chi; /* Euler characteristic */
  int g; /* order of symmetry group */
  int K; /* symmetry type: D=2, T=3, O=4, I=5 */
  int hemi;/* flag hemi polyhedron */
  int onesided;/* flag onesided polyhedron */
  int even; /* removed face in pqr| */
  int *Fi; /* face counts by type (array N)*/
  int *rot; /* vertex configuration (array M of 0..N-1) */
  int *snub; /* snub triangle configuration (array M of 0..1) */
  int *firstrot; /* temporary for vertex generation (array V) */
  int *anti; /* temporary for direction of ideal vertices (array E) */
  int *ftype; /* face types (array F) */
  int **e; /* edges (matrix 2 x E of 0..V-1)*/
  int **dual_e; /* dual edges (matrix 2 x E of 0..F-1)*/
  int **incid; /* vertex-face incidence (matrix M x V of 0..F-1)*/
  int **adj; /* vertex-vertex adjacency (matrix M x V of 0..V-1)*/
  double p[4]; /* p, q and r; |=0 */
  double minr; /* smallest nonzero inradius */
  double gon; /* basis type for dihedral polyhedra */
  double *n; /* number of side of a face of each type (array N) */
  double *m; /* number of faces at a vertex of each type (array N) */
  double *gamma; /* fundamental angles in radians (array N) */
  char *polyform; /* printable Wythoff symbol */
  char *config; /* printable vertex configuration */
  char *group; /* printable group name */
  char *name; /* name, standard or manifuctured */
  char *dual_name; /* dual name, standard or manifuctured */
  char *class;
  char *dual_class;
  Vector *v; /* vertex coordinates (array V) */
  Vector *f; /* face coordinates (array F)*/
} Polyhedron;

typedef struct {
  long n,d;
} Fraction;

static Polyhedron *polyalloc(void);
static Vector rotate(Vector vertex, Vector axis, double angle);

static Vector sum3(Vector a, Vector b, Vector c);
static Vector scale(double k, Vector a);
static Vector sum(Vector a, Vector b);
static Vector diff(Vector a, Vector b);
static Vector pole (double r, Vector a, Vector b, Vector c);
static Vector cross(Vector a, Vector b);
static double dot(Vector a, Vector b);
static int same(Vector a, Vector b, double epsilon);

static char *sprintfrac(double x);

static void frac(double x);
static void matfree(void *mat, int rows);
static void *matalloc(int rows, int row_size);

static Fraction frax;


static const struct {
  char *Wythoff, *name, *dual, *group, *class, *dual_class;
  short Coxeter, Wenninger;
} uniform[] = {

  /****************************************************************************
   *		Dihedral Schwarz Triangles (D5 only)
   ***************************************************************************/

							   /* (2 2 5) (D1/5) */
  /*  1 */	{"2 5|2",	"Pentagonal Prism",
				"Pentagonal Dipyramid",
				"Dihedral (D[1/5])",
				"",
				"",
				0, 0},

  /*  2 */	{"|2 2 5",	"Pentagonal Antiprism",
				"Pentagonal Deltohedron",
				"Dihedral (D[1/5])",
				"",
				"",
				0, 0},
							 /* (2 2 5/2) (D2/5) */
  /*  3 */	{"2 5/2|2",	"Pentagrammic Prism",
				"Pentagrammic Dipyramid",
				"Dihedral (D[2/5])",
				"",
				"",
				0, 0},

  /*  4 */	{"|2 2 5/2",	"Pentagrammic Antiprism",
				"Pentagrammic Deltohedron",
				"Dihedral (D[2/5])",
				"",
				"",
				0, 0},
							 /* (5/3 2 2) (D3/5) */

  /*  5 */	{"|2 2 5/3",	"Pentagrammic Crossed Antiprism",
				"Pentagrammic Concave Deltohedron",
				"Dihedral (D[3/5])",
				"",
				"",
				0, 0},

  /****************************************************************************
   *		Tetrahedral
   ***************************************************************************/

							     /* (2 3 3) (T1) */
  /*  6 */	{"3|2 3",	"Tetrahedron",
				"Tetrahedron",
				"Tetrahedral (T[1])",
				"Platonic Solid",
				"Platonic Solid",
				15, 1},

  /*  7 */	{"2 3|3",	"Truncated Tetrahedron",
				"Triakistetrahedron",
				"Tetrahedral (T[1])",
				"Archimedian Solid",
				"Catalan Solid",
				16, 6},
							   /* (3/2 3 3) (T2) */
  /*  8 */	{"3/2 3|3",	"Octahemioctahedron",
				"Octahemioctacron",
				"Tetrahedral (T[2])",
				"",
				"",
				37, 68},

							   /* (3/2 2 3) (T3) */
  /*  9 */	{"3/2 3|2",	"Tetrahemihexahedron",
				"Tetrahemihexacron",
				"Tetrahedral (T[3])",
				"",
				"",
				36, 67},

  /****************************************************************************
   *		Octahedral
   ***************************************************************************/

							     /* (2 3 4) (O1) */
  /* 10 */	{"4|2 3",	"Octahedron",
				"Cube",
				"Octahedral (O[1])",
				"Platonic Solid",
				"Platonic Solid",
				17, 2},

  /* 11 */	{"3|2 4",	"Cube",
				"Octahedron",
				"Octahedral (O[1])",
				"Platonic Solid",
				"Platonic Solid",
				18, 3},

  /* 12 */	{"2|3 4",	"Cuboctahedron",
				"Rhombic Dodecahedron",
				"Octahedral (O[1])",
				"Archimedian Solid",
				"Catalan Solid",
				19, 11},

  /* 13 */	{"2 4|3",	"Truncated Octahedron",
				"Tetrakishexahedron",
				"Octahedral (O[1])",
				"Archimedian Solid",
				"Catalan Solid",
				20, 7},

  /* 14 */	{"2 3|4",	"Truncated Cube",
				"Triakisoctahedron",
				"Octahedral (O[1])",
				"Archimedian Solid",
				"Catalan Solid",
				21, 8},

  /* 15 */	{"3 4|2",	"Rhombicuboctahedron",
				"Deltoidal Icositetrahedron",
				"Octahedral (O[1])",
				"Archimedian Solid",
				"Catalan Solid",
				22, 13},

  /* 16 */	{"2 3 4|",	"Truncated Cuboctahedron",
				"Disdyakisdodecahedron",
				"Octahedral (O[1])",
				"Archimedian Solid",
				"Catalan Solid",
				23, 15},

  /* 17 */	{"|2 3 4",	"Snub Cube",
				"Pentagonal Icositetrahedron",
				"Octahedral (O[1])",
				"Archimedian Solid",
				"Catalan Solid",
				24, 17},
							  /* (3/2 4 4) (O2b) */

  /* 18 */	{"3/2 4|4",	"Small Cubicuboctahedron",
				"Small Hexacronic Icositetrahedron",
				"Octahedral (O[2b])",
				"",
				"",
				38, 69},
							   /* (4/3 3 4) (O4) */

  /* 19 */	{"3 4|4/3",	"Great Cubicuboctahedron",
				"Great Hexacronic Icositetrahedron",
				"Octahedral (O[4])",
				"",
				"",
				50, 77},

  /* 20 */	{"4/3 4|3",	"Cubohemioctahedron",
				"Hexahemioctacron",
				"Octahedral (O[4])",
				"",
				"",
				51, 78},

  /* 21 */	{"4/3 3 4|",	"Cubitruncated Cuboctahedron",
				"Tetradyakishexahedron",
				"Octahedral (O[4])",
				"",
				"",
				52, 79},
							   /* (3/2 2 4) (O5) */

  /* 22 */	{"3/2 4|2",	"Great Rhombicuboctahedron",
				"Great Deltoidal Icositetrahedron",
				"Octahedral (O[5])",
				"",
				"",
				59, 85},

  /* 23 */	{"3/2 2 4|",	"Small Rhombihexahedron",
				"Small Rhombihexacron",
				"Octahedral (O[5])",
				"",
				"",
				60, 86},
							   /* (4/3 2 3) (O7) */

  /* 24 */	{"2 3|4/3",	"Stellated Truncated Hexahedron",
				"Great Triakisoctahedron",
				"Octahedral (O[7])",
				"",
				"",
				66, 92},

  /* 25 */	{"4/3 2 3|",	"Great Truncated Cuboctahedron",
				"Great Disdyakisdodecahedron",
				"Octahedral (O[7])",
				"",
				"",
				67, 93},
							/* (4/3 3/2 2) (O11) */

  /* 26 */	{"4/3 3/2 2|",	"Great Rhombihexahedron",
				"Great Rhombihexacron",
				"Octahedral (O[11])",
				"",
				"",
				82, 103},

  /****************************************************************************
   *		Icosahedral
   ***************************************************************************/

							     /* (2 3 5) (I1) */
  /* 27 */	{"5|2 3",	"Icosahedron",
				"Dodecahedron",
				"Icosahedral (I[1])",
				"Platonic Solid",
				"Platonic Solid",
				25, 4},

  /* 28 */	{"3|2 5",	"Dodecahedron",
				"Icosahedron",
				"Icosahedral (I[1])",
				"Platonic Solid",
				"Platonic Solid",
				26, 5},

  /* 29 */	{"2|3 5",	"Icosidodecahedron",
				"Rhombic Triacontahedron",
				"Icosahedral (I[1])",
				"Archimedian Solid",
				"Catalan Solid",
				28, 12},

  /* 30 */	{"2 5|3",	"Truncated Icosahedron",
				"Pentakisdodecahedron",
				"Icosahedral (I[1])",
				"Archimedian Solid",
				"Catalan Solid",
				27, 9},

  /* 31 */	{"2 3|5",	"Truncated Dodecahedron",
				"Triakisicosahedron",
				"Icosahedral (I[1])",
				"Archimedian Solid",
				"Catalan Solid",
				29, 10},

  /* 32 */	{"3 5|2",	"Rhombicosidodecahedron",
				"Deltoidal Hexecontahedron",
				"Icosahedral (I[1])",
				"Archimedian Solid",
				"Catalan Solid",
				30, 14},

  /* 33 */	{"2 3 5|",	"Truncated Icosidodechedon",
				"Disdyakistriacontahedron",
				"Icosahedral (I[1])",
				"Archimedian Solid",
				"Catalan Solid",
				31, 16},

  /* 34 */	{"|2 3 5",	"Snub Dodecahedron",
				"Pentagonal Hexecontahedron",
				"Icosahedral (I[1])",
				"Archimedian Solid",
				"Catalan Solid",
				32, 18},
							  /* (5/2 3 3) (I2a) */

  /* 35 */	{"3|5/2 3",	"Small Ditrigonal Icosidodecahedron",
				"Small Triambic Icosahedron",
				"Icosahedral (I[2a])",
				"",
				"",
				39, 70},

  /* 36 */	{"5/2 3|3",	"Small Icosicosidodecahedron",
				"Small Icosacronic Hexecontahedron",
				"Icosahedral (I[2a])",
				"",
				"",
				40, 71},

  /* 37 */	{"|5/2 3 3",	"Small Snub Icosicosidodecahedron",
				"Small Hexagonal Hexecontahedron",
				"Icosahedral (I[2a])",
				"",
				"",
				41, 110},
							  /* (3/2 5 5) (I2b) */

  /* 38 */	{"3/2 5|5",	"Small Dodecicosidodecahedron",
				"Small Dodecacronic Hexecontahedron",
				"Icosahedral (I[2b])",
				"",
				"",
				42, 72},
							   /* (2 5/2 5) (I3) */

  /* 39 */	{"5|2 5/2",	"Small Stellated Dodecahedron",
				"Great Dodecahedron",
				"Icosahedral (I[3])",
				"Truncated Kepler-Poinsot Solid",
				"",
				43, 20},

  /* 40 */	{"5/2|2 5",	"Great Dodecahedron",
				"Small Stellated Dodecahedron",
				"Icosahedral (I[3])",
				"",
				"",
				44, 21},

  /* 41 */	{"2|5/2 5",	"Great Dodecadodecahedron",
				"Medial Rhombic Triacontahedron",
				"Icosahedral (I[3])",
				"",
				"",
				45, 73},

  /* 42 */	{"2 5/2|5",	"Truncated Great Dodecahedron",
				"Small Stellapentakisdodecahedron",
				"Icosahedral (I[3])",
				"Truncated Kepler-Poinsot Solid",
				"",
				47, 75},

  /* 43 */	{"5/2 5|2",	"Rhombidodecadodecahedron",
				"Medial Deltoidal Hexecontahedron",
				"Icosahedral (I[3])",
				"",
				"",
				48, 76},

  /* 44 */	{"2 5/2 5|",	"Small Rhombidodecahedron",
				"Small Rhombidodecacron",
				"Icosahedral (I[3])",
				"",
				"",
				46, 74},

  /* 45 */	{"|2 5/2 5",	"Snub Dodecadodecahedron",
				"Medial Pentagonal Hexecontahedron",
				"Icosahedral (I[3])",
				"",
				"",
				49, 111},
							   /* (5/3 3 5) (I4) */

  /* 46 */	{"3|5/3 5",	"Ditrigonal Dodecadodecahedron",
				"Medial Triambic Icosahedron",
				"Icosahedral (I[4])",
				"",
				"",
				53, 80},

  /* 47 */	{"3 5|5/3",	"Great Ditrigonal Dodecicosidodecahedron",
			       "Great Ditrigonal Dodecacronic Hexecontahedron",
				"Icosahedral (I[4])",
				"",
				"",
				54, 81},

  /* 48 */	{"5/3 3|5",	"Small Ditrigonal Dodecicosidodecahedron",
			       "Small Ditrigonal Dodecacronic Hexecontahedron",
				"Icosahedral (I[4])",
				"",
				"",
				55, 82},

  /* 49 */	{"5/3 5|3",	"Icosidodecadodecahedron",
				"Medial Icosacronic Hexecontahedron",
				"Icosahedral (I[4])",
				"",
				"",
				56, 83},

  /* 50 */	{"5/3 3 5|",	"Icositruncated Dodecadodecahedron",
				"Tridyakisicosahedron",
				"Icosahedral (I[4])",
				"",
				"",
				57, 84},

  /* 51 */	{"|5/3 3 5",	"Snub Icosidodecadodecahedron",
				"Medial Hexagonal Hexecontahedron",
				"Icosahedral (I[4])",
				"",
				"",
				58, 112},
							  /* (3/2 3 5) (I6b) */

  /* 52 */	{"3/2|3 5",	"Great Ditrigonal Icosidodecahedron",
				"Great Triambic Icosahedron",
				"Icosahedral (I[6b])",
				"",
				"",
				61, 87},

  /* 53 */	{"3/2 5|3",	"Great Icosicosidodecahedron",
				"Great Icosacronic Hexecontahedron",
				"Icosahedral (I[6b])",
				"",
				"",
				62, 88},

  /* 54 */	{"3/2 3|5",	"Small Icosihemidodecahedron",
				"Small Icosihemidodecacron",
				"Icosahedral (I[6b])",
				"",
				"",
				63, 89},

  /* 55 */	{"3/2 3 5|",	"Small Dodecicosahedron",
				"Small Dodecicosacron",
				"Icosahedral (I[6b])",
				"",
				"",
				64, 90},
							  /* (5/4 5 5) (I6c) */

  /* 56 */	{"5/4 5|5",	"Small Dodecahemidodecahedron",
				"Small Dodecahemidodecacron",
				"Icosahedral (I[6c])",
				"",
				"",
				65, 91},
							   /* (2 5/2 3) (I7) */

  /* 57 */	{"3|2 5/2",	"Great Stellated Dodecahedron",
				"Great Icosahedron",
				"Icosahedral (I[7])",
				"",
				"",
				68, 22},

  /* 58 */	{"5/2|2 3",	"Great Icosahedron",
				"Great Stellated Dodecahedron",
				"Icosahedral (I[7])",
				"",
				"",
				69, 41},

  /* 59 */	{"2|5/2 3",	"Great Icosidodecahedron",
				"Great Rhombic Triacontahedron",
				"Icosahedral (I[7])",
				"Truncated Kepler-Poinsot Solid",
				"",
				70, 94},

  /* 60 */	{"2 5/2|3",	"Great Truncated Icosahedron",
				"Great Stellapentakisdodecahedron",
				"Icosahedral (I[7])",
				"Truncated Kepler-Poinsot Solid",
				"",
				71, 95},

  /* 61 */	{"2 5/2 3|",	"Rhombicosahedron",
				"Rhombicosacron",
				"Icosahedral (I[7])",
				"",
				"",
				72, 96},

  /* 62 */	{"|2 5/2 3",	"Great Snub Icosidodecahedron",
				"Great Pentagonal Hexecontahedron",
				"Icosahedral (I[7])",
				"",
				"",
				73, 113},
							   /* (5/3 2 5) (I9) */

  /* 63 */	{"2 5|5/3",	"Small Stellated Truncated Dodecahedron",
				"Great Pentakisdodekahedron",
				"Icosahedral (I[9])",
				"",
				"",
				74, 97},

  /* 64 */	{"5/3 2 5|",	"Truncated Dodecadodecahedron",
				"Medial Disdyakistriacontahedron",
				"Icosahedral (I[9])",
				"",
				"",
				75, 98},

  /* 65 */	{"|5/3 2 5",	"Inverted Snub Dodecadodecahedron",
				"Medial Inverted Pentagonal Hexecontahedron",
				"Icosahedral (I[9])",
				"",
				"",
				76, 114},
						       /* (5/3 5/2 3) (I10a) */

  /* 66 */	{"5/2 3|5/3",	"Great Dodecicosidodecahedron",
				"Great Dodecacronic Hexecontahedron",
				"Icosahedral (I[10a])",
				"",
				"",
				77, 99},

  /* 67 */	{"5/3 5/2|3",	"Small Dodecahemicosahedron",
				"Small Dodecahemicosacron",
				"Icosahedral (I[10a])",
				"",
				"",
				78, 100},

  /* 68 */	{"5/3 5/2 3|",	"Great Dodecicosahedron",
				"Great Dodecicosacron",
				"Icosahedral (I[10a])",
				"",
				"",
				79, 101},

  /* 69 */	{"|5/3 5/2 3",	"Great Snub Dodecicosidodecahedron",
				"Great Hexagonal Hexecontahedron",
				"Icosahedral (I[10a])",
				"",
				"",
				80, 115},
							 /* (5/4 3 5) (I10b) */

  /* 70 */	{"5/4 5|3",	"Great Dodecahemicosahedron",
				"Great Dodecahemicosacron",
				"Icosahedral (I[10b])",
				"",
				"",
				81, 102},
							  /* (5/3 2 3) (I13) */

  /* 71 */	{"2 3|5/3",	"Great Stellated Truncated Dodecahedron",
				"Great Triakisicosahedron",
				"Icosahedral (I[13])",
				"",
				"",
				83, 104},

  /* 72 */	{"5/3 3|2",	"Great Rhombicosidodecahedron",
				"Great Deltoidal Hexecontahedron",
				"Icosahedral (I[13])",
				"",
				"",
				84, 105},

  /* 73 */	{"5/3 2 3|",	"Great Truncated Icosidodecahedron",
				"Great Disdyakistriacontahedron",
				"Icosahedral (I[13])",
				"",
				"",
				87, 108},

  /* 74 */	{"|5/3 2 3",	"Great Inverted Snub Icosidodecahedron",
				"Great Inverted Pentagonal Hexecontahedron",
				"Icosahedral (I[13])",
				"",
				"",
				88, 116},
						     /* (5/3 5/3 5/2) (I18a) */

  /* 75 */	{"5/3 5/2|5/3",	"Great Dodecahemidodecahedron",
				"Great Dodecahemidodecacron",
				"Icosahedral (I[18a])",
				"",
				"",
				86, 107},
						       /* (3/2 5/3 3) (I18b) */

  /* 76 */	{"3/2 3|5/3",	"Great Icosihemidodecahedron",
				"Great Icosihemidodecacron",
				"Icosahedral (I[18b])",
				"",
				"",
				85, 106},
						      /* (3/2 3/2 5/3) (I22) */

  /* 77 */	{"|3/2 3/2 5/2","Small Retrosnub Icosicosidodecahedron",
				"Small Hexagrammic Hexecontahedron",
				"Icosahedral (I[22])",
				"",
				"",
				91, 118},
							/* (3/2 5/3 2) (I23) */

  /* 78 */	{"3/2 5/3 2|",	"Great Rhombidodecahedron",
				"Great Rhombidodecacron",
				"Icosahedral (I[23])",
				"",
				"",
				89, 109},

  /* 79 */	{"|3/2 5/3 2",	"Great Retrosnub Icosidodecahedron",
				"Great Pentagrammic Hexecontahedron",
				"Icosahedral (I[23])",
				"",
				"",
				90, 117},

  /****************************************************************************
   *		Last But Not Least
   ***************************************************************************/

  /* 80 */    {"3/2 5/3 3 5/2",	"Great Dirhombicosidodecahedron",
				"Great Dirhombicosidodecacron",
				"Non-Wythoffian",
				"",
				"",
				92, 119}
};

static int last_uniform = sizeof (uniform) / sizeof (uniform[0]);



static int unpacksym(char *sym, Polyhedron *P);
static int moebius(Polyhedron *P);
static int decompose(Polyhedron *P);
static int guessname(Polyhedron *P);
static int newton(Polyhedron *P, int need_approx);
static int exceptions(Polyhedron *P);
static int count(Polyhedron *P);
static int configuration(Polyhedron *P);
static int vertices(Polyhedron *P);
static int faces(Polyhedron *P);
static int edgelist(Polyhedron *P);

static Polyhedron *
kaleido(char *sym, int need_coordinates, int need_edgelist, int need_approx,
	int just_list)
{
  Polyhedron *P;
  /*
   * Allocate a Polyhedron structure P.
   */
  if (!(P = polyalloc()))
    return 0;
  /*
   * Unpack input symbol into P.
   */
  if (!unpacksym(sym, P))
    return 0;
  /*
   * Find Mebius triangle, its density and Euler characteristic.
   */
  if (!moebius(P))
    return 0;
  /*
   * Decompose Schwarz triangle.
   */
  if (!decompose(P))
    return 0;
  /*
   * Find the names of the polyhedron and its dual.
   */
  if (!guessname(P))
    return 0;
  if (just_list)
    return P;
  /*
   * Solve Fundamental triangles, optionally printing approximations.
   */
  if (!newton(P,need_approx))
    return 0;
  /*
   * Deal with exceptional polyhedra.
   */
  if (!exceptions(P))
    return 0;
  /*
   * Count edges and faces, update density and characteristic if needed.
   */
  if (!count(P))
    return 0;
  /*
   * Generate printable vertex configuration.
   */
  if (!configuration(P))
    return 0;
  /*
   * Compute coordinates.
   */
  if (!need_coordinates && !need_edgelist)
    return P;
  if (!vertices(P))
    return 0;
  if (!faces (P))
    return 0;
  /*
   * Compute edgelist.
   */
  if (!need_edgelist)
    return P;
  if (!edgelist(P))
    return 0;
  return P;
}

/*
 * Allocate a blank Polyhedron structure and initialize some of its nonblank
 * fields.
 *
 * Array and matrix field are allocated when needed.
 */
static Polyhedron *
polyalloc()
{
  Polyhedron *P;
  Calloc(P, 1, Polyhedron);
  P->index = -1;
  P->even = -1;
  P->K = 2;
  return P;
}

/*
 * Free the struture allocated by polyalloc(), as well as all the array and
 * matrix fields.
 */
static void
polyfree(Polyhedron *P)
{
  Free(P->Fi);
  Free(P->n);
  Free(P->m);
  Free(P->gamma);
  Free(P->rot);
  Free(P->snub);
  Free(P->firstrot);
  Free(P->anti);
  Free(P->ftype);
  Free(P->polyform);
  Free(P->config);
  if (P->index < 0) {
    Free(P->name);
    Free(P->dual_name);
  }
  Free(P->v);
  Free(P->f);
  Matfree(P->e, 2);
  Matfree(P->dual_e, 2);
  Matfree(P->incid, P->M);
  Matfree(P->adj, P->M);
  free(P);
}

static void *
matalloc(int rows, int row_size)
{
  void **mat;
  int i = 0;
  if (!(mat = malloc(rows * sizeof (void *))))
    return 0;
  while ((mat[i] = malloc(row_size)) && ++i < rows)
    ;
  if (i == rows)
    return (void *)mat;
  while (--i >= 0)
    free(mat[i]);
  free(mat);
  return 0;
}

static void
matfree(void *mat, int rows)
{
  while (--rows >= 0)
    free(((void **)mat)[rows]);
  free(mat);
}

/*
 * compute the mathematical modulus function.
 */
static int
mod (int i, int j)
{
  return (i%=j)>=0?i:j<0?i-j:i+j;
}


/*
 * Find the numerator and the denominator using the Euclidean algorithm.
 */
static void
frac(double x)
{
  static const Fraction zero = {0,1}, inf = {1,0};
  Fraction r0, r;
  long f;
  double s = x;
  r = zero;
  frax = inf;
  for (;;) {
    if (fabs(s) > (double) MAXLONG)
      return;
    f = (long) floor (s);
    r0 = r;
    r = frax;
    frax.n = frax.n * f + r0.n;
    frax.d = frax.d * f + r0.d;
    if (x == (double)frax.n/(double)frax.d)
      return;
    s = 1 / (s - f);
  }
}


/*
 * Unpack input symbol: Wythoff symbol or an index to uniform[]. The symbol is
 * a # followed by a number, or a three fractions and a bar in some order. We
 * allow no bars only if it result from the input symbol #80.
 */
static int
unpacksym(char *sym, Polyhedron *P)
{
  int i = 0, n, d, bars = 0;
  char c;
  while ((c = *sym++) && isspace(c))
    ;
  if (!c) Err("no data");
  if (c == '#') {
    while ((c = *sym++) && isspace(c))
      ;
    if (!c)
      Err("no digit after #");
    if (!isdigit(c))
      Err("not a digit");
    n = c - '0';
    while ((c = *sym++) && isdigit(c))
      n = n * 10 + c - '0';
    if (!n)
      Err("zero index");
    if (n > last_uniform)
      Err("index too big");
    sym--;
    while ((c = *sym++) && isspace(c))
      ;
    if (c)
      Err("data exceeded");
    sym = uniform[P->index = n - 1].Wythoff;
  } else
    sym--;

  for (;;) {
    while ((c = *sym++) && isspace(c))
      ;
    if (!c) {
      if (i == 4 && (bars || P->index == last_uniform - 1))
        return 1;
      if (!bars)
        Err("no bars");
      Err("not enough fractions");
    }
    if (i == 4)
      Err("data exceeded");
    if (c == '|'){
      if (++bars > 1)
        Err("too many bars");
      P->p[i++] = 0;
      continue;
    }
    if (!isdigit(c))
      Err("not a digit");
    n = c - '0';
    while ((c = *sym++) && isdigit(c))
      n = n * 10 + c - '0';
    if (c && isspace (c))
      while ((c = *sym++) && isspace(c))
        ;
    if (c != '/') {
      sym--;
      if ((P->p[i++] = n) <= 1)
        Err("fraction<=1");
      continue;
    }
    while ((c = *sym++) && isspace(c))
      ;
    if (!c || !isdigit(c))
      return 0;
    d = c - '0';
    while ((c = *sym++) && isdigit(c))
      d = d * 10 + c - '0';
    if (!d)
      Err("zero denominator");
    sym--;
    if ((P->p[i++] = (double) n / d) <= 1)
      Err("fraction<=1");
  }
}

/*
 * Using Wythoff symbol (p|qr, pq|r, pqr| or |pqr), find the Moebius triangle
 * (2 3 K) (or (2 2 n)) of the Schwarz triangle (pqr), the order g of its
 * symmetry group, its Euler characteristic chi, and its covering density D.
 * g is the number of copies of (2 3 K) covering the sphere, i.e.,
 *
 *		g * pi * (1/2 + 1/3 + 1/K - 1) = 4 * pi
 *
 * D is the number of times g copies of (pqr) cover the sphere, i.e.
 *
 *		D * 4 * pi = g * pi * (1/p + 1/q + 1/r - 1)
 *
 * chi is V - E + F, where F = g is the number of triangles, E = 3*g/2 is the
 * number of triangle edges, and V = Vp+ Vq+ Vr, with Vp = g/(2*np) being the
 * number of vertices with angle pi/p (np is the numerator of p).
 */
static int
moebius(Polyhedron *P)
{
  int twos = 0, j, len = 1;
  /*
   * Arrange Wythoff symbol in a presentable form. In the same time check the
   * restrictions on the three fractions: They all have to be greater then one,
   * and the numerators 4 or 5 cannot occur together.  We count the ocurrences
   * of 2 in `two', and save the largest numerator in `P->K', since they
   * reflect on the symmetry group.
   */
  P->K = 2;
  if (P->index == last_uniform - 1) {
    Malloc(P->polyform, ++len, char);
    strcpy(P->polyform, "|");
  } else
    Calloc(P->polyform, len, char);
  for (j = 0; j < 4; j++) {
    if (P->p[j]) {
      char *s;
      Sprintfrac(s, P->p[j]);
      if (j && P->p[j-1]) {
        Realloc(P->polyform, len += strlen (s) + 1, char);
        strcat(P->polyform, " ");
      } else
        Realloc (P->polyform, len += strlen (s), char);
      strcat(P->polyform, s);
      free(s);
      if (P->p[j] != 2) {
        int k;
        if ((k = numerator (P->p[j])) > P->K) {
          if (P->K == 4)
            break;
          P->K = k;
        } else if (k < P->K && k == 4)
          break;
      } else
        twos++;
    } else  {
      Realloc(P->polyform, ++len, char);
      strcat(P->polyform, "|");
    }
  }
  /*
   * Find the symmetry group P->K (where 2, 3, 4, 5 represent the dihedral,
   * tetrahedral, octahedral and icosahedral groups, respectively), and its
   * order P->g.
   */
  if (twos >= 2) {/* dihedral */
    P->g = 4 * P->K;
    P->K = 2;
  } else {
    if (P->K > 5)
      Err("numerator too large");
    P->g = 24 * P->K / (6 - P->K);
  }
  /*
   * Compute the nominal density P->D and Euler characteristic P->chi.
   * In few exceptional cases, these values will be modified later.
   */
  if (P->index != last_uniform - 1) {
    int i;
    P->D = P->chi = - P->g;
    for (j = 0; j < 4; j++) if (P->p[j]) {
      P->chi += i = P->g / numerator(P->p[j]);
      P->D += i * denominator(P->p[j]);
    }
    P->chi /= 2;
    P->D /= 4;
    if (P->D <= 0)
      Err("nonpositive density");
  }
  return 1;
}

/*
 * Decompose Schwarz triangle into N right triangles and compute the vertex
 * count V and the vertex valency M.  V is computed from the number g of
 * Schwarz triangles in the cover, divided by the number of triangles which
 * share a vertex. It is halved for one-sided polyhedra, because the
 * kaleidoscopic construction really produces a double orientable covering of
 * such polyhedra. All q' q|r are of the "hemi" type, i.e. have equatorial {2r}
 * faces, and therefore are (except 3/2 3|3 and the dihedra 2 2|r) one-sided. A
 * well known example is 3/2 3|4, the "one-sided heptahedron". Also, all p q r|
 * with one even denominator have a crossed parallelogram as a vertex figure,
 * and thus are one-sided as well.
 */
static int
decompose(Polyhedron *P)
{
  int j, J, *s, *t;
  if (!P->p[1]) { /* p|q r */
    P->N = 2;
    P->M = 2 * numerator(P->p[0]);
    P->V = P->g / P->M;
    Malloc(P->n, P->N, double);
    Malloc(P->m, P->N, double);
    Malloc(P->rot, P->M, int);
    s = P->rot;
    for (j = 0; j < 2; j++) {
      P->n[j] = P->p[j+2];
      P->m[j] = P->p[0];
    }
    for (j = P->M / 2; j--;) {
      *s++ = 0;
      *s++ = 1;
    }
  } else if (!P->p[2]) { /* p q|r */
    P->N = 3;
    P->M = 4;
    P->V = P->g / 2;
    Malloc(P->n, P->N, double);
    Malloc(P->m, P->N, double);
    Malloc(P->rot, P->M, int);
    s = P->rot;
    P->n[0] = 2 * P->p[3];
    P->m[0] = 2;
    for (j = 1; j < 3; j++) {
      P->n[j] = P->p[j-1];
      P->m[j] = 1;
      *s++ = 0;
      *s++ = j;
    }
    if (fabs(P->p[0] - compl (P->p[1])) < DBL_EPSILON) {/* p = q' */
      /* P->p[0]==compl(P->p[1]) should work.  However, MSDOS
       * yeilds a 7e-17 difference! Reported by Jim Buddenhagen
       * <jb1556@daditz.sbc.com> */
      P->hemi = 1;
      P->D = 0;
      if (P->p[0] != 2 && !(P->p[3] == 3 && (P->p[0] == 3 ||
                                             P->p[1] == 3))) {
        P->onesided = 1;
        P->V /= 2;
        P->chi /= 2;
      }
    }
  } else if (!P->p[3]) { /* p q r| */
    P->M = P->N = 3;
    P->V = P->g;
    Malloc(P->n, P->N, double);
    Malloc(P->m, P->N, double);
    Malloc(P->rot, P->M, int);
    s = P->rot;
    for (j = 0; j < 3; j++) {
      if (!(denominator(P->p[j]) % 2)) {
        /* what happens if there is more then one even denominator? */
        if (P->p[(j+1)%3] != P->p[(j+2)%3]) { /* needs postprocessing */
          P->even = j;/* memorize the removed face */
          P->chi -= P->g / numerator(P->p[j]) / 2;
          P->onesided = 1;
          P->D = 0;
        } else {/* for p = q we get a double 2 2r|p */
		/* noted by Roman Maeder <maeder@inf.ethz.ch> for 4 4 3/2| */
		/* Euler characteristic is still wrong */
          P->D /= 2;
        }
        P->V /= 2;
      }
      P->n[j] = 2 * P->p[j];
      P->m[j] = 1;
      *s++ = j;
    }
  } else { /* |p q r - snub polyhedron */
    P->N = 4;
    P->M = 6;
    P->V = P->g / 2;/* Only "white" triangles carry a vertex */
    Malloc(P->n, P->N, double);
    Malloc(P->m, P->N, double);
    Malloc(P->rot, P->M, int);
    Malloc(P->snub, P->M, int);
    s = P->rot;
    t = P->snub;
    P->m[0] = P->n[0] = 3;
    for (j = 1; j < 4; j++) {
      P->n[j] = P->p[j];
      P->m[j] = 1;
      *s++ = 0;
      *s++ = j;
      *t++ = 1;
      *t++ = 0;
    }
  }
  /*
   * Sort the fundamental triangles (using bubble sort) according to decreasing
   * n[i], while pushing the trivial triangles (n[i] = 2) to the end.
   */
  J = P->N - 1;
  while (J) {
    int last;
    last = J;
    J = 0;
    for (j = 0; j < last; j++) {
      if ((P->n[j] < P->n[j+1] || P->n[j] == 2) && P->n[j+1] != 2) {
        int i;
        double temp;
        temp = P->n[j];
        P->n[j] = P->n[j+1];
        P->n[j+1] = temp;
        temp = P->m[j];
        P->m[j] = P->m[j+1];
        P->m[j+1] = temp;
        for (i = 0; i < P->M; i++) {
          if (P->rot[i] == j)
            P->rot[i] = j+1;
          else if (P->rot[i] == j+1)
            P->rot[i] = j;
        }
        if (P->even != -1) {
          if (P->even == j)
            P->even = j+1;
          else if (P->even == j+1)
            P->even = j;
        }
        J = j;
      }
    }
  }
  /*
   *  Get rid of repeated triangles.
   */
  for (J = 0; J < P->N && P->n[J] != 2;J++) {
    int k, i;
    for (j = J+1; j < P->N && P->n[j]==P->n[J]; j++)
      P->m[J] += P->m[j];
    k = j - J - 1;
    if (k) {
      for (i = j; i < P->N; i++) {
        P->n[i - k] = P->n[i];
        P->m[i - k] = P->m[i];
      }
      P->N -= k;
      for (i = 0; i < P->M; i++) {
        if (P->rot[i] >= j)
          P->rot[i] -= k;
        else if (P->rot[i] > J)
          P->rot[i] = J;
      }
      if (P->even >= j)
        P->even -= k;
    }
  }
  /*
   * Get rid of trivial triangles.
   */
  if (!J)
    J = 1; /* hosohedron */
  if (J < P->N) {
    int i;
    P->N = J;
    for (i = 0; i < P->M; i++) {
      if (P->rot[i] >= P->N) {
        for (j = i + 1; j < P->M; j++) {
          P->rot[j-1] = P->rot[j];
          if (P->snub)
            P->snub[j-1] = P->snub[j];
        }
        P->M--;
      }
    }
  }
  /*
   * Truncate arrays
   */
  Realloc(P->n, P->N, double);
  Realloc(P->m, P->N, double);
  Realloc(P->rot, P->M, int);
  if (P->snub)
    Realloc(P->snub, P->M, int);
  return 1;
}


static int dihedral(Polyhedron *P, char *name, char *dual_name);


/*
 * Get the polyhedron name, using standard list or guesswork. Ideally, we
 * should try to locate the Wythoff symbol in the standard list (unless, of
 * course, it is dihedral), after doing few normalizations, such as sorting
 * angles and splitting isoceles triangles.
 */
static int
guessname(Polyhedron *P)
{
  if (P->index != -1) {/* tabulated */
    P->name = uniform[P->index].name;
    P->dual_name = uniform[P->index].dual;
    P->group = uniform[P->index].group;
    P->class = uniform[P->index].class;
    P->dual_class = uniform[P->index].dual_class;
    return 1;
  } else if (P->K == 2) {/* dihedral nontabulated */
    if (!P->p[0]) {
      if (P->N == 1) {
        Malloc(P->name, sizeof ("Octahedron"), char);
        Malloc(P->dual_name, sizeof ("Cube"), char);
        strcpy(P->name, "Octahedron");
        strcpy(P->dual_name, "Cube");
        return 1;
      }
      P->gon = P->n[0] == 3 ? P->n[1] : P->n[0];
      if (P->gon >= 2)
        return dihedral(P, "Antiprism", "Deltohedron");
      else
        return dihedral(P, "Crossed Antiprism", "Concave Deltohedron");
    } else if (!P->p[3] ||
               (!P->p[2] &&
                P->p[3] == 2)) {
      if (P->N == 1) {
        Malloc(P->name, sizeof("Cube"), char);
        Malloc(P->dual_name, sizeof("Octahedron"), char);
        strcpy(P->name, "Cube");
        strcpy(P->dual_name, "Octahedron");
        return 1;
      }
      P->gon = P->n[0] == 4 ? P->n[1] : P->n[0];
      return dihedral(P, "Prism", "Dipyramid");
    } else if (!P->p[1] && P->p[0] != 2) {
      P->gon = P->m[0];
      return dihedral(P, "Hosohedron", "Dihedron");
    } else {
      P->gon = P->n[0];
      return dihedral(P, "Dihedron", "Hosohedron");
    }
  } else {/* other nontabulated */
    static const char *pre[] = {"Tetr", "Oct", "Icos"};
    Malloc(P->name, 50, char);
    Malloc(P->dual_name, 50, char);
    sprintf(P->name, "%sahedral ", pre[P->K - 3]);
    if (P->onesided)
      strcat (P->name, "One-Sided ");
    else if (P->D == 1)
      strcat(P->name, "Convex ");
    else
      strcat(P->name, "Nonconvex ");
    strcpy(P->dual_name, P->name);
    strcat(P->name, "Isogonal Polyhedron");
    strcat(P->dual_name, "Isohedral Polyhedron");
    Realloc(P->name, strlen (P->name) + 1, char);
    Realloc(P->dual_name, strlen (P->dual_name) + 1, char);
    return 1;
  }
}

static int
dihedral(Polyhedron *P, char *name, char *dual_name)
{
  char *s;
  int i;
  Sprintfrac(s, P->gon < 2 ? compl (P->gon) : P->gon);
  i = strlen(s) + sizeof ("-gonal ");
  Malloc(P->name, i + strlen (name), char);
  Malloc(P->dual_name, i + strlen (dual_name), char);
  sprintf(P->name, "%s-gonal %s", s, name);
  sprintf(P->dual_name, "%s-gonal %s", s, dual_name);
  free(s);
  return 1;
}

/*
 * Solve the fundamental right spherical triangles.
 * If need_approx is set, print iterations on standard error.
 */
static int
newton(Polyhedron *P, int need_approx)
{
  /*
   * First, we find initial approximations.
   */
  int j;
  double cosa;
  Malloc(P->gamma, P->N, double);
  if (P->N == 1) {
    P->gamma[0] = M_PI / P->m[0];
    return 1;
  }
  for (j = 0; j < P->N; j++)
    P->gamma[j] = M_PI / 2 - M_PI / P->n[j];
  errno = 0; /* may be non-zero from some reason */
  /*
   * Next, iteratively find closer approximations for gamma[0] and compute
   * other gamma[j]'s from Napier's equations.
   */
  if (need_approx)
    fprintf(stderr, "Solving %s\n", P->polyform);
  for (;;) {
    double delta = M_PI, sigma = 0;
    for (j = 0; j < P->N; j++) {
      if (need_approx)
        fprintf(stderr, "%-20.15f", P->gamma[j]);
      delta -= P->m[j] * P->gamma[j];
    }
    if (need_approx)
      printf("(%g)\n", delta);
    if (fabs(delta) < 11 * DBL_EPSILON)
      return 1;
    /* On a RS/6000, fabs(delta)/DBL_EPSILON may occilate between 8 and
     * 10. Reported by David W. Sanderson <dws@ssec.wisc.edu> */
    for (j = 0; j < P->N; j++)
      sigma += P->m[j] * tan(P->gamma[j]);
    P->gamma[0] += delta * tan(P->gamma[0]) / sigma;
    if (P->gamma[0] < 0 || P->gamma[0] > M_PI)
      Err("gamma out of bounds");
    cosa = cos(M_PI / P->n[0]) / sin(P->gamma[0]);
    for (j = 1; j < P->N; j++)
      P->gamma[j] = asin(cos(M_PI / P->n[j]) / cosa);
    if (errno)
      Err(strerror(errno));
  }
}

/*
 * Postprocess pqr| where r has an even denominator (cf. Coxeter &al. Sec.9).
 * Remove the {2r} and add a retrograde {2p} and retrograde {2q}.
 */
static int
exceptions(Polyhedron *P)
{
  int j;
  if (P->even != -1) {
    P->M = P->N = 4;
    Realloc(P->n, P->N, double);
    Realloc(P->m, P->N, double);
    Realloc(P->gamma, P->N, double);
    Realloc(P->rot, P->M, int);
    for (j = P->even + 1; j < 3; j++) {
      P->n[j-1] = P->n[j];
      P->gamma[j-1] = P->gamma[j];
    }
    P->n[2] = compl(P->n[1]);
    P->gamma[2] = - P->gamma[1];
    P->n[3] = compl(P->n[0]);
    P->m[3] = 1;
    P->gamma[3] = - P->gamma[0];
    P->rot[0] = 0;
    P->rot[1] = 1;
    P->rot[2] = 3;
    P->rot[3] = 2;
  }

  /*
   * Postprocess the last polyhedron |3/2 5/3 3 5/2 by taking a |5/3 3 5/2,
   * replacing the three snub triangles by four equatorial squares and adding
   * the missing {3/2} (retrograde triangle, cf. Coxeter &al. Sec. 11).
   */
  if (P->index == last_uniform - 1) {
    P->N = 5;
    P->M = 8;
    Realloc(P->n, P->N, double);
    Realloc(P->m, P->N, double);
    Realloc(P->gamma, P->N, double);
    Realloc(P->rot, P->M, int);
    Realloc(P->snub, P->M, int);
    P->hemi = 1;
    P->D = 0;
    for (j = 3; j; j--) {
      P->m[j] = 1;
      P->n[j] = P->n[j-1];
      P->gamma[j] = P->gamma[j-1];
    }
    P->m[0] = P->n[0] = 4;
    P->gamma[0] = M_PI / 2;
    P->m[4] = 1;
    P->n[4] = compl(P->n[1]);
    P->gamma[4] = - P->gamma[1];
    for (j = 1; j < 6; j += 2) P->rot[j]++;
    P->rot[6] = 0;
    P->rot[7] = 4;
    P->snub[6] = 1;
    P->snub[7] = 0;
  }
  return 1;
}

/*
 * Compute edge and face counts, and update D and chi.  Update D in the few
 * cases the density of the polyhedron is meaningful but different than the
 * density of the corresponding Schwarz triangle (cf. Coxeter &al., p. 418 and
 * p. 425).
 * In these cases, spherical faces of one type are concave (bigger than a
 * hemisphere), and the actual density is the number of these faces less the
 * computed density.  Note that if j != 0, the assignment gamma[j] = asin(...)
 * implies gamma[j] cannot be obtuse.  Also, compute chi for the only
 * non-Wythoffian polyhedron.
 */
static int
count(Polyhedron *P)
{
  int j, temp;
  Malloc(P->Fi, P->N, int);
  for (j = 0; j < P->N; j++) {
    P->E += temp = P->V * numerator(P->m[j]);
    P->F += P->Fi[j] = temp / numerator(P->n[j]);
  }
  P->E /= 2;
  if (P->D && P->gamma[0] > M_PI / 2)
    P->D = P->Fi[0] - P->D;
  if (P->index == last_uniform - 1)
    P->chi = P->V - P->E + P->F;
  return 1;
}

/*
 * Generate a printable vertex configuration symbol.
 */
static int
configuration(Polyhedron *P)
{
  int j, len = 2;
  for (j = 0; j < P->M; j++) {
    char *s;
    Sprintfrac(s, P->n[P->rot[j]]);
    len += strlen (s) + 2;
    if (!j) {
      Malloc(P->config, len, char);
/*      strcpy(P->config, "(");*/
      strcpy(P->config, "");
    } else {
      Realloc(P->config, len, char);
      strcat(P->config, ", ");
    }
    strcat(P->config, s);
    free(s);
  }
/*  strcat (P->config, ")");*/
  if ((j = denominator (P->m[0])) != 1) {
    char s[MAXDIGITS + 2];
    sprintf(s, "/%d", j);
    Realloc(P->config, len + strlen (s), char);
    strcat(P->config, s);
  }
  return 1;
}

/*
 * Compute polyhedron vertices and vertex adjecency lists.
 * The vertices adjacent to v[i] are v[adj[0][i], v[adj[1][i], ...
 * v[adj[M-1][i], ordered counterclockwise.  The algorith is a BFS on the
 * vertices, in such a way that the vetices adjacent to a givem vertex are
 * obtained from its BFS parent by a cyclic sequence of rotations. firstrot[i]
 * points to the first  rotaion in the sequence when applied to v[i]. Note that
 * for non-snub polyhedra, the rotations at a child are opposite in sense when
 * compared to the rotations at the parent. Thus, we fill adj[*][i] from the
 * end to signify clockwise rotations. The firstrot[] array is not needed for
 * display thus it is freed after being used for face computations below.
 */
static int
vertices(Polyhedron *P)
{
  int i, newV = 2;
  double cosa;
  Malloc(P->v, P->V, Vector);
  Matalloc(P->adj, P->M, P->V, int);
  Malloc(P->firstrot, P->V, int); /* temporary , put in Polyhedron
                                     structure so that may be freed on
                                     error */
  cosa = cos(M_PI / P->n[0]) / sin(P->gamma[0]);
  P->v[0].x = 0;
  P->v[0].y = 0;
  P->v[0].z = 1;
  P->firstrot[0] = 0;
  P->adj[0][0] = 1;
  P->v[1].x = 2 * cosa * sqrt(1 - cosa * cosa);
  P->v[1].y = 0;
  P->v[1].z = 2 * cosa * cosa - 1;
  if (!P->snub) {
    P->firstrot[1] = 0;
    P->adj[0][1] = -1;/* start the other side */
    P->adj[P->M-1][1] = 0;
  } else {
    P->firstrot[1] = P->snub[P->M-1] ? 0 : P->M-1 ;
    P->adj[0][1] = 0;
  }
  for (i = 0; i < newV; i++) {
    int j, k;
    int last, one, start, limit;
    if (P->adj[0][i] == -1) {
      one = -1; start = P->M-2; limit = -1;
    } else {
      one = 1; start = 1; limit = P->M;
    }
    k = P->firstrot[i];
    for (j = start; j != limit; j += one) {
      Vector temp;
      int J;
      temp = rotate (P->v[P->adj[j-one][i]], P->v[i],
                     one * 2 * P->gamma[P->rot[k]]);
      for (J=0; J<newV && !same(P->v[J],temp,BIG_EPSILON); J++)
        ;/* noop */
      P->adj[j][i] = J;
      last = k;
      if (++k == P->M)
        k = 0;
      if (J == newV) { /* new vertex */
        if (newV == P->V) Err ("too many vertices");
        P->v[newV++] = temp;
        if (!P->snub) {
          P->firstrot[J] = k;
          if (one > 0) {
            P->adj[0][J] = -1;
            P->adj[P->M-1][J] = i;
          } else {
            P->adj[0][J] = i;
          }
        } else {
          P->firstrot[J] = !P->snub[last] ? last :
            !P->snub[k] ? (k+1)%P->M : k ;
          P->adj[0][J] = i;
        }
      }
    }
  }
  return 1;
}

/*
 * Compute polyhedron faces (dual vertices) and incidence matrices.
 * For orientable polyhedra, we can distinguish between the two faces meeting
 * at a given directed edge and identify the face on the left and the face on
 * the right, as seen from the outside.  For one-sided polyhedra, the vertex
 * figure is a papillon (in Coxeter &al.  terminology, a crossed parallelogram)
 * and the two faces meeting at an edge can be identified as the side face
 * (n[1] or n[2]) and the diagonal face (n[0] or n[3]).
 */
static int
faces(Polyhedron *P)
{
  int i, newF = 0;
  Malloc (P->f, P->F, Vector);
  Malloc (P->ftype, P->F, int);
  Matalloc (P->incid, P->M, P->V, int);
  P->minr = 1 / fabs (tan (M_PI / P->n[P->hemi]) * tan (P->gamma[P->hemi]));
  for (i = P->M; --i>=0;) {
    int j;
    for (j = P->V; --j>=0;)
      P->incid[i][j] = -1;
  }
  for (i = 0; i < P->V; i++) {
    int j;
    for (j = 0; j < P->M; j++) {
      int i0, J;
      int pap=0;/* papillon edge type */
      if (P->incid[j][i] != -1)
        continue;
      P->incid[j][i] = newF;
      if (newF == P->F)
        Err("too many faces");
      P->f[newF] = pole(P->minr, P->v[i], P->v[P->adj[j][i]],
                        P->v[P->adj[mod(j + 1, P->M)][i]]);
      P->ftype[newF] = P->rot[mod(P->firstrot[i] + ((P->adj[0][i] <
                                                     P->adj[P->M - 1][i])
                                                    ?  j
                                                    : -j - 2),
                                  P->M)];
      if (P->onesided)
        pap = (P->firstrot[i] + j) % 2;
      i0 = i;
      J = j;
      for (;;) {
        int k;
        k = i0;
        if ((i0 = P->adj[J][k]) == i) break;
        for (J = 0; J < P->M && P->adj[J][i0] != k; J++)
          ;/* noop */
        if (J == P->M)
          Err("too many faces");
        if (P->onesided && (J + P->firstrot[i0]) % 2 == pap) {
          P->incid [J][i0] = newF;
          if (++J >= P->M)
            J = 0;
        } else {
          if (--J < 0)
            J = P->M - 1;
          P->incid [J][i0] = newF;
        }
      }
      newF++;
    }
  }
  Free(P->firstrot);
  Free(P->rot);
  Free(P->snub);
  return 1;
}

/*
 * Compute edge list and graph polyhedron and dual.
 * If the polyhedron is of the "hemi" type, each edge has one finite vertex and
 * one ideal vertex. We make sure the latter is always the out-vertex, so that
 * the edge becomes a ray (half-line).  Each ideal vertex is represented by a
 * unit Vector, and the direction of the ray is either parallel or
 * anti-parallel this Vector. We flag this in the array P->anti[E].
 */
static int
edgelist(Polyhedron *P)
{
  int i, j, *s, *t, *u;
  Matalloc(P->e, 2, P->E, int);
  Matalloc(P->dual_e, 2, P->E, int);
  s = P->e[0];
  t = P->e[1];
  for (i = 0; i < P->V; i++)
    for (j = 0; j < P->M; j++)
      if (i < P->adj[j][i]) {
        *s++ = i;
        *t++ = P->adj[j][i];
      }
  s = P->dual_e[0];
  t = P->dual_e[1];
  if (!P->hemi)
    P->anti = 0;
  else
    Malloc(P->anti, P->E, int);
  u = P->anti;
  for (i = 0; i < P->V; i++)
    for (j = 0; j < P->M; j++)
      if (i < P->adj[j][i])
        {
          if (!u) {
            *s++ = P->incid[mod(j-1,P->M)][i];
            *t++ = P->incid[j][i];
          } else {
            if (P->ftype[P->incid[j][i]]) {
              *s = P->incid[j][i];
              *t = P->incid[mod(j-1,P->M)][i];
            } else {
              *s = P->incid[mod(j-1,P->M)][i];
              *t = P->incid[j][i];
            }
            *u++ = dot(P->f[*s++], P->f[*t++]) > 0;
          }
        }
  return 1;
}


static char *
sprintfrac(double x)
{
  char *s;
  frac (x);
  if (!frax.d) {
    Malloc(s, sizeof ("infinity"), char);
    strcpy(s, "infinity");
  } else if (frax.d == 1) {
    char n[MAXDIGITS + 1];
    sprintf(n, "%ld", frax.n);
    Malloc(s, strlen (n) +  1, char);
    strcpy(s, n);
  } else {
    char n[MAXDIGITS + 1], d[MAXDIGITS + 1];
    sprintf(n, "%ld", frax.n);
    sprintf(d, "%ld", frax.d);
    Malloc(s, strlen (n) + strlen (d) + 2, char);
    sprintf(s, "%s/%s", n, d);
  }
  return s;
}

static double
dot(Vector a, Vector b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vector
scale(double k, Vector a)
{
  a.x *= k;
  a.y *= k;
  a.z *= k;
  return a;
}

static Vector
diff(Vector a, Vector b)
{
  a.x -= b.x;
  a.y -= b.y;
  a.z -= b.z;
  return a;
}

static Vector
cross(Vector a, Vector b)
{
  Vector p;
  p.x = a.y * b.z - a.z * b.y;
  p.y = a.z * b.x - a.x * b.z;
  p.z = a.x * b.y - a.y * b.x;
  return p;
}

static Vector
sum(Vector a, Vector b)
{
  a.x += b.x;
  a.y += b.y;
  a.z += b.z;
  return a;
}

static Vector
sum3(Vector a, Vector b, Vector c)
{
  a.x += b.x + c.x;
  a.y += b.y + c.y;
  a.z += b.z + c.z;
  return a;
}

static Vector
rotate(Vector vertex, Vector axis, double angle)
{
  Vector p;
  p = scale(dot (axis, vertex), axis);
  return sum3(p, scale(cos(angle), diff(vertex, p)),
              scale(sin(angle), cross(axis, vertex)));
}

static Vector x, y, z;

/*
 * rotate the standard frame
 */
static void
rotframe(double azimuth, double elevation, double angle)
{
  static const Vector X = {1,0,0}, Y = {0,1,0}, Z = {0,0,1};
  Vector axis;

  axis = rotate(rotate (X, Y, elevation), Z, azimuth);
  x = rotate(X, axis, angle);
  y = rotate(Y, axis, angle);
  z = rotate(Z, axis, angle);
}

/*
 * rotate an array of n Vectors
 */
static void
rotarray(Vector *new, Vector *old, int n)
{
  while (n--) {
    *new++ = sum3(scale(old->x, x), scale(old->y, y), scale(old->z, z));
    old++;
  }
}

static int
same(Vector a, Vector b, double epsilon)
{
  return fabs(a.x - b.x) < epsilon && fabs(a.y - b.y) < epsilon
    && fabs(a.z - b.z) < epsilon;
}

/*
 * Compute the polar reciprocal of the plane containing a, b and c:
 *
 * If this plane does not contain the origin, return p such that
 *	dot(p,a) = dot(p,b) = dot(p,b) = r.
 *
 * Otherwise, return p such that
 *	dot(p,a) = dot(p,b) = dot(p,c) = 0
 * and
 *	dot(p,p) = 1.
 */
static Vector
pole(double r, Vector a, Vector b, Vector c)
{
  Vector p;
  double k;
  p = cross(diff(b, a), diff(c, a));
  k = dot(p, a);
  if (fabs(k) < 1e-6)
    return scale(1 / sqrt(dot(p, p)), p);
  else
    return scale(r/ k , p);
}


/* output */




static void rotframe(double azimuth, double elevation, double angle);
static void rotarray(Vector *new, Vector *old, int n);
static int mod (int i, int j);


static void
push_point (polyhedron *p, Vector v)
{
  p->points[p->npoints].x = v.x;
  p->points[p->npoints].y = v.y;
  p->points[p->npoints].z = v.z;
  p->npoints++;
}

static void
push_face3 (polyhedron *p, int x, int y, int z)
{
  p->faces[p->nfaces].npoints = 3;
  Malloc (p->faces[p->nfaces].points, 3, int);
  p->faces[p->nfaces].points[0] = x;
  p->faces[p->nfaces].points[1] = y;
  p->faces[p->nfaces].points[2] = z;
  p->nfaces++;
}

static void
push_face4 (polyhedron *p, int x, int y, int z, int w)
{
  p->faces[p->nfaces].npoints = 4;
  Malloc (p->faces[p->nfaces].points, 4, int);
  p->faces[p->nfaces].points[0] = x;
  p->faces[p->nfaces].points[1] = y;
  p->faces[p->nfaces].points[2] = z;
  p->faces[p->nfaces].points[3] = w;
  p->nfaces++;
}




static polyhedron *
construct_polyhedron (Polyhedron *P, Vector *v, int V, Vector *f, int F,
                      char *name, char *dual, char *class, char *star,
                      double azimuth, double elevation, double freeze)
{
  int i, j, k=0, l, ll, ii, *hit=0, facelets;

  polyhedron *result;
  Vector *temp;

  Malloc (result, 1, polyhedron);
  memset (result, 0, sizeof(*result));

  /*
   * Rotate polyhedron
   */
  rotframe(azimuth, elevation, freeze);
  Malloc(temp, V, Vector);
  rotarray(temp, v, V);
  v = temp;
  Malloc(temp, F, Vector);
  rotarray(temp, f, F);
  f = temp;

  result->number = P->index + 1;
  result->name = strdup (name);
  result->dual = strdup (dual);
  result->wythoff = strdup (P->polyform);
  result->config = strdup (P->config);
  result->group = strdup (P->group);
  result->class = strdup (class);

  /*
   * Vertex list
   */
  Malloc (result->points, V + F * 13, point);
  result->npoints = 0;

  result->nedges = P->E;
  result->logical_faces = F;
  result->logical_vertices = V;
  result->density = P->D;
  result->chi = P->chi;

  for (i = 0; i < V; i++)
    push_point (result, v[i]);

  /*
   * Auxiliary vertices (needed because current VRML browsers cannot handle
   * non-simple polygons, i.e., ploygons with self intersections): Each
   * non-simple face is assigned an auxiliary vertex. By connecting it to the
   * rest of the vertices the face is triangulated. The circum-center is used
   * for the regular star faces of uniform polyhedra. The in-center is used for
   * the pentagram (#79) and hexagram (#77) of the high-density snub duals, and
   * for the pentagrams (#40, #58) and hexagram (#52) of the stellated duals
   * with configuration (....)/2. Finally, the self-intersection of the crossed
   * parallelogram is used for duals with form p q r| with an even denominator.
   *
   * This method do not work for the hemi-duals, whose faces are not
   * star-shaped and have two self-intersections each.
   *
   * Thus, for each face we need six auxiliary vertices: The self intersections
   * and the terminal points of the truncations of the infinite edges. The
   * ideal vertices are listed, but are not used by the face-list.
   *
   * Note that the face of the last dual (#80) is octagonal, and constists of
   * two quadrilaterals of the infinite type.
   */

  if (*star && P->even != -1)
    Malloc(hit, F, int);
  for (i = 0; i < F; i++)
    if ((!*star &&
         (frac(P->n[P->ftype[i]]), frax.d != 1 && frax.d != frax.n - 1)) ||
        (*star &&
         P->K == 5 &&
         (P->D > 30 ||
          denominator (P->m[0]) != 1))) {
      /* find the center of the face */
      double h;
      if (!*star && P->hemi && !P->ftype[i])
        h = 0;
      else
        h = P->minr / dot(f[i],f[i]);
      push_point(result, scale (h, f[i]));

    } else if (*star && P->even != -1) {
      /* find the self-intersection of a crossed parallelogram.
       * hit is set if v0v1 intersects v2v3*/
      Vector v0, v1, v2, v3, c0, c1, p;
      double d0, d1;
      v0 = v[P->incid[0][i]];
      v1 = v[P->incid[1][i]];
      v2 = v[P->incid[2][i]];
      v3 = v[P->incid[3][i]];
      d0 = sqrt(dot(diff(v0, v2), diff(v0, v2)));
      d1 = sqrt(dot (diff(v1, v3), diff(v1, v3)));
      c0 = scale(d1, sum(v0, v2));
      c1 = scale(d0, sum(v1, v3));
      p = scale(0.5 / (d0 + d1), sum(c0, c1));
      push_point (result, p);
      p = cross(diff(p, v2), diff(p, v3));
      hit[i] = (dot(p, p) < 1e-6);
    } else if (*star && P->hemi && P->index != last_uniform - 1) {
      /* find the terminal points of the truncation and the
       * self-intersections.
       *  v23       v0       v21
       *  |  \     /  \     /  |
       *  |   v0123    v0321   |
       *  |  /     \  /     \  |
       *  v01       v2       v03
       */
      Vector v0, v1, v2, v3, v01, v03, v21, v23, v0123, v0321 ;
      Vector u;
      double t = 1.5;/* truncation adjustment factor */
      j = !P->ftype[P->incid[0][i]];
      v0 = v[P->incid[j][i]];/* real vertex */
      v1 = v[P->incid[j+1][i]];/* ideal vertex (unit vector) */
      v2 = v[P->incid[j+2][i]];/* real */
      v3 = v[P->incid[(j+3)%4][i]];/* ideal */
      /* compute intersections
       * this uses the following linear algebra:
       * v0123 = v0 + a v1 = v2 + b v3
       * v0 x v3 + a (v1 x v3) = v2 x v3
       * a (v1 x v3) = (v2 - v0) x v3
       * a (v1 x v3) . (v1 x v3) = (v2 - v0) x v3 . (v1 x v3)
       */
      u = cross(v1, v3);
      v0123 = sum(v0, scale(dot(cross(diff(v2, v0), v3), u) / dot(u,u),
                            v1));
      v0321 = sum(v0, scale(dot(cross(diff(v0, v2), v1), u) / dot(u,u),
                            v3));
      /* compute truncations */
      v01 = sum(v0 , scale(t, diff(v0123, v0)));
      v23 = sum(v2 , scale(t, diff(v0123, v2)));
      v03 = sum(v0 , scale(t, diff(v0321, v0)));
      v21 = sum(v2 , scale(t, diff(v0321, v2)));

      push_point(result, v01);
      push_point(result, v23);
      push_point(result, v0123);
      push_point(result, v03);
      push_point(result, v21);
      push_point(result, v0321);

    } else if (*star && P->index == last_uniform - 1) {
      /* find the terminal points of the truncation and the
       * self-intersections.
       *  v23       v0       v21
       *  |  \     /  \     /  |
       *  |   v0123    v0721   |
       *  |  /     \  /     \  |
       *  v01       v2       v07
       *
       *  v65       v4       v67
       *  |  \     /  \     /  |
       *  |   v4365    v4567   |
       *  |  /     \  /     \  |
       *  v43       v6       v45
       */
      Vector v0, v1, v2, v3, v4, v5, v6, v7, v01, v07, v21, v23;
      Vector v43, v45, v65, v67, v0123, v0721, v4365, v4567;
      double t = 1.5;/* truncation adjustment factor */
      Vector u;
      for (j = 0; j < 8; j++)
        if (P->ftype[P->incid[j][i]] == 3)
          break;
      v0 = v[P->incid[j][i]];/* real {5/3} */
      v1 = v[P->incid[(j+1)%8][i]];/* ideal */
      v2 = v[P->incid[(j+2)%8][i]];/* real {3} */
      v3 = v[P->incid[(j+3)%8][i]];/* ideal */
      v4 = v[P->incid[(j+4)%8][i]];/* real {5/2} */
      v5 = v[P->incid[(j+5)%8][i]];/* ideal */
      v6 = v[P->incid[(j+6)%8][i]];/* real {3/2} */
      v7 = v[P->incid[(j+7)%8][i]];/* ideal */
      /* compute intersections */
      u = cross(v1, v3);
      v0123 = sum(v0, scale(dot(cross(diff(v2, v0), v3), u) / dot(u,u),
                            v1));
      u = cross(v7, v1);
      v0721 = sum(v0, scale(dot(cross(diff(v2, v0), v1), u) / dot(u,u),
                            v7));
      u = cross(v5, v7);
      v4567 = sum(v4, scale(dot(cross(diff(v6, v4), v7), u) / dot(u,u),
                            v5));
      u = cross(v3, v5);
      v4365 = sum(v4, scale(dot(cross(diff(v6, v4), v5), u) / dot(u,u),
                            v3));
      /* compute truncations */
      v01 = sum(v0 , scale(t, diff(v0123, v0)));
      v23 = sum(v2 , scale(t, diff(v0123, v2)));
      v07 = sum(v0 , scale(t, diff(v0721, v0)));
      v21 = sum(v2 , scale(t, diff(v0721, v2)));
      v45 = sum(v4 , scale(t, diff(v4567, v4)));
      v67 = sum(v6 , scale(t, diff(v4567, v6)));
      v43 = sum(v4 , scale(t, diff(v4365, v4)));
      v65 = sum(v6 , scale(t, diff(v4365, v6)));

      push_point(result, v01);
      push_point(result, v23);
      push_point(result, v0123);
      push_point(result, v07);
      push_point(result, v21);
      push_point(result, v0721);
      push_point(result, v45);
      push_point(result, v67);
      push_point(result, v4567);
      push_point(result, v43);
      push_point(result, v65);
      push_point(result, v4365);
    }

  /*
   * Face list:
   * Each face is printed in a separate line, by listing the indices of its
   * vertices. In the non-simple case, the polygon is represented by the
   * triangulation, each triangle consists of two polyhedron vertices and one
   * auxiliary vertex.
   */
  Malloc (result->faces, F * 10, face);
  result->nfaces = 0;

  ii = V;
  facelets = 0;
  for (i = 0; i < F; i++) {
    if (*star) {
      if (P->K == 5 &&
          (P->D > 30 ||
           denominator (P->m[0]) != 1)) {
        for (j = 0; j < P->M - 1; j++) {
          push_face3 (result, P->incid[j][i], P->incid[j+1][i],  ii);
          facelets++;
        }

        push_face3 (result, P->incid[j][i], P->incid[0][i],  ii++);
        facelets++;

      } else if (P->even != -1) {
        if (hit[i]) {
          push_face3 (result, P->incid[3][i], P->incid[0][i],  ii);
          push_face3 (result, P->incid[1][i], P->incid[2][i],  ii);
        } else {
          push_face3 (result, P->incid[0][i], P->incid[1][i],  ii);
          push_face3 (result, P->incid[2][i], P->incid[3][i],  ii);
        }
        ii++;
        facelets += 2;

      } else if (P->hemi && P->index != last_uniform - 1) {
        j = !P->ftype[P->incid[0][i]];

        push_face3 (result, ii, ii + 1, ii + 2);
        push_face4 (result, P->incid[j][i], ii + 2, P->incid[j+2][i], ii + 5);
        push_face3 (result, ii + 3, ii + 4, ii + 5);
        ii += 6;
        facelets += 3;
      } else if (P->index == last_uniform - 1) {
        for (j = 0; j < 8; j++)
          if (P->ftype[P->incid[j][i]] == 3)
            break;
        push_face3 (result, ii, ii + 1, ii + 2);
        push_face4 (result,
                    P->incid[j][i], ii + 2, P->incid[(j+2)%8][i], ii + 5);
        push_face3 (result, ii + 3, ii + 4, ii + 5);

        push_face3 (result, ii + 6, ii + 7, ii + 8);
        push_face4 (result,
                    P->incid[(j+4)%8][i], ii + 8, P->incid[(j+6)%8][i],
                    ii + 11);
        push_face3 (result, ii + 9, ii + 10, ii + 11);
        ii += 12;
        facelets += 6;
      } else {

        result->faces[result->nfaces].npoints = P->M;
        Malloc (result->faces[result->nfaces].points, P->M, int);
        for (j = 0; j < P->M; j++)
          result->faces[result->nfaces].points[j] = P->incid[j][i];
        result->nfaces++;
        facelets++;
      }
    } else {
      int split = (frac(P->n[P->ftype[i]]),
                   frax.d != 1 && frax.d != frax.n - 1);
      for (j = 0; j < V; j++) {
        for (k = 0; k < P->M; k++)
          if (P->incid[k][j] == i)
            break;
        if (k != P->M)
          break;
      }
      if (split) {
        ll = j;
        for (l = P->adj[k][j]; l != j; l = P->adj[k][l]) {
          for (k = 0; k < P->M; k++)
            if (P->incid[k][l] == i)
              break;
          if (P->adj[k][l] == ll)
            k = mod(k + 1 , P->M);
          push_face3 (result, ll, l, ii);
          facelets++;
          ll = l;
        }
        push_face3 (result, ll, j, ii++);
        facelets++;

      } else {

        int *pp;
        int pi = 0;
        Malloc (pp, 100, int);

        pp[pi++] = j;
        ll = j;
        for (l = P->adj[k][j]; l != j; l = P->adj[k][l]) {
          for (k = 0; k < P->M; k++)
            if (P->incid[k][l] == i)
              break;
          if (P->adj[k][l] == ll)
            k = mod(k + 1 , P->M);
          pp[pi++] = l;
          ll = l;
        }
        result->faces[result->nfaces].npoints = pi;
        result->faces[result->nfaces].points = pp;
        result->nfaces++;
        facelets++;
      }
    }
  }

  /*
   * Face color indices - for polyhedra with multiple face types
   * For non-simple faces, the index is repeated as many times as needed by the
   * triangulation.
   */
  {
    int ff = 0;
    if (!*star && P->N != 1) {
      for (i = 0; i < F; i++)
        if (frac(P->n[P->ftype[i]]), frax.d == 1 || frax.d == frax.n - 1)
          result->faces[ff++].color = P->ftype[i];
        else
          for (j = 0; j < frax.n; j++)
            result->faces[ff++].color = P->ftype[i];
    } else {
      for (i = 0; i < facelets; i++)
        result->faces[ff++].color = 0;
    }
  }

  if (*star && P->even != -1)
    free(hit);
  free(v);
  free(f);

  return result;
}



/* External interface (jwz)
 */

void
free_polyhedron (polyhedron *p)
{
  if (!p) return;
  Free (p->wythoff);
  Free (p->name);
  Free (p->group);
  Free (p->class);
  if (p->faces)
    {
      int i;
      for (i = 0; i < p->nfaces; i++)
        Free (p->faces[i].points);
      Free (p->faces);
    }
  Free (p);
}


int
construct_polyhedra (polyhedron ***polyhedra_ret)
{
  double freeze = 0;
  double azimuth = AZ;
  double elevation = EL;
  int index = 0;

  int count = 0;
  polyhedron **result;
  Malloc (result, last_uniform * 2 + 1, polyhedron*);

  while (index < last_uniform) {
    char sym[4];
    Polyhedron *P;

    sprintf(sym, "#%d", index + 1);
    if (!(P = kaleido(sym, 1, 0, 0, 0))) {
      Err (strerror(errno));
    }

    result[count++] = construct_polyhedron (P, P->v, P->V, P->f, P->F,
                                            P->name, P->dual_name,
                                            P->class, "",
                                            azimuth, elevation, freeze);

    result[count++] = construct_polyhedron (P, P->f, P->F, P->v, P->V,
                                            P->dual_name, P->name,
                                            P->dual_class, "*",
                                            azimuth, elevation, freeze);
    polyfree(P);
    index++;
  }

  *polyhedra_ret = result;
  return count;
}
