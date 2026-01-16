/* hopffibration --- Displays the Hopf fibration of the 4D hypersphere S³. */

#if 0
static const char sccsid[] = "@(#)hopffibration.c  1.1 25/02/01 xlockmore";
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
 * C. Steger - 25/02/06: Initial version
 * C. Steger - 26/01/02: Make the code work in an OpenGL core profile
 */

/*
 * This program shows the Hopf fibration of the 4d hypersphere S³.
 * The Hopf fibration is based on the Hopf map, a many-to-one
 * continuous function from S³ onto the the ordinary 3d sphere S² such
 * that each distinct point of S² is mapped from a distinct great
 * circle S¹ of S³.  Hence, the inverse image of a point on S²
 * corresponds to a great circle S¹ on S³. The sphere S² is called the
 * base space, each S¹ corresponding to a point on S² is called a
 * fiber, and S³ is called the total space.
 *
 * The program displays the base space S² as a semi-transparent gray
 * sphere in the bottom right corner of the display.  The points on
 * the base space are displayed as small colored spheres.  The fibers
 * in the total space are displayed in the same color as the
 * corresponding point on the base space.
 *
 * The fibers in the total space are projected from 4d to 3d using
 * stereographic projection and then compressing the infinite 3d space
 * to a finite 3d ball to display the fibers compactly.  All fibers
 * except one fiber that passes through the north pole of S³ are thus
 * projected to deformed circles in 3d.  The program displays these
 * deformed circles as closed tubes (topological tori).  The single
 * fiber that passes through the north pole of S³ is projected to an
 * infinite line by the stereographic projection.  This line passes
 * through infinity in 3d and therefore topologically is a circle.
 * Compressing this infinite line to a finite ball maps it to a
 * straight line segment.  The program displays this line segment as a
 * cylinder.  However, it should be thought of as a circle through
 * infinity.
 *
 * The fibers, base space, and base points are then projected to the
 * screen either perspectively or orthographically.
 *
 * The program displays various interesting configurations of base
 * points and fibers.  Look out for the following configurations:
 *
 *   • Any two fibers form a Hopf link.
 *   • More generally, each fiber is linked with each other fiber
 *     exactly once.
 *   • Each circle on S² creates a set of fibers that forms a Clifford
 *     torus on S³ (i.e., in 4d).  Clifford tori are flat (in the same
 *     sense that the surface of a cylinder is flat).
 *   • If a circle on S² is not a circle of latitude, the projection
 *     of the Clifford torus to 3d results in a (compressed) Dupin
 *     cyclide.
 *   • More generally, any closed curve on S² creates a torus-like
 *     surface on S³ that is flat.  These surfaces are called Hopf
 *     tori or Bianchi-Pinkall flat tori.  Look for the wave-like
 *     curve on S² to see a Hopf torus.
 *   • A circular arc on S² creates a Hopf band on S³.  The Hopf
 *     band is a Seifert surface of the Hopf link that forms the
 *     boundaries of the Hopf band.
 *   • Two or more circles of latitude on S² create two or more nested
 *     Clifford tori on S³.
 *   • More generally, two or more disjoint circles on S² create two
 *     or more linked Clifford tori on S³.
 *   • A great circle through the north pole of S² creates a parabolic
 *     ring cyclide (which is compressed to lie within the ball in the
 *     3d projection).  A parabolic ring cyclide divides the entire 3d
 *     space into two congruent parts that are interlocked, i.e.,
 *     linked.
 *   • By turning a circle on S² so that it passes through the north
 *     pole of S², the projection of the corresponding Clifford torus
 *     reverses its inside and outside in 3d.
 *   • The Clifford torus corresponding to a great circle on S²
 *     divides S³ into two congruent solid tori that fill the entire
 *     S³.  The two solid tori on S³ correspond to the two hemispheres
 *     into which the great circle divides S².  The solid tori in S³
 *     are attached to each other at the Clifford torus.  The
 *     congruence of the solid tori is visible in a particularly
 *     striking manner if the great circle that creates the Clifford
 *     torus is rotated so that it passes through the north pole of
 *     S², thereby creating a parabolic ring cyclide via the
 *     projection of the Clifford torus to 3d (see above).
 *
 * During the animations, two kinds of motions are used.  Usually, the
 * points on the base space are moved or rotated to particular
 * configurations.  This is apparent by the small spheres that
 * represent the base points changing their position on the base
 * space, which leads to a corresponding change of the configuration
 * of the fibers.  The base space itself, however, is not moved or
 * rotated, i.e., its orientation remains fixed.  Sometimes, only the
 * projection of the fibers is rotated in 3d to show some interesting
 * configurations more clearly, e.g., that a Hopf torus has a hole
 * like a regular torus.  In this case, the base space also maintains
 * its orientation in space.  Since a rotation in 3d does not change
 * the configuration of the fibers, in this kind of animation, the
 * points on the base space also remain fixed.  Sometimes, both types
 * of animations are combined, e.g., when the projection of one or
 * more Clifford tori is rotated in 3d while the base points of the
 * Clifford tori also rotate on the base space.  In this case, the
 * base space will only show the movement of the base points on the
 * base space and not the 3d rotation of the projection of the fibers.
 *
 * To enhance the 3d depth impression, the program displays the
 * shadows of the fibers and base points by default.  This is done by
 * way of a two-pass rendering algorithm in which the geometry is
 * rendered twice.  Depending on the speed of the GPU, displaying
 * shadows might slow down the rendering significantly.  If this is
 * the case, the rendering of shadows can be switched off, saving one
 * render pass and thus speeding up the rendering.
 *
 * Some of the animations render complex geometries with a very large
 * number of polygons. This can cause the rendering to become slow on
 * some types of GPU.  To speed up the rendering process, the amount
 * of details that are rendered can be controlled in three
 * granularities (coarse, medium, and fine).  Devices with relatively
 * small screens and relatively low-powered GPUs, such as phones or
 * tablets, should typically select coarse details.  Standard GPUs
 * should select medium details (the default).  High-powered GPUs on
 * large screens may benefit from fine details.
 *
 * By default, the base space and base points are displayed as
 * described above.  If desired, the display of the base space and
 * base points can be switched off so that only the fibers are
 * displayed.
 *
 * During the animation of the Hopf fibration, sometimes multiple
 * fibers that are very close to each other are displayed.  This can
 * create disturbing aliasing artifacts that are especially noticeable
 * when the fibers are moving or turning slowly.  Therefore, by
 * default, the rendering is performed using anti-aliasing.  This
 * typically has a negligible effect on the rendering speed.  However,
 * if shadows have already been switched off, coarse details have been
 * selected, and the rendering is still slow, anti-aliasing also can
 * be switched off to check whether it has a noticeable effect on the
 * rendering speed.
 *
 * This program was inspired by Niles Johnson's visualization of the
 * Hopf fibration (https://nilesjohnson.net/hopf.html).
 */


#define DISP_PERSPECTIVE  0
#define DISP_ORTHOGRAPHIC 1

#define DISP_DETAILS_COARSE 0
#define DISP_DETAILS_MEDIUM 1
#define DISP_DETAILS_FINE   2

#define DEF_SHADOWS       "True"
#define DEF_BASE_SPACE    "True"
#define DEF_ANTI_ALIASING "True"
#define DEF_DETAILS       "medium"
#define DEF_PROJECTION    "perspective"

#ifdef STANDALONE
# define DEFAULTS           "*delay:       20000 \n" \
                            "*showFPS:     False \n" \
                            "*prefersGLSL: True \n" \

# define release_hopffibration 0
# include "xlockmore.h"         /* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"             /* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL

#include "glsl-utils.h"
#include "gltrackball.h"
#include "hopfanimations.h"


#if (defined(HAVE_COCOA) || defined(__APPLE__)) && !defined(HAVE_IPHONE)
#ifndef GL_COMPARE_REF_TO_TEXTURE
#define GL_COMPARE_REF_TO_TEXTURE GL_COMPARE_R_TO_TEXTURE_ARB
#endif
#ifndef GL_TEXTURE_COMPARE_MODE
#define GL_TEXTURE_COMPARE_MODE GL_TEXTURE_COMPARE_MODE_ARB
#endif
#ifndef GL_TEXTURE_COMPARE_FUNC
#define GL_TEXTURE_COMPARE_FUNC GL_TEXTURE_COMPARE_FUNC_ARB
#endif
#endif


/* Define ROT_BASE_SPACE if the base space and the base points should be
   rotated in 3d in the same manner as the fibers in the total space.  This
   is currently not defined since it makes the rotations of the base points
   in 3d harder to distinguish from the animations that change the base
   points on the base space and therefore change the fibers. */
#undef ROT_BASE_SPACE

#define LINCOOR(r,c,w) ((r)*(w)+(c))

#define M_SQRT2_F 1.4142135623731f
#define M_COS_1_F 0.9998476951564f

#define MAX_CIRCLE_PNT     512
#define MAX_CIRCLE_STACK   64
#define TUBE_RADIUS        0.01f
#define BASE_POINT_RADIUS  0.01f
#define BASE_SPHERE_RADIUS 0.2f
#define BASE_SPHERE_S      14

#define NUM_TUBE_COARSE 8
#define NUM_TUBE_MEDIUM 12
#define NUM_TUBE_FINE   16

#define BASE_SPHERE_S_COARSE 12
#define BASE_SPHERE_S_MEDIUM 14
#define BASE_SPHERE_S_FINE   20

#define BASE_POINT_S_COARSE 2
#define BASE_POINT_S_MEDIUM 2
#define BASE_POINT_S_FINE   3

#define MAX_CIRCLE_DIST_COARSE 0.0010f
#define MAX_CIRCLE_DIST_MEDIUM 0.0005f
#define MAX_CIRCLE_DIST_FINE   0.0003f

#define MSAA_SAMPLES 4


/* The number of vertices and triangles of an icosahedron. */
#define NUM_ICOSA_VERT 12
#define NUM_ICOSA_TRI  20

/* The nontrivial coordinates of an icosahedron. */
#define ICO_X1 0.8944271910f
#define ICO_X2 0.2763932023f
#define ICO_X3 0.7236067977f
#define ICO_Y1 0.8506508084f
#define ICO_Y2 0.5257311121f
#define ICO_Z  0.4472135955f

/* The vertices of an icosahedron. */
static float icosa_vert[3*NUM_ICOSA_VERT] = {
   0.0f,    0.0f,    1.0f,
   ICO_X1,  0.0f,    ICO_Z,
   ICO_X2,  ICO_Y1,  ICO_Z,
  -ICO_X3,  ICO_Y2,  ICO_Z,
  -ICO_X3, -ICO_Y2,  ICO_Z,
   ICO_X2, -ICO_Y1,  ICO_Z,
   ICO_X3,  ICO_Y2, -ICO_Z,
  -ICO_X2,  ICO_Y1, -ICO_Z,
  -ICO_X1,  0.0f,   -ICO_Z,
  -ICO_X2, -ICO_Y1, -ICO_Z,
   ICO_X3, -ICO_Y2, -ICO_Z,
   0.0f,    0.0f,   -1.0f
};

/* The triangles of an icosahedron. */
static int icosa_tri[3*NUM_ICOSA_TRI] = {
   0,  1,  2,
   0,  2,  3,
   0,  3,  4,
   0,  4,  5,
   0,  5,  1,
   1,  6,  2,
   2,  7,  3,
   3,  8,  4,
   4,  9,  5,
   5, 10,  1,
   2,  6,  7,
   3,  7,  8,
   4,  8,  9,
   5,  9, 10,
   1, 10,  6,
   6, 11,  7,
   7, 11,  8,
   8, 11,  9,
   9, 11, 10,
  10, 11,  6
};

/* A data structure that stores the data of an icosahedron. vert stores the
   num_vert vertices, while tri stores the num_tri triangles of the
   icosahedron. */
typedef struct {
  float *vert;
  int num_vert;
  int *tri;
  int num_tri;
} icosahedron_data;

/* A data structure that stores the data of an icosphere.  vert stores the
   num_vert vertices, while norm stores the corresponding normals at the
   vertices.  tri stores the num_tri triangles.  stri stores the num_tri
   triangles that have been sorted according to the depth (z value) of
   their centroid after the vertices have been transformed by the rotation
   matrix stored in smat. */
typedef struct {
  float *vert;
  float *norm;
  int num_vert;
  int *tri;
  int *stri;
  int num_tri;
  float smat[3][3];
} icosphere_data;

/* An instance of an icosahedron. */
static icosahedron_data icosa = {
  icosa_vert, NUM_ICOSA_VERT, icosa_tri, NUM_ICOSA_TRI
};

/* A data structure to sort triangles by the z value of their centroid. */
typedef struct {
  float z;
  int idx;
} trisort;


/* A data structure that contains a base point of the Hopf fibration.  This
   is a point on the unit sphere S² in 3D. Therefore, the Euclidean length
   of the vector (a,b,c) is assumed to be 1. */
typedef struct {
  float a, b, c;
} hopf_base_point;

/* A data structure that holds the data that is necessary to construct a
   Hopf circle fiber that has been projected from 4D to 3D.  See the
   function init_hopf_circle. */
typedef struct {
  hopf_base_point base;
  float al, be;
  float atab;
} hopf_circle_par;

/* A data structure that holds the data for one Hopf circle fiber point in
   3D.  p is the point, while t, n, and b define the Frenet-Serret frame of
   the Hopf circle at p, i.e, t is the unit tangent vector, n the unit
   normal vector, and b the unit binormal vector at p.  phi is the angle
   parameter of p on the circle.  See the functions gen_hopf_circle_points,
   hopf_circle_point, hopf_circle_tangent, and hopf_circle_binormal. */
typedef struct {
  float p[3];
  float t[3];
  float n[3];
  float b[3];
  float phi;
} hopf_circle_pnt;

/* A data structure that defines a segment from the starting point s to the
   end point e on a Hopf circle.  This data structure is used to construct
   the Hopf circle projected to 3D recursively in gen_hopf_circle_points. */
typedef struct {
  hopf_circle_pnt s, e;
} hopf_circle_seg;


#ifdef USE_MODULES
ModStruct hopffibration_description =
{"hopffibration", "init_hopffibration", "draw_hopffibration",
 NULL, "draw_hopffibration", "change_hopffibration",
 "free_hopffibration", hopffibration_opts, 20000, 1, 1, 1, 1.0, 4, "",
 "Display a Hopf fibration",
 0, NULL};
#endif


static Bool shadows;
static Bool base_space;
static Bool anti_aliasing;
static char *det;
static char *proj;


static XrmOptionDescRec opts[] =
{
  {"-shadows",       ".shadows",      XrmoptionNoArg,  "on"},
  {"+shadows",       ".shadows",      XrmoptionNoArg,  "off"},
  {"-base-space",    ".baseSpace",    XrmoptionNoArg,  "on"},
  {"+base-space",    ".baseSpace",    XrmoptionNoArg,  "off"},
  {"-anti-aliasing", ".antiAliasing", XrmoptionNoArg,  "on"},
  {"+anti-aliasing", ".antiAliasing", XrmoptionNoArg,  "off"},
  {"-details",       ".details",      XrmoptionSepArg, 0 },
  {"-perspective",   ".projection",   XrmoptionNoArg,  "perspective" },
  {"-orthographic",  ".projection",   XrmoptionNoArg,  "orthographic" },
};

static argtype vars[] =
{
  { &shadows,       "shadows",      "Shadows",      DEF_SHADOWS,       t_Bool },
  { &base_space,    "baseSpace",    "BaseSpace",    DEF_BASE_SPACE,    t_Bool },
  { &anti_aliasing, "antiAliasing", "AntiAliasing", DEF_ANTI_ALIASING, t_Bool },
  { &det,           "details",      "Details",      DEF_DETAILS,       t_String },
  { &proj,          "projection",   "Projection",   DEF_PROJECTION,    t_String }
};

ENTRYPOINT ModeSpecOpt hopffibration_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, NULL};


typedef struct {
  GLint WindH, WindW;
  GLXContext *glx_context;
  /* Options */
  Bool shadows;
  Bool base_space;
  Bool anti_aliasing;
  int projection;
  int details;
  int num_tube;
  int base_sphere_s;
  int base_point_s;
  float max_circle_dist;
  /* Aspect ratio of the current window */
  float aspect;
  /* Trackball states */
  trackball_state *trackball;
  Bool button_pressed;
  /* 3D rotation angles */
  float alpha, beta, delta;
  /* Rotation angles for the base space. */
  float zeta, eta, theta;
  /* A rotation axis around which the base points are rotated. */
  float rot_axis_base[3];
  /* The quaternion corresponding to a rotation by the current angle around
     rot_axis_base. */
  float quat_base[4];
  /* A rotation axis around which the Hopf fibers in the projection of the
     total space are rotated. */
  float rot_axis_space[3];
  /* The quaternion corresponding to a rotation by the current angle around
     rot_axis_space. */
  float quat_space[4];
  /* The angle range through which the Hopf fibers in the projection of the
     total space are rotated. */
  float angle_space_start, angle_space_end;
  /* The current state of the animation. */
  int anim_state;
  /* A variable that counts how many cycles to remain in the current state. */
  int anim_remain_in_state;
  /* The phase of the current animation. */
  int anim_phase;
  /* The number of phases in the current animation. */
  int anim_phase_num;
  /* The number of steps in the current animation. */
  int anim_step;
  /* The easing function for a rotation around a rot_axis_base. */
  int anim_easing_fct_rot_rnd;
  /* The easing function for a rotation around rot_axis_space. */
  int anim_easing_fct_rot_space;
  /* If true, perform a rotation around rot_axis_base. */
  Bool anim_rotate_rnd;
  /* If true, perform a rotation around rot_axis_space. */
  Bool anim_rotate_space;
  /* The phases of the current animation. */
  animation_phases *anim_phases;
  /* The objects that are animated in the current phase of the current
     animation. */
  animation_multi_obj *anim;
  /* The state variables current phase of the current animation. */
  animation_geometry anim_geom[MAX_ANIM_GEOM];
  /* The set of base points for which Hopf fibers should be computed and
     displayed. */
  hopf_base_point *base_points;
  /* The maximum number of base points over all animations in
     hopfanimations.c. */
  int max_base_points;
  /* The number of elements in base_points that have been set. */
  int num_base_points;
  /* An icosphere that is used to display the base sphere S². */
  icosphere_data *sphere_base;
  /* An icosphere that is used to display the base points in base_points on
     the base sphere S². */
  icosphere_data *sphere_base_point;
  /* The number of polygons that were drawn. */
  int num_poly;
#ifdef HAVE_GLSL
  float *vert;
  float *norm;
  int *tri;
  Bool use_shaders, use_msaa_fbo, use_shadow_fbo;
  Bool shadow_fbo_req_col_tex, use_vao;
  GLuint shader_program, shadow_program;
  GLint pos_index, normal_index, color_index;
  GLint mv_index, proj_index, lmvp_index;
  GLint glbl_ambient_index, lt_ambient_index;
  GLint lt_diffuse_index, lt_specular_index;
  GLint lt_direction_index, lt_halfvect_index;
  GLint ambient_index, diffuse_index;
  GLint specular_index, shininess_index;
  GLint use_shadows_index, shadow_smpl_index;
  GLint shadow_pix_size_index;
  GLint shadow_pos_index, shadow_lmvp_index;
  GLuint *fiber_pos_buffer, *fiber_normal_buffer;
  GLuint *fiber_indices_buffer;
  GLuint *fiber_num_tri;
  GLuint sphere_base_pos_buffer, sphere_base_normal_buffer;
  GLuint sphere_base_indices_buffer;
  GLuint sphere_base_num_tri;
  GLuint base_point_pos_buffer, base_point_normal_buffer;
  GLuint base_point_indices_buffer;
  GLuint base_point_num_tri;
  GLuint default_draw_fbo, default_read_fbo;
  GLuint msaa_fbo, msaa_rb_color, msaa_rb_depth;
  GLuint vertex_array_object;
  GLint msaa_samples;
  GLuint shadow_fbo;
  GLuint shadow_tex, shadow_col_tex, shadow_dummy_tex;
  GLint shadow_tex_size, max_tex_size;
#endif /* HAVE_GLSL */
} hopffibrationstruct;

static hopffibrationstruct *hopffibration = (hopffibrationstruct *) NULL;


/* The camera position. */
static float eye_pos[3] = {
  0.0f, 0.0f, 3.0f
};

/* The position of the base sphere S² with respect to the projection of the
   fibers in the total space S³, which all lie in a ball in 3-space
   centrered at the origin. */
static float base_offset[3] = {
  0.9f, -0.9f, 0.25f
};



#ifdef HAVE_GLSL

/* The vertex shader code. */
static const GLchar *vertex_shader = "\
#ifdef GL_ES                                                             \n\
precision highp float;                                                   \n\
precision highp int;                                                     \n\
precision highp sampler2DShadow;                                         \n\
#endif                                                                   \n\
                                                                         \n\
#if __VERSION__ <= 120                                                   \n\
attribute vec3 VertexPosition;                                           \n\
attribute vec3 VertexNormal;                                             \n\
attribute vec4 VertexColor;                                              \n\
varying vec3 Normal;                                                     \n\
varying vec4 Color;                                                      \n\
varying vec4 ShadowCoord;                                                \n\
#else                                                                    \n\
in vec3 VertexPosition;                                                  \n\
in vec3 VertexNormal;                                                    \n\
in vec4 VertexColor;                                                     \n\
out vec3 Normal;                                                         \n\
out vec4 Color;                                                          \n\
out vec4 ShadowCoord;                                                    \n\
#endif                                                                   \n\
                                                                         \n\
uniform mat4 MatModelView;                                               \n\
uniform mat4 MatProj;                                                    \n\
uniform mat4 MatLightMVP;                                                \n\
                                                                         \n\
void main(void)                                                          \n\
{                                                                        \n\
  Color = VertexColor;                                                   \n\
  Normal = normalize(MatModelView*vec4(VertexNormal,0.0f)).xyz;          \n\
  vec4 Position = MatModelView*vec4(VertexPosition,1.0f);                \n\
  gl_Position = MatProj*Position;                                        \n\
  ShadowCoord = 0.5f*MatLightMVP*vec4(VertexPosition,1.0f)+0.5f;         \n\
}                                                                        \n";

/* The fragment shader code. */
static const GLchar *fragment_shader = "\
#ifdef GL_ES                                                             \n\
precision highp float;                                                   \n\
precision highp int;                                                     \n\
precision highp sampler2DShadow;                                         \n\
#endif                                                                   \n\
                                                                         \n\
#if __VERSION__ <= 120                                                   \n\
varying vec3 Normal;                                                     \n\
varying vec4 Color;                                                      \n\
varying vec4 ShadowCoord;                                                \n\
#else                                                                    \n\
in vec3 Normal;                                                          \n\
in vec4 Color;                                                           \n\
in vec4 ShadowCoord;                                                     \n\
out vec4 FragColor;                                                      \n\
#endif                                                                   \n\
                                                                         \n\
uniform vec4 LtGlblAmbient;                                              \n\
uniform vec4 LtAmbient, LtDiffuse, LtSpecular;                           \n\
uniform vec3 LtDirection, LtHalfVector;                                  \n\
uniform vec4 MatAmbient;                                                 \n\
uniform vec4 MatDiffuse;                                                 \n\
uniform vec4 MatSpecular;                                                \n\
uniform float MatShininess;                                              \n\
uniform bool UseShadows;                                                 \n\
uniform float ShadowPixSize;                                             \n\
uniform sampler2DShadow ShadowTex;                                       \n\
                                                                         \n\
float SampleShadowPCF(int x, int y)                                      \n\
{                                                                        \n\
  vec4 offset;                                                           \n\
                                                                         \n\
#if __VERSION__ <= 120                                                   \n\
  offset = vec4(x*ShadowPixSize,y*ShadowPixSize,0.0f,0.0f);              \n\
  return shadow2DProj(ShadowTex,ShadowCoord+offset).x;                   \n\
#else                                                                    \n\
  offset = vec4(float(x)*ShadowPixSize,float(y)*ShadowPixSize,           \n\
                0.0f,0.0f);                                              \n\
  return textureProj(ShadowTex,ShadowCoord+offset);                      \n\
#endif                                                                   \n\
}                                                                        \n\
                                                                         \n\
void main(void)                                                          \n\
{                                                                        \n\
  vec3 normalDirection;                                                  \n\
  vec4 ambientColor, diffuseColor, sceneColor;                           \n\
  vec4 ambientLighting, diffuseReflection, specularReflection, color;    \n\
  float ndotl, ndoth, pf, shadowPCF;                                     \n\
  int i, j;                                                              \n\
                                                                         \n\
  if (gl_FrontFacing)                                                    \n\
    normalDirection = normalize(Normal);                                 \n\
  else                                                                   \n\
    normalDirection = -normalize(Normal);                                \n\
  sceneColor = Color*MatAmbient*LtGlblAmbient;                           \n\
  ambientColor = Color*MatAmbient;                                       \n\
  diffuseColor = Color*MatDiffuse;                                       \n\
                                                                         \n\
  if (UseShadows)                                                        \n\
  {                                                                      \n\
    shadowPCF = 0.0f;                                                    \n\
    for (i=-1; i<=1; i++)                                                \n\
      for (j=-1; j<=1; j++)                                              \n\
        shadowPCF += SampleShadowPCF(i,j);                               \n\
    shadowPCF *= 1.0f/9.0f;                                              \n\
  }                                                                      \n\
  else                                                                   \n\
  {                                                                      \n\
    shadowPCF = 1.0f;                                                    \n\
  }                                                                      \n\
                                                                         \n\
  ndotl = max(0.0f,dot(normalDirection,LtDirection));                    \n\
  ndoth = max(0.0f,dot(normalDirection,LtHalfVector));                   \n\
  if (ndotl == 0.0f)                                                     \n\
    pf = 0.0f;                                                           \n\
  else                                                                   \n\
    pf = pow(ndoth,MatShininess);                                        \n\
  ambientLighting = ambientColor*LtAmbient;                              \n\
  diffuseReflection = LtDiffuse*diffuseColor*ndotl;                      \n\
  specularReflection = LtSpecular*MatSpecular*pf;                        \n\
  color = sceneColor+ambientLighting;                                    \n\
  color += shadowPCF*(diffuseReflection+specularReflection);             \n\
  color.a = diffuseColor.a;                                              \n\
#if __VERSION__ <= 120                                                   \n\
  gl_FragColor = clamp(color,0.0f,1.0f);                                 \n\
#else                                                                    \n\
  FragColor = clamp(color,0.0f,1.0f);                                    \n\
#endif                                                                   \n\
}                                                                        \n";


/* The shadow vertex shader code. */
static const GLchar *shadow_vertex_shader = "\
#ifdef GL_ES                                                             \n\
precision highp float;                                                   \n\
precision highp int;                                                     \n\
precision highp sampler2DShadow;                                         \n\
#endif                                                                   \n\
                                                                         \n\
#if __VERSION__ <= 120                                                   \n\
attribute vec4 VertexPosition;                                           \n\
#else                                                                    \n\
in vec4 VertexPosition;                                                  \n\
#endif                                                                   \n\
                                                                         \n\
uniform mat4 MatLightMVP;                                                \n\
                                                                         \n\
void main(void)                                                          \n\
{                                                                        \n\
  gl_Position = MatLightMVP*VertexPosition;                              \n\
}                                                                        \n";

/* The shadow fragment shader code. */
static const GLchar *shadow_fragment_shader = "\
#ifdef GL_ES                                                             \n\
precision highp float;                                                   \n\
precision highp int;                                                     \n\
precision highp sampler2DShadow;                                         \n\
#endif                                                                   \n\
                                                                         \n\
void main(void)                                                          \n\
{                                                                        \n\
  // Nothing to do since depth is handled automatically by OpenGL.       \n\
}                                                                        \n";

#endif /* HAVE_GLSL */


/* Compute the Euclidean norm of a vector. */
static inline float norm(const float v[3])
{
  return sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
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


/* Normalize a vector to length r. */
static inline void normalize_to_length(float v[3], float r)
{
  float l;

  l = norm(v);
  if (l > 0.0f)
    l = r/l;
  v[0] *= l;
  v[1] *= l;
  v[2] *= l;
}


/* Compute the vector difference s = a - b. */
static inline void sub(float s[3], const float a[3], const float b[3])
{
  s[0] = a[0]-b[0];
  s[1] = a[1]-b[1];
  s[2] = a[2]-b[2];
}


/* Compute the cross product c = m × n. */
static inline void cross(float c[3], const float m[3], const float n[3])
{
  c[0] = m[1]*n[2]-m[2]*n[1];
  c[1] = m[2]*n[0]-m[0]*n[2];
  c[2] = m[0]*n[1]-m[1]*n[0];
}


/* Compute the distance of the point p from the line given by the two points
   q and r. It is assumed that q and r are distinct points. */
static float distance_point_line(const float p[3], const float q[3],
                                 const float r[3])
{
  float v[3], w[3], c[3], lv, lc;

  sub(v,r,q);
  lv = norm(v);
  sub(w,p,q);
  cross(c,w,v);
  lc = norm(c);
  if (lv > 0.0f)
    return lc/lv;
  else
    return 0.0f;
}


/* Compute the identity matrix. */
static void identity(float m[3][3])
{
  int i, j;

  for (i=0; i<3; i++)
    for (j=0; j<3; j++)
      m[i][j] = (float)(i==j);
}


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


/* Multiply two rotation matrices: o = m * n. */
static void mult_rotmat(float o[3][3], float m[3][3], float n[3][3])
{
  int i, j, k;

  for (i=0; i<3; i++)
  {
    for (j=0; j<3; j++)
    {
      o[i][j] = 0.0f;
      for (k=0; k<3; k++)
        o[i][j] += m[i][k]*n[k][j];
    }
  }
}


/* Multiply a rotation matrix with a vector: o = m * v. */
static void mult_rotmat_vec(float o[3], float m[3][3], float v[3])
{
  int i, j;

  for (i=0; i<3; i++)
  {
    o[i] = 0.0f;
    for (j=0; j<3; j++)
      o[i] += m[i][j]*v[j];
  }
}


/* Create a look-at rotation matrix. */
static void look_at_rotmat(float c[3][3],
                           GLfloat eyex, GLfloat eyey, GLfloat eyez,
                           GLfloat centerx, GLfloat centery, GLfloat centerz,
                           GLfloat upx, GLfloat upy, GLfloat upz)
{
  float forward[3], side[3], up[3];

  forward[0] = centerx-eyex;
  forward[1] = centery-eyey;
  forward[2] = centerz-eyez;
  normalize(forward);

  up[0] = upx;
  up[1] = upy;
  up[2] = upz;

  /* side = forward × up */
  cross(side,forward,up);
  normalize(side);

  /* up = side × forward */
  cross(up,side,forward);

  c[0][0] = side[0];
  c[0][1] = side[1];
  c[0][2] = side[2];
  c[1][0] = up[0];
  c[1][1] = up[1];
  c[1][2] = up[2];
  c[2][0] = -forward[0];
  c[2][1] = -forward[1];
  c[2][2] = -forward[2];
}


/* Create a rotation matrix from a quaternion. */
static void quat_to_rotmat(float q[4], float m[3][3])
{
  m[0][0] = q[0]*q[0]+q[1]*q[1]-q[2]*q[2]-q[3]*q[3];
  m[0][1] = 2.0f*(q[1]*q[2]-q[0]*q[3]);
  m[0][2] = 2.0f*(q[1]*q[3]+q[0]*q[2]);
  m[1][0] = 2.0f*(q[1]*q[2]+q[0]*q[3]);
  m[1][1] = q[0]*q[0]-q[1]*q[1]+q[2]*q[2]-q[3]*q[3];
  m[1][2] = 2.0f*(q[2]*q[3]-q[0]*q[1]);
  m[2][0] = 2.0f*(q[1]*q[3]-q[0]*q[2]);
  m[2][1] = 2.0f*(q[2]*q[3]+q[0]*q[1]);
  m[2][2] = q[0]*q[0]-q[1]*q[1]-q[2]*q[2]+q[3]*q[3];
}


/* Create a 4×4 column-major rotation matrix from a quaternion. */
static void quat_to_rotmat_4x4(GLfloat q[4], GLfloat c[16])
{
  c[0] = q[0]*q[0]+q[1]*q[1]-q[2]*q[2]-q[3]*q[3];
  c[1] = 2.0f*(q[1]*q[2]+q[0]*q[3]);
  c[2] = 2.0f*(q[1]*q[3]-q[0]*q[2]);
  c[3] = 0.0f;
  c[4] = 2.0f*(q[1]*q[2]-q[0]*q[3]);
  c[5] = q[0]*q[0]-q[1]*q[1]+q[2]*q[2]-q[3]*q[3];
  c[6] = 2.0f*(q[2]*q[3]+q[0]*q[1]);
  c[7] = 0.0f;
  c[8] = 2.0f*(q[1]*q[3]+q[0]*q[2]);
  c[9] = 2.0f*(q[2]*q[3]-q[0]*q[1]);
  c[10] = q[0]*q[0]-q[1]*q[1]-q[2]*q[2]+q[3]*q[3];
  c[11] = 0.0f;
  c[12] = 0.0f;
  c[13] = 0.0f;
  c[14] = 0.0f;
  c[15] = 1.0f;
}


/* Compute a 3×3 rotation matrix from an xscreensaver unit quaternion. Note
   that xscreensaver has a different convention for unit quaternions than
   the one that is used in this hack. */
static void quat_to_rotmat_trackball(float p[4], float m[3][3])
{
  float al, be, de;
  float r00, r01, r02, r12, r22;

  r00 = 1.0f-2.0f*(p[1]*p[1]+p[2]*p[2]);
  r01 = 2.0f*(p[0]*p[1]+p[2]*p[3]);
  r02 = 2.0f*(p[2]*p[0]-p[1]*p[3]);
  r12 = 2.0f*(p[1]*p[2]+p[0]*p[3]);
  r22 = 1.0f-2.0f*(p[1]*p[1]+p[0]*p[0]);

  al = atan2f(-r12,r22)*180.0f/M_PI_F;
  be = atan2f(r02,sqrtf(r00*r00+r01*r01))*180.0f/M_PI_F;
  de = atan2f(-r01,r00)*180.0f/M_PI_F;
  rotateall(al,be,de,m);
}


/* Compute a 4×4 rotation matrix from an xscreensaver unit quaternion. Note
   that xscreensaver has a different convention for unit quaternions than
   the one that is used in this hack. */
static void quat_to_rotmat_trackball_4x4(float p[4], float r[16])
{
  float m[3][3];
  int   i, j;

  quat_to_rotmat_trackball(p,m);
  for (i=0; i<3; i++)
  {
    for (j=0; j<3; j++)
      r[j*4+i] = m[i][j];
    r[3*4+i] = 0.0f;
    r[i*4+3] = 0.0f;
  }
  r[3*4+3] = 1.0f;
}


/* Generate the vertices and triangles of the icosphere icosp with radius r
   by subdividing the triangles of the icosahedron icosa into s segments
   along each edge, resulting in s*s triangles for each triangle of the
   icosahedron. */
static void gen_icosphere(const icosahedron_data *icosa, icosphere_data *icosp,
                          int s, float r)
{
  int i, j, k, m, n, o, num_edge;
  int i1, i2, i3, l1, l2, l3, k1, k2;
  int vf, vt;
  float t;
  int num_vert, num_tri;
  const float *v;
  const int *tri;
  unsigned char *adj;
  int *edge_index_arr;
  int **edge_indices;
  float *vs;
  int *vi, *ts;

  /* Sanity check. */
  if (s < 1)
    s = 1;

  v = icosa->vert;
  num_vert = icosa->num_vert;
  tri = icosa->tri;
  num_tri = icosa->num_tri;
  vs = icosp->vert;
  ts = icosp->tri;

  /* Allocate and clear the adjacency matrix. */
  adj = calloc(num_vert*num_vert,sizeof(*adj));
  /* Allocate the array that stores the vertex indices of a subdivided
     triangle. */
  vi = malloc((s+1)*(s+2)*sizeof(*vi));
  if (adj == NULL || vi == NULL)
    abort();

  /* Initialize the adjacency matrix. */
  for (i=0; i<num_tri; i++)
  {
    for (j=0; j<3; j++)
    {
      vf = tri[3*i+j];
      vt = tri[3*i+(j+1)%3];
      adj[LINCOOR(vf,vt,num_vert)] = 1;
      adj[LINCOOR(vt,vf,num_vert)] = 1;
    }
  }

  /* Count the number of edges of the polyhedron. */
  num_edge = 0;
  for (i=0; i<num_vert; i++)
  {
    for (j=i+1; j<num_vert; j++)
      num_edge += adj[LINCOOR(i,j,num_vert)];
  }

  /* Allocate the array that holds all the vertex indices for the newly
     created vertices on the edges. */
  edge_index_arr = malloc(2*num_edge*(s-1)*sizeof(*edge_index_arr));
  /* Allocate the matrix that holds the pointers to the indices for the
     newly created vertices on the edges and populate the matrix with
     pointers into edge_index_arr. */
  edge_indices = malloc(num_vert*num_vert*sizeof(*edge_indices));
  if (edge_index_arr == NULL || edge_indices == NULL)
    abort();

  n = 0;
  for (i=0; i<num_vert; i++)
  {
    for (j=0; j<num_vert; j++)
    {
      k = LINCOOR(i,j,num_vert);
      if (adj[k])
      {
        edge_indices[k] = &edge_index_arr[n*(s-1)];
        n++;
      }
      else
      {
        edge_indices[k] = NULL;
      }
    }
  }

  /* Copy the input vertices to the output vertices. */
  for (i=0; i<num_vert; i++)
  {
    k = 3*i;
    vs[k+0] = v[k+0];
    vs[k+1] = v[k+1];
    vs[k+2] = v[k+2];
  }
  n = num_vert;

  /* Compute the s-1 vertices that are added on each edge and set the edge
     indices in the upper right half of the edge_indices array. */
  for (i=0; i<num_vert; i++)
  {
    l1 = 3*i;
    for (j=i+1; j<num_vert; j++)
    {
      l2 = 3*j;
      k = LINCOOR(i,j,num_vert);
      if (adj[k])
      {
        for (m=1; m<=s-1; m++)
        {
          l3 = 3*n;
          t = (float)m/(float)s;
          vs[l3+0] = (1.0f-t)*v[l1+0]+t*v[l2+0];
          vs[l3+1] = (1.0f-t)*v[l1+1]+t*v[l2+1];
          vs[l3+2] = (1.0f-t)*v[l1+2]+t*v[l2+2];
          edge_indices[k][m-1] = n;
          n++;
        }
      }
    }
  }

  /* Set the lower left half of the edge_indices array to the reverse list
     stored in the corresponding entry in the upper right half. */
  for (i=0; i<num_vert; i++)
  {
    for (j=i+1; j<num_vert; j++)
    {
      k1 = LINCOOR(i,j,num_vert);
      k2 = LINCOOR(j,i,num_vert);
      if (adj[k1])
      {
        for (m=1; m<=s-1; m++)
          edge_indices[k2][s-1-m] = edge_indices[k1][m-1];
      }
    }
  }

  m = 0;
  for (i=0; i<num_tri; i++)
  {
    /* Compute the vertices that are added inside each triangle. */
    i1 = tri[3*i+0];
    i2 = tri[3*i+1];
    i3 = tri[3*i+2];
    o = 0;
    vi[o++] = i2;
    for (j=1; j<=s-1; j++)
    {
      k1 = edge_indices[LINCOOR(i2,i3,num_vert)][j-1];
      k2 = edge_indices[LINCOOR(i2,i1,num_vert)][j-1];
      vi[o++] = k1;
      for (k=1; k<=j-1; k++)
      {
        l1 = 3*k1;
        l2 = 3*k2;
        l3 = 3*n;
        t = (float)k/(float)j;
        vs[l3+0] = (1.0f-t)*vs[l1+0]+t*vs[l2+0];
        vs[l3+1] = (1.0f-t)*vs[l1+1]+t*vs[l2+1];
        vs[l3+2] = (1.0f-t)*vs[l1+2]+t*vs[l2+2];
        vi[o++] = n;
        n++;
      }
      vi[o++] = k2;
    }
    vi[o++] = i3;
    for (k=1; k<=j-1; k++)
    {
      k1 = edge_indices[LINCOOR(i3,i1,num_vert)][k-1];
      vi[o++] = k1;
    }
    vi[o++] = i1;

    /* Compute the triangles of the subdivided triangle. */
    for (j=0; j<s; j++)
    {
      k1 = j*(j+1)/2;
      k2 = k1+j+1;
      for (k=0; k<j; k++)
      {
        ts[m+0] = vi[k1+k];
        ts[m+1] = vi[k2+k];
        ts[m+2] = vi[k2+k+1];
        ts[m+3] = vi[k1+k];
        ts[m+4] = vi[k2+k+1];
        ts[m+5] = vi[k1+k+1];
        m += 6;
      }
      ts[m+0] = vi[k1+k];
      ts[m+1] = vi[k2+k];
      ts[m+2] = vi[k2+k+1];
      m += 3;
    }
  }

  /* Normalize the length of the vertices so that they lie on the sphere of
     radius r. */
  for (i=0; i<n; i++)
  {
    k = 3*i;
    normalize_to_length(&vs[k],r);
  }

  /* Free the temporary data structures. */
  free(edge_indices);
  free(edge_index_arr);
  free(vi);
  free(adj);
}


/* Generate the normals of the icosphere icosp. */
static void gen_icosphere_normals(icosphere_data *icosp)
{
  int i, k, n;
  const float *vert;
  float *norm;

  vert = icosp->vert;
  norm = icosp->norm;
  n = icosp->num_vert;

  for (i=0; i<n; i++)
  {
    k = 3*i;
    norm[k+0] = vert[k+0];
    norm[k+1] = vert[k+1];
    norm[k+2] = vert[k+2];
    normalize(&norm[k]);
  }
}


/* Generate the complete data necessary to render an icosphere with radius r
   by subdividing the triangles of the icosahedron icosa into s segments
   along each edge, resulting in s*s triangles for each triangle of the
   icosahedron. */
static icosphere_data *gen_icosphere_data(const icosahedron_data *icosa,
                                          int s, float r)
{
  icosphere_data *icosp;

  icosp = malloc(sizeof(*icosp));
  if (icosp == NULL)
    return NULL;

  icosp->num_vert = s*s*(icosa->num_vert-2)+2;
  icosp->num_tri = s*s*icosa->num_tri;
  icosp->vert = malloc(3*(icosp->num_vert)*sizeof(*icosp->vert));
  icosp->norm = malloc(3*icosp->num_vert*sizeof(*icosp->norm));
  icosp->tri = malloc(3*(icosp->num_tri)*sizeof(*icosp->tri));
  icosp->stri = malloc(3*(icosp->num_tri)*sizeof(*icosp->stri));
  if (icosp->vert == NULL || icosp->norm == NULL ||
      icosp->tri == NULL || icosp->stri == NULL)
    return NULL;

  gen_icosphere(icosa,icosp,s,r);
  gen_icosphere_normals(icosp);

  /* Cause the sorted triangles and sorted triangle normals to be computed
     in sort_icosphere_triangles by setting the sort matrix to a null
     matrix. */
  memset(icosp->smat,0,sizeof(icosp->smat));

  return icosp;
}


/* Compare the z values of two triangles; used by qsort. */
static int compare_z(const void *p1, const void *p2)
{
  const trisort *t1, *t2;

  t1 = (const trisort *)p1;
  t2 = (const trisort *)p2;
  if (t1->z > t2->z)
    return 1;
  else if (t1->z < t2->z)
    return -1;
  else
    return 0;
}


/* Sort the triangles of the icosphere icosp based on the rotation given by
   mat.  The sort is only performed if mat is different from the matrix smat
   stored in icosp.  Return false if no sort was necessary and true if a
   sort has been performed. */
static Bool sort_icosphere_triangles(icosphere_data *icosp, float mat[3][3])
{
  int i, j, k, l;
  float *vertz;
  trisort *tris;
  int num_vert, num_tri;
  const float *vert;
  const int *tri;
  int *stri;

  if (!memcmp(icosp->smat,mat,sizeof(icosp->smat)))
    return False;

  num_vert = icosp->num_vert;
  num_tri = icosp->num_tri;
  vert = icosp->vert;
  tri = icosp->tri;
  stri = icosp->stri;

  vertz = malloc(num_vert*sizeof(*vertz));
  tris = malloc(num_tri*sizeof(*tris));
  if (vertz == NULL || tris == NULL)
    abort();

  /* Initialize the indices of the triangle sorting data structure. */
  for (i=0; i<num_tri; i++)
    tris[i].idx = i;

  /* Compute the z coordinates of the vertices transformed with mat. */
  for (i=0; i<num_vert; i++)
  {
    l = 3*i;
    vertz[i] = mat[2][0]*vert[l+0]+mat[2][1]*vert[l+1]+mat[2][2]*vert[l+2];
  }

  /* Compute the z coordinate of the centroid of the transformed triangles. */
  for (i=0; i<num_tri; i++)
  {
    l = 3*i;
    tris[i].z = (1.0f/3.0f)*(vertz[tri[l+0]]+vertz[tri[l+1]]+vertz[tri[l+2]]);
  }

  /* Sort the z values of the centroids of the triangles. */
  qsort(tris,num_tri,sizeof(*tris),compare_z);

  /* Compute the sorted triangles according to the indices returned by
     sorting the triangles. */
  for (i=0; i<num_tri; i++)
  {
    k = 3*i;
    l = 3*tris[i].idx;
    for (j=0; j<3; j++)
      stri[k+j] = tri[l+j];
  }

  free(tris);
  free(vertz);

  /* Store the current transformation matrix with the icosphere. */
  memcpy(icosp->smat,mat,sizeof(icosp->smat));

  return True;
}


/* Free the icosphere data stored in icosp. */
static void free_icosphere_data(icosphere_data *icosp)
{
  free(icosp->stri);
  free(icosp->tri);
  free(icosp->norm);
  free(icosp->vert);
  free(icosp);
}


/* Initialize a Hopf circle fiber with base (a,b,c).  It is assumed that the
   vector (a,b,c) has Euclidean length 1, i.e., that it lies on the unit
   sphere S² in 3D. */
static void init_hopf_circle(const hopf_base_point *hb, hopf_circle_par *hc)
{
  hc->base = *hb;
  hc->al = sqrtf(0.5f*(1.0f+hb->c));
  hc->be = sqrtf(0.5f*(1.0f-hb->c));
  hc->atab = atan2f(-hb->a,hb->b);
}


/* Calculate a Hopf circle point with parameter phi. */
static void hopf_circle_point(const hopf_circle_par *hc, float phi,
                              hopf_circle_pnt *hcp)
{
  float pht, tht;
  float w, x, y, z, r;

  tht = phi;
  pht = hc->atab-phi;

  w = hc->al*cosf(tht);
  x = -hc->be*cosf(pht);
  y = -hc->be*sinf(pht);
  z = hc->al*sinf(tht);
  r = acosf(w)/(M_PI_F*sqrtf(1.0f-w*w));
  hcp->p[0] = x*r;
  hcp->p[1] = y*r;
  hcp->p[2] = z*r;
  hcp->phi = phi;
}


/* Calculate the unit tangent vector of a Hopf circle point with parameter
   phi. */
static void hopf_circle_tangent(const hopf_circle_par *hc, float phi,
                                float t[3])
{
  float pht, tht;
  float w, x, y, z, r, d, n, s;
  float dwdp, dxdp, dydp, dzdp, drdp;
  float dndp, dddp;

  tht = phi;
  pht = hc->atab-phi;

  w = hc->al*cosf(tht);
  x = -hc->be*cosf(pht);
  y = -hc->be*sinf(pht);
  z = hc->al*sinf(tht);

  s = sqrtf(1.0f-w*w);
  n = acosf(w);
  d = M_PI_F*s;
  r = n/d;

  dwdp = -hc->al*sinf(tht);
  dxdp = -hc->be*sinf(pht);
  dydp = hc->be*cosf(pht);
  dzdp = hc->al*cosf(tht);

  dndp = -dwdp/s;
  dddp = -M_PI_F*w*dwdp/s;
  drdp = (dndp*d-n*dddp)/(d*d);

  t[0] = dxdp*r+x*drdp;
  t[1] = dydp*r+y*drdp;
  t[2] = dzdp*r+z*drdp;
  normalize(t);
}


/* Calculate the unit binormal vector for a Hopf circle. */
static void hopf_circle_binormal(const hopf_circle_par *hc, float n[3])
{
  float a, b, c, sab, pisqr8, sq1pc, sq1mc, ac;

  a = hc->base.a;
  b = hc->base.b;
  c = hc->base.c;
  sab = sqrtf(a*a+b*b);
  if (sab > 0.0f)
  {
    pisqr8 = 2.0f*M_SQRT2_F*M_PI_F;
    sq1mc = sqrtf(1.0f-c);
    sq1pc = sqrtf(1.0f+c);
    ac = acosf(-sq1pc/M_SQRT2_F);
    n[0] = -a*sq1pc*ac/(pisqr8*sab);
    n[1] = -b*sq1pc*ac/(pisqr8*sab);
    n[2] = sq1mc*ac/pisqr8;
    normalize(n);
  }
  else if (c >= 0.0f) /* Actually, c = 1.0f in this case. */
  {
    n[0] = 1.0f;
    n[1] = 0.0f;
    n[2] = 0.0f;
  }
  else /* c < 0.0f, i.e., c = -1.0f in this case. */
  {
    n[0] = 0.0f;
    n[1] = 0.0f;
    n[2] = 1.0f;
  }
}


/* Generate a Hopf circle as a polygon that approximates the true Hopf
   circle with a maximum deviation <= MAX_CIRCLE_DIST. */
static void gen_hopf_circle_points(ModeInfo *mi, const hopf_circle_par *hc,
                                   hopf_circle_pnt *hcpa, int *num)
{
  hopffibrationstruct *hf = &hopffibration[MI_SCREEN(mi)];
  int i, n, sp;
  float phi, d, b[3];
  hopf_circle_pnt hcp, hcps, hcpe;
  hopf_circle_seg hcs[MAX_CIRCLE_STACK];

  if (hc->base.c < M_COS_1_F)
  {
    /* Compute the initial four Hopf circle segments. */
    sp = 0;
    for (i=3; i>=0; i--)
    {
      phi = (float)i*M_PI_F/2.0f;
      hopf_circle_point(hc,phi,&hcs[sp].s);
      phi = (float)(i+1)*M_PI_F/2.0f;
      hopf_circle_point(hc,phi,&hcs[sp].e);
      sp++;
    }

    /* Recursively subdivide the Hopf circle segments until they approximate
       the Hopf circle by a distance <= MAX_CIRCLE_DIST. */
    n = 0;
    sp--;
    while (sp >= 0)
    {
      hcps = hcs[sp].s;
      hcpe = hcs[sp].e;
      phi = 0.5f*(hcps.phi+hcpe.phi);
      hopf_circle_point(hc,phi,&hcp);
      d = distance_point_line(hcp.p,hcps.p,hcpe.p);
      if (d > hf->max_circle_dist)
      {
        if (sp >= MAX_CIRCLE_STACK-2)
          abort();
        hcs[sp].s.p[0] = hcp.p[0];
        hcs[sp].s.p[1] = hcp.p[1];
        hcs[sp].s.p[2] = hcp.p[2];
        hcs[sp].s.phi = phi;
        hcs[sp].e = hcpe;
        sp++;
        hcs[sp].s = hcps;
        hcs[sp].e.p[0] = hcp.p[0];
        hcs[sp].e.p[1] = hcp.p[1];
        hcs[sp].e.p[2] = hcp.p[2];
        hcs[sp].e.phi = phi;
        sp++;
      }
      else
      {
        if (n >= MAX_CIRCLE_PNT-1)
          abort();
        hcpa[n] = hcps;
        n++;
      }
      sp--;
    }
    hcpa[n] = hcpe;
    n++;

    /* Compute the unit tangent, unit normal, and unit binormal vectors of
       each Hopf circle point. Note that the binormal vector is identical
       for all Hopf circle points since the projected Hopf circle lies in a
       plane in 3D. */
    hopf_circle_binormal(hc,b);
    for (i=0; i<n; i++)
    {
      hcpa[i].b[0] = b[0];
      hcpa[i].b[1] = b[1];
      hcpa[i].b[2] = b[2];
      hopf_circle_tangent(hc,hcpa[i].phi,hcpa[i].t);
      cross(hcpa[i].n,hcpa[i].b,hcpa[i].t);
    }
  }
  else /* hc->base.c >= M_COS_1_F */
  {
    /* The circle for (a,b,c) = (0,0,1) projects to a vertical line segment. */
    hcpa[0].p[0] = 0.0f;
    hcpa[0].p[1] = 0.0f;
    hcpa[0].p[2] = 1.0f;
    hcpa[0].t[0] = 0.0f;
    hcpa[0].t[1] = 0.0f;
    hcpa[0].t[2] = 1.0f;
    hcpa[0].n[0] = 0.0f;
    hcpa[0].n[1] = 1.0f;
    hcpa[0].n[2] = 0.0f;
    hcpa[0].b[0] = 1.0f;
    hcpa[0].b[1] = 0.0f;
    hcpa[0].b[2] = 0.0f;
    hcpa[0].phi = 0.0f;

    hcpa[1].p[0] = 0.0f;
    hcpa[1].p[1] = 0.0f;
    hcpa[1].p[2] = -1.0f;
    hcpa[1].t[0] = 0.0f;
    hcpa[1].t[1] = 0.0f;
    hcpa[1].t[2] = 1.0f;
    hcpa[1].n[0] = 0.0f;
    hcpa[1].n[1] = 1.0f;
    hcpa[1].n[2] = 0.0f;
    hcpa[1].b[0] = 1.0f;
    hcpa[1].b[1] = 0.0f;
    hcpa[1].b[2] = 0.0f;
    hcpa[1].phi = 2.0f*M_PI_F;

    n = 2;
  }

  *num = n;
}


/* Rotate the base point bp with the rotation matrix m. */
static void rotate_base_point(float m[3][3], const hopf_base_point *bp,
                              hopf_base_point *bpr)
{
  bpr->a = m[0][0]*bp->a+m[0][1]*bp->b+m[0][2]*bp->c;
  bpr->b = m[1][0]*bp->a+m[1][1]*bp->b+m[1][2]*bp->c;
  bpr->c = m[2][0]*bp->a+m[2][1]*bp->b+m[2][2]*bp->c;
}


/* Compute the color corresponding to the fiber (a,b,c). */
static inline void color(const hopf_base_point *hb, float col[4])
{
  col[0] = 0.5f*(1.0f+hb->a);
  col[1] = 0.5f*(1.0f+hb->b);
  col[2] = 0.5f*(1.0f+hb->c);
  col[3] = 1.0f;
}


/* Draw the icosphere icosp using the OpenGL fixed functionality. */
static void draw_icosphere_ff(ModeInfo *mi, icosphere_data *icosp)
{
  hopffibrationstruct *hf = &hopffibration[MI_SCREEN(mi)];
  int i, j, l;
  float mat[3][3];
  int num_tri;
  const float *vert;
  const int *tri;
  const float *norm;
#ifdef ROT_BASE_SPACE
  float matl[3][3], mats[3][3], matq[3][3];
  float qu[4], qr[3][3];

  gltrackball_get_quaternion(hf->trackball,qu);
  quat_to_rotmat_trackball(qu,qr);
  quat_to_rotmat(hf->quat_space,mats);
  look_at_rotmat(matl,eye_pos[0],eye_pos[1],eye_pos[2],
                 0.0f,0.0f,0.0f,0.0f,1.0f,0.0f);
  mult_rotmat(matq,matl,qr);
  mult_rotmat(mat,matq,mats);
#else
  look_at_rotmat(mat,eye_pos[0],eye_pos[1],eye_pos[2],
                 0.0f,0.0f,0.0f,0.0f,1.0f,0.0f);
#endif
  rotatex(mat,hf->alpha);
  rotatey(mat,hf->beta);
  rotatez(mat,hf->delta);
  sort_icosphere_triangles(icosp,mat);

  num_tri = icosp->num_tri;
  vert = icosp->vert;
  tri = icosp->stri;
  norm = icosp->norm;

  glBegin(GL_TRIANGLES);
  for (i=0; i<num_tri; i++)
  {
    for (j=0; j<3; j++)
    {
      l = tri[3*i+j];
      glNormal3fv(&norm[3*l]);
      glVertex3fv(&vert[3*l]);
    }
  }
  glEnd();

  hf->num_poly += num_tri;
}


/* Draw a Hopf circle with base (a,b,c) using the OpenGL fixed
   functionality. */
static void draw_hopf_circle_ff(ModeInfo *mi, const hopf_base_point *hb)
{
  hopffibrationstruct *hf = &hopffibration[MI_SCREEN(mi)];
  int i, j, k, l, m, num;
  float p[3], n[3], phi, cp, sp, t, sgn;
  hopf_circle_par hc;
  hopf_circle_pnt hcpa[MAX_CIRCLE_PNT];

  init_hopf_circle(hb,&hc);
  gen_hopf_circle_points(mi,&hc,hcpa,&num);

  for (i=0; i<num-1; i++)
  {
    glBegin(GL_TRIANGLE_STRIP);
    for (j=hf->num_tube; j>=0; j--)
    {
      for (k=0; k<=1; k++)
      {
        l = i+k;
        phi = j*(2.0f*M_PI_F/hf->num_tube);
        cp = cosf(phi);
        sp = sinf(phi);
        for (m=0; m<3; m++)
        {
          t = cp*hcpa[l].n[m]+sp*hcpa[l].b[m];
          p[m] = hcpa[l].p[m]+TUBE_RADIUS*t;
          n[m] = t;
        }
        glNormal3fv(n);
        glVertex3fv(p);
      }
    }
    glEnd();
  }

  hf->num_poly += 2*(num-1)*hf->num_tube;

  if (hc.base.c >= M_COS_1_F)
  {
    /* In this case, the Hopf circle projects to a vertical tube, which is
       drawn by the above code. In addition, we must draw a circle at each
       end of the tube. Note that we rely on the fact that num == 2 in this
       case. */
    for (i=0; i<num; i++)
    {
      if (i == 0)
        sgn = 1.0f;
      else
        sgn = -1.0f;
      n[0] = 0.0f;
      n[1] = 0.0f;
      n[2] = sgn;

      glBegin(GL_TRIANGLE_FAN);
      for (m=0; m<3; m++)
        p[m] = hcpa[i].p[m];
      glNormal3fv(n);
      glVertex3fv(p);

      for (j=hf->num_tube; j>=0; j--)
      {
        phi = sgn*j*(2.0f*M_PI_F/hf->num_tube);
        cp = cosf(phi);
        sp = sinf(phi);
        for (m=0; m<3; m++)
        {
          t = cp*hcpa[i].n[m]+sp*hcpa[i].b[m];
          p[m] = hcpa[i].p[m]+TUBE_RADIUS*t;
        }
        glNormal3fv(n);
        glVertex3fv(p);
      }
      glEnd();
    }

    hf->num_poly += hf->num_tube;
  }
}


/* Draw the num_bp Hopf circles given by hb using the OpenGL fixed
   functionality. */
static int draw_hopf_circles_ff(ModeInfo *mi)
{
  static const GLfloat light_ambient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
  static const GLfloat light_diffuse[] = { 0.7f, 0.7f, 0.7f, 1.0f };
  static const GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  static const GLfloat light_pos[] = { 0.7f, 0.7f, 1.0f, 0.0f };
  static const GLfloat mat_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  static const GLfloat mat_gray[] = { 0.7f, 0.7f, 0.7f, 0.6f };
  hopffibrationstruct *hf = &hopffibration[MI_SCREEN(mi)];
  hopf_base_point ht;
  float col[4], mat[3][3], mat1[3][3], mat2[3][3], mats[16];
  float qu[4], qr[16];
  int i;

  hf->num_poly = 0;

  /* Compute the rotation matrix that rotates the base points. */
  identity(mat1);
  rotatex(mat1,hf->zeta);
  rotatey(mat1,hf->eta);
  rotatez(mat1,hf->theta);
  quat_to_rotmat(hf->quat_base,mat2);
  mult_rotmat(mat,mat2,mat1);

  /* Compute the rotation matrix that rotates the space points. */
  quat_to_rotmat_4x4(hf->quat_space,mats);

  /* Compute the space rotation matrix from the trackball state. */
  gltrackball_get_quaternion(hf->trackball,qu);
  quat_to_rotmat_trackball_4x4(qu,qr);

  glViewport(0,0,hf->WindW,hf->WindH);

  glClearColor(0.0f,0.0f,0.0f,0.0f);
  glClearDepth(1.0f);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  if (hf->projection == DISP_ORTHOGRAPHIC)
  {
    if (hf->aspect >= 1.0f)
      glOrtho(-1.2*hf->aspect,1.2*hf->aspect,-1.2,1.2,0.1,10.0);
    else
      glOrtho(-1.2,1.2,-1.2/hf->aspect,1.2/hf->aspect,0.1,10.0);
  }
  else /* hf->projection == DISP_PERSPECTIVE */
  {
    if (hf->aspect >= 1.0f)
      gluPerspective(45.0,hf->aspect,0.1,10.0);
    else
      gluPerspective(360.0/M_PI*atan(tan(45.0*M_PI/360.0)/hf->aspect),
                     hf->aspect,0.1,10.0);
  }

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
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
  glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,50.0f);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);

  gluLookAt(eye_pos[0],eye_pos[1],eye_pos[2],0.0,0.0,0.0,0.0,1.0,0.0);
  glMultMatrixf(qr);
  glMultMatrixf(mats);
  glRotatef(hf->alpha,1.0f,0.0f,0.0f);
  glRotatef(hf->beta,0.0f,1.0f,0.0f);
  glRotatef(hf->delta,0.0f,0.0f,1.0f);

  /* Draw the fibers (Hopf circles). */
  for (i=0; i<hf->num_base_points; i++)
  {
    rotate_base_point(mat,&hf->base_points[i],&ht);

    color(&ht,col);
    glColor3fv(col);
    glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,col);

    draw_hopf_circle_ff(mi,&ht);
  }

  if (hf->base_space)
  {
    /* Draw the base points. */
    for (i=0; i<hf->num_base_points; i++)
    {
      rotate_base_point(mat,&hf->base_points[i],&ht);

      glMatrixMode(GL_MODELVIEW);
      glLoadIdentity();

      gluLookAt(eye_pos[0],eye_pos[1],eye_pos[2],0.0,0.0,0.0,0.0,1.0,0.0);
      glTranslatef(base_offset[0],base_offset[1],base_offset[2]);
#ifdef ROT_BASE_SPACE
      glMultMatrixf(qr);
      glMultMatrixf(mats);
#endif
      glRotatef(hf->alpha,1.0f,0.0f,0.0f);
      glRotatef(hf->beta,0.0f,1.0f,0.0f);
      glRotatef(hf->delta,0.0f,0.0f,1.0f);
      glTranslatef(ht.a*BASE_SPHERE_RADIUS,ht.b*BASE_SPHERE_RADIUS,
                   ht.c*BASE_SPHERE_RADIUS);

      color(&ht,col);
      glColor3fv(col);
      glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,col);

      draw_icosphere_ff(mi,hf->sphere_base_point);
    }

    /* Draw the base sphere. */
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    gluLookAt(eye_pos[0],eye_pos[1],eye_pos[2],0.0,0.0,0.0,0.0,1.0,0.0);
    glTranslatef(base_offset[0],base_offset[1],base_offset[2]);
#ifdef ROT_BASE_SPACE
    glMultMatrixf(qr);
    glMultMatrixf(mats);
#endif
    glRotatef(hf->alpha,1.0f,0.0f,0.0f);
    glRotatef(hf->beta,0.0f,1.0f,0.0f);
    glRotatef(hf->delta,0.0f,0.0f,1.0f);

    glColor3fv(mat_gray);
    glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,mat_gray);
    draw_icosphere_ff(mi,hf->sphere_base);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
  }

  return hf->num_poly;
}


#ifdef HAVE_GLSL

/* Generate vertex, normal, and index buffers for the icosphere icosp. */
static GLuint gen_icosphere_buffers(ModeInfo *mi, icosphere_data *icosp,
                                    GLint pos_buffer, GLuint normal_buffer,
                                    GLuint indices_buffer)
{
  hopffibrationstruct *hf = &hopffibration[MI_SCREEN(mi)];
  float mat[3][3];
  int num_tri, num_vert;
  const float *vert;
  const int *tri;
  const float *norm;
#ifdef ROT_BASE_SPACE
  float matl[3][3], mats[3][3], matq[3][3];
  float qu[4], qr[3][3];

  gltrackball_get_quaternion(hf->trackball,qu);
  quat_to_rotmat_trackball(qu,qr);
  quat_to_rotmat(hf->quat_space,mats);
  look_at_rotmat(matl,eye_pos[0],eye_pos[1],eye_pos[2],
                 0.0f,0.0f,0.0f,0.0f,1.0f,0.0f);
  mult_rotmat(matq,matl,qr);
  mult_rotmat(mat,matq,mats);
#else
  look_at_rotmat(mat,eye_pos[0],eye_pos[1],eye_pos[2],
                 0.0f,0.0f,0.0f,0.0f,1.0f,0.0f);
#endif
  rotatex(mat,hf->alpha);
  rotatey(mat,hf->beta);
  rotatez(mat,hf->delta);
  sort_icosphere_triangles(icosp,mat);

  num_tri = icosp->num_tri;
  num_vert = icosp->num_vert;
  vert = icosp->vert;
  tri = icosp->stri;
  norm = icosp->norm;

  glBindBuffer(GL_ARRAY_BUFFER,pos_buffer);
  glBufferData(GL_ARRAY_BUFFER,3*num_vert*sizeof(GLfloat),vert,
               GL_STREAM_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

  glBindBuffer(GL_ARRAY_BUFFER,normal_buffer);
  glBufferData(GL_ARRAY_BUFFER,3*num_vert*sizeof(GLfloat),norm,
               GL_STREAM_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,indices_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,3*num_tri*sizeof(GLuint),tri,
               GL_STREAM_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

  return num_tri;
}


/* Generate vertex, normal, and index buffers for a Hopf circle with base
   point (a,b,c). */
static int gen_hopf_circle_buffers(ModeInfo *mi, const hopf_base_point *hb,
                                   GLint pos_buffer, GLuint normal_buffer,
                                   GLuint indices_buffer)
{
  hopffibrationstruct *hf = &hopffibration[MI_SCREEN(mi)];
  int i, j, m, num;
  int num_vert, num_tri;
  float phi, cp, sp, t, sgn, n[3];
  hopf_circle_par hc;
  hopf_circle_pnt hcpa[MAX_CIRCLE_PNT];
  float *vert, *norm;
  int *tri;

  vert = hf->vert;
  norm = hf->norm;
  tri = hf->tri;

  init_hopf_circle(hb,&hc);
  gen_hopf_circle_points(mi,&hc,hcpa,&num);

  num_tri = 0;
  for (i=0; i<num; i++)
  {
    for (j=hf->num_tube-1; j>=0; j--)
    {
      phi = j*(2.0f*M_PI_F/hf->num_tube);
      cp = cosf(phi);
      sp = sinf(phi);
      for (m=0; m<3; m++)
      {
        t = cp*hcpa[i].n[m]+sp*hcpa[i].b[m];
        vert[3*(i*hf->num_tube+j)+m] = hcpa[i].p[m]+TUBE_RADIUS*t;
        norm[3*(i*hf->num_tube+j)+m] = t;
      }
      if (i < num-1)
      {
        tri[3*num_tri+0] = i*hf->num_tube+(j+1)%hf->num_tube;
        tri[3*num_tri+1] = (i+1)*hf->num_tube+(j+1)%hf->num_tube;
        tri[3*num_tri+2] = i*hf->num_tube+j;
        tri[3*num_tri+3] = i*hf->num_tube+j;
        tri[3*num_tri+4] = (i+1)*hf->num_tube+(j+1)%hf->num_tube;
        tri[3*num_tri+5] = (i+1)*hf->num_tube+j;
        num_tri += 2;
      }
    }
  }
  num_vert = num*hf->num_tube;

  if (hc.base.c >= M_COS_1_F)
  {
    /* In this case, the Hopf circle projects to a vertical tube, which is
       created by the above code. In addition, we must draw a disk at each
       end of the tube.  Note that we rely on the fact that num == 2 in this
       case.*/
    for (i=0; i<num; i++)
    {
      if (i == 0)
        sgn = 1.0f;
      else
        sgn = -1.0f;
      n[0] = 0.0f;
      n[1] = 0.0f;
      n[2] = sgn;

      /* Add the center of the circle to the vertex and normal arrays. */
      for (m=0; m<3; m++)
      {
        vert[3*num_vert+m] = hcpa[i].p[m];
        norm[3*num_vert+m] = n[m];
      }

      /* Add the vertices, normals, and triangles for the disk. */
      for (j=hf->num_tube; j>=0; j--)
      {
        phi = sgn*j*(2.0f*M_PI_F/hf->num_tube);
        cp = cosf(phi);
        sp = sinf(phi);
        for (m=0; m<3; m++)
        {
          t = cp*hcpa[i].n[m]+sp*hcpa[i].b[m];
          vert[3*(num_vert+j+1)+m] = hcpa[i].p[m]+TUBE_RADIUS*t;
          norm[3*(num_vert+j+1)+m] = n[m];
        }
        if (j > 0)
        {
          tri[3*num_tri+0] = num_vert;
          tri[3*num_tri+1] = num_vert+j+1;
          tri[3*num_tri+2] = num_vert+j;
          num_tri++;
        }
      }
      num_vert += hf->num_tube+2;
    }
  }

  glBindBuffer(GL_ARRAY_BUFFER,pos_buffer);
  glBufferData(GL_ARRAY_BUFFER,3*num_vert*sizeof(GLfloat),vert,
               GL_STREAM_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER,normal_buffer);
  glBufferData(GL_ARRAY_BUFFER,3*num_vert*sizeof(GLfloat),norm,
               GL_STREAM_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,indices_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,3*num_tri*sizeof(GLuint),tri,
               GL_STREAM_DRAW);

  return num_tri;
}


/* Draw a set of vertex, normal, and index buffers. */
static void draw_buffers(ModeInfo *mi, int from_light, GLint pos_buffer,
                         GLuint normal_buffer, GLuint indices_buffer,
                         GLuint num_tri)
{
  hopffibrationstruct *hf = &hopffibration[MI_SCREEN(mi)];

  if (from_light)
  {
    glEnableVertexAttribArray(hf->shadow_pos_index);
    glBindBuffer(GL_ARRAY_BUFFER,pos_buffer);
    glVertexAttribPointer(hf->shadow_pos_index,3,GL_FLOAT,GL_FALSE,0,0);
  }
  else
  {
    glEnableVertexAttribArray(hf->pos_index);
    glBindBuffer(GL_ARRAY_BUFFER,pos_buffer);
    glVertexAttribPointer(hf->pos_index,3,GL_FLOAT,GL_FALSE,0,0);

    glEnableVertexAttribArray(hf->normal_index);
    glBindBuffer(GL_ARRAY_BUFFER,normal_buffer);
    glVertexAttribPointer(hf->normal_index,3,GL_FLOAT,GL_FALSE,0,0);

    glDisableVertexAttribArray(hf->color_index);
  }

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,indices_buffer);

  glDrawElements(GL_TRIANGLES,3*num_tri,GL_UNSIGNED_INT,(const GLvoid *)0);

  if (from_light)
  {
    glDisableVertexAttribArray(hf->shadow_pos_index);
  }
  else
  {
    glDisableVertexAttribArray(hf->pos_index);
    glDisableVertexAttribArray(hf->normal_index);
  }
  glBindBuffer(GL_ARRAY_BUFFER,0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

  hf->num_poly += num_tri;
}


/* Draw the num_bp Hopf circles given by hb using the OpenGL programmable
   functionality. */
static int draw_hopf_circles_pf(ModeInfo *mi)
{
  static const GLfloat light_model_ambient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
  static const GLfloat light_ambient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
  static const GLfloat light_diffuse[] = { 0.7f, 0.7f, 0.7f, 1.0f };
  static const GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  static const GLfloat light_pos[] = { 0.7f, 0.7f, 1.0f, 0.0f };
  static const GLfloat mat_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  static const GLfloat mat_diff_white[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  static const GLfloat mat_gray[] = { 0.7f, 0.7f, 0.7f, 0.6f };
  hopffibrationstruct *hf = &hopffibration[MI_SCREEN(mi)];
  GLfloat p_mat[16], mv_mat[16];
  GLfloat lp_mat[16], lmv_mat[16], lmvp_mat[16], lmvp_bp_mat[16];
  GLfloat light_direction[3], half_vector[3], len;
  hopf_base_point ht;
  float col[4], mat[3][3], mat1[3][3], mat2[3][3], mats[16];
  float qu[4], qr[16];
  float mid_depth;
  int i, from_light;

  hf->num_poly = 0;

  if (hf->use_vao)
    glBindVertexArray(hf->vertex_array_object);

  len = norm(light_pos);
  light_direction[0] = light_pos[0]/len;
  light_direction[1] = light_pos[1]/len;
  light_direction[2] = light_pos[2]/len;
  half_vector[0] = light_direction[0];
  half_vector[1] = light_direction[1];
  half_vector[2] = light_direction[2]+1.0f;
  len = sqrtf(half_vector[0]*half_vector[0]+
              half_vector[1]*half_vector[1]+
              half_vector[2]*half_vector[2]);
  half_vector[0] /= len;
  half_vector[1] /= len;
  half_vector[2] /= len;

  /* Compute the rotation matrix that rotates the base points. */
  identity(mat1);
  rotatex(mat1,hf->zeta);
  rotatey(mat1,hf->eta);
  rotatez(mat1,hf->theta);
  quat_to_rotmat(hf->quat_base,mat2);
  mult_rotmat(mat,mat2,mat1);

  /* Compute the rotation matrix that rotates the space points. */
  quat_to_rotmat_4x4(hf->quat_space,mats);

  /* Compute the space rotation matrix from the trackball state. */
  gltrackball_get_quaternion(hf->trackball,qu);
  quat_to_rotmat_trackball_4x4(qu,qr);

  /* Generate the icosphere for the base. */
  hf->sphere_base_num_tri =
    gen_icosphere_buffers(mi,hf->sphere_base,hf->sphere_base_pos_buffer,
                          hf->sphere_base_normal_buffer,
                          hf->sphere_base_indices_buffer);

  /* Generate the icosphere for a base point. */
  hf->base_point_num_tri =
    gen_icosphere_buffers(mi,hf->sphere_base_point,hf->base_point_pos_buffer,
                          hf->base_point_normal_buffer,
                          hf->base_point_indices_buffer);

  /* Generate the Hopf circles. */
  for (i=0; i<hf->num_base_points; i++)
  {
    rotate_base_point(mat,&hf->base_points[i],&ht);
    hf->fiber_num_tri[i] =
      gen_hopf_circle_buffers(mi,&ht,hf->fiber_pos_buffer[i],
                              hf->fiber_normal_buffer[i],
                              hf->fiber_indices_buffer[i]);
  }

  mid_depth = norm(eye_pos);
  glsl_Identity(p_mat);
  if (hf->projection == DISP_ORTHOGRAPHIC)
  {
    if (hf->aspect >= 1.0f)
      glsl_Orthographic(p_mat,-1.2f*hf->aspect,1.2f*hf->aspect,-1.2f,1.2f,
                        mid_depth-1.2f,mid_depth+1.2f);
    else
      glsl_Orthographic(p_mat,-1.2f,1.2f,-1.2f/hf->aspect,1.2f/hf->aspect,
                        mid_depth-1.2f,mid_depth+1.2f);
  }
  else /* hf->projection == DISP_PERSPECTIVE */
  {
    if (hf->aspect >= 1.0f)
      glsl_Perspective(p_mat,45.0f,hf->aspect,mid_depth-1.2f,mid_depth+1.2f);
    else
      glsl_Perspective(p_mat,
                       360.0f/M_PI_F*atanf(tanf(45.0f*M_PI_F/360.0f)/
                                           hf->aspect),
                       hf->aspect,mid_depth-1.2f,mid_depth+1.2f);
  }

  mid_depth = norm(light_pos);
  glsl_Identity(lp_mat);
  glsl_Orthographic(lp_mat,-1.4f,1.4f,-1.4f,1.4f,
                    mid_depth-1.4f,mid_depth+1.4f);

  for (from_light = (hf->shadows && hf->use_shadow_fbo); from_light >= 0;
       from_light--)
  {
    glsl_Identity(mv_mat);
    glsl_LookAt(mv_mat,eye_pos[0],eye_pos[1],eye_pos[2],
                0.0f,0.0f,0.0f,0.0f,1.0f,0.0f);
    glsl_MultMatrix(mv_mat,qr);
    glsl_MultMatrix(mv_mat,mats);
    glsl_Rotate(mv_mat,hf->alpha,1.0f,0.0f,0.0f);
    glsl_Rotate(mv_mat,hf->beta,0.0f,1.0f,0.0f);
    glsl_Rotate(mv_mat,hf->delta,0.0f,0.0f,1.0f);

    glsl_Identity(lmv_mat);
    glsl_LookAt(lmv_mat,light_pos[0],light_pos[1],light_pos[2],
                0.0f,0.0f,0.0f,0.0f,1.0f,0.0f);
    glsl_MultMatrix(lmv_mat,qr);
    glsl_MultMatrix(lmv_mat,mats);
    glsl_Rotate(lmv_mat,hf->alpha,1.0f,0.0f,0.0f);
    glsl_Rotate(lmv_mat,hf->beta,0.0f,1.0f,0.0f);
    glsl_Rotate(lmv_mat,hf->delta,0.0f,0.0f,1.0f);

    glsl_CopyMatrix(lmvp_mat,lp_mat);
    glsl_MultMatrix(lmvp_mat,lmv_mat);

    if (from_light)
    {
      glBindFramebuffer(GL_FRAMEBUFFER,hf->shadow_fbo);

      glUseProgram(hf->shadow_program);

      glViewport(0,0,hf->shadow_tex_size,hf->shadow_tex_size);

      glClearDepth(1.0f);
      glClear(GL_DEPTH_BUFFER_BIT);

      glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);

      glEnable(GL_POLYGON_OFFSET_FILL);
      glPolygonOffset(4.0f,4.0f);

      glEnable(GL_DEPTH_TEST);
      glDepthFunc(GL_LESS);
      glDepthMask(GL_TRUE);
      glDisable(GL_BLEND);
      glEnable(GL_CULL_FACE);
      glCullFace(GL_BACK);

      glUniformMatrix4fv(hf->shadow_lmvp_index,1,GL_FALSE,lmvp_mat);
    }
    else
    {
      if (hf->use_msaa_fbo)
      {
        glBindFramebuffer(GL_FRAMEBUFFER,hf->msaa_fbo);
      }
      else
      {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,hf->default_draw_fbo);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,hf->default_read_fbo);
      }

      glUseProgram(hf->shader_program);

      glViewport(0,0,hf->WindW,hf->WindH);

      glClearColor(0.0f,0.0f,0.0f,0.0f);
      glClearDepth(1.0f);
      glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

      glEnable(GL_DEPTH_TEST);
      glDepthFunc(GL_LESS);
      glDepthMask(GL_TRUE);
      glDisable(GL_BLEND);
      glEnable(GL_CULL_FACE);
      glCullFace(GL_BACK);

      glUniform4fv(hf->glbl_ambient_index,1,light_model_ambient);
      glUniform4fv(hf->lt_ambient_index,1,light_ambient);
      glUniform4fv(hf->lt_diffuse_index,1,light_diffuse);
      glUniform4fv(hf->lt_specular_index,1,light_specular);
      glUniform3fv(hf->lt_direction_index,1,light_direction);
      glUniform3fv(hf->lt_halfvect_index,1,half_vector);
      glUniform4fv(hf->ambient_index,1,mat_diff_white);
      glUniform4fv(hf->diffuse_index,1,mat_diff_white);
      glUniform4fv(hf->specular_index,1,mat_specular);
      glUniform1f(hf->shininess_index,50.0f);

      glUniformMatrix4fv(hf->proj_index,1,GL_FALSE,p_mat);
      glUniformMatrix4fv(hf->mv_index,1,GL_FALSE,mv_mat);
      glUniformMatrix4fv(hf->lmvp_index,1,GL_FALSE,lmvp_mat);

      glVertexAttrib4f(hf->color_index,1.0f,1.0f,1.0f,1.0f);

      glUniform1i(hf->use_shadows_index,(hf->shadows && hf->use_shadow_fbo));
      glUniform1f(hf->shadow_pix_size_index,1.0f/hf->shadow_tex_size);
      glActiveTexture(GL_TEXTURE0);
      if (hf->shadows && hf->use_shadow_fbo)
        glBindTexture(GL_TEXTURE_2D,hf->shadow_tex);
      else
        glBindTexture(GL_TEXTURE_2D,hf->shadow_dummy_tex);
      glUniform1i(hf->shadow_smpl_index,0);
    }

    /* Draw the fibers (Hopf circles). */
    for (i=0; i<hf->num_base_points; i++)
    {
      rotate_base_point(mat,&hf->base_points[i],&ht);

      if (!from_light)
      {
        color(&ht,col);
        glVertexAttrib4fv(hf->color_index,col);
      }

      draw_buffers(mi,from_light,hf->fiber_pos_buffer[i],
                   hf->fiber_normal_buffer[i],hf->fiber_indices_buffer[i],
                   hf->fiber_num_tri[i]);
    }

    if (hf->base_space)
    {
      /* Draw the base points. */
      for (i=0; i<hf->num_base_points; i++)
      {
        rotate_base_point(mat,&hf->base_points[i],&ht);

        glsl_Identity(mv_mat);
        glsl_LookAt(mv_mat,eye_pos[0],eye_pos[1],eye_pos[2],
                    0.0f,0.0f,0.0f,0.0f,1.0f,0.0f);
        glsl_Translate(mv_mat,base_offset[0],base_offset[1],base_offset[2]);
#ifdef ROT_BASE_SPACE
        glsl_MultMatrix(mv_mat,qr);
        glsl_MultMatrix(mv_mat,mats);
#endif
        glsl_Rotate(mv_mat,hf->alpha,1.0f,0.0f,0.0f);
        glsl_Rotate(mv_mat,hf->beta,0.0f,1.0f,0.0f);
        glsl_Rotate(mv_mat,hf->delta,0.0f,0.0f,1.0f);
        glsl_Translate(mv_mat,
                       ht.a*BASE_SPHERE_RADIUS,
                       ht.b*BASE_SPHERE_RADIUS,
                       ht.c*BASE_SPHERE_RADIUS);

        glsl_Identity(lmv_mat);
        glsl_LookAt(lmv_mat,light_pos[0],light_pos[1],light_pos[2],
                    0.0f,0.0f,0.0f,0.0f,1.0f,0.0f);
        glsl_Translate(lmv_mat,base_offset[0],base_offset[1],base_offset[2]);
#ifdef ROT_BASE_SPACE
        glsl_MultMatrix(lmv_mat,qr);
        glsl_MultMatrix(lmv_mat,mats);
#endif
        glsl_Rotate(lmv_mat,hf->alpha,1.0f,0.0f,0.0f);
        glsl_Rotate(lmv_mat,hf->beta,0.0f,1.0f,0.0f);
        glsl_Rotate(lmv_mat,hf->delta,0.0f,0.0f,1.0f);
        glsl_Translate(lmv_mat,
                       ht.a*BASE_SPHERE_RADIUS,
                       ht.b*BASE_SPHERE_RADIUS,
                       ht.c*BASE_SPHERE_RADIUS);

        glsl_CopyMatrix(lmvp_bp_mat,lp_mat);
        glsl_MultMatrix(lmvp_bp_mat,lmv_mat);

        if (from_light)
        {
          glUniformMatrix4fv(hf->shadow_lmvp_index,1,GL_FALSE,lmvp_bp_mat);
        }
        else
        {
          glUniformMatrix4fv(hf->proj_index,1,GL_FALSE,p_mat);
          glUniformMatrix4fv(hf->mv_index,1,GL_FALSE,mv_mat);
          glUniformMatrix4fv(hf->lmvp_index,1,GL_FALSE,lmvp_bp_mat);

          color(&ht,col);
          glVertexAttrib4fv(hf->color_index,col);
        }

        draw_buffers(mi,from_light,hf->base_point_pos_buffer,
                     hf->base_point_normal_buffer,
                     hf->base_point_indices_buffer,hf->base_point_num_tri);
      }

      if (!from_light)
      {
        /* Draw the base sphere, but not for the shadow buffer pass since
           it is transparent. */
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);

        glsl_Identity(mv_mat);
        glsl_LookAt(mv_mat,eye_pos[0],eye_pos[1],eye_pos[2],
                    0.0f,0.0f,0.0f,0.0f,1.0f,0.0f);
        glsl_Translate(mv_mat,base_offset[0],base_offset[1],base_offset[2]);
#ifdef ROT_BASE_SPACE
        glsl_MultMatrix(mv_mat,qr);
        glsl_MultMatrix(mv_mat,mats);
#endif
        glsl_Rotate(mv_mat,hf->alpha,1.0f,0.0f,0.0f);
        glsl_Rotate(mv_mat,hf->beta,0.0f,1.0f,0.0f);
        glsl_Rotate(mv_mat,hf->delta,0.0f,0.0f,1.0f);

        glsl_Identity(lmv_mat);
        glsl_LookAt(lmv_mat,light_pos[0],light_pos[1],light_pos[2],
                    0.0f,0.0f,0.0f,0.0f,1.0f,0.0f);
        glsl_Translate(lmv_mat,base_offset[0],base_offset[1],base_offset[2]);
#ifdef ROT_BASE_SPACE
        glsl_MultMatrix(lmv_mat,qr);
        glsl_MultMatrix(lmv_mat,mats);
#endif
        glsl_Rotate(lmv_mat,hf->alpha,1.0f,0.0f,0.0f);
        glsl_Rotate(lmv_mat,hf->beta,0.0f,1.0f,0.0f);
        glsl_Rotate(lmv_mat,hf->delta,0.0f,0.0f,1.0f);

        glsl_CopyMatrix(lmvp_bp_mat,lp_mat);
        glsl_MultMatrix(lmvp_bp_mat,lmv_mat);

        glUniformMatrix4fv(hf->proj_index,1,GL_FALSE,p_mat);
        glUniformMatrix4fv(hf->mv_index,1,GL_FALSE,mv_mat);
        glUniformMatrix4fv(hf->lmvp_index,1,GL_FALSE,lmvp_bp_mat);

        glVertexAttrib4fv(hf->color_index,mat_gray);

        draw_buffers(mi,from_light,hf->sphere_base_pos_buffer,
                     hf->sphere_base_normal_buffer,
                     hf->sphere_base_indices_buffer,hf->sphere_base_num_tri);

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
      }
    }

    glUseProgram(0);

    if (from_light)
    {
      glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
      glDisable(GL_POLYGON_OFFSET_FILL);
    }
    else
    {
      if (hf->use_msaa_fbo)
      {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,hf->default_draw_fbo);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,hf->msaa_fbo);

        glBlitFramebuffer(0,0,hf->WindW,hf->WindH,0,0,hf->WindW,hf->WindH,
                          GL_COLOR_BUFFER_BIT,GL_NEAREST);
      }
    }

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,hf->default_draw_fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,hf->default_read_fbo);
  }

  glBindTexture(GL_TEXTURE_2D,0);

  if (hf->use_vao)
    glBindVertexArray(0);

  return hf->num_poly;
}

#endif /* HAVE_GLSL */


/* Generate a torus, a Hopf torus, or a Hopf flower on the base.  A torus is
   generated if q = 0.0 and r = 0.0.  A Hopf torus is generated if r = 0.0.
   Otherwise, a Hopf flower is generated. */
static void gen_hopf_torus_base(ModeInfo *mi, float p, float q, float r,
                                int n, float offset, float sector, int num,
                                Bool rotate, float quat[4])
{
  hopffibrationstruct *hf = &hopffibration[MI_SCREEN(mi)];
  float t, g, h;
  float bp[3], bpr[3];
  float m[3][3];
  int i;

  if (num < 1)
    num = 1;
  if ((p == 0.0f || p == M_PI_F) && q == 0.0f)
    num = 1;
  if (sector == 0.0f)
    num = 1;

  if (rotate)
    quat_to_rotmat(quat,m);

  for (i=0; i<num; i++)
  {
    t = offset+i*sector/num;
    if (q == 0.0f)
      g = p;
    else
      g = p+q*sinf(n*t);
    if (r == 0.0f)
      h = t;
    else
      h = t+r*cosf(n*t);
    bp[0] = cosf(h)*sinf(g);
    bp[1] = sinf(h)*sinf(g);
    bp[2] = cosf(g);
    if (!rotate)
    {
      hf->base_points[hf->num_base_points].a = bp[0];
      hf->base_points[hf->num_base_points].b = bp[1];
      hf->base_points[hf->num_base_points].c = bp[2];
    }
    else
    {
      mult_rotmat_vec(bpr,m,bp);
      hf->base_points[hf->num_base_points].a = bpr[0];
      hf->base_points[hf->num_base_points].b = bpr[1];
      hf->base_points[hf->num_base_points].c = bpr[2];
    }
    hf->num_base_points++;
  }
}


/* Generate a Hopf spiral on the base. */
static void gen_hopf_spiral_base(ModeInfo *mi, float p, float q, float r,
                                 float offset, float sector, int num,
                                 Bool rotate, float quat[4])
{
  hopffibrationstruct *hf = &hopffibration[MI_SCREEN(mi)];
  float t;
  float bp[3], bpr[3];
  float m[3][3];
  int i;

  if (num < 1)
    num = 1;
  if (sector == 0.0f)
    num = 1;

  if (rotate)
    quat_to_rotmat(quat,m);

  for (i=0; i<num; i++)
  {
    t = offset+i*sector/num;
    bp[0] = cosf(-r*t)*cosf(p+0.5f*q*t);
    bp[1] = sinf(-r*t)*cosf(p+0.5f*q*t);
    bp[2] = -sinf(p+0.5f*q*t);
    if (!rotate)
    {
      hf->base_points[hf->num_base_points].a = bp[0];
      hf->base_points[hf->num_base_points].b = bp[1];
      hf->base_points[hf->num_base_points].c = bp[2];
    }
    else
    {
      mult_rotmat_vec(bpr,m,bp);
      hf->base_points[hf->num_base_points].a = bpr[0];
      hf->base_points[hf->num_base_points].b = bpr[1];
      hf->base_points[hf->num_base_points].c = bpr[2];
    }
    hf->num_base_points++;
  }
}


/* An easing function for values of t between 0 and 1. */
static float ease(float t, int easing)
{
  if (easing == EASING_NONE)
    return 0.0f;
  else if (easing == EASING_CUBIC)
    return t*t*(3.0f-2.0f*t);
  else if (easing == EASING_SIN)
    return (t < 0.25f ? 0.5f*ease(4.0f*t,EASING_CUBIC)+0.5f :
            (t > 0.75f ? 0.5f*ease(4.0f*t-3.0f,EASING_CUBIC) :
             1.0f-ease(2.0f*t-0.5f,EASING_CUBIC)));
  else if (easing == EASING_COS)
    return (t < 0.5f ? 1.0f-ease(2.0f*t,EASING_CUBIC) :
            ease(2.0f*t-1.0f,EASING_CUBIC));
  else if (easing == EASING_LIN)
    return t;
  else if (easing == EASING_ACCEL)
    return t*t*(2.0f-t);
  else if (easing == EASING_DECEL)
    return t*(1.0f+t*(1.0f-t));
  else
    return 0.0f;
}


/* Compute a quaternion from a rotation axis and an angle in radians. */
static void axis_angle_to_quat(float rot_axis[3], float angle, float q[4])
{
  q[0] = cosf(0.5f*angle);
  q[1] = sinf(0.5f*angle)*rot_axis[0];
  q[2] = sinf(0.5f*angle)*rot_axis[1];
  q[3] = sinf(0.5f*angle)*rot_axis[2];
}


/* Compute a uniformly distributed random rotation axis, i.e., a vector of
   length 1 that is uniformly distributed on the unit sphere S². */
static void gen_random_rot_axis(float rot_axis[3])
{
  float p, t;

  t = (float)frand(2.0f*M_PI_F);
  p = acosf((float)frand(2.0f)-1.0f);
  rot_axis[0] = sinf(p)*cosf(t);
  rot_axis[1] = sinf(p)*sinf(t);
  rot_axis[2] = cosf(p);
}


/* Set the animation quaternions based on the animation time t. */
static void set_animation_quats(ModeInfo *mi, float t)
{
  hopffibrationstruct *hf = &hopffibration[MI_SCREEN(mi)];
  float te;

  if (hf->anim_rotate_rnd)
  {
    te = 2.0f*M_PI_F*ease(t,hf->anim_easing_fct_rot_rnd);
    axis_angle_to_quat(hf->rot_axis_base,te,hf->quat_base);
  }
  if (hf->anim_rotate_space)
  {
    te = ease(t,hf->anim_easing_fct_rot_space);
    te = (1.0f-te)*hf->angle_space_start+te*hf->angle_space_end;
    axis_angle_to_quat(hf->rot_axis_space,te,hf->quat_space);
  }
}


/* Set the geometry parameters for the animation object i based on the
   animation time t. */
static void set_animation_geometry(ModeInfo *mi, float t, int i)
{
  hopffibrationstruct *hf = &hopffibration[MI_SCREEN(mi)];
  float te, angle;

  /* Sanity check. */
  if (i >= MAX_ANIM_GEOM)
    return;

  hf->anim_geom[i].num = hf->anim->anim_so[i].num;
  te = ease(t,hf->anim->anim_so[i].easing_function_p);
  hf->anim_geom[i].p = ((1.0f-te)*hf->anim->anim_so[i].p_start+
                        te*hf->anim->anim_so[i].p_end);
  te = ease(t,hf->anim->anim_so[i].easing_function_q);
  hf->anim_geom[i].q = ((1.0f-te)*hf->anim->anim_so[i].q_start+
                        te*hf->anim->anim_so[i].q_end);
  te = ease(t,hf->anim->anim_so[i].easing_function_r);
  hf->anim_geom[i].r = ((1.0f-te)*hf->anim->anim_so[i].r_start+
                        te*hf->anim->anim_so[i].r_end);
  te = ease(t,hf->anim->anim_so[i].easing_function_offset);
  hf->anim_geom[i].offset = ((1.0f-te)*hf->anim->anim_so[i].offset_start+
                             te*hf->anim->anim_so[i].offset_end);
  te = ease(t,hf->anim->anim_so[i].easing_function_sector);
  hf->anim_geom[i].sector = ((1.0f-te)*hf->anim->anim_so[i].sector_start+
                             te*hf->anim->anim_so[i].sector_end);
  if (hf->anim_geom[i].rotate)
  {
    te = ease(t,hf->anim->anim_so[i].easing_function_rotate);
    angle = ((1.0f-te)*hf->anim->anim_so[i].angle_start+
             te*hf->anim->anim_so[i].angle_end);
    axis_angle_to_quat(hf->anim->anim_so[i].rot_axis_base,angle,
                       hf->anim_geom[i].quat_base);
  }
}


/* Select the next animation phase. */
static void init_next_anim_phase(ModeInfo *mi, int phase)
{
  hopffibrationstruct *hf = &hopffibration[MI_SCREEN(mi)];
  int i;
  float nrm;

  hf->anim = hf->anim_phases->anim_mo[phase];
  hf->anim_step = 0;

  for (i=0; i<hf->anim->num; i++)
  {
    /* Sanity check. */
    if (i >= MAX_ANIM_GEOM)
      break;

    hf->anim_geom[i].generator = hf->anim->anim_so[i].generator;

    hf->anim_geom[i].n = hf->anim->anim_so[i].n;

    nrm = norm(hf->anim->anim_so[i].rot_axis_base);
    hf->anim_geom[i].rotate = (nrm > 0.0f);
    hf->anim_geom[i].quat_base[0] = 1.0f;
    hf->anim_geom[i].quat_base[1] = 0.0f;
    hf->anim_geom[i].quat_base[2] = 0.0f;
    hf->anim_geom[i].quat_base[3] = 0.0f;
  }

  if (hf->anim->rotate_prob == 0.0f)
    hf->anim_rotate_rnd = False;
  else if (hf->anim->rotate_prob == 1.0f)
    hf->anim_rotate_rnd = True;
  else
    hf->anim_rotate_rnd = (frand(1.0f) < hf->anim->rotate_prob);
  hf->anim_easing_fct_rot_rnd = hf->anim->easing_function_rot_rnd;

  gen_random_rot_axis(hf->rot_axis_base);

  hf->anim_easing_fct_rot_space = hf->anim->easing_function_rot_space;
  hf->rot_axis_space[0] = hf->anim->rot_axis_space[0];
  hf->rot_axis_space[1] = hf->anim->rot_axis_space[1];
  hf->rot_axis_space[2] = hf->anim->rot_axis_space[2];
  nrm = norm(hf->rot_axis_space);
  hf->anim_rotate_space = (nrm > 0.0f);
  hf->angle_space_start = hf->anim->angle_start;
  hf->angle_space_end = hf->anim->angle_end;

  set_animation_quats(mi,0.0f);
  for (i=0; i<hf->anim->num; i++)
    set_animation_geometry(mi,0.0f,i);
}


/* Select the next animation. */
static void set_next_anim(ModeInfo *mi)
{
  hopffibrationstruct *hf = &hopffibration[MI_SCREEN(mi)];
  int anim_state_next, idx_anim, num_anim;
  animations *anims_next;

  if (hf->anim_remain_in_state <= 0)
    anim_state_next = (int)floor(frand(NUM_ANIM_STATES));
  else
    anim_state_next = hf->anim_state;

  if (anim_state_next == hf->anim_state)
  {
    hf->anim_remain_in_state--;
  }
  else
  {
    /* Set the number of animations to execute in the next animation state
       based on the number of available animations in that state. */
    hf->anim_remain_in_state =
      (hopf_animations[anim_state_next][anim_state_next]->num_anim+2)/3;
  }

  anims_next = hopf_animations[hf->anim_state][anim_state_next];
  hf->anim_state = anim_state_next;

  num_anim = anims_next->num_anim;

  if (num_anim > 1)
  {
    /* If there are animations to choose from, avoid using the same
       animation twice in succession. */
    do
      idx_anim = (int)floor(frand(num_anim));
    while (hf->anim_phases == anims_next->anim[idx_anim]);
  }
  else
  {
    /* Select the only available choice. */
    idx_anim = 0;
  }

  hf->anim_phases = anims_next->anim[idx_anim];
  hf->anim_phase_num = hf->anim_phases->num_phases;
  hf->anim_phase = 0;

  init_next_anim_phase(mi,hf->anim_phase);
}


/* Display the Hopf fibration. */
static void display_hopffibration(ModeInfo *mi)
{
  hopffibrationstruct *hf = &hopffibration[MI_SCREEN(mi)];
  float t;
  int i;

  if (!hf->button_pressed)
  {
    hf->anim_step++;
    t = (float)hf->anim_step/(float)hf->anim->num_steps;

    set_animation_quats(mi,t);
    for (i=0; i<hf->anim->num; i++)
      set_animation_geometry(mi,t,i);
  
    if (hf->anim_step >= hf->anim->num_steps)
    {
      hf->anim_step = 0;
      if (hf->anim_phase < hf->anim_phase_num-1)
      {
        hf->anim_phase++;
        init_next_anim_phase(mi,hf->anim_phase);
      }
      else
      {
        set_next_anim(mi);
      }
    }
  }

  gltrackball_rotate(hf->trackball);

  hf->num_base_points = 0;

  for (i=0; i<hf->anim->num; i++)
  {
    if (hf->anim_geom[i].generator == GEN_TORUS)
      gen_hopf_torus_base(mi,hf->anim_geom[i].p,hf->anim_geom[i].q,
                          hf->anim_geom[i].r,hf->anim_geom[i].n,
                          hf->anim_geom[i].offset,hf->anim_geom[i].sector,
                          hf->anim_geom[i].num,hf->anim_geom[i].rotate,
                          hf->anim_geom[i].quat_base);
    else /* hf->anim_geom[i].generator == GEN_SPIRAL */
      gen_hopf_spiral_base(mi,hf->anim_geom[i].p,hf->anim_geom[i].q,
                           hf->anim_geom[i].r,hf->anim_geom[i].offset,
                           hf->anim_geom[i].sector,hf->anim_geom[i].num,
                           hf->anim_geom[i].rotate,hf->anim_geom[i].quat_base);
  }

#ifdef HAVE_GLSL
  if (hf->use_shaders)
    mi->polygon_count = draw_hopf_circles_pf(mi);
  else
#endif /* HAVE_GLSL */
    mi->polygon_count = draw_hopf_circles_ff(mi);
}


#ifdef HAVE_GLSL

/* Compute the next highest power of 2 of v. */
static unsigned int next_pow2(unsigned int v)
{
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;
  v += (v == 0);
  return v;
}


/* Delete the storage for the MSAA framebuffer object. */
static void delete_msaa_fbo(ModeInfo *mi)
{
  hopffibrationstruct *hf = &hopffibration[MI_SCREEN(mi)];

  if (hf->use_msaa_fbo)
  {
    glBindFramebuffer(GL_FRAMEBUFFER,hf->msaa_fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER,0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER,0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,hf->default_draw_fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,hf->default_read_fbo);
    glDeleteRenderbuffers(1,&hf->msaa_rb_color);
    glDeleteRenderbuffers(1,&hf->msaa_rb_depth);
    glDeleteFramebuffers(1,&hf->msaa_fbo);
  }
}


/* Allocate the storage for the MSAA framebuffer object. */
static void init_msaa_fbo(ModeInfo *mi)
{
  hopffibrationstruct *hf = &hopffibration[MI_SCREEN(mi)];
  GLenum status;

  if (hf->use_msaa_fbo)
  {
    glGenFramebuffers(1,&hf->msaa_fbo);

    glGenRenderbuffers(1,&hf->msaa_rb_color);
    glGenRenderbuffers(1,&hf->msaa_rb_depth);

    glBindRenderbuffer(GL_RENDERBUFFER,hf->msaa_rb_color);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER,hf->msaa_samples,GL_RGBA8,
                                     hf->WindW,hf->WindH);

    glBindRenderbuffer(GL_RENDERBUFFER,hf->msaa_rb_depth);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER,hf->msaa_samples,
                                     GL_DEPTH_COMPONENT24,
                                     hf->WindW,hf->WindH);
    glBindRenderbuffer(GL_RENDERBUFFER,0);

    glBindFramebuffer(GL_FRAMEBUFFER,hf->msaa_fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER,hf->msaa_rb_color);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER,hf->msaa_rb_depth);

    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
      delete_msaa_fbo(mi);
      hf->use_msaa_fbo = False;
    }

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,hf->default_draw_fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,hf->default_read_fbo);
  }
}


/* Delete the storage for the shadow map framebuffer object. */
static void delete_shadow_fbo(ModeInfo *mi)
{
  hopffibrationstruct *hf = &hopffibration[MI_SCREEN(mi)];

  if (hf->use_shadow_fbo)
  {
    glBindFramebuffer(GL_FRAMEBUFFER,hf->shadow_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,
                           0,0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,hf->default_draw_fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,hf->default_read_fbo);
    glDeleteTextures(1,&hf->shadow_tex);
    if (hf->shadow_fbo_req_col_tex)
      glDeleteTextures(1,&hf->shadow_col_tex);
    glDeleteFramebuffers(1,&hf->shadow_fbo);
  }
}


/* Allocate the storage for the shadow map framebuffer object. */
static void init_shadow_fbo(ModeInfo *mi)
{
  hopffibrationstruct *hf = &hopffibration[MI_SCREEN(mi)];
  GLenum status;
  GLenum draw_buffer = GL_NONE;

  if (hf->use_shadow_fbo)
  {
    glGenFramebuffers(1,&hf->shadow_fbo);

    glGenTextures(1,&hf->shadow_tex);

    hf->shadow_tex_size = MIN(next_pow2(hf->WindW),next_pow2(hf->WindH));
    hf->shadow_tex_size = MIN(hf->shadow_tex_size,hf->max_tex_size);
    glBindTexture(GL_TEXTURE_2D,hf->shadow_tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_MODE,
                    GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_FUNC,GL_LEQUAL);
    glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT32F,hf->shadow_tex_size,
                 hf->shadow_tex_size,0,GL_DEPTH_COMPONENT,GL_FLOAT,NULL);
    glBindTexture(GL_TEXTURE_2D,0);

    glBindFramebuffer(GL_FRAMEBUFFER,hf->shadow_fbo);
    glDrawBuffers(1,&draw_buffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,
                           hf->shadow_tex,0);

    if (hf->shadow_fbo_req_col_tex)
    {
      draw_buffer = GL_COLOR_ATTACHMENT0;

      glGenTextures(1,&hf->shadow_col_tex);

      glBindTexture(GL_TEXTURE_2D,hf->shadow_col_tex);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);

      glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,hf->shadow_tex_size,
                   hf->shadow_tex_size,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
      glBindTexture(GL_TEXTURE_2D,0);

      glDrawBuffers(1,&draw_buffer);
      glBindFramebuffer(GL_FRAMEBUFFER,hf->shadow_fbo);
      glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,
                             hf->shadow_col_tex,0);
    }
    else
    {
      hf->shadow_col_tex = 0;
    }

    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
      delete_shadow_fbo(mi);
      hf->use_shadow_fbo = False;
    }

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,hf->default_draw_fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,hf->default_read_fbo);
  }
}


/* Initialize the programmable OpenGL functionality. */
static void init_glsl(ModeInfo *mi)
{
  hopffibrationstruct *hf = &hopffibration[MI_SCREEN(mi)];
  GLint gl_major, gl_minor, glsl_major, glsl_minor;
  GLboolean gl_gles3;
  const GLchar *vertex_shader_source[2];
  const GLchar *fragment_shader_source[2];
  GLint samples, sample_buffers;
  const char *gl_ext;
  static float one = 1.0f;

  hf->use_shaders = False;
  hf->use_msaa_fbo = False;
  hf->use_shadow_fbo = False;
  hf->shader_program = 0;
  hf->shadow_program = 0;
  hf->use_vao = False;

  if (!glsl_GetGlAndGlslVersions(&gl_major,&gl_minor,&glsl_major,&glsl_minor,
                                 &gl_gles3))
    return;

  vertex_shader_source[0] = glsl_GetGLSLVersionString();
  vertex_shader_source[1] = vertex_shader;
  fragment_shader_source[0] = glsl_GetGLSLVersionString();
  fragment_shader_source[1] = fragment_shader;
  if (!glsl_CompileAndLinkShaders(2,vertex_shader_source,
                                  2,fragment_shader_source,
                                  &hf->shader_program))
    return;

  hf->pos_index = glGetAttribLocation(hf->shader_program,
                                      "VertexPosition");
  hf->normal_index = glGetAttribLocation(hf->shader_program,
                                         "VertexNormal");
  hf->color_index = glGetAttribLocation(hf->shader_program,
                                        "VertexColor");
  hf->mv_index = glGetUniformLocation(hf->shader_program,
                                      "MatModelView");
  hf->proj_index = glGetUniformLocation(hf->shader_program,
                                        "MatProj");
  hf->lmvp_index = glGetUniformLocation(hf->shader_program,
                                        "MatLightMVP");
  hf->glbl_ambient_index = glGetUniformLocation(hf->shader_program,
                                                "LtGlblAmbient");
  hf->lt_ambient_index = glGetUniformLocation(hf->shader_program,
                                              "LtAmbient");
  hf->lt_diffuse_index = glGetUniformLocation(hf->shader_program,
                                              "LtDiffuse");
  hf->lt_specular_index = glGetUniformLocation(hf->shader_program,
                                               "LtSpecular");
  hf->lt_direction_index = glGetUniformLocation(hf->shader_program,
                                                "LtDirection");
  hf->lt_halfvect_index = glGetUniformLocation(hf->shader_program,
                                               "LtHalfVector");
  hf->ambient_index = glGetUniformLocation(hf->shader_program,
                                           "MatAmbient");
  hf->diffuse_index = glGetUniformLocation(hf->shader_program,
                                           "MatDiffuse");
  hf->specular_index = glGetUniformLocation(hf->shader_program,
                                            "MatSpecular");
  hf->shininess_index = glGetUniformLocation(hf->shader_program,
                                             "MatShininess");
  hf->use_shadows_index = glGetUniformLocation(hf->shader_program,
                                               "UseShadows");
  hf->shadow_smpl_index = glGetUniformLocation(hf->shader_program,
                                               "ShadowTex");
  hf->shadow_pix_size_index = glGetUniformLocation(hf->shader_program,
                                                   "ShadowPixSize");
  if (hf->pos_index == -1 ||
      hf->normal_index == -1 ||
      hf->color_index == -1 ||
      hf->mv_index == -1 ||
      hf->proj_index == -1 ||
      hf->lmvp_index == -1 ||
      hf->glbl_ambient_index == -1 ||
      hf->lt_ambient_index == -1 ||
      hf->lt_diffuse_index == -1 ||
      hf->lt_specular_index == -1 ||
      hf->lt_direction_index == -1 ||
      hf->lt_halfvect_index == -1 ||
      hf->ambient_index == -1 ||
      hf->diffuse_index == -1 ||
      hf->specular_index == -1 ||
      hf->shininess_index == -1 ||
      hf->use_shadows_index == -1 ||
      hf->shadow_smpl_index == -1 ||
      hf->shadow_pix_size_index == -1)
  {
    glDeleteProgram(hf->shader_program);
    hf->shader_program = 0;
    return;
  }

  vertex_shader_source[0] = glsl_GetGLSLVersionString();
  vertex_shader_source[1] = shadow_vertex_shader;
  fragment_shader_source[0] = glsl_GetGLSLVersionString();
  fragment_shader_source[1] = shadow_fragment_shader;
  if (!glsl_CompileAndLinkShaders(2,vertex_shader_source,
                                  2,fragment_shader_source,
                                  &hf->shadow_program))
  {
    glDeleteProgram(hf->shader_program);
    hf->shader_program = 0;
    return;
  }

  hf->use_shadow_fbo = True;
  hf->shadow_pos_index = glGetAttribLocation(hf->shadow_program,
                                             "VertexPosition");
  hf->shadow_lmvp_index = glGetUniformLocation(hf->shadow_program,
                                               "MatLightMVP");
  if (hf->shadow_pos_index == -1 ||
      hf->shadow_lmvp_index == -1)
  {
    glDeleteProgram(hf->shader_program);
    glDeleteProgram(hf->shadow_program);
    hf->shader_program = 0;
    hf->shadow_program = 0;
    hf->use_shadow_fbo = False;
  }

  hf->fiber_pos_buffer = calloc(hf->max_base_points,
                                sizeof(*hf->fiber_pos_buffer));
  hf->fiber_normal_buffer = calloc(hf->max_base_points,
                                   sizeof(*hf->fiber_normal_buffer));
  hf->fiber_indices_buffer = calloc(hf->max_base_points,
                                    sizeof(*hf->fiber_indices_buffer));
  hf->fiber_num_tri = calloc(hf->max_base_points,sizeof(*hf->fiber_num_tri));
  if (hf->fiber_pos_buffer == NULL || hf->fiber_normal_buffer == NULL ||
      hf->fiber_indices_buffer == NULL || hf->fiber_num_tri == NULL)
    abort();
  glGenBuffers(hf->max_base_points,hf->fiber_pos_buffer);
  glGenBuffers(hf->max_base_points,hf->fiber_normal_buffer);
  glGenBuffers(hf->max_base_points,hf->fiber_indices_buffer);
  glGenBuffers(1,&hf->sphere_base_pos_buffer);
  glGenBuffers(1,&hf->sphere_base_normal_buffer);
  glGenBuffers(1,&hf->sphere_base_indices_buffer);
  glGenBuffers(1,&hf->base_point_pos_buffer);
  glGenBuffers(1,&hf->base_point_normal_buffer);
  glGenBuffers(1,&hf->base_point_indices_buffer);

  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,(GLint *)&hf->default_draw_fbo);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,(GLint *)&hf->default_read_fbo);

  glGetIntegerv(GL_SAMPLES,&samples);
  glGetIntegerv(GL_SAMPLE_BUFFERS,&sample_buffers);
  hf->use_msaa_fbo = (hf->anti_aliasing &&
                      (sample_buffers == 0 || samples == 0));
  if (!gl_gles3 && gl_major < 3 && hf->use_msaa_fbo)
  {
    gl_ext = (const char *)glGetString(GL_EXTENSIONS);
    if (gl_ext == NULL)
      hf->use_msaa_fbo = False;
    else
      hf->use_msaa_fbo =
        (strstr(gl_ext,"GL_EXT_framebuffer_object") != NULL &&
         strstr(gl_ext,"GL_EXT_framebuffer_blit") != NULL &&
         strstr(gl_ext,"GL_EXT_framebuffer_multisample") != NULL);
  }

  if (hf->use_msaa_fbo)
  {
    glEnable(GL_MULTISAMPLE);
    glGetIntegerv(GL_MAX_SAMPLES,&hf->msaa_samples);
    hf->msaa_samples = MIN(hf->msaa_samples,MSAA_SAMPLES);
    init_msaa_fbo(mi);
  }

  hf->shadow_fbo_req_col_tex = (!gl_gles3 && gl_major < 3);
  if (!gl_gles3 && gl_major < 3 && hf->use_shadow_fbo)
  {
    gl_ext = (const char *)glGetString(GL_EXTENSIONS);
    if (gl_ext == NULL)
      hf->use_shadow_fbo = GL_FALSE;
    else
      hf->use_shadow_fbo =
        (strstr(gl_ext,"GL_ARB_shadow") != NULL &&
         strstr(gl_ext,"GL_ARB_depth_buffer_float") != NULL);
  }

  if (hf->use_shadow_fbo)
  {
    glGetIntegerv(GL_MAX_TEXTURE_SIZE,&hf->max_tex_size);
    init_shadow_fbo(mi);
  }

  glGenTextures(1,&hf->shadow_dummy_tex);
  glBindTexture(GL_TEXTURE_2D,hf->shadow_dummy_tex);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_MODE,
                  GL_COMPARE_REF_TO_TEXTURE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_FUNC,GL_LEQUAL);
  glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT32F,1,1,0,
               GL_DEPTH_COMPONENT,GL_FLOAT,&one);
  glBindTexture(GL_TEXTURE_2D,0);

  hf->vert = calloc(3*MAX_CIRCLE_PNT*hf->num_tube,sizeof(*hf->vert));
  hf->norm = calloc(3*MAX_CIRCLE_PNT*hf->num_tube,sizeof(*hf->norm));
  hf->tri = calloc(2*3*MAX_CIRCLE_PNT*hf->num_tube,sizeof(*hf->tri));

  hf->use_vao = glsl_IsCoreProfile();
  if (hf->use_vao)
    glGenVertexArrays(1,&hf->vertex_array_object);

  hf->use_shaders = True;
}

#endif /* HAVE_GLSL */


static int get_max_base_points(void)
{
  int i, j, k, l, m, n, max_base_points;
  animations *anims;
  animation_phases *anim_ps;
  animation_multi_obj *anim_mo;

  max_base_points = 0;
  for (i=0; i<NUM_ANIM_STATES; i++)
  {
    for (j=0; j<NUM_ANIM_STATES; j++)
    {
      anims = hopf_animations[i][j];
      for (k=0; k<anims->num_anim; k++)
      {
        anim_ps = anims->anim[k];
        for (l=0; l<anim_ps->num_phases; l++)
        {
          anim_mo = anim_ps->anim_mo[l];
          n = 0;
          for (m=0; m<anim_mo->num; m++)
            n += anim_mo->anim_so[m].num;
          if (n > max_base_points)
            max_base_points = n;
        }
      }
    }
  }
  return max_base_points;
}


static void init(ModeInfo *mi)
{
  hopffibrationstruct *hf = &hopffibration[MI_SCREEN(mi)];

  if (hf->details == DISP_DETAILS_COARSE)
  {
    hf->num_tube = NUM_TUBE_COARSE;
    hf->base_sphere_s = BASE_SPHERE_S_COARSE;
    hf->base_point_s = BASE_POINT_S_COARSE;
    hf->max_circle_dist = MAX_CIRCLE_DIST_COARSE;
  }
  else if (hf->details == DISP_DETAILS_FINE)
  {
    hf->num_tube = NUM_TUBE_FINE;
    hf->base_sphere_s = BASE_SPHERE_S_FINE;
    hf->base_point_s = BASE_POINT_S_FINE;
    hf->max_circle_dist = MAX_CIRCLE_DIST_FINE;
  }
  else /* hf->details == DISP_DETAILS_MEDIUM) */
  {
    hf->num_tube = NUM_TUBE_MEDIUM;
    hf->base_sphere_s = BASE_SPHERE_S_MEDIUM;
    hf->base_point_s = BASE_POINT_S_MEDIUM;
    hf->max_circle_dist = MAX_CIRCLE_DIST_MEDIUM;
  }

  hf->alpha = 290.0f;
  hf->beta = 0.0f;
  hf->delta = 270.0f;

  hf->zeta = 0.0f;
  hf->eta = 0.0f;
  hf->theta = 0.0f;

  hf->rot_axis_base[0] = 1.0f;
  hf->rot_axis_base[1] = 0.0f;
  hf->rot_axis_base[2] = 0.0f;
  hf->quat_base[0] = 1.0f;
  hf->quat_base[1] = 0.0f;
  hf->quat_base[2] = 0.0f;
  hf->quat_base[3] = 0.0f;


  hf->rot_axis_space[0] = 1.0f;
  hf->rot_axis_space[1] = 0.0f;
  hf->rot_axis_space[2] = 0.0f;
  hf->quat_space[0] = 1.0f;
  hf->quat_space[1] = 0.0f;
  hf->quat_space[2] = 0.0f;
  hf->quat_space[3] = 0.0f;

  hf->angle_space_start = 0.0f;
  hf->angle_space_end = 2.0f*M_PI_F;

  hf->anim = NULL;
  hf->anim_state = (int)floor(frand(NUM_ANIM_STATES));
  hf->anim_remain_in_state = 1;
  hf->anim_step = 0;
  hf->anim_phases = NULL;
  hf->anim_phase_num = 0;
  hf->anim_phase = 0;
  hf->anim_rotate_rnd = False;
  hf->anim_rotate_space = False;
  hf->anim_easing_fct_rot_rnd = EASING_NONE;
  hf->anim_easing_fct_rot_space = EASING_NONE;
  set_next_anim(mi);

  hf->max_base_points = get_max_base_points();
  hf->base_points = calloc(hf->max_base_points,sizeof(*hf->base_points));
  if (hf->base_points == NULL)
    abort();
  hf->num_base_points = 0;

  hf->sphere_base = gen_icosphere_data(&icosa,hf->base_sphere_s,
                                       BASE_SPHERE_RADIUS);
  hf->sphere_base_point = gen_icosphere_data(&icosa,hf->base_point_s,
                                             BASE_POINT_RADIUS);
  if (hf->sphere_base == NULL || hf->sphere_base_point == NULL)
    abort();

#ifdef HAVE_GLSL
  init_glsl(mi);
#endif /* HAVE_GLSL */
}


ENTRYPOINT void reshape_hopffibration(ModeInfo *mi, int width, int height)
{
  hopffibrationstruct *hf = &hopffibration[MI_SCREEN(mi)];

  hf->WindW = (GLint)width;
  hf->WindH = (GLint)height;
  hf->aspect = (GLfloat)width/(GLfloat)height;
#ifdef HAVE_GLSL
  delete_msaa_fbo(mi);
  init_msaa_fbo(mi);
  delete_shadow_fbo(mi);
  init_shadow_fbo(mi);
#endif /* HAVE_GLSL */
}


ENTRYPOINT Bool hopffibration_handle_event(ModeInfo *mi, XEvent *event)
{
  hopffibrationstruct *hf = &hopffibration[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button == Button1)
  {
    hf->button_pressed = True;
    gltrackball_start(hf->trackball,event->xbutton.x,event->xbutton.y,
                      MI_WIDTH(mi),MI_HEIGHT(mi));
    return True;
  }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
  {
    hf->button_pressed = False;
    gltrackball_stop(hf->trackball);
    return True;
  }
  else if (event->xany.type == MotionNotify && hf->button_pressed)
  {
    gltrackball_track(hf->trackball,event->xmotion.x,event->xmotion.y,
                      MI_WIDTH(mi),MI_HEIGHT(mi));
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
 *    Initialize hopffibration.  Called each time the window changes.
 *-----------------------------------------------------------------------------
 */

ENTRYPOINT void init_hopffibration(ModeInfo *mi)
{
  hopffibrationstruct *hf;

  MI_INIT(mi,hopffibration);
  hf = &hopffibration[MI_SCREEN(mi)];

  hf->shadows = shadows;

  hf->base_space = base_space;

  hf->anti_aliasing = anti_aliasing;

  /* Set the projection mode. */
  if (!strcasecmp(proj,"perspective"))
  {
    hf->projection = DISP_PERSPECTIVE;
  }
  else if (!strcasecmp(proj,"orthographic"))
  {
    hf->projection = DISP_ORTHOGRAPHIC;
  }
  else
  {
    hf->projection = DISP_PERSPECTIVE;
  }

  /* Set the detail level. */
  if (!strcasecmp(det,"coarse"))
  {
    hf->details = DISP_DETAILS_COARSE;
  }
  else if (!strcasecmp(det,"fine"))
  {
    hf->details = DISP_DETAILS_FINE;
  }
  else
  {
    hf->details = DISP_DETAILS_MEDIUM;
  }

  hf->trackball = gltrackball_init(False);
  hf->button_pressed = False;

  if ((hf->glx_context = init_GL(mi)) != NULL)
  {
    reshape_hopffibration(mi,MI_WIDTH(mi),MI_HEIGHT(mi));
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
ENTRYPOINT void draw_hopffibration(ModeInfo *mi)
{
  Display *display = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  hopffibrationstruct *hf;

  if (hopffibration == NULL)
    return;
  hf = &hopffibration[MI_SCREEN(mi)];

  MI_IS_DRAWN(mi) = True;
  if (!hf->glx_context)
    return;

  glXMakeCurrent(display,window,*hf->glx_context);

  display_hopffibration(mi);

  if (MI_IS_FPS(mi))
    do_fps (mi);

  glFlush();

  glXSwapBuffers(display,window);
}


#ifndef STANDALONE
ENTRYPOINT void change_hopffibration(ModeInfo *mi)
{
  hopffibrationstruct *hf = &hopffibration[MI_SCREEN(mi)];

  if (!hf->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi),MI_WINDOW(mi),*hf->glx_context);
  init(mi);
}
#endif /* !STANDALONE */


ENTRYPOINT void free_hopffibration(ModeInfo *mi)
{
  hopffibrationstruct *hf = &hopffibration[MI_SCREEN(mi)];

  if (!hf->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi),MI_WINDOW(mi),*hf->glx_context);

  gltrackball_free(hf->trackball);

  free(hf->base_points);
  free_icosphere_data(hf->sphere_base_point);
  free_icosphere_data(hf->sphere_base);

#ifdef HAVE_GLSL
  if (hf->use_shaders)
  {
    glUseProgram(0);
    if (hf->shader_program != 0)
      glDeleteProgram(hf->shader_program);
    if (hf->shadow_program != 0)
      glDeleteProgram(hf->shadow_program);
    glDeleteBuffers(1,&hf->base_point_pos_buffer);
    glDeleteBuffers(1,&hf->base_point_normal_buffer);
    glDeleteBuffers(1,&hf->base_point_indices_buffer);
    glDeleteBuffers(1,&hf->sphere_base_pos_buffer);
    glDeleteBuffers(1,&hf->sphere_base_normal_buffer);
    glDeleteBuffers(1,&hf->sphere_base_indices_buffer);
    glDeleteBuffers(hf->max_base_points,hf->fiber_pos_buffer);
    glDeleteBuffers(hf->max_base_points,hf->fiber_normal_buffer);
    glDeleteBuffers(hf->max_base_points,hf->fiber_indices_buffer);
    glDeleteTextures(1,&hf->shadow_dummy_tex);
    if (hf->use_vao)
      glDeleteVertexArrays(1,&hf->vertex_array_object);
    delete_msaa_fbo(mi);
    delete_shadow_fbo(mi);
    free(hf->fiber_pos_buffer);
    free(hf->fiber_normal_buffer);
    free(hf->fiber_indices_buffer);
    free(hf->fiber_num_tri);
    free(hf->vert);
    free(hf->norm);
    free(hf->tri);
  }
#endif /* HAVE_GLSL */
}


XSCREENSAVER_MODULE ("HopfFibration", hopffibration)

#endif /* USE_GL */
