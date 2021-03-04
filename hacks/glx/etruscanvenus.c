/* etruscanvenus --- Shows a 3d immersion of a Klein bottle that
   rotates in 3d or on which you can walk and that can deform smoothly
   between the Etruscan Venus surface, the Roman surface, the Boy
   surface surface, and the Ida surface. */

#if 0
static const char sccsid[] = "@(#)etruscanvenus.c  1.1 05/01/20 xlockmore";
#endif

/* Copyright (c) 2019-2021 Carsten Steger <carsten@mirsanmir.org>. */

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
 * C. Steger - 05/01/20: Initial version
 * C. Steger - 20/12/20: Added per-fragment shading
 * C. Steger - 20/12/30: Make the shader code work under macOS and iOS
 */

/*
 * This program shows a 3d immersion of a Klein bottle that smoothly
 * deforms between the Etruscan Venus surface, the Roman surface, the
 * Boy surface, and the Ida surface.  You can walk on the Klein bottle
 * or turn it in 3d.  Topologically, all surfaces are Klein bottles,
 * even the Roman and Boy surfaces, which are doubly covered and
 * therefore appear to be an immersed real projective plane.  The
 * smooth deformation between these surfaces was constructed by George
 * K. Francis.
 *
 * The Klein bottle is a non-orientable surface.  To make this
 * apparent, the two-sided color mode can be used.  Alternatively,
 * orientation markers (curling arrows) can be drawn as a texture map
 * on the surface of the Klein bottle.  While walking on the Klein
 * bottle, you will notice that the orientation of the curling arrows
 * changes (which it must because the Klein bottle is non-orientable).
 * Since all the surfaces except the Ida surface have points where the
 * surface normal is not well defined for some points, walking is only
 * performed on the Ida surface.
 *
 * As mentioned above, the Roman and Boy surfaces are doubly covered
 * and therefore appear to be an immersed real projective plane.
 * Since some of the parameter names are based on this interpretation
 * of the surface, the geometry of the real projective plane will be
 * briefly disussed.  The real projective plane is a model for the
 * projective geometry in 2d space.  One point can be singled out as
 * the origin.  A line can be singled out as the line at infinity,
 * i.e., a line that lies at an infinite distance to the origin.  The
 * line at infinity is topologically a circle.  Points on the line at
 * infinity are also used to model directions in projective geometry.
 * Direction and distance bands refer to this interpretation of the
 * surface.  If direction bands are used, the bands extend from the
 * origin of the projective plane in different directions to the line
 * at infinity and back to the origin.  If distance bands are used,
 * the bands lie at constant distances to the origin.  The same
 * interpretation is used for distance and direction colors.  Although
 * there is no conceptually equivalent geometric interpretation for
 * the two Klein bottle surfaces (the Etruscan Venus and Ida
 * surfaces), the smooth deformation between the surfaces results in a
 * natural extension of these concepts to the Klein bottle surfaces.
 *
 * The immersed surfaces can be projected to the screen either
 * perspectively or orthographically.  When using the walking mode,
 * perspective projection to the screen will be used.
 *
 * There are three display modes for the Klein bottle: mesh
 * (wireframe), solid, or transparent.  Furthermore, the appearance of
 * the surface can be as a solid object or as a set of see-through
 * bands.  The bands can be distance bands or direction bands, as
 * explained above.
 *
 * The colors with with the Klein bottle is drawn can be set to
 * one-sided, two-sided, distance, or direction.  In one-sided mode,
 * the surface is drawn with the same color on both sides of the
 * underlying triangles.  In two-sided mode, the surface is drawn with
 * red on one side of the underlying triangles and green on the other
 * side.  Since the surface actually only has one side, the color
 * jumps from red to green along a line on the surface.  This mode
 * enables you to see that the surface is non-orientable.  In distance
 * mode, the surface is displayed with fully saturated colors that
 * depend on the distance of the points on the projective plane to the
 * origin, as described above.  If the surface is displayed as
 * distance bands, each band will be displayed with a different color.
 * In direction mode, the surface is displayed with fully saturated
 * colors that depend on the angle of the points on the projective
 * plane with respect to the origin (see above for an explanation).
 * If the surface is displayed as direction bands, each band will be
 * displayed with a different color.  The colors used to color the
 * surface can either be static or can be changed dynamically.
 *
 * The rotation speed for each of the three coordinate axes around
 * which the Klein bottle rotates can be chosen.
 *
 * Furthermore, in the walking mode the walking direction in the 2d
 * base square of the surface and the walking speed can be chosen.
 * The walking direction is measured as an angle in degrees in the 2d
 * square that forms the coordinate system of the surface.  A value of
 * 0 or 180 means that the walk is along a circle at a randomly chosen
 * distance from the origin (parallel to a distance band).  A value of
 * 90 or 270 means that the walk is directly along a direction band.
 * Any other value results in a curved path along the surface.  As
 * noted above, walking is performed only on the Ida surface.
 *
 * By default, the immersion of the Klein bottle smoothly deforms
 * between the Etruscan Venus surface, the Roman surface, the Boy
 * surface, and the Ida surface.  It is possible to choose the speed
 * of the deformation.  Furthermore, it is possible to switch the
 * deformation off.  It is also possible to determine the initial
 * deformation of the immersion.  This is mostly useful if the
 * deformation is switched off, in which case it will determine the
 * appearance of the surface.  A value of 0 corresponds to the
 * Etruscan Venus surface, a value of 1000 to the Roman surface, a
 * value of 2000 to the Boy surface, and a value of 3000 to the Ida
 * surface.
 *
 * This program is inspired by George K. Francis's book "A Topological
 * Picturebook", Springer, 1987, by George K. Francis's paper "The
 * Etruscan Venus" in P. Concus, R. Finn, and D. A. Hoffman:
 * "Geometric Analysis and Computer Graphics", Springer, 1991, and by
 * a video entitled "The Etruscan Venus" by Donna J. Cox, George
 * K. Francis, and Raymond L. Idaszak, presented at SIGGRAPH 1989.
 */

#include "curlicue.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DISP_WIREFRAME             0
#define DISP_SURFACE               1
#define DISP_TRANSPARENT           2
#define NUM_DISPLAY_MODES          3

#define APPEARANCE_SOLID           0
#define APPEARANCE_DISTANCE_BANDS  1
#define APPEARANCE_DIRECTION_BANDS 2
#define NUM_APPEARANCES            3

#define COLORS_ONESIDED            0
#define COLORS_TWOSIDED            1
#define COLORS_DISTANCE            2
#define COLORS_DIRECTION           3
#define NUM_COLORS                 4

#define VIEW_WALK                  0
#define VIEW_TURN                  1
#define NUM_VIEW_MODES             2

#define DISP_PERSPECTIVE           0
#define DISP_ORTHOGRAPHIC          1
#define NUM_DISP_MODES             2

#define DEF_DISPLAY_MODE           "random"
#define DEF_APPEARANCE             "random"
#define DEF_COLORS                 "random"
#define DEF_VIEW_MODE              "random"
#define DEF_MARKS                  "False"
#define DEF_CHANGE_COLORS          "True"
#define DEF_DEFORM                 "True"
#define DEF_PROJECTION             "random"
#define DEF_SPEEDX                 "1.1"
#define DEF_SPEEDY                 "1.3"
#define DEF_SPEEDZ                 "1.5"
#define DEF_WALK_DIRECTION         "83.0"
#define DEF_WALK_SPEED             "20.0"
#define DEF_DEFORM_SPEED           "10.0"
#define DEF_INIT_DEFORM            "0.0"


#ifdef STANDALONE
# define DEFAULTS           "*delay:      25000 \n" \
                            "*showFPS:    False \n" \
                            "*prefersGLSL: True \n" \

# define release_etruscanvenus 0
# include "xlockmore.h"         /* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"             /* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL

#include "glsl-utils.h"
#include "gltrackball.h"

#include <float.h>


#ifdef USE_MODULES
ModStruct etruscanvenus_description =
{"etruscanvenus", "init_etruscanvenus", "draw_etruscanvenus",
 NULL, "draw_etruscanvenus", "change_etruscanvenus",
 "free_etruscanvenus", &etruscanvenus_opts, 25000, 1, 1, 1, 1.0, 4, "",
 "Rotate a 3d immersion of a Klein bottle in 3d or walk on it",
 0, NULL};

#endif


static char *mode;
static char *appear;
static char *color_mode;
static char *view_mode;
static Bool marks;
static Bool deform;
static Bool change_colors;
static char *proj;
static float speed_x;
static float speed_y;
static float speed_z;
static float walk_direction;
static float walk_speed;
static float deform_speed;
static float init_deform;


static XrmOptionDescRec opts[] =
{
  {"-mode",                ".displayMode",   XrmoptionSepArg, 0 },
  {"-wireframe",           ".displayMode",   XrmoptionNoArg,  "wireframe" },
  {"-surface",             ".displayMode",   XrmoptionNoArg,  "surface" },
  {"-transparent",         ".displayMode",   XrmoptionNoArg,  "transparent" },
  {"-appearance",          ".appearance",    XrmoptionSepArg, 0 },
  {"-solid",               ".appearance",    XrmoptionNoArg,  "solid" },
  {"-distance-bands",      ".appearance",    XrmoptionNoArg,  "distance-bands" },
  {"-direction-bands",     ".appearance",    XrmoptionNoArg,  "direction-bands" },
  {"-colors",              ".colors",        XrmoptionSepArg, 0 },
  {"-onesided-colors",     ".colors",        XrmoptionNoArg,  "one-sided" },
  {"-twosided-colors",     ".colors",        XrmoptionNoArg,  "two-sided" },
  {"-distance-colors",     ".colors",        XrmoptionNoArg,  "distance" },
  {"-direction-colors",    ".colors",        XrmoptionNoArg,  "direction" },
  {"-change-colors",       ".changeColors",  XrmoptionNoArg,  "on"},
  {"+change-colors",       ".changeColors",  XrmoptionNoArg,  "off"},
  {"-view-mode",           ".viewMode",      XrmoptionSepArg, 0 },
  {"-walk",                ".viewMode",      XrmoptionNoArg,  "walk" },
  {"-turn",                ".viewMode",      XrmoptionNoArg,  "turn" },
  {"-deform",              ".deform",        XrmoptionNoArg,  "on"},
  {"+deform",              ".deform",        XrmoptionNoArg,  "off"},
  {"-orientation-marks",   ".marks",         XrmoptionNoArg,  "on"},
  {"+orientation-marks",   ".marks",         XrmoptionNoArg,  "off"},
  {"-projection",          ".projection",    XrmoptionSepArg, 0 },
  {"-perspective",         ".projection",    XrmoptionNoArg,  "perspective" },
  {"-orthographic",        ".projection",    XrmoptionNoArg,  "orthographic" },
  {"-speed-x",             ".speedx",        XrmoptionSepArg, 0 },
  {"-speed-y",             ".speedy",        XrmoptionSepArg, 0 },
  {"-speed-z",             ".speedz",        XrmoptionSepArg, 0 },
  {"-walk-direction",      ".walkDirection", XrmoptionSepArg, 0 },
  {"-walk-speed",          ".walkSpeed",     XrmoptionSepArg, 0 },
  {"-deformation-speed",   ".deformSpeed",   XrmoptionSepArg, 0 },
  {"-initial-deformation", ".initDeform",    XrmoptionSepArg, 0 },
  {"-etruscan-venus",      ".initDeform",    XrmoptionNoArg,  "0.0" },
  {"-roman",               ".initDeform",    XrmoptionNoArg,  "1000.0" },
  {"-boy",                 ".initDeform",    XrmoptionNoArg,  "2000.0" },
  {"-ida",                 ".initDeform",    XrmoptionNoArg,  "3000.0" },
};

static argtype vars[] =
{
  { &mode,           "displayMode",   "DisplayMode",   DEF_DISPLAY_MODE,   t_String },
  { &appear,         "appearance",    "Appearance",    DEF_APPEARANCE,     t_String },
  { &color_mode,     "colors",        "Colors",        DEF_COLORS,         t_String },
  { &change_colors,  "changeColors",  "ChangeColors",  DEF_CHANGE_COLORS,  t_Bool },
  { &view_mode,      "viewMode",      "ViewMode",      DEF_VIEW_MODE,      t_String },
  { &deform,         "deform",        "Deform",        DEF_DEFORM,         t_Bool },
  { &marks,          "marks",         "Marks",         DEF_MARKS,          t_Bool },
  { &proj,           "projection",    "Projection",    DEF_PROJECTION,     t_String },
  { &speed_x,        "speedx",        "Speedx",        DEF_SPEEDX,         t_Float},
  { &speed_y,        "speedy",        "Speedy",        DEF_SPEEDY,         t_Float},
  { &speed_z,        "speedz",        "Speedz",        DEF_SPEEDZ,         t_Float},
  { &walk_direction, "walkDirection", "WalkDirection", DEF_WALK_DIRECTION, t_Float},
  { &walk_speed,     "walkSpeed",     "WalkSpeed",     DEF_WALK_SPEED,     t_Float},
  { &deform_speed,   "deformSpeed",   "DeformSpeed",   DEF_DEFORM_SPEED,   t_Float},
  { &init_deform,    "initDeform",    "InitDeform",    DEF_INIT_DEFORM,    t_Float },
};

ENTRYPOINT ModeSpecOpt etruscanvenus_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, NULL};


/* Offset by which we walk above the surface */
#define DELTAY  0.01

/* Color change speeds */
#define DRHO    0.7
#define DSIGMA  1.1
#define DTAU    1.7

/* Number of subdivisions of the surface */
#define NUMU 192
#define NUMV 128

/* Number of subdivisions per band */
#define NUMBDIR  8
#define NUMBDIST 4

/* Constants that are used to calculate the approximate center of the
   surface in the z direction. */
#define Z1 (0.8141179221194051)
#define Z2 (0.1359276851926206)
#define Z3 (1.1581097545867050)
#define Z4 (0.7186549129158579)
#define Z5 (2.5393401559381240)

/* Constants that are used to calculate the approximate radius of the
   surface. */
#define R1 (1.308007044714129)
#define R2 (4.005205981405042)
#define R3 (-2.893994600199527)
#define R4 (-1.266709537162707)


typedef struct {
  GLint WindH, WindW;
  GLXContext *glx_context;
  /* Options */
  int display_mode;
  int appearance;
  int colors;
  Bool change_colors;
  int view;
  int projection;
  Bool marks;
  /* 3D rotation angles */
  float alpha, beta, delta;
  /* Color rotation angles */
  float rho, sigma, tau;
  /* Movement parameters */
  float umove, vmove, dumove, dvmove;
  int side;
  /* Deformation parameters */
  float dd;
  int defdir;
  /* The viewing offset in 3d */
  float offset3d[3];
  /* The 3d coordinates of the surface and their normals */
  float *ev;
  float *evn;
  /* The precomputed colors of the surface */
  float *col;
  /* The precomputed texture coordinates of the surface */
  float *tex;
  /* The "curlicue" texture */
  GLuint tex_name;
  /* Aspect ratio of the current window */
  float aspect;
  /* Trackball states */
  trackball_state *trackball;
  Bool button_pressed;
  /* A random factor to modify the rotation speeds */
  float speed_scale;
#ifdef HAVE_GLSL
  GLfloat *uv;
  GLuint *indices;
  Bool use_shaders, buffers_initialized;
  GLuint shader_program;
  GLint vertex_uv_index, vertex_t_index, color_index;
  GLint mat_mv_index, mat_p_index, db_index, dl_index;
  GLint bool_textures_index, draw_lines_index;
  GLint glbl_ambient_index, lt_ambient_index;
  GLint lt_diffuse_index, lt_specular_index;
  GLint lt_direction_index, lt_halfvect_index;
  GLint front_ambient_index, back_ambient_index;
  GLint front_diffuse_index, back_diffuse_index;
  GLint specular_index, shininess_index;
  GLint texture_sampler_index;
  GLuint vertex_uv_buffer, vertex_t_buffer;
  GLuint color_buffer, indices_buffer;
  GLint ni, ne, nt;
#endif /* HAVE_GLSL */
} etruscanvenusstruct;

static etruscanvenusstruct *etruscanvenus = (etruscanvenusstruct *) NULL;


#ifdef HAVE_GLSL

/* The GLSL versions that correspond to different versions of OpenGL. */
static const GLchar *shader_version_2_1 =
  "#version 120\n";
static const GLchar *shader_version_3_0 =
  "#version 130\n";
static const GLchar *shader_version_3_0_es =
  "#version 300 es\n"
  "precision highp float;\n"
  "precision highp int;\n";

/* The vertex shader code is composed of code fragments that depend on
   the OpenGL version and code fragments that are version-independent.
   They are concatenated by glsl_CompileAndLinkShaders in the function
   init_glsl(). */
static const GLchar *vertex_shader_attribs_2_1 =
  "attribute vec3 VertexUV;\n"
  "attribute vec4 VertexT;\n"
  "attribute vec4 VertexColor;\n"
  "\n"
  "varying vec3 Normal;\n"
  "varying vec4 Color;\n"
  "varying vec4 TexCoord;\n"
  "\n";
static const GLchar *vertex_shader_attribs_3_0 =
  "in vec3 VertexUV;\n"
  "in vec4 VertexT;\n"
  "in vec4 VertexColor;\n"
  "\n"
  "out vec3 Normal;\n"
  "out vec4 Color;\n"
  "out vec4 TexCoord;\n"
  "\n";
static const GLchar *vertex_shader_main =
  "uniform mat4 MatModelView;\n"
  "uniform mat4 MatProj;\n"
  "uniform float DB;\n"
  "uniform float DL;\n"
  "uniform bool BoolTextures;\n"
  "\n"
  "void main (void)\n"
  "{\n"
  "  const float EPSILON = 1.19e-6f;\n"
  "  const float M_SQRT2 = 1.41421356237f;\n"
  "  float u = VertexUV.x;\n"
  "  float v = VertexUV.y;\n"
  "  float bosqrt2 = DB/M_SQRT2;\n"
  "  float b2osqrt2 = 2.0f*bosqrt2;\n"
  "  float b3osqrt2 = 3.0f*bosqrt2;\n"
  "  float cu = cos(u);\n"
  "  float su = sin(u);\n"
  "  float c2u = cos(2.0f*u);\n"
  "  float s2u = sin(2.0f*u);\n"
  "  float c3u = cos(3.0f*u);\n"
  "  float s3u = sin(3.0f*u);\n"
  "  float cv = cos(v);\n"
  "  float sv = sin(v);\n"
  "  float c2v = cos(2.0f*v);\n"
  "  float s2v = sin(2.0f*v);\n"
  "  float nom = (1.0f-DL+DL*cv);\n"
  "  float den = (1.0f-bosqrt2*s3u*s2v);\n"
  "  float f = nom/den;\n"
  "  float fx = c2u*cv+cu*sv;\n"
  "  float fy = s2u*cv-su*sv;\n"
  "  float fz = M_SQRT2*cv;\n"
  "  vec3 x = f*vec3(fx,fy,fz);\n"
  "  float nomv = -DL*sv;\n"
  "  float denu = -b3osqrt2*c3u*s2v;\n"
  "  float denv = -b2osqrt2*s3u*c2v;\n"
  "  float den2 = 1.0f/(den*den);\n"
  "  float fu = -nom*denu*den2;\n"
  "  float fv = (den*nomv-nom*denv)*den2;\n"
  "  float fxu = -su*sv-2.0f*s2u*cv;\n"
  "  float fxv = cu*cv-c2u*sv;\n"
  "  float fyu = 2.0f*c2u*cv-cu*sv;\n"
  "  float fyv = -s2u*sv-su*cv;\n"
  "  float fzv = -M_SQRT2*sv;\n"
  "  vec3 xu = vec3(fu*fx+f*fxu,fu*fy+f*fyu,fu*fz);\n"
  "  vec3 xv = vec3(fv*fx+f*fxv,fv*fy+f*fyv,fv*fz+f*fzv);\n"
  "  vec3 n = cross(xu,xv);\n"
  "  float t = length(n);\n"
  "  if (t < EPSILON)\n"
  "  {\n"
  "    u += 0.01f;\n"
  "    v += 0.01f;\n"
  "    cu = cos(u);\n"
  "    su = sin(u);\n"
  "    c2u = cos(2.0f*u);\n"
  "    s2u = sin(2.0f*u);\n"
  "    c3u = cos(3.0f*u);\n"
  "    s3u = sin(3.0f*u);\n"
  "    cv = cos(v);\n"
  "    sv = sin(v);\n"
  "    c2v = cos(2.0f*v);\n"
  "    s2v = sin(2.0f*v);\n"
  "    nom = (1.0f-DL+DL*cv);\n"
  "    den = (1.0f-bosqrt2*s3u*s2v);\n"
  "    f = nom/den;\n"
  "    fx = c2u*cv+cu*sv;\n"
  "    fy = s2u*cv-su*sv;\n"
  "    fz = M_SQRT2*cv;\n"
  "    nomv = -DL*sv;\n"
  "    denu = -b3osqrt2*c3u*s2v;\n"
  "    denv = -b2osqrt2*s3u*c2v;\n"
  "    den2 = 1.0f/(den*den);\n"
  "    fu = -nom*denu*den2;\n"
  "    fv = (den*nomv-nom*denv)*den2;\n"
  "    fxu = -su*sv-2.0f*s2u*cv;\n"
  "    fxv = cu*cv-c2u*sv;\n"
  "    fyu = 2.0f*c2u*cv-cu*sv;\n"
  "    fyv = -s2u*sv-su*cv;\n"
  "    fzv = -M_SQRT2*sv;\n"
  "    xu = vec3(fu*fx+f*fxu,fu*fy+f*fyu,fu*fz);\n"
  "    xv = vec3(fv*fx+f*fxv,fv*fy+f*fyv,fv*fz+f*fzv);\n"
  "  }\n"
  "  vec4 Position = MatModelView*vec4(x,1.0f);\n"
  "  vec4 pu = MatModelView*vec4(xu,0.0f);\n"
  "  vec4 pv = MatModelView*vec4(xv,0.0f);\n"
  "  Normal = normalize(cross(pu.xyz,pv.xyz));\n"
  "  gl_Position = MatProj*Position;\n"
  "  Color = VertexColor;\n"
  "  if (BoolTextures)\n"
  "    TexCoord = VertexT;\n"
  "}\n";

/* The fragment shader code is composed of code fragments that depend on
   the OpenGL version and code fragments that are version-independent.
   They are concatenated by glsl_CompileAndLinkShaders in the function
   init_glsl(). */
static const GLchar *fragment_shader_attribs_2_1 =
  "varying vec3 Normal;\n"
  "varying vec4 Color;\n"
  "varying vec4 TexCoord;\n"
  "\n";
static const GLchar *fragment_shader_attribs_3_0 =
  "in vec3 Normal;\n"
  "in vec4 Color;\n"
  "in vec4 TexCoord;\n"
  "\n"
  "out vec4 FragColor;\n"
  "\n";
static const GLchar *fragment_shader_main =
  "uniform bool DrawLines;\n"
  "uniform vec4 LtGlblAmbient;\n"
  "uniform vec4 LtAmbient, LtDiffuse, LtSpecular;\n"
  "uniform vec3 LtDirection, LtHalfVector;\n"
  "uniform vec4 MatFrontAmbient, MatBackAmbient;\n"
  "uniform vec4 MatFrontDiffuse, MatBackDiffuse;\n"
  "uniform vec4 MatSpecular;\n"
  "uniform float MatShininess;\n"
  "uniform bool BoolTextures;\n"
  "uniform sampler2D TextureSampler;"
  "\n"
  "void main (void)\n"
  "{\n"
  "  vec4 color;\n"
  "  if (DrawLines)\n"
  "  {\n"
  "    color = Color;\n"
  "  }\n"
  "  else\n"
  "  {\n"
  "    vec3 normalDirection;\n"
  "    vec4 ambientColor, diffuseColor, sceneColor;\n"
  "    vec4 ambientLighting, diffuseReflection, specularReflection;\n"
  "    float ndotl, ndoth, pf;\n"
  "    \n"
  "    if (gl_FrontFacing)\n"
  "    {\n"
  "      normalDirection = normalize(Normal);\n"
  "      sceneColor = Color*MatFrontAmbient*LtGlblAmbient;\n"
  "      ambientColor = Color*MatFrontAmbient;\n"
  "      diffuseColor = Color*MatFrontDiffuse;\n"
  "    }\n"
  "    else\n"
  "    {\n"
  "      normalDirection = -normalize(Normal);\n"
  "      sceneColor = Color*MatBackAmbient*LtGlblAmbient;\n"
  "      ambientColor = Color*MatBackAmbient;\n"
  "      diffuseColor = Color*MatBackDiffuse;\n"
  "    }\n"
  "    \n"
  "    ndotl = max(0.0,dot(normalDirection,LtDirection));\n"
  "    ndoth = max(0.0,dot(normalDirection,LtHalfVector));\n"
  "    if (ndotl == 0.0)\n"
  "      pf = 0.0;\n"
  "    else\n"
  "      pf = pow(ndoth,MatShininess);\n"
  "    ambientLighting = ambientColor*LtAmbient;\n"
  "    diffuseReflection = LtDiffuse*diffuseColor*ndotl;\n"
  "    specularReflection = LtSpecular*MatSpecular*pf;\n"
  "    color = sceneColor+ambientLighting+diffuseReflection;\n";
static const GLchar *fragment_shader_out_2_1 =
  "    if (BoolTextures)\n"
  "      color *= texture2D(TextureSampler,TexCoord.st);"
  "    color += specularReflection;\n"
  "  }\n"
  "  gl_FragColor = clamp(color,0.0,1.0);\n"
  "}\n";
static const GLchar *fragment_shader_out_3_0 =
  "    if (BoolTextures)\n"
  "      color *= texture(TextureSampler,TexCoord.st);"
  "    color += specularReflection;\n"
  "  }\n"
  "  FragColor = clamp(color,0.0,1.0);\n"
  "}\n";

#endif /* HAVE_GLSL */


/* Add a rotation around the x-axis to the matrix m. */
static void rotatex(float m[3][3], float phi)
{
  float c, s, u, v;
  int i;

  phi *= M_PI/180.0;
  c = cos(phi);
  s = sin(phi);
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

  phi *= M_PI/180.0;
  c = cos(phi);
  s = sin(phi);
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

  phi *= M_PI/180.0;
  c = cos(phi);
  s = sin(phi);
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


/* Multiply two rotation matrices: o=m*n. */
static void mult_rotmat(float m[3][3], float n[3][3], float o[3][3])
{
  int i, j, k;

  for (i=0; i<3; i++)
  {
    for (j=0; j<3; j++)
    {
      o[i][j] = 0.0;
      for (k=0; k<3; k++)
        o[i][j] += m[i][k]*n[k][j];
    }
  }
}


/* Compute a 3D rotation matrix from a unit quaternion. */
static void quat_to_rotmat(float p[4], float m[3][3])
{
  double al, be, de;
  double r00, r01, r02, r12, r22;

  r00 = 1.0-2.0*(p[1]*p[1]+p[2]*p[2]);
  r01 = 2.0*(p[0]*p[1]+p[2]*p[3]);
  r02 = 2.0*(p[2]*p[0]-p[1]*p[3]);
  r12 = 2.0*(p[1]*p[2]+p[0]*p[3]);
  r22 = 1.0-2.0*(p[1]*p[1]+p[0]*p[0]);

  al = atan2(-r12,r22)*180.0/M_PI;
  be = atan2(r02,sqrt(r00*r00+r01*r01))*180.0/M_PI;
  de = atan2(-r01,r00)*180.0/M_PI;

  rotateall(al,be,de,m);
}


/* Compute a fully saturated and bright color based on an angle and,
   optionally, a color rotation matrix. */
static void color(etruscanvenusstruct *ev, double angle, float mat[3][3],
                  float col[4])
{
  int s;
  double t, ca, sa;
  float m;

  if (!ev->change_colors)
  {
    if (ev->colors == COLORS_ONESIDED || ev->colors == COLORS_TWOSIDED)
      return;

    if (angle >= 0.0)
      angle = fmod(angle,2.0*M_PI);
    else
      angle = fmod(angle,-2.0*M_PI);
    s = floor(angle/(M_PI/3));
    t = angle/(M_PI/3)-s;
    if (s >= 6)
      s = 0;
    switch (s)
    {
      case 0:
        col[0] = 1.0;
        col[1] = t;
        col[2] = 0.0;
        break;
      case 1:
        col[0] = 1.0-t;
        col[1] = 1.0;
        col[2] = 0.0;
        break;
      case 2:
        col[0] = 0.0;
        col[1] = 1.0;
        col[2] = t;
        break;
      case 3:
        col[0] = 0.0;
        col[1] = 1.0-t;
        col[2] = 1.0;
        break;
      case 4:
        col[0] = t;
        col[1] = 0.0;
        col[2] = 1.0;
        break;
      case 5:
        col[0] = 1.0;
        col[1] = 0.0;
        col[2] = 1.0-t;
        break;
    }
  }
  else /* ev->change_colors */
  {
    if (ev->colors == COLORS_ONESIDED || ev->colors == COLORS_TWOSIDED)
    {
      col[0] = mat[0][2];
      col[1] = mat[1][2];
      col[2] = mat[2][2];
    }
    else
    {
      ca = cos(angle);
      sa = sin(angle);
      col[0] = ca*mat[0][0]+sa*mat[0][1];
      col[1] = ca*mat[1][0]+sa*mat[1][1];
      col[2] = ca*mat[2][0]+sa*mat[2][1];
    }
    m = 0.5f/fmaxf(fmaxf(fabsf(col[0]),fabsf(col[1])),fabsf(col[2]));
    col[0] = m*col[0]+0.5f;
    col[1] = m*col[1]+0.5f;
    col[2] = m*col[2]+0.5f;
  }
  if (ev->display_mode == DISP_TRANSPARENT)
    col[3] = 0.7;
  else
    col[3] = 1.0;
}


/* Set up the surface colors and texture. */
static void setup_etruscan_venus_color_texture(ModeInfo *mi, double umin,
                                               double umax, double vmin,
                                               double vmax, int numu, int numv)
{
  int i, j, k;
  double u, v, ur, vr, vc;
  etruscanvenusstruct *ev = &etruscanvenus[MI_SCREEN(mi)];

  ur = umax-umin;
  vr = vmax-vmin;
  for (i=0; i<=numv; i++)
  {
    for (j=0; j<=numu; j++)
    {
      k = i*(numu+1)+j;
      u = ur*j/numu+umin;
      if (ev->appearance == APPEARANCE_DISTANCE_BANDS)
        v = -vr*i/numv+vmin;
      else
        v = vr*i/numv+vmin;
      if (!ev->change_colors)
      {
        if (ev->colors == COLORS_DISTANCE)
        {
          if (ev->appearance == APPEARANCE_DISTANCE_BANDS)
            vc = -4.0*v;
          else
            vc = 4.0*v;
          if (vc >= 4.0*M_PI)
            vc -= 4.0*M_PI;
          if (vc >= 2.0*M_PI)
            vc = 4.0*M_PI-vc;
          color(ev,vc,NULL,&ev->col[4*k]);
        }
        else /* ev->colors == COLORS_DIRECTION */
        {
          color(ev,u,NULL,&ev->col[4*k]);
        }
      }
      ev->tex[2*k+0] = 48*u/(2.0*M_PI);
      if (ev->appearance == APPEARANCE_DISTANCE_BANDS)
        ev->tex[2*k+1] = 64*v/(2.0*M_PI)-0.5;
      else
        ev->tex[2*k+1] = 64*v/(2.0*M_PI);
    }
  }
}


/* Compute the current walk frame, i.e., the coordinate system of the
   point and direction at which the viewer is currently walking on the
   surface. */
static void compute_walk_frame(etruscanvenusstruct *ev, float db,
                               float dl, float radius, float oz,
                               float mat[3][3])
{
  float p[3], pu[3], pv[3], pm[3], n[3], b[3];
  int l, m;
  float u, v;
  float xx[3], xxu[3], xxv[3];
  float r, t;
  float cv, sv, c2v, s2v, cu, su, c2u, s2u, c3u, s3u;
  float bosqrt2, b2osqrt2, b3osqrt2, nom, den, nomv, denu, denv, den2;
  float f, fx, fy, fz, x, y, z;
  float fu, fv, fxu, fxv, fyu, fyv, fzv, xu, xv, yu, yv, zu, zv;

  u = ev->umove;
  v = ev->vmove;
  u = 0.5f*u;
  bosqrt2 = db/(float)M_SQRT2;
  b2osqrt2 = 2.0f*bosqrt2;
  b3osqrt2 = 3.0f*bosqrt2;
  cu = cosf(u);
  su = sinf(u);
  c2u = cosf(2.0f*u);
  s2u = sinf(2.0f*u);
  c3u = cosf(3.0f*u);
  s3u = sinf(3.0f*u);
  cv = cosf(v);
  sv = sinf(v);
  c2v = cosf(2.0f*v);
  s2v = sinf(2.0f*v);
  nom = (1.0f-dl+dl*cv);
  den = (1.0f-bosqrt2*s3u*s2v);
  f = nom/den;
  fx = c2u*cv+cu*sv;
  fy = s2u*cv-su*sv;
  fz = (float)M_SQRT2*cv;
  x = f*fx;
  y = f*fy;
  z = f*fz;
  nomv = -dl*sv;
  denu = -b3osqrt2*c3u*s2v;
  denv = -b2osqrt2*s3u*c2v;
  den2 = 1.0f/(den*den);
  fu = -nom*denu*den2;
  fv = (den*nomv-nom*denv)*den2;
  fxu = -su*sv-2.0f*s2u*cv;
  fxv = cu*cv-c2u*sv;
  fyu = 2.0f*c2u*cv-cu*sv;
  fyv = -s2u*sv-su*cv;
  fzv = -(float)M_SQRT2*sv;
  xu = fu*fx+f*fxu;
  xv = fv*fx+f*fxv;
  yu = fu*fy+f*fyu;
  yv = fv*fy+f*fyv;
  zu = fu*fz;
  zv = fv*fz+f*fzv;
  xx[0] = x;
  xx[1] = y;
  xx[2] = z-oz;
  n[0] = yu*zv-zu*yv;
  n[1] = zu*xv-xu*zv;
  n[2] = xu*yv-yu*xv;
  t = n[0]*n[0]+n[1]*n[1]+n[2]*n[2];
  /* Avoid degenerate tangential plane basis vectors as much as possible. */
  if (t < 10.0f*FLT_EPSILON)
  {
    u += 0.01f;
    v += 0.01f;
    cu = cosf(u);
    su = sinf(u);
    c2u = cosf(2.0f*u);
    s2u = sinf(2.0f*u);
    c3u = cosf(3.0f*u);
    s3u = sinf(3.0f*u);
    cv = cosf(v);
    sv = sinf(v);
    c2v = cosf(2.0f*v);
    s2v = sinf(2.0f*v);
    nom = (1.0f-dl+dl*cv);
    den = (1.0f-bosqrt2*s3u*s2v);
    f = nom/den;
    fx = c2u*cv+cu*sv;
    fy = s2u*cv-su*sv;
    fz = (float)M_SQRT2*cv;
    nomv = -dl*sv;
    denu = -b3osqrt2*c3u*s2v;
    denv = -b2osqrt2*s3u*c2v;
    den2 = 1.0f/(den*den);
    fu = -nom*denu*den2;
    fv = (den*nomv-nom*denv)*den2;
    fxu = -su*sv-2.0f*s2u*cv;
    fxv = cu*cv-c2u*sv;
    fyu = 2.0f*c2u*cv-cu*sv;
    fyv = -s2u*sv-su*cv;
    fzv = -(float)M_SQRT2*sv;
    xu = fu*fx+f*fxu;
    xv = fv*fx+f*fxv;
    yu = fu*fy+f*fyu;
    yv = fv*fy+f*fyv;
    zu = fu*fz;
    zv = fv*fz+f*fzv;
  }
  xxu[0] = xu;
  xxu[1] = yu;
  xxu[2] = zu;
  xxv[0] = xv;
  xxv[1] = yv;
  xxv[2] = zv;
  for (l=0; l<3; l++)
  {
    p[l] = xx[l]*radius;
    pu[l] = xxu[l]*radius;
    pv[l] = xxv[l]*radius;
  }
  n[0] = pu[1]*pv[2]-pu[2]*pv[1];
  n[1] = pu[2]*pv[0]-pu[0]*pv[2];
  n[2] = pu[0]*pv[1]-pu[1]*pv[0];
  t = 1.0f/(ev->side*4.0f*sqrtf(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]));
  n[0] *= t;
  n[1] *= t;
  n[2] *= t;
  pm[0] = 0.5f*pu[0]*ev->dumove+pv[0]*ev->dvmove;
  pm[1] = 0.5f*pu[1]*ev->dumove+pv[1]*ev->dvmove;
  pm[2] = 0.5f*pu[2]*ev->dumove+pv[2]*ev->dvmove;
  t = 1.0f/(4.0f*sqrtf(pm[0]*pm[0]+pm[1]*pm[1]+pm[2]*pm[2]));
  pm[0] *= t;
  pm[1] *= t;
  pm[2] *= t;
  b[0] = n[1]*pm[2]-n[2]*pm[1];
  b[1] = n[2]*pm[0]-n[0]*pm[2];
  b[2] = n[0]*pm[1]-n[1]*pm[0];
  t = 1.0f/(4.0f*sqrtf(b[0]*b[0]+b[1]*b[1]+b[2]*b[2]));
  b[0] *= t;
  b[1] *= t;
  b[2] *= t;

  /* Compute alpha, beta, gamma from the three basis vectors.
         |  -b[0]  -b[1]  -b[2] |
     m = |   n[0]   n[1]   n[2] |
         | -pm[0] -pm[1] -pm[2] |
  */
  ev->alpha = atan2f(-n[2],-pm[2])*180.0f/(float)M_PI;
  ev->beta = atan2f(-b[2],sqrtf(b[0]*b[0]+b[1]*b[1]))*180.0f/(float)M_PI;
  ev->delta = atan2f(b[1],-b[0])*180.0f/(float)M_PI;

  /* Compute the rotation that rotates the surface in 3D. */
  rotateall(ev->alpha,ev->beta,ev->delta,mat);

  u = ev->umove;
  v = ev->vmove;
  u = 0.5f*u;
  bosqrt2 = db/(float)M_SQRT2;
  b2osqrt2 = 2.0f*bosqrt2;
  b3osqrt2 = 3.0f*bosqrt2;
  cu = cosf(u);
  su = sinf(u);
  c2u = cosf(2.0f*u);
  s2u = sinf(2.0f*u);
  s3u = sinf(3.0f*u);
  cv = cosf(v);
  sv = sinf(v);
  s2v = sinf(2.0f*v);
  nom = (1.0f-dl+dl*cv);
  den = (1.0f-bosqrt2*s3u*s2v);
  f = nom/den;
  fx = c2u*cv+cu*sv;
  fy = s2u*cv-su*sv;
  fz = (float)M_SQRT2*cv;
  x = f*fx;
  y = f*fy;
  z = f*fz;
  xx[0] = x;
  xx[1] = y;
  xx[2] = z-oz;
  for (l=0; l<3; l++)
  {
    r = 0.0f;
    for (m=0; m<3; m++)
      r += mat[l][m]*xx[m];
    p[l] = r*radius;
  }

  ev->offset3d[0] = -p[0];
  ev->offset3d[1] = -p[1]-DELTAY;
  ev->offset3d[2] = -p[2];
}


/* Draw a 3d immersion of the surface using OpenGL's fixed functionality. */
static int etruscan_venus_ff(ModeInfo *mi, double umin, double umax,
                             double vmin, double vmax, int numu, int numv)
{
  static const GLfloat light_ambient[]          = { 0.0, 0.0, 0.0, 1.0 };
  static const GLfloat light_diffuse[]          = { 1.0, 1.0, 1.0, 1.0 };
  static const GLfloat light_specular[]         = { 1.0, 1.0, 1.0, 1.0 };
  static const GLfloat light_position[]         = { 1.0, 1.0, 1.0, 0.0 };
  static const GLfloat mat_specular[]           = { 1.0, 1.0, 1.0, 1.0 };
  static const GLfloat mat_diff_red[]           = { 1.0, 0.0, 0.0, 1.0 };
  static const GLfloat mat_diff_green[]         = { 0.0, 1.0, 0.0, 1.0 };
  static const GLfloat mat_diff_oneside[]       = { 0.9, 0.4, 0.3, 1.0 };
  static const GLfloat mat_diff_trans_red[]     = { 1.0, 0.0, 0.0, 0.7 };
  static const GLfloat mat_diff_trans_green[]   = { 0.0, 1.0, 0.0, 0.7 };
  static const GLfloat mat_diff_trans_oneside[] = { 0.9, 0.4, 0.3, 0.7 };
  float mat_diff_dyn[4], mat_diff_dyn_compl[4];
  float p[3], pu[3], pv[3], n[3], mat[3][3], matc[3][3];
  int i, j, k, l, m, o;
  float u, v, ur, vr, oz, vc;
  float xx[3], xxu[3], xxv[3];
  float r, s, t;
  float dd, bb, ll, db, dl, radius;
  float cv, sv, c2v, s2v, cu, su, c2u, s2u, c3u, s3u;
  float bosqrt2, b2osqrt2, b3osqrt2, nom, den, nomv, denu, denv, den2;
  float f, fx, fy, fz, x, y, z;
  float fu, fv, fxu, fxv, fyu, fyv, fzv, xu, xv, yu, yv, zu, zv;
  float qu[4], r1[3][3], r2[3][3];
  etruscanvenusstruct *ev = &etruscanvenus[MI_SCREEN(mi)];
  int polys;

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  if (ev->projection == DISP_PERSPECTIVE || ev->view == VIEW_WALK)
  {
    if (ev->view == VIEW_WALK)
      gluPerspective(60.0,ev->aspect,0.01,10.0);
    else
      gluPerspective(60.0,ev->aspect,0.1,10.0);
  }
  else
  {
    if (ev->aspect >= 1.0)
      glOrtho(-ev->aspect,ev->aspect,-1.0,1.0,0.1,10.0);
    else
      glOrtho(-1.0,1.0,-1.0/ev->aspect,1.0/ev->aspect,0.1,10.0);
  }
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  if (ev->display_mode == DISP_SURFACE)
  {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glShadeModel(GL_SMOOTH);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0,GL_AMBIENT,light_ambient);
    glLightfv(GL_LIGHT0,GL_DIFFUSE,light_diffuse);
    glLightfv(GL_LIGHT0,GL_SPECULAR,light_specular);
    glLightfv(GL_LIGHT0,GL_POSITION,light_position);
    glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,mat_specular);
    glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,50.0);
    glDisable(GL_BLEND);
  }
  else if (ev->display_mode == DISP_TRANSPARENT)
  {
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glShadeModel(GL_SMOOTH);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0,GL_AMBIENT,light_ambient);
    glLightfv(GL_LIGHT0,GL_DIFFUSE,light_diffuse);
    glLightfv(GL_LIGHT0,GL_SPECULAR,light_specular);
    glLightfv(GL_LIGHT0,GL_POSITION,light_position);
    glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,mat_specular);
    glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,50.0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE);
  }
  else  /* ev->display_mode == DISP_WIREFRAME */
  {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glShadeModel(GL_FLAT);
    glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
    glDisable(GL_BLEND);
  }

  if (ev->marks)
  {
    glEnable(GL_TEXTURE_2D);
#ifndef HAVE_JWZGLES
    glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL,GL_SEPARATE_SPECULAR_COLOR);
#endif
  }
  else
  {
    glDisable(GL_TEXTURE_2D);
#ifndef HAVE_JWZGLES
    glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL,GL_SINGLE_COLOR);
#endif
  }

  dd = ev->dd;
  if (dd < 1.0f)
  {
    bb = 0.0f;
    ll = dd;
  }
  else if (dd < 2.0f)
  {
    bb = dd-1.0f;
    ll = 1.0;
  }
  else if (dd < 3.0f)
  {
    bb = 1.0f;
    ll = 3.0f-dd;
  }
  else /* dd < 4.0f */
  {
    bb = 4.0f-dd;
    ll = 0.0f;
  }
  db = ((6.0f*bb-15.0f)*bb+10.0f)*bb*bb*bb;
  dl = ((6.0f*ll-15.0f)*ll+10.0f)*ll*ll*ll;
  /* Calculate the approximate center of the surface in the z direction. */
  oz = (Z1*(sinf(0.5f*M_PI*powf(dl,Z3))+Z2*sinf(1.5f*M_PI*powf(dl,Z3)))*
        expf(Z4*powf(db,Z5)));
  /* Calculate the approximate radius of the surface. */
  r = R1+(db-0.5f)*(dl-0.5f)+R2*expf(R3*(1.0f-db))*expf(R4*dl);
  radius = 0.8f/r;

  if (ev->change_colors)
    rotateall(ev->rho,ev->sigma,ev->tau,matc);

  if (ev->view == VIEW_WALK)
  {
    /* Compute the walk frame. */
    compute_walk_frame(ev,db,dl,radius,oz,mat);
  }
  else
  {
    /* Compute the rotation that rotates the surface in 3D, including the
       trackball rotations. */
    rotateall(ev->alpha,ev->beta,ev->delta,r1);

    gltrackball_get_quaternion(ev->trackball,qu);
    quat_to_rotmat(qu,r2);

    mult_rotmat(r2,r1,mat);
  }

  if (!ev->change_colors)
  {
    if (ev->colors == COLORS_ONESIDED)
    {
      glColor3fv(mat_diff_oneside);
      if (ev->display_mode == DISP_TRANSPARENT)
      {
        glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,
                     mat_diff_trans_oneside);
      }
      else
      {
        glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,
                     mat_diff_oneside);
      }
    }
    else if (ev->colors == COLORS_TWOSIDED)
    {
      glColor3fv(mat_diff_red);
      if (ev->display_mode == DISP_TRANSPARENT)
      {
        glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,mat_diff_trans_red);
        glMaterialfv(GL_BACK,GL_AMBIENT_AND_DIFFUSE,mat_diff_trans_green);
      }
      else
      {
        glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,mat_diff_red);
        glMaterialfv(GL_BACK,GL_AMBIENT_AND_DIFFUSE,mat_diff_green);
      }
    }
  }
  else /* ev->change_colors */
  {
    color(ev,0.0,matc,mat_diff_dyn);
    if (ev->colors == COLORS_ONESIDED)
    {
      glColor3fv(mat_diff_dyn);
      glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,mat_diff_dyn);
    }
    else if (ev->colors == COLORS_TWOSIDED)
    {
      mat_diff_dyn_compl[0] = 1.0f-mat_diff_dyn[0];
      mat_diff_dyn_compl[1] = 1.0f-mat_diff_dyn[1];
      mat_diff_dyn_compl[2] = 1.0f-mat_diff_dyn[2];
      mat_diff_dyn_compl[3] = mat_diff_dyn[3];
      glColor3fv(mat_diff_dyn);
      glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,mat_diff_dyn);
      glMaterialfv(GL_BACK,GL_AMBIENT_AND_DIFFUSE,mat_diff_dyn_compl);
    }
  }
  glBindTexture(GL_TEXTURE_2D,ev->tex_name);

  ur = umax-umin;
  vr = vmax-vmin;

  /* Set up the surface coordinates and normals. */
  if (ev->appearance == APPEARANCE_DISTANCE_BANDS)
  {
    for (i=0; i<=numv; i++)
    {
      if ((i & (NUMBDIST-1)) >= NUMBDIST/4+1 &&
          (i & (NUMBDIST-1)) < 3*NUMBDIST/4)
        continue;
      for (j=0; j<=numu; j++)
      {
        o = i*(numu+1)+j;
        u = ur*j/numu+umin;
        v = -vr*i/numv+vmin;
        if (ev->change_colors)
        {
          /* Compute the colors dynamically. */
          if (ev->colors == COLORS_DISTANCE)
          {
            vc = -4.0f*v;
            if (vc >= 4.0f*M_PI)
              vc -= 4.0f*M_PI;
            if (vc >= 2.0f*M_PI)
              vc = 4.0f*M_PI-vc;
            color(ev,vc,matc,&ev->col[4*o]);
          }
          else if (ev->colors == COLORS_DIRECTION)
          {
            color(ev,u,matc,&ev->col[4*o]);
          }
        }
        u = 0.5f*u;
        bosqrt2 = db/(float)M_SQRT2;
        b2osqrt2 = 2.0f*bosqrt2;
        b3osqrt2 = 3.0f*bosqrt2;
        cu = cosf(u);
        su = sinf(u);
        c2u = cosf(2.0f*u);
        s2u = sinf(2.0f*u);
        c3u = cosf(3.0f*u);
        s3u = sinf(3.0f*u);
        cv = cosf(v);
        sv = sinf(v);
        c2v = cosf(2.0f*v);
        s2v = sinf(2.0f*v);
        nom = (1.0f-dl+dl*cv);
        den = (1.0f-bosqrt2*s3u*s2v);
        f = nom/den;
        fx = c2u*cv+cu*sv;
        fy = s2u*cv-su*sv;
        fz = (float)M_SQRT2*cv;
        x = f*fx;
        y = f*fy;
        z = f*fz;
        nomv = -dl*sv;
        denu = -b3osqrt2*c3u*s2v;
        denv = -b2osqrt2*s3u*c2v;
        den2 = 1.0f/(den*den);
        fu = -nom*denu*den2;
        fv = (den*nomv-nom*denv)*den2;
        fxu = -su*sv-2.0f*s2u*cv;
        fxv = cu*cv-c2u*sv;
        fyu = 2.0f*c2u*cv-cu*sv;
        fyv = -s2u*sv-su*cv;
        fzv = -(float)M_SQRT2*sv;
        xu = fu*fx+f*fxu;
        xv = fv*fx+f*fxv;
        yu = fu*fy+f*fyu;
        yv = fv*fy+f*fyv;
        zu = fu*fz;
        zv = fv*fz+f*fzv;
        xx[0] = x;
        xx[1] = y;
        xx[2] = z-oz;
        n[0] = yu*zv-zu*yv;
        n[1] = zu*xv-xu*zv;
        n[2] = xu*yv-yu*xv;
        t = n[0]*n[0]+n[1]*n[1]+n[2]*n[2];
        /* Avoid degenerate tangential plane basis vectors as much as
           possible. */
        if (t < 10.0f*FLT_EPSILON)
        {
          u += 0.01f;
          v += 0.01f;
          cu = cosf(u);
          su = sinf(u);
          c2u = cosf(2.0f*u);
          s2u = sinf(2.0f*u);
          c3u = cosf(3.0f*u);
          s3u = sinf(3.0f*u);
          cv = cosf(v);
          sv = sinf(v);
          c2v = cosf(2.0f*v);
          s2v = sinf(2.0f*v);
          nom = (1.0f-dl+dl*cv);
          den = (1.0f-bosqrt2*s3u*s2v);
          f = nom/den;
          fx = c2u*cv+cu*sv;
          fy = s2u*cv-su*sv;
          fz = (float)M_SQRT2*cv;
          nomv = -dl*sv;
          denu = -b3osqrt2*c3u*s2v;
          denv = -b2osqrt2*s3u*c2v;
          den2 = 1.0f/(den*den);
          fu = -nom*denu*den2;
          fv = (den*nomv-nom*denv)*den2;
          fxu = -su*sv-2.0f*s2u*cv;
          fxv = cu*cv-c2u*sv;
          fyu = 2.0f*c2u*cv-cu*sv;
          fyv = -s2u*sv-su*cv;
          fzv = -(float)M_SQRT2*sv;
          xu = fu*fx+f*fxu;
          xv = fv*fx+f*fxv;
          yu = fu*fy+f*fyu;
          yv = fv*fy+f*fyv;
          zu = fu*fz;
          zv = fv*fz+f*fzv;
        }
        xxu[0] = xu;
        xxu[1] = yu;
        xxu[2] = zu;
        xxv[0] = xv;
        xxv[1] = yv;
        xxv[2] = zv;
        for (l=0; l<3; l++)
        {
          r = 0.0f;
          s = 0.0f;
          t = 0.0f;
          for (m=0; m<3; m++)
          {
            r += mat[l][m]*xx[m];
            s += mat[l][m]*xxu[m];
            t += mat[l][m]*xxv[m];
          }
          p[l] = r*radius+ev->offset3d[l];
          pu[l] = s*radius;
          pv[l] = t*radius;
        }
        n[0] = pu[1]*pv[2]-pu[2]*pv[1];
        n[1] = pu[2]*pv[0]-pu[0]*pv[2];
        n[2] = pu[0]*pv[1]-pu[1]*pv[0];
        t = 1.0f/sqrtf(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
        n[0] *= t;
        n[1] *= t;
        n[2] *= t;
        ev->ev[3*o+0] = p[0];
        ev->ev[3*o+1] = p[1];
        ev->ev[3*o+2] = p[2];
        ev->evn[3*o+0] = n[0];
        ev->evn[3*o+1] = n[1];
        ev->evn[3*o+2] = n[2];
      }
    }
  }
  else /* ev->appearance != APPEARANCE_DISTANCE_BANDS */
  {
    for (j=0; j<=numu; j++)
    {
      if (ev->appearance == APPEARANCE_DIRECTION_BANDS &&
          ((j & (NUMBDIR-1)) >= NUMBDIR/2+1))
        continue;
      for (i=0; i<=numv; i++)
      {
        o = i*(numu+1)+j;
        u = ur*j/numu+umin;
        v = vr*i/numv+vmin;
        if (ev->change_colors)
        {
          /* Compute the colors dynamically. */
          if (ev->colors == COLORS_DISTANCE)
          {
            vc = 4.0f*v;
            if (vc >= 4.0f*M_PI)
              vc -= 4.0f*M_PI;
            if (vc >= 2.0f*M_PI)
              vc = 4.0f*M_PI-vc;
            color(ev,vc,matc,&ev->col[4*o]);
          }
          else if (ev->colors == COLORS_DIRECTION)
          {
            color(ev,u,matc,&ev->col[4*o]);
          }
        }
        u = 0.5f*u;
        bosqrt2 = db/(float)M_SQRT2;
        b2osqrt2 = 2.0f*bosqrt2;
        b3osqrt2 = 3.0f*bosqrt2;
        cu = cosf(u);
        su = sinf(u);
        c2u = cosf(2.0f*u);
        s2u = sinf(2.0f*u);
        c3u = cosf(3.0f*u);
        s3u = sinf(3.0f*u);
        cv = cosf(v);
        sv = sinf(v);
        c2v = cosf(2.0f*v);
        s2v = sinf(2.0f*v);
        nom = (1.0f-dl+dl*cv);
        den = (1.0f-bosqrt2*s3u*s2v);
        f = nom/den;
        fx = c2u*cv+cu*sv;
        fy = s2u*cv-su*sv;
        fz = (float)M_SQRT2*cv;
        x = f*fx;
        y = f*fy;
        z = f*fz;
        nomv = -dl*sv;
        denu = -b3osqrt2*c3u*s2v;
        denv = -b2osqrt2*s3u*c2v;
        den2 = 1.0f/(den*den);
        fu = -nom*denu*den2;
        fv = (den*nomv-nom*denv)*den2;
        fxu = -su*sv-2.0f*s2u*cv;
        fxv = cu*cv-c2u*sv;
        fyu = 2.0f*c2u*cv-cu*sv;
        fyv = -s2u*sv-su*cv;
        fzv = -(float)M_SQRT2*sv;
        xu = fu*fx+f*fxu;
        xv = fv*fx+f*fxv;
        yu = fu*fy+f*fyu;
        yv = fv*fy+f*fyv;
        zu = fu*fz;
        zv = fv*fz+f*fzv;
        xx[0] = x;
        xx[1] = y;
        xx[2] = z-oz;
        n[0] = yu*zv-zu*yv;
        n[1] = zu*xv-xu*zv;
        n[2] = xu*yv-yu*xv;
        t = n[0]*n[0]+n[1]*n[1]+n[2]*n[2];
        /* Avoid degenerate tangential plane basis vectors as much as
           possible. */
        if (t < 10.0f*FLT_EPSILON)
        {
          u += 0.01f;
          v += 0.01f;
          cu = cosf(u);
          su = sinf(u);
          c2u = cosf(2.0f*u);
          s2u = sinf(2.0f*u);
          c3u = cosf(3.0f*u);
          s3u = sinf(3.0f*u);
          cv = cosf(v);
          sv = sinf(v);
          c2v = cosf(2.0f*v);
          s2v = sinf(2.0f*v);
          nom = (1.0f-dl+dl*cv);
          den = (1.0f-bosqrt2*s3u*s2v);
          f = nom/den;
          fx = c2u*cv+cu*sv;
          fy = s2u*cv-su*sv;
          fz = (float)M_SQRT2*cv;
          nomv = -dl*sv;
          denu = -b3osqrt2*c3u*s2v;
          denv = -b2osqrt2*s3u*c2v;
          den2 = 1.0f/(den*den);
          fu = -nom*denu*den2;
          fv = (den*nomv-nom*denv)*den2;
          fxu = -su*sv-2.0f*s2u*cv;
          fxv = cu*cv-c2u*sv;
          fyu = 2.0f*c2u*cv-cu*sv;
          fyv = -s2u*sv-su*cv;
          fzv = -(float)M_SQRT2*sv;
          xu = fu*fx+f*fxu;
          xv = fv*fx+f*fxv;
          yu = fu*fy+f*fyu;
          yv = fv*fy+f*fyv;
          zu = fu*fz;
          zv = fv*fz+f*fzv;
        }
        xxu[0] = xu;
        xxu[1] = yu;
        xxu[2] = zu;
        xxv[0] = xv;
        xxv[1] = yv;
        xxv[2] = zv;
        for (l=0; l<3; l++)
        {
          r = 0.0f;
          s = 0.0f;
          t = 0.0f;
          for (m=0; m<3; m++)
          {
            r += mat[l][m]*xx[m];
            s += mat[l][m]*xxu[m];
            t += mat[l][m]*xxv[m];
          }
          p[l] = r*radius+ev->offset3d[l];
          pu[l] = s*radius;
          pv[l] = t*radius;
        }
        n[0] = pu[1]*pv[2]-pu[2]*pv[1];
        n[1] = pu[2]*pv[0]-pu[0]*pv[2];
        n[2] = pu[0]*pv[1]-pu[1]*pv[0];
        t = 1.0f/sqrtf(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
        n[0] *= t;
        n[1] *= t;
        n[2] *= t;
        ev->ev[3*o+0] = p[0];
        ev->ev[3*o+1] = p[1];
        ev->ev[3*o+2] = p[2];
        ev->evn[3*o+0] = n[0];
        ev->evn[3*o+1] = n[1];
        ev->evn[3*o+2] = n[2];
      }
    }
  }

  if (ev->appearance == APPEARANCE_DISTANCE_BANDS)
  {
    for (i=0; i<numv; i++)
    {
      if ((i & (NUMBDIST-1)) >= NUMBDIST/4 &&
          (i & (NUMBDIST-1)) < 3*NUMBDIST/4)
        continue;
      if (ev->display_mode == DISP_WIREFRAME)
        glBegin(GL_QUAD_STRIP);
      else
        glBegin(GL_TRIANGLE_STRIP);
      for (j=0; j<=numu; j++)
      {
        for (k=0; k<=1; k++)
        {
          l = i+k;
          m = j;
          o = l*(numu+1)+m;
          glTexCoord2fv(&ev->tex[2*o]);
          if (ev->colors != COLORS_ONESIDED && ev->colors != COLORS_TWOSIDED)
          {
            glColor3fv(&ev->col[4*o]);
            glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,
                         &ev->col[4*o]);
          }
          glNormal3fv(&ev->evn[3*o]);
          glVertex3fv(&ev->ev[3*o]);
        }
      }
      glEnd();
    }
    polys = numv*(numu+1);
  }
  else /* ev->appearance != APPEARANCE_DISTANCE_BANDS */
  {
    for (j=0; j<numu; j++)
    {
      if (ev->appearance == APPEARANCE_DIRECTION_BANDS &&
          ((j & (NUMBDIR-1)) >= NUMBDIR/2))
        continue;
      if (ev->display_mode == DISP_WIREFRAME)
        glBegin(GL_QUAD_STRIP);
      else
        glBegin(GL_TRIANGLE_STRIP);
      for (i=0; i<=numv; i++)
      {
        for (k=0; k<=1; k++)
        {
          l = i;
          m = j+k;
          o = l*(numu+1)+m;
          glTexCoord2fv(&ev->tex[2*o]);
          if (ev->colors != COLORS_ONESIDED && ev->colors != COLORS_TWOSIDED)
          {
            glColor3fv(&ev->col[4*o]);
            glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,
                         &ev->col[4*o]);
          }
          glNormal3fv(&ev->evn[3*o]);
          glVertex3fv(&ev->ev[3*o]);
        }
      }
      glEnd();
    }
    polys = 2*numu*(numv+1);
    if (ev->appearance == APPEARANCE_DIRECTION_BANDS)
      polys /= 2;
  }

  return polys;
}


#ifdef HAVE_GLSL

/* Draw a 3d immersion of the surface using OpenGL's programmable
   functionality. */
static int etruscan_venus_pf(ModeInfo *mi, double umin, double umax,
                             double vmin, double vmax, int numu, int numv)
{
  static const GLfloat light_model_ambient[]    = { 0.2, 0.2, 0.2, 1.0 };
  static const GLfloat light_ambient[]          = { 0.0, 0.0, 0.0, 1.0 };
  static const GLfloat light_diffuse[]          = { 1.0, 1.0, 1.0, 1.0 };
  static const GLfloat light_specular[]         = { 1.0, 1.0, 1.0, 1.0 };
  static const GLfloat light_position[]         = { 1.0, 1.0, 1.0, 0.0 };
  static const GLfloat mat_specular[]           = { 1.0, 1.0, 1.0, 1.0 };
  static const GLfloat mat_diff_red[]           = { 1.0, 0.0, 0.0, 1.0 };
  static const GLfloat mat_diff_green[]         = { 0.0, 1.0, 0.0, 1.0 };
  static const GLfloat mat_diff_oneside[]       = { 0.9, 0.4, 0.3, 1.0 };
  static const GLfloat mat_diff_trans_red[]     = { 1.0, 0.0, 0.0, 0.7 };
  static const GLfloat mat_diff_trans_green[]   = { 0.0, 1.0, 0.0, 0.7 };
  static const GLfloat mat_diff_trans_oneside[] = { 0.9, 0.4, 0.3, 0.7 };
  static const GLfloat mat_diff_white[]         = { 1.0, 1.0, 1.0, 1.0 };
  GLfloat light_direction[3], half_vector[3], len;
  GLfloat p_mat[16], mv_mat[16], rot_mat[16];
  float mat_diff_dyn[4], mat_diff_dyn_compl[4];
  float mat[3][3], matc[3][3];
  int i, j, k, l, m, o;
  float u, v, ur, vr, oz, vc;
  float r;
  float dd, bb, ll, db, dl, radius;
  float qu[4], r1[3][3], r2[3][3];
  GLsizeiptr index_offset;
  etruscanvenusstruct *ev = &etruscanvenus[MI_SCREEN(mi)];
  int polys;

  if (!ev->use_shaders)
    return 0;

  dd = ev->dd;
  if (dd < 1.0f)
  {
    bb = 0.0f;
    ll = dd;
  }
  else if (dd < 2.0f)
  {
    bb = dd-1.0f;
    ll = 1.0;
  }
  else if (dd < 3.0f)
  {
    bb = 1.0f;
    ll = 3.0f-dd;
  }
  else /* dd < 4.0f */
  {
    bb = 4.0f-dd;
    ll = 0.0f;
  }
  db = ((6.0f*bb-15.0f)*bb+10.0f)*bb*bb*bb;
  dl = ((6.0f*ll-15.0f)*ll+10.0f)*ll*ll*ll;
  /* Calculate the approximate center of the surface in the z direction. */
  oz = (Z1*(sinf(0.5f*M_PI*powf(dl,Z3))+Z2*sinf(1.5f*M_PI*powf(dl,Z3)))*
        expf(Z4*powf(db,Z5)));
  /* Calculate the approximate radius of the surface. */
  r = R1+(db-0.5f)*(dl-0.5f)+R2*expf(R3*(1.0f-db))*expf(R4*dl);
  radius = 0.8f/r;

  if (!ev->buffers_initialized)
  {
    /* The u and v values need to be computed once (or each time the value
       of appearance changes, once we support that). */
    ur = umax-umin;
    vr = vmax-vmin;
    for (j=0; j<=numu; j++)
    {
      for (i=0; i<=numv; i++)
      {
        o = i*(numu+1)+j;
        u = 0.5f*ur*j/numu+umin;
        if (ev->appearance == APPEARANCE_DISTANCE_BANDS)
          v = -vr*i/numv+vmin;
        else
          v = vr*i/numv+vmin;
        ev->uv[2*o+0] = u;
        ev->uv[2*o+1] = v;
      }
    }
    glBindBuffer(GL_ARRAY_BUFFER,ev->vertex_uv_buffer);
    glBufferData(GL_ARRAY_BUFFER,2*(numu+1)*(numv+1)*sizeof(GLfloat),
                 ev->uv,GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);

    glBindBuffer(GL_ARRAY_BUFFER,ev->vertex_t_buffer);
    glBufferData(GL_ARRAY_BUFFER,2*(numu+1)*(numv+1)*sizeof(GLfloat),
                 ev->tex,GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);

    if (!ev->change_colors &&
        ev->colors != COLORS_ONESIDED && ev->colors != COLORS_TWOSIDED)
    {
      glBindBuffer(GL_ARRAY_BUFFER,ev->color_buffer);
      glBufferData(GL_ARRAY_BUFFER,4*(numu+1)*(numv+1)*sizeof(GLfloat),
                   ev->col,GL_STATIC_DRAW);
      glBindBuffer(GL_ARRAY_BUFFER,0);
    }

    /* The indices only need to be computed once (or each time the value of
       appearance changes, once we support that). */
    ev->ni = 0;
    ev->ne = 0;
    ev->nt = 0;
    if (ev->display_mode != DISP_WIREFRAME)
    {
      if (ev->appearance == APPEARANCE_DISTANCE_BANDS)
      {
        for (i=0; i<numv; i++)
        {
          if ((i & (NUMBDIST-1)) >= NUMBDIST/4 &&
              (i & (NUMBDIST-1)) < 3*NUMBDIST/4)
            continue;
          for (j=0; j<=numu; j++)
          {
            for (k=0; k<=1; k++)
            {
              l = i+k;
              m = j;
              o = l*(numu+1)+m;
              ev->indices[ev->ni++] = o;
            }
          }
          ev->ne++;
        }
        ev->nt = 2*(numu+1);
      }
      else /* ev->appearance != APPEARANCE_DISTANCE_BANDS */
      {
        for (j=0; j<numu; j++)
        {
          if (ev->appearance == APPEARANCE_DIRECTION_BANDS &&
              ((j & (NUMBDIR-1)) >= NUMBDIR/2))
            continue;
          for (i=0; i<=numv; i++)
          {
            for (k=0; k<=1; k++)
            {
              l = i;
              m = j+k;
              o = l*(numu+1)+m;
              ev->indices[ev->ni++] = o;
            }
          }
          ev->ne++;
        }
        ev->nt = 2*(numv+1);
      }
    }
    else /* ev->display_mode == DISP_WIREFRAME */
    {
      if (ev->appearance == APPEARANCE_DISTANCE_BANDS)
      {
        for (i=0; i<=numv; i++)
        {
          if ((i & (NUMBDIST-1)) > NUMBDIST/4 &&
              (i & (NUMBDIST-1)) < 3*NUMBDIST/4)
            continue;
          if ((i & (NUMBDIST-1)) == NUMBDIST/4)
          {
            for (j=0; j<numu; j++)
            {
              ev->indices[ev->ni++] = i*(numu+1)+j;
              ev->indices[ev->ni++] = i*(numu+1)+j+1;
            }
            continue;
          }
          for (j=0; j<numu; j++)
          {
            ev->indices[ev->ni++] = i*(numu+1)+j;
            ev->indices[ev->ni++] = i*(numu+1)+j+1;
            if (i < numv)
            {
              ev->indices[ev->ni++] = i*(numu+1)+j;
              ev->indices[ev->ni++] = (i+1)*(numu+1)+j;
            }
          }
        }
      }
      else /* ev->appearance != APPEARANCE_DISTANCE_BANDS */
      {
        for (j=0; j<numu; j++)
        {
          if (ev->appearance == APPEARANCE_DIRECTION_BANDS &&
              ((j & (NUMBDIR-1)) > NUMBDIR/2))
            continue;
          if (ev->appearance == APPEARANCE_DIRECTION_BANDS &&
              ((j & (NUMBDIR-1)) == NUMBDIR/2))
          {
            for (i=0; i<numv; i++)
            {
              ev->indices[ev->ni++] = i*(numu+1)+j;
              ev->indices[ev->ni++] = (i+1)*(numu+1)+j;
            }
            continue;
          }
          for (i=0; i<=numv; i++)
          {
            ev->indices[ev->ni++] = i*(numu+1)+j;
            ev->indices[ev->ni++] = i*(numu+1)+j+1;
            if (i < numv)
            {
              ev->indices[ev->ni++] = i*(numu+1)+j;
              ev->indices[ev->ni++] = (i+1)*(numu+1)+j;
            }
          }
        }
      }
      ev->ne = 1;
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ev->indices_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,ev->ni*sizeof(GLuint),
                 ev->indices,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

    ev->buffers_initialized = True;
  }

  if (ev->change_colors)
    rotateall(ev->rho,ev->sigma,ev->tau,matc);

  if (ev->view == VIEW_WALK)
  {
    /* Compute the walk frame. */
    compute_walk_frame(ev,db,dl,radius,oz,mat);
  }
  else
  {
    /* Compute the rotation that rotates the surface in 3D, including the
       trackball rotations. */
    rotateall(ev->alpha,ev->beta,ev->delta,r1);

    gltrackball_get_quaternion(ev->trackball,qu);
    quat_to_rotmat(qu,r2);

    mult_rotmat(r2,r1,mat);
  }

  if (ev->change_colors &&
      (ev->colors == COLORS_DISTANCE || ev->colors == COLORS_DIRECTION))
  {
    ur = umax-umin;
    vr = vmax-vmin;
    for (j=0; j<=numu; j++)
    {
      for (i=0; i<=numv; i++)
      {
        o = i*(numu+1)+j;
        u = ur*j/numu+umin;
        if (ev->appearance == APPEARANCE_DISTANCE_BANDS)
          v = -vr*i/numv+vmin;
        else
          v = vr*i/numv+vmin;
        if (ev->colors == COLORS_DISTANCE)
        {
          if (ev->appearance == APPEARANCE_DISTANCE_BANDS)
            vc = -4.0f*v;
          else
            vc = 4.0f*v;
          if (vc >= 4.0f*M_PI)
            vc -= 4.0f*M_PI;
          if (vc >= 2.0f*M_PI)
            vc = 4.0f*M_PI-vc;
          color(ev,vc,matc,&ev->col[4*o]);
        }
        else if (ev->colors == COLORS_DIRECTION)
        {
          color(ev,u,matc,&ev->col[4*o]);
        }
      }
    }
  }

  glUseProgram(ev->shader_program);

  glUniform1f(ev->db_index,db);
  glUniform1f(ev->dl_index,dl);

  glsl_Identity(p_mat);
  if (ev->projection == DISP_PERSPECTIVE || ev->view == VIEW_WALK)
  {
    if (ev->view == VIEW_WALK)
      glsl_Perspective(p_mat,60.0f,ev->aspect,0.01f,10.0f);
    else
      glsl_Perspective(p_mat,60.0f,ev->aspect,0.1f,10.0f);
  }
  else
  {
    if (ev->aspect >= 1.0)
      glsl_Orthographic(p_mat,-ev->aspect,ev->aspect,-1.0f,1.0f,
                        0.1f,10.0f);
    else
      glsl_Orthographic(p_mat,-1.0f,1.0f,-1.0f/ev->aspect,1.0f/ev->aspect,
                        0.1f,10.0f);
  }
  glUniformMatrix4fv(ev->mat_p_index,1,GL_FALSE,p_mat);
  glsl_Identity(rot_mat);
  for (i=0; i<3; i++)
    for (j=0; j<3; j++)
      rot_mat[GLSL__LINCOOR(i,j,4)] = mat[i][j];
  glsl_Identity(mv_mat);
  glsl_Translate(mv_mat,ev->offset3d[0],ev->offset3d[1],ev->offset3d[2]);
  glsl_Scale(mv_mat,radius,radius,radius);
  glsl_MultMatrix(mv_mat,rot_mat);
  glsl_Translate(mv_mat,0.0f,0.0f,-oz);
  glUniformMatrix4fv(ev->mat_mv_index,1,GL_FALSE,mv_mat);

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

  glUniform4fv(ev->front_ambient_index,1,mat_diff_white);
  glUniform4fv(ev->front_diffuse_index,1,mat_diff_white);
  glUniform4fv(ev->back_ambient_index,1,mat_diff_white);
  glUniform4fv(ev->back_diffuse_index,1,mat_diff_white);
  glVertexAttrib4f(ev->color_index,1.0f,1.0f,1.0f,1.0f);

  if (ev->display_mode == DISP_SURFACE)
  {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glUniform4fv(ev->glbl_ambient_index,1,light_model_ambient);
    glUniform4fv(ev->lt_ambient_index,1,light_ambient);
    glUniform4fv(ev->lt_diffuse_index,1,light_diffuse);
    glUniform4fv(ev->lt_specular_index,1,light_specular);
    glUniform3fv(ev->lt_direction_index,1,light_direction);
    glUniform3fv(ev->lt_halfvect_index,1,half_vector);
    glUniform4fv(ev->specular_index,1,mat_specular);
    glUniform1f(ev->shininess_index,50.0f);
    glUniform1i(ev->draw_lines_index,GL_FALSE);
  }
  else if (ev->display_mode == DISP_TRANSPARENT)
  {
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE);
    glUniform4fv(ev->glbl_ambient_index,1,light_model_ambient);
    glUniform4fv(ev->lt_ambient_index,1,light_ambient);
    glUniform4fv(ev->lt_diffuse_index,1,light_diffuse);
    glUniform4fv(ev->lt_specular_index,1,light_specular);
    glUniform3fv(ev->lt_direction_index,1,light_direction);
    glUniform3fv(ev->lt_halfvect_index,1,half_vector);
    glUniform4fv(ev->specular_index,1,mat_specular);
    glUniform1f(ev->shininess_index,50.0f);
    glUniform1i(ev->draw_lines_index,GL_FALSE);
  }
  else  /* ev->display_mode == DISP_WIREFRAME */
  {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glUniform1i(ev->draw_lines_index,GL_TRUE);
  }

  if (ev->marks)
    glEnable(GL_TEXTURE_2D);
  else
    glDisable(GL_TEXTURE_2D);

  if (!ev->change_colors)
  {
    if (ev->colors == COLORS_ONESIDED)
    {
      if (ev->display_mode == DISP_TRANSPARENT)
      {
        glUniform4fv(ev->front_ambient_index,1,mat_diff_trans_oneside);
        glUniform4fv(ev->front_diffuse_index,1,mat_diff_trans_oneside);
        glUniform4fv(ev->back_ambient_index,1,mat_diff_trans_oneside);
        glUniform4fv(ev->back_diffuse_index,1,mat_diff_trans_oneside);
      }
      else if (ev->display_mode == DISP_SURFACE)
      {
        glUniform4fv(ev->front_ambient_index,1,mat_diff_oneside);
        glUniform4fv(ev->front_diffuse_index,1,mat_diff_oneside);
        glUniform4fv(ev->back_ambient_index,1,mat_diff_oneside);
        glUniform4fv(ev->back_diffuse_index,1,mat_diff_oneside);
      }
      else /* ev->display_mode == DISP_WIREFRAME */
      {
        glVertexAttrib4fv(ev->color_index,mat_diff_oneside);
      }
    }
    else if (ev->colors == COLORS_TWOSIDED)
    {
      if (ev->display_mode == DISP_TRANSPARENT)
      {
        glUniform4fv(ev->front_ambient_index,1,mat_diff_trans_red);
        glUniform4fv(ev->front_diffuse_index,1,mat_diff_trans_red);
        glUniform4fv(ev->back_ambient_index,1,mat_diff_trans_green);
        glUniform4fv(ev->back_diffuse_index,1,mat_diff_trans_green);
      }
      else if (ev->display_mode == DISP_SURFACE)
      {
        glUniform4fv(ev->front_ambient_index,1,mat_diff_red);
        glUniform4fv(ev->front_diffuse_index,1,mat_diff_red);
        glUniform4fv(ev->back_ambient_index,1,mat_diff_green);
        glUniform4fv(ev->back_diffuse_index,1,mat_diff_green);
      }
      else /* ev->display_mode == DISP_WIREFRAME */
      {
        glVertexAttrib4fv(ev->color_index,mat_diff_red);
      }
    }
  }
  else /* ev->change_colors */
  {
    color(ev,0.0,matc,mat_diff_dyn);
    if (ev->colors == COLORS_ONESIDED)
    {
      if (ev->display_mode == DISP_TRANSPARENT ||
          ev->display_mode == DISP_SURFACE)
      {
        glUniform4fv(ev->front_ambient_index,1,mat_diff_dyn);
        glUniform4fv(ev->front_diffuse_index,1,mat_diff_dyn);
        glUniform4fv(ev->back_ambient_index,1,mat_diff_dyn);
        glUniform4fv(ev->back_diffuse_index,1,mat_diff_dyn);
      }
      else /* ev->display_mode == DISP_WIREFRAME */
      {
        glVertexAttrib4fv(ev->color_index,mat_diff_dyn);
      }
    }
    else if (ev->colors == COLORS_TWOSIDED)
    {
      if (ev->display_mode == DISP_TRANSPARENT ||
          ev->display_mode == DISP_SURFACE)
      {
        mat_diff_dyn_compl[0] = 1.0f-mat_diff_dyn[0];
        mat_diff_dyn_compl[1] = 1.0f-mat_diff_dyn[1];
        mat_diff_dyn_compl[2] = 1.0f-mat_diff_dyn[2];
        mat_diff_dyn_compl[3] = mat_diff_dyn[3];
        glUniform4fv(ev->front_ambient_index,1,mat_diff_dyn);
        glUniform4fv(ev->front_diffuse_index,1,mat_diff_dyn);
        glUniform4fv(ev->back_ambient_index,1,mat_diff_dyn_compl);
        glUniform4fv(ev->back_diffuse_index,1,mat_diff_dyn_compl);
      }
      else /* ev->display_mode == DISP_WIREFRAME */
      {
        glVertexAttrib4fv(ev->color_index,mat_diff_dyn);
      }
    }
  }

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D,ev->tex_name);
  glUniform1i(ev->texture_sampler_index,0);
  glUniform1i(ev->bool_textures_index,marks);

  glEnableVertexAttribArray(ev->vertex_uv_index);
  glBindBuffer(GL_ARRAY_BUFFER,ev->vertex_uv_buffer);
  glVertexAttribPointer(ev->vertex_uv_index,2,GL_FLOAT,GL_FALSE,0,0);

  glEnableVertexAttribArray(ev->vertex_t_index);
  glBindBuffer(GL_ARRAY_BUFFER,ev->vertex_t_buffer);
  glVertexAttribPointer(ev->vertex_t_index,2,GL_FLOAT,GL_FALSE,0,0);

  if (ev->colors != COLORS_ONESIDED && ev->colors != COLORS_TWOSIDED)
  {
    glEnableVertexAttribArray(ev->color_index);
    glBindBuffer(GL_ARRAY_BUFFER,ev->color_buffer);
    if (ev->change_colors)
      glBufferData(GL_ARRAY_BUFFER,4*(numu+1)*(numv+1)*sizeof(GLfloat),
                   ev->col,GL_STREAM_DRAW);
    glVertexAttribPointer(ev->color_index,4,GL_FLOAT,GL_FALSE,0,0);
  }

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ev->indices_buffer);

  if (ev->display_mode != DISP_WIREFRAME)
  {
    for (i=0; i<ev->ne; i++)
    {
      index_offset = ev->nt*i*sizeof(GLuint);
      glDrawElements(GL_TRIANGLE_STRIP,ev->nt,GL_UNSIGNED_INT,
                     (const GLvoid *)index_offset);
    }
  }
  else /* ev->display_mode == DISP_WIREFRAME */
  {
    glLineWidth(1.0f);
    index_offset = 0;
    glDrawElements(GL_LINES,ev->ni,GL_UNSIGNED_INT,
                   (const void *)index_offset);
  }

  glDisableVertexAttribArray(ev->vertex_uv_index);
  if (ev->marks)
    glDisableVertexAttribArray(ev->vertex_t_index);
  if (ev->colors != COLORS_ONESIDED && ev->colors != COLORS_TWOSIDED)
    glDisableVertexAttribArray(ev->color_index);
  glBindBuffer(GL_ARRAY_BUFFER,0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

  glUseProgram(0);

  if (ev->appearance == APPEARANCE_DISTANCE_BANDS)
  {
    polys = numv*(numu+1);
  }
  else /* ev->appearance != APPEARANCE_DISTANCE_BANDS */
  {
    polys = 2*numu*(numv+1);
    if (ev->appearance == APPEARANCE_DIRECTION_BANDS)
      polys /= 2;
  }

  return polys;
}

#endif /* HAVE_GLSL */


/* Generate a texture image that shows the orientation reversal. */
static void gen_texture(ModeInfo *mi)
{
  etruscanvenusstruct *ev = &etruscanvenus[MI_SCREEN(mi)];

  glPixelStorei(GL_UNPACK_ALIGNMENT,1);
  glGenTextures(1,&ev->tex_name);
  glBindTexture(GL_TEXTURE_2D,ev->tex_name);
  glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
  glTexImage2D(GL_TEXTURE_2D,0,GL_LUMINANCE,TEX_DIMENSION,TEX_DIMENSION,0,
               GL_LUMINANCE,GL_UNSIGNED_BYTE,texture);
}


#ifdef HAVE_GLSL

static void init_glsl(ModeInfo *mi)
{
  etruscanvenusstruct *ev = &etruscanvenus[MI_SCREEN(mi)];
  GLint gl_major, gl_minor, glsl_major, glsl_minor;
  GLboolean gl_gles3;
  const GLchar *vertex_shader_source[3];
  const GLchar *fragment_shader_source[4];

  ev->uv = calloc(2*(NUMU+1)*(NUMV+1),sizeof(float));
  ev->indices = calloc(4*(NUMU+1)*(NUMV+1),sizeof(float));

  /* Determine whether to use shaders to render the Klein bottle. */
  ev->use_shaders = False;
  ev->buffers_initialized = False;
  ev->shader_program = 0;
  ev->ni = 0;
  ev->ne = 0;
  ev->nt = 0;

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
      vertex_shader_source[0] = shader_version_2_1;
      vertex_shader_source[1] = vertex_shader_attribs_2_1;
      vertex_shader_source[2] = vertex_shader_main;
      fragment_shader_source[0] = shader_version_2_1;
      fragment_shader_source[1] = fragment_shader_attribs_2_1;
      fragment_shader_source[2] = fragment_shader_main;
      fragment_shader_source[3] = fragment_shader_out_2_1;
    }
    else
    {
      /* We have at least OpenGL 3.0 and at least GLSL 1.30. */
      vertex_shader_source[0] = shader_version_3_0;
      vertex_shader_source[1] = vertex_shader_attribs_3_0;
      vertex_shader_source[2] = vertex_shader_main;
      fragment_shader_source[0] = shader_version_3_0;
      fragment_shader_source[1] = fragment_shader_attribs_3_0;
      fragment_shader_source[2] = fragment_shader_main;
      fragment_shader_source[3] = fragment_shader_out_3_0;
    }
  }
  else /* gl_gles3 */
  {
    if (gl_major < 3 || glsl_major < 3)
      return;
    /* We have at least OpenGL ES 3.0 and at least GLSL ES 3.0. */
    vertex_shader_source[0] = shader_version_3_0_es;
    vertex_shader_source[1] = vertex_shader_attribs_3_0;
    vertex_shader_source[2] = vertex_shader_main;
    fragment_shader_source[0] = shader_version_3_0_es;
    fragment_shader_source[1] = fragment_shader_attribs_3_0;
    fragment_shader_source[2] = fragment_shader_main;
    fragment_shader_source[3] = fragment_shader_out_3_0;
  }
  if (!glsl_CompileAndLinkShaders(3,vertex_shader_source,
                                  4,fragment_shader_source,
                                  &ev->shader_program))
    return;
  ev->vertex_uv_index = glGetAttribLocation(ev->shader_program,"VertexUV");
  ev->vertex_t_index = glGetAttribLocation(ev->shader_program,"VertexT");
  ev->color_index = glGetAttribLocation(ev->shader_program,"VertexColor");
  if (ev->vertex_uv_index == -1 || ev->vertex_t_index == -1 ||
      ev->color_index == -1)
  {
    glDeleteProgram(ev->shader_program);
    return;
  }
  ev->mat_mv_index = glGetUniformLocation(ev->shader_program,
                                          "MatModelView");
  ev->mat_p_index = glGetUniformLocation(ev->shader_program,
                                         "MatProj");
  ev->db_index = glGetUniformLocation(ev->shader_program,
                                      "DB");
  ev->dl_index = glGetUniformLocation(ev->shader_program,
                                      "DL");
  ev->bool_textures_index = glGetUniformLocation(ev->shader_program,
                                                 "BoolTextures");
  ev->draw_lines_index = glGetUniformLocation(ev->shader_program,
                                              "DrawLines");
  ev->glbl_ambient_index = glGetUniformLocation(ev->shader_program,
                                                "LtGlblAmbient");
  ev->lt_ambient_index = glGetUniformLocation(ev->shader_program,
                                              "LtAmbient");
  ev->lt_diffuse_index = glGetUniformLocation(ev->shader_program,
                                              "LtDiffuse");
  ev->lt_specular_index = glGetUniformLocation(ev->shader_program,
                                               "LtSpecular");
  ev->lt_direction_index = glGetUniformLocation(ev->shader_program,
                                                "LtDirection");
  ev->lt_halfvect_index = glGetUniformLocation(ev->shader_program,
                                               "LtHalfVector");
  ev->front_ambient_index = glGetUniformLocation(ev->shader_program,
                                                 "MatFrontAmbient");
  ev->back_ambient_index = glGetUniformLocation(ev->shader_program,
                                                "MatBackAmbient");
  ev->front_diffuse_index = glGetUniformLocation(ev->shader_program,
                                                 "MatFrontDiffuse");
  ev->back_diffuse_index = glGetUniformLocation(ev->shader_program,
                                                "MatBackDiffuse");
  ev->specular_index = glGetUniformLocation(ev->shader_program,
                                            "MatSpecular");
  ev->shininess_index = glGetUniformLocation(ev->shader_program,
                                             "MatShininess");
  ev->texture_sampler_index = glGetUniformLocation(ev->shader_program,
                                                   "TextureSampler");
  if (ev->mat_mv_index == -1 || ev->mat_p_index == -1 ||
      ev->db_index == -1 || ev->dl_index == -1 ||
      ev->bool_textures_index == -1 || ev->draw_lines_index == -1 ||
      ev->glbl_ambient_index == -1 || ev->lt_ambient_index == -1 ||
      ev->lt_diffuse_index == -1 || ev->lt_specular_index == -1 ||
      ev->lt_direction_index == -1 || ev->lt_halfvect_index == -1 ||
      ev->front_ambient_index == -1 || ev->back_ambient_index == -1 ||
      ev->front_diffuse_index == -1 || ev->back_diffuse_index == -1 ||
      ev->specular_index == -1 || ev->shininess_index == -1 ||
      ev->texture_sampler_index == -1)
  {
    glDeleteProgram(ev->shader_program);
    return;
  }

  glGenBuffers(1,&ev->vertex_uv_buffer);
  glGenBuffers(1,&ev->vertex_t_buffer);
  glGenBuffers(1,&ev->color_buffer);
  glGenBuffers(1,&ev->indices_buffer);

  ev->use_shaders = True;
}

#endif /* HAVE_GLSL */


static void init(ModeInfo *mi)
{
  etruscanvenusstruct *ev = &etruscanvenus[MI_SCREEN(mi)];

  if (deform_speed == 0.0)
    deform_speed = 10.0;

  if (init_deform < 0.0)
    init_deform = 0.0;
  if (init_deform >= 4000.0)
    init_deform = 0.0;

  if (walk_speed == 0.0)
    walk_speed = 20.0;

  if (ev->view == VIEW_TURN)
  {
    ev->alpha = frand(360.0);
    ev->beta = frand(360.0);
    ev->delta = frand(360.0);
  }
  else
  {
    ev->alpha = 0.0;
    ev->beta = 0.0;
    ev->delta = 0.0;
  }
  ev->umove = frand(2.0*M_PI);
  ev->vmove = frand(2.0*M_PI);
  ev->dumove = 0.0;
  ev->dvmove = 0.0;
  ev->side = 1;

  ev->dd = init_deform*0.001;
  ev->defdir = 1;

  ev->rho = frand(360.0);
  ev->sigma = frand(360.0);
  ev->tau = frand(360.0);

  ev->offset3d[0] = 0.0;
  ev->offset3d[1] = 0.0;
  ev->offset3d[2] = -2.0;

  ev->ev = calloc(3*(NUMU+1)*(NUMV+1),sizeof(float));
  ev->evn = calloc(3*(NUMU+1)*(NUMV+1),sizeof(float));
  ev->col = calloc(4*(NUMU+1)*(NUMV+1),sizeof(float));
  ev->tex = calloc(2*(NUMU+1)*(NUMV+1),sizeof(float));

  gen_texture(mi);
  setup_etruscan_venus_color_texture(mi,0.0,2.0*M_PI,0.0,2.0*M_PI,NUMU,NUMV);

#ifdef HAVE_GLSL
  init_glsl(mi);
#endif /* HAVE_GLSL */

#ifdef HAVE_ANDROID
  /* glPolygonMode(...,GL_LINE) is not supported for an OpenGL ES 1.1
     context. */
  if (!ev->use_shaders && ev->display_mode == DISP_WIREFRAME)
    ev->display_mode = DISP_SURFACE;
#endif /* HAVE_ANDROID */
}


/* Redisplay the Klein bottle. */
static void display_etruscanvenus(ModeInfo *mi)
{
  etruscanvenusstruct *ev = &etruscanvenus[MI_SCREEN(mi)];

  if (!ev->button_pressed)
  {
    if (deform)
    {
      ev->dd += ev->defdir*deform_speed*0.001;
      if (ev->dd < 0.0)
        ev->dd += 4.0;
      if (ev->dd >= 4.0)
        ev->dd -= 4.0;
      /* Randomly change the deformation direction at one of the four
         surface types in 10% of the cases. */
      if (fabs(round(ev->dd)-ev->dd) <= deform_speed*0.0005)
      {
        if (LRAND() % 10 == 0)
          ev->defdir = -ev->defdir;
      }
    }
    if (ev->view == VIEW_TURN)
    {
      ev->alpha += speed_x*ev->speed_scale;
      if (ev->alpha >= 360.0)
        ev->alpha -= 360.0;
      ev->beta += speed_y*ev->speed_scale;
      if (ev->beta >= 360.0)
        ev->beta -= 360.0;
      ev->delta += speed_z*ev->speed_scale;
      if (ev->delta >= 360.0)
        ev->delta -= 360.0;
    }
    if (ev->view == VIEW_WALK)
    {
      ev->dumove = cos(walk_direction*M_PI/180.0)*walk_speed*M_PI/4096.0;
      ev->dvmove = sin(walk_direction*M_PI/180.0)*walk_speed*M_PI/4096.0;
      ev->umove += ev->dumove;
      if (ev->umove >= 2.0*M_PI)
      {
        ev->umove -= 2.0*M_PI;
        ev->vmove = 2.0*M_PI-ev->vmove;
        ev->side = -ev->side;
      }
      if (ev->umove < 0.0)
      {
        ev->umove += 2.0*M_PI;
        ev->vmove = 2.0*M_PI-ev->vmove;
        ev->side = -ev->side;
      }
      ev->vmove += ev->dvmove;
      if (ev->vmove >= 2.0*M_PI)
        ev->vmove -= 2.0*M_PI;
      if (ev->vmove < 0.0)
        ev->vmove += 2.0*M_PI;
    }
    if (ev->change_colors)
    {
      ev->rho += DRHO;
      if (ev->rho >= 360.0)
        ev->rho -= 360.0;
      ev->sigma += DSIGMA;
      if (ev->sigma >= 360.0)
        ev->sigma -= 360.0;
      ev->tau += DTAU;
      if (ev->tau >= 360.0)
        ev->tau -= 360.0;
    }
  }

#ifdef HAVE_GLSL
  if (ev->use_shaders)
    mi->polygon_count = etruscan_venus_pf(mi,0.0,2.0*M_PI,0.0,2.0*M_PI,
                                          NUMU,NUMV);
  else
#endif /* HAVE_GLSL */
    mi->polygon_count = etruscan_venus_ff(mi,0.0,2.0*M_PI,0.0,2.0*M_PI,
                                          NUMU,NUMV);
}


ENTRYPOINT void reshape_etruscanvenus(ModeInfo *mi, int width, int height)
{
  etruscanvenusstruct *ev = &etruscanvenus[MI_SCREEN(mi)];
  int y = 0;

  if (width > height * 5) {   /* tiny window: show middle */
    height = width;
    y = -height/2;
  }

  ev->WindW = (GLint)width;
  ev->WindH = (GLint)height;
  glViewport(0,y,width,height);
  ev->aspect = (GLfloat)width/(GLfloat)height;
}


ENTRYPOINT Bool etruscanvenus_handle_event(ModeInfo *mi, XEvent *event)
{
  etruscanvenusstruct *ev = &etruscanvenus[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress && event->xbutton.button == Button1)
  {
    ev->button_pressed = True;
    gltrackball_start(ev->trackball, event->xbutton.x, event->xbutton.y,
                      MI_WIDTH(mi), MI_HEIGHT(mi));
    return True;
  }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
  {
    ev->button_pressed = False;
    return True;
  }
  else if (event->xany.type == MotionNotify && ev->button_pressed)
  {
    gltrackball_track(ev->trackball, event->xmotion.x, event->xmotion.y,
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
 *    Initialize etruscanvenus.  Called each time the window changes.
 *-----------------------------------------------------------------------------
 */

ENTRYPOINT void init_etruscanvenus(ModeInfo *mi)
{
  etruscanvenusstruct *ev;

  MI_INIT (mi, etruscanvenus);
  ev = &etruscanvenus[MI_SCREEN(mi)];

  ev->trackball = gltrackball_init(True);
  ev->button_pressed = False;

  /* Set the display mode. */
  if (!strcasecmp(mode,"random"))
  {
    ev->display_mode = random() % NUM_DISPLAY_MODES;
  }
  else if (!strcasecmp(mode,"wireframe"))
  {
    ev->display_mode = DISP_WIREFRAME;
  }
  else if (!strcasecmp(mode,"surface"))
  {
    ev->display_mode = DISP_SURFACE;
  }
  else if (!strcasecmp(mode,"transparent"))
  {
    ev->display_mode = DISP_TRANSPARENT;
  }
  else
  {
    ev->display_mode = random() % NUM_DISPLAY_MODES;
  }

  ev->marks = marks;

  /* Orientation marks don't make sense in wireframe mode. */
  if (ev->display_mode == DISP_WIREFRAME)
    ev->marks = False;

  /* Set the appearance. */
  if (!strcasecmp(appear,"random"))
  {
    ev->appearance = random() % NUM_APPEARANCES;
  }
  else if (!strcasecmp(appear,"solid"))
  {
    ev->appearance = APPEARANCE_SOLID;
  }
  else if (!strcasecmp(appear,"distance-bands"))
  {
    ev->appearance = APPEARANCE_DISTANCE_BANDS;
  }
  else if (!strcasecmp(appear,"direction-bands"))
  {
    ev->appearance = APPEARANCE_DIRECTION_BANDS;
  }
  else
  {
    ev->appearance = random() % NUM_APPEARANCES;
  }

  /* Set the color mode. */
  if (!strcasecmp(color_mode,"random"))
  {
    ev->colors = random() % NUM_COLORS;
  }
  else if (!strcasecmp(color_mode,"one-sided"))
  {
    ev->colors = COLORS_ONESIDED;
  }
  else if (!strcasecmp(color_mode,"two-sided"))
  {
    ev->colors = COLORS_TWOSIDED;
  }
  else if (!strcasecmp(color_mode,"distance"))
  {
    ev->colors = COLORS_DISTANCE;
  }
  else if (!strcasecmp(color_mode,"direction"))
  {
    ev->colors = COLORS_DIRECTION;
  }
  else
  {
    ev->colors = random() % NUM_COLORS;
  }

  ev->change_colors = change_colors;

  /* Set the view mode. */
  if (!strcasecmp(view_mode,"random"))
  {
    /* Select the walking mode only in 10% of the cases. */
    if (LRAND() % 10 == 0)
      ev->view = VIEW_WALK;
    else
      ev->view = VIEW_TURN;
  }
  else if (!strcasecmp(view_mode,"walk"))
  {
    ev->view = VIEW_WALK;
  }
  else if (!strcasecmp(view_mode,"turn"))
  {
    ev->view = VIEW_TURN;
  }
  else
  {
    /* Select the walking mode only in 10% of the cases. */
    if (LRAND() % 10 == 0)
      ev->view = VIEW_WALK;
    else
      ev->view = VIEW_TURN;
  }

  if (ev->view == VIEW_WALK)
  {
    /* Walking only works on the Ida surface.  Therefore, set the initial
       deformation to the Ida surface and switch off the deformation. */
    init_deform = 3000.0;
    deform = False;
  }

  /* Set the 3d projection mode. */
  if (!strcasecmp(proj,"random"))
  {
    /* Orthographic projection only makes sense in turn mode. */
    if (ev->view == VIEW_TURN)
      ev->projection = random() % NUM_DISP_MODES;
    else
      ev->projection = DISP_PERSPECTIVE;
  }
  else if (!strcasecmp(proj,"perspective"))
  {
    ev->projection = DISP_PERSPECTIVE;
  }
  else if (!strcasecmp(proj,"orthographic"))
  {
    ev->projection = DISP_ORTHOGRAPHIC;
  }
  else
  {
    /* Orthographic projection only makes sense in turn mode. */
    if (ev->view == VIEW_TURN)
      ev->projection = random() % NUM_DISP_MODES;
    else
      ev->projection = DISP_PERSPECTIVE;
  }

  /* Make multiple screens rotate at slightly different rates. */
  ev->speed_scale = 0.9+frand(0.3);

  if ((ev->glx_context = init_GL(mi)) != NULL)
  {
    reshape_etruscanvenus(mi,MI_WIDTH(mi),MI_HEIGHT(mi));
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
ENTRYPOINT void draw_etruscanvenus(ModeInfo *mi)
{
  Display *display = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  etruscanvenusstruct *ev;

  if (etruscanvenus == NULL)
    return;
  ev = &etruscanvenus[MI_SCREEN(mi)];

  MI_IS_DRAWN(mi) = True;
  if (!ev->glx_context)
    return;

  glXMakeCurrent(display, window, *ev->glx_context);

  glClearColor(0.0f,0.0f,0.0f,1.0f);
  glClearDepth(1.0f);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();

  display_etruscanvenus(mi);

  if (MI_IS_FPS(mi))
    do_fps (mi);

  glFlush();

  glXSwapBuffers(display,window);
}


#ifndef STANDALONE
ENTRYPOINT void change_etruscanvenus(ModeInfo *mi)
{
  etruscanvenusstruct *ev = &etruscanvenus[MI_SCREEN(mi)];

  if (!ev->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *ev->glx_context);
  init(mi);
}
#endif /* !STANDALONE */


ENTRYPOINT void free_etruscanvenus(ModeInfo *mi)
{
  etruscanvenusstruct *ev = &etruscanvenus[MI_SCREEN(mi)];

  if (!ev->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *ev->glx_context);

  if (ev->ev) free(ev->ev);
  if (ev->evn) free(ev->evn);
  if (ev->col) free(ev->col);
  if (ev->tex) free(ev->tex);
  gltrackball_free(ev->trackball);
  if (ev->tex_name) glDeleteTextures(1, &ev->tex_name);
#ifdef HAVE_GLSL
  if (ev->uv) free(ev->uv);
  if (ev->indices) free(ev->indices);
  if (ev->use_shaders)
  {
    glDeleteBuffers(1,&ev->vertex_uv_buffer);
    glDeleteBuffers(1,&ev->vertex_t_buffer);
    glDeleteBuffers(1,&ev->color_buffer);
    glDeleteBuffers(1,&ev->indices_buffer);
    if (ev->shader_program != 0)
    {
      glUseProgram(0);
      glDeleteProgram(ev->shader_program);
    }
  }
#endif /* HAVE_GLSL */
}


XSCREENSAVER_MODULE ("EtruscanVenus", etruscanvenus)

#endif /* USE_GL */
