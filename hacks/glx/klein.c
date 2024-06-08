/* klein --- Shows a Klein bottle that rotates in 4d or on which you
   can walk */

#if 0
static const char sccsid[] = "@(#)klein.c  1.1 04/10/08 xlockmore";
#endif

/* Copyright (c) 2005-2021 Carsten Steger <carsten@mirsanmir.org>. */

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
 * C. Steger - 04/10/08: Initial version
 * C. Steger - 09/08/03: Changes to the parameter handling
 * C. Steger - 13/12/25: Added the squeezed torus Klein bottle
 * C. Steger - 14/10/03: Moved the curlicue texture to curlicue.h
 * C. Steger - 20/01/11: Added the changing colors mode
 * C. Steger - 20/12/12: Added per-fragment shading
 * C. Steger - 20/12/30: Make the shader code work under macOS and iOS
 */

/*
 * This program shows three different Klein bottles in 4d: the
 * figure-8 Klein bottle, the pinched torus Klein bottle, or the
 * Lawson Klein bottle.  You can walk on the Klein bottle, see it turn
 * in 4d, or walk on it while it turns in 4d.  The figure-8 Klein
 * bottle is well known in its 3d form.  The 4d form used in this
 * program is an extension of the 3d form to 4d that does not
 * intersect itself in 4d (which can be seen in the depth colors mode
 * when using static colors).  The pinched torus Klein bottle also
 * does not intersect itself in 4d (which can be seen in the depth
 * colors mode when using static colors).  The Lawson Klein bottle, on
 * the other hand, does intersect itself in 4d.  Its primary use is
 * that it has a nice appearance for walking and for turning in 3d.
 *
 * The Klein bottle is a non-orientable surface.  To make this
 * apparent, the two-sided color mode can be used.  Alternatively,
 * orientation markers (curling arrows) can be drawn as a texture map
 * on the surface of the Klein bottle.  While walking on the Klein
 * bottle, you will notice that the orientation of the curling arrows
 * changes (which it must because the Klein bottle is non-orientable).
 *
 * The program projects the 4d Klein bottle to 3d using either a
 * perspective or an orthographic projection.  Which of the two
 * alternatives looks more appealing depends on the viewing mode and
 * the Klein bottle.  For example, the Lawson Klein bottle looks
 * nicest when projected perspectively.  The figure-8 Klein bottle, on
 * the other hand, looks nicer while walking when projected
 * orthographically from 4d.  For the pinched torus Klein bottle, both
 * projection modes give equally acceptable projections.
 *
 * The projected Klein bottle can then be projected to the screen
 * either perspectively or orthographically.  When using the walking
 * modes, perspective projection to the screen should be used.
 *
 * There are three display modes for the Klein bottle: mesh
 * (wireframe), solid, or transparent.  Furthermore, the appearance of
 * the Klein bottle can be as a solid object or as a set of
 * see-through bands.  Finally, the colors with with the Klein bottle
 * is drawn can be set to one-sided, two-sided, rainbow, or depth.  In
 * one-sided mode, the Klein bottle is drawn with the same color on
 * both "sides."  In two-sided mode (using static colors), the Klein
 * bottle is drawn with red on one "side" and green on the "other
 * side."  Of course, the Klein bottle only has one side, so the color
 * jumps from red to green along a curve on the surface of the Klein
 * bottle.  This mode enables you to see that the Klein bottle is
 * non-orientable.  If changing colors are used in two-sided mode,
 * changing complementary colors are used on the respective "sides."
 * The rainbow color mode (using static colors) draws the Klein bottle
 * with a color wheel of fully saturated rainbow colors.  If changing
 * colors are used, the color wheel's colors change dynamically.  The
 * rainbow color mode gives a very nice effect when combined with the
 * see-through bands mode or with the orientation markers drawn.  The
 * depth color mode draws the Klein bottle with colors that are chosen
 * according to the 4d "depth" of the points.  If static colors are
 * used, this mode enables you to see that the figure-8 and pinched
 * torus Klein bottles do not intersect themselves in 4d, while the
 * Lawson Klein bottle does intersect itself.
 *
 * The rotation speed for each of the six planes around which the
 * Klein bottle rotates can be chosen.  For the walk-and-turn mode,
 * only the rotation speeds around the true 4d planes are used (the
 * xy, xz, and yz planes).
 *
 * Furthermore, in the walking modes the walking direction in the 2d
 * base square of the Klein bottle and the walking speed can be
 * chosen.
 *
 * This program is somewhat inspired by Thomas Banchoff's book "Beyond
 * the Third Dimension: Geometry, Computer Graphics, and Higher
 * Dimensions", Scientific American Library, 1990.
 */

#include "curlicue.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define KLEIN_BOTTLE_FIGURE_8      0
#define KLEIN_BOTTLE_PINCHED_TORUS 1
#define KLEIN_BOTTLE_LAWSON        2
#define NUM_KLEIN_BOTTLES          3

#define DISP_WIREFRAME             0
#define DISP_SURFACE               1
#define DISP_TRANSPARENT           2
#define NUM_DISPLAY_MODES          3

#define APPEARANCE_SOLID           0
#define APPEARANCE_BANDS           1
#define NUM_APPEARANCES            2

#define COLORS_ONESIDED            0
#define COLORS_TWOSIDED            1
#define COLORS_RAINBOW             2
#define COLORS_DEPTH               3
#define NUM_COLORS                 4

#define VIEW_WALK                  0
#define VIEW_TURN                  1
#define VIEW_WALKTURN              2
#define NUM_VIEW_MODES             3

#define DISP_3D_PERSPECTIVE        0
#define DISP_3D_ORTHOGRAPHIC       1
#define NUM_DISP_3D_MODES          2

#define DISP_4D_PERSPECTIVE        0
#define DISP_4D_ORTHOGRAPHIC       1
#define NUM_DISP_4D_MODES          2

#define DEF_KLEIN_BOTTLE           "random"
#define DEF_DISPLAY_MODE           "random"
#define DEF_APPEARANCE             "random"
#define DEF_COLORS                 "random"
#define DEF_VIEW_MODE              "random"
#define DEF_MARKS                  "False"
#define DEF_CHANGE_COLORS          "False"
#define DEF_PROJECTION_3D          "random"
#define DEF_PROJECTION_4D          "random"
#define DEF_SPEEDWX                "1.1"
#define DEF_SPEEDWY                "1.3"
#define DEF_SPEEDWZ                "1.5"
#define DEF_SPEEDXY                "1.7"
#define DEF_SPEEDXZ                "1.9"
#define DEF_SPEEDYZ                "2.1"
#define DEF_WALK_DIRECTION         "7.0"
#define DEF_WALK_SPEED             "20.0"


#ifdef STANDALONE
# define DEFAULTS           "*delay:      25000 \n" \
                            "*showFPS:    False \n" \
                            "*prefersGLSL: True \n" \

# define release_klein 0
# include "xlockmore.h"         /* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"             /* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL

#ifndef HAVE_JWXYZ
# include <X11/keysym.h>
#endif

#include "glsl-utils.h"
#include "gltrackball.h"


#ifdef USE_MODULES
ModStruct   klein_description =
{"klein", "init_klein", "draw_klein", NULL,
 "draw_klein", "change_klein", NULL, &klein_opts,
 25000, 1, 1, 1, 1.0, 4, "",
 "Rotate a Klein bottle in 4d or walk on it", 0, NULL};

#endif


static char *klein_bottle;
static char *mode;
static char *appear;
static char *color_mode;
static char *view_mode;
static Bool marks;
static Bool change_colors;
static char *proj_3d;
static char *proj_4d;
static float speed_wx;
static float speed_wy;
static float speed_wz;
static float speed_xy;
static float speed_xz;
static float speed_yz;
static float walk_direction;
static float walk_speed;


static XrmOptionDescRec opts[] =
{
  {"-klein-bottle",      ".kleinBottle",   XrmoptionSepArg, 0 },
  {"-figure-8",          ".kleinBottle",   XrmoptionNoArg,  "figure-8" },
  {"-pinched-torus",     ".kleinBottle",   XrmoptionNoArg,  "pinched-torus" },
  {"-lawson",            ".kleinBottle",   XrmoptionNoArg,  "lawson" },
  {"-mode",              ".displayMode",   XrmoptionSepArg, 0 },
  {"-wireframe",         ".displayMode",   XrmoptionNoArg,  "wireframe" },
  {"-surface",           ".displayMode",   XrmoptionNoArg,  "surface" },
  {"-transparent",       ".displayMode",   XrmoptionNoArg,  "transparent" },
  {"-appearance",        ".appearance",    XrmoptionSepArg, 0 },
  {"-solid",             ".appearance",    XrmoptionNoArg,  "solid" },
  {"-bands",             ".appearance",    XrmoptionNoArg,  "bands" },
  {"-colors",            ".colors",        XrmoptionSepArg, 0 },
  {"-onesided",          ".colors",        XrmoptionNoArg,  "one-sided" },
  {"-twosided",          ".colors",        XrmoptionNoArg,  "two-sided" },
  {"-rainbow",           ".colors",        XrmoptionNoArg,  "rainbow" },
  {"-depth",             ".colors",        XrmoptionNoArg,  "depth" },
  {"-change-colors",     ".changeColors",  XrmoptionNoArg,  "on"},
  {"+change-colors",     ".changeColors",  XrmoptionNoArg,  "off"},
  {"-view-mode",         ".viewMode",      XrmoptionSepArg, 0 },
  {"-walk",              ".viewMode",      XrmoptionNoArg,  "walk" },
  {"-turn",              ".viewMode",      XrmoptionNoArg,  "turn" },
  {"-walk-turn",         ".viewMode",      XrmoptionNoArg,  "walk-turn" },
  {"-orientation-marks", ".marks",         XrmoptionNoArg,  "on"},
  {"+orientation-marks", ".marks",         XrmoptionNoArg,  "off"},
  {"-projection-3d",     ".projection3d",  XrmoptionSepArg, 0 },
  {"-perspective-3d",    ".projection3d",  XrmoptionNoArg,  "perspective" },
  {"-orthographic-3d",   ".projection3d",  XrmoptionNoArg,  "orthographic" },
  {"-projection-4d",     ".projection4d",  XrmoptionSepArg, 0 },
  {"-perspective-4d",    ".projection4d",  XrmoptionNoArg,  "perspective" },
  {"-orthographic-4d",   ".projection4d",  XrmoptionNoArg,  "orthographic" },
  {"-speed-wx",          ".speedwx",       XrmoptionSepArg, 0 },
  {"-speed-wy",          ".speedwy",       XrmoptionSepArg, 0 },
  {"-speed-wz",          ".speedwz",       XrmoptionSepArg, 0 },
  {"-speed-xy",          ".speedxy",       XrmoptionSepArg, 0 },
  {"-speed-xz",          ".speedxz",       XrmoptionSepArg, 0 },
  {"-speed-yz",          ".speedyz",       XrmoptionSepArg, 0 },
  {"-walk-direction",    ".walkDirection", XrmoptionSepArg, 0 },
  {"-walk-speed",        ".walkSpeed",     XrmoptionSepArg, 0 }
};

static argtype vars[] =
{
  { &klein_bottle,   "kleinBottle",   "KleinBottle",   DEF_KLEIN_BOTTLE,   t_String },
  { &mode,           "displayMode",   "DisplayMode",   DEF_DISPLAY_MODE,   t_String },
  { &appear,         "appearance",    "Appearance",    DEF_APPEARANCE,     t_String },
  { &color_mode,     "colors",        "Colors",        DEF_COLORS,         t_String },
  { &change_colors,  "changeColors",  "ChangeColors",  DEF_CHANGE_COLORS,  t_Bool },
  { &view_mode,      "viewMode",      "ViewMode",      DEF_VIEW_MODE,      t_String },
  { &marks,          "marks",         "Marks",         DEF_MARKS,          t_Bool },
  { &proj_3d,        "projection3d",  "Projection3d",  DEF_PROJECTION_3D,  t_String },
  { &proj_4d,        "projection4d",  "Projection4d",  DEF_PROJECTION_4D,  t_String },
  { &speed_wx,       "speedwx",       "Speedwx",       DEF_SPEEDWX,        t_Float},
  { &speed_wy,       "speedwy",       "Speedwy",       DEF_SPEEDWY,        t_Float},
  { &speed_wz,       "speedwz",       "Speedwz",       DEF_SPEEDWZ,        t_Float},
  { &speed_xy,       "speedxy",       "Speedxy",       DEF_SPEEDXY,        t_Float},
  { &speed_xz,       "speedxz",       "Speedxz",       DEF_SPEEDXZ,        t_Float},
  { &speed_yz,       "speedyz",       "Speedyz",       DEF_SPEEDYZ,        t_Float},
  { &walk_direction, "walkDirection", "WalkDirection", DEF_WALK_DIRECTION, t_Float},
  { &walk_speed,     "walkSpeed",     "WalkSpeed",     DEF_WALK_SPEED,     t_Float}
};

ENTRYPOINT ModeSpecOpt klein_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, NULL};


/* Radius of the figure-8 Klein bottle */
#define FIGURE_8_RADIUS 2.0

/* An increment for the radius of the figure-8 Klein bottle */
#define RADIUS_INCR 1.25

/* Radius of the pinched torus Klein bottle */
#define PINCHED_TORUS_RADIUS 2.0

/* Offset by which we walk above the Klein bottle */
#define DELTAY  0.02

/* Color change speeds */
#define DRHO    0.7
#define DSIGMA  1.1
#define DTAU    1.7

/* Number of subdivisions of the Klein bottle */
#define NUMU 128
#define NUMV 128

/* Number of subdivisions per band */
#define NUMB 8


typedef struct {
  GLint      WindH, WindW;
  GLXContext *glx_context;
  /* Options */
  int bottle_type;
  int display_mode;
  int appearance;
  int colors;
  Bool change_colors;
  int view;
  int projection_3d;
  int projection_4d;
  /* 4D rotation angles */
  float alpha, beta, delta, zeta, eta, theta;
  /* Color rotation angles */
  float rho, sigma, tau;
  /* Movement parameters */
  float umove, vmove, dumove, dvmove;
  int side;
  /* The viewing offset in 4d */
  float offset4d[4];
  /* The viewing offset in 3d */
  float offset3d[4];
  /* The 4d coordinates of the Klein bottle and their derivatives */
  float x[(NUMU+1)*(NUMV+1)][4];
  float xu[(NUMU+1)*(NUMV+1)][4];
  float xv[(NUMU+1)*(NUMV+1)][4];
  float pp[(NUMU+1)*(NUMV+1)][3];
  float pn[(NUMU+1)*(NUMV+1)][3];
  /* The precomputed colors of the Klein bottle */
  float col[(NUMU+1)*(NUMV+1)][4];
  /* The precomputed texture coordinates of the Klein bottle */
  float tex[(NUMU+1)*(NUMV+1)][2];
  /* The "curlicue" texture */
  GLuint tex_name;
  /* Aspect ratio of the current window */
  float aspect;
  /* Trackball states */
  trackball_state *trackballs[2];
  int current_trackball;
  Bool button_pressed;
  /* A random factor to modify the rotation speeds */
  float speed_scale;
#ifdef HAVE_GLSL
  GLfloat uv[(NUMU+1)*(NUMV+1)][2];
  GLuint indices[4*(NUMU+1)*(NUMV+1)];
  Bool use_shaders, buffers_initialized;
  GLuint shader_program;
  GLint vertex_uv_index, vertex_t_index, color_index;
  GLint mat_rot_index, mat_p_index, bool_persp_index;
  GLint off4d_index, off3d_index, bool_textures_index;
  GLint bottle_type_index, draw_lines_index;
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
} kleinstruct;

static kleinstruct *klein = (kleinstruct *) NULL;


#ifdef HAVE_GLSL

#define STRH(x) #x
#define STR(x) STRH(x)

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
  "attribute vec2 VertexUV;\n"
  "attribute vec4 VertexT;\n"
  "attribute vec4 VertexColor;\n"
  "\n"
  "varying vec3 Normal;\n"
  "varying vec4 Color;\n"
  "varying vec4 TexCoord;\n"
  "\n";
static const GLchar *vertex_shader_attribs_3_0 =
  "in vec2 VertexUV;\n"
  "in vec4 VertexT;\n"
  "in vec4 VertexColor;\n"
  "\n"
  "out vec3 Normal;\n"
  "out vec4 Color;\n"
  "out vec4 TexCoord;\n"
  "\n";
static const GLchar *vertex_shader_main =
  "#define KLEIN_BOTTLE_FIGURE_8      "STR(KLEIN_BOTTLE_FIGURE_8)"\n"
  "#define KLEIN_BOTTLE_PINCHED_TORUS "STR(KLEIN_BOTTLE_PINCHED_TORUS)"\n"
  "#define KLEIN_BOTTLE_LAWSON        "STR(KLEIN_BOTTLE_LAWSON)"\n"
  "#define FIGURE_8_RADIUS            "STR(FIGURE_8_RADIUS)"f\n"
  "#define PINCHED_TORUS_RADIUS       "STR(PINCHED_TORUS_RADIUS)"f\n"
  "#define RADIUS_INCR                "STR(RADIUS_INCR)"f\n"
  "\n"
  "uniform mat4 MatRot4D;\n"
  "uniform mat4 MatProj;\n"
  "uniform bool BoolPersp;\n"
  "uniform vec4 Offset4D;\n"
  "uniform vec4 Offset3D;\n"
  "uniform bool BoolTextures;\n"
  "uniform int BottleType;\n"
  "\n"
  "void main (void)\n"
  "{\n"
  "  float u, v;\n"
  "  vec4 x, xu, xv, xx, xxu, xxv;\n"
  "  vec3 p, pu, pv;\n"
  "  u = VertexUV.x;\n"
  "  v = VertexUV.y;\n"
  "  if (BottleType == KLEIN_BOTTLE_FIGURE_8)\n"
  "  {\n"
  "    float su, cu, sv, cv, s2u, c2u, sv2, cv2;\n"
  "    cu = cos(u);\n"
  "    su = sin(u);\n"
  "    cv = cos(v);\n"
  "    sv = sin(v);\n"
  "    cv2 = cos(0.5f*v);\n"
  "    sv2 = sin(0.5f*v);\n"
  "    c2u = cos(2.0f*u);\n"
  "    s2u = sin(2.0f*u);\n"
  "    xx = vec4((su*cv2-s2u*sv2+FIGURE_8_RADIUS)*cv,\n"
  "              (su*cv2-s2u*sv2+FIGURE_8_RADIUS)*sv,\n"
  "               su*sv2+s2u*cv2,\n"
  "               cu)/(FIGURE_8_RADIUS+RADIUS_INCR);\n"
  "    xxu = vec4((cu*cv2-2.0f*c2u*sv2)*cv,\n"
  "               (cu*cv2-2.0f*c2u*sv2)*sv,\n"
  "               cu*sv2+2.0f*c2u*cv2,\n"
  "               -su)/(FIGURE_8_RADIUS+RADIUS_INCR);\n"
  "    xxv = vec4(((-0.5f*su*sv2-0.5f*s2u*cv2)*cv-\n"
  "                (su*cv2-s2u*sv2+FIGURE_8_RADIUS)*sv),\n"
  "               ((-0.5f*su*sv2-0.5f*s2u*cv2)*sv+\n"
  "                (su*cv2-s2u*sv2+FIGURE_8_RADIUS)*cv),\n"
  "               0.5f*su*cv2-0.5f*s2u*sv2,\n"
  "               0.0f)/(FIGURE_8_RADIUS+RADIUS_INCR);\n"
  "  }\n"
  "  else if (BottleType == KLEIN_BOTTLE_PINCHED_TORUS)\n"
  "  {\n"
  "    float cu, su, cv, sv, cv2, sv2;\n"
  "    cu = cos(u);\n"
  "    su = sin(u);\n"
  "    cv = cos(v);\n"
  "    sv = sin(v);\n"
  "    cv2 = cos(0.5f*v);\n"
  "    sv2 = sin(0.5f*v);\n"
  "    xx = vec4((PINCHED_TORUS_RADIUS+cu)*cv,\n"
  "              (PINCHED_TORUS_RADIUS+cu)*sv,\n"
  "              su*cv2,\n"
  "              su*sv2)/(PINCHED_TORUS_RADIUS+RADIUS_INCR)\n;"
  "    xxu = vec4(-su*cv,\n"
  "               -su*sv,\n"
  "               cu*cv2,\n"
  "               cu*sv2)/(PINCHED_TORUS_RADIUS+RADIUS_INCR)\n;"
  "    xxv = vec4(-(PINCHED_TORUS_RADIUS+cu)*sv,\n"
  "               (PINCHED_TORUS_RADIUS+cu)*cv,\n"
  "               -0.5f*su*sv2,\n"
  "               0.5f*su*cv2)/(PINCHED_TORUS_RADIUS+RADIUS_INCR)\n;"
  "  }\n"
  "  else // BottleType == KLEIN_BOTTLE_LAWSON\n"
  "  {\n"
  "    float cu, su, cv, sv, cv2, sv2;\n"
  "    cu = cos(u);\n"
  "    su = sin(u);\n"
  "    cv = cos(v);\n"
  "    sv = sin(v);\n"
  "    cv2 = cos(0.5f*v);\n"
  "    sv2 = sin(0.5f*v);\n"
  "    xx = vec4(cu*cv,\n"
  "              cu*sv,\n"
  "              su*sv2,\n"
  "              su*cv2)\n;"
  "    xxu = vec4(-su*cv,\n"
  "               -su*sv,\n"
  "               cu*sv2,\n"
  "               cu*cv2)\n;"
  "    xxv = vec4(-cu*sv,\n"
  "               cu*cv,\n"
  "               0.5f*su*cv2,\n"
  "               -0.5f*su*sv2)\n;"
  "  }\n"
  "  x = MatRot4D*xx+Offset4D;\n"
  "  xu = MatRot4D*xxu;\n"
  "  xv = MatRot4D*xxv;\n"
  "  if (BoolPersp)\n"
  "  {\n"
  "    vec3 r = x.xyz;\n"
  "    float s = x.w;\n"
  "    float t = s*s;\n"
  "    p = r/s+Offset3D.xyz;\n"
  "    pu = (s*xu.xyz-r*xu.w)/t;\n"
  "    pv = (s*xv.xyz-r*xv.w)/t;\n"
  "  }\n"
  "  else\n"
  "  {\n"
  "    p = x.xyz+Offset3D.xyz;\n"
  "    pu = xu.xyz;\n"
  "    pv = xv.xyz;\n"
  "  }\n"
  "  vec4 Position = vec4(p,1.0);\n"
  "  Normal = normalize(cross(pu,pv));\n"
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
  rotatexy(m,ze);
  rotatexz(m,et);
  rotateyz(m,th);
}


/* Compute the rotation matrix m from the 4d rotation angles. */
static void rotateall4d(float ze, float et, float th, float m[4][4])
{
  int i, j;

  for (i=0; i<4; i++)
    for (j=0; j<4; j++)
      m[i][j] = (i==j);
  rotatexy(m,ze);
  rotatexz(m,et);
  rotateyz(m,th);
}


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


/* Compute the 3d rotation matrix m from the 3d rotation angles. */
static void rotateall3d(float al, float be, float de, float m[3][3])
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


/* Compute a fully saturated and bright color based on an angle. */
static void color(kleinstruct *kb, double angle, float mat[3][3], float col[4])
{
  int s;
  double t, ca, sa;
  float m;

  if (!kb->change_colors)
  {
    if (kb->colors == COLORS_ONESIDED || kb->colors == COLORS_TWOSIDED)
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
  else /* kb->change_colors */
  {
    if (kb->colors == COLORS_ONESIDED || kb->colors == COLORS_TWOSIDED)
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
  if (kb->display_mode == DISP_TRANSPARENT)
    col[3] = 0.7;
  else
    col[3] = 1.0;
}


/* Set up the figure-8 Klein bottle coordinates, colors, and texture. */
static void setup_figure8(ModeInfo *mi, double umin, double umax, double vmin,
                          double vmax)
{
  int i, j, k, l;
  double u, v, ur, vr;
  double cu, su, cv, sv, cv2, sv2, c2u, s2u;
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  ur = umax-umin;
  vr = vmax-vmin;
  for (i=0; i<=NUMU; i++)
  {
    for (j=0; j<=NUMV; j++)
    {
      k = i*(NUMV+1)+j;
      u = -ur*j/NUMU+umin;
      v = vr*i/NUMV+vmin;
      if (!kb->change_colors)
      {
        if (kb->colors == COLORS_DEPTH)
          color(kb,(cos(u)+1.0)*M_PI*2.0/3.0,NULL,kb->col[k]);
        else if (kb->colors == COLORS_RAINBOW)
          color(kb,v,NULL,kb->col[k]);
      }
      kb->tex[k][0] = -32*u/(2.0*M_PI);
      kb->tex[k][1] = 32*v/(2.0*M_PI);
      cu = cos(u);
      su = sin(u);
      cv = cos(v);
      sv = sin(v);
      cv2 = cos(0.5*v);
      sv2 = sin(0.5*v);
      c2u = cos(2.0*u);
      s2u = sin(2.0*u);
      kb->x[k][0] = (su*cv2-s2u*sv2+FIGURE_8_RADIUS)*cv;
      kb->x[k][1] = (su*cv2-s2u*sv2+FIGURE_8_RADIUS)*sv;
      kb->x[k][2] = su*sv2+s2u*cv2;
      kb->x[k][3] = cu;
      kb->xu[k][0] = (cu*cv2-2.0*c2u*sv2)*cv;
      kb->xu[k][1] = (cu*cv2-2.0*c2u*sv2)*sv;
      kb->xu[k][2] = cu*sv2+2.0*c2u*cv2;
      kb->xu[k][3] = -su;
      kb->xv[k][0] = ((-0.5*su*sv2-0.5*s2u*cv2)*cv-
                      (su*cv2-s2u*sv2+FIGURE_8_RADIUS)*sv);
      kb->xv[k][1] = ((-0.5*su*sv2-0.5*s2u*cv2)*sv+
                      (su*cv2-s2u*sv2+FIGURE_8_RADIUS)*cv);
      kb->xv[k][2] = 0.5*su*cv2-0.5*s2u*sv2;
      kb->xv[k][3] = 0.0;
      for (l=0; l<4; l++)
      {
        kb->x[k][l] /= FIGURE_8_RADIUS+RADIUS_INCR;
        kb->xu[k][l] /= FIGURE_8_RADIUS+RADIUS_INCR;
        kb->xv[k][l] /= FIGURE_8_RADIUS+RADIUS_INCR;
      }
    }
  }
}


/* Set up the pinched torus Klein bottle coordinates, colors, and texture. */
static void setup_pinched_torus(ModeInfo *mi, double umin, double umax,
                                double vmin, double vmax)
{
  int i, j, k, l;
  double u, v, ur, vr;
  double cu, su, cv, sv, cv2, sv2;
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  ur = umax-umin;
  vr = vmax-vmin;
  for (i=0; i<=NUMU; i++)
  {
    for (j=0; j<=NUMV; j++)
    {
      k = i*(NUMV+1)+j;
      u = -ur*j/NUMU+umin;
      v = vr*i/NUMV+vmin;
      if (!kb->change_colors)
      {
        if (kb->colors == COLORS_DEPTH)
          color(kb,(sin(u)*sin(0.5*v)+1.0)*M_PI*2.0/3.0,NULL,kb->col[k]);
        else if (kb->colors == COLORS_RAINBOW)
          color(kb,v,NULL,kb->col[k]);
      }
      kb->tex[k][0] = -32*u/(2.0*M_PI);
      kb->tex[k][1] = 32*v/(2.0*M_PI);
      cu = cos(u);
      su = sin(u);
      cv = cos(v);
      sv = sin(v);
      cv2 = cos(0.5*v);
      sv2 = sin(0.5*v);
      kb->x[k][0] = (PINCHED_TORUS_RADIUS+cu)*cv;
      kb->x[k][1] = (PINCHED_TORUS_RADIUS+cu)*sv;
      kb->x[k][2] = su*cv2;
      kb->x[k][3] = su*sv2;
      kb->xu[k][0] = -su*cv;
      kb->xu[k][1] = -su*sv;
      kb->xu[k][2] = cu*cv2;
      kb->xu[k][3] = cu*sv2;
      kb->xv[k][0] = -(PINCHED_TORUS_RADIUS+cu)*sv;
      kb->xv[k][1] = (PINCHED_TORUS_RADIUS+cu)*cv;
      kb->xv[k][2] = -0.5*su*sv2;
      kb->xv[k][3] = 0.5*su*cv2;
      for (l=0; l<4; l++)
      {
        kb->x[k][l] /= PINCHED_TORUS_RADIUS+RADIUS_INCR;
        kb->xu[k][l] /= PINCHED_TORUS_RADIUS+RADIUS_INCR;
        kb->xv[k][l] /= PINCHED_TORUS_RADIUS+RADIUS_INCR;
      }
    }
  }
}


/* Set up the Lawson Klein bottle coordinates, colors, and texture. */
static void setup_lawson(ModeInfo *mi, double umin, double umax, double vmin,
                         double vmax)
{
  int i, j, k;
  double u, v, ur, vr;
  double cu, su, cv, sv, cv2, sv2;
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  ur = umax-umin;
  vr = vmax-vmin;
  for (i=0; i<=NUMV; i++)
  {
    for (j=0; j<=NUMU; j++)
    {
      k = i*(NUMU+1)+j;
      u = -ur*j/NUMU+umin;
      v = vr*i/NUMV+vmin;
      if (!kb->change_colors)
      {
        if (kb->colors == COLORS_DEPTH)
          color(kb,(sin(u)*cos(0.5*v)+1.0)*M_PI*2.0/3.0,NULL,kb->col[k]);
        else if (kb->colors == COLORS_RAINBOW)
          color(kb,v,NULL,kb->col[k]);
      }
      kb->tex[k][0] = -32*u/(2.0*M_PI);
      kb->tex[k][1] = 32*v/(2.0*M_PI);
      cu = cos(u);
      su = sin(u);
      cv = cos(v);
      sv = sin(v);
      cv2 = cos(0.5*v);
      sv2 = sin(0.5*v);
      kb->x[k][0] = cu*cv;
      kb->x[k][1] = cu*sv;
      kb->x[k][2] = su*sv2;
      kb->x[k][3] = su*cv2;
      kb->xu[k][0] = -su*cv;
      kb->xu[k][1] = -su*sv;
      kb->xu[k][2] = cu*sv2;
      kb->xu[k][3] = cu*cv2;
      kb->xv[k][0] = -cu*sv;
      kb->xv[k][1] = cu*cv;
      kb->xv[k][2] = 0.5*su*cv2;
      kb->xv[k][3] = -0.5*su*sv2;
    }
  }
}


/* Rotate a 4D point x by the rotation matrix mat and project it to the 3D
   point p according to the 4D projection type that is stored in the
   kleinstruct kb. */
static void project_4d_point_to_3d(kleinstruct *kb, double x[4],
                                   float mat[4][4], float p[3])
{
  int l, m;
  double r, s;
  double y[4];

  for (l=0; l<4; l++)
  {
    r = 0.0;
    for (m=0; m<4; m++)
      r += mat[l][m]*x[m];
    y[l] = r;
  }
  if (kb->projection_4d == DISP_4D_ORTHOGRAPHIC)
  {
    for (l=0; l<3; l++)
      p[l] = y[l]+kb->offset4d[l];
  }
  else
  {
    s = y[3]+kb->offset4d[3];
    for (l=0; l<3; l++)
      p[l] = (y[l]+kb->offset4d[l])/s;
  }
}


/* Compute a tangent space basis in 3D from the 4D point x and the 4D
   partial derivative vectors xu and xv. The vectors x, xu, and xv are
   transformed by the 4D rotation matrix mat. The resulting tangent space
   basis is converted into the 3D rotation angles alpha, beta, and delta,
   which are stored in the kleinstruct kb. */
static void compute_tangent_space_basis_rotation(kleinstruct *kb, double x[4],
                                                 double xu[4], double xv[4],
                                                 float mat[4][4])
{
  int l;
  float pu[3], pv[3], pm[3], n[3], b[3];
  double y[4], yu[4], yv[4];
  double q, r, s, t;

  for (l=0; l<4; l++)
  {
    y[l] = (mat[l][0]*x[0]+mat[l][1]*x[1]+
            mat[l][2]*x[2]+mat[l][3]*x[3]);
    yu[l] = (mat[l][0]*xu[0]+mat[l][1]*xu[1]+
             mat[l][2]*xu[2]+mat[l][3]*xu[3]);
    yv[l] = (mat[l][0]*xv[0]+mat[l][1]*xv[1]+
             mat[l][2]*xv[2]+mat[l][3]*xv[3]);
  }
  if (kb->projection_4d == DISP_4D_ORTHOGRAPHIC)
  {
    for (l=0; l<3; l++)
    {
      pu[l] = yu[l];
      pv[l] = yv[l];
    }
  }
  else
  {
    s = y[3]+kb->offset4d[3];
    q = 1.0/s;
    t = q*q;
    for (l=0; l<3; l++)
    {
      r = y[l]+kb->offset4d[l];
      pu[l] = (yu[l]*s-r*yu[3])*t;
      pv[l] = (yv[l]*s-r*yv[3])*t;
    }
  }
  n[0] = pu[1]*pv[2]-pu[2]*pv[1];
  n[1] = pu[2]*pv[0]-pu[0]*pv[2];
  n[2] = pu[0]*pv[1]-pu[1]*pv[0];
  t = 1.0/(kb->side*4.0*sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]));
  n[0] *= t;
  n[1] *= t;
  n[2] *= t;
  pm[0] = pu[0]*kb->dumove+pv[0]*kb->dvmove;
  pm[1] = pu[1]*kb->dumove+pv[1]*kb->dvmove;
  pm[2] = pu[2]*kb->dumove+pv[2]*kb->dvmove;
  t = 1.0/(4.0*sqrt(pm[0]*pm[0]+pm[1]*pm[1]+pm[2]*pm[2]));
  pm[0] *= t;
  pm[1] *= t;
  pm[2] *= t;
  b[0] = n[1]*pm[2]-n[2]*pm[1];
  b[1] = n[2]*pm[0]-n[0]*pm[2];
  b[2] = n[0]*pm[1]-n[1]*pm[0];
  t = 1.0/(4.0*sqrt(b[0]*b[0]+b[1]*b[1]+b[2]*b[2]));
  b[0] *= t;
  b[1] *= t;
  b[2] *= t;

  /* Compute alpha, beta, delta from the three basis vectors.
         |  -b[0]  -b[1]  -b[2] |
     m = |   n[0]   n[1]   n[2] |
         | -pm[0] -pm[1] -pm[2] |
  */
  kb->alpha = atan2(-n[2],-pm[2])*180/M_PI;
  kb->beta = atan2(-b[2],sqrt(b[0]*b[0]+b[1]*b[1]))*180/M_PI;
  kb->delta = atan2(b[1],-b[0])*180/M_PI;

  /* Compute the rotation that rotates the Klein bottle in 4D. */
  rotateall(kb->alpha,kb->beta,kb->delta,kb->zeta,kb->eta,kb->theta,mat);
}


/* Compute the current walk frame for the figure-8 Klein bottle, i.e.,
   the coordinate system of the point and direction at which the viewer is
   currently walking on the projective plane. */
static void compute_walk_frame_figure8(kleinstruct *kb, float mat[4][4])
{
  float p[3];
  int l;
  double u, v;
  double xx[4], xxu[4], xxv[4];
  double cu, su, cv, sv, cv2, sv2, c2u, s2u;

  /* Compute the rotation that rotates the Klein bottle in 4D without the
     trackball rotations. */
  rotateall4d(kb->zeta,kb->eta,kb->theta,mat);

  u = kb->umove;
  v = kb->vmove;
  cu = cos(u);
  su = sin(u);
  cv = cos(v);
  sv = sin(v);
  cv2 = cos(0.5*v);
  sv2 = sin(0.5*v);
  c2u = cos(2.0*u);
  s2u = sin(2.0*u);
  xx[0] = (su*cv2-s2u*sv2+FIGURE_8_RADIUS)*cv;
  xx[1] = (su*cv2-s2u*sv2+FIGURE_8_RADIUS)*sv;
  xx[2] = su*sv2+s2u*cv2;
  xx[3] = cu;
  xxu[0] = (cu*cv2-2.0*c2u*sv2)*cv;
  xxu[1] = (cu*cv2-2.0*c2u*sv2)*sv;
  xxu[2] = cu*sv2+2.0*c2u*cv2;
  xxu[3] = -su;
  xxv[0] = ((-0.5*su*sv2-0.5*s2u*cv2)*cv-
            (su*cv2-s2u*sv2+FIGURE_8_RADIUS)*sv);
  xxv[1] = ((-0.5*su*sv2-0.5*s2u*cv2)*sv+
            (su*cv2-s2u*sv2+FIGURE_8_RADIUS)*cv);
  xxv[2] = 0.5*su*cv2-0.5*s2u*sv2;
  xxv[3] = 0.0;
  for (l=0; l<4; l++)
  {
    xx[l] /= FIGURE_8_RADIUS+RADIUS_INCR;
    xxu[l] /= FIGURE_8_RADIUS+RADIUS_INCR;
    xxv[l] /= FIGURE_8_RADIUS+RADIUS_INCR;
  }

  compute_tangent_space_basis_rotation(kb,xx,xxu,xxv,mat);

  u = kb->umove;
  v = kb->vmove;
  cu = cos(u);
  su = sin(u);
  cv = cos(v);
  sv = sin(v);
  cv2 = cos(0.5*v);
  sv2 = sin(0.5*v);
  s2u = sin(2.0*u);
  xx[0] = (su*cv2-s2u*sv2+FIGURE_8_RADIUS)*cv;
  xx[1] = (su*cv2-s2u*sv2+FIGURE_8_RADIUS)*sv;
  xx[2] = su*sv2+s2u*cv2;
  xx[3] = cu;
  for (l=0; l<4; l++)
    xx[l] /= FIGURE_8_RADIUS+RADIUS_INCR;

  project_4d_point_to_3d(kb,xx,mat,p);

  kb->offset3d[0] = -p[0];
  kb->offset3d[1] = -p[1]-DELTAY;
  kb->offset3d[2] = -p[2];
}


/* Compute the current walk frame for the pinched torus Klein bottle, i.e.,
   the coordinate system of the point and direction at which the viewer is
   currently walking on the projective plane. */
static void compute_walk_frame_pinched_torus(kleinstruct *kb, float mat[4][4])
{
  float p[3];
  int l;
  double u, v;
  double xx[4], xxu[4], xxv[4];
  double cu, su, cv, sv, cv2, sv2;

  /* Compute the rotation that rotates the Klein bottle in 4D without the
     trackball rotations. */
  rotateall4d(kb->zeta,kb->eta,kb->theta,mat);

  u = kb->umove;
  v = kb->vmove;
  cu = cos(u);
  su = sin(u);
  cv = cos(v);
  sv = sin(v);
  cv2 = cos(0.5*v);
  sv2 = sin(0.5*v);
  xx[0] = (PINCHED_TORUS_RADIUS+cu)*cv;
  xx[1] = (PINCHED_TORUS_RADIUS+cu)*sv;
  xx[2] = su*cv2;
  xx[3] = su*sv2;
  xxu[0] = -su*cv;
  xxu[1] = -su*sv;
  xxu[2] = cu*cv2;
  xxu[3] = cu*sv2;
  xxv[0] = -(PINCHED_TORUS_RADIUS+cu)*sv;
  xxv[1] = (PINCHED_TORUS_RADIUS+cu)*cv;
  xxv[2] = -0.5*su*sv2;
  xxv[3] = 0.5*su*cv2;
  for (l=0; l<4; l++)
  {
    xx[l] /= PINCHED_TORUS_RADIUS+RADIUS_INCR;
    xxu[l] /= PINCHED_TORUS_RADIUS+RADIUS_INCR;
    xxv[l] /= PINCHED_TORUS_RADIUS+RADIUS_INCR;
  }

  compute_tangent_space_basis_rotation(kb,xx,xxu,xxv,mat);

  u = kb->umove;
  v = kb->vmove;
  cu = cos(u);
  su = sin(u);
  cv = cos(v);
  sv = sin(v);
  cv2 = cos(0.5*v);
  sv2 = sin(0.5*v);
  xx[0] = (PINCHED_TORUS_RADIUS+cu)*cv;
  xx[1] = (PINCHED_TORUS_RADIUS+cu)*sv;
  xx[2] = su*cv2;
  xx[3] = su*sv2;
  for (l=0; l<4; l++)
    xx[l] /= PINCHED_TORUS_RADIUS+RADIUS_INCR;

  project_4d_point_to_3d(kb,xx,mat,p);

  kb->offset3d[0] = -p[0];
  kb->offset3d[1] = -p[1]-DELTAY;
  kb->offset3d[2] = -p[2];
}


/* Compute the current walk frame for the Lawson Klein bottle, i.e.,
   the coordinate system of the point and direction at which the viewer is
   currently walking on the projective plane. */
static void compute_walk_frame_lawson(kleinstruct *kb, float mat[4][4])
{
  float p[3];
  double u, v;
  double xx[4], xxu[4], xxv[4];
  double cu, su, cv, sv, cv2, sv2;

  /* Compute the rotation that rotates the Klein bottle in 4D without the
     trackball rotations. */
  rotateall4d(kb->zeta,kb->eta,kb->theta,mat);

  u = kb->umove;
  v = kb->vmove;
  cu = cos(u);
  su = sin(u);
  cv = cos(v);
  sv = sin(v);
  cv2 = cos(0.5*v);
  sv2 = sin(0.5*v);
  xx[0] = cu*cv;
  xx[1] = cu*sv;
  xx[2] = su*sv2;
  xx[3] = su*cv2;
  xxu[0] = -su*cv;
  xxu[1] = -su*sv;
  xxu[2] = cu*sv2;
  xxu[3] = cu*cv2;
  xxv[0] = -cu*sv;
  xxv[1] = cu*cv;
  xxv[2] = 0.5*su*cv2;
  xxv[3] = -0.5*su*sv2;

  compute_tangent_space_basis_rotation(kb,xx,xxu,xxv,mat);

  u = kb->umove;
  v = kb->vmove;
  cu = cos(u);
  su = sin(u);
  cv = cos(v);
  sv = sin(v);
  cv2 = cos(0.5*v);
  sv2 = sin(0.5*v);
  xx[0] = cu*cv;
  xx[1] = cu*sv;
  xx[2] = su*sv2;
  xx[3] = su*cv2;

  project_4d_point_to_3d(kb,xx,mat,p);

  kb->offset3d[0] = -p[0];
  kb->offset3d[1] = -p[1]-DELTAY;
  kb->offset3d[2] = -p[2];
}


/* Set the OpenGL fixed functionality projection and lighting state
   for displaying a Klein bottle. */
static void set_opengl_state_ff(ModeInfo *mi, float matc[3][3])
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
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  if (kb->projection_3d == DISP_3D_PERSPECTIVE ||
      kb->view == VIEW_WALK || kb->view == VIEW_WALKTURN)
  {
    if (kb->view == VIEW_WALK || kb->view == VIEW_WALKTURN)
      gluPerspective(60.0,kb->aspect,0.01,10.0);
    else
      gluPerspective(60.0,kb->aspect,0.1,10.0);
  }
  else
  {
    if (kb->aspect >= 1.0)
      glOrtho(-kb->aspect,kb->aspect,-1.0,1.0,0.1,10.0);
    else
      glOrtho(-1.0,1.0,-1.0/kb->aspect,1.0/kb->aspect,0.1,10.0);
  }
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  if (kb->display_mode == DISP_SURFACE)
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
  else if (kb->display_mode == DISP_TRANSPARENT)
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
  else  /* kb->display_mode == DISP_WIREFRAME */
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

  if (marks)
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

  if (!kb->change_colors)
  {
    if (kb->colors == COLORS_ONESIDED)
    {
      glColor3fv(mat_diff_oneside);
      if (kb->display_mode == DISP_TRANSPARENT)
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
    else if (kb->colors == COLORS_TWOSIDED)
    {
      glColor3fv(mat_diff_red);
      if (kb->display_mode == DISP_TRANSPARENT)
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
  else /* kb->change_colors */
  {
    color(kb,0.0,matc,mat_diff_dyn);
    if (kb->colors == COLORS_ONESIDED)
    {
      glColor3fv(mat_diff_dyn);
      glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,mat_diff_dyn);
    }
    else if (kb->colors == COLORS_TWOSIDED)
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
  glBindTexture(GL_TEXTURE_2D,kb->tex_name);
}


#ifdef HAVE_GLSL

/* Set the OpenGL programmable functionality projection and lighting state
   for displaying a Klein bottle. */
static void set_opengl_state_pf(ModeInfo *mi, float mat[4][4],
                                float matc[3][3])
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
  float mat_diff_dyn[4], mat_diff_dyn_compl[4];
  GLfloat light_direction[3], half_vector[3], len;
  GLfloat p_mat[16];
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  glsl_Identity(p_mat);
  if (kb->projection_3d == DISP_3D_PERSPECTIVE ||
      kb->view == VIEW_WALK || kb->view == VIEW_WALKTURN)
  {
    if (kb->view == VIEW_WALK || kb->view == VIEW_WALKTURN)
      glsl_Perspective(p_mat,60.0f,kb->aspect,0.01f,10.0f);
    else
      glsl_Perspective(p_mat,60.0f,kb->aspect,0.1f,10.0f);
  }
  else
  {
    if (kb->aspect >= 1.0)
      glsl_Orthographic(p_mat,-kb->aspect,kb->aspect,-1.0f,1.0f,0.1f,10.0f);
    else
      glsl_Orthographic(p_mat,-1.0f,1.0f,-1.0f/kb->aspect,1.0f/kb->aspect,
                        0.1f,10.0f);
  }
  glUniformMatrix4fv(kb->mat_rot_index,1,GL_TRUE,(GLfloat *)mat);
  glUniformMatrix4fv(kb->mat_p_index,1,GL_FALSE,p_mat);
  glUniform1i(kb->bool_persp_index,kb->projection_4d == DISP_4D_PERSPECTIVE);
  glUniform4fv(kb->off4d_index,1,kb->offset4d);
  glUniform4fv(kb->off3d_index,1,kb->offset3d);

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

  if (kb->display_mode == DISP_SURFACE)
  {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glUniform4fv(kb->glbl_ambient_index,1,light_model_ambient);
    glUniform4fv(kb->lt_ambient_index,1,light_ambient);
    glUniform4fv(kb->lt_diffuse_index,1,light_diffuse);
    glUniform4fv(kb->lt_specular_index,1,light_specular);
    glUniform3fv(kb->lt_direction_index,1,light_direction);
    glUniform3fv(kb->lt_halfvect_index,1,half_vector);
    glUniform4fv(kb->specular_index,1,mat_specular);
    glUniform1f(kb->shininess_index,50.0f);
    glUniform1i(kb->draw_lines_index,GL_FALSE);
  }
  else if (kb->display_mode == DISP_TRANSPARENT)
  {
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE);
    glUniform4fv(kb->glbl_ambient_index,1,light_model_ambient);
    glUniform4fv(kb->lt_ambient_index,1,light_ambient);
    glUniform4fv(kb->lt_diffuse_index,1,light_diffuse);
    glUniform4fv(kb->lt_specular_index,1,light_specular);
    glUniform3fv(kb->lt_direction_index,1,light_direction);
    glUniform3fv(kb->lt_halfvect_index,1,half_vector);
    glUniform4fv(kb->specular_index,1,mat_specular);
    glUniform1f(kb->shininess_index,50.0f);
    glUniform1i(kb->draw_lines_index,GL_FALSE);
  }
  else /* kb->display_mode == DISP_WIREFRAME */
  {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glUniform1i(kb->draw_lines_index,GL_TRUE);
  }

  if (marks)
    glEnable(GL_TEXTURE_2D);
  else
    glDisable(GL_TEXTURE_2D);

  glUniform4fv(kb->front_ambient_index,1,mat_diff_white);
  glUniform4fv(kb->front_diffuse_index,1,mat_diff_white);
  glUniform4fv(kb->back_ambient_index,1,mat_diff_white);
  glUniform4fv(kb->back_diffuse_index,1,mat_diff_white);
  glVertexAttrib4f(kb->color_index,1.0f,1.0f,1.0f,1.0f);
  if (!kb->change_colors)
  {
    if (kb->colors == COLORS_ONESIDED)
    {
      if (kb->display_mode == DISP_TRANSPARENT)
      {
        glUniform4fv(kb->front_ambient_index,1,mat_diff_trans_oneside);
        glUniform4fv(kb->front_diffuse_index,1,mat_diff_trans_oneside);
        glUniform4fv(kb->back_ambient_index,1,mat_diff_trans_oneside);
        glUniform4fv(kb->back_diffuse_index,1,mat_diff_trans_oneside);
      }
      else if (kb->display_mode == DISP_SURFACE)
      {
        glUniform4fv(kb->front_ambient_index,1,mat_diff_oneside);
        glUniform4fv(kb->front_diffuse_index,1,mat_diff_oneside);
        glUniform4fv(kb->back_ambient_index,1,mat_diff_oneside);
        glUniform4fv(kb->back_diffuse_index,1,mat_diff_oneside);
      }
      else /* kb->display_mode == DISP_WIREFRAME */
      {
        glVertexAttrib4fv(kb->color_index,mat_diff_oneside);
      }
    }
    else if (kb->colors == COLORS_TWOSIDED)
    {
      if (kb->display_mode == DISP_TRANSPARENT)
      {
        glUniform4fv(kb->front_ambient_index,1,mat_diff_trans_red);
        glUniform4fv(kb->front_diffuse_index,1,mat_diff_trans_red);
        glUniform4fv(kb->back_ambient_index,1,mat_diff_trans_green);
        glUniform4fv(kb->back_diffuse_index,1,mat_diff_trans_green);
      }
      else if (kb->display_mode == DISP_SURFACE)
      {
        glUniform4fv(kb->front_ambient_index,1,mat_diff_red);
        glUniform4fv(kb->front_diffuse_index,1,mat_diff_red);
        glUniform4fv(kb->back_ambient_index,1,mat_diff_green);
        glUniform4fv(kb->back_diffuse_index,1,mat_diff_green);
      }
      else /* kb->display_mode == DISP_WIREFRAME */
      {
        glVertexAttrib4fv(kb->color_index,mat_diff_red);
      }
    }
  }
  else /* kb->change_colors */
  {
    color(kb,0.0,matc,mat_diff_dyn);
    if (kb->colors == COLORS_ONESIDED)
    {
      if (kb->display_mode == DISP_TRANSPARENT ||
          kb->display_mode == DISP_SURFACE)
      {
        glUniform4fv(kb->front_ambient_index,1,mat_diff_dyn);
        glUniform4fv(kb->front_diffuse_index,1,mat_diff_dyn);
        glUniform4fv(kb->back_ambient_index,1,mat_diff_dyn);
        glUniform4fv(kb->back_diffuse_index,1,mat_diff_dyn);
      }
      else /* kb->display_mode == DISP_WIREFRAME */
      {
        glVertexAttrib4fv(kb->color_index,mat_diff_dyn);
      }
    }
    else if (kb->colors == COLORS_TWOSIDED)
    {
      if (kb->display_mode == DISP_TRANSPARENT ||
          kb->display_mode == DISP_SURFACE)
      {
        mat_diff_dyn_compl[0] = 1.0f-mat_diff_dyn[0];
        mat_diff_dyn_compl[1] = 1.0f-mat_diff_dyn[1];
        mat_diff_dyn_compl[2] = 1.0f-mat_diff_dyn[2];
        mat_diff_dyn_compl[3] = mat_diff_dyn[3];
        glUniform4fv(kb->front_ambient_index,1,mat_diff_dyn);
        glUniform4fv(kb->front_diffuse_index,1,mat_diff_dyn);
        glUniform4fv(kb->back_ambient_index,1,mat_diff_dyn_compl);
        glUniform4fv(kb->back_diffuse_index,1,mat_diff_dyn_compl);
      }
      else /* kb->display_mode == DISP_WIREFRAME */
      {
        glVertexAttrib4fv(kb->color_index,mat_diff_dyn);
      }
    }
  }

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D,kb->tex_name);
  glUniform1i(kb->texture_sampler_index,0);
  glUniform1i(kb->bool_textures_index,marks);
  glUniform1i(kb->bottle_type_index,kb->bottle_type);
}

#endif /* HAVE_GLSL */


/* Draw a figure-8 Klein bottle projected into 3D. */
static int figure8_ff(ModeInfo *mi, double umin, double umax, double vmin,
                      double vmax)
{
  int polys;
  float pu[3], pv[3], mat[4][4], matc[3][3];
  int i, j, k, l, m, o;
  double u, v, ur, vr;
  double y[4], yu[4], yv[4];
  double q, r, s, t;
  float q1[4], q2[4], r1[4][4], r2[4][4];
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  if (kb->change_colors)
    rotateall3d(kb->rho,kb->sigma,kb->tau,matc);

  set_opengl_state_ff(mi,matc);

  if (kb->view == VIEW_WALK || kb->view == VIEW_WALKTURN)
  {
    compute_walk_frame_figure8(kb,mat);
  }
  else
  {
    /* Compute the rotation that rotates the Klein bottle in 4D, including
       the trackball rotations. */
    rotateall(kb->alpha,kb->beta,kb->delta,kb->zeta,kb->eta,kb->theta,r1);

    gltrackball_get_quaternion(kb->trackballs[0],q1);
    gltrackball_get_quaternion(kb->trackballs[1],q2);
    quats_to_rotmat(q1,q2,r2);

    mult_rotmat(r2,r1,mat);
  }

  /* Project the points from 4D to 3D. */
  for (i=0; i<=NUMU; i++)
  {
    for (j=0; j<=NUMV; j++)
    {
      o = i*(NUMV+1)+j;
      for (l=0; l<4; l++)
      {
        y[l] = (mat[l][0]*kb->x[o][0]+mat[l][1]*kb->x[o][1]+
                mat[l][2]*kb->x[o][2]+mat[l][3]*kb->x[o][3]);
        yu[l] = (mat[l][0]*kb->xu[o][0]+mat[l][1]*kb->xu[o][1]+
                 mat[l][2]*kb->xu[o][2]+mat[l][3]*kb->xu[o][3]);
        yv[l] = (mat[l][0]*kb->xv[o][0]+mat[l][1]*kb->xv[o][1]+
                 mat[l][2]*kb->xv[o][2]+mat[l][3]*kb->xv[o][3]);
      }
      if (kb->projection_4d == DISP_4D_ORTHOGRAPHIC)
      {
        for (l=0; l<3; l++)
        {
          kb->pp[o][l] = (y[l]+kb->offset4d[l])+kb->offset3d[l];
          pu[l] = yu[l];
          pv[l] = yv[l];
        }
      }
      else
      {
        s = y[3]+kb->offset4d[3];
        q = 1.0/s;
        t = q*q;
        for (l=0; l<3; l++)
        {
          r = y[l]+kb->offset4d[l];
          kb->pp[o][l] = r*q+kb->offset3d[l];
          pu[l] = (yu[l]*s-r*yu[3])*t;
          pv[l] = (yv[l]*s-r*yv[3])*t;
        }
      }
      kb->pn[o][0] = pu[1]*pv[2]-pu[2]*pv[1];
      kb->pn[o][1] = pu[2]*pv[0]-pu[0]*pv[2];
      kb->pn[o][2] = pu[0]*pv[1]-pu[1]*pv[0];
      t = 1.0/sqrt(kb->pn[o][0]*kb->pn[o][0]+kb->pn[o][1]*kb->pn[o][1]+
                   kb->pn[o][2]*kb->pn[o][2]);
      kb->pn[o][0] *= t;
      kb->pn[o][1] *= t;
      kb->pn[o][2] *= t;
    }
  }

  ur = umax-umin;
  vr = vmax-vmin;
  for (i=0; i<NUMU; i++)
  {
    if (kb->appearance == APPEARANCE_BANDS && ((i & (NUMB-1)) >= NUMB/2))
      continue;
    if (kb->display_mode == DISP_WIREFRAME)
      glBegin(GL_QUAD_STRIP);
    else
      glBegin(GL_TRIANGLE_STRIP);
    for (j=0; j<=NUMV; j++)
    {
      for (k=0; k<=1; k++)
      {
        l = i+k;
        m = j;
        o = l*(NUMV+1)+m;
        glNormal3fv(kb->pn[o]);
        glTexCoord2fv(kb->tex[o]);
        if (kb->change_colors)
        {
          if (kb->colors == COLORS_DEPTH)
          {
            u = -ur*m/NUMU+umin;
            color(kb,(cos(u)+1.0)*M_PI*2.0/3.0,matc,kb->col[o]);
          }
          else if (kb->colors == COLORS_RAINBOW)
          {
            v = vr*l/NUMV+vmin;
            color(kb,v,matc,kb->col[o]);
          }
        }
        if (kb->colors != COLORS_ONESIDED && kb->colors != COLORS_TWOSIDED)
        {
          glColor3fv(kb->col[o]);
          glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,kb->col[o]);
        }
        glVertex3fv(kb->pp[o]);
      }
    }
    glEnd();
  }

  polys = 2*NUMU*NUMV;
  if (kb->appearance != APPEARANCE_SOLID)
    polys /= 2;
  return polys;
}


#ifdef HAVE_GLSL

/* Draw a figure-8 Klein bottle projected into 3D. */
static int figure8_pf(ModeInfo *mi, double umin, double umax, double vmin,
                      double vmax)
{
  int polys;
  float mat[4][4], matc[3][3];
  int i, j, k, l, m, o;
  double u, v, ur, vr;
  float q1[4], q2[4], r1[4][4], r2[4][4];
  GLsizeiptr index_offset;
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  if (!kb->use_shaders)
    return 0;

  if (!kb->buffers_initialized)
  {
    /* The u and v values need to be computed once (or each time the value
       of appearance changes, once we support that). */
    ur = umax-umin;
    vr = vmax-vmin;
    for (i=0; i<=NUMU; i++)
    {
      for (j=0; j<=NUMV; j++)
      {
        o = i*(NUMU+1)+j;
        u = -ur*j/NUMU+umin;
        v = vr*i/NUMV+vmin;
        kb->uv[o][0] = u;
        kb->uv[o][1] = v;
      }
    }
    glBindBuffer(GL_ARRAY_BUFFER,kb->vertex_uv_buffer);
    glBufferData(GL_ARRAY_BUFFER,2*(NUMU+1)*(NUMV+1)*sizeof(GLfloat),
                 kb->uv,GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);

    glBindBuffer(GL_ARRAY_BUFFER,kb->vertex_t_buffer);
    glBufferData(GL_ARRAY_BUFFER,2*(NUMU+1)*(NUMV+1)*sizeof(GLfloat),
                 kb->tex,GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);

    if (!kb->change_colors &&
        kb->colors != COLORS_ONESIDED && kb->colors != COLORS_TWOSIDED)
    {
      glBindBuffer(GL_ARRAY_BUFFER,kb->color_buffer);
      glBufferData(GL_ARRAY_BUFFER,4*(NUMU+1)*(NUMV+1)*sizeof(GLfloat),
                   kb->col,GL_STATIC_DRAW);
      glBindBuffer(GL_ARRAY_BUFFER,0);
    }

    /* The indices only need to be computed once (or each time the value of
       appearance changes, once we support that). */
    kb->ni = 0;
    kb->ne = 0;
    kb->nt = 0;
    if (kb->display_mode != DISP_WIREFRAME)
    {
      for (i=0; i<NUMU; i++)
      {
        if (kb->appearance == APPEARANCE_BANDS && ((i & (NUMB-1)) >= NUMB/2))
          continue;
        for (j=0; j<=NUMV; j++)
        {
          for (k=0; k<=1; k++)
          {
            l = i+k;
            m = j;
            o = l*(NUMV+1)+m;
            kb->indices[kb->ni++] = o;
          }
        }
        kb->ne++;
      }
      kb->nt = 2*(NUMV+1);
    }
    else /* kb->display_mode == DISP_WIREFRAME */
    {
      for (i=0; i<NUMU; i++)
      {
        if (kb->appearance == APPEARANCE_BANDS && ((i & (NUMB-1)) > NUMB/2))
          continue;
        if (kb->appearance == APPEARANCE_BANDS && ((i & (NUMB-1)) == NUMB/2))
        {
          for (j=0; j<NUMV; j++)
          {
            kb->indices[kb->ni++] = i*(NUMV+1)+j;
            kb->indices[kb->ni++] = i*(NUMV+1)+j+1;
          }
          continue;
        }
        for (j=0; j<=NUMV; j++)
        {
          kb->indices[kb->ni++] = i*(NUMV+1)+j;
          kb->indices[kb->ni++] = i*(NUMV+1)+j+1;
          if (i < NUMV)
          {
            kb->indices[kb->ni++] = i*(NUMV+1)+j;
            kb->indices[kb->ni++] = (i+1)*(NUMV+1)+j;
          }
        }
      }
      kb->ne = 1;
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,kb->indices_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,kb->ni*sizeof(GLuint),
                 kb->indices,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

    kb->buffers_initialized = True;
  }

  if (kb->change_colors)
    rotateall3d(kb->rho,kb->sigma,kb->tau,matc);

  if (kb->view == VIEW_WALK || kb->view == VIEW_WALKTURN)
  {
    compute_walk_frame_figure8(kb,mat);
  }
  else
  {
    /* Compute the rotation that rotates the Klein bottle in 4D, including
       the trackball rotations. */
    rotateall(kb->alpha,kb->beta,kb->delta,kb->zeta,kb->eta,kb->theta,r1);

    gltrackball_get_quaternion(kb->trackballs[0],q1);
    gltrackball_get_quaternion(kb->trackballs[1],q2);
    quats_to_rotmat(q1,q2,r2);

    mult_rotmat(r2,r1,mat);
  }

  if (kb->change_colors &&
      (kb->colors == COLORS_RAINBOW || kb->colors == COLORS_DEPTH))
  {
    ur = umax-umin;
    vr = vmax-vmin;
    for (i=0; i<=NUMU; i++)
    {
      for (j=0; j<=NUMV; j++)
      {
        o = i*(NUMV+1)+j;
        if (kb->colors == COLORS_DEPTH)
        {
          u = -ur*j/NUMU+umin;
          color(kb,(cos(u)+1.0)*M_PI*2.0/3.0,matc,kb->col[o]);
        }
        else if (kb->colors == COLORS_RAINBOW)
        {
          v = vr*i/NUMV+vmin;
          color(kb,v,matc,kb->col[o]);
        }
      }
    }
  }

  glUseProgram(kb->shader_program);

  set_opengl_state_pf(mi,mat,matc);

  glEnableVertexAttribArray(kb->vertex_uv_index);
  glBindBuffer(GL_ARRAY_BUFFER,kb->vertex_uv_buffer);
  glVertexAttribPointer(kb->vertex_uv_index,2,GL_FLOAT,GL_FALSE,0,0);

  glEnableVertexAttribArray(kb->vertex_t_index);
  glBindBuffer(GL_ARRAY_BUFFER,kb->vertex_t_buffer);
  glVertexAttribPointer(kb->vertex_t_index,2,GL_FLOAT,GL_FALSE,0,0);

  if (kb->colors != COLORS_ONESIDED && kb->colors != COLORS_TWOSIDED)
  {
    glEnableVertexAttribArray(kb->color_index);
    glBindBuffer(GL_ARRAY_BUFFER,kb->color_buffer);
    if (kb->change_colors)
      glBufferData(GL_ARRAY_BUFFER,4*(NUMU+1)*(NUMV+1)*sizeof(GLfloat),
                   kb->col,GL_STREAM_DRAW);
    glVertexAttribPointer(kb->color_index,4,GL_FLOAT,GL_FALSE,0,0);
  }

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,kb->indices_buffer);

  if (kb->display_mode != DISP_WIREFRAME)
  {
    for (i=0; i<kb->ne; i++)
    {
      index_offset = kb->nt*i*sizeof(GLuint);
      glDrawElements(GL_TRIANGLE_STRIP,kb->nt,GL_UNSIGNED_INT,
                     (const GLvoid *)index_offset);
      polys = 2*kb->ne*NUMV;
    }
  }
  else /* kb->display_mode == DISP_WIREFRAME */
  {
    glLineWidth(1.0f);
    index_offset = 0;
    glDrawElements(GL_LINES,kb->ni,GL_UNSIGNED_INT,
                   (const void *)index_offset);
  }

  glDisableVertexAttribArray(kb->vertex_uv_index);
  glDisableVertexAttribArray(kb->vertex_t_index);
  if (kb->colors != COLORS_ONESIDED && kb->colors != COLORS_TWOSIDED)
    glDisableVertexAttribArray(kb->color_index);
  glBindBuffer(GL_ARRAY_BUFFER,0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

  glUseProgram(0);

  polys = 2*NUMU*NUMV;
  if (kb->appearance != APPEARANCE_SOLID)
    polys /= 2;
  return polys;
}

#endif /* HAVE_GLSL */


/* Draw a pinched torus Klein bottle projected into 3D. */
static int pinched_torus_ff(ModeInfo *mi, double umin, double umax,
                            double vmin, double vmax)
{
  int polys;
  float pu[3], pv[3], mat[4][4], matc[3][3];
  int i, j, k, l, m, o;
  double u, v, ur, vr;
  double y[4], yu[4], yv[4];
  double q, r, s, t;
  float q1[4], q2[4], r1[4][4], r2[4][4];
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  if (kb->change_colors)
    rotateall3d(kb->rho,kb->sigma,kb->tau,matc);

  set_opengl_state_ff(mi,matc);

  if (kb->view == VIEW_WALK || kb->view == VIEW_WALKTURN)
  {
    compute_walk_frame_pinched_torus(kb,mat);
  }
  else
  {
    /* Compute the rotation that rotates the Klein bottle in 4D, including
       the trackball rotations. */
    rotateall(kb->alpha,kb->beta,kb->delta,kb->zeta,kb->eta,kb->theta,r1);

    gltrackball_get_quaternion(kb->trackballs[0],q1);
    gltrackball_get_quaternion(kb->trackballs[1],q2);
    quats_to_rotmat(q1,q2,r2);

    mult_rotmat(r2,r1,mat);
  }

  /* Project the points from 4D to 3D. */
  for (i=0; i<=NUMU; i++)
  {
    for (j=0; j<=NUMV; j++)
    {
      o = i*(NUMV+1)+j;
      for (l=0; l<4; l++)
      {
        y[l] = (mat[l][0]*kb->x[o][0]+mat[l][1]*kb->x[o][1]+
                mat[l][2]*kb->x[o][2]+mat[l][3]*kb->x[o][3]);
        yu[l] = (mat[l][0]*kb->xu[o][0]+mat[l][1]*kb->xu[o][1]+
                 mat[l][2]*kb->xu[o][2]+mat[l][3]*kb->xu[o][3]);
        yv[l] = (mat[l][0]*kb->xv[o][0]+mat[l][1]*kb->xv[o][1]+
                 mat[l][2]*kb->xv[o][2]+mat[l][3]*kb->xv[o][3]);
      }
      if (kb->projection_4d == DISP_4D_ORTHOGRAPHIC)
      {
        for (l=0; l<3; l++)
        {
          kb->pp[o][l] = (y[l]+kb->offset4d[l])+kb->offset3d[l];
          pu[l] = yu[l];
          pv[l] = yv[l];
        }
      }
      else
      {
        s = y[3]+kb->offset4d[3];
        q = 1.0/s;
        t = q*q;
        for (l=0; l<3; l++)
        {
          r = y[l]+kb->offset4d[l];
          kb->pp[o][l] = r*q+kb->offset3d[l];
          pu[l] = (yu[l]*s-r*yu[3])*t;
          pv[l] = (yv[l]*s-r*yv[3])*t;
        }
      }
      kb->pn[o][0] = pu[1]*pv[2]-pu[2]*pv[1];
      kb->pn[o][1] = pu[2]*pv[0]-pu[0]*pv[2];
      kb->pn[o][2] = pu[0]*pv[1]-pu[1]*pv[0];
      t = 1.0/sqrt(kb->pn[o][0]*kb->pn[o][0]+kb->pn[o][1]*kb->pn[o][1]+
                   kb->pn[o][2]*kb->pn[o][2]);
      kb->pn[o][0] *= t;
      kb->pn[o][1] *= t;
      kb->pn[o][2] *= t;
    }
  }

  ur = umax-umin;
  vr = vmax-vmin;
  for (i=0; i<NUMU; i++)
  {
    if (kb->appearance == APPEARANCE_BANDS && ((i & (NUMB-1)) >= NUMB/2))
      continue;
    if (kb->display_mode == DISP_WIREFRAME)
      glBegin(GL_QUAD_STRIP);
    else
      glBegin(GL_TRIANGLE_STRIP);
    for (j=0; j<=NUMV; j++)
    {
      for (k=0; k<=1; k++)
      {
        l = i+k;
        m = j;
        o = l*(NUMV+1)+m;
        glNormal3fv(kb->pn[o]);
        glTexCoord2fv(kb->tex[o]);
        if (kb->change_colors)
        {
          v = vr*l/NUMV+vmin;
          if (kb->colors == COLORS_DEPTH)
          {
            u = -ur*m/NUMU+umin;
            color(kb,(sin(u)*sin(0.5*v)+1.0)*M_PI*2.0/3.0,matc,kb->col[o]);
          }
          else if (kb->colors == COLORS_RAINBOW)
          {
            color(kb,v,matc,kb->col[o]);
          }
        }
        if (kb->colors != COLORS_ONESIDED && kb->colors != COLORS_TWOSIDED)
        {
          glColor3fv(kb->col[o]);
          glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,kb->col[o]);
        }
        glVertex3fv(kb->pp[o]);
      }
    }
    glEnd();
  }

  polys = 2*NUMU*NUMV;
  if (kb->appearance != APPEARANCE_SOLID)
    polys /= 2;
  return polys;
}


#ifdef HAVE_GLSL

/* Draw a pinched torus Klein bottle projected into 3D. */
static int pinched_torus_pf(ModeInfo *mi, double umin, double umax,
                            double vmin, double vmax)
{
  int polys;
  float mat[4][4], matc[3][3];
  int i, j, k, l, m, o;
  double u, v, ur, vr;
  float q1[4], q2[4], r1[4][4], r2[4][4];
  GLsizeiptr index_offset;
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  if (!kb->use_shaders)
    return 0;

  if (!kb->buffers_initialized)
  {
    /* The u and v values need to be computed once (or each time the value
       of appearance changes, once we support that). */
    ur = umax-umin;
    vr = vmax-vmin;
    for (i=0; i<=NUMU; i++)
    {
      for (j=0; j<=NUMV; j++)
      {
        o = i*(NUMU+1)+j;
        u = -ur*j/NUMU+umin;
        v = vr*i/NUMV+vmin;
        kb->uv[o][0] = u;
        kb->uv[o][1] = v;
      }
    }
    glBindBuffer(GL_ARRAY_BUFFER,kb->vertex_uv_buffer);
    glBufferData(GL_ARRAY_BUFFER,2*(NUMU+1)*(NUMV+1)*sizeof(GLfloat),
                 kb->uv,GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);

    glBindBuffer(GL_ARRAY_BUFFER,kb->vertex_t_buffer);
    glBufferData(GL_ARRAY_BUFFER,2*(NUMU+1)*(NUMV+1)*sizeof(GLfloat),
                 kb->tex,GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);

    if (!kb->change_colors &&
        kb->colors != COLORS_ONESIDED && kb->colors != COLORS_TWOSIDED)
    {
      glBindBuffer(GL_ARRAY_BUFFER,kb->color_buffer);
      glBufferData(GL_ARRAY_BUFFER,4*(NUMU+1)*(NUMV+1)*sizeof(GLfloat),
                   kb->col,GL_STATIC_DRAW);
      glBindBuffer(GL_ARRAY_BUFFER,0);
    }

    /* The indices only need to be computed once (or each time the value of
       appearance changes, once we support that). */
    kb->ni = 0;
    kb->ne = 0;
    kb->nt = 0;
    if (kb->display_mode != DISP_WIREFRAME)
    {
      for (i=0; i<NUMU; i++)
      {
        if (kb->appearance == APPEARANCE_BANDS && ((i & (NUMB-1)) >= NUMB/2))
          continue;
        for (j=0; j<=NUMV; j++)
        {
          for (k=0; k<=1; k++)
          {
            l = i+k;
            m = j;
            o = l*(NUMV+1)+m;
            kb->indices[kb->ni++] = o;
          }
        }
        kb->ne++;
      }
      kb->nt = 2*(NUMV+1);
    }
    else /* kb->display_mode == DISP_WIREFRAME */
    {
      for (i=0; i<NUMU; i++)
      {
        if (kb->appearance == APPEARANCE_BANDS && ((i & (NUMB-1)) > NUMB/2))
          continue;
        if (kb->appearance == APPEARANCE_BANDS && ((i & (NUMB-1)) == NUMB/2))
        {
          for (j=0; j<NUMV; j++)
          {
            kb->indices[kb->ni++] = i*(NUMV+1)+j;
            kb->indices[kb->ni++] = i*(NUMV+1)+j+1;
          }
          continue;
        }
        for (j=0; j<=NUMV; j++)
        {
          kb->indices[kb->ni++] = i*(NUMV+1)+j;
          kb->indices[kb->ni++] = i*(NUMV+1)+j+1;
          if (i < NUMV)
          {
            kb->indices[kb->ni++] = i*(NUMV+1)+j;
            kb->indices[kb->ni++] = (i+1)*(NUMV+1)+j;
          }
        }
      }
      kb->ne = 1;
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,kb->indices_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,kb->ni*sizeof(GLuint),
                 kb->indices,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

    kb->buffers_initialized = True;
  }

  if (kb->change_colors)
    rotateall3d(kb->rho,kb->sigma,kb->tau,matc);

  if (kb->view == VIEW_WALK || kb->view == VIEW_WALKTURN)
  {
    compute_walk_frame_pinched_torus(kb,mat);
  }
  else
  {
    /* Compute the rotation that rotates the Klein bottle in 4D, including
       the trackball rotations. */
    rotateall(kb->alpha,kb->beta,kb->delta,kb->zeta,kb->eta,kb->theta,r1);

    gltrackball_get_quaternion(kb->trackballs[0],q1);
    gltrackball_get_quaternion(kb->trackballs[1],q2);
    quats_to_rotmat(q1,q2,r2);

    mult_rotmat(r2,r1,mat);
  }

  if (kb->change_colors &&
      (kb->colors == COLORS_RAINBOW || kb->colors == COLORS_DEPTH))
  {
    ur = umax-umin;
    vr = vmax-vmin;
    for (i=0; i<=NUMU; i++)
    {
      for (j=0; j<=NUMV; j++)
      {
        o = i*(NUMV+1)+j;
        v = vr*i/NUMV+vmin;
        if (kb->colors == COLORS_DEPTH)
        {
          u = -ur*j/NUMU+umin;
          color(kb,(sin(u)*sin(0.5*v)+1.0)*M_PI*2.0/3.0,matc,kb->col[o]);
        }
        else if (kb->colors == COLORS_RAINBOW)
        {
          color(kb,v,matc,kb->col[o]);
        }
      }
    }
  }

  glUseProgram(kb->shader_program);

  set_opengl_state_pf(mi,mat,matc);

  glEnableVertexAttribArray(kb->vertex_uv_index);
  glBindBuffer(GL_ARRAY_BUFFER,kb->vertex_uv_buffer);
  glVertexAttribPointer(kb->vertex_uv_index,2,GL_FLOAT,GL_FALSE,0,0);

  glEnableVertexAttribArray(kb->vertex_t_index);
  glBindBuffer(GL_ARRAY_BUFFER,kb->vertex_t_buffer);
  glVertexAttribPointer(kb->vertex_t_index,2,GL_FLOAT,GL_FALSE,0,0);

  if (kb->colors != COLORS_ONESIDED && kb->colors != COLORS_TWOSIDED)
  {
    glEnableVertexAttribArray(kb->color_index);
    glBindBuffer(GL_ARRAY_BUFFER,kb->color_buffer);
    if (kb->change_colors)
      glBufferData(GL_ARRAY_BUFFER,4*(NUMU+1)*(NUMV+1)*sizeof(GLfloat),
                   kb->col,GL_STREAM_DRAW);
    glVertexAttribPointer(kb->color_index,4,GL_FLOAT,GL_FALSE,0,0);
  }

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,kb->indices_buffer);

  if (kb->display_mode != DISP_WIREFRAME)
  {
    for (i=0; i<kb->ne; i++)
    {
      index_offset = kb->nt*i*sizeof(GLuint);
      glDrawElements(GL_TRIANGLE_STRIP,kb->nt,GL_UNSIGNED_INT,
                     (const GLvoid *)index_offset);
      polys = 2*kb->ne*NUMV;
    }
  }
  else /* kb->display_mode == DISP_WIREFRAME */
  {
    glLineWidth(1.0f);
    index_offset = 0;
    glDrawElements(GL_LINES,kb->ni,GL_UNSIGNED_INT,
                   (const void *)index_offset);
  }

  glDisableVertexAttribArray(kb->vertex_uv_index);
  glDisableVertexAttribArray(kb->vertex_t_index);
  if (kb->colors != COLORS_ONESIDED && kb->colors != COLORS_TWOSIDED)
    glDisableVertexAttribArray(kb->color_index);
  glBindBuffer(GL_ARRAY_BUFFER,0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

  glUseProgram(0);

  polys = 2*NUMU*NUMV;
  if (kb->appearance != APPEARANCE_SOLID)
    polys /= 2;
  return polys;
}

#endif /* HAVE_GLSL */


/* Draw a Lawson Klein bottle projected into 3D. */
static int lawson_ff(ModeInfo *mi, double umin, double umax, double vmin,
                     double vmax)
{
  int polys;
  float pu[3], pv[3], mat[4][4], matc[3][3];
  int i, j, k, l, m, o;
  double u, v, ur, vr;
  double y[4], yu[4], yv[4];
  double q, r, s, t;
  float q1[4], q2[4], r1[4][4], r2[4][4];
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  if (kb->change_colors)
    rotateall3d(kb->rho,kb->sigma,kb->tau,matc);

  set_opengl_state_ff(mi,matc);

  if (kb->view == VIEW_WALK || kb->view == VIEW_WALKTURN)
  {
    compute_walk_frame_lawson(kb,mat);
  }
  else
  {
    /* Compute the rotation that rotates the Klein bottle in 4D, including
       the trackball rotations. */
    rotateall(kb->alpha,kb->beta,kb->delta,kb->zeta,kb->eta,kb->theta,r1);

    gltrackball_get_quaternion(kb->trackballs[0],q1);
    gltrackball_get_quaternion(kb->trackballs[1],q2);
    quats_to_rotmat(q1,q2,r2);

    mult_rotmat(r2,r1,mat);
  }

  /* Project the points from 4D to 3D. */
  for (i=0; i<=NUMV; i++)
  {
    for (j=0; j<=NUMU; j++)
    {
      o = i*(NUMU+1)+j;
      for (l=0; l<4; l++)
      {
        y[l] = (mat[l][0]*kb->x[o][0]+mat[l][1]*kb->x[o][1]+
                mat[l][2]*kb->x[o][2]+mat[l][3]*kb->x[o][3]);
        yu[l] = (mat[l][0]*kb->xu[o][0]+mat[l][1]*kb->xu[o][1]+
                 mat[l][2]*kb->xu[o][2]+mat[l][3]*kb->xu[o][3]);
        yv[l] = (mat[l][0]*kb->xv[o][0]+mat[l][1]*kb->xv[o][1]+
                 mat[l][2]*kb->xv[o][2]+mat[l][3]*kb->xv[o][3]);
      }
      if (kb->projection_4d == DISP_4D_ORTHOGRAPHIC)
      {
        for (l=0; l<3; l++)
        {
          kb->pp[o][l] = (y[l]+kb->offset4d[l])+kb->offset3d[l];
          pu[l] = yu[l];
          pv[l] = yv[l];
        }
      }
      else
      {
        s = y[3]+kb->offset4d[3];
        q = 1.0/s;
        t = q*q;
        for (l=0; l<3; l++)
        {
          r = y[l]+kb->offset4d[l];
          kb->pp[o][l] = r*q+kb->offset3d[l];
          pu[l] = (yu[l]*s-r*yu[3])*t;
          pv[l] = (yv[l]*s-r*yv[3])*t;
        }
      }
      kb->pn[o][0] = pu[1]*pv[2]-pu[2]*pv[1];
      kb->pn[o][1] = pu[2]*pv[0]-pu[0]*pv[2];
      kb->pn[o][2] = pu[0]*pv[1]-pu[1]*pv[0];
      t = 1.0/sqrt(kb->pn[o][0]*kb->pn[o][0]+kb->pn[o][1]*kb->pn[o][1]+
                   kb->pn[o][2]*kb->pn[o][2]);
      kb->pn[o][0] *= t;
      kb->pn[o][1] *= t;
      kb->pn[o][2] *= t;
    }
  }

  ur = umax-umin;
  vr = vmax-vmin;
  for (i=0; i<NUMV; i++)
  {
    if (kb->appearance == APPEARANCE_BANDS && ((i & (NUMB-1)) >= NUMB/2))
      continue;
    if (kb->display_mode == DISP_WIREFRAME)
      glBegin(GL_QUAD_STRIP);
    else
      glBegin(GL_TRIANGLE_STRIP);
    for (j=0; j<=NUMU; j++)
    {
      for (k=0; k<=1; k++)
      {
        l = i+k;
        m = j;
        o = l*(NUMU+1)+m;
        glNormal3fv(kb->pn[o]);
        glTexCoord2fv(kb->tex[o]);
        if (kb->change_colors)
        {
          v = vr*l/NUMV+vmin;
          if (kb->colors == COLORS_DEPTH)
          {
            u = -ur*m/NUMU+umin;
            color(kb,(sin(u)*cos(0.5*v)+1.0)*M_PI*2.0/3.0,matc,kb->col[o]);
          }
          else if (kb->colors == COLORS_RAINBOW)
          {
            color(kb,v,matc,kb->col[o]);
          }
        }
        if (kb->colors != COLORS_ONESIDED && kb->colors != COLORS_TWOSIDED)
        {
          glColor3fv(kb->col[o]);
          glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,kb->col[o]);
        }
        glVertex3fv(kb->pp[o]);
      }
    }
    glEnd();
  }

  polys = 2*NUMU*NUMV;
  if (kb->appearance != APPEARANCE_SOLID)
    polys /= 2;
  return polys;
}


#ifdef HAVE_GLSL

/* Draw a Lawson Klein bottle projected into 3D. */
static int lawson_pf(ModeInfo *mi, double umin, double umax, double vmin,
                     double vmax)
{
  int polys;
  float mat[4][4], matc[3][3];
  int i, j, k, l, m, o;
  double u, v, ur, vr;
  float q1[4], q2[4], r1[4][4], r2[4][4];
  GLsizeiptr index_offset;
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  if (!kb->use_shaders)
    return 0;

  if (!kb->buffers_initialized)
  {
    /* The u and v values need to be computed once (or each time the value
       of appearance changes, once we support that). */
    ur = umax-umin;
    vr = vmax-vmin;
    for (i=0; i<=NUMV; i++)
    {
      for (j=0; j<=NUMU; j++)
      {
        o = i*(NUMU+1)+j;
        u = -ur*j/NUMU+umin;
        v = vr*i/NUMV+vmin;
        kb->uv[o][0] = u;
        kb->uv[o][1] = v;
      }
    }
    glBindBuffer(GL_ARRAY_BUFFER,kb->vertex_uv_buffer);
    glBufferData(GL_ARRAY_BUFFER,2*(NUMU+1)*(NUMV+1)*sizeof(GLfloat),
                 kb->uv,GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);

    glBindBuffer(GL_ARRAY_BUFFER,kb->vertex_t_buffer);
    glBufferData(GL_ARRAY_BUFFER,2*(NUMU+1)*(NUMV+1)*sizeof(GLfloat),
                 kb->tex,GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);

    if (!kb->change_colors &&
        kb->colors != COLORS_ONESIDED && kb->colors != COLORS_TWOSIDED)
    {
      glBindBuffer(GL_ARRAY_BUFFER,kb->color_buffer);
      glBufferData(GL_ARRAY_BUFFER,4*(NUMU+1)*(NUMV+1)*sizeof(GLfloat),
                   kb->col,GL_STATIC_DRAW);
      glBindBuffer(GL_ARRAY_BUFFER,0);
    }

    /* The indices only need to be computed once (or each time the value of
       appearance changes, once we support that). */
    kb->ni = 0;
    kb->ne = 0;
    kb->nt = 0;
    if (kb->display_mode != DISP_WIREFRAME)
    {
      for (i=0; i<NUMV; i++)
      {
        if (kb->appearance == APPEARANCE_BANDS && ((i & (NUMB-1)) >= NUMB/2))
          continue;
        for (j=0; j<=NUMU; j++)
        {
          for (k=0; k<=1; k++)
          {
            l = i+k;
            m = j;
            o = l*(NUMU+1)+m;
            kb->indices[kb->ni++] = o;
          }
        }
        kb->ne++;
      }
      kb->nt = 2*(NUMU+1);
    }
    else /* kb->display_mode == DISP_WIREFRAME */
    {
      for (i=0; i<NUMV; i++)
      {
        if (kb->appearance == APPEARANCE_BANDS && ((i & (NUMB-1)) > NUMB/2))
          continue;
        if (kb->appearance == APPEARANCE_BANDS && ((i & (NUMB-1)) == NUMB/2))
        {
          for (j=0; j<NUMU; j++)
          {
            kb->indices[kb->ni++] = i*(NUMU+1)+j;
            kb->indices[kb->ni++] = i*(NUMU+1)+j+1;
          }
          continue;
        }
        for (j=0; j<=NUMV; j++)
        {
          kb->indices[kb->ni++] = i*(NUMU+1)+j;
          kb->indices[kb->ni++] = i*(NUMU+1)+j+1;
          if (i < NUMV)
          {
            kb->indices[kb->ni++] = i*(NUMU+1)+j;
            kb->indices[kb->ni++] = (i+1)*(NUMU+1)+j;
          }
        }
      }
      kb->ne = 1;
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,kb->indices_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,kb->ni*sizeof(GLuint),
                 kb->indices,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

    kb->buffers_initialized = True;
  }

  if (kb->change_colors)
    rotateall3d(kb->rho,kb->sigma,kb->tau,matc);

  set_opengl_state_pf(mi,mat,matc);

  if (kb->view == VIEW_WALK || kb->view == VIEW_WALKTURN)
  {
    compute_walk_frame_lawson(kb,mat);
  }
  else
  {
    /* Compute the rotation that rotates the Klein bottle in 4D, including
       the trackball rotations. */
    rotateall(kb->alpha,kb->beta,kb->delta,kb->zeta,kb->eta,kb->theta,r1);

    gltrackball_get_quaternion(kb->trackballs[0],q1);
    gltrackball_get_quaternion(kb->trackballs[1],q2);
    quats_to_rotmat(q1,q2,r2);

    mult_rotmat(r2,r1,mat);
  }

  if (kb->change_colors &&
      (kb->colors == COLORS_RAINBOW || kb->colors == COLORS_DEPTH))
  {
    ur = umax-umin;
    vr = vmax-vmin;
    for (i=0; i<=NUMV; i++)
    {
      for (j=0; j<=NUMU; j++)
      {
        o = i*(NUMV+1)+j;
        v = vr*i/NUMV+vmin;
        if (kb->colors == COLORS_DEPTH)
        {
          u = -ur*j/NUMU+umin;
          color(kb,(sin(u)*cos(0.5*v)+1.0)*M_PI*2.0/3.0,matc,kb->col[o]);
        }
        else if (kb->colors == COLORS_RAINBOW)
        {
          color(kb,v,matc,kb->col[o]);
        }
      }
    }
  }

  glUseProgram(kb->shader_program);

  set_opengl_state_pf(mi,mat,matc);

  glEnableVertexAttribArray(kb->vertex_uv_index);
  glBindBuffer(GL_ARRAY_BUFFER,kb->vertex_uv_buffer);
  glVertexAttribPointer(kb->vertex_uv_index,2,GL_FLOAT,GL_FALSE,0,0);

  glEnableVertexAttribArray(kb->vertex_t_index);
  glBindBuffer(GL_ARRAY_BUFFER,kb->vertex_t_buffer);
  glVertexAttribPointer(kb->vertex_t_index,2,GL_FLOAT,GL_FALSE,0,0);

  if (kb->colors != COLORS_ONESIDED && kb->colors != COLORS_TWOSIDED)
  {
    glEnableVertexAttribArray(kb->color_index);
    glBindBuffer(GL_ARRAY_BUFFER,kb->color_buffer);
    if (kb->change_colors)
      glBufferData(GL_ARRAY_BUFFER,4*(NUMU+1)*(NUMV+1)*sizeof(GLfloat),
                   kb->col,GL_STREAM_DRAW);
    glVertexAttribPointer(kb->color_index,4,GL_FLOAT,GL_FALSE,0,0);
  }

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,kb->indices_buffer);

  if (kb->display_mode != DISP_WIREFRAME)
  {
    for (i=0; i<kb->ne; i++)
    {
      index_offset = kb->nt*i*sizeof(GLuint);
      glDrawElements(GL_TRIANGLE_STRIP,kb->nt,GL_UNSIGNED_INT,
                     (const GLvoid *)index_offset);
      polys = 2*kb->ne*NUMV;
    }
  }
  else /* kb->display_mode == DISP_WIREFRAME */
  {
    glLineWidth(1.0f);
    index_offset = 0;
    glDrawElements(GL_LINES,kb->ni,GL_UNSIGNED_INT,
                   (const void *)index_offset);
  }

  glDisableVertexAttribArray(kb->vertex_uv_index);
  glDisableVertexAttribArray(kb->vertex_t_index);
  if (kb->colors != COLORS_ONESIDED && kb->colors != COLORS_TWOSIDED)
    glDisableVertexAttribArray(kb->color_index);
  glBindBuffer(GL_ARRAY_BUFFER,0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

  glUseProgram(0);

  polys = 2*NUMU*NUMV;
  if (kb->appearance != APPEARANCE_SOLID)
    polys /= 2;
  return polys;
}

#endif /* HAVE_GLSL */


/* Generate a texture image that shows the orientation reversal. */
static void gen_texture(ModeInfo *mi)
{
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  glPixelStorei(GL_UNPACK_ALIGNMENT,1);
  glGenTextures(1,&kb->tex_name);
  glBindTexture(GL_TEXTURE_2D,kb->tex_name);
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
  kleinstruct *kb = &klein[MI_SCREEN(mi)];
  GLint gl_major, gl_minor, glsl_major, glsl_minor;
  GLboolean gl_gles3;
  const GLchar *vertex_shader_source[3];
  const GLchar *fragment_shader_source[4];

  /* Determine whether to use shaders to render the Klein bottle. */
  kb->use_shaders = False;
  kb->buffers_initialized = False;
  kb->shader_program = 0;
  kb->ni = 0;
  kb->ne = 0;
  kb->nt = 0;

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
                                  &kb->shader_program))
    return;
  kb->vertex_uv_index = glGetAttribLocation(kb->shader_program,"VertexUV");
  kb->vertex_t_index = glGetAttribLocation(kb->shader_program,"VertexT");
  kb->color_index = glGetAttribLocation(kb->shader_program,"VertexColor");
  if (kb->vertex_uv_index == -1 || kb->vertex_t_index == -1 ||
      kb->color_index == -1)
  {
    glDeleteProgram(kb->shader_program);
    return;
  }
  kb->mat_rot_index = glGetUniformLocation(kb->shader_program,
                                           "MatRot4D");
  kb->mat_p_index = glGetUniformLocation(kb->shader_program,
                                         "MatProj");
  kb->bool_persp_index = glGetUniformLocation(kb->shader_program,
                                              "BoolPersp");
  kb->off4d_index = glGetUniformLocation(kb->shader_program,
                                         "Offset4D");
  kb->off3d_index = glGetUniformLocation(kb->shader_program,
                                         "Offset3D");
  kb->bool_textures_index = glGetUniformLocation(kb->shader_program,
                                                "BoolTextures");
  kb->bottle_type_index = glGetUniformLocation(kb->shader_program,
                                                "BottleType");
  kb->draw_lines_index = glGetUniformLocation(kb->shader_program,
                                              "DrawLines");
  kb->glbl_ambient_index = glGetUniformLocation(kb->shader_program,
                                                "LtGlblAmbient");
  kb->lt_ambient_index = glGetUniformLocation(kb->shader_program,
                                              "LtAmbient");
  kb->lt_diffuse_index = glGetUniformLocation(kb->shader_program,
                                              "LtDiffuse");
  kb->lt_specular_index = glGetUniformLocation(kb->shader_program,
                                               "LtSpecular");
  kb->lt_direction_index = glGetUniformLocation(kb->shader_program,
                                                "LtDirection");
  kb->lt_halfvect_index = glGetUniformLocation(kb->shader_program,
                                               "LtHalfVector");
  kb->front_ambient_index = glGetUniformLocation(kb->shader_program,
                                                 "MatFrontAmbient");
  kb->back_ambient_index = glGetUniformLocation(kb->shader_program,
                                                "MatBackAmbient");
  kb->front_diffuse_index = glGetUniformLocation(kb->shader_program,
                                                 "MatFrontDiffuse");
  kb->back_diffuse_index = glGetUniformLocation(kb->shader_program,
                                                "MatBackDiffuse");
  kb->specular_index = glGetUniformLocation(kb->shader_program,
                                            "MatSpecular");
  kb->shininess_index = glGetUniformLocation(kb->shader_program,
                                             "MatShininess");
  kb->texture_sampler_index = glGetUniformLocation(kb->shader_program,
                                                   "TextureSampler");
  if (kb->mat_rot_index == -1 ||kb->mat_p_index == -1 ||
      kb->bool_persp_index == -1 || kb->off4d_index == -1 ||
      kb->off3d_index == -1 || kb->bool_textures_index == -1 ||
      kb->bottle_type_index == -1 || kb->draw_lines_index == -1 ||
      kb->glbl_ambient_index == -1 || kb->lt_ambient_index == -1 ||
      kb->lt_diffuse_index == -1 || kb->lt_specular_index == -1 ||
      kb->lt_direction_index == -1 || kb->lt_halfvect_index == -1 ||
      kb->front_ambient_index == -1 || kb->back_ambient_index == -1 ||
      kb->front_diffuse_index == -1 || kb->back_diffuse_index == -1 ||
      kb->specular_index == -1 || kb->shininess_index == -1 ||
      kb->texture_sampler_index == -1)
  {
    glDeleteProgram(kb->shader_program);
    return;
  }

  glGenBuffers(1,&kb->vertex_uv_buffer);
  glGenBuffers(1,&kb->vertex_t_buffer);
  glGenBuffers(1,&kb->color_buffer);
  glGenBuffers(1,&kb->indices_buffer);

  kb->use_shaders = True;
}

#endif /* HAVE_GLSL */


static void init(ModeInfo *mi)
{
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  if (walk_speed == 0.0)
    walk_speed = 20.0;

  if (kb->view == VIEW_TURN)
  {
    kb->alpha = frand(360.0);
    kb->beta = frand(360.0);
    kb->delta = frand(360.0);
  }
  else
  {
    kb->alpha = 0.0;
    kb->beta = 0.0;
    kb->delta = 0.0;
  }
  kb->zeta = 0.0;
  if (kb->bottle_type == KLEIN_BOTTLE_FIGURE_8 ||
      kb->bottle_type == KLEIN_BOTTLE_PINCHED_TORUS)
    kb->eta = 0.0;
  else
    kb->eta = 45.0;
  kb->theta = 0.0;
  kb->umove = frand(2.0*M_PI);
  kb->vmove = frand(2.0*M_PI);
  kb->dumove = 0.0;
  kb->dvmove = 0.0;
  kb->side = 1;

  kb->rho = frand(360.0);
  kb->sigma = frand(360.0);
  kb->tau = frand(360.0);

  if (kb->bottle_type == KLEIN_BOTTLE_FIGURE_8)
  {
    kb->offset4d[0] = 0.0;
    kb->offset4d[1] = 0.0;
    kb->offset4d[2] = 0.0;
    kb->offset4d[3] = 1.5;
    kb->offset3d[0] = 0.0;
    kb->offset3d[1] = 0.0;
    if (kb->projection_4d == DISP_4D_ORTHOGRAPHIC)
      kb->offset3d[2] = -2.1;
    else
      kb->offset3d[2] = -1.9;
    kb->offset3d[3] = 0.0;
  }
  else if (kb->bottle_type == KLEIN_BOTTLE_PINCHED_TORUS)
  {
    kb->offset4d[0] = 0.0;
    kb->offset4d[1] = 0.0;
    kb->offset4d[2] = 0.0;
    kb->offset4d[3] = 1.4;
    kb->offset3d[0] = 0.0;
    kb->offset3d[1] = 0.0;
    kb->offset3d[2] = -2.0;
    kb->offset3d[3] = 0.0;
  }
  else /* kb->bottle_type == KLEIN_BOTTLE_LAWSON */
  {
    kb->offset4d[0] = 0.0;
    kb->offset4d[1] = 0.0;
    kb->offset4d[2] = 0.0;
    if (kb->projection_4d == DISP_4D_PERSPECTIVE &&
        kb->projection_3d == DISP_3D_ORTHOGRAPHIC)
      kb->offset4d[3] = 1.5;
    else
      kb->offset4d[3] = 1.1;
    kb->offset3d[0] = 0.0;
    kb->offset3d[1] = 0.0;
    if (kb->projection_4d == DISP_4D_ORTHOGRAPHIC)
      kb->offset3d[2] = -2.0;
    else
      kb->offset3d[2] = -5.0;
    kb->offset3d[3] = 0.0;
  }

  gen_texture(mi);
  if (kb->bottle_type == KLEIN_BOTTLE_FIGURE_8)
    setup_figure8(mi,0.0,2.0*M_PI,0.0,2.0*M_PI);
  else if (kb->bottle_type == KLEIN_BOTTLE_PINCHED_TORUS)
    setup_pinched_torus(mi,0.0,2.0*M_PI,0.0,2.0*M_PI);
  else /* kb->bottle_type == KLEIN_BOTTLE_LAWSON */
    setup_lawson(mi,0.0,2.0*M_PI,0.0,2.0*M_PI);

#ifdef HAVE_GLSL
  init_glsl(mi);
#endif /* HAVE_GLSL */

#ifdef HAVE_ANDROID
  /* glPolygonMode(...,GL_LINE) is not supported for an OpenGL ES 1.1
     context. */
  if (!kb->use_shaders && kb->display_mode == DISP_WIREFRAME)
    kb->display_mode = DISP_SURFACE;
#endif /* HAVE_ANDROID */
}


/* Redisplay the Klein bottle. */
static void display_klein(ModeInfo *mi)
{
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  if (!kb->button_pressed)
  {
    if (kb->view == VIEW_TURN)
    {
      kb->alpha += speed_wx * kb->speed_scale;
      if (kb->alpha >= 360.0)
        kb->alpha -= 360.0;
      kb->beta += speed_wy * kb->speed_scale;
      if (kb->beta >= 360.0)
        kb->beta -= 360.0;
      kb->delta += speed_wz * kb->speed_scale;
      if (kb->delta >= 360.0)
        kb->delta -= 360.0;
      kb->zeta += speed_xy * kb->speed_scale;
      if (kb->zeta >= 360.0)
        kb->zeta -= 360.0;
      kb->eta += speed_xz * kb->speed_scale;
      if (kb->eta >= 360.0)
        kb->eta -= 360.0;
      kb->theta += speed_yz * kb->speed_scale;
      if (kb->theta >= 360.0)
        kb->theta -= 360.0;
    }
    if (kb->view == VIEW_WALKTURN)
    {
      kb->zeta += speed_xy * kb->speed_scale;
      if (kb->zeta >= 360.0)
        kb->zeta -= 360.0;
      kb->eta += speed_xz * kb->speed_scale;
      if (kb->eta >= 360.0)
        kb->eta -= 360.0;
      kb->theta += speed_yz * kb->speed_scale;
      if (kb->theta >= 360.0)
        kb->theta -= 360.0;
    }
    if (kb->view == VIEW_WALK || kb->view == VIEW_WALKTURN)
    {
      kb->dvmove = cos(walk_direction*M_PI/180.0)*walk_speed*M_PI/4096.0;
      kb->vmove += kb->dvmove;
      if (kb->vmove >= 2.0*M_PI)
      {
        kb->vmove -= 2.0*M_PI;
        kb->umove = 2.0*M_PI-kb->umove;
        kb->side = -kb->side;
      }
      kb->dumove = (kb->side*sin(walk_direction*M_PI/180.0)*
                    walk_speed*M_PI/4096.0);
      kb->umove += kb->dumove;
      if (kb->umove >= 2.0*M_PI)
        kb->umove -= 2.0*M_PI;
      if (kb->umove < 0.0)
        kb->umove += 2.0*M_PI;
    }
    if (kb->change_colors)
    {
      kb->rho += DRHO;
      if (kb->rho >= 360.0)
        kb->rho -= 360.0;
      kb->sigma += DSIGMA;
      if (kb->sigma >= 360.0)
        kb->sigma -= 360.0;
      kb->tau += DTAU;
      if (kb->tau >= 360.0)
        kb->tau -= 360.0;
    }
  }

  gltrackball_rotate(kb->trackballs[kb->current_trackball]);
  if (kb->bottle_type == KLEIN_BOTTLE_FIGURE_8)
  {
#ifdef HAVE_GLSL
    if (kb->use_shaders)
      mi->polygon_count = figure8_pf(mi,0.0,2.0*M_PI,0.0,2.0*M_PI);
    else
#endif /* HAVE_GLSL */
      mi->polygon_count = figure8_ff(mi,0.0,2.0*M_PI,0.0,2.0*M_PI);
  }
  else if (kb->bottle_type == KLEIN_BOTTLE_PINCHED_TORUS)
  {
#ifdef HAVE_GLSL
    if (kb->use_shaders)
      mi->polygon_count = pinched_torus_pf(mi,0.0,2.0*M_PI,0.0,2.0*M_PI);
    else
#endif /* HAVE_GLSL */
      mi->polygon_count = pinched_torus_ff(mi,0.0,2.0*M_PI,0.0,2.0*M_PI);
  }
  else /* kb->bottle_type == KLEIN_BOTTLE_LAWSON */
  {
#ifdef HAVE_GLSL
    if (kb->use_shaders)
      mi->polygon_count = lawson_pf(mi,0.0,2.0*M_PI,0.0,2.0*M_PI);
    else
#endif /* HAVE_GLSL */
      mi->polygon_count = lawson_ff(mi,0.0,2.0*M_PI,0.0,2.0*M_PI);
  }
}


ENTRYPOINT void reshape_klein(ModeInfo *mi, int width, int height)
{
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  kb->WindW = (GLint)width;
  kb->WindH = (GLint)height;
  glViewport(0,0,width,height);
  kb->aspect = (GLfloat)width/(GLfloat)height;
}


ENTRYPOINT Bool klein_handle_event(ModeInfo *mi, XEvent *event)
{
  kleinstruct *kb = &klein[MI_SCREEN(mi)];
  KeySym  sym = 0;
  char c = 0;

  if (event->xany.type == KeyPress || event->xany.type == KeyRelease)
    XLookupString (&event->xkey, &c, 1, &sym, 0);

  if (event->xany.type == ButtonPress &&
      event->xbutton.button == Button1)
  {
    kb->button_pressed = True;
    gltrackball_start(kb->trackballs[kb->current_trackball],
                      event->xbutton.x, event->xbutton.y,
                      MI_WIDTH(mi), MI_HEIGHT(mi));
    return True;
  }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
  {
    kb->button_pressed = False;
    gltrackball_stop(kb->trackballs[kb->current_trackball]);
    return True;
  }
  else if (event->xany.type == KeyPress)
  {
    if (sym == XK_Shift_L || sym == XK_Shift_R)
    {
      kb->current_trackball = 1;
      if (kb->button_pressed)
        gltrackball_start(kb->trackballs[kb->current_trackball],
                          event->xbutton.x, event->xbutton.y,
                          MI_WIDTH(mi), MI_HEIGHT(mi));
      return True;
    }
  }
  else if (event->xany.type == KeyRelease)
  {
    if (sym == XK_Shift_L || sym == XK_Shift_R)
    {
      kb->current_trackball = 0;
      if (kb->button_pressed)
        gltrackball_start(kb->trackballs[kb->current_trackball],
                          event->xbutton.x, event->xbutton.y,
                          MI_WIDTH(mi), MI_HEIGHT(mi));
      return True;
    }
  }
  else if (event->xany.type == MotionNotify && kb->button_pressed)
  {
    gltrackball_track(kb->trackballs[kb->current_trackball],
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
 *    Initialize klein.  Called each time the window changes.
 *-----------------------------------------------------------------------------
 */

ENTRYPOINT void init_klein(ModeInfo *mi)
{
  kleinstruct *kb;

  MI_INIT(mi, klein);
  kb = &klein[MI_SCREEN(mi)];

  
  kb->trackballs[0] = gltrackball_init(False);
  kb->trackballs[1] = gltrackball_init(False);
  kb->current_trackball = 0;
  kb->button_pressed = False;

  /* Set the Klein bottle. */
  if (!strcasecmp(klein_bottle,"random"))
  {
    kb->bottle_type = random() % NUM_KLEIN_BOTTLES;
  }
  else if (!strcasecmp(klein_bottle,"figure-8"))
  {
    kb->bottle_type = KLEIN_BOTTLE_FIGURE_8;
  }
  else if (!strcasecmp(klein_bottle,"pinched-torus"))
  {
    kb->bottle_type = KLEIN_BOTTLE_PINCHED_TORUS;
  }
  else if (!strcasecmp(klein_bottle,"lawson"))
  {
    kb->bottle_type = KLEIN_BOTTLE_LAWSON;
  }
  else
  {
    kb->bottle_type = random() % NUM_KLEIN_BOTTLES;
  }

  /* Set the display mode. */
  if (!strcasecmp(mode,"random"))
  {
    kb->display_mode = random() % NUM_DISPLAY_MODES;
  }
  else if (!strcasecmp(mode,"wireframe"))
  {
    kb->display_mode = DISP_WIREFRAME;
  }
  else if (!strcasecmp(mode,"surface"))
  {
    kb->display_mode = DISP_SURFACE;
  }
  else if (!strcasecmp(mode,"transparent"))
  {
    kb->display_mode = DISP_TRANSPARENT;
  }
  else
  {
    kb->display_mode = random() % NUM_DISPLAY_MODES;
  }

  /* Orientation marks don't make sense in wireframe mode. */
  if (kb->display_mode == DISP_WIREFRAME)
    marks = False;

  /* Set the appearance. */
  if (!strcasecmp(appear,"random"))
  {
    kb->appearance = random() % NUM_APPEARANCES;
  }
  else if (!strcasecmp(appear,"solid"))
  {
    kb->appearance = APPEARANCE_SOLID;
  }
  else if (!strcasecmp(appear,"bands"))
  {
    kb->appearance = APPEARANCE_BANDS;
  }
  else
  {
    kb->appearance = random() % NUM_APPEARANCES;
  }

  /* Set the color mode. */
  if (!strcasecmp(color_mode,"random"))
  {
    kb->colors = random() % NUM_COLORS;
  }
  else if (!strcasecmp(color_mode,"one-sided"))
  {
    kb->colors = COLORS_ONESIDED;
  }
  else if (!strcasecmp(color_mode,"two-sided"))
  {
    kb->colors = COLORS_TWOSIDED;
  }
  else if (!strcasecmp(color_mode,"rainbow"))
  {
    kb->colors = COLORS_RAINBOW;
  }
  else if (!strcasecmp(color_mode,"depth"))
  {
    kb->colors = COLORS_DEPTH;
  }
  else
  {
    kb->colors = random() % NUM_COLORS;
  }

  kb->change_colors = change_colors;

  /* Set the view mode. */
  if (!strcasecmp(view_mode,"random"))
  {
    kb->view = random() % NUM_VIEW_MODES;
  }
  else if (!strcasecmp(view_mode,"walk"))
  {
    kb->view = VIEW_WALK;
  }
  else if (!strcasecmp(view_mode,"turn"))
  {
    kb->view = VIEW_TURN;
  }
  else if (!strcasecmp(view_mode,"walk-turn"))
  {
    kb->view = VIEW_WALKTURN;
  }
  else
  {
    kb->view = random() % NUM_VIEW_MODES;
  }

  /* Set the 3d projection mode. */
  if (!strcasecmp(proj_3d,"random"))
  {
    /* Orthographic projection only makes sense in turn mode. */
    if (kb->view == VIEW_TURN)
      kb->projection_3d = random() % NUM_DISP_3D_MODES;
    else
      kb->projection_3d = DISP_3D_PERSPECTIVE;
  }
  else if (!strcasecmp(proj_3d,"perspective"))
  {
    kb->projection_3d = DISP_3D_PERSPECTIVE;
  }
  else if (!strcasecmp(proj_3d,"orthographic"))
  {
    kb->projection_3d = DISP_3D_ORTHOGRAPHIC;
  }
  else
  {
    /* Orthographic projection only makes sense in turn mode. */
    if (kb->view == VIEW_TURN)
      kb->projection_3d = random() % NUM_DISP_3D_MODES;
    else
      kb->projection_3d = DISP_3D_PERSPECTIVE;
  }

  /* Set the 4d projection mode. */
  if (!strcasecmp(proj_4d,"random"))
  {
    kb->projection_4d = random() % NUM_DISP_4D_MODES;
  }
  else if (!strcasecmp(proj_4d,"perspective"))
  {
    kb->projection_4d = DISP_4D_PERSPECTIVE;
  }
  else if (!strcasecmp(proj_4d,"orthographic"))
  {
    kb->projection_4d = DISP_4D_ORTHOGRAPHIC;
  }
  else
  {
    kb->projection_4d = random() % NUM_DISP_4D_MODES;
  }

  /* Modify the speeds to a useful range in walk-and-turn mode. */
  if (kb->view == VIEW_WALKTURN)
  {
    speed_wx *= 0.2;
    speed_wy *= 0.2;
    speed_wz *= 0.2;
    speed_xy *= 0.2;
    speed_xz *= 0.2;
    speed_yz *= 0.2;
  }

  /* make multiple screens rotate at slightly different rates. */
  kb->speed_scale = 0.9 + frand(0.3);

  if ((kb->glx_context = init_GL(mi)) != NULL)
  {
    reshape_klein(mi,MI_WIDTH(mi),MI_HEIGHT(mi));
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
ENTRYPOINT void draw_klein(ModeInfo *mi)
{
  Display          *display = MI_DISPLAY(mi);
  Window           window = MI_WINDOW(mi);
  kleinstruct *kb;

  if (klein == NULL)
    return;
  kb = &klein[MI_SCREEN(mi)];

  MI_IS_DRAWN(mi) = True;
  if (!kb->glx_context)
    return;

  glXMakeCurrent(display, window, *kb->glx_context);

  glClearColor(0.0f,0.0f,0.0f,1.0f);
  glClearDepth(1.0f);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();

  display_klein(mi);

  if (MI_IS_FPS(mi))
    do_fps (mi);

  glFlush();

  glXSwapBuffers(display,window);
}


#ifndef STANDALONE
ENTRYPOINT void change_klein(ModeInfo *mi)
{
  kleinstruct *kb = &klein[MI_SCREEN(mi)];

  if (!kb->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *kb->glx_context);
  init(mi);
}
#endif /* !STANDALONE */


ENTRYPOINT void free_klein(ModeInfo *mi)
{
  kleinstruct *kb = &klein[MI_SCREEN(mi)];
  if (!kb->glx_context) return;
  glXMakeCurrent (MI_DISPLAY(mi), MI_WINDOW(mi), *kb->glx_context);
  gltrackball_free (kb->trackballs[0]);
  gltrackball_free (kb->trackballs[1]);
  if (kb->tex_name) glDeleteTextures (1, &kb->tex_name);
#ifdef HAVE_GLSL
  if (kb->use_shaders)
  {
    glDeleteBuffers(1,&kb->vertex_uv_buffer);
    glDeleteBuffers(1,&kb->vertex_t_buffer);
    glDeleteBuffers(1,&kb->color_buffer);
    glDeleteBuffers(1,&kb->indices_buffer);
    if (kb->shader_program != 0)
    {
      glUseProgram(0);
      glDeleteProgram(kb->shader_program);
    }
  }
#endif /* HAVE_GLSL */
}


XSCREENSAVER_MODULE ("Klein", klein)

#endif /* USE_GL */
