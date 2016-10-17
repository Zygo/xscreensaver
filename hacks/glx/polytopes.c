/* polytopes --- Shows one of the six regular polytopes rotating in 4d */

#if 0
static const char sccsid[] = "@(#)polytopes.c  1.2 05/09/28 xlockmore";
#endif

/* Copyright (c) 2003-2009 Carsten Steger <carsten@mirsanmir.org>. */

/*
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
 * REVISION HISTORY:
 * C. Steger - 03/08/10: Initial version
 * C. Steger - 05/09/28: Added trackball support
 * C. Steger - 07/01/23: Improved 4d trackball support
 * C. Steger - 09/08/23: Removed check-config.pl warnings
 */

/*
 * This program shows one of the six regular 4D polytopes rotating in 4D.
 * Written by Carsten Steger, inspired by H.S.M Coxeter's book "Regular
 * Polytopes", 3rd Edition, Dover Publications, Inc., 1973, and Thomas
 * Banchoff's book "Beyond the Third Dimension: Geometry, Computer
 * Graphics, and Higher Dimensions", Scientific American Library, 1990.
 */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SQRT15OVER4 1.93649167310370844259 /* sqrt(15/4) */
#define SQRT10OVER3 1.82574185835055371152 /* sqrt(10/3) */
#define SQRT5OVER2  1.58113883008418966600 /* sqrt(5/2) */
#define SQRT5OVER6  0.91287092917527685576 /* sqrt(5/6) */
#define SQRT5OVER12 0.64549722436790281420 /* sqrt(5/12) */
#define SQRT2       1.41421356237309504880 /* sqrt(2) */
#define GOLDEN      1.61803398874989484820 /* (sqrt(5)+1)/2 */
#define GOLDENINV   0.61803398874989484820 /* (sqrt(5)-1)/2 */
#define SQRT2INV    0.70710678118654752440 /* sqrt(1/2) */
#define GOLDEN2     1.14412280563536859520 /* ((sqrt(5)+1)/2)/sqrt(2) */
#define GOLDENINV2  0.43701602444882107080 /* ((sqrt(5)-1)/2)/sqrt(2) */
#define GOLDEN22    1.85122958682191611960 /* (((sqrt(5)+1)/2)^2)/sqrt(2) */
#define GOLDENINV22 0.27009075673772645360 /* (((sqrt(5)-1)/2)^2)/sqrt(2) */

#define DISP_WIREFRAME            0
#define DISP_SURFACE              1
#define DISP_TRANSPARENT          2

#define POLYTOPE_RANDOM          -1
#define POLYTOPE_5_CELL           0
#define POLYTOPE_8_CELL           1
#define POLYTOPE_16_CELL          2
#define POLYTOPE_24_CELL          3
#define POLYTOPE_120_CELL         4
#define POLYTOPE_600_CELL         5
#define POLYTOPE_LAST_CELL        POLYTOPE_600_CELL

#define COLORS_SINGLE             0
#define COLORS_DEPTH              1

#define DISP_3D_PERSPECTIVE       0
#define DISP_3D_ORTHOGRAPHIC      1

#define DISP_4D_PERSPECTIVE       0
#define DISP_4D_PERSPECTIVE_STR  "0"
#define DISP_4D_ORTHOGRAPHIC      1
#define DISP_4D_ORTHOGRAPHIC_STR "1"

#define DEF_DISPLAY_MODE          "transparent"
#define DEF_POLYTOPE              "random"
#define DEF_COLORS                "depth"
#define DEF_PROJECTION_3D         "perspective"
#define DEF_PROJECTION_4D         "perspective"
#define DEF_SPEEDWX               "1.1"
#define DEF_SPEEDWY               "1.3"
#define DEF_SPEEDWZ               "1.5"
#define DEF_SPEEDXY               "1.7"
#define DEF_SPEEDXZ               "1.9"
#define DEF_SPEEDYZ               "2.1"

#ifdef STANDALONE
# define DEFAULTS           "*delay:      25000 \n" \
                            "*showFPS:    False \n" \
			    "*suppressRotationAnimation: True\n" \

# define refresh_polytopes 0
# include "xlockmore.h"         /* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"             /* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL

#ifndef HAVE_JWXYZ
# include <X11/keysym.h>
#endif
#include "gltrackball.h"


#ifdef USE_MODULES
ModStruct   polytopes_description =
{"polytopes", "init_polytopes", "draw_polytopes", "release_polytopes",
 "draw_polytopes", "change_polytopes", NULL, &polytopes_opts,
 25000, 1, 1, 1, 1.0, 4, "",
 "Shows one of the six regular 4d polytopes rotating in 4d", 0, NULL};

#endif


static char *mode;
static int display_mode;
static char *poly_str;
static int polytope;
static char *color_str;
static int color_mode;
static char *proj_3d;
static int projection_3d;
static char *proj_4d;
static int projection_4d;
static float speed_wx;
static float speed_wy;
static float speed_wz;
static float speed_xy;
static float speed_xz;
static float speed_yz;

static const float offset4d[4] = {  0.0,  0.0,  0.0,  3.0 };
static const float offset3d[4] = {  0.0,  0.0, -2.0,  0.0 };


static XrmOptionDescRec opts[] =
{
  {"-mode",            ".displayMode",  XrmoptionSepArg, 0 },
  {"-wireframe",       ".displayMode",  XrmoptionNoArg,  "wireframe" },
  {"-surface",         ".displayMode",  XrmoptionNoArg,  "surface" },
  {"-transparent",     ".displayMode",  XrmoptionNoArg,  "transparent" },
  {"-polytope",        ".polytope",     XrmoptionSepArg, 0 },
  {"-random",          ".polytope",     XrmoptionNoArg,  "random" },
  {"-5-cell",          ".polytope",     XrmoptionNoArg,  "5-cell" },
  {"-8-cell",          ".polytope",     XrmoptionNoArg,  "8-cell" },
  {"-16-cell",         ".polytope",     XrmoptionNoArg,  "16-cell" },
  {"-24-cell",         ".polytope",     XrmoptionNoArg,  "24-cell" },
  {"-120-cell",        ".polytope",     XrmoptionNoArg,  "120-cell" },
  {"-600-cell",        ".polytope",     XrmoptionNoArg,  "600-cell" },
  {"-single-color",    ".colors",       XrmoptionNoArg,  "single" },
  {"-depth-colors",    ".colors",       XrmoptionNoArg,  "depth" },
  {"-perspective-3d",  ".projection3d", XrmoptionNoArg,  "perspective" },
  {"-orthographic-3d", ".projection3d", XrmoptionNoArg,  "orthographic" },
  {"-perspective-4d",  ".projection4d", XrmoptionNoArg,  "perspective" },
  {"-orthographic-4d", ".projection4d", XrmoptionNoArg,  "orthographic" },
  {"-speed-wx",        ".speedwx",      XrmoptionSepArg, 0 },
  {"-speed-wy",        ".speedwy",      XrmoptionSepArg, 0 },
  {"-speed-wz",        ".speedwz",      XrmoptionSepArg, 0 },
  {"-speed-xy",        ".speedxy",      XrmoptionSepArg, 0 },
  {"-speed-xz",        ".speedxz",      XrmoptionSepArg, 0 },
  {"-speed-yz",        ".speedyz",      XrmoptionSepArg, 0 }
};

static argtype vars[] =
{
  { &mode,      "displayMode",  "DisplayMode", DEF_DISPLAY_MODE,  t_String },
  { &poly_str,  "polytope",     "Polytope",    DEF_POLYTOPE,      t_String },
  { &color_str, "colors",       "Colors",      DEF_COLORS,        t_String },
  { &proj_3d,   "projection3d", "Projection3d",DEF_PROJECTION_3D, t_String },
  { &proj_4d,   "projection4d", "Projection4d",DEF_PROJECTION_4D, t_String },
  { &speed_wx,  "speedwx",      "Speedwx",     DEF_SPEEDWX,       t_Float},
  { &speed_wy,  "speedwy",      "Speedwy",     DEF_SPEEDWY,       t_Float},
  { &speed_wz,  "speedwz",      "Speedwz",     DEF_SPEEDWZ,       t_Float},
  { &speed_xy,  "speedxy",      "Speedxy",     DEF_SPEEDXY,       t_Float},
  { &speed_xz,  "speedxz",      "Speedxz",     DEF_SPEEDXZ,       t_Float},
  { &speed_yz,  "speedyz",      "Speedyz",     DEF_SPEEDYZ,       t_Float}
};

static OptionStruct desc[] =
{
  { "-wireframe",       "display the polytope as a wireframe mesh" },
  { "-surface",         "display the polytope as a solid surface" },
  { "-transparent",     "display the polytope as a transparent surface" },
  { "-solid",           "display the polytope as a solid object" },
  { "-bands",           "display the polytope as see-through bands" },
  { "-twosided",        "display the polytope with two colors" },
  { "-colorwheel",      "display the polytope with a smooth color wheel" },
  { "-perspective-3d",  "project the polytope perspectively from 3d to 2d" },
  { "-orthographic-3d", "project the polytope orthographically from 3d to 2d"},
  { "-perspective-4d",  "project the polytope perspectively from 4d to 3d" },
  { "-orthographic-4d", "project the polytope orthographically from 4d to 3d"},
  { "-speed-wx <arg>",  "rotation speed around the wx plane" },
  { "-speed-wy <arg>",  "rotation speed around the wy plane" },
  { "-speed-wz <arg>",  "rotation speed around the wz plane" },
  { "-speed-xy <arg>",  "rotation speed around the xy plane" },
  { "-speed-xz <arg>",  "rotation speed around the xz plane" },
  { "-speed-yz <arg>",  "rotation speed around the yz plane" }
};

ENTRYPOINT ModeSpecOpt polytopes_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};


/* 5-cell {3,3,3} */
#define NUM_VERT_5 5
#define NUM_EDGE_5 10
#define NUM_FACE_5 10
#define VERT_PER_FACE_5 3

#define MIN_EDGE_DEPTH_5 (-0.5)
#define MAX_EDGE_DEPTH_5 0.75
#define MIN_FACE_DEPTH_5 (-0.5)
#define MAX_FACE_DEPTH_5 (1.0/3.0)


/* 8-cell {4,3,3} */
#define NUM_VERT_8 16
#define NUM_EDGE_8 32
#define NUM_FACE_8 24
#define VERT_PER_FACE_8 4

#define MIN_EDGE_DEPTH_8 (-1.0)
#define MAX_EDGE_DEPTH_8 1.0
#define MIN_FACE_DEPTH_8 (-1.0)
#define MAX_FACE_DEPTH_8 1.0

/* 16-cell {3,3,4} */
#define NUM_VERT_16 8
#define NUM_EDGE_16 24
#define NUM_FACE_16 32
#define VERT_PER_FACE_16 3

#define MIN_EDGE_DEPTH_16 (-1.0)
#define MAX_EDGE_DEPTH_16 1.0
#define MIN_FACE_DEPTH_16 (-2.0/3.0)
#define MAX_FACE_DEPTH_16 (2.0/3.0)


/* 24-cell {3,4,3} */
#define NUM_VERT_24 24
#define NUM_EDGE_24 96
#define NUM_FACE_24 96
#define VERT_PER_FACE_24 3

#define MIN_EDGE_DEPTH_24 (-SQRT2)
#define MAX_EDGE_DEPTH_24 SQRT2
#define MIN_FACE_DEPTH_24 (-SQRT2)
#define MAX_FACE_DEPTH_24 SQRT2


/* 120-cell {5,3,3} */
#define NUM_VERT_120 600
#define NUM_EDGE_120 1200
#define NUM_FACE_120 720
#define VERT_PER_FACE_120 5

#define MIN_EDGE_DEPTH_120 (-GOLDEN22)
#define MAX_EDGE_DEPTH_120 GOLDEN22
#define MIN_FACE_DEPTH_120 (-GOLDEN22)
#define MAX_FACE_DEPTH_120 GOLDEN22


/* 600-cell {3,3,5} */
#define NUM_VERT_600 120
#define NUM_EDGE_600 720
#define NUM_FACE_600 1200
#define VERT_PER_FACE_600 3

#define MIN_EDGE_DEPTH_600 (-GOLDEN/2.0-1)
#define MAX_EDGE_DEPTH_600 (GOLDEN/2.0+1)
#define MIN_FACE_DEPTH_600 ((-2*GOLDEN-2)/3.0)
#define MAX_FACE_DEPTH_600 ((2*GOLDEN+2)/3.0)


typedef struct {
  GLint       WindH, WindW;
  GLXContext *glx_context;
  /* 4D rotation angles */
  float alpha, beta, delta, zeta, eta, theta;
  /* Aspect ratio of the current window */
  float aspect;
  /* Counter */
  int tick, poly;
  /* Trackball states */
  trackball_state *trackballs[2];
  int current_trackball;
  Bool button_pressed;

  float edge_color_5[NUM_EDGE_5][4];
  float face_color_5[NUM_FACE_5][4];
  float face_color_trans_5[NUM_FACE_5][4];

  float edge_color_8[NUM_EDGE_8][4];
  float face_color_8[NUM_FACE_8][4];
  float face_color_trans_8[NUM_FACE_8][4];

  float edge_color_16[NUM_EDGE_16][4];
  float face_color_16[NUM_FACE_16][4];
  float face_color_trans_16[NUM_FACE_16][4];

  float edge_color_24[NUM_EDGE_24][4];
  float face_color_24[NUM_FACE_24][4];
  float face_color_trans_24[NUM_FACE_24][4];

  float edge_color_120[NUM_EDGE_120][4];
  float face_color_120[NUM_FACE_120][4];
  float face_color_trans_120[NUM_FACE_120][4];

  float edge_color_600[NUM_EDGE_600][4];
  float face_color_600[NUM_FACE_600][4];
  float face_color_trans_600[NUM_FACE_600][4];

  float speed_scale;

} polytopesstruct;

static polytopesstruct *poly = (polytopesstruct *) NULL;


/* Vertex, edge, and face data for the six regular polytopes.  All the
   polytopes are constructed with coordinates chosen such that their 4d
   circumsphere has a radius of 2. */

static const float vert_5[NUM_VERT_5][4] = {
  { -SQRT5OVER2, -SQRT5OVER6, -SQRT5OVER12, -0.5 },
  {  SQRT5OVER2, -SQRT5OVER6, -SQRT5OVER12, -0.5 },
  {         0.0, SQRT10OVER3, -SQRT5OVER12, -0.5 },
  {         0.0,         0.0,  SQRT15OVER4, -0.5 },
  {         0.0,         0.0,          0.0,  2.0 }
};

static const int edge_5[NUM_EDGE_5][2] = {
  { 0, 1 }, { 0, 2 }, { 0, 3 }, { 0, 4 }, { 1, 2 }, { 1, 3 }, { 1, 4 },
  { 2, 3 }, { 2, 4 }, { 3, 4 }
};

static const int face_5[NUM_FACE_5][VERT_PER_FACE_5] = {
  { 0, 1, 2 }, { 0, 1, 3 }, { 0, 1, 4 }, { 0, 2, 3 }, { 0, 2, 4 }, { 0, 3, 4 },
  { 1, 2, 3 }, { 1, 2, 4 }, { 1, 3, 4 }, { 2, 3, 4 }
};


static const float vert_8[NUM_VERT_8][4] = {
  { -1.0, -1.0, -1.0, -1.0 }, {  1.0, -1.0, -1.0, -1.0 },
  { -1.0,  1.0, -1.0, -1.0 }, {  1.0,  1.0, -1.0, -1.0 },
  { -1.0, -1.0,  1.0, -1.0 }, {  1.0, -1.0,  1.0, -1.0 },
  { -1.0,  1.0,  1.0, -1.0 }, {  1.0,  1.0,  1.0, -1.0 },
  { -1.0, -1.0, -1.0,  1.0 }, {  1.0, -1.0, -1.0,  1.0 },
  { -1.0,  1.0, -1.0,  1.0 }, {  1.0,  1.0, -1.0,  1.0 },
  { -1.0, -1.0,  1.0,  1.0 }, {  1.0, -1.0,  1.0,  1.0 },
  { -1.0,  1.0,  1.0,  1.0 }, {  1.0,  1.0,  1.0,  1.0 }
};

static const int edge_8[NUM_EDGE_8][2] = {
  {  0,  1 }, {  0,  2 }, {  0,  4 }, {  0,  8 }, {  1,  3 }, {  1,  5 },
  {  1,  9 }, {  2,  3 }, {  2,  6 }, {  2, 10 }, {  3,  7 }, {  3, 11 },
  {  4,  5 }, {  4,  6 }, {  4, 12 }, {  5,  7 }, {  5, 13 }, {  6,  7 },
  {  6, 14 }, {  7, 15 }, {  8,  9 }, {  8, 10 }, {  8, 12 }, {  9, 11 },
  {  9, 13 }, { 10, 11 }, { 10, 14 }, { 11, 15 }, { 12, 13 }, { 12, 14 },
  { 13, 15 }, { 14, 15 }
};

static const int face_8[NUM_FACE_8][VERT_PER_FACE_8] = {
  {  0,  1,  3,  2 }, {  0,  1,  5,  4 }, {  0,  1,  9,  8 },
  {  0,  2,  6,  4 }, {  0,  2, 10,  8 }, {  0,  4, 12,  8 },
  {  1,  3,  7,  5 }, {  1,  3, 11,  9 }, {  1,  5, 13,  9 },
  {  2,  3,  7,  6 }, {  2,  3, 11, 10 }, {  2,  6, 14, 10 },
  {  3,  7, 15, 11 }, {  4,  5,  7,  6 }, {  4,  5, 13, 12 },
  {  4,  6, 14, 12 }, {  5,  7, 15, 13 }, {  6,  7, 15, 14 },
  {  8,  9, 11, 10 }, {  8,  9, 13, 12 }, {  8, 10, 14, 12 },
  {  9, 11, 15, 13 }, { 10, 11, 15, 14 }, { 12, 13, 15, 14 }
};



static const float vert_16[NUM_VERT_16][4] = {
  {  0.0,  0.0,  0.0, -2.0 }, {  0.0,  0.0, -2.0,  0.0 },
  {  0.0, -2.0,  0.0,  0.0 }, { -2.0,  0.0,  0.0,  0.0 },
  {  2.0,  0.0,  0.0,  0.0 }, {  0.0,  2.0,  0.0,  0.0 },
  {  0.0,  0.0,  2.0,  0.0 }, {  0.0,  0.0,  0.0,  2.0 }
};

static const int edge_16[NUM_EDGE_16][2] = {
  { 0, 1 }, { 0, 2 }, { 0, 3 }, { 0, 4 }, { 0, 5 }, { 0, 6 }, { 1, 2 },
  { 1, 3 }, { 1, 4 }, { 1, 5 }, { 1, 7 }, { 2, 3 }, { 2, 4 }, { 2, 6 },
  { 2, 7 }, { 3, 5 }, { 3, 6 }, { 3, 7 }, { 4, 5 }, { 4, 6 }, { 4, 7 },
  { 5, 6 }, { 5, 7 }, { 6, 7 }
};

static const int face_16[NUM_FACE_16][VERT_PER_FACE_16] = {
  { 0, 1, 2 }, { 0, 1, 3 }, { 0, 1, 4 }, { 0, 1, 5 }, { 0, 2, 3 }, { 0, 2, 4 },
  { 0, 2, 6 }, { 0, 3, 5 }, { 0, 3, 6 }, { 0, 4, 5 }, { 0, 4, 6 }, { 0, 5, 6 },
  { 1, 2, 3 }, { 1, 2, 4 }, { 1, 2, 7 }, { 1, 3, 5 }, { 1, 3, 7 }, { 1, 4, 5 },
  { 1, 4, 7 }, { 1, 5, 7 }, { 2, 3, 6 }, { 2, 3, 7 }, { 2, 4, 6 }, { 2, 4, 7 },
  { 2, 6, 7 }, { 3, 5, 6 }, { 3, 5, 7 }, { 3, 6, 7 }, { 4, 5, 6 }, { 4, 5, 7 },
  { 4, 6, 7 }, { 5, 6, 7 }
};



static const float vert_24[NUM_VERT_24][4] = {
  {    0.0,    0.0, -SQRT2, -SQRT2 }, {    0.0, -SQRT2,    0.0, -SQRT2 },
  { -SQRT2,    0.0,    0.0, -SQRT2 }, {  SQRT2,    0.0,    0.0, -SQRT2 },
  {    0.0,  SQRT2,    0.0, -SQRT2 }, {    0.0,    0.0,  SQRT2, -SQRT2 },
  {    0.0, -SQRT2, -SQRT2,    0.0 }, { -SQRT2,    0.0, -SQRT2,    0.0 },
  {  SQRT2,    0.0, -SQRT2,    0.0 }, {    0.0,  SQRT2, -SQRT2,    0.0 },
  { -SQRT2, -SQRT2,    0.0,    0.0 }, {  SQRT2, -SQRT2,    0.0,    0.0 },
  { -SQRT2,  SQRT2,    0.0,    0.0 }, {  SQRT2,  SQRT2,    0.0,    0.0 },
  {    0.0, -SQRT2,  SQRT2,    0.0 }, { -SQRT2,    0.0,  SQRT2,    0.0 },
  {  SQRT2,    0.0,  SQRT2,    0.0 }, {    0.0,  SQRT2,  SQRT2,    0.0 },
  {    0.0,    0.0, -SQRT2,  SQRT2 }, {    0.0, -SQRT2,    0.0,  SQRT2 },
  { -SQRT2,    0.0,    0.0,  SQRT2 }, {  SQRT2,    0.0,    0.0,  SQRT2 },
  {    0.0,  SQRT2,    0.0,  SQRT2 }, {    0.0,    0.0,  SQRT2,  SQRT2 }
};

static const int edge_24[NUM_EDGE_24][2] = {
  {  0,  1 }, {  0,  2 }, {  0,  3 }, {  0,  4 }, {  0,  6 }, {  0,  7 },
  {  0,  8 }, {  0,  9 }, {  1,  2 }, {  1,  3 }, {  1,  5 }, {  1,  6 },
  {  1, 10 }, {  1, 11 }, {  1, 14 }, {  2,  4 }, {  2,  5 }, {  2,  7 },
  {  2, 10 }, {  2, 12 }, {  2, 15 }, {  3,  4 }, {  3,  5 }, {  3,  8 },
  {  3, 11 }, {  3, 13 }, {  3, 16 }, {  4,  5 }, {  4,  9 }, {  4, 12 },
  {  4, 13 }, {  4, 17 }, {  5, 14 }, {  5, 15 }, {  5, 16 }, {  5, 17 },
  {  6,  7 }, {  6,  8 }, {  6, 10 }, {  6, 11 }, {  6, 18 }, {  6, 19 },
  {  7,  9 }, {  7, 10 }, {  7, 12 }, {  7, 18 }, {  7, 20 }, {  8,  9 },
  {  8, 11 }, {  8, 13 }, {  8, 18 }, {  8, 21 }, {  9, 12 }, {  9, 13 },
  {  9, 18 }, {  9, 22 }, { 10, 14 }, { 10, 15 }, { 10, 19 }, { 10, 20 },
  { 11, 14 }, { 11, 16 }, { 11, 19 }, { 11, 21 }, { 12, 15 }, { 12, 17 },
  { 12, 20 }, { 12, 22 }, { 13, 16 }, { 13, 17 }, { 13, 21 }, { 13, 22 },
  { 14, 15 }, { 14, 16 }, { 14, 19 }, { 14, 23 }, { 15, 17 }, { 15, 20 },
  { 15, 23 }, { 16, 17 }, { 16, 21 }, { 16, 23 }, { 17, 22 }, { 17, 23 },
  { 18, 19 }, { 18, 20 }, { 18, 21 }, { 18, 22 }, { 19, 20 }, { 19, 21 },
  { 19, 23 }, { 20, 22 }, { 20, 23 }, { 21, 22 }, { 21, 23 }, { 22, 23 }
};

static const int face_24[NUM_FACE_24][VERT_PER_FACE_24] = {
  {  0,  1,  2 }, {  0,  1,  3 }, {  0,  1,  6 }, {  0,  2,  4 }, 
  {  0,  2,  7 }, {  0,  3,  4 }, {  0,  3,  8 }, {  0,  4,  9 },
  {  0,  6,  7 }, {  0,  6,  8 }, {  0,  7,  9 }, {  0,  8,  9 },
  {  1,  2,  5 }, {  1,  2, 10 }, {  1,  3,  5 }, {  1,  3, 11 },
  {  1,  5, 14 }, {  1,  6, 10 }, {  1,  6, 11 }, {  1, 10, 14 },
  {  1, 11, 14 }, {  2,  4,  5 }, {  2,  4, 12 }, {  2,  5, 15 },
  {  2,  7, 10 }, {  2,  7, 12 }, {  2, 10, 15 }, {  2, 12, 15 },
  {  3,  4,  5 }, {  3,  4, 13 }, {  3,  5, 16 }, {  3,  8, 11 },
  {  3,  8, 13 }, {  3, 11, 16 }, {  3, 13, 16 }, {  4,  5, 17 },
  {  4,  9, 12 }, {  4,  9, 13 }, {  4, 12, 17 }, {  4, 13, 17 },
  {  5, 14, 15 }, {  5, 14, 16 }, {  5, 15, 17 }, {  5, 16, 17 },
  {  6,  7, 10 }, {  6,  7, 18 }, {  6,  8, 11 }, {  6,  8, 18 },
  {  6, 10, 19 }, {  6, 11, 19 }, {  6, 18, 19 }, {  7,  9, 12 },
  {  7,  9, 18 }, {  7, 10, 20 }, {  7, 12, 20 }, {  7, 18, 20 },
  {  8,  9, 13 }, {  8,  9, 18 }, {  8, 11, 21 }, {  8, 13, 21 },
  {  8, 18, 21 }, {  9, 12, 22 }, {  9, 13, 22 }, {  9, 18, 22 },
  { 10, 14, 15 }, { 10, 14, 19 }, { 10, 15, 20 }, { 10, 19, 20 },
  { 11, 14, 16 }, { 11, 14, 19 }, { 11, 16, 21 }, { 11, 19, 21 },
  { 12, 15, 17 }, { 12, 15, 20 }, { 12, 17, 22 }, { 12, 20, 22 },
  { 13, 16, 17 }, { 13, 16, 21 }, { 13, 17, 22 }, { 13, 21, 22 },
  { 14, 15, 23 }, { 14, 16, 23 }, { 14, 19, 23 }, { 15, 17, 23 },
  { 15, 20, 23 }, { 16, 17, 23 }, { 16, 21, 23 }, { 17, 22, 23 },
  { 18, 19, 20 }, { 18, 19, 21 }, { 18, 20, 22 }, { 18, 21, 22 },
  { 19, 20, 23 }, { 19, 21, 23 }, { 20, 22, 23 }, { 21, 22, 23 }
};




static const float vert_120[NUM_VERT_120][4] = {
  { -GOLDENINV22,          0.0,    -SQRT2INV,    -GOLDEN22 },
  {  GOLDENINV22,          0.0,    -SQRT2INV,    -GOLDEN22 },
  {  -GOLDENINV2,  -GOLDENINV2,  -GOLDENINV2,    -GOLDEN22 },
  {   GOLDENINV2,  -GOLDENINV2,  -GOLDENINV2,    -GOLDEN22 },
  {  -GOLDENINV2,   GOLDENINV2,  -GOLDENINV2,    -GOLDEN22 },
  {   GOLDENINV2,   GOLDENINV2,  -GOLDENINV2,    -GOLDEN22 },
  {          0.0,    -SQRT2INV, -GOLDENINV22,    -GOLDEN22 },
  {          0.0,     SQRT2INV, -GOLDENINV22,    -GOLDEN22 },
  {    -SQRT2INV, -GOLDENINV22,          0.0,    -GOLDEN22 },
  {     SQRT2INV, -GOLDENINV22,          0.0,    -GOLDEN22 },
  {    -SQRT2INV,  GOLDENINV22,          0.0,    -GOLDEN22 },
  {     SQRT2INV,  GOLDENINV22,          0.0,    -GOLDEN22 },
  {          0.0,    -SQRT2INV,  GOLDENINV22,    -GOLDEN22 },
  {          0.0,     SQRT2INV,  GOLDENINV22,    -GOLDEN22 },
  {  -GOLDENINV2,  -GOLDENINV2,   GOLDENINV2,    -GOLDEN22 },
  {   GOLDENINV2,  -GOLDENINV2,   GOLDENINV2,    -GOLDEN22 },
  {  -GOLDENINV2,   GOLDENINV2,   GOLDENINV2,    -GOLDEN22 },
  {   GOLDENINV2,   GOLDENINV2,   GOLDENINV2,    -GOLDEN22 },
  { -GOLDENINV22,          0.0,     SQRT2INV,    -GOLDEN22 },
  {  GOLDENINV22,          0.0,     SQRT2INV,    -GOLDEN22 },
  {  -GOLDENINV2,          0.0,     -GOLDEN2,  -SQRT5OVER2 },
  {   GOLDENINV2,          0.0,     -GOLDEN2,  -SQRT5OVER2 },
  {    -SQRT2INV,    -SQRT2INV,    -SQRT2INV,  -SQRT5OVER2 },
  {     SQRT2INV,    -SQRT2INV,    -SQRT2INV,  -SQRT5OVER2 },
  {    -SQRT2INV,     SQRT2INV,    -SQRT2INV,  -SQRT5OVER2 },
  {     SQRT2INV,     SQRT2INV,    -SQRT2INV,  -SQRT5OVER2 },
  {          0.0,     -GOLDEN2,  -GOLDENINV2,  -SQRT5OVER2 },
  {          0.0,      GOLDEN2,  -GOLDENINV2,  -SQRT5OVER2 },
  {     -GOLDEN2,  -GOLDENINV2,          0.0,  -SQRT5OVER2 },
  {      GOLDEN2,  -GOLDENINV2,          0.0,  -SQRT5OVER2 },
  {     -GOLDEN2,   GOLDENINV2,          0.0,  -SQRT5OVER2 },
  {      GOLDEN2,   GOLDENINV2,          0.0,  -SQRT5OVER2 },
  {          0.0,     -GOLDEN2,   GOLDENINV2,  -SQRT5OVER2 },
  {          0.0,      GOLDEN2,   GOLDENINV2,  -SQRT5OVER2 },
  {    -SQRT2INV,    -SQRT2INV,     SQRT2INV,  -SQRT5OVER2 },
  {     SQRT2INV,    -SQRT2INV,     SQRT2INV,  -SQRT5OVER2 },
  {    -SQRT2INV,     SQRT2INV,     SQRT2INV,  -SQRT5OVER2 },
  {     SQRT2INV,     SQRT2INV,     SQRT2INV,  -SQRT5OVER2 },
  {  -GOLDENINV2,          0.0,      GOLDEN2,  -SQRT5OVER2 },
  {   GOLDENINV2,          0.0,      GOLDEN2,  -SQRT5OVER2 },
  {          0.0,          0.0,       -SQRT2,       -SQRT2 },
  {    -SQRT2INV,  -GOLDENINV2,     -GOLDEN2,       -SQRT2 },
  {     SQRT2INV,  -GOLDENINV2,     -GOLDEN2,       -SQRT2 },
  {    -SQRT2INV,   GOLDENINV2,     -GOLDEN2,       -SQRT2 },
  {     SQRT2INV,   GOLDENINV2,     -GOLDEN2,       -SQRT2 },
  {  -GOLDENINV2,     -GOLDEN2,    -SQRT2INV,       -SQRT2 },
  {   GOLDENINV2,     -GOLDEN2,    -SQRT2INV,       -SQRT2 },
  {  -GOLDENINV2,      GOLDEN2,    -SQRT2INV,       -SQRT2 },
  {   GOLDENINV2,      GOLDEN2,    -SQRT2INV,       -SQRT2 },
  {     -GOLDEN2,    -SQRT2INV,  -GOLDENINV2,       -SQRT2 },
  {      GOLDEN2,    -SQRT2INV,  -GOLDENINV2,       -SQRT2 },
  {     -GOLDEN2,     SQRT2INV,  -GOLDENINV2,       -SQRT2 },
  {      GOLDEN2,     SQRT2INV,  -GOLDENINV2,       -SQRT2 },
  {          0.0,       -SQRT2,          0.0,       -SQRT2 },
  {       -SQRT2,          0.0,          0.0,       -SQRT2 },
  {        SQRT2,          0.0,          0.0,       -SQRT2 },
  {          0.0,        SQRT2,          0.0,       -SQRT2 },
  {     -GOLDEN2,    -SQRT2INV,   GOLDENINV2,       -SQRT2 },
  {      GOLDEN2,    -SQRT2INV,   GOLDENINV2,       -SQRT2 },
  {     -GOLDEN2,     SQRT2INV,   GOLDENINV2,       -SQRT2 },
  {      GOLDEN2,     SQRT2INV,   GOLDENINV2,       -SQRT2 },
  {  -GOLDENINV2,     -GOLDEN2,     SQRT2INV,       -SQRT2 },
  {   GOLDENINV2,     -GOLDEN2,     SQRT2INV,       -SQRT2 },
  {  -GOLDENINV2,      GOLDEN2,     SQRT2INV,       -SQRT2 },
  {   GOLDENINV2,      GOLDEN2,     SQRT2INV,       -SQRT2 },
  {    -SQRT2INV,  -GOLDENINV2,      GOLDEN2,       -SQRT2 },
  {     SQRT2INV,  -GOLDENINV2,      GOLDEN2,       -SQRT2 },
  {    -SQRT2INV,   GOLDENINV2,      GOLDEN2,       -SQRT2 },
  {     SQRT2INV,   GOLDENINV2,      GOLDEN2,       -SQRT2 },
  {          0.0,          0.0,        SQRT2,       -SQRT2 },
  {          0.0,  -GOLDENINV2,  -SQRT5OVER2,     -GOLDEN2 },
  {          0.0,   GOLDENINV2,  -SQRT5OVER2,     -GOLDEN2 },
  {  -GOLDENINV2,    -SQRT2INV,       -SQRT2,     -GOLDEN2 },
  {   GOLDENINV2,    -SQRT2INV,       -SQRT2,     -GOLDEN2 },
  {  -GOLDENINV2,     SQRT2INV,       -SQRT2,     -GOLDEN2 },
  {   GOLDENINV2,     SQRT2INV,       -SQRT2,     -GOLDEN2 },
  { -GOLDENINV22,     -GOLDEN2,     -GOLDEN2,     -GOLDEN2 },
  {  GOLDENINV22,     -GOLDEN2,     -GOLDEN2,     -GOLDEN2 },
  {     -GOLDEN2, -GOLDENINV22,     -GOLDEN2,     -GOLDEN2 },
  {      GOLDEN2, -GOLDENINV22,     -GOLDEN2,     -GOLDEN2 },
  {     -GOLDEN2,  GOLDENINV22,     -GOLDEN2,     -GOLDEN2 },
  {      GOLDEN2,  GOLDENINV22,     -GOLDEN2,     -GOLDEN2 },
  { -GOLDENINV22,      GOLDEN2,     -GOLDEN2,     -GOLDEN2 },
  {  GOLDENINV22,      GOLDEN2,     -GOLDEN2,     -GOLDEN2 },
  {       -SQRT2,  -GOLDENINV2,    -SQRT2INV,     -GOLDEN2 },
  {        SQRT2,  -GOLDENINV2,    -SQRT2INV,     -GOLDEN2 },
  {       -SQRT2,   GOLDENINV2,    -SQRT2INV,     -GOLDEN2 },
  {        SQRT2,   GOLDENINV2,    -SQRT2INV,     -GOLDEN2 },
  {    -SQRT2INV,       -SQRT2,  -GOLDENINV2,     -GOLDEN2 },
  {     SQRT2INV,       -SQRT2,  -GOLDENINV2,     -GOLDEN2 },
  {  -SQRT5OVER2,          0.0,  -GOLDENINV2,     -GOLDEN2 },
  {   SQRT5OVER2,          0.0,  -GOLDENINV2,     -GOLDEN2 },
  {    -SQRT2INV,        SQRT2,  -GOLDENINV2,     -GOLDEN2 },
  {     SQRT2INV,        SQRT2,  -GOLDENINV2,     -GOLDEN2 },
  {     -GOLDEN2,     -GOLDEN2, -GOLDENINV22,     -GOLDEN2 },
  {      GOLDEN2,     -GOLDEN2, -GOLDENINV22,     -GOLDEN2 },
  {     -GOLDEN2,      GOLDEN2, -GOLDENINV22,     -GOLDEN2 },
  {      GOLDEN2,      GOLDEN2, -GOLDENINV22,     -GOLDEN2 },
  {  -GOLDENINV2,  -SQRT5OVER2,          0.0,     -GOLDEN2 },
  {   GOLDENINV2,  -SQRT5OVER2,          0.0,     -GOLDEN2 },
  {  -GOLDENINV2,   SQRT5OVER2,          0.0,     -GOLDEN2 },
  {   GOLDENINV2,   SQRT5OVER2,          0.0,     -GOLDEN2 },
  {     -GOLDEN2,     -GOLDEN2,  GOLDENINV22,     -GOLDEN2 },
  {      GOLDEN2,     -GOLDEN2,  GOLDENINV22,     -GOLDEN2 },
  {     -GOLDEN2,      GOLDEN2,  GOLDENINV22,     -GOLDEN2 },
  {      GOLDEN2,      GOLDEN2,  GOLDENINV22,     -GOLDEN2 },
  {    -SQRT2INV,       -SQRT2,   GOLDENINV2,     -GOLDEN2 },
  {     SQRT2INV,       -SQRT2,   GOLDENINV2,     -GOLDEN2 },
  {  -SQRT5OVER2,          0.0,   GOLDENINV2,     -GOLDEN2 },
  {   SQRT5OVER2,          0.0,   GOLDENINV2,     -GOLDEN2 },
  {    -SQRT2INV,        SQRT2,   GOLDENINV2,     -GOLDEN2 },
  {     SQRT2INV,        SQRT2,   GOLDENINV2,     -GOLDEN2 },
  {       -SQRT2,  -GOLDENINV2,     SQRT2INV,     -GOLDEN2 },
  {        SQRT2,  -GOLDENINV2,     SQRT2INV,     -GOLDEN2 },
  {       -SQRT2,   GOLDENINV2,     SQRT2INV,     -GOLDEN2 },
  {        SQRT2,   GOLDENINV2,     SQRT2INV,     -GOLDEN2 },
  { -GOLDENINV22,     -GOLDEN2,      GOLDEN2,     -GOLDEN2 },
  {  GOLDENINV22,     -GOLDEN2,      GOLDEN2,     -GOLDEN2 },
  {     -GOLDEN2, -GOLDENINV22,      GOLDEN2,     -GOLDEN2 },
  {      GOLDEN2, -GOLDENINV22,      GOLDEN2,     -GOLDEN2 },
  {     -GOLDEN2,  GOLDENINV22,      GOLDEN2,     -GOLDEN2 },
  {      GOLDEN2,  GOLDENINV22,      GOLDEN2,     -GOLDEN2 },
  { -GOLDENINV22,      GOLDEN2,      GOLDEN2,     -GOLDEN2 },
  {  GOLDENINV22,      GOLDEN2,      GOLDEN2,     -GOLDEN2 },
  {  -GOLDENINV2,    -SQRT2INV,        SQRT2,     -GOLDEN2 },
  {   GOLDENINV2,    -SQRT2INV,        SQRT2,     -GOLDEN2 },
  {  -GOLDENINV2,     SQRT2INV,        SQRT2,     -GOLDEN2 },
  {   GOLDENINV2,     SQRT2INV,        SQRT2,     -GOLDEN2 },
  {          0.0,  -GOLDENINV2,   SQRT5OVER2,     -GOLDEN2 },
  {          0.0,   GOLDENINV2,   SQRT5OVER2,     -GOLDEN2 },
  {          0.0, -GOLDENINV22,    -GOLDEN22,    -SQRT2INV },
  {          0.0,  GOLDENINV22,    -GOLDEN22,    -SQRT2INV },
  {    -SQRT2INV,    -SQRT2INV,  -SQRT5OVER2,    -SQRT2INV },
  {     SQRT2INV,    -SQRT2INV,  -SQRT5OVER2,    -SQRT2INV },
  {    -SQRT2INV,     SQRT2INV,  -SQRT5OVER2,    -SQRT2INV },
  {     SQRT2INV,     SQRT2INV,  -SQRT5OVER2,    -SQRT2INV },
  {     -GOLDEN2,  -GOLDENINV2,       -SQRT2,    -SQRT2INV },
  {      GOLDEN2,  -GOLDENINV2,       -SQRT2,    -SQRT2INV },
  {     -GOLDEN2,   GOLDENINV2,       -SQRT2,    -SQRT2INV },
  {      GOLDEN2,   GOLDENINV2,       -SQRT2,    -SQRT2INV },
  {  -GOLDENINV2,       -SQRT2,     -GOLDEN2,    -SQRT2INV },
  {   GOLDENINV2,       -SQRT2,     -GOLDEN2,    -SQRT2INV },
  {  -GOLDENINV2,        SQRT2,     -GOLDEN2,    -SQRT2INV },
  {   GOLDENINV2,        SQRT2,     -GOLDEN2,    -SQRT2INV },
  {    -SQRT2INV,  -SQRT5OVER2,    -SQRT2INV,    -SQRT2INV },
  {     SQRT2INV,  -SQRT5OVER2,    -SQRT2INV,    -SQRT2INV },
  {  -SQRT5OVER2,    -SQRT2INV,    -SQRT2INV,    -SQRT2INV },
  {   SQRT5OVER2,    -SQRT2INV,    -SQRT2INV,    -SQRT2INV },
  {  -SQRT5OVER2,     SQRT2INV,    -SQRT2INV,    -SQRT2INV },
  {   SQRT5OVER2,     SQRT2INV,    -SQRT2INV,    -SQRT2INV },
  {    -SQRT2INV,   SQRT5OVER2,    -SQRT2INV,    -SQRT2INV },
  {     SQRT2INV,   SQRT5OVER2,    -SQRT2INV,    -SQRT2INV },
  {       -SQRT2,     -GOLDEN2,  -GOLDENINV2,    -SQRT2INV },
  {        SQRT2,     -GOLDEN2,  -GOLDENINV2,    -SQRT2INV },
  {       -SQRT2,      GOLDEN2,  -GOLDENINV2,    -SQRT2INV },
  {        SQRT2,      GOLDEN2,  -GOLDENINV2,    -SQRT2INV },
  {    -GOLDEN22,          0.0, -GOLDENINV22,    -SQRT2INV },
  {     GOLDEN22,          0.0, -GOLDENINV22,    -SQRT2INV },
  { -GOLDENINV22,    -GOLDEN22,          0.0,    -SQRT2INV },
  {  GOLDENINV22,    -GOLDEN22,          0.0,    -SQRT2INV },
  { -GOLDENINV22,     GOLDEN22,          0.0,    -SQRT2INV },
  {  GOLDENINV22,     GOLDEN22,          0.0,    -SQRT2INV },
  {    -GOLDEN22,          0.0,  GOLDENINV22,    -SQRT2INV },
  {     GOLDEN22,          0.0,  GOLDENINV22,    -SQRT2INV },
  {       -SQRT2,     -GOLDEN2,   GOLDENINV2,    -SQRT2INV },
  {        SQRT2,     -GOLDEN2,   GOLDENINV2,    -SQRT2INV },
  {       -SQRT2,      GOLDEN2,   GOLDENINV2,    -SQRT2INV },
  {        SQRT2,      GOLDEN2,   GOLDENINV2,    -SQRT2INV },
  {    -SQRT2INV,  -SQRT5OVER2,     SQRT2INV,    -SQRT2INV },
  {     SQRT2INV,  -SQRT5OVER2,     SQRT2INV,    -SQRT2INV },
  {  -SQRT5OVER2,    -SQRT2INV,     SQRT2INV,    -SQRT2INV },
  {   SQRT5OVER2,    -SQRT2INV,     SQRT2INV,    -SQRT2INV },
  {  -SQRT5OVER2,     SQRT2INV,     SQRT2INV,    -SQRT2INV },
  {   SQRT5OVER2,     SQRT2INV,     SQRT2INV,    -SQRT2INV },
  {    -SQRT2INV,   SQRT5OVER2,     SQRT2INV,    -SQRT2INV },
  {     SQRT2INV,   SQRT5OVER2,     SQRT2INV,    -SQRT2INV },
  {  -GOLDENINV2,       -SQRT2,      GOLDEN2,    -SQRT2INV },
  {   GOLDENINV2,       -SQRT2,      GOLDEN2,    -SQRT2INV },
  {  -GOLDENINV2,        SQRT2,      GOLDEN2,    -SQRT2INV },
  {   GOLDENINV2,        SQRT2,      GOLDEN2,    -SQRT2INV },
  {     -GOLDEN2,  -GOLDENINV2,        SQRT2,    -SQRT2INV },
  {      GOLDEN2,  -GOLDENINV2,        SQRT2,    -SQRT2INV },
  {     -GOLDEN2,   GOLDENINV2,        SQRT2,    -SQRT2INV },
  {      GOLDEN2,   GOLDENINV2,        SQRT2,    -SQRT2INV },
  {    -SQRT2INV,    -SQRT2INV,   SQRT5OVER2,    -SQRT2INV },
  {     SQRT2INV,    -SQRT2INV,   SQRT5OVER2,    -SQRT2INV },
  {    -SQRT2INV,     SQRT2INV,   SQRT5OVER2,    -SQRT2INV },
  {     SQRT2INV,     SQRT2INV,   SQRT5OVER2,    -SQRT2INV },
  {          0.0, -GOLDENINV22,     GOLDEN22,    -SQRT2INV },
  {          0.0,  GOLDENINV22,     GOLDEN22,    -SQRT2INV },
  {  -GOLDENINV2,  -GOLDENINV2,    -GOLDEN22,  -GOLDENINV2 },
  {   GOLDENINV2,  -GOLDENINV2,    -GOLDEN22,  -GOLDENINV2 },
  {  -GOLDENINV2,   GOLDENINV2,    -GOLDEN22,  -GOLDENINV2 },
  {   GOLDENINV2,   GOLDENINV2,    -GOLDEN22,  -GOLDENINV2 },
  {     -GOLDEN2,          0.0,  -SQRT5OVER2,  -GOLDENINV2 },
  {      GOLDEN2,          0.0,  -SQRT5OVER2,  -GOLDENINV2 },
  {    -SQRT2INV,     -GOLDEN2,       -SQRT2,  -GOLDENINV2 },
  {     SQRT2INV,     -GOLDEN2,       -SQRT2,  -GOLDENINV2 },
  {    -SQRT2INV,      GOLDEN2,       -SQRT2,  -GOLDENINV2 },
  {     SQRT2INV,      GOLDEN2,       -SQRT2,  -GOLDENINV2 },
  {          0.0,  -SQRT5OVER2,     -GOLDEN2,  -GOLDENINV2 },
  {       -SQRT2,    -SQRT2INV,     -GOLDEN2,  -GOLDENINV2 },
  {        SQRT2,    -SQRT2INV,     -GOLDEN2,  -GOLDENINV2 },
  {       -SQRT2,     SQRT2INV,     -GOLDEN2,  -GOLDENINV2 },
  {        SQRT2,     SQRT2INV,     -GOLDEN2,  -GOLDENINV2 },
  {          0.0,   SQRT5OVER2,     -GOLDEN2,  -GOLDENINV2 },
  {     -GOLDEN2,       -SQRT2,    -SQRT2INV,  -GOLDENINV2 },
  {      GOLDEN2,       -SQRT2,    -SQRT2INV,  -GOLDENINV2 },
  {     -GOLDEN2,        SQRT2,    -SQRT2INV,  -GOLDENINV2 },
  {      GOLDEN2,        SQRT2,    -SQRT2INV,  -GOLDENINV2 },
  {  -GOLDENINV2,    -GOLDEN22,  -GOLDENINV2,  -GOLDENINV2 },
  {   GOLDENINV2,    -GOLDEN22,  -GOLDENINV2,  -GOLDENINV2 },
  {    -GOLDEN22,  -GOLDENINV2,  -GOLDENINV2,  -GOLDENINV2 },
  {     GOLDEN22,  -GOLDENINV2,  -GOLDENINV2,  -GOLDENINV2 },
  {    -GOLDEN22,   GOLDENINV2,  -GOLDENINV2,  -GOLDENINV2 },
  {     GOLDEN22,   GOLDENINV2,  -GOLDENINV2,  -GOLDENINV2 },
  {  -GOLDENINV2,     GOLDEN22,  -GOLDENINV2,  -GOLDENINV2 },
  {   GOLDENINV2,     GOLDEN22,  -GOLDENINV2,  -GOLDENINV2 },
  {  -SQRT5OVER2,     -GOLDEN2,          0.0,  -GOLDENINV2 },
  {   SQRT5OVER2,     -GOLDEN2,          0.0,  -GOLDENINV2 },
  {  -SQRT5OVER2,      GOLDEN2,          0.0,  -GOLDENINV2 },
  {   SQRT5OVER2,      GOLDEN2,          0.0,  -GOLDENINV2 },
  {  -GOLDENINV2,    -GOLDEN22,   GOLDENINV2,  -GOLDENINV2 },
  {   GOLDENINV2,    -GOLDEN22,   GOLDENINV2,  -GOLDENINV2 },
  {    -GOLDEN22,  -GOLDENINV2,   GOLDENINV2,  -GOLDENINV2 },
  {     GOLDEN22,  -GOLDENINV2,   GOLDENINV2,  -GOLDENINV2 },
  {    -GOLDEN22,   GOLDENINV2,   GOLDENINV2,  -GOLDENINV2 },
  {     GOLDEN22,   GOLDENINV2,   GOLDENINV2,  -GOLDENINV2 },
  {  -GOLDENINV2,     GOLDEN22,   GOLDENINV2,  -GOLDENINV2 },
  {   GOLDENINV2,     GOLDEN22,   GOLDENINV2,  -GOLDENINV2 },
  {     -GOLDEN2,       -SQRT2,     SQRT2INV,  -GOLDENINV2 },
  {      GOLDEN2,       -SQRT2,     SQRT2INV,  -GOLDENINV2 },
  {     -GOLDEN2,        SQRT2,     SQRT2INV,  -GOLDENINV2 },
  {      GOLDEN2,        SQRT2,     SQRT2INV,  -GOLDENINV2 },
  {          0.0,  -SQRT5OVER2,      GOLDEN2,  -GOLDENINV2 },
  {       -SQRT2,    -SQRT2INV,      GOLDEN2,  -GOLDENINV2 },
  {        SQRT2,    -SQRT2INV,      GOLDEN2,  -GOLDENINV2 },
  {       -SQRT2,     SQRT2INV,      GOLDEN2,  -GOLDENINV2 },
  {        SQRT2,     SQRT2INV,      GOLDEN2,  -GOLDENINV2 },
  {          0.0,   SQRT5OVER2,      GOLDEN2,  -GOLDENINV2 },
  {    -SQRT2INV,     -GOLDEN2,        SQRT2,  -GOLDENINV2 },
  {     SQRT2INV,     -GOLDEN2,        SQRT2,  -GOLDENINV2 },
  {    -SQRT2INV,      GOLDEN2,        SQRT2,  -GOLDENINV2 },
  {     SQRT2INV,      GOLDEN2,        SQRT2,  -GOLDENINV2 },
  {     -GOLDEN2,          0.0,   SQRT5OVER2,  -GOLDENINV2 },
  {      GOLDEN2,          0.0,   SQRT5OVER2,  -GOLDENINV2 },
  {  -GOLDENINV2,  -GOLDENINV2,     GOLDEN22,  -GOLDENINV2 },
  {   GOLDENINV2,  -GOLDENINV2,     GOLDEN22,  -GOLDENINV2 },
  {  -GOLDENINV2,   GOLDENINV2,     GOLDEN22,  -GOLDENINV2 },
  {   GOLDENINV2,   GOLDENINV2,     GOLDEN22,  -GOLDENINV2 },
  {    -SQRT2INV,          0.0,    -GOLDEN22, -GOLDENINV22 },
  {     SQRT2INV,          0.0,    -GOLDEN22, -GOLDENINV22 },
  {     -GOLDEN2,     -GOLDEN2,     -GOLDEN2, -GOLDENINV22 },
  {      GOLDEN2,     -GOLDEN2,     -GOLDEN2, -GOLDENINV22 },
  {     -GOLDEN2,      GOLDEN2,     -GOLDEN2, -GOLDENINV22 },
  {      GOLDEN2,      GOLDEN2,     -GOLDEN2, -GOLDENINV22 },
  {          0.0,    -GOLDEN22,    -SQRT2INV, -GOLDENINV22 },
  {          0.0,     GOLDEN22,    -SQRT2INV, -GOLDENINV22 },
  {    -GOLDEN22,    -SQRT2INV,          0.0, -GOLDENINV22 },
  {     GOLDEN22,    -SQRT2INV,          0.0, -GOLDENINV22 },
  {    -GOLDEN22,     SQRT2INV,          0.0, -GOLDENINV22 },
  {     GOLDEN22,     SQRT2INV,          0.0, -GOLDENINV22 },
  {          0.0,    -GOLDEN22,     SQRT2INV, -GOLDENINV22 },
  {          0.0,     GOLDEN22,     SQRT2INV, -GOLDENINV22 },
  {     -GOLDEN2,     -GOLDEN2,      GOLDEN2, -GOLDENINV22 },
  {      GOLDEN2,     -GOLDEN2,      GOLDEN2, -GOLDENINV22 },
  {     -GOLDEN2,      GOLDEN2,      GOLDEN2, -GOLDENINV22 },
  {      GOLDEN2,      GOLDEN2,      GOLDEN2, -GOLDENINV22 },
  {    -SQRT2INV,          0.0,     GOLDEN22, -GOLDENINV22 },
  {     SQRT2INV,          0.0,     GOLDEN22, -GOLDENINV22 },
  { -GOLDENINV22,    -SQRT2INV,    -GOLDEN22,          0.0 },
  {  GOLDENINV22,    -SQRT2INV,    -GOLDEN22,          0.0 },
  { -GOLDENINV22,     SQRT2INV,    -GOLDEN22,          0.0 },
  {  GOLDENINV22,     SQRT2INV,    -GOLDEN22,          0.0 },
  {  -GOLDENINV2,     -GOLDEN2,  -SQRT5OVER2,          0.0 },
  {   GOLDENINV2,     -GOLDEN2,  -SQRT5OVER2,          0.0 },
  {  -GOLDENINV2,      GOLDEN2,  -SQRT5OVER2,          0.0 },
  {   GOLDENINV2,      GOLDEN2,  -SQRT5OVER2,          0.0 },
  {          0.0,       -SQRT2,       -SQRT2,          0.0 },
  {       -SQRT2,          0.0,       -SQRT2,          0.0 },
  {        SQRT2,          0.0,       -SQRT2,          0.0 },
  {          0.0,        SQRT2,       -SQRT2,          0.0 },
  {  -SQRT5OVER2,  -GOLDENINV2,     -GOLDEN2,          0.0 },
  {   SQRT5OVER2,  -GOLDENINV2,     -GOLDEN2,          0.0 },
  {  -SQRT5OVER2,   GOLDENINV2,     -GOLDEN2,          0.0 },
  {   SQRT5OVER2,   GOLDENINV2,     -GOLDEN2,          0.0 },
  {    -GOLDEN22, -GOLDENINV22,    -SQRT2INV,          0.0 },
  {     GOLDEN22, -GOLDENINV22,    -SQRT2INV,          0.0 },
  {    -GOLDEN22,  GOLDENINV22,    -SQRT2INV,          0.0 },
  {     GOLDEN22,  GOLDENINV22,    -SQRT2INV,          0.0 },
  {     -GOLDEN2,  -SQRT5OVER2,  -GOLDENINV2,          0.0 },
  {      GOLDEN2,  -SQRT5OVER2,  -GOLDENINV2,          0.0 },
  {     -GOLDEN2,   SQRT5OVER2,  -GOLDENINV2,          0.0 },
  {      GOLDEN2,   SQRT5OVER2,  -GOLDENINV2,          0.0 },
  {    -SQRT2INV,    -GOLDEN22, -GOLDENINV22,          0.0 },
  {     SQRT2INV,    -GOLDEN22, -GOLDENINV22,          0.0 },
  {    -SQRT2INV,     GOLDEN22, -GOLDENINV22,          0.0 },
  {     SQRT2INV,     GOLDEN22, -GOLDENINV22,          0.0 },
  {       -SQRT2,       -SQRT2,          0.0,          0.0 },
  {        SQRT2,       -SQRT2,          0.0,          0.0 },
  {       -SQRT2,        SQRT2,          0.0,          0.0 },
  {        SQRT2,        SQRT2,          0.0,          0.0 },
  {    -SQRT2INV,    -GOLDEN22,  GOLDENINV22,          0.0 },
  {     SQRT2INV,    -GOLDEN22,  GOLDENINV22,          0.0 },
  {    -SQRT2INV,     GOLDEN22,  GOLDENINV22,          0.0 },
  {     SQRT2INV,     GOLDEN22,  GOLDENINV22,          0.0 },
  {     -GOLDEN2,  -SQRT5OVER2,   GOLDENINV2,          0.0 },
  {      GOLDEN2,  -SQRT5OVER2,   GOLDENINV2,          0.0 },
  {     -GOLDEN2,   SQRT5OVER2,   GOLDENINV2,          0.0 },
  {      GOLDEN2,   SQRT5OVER2,   GOLDENINV2,          0.0 },
  {    -GOLDEN22, -GOLDENINV22,     SQRT2INV,          0.0 },
  {     GOLDEN22, -GOLDENINV22,     SQRT2INV,          0.0 },
  {    -GOLDEN22,  GOLDENINV22,     SQRT2INV,          0.0 },
  {     GOLDEN22,  GOLDENINV22,     SQRT2INV,          0.0 },
  {  -SQRT5OVER2,  -GOLDENINV2,      GOLDEN2,          0.0 },
  {   SQRT5OVER2,  -GOLDENINV2,      GOLDEN2,          0.0 },
  {  -SQRT5OVER2,   GOLDENINV2,      GOLDEN2,          0.0 },
  {   SQRT5OVER2,   GOLDENINV2,      GOLDEN2,          0.0 },
  {          0.0,       -SQRT2,        SQRT2,          0.0 },
  {       -SQRT2,          0.0,        SQRT2,          0.0 },
  {        SQRT2,          0.0,        SQRT2,          0.0 },
  {          0.0,        SQRT2,        SQRT2,          0.0 },
  {  -GOLDENINV2,     -GOLDEN2,   SQRT5OVER2,          0.0 },
  {   GOLDENINV2,     -GOLDEN2,   SQRT5OVER2,          0.0 },
  {  -GOLDENINV2,      GOLDEN2,   SQRT5OVER2,          0.0 },
  {   GOLDENINV2,      GOLDEN2,   SQRT5OVER2,          0.0 },
  { -GOLDENINV22,    -SQRT2INV,     GOLDEN22,          0.0 },
  {  GOLDENINV22,    -SQRT2INV,     GOLDEN22,          0.0 },
  { -GOLDENINV22,     SQRT2INV,     GOLDEN22,          0.0 },
  {  GOLDENINV22,     SQRT2INV,     GOLDEN22,          0.0 },
  {    -SQRT2INV,          0.0,    -GOLDEN22,  GOLDENINV22 },
  {     SQRT2INV,          0.0,    -GOLDEN22,  GOLDENINV22 },
  {     -GOLDEN2,     -GOLDEN2,     -GOLDEN2,  GOLDENINV22 },
  {      GOLDEN2,     -GOLDEN2,     -GOLDEN2,  GOLDENINV22 },
  {     -GOLDEN2,      GOLDEN2,     -GOLDEN2,  GOLDENINV22 },
  {      GOLDEN2,      GOLDEN2,     -GOLDEN2,  GOLDENINV22 },
  {          0.0,    -GOLDEN22,    -SQRT2INV,  GOLDENINV22 },
  {          0.0,     GOLDEN22,    -SQRT2INV,  GOLDENINV22 },
  {    -GOLDEN22,    -SQRT2INV,          0.0,  GOLDENINV22 },
  {     GOLDEN22,    -SQRT2INV,          0.0,  GOLDENINV22 },
  {    -GOLDEN22,     SQRT2INV,          0.0,  GOLDENINV22 },
  {     GOLDEN22,     SQRT2INV,          0.0,  GOLDENINV22 },
  {          0.0,    -GOLDEN22,     SQRT2INV,  GOLDENINV22 },
  {          0.0,     GOLDEN22,     SQRT2INV,  GOLDENINV22 },
  {     -GOLDEN2,     -GOLDEN2,      GOLDEN2,  GOLDENINV22 },
  {      GOLDEN2,     -GOLDEN2,      GOLDEN2,  GOLDENINV22 },
  {     -GOLDEN2,      GOLDEN2,      GOLDEN2,  GOLDENINV22 },
  {      GOLDEN2,      GOLDEN2,      GOLDEN2,  GOLDENINV22 },
  {    -SQRT2INV,          0.0,     GOLDEN22,  GOLDENINV22 },
  {     SQRT2INV,          0.0,     GOLDEN22,  GOLDENINV22 },
  {  -GOLDENINV2,  -GOLDENINV2,    -GOLDEN22,   GOLDENINV2 },
  {   GOLDENINV2,  -GOLDENINV2,    -GOLDEN22,   GOLDENINV2 },
  {  -GOLDENINV2,   GOLDENINV2,    -GOLDEN22,   GOLDENINV2 },
  {   GOLDENINV2,   GOLDENINV2,    -GOLDEN22,   GOLDENINV2 },
  {     -GOLDEN2,          0.0,  -SQRT5OVER2,   GOLDENINV2 },
  {      GOLDEN2,          0.0,  -SQRT5OVER2,   GOLDENINV2 },
  {    -SQRT2INV,     -GOLDEN2,       -SQRT2,   GOLDENINV2 },
  {     SQRT2INV,     -GOLDEN2,       -SQRT2,   GOLDENINV2 },
  {    -SQRT2INV,      GOLDEN2,       -SQRT2,   GOLDENINV2 },
  {     SQRT2INV,      GOLDEN2,       -SQRT2,   GOLDENINV2 },
  {          0.0,  -SQRT5OVER2,     -GOLDEN2,   GOLDENINV2 },
  {       -SQRT2,    -SQRT2INV,     -GOLDEN2,   GOLDENINV2 },
  {        SQRT2,    -SQRT2INV,     -GOLDEN2,   GOLDENINV2 },
  {       -SQRT2,     SQRT2INV,     -GOLDEN2,   GOLDENINV2 },
  {        SQRT2,     SQRT2INV,     -GOLDEN2,   GOLDENINV2 },
  {          0.0,   SQRT5OVER2,     -GOLDEN2,   GOLDENINV2 },
  {     -GOLDEN2,       -SQRT2,    -SQRT2INV,   GOLDENINV2 },
  {      GOLDEN2,       -SQRT2,    -SQRT2INV,   GOLDENINV2 },
  {     -GOLDEN2,        SQRT2,    -SQRT2INV,   GOLDENINV2 },
  {      GOLDEN2,        SQRT2,    -SQRT2INV,   GOLDENINV2 },
  {  -GOLDENINV2,    -GOLDEN22,  -GOLDENINV2,   GOLDENINV2 },
  {   GOLDENINV2,    -GOLDEN22,  -GOLDENINV2,   GOLDENINV2 },
  {    -GOLDEN22,  -GOLDENINV2,  -GOLDENINV2,   GOLDENINV2 },
  {     GOLDEN22,  -GOLDENINV2,  -GOLDENINV2,   GOLDENINV2 },
  {    -GOLDEN22,   GOLDENINV2,  -GOLDENINV2,   GOLDENINV2 },
  {     GOLDEN22,   GOLDENINV2,  -GOLDENINV2,   GOLDENINV2 },
  {  -GOLDENINV2,     GOLDEN22,  -GOLDENINV2,   GOLDENINV2 },
  {   GOLDENINV2,     GOLDEN22,  -GOLDENINV2,   GOLDENINV2 },
  {  -SQRT5OVER2,     -GOLDEN2,          0.0,   GOLDENINV2 },
  {   SQRT5OVER2,     -GOLDEN2,          0.0,   GOLDENINV2 },
  {  -SQRT5OVER2,      GOLDEN2,          0.0,   GOLDENINV2 },
  {   SQRT5OVER2,      GOLDEN2,          0.0,   GOLDENINV2 },
  {  -GOLDENINV2,    -GOLDEN22,   GOLDENINV2,   GOLDENINV2 },
  {   GOLDENINV2,    -GOLDEN22,   GOLDENINV2,   GOLDENINV2 },
  {    -GOLDEN22,  -GOLDENINV2,   GOLDENINV2,   GOLDENINV2 },
  {     GOLDEN22,  -GOLDENINV2,   GOLDENINV2,   GOLDENINV2 },
  {    -GOLDEN22,   GOLDENINV2,   GOLDENINV2,   GOLDENINV2 },
  {     GOLDEN22,   GOLDENINV2,   GOLDENINV2,   GOLDENINV2 },
  {  -GOLDENINV2,     GOLDEN22,   GOLDENINV2,   GOLDENINV2 },
  {   GOLDENINV2,     GOLDEN22,   GOLDENINV2,   GOLDENINV2 },
  {     -GOLDEN2,       -SQRT2,     SQRT2INV,   GOLDENINV2 },
  {      GOLDEN2,       -SQRT2,     SQRT2INV,   GOLDENINV2 },
  {     -GOLDEN2,        SQRT2,     SQRT2INV,   GOLDENINV2 },
  {      GOLDEN2,        SQRT2,     SQRT2INV,   GOLDENINV2 },
  {          0.0,  -SQRT5OVER2,      GOLDEN2,   GOLDENINV2 },
  {       -SQRT2,    -SQRT2INV,      GOLDEN2,   GOLDENINV2 },
  {        SQRT2,    -SQRT2INV,      GOLDEN2,   GOLDENINV2 },
  {       -SQRT2,     SQRT2INV,      GOLDEN2,   GOLDENINV2 },
  {        SQRT2,     SQRT2INV,      GOLDEN2,   GOLDENINV2 },
  {          0.0,   SQRT5OVER2,      GOLDEN2,   GOLDENINV2 },
  {    -SQRT2INV,     -GOLDEN2,        SQRT2,   GOLDENINV2 },
  {     SQRT2INV,     -GOLDEN2,        SQRT2,   GOLDENINV2 },
  {    -SQRT2INV,      GOLDEN2,        SQRT2,   GOLDENINV2 },
  {     SQRT2INV,      GOLDEN2,        SQRT2,   GOLDENINV2 },
  {     -GOLDEN2,          0.0,   SQRT5OVER2,   GOLDENINV2 },
  {      GOLDEN2,          0.0,   SQRT5OVER2,   GOLDENINV2 },
  {  -GOLDENINV2,  -GOLDENINV2,     GOLDEN22,   GOLDENINV2 },
  {   GOLDENINV2,  -GOLDENINV2,     GOLDEN22,   GOLDENINV2 },
  {  -GOLDENINV2,   GOLDENINV2,     GOLDEN22,   GOLDENINV2 },
  {   GOLDENINV2,   GOLDENINV2,     GOLDEN22,   GOLDENINV2 },
  {          0.0, -GOLDENINV22,    -GOLDEN22,     SQRT2INV },
  {          0.0,  GOLDENINV22,    -GOLDEN22,     SQRT2INV },
  {    -SQRT2INV,    -SQRT2INV,  -SQRT5OVER2,     SQRT2INV },
  {     SQRT2INV,    -SQRT2INV,  -SQRT5OVER2,     SQRT2INV },
  {    -SQRT2INV,     SQRT2INV,  -SQRT5OVER2,     SQRT2INV },
  {     SQRT2INV,     SQRT2INV,  -SQRT5OVER2,     SQRT2INV },
  {     -GOLDEN2,  -GOLDENINV2,       -SQRT2,     SQRT2INV },
  {      GOLDEN2,  -GOLDENINV2,       -SQRT2,     SQRT2INV },
  {     -GOLDEN2,   GOLDENINV2,       -SQRT2,     SQRT2INV },
  {      GOLDEN2,   GOLDENINV2,       -SQRT2,     SQRT2INV },
  {  -GOLDENINV2,       -SQRT2,     -GOLDEN2,     SQRT2INV },
  {   GOLDENINV2,       -SQRT2,     -GOLDEN2,     SQRT2INV },
  {  -GOLDENINV2,        SQRT2,     -GOLDEN2,     SQRT2INV },
  {   GOLDENINV2,        SQRT2,     -GOLDEN2,     SQRT2INV },
  {    -SQRT2INV,  -SQRT5OVER2,    -SQRT2INV,     SQRT2INV },
  {     SQRT2INV,  -SQRT5OVER2,    -SQRT2INV,     SQRT2INV },
  {  -SQRT5OVER2,    -SQRT2INV,    -SQRT2INV,     SQRT2INV },
  {   SQRT5OVER2,    -SQRT2INV,    -SQRT2INV,     SQRT2INV },
  {  -SQRT5OVER2,     SQRT2INV,    -SQRT2INV,     SQRT2INV },
  {   SQRT5OVER2,     SQRT2INV,    -SQRT2INV,     SQRT2INV },
  {    -SQRT2INV,   SQRT5OVER2,    -SQRT2INV,     SQRT2INV },
  {     SQRT2INV,   SQRT5OVER2,    -SQRT2INV,     SQRT2INV },
  {       -SQRT2,     -GOLDEN2,  -GOLDENINV2,     SQRT2INV },
  {        SQRT2,     -GOLDEN2,  -GOLDENINV2,     SQRT2INV },
  {       -SQRT2,      GOLDEN2,  -GOLDENINV2,     SQRT2INV },
  {        SQRT2,      GOLDEN2,  -GOLDENINV2,     SQRT2INV },
  {    -GOLDEN22,          0.0, -GOLDENINV22,     SQRT2INV },
  {     GOLDEN22,          0.0, -GOLDENINV22,     SQRT2INV },
  { -GOLDENINV22,    -GOLDEN22,          0.0,     SQRT2INV },
  {  GOLDENINV22,    -GOLDEN22,          0.0,     SQRT2INV },
  { -GOLDENINV22,     GOLDEN22,          0.0,     SQRT2INV },
  {  GOLDENINV22,     GOLDEN22,          0.0,     SQRT2INV },
  {    -GOLDEN22,          0.0,  GOLDENINV22,     SQRT2INV },
  {     GOLDEN22,          0.0,  GOLDENINV22,     SQRT2INV },
  {       -SQRT2,     -GOLDEN2,   GOLDENINV2,     SQRT2INV },
  {        SQRT2,     -GOLDEN2,   GOLDENINV2,     SQRT2INV },
  {       -SQRT2,      GOLDEN2,   GOLDENINV2,     SQRT2INV },
  {        SQRT2,      GOLDEN2,   GOLDENINV2,     SQRT2INV },
  {    -SQRT2INV,  -SQRT5OVER2,     SQRT2INV,     SQRT2INV },
  {     SQRT2INV,  -SQRT5OVER2,     SQRT2INV,     SQRT2INV },
  {  -SQRT5OVER2,    -SQRT2INV,     SQRT2INV,     SQRT2INV },
  {   SQRT5OVER2,    -SQRT2INV,     SQRT2INV,     SQRT2INV },
  {  -SQRT5OVER2,     SQRT2INV,     SQRT2INV,     SQRT2INV },
  {   SQRT5OVER2,     SQRT2INV,     SQRT2INV,     SQRT2INV },
  {    -SQRT2INV,   SQRT5OVER2,     SQRT2INV,     SQRT2INV },
  {     SQRT2INV,   SQRT5OVER2,     SQRT2INV,     SQRT2INV },
  {  -GOLDENINV2,       -SQRT2,      GOLDEN2,     SQRT2INV },
  {   GOLDENINV2,       -SQRT2,      GOLDEN2,     SQRT2INV },
  {  -GOLDENINV2,        SQRT2,      GOLDEN2,     SQRT2INV },
  {   GOLDENINV2,        SQRT2,      GOLDEN2,     SQRT2INV },
  {     -GOLDEN2,  -GOLDENINV2,        SQRT2,     SQRT2INV },
  {      GOLDEN2,  -GOLDENINV2,        SQRT2,     SQRT2INV },
  {     -GOLDEN2,   GOLDENINV2,        SQRT2,     SQRT2INV },
  {      GOLDEN2,   GOLDENINV2,        SQRT2,     SQRT2INV },
  {    -SQRT2INV,    -SQRT2INV,   SQRT5OVER2,     SQRT2INV },
  {     SQRT2INV,    -SQRT2INV,   SQRT5OVER2,     SQRT2INV },
  {    -SQRT2INV,     SQRT2INV,   SQRT5OVER2,     SQRT2INV },
  {     SQRT2INV,     SQRT2INV,   SQRT5OVER2,     SQRT2INV },
  {          0.0, -GOLDENINV22,     GOLDEN22,     SQRT2INV },
  {          0.0,  GOLDENINV22,     GOLDEN22,     SQRT2INV },
  {          0.0,  -GOLDENINV2,  -SQRT5OVER2,      GOLDEN2 },
  {          0.0,   GOLDENINV2,  -SQRT5OVER2,      GOLDEN2 },
  {  -GOLDENINV2,    -SQRT2INV,       -SQRT2,      GOLDEN2 },
  {   GOLDENINV2,    -SQRT2INV,       -SQRT2,      GOLDEN2 },
  {  -GOLDENINV2,     SQRT2INV,       -SQRT2,      GOLDEN2 },
  {   GOLDENINV2,     SQRT2INV,       -SQRT2,      GOLDEN2 },
  { -GOLDENINV22,     -GOLDEN2,     -GOLDEN2,      GOLDEN2 },
  {  GOLDENINV22,     -GOLDEN2,     -GOLDEN2,      GOLDEN2 },
  {     -GOLDEN2, -GOLDENINV22,     -GOLDEN2,      GOLDEN2 },
  {      GOLDEN2, -GOLDENINV22,     -GOLDEN2,      GOLDEN2 },
  {     -GOLDEN2,  GOLDENINV22,     -GOLDEN2,      GOLDEN2 },
  {      GOLDEN2,  GOLDENINV22,     -GOLDEN2,      GOLDEN2 },
  { -GOLDENINV22,      GOLDEN2,     -GOLDEN2,      GOLDEN2 },
  {  GOLDENINV22,      GOLDEN2,     -GOLDEN2,      GOLDEN2 },
  {       -SQRT2,  -GOLDENINV2,    -SQRT2INV,      GOLDEN2 },
  {        SQRT2,  -GOLDENINV2,    -SQRT2INV,      GOLDEN2 },
  {       -SQRT2,   GOLDENINV2,    -SQRT2INV,      GOLDEN2 },
  {        SQRT2,   GOLDENINV2,    -SQRT2INV,      GOLDEN2 },
  {    -SQRT2INV,       -SQRT2,  -GOLDENINV2,      GOLDEN2 },
  {     SQRT2INV,       -SQRT2,  -GOLDENINV2,      GOLDEN2 },
  {  -SQRT5OVER2,          0.0,  -GOLDENINV2,      GOLDEN2 },
  {   SQRT5OVER2,          0.0,  -GOLDENINV2,      GOLDEN2 },
  {    -SQRT2INV,        SQRT2,  -GOLDENINV2,      GOLDEN2 },
  {     SQRT2INV,        SQRT2,  -GOLDENINV2,      GOLDEN2 },
  {     -GOLDEN2,     -GOLDEN2, -GOLDENINV22,      GOLDEN2 },
  {      GOLDEN2,     -GOLDEN2, -GOLDENINV22,      GOLDEN2 },
  {     -GOLDEN2,      GOLDEN2, -GOLDENINV22,      GOLDEN2 },
  {      GOLDEN2,      GOLDEN2, -GOLDENINV22,      GOLDEN2 },
  {  -GOLDENINV2,  -SQRT5OVER2,          0.0,      GOLDEN2 },
  {   GOLDENINV2,  -SQRT5OVER2,          0.0,      GOLDEN2 },
  {  -GOLDENINV2,   SQRT5OVER2,          0.0,      GOLDEN2 },
  {   GOLDENINV2,   SQRT5OVER2,          0.0,      GOLDEN2 },
  {     -GOLDEN2,     -GOLDEN2,  GOLDENINV22,      GOLDEN2 },
  {      GOLDEN2,     -GOLDEN2,  GOLDENINV22,      GOLDEN2 },
  {     -GOLDEN2,      GOLDEN2,  GOLDENINV22,      GOLDEN2 },
  {      GOLDEN2,      GOLDEN2,  GOLDENINV22,      GOLDEN2 },
  {    -SQRT2INV,       -SQRT2,   GOLDENINV2,      GOLDEN2 },
  {     SQRT2INV,       -SQRT2,   GOLDENINV2,      GOLDEN2 },
  {  -SQRT5OVER2,          0.0,   GOLDENINV2,      GOLDEN2 },
  {   SQRT5OVER2,          0.0,   GOLDENINV2,      GOLDEN2 },
  {    -SQRT2INV,        SQRT2,   GOLDENINV2,      GOLDEN2 },
  {     SQRT2INV,        SQRT2,   GOLDENINV2,      GOLDEN2 },
  {       -SQRT2,  -GOLDENINV2,     SQRT2INV,      GOLDEN2 },
  {        SQRT2,  -GOLDENINV2,     SQRT2INV,      GOLDEN2 },
  {       -SQRT2,   GOLDENINV2,     SQRT2INV,      GOLDEN2 },
  {        SQRT2,   GOLDENINV2,     SQRT2INV,      GOLDEN2 },
  { -GOLDENINV22,     -GOLDEN2,      GOLDEN2,      GOLDEN2 },
  {  GOLDENINV22,     -GOLDEN2,      GOLDEN2,      GOLDEN2 },
  {     -GOLDEN2, -GOLDENINV22,      GOLDEN2,      GOLDEN2 },
  {      GOLDEN2, -GOLDENINV22,      GOLDEN2,      GOLDEN2 },
  {     -GOLDEN2,  GOLDENINV22,      GOLDEN2,      GOLDEN2 },
  {      GOLDEN2,  GOLDENINV22,      GOLDEN2,      GOLDEN2 },
  { -GOLDENINV22,      GOLDEN2,      GOLDEN2,      GOLDEN2 },
  {  GOLDENINV22,      GOLDEN2,      GOLDEN2,      GOLDEN2 },
  {  -GOLDENINV2,    -SQRT2INV,        SQRT2,      GOLDEN2 },
  {   GOLDENINV2,    -SQRT2INV,        SQRT2,      GOLDEN2 },
  {  -GOLDENINV2,     SQRT2INV,        SQRT2,      GOLDEN2 },
  {   GOLDENINV2,     SQRT2INV,        SQRT2,      GOLDEN2 },
  {          0.0,  -GOLDENINV2,   SQRT5OVER2,      GOLDEN2 },
  {          0.0,   GOLDENINV2,   SQRT5OVER2,      GOLDEN2 },
  {          0.0,          0.0,       -SQRT2,        SQRT2 },
  {    -SQRT2INV,  -GOLDENINV2,     -GOLDEN2,        SQRT2 },
  {     SQRT2INV,  -GOLDENINV2,     -GOLDEN2,        SQRT2 },
  {    -SQRT2INV,   GOLDENINV2,     -GOLDEN2,        SQRT2 },
  {     SQRT2INV,   GOLDENINV2,     -GOLDEN2,        SQRT2 },
  {  -GOLDENINV2,     -GOLDEN2,    -SQRT2INV,        SQRT2 },
  {   GOLDENINV2,     -GOLDEN2,    -SQRT2INV,        SQRT2 },
  {  -GOLDENINV2,      GOLDEN2,    -SQRT2INV,        SQRT2 },
  {   GOLDENINV2,      GOLDEN2,    -SQRT2INV,        SQRT2 },
  {     -GOLDEN2,    -SQRT2INV,  -GOLDENINV2,        SQRT2 },
  {      GOLDEN2,    -SQRT2INV,  -GOLDENINV2,        SQRT2 },
  {     -GOLDEN2,     SQRT2INV,  -GOLDENINV2,        SQRT2 },
  {      GOLDEN2,     SQRT2INV,  -GOLDENINV2,        SQRT2 },
  {          0.0,       -SQRT2,          0.0,        SQRT2 },
  {       -SQRT2,          0.0,          0.0,        SQRT2 },
  {        SQRT2,          0.0,          0.0,        SQRT2 },
  {          0.0,        SQRT2,          0.0,        SQRT2 },
  {     -GOLDEN2,    -SQRT2INV,   GOLDENINV2,        SQRT2 },
  {      GOLDEN2,    -SQRT2INV,   GOLDENINV2,        SQRT2 },
  {     -GOLDEN2,     SQRT2INV,   GOLDENINV2,        SQRT2 },
  {      GOLDEN2,     SQRT2INV,   GOLDENINV2,        SQRT2 },
  {  -GOLDENINV2,     -GOLDEN2,     SQRT2INV,        SQRT2 },
  {   GOLDENINV2,     -GOLDEN2,     SQRT2INV,        SQRT2 },
  {  -GOLDENINV2,      GOLDEN2,     SQRT2INV,        SQRT2 },
  {   GOLDENINV2,      GOLDEN2,     SQRT2INV,        SQRT2 },
  {    -SQRT2INV,  -GOLDENINV2,      GOLDEN2,        SQRT2 },
  {     SQRT2INV,  -GOLDENINV2,      GOLDEN2,        SQRT2 },
  {    -SQRT2INV,   GOLDENINV2,      GOLDEN2,        SQRT2 },
  {     SQRT2INV,   GOLDENINV2,      GOLDEN2,        SQRT2 },
  {          0.0,          0.0,        SQRT2,        SQRT2 },
  {  -GOLDENINV2,          0.0,     -GOLDEN2,   SQRT5OVER2 },
  {   GOLDENINV2,          0.0,     -GOLDEN2,   SQRT5OVER2 },
  {    -SQRT2INV,    -SQRT2INV,    -SQRT2INV,   SQRT5OVER2 },
  {     SQRT2INV,    -SQRT2INV,    -SQRT2INV,   SQRT5OVER2 },
  {    -SQRT2INV,     SQRT2INV,    -SQRT2INV,   SQRT5OVER2 },
  {     SQRT2INV,     SQRT2INV,    -SQRT2INV,   SQRT5OVER2 },
  {          0.0,     -GOLDEN2,  -GOLDENINV2,   SQRT5OVER2 },
  {          0.0,      GOLDEN2,  -GOLDENINV2,   SQRT5OVER2 },
  {     -GOLDEN2,  -GOLDENINV2,          0.0,   SQRT5OVER2 },
  {      GOLDEN2,  -GOLDENINV2,          0.0,   SQRT5OVER2 },
  {     -GOLDEN2,   GOLDENINV2,          0.0,   SQRT5OVER2 },
  {      GOLDEN2,   GOLDENINV2,          0.0,   SQRT5OVER2 },
  {          0.0,     -GOLDEN2,   GOLDENINV2,   SQRT5OVER2 },
  {          0.0,      GOLDEN2,   GOLDENINV2,   SQRT5OVER2 },
  {    -SQRT2INV,    -SQRT2INV,     SQRT2INV,   SQRT5OVER2 },
  {     SQRT2INV,    -SQRT2INV,     SQRT2INV,   SQRT5OVER2 },
  {    -SQRT2INV,     SQRT2INV,     SQRT2INV,   SQRT5OVER2 },
  {     SQRT2INV,     SQRT2INV,     SQRT2INV,   SQRT5OVER2 },
  {  -GOLDENINV2,          0.0,      GOLDEN2,   SQRT5OVER2 },
  {   GOLDENINV2,          0.0,      GOLDEN2,   SQRT5OVER2 },
  { -GOLDENINV22,          0.0,    -SQRT2INV,     GOLDEN22 },
  {  GOLDENINV22,          0.0,    -SQRT2INV,     GOLDEN22 },
  {  -GOLDENINV2,  -GOLDENINV2,  -GOLDENINV2,     GOLDEN22 },
  {   GOLDENINV2,  -GOLDENINV2,  -GOLDENINV2,     GOLDEN22 },
  {  -GOLDENINV2,   GOLDENINV2,  -GOLDENINV2,     GOLDEN22 },
  {   GOLDENINV2,   GOLDENINV2,  -GOLDENINV2,     GOLDEN22 },
  {          0.0,    -SQRT2INV, -GOLDENINV22,     GOLDEN22 },
  {          0.0,     SQRT2INV, -GOLDENINV22,     GOLDEN22 },
  {    -SQRT2INV, -GOLDENINV22,          0.0,     GOLDEN22 },
  {     SQRT2INV, -GOLDENINV22,          0.0,     GOLDEN22 },
  {    -SQRT2INV,  GOLDENINV22,          0.0,     GOLDEN22 },
  {     SQRT2INV,  GOLDENINV22,          0.0,     GOLDEN22 },
  {          0.0,    -SQRT2INV,  GOLDENINV22,     GOLDEN22 },
  {          0.0,     SQRT2INV,  GOLDENINV22,     GOLDEN22 },
  {  -GOLDENINV2,  -GOLDENINV2,   GOLDENINV2,     GOLDEN22 },
  {   GOLDENINV2,  -GOLDENINV2,   GOLDENINV2,     GOLDEN22 },
  {  -GOLDENINV2,   GOLDENINV2,   GOLDENINV2,     GOLDEN22 },
  {   GOLDENINV2,   GOLDENINV2,   GOLDENINV2,     GOLDEN22 },
  { -GOLDENINV22,          0.0,     SQRT2INV,     GOLDEN22 },
  {  GOLDENINV22,          0.0,     SQRT2INV,     GOLDEN22 }
};

static const int edge_120[NUM_EDGE_120][2] = {
  {   0,   1 }, {   0,   2 }, {   0,   4 }, {   0,  20 }, {   1,   3 },
  {   1,   5 }, {   1,  21 }, {   2,   6 }, {   2,   8 }, {   2,  22 },
  {   3,   6 }, {   3,   9 }, {   3,  23 }, {   4,   7 }, {   4,  10 },
  {   4,  24 }, {   5,   7 }, {   5,  11 }, {   5,  25 }, {   6,  12 },
  {   6,  26 }, {   7,  13 }, {   7,  27 }, {   8,  10 }, {   8,  14 },
  {   8,  28 }, {   9,  11 }, {   9,  15 }, {   9,  29 }, {  10,  16 },
  {  10,  30 }, {  11,  17 }, {  11,  31 }, {  12,  14 }, {  12,  15 },
  {  12,  32 }, {  13,  16 }, {  13,  17 }, {  13,  33 }, {  14,  18 },
  {  14,  34 }, {  15,  19 }, {  15,  35 }, {  16,  18 }, {  16,  36 },
  {  17,  19 }, {  17,  37 }, {  18,  19 }, {  18,  38 }, {  19,  39 },
  {  20,  40 }, {  20,  41 }, {  20,  43 }, {  21,  40 }, {  21,  42 },
  {  21,  44 }, {  22,  41 }, {  22,  45 }, {  22,  49 }, {  23,  42 },
  {  23,  46 }, {  23,  50 }, {  24,  43 }, {  24,  47 }, {  24,  51 },
  {  25,  44 }, {  25,  48 }, {  25,  52 }, {  26,  45 }, {  26,  46 },
  {  26,  53 }, {  27,  47 }, {  27,  48 }, {  27,  56 }, {  28,  49 },
  {  28,  54 }, {  28,  57 }, {  29,  50 }, {  29,  55 }, {  29,  58 },
  {  30,  51 }, {  30,  54 }, {  30,  59 }, {  31,  52 }, {  31,  55 },
  {  31,  60 }, {  32,  53 }, {  32,  61 }, {  32,  62 }, {  33,  56 },
  {  33,  63 }, {  33,  64 }, {  34,  57 }, {  34,  61 }, {  34,  65 },
  {  35,  58 }, {  35,  62 }, {  35,  66 }, {  36,  59 }, {  36,  63 },
  {  36,  67 }, {  37,  60 }, {  37,  64 }, {  37,  68 }, {  38,  65 },
  {  38,  67 }, {  38,  69 }, {  39,  66 }, {  39,  68 }, {  39,  69 },
  {  40,  70 }, {  40,  71 }, {  41,  72 }, {  41,  78 }, {  42,  73 },
  {  42,  79 }, {  43,  74 }, {  43,  80 }, {  44,  75 }, {  44,  81 },
  {  45,  76 }, {  45,  88 }, {  46,  77 }, {  46,  89 }, {  47,  82 },
  {  47,  92 }, {  48,  83 }, {  48,  93 }, {  49,  84 }, {  49,  94 },
  {  50,  85 }, {  50,  95 }, {  51,  86 }, {  51,  96 }, {  52,  87 },
  {  52,  97 }, {  53,  98 }, {  53,  99 }, {  54,  90 }, {  54, 108 },
  {  55,  91 }, {  55, 109 }, {  56, 100 }, {  56, 101 }, {  57, 102 },
  {  57, 112 }, {  58, 103 }, {  58, 113 }, {  59, 104 }, {  59, 114 },
  {  60, 105 }, {  60, 115 }, {  61, 106 }, {  61, 116 }, {  62, 107 },
  {  62, 117 }, {  63, 110 }, {  63, 122 }, {  64, 111 }, {  64, 123 },
  {  65, 118 }, {  65, 124 }, {  66, 119 }, {  66, 125 }, {  67, 120 },
  {  67, 126 }, {  68, 121 }, {  68, 127 }, {  69, 128 }, {  69, 129 },
  {  70,  72 }, {  70,  73 }, {  70, 130 }, {  71,  74 }, {  71,  75 },
  {  71, 131 }, {  72,  76 }, {  72, 132 }, {  73,  77 }, {  73, 133 },
  {  74,  82 }, {  74, 134 }, {  75,  83 }, {  75, 135 }, {  76,  77 },
  {  76, 140 }, {  77, 141 }, {  78,  80 }, {  78,  84 }, {  78, 136 },
  {  79,  81 }, {  79,  85 }, {  79, 137 }, {  80,  86 }, {  80, 138 },
  {  81,  87 }, {  81, 139 }, {  82,  83 }, {  82, 142 }, {  83, 143 },
  {  84,  90 }, {  84, 146 }, {  85,  91 }, {  85, 147 }, {  86,  90 },
  {  86, 148 }, {  87,  91 }, {  87, 149 }, {  88,  94 }, {  88,  98 },
  {  88, 144 }, {  89,  95 }, {  89,  99 }, {  89, 145 }, {  90, 156 },
  {  91, 157 }, {  92,  96 }, {  92, 100 }, {  92, 150 }, {  93,  97 },
  {  93, 101 }, {  93, 151 }, {  94, 102 }, {  94, 152 }, {  95, 103 },
  {  95, 153 }, {  96, 104 }, {  96, 154 }, {  97, 105 }, {  97, 155 },
  {  98, 106 }, {  98, 158 }, {  99, 107 }, {  99, 159 }, { 100, 110 },
  { 100, 160 }, { 101, 111 }, { 101, 161 }, { 102, 106 }, { 102, 164 },
  { 103, 107 }, { 103, 165 }, { 104, 110 }, { 104, 166 }, { 105, 111 },
  { 105, 167 }, { 106, 168 }, { 107, 169 }, { 108, 112 }, { 108, 114 },
  { 108, 162 }, { 109, 113 }, { 109, 115 }, { 109, 163 }, { 110, 174 },
  { 111, 175 }, { 112, 118 }, { 112, 170 }, { 113, 119 }, { 113, 171 },
  { 114, 120 }, { 114, 172 }, { 115, 121 }, { 115, 173 }, { 116, 117 },
  { 116, 124 }, { 116, 176 }, { 117, 125 }, { 117, 177 }, { 118, 120 },
  { 118, 180 }, { 119, 121 }, { 119, 181 }, { 120, 182 }, { 121, 183 },
  { 122, 123 }, { 122, 126 }, { 122, 178 }, { 123, 127 }, { 123, 179 },
  { 124, 128 }, { 124, 184 }, { 125, 128 }, { 125, 185 }, { 126, 129 },
  { 126, 186 }, { 127, 129 }, { 127, 187 }, { 128, 188 }, { 129, 189 },
  { 130, 131 }, { 130, 190 }, { 130, 191 }, { 131, 192 }, { 131, 193 },
  { 132, 136 }, { 132, 190 }, { 132, 196 }, { 133, 137 }, { 133, 191 },
  { 133, 197 }, { 134, 138 }, { 134, 192 }, { 134, 198 }, { 135, 139 },
  { 135, 193 }, { 135, 199 }, { 136, 194 }, { 136, 201 }, { 137, 195 },
  { 137, 202 }, { 138, 194 }, { 138, 203 }, { 139, 195 }, { 139, 204 },
  { 140, 144 }, { 140, 196 }, { 140, 200 }, { 141, 145 }, { 141, 197 },
  { 141, 200 }, { 142, 150 }, { 142, 198 }, { 142, 205 }, { 143, 151 },
  { 143, 199 }, { 143, 205 }, { 144, 206 }, { 144, 210 }, { 145, 207 },
  { 145, 211 }, { 146, 152 }, { 146, 201 }, { 146, 212 }, { 147, 153 },
  { 147, 202 }, { 147, 213 }, { 148, 154 }, { 148, 203 }, { 148, 214 },
  { 149, 155 }, { 149, 204 }, { 149, 215 }, { 150, 208 }, { 150, 216 },
  { 151, 209 }, { 151, 217 }, { 152, 206 }, { 152, 218 }, { 153, 207 },
  { 153, 219 }, { 154, 208 }, { 154, 220 }, { 155, 209 }, { 155, 221 },
  { 156, 162 }, { 156, 212 }, { 156, 214 }, { 157, 163 }, { 157, 213 },
  { 157, 215 }, { 158, 159 }, { 158, 210 }, { 158, 222 }, { 159, 211 },
  { 159, 223 }, { 160, 161 }, { 160, 216 }, { 160, 228 }, { 161, 217 },
  { 161, 229 }, { 162, 224 }, { 162, 226 }, { 163, 225 }, { 163, 227 },
  { 164, 170 }, { 164, 218 }, { 164, 230 }, { 165, 171 }, { 165, 219 },
  { 165, 231 }, { 166, 172 }, { 166, 220 }, { 166, 232 }, { 167, 173 },
  { 167, 221 }, { 167, 233 }, { 168, 176 }, { 168, 222 }, { 168, 230 },
  { 169, 177 }, { 169, 223 }, { 169, 231 }, { 170, 224 }, { 170, 235 },
  { 171, 225 }, { 171, 236 }, { 172, 226 }, { 172, 237 }, { 173, 227 },
  { 173, 238 }, { 174, 178 }, { 174, 228 }, { 174, 232 }, { 175, 179 },
  { 175, 229 }, { 175, 233 }, { 176, 234 }, { 176, 240 }, { 177, 234 },
  { 177, 241 }, { 178, 239 }, { 178, 242 }, { 179, 239 }, { 179, 243 },
  { 180, 184 }, { 180, 235 }, { 180, 244 }, { 181, 185 }, { 181, 236 },
  { 181, 245 }, { 182, 186 }, { 182, 237 }, { 182, 244 }, { 183, 187 },
  { 183, 238 }, { 183, 245 }, { 184, 240 }, { 184, 246 }, { 185, 241 },
  { 185, 247 }, { 186, 242 }, { 186, 248 }, { 187, 243 }, { 187, 249 },
  { 188, 189 }, { 188, 246 }, { 188, 247 }, { 189, 248 }, { 189, 249 },
  { 190, 250 }, { 190, 270 }, { 191, 251 }, { 191, 271 }, { 192, 250 },
  { 192, 272 }, { 193, 251 }, { 193, 273 }, { 194, 250 }, { 194, 279 },
  { 195, 251 }, { 195, 280 }, { 196, 252 }, { 196, 274 }, { 197, 253 },
  { 197, 275 }, { 198, 254 }, { 198, 276 }, { 199, 255 }, { 199, 277 },
  { 200, 256 }, { 200, 278 }, { 201, 252 }, { 201, 282 }, { 202, 253 },
  { 202, 283 }, { 203, 254 }, { 203, 284 }, { 204, 255 }, { 204, 285 },
  { 205, 257 }, { 205, 281 }, { 206, 252 }, { 206, 290 }, { 207, 253 },
  { 207, 291 }, { 208, 254 }, { 208, 292 }, { 209, 255 }, { 209, 293 },
  { 210, 256 }, { 210, 294 }, { 211, 256 }, { 211, 295 }, { 212, 258 },
  { 212, 286 }, { 213, 259 }, { 213, 287 }, { 214, 260 }, { 214, 288 },
  { 215, 261 }, { 215, 289 }, { 216, 257 }, { 216, 296 }, { 217, 257 },
  { 217, 297 }, { 218, 258 }, { 218, 298 }, { 219, 259 }, { 219, 299 },
  { 220, 260 }, { 220, 300 }, { 221, 261 }, { 221, 301 }, { 222, 262 },
  { 222, 302 }, { 223, 262 }, { 223, 303 }, { 224, 258 }, { 224, 310 },
  { 225, 259 }, { 225, 311 }, { 226, 260 }, { 226, 312 }, { 227, 261 },
  { 227, 313 }, { 228, 263 }, { 228, 304 }, { 229, 263 }, { 229, 305 },
  { 230, 264 }, { 230, 306 }, { 231, 265 }, { 231, 307 }, { 232, 266 },
  { 232, 308 }, { 233, 267 }, { 233, 309 }, { 234, 262 }, { 234, 318 },
  { 235, 264 }, { 235, 314 }, { 236, 265 }, { 236, 315 }, { 237, 266 },
  { 237, 316 }, { 238, 267 }, { 238, 317 }, { 239, 263 }, { 239, 321 },
  { 240, 264 }, { 240, 322 }, { 241, 265 }, { 241, 323 }, { 242, 266 },
  { 242, 324 }, { 243, 267 }, { 243, 325 }, { 244, 268 }, { 244, 319 },
  { 245, 269 }, { 245, 320 }, { 246, 268 }, { 246, 326 }, { 247, 269 },
  { 247, 327 }, { 248, 268 }, { 248, 328 }, { 249, 269 }, { 249, 329 },
  { 250, 330 }, { 251, 331 }, { 252, 332 }, { 253, 333 }, { 254, 334 },
  { 255, 335 }, { 256, 336 }, { 257, 337 }, { 258, 338 }, { 259, 339 },
  { 260, 340 }, { 261, 341 }, { 262, 342 }, { 263, 343 }, { 264, 344 },
  { 265, 345 }, { 266, 346 }, { 267, 347 }, { 268, 348 }, { 269, 349 },
  { 270, 271 }, { 270, 274 }, { 270, 350 }, { 271, 275 }, { 271, 351 },
  { 272, 273 }, { 272, 276 }, { 272, 352 }, { 273, 277 }, { 273, 353 },
  { 274, 278 }, { 274, 356 }, { 275, 278 }, { 275, 357 }, { 276, 281 },
  { 276, 358 }, { 277, 281 }, { 277, 359 }, { 278, 360 }, { 279, 282 },
  { 279, 284 }, { 279, 354 }, { 280, 283 }, { 280, 285 }, { 280, 355 },
  { 281, 365 }, { 282, 286 }, { 282, 361 }, { 283, 287 }, { 283, 362 },
  { 284, 288 }, { 284, 363 }, { 285, 289 }, { 285, 364 }, { 286, 288 },
  { 286, 372 }, { 287, 289 }, { 287, 373 }, { 288, 374 }, { 289, 375 },
  { 290, 294 }, { 290, 298 }, { 290, 366 }, { 291, 295 }, { 291, 299 },
  { 291, 367 }, { 292, 296 }, { 292, 300 }, { 292, 368 }, { 293, 297 },
  { 293, 301 }, { 293, 369 }, { 294, 302 }, { 294, 370 }, { 295, 303 },
  { 295, 371 }, { 296, 304 }, { 296, 376 }, { 297, 305 }, { 297, 377 },
  { 298, 306 }, { 298, 378 }, { 299, 307 }, { 299, 379 }, { 300, 308 },
  { 300, 380 }, { 301, 309 }, { 301, 381 }, { 302, 306 }, { 302, 382 },
  { 303, 307 }, { 303, 383 }, { 304, 308 }, { 304, 388 }, { 305, 309 },
  { 305, 389 }, { 306, 390 }, { 307, 391 }, { 308, 392 }, { 309, 393 },
  { 310, 312 }, { 310, 314 }, { 310, 384 }, { 311, 313 }, { 311, 315 },
  { 311, 385 }, { 312, 316 }, { 312, 386 }, { 313, 317 }, { 313, 387 },
  { 314, 319 }, { 314, 395 }, { 315, 320 }, { 315, 396 }, { 316, 319 },
  { 316, 397 }, { 317, 320 }, { 317, 398 }, { 318, 322 }, { 318, 323 },
  { 318, 394 }, { 319, 404 }, { 320, 405 }, { 321, 324 }, { 321, 325 },
  { 321, 399 }, { 322, 326 }, { 322, 400 }, { 323, 327 }, { 323, 401 },
  { 324, 328 }, { 324, 402 }, { 325, 329 }, { 325, 403 }, { 326, 327 },
  { 326, 406 }, { 327, 407 }, { 328, 329 }, { 328, 408 }, { 329, 409 },
  { 330, 350 }, { 330, 352 }, { 330, 354 }, { 331, 351 }, { 331, 353 },
  { 331, 355 }, { 332, 356 }, { 332, 361 }, { 332, 366 }, { 333, 357 },
  { 333, 362 }, { 333, 367 }, { 334, 358 }, { 334, 363 }, { 334, 368 },
  { 335, 359 }, { 335, 364 }, { 335, 369 }, { 336, 360 }, { 336, 370 },
  { 336, 371 }, { 337, 365 }, { 337, 376 }, { 337, 377 }, { 338, 372 },
  { 338, 378 }, { 338, 384 }, { 339, 373 }, { 339, 379 }, { 339, 385 },
  { 340, 374 }, { 340, 380 }, { 340, 386 }, { 341, 375 }, { 341, 381 },
  { 341, 387 }, { 342, 382 }, { 342, 383 }, { 342, 394 }, { 343, 388 },
  { 343, 389 }, { 343, 399 }, { 344, 390 }, { 344, 395 }, { 344, 400 },
  { 345, 391 }, { 345, 396 }, { 345, 401 }, { 346, 392 }, { 346, 397 },
  { 346, 402 }, { 347, 393 }, { 347, 398 }, { 347, 403 }, { 348, 404 },
  { 348, 406 }, { 348, 408 }, { 349, 405 }, { 349, 407 }, { 349, 409 },
  { 350, 410 }, { 350, 412 }, { 351, 410 }, { 351, 413 }, { 352, 411 },
  { 352, 414 }, { 353, 411 }, { 353, 415 }, { 354, 416 }, { 354, 418 },
  { 355, 417 }, { 355, 419 }, { 356, 412 }, { 356, 420 }, { 357, 413 },
  { 357, 421 }, { 358, 414 }, { 358, 422 }, { 359, 415 }, { 359, 423 },
  { 360, 420 }, { 360, 421 }, { 361, 416 }, { 361, 426 }, { 362, 417 },
  { 362, 427 }, { 363, 418 }, { 363, 428 }, { 364, 419 }, { 364, 429 },
  { 365, 422 }, { 365, 423 }, { 366, 424 }, { 366, 432 }, { 367, 425 },
  { 367, 433 }, { 368, 430 }, { 368, 434 }, { 369, 431 }, { 369, 435 },
  { 370, 424 }, { 370, 438 }, { 371, 425 }, { 371, 439 }, { 372, 426 },
  { 372, 436 }, { 373, 427 }, { 373, 437 }, { 374, 428 }, { 374, 436 },
  { 375, 429 }, { 375, 437 }, { 376, 430 }, { 376, 440 }, { 377, 431 },
  { 377, 441 }, { 378, 432 }, { 378, 444 }, { 379, 433 }, { 379, 445 },
  { 380, 434 }, { 380, 446 }, { 381, 435 }, { 381, 447 }, { 382, 438 },
  { 382, 448 }, { 383, 439 }, { 383, 449 }, { 384, 442 }, { 384, 450 },
  { 385, 443 }, { 385, 451 }, { 386, 442 }, { 386, 452 }, { 387, 443 },
  { 387, 453 }, { 388, 440 }, { 388, 454 }, { 389, 441 }, { 389, 455 },
  { 390, 444 }, { 390, 448 }, { 391, 445 }, { 391, 449 }, { 392, 446 },
  { 392, 454 }, { 393, 447 }, { 393, 455 }, { 394, 456 }, { 394, 457 },
  { 395, 450 }, { 395, 460 }, { 396, 451 }, { 396, 461 }, { 397, 452 },
  { 397, 462 }, { 398, 453 }, { 398, 463 }, { 399, 458 }, { 399, 459 },
  { 400, 456 }, { 400, 464 }, { 401, 457 }, { 401, 465 }, { 402, 458 },
  { 402, 466 }, { 403, 459 }, { 403, 467 }, { 404, 460 }, { 404, 462 },
  { 405, 461 }, { 405, 463 }, { 406, 464 }, { 406, 468 }, { 407, 465 },
  { 407, 468 }, { 408, 466 }, { 408, 469 }, { 409, 467 }, { 409, 469 },
  { 410, 411 }, { 410, 470 }, { 411, 471 }, { 412, 416 }, { 412, 472 },
  { 413, 417 }, { 413, 473 }, { 414, 418 }, { 414, 474 }, { 415, 419 },
  { 415, 475 }, { 416, 478 }, { 417, 479 }, { 418, 480 }, { 419, 481 },
  { 420, 424 }, { 420, 476 }, { 421, 425 }, { 421, 477 }, { 422, 430 },
  { 422, 482 }, { 423, 431 }, { 423, 483 }, { 424, 488 }, { 425, 489 },
  { 426, 432 }, { 426, 484 }, { 427, 433 }, { 427, 485 }, { 428, 434 },
  { 428, 486 }, { 429, 435 }, { 429, 487 }, { 430, 492 }, { 431, 493 },
  { 432, 494 }, { 433, 495 }, { 434, 496 }, { 435, 497 }, { 436, 442 },
  { 436, 490 }, { 437, 443 }, { 437, 491 }, { 438, 439 }, { 438, 498 },
  { 439, 499 }, { 440, 441 }, { 440, 500 }, { 441, 501 }, { 442, 508 },
  { 443, 509 }, { 444, 450 }, { 444, 502 }, { 445, 451 }, { 445, 503 },
  { 446, 452 }, { 446, 504 }, { 447, 453 }, { 447, 505 }, { 448, 456 },
  { 448, 506 }, { 449, 457 }, { 449, 507 }, { 450, 512 }, { 451, 513 },
  { 452, 514 }, { 453, 515 }, { 454, 458 }, { 454, 510 }, { 455, 459 },
  { 455, 511 }, { 456, 516 }, { 457, 517 }, { 458, 522 }, { 459, 523 },
  { 460, 464 }, { 460, 518 }, { 461, 465 }, { 461, 519 }, { 462, 466 },
  { 462, 520 }, { 463, 467 }, { 463, 521 }, { 464, 524 }, { 465, 525 },
  { 466, 526 }, { 467, 527 }, { 468, 469 }, { 468, 528 }, { 469, 529 },
  { 470, 472 }, { 470, 473 }, { 470, 530 }, { 471, 474 }, { 471, 475 },
  { 471, 530 }, { 472, 476 }, { 472, 531 }, { 473, 477 }, { 473, 532 },
  { 474, 482 }, { 474, 533 }, { 475, 483 }, { 475, 534 }, { 476, 477 },
  { 476, 535 }, { 477, 536 }, { 478, 480 }, { 478, 484 }, { 478, 531 },
  { 479, 481 }, { 479, 485 }, { 479, 532 }, { 480, 486 }, { 480, 533 },
  { 481, 487 }, { 481, 534 }, { 482, 483 }, { 482, 537 }, { 483, 538 },
  { 484, 490 }, { 484, 539 }, { 485, 491 }, { 485, 540 }, { 486, 490 },
  { 486, 541 }, { 487, 491 }, { 487, 542 }, { 488, 494 }, { 488, 498 },
  { 488, 535 }, { 489, 495 }, { 489, 499 }, { 489, 536 }, { 490, 544 },
  { 491, 545 }, { 492, 496 }, { 492, 500 }, { 492, 537 }, { 493, 497 },
  { 493, 501 }, { 493, 538 }, { 494, 502 }, { 494, 539 }, { 495, 503 },
  { 495, 540 }, { 496, 504 }, { 496, 541 }, { 497, 505 }, { 497, 542 },
  { 498, 506 }, { 498, 543 }, { 499, 507 }, { 499, 543 }, { 500, 510 },
  { 500, 546 }, { 501, 511 }, { 501, 546 }, { 502, 506 }, { 502, 547 },
  { 503, 507 }, { 503, 548 }, { 504, 510 }, { 504, 549 }, { 505, 511 },
  { 505, 550 }, { 506, 551 }, { 507, 552 }, { 508, 512 }, { 508, 514 },
  { 508, 544 }, { 509, 513 }, { 509, 515 }, { 509, 545 }, { 510, 553 },
  { 511, 554 }, { 512, 518 }, { 512, 547 }, { 513, 519 }, { 513, 548 },
  { 514, 520 }, { 514, 549 }, { 515, 521 }, { 515, 550 }, { 516, 517 },
  { 516, 524 }, { 516, 551 }, { 517, 525 }, { 517, 552 }, { 518, 520 },
  { 518, 555 }, { 519, 521 }, { 519, 556 }, { 520, 557 }, { 521, 558 },
  { 522, 523 }, { 522, 526 }, { 522, 553 }, { 523, 527 }, { 523, 554 },
  { 524, 528 }, { 524, 555 }, { 525, 528 }, { 525, 556 }, { 526, 529 },
  { 526, 557 }, { 527, 529 }, { 527, 558 }, { 528, 559 }, { 529, 559 },
  { 530, 560 }, { 530, 561 }, { 531, 560 }, { 531, 562 }, { 532, 561 },
  { 532, 563 }, { 533, 560 }, { 533, 564 }, { 534, 561 }, { 534, 565 },
  { 535, 562 }, { 535, 566 }, { 536, 563 }, { 536, 566 }, { 537, 564 },
  { 537, 567 }, { 538, 565 }, { 538, 567 }, { 539, 562 }, { 539, 568 },
  { 540, 563 }, { 540, 569 }, { 541, 564 }, { 541, 570 }, { 542, 565 },
  { 542, 571 }, { 543, 566 }, { 543, 572 }, { 544, 568 }, { 544, 570 },
  { 545, 569 }, { 545, 571 }, { 546, 567 }, { 546, 573 }, { 547, 568 },
  { 547, 574 }, { 548, 569 }, { 548, 575 }, { 549, 570 }, { 549, 576 },
  { 550, 571 }, { 550, 577 }, { 551, 572 }, { 551, 574 }, { 552, 572 },
  { 552, 575 }, { 553, 573 }, { 553, 576 }, { 554, 573 }, { 554, 577 },
  { 555, 574 }, { 555, 578 }, { 556, 575 }, { 556, 579 }, { 557, 576 },
  { 557, 578 }, { 558, 577 }, { 558, 579 }, { 559, 578 }, { 559, 579 },
  { 560, 580 }, { 561, 581 }, { 562, 582 }, { 563, 583 }, { 564, 584 },
  { 565, 585 }, { 566, 586 }, { 567, 587 }, { 568, 588 }, { 569, 589 },
  { 570, 590 }, { 571, 591 }, { 572, 592 }, { 573, 593 }, { 574, 594 },
  { 575, 595 }, { 576, 596 }, { 577, 597 }, { 578, 598 }, { 579, 599 },
  { 580, 581 }, { 580, 582 }, { 580, 584 }, { 581, 583 }, { 581, 585 },
  { 582, 586 }, { 582, 588 }, { 583, 586 }, { 583, 589 }, { 584, 587 },
  { 584, 590 }, { 585, 587 }, { 585, 591 }, { 586, 592 }, { 587, 593 },
  { 588, 590 }, { 588, 594 }, { 589, 591 }, { 589, 595 }, { 590, 596 },
  { 591, 597 }, { 592, 594 }, { 592, 595 }, { 593, 596 }, { 593, 597 },
  { 594, 598 }, { 595, 599 }, { 596, 598 }, { 597, 599 }, { 598, 599 }
};

static const int face_120[NUM_FACE_120][VERT_PER_FACE_120] = {
  {   0,   1,   3,   6,   2 }, {   0,   1,   5,   7,   4 },
  {   0,   1,  21,  40,  20 }, {   0,   2,   8,  10,   4 },
  {   0,   2,  22,  41,  20 }, {   0,   4,  24,  43,  20 },
  {   1,   3,   9,  11,   5 }, {   1,   3,  23,  42,  21 },
  {   1,   5,  25,  44,  21 }, {   2,   6,  12,  14,   8 },
  {   2,   6,  26,  45,  22 }, {   2,   8,  28,  49,  22 },
  {   3,   6,  12,  15,   9 }, {   3,   6,  26,  46,  23 },
  {   3,   9,  29,  50,  23 }, {   4,   7,  13,  16,  10 },
  {   4,   7,  27,  47,  24 }, {   4,  10,  30,  51,  24 },
  {   5,   7,  13,  17,  11 }, {   5,   7,  27,  48,  25 },
  {   5,  11,  31,  52,  25 }, {   6,  12,  32,  53,  26 },
  {   7,  13,  33,  56,  27 }, {   8,  10,  16,  18,  14 },
  {   8,  10,  30,  54,  28 }, {   8,  14,  34,  57,  28 },
  {   9,  11,  17,  19,  15 }, {   9,  11,  31,  55,  29 },
  {   9,  15,  35,  58,  29 }, {  10,  16,  36,  59,  30 },
  {  11,  17,  37,  60,  31 }, {  12,  14,  18,  19,  15 },
  {  12,  14,  34,  61,  32 }, {  12,  15,  35,  62,  32 },
  {  13,  16,  18,  19,  17 }, {  13,  16,  36,  63,  33 },
  {  13,  17,  37,  64,  33 }, {  14,  18,  38,  65,  34 },
  {  15,  19,  39,  66,  35 }, {  16,  18,  38,  67,  36 },
  {  17,  19,  39,  68,  37 }, {  18,  19,  39,  69,  38 },
  {  20,  40,  70,  72,  41 }, {  20,  40,  71,  74,  43 },
  {  20,  41,  78,  80,  43 }, {  21,  40,  70,  73,  42 },
  {  21,  40,  71,  75,  44 }, {  21,  42,  79,  81,  44 },
  {  22,  41,  72,  76,  45 }, {  22,  41,  78,  84,  49 },
  {  22,  45,  88,  94,  49 }, {  23,  42,  73,  77,  46 },
  {  23,  42,  79,  85,  50 }, {  23,  46,  89,  95,  50 },
  {  24,  43,  74,  82,  47 }, {  24,  43,  80,  86,  51 },
  {  24,  47,  92,  96,  51 }, {  25,  44,  75,  83,  48 },
  {  25,  44,  81,  87,  52 }, {  25,  48,  93,  97,  52 },
  {  26,  45,  76,  77,  46 }, {  26,  45,  88,  98,  53 },
  {  26,  46,  89,  99,  53 }, {  27,  47,  82,  83,  48 },
  {  27,  47,  92, 100,  56 }, {  27,  48,  93, 101,  56 },
  {  28,  49,  84,  90,  54 }, {  28,  49,  94, 102,  57 },
  {  28,  54, 108, 112,  57 }, {  29,  50,  85,  91,  55 },
  {  29,  50,  95, 103,  58 }, {  29,  55, 109, 113,  58 },
  {  30,  51,  86,  90,  54 }, {  30,  51,  96, 104,  59 },
  {  30,  54, 108, 114,  59 }, {  31,  52,  87,  91,  55 },
  {  31,  52,  97, 105,  60 }, {  31,  55, 109, 115,  60 },
  {  32,  53,  98, 106,  61 }, {  32,  53,  99, 107,  62 },
  {  32,  61, 116, 117,  62 }, {  33,  56, 100, 110,  63 },
  {  33,  56, 101, 111,  64 }, {  33,  63, 122, 123,  64 },
  {  34,  57, 102, 106,  61 }, {  34,  57, 112, 118,  65 },
  {  34,  61, 116, 124,  65 }, {  35,  58, 103, 107,  62 },
  {  35,  58, 113, 119,  66 }, {  35,  62, 117, 125,  66 },
  {  36,  59, 104, 110,  63 }, {  36,  59, 114, 120,  67 },
  {  36,  63, 122, 126,  67 }, {  37,  60, 105, 111,  64 },
  {  37,  60, 115, 121,  68 }, {  37,  64, 123, 127,  68 },
  {  38,  65, 118, 120,  67 }, {  38,  65, 124, 128,  69 },
  {  38,  67, 126, 129,  69 }, {  39,  66, 119, 121,  68 },
  {  39,  66, 125, 128,  69 }, {  39,  68, 127, 129,  69 },
  {  40,  70, 130, 131,  71 }, {  41,  72, 132, 136,  78 },
  {  42,  73, 133, 137,  79 }, {  43,  74, 134, 138,  80 },
  {  44,  75, 135, 139,  81 }, {  45,  76, 140, 144,  88 },
  {  46,  77, 141, 145,  89 }, {  47,  82, 142, 150,  92 },
  {  48,  83, 143, 151,  93 }, {  49,  84, 146, 152,  94 },
  {  50,  85, 147, 153,  95 }, {  51,  86, 148, 154,  96 },
  {  52,  87, 149, 155,  97 }, {  53,  98, 158, 159,  99 },
  {  54,  90, 156, 162, 108 }, {  55,  91, 157, 163, 109 },
  {  56, 100, 160, 161, 101 }, {  57, 102, 164, 170, 112 },
  {  58, 103, 165, 171, 113 }, {  59, 104, 166, 172, 114 },
  {  60, 105, 167, 173, 115 }, {  61, 106, 168, 176, 116 },
  {  62, 107, 169, 177, 117 }, {  63, 110, 174, 178, 122 },
  {  64, 111, 175, 179, 123 }, {  65, 118, 180, 184, 124 },
  {  66, 119, 181, 185, 125 }, {  67, 120, 182, 186, 126 },
  {  68, 121, 183, 187, 127 }, {  69, 128, 188, 189, 129 },
  {  70,  72,  76,  77,  73 }, {  70,  72, 132, 190, 130 },
  {  70,  73, 133, 191, 130 }, {  71,  74,  82,  83,  75 },
  {  71,  74, 134, 192, 131 }, {  71,  75, 135, 193, 131 },
  {  72,  76, 140, 196, 132 }, {  73,  77, 141, 197, 133 },
  {  74,  82, 142, 198, 134 }, {  75,  83, 143, 199, 135 },
  {  76,  77, 141, 200, 140 }, {  78,  80,  86,  90,  84 },
  {  78,  80, 138, 194, 136 }, {  78,  84, 146, 201, 136 },
  {  79,  81,  87,  91,  85 }, {  79,  81, 139, 195, 137 },
  {  79,  85, 147, 202, 137 }, {  80,  86, 148, 203, 138 },
  {  81,  87, 149, 204, 139 }, {  82,  83, 143, 205, 142 },
  {  84,  90, 156, 212, 146 }, {  85,  91, 157, 213, 147 },
  {  86,  90, 156, 214, 148 }, {  87,  91, 157, 215, 149 },
  {  88,  94, 102, 106,  98 }, {  88,  94, 152, 206, 144 },
  {  88,  98, 158, 210, 144 }, {  89,  95, 103, 107,  99 },
  {  89,  95, 153, 207, 145 }, {  89,  99, 159, 211, 145 },
  {  92,  96, 104, 110, 100 }, {  92,  96, 154, 208, 150 },
  {  92, 100, 160, 216, 150 }, {  93,  97, 105, 111, 101 },
  {  93,  97, 155, 209, 151 }, {  93, 101, 161, 217, 151 },
  {  94, 102, 164, 218, 152 }, {  95, 103, 165, 219, 153 },
  {  96, 104, 166, 220, 154 }, {  97, 105, 167, 221, 155 },
  {  98, 106, 168, 222, 158 }, {  99, 107, 169, 223, 159 },
  { 100, 110, 174, 228, 160 }, { 101, 111, 175, 229, 161 },
  { 102, 106, 168, 230, 164 }, { 103, 107, 169, 231, 165 },
  { 104, 110, 174, 232, 166 }, { 105, 111, 175, 233, 167 },
  { 108, 112, 118, 120, 114 }, { 108, 112, 170, 224, 162 },
  { 108, 114, 172, 226, 162 }, { 109, 113, 119, 121, 115 },
  { 109, 113, 171, 225, 163 }, { 109, 115, 173, 227, 163 },
  { 112, 118, 180, 235, 170 }, { 113, 119, 181, 236, 171 },
  { 114, 120, 182, 237, 172 }, { 115, 121, 183, 238, 173 },
  { 116, 117, 125, 128, 124 }, { 116, 117, 177, 234, 176 },
  { 116, 124, 184, 240, 176 }, { 117, 125, 185, 241, 177 },
  { 118, 120, 182, 244, 180 }, { 119, 121, 183, 245, 181 },
  { 122, 123, 127, 129, 126 }, { 122, 123, 179, 239, 178 },
  { 122, 126, 186, 242, 178 }, { 123, 127, 187, 243, 179 },
  { 124, 128, 188, 246, 184 }, { 125, 128, 188, 247, 185 },
  { 126, 129, 189, 248, 186 }, { 127, 129, 189, 249, 187 },
  { 130, 131, 192, 250, 190 }, { 130, 131, 193, 251, 191 },
  { 130, 190, 270, 271, 191 }, { 131, 192, 272, 273, 193 },
  { 132, 136, 194, 250, 190 }, { 132, 136, 201, 252, 196 },
  { 132, 190, 270, 274, 196 }, { 133, 137, 195, 251, 191 },
  { 133, 137, 202, 253, 197 }, { 133, 191, 271, 275, 197 },
  { 134, 138, 194, 250, 192 }, { 134, 138, 203, 254, 198 },
  { 134, 192, 272, 276, 198 }, { 135, 139, 195, 251, 193 },
  { 135, 139, 204, 255, 199 }, { 135, 193, 273, 277, 199 },
  { 136, 194, 279, 282, 201 }, { 137, 195, 280, 283, 202 },
  { 138, 194, 279, 284, 203 }, { 139, 195, 280, 285, 204 },
  { 140, 144, 206, 252, 196 }, { 140, 144, 210, 256, 200 },
  { 140, 196, 274, 278, 200 }, { 141, 145, 207, 253, 197 },
  { 141, 145, 211, 256, 200 }, { 141, 197, 275, 278, 200 },
  { 142, 150, 208, 254, 198 }, { 142, 150, 216, 257, 205 },
  { 142, 198, 276, 281, 205 }, { 143, 151, 209, 255, 199 },
  { 143, 151, 217, 257, 205 }, { 143, 199, 277, 281, 205 },
  { 144, 206, 290, 294, 210 }, { 145, 207, 291, 295, 211 },
  { 146, 152, 206, 252, 201 }, { 146, 152, 218, 258, 212 },
  { 146, 201, 282, 286, 212 }, { 147, 153, 207, 253, 202 },
  { 147, 153, 219, 259, 213 }, { 147, 202, 283, 287, 213 },
  { 148, 154, 208, 254, 203 }, { 148, 154, 220, 260, 214 },
  { 148, 203, 284, 288, 214 }, { 149, 155, 209, 255, 204 },
  { 149, 155, 221, 261, 215 }, { 149, 204, 285, 289, 215 },
  { 150, 208, 292, 296, 216 }, { 151, 209, 293, 297, 217 },
  { 152, 206, 290, 298, 218 }, { 153, 207, 291, 299, 219 },
  { 154, 208, 292, 300, 220 }, { 155, 209, 293, 301, 221 },
  { 156, 162, 224, 258, 212 }, { 156, 162, 226, 260, 214 },
  { 156, 212, 286, 288, 214 }, { 157, 163, 225, 259, 213 },
  { 157, 163, 227, 261, 215 }, { 157, 213, 287, 289, 215 },
  { 158, 159, 211, 256, 210 }, { 158, 159, 223, 262, 222 },
  { 158, 210, 294, 302, 222 }, { 159, 211, 295, 303, 223 },
  { 160, 161, 217, 257, 216 }, { 160, 161, 229, 263, 228 },
  { 160, 216, 296, 304, 228 }, { 161, 217, 297, 305, 229 },
  { 162, 224, 310, 312, 226 }, { 163, 225, 311, 313, 227 },
  { 164, 170, 224, 258, 218 }, { 164, 170, 235, 264, 230 },
  { 164, 218, 298, 306, 230 }, { 165, 171, 225, 259, 219 },
  { 165, 171, 236, 265, 231 }, { 165, 219, 299, 307, 231 },
  { 166, 172, 226, 260, 220 }, { 166, 172, 237, 266, 232 },
  { 166, 220, 300, 308, 232 }, { 167, 173, 227, 261, 221 },
  { 167, 173, 238, 267, 233 }, { 167, 221, 301, 309, 233 },
  { 168, 176, 234, 262, 222 }, { 168, 176, 240, 264, 230 },
  { 168, 222, 302, 306, 230 }, { 169, 177, 234, 262, 223 },
  { 169, 177, 241, 265, 231 }, { 169, 223, 303, 307, 231 },
  { 170, 224, 310, 314, 235 }, { 171, 225, 311, 315, 236 },
  { 172, 226, 312, 316, 237 }, { 173, 227, 313, 317, 238 },
  { 174, 178, 239, 263, 228 }, { 174, 178, 242, 266, 232 },
  { 174, 228, 304, 308, 232 }, { 175, 179, 239, 263, 229 },
  { 175, 179, 243, 267, 233 }, { 175, 229, 305, 309, 233 },
  { 176, 234, 318, 322, 240 }, { 177, 234, 318, 323, 241 },
  { 178, 239, 321, 324, 242 }, { 179, 239, 321, 325, 243 },
  { 180, 184, 240, 264, 235 }, { 180, 184, 246, 268, 244 },
  { 180, 235, 314, 319, 244 }, { 181, 185, 241, 265, 236 },
  { 181, 185, 247, 269, 245 }, { 181, 236, 315, 320, 245 },
  { 182, 186, 242, 266, 237 }, { 182, 186, 248, 268, 244 },
  { 182, 237, 316, 319, 244 }, { 183, 187, 243, 267, 238 },
  { 183, 187, 249, 269, 245 }, { 183, 238, 317, 320, 245 },
  { 184, 240, 322, 326, 246 }, { 185, 241, 323, 327, 247 },
  { 186, 242, 324, 328, 248 }, { 187, 243, 325, 329, 249 },
  { 188, 189, 248, 268, 246 }, { 188, 189, 249, 269, 247 },
  { 188, 246, 326, 327, 247 }, { 189, 248, 328, 329, 249 },
  { 190, 250, 330, 350, 270 }, { 191, 251, 331, 351, 271 },
  { 192, 250, 330, 352, 272 }, { 193, 251, 331, 353, 273 },
  { 194, 250, 330, 354, 279 }, { 195, 251, 331, 355, 280 },
  { 196, 252, 332, 356, 274 }, { 197, 253, 333, 357, 275 },
  { 198, 254, 334, 358, 276 }, { 199, 255, 335, 359, 277 },
  { 200, 256, 336, 360, 278 }, { 201, 252, 332, 361, 282 },
  { 202, 253, 333, 362, 283 }, { 203, 254, 334, 363, 284 },
  { 204, 255, 335, 364, 285 }, { 205, 257, 337, 365, 281 },
  { 206, 252, 332, 366, 290 }, { 207, 253, 333, 367, 291 },
  { 208, 254, 334, 368, 292 }, { 209, 255, 335, 369, 293 },
  { 210, 256, 336, 370, 294 }, { 211, 256, 336, 371, 295 },
  { 212, 258, 338, 372, 286 }, { 213, 259, 339, 373, 287 },
  { 214, 260, 340, 374, 288 }, { 215, 261, 341, 375, 289 },
  { 216, 257, 337, 376, 296 }, { 217, 257, 337, 377, 297 },
  { 218, 258, 338, 378, 298 }, { 219, 259, 339, 379, 299 },
  { 220, 260, 340, 380, 300 }, { 221, 261, 341, 381, 301 },
  { 222, 262, 342, 382, 302 }, { 223, 262, 342, 383, 303 },
  { 224, 258, 338, 384, 310 }, { 225, 259, 339, 385, 311 },
  { 226, 260, 340, 386, 312 }, { 227, 261, 341, 387, 313 },
  { 228, 263, 343, 388, 304 }, { 229, 263, 343, 389, 305 },
  { 230, 264, 344, 390, 306 }, { 231, 265, 345, 391, 307 },
  { 232, 266, 346, 392, 308 }, { 233, 267, 347, 393, 309 },
  { 234, 262, 342, 394, 318 }, { 235, 264, 344, 395, 314 },
  { 236, 265, 345, 396, 315 }, { 237, 266, 346, 397, 316 },
  { 238, 267, 347, 398, 317 }, { 239, 263, 343, 399, 321 },
  { 240, 264, 344, 400, 322 }, { 241, 265, 345, 401, 323 },
  { 242, 266, 346, 402, 324 }, { 243, 267, 347, 403, 325 },
  { 244, 268, 348, 404, 319 }, { 245, 269, 349, 405, 320 },
  { 246, 268, 348, 406, 326 }, { 247, 269, 349, 407, 327 },
  { 248, 268, 348, 408, 328 }, { 249, 269, 349, 409, 329 },
  { 270, 271, 275, 278, 274 }, { 270, 271, 351, 410, 350 },
  { 270, 274, 356, 412, 350 }, { 271, 275, 357, 413, 351 },
  { 272, 273, 277, 281, 276 }, { 272, 273, 353, 411, 352 },
  { 272, 276, 358, 414, 352 }, { 273, 277, 359, 415, 353 },
  { 274, 278, 360, 420, 356 }, { 275, 278, 360, 421, 357 },
  { 276, 281, 365, 422, 358 }, { 277, 281, 365, 423, 359 },
  { 279, 282, 286, 288, 284 }, { 279, 282, 361, 416, 354 },
  { 279, 284, 363, 418, 354 }, { 280, 283, 287, 289, 285 },
  { 280, 283, 362, 417, 355 }, { 280, 285, 364, 419, 355 },
  { 282, 286, 372, 426, 361 }, { 283, 287, 373, 427, 362 },
  { 284, 288, 374, 428, 363 }, { 285, 289, 375, 429, 364 },
  { 286, 288, 374, 436, 372 }, { 287, 289, 375, 437, 373 },
  { 290, 294, 302, 306, 298 }, { 290, 294, 370, 424, 366 },
  { 290, 298, 378, 432, 366 }, { 291, 295, 303, 307, 299 },
  { 291, 295, 371, 425, 367 }, { 291, 299, 379, 433, 367 },
  { 292, 296, 304, 308, 300 }, { 292, 296, 376, 430, 368 },
  { 292, 300, 380, 434, 368 }, { 293, 297, 305, 309, 301 },
  { 293, 297, 377, 431, 369 }, { 293, 301, 381, 435, 369 },
  { 294, 302, 382, 438, 370 }, { 295, 303, 383, 439, 371 },
  { 296, 304, 388, 440, 376 }, { 297, 305, 389, 441, 377 },
  { 298, 306, 390, 444, 378 }, { 299, 307, 391, 445, 379 },
  { 300, 308, 392, 446, 380 }, { 301, 309, 393, 447, 381 },
  { 302, 306, 390, 448, 382 }, { 303, 307, 391, 449, 383 },
  { 304, 308, 392, 454, 388 }, { 305, 309, 393, 455, 389 },
  { 310, 312, 316, 319, 314 }, { 310, 312, 386, 442, 384 },
  { 310, 314, 395, 450, 384 }, { 311, 313, 317, 320, 315 },
  { 311, 313, 387, 443, 385 }, { 311, 315, 396, 451, 385 },
  { 312, 316, 397, 452, 386 }, { 313, 317, 398, 453, 387 },
  { 314, 319, 404, 460, 395 }, { 315, 320, 405, 461, 396 },
  { 316, 319, 404, 462, 397 }, { 317, 320, 405, 463, 398 },
  { 318, 322, 326, 327, 323 }, { 318, 322, 400, 456, 394 },
  { 318, 323, 401, 457, 394 }, { 321, 324, 328, 329, 325 },
  { 321, 324, 402, 458, 399 }, { 321, 325, 403, 459, 399 },
  { 322, 326, 406, 464, 400 }, { 323, 327, 407, 465, 401 },
  { 324, 328, 408, 466, 402 }, { 325, 329, 409, 467, 403 },
  { 326, 327, 407, 468, 406 }, { 328, 329, 409, 469, 408 },
  { 330, 350, 410, 411, 352 }, { 330, 350, 412, 416, 354 },
  { 330, 352, 414, 418, 354 }, { 331, 351, 410, 411, 353 },
  { 331, 351, 413, 417, 355 }, { 331, 353, 415, 419, 355 },
  { 332, 356, 412, 416, 361 }, { 332, 356, 420, 424, 366 },
  { 332, 361, 426, 432, 366 }, { 333, 357, 413, 417, 362 },
  { 333, 357, 421, 425, 367 }, { 333, 362, 427, 433, 367 },
  { 334, 358, 414, 418, 363 }, { 334, 358, 422, 430, 368 },
  { 334, 363, 428, 434, 368 }, { 335, 359, 415, 419, 364 },
  { 335, 359, 423, 431, 369 }, { 335, 364, 429, 435, 369 },
  { 336, 360, 420, 424, 370 }, { 336, 360, 421, 425, 371 },
  { 336, 370, 438, 439, 371 }, { 337, 365, 422, 430, 376 },
  { 337, 365, 423, 431, 377 }, { 337, 376, 440, 441, 377 },
  { 338, 372, 426, 432, 378 }, { 338, 372, 436, 442, 384 },
  { 338, 378, 444, 450, 384 }, { 339, 373, 427, 433, 379 },
  { 339, 373, 437, 443, 385 }, { 339, 379, 445, 451, 385 },
  { 340, 374, 428, 434, 380 }, { 340, 374, 436, 442, 386 },
  { 340, 380, 446, 452, 386 }, { 341, 375, 429, 435, 381 },
  { 341, 375, 437, 443, 387 }, { 341, 381, 447, 453, 387 },
  { 342, 382, 438, 439, 383 }, { 342, 382, 448, 456, 394 },
  { 342, 383, 449, 457, 394 }, { 343, 388, 440, 441, 389 },
  { 343, 388, 454, 458, 399 }, { 343, 389, 455, 459, 399 },
  { 344, 390, 444, 450, 395 }, { 344, 390, 448, 456, 400 },
  { 344, 395, 460, 464, 400 }, { 345, 391, 445, 451, 396 },
  { 345, 391, 449, 457, 401 }, { 345, 396, 461, 465, 401 },
  { 346, 392, 446, 452, 397 }, { 346, 392, 454, 458, 402 },
  { 346, 397, 462, 466, 402 }, { 347, 393, 447, 453, 398 },
  { 347, 393, 455, 459, 403 }, { 347, 398, 463, 467, 403 },
  { 348, 404, 460, 464, 406 }, { 348, 404, 462, 466, 408 },
  { 348, 406, 468, 469, 408 }, { 349, 405, 461, 465, 407 },
  { 349, 405, 463, 467, 409 }, { 349, 407, 468, 469, 409 },
  { 350, 410, 470, 472, 412 }, { 351, 410, 470, 473, 413 },
  { 352, 411, 471, 474, 414 }, { 353, 411, 471, 475, 415 },
  { 354, 416, 478, 480, 418 }, { 355, 417, 479, 481, 419 },
  { 356, 412, 472, 476, 420 }, { 357, 413, 473, 477, 421 },
  { 358, 414, 474, 482, 422 }, { 359, 415, 475, 483, 423 },
  { 360, 420, 476, 477, 421 }, { 361, 416, 478, 484, 426 },
  { 362, 417, 479, 485, 427 }, { 363, 418, 480, 486, 428 },
  { 364, 419, 481, 487, 429 }, { 365, 422, 482, 483, 423 },
  { 366, 424, 488, 494, 432 }, { 367, 425, 489, 495, 433 },
  { 368, 430, 492, 496, 434 }, { 369, 431, 493, 497, 435 },
  { 370, 424, 488, 498, 438 }, { 371, 425, 489, 499, 439 },
  { 372, 426, 484, 490, 436 }, { 373, 427, 485, 491, 437 },
  { 374, 428, 486, 490, 436 }, { 375, 429, 487, 491, 437 },
  { 376, 430, 492, 500, 440 }, { 377, 431, 493, 501, 441 },
  { 378, 432, 494, 502, 444 }, { 379, 433, 495, 503, 445 },
  { 380, 434, 496, 504, 446 }, { 381, 435, 497, 505, 447 },
  { 382, 438, 498, 506, 448 }, { 383, 439, 499, 507, 449 },
  { 384, 442, 508, 512, 450 }, { 385, 443, 509, 513, 451 },
  { 386, 442, 508, 514, 452 }, { 387, 443, 509, 515, 453 },
  { 388, 440, 500, 510, 454 }, { 389, 441, 501, 511, 455 },
  { 390, 444, 502, 506, 448 }, { 391, 445, 503, 507, 449 },
  { 392, 446, 504, 510, 454 }, { 393, 447, 505, 511, 455 },
  { 394, 456, 516, 517, 457 }, { 395, 450, 512, 518, 460 },
  { 396, 451, 513, 519, 461 }, { 397, 452, 514, 520, 462 },
  { 398, 453, 515, 521, 463 }, { 399, 458, 522, 523, 459 },
  { 400, 456, 516, 524, 464 }, { 401, 457, 517, 525, 465 },
  { 402, 458, 522, 526, 466 }, { 403, 459, 523, 527, 467 },
  { 404, 460, 518, 520, 462 }, { 405, 461, 519, 521, 463 },
  { 406, 464, 524, 528, 468 }, { 407, 465, 525, 528, 468 },
  { 408, 466, 526, 529, 469 }, { 409, 467, 527, 529, 469 },
  { 410, 411, 471, 530, 470 }, { 412, 416, 478, 531, 472 },
  { 413, 417, 479, 532, 473 }, { 414, 418, 480, 533, 474 },
  { 415, 419, 481, 534, 475 }, { 420, 424, 488, 535, 476 },
  { 421, 425, 489, 536, 477 }, { 422, 430, 492, 537, 482 },
  { 423, 431, 493, 538, 483 }, { 426, 432, 494, 539, 484 },
  { 427, 433, 495, 540, 485 }, { 428, 434, 496, 541, 486 },
  { 429, 435, 497, 542, 487 }, { 436, 442, 508, 544, 490 },
  { 437, 443, 509, 545, 491 }, { 438, 439, 499, 543, 498 },
  { 440, 441, 501, 546, 500 }, { 444, 450, 512, 547, 502 },
  { 445, 451, 513, 548, 503 }, { 446, 452, 514, 549, 504 },
  { 447, 453, 515, 550, 505 }, { 448, 456, 516, 551, 506 },
  { 449, 457, 517, 552, 507 }, { 454, 458, 522, 553, 510 },
  { 455, 459, 523, 554, 511 }, { 460, 464, 524, 555, 518 },
  { 461, 465, 525, 556, 519 }, { 462, 466, 526, 557, 520 },
  { 463, 467, 527, 558, 521 }, { 468, 469, 529, 559, 528 },
  { 470, 472, 476, 477, 473 }, { 470, 472, 531, 560, 530 },
  { 470, 473, 532, 561, 530 }, { 471, 474, 482, 483, 475 },
  { 471, 474, 533, 560, 530 }, { 471, 475, 534, 561, 530 },
  { 472, 476, 535, 562, 531 }, { 473, 477, 536, 563, 532 },
  { 474, 482, 537, 564, 533 }, { 475, 483, 538, 565, 534 },
  { 476, 477, 536, 566, 535 }, { 478, 480, 486, 490, 484 },
  { 478, 480, 533, 560, 531 }, { 478, 484, 539, 562, 531 },
  { 479, 481, 487, 491, 485 }, { 479, 481, 534, 561, 532 },
  { 479, 485, 540, 563, 532 }, { 480, 486, 541, 564, 533 },
  { 481, 487, 542, 565, 534 }, { 482, 483, 538, 567, 537 },
  { 484, 490, 544, 568, 539 }, { 485, 491, 545, 569, 540 },
  { 486, 490, 544, 570, 541 }, { 487, 491, 545, 571, 542 },
  { 488, 494, 502, 506, 498 }, { 488, 494, 539, 562, 535 },
  { 488, 498, 543, 566, 535 }, { 489, 495, 503, 507, 499 },
  { 489, 495, 540, 563, 536 }, { 489, 499, 543, 566, 536 },
  { 492, 496, 504, 510, 500 }, { 492, 496, 541, 564, 537 },
  { 492, 500, 546, 567, 537 }, { 493, 497, 505, 511, 501 },
  { 493, 497, 542, 565, 538 }, { 493, 501, 546, 567, 538 },
  { 494, 502, 547, 568, 539 }, { 495, 503, 548, 569, 540 },
  { 496, 504, 549, 570, 541 }, { 497, 505, 550, 571, 542 },
  { 498, 506, 551, 572, 543 }, { 499, 507, 552, 572, 543 },
  { 500, 510, 553, 573, 546 }, { 501, 511, 554, 573, 546 },
  { 502, 506, 551, 574, 547 }, { 503, 507, 552, 575, 548 },
  { 504, 510, 553, 576, 549 }, { 505, 511, 554, 577, 550 },
  { 508, 512, 518, 520, 514 }, { 508, 512, 547, 568, 544 },
  { 508, 514, 549, 570, 544 }, { 509, 513, 519, 521, 515 },
  { 509, 513, 548, 569, 545 }, { 509, 515, 550, 571, 545 },
  { 512, 518, 555, 574, 547 }, { 513, 519, 556, 575, 548 },
  { 514, 520, 557, 576, 549 }, { 515, 521, 558, 577, 550 },
  { 516, 517, 525, 528, 524 }, { 516, 517, 552, 572, 551 },
  { 516, 524, 555, 574, 551 }, { 517, 525, 556, 575, 552 },
  { 518, 520, 557, 578, 555 }, { 519, 521, 558, 579, 556 },
  { 522, 523, 527, 529, 526 }, { 522, 523, 554, 573, 553 },
  { 522, 526, 557, 576, 553 }, { 523, 527, 558, 577, 554 },
  { 524, 528, 559, 578, 555 }, { 525, 528, 559, 579, 556 },
  { 526, 529, 559, 578, 557 }, { 527, 529, 559, 579, 558 },
  { 530, 560, 580, 581, 561 }, { 531, 560, 580, 582, 562 },
  { 532, 561, 581, 583, 563 }, { 533, 560, 580, 584, 564 },
  { 534, 561, 581, 585, 565 }, { 535, 562, 582, 586, 566 },
  { 536, 563, 583, 586, 566 }, { 537, 564, 584, 587, 567 },
  { 538, 565, 585, 587, 567 }, { 539, 562, 582, 588, 568 },
  { 540, 563, 583, 589, 569 }, { 541, 564, 584, 590, 570 },
  { 542, 565, 585, 591, 571 }, { 543, 566, 586, 592, 572 },
  { 544, 568, 588, 590, 570 }, { 545, 569, 589, 591, 571 },
  { 546, 567, 587, 593, 573 }, { 547, 568, 588, 594, 574 },
  { 548, 569, 589, 595, 575 }, { 549, 570, 590, 596, 576 },
  { 550, 571, 591, 597, 577 }, { 551, 572, 592, 594, 574 },
  { 552, 572, 592, 595, 575 }, { 553, 573, 593, 596, 576 },
  { 554, 573, 593, 597, 577 }, { 555, 574, 594, 598, 578 },
  { 556, 575, 595, 599, 579 }, { 557, 576, 596, 598, 578 },
  { 558, 577, 597, 599, 579 }, { 559, 578, 598, 599, 579 },
  { 580, 581, 583, 586, 582 }, { 580, 581, 585, 587, 584 },
  { 580, 582, 588, 590, 584 }, { 581, 583, 589, 591, 585 },
  { 582, 586, 592, 594, 588 }, { 583, 586, 592, 595, 589 },
  { 584, 587, 593, 596, 590 }, { 585, 587, 593, 597, 591 },
  { 588, 590, 596, 598, 594 }, { 589, 591, 597, 599, 595 },
  { 592, 594, 598, 599, 595 }, { 593, 596, 598, 599, 597 }
};



static const float vert_600[NUM_VERT_600][4] = {
  {        0.0,        0.0,        0.0,       -2.0 },
  {        0.0, -GOLDENINV,       -1.0,    -GOLDEN },
  {        0.0,  GOLDENINV,       -1.0,    -GOLDEN },
  {       -1.0,        0.0, -GOLDENINV,    -GOLDEN },
  {        1.0,        0.0, -GOLDENINV,    -GOLDEN },
  { -GOLDENINV,       -1.0,        0.0,    -GOLDEN },
  {  GOLDENINV,       -1.0,        0.0,    -GOLDEN },
  { -GOLDENINV,        1.0,        0.0,    -GOLDEN },
  {  GOLDENINV,        1.0,        0.0,    -GOLDEN },
  {       -1.0,        0.0,  GOLDENINV,    -GOLDEN },
  {        1.0,        0.0,  GOLDENINV,    -GOLDEN },
  {        0.0, -GOLDENINV,        1.0,    -GOLDEN },
  {        0.0,  GOLDENINV,        1.0,    -GOLDEN },
  { -GOLDENINV,        0.0,    -GOLDEN,       -1.0 },
  {  GOLDENINV,        0.0,    -GOLDEN,       -1.0 },
  {       -1.0,       -1.0,       -1.0,       -1.0 },
  {        1.0,       -1.0,       -1.0,       -1.0 },
  {       -1.0,        1.0,       -1.0,       -1.0 },
  {        1.0,        1.0,       -1.0,       -1.0 },
  {        0.0,    -GOLDEN, -GOLDENINV,       -1.0 },
  {        0.0,     GOLDEN, -GOLDENINV,       -1.0 },
  {    -GOLDEN, -GOLDENINV,        0.0,       -1.0 },
  {     GOLDEN, -GOLDENINV,        0.0,       -1.0 },
  {    -GOLDEN,  GOLDENINV,        0.0,       -1.0 },
  {     GOLDEN,  GOLDENINV,        0.0,       -1.0 },
  {        0.0,    -GOLDEN,  GOLDENINV,       -1.0 },
  {        0.0,     GOLDEN,  GOLDENINV,       -1.0 },
  {       -1.0,       -1.0,        1.0,       -1.0 },
  {        1.0,       -1.0,        1.0,       -1.0 },
  {       -1.0,        1.0,        1.0,       -1.0 },
  {        1.0,        1.0,        1.0,       -1.0 },
  { -GOLDENINV,        0.0,     GOLDEN,       -1.0 },
  {  GOLDENINV,        0.0,     GOLDEN,       -1.0 },
  {        0.0,       -1.0,    -GOLDEN, -GOLDENINV },
  {        0.0,        1.0,    -GOLDEN, -GOLDENINV },
  {    -GOLDEN,        0.0,       -1.0, -GOLDENINV },
  {     GOLDEN,        0.0,       -1.0, -GOLDENINV },
  {       -1.0,    -GOLDEN,        0.0, -GOLDENINV },
  {        1.0,    -GOLDEN,        0.0, -GOLDENINV },
  {       -1.0,     GOLDEN,        0.0, -GOLDENINV },
  {        1.0,     GOLDEN,        0.0, -GOLDENINV },
  {    -GOLDEN,        0.0,        1.0, -GOLDENINV },
  {     GOLDEN,        0.0,        1.0, -GOLDENINV },
  {        0.0,       -1.0,     GOLDEN, -GOLDENINV },
  {        0.0,        1.0,     GOLDEN, -GOLDENINV },
  {        0.0,        0.0,       -2.0,        0.0 },
  {       -1.0, -GOLDENINV,    -GOLDEN,        0.0 },
  {        1.0, -GOLDENINV,    -GOLDEN,        0.0 },
  {       -1.0,  GOLDENINV,    -GOLDEN,        0.0 },
  {        1.0,  GOLDENINV,    -GOLDEN,        0.0 },
  { -GOLDENINV,    -GOLDEN,       -1.0,        0.0 },
  {  GOLDENINV,    -GOLDEN,       -1.0,        0.0 },
  { -GOLDENINV,     GOLDEN,       -1.0,        0.0 },
  {  GOLDENINV,     GOLDEN,       -1.0,        0.0 },
  {    -GOLDEN,       -1.0, -GOLDENINV,        0.0 },
  {     GOLDEN,       -1.0, -GOLDENINV,        0.0 },
  {    -GOLDEN,        1.0, -GOLDENINV,        0.0 },
  {     GOLDEN,        1.0, -GOLDENINV,        0.0 },
  {        0.0,       -2.0,        0.0,        0.0 },
  {       -2.0,        0.0,        0.0,        0.0 },
  {        2.0,        0.0,        0.0,        0.0 },
  {        0.0,        2.0,        0.0,        0.0 },
  {    -GOLDEN,       -1.0,  GOLDENINV,        0.0 },
  {     GOLDEN,       -1.0,  GOLDENINV,        0.0 },
  {    -GOLDEN,        1.0,  GOLDENINV,        0.0 },
  {     GOLDEN,        1.0,  GOLDENINV,        0.0 },
  { -GOLDENINV,    -GOLDEN,        1.0,        0.0 },
  {  GOLDENINV,    -GOLDEN,        1.0,        0.0 },
  { -GOLDENINV,     GOLDEN,        1.0,        0.0 },
  {  GOLDENINV,     GOLDEN,        1.0,        0.0 },
  {       -1.0, -GOLDENINV,     GOLDEN,        0.0 },
  {        1.0, -GOLDENINV,     GOLDEN,        0.0 },
  {       -1.0,  GOLDENINV,     GOLDEN,        0.0 },
  {        1.0,  GOLDENINV,     GOLDEN,        0.0 },
  {        0.0,        0.0,        2.0,        0.0 },
  {        0.0,       -1.0,    -GOLDEN,  GOLDENINV },
  {        0.0,        1.0,    -GOLDEN,  GOLDENINV },
  {    -GOLDEN,        0.0,       -1.0,  GOLDENINV },
  {     GOLDEN,        0.0,       -1.0,  GOLDENINV },
  {       -1.0,    -GOLDEN,        0.0,  GOLDENINV },
  {        1.0,    -GOLDEN,        0.0,  GOLDENINV },
  {       -1.0,     GOLDEN,        0.0,  GOLDENINV },
  {        1.0,     GOLDEN,        0.0,  GOLDENINV },
  {    -GOLDEN,        0.0,        1.0,  GOLDENINV },
  {     GOLDEN,        0.0,        1.0,  GOLDENINV },
  {        0.0,       -1.0,     GOLDEN,  GOLDENINV },
  {        0.0,        1.0,     GOLDEN,  GOLDENINV },
  { -GOLDENINV,        0.0,    -GOLDEN,        1.0 },
  {  GOLDENINV,        0.0,    -GOLDEN,        1.0 },
  {       -1.0,       -1.0,       -1.0,        1.0 },
  {        1.0,       -1.0,       -1.0,        1.0 },
  {       -1.0,        1.0,       -1.0,        1.0 },
  {        1.0,        1.0,       -1.0,        1.0 },
  {        0.0,    -GOLDEN, -GOLDENINV,        1.0 },
  {        0.0,     GOLDEN, -GOLDENINV,        1.0 },
  {    -GOLDEN, -GOLDENINV,        0.0,        1.0 },
  {     GOLDEN, -GOLDENINV,        0.0,        1.0 },
  {    -GOLDEN,  GOLDENINV,        0.0,        1.0 },
  {     GOLDEN,  GOLDENINV,        0.0,        1.0 },
  {        0.0,    -GOLDEN,  GOLDENINV,        1.0 },
  {        0.0,     GOLDEN,  GOLDENINV,        1.0 },
  {       -1.0,       -1.0,        1.0,        1.0 },
  {        1.0,       -1.0,        1.0,        1.0 },
  {       -1.0,        1.0,        1.0,        1.0 },
  {        1.0,        1.0,        1.0,        1.0 },
  { -GOLDENINV,        0.0,     GOLDEN,        1.0 },
  {  GOLDENINV,        0.0,     GOLDEN,        1.0 },
  {        0.0, -GOLDENINV,       -1.0,     GOLDEN },
  {        0.0,  GOLDENINV,       -1.0,     GOLDEN },
  {       -1.0,        0.0, -GOLDENINV,     GOLDEN },
  {        1.0,        0.0, -GOLDENINV,     GOLDEN },
  { -GOLDENINV,       -1.0,        0.0,     GOLDEN },
  {  GOLDENINV,       -1.0,        0.0,     GOLDEN },
  { -GOLDENINV,        1.0,        0.0,     GOLDEN },
  {  GOLDENINV,        1.0,        0.0,     GOLDEN },
  {       -1.0,        0.0,  GOLDENINV,     GOLDEN },
  {        1.0,        0.0,  GOLDENINV,     GOLDEN },
  {        0.0, -GOLDENINV,        1.0,     GOLDEN },
  {        0.0,  GOLDENINV,        1.0,     GOLDEN },
  {        0.0,        0.0,        0.0,        2.0 }
};

static const int edge_600[NUM_EDGE_600][2] = {
  {   0,   1 }, {   0,   2 }, {   0,   3 }, {   0,   4 }, {   0,   5 },
  {   0,   6 }, {   0,   7 }, {   0,   8 }, {   0,   9 }, {   0,  10 },
  {   0,  11 }, {   0,  12 }, {   1,   2 }, {   1,   3 }, {   1,   4 },
  {   1,   5 }, {   1,   6 }, {   1,  13 }, {   1,  14 }, {   1,  15 },
  {   1,  16 }, {   1,  19 }, {   1,  33 }, {   2,   3 }, {   2,   4 },
  {   2,   7 }, {   2,   8 }, {   2,  13 }, {   2,  14 }, {   2,  17 },
  {   2,  18 }, {   2,  20 }, {   2,  34 }, {   3,   5 }, {   3,   7 },
  {   3,   9 }, {   3,  13 }, {   3,  15 }, {   3,  17 }, {   3,  21 },
  {   3,  23 }, {   3,  35 }, {   4,   6 }, {   4,   8 }, {   4,  10 },
  {   4,  14 }, {   4,  16 }, {   4,  18 }, {   4,  22 }, {   4,  24 },
  {   4,  36 }, {   5,   6 }, {   5,   9 }, {   5,  11 }, {   5,  15 },
  {   5,  19 }, {   5,  21 }, {   5,  25 }, {   5,  27 }, {   5,  37 },
  {   6,  10 }, {   6,  11 }, {   6,  16 }, {   6,  19 }, {   6,  22 },
  {   6,  25 }, {   6,  28 }, {   6,  38 }, {   7,   8 }, {   7,   9 },
  {   7,  12 }, {   7,  17 }, {   7,  20 }, {   7,  23 }, {   7,  26 },
  {   7,  29 }, {   7,  39 }, {   8,  10 }, {   8,  12 }, {   8,  18 },
  {   8,  20 }, {   8,  24 }, {   8,  26 }, {   8,  30 }, {   8,  40 },
  {   9,  11 }, {   9,  12 }, {   9,  21 }, {   9,  23 }, {   9,  27 },
  {   9,  29 }, {   9,  31 }, {   9,  41 }, {  10,  11 }, {  10,  12 },
  {  10,  22 }, {  10,  24 }, {  10,  28 }, {  10,  30 }, {  10,  32 },
  {  10,  42 }, {  11,  12 }, {  11,  25 }, {  11,  27 }, {  11,  28 },
  {  11,  31 }, {  11,  32 }, {  11,  43 }, {  12,  26 }, {  12,  29 },
  {  12,  30 }, {  12,  31 }, {  12,  32 }, {  12,  44 }, {  13,  14 },
  {  13,  15 }, {  13,  17 }, {  13,  33 }, {  13,  34 }, {  13,  35 },
  {  13,  45 }, {  13,  46 }, {  13,  48 }, {  14,  16 }, {  14,  18 },
  {  14,  33 }, {  14,  34 }, {  14,  36 }, {  14,  45 }, {  14,  47 },
  {  14,  49 }, {  15,  19 }, {  15,  21 }, {  15,  33 }, {  15,  35 },
  {  15,  37 }, {  15,  46 }, {  15,  50 }, {  15,  54 }, {  16,  19 },
  {  16,  22 }, {  16,  33 }, {  16,  36 }, {  16,  38 }, {  16,  47 },
  {  16,  51 }, {  16,  55 }, {  17,  20 }, {  17,  23 }, {  17,  34 },
  {  17,  35 }, {  17,  39 }, {  17,  48 }, {  17,  52 }, {  17,  56 },
  {  18,  20 }, {  18,  24 }, {  18,  34 }, {  18,  36 }, {  18,  40 },
  {  18,  49 }, {  18,  53 }, {  18,  57 }, {  19,  25 }, {  19,  33 },
  {  19,  37 }, {  19,  38 }, {  19,  50 }, {  19,  51 }, {  19,  58 },
  {  20,  26 }, {  20,  34 }, {  20,  39 }, {  20,  40 }, {  20,  52 },
  {  20,  53 }, {  20,  61 }, {  21,  23 }, {  21,  27 }, {  21,  35 },
  {  21,  37 }, {  21,  41 }, {  21,  54 }, {  21,  59 }, {  21,  62 },
  {  22,  24 }, {  22,  28 }, {  22,  36 }, {  22,  38 }, {  22,  42 },
  {  22,  55 }, {  22,  60 }, {  22,  63 }, {  23,  29 }, {  23,  35 },
  {  23,  39 }, {  23,  41 }, {  23,  56 }, {  23,  59 }, {  23,  64 },
  {  24,  30 }, {  24,  36 }, {  24,  40 }, {  24,  42 }, {  24,  57 },
  {  24,  60 }, {  24,  65 }, {  25,  27 }, {  25,  28 }, {  25,  37 },
  {  25,  38 }, {  25,  43 }, {  25,  58 }, {  25,  66 }, {  25,  67 },
  {  26,  29 }, {  26,  30 }, {  26,  39 }, {  26,  40 }, {  26,  44 },
  {  26,  61 }, {  26,  68 }, {  26,  69 }, {  27,  31 }, {  27,  37 },
  {  27,  41 }, {  27,  43 }, {  27,  62 }, {  27,  66 }, {  27,  70 },
  {  28,  32 }, {  28,  38 }, {  28,  42 }, {  28,  43 }, {  28,  63 },
  {  28,  67 }, {  28,  71 }, {  29,  31 }, {  29,  39 }, {  29,  41 },
  {  29,  44 }, {  29,  64 }, {  29,  68 }, {  29,  72 }, {  30,  32 },
  {  30,  40 }, {  30,  42 }, {  30,  44 }, {  30,  65 }, {  30,  69 },
  {  30,  73 }, {  31,  32 }, {  31,  41 }, {  31,  43 }, {  31,  44 },
  {  31,  70 }, {  31,  72 }, {  31,  74 }, {  32,  42 }, {  32,  43 },
  {  32,  44 }, {  32,  71 }, {  32,  73 }, {  32,  74 }, {  33,  45 },
  {  33,  46 }, {  33,  47 }, {  33,  50 }, {  33,  51 }, {  33,  75 },
  {  34,  45 }, {  34,  48 }, {  34,  49 }, {  34,  52 }, {  34,  53 },
  {  34,  76 }, {  35,  46 }, {  35,  48 }, {  35,  54 }, {  35,  56 },
  {  35,  59 }, {  35,  77 }, {  36,  47 }, {  36,  49 }, {  36,  55 },
  {  36,  57 }, {  36,  60 }, {  36,  78 }, {  37,  50 }, {  37,  54 },
  {  37,  58 }, {  37,  62 }, {  37,  66 }, {  37,  79 }, {  38,  51 },
  {  38,  55 }, {  38,  58 }, {  38,  63 }, {  38,  67 }, {  38,  80 },
  {  39,  52 }, {  39,  56 }, {  39,  61 }, {  39,  64 }, {  39,  68 },
  {  39,  81 }, {  40,  53 }, {  40,  57 }, {  40,  61 }, {  40,  65 },
  {  40,  69 }, {  40,  82 }, {  41,  59 }, {  41,  62 }, {  41,  64 },
  {  41,  70 }, {  41,  72 }, {  41,  83 }, {  42,  60 }, {  42,  63 },
  {  42,  65 }, {  42,  71 }, {  42,  73 }, {  42,  84 }, {  43,  66 },
  {  43,  67 }, {  43,  70 }, {  43,  71 }, {  43,  74 }, {  43,  85 },
  {  44,  68 }, {  44,  69 }, {  44,  72 }, {  44,  73 }, {  44,  74 },
  {  44,  86 }, {  45,  46 }, {  45,  47 }, {  45,  48 }, {  45,  49 },
  {  45,  75 }, {  45,  76 }, {  45,  87 }, {  45,  88 }, {  46,  48 },
  {  46,  50 }, {  46,  54 }, {  46,  75 }, {  46,  77 }, {  46,  87 },
  {  46,  89 }, {  47,  49 }, {  47,  51 }, {  47,  55 }, {  47,  75 },
  {  47,  78 }, {  47,  88 }, {  47,  90 }, {  48,  52 }, {  48,  56 },
  {  48,  76 }, {  48,  77 }, {  48,  87 }, {  48,  91 }, {  49,  53 },
  {  49,  57 }, {  49,  76 }, {  49,  78 }, {  49,  88 }, {  49,  92 },
  {  50,  51 }, {  50,  54 }, {  50,  58 }, {  50,  75 }, {  50,  79 },
  {  50,  89 }, {  50,  93 }, {  51,  55 }, {  51,  58 }, {  51,  75 },
  {  51,  80 }, {  51,  90 }, {  51,  93 }, {  52,  53 }, {  52,  56 },
  {  52,  61 }, {  52,  76 }, {  52,  81 }, {  52,  91 }, {  52,  94 },
  {  53,  57 }, {  53,  61 }, {  53,  76 }, {  53,  82 }, {  53,  92 },
  {  53,  94 }, {  54,  59 }, {  54,  62 }, {  54,  77 }, {  54,  79 },
  {  54,  89 }, {  54,  95 }, {  55,  60 }, {  55,  63 }, {  55,  78 },
  {  55,  80 }, {  55,  90 }, {  55,  96 }, {  56,  59 }, {  56,  64 },
  {  56,  77 }, {  56,  81 }, {  56,  91 }, {  56,  97 }, {  57,  60 },
  {  57,  65 }, {  57,  78 }, {  57,  82 }, {  57,  92 }, {  57,  98 },
  {  58,  66 }, {  58,  67 }, {  58,  79 }, {  58,  80 }, {  58,  93 },
  {  58,  99 }, {  59,  62 }, {  59,  64 }, {  59,  77 }, {  59,  83 },
  {  59,  95 }, {  59,  97 }, {  60,  63 }, {  60,  65 }, {  60,  78 },
  {  60,  84 }, {  60,  96 }, {  60,  98 }, {  61,  68 }, {  61,  69 },
  {  61,  81 }, {  61,  82 }, {  61,  94 }, {  61, 100 }, {  62,  66 },
  {  62,  70 }, {  62,  79 }, {  62,  83 }, {  62,  95 }, {  62, 101 },
  {  63,  67 }, {  63,  71 }, {  63,  80 }, {  63,  84 }, {  63,  96 },
  {  63, 102 }, {  64,  68 }, {  64,  72 }, {  64,  81 }, {  64,  83 },
  {  64,  97 }, {  64, 103 }, {  65,  69 }, {  65,  73 }, {  65,  82 },
  {  65,  84 }, {  65,  98 }, {  65, 104 }, {  66,  67 }, {  66,  70 },
  {  66,  79 }, {  66,  85 }, {  66,  99 }, {  66, 101 }, {  67,  71 },
  {  67,  80 }, {  67,  85 }, {  67,  99 }, {  67, 102 }, {  68,  69 },
  {  68,  72 }, {  68,  81 }, {  68,  86 }, {  68, 100 }, {  68, 103 },
  {  69,  73 }, {  69,  82 }, {  69,  86 }, {  69, 100 }, {  69, 104 },
  {  70,  72 }, {  70,  74 }, {  70,  83 }, {  70,  85 }, {  70, 101 },
  {  70, 105 }, {  71,  73 }, {  71,  74 }, {  71,  84 }, {  71,  85 },
  {  71, 102 }, {  71, 106 }, {  72,  74 }, {  72,  83 }, {  72,  86 },
  {  72, 103 }, {  72, 105 }, {  73,  74 }, {  73,  84 }, {  73,  86 },
  {  73, 104 }, {  73, 106 }, {  74,  85 }, {  74,  86 }, {  74, 105 },
  {  74, 106 }, {  75,  87 }, {  75,  88 }, {  75,  89 }, {  75,  90 },
  {  75,  93 }, {  75, 107 }, {  76,  87 }, {  76,  88 }, {  76,  91 },
  {  76,  92 }, {  76,  94 }, {  76, 108 }, {  77,  87 }, {  77,  89 },
  {  77,  91 }, {  77,  95 }, {  77,  97 }, {  77, 109 }, {  78,  88 },
  {  78,  90 }, {  78,  92 }, {  78,  96 }, {  78,  98 }, {  78, 110 },
  {  79,  89 }, {  79,  93 }, {  79,  95 }, {  79,  99 }, {  79, 101 },
  {  79, 111 }, {  80,  90 }, {  80,  93 }, {  80,  96 }, {  80,  99 },
  {  80, 102 }, {  80, 112 }, {  81,  91 }, {  81,  94 }, {  81,  97 },
  {  81, 100 }, {  81, 103 }, {  81, 113 }, {  82,  92 }, {  82,  94 },
  {  82,  98 }, {  82, 100 }, {  82, 104 }, {  82, 114 }, {  83,  95 },
  {  83,  97 }, {  83, 101 }, {  83, 103 }, {  83, 105 }, {  83, 115 },
  {  84,  96 }, {  84,  98 }, {  84, 102 }, {  84, 104 }, {  84, 106 },
  {  84, 116 }, {  85,  99 }, {  85, 101 }, {  85, 102 }, {  85, 105 },
  {  85, 106 }, {  85, 117 }, {  86, 100 }, {  86, 103 }, {  86, 104 },
  {  86, 105 }, {  86, 106 }, {  86, 118 }, {  87,  88 }, {  87,  89 },
  {  87,  91 }, {  87, 107 }, {  87, 108 }, {  87, 109 }, {  88,  90 },
  {  88,  92 }, {  88, 107 }, {  88, 108 }, {  88, 110 }, {  89,  93 },
  {  89,  95 }, {  89, 107 }, {  89, 109 }, {  89, 111 }, {  90,  93 },
  {  90,  96 }, {  90, 107 }, {  90, 110 }, {  90, 112 }, {  91,  94 },
  {  91,  97 }, {  91, 108 }, {  91, 109 }, {  91, 113 }, {  92,  94 },
  {  92,  98 }, {  92, 108 }, {  92, 110 }, {  92, 114 }, {  93,  99 },
  {  93, 107 }, {  93, 111 }, {  93, 112 }, {  94, 100 }, {  94, 108 },
  {  94, 113 }, {  94, 114 }, {  95,  97 }, {  95, 101 }, {  95, 109 },
  {  95, 111 }, {  95, 115 }, {  96,  98 }, {  96, 102 }, {  96, 110 },
  {  96, 112 }, {  96, 116 }, {  97, 103 }, {  97, 109 }, {  97, 113 },
  {  97, 115 }, {  98, 104 }, {  98, 110 }, {  98, 114 }, {  98, 116 },
  {  99, 101 }, {  99, 102 }, {  99, 111 }, {  99, 112 }, {  99, 117 },
  { 100, 103 }, { 100, 104 }, { 100, 113 }, { 100, 114 }, { 100, 118 },
  { 101, 105 }, { 101, 111 }, { 101, 115 }, { 101, 117 }, { 102, 106 },
  { 102, 112 }, { 102, 116 }, { 102, 117 }, { 103, 105 }, { 103, 113 },
  { 103, 115 }, { 103, 118 }, { 104, 106 }, { 104, 114 }, { 104, 116 },
  { 104, 118 }, { 105, 106 }, { 105, 115 }, { 105, 117 }, { 105, 118 },
  { 106, 116 }, { 106, 117 }, { 106, 118 }, { 107, 108 }, { 107, 109 },
  { 107, 110 }, { 107, 111 }, { 107, 112 }, { 107, 119 }, { 108, 109 },
  { 108, 110 }, { 108, 113 }, { 108, 114 }, { 108, 119 }, { 109, 111 },
  { 109, 113 }, { 109, 115 }, { 109, 119 }, { 110, 112 }, { 110, 114 },
  { 110, 116 }, { 110, 119 }, { 111, 112 }, { 111, 115 }, { 111, 117 },
  { 111, 119 }, { 112, 116 }, { 112, 117 }, { 112, 119 }, { 113, 114 },
  { 113, 115 }, { 113, 118 }, { 113, 119 }, { 114, 116 }, { 114, 118 },
  { 114, 119 }, { 115, 117 }, { 115, 118 }, { 115, 119 }, { 116, 117 },
  { 116, 118 }, { 116, 119 }, { 117, 118 }, { 117, 119 }, { 118, 119 }
};

static const int face_600[NUM_FACE_600][VERT_PER_FACE_600] = {
  {   0,   1,   2 }, {   0,   1,   3 }, {   0,   1,   4 }, {   0,   1,   5 },
  {   0,   1,   6 }, {   0,   2,   3 }, {   0,   2,   4 }, {   0,   2,   7 },
  {   0,   2,   8 }, {   0,   3,   5 }, {   0,   3,   7 }, {   0,   3,   9 },
  {   0,   4,   6 }, {   0,   4,   8 }, {   0,   4,  10 }, {   0,   5,   6 },
  {   0,   5,   9 }, {   0,   5,  11 }, {   0,   6,  10 }, {   0,   6,  11 },
  {   0,   7,   8 }, {   0,   7,   9 }, {   0,   7,  12 }, {   0,   8,  10 },
  {   0,   8,  12 }, {   0,   9,  11 }, {   0,   9,  12 }, {   0,  10,  11 },
  {   0,  10,  12 }, {   0,  11,  12 }, {   1,   2,   3 }, {   1,   2,   4 },
  {   1,   2,  13 }, {   1,   2,  14 }, {   1,   3,   5 }, {   1,   3,  13 },
  {   1,   3,  15 }, {   1,   4,   6 }, {   1,   4,  14 }, {   1,   4,  16 },
  {   1,   5,   6 }, {   1,   5,  15 }, {   1,   5,  19 }, {   1,   6,  16 },
  {   1,   6,  19 }, {   1,  13,  14 }, {   1,  13,  15 }, {   1,  13,  33 },
  {   1,  14,  16 }, {   1,  14,  33 }, {   1,  15,  19 }, {   1,  15,  33 },
  {   1,  16,  19 }, {   1,  16,  33 }, {   1,  19,  33 }, {   2,   3,   7 },
  {   2,   3,  13 }, {   2,   3,  17 }, {   2,   4,   8 }, {   2,   4,  14 },
  {   2,   4,  18 }, {   2,   7,   8 }, {   2,   7,  17 }, {   2,   7,  20 },
  {   2,   8,  18 }, {   2,   8,  20 }, {   2,  13,  14 }, {   2,  13,  17 },
  {   2,  13,  34 }, {   2,  14,  18 }, {   2,  14,  34 }, {   2,  17,  20 },
  {   2,  17,  34 }, {   2,  18,  20 }, {   2,  18,  34 }, {   2,  20,  34 },
  {   3,   5,   9 }, {   3,   5,  15 }, {   3,   5,  21 }, {   3,   7,   9 },
  {   3,   7,  17 }, {   3,   7,  23 }, {   3,   9,  21 }, {   3,   9,  23 },
  {   3,  13,  15 }, {   3,  13,  17 }, {   3,  13,  35 }, {   3,  15,  21 },
  {   3,  15,  35 }, {   3,  17,  23 }, {   3,  17,  35 }, {   3,  21,  23 },
  {   3,  21,  35 }, {   3,  23,  35 }, {   4,   6,  10 }, {   4,   6,  16 },
  {   4,   6,  22 }, {   4,   8,  10 }, {   4,   8,  18 }, {   4,   8,  24 },
  {   4,  10,  22 }, {   4,  10,  24 }, {   4,  14,  16 }, {   4,  14,  18 },
  {   4,  14,  36 }, {   4,  16,  22 }, {   4,  16,  36 }, {   4,  18,  24 },
  {   4,  18,  36 }, {   4,  22,  24 }, {   4,  22,  36 }, {   4,  24,  36 },
  {   5,   6,  11 }, {   5,   6,  19 }, {   5,   6,  25 }, {   5,   9,  11 },
  {   5,   9,  21 }, {   5,   9,  27 }, {   5,  11,  25 }, {   5,  11,  27 },
  {   5,  15,  19 }, {   5,  15,  21 }, {   5,  15,  37 }, {   5,  19,  25 },
  {   5,  19,  37 }, {   5,  21,  27 }, {   5,  21,  37 }, {   5,  25,  27 },
  {   5,  25,  37 }, {   5,  27,  37 }, {   6,  10,  11 }, {   6,  10,  22 },
  {   6,  10,  28 }, {   6,  11,  25 }, {   6,  11,  28 }, {   6,  16,  19 },
  {   6,  16,  22 }, {   6,  16,  38 }, {   6,  19,  25 }, {   6,  19,  38 },
  {   6,  22,  28 }, {   6,  22,  38 }, {   6,  25,  28 }, {   6,  25,  38 },
  {   6,  28,  38 }, {   7,   8,  12 }, {   7,   8,  20 }, {   7,   8,  26 },
  {   7,   9,  12 }, {   7,   9,  23 }, {   7,   9,  29 }, {   7,  12,  26 },
  {   7,  12,  29 }, {   7,  17,  20 }, {   7,  17,  23 }, {   7,  17,  39 },
  {   7,  20,  26 }, {   7,  20,  39 }, {   7,  23,  29 }, {   7,  23,  39 },
  {   7,  26,  29 }, {   7,  26,  39 }, {   7,  29,  39 }, {   8,  10,  12 },
  {   8,  10,  24 }, {   8,  10,  30 }, {   8,  12,  26 }, {   8,  12,  30 },
  {   8,  18,  20 }, {   8,  18,  24 }, {   8,  18,  40 }, {   8,  20,  26 },
  {   8,  20,  40 }, {   8,  24,  30 }, {   8,  24,  40 }, {   8,  26,  30 },
  {   8,  26,  40 }, {   8,  30,  40 }, {   9,  11,  12 }, {   9,  11,  27 },
  {   9,  11,  31 }, {   9,  12,  29 }, {   9,  12,  31 }, {   9,  21,  23 },
  {   9,  21,  27 }, {   9,  21,  41 }, {   9,  23,  29 }, {   9,  23,  41 },
  {   9,  27,  31 }, {   9,  27,  41 }, {   9,  29,  31 }, {   9,  29,  41 },
  {   9,  31,  41 }, {  10,  11,  12 }, {  10,  11,  28 }, {  10,  11,  32 },
  {  10,  12,  30 }, {  10,  12,  32 }, {  10,  22,  24 }, {  10,  22,  28 },
  {  10,  22,  42 }, {  10,  24,  30 }, {  10,  24,  42 }, {  10,  28,  32 },
  {  10,  28,  42 }, {  10,  30,  32 }, {  10,  30,  42 }, {  10,  32,  42 },
  {  11,  12,  31 }, {  11,  12,  32 }, {  11,  25,  27 }, {  11,  25,  28 },
  {  11,  25,  43 }, {  11,  27,  31 }, {  11,  27,  43 }, {  11,  28,  32 },
  {  11,  28,  43 }, {  11,  31,  32 }, {  11,  31,  43 }, {  11,  32,  43 },
  {  12,  26,  29 }, {  12,  26,  30 }, {  12,  26,  44 }, {  12,  29,  31 },
  {  12,  29,  44 }, {  12,  30,  32 }, {  12,  30,  44 }, {  12,  31,  32 },
  {  12,  31,  44 }, {  12,  32,  44 }, {  13,  14,  33 }, {  13,  14,  34 },
  {  13,  14,  45 }, {  13,  15,  33 }, {  13,  15,  35 }, {  13,  15,  46 },
  {  13,  17,  34 }, {  13,  17,  35 }, {  13,  17,  48 }, {  13,  33,  45 },
  {  13,  33,  46 }, {  13,  34,  45 }, {  13,  34,  48 }, {  13,  35,  46 },
  {  13,  35,  48 }, {  13,  45,  46 }, {  13,  45,  48 }, {  13,  46,  48 },
  {  14,  16,  33 }, {  14,  16,  36 }, {  14,  16,  47 }, {  14,  18,  34 },
  {  14,  18,  36 }, {  14,  18,  49 }, {  14,  33,  45 }, {  14,  33,  47 },
  {  14,  34,  45 }, {  14,  34,  49 }, {  14,  36,  47 }, {  14,  36,  49 },
  {  14,  45,  47 }, {  14,  45,  49 }, {  14,  47,  49 }, {  15,  19,  33 },
  {  15,  19,  37 }, {  15,  19,  50 }, {  15,  21,  35 }, {  15,  21,  37 },
  {  15,  21,  54 }, {  15,  33,  46 }, {  15,  33,  50 }, {  15,  35,  46 },
  {  15,  35,  54 }, {  15,  37,  50 }, {  15,  37,  54 }, {  15,  46,  50 },
  {  15,  46,  54 }, {  15,  50,  54 }, {  16,  19,  33 }, {  16,  19,  38 },
  {  16,  19,  51 }, {  16,  22,  36 }, {  16,  22,  38 }, {  16,  22,  55 },
  {  16,  33,  47 }, {  16,  33,  51 }, {  16,  36,  47 }, {  16,  36,  55 },
  {  16,  38,  51 }, {  16,  38,  55 }, {  16,  47,  51 }, {  16,  47,  55 },
  {  16,  51,  55 }, {  17,  20,  34 }, {  17,  20,  39 }, {  17,  20,  52 },
  {  17,  23,  35 }, {  17,  23,  39 }, {  17,  23,  56 }, {  17,  34,  48 },
  {  17,  34,  52 }, {  17,  35,  48 }, {  17,  35,  56 }, {  17,  39,  52 },
  {  17,  39,  56 }, {  17,  48,  52 }, {  17,  48,  56 }, {  17,  52,  56 },
  {  18,  20,  34 }, {  18,  20,  40 }, {  18,  20,  53 }, {  18,  24,  36 },
  {  18,  24,  40 }, {  18,  24,  57 }, {  18,  34,  49 }, {  18,  34,  53 },
  {  18,  36,  49 }, {  18,  36,  57 }, {  18,  40,  53 }, {  18,  40,  57 },
  {  18,  49,  53 }, {  18,  49,  57 }, {  18,  53,  57 }, {  19,  25,  37 },
  {  19,  25,  38 }, {  19,  25,  58 }, {  19,  33,  50 }, {  19,  33,  51 },
  {  19,  37,  50 }, {  19,  37,  58 }, {  19,  38,  51 }, {  19,  38,  58 },
  {  19,  50,  51 }, {  19,  50,  58 }, {  19,  51,  58 }, {  20,  26,  39 },
  {  20,  26,  40 }, {  20,  26,  61 }, {  20,  34,  52 }, {  20,  34,  53 },
  {  20,  39,  52 }, {  20,  39,  61 }, {  20,  40,  53 }, {  20,  40,  61 },
  {  20,  52,  53 }, {  20,  52,  61 }, {  20,  53,  61 }, {  21,  23,  35 },
  {  21,  23,  41 }, {  21,  23,  59 }, {  21,  27,  37 }, {  21,  27,  41 },
  {  21,  27,  62 }, {  21,  35,  54 }, {  21,  35,  59 }, {  21,  37,  54 },
  {  21,  37,  62 }, {  21,  41,  59 }, {  21,  41,  62 }, {  21,  54,  59 },
  {  21,  54,  62 }, {  21,  59,  62 }, {  22,  24,  36 }, {  22,  24,  42 },
  {  22,  24,  60 }, {  22,  28,  38 }, {  22,  28,  42 }, {  22,  28,  63 },
  {  22,  36,  55 }, {  22,  36,  60 }, {  22,  38,  55 }, {  22,  38,  63 },
  {  22,  42,  60 }, {  22,  42,  63 }, {  22,  55,  60 }, {  22,  55,  63 },
  {  22,  60,  63 }, {  23,  29,  39 }, {  23,  29,  41 }, {  23,  29,  64 },
  {  23,  35,  56 }, {  23,  35,  59 }, {  23,  39,  56 }, {  23,  39,  64 },
  {  23,  41,  59 }, {  23,  41,  64 }, {  23,  56,  59 }, {  23,  56,  64 },
  {  23,  59,  64 }, {  24,  30,  40 }, {  24,  30,  42 }, {  24,  30,  65 },
  {  24,  36,  57 }, {  24,  36,  60 }, {  24,  40,  57 }, {  24,  40,  65 },
  {  24,  42,  60 }, {  24,  42,  65 }, {  24,  57,  60 }, {  24,  57,  65 },
  {  24,  60,  65 }, {  25,  27,  37 }, {  25,  27,  43 }, {  25,  27,  66 },
  {  25,  28,  38 }, {  25,  28,  43 }, {  25,  28,  67 }, {  25,  37,  58 },
  {  25,  37,  66 }, {  25,  38,  58 }, {  25,  38,  67 }, {  25,  43,  66 },
  {  25,  43,  67 }, {  25,  58,  66 }, {  25,  58,  67 }, {  25,  66,  67 },
  {  26,  29,  39 }, {  26,  29,  44 }, {  26,  29,  68 }, {  26,  30,  40 },
  {  26,  30,  44 }, {  26,  30,  69 }, {  26,  39,  61 }, {  26,  39,  68 },
  {  26,  40,  61 }, {  26,  40,  69 }, {  26,  44,  68 }, {  26,  44,  69 },
  {  26,  61,  68 }, {  26,  61,  69 }, {  26,  68,  69 }, {  27,  31,  41 },
  {  27,  31,  43 }, {  27,  31,  70 }, {  27,  37,  62 }, {  27,  37,  66 },
  {  27,  41,  62 }, {  27,  41,  70 }, {  27,  43,  66 }, {  27,  43,  70 },
  {  27,  62,  66 }, {  27,  62,  70 }, {  27,  66,  70 }, {  28,  32,  42 },
  {  28,  32,  43 }, {  28,  32,  71 }, {  28,  38,  63 }, {  28,  38,  67 },
  {  28,  42,  63 }, {  28,  42,  71 }, {  28,  43,  67 }, {  28,  43,  71 },
  {  28,  63,  67 }, {  28,  63,  71 }, {  28,  67,  71 }, {  29,  31,  41 },
  {  29,  31,  44 }, {  29,  31,  72 }, {  29,  39,  64 }, {  29,  39,  68 },
  {  29,  41,  64 }, {  29,  41,  72 }, {  29,  44,  68 }, {  29,  44,  72 },
  {  29,  64,  68 }, {  29,  64,  72 }, {  29,  68,  72 }, {  30,  32,  42 },
  {  30,  32,  44 }, {  30,  32,  73 }, {  30,  40,  65 }, {  30,  40,  69 },
  {  30,  42,  65 }, {  30,  42,  73 }, {  30,  44,  69 }, {  30,  44,  73 },
  {  30,  65,  69 }, {  30,  65,  73 }, {  30,  69,  73 }, {  31,  32,  43 },
  {  31,  32,  44 }, {  31,  32,  74 }, {  31,  41,  70 }, {  31,  41,  72 },
  {  31,  43,  70 }, {  31,  43,  74 }, {  31,  44,  72 }, {  31,  44,  74 },
  {  31,  70,  72 }, {  31,  70,  74 }, {  31,  72,  74 }, {  32,  42,  71 },
  {  32,  42,  73 }, {  32,  43,  71 }, {  32,  43,  74 }, {  32,  44,  73 },
  {  32,  44,  74 }, {  32,  71,  73 }, {  32,  71,  74 }, {  32,  73,  74 },
  {  33,  45,  46 }, {  33,  45,  47 }, {  33,  45,  75 }, {  33,  46,  50 },
  {  33,  46,  75 }, {  33,  47,  51 }, {  33,  47,  75 }, {  33,  50,  51 },
  {  33,  50,  75 }, {  33,  51,  75 }, {  34,  45,  48 }, {  34,  45,  49 },
  {  34,  45,  76 }, {  34,  48,  52 }, {  34,  48,  76 }, {  34,  49,  53 },
  {  34,  49,  76 }, {  34,  52,  53 }, {  34,  52,  76 }, {  34,  53,  76 },
  {  35,  46,  48 }, {  35,  46,  54 }, {  35,  46,  77 }, {  35,  48,  56 },
  {  35,  48,  77 }, {  35,  54,  59 }, {  35,  54,  77 }, {  35,  56,  59 },
  {  35,  56,  77 }, {  35,  59,  77 }, {  36,  47,  49 }, {  36,  47,  55 },
  {  36,  47,  78 }, {  36,  49,  57 }, {  36,  49,  78 }, {  36,  55,  60 },
  {  36,  55,  78 }, {  36,  57,  60 }, {  36,  57,  78 }, {  36,  60,  78 },
  {  37,  50,  54 }, {  37,  50,  58 }, {  37,  50,  79 }, {  37,  54,  62 },
  {  37,  54,  79 }, {  37,  58,  66 }, {  37,  58,  79 }, {  37,  62,  66 },
  {  37,  62,  79 }, {  37,  66,  79 }, {  38,  51,  55 }, {  38,  51,  58 },
  {  38,  51,  80 }, {  38,  55,  63 }, {  38,  55,  80 }, {  38,  58,  67 },
  {  38,  58,  80 }, {  38,  63,  67 }, {  38,  63,  80 }, {  38,  67,  80 },
  {  39,  52,  56 }, {  39,  52,  61 }, {  39,  52,  81 }, {  39,  56,  64 },
  {  39,  56,  81 }, {  39,  61,  68 }, {  39,  61,  81 }, {  39,  64,  68 },
  {  39,  64,  81 }, {  39,  68,  81 }, {  40,  53,  57 }, {  40,  53,  61 },
  {  40,  53,  82 }, {  40,  57,  65 }, {  40,  57,  82 }, {  40,  61,  69 },
  {  40,  61,  82 }, {  40,  65,  69 }, {  40,  65,  82 }, {  40,  69,  82 },
  {  41,  59,  62 }, {  41,  59,  64 }, {  41,  59,  83 }, {  41,  62,  70 },
  {  41,  62,  83 }, {  41,  64,  72 }, {  41,  64,  83 }, {  41,  70,  72 },
  {  41,  70,  83 }, {  41,  72,  83 }, {  42,  60,  63 }, {  42,  60,  65 },
  {  42,  60,  84 }, {  42,  63,  71 }, {  42,  63,  84 }, {  42,  65,  73 },
  {  42,  65,  84 }, {  42,  71,  73 }, {  42,  71,  84 }, {  42,  73,  84 },
  {  43,  66,  67 }, {  43,  66,  70 }, {  43,  66,  85 }, {  43,  67,  71 },
  {  43,  67,  85 }, {  43,  70,  74 }, {  43,  70,  85 }, {  43,  71,  74 },
  {  43,  71,  85 }, {  43,  74,  85 }, {  44,  68,  69 }, {  44,  68,  72 },
  {  44,  68,  86 }, {  44,  69,  73 }, {  44,  69,  86 }, {  44,  72,  74 },
  {  44,  72,  86 }, {  44,  73,  74 }, {  44,  73,  86 }, {  44,  74,  86 },
  {  45,  46,  48 }, {  45,  46,  75 }, {  45,  46,  87 }, {  45,  47,  49 },
  {  45,  47,  75 }, {  45,  47,  88 }, {  45,  48,  76 }, {  45,  48,  87 },
  {  45,  49,  76 }, {  45,  49,  88 }, {  45,  75,  87 }, {  45,  75,  88 },
  {  45,  76,  87 }, {  45,  76,  88 }, {  45,  87,  88 }, {  46,  48,  77 },
  {  46,  48,  87 }, {  46,  50,  54 }, {  46,  50,  75 }, {  46,  50,  89 },
  {  46,  54,  77 }, {  46,  54,  89 }, {  46,  75,  87 }, {  46,  75,  89 },
  {  46,  77,  87 }, {  46,  77,  89 }, {  46,  87,  89 }, {  47,  49,  78 },
  {  47,  49,  88 }, {  47,  51,  55 }, {  47,  51,  75 }, {  47,  51,  90 },
  {  47,  55,  78 }, {  47,  55,  90 }, {  47,  75,  88 }, {  47,  75,  90 },
  {  47,  78,  88 }, {  47,  78,  90 }, {  47,  88,  90 }, {  48,  52,  56 },
  {  48,  52,  76 }, {  48,  52,  91 }, {  48,  56,  77 }, {  48,  56,  91 },
  {  48,  76,  87 }, {  48,  76,  91 }, {  48,  77,  87 }, {  48,  77,  91 },
  {  48,  87,  91 }, {  49,  53,  57 }, {  49,  53,  76 }, {  49,  53,  92 },
  {  49,  57,  78 }, {  49,  57,  92 }, {  49,  76,  88 }, {  49,  76,  92 },
  {  49,  78,  88 }, {  49,  78,  92 }, {  49,  88,  92 }, {  50,  51,  58 },
  {  50,  51,  75 }, {  50,  51,  93 }, {  50,  54,  79 }, {  50,  54,  89 },
  {  50,  58,  79 }, {  50,  58,  93 }, {  50,  75,  89 }, {  50,  75,  93 },
  {  50,  79,  89 }, {  50,  79,  93 }, {  50,  89,  93 }, {  51,  55,  80 },
  {  51,  55,  90 }, {  51,  58,  80 }, {  51,  58,  93 }, {  51,  75,  90 },
  {  51,  75,  93 }, {  51,  80,  90 }, {  51,  80,  93 }, {  51,  90,  93 },
  {  52,  53,  61 }, {  52,  53,  76 }, {  52,  53,  94 }, {  52,  56,  81 },
  {  52,  56,  91 }, {  52,  61,  81 }, {  52,  61,  94 }, {  52,  76,  91 },
  {  52,  76,  94 }, {  52,  81,  91 }, {  52,  81,  94 }, {  52,  91,  94 },
  {  53,  57,  82 }, {  53,  57,  92 }, {  53,  61,  82 }, {  53,  61,  94 },
  {  53,  76,  92 }, {  53,  76,  94 }, {  53,  82,  92 }, {  53,  82,  94 },
  {  53,  92,  94 }, {  54,  59,  62 }, {  54,  59,  77 }, {  54,  59,  95 },
  {  54,  62,  79 }, {  54,  62,  95 }, {  54,  77,  89 }, {  54,  77,  95 },
  {  54,  79,  89 }, {  54,  79,  95 }, {  54,  89,  95 }, {  55,  60,  63 },
  {  55,  60,  78 }, {  55,  60,  96 }, {  55,  63,  80 }, {  55,  63,  96 },
  {  55,  78,  90 }, {  55,  78,  96 }, {  55,  80,  90 }, {  55,  80,  96 },
  {  55,  90,  96 }, {  56,  59,  64 }, {  56,  59,  77 }, {  56,  59,  97 },
  {  56,  64,  81 }, {  56,  64,  97 }, {  56,  77,  91 }, {  56,  77,  97 },
  {  56,  81,  91 }, {  56,  81,  97 }, {  56,  91,  97 }, {  57,  60,  65 },
  {  57,  60,  78 }, {  57,  60,  98 }, {  57,  65,  82 }, {  57,  65,  98 },
  {  57,  78,  92 }, {  57,  78,  98 }, {  57,  82,  92 }, {  57,  82,  98 },
  {  57,  92,  98 }, {  58,  66,  67 }, {  58,  66,  79 }, {  58,  66,  99 },
  {  58,  67,  80 }, {  58,  67,  99 }, {  58,  79,  93 }, {  58,  79,  99 },
  {  58,  80,  93 }, {  58,  80,  99 }, {  58,  93,  99 }, {  59,  62,  83 },
  {  59,  62,  95 }, {  59,  64,  83 }, {  59,  64,  97 }, {  59,  77,  95 },
  {  59,  77,  97 }, {  59,  83,  95 }, {  59,  83,  97 }, {  59,  95,  97 },
  {  60,  63,  84 }, {  60,  63,  96 }, {  60,  65,  84 }, {  60,  65,  98 },
  {  60,  78,  96 }, {  60,  78,  98 }, {  60,  84,  96 }, {  60,  84,  98 },
  {  60,  96,  98 }, {  61,  68,  69 }, {  61,  68,  81 }, {  61,  68, 100 },
  {  61,  69,  82 }, {  61,  69, 100 }, {  61,  81,  94 }, {  61,  81, 100 },
  {  61,  82,  94 }, {  61,  82, 100 }, {  61,  94, 100 }, {  62,  66,  70 },
  {  62,  66,  79 }, {  62,  66, 101 }, {  62,  70,  83 }, {  62,  70, 101 },
  {  62,  79,  95 }, {  62,  79, 101 }, {  62,  83,  95 }, {  62,  83, 101 },
  {  62,  95, 101 }, {  63,  67,  71 }, {  63,  67,  80 }, {  63,  67, 102 },
  {  63,  71,  84 }, {  63,  71, 102 }, {  63,  80,  96 }, {  63,  80, 102 },
  {  63,  84,  96 }, {  63,  84, 102 }, {  63,  96, 102 }, {  64,  68,  72 },
  {  64,  68,  81 }, {  64,  68, 103 }, {  64,  72,  83 }, {  64,  72, 103 },
  {  64,  81,  97 }, {  64,  81, 103 }, {  64,  83,  97 }, {  64,  83, 103 },
  {  64,  97, 103 }, {  65,  69,  73 }, {  65,  69,  82 }, {  65,  69, 104 },
  {  65,  73,  84 }, {  65,  73, 104 }, {  65,  82,  98 }, {  65,  82, 104 },
  {  65,  84,  98 }, {  65,  84, 104 }, {  65,  98, 104 }, {  66,  67,  85 },
  {  66,  67,  99 }, {  66,  70,  85 }, {  66,  70, 101 }, {  66,  79,  99 },
  {  66,  79, 101 }, {  66,  85,  99 }, {  66,  85, 101 }, {  66,  99, 101 },
  {  67,  71,  85 }, {  67,  71, 102 }, {  67,  80,  99 }, {  67,  80, 102 },
  {  67,  85,  99 }, {  67,  85, 102 }, {  67,  99, 102 }, {  68,  69,  86 },
  {  68,  69, 100 }, {  68,  72,  86 }, {  68,  72, 103 }, {  68,  81, 100 },
  {  68,  81, 103 }, {  68,  86, 100 }, {  68,  86, 103 }, {  68, 100, 103 },
  {  69,  73,  86 }, {  69,  73, 104 }, {  69,  82, 100 }, {  69,  82, 104 },
  {  69,  86, 100 }, {  69,  86, 104 }, {  69, 100, 104 }, {  70,  72,  74 },
  {  70,  72,  83 }, {  70,  72, 105 }, {  70,  74,  85 }, {  70,  74, 105 },
  {  70,  83, 101 }, {  70,  83, 105 }, {  70,  85, 101 }, {  70,  85, 105 },
  {  70, 101, 105 }, {  71,  73,  74 }, {  71,  73,  84 }, {  71,  73, 106 },
  {  71,  74,  85 }, {  71,  74, 106 }, {  71,  84, 102 }, {  71,  84, 106 },
  {  71,  85, 102 }, {  71,  85, 106 }, {  71, 102, 106 }, {  72,  74,  86 },
  {  72,  74, 105 }, {  72,  83, 103 }, {  72,  83, 105 }, {  72,  86, 103 },
  {  72,  86, 105 }, {  72, 103, 105 }, {  73,  74,  86 }, {  73,  74, 106 },
  {  73,  84, 104 }, {  73,  84, 106 }, {  73,  86, 104 }, {  73,  86, 106 },
  {  73, 104, 106 }, {  74,  85, 105 }, {  74,  85, 106 }, {  74,  86, 105 },
  {  74,  86, 106 }, {  74, 105, 106 }, {  75,  87,  88 }, {  75,  87,  89 },
  {  75,  87, 107 }, {  75,  88,  90 }, {  75,  88, 107 }, {  75,  89,  93 },
  {  75,  89, 107 }, {  75,  90,  93 }, {  75,  90, 107 }, {  75,  93, 107 },
  {  76,  87,  88 }, {  76,  87,  91 }, {  76,  87, 108 }, {  76,  88,  92 },
  {  76,  88, 108 }, {  76,  91,  94 }, {  76,  91, 108 }, {  76,  92,  94 },
  {  76,  92, 108 }, {  76,  94, 108 }, {  77,  87,  89 }, {  77,  87,  91 },
  {  77,  87, 109 }, {  77,  89,  95 }, {  77,  89, 109 }, {  77,  91,  97 },
  {  77,  91, 109 }, {  77,  95,  97 }, {  77,  95, 109 }, {  77,  97, 109 },
  {  78,  88,  90 }, {  78,  88,  92 }, {  78,  88, 110 }, {  78,  90,  96 },
  {  78,  90, 110 }, {  78,  92,  98 }, {  78,  92, 110 }, {  78,  96,  98 },
  {  78,  96, 110 }, {  78,  98, 110 }, {  79,  89,  93 }, {  79,  89,  95 },
  {  79,  89, 111 }, {  79,  93,  99 }, {  79,  93, 111 }, {  79,  95, 101 },
  {  79,  95, 111 }, {  79,  99, 101 }, {  79,  99, 111 }, {  79, 101, 111 },
  {  80,  90,  93 }, {  80,  90,  96 }, {  80,  90, 112 }, {  80,  93,  99 },
  {  80,  93, 112 }, {  80,  96, 102 }, {  80,  96, 112 }, {  80,  99, 102 },
  {  80,  99, 112 }, {  80, 102, 112 }, {  81,  91,  94 }, {  81,  91,  97 },
  {  81,  91, 113 }, {  81,  94, 100 }, {  81,  94, 113 }, {  81,  97, 103 },
  {  81,  97, 113 }, {  81, 100, 103 }, {  81, 100, 113 }, {  81, 103, 113 },
  {  82,  92,  94 }, {  82,  92,  98 }, {  82,  92, 114 }, {  82,  94, 100 },
  {  82,  94, 114 }, {  82,  98, 104 }, {  82,  98, 114 }, {  82, 100, 104 },
  {  82, 100, 114 }, {  82, 104, 114 }, {  83,  95,  97 }, {  83,  95, 101 },
  {  83,  95, 115 }, {  83,  97, 103 }, {  83,  97, 115 }, {  83, 101, 105 },
  {  83, 101, 115 }, {  83, 103, 105 }, {  83, 103, 115 }, {  83, 105, 115 },
  {  84,  96,  98 }, {  84,  96, 102 }, {  84,  96, 116 }, {  84,  98, 104 },
  {  84,  98, 116 }, {  84, 102, 106 }, {  84, 102, 116 }, {  84, 104, 106 },
  {  84, 104, 116 }, {  84, 106, 116 }, {  85,  99, 101 }, {  85,  99, 102 },
  {  85,  99, 117 }, {  85, 101, 105 }, {  85, 101, 117 }, {  85, 102, 106 },
  {  85, 102, 117 }, {  85, 105, 106 }, {  85, 105, 117 }, {  85, 106, 117 },
  {  86, 100, 103 }, {  86, 100, 104 }, {  86, 100, 118 }, {  86, 103, 105 },
  {  86, 103, 118 }, {  86, 104, 106 }, {  86, 104, 118 }, {  86, 105, 106 },
  {  86, 105, 118 }, {  86, 106, 118 }, {  87,  88, 107 }, {  87,  88, 108 },
  {  87,  89, 107 }, {  87,  89, 109 }, {  87,  91, 108 }, {  87,  91, 109 },
  {  87, 107, 108 }, {  87, 107, 109 }, {  87, 108, 109 }, {  88,  90, 107 },
  {  88,  90, 110 }, {  88,  92, 108 }, {  88,  92, 110 }, {  88, 107, 108 },
  {  88, 107, 110 }, {  88, 108, 110 }, {  89,  93, 107 }, {  89,  93, 111 },
  {  89,  95, 109 }, {  89,  95, 111 }, {  89, 107, 109 }, {  89, 107, 111 },
  {  89, 109, 111 }, {  90,  93, 107 }, {  90,  93, 112 }, {  90,  96, 110 },
  {  90,  96, 112 }, {  90, 107, 110 }, {  90, 107, 112 }, {  90, 110, 112 },
  {  91,  94, 108 }, {  91,  94, 113 }, {  91,  97, 109 }, {  91,  97, 113 },
  {  91, 108, 109 }, {  91, 108, 113 }, {  91, 109, 113 }, {  92,  94, 108 },
  {  92,  94, 114 }, {  92,  98, 110 }, {  92,  98, 114 }, {  92, 108, 110 },
  {  92, 108, 114 }, {  92, 110, 114 }, {  93,  99, 111 }, {  93,  99, 112 },
  {  93, 107, 111 }, {  93, 107, 112 }, {  93, 111, 112 }, {  94, 100, 113 },
  {  94, 100, 114 }, {  94, 108, 113 }, {  94, 108, 114 }, {  94, 113, 114 },
  {  95,  97, 109 }, {  95,  97, 115 }, {  95, 101, 111 }, {  95, 101, 115 },
  {  95, 109, 111 }, {  95, 109, 115 }, {  95, 111, 115 }, {  96,  98, 110 },
  {  96,  98, 116 }, {  96, 102, 112 }, {  96, 102, 116 }, {  96, 110, 112 },
  {  96, 110, 116 }, {  96, 112, 116 }, {  97, 103, 113 }, {  97, 103, 115 },
  {  97, 109, 113 }, {  97, 109, 115 }, {  97, 113, 115 }, {  98, 104, 114 },
  {  98, 104, 116 }, {  98, 110, 114 }, {  98, 110, 116 }, {  98, 114, 116 },
  {  99, 101, 111 }, {  99, 101, 117 }, {  99, 102, 112 }, {  99, 102, 117 },
  {  99, 111, 112 }, {  99, 111, 117 }, {  99, 112, 117 }, { 100, 103, 113 },
  { 100, 103, 118 }, { 100, 104, 114 }, { 100, 104, 118 }, { 100, 113, 114 },
  { 100, 113, 118 }, { 100, 114, 118 }, { 101, 105, 115 }, { 101, 105, 117 },
  { 101, 111, 115 }, { 101, 111, 117 }, { 101, 115, 117 }, { 102, 106, 116 },
  { 102, 106, 117 }, { 102, 112, 116 }, { 102, 112, 117 }, { 102, 116, 117 },
  { 103, 105, 115 }, { 103, 105, 118 }, { 103, 113, 115 }, { 103, 113, 118 },
  { 103, 115, 118 }, { 104, 106, 116 }, { 104, 106, 118 }, { 104, 114, 116 },
  { 104, 114, 118 }, { 104, 116, 118 }, { 105, 106, 117 }, { 105, 106, 118 },
  { 105, 115, 117 }, { 105, 115, 118 }, { 105, 117, 118 }, { 106, 116, 117 },
  { 106, 116, 118 }, { 106, 117, 118 }, { 107, 108, 109 }, { 107, 108, 110 },
  { 107, 108, 119 }, { 107, 109, 111 }, { 107, 109, 119 }, { 107, 110, 112 },
  { 107, 110, 119 }, { 107, 111, 112 }, { 107, 111, 119 }, { 107, 112, 119 },
  { 108, 109, 113 }, { 108, 109, 119 }, { 108, 110, 114 }, { 108, 110, 119 },
  { 108, 113, 114 }, { 108, 113, 119 }, { 108, 114, 119 }, { 109, 111, 115 },
  { 109, 111, 119 }, { 109, 113, 115 }, { 109, 113, 119 }, { 109, 115, 119 },
  { 110, 112, 116 }, { 110, 112, 119 }, { 110, 114, 116 }, { 110, 114, 119 },
  { 110, 116, 119 }, { 111, 112, 117 }, { 111, 112, 119 }, { 111, 115, 117 },
  { 111, 115, 119 }, { 111, 117, 119 }, { 112, 116, 117 }, { 112, 116, 119 },
  { 112, 117, 119 }, { 113, 114, 118 }, { 113, 114, 119 }, { 113, 115, 118 },
  { 113, 115, 119 }, { 113, 118, 119 }, { 114, 116, 118 }, { 114, 116, 119 },
  { 114, 118, 119 }, { 115, 117, 118 }, { 115, 117, 119 }, { 115, 118, 119 },
  { 116, 117, 118 }, { 116, 117, 119 }, { 116, 118, 119 }, { 117, 118, 119 }
};



/* Add a rotation around the wx-plane to the matrix m. */
static void rotatewx(float m[4][4], float phi)
{
  float c, s, u, v;
  int i;

  phi *= M_PI/180.0;
  c = cos(phi);
  s = sin(phi);
  for (i=0; i<4; i++)
  {
    u = m[i][1];
    v = m[i][2];
    m[i][1] = c*u+s*v;
    m[i][2] = -s*u+c*v;
  }
}


/* Add a rotation around the wy-plane to the matrix m. */
static void rotatewy(float m[4][4], float phi)
{
  float c, s, u, v;
  int i;

  phi *= M_PI/180.0;
  c = cos(phi);
  s = sin(phi);
  for (i=0; i<4; i++)
  {
    u = m[i][0];
    v = m[i][2];
    m[i][0] = c*u-s*v;
    m[i][2] = s*u+c*v;
  }
}


/* Add a rotation around the wz-plane to the matrix m. */
static void rotatewz(float m[4][4], float phi)
{
  float c, s, u, v;
  int i;

  phi *= M_PI/180.0;
  c = cos(phi);
  s = sin(phi);
  for (i=0; i<4; i++)
  {
    u = m[i][0];
    v = m[i][1];
    m[i][0] = c*u+s*v;
    m[i][1] = -s*u+c*v;
  }
}


/* Add a rotation around the xy-plane to the matrix m. */
static void rotatexy(float m[4][4], float phi)
{
  float c, s, u, v;
  int i;

  phi *= M_PI/180.0;
  c = cos(phi);
  s = sin(phi);
  for (i=0; i<4; i++)
  {
    u = m[i][2];
    v = m[i][3];
    m[i][2] = c*u+s*v;
    m[i][3] = -s*u+c*v;
  }
}


/* Add a rotation around the xz-plane to the matrix m. */
static void rotatexz(float m[4][4], float phi)
{
  float c, s, u, v;
  int i;

  phi *= M_PI/180.0;
  c = cos(phi);
  s = sin(phi);
  for (i=0; i<4; i++)
  {
    u = m[i][1];
    v = m[i][3];
    m[i][1] = c*u-s*v;
    m[i][3] = s*u+c*v;
  }
}


/* Add a rotation around the yz-plane to the matrix m. */
static void rotateyz(float m[4][4], float phi)
{
  float c, s, u, v;
  int i;

  phi *= M_PI/180.0;
  c = cos(phi);
  s = sin(phi);
  for (i=0; i<4; i++)
  {
    u = m[i][0];
    v = m[i][3];
    m[i][0] = c*u-s*v;
    m[i][3] = s*u+c*v;
  }
}


/* Compute the rotation matrix m from the rotation angles. */
static void rotateall(float al, float be, float de, float ze, float et,
                      float th, float m[4][4])
{
  int i, j;

  for (i=0; i<4; i++)
    for (j=0; j<4; j++)
      m[i][j] = (i==j);
  rotatewx(m,al);
  rotatewy(m,be);
  rotatewz(m,de);
  rotatexz(m,et);
  rotatexy(m,ze);
  rotateyz(m,th);
}


/* Multiply two rotation matrices: o=m*n. */
static void mult_rotmat(float m[4][4], float n[4][4], float o[4][4])
{
  int i, j, k;

  for (i=0; i<4; i++)
  {
    for (j=0; j<4; j++)
    {
      o[i][j] = 0.0;
      for (k=0; k<4; k++)
        o[i][j] += m[i][k]*n[k][j];
    }
  }
}


/* Compute a 4D rotation matrix from two unit quaternions. */
static void quats_to_rotmat(float p[4], float q[4], float m[4][4])
{
  double al, be, de, ze, et, th;
  double r00, r01, r02, r12, r22;

  r00 = 1.0-2.0*(p[1]*p[1]+p[2]*p[2]);
  r01 = 2.0*(p[0]*p[1]+p[2]*p[3]);
  r02 = 2.0*(p[2]*p[0]-p[1]*p[3]);
  r12 = 2.0*(p[1]*p[2]+p[0]*p[3]);
  r22 = 1.0-2.0*(p[1]*p[1]+p[0]*p[0]);

  al = atan2(-r12,r22)*180.0/M_PI;
  be = atan2(r02,sqrt(r00*r00+r01*r01))*180.0/M_PI;
  de = atan2(-r01,r00)*180.0/M_PI;

  r00 = 1.0-2.0*(q[1]*q[1]+q[2]*q[2]);
  r01 = 2.0*(q[0]*q[1]+q[2]*q[3]);
  r02 = 2.0*(q[2]*q[0]-q[1]*q[3]);
  r12 = 2.0*(q[1]*q[2]+q[0]*q[3]);
  r22 = 1.0-2.0*(q[1]*q[1]+q[0]*q[0]);

  et = atan2(-r12,r22)*180.0/M_PI;
  th = atan2(r02,sqrt(r00*r00+r01*r01))*180.0/M_PI;
  ze = atan2(-r01,r00)*180.0/M_PI;

  rotateall(al,be,de,ze,et,-th,m);
}


/* Compute the normal vector of a plane based on three points in the plane. */
static void normal(const float *p, const float *q, const float *r, 
                   float *n)
{
  float u[3], v[3], t;

  u[0] = q[0]-p[0];
  u[1] = q[1]-p[1];
  u[2] = q[2]-p[2];
  v[0] = r[0]-p[0];
  v[1] = r[1]-p[1];
  v[2] = r[2]-p[2];
  n[0] = u[1]*v[2]-u[2]*v[1];
  n[1] = u[2]*v[0]-u[0]*v[2];
  n[2] = u[0]*v[1]-u[1]*v[0];
  t = sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
  n[0] /= t;
  n[1] /= t;
  n[2] /= t;
}


/* Project an array of vertices from 4d to 3d. */
static void project(ModeInfo *mi, const float vert[][4], float v[][4], int num)
{
  float s, q1[4], q2[4], r1[4][4], r2[4][4], m[4][4];
  int i, j, k;
  polytopesstruct *pp = &poly[MI_SCREEN(mi)];

  rotateall(pp->alpha,pp->beta,pp->delta,pp->zeta,pp->eta,pp->theta,r1);

  gltrackball_get_quaternion(pp->trackballs[0],q1);
  gltrackball_get_quaternion(pp->trackballs[1],q2);
  quats_to_rotmat(q1,q2,r2);

  mult_rotmat(r2,r1,m);

  /* Project the vertices from 4d to 3d. */
  for (i=0; i<num; i++)
  {
    for (j=0; j<4; j++)
    {
      s = 0.0;
      for (k=0; k<4; k++)
        s += m[j][k]*vert[i][k];
      v[i][j] = s+offset4d[j];
    }
    if (projection_4d == DISP_4D_ORTHOGRAPHIC)
    {
      for (j=0; j<3; j++)
        v[i][j] /= 2.0;
      v[i][3] = 1.0;
    }
    else
    {
      for (j=0; j<4; j++)
        v[i][j] /= v[i][3];
    }
  }

  /* Move the projected vertices along the z-axis so that they become visible
     in the viewport. */
  for (i=0; i<num; i++)
  {
    for (j=0; j<4; j++)
      v[i][j] += offset3d[j];
  }
}


/* Draw a single polytope. */
static void draw(ModeInfo *mi,
                 float v[][4], 
                 const int edge[][2], int num_edge, 
                 const int face[], int num_face, 
                 int vert_per_face, 
                 float edge_color[][4], 
                 float face_color[][4], 
                 float face_color_trans[][4])
{
  int i, j;
  float n[3];
  GLfloat red[4] = { 1.0, 0.0, 0.0, 1.0 };
  GLfloat red_trans[4] = { 1.0, 0.0, 0.0, 1.0 };

  mi->polygon_count = 0;
  if (display_mode == DISP_WIREFRAME)
  {
    if (color_mode == COLORS_SINGLE)
      glColor4fv(red);
    glBegin(GL_LINES);
    for (i=0; i<num_edge; i++)
    {
      if (color_mode == COLORS_DEPTH)
        glColor4fv(edge_color[i]);
      glVertex4fv(v[edge[i][0]]);
      glVertex4fv(v[edge[i][1]]);
    }
    glEnd();
    mi->polygon_count = num_edge;
  }
  else
  {
    if (color_mode == COLORS_SINGLE)
    {
      red_trans[3] = face_color_trans[0][3]/2.0;
      if (display_mode == DISP_TRANSPARENT)
        glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,red_trans);
      else
        glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,red);
    }
    for (i=0; i<num_face; i++)
    {
      glBegin(GL_POLYGON);
      if (color_mode == COLORS_DEPTH)
      {
        if (display_mode == DISP_TRANSPARENT)
          glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,
                       face_color_trans[i]);
        else
          glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,
                       face_color[i]);
      }
      normal(v[face[i*vert_per_face]],v[face[i*vert_per_face+1]],
             v[face[i*vert_per_face+2]],n);
      glNormal3fv(n);
      for (j=0; j<vert_per_face; j++)
        glVertex4fv(v[face[i*vert_per_face+j]]);
      glEnd();
    }
    mi->polygon_count = num_face;
  }
}


/* Draw a 5-cell {3,3,3} projected into 3d. */
static void cell_5(ModeInfo *mi)
{
  polytopesstruct *hp = &poly[MI_SCREEN(mi)];
  float v[NUM_VERT_5][4];

  project(mi,vert_5,v,NUM_VERT_5);
  draw(mi,v,edge_5,NUM_EDGE_5,(int *)face_5,NUM_FACE_5,
       VERT_PER_FACE_5,hp->edge_color_5,hp->face_color_5,
       hp->face_color_trans_5);
}


/* Draw a 8-cell {4,3,3} projected into 3d. */
static void cell_8(ModeInfo *mi)
{
  polytopesstruct *hp = &poly[MI_SCREEN(mi)];
  float v[NUM_VERT_8][4];

  project(mi,vert_8,v,NUM_VERT_8);
  draw(mi,v,edge_8,NUM_EDGE_8,(int *)face_8,NUM_FACE_8,
       VERT_PER_FACE_8,hp->edge_color_8,hp->face_color_8,
       hp->face_color_trans_8);
}


/* Draw a 16-cell {3,3,4} projected into 3d. */
static void cell_16(ModeInfo *mi)
{
  polytopesstruct *hp = &poly[MI_SCREEN(mi)];
  float v[NUM_VERT_16][4];

  project(mi,vert_16,v,NUM_VERT_16);
  draw(mi,v,edge_16,NUM_EDGE_16,(int *)face_16,NUM_FACE_16,
       VERT_PER_FACE_16,hp->edge_color_16,hp->face_color_16,
       hp->face_color_trans_16);
}


/* Draw a 24-cell {3,4,3} projected into 3d. */
static void cell_24(ModeInfo *mi)
{
  polytopesstruct *hp = &poly[MI_SCREEN(mi)];
  float v[NUM_VERT_24][4];

  project(mi,vert_24,v,NUM_VERT_24);
  draw(mi,v,edge_24,NUM_EDGE_24,(int *)face_24,NUM_FACE_24,
       VERT_PER_FACE_24,hp->edge_color_24,hp->face_color_24,
       hp->face_color_trans_24);
}


/* Draw a 120-cell {5,3,3} projected into 3d. */
static void cell_120(ModeInfo *mi)
{
  polytopesstruct *hp = &poly[MI_SCREEN(mi)];
  float v[NUM_VERT_120][4];

  project(mi,vert_120,v,NUM_VERT_120);
  draw(mi,v,edge_120,NUM_EDGE_120,(int *)face_120,NUM_FACE_120,
       VERT_PER_FACE_120,hp->edge_color_120,hp->face_color_120,
       hp->face_color_trans_120);
}


/* Draw a 600-cell {3,3,5} projected into 3d. */
static void cell_600(ModeInfo *mi)
{
  polytopesstruct *hp = &poly[MI_SCREEN(mi)];
  float v[NUM_VERT_600][4];

  project(mi,vert_600,v,NUM_VERT_600);
  draw(mi,v,edge_600,NUM_EDGE_600,(int *)face_600,NUM_FACE_600,
       VERT_PER_FACE_600,hp->edge_color_600,hp->face_color_600,
       hp->face_color_trans_600);
}


/* Compute a color based on the w-depth of a point. */
static void color(float depth, float alpha, float min, float max,
                  float color[4])
{
  double d, t;
  int s;

  d = (depth-min)/(max-min);
  s = floor(d*4.0);
  t = d*4.0-s;
  if (s < 0)
    s += 6;
  if (s >= 6)
    s -= 6;
  switch (s)
  {
    case 0:
      color[0] = 1.0;
      color[1] = t;
      color[2] = 0.0;
      break;
    case 1:
      color[0] = 1.0-t;
      color[1] = 1.0;
      color[2] = 0.0;
      break;
    case 2:
      color[0] = 0.0;
      color[1] = 1.0;
      color[2] = t;
      break;
    case 3:
      color[0] = 0.0;
      color[1] = 1.0-t;
      color[2] = 1.0;
      break;
    case 4:
      color[0] = t;
      color[1] = 0.0;
      color[2] = 1.0;
      break;
    case 5:
      color[0] = 1.0;
      color[1] = 0.0;
      color[2] = 1.0-t;
      break;
  }
  color[3] = alpha;
}


/* Set the colors of a single polytope's edges and faces. */
static void colors(const float vert[][4], 
                   const int edge[][2], int num_edge, 
                   const int face[], int num_face, 
                   int vert_per_face, 
                   float edge_color[][4], 
                   float face_color[][4],
                   float face_color_trans[][4],
                   float alpha, 
                   float min_edge_depth, float max_edge_depth,
                   float min_face_depth, float max_face_depth)
{
  int i, j;
  float depth;

  for (i=0; i<num_edge; i++)
  {
    depth = (vert[edge[i][0]][3]+vert[edge[i][1]][3])/2.0;
    color(depth,1.0,min_edge_depth,max_edge_depth,edge_color[i]);
  }
  for (i=0; i<num_face; i++)
  {
    depth = 0.0;
    for (j=0; j<vert_per_face; j++)
      depth += vert[face[i*vert_per_face+j]][3];
    depth /= vert_per_face;
    color(depth,1.0,min_face_depth,max_face_depth,face_color[i]);
    color(depth,alpha,min_face_depth,max_face_depth,face_color_trans[i]);
  }
}


/* Set the colors of the polytopes' edges and faces. */
static void set_colors(ModeInfo *mi)
{
  polytopesstruct *hp = &poly[MI_SCREEN(mi)];
  /* 5-cell. */
  colors(vert_5,edge_5,NUM_EDGE_5,(int *)face_5,NUM_FACE_5,
         VERT_PER_FACE_5,hp->edge_color_5,hp->face_color_5,
         hp->face_color_trans_5,0.5,MIN_EDGE_DEPTH_5,
         MAX_EDGE_DEPTH_5,MIN_FACE_DEPTH_5,MAX_FACE_DEPTH_5);

  /* 8-cell. */
  colors(vert_8,edge_8,NUM_EDGE_8,(int *)face_8,NUM_FACE_8,
         VERT_PER_FACE_8,hp->edge_color_8,hp->face_color_8,
         hp->face_color_trans_8,0.4,MIN_EDGE_DEPTH_8,
         MAX_EDGE_DEPTH_8,MIN_FACE_DEPTH_8,MAX_FACE_DEPTH_8);

  /* 16-cell. */
  colors(vert_16,edge_16,NUM_EDGE_16,(int *)face_16,NUM_FACE_16,
         VERT_PER_FACE_16,hp->edge_color_16,hp->face_color_16,
         hp->face_color_trans_16,0.25,MIN_EDGE_DEPTH_16,
         MAX_EDGE_DEPTH_16,MIN_FACE_DEPTH_16,MAX_FACE_DEPTH_16);

  /* 24-cell. */
  colors(vert_24,edge_24,NUM_EDGE_24,(int *)face_24,NUM_FACE_24,
         VERT_PER_FACE_24,hp->edge_color_24,hp->face_color_24,
         hp->face_color_trans_24,0.25,MIN_EDGE_DEPTH_24,
         MAX_EDGE_DEPTH_24,MIN_FACE_DEPTH_24,MAX_FACE_DEPTH_24);

  /* 120-cell. */
  colors(vert_120,edge_120,NUM_EDGE_120,(int *)face_120,NUM_FACE_120,
         VERT_PER_FACE_120,hp->edge_color_120,hp->face_color_120,
         hp->face_color_trans_120,0.15,MIN_EDGE_DEPTH_120,
         MAX_EDGE_DEPTH_120,MIN_FACE_DEPTH_120,MAX_FACE_DEPTH_120);

  /* 600-cell. */
  colors(vert_600,edge_600,NUM_EDGE_600,(int *)face_600,NUM_FACE_600,
         VERT_PER_FACE_600,hp->edge_color_600,hp->face_color_600,
         hp->face_color_trans_600,0.06,MIN_EDGE_DEPTH_600,
         MAX_EDGE_DEPTH_600,MIN_FACE_DEPTH_600,MAX_FACE_DEPTH_600);
}


static void init(ModeInfo *mi)
{
  polytopesstruct *pp = &poly[MI_SCREEN(mi)];

  set_colors(mi);

  pp->alpha = 0.0;
  pp->beta = 0.0;
  pp->delta = 0.0;
  pp->zeta = 0.0;
  pp->eta = 0.0;
  pp->theta = 0.0;

  pp->tick = 0;
  pp->poly = 0;
}


/* Redisplay the polytopes. */
static void display_polytopes(ModeInfo *mi)
{
  polytopesstruct *pp = &poly[MI_SCREEN(mi)];

  if (!pp->button_pressed)
  {
    pp->alpha += speed_wx * pp->speed_scale;
    if (pp->alpha >= 360.0)
      pp->alpha -= 360.0;
    pp->beta += speed_wy * pp->speed_scale;
    if (pp->beta >= 360.0)
      pp->beta -= 360.0;
    pp->delta += speed_wz * pp->speed_scale;
    if (pp->delta >= 360.0)
      pp->delta -= 360.0;
    pp->zeta += speed_xy * pp->speed_scale;
    if (pp->zeta >= 360.0)
      pp->zeta -= 360.0;
    pp->eta += speed_xz * pp->speed_scale;
    if (pp->eta >= 360.0)
      pp->eta -= 360.0;
    pp->theta += speed_yz * pp->speed_scale;
    if (pp->theta >= 360.0)
      pp->theta -= 360.0;
  }

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  if (projection_3d == DISP_3D_ORTHOGRAPHIC)
  {
    if (pp->aspect >= 1.0)
      glOrtho(-pp->aspect,pp->aspect,-1.0,1.0,0.1,100.0);
    else
      glOrtho(-1.0,1.0,-1.0/pp->aspect,1.0/pp->aspect,0.1,100.0);
  }
  else
  {
    gluPerspective(60.0,pp->aspect,0.1,100.0);
  }

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  if (pp->tick == 0)
  {
    if (polytope == POLYTOPE_RANDOM)
      pp->poly = random() % (POLYTOPE_LAST_CELL+1);
    else
      pp->poly = polytope;
  }
  if (++pp->tick > 1000)
    pp->tick = 0;

  if (pp->poly == POLYTOPE_5_CELL)
    cell_5(mi);
  else if (pp->poly == POLYTOPE_8_CELL)
    cell_8(mi);
  else if (pp->poly == POLYTOPE_16_CELL)
    cell_16(mi);
  else if (pp->poly == POLYTOPE_24_CELL)
    cell_24(mi);
  else if (pp->poly == POLYTOPE_120_CELL)
    cell_120(mi);
  else if (pp->poly == POLYTOPE_600_CELL)
    cell_600(mi);
  else
    abort();
}


ENTRYPOINT void reshape_polytopes(ModeInfo *mi, int width, int height)
{
  polytopesstruct *pp = &poly[MI_SCREEN(mi)];

  pp->WindW = (GLint)width;
  pp->WindH = (GLint)height;
  glViewport(0,0,width,height);
  pp->aspect = (GLfloat)width/(GLfloat)height;
}


ENTRYPOINT Bool polytopes_handle_event(ModeInfo *mi, XEvent *event)
{
  polytopesstruct *pp = &poly[MI_SCREEN(mi)];
  KeySym  sym = 0;
  char c = 0;

  if (event->xany.type == KeyPress || event->xany.type == KeyRelease)
    XLookupString (&event->xkey, &c, 1, &sym, 0);

  if (event->xany.type == ButtonPress &&
      event->xbutton.button == Button1)
  {
    pp->button_pressed = True;
    gltrackball_start(pp->trackballs[pp->current_trackball],
                      event->xbutton.x, event->xbutton.y,
                      MI_WIDTH(mi), MI_HEIGHT(mi));
    return True;
  }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
  {
    pp->button_pressed = False;
    return True;
  }
  else if (event->xany.type == KeyPress)
  {
    if (sym == XK_Shift_L || sym == XK_Shift_R)
    {
      pp->current_trackball = 1;
      if (pp->button_pressed)
        gltrackball_start(pp->trackballs[pp->current_trackball],
                          event->xbutton.x, event->xbutton.y,
                          MI_WIDTH(mi), MI_HEIGHT(mi));
      return True;
    }
  }
  else if (event->xany.type == KeyRelease)
  {
    if (sym == XK_Shift_L || sym == XK_Shift_R)
    {
      pp->current_trackball = 0;
      if (pp->button_pressed)
        gltrackball_start(pp->trackballs[pp->current_trackball],
                          event->xbutton.x, event->xbutton.y,
                          MI_WIDTH(mi), MI_HEIGHT(mi));
      return True;
    }
  }
  else if (event->xany.type == MotionNotify && pp->button_pressed)
  {
    gltrackball_track(pp->trackballs[pp->current_trackball],
                      event->xmotion.x, event->xmotion.y,
                      MI_WIDTH(mi), MI_HEIGHT(mi));
    return True;
  }

  return False;
}


/*
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 *    Xlock hooks.
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 */

/*
 *-----------------------------------------------------------------------------
 *    Initialize polytopes.  Called each time the window changes.
 *-----------------------------------------------------------------------------
 */

ENTRYPOINT void init_polytopes(ModeInfo *mi)
{
  polytopesstruct *pp;

  if (poly == NULL)
  {
    poly = (polytopesstruct *)calloc(MI_NUM_SCREENS(mi),
                                     sizeof(polytopesstruct));
    if (poly == NULL)
      return;
  }
  pp = &poly[MI_SCREEN(mi)];

  pp->trackballs[0] = gltrackball_init(True);
  pp->trackballs[1] = gltrackball_init(True);
  pp->current_trackball = 0;
  pp->button_pressed = False;

  /* Set the display mode. */
  if (!strcasecmp(mode,"wireframe") || !strcasecmp(mode,"0"))
  {
    display_mode = DISP_WIREFRAME;
  }
  else if (!strcasecmp(mode,"surface") || !strcasecmp(mode,"1"))
  {
    display_mode = DISP_SURFACE;
  }
  else if (!strcasecmp(mode,"transparent") || !strcasecmp(mode,"2"))
  {
    display_mode = DISP_TRANSPARENT;
  }
  else
  {
    display_mode = DISP_TRANSPARENT;
  }

  /* Set the Klein bottle. */
  if (!strcasecmp(poly_str,"random") || !strcasecmp(poly_str,"-1"))
  {
    polytope = POLYTOPE_RANDOM;
  }
  else if (!strcasecmp(poly_str,"5-cell") || !strcasecmp(poly_str,"0"))
  {
    polytope = POLYTOPE_5_CELL;
  }
  else if (!strcasecmp(poly_str,"8-cell") || !strcasecmp(poly_str,"1"))
  {
    polytope = POLYTOPE_8_CELL;
  }
  else if (!strcasecmp(poly_str,"16-cell") || !strcasecmp(poly_str,"2"))
  {
    polytope = POLYTOPE_16_CELL;
  }
  else if (!strcasecmp(poly_str,"24-cell") || !strcasecmp(poly_str,"3"))
  {
    polytope = POLYTOPE_24_CELL;
  }
  else if (!strcasecmp(poly_str,"120-cell") || !strcasecmp(poly_str,"4"))
  {
    polytope = POLYTOPE_120_CELL;
  }
  else if (!strcasecmp(poly_str,"600-cell") || !strcasecmp(poly_str,"5"))
  {
    polytope = POLYTOPE_600_CELL;
  }
  else
  {
    polytope = POLYTOPE_RANDOM;
  }

  /* Set the color mode. */
  if (!strcasecmp(color_str,"single") || !strcasecmp(color_str,"0"))
  {
    color_mode = COLORS_SINGLE;
  }
  else if (!strcasecmp(color_str,"depth") || !strcasecmp(color_str,"1"))
  {
    color_mode = COLORS_DEPTH;
  }
  else
  {
    color_mode = COLORS_DEPTH;
  }

  /* Set the 3d projection mode. */
  if (!strcasecmp(proj_3d,"perspective") || !strcasecmp(proj_3d,"0"))
  {
    projection_3d = DISP_3D_PERSPECTIVE;
  }
  else if (!strcasecmp(proj_3d,"orthographic") || !strcasecmp(proj_3d,"1"))
  {
    projection_3d = DISP_3D_ORTHOGRAPHIC;
  }
  else
  {
    projection_3d = DISP_3D_PERSPECTIVE;
  }

  /* Set the 4d projection mode. */
  if (!strcasecmp(proj_4d,"perspective") || !strcasecmp(proj_4d,"0"))
  {
    projection_4d = DISP_4D_PERSPECTIVE;
  }
  else if (!strcasecmp(proj_4d,"orthographic") || !strcasecmp(proj_4d,"1"))
  {
    projection_4d = DISP_4D_ORTHOGRAPHIC;
  }
  else
  {
    projection_4d = DISP_4D_PERSPECTIVE;
  }

  /* make multiple screens rotate at slightly different rates. */
  pp->speed_scale = 0.9 + frand(0.3);

  if ((pp->glx_context = init_GL(mi)) != NULL)
  {
    reshape_polytopes(mi,MI_WIDTH(mi),MI_HEIGHT(mi));
    glDrawBuffer(GL_BACK);
    init(mi);
  }
  else
  {
    MI_CLEARWINDOW(mi);
  }
}

/*
 *-----------------------------------------------------------------------------
 *    Called by the mainline code periodically to update the display.
 *-----------------------------------------------------------------------------
 */
ENTRYPOINT void draw_polytopes(ModeInfo *mi)
{
  static const GLfloat light_ambient[]  = { 0.0, 0.0, 0.0, 1.0 };
  static const GLfloat light_diffuse[]  = { 1.0, 1.0, 1.0, 1.0 };
  static const GLfloat light_specular[] = { 0.0, 0.0, 0.0, 1.0 };
  static const GLfloat light_position[] = { 0.0, 0.0, 1.0, 0.0 };
  static const GLfloat mat_specular[]   = { 1.0, 1.0, 1.0, 1.0 };

  Display         *display = MI_DISPLAY(mi);
  Window          window = MI_WINDOW(mi);
  polytopesstruct *hp;

  if (poly == NULL)
    return;
  hp = &poly[MI_SCREEN(mi)];

  MI_IS_DRAWN(mi) = True;
  if (!hp->glx_context)
    return;

  glXMakeCurrent(display,window,*(hp->glx_context));


  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  if (projection_3d == DISP_3D_PERSPECTIVE)
    gluPerspective(60.0,1.0,0.1,100.0);
  else
    glOrtho(-1.0,1.0,-1.0,1.0,0.1,100.0);;
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  if (display_mode == DISP_WIREFRAME)
  {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
    glDisable(GL_BLEND);
  }
  else if (display_mode == DISP_SURFACE)
  {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0,GL_AMBIENT,light_ambient);
    glLightfv(GL_LIGHT0,GL_DIFFUSE,light_diffuse);
    glLightfv(GL_LIGHT0,GL_SPECULAR,light_specular);
    glLightfv(GL_LIGHT0,GL_POSITION,light_position);
    glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,mat_specular);
    glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,0.0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
  }
  else if (display_mode == DISP_TRANSPARENT)
  {
    glDisable(GL_DEPTH_TEST);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0,GL_AMBIENT,light_ambient);
    glLightfv(GL_LIGHT0,GL_DIFFUSE,light_diffuse);
    glLightfv(GL_LIGHT0,GL_SPECULAR,light_specular);
    glLightfv(GL_LIGHT0,GL_POSITION,light_position);
    glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,mat_specular);
    glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,0.0);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE);
  }
  else
  {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
    glDisable(GL_BLEND);
  }


  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();

  display_polytopes(mi);

  if (MI_IS_FPS(mi))
    do_fps (mi);

  glFlush();

  glXSwapBuffers(display,window);
}


/*
 *-----------------------------------------------------------------------------
 *    The display is being taken away from us.  Free up malloc'ed 
 *      memory and X resources that we've alloc'ed.  Only called
 *      once, we must zap everything for every screen.
 *-----------------------------------------------------------------------------
 */

ENTRYPOINT void release_polytopes(ModeInfo *mi)
{
  if (poly != NULL)
  {
    int screen;

    for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
    {
      polytopesstruct *hp = &poly[screen];

      if (hp->glx_context)
        hp->glx_context = (GLXContext *)NULL;
    }
    (void) free((void *)poly);
    poly = (polytopesstruct *)NULL;
  }
  FreeAllGL(mi);
}

#ifndef STANDALONE
ENTRYPOINT void change_polytopes(ModeInfo *mi)
{
  polytopesstruct *hp = &poly[MI_SCREEN(mi)];

  if (!hp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi),MI_WINDOW(mi),*(hp->glx_context));
  init(mi);
}
#endif /* !STANDALONE */

XSCREENSAVER_MODULE ("Polytopes", polytopes)

#endif /* USE_GL */
