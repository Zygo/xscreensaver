/* platonicfolding --- Displays the unfolding and folding of the Platonic
   solids. */

#if 0
static const char sccsid[] = "@(#)platonicfolding.c  1.1 25/03/18 xlockmore";
#endif

/* Copyright (c) 2025-2026 Carsten Steger <carsten@mirsanmir.org>. */

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
 * C. Steger - 25/03/18: Initial version
 * C. Steger - 26/01/02: Make the code work in an OpenGL core profile
 */

/*
 * This program shows the unfolding and folding of the Platonic
 * solids.  For the five Platonic solids (the tetrahedron, cube,
 * octahedron, dodecahedron, and icosahedron), all unfoldings of its
 * faces are non-overlapping: they form a net. The tetrahedron has 16
 * unfoldings, of which two are essentially different
 * (non-isomorphic), the cube and octahedron each have 384 unfoldings,
 * of which eleven are non-isomorphic, and the dodecahedron and
 * icosahedron each have 5,184,000 unfoldings, of which 43,380 are
 * non-isomorphic. This program displays randomly selected unfoldings
 * for the five Platonic solids.  Note that while it is guaranteed
 * that the nets of the Platonic solids are non-overlapping, their
 * faces occasionally intersect during the unfolding and folding.
 *
 * The program displays the Platonic solids either using different
 * colors for each face (face colors) or with a illuminated view of
 * the earth (earth colors).  When using face colors, the colors of
 * the faces are randomly chosen each time a new Platonic solid is
 * selected.  When using earth colors, the Platonic solid is displayed
 * as if the sphere of the earth were illuminated with the current
 * position of the sun at the time the program is run.  The hemisphere
 * the sun is currently illuminating is displayed with a satellite
 * image of the earth by day and the other hemisphere is displayed
 * with a satellite image of the earth by night.  The specular
 * highlight on the illuminated hemisphere (which is only shown over
 * bodies of water) is the subsolar point (the point on earth above
 * which the sun is perpendicular).  The earth's sphere is then
 * projected onto the Platonic solid via a gnomonic projection.  The
 * program randomly selects whether the north pole or the south pole
 * is facing upwards.  The inside of the earth is displayed with a
 * magma-like texture.
 *
 * At the beginning of each cycle, the program selects one of the five
 * Platonic solids randomly and moves it to the center of the screen.
 * It then repeatedly selects a random net of the polyhedron and
 * unfolds and folds the polyhedron.  The unfolding and folding can
 * occur around each edge of the net successively or around all edges
 * simultaneously.  At the end of each cycle, the Platonic solid is
 * moved offscreen and the next cycle begins.
 *
 * While the Platonic solid is moved on the screen or is unfolded or
 * folded, it is rotated by default.  If earth colors are used, the
 * rotation is always performed in the direction the earth is rotating
 * (counterclockwise as viewed from the north pole towards the center
 * of the earth).  This rotation optionally can be switched off.
 */


#define DEF_ROTATE   "True"
#define DEF_COLORS   "random"
#define DEF_FOLDINGS "random"

#define COLORS_FACE  0
#define COLORS_EARTH 1
#define NUM_COLORS   2

#define RANDOM_NUM_FOLDINGS -1

#ifdef STANDALONE
# define DEFAULTS           "*delay:       25000 \n" \
                            "*showFPS:     False \n" \
                            "*prefersGLSL: True \n" \

# define release_platonicfolding 0
# include "xlockmore.h"         /* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"             /* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL

/* The possible animation states. */
#define ANIM_INIT       0
#define ANIM_APPEAR     1
#define ANIM_DISAPPEAR  2
#define ANIM_UNFOLD_JNT 3
#define ANIM_FOLD_JNT   4
#define ANIM_UNFOLD_SEP 5
#define ANIM_FOLD_SEP   6

/* The possible easing functions. */
#define EASING_QUINTIC 0
#define EASING_ACCEL   1
#define EASING_DECEL   2

/* Hash size for the Perlin noise generator. */
#define HASH_SIZE 256

/* Number of octaves of the Perlin noise generator to use for the magma
   noise texture. */
#define NUM_OCTAVES 4

/* Frequency of the first octave of the Perlin noise generator to use for
   the magma noise texture. */
#define START_FREQUENCY 4

/* Size of one dimension of the 3D magma noise texture. */
#define MAGMA_TEX_SIZE  128

/* Gamma value to use for the magma look-up table. */
#define MAGMA_LUT_GAMMA (1.0f/1.5f)

#ifndef M_PI
#define M_PI            3.14159265358979323846
#endif
#define M_PI_F          3.14159265359f
#define SQRT_2_2_F      0.70710678119f
#define SQRT_2_4_F      0.35355339059f
#define SQRT_3_2_F      0.86602540379f
#define COS_36_F        0.80901699438f
#define SIN_36_F        0.58778525229f
#define COS_72_F        0.30901699438f
#define SIN_72_F        0.95105651630f
#define DODECA_IN_RAD_F 1.30901699437f
#define ICOSA_IN_RAD_F  1.30901699437f

/* The maximum stack size for the traversal of the polygon tree that
   represents the folding and unfolding of a Platonic solid. */
#define MAX_STACK 20

/* The maximum number of folding angles. */
#define NUM_ANGLES 19

/* The maximum number of vertices over all polygons of any unfolding.  This
   is achieved for the dodecahedron (12*5 vertices) and the icosahedron
   (20*3 vertices). */
#define NUM_VERTEX 60

/* Constants to label vertices during the computation of the minimum
   spanning tree of an unfolding of a Platonic solid. */
#define UNSEEN (-1)
#define SEEN   (-2)

/* The different Platonic solid types. */
#define TETRAHEDRON  0
#define HEXAHEDRON   1
#define OCTAHEDRON   2
#define DODECAHEDRON 3
#define ICOSAHEDRON  4

/* The number of faces and edges for each Platonic solid. */
#define TETRAHEDRON_NUM_FACES  4
#define TETRAHEDRON_NUM_EDGES  6
#define HEXAHEDRON_NUM_FACES   6
#define HEXAHEDRON_NUM_EDGES   12
#define OCTAHEDRON_NUM_FACES   8
#define OCTAHEDRON_NUM_EDGES   12
#define DODECAHEDRON_NUM_FACES 12
#define DODECAHEDRON_NUM_EDGES 30
#define ICOSAHEDRON_NUM_FACES  20
#define ICOSAHEDRON_NUM_EDGES  30

/* The maximum rotation angle for a triangle in the folding of a
   tetrahedron.  This is 180° minus the dihedral angle of the faces, which
   is acos(1/3) = atan(2*sqrt(2)).  Hence, this angle is acos(-1/3) =
   2*atan(sqrt(2)). */
#define TETRAHEDRON_MAX_ANGLE   109.4712206344907f
#define TETRAHEDRON_DELTA_ANGLE (TETRAHEDRON_MAX_ANGLE/30.0f)

/* The maximum rotation angle for a square in the folding of a hexahedron
   (cube).  This is 180° minus the dihedral angle of the faces, which
   obviously is 90°. */
#define HEXAHEDRON_MAX_ANGLE   90.0f
#define HEXAHEDRON_DELTA_ANGLE (HEXAHEDRON_MAX_ANGLE/30.0f)

/* The maximum rotation angle for a triangle in the folding of an
   octahedron.  This is 180° minus the dihedral angle of the faces, which
   is acos(-1/3) = 2*atan(sqrt(2)).  Hence, this angle is acos(1/3) =
   atan(2*sqrt(2)). */
#define OCTAHEDRON_MAX_ANGLE   70.5287793655093f
#define OCTAHEDRON_DELTA_ANGLE (OCTAHEDRON_MAX_ANGLE/30.0f)

/* The maximum rotation angle for a pentagon in the folding of a
   dodecahedron.  This is 180° minus the dihedral angle of the faces, which
   is 180°-2*atan((sqrt(5)-1)/2).  Hence, this angle is
   2*atan((sqrt(5)-1)/2). */
#define DODECAHEDRON_MAX_ANGLE   63.4349488229220f
#define DODECAHEDRON_DELTA_ANGLE (DODECAHEDRON_MAX_ANGLE/30.0f)

/* The maximum rotation angle for a triangle in the folding of an
   icosahedron.  This is 180° minus the dihedral angle of the faces, which
   is 180°-2*atan((3-sqrt(5))/2).  Hence, this angle is
   2*atan((3-sqrt(5))/2). */
#define ICOSAHEDRON_MAX_ANGLE   41.8103148957786f
#define ICOSAHEDRON_DELTA_ANGLE (ICOSAHEDRON_MAX_ANGLE/30.0f)


#include <stdbool.h>
#include "glsl-utils.h"
#include "gltrackball.h"

#include "images/gen/earth_png.h"
#include "images/gen/earth_night_png.h"
#include "images/gen/earth_water_png.h"
#include "ximage-loader.h"


#ifdef USE_MODULES
ModStruct platonicfolding_description =
{"platonicfolding", "init_platonicfolding", "draw_platonicfolding",
 NULL, "draw_platonicfolding", "change_platonicfolding",
 "free_platonicfolding", &platonicfolding_opts, 20000, 1, 1, 1, 1.0, 4, "",
 "Display the unfolding and folding of Platonic solids",
 0, NULL};
#endif


static Bool rotate;
static char *color_mode;
static char *foldings;


static XrmOptionDescRec opts[] =
{
  {"-rotate",       ".rotate",   XrmoptionNoArg,  "on"},
  {"+rotate",       ".rotate",   XrmoptionNoArg,  "off"},
  {"-colors",       ".colors",   XrmoptionSepArg, 0 },
  {"-face-colors",  ".colors",   XrmoptionNoArg,  "face" },
  {"-earth-colors", ".colors",   XrmoptionNoArg,  "earth" },
  {"-foldings",     ".foldings", XrmoptionSepArg, 0 },
};

static argtype vars[] =
{
  { &rotate,     "rotate",   "Rotate",   DEF_ROTATE,   t_Bool },
  { &color_mode, "colors",   "Colors",   DEF_COLORS,   t_String },
  { &foldings,   "foldings", "Foldings", DEF_FOLDINGS, t_String }
};

ENTRYPOINT ModeSpecOpt platonicfolding_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, NULL};


/* ------------------------------------------------------------------------- */


typedef float vertex[4];
typedef float normal[4];
typedef float color[4];
typedef float texcoord[3];
typedef float matrix[4][4];

/* Data structure to represent an edge of a graph that represents the
   adjacency of polygons in a polyhedron. The variables src and dst
   describe which polygon faces are adjacent.  The variables src_edge and
   dst_edge describe which edges of the respective faces are touching.
   The variable weight is set to a random value to create the minimum
   spanning tree. */
typedef struct {
  int src, dst;
  int src_edge, dst_edge;
  float weight;
} edge;

/* Data structure to represent an edge-based graph with num_v vertices,
   num_e edges, and an array of edges.  The graph represents the adjacency
   of faces in a polyhedron via the array edges. */
typedef struct {
  int num_v, num_e;
  edge *edges;
} edge_graph;

/* Disjoint set union (DSU) data structure with parent and rank.  This is
   used in the algorithm that determines the edges of a minimum spanning
   tree of a polyhedron. */
typedef struct {
  int *parent;
  int *rank;
} dsu;

/* Data structure for an adjacency-list-based graph.  This is used to
   convert the edges of the minimum spanning tree into a graph that is then
   traversed to create the actual minimum spanning tree.  The variable v
   stores the face number of the polygon in the polyhedron.  The variables
   edge_parent and edge_self store the edges by which a polygon that is
   adjacent to v is connected to the polygon v.  The value of edge_parent
   refers to the edge in the parent polygon of v in the graph, while
   edge_self refers to the edge in the polygon v. */
typedef struct graph_node {
  struct graph_node *next;
  int v;
  int edge_parent, edge_self;
} graph_node;

/* The data structure for a tree that describes an unfolded and folded
   polyhedron.  The pointer next points to the next base polygon on the
   same tree level.  The pointer child points to the first polygon on the
   next lower tree level.  Hence, the unfolding is described by a general
   tree.  The index of the polygon in the polyhedron is given by
   polygon_index.  This is used to color the polygons in different foldings
   consistently.  The indices edge_parent and edge_self contain the indices
   of the edge in the parent polygon and the edge in the polygon described
   by the current node that are attached to each other.  Since it is assumed
   that the base polygon is oriented counterclockwise, the edges point in
   opposite directions.  The root of the tree has no parent.  Therefore,
   both indices must be set to -1 for the root node.  The matrix
   unfold_pose describes the pose of the polygon described by the node with
   respect to its parent polygon in the unfolded state.  Hence, it is a
   relative pose and not an absolute pose.  The variable fold_angle_index
   is used to determine which angle to use for the rotation of the polygon
   described by a node with respect to its parent polygon.  The values of
   unfold_pose and fold_angle_index are determined dynamically by the order
   of the tree traversal in the initialization function
   determine_unfolding_poses.  The variable fold_angle stores the current
   fold angle of the polygon described by the node with respect to its
   parent.  The matrix fold_pose stores the relative pose obtained by
   rotating the polygon described by a node with respect to its parent
   polygon around the edge described by edge_parent and edge_self by the
   angle fold_angle.  The values of fold_angle and fold_pose are typically
   determined when the folded polyhedron is drawn. */
typedef struct tree_node {
  struct tree_node *next;
  struct tree_node *child;
  int polygon_index;
  int edge_parent, edge_self;
  matrix unfold_pose;
  int fold_angle_index;
  float fold_angle;
  matrix fold_pose;
} tree_node;

/* The data structure for a polygon consisting of num vertices v, a single
   normal vector n, num texture coordinates t (one per vertex), and a
   color c. */
typedef struct {
  int num;
  vertex *v;
  normal n;
  texcoord *t;
  color c;
} polygon;

/* The data structure for a set of polygons with polygon-specific data such
   as texture coordinates for each vertex. */
typedef struct {
  int num;
  polygon *poly;
} polygons;


/* ------------------------------------------------------------------------- */


/* The edge-based adjacency graph of a tetrahedron. */
static edge tetrahedron_edges[TETRAHEDRON_NUM_EDGES] = {
  {  0,  1, 0, 0, 0.0f },
  {  0,  2, 2, 0, 0.0f },
  {  0,  3, 1, 0, 0.0f },
  {  1,  2, 1, 2, 0.0f },
  {  1,  3, 2, 1, 0.0f },
  {  2,  3, 1, 2, 0.0f }
};

/* The edge-based adjacency graph of a hexahedron. */
static edge hexahedron_edges[HEXAHEDRON_NUM_EDGES] = {
  {  0,  1, 0, 0, 0.0f },
  {  0,  2, 1, 0, 0.0f },
  {  0,  3, 2, 0, 0.0f },
  {  0,  4, 3, 0, 0.0f },
  {  1,  2, 3, 1, 0.0f },
  {  1,  4, 1, 3, 0.0f },
  {  1,  5, 2, 0, 0.0f },
  {  2,  3, 3, 1, 0.0f },
  {  2,  5, 2, 3, 0.0f },
  {  3,  4, 3, 1, 0.0f },
  {  3,  5, 2, 2, 0.0f },
  {  4,  5, 2, 1, 0.0f }
};

/* The edge-based adjacency graph of an octahedron. */
static edge octahedron_edges[OCTAHEDRON_NUM_EDGES] = {
  {  0,  1, 0, 0, 0.0f },
  {  0,  2, 1, 0, 0.0f },
  {  0,  3, 2, 0, 0.0f },
  {  1,  4, 1, 2, 0.0f },
  {  1,  5, 2, 1, 0.0f },
  {  2,  5, 1, 0, 0.0f },
  {  2,  6, 2, 0, 0.0f },
  {  3,  4, 2, 0, 0.0f },
  {  3,  6, 1, 2, 0.0f },
  {  4,  7, 1, 1, 0.0f },
  {  5,  7, 2, 0, 0.0f },
  {  6,  7, 1, 2, 0.0f }
};

/* The edge-based adjacency graph of a dodecahedron. */
static edge dodecahedron_edges[DODECAHEDRON_NUM_EDGES] = {
  {  0,  1, 0, 0, 0.0f },
  {  0,  2, 1, 4, 0.0f },
  {  0,  3, 2, 0, 0.0f },
  {  0,  4, 3, 3, 0.0f },
  {  0,  5, 4, 1, 0.0f },
  {  1,  2, 4, 0, 0.0f },
  {  1,  5, 1, 0, 0.0f },
  {  1,  6, 2, 0, 0.0f },
  {  1,  7, 3, 0, 0.0f },
  {  2,  3, 3, 1, 0.0f },
  {  2,  7, 1, 4, 0.0f },
  {  2,  8, 2, 2, 0.0f },
  {  3,  4, 4, 4, 0.0f },
  {  3,  8, 2, 1, 0.0f },
  {  3,  9, 3, 0, 0.0f },
  {  4,  5, 2, 2, 0.0f },
  {  4,  9, 0, 4, 0.0f },
  {  4, 10, 1, 4, 0.0f },
  {  5,  6, 4, 1, 0.0f },
  {  5, 10, 3, 3, 0.0f },
  {  6,  7, 4, 1, 0.0f },
  {  6, 10, 2, 2, 0.0f },
  {  6, 11, 3, 3, 0.0f },
  {  7,  8, 3, 3, 0.0f },
  {  7, 11, 2, 2, 0.0f },
  {  8,  9, 0, 1, 0.0f },
  {  8, 11, 4, 1, 0.0f },
  {  9, 10, 3, 0, 0.0f },
  {  9, 11, 2, 0, 0.0f },
  { 10, 11, 1, 4, 0.0f }
};

/* The edge-based adjacency graph of an icosahedron. */
static edge icosahedron_edges[ICOSAHEDRON_NUM_EDGES] = {
  {  0,  1, 2, 0, 0.0f },
  {  0,  2, 0, 0, 0.0f },
  {  0,  3, 1, 0, 0.0f },
  {  1,  4, 2, 0, 0.0f },
  {  1,  9, 1, 0, 0.0f },
  {  2,  5, 1, 2, 0.0f },
  {  2,  6, 2, 1, 0.0f },
  {  3,  7, 1, 0, 0.0f },
  {  3,  8, 2, 0, 0.0f },
  {  4,  5, 2, 0, 0.0f },
  {  4, 10, 1, 0, 0.0f },
  {  5, 15, 1, 2, 0.0f },
  {  6,  7, 0, 1, 0.0f },
  {  6, 14, 2, 1, 0.0f },
  {  7, 13, 2, 0, 0.0f },
  {  8,  9, 2, 1, 0.0f },
  {  8, 12, 1, 2, 0.0f },
  {  9, 11, 2, 1, 0.0f },
  { 10, 11, 1, 0, 0.0f },
  { 10, 18, 2, 0, 0.0f },
  { 11, 17, 2, 1, 0.0f },
  { 12, 13, 0, 2, 0.0f },
  { 12, 17, 1, 2, 0.0f },
  { 13, 16, 1, 0, 0.0f },
  { 14, 15, 2, 1, 0.0f },
  { 14, 16, 0, 1, 0.0f },
  { 15, 18, 0, 2, 0.0f },
  { 16, 19, 2, 0, 0.0f },
  { 17, 19, 0, 2, 0.0f },
  { 18, 19, 1, 1, 0.0f }
};

/* The vertices of a tetrahedron triangle.  All vertices lie on the unit
   circle. */
static vertex tetrahedron_triangle_vert[3] = {
  {  1.0f,        0.0f, -SQRT_2_4_F, 1.0f },
  { -0.5f,  SQRT_3_2_F, -SQRT_2_4_F, 1.0f },
  { -0.5f, -SQRT_3_2_F, -SQRT_2_4_F, 1.0f }
};

/* The triangle that is used for the tetrahedron. */
static polygon tetrahedron_triangle = {
  3,
  tetrahedron_triangle_vert,
  { 0.0f, 0.0f, 1.0f, 0.0f },
  NULL,
  { 0.0f, 0.0f, 0.0f, 1.0f }
};

/* The eye position for a terahedron. */
static const GLfloat tetrahedron_eye_pos[3] = { 0.0f, 0.0f, 6.0f };


/* The vertices of a hexahedron square.  All vertices lie on the unit
   circle. */
static vertex hexahedron_square_vert[4] = {
  {  SQRT_2_2_F, -SQRT_2_2_F, -SQRT_2_2_F, 1.0f },
  {  SQRT_2_2_F,  SQRT_2_2_F, -SQRT_2_2_F, 1.0f },
  { -SQRT_2_2_F,  SQRT_2_2_F, -SQRT_2_2_F, 1.0f },
  { -SQRT_2_2_F, -SQRT_2_2_F, -SQRT_2_2_F, 1.0f }
};

/* The square that is used for the hexahedron (cube). */
static polygon hexahedron_square = {
  4,
  hexahedron_square_vert,
  { 0.0f, 0.0f, 1.0f, 0.0f },
  NULL,
  { 0.0f, 0.0f, 0.0f, 1.0f }
};

/* The eye position for a hexahedron. */
static const GLfloat hexahedron_eye_pos[3] = { 0.0f, 0.0f, 6.0f };


/* The vertices of an octahedron triangle.  All vertices lie on the unit
   circle. */
static vertex octahedron_triangle_vert[3] = {
  {  1.0f,        0.0f, -SQRT_2_2_F, 1.0f },
  { -0.5f,  SQRT_3_2_F, -SQRT_2_2_F, 1.0f },
  { -0.5f, -SQRT_3_2_F, -SQRT_2_2_F, 1.0f }
};

/* The triangle that is used for the octahedron. */
static polygon octahedron_triangle = {
  3,
  octahedron_triangle_vert,
  { 0.0f, 0.0f, 1.0f, 0.0f },
  NULL,
  { 0.0f, 0.0f, 0.0f, 1.0f }
};

/* The eye position for an octahedron. */
static const GLfloat octahedron_eye_pos[3] = { 0.0f, 0.0f, 6.0f };


/* The vertices of a dodecahedron pentagon.  All vertices lie on the unit
   circle. */
static vertex dodecahedron_pentagon_vert[5] = {
  {      1.0f,      0.0f, -DODECA_IN_RAD_F, 1.0f },
  {  COS_72_F,  SIN_72_F, -DODECA_IN_RAD_F, 1.0f },
  { -COS_36_F,  SIN_36_F, -DODECA_IN_RAD_F, 1.0f },
  { -COS_36_F, -SIN_36_F, -DODECA_IN_RAD_F, 1.0f },
  {  COS_72_F, -SIN_72_F, -DODECA_IN_RAD_F, 1.0f }
};

/* The pentagon that is used for the dodecahedron. */
static polygon dodecahedron_pentagon = {
  5,
  dodecahedron_pentagon_vert,
  { 0.0f, 0.0f, 1.0f, 0.0f },
  NULL,
  { 0.0f, 0.0f, 0.0f, 1.0f }
};

/* The eye position for a dodecahedron. */
static const GLfloat dodecahedron_eye_pos[3] = { 0.0f, 0.0f, 12.0f };


/* The vertices of a icosahedron triangle.  All vertices lie on the unit
   circle. */
static vertex icosahedron_triangle_vert[3] = {
  {  1.0f,        0.0f, -ICOSA_IN_RAD_F, 1.0f },
  { -0.5f,  SQRT_3_2_F, -ICOSA_IN_RAD_F, 1.0f },
  { -0.5f, -SQRT_3_2_F, -ICOSA_IN_RAD_F, 1.0f }
};

/* The triangle that is used for the icosahedron. */
static polygon icosahedron_triangle = {
  3,
  icosahedron_triangle_vert,
  { 0.0f, 0.0f, 1.0f, 0.0f },
  NULL,
  { 0.0f, 0.0f, 0.0f, 1.0f }
};

/* The eye position for an icosahedron. */
static const GLfloat icosahedron_eye_pos[3] = { 0.0f, 0.0f, 12.0f };


/* ------------------------------------------------------------------------- */


typedef struct {
  GLint WindH, WindW;
  GLXContext *glx_context;
  /* Options */
  bool rotate;
  int colors;
  bool random_colors;
  int num_foldings;
  /* Aspect ratio of the current window */
  float aspect;
  /* Trackball states */
  trackball_state *trackball;
  Bool button_pressed;
  /* The folding angle data. */
  float angle[NUM_ANGLES];
  float max_angle, delta_angle, angle_dir;
  /* The index of the current angle that is folded or unfolded. */
  int fold_angle;
  /* The number of angles that can be folded for the current polyhedron. */
  int num_fold_angles;
  /* The 3D rotation angles. */
  float alpha, beta, delta, delta_delta;
  /* The eype position of the current polyhedron. */
  const GLfloat *eye_pos;
  /* The unfolding tree of the current polyhedron. */
  tree_node *polygon_unfolding;
  /* The base polygons of the current polyhedron. */
  polygons *base_polygons;
  /* Should the north pole or the south pole face upwards? */
  bool north_up;
  /* Animation state variables. */
  int anim_state, anim_poly, anim_step, anim_num_steps, anim_remaining;
  float poly_pos[3], spoly_pos[3], dpoly_pos[3];
  /* The current color rotation matrix. */
  matrix color_matrix;
  /* Buffers for drawing a polyhedron. */
  vertex vtx[NUM_VERTEX];
  normal nrm[NUM_VERTEX];
  color col[NUM_VERTEX];
  texcoord tex[NUM_VERTEX];
  int num[NUM_VERTEX];
#ifdef HAVE_GLSL
  bool use_shaders, use_mipmaps, use_vao;
  GLuint shader_program;
  GLint pos_index, normal_index, color_index, tex_index;
  GLint mv_index, proj_index;
  GLint glbl_ambient_index, lt_ambient_index;
  GLint lt_diffuse_index, lt_specular_index;
  GLint lt_direction_index, lt_halfvect_index;
  GLint ambient_index, diffuse_index;
  GLint specular_index, shininess_index;
  GLint bool_textures_index, north_up_index, magma_offs_index;
  GLint samp_mgm_index, samp_day_index, samp_ngt_index, samp_wtr_index;
  GLuint vertex_buffer, normal_buffer, color_buffer, tex_buffer;
  GLuint vertex_array_object;
  GLuint magma_tex, earth_tex[3];
  GLubyte *magma_tex_rg;
  GLfloat magma_tex_offset[3];
  bool textures_supported, aniso_textures, use_textures;
#endif /* HAVE_GLSL */
} platonicfoldingstruct;

static platonicfoldingstruct *platonicfolding = (platonicfoldingstruct *) NULL;


/* ------------------------------------------------------------------------- */


#ifdef HAVE_GLSL

/* The hash table for the Perlin noise generator.  In Perlin's original
   code, only half of this table is stored and then copied to a hash table
   twice the size.  We directly store the table of twice the size. */
static GLushort permutation[2*HASH_SIZE] = {
  151, 160, 137,  91,  90,  15, 131,  13,
  201,  95,  96,  53, 194, 233,   7, 225,
  140,  36, 103,  30,  69, 142,   8,  99,
   37, 240,  21,  10,  23, 190,   6, 148,
  247, 120, 234,  75,   0,  26, 197,  62,
   94, 252, 219, 203, 117,  35,  11,  32,
   57, 177,  33,  88, 237, 149,  56,  87,
  174,  20, 125, 136, 171, 168,  68, 175,
   74, 165,  71, 134, 139,  48,  27, 166,
   77, 146, 158, 231,  83, 111, 229, 122,
   60, 211, 133, 230, 220, 105,  92,  41,
   55,  46, 245,  40, 244, 102, 143,  54,
   65,  25,  63, 161,   1, 216,  80,  73,
  209,  76, 132, 187, 208,  89,  18, 169,
  200, 196, 135, 130, 116, 188, 159,  86,
  164, 100, 109, 198, 173, 186,   3,  64,
   52, 217, 226, 250, 124, 123,   5, 202,
   38, 147, 118, 126, 255,  82,  85, 212,
  207, 206,  59, 227,  47,  16,  58,  17,
  182, 189,  28,  42, 223, 183, 170, 213,
  119, 248, 152,   2,  44, 154, 163,  70,
  221, 153, 101, 155, 167,  43, 172,   9,
  129,  22,  39, 253,  19,  98, 108, 110,
   79, 113, 224, 232, 178, 185, 112, 104,
  218, 246,  97, 228, 251,  34, 242, 193,
  238, 210, 144,  12, 191, 179, 162, 241,
   81,  51, 145, 235, 249,  14, 239, 107,
   49, 192, 214,  31, 181, 199, 106, 157,
  184,  84, 204, 176, 115, 121,  50,  45,
  127,   4, 150, 254, 138, 236, 205,  93,
  222, 114,  67,  29,  24,  72, 243, 141,
  128, 195,  78,  66, 215,  61, 156, 180,
  151, 160, 137,  91,  90,  15, 131,  13,
  201,  95,  96,  53, 194, 233,   7, 225,
  140,  36, 103,  30,  69, 142,   8,  99,
   37, 240,  21,  10,  23, 190,   6, 148,
  247, 120, 234,  75,   0,  26, 197,  62,
   94, 252, 219, 203, 117,  35,  11,  32,
   57, 177,  33,  88, 237, 149,  56,  87,
  174,  20, 125, 136, 171, 168,  68, 175,
   74, 165,  71, 134, 139,  48,  27, 166,
   77, 146, 158, 231,  83, 111, 229, 122,
   60, 211, 133, 230, 220, 105,  92,  41,
   55,  46, 245,  40, 244, 102, 143,  54,
   65,  25,  63, 161,   1, 216,  80,  73,
  209,  76, 132, 187, 208,  89,  18, 169,
  200, 196, 135, 130, 116, 188, 159,  86,
  164, 100, 109, 198, 173, 186,   3,  64,
   52, 217, 226, 250, 124, 123,   5, 202,
   38, 147, 118, 126, 255,  82,  85, 212,
  207, 206,  59, 227,  47,  16,  58,  17,
  182, 189,  28,  42, 223, 183, 170, 213,
  119, 248, 152,   2,  44, 154, 163,  70,
  221, 153, 101, 155, 167,  43, 172,   9,
  129,  22,  39, 253,  19,  98, 108, 110,
   79, 113, 224, 232, 178, 185, 112, 104,
  218, 246,  97, 228, 251,  34, 242, 193,
  238, 210, 144,  12, 191, 179, 162, 241,
   81,  51, 145, 235, 249,  14, 239, 107,
   49, 192, 214,  31, 181, 199, 106, 157,
  184,  84, 204, 176, 115, 121,  50,  45,
  127,   4, 150, 254, 138, 236, 205,  93,
  222, 114,  67,  29,  24,  72, 243, 141,
  128, 195,  78,  66, 215,  61, 156, 180
};


/* The fade function of Perlin's improved noise generator. */
static inline float fade(float t)
{
  return ((6.0f*t-15.0f)*t+10.0f)*t*t*t;
}


/* The linear interpolation function of Perlin's improved noise generator. */
static inline float lerp(float t, float a, float b)
{
  return a+t*(b-a);
}


/* The gradient noise generator of Perlin's improved noise generator. */
static float grad(GLushort hash, float x, float y, float z)
{
  GLushort h;
  float u, v;

  /* Convert the low four bits of the hash code into twelve gradient
     directions. */
  h = hash&15;
  u = h<8 ? x : y;
  v = h<4 ? y : h==12 || h==14 ? x : z;
  return ((h&1) == 0 ? u : -u) + ((h&2) == 0 ? v : -v);
}


/* Perlin's improved noise generator. */
static float noise3(float x, float y, float z)
{
  static const GLushort *p = permutation;
  float u, v, w;
  GLushort X, Y, Z, A, B, AA, AB, BA, BB;

  /* Find unit cube that contains the point (x,y,z). */
  X = (int)floorf(x) & (HASH_SIZE-1);
  Y = (int)floorf(y) & (HASH_SIZE-1);
  Z = (int)floorf(z) & (HASH_SIZE-1);
  /* Find the relative x, y, and z coordinates of the point in the cube. */
  x -= floorf(x);
  y -= floorf(y);
  z -= floorf(z);
  /* Compute fade curves for each of x, y, and z. */
  u = fade(x);
  v = fade(y);
  w = fade(z);
  /* Compute the hash coordinates of the eight cube corners. */
  A = p[X]+Y;
  AA = p[A]+Z;
  AB = p[A+1]+Z;
  B = p[X+1]+Y;
  BA = p[B]+Z;
  BB = p[B+1]+Z;
  /* Add the blended results from the eight corners of the cube. */
  return lerp(w,lerp(v,lerp(u,grad(p[AA],x,y,z),
                              grad(p[BA],x-1.0f,y,z)),
                       lerp(u,grad(p[AB],x,y-1.0f,z),
                              grad(p[BB],x-1.0f,y-1.0f,z))),
                lerp(v,lerp(u,grad(p[AA+1],x,y,z-1.0f),
                              grad(p[BA+1],x-1.0f,y,z-1.0f)),
                       lerp(u,grad(p[AB+1],x,y-1.0f,z-1.0f),
                              grad(p[BB+1],x-1.0f,y-1.0f,z-1.0f))));
}


/* The smoothstep function of the OpenGL shading language. */
static inline float smoothstep(float min, float max, float x)
{
  float t;

  if (x < min)
  {
    return 0.0f;
  }
  else if (x > max)
  {
    return 1.0f;
  }
  else
  {
    t = (x-min)/(max-min);
    return t*t*(3.0f-2.0f*t);
  }
}


/* Create a red and green look-up table that simulates the look of a heated
   material.  The blue look-up table would entirely consist of zeros, so we
   don't create it. */
static void create_magma_lut_rg(GLubyte lutr[256], GLubyte lutg[256])
{
  int i;

  for (i=0; i<256; i++)
  {
    lutr[i] = (GLubyte)(255.0f*powf(smoothstep(0.0f,159.0f,(float)i),
                                    MAGMA_LUT_GAMMA));
    lutg[i] = (GLubyte)(255.0f*powf(smoothstep(96.0f,255.0f,(float)i),
                                    MAGMA_LUT_GAMMA));
  }
}


/* Create a 3D magma texture of dimension size×size×size.  Since the blue
   channel would be entirely filled with zeros, we omit this channel and
   create a texture consisting only of the red and green channels. */
static GLubyte *gen_3d_magma_texture_rg(int size)
{
  const float startx = 0.0f, starty = 0.0f, startz = 0.0f;
  int f, i, j, k, frequency;
  float x, y, z, inc[NUM_OCTAVES];
  float a, amp[NUM_OCTAVES], n;
  GLubyte *ptr, *tex_ptr, g, lutr[256], lutg[256];

  create_magma_lut_rg(lutr,lutg);

  tex_ptr = malloc(2*size*size*size*sizeof(*tex_ptr));
  if (tex_ptr == NULL)
    abort();

  frequency = START_FREQUENCY;
  a = 196.0f;
  for (f=0; f<NUM_OCTAVES; f++)
  {
    inc[f] = (float)frequency/(float)size;
    amp[f] = a;
    frequency *= 2;
    a *= 0.5f;
  }

  ptr = tex_ptr;
  for (i=0; i<size; i++)
  {
    for (j=0; j<size; j++)
    {
      for (k=0; k<size; k++, ptr+=2)
      {
        n = 0.0f;
        for (f=0; f<NUM_OCTAVES; f++)
        {
          x = startx+i*inc[f];
          y = starty+j*inc[f];
          z = startz+k*inc[f];
          n += fabsf(noise3(x,y,z))*amp[f];
        }
        if (n > 255.0f)
          n = 255.0f;
        g = (unsigned char)floorf(n+0.5f);
        ptr[0] = lutr[g];
        ptr[1] = lutg[g];
      }
    }
  }

  return tex_ptr;
}

#endif /* HAVE_GLSL */


/* ------------------------------------------------------------------------- */


#ifdef HAVE_GLSL

/* Convert degrees to radians, */
static inline double rad(double deg)
{
  return (M_PI/180.0)*deg;
}


/* Convert radians to degrees, */
static inline double deg(double rad)
{
  return (180.0/M_PI)*rad;
}


/* Compute the Fortran modulo function. */
static inline double modulo(double a, double p)
{
  return a-floor(a/p)*p;
}


/* Get the Julian date based on the current time. */
static inline double get_current_julian_date(void)
{
  return (double)time(NULL)/86400.0+2440587.5;
}


/* Get the direction of the sun on the Julian date given by jd.  The
   function calculates the latitude and longitude of the subsolar point,
   i.e., the point on earth for which the sun is perpendicular.  The
   longitude and latitude are then converted to a direction vector that
   points to the sun.  See: Taiping Zhang, Paul W. Stackhouse Jr., Bradley
   Macpherson, and J. Colleen Mikovitz: A solar azimuth formula that
   renders circumstantial treatment unnecessary without compromising
   mathematical rigor: Mathematical setup, application and extension of a
   formula based on the subsolar point and atan2 function. Renewable Energy
   172:1333-1340, 2021. */
static inline void get_sun_direction(double jd, float sun[4])
{
  double n, utc;
  double l, g, gr, lambda, lambdar, epsilon, epsilonr;
  double alpha, delta, eot, lat, lon;
  float latr, lonr;

  n = jd-2451545.0;
  utc = 24.0*modulo(jd-0.5,1.0);
  l = modulo(280.460+0.9856474*n,360.0);
  g = modulo(357.528+0.9856003*n,360.0);
  gr = rad(g);
  lambda = modulo(l+1.915*sin(gr)+0.020*sin(2.0*gr),360.0);
  epsilon = 23.439-0.0000004*n;
  lambdar = rad(lambda);
  epsilonr = rad(epsilon);
  alpha = modulo(deg(atan2(cos(epsilonr)*sin(lambdar),cos(lambdar))),360.0);
  delta = deg(asin(sin(epsilonr)*sin(lambdar)));
  eot = modulo(l-alpha+180.0,360.0)-180.0;
  lat = delta;
  lon = -15.0*(utc-12.0+eot*(4.0/60.0));
  latr = (float)rad(lat);
  lonr = (float)rad(lon);
  sun[0] = cosf(latr)*cosf(lonr);
  sun[1] = cosf(latr)*sinf(lonr);
  sun[2] = sinf(latr);
  sun[3] = 0.0f;
}

#endif /* HAVE_GLSL */


/* ------------------------------------------------------------------------- */


#ifdef HAVE_GLSL

/* The vertex shader code. */
static const GLchar *vertex_shader = "\
#ifdef GL_ES                                                             \n\
precision highp float;                                                   \n\
precision highp int;                                                     \n\
precision highp sampler2D;                                               \n\
precision highp sampler3D;                                               \n\
#endif                                                                   \n\
                                                                         \n\
#if __VERSION__ <= 120                                                   \n\
attribute vec4 VertexPosition;                                           \n\
attribute vec4 VertexNormal;                                             \n\
attribute vec4 VertexColor;                                              \n\
attribute vec3 VertexTexCoord;                                           \n\
varying vec3 Normal;                                                     \n\
varying vec4 Color;                                                      \n\
varying vec3 EarthTexCoord;                                              \n\
varying vec3 LightTexCoord;                                              \n\
varying vec3 MagmaTexCoord;                                              \n\
#else                                                                    \n\
in vec4 VertexPosition;                                                  \n\
in vec4 VertexNormal;                                                    \n\
in vec4 VertexColor;                                                     \n\
in vec3 VertexTexCoord;                                                  \n\
out vec3 Normal;                                                         \n\
out vec4 Color;                                                          \n\
out vec3 EarthTexCoord;                                                  \n\
out vec3 LightTexCoord;                                                  \n\
out vec3 MagmaTexCoord;                                                  \n\
#endif                                                                   \n\
                                                                         \n\
uniform mat4 MatModelView;                                               \n\
uniform mat4 MatProj;                                                    \n\
uniform vec3 TexOffsetMagma;                                             \n\
uniform bool NorthUp;                                                    \n\
                                                                         \n\
void main(void)                                                          \n\
{                                                                        \n\
  Color = VertexColor;                                                   \n\
  Normal = normalize(MatModelView*VertexNormal).xyz;                     \n\
  vec4 Position = MatModelView*VertexPosition;                           \n\
  gl_Position = MatProj*Position;                                        \n\
  if (NorthUp)                                                           \n\
    EarthTexCoord = VertexTexCoord;                                      \n\
  else                                                                   \n\
    EarthTexCoord = vec3(-1.0f,1.0f,-1.0f)*VertexTexCoord;               \n\
  LightTexCoord = (MatModelView*vec4(VertexTexCoord,0.0f)).xyz;          \n\
  MagmaTexCoord = 0.4f*(VertexTexCoord+1.0f)+TexOffsetMagma;             \n\
}                                                                        \n";

/* The fragment shader code. */
static const GLchar *fragment_shader = "\
#ifdef GL_ES                                                             \n\
precision highp float;                                                   \n\
precision highp int;                                                     \n\
precision highp sampler2D;                                               \n\
precision highp sampler3D;                                               \n\
#endif                                                                   \n\
                                                                         \n\
#if __VERSION__ <= 120                                                   \n\
varying vec3 Normal;                                                     \n\
varying vec4 Color;                                                      \n\
varying vec3 EarthTexCoord;                                              \n\
varying vec3 LightTexCoord;                                              \n\
varying vec3 MagmaTexCoord;                                              \n\
#else                                                                    \n\
in vec3 Normal;                                                          \n\
in vec4 Color;                                                           \n\
in vec3 EarthTexCoord;                                                   \n\
in vec3 LightTexCoord;                                                   \n\
in vec3 MagmaTexCoord;                                                   \n\
out vec4 FragColor;                                                      \n\
#endif                                                                   \n\
                                                                         \n\
const float I_M_PI_F = 0.318309886184f;                                  \n\
const float I_M_2PI_F = 0.159154943092f;                                 \n\
uniform vec4 LtGlblAmbient;                                              \n\
uniform vec4 LtAmbient, LtDiffuse, LtSpecular;                           \n\
uniform vec3 LtDirection, LtHalfVector;                                  \n\
uniform vec4 MatAmbient;                                                 \n\
uniform vec4 MatDiffuse;                                                 \n\
uniform vec4 MatSpecular;                                                \n\
uniform float MatShininess;                                              \n\
uniform bool BoolTextures;                                               \n\
uniform sampler3D TextureSamplerMagma;                                   \n\
uniform sampler2D TextureSamplerDay;                                     \n\
uniform sampler2D TextureSamplerNight;                                   \n\
uniform sampler2D TextureSamplerWater;                                   \n\
                                                                         \n\
void main(void)                                                          \n\
{                                                                        \n\
  vec3 normalDirection;                                                  \n\
  vec4 ambientColor, diffuseColor, sceneColor;                           \n\
  vec4 ambientLighting, diffuseReflection, specularReflection;           \n\
  vec4 color, colord, colorn, specd, specn;                              \n\
  vec3 normTexCoord, normLightCoord;                                     \n\
  vec2 texCoor;                                                          \n\
  float ndotl, ldotls, ndoth, attd, attn, pf, lat, lon;                  \n\
                                                                         \n\
  if (!BoolTextures)                                                     \n\
  {                                                                      \n\
    if (gl_FrontFacing)                                                  \n\
    {                                                                    \n\
      normalDirection = normalize(Normal);                               \n\
      sceneColor = Color*MatAmbient*LtGlblAmbient;                       \n\
      ambientColor = Color*MatAmbient;                                   \n\
      diffuseColor = Color*MatDiffuse;                                   \n\
    }                                                                    \n\
    else                                                                 \n\
    {                                                                    \n\
      normalDirection = -normalize(Normal);                              \n\
      sceneColor = Color*MatAmbient*LtGlblAmbient;                       \n\
      ambientColor = Color*MatAmbient;                                   \n\
      diffuseColor = Color*MatDiffuse;                                   \n\
    }                                                                    \n\
                                                                         \n\
    ndotl = max(0.0f,dot(normalDirection,LtDirection));                  \n\
    ndoth = max(0.0f,dot(normalDirection,LtHalfVector));                 \n\
    if (ndotl == 0.0f)                                                   \n\
      pf = 0.0f;                                                         \n\
    else                                                                 \n\
      pf = pow(ndoth,MatShininess);                                      \n\
    ambientLighting = ambientColor*LtAmbient;                            \n\
    diffuseReflection = LtDiffuse*diffuseColor*ndotl;                    \n\
    specularReflection = LtSpecular*MatSpecular*pf;                      \n\
    color = sceneColor+ambientLighting+diffuseReflection+                \n\
            specularReflection;                                          \n\
    color.a = diffuseColor.a;                                            \n\
  }                                                                      \n\
  else                                                                   \n\
  {                                                                      \n\
    if (gl_FrontFacing)                                                  \n\
    {                                                                    \n\
#if __VERSION__ <= 120                                                   \n\
      color = texture3D(TextureSamplerMagma,MagmaTexCoord);              \n\
#else                                                                    \n\
      color = texture(TextureSamplerMagma,MagmaTexCoord);                \n\
#endif                                                                   \n\
      sceneColor = color*MatAmbient*LtGlblAmbient;                       \n\
      ambientColor = color*MatAmbient;                                   \n\
      diffuseColor = color*MatDiffuse;                                   \n\
      specularReflection = vec4(0.0f,0.0f,0.0f,0.0f);                    \n\
      diffuseReflection = LtDiffuse*diffuseColor;                        \n\
    }                                                                    \n\
    else                                                                 \n\
    {                                                                    \n\
      normTexCoord = normalize(EarthTexCoord);                           \n\
      normLightCoord = normalize(LightTexCoord);                         \n\
      lat = asin(normTexCoord.z);                                        \n\
      lon = atan(normTexCoord.y,normTexCoord.x);                         \n\
      texCoor = vec2(0.5f+lon*I_M_2PI_F,0.5f+lat*I_M_PI_F);              \n\
      ndotl = max(0.0f,dot(normLightCoord,LtDirection));                 \n\
      ndoth = max(0.0f,dot(normLightCoord,LtHalfVector));                \n\
      if (ndotl == 0.0f)                                                 \n\
        pf = 0.0f;                                                       \n\
      else                                                               \n\
        pf = pow(ndoth,MatShininess);                                    \n\
      specularReflection = LtSpecular*MatSpecular*pf;                    \n\
      ldotls = dot(normLightCoord,LtDirection);                          \n\
#if __VERSION__ <= 120                                                   \n\
      colord = texture2D(TextureSamplerDay,texCoor);                     \n\
#else                                                                    \n\
      colord = texture(TextureSamplerDay,texCoor);                       \n\
#endif                                                                   \n\
      attd = smoothstep(-0.2f,0.2f,ldotls);                              \n\
#if __VERSION__ <= 120                                                   \n\
      colorn = texture2D(TextureSamplerNight,texCoor);                   \n\
#else                                                                    \n\
      colorn = texture(TextureSamplerNight,texCoor);                     \n\
#endif                                                                   \n\
      attn = smoothstep(-0.2f,0.2f,-ldotls);                             \n\
      color = attd*colord+attn*colorn;                                   \n\
#if __VERSION__ <= 120                                                   \n\
      specd = texture2D(TextureSamplerWater,texCoor);                    \n\
#else                                                                    \n\
      specd = texture(TextureSamplerWater,texCoor);                      \n\
#endif                                                                   \n\
      specn = vec4(0.0f,0.0f,0.0f,1.0f);                                 \n\
      specularReflection *= attd*specd+attn*specn;                       \n\
      sceneColor = color*MatAmbient*LtGlblAmbient;                       \n\
      ambientColor = color*MatAmbient;                                   \n\
      diffuseColor = color*MatDiffuse;                                   \n\
      diffuseReflection = LtDiffuse*diffuseColor*ndotl;                  \n\
    }                                                                    \n\
                                                                         \n\
    ambientLighting = ambientColor*LtAmbient;                            \n\
    color = sceneColor+ambientLighting+diffuseReflection+                \n\
            specularReflection;                                          \n\
    color.a = diffuseColor.a;                                            \n\
  }                                                                      \n\
#if __VERSION__ <= 120                                                   \n\
  gl_FragColor = clamp(color,0.0f,1.0f);                                 \n\
#else                                                                    \n\
  FragColor = clamp(color,0.0f,1.0f);                                    \n\
#endif                                                                   \n\
}                                                                        \n";

#endif /* HAVE_GLSL */


/* ------------------------------------------------------------------------- */


/* Copy a vector c = v. */
static inline void copy_vector(float c[3], const float v[3])
{
  c[0] = v[0];
  c[1] = v[1];
  c[2] = v[2];
}


/* Compute the negative of a vector n = -a. */
static inline void neg(float n[3], const float a[3])
{
  n[0] = -a[0];
  n[1] = -a[1];
  n[2] = -a[2];
}


/* Scale a vector by a factor of s: w = v * s. */
static inline void scale(float w[3], const float v[3], float s)
{
  w[0] = v[0]*s;
  w[1] = v[1]*s;
  w[2] = v[2]*s;
}


/* Compute the vector sum s = a + b. */
static inline void add(float s[3], const float a[3], const float b[3])
{
  s[0] = a[0]+b[0];
  s[1] = a[1]+b[1];
  s[2] = a[2]+b[2];
}


/* Compute the vector difference s = a - b. */
static inline void sub(float s[3], const float a[3], const float b[3])
{
  s[0] = a[0]-b[0];
  s[1] = a[1]-b[1];
  s[2] = a[2]-b[2];
}


/* Compute the midpoint of two vectors m = (a + b) / 2. */
static inline void mid(float m[3], const float a[3], const float b[3])
{
  m[0] = 0.5f*(a[0]+b[0]);
  m[1] = 0.5f*(a[1]+b[1]);
  m[2] = 0.5f*(a[2]+b[2]);
}


/* Compute the dot product of two vectors. */
static inline float dot(const float v[3], const float w[3])
{
  return v[0]*w[0]+v[1]*w[1]+v[2]*w[2];
}


/* Compute the Euclidean norm of a vector. */
static inline float norm(const float v[3])
{
  return sqrtf(dot(v,v));
}


/* Normalize a vector. */
static inline void normalize(float v[3])
{
  float l;

  l = norm(v);
  if (l > 0.0f)
    l = 1.0f/l;
  v[0] *= l;
  v[1] *= l;
  v[2] *= l;
}


/* Compute the cross product c = m × n. */
static inline void cross(float c[3], const float m[3], const float n[3])
{
  c[0] = m[1]*n[2]-m[2]*n[1];
  c[1] = m[2]*n[0]-m[0]*n[2];
  c[2] = m[0]*n[1]-m[1]*n[0];
}


/* Copy a matrix: c = m. */
static void copy_matrix(matrix c, matrix m)
{
  int i, j;

  for (j=0; j<4; j++)
    for (i=0; i<4; i++)
      c[i][j] = m[i][j];
}


/* Compute the identity matrix. */
static void identity_matrix(matrix m)
{
  int i, j;

  for (i=0; i<4; i++)
    for (j=0; j<4; j++)
      m[i][j] = (float)(i==j);
}


/* Multiply two matrices: c = c * m. */
static void mult_matrix(matrix c, matrix m)
{
  int i, j;
  matrix t;

  /* Copy c to t. */
  copy_matrix(t,c);
  /* Compute c = t*m. */
  for (j=0; j<4; j++)
    for (i=0; i<4; i++)
      c[i][j] = (t[i][0]*m[0][j]+t[i][1]*m[1][j]+
                 t[i][2]*m[2][j]+t[i][3]*m[3][j]);
}


/* Add a translation matrix from the right to a matrix. */
static void translate_matrix(matrix m, const float t[3])
{
  int i;

  for (i=0; i<4; i++)
    m[i][3] = (t[0]*m[i][0]+t[1]*m[i][1]+t[2]*m[i][2]+m[i][3]);
}


/* Add a rotation in the xy plane to the matrix m. */
static void rotate_xy_matrix(matrix m, float phi)
{
  float c, s, u, v;
  int i;

  phi *= M_PI_F/180.0f;
  c = cosf(phi);
  s = sinf(phi);
  for (i=0; i<4; i++)
  {
    u = m[i][0];
    v = m[i][1];
    m[i][0] = c*u+s*v;
    m[i][1] = -s*u+c*v;
  }
}


/* Multiply a matrix with a vector: o = m * v. */
static void mult_matrix_vector(float o[4], matrix m, float v[4])
{
  int i, j;

  for (i=0; i<4; i++)
  {
    o[i] = 0.0f;
    for (j=0; j<4; j++)
      o[i] += m[i][j]*v[j];
  }
}


/* Generate a uniformly distributed random rotation matrix. */
static void rnd_rot_matrix(matrix m)
{
  float theta, phi, z, r, vx, vy, vz, st, ct, sx, sy;

  theta = (float)frand(2.0*M_PI);
  phi = (float)frand(2.0*M_PI);
  z = (float)frand(2.0);
  r  = sqrtf(z);
  vx = r*sinf(phi);
  vy = r*cosf(phi);
  vz = sqrtf(2.0f-z);
  st = sinf(theta);
  ct = cosf(theta);
  sx = vx*ct-vy*st;
  sy = vx*st+vy*ct;

  identity_matrix(m);
  m[0][0] = vx*sx-ct;
  m[0][1] = vx*sy-st;
  m[0][2] = vx*vz;
  m[1][0] = vy*sx+st;
  m[1][1] = vy*sy-ct;
  m[1][2] = vy*vz;
  m[2][0] = vz*sx;
  m[2][1] = vz*sy;
  m[2][2] = 1.0f-z;
}


/* An easing function for values of t between 0 and max. */
static inline float ease(float t, float max, int easing)
{
  float s, e;

  s = t/max;
  if (easing == EASING_QUINTIC)
    e = ((6.0f*s-15.0f)*s+10.0f)*s*s*s;
  else if (easing == EASING_ACCEL)
    e = s*s*(2.0f-s);
  else if (easing == EASING_DECEL)
    e = s*(1.0f+s*(1.0f-s));
  else
    e = 0.0f;
  return max*e;
}


/* ------------------------------------------------------------------------- */


/* Add a rotation around the x axis to the matrix m. */
static void rotatex(float m[3][3], float phi)
{
  float c, s, u, v;
  int i;

  phi *= M_PI_F/180.0f;
  c = cosf(phi);
  s = sinf(phi);
  for (i=0; i<3; i++)
  {
    u = m[i][1];
    v = m[i][2];
    m[i][1] = c*u+s*v;
    m[i][2] = -s*u+c*v;
  }
}


/* Add a rotation around the y axis to the matrix m. */
static void rotatey(float m[3][3], float phi)
{
  float c, s, u, v;
  int i;

  phi *= M_PI_F/180.0f;
  c = cosf(phi);
  s = sinf(phi);
  for (i=0; i<3; i++)
  {
    u = m[i][0];
    v = m[i][2];
    m[i][0] = c*u-s*v;
    m[i][2] = s*u+c*v;
  }
}


/* Add a rotation around the z axis to the matrix m. */
static void rotatez(float m[3][3], float phi)
{
  float c, s, u, v;
  int i;

  phi *= M_PI_F/180.0f;
  c = cosf(phi);
  s = sinf(phi);
  for (i=0; i<3; i++)
  {
    u = m[i][0];
    v = m[i][1];
    m[i][0] = c*u+s*v;
    m[i][1] = -s*u+c*v;
  }
}


/* Compute the rotation matrix m from the rotation angles. */
static void rotateall(float al, float be, float de, float m[3][3])
{
  int i, j;

  for (i=0; i<3; i++)
    for (j=0; j<3; j++)
      m[i][j] = (i==j);
  rotatex(m,al);
  rotatey(m,be);
  rotatez(m,de);
}


/* Compute a 4x4 rotation matrix from an xscreensaver unit quaternion. Note
   that xscreensaver has a different convention for unit quaternions than
   the one that is used in this hack. */
static void quat_to_rotmat_trackball(float p[4], float r[16])
{
  float al, be, de;
  float r00, r01, r02, r12, r22;
  float m[3][3];
  int   i, j;

  r00 = 1.0f-2.0f*(p[1]*p[1]+p[2]*p[2]);
  r01 = 2.0f*(p[0]*p[1]+p[2]*p[3]);
  r02 = 2.0f*(p[2]*p[0]-p[1]*p[3]);
  r12 = 2.0f*(p[1]*p[2]+p[0]*p[3]);
  r22 = 1.0f-2.0f*(p[1]*p[1]+p[0]*p[0]);

  al = atan2f(-r12,r22)*180.0f/M_PI_F;
  be = atan2f(r02,sqrtf(r00*r00+r01*r01))*180.0f/M_PI_F;
  de = atan2f(-r01,r00)*180.0f/M_PI_F;
  rotateall(al,be,de,m);
  for (i=0; i<3; i++)
  {
    for (j=0; j<3; j++)
      r[j*4+i] = m[i][j];
    r[3*4+i] = 0.0f;
    r[i*4+3] = 0.0f;
  }
  r[3*4+3] = 1.0f;
}


/* ------------------------------------------------------------------------- */


/* Create an edge-based graph eg with num_v vertices, num_e edges, and the
   edges passed in edges. */
static edge_graph *create_edge_graph(int num_v, int num_e, edge *edges)
{
  edge_graph *eg;

  eg = malloc(sizeof(*eg));
  if (eg == NULL)
    abort();
  eg->edges = malloc(num_e*sizeof(*eg->edges));
  if (eg->edges == NULL)
    abort();
  eg->num_v = num_v;
  eg->num_e = num_e;
  memcpy(eg->edges,edges,num_e*sizeof(*eg->edges));

  return eg;
}


/* Free the edge-based graph eg. */
static void free_edge_graph(edge_graph *eg)
{
  free(eg->edges);
  free(eg);
}


/* Create a DSU with num_v vertices. */
static dsu *create_dsu(int num_v)
{
  dsu *d;
  int i;

  d = malloc(sizeof(*d));
  if (d == NULL)
    abort();
  d->parent = malloc(num_v*sizeof(*d->parent));
  d->rank = malloc(num_v*sizeof(*d->rank));
  if (d->parent == NULL || d->rank == NULL)
    abort();
  for (i=0; i<num_v; i++)
  {
    d->parent[i] = i;
    d->rank[i] = 0;
  }
  return d;
}


/* Free a DSU. */
static void free_dsu(dsu *d)
{
  free(d->parent);
  free(d->rank);
  free(d);
}


/* Find the representative (root) of a set with path compression. */
static int find_root(dsu *d, int x)
{
  if (d->parent[x] != x)
    d->parent[x] = find_root(d,d->parent[x]);
  return d->parent[x];
}


/* Compute the union of two sets by rank. */
static void union_sets(dsu *d, int x, int y)
{
  int root_x, root_y;

  root_x = find_root(d,x);
  root_y = find_root(d,y);
  if (root_x != root_y)
  {
    if (d->rank[root_x] < d->rank[root_y])
    {
      d->parent[root_x] = root_y;
    }
    else if (d->rank[root_x] > d->rank[root_y])
    {
      d->parent[root_y] = root_x;
    }
    else
    {
      d->parent[root_y] = root_x;
      d->rank[root_x]++;
    }
  }
}


/* Compare function for sorting edges by weight. */
static int compare_edges(const void *a, const void *b)
{
  const edge *edge_a, *edge_b;

  edge_a = (edge *)a;
  edge_b = (edge *)b;
  if (edge_a->weight > edge_b->weight)
    return 1;
  else if (edge_a->weight < edge_b->weight)
    return -1;
  else
    return 0;
}


/* Kruskal's algorithm to find the g->num_v-1 edges of the minimum spanning
   tree of the edge-based graph g. */
static edge *minimum_spanning_tree_edges(edge_graph *g)
{
  edge *mst_edges, next_edge;
  dsu *d;
  int edge_count, i;
  int src_root, dst_root;

  mst_edges = malloc((g->num_v-1)*sizeof(*mst_edges));
  if (mst_edges == NULL)
    abort();

  qsort(g->edges,g->num_e,sizeof(*g->edges),compare_edges);

  d = create_dsu(g->num_v);

  edge_count = 0;
  i = 0;
  while (edge_count<g->num_v-1 && i<g->num_e)
  {
    next_edge = g->edges[i++];
    src_root = find_root(d,next_edge.src);
    dst_root = find_root(d,next_edge.dst);

    if (src_root != dst_root)
    {
      mst_edges[edge_count++] = next_edge;
      union_sets(d,src_root,dst_root);
    }
  }

  free_dsu(d);

  return mst_edges;
}


/* Create an adjacenct-list-based graph from a set of edges. */
static graph_node **create_adj_list_graph_from_edges(const edge *e, int num_v,
                                                     int num_e)
{
  graph_node **ag, *n;
  int i, src, dst, src_edge, dst_edge;

  ag = malloc(num_v*sizeof(*ag));
  if (ag == NULL)
    abort();
  for (i=0; i<num_v; i++)
    ag[i] = NULL;
  for (i=0; i<num_e; i++)
  {
    src = e[i].src;
    dst = e[i].dst;
    src_edge = e[i].src_edge;
    dst_edge = e[i].dst_edge;
    n = malloc(sizeof(*n));
    if (n == NULL)
      abort();
    n->v = src;
    n->edge_parent = dst_edge;
    n->edge_self = src_edge;
    n->next = ag[dst];
    ag[dst] = n;
    n = malloc(sizeof(*n));
    if (n == NULL)
      abort();
    n->v = dst;
    n->edge_parent = src_edge;
    n->edge_self = dst_edge;
    n->next = ag[src];
    ag[src] = n;
  }
  return ag;
}


/* Free an adjacenct-list-based graph. */
static void free_adj_list_graph(graph_node **ag, int num_v)
{
  graph_node *a, *an;
  int i;

  for (i=0; i<num_v; i++)
  {
    a = ag[i];
    while (a != NULL)
    {
      an = a->next;
      free(a);
      a = an;
    }
  }
  free(ag);
}


/* Create a minimum spanning tree from a adjacency-list-based graph that
   represents the minimum spanning tree. */
static tree_node *create_minimum_spanning_tree(graph_node **ag, int num_v)
{
  int i, k, id, sp;
  int *val, *vstack;
  tree_node **tstack, *t, *tp, *tn, *tree;
  graph_node *n;

  val = malloc(num_v*sizeof(*val));
  vstack = malloc(num_v*sizeof(*vstack));
  tstack = malloc(num_v*sizeof(*tstack));
  if (val == NULL || vstack == NULL || tstack == NULL)
    abort();

  for (i=0; i<num_v; i++)
    val[i] = UNSEEN;

  tree = NULL;
  id = 0;
  sp = 0;
  for (i=0; i<num_v; i++)
  {
    if (val[i] == UNSEEN)
    {
      t = malloc(sizeof(*t));
      if (t == NULL)
        abort();
      t->next = NULL;
      t->child = NULL;
      t->polygon_index = i;
      t->edge_parent = -1;
      t->edge_self = -1;
      tree = t;
      vstack[sp] = i;
      tstack[sp] = t;
      while (sp >= 0)
      {
        k = vstack[sp];
        tp = tstack[sp];
        sp--;
        val[k] = id++;
        tn = NULL;
        for (n=ag[k]; n!=NULL; n=n->next)
        {
          if (val[n->v] == UNSEEN)
          {
            t = malloc(sizeof(*t));
            if (t == NULL)
              abort();
            t->next = NULL;
            t->child = NULL;
            t->polygon_index = n->v;
            t->edge_parent = n->edge_parent;
            t->edge_self = n->edge_self;
            if (tp->child == NULL)
            {
              tp->child = t;
              tn = t;
            }
            else
            {
              tn->next = t;
              tn = t;
            }
            sp++;
            vstack[sp] = n->v;
            tstack[sp] = t;
            val[n->v] = SEEN;
          }
        }
      }
    }
  }

  free(tstack);
  free(vstack);
  free(val);

  return tree;
}


/* Free a tree. */
static void free_tree(tree_node *t)
{
  if (t->child != NULL)
    free_tree(t->child);
  if (t->next != NULL)
    free_tree(t->next);
  free(t);
}


/* Generate a random polyhedron unfolding of the polyhedron with num_faces
   faces and num_edges edges represented by the adjacency information of the
   faces given by adj. */
static tree_node *create_random_polyhedron_unfolding(int num_faces,
                                                     int num_edges,
                                                     edge *adj)
{
  edge_graph *eg;
  edge *mst_edges;
  graph_node **ag;
  tree_node *mst;
  int i;

  /* Create an edge-based adjacency graph from adj. */
  eg = create_edge_graph(num_faces,num_edges,adj);

  /* Set random weights for the edges in eg. */
  for (i=0; i<num_edges; i++)
    eg->edges[i].weight = (float)frand(1.0);

  /* Compute the edges of the minimum spanning tree. */
  mst_edges = minimum_spanning_tree_edges(eg);

  /* Create an adjacency-list-based graph from the edges of the minimum
     spanning tree. */
  ag = create_adj_list_graph_from_edges(mst_edges,num_faces,num_faces-1);

  /* Create an actual minimum spanning tree from the adjacency-list-based
     graph. */
  mst = create_minimum_spanning_tree(ag,num_faces);

  free_adj_list_graph(ag,num_faces);
  free(mst_edges);
  free_edge_graph(eg);

  return mst;
}


/* ------------------------------------------------------------------------- */


/* Compute the fold pose of the base polygon of the node given by node and
   the angle given by angles at the index node->fold_angle_index.  The
   function assumes that node->unfold_pose is relative to its parent node
   has been set by the caller before the call.  Furthermore, the function
   assumes that node->fold_pose has been initialized with its parent's fold
   pose.  On output, the function modifies node->fold_pose to store the
   fold pose of the polygon described by the node. */
static void compute_fold_pose(tree_node *node, const polygon *base_polygon,
                              const float *angles, float max_angle)
{
  int i, i1, i2;
  float t[3], mt[3], a[3], b[3], c[3], phi;
  vertex t1, t2;
  matrix matr, mats, matst;

  if (node->fold_angle_index >= 0)
  {
    phi = angles[node->fold_angle_index];
    node->fold_angle = ease(phi,max_angle,EASING_QUINTIC);

    i1 = node->edge_self;
    i2 = (i1+1)%base_polygon->num;

    /* Compute the translation vector.  This is the midpoint of the edge of
       the polygon that is adjacent to the parent polygon. */
    mult_matrix_vector(t1,node->unfold_pose,base_polygon->v[i1]);
    mult_matrix_vector(t2,node->unfold_pose,base_polygon->v[i2]);
    mid(t,t1,t2);
    neg(mt,t);

    /* Compute the direction that remains fixed during the rotation.  This
       is the normalized vector that points from the first to the second
       vertex of the polygon that is adjacent to the parent polygon. */
    sub(c,t2,t1);
    normalize(c);

    /* Compute the first basis vector of the plane in which the rotation is
       performed.  This is the normalized version of the translation vector
       that was computed above, except its z coordinate is 0. */
    a[0] = t[0];
    a[1] = t[1];
    a[2] = 0.0f;
    normalize(a);

    /* Compute the second basis vector of the plane in which the rotation
       is performed.  This is simply the z direction. */
    b[0] = 0.0f;
    b[1] = 0.0f;
    b[2] = 1.0f;

    /* Insert the three vectors into the matrix mats and its transpose
       matst. */
    identity_matrix(mats);
    identity_matrix(matst);
    for (i=0; i<3; i++)
    {
      mats[i][0] = a[i];
      mats[i][1] = b[i];
      mats[i][2] = c[i];
      matst[0][i] = a[i];
      matst[1][i] = b[i];
      matst[2][i] = c[i];
    }

    /* Compute the rotation matrix that rotates the polygon. */
    identity_matrix(matr);
    translate_matrix(matr,t);
    mult_matrix(matr,mats);
    rotate_xy_matrix(matr,node->fold_angle);
    mult_matrix(matr,matst);
    translate_matrix(matr,mt);

    /* Add the fold angle transformation. */
    mult_matrix(node->fold_pose,matr);
  }

  /* Add the unfolding pose to the current polygon. */
  mult_matrix(node->fold_pose,node->unfold_pose);
}


/* Determine the color data of the polygons, i.e., the colors of the faces
   and the texture coordinates of its vertices, based on the folded
   polyhedron. */
static void determine_polygon_color_data(tree_node *unfolding,
                                         const polygons *base_polygons,
                                         float max_angle, matrix color_matrix)
{
  float angles[NUM_ANGLES];
  int i, sp, pindex;
  vertex tv, c, cr;
  matrix matp;
  tree_node *node_stack[MAX_STACK], *node;
  polygon *poly;

  for (i=0; i<NUM_ANGLES; i++)
    angles[i] = max_angle;

  sp = 0;
  node_stack[sp] = unfolding;
  while (sp >= 0)
  {
    node = node_stack[sp--];
    pindex = node->polygon_index;
    poly = &base_polygons->poly[pindex];

    /* Initialize the fold pose when processing the root of the tree. */
    if (node->edge_parent < 0 || node->edge_self < 0)
      identity_matrix(node->fold_pose);

    /* The parent fold pose is initially stored in the current node.  Save
       it for later. */
    copy_matrix(matp,node->fold_pose);

    /* Compute the folding transformation to the current polygon. */
    compute_fold_pose(node,poly,angles,max_angle);

    /* Transform the polygon, compute the center of the transformed polygon,
       and normalize it to length 1. */
    for (i=0; i<3; i++)
      c[i] = 0.0f;
    c[3] = 1.0f;
    for (i=0; i<poly->num; i++)
    {
      /* Transform the vertices of the base polygon with the fold
         transformation. */
      mult_matrix_vector(tv,node->fold_pose,poly->v[i]);
      add(c,c,tv);
      /* Set the 3D texture coordinate of the current vertex to the position
         of the vertex on a unit sphere. */
      normalize(tv);
      copy_vector(poly->t[i],tv);
    }
    mult_matrix_vector(cr,color_matrix,c);
    normalize(cr);

    /* Set the color of the polygon. */
    for (i=0; i<3; i++)
      poly->c[i] = 0.5f*(cr[i]+1.0f);
    poly->c[3] = 1.0f;

    if (node->child != NULL)
    {
      copy_matrix(node->child->fold_pose,node->fold_pose);
      node_stack[++sp] = node->child;
    }
    if (node->next != NULL)
    {
      copy_matrix(node->next->fold_pose,matp);
      node_stack[++sp] = node->next;
    }
  }
}


/* Determine the poses of the polygons in the unfolding. */
static void determine_unfolding_poses(tree_node *unfolding,
                                      const polygons *base_polygons)
{
  tree_node *node_stack[MAX_STACK], *node;
  int sp, p1, p2, s1, s2, ai, pindex;
  float vp[3], vs[3], c[3], m[3], t[3], dp, phi;
  polygon *poly;

  ai = 0;
  sp = 0;
  node_stack[sp] = unfolding;
  while (sp >= 0)
  {
    node = node_stack[sp--];
    pindex = node->polygon_index;
    poly = &base_polygons->poly[pindex];

    if (node->edge_parent >= 0 && node->edge_self >= 0)
    {
      node->fold_angle_index = ai++;

      /* The unfold poses are all relative to its parent node.  Therefore,
         they must be initialized with the identity matrix. */
      identity_matrix(node->unfold_pose);

      /* The rotation angle phi is determined from the vectors corresponding
         to the edges that are adjacent to each other in the polyhedron,
         i.e., the edges that should align in the unfolded state after the
         transformation of the base polygon. */
      p1 = node->edge_parent;
      p2 = (p1+1)%poly->num;
      s1 = node->edge_self;
      s2 = (s1+1)%poly->num;

      /* We have to take into account that, since both polygons must be
         oriented counterclockwise, the edge in the transformed polygon
         must be taken in the opposite direction to compute the correct
         rotation angle. */
      sub(vp,poly->v[p2],poly->v[p1]);
      sub(vs,poly->v[s1],poly->v[s2]);
      normalize(vp);
      normalize(vs);
      cross(c,vs,vp);
      dp = dot(vs,vp);
      phi = (180.0f/M_PI_F)*atan2f(c[2],dp);

      /* Compute the translation vector.  This is the midpoint of the edge
         in the parent polygon that is adjacent to the transformed polygon,
         scaled by a factor of 2.  However, since all polygons must lie in
         one plane in the unfolded state, we must set the z coordinate of
         the translation to 0. */
      mid(m,poly->v[p1],poly->v[p2]);
      m[2] = 0.0f;
      scale(t,m,2.0f);

      /* Compute the transformation matrix of the current polygon with
         respect to its parent. */
      translate_matrix(node->unfold_pose,t);
      rotate_xy_matrix(node->unfold_pose,phi);
    }
    else
    {
      node->fold_angle_index = -1;

      /* The unfold pose for the root of the tree is the identity matrix by
         definition. */
      identity_matrix(node->unfold_pose);
    }

    if (node->child != NULL)
      node_stack[++sp] = node->child;
    if (node->next != NULL)
      node_stack[++sp] = node->next;
  }
}


/* Initialize the base polygons. */
static polygons *init_base_polygons(tree_node *unfolding,
                                    const polygon *base_polygon)
{
  tree_node *node_stack[MAX_STACK], *node;
  int i, num_poly, sp;
  polygon *poly;
  polygons *polys;

  /* Count the number of polygons in the unfolding. */
  num_poly = 0;
  sp = 0;
  node_stack[sp] = unfolding;
  while (sp >= 0)
  {
    node = node_stack[sp--];
    num_poly++;
    if (node->child != NULL)
      node_stack[++sp] = node->child;
    if (node->next != NULL)
      node_stack[++sp] = node->next;
  }

  polys = malloc(sizeof(*polys));
  poly = malloc(num_poly*sizeof(*poly));
  if (polys == NULL || poly == NULL)
    abort();

  polys->poly = poly;
  polys->num = num_poly;

  /* Copy the base polygon to all base polygons and set the texture
     coordinates and color to 0. */
  for (i=0; i<num_poly; i++)
  {
    poly[i].v = malloc(base_polygon->num*sizeof(*poly[i].v));
    poly[i].t = malloc(base_polygon->num*sizeof(*poly[i].t));
    if (poly[i].v == NULL || poly[i].t == NULL)
      abort();
    poly[i].num = base_polygon->num;
    memcpy(poly[i].v,base_polygon->v,base_polygon->num*sizeof(*poly[i].v));
    memcpy(poly[i].n,base_polygon->n,sizeof(poly[i].n));
    memset(poly[i].t,0,base_polygon->num*sizeof(*poly[i].t));
    memset(poly[i].c,0,sizeof(poly[i].c));
  }

  return polys;
}


/* Free the base polygons. */
static void free_base_polygons(polygons *base_polygons)
{
  int i;

  for (i=0; i<base_polygons->num; i++)
  {
    free(base_polygons->poly[i].v);
    free(base_polygons->poly[i].t);
  }
  free(base_polygons->poly);
  free(base_polygons);
}


/* Initialize the color matrix that determines the random face colors. */
static void init_color_matrix(matrix color_matrix)
{
  rnd_rot_matrix(color_matrix);
}


/* Initialize the polygon unfolding for a polyhedron of type poly. */
static void init_polygon_unfolding(ModeInfo *mi, int poly)
{
  platonicfoldingstruct *pf = &platonicfolding[MI_SCREEN(mi)];

  switch (poly)
  {
    case TETRAHEDRON:
      free_base_polygons(pf->base_polygons);
      free_tree(pf->polygon_unfolding);
      pf->polygon_unfolding =
        create_random_polyhedron_unfolding(TETRAHEDRON_NUM_FACES,
                                           TETRAHEDRON_NUM_EDGES,
                                           tetrahedron_edges);
      pf->max_angle = TETRAHEDRON_MAX_ANGLE;
      pf->delta_angle = TETRAHEDRON_DELTA_ANGLE;
      pf->num_fold_angles = TETRAHEDRON_NUM_FACES-1;
      pf->eye_pos = tetrahedron_eye_pos;
      pf->base_polygons = init_base_polygons(pf->polygon_unfolding,
                                             &tetrahedron_triangle);
      determine_unfolding_poses(pf->polygon_unfolding,pf->base_polygons);
      determine_polygon_color_data(pf->polygon_unfolding,pf->base_polygons,
                                   pf->max_angle,pf->color_matrix);
      break;
    case HEXAHEDRON:
      free_base_polygons(pf->base_polygons);
      free_tree(pf->polygon_unfolding);
      pf->polygon_unfolding =
        create_random_polyhedron_unfolding(HEXAHEDRON_NUM_FACES,
                                           HEXAHEDRON_NUM_EDGES,
                                           hexahedron_edges);
      pf->max_angle = HEXAHEDRON_MAX_ANGLE;
      pf->delta_angle = HEXAHEDRON_DELTA_ANGLE;
      pf->num_fold_angles = HEXAHEDRON_NUM_FACES-1;
      pf->eye_pos = hexahedron_eye_pos;
      pf->base_polygons = init_base_polygons(pf->polygon_unfolding,
                                             &hexahedron_square);
      determine_unfolding_poses(pf->polygon_unfolding,pf->base_polygons);
      determine_polygon_color_data(pf->polygon_unfolding,pf->base_polygons,
                                   pf->max_angle,pf->color_matrix);
      break;
    case OCTAHEDRON:
      free_base_polygons(pf->base_polygons);
      free_tree(pf->polygon_unfolding);
      pf->polygon_unfolding =
        create_random_polyhedron_unfolding(OCTAHEDRON_NUM_FACES,
                                           OCTAHEDRON_NUM_EDGES,
                                           octahedron_edges);
      pf->max_angle = OCTAHEDRON_MAX_ANGLE;
      pf->delta_angle = OCTAHEDRON_DELTA_ANGLE;
      pf->num_fold_angles = OCTAHEDRON_NUM_FACES-1;
      pf->eye_pos = octahedron_eye_pos;
      pf->base_polygons = init_base_polygons(pf->polygon_unfolding,
                                             &octahedron_triangle);
      determine_unfolding_poses(pf->polygon_unfolding,pf->base_polygons);
      determine_polygon_color_data(pf->polygon_unfolding,pf->base_polygons,
                                   pf->max_angle,pf->color_matrix);
      break;
    case DODECAHEDRON:
      free_base_polygons(pf->base_polygons);
      free_tree(pf->polygon_unfolding);
      pf->polygon_unfolding =
        create_random_polyhedron_unfolding(DODECAHEDRON_NUM_FACES,
                                           DODECAHEDRON_NUM_EDGES,
                                           dodecahedron_edges);
      pf->max_angle = DODECAHEDRON_MAX_ANGLE;
      pf->delta_angle = DODECAHEDRON_DELTA_ANGLE;
      pf->num_fold_angles = DODECAHEDRON_NUM_FACES-1;
      pf->eye_pos = dodecahedron_eye_pos;
      pf->base_polygons = init_base_polygons(pf->polygon_unfolding,
                                             &dodecahedron_pentagon);
      determine_unfolding_poses(pf->polygon_unfolding,pf->base_polygons);
      determine_polygon_color_data(pf->polygon_unfolding,pf->base_polygons,
                                   pf->max_angle,pf->color_matrix);
      break;
    case ICOSAHEDRON:
      free_base_polygons(pf->base_polygons);
      free_tree(pf->polygon_unfolding);
      pf->polygon_unfolding =
        create_random_polyhedron_unfolding(ICOSAHEDRON_NUM_FACES,
                                           ICOSAHEDRON_NUM_EDGES,
                                           icosahedron_edges);
      pf->max_angle = ICOSAHEDRON_MAX_ANGLE;
      pf->delta_angle = ICOSAHEDRON_DELTA_ANGLE;
      pf->num_fold_angles = ICOSAHEDRON_NUM_FACES-1;
      pf->eye_pos = icosahedron_eye_pos;
      pf->base_polygons = init_base_polygons(pf->polygon_unfolding,
                                             &icosahedron_triangle);
      determine_unfolding_poses(pf->polygon_unfolding,pf->base_polygons);
      determine_polygon_color_data(pf->polygon_unfolding,pf->base_polygons,
                                   pf->max_angle,pf->color_matrix);
      break;
  }
}


/* ------------------------------------------------------------------------- */


/* Generate the vertex, normal, color, and texture buffers for a polygon
   folding. */
static int gen_polygon_folding_data(tree_node *unfolding,
                                    const polygons *base_polygons,
                                    const float *angles, float max_angle,
                                    vertex *vtx, normal *nrm, color *col,
                                    texcoord *tex, int *num)
{
  int i, j, n, num_poly, sp, pindex;
  vertex tv, tn;
  matrix matp;
  tree_node *node_stack[MAX_STACK], *node;
  polygon *poly;

  num_poly = 0;
  n = 0;
  sp = 0;
  node_stack[sp] = unfolding;
  while (sp >= 0)
  {
    node = node_stack[sp--];
    pindex = node->polygon_index;
    poly = &base_polygons->poly[pindex];

    /* Initialize the fold pose when processing the root of the tree. */
    if (node->edge_parent < 0 || node->edge_self < 0)
      identity_matrix(node->fold_pose);

    /* The parent's fold pose is initially stored in the current node.  Save
       it for later. */
    copy_matrix(matp,node->fold_pose);

    /* Compute the folding transformation to the current polygon. */
    compute_fold_pose(node,poly,angles,max_angle);

    /* Transform the normal vector of the polygon.  Note that we assume
       that node->fold_pose is a rigid transformation.  Otherwise, we would
       have to compute and use its inverse transpose. */
    mult_matrix_vector(tn,node->fold_pose,poly->n);

    for (i=0; i<poly->num; i++)
    {
      /* Transform the vertices of the base polygon with the fold
         transformation. */
      mult_matrix_vector(tv,node->fold_pose,poly->v[i]);

      for (j=0; j<4; j++)
        vtx[n][j] = tv[j];
      for (j=0; j<4; j++)
        nrm[n][j] = tn[j];
      for (j=0; j<4; j++)
        col[n][j] = poly->c[j];
      for (j=0; j<3; j++)
        tex[n][j] = poly->t[i][j];
      n++;
    }

    num[num_poly] = poly->num;
    num_poly++;

    if (node->child != NULL)
    {
      copy_matrix(node->child->fold_pose,node->fold_pose);
      node_stack[++sp] = node->child;
    }
    if (node->next != NULL)
    {
      copy_matrix(node->next->fold_pose,matp);
      node_stack[++sp] = node->next;
    }
  }

  return num_poly;
}


/* Draw the polygon folding using OpenGL's fixed functionality. */
static int polygon_folding_ff(ModeInfo *mi)
{
  static const GLfloat light_ambient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
  static const GLfloat light_diffuse[] = { 0.7f, 0.7f, 0.7f, 1.0f };
  static const GLfloat light_specular[] = { 0.75f, 0.75f, 0.75f, 1.0f };
  static const GLfloat light_pos[] = { 1.0f, 1.0f, 1.0f, 0.0f };
  static const GLfloat mat_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  platonicfoldingstruct *pf = &platonicfolding[MI_SCREEN(mi)];
  float qu[4], qr[16];
  int i, j, n, num_poly;

  /* Collect the polygon data to draw. */
  num_poly = gen_polygon_folding_data(pf->polygon_unfolding,pf->base_polygons,
                                      pf->angle,pf->max_angle,pf->vtx,
                                      pf->nrm,pf->col,pf->tex,pf->num);

  glViewport(0,0,pf->WindW,pf->WindH);

  glClearColor(0.0f,0.0f,0.0f,0.0f);
  glClearDepth(1.0f);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  if (pf->aspect >= 1.0f)
    gluPerspective(45.0,pf->aspect,0.1,30.0);
  else
    gluPerspective(360.0/M_PI*atan(tan(45.0*M_PI/360.0)/pf->aspect),
                   pf->aspect,0.1,30.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  /* Note: in the unfolded state, all visible, i.e, front-facing, triangles
     are oriented counterclockwise.  This means that in the folded state,
     all visible triangles, i.e., the triangles that describe the surface of
     the polyhedron, are back-facing.  Therefore, we must disable face
     culling. */
  glFrontFace(GL_CCW);
  glDisable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glShadeModel(GL_SMOOTH);
  glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
  glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glLightfv(GL_LIGHT0,GL_AMBIENT,light_ambient);
  glLightfv(GL_LIGHT0,GL_DIFFUSE,light_diffuse);
  glLightfv(GL_LIGHT0,GL_SPECULAR,light_specular);
  glLightfv(GL_LIGHT0,GL_POSITION,light_pos);
  glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,mat_specular);
  glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,30.0f);

  gltrackball_get_quaternion(pf->trackball,qu);
  quat_to_rotmat_trackball(qu,qr);

  gluLookAt(pf->eye_pos[0],pf->eye_pos[1],pf->eye_pos[2],0.0,0.0,0.0,
            0.0,1.0,0.0);
  glTranslatef(pf->poly_pos[0],pf->poly_pos[1],pf->poly_pos[2]);
  glMultMatrixf(qr);
  glRotatef(pf->alpha,1.0f,0.0f,0.0f);
  glRotatef(pf->beta,0.0f,1.0f,0.0f);
  glRotatef(pf->delta,0.0f,0.0f,1.0f);

  /* Draw the collected polygons. */
  n = 0;
  for (i=0; i<num_poly; i++)
  {
    glBegin(GL_TRIANGLE_FAN);
    for (j=0; j<pf->num[i]; j++)
    {
      glColor3fv(pf->col[n]);
      glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,pf->col[n]);
      glNormal3fv(pf->nrm[n]);
      glVertex4fv(pf->vtx[n]);
      n++;
    }
    glEnd();
  }

  return num_poly;
}


#ifdef HAVE_GLSL

/* Draw the polygon folding using OpenGL's programmable functionality. */
static int polygon_folding_pf(ModeInfo *mi)
{
  static const GLfloat light_model_amb[] = { 0.2f, 0.2f, 0.2f, 1.0f };
  static const GLfloat light_ambient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
  static const GLfloat light_diffuse[] = { 0.7f, 0.7f, 0.7f, 1.0f };
  static const GLfloat light_model_amb_earth[] = { 0.3f, 0.3f, 0.3f, 1.0f };
  static const GLfloat light_ambient_earth[] = { 0.5f, 0.5f, 0.5f, 1.0f };
  static const GLfloat light_diffuse_earth[] = { 0.5f, 0.5f, 0.5f, 1.0f };
  static const GLfloat light_specular[] = { 0.75f, 0.75f, 0.75f, 1.0f };
  static const GLfloat light_pos[] = { 1.0f, 1.0f, 1.0f, 0.0f };
  static const GLfloat mat_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  static const GLfloat mat_diff_white[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  platonicfoldingstruct *pf = &platonicfolding[MI_SCREEN(mi)];
  float p_mat[16], mv_mat[16], sun_pos[4];
  float qu[4], qr[16];
  float light_direction[4], half_vector[3];
  int i, n, num_poly;

  /* Collect the polygon data to draw. */
  num_poly = gen_polygon_folding_data(pf->polygon_unfolding,pf->base_polygons,
                                      pf->angle,pf->max_angle,pf->vtx,
                                      pf->nrm,pf->col,pf->tex,pf->num);

  /* Count the number of elements in the polygon data. */
  n = 0;
  for (i=0; i<num_poly; i++)
    n += pf->num[i];

  if (pf->use_vao)
    glBindVertexArray(pf->vertex_array_object);

  /* Copy the collected polygon data to the respective buffer. */
  glBindBuffer(GL_ARRAY_BUFFER,pf->vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER,4*n*sizeof(GLfloat),pf->vtx,GL_STREAM_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

  glBindBuffer(GL_ARRAY_BUFFER,pf->normal_buffer);
  glBufferData(GL_ARRAY_BUFFER,4*n*sizeof(GLfloat),pf->nrm,GL_STREAM_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

  glBindBuffer(GL_ARRAY_BUFFER,pf->color_buffer);
  glBufferData(GL_ARRAY_BUFFER,4*n*sizeof(GLfloat),pf->col,GL_STREAM_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

  glBindBuffer(GL_ARRAY_BUFFER,pf->tex_buffer);
  glBufferData(GL_ARRAY_BUFFER,3*n*sizeof(GLfloat),pf->tex,GL_STREAM_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

  glsl_Identity(p_mat);
  if (pf->aspect >= 1.0f)
    glsl_Perspective(p_mat,45.0f,pf->aspect,0.1f,30.0f);
  else
    glsl_Perspective(p_mat,
                     360.0f/M_PI_F*atanf(tanf(45.0f*M_PI_F/360.0f)/
                                         pf->aspect),
                     pf->aspect,0.1f,30.0f);

  gltrackball_get_quaternion(pf->trackball,qu);
  quat_to_rotmat_trackball(qu,qr);

  glsl_Identity(mv_mat);
  glsl_LookAt(mv_mat,pf->eye_pos[0],pf->eye_pos[1],pf->eye_pos[2],
              0.0f,0.0f,0.0f,0.0f,1.0f,0.0f);
  glsl_Translate(mv_mat,pf->poly_pos[0],pf->poly_pos[1],pf->poly_pos[2]);
  glsl_MultMatrix(mv_mat,qr);
  glsl_Rotate(mv_mat,pf->alpha,1.0f,0.0f,0.0f);
  glsl_Rotate(mv_mat,pf->beta,0.0f,1.0f,0.0f);
  glsl_Rotate(mv_mat,pf->delta,0.0f,0.0f,1.0f);

  if (!pf->use_textures)
  {
    light_direction[0] = light_pos[0];
    light_direction[1] = light_pos[1];
    light_direction[2] = light_pos[2];
    light_direction[3] = 0.0f;
    normalize(light_direction);
    half_vector[0] = light_direction[0];
    half_vector[1] = light_direction[1];
    half_vector[2] = light_direction[2]+1.0f;
    normalize(half_vector);
  }
  else
  {
    get_sun_direction(get_current_julian_date(),sun_pos);
    if (!pf->north_up)
    {
      sun_pos[0] = -sun_pos[0];
      sun_pos[2] = -sun_pos[2];
    }
    glsl_MultMatrixVector(light_direction,mv_mat,sun_pos);
    normalize(light_direction);
    half_vector[0] = light_direction[0];
    half_vector[1] = light_direction[1];
    half_vector[2] = light_direction[2];
    normalize(half_vector);
  }

  glUseProgram(pf->shader_program);

  glViewport(0,0,pf->WindW,pf->WindH);

  glClearColor(0.0f,0.0f,0.0f,0.0f);
  glClearDepth(1.0f);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  glFrontFace(GL_CCW);
  glDisable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);

  if (!pf->use_textures)
  {
    glUniform4fv(pf->glbl_ambient_index,1,light_model_amb);
    glUniform4fv(pf->lt_ambient_index,1,light_ambient);
    glUniform4fv(pf->lt_diffuse_index,1,light_diffuse);
  }
  else
  {
    glUniform4fv(pf->glbl_ambient_index,1,light_model_amb_earth);
    glUniform4fv(pf->lt_ambient_index,1,light_ambient_earth);
    glUniform4fv(pf->lt_diffuse_index,1,light_diffuse_earth);
  }
  glUniform4fv(pf->lt_specular_index,1,light_specular);
  glUniform3fv(pf->lt_direction_index,1,light_direction);
  glUniform3fv(pf->lt_halfvect_index,1,half_vector);
  glUniform4fv(pf->ambient_index,1,mat_diff_white);
  glUniform4fv(pf->diffuse_index,1,mat_diff_white);
  glUniform4fv(pf->specular_index,1,mat_specular);
  glUniform1f(pf->shininess_index,30.0f);
  glUniform1i(pf->north_up_index,pf->north_up);
  glUniform1i(pf->bool_textures_index,pf->use_textures);
  glUniform3fv(pf->magma_offs_index,1,pf->magma_tex_offset);

  glUniformMatrix4fv(pf->proj_index,1,GL_FALSE,p_mat);
  glUniformMatrix4fv(pf->mv_index,1,GL_FALSE,mv_mat);

  if (pf->textures_supported)
  {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D,pf->magma_tex);
    glUniform1i(pf->samp_mgm_index,0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D,pf->earth_tex[0]);
    glUniform1i(pf->samp_day_index,1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D,pf->earth_tex[1]);
    glUniform1i(pf->samp_ngt_index,2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D,pf->earth_tex[2]);
    glUniform1i(pf->samp_wtr_index,3);
  }

  /* Draw the collected polygons. */
  glEnableVertexAttribArray(pf->pos_index);
  glBindBuffer(GL_ARRAY_BUFFER,pf->vertex_buffer);
  glVertexAttribPointer(pf->pos_index,4,GL_FLOAT,GL_FALSE,0,0);

  glEnableVertexAttribArray(pf->normal_index);
  glBindBuffer(GL_ARRAY_BUFFER,pf->normal_buffer);
  glVertexAttribPointer(pf->normal_index,4,GL_FLOAT,GL_FALSE,0,0);

  glEnableVertexAttribArray(pf->color_index);
  glBindBuffer(GL_ARRAY_BUFFER,pf->color_buffer);
  glVertexAttribPointer(pf->color_index,4,GL_FLOAT,GL_FALSE,0,0);

  glEnableVertexAttribArray(pf->tex_index);
  glBindBuffer(GL_ARRAY_BUFFER,pf->tex_buffer);
  glVertexAttribPointer(pf->tex_index,3,GL_FLOAT,GL_FALSE,0,0);

  n = 0;
  for (i=0; i<num_poly; i++)
  {
    glDrawArrays(GL_TRIANGLE_FAN,n,pf->num[i]);
    n += pf->num[i];
  }

  glDisableVertexAttribArray(pf->pos_index);
  glDisableVertexAttribArray(pf->normal_index);
  glDisableVertexAttribArray(pf->color_index);
  glDisableVertexAttribArray(pf->tex_index);
  glBindBuffer(GL_ARRAY_BUFFER,0);

  if (pf->use_vao)
    glBindVertexArray(0);

  glUseProgram(0);

  return num_poly;
}

#endif /* HAVE_GLSL */


/* Display the Platonic solid unfolding and folding. */
static void display_platonicfolding(ModeInfo *mi)
{
  platonicfoldingstruct *pf = &platonicfolding[MI_SCREEN(mi)];
  int i, poly;
  bool change_dir;
  float t;

  if (!pf->button_pressed)
  {
    if (pf->anim_state == ANIM_INIT)
    {
#ifdef HAVE_GLSL
      /* Select whether to use textures.  Textures are used in one third of
         the animations if the user has selected random colors. */
      if (pf->random_colors)
        pf->use_textures = pf->textures_supported && frand(1.0) < 1.0/3.0;
      else
        pf->use_textures = (pf->colors == COLORS_EARTH);
      
      /* Select a random offset for the magma texture. */
      pf->magma_tex_offset[0] = 0.2f*((float)frand(1.0)-0.5f);
      pf->magma_tex_offset[1] = 0.2f*((float)frand(1.0)-0.5f);
      pf->magma_tex_offset[2] = 0.2f*((float)frand(1.0)-0.5f);
#endif /* HAVE_GLSL */

      /* Select whether the north or south pole are facing upwards in the
         texture.  Both cases occur with equal probability. */
      pf->north_up = frand(1.0) < 0.5;

      /* Select whether to fold upwards or downwards.  Both cases occur with
         equal probability. */
      pf->alpha = frand(1.0) < 0.5 ? 300.0f : 120.0f;

      /* Set the rotation around the y axis to 0. */
      pf->beta = 0.0f;

      /* Initialize the rotation around the z axis to a random angle. */
      pf->delta = (float)frand(360.0);

      /* Set the angle increment for the rotation around the z axis. */
      if (pf->north_up)
        pf->delta_delta = 0.5f;
      else
        pf->delta_delta = -0.5f;

      /* Select a random polyhedron that is different from the last
         polyhedron. */
      do
        poly = (int)floor(frand(5.0));
      while (poly == pf->anim_poly);
      pf->anim_poly = poly;
      init_color_matrix(pf->color_matrix);
      init_polygon_unfolding(mi,pf->anim_poly);
      if (pf->num_foldings == RANDOM_NUM_FOLDINGS)
      {
        if (pf->anim_poly == TETRAHEDRON)
          pf->anim_remaining = 3+(int)floor(frand(3.0));
        else if (pf->anim_poly == HEXAHEDRON || pf->anim_poly == OCTAHEDRON)
          pf->anim_remaining = 4+(int)floor(frand(5.0));
        else /* pf->anim_poly==DODECAHEDRON || pf->anim_poly==ICOSAHEDRON */
          pf->anim_remaining = 6+(int)floor(frand(7.0));
      }
      else
      {
        pf->anim_remaining = pf->num_foldings;
      }

      /* Set all angles to the folded state. */
      for (i=0; i<pf->num_fold_angles; i++)
        pf->angle[i] = pf->max_angle;
      for (; i<NUM_ANGLES; i++)
        pf->angle[i] = 0.0f;

      /* Initialize the fold angle index. */
      pf->fold_angle = 0;

      /* Set the appear and disappear parameters of the polyhedron. */
      if (pf->aspect >= 1.0f)
      {
        pf->spoly_pos[0] = 0.0f;
        pf->spoly_pos[1] = -pf->eye_pos[2]*(2.0f/3.0f);
        pf->spoly_pos[2] = 0.0f;
        pf->dpoly_pos[0] = 0.0f;
        pf->dpoly_pos[1] = pf->eye_pos[2]*(2.0f/3.0f);
        pf->dpoly_pos[2] = 0.0f;
      }
      else
      {
        pf->spoly_pos[0] = -pf->eye_pos[2]*(2.0f/3.0f);
        pf->spoly_pos[1] = 0.0f;
        pf->spoly_pos[2] = 0.0f;
        pf->dpoly_pos[0] = pf->eye_pos[2]*(2.0f/3.0f);
        pf->dpoly_pos[1] = 0.0f;
        pf->dpoly_pos[2] = 0.0f;
      }
      pf->poly_pos[0] = pf->spoly_pos[0];
      pf->poly_pos[1] = pf->spoly_pos[1];
      pf->poly_pos[2] = pf->spoly_pos[2];

      pf->anim_num_steps = 180;
      pf->anim_step = 0;
      pf->anim_state = ANIM_APPEAR;
    }

    if (pf->anim_state == ANIM_APPEAR)
    {
      t = (float)pf->anim_step/(float)pf->anim_num_steps;
      t = ease(t,1.0f,EASING_DECEL);
      pf->poly_pos[0] = pf->spoly_pos[0]+t*pf->dpoly_pos[0];
      pf->poly_pos[1] = pf->spoly_pos[1]+t*pf->dpoly_pos[1];
      pf->poly_pos[2] = pf->spoly_pos[2]+t*pf->dpoly_pos[2];

      if (pf->rotate)
        pf->delta += pf->delta_delta;

      pf->anim_step++;
      if (pf->anim_step > pf->anim_num_steps)
      {
        /* Select the unfolding mode.  Separate unfolding is used in one
           fourth of the cases. */
        pf->anim_state =
          frand(1.0) < 0.25 ? ANIM_UNFOLD_SEP : ANIM_UNFOLD_JNT;
      }
    }
    else if (pf->anim_state == ANIM_DISAPPEAR)
    {
      t = (float)pf->anim_step/(float)pf->anim_num_steps;
      t = 1.0f+ease(t,1.0f,EASING_ACCEL);
      pf->poly_pos[0] = pf->spoly_pos[0]+t*pf->dpoly_pos[0];
      pf->poly_pos[1] = pf->spoly_pos[1]+t*pf->dpoly_pos[1];
      pf->poly_pos[2] = pf->spoly_pos[2]+t*pf->dpoly_pos[2];

      if (pf->rotate)
        pf->delta += pf->delta_delta;

      pf->anim_step++;
      if (pf->anim_step > pf->anim_num_steps)
        pf->anim_state = ANIM_INIT;
    }
    else if (pf->anim_state == ANIM_UNFOLD_JNT)
    {
      change_dir = false;
      for (i=0; i<pf->num_fold_angles; i++)
      {
        pf->angle[i] -= pf->delta_angle/3.0f;
        if (pf->angle[i] < 0.0f)
        {
          pf->angle[i] = 0.0f;
          change_dir = true;
        }
      }

      if (pf->rotate)
        pf->delta += pf->delta_delta;

      if (change_dir)
        pf->anim_state = ANIM_FOLD_JNT;
    }
    else if (pf->anim_state == ANIM_FOLD_JNT)
    {
      change_dir = false;
      for (i=0; i<pf->num_fold_angles; i++)
      {
        pf->angle[i] += pf->delta_angle/3.0f;
        if (pf->angle[i] > pf->max_angle)
        {
          pf->angle[i] = pf->max_angle;
          change_dir = true;
        }
      }

      if (pf->rotate)
        pf->delta += pf->delta_delta;

      if (change_dir)
      {
        pf->anim_remaining--;
        if (pf->anim_remaining > 0)
        {
          /* Create a new unfolding of the current polhedron type. */
          init_polygon_unfolding(mi,pf->anim_poly);
          /* Select the unfolding mode.  Separate unfolding is used in one
             fourth of the cases. */
          pf->anim_state =
            frand(1.0) < 0.25 ? ANIM_UNFOLD_SEP : ANIM_UNFOLD_JNT;
        }
        else
        {
          pf->anim_num_steps = 180;
          pf->anim_step = 0;
          pf->anim_state = ANIM_DISAPPEAR;
        }
      }
    }
    else if (pf->anim_state == ANIM_UNFOLD_SEP)
    {
      change_dir = false;
      pf->angle[pf->fold_angle] -= pf->delta_angle;
      if (pf->angle[pf->fold_angle] < 0.0f)
      {
        pf->angle[pf->fold_angle] = 0.0f;
        pf->fold_angle++;
        if (pf->fold_angle >= pf->num_fold_angles)
        {
          pf->fold_angle = pf->num_fold_angles-1;
          change_dir = true;
        }
      }

      if (pf->rotate)
        pf->delta += pf->delta_delta;

      if (change_dir)
        pf->anim_state = ANIM_FOLD_SEP;
    }
    else if (pf->anim_state == ANIM_FOLD_SEP)
    {
      change_dir = false;
      pf->angle[pf->fold_angle] += pf->delta_angle;
      if (pf->angle[pf->fold_angle] > pf->max_angle)
      {
        pf->angle[pf->fold_angle] = pf->max_angle;
        pf->fold_angle--;
        if (pf->fold_angle < 0)
        {
          pf->fold_angle = 0;
          change_dir = true;
        }
      }

      if (pf->rotate)
        pf->delta += pf->delta_delta;

      if (change_dir)
      {
        pf->anim_remaining--;
        if (pf->anim_remaining > 0)
        {
          /* Create a new unfolding of the current polhedron type. */
          init_polygon_unfolding(mi,pf->anim_poly);
          /* Select the unfolding mode.  Separate unfolding is used in one
             fourth of the cases. */
          pf->anim_state =
            frand(1.0) < 0.25 ? ANIM_UNFOLD_SEP : ANIM_UNFOLD_JNT;
        }
        else
        {
          pf->anim_num_steps = 180;
          pf->anim_step = 0;
          pf->anim_state = ANIM_DISAPPEAR;
        }
      }
    }
  }

  gltrackball_rotate(pf->trackball);

#ifdef HAVE_GLSL
  if (pf->use_shaders)
    mi->polygon_count = polygon_folding_pf(mi);
  else
#endif /* HAVE_GLSL */
    mi->polygon_count = polygon_folding_ff(mi);
}



/* ------------------------------------------------------------------------- */


#ifdef HAVE_GLSL


/* Set up and enable texturing on our object */
static void setup_xpm_texture(ModeInfo *mi, const unsigned char *data,
                              unsigned long size)
{
  platonicfoldingstruct *pf = &platonicfolding[MI_SCREEN(mi)];
  XImage *image;
  GLint max_aniso, aniso;

  image = image_data_to_ximage(MI_DISPLAY(mi),MI_VISUAL(mi),data,size);
#ifndef HAVE_JWZGLES
  glEnable(GL_TEXTURE_2D);
#endif
  glPixelStorei(GL_UNPACK_ALIGNMENT,1);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,image->width,image->height,0,GL_RGBA,
               GL_UNSIGNED_BYTE,image->data);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  if (pf->use_mipmaps)
  {
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,4);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
  }
  else
  {
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  }
  if (pf->aniso_textures)
  {
    glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,&max_aniso);
    aniso = MIN(max_aniso,10);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_ANISOTROPY_EXT,aniso);
  }
  XDestroyImage(image);
}


/* Generate the textures that show the the earth by day and night. */
static void gen_earth_textures(ModeInfo *mi)
{
  platonicfoldingstruct *pf = &platonicfolding[MI_SCREEN(mi)];

  glGenTextures(3,pf->earth_tex);

  /* Set up the earth by day texture. */
  glBindTexture(GL_TEXTURE_2D,pf->earth_tex[0]);
  setup_xpm_texture(mi,earth_png,sizeof(earth_png));

  /* Set up the earth by night texture. */
  glBindTexture(GL_TEXTURE_2D,pf->earth_tex[1]);
  setup_xpm_texture(mi,earth_night_png,sizeof(earth_night_png));

  /* Set up the earth water texture. */
  glBindTexture(GL_TEXTURE_2D,pf->earth_tex[2]);
  setup_xpm_texture(mi,earth_water_png,sizeof(earth_water_png));

  glBindTexture(GL_TEXTURE_2D,0);
}


/* Generate the magma texture. */
static void gen_magma_texture(ModeInfo *mi)
{
  platonicfoldingstruct *pf = &platonicfolding[MI_SCREEN(mi)];
  GLint max_aniso, aniso;

  pf->magma_tex_rg = gen_3d_magma_texture_rg(MAGMA_TEX_SIZE);

#ifndef HAVE_JWZGLES
  glEnable(GL_TEXTURE_3D);
#endif
  glGenTextures(1,&pf->magma_tex);
  glBindTexture(GL_TEXTURE_3D,pf->magma_tex);
  glPixelStorei(GL_UNPACK_ALIGNMENT,1);
#ifdef HAVE_JWZGLES
  glTexImage3D(GL_TEXTURE_3D,0,GL_RG8,MAGMA_TEX_SIZE,MAGMA_TEX_SIZE,
               MAGMA_TEX_SIZE,0,GL_RG,GL_UNSIGNED_BYTE,pf->magma_tex_rg);
#else
  glTexImage3D(GL_TEXTURE_3D,0,GL_RG,MAGMA_TEX_SIZE,MAGMA_TEX_SIZE,
               MAGMA_TEX_SIZE,0,GL_RG,GL_UNSIGNED_BYTE,pf->magma_tex_rg);
#endif
  glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  if (pf->use_mipmaps)
  {
    glGenerateMipmap(GL_TEXTURE_3D);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAX_LEVEL,4);
  }
  else
  {
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  }
  if (pf->aniso_textures)
  {
    glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,&max_aniso);
    aniso = MIN(max_aniso,10);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAX_ANISOTROPY_EXT,aniso);
  }
  glBindTexture(GL_TEXTURE_2D,0);
}


/* Initialize the programmable OpenGL functionality. */
static void init_glsl(ModeInfo *mi)
{
  platonicfoldingstruct *pf = &platonicfolding[MI_SCREEN(mi)];
  GLint gl_major, gl_minor, glsl_major, glsl_minor;
  GLboolean gl_gles3;
  const char *gl_ext;
  const GLchar *vertex_shader_source[2];
  const GLchar *fragment_shader_source[2];

  pf->use_textures = false;
  pf->use_mipmaps = false;
  pf->use_vao = False;

  pf->magma_tex_offset[0] = 0.0f;
  pf->magma_tex_offset[1] = 0.0f;
  pf->magma_tex_offset[2] = 0.0f;

  pf->use_shaders = false;
  pf->textures_supported = false;
  pf->aniso_textures = false;
  pf->shader_program = 0;

  if (!glsl_GetGlAndGlslVersions(&gl_major,&gl_minor,&glsl_major,&glsl_minor,
                                 &gl_gles3))
    return;

  vertex_shader_source[0] = glsl_GetGLSLVersionString();
  vertex_shader_source[1] = vertex_shader;
  fragment_shader_source[0] = glsl_GetGLSLVersionString();
  fragment_shader_source[1] = fragment_shader;
  if (!glsl_CompileAndLinkShaders(2,vertex_shader_source,
                                  2,fragment_shader_source,
                                  &pf->shader_program))
    return;

  pf->pos_index =
    glGetAttribLocation(pf->shader_program,"VertexPosition");
  pf->normal_index =
    glGetAttribLocation(pf->shader_program,"VertexNormal");
  pf->color_index =
    glGetAttribLocation(pf->shader_program,"VertexColor");
  pf->tex_index =
    glGetAttribLocation(pf->shader_program,"VertexTexCoord");
  pf->mv_index =
    glGetUniformLocation(pf->shader_program,"MatModelView");
  pf->proj_index =
    glGetUniformLocation(pf->shader_program,"MatProj");
  pf->magma_offs_index =
    glGetUniformLocation(pf->shader_program,"TexOffsetMagma");
  pf->north_up_index =
    glGetUniformLocation(pf->shader_program,"NorthUp");
  pf->glbl_ambient_index =
    glGetUniformLocation(pf->shader_program,"LtGlblAmbient");
  pf->lt_ambient_index =
    glGetUniformLocation(pf->shader_program,"LtAmbient");
  pf->lt_diffuse_index =
    glGetUniformLocation(pf->shader_program,"LtDiffuse");
  pf->lt_specular_index =
    glGetUniformLocation(pf->shader_program,"LtSpecular");
  pf->lt_direction_index =
    glGetUniformLocation(pf->shader_program,"LtDirection");
  pf->lt_halfvect_index =
    glGetUniformLocation(pf->shader_program,"LtHalfVector");
  pf->ambient_index =
    glGetUniformLocation(pf->shader_program,"MatAmbient");
  pf->diffuse_index =
    glGetUniformLocation(pf->shader_program,"MatDiffuse");
  pf->specular_index =
    glGetUniformLocation(pf->shader_program,"MatSpecular");
  pf->shininess_index =
    glGetUniformLocation(pf->shader_program,"MatShininess");
  pf->bool_textures_index =
    glGetUniformLocation(pf->shader_program,"BoolTextures");
  pf->samp_mgm_index =
    glGetUniformLocation(pf->shader_program,"TextureSamplerMagma");
  pf->samp_day_index =
    glGetUniformLocation(pf->shader_program,"TextureSamplerDay");
  pf->samp_ngt_index =
    glGetUniformLocation(pf->shader_program,"TextureSamplerNight");
  pf->samp_wtr_index =
    glGetUniformLocation(pf->shader_program,"TextureSamplerWater");
  if (pf->pos_index == -1 ||
      pf->normal_index == -1 ||
      pf->color_index == -1 ||
      pf->tex_index == -1 ||
      pf->mv_index == -1 ||
      pf->proj_index == -1 ||
      pf->magma_offs_index == -1 ||
      pf->north_up_index == -1 ||
      pf->glbl_ambient_index == -1 ||
      pf->lt_ambient_index == -1 ||
      pf->lt_diffuse_index == -1 ||
      pf->lt_specular_index == -1 ||
      pf->lt_direction_index == -1 ||
      pf->lt_halfvect_index == -1 ||
      pf->ambient_index == -1 ||
      pf->diffuse_index == -1 ||
      pf->specular_index == -1 ||
      pf->shininess_index == -1 ||
      pf->bool_textures_index == -1 ||
      pf->samp_mgm_index == -1 ||
      pf->samp_day_index == -1 ||
      pf->samp_ngt_index == -1 ||
      pf->samp_wtr_index == -1)
  {
    glDeleteProgram(pf->shader_program);
    return;
  }

  glGenBuffers(1,&pf->vertex_buffer);
  glGenBuffers(1,&pf->normal_buffer);
  glGenBuffers(1,&pf->color_buffer);
  glGenBuffers(1,&pf->tex_buffer);

  pf->textures_supported = true;
  if (!gl_gles3 && gl_major < 3)
  {
    gl_ext = (const char *)glGetString(GL_EXTENSIONS);
    if (gl_ext == NULL)
      pf->textures_supported = false;
    else
      pf->textures_supported = (strstr(gl_ext,"GL_ARB_texture_rg") != NULL);
  }

  pf->magma_tex_rg = NULL;
  if (pf->textures_supported)
  {
    gl_ext = (char *)glGetString(GL_EXTENSIONS);
    if (gl_ext == NULL)
      gl_ext = "";
#if defined(HAVE_IPHONE)
    /* Don't use anisotropic texture filtering on iOS since it leads to
       artifacts at the ±180° meridian. */
    pf->aniso_textures = false;
#else
    pf->aniso_textures =
      ((strstr(gl_ext,"GL_EXT_texture_filter_anisotropic") != NULL) ||
       (strstr(gl_ext,"GL_ARB_texture_filter_anisotropic") != NULL));
#endif

    if (!gl_gles3 && gl_major < 3)
    {
      /* On OpenGL 2.1 systems, such as the native macOS OpenGL version used
         by XScreenSaver, we need the GL_ARB_framebuffer_object extension
         to be able to use the glGenerateMipmap function. */
      if (strstr(gl_ext,"GL_ARB_framebuffer_object") != NULL)
        pf->use_mipmaps = true;
    }
    else if ((!gl_gles3 && gl_major >= 3) || gl_gles3)
    {
      pf->use_mipmaps = true;
    }

    gen_magma_texture(mi);
    gen_earth_textures(mi);
  }

  pf->use_vao = glsl_IsCoreProfile();
  if (pf->use_vao)
    glGenVertexArrays(1,&pf->vertex_array_object);

  pf->use_shaders = true;
#if 0
  fprintf(stderr,"Use shaders = %d\n",pf->use_shaders);
  fprintf(stderr,"Textures supported = %d\n",pf->textures_supported);
  fprintf(stderr,"Error = %d\n",glGetError());
#endif
}

#endif /* HAVE_GLSL */


/* Initialize the data structures. */
static void init(ModeInfo *mi)
{
  platonicfoldingstruct *pf = &platonicfolding[MI_SCREEN(mi)];
  int i;

#ifdef HAVE_GLSL
  init_glsl(mi);
#endif /* HAVE_GLSL */

  pf->anim_state = ANIM_INIT;
  pf->anim_poly = -1;
  pf->fold_angle = 0;
  pf->north_up = true;

  pf->alpha = 300.0f;
  pf->beta = 0.0f;
  pf->delta = 270.0f;
  pf->delta_delta = 0.0f;

  pf->poly_pos[0] = 0.0f;
  pf->poly_pos[1] = 0.0f;
  pf->poly_pos[2] = 0.0f;

  for (i=0; i<NUM_ANGLES; i++)
    pf->angle[i] = 0.0f;
  pf->angle_dir = 1.0f;

  init_color_matrix(pf->color_matrix);

  pf->polygon_unfolding =
    create_random_polyhedron_unfolding(TETRAHEDRON_NUM_FACES,
                                       TETRAHEDRON_NUM_EDGES,
                                       tetrahedron_edges);
  pf->max_angle = TETRAHEDRON_MAX_ANGLE;
  pf->delta_angle = TETRAHEDRON_DELTA_ANGLE;
  pf->num_fold_angles = TETRAHEDRON_NUM_FACES-1;
  pf->eye_pos = tetrahedron_eye_pos;
  pf->base_polygons = init_base_polygons(pf->polygon_unfolding,
                                         &tetrahedron_triangle);
  determine_unfolding_poses(pf->polygon_unfolding,pf->base_polygons);
  determine_polygon_color_data(pf->polygon_unfolding,pf->base_polygons,
                               pf->max_angle,pf->color_matrix);
}


/*
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 *    Xlock hooks.
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 */


ENTRYPOINT void reshape_platonicfolding(ModeInfo *mi, int width, int height)
{
  platonicfoldingstruct *pf = &platonicfolding[MI_SCREEN(mi)];

  pf->WindW = (GLint)width;
  pf->WindH = (GLint)height;
  pf->aspect = (GLfloat)width/(GLfloat)height;

  /* Set the appear and disappear parameters of the polyhedron. */
  if (pf->eye_pos != NULL)
  {
    if (pf->aspect >= 1.0f)
    {
      pf->spoly_pos[0] = 0.0f;
      pf->spoly_pos[1] = -pf->eye_pos[2]*(2.0f/3.0f);
      pf->spoly_pos[2] = 0.0f;
      pf->dpoly_pos[0] = 0.0f;
      pf->dpoly_pos[1] = pf->eye_pos[2]*(2.0f/3.0f);
      pf->dpoly_pos[2] = 0.0f;
    }
    else
    {
      pf->spoly_pos[0] = -pf->eye_pos[2]*(2.0f/3.0f);
      pf->spoly_pos[1] = 0.0f;
      pf->spoly_pos[2] = 0.0f;
      pf->dpoly_pos[0] = pf->eye_pos[2]*(2.0f/3.0f);
      pf->dpoly_pos[1] = 0.0f;
      pf->dpoly_pos[2] = 0.0f;
    }
  }
}


ENTRYPOINT Bool platonicfolding_handle_event(ModeInfo *mi, XEvent *event)
{
  platonicfoldingstruct *pf = &platonicfolding[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button == Button1)
  {
    pf->button_pressed = True;
    gltrackball_start(pf->trackball,event->xbutton.x,event->xbutton.y,
                      MI_WIDTH(mi),MI_HEIGHT(mi));
    return True;
  }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
  {
    pf->button_pressed = False;
    gltrackball_stop(pf->trackball);
    return True;
  }
  else if (event->xany.type == MotionNotify && pf->button_pressed)
  {
    gltrackball_track(pf->trackball,event->xmotion.x,event->xmotion.y,
                      MI_WIDTH(mi),MI_HEIGHT(mi));
    return True;
  }
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
  {
    pf->anim_state = ANIM_INIT;
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
 *    Initialize platonicfolding.  Called each time the window changes.
 *-----------------------------------------------------------------------------
 */

ENTRYPOINT void init_platonicfolding(ModeInfo *mi)
{
  platonicfoldingstruct *pf;
  int n;

  MI_INIT(mi,platonicfolding);
  pf = &platonicfolding[MI_SCREEN(mi)];

  pf->rotate = rotate;

  /* Set the color mode. */
  if (!strcasecmp(color_mode,"face"))
  {
    pf->colors = COLORS_FACE;
    pf->random_colors = false;
  }
  else if (!strcasecmp(color_mode,"earth"))
  {
    pf->colors = COLORS_EARTH;
    pf->random_colors = false;
  }
  else
  {
    pf->colors = random() % NUM_COLORS;
    pf->random_colors = true;
  }
#ifndef HAVE_GLSL
  if (pf->colors == COLORS_EARTH)
    pf->colors = COLORS_FACE;
#endif

  /* Set the number of foldings. */
  pf->num_foldings = RANDOM_NUM_FOLDINGS;
  if (!strcasecmp(foldings,"random"))
  {
    pf->num_foldings = RANDOM_NUM_FOLDINGS;
  }
  else
  {
    n = sscanf(foldings,"%d",&pf->num_foldings);
    if (n != 1)
      pf->num_foldings = RANDOM_NUM_FOLDINGS;
    else if (pf->num_foldings < 1)
      pf->num_foldings = 1;
    else if (pf->num_foldings > 20)
      pf->num_foldings = 20;
  }

  pf->trackball = gltrackball_init(False);
  pf->button_pressed = False;

  if ((pf->glx_context = init_GL(mi)) != NULL)
  {
    pf->eye_pos = NULL;
    reshape_platonicfolding(mi,MI_WIDTH(mi),MI_HEIGHT(mi));
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
ENTRYPOINT void draw_platonicfolding(ModeInfo *mi)
{
  Display *display = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  platonicfoldingstruct *pf;

  if (platonicfolding == NULL)
    return;
  pf = &platonicfolding[MI_SCREEN(mi)];

  MI_IS_DRAWN(mi) = True;
  if (!pf->glx_context)
    return;

  glXMakeCurrent(display,window,*pf->glx_context);

  display_platonicfolding(mi);

  if (MI_IS_FPS(mi))
    do_fps (mi);

  glFlush();

  glXSwapBuffers(display,window);
}


#ifndef STANDALONE
ENTRYPOINT void change_platonicfolding(ModeInfo *mi)
{
  platonicfoldingstruct *pf = &platonicfolding[MI_SCREEN(mi)];

  if (!pf->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi),MI_WINDOW(mi),*pf->glx_context);
  init(mi);
}
#endif /* !STANDALONE */


/*
 *-----------------------------------------------------------------------------
 *    The display is being taken away from us.  Free up malloc'ed
 *      memory and X resources that we've alloc'ed.
 *-----------------------------------------------------------------------------
 */

ENTRYPOINT void free_platonicfolding(ModeInfo *mi)
{
  platonicfoldingstruct *pf = &platonicfolding[MI_SCREEN(mi)];

  if (!pf->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi),MI_WINDOW(mi),*pf->glx_context);

  gltrackball_free(pf->trackball);

  free_base_polygons(pf->base_polygons);

#ifdef HAVE_GLSL
  if (pf->use_shaders)
  {
    glUseProgram(0);
    if (pf->shader_program != 0)
      glDeleteProgram(pf->shader_program);
    glDeleteBuffers(1,&pf->vertex_buffer);
    glDeleteBuffers(1,&pf->normal_buffer);
    glDeleteBuffers(1,&pf->color_buffer);
    glDeleteBuffers(1,&pf->tex_buffer);
    if (pf->use_vao)
      glDeleteVertexArrays(1,&pf->vertex_array_object);
    if (pf->textures_supported)
    {
      glDeleteTextures(1,&pf->magma_tex);
      free(pf->magma_tex_rg);
      glDeleteTextures(3,pf->earth_tex);
    }
  }
#endif /* HAVE_GLSL */
}


XSCREENSAVER_MODULE ("PlatonicFolding", platonicfolding)

#endif /* USE_GL */
