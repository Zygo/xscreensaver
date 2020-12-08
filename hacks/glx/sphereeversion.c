/* sphereeversion --- Shows a sphere eversion, i.e., a smooth deformation
   (homotopy) that turns a sphere inside out.  During the eversion, the
   deformed sphere is allowed to intersect itself transversally.  However,
   no creases or pinch points are allowed to occur. */

#if 0
static const char sccsid[] = "@(#)sphereeversion.c  1.1 22/03/20 xlockmore";
#endif

/* Copyright (c) 2020 Carsten Steger <carsten@mirsanmir.org>. */

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
 * C. Steger - 22/03/20: Initial version
 * C. Steger - 10/04/20: Added blending between visualization modes
 * C. Steger - 01/06/20: Removed blending because accumulation buffers have
 * *                     been deprecated since OpenGL 3.0
 * C. Steger - 26/07/20: Make the polygon offset work under OpenGL ES
 * C. Steger - 03/08/20: Add an easing function for one part of the animation
 * C. Steger - 11/10/20: Add easing functions for more parts of the animation
 */

/*
 * This program shows a sphere eversion, i.e., a smooth deformation
 * (homotopy) that turns a sphere inside out.  During the eversion,
 * the deformed sphere is allowed to intersect itself transversally.
 * However, no creases or pinch points are allowed to occur.
 *
 * The deformed sphere can be projected to the screen either
 * perspectively or orthographically.
 *
 * There are three display modes for the sphere: solid, transparent,
 * or random.  If random mode is selected, the mode is changed each
 * time an eversion has been completed.
 *
 * The appearance of the sphere can be as a solid object, as a set of
 * see-through bands, or random.  The bands can be parallel bands or
 * meridian bands, i.e., bands that run along the parallels (lines of
 * latitude) or bands that run along the meridians (lines of
 * longitude) of the sphere.  If random mode is selected, the
 * appearance is changed each time an eversion has been completed.
 *
 * It is also possible to display a graticule (i.e., a coordinate grid
 * consisting of parallel and meridian lines) on top of the surface.
 * The graticule mode can be set to on, off, or random.  If random
 * mode is selected, the graticule mode is changed each time an
 * eversion has been completed.
 *
 * It is possible to define a surface order of the sphere eversion as
 * random or as a value between 2 and 5.  This determines the the
 * complexity of the deformation.  For higher surface orders, some
 * z-fighting might occur around the central stage of the eversion,
 * which might lead to some irregular flickering of the displayed
 * surface if it is displayed as a solid object.  For odd surface
 * orders, z-fighting will occur very close to the central stage of
 * the eversion since the deformed sphere is a doubly covered Boy
 * surface (for surface order 3) or a doubly covered generalized Boy
 * surface (for surface order 5) in this case.  If you find this
 * distracting, you should set the surface order to 2.  If a random
 * surface order is selected, the surface order is changed each time
 * an eversion has been completed.
 *
 * The colors with with the sphere is drawn can be set to two-sided,
 * parallel, meridian, or random.  In two-sided mode, the sphere is
 * drawn with red on one side and green on the other side.  In
 * parallel mode, the sphere is displayed with colors that run from
 * red to cyan on one side of the surface and from green to violet on
 * the other side.  The colors are aligned with the parallels of the
 * sphere in this mode.  In meridian mode, the the sphere is displayed
 * with colors that run from red to white to cyan to black and back to
 * red on one side of the surface and from green to white to violet to
 * black and back to green on the other side.  The colors are aligned
 * with the meridians of the sphere in this mode. If random mode is
 * selected, the color scheme is changed each time an eversion has
 * been completed.
 *
 * By default, the sphere is rotated to a new viewing position each
 * time an eversion has been completed.  In addition, it is possible
 * to rotate the sphere while it is deforming.  The rotation speed for
 * each of the three coordinate axes around which the sphere rotates
 * can be chosen arbitrarily.  For best effects, however, it is
 * suggested to rotate only around the z axis while the sphere is
 * deforming.
 *
 * This program is inspired by the following paper: Adam Bednorz,
 * Witold Bednorz: "Analytic sphere eversion using ruled surfaces",
 * Differential Geometry and its Applications 64:59-79, 2019.
 */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DISP_SURFACE               0
#define DISP_TRANSPARENT           1
#define NUM_DISPLAY_MODES          2

#define APPEARANCE_SOLID           0
#define APPEARANCE_PARALLEL_BANDS  1
#define APPEARANCE_MERIDIAN_BANDS  2
#define NUM_APPEARANCES            3

#define COLORS_TWOSIDED            0
#define COLORS_PARALLEL            1
#define COLORS_MERIDIAN            2
#define NUM_COLORS                 3

#define DISP_PERSPECTIVE           0
#define DISP_ORTHOGRAPHIC          1
#define NUM_DISP_MODES             2

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

#ifdef STANDALONE
# define DEFAULTS           "*delay:      10000 \n" \
                            "*showFPS:    False \n" \

# define release_sphereeversion 0
# include "xlockmore.h"         /* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"             /* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL

#ifndef HAVE_JWXYZ
# include <X11/keysym.h>
#endif

#include "gltrackball.h"

#include <float.h>


#ifdef USE_MODULES
ModStruct sphereeversion_description =
{"sphereeversion", "init_sphereeversion", "draw_sphereeversion",
 NULL, "draw_sphereeversion", "change_sphereeversion",
 "free_sphereeversion", &sphereeversion_opts, 25000, 1, 1, 1, 1.0, 4, "",
 "Show a sphere eversion", 0, NULL};

#endif


static char *mode;
static char *appear;
static char *color_mode;
static char *graticule;
static char *proj;
static float speed_x;
static float speed_y;
static float speed_z;
static float deform_speed;
static char *surface_order;


static XrmOptionDescRec opts[] =
{
  {"-mode",                ".displayMode",   XrmoptionSepArg, 0 },
  {"-surface",             ".displayMode",   XrmoptionNoArg,  "surface" },
  {"-transparent",         ".displayMode",   XrmoptionNoArg,  "transparent" },
  {"-appearance",          ".appearance",    XrmoptionSepArg, 0 },
  {"-solid",               ".appearance",    XrmoptionNoArg,  "solid" },
  {"-parallel-bands",      ".appearance",    XrmoptionNoArg,  "parallel-bands" },
  {"-meridian-bands",      ".appearance",    XrmoptionNoArg,  "meridian-bands" },
  {"-graticule",           ".graticule",     XrmoptionSepArg, 0 },
  {"-colors",              ".colors",        XrmoptionSepArg, 0 },
  {"-twosided-colors",     ".colors",        XrmoptionNoArg,  "two-sided" },
  {"-parallel-colors",     ".colors",        XrmoptionNoArg,  "parallel" },
  {"-meridian-colors",     ".colors",        XrmoptionNoArg,  "meridian" },
  {"-projection",          ".projection",    XrmoptionSepArg, 0 },
  {"-perspective",         ".projection",    XrmoptionNoArg,  "perspective" },
  {"-orthographic",        ".projection",    XrmoptionNoArg,  "orthographic" },
  {"-speed-x",             ".speedx",        XrmoptionSepArg, 0 },
  {"-speed-y",             ".speedy",        XrmoptionSepArg, 0 },
  {"-speed-z",             ".speedz",        XrmoptionSepArg, 0 },
  {"-deformation-speed",   ".deformSpeed",   XrmoptionSepArg, 0 },
  {"-surface-order",       ".surfaceOrder",  XrmoptionSepArg, 0 },
};

static argtype vars[] =
{
  { &mode,           "displayMode",   "DisplayMode",   DEF_DISPLAY_MODE,   t_String },
  { &appear,         "appearance",    "Appearance",    DEF_APPEARANCE,     t_String },
  { &graticule,      "graticule",     "Graticule",     DEF_GRATICULE,      t_String },
  { &color_mode,     "colors",        "Colors",        DEF_COLORS,         t_String },
  { &surface_order,  "surfaceOrder",  "SurfaceOrder",  DEF_SURFACE_ORDER,  t_String },
  { &proj,           "projection",    "Projection",    DEF_PROJECTION,     t_String },
  { &speed_x,        "speedx",        "Speedx",        DEF_SPEEDX,         t_Float},
  { &speed_y,        "speedy",        "Speedy",        DEF_SPEEDY,         t_Float},
  { &speed_z,        "speedz",        "Speedz",        DEF_SPEEDZ,         t_Float},
  { &deform_speed,   "deformSpeed",   "DeformSpeed",   DEF_DEFORM_SPEED,   t_Float},
};

ENTRYPOINT ModeSpecOpt sphereeversion_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, NULL};


/* Shape parameters for the Bednorz sphere eversion. */
#define BEDNORZ_OMEGA 2.0
#define BEDNORZ_Q (2.0/3.0)
#define BEDNORZ_ETA_MIN (3.0/4.0)
#define BEDNORZ_BETA_MAX 0.1
#define BEDNORZ_ALPHA 1.0
#define BEDNORZ_EPS2 0.001
#define BEDNORZ_EPS3 0.002
#define BEDNORZ_EPS4 0.001
#define BEDNORZ_EPS5 0.002

#define BEDNORZ_TAU1 (1.0/BEDNORZ_Q)
#define BEDNORZ_TAU2 2.5
#define BEDNORZ_TAU3 4.5
#define BEDNORZ_TAU4 6.0

#define BEDNORZ_TAU_MIN (-BEDNORZ_TAU4)
#define BEDNORZ_TAU_MAX (BEDNORZ_TAU4)

/* Number of subdivisions of the surface */
#define NUMTH 256
#define NUMPH 256

/* Number of subdivisions between grid lines */
#define NUMGRID 32

/* Number of subdivisions per band */
#define NUMBDIR  16
#define NUMBDIST 16

/* Animation states */
#define ANIM_DEFORM 0
#define ANIM_TURN   1

/* Angle of a single turn step */
#define TURN_STEP 1.0

typedef struct {
  int n;
  double kappa;
  double omega;
  double t;
  double p;
  double q;
  double xi;
  double eta;
  double alpha;
  double beta;
  double gamma;
  double lambda;
  double eps;
} bednorz_shape_par;


typedef struct {
  GLint WindH, WindW;
  GLXContext *glx_context;
  /* Options */
  int display_mode;
  Bool random_display_mode;
  int appearance;
  Bool random_appearance;
  Bool graticule;
  Bool random_graticule;
  int colors;
  Bool random_colors;
  int projection;
  /* 3D rotation angles */
  float alpha, beta, delta;
  /* Animation state */
  int anim_state;
  /* Deformation parameters */
  float tau;
  int defdir;
  /* Turning parameters */
  int turn_step;
  int num_turn;
  float qs[4], qe[4];
  /* Two global shape parameters of the Bednorz sphere eversion */
  float eta_min, beta_max;
  /* The order of the Bednorz sphere eversion */
  int g;
  Bool random_g;
  /* The viewing offset in 3d */
  float offset3d[3];
  /* The 3d coordinates of the surface and the corresponding normal vectors */
  float *se;
  float *sen;
  /* The precomputed colors of the surface */
  float *colf;
  float *colb;
  /* Aspect ratio of the current window */
  float aspect;
  /* Trackball states */
  trackball_state *trackball;
  Bool button_pressed;
  /* A random factor to modify the rotation speeds */
  float speed_scale;
} sphereeversionstruct;

static sphereeversionstruct *sphereeversion = (sphereeversionstruct *) NULL;


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


/* Compute 3D rotation angles from a unit quaternion. */
static void quat_to_angles(float q[4], float *alpha, float *beta, float *delta)
{
  double r00, r01, r02, r12, r22;

  r00 = q[0]*q[0]+q[1]*q[1]-q[2]*q[2]-q[3]*q[3];
  r01 = 2.0*(q[1]*q[2]-q[0]*q[3]);
  r02 = 2.0*(q[1]*q[3]+q[0]*q[2]);
  r12 = 2.0*(q[2]*q[3]-q[0]*q[1]);
  r22 = q[0]*q[0]-q[1]*q[1]-q[2]*q[2]+q[3]*q[3];

  *alpha = atan2(-r12,r22)*180.0/M_PI;
  *beta = atan2(r02,sqrt(r00*r00+r01*r01))*180.0/M_PI;
  *delta = atan2(-r01,r00)*180.0/M_PI;
}


/* Compute a 3D rotation matrix from an xscreensaver unit quaternion. Note
   that xscreensaver has a different convention for unit quaternions than
   the one that is used in this hack. */
static void quat_to_rotmat(float p[4], float m[3][3])
{
  float al, be, de;
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


/* Compute a quaternion from angles in degrees. */
static void angles_to_quat(float alpha, float beta, float delta, float p[4])
{
  alpha *= M_PI/180.0;
  beta *= M_PI/180.0;
  delta *= M_PI/180.0;
  p[0] = (cos(0.5*alpha)*cos(0.5*beta)*cos(0.5*delta)-
          sin(0.5*alpha)*sin(0.5*beta)*sin(0.5*delta));
  p[1] = (sin(0.5*alpha)*cos(0.5*beta)*cos(0.5*delta)+
          cos(0.5*alpha)*sin(0.5*beta)*sin(0.5*delta));
  p[2] = (cos(0.5*alpha)*sin(0.5*beta)*cos(0.5*delta)-
          sin(0.5*alpha)*cos(0.5*beta)*sin(0.5*delta));
  p[3] = (cos(0.5*alpha)*cos(0.5*beta)*sin(0.5*delta)+
          sin(0.5*alpha)*sin(0.5*beta)*cos(0.5*delta));
}


/* Perform a spherical linear interpolation between two quaternions. */
static void quat_slerp(float t, float qs[4], float qe[4], float q[4])
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
static void color(sphereeversionstruct *se, float angle, float mat[3][3],
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
  if (se->display_mode == DISP_TRANSPARENT)
    colf[3] = 0.7f;
  else
    colf[3] = 1.0f;

  colb[0] = ca*mat[0][1]+sa*mat[0][2];
  colb[1] = ca*mat[1][1]+sa*mat[1][2];
  colb[2] = ca*mat[2][1]+sa*mat[2][2];
  m = 0.5f/fmaxf(fmaxf(fabsf(colb[0]),fabsf(colb[1])),fabsf(colb[2]));
  colb[0] = m*colb[0]+0.5f;
  colb[1] = m*colb[1]+0.5f;
  colb[2] = m*colb[2]+0.5f;
  if (se->display_mode == DISP_TRANSPARENT)
    colb[3] = 0.7f;
  else
    colb[3] = 1.0f;
}


/* Compute the cross product between the vectors a and b. */
static inline void cross(float a[3], float b[3], float c[3])
{
  c[0] = a[1]*b[2]-a[2]*b[1];
  c[1] = a[2]*b[0]-a[0]*b[2];
  c[2] = a[0]*b[1]-a[1]*b[0];
}


/* Compute x^n for integers 0 <= n <= 11 efficiently. */
static inline double ipow(double x, int n)
{
  double x2, x4, x8;

  switch (n)
  {
    case 0:
      return 1.0;
    case 1:
      return x;
    case 2:
      x2 = x*x;
      return x2;
    case 3:
      x2 = x*x;
      return x2*x;
    case 4:
      x2 = x*x;
      x4 = x2*x2;
      return x4;
    case 5:
      x2 = x*x;
      x4 = x2*x2;
      return x4*x;
    case 6:
      x2 = x*x;
      x4 = x2*x2;
      return x4*x2;
    case 7:
      x2 = x*x;
      x4 = x2*x2;
      return x4*x2*x;
    case 8:
      x2 = x*x;
      x4 = x2*x2;
      x8 = x4*x4;
      return x8;
    case 9:
      x2 = x*x;
      x4 = x2*x2;
      x8 = x4*x4;
      return x8*x;
    case 10:
      x2 = x*x;
      x4 = x2*x2;
      x8 = x4*x4;
      return x8*x2;
    case 11:
      x2 = x*x;
      x4 = x2*x2;
      x8 = x4*x4;
      return x8*x2*x;
    default:
      return pow(x,n);
  }
}


/* Compute the Bednorz shape parameter kappa based on the eversion order n. */
static inline double bednorz_get_kappa(int n)
{
  return (n-1.0)/(2.0*n);
}


/* Compute the Bednorz shape parameter t based on the deformation
   parameter tau. */
static inline double bednorz_get_t(double tau)
{
  return (tau >= BEDNORZ_TAU1 ?
          BEDNORZ_TAU1 :
          (tau <= -BEDNORZ_TAU1 ?
           -BEDNORZ_TAU1 :
           tau));
}


/* Compute the Bednorz shape parameter q based on the deformation
   parameter tau. */
static inline double bednorz_get_q(double tau)
{
  double abs_tau;

  abs_tau = fabs(tau);
  return (abs_tau < BEDNORZ_TAU1 ?
          0.0 :
          (abs_tau < BEDNORZ_TAU2 ?
           BEDNORZ_Q*(abs_tau-BEDNORZ_TAU1)/(BEDNORZ_TAU2-BEDNORZ_TAU1) :
           BEDNORZ_Q));
}


/* Compute the Bednorz shape parameter p based on the deformation
   parameter tau. */
static inline double bednorz_get_p(double tau)
{
  return 1.0-fabs(bednorz_get_q(tau)*bednorz_get_t(tau));
}


/* Compute the Bednorz shape parameter xi based on the deformation
   parameter tau. */
static inline double bednorz_get_xi(double tau)
{
  double abs_tau;

  abs_tau = fabs(tau);
  return (abs_tau < BEDNORZ_TAU2 ?
          1.0 :
          (abs_tau < BEDNORZ_TAU3 ?
           (BEDNORZ_TAU3-abs_tau)/(BEDNORZ_TAU3-BEDNORZ_TAU2) :
           0.0));
}


/* Compute the Bednorz shape parameter eta based on the deformation
   parameter tau and the shape parameter eta_min. */
static inline double bednorz_get_eta(double tau, double eta_min)
{
  double abs_tau;

  abs_tau = fabs(tau);
  return (abs_tau < BEDNORZ_TAU2 ?
          eta_min :
          (abs_tau < BEDNORZ_TAU3 ?
           (eta_min+(1.0-eta_min)*
            (abs_tau-BEDNORZ_TAU2)/(BEDNORZ_TAU3-BEDNORZ_TAU2)) :
           1.0));
}


/* Compute the Bednorz shape parameter alpha based on the deformation
   parameter tau. */
static inline double bednorz_get_alpha(double tau)
{
  double xi;

  xi = bednorz_get_xi(tau);
  return BEDNORZ_ALPHA*ipow(xi,2);
}


/* Compute the Bednorz shape parameter beta based on the deformation
   parameter tau and the shape parameter beta_max. */
static inline double bednorz_get_beta(double tau, double beta_max)
{
  double xi;

  xi = bednorz_get_xi(tau);
  return ipow(1.0-xi,2)+beta_max*ipow(xi,3);
}


/* Compute the Bednorz shape parameter gamma based on the shape
   parameters alpha and beta. */
static inline double bednorz_get_gamma(double alpha, double beta)
{
  return 2.0*sqrt(alpha*beta);
}


/* Compute the Bednorz shape parameter lambda based on the deformation
   parameter tau. */
static inline double bednorz_get_lambda(double tau)
{
  double abs_tau;

  abs_tau = fabs(tau);
  return (abs_tau < BEDNORZ_TAU3 ?
          1.0 :
          (abs_tau < BEDNORZ_TAU4 ?
           (BEDNORZ_TAU4-abs_tau)/(BEDNORZ_TAU4-BEDNORZ_TAU3) :
           0.0));
}


/* Compute the Bednorz shape parameter eps based on the deformation
   parameter tau and the eversion order n.  This is an extension to the
   original approach that prevents z fighting to some extent in certain
   stages of the eversion. */
static inline double bednorz_get_eps(double tau, int n)
{
  double sgn_tau, abs_tau;

  sgn_tau = (tau < 0.0 ? -1.0 : (tau > 0.0 ? 1.0 : 0.0));
  abs_tau = fabs(tau);
  switch (n)
  {
    case 2:
      return (abs_tau < BEDNORZ_TAU1 ?
              0.0 :
              (abs_tau < BEDNORZ_TAU2 ?
               (BEDNORZ_EPS2*sgn_tau*
                (abs_tau-BEDNORZ_TAU1)/(BEDNORZ_TAU2-BEDNORZ_TAU1)) :
               (abs_tau < BEDNORZ_TAU3 ?
                BEDNORZ_EPS2*sgn_tau :
                (abs_tau < BEDNORZ_TAU4 ?
                 (BEDNORZ_EPS2*sgn_tau*
                  (BEDNORZ_TAU4-abs_tau)/(BEDNORZ_TAU4-BEDNORZ_TAU3)) :
                 0.0))));
    case 3:
      return (abs_tau < BEDNORZ_TAU1 ?
              BEDNORZ_EPS3*sgn_tau*abs_tau/BEDNORZ_TAU1 :
              (abs_tau < BEDNORZ_TAU3 ?
               BEDNORZ_EPS3*sgn_tau :
               (abs_tau < BEDNORZ_TAU4 ?
                (BEDNORZ_EPS3*sgn_tau*
                 (BEDNORZ_TAU4-abs_tau)/(BEDNORZ_TAU4-BEDNORZ_TAU3)) :
                0.0)));
    case 4:
      return (abs_tau < BEDNORZ_TAU1 ?
              BEDNORZ_EPS4*sgn_tau*abs_tau/BEDNORZ_TAU1 :
              (abs_tau < BEDNORZ_TAU3 ?
               BEDNORZ_EPS4*sgn_tau :
               (abs_tau < BEDNORZ_TAU4 ?
                (BEDNORZ_EPS4*sgn_tau*
                 (BEDNORZ_TAU4-abs_tau)/(BEDNORZ_TAU4-BEDNORZ_TAU3)) :
                0.0)));
    case 5:
      return (abs_tau < BEDNORZ_TAU1 ?
              BEDNORZ_EPS5*sgn_tau*abs_tau/BEDNORZ_TAU1 :
              (abs_tau < BEDNORZ_TAU3 ?
               BEDNORZ_EPS5*sgn_tau :
               (abs_tau < BEDNORZ_TAU4 ?
                (BEDNORZ_EPS5*sgn_tau*
                 (BEDNORZ_TAU4-abs_tau)/(BEDNORZ_TAU4-BEDNORZ_TAU3)) :
                0.0)));
    default:
      return 0.0;
  }
}


/* Compute the equations for a point x and its partial derivatives dxdph and
   dxdth in the Bednorz sphere eversion based on the sphere parameters phi
   (longitude) and theta (latitude) and the shape parameters bsp.  This
   corresponds to equations (4), (12), and (15) in the paper. */
static inline void bednorz_get_p0(double phi, double theta,
                                  bednorz_shape_par *bsp, double x[3],
                                  double dxdph[3], double dxdth[3])
{
  int n;
  double kappa, omega, t, p, q, eta, lambda;
  double ct, st, cp, sp, cnm1p, snm1p, cnp, snp;
  double ctn, ictn, ictnp1, ct2, st2, ct2n, ict2n, ict2np1, ton;
  double oml, omlplctn, pe1pk, tat2k, nst2pct2, ost, snpmqt;
  double nm1p, ostictn, oictnp1, nst2pct2oictnp1, tcp, tsp;
  double lost, tomlplctn, lostcp, lostsp, tomlplctncp, tomlplctnsp;
  double tomlplctncpmlostsp, tomlplctnspplostcp, ntctnst, oct2;
  double omlpe1pktat2k, nst2;

  n = bsp->n;
  kappa = bsp->kappa;
  omega = bsp->omega;
  t = bsp->t;
  p = bsp->p;
  q = bsp->q;
  eta = bsp->eta;
  lambda = bsp->lambda;

  ct = cos(theta);
  st = sin(theta);
  cp = cos(phi);
  sp = sin(phi);
  cnp = cos(n*phi);
  snp = sin(n*phi);
  ctn = ipow(ct,n);
  ictn = 1.0/ctn;
  ictnp1 = ictn/ct;
  ct2 = ct*ct;
  st2 = st*st;
  ton = t/n;
  snpmqt = snp-q*t;
  ost = omega*st;

  if (lambda >= 1.0)
  {
    cnm1p = cos((n-1)*phi);
    snm1p = sin((n-1)*phi);
    nst2pct2 = n*st2+ct2;
    nm1p = (n-1)*p;
    tcp = t*cp;
    tsp = t*sp;
    ostictn = ost*ictn;
    oictnp1 = omega*ictnp1;
    nst2pct2oictnp1 = nst2pct2*oictnp1;
    x[0] = p*snm1p-sp*ostictn+tcp;
    x[1] = p*cnm1p+cp*ostictn+tsp;
    x[2] = snpmqt*ostictn-ton*cnp;
    dxdph[0] = nm1p*cnm1p-cp*ostictn-tsp;
    dxdph[1] = -nm1p*snm1p-sp*ostictn+tcp;
    dxdph[2] = n*cnp*ostictn+t*snp;
    dxdth[0] = -sp*nst2pct2oictnp1;
    dxdth[1] = cp*nst2pct2oictnp1;
    dxdth[2] = snpmqt*nst2pct2oictnp1;
    /* The same formulas written out in full glory:
       x[0] = (t*cos(phi)+
               p*sin((n-1)*phi)-
               (omega*sin(theta)/ipow(cos(theta),n))*sin(phi));
       x[1] = (t*sin(phi)+
               p*cos((n-1)*phi)+
               (omega*sin(theta)/ipow(cos(theta),n))*cos(phi));
       x[2] = ((omega*sin(theta)/ipow(cos(theta),n))*sin(n*phi)-
               (t/n)*cos(n*phi)-
               omega*q*t*sin(theta)/ipow(cos(theta),n));
       dxdph[0] = (-t*sin(phi)+
                   (n-1)*p*cos((n-1)*phi)-
                   (omega*sin(theta)/ipow(cos(theta),n))*cos(phi));
       dxdph[1] = (t*cos(phi)-
                   (n-1)*p*sin((n-1)*phi)-
                   (omega*sin(theta)/ipow(cos(theta),n))*sin(phi));
       dxdph[2] = ((n*omega*sin(theta)/ipow(cos(theta),n))*cos(n*phi)+
                   t*sin(n*phi));
       dxdth[0] = -((omega*sin(phi)*(n*ipow(sin(theta),2)+ipow(cos(theta),2)))/
                    ipow(cos(theta),n+1));
       dxdth[1] = ((omega*cos(phi)*(n*ipow(sin(theta),2)+ipow(cos(theta),2)))/
                   ipow(cos(theta),n+1));
       dxdth[2] = ((omega*sin(n*phi)*(n*ipow(sin(theta),2)+ipow(cos(theta),2))/
                    ipow(cos(theta),n+1))-
                   (omega*q*t*(n*ipow(sin(theta),2)+ipow(cos(theta),2))/
                    ipow(cos(theta),n+1)));
     */
  }
  else
  {
    ct2n = ipow(ct,2*n);
    ict2n = 1.0/ct2n;
    ict2np1 = ict2n/ct;
    oml = 1.0-lambda;
    omlplctn = oml+lambda*ctn;
    pe1pk = pow(eta,1.0+kappa);
    tat2k = t*pow(fabs(t),2.0*kappa);
    lost = lambda*ost;
    lostcp = lost*cp;
    lostsp = lost*sp;
    tomlplctn = t*omlplctn;
    tomlplctncp = tomlplctn*cp;
    tomlplctnsp = tomlplctn*sp;
    tomlplctncpmlostsp = tomlplctncp-lostsp;
    tomlplctnspplostcp = tomlplctnsp+lostcp;
    ntctnst = n*t*ctn*st;
    oct2 = omega*ct2;
    omlpe1pktat2k = oml*pe1pk*tat2k;
    nst2 = n*st2;
    x[0] = tomlplctncpmlostsp*ictn;
    x[1] = tomlplctnspplostcp*ictn;
    x[2] = lambda*(ost*snpmqt*ictn-ton*cnp)-omlpe1pktat2k*st*ict2n;
    dxdph[0] = -tomlplctnspplostcp*ictn;
    dxdph[1] = tomlplctncpmlostsp*ictn;
    dxdph[2] = lambda*(omega*n*st*cnp*ictn+t*snp);
    dxdth[0] = (n*tomlplctncpmlostsp*st-lambda*(ntctnst*cp+oct2*sp))*ictnp1;
    dxdth[1] = (n*tomlplctnspplostcp*st-lambda*(ntctnst*sp-oct2*cp))*ictnp1;
    dxdth[2] = (lambda*omega*snpmqt*(nst2+ct2)*ictnp1-
                omlpe1pktat2k*(2.0*nst2+ct2)*ict2np1);
    /* The same formulas written out in full glory:
       x[0] = ((t*(1.0-lambda+lambda*ipow(cos(theta),n))*cos(phi)-
                lambda*omega*sin(theta)*sin(phi))/
               ipow(cos(theta),n));
       x[1] = ((t*(1.0-lambda+lambda*ipow(cos(theta),n))*sin(phi)+
                lambda*omega*sin(theta)*cos(phi))/
               ipow(cos(theta),n));
       x[2] = (lambda*(omega*sin(theta)*(sin(n*phi)-q*t)/ipow(cos(theta),n)-
                       (t/n)*cos(n*phi))-
               (1.0-lambda)*pow(eta,1.0+kappa)*
               t*pow(fabs(t),2.0*kappa)*sin(theta)/ipow(cos(theta),2*n));
       dxdph[0] = ((-t*(1.0-lambda+lambda*ipow(cos(theta),n))*sin(phi)-
                    lambda*omega*sin(theta)*cos(phi))/
                   ipow(cos(theta),n));
       dxdph[1] = ((t*(1.0-lambda+lambda*ipow(cos(theta),n))*cos(phi)-
                    lambda*omega*sin(theta)*sin(phi))/
                   ipow(cos(theta),n));
       dxdph[2] = (lambda*(omega*n*sin(theta)*(cos(n*phi))/ipow(cos(theta),n)+
                           t*sin(n*phi)));
       dxdth[0] = ((n*(t*(1.0-lambda+lambda*ipow(cos(theta),n))*cos(phi)-
                       lambda*omega*sin(theta)*sin(phi))*sin(theta)/
                    ipow(cos(theta),n+1))-
                   (lambda*(n*t*ipow(cos(theta),n)*sin(theta)*cos(phi)+
                            omega*ipow(cos(theta),2)*sin(phi))/
                    ipow(cos(theta),n+1)));
       dxdth[1] = ((n*(t*(1.0-lambda+lambda*ipow(cos(theta),n))*sin(phi)+
                       lambda*omega*sin(theta)*cos(phi))*sin(theta)/
                    ipow(cos(theta),n+1))-
                   (lambda*(n*t*ipow(cos(theta),n)*sin(theta)*sin(phi)-
                            omega*ipow(cos(theta),2)*cos(phi))/
                    ipow(cos(theta),n+1)));
       dxdth[2] = ((lambda*omega*(sin(n*phi)-q*t)*
                    (n*ipow(sin(theta),2)+ipow(cos(theta),2))/
                    ipow(cos(theta),n+1))-
                   ((1.0-lambda)*pow(eta,1.0+kappa)*t*pow(fabs(t),2.0*kappa)*
                    (2.0*n*ipow(sin(theta),2)+ipow(cos(theta),2))/
                    ipow(cos(theta),2*n+1)));
     */
  }
}


/* Compute the equations for a point y and its partial derivatives dydph and
   dydth in the Bednorz sphere eversion based on the sphere parameters phi
   (longitude) and theta (latitude) and the shape parameters bsp.  This
   corresponds to equation (7) in the paper. */
static inline void bednorz_get_p1(double phi, double theta,
                                  bednorz_shape_par *bsp, double y[3],
                                  double dydph[3], double dydth[3])
{
  double kappa, xi, eta;
  double x[3], dxdph[3], dxdth[3];
  double x0, x1, x2, x02, x12, x02px12, ex02px12, xipex02px12;
  double ixipex02px122, ixipex02px12k, ixipex02px12kp1;
  double x0dx0dphpx1dx1dph, x0dx0dthpx1dx1dth;
  double tex0dx0dphpx1dx1dph, tex0dx0dthpx1dx1dth;
  double ktex0dx0dphpx1dx1dph, ktex0dx0dthpx1dx1dth;

  kappa = bsp->kappa;
  xi = bsp->xi;
  eta = bsp->eta;

  bednorz_get_p0(phi,theta,bsp,x,dxdph,dxdth);

  x0 = x[0];
  x1 = x[1];
  x2 = x[2];
  x02 = x0*x0;
  x12 = x1*x1;
  x02px12 = x02+x12;
  ex02px12 = eta*x02px12;
  xipex02px12 = xi+ex02px12;
  ixipex02px122 = 1.0/(xipex02px12*xipex02px12);
  ixipex02px12k = 1.0/pow(xipex02px12,kappa);
  ixipex02px12kp1 = ixipex02px12k/xipex02px12;
  x0dx0dphpx1dx1dph = x0*dxdph[0]+x1*dxdph[1];
  x0dx0dthpx1dx1dth = x0*dxdth[0]+x1*dxdth[1];
  tex0dx0dphpx1dx1dph = 2.0*eta*x0dx0dphpx1dx1dph;
  tex0dx0dthpx1dx1dth = 2.0*eta*x0dx0dthpx1dx1dth;
  ktex0dx0dphpx1dx1dph = kappa*tex0dx0dphpx1dx1dph;
  ktex0dx0dthpx1dx1dth = kappa*tex0dx0dthpx1dx1dth;

  y[0] = x0*ixipex02px12k;
  y[1] = x1*ixipex02px12k;
  y[2] = x2/xipex02px12;
  dydph[0] = (dxdph[0]*xipex02px12-ktex0dx0dphpx1dx1dph*x0)*ixipex02px12kp1;
  dydph[1] = (dxdph[1]*xipex02px12-ktex0dx0dphpx1dx1dph*x1)*ixipex02px12kp1;
  dydph[2] = (dxdph[2]*xipex02px12-tex0dx0dphpx1dx1dph*x2)*ixipex02px122;
  dydth[0] = (dxdth[0]*xipex02px12-ktex0dx0dthpx1dx1dth*x0)*ixipex02px12kp1;
  dydth[1] = (dxdth[1]*xipex02px12-ktex0dx0dthpx1dx1dth*x1)*ixipex02px12kp1;
  dydth[2] = (dxdth[2]*xipex02px12-tex0dx0dthpx1dx1dth*x2)*ixipex02px122;

  /* The same formulas written out in full glory:
     y[0] = x[0]/pow(xi+eta*(ipow(x[0],2)+ipow(x[1],2)),kappa);
     y[1] = x[1]/pow(xi+eta*(ipow(x[0],2)+ipow(x[1],2)),kappa);
     y[2] = x[2]/(xi+eta*(ipow(x[0],2)+ipow(x[1],2)));
     dydph[0] = ((dxdph[0]*(xi+eta*(ipow(x[0],2)+ipow(x[1],2)))-
                  2.0*eta*kappa*x[0]*(x[0]*dxdph[0]+x[1]*dxdph[1]))/
                 pow(xi+eta*(ipow(x[0],2)+ipow(x[1],2)),kappa+1.0));
     dydph[1] = ((dxdph[1]*(xi+eta*(ipow(x[0],2)+ipow(x[1],2)))-
                  2.0*eta*kappa*x[1]*(x[0]*dxdph[0]+x[1]*dxdph[1]))/
                 pow(xi+eta*(ipow(x[0],2)+ipow(x[1],2)),kappa+1.0));
     dydph[2] = ((dxdph[2]*(xi+eta*(ipow(x[0],2)+ipow(x[1],2)))-
                  2.0*eta*x[2]*(x[0]*dxdph[0]+x[1]*dxdph[1]))/
                 ipow(xi+eta*(ipow(x[0],2)+ipow(x[1],2)),2));
     dydth[0] = ((dxdth[0]*(xi+eta*(ipow(x[0],2)+ipow(x[1],2)))-
                  2.0*eta*kappa*x[0]*(x[0]*dxdth[0]+x[1]*dxdth[1]))/
                 pow(xi+eta*(ipow(x[0],2)+ipow(x[1],2)),kappa+1.0));
     dydth[1] = ((dxdth[1]*(xi+eta*(ipow(x[0],2)+ipow(x[1],2)))-
                  2.0*eta*kappa*x[1]*(x[0]*dxdth[0]+x[1]*dxdth[1]))/
                 pow(xi+eta*(ipow(x[0],2)+ipow(x[1],2)),kappa+1.0));
     dydth[2] = ((dxdth[2]*(xi+eta*(ipow(x[0],2)+ipow(x[1],2)))-
                  2.0*eta*x[2]*(x[0]*dxdth[0]+x[1]*dxdth[1]))/
                 ipow(xi+eta*(ipow(x[0],2)+ipow(x[1],2)),2));
  */
}


/* Compute the equations for a point z and its partial derivatives dzdph and
   dzdth in the Bednorz sphere eversion based on the sphere parameters phi
   (longitude) and theta (latitude) and the shape parameters bsp.  This
   corresponds to equations (8) and (9) in the paper. */
static void inline bednorz_get_p2(double phi, double theta,
                                  bednorz_shape_par *bsp, double z[3],
                                  double dzdph[3], double dzdth[3])
{
  double alpha, beta, gamma;
  double y[3], dydph[3], dydth[3];
  double y0, y1, y2, y02, y12, y02py12, egy2, apby02py12, amby02py12;
  double iapby02py12, iapby02py122, igapby02py12, igapby02py122;
  double ambogapb, egy2apby02py12, egy2amby02py12, tbegy2;
  double y0dy0dphpy1dy1dph, y0dy0dthpy1dy1dth;
  double iy02py12, iy02py122, ty0dy0dphpy1dy1dph, ty0dy0dthpy1dy1dth;
  double tbegy2y0dy0dphpy1dy1dph, tbegy2y0dy0dthpy1dy1dth;
  double gigapby02py122, gegy2amby02py12;

  alpha = bsp->alpha;
  beta = bsp->beta;
  gamma = bsp->gamma;

  bednorz_get_p1(phi,theta,bsp,y,dydph,dydth);

  y0 = y[0];
  y1 = y[1];
  y2 = y[2];
  y02 = y0*y0;
  y12 = y1*y1;
  y02py12 = y02+y12;
  y0dy0dphpy1dy1dph = y0*dydph[0]+y1*dydph[1];
  y0dy0dthpy1dy1dth = y0*dydth[0]+y1*dydth[1];

  if (alpha > 0.0)
  {
    egy2 = exp(gamma*y2);
    apby02py12 = alpha+beta*y02py12;
    amby02py12 = alpha-beta*y02py12;
    iapby02py12 = 1.0/apby02py12;
    iapby02py122 = iapby02py12*iapby02py12;
    igapby02py12 = iapby02py12/gamma;
    igapby02py122 = igapby02py12*igapby02py12;
    ambogapb = (alpha-beta)/(gamma*(alpha+beta));
    egy2apby02py12 = egy2*apby02py12;
    egy2amby02py12 = egy2*amby02py12;
    tbegy2 = 2.0*beta*egy2;
    tbegy2y0dy0dphpy1dy1dph = tbegy2*y0dy0dphpy1dy1dph;
    tbegy2y0dy0dthpy1dy1dth = tbegy2*y0dy0dthpy1dy1dth;
    gigapby02py122 = gamma*igapby02py122;
    gegy2amby02py12 = gamma*egy2amby02py12;

    z[0] = y0*egy2*iapby02py12;
    z[1] = y1*egy2*iapby02py12;
    z[2] = egy2amby02py12*igapby02py12-ambogapb;
    dzdph[0] = ((y0*gamma*dydph[2]+dydph[0])*egy2apby02py12-
                y0*tbegy2y0dy0dphpy1dy1dph)*iapby02py122;
    dzdph[1] = ((y1*gamma*dydph[2]+dydph[1])*egy2apby02py12-
                y1*tbegy2y0dy0dphpy1dy1dph)*iapby02py122;
    dzdph[2] = (((gegy2amby02py12*dydph[2]-tbegy2y0dy0dphpy1dy1dph)*
                 apby02py12-tbegy2y0dy0dphpy1dy1dph*amby02py12)*
                gigapby02py122);
    dzdth[0] = ((y0*gamma*dydth[2]+dydth[0])*egy2apby02py12-
                y0*tbegy2y0dy0dthpy1dy1dth)*iapby02py122;
    dzdth[1] = ((y1*gamma*dydth[2]+dydth[1])*egy2apby02py12-
                y1*tbegy2y0dy0dthpy1dy1dth)*iapby02py122;
    dzdth[2] = (((gegy2amby02py12*dydth[2]-tbegy2y0dy0dthpy1dy1dth)*
                 apby02py12-tbegy2y0dy0dthpy1dy1dth*amby02py12)*
                gigapby02py122);

    /* The same formulas written out in full glory:
       z[0] = (y[0]*exp(gamma*y[2]))/(alpha+beta*(ipow(y[0],2)+ipow(y[1],2)));
       z[1] = (y[1]*exp(gamma*y[2]))/(alpha+beta*(ipow(y[0],2)+ipow(y[1],2)));
       z[2] = ((exp(gamma*y[2])*(alpha-beta*(ipow(y[0],2)+ipow(y[1],2))))/
               (gamma*(alpha+beta*(ipow(y[0],2)+ipow(y[1],2))))-
               ((alpha-beta)/(gamma*(alpha+beta))));
       dzdph[0] = (((exp(gamma*y[2])*(gamma*y[0]*dydph[2]+dydph[0])*
                     (alpha+beta*(ipow(y[0],2)+ipow(y[1],2))))-
                    (2.0*beta*exp(gamma*y[2])*y[0]*
                     (y[0]*dydph[0]+y[1]*dydph[1])))/
                   ipow(alpha+beta*(ipow(y[0],2)+ipow(y[1],2)),2));
       dzdph[1] = (((exp(gamma*y[2])*(gamma*y[1]*dydph[2]+dydph[1])*
                     (alpha+beta*(ipow(y[0],2)+ipow(y[1],2))))-
                    (2.0*beta*exp(gamma*y[2])*y[1]*
                     (y[0]*dydph[0]+y[1]*dydph[1])))/
                   ipow(alpha+beta*(ipow(y[0],2)+ipow(y[1],2)),2));
       dzdph[2] = (((gamma*exp(gamma*y[2])*
                     (alpha-beta*(ipow(y[0],2)+ipow(y[1],2)))*dydph[2]-
                     2.0*beta*exp(gamma*y[2])*(y[0]*dydph[0]+y[1]*dydph[1]))*
                    (gamma*(alpha+beta*(ipow(y[0],2)+ipow(y[1],2))))-
                    (2.0*gamma*beta*exp(gamma*y[2])*
                     (alpha-beta*(ipow(y[0],2)+ipow(y[1],2)))*
                     (y[0]*dydph[0]+y[1]*dydph[1])))/
                   ipow(gamma*(alpha+beta*(ipow(y[0],2)+ipow(y[1],2))),2));
       dzdth[0] = (((exp(gamma*y[2])*(gamma*y[0]*dydth[2]+dydth[0])*
                     (alpha+beta*(ipow(y[0],2)+ipow(y[1],2))))-
                    (2.0*beta*exp(gamma*y[2])*y[0]*
                     (y[0]*dydth[0]+y[1]*dydth[1])))/
                   ipow(alpha+beta*(ipow(y[0],2)+ipow(y[1],2)),2));
       dzdth[1] = (((exp(gamma*y[2])*(gamma*y[1]*dydth[2]+dydth[1])*
                     (alpha+beta*(ipow(y[0],2)+ipow(y[1],2))))-
                    (2.0*beta*exp(gamma*y[2])*y[1]*
                     (y[0]*dydth[0]+y[1]*dydth[1])))/
                   ipow(alpha+beta*(ipow(y[0],2)+ipow(y[1],2)),2));
       dzdth[2] = (((gamma*exp(gamma*y[2])*
                     (alpha-beta*(ipow(y[0],2)+ipow(y[1],2)))*dydth[2]-
                     2.0*beta*exp(gamma*y[2])*(y[0]*dydth[0]+y[1]*dydth[1]))*
                    (gamma*(alpha+beta*(ipow(y[0],2)+ipow(y[1],2))))-
                    (2.0*gamma*beta*exp(gamma*y[2])*
                     (alpha-beta*(ipow(y[0],2)+ipow(y[1],2)))*
                     (y[0]*dydth[0]+y[1]*dydth[1])))/
                   ipow(gamma*(alpha+beta*(ipow(y[0],2)+ipow(y[1],2))),2));
     */
  }
  else
  {
    iy02py12 = 1.0/y02py12;
    iy02py122 = iy02py12*iy02py12;
    ty0dy0dphpy1dy1dph = 2.0*y0dy0dphpy1dy1dph;
    ty0dy0dthpy1dy1dth = 2.0*y0dy0dthpy1dy1dth;

    z[0] = y0*iy02py12;
    z[1] = y1*iy02py12;
    z[2] = -y2;
    dzdph[0] = (dydph[0]*y02py12-y0*ty0dy0dphpy1dy1dph)*iy02py122;
    dzdph[1] = (dydph[1]*y02py12-y1*ty0dy0dphpy1dy1dph)*iy02py122;
    dzdph[2] = -dydph[2];
    dzdth[0] = (dydth[0]*y02py12-y0*ty0dy0dthpy1dy1dth)*iy02py122;
    dzdth[1] = (dydth[1]*y02py12-y1*ty0dy0dthpy1dy1dth)*iy02py122;
    dzdth[2] = -dydth[2];

    /* The same formulas written out in full glory:
       z[0] = y[0]/(ipow(y[0],2)+ipow(y[1],2));
       z[1] = y[1]/(ipow(y[0],2)+ipow(y[1],2));
       z[2] = -y[2];
       dzdph[0] = ((dydph[0]*(ipow(y[0],2)+ipow(y[1],2))-
                    2.0*y[0]*(y[0]*dydph[0]+y[1]*dydph[1]))/
                   ipow(ipow(y[0],2)+ipow(y[1],2),2));
       dzdph[1] = ((dydph[1]*(ipow(y[0],2)+ipow(y[1],2))-
                    2.0*y[1]*(y[0]*dydph[0]+y[1]*dydph[1]))/
                   ipow(ipow(y[0],2)+ipow(y[1],2),2));
       dzdph[2] = -dydph[2];
       dzdth[0] = ((dydth[0]*(ipow(y[0],2)+ipow(y[1],2))-
                    2.0*y[0]*(y[0]*dydth[0]+y[1]*dydth[1]))/
                   ipow(ipow(y[0],2)+ipow(y[1],2),2));
       dzdth[1] = ((dydth[1]*(ipow(y[0],2)+ipow(y[1],2))-
                    2.0*y[1]*(y[0]*dydth[0]+y[1]*dydth[1]))/
                   ipow(ipow(y[0],2)+ipow(y[1],2),2));
       dzdth[2] = -dydth[2];
     */
  }
}


/* Compute a point p and its surface normal n in the Bednorz sphere
   eversion based on the sphere parameters phi (longitude) and theta
   (latitude) and the shape parameters bsp. */
static void inline bednorz_point_normal(double phi, double theta,
                                        bednorz_shape_par *bsp,
                                        float p[3], float n[3])
{
  double z[3], dzdph[3], dzdth[3];
  float a[3], b[3], t;
  float lambda, eps, oz;
  int i;

  bednorz_get_p2(phi,theta,bsp,z,dzdph,dzdth);

  for (i=0; i<3; i++)
  {
    p[i] = z[i];
    a[i] = dzdph[i];
    b[i] = dzdth[i];
  }

  /* In the original version of the Bednorz sphere eversion, the regions
     around the north and south poles of the sphere are deformed to points
     that lie very close to each other.  This leads to a significant amount
     of z fighting, especially for higher eversion orders, which is visually
     unpleasant.  Therefore, we modify the shape of the deformed sphere very
     slightly to avoid or at least ameliorate the z fignting. */
  lambda = bsp->lambda;
  eps = bsp->eps;
  if (lambda == 1.0)
    oz = eps*sin(theta);
  else
    oz = 0.0f;
  p[2] += oz;

  /* We now compute the surface normal vector based on the cross product
     of the partial derivative vectors.  For the north and south poles, the
     partial derivative vectors are linearly dependent and thus don't
     yield a useful normal vector.  Therefore, we have to include a special
     treatment for the two poles. */
  if (fabs(theta-M_PI/2.0) <= 1.0e-4)
  {
    n[0] = 0.0f;
    n[1] = 0.0f;
    n[2] = 1.0f;
  }
  else if (fabs(theta+M_PI/2.0) <= 1.0e-4)
  {
    n[0] = 0.0f;
    n[1] = 0.0f;
    n[2] = -1.0f;
  }
  else
  {
    cross(a,b,n);
    t = 1.0f/sqrtf(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
    n[0] *= t;
    n[1] *= t;
    n[2] *= t;
  }
}


/* Set up the surface colors for the main pass (i.e., index 0). */
static void setup_surface_colors(ModeInfo *mi, float phi_min, float phi_max,
                                 float theta_min, float theta_max,
                                 int num_phi, int num_theta)
{
  int i, j, o;
  float matc[3][3];
  float phi, theta, phi_range, theta_range;
  sphereeversionstruct *se = &sphereeversion[MI_SCREEN(mi)];

  if (se->colors == COLORS_TWOSIDED)
    return;

  /* Compute the rotation that rotates the color basis vectors. */
  for (i=0; i<3; i++)
    for (j=0; j<3; j++)
      matc[i][j] = (i==j);
  rotatez(matc,-26.565051177078015);
  rotatey(matc,-335.90515744788928);
  rotatex(matc,-50.768479516407787);

  phi_range = phi_max-phi_min;
  theta_range = theta_max-theta_min;
  for (j=0; j<=num_phi; j++)
  {
    phi = phi_range*j/num_phi+phi_min;
    for (i=0; i<=num_theta; i++)
    {
      o = j*(num_theta+1)+i;
      theta = theta_range*i/num_theta+theta_min;
      if (se->colors == COLORS_PARALLEL)
        color(se,(2.0f*theta+M_PI)*(2.0f/3.0f),matc,&se->colf[4*o],
              &se->colb[4*o]);
      else /* se->colors == COLORS_MERIDIAN */
        color(se,phi+M_PI,matc,&se->colf[4*o],&se->colb[4*o]);
    }
  }
}


/* Draw the Bednorz sphere eversion. */
static int bednorz_shpere_eversion(ModeInfo *mi, float phi_min, float phi_max,
                                   float theta_min, float theta_max,
                                   int num_phi, int num_theta, int numb_dist,
                                   int numb_dir, int num_grid)
{
  int polys = 0;
  static const GLfloat light_ambient[] = { 0.0, 0.0, 0.0, 1.0 };
  static const GLfloat light_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
  static const GLfloat light_specular[] = { 1.0, 1.0, 1.0, 1.0 };
  static const GLfloat light_position[] = { 1.0, 1.0, 1.0, 0.0 };
  static const GLfloat mat_specular[] = { 1.0, 1.0, 1.0, 1.0 };
  static const GLfloat mat_diff_front[] = { 1.0f, 0.0f, 0.0f, 1.0f };
  static const GLfloat mat_diff_back[] = { 0.0f, 1.0f, 0.0f, 1.0f };
  static const GLfloat mat_diff_trans_front[] = { 1.0f, 0.0f, 0.0f, 0.7f };
  static const GLfloat mat_diff_trans_back[] = { 0.0f, 1.0f, 0.0f, 0.7f };
  float p[3], n[3], mat[3][3];
  int i, j, k, l, m, o;
  int numb_dist_mask, numb_dist_min, numb_dist_max;
  int numb_dir_mask, numb_dir_min, numb_dir_max;
  float phi, theta, phi_range, theta_range;
  float *xx, *xn, *cf, *cb;
  float a, b, c, d, e, tau, tau_min, tau_max, r, s;
  float x, y, z, zmin, zmax, rmax, scale, offset_z;
  bednorz_shape_par bsp;
  float qu[4], r1[3][3], r2[3][3];
  sphereeversionstruct *se = &sphereeversion[MI_SCREEN(mi)];

  tau = se->tau;
  /* Apply easing functions to the different ranges of tau. */
  if (fabsf(tau) <= BEDNORZ_TAU4)
  {
    if (fabsf(tau) <= BEDNORZ_TAU1)
    {
      tau_min = 0.0f;
      tau_max = BEDNORZ_TAU1;
    }
    else
    if (fabsf(tau) <= BEDNORZ_TAU2)
    {
      tau_min = BEDNORZ_TAU1;
      tau_max = BEDNORZ_TAU2;
    }
    else if (fabsf(tau) <= BEDNORZ_TAU3)
    {
      tau_min = BEDNORZ_TAU2;
      tau_max = BEDNORZ_TAU3;
    }
    else /* fabsf(tau) <= BEDNORZ_TAU4 */
    {
      tau_min = BEDNORZ_TAU3;
      tau_max = BEDNORZ_TAU4;
    }
    e = 1.0f/(tau_min*tau_min-2.0f*tau_min*tau_max+tau_max*tau_max);
    a = -2.0f*e;
    b = 3.0f*(tau_min+tau_max)*e;
    c = -6.0f*tau_min*tau_max*e;
    d = tau_min*tau_max*(tau_min+tau_max)*e;
    if (tau >= 0.0f)
      tau = ((a*tau+b)*tau+c)*tau+d;
    else
      tau = ((a*tau-b)*tau+c)*tau-d;
  }
  /* Set up the shape parameters. */
  bsp.n = se->g;
  bsp.kappa = bednorz_get_kappa(bsp.n);
  bsp.omega = BEDNORZ_OMEGA;
  bsp.t = bednorz_get_t(tau);
  bsp.p = bednorz_get_p(tau);
  bsp.q = bednorz_get_q(tau);
  bsp.xi = bednorz_get_xi(tau);
  bsp.eta = bednorz_get_eta(tau,se->eta_min);
  bsp.alpha = bednorz_get_alpha(tau);
  bsp.beta = bednorz_get_beta(tau,se->beta_max);
  bsp.gamma = bednorz_get_gamma(bsp.alpha,bsp.beta);
  bsp.lambda = bednorz_get_lambda(tau);
  bsp.eps = bednorz_get_eps(tau,bsp.n);

  /* Compute the surface points and normals. */
  phi_range = phi_max-phi_min;
  theta_range = theta_max-theta_min;
  for (j=0; j<=num_phi; j++)
  {
    phi = phi_range*j/num_phi+phi_min;
    for (i=0; i<=num_theta; i++)
    {
      o = j*(num_theta+1)+i;
      theta = theta_range*i/num_theta+theta_min;
      bednorz_point_normal(phi,theta,&bsp,&se->se[3*o],&se->sen[3*o]);
    }
  }

  /* Compute the z offset. */
  zmin = FLT_MAX;
  zmax = -FLT_MAX;
  for (j=0; j<=num_phi; j++)
  {
    for (i=0; i<=num_theta; i++)
    {
      o = j*(num_theta+1)+i;
      z = se->se[3*o+2];
      if (z < zmin)
        zmin = z;
      if (z > zmax)
        zmax = z;
    }
  }
  offset_z = -0.5f*(zmin+zmax);

  /* Shift the surface in the z direction and compute the scale. */
  rmax = -FLT_MAX;
  for (j=0; j<=num_phi; j++)
  {
    for (i=0; i<=num_theta; i++)
    {
      o = j*(num_theta+1)+i;
      se->se[3*o+2] += offset_z;
      x = se->se[3*o];
      y = se->se[3*o+1];
      z = se->se[3*o+2];
      r = x*x+y*y+z*z;
      if (r > rmax)
        rmax = r;
    }
  }
  scale = 0.75f/sqrtf(rmax);

  /* Scale the surface. */
  for (j=0; j<=num_phi; j++)
  {
    for (i=0; i<=num_theta; i++)
    {
      o = j*(num_theta+1)+i;
      se->se[3*o] *= scale;
      se->se[3*o+1] *= scale;
      se->se[3*o+2] *= scale;
    }
  }

  /* Compute the rotation that rotates the surface in 3D, including the
     trackball rotations. */
  rotateall(se->alpha,se->beta,se->delta,r1);

  gltrackball_get_quaternion(se->trackball,qu);
  quat_to_rotmat(qu,r2);

  mult_rotmat(r2,r1,mat);

  numb_dist_mask = numb_dist-1;
  numb_dist_min = numb_dist/4;
  numb_dist_max = 3*numb_dist/4;
  numb_dir_mask = numb_dir-1;
  numb_dir_min = numb_dir/4;
  numb_dir_max = 3*numb_dir/4;

  glClearColor(0.0f,0.0f,0.0f,1.0f);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  if (se->display_mode == DISP_SURFACE)
  {
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
    glLightfv(GL_LIGHT0,GL_POSITION,light_position);
    glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,mat_specular);
    glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,50.0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
  }
  else /* se->display_mode == DISP_TRANSPARENT */
  {
    glDisable(GL_DEPTH_TEST);
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
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE);
  }

  if (se->colors == COLORS_TWOSIDED)
  {
    glColor3fv(mat_diff_front);
    if (se->display_mode == DISP_TRANSPARENT)
    {
      glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,mat_diff_trans_front);
      glMaterialfv(GL_BACK,GL_AMBIENT_AND_DIFFUSE,mat_diff_trans_back);
    }
    else
    {
      glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,mat_diff_front);
      glMaterialfv(GL_BACK,GL_AMBIENT_AND_DIFFUSE,mat_diff_back);
    }
  }
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(1.0f,1.0f);

  if (se->appearance == APPEARANCE_PARALLEL_BANDS)
  {
    for (i=0; i<num_theta; i++)
    {
      if (((i & numb_dist_mask) >= numb_dist_min) &&
          ((i & numb_dist_mask) < numb_dist_max))
        continue;
      glBegin(GL_TRIANGLE_STRIP);
      for (j=num_phi; j>=0; j--)
      {
        for (k=0; k<=1; k++)
        {
          l = (i+k);
          m = j;
          o = m*(num_theta+1)+l;
          phi = phi_range*m/num_phi+phi_min;
          theta = theta_range*l/num_theta+theta_min;
          xx = &se->se[3*o];
          xn = &se->sen[3*o];
          for (l=0; l<3; l++)
          {
            r = 0.0f;
            s = 0.0f;
            for (m=0; m<3; m++)
            {
              r += mat[l][m]*xx[m];
              s += mat[l][m]*xn[m];
            }
            p[l] = r+se->offset3d[l];
            n[l] = s;
          }
          if (se->colors != COLORS_TWOSIDED)
          {
            cf = &se->colf[4*o];
            cb = &se->colb[4*o];
            glColor3fv(cf);
            glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,cf);
            glMaterialfv(GL_BACK,GL_AMBIENT_AND_DIFFUSE,cb);
          }
          glNormal3fv(n);
          glVertex3fv(p);
          polys++;
        }
      }
      glEnd();
    }
  }
  else /* se->appearance != APPEARANCE_PARALLEL_BANDS */
  {
    for (j=0; j<num_phi; j++)
    {
      if (se->appearance == APPEARANCE_MERIDIAN_BANDS &&
          ((j & numb_dir_mask) >= numb_dir_min) &&
          ((j & numb_dir_mask) < numb_dir_max))
        continue;
      glBegin(GL_TRIANGLE_STRIP);
      for (i=0; i<=num_theta; i++)
      {
        for (k=0; k<=1; k++)
        {
          l = i;
          m = (j+k);
          o = m*(num_theta+1)+l;
          phi = phi_range*m/num_phi+phi_min;
          theta = theta_range*l/num_theta+theta_min;
          xx = &se->se[3*o];
          xn = &se->sen[3*o];
          for (l=0; l<3; l++)
          {
            r = 0.0f;
            s = 0.0f;
            for (m=0; m<3; m++)
            {
              r += mat[l][m]*xx[m];
              s += mat[l][m]*xn[m];
            }
            p[l] = r+se->offset3d[l];
            n[l] = s;
          }
          if (se->colors != COLORS_TWOSIDED)
          {
            cf = &se->colf[4*o];
            cb = &se->colb[4*o];
            glColor3fv(cf);
            glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,cf);
            glMaterialfv(GL_BACK,GL_AMBIENT_AND_DIFFUSE,cb);
          }
          glNormal3fv(n);
          glVertex3fv(p);
          polys++;
        }
      }
      glEnd();
    }
  }
  glDisable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(0.0f,0.0f);
  if (se->graticule)
  {
    glColor4f(1.0f,1.0f,1.0f,1.0f);
    glLineWidth(2.0f);
    glEnable(GL_LINE_SMOOTH);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
    glEnable(GL_BLEND);
    glHint(GL_LINE_SMOOTH_HINT,GL_DONT_CARE);
    /* Draw meridians. */
    for (j=0; j<num_phi; j+=num_grid)
    {
      glBegin(GL_LINE_STRIP);
      for (i=0; i<=num_theta; i++)
      {
        o = j*(num_theta+1)+i;
        phi = phi_range*j/num_phi+phi_min;
        theta = theta_range*i/num_theta+theta_min;
        xx = &se->se[3*o];
        for (l=0; l<3; l++)
        {
          r = 0.0f;
          for (m=0; m<3; m++)
            r += mat[l][m]*xx[m];
          p[l] = r+se->offset3d[l];
        }
        glVertex3fv(p);
      }
      glEnd();
    }
    /* Draw parallels. */
    for (i=num_grid; i<=num_theta-num_grid; i+=num_grid)
    {
      glBegin(GL_LINE_LOOP);
      for (j=num_phi-1; j>=0; j--)
      {
        o = j*(num_theta+1)+i;
        phi = phi_range*j/num_phi+phi_min;
        theta = theta_range*i/num_theta+theta_min;
        xx = &se->se[3*o];
        for (l=0; l<3; l++)
        {
          r = 0.0f;
          for (m=0; m<3; m++)
            r += mat[l][m]*xx[m];
          p[l] = r+se->offset3d[l];
        }
        glVertex3fv(p);
      }
      glEnd();
    }
    glLineWidth(1.0f);
    glPolygonOffset(0.0f,0.0f);
    glDisable(GL_LINE_SMOOTH);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    if (se->display_mode != DISP_TRANSPARENT)
      glDisable(GL_BLEND);
  }

  polys /= 2;
  return polys;
}


static void init(ModeInfo *mi)
{
  sphereeversionstruct *se = &sphereeversion[MI_SCREEN(mi)];

  if (deform_speed == 0.0f)
    deform_speed = 10.0f;

  se->alpha = frand(120.0)-60.0;
  se->beta = frand(120.0)-60.0;
  se->delta = frand(360.0);

  se->eta_min = BEDNORZ_ETA_MIN;
  se->beta_max = BEDNORZ_BETA_MAX;

  se->anim_state = ANIM_DEFORM;
  se->tau = BEDNORZ_TAU_MAX;
  se->defdir = -1;
  se->turn_step = 0;
  se->num_turn = 0;

  se->offset3d[0] = 0.0f;
  se->offset3d[1] = 0.0f;
  se->offset3d[2] = -1.8f;

  setup_surface_colors(mi,-M_PI,M_PI,-M_PI/2.0,M_PI/2.0,NUMPH,NUMTH);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  if (se->projection == DISP_PERSPECTIVE)
    gluPerspective(60.0,1.0,0.1,10.0);
  else
    glOrtho(-1.0,1.0,-1.0,1.0,0.1,10.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}


/* Redisplay the deformed surface. */
static void display_sphereeversion(ModeInfo *mi)
{
  float alpha, beta, delta, dot, a, t, q[4];
  sphereeversionstruct *se = &sphereeversion[MI_SCREEN(mi)];

  if (!se->button_pressed)
  {
    if (se->anim_state == ANIM_DEFORM)
    {
      se->tau += se->defdir*deform_speed*0.001;
      if (se->tau < BEDNORZ_TAU_MIN)
      {
        se->tau = BEDNORZ_TAU_MIN;
        se->defdir = -se->defdir;
        se->anim_state = ANIM_TURN;
      }
      if (se->tau > BEDNORZ_TAU_MAX)
      {
        se->tau = BEDNORZ_TAU_MAX;
        se->defdir = -se->defdir;
        se->anim_state = ANIM_TURN;
      }
      if (se->anim_state == ANIM_TURN)
      {
        /* Convert the current rotation angles to a quaternion. */
        angles_to_quat(se->alpha,se->beta,se->delta,se->qs);
        /* Determine random target rotation angles and  convert them to the
           target quaternion. */
        alpha = frand(120.0)-60.0;
        beta = frand(120.0)-60.0;
        delta = frand(360.0);
        angles_to_quat(alpha,beta,delta,se->qe);
        /* Compute the angle between qs and qe. */
        dot = (se->qs[0]*se->qe[0]+se->qs[1]*se->qe[1]+
               se->qs[2]*se->qe[2]+se->qs[3]*se->qe[3]);
        if (dot < 0.0f)
        {
          se->qe[0] = -se->qe[0];
          se->qe[1] = -se->qe[1];
          se->qe[2] = -se->qe[2];
          se->qe[3] = -se->qe[3];
          dot = -dot;
        }
        a = 180.0/M_PI*acos(dot);
        se->num_turn = ceil(a/TURN_STEP);

        /* Change the parameters randomly after one full eversion when a
           turn to the new orientation starts. */
        /* Randomly select new display modes for the main pass. */
        if (se->random_display_mode)
          se->display_mode = random() % NUM_DISPLAY_MODES;
        if (se->random_appearance)
          se->appearance = random() % NUM_APPEARANCES;
        if (se->random_colors)
        {
          se->colors = random() % NUM_COLORS;
          setup_surface_colors(mi,-M_PI,M_PI,-M_PI/2.0,M_PI/2.0,NUMPH,NUMTH);
        }
        if (se->random_graticule)
          se->graticule = random() & 1;
        if (se->random_g)
          se->g = random() % 4 + 2;
      }
    }
    else /* se->anim_state == ANIM_TURN */
    {
      t = (float)se->turn_step/(float)se->num_turn;
      /* Apply an easing function to t. */
      t = (3.0-2.0*t)*t*t;
      quat_slerp(t,se->qs,se->qe,q);
      quat_to_angles(q,&se->alpha,&se->beta,&se->delta);
      se->turn_step++;
      if (se->turn_step > se->num_turn)
      {
        se->turn_step = 0;
        se->anim_state = ANIM_DEFORM;
      }
    }

    if (se->anim_state == ANIM_DEFORM)
    {
      se->alpha += speed_x*se->speed_scale;
      if (se->alpha >= 360.0f)
        se->alpha -= 360.0f;
      se->beta += speed_y*se->speed_scale;
      if (se->beta >= 360.0f)
        se->beta -= 360.0f;
      se->delta += speed_z*se->speed_scale;
      if (se->delta >= 360.0f)
        se->delta -= 360.0f;
    }
  }

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  if (se->projection == DISP_PERSPECTIVE)
  {
      gluPerspective(60.0,se->aspect,0.1,10.0);
  }
  else
  {
    if (se->aspect >= 1.0)
      glOrtho(-se->aspect,se->aspect,-1.0,1.0,0.1,10.0);
    else
      glOrtho(-1.0,1.0,-1.0/se->aspect,1.0/se->aspect,0.1,10.0);
  }
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  mi->polygon_count = bednorz_shpere_eversion(mi,-M_PI,M_PI,-M_PI/2.0,
                                              M_PI/2.0,NUMPH,NUMTH,
                                              NUMBDIST,NUMBDIR,
                                              NUMGRID);
}


ENTRYPOINT void reshape_sphereeversion(ModeInfo *mi, int width, int height)
{
  sphereeversionstruct *se = &sphereeversion[MI_SCREEN(mi)];
  int y = 0;

  if (width > height * 5) {   /* tiny window: show middle */
    height = width;
    y = -height/2;
  }

  se->WindW = (GLint)width;
  se->WindH = (GLint)height;
  glViewport(0,y,width,height);
  se->aspect = (GLfloat)width/(GLfloat)height;
}


ENTRYPOINT Bool sphereeversion_handle_event(ModeInfo *mi, XEvent *event)
{
  sphereeversionstruct *se = &sphereeversion[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress && event->xbutton.button == Button1)
  {
    se->button_pressed = True;
    gltrackball_start(se->trackball, event->xbutton.x, event->xbutton.y,
                      MI_WIDTH(mi), MI_HEIGHT(mi));
    return True;
  }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
  {
    se->button_pressed = False;
    return True;
  }
  else if (event->xany.type == MotionNotify && se->button_pressed)
  {
    gltrackball_track(se->trackball, event->xmotion.x, event->xmotion.y,
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
 *    Initialize sphereeversion.  Called each time the window changes.
 *-----------------------------------------------------------------------------
 */

ENTRYPOINT void init_sphereeversion(ModeInfo *mi)
{
  sphereeversionstruct *se;

  MI_INIT (mi, sphereeversion);
  se = &sphereeversion[MI_SCREEN(mi)];

  se->se = calloc(3*(NUMPH+1)*(NUMTH+1),sizeof(float));
  se->sen = calloc(3*(NUMPH+1)*(NUMTH+1),sizeof(float));
  se->colf = calloc(4*(NUMPH+1)*(NUMTH+1),sizeof(float));
  se->colb = calloc(4*(NUMPH+1)*(NUMTH+1),sizeof(float));

  se->trackball = gltrackball_init(True);
  se->button_pressed = False;

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

  /* Set the display mode. */
  if (!strcasecmp(mode,"surface"))
  {
    se->display_mode = DISP_SURFACE;
    se->random_display_mode = False;
  }
  else if (!strcasecmp(mode,"transparent"))
  {
    se->display_mode = DISP_TRANSPARENT;
    se->random_display_mode = False;
  }
  else
  {
    se->display_mode = random() % NUM_DISPLAY_MODES;
    se->random_display_mode = True;
  }

  /* Set the appearance. */
  if (!strcasecmp(appear,"solid"))
  {
    se->appearance = APPEARANCE_SOLID;
    se->random_appearance = False;
  }
  else if (!strcasecmp(appear,"parallel-bands"))
  {
    se->appearance = APPEARANCE_PARALLEL_BANDS;
    se->random_appearance = False;
  }
  else if (!strcasecmp(appear,"meridian-bands"))
  {
    se->appearance = APPEARANCE_MERIDIAN_BANDS;
    se->random_appearance = False;
  }
  else
  {
    se->appearance = random() % NUM_APPEARANCES;
    se->random_appearance = True;
  }

  /* Set the color mode. */
  if (!strcasecmp(color_mode,"two-sided"))
  {
    se->colors = COLORS_TWOSIDED;
    se->random_colors = False;
  }
  else if (!strcasecmp(color_mode,"parallel"))
  {
    se->colors = COLORS_PARALLEL;
    se->random_colors = False;
  }
  else if (!strcasecmp(color_mode,"meridian"))
  {
    se->colors = COLORS_MERIDIAN;
    se->random_colors = False;
  }
  else
  {
    se->colors = random() % NUM_COLORS;
    se->random_colors = True;
  }

  /* Set the graticule mode. */
  if (!strcasecmp(graticule,"on"))
  {
    se->graticule = True;
    se->random_graticule = False;
  }
  else if (!strcasecmp(graticule,"off"))
  {
    se->graticule = False;
    se->random_graticule = False;
  }
  else
  {
    se->graticule = random() & 1;
    se->random_graticule = True;
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

  /* Make multiple screens rotate at slightly different rates. */
  se->speed_scale = 0.9+frand(0.3);

  if ((se->glx_context = init_GL(mi)) != NULL)
  {
    reshape_sphereeversion(mi,MI_WIDTH(mi),MI_HEIGHT(mi));
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

  glXMakeCurrent(display, window, *se->glx_context);

  glLoadIdentity();

  display_sphereeversion(mi);

  if (MI_IS_FPS(mi))
    do_fps (mi);

  glFlush();

  glXSwapBuffers(display,window);
}


/*
 *-----------------------------------------------------------------------------
 *    The display is being taken away from us.  Free up malloc'ed 
 *      memory and X resources that we've alloc'ed.
 *-----------------------------------------------------------------------------
 */

ENTRYPOINT void free_sphereeversion(ModeInfo *mi)
{
  sphereeversionstruct *se = &sphereeversion[MI_SCREEN(mi)];

  if (!se->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *se->glx_context);

  if (se->se) free(se->se);
  if (se->sen) free(se->sen);
  if (se->colf) free(se->colf);
  if (se->colb) free(se->colb);
  gltrackball_free(se->trackball);
}

#ifndef STANDALONE
ENTRYPOINT void change_sphereeversion(ModeInfo *mi)
{
  sphereeversionstruct *ev = &sphereeversion[MI_SCREEN(mi)];

  if (!ev->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *ev->glx_context);
  init(mi);
}
#endif /* !STANDALONE */

XSCREENSAVER_MODULE ("SphereEversion", sphereeversion)

#endif /* USE_GL */
