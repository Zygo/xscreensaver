/* cubocteversion --- Turns a cuboctahedron inside out: a smooth
   deformation (homotopy). During the eversion, the deformed cuboctahedron
   is allowed to intersect itself transversally, however, no fold edges or
   non-injective neighborhoods of vertices occur. */

#if 0
static const char sccsid[] = "@(#)cubocteversion.c  1.1 23/03/02 xlockmore";
#endif

/* Copyright (c) 2023-2025 Carsten Steger <carsten@mirsanmir.org>. */

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
 * C. Steger - 23/03/06: Initial version
 */

/*
 * This program shows a cuboctahedron eversion, i.e., a smooth
 * deformation (homotopy) that turns a cuboctahedron inside out.
 * During the eversion, the deformed cuboctahedron is allowed to
 * intersect itself transversally.  However, no fold edges or
 * non-injective neighborhoods of vertices are allowed to occur.
 *
 * The cuboctahedron can be deformed with two eversion methods:
 * Morin-Denner or Apéry.
 *
 * The deformed cuboctahedron can be projected to the screen either
 * perspectively or orthographically.
 *
 * There are three display modes for the cuboctahedron: solid,
 * transparent, or random.  If random mode is selected, the mode is
 * changed each time an eversion has been completed.
 *
 * The edges of the faces of the cuboctahedron can be visualized in
 * three modes: without edge tubes, with edge tubes, or random.  If
 * edge tubes are selected, solid gray tubes are displayed around the
 * edges of the cuboctahedron.  This makes them more prominent.  If
 * random mode is selected, the mode is changed each time an eversion
 * has been completed.
 *
 * During the eversion, the cuboctahedron must intersect itself.  It
 * can be selected how these self-intersections are displayed: without
 * self-intersection tubes, with self-intersection tubes, or random.
 * If self-intersection tubes are selected, solid orange tubes are
 * displayed around the self-intersections of the cuboctahedron.  This
 * makes them more prominent.  If random mode is selected, the mode is
 * changed each time an eversion has been completed.
 *
 * The colors with with the cuboctahedron is drawn can be set to
 * two-sided, face, earth, or random.  In two-sided mode, the
 * cuboctahedron is drawn with magenta on one side and cyan on the
 * other side.  In face mode, the cuboctahedron is displayed with
 * different colors for each face.  The colors of the faces are
 * identical on the inside and outside of the cuboctahedron.  Colors
 * on the northern hemi-cuboctahedron are brighter than those on the
 * southern hemi-cuboctahedron.  In earth mode, the cuboctahedron is
 * drawn with a texture of earth by day on one side and with a texture
 * of earth by night on the other side.  Initially, the earth by day
 * is on the outside and the earth by night on the inside.  After the
 * first eversion, the earth by night will be on the outside.  All
 * points of the earth on the inside and outside are at the same
 * positions on the cuboctahedron.  Since an eversion transforms the
 * cuboctahedron into its inverse, the earth by night will appear with
 * all continents mirror reversed.  If random mode is selected, the
 * color scheme is changed each time an eversion has been completed.
 *
 * It is possible to rotate the cuboctahedron while it is deforming.
 * The rotation speed for each of the three coordinate axes around
 * which the cuboctahedron rotates can be chosen arbitrarily.
 */

#define DEF_EVERSION_METHOD    "random"
#define DEF_DISPLAY_MODE       "random"
#define DEF_EDGES              "random"
#define DEF_SELF_INTERSECTIONS "random"
#define DEF_COLORS             "random"
#define DEF_PROJECTION         "random"
#define DEF_SPEEDX             "0.0"
#define DEF_SPEEDY             "0.0"
#define DEF_SPEEDZ             "0.0"
#define DEF_DEFORM_SPEED       "20.0"
#define DEF_TRANSPARENCY       "correct"


#ifdef STANDALONE
# define DEFAULTS           "*delay:       20000 \n" \
                            "*showFPS:     False \n" \
                            "*prefersGLSL: True \n" \

# define release_cubocteversion 0
# include "xlockmore.h"         /* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"             /* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL

#ifndef TEXTURE_MAX_ANISOTROPY_EXT
#define TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif
#ifndef MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

#define M_PI_F    3.14159265359f
#define M_SQRT2_F 1.41421356237f
#define M_SQRT3_F 1.73205080757f
#define M_SQRT6_F 2.44948974278f

#define EVERSION_MORIN_DENNER      0
#define EVERSION_APERY             1
#define NUM_EVERSIONS              2

#define DISP_SURFACE               0
#define DISP_TRANSPARENT           1
#define NUM_DISPLAY_MODES          2

#define COLORS_TWOSIDED            0
#define COLORS_FACE                1
#define COLORS_EARTH               2
#define NUM_COLORS                 3

#define DISP_PERSPECTIVE           0
#define DISP_ORTHOGRAPHIC          1
#define NUM_DISP_MODES             2

#define TRANSPARENCY_CORRECT       0
#define TRANSPARENCY_APPROXIMATE   1
#define TRANSPARENCY_STANDARD      2

/* Animation states */
#define ANIM_DEFORM  0
#define ANIM_SWIPE_D 1
#define ANIM_SWIPE_U 2

/* Number of steps to swipe the cuboctahedron in and out */
#define NUM_SWIPE 100

/* Number of polyhedral models in the Apery and Morin-Denner eversions */
#define NUM_MODELS_APERY  7
#define NUM_MODELS_DENNER 45

/* Number of vertices, edges, and faces in all of the polyhedral models */
#define NUM_VERT 12
#define NUM_EDGE 30
#define NUM_FACE 20

/* Number of subdivisions for spheres (NUMU,NUMV) and cylinders (NUMV) */
#define NUMU 16
#define NUMV 16

/* Radii of the edge and self-intersection tubes */
#define RADIUS_TUBE  0.020f
#define RADIUS_SELF  0.019f

/* Minimum and maximum FOV angle for perspective projection */
#define MIN_FOV_ANGLE 20.0f
#define MAX_FOV_ANGLE 60.0f

/* Minimum and maximum FOV for orthographic projection */
#define MIN_FOV_ORTHO 2.0f
#define MAX_FOV_ORTHO 6.0f

/* Minimum and maximum opacity of transparent faces */
#define MIN_OPACITY 0.3f
#define MAX_OPACITY 0.7f

/* Minimum and maximum time and time step for the deformation */
#define TIME_MIN -56.0f
#define TIME_MAX  56.0f
#define DTIME     0.125f

/* Maximum depth for the dual depth peeling algorithm */
#define MAX_DEPTH  1.0f

/* Maximum number of passes of the dual depth peeling algorithm */
#define MAX_PASSES 10


#include "glsl-utils.h"
#include "gltrackball.h"


typedef struct {
  /* The OpenGL context */
  GLXContext *glx_context;
  /* The width and height of the current window */
  GLint WindH, WindW;
  /* Aspect ratio of the current window */
  float aspect;
  /* Options */
  int eversion_method;
  Bool random_eversion_method;
  int display_mode;
  Bool random_display_mode;
  Bool edge_tubes;
  Bool random_edge_tubes;
  Bool self_tubes;
  Bool random_self_tubes;
  int colors;
  Bool random_colors;
  int projection;
  Bool random_projection;
  int transparency;
  /* 3D rotation angles */
  float alpha, beta, delta;
  /* Animation state */
  int anim_state;
  /* Deformation parameters for the eversion */
  float time;
  int defdir;
  /* Swipe parameters */
  int swipe_step;
  float swipe_y;
  /* The viewing offset */
  GLfloat offset3d[3];
  /* The viewing angle for perspective projection */
  float fov_angle;
  /* The viewing size for orthographic projection */
  float fov_ortho;
  /* The opacity of the transparent faces */
  float opacity;
  /* Trackball states */
  trackball_state *trackball;
  Bool button_pressed;
  /* A random factor to modify the rotation speeds */
  float speed_scale;
  /* The coordinates of the cuboctahedron faces */
  GLfloat tt[3*3*NUM_FACE];
  /* The normal vectors of the cuboctahedron faces */
  GLfloat tn[3*3*NUM_FACE];
  /* The coordinates of the edges of the cuboctahedron */
  GLfloat te[3*2*NUM_EDGE];
  /* The coordinates of the vertices of the cuboctahedron */
  GLfloat tv[3*NUM_VERT];
  /* The front and back face colors of the cuboctahedron */
  GLfloat tcf[4*3*NUM_FACE];
  GLfloat tcb[4*3*NUM_FACE];
  /* The coordinates of the self-intersection edges of the cuboctahedron */
  GLfloat se[3*3*2*NUM_FACE];
  /* The coordinates of the self-intersection vertices of the cuboctahedron */
  GLfloat sv[3*3*NUM_FACE];
  /* The number of self-intersection edges */
  int num_se;
  /* The number of self-intersection vertices */
  int num_sv;
  /* The coordinates of the edge tubes (spheres and cylinders) */
  GLfloat tube_vertex[NUM_VERT*3*NUMU*(NUMV+1)+NUM_EDGE*3*2*(NUMV+1)];
  /* The normal vectors of the edge tubes */
  GLfloat tube_normal[NUM_VERT*3*NUMU*(NUMV+1)+NUM_EDGE*3*2*(NUMV+1)];
  /* The front and back face colors of the edge tubes */
  GLfloat tube_colorf[NUM_VERT*4*NUMU*(NUMV+1)+NUM_EDGE*4*2*(NUMV+1)];
  GLfloat tube_colorb[NUM_VERT*4*NUMU*(NUMV+1)+NUM_EDGE*4*2*(NUMV+1)];
  /* The indices of the triangle strips in the edge tubes */
  GLuint tube_index[NUM_VERT*NUMU*2*(NUMV+1)+NUM_EDGE*2*(NUMV+1)];
  /* The indices of the start index of the triangle strips in the edge tubes */
  GLuint tube_tri_offset[NUMU*NUM_VERT+NUM_EDGE];
  /* The number of triangles in the triangle strips in the edge tubes */
  GLuint tube_tri_num[NUMU*NUM_VERT+NUM_EDGE];
  /* The number of triangle strips in the edge tubes */
  GLuint tube_tri_strips;
  /* The coordinates of the self-intersection tubes (spheres and cylinders) */
  GLfloat self_vertex[3*NUM_FACE*3*NUMU*(NUMV+1)+3*NUM_FACE*3*(NUMV+1)];
  /* The normal vectors of the self-intersection tubes */
  GLfloat self_normal[3*NUM_FACE*3*NUMU*(NUMV+1)+3*NUM_FACE*3*(NUMV+1)];
  /* The front and back face colors of the self-intersection tubes */
  GLfloat self_colorf[3*NUM_FACE*4*NUMU*(NUMV+1)+3*NUM_FACE*4*(NUMV+1)];
  GLfloat self_colorb[3*NUM_FACE*4*NUMU*(NUMV+1)+3*NUM_FACE*4*(NUMV+1)];
  /* The indices of the triangle strips in the self-intersection tubes */
  GLuint self_index[3*NUM_FACE*NUMU*(2*(NUMV+1)+1)+3*NUM_FACE*(2*(NUMV+1)+1)];
  /* The indices of the start index of the triangle strips in the
     self-intersection tubes */
  GLuint self_tri_offset[NUMU*3*NUM_FACE+3*NUM_FACE];
  /* The number of triangles in the triangle strips in the
     self-intersection tubes */
  GLuint self_tri_num[NUMU*3*NUM_FACE+3*NUM_FACE];
  /* The number of triangle strips in the self-intersection tubes */
  GLuint self_tri_strips;
  Bool use_shaders;
#ifdef HAVE_GLSL
  Bool use_mipmaps;
  /* The precomputed texture coordinates of the cuboctahedron */
  GLfloat tex[3*3*NUM_FACE];
  /* The textures of earth by day (index 0), earth by night (index 1), and
     the water mask (index 2) */
  GLuint tex_names[3];
  /* The inverse scaling factors of the dual depth peeling textures */
  GLfloat tex_scale[2];
  /* Indices for uniform variables and attributes */
  GLuint poly_shader_program;
  GLint poly_pos_index, poly_normal_index;
  GLint poly_colorf_index, poly_colorb_index, poly_vertex_tex_index;
  GLint poly_mv_index, poly_proj_index;
  GLint poly_glbl_ambient_index, poly_lt_ambient_index;
  GLint poly_lt_diffuse_index, poly_lt_specular_index;
  GLint poly_lt_direction_index, poly_lt_halfvect_index;
  GLint poly_front_ambient_index, poly_back_ambient_index;
  GLint poly_front_diffuse_index, poly_back_diffuse_index;
  GLint poly_specular_index, poly_shininess_index;
  GLint poly_tex_samp_f_index, poly_tex_samp_b_index;
  GLint poly_tex_samp_w_index, poly_bool_textures_index;
  GLuint cuboct_pos_buffer, cuboct_normal_buffer;
  GLuint cuboct_colorf_buffer, cuboct_colorb_buffer;
  GLuint cuboct_tex_coord_buffer;
  GLuint tube_vertex_buffer, tube_normal_buffer;
  GLuint tube_colorf_buffer, tube_colorb_buffer, tube_index_buffer;
  GLuint self_vertex_buffer, self_normal_buffer;
  GLuint self_colorf_buffer, self_colorb_buffer, self_index_buffer;
  GLuint dp_depth_tex[2], dp_front_blender_tex[2];
  GLuint dp_back_temp_tex[2], dp_back_blender_tex;
  GLuint dp_peeling_fbo[2], dp_color_fbo[2], dp_back_blender_fbo;
  GLuint dp_opaque_depth_tex, dp_opaque_color_tex;
  GLuint dp_opaque_fbo;
  GLuint dp_draw_fbo, dp_read_fbo;
  GLuint dp_init_shader_program;
  GLint dp_init_pos_index, dp_init_mv_index, dp_init_proj_index;
  GLint dp_init_tex_scale_index, dp_init_tex_samp_depth;
  GLuint dp_peel_shader_program;
  GLint dp_peel_pos_index, dp_peel_normal_index;
  GLint dp_peel_colorf_index, dp_peel_colorb_index, dp_peel_vertex_tex_index;
  GLint dp_peel_mv_index, dp_peel_proj_index;
  GLint dp_peel_glbl_ambient_index, dp_peel_lt_ambient_index;
  GLint dp_peel_lt_diffuse_index, dp_peel_lt_specular_index;
  GLint dp_peel_lt_direction_index, dp_peel_lt_halfvect_index;
  GLint dp_peel_front_ambient_index, dp_peel_back_ambient_index;
  GLint dp_peel_front_diffuse_index, dp_peel_back_diffuse_index;
  GLint dp_peel_specular_index, dp_peel_shininess_index;
  GLint dp_peel_tex_samp_depth_blend, dp_peel_tex_samp_front_blend;
  GLint dp_peel_tex_samp_f_index, dp_peel_tex_samp_b_index;
  GLint dp_peel_tex_samp_w_index, dp_peel_bool_textures_index;
  GLint dp_peel_tex_scale_index;
  GLuint dp_blnd_shader_program;
  GLint dp_blnd_pos_index, dp_blnd_tex_scale_index, dp_blnd_tex_samp_temp;
  GLuint dp_query;
  GLuint dp_finl_shader_program;
  GLint dp_finl_pos_index, dp_finl_tex_scale_index;
  GLint dp_finl_tex_samp_front, dp_finl_tex_samp_back;
  GLint dp_finl_tex_samp_opaque;
  GLuint wa_opaque_depth_tex, wa_opaque_color_tex;
  GLuint wa_opaque_fbo;
  GLuint wa_color_tex, wa_num_tex;
  GLuint wa_accumulation_fbo;
  GLuint wa_init_shader_program;
  GLint wa_init_pos_index, wa_init_normal_index;
  GLint wa_init_colorf_index, wa_init_colorb_index, wa_init_vertex_tex_index;
  GLint wa_init_mv_index, wa_init_proj_index;
  GLint wa_init_glbl_ambient_index, wa_init_lt_ambient_index;
  GLint wa_init_lt_diffuse_index, wa_init_lt_specular_index;
  GLint wa_init_lt_direction_index, wa_init_lt_halfvect_index;
  GLint wa_init_front_ambient_index, wa_init_back_ambient_index;
  GLint wa_init_front_diffuse_index, wa_init_back_diffuse_index;
  GLint wa_init_specular_index, wa_init_shininess_index;
  GLint wa_init_tex_samp_f_index, wa_init_tex_samp_b_index;
  GLint wa_init_tex_samp_w_index, wa_init_bool_textures_index;
  GLuint wa_finl_shader_program;
  GLint wa_finl_pos_index, wa_finl_tex_scale_index;
  GLint wa_finl_tex_samp_color, wa_finl_tex_samp_num;
  GLint wa_finl_tex_samp_opaque;
  GLuint quad_buffer;
#endif /* HAVE_GLSL */
} cubocteversionstruct;

extern cubocteversionstruct *cubocteversion;


#include "images/gen/earth_png.h"
#include "images/gen/earth_night_png.h"
#include "images/gen/earth_water_png.h"
#include "ximage-loader.h"

#include <float.h>


#ifdef USE_MODULES
ModStruct cubocteversion_description =
{"cubocteversion", "init_cubocteversion",
 "draw_cubocteversion", NULL, "draw_cubocteversion",
 "change_cubocteversion", "free_cubocteversion",
 &cubocteversion_opts, 25000, 1, 1, 1, 1.0, 4, "",
 "Show a cuboctahedron eversion", 0, NULL};

#endif


static char *eversion_method;
static char *mode;
static char *edge_tubes;
static char *self_tubes;
static char *color_mode;
static char *proj;
static char *transparency;
static float speed_x;
static float speed_y;
static float speed_z;
static float deform_speed;


static XrmOptionDescRec opts[] =
{
  {"-eversion-method",          ".eversionMethod",    XrmoptionSepArg, 0 },
  {"-morin-denner",             ".eversionMethod",    XrmoptionNoArg,  "morin-denner" },
  {"-apery",                    ".eversionMethod",    XrmoptionNoArg,  "apery" },
  {"-mode",                     ".displayMode",       XrmoptionSepArg, 0 },
  {"-surface",                  ".displayMode",       XrmoptionNoArg,  "surface" },
  {"-transparent",              ".displayMode",       XrmoptionNoArg,  "transparent" },
  {"-edges",                    ".edges",             XrmoptionSepArg, 0 },
  {"-self-intersections",       ".selfIntersections", XrmoptionSepArg, 0 },
  {"-colors",                   ".colors",            XrmoptionSepArg, 0 },
  {"-twosided-colors",          ".colors",            XrmoptionNoArg,  "two-sided" },
  {"-face-colors",              ".colors",            XrmoptionNoArg,  "face" },
  {"-earth-colors",             ".colors",            XrmoptionNoArg,  "earth" },
  {"-projection",               ".projection",        XrmoptionSepArg, 0 },
  {"-perspective",              ".projection",        XrmoptionNoArg,  "perspective" },
  {"-orthographic",             ".projection",        XrmoptionNoArg,  "orthographic" },
  {"-transparency",             ".transparency",      XrmoptionSepArg, 0 },
  {"-correct-transparency",     ".transparency",      XrmoptionNoArg,  "correct" },
  {"-approximate-transparency", ".transparency",      XrmoptionNoArg,  "approximate" },
  {"-standard-transparency",    ".transparency",      XrmoptionNoArg,  "standard" },
  {"-speed-x",                  ".speedx",            XrmoptionSepArg, 0 },
  {"-speed-y",                  ".speedy",            XrmoptionSepArg, 0 },
  {"-speed-z",                  ".speedz",            XrmoptionSepArg, 0 },
  {"-deformation-speed",        ".deformSpeed",       XrmoptionSepArg, 0 },
};

static argtype vars[] =
{
  { &eversion_method, "eversionMethod",    "EversionMethod",    DEF_EVERSION_METHOD,    t_String },
  { &mode,            "displayMode",       "DisplayMode",       DEF_DISPLAY_MODE,       t_String },
  { &edge_tubes,      "edges",             "Edges",             DEF_EDGES     ,         t_String },
  { &self_tubes,      "selfIntersections", "SelfIntersections", DEF_SELF_INTERSECTIONS, t_String },
  { &color_mode,      "colors",            "Colors",            DEF_COLORS,             t_String },
  { &proj,            "projection",        "Projection",        DEF_PROJECTION,         t_String },
  { &transparency,    "transparency",      "Transparency",      DEF_TRANSPARENCY,       t_String },
  { &speed_x,         "speedx",            "Speedx",            DEF_SPEEDX,             t_Float },
  { &speed_y,         "speedy",            "Speedy",            DEF_SPEEDY,             t_Float },
  { &speed_z,         "speedz",            "Speedz",            DEF_SPEEDZ,             t_Float },
  { &deform_speed,    "deformSpeed",       "DeformSpeed",       DEF_DEFORM_SPEED,       t_Float },
};

ENTRYPOINT ModeSpecOpt cubocteversion_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, NULL};


cubocteversionstruct *cubocteversion =
  (cubocteversionstruct *) NULL;


/* ------------------------------------------------------------------------- */


static int edge[NUM_EDGE][2] =
{
  {  0,  2 }, /* a0, a2 */
  {  0,  8 }, /* a0, c0 */
  {  8,  2 }, /* c0, a2 */
  {  2, 10 }, /* a2, c2 */
  { 10,  0 }, /* c2, a0 */
  {  0,  4 }, /* a0, b0 */
  {  8,  4 }, /* c0, b0 */
  {  8,  5 }, /* c0, b1 */
  {  2,  5 }, /* a2, b1 */
  {  2,  6 }, /* a2, b2 */
  { 10,  6 }, /* c2, b2 */
  { 10,  7 }, /* c2, b3 */
  {  0,  7 }, /* a0, b3 */
  {  4,  5 }, /* b0, b1 */
  {  5,  6 }, /* b1, b2 */
  {  6,  7 }, /* b2, b3 */
  {  7,  4 }, /* b3, b0 */
  { 11,  4 }, /* c3, b0 */
  {  1,  4 }, /* a1, b0 */
  {  1,  5 }, /* a1, b1 */
  {  9,  5 }, /* c1, b1 */
  {  9,  6 }, /* c1, b2 */
  {  3,  6 }, /* a3, b2 */
  {  3,  7 }, /* a3, b3 */
  { 11,  7 }, /* c3, b3 */
  { 11,  1 }, /* c3, a1 */
  {  1,  9 }, /* a1, c1 */
  {  9,  3 }, /* c1, a3 */
  {  3, 11 }, /* a3, c3 */
  {  1,  3 }  /* a1, a3 */
};

static int face[NUM_FACE][3] =
{
  {  0,  2, 10 }, /* a0, a2, c2 = R2    */
  {  0,  8,  2 }, /* a0, c0, a2 = R0    */
  {  5,  6,  2 }, /* b1, b2, a2 = P'''1 */
  {  6,  7, 10 }, /* b2, b3, c2 = P''2  */
  {  7,  4,  0 }, /* b3, b0, a0 = P'''3 */
  {  4,  5,  8 }, /* b0, b1, c0 = P''0  */
  {  0,  4,  8 }, /* a0, b0, c0 = P'0   */
  {  2,  6, 10 }, /* a2, b2, c2 = P'2   */
  {  0, 10,  7 }, /* a0, c2, b3 = Q2    */
  {  2,  8,  5 }, /* a2, c0, b1 = Q0    */
  {  1,  4, 11 }, /* a1, b0, c3 = Q3    */
  {  1,  9,  5 }, /* a1, c1, b1 = P'1   */
  {  1,  3,  9 }, /* a1, a3, c1 = R1    */
  {  1, 11,  3 }, /* a1, c3, a3 = R3    */
  {  4,  1,  5 }, /* b0, a1, b1 = P'''0 */
  {  5,  9,  6 }, /* b1, c1, b2 = P''1  */
  {  6,  3,  7 }, /* b2, a3, b3 = P'''2 */
  {  7, 11,  4 }, /* b3, c3, b0 = P''3  */
  {  3, 11,  7 }, /* a3, c3, b3 = P'3   */
  {  3,  6,  9 }  /* a3, b2, c1 = Q1    */
};

static float vert_apery[NUM_MODELS_APERY][NUM_VERT][3] =
{
  {
    {         1.0f,        -1.0f,                       0.0f }, /* a0 */
    {        -1.0f,        -1.0f,             2.0f*M_SQRT2_F }, /* a1 */
    {        -1.0f,         1.0f,                       0.0f }, /* a2 */
    {         1.0f,         1.0f,             2.0f*M_SQRT2_F }, /* a3 */
    {         0.0f,        -2.0f,                  M_SQRT2_F }, /* b0 */
    {        -2.0f,         0.0f,                  M_SQRT2_F }, /* b1 */
    {         0.0f,         2.0f,                  M_SQRT2_F }, /* b2 */
    {         2.0f,         0.0f,                  M_SQRT2_F }, /* b3 */
    {        -1.0f,        -1.0f,                       0.0f }, /* c0 */
    {        -1.0f,         1.0f,             2.0f*M_SQRT2_F }, /* c1 */
    {         1.0f,         1.0f,                       0.0f }, /* c2 */
    {         1.0f,        -1.0f,             2.0f*M_SQRT2_F }  /* c3 */
  },
  {
    {         1.0f,        -1.0f,                       0.0f }, /* a0 */
    {   -1.0f/8.0f,  -1.0f/16.0f, 15.0f/16.0f+1.0f/M_SQRT2_F }, /* a1 */
    {        -1.0f,         1.0f,                       0.0f }, /* a2 */
    {    1.0f/8.0f,   1.0f/16.0f, 15.0f/16.0f+1.0f/M_SQRT2_F }, /* a3 */
    {   -1.0f/6.0f,   -2.0f/3.0f,  11.0f/8.0f+0.5f/M_SQRT2_F }, /* b0 */
    { -11.0f/24.0f,    1.0f/4.0f, 17.0f/16.0f+0.5f/M_SQRT2_F }, /* b1 */
    {    1.0f/6.0f,    2.0f/3.0f,  11.0f/8.0f+0.5f/M_SQRT2_F }, /* b2 */
    {  11.0f/24.0f,   -1.0f/4.0f, 17.0f/16.0f+0.5f/M_SQRT2_F }, /* b3 */
    {  -22.0f/7.0f, -79.0f/28.0f,               153.0f/28.0f }, /* c0 */
    {   1.0f/14.0f,   5.0f/14.0f,                       1.0f }, /* c1 */
    {   22.0f/7.0f,  79.0f/28.0f,               153.0f/28.0f }, /* c2 */
    {  -1.0f/14.0f,  -5.0f/14.0f,                       1.0f }  /* c3 */
  },
  {
    {         1.0f,        -1.0f,                       0.0f }, /* a0 */
    {    1.0f/6.0f,    1.0f/4.0f,                  5.0f/4.0f }, /* a1 */
    {        -1.0f,         1.0f,                       0.0f }, /* a2 */
    {   -1.0f/6.0f,   -1.0f/4.0f,                  5.0f/4.0f }, /* a3 */
    {   -2.0f/9.0f,   -2.0f/9.0f,                 11.0f/6.0f }, /* b0 */
    {   1.0f/18.0f,    1.0f/3.0f,                17.0f/12.0f }, /* b1 */
    {    2.0f/9.0f,    2.0f/9.0f,                 11.0f/6.0f }, /* b2 */
    {  -1.0f/18.0f,   -1.0f/3.0f,                17.0f/12.0f }, /* b3 */
    {  -27.0f/7.0f,  -24.0f/7.0f,                 51.0f/7.0f }, /* c0 */
    {    3.0f/7.0f,    1.0f/7.0f,                25.0f/14.0f }, /* c1 */
    {   27.0f/7.0f,   24.0f/7.0f,                 51.0f/7.0f }, /* c2 */
    {   -3.0f/7.0f,   -1.0f/7.0f,                25.0f/14.0f }  /* c3 */
  },
  {
    {         1.0f,        -1.0f,                       0.0f }, /* a0 */
    {         1.0f,         1.0f,                       0.0f }, /* a1 */
    {        -1.0f,         1.0f,                       0.0f }, /* a2 */
    {        -1.0f,        -1.0f,                       0.0f }, /* a3 */
    {    1.0f/3.0f,   -1.0f/3.0f,                       1.0f }, /* b0 */
    {    1.0f/3.0f,    1.0f/3.0f,                       1.0f }, /* b1 */
    {   -1.0f/3.0f,    1.0f/3.0f,                       1.0f }, /* b2 */
    {   -1.0f/3.0f,   -1.0f/3.0f,                       1.0f }, /* b3 */
    {   -1.0f/7.0f,  -11.0f/7.0f,                 12.0f/7.0f }, /* c0 */
    {   11.0f/7.0f,   -1.0f/7.0f,                 12.0f/7.0f }, /* c1 */
    {    1.0f/7.0f,   11.0f/7.0f,                 12.0f/7.0f }, /* c2 */
    {  -11.0f/7.0f,    1.0f/7.0f,                 12.0f/7.0f }  /* c3 */
  },
  {
    {    1.0f/4.0f,   -1.0f/6.0f,                  5.0f/4.0f }, /* a0 */
    {         1.0f,         1.0f,                       0.0f }, /* a1 */
    {   -1.0f/4.0f,    1.0f/6.0f,                  5.0f/4.0f }, /* a2 */
    {        -1.0f,        -1.0f,                       0.0f }, /* a3 */
    {    1.0f/3.0f,  -1.0f/18.0f,                17.0f/12.0f }, /* b0 */
    {    2.0f/9.0f,   -2.0f/9.0f,                 11.0f/6.0f }, /* b1 */
    {   -1.0f/3.0f,   1.0f/18.0f,                17.0f/12.0f }, /* b2 */
    {   -2.0f/9.0f,    2.0f/9.0f,                 11.0f/6.0f }, /* b3 */
    {    1.0f/7.0f,   -3.0f/7.0f,                25.0f/14.0f }, /* c0 */
    {   24.0f/7.0f,  -27.0f/7.0f,                 51.0f/7.0f }, /* c1 */
    {   -1.0f/7.0f,    3.0f/7.0f,                25.0f/14.0f }, /* c2 */
    {  -24.0f/7.0f,   27.0f/7.0f,                 51.0f/7.0f }  /* c3 */
  },
  {
    {  -1.0f/16.0f,    1.0f/8.0f, 15.0f/16.0f+1.0f/M_SQRT2_F }, /* a0 */
    {         1.0f,         1.0f,                       0.0f }, /* a1 */
    {   1.0f/16.0f,   -1.0f/8.0f, 15.0f/16.0f+1.0f/M_SQRT2_F }, /* a2 */
    {        -1.0f,        -1.0f,                       0.0f }, /* a3 */
    {    1.0f/4.0f,  11.0f/24.0f, 17.0f/16.0f+0.5f/M_SQRT2_F }, /* b0 */
    {    2.0f/3.0f,   -1.0f/6.0f,  11.0f/8.0f+0.5f/M_SQRT2_F }, /* b1 */
    {   -1.0f/4.0f, -11.0f/24.0f, 17.0f/16.0f+0.5f/M_SQRT2_F }, /* b2 */
    {   -2.0f/3.0f,    1.0f/6.0f,  11.0f/8.0f+0.5f/M_SQRT2_F }, /* b3 */
    {   5.0f/14.0f,  -1.0f/14.0f,                       1.0f }, /* c0 */
    {  79.0f/28.0f,  -22.0f/7.0f,               153.0f/28.0f }, /* c1 */
    {  -5.0f/14.0f,   1.0f/14.0f,                       1.0f }, /* c2 */
    { -79.0f/28.0f,   22.0f/7.0f,               153.0f/28.0f }  /* c3 */
  },
  {
    {        -1.0f,         1.0f,             2.0f*M_SQRT2_F }, /* a0 */
    {         1.0f,         1.0f,                       0.0f }, /* a1 */
    {         1.0f,        -1.0f,             2.0f*M_SQRT2_F }, /* a2 */
    {        -1.0f,        -1.0f,                       0.0f }, /* a3 */
    {         0.0f,         2.0f,                  M_SQRT2_F }, /* b0 */
    {         2.0f,         0.0f,                  M_SQRT2_F }, /* b1 */
    {         0.0f,        -2.0f,                  M_SQRT2_F }, /* b2 */
    {        -2.0f,         0.0f,                  M_SQRT2_F }, /* b3 */
    {         1.0f,         1.0f,             2.0f*M_SQRT2_F }, /* c0 */
    {         1.0f,        -1.0f,                       0.0f }, /* c1 */
    {        -1.0f,        -1.0f,             2.0f*M_SQRT2_F }, /* c2 */
    {        -1.0f,         1.0f,                       0.0f }  /* c3 */
  }
};


static float vert_denner[NUM_MODELS_DENNER][NUM_VERT][3] =
{
  {
    {          0.0f,         -1.0f,         0.0f },
    {         -1.0f,          0.0f,         2.0f },
    {          0.0f,          1.0f,         0.0f },
    {          1.0f,          0.0f,         2.0f },
    {         -1.0f,         -1.0f,         1.0f },
    {         -1.0f,          1.0f,         1.0f },
    {          1.0f,          1.0f,         1.0f },
    {          1.0f,         -1.0f,         1.0f },
    {         -1.0f,          0.0f,         0.0f },
    {          0.0f,          1.0f,         2.0f },
    {          1.0f,          0.0f,         0.0f },
    {          0.0f,         -1.0f,         2.0f }
  },
  {
    {          0.0f,         -1.0f,         0.0f },
    {         -1.0f,          0.0f,         2.0f },
    {          0.0f,          1.0f,         0.0f },
    {          1.0f,          0.0f,         2.0f },
    {         -2.0f,         -1.0f,         2.0f },
    {         -1.0f,          1.0f,         1.0f },
    {          2.0f,          1.0f,         2.0f },
    {          1.0f,         -1.0f,         1.0f },
    {         -1.0f,          0.0f,         0.0f },
    {          0.0f,          1.0f,         2.0f },
    {          1.0f,          0.0f,         0.0f },
    {          0.0f,         -1.0f,         2.0f }
  },
  {
    {          0.0f,         -1.0f,         0.0f },
    {         -1.0f,          0.0f,         2.0f },
    {          0.0f,          1.0f,         0.0f },
    {          1.0f,          0.0f,         2.0f },
    {         -2.0f,         -1.0f,         2.0f },
    {         -1.0f,          2.0f,         2.0f },
    {          2.0f,          1.0f,         2.0f },
    {          1.0f,         -2.0f,         2.0f },
    {         -1.0f,          0.0f,         0.0f },
    {          0.0f,          1.0f,         2.0f },
    {          1.0f,          0.0f,         0.0f },
    {          0.0f,         -1.0f,         2.0f }
  },
  {
    {          0.0f,         -1.0f,         0.0f },
    { -0.111904762f,          0.0f,         2.0f },
    {          0.0f,          1.0f,         0.0f },
    {  0.111904762f,          0.0f,         2.0f },
    {         -2.0f,         -1.0f,         2.0f },
    {         -1.0f,          2.0f,         2.0f },
    {          2.0f,          1.0f,         2.0f },
    {          1.0f,         -2.0f,         2.0f },
    {         -1.0f,          0.0f,         0.0f },
    {          0.0f,          1.0f,         2.0f },
    {          1.0f,          0.0f,         0.0f },
    {          0.0f,         -1.0f,         2.0f }
  },
  {
    {          0.0f,         -1.0f,         0.0f },
    { -0.111904762f,          0.0f,         2.0f },
    {          0.0f,          1.0f,         0.0f },
    {  0.111904762f,          0.0f,         2.0f },
    {          0.0f, -1.666666666f,         2.0f },
    {         -1.0f,          2.0f,         2.0f },
    {          0.0f,  1.666666666f,         2.0f },
    {          1.0f,         -2.0f,         2.0f },
    {         -1.0f,          0.0f,         0.0f },
    {          0.0f,          1.0f,         2.0f },
    {          1.0f,          0.0f,         0.0f },
    {          0.0f,         -1.0f,         2.0f }
  },
  {
    {          0.0f,         -1.0f,         0.0f },
    { -0.111904762f,          0.0f,         2.0f },
    {          0.0f,          1.0f,         0.0f },
    {  0.111904762f,          0.0f,         2.0f },
    {          0.0f, -1.666666666f,         2.0f },
    {         -1.0f,          2.0f,         2.0f },
    {          0.0f,  1.666666666f,         2.0f },
    {          1.0f,         -2.0f,         2.0f },
    {         -1.0f,          0.0f,         0.0f },
    {          0.0f,  0.040216369f,         2.0f },
    {          1.0f,          0.0f,         0.0f },
    {          0.0f, -0.040216369f,         2.0f }
  },
  {
    {          0.0f,         -1.0f,         0.0f },
    { -0.111904762f,          0.0f,         2.0f },
    {          0.0f,          1.0f,         0.0f },
    {  0.111904762f,          0.0f,         2.0f },
    {          0.0f, -1.666666666f,         2.0f },
    {  -0.45454545f,          0.0f,         2.0f },
    {          0.0f,  1.666666666f,         2.0f },
    {   0.45454545f,          0.0f,         2.0f },
    {         -1.0f,          0.0f,         0.0f },
    {          0.0f,  0.040216369f,         2.0f },
    {          1.0f,          0.0f,         0.0f },
    {          0.0f, -0.040216369f,         2.0f }
  },
  {
    {          0.0f,         -1.0f,         0.0f },
    { -0.111904762f,          0.0f,         2.0f },
    {          0.0f,          1.0f,         0.0f },
    {  0.111904762f,          0.0f,         2.0f },
    {          0.0f, -1.666666666f,         2.0f },
    { -0.333333333f,          0.0f,         2.0f },
    {          0.0f,  1.666666666f,         2.0f },
    {  0.333333333f,          0.0f,         2.0f },
    {         -1.0f,          0.0f,         0.0f },
    {          0.0f,  0.040216369f,         2.0f },
    {          1.0f,          0.0f,         0.0f },
    {          0.0f, -0.040216369f,         2.0f }
  },
  {
    {          2.0f,         -3.0f,         0.0f },
    { -0.111904762f,          0.0f,         2.0f },
    {         -2.0f,          3.0f,         0.0f },
    {  0.111904762f,          0.0f,         2.0f },
    {          0.0f, -1.666666666f,         2.0f },
    { -0.333333333f,          0.0f,         2.0f },
    {          0.0f,  1.666666666f,         2.0f },
    {  0.333333333f,          0.0f,         2.0f },
    {         -1.0f,          0.0f,         0.0f },
    {          0.0f,  0.040216369f,         2.0f },
    {          1.0f,          0.0f,         0.0f },
    {          0.0f, -0.040216369f,         2.0f }
  },
  {
    {          2.0f,         -3.0f,         0.0f },
    { -0.111904762f,          0.0f,         2.0f },
    {         -2.0f,          3.0f,         0.0f },
    {  0.111904762f,          0.0f,         2.0f },
    {          0.0f, -1.666666666f,         2.0f },
    { -0.333333333f,          0.0f,         2.0f },
    {          0.0f,  1.666666666f,         2.0f },
    {  0.333333333f,          0.0f,         2.0f },
    {          0.0f,         -1.0f,         0.0f },
    {          0.0f,  0.040216369f,         2.0f },
    {          0.0f,          1.0f,         0.0f },
    {          0.0f, -0.040216369f,         2.0f }
  },
  {
    {          1.0f,         -1.0f,         0.0f },
    { -0.111904762f,          0.0f,         2.0f },
    {         -1.0f,          1.0f,         0.0f },
    {  0.111904762f,          0.0f,         2.0f },
    {          0.0f, -1.666666666f,         2.0f },
    { -0.333333333f,          0.0f,         2.0f },
    {          0.0f,  1.666666666f,         2.0f },
    {  0.333333333f,          0.0f,         2.0f },
    {          0.0f,         -1.0f,         0.0f },
    {          0.0f,  0.040216369f,         2.0f },
    {          0.0f,          1.0f,         0.0f },
    {          0.0f, -0.040216369f,         2.0f }
  },
  {
    {          1.0f,         -1.0f,         0.0f },
    { -0.111904762f,          0.0f,         2.0f },
    {         -1.0f,          1.0f,         0.0f },
    {  0.111904762f,          0.0f,         2.0f },
    {          0.0f, -1.666666666f,         2.0f },
    { -0.333333333f,          0.0f,         2.0f },
    {          0.0f,  1.666666666f,         2.0f },
    {  0.333333333f,          0.0f,         2.0f },
    {          0.0f,         -1.5f,         1.5f },
    {          0.0f,  0.040216369f,         2.0f },
    {          0.0f,          1.5f,         1.5f },
    {          0.0f, -0.040216369f,         2.0f }
  },
  {
    {          1.0f,         -1.0f,         0.0f },
    { -0.111904762f,          0.0f,         2.0f },
    {         -1.0f,          1.0f,         0.0f },
    {  0.111904762f,          0.0f,         2.0f },
    {          0.0f,         -0.1f,         2.0f },
    { -0.333333333f,          0.0f,         2.0f },
    {          0.0f,          0.1f,         2.0f },
    {  0.333333333f,          0.0f,         2.0f },
    {          0.0f,         -1.5f,         1.5f },
    {          0.0f,  0.040216369f,         2.0f },
    {          0.0f,          1.5f,         1.5f },
    {          0.0f, -0.040216369f,         2.0f }
  },
  {
    {          1.0f,         -1.0f,         0.0f },
    { -0.111904762f,          0.0f,         2.0f },
    {         -1.0f,          1.0f,         0.0f },
    {  0.111904762f,          0.0f,         2.0f },
    {          0.0f,         -0.1f,         2.0f },
    { -0.333333333f,          0.0f,         2.0f },
    {          0.0f,          0.1f,         2.0f },
    {  0.333333333f,          0.0f,         2.0f },
    {          0.0f,         -1.5f,         1.5f },
    { -0.047712497f,  0.057363301f,         2.0f },
    {          0.0f,          1.5f,         1.5f },
    {  0.047712497f, -0.057363301f,         2.0f }
  },
  {
    {          1.0f,         -1.0f,         0.0f },
    { -0.111904762f,          0.0f,         2.0f },
    {         -1.0f,          1.0f,         0.0f },
    {  0.111904762f,          0.0f,         2.0f },
    {          0.0f,         -0.1f,         2.0f },
    { -0.333333333f,          0.0f,         2.0f },
    {          0.0f,          0.1f,         2.0f },
    {  0.333333333f,          0.0f,         2.0f },
    { -4.567567568f, -3.783783784f, 8.351351351f },
    { -0.047712497f,  0.057363301f,         2.0f },
    {  4.567567568f,  3.783783784f, 8.351351351f },
    {  0.047712497f, -0.057363301f,         2.0f }
  },
  {
    {          1.0f,         -1.0f,         0.0f },
    { -0.111904762f,          0.0f,         2.0f },
    {         -1.0f,          1.0f,         0.0f },
    {  0.111904762f,          0.0f,         2.0f },
    { -0.333333333f,         -0.2f,         2.0f },
    { -0.333333333f,          0.0f,         2.0f },
    {  0.333333333f,          0.2f,         2.0f },
    {  0.333333333f,          0.0f,         2.0f },
    { -4.567567568f, -3.783783784f, 8.351351351f },
    { -0.047712497f,  0.057363301f,         2.0f },
    {  4.567567568f,  3.783783784f, 8.351351351f },
    {  0.047712497f, -0.057363301f,         2.0f }
  },
  {
    {          1.0f,         -1.0f,         0.0f },
    {          0.0f,          0.1f,         1.5f },
    {         -1.0f,          1.0f,         0.0f },
    {          0.0f,         -0.1f,         1.5f },
    { -0.333333333f,         -0.2f,         2.0f },
    {          0.0f,   0.29787234f,         1.5f },
    {  0.333333333f,          0.2f,         2.0f },
    {          0.0f,  -0.29787234f,         1.5f },
    { -4.567567568f, -3.783783784f, 8.351351351f },
    {  0.191211003f,  0.157363301f, 1.786816505f },
    {  4.567567568f,  3.783783784f, 8.351351351f },
    { -0.191211003f, -0.157363301f, 1.786816505f }
  },
  {
    {          1.0f,         -1.0f,         0.0f },
    {          1.0f,          1.0f,         0.0f },
    {         -1.0f,          1.0f,         0.0f },
    {         -1.0f,         -1.0f,         0.0f },
    { -0.333333333f,         -0.2f,         2.0f },
    {  0.347826087f,  0.608695652f,  0.97826087f },
    {  0.333333333f,          0.2f,         2.0f },
    { -0.347826087f, -0.608695652f,  0.97826087f },
    { -4.567567568f, -3.783783784f, 8.351351351f },
    {  0.617577994f,  0.541093593f, 1.147266018f },
    {  4.567567568f,  3.783783784f, 8.351351351f },
    { -0.617577994f, -0.541093593f, 1.147266018f }
  },
  {
    {          1.0f,         -1.0f,         0.0f },
    {          1.0f,          1.0f,         0.0f },
    {         -1.0f,          1.0f,         0.0f },
    {         -1.0f,         -1.0f,         0.0f },
    { -0.333333333f,         -0.2f,         2.0f },
    {  0.347826087f,  0.608695652f,  0.97826087f },
    {  0.333333333f,          0.2f,         2.0f },
    { -0.347826087f, -0.608695652f,  0.97826087f },
    { -4.567567568f, -3.783783784f, 8.351351351f },
    {  2.333333333f,  0.111111111f, 2.222222222f },
    {  4.567567568f,  3.783783784f, 8.351351351f },
    { -2.333333333f, -0.111111111f, 2.222222222f }
  },
  {
    {          1.0f,         -1.0f,         0.0f },
    {          1.0f,          1.0f,         0.0f },
    {         -1.0f,          1.0f,         0.0f },
    {         -1.0f,         -1.0f,         0.0f },
    {  0.230769231f, -0.538461538f, 1.153846154f },
    {  0.347826087f,  0.608695652f,  0.97826087f },
    { -0.230769231f,  0.538461538f, 1.153846154f },
    { -0.347826087f, -0.608695652f,  0.97826087f },
    { -4.567567568f, -3.783783784f, 8.351351351f },
    {  2.333333333f,  0.111111111f, 2.222222222f },
    {  4.567567568f,  3.783783784f, 8.351351351f },
    { -2.333333333f, -0.111111111f, 2.222222222f }
  },
  {
    {          1.0f,         -1.0f,         0.0f },
    {          1.0f,          1.0f,         0.0f },
    {         -1.0f,          1.0f,         0.0f },
    {         -1.0f,         -1.0f,         0.0f },
    {  0.230769231f, -0.538461538f, 1.153846154f },
    {  0.347826087f,  0.608695652f,  0.97826087f },
    { -0.230769231f,  0.538461538f, 1.153846154f },
    { -0.347826087f, -0.608695652f,  0.97826087f },
    { -0.263157895f, -1.631578947f, 1.894736842f },
    {  2.333333333f,  0.111111111f, 2.222222222f },
    {  0.263157895f,  1.631578947f, 1.894736842f },
    { -2.333333333f, -0.111111111f, 2.222222222f }
  },
  {
    {          1.0f,         -1.0f,         0.0f },
    {          1.0f,          1.0f,         0.0f },
    {         -1.0f,          1.0f,         0.0f },
    {         -1.0f,         -1.0f,         0.0f },
    {  0.333333333f, -0.333333333f,         1.0f },
    {         0.25f,         0.25f,       1.125f },
    { -0.333333333f,  0.333333333f,         1.0f },
    {        -0.25f,        -0.25f,       1.125f },
    { -0.263157895f, -1.631578947f, 1.894736842f },
    {  1.571428571f, -0.142857143f, 1.714285714f },
    {  0.263157895f,  1.631578947f, 1.894736842f },
    { -1.571428571f,  0.142857143f, 1.714285714f }
  },
  {
    {          1.0f,         -1.0f,         0.0f },
    {          1.0f,          1.0f,         0.0f },
    {         -1.0f,          1.0f,         0.0f },
    {         -1.0f,         -1.0f,         0.0f },
    {  0.333333333f, -0.333333333f,         1.0f },
    {  0.333333333f,  0.333333333f,         1.0f },
    { -0.333333333f,  0.333333333f,         1.0f },
    { -0.333333333f, -0.333333333f,         1.0f },
    { -0.142857143f, -1.571428571f, 1.714285714f },
    {  1.571428571f, -0.142857143f, 1.714285714f },
    {  0.142857143f,  1.571428571f, 1.714285714f },
    { -1.571428571f,  0.142857143f, 1.714285714f }
  },
  {
    {          1.0f,         -1.0f,         0.0f },
    {          1.0f,          1.0f,         0.0f },
    {         -1.0f,          1.0f,         0.0f },
    {         -1.0f,         -1.0f,         0.0f },
    {         0.25f,        -0.25f,       1.125f },
    {  0.333333333f,  0.333333333f,         1.0f },
    {        -0.25f,         0.25f,       1.125f },
    { -0.333333333f, -0.333333333f,         1.0f },
    { -0.142857143f, -1.571428571f, 1.714285714f },
    {  1.631578947f,  -0.26315895f, 1.894736842f },
    {  0.142857143f,  1.571428571f, 1.714285714f },
    { -1.631578947f,   0.26315895f, 1.894736842f }
  },
  {
    {          1.0f,         -1.0f,         0.0f },
    {          1.0f,          1.0f,         0.0f },
    {         -1.0f,          1.0f,         0.0f },
    {         -1.0f,         -1.0f,         0.0f },
    {  0.608695652f, -0.347826087f,  0.97826087f },
    {  0.538461538f,  0.230769231f, 1.153846154f },
    { -0.608695652f,  0.347826087f,  0.97826087f },
    { -0.538461538f, -0.230769231f, 1.153846154f },
    {  0.111111111f, -2.333333333f, 2.222222222f },
    {  1.631578947f, -0.263157895f, 1.894736842f },
    { -0.111111111f,  2.333333333f, 2.222222222f },
    { -1.631578947f,  0.263157895f, 1.894736842f }
  },
  {
    {          1.0f,         -1.0f,         0.0f },
    {          1.0f,          1.0f,         0.0f },
    {         -1.0f,          1.0f,         0.0f },
    {         -1.0f,         -1.0f,         0.0f },
    {  0.608695652f, -0.347826087f,  0.97826087f },
    {  0.538461538f,  0.230769231f, 1.153846154f },
    { -0.608695652f,  0.347826087f,  0.97826087f },
    { -0.538461538f, -0.230769231f, 1.153846154f },
    {  0.111111111f, -2.333333333f, 2.222222222f },
    {  3.783783784f, -4.567567568f, 8.351351351f },
    { -0.111111111f,  2.333333333f, 2.222222222f },
    { -3.783783784f,  4.567567568f, 8.351351351f }
  },
  {
    {          1.0f,         -1.0f,         0.0f },
    {          1.0f,          1.0f,         0.0f },
    {         -1.0f,          1.0f,         0.0f },
    {         -1.0f,         -1.0f,         0.0f },
    {  0.608695652f, -0.347826087f,  0.97826087f },
    {          0.2f, -0.333333333f,         2.0f },
    { -0.608695652f,  0.347826087f,  0.97826087f },
    {         -0.2f,  0.333333333f,         2.0f },
    {  0.111111111f, -2.333333333f, 2.222222222f },
    {  3.783783784f, -4.567567568f, 8.351351351f },
    { -0.111111111f,  2.333333333f, 2.222222222f },
    { -3.783783784f,  4.567567568f, 8.351351351f }
  },
  {
    {          1.0f,         -1.0f,         0.0f },
    {          1.0f,          1.0f,         0.0f },
    {         -1.0f,          1.0f,         0.0f },
    {         -1.0f,         -1.0f,         0.0f },
    {  0.608695652f, -0.347826087f,  0.97826087f },
    {          0.2f, -0.333333333f,         2.0f },
    { -0.608695652f,  0.347826087f,  0.97826087f },
    {         -0.2f,  0.333333333f,         2.0f },
    {  0.541093593f, -0.617577994f, 1.147266018f },
    {  3.783783784f, -4.567567568f, 8.351351351f },
    { -0.541093593f,  0.617577994f, 1.147266018f },
    { -3.783783784f,  4.567567568f, 8.351351351f }
  },
  {
    {          0.1f,          0.0f,         1.5f },
    {          1.0f,          1.0f,         0.0f },
    {         -0.1f,          0.0f,         1.5f },
    {         -1.0f,         -1.0f,         0.0f },
    {   0.29787234f,          0.0f,         1.5f },
    {          0.2f, -0.333333333f,         2.0f },
    {  -0.29787234f,          0.0f,         1.5f },
    {         -0.2f,  0.333333333f,         2.0f },
    {  0.157363301f, -0.191211003f, 1.786816505f },
    {  3.783783784f, -4.567567568f, 8.351351351f },
    { -0.157363301f,  0.191211003f, 1.786816505f },
    { -3.783783784f,  4.567567568f, 8.351351351f }
  },
  {
    {          0.0f,  0.111904762f,         2.0f },
    {          1.0f,          1.0f,         0.0f },
    {          0.0f, -0.111904762f,         2.0f },
    {         -1.0f,         -1.0f,         0.0f },
    {          0.0f,  0.333333333f,         2.0f },
    {          0.2f, -0.333333333f,         2.0f },
    {          0.0f, -0.333333333f,         2.0f },
    {         -0.2f,  0.333333333f,         2.0f },
    {  0.057363301f,  0.047712497f,         2.0f },
    {  3.783783784f, -4.567567568f, 8.351351351f },
    { -0.057363301f, -0.047712497f,         2.0f },
    { -3.783783784f,  4.567567568f, 8.351351351f }
  },
  {
    {          0.0f,  0.111904762f,         2.0f },
    {          1.0f,          1.0f,         0.0f },
    {          0.0f, -0.111904762f,         2.0f },
    {         -1.0f,         -1.0f,         0.0f },
    {          0.0f,  0.333333333f,         2.0f },
    {          0.1f,          0.0f,         2.0f },
    {          0.0f, -0.333333333f,         2.0f },
    {         -0.1f,          0.0f,         2.0f },
    {  0.057363301f,  0.047712497f,         2.0f },
    {  3.783783784f, -4.567567568f, 8.351351351f },
    { -0.057363301f, -0.047712497f,         2.0f },
    { -3.783783784f,  4.567567568f, 8.351351351f }
  },
  {
    {          0.0f,  0.111904762f,         2.0f },
    {          1.0f,          1.0f,         0.0f },
    {          0.0f, -0.111904762f,         2.0f },
    {         -1.0f,         -1.0f,         0.0f },
    {          0.0f,  0.333333333f,         2.0f },
    {          0.1f,          0.0f,         2.0f },
    {          0.0f, -0.333333333f,         2.0f },
    {         -0.1f,          0.0f,         2.0f },
    {  0.057363301f,  0.047712497f,         2.0f },
    {          1.5f,          0.0f,         1.5f },
    { -0.057363301f, -0.047712497f,         2.0f },
    {         -1.5f,          0.0f,         1.5f }
  },
  {
    {          0.0f,  0.111904762f,         2.0f },
    {          1.0f,          1.0f,         0.0f },
    {          0.0f, -0.111904762f,         2.0f },
    {         -1.0f,         -1.0f,         0.0f },
    {          0.0f,  0.333333333f,         2.0f },
    {          0.1f,          0.0f,         2.0f },
    {          0.0f, -0.333333333f,         2.0f },
    {         -0.1f,          0.0f,         2.0f },
    {  0.040216369f,          0.0f,         2.0f },
    {          1.5f,          0.0f,         1.5f },
    { -0.040216369f,          0.0f,         2.0f },
    {         -1.5f,          0.0f,         1.5f }
  },
  {
    {          0.0f,  0.111904762f,         2.0f },
    {          1.0f,          1.0f,         0.0f },
    {          0.0f, -0.111904762f,         2.0f },
    {         -1.0f,         -1.0f,         0.0f },
    {          0.0f,  0.333333333f,         2.0f },
    {  1.666666666f,          0.0f,         2.0f },
    {          0.0f, -0.333333333f,         2.0f },
    { -1.666666666f,          0.0f,         2.0f },
    {  0.040216369f,          0.0f,         2.0f },
    {          1.5f,          0.0f,         1.5f },
    { -0.040216369f,          0.0f,         2.0f },
    {         -1.5f,          0.0f,         1.5f }
  },
  {
    {          0.0f,  0.111904762f,         2.0f },
    {          1.0f,          1.0f,         0.0f },
    {          0.0f, -0.111904762f,         2.0f },
    {         -1.0f,         -1.0f,         0.0f },
    {          0.0f,  0.333333333f,         2.0f },
    {  1.666666666f,          0.0f,         2.0f },
    {          0.0f, -0.333333333f,         2.0f },
    { -1.666666666f,          0.0f,         2.0f },
    {  0.040216369f,          0.0f,         2.0f },
    {          1.0f,          0.0f,         0.0f },
    { -0.040216369f,          0.0f,         2.0f },
    {         -1.0f,          0.0f,         0.0f }
  },
  {
    {          0.0f,  0.111904762f,         2.0f },
    {          3.0f,          2.0f,         0.0f },
    {          0.0f, -0.111904762f,         2.0f },
    {         -3.0f,         -2.0f,         0.0f },
    {          0.0f,  0.333333333f,         2.0f },
    {  1.666666666f,          0.0f,         2.0f },
    {          0.0f, -0.333333333f,         2.0f },
    { -1.666666666f,          0.0f,         2.0f },
    {  0.040216369f,          0.0f,         2.0f },
    {          1.0f,          0.0f,         0.0f },
    { -0.040216369f,          0.0f,         2.0f },
    {         -1.0f,          0.0f,         0.0f }
  },
  {
    {          0.0f,  0.111904762f,         2.0f },
    {          3.0f,          2.0f,         0.0f },
    {          0.0f, -0.111904762f,         2.0f },
    {         -3.0f,         -2.0f,         0.0f },
    {          0.0f,  0.333333333f,         2.0f },
    {  1.666666666f,          0.0f,         2.0f },
    {          0.0f, -0.333333333f,         2.0f },
    { -1.666666666f,          0.0f,         2.0f },
    {  0.040216369f,          0.0f,         2.0f },
    {          0.0f,         -1.0f,         0.0f },
    { -0.040216369f,          0.0f,         2.0f },
    {          0.0f,          1.0f,         0.0f }
  },
  {
    {          0.0f,  0.111904762f,         2.0f },
    {          1.0f,          0.0f,         0.0f },
    {          0.0f, -0.111904762f,         2.0f },
    {         -1.0f,          0.0f,         0.0f },
    {          0.0f,  0.333333333f,         2.0f },
    {  1.666666666f,          0.0f,         2.0f },
    {          0.0f, -0.333333333f,         2.0f },
    { -1.666666666f,          0.0f,         2.0f },
    {  0.040216369f,          0.0f,         2.0f },
    {          0.0f,         -1.0f,         0.0f },
    { -0.040216369f,          0.0f,         2.0f },
    {          0.0f,          1.0f,         0.0f }
  },
  {
    {          0.0f,  0.111904762f,         2.0f },
    {          1.0f,          0.0f,         0.0f },
    {          0.0f, -0.111904762f,         2.0f },
    {         -1.0f,          0.0f,         0.0f },
    {          0.0f,   0.45454545f,         2.0f },
    {  1.666666666f,          0.0f,         2.0f },
    {          0.0f,  -0.45454545f,         2.0f },
    { -1.666666666f,          0.0f,         2.0f },
    {  0.040216369f,          0.0f,         2.0f },
    {          0.0f,         -1.0f,         0.0f },
    { -0.040216369f,          0.0f,         2.0f },
    {          0.0f,          1.0f,         0.0f }
  },
  {
    {          0.0f,  0.111904762f,         2.0f },
    {          1.0f,          0.0f,         0.0f },
    {          0.0f, -0.111904762f,         2.0f },
    {         -1.0f,          0.0f,         0.0f },
    {          2.0f,          1.0f,         2.0f },
    {  1.666666666f,          0.0f,         2.0f },
    {         -2.0f,         -1.0f,         2.0f },
    { -1.666666666f,          0.0f,         2.0f },
    {  0.040216369f,          0.0f,         2.0f },
    {          0.0f,         -1.0f,         0.0f },
    { -0.040216369f,          0.0f,         2.0f },
    {          0.0f,          1.0f,         0.0f }
  },
  {
    {          0.0f,  0.111904762f,         2.0f },
    {          1.0f,          0.0f,         0.0f },
    {          0.0f, -0.111904762f,         2.0f },
    {         -1.0f,          0.0f,         0.0f },
    {          2.0f,          1.0f,         2.0f },
    {  1.666666666f,          0.0f,         2.0f },
    {         -2.0f,         -1.0f,         2.0f },
    { -1.666666666f,          0.0f,         2.0f },
    {          1.0f,          0.0f,         2.0f },
    {          0.0f,         -1.0f,         0.0f },
    {         -1.0f,          0.0f,         2.0f },
    {          0.0f,          1.0f,         0.0f }
  },
  {
    {          0.0f,  0.111904762f,         2.0f },
    {          1.0f,          0.0f,         0.0f },
    {          0.0f, -0.111904762f,         2.0f },
    {         -1.0f,          0.0f,         0.0f },
    {          2.0f,          1.0f,         2.0f },
    {          1.0f,         -2.0f,         2.0f },
    {         -2.0f,         -1.0f,         2.0f },
    {         -1.0f,          2.0f,         2.0f },
    {          1.0f,          0.0f,         2.0f },
    {          0.0f,         -1.0f,         0.0f },
    {         -1.0f,          0.0f,         2.0f },
    {          0.0f,          1.0f,         0.0f }
  },
  {
    {          0.0f,          1.0f,         2.0f },
    {          1.0f,          0.0f,         0.0f },
    {          0.0f,         -1.0f,         2.0f },
    {         -1.0f,          0.0f,         0.0f },
    {          2.0f,          1.0f,         2.0f },
    {          1.0f,         -2.0f,         2.0f },
    {         -2.0f,         -1.0f,         2.0f },
    {         -1.0f,          2.0f,         2.0f },
    {          1.0f,          0.0f,         2.0f },
    {          0.0f,         -1.0f,         0.0f },
    {         -1.0f,          0.0f,         2.0f },
    {          0.0f,          1.0f,         0.0f }
  },
  {
    {          0.0f,          1.0f,         2.0f },
    {          1.0f,          0.0f,         0.0f },
    {          0.0f,         -1.0f,         2.0f },
    {         -1.0f,          0.0f,         0.0f },
    {          1.0f,          1.0f,         1.0f },
    {          1.0f,         -2.0f,         2.0f },
    {         -1.0f,         -1.0f,         1.0f },
    {         -1.0f,          2.0f,         2.0f },
    {          1.0f,          0.0f,         2.0f },
    {          0.0f,         -1.0f,         0.0f },
    {         -1.0f,          0.0f,         2.0f },
    {          0.0f,          1.0f,         0.0f }
  },
  {
    {          0.0f,          1.0f,         2.0f },
    {          1.0f,          0.0f,         0.0f },
    {          0.0f,         -1.0f,         2.0f },
    {         -1.0f,          0.0f,         0.0f },
    {          1.0f,          1.0f,         1.0f },
    {          1.0f,         -1.0f,         1.0f },
    {         -1.0f,         -1.0f,         1.0f },
    {         -1.0f,          1.0f,         1.0f },
    {          1.0f,          0.0f,         2.0f },
    {          0.0f,         -1.0f,         0.0f },
    {         -1.0f,          0.0f,         2.0f },
    {          0.0f,          1.0f,         0.0f }
  }
};


/* ------------------------------------------------------------------------- */


#ifdef HAVE_GLSL

/* The GLSL versions that correspond to different versions of OpenGL. */
static const GLchar *shader_version_2_1 =
  "#version 120\n";
static const GLchar *shader_version_3_0 =
  "#version 130\n";
static const GLchar *shader_version_3_0_es =
  "#version 300 es\n"
  "precision highp float;\n"
  "precision highp int;\n"
  "precision highp sampler2D;\n";


/* The vertex shader code is composed of code fragments that depend on
   the OpenGL version and code fragments that are version-independent.
   They are concatenated by glShaderSource in the function init_glsl(). */
static const GLchar *poly_vertex_shader_attribs_2_1 =
  "attribute vec3 VertexPosition;\n"
  "attribute vec3 VertexNormal;\n"
  "attribute vec4 VertexColorF;\n"
  "attribute vec4 VertexColorB;\n"
  "attribute vec3 VertexTexCoord;\n"
  "\n"
  "varying vec3 Normal;\n"
  "varying vec4 ColorF;\n"
  "varying vec4 ColorB;\n"
  "varying vec3 TexCoord;\n"
  "\n";
static const GLchar *poly_vertex_shader_attribs_3_0 =
  "in vec3 VertexPosition;\n"
  "in vec3 VertexNormal;\n"
  "in vec4 VertexColorF;\n"
  "in vec4 VertexColorB;\n"
  "in vec3 VertexTexCoord;\n"
  "\n"
  "out vec3 Normal;\n"
  "out vec4 ColorF;\n"
  "out vec4 ColorB;\n"
  "out vec3 TexCoord;\n"
  "\n";
static const GLchar *poly_vertex_shader_main =
  "uniform mat4 MatModelView;\n"
  "uniform mat4 MatProj;\n"
  "uniform bool BoolTextures;\n"
  "\n"
  "void main (void)\n"
  "{\n"
  "  ColorF = VertexColorF;\n"
  "  ColorB = VertexColorB;\n"
  "  Normal = normalize(MatModelView*vec4(VertexNormal,0.0f)).xyz;\n"
  "  vec4 Position = MatModelView*vec4(VertexPosition,1.0f);\n"
  "  gl_Position = MatProj*Position;\n"
  "  if (BoolTextures)\n"
  "    TexCoord = VertexTexCoord;\n"
  "}\n";

/* The fragment shader code is composed of code fragments that depend on
   the OpenGL version and code fragments that are version-independent.
   They are concatenated by glsl_CompileAndLinkShaders in the function
   init_glsl(). */
static const GLchar *poly_fragment_shader_attribs_2_1 =
  "varying vec3 Normal;\n"
  "varying vec4 ColorF;\n"
  "varying vec4 ColorB;\n"
  "varying vec3 TexCoord;\n"
  "\n";
static const GLchar *poly_fragment_shader_attribs_3_0 =
  "in vec3 Normal;\n"
  "in vec4 ColorF;\n"
  "in vec4 ColorB;\n"
  "in vec3 TexCoord;\n"
  "\n"
  "out vec4 FragColor;\n"
  "\n";
static const GLchar *poly_fragment_shader_main =
  "const float M_PI_F  = 3.14159265359f;\n"
  "const float M_2PI_F = 6.28318530718f;\n"
  "uniform vec4 LtGlblAmbient;\n"
  "uniform vec4 LtAmbient, LtDiffuse, LtSpecular;\n"
  "uniform vec3 LtDirection, LtHalfVector;\n"
  "uniform vec4 MatFrontAmbient, MatBackAmbient;\n"
  "uniform vec4 MatFrontDiffuse, MatBackDiffuse;\n"
  "uniform vec4 MatSpecular;\n"
  "uniform float MatShininess;\n"
  "uniform bool BoolTextures;\n"
  "uniform sampler2D TextureSamplerFront;\n"
  "uniform sampler2D TextureSamplerBack;\n"
  "uniform sampler2D TextureSamplerWater;\n"
  "\n"
  "void main (void)\n"
  "{\n"
  "  vec3 normalDirection;\n"
  "  vec4 ambientColor, diffuseColor, sceneColor;\n"
  "  vec4 ambientLighting, diffuseReflection, specularReflection, color;\n"
  "  vec3 normTexCoord\n;"
  "  vec2 texCoor;\n"
  "  float ndotl, ndoth, pf, lat, lon;\n"
  "  \n"
  "  if (gl_FrontFacing)\n"
  "  {\n"
  "    normalDirection = normalize(Normal);\n"
  "    sceneColor = ColorF*MatFrontAmbient*LtGlblAmbient;\n"
  "    ambientColor = ColorF*MatFrontAmbient;\n"
  "    diffuseColor = ColorF*MatFrontDiffuse;\n"
  "  }\n"
  "  else\n"
  "  {\n"
  "    normalDirection = -normalize(Normal);\n"
  "    sceneColor = ColorB*MatBackAmbient*LtGlblAmbient;\n"
  "    ambientColor = ColorB*MatBackAmbient;\n"
  "    diffuseColor = ColorB*MatBackDiffuse;\n"
  "  }\n"
  "  \n"
  "  ndotl = max(0.0f,dot(normalDirection,LtDirection));\n"
  "  ndoth = max(0.0f,dot(normalDirection,LtHalfVector));\n"
  "  if (ndotl == 0.0f)\n"
  "    pf = 0.0f;\n"
  "  else\n"
  "    pf = pow(ndoth,MatShininess);\n"
  "  ambientLighting = ambientColor*LtAmbient;\n"
  "  diffuseReflection = LtDiffuse*diffuseColor*ndotl;\n"
  "  specularReflection = LtSpecular*MatSpecular*pf;\n"
  "  color = sceneColor+ambientLighting+diffuseReflection;\n";
static const GLchar *poly_fragment_shader_out_2_1 =
  "  if (BoolTextures)\n"
  "  {\n"
  "    normTexCoord = normalize(TexCoord);\n"
  "    lat = asin(normTexCoord.z);\n"
  "    lon = atan(normTexCoord.y,normTexCoord.x);\n"
  "    texCoor = vec2(0.5f+lon/M_2PI_F,0.5f+lat/M_PI_F);\n"
  "    if (gl_FrontFacing)\n"
  "    {\n"
  "      color *= texture2D(TextureSamplerFront,texCoor);\n"
  "      specularReflection *= texture2D(TextureSamplerWater,texCoor);\n"
  "    }\n"
  "    else\n"
  "    {\n"
  "      color *= texture2D(TextureSamplerBack,texCoor);\n"
  "      specularReflection *= 0.0f;\n"
  "    }\n"
  "  }\n"
  "  color += specularReflection;\n"
  "  gl_FragColor = clamp(color,0.0f,1.0f);\n"
  "  gl_FragColor.a = diffuseColor.a;\n"
  "}\n";
static const GLchar *poly_fragment_shader_out_3_0 =
  "  if (BoolTextures)\n"
  "  {\n"
  "    normTexCoord = normalize(TexCoord);\n"
  "    lat = asin(normTexCoord.z);\n"
  "    lon = atan(normTexCoord.y,normTexCoord.x);\n"
  "    texCoor = vec2(0.5f+lon/M_2PI_F,0.5f+lat/M_PI_F);\n"
  "    if (gl_FrontFacing)\n"
  "    {\n"
  "      color *= texture(TextureSamplerFront,texCoor);\n"
  "      specularReflection *= texture(TextureSamplerWater,texCoor);\n"
  "    }\n"
  "    else\n"
  "    {\n"
  "      color *= texture(TextureSamplerBack,texCoor);\n"
  "      specularReflection *= 0.0f;\n"
  "    }\n"
  "  }\n"
  "  color += specularReflection;\n"
  "  FragColor = clamp(color,0.0f,1.0f);\n"
  "  FragColor.a = diffuseColor.a;\n"
  "}\n";


/* The vertex shader code is composed of code fragments that depend on
   the OpenGL version and code fragments that are version-independent.
   They are concatenated by glsl_CompileAndLinkShaders in the function
   init_glsl(). */
static const GLchar *dp_init_vertex_shader_attribs_2_1 =
  "attribute vec3 VertexPosition;\n"
  "\n";
static const GLchar *dp_init_vertex_shader_attribs_3_0 =
  "in vec3 VertexPosition;\n"
  "\n";
static const GLchar *dp_init_vertex_shader_main =
  "uniform mat4 MatModelView;\n"
  "uniform mat4 MatProj;\n"
  "\n"
  "void main (void)\n"
  "{\n"
  "  vec4 Position = MatModelView*vec4(VertexPosition,1.0f);\n"
  "  gl_Position = MatProj*Position;\n"
  "}\n";

/* The fragment shader code is composed of code fragments that depend on
   the OpenGL version and code fragments that are version-independent.
   They are concatenated by glsl_CompileAndLinkShaders in the function
   init_glsl(). */
static const GLchar *dp_init_fragment_shader_attribs_2_1 =
  "";
static const GLchar *dp_init_fragment_shader_attribs_3_0 =
  "out vec2 FragColor;\n"
  "\n";
static const GLchar *dp_init_fragment_shader_main =
  "const float MAX_DEPTH = 1.0f;\n"
  "uniform vec2 TexScale;\n"
  "uniform sampler2D TexSamplerOpaqueDepth;\n"
  "\n"
  "void main (void)\n"
  "{\n"
  "  vec2 texCoord = gl_FragCoord.xy*TexScale;\n";
static const GLchar *dp_init_fragment_shader_out_2_1 =
  "  float opaqueDepth = texture2D(TexSamplerOpaqueDepth,texCoord.st).x;\n"
  "  if (opaqueDepth != 1.0f && gl_FragCoord.z > opaqueDepth)\n"
  "    gl_FragColor.rg = vec2(-MAX_DEPTH,opaqueDepth);\n"
  "  else\n"
  "    gl_FragColor.rg = vec2(-gl_FragCoord.z,gl_FragCoord.z);\n"
  "}\n";
static const GLchar *dp_init_fragment_shader_out_3_0 =
  "  float opaqueDepth = texture(TexSamplerOpaqueDepth,texCoord.st).x;\n"
  "  if (opaqueDepth != 1.0f && gl_FragCoord.z > opaqueDepth)\n"
  "    FragColor = vec2(-MAX_DEPTH,opaqueDepth);\n"
  "  else\n"
  "    FragColor = vec2(-gl_FragCoord.z,gl_FragCoord.z);\n"
  "}\n";


/* The vertex shader code is composed of code fragments that depend on
   the OpenGL version and code fragments that are version-independent.
   They are concatenated by glShaderSource in the function init_glsl(). */
static const GLchar *dp_peel_vertex_shader_attribs_2_1 =
  "attribute vec3 VertexPosition;\n"
  "attribute vec3 VertexNormal;\n"
  "attribute vec4 VertexColorF;\n"
  "attribute vec4 VertexColorB;\n"
  "attribute vec3 VertexTexCoord;\n"
  "\n"
  "varying vec3 Normal;\n"
  "varying vec4 ColorF;\n"
  "varying vec4 ColorB;\n"
  "varying vec3 TexCoord;\n"
  "\n";
static const GLchar *dp_peel_vertex_shader_attribs_3_0 =
  "in vec3 VertexPosition;\n"
  "in vec3 VertexNormal;\n"
  "in vec4 VertexColorF;\n"
  "in vec4 VertexColorB;\n"
  "in vec3 VertexTexCoord;\n"
  "\n"
  "out vec3 Normal;\n"
  "out vec4 ColorF;\n"
  "out vec4 ColorB;\n"
  "out vec3 TexCoord;\n"
  "\n";
static const GLchar *dp_peel_vertex_shader_main =
  "uniform mat4 MatModelView;\n"
  "uniform mat4 MatProj;\n"
  "uniform bool BoolTextures;\n"
  "\n"
  "void main (void)\n"
  "{\n"
  "  ColorF = VertexColorF;\n"
  "  ColorB = VertexColorB;\n"
  "  Normal = normalize(MatModelView*vec4(VertexNormal,0.0f)).xyz;\n"
  "  vec4 Position = MatModelView*vec4(VertexPosition,1.0f);\n"
  "  gl_Position = MatProj*Position;\n"
  "  if (BoolTextures)\n"
  "    TexCoord = VertexTexCoord;\n"
  "}\n";

/* The fragment shader code is composed of code fragments that depend on
   the OpenGL version and code fragments that are version-independent.
   They are concatenated by glsl_CompileAndLinkShaders in the function
   init_glsl(). */
static const GLchar *dp_peel_fragment_shader_attribs_2_1 =
  "varying vec3 Normal;\n"
  "varying vec4 ColorF;\n"
  "varying vec4 ColorB;\n"
  "varying vec3 TexCoord;\n"
  "\n";
static const GLchar *dp_peel_fragment_shader_attribs_3_0 =
  "in vec3 Normal;\n"
  "in vec4 ColorF;\n"
  "in vec4 ColorB;\n"
  "in vec3 TexCoord;\n"
  "\n";
static const GLchar *dp_peel_fragment_shader_attribs_3_0_es =
  "in vec3 Normal;\n"
  "in vec4 ColorF;\n"
  "in vec4 ColorB;\n"
  "in vec3 TexCoord;\n"
  "\n"
  "layout(location = 0) out vec4 FragData0;\n"
  "layout(location = 1) out vec4 FragData1;\n"
  "layout(location = 2) out vec4 FragData2;\n"
  "\n";
static const GLchar *dp_peel_fragment_shader_main =
  "const float M_PI_F  = 3.14159265359f;\n"
  "const float M_2PI_F = 6.28318530718f;\n"
  "const float MAX_DEPTH = 1.0f;\n"
  "const float EPS = 1.0e-7f;\n"
  "uniform vec4 LtGlblAmbient;\n"
  "uniform vec4 LtAmbient, LtDiffuse, LtSpecular;\n"
  "uniform vec3 LtDirection, LtHalfVector;\n"
  "uniform vec4 MatFrontAmbient, MatBackAmbient;\n"
  "uniform vec4 MatFrontDiffuse, MatBackDiffuse;\n"
  "uniform vec4 MatSpecular;\n"
  "uniform float MatShininess;\n"
  "uniform bool BoolTextures;\n"
  "uniform vec2 TexScale;\n"
  "uniform sampler2D TexSamplerDepthBlender;\n"
  "uniform sampler2D TexSamplerFrontBlender;\n"
  "uniform sampler2D TexSamplerFront;\n"
  "uniform sampler2D TexSamplerBack;\n"
  "uniform sampler2D TexSamplerWater;\n"
  "\n"
  "void main (void)\n"
  "{\n"
  "  float fragDepth, nearestDepth, farthestDepth, alphaMultiplier;\n"
  "  vec2 depthBlender, peelTexCoord;\n"
  "  vec4 forwardTemp;\n"
  "  \n"
  "  vec3 normalDirection;\n"
  "  vec4 ambientColor, diffuseColor, sceneColor;\n"
  "  vec4 ambientLighting, diffuseReflection, specularReflection, color;\n"
  "  vec3 normTexCoord\n;"
  "  vec2 texCoor;\n"
  "  float ndotl, ndoth, pf, lat, lon;\n"
  "  \n";
static const GLchar *dp_peel_fragment_shader_peel_2_1 =
  "  peelTexCoord = gl_FragCoord.xy*TexScale;\n"
  "  fragDepth = gl_FragCoord.z;\n"
  "  depthBlender = texture2D(TexSamplerDepthBlender,peelTexCoord.st).xy;\n"
  "  forwardTemp = texture2D(TexSamplerFrontBlender,peelTexCoord.st);\n"
  "  gl_FragData[0].xy = depthBlender;\n"
  "  gl_FragData[1] = forwardTemp;\n"
  "  gl_FragData[2] = vec4(0.0f);\n"
  "  nearestDepth = -depthBlender.x;\n"
  "  farthestDepth = depthBlender.y;\n"
  "  alphaMultiplier = 1.0f-forwardTemp.a;\n"
  "  if (fragDepth < nearestDepth-EPS || fragDepth > farthestDepth+EPS)\n"
  "  {\n"
  "    gl_FragData[0].xy = vec2(-MAX_DEPTH);\n"
  "  }\n"
  "  else if (fragDepth > nearestDepth+EPS && fragDepth < farthestDepth-EPS)\n"
  "  {\n"
  "    gl_FragData[0].xy = vec2(-fragDepth,fragDepth);\n"
  "  }\n"
  "  else\n"
  "  {\n";
static const GLchar *dp_peel_fragment_shader_peel_3_0 =
  "  peelTexCoord = gl_FragCoord.xy*TexScale;\n"
  "  fragDepth = gl_FragCoord.z;\n"
  "  depthBlender = texture(TexSamplerDepthBlender,peelTexCoord.st).xy;\n"
  "  forwardTemp = texture(TexSamplerFrontBlender,peelTexCoord.st);\n"
  "  gl_FragData[0].xy = depthBlender;\n"
  "  gl_FragData[1] = forwardTemp;\n"
  "  gl_FragData[2] = vec4(0.0f);\n"
  "  nearestDepth = -depthBlender.x;\n"
  "  farthestDepth = depthBlender.y;\n"
  "  alphaMultiplier = 1.0f-forwardTemp.a;\n"
  "  if (fragDepth < nearestDepth-EPS || fragDepth > farthestDepth+EPS)\n"
  "  {\n"
  "    gl_FragData[0].xy = vec2(-MAX_DEPTH);\n"
  "  }\n"
  "  else if (fragDepth > nearestDepth+EPS && fragDepth < farthestDepth-EPS)\n"
  "  {\n"
  "    gl_FragData[0].xy = vec2(-fragDepth,fragDepth);\n"
  "  }\n"
  "  else\n"
  "  {\n";
static const GLchar *dp_peel_fragment_shader_peel_3_0_es =
  "  peelTexCoord = gl_FragCoord.xy*TexScale;\n"
  "  fragDepth = gl_FragCoord.z;\n"
  "  depthBlender = texture(TexSamplerDepthBlender,peelTexCoord.st).xy;\n"
  "  forwardTemp = texture(TexSamplerFrontBlender,peelTexCoord.st);\n"
  "  FragData0.xy = depthBlender;\n"
  "  FragData1 = forwardTemp;\n"
  "  FragData2 = vec4(0.0f);\n"
  "  nearestDepth = -depthBlender.x;\n"
  "  farthestDepth = depthBlender.y;\n"
  "  alphaMultiplier = 1.0f-forwardTemp.a;\n"
  "  if (fragDepth < nearestDepth-EPS || fragDepth > farthestDepth+EPS)\n"
  "  {\n"
  "    FragData0.xy = vec2(-MAX_DEPTH);\n"
  "  }\n"
  "  else if (fragDepth > nearestDepth+EPS && fragDepth < farthestDepth-EPS)\n"
  "  {\n"
  "    FragData0.xy = vec2(-fragDepth,fragDepth);\n"
  "  }\n"
  "  else\n"
  "  {\n";
static const GLchar *dp_peel_fragment_shader_col =
  "    if (gl_FrontFacing)\n"
  "    {\n"
  "      normalDirection = normalize(Normal);\n"
  "      sceneColor = ColorF*MatFrontAmbient*LtGlblAmbient;\n"
  "      ambientColor = ColorF*MatFrontAmbient;\n"
  "      diffuseColor = ColorF*MatFrontDiffuse;\n"
  "    }\n"
  "    else\n"
  "    {\n"
  "      normalDirection = -normalize(Normal);\n"
  "      sceneColor = ColorB*MatBackAmbient*LtGlblAmbient;\n"
  "      ambientColor = ColorB*MatBackAmbient;\n"
  "      diffuseColor = ColorB*MatBackDiffuse;\n"
  "    }\n"
  "    \n"
  "    ndotl = max(0.0f,dot(normalDirection,LtDirection));\n"
  "    ndoth = max(0.0f,dot(normalDirection,LtHalfVector));\n"
  "    if (ndotl == 0.0f)\n"
  "      pf = 0.0f;\n"
  "    else\n"
  "      pf = pow(ndoth,MatShininess);\n"
  "    ambientLighting = ambientColor*LtAmbient;\n"
  "    diffuseReflection = LtDiffuse*diffuseColor*ndotl;\n"
  "    specularReflection = LtSpecular*MatSpecular*pf;\n"
  "    color = sceneColor+ambientLighting+diffuseReflection;\n";
static const GLchar *dp_peel_fragment_shader_col_2_1 =
  "    if (BoolTextures)\n"
  "    {\n"
  "      normTexCoord = normalize(TexCoord);\n"
  "      lat = asin(normTexCoord.z);\n"
  "      lon = atan(normTexCoord.y,normTexCoord.x);\n"
  "      texCoor = vec2(0.5f+lon/M_2PI_F,0.5f+lat/M_PI_F);\n"
  "      if (gl_FrontFacing)\n"
  "      {\n"
  "        color *= texture2D(TexSamplerFront,texCoor);\n"
  "        specularReflection *= texture2D(TexSamplerWater,texCoor);\n"
  "      }\n"
  "      else\n"
  "      {\n"
  "        color *= texture2D(TexSamplerBack,texCoor);\n"
  "        specularReflection *= 0.0f;\n"
  "      }\n"
  "    }\n"
  "    color += specularReflection;\n"
  "    color = clamp(color,0.0f,1.0f);\n"
  "    color.a = diffuseColor.a;\n"
  "    \n";
static const GLchar *dp_peel_fragment_shader_col_3_0 =
  "    if (BoolTextures)\n"
  "    {\n"
  "      normTexCoord = normalize(TexCoord);\n"
  "      lat = asin(normTexCoord.z);\n"
  "      lon = atan(normTexCoord.y,normTexCoord.x);\n"
  "      texCoor = vec2(0.5f+lon/M_2PI_F,0.5f+lat/M_PI_F);\n"
  "      if (gl_FrontFacing)\n"
  "      {\n"
  "        color *= texture(TexSamplerFront,texCoor);\n"
  "        specularReflection *= texture(TexSamplerWater,texCoor);\n"
  "      }\n"
  "      else\n"
  "      {\n"
  "        color *= texture(TexSamplerBack,texCoor);\n"
  "        specularReflection *= 0.0f;\n"
  "      }\n"
  "    }\n"
  "    color += specularReflection;\n"
  "    color = clamp(color,0.0f,1.0f);\n"
  "    color.a = diffuseColor.a;\n"
  "    \n";
static const GLchar *dp_peel_fragment_shader_out_2_1 =
  "    gl_FragData[0].xy = vec2(-MAX_DEPTH);\n"
  "    if (fragDepth >= nearestDepth-EPS && fragDepth <= nearestDepth+EPS)\n"
  "    {\n"
  "      gl_FragData[1].rgb += color.rgb*color.a*alphaMultiplier;\n"
  "      gl_FragData[1].a = 1.0f-alphaMultiplier*(1.0f-color.a);\n"
  "    }\n"
  "    else\n"
  "    {\n"
  "      gl_FragData[2] += color;\n"
  "    }\n"
  "  }\n"
  "}\n";
static const GLchar *dp_peel_fragment_shader_out_3_0 =
  "    gl_FragData[0].xy = vec2(-MAX_DEPTH);\n"
  "    if (fragDepth >= nearestDepth-EPS && fragDepth <= nearestDepth+EPS)\n"
  "    {\n"
  "      gl_FragData[1].rgb += color.rgb*color.a*alphaMultiplier;\n"
  "      gl_FragData[1].a = 1.0f-alphaMultiplier*(1.0f-color.a);\n"
  "    }\n"
  "    else\n"
  "    {\n"
  "      gl_FragData[2] += color;\n"
  "    }\n"
  "  }\n"
  "}\n";
static const GLchar *dp_peel_fragment_shader_out_3_0_es =
  "    FragData0.xy = vec2(-MAX_DEPTH);\n"
  "    if (fragDepth >= nearestDepth-EPS && fragDepth <= nearestDepth+EPS)\n"
  "    {\n"
  "      FragData1.rgb += color.rgb*color.a*alphaMultiplier;\n"
  "      FragData1.a = 1.0f-alphaMultiplier*(1.0f-color.a);\n"
  "    }\n"
  "    else\n"
  "    {\n"
  "      FragData2 += color;\n"
  "    }\n"
  "  }\n"
  "}\n";


/* The vertex shader code is composed of code fragments that depend on
   the OpenGL version and code fragments that are version-independent.
   They are concatenated by glsl_CompileAndLinkShaders in the function
   init_glsl(). */
static const GLchar *dp_blnd_vertex_shader_attribs_2_1 =
  "attribute vec3 VertexPosition;\n"
  "\n";
static const GLchar *dp_blnd_vertex_shader_attribs_3_0 =
  "in vec3 VertexPosition;\n"
  "\n";
static const GLchar *dp_blnd_vertex_shader_main =
  "void main (void)\n"
  "{\n"
  "  gl_Position = vec4(VertexPosition,1.0f);\n"
  "}\n";

/* The fragment shader code is composed of code fragments that depend on
   the OpenGL version and code fragments that are version-independent.
   They are concatenated by glsl_CompileAndLinkShaders in the function
   init_glsl(). */
static const GLchar *dp_blnd_fragment_shader_attribs_2_1 =
  "";
static const GLchar *dp_blnd_fragment_shader_attribs_3_0 =
  "out vec4 FragColor;\n"
  "\n";
static const GLchar *dp_blnd_fragment_shader_main =
  "uniform vec2 TexScale;\n"
  "uniform sampler2D TexSamplerTemp;\n"
  "\n"
  "void main (void)\n"
  "{\n"
  "  vec2 TexCoord = gl_FragCoord.xy*TexScale;\n";
static const GLchar *dp_blnd_fragment_shader_out_2_1 =
  "  gl_FragColor = texture2D(TexSamplerTemp,TexCoord.st);\n"
  "  if (gl_FragColor.a == 0.0f)\n"
  "    discard;\n"
  "}\n";
static const GLchar *dp_blnd_fragment_shader_out_3_0 =
  "  FragColor = texture(TexSamplerTemp,TexCoord.st);\n"
  "  if (FragColor.a == 0.0f)\n"
  "    discard;\n"
  "}\n";


/* The vertex shader code is composed of code fragments that depend on
   the OpenGL version and code fragments that are version-independent.
   They are concatenated by glsl_CompileAndLinkShaders in the function
   init_glsl(). */
static const GLchar *dp_finl_vertex_shader_attribs_2_1 =
  "attribute vec3 VertexPosition;\n"
  "\n";
static const GLchar *dp_finl_vertex_shader_attribs_3_0 =
  "in vec3 VertexPosition;\n"
  "\n";
static const GLchar *dp_finl_vertex_shader_main =
  "void main (void)\n"
  "{\n"
  "  gl_Position = vec4(VertexPosition,1.0f);\n"
  "}\n";

/* The fragment shader code is composed of code fragments that depend on
   the OpenGL version and code fragments that are version-independent.
   They are concatenated by glsl_CompileAndLinkShaders in the function
   init_glsl(). */
static const GLchar *dp_finl_fragment_shader_attribs_2_1 =
  "";
static const GLchar *dp_finl_fragment_shader_attribs_3_0 =
  "out vec4 FragColor;\n"
  "\n";
static const GLchar *dp_finl_fragment_shader_main =
  "uniform vec2 TexScale;\n"
  "uniform sampler2D TexSamplerFrontBlender;\n"
  "uniform sampler2D TexSamplerBackBlender;\n"
  "uniform sampler2D TexSamplerOpaqueBlender;\n"
  "\n"
  "void main (void)\n"
  "{\n"
  "  vec4 transColor;\n"
  "  vec2 TexCoord = gl_FragCoord.xy*TexScale;\n";
static const GLchar *dp_finl_fragment_shader_out_2_1 =
  "  vec4 frontColor = texture2D(TexSamplerFrontBlender,TexCoord.st);\n"
  "  vec4 backColor = texture2D(TexSamplerBackBlender,TexCoord.st);\n"
  "  vec4 opaqueColor = texture2D(TexSamplerOpaqueBlender,TexCoord.st);\n"
  "  frontColor.a = 1.0f-frontColor.a;\n"
  "  transColor.rgb = frontColor.rgb+backColor.rgb*frontColor.a;\n"
  "  transColor.a = (1.0f-frontColor.a*(1.0f-backColor.a));\n"
  "  gl_FragColor.rgb = transColor.rgb+opaqueColor.rgb*(1.0f-transColor.a);\n"
  "  gl_FragColor.a = transColor.a+opaqueColor.a*(1.0f-transColor.a);\n"
  "}\n";
static const GLchar *dp_finl_fragment_shader_out_3_0 =
  "  vec4 frontColor = texture(TexSamplerFrontBlender,TexCoord.st);\n"
  "  vec4 backColor = texture(TexSamplerBackBlender,TexCoord.st);\n"
  "  vec4 opaqueColor = texture(TexSamplerOpaqueBlender,TexCoord.st);\n"
  "  frontColor.a = 1.0f-frontColor.a;\n"
  "  transColor.rgb = frontColor.rgb+backColor.rgb*frontColor.a;\n"
  "  transColor.a = (1.0f-frontColor.a*(1.0f-backColor.a));\n"
  "  FragColor.rgb = transColor.rgb+opaqueColor.rgb*(1.0f-transColor.a);\n"
  "  FragColor.a = transColor.a+opaqueColor.a*(1.0f-transColor.a);\n"
  "}\n";


/* The vertex shader code is composed of code fragments that depend on
   the OpenGL version and code fragments that are version-independent.
   They are concatenated by glShaderSource in the function init_glsl(). */
static const GLchar *wa_init_vertex_shader_attribs_2_1 =
  "attribute vec3 VertexPosition;\n"
  "attribute vec3 VertexNormal;\n"
  "attribute vec4 VertexColorF;\n"
  "attribute vec4 VertexColorB;\n"
  "attribute vec3 VertexTexCoord;\n"
  "\n"
  "varying vec3 Normal;\n"
  "varying vec4 ColorF;\n"
  "varying vec4 ColorB;\n"
  "varying vec3 TexCoord;\n"
  "\n";
static const GLchar *wa_init_vertex_shader_attribs_3_0 =
  "in vec3 VertexPosition;\n"
  "in vec3 VertexNormal;\n"
  "in vec4 VertexColorF;\n"
  "in vec4 VertexColorB;\n"
  "in vec3 VertexTexCoord;\n"
  "\n"
  "out vec3 Normal;\n"
  "out vec4 ColorF;\n"
  "out vec4 ColorB;\n"
  "out vec3 TexCoord;\n"
  "\n";
static const GLchar *wa_init_vertex_shader_main =
  "uniform mat4 MatModelView;\n"
  "uniform mat4 MatProj;\n"
  "uniform bool BoolTextures;\n"
  "\n"
  "void main (void)\n"
  "{\n"
  "  ColorF = VertexColorF;\n"
  "  ColorB = VertexColorB;\n"
  "  Normal = normalize(MatModelView*vec4(VertexNormal,0.0f)).xyz;\n"
  "  vec4 Position = MatModelView*vec4(VertexPosition,1.0f);\n"
  "  gl_Position = MatProj*Position;\n"
  "  if (BoolTextures)\n"
  "    TexCoord = VertexTexCoord;\n"
  "}\n";

/* The fragment shader code is composed of code fragments that depend on
   the OpenGL version and code fragments that are version-independent.
   They are concatenated by glsl_CompileAndLinkShaders in the function
   init_glsl(). */
static const GLchar *wa_init_fragment_shader_attribs_2_1 =
  "varying vec3 Normal;\n"
  "varying vec4 ColorF;\n"
  "varying vec4 ColorB;\n"
  "varying vec3 TexCoord;\n"
  "\n";
static const GLchar *wa_init_fragment_shader_attribs_3_0 =
  "in vec3 Normal;\n"
  "in vec4 ColorF;\n"
  "in vec4 ColorB;\n"
  "in vec3 TexCoord;\n"
  "\n";
static const GLchar *wa_init_fragment_shader_attribs_3_0_es =
  "in vec3 Normal;\n"
  "in vec4 ColorF;\n"
  "in vec4 ColorB;\n"
  "in vec3 TexCoord;\n"
  "\n"
  "layout(location = 0) out vec4 FragData0;\n"
  "layout(location = 1) out vec4 FragData1;\n"
  "\n";
static const GLchar *wa_init_fragment_shader_main =
  "const float M_PI_F  = 3.14159265359f;\n"
  "const float M_2PI_F = 6.28318530718f;\n"
  "uniform vec4 LtGlblAmbient;\n"
  "uniform vec4 LtAmbient, LtDiffuse, LtSpecular;\n"
  "uniform vec3 LtDirection, LtHalfVector;\n"
  "uniform vec4 MatFrontAmbient, MatBackAmbient;\n"
  "uniform vec4 MatFrontDiffuse, MatBackDiffuse;\n"
  "uniform vec4 MatSpecular;\n"
  "uniform float MatShininess;\n"
  "uniform bool BoolTextures;\n"
  "uniform sampler2D TextureSamplerFront;\n"
  "uniform sampler2D TextureSamplerBack;\n"
  "uniform sampler2D TextureSamplerWater;\n"
  "\n"
  "void main (void)\n"
  "{\n"
  "  vec3 normalDirection;\n"
  "  vec4 ambientColor, diffuseColor, sceneColor;\n"
  "  vec4 ambientLighting, diffuseReflection, specularReflection, color;\n"
  "  vec3 normTexCoord\n;"
  "  vec2 texCoor;\n"
  "  float ndotl, ndoth, pf, lat, lon;\n"
  "  \n"
  "  if (gl_FrontFacing)\n"
  "  {\n"
  "    normalDirection = normalize(Normal);\n"
  "    sceneColor = ColorF*MatFrontAmbient*LtGlblAmbient;\n"
  "    ambientColor = ColorF*MatFrontAmbient;\n"
  "    diffuseColor = ColorF*MatFrontDiffuse;\n"
  "  }\n"
  "  else\n"
  "  {\n"
  "    normalDirection = -normalize(Normal);\n"
  "    sceneColor = ColorB*MatBackAmbient*LtGlblAmbient;\n"
  "    ambientColor = ColorB*MatBackAmbient;\n"
  "    diffuseColor = ColorB*MatBackDiffuse;\n"
  "  }\n"
  "  \n"
  "  ndotl = max(0.0f,dot(normalDirection,LtDirection));\n"
  "  ndoth = max(0.0f,dot(normalDirection,LtHalfVector));\n"
  "  if (ndotl == 0.0f)\n"
  "    pf = 0.0f;\n"
  "  else\n"
  "    pf = pow(ndoth,MatShininess);\n"
  "  ambientLighting = ambientColor*LtAmbient;\n"
  "  diffuseReflection = LtDiffuse*diffuseColor*ndotl;\n"
  "  specularReflection = LtSpecular*MatSpecular*pf;\n"
  "  color = sceneColor+ambientLighting+diffuseReflection;\n";
static const GLchar *wa_init_fragment_shader_out_2_1 =
  "  if (BoolTextures)\n"
  "  {\n"
  "    normTexCoord = normalize(TexCoord);\n"
  "    lat = asin(normTexCoord.z);\n"
  "    lon = atan(normTexCoord.y,normTexCoord.x);\n"
  "    texCoor = vec2(0.5f+lon/M_2PI_F,0.5f+lat/M_PI_F);\n"
  "    if (gl_FrontFacing)\n"
  "    {\n"
  "      color *= texture2D(TextureSamplerFront,texCoor);\n"
  "      specularReflection *= texture2D(TextureSamplerWater,texCoor);\n"
  "    }\n"
  "    else\n"
  "    {\n"
  "      color *= texture2D(TextureSamplerBack,texCoor);\n"
  "      specularReflection *= 0.0f;\n"
  "    }\n"
  "  }\n"
  "  color += specularReflection;\n"
  "  color = clamp(color,0.0f,1.0f);\n"
  "  color.a = diffuseColor.a;\n"
  "  gl_FragData[0] = vec4(color.rgb*color.a,color.a);\n"
  "  gl_FragData[1] = vec4(1.0f);\n"
  "}\n";
static const GLchar *wa_init_fragment_shader_out_3_0 =
  "  if (BoolTextures)\n"
  "  {\n"
  "    normTexCoord = normalize(TexCoord);\n"
  "    lat = asin(normTexCoord.z);\n"
  "    lon = atan(normTexCoord.y,normTexCoord.x);\n"
  "    texCoor = vec2(0.5f+lon/M_2PI_F,0.5f+lat/M_PI_F);\n"
  "    if (gl_FrontFacing)\n"
  "    {\n"
  "      color *= texture(TextureSamplerFront,texCoor);\n"
  "      specularReflection *= texture(TextureSamplerWater,texCoor);\n"
  "    }\n"
  "    else\n"
  "    {\n"
  "      color *= texture(TextureSamplerBack,texCoor);\n"
  "      specularReflection *= 0.0f;\n"
  "    }\n"
  "  }\n"
  "  color += specularReflection;\n"
  "  color = clamp(color,0.0f,1.0f);\n"
  "  color.a = diffuseColor.a;\n"
  "  gl_FragData[0] = vec4(color.rgb*color.a,color.a);\n"
  "  gl_FragData[1] = vec4(1.0f);\n"
  "}\n";
static const GLchar *wa_init_fragment_shader_out_3_0_es =
  "  if (BoolTextures)\n"
  "  {\n"
  "    normTexCoord = normalize(TexCoord);\n"
  "    lat = asin(normTexCoord.z);\n"
  "    lon = atan(normTexCoord.y,normTexCoord.x);\n"
  "    texCoor = vec2(0.5f+lon/M_2PI_F,0.5f+lat/M_PI_F);\n"
  "    if (gl_FrontFacing)\n"
  "    {\n"
  "      color *= texture(TextureSamplerFront,texCoor);\n"
  "      specularReflection *= texture(TextureSamplerWater,texCoor);\n"
  "    }\n"
  "    else\n"
  "    {\n"
  "      color *= texture(TextureSamplerBack,texCoor);\n"
  "      specularReflection *= 0.0f;\n"
  "    }\n"
  "  }\n"
  "  color += specularReflection;\n"
  "  color = clamp(color,0.0f,1.0f);\n"
  "  color.a = diffuseColor.a;\n"
  "  FragData0 = vec4(color.rgb*color.a,color.a);\n"
  "  FragData1 = vec4(1.0f);\n"
  "}\n";


/* The vertex shader code is composed of code fragments that depend on
   the OpenGL version and code fragments that are version-independent.
   They are concatenated by glsl_CompileAndLinkShaders in the function
   init_glsl(). */
static const GLchar *wa_finl_vertex_shader_attribs_2_1 =
  "attribute vec3 VertexPosition;\n"
  "\n";
static const GLchar *wa_finl_vertex_shader_attribs_3_0 =
  "in vec3 VertexPosition;\n"
  "\n";
static const GLchar *wa_finl_vertex_shader_main =
  "void main (void)\n"
  "{\n"
  "  gl_Position = vec4(VertexPosition,1.0f);\n"
  "}\n";

/* The fragment shader code is composed of code fragments that depend on
   the OpenGL version and code fragments that are version-independent.
   They are concatenated by glsl_CompileAndLinkShaders in the function
   init_glsl(). */
static const GLchar *wa_finl_fragment_shader_attribs_2_1 =
  "";
static const GLchar *wa_finl_fragment_shader_attribs_3_0 =
  "out vec4 FragColor;\n"
  "\n";
static const GLchar *wa_finl_fragment_shader_main =
  "uniform vec2 TexScale;\n"
  "uniform sampler2D TexSamplerColorBlender;\n"
  "uniform sampler2D TexSamplerNumBlender;\n"
  "uniform sampler2D TexSamplerOpaqueBlender;\n"
  "\n"
  "void main (void)\n"
  "{\n"
  "  vec2 TexCoord = gl_FragCoord.xy*TexScale;\n";
static const GLchar *wa_finl_fragment_shader_out_2_1 =
  "  vec4 backColor = texture2D(TexSamplerOpaqueBlender,TexCoord.st);\n"
  "  vec4 sumColor = texture2D(TexSamplerColorBlender,TexCoord.st);\n"
  "  float n = texture2D(TexSamplerNumBlender,TexCoord.st).r;\n"
  "  if (n == 0.0)\n"
  "  {\n"
  "    gl_FragColor = backColor;\n"
  "  }\n"
  "  else\n"
  "  {\n"
  "    vec3 avgColor = sumColor.rgb/sumColor.a;\n"
  "    float avgAlpha = sumColor.a/n;\n"
  "    float t = pow(1.0f-avgAlpha,n);\n"
  "    gl_FragColor.rgb = avgColor*(1.0f-t)+backColor.rgb*t;\n"
  "    gl_FragColor.a = 1.0f-t;\n"
  "  }\n"
  "}\n";
static const GLchar *wa_finl_fragment_shader_out_3_0 =
  "  vec4 backColor = texture(TexSamplerOpaqueBlender,TexCoord.st);\n"
  "  vec4 sumColor = texture(TexSamplerColorBlender,TexCoord.st);\n"
  "  float n = texture(TexSamplerNumBlender,TexCoord.st).r;\n"
  "  if (n == 0.0)\n"
  "  {\n"
  "    FragColor = backColor;\n"
  "  }\n"
  "  else\n"
  "  {\n"
  "    vec3 avgColor = sumColor.rgb/sumColor.a;\n"
  "    float avgAlpha = sumColor.a/n;\n"
  "    float t = pow(1.0f-avgAlpha,n);\n"
  "    FragColor.rgb = avgColor*(1.0f-t)+backColor.rgb*t;\n"
  "    FragColor.a = 1.0f-t;\n"
  "  }\n"
  "}\n";


#endif /* HAVE_GLSL */


/* ------------------------------------------------------------------------- */


static void sub(double dest[3], double v1[3], double v2[3])
{
  dest[0] = v1[0]-v2[0];
  dest[1] = v1[1]-v2[1];
  dest[2] = v1[2]-v2[2];
}


static void cross(double dest[3], double v1[3], double v2[3])
{
  dest[0] = v1[1]*v2[2]-v1[2]*v2[1];
  dest[1] = v1[2]*v2[0]-v1[0]*v2[2];
  dest[2] = v1[0]*v2[1]-v1[1]*v2[0];
}


static double dot(double v1[3], double v2[3])
{
  return v1[0]*v2[0]+v1[1]*v2[1]+v1[2]*v2[2];
}


static void scalar(double dest[3], double alpha, double v[3])
{
  dest[0] = alpha*v[0];
  dest[1] = alpha*v[1];
  dest[2] = alpha*v[2];
}


static Bool construct_intersection(double p1[3], double q1[3],
                                   double r1[3], double p2[3],
                                   double q2[3], double r2[3],
                                   double N1[3], double N2[3],
                                   double source[3], double target[3])
{
  double v1[3], v2[3], v[3];
  double N[3];
  double alpha;

  sub(v1,q1,p1);
  sub(v2,r2,p1);
  cross(N,v1,v2);
  sub(v,p2,p1);
  if (dot(v,N) > 0.0)
  {
    sub(v1,r1,p1);
    cross(N,v1,v2);
    if (dot(v,N) <= 0.0)
    {
      sub(v2,q2,p1);
      cross(N,v1,v2);
      if (dot(v,N) > 0.0)
      {
        sub(v1,p1,p2);
        sub(v2,p1,r1);
        alpha = dot(v1,N2)/dot(v2,N2);
        scalar(v1,alpha,v2);
        sub(source,p1,v1);
        sub(v1,p2,p1);
        sub(v2,p2,r2);
        alpha = dot(v1,N1)/dot(v2,N1);
        scalar(v1,alpha,v2);
        sub(target,p2,v1);
        return True;
      }
      else
      {
        sub(v1,p2,p1);
        sub(v2,p2,q2);
        alpha = dot(v1,N1)/dot(v2,N1);
        scalar(v1,alpha,v2);
        sub(source,p2,v1);
        sub(v1,p2,p1);
        sub(v2,p2,r2);
        alpha = dot(v1,N1)/dot(v2,N1);
        scalar(v1,alpha,v2);
        sub(target,p2,v1);
        return True;
      }
    }
    else
    {
      return False;
    }
  }
  else
  {
    sub(v2,q2,p1);
    cross(N,v1,v2);
    if (dot(v,N) < 0.0)
    {
      return False;
    }
    else
    {
      sub(v1,r1,p1);
      cross(N,v1,v2);
      if (dot(v,N) >= 0.0)
      {
        sub(v1,p1,p2);
        sub(v2,p1,r1);
        alpha = dot(v1,N2)/dot(v2,N2);
        scalar(v1,alpha,v2);
        sub(source,p1,v1);
        sub(v1,p1,p2);
        sub(v2,p1,q1);
        alpha = dot(v1,N2)/dot(v2,N2);
        scalar(v1,alpha,v2);
        sub(target,p1,v1);
        return True;
      }
      else
      {
        sub(v1,p2,p1);
        sub(v2,p2,q2);
        alpha = dot(v1,N1)/dot(v2,N1);
        scalar(v1,alpha,v2);
        sub(source,p2,v1);
        sub(v1,p1,p2);
        sub(v2,p1,q1);
        alpha = dot(v1,N2)/dot(v2,N2);
        scalar(v1,alpha,v2);
        sub(target,p1,v1);
        return True;
      }
    }
  }
}


static Bool tri_tri_inter_3d(double p1[3], double q1[3], double r1[3],
                             double p2[3], double q2[3], double r2[3],
                             double dp2, double dq2, double dr2,
                             double N1[3], double N2[3],
                             double source[3], double target[3])
{
  if (dp2 > 0.0)
  {
    if (dq2 > 0.0)
      return construct_intersection(p1,r1,q1,r2,p2,q2,N1,N2,source,target);
    else if (dr2 > 0.0)
      return construct_intersection(p1,r1,q1,q2,r2,p2,N1,N2,source,target);
    else
      return construct_intersection(p1,q1,r1,p2,q2,r2,N1,N2,source,target);
  }
  else if (dp2 < 0.0)
  {
    if (dq2 < 0.0)
      return construct_intersection(p1,q1,r1,r2,p2,q2,N1,N2,source,target);
    else if (dr2 < 0.0)
      return construct_intersection(p1,q1,r1,q2,r2,p2,N1,N2,source,target);
    else
      return construct_intersection(p1,r1,q1,p2,q2,r2,N1,N2,source,target);
  }
  else
  {
    if (dq2 < 0.0)
    {
      if (dr2 >= 0.0)
        return construct_intersection(p1,r1,q1,q2,r2,p2,N1,N2,source,target);
      else
        return construct_intersection(p1,q1,r1,p2,q2,r2,N1,N2,source,target);
    }
    else if (dq2 > 0.0)
    {
      if (dr2 > 0.0)
        return construct_intersection(p1,r1,q1,p2,q2,r2,N1,N2,source,target);
      else
        return construct_intersection(p1,q1,r1,q2,r2,p2,N1,N2,source,target);
    }
    else
    {
      if (dr2 > 0.0)
        return construct_intersection(p1,q1,r1,r2,p2,q2,N1,N2,source,target);
      else if (dr2 < 0.0)
        return construct_intersection(p1,r1,q1,r2,p2,q2,N1,N2,source,target);
      else
        return False;
    }
  }
}


static Bool tri_tri_intersection_3d(double p1[3], double q1[3],
                                    double r1[3], double p2[3],
                                    double q2[3], double r2[3],
                                    double source[3], double target[3])

{
  double dp1, dq1, dr1, dp2, dq2, dr2;
  double v1[3], v2[3];
  double N1[3], N2[3];

  sub(v1,p2,r2);
  sub(v2,q2,r2);
  cross(N2,v1,v2);

  sub(v1,p1,r2);
  dp1 = dot(v1,N2);
  sub(v1,q1,r2);
  dq1 = dot(v1,N2);
  sub(v1,r1,r2);
  dr1 = dot(v1,N2);

  if (dp1*dq1 > 0.0 && dp1*dr1 > 0.0)
    return False;

  sub(v1,q1,p1);
  sub(v2,r1,p1);
  cross(N1,v1,v2);

  sub(v1,p2,r1);
  dp2 = dot(v1,N1);
  sub(v1,q2,r1);
  dq2 = dot(v1,N1);
  sub(v1,r2,r1);
  dr2 = dot(v1,N1);

  if (dp2*dq2 > 0.0 && dp2*dr2 > 0.0)
    return False;

  if (dp1 > 0.0)
  {
    if (dq1 > 0.0)
      return tri_tri_inter_3d(r1,p1,q1,p2,r2,q2,dp2,dr2,dq2,N1,N2,
                              source,target);
    else if (dr1 > 0.0)
      return tri_tri_inter_3d(q1,r1,p1,p2,r2,q2,dp2,dr2,dq2,N1,N2,
                              source,target);
    else
      return tri_tri_inter_3d(p1,q1,r1,p2,q2,r2,dp2,dq2,dr2,N1,N2,
                              source,target);
  }
  else if (dp1 < 0.0)
  {
    if (dq1 < 0.0)
      return tri_tri_inter_3d(r1,p1,q1,p2,q2,r2,dp2,dq2,dr2,N1,N2,
                              source,target);
    else if (dr1 < 0.0)
      return tri_tri_inter_3d(q1,r1,p1,p2,q2,r2,dp2,dq2,dr2,N1,N2,
                              source,target);
    else
      return tri_tri_inter_3d(p1,q1,r1,p2,r2,q2,dp2,dr2,dq2,N1,N2,
                              source,target);
  }
  else
  {
    if (dq1 < 0.0)
    {
      if (dr1 >= 0.0)
        return tri_tri_inter_3d(q1,r1,p1,p2,r2,q2,dp2,dr2,dq2,N1,N2,
                                source,target);
      else
        return tri_tri_inter_3d(p1,q1,r1,p2,q2,r2,dp2,dq2,dr2,N1,N2,
                                source,target);
    }
    else if (dq1 > 0.0)
    {
      if (dr1 > 0.0)
        return tri_tri_inter_3d(p1,q1,r1,p2,r2,q2,dp2,dr2,dq2,N1,N2,
                                source,target);
      else
        return tri_tri_inter_3d(q1,r1,p1,p2,q2,r2,dp2,dq2,dr2,N1,N2,
                                source,target);
    }
    else
    {
      if (dr1 > 0.0)
        return tri_tri_inter_3d(r1,p1,q1,p2,q2,r2,dp2,dq2,dr2,N1,N2,
                                source,target);
      else if (dr1 < 0.0)
        return tri_tri_inter_3d(r1,p1,q1,p2,r2,q2,dp2,dr2,dq2,N1,N2,
                                source,target);
      else
        return False;
    }
  }
}


static void barycenter(const float x[3], const float y[3], const float z[3],
                       float c[3])
{
  c[0] = (x[0]+y[0]+z[0])/3.0f;
  c[1] = (x[1]+y[1]+z[1])/3.0f;
  c[2] = (x[2]+y[2]+z[2])/3.0f;
}


static void unit_normal(const float x[3], const float y[3], const float z[3],
                        float n[3])
{
  float a[3], b[3], l;

  a[0] = y[0]-x[0];
  a[1] = y[1]-x[1];
  a[2] = y[2]-x[2];
  b[0] = z[0]-x[0];
  b[1] = z[1]-x[1];
  b[2] = z[2]-x[2];
  n[0] = a[1]*b[2]-a[2]*b[1];
  n[1] = a[2]*b[0]-a[0]*b[2];
  n[2] = a[0]*b[1]-a[1]*b[0];
  l = sqrtf(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
  if (l > 0.0f)
  {
    l = 1.0f/l;
    n[0] *= l;
    n[1] *= l;
    n[2] *= l;
  }
  else
  {
    n[0] = 0.0f;
    n[1] = 0.0f;
    n[2] = 1.0f;
  }
}


static float ease(float t)
{
  return t*t*(3.0f-2.0f*t);
}


static void generate_geometry(ModeInfo *mi)
{
  cubocteversionstruct *ce = &cubocteversion[MI_SCREEN(mi)];
  int   i, j, k, l, m, nse, nsv;
  float n[3], t, tf, t1, t2, d;
  float (*v1)[3], (*v2)[3], v[NUM_VERT][3], q1[3], q2[3], e[3];
  double p1[3][3], p2[3][3], s1[3], s2[3];
  Bool self_inter_possible, intersects, adjacent, present;

  ce->num_se = 0;
  ce->num_sv = 0;
  self_inter_possible = False;

  if (ce->eversion_method == EVERSION_APERY)
  {
    if (fabsf(ce->time) <= 28.0f)
    {
      t = ce->time/28.0f;
      self_inter_possible = True;
    }
    else if (fabsf(ce->time) <= 35.0f)
    {
      if (ce->time <= 0.0f)
        t = (ce->time+21.0f)/7.0f;
      else
        t = (ce->time-21.0f)/7.0f;
    }
    else
    {
      if (ce->time <= 0.0f)
        t = (ce->time-7.0f)/21.0f;
      else
        t = (ce->time+7.0f)/21.0f;
    }
    tf = floorf(t);
    if (tf >= 2.0f)
      tf = 2.0f;
    t1 = ease(tf+1.0f-t);
    t2 = 1.0f-t1;

    m = (int)(tf+3.0f);
    v1 = vert_apery[m];
    v2 = vert_apery[m+1];
  }
  else /* ce->eversion_method == EVERSION_MORIN_DENNER */
  {
    if (fabsf(ce->time) <= 24.0f)
    {
      t = ce->time/4.0f;
      self_inter_possible = True;
    }
    else
    {
      if (ce->time <= 0.0f)
        t = (ce->time+12.0f)/2.0f;
      else
        t = (ce->time-12.0f)/2.0f;
    }
    tf = floorf(t);
    if (tf >= 21.0f)
      tf = 21.0f;
    t1 = ease(tf+1.0f-t);
    t2 = 1.0f-t1;

    m = (int)(tf+22.0f);
    v1 = vert_denner[m];
    v2 = vert_denner[m+1];
  }

  for (i=0; i<NUM_VERT; i++)
  {
    for (j=0; j<3; j++)
      v[i][j] = t1*v1[i][j]+t2*v2[i][j];
  }

  for (i=0; i<NUM_FACE; i++)
  {
    unit_normal(&v[face[i][0]][0],&v[face[i][1]][0],&v[face[i][2]][0],n);
    for (j=0; j<3; j++)
    {
      for (k=0; k<3; k++)
      {
        ce->tt[9*i+3*j+k] = v[face[i][j]][k];
        ce->tn[9*i+3*j+k] = n[k];
      }
    }
  }

  for (i=0; i<NUM_EDGE; i++)
  {
    for (j=0; j<2; j++)
    {
      for (k=0; k<3; k++)
        ce->te[6*i+3*j+k] = v[edge[i][j]][k];
    }
  }

  for (i=0; i<NUM_VERT; i++)
  {
    for (j=0; j<3; j++)
      ce->tv[3*i+j] = v[i][j];
  }

  if (self_inter_possible && ce->self_tubes)
  {
    nse = 0;
    nsv = 0;
    for (i=0; i<NUM_FACE; i++)
    {
      for (j=i+1; j<NUM_FACE; j++)
      {
        /* Check if the two triangles have at least one point in common. */
        adjacent = False;
        for (k=0; k<3; k++)
        {
          for (l=0; l<3; l++)
          {
            if (face[i][k] == face[j][l])
            {
              adjacent = True;
              break;
            }
          }
          if (adjacent)
            break;
        }
        if (adjacent)
          continue;

        /* Compute the intersection between the triangles. */
        for (k=0; k<3; k++)
        {
          for (l=0; l<3; l++)
          {
            p1[k][l] = v[face[i][k]][l];
            p2[k][l] = v[face[j][k]][l];
          }
        }
        intersects = tri_tri_intersection_3d(p1[0],p1[1],p1[2],
                                             p2[0],p2[1],p2[2],
                                             s1,s2);
        if (intersects)
        {
          ce->se[6*nse+0] = (float)s1[0];
          ce->se[6*nse+1] = (float)s1[1];
          ce->se[6*nse+2] = (float)s1[2];
          ce->se[6*nse+3] = (float)s2[0];
          ce->se[6*nse+4] = (float)s2[1];
          ce->se[6*nse+5] = (float)s2[2];

          for (l=0; l<2; l++)
          {
            q1[0] = ce->se[6*nse+3*l+0];
            q1[1] = ce->se[6*nse+3*l+1];
            q1[2] = ce->se[6*nse+3*l+2];
            present = False;
            for (k=0; k<nsv; k++)
            {
              q2[0] = ce->sv[3*k+0];
              q2[1] = ce->sv[3*k+1];
              q2[2] = ce->sv[3*k+2];
              e[0] = q2[0]-q1[0];
              e[1] = q2[1]-q1[1];
              e[2] = q2[2]-q1[2];
              d = sqrtf(e[0]*e[0]+e[1]*e[1]+e[2]*e[2]);
              if (d <= 100.0f*FLT_EPSILON)
              {
                present = True;
                break;
              }
            }
            if (!present)
            {
              ce->sv[3*nsv+0] = q1[0];
              ce->sv[3*nsv+1] = q1[1];
              ce->sv[3*nsv+2] = q1[2];
              nsv++;
            }
          }
          nse++;
        }
      }
    }
    ce->num_se = nse;
    ce->num_sv = nsv;
  }
}


static void gen_sphere(GLfloat cx, GLfloat cy, GLfloat cz, GLfloat r,
                       int num_u, int num_v, GLfloat *vertex, GLfloat *normal,
                       GLfloat *colorf, GLfloat *colorb, GLuint *index,
                       GLuint *tri_offset, GLuint *tri_num, GLsizei *num_p,
                       GLsizei *num_t, GLsizei *num_i)
{
  int i, j, k, l, m;
  GLsizei np, nt, ni;
  GLfloat u, v, cu, su, cv, sv;
  GLfloat x, y, z;

  np = *num_p;
  nt = *num_t;
  ni = *num_i;
  for (i=0; i<num_u; i++)
  {
    tri_offset[ni] = nt;
    k = (i+1)%num_u;
    u = (float)i*2.0f*M_PI_F/(float)num_u;
    cu = cosf(u);
    su = sinf(u);
    for (j=0; j<=num_v; j++)
    {
      l = np+i*(num_v+1)+j;
      m = np+k*(num_v+1)+j;
      v = (float)j*M_PI_F/(float)num_v;
      cv = cosf(v);
      sv = sinf(v);
      x = sv*cu;
      y = sv*su;
      z = cv;
      vertex[3*l+0] = cx+r*x;
      vertex[3*l+1] = cy+r*y;
      vertex[3*l+2] = cz+r*z;
      normal[3*l+0] = -x;
      normal[3*l+1] = -y;
      normal[3*l+2] = -z;
      colorf[4*l+0] = 1.0f;
      colorf[4*l+1] = 1.0f;
      colorf[4*l+2] = 1.0f;
      colorf[4*l+3] = 1.0f;
      colorb[4*l+0] = 1.0f;
      colorb[4*l+1] = 1.0f;
      colorb[4*l+2] = 1.0f;
      colorb[4*l+3] = 1.0f;
      index[nt++] = l;
      index[nt++] = m;
    }
    tri_num[ni++] = 2*(num_v+1);
  }
  np += num_u*(num_v+1);
  *num_p = np;
  *num_t = nt;
  *num_i = ni;
}


static void gen_cylinder(GLfloat cx, GLfloat cy, GLfloat cz, GLfloat ax,
                         GLfloat ay, GLfloat az, GLfloat length, GLfloat r,
                         int num_u, int num_v, GLfloat *vertex,
                         GLfloat *normal, GLfloat *colorf, GLfloat *colorb,
                         GLuint *index, GLuint *tri_offset, GLuint *tri_num,
                         GLsizei *num_p, GLsizei *num_t, GLsizei *num_i)
{
  int i, j, k, l, m;
  GLfloat u, v, cv, sv;
  GLsizei np, nt, ni;
  GLfloat x, y, z, nx, ny, nz, len;
  GLfloat ex, ey, ez, fx, fy, fz;

  np = *num_p;
  nt = *num_t;
  ni = *num_i;
  len = sqrtf(ax*ax+ay*ay+az*az);
  if (len < 10.0f*FLT_EPSILON || length < 10.0f*FLT_EPSILON)
    return;
  len = 1.0f/len;
  ax *= len;
  ay *= len;
  az *= len;
  if (fabsf(az) < 1.0f)
  {
    ex = ay;
    ey = -ax;
    len = 1.0f/sqrtf(ex*ex+ey*ey);
    ex *= len;
    ey *= len;
    ez = 0.0f;
    fx = ay*ez-az*ey;
    fy = az*ex-ax*ez;
    fz = ax*ey-ay*ex;
  }
  else
  {
    /* The case az = +/-1 does not occur in this program. */
    return;
  }
  tri_offset[ni] = nt;
  for (i=0; i<=1; i++)
  {
    k = i+1;
    u = (float)(2*i-1);
    for (j=0; j<=num_v; j++)
    {
      l = np+i*(num_v+1)+j;
      m = np+k*(num_v+1)+j;
      v = (float)j*2.0f*M_PI_F/(float)num_v;
      cv = cosf(v);
      sv = sinf(v);
      nx = ex*cv+fx*sv;
      ny = ey*cv+fy*sv;
      nz = ez*cv+fz*sv;
      x = u*length*ax+r*nx;
      y = u*length*ay+r*ny;
      z = u*length*az+r*nz;
      vertex[3*l+0] = cx+x;
      vertex[3*l+1] = cy+y;
      vertex[3*l+2] = cz+z;
      normal[3*l+0] = -nx;
      normal[3*l+1] = -ny;
      normal[3*l+2] = -nz;
      colorf[4*l+0] = 1.0f;
      colorf[4*l+1] = 1.0f;
      colorf[4*l+2] = 1.0f;
      colorf[4*l+3] = 1.0f;
      colorb[4*l+0] = 1.0f;
      colorb[4*l+1] = 1.0f;
      colorb[4*l+2] = 1.0f;
      colorb[4*l+3] = 1.0f;
      if (i < 1)
      {
        index[nt++] = l;
        index[nt++] = m;
      }
    }
  }
  tri_num[ni++] = 2*(num_v+1);
  np += 2*(num_v+1);
  *num_p = np;
  *num_t = nt;
  *num_i = ni;
}


static void gen_vertex_edge_tube(ModeInfo *mi)
{
  cubocteversionstruct *ce = &cubocteversion[MI_SCREEN(mi)];
  GLsizei num_p, num_t, num_i;
  GLfloat cx, cy, cz, ax, ay, az, len;
  int i, l;

  num_p = 0;
  num_t = 0;
  num_i = 0;
  for (i=0; i<NUM_VERT; i++)
  {
    l = 3*i;
    gen_sphere(ce->tv[l+0],ce->tv[l+1],ce->tv[l+2],RADIUS_TUBE,NUMU,NUMV,
               ce->tube_vertex,ce->tube_normal,ce->tube_colorf,
               ce->tube_colorb,ce->tube_index,ce->tube_tri_offset,
               ce->tube_tri_num,&num_p,&num_t,&num_i);
  }
  for (i=0; i<NUM_EDGE; i++)
  {
    l = 6*i;
    cx = 0.5f*(ce->te[l+0]+ce->te[l+3]);
    cy = 0.5f*(ce->te[l+1]+ce->te[l+4]);
    cz = 0.5f*(ce->te[l+2]+ce->te[l+5]);
    ax = ce->te[l+3]-ce->te[l+0];
    ay = ce->te[l+4]-ce->te[l+1];
    az = ce->te[l+5]-ce->te[l+2];
    len = 0.5f*sqrtf(ax*ax+ay*ay+az*az);
    gen_cylinder(cx,cy,cz,ax,ay,az,len,RADIUS_TUBE,NUMU,NUMV,
                 ce->tube_vertex,ce->tube_normal,ce->tube_colorf,
                 ce->tube_colorb,ce->tube_index,ce->tube_tri_offset,
                 ce->tube_tri_num,&num_p,&num_t,&num_i);
  }
  ce->tube_tri_strips = num_i;

#ifdef HAVE_GLSL
  if (ce->use_shaders)
  {
    glBindBuffer(GL_ARRAY_BUFFER,ce->tube_vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER,3*num_p*sizeof(GLfloat),ce->tube_vertex,
                 GL_STREAM_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);

    glBindBuffer(GL_ARRAY_BUFFER,ce->tube_normal_buffer);
    glBufferData(GL_ARRAY_BUFFER,3*num_p*sizeof(GLfloat),ce->tube_normal,
                 GL_STREAM_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);

    glBindBuffer(GL_ARRAY_BUFFER,ce->tube_colorf_buffer);
    glBufferData(GL_ARRAY_BUFFER,4*num_p*sizeof(GLfloat),ce->tube_colorf,
                 GL_STREAM_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);

    glBindBuffer(GL_ARRAY_BUFFER,ce->tube_colorb_buffer);
    glBufferData(GL_ARRAY_BUFFER,4*num_p*sizeof(GLfloat),ce->tube_colorb,
                 GL_STREAM_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ce->tube_index_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,num_t*sizeof(GLuint),ce->tube_index,
                 GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
  }
#endif
}


static void gen_self_intersection_tube(ModeInfo *mi)
{
  cubocteversionstruct *ce = &cubocteversion[MI_SCREEN(mi)];
  GLsizei num_p, num_t, num_i;
  GLfloat cx, cy, cz, ax, ay, az, len;
  int i, l;

  num_p = 0;
  num_t = 0;
  num_i = 0;
  for (i=0; i<ce->num_sv; i++)
  {
    l = 3*i;
    gen_sphere(ce->sv[l+0],ce->sv[l+1],ce->sv[l+2],RADIUS_SELF,NUMU,NUMV,
               ce->self_vertex,ce->self_normal,ce->self_colorf,
               ce->self_colorb,ce->self_index,ce->self_tri_offset,
               ce->self_tri_num,&num_p,&num_t,&num_i);
  }
  for (i=0; i<ce->num_se; i++)
  {
    l = 6*i;
    cx = 0.5f*(ce->se[l+0]+ce->se[l+3]);
    cy = 0.5f*(ce->se[l+1]+ce->se[l+4]);
    cz = 0.5f*(ce->se[l+2]+ce->se[l+5]);
    ax = ce->se[l+3]-ce->se[l+0];
    ay = ce->se[l+4]-ce->se[l+1];
    az = ce->se[l+5]-ce->se[l+2];
    len = 0.5f*sqrtf(ax*ax+ay*ay+az*az);
    gen_cylinder(cx,cy,cz,ax,ay,az,len,RADIUS_SELF,NUMU,NUMV,
                 ce->self_vertex,ce->self_normal,ce->self_colorf,
                 ce->self_colorb,ce->self_index,ce->self_tri_offset,
                 ce->self_tri_num,&num_p,&num_t,&num_i);
  }
  ce->self_tri_strips = num_i;

#ifdef HAVE_GLSL
  if (ce->use_shaders)
  {
    glBindBuffer(GL_ARRAY_BUFFER,ce->self_vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER,3*num_p*sizeof(GLfloat),ce->self_vertex,
                 GL_STREAM_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);

    glBindBuffer(GL_ARRAY_BUFFER,ce->self_normal_buffer);
    glBufferData(GL_ARRAY_BUFFER,3*num_p*sizeof(GLfloat),ce->self_normal,
                 GL_STREAM_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);

    glBindBuffer(GL_ARRAY_BUFFER,ce->self_colorf_buffer);
    glBufferData(GL_ARRAY_BUFFER,4*num_p*sizeof(GLfloat),ce->self_colorf,
                 GL_STREAM_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);

    glBindBuffer(GL_ARRAY_BUFFER,ce->self_colorb_buffer);
    glBufferData(GL_ARRAY_BUFFER,4*num_p*sizeof(GLfloat),ce->self_colorb,
                 GL_STREAM_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ce->self_index_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,num_t*sizeof(GLuint),ce->self_index,
                 GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
  }
#endif
}


/* ------------------------------------------------------------------------- */


/* Add a rotation around the x-axis to the matrix m. */
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


/* Add a rotation around the y-axis to the matrix m. */
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


/* Add a rotation around the z-axis to the matrix m. */
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
static void quat_to_rotmat(float p[4], float r[16])
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


/* Setup the surface colors. */
static void setup_colors(ModeInfo *mi)
{
  cubocteversionstruct *ce = &cubocteversion[MI_SCREEN(mi)];
  static float matc[3][3] = {
    {  2.0f/M_SQRT6_F,            0.0f, 1.0f/M_SQRT3_F },
    { -1.0f/M_SQRT6_F,  1.0f/M_SQRT2_F, 1.0f/M_SQRT3_F },
    { -1.0f/M_SQRT6_F, -1.0f/M_SQRT2_F, 1.0f/M_SQRT3_F }
  };
  float (*v)[3], b[3], c[3];
  int   i, j, k, l;

  if (ce->colors == COLORS_FACE)
  {
    v = vert_apery[0];
    for (i=0; i<NUM_FACE; i++)
    {
      barycenter(&v[face[i][0]][0],&v[face[i][1]][0],&v[face[i][2]][0],b);
      b[0] = b[0]/(2.0f*M_SQRT6_F);
      b[1] = b[1]/(2.0f*M_SQRT6_F);
      b[2] = (b[2]+(M_SQRT6_F-M_SQRT2_F))*(M_SQRT3_F/(2.0f*M_SQRT6_F));
      for (j=0; j<3; j++)
      {
        c[j] = 0.0f;
        for (k=0; k<3; k++)
          c[j] += matc[j][k]*b[k];
      }
      for (j=0; j<3; j++)
      {
        for (k=0; k<3; k++)
        {
          l = 12*i+4*j+k;
          ce->tcf[l] = c[k];
          ce->tcb[l] = c[k];
        }
        l = 12*i+4*j+3;
        if (ce->display_mode == DISP_TRANSPARENT)
        {
          ce->tcf[l] = ce->opacity;
          ce->tcb[l] = ce->opacity;
        }
        else
        {
          ce->tcf[l] = 1.0f;
          ce->tcb[l] = 1.0f;
        }
      }
    }
  }
  else /* ce->colors != COLORS_FACE */
  {
    for (i=0; i<NUM_FACE; i++)
    {
      for (j=0; j<3; j++)
      {
        for (k=0; k<4; k++)
        {
          l = 12*i+4*j+k;
          ce->tcf[l] = 1.0f;
          ce->tcb[l] = 1.0f;
        }
      }
    }
  }

#ifdef HAVE_GLSL
  if (ce->use_shaders)
  {
    glBindBuffer(GL_ARRAY_BUFFER,ce->cuboct_colorf_buffer);
    glBufferData(GL_ARRAY_BUFFER,4*3*NUM_FACE*sizeof(GLfloat),ce->tcf,
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);

    glBindBuffer(GL_ARRAY_BUFFER,ce->cuboct_colorb_buffer);
    glBufferData(GL_ARRAY_BUFFER,4*3*NUM_FACE*sizeof(GLfloat),ce->tcb,
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);
  }
#endif
}


#ifdef HAVE_GLSL

/* Set up the texture coordinates. */
static void setup_tex_coords(ModeInfo *mi)
{
  cubocteversionstruct *ce = &cubocteversion[MI_SCREEN(mi)];
  float (*v)[3], p[3], len;
  int   i, j, l;

  v = vert_apery[0];
  for (i=0; i<NUM_FACE; i++)
  {
    for (j=0; j<3; j++)
    {
      p[0] = v[face[i][j]][0];
      p[1] = v[face[i][j]][1];
      p[2] = v[face[i][j]][2]-M_SQRT2_F;
      len = sqrtf(p[0]*p[0]+p[1]*p[1]+p[2]*p[2]);
      p[0] /= len;
      p[1] /= len;
      p[2] /= len;
      l = 3*i+j;
      ce->tex[3*l+0] = p[0];
      ce->tex[3*l+1] = p[1];
      ce->tex[3*l+2] = p[2];
    }
  }

  if (ce->use_shaders)
  {
    glBindBuffer(GL_ARRAY_BUFFER,ce->cuboct_tex_coord_buffer);
    glBufferData(GL_ARRAY_BUFFER,3*3*NUM_FACE*sizeof(GLfloat),ce->tex,
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);
  }
}

#endif


/* Draw the cuboctahedron eversion using OpenGL's fixed functionality. */
static int cuboctahedron_eversion_ff(ModeInfo *mi)
{
  cubocteversionstruct *ce = &cubocteversion[MI_SCREEN(mi)];
  static const GLfloat light_position[] = { -0.3f, 0.3f, 1.0f, 0.0f };
  static const GLfloat light_model_ambient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
  static const GLfloat light_ambient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
  static const GLfloat light_diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  static const GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  static const GLfloat mat_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  static const GLfloat mat_diff_magenta[] = { 1.0f, 0.0, 0.5f, 1.0f };
#ifndef HAVE_JWZGLES
  static const GLfloat mat_diff_cyan[] = { 0.0f, 0.5f, 1.0f, 1.0f };
#endif
  static const GLfloat mat_diff_tube[] = { 0.7f, 0.7f, 0.7f, 1.0f };
  static const GLfloat mat_diff_self[] = { 1.0f, 0.5f, 0.0f, 1.0f };
  GLfloat mat_diff_magenta_trans[] = { 1.0f, 0.0, 0.5f, 0.5f };
#ifndef HAVE_JWZGLES
  GLfloat mat_diff_cyan_trans[] = { 0.0f, 0.5f, 1.0f, 0.5f };
#endif
  float qu[4], qr[16];
  GLfloat z_min, z_max;
  int i, j, l;
  unsigned int k;
  int polys = 0;

  generate_geometry(mi);
  if (ce->edge_tubes)
    gen_vertex_edge_tube(mi);
  if (ce->self_tubes)
    gen_self_intersection_tube(mi);

  glClearColor(0.0f,0.0f,0.0f,0.0f);
  glClearDepth(1.0f);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  mat_diff_magenta_trans[3] = ce->opacity;
#ifndef HAVE_JWZGLES
  mat_diff_cyan_trans[3] = ce->opacity;
#endif

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  z_min = MAX(-ce->offset3d[2]-9.5f,0.1f);
  z_max = -ce->offset3d[2]+9.5f;
  if (ce->projection == DISP_ORTHOGRAPHIC)
  {
    glOrtho(-ce->fov_ortho*ce->aspect,ce->fov_ortho*ce->aspect,
            -ce->fov_ortho,ce->fov_ortho,z_min,z_max);
  }
  else
  {
    gluPerspective(ce->fov_angle,ce->aspect,z_min,z_max);
  }

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glDisable(GL_COLOR_MATERIAL);
  glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
  glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);
  glLightModelfv(GL_LIGHT_MODEL_AMBIENT,light_model_ambient);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glLightfv(GL_LIGHT0,GL_AMBIENT,light_ambient);
  glLightfv(GL_LIGHT0,GL_DIFFUSE,light_diffuse);
  glLightfv(GL_LIGHT0,GL_SPECULAR,light_specular);
  glLightfv(GL_LIGHT0,GL_POSITION,light_position);
  glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,mat_specular);
  glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,50.0);

  gltrackball_get_quaternion(ce->trackball,qu);
  quat_to_rotmat(qu,qr);
  glTranslatef(ce->offset3d[0],ce->offset3d[1],ce->offset3d[2]);
  glMultMatrixf(qr);
  glRotatef(ce->alpha,1.0f,0.0f,0.0f);
  glRotatef(ce->beta,0.0f,1.0f,0.0f);
  glRotatef(ce->delta,0.0f,0.0f,1.0f);
  if (ce->eversion_method == EVERSION_APERY)
    glTranslatef(0.0f,0.0f,-M_SQRT2_F);
  else
    glTranslatef(0.0f,0.0f,-1.0f);

  glShadeModel(GL_SMOOTH);
  glDisable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);

  if (ce->edge_tubes)
  {
    glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,mat_diff_tube);

    for (i=0; i<ce->tube_tri_strips; i++)
    {
      glBegin(GL_TRIANGLE_STRIP);
      for (k=ce->tube_tri_offset[i];
           k<ce->tube_tri_offset[i]+ce->tube_tri_num[i]; k++)
      {
        l = 3*ce->tube_index[k];
        glNormal3fv(&ce->tube_normal[l]);
        glVertex3fv(&ce->tube_vertex[l]);
      }
      glEnd();
      polys += ce->tube_tri_num[i]-2;
    }
  }

  if (ce->self_tubes)
  {
    glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,mat_diff_self);

    for (i=0; i<ce->self_tri_strips; i++)
    {
      glBegin(GL_TRIANGLE_STRIP);
      for (k=ce->self_tri_offset[i];
           k<ce->self_tri_offset[i]+ce->self_tri_num[i]; k++)
      {
        l = 3*ce->self_index[k];
        glNormal3fv(&ce->self_normal[l]);
        glVertex3fv(&ce->self_vertex[l]);
      }
      glEnd();
      polys += ce->self_tri_num[i]-2;
    }
  }

  glShadeModel(GL_FLAT);
  if (ce->display_mode == DISP_SURFACE)
  {
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
  }
  else /* ce->display_mode == DISP_TRANSPARENT */
  {
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE);
  }

  if (ce->colors == COLORS_TWOSIDED)
  {
    if (ce->display_mode == DISP_SURFACE)
    {
#ifndef HAVE_JWZGLES
      glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,mat_diff_magenta);
      glMaterialfv(GL_BACK,GL_AMBIENT_AND_DIFFUSE,mat_diff_cyan);
#else
      glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,
                   mat_diff_magenta);
#endif
    }
    else /* ce->display_mode == DISP_TRANSPARENT */
    {
#ifndef HAVE_JWZGLES
      glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,mat_diff_magenta_trans);
      glMaterialfv(GL_BACK,GL_AMBIENT_AND_DIFFUSE,mat_diff_cyan_trans);
#else
      glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,
                   mat_diff_magenta_trans);
#endif
    }
  }

  glBegin(GL_TRIANGLES);
  for (i=0; i<NUM_FACE; i++)
  {
    for (j=0; j<3; j++)
    {
      if (ce->colors != COLORS_TWOSIDED)
      {
        l = 12*i+4*j;
        glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,&ce->tcf[l]);
        glMaterialfv(GL_BACK,GL_AMBIENT_AND_DIFFUSE,&ce->tcb[l]);
      }
      l = 9*i+3*j;
      glNormal3fv(&ce->tn[l]);
      glVertex3fv(&ce->tt[l]);
    }
  }
  glEnd();
  polys += NUM_FACE;

  glShadeModel(GL_SMOOTH);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);

  return polys;
}


#ifdef HAVE_GLSL

/* Draw the cuboctahedron eversion using OpenGL's programmable
   functionality. */
static int cuboctahedron_eversion_pf(ModeInfo *mi)
{
  cubocteversionstruct *ce = &cubocteversion[MI_SCREEN(mi)];
  static const GLenum draw_buffers[4] =
  {
    GL_COLOR_ATTACHMENT0,
    GL_COLOR_ATTACHMENT1,
    GL_COLOR_ATTACHMENT2,
    GL_BACK
  };
  static const GLfloat light_position[] = { -0.3f, 0.3f, 1.0f, 0.0f };
  static const GLfloat light_model_ambient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
  static const GLfloat light_ambient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
  static const GLfloat light_ambient_earth[]  = { 0.3f, 0.3f, 0.3f, 1.0f };
  static const GLfloat light_diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  static const GLfloat light_specular[] = { 0.7f, 0.7f, 0.7f, 1.0f };
  static const GLfloat mat_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  static const GLfloat mat_diff_magenta[] = { 1.0f, 0.0f, 0.5f, 1.0f };
  static const GLfloat mat_diff_cyan[] = { 0.0f, 0.5f, 1.0f, 1.0f };
  static const GLfloat mat_diff_white[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  static const GLfloat mat_diff_tube[] = { 0.7f, 0.7f, 0.7f, 1.0f };
  static const GLfloat mat_diff_self[] = { 1.0f, 0.5f, 0.0f, 1.0f };
  GLfloat mat_diff_magenta_trans[] = { 1.0f, 0.0f, 0.5f, 0.5f };
  GLfloat mat_diff_cyan_trans[] = { 0.0f, 0.5f, 1.0f, 0.5f };
  GLfloat mat_diff_white_trans[] = { 1.0f, 1.0f, 1.0f, 0.5f };
  GLfloat light_direction[3], half_vector[3], len;
  GLfloat p_mat[16], mv_mat[16];
  GLfloat z_min, z_max;
  GLuint sample_count;
  float qu[4], qr[16];
  int curr, prev, pass;
  GLsizeiptr index_offset;
  int i;
  int polys = 0;

  if (!ce->use_shaders)
    return 0;

  generate_geometry(mi);
  if (ce->edge_tubes)
    gen_vertex_edge_tube(mi);
  if (ce->self_tubes)
    gen_self_intersection_tube(mi);

  glBindBuffer(GL_ARRAY_BUFFER,ce->cuboct_pos_buffer);
  glBufferData(GL_ARRAY_BUFFER,3*3*NUM_FACE*sizeof(GLfloat),ce->tt,
               GL_STREAM_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER,0);

  glBindBuffer(GL_ARRAY_BUFFER,ce->cuboct_normal_buffer);
  glBufferData(GL_ARRAY_BUFFER,3*3*NUM_FACE*sizeof(GLfloat),ce->tn,
               GL_STREAM_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER,0);

  mat_diff_magenta_trans[3] = ce->opacity;
  mat_diff_cyan_trans[3] = ce->opacity;

  z_min = MAX(-ce->offset3d[2]-9.5f,0.1f);
  z_max = -ce->offset3d[2]+9.5f;
  glsl_Identity(p_mat);
  if (ce->projection == DISP_ORTHOGRAPHIC)
  {
    glsl_Orthographic(p_mat,-ce->fov_ortho*ce->aspect,ce->fov_ortho*ce->aspect,
                      -ce->fov_ortho,ce->fov_ortho,z_min,z_max);
  }
  else
  {
    glsl_Perspective(p_mat,ce->fov_angle,ce->aspect,z_min,z_max);
  }

  gltrackball_get_quaternion(ce->trackball,qu);
  quat_to_rotmat(qu,qr);
  glsl_Identity(mv_mat);
  glsl_Translate(mv_mat,ce->offset3d[0],ce->offset3d[1],ce->offset3d[2]);
  glsl_MultMatrix(mv_mat,qr);
  glsl_Rotate(mv_mat,ce->alpha,1.0f,0.0f,0.0f);
  glsl_Rotate(mv_mat,ce->beta,0.0f,1.0f,0.0f);
  glsl_Rotate(mv_mat,ce->delta,0.0f,0.0f,1.0f);
  if (ce->eversion_method == EVERSION_APERY)
    glsl_Translate(mv_mat,0.0f,0.0f,-M_SQRT2_F);
  else
    glsl_Translate(mv_mat,0.0f,0.0f,-1.0f);

  len = sqrtf(light_position[0]*light_position[0]+
              light_position[1]*light_position[1]+
              light_position[2]*light_position[2]);
  light_direction[0] = light_position[0]/len;
  light_direction[1] = light_position[1]/len;
  light_direction[2] = light_position[2]/len;
  half_vector[0] = light_direction[0];
  half_vector[1] = light_direction[1];
  half_vector[2] = light_direction[2]+1.0f;
  len = sqrtf(half_vector[0]*half_vector[0]+
              half_vector[1]*half_vector[1]+
              half_vector[2]*half_vector[2]);
  half_vector[0] /= len;
  half_vector[1] /= len;
  half_vector[2] /= len;

  if (ce->display_mode == DISP_SURFACE ||
      ce->transparency == TRANSPARENCY_STANDARD)
  {
    glClearColor(0.0f,0.0f,0.0f,0.0f);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    glUseProgram(ce->poly_shader_program);

    glUniformMatrix4fv(ce->poly_mv_index,1,GL_FALSE,mv_mat);
    glUniformMatrix4fv(ce->poly_proj_index,1,GL_FALSE,p_mat);

    glUniform4fv(ce->poly_front_ambient_index,1,mat_diff_white);
    glUniform4fv(ce->poly_front_diffuse_index,1,mat_diff_white);
    glUniform4fv(ce->poly_back_ambient_index,1,mat_diff_white);
    glUniform4fv(ce->poly_back_diffuse_index,1,mat_diff_white);

    glUniform4fv(ce->poly_glbl_ambient_index,1,light_model_ambient);
    if (ce->colors != COLORS_EARTH)
      glUniform4fv(ce->poly_lt_ambient_index,1,light_ambient);
    else
      glUniform4fv(ce->poly_lt_ambient_index,1,light_ambient_earth);
    glUniform4fv(ce->poly_lt_diffuse_index,1,light_diffuse);
    glUniform4fv(ce->poly_lt_specular_index,1,light_specular);
    glUniform3fv(ce->poly_lt_direction_index,1,light_direction);
    glUniform3fv(ce->poly_lt_halfvect_index,1,half_vector);
    glUniform4fv(ce->poly_specular_index,1,mat_specular);
    glUniform1f(ce->poly_shininess_index,50.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,ce->tex_names[0]);
    glUniform1i(ce->poly_tex_samp_f_index,0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D,ce->tex_names[1]);
    glUniform1i(ce->poly_tex_samp_b_index,1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D,ce->tex_names[2]);
    glUniform1i(ce->poly_tex_samp_w_index,2);
    glActiveTexture(GL_TEXTURE0);

    if (ce->edge_tubes)
    {
      glUniform4fv(ce->poly_front_ambient_index,1,mat_diff_tube);
      glUniform4fv(ce->poly_front_diffuse_index,1,mat_diff_tube);
      glUniform4fv(ce->poly_back_ambient_index,1,mat_diff_tube);
      glUniform4fv(ce->poly_back_diffuse_index,1,mat_diff_tube);

      glUniform1i(ce->poly_bool_textures_index,GL_FALSE);

      glEnableVertexAttribArray(ce->poly_pos_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->tube_vertex_buffer);
      glVertexAttribPointer(ce->poly_pos_index,3,GL_FLOAT,GL_FALSE,0,0);

      glEnableVertexAttribArray(ce->poly_normal_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->tube_normal_buffer);
      glVertexAttribPointer(ce->poly_normal_index,3,GL_FLOAT,GL_FALSE,0,0);

      glEnableVertexAttribArray(ce->poly_colorf_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->tube_colorf_buffer);
      glVertexAttribPointer(ce->poly_colorf_index,4,GL_FLOAT,GL_FALSE,0,0);

      glEnableVertexAttribArray(ce->poly_colorb_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->tube_colorb_buffer);
      glVertexAttribPointer(ce->poly_colorb_index,4,GL_FLOAT,GL_FALSE,0,0);

      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ce->tube_index_buffer);

      for (i=0; i<ce->tube_tri_strips; i++)
      {
        index_offset = ce->tube_tri_offset[i]*sizeof(GLuint);
        glDrawElements(GL_TRIANGLE_STRIP,ce->tube_tri_num[i],GL_UNSIGNED_INT,
                       (const GLvoid *)index_offset);
        polys += ce->tube_tri_num[i]-2;
      }

      glDisableVertexAttribArray(ce->poly_pos_index);
      glDisableVertexAttribArray(ce->poly_normal_index);
      glDisableVertexAttribArray(ce->poly_colorf_index);
      glDisableVertexAttribArray(ce->poly_colorb_index);

      glBindBuffer(GL_ARRAY_BUFFER,0);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
    }

    if (ce->self_tubes)
    {
      glUniform4fv(ce->poly_front_ambient_index,1,mat_diff_self);
      glUniform4fv(ce->poly_front_diffuse_index,1,mat_diff_self);
      glUniform4fv(ce->poly_back_ambient_index,1,mat_diff_self);
      glUniform4fv(ce->poly_back_diffuse_index,1,mat_diff_self);

      glUniform1i(ce->poly_bool_textures_index,GL_FALSE);

      glEnableVertexAttribArray(ce->poly_pos_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->self_vertex_buffer);
      glVertexAttribPointer(ce->poly_pos_index,3,GL_FLOAT,GL_FALSE,0,0);

      glEnableVertexAttribArray(ce->poly_normal_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->self_normal_buffer);
      glVertexAttribPointer(ce->poly_normal_index,3,GL_FLOAT,GL_FALSE,0,0);

      glEnableVertexAttribArray(ce->poly_colorf_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->self_colorf_buffer);
      glVertexAttribPointer(ce->poly_colorf_index,4,GL_FLOAT,GL_FALSE,0,0);

      glEnableVertexAttribArray(ce->poly_colorb_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->self_colorb_buffer);
      glVertexAttribPointer(ce->poly_colorb_index,4,GL_FLOAT,GL_FALSE,0,0);

      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ce->self_index_buffer);

      for (i=0; i<ce->self_tri_strips; i++)
      {
        index_offset = ce->self_tri_offset[i]*sizeof(GLuint);
        glDrawElements(GL_TRIANGLE_STRIP,ce->self_tri_num[i],GL_UNSIGNED_INT,
                       (const GLvoid *)index_offset);
        polys += ce->self_tri_num[i]-2;
      }

      glDisableVertexAttribArray(ce->poly_pos_index);
      glDisableVertexAttribArray(ce->poly_normal_index);
      glDisableVertexAttribArray(ce->poly_colorf_index);
      glDisableVertexAttribArray(ce->poly_colorb_index);

      glBindBuffer(GL_ARRAY_BUFFER,0);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
    }

    if (ce->display_mode == DISP_SURFACE)
    {
      glDepthMask(GL_TRUE);
      glDisable(GL_BLEND);
    }
    else /* ce->display_mode == DISP_TRANSPARENT */
    {
      glDepthMask(GL_FALSE);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA,GL_ONE);
    }

    if (ce->colors == COLORS_TWOSIDED)
    {
      if (ce->display_mode == DISP_SURFACE)
      {
        glUniform4fv(ce->poly_front_ambient_index,1,mat_diff_magenta);
        glUniform4fv(ce->poly_front_diffuse_index,1,mat_diff_magenta);
        glUniform4fv(ce->poly_back_ambient_index,1,mat_diff_cyan);
        glUniform4fv(ce->poly_back_diffuse_index,1,mat_diff_cyan);
      }
      else
      {
        glUniform4fv(ce->poly_front_ambient_index,1,mat_diff_magenta_trans);
        glUniform4fv(ce->poly_front_diffuse_index,1,mat_diff_magenta_trans);
        glUniform4fv(ce->poly_back_ambient_index,1,mat_diff_cyan_trans);
        glUniform4fv(ce->poly_back_diffuse_index,1,mat_diff_cyan_trans);
      }
    }
    else /* ce->colors != COLORS_TWOSIDED */
    {
      if (ce->display_mode == DISP_SURFACE)
      {
        glUniform4fv(ce->poly_front_ambient_index,1,mat_diff_white);
        glUniform4fv(ce->poly_front_diffuse_index,1,mat_diff_white);
        glUniform4fv(ce->poly_back_ambient_index,1,mat_diff_white);
        glUniform4fv(ce->poly_back_diffuse_index,1,mat_diff_white);
      }
      else
      {
        glUniform4fv(ce->poly_front_ambient_index,1,mat_diff_white_trans);
        glUniform4fv(ce->poly_front_diffuse_index,1,mat_diff_white_trans);
        glUniform4fv(ce->poly_back_ambient_index,1,mat_diff_white_trans);
        glUniform4fv(ce->poly_back_diffuse_index,1,mat_diff_white_trans);
      }
    }

    glUniform1i(ce->poly_bool_textures_index,ce->colors == COLORS_EARTH);

    glEnableVertexAttribArray(ce->poly_pos_index);
    glBindBuffer(GL_ARRAY_BUFFER,ce->cuboct_pos_buffer);
    glVertexAttribPointer(ce->poly_pos_index,3,GL_FLOAT,GL_FALSE,0,0);

    glEnableVertexAttribArray(ce->poly_normal_index);
    glBindBuffer(GL_ARRAY_BUFFER,ce->cuboct_normal_buffer);
    glVertexAttribPointer(ce->poly_normal_index,3,GL_FLOAT,GL_FALSE,0,0);

    glEnableVertexAttribArray(ce->poly_colorf_index);
    glBindBuffer(GL_ARRAY_BUFFER,ce->cuboct_colorf_buffer);
    glVertexAttribPointer(ce->poly_colorf_index,4,GL_FLOAT,GL_FALSE,0,0);

    glEnableVertexAttribArray(ce->poly_colorb_index);
    glBindBuffer(GL_ARRAY_BUFFER,ce->cuboct_colorb_buffer);
    glVertexAttribPointer(ce->poly_colorb_index,4,GL_FLOAT,GL_FALSE,0,0);

    glEnableVertexAttribArray(ce->poly_vertex_tex_index);
    glBindBuffer(GL_ARRAY_BUFFER,ce->cuboct_tex_coord_buffer);
    glVertexAttribPointer(ce->poly_vertex_tex_index,3,GL_FLOAT,GL_FALSE,0,0);

    glDrawArrays(GL_TRIANGLES,0,3*NUM_FACE);
    polys += NUM_FACE;

    glDisableVertexAttribArray(ce->poly_pos_index);
    glDisableVertexAttribArray(ce->poly_normal_index);
    glDisableVertexAttribArray(ce->poly_colorf_index);
    glDisableVertexAttribArray(ce->poly_colorb_index);
    glDisableVertexAttribArray(ce->poly_vertex_tex_index);

    glBindBuffer(GL_ARRAY_BUFFER,0);

    glUseProgram(0);

    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
  }
  else if (ce->transparency == TRANSPARENCY_CORRECT)
  {
    /* Render the tubes into a texture. */
    glBindFramebuffer(GL_FRAMEBUFFER,ce->dp_opaque_fbo);
    glDrawBuffers(1,&draw_buffers[0]);
    glClearColor(0.0f,0.0f,0.0f,0.0f);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    glUseProgram(ce->poly_shader_program);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D,ce->tex_names[0]);
    glUniform1i(ce->poly_tex_samp_f_index,2);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D,ce->tex_names[1]);
    glUniform1i(ce->poly_tex_samp_b_index,3);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D,ce->tex_names[2]);
    glUniform1i(ce->poly_tex_samp_w_index,4);
    glActiveTexture(GL_TEXTURE0);

    if (ce->edge_tubes)
    {
      glUniformMatrix4fv(ce->poly_proj_index,1,GL_FALSE,p_mat);
      glUniformMatrix4fv(ce->poly_mv_index,1,GL_FALSE,mv_mat);
      glUniform1i(ce->poly_bool_textures_index,GL_FALSE);

      glUniform4fv(ce->poly_glbl_ambient_index,1,light_model_ambient);
      glUniform4fv(ce->poly_lt_ambient_index,1,light_ambient);

      glUniform4fv(ce->poly_lt_diffuse_index,1,light_diffuse);
      glUniform4fv(ce->poly_lt_specular_index,1,light_specular);
      glUniform3fv(ce->poly_lt_direction_index,1,light_direction);
      glUniform3fv(ce->poly_lt_halfvect_index,1,half_vector);
      glUniform4fv(ce->poly_specular_index,1,mat_specular);
      glUniform1f(ce->poly_shininess_index,50.0f);

      glUniform4fv(ce->poly_front_ambient_index,1,mat_diff_tube);
      glUniform4fv(ce->poly_front_diffuse_index,1,mat_diff_tube);
      glUniform4fv(ce->poly_back_ambient_index,1,mat_diff_tube);
      glUniform4fv(ce->poly_back_diffuse_index,1,mat_diff_tube);

      glEnableVertexAttribArray(ce->poly_pos_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->tube_vertex_buffer);
      glVertexAttribPointer(ce->poly_pos_index,3,GL_FLOAT,GL_FALSE,0,0);

      glEnableVertexAttribArray(ce->poly_normal_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->tube_normal_buffer);
      glVertexAttribPointer(ce->poly_normal_index,3,GL_FLOAT,GL_FALSE,0,0);

      glEnableVertexAttribArray(ce->poly_colorf_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->tube_colorf_buffer);
      glVertexAttribPointer(ce->poly_colorf_index,4,GL_FLOAT,GL_FALSE,0,0);

      glEnableVertexAttribArray(ce->poly_colorb_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->tube_colorb_buffer);
      glVertexAttribPointer(ce->poly_colorb_index,4,GL_FLOAT,GL_FALSE,0,0);

      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ce->tube_index_buffer);

      for (i=0; i<ce->tube_tri_strips; i++)
      {
        index_offset = ce->tube_tri_offset[i]*sizeof(GLuint);
        glDrawElements(GL_TRIANGLE_STRIP,ce->tube_tri_num[i],GL_UNSIGNED_INT,
                       (const GLvoid *)index_offset);
        polys += ce->tube_tri_num[i]-2;
      }

      glDisableVertexAttribArray(ce->poly_pos_index);
      glDisableVertexAttribArray(ce->poly_normal_index);

      glBindBuffer(GL_ARRAY_BUFFER,0);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
    }

    if (ce->self_tubes)
    {
      glUniformMatrix4fv(ce->poly_proj_index,1,GL_FALSE,p_mat);
      glUniformMatrix4fv(ce->poly_mv_index,1,GL_FALSE,mv_mat);
      glUniform1i(ce->poly_bool_textures_index,GL_FALSE);

      glUniform4fv(ce->poly_glbl_ambient_index,1,light_model_ambient);
      glUniform4fv(ce->poly_lt_ambient_index,1,light_ambient);

      glUniform4fv(ce->poly_lt_diffuse_index,1,light_diffuse);
      glUniform4fv(ce->poly_lt_specular_index,1,light_specular);
      glUniform3fv(ce->poly_lt_direction_index,1,light_direction);
      glUniform3fv(ce->poly_lt_halfvect_index,1,half_vector);
      glUniform4fv(ce->poly_specular_index,1,mat_specular);
      glUniform1f(ce->poly_shininess_index,50.0f);

      glUniform4fv(ce->poly_front_ambient_index,1,mat_diff_self);
      glUniform4fv(ce->poly_front_diffuse_index,1,mat_diff_self);
      glUniform4fv(ce->poly_back_ambient_index,1,mat_diff_self);
      glUniform4fv(ce->poly_back_diffuse_index,1,mat_diff_self);

      glEnableVertexAttribArray(ce->poly_pos_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->self_vertex_buffer);
      glVertexAttribPointer(ce->poly_pos_index,3,GL_FLOAT,GL_FALSE,0,0);

      glEnableVertexAttribArray(ce->poly_normal_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->self_normal_buffer);
      glVertexAttribPointer(ce->poly_normal_index,3,GL_FLOAT,GL_FALSE,0,0);

      glEnableVertexAttribArray(ce->poly_colorf_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->self_colorf_buffer);
      glVertexAttribPointer(ce->poly_colorf_index,4,GL_FLOAT,GL_FALSE,0,0);

      glEnableVertexAttribArray(ce->poly_colorb_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->self_colorb_buffer);
      glVertexAttribPointer(ce->poly_colorb_index,4,GL_FLOAT,GL_FALSE,0,0);

      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ce->self_index_buffer);

      for (i=0; i<ce->self_tri_strips; i++)
      {
        index_offset = ce->self_tri_offset[i]*sizeof(GLuint);
        glDrawElements(GL_TRIANGLE_STRIP,ce->self_tri_num[i],GL_UNSIGNED_INT,
                       (const GLvoid *)index_offset);
        polys += ce->self_tri_num[i]-2;
      }

      glDisableVertexAttribArray(ce->poly_pos_index);
      glDisableVertexAttribArray(ce->poly_normal_index);

      glBindBuffer(GL_ARRAY_BUFFER,0);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

    }

    glUseProgram(0);

    /* Initialize the dual depth peeling. */
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);

    glBindFramebuffer(GL_FRAMEBUFFER,ce->dp_back_blender_fbo);
    glDrawBuffers(1,&draw_buffers[0]);
    glClearColor(0.0f,0.0f,0.0f,0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER,ce->dp_peeling_fbo[0]);
    glDrawBuffers(1,&draw_buffers[0]);
    glClearColor(-MAX_DEPTH,-MAX_DEPTH,0.0f,0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER,ce->dp_color_fbo[0]);
    glDrawBuffers(2,&draw_buffers[0]);
    glClearColor(0.0f,0.0f,0.0f,0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER,ce->dp_peeling_fbo[0]);
    glDrawBuffers(1,&draw_buffers[0]);
    glBlendEquation(GL_MAX);

    glUseProgram(ce->dp_init_shader_program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,ce->dp_opaque_depth_tex);
    glUniform1i(ce->dp_init_tex_samp_depth,0);
    glActiveTexture(GL_TEXTURE0);

    glUniformMatrix4fv(ce->dp_init_proj_index,1,GL_FALSE,p_mat);
    glUniformMatrix4fv(ce->dp_init_mv_index,1,GL_FALSE,mv_mat);
    glUniform2fv(ce->dp_init_tex_scale_index,1,ce->tex_scale);

    glEnableVertexAttribArray(ce->dp_init_pos_index);
    glBindBuffer(GL_ARRAY_BUFFER,ce->cuboct_pos_buffer);
    glVertexAttribPointer(ce->dp_init_pos_index,3,GL_FLOAT,GL_FALSE,0,0);

    glDrawArrays(GL_TRIANGLES,0,3*NUM_FACE);
    polys += NUM_FACE;

    glDisableVertexAttribArray(ce->dp_init_pos_index);

    glUseProgram(0);

    /* Dual depth peeling and blending. */
    curr = 0;
    for (pass=1; pass<=MAX_PASSES; pass++)
    {
      curr = pass%2;
      prev = 1-curr;

      glBindFramebuffer(GL_FRAMEBUFFER,ce->dp_peeling_fbo[curr]);
      glDrawBuffers(1,&draw_buffers[0]);
      glClearColor(-MAX_DEPTH,-MAX_DEPTH,0.0f,0.0f);
      glClear(GL_COLOR_BUFFER_BIT);

      glBindFramebuffer(GL_FRAMEBUFFER,ce->dp_color_fbo[curr]);
      glDrawBuffers(2,&draw_buffers[0]);
      glClearColor(0.0f,0.0f,0.0f,0.0f);
      glClear(GL_COLOR_BUFFER_BIT);

      glBindFramebuffer(GL_FRAMEBUFFER,ce->dp_peeling_fbo[curr]);
      glDrawBuffers(3,&draw_buffers[0]);
      glBlendEquation(GL_MAX);

      glUseProgram(ce->dp_peel_shader_program);

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D,ce->dp_depth_tex[prev]);
      glUniform1i(ce->dp_peel_tex_samp_depth_blend,0);
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D,ce->dp_front_blender_tex[prev]);
      glUniform1i(ce->dp_peel_tex_samp_front_blend,1);
      glActiveTexture(GL_TEXTURE0);

      glUniform2fv(ce->dp_peel_tex_scale_index,1,ce->tex_scale);

      glUniformMatrix4fv(ce->dp_peel_proj_index,1,GL_FALSE,p_mat);
      glUniformMatrix4fv(ce->dp_peel_mv_index,1,GL_FALSE,mv_mat);

      glUniform4fv(ce->dp_peel_front_ambient_index,1,mat_diff_white);
      glUniform4fv(ce->dp_peel_front_diffuse_index,1,mat_diff_white);
      glUniform4fv(ce->dp_peel_back_ambient_index,1,mat_diff_white);
      glUniform4fv(ce->dp_peel_back_diffuse_index,1,mat_diff_white);

      glUniform4fv(ce->dp_peel_front_ambient_index,1,mat_diff_white);
      glUniform4fv(ce->dp_peel_front_diffuse_index,1,mat_diff_white);
      glUniform4fv(ce->dp_peel_back_ambient_index,1,mat_diff_white);
      glUniform4fv(ce->dp_peel_back_diffuse_index,1,mat_diff_white);

      glUniform4fv(ce->dp_peel_glbl_ambient_index,1,light_model_ambient);
      if (ce->colors != COLORS_EARTH)
        glUniform4fv(ce->dp_peel_lt_ambient_index,1,light_ambient);
      else
        glUniform4fv(ce->dp_peel_lt_ambient_index,1,light_ambient_earth);
      glUniform4fv(ce->dp_peel_lt_diffuse_index,1,light_diffuse);
      glUniform4fv(ce->dp_peel_lt_specular_index,1,light_specular);
      glUniform3fv(ce->dp_peel_lt_direction_index,1,light_direction);
      glUniform3fv(ce->dp_peel_lt_halfvect_index,1,half_vector);
      glUniform4fv(ce->dp_peel_specular_index,1,mat_specular);
      glUniform1f(ce->dp_peel_shininess_index,50.0f);

      if (ce->colors == COLORS_TWOSIDED)
      {
        glUniform4fv(ce->dp_peel_front_ambient_index,1,mat_diff_magenta_trans);
        glUniform4fv(ce->dp_peel_front_diffuse_index,1,mat_diff_magenta_trans);
        glUniform4fv(ce->dp_peel_back_ambient_index,1,mat_diff_cyan_trans);
        glUniform4fv(ce->dp_peel_back_diffuse_index,1,mat_diff_cyan_trans);
      }
      else
      {
        glUniform4fv(ce->dp_peel_front_ambient_index,1,mat_diff_white_trans);
        glUniform4fv(ce->dp_peel_front_diffuse_index,1,mat_diff_white_trans);
        glUniform4fv(ce->dp_peel_back_ambient_index,1,mat_diff_white_trans);
        glUniform4fv(ce->dp_peel_back_diffuse_index,1,mat_diff_white_trans);
      }

      glActiveTexture(GL_TEXTURE2);
      glBindTexture(GL_TEXTURE_2D,ce->tex_names[0]);
      glUniform1i(ce->dp_peel_tex_samp_f_index,2);
      glActiveTexture(GL_TEXTURE3);
      glBindTexture(GL_TEXTURE_2D,ce->tex_names[1]);
      glUniform1i(ce->dp_peel_tex_samp_b_index,3);
      glActiveTexture(GL_TEXTURE4);
      glBindTexture(GL_TEXTURE_2D,ce->tex_names[2]);
      glUniform1i(ce->dp_peel_tex_samp_w_index,4);
      glUniform1i(ce->dp_peel_bool_textures_index,ce->colors == COLORS_EARTH);
      glActiveTexture(GL_TEXTURE0);

      glEnableVertexAttribArray(ce->dp_peel_pos_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->cuboct_pos_buffer);
      glVertexAttribPointer(ce->dp_peel_pos_index,3,GL_FLOAT,GL_FALSE,
                            0,0);

      glEnableVertexAttribArray(ce->dp_peel_normal_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->cuboct_normal_buffer);
      glVertexAttribPointer(ce->dp_peel_normal_index,3,GL_FLOAT,GL_FALSE,
                            0,0);

      glEnableVertexAttribArray(ce->dp_peel_colorf_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->cuboct_colorf_buffer);
      glVertexAttribPointer(ce->dp_peel_colorf_index,4,GL_FLOAT,GL_FALSE,
                            0,0);

      glEnableVertexAttribArray(ce->dp_peel_colorb_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->cuboct_colorb_buffer);
      glVertexAttribPointer(ce->dp_peel_colorb_index,4,GL_FLOAT,GL_FALSE,
                            0,0);

      glEnableVertexAttribArray(ce->dp_peel_vertex_tex_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->cuboct_tex_coord_buffer);
      glVertexAttribPointer(ce->dp_peel_vertex_tex_index,3,GL_FLOAT,GL_FALSE,
                            0,0);

      glDrawArrays(GL_TRIANGLES,0,3*NUM_FACE);

      glDisableVertexAttribArray(ce->dp_peel_pos_index);
      glDisableVertexAttribArray(ce->dp_peel_normal_index);
      glDisableVertexAttribArray(ce->dp_peel_colorf_index);
      glDisableVertexAttribArray(ce->dp_peel_colorb_index);
      glDisableVertexAttribArray(ce->dp_peel_vertex_tex_index);

      glBindBuffer(GL_ARRAY_BUFFER,0);

      glUseProgram(0);

      glBindFramebuffer(GL_FRAMEBUFFER,ce->dp_back_blender_fbo);
      glDrawBuffers(1,&draw_buffers[0]);
      glBlendEquation(GL_FUNC_ADD);
      glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

#ifdef HAVE_JWZGLES
      glBeginQuery(GL_ANY_SAMPLES_PASSED,ce->dp_query);
#else
      glBeginQuery(GL_SAMPLES_PASSED,ce->dp_query);
#endif

      glUseProgram(ce->dp_blnd_shader_program);

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D,ce->dp_back_temp_tex[curr]);
      glUniform1i(ce->dp_blnd_tex_samp_temp,0);
      glActiveTexture(GL_TEXTURE0);

      glUniform2fv(ce->dp_blnd_tex_scale_index,1,ce->tex_scale);

      glEnableVertexAttribArray(ce->dp_blnd_pos_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->quad_buffer);
      glVertexAttribPointer(ce->dp_blnd_pos_index,2,GL_FLOAT,GL_FALSE,0,0);

      glDrawArrays(GL_TRIANGLE_STRIP,0,4);

      glBindBuffer(GL_ARRAY_BUFFER,0);

      glDisableVertexAttribArray(ce->dp_blnd_pos_index);

      glUseProgram(0);

#ifdef HAVE_JWZGLES
      glEndQuery(GL_ANY_SAMPLES_PASSED);
#else
      glEndQuery(GL_SAMPLES_PASSED);
#endif

      glGetQueryObjectuiv(ce->dp_query,GL_QUERY_RESULT,&sample_count);
      if (sample_count == 0)
        break;
    }

    glDisable(GL_BLEND);

    /* Final pass. */
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,ce->dp_draw_fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,ce->dp_read_fbo);
#if (defined(HAVE_COCOA) || defined(__APPLE__)) && !defined(HAVE_IPHONE)
    glDrawBuffer(GL_BACK);
#else
    glDrawBuffers(1,&draw_buffers[3]);
#endif

    glUseProgram(ce->dp_finl_shader_program);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D,ce->dp_front_blender_tex[curr]);
    glUniform1i(ce->dp_finl_tex_samp_front,1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D,ce->dp_back_blender_tex);
    glUniform1i(ce->dp_finl_tex_samp_back,2);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D,ce->dp_opaque_color_tex);
    glUniform1i(ce->dp_finl_tex_samp_opaque,3);
    glActiveTexture(GL_TEXTURE0);

    glUniform2fv(ce->dp_finl_tex_scale_index,1,ce->tex_scale);

    glEnableVertexAttribArray(ce->dp_finl_pos_index);
    glBindBuffer(GL_ARRAY_BUFFER,ce->quad_buffer);
    glVertexAttribPointer(ce->dp_finl_pos_index,2,GL_FLOAT,GL_FALSE,0,0);

    glDrawArrays(GL_TRIANGLE_STRIP,0,4);

    glDisableVertexAttribArray(ce->dp_finl_pos_index);

    glBindBuffer(GL_ARRAY_BUFFER,0);

    glUseProgram(0);

    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
  }
  else /* ce->transparency == TRANSPARENCY_APPROXIMATE */
  {
    /* Render the tubes and lines into a texture. */
    glBindFramebuffer(GL_FRAMEBUFFER,ce->wa_opaque_fbo);
    glDrawBuffers(1,&draw_buffers[0]);
    glClearColor(0.0f,0.0f,0.0f,0.0f);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    glUseProgram(ce->poly_shader_program);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D,ce->tex_names[0]);
    glUniform1i(ce->poly_tex_samp_f_index,2);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D,ce->tex_names[1]);
    glUniform1i(ce->poly_tex_samp_b_index,3);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D,ce->tex_names[2]);
    glUniform1i(ce->poly_tex_samp_w_index,4);
    glActiveTexture(GL_TEXTURE0);

    if (ce->edge_tubes)
    {
      glUniformMatrix4fv(ce->poly_proj_index,1,GL_FALSE,p_mat);
      glUniformMatrix4fv(ce->poly_mv_index,1,GL_FALSE,mv_mat);
      glUniform1i(ce->poly_bool_textures_index,GL_FALSE);

      glUniform4fv(ce->poly_glbl_ambient_index,1,light_model_ambient);
      glUniform4fv(ce->poly_lt_ambient_index,1,light_ambient);

      glUniform4fv(ce->poly_lt_diffuse_index,1,light_diffuse);
      glUniform4fv(ce->poly_lt_specular_index,1,light_specular);
      glUniform3fv(ce->poly_lt_direction_index,1,light_direction);
      glUniform3fv(ce->poly_lt_halfvect_index,1,half_vector);
      glUniform4fv(ce->poly_specular_index,1,mat_specular);
      glUniform1f(ce->poly_shininess_index,50.0f);

      glUniform4fv(ce->poly_front_ambient_index,1,mat_diff_tube);
      glUniform4fv(ce->poly_front_diffuse_index,1,mat_diff_tube);
      glUniform4fv(ce->poly_back_ambient_index,1,mat_diff_tube);
      glUniform4fv(ce->poly_back_diffuse_index,1,mat_diff_tube);

      glEnableVertexAttribArray(ce->poly_pos_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->tube_vertex_buffer);
      glVertexAttribPointer(ce->poly_pos_index,3,GL_FLOAT,GL_FALSE,0,0);

      glEnableVertexAttribArray(ce->poly_normal_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->tube_normal_buffer);
      glVertexAttribPointer(ce->poly_normal_index,3,GL_FLOAT,GL_FALSE,0,0);

      glEnableVertexAttribArray(ce->poly_colorf_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->tube_colorf_buffer);
      glVertexAttribPointer(ce->poly_colorf_index,4,GL_FLOAT,GL_FALSE,0,0);

      glEnableVertexAttribArray(ce->poly_colorb_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->tube_colorb_buffer);
      glVertexAttribPointer(ce->poly_colorb_index,4,GL_FLOAT,GL_FALSE,0,0);

      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ce->tube_index_buffer);

      for (i=0; i<ce->tube_tri_strips; i++)
      {
        index_offset = ce->tube_tri_offset[i]*sizeof(GLuint);
        glDrawElements(GL_TRIANGLE_STRIP,ce->tube_tri_num[i],GL_UNSIGNED_INT,
                       (const GLvoid *)index_offset);
        polys += ce->tube_tri_num[i]-2;
      }

      glDisableVertexAttribArray(ce->poly_pos_index);
      glDisableVertexAttribArray(ce->poly_normal_index);

      glBindBuffer(GL_ARRAY_BUFFER,0);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
    }

    if (ce->self_tubes)
    {
      glUniformMatrix4fv(ce->poly_proj_index,1,GL_FALSE,p_mat);
      glUniformMatrix4fv(ce->poly_mv_index,1,GL_FALSE,mv_mat);
      glUniform1i(ce->poly_bool_textures_index,GL_FALSE);

      glUniform4fv(ce->poly_glbl_ambient_index,1,light_model_ambient);
      glUniform4fv(ce->poly_lt_ambient_index,1,light_ambient);

      glUniform4fv(ce->poly_lt_diffuse_index,1,light_diffuse);
      glUniform4fv(ce->poly_lt_specular_index,1,light_specular);
      glUniform3fv(ce->poly_lt_direction_index,1,light_direction);
      glUniform3fv(ce->poly_lt_halfvect_index,1,half_vector);
      glUniform4fv(ce->poly_specular_index,1,mat_specular);
      glUniform1f(ce->poly_shininess_index,50.0f);

      glUniform4fv(ce->poly_front_ambient_index,1,mat_diff_self);
      glUniform4fv(ce->poly_front_diffuse_index,1,mat_diff_self);
      glUniform4fv(ce->poly_back_ambient_index,1,mat_diff_self);
      glUniform4fv(ce->poly_back_diffuse_index,1,mat_diff_self);

      glEnableVertexAttribArray(ce->poly_pos_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->self_vertex_buffer);
      glVertexAttribPointer(ce->poly_pos_index,3,GL_FLOAT,GL_FALSE,0,0);

      glEnableVertexAttribArray(ce->poly_normal_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->self_normal_buffer);
      glVertexAttribPointer(ce->poly_normal_index,3,GL_FLOAT,GL_FALSE,0,0);

      glEnableVertexAttribArray(ce->poly_colorf_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->self_colorf_buffer);
      glVertexAttribPointer(ce->poly_colorf_index,4,GL_FLOAT,GL_FALSE,0,0);

      glEnableVertexAttribArray(ce->poly_colorb_index);
      glBindBuffer(GL_ARRAY_BUFFER,ce->self_colorb_buffer);
      glVertexAttribPointer(ce->poly_colorb_index,4,GL_FLOAT,GL_FALSE,0,0);

      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ce->self_index_buffer);

      for (i=0; i<ce->self_tri_strips; i++)
      {
        index_offset = ce->self_tri_offset[i]*sizeof(GLuint);
        glDrawElements(GL_TRIANGLE_STRIP,ce->self_tri_num[i],GL_UNSIGNED_INT,
                       (const GLvoid *)index_offset);
        polys += ce->self_tri_num[i]-2;
      }

      glDisableVertexAttribArray(ce->poly_pos_index);
      glDisableVertexAttribArray(ce->poly_normal_index);

      glBindBuffer(GL_ARRAY_BUFFER,0);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

    }

    glUseProgram(0);

    /* Accumulate colors and depth complexity. */
    glBindFramebuffer(GL_FRAMEBUFFER,ce->wa_accumulation_fbo);
    glDrawBuffers(2,&draw_buffers[0]);

    glClearColor(0.0f,0.0f,0.0f,0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE,GL_ONE);

    glUseProgram(ce->wa_init_shader_program);

    glUniformMatrix4fv(ce->wa_init_proj_index,1,GL_FALSE,p_mat);

    glUniform4fv(ce->wa_init_front_ambient_index,1,mat_diff_white);
    glUniform4fv(ce->wa_init_front_diffuse_index,1,mat_diff_white);
    glUniform4fv(ce->wa_init_back_ambient_index,1,mat_diff_white);
    glUniform4fv(ce->wa_init_back_diffuse_index,1,mat_diff_white);

    glUniform4fv(ce->wa_init_glbl_ambient_index,1,light_model_ambient);
    if (ce->colors != COLORS_EARTH)
      glUniform4fv(ce->wa_init_lt_ambient_index,1,light_ambient);
    else
      glUniform4fv(ce->wa_init_lt_ambient_index,1,light_ambient_earth);
    glUniform4fv(ce->wa_init_lt_diffuse_index,1,light_diffuse);
    glUniform4fv(ce->wa_init_lt_specular_index,1,light_specular);
    glUniform3fv(ce->wa_init_lt_direction_index,1,light_direction);
    glUniform3fv(ce->wa_init_lt_halfvect_index,1,half_vector);
    glUniform4fv(ce->wa_init_specular_index,1,mat_specular);
    glUniform1f(ce->wa_init_shininess_index,50.0f);

    if (ce->colors == COLORS_TWOSIDED)
    {
      glUniform4fv(ce->wa_init_front_ambient_index,1,mat_diff_magenta_trans);
      glUniform4fv(ce->wa_init_front_diffuse_index,1,mat_diff_magenta_trans);
      glUniform4fv(ce->wa_init_back_ambient_index,1,mat_diff_cyan_trans);
      glUniform4fv(ce->wa_init_back_diffuse_index,1,mat_diff_cyan_trans);
    }
    else
    {
      glUniform4fv(ce->wa_init_front_ambient_index,1,mat_diff_white_trans);
      glUniform4fv(ce->wa_init_front_diffuse_index,1,mat_diff_white_trans);
      glUniform4fv(ce->wa_init_back_ambient_index,1,mat_diff_white_trans);
      glUniform4fv(ce->wa_init_back_diffuse_index,1,mat_diff_white_trans);
    }

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D,ce->tex_names[0]);
    glUniform1i(ce->wa_init_tex_samp_f_index,2);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D,ce->tex_names[1]);
    glUniform1i(ce->wa_init_tex_samp_b_index,3);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D,ce->tex_names[2]);
    glUniform1i(ce->wa_init_tex_samp_w_index,4);
    glUniform1i(ce->wa_init_bool_textures_index,ce->colors == COLORS_EARTH);
    glActiveTexture(GL_TEXTURE0);

    glEnableVertexAttribArray(ce->wa_init_pos_index);
    glBindBuffer(GL_ARRAY_BUFFER,ce->cuboct_pos_buffer);
    glVertexAttribPointer(ce->wa_init_pos_index,3,GL_FLOAT,GL_FALSE,0,0);

    glEnableVertexAttribArray(ce->wa_init_normal_index);
    glBindBuffer(GL_ARRAY_BUFFER,ce->cuboct_normal_buffer);
    glVertexAttribPointer(ce->wa_init_normal_index,3,GL_FLOAT,GL_FALSE,0,0);

    glEnableVertexAttribArray(ce->wa_init_colorf_index);
    glBindBuffer(GL_ARRAY_BUFFER,ce->cuboct_colorf_buffer);
    glVertexAttribPointer(ce->wa_init_colorf_index,4,GL_FLOAT,GL_FALSE,0,0);

    glEnableVertexAttribArray(ce->wa_init_colorb_index);
    glBindBuffer(GL_ARRAY_BUFFER,ce->cuboct_colorb_buffer);
    glVertexAttribPointer(ce->wa_init_colorb_index,4,GL_FLOAT,GL_FALSE,0,0);

    glEnableVertexAttribArray(ce->wa_init_vertex_tex_index);
    glBindBuffer(GL_ARRAY_BUFFER,ce->cuboct_tex_coord_buffer);
    glVertexAttribPointer(ce->wa_init_vertex_tex_index,3,GL_FLOAT,GL_FALSE,
                          0,0);

    glUniformMatrix4fv(ce->wa_init_mv_index,1,GL_FALSE,mv_mat);

    glDrawArrays(GL_TRIANGLES,0,3*NUM_FACE);
    polys += NUM_FACE;

    glDisableVertexAttribArray(ce->wa_init_pos_index);
    glDisableVertexAttribArray(ce->wa_init_normal_index);
    glDisableVertexAttribArray(ce->wa_init_colorf_index);
    glDisableVertexAttribArray(ce->wa_init_colorb_index);
    glDisableVertexAttribArray(ce->wa_init_vertex_tex_index);

    glBindBuffer(GL_ARRAY_BUFFER,0);

    glUseProgram(0);

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    /* Approximate blending. */
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,ce->dp_draw_fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,ce->dp_read_fbo);
#if (defined(HAVE_COCOA) || defined(__APPLE__)) && !defined(HAVE_IPHONE)
    glDrawBuffer(GL_BACK);
#else
    glDrawBuffers(1,&draw_buffers[3]);
#endif
    glUseProgram(ce->wa_finl_shader_program);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D,ce->wa_color_tex);
    glUniform1i(ce->wa_finl_tex_samp_color,1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D,ce->wa_num_tex);
    glUniform1i(ce->wa_finl_tex_samp_num,2);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D,ce->wa_opaque_color_tex);
    glUniform1i(ce->wa_finl_tex_samp_opaque,3);
    glActiveTexture(GL_TEXTURE0);

    glUniform2fv(ce->wa_finl_tex_scale_index,1,ce->tex_scale);

    glEnableVertexAttribArray(ce->wa_finl_pos_index);
    glBindBuffer(GL_ARRAY_BUFFER,ce->quad_buffer);
    glVertexAttribPointer(ce->wa_finl_pos_index,2,GL_FLOAT,GL_FALSE,0,0);

    glDrawArrays(GL_TRIANGLE_STRIP,0,4);

    glDisableVertexAttribArray(ce->wa_finl_pos_index);

    glBindBuffer(GL_ARRAY_BUFFER,0);

    glUseProgram(0);

    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
  }

  return polys;
}


/* Set up and enable texturing on our object */
static void setup_xpm_texture(ModeInfo *mi, const unsigned char *data,
                              unsigned long size)
{
  cubocteversionstruct *ce = &cubocteversion[MI_SCREEN(mi)];
  XImage *image;
  const char *gl_ext;
  GLboolean have_aniso;
  GLint max_aniso, aniso;

  image = image_data_to_ximage(MI_DISPLAY(mi),MI_VISUAL(mi),data,size);
#ifndef HAVE_JWZGLES
  glEnable(GL_TEXTURE_2D);
#endif
  glPixelStorei(GL_UNPACK_ALIGNMENT,1);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,image->width,image->height,0,GL_RGBA,
               GL_UNSIGNED_BYTE,image->data);
  if (ce->use_mipmaps)
    glGenerateMipmap(GL_TEXTURE_2D);
  /* Limit the maximum mipmap level to 4 to avoid texture interpolation
     artifacts at the poles. */
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,4);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  if (ce->use_mipmaps)
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
  else
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  gl_ext = (const char *)glGetString(GL_EXTENSIONS);
  if (gl_ext == NULL)
    have_aniso = GL_FALSE;
  else
    have_aniso = (strstr(gl_ext,"GL_EXT_texture_filter_anisotropic") != NULL ||
                  strstr(gl_ext,"GL_ARB_texture_filter_anisotropic") != NULL);
  if (have_aniso)
  {
    glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,&max_aniso);
    aniso = MIN(max_aniso,10);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_ANISOTROPY_EXT,aniso);
  }
  XDestroyImage(image);
}


/* Generate the textures that show the the earth by day and night. */
static void gen_textures(ModeInfo *mi)
{
  cubocteversionstruct *ce = &cubocteversion[MI_SCREEN(mi)];

  glGenTextures(3,ce->tex_names);

  /* Set up the earth by day texture. */
  glBindTexture(GL_TEXTURE_2D,ce->tex_names[0]);
  setup_xpm_texture(mi,earth_png,sizeof(earth_png));

  /* Set up the earth by night texture. */
  glBindTexture(GL_TEXTURE_2D,ce->tex_names[1]);
  setup_xpm_texture(mi,earth_night_png,sizeof(earth_night_png));

  /* Set up the earth water texture. */
  glBindTexture(GL_TEXTURE_2D,ce->tex_names[2]);
  setup_xpm_texture(mi,earth_water_png,sizeof(earth_water_png));

  glBindTexture(GL_TEXTURE_2D,0);
}


/* Delete the dual depth peeling render targets. */
static void delete_dual_peeling_render_targets(ModeInfo *mi)
{
  cubocteversionstruct *ce = &cubocteversion[MI_SCREEN(mi)];

  if (ce->use_shaders && ce->transparency == TRANSPARENCY_CORRECT)
  {
    glDeleteFramebuffers(2,ce->dp_peeling_fbo);
    glDeleteFramebuffers(2,ce->dp_color_fbo);
    glDeleteFramebuffers(1,&ce->dp_back_blender_fbo);
    glDeleteFramebuffers(1,&ce->dp_opaque_fbo);
    glDeleteTextures(2,ce->dp_depth_tex);
    glDeleteTextures(2,ce->dp_front_blender_tex);
    glDeleteTextures(2,ce->dp_back_temp_tex);
    glDeleteTextures(1,&ce->dp_back_blender_tex);
    glDeleteTextures(1,&ce->dp_opaque_depth_tex);
    glDeleteTextures(1,&ce->dp_opaque_color_tex);
  }
}


/* Initialize the dual depth peeling render targets. */
static void init_dual_peeling_render_targets(ModeInfo *mi)
{
  cubocteversionstruct *ce = &cubocteversion[MI_SCREEN(mi)];
  int i;
  GLenum status;
  Bool all_fbo_complete = True;
#ifdef HAVE_JWZGLES
  const GLint rgba_int = GL_RGBA16F;
  const GLenum float_format = GL_HALF_FLOAT;
#else
  const GLint rgba_int = GL_RGBA;
  const GLenum float_format = GL_FLOAT;
#endif

  if (ce->use_shaders && ce->transparency == TRANSPARENCY_CORRECT)
  {
    glGenFramebuffers(2,ce->dp_peeling_fbo);
    glGenFramebuffers(2,ce->dp_color_fbo);
    glGenFramebuffers(1,&ce->dp_back_blender_fbo);
    glGenFramebuffers(1,&ce->dp_opaque_fbo);

    glGenTextures(2,ce->dp_depth_tex);
    glGenTextures(2,ce->dp_front_blender_tex);
    glGenTextures(2,ce->dp_back_temp_tex);

    for (i=0; i<2; i++)
    {
      glBindTexture(GL_TEXTURE_2D,ce->dp_depth_tex[i]);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
      glTexImage2D(GL_TEXTURE_2D,0,GL_RG32F,ce->WindW,ce->WindH,0,GL_RG,
                   GL_FLOAT,0);

      glBindTexture(GL_TEXTURE_2D,ce->dp_front_blender_tex[i]);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
      glTexImage2D(GL_TEXTURE_2D,0,rgba_int,ce->WindW,ce->WindH,0,GL_RGBA,
                   float_format,0);

      glBindTexture(GL_TEXTURE_2D,ce->dp_back_temp_tex[i]);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
      glTexImage2D(GL_TEXTURE_2D,0,rgba_int,ce->WindW,ce->WindH,0,GL_RGBA,
                   float_format,0);

      glBindFramebuffer(GL_FRAMEBUFFER,ce->dp_peeling_fbo[i]);
      glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,
                             ce->dp_depth_tex[i],0);
      glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D,
                             ce->dp_front_blender_tex[i],0);
      glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT2,GL_TEXTURE_2D,
                             ce->dp_back_temp_tex[i],0);
      status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
      if (status != GL_FRAMEBUFFER_COMPLETE)
        all_fbo_complete = False;

      glBindFramebuffer(GL_FRAMEBUFFER,ce->dp_color_fbo[i]);
      glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,
                             ce->dp_front_blender_tex[i],0);
      glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D,
                             ce->dp_back_temp_tex[i],0);
      status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
      if (status != GL_FRAMEBUFFER_COMPLETE)
        all_fbo_complete = False;
    }

    glGenTextures(1,&ce->dp_back_blender_tex);
    glBindTexture(GL_TEXTURE_2D,ce->dp_back_blender_tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,0,rgba_int,ce->WindW,ce->WindH,0,GL_RGBA,
                 float_format,0);

    glBindFramebuffer(GL_FRAMEBUFFER,ce->dp_back_blender_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,
                           ce->dp_back_blender_tex,0);
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
      all_fbo_complete = False;

    glGenTextures(1,&ce->dp_opaque_depth_tex);
    glBindTexture(GL_TEXTURE_2D,ce->dp_opaque_depth_tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT32F,ce->WindW,ce->WindH,0,
                 GL_DEPTH_COMPONENT,GL_FLOAT,0);

    glGenTextures(1,&ce->dp_opaque_color_tex);
    glBindTexture(GL_TEXTURE_2D,ce->dp_opaque_color_tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,0,rgba_int,ce->WindW,ce->WindH,0,GL_RGBA,
                 float_format,0);

    glBindFramebuffer(GL_FRAMEBUFFER,ce->dp_opaque_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,
                           ce->dp_opaque_depth_tex,0);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,
                           ce->dp_opaque_color_tex,0);
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
      all_fbo_complete = False;

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,ce->dp_draw_fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,ce->dp_read_fbo);
    if (!all_fbo_complete)
    {
      delete_dual_peeling_render_targets(mi);
      ce->transparency = TRANSPARENCY_STANDARD;
    }
  }
}


/* Delete the weighted average render targets. */
static void delete_weighted_average_render_targets(ModeInfo *mi)
{
  cubocteversionstruct *ce = &cubocteversion[MI_SCREEN(mi)];

  if (ce->use_shaders && ce->transparency == TRANSPARENCY_APPROXIMATE)
  {
    glDeleteFramebuffers(1,&ce->wa_opaque_fbo);
    glDeleteFramebuffers(1,&ce->wa_accumulation_fbo);
    glDeleteTextures(1,&ce->wa_opaque_depth_tex);
    glDeleteTextures(1,&ce->wa_opaque_color_tex);
    glDeleteTextures(1,&ce->wa_color_tex);
    glDeleteTextures(1,&ce->wa_num_tex);
  }
}


/* Initialize the weighted average render targets. */
static void init_weighted_average_render_targets(ModeInfo *mi)
{
  cubocteversionstruct *ce = &cubocteversion[MI_SCREEN(mi)];
  GLenum status;
  Bool all_fbo_complete = True;
#ifdef HAVE_JWZGLES
  const GLint rgba_int = GL_RGBA16F;
  const GLint r_int = GL_R16F;
  const GLenum float_format = GL_HALF_FLOAT;
#else
  const GLint rgba_int = GL_RGBA;
  const GLint r_int = GL_RED;
  const GLenum float_format = GL_FLOAT;
#endif

  if (ce->use_shaders && ce->transparency == TRANSPARENCY_APPROXIMATE)
  {
    glGenTextures(1,&ce->wa_opaque_depth_tex);
    glBindTexture(GL_TEXTURE_2D,ce->wa_opaque_depth_tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT32F,ce->WindW,ce->WindH,0,
                 GL_DEPTH_COMPONENT,GL_FLOAT,0);

    glGenTextures(1,&ce->wa_opaque_color_tex);
    glBindTexture(GL_TEXTURE_2D,ce->wa_opaque_color_tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,0,rgba_int,ce->WindW,ce->WindH,0,
                 GL_RGBA,float_format,0);

    glGenFramebuffers(1,&ce->wa_opaque_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER,ce->wa_opaque_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,
                           ce->wa_opaque_depth_tex,0);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,
                           ce->wa_opaque_color_tex,0);
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
      all_fbo_complete = False;

    glGenTextures(1,&ce->wa_color_tex);
    glBindTexture(GL_TEXTURE_2D,ce->wa_color_tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,0,rgba_int,ce->WindW,ce->WindH,0,
                 GL_RGBA,float_format,0);

    glGenTextures(1,&ce->wa_num_tex);
    glBindTexture(GL_TEXTURE_2D,ce->wa_num_tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,0,r_int,ce->WindW,ce->WindH,0,GL_RED,
                 float_format,0);

    glGenFramebuffers(1,&ce->wa_accumulation_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER,ce->wa_accumulation_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,
                           ce->wa_opaque_depth_tex,0);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,
                           ce->wa_color_tex,0);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D,
                           ce->wa_num_tex,0);
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
      all_fbo_complete = False;

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,ce->dp_draw_fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,ce->dp_read_fbo);
    if (!all_fbo_complete)
    {
      delete_weighted_average_render_targets(mi);
      ce->transparency = TRANSPARENCY_STANDARD;
    }
  }
}


static void init_glsl(ModeInfo *mi)
{
  cubocteversionstruct *ce = &cubocteversion[MI_SCREEN(mi)];
  GLint gl_major, gl_minor, glsl_major, glsl_minor;
  GLboolean gl_gles3;
  const char *gl_ext;
  const GLchar *poly_vertex_shader_source[3];
  const GLchar *poly_fragment_shader_source[4];
  const GLchar *dp_init_vertex_shader_source[3];
  const GLchar *dp_init_fragment_shader_source[4];
  const GLchar *dp_peel_vertex_shader_source[3];
  const GLchar *dp_peel_fragment_shader_source[7];
  const GLchar *dp_blnd_vertex_shader_source[3];
  const GLchar *dp_blnd_fragment_shader_source[4];
  const GLchar *dp_finl_vertex_shader_source[3];
  const GLchar *dp_finl_fragment_shader_source[4];
  const GLchar *wa_init_vertex_shader_source[3];
  const GLchar *wa_init_fragment_shader_source[4];
  const GLchar *wa_finl_vertex_shader_source[3];
  const GLchar *wa_finl_fragment_shader_source[4];
  static const GLfloat quad_verts[] =
  {
    -1.0f, -1.0f,
     1.0f, -1.0f,
    -1.0f,  1.0f,
     1.0f,  1.0f,
  };

  /* Determine whether to use shaders to render the cuboctahedron eversion. */
  ce->use_shaders = False;
  ce->use_mipmaps = False;
  ce->poly_shader_program = 0;
  ce->dp_init_shader_program = 0;
  ce->dp_peel_shader_program = 0;
  ce->dp_blnd_shader_program = 0;
  ce->dp_finl_shader_program = 0;
  ce->wa_init_shader_program = 0;
  ce->wa_finl_shader_program = 0;

  if (!glsl_GetGlAndGlslVersions(&gl_major,&gl_minor,&glsl_major,&glsl_minor,
                                 &gl_gles3))
    return;

  if (!gl_gles3)
  {
    if (gl_major < 3 ||
        (glsl_major < 1 || (glsl_major == 1 && glsl_minor < 30)))
    {
      if ((gl_major < 2 || (gl_major == 2 && gl_minor < 1)) ||
          (glsl_major < 1 || (glsl_major == 1 && glsl_minor < 20)))
        return;
      /* We have at least OpenGL 2.1 and at least GLSL 1.20. */
      poly_vertex_shader_source[0] = shader_version_2_1;
      poly_vertex_shader_source[1] = poly_vertex_shader_attribs_2_1;
      poly_vertex_shader_source[2] = poly_vertex_shader_main;
      poly_fragment_shader_source[0] = shader_version_2_1;
      poly_fragment_shader_source[1] = poly_fragment_shader_attribs_2_1;
      poly_fragment_shader_source[2] = poly_fragment_shader_main;
      poly_fragment_shader_source[3] = poly_fragment_shader_out_2_1;

      dp_init_vertex_shader_source[0] = shader_version_2_1;
      dp_init_vertex_shader_source[1] = dp_init_vertex_shader_attribs_2_1;
      dp_init_vertex_shader_source[2] = dp_init_vertex_shader_main;
      dp_init_fragment_shader_source[0] = shader_version_2_1;
      dp_init_fragment_shader_source[1] = dp_init_fragment_shader_attribs_2_1;
      dp_init_fragment_shader_source[2] = dp_init_fragment_shader_main;
      dp_init_fragment_shader_source[3] = dp_init_fragment_shader_out_2_1;

      dp_peel_vertex_shader_source[0] = shader_version_2_1;
      dp_peel_vertex_shader_source[1] = dp_peel_vertex_shader_attribs_2_1;
      dp_peel_vertex_shader_source[2] = dp_peel_vertex_shader_main;
      dp_peel_fragment_shader_source[0] = shader_version_2_1;
      dp_peel_fragment_shader_source[1] = dp_peel_fragment_shader_attribs_2_1;
      dp_peel_fragment_shader_source[2] = dp_peel_fragment_shader_main;
      dp_peel_fragment_shader_source[3] = dp_peel_fragment_shader_peel_2_1;
      dp_peel_fragment_shader_source[4] = dp_peel_fragment_shader_col;
      dp_peel_fragment_shader_source[5] = dp_peel_fragment_shader_col_2_1;
      dp_peel_fragment_shader_source[6] = dp_peel_fragment_shader_out_2_1;

      dp_blnd_vertex_shader_source[0] = shader_version_2_1;
      dp_blnd_vertex_shader_source[1] = dp_blnd_vertex_shader_attribs_2_1;
      dp_blnd_vertex_shader_source[2] = dp_blnd_vertex_shader_main;
      dp_blnd_fragment_shader_source[0] = shader_version_2_1;
      dp_blnd_fragment_shader_source[1] = dp_blnd_fragment_shader_attribs_2_1;
      dp_blnd_fragment_shader_source[2] = dp_blnd_fragment_shader_main;
      dp_blnd_fragment_shader_source[3] = dp_blnd_fragment_shader_out_2_1;

      dp_finl_vertex_shader_source[0] = shader_version_2_1;
      dp_finl_vertex_shader_source[1] = dp_finl_vertex_shader_attribs_2_1;
      dp_finl_vertex_shader_source[2] = dp_finl_vertex_shader_main;
      dp_finl_fragment_shader_source[0] = shader_version_2_1;
      dp_finl_fragment_shader_source[1] = dp_finl_fragment_shader_attribs_2_1;
      dp_finl_fragment_shader_source[2] = dp_finl_fragment_shader_main;
      dp_finl_fragment_shader_source[3] = dp_finl_fragment_shader_out_2_1;

      wa_init_vertex_shader_source[0] = shader_version_2_1;
      wa_init_vertex_shader_source[1] = wa_init_vertex_shader_attribs_2_1;
      wa_init_vertex_shader_source[2] = wa_init_vertex_shader_main;
      wa_init_fragment_shader_source[0] = shader_version_2_1;
      wa_init_fragment_shader_source[1] = wa_init_fragment_shader_attribs_2_1;
      wa_init_fragment_shader_source[2] = wa_init_fragment_shader_main;
      wa_init_fragment_shader_source[3] = wa_init_fragment_shader_out_2_1;

      wa_finl_vertex_shader_source[0] = shader_version_2_1;
      wa_finl_vertex_shader_source[1] = wa_finl_vertex_shader_attribs_2_1;
      wa_finl_vertex_shader_source[2] = wa_finl_vertex_shader_main;
      wa_finl_fragment_shader_source[0] = shader_version_2_1;
      wa_finl_fragment_shader_source[1] = wa_finl_fragment_shader_attribs_2_1;
      wa_finl_fragment_shader_source[2] = wa_finl_fragment_shader_main;
      wa_finl_fragment_shader_source[3] = wa_finl_fragment_shader_out_2_1;
    }
    else
    {
      /* We have at least OpenGL 3.0 and at least GLSL 1.30. */
      poly_vertex_shader_source[0] = shader_version_3_0;
      poly_vertex_shader_source[1] = poly_vertex_shader_attribs_3_0;
      poly_vertex_shader_source[2] = poly_vertex_shader_main;
      poly_fragment_shader_source[0] = shader_version_3_0;
      poly_fragment_shader_source[1] = poly_fragment_shader_attribs_3_0;
      poly_fragment_shader_source[2] = poly_fragment_shader_main;
      poly_fragment_shader_source[3] = poly_fragment_shader_out_3_0;

      dp_init_vertex_shader_source[0] = shader_version_3_0;
      dp_init_vertex_shader_source[1] = dp_init_vertex_shader_attribs_3_0;
      dp_init_vertex_shader_source[2] = dp_init_vertex_shader_main;
      dp_init_fragment_shader_source[0] = shader_version_3_0;
      dp_init_fragment_shader_source[1] = dp_init_fragment_shader_attribs_3_0;
      dp_init_fragment_shader_source[2] = dp_init_fragment_shader_main;
      dp_init_fragment_shader_source[3] = dp_init_fragment_shader_out_3_0;

      dp_peel_vertex_shader_source[0] = shader_version_3_0;
      dp_peel_vertex_shader_source[1] = dp_peel_vertex_shader_attribs_3_0;
      dp_peel_vertex_shader_source[2] = dp_peel_vertex_shader_main;
      dp_peel_fragment_shader_source[0] = shader_version_3_0;
      dp_peel_fragment_shader_source[1] = dp_peel_fragment_shader_attribs_3_0;
      dp_peel_fragment_shader_source[2] = dp_peel_fragment_shader_main;
      dp_peel_fragment_shader_source[3] = dp_peel_fragment_shader_peel_3_0;
      dp_peel_fragment_shader_source[4] = dp_peel_fragment_shader_col;
      dp_peel_fragment_shader_source[5] = dp_peel_fragment_shader_col_3_0;
      dp_peel_fragment_shader_source[6] = dp_peel_fragment_shader_out_3_0;

      dp_blnd_vertex_shader_source[0] = shader_version_3_0;
      dp_blnd_vertex_shader_source[1] = dp_blnd_vertex_shader_attribs_3_0;
      dp_blnd_vertex_shader_source[2] = dp_blnd_vertex_shader_main;
      dp_blnd_fragment_shader_source[0] = shader_version_3_0;
      dp_blnd_fragment_shader_source[1] = dp_blnd_fragment_shader_attribs_3_0;
      dp_blnd_fragment_shader_source[2] = dp_blnd_fragment_shader_main;
      dp_blnd_fragment_shader_source[3] = dp_blnd_fragment_shader_out_3_0;

      dp_finl_vertex_shader_source[0] = shader_version_3_0;
      dp_finl_vertex_shader_source[1] = dp_finl_vertex_shader_attribs_3_0;
      dp_finl_vertex_shader_source[2] = dp_finl_vertex_shader_main;
      dp_finl_fragment_shader_source[0] = shader_version_3_0;
      dp_finl_fragment_shader_source[1] = dp_finl_fragment_shader_attribs_3_0;
      dp_finl_fragment_shader_source[2] = dp_finl_fragment_shader_main;
      dp_finl_fragment_shader_source[3] = dp_finl_fragment_shader_out_3_0;

      wa_init_vertex_shader_source[0] = shader_version_3_0;
      wa_init_vertex_shader_source[1] = wa_init_vertex_shader_attribs_3_0;
      wa_init_vertex_shader_source[2] = wa_init_vertex_shader_main;
      wa_init_fragment_shader_source[0] = shader_version_3_0;
      wa_init_fragment_shader_source[1] = wa_init_fragment_shader_attribs_3_0;
      wa_init_fragment_shader_source[2] = wa_init_fragment_shader_main;
      wa_init_fragment_shader_source[3] = wa_init_fragment_shader_out_3_0;

      wa_finl_vertex_shader_source[0] = shader_version_3_0;
      wa_finl_vertex_shader_source[1] = wa_finl_vertex_shader_attribs_3_0;
      wa_finl_vertex_shader_source[2] = wa_finl_vertex_shader_main;
      wa_finl_fragment_shader_source[0] = shader_version_3_0;
      wa_finl_fragment_shader_source[1] = wa_finl_fragment_shader_attribs_3_0;
      wa_finl_fragment_shader_source[2] = wa_finl_fragment_shader_main;
      wa_finl_fragment_shader_source[3] = wa_finl_fragment_shader_out_3_0;
    }
  }
  else /* gl_gles3 */
  {
    if (gl_major < 3 || glsl_major < 3)
      return;
    /* We have at least OpenGL ES 3.0 and at least GLSL ES 3.0. */
    poly_vertex_shader_source[0] = shader_version_3_0_es;
    poly_vertex_shader_source[1] = poly_vertex_shader_attribs_3_0;
    poly_vertex_shader_source[2] = poly_vertex_shader_main;
    poly_fragment_shader_source[0] = shader_version_3_0_es;
    poly_fragment_shader_source[1] = poly_fragment_shader_attribs_3_0;
    poly_fragment_shader_source[2] = poly_fragment_shader_main;
    poly_fragment_shader_source[3] = poly_fragment_shader_out_3_0;

    dp_init_vertex_shader_source[0] = shader_version_3_0_es;
    dp_init_vertex_shader_source[1] = dp_init_vertex_shader_attribs_3_0;
    dp_init_vertex_shader_source[2] = dp_init_vertex_shader_main;
    dp_init_fragment_shader_source[0] = shader_version_3_0_es;
    dp_init_fragment_shader_source[1] = dp_init_fragment_shader_attribs_3_0;
    dp_init_fragment_shader_source[2] = dp_init_fragment_shader_main;
    dp_init_fragment_shader_source[3] = dp_init_fragment_shader_out_3_0;

    dp_peel_vertex_shader_source[0] = shader_version_3_0_es;
    dp_peel_vertex_shader_source[1] = dp_peel_vertex_shader_attribs_3_0;
    dp_peel_vertex_shader_source[2] = dp_peel_vertex_shader_main;
    dp_peel_fragment_shader_source[0] = shader_version_3_0_es;
    dp_peel_fragment_shader_source[1] = dp_peel_fragment_shader_attribs_3_0_es;
    dp_peel_fragment_shader_source[2] = dp_peel_fragment_shader_main;
    dp_peel_fragment_shader_source[3] = dp_peel_fragment_shader_peel_3_0_es;
    dp_peel_fragment_shader_source[4] = dp_peel_fragment_shader_col;
    dp_peel_fragment_shader_source[5] = dp_peel_fragment_shader_col_3_0;
    dp_peel_fragment_shader_source[6] = dp_peel_fragment_shader_out_3_0_es;

    dp_blnd_vertex_shader_source[0] = shader_version_3_0_es;
    dp_blnd_vertex_shader_source[1] = dp_blnd_vertex_shader_attribs_3_0;
    dp_blnd_vertex_shader_source[2] = dp_blnd_vertex_shader_main;
    dp_blnd_fragment_shader_source[0] = shader_version_3_0_es;
    dp_blnd_fragment_shader_source[1] = dp_blnd_fragment_shader_attribs_3_0;
    dp_blnd_fragment_shader_source[2] = dp_blnd_fragment_shader_main;
    dp_blnd_fragment_shader_source[3] = dp_blnd_fragment_shader_out_3_0;

    dp_finl_vertex_shader_source[0] = shader_version_3_0_es;
    dp_finl_vertex_shader_source[1] = dp_finl_vertex_shader_attribs_3_0;
    dp_finl_vertex_shader_source[2] = dp_finl_vertex_shader_main;
    dp_finl_fragment_shader_source[0] = shader_version_3_0_es;
    dp_finl_fragment_shader_source[1] = dp_finl_fragment_shader_attribs_3_0;
    dp_finl_fragment_shader_source[2] = dp_finl_fragment_shader_main;
    dp_finl_fragment_shader_source[3] = dp_finl_fragment_shader_out_3_0;

    wa_init_vertex_shader_source[0] = shader_version_3_0_es;
    wa_init_vertex_shader_source[1] = wa_init_vertex_shader_attribs_3_0;
    wa_init_vertex_shader_source[2] = wa_init_vertex_shader_main;
    wa_init_fragment_shader_source[0] = shader_version_3_0_es;
    wa_init_fragment_shader_source[1] = wa_init_fragment_shader_attribs_3_0_es;
    wa_init_fragment_shader_source[2] = wa_init_fragment_shader_main;
    wa_init_fragment_shader_source[3] = wa_init_fragment_shader_out_3_0_es;

    wa_finl_vertex_shader_source[0] = shader_version_3_0_es;
    wa_finl_vertex_shader_source[1] = wa_finl_vertex_shader_attribs_3_0;
    wa_finl_vertex_shader_source[2] = wa_finl_vertex_shader_main;
    wa_finl_fragment_shader_source[0] = shader_version_3_0_es;
    wa_finl_fragment_shader_source[1] = wa_finl_fragment_shader_attribs_3_0;
    wa_finl_fragment_shader_source[2] = wa_finl_fragment_shader_main;
    wa_finl_fragment_shader_source[3] = wa_finl_fragment_shader_out_3_0;
  }

  if (!glsl_CompileAndLinkShaders(3,poly_vertex_shader_source,
                                  4,poly_fragment_shader_source,
                                  &ce->poly_shader_program))
    return;

  ce->poly_pos_index =
    glGetAttribLocation(ce->poly_shader_program,"VertexPosition");
  ce->poly_normal_index =
    glGetAttribLocation(ce->poly_shader_program,"VertexNormal");
  ce->poly_colorf_index =
    glGetAttribLocation(ce->poly_shader_program,"VertexColorF");
  ce->poly_colorb_index =
    glGetAttribLocation(ce->poly_shader_program,"VertexColorB");
  ce->poly_vertex_tex_index =
    glGetAttribLocation(ce->poly_shader_program,"VertexTexCoord");
  ce->poly_mv_index =
    glGetUniformLocation(ce->poly_shader_program,"MatModelView");
  ce->poly_proj_index =
    glGetUniformLocation(ce->poly_shader_program,"MatProj");
  ce->poly_glbl_ambient_index =
    glGetUniformLocation(ce->poly_shader_program,"LtGlblAmbient");
  ce->poly_lt_ambient_index =
    glGetUniformLocation(ce->poly_shader_program,"LtAmbient");
  ce->poly_lt_diffuse_index =
    glGetUniformLocation(ce->poly_shader_program,"LtDiffuse");
  ce->poly_lt_specular_index =
    glGetUniformLocation(ce->poly_shader_program,"LtSpecular");
  ce->poly_lt_direction_index =
    glGetUniformLocation(ce->poly_shader_program,"LtDirection");
  ce->poly_lt_halfvect_index =
    glGetUniformLocation(ce->poly_shader_program,"LtHalfVector");
  ce->poly_front_ambient_index =
    glGetUniformLocation(ce->poly_shader_program,"MatFrontAmbient");
  ce->poly_back_ambient_index =
    glGetUniformLocation(ce->poly_shader_program,"MatBackAmbient");
  ce->poly_front_diffuse_index =
    glGetUniformLocation(ce->poly_shader_program,"MatFrontDiffuse");
  ce->poly_back_diffuse_index =
    glGetUniformLocation(ce->poly_shader_program,"MatBackDiffuse");
  ce->poly_specular_index =
    glGetUniformLocation(ce->poly_shader_program,"MatSpecular");
  ce->poly_shininess_index =
    glGetUniformLocation(ce->poly_shader_program,"MatShininess");
  ce->poly_bool_textures_index =
    glGetUniformLocation(ce->poly_shader_program,"BoolTextures");
  ce->poly_tex_samp_f_index =
    glGetUniformLocation(ce->poly_shader_program,"TextureSamplerFront");
  ce->poly_tex_samp_b_index =
    glGetUniformLocation(ce->poly_shader_program,"TextureSamplerBack");
  ce->poly_tex_samp_w_index =
    glGetUniformLocation(ce->poly_shader_program,"TextureSamplerWater");
  if (ce->poly_pos_index == -1 ||
      ce->poly_normal_index == -1 ||
      ce->poly_colorf_index == -1 ||
      ce->poly_colorb_index == -1 ||
      ce->poly_mv_index == -1 ||
      ce->poly_proj_index == -1 ||
      ce->poly_glbl_ambient_index == -1 ||
      ce->poly_lt_ambient_index == -1 ||
      ce->poly_lt_diffuse_index == -1 ||
      ce->poly_lt_specular_index == -1 ||
      ce->poly_lt_direction_index == -1 ||
      ce->poly_lt_halfvect_index == -1 ||
      ce->poly_front_ambient_index == -1 ||
      ce->poly_back_ambient_index == -1 ||
      ce->poly_front_diffuse_index == -1 ||
      ce->poly_back_diffuse_index == -1 ||
      ce->poly_specular_index == -1 ||
      ce->poly_shininess_index == -1 ||
      ce->poly_bool_textures_index == -1 ||
      ce->poly_tex_samp_f_index == -1 ||
      ce->poly_tex_samp_b_index == -1 ||
      ce->poly_tex_samp_w_index == -1)
  {
    glDeleteProgram(ce->poly_shader_program);
    return;
  }

  glGenBuffers(1,&ce->cuboct_pos_buffer);
  glGenBuffers(1,&ce->cuboct_normal_buffer);
  glGenBuffers(1,&ce->cuboct_colorf_buffer);
  glGenBuffers(1,&ce->cuboct_colorb_buffer);
  glGenBuffers(1,&ce->cuboct_tex_coord_buffer);
  glGenBuffers(1,&ce->tube_vertex_buffer);
  glGenBuffers(1,&ce->tube_normal_buffer);
  glGenBuffers(1,&ce->tube_colorf_buffer);
  glGenBuffers(1,&ce->tube_colorb_buffer);
  glGenBuffers(1,&ce->tube_index_buffer);
  glGenBuffers(1,&ce->self_vertex_buffer);
  glGenBuffers(1,&ce->self_normal_buffer);
  glGenBuffers(1,&ce->self_colorf_buffer);
  glGenBuffers(1,&ce->self_colorb_buffer);
  glGenBuffers(1,&ce->self_index_buffer);

  /* Initialize dual depth peeling data. */
  if (!glsl_CompileAndLinkShaders(3,dp_init_vertex_shader_source,
                                  4,dp_init_fragment_shader_source,
                                  &ce->dp_init_shader_program))
  {
    glDeleteProgram(ce->poly_shader_program);
    return;
  }

  ce->dp_init_pos_index =
    glGetAttribLocation(ce->dp_init_shader_program,"VertexPosition");
  ce->dp_init_mv_index =
    glGetUniformLocation(ce->dp_init_shader_program,"MatModelView");
  ce->dp_init_proj_index =
    glGetUniformLocation(ce->dp_init_shader_program,"MatProj");
  ce->dp_init_tex_scale_index =
    glGetUniformLocation(ce->dp_init_shader_program,"TexScale");
  ce->dp_init_tex_samp_depth =
    glGetUniformLocation(ce->dp_init_shader_program,"TexSamplerOpaqueDepth");
  if (ce->dp_init_pos_index == -1 ||
      ce->dp_init_mv_index == -1 ||
      ce->dp_init_proj_index == -1 ||
      ce->dp_init_tex_scale_index == -1 ||
      ce->dp_init_tex_samp_depth == -1)
  {
    glDeleteProgram(ce->poly_shader_program);
    glDeleteProgram(ce->dp_init_shader_program);
    return;
  }

  if (!glsl_CompileAndLinkShaders(3,dp_peel_vertex_shader_source,
                                  7,dp_peel_fragment_shader_source,
                                  &ce->dp_peel_shader_program))
  {
    glDeleteProgram(ce->poly_shader_program);
    glDeleteProgram(ce->dp_init_shader_program);
    return;
  }

  ce->dp_peel_pos_index =
    glGetAttribLocation(ce->dp_peel_shader_program,"VertexPosition");
  ce->dp_peel_normal_index =
    glGetAttribLocation(ce->dp_peel_shader_program,"VertexNormal");
  ce->dp_peel_colorf_index =
    glGetAttribLocation(ce->dp_peel_shader_program,"VertexColorF");
  ce->dp_peel_colorb_index =
    glGetAttribLocation(ce->dp_peel_shader_program,"VertexColorB");
  ce->dp_peel_vertex_tex_index =
    glGetAttribLocation(ce->dp_peel_shader_program,"VertexTexCoord");
  ce->dp_peel_mv_index =
    glGetUniformLocation(ce->dp_peel_shader_program,"MatModelView");
  ce->dp_peel_proj_index =
    glGetUniformLocation(ce->dp_peel_shader_program,"MatProj");
  ce->dp_peel_glbl_ambient_index =
    glGetUniformLocation(ce->dp_peel_shader_program,"LtGlblAmbient");
  ce->dp_peel_lt_ambient_index =
    glGetUniformLocation(ce->dp_peel_shader_program,"LtAmbient");
  ce->dp_peel_lt_diffuse_index =
    glGetUniformLocation(ce->dp_peel_shader_program,"LtDiffuse");
  ce->dp_peel_lt_specular_index =
    glGetUniformLocation(ce->dp_peel_shader_program,"LtSpecular");
  ce->dp_peel_lt_direction_index =
    glGetUniformLocation(ce->dp_peel_shader_program,"LtDirection");
  ce->dp_peel_lt_halfvect_index =
    glGetUniformLocation(ce->dp_peel_shader_program,"LtHalfVector");
  ce->dp_peel_front_ambient_index =
    glGetUniformLocation(ce->dp_peel_shader_program,"MatFrontAmbient");
  ce->dp_peel_back_ambient_index =
    glGetUniformLocation(ce->dp_peel_shader_program,"MatBackAmbient");
  ce->dp_peel_front_diffuse_index =
    glGetUniformLocation(ce->dp_peel_shader_program,"MatFrontDiffuse");
  ce->dp_peel_back_diffuse_index =
    glGetUniformLocation(ce->dp_peel_shader_program,"MatBackDiffuse");
  ce->dp_peel_specular_index =
    glGetUniformLocation(ce->dp_peel_shader_program,"MatSpecular");
  ce->dp_peel_shininess_index =
    glGetUniformLocation(ce->dp_peel_shader_program,"MatShininess");
  ce->dp_peel_bool_textures_index =
    glGetUniformLocation(ce->dp_peel_shader_program,"BoolTextures");
  ce->dp_peel_tex_scale_index =
    glGetUniformLocation(ce->dp_peel_shader_program,"TexScale");
  ce->dp_peel_tex_samp_f_index =
    glGetUniformLocation(ce->dp_peel_shader_program,"TexSamplerFront");
  ce->dp_peel_tex_samp_b_index =
    glGetUniformLocation(ce->dp_peel_shader_program,"TexSamplerBack");
  ce->dp_peel_tex_samp_w_index =
    glGetUniformLocation(ce->dp_peel_shader_program,"TexSamplerWater");
  ce->dp_peel_tex_samp_depth_blend =
    glGetUniformLocation(ce->dp_peel_shader_program,"TexSamplerDepthBlender");
  ce->dp_peel_tex_samp_front_blend =
    glGetUniformLocation(ce->dp_peel_shader_program,"TexSamplerFrontBlender");
  if (ce->dp_peel_pos_index == -1 ||
      ce->dp_peel_normal_index == -1 ||
      ce->dp_peel_colorf_index == -1 ||
      ce->dp_peel_colorb_index == -1 ||
      ce->dp_peel_mv_index == -1 ||
      ce->dp_peel_proj_index == -1 ||
      ce->dp_peel_glbl_ambient_index == -1 ||
      ce->dp_peel_lt_ambient_index == -1 ||
      ce->dp_peel_lt_diffuse_index == -1 ||
      ce->dp_peel_lt_specular_index == -1 ||
      ce->dp_peel_lt_direction_index == -1 ||
      ce->dp_peel_lt_halfvect_index == -1 ||
      ce->dp_peel_front_ambient_index == -1 ||
      ce->dp_peel_back_ambient_index == -1 ||
      ce->dp_peel_front_diffuse_index == -1 ||
      ce->dp_peel_back_diffuse_index == -1 ||
      ce->dp_peel_specular_index == -1 ||
      ce->dp_peel_shininess_index == -1 ||
      ce->dp_peel_bool_textures_index == -1 ||
      ce->dp_peel_tex_scale_index == -1 ||
      ce->dp_peel_tex_samp_f_index == -1 ||
      ce->dp_peel_tex_samp_b_index == -1 ||
      ce->dp_peel_tex_samp_w_index == -1 ||
      ce->dp_peel_tex_samp_depth_blend == -1 ||
      ce->dp_peel_tex_samp_front_blend == -1)
  {
    glDeleteProgram(ce->poly_shader_program);
    glDeleteProgram(ce->dp_init_shader_program);
    glDeleteProgram(ce->dp_peel_shader_program);
    return;
  }

  if (!glsl_CompileAndLinkShaders(3,dp_blnd_vertex_shader_source,
                                  4,dp_blnd_fragment_shader_source,
                                  &ce->dp_blnd_shader_program))
  {
    glDeleteProgram(ce->poly_shader_program);
    glDeleteProgram(ce->dp_init_shader_program);
    glDeleteProgram(ce->dp_peel_shader_program);
    return;
  }

  ce->dp_blnd_pos_index =
    glGetAttribLocation(ce->dp_blnd_shader_program,"VertexPosition");
  ce->dp_blnd_tex_scale_index =
    glGetUniformLocation(ce->dp_blnd_shader_program,"TexScale");
  ce->dp_blnd_tex_samp_temp =
    glGetUniformLocation(ce->dp_blnd_shader_program,"TexSamplerTemp");
  if (ce->dp_blnd_pos_index == -1 ||
      ce->dp_blnd_tex_scale_index == -1 ||
      ce->dp_blnd_tex_samp_temp == -1)
  {
    glDeleteProgram(ce->poly_shader_program);
    glDeleteProgram(ce->dp_init_shader_program);
    glDeleteProgram(ce->dp_peel_shader_program);
    glDeleteProgram(ce->dp_blnd_shader_program);
    return;
  }

  if (!glsl_CompileAndLinkShaders(3,dp_finl_vertex_shader_source,
                                  4,dp_finl_fragment_shader_source,
                                  &ce->dp_finl_shader_program))
  {
    glDeleteProgram(ce->poly_shader_program);
    glDeleteProgram(ce->dp_init_shader_program);
    glDeleteProgram(ce->dp_peel_shader_program);
    glDeleteProgram(ce->dp_blnd_shader_program);
    return;
  }

  ce->dp_finl_pos_index =
    glGetAttribLocation(ce->dp_finl_shader_program,"VertexPosition");
  ce->dp_finl_tex_scale_index =
    glGetUniformLocation(ce->dp_finl_shader_program,"TexScale");
  ce->dp_finl_tex_samp_front =
    glGetUniformLocation(ce->dp_finl_shader_program,"TexSamplerFrontBlender");
  ce->dp_finl_tex_samp_back =
    glGetUniformLocation(ce->dp_finl_shader_program,"TexSamplerBackBlender");
  ce->dp_finl_tex_samp_opaque =
    glGetUniformLocation(ce->dp_finl_shader_program,"TexSamplerOpaqueBlender");
  if (ce->dp_finl_pos_index == -1 ||
      ce->dp_finl_tex_scale_index == -1 ||
      ce->dp_finl_tex_samp_front == -1 ||
      ce->dp_finl_tex_samp_back == -1 ||
      ce->dp_finl_tex_samp_opaque == -1)
  {
    glDeleteProgram(ce->poly_shader_program);
    glDeleteProgram(ce->dp_init_shader_program);
    glDeleteProgram(ce->dp_peel_shader_program);
    glDeleteProgram(ce->dp_blnd_shader_program);
    glDeleteProgram(ce->dp_finl_shader_program);
    return;
  }

  glGenQueries(1,&ce->dp_query);

  if (!glsl_CompileAndLinkShaders(3,wa_init_vertex_shader_source,
                                  4,wa_init_fragment_shader_source,
                                  &ce->wa_init_shader_program))
    return;

  ce->wa_init_pos_index =
    glGetAttribLocation(ce->wa_init_shader_program,"VertexPosition");
  ce->wa_init_normal_index =
    glGetAttribLocation(ce->wa_init_shader_program,"VertexNormal");
  ce->wa_init_colorf_index =
    glGetAttribLocation(ce->wa_init_shader_program,"VertexColorF");
  ce->wa_init_colorb_index =
    glGetAttribLocation(ce->wa_init_shader_program,"VertexColorB");
  ce->wa_init_vertex_tex_index =
    glGetAttribLocation(ce->wa_init_shader_program,"VertexTexCoord");
  ce->wa_init_mv_index =
    glGetUniformLocation(ce->wa_init_shader_program,"MatModelView");
  ce->wa_init_proj_index =
    glGetUniformLocation(ce->wa_init_shader_program,"MatProj");
  ce->wa_init_glbl_ambient_index =
    glGetUniformLocation(ce->wa_init_shader_program,"LtGlblAmbient");
  ce->wa_init_lt_ambient_index =
    glGetUniformLocation(ce->wa_init_shader_program,"LtAmbient");
  ce->wa_init_lt_diffuse_index =
    glGetUniformLocation(ce->wa_init_shader_program,"LtDiffuse");
  ce->wa_init_lt_specular_index =
    glGetUniformLocation(ce->wa_init_shader_program,"LtSpecular");
  ce->wa_init_lt_direction_index =
    glGetUniformLocation(ce->wa_init_shader_program,"LtDirection");
  ce->wa_init_lt_halfvect_index =
    glGetUniformLocation(ce->wa_init_shader_program,"LtHalfVector");
  ce->wa_init_front_ambient_index =
    glGetUniformLocation(ce->wa_init_shader_program,"MatFrontAmbient");
  ce->wa_init_back_ambient_index =
    glGetUniformLocation(ce->wa_init_shader_program,"MatBackAmbient");
  ce->wa_init_front_diffuse_index =
    glGetUniformLocation(ce->wa_init_shader_program,"MatFrontDiffuse");
  ce->wa_init_back_diffuse_index =
    glGetUniformLocation(ce->wa_init_shader_program,"MatBackDiffuse");
  ce->wa_init_specular_index =
    glGetUniformLocation(ce->wa_init_shader_program,"MatSpecular");
  ce->wa_init_shininess_index =
    glGetUniformLocation(ce->wa_init_shader_program,"MatShininess");
  ce->wa_init_bool_textures_index =
    glGetUniformLocation(ce->wa_init_shader_program,"BoolTextures");
  ce->wa_init_tex_samp_f_index =
    glGetUniformLocation(ce->wa_init_shader_program,"TextureSamplerFront");
  ce->wa_init_tex_samp_b_index =
    glGetUniformLocation(ce->wa_init_shader_program,"TextureSamplerBack");
  ce->wa_init_tex_samp_w_index =
    glGetUniformLocation(ce->wa_init_shader_program,"TextureSamplerWater");
  if (ce->wa_init_pos_index == -1 ||
      ce->wa_init_normal_index == -1 ||
      ce->wa_init_colorf_index == -1 ||
      ce->wa_init_colorb_index == -1 ||
      ce->wa_init_mv_index == -1 ||
      ce->wa_init_proj_index == -1 ||
      ce->wa_init_glbl_ambient_index == -1 ||
      ce->wa_init_lt_ambient_index == -1 ||
      ce->wa_init_lt_diffuse_index == -1 ||
      ce->wa_init_lt_specular_index == -1 ||
      ce->wa_init_lt_direction_index == -1 ||
      ce->wa_init_lt_halfvect_index == -1 ||
      ce->wa_init_front_ambient_index == -1 ||
      ce->wa_init_back_ambient_index == -1 ||
      ce->wa_init_front_diffuse_index == -1 ||
      ce->wa_init_back_diffuse_index == -1 ||
      ce->wa_init_specular_index == -1 ||
      ce->wa_init_shininess_index == -1 ||
      ce->wa_init_bool_textures_index == -1 ||
      ce->wa_init_tex_samp_f_index == -1 ||
      ce->wa_init_tex_samp_b_index == -1 ||
      ce->wa_init_tex_samp_w_index == -1)
  {
    glDeleteProgram(ce->poly_shader_program);
    glDeleteProgram(ce->dp_init_shader_program);
    glDeleteProgram(ce->dp_peel_shader_program);
    glDeleteProgram(ce->dp_blnd_shader_program);
    glDeleteProgram(ce->dp_finl_shader_program);
    glDeleteProgram(ce->wa_init_shader_program);
    return;
  }

  if (!glsl_CompileAndLinkShaders(3,wa_finl_vertex_shader_source,
                                  4,wa_finl_fragment_shader_source,
                                  &ce->wa_finl_shader_program))
  {
    glDeleteProgram(ce->poly_shader_program);
    glDeleteProgram(ce->dp_init_shader_program);
    glDeleteProgram(ce->dp_peel_shader_program);
    glDeleteProgram(ce->dp_blnd_shader_program);
    glDeleteProgram(ce->dp_finl_shader_program);
    glDeleteProgram(ce->wa_init_shader_program);
    glDeleteProgram(ce->wa_finl_shader_program);
    return;
  }

  ce->wa_finl_pos_index =
    glGetAttribLocation(ce->wa_finl_shader_program,"VertexPosition");
  ce->wa_finl_tex_scale_index =
    glGetUniformLocation(ce->wa_finl_shader_program,"TexScale");
  ce->wa_finl_tex_samp_color =
    glGetUniformLocation(ce->wa_finl_shader_program,"TexSamplerColorBlender");
  ce->wa_finl_tex_samp_num =
    glGetUniformLocation(ce->wa_finl_shader_program,"TexSamplerNumBlender");
  ce->wa_finl_tex_samp_opaque =
    glGetUniformLocation(ce->wa_finl_shader_program,"TexSamplerOpaqueBlender");
  if (ce->wa_finl_pos_index == -1 ||
      ce->wa_finl_tex_scale_index == -1 ||
      ce->wa_finl_tex_samp_color == -1 ||
      ce->wa_finl_tex_samp_num == -1 ||
      ce->wa_finl_tex_samp_opaque == -1)
  {
    glDeleteProgram(ce->poly_shader_program);
    glDeleteProgram(ce->dp_init_shader_program);
    glDeleteProgram(ce->dp_peel_shader_program);
    glDeleteProgram(ce->dp_blnd_shader_program);
    glDeleteProgram(ce->dp_finl_shader_program);
    glDeleteProgram(ce->wa_init_shader_program);
    glDeleteProgram(ce->wa_finl_shader_program);
    return;
  }

  glGenBuffers(1,&ce->quad_buffer);
  glBindBuffer(GL_ARRAY_BUFFER,ce->quad_buffer);
  glBufferData(GL_ARRAY_BUFFER,sizeof(quad_verts),quad_verts,GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER,0);

  ce->use_shaders = True;
  if (!gl_gles3 && gl_major < 3)
  {
    /* On OpenGL 2.1 systems, such as the native macOS OpenGL version used
       by XScreenSaver, we need the GL_ARB_framebuffer_object extension for
       the more sophisticated transparency modes.  This extension also
       provides the glGenerateMipmap function. */
    gl_ext = (const char *)glGetString(GL_EXTENSIONS);
    if (gl_ext != NULL)
    {
      if (strstr(gl_ext,"GL_ARB_framebuffer_object") != NULL)
        ce->use_mipmaps = True;
      else
        ce->transparency = TRANSPARENCY_STANDARD;
    }
  }
  else if ((!gl_gles3 && gl_major >= 3) || gl_gles3)
  {
    ce->use_mipmaps = True;
  }
#if defined(HAVE_IPHONE)
  /* OpenGL ES 3.0, which is the highest OpenGL ES version supported on
     iOS and iPadOS, does not support the more sophisticated transparency
     modes.  Switch to standard transparency mode. */
  ce->transparency = TRANSPARENCY_STANDARD;
#endif
#if defined(__APPLE__) && !defined(HAVE_COCOA)
  /* The X11 OpenGL 2.1 version on macOS does not support the more
     sophisticated transparency modes.  Switch to standard transparency
     mode. */
  ce->transparency = TRANSPARENCY_STANDARD;
#endif

  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,(GLint *)&ce->dp_draw_fbo);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,(GLint *)&ce->dp_read_fbo);

  init_dual_peeling_render_targets(mi);
  init_weighted_average_render_targets(mi);

#ifndef HAVE_JWZGLES
  if (ce->transparency != TRANSPARENCY_STANDARD)
    glDisable(GL_MULTISAMPLE);
#endif
}

#endif /* HAVE_GLSL */


static void init(ModeInfo *mi)
{
  cubocteversionstruct *ce = &cubocteversion[MI_SCREEN(mi)];

  if (deform_speed == 0.0f)
    deform_speed = 20.0f;

  ce->alpha = frand(120.0)-60.0;
  ce->beta = frand(120.0)-60.0;
  ce->delta = frand(360.0);

  ce->anim_state = ANIM_DEFORM;
  ce->time = TIME_MIN;
  ce->defdir = 1;

  ce->swipe_step = 0;
  ce->swipe_y = 0.0f;

  ce->offset3d[0] = 0.0f;
  ce->offset3d[1] = 0.0f;
  ce->offset3d[2] = -10.0f;

  ce->fov_angle = frand(MAX_FOV_ANGLE-MIN_FOV_ANGLE)+MIN_FOV_ANGLE;
  ce->fov_ortho = frand(MAX_FOV_ORTHO-MIN_FOV_ORTHO)+MIN_FOV_ORTHO;
  ce->opacity = frand(MAX_OPACITY-MIN_OPACITY)+MIN_OPACITY;

#ifdef HAVE_GLSL
  init_glsl(mi);
  gen_textures(mi);
  setup_tex_coords(mi);
#endif /* HAVE_GLSL */

  setup_colors(mi);
}


static void display_cubocteversion(ModeInfo *mi)
{
  cubocteversionstruct *ce = &cubocteversion[MI_SCREEN(mi)];
  float t;

  if (!ce->button_pressed)
  {
    if (ce->anim_state == ANIM_DEFORM)
    {
      ce->time += ce->defdir*deform_speed*0.005f;
      if (ce->time < TIME_MIN)
      {
        ce->time = TIME_MIN;
        ce->defdir = -ce->defdir;
        ce->anim_state = ANIM_SWIPE_D;
      }
      if (ce->time > TIME_MAX)
      {
        ce->time = TIME_MAX;
        ce->defdir = -ce->defdir;
        ce->anim_state = ANIM_SWIPE_D;
      }
      if (ce->anim_state == ANIM_SWIPE_D)
      {
        ce->swipe_step = 0;
        if (ce->projection == DISP_PERSPECTIVE)
          ce->swipe_y = 0.1f*ce->fov_angle+2.5f;
        else /* ce->projection == DISP_ORTHOGRAPHIC */
          ce->swipe_y = ce->fov_ortho+2.5f;
      }
    }
    else if (ce->anim_state == ANIM_SWIPE_D)
    {
      ce->swipe_step++;
      t = 0.5f*ce->swipe_step/NUM_SWIPE;
      t = 2.0f*ease(t);
      ce->offset3d[1] = -t*ce->swipe_y;
      if (ce->swipe_step > NUM_SWIPE)
        ce->anim_state = ANIM_SWIPE_U;
    }
    else /* ce->anim_state == ANIM_SWIPE_U */
    {
      if (ce->swipe_step > NUM_SWIPE)
      {
        ce->alpha = frand(120.0)-60.0;
        ce->beta = frand(120.0)-60.0;
        ce->delta = frand(360.0);
        ce->fov_angle = frand(MAX_FOV_ANGLE-MIN_FOV_ANGLE)+MIN_FOV_ANGLE;
        ce->fov_ortho = frand(MAX_FOV_ORTHO-MIN_FOV_ORTHO)+MIN_FOV_ORTHO;
        ce->opacity = frand(MAX_OPACITY-MIN_OPACITY)+MIN_OPACITY;
        if (ce->random_eversion_method)
          ce->eversion_method = random() % NUM_EVERSIONS;
        if (ce->random_display_mode)
          ce->display_mode = random() % NUM_DISPLAY_MODES;
        if (ce->random_edge_tubes)
          ce->edge_tubes = random() & 1;
        if (ce->random_self_tubes)
          ce->self_tubes = random() & 1;
        if (ce->random_colors)
        {
          if (ce->use_shaders)
            ce->colors = random() % NUM_COLORS;
          else
            ce->colors = random() % (NUM_COLORS-1);
        }
        if (ce->random_projection)
          ce->projection = random() % NUM_DISP_MODES;
        if (ce->projection == DISP_PERSPECTIVE)
          ce->swipe_y = 0.1f*ce->fov_angle+3.0f;
        else /* ce->projection == DISP_ORTHOGRAPHIC */
          ce->swipe_y = ce->fov_ortho+3.0f;
        setup_colors(mi);
      }
      ce->swipe_step--;
      t = 0.5f*ce->swipe_step/NUM_SWIPE;
      t = 2.0f*ease(t);
      ce->offset3d[1] = t*ce->swipe_y;
      if (ce->swipe_step < 0)
        ce->anim_state = ANIM_DEFORM;
    }

    ce->alpha += speed_x*ce->speed_scale;
    if (ce->alpha >= 360.0f)
      ce->alpha -= 360.0f;
    ce->beta += speed_y*ce->speed_scale;
    if (ce->beta >= 360.0f)
      ce->beta -= 360.0f;
    ce->delta += speed_z*ce->speed_scale;
    if (ce->delta >= 360.0f)
      ce->delta -= 360.0f;
  }

  gltrackball_rotate(ce->trackball);
#ifdef HAVE_GLSL
  if (ce->use_shaders)
    mi->polygon_count = cuboctahedron_eversion_pf(mi);
  else
#endif /* HAVE_GLSL */
    mi->polygon_count = cuboctahedron_eversion_ff(mi);
}


/*
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 *    Xlock hooks.
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 */


ENTRYPOINT void reshape_cubocteversion(ModeInfo *mi, int width, int height)
{
  cubocteversionstruct *ce = &cubocteversion[MI_SCREEN(mi)];
  int y = 0;

  if (width > height*5)
  {
    /* tiny window: show middle */
    height = width;
    y = -height/2;
  }

  ce->WindW = (GLint)width;
  ce->WindH = (GLint)height;
  glViewport(0,y,width,height);
  ce->aspect = (GLfloat)width/(GLfloat)height;
#ifdef HAVE_GLSL
  ce->tex_scale[0] = 1.0f/(GLfloat)width;
  ce->tex_scale[1] = 1.0f/(GLfloat)height;
  delete_dual_peeling_render_targets(mi);
  init_dual_peeling_render_targets(mi);
  delete_weighted_average_render_targets(mi);
  init_weighted_average_render_targets(mi);
#endif /* HAVE_GLSL */
}


ENTRYPOINT Bool cubocteversion_handle_event(ModeInfo *mi, XEvent *event)
{
  cubocteversionstruct *ce = &cubocteversion[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress && event->xbutton.button == Button1)
  {
    ce->button_pressed = True;
    gltrackball_start(ce->trackball,event->xbutton.x,event->xbutton.y,
                      MI_WIDTH(mi),MI_HEIGHT(mi));
    return True;
  }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
  {
    ce->button_pressed = False;
    gltrackball_stop(ce->trackball);
    return True;
  }
  else if (event->xany.type == MotionNotify && ce->button_pressed)
  {
    gltrackball_track(ce->trackball,event->xmotion.x,event->xmotion.y,
                      MI_WIDTH(mi),MI_HEIGHT(mi));
    return True;
  }

  return False;
}


/*
 *-----------------------------------------------------------------------------
 *    Initialize cubocteversion.  Called each time the window changes.
 *-----------------------------------------------------------------------------
 */

ENTRYPOINT void init_cubocteversion(ModeInfo *mi)
{
  cubocteversionstruct *ce;

  MI_INIT(mi,cubocteversion);
  ce = &cubocteversion[MI_SCREEN(mi)];

  ce->trackball = gltrackball_init(False);
  ce->button_pressed = False;

  /* Set the eversion method. */
  if (!strcasecmp(eversion_method,"random"))
  {
    ce->eversion_method = random() % NUM_EVERSIONS;
    ce->random_eversion_method = True;
  }
  else if (!strcasecmp(eversion_method,"morin-denner"))
  {
    ce->eversion_method = EVERSION_MORIN_DENNER;
    ce->random_eversion_method = False;
  }
  else if (!strcasecmp(eversion_method,"apery"))
  {
    ce->eversion_method = EVERSION_APERY;
    ce->random_eversion_method = False;
  }
  else
  {
    ce->eversion_method = random() % NUM_EVERSIONS;
    ce->random_eversion_method = True;
  }

  /* Set the display mode. */
  if (!strcasecmp(mode,"surface"))
  {
    ce->display_mode = DISP_SURFACE;
    ce->random_display_mode = False;
  }
  else if (!strcasecmp(mode,"transparent"))
  {
    ce->display_mode = DISP_TRANSPARENT;
    ce->random_display_mode = False;
  }
  else
  {
    ce->display_mode = random() % NUM_DISPLAY_MODES;
    ce->random_display_mode = True;
  }

  /* Set the edge tube mode. */
  if (!strcasecmp(edge_tubes,"on"))
  {
    ce->edge_tubes = True;
    ce->random_edge_tubes = False;
  }
  else if (!strcasecmp(edge_tubes,"off"))
  {
    ce->edge_tubes = False;
    ce->random_edge_tubes = False;
  }
  else
  {
    ce->edge_tubes = random() & 1;
    ce->random_edge_tubes = True;
  }

  /* Set the self-intersection tube mode. */
  if (!strcasecmp(self_tubes,"on"))
  {
    ce->self_tubes = True;
    ce->random_self_tubes = False;
  }
  else if (!strcasecmp(self_tubes,"off"))
  {
    ce->self_tubes = False;
    ce->random_self_tubes = False;
  }
  else
  {
    ce->self_tubes = random() & 1;
    ce->random_self_tubes = True;
  }

  /* Set the color mode. */
  if (!strcasecmp(color_mode,"two-sided"))
  {
    ce->colors = COLORS_TWOSIDED;
    ce->random_colors = False;
  }
  else if (!strcasecmp(color_mode,"face"))
  {
    ce->colors = COLORS_FACE;
    ce->random_colors = False;
  }
  else if (!strcasecmp(color_mode,"earth"))
  {
    ce->colors = COLORS_EARTH;
    ce->random_colors = False;
  }
  else
  {
    ce->colors = random() % NUM_COLORS;
    ce->random_colors = True;
  }
#ifndef HAVE_GLSL
  if (ce->colors == COLORS_EARTH)
    ce->colors = COLORS_FACE;
#endif

  /* Set the 3d projection mode. */
  if (!strcasecmp(proj,"perspective"))
  {
    ce->projection = DISP_PERSPECTIVE;
    ce->random_projection = False;
  }
  else if (!strcasecmp(proj,"orthographic"))
  {
    ce->projection = DISP_ORTHOGRAPHIC;
    ce->random_projection = False;
  }
  else
  {
    ce->projection = random() % NUM_DISP_MODES;
    ce->random_projection = True;
  }

  /* Set the transparency mode. */
  if (!strcasecmp(transparency,"correct"))
  {
    ce->transparency = TRANSPARENCY_CORRECT;
  }
  else if (!strcasecmp(transparency,"approximate"))
  {
    ce->transparency = TRANSPARENCY_APPROXIMATE;
  }
  else if (!strcasecmp(transparency,"standard"))
  {
    ce->transparency = TRANSPARENCY_STANDARD;
  }
  else
  {
    ce->transparency = TRANSPARENCY_CORRECT;
  }

  /* Make multiple screens rotate at slightly different rates. */
  ce->speed_scale = 0.9+frand(0.3);

  if ((ce->glx_context = init_GL(mi)) != NULL)
  {
    reshape_cubocteversion(mi,MI_WIDTH(mi),MI_HEIGHT(mi));
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
ENTRYPOINT void draw_cubocteversion(ModeInfo *mi)
{
  Display *display = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  cubocteversionstruct *ce;

  if (cubocteversion == NULL)
    return;
  ce = &cubocteversion[MI_SCREEN(mi)];

  MI_IS_DRAWN(mi) = True;
  if (!ce->glx_context)
    return;

  glXMakeCurrent(display,window,*ce->glx_context);

  display_cubocteversion(mi);

  if (MI_IS_FPS(mi))
    do_fps(mi);

  glFlush();

  glXSwapBuffers(display,window);
}


#ifndef STANDALONE
ENTRYPOINT void change_cubocteversion(ModeInfo *mi)
{
  cubocteversionstruct *ce = &cubocteversion[MI_SCREEN(mi)];

  if (!ce->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi),MI_WINDOW(mi),*ce->glx_context);

  init(mi);
}
#endif /* !STANDALONE */


/*
 *-----------------------------------------------------------------------------
 *    The display is being taken away from us.  Free up malloc'ed
 *      memory and X resources that we've alloc'ed.
 *-----------------------------------------------------------------------------
 */

ENTRYPOINT void free_cubocteversion(ModeInfo *mi)
{
  cubocteversionstruct *ce = &cubocteversion[MI_SCREEN(mi)];

  if (!ce->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi),MI_WINDOW(mi),*ce->glx_context);

  gltrackball_free(ce->trackball);
#ifdef HAVE_GLSL
  if (ce->use_shaders)
  {
    glDeleteTextures(3,ce->tex_names);
    glUseProgram(0);
    if (ce->poly_shader_program != 0)
      glDeleteProgram(ce->poly_shader_program);
    if (ce->dp_init_shader_program != 0)
      glDeleteProgram(ce->dp_init_shader_program);
    if (ce->dp_peel_shader_program != 0)
      glDeleteProgram(ce->dp_peel_shader_program);
    if (ce->dp_blnd_shader_program != 0)
      glDeleteProgram(ce->dp_blnd_shader_program);
    if (ce->dp_finl_shader_program != 0)
      glDeleteProgram(ce->dp_finl_shader_program);
    if (ce->wa_init_shader_program != 0)
      glDeleteProgram(ce->wa_init_shader_program);
    if (ce->wa_finl_shader_program != 0)
      glDeleteProgram(ce->wa_finl_shader_program);
    glDeleteBuffers(1,&ce->cuboct_pos_buffer);
    glDeleteBuffers(1,&ce->cuboct_normal_buffer);
    glDeleteBuffers(1,&ce->cuboct_colorf_buffer);
    glDeleteBuffers(1,&ce->cuboct_colorb_buffer);
    glDeleteBuffers(1,&ce->cuboct_tex_coord_buffer);
    glDeleteBuffers(1,&ce->tube_vertex_buffer);
    glDeleteBuffers(1,&ce->tube_normal_buffer);
    glDeleteBuffers(1,&ce->tube_colorf_buffer);
    glDeleteBuffers(1,&ce->tube_colorb_buffer);
    glDeleteBuffers(1,&ce->tube_index_buffer);
    glDeleteBuffers(1,&ce->self_vertex_buffer);
    glDeleteBuffers(1,&ce->self_normal_buffer);
    glDeleteBuffers(1,&ce->self_colorf_buffer);
    glDeleteBuffers(1,&ce->self_colorb_buffer);
    glDeleteBuffers(1,&ce->self_index_buffer);
    glDeleteBuffers(1,&ce->quad_buffer);
    glDeleteQueries(1,&ce->dp_query);
    delete_dual_peeling_render_targets(mi);
    delete_weighted_average_render_targets(mi);
  }
#endif /* HAVE_GLSL */
}


XSCREENSAVER_MODULE ("CuboctEversion", cubocteversion)

#endif /* USE_GL */
