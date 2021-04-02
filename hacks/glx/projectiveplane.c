/* projectiveplane --- Shows a 4d embedding of the real projective plane
   that rotates in 4d or on which you can walk */

#if 0
static const char sccsid[] = "@(#)projectiveplane.c  1.1 14/01/03 xlockmore";
#endif

/* Copyright (c) 2013-2021 Carsten Steger <carsten@mirsanmir.org>. */

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
 * C. Steger - 14/01/03: Initial version
 * C. Steger - 14/10/03: Moved the curlicue texture to curlicue.h
 * C. Steger - 20/01/06: Added the changing colors mode
 * C. Steger - 20/12/05: Added per-fragment shading
 * C. Steger - 20/12/06: Moved all GLSL support code into glsl-utils.[hc]
 * C. Steger - 20/12/30: Make the shader code work under macOS and iOS
 */

/*
 * This program shows a 4d embedding of the real projective plane.
 * You can walk on the projective plane, see it turn in 4d, or walk on
 * it while it turns in 4d.  The fact that the surface is an embedding
 * of the real projective plane in 4d can be seen in the depth colors
 * mode (using static colors): set all rotation speeds to 0 and the
 * projection mode to 4d orthographic projection.  In its default
 * orientation, the embedding of the real projective plane will then
 * project to the Roman surface, which has three lines of
 * self-intersection.  However, at the three lines of
 * self-intersection the parts of the surface that intersect have
 * different colors, i.e., different 4d depths.
 *
 * The real projective plane is a non-orientable surface.  To make
 * this apparent, the two-sided color mode can be used.
 * Alternatively, orientation markers (curling arrows) can be drawn as
 * a texture map on the surface of the projective plane.  While
 * walking on the projective plane, you will notice that the
 * orientation of the curling arrows changes (which it must because
 * the projective plane is non-orientable).
 *
 * The real projective plane is a model for the projective geometry in
 * 2d space.  One point can be singled out as the origin.  A line can
 * be singled out as the line at infinity, i.e., a line that lies at
 * an infinite distance to the origin.  The line at infinity, like all
 * lines in the projective plane, is topologically a circle.  Points
 * on the line at infinity are also used to model directions in
 * projective geometry.  The origin can be visualized in different
 * manners.  When using distance colors (and using static colors), the
 * origin is the point that is displayed as fully saturated red, which
 * is easier to see as the center of the reddish area on the
 * projective plane.  Alternatively, when using distance bands, the
 * origin is the center of the only band that projects to a disk.
 * When using direction bands, the origin is the point where all
 * direction bands collapse to a point.  Finally, when orientation
 * markers are being displayed, the origin the the point where all
 * orientation markers are compressed to a point.  The line at
 * infinity can also be visualized in different ways.  When using
 * distance colors (and using static colors), the line at infinity is
 * the line that is displayed as fully saturated magenta.  When
 * two-sided (and static) colors are used, the line at infinity lies
 * at the points where the red and green "sides" of the projective
 * plane meet (of course, the real projective plane only has one side,
 * so this is a design choice of the visualization).  Alternatively,
 * when orientation markers are being displayed, the line at infinity
 * is the place where the orientation markers change their
 * orientation.
 *
 * Note that when the projective plane is displayed with bands, the
 * orientation markers are placed in the middle of the bands.  For
 * distance bands, the bands are chosen in such a way that the band at
 * the origin is only half as wide as the remaining bands, which
 * results in a disk being displayed at the origin that has the same
 * diameter as the remaining bands.  This choice, however, also
 * implies that the band at infinity is half as wide as the other
 * bands.  Since the projective plane is attached to itself (in a
 * complicated fashion) at the line at infinity, effectively the band
 * at infinity is again as wide as the remaining bands.  However,
 * since the orientation markers are displayed in the middle of the
 * bands, this means that only one half of the orientation markers
 * will be displayed twice at the line at infinity if distance bands
 * are used.  If direction bands are used or if the projective plane
 * is displayed as a solid surface, the orientation markers are
 * displayed fully at the respective sides of the line at infinity.
 *
 * The program projects the 4d projective plane to 3d using either a
 * perspective or an orthographic projection.  Which of the two
 * alternatives looks more appealing is up to you.  However, two
 * famous surfaces are obtained if orthographic 4d projection is used:
 * The Roman surface and the cross cap.  If the projective plane is
 * rotated in 4d, the result of the projection for certain rotations
 * is a Roman surface and for certain rotations it is a cross cap.
 * The easiest way to see this is to set all rotation speeds to 0 and
 * the rotation speed around the yz plane to a value different from 0.
 * However, for any 4d rotation speeds, the projections will generally
 * cycle between the Roman surface and the cross cap.  The difference
 * is where the origin and the line at infinity will lie with respect
 * to the self-intersections in the projections to 3d.
 *
 * The projected projective plane can then be projected to the screen
 * either perspectively or orthographically.  When using the walking
 * modes, perspective projection to the screen will be used.
 *
 * There are three display modes for the projective plane: mesh
 * (wireframe), solid, or transparent.  Furthermore, the appearance of
 * the projective plane can be as a solid object or as a set of
 * see-through bands.  The bands can be distance bands, i.e., bands
 * that lie at increasing distances from the origin, or direction
 * bands, i.e., bands that lie at increasing angles with respect to
 * the origin.
 *
 * When the projective plane is displayed with direction bands, you
 * will be able to see that each direction band (modulo the "pinching"
 * at the origin) is a Moebius strip, which also shows that the
 * projective plane is non-orientable.
 *
 * Finally, the colors with with the projective plane is drawn can be
 * set to one-sided, two-sided, distance, direction, or depth.  In
 * one-sided mode, the projective plane is drawn with the same color
 * on both "sides."  In two-sided mode (using static colors), the
 * projective plane is drawn with red on one "side" and green on the
 * "other side."  As described above, the projective plane only has
 * one side, so the color jumps from red to green along the line at
 * infinity.  This mode enables you to see that the projective plane
 * is non-orientable.  If changing colors are used in two-sided mode,
 * changing complementary colors are used on the respective "sides."
 * In distance mode, the projective plane is displayed with fully
 * saturated colors that depend on the distance of the points on the
 * projective plane to the origin.  If static colors are used, the
 * origin is displayed in red, while the line at infinity is displayed
 * in magenta.  If the projective plane is displayed as distance
 * bands, each band will be displayed with a different color.  In
 * direction mode, the projective plane is displayed with fully
 * saturated colors that depend on the angle of the points on the
 * projective plane with respect to the origin.  Angles in opposite
 * directions to the origin (e.g., 15 and 205 degrees) are displayed
 * in the same color since they are projectively equivalent.  If the
 * projective plane is displayed as direction bands, each band will be
 * displayed with a different color.  Finally, in depth mode the
 * projective plane is displayed with colors chosen depending on the
 * 4d "depth" (i.e., the w coordinate) of the points on the projective
 * plane at its default orientation in 4d.  As discussed above, this
 * mode enables you to see that the projective plane does not
 * intersect itself in 4d.
 *
 * The rotation speed for each of the six planes around which the
 * projective plane rotates can be chosen.  For the walk-and-turn
 * mode, only the rotation speeds around the true 4d planes are used
 * (the xy, xz, and yz planes).
 *
 * Furthermore, in the walking modes the walking direction in the 2d
 * base square of the projective plane and the walking speed can be
 * chosen.  The walking direction is measured as an angle in degrees
 * in the 2d square that forms the coordinate system of the surface of
 * the projective plane.  A value of 0 or 180 means that the walk is
 * along a circle at a randomly chosen distance from the origin
 * (parallel to a distance band).  A value of 90 or 270 means that the
 * walk is directly from the origin to the line at infinity and back
 * (analogous to a direction band).  Any other value results in a
 * curved path from the origin to the line at infinity and back.
 *
 * This program is somewhat inspired by Thomas Banchoff's book "Beyond
 * the Third Dimension: Geometry, Computer Graphics, and Higher
 * Dimensions", Scientific American Library, 1990.
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
#define COLORS_DEPTH               4
#define NUM_COLORS                 5

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
#define DEF_WALK_DIRECTION         "83.0"
#define DEF_WALK_SPEED             "20.0"


#ifdef STANDALONE
# define DEFAULTS           "*delay:      25000 \n" \
                            "*showFPS:    False \n" \
                            "*prefersGLSL: True \n" \

# define release_projectiveplane 0
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

#include <float.h>


#ifdef USE_MODULES
ModStruct projectiveplane_description =
{"projectiveplane", "init_projectiveplane", "draw_projectiveplane",
 NULL, "draw_projectiveplane", "change_projectiveplane",
 NULL, &projectiveplane_opts, 25000, 1, 1, 1, 1.0, 4, "",
 "Rotate a 4d embedding of the real projective plane in 4d or walk on it",
 0, NULL};

#endif


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
  {"-mode",              ".displayMode",   XrmoptionSepArg, 0 },
  {"-wireframe",         ".displayMode",   XrmoptionNoArg,  "wireframe" },
  {"-surface",           ".displayMode",   XrmoptionNoArg,  "surface" },
  {"-transparent",       ".displayMode",   XrmoptionNoArg,  "transparent" },
  {"-appearance",        ".appearance",    XrmoptionSepArg, 0 },
  {"-solid",             ".appearance",    XrmoptionNoArg,  "solid" },
  {"-distance-bands",    ".appearance",    XrmoptionNoArg,  "distance-bands" },
  {"-direction-bands",   ".appearance",    XrmoptionNoArg,  "direction-bands" },
  {"-colors",            ".colors",        XrmoptionSepArg, 0 },
  {"-onesided-colors",   ".colors",        XrmoptionNoArg,  "one-sided" },
  {"-twosided-colors",   ".colors",        XrmoptionNoArg,  "two-sided" },
  {"-distance-colors",   ".colors",        XrmoptionNoArg,  "distance" },
  {"-direction-colors",  ".colors",        XrmoptionNoArg,  "direction" },
  {"-depth-colors",      ".colors",        XrmoptionNoArg,  "depth" },
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

ENTRYPOINT ModeSpecOpt projectiveplane_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, NULL};


/* Offset by which we walk above the projective plane */
#define DELTAY  0.01

/* Color change speeds */
#define DRHO    0.7
#define DSIGMA  1.1
#define DTAU    1.7

/* Number of subdivisions of the projective plane */
#define NUMU 128
#define NUMV 128

/* Number of subdivisions per band */
#define NUMB 8


#if !defined(__GNUC__) && !defined(__extension__)
 /* don't warn about "string length is greater than the length ISO C89
    compilers are required to support" in these string constants... */
# define  __extension__ /**/
#endif


typedef struct {
  GLint      WindH, WindW;
  GLXContext *glx_context;
  /* Options */
  int display_mode;
  int appearance;
  int colors;
  Bool change_colors;
  int view;
  Bool marks;
  int projection_3d;
  int projection_4d;
  /* 4D rotation angles */
  float alpha, beta, delta, zeta, eta, theta;
  /* Color rotation angles */
  float rho, sigma, tau;
  /* Movement parameters */
  float umove, vmove, dumove, dvmove;
  int side, dir;
  /* The viewing offset in 4d */
  float offset4d[4];
  /* The viewing offset in 3d */
  float offset3d[4];
  /* The 4d coordinates of the projective plane and their derivatives */
  float x[(NUMU+1)*(NUMV+1)][4];
  float xu[(NUMU+1)*(NUMV+1)][4];
  float xv[(NUMU+1)*(NUMV+1)][4];
  float pp[(NUMU+1)*(NUMV+1)][3];
  float pn[(NUMU+1)*(NUMV+1)][3];
  /* The precomputed colors of the projective plane */
  float col[(NUMU+1)*(NUMV+1)][4];
  /* The precomputed texture coordinates of the projective plane */
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
  GLint off4d_index, off3d_index;
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
} projectiveplanestruct;

static projectiveplanestruct *projectiveplane = (projectiveplanestruct *) NULL;


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
  __extension__
  "uniform mat4 MatRot4D;\n"
  "uniform mat4 MatProj;\n"
  "uniform bool BoolPersp;\n"
  "uniform vec4 Offset4D;\n"
  "uniform vec4 Offset3D;\n"
  "uniform bool BoolTextures;\n"
  "\n"
  "void main (void)\n"
  "{\n"
  "  const float EPSILON = 1.0e-7f;\n"
  "  float u, v, su, cu, s2u, c2u, sv2, cv2, sv4, cv4;\n"
  "  vec3 p, pu, pv;\n"
  "  u = VertexUV.x;\n"
  "  v = VertexUV.y;\n"
  "  su = sin(u)\n;"
  "  cu = cos(u)\n;"
  "  s2u = sin(2.0f*u)\n;"
  "  c2u = cos(2.0f*u)\n;"
  "  sv2 = sin(0.5f*v)\n;"
  "  cv2 = cos(0.5f*v)\n;"
  "  sv4 = sin(0.25f*v)\n;"
  "  cv4 = cos(0.25f*v)\n;"
  "  vec4 xx = vec4(0.5f*s2u*sv4*sv4,\n"
  "                 0.5f*su*sv2,\n"
  "                 0.5f*cu*sv2,\n"
  "                 0.5f*(su*su*sv4*sv4-cv4*cv4));\n"
  "  if (v < EPSILON)\n"
  "  {\n"
  "    v = EPSILON;\n"
  "    sv2 = sin(0.5f*v)\n;"
  "    cv2 = cos(0.5f*v)\n;"
  "    sv4 = sin(0.25f*v)\n;"
  "  }\n"
  "  vec4 xxu = vec4(c2u*sv4*sv4,\n"
  "                  0.5f*cu*sv2,\n"
  "                  -0.5f*su*sv2,\n"
  "                  0.5f*s2u*sv4*sv4);\n"
  "  vec4 xxv = vec4(0.125f*s2u*sv2,\n"
  "                  0.25f*su*cv2,\n"
  "                  0.25f*cu*cv2,\n"
  "                  0.125f*(su*su+1.0f)*sv2);\n"
  "  vec4 x = MatRot4D*xx+Offset4D;\n"
  "  vec4 xu = MatRot4D*xxu;\n"
  "  vec4 xv = MatRot4D*xxv;\n"
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
  __extension__
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
static void color(projectiveplanestruct *pp, double angle, float mat[3][3],
                  float col[4])
{
  int s;
  double t, ca, sa;
  float m;

  if (!pp->change_colors)
  {
    if (pp->colors == COLORS_ONESIDED || pp->colors == COLORS_TWOSIDED)
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
  else /* pp->change_colors */
  {
    if (pp->colors == COLORS_ONESIDED || pp->colors == COLORS_TWOSIDED)
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
  if (pp->display_mode == DISP_TRANSPARENT)
    col[3] = 0.7;
  else
    col[3] = 1.0;
}


/* Set up the projective plane coordinates, colors, and texture. */
static void setup_projective_plane(ModeInfo *mi, double umin, double umax,
                                   double vmin, double vmax)
{
  int i, j, k;
  double u, v, w, ur, vr;
  double cu, su, cv2, sv2, cv4, sv4, c2u, s2u;
  projectiveplanestruct *pp = &projectiveplane[MI_SCREEN(mi)];

  ur = umax-umin;
  vr = vmax-vmin;
  for (i=0; i<=NUMV; i++)
  {
    for (j=0; j<=NUMU; j++)
    {
      k = i*(NUMU+1)+j;
      if (pp->appearance != APPEARANCE_DIRECTION_BANDS)
        u = -ur*j/NUMU+umin;
      else
        u = ur*j/NUMU+umin;
      v = vr*i/NUMV+vmin;
      su = sin(u);
      cu = cos(u);
      s2u = sin(2.0*u);
      c2u = cos(2.0*u);
      sv2 = sin(0.5*v);
      sv4 = sin(0.25*v);
      cv4 = cos(0.25*v);
      w = 0.5*(su*su*sv4*sv4-cv4*cv4);
      if (!pp->change_colors)
      {
        if (pp->colors == COLORS_DEPTH)
          color(pp,(2.0*w+1.0)*M_PI*2.0/3.0,NULL,pp->col[k]);
        else if (pp->colors == COLORS_DIRECTION)
          color(pp,2.0*M_PI+fmod(2.0*u,2.0*M_PI),NULL,pp->col[k]);
        else /* pp->colors == COLORS_DISTANCE */
          color(pp,v*(5.0/6.0),NULL,pp->col[k]);
      }
      pp->tex[k][0] = -32*u/(2.0*M_PI);
      if (pp->appearance != APPEARANCE_DISTANCE_BANDS)
        pp->tex[k][1] = 32*v/(2.0*M_PI);
      else
        pp->tex[k][1] = 32*v/(2.0*M_PI)-0.5;
      pp->x[k][0] = 0.5*s2u*sv4*sv4;
      pp->x[k][1] = 0.5*su*sv2;
      pp->x[k][2] = 0.5*cu*sv2;
      pp->x[k][3] = w;
      /* Avoid degenerate tangential plane basis vectors. */
      if (v < FLT_EPSILON)
        v = FLT_EPSILON;
      sv2 = sin(0.5*v);
      cv2 = cos(0.5*v);
      sv4 = sin(0.25*v);
      pp->xu[k][0] = c2u*sv4*sv4;
      pp->xu[k][1] = 0.5*cu*sv2;
      pp->xu[k][2] = -0.5*su*sv2;
      pp->xu[k][3] = 0.5*s2u*sv4*sv4;
      pp->xv[k][0] = 0.125*s2u*sv2;
      pp->xv[k][1] = 0.25*su*cv2;
      pp->xv[k][2] = 0.25*cu*cv2;
      pp->xv[k][3] = 0.125*(su*su+1.0)*sv2;
    }
  }
}


/* Compute the current walk frame, i.e., the coordinate system of the
   point and direction at which the viewer is currently walking on the
   projective plane. */
static void compute_walk_frame(projectiveplanestruct *pp, float mat[4][4])
{
  int l, m;
  double u, v;
  double q, r, s, t;
  double cu, su, cv2, sv2, cv4, sv4, c2u, s2u;
  float p[3], pu[3], pv[3], pm[3], n[3], b[3];
  double xx[4], xxu[4], xxv[4], y[4], yu[4], yv[4];

  /* Compute the rotation that rotates the projective plane in 4D without
     the trackball rotations. */
  rotateall4d(pp->zeta,pp->eta,pp->theta,mat);

  u = pp->umove;
  v = pp->vmove;
  su = sin(u);
  cu = cos(u);
  s2u = sin(2.0*u);
  c2u = cos(2.0*u);
  sv2 = sin(0.5*v);
  sv4 = sin(0.25*v);
  cv4 = cos(0.25*v);
  xx[0] = 0.5*s2u*sv4*sv4;
  xx[1] = 0.5*su*sv2;
  xx[2] = 0.5*cu*sv2;
  xx[3] = 0.5*(su*su*sv4*sv4-cv4*cv4);
  /* Avoid degenerate tangential plane basis vectors. */
  if (v < FLT_EPSILON)
    v = FLT_EPSILON;
  sv2 = sin(0.5*v);
  cv2 = cos(0.5*v);
  sv4 = sin(0.25*v);
  xxu[0] = c2u*sv4*sv4;
  xxu[1] = 0.5*cu*sv2;
  xxu[2] = -0.5*su*sv2;
  xxu[3] = 0.5*s2u*sv4*sv4;
  xxv[0] = 0.125*s2u*sv2;
  xxv[1] = 0.25*su*cv2;
  xxv[2] = 0.25*cu*cv2;
  xxv[3] = 0.125*(su*su+1.0)*sv2;
  for (l=0; l<4; l++)
  {
    y[l] = (mat[l][0]*xx[0]+mat[l][1]*xx[1]+
            mat[l][2]*xx[2]+mat[l][3]*xx[3]);
    yu[l] = (mat[l][0]*xxu[0]+mat[l][1]*xxu[1]+
             mat[l][2]*xxu[2]+mat[l][3]*xxu[3]);
    yv[l] = (mat[l][0]*xxv[0]+mat[l][1]*xxv[1]+
             mat[l][2]*xxv[2]+mat[l][3]*xxv[3]);
  }
  if (pp->projection_4d == DISP_4D_ORTHOGRAPHIC)
  {
    for (l=0; l<3; l++)
    {
      p[l] = y[l]+pp->offset4d[l];
      pu[l] = yu[l];
      pv[l] = yv[l];
    }
  }
  else
  {
    s = y[3]+pp->offset4d[3];
    q = 1.0/s;
    t = q*q;
    for (l=0; l<3; l++)
    {
      r = y[l]+pp->offset4d[l];
      p[l] = r*q;
      pu[l] = (yu[l]*s-r*yu[3])*t;
      pv[l] = (yv[l]*s-r*yv[3])*t;
    }
  }
  n[0] = pu[1]*pv[2]-pu[2]*pv[1];
  n[1] = pu[2]*pv[0]-pu[0]*pv[2];
  n[2] = pu[0]*pv[1]-pu[1]*pv[0];
  t = 1.0/(pp->side*4.0*sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]));
  n[0] *= t;
  n[1] *= t;
  n[2] *= t;
  pm[0] = pu[0]*pp->dumove+pv[0]*pp->dvmove;
  pm[1] = pu[1]*pp->dumove+pv[1]*pp->dvmove;
  pm[2] = pu[2]*pp->dumove+pv[2]*pp->dvmove;
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
  pp->alpha = atan2(-n[2],-pm[2])*180/M_PI;
  pp->beta = atan2(-b[2],sqrt(b[0]*b[0]+b[1]*b[1]))*180/M_PI;
  pp->delta = atan2(b[1],-b[0])*180/M_PI;

  /* Compute the rotation that rotates the projective plane in 4D. */
  rotateall(pp->alpha,pp->beta,pp->delta,pp->zeta,pp->eta,pp->theta,mat);

  u = pp->umove;
  v = pp->vmove;
  su = sin(u);
  cu = cos(u);
  s2u = sin(2.0*u);
  sv2 = sin(0.5*v);
  sv4 = sin(0.25*v);
  cv4 = cos(0.25*v);
  xx[0] = 0.5*s2u*sv4*sv4;
  xx[1] = 0.5*su*sv2;
  xx[2] = 0.5*cu*sv2;
  xx[3] = 0.5*(su*su*sv4*sv4-cv4*cv4);
  for (l=0; l<4; l++)
  {
    r = 0.0;
    for (m=0; m<4; m++)
      r += mat[l][m]*xx[m];
    y[l] = r;
  }
  if (pp->projection_4d == DISP_4D_ORTHOGRAPHIC)
  {
    for (l=0; l<3; l++)
      p[l] = y[l]+pp->offset4d[l];
  }
  else
  {
    s = y[3]+pp->offset4d[3];
    for (l=0; l<3; l++)
      p[l] = (y[l]+pp->offset4d[l])/s;
  }

  pp->offset3d[0] = -p[0];
  pp->offset3d[1] = -p[1]-DELTAY;
  pp->offset3d[2] = -p[2];
}


/* Draw a 4d embedding of the projective plane projected into 3D using
   OpenGL's fixed functionality. */
static int projective_plane_ff(ModeInfo *mi, double umin, double umax,
                               double vmin, double vmax)
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
  float pu[3], pv[3], mat[4][4], matc[3][3];
  int i, j, k, l, m, o;
  double u, v, ur, vr;
  double y[4], yu[4], yv[4];
  double q, r, s, t;
  float q1[4], q2[4], r1[4][4], r2[4][4];
  projectiveplanestruct *pp = &projectiveplane[MI_SCREEN(mi)];
  int polys;

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  if (pp->projection_3d == DISP_3D_PERSPECTIVE ||
      pp->view == VIEW_WALK || pp->view == VIEW_WALKTURN)
  {
    if (pp->view == VIEW_WALK || pp->view == VIEW_WALKTURN)
      gluPerspective(60.0,pp->aspect,0.01,10.0);
    else
      gluPerspective(60.0,pp->aspect,0.1,10.0);
  }
  else
  {
    if (pp->aspect >= 1.0)
      glOrtho(-0.6*pp->aspect,0.6*pp->aspect,-0.6,0.6,0.1,10.0);
    else
      glOrtho(-0.6,0.6,-0.6/pp->aspect,0.6/pp->aspect,0.1,10.0);
  }
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  if (pp->display_mode == DISP_SURFACE)
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
  else if (pp->display_mode == DISP_TRANSPARENT)
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
  else  /* pp->display_mode == DISP_WIREFRAME */
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

  if (pp->marks)
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

  if (pp->change_colors)
    rotateall3d(pp->rho,pp->sigma,pp->tau,matc);

  if (pp->view == VIEW_WALK || pp->view == VIEW_WALKTURN)
  {
    /* Compute the walk frame. */
    compute_walk_frame(pp,mat);
  }
  else
  {
    /* Compute the rotation that rotates the projective plane in 4D,
       including the trackball rotations. */
    rotateall(pp->alpha,pp->beta,pp->delta,pp->zeta,pp->eta,pp->theta,r1);

    gltrackball_get_quaternion(pp->trackballs[0],q1);
    gltrackball_get_quaternion(pp->trackballs[1],q2);
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
        y[l] = (mat[l][0]*pp->x[o][0]+mat[l][1]*pp->x[o][1]+
                mat[l][2]*pp->x[o][2]+mat[l][3]*pp->x[o][3]);
        yu[l] = (mat[l][0]*pp->xu[o][0]+mat[l][1]*pp->xu[o][1]+
                 mat[l][2]*pp->xu[o][2]+mat[l][3]*pp->xu[o][3]);
        yv[l] = (mat[l][0]*pp->xv[o][0]+mat[l][1]*pp->xv[o][1]+
                 mat[l][2]*pp->xv[o][2]+mat[l][3]*pp->xv[o][3]);
      }
      if (pp->projection_4d == DISP_4D_ORTHOGRAPHIC)
      {
        for (l=0; l<3; l++)
        {
          pp->pp[o][l] = (y[l]+pp->offset4d[l])+pp->offset3d[l];
          pu[l] = yu[l];
          pv[l] = yv[l];
        }
      }
      else
      {
        s = y[3]+pp->offset4d[3];
        q = 1.0/s;
        t = q*q;
        for (l=0; l<3; l++)
        {
          r = y[l]+pp->offset4d[l];
          pp->pp[o][l] = r*q+pp->offset3d[l];
          pu[l] = (yu[l]*s-r*yu[3])*t;
          pv[l] = (yv[l]*s-r*yv[3])*t;
        }
      }
      pp->pn[o][0] = pu[1]*pv[2]-pu[2]*pv[1];
      pp->pn[o][1] = pu[2]*pv[0]-pu[0]*pv[2];
      pp->pn[o][2] = pu[0]*pv[1]-pu[1]*pv[0];
      t = 1.0/sqrt(pp->pn[o][0]*pp->pn[o][0]+pp->pn[o][1]*pp->pn[o][1]+
                   pp->pn[o][2]*pp->pn[o][2]);
      pp->pn[o][0] *= t;
      pp->pn[o][1] *= t;
      pp->pn[o][2] *= t;
    }
  }

  if (!pp->change_colors)
  {
    if (pp->colors == COLORS_ONESIDED)
    {
      glColor3fv(mat_diff_oneside);
      if (pp->display_mode == DISP_TRANSPARENT)
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
    else if (pp->colors == COLORS_TWOSIDED)
    {
      glColor3fv(mat_diff_red);
      if (pp->display_mode == DISP_TRANSPARENT)
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
  else /* pp->change_colors */
  {
    color(pp,0.0,matc,mat_diff_dyn);
    if (pp->colors == COLORS_ONESIDED)
    {
      glColor3fv(mat_diff_dyn);
      glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,mat_diff_dyn);
    }
    else if (pp->colors == COLORS_TWOSIDED)
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
  glBindTexture(GL_TEXTURE_2D,pp->tex_name);

  ur = umax-umin;
  vr = vmax-vmin;
  if (pp->appearance != APPEARANCE_DIRECTION_BANDS)
  {
    for (i=0; i<NUMV; i++)
    {
      if (pp->appearance == APPEARANCE_DISTANCE_BANDS &&
          ((i & (NUMB-1)) >= NUMB/4) && ((i & (NUMB-1)) < 3*NUMB/4))
        continue;
      if (pp->display_mode == DISP_WIREFRAME)
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
          glNormal3fv(pp->pn[o]);
          glTexCoord2fv(pp->tex[o]);
          if (pp->change_colors)
          {
            if (pp->colors == COLORS_DEPTH)
            {
              color(pp,(2.0*pp->x[o][3]+1.0)*M_PI*2.0/3.0,matc,pp->col[o]);
            }
            else if (pp->colors == COLORS_DIRECTION)
            {
              u = -ur*m/NUMU+umin;
              color(pp,2.0*M_PI+fmod(2.0*u,2.0*M_PI),matc,pp->col[o]);
            }
            else if (pp->colors == COLORS_DISTANCE)
            {
              v = vr*l/NUMV+vmin;
              color(pp,v*(5.0/6.0),matc,pp->col[o]);
            }
          }
          if (pp->colors != COLORS_ONESIDED && pp->colors != COLORS_TWOSIDED)
          {
            glColor3fv(pp->col[o]);
            glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,pp->col[o]);
          }
          glVertex3fv(pp->pp[o]);
        }
      }
      glEnd();
    }
  }
  else /* pp->appearance == APPEARANCE_DIRECTION_BANDS */
  {
    for (j=0; j<NUMU; j++)
    {
      if ((j & (NUMB-1)) >= NUMB/2)
        continue;
      if (pp->display_mode == DISP_WIREFRAME)
        glBegin(GL_QUAD_STRIP);
      else
        glBegin(GL_TRIANGLE_STRIP);
      for (i=0; i<=NUMV; i++)
      {
        for (k=0; k<=1; k++)
        {
          l = i;
          m = j+k;
          o = l*(NUMU+1)+m;
          glNormal3fv(pp->pn[o]);
          glTexCoord2fv(pp->tex[o]);
          if (pp->change_colors)
          {
            if (pp->colors == COLORS_DEPTH)
            {
              color(pp,(2.0*pp->x[o][3]+1.0)*M_PI*2.0/3.0,matc,pp->col[o]);
            }
            else if (pp->colors == COLORS_DIRECTION)
            {
              u = ur*m/NUMU+umin;
              color(pp,2.0*M_PI+fmod(2.0*u,2.0*M_PI),matc,pp->col[o]);
            }
            else if (pp->colors == COLORS_DISTANCE)
            {
              v = vr*l/NUMV+vmin;
              color(pp,v*(5.0/6.0),matc,pp->col[o]);
            }
          }
          if (pp->colors != COLORS_ONESIDED && pp->colors != COLORS_TWOSIDED)
          {
            glColor3fv(pp->col[o]);
            glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,pp->col[o]);
          }
          glVertex3fv(pp->pp[o]);
        }
      }
      glEnd();
    }
  }

  polys = 2*NUMU*NUMV;
  if (pp->appearance != APPEARANCE_SOLID)
    polys /= 2;
  return polys;
}


#ifdef HAVE_GLSL

/* Draw a 4d embedding of the projective plane projected into 3D using
   OpenGL's programmable functionality. */
static int projective_plane_pf(ModeInfo *mi, double umin, double umax,
                               double vmin, double vmax)
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
  GLfloat p_mat[16];
  float mat_diff_dyn[4], mat_diff_dyn_compl[4];
  float mat[4][4], matc[3][3];
  int i, j, k, l, m, o;
  double u, v, ur, vr;
  float q1[4], q2[4], r1[4][4], r2[4][4];
  GLsizeiptr index_offset;
  projectiveplanestruct *pp = &projectiveplane[MI_SCREEN(mi)];
  int polys;

  if (!pp->use_shaders)
    return 0;

  if (!pp->buffers_initialized)
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
        if (pp->appearance != APPEARANCE_DIRECTION_BANDS)
          u = -ur*j/NUMU+umin;
        else
          u = ur*j/NUMU+umin;
        v = vr*i/NUMV+vmin;
        pp->uv[o][0] = u;
        pp->uv[o][1] = v;
      }
    }
    glBindBuffer(GL_ARRAY_BUFFER,pp->vertex_uv_buffer);
    glBufferData(GL_ARRAY_BUFFER,2*(NUMU+1)*(NUMV+1)*sizeof(GLfloat),
                 pp->uv,GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);

    glBindBuffer(GL_ARRAY_BUFFER,pp->vertex_t_buffer);
    glBufferData(GL_ARRAY_BUFFER,2*(NUMU+1)*(NUMV+1)*sizeof(GLfloat),
                 pp->tex,GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);

    if (!pp->change_colors &&
        pp->colors != COLORS_ONESIDED && pp->colors != COLORS_TWOSIDED)
    {
      glBindBuffer(GL_ARRAY_BUFFER,pp->color_buffer);
      glBufferData(GL_ARRAY_BUFFER,4*(NUMU+1)*(NUMV+1)*sizeof(GLfloat),
                   pp->col,GL_STATIC_DRAW);
      glBindBuffer(GL_ARRAY_BUFFER,0);
    }

    /* The indices only need to be computed once (or each time the value of
       appearance changes, once we support that). */
    pp->ni = 0;
    pp->ne = 0;
    pp->nt = 0;
    if (pp->display_mode != DISP_WIREFRAME)
    {
      if (pp->appearance != APPEARANCE_DIRECTION_BANDS)
      {
        for (i=0; i<NUMV; i++)
        {
          if (pp->appearance == APPEARANCE_DISTANCE_BANDS &&
              ((i & (NUMB-1)) >= NUMB/4) && ((i & (NUMB-1)) < 3*NUMB/4))
            continue;
          for (j=0; j<=NUMU; j++)
          {
            for (k=0; k<=1; k++)
            {
              l = i+k;
              m = j;
              o = l*(NUMU+1)+m;
              pp->indices[pp->ni++] = o;
            }
          }
          pp->ne++;
        }
        pp->nt = 2*(NUMU+1);
      }
      else /* pp->appearance == APPEARANCE_DIRECTION_BANDS */
      {
        for (j=0; j<NUMU; j++)
        {
          if ((j & (NUMB-1)) >= NUMB/2)
            continue;
          for (i=0; i<=NUMV; i++)
          {
            for (k=0; k<=1; k++)
            {
              l = i;
              m = j+k;
              o = l*(NUMU+1)+m;
              pp->indices[pp->ni++] = o;
            }
          }
          pp->ne++;
        }
        pp->nt = 2*(NUMV+1);
      }
    }
    else /* pp->display_mode == DISP_WIREFRAME */
    {
      if (pp->appearance != APPEARANCE_DIRECTION_BANDS)
      {
        for (i=0; i<=NUMV; i++)
        {
          if (pp->appearance == APPEARANCE_DISTANCE_BANDS &&
              ((i & (NUMB-1)) > NUMB/4) && ((i & (NUMB-1)) < 3*NUMB/4))
            continue;
          if (pp->appearance == APPEARANCE_DISTANCE_BANDS &&
              ((i & (NUMB-1)) == NUMB/4))
          {
            for (j=0; j<NUMU; j++)
            {
              pp->indices[pp->ni++] = i*(NUMU+1)+j;
              pp->indices[pp->ni++] = i*(NUMU+1)+j+1;
            }
            continue;
          }
          for (j=0; j<NUMU; j++)
          {
            pp->indices[pp->ni++] = i*(NUMU+1)+j;
            pp->indices[pp->ni++] = i*(NUMU+1)+j+1;
            if (i < NUMV)
            {
              pp->indices[pp->ni++] = i*(NUMU+1)+j;
              pp->indices[pp->ni++] = (i+1)*(NUMU+1)+j;
            }
          }
        }
      }
      else /* pp->appearance == APPEARANCE_DIRECTION_BANDS */
      {
        for (j=0; j<NUMU; j++)
        {
          if ((j & (NUMB-1)) > NUMB/2)
            continue;
          if ((j & (NUMB-1)) == NUMB/2)
          {
            for (i=0; i<NUMV; i++)
            {
              pp->indices[pp->ni++] = i*(NUMU+1)+j;
              pp->indices[pp->ni++] = (i+1)*(NUMU+1)+j;
            }
            continue;
          }
          for (i=0; i<=NUMV; i++)
          {
            pp->indices[pp->ni++] = i*(NUMU+1)+j;
            pp->indices[pp->ni++] = i*(NUMU+1)+j+1;
            if (i < NUMV)
            {
              pp->indices[pp->ni++] = i*(NUMU+1)+j;
              pp->indices[pp->ni++] = (i+1)*(NUMU+1)+j;
            }
          }
        }
      }
      pp->ne = 1;
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,pp->indices_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,pp->ni*sizeof(GLuint),
                 pp->indices,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

    pp->buffers_initialized = True;
  }

  if (pp->change_colors)
    rotateall3d(pp->rho,pp->sigma,pp->tau,matc);

  if (pp->view == VIEW_WALK || pp->view == VIEW_WALKTURN)
  {
    /* Compute the walk frame. */
    compute_walk_frame(pp,mat);
  }
  else
  {
    /* Compute the rotation that rotates the projective plane in 4D,
       including the trackball rotations. */
    rotateall(pp->alpha,pp->beta,pp->delta,pp->zeta,pp->eta,pp->theta,r1);

    gltrackball_get_quaternion(pp->trackballs[0],q1);
    gltrackball_get_quaternion(pp->trackballs[1],q2);
    quats_to_rotmat(q1,q2,r2);

    mult_rotmat(r2,r1,mat);
  }

  if (pp->change_colors &&
      (pp->colors == COLORS_DEPTH || pp->colors == COLORS_DIRECTION ||
       pp->colors == COLORS_DISTANCE))
  {
    ur = umax-umin;
    vr = vmax-vmin;
    for (i=0; i<=NUMV; i++)
    {
      for (j=0; j<=NUMU; j++)
      {
        o = i*(NUMU+1)+j;
        if (pp->colors == COLORS_DEPTH)
        {
          color(pp,(2.0*pp->x[o][3]+1.0)*M_PI*2.0/3.0,matc,pp->col[o]);
        }
        else if (pp->colors == COLORS_DIRECTION)
        {
          u = -ur*j/NUMU+umin;
          color(pp,2.0*M_PI+fmod(2.0*u,2.0*M_PI),matc,pp->col[o]);
        }
        else if (pp->colors == COLORS_DISTANCE)
        {
          v = vr*i/NUMV+vmin;
          color(pp,v*(5.0/6.0),matc,pp->col[o]);
        }
      }
    }
  }

  glUseProgram(pp->shader_program);

  glsl_Identity(p_mat);
  if (pp->projection_3d == DISP_3D_PERSPECTIVE ||
      pp->view == VIEW_WALK || pp->view == VIEW_WALKTURN)
  {
    if (pp->view == VIEW_WALK || pp->view == VIEW_WALKTURN)
      glsl_Perspective(p_mat,60.0f,pp->aspect,0.01f,10.0f);
    else
      glsl_Perspective(p_mat,60.0f,pp->aspect,0.1f,10.0f);
  }
  else
  {
    if (pp->aspect >= 1.0)
      glsl_Orthographic(p_mat,-0.6f*pp->aspect,0.6f*pp->aspect,-0.6f,0.6f,
                        0.1f,10.0f);
    else
      glsl_Orthographic(p_mat,-0.6f,0.6f,-0.6f/pp->aspect,0.6f/pp->aspect,
                        0.1f,10.0f);
  }
  glUniformMatrix4fv(pp->mat_rot_index,1,GL_TRUE,(GLfloat *)mat);
  glUniformMatrix4fv(pp->mat_p_index,1,GL_FALSE,p_mat);
  glUniform1i(pp->bool_persp_index,pp->projection_4d == DISP_4D_PERSPECTIVE);
  glUniform4fv(pp->off4d_index,1,pp->offset4d);
  glUniform4fv(pp->off3d_index,1,pp->offset3d);

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

  if (pp->display_mode == DISP_SURFACE)
  {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glUniform4fv(pp->glbl_ambient_index,1,light_model_ambient);
    glUniform4fv(pp->lt_ambient_index,1,light_ambient);
    glUniform4fv(pp->lt_diffuse_index,1,light_diffuse);
    glUniform4fv(pp->lt_specular_index,1,light_specular);
    glUniform3fv(pp->lt_direction_index,1,light_direction);
    glUniform3fv(pp->lt_halfvect_index,1,half_vector);
    glUniform4fv(pp->specular_index,1,mat_specular);
    glUniform1f(pp->shininess_index,50.0f);
    glUniform1i(pp->draw_lines_index,GL_FALSE);
  }
  else if (pp->display_mode == DISP_TRANSPARENT)
  {
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE);
    glUniform4fv(pp->glbl_ambient_index,1,light_model_ambient);
    glUniform4fv(pp->lt_ambient_index,1,light_ambient);
    glUniform4fv(pp->lt_diffuse_index,1,light_diffuse);
    glUniform4fv(pp->lt_specular_index,1,light_specular);
    glUniform3fv(pp->lt_direction_index,1,light_direction);
    glUniform3fv(pp->lt_halfvect_index,1,half_vector);
    glUniform4fv(pp->specular_index,1,mat_specular);
    glUniform1f(pp->shininess_index,50.0f);
    glUniform1i(pp->draw_lines_index,GL_FALSE);
  }
  else /* pp->display_mode == DISP_WIREFRAME */
  {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glUniform1i(pp->draw_lines_index,GL_TRUE);
  }

  if (pp->marks)
    glEnable(GL_TEXTURE_2D);
  else
    glDisable(GL_TEXTURE_2D);

  glUniform4fv(pp->front_ambient_index,1,mat_diff_white);
  glUniform4fv(pp->front_diffuse_index,1,mat_diff_white);
  glUniform4fv(pp->back_ambient_index,1,mat_diff_white);
  glUniform4fv(pp->back_diffuse_index,1,mat_diff_white);
  glVertexAttrib4f(pp->color_index,1.0f,1.0f,1.0f,1.0f);
  if (!pp->change_colors)
  {
    if (pp->colors == COLORS_ONESIDED)
    {
      if (pp->display_mode == DISP_TRANSPARENT)
      {
        glUniform4fv(pp->front_ambient_index,1,mat_diff_trans_oneside);
        glUniform4fv(pp->front_diffuse_index,1,mat_diff_trans_oneside);
        glUniform4fv(pp->back_ambient_index,1,mat_diff_trans_oneside);
        glUniform4fv(pp->back_diffuse_index,1,mat_diff_trans_oneside);
      }
      else if (pp->display_mode == DISP_SURFACE)
      {
        glUniform4fv(pp->front_ambient_index,1,mat_diff_oneside);
        glUniform4fv(pp->front_diffuse_index,1,mat_diff_oneside);
        glUniform4fv(pp->back_ambient_index,1,mat_diff_oneside);
        glUniform4fv(pp->back_diffuse_index,1,mat_diff_oneside);
      }
      else /* pp->display_mode == DISP_WIREFRAME */
      {
        glVertexAttrib4fv(pp->color_index,mat_diff_oneside);
      }
    }
    else if (pp->colors == COLORS_TWOSIDED)
    {
      if (pp->display_mode == DISP_TRANSPARENT)
      {
        glUniform4fv(pp->front_ambient_index,1,mat_diff_trans_red);
        glUniform4fv(pp->front_diffuse_index,1,mat_diff_trans_red);
        glUniform4fv(pp->back_ambient_index,1,mat_diff_trans_green);
        glUniform4fv(pp->back_diffuse_index,1,mat_diff_trans_green);
      }
      else if (pp->display_mode == DISP_SURFACE)
      {
        glUniform4fv(pp->front_ambient_index,1,mat_diff_red);
        glUniform4fv(pp->front_diffuse_index,1,mat_diff_red);
        glUniform4fv(pp->back_ambient_index,1,mat_diff_green);
        glUniform4fv(pp->back_diffuse_index,1,mat_diff_green);
      }
      else /* pp->display_mode == DISP_WIREFRAME */
      {
        glVertexAttrib4fv(pp->color_index,mat_diff_red);
      }
    }
  }
  else /* pp->change_colors */
  {
    color(pp,0.0,matc,mat_diff_dyn);
    if (pp->colors == COLORS_ONESIDED)
    {
      if (pp->display_mode == DISP_TRANSPARENT ||
          pp->display_mode == DISP_SURFACE)
      {
        glUniform4fv(pp->front_ambient_index,1,mat_diff_dyn);
        glUniform4fv(pp->front_diffuse_index,1,mat_diff_dyn);
        glUniform4fv(pp->back_ambient_index,1,mat_diff_dyn);
        glUniform4fv(pp->back_diffuse_index,1,mat_diff_dyn);
      }
      else /* pp->display_mode == DISP_WIREFRAME */
      {
        glVertexAttrib4fv(pp->color_index,mat_diff_dyn);
      }
    }
    else if (pp->colors == COLORS_TWOSIDED)
    {
      if (pp->display_mode == DISP_TRANSPARENT ||
          pp->display_mode == DISP_SURFACE)
      {
        mat_diff_dyn_compl[0] = 1.0f-mat_diff_dyn[0];
        mat_diff_dyn_compl[1] = 1.0f-mat_diff_dyn[1];
        mat_diff_dyn_compl[2] = 1.0f-mat_diff_dyn[2];
        mat_diff_dyn_compl[3] = mat_diff_dyn[3];
        glUniform4fv(pp->front_ambient_index,1,mat_diff_dyn);
        glUniform4fv(pp->front_diffuse_index,1,mat_diff_dyn);
        glUniform4fv(pp->back_ambient_index,1,mat_diff_dyn_compl);
        glUniform4fv(pp->back_diffuse_index,1,mat_diff_dyn_compl);
      }
      else /* pp->display_mode == DISP_WIREFRAME */
      {
        glVertexAttrib4fv(pp->color_index,mat_diff_dyn);
      }
    }
  }

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D,pp->tex_name);
  glUniform1i(pp->texture_sampler_index,0);
  glUniform1i(pp->bool_textures_index,pp->marks);

  glEnableVertexAttribArray(pp->vertex_uv_index);
  glBindBuffer(GL_ARRAY_BUFFER,pp->vertex_uv_buffer);
  glVertexAttribPointer(pp->vertex_uv_index,2,GL_FLOAT,GL_FALSE,0,0);

  glEnableVertexAttribArray(pp->vertex_t_index);
  glBindBuffer(GL_ARRAY_BUFFER,pp->vertex_t_buffer);
  glVertexAttribPointer(pp->vertex_t_index,2,GL_FLOAT,GL_FALSE,0,0);

  if (pp->colors != COLORS_ONESIDED && pp->colors != COLORS_TWOSIDED)
  {
    glEnableVertexAttribArray(pp->color_index);
    glBindBuffer(GL_ARRAY_BUFFER,pp->color_buffer);
    if (pp->change_colors)
      glBufferData(GL_ARRAY_BUFFER,4*(NUMU+1)*(NUMV+1)*sizeof(GLfloat),
                   pp->col,GL_STREAM_DRAW);
    glVertexAttribPointer(pp->color_index,4,GL_FLOAT,GL_FALSE,0,0);
  }

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,pp->indices_buffer);

  if (pp->display_mode != DISP_WIREFRAME)
  {
    for (i=0; i<pp->ne; i++)
    {
      index_offset = pp->nt*i*sizeof(GLuint);
      glDrawElements(GL_TRIANGLE_STRIP,pp->nt,GL_UNSIGNED_INT,
                     (const GLvoid *)index_offset);
    }
  }
  else /* pp->display_mode == DISP_WIREFRAME */
  {
    glLineWidth(1.0f);
    index_offset = 0;
    glDrawElements(GL_LINES,pp->ni,GL_UNSIGNED_INT,
                   (const void *)index_offset);
  }

  glDisableVertexAttribArray(pp->vertex_uv_index);
  glDisableVertexAttribArray(pp->vertex_t_index);
  if (pp->colors != COLORS_ONESIDED && pp->colors != COLORS_TWOSIDED)
    glDisableVertexAttribArray(pp->color_index);
  glBindBuffer(GL_ARRAY_BUFFER,0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

  glUseProgram(0);

  polys = 2*NUMU*NUMV;
  if (pp->appearance != APPEARANCE_SOLID)
    polys /= 2;
  return polys;
}

#endif /* HAVE_GLSL */


/* Generate a texture image that shows the orientation reversal. */
static void gen_texture(ModeInfo *mi)
{
  projectiveplanestruct *pp = &projectiveplane[MI_SCREEN(mi)];

  glPixelStorei(GL_UNPACK_ALIGNMENT,1);
  glGenTextures(1,&pp->tex_name);
  glBindTexture(GL_TEXTURE_2D,pp->tex_name);
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
  projectiveplanestruct *pp = &projectiveplane[MI_SCREEN(mi)];
  GLint gl_major, gl_minor, glsl_major, glsl_minor;
  GLboolean gl_gles3;
  const GLchar *vertex_shader_source[3];
  const GLchar *fragment_shader_source[4];

  /* Determine whether to use shaders to render the projective plane. */
  pp->use_shaders = False;
  pp->buffers_initialized = False;
  pp->shader_program = 0;
  pp->ni = 0;
  pp->ne = 0;
  pp->nt = 0;

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
                                  &pp->shader_program))
    return;
  pp->vertex_uv_index = glGetAttribLocation(pp->shader_program,"VertexUV");
  pp->vertex_t_index = glGetAttribLocation(pp->shader_program,"VertexT");
  pp->color_index = glGetAttribLocation(pp->shader_program,"VertexColor");
  if (pp->vertex_uv_index == -1 || pp->vertex_t_index == -1 ||
      pp->color_index == -1)
  {
    glDeleteProgram(pp->shader_program);
    return;
  }
  pp->mat_rot_index = glGetUniformLocation(pp->shader_program,
                                           "MatRot4D");
  pp->mat_p_index = glGetUniformLocation(pp->shader_program,
                                         "MatProj");
  pp->bool_persp_index = glGetUniformLocation(pp->shader_program,
                                              "BoolPersp");
  pp->off4d_index = glGetUniformLocation(pp->shader_program,
                                         "Offset4D");
  pp->off3d_index = glGetUniformLocation(pp->shader_program,
                                         "Offset3D");
  pp->bool_textures_index = glGetUniformLocation(pp->shader_program,
                                                "BoolTextures");
  pp->draw_lines_index = glGetUniformLocation(pp->shader_program,
                                              "DrawLines");
  pp->glbl_ambient_index = glGetUniformLocation(pp->shader_program,
                                                "LtGlblAmbient");
  pp->lt_ambient_index = glGetUniformLocation(pp->shader_program,
                                              "LtAmbient");
  pp->lt_diffuse_index = glGetUniformLocation(pp->shader_program,
                                              "LtDiffuse");
  pp->lt_specular_index = glGetUniformLocation(pp->shader_program,
                                               "LtSpecular");
  pp->lt_direction_index = glGetUniformLocation(pp->shader_program,
                                                "LtDirection");
  pp->lt_halfvect_index = glGetUniformLocation(pp->shader_program,
                                               "LtHalfVector");
  pp->front_ambient_index = glGetUniformLocation(pp->shader_program,
                                                 "MatFrontAmbient");
  pp->back_ambient_index = glGetUniformLocation(pp->shader_program,
                                                "MatBackAmbient");
  pp->front_diffuse_index = glGetUniformLocation(pp->shader_program,
                                                 "MatFrontDiffuse");
  pp->back_diffuse_index = glGetUniformLocation(pp->shader_program,
                                                "MatBackDiffuse");
  pp->specular_index = glGetUniformLocation(pp->shader_program,
                                            "MatSpecular");
  pp->shininess_index = glGetUniformLocation(pp->shader_program,
                                             "MatShininess");
  pp->texture_sampler_index = glGetUniformLocation(pp->shader_program,
                                                   "TextureSampler");
  if (pp->mat_rot_index == -1 || pp->mat_p_index == -1 ||
      pp->bool_persp_index == -1 || pp->off4d_index == -1 ||
      pp->off3d_index == -1 || pp->bool_textures_index == -1 ||
      pp->draw_lines_index == -1 || pp->glbl_ambient_index == -1 ||
      pp->lt_ambient_index == -1 || pp->lt_diffuse_index == -1 ||
      pp->lt_specular_index == -1 || pp->lt_direction_index == -1 ||
      pp->lt_halfvect_index == -1 || pp->front_ambient_index == -1 ||
      pp->back_ambient_index == -1 || pp->front_diffuse_index == -1 ||
      pp->back_diffuse_index == -1 || pp->specular_index == -1 ||
      pp->shininess_index == -1 || pp->texture_sampler_index == -1)
  {
    glDeleteProgram(pp->shader_program);
    return;
  }

  glGenBuffers(1,&pp->vertex_uv_buffer);
  glGenBuffers(1,&pp->vertex_t_buffer);
  glGenBuffers(1,&pp->color_buffer);
  glGenBuffers(1,&pp->indices_buffer);

  pp->use_shaders = True;
}

#endif /* HAVE_GLSL */


static void init(ModeInfo *mi)
{
  projectiveplanestruct *pp = &projectiveplane[MI_SCREEN(mi)];

  if (walk_speed == 0.0)
    walk_speed = 20.0;

  if (pp->view == VIEW_TURN)
  {
    pp->alpha = frand(360.0);
    pp->beta = frand(360.0);
    pp->delta = frand(360.0);
    pp->zeta = 0.0;
    pp->eta = 0.0;
    pp->theta = 0.0;
  }
  else
  {
    pp->alpha = 0.0;
    pp->beta = 0.0;
    pp->delta = 0.0;
    pp->zeta = 120.0;
    pp->eta = 180.0;
    pp->theta = 90.0;
  }
  pp->umove = frand(2.0*M_PI);
  pp->vmove = frand(2.0*M_PI);
  pp->dumove = 0.0;
  pp->dvmove = 0.0;
  pp->side = 1;
  if (sin(walk_direction*M_PI/180.0) >= 0.0)
    pp->dir = 1;
  else
    pp->dir = -1;

  pp->rho = frand(360.0);
  pp->sigma = frand(360.0);
  pp->tau = frand(360.0);

  pp->offset4d[0] = 0.0;
  pp->offset4d[1] = 0.0;
  pp->offset4d[2] = 0.0;
  pp->offset4d[3] = 1.2;
  pp->offset3d[0] = 0.0;
  pp->offset3d[1] = 0.0;
  pp->offset3d[2] = -1.2;
  pp->offset3d[3] = 0.0;

  gen_texture(mi);
  setup_projective_plane(mi,0.0,2.0*M_PI,0.0,2.0*M_PI);

#ifdef HAVE_GLSL
  init_glsl(mi);
#endif /* HAVE_GLSL */

#ifdef HAVE_ANDROID
  /* glPolygonMode(...,GL_LINE) is not supported for an OpenGL ES 1.1
     context. */
  if (!pp->use_shaders && pp->display_mode == DISP_WIREFRAME)
    pp->display_mode = DISP_SURFACE;
#endif /* HAVE_ANDROID */
}


/* Redisplay the Klein bottle. */
static void display_projectiveplane(ModeInfo *mi)
{
  projectiveplanestruct *pp = &projectiveplane[MI_SCREEN(mi)];

  if (!pp->button_pressed)
  {
    if (pp->view == VIEW_TURN)
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
    if (pp->view == VIEW_WALKTURN)
    {
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
    if (pp->view == VIEW_WALK || pp->view == VIEW_WALKTURN)
    {
      pp->dvmove = (pp->dir*sin(walk_direction*M_PI/180.0)*
                    walk_speed*M_PI/4096.0);
      pp->vmove += pp->dvmove;
      if (pp->vmove > 2.0*M_PI)
      {
        pp->vmove = 4.0*M_PI-pp->vmove;
        pp->umove = pp->umove-M_PI;
        if (pp->umove < 0.0)
          pp->umove += 2.0*M_PI;
        pp->side = -pp->side;
        pp->dir = -pp->dir;
        pp->dvmove = -pp->dvmove;
      }
      if (pp->vmove < 0.0)
      {
        pp->vmove = -pp->vmove;
        pp->umove = pp->umove-M_PI;
        if (pp->umove < 0.0)
          pp->umove += 2.0*M_PI;
        pp->dir = -pp->dir;
        pp->dvmove = -pp->dvmove;
      }
      pp->dumove = cos(walk_direction*M_PI/180.0)*walk_speed*M_PI/4096.0;
      pp->umove += pp->dumove;
      if (pp->umove >= 2.0*M_PI)
        pp->umove -= 2.0*M_PI;
      if (pp->umove < 0.0)
        pp->umove += 2.0*M_PI;
    }
    if (pp->change_colors)
    {
      pp->rho += DRHO;
      if (pp->rho >= 360.0)
        pp->rho -= 360.0;
      pp->sigma += DSIGMA;
      if (pp->sigma >= 360.0)
        pp->sigma -= 360.0;
      pp->tau += DTAU;
      if (pp->tau >= 360.0)
        pp->tau -= 360.0;
    }
  }

#ifdef HAVE_GLSL
  if (pp->use_shaders)
    mi->polygon_count = projective_plane_pf(mi,0.0,2.0*M_PI,0.0,2.0*M_PI);
  else
#endif /* HAVE_GLSL */
    mi->polygon_count = projective_plane_ff(mi,0.0,2.0*M_PI,0.0,2.0*M_PI);
}


ENTRYPOINT void reshape_projectiveplane(ModeInfo *mi, int width, int height)
{
  projectiveplanestruct *pp = &projectiveplane[MI_SCREEN(mi)];

  pp->WindW = (GLint)width;
  pp->WindH = (GLint)height;
  glViewport(0,0,width,height);
  pp->aspect = (GLfloat)width/(GLfloat)height;
}


ENTRYPOINT Bool projectiveplane_handle_event(ModeInfo *mi, XEvent *event)
{
  projectiveplanestruct *pp = &projectiveplane[MI_SCREEN(mi)];
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
 *    Initialize projectiveplane.  Called each time the window changes.
 *-----------------------------------------------------------------------------
 */

ENTRYPOINT void init_projectiveplane(ModeInfo *mi)
{
  projectiveplanestruct *pp;

  MI_INIT(mi, projectiveplane);
  pp = &projectiveplane[MI_SCREEN(mi)];

  pp->trackballs[0] = gltrackball_init(True);
  pp->trackballs[1] = gltrackball_init(True);
  pp->current_trackball = 0;
  pp->button_pressed = False;

  /* Set the display mode. */
  if (!strcasecmp(mode,"random"))
  {
    pp->display_mode = random() % NUM_DISPLAY_MODES;
  }
  else if (!strcasecmp(mode,"wireframe"))
  {
    pp->display_mode = DISP_WIREFRAME;
  }
  else if (!strcasecmp(mode,"surface"))
  {
    pp->display_mode = DISP_SURFACE;
  }
  else if (!strcasecmp(mode,"transparent"))
  {
    pp->display_mode = DISP_TRANSPARENT;
  }
  else
  {
    pp->display_mode = random() % NUM_DISPLAY_MODES;
  }

  /* Orientation marks don't make sense in wireframe mode. */
  pp->marks = marks;
  if (pp->display_mode == DISP_WIREFRAME)
    pp->marks = False;

  /* Set the appearance. */
  if (!strcasecmp(appear,"random"))
  {
    pp->appearance = random() % NUM_APPEARANCES;
  }
  else if (!strcasecmp(appear,"solid"))
  {
    pp->appearance = APPEARANCE_SOLID;
  }
  else if (!strcasecmp(appear,"distance-bands"))
  {
    pp->appearance = APPEARANCE_DISTANCE_BANDS;
  }
  else if (!strcasecmp(appear,"direction-bands"))
  {
    pp->appearance = APPEARANCE_DIRECTION_BANDS;
  }
  else
  {
    pp->appearance = random() % NUM_APPEARANCES;
  }

  /* Set the color mode. */
  if (!strcasecmp(color_mode,"random"))
  {
    pp->colors = random() % NUM_COLORS;
  }
  else if (!strcasecmp(color_mode,"one-sided"))
  {
    pp->colors = COLORS_ONESIDED;
  }
  else if (!strcasecmp(color_mode,"two-sided"))
  {
    pp->colors = COLORS_TWOSIDED;
  }
  else if (!strcasecmp(color_mode,"distance"))
  {
    pp->colors = COLORS_DISTANCE;
  }
  else if (!strcasecmp(color_mode,"direction"))
  {
    pp->colors = COLORS_DIRECTION;
  }
  else if (!strcasecmp(color_mode,"depth"))
  {
    pp->colors = COLORS_DEPTH;
  }
  else
  {
    pp->colors = random() % NUM_COLORS;
  }

  pp->change_colors = change_colors;

  /* Set the view mode. */
  if (!strcasecmp(view_mode,"random"))
  {
    pp->view = random() % NUM_VIEW_MODES;
  }
  else if (!strcasecmp(view_mode,"walk"))
  {
    pp->view = VIEW_WALK;
  }
  else if (!strcasecmp(view_mode,"turn"))
  {
    pp->view = VIEW_TURN;
  }
  else if (!strcasecmp(view_mode,"walk-turn"))
  {
    pp->view = VIEW_WALKTURN;
  }
  else
  {
    pp->view = random() % NUM_VIEW_MODES;
  }

  /* Set the 3d projection mode. */
  if (!strcasecmp(proj_3d,"random"))
  {
    /* Orthographic projection only makes sense in turn mode. */
    if (pp->view == VIEW_TURN)
      pp->projection_3d = random() % NUM_DISP_3D_MODES;
    else
      pp->projection_3d = DISP_3D_PERSPECTIVE;
  }
  else if (!strcasecmp(proj_3d,"perspective"))
  {
    pp->projection_3d = DISP_3D_PERSPECTIVE;
  }
  else if (!strcasecmp(proj_3d,"orthographic"))
  {
    pp->projection_3d = DISP_3D_ORTHOGRAPHIC;
  }
  else
  {
    /* Orthographic projection only makes sense in turn mode. */
    if (pp->view == VIEW_TURN)
      pp->projection_3d = random() % NUM_DISP_3D_MODES;
    else
      pp->projection_3d = DISP_3D_PERSPECTIVE;
  }

  /* Set the 4d projection mode. */
  if (!strcasecmp(proj_4d,"random"))
  {
    pp->projection_4d = random() % NUM_DISP_4D_MODES;
  }
  else if (!strcasecmp(proj_4d,"perspective"))
  {
    pp->projection_4d = DISP_4D_PERSPECTIVE;
  }
  else if (!strcasecmp(proj_4d,"orthographic"))
  {
    pp->projection_4d = DISP_4D_ORTHOGRAPHIC;
  }
  else
  {
    pp->projection_4d = random() % NUM_DISP_4D_MODES;
  }

  /* Modify the speeds to a useful range in walk-and-turn mode. */
  if (pp->view == VIEW_WALKTURN)
  {
    speed_wx *= 0.2;
    speed_wy *= 0.2;
    speed_wz *= 0.2;
    speed_xy *= 0.2;
    speed_xz *= 0.2;
    speed_yz *= 0.2;
  }

  /* make multiple screens rotate at slightly different rates. */
  pp->speed_scale = 0.9 + frand(0.3);

  if ((pp->glx_context = init_GL(mi)) != NULL)
  {
    reshape_projectiveplane(mi,MI_WIDTH(mi),MI_HEIGHT(mi));
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
ENTRYPOINT void draw_projectiveplane(ModeInfo *mi)
{
  Display          *display = MI_DISPLAY(mi);
  Window           window = MI_WINDOW(mi);
  projectiveplanestruct *pp;

  if (projectiveplane == NULL)
    return;
  pp = &projectiveplane[MI_SCREEN(mi)];

  MI_IS_DRAWN(mi) = True;
  if (!pp->glx_context)
    return;

  glXMakeCurrent(display, window, *pp->glx_context);

  glClearColor(0.0f,0.0f,0.0f,1.0f);
  glClearDepth(1.0f);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();

  display_projectiveplane(mi);

  if (MI_IS_FPS(mi))
    do_fps (mi);

  glFlush();

  glXSwapBuffers(display,window);
}


#ifndef STANDALONE
ENTRYPOINT void change_projectiveplane(ModeInfo *mi)
{
  projectiveplanestruct *pp = &projectiveplane[MI_SCREEN(mi)];

  if (!pp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *pp->glx_context);
  init(mi);
}
#endif /* !STANDALONE */


ENTRYPOINT void free_projectiveplane(ModeInfo *mi)
{
  projectiveplanestruct *pp = &projectiveplane[MI_SCREEN(mi)];
  if (!pp->glx_context) return;
  glXMakeCurrent (MI_DISPLAY(mi), MI_WINDOW(mi), *pp->glx_context);
  gltrackball_free (pp->trackballs[0]);
  gltrackball_free (pp->trackballs[1]);
  if (pp->tex_name) glDeleteTextures (1, &pp->tex_name);
#ifdef HAVE_GLSL
  if (pp->use_shaders)
  {
    glDeleteBuffers(1,&pp->vertex_uv_buffer);
    glDeleteBuffers(1,&pp->vertex_t_buffer);
    glDeleteBuffers(1,&pp->color_buffer);
    glDeleteBuffers(1,&pp->indices_buffer);
    if (pp->shader_program != 0)
    {
      glUseProgram(0);
      glDeleteProgram(pp->shader_program);
    }
  }
#endif /* HAVE_GLSL */
}


XSCREENSAVER_MODULE ("ProjectivePlane", projectiveplane)

#endif /* USE_GL */
