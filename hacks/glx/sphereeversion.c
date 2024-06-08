/* sphereeversion --- Shows a sphere eversion, i.e., a smooth deformation
   (homotopy) that turns a sphere inside out.  During the eversion, the
   deformed sphere is allowed to intersect itself transversally.  However,
   no creases or pinch points are allowed to occur. */

#if 0
static const char sccsid[] = "@(#)sphereeversion.c  1.1 20/03/22 xlockmore";
#endif

/* Copyright (c) 2020-2022 Carsten Steger <carsten@mirsanmir.org>. */

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
 * C. Steger - 20/03/22: Initial version
 * C. Steger - 20/04/10: Added blending between visualization modes
 * C. Steger - 20/06/01: Removed blending because accumulation buffers have
 * *                     been deprecated since OpenGL 3.0
 * C. Steger - 20/07/26: Make the polygon offset work under OpenGL ES
 * C. Steger - 20/08/03: Add an easing function for one part of the animation
 * C. Steger - 20/10/11: Add easing functions for more parts of the animation
 * C. Steger - 21/01/03: Added per-fragment shading
 * C. Steger - 21/01/05: Added blending between visualization modes using
 *                       multiple render passes
 * C. Steger - 21/12/23: Added the earth color mode
 * C. Steger - 22/02/25: Added the corrugations sphere eversion
 */

/*
 * This program shows a sphere eversion, i.e., a smooth deformation
 * (homotopy) that turns a sphere inside out.  During the eversion,
 * the deformed sphere is allowed to intersect itself transversally.
 * However, no creases or pinch points are allowed to occur.
 *
 * Two sversion methods are supported: analytic and corrugations.  For
 * a detailed description of the two methods, see the files
 * sphereeversion-analytic.c and sphereeversion-corrugations.c.
 */

#define DEF_EVERSION_METHOD        "random"
#define DEF_DISPLAY_MODE           "random"
#define DEF_APPEARANCE             "random"
#define DEF_GRATICULE              "random"
#define DEF_COLORS                 "random"
#define DEF_PROJECTION             "random"
#define DEF_SPEEDX                 "0.0"
#define DEF_SPEEDY                 "0.0"
#define DEF_SPEEDZ                 "0.0"
#define DEF_DEFORM_SPEED           "10.0"
#define DEF_SURFACE_ORDER          "random"
#define DEF_LUNES                  "lunes-8"
#define DEF_HEMISPHERES            "hemispheres-2"


#ifdef STANDALONE
# define DEFAULTS           "*delay:      10000 \n" \
                            "*showFPS:    False \n" \
                            "*prefersGLSL: True \n" \

# define release_sphereeversion 0
# include "xlockmore.h"         /* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"             /* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL

#include "sphereeversion.h"

#ifndef TEXTURE_MAX_ANISOTROPY_EXT
#define TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif
#ifndef MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

#include "images/gen/earth_png.h"
#include "images/gen/earth_night_png.h"
#include "images/gen/earth_water_png.h"
#include "ximage-loader.h"

#include <float.h>


#ifdef USE_MODULES
ModStruct sphereeversion_description =
{"sphereeversion", "init_sphereeversion", "draw_sphereeversion",
 NULL, "draw_sphereeversion", "change_sphereeversion",
 "free_sphereeversion", &sphereeversion_opts, 25000, 1, 1, 1, 1.0, 4, "",
 "Show a sphere eversion", 0, NULL};

#endif


char *eversion_method;
char *mode;
char *appear;
char *color_mode;
char *graticule;
char *proj;
float speed_x;
float speed_y;
float speed_z;
float deform_speed;
char *surface_order;
char *lunes;
char *hemispheres;


static XrmOptionDescRec opts[] =
{
  {"-eversion-method",     ".eversionMethod", XrmoptionSepArg, 0 },
  {"-analytic",            ".eversionMethod", XrmoptionNoArg,  "analytic" },
  {"-corrugations",        ".eversionMethod", XrmoptionNoArg,  "corrugations" },
  {"-mode",                ".displayMode",    XrmoptionSepArg, 0 },
  {"-surface",             ".displayMode",    XrmoptionNoArg,  "surface" },
  {"-transparent",         ".displayMode",    XrmoptionNoArg,  "transparent" },
  {"-appearance",          ".appearance",     XrmoptionSepArg, 0 },
  {"-solid",               ".appearance",     XrmoptionNoArg,  "solid" },
  {"-parallel-bands",      ".appearance",     XrmoptionNoArg,  "parallel-bands" },
  {"-meridian-bands",      ".appearance",     XrmoptionNoArg,  "meridian-bands" },
  {"-colors",              ".colors",         XrmoptionSepArg, 0 },
  {"-twosided-colors",     ".colors",         XrmoptionNoArg,  "two-sided" },
  {"-parallel-colors",     ".colors",         XrmoptionNoArg,  "parallel" },
  {"-meridian-colors",     ".colors",         XrmoptionNoArg,  "meridian" },
  {"-earth-colors"   ,     ".colors",         XrmoptionNoArg,  "earth" },
  {"-projection",          ".projection",     XrmoptionSepArg, 0 },
  {"-perspective",         ".projection",     XrmoptionNoArg,  "perspective" },
  {"-orthographic",        ".projection",     XrmoptionNoArg,  "orthographic" },
  {"-graticule",           ".graticule",      XrmoptionSepArg, 0 },
  {"-speed-x",             ".speedx",         XrmoptionSepArg, 0 },
  {"-speed-y",             ".speedy",         XrmoptionSepArg, 0 },
  {"-speed-z",             ".speedz",         XrmoptionSepArg, 0 },
  {"-deformation-speed",   ".deformSpeed",    XrmoptionSepArg, 0 },
  {"-surface-order",       ".surfaceOrder",   XrmoptionSepArg, 0 },
  {"-lunes-1",             ".lunes",          XrmoptionNoArg,  "lunes-1" },
  {"-lunes-2",             ".lunes",          XrmoptionNoArg,  "lunes-2" },
  {"-lunes-4",             ".lunes",          XrmoptionNoArg,  "lunes-4" },
  {"-lunes-8",             ".lunes",          XrmoptionNoArg,  "lunes-8" },
  {"-hemispheres-1",       ".hemispheres",    XrmoptionNoArg,  "hemispheres-1" },
  {"-hemispheres-2",       ".hemispheres",    XrmoptionNoArg,  "hemispheres-2" },
};

static argtype vars[] =
{
  { &eversion_method, "eversionMethod", "EversionMethod", DEF_EVERSION_METHOD, t_String },
  { &mode,            "displayMode",    "DisplayMode",    DEF_DISPLAY_MODE,    t_String },
  { &appear,          "appearance",     "Appearance",     DEF_APPEARANCE,      t_String },
  { &color_mode,      "colors",         "Colors",         DEF_COLORS,          t_String },
  { &proj,            "projection",     "Projection",     DEF_PROJECTION,      t_String },
  { &graticule,       "graticule",      "Graticule",      DEF_GRATICULE,       t_String },
  { &speed_x,         "speedx",         "Speedx",         DEF_SPEEDX,          t_Float},
  { &speed_y,         "speedy",         "Speedy",         DEF_SPEEDY,          t_Float},
  { &speed_z,         "speedz",         "Speedz",         DEF_SPEEDZ,          t_Float},
  { &deform_speed,    "deformSpeed",    "DeformSpeed",    DEF_DEFORM_SPEED,    t_Float},
  { &surface_order,   "surfaceOrder",   "SurfaceOrder",   DEF_SURFACE_ORDER,   t_String },
  { &lunes,           "lunes",          "Lunes",          DEF_LUNES,           t_String },
  { &hemispheres,     "hemispheres",    "Hemispheres",    DEF_HEMISPHERES,     t_String },
};

ENTRYPOINT ModeSpecOpt sphereeversion_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, NULL};


sphereeversionstruct *sphereeversion = (sphereeversionstruct *) NULL;


/*
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 * Functions that are used in both sphere eversion methods
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 */


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
   They are concatenated by glShaderSource in the function init_glsl(). */
static const GLchar *poly_vertex_shader_attribs_2_1 =
  "attribute vec3 VertexPosition;\n"
  "attribute vec3 VertexNormal;\n"
  "attribute vec4 VertexColorF;\n"
  "attribute vec4 VertexColorB;\n"
  "attribute vec4 VertexTexCoord;\n"
  "\n"
  "varying vec3 Normal;\n"
  "varying vec4 ColorF;\n"
  "varying vec4 ColorB;\n"
  "varying vec4 TexCoord;\n"
  "\n";
static const GLchar *poly_vertex_shader_attribs_3_0 =
  "in vec3 VertexPosition;\n"
  "in vec3 VertexNormal;\n"
  "in vec4 VertexColorF;\n"
  "in vec4 VertexColorB;\n"
  "in vec4 VertexTexCoord;\n"
  "\n"
  "out vec3 Normal;\n"
  "out vec4 ColorF;\n"
  "out vec4 ColorB;\n"
  "out vec4 TexCoord;\n"
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
  "varying vec4 TexCoord;\n"
  "\n";
static const GLchar *poly_fragment_shader_attribs_3_0 =
  "in vec3 Normal;\n"
  "in vec4 ColorF;\n"
  "in vec4 ColorB;\n"
  "in vec4 TexCoord;\n"
  "\n"
  "out vec4 FragColor;\n"
  "\n";
static const GLchar *poly_fragment_shader_main =
  "uniform vec4 LtGlblAmbient;\n"
  "uniform vec4 LtAmbient, LtDiffuse, LtSpecular;\n"
  "uniform vec3 LtDirection, LtHalfVector;\n"
  "uniform vec4 MatFrontAmbient, MatBackAmbient;\n"
  "uniform vec4 MatFrontDiffuse, MatBackDiffuse;\n"
  "uniform vec4 MatSpecular;\n"
  "uniform float MatShininess;\n"
  "uniform bool BoolTextures;\n"
  "uniform sampler2D TextureSamplerFront;"
  "uniform sampler2D TextureSamplerBack;"
  "uniform sampler2D TextureSamplerWater;"
  "\n"
  "void main (void)\n"
  "{\n"
  "  vec3 normalDirection;\n"
  "  vec4 ambientColor, diffuseColor, sceneColor;\n"
  "  vec4 ambientLighting, diffuseReflection, specularReflection, color;\n"
  "  float ndotl, ndoth, pf;\n"
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
  "    if (gl_FrontFacing)\n"
  "    {\n"
  "      color *= texture2D(TextureSamplerFront,TexCoord.st);\n"
  "      specularReflection *= texture2D(TextureSamplerWater,TexCoord.st);\n"
  "    }\n"
  "    else\n"
  "    {\n"
  "      color *= texture2D(TextureSamplerBack,TexCoord.st);\n"
  "      specularReflection *= 0.0f;\n"
  "    }\n"
  "  }\n"
  "  color += specularReflection;\n"
  "  gl_FragColor = clamp(color,0.0f,1.0f);\n"
  "}\n";
static const GLchar *poly_fragment_shader_out_3_0 =
  "  if (BoolTextures)\n"
  "  {\n"
  "    if (gl_FrontFacing)\n"
  "    {\n"
  "      color *= texture(TextureSamplerFront,TexCoord.st);\n"
  "      specularReflection *= texture(TextureSamplerWater,TexCoord.st);\n"
  "    }\n"
  "    else\n"
  "    {\n"
  "      color *= texture(TextureSamplerBack,TexCoord.st);\n"
  "      specularReflection *= 0.0f;\n"
  "    }\n"
  "  }\n"
  "  color += specularReflection;\n"
  "  FragColor = clamp(color,0.0f,1.0f);\n"
  "}\n";

/* The vertex shader code is composed of code fragments that depend on
   the OpenGL version and code fragments that are version-independent.
   They are concatenated by glsl_CompileAndLinkShaders in the function
   init_glsl(). */
static const GLchar *line_vertex_shader_attribs_2_1 =
  "attribute vec3 VertexPosition;\n"
  "\n";
static const GLchar *line_vertex_shader_attribs_3_0 =
  "in vec3 VertexPosition;\n"
  "\n";
static const GLchar *line_vertex_shader_main =
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
static const GLchar *line_fragment_shader_attribs_2_1 =
  "";
static const GLchar *line_fragment_shader_attribs_3_0 =
  "out vec4 FragColor;\n"
  "\n";
static const GLchar *line_fragment_shader_main =
  "uniform vec4 LineColor;\n"
  "\n"
  "void main (void)\n"
  "{\n";
static const GLchar *line_fragment_shader_out_2_1 =
  "  gl_FragColor = LineColor;\n"
  "}\n";
static const GLchar *line_fragment_shader_out_3_0 =
  "  FragColor = LineColor;\n"
  "}\n";

/* The vertex shader code is composed of code fragments that depend on
   the OpenGL version and code fragments that are version-independent.
   They are concatenated by glsl_CompileAndLinkShaders in the function
   init_glsl(). */
static const GLchar *blend_vertex_shader_attribs_2_1 =
  "attribute vec2 VertexP;\n"
  "attribute vec2 VertexT;\n"
  "\n"
  "varying vec2 TexCoord;\n"
  "\n";
static const GLchar *blend_vertex_shader_attribs_3_0 =
  "in vec2 VertexP;\n"
  "in vec2 VertexT;\n"
  "\n"
  "out vec2 TexCoord;\n"
  "\n";
static const GLchar *blend_vertex_shader_main =
  "void main (void)\n"
  "{\n"
  "  gl_Position = vec4(VertexP,0.0f,1.0f);\n"
  "  TexCoord = VertexT;\n"
  "}\n";

/* The fragment shader code is composed of code fragments that depend on
   the OpenGL version and code fragments that are version-independent.
   They are concatenated by glsl_CompileAndLinkShaders in the function
   init_glsl(). */
static const GLchar *blend_fragment_shader_attribs_2_1 =
  "varying vec2 TexCoord;\n"
  "\n";
static const GLchar *blend_fragment_shader_attribs_3_0 =
  "in vec2 TexCoord;\n"
  "\n"
  "out vec4 FragColor;\n"
  "\n";
static const GLchar *blend_fragment_shader_main =
  "uniform sampler2D TextureSampler0;"
  "uniform sampler2D TextureSampler1;"
  "uniform float T;"
  "\n"
  "void main (void)\n"
  "{\n";
static const GLchar *blend_fragment_shader_out_2_1 =
  "  vec3 Color0 = texture2D(TextureSampler0,TexCoord.st).rgb;\n"
  "  vec3 Color1 = texture2D(TextureSampler1,TexCoord.st).rgb;\n"
  "  gl_FragColor = vec4(T*Color0+(1.0f-T)*Color1,1.0f);\n"
  "}\n";
static const GLchar *blend_fragment_shader_out_3_0 =
  "  vec3 Color0 = texture(TextureSampler0,TexCoord.st).rgb;\n"
  "  vec3 Color1 = texture(TextureSampler1,TexCoord.st).rgb;\n"
  "  FragColor = vec4(T*Color0+(1.0f-T)*Color1,1.0f);\n"
  "}\n";

#endif /* HAVE_GLSL */


/* Add a rotation around the x-axis to the matrix m. */
void rotatex(float m[3][3], float phi)
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
void rotatey(float m[3][3], float phi)
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
void rotatez(float m[3][3], float phi)
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
void rotateall(float al, float be, float de, float m[3][3])
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
void mult_rotmat(float m[3][3], float n[3][3], float o[3][3])
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


/* Compute 3D rotation angles from a unit quaternion. */
void quat_to_angles(float q[4], float *alpha, float *beta, float *delta)
{
  float r00, r01, r02, r12, r22;

  r00 = q[0]*q[0]+q[1]*q[1]-q[2]*q[2]-q[3]*q[3];
  r01 = 2.0f*(q[1]*q[2]-q[0]*q[3]);
  r02 = 2.0f*(q[1]*q[3]+q[0]*q[2]);
  r12 = 2.0f*(q[2]*q[3]-q[0]*q[1]);
  r22 = q[0]*q[0]-q[1]*q[1]-q[2]*q[2]+q[3]*q[3];

  *alpha = atan2f(-r12,r22)*180.0f/M_PI_F;
  *beta = atan2f(r02,sqrtf(r00*r00+r01*r01))*180.0f/M_PI_F;
  *delta = atan2f(-r01,r00)*180.0f/M_PI_F;
}


/* Compute a quaternion from angles in degrees. */
void angles_to_quat(float alpha, float beta, float delta, float p[4])
{
  alpha *= M_PI_F/180.0f;
  beta *= M_PI_F/180.0f;
  delta *= M_PI_F/180.0f;
  p[0] = (cosf(0.5f*alpha)*cosf(0.5f*beta)*cosf(0.5f*delta)-
          sinf(0.5f*alpha)*sinf(0.5f*beta)*sinf(0.5f*delta));
  p[1] = (sinf(0.5f*alpha)*cosf(0.5f*beta)*cosf(0.5f*delta)+
          cosf(0.5f*alpha)*sinf(0.5f*beta)*sinf(0.5f*delta));
  p[2] = (cosf(0.5f*alpha)*sinf(0.5f*beta)*cosf(0.5f*delta)-
          sinf(0.5f*alpha)*cosf(0.5f*beta)*sinf(0.5f*delta));
  p[3] = (cosf(0.5f*alpha)*cosf(0.5f*beta)*sinf(0.5f*delta)+
          sinf(0.5f*alpha)*sinf(0.5f*beta)*cosf(0.5f*delta));
}


/* Perform a spherical linear interpolation between two quaternions. */
void quat_slerp(float t, float qs[4], float qe[4], float q[4])
{
  double cos_t, sin_t, alpha, beta, theta, phi, l;

  alpha = t;
  cos_t = qs[0]*qe[0]+qs[1]*qe[1]+qs[2]*qe[2]+qs[3]*qe[3];
  if (1.0-cos_t < FLT_EPSILON)
  {
    beta = 1.0-alpha;
  }
  else
  {
    theta = acos(cos_t);
    phi = theta;
    sin_t = sin(theta);
    beta = sin(theta-alpha*phi)/sin_t;
    alpha = sin(alpha*phi)/sin_t;
  }
  q[0] = beta*qs[0]+alpha*qe[0];
  q[1] = beta*qs[1]+alpha*qe[1];
  q[2] = beta*qs[2]+alpha*qe[2];
  q[3] = beta*qs[3]+alpha*qe[3];
  l = 1.0/sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);
  q[0] *= l;
  q[1] *= l;
  q[2] *= l;
  q[3] *= l;
}


/* Compute a fully saturated and bright color based on an angle and a color
   rotation matrix. */
void color(sphereeversionstruct *se, float angle, const float mat[3][3],
           float colf[4], float colb[4])
{
  float ca, sa;
  float m;

  ca = cosf(angle);
  sa = sinf(angle);

  colf[0] = ca*mat[0][0]+sa*mat[0][2];
  colf[1] = ca*mat[1][0]+sa*mat[1][2];
  colf[2] = ca*mat[2][0]+sa*mat[2][2];
  m = 0.5f/fmaxf(fmaxf(fabsf(colf[0]),fabsf(colf[1])),fabsf(colf[2]));
  colf[0] = m*colf[0]+0.5f;
  colf[1] = m*colf[1]+0.5f;
  colf[2] = m*colf[2]+0.5f;
  if (se->display_mode[0] == DISP_TRANSPARENT)
    colf[3] = 0.7f;
  else
    colf[3] = 1.0f;

  colb[0] = -ca*mat[0][1]-sa*mat[0][2];
  colb[1] = -ca*mat[1][1]-sa*mat[1][2];
  colb[2] = -ca*mat[2][1]-sa*mat[2][2];
  m = 0.5f/fmaxf(fmaxf(fabsf(colb[0]),fabsf(colb[1])),fabsf(colb[2]));
  colb[0] = m*colb[0]+0.5f;
  colb[1] = m*colb[1]+0.5f;
  colb[2] = m*colb[2]+0.5f;
  if (se->display_mode[0] == DISP_TRANSPARENT)
    colb[3] = 0.7f;
  else
    colb[3] = 1.0f;
}


#ifdef HAVE_GLSL

/* Set up and enable texturing on our object */
void setup_xpm_texture(ModeInfo *mi, const unsigned char *data,
                       unsigned long size)
{
  sphereeversionstruct *se = &sphereeversion[MI_SCREEN(mi)];
  XImage *image;
  char *gl_ext;
  GLboolean have_aniso;
  GLint max_aniso, aniso;

  image = image_data_to_ximage(MI_DISPLAY(mi),MI_VISUAL(mi),data,size);
  glEnable(GL_TEXTURE_2D);
  glPixelStorei(GL_UNPACK_ALIGNMENT,1);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,image->width,image->height,0,GL_RGBA,
               GL_UNSIGNED_BYTE,image->data);
  if (se->use_mipmaps)
    glGenerateMipmap(GL_TEXTURE_2D);
  /* Limit the maximum mipmap level to 4 to avoid texture interpolation
     artifacts at the poles. */
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,4);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  if (se->use_mipmaps)
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
  else
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  gl_ext = (char *)glGetString(GL_EXTENSIONS);
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
void gen_textures(ModeInfo *mi)
{
  sphereeversionstruct *se = &sphereeversion[MI_SCREEN(mi)];

  glGenTextures(3,se->tex_names);

  /* Set up the earth by day texture. */
  glBindTexture(GL_TEXTURE_2D,se->tex_names[0]);
  setup_xpm_texture(mi,earth_png,sizeof(earth_png));

  /* Set up the earth by night texture. */
  glBindTexture(GL_TEXTURE_2D,se->tex_names[1]);
  setup_xpm_texture(mi,earth_night_png,sizeof(earth_night_png));

  /* Set up the earth water texture. */
  glBindTexture(GL_TEXTURE_2D,se->tex_names[2]);
  setup_xpm_texture(mi,earth_water_png,sizeof(earth_water_png));

  glBindTexture(GL_TEXTURE_2D,0);
}


void init_glsl(ModeInfo *mi)
{
  sphereeversionstruct *se = &sphereeversion[MI_SCREEN(mi)];
  GLint gl_major, gl_minor, glsl_major, glsl_minor;
  GLboolean gl_gles3;
  int i, wt, ht;
  const GLchar *poly_vertex_shader_source[3];
  const GLchar *poly_fragment_shader_source[4];
  const GLchar *line_vertex_shader_source[3];
  const GLchar *line_fragment_shader_source[4];
  const GLchar *blend_vertex_shader_source[3];
  const GLchar *blend_fragment_shader_source[4];

  /* Determine whether to use shaders to render the sphere eversion. */
  se->use_shaders = False;
  se->use_mipmaps = False;
  se->buffers_initialized = False;
  se->poly_shader_program = 0;
  se->line_shader_program = 0;
  se->blend_shader_program = 0;
  se->max_tex_size = -1;

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

      line_vertex_shader_source[0] = shader_version_2_1;
      line_vertex_shader_source[1] = line_vertex_shader_attribs_2_1;
      line_vertex_shader_source[2] = line_vertex_shader_main;
      line_fragment_shader_source[0] = shader_version_2_1;
      line_fragment_shader_source[1] = line_fragment_shader_attribs_2_1;
      line_fragment_shader_source[2] = line_fragment_shader_main;
      line_fragment_shader_source[3] = line_fragment_shader_out_2_1;

      blend_vertex_shader_source[0] = shader_version_2_1;
      blend_vertex_shader_source[1] = blend_vertex_shader_attribs_2_1;
      blend_vertex_shader_source[2] = blend_vertex_shader_main;
      blend_fragment_shader_source[0] = shader_version_2_1;
      blend_fragment_shader_source[1] = blend_fragment_shader_attribs_2_1;
      blend_fragment_shader_source[2] = blend_fragment_shader_main;
      blend_fragment_shader_source[3] = blend_fragment_shader_out_2_1;
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

      line_vertex_shader_source[0] = shader_version_3_0;
      line_vertex_shader_source[1] = line_vertex_shader_attribs_3_0;
      line_vertex_shader_source[2] = line_vertex_shader_main;
      line_fragment_shader_source[0] = shader_version_3_0;
      line_fragment_shader_source[1] = line_fragment_shader_attribs_3_0;
      line_fragment_shader_source[2] = line_fragment_shader_main;
      line_fragment_shader_source[3] = line_fragment_shader_out_3_0;

      blend_vertex_shader_source[0] = shader_version_3_0;
      blend_vertex_shader_source[1] = blend_vertex_shader_attribs_3_0;
      blend_vertex_shader_source[2] = blend_vertex_shader_main;
      blend_fragment_shader_source[0] = shader_version_3_0;
      blend_fragment_shader_source[1] = blend_fragment_shader_attribs_3_0;
      blend_fragment_shader_source[2] = blend_fragment_shader_main;
      blend_fragment_shader_source[3] = blend_fragment_shader_out_3_0;
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

    line_vertex_shader_source[0] = shader_version_3_0_es;
    line_vertex_shader_source[1] = line_vertex_shader_attribs_3_0;
    line_vertex_shader_source[2] = line_vertex_shader_main;
    line_fragment_shader_source[0] = shader_version_3_0_es;
    line_fragment_shader_source[1] = line_fragment_shader_attribs_3_0;
    line_fragment_shader_source[2] = line_fragment_shader_main;
    line_fragment_shader_source[3] = line_fragment_shader_out_3_0;

    blend_vertex_shader_source[0] = shader_version_3_0_es;
    blend_vertex_shader_source[1] = blend_vertex_shader_attribs_3_0;
    blend_vertex_shader_source[2] = blend_vertex_shader_main;
    blend_fragment_shader_source[0] = shader_version_3_0_es;
    blend_fragment_shader_source[1] = blend_fragment_shader_attribs_3_0;
    blend_fragment_shader_source[2] = blend_fragment_shader_main;
    blend_fragment_shader_source[3] = blend_fragment_shader_out_3_0;
  }

  if (!glsl_CompileAndLinkShaders(3,poly_vertex_shader_source,
                                  4,poly_fragment_shader_source,
                                  &se->poly_shader_program))
    return;
  se->poly_pos_index = glGetAttribLocation(se->poly_shader_program,
                                           "VertexPosition");
  se->poly_normal_index = glGetAttribLocation(se->poly_shader_program,
                                              "VertexNormal");
  se->poly_colorf_index = glGetAttribLocation(se->poly_shader_program,
                                              "VertexColorF");
  se->poly_colorb_index = glGetAttribLocation(se->poly_shader_program,
                                              "VertexColorB");
  se->poly_vertex_tex_index = glGetAttribLocation(se->poly_shader_program,
                                                  "VertexTexCoord");
  se->poly_mv_index = glGetUniformLocation(se->poly_shader_program,
                                           "MatModelView");
  se->poly_proj_index = glGetUniformLocation(se->poly_shader_program,
                                             "MatProj");
  se->poly_glbl_ambient_index = glGetUniformLocation(se->poly_shader_program,
                                                     "LtGlblAmbient");
  se->poly_lt_ambient_index = glGetUniformLocation(se->poly_shader_program,
                                                   "LtAmbient");
  se->poly_lt_diffuse_index = glGetUniformLocation(se->poly_shader_program,
                                                   "LtDiffuse");
  se->poly_lt_specular_index = glGetUniformLocation(se->poly_shader_program,
                                                    "LtSpecular");
  se->poly_lt_direction_index = glGetUniformLocation(se->poly_shader_program,
                                                     "LtDirection");
  se->poly_lt_halfvect_index = glGetUniformLocation(se->poly_shader_program,
                                                    "LtHalfVector");
  se->poly_front_ambient_index = glGetUniformLocation(se->poly_shader_program,
                                                      "MatFrontAmbient");
  se->poly_back_ambient_index = glGetUniformLocation(se->poly_shader_program,
                                                     "MatBackAmbient");
  se->poly_front_diffuse_index = glGetUniformLocation(se->poly_shader_program,
                                                      "MatFrontDiffuse");
  se->poly_back_diffuse_index = glGetUniformLocation(se->poly_shader_program,
                                                     "MatBackDiffuse");
  se->poly_specular_index = glGetUniformLocation(se->poly_shader_program,
                                                 "MatSpecular");
  se->poly_shininess_index = glGetUniformLocation(se->poly_shader_program,
                                                  "MatShininess");
  se->poly_bool_textures_index = glGetUniformLocation(se->poly_shader_program,
                                                      "BoolTextures");
  se->poly_tex_samp_f_index = glGetUniformLocation(se->poly_shader_program,
                                                   "TextureSamplerFront");
  se->poly_tex_samp_b_index = glGetUniformLocation(se->poly_shader_program,
                                                   "TextureSamplerBack");
  se->poly_tex_samp_w_index = glGetUniformLocation(se->poly_shader_program,
                                                   "TextureSamplerWater");
  if (se->poly_pos_index == -1 ||
      se->poly_normal_index == -1 ||
      se->poly_colorf_index == -1 ||
      se->poly_colorb_index == -1 ||
      se->poly_vertex_tex_index == -1 ||
      se->poly_mv_index == -1 ||
      se->poly_proj_index == -1 ||
      se->poly_glbl_ambient_index == -1 ||
      se->poly_lt_ambient_index == -1 ||
      se->poly_lt_diffuse_index == -1 ||
      se->poly_lt_specular_index == -1 ||
      se->poly_lt_direction_index == -1 ||
      se->poly_lt_halfvect_index == -1 ||
      se->poly_front_ambient_index == -1 ||
      se->poly_back_ambient_index == -1 ||
      se->poly_front_diffuse_index == -1 ||
      se->poly_back_diffuse_index == -1 ||
      se->poly_specular_index == -1 ||
      se->poly_shininess_index == -1 ||
      se->poly_bool_textures_index == -1 ||
      se->poly_tex_samp_f_index == -1 ||
      se->poly_tex_samp_b_index == -1 ||
      se->poly_tex_samp_w_index == -1)
  {
    glDeleteProgram(se->poly_shader_program);
    return;
  }

  if (!glsl_CompileAndLinkShaders(3,line_vertex_shader_source,
                                  4,line_fragment_shader_source,
                                  &se->line_shader_program))
  {
    glDeleteProgram(se->poly_shader_program);
    return;
  }
  se->line_pos_index = glGetAttribLocation(se->line_shader_program,
                                           "VertexPosition");
  se->line_color_index = glGetUniformLocation(se->line_shader_program,
                                              "LineColor");
  se->line_mv_index = glGetUniformLocation(se->line_shader_program,
                                           "MatModelView");
  se->line_proj_index = glGetUniformLocation(se->line_shader_program,
                                             "MatProj");
  if (se->line_pos_index == -1 ||
      se->line_color_index == -1 ||
      se->line_mv_index == -1 ||
      se->line_proj_index == -1)
  {
    glDeleteProgram(se->poly_shader_program);
    glDeleteProgram(se->line_shader_program);
    return;
  }

  if (!glsl_CompileAndLinkShaders(3,blend_vertex_shader_source,
                                  4,blend_fragment_shader_source,
                                  &se->blend_shader_program))
  {
    glDeleteProgram(se->poly_shader_program);
    glDeleteProgram(se->line_shader_program);
    return;
  }
  se->blend_vertex_p_index = glGetAttribLocation(se->blend_shader_program,
                                                 "VertexP");
  se->blend_vertex_t_index = glGetAttribLocation(se->blend_shader_program,
                                                 "VertexT");
  se->blend_t_index = glGetUniformLocation(se->blend_shader_program,
                                           "T");
  se->blend_sampler0_index = glGetUniformLocation(se->blend_shader_program,
                                                  "TextureSampler0");
  se->blend_sampler1_index = glGetUniformLocation(se->blend_shader_program,
                                                  "TextureSampler1");
  if (se->blend_vertex_p_index == -1 ||
      se->blend_vertex_t_index == -1 ||
      se->blend_t_index == -1 ||
      se->blend_sampler0_index == -1 ||
      se->blend_sampler1_index == -1)
  {
    glDeleteProgram(se->poly_shader_program);
    glDeleteProgram(se->line_shader_program);
    glDeleteProgram(se->blend_shader_program);
    return;
  }

  glGetIntegerv(GL_MAX_TEXTURE_SIZE,&se->max_tex_size);
  if (se->WindW <= se->max_tex_size && se->WindH <= se->max_tex_size)
  {
    wt = se->WindW;
    ht = se->WindH;
  }
  else
  {
    wt = MIN(se->max_tex_size,se->WindW);
    ht = MIN(se->max_tex_size,se->WindH);
  }

  glGenTextures(2,se->color_textures);
  for (i=0; i<2; i++)
  {
    glBindTexture(GL_TEXTURE_2D,se->color_textures[i]);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,wt,ht,0,GL_RGBA,GL_UNSIGNED_BYTE,
                 NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  }

  glGenBuffers(1,&se->vertex_pos_buffer);
  glGenBuffers(1,&se->vertex_normal_buffer);
  glGenBuffers(2,se->vertex_colorf_buffer);
  glGenBuffers(2,se->vertex_colorb_buffer);
  glGenBuffers(1,&se->vertex_tex_coord_buffer);
  glGenBuffers(1,&se->solid_indices_buffer);
  glGenBuffers(1,&se->parallel_indices_buffer);
  glGenBuffers(1,&se->meridian_indices_buffer);
  glGenBuffers(1,&se->line_indices_buffer);

  se->use_shaders = True;
  if ((!gl_gles3 && gl_major >= 3) || gl_gles3)
    se->use_mipmaps = True;
}

#endif /* HAVE_GLSL */


/*
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 *    Xlock hooks.
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 */


ENTRYPOINT void reshape_sphereeversion(ModeInfo *mi, int width, int height)
{
  sphereeversionstruct *se = &sphereeversion[MI_SCREEN(mi)];
  int y = 0;
#ifdef HAVE_GLSL
  int i, wt, ht;
#endif /* HAVE_GLSL */

  if (width > height * 5) {   /* tiny window: show middle */
    height = width;
    y = -height/2;
  }

  se->WindW = (GLint)width;
  se->WindH = (GLint)height;
  glViewport(0,y,width,height);
  se->aspect = (GLfloat)width/(GLfloat)height;
#ifdef HAVE_GLSL
  if (se->use_shaders)
  {
    if (se->WindW <= se->max_tex_size && se->WindH <= se->max_tex_size)
    {
      wt = se->WindW;
      ht = se->WindH;
    }
    else
    {
      wt = MIN(se->max_tex_size,se->WindW);
      ht = MIN(se->max_tex_size,se->WindH);
    }
    for (i=0; i<2; i++)
    {
      glBindTexture(GL_TEXTURE_2D,se->color_textures[i]);
      glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,wt,ht,0,GL_RGBA,GL_UNSIGNED_BYTE,
                   NULL);
    }
  }
#endif /* HAVE_GLSL */
}


ENTRYPOINT Bool sphereeversion_handle_event(ModeInfo *mi, XEvent *event)
{
  sphereeversionstruct *se = &sphereeversion[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress && event->xbutton.button == Button1)
  {
    se->button_pressed = True;
    gltrackball_start(se->trackball,event->xbutton.x,event->xbutton.y,
                      MI_WIDTH(mi),MI_HEIGHT(mi));
    return True;
  }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
  {
    se->button_pressed = False;
    gltrackball_stop(se->trackball);
    return True;
  }
  else if (event->xany.type == MotionNotify && se->button_pressed)
  {
    gltrackball_track(se->trackball,event->xmotion.x,event->xmotion.y,
                      MI_WIDTH(mi),MI_HEIGHT(mi));
    return True;
  }

  return False;
}


/*
 *-----------------------------------------------------------------------------
 *    Initialize sphereeversion.  Called each time the window changes.
 *-----------------------------------------------------------------------------
 */

ENTRYPOINT void init_sphereeversion(ModeInfo *mi)
{
  sphereeversionstruct *se;

  MI_INIT(mi,sphereeversion);
  se = &sphereeversion[MI_SCREEN(mi)];

  se->trackball = gltrackball_init(False);
  se->button_pressed = False;

  /* Set the eversion method. */
  if (!strcasecmp(eversion_method,"random"))
  {
    se->eversion_method = random() % NUM_EVERSIONS;
  }
  else if (!strcasecmp(eversion_method,"analytic"))
  {
    se->eversion_method = EVERSION_ANALYTIC;
  }
  else if (!strcasecmp(eversion_method,"corrugations"))
  {
    se->eversion_method = EVERSION_CORRUGATIONS;
  }
  else
  {
    se->eversion_method = random() % NUM_EVERSIONS;
  }

  /* Set the display mode. */
  if (!strcasecmp(mode,"surface"))
  {
    se->display_mode[0] = DISP_SURFACE;
    se->random_display_mode = False;
  }
  else if (!strcasecmp(mode,"transparent"))
  {
    se->display_mode[0] = DISP_TRANSPARENT;
    se->random_display_mode = False;
  }
  else
  {
    se->display_mode[0] = random() % NUM_DISPLAY_MODES;
    se->random_display_mode = True;
  }

  /* Set the appearance. */
  if (!strcasecmp(appear,"solid"))
  {
    se->appearance[0] = APPEARANCE_SOLID;
    se->random_appearance = False;
  }
  else if (!strcasecmp(appear,"parallel-bands"))
  {
    se->appearance[0] = APPEARANCE_PARALLEL_BANDS;
    se->random_appearance = False;
  }
  else if (!strcasecmp(appear,"meridian-bands"))
  {
    se->appearance[0] = APPEARANCE_MERIDIAN_BANDS;
    se->random_appearance = False;
  }
  else
  {
    se->appearance[0] = random() % NUM_APPEARANCES;
    se->random_appearance = True;
  }

  /* Set the color mode. */
  if (!strcasecmp(color_mode,"two-sided"))
  {
    se->colors[0] = COLORS_TWOSIDED;
    se->random_colors = False;
  }
  else if (!strcasecmp(color_mode,"parallel"))
  {
    se->colors[0] = COLORS_PARALLEL;
    se->random_colors = False;
  }
  else if (!strcasecmp(color_mode,"meridian"))
  {
    se->colors[0] = COLORS_MERIDIAN;
    se->random_colors = False;
  }
  else if (!strcasecmp(color_mode,"earth"))
  {
    se->colors[0] = COLORS_EARTH;
    se->random_colors = False;
  }
  else
  {
    se->colors[0] = random() % NUM_COLORS;
    se->random_colors = True;
  }

  /* Set the 3d projection mode. */
  if (!strcasecmp(proj,"perspective"))
  {
    se->projection = DISP_PERSPECTIVE;
  }
  else if (!strcasecmp(proj,"orthographic"))
  {
    se->projection = DISP_ORTHOGRAPHIC;
  }
  else
  {
    se->projection = random() % NUM_DISP_MODES;
  }

  /* Set the graticule mode. */
  if (!strcasecmp(graticule,"on"))
  {
    se->graticule[0] = True;
    se->random_graticule = False;
  }
  else if (!strcasecmp(graticule,"off"))
  {
    se->graticule[0] = False;
    se->random_graticule = False;
  }
  else
  {
    se->graticule[0] = random() & 1;
    se->random_graticule = True;
  }

  /* Set the surface order. */
  if (!strcasecmp(surface_order,"2"))
  {
    se->g = 2;
    se->random_g = False;
  }
  else if (!strcasecmp(surface_order,"3"))
  {
    se->g = 3;
    se->random_g = False;
  }
  else if (!strcasecmp(surface_order,"4"))
  {
    se->g = 4;
    se->random_g = False;
  }
  else if (!strcasecmp(surface_order,"5"))
  {
    se->g = 5;
    se->random_g = False;
  }
  else
  {
    se->g = random() % 4 + 2;
    se->random_g = True;
  }

  /* Set the number of strips. */
  if (!strcasecmp(lunes,"lunes-1"))
  {
    se->strip_step = 8;
  }
  else if (!strcasecmp(lunes,"lunes-2"))
  {
    se->strip_step = 4;
  }
  else if (!strcasecmp(lunes,"lunes-4"))
  {
    se->strip_step = 2;
  }
  else if (!strcasecmp(lunes,"lunes-8"))
  {
    se->strip_step = 1;
  }
  else
  {
    se->strip_step = 1;
  }

  /* Set the number of hemispheres. */
  if (!strcasecmp(hemispheres,"hemispheres-1"))
  {
    se->num_hemispheres = 1;
  }
  else if (!strcasecmp(hemispheres,"hemispheres-2"))
  {
    se->num_hemispheres = 2;
  }
  else
  {
    se->num_hemispheres = 2;
  }

  /* Make multiple screens rotate at slightly different rates. */
  se->speed_scale = 0.9+frand(0.3);

  if ((se->glx_context = init_GL(mi)) != NULL)
  {
    reshape_sphereeversion(mi,MI_WIDTH(mi),MI_HEIGHT(mi));
    if (se->eversion_method == EVERSION_ANALYTIC)
      init_sphereeversion_analytic(mi);
    else /* se->eversion_method == EVERSION_CORRUGATIONS */
      init_sphereeversion_corrugations(mi);
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
ENTRYPOINT void draw_sphereeversion(ModeInfo *mi)
{
  Display *display = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  sphereeversionstruct *se;

  if (sphereeversion == NULL)
    return;
  se = &sphereeversion[MI_SCREEN(mi)];

  MI_IS_DRAWN(mi) = True;
  if (!se->glx_context)
    return;

  glXMakeCurrent(display,window,*se->glx_context);

  if (se->eversion_method == EVERSION_ANALYTIC)
    display_sphereeversion_analytic(mi);
  else /* se->eversion_method == EVERSION_CORRUGATIONS */
    display_sphereeversion_corrugations(mi);

  if (MI_IS_FPS(mi))
    do_fps(mi);

  glFlush();

  glXSwapBuffers(display,window);
}


#ifndef STANDALONE
ENTRYPOINT void change_sphereeversion(ModeInfo *mi)
{
  sphereeversionstruct *ev = &sphereeversion[MI_SCREEN(mi)];

  if (!ev->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi),MI_WINDOW(mi),*ev->glx_context);

  if (se->eversion_method == EVERSION_ANALYTIC)
    init_sphereeversion_analytic(mi);
  else /* se->eversion_method == EVERSION_CORRUGATIONS */
    init_sphereeversion_corrugations(mi);
}
#endif /* !STANDALONE */


/*
 *-----------------------------------------------------------------------------
 *    The display is being taken away from us.  Free up malloc'ed 
 *      memory and X resources that we've alloc'ed.
 *-----------------------------------------------------------------------------
 */

ENTRYPOINT void free_sphereeversion(ModeInfo *mi)
{
  sphereeversionstruct *se = &sphereeversion[MI_SCREEN(mi)];

  if (!se->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi),MI_WINDOW(mi),*se->glx_context);

  if (se->sp) free(se->sp);
  if (se->sn) free(se->sn);
  if (se->colf[0]) free(se->colf[0]);
  if (se->colf[1]) free(se->colf[1]);
  if (se->colb[0]) free(se->colb[0]);
  if (se->colb[1]) free(se->colb[1]);
  gltrackball_free(se->trackball);
#ifdef HAVE_GLSL
  if (se->tex) free(se->tex);
  if (se->tex_names[0]) glDeleteTextures(1,&se->tex_names[0]);
  if (se->tex_names[1]) glDeleteTextures(1,&se->tex_names[1]);
  if (se->tex_names[2]) glDeleteTextures(1,&se->tex_names[2]);
  if (se->solid_indices) free(se->solid_indices);
  if (se->parallel_indices) free(se->parallel_indices);
  if (se->meridian_indices) free(se->meridian_indices);
  if (se->line_indices) free(se->line_indices);
  if (se->use_shaders)
  {
    glUseProgram(0);
    if (se->poly_shader_program != 0)
      glDeleteProgram(se->poly_shader_program);
    if (se->line_shader_program != 0)
      glDeleteProgram(se->line_shader_program);
    if (se->blend_shader_program != 0)
      glDeleteProgram(se->blend_shader_program);
    glDeleteTextures(2,se->color_textures);
    glDeleteBuffers(1,&se->vertex_pos_buffer);
    glDeleteBuffers(1,&se->vertex_normal_buffer);
    glDeleteBuffers(2,se->vertex_colorf_buffer);
    glDeleteBuffers(2,se->vertex_colorb_buffer);
    glDeleteBuffers(1,&se->vertex_tex_coord_buffer);
    glDeleteBuffers(1,&se->solid_indices_buffer);
    glDeleteBuffers(1,&se->parallel_indices_buffer);
    glDeleteBuffers(1,&se->meridian_indices_buffer);
    glDeleteBuffers(1,&se->line_indices_buffer);
  }
#endif /* HAVE_GLSL */
}


XSCREENSAVER_MODULE ("SphereEversion", sphereeversion)

#endif /* USE_GL */
