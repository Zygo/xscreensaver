/* romanboy --- Shows a 3d immersion of the real projective plane
   that rotates in 3d or on which you can walk and that can deform
   smoothly between the Roman surface and the Boy surface. */

#if 0
static const char sccsid[] = "@(#)romanboy.c  1.1 14/10/03 xlockmore";
#endif

/* Copyright (c) 2014-2021 Carsten Steger <carsten@mirsanmir.org>. */

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
 * C. Steger - 14/10/03: Initial version
 * C. Steger - 20/01/06: Added the changing colors mode
 * C. Steger - 20/12/19: Added per-fragment shading
 * C. Steger - 20/12/30: Make the shader code work under macOS and iOS
 */

/*
 * This program shows a 3d immersion of the real projective plane that
 * smoothly deforms between the Roman surface and the Boy surface.
 * You can walk on the projective plane or turn in 3d.  The smooth
 * deformation (homotopy) between these two famous immersions of the
 * real projective plane was constructed by François Apéry.
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
 * The immersed projective plane can be projected to the screen either
 * perspectively or orthographically.  When using the walking modes,
 * perspective projection to the screen will be used.
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
 * set to one-sided, two-sided, distance, or direction.  In one-sided
 * mode, the projective plane is drawn with the same color on both
 * "sides."  In two-sided mode (using static colors), the projective
 * plane is drawn with red on one "side" and green on the "other
 * side."  As described above, the projective plane only has one side,
 * so the color jumps from red to green along the line at infinity.
 * This mode enables you to see that the projective plane is
 * non-orientable.  If changing colors are used in two-sided mode,
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
 * displayed with a different color.
 *
 * The rotation speed for each of the three coordinate axes around
 * which the projective plane rotates can be chosen.
 *
 * Furthermore, in the walking mode the walking direction in the 2d
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
 * By default, the immersion of the real projective plane smoothly
 * deforms between the Roman and Boy surfaces.  It is possible to
 * choose the speed of the deformation.  Furthermore, it is possible
 * to switch the deformation off.  It is also possible to determine
 * the initial deformation of the immersion.  This is mostly useful if
 * the deformation is switched off, in which case it will determine
 * the appearance of the surface.
 *
 * As a final option, it is possible to display generalized versions
 * of the immersion discussed above by specifying the order of the
 * surface.  The default surface order of 3 results in the immersion
 * of the real projective described above.  The surface order can be
 * chosen between 2 and 9.  Odd surface orders result in generalized
 * immersions of the real projective plane, while even numbers result
 * in a immersion of a topological sphere (which is orientable).  The
 * most interesting even case is a surface order of 2, which results
 * in an immersion of the halfway model of Morin's sphere eversion (if
 * the deformation is switched off).
 *
 * This program is inspired by François Apéry's book "Models of the
 * Real Projective Plane", Vieweg, 1987.
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
#define DEF_CHANGE_COLORS          "False"
#define DEF_DEFORM                 "True"
#define DEF_PROJECTION             "random"
#define DEF_SPEEDX                 "1.1"
#define DEF_SPEEDY                 "1.3"
#define DEF_SPEEDZ                 "1.5"
#define DEF_WALK_DIRECTION         "83.0"
#define DEF_WALK_SPEED             "20.0"
#define DEF_DEFORM_SPEED           "10.0"
#define DEF_INIT_DEFORM            "1000.0"
#define DEF_SURFACE_ORDER          "3"


#ifdef STANDALONE
# define DEFAULTS           "*delay:      25000 \n" \
                            "*showFPS:    False \n" \
                            "*prefersGLSL: True \n" \

# define release_romanboy 0
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
ModStruct romanboy_description =
{"romanboy", "init_romanboy", "draw_romanboy",
 NULL, "draw_romanboy", "change_romanboy",
 "free_romanboy", &romanboy_opts, 25000, 1, 1, 1, 1.0, 4, "",
 "Rotate a 3d immersion of the real projective plane in 3d or walk on it",
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
static int surface_order;


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
  {"-roman",               ".initDeform",    XrmoptionNoArg,  "0.0" },
  {"-boy",                 ".initDeform",    XrmoptionNoArg,  "1000.0" },
  {"-surface-order",       ".surfaceOrder",  XrmoptionSepArg, 0 },
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
  { &surface_order,  "surfaceOrder",  "SurfaceOrder",  DEF_SURFACE_ORDER,  t_Int }
};

ENTRYPOINT ModeSpecOpt romanboy_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, NULL};


/* Offset by which we walk above the projective plane */
#define DELTAY  0.01

/* Color change speeds */
#define DRHO    0.7
#define DSIGMA  1.1
#define DTAU    1.7

/* Number of subdivisions of the projective plane */
#define NUMU 64
#define NUMV 128

/* Number of subdivisions per band */
#define NUMB 8


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
  int side, dir;
  /* Deformation parameters */
  float dd;
  int defdir;
  /* The type of the generalized Roman-Boy surface */
  int g;
  /* The viewing offset in 3d */
  float offset3d[3];
  /* The 3d coordinates of the projective plane and their normals */
  float *pp;
  float *pn;
  /* The precomputed colors of the projective plane */
  float *col;
  /* The precomputed texture coordinates of the projective plane */
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
  GLint mat_mv_index, mat_p_index, g_index, d_index;
  GLint bool_textures_index, draw_lines_index;
  GLint glbl_ambient_index, lt_ambient_index;
  GLint lt_diffuse_index, lt_specular_index;
  GLint lt_direction_index, lt_halfvect_index;
  GLint front_ambient_index, back_ambient_index;
  GLint front_diffuse_index, back_diffuse_index;
  GLint specular_index, shininess_index;
  GLint texture_sampler_index;
  GLuint vertex_uv_buffer,  vertex_t_buffer;
  GLuint color_buffer, indices_buffer;
  GLint ni, ne, nt;
#endif /* HAVE_GLSL */
} romanboystruct;

static romanboystruct *romanboy = (romanboystruct *) NULL;


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
  "uniform mat4 MatModelView;\n"
  "uniform mat4 MatProj;\n"
  "uniform int G;\n"
  "uniform float D;\n"
  "uniform bool BoolTextures;\n"
  "\n"
  "void main (void)\n"
  "{\n"
  "  const float EPSILON = 1.19e-6f;\n"
  "  const float M_PI = 3.14159265359f;\n"
  "  const float M_SQRT2 = 1.41421356237f;\n"
  "  float g = float(G);\n"
  "  float u = VertexUV.x;\n"
  "  float v = VertexUV.y;\n"
  "  float sqrt2og = M_SQRT2/g;\n"
  "  float h1m1og = 0.5f*(1.0f-1.0f/g);\n"
  "  float gm1 = g-1.0f;\n"
  "  float cu = cos(u);\n"
  "  float su = sin(u);\n"
  "  float cgu = cos(g*u);\n"
  "  float sgu = sin(g*u);\n"
  "  float cgm1u = cos(gm1*u);\n"
  "  float sgm1u = sin(gm1*u);\n"
  "  float cv = cos(v);\n"
  "  float c2v = cos(2.0f*v);\n"
  "  float s2v = sin(2.0f*v);\n"
  "  float cv2 = cv*cv;\n"
  "  float nomx = sqrt2og*cv2*cgm1u+h1m1og*s2v*cu;\n"
  "  float nomy = sqrt2og*cv2*sgm1u-h1m1og*s2v*su;\n"
  "  float nomux = -sqrt2og*cv2*gm1*sgm1u-h1m1og*s2v*su;\n"
  "  float nomuy = sqrt2og*cv2*gm1*cgm1u-h1m1og*s2v*cu;\n"
  "  float nomvx = -sqrt2og*s2v*cgm1u+2.0f*h1m1og*c2v*cu;\n"
  "  float nomvy = -sqrt2og*s2v*sgm1u-2.0f*h1m1og*c2v*su;\n"
  "  float den = 1.0f/(1.0f-0.5f*M_SQRT2*D*s2v*sgu);\n"
  "  float den2 = den*den;\n"
  "  float denu = 0.5f*M_SQRT2*D*g*cgu*s2v;\n"
  "  float denv = M_SQRT2*D*sgu*c2v;\n"
  "  vec3 x = vec3(nomx*den,\n"
  "                nomy*den,\n"
  "                cv2*den);\n"
  "  if (0.5f*M_PI-abs(v) < EPSILON)\n"
  "  {\n"
  "    if (0.5f*M_PI-v < EPSILON)\n"
  "      v = 0.5f*M_PI-EPSILON;\n"
  "    else\n"
  "      v = -0.5f*M_PI+EPSILON;\n"
  "    cv = cos(v);\n"
  "    c2v = cos(2.0f*v);\n"
  "    s2v = sin(2.0f*v);\n"
  "    cv2 = cv*cv;\n"
  "    nomx = sqrt2og*cv2*cgm1u+h1m1og*s2v*cu;\n"
  "    nomy = sqrt2og*cv2*sgm1u-h1m1og*s2v*su;\n"
  "    nomux = -sqrt2og*cv2*gm1*sgm1u-h1m1og*s2v*su;\n"
  "    nomuy = sqrt2og*cv2*gm1*cgm1u-h1m1og*s2v*cu;\n"
  "    nomvx = -sqrt2og*s2v*cgm1u+2.0f*h1m1og*c2v*cu;\n"
  "    nomvy = -sqrt2og*s2v*sgm1u-2.0f*h1m1og*c2v*su;\n"
  "    den = 1.0f/(1.0f-0.5f*M_SQRT2*D*s2v*sgu);\n"
  "    den2 = den*den;\n"
  "    denu = 0.5f*M_SQRT2*D*g*cgu*s2v;\n"
  "    denv = M_SQRT2*D*sgu*c2v;\n"
  "  }\n"
  "  vec3 xu = vec3(nomux*den+nomx*denu*den2,\n"
  "                 nomuy*den+nomy*denu*den2,\n"
  "                 cv2*denu*den2);\n"
  "  vec3 xv = vec3(nomvx*den+nomx*denv*den2,\n"
  "                 nomvy*den+nomy*denv*den2,\n"
  "                 -s2v*den+cv2*denv*den2);\n"
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


/* Compute a fully saturated and bright color based on an angle. */
static void color(romanboystruct *pp, double angle, float mat[3][3],
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


/* Set up the projective plane colors and texture. */
static void setup_roman_boy_color_texture(ModeInfo *mi, double umin,
                                          double umax, double vmin,
                                          double vmax, int numu, int numv)
{
  int i, j, k, g;
  double u, v, ur, vr;
  romanboystruct *pp = &romanboy[MI_SCREEN(mi)];

  g = pp->g;
  ur = umax-umin;
  vr = vmax-vmin;
  for (i=0; i<=numv; i++)
  {
    for (j=0; j<=numu; j++)
    {
      k = i*(numu+1)+j;
      if (pp->appearance != APPEARANCE_DIRECTION_BANDS)
        u = -ur*j/numu+umin;
      else
        u = ur*j/numu+umin;
      v = vr*i/numv+vmin;
      if (!pp->change_colors)
      {
        if (pp->colors == COLORS_DIRECTION)
          color(pp,2.0*M_PI-fmod(2.0*u,2.0*M_PI),NULL,&pp->col[4*k]);
        else /* pp->colors == COLORS_DISTANCE */
          color(pp,v*(5.0/6.0),NULL,&pp->col[4*k]);
      }
      pp->tex[2*k+0] = -16*g*u/(2.0*M_PI);
      if (pp->appearance == APPEARANCE_DISTANCE_BANDS)
        pp->tex[2*k+1] = 32*v/(2.0*M_PI)-0.5;
      else
        pp->tex[2*k+1] = 32*v/(2.0*M_PI);
    }
  }
}


/* Compute the current walk frame, i.e., the coordinate system of the
   point and direction at which the viewer is currently walking on the
   projective plane. */
static void compute_walk_frame(romanboystruct *pp, int g, float d,
                               float radius, float oz, float mat[3][3])
{
  float p[3], pu[3], pv[3], pm[3], n[3], b[3];
  int l, m;
  float u, v;
  float xx[3], xxu[3], xxv[3];
  float r, t;
  float cu, su, cgu, sgu, cgm1u, sgm1u, cv, c2v, s2v, cv2;
  float sqrt2og, h1m1og, gm1, nomx, nomy, nomux, nomuy, nomvx, nomvy;
  float den, den2, denu, denv;

  u = pp->umove;
  v = pp->vmove;
  if (g & 1)
    v = 0.5f*(float)M_PI-0.25f*v;
  else
    v = 0.5f*(float)M_PI-0.5f*v;
  sqrt2og = (float)M_SQRT2/g;
  h1m1og = 0.5f*(1.0f-1.0f/g);
  gm1 = g-1.0f;
  cu = cosf(u);
  su = sinf(u);
  cgu = cosf(g*u);
  sgu = sinf(g*u);
  cgm1u = cosf(gm1*u);
  sgm1u = sinf(gm1*u);
  cv = cosf(v);
  c2v = cosf(2.0f*v);
  s2v = sinf(2.0f*v);
  cv2 = cv*cv;
  nomx = sqrt2og*cv2*cgm1u+h1m1og*s2v*cu;
  nomy = sqrt2og*cv2*sgm1u-h1m1og*s2v*su;
  nomux = -sqrt2og*cv2*gm1*sgm1u-h1m1og*s2v*su;
  nomuy = sqrt2og*cv2*gm1*cgm1u-h1m1og*s2v*cu;
  nomvx = -sqrt2og*s2v*cgm1u+2.0f*h1m1og*c2v*cu;
  nomvy = -sqrt2og*s2v*sgm1u-2.0f*h1m1og*c2v*su;
  den = 1.0f/(1.0f-0.5f*(float)M_SQRT2*d*s2v*sgu);
  den2 = den*den;
  denu = 0.5f*(float)M_SQRT2*d*g*cgu*s2v;
  denv = (float)M_SQRT2*d*sgu*c2v;
  xx[0] = nomx*den;
  xx[1] = nomy*den;
  xx[2] = cv2*den-oz;
  /* Avoid degenerate tangential plane basis vectors. */
  if (0.5f*(float)M_PI-fabsf(v) < 10.0f*(float)FLT_EPSILON)
  {
    if (0.5f*(float)M_PI-v < 10.0f*(float)FLT_EPSILON)
      v = 0.5f*(float)M_PI-10.0f*(float)FLT_EPSILON;
    else
      v = -0.5f*(float)M_PI+10.0f*(float)FLT_EPSILON;
    cv = cosf(v);
    c2v = cosf(2.0f*v);
    s2v = sinf(2.0f*v);
    cv2 = cv*cv;
    nomx = sqrt2og*cv2*cgm1u+h1m1og*s2v*cu;
    nomy = sqrt2og*cv2*sgm1u-h1m1og*s2v*su;
    nomux = -sqrt2og*cv2*gm1*sgm1u-h1m1og*s2v*su;
    nomuy = sqrt2og*cv2*gm1*cgm1u-h1m1og*s2v*cu;
    nomvx = -sqrt2og*s2v*cgm1u+2.0f*h1m1og*c2v*cu;
    nomvy = -sqrt2og*s2v*sgm1u-2.0f*h1m1og*c2v*su;
    den = 1.0f/(1.0f-0.5f*(float)M_SQRT2*d*s2v*sgu);
    den2 = den*den;
    denu = 0.5f*(float)M_SQRT2*d*g*cgu*s2v;
    denv = (float)M_SQRT2*d*sgu*c2v;
  }
  xxu[0] = nomux*den+nomx*denu*den2;
  xxu[1] = nomuy*den+nomy*denu*den2;
  xxu[2] = cv2*denu*den2;
  xxv[0] = nomvx*den+nomx*denv*den2;
  xxv[1] = nomvy*den+nomy*denv*den2;
  xxv[2] = -s2v*den+cv2*denv*den2;
  for (l=0; l<3; l++)
  {
    p[l] = xx[l]*radius;
    pu[l] = xxu[l]*radius;
    pv[l] = xxv[l]*radius;
  }
  n[0] = pu[1]*pv[2]-pu[2]*pv[1];
  n[1] = pu[2]*pv[0]-pu[0]*pv[2];
  n[2] = pu[0]*pv[1]-pu[1]*pv[0];
  t = 1.0f/(pp->side*4.0f*sqrtf(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]));
  n[0] *= t;
  n[1] *= t;
  n[2] *= t;
  pm[0] = pu[0]*pp->dumove-pv[0]*0.25f*pp->dvmove;
  pm[1] = pu[1]*pp->dumove-pv[1]*0.25f*pp->dvmove;
  pm[2] = pu[2]*pp->dumove-pv[2]*0.25f*pp->dvmove;
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
  pp->alpha = atan2f(-n[2],-pm[2])*180.0f/(float)M_PI;
  pp->beta = atan2f(-b[2],sqrtf(b[0]*b[0]+b[1]*b[1]))*180.0f/(float)M_PI;
  pp->delta = atan2f(b[1],-b[0])*180.0f/(float)M_PI;

  /* Compute the rotation that rotates the projective plane in 3D. */
  rotateall(pp->alpha,pp->beta,pp->delta,mat);

  u = pp->umove;
  v = pp->vmove;
  if (g & 1)
    v = 0.5f*(float)M_PI-0.25f*v;
  else
    v = 0.5f*(float)M_PI-0.5f*v;
  sqrt2og = (float)M_SQRT2/g;
  h1m1og = 0.5f*(1.0f-1.0f/g);
  gm1 = g-1.0f;
  cu = cosf(u);
  su = sinf(u);
  sgu = sinf(g*u);
  cgm1u = cosf(gm1*u);
  sgm1u = sinf(gm1*u);
  cv = cosf(v);
  s2v = sinf(2.0f*v);
  cv2 = cv*cv;
  nomx = sqrt2og*cv2*cgm1u+h1m1og*s2v*cu;
  nomy = sqrt2og*cv2*sgm1u-h1m1og*s2v*su;
  den = 1.0f/(1.0f-0.5f*(float)M_SQRT2*d*s2v*sgu);
  xx[0] = nomx*den;
  xx[1] = nomy*den;
  xx[2] = cv2*den-oz;
  for (l=0; l<3; l++)
  {
    r = 0.0;
    for (m=0; m<3; m++)
      r += mat[l][m]*xx[m];
    p[l] = r*radius;
  }

  pp->offset3d[0] = -p[0];
  pp->offset3d[1] = -p[1]-DELTAY;
  pp->offset3d[2] = -p[2];
}


/* Draw a 3d immersion of the projective plane using OpenGL's fixed
   functionality. */
static int roman_boy_ff(ModeInfo *mi, double umin, double umax,
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
  int i, j, k, l, m, o, g;
  float u, v, ur, vr, oz;
  float xx[3], xxu[3], xxv[3];
  float r, s, t;
  float d, dd, radius;
  float cu, su, cgu, sgu, cgm1u, sgm1u, cv, c2v, s2v, cv2;
  float sqrt2og, h1m1og, gm1, nomx, nomy, nomux, nomuy, nomvx, nomvy;
  float den, den2, denu, denv;
  float qu[4], r1[3][3], r2[3][3];
  romanboystruct *pp = &romanboy[MI_SCREEN(mi)];
  int polys;

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  if (pp->projection == DISP_PERSPECTIVE || pp->view == VIEW_WALK)
  {
    if (pp->view == VIEW_WALK)
      gluPerspective(60.0,pp->aspect,0.01,10.0);
    else
      gluPerspective(60.0,pp->aspect,0.1,10.0);
  }
  else
  {
    if (pp->aspect >= 1.0)
      glOrtho(-pp->aspect,pp->aspect,-1.0,1.0,0.1,10.0);
    else
      glOrtho(-1.0,1.0,-1.0/pp->aspect,1.0/pp->aspect,0.1,10.0);
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

  g = pp->g;
  dd = pp->dd;
  d = ((6.0f*dd-15.0f)*dd+10.0f)*dd*dd*dd;
  r = 1.0f+d*d*(1.0f/2.0f+d*d*(1.0f/6.0f+d*d*(1.0f/3.0f)));
  radius = 1.0f/r;
  oz = 0.5f*r;

  if (pp->change_colors)
    rotateall(pp->rho,pp->sigma,pp->tau,matc);

  if (pp->view == VIEW_WALK)
  {
    /* Compute the walk frame. */
    compute_walk_frame(pp,g,d,radius,oz,mat);
  }
  else
  {
    /* Compute the rotation that rotates the projective plane in 3D,
       including the trackball rotations. */
    rotateall(pp->alpha,pp->beta,pp->delta,r1);

    gltrackball_get_quaternion(pp->trackball,qu);
    quat_to_rotmat(qu,r2);

    mult_rotmat(r2,r1,mat);
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

  /* Set up the projective plane coordinates and normals. */
  if (pp->appearance != APPEARANCE_DIRECTION_BANDS)
  {
    for (i=0; i<=numv; i++)
    {
      if (pp->appearance == APPEARANCE_DISTANCE_BANDS &&
          ((i & (NUMB-1)) >= NUMB/4+1) && ((i & (NUMB-1)) < 3*NUMB/4))
        continue;
      for (j=0; j<=numu; j++)
      {
        o = i*(numu+1)+j;
        u = ur*j/numu+umin;
        v = vr*i/numv+vmin;
        if (pp->change_colors)
        {
          /* Compute the colors dynamically. */
          if (pp->colors == COLORS_DIRECTION)
            color(pp,2.0*M_PI-fmod(2.0*u,2.0*M_PI),matc,&pp->col[4*o]);
          else if (pp->colors == COLORS_DISTANCE)
            color(pp,v*(5.0/6.0),matc,&pp->col[4*o]);
        }
        if (g & 1)
          v = 0.5f*(float)M_PI-0.25f*v;
        else
          v = 0.5f*(float)M_PI-0.5f*v;
        sqrt2og = (float)M_SQRT2/g;
        h1m1og = 0.5f*(1.0f-1.0f/g);
        gm1 = g-1.0f;
        cu = cosf(u);
        su = sinf(u);
        cgu = cosf(g*u);
        sgu = sinf(g*u);
        cgm1u = cosf(gm1*u);
        sgm1u = sinf(gm1*u);
        cv = cosf(v);
        c2v = cosf(2.0f*v);
        s2v = sinf(2.0f*v);
        cv2 = cv*cv;
        nomx = sqrt2og*cv2*cgm1u+h1m1og*s2v*cu;
        nomy = sqrt2og*cv2*sgm1u-h1m1og*s2v*su;
        nomux = -sqrt2og*cv2*gm1*sgm1u-h1m1og*s2v*su;
        nomuy = sqrt2og*cv2*gm1*cgm1u-h1m1og*s2v*cu;
        nomvx = -sqrt2og*s2v*cgm1u+2.0f*h1m1og*c2v*cu;
        nomvy = -sqrt2og*s2v*sgm1u-2.0f*h1m1og*c2v*su;
        den = 1.0f/(1.0f-0.5f*(float)M_SQRT2*d*s2v*sgu);
        den2 = den*den;
        denu = 0.5f*(float)M_SQRT2*d*g*cgu*s2v;
        denv = (float)M_SQRT2*d*sgu*c2v;
        xx[0] = nomx*den;
        xx[1] = nomy*den;
        xx[2] = cv2*den-oz;
        /* Avoid degenerate tangential plane basis vectors. */
        if (0.5f*(float)M_PI-fabsf(v) < 10.0f*(float)FLT_EPSILON)
        {
          if (0.5f*(float)M_PI-v < 10.0f*(float)FLT_EPSILON)
            v = 0.5f*(float)M_PI-10.0f*(float)FLT_EPSILON;
          else
            v = -0.5f*(float)M_PI+10.0f*(float)FLT_EPSILON;
          cv = cosf(v);
          c2v = cosf(2.0f*v);
          s2v = sinf(2.0f*v);
          cv2 = cv*cv;
          nomx = sqrt2og*cv2*cgm1u+h1m1og*s2v*cu;
          nomy = sqrt2og*cv2*sgm1u-h1m1og*s2v*su;
          nomux = -sqrt2og*cv2*gm1*sgm1u-h1m1og*s2v*su;
          nomuy = sqrt2og*cv2*gm1*cgm1u-h1m1og*s2v*cu;
          nomvx = -sqrt2og*s2v*cgm1u+2.0f*h1m1og*c2v*cu;
          nomvy = -sqrt2og*s2v*sgm1u-2.0f*h1m1og*c2v*su;
          den = 1.0f/(1.0f-0.5f*(float)M_SQRT2*d*s2v*sgu);
          den2 = den*den;
          denu = 0.5f*(float)M_SQRT2*d*g*cgu*s2v;
          denv = (float)M_SQRT2*d*sgu*c2v;
        }
        xxu[0] = nomux*den+nomx*denu*den2;
        xxu[1] = nomuy*den+nomy*denu*den2;
        xxu[2] = cv2*denu*den2;
        xxv[0] = nomvx*den+nomx*denv*den2;
        xxv[1] = nomvy*den+nomy*denv*den2;
        xxv[2] = -s2v*den+cv2*denv*den2;
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
          p[l] = r*radius+pp->offset3d[l];
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
        pp->pp[3*o+0] = p[0];
        pp->pp[3*o+1] = p[1];
        pp->pp[3*o+2] = p[2];
        pp->pn[3*o+0] = n[0];
        pp->pn[3*o+1] = n[1];
        pp->pn[3*o+2] = n[2];
      }
    }
  }
  else /* pp->appearance == APPEARANCE_DIRECTION_BANDS */
  {
    for (j=0; j<=numu; j++)
    {
      if ((j & (NUMB-1)) >= NUMB/2+1)
        continue;
      for (i=0; i<=numv; i++)
      {
        o = i*(numu+1)+j;
        u = -ur*j/numu+umin;
        v = vr*i/numv+vmin;
        if (pp->change_colors)
        {
          /* Compute the colors dynamically. */
          if (pp->colors == COLORS_DIRECTION)
            color(pp,2.0*M_PI-fmod(2.0*u,2.0*M_PI),matc,&pp->col[4*o]);
          else if (pp->colors == COLORS_DISTANCE)
            color(pp,v*(5.0/6.0),matc,&pp->col[4*o]);
        }
        if (g & 1)
          v = 0.5f*(float)M_PI-0.25f*v;
        else
          v = 0.5f*(float)M_PI-0.5f*v;
        sqrt2og = (float)M_SQRT2/g;
        h1m1og = 0.5f*(1.0f-1.0f/g);
        gm1 = g-1.0f;
        cu = cosf(u);
        su = sinf(u);
        cgu = cosf(g*u);
        sgu = sinf(g*u);
        cgm1u = cosf(gm1*u);
        sgm1u = sinf(gm1*u);
        cv = cosf(v);
        c2v = cosf(2.0f*v);
        s2v = sinf(2.0f*v);
        cv2 = cv*cv;
        nomx = sqrt2og*cv2*cgm1u+h1m1og*s2v*cu;
        nomy = sqrt2og*cv2*sgm1u-h1m1og*s2v*su;
        nomux = -sqrt2og*cv2*gm1*sgm1u-h1m1og*s2v*su;
        nomuy = sqrt2og*cv2*gm1*cgm1u-h1m1og*s2v*cu;
        nomvx = -sqrt2og*s2v*cgm1u+2.0f*h1m1og*c2v*cu;
        nomvy = -sqrt2og*s2v*sgm1u-2.0f*h1m1og*c2v*su;
        den = 1.0f/(1.0f-0.5f*(float)M_SQRT2*d*s2v*sgu);
        den2 = den*den;
        denu = 0.5f*(float)M_SQRT2*d*g*cgu*s2v;
        denv = (float)M_SQRT2*d*sgu*c2v;
        xx[0] = nomx*den;
        xx[1] = nomy*den;
        xx[2] = cv2*den-oz;
        /* Avoid degenerate tangential plane basis vectors. */
        if (0.5f*(float)M_PI-fabsf(v) < 10.0f*(float)FLT_EPSILON)
        {
          if (0.5f*(float)M_PI-v < 10.0f*(float)FLT_EPSILON)
            v = 0.5f*(float)M_PI-10.0f*(float)FLT_EPSILON;
          else
            v = -0.5f*(float)M_PI+10.0f*(float)FLT_EPSILON;
          cv = cosf(v);
          c2v = cosf(2.0f*v);
          s2v = sinf(2.0f*v);
          cv2 = cv*cv;
          nomx = sqrt2og*cv2*cgm1u+h1m1og*s2v*cu;
          nomy = sqrt2og*cv2*sgm1u-h1m1og*s2v*su;
          nomux = -sqrt2og*cv2*gm1*sgm1u-h1m1og*s2v*su;
          nomuy = sqrt2og*cv2*gm1*cgm1u-h1m1og*s2v*cu;
          nomvx = -sqrt2og*s2v*cgm1u+2.0f*h1m1og*c2v*cu;
          nomvy = -sqrt2og*s2v*sgm1u-2.0f*h1m1og*c2v*su;
          den = 1.0f/(1.0f-0.5f*(float)M_SQRT2*d*s2v*sgu);
          den2 = den*den;
          denu = 0.5f*(float)M_SQRT2*d*g*cgu*s2v;
          denv = (float)M_SQRT2*d*sgu*c2v;
        }
        xxu[0] = nomux*den+nomx*denu*den2;
        xxu[1] = nomuy*den+nomy*denu*den2;
        xxu[2] = cv2*denu*den2;
        xxv[0] = nomvx*den+nomx*denv*den2;
        xxv[1] = nomvy*den+nomy*denv*den2;
        xxv[2] = -s2v*den+cv2*denv*den2;
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
          p[l] = r*radius+pp->offset3d[l];
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
        pp->pp[3*o+0] = p[0];
        pp->pp[3*o+1] = p[1];
        pp->pp[3*o+2] = p[2];
        pp->pn[3*o+0] = n[0];
        pp->pn[3*o+1] = n[1];
        pp->pn[3*o+2] = n[2];
      }
    }
  }

  if (pp->appearance != APPEARANCE_DIRECTION_BANDS)
  {
    for (i=0; i<numv; i++)
    {
      if (pp->appearance == APPEARANCE_DISTANCE_BANDS &&
          ((i & (NUMB-1)) >= NUMB/4) && ((i & (NUMB-1)) < 3*NUMB/4))
        continue;
      if (pp->display_mode == DISP_WIREFRAME)
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
          glTexCoord2fv(&pp->tex[2*o]);
          if (pp->colors != COLORS_ONESIDED && pp->colors != COLORS_TWOSIDED)
          {
            glColor3fv(&pp->col[4*o]);
            glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,
                         &pp->col[4*o]);
          }
          glNormal3fv(&pp->pn[3*o]);
          glVertex3fv(&pp->pp[3*o]);
        }
      }
      glEnd();
    }
    polys = 2*numv*(numu+1);
    if (pp->appearance == APPEARANCE_DISTANCE_BANDS)
      polys /= 2;
  }
  else /* pp->appearance == APPEARANCE_DIRECTION_BANDS */
  {
    for (j=0; j<numu; j++)
    {
      if ((j & (NUMB-1)) >= NUMB/2)
        continue;
      if (pp->display_mode == DISP_WIREFRAME)
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
          glTexCoord2fv(&pp->tex[2*o]);
          if (pp->colors != COLORS_ONESIDED && pp->colors != COLORS_TWOSIDED)
          {
            glColor3fv(&pp->col[4*o]);
            glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,
                         &pp->col[4*o]);
          }
          glNormal3fv(&pp->pn[3*o]);
          glVertex3fv(&pp->pp[3*o]);
        }
      }
      glEnd();
    }
    polys = numu*(numv+1);
  }

  return polys;
}


#ifdef HAVE_GLSL

/* Draw a 3d immersion of the projective plane using OpenGL's programmable
   functionality. */
static int roman_boy_pf(ModeInfo *mi, double umin, double umax,
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
  int i, j, k, l, m, o, g;
  float u, v, ur, vr, oz;
  float r;
  float d, dd, radius;
  float qu[4], r1[3][3], r2[3][3];
  GLsizeiptr index_offset;
  romanboystruct *pp = &romanboy[MI_SCREEN(mi)];
  int polys;

  if (!pp->use_shaders)
    return 0;

  g = pp->g;
  dd = pp->dd;
  d = ((6.0f*dd-15.0f)*dd+10.0f)*dd*dd*dd;
  r = 1.0f+d*d*(1.0f/2.0f+d*d*(1.0f/6.0f+d*d*(1.0f/3.0f)));
  radius = 1.0f/r;
  oz = 0.5f*r;

  if (!pp->buffers_initialized)
  {
    /* The u and v values need to be computed once (or each time the value
       of appearance changes, once we support that). */
    ur = umax-umin;
    vr = vmax-vmin;
    for (i=0; i<=numv; i++)
    {
      for (j=0; j<=numu; j++)
      {
        o = i*(numu+1)+j;
        if (pp->appearance != APPEARANCE_DIRECTION_BANDS)
          u = ur*j/numu+umin;
        else
          u = -ur*j/numu+umin;
        v = vr*i/numv+vmin;
        if (g & 1)
          v = 0.5f*(float)M_PI-0.25f*v;
        else
          v = 0.5f*(float)M_PI-0.5f*v;
        pp->uv[2*o+0] = u;
        pp->uv[2*o+1] = v;
      }
    }
    glBindBuffer(GL_ARRAY_BUFFER,pp->vertex_uv_buffer);
    glBufferData(GL_ARRAY_BUFFER,2*(numu+1)*(numv+1)*sizeof(GLfloat),
                 pp->uv,GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);

    glBindBuffer(GL_ARRAY_BUFFER,pp->vertex_t_buffer);
    glBufferData(GL_ARRAY_BUFFER,2*(numu+1)*(numv+1)*sizeof(GLfloat),
                 pp->tex,GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);

    if (!pp->change_colors &&
        pp->colors != COLORS_ONESIDED && pp->colors != COLORS_TWOSIDED)
    {
      glBindBuffer(GL_ARRAY_BUFFER,pp->color_buffer);
      glBufferData(GL_ARRAY_BUFFER,4*(numu+1)*(numv+1)*sizeof(GLfloat),
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
        for (i=0; i<numv; i++)
        {
          if (pp->appearance == APPEARANCE_DISTANCE_BANDS &&
              ((i & (NUMB-1)) >= NUMB/4) && ((i & (NUMB-1)) < 3*NUMB/4))
            continue;
          for (j=0; j<=numu; j++)
          {
            for (k=0; k<=1; k++)
            {
              l = i+k;
              m = j;
              o = l*(numu+1)+m;
              pp->indices[pp->ni++] = o;
            }
          }
          pp->ne++;
        }
        pp->nt = 2*(numu+1);
      }
      else /* pp->appearance == APPEARANCE_DIRECTION_BANDS */
      {
        for (j=0; j<numu; j++)
        {
          if ((j & (NUMB-1)) >= NUMB/2)
            continue;
          for (i=0; i<=numv; i++)
          {
            for (k=0; k<=1; k++)
            {
              l = i;
              m = j+k;
              o = l*(numu+1)+m;
              pp->indices[pp->ni++] = o;
            }
          }
          pp->ne++;
        }
        pp->nt = 2*(numv+1);
      }
    }
    else /* pp->display_mode == DISP_WIREFRAME */
    {
      if (pp->appearance != APPEARANCE_DIRECTION_BANDS)
      {
        for (i=0; i<=numv; i++)
        {
          if (pp->appearance == APPEARANCE_DISTANCE_BANDS &&
              ((i & (NUMB-1)) > NUMB/4) && ((i & (NUMB-1)) < 3*NUMB/4))
            continue;
          if (pp->appearance == APPEARANCE_DISTANCE_BANDS &&
              ((i & (NUMB-1)) == NUMB/4))
          {
            for (j=0; j<numu; j++)
            {
              pp->indices[pp->ni++] = i*(numu+1)+j;
              pp->indices[pp->ni++] = i*(numu+1)+j+1;
            }
            continue;
          }
          for (j=0; j<numu; j++)
          {
            pp->indices[pp->ni++] = i*(numu+1)+j;
            pp->indices[pp->ni++] = i*(numu+1)+j+1;
            if (i < numv)
            {
              pp->indices[pp->ni++] = i*(numu+1)+j;
              pp->indices[pp->ni++] = (i+1)*(numu+1)+j;
            }
          }
        }
      }
      else /* pp->appearance == APPEARANCE_DIRECTION_BANDS */
      {
        for (j=0; j<numu; j++)
        {
          if ((j & (NUMB-1)) > NUMB/2)
            continue;
          if ((j & (NUMB-1)) == NUMB/2)
          {
            for (i=0; i<numv; i++)
            {
              pp->indices[pp->ni++] = i*(numu+1)+j;
              pp->indices[pp->ni++] = (i+1)*(numu+1)+j;
            }
            continue;
          }
          for (i=0; i<=numv; i++)
          {
            pp->indices[pp->ni++] = i*(numu+1)+j;
            pp->indices[pp->ni++] = i*(numu+1)+j+1;
            if (i < numv)
            {
              pp->indices[pp->ni++] = i*(numu+1)+j;
              pp->indices[pp->ni++] = (i+1)*(numu+1)+j;
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
    rotateall(pp->rho,pp->sigma,pp->tau,matc);

  if (pp->view == VIEW_WALK)
  {
    /* Compute the walk frame. */
    compute_walk_frame(pp,g,d,radius,oz,mat);
  }
  else
  {
    /* Compute the rotation that rotates the projective plane in 3D,
       including the trackball rotations. */
    rotateall(pp->alpha,pp->beta,pp->delta,r1);

    gltrackball_get_quaternion(pp->trackball,qu);
    quat_to_rotmat(qu,r2);

    mult_rotmat(r2,r1,mat);
  }

  if (pp->change_colors &&
      (pp->colors == COLORS_DIRECTION || pp->colors == COLORS_DISTANCE))
  {
    ur = umax-umin;
    vr = vmax-vmin;
    for (i=0; i<=numv; i++)
    {
      for (j=0; j<=numu; j++)
      {
        o = i*(numu+1)+j;
        if (pp->appearance != APPEARANCE_DIRECTION_BANDS)
          u = ur*j/numu+umin;
        else
          u = -ur*j/numu+umin;
        v = vr*i/numv+vmin;
        if (pp->colors == COLORS_DIRECTION)
          color(pp,2.0*M_PI-fmod(2.0*u,2.0*M_PI),matc,&pp->col[4*o]);
        else if (pp->colors == COLORS_DISTANCE)
          color(pp,v*(5.0/6.0),matc,&pp->col[4*o]);
      }
    }
  }

  glUseProgram(pp->shader_program);

  glUniform1i(pp->g_index,g);
  glUniform1f(pp->d_index,d);

  glsl_Identity(p_mat);
  if (pp->projection == DISP_PERSPECTIVE || pp->view == VIEW_WALK)
  {
    if (pp->view == VIEW_WALK)
      glsl_Perspective(p_mat,60.0f,pp->aspect,0.01f,10.0f);
    else
      glsl_Perspective(p_mat,60.0f,pp->aspect,0.1f,10.0f);
  }
  else
  {
    if (pp->aspect >= 1.0)
      glsl_Orthographic(p_mat,-pp->aspect,pp->aspect,-1.0f,1.0f,
                        0.1f,10.0f);
    else
      glsl_Orthographic(p_mat,-1.0f,1.0f,-1.0f/pp->aspect,1.0f/pp->aspect,
                        0.1f,10.0f);
  }
  glUniformMatrix4fv(pp->mat_p_index,1,GL_FALSE,p_mat);
  glsl_Identity(rot_mat);
  for (i=0; i<3; i++)
    for (j=0; j<3; j++)
      rot_mat[GLSL__LINCOOR(i,j,4)] = mat[i][j];
  glsl_Identity(mv_mat);
  glsl_Translate(mv_mat,pp->offset3d[0],pp->offset3d[1],pp->offset3d[2]);
  glsl_Scale(mv_mat,radius,radius,radius);
  glsl_MultMatrix(mv_mat,rot_mat);
  glsl_Translate(mv_mat,0.0f,0.0f,-oz);
  glUniformMatrix4fv(pp->mat_mv_index,1,GL_FALSE,mv_mat);

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

  glUniform4fv(pp->front_ambient_index,1,mat_diff_white);
  glUniform4fv(pp->front_diffuse_index,1,mat_diff_white);
  glUniform4fv(pp->back_ambient_index,1,mat_diff_white);
  glUniform4fv(pp->back_diffuse_index,1,mat_diff_white);
  glVertexAttrib4f(pp->color_index,1.0f,1.0f,1.0f,1.0f);

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
  else  /* pp->display_mode == DISP_WIREFRAME */
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
  glUniform1i(pp->bool_textures_index,marks);

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
      glBufferData(GL_ARRAY_BUFFER,4*(numu+1)*(numv+1)*sizeof(GLfloat),
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

  if (pp->appearance != APPEARANCE_DIRECTION_BANDS)
  {
    polys = 2*numv*(numu+1);
    if (pp->appearance == APPEARANCE_DISTANCE_BANDS)
      polys /= 2;
  }
  else /* pp->appearance == APPEARANCE_DIRECTION_BANDS */
  {
    polys = numu*(numv+1);
  }

  return polys;
}

#endif /* HAVE_GLSL */


/* Generate a texture image that shows the orientation reversal. */
static void gen_texture(ModeInfo *mi)
{
  romanboystruct *pp = &romanboy[MI_SCREEN(mi)];

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
  romanboystruct *pp = &romanboy[MI_SCREEN(mi)];
  GLint gl_major, gl_minor, glsl_major, glsl_minor;
  GLboolean gl_gles3;
  const GLchar *vertex_shader_source[3];
  const GLchar *fragment_shader_source[4];

  pp->uv = calloc(2*pp->g*(NUMU+1)*(NUMV+1),sizeof(float));
  pp->indices = calloc(4*pp->g*(NUMU+1)*(NUMV+1),sizeof(float));

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
  pp->mat_mv_index = glGetUniformLocation(pp->shader_program,
                                          "MatModelView");
  pp->mat_p_index = glGetUniformLocation(pp->shader_program,
                                         "MatProj");
  pp->g_index = glGetUniformLocation(pp->shader_program,
                                     "G");
  pp->d_index = glGetUniformLocation(pp->shader_program,
                                     "D");
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
  if (pp->mat_mv_index == -1 || pp->mat_p_index == -1 ||
      pp->g_index == -1 || pp->d_index == -1 ||
      pp->bool_textures_index == -1 || pp->draw_lines_index == -1 ||
      pp->glbl_ambient_index == -1 || pp->lt_ambient_index == -1 ||
      pp->lt_diffuse_index == -1 || pp->lt_specular_index == -1 ||
      pp->lt_direction_index == -1 || pp->lt_halfvect_index == -1 ||
      pp->front_ambient_index == -1 || pp->back_ambient_index == -1 ||
      pp->front_diffuse_index == -1 || pp->back_diffuse_index == -1 ||
      pp->specular_index == -1 || pp->shininess_index == -1 ||
      pp->texture_sampler_index == -1)
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
  romanboystruct *pp = &romanboy[MI_SCREEN(mi)];

  if (deform_speed == 0.0)
    deform_speed = 10.0;

  if (init_deform < 0.0)
    init_deform = 0.0;
  if (init_deform > 1000.0)
    init_deform = 1000.0;

  if (walk_speed == 0.0)
    walk_speed = 20.0;

  if (pp->view == VIEW_TURN)
  {
    pp->alpha = frand(360.0);
    pp->beta = frand(360.0);
    pp->delta = frand(360.0);
  }
  else
  {
    pp->alpha = 0.0;
    pp->beta = 0.0;
    pp->delta = 0.0;
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

  pp->dd = init_deform*0.001;
  pp->defdir = -1;

  pp->rho = frand(360.0);
  pp->sigma = frand(360.0);
  pp->tau = frand(360.0);

  pp->offset3d[0] = 0.0;
  pp->offset3d[1] = 0.0;
  pp->offset3d[2] = -1.8;

  pp->pp = calloc(3*pp->g*(NUMU+1)*(NUMV+1),sizeof(float));
  pp->pn = calloc(3*pp->g*(NUMU+1)*(NUMV+1),sizeof(float));
  pp->col = calloc(4*pp->g*(NUMU+1)*(NUMV+1),sizeof(float));
  pp->tex = calloc(2*pp->g*(NUMU+1)*(NUMV+1),sizeof(float));

  gen_texture(mi);
  setup_roman_boy_color_texture(mi,0.0,2.0*M_PI,0.0,2.0*M_PI,pp->g*NUMU,NUMV);

#ifdef HAVE_GLSL
  init_glsl(mi);
#endif /* HAVE_GLSL */

#ifdef HAVE_ANDROID
  /* glPolygonMode(...,GL_LINE) is not supported for an OpenGL ES 1.1
     context. */
  if (!pp->use_shaders && pp->display_mode == DISP_WIREFRAME)
    pp->display_mode = DISP_SURFACE;
#endif /* HAVE_GLSL */
}


/* Redisplay the projective plane. */
static void display_romanboy(ModeInfo *mi)
{
  romanboystruct *pp = &romanboy[MI_SCREEN(mi)];

  if (!pp->button_pressed)
  {
    if (deform)
    {
      pp->dd += pp->defdir*deform_speed*0.001;
      if (pp->dd < 0.0)
      {
        pp->dd = -pp->dd;
        pp->defdir = -pp->defdir;
      }
      if (pp->dd > 1.0)
      {
        pp->dd = 2.0-pp->dd;
        pp->defdir = -pp->defdir;
      }
    }
    if (pp->view == VIEW_TURN)
    {
      pp->alpha += speed_x * pp->speed_scale;
      if (pp->alpha >= 360.0)
        pp->alpha -= 360.0;
      pp->beta += speed_y * pp->speed_scale;
      if (pp->beta >= 360.0)
        pp->beta -= 360.0;
      pp->delta += speed_z * pp->speed_scale;
      if (pp->delta >= 360.0)
        pp->delta -= 360.0;
    }
    if (pp->view == VIEW_WALK)
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

  gltrackball_rotate(pp->trackball);
#ifdef HAVE_GLSL
  if (pp->use_shaders)
    mi->polygon_count = roman_boy_pf(mi,0.0,2.0*M_PI,0.0,2.0*M_PI,
                                     pp->g*NUMU,NUMV);
  else
#endif /* HAVE_GLSL */
    mi->polygon_count = roman_boy_ff(mi,0.0,2.0*M_PI,0.0,2.0*M_PI,
                                     pp->g*NUMU,NUMV);
}


ENTRYPOINT void reshape_romanboy(ModeInfo *mi, int width, int height)
{
  romanboystruct *pp = &romanboy[MI_SCREEN(mi)];

  pp->WindW = (GLint)width;
  pp->WindH = (GLint)height;
  glViewport(0,0,width,height);
  pp->aspect = (GLfloat)width/(GLfloat)height;
}


ENTRYPOINT Bool romanboy_handle_event(ModeInfo *mi, XEvent *event)
{
  romanboystruct *pp = &romanboy[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress && event->xbutton.button == Button1)
  {
    pp->button_pressed = True;
    gltrackball_start(pp->trackball, event->xbutton.x, event->xbutton.y,
                      MI_WIDTH(mi), MI_HEIGHT(mi));
    return True;
  }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
  {
    pp->button_pressed = False;
    gltrackball_stop(pp->trackball);
    return True;
  }
  else if (event->xany.type == MotionNotify && pp->button_pressed)
  {
    gltrackball_track(pp->trackball, event->xmotion.x, event->xmotion.y,
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
 *    Initialize romanboy.  Called each time the window changes.
 *-----------------------------------------------------------------------------
 */

ENTRYPOINT void init_romanboy(ModeInfo *mi)
{
  romanboystruct *pp;

  MI_INIT (mi, romanboy);
  pp = &romanboy[MI_SCREEN(mi)];

  if (surface_order < 2)
    pp->g = 2;
  else if (surface_order > 9)
    pp->g = 9;
  else
    pp->g = surface_order;

  pp->trackball = gltrackball_init(False);
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

  pp->marks = marks;

  /* Orientation marks don't make sense in wireframe mode. */
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
  else
  {
    pp->view = random() % NUM_VIEW_MODES;
  }

  /* Set the 3d projection mode. */
  if (!strcasecmp(proj,"random"))
  {
    /* Orthographic projection only makes sense in turn mode. */
    if (pp->view == VIEW_TURN)
      pp->projection = random() % NUM_DISP_MODES;
    else
      pp->projection = DISP_PERSPECTIVE;
  }
  else if (!strcasecmp(proj,"perspective"))
  {
    pp->projection = DISP_PERSPECTIVE;
  }
  else if (!strcasecmp(proj,"orthographic"))
  {
    pp->projection = DISP_ORTHOGRAPHIC;
  }
  else
  {
    /* Orthographic projection only makes sense in turn mode. */
    if (pp->view == VIEW_TURN)
      pp->projection = random() % NUM_DISP_MODES;
    else
      pp->projection = DISP_PERSPECTIVE;
  }

  /* make multiple screens rotate at slightly different rates. */
  pp->speed_scale = 0.9 + frand(0.3);

  if ((pp->glx_context = init_GL(mi)) != NULL)
  {
    reshape_romanboy(mi,MI_WIDTH(mi),MI_HEIGHT(mi));
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
ENTRYPOINT void draw_romanboy(ModeInfo *mi)
{
  Display *display = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  romanboystruct *pp;

  if (romanboy == NULL)
    return;
  pp = &romanboy[MI_SCREEN(mi)];

  MI_IS_DRAWN(mi) = True;
  if (!pp->glx_context)
    return;

  glXMakeCurrent(display, window, *pp->glx_context);

  glClearColor(0.0f,0.0f,0.0f,1.0f);
  glClearDepth(1.0f);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();

  display_romanboy(mi);

  if (MI_IS_FPS(mi))
    do_fps (mi);

  glFlush();

  glXSwapBuffers(display,window);
}


#ifndef STANDALONE
ENTRYPOINT void change_romanboy(ModeInfo *mi)
{
  romanboystruct *pp = &romanboy[MI_SCREEN(mi)];

  if (!pp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *pp->glx_context);
  init(mi);
}
#endif /* !STANDALONE */


ENTRYPOINT void free_romanboy(ModeInfo *mi)
{
  romanboystruct *pp = &romanboy[MI_SCREEN(mi)];

  if (!pp->glx_context) return;
  glXMakeCurrent (MI_DISPLAY(mi), MI_WINDOW(mi), *pp->glx_context);

  if (pp->pp) free(pp->pp);
  if (pp->pn) free(pp->pn);
  if (pp->col) free(pp->col);
  if (pp->tex) free(pp->tex);
  gltrackball_free (pp->trackball);
  if (pp->tex_name) glDeleteTextures (1, &pp->tex_name);
#ifdef HAVE_GLSL
  if (pp->uv) free(pp->uv);
  if (pp->indices) free(pp->indices);
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


XSCREENSAVER_MODULE ("RomanBoy", romanboy)

#endif /* USE_GL */
