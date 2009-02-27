/* hypertorus --- Shows a hypertorus that rotates in 4d */

#if 0
static const char sccsid[] = "@(#)hypertorus.c  1.1 03/05/18 xlockmore";
#endif

/* Copyright (c) 2003 Carsten Steger <carsten@mirsanmir.org>. */

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
 * C. Steger - 03/05/18: Initial version
 */

/*
 * This program shows the Clifford torus as it rotates in 4d.  The Clifford
 * torus is a torus lies on the "surface" of the hypersphere in 4d.  The
 * program projects the 4d torus to 3d using either a perspective or an
 * orthographic projection.  Of the two alternatives, the perspecitve
 * projection looks much more appealing.  In orthographic projections the
 * torus degenerates into a doubly covered cylinder for some angles.  The
 * projected 3d torus can then be projected to the screen either perspectively
 * or orthographically.  There are three display modes for the torus: mesh
 * (wireframe), solid, or transparent.  Furthermore, the appearance of the
 * torus can be as a solid object or as a set of see-through bands.  Finally,
 * the colors with with the torus is drawn can be set to either two-sided or
 * to colorwheel.  In the first case, the torus is drawn with red on the
 * outside and green on the inside.  This mode enables you to see that the
 * torus turns inside-out as it rotates in 4d.  The second mode draws the
 * torus in a fully saturated color wheel.  This gives a very nice effect
 * when combined with the see-through bands mode.  The rotation speed for
 * each of the six planes around which the torus rotates can be chosen.
 * This program is very much inspired by Thomas Banchoff's book "Beyond the
 * Third Dimension: Geometry, Computer Graphics, and Higher Dimensions",
 * Scientific American Library, 1990.
 */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DISP_WIREFRAME            0
#define DISP_WIREFRAME_STR       "0"
#define DISP_SURFACE              1
#define DISP_SURFACE_STR         "1"
#define DISP_TRANSPARENT          2
#define DISP_TRANSPARENT_STR     "2"

#define APPEARANCE_SOLID          0
#define APPEARANCE_SOLID_STR     "0"
#define APPEARANCE_BANDS          1
#define APPEARANCE_BANDS_STR     "1"

#define COLORS_TWOSIDED           0
#define COLORS_TWOSIDED_STR      "0"
#define COLORS_COLORWHEEL         1
#define COLORS_COLORWHEEL_STR    "1"

#define DISP_3D_PERSPECTIVE       0
#define DISP_3D_PERSPECTIVE_STR  "0"
#define DISP_3D_ORTHOGRAPHIC      1
#define DISP_3D_ORTHOGRAPHIC_STR "1"

#define DISP_4D_PERSPECTIVE       0
#define DISP_4D_PERSPECTIVE_STR  "0"
#define DISP_4D_ORTHOGRAPHIC      1
#define DISP_4D_ORTHOGRAPHIC_STR "1"

#define DALPHA                    1.1
#define DALPHA_STR               "1.1"
#define DBETA                     1.3
#define DBETA_STR                "1.3"
#define DDELTA                    1.5
#define DDELTA_STR               "1.5"
#define DZETA                     1.7
#define DZETA_STR                "1.7"
#define DETA                      1.9
#define DETA_STR                 "1.9"
#define DTHETA                    2.1
#define DTHETA_STR               "2.1"

#define DEF_DISPLAY_MODE          DISP_SURFACE_STR   
#define DEF_APPEARANCE            APPEARANCE_BANDS_STR
#define DEF_COLORS                COLORS_COLORWHEEL_STR
#define DEF_3D_PROJECTION         DISP_3D_PERSPECTIVE_STR
#define DEF_4D_PROJECTION         DISP_4D_PERSPECTIVE_STR
#define DEF_DALPHA                DALPHA_STR
#define DEF_DBETA                 DBETA_STR
#define DEF_DDELTA                DDELTA_STR
#define DEF_DZETA                 DZETA_STR
#define DEF_DETA                  DETA_STR
#define DEF_DTHETA                DTHETA_STR

#ifdef STANDALONE
# define PROGCLASS       "Hypertorus"
# define HACK_INIT       init_hypertorus
# define HACK_DRAW       draw_hypertorus
# define HACK_RESHAPE    reshape_hypertorus
# define hypertorus_opts xlockmore_opts
# define DEFAULTS        "*delay:      25000 \n" \
                         "*showFPS:    False \n" \
                         "*wireframe:  False \n" \
                         "*displayMode: "  DEF_DISPLAY_MODE  " \n" \
                         "*appearance: "   DEF_APPEARANCE    " \n" \
                         "*colors: "       DEF_COLORS        " \n" \
                         "*projection3d: " DEF_3D_PROJECTION " \n" \
                         "*projection4d: " DEF_4D_PROJECTION " \n" \
                         "speedwx: "       DEF_DALPHA        " \n" \
                         "speedwy: "       DEF_DBETA         " \n" \
                         "speedwz: "       DEF_DDELTA        " \n" \
                         "speedxy: "       DEF_DZETA         " \n" \
                         "speedxz: "       DEF_DETA          " \n" \
                         "speedyz: "       DEF_DTHETA        " \n"
# include "xlockmore.h"         /* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"             /* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL

#include <GL/gl.h>
#include <GL/glu.h>


#ifdef USE_MODULES
ModStruct   hypertorus_description =
{"hypertorus", "init_hypertorus", "draw_hypertorus", "release_hypertorus",
 "draw_hypertorus", "change_hypertorus", NULL, &hypertorus_opts,
 25000, 1, 1, 1, 1.0, 4, "",
 "Shows a hypertorus rotating in 4d", 0, NULL};

#endif


static int display_mode;
static int appearance;
static int colors;
static int projection_3d;
static int projection_4d;
static float speed_wx;
static float speed_wy;
static float speed_wz;
static float speed_xy;
static float speed_xz;
static float speed_yz;

/* 4D rotation angles */
static float alpha, beta, delta, zeta, eta, theta;
static float aspect;

static const float offset4d[4] = {
  0.0, 0.0, 0.0, 2.0
};

static const float offset3d[4] = {
  0.0, 0.0, -2.0, 0.0
};


static XrmOptionDescRec opts[] =
{
  {"-mesh",            ".hypertorus.displayMode",  XrmoptionNoArg,
                       DISP_WIREFRAME_STR },
  {"-surface",         ".hypertorus.displayMode",  XrmoptionNoArg,
                       DISP_SURFACE_STR },
  {"-transparent",     ".hypertorus.displayMode",  XrmoptionNoArg,
                       DISP_TRANSPARENT_STR },
  {"-solid",           ".hypertorus.appearance",   XrmoptionNoArg,
                       APPEARANCE_SOLID_STR },
  {"-bands",           ".hypertorus.appearance",   XrmoptionNoArg,
                       APPEARANCE_BANDS_STR },
  {"-twosided",        ".hypertorus.colors",       XrmoptionNoArg,
                       COLORS_TWOSIDED_STR },
  {"-colorwheel",      ".hypertorus.colors",       XrmoptionNoArg,
                       COLORS_COLORWHEEL_STR },
  {"-perspective-3d",  ".hypertorus.projection3d", XrmoptionNoArg,
                       DISP_3D_PERSPECTIVE_STR },
  {"-orthographic-3d", ".hypertorus.projection3d", XrmoptionNoArg,
                       DISP_3D_ORTHOGRAPHIC_STR },
  {"-perspective-4d",  ".hypertorus.projection4d", XrmoptionNoArg,
                       DISP_4D_PERSPECTIVE_STR },
  {"-orthographic-4d", ".hypertorus.projection4d", XrmoptionNoArg,
                       DISP_4D_ORTHOGRAPHIC_STR },
  {"-speed-wx",        ".hypertorus.speedwx",      XrmoptionSepArg, 0 },
  {"-speed-wy",        ".hypertorus.speedwy",      XrmoptionSepArg, 0 },
  {"-speed-wz",        ".hypertorus.speedwz",      XrmoptionSepArg, 0 },
  {"-speed-xy",        ".hypertorus.speedxy",      XrmoptionSepArg, 0 },
  {"-speed-xz",        ".hypertorus.speedxz",      XrmoptionSepArg, 0 },
  {"-speed-yz",        ".hypertorus.speedyz",      XrmoptionSepArg, 0 }
};

static argtype vars[] =
{
  { &display_mode,  "displayMode",  "DisplayMode",
    DEF_DISPLAY_MODE,  t_Int },
  { &appearance,    "appearance",   "Appearance",
    DEF_APPEARANCE,    t_Int },
  { &colors,        "colors",       "Colors",
    DEF_COLORS,        t_Int },
  { &projection_3d, "projection3d", "Projection3d",
    DEF_3D_PROJECTION, t_Int },
  { &projection_4d, "projection4d", "Projection4d",
    DEF_4D_PROJECTION, t_Int },
  { &speed_wx,      "speedwx",      "Speedwx",
    DEF_DALPHA,        t_Float},
  { &speed_wy,      "speedwy",      "Speedwy",
    DEF_DBETA,         t_Float},
  { &speed_wz,      "speedwz",      "Speedwz",
    DEF_DDELTA,        t_Float},
  { &speed_xy,      "speedxy",      "Speedxy",
    DEF_DZETA,         t_Float},
  { &speed_xz,      "speedxz",      "Speedxz",
    DEF_DETA,          t_Float},
  { &speed_yz,      "speedyz",      "Speedyz",
    DEF_DTHETA,        t_Float}
};

static OptionStruct desc[] =
{
  { "-mesh",            "display the torus as a wireframe mesh" },
  { "-surface",         "display the torus as a solid surface" },
  { "-transparent",     "display the torus as a transparent surface" },
  { "-solid",           "display the torus as a solid object" },
  { "-bands",           "display the torus as see-through bands" },
  { "-twosided",        "display the torus with two colors" },
  { "-colorwheel",      "display the torus with a smooth color wheel" },
  { "-perspective-3d",  "project the torus perspectively from 3d to 2d" },
  { "-orthographic-3d", "project the torus orthographically from 3d to 2d" },
  { "-perspective-4d",  "project the torus perspectively from 4d to 3d" },
  { "-orthographic-4d", "project the torus orthographically from 4d to 3d" },
  { "-speed-wx <arg>",  "rotation speed around the wx plane" },
  { "-speed-wy <arg>",  "rotation speed around the wy plane" },
  { "-speed-wz <arg>",  "rotation speed around the wz plane" },
  { "-speed-xy <arg>",  "rotation speed around the xy plane" },
  { "-speed-xz <arg>",  "rotation speed around the xz plane" },
  { "-speed-yz <arg>",  "rotation speed around the yz plane" }
};

ModeSpecOpt hypertorus_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};


typedef struct {
  GLint       WindH, WindW;
  GLXContext *glx_context;
} hypertorusstruct;

static hypertorusstruct *hyper = (hypertorusstruct *) NULL;


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


/* Compute a fully saturated and bright color based on an angle. */
static void color(double angle)
{
  int s;
  double t;
  float color[4];

  if (colors != COLORS_COLORWHEEL)
    return;

  if (angle >= 0.0)
    angle = fmod(angle,2*M_PI);
  else
    angle = fmod(angle,-2*M_PI);
  s = floor(angle/(M_PI/3));
  t = angle/(M_PI/3)-s;
  if (s >= 6)
    s = 0;
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
  if (display_mode == DISP_TRANSPARENT)
    color[3] = 0.5;
  else
    color[3] = 1.0;
  glColor3fv(color);
  glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,color);
}


/* Draw a hyperturus projected into 3D. */
static void hypertorus(double umin, double umax, double vmin, double vmax,
                       int numu, int numv)
{
  static GLfloat mat_diff_red[] = { 1.0, 0.0, 0.0, 1.0 };
  static GLfloat mat_diff_green[] = { 0.0, 1.0, 0.0, 1.0 };
  static GLfloat mat_diff_trans_red[] = { 1.0, 0.0, 0.0, 0.5 };
  static GLfloat mat_diff_trans_green[] = { 0.0, 1.0, 0.0, 0.5 };
  float p[3], pu[3], pv[3], n[3], mat[4][4];
  int i, j, k, l, m;
  double u, v, ur, vr;
  double cu, su, cv, sv;
  double xx[4], xxu[4], xxv[4], x[4], xu[4], xv[4];
  double r, s, t;

  /* Compute the rotation that rotates the hypercube in 4D. */
  for (i=0; i<4; i++)
    for (j=0; j<4; j++)
      mat[i][j] = (i==j);
  rotatewx(mat,alpha);
  rotatewy(mat,beta);
  rotatewz(mat,delta);
  rotatexy(mat,zeta);
  rotatexz(mat,eta);
  rotateyz(mat,theta);

  if (colors != COLORS_COLORWHEEL)
  {
    glColor3fv(mat_diff_red);
    if (display_mode == DISP_TRANSPARENT)
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

  ur = umax-umin;
  vr = vmax-vmin;
  for (i=0; i<numu; i++)
  {
    if (appearance == APPEARANCE_BANDS && ((i & 3) >= 2))
      continue;
    if (display_mode == DISP_WIREFRAME)
      glBegin(GL_QUAD_STRIP);
    else
      glBegin(GL_TRIANGLE_STRIP);
    for (j=0; j<=numv; j++)
    {
      for (k=0; k<=1; k++)
      {
        l = (i+k);
        m = j;
        u = ur*l/numu+umin;
        v = vr*m/numv+vmin;
        color(u);
        cu = cos(u);
        su = sin(u);
        cv = cos(v);
        sv = sin(v);
        xx[0] = cu;
        xx[1] = su;
        xx[2] = cv;
        xx[3] = sv;
        xxu[0] = -su;
        xxu[1] = cu;
        xxu[2] = 0.0;
        xxu[3] = 0.0;
        xxv[0] = 0.0;
        xxv[1] = 0.0;
        xxv[2] = -sv;
        xxv[3] = cv;
        for (l=0; l<4; l++)
        {
          r = 0.0;
          s = 0.0;
          t = 0.0;
          for (m=0; m<4; m++)
          {
            r += mat[l][m]*xx[m];
            s += mat[l][m]*xxu[m];
            t += mat[l][m]*xxv[m];
          }
          x[l] = r;
          xu[l] = s;
          xv[l] = t;
        }
        if (projection_4d == DISP_4D_ORTHOGRAPHIC)
        {
          for (l=0; l<3; l++)
          {
            p[l] = (x[l]+offset4d[l])/1.5+offset3d[l];
            pu[l] = xu[l];
            pv[l] = xv[l];
          }
        }
        else
        {
          s = x[3]+offset4d[3];
          t = s*s;
          for (l=0; l<3; l++)
          {
            r = x[l]+offset4d[l];
            p[l] = r/s+offset3d[l];
            pu[l] = (xu[l]*s-r*xu[3])/t;
            pv[l] = (xv[l]*s-r*xv[3])/t;
          }
        }
        n[0] = pu[1]*pv[2]-pu[2]*pv[1];
        n[1] = pu[2]*pv[0]-pu[0]*pv[2];
        n[2] = pu[0]*pv[1]-pu[1]*pv[0];
        t = sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
        n[0] /= t;
        n[1] /= t;
        n[2] /= t;
        glNormal3fv(n);
        glVertex3fv(p);
      }
    }
    glEnd();
  }
}


static void init(ModeInfo *mi)
{
  static GLfloat light_ambient[] = { 0.0, 0.0, 0.0, 1.0 };
  static GLfloat light_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
  static GLfloat light_specular[] = { 1.0, 1.0, 1.0, 1.0 };
  static GLfloat light_position[] = { 1.0, 1.0, 1.0, 0.0 };
  static GLfloat mat_specular[] = { 1.0, 1.0, 1.0, 1.0 };

  alpha = 0.0;
  beta = 0.0;
  delta = 0.0;
  zeta = 0.0;
  eta = 0.0;
  theta = 0.0;

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
    glShadeModel(GL_FLAT);
    glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
    glDisable(GL_BLEND);
  }
  else if (display_mode == DISP_SURFACE)
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
  else if (display_mode == DISP_TRANSPARENT)
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
  else
  {
    glDisable(GL_DEPTH_TEST);
    glShadeModel(GL_FLAT);
    glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
    glDisable(GL_BLEND);
  }
}


/* Redisplay the hypertorus. */
static void display_hypertorus(void)
{
  alpha += speed_wx;
  if (alpha >= 360.0)
    alpha -= 360.0;
  beta += speed_wy;
  if (beta >= 360.0)
    beta -= 360.0;
  delta += speed_wz;
  if (delta >= 360.0)
    delta -= 360.0;
  zeta += speed_xy;
  if (zeta >= 360.0)
    zeta -= 360.0;
  eta += speed_xz;
  if (eta >= 360.0)
    eta -= 360.0;
  theta += speed_yz;
  if (theta >= 360.0)
    theta -= 360.0;

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  if (projection_3d == DISP_3D_ORTHOGRAPHIC)
  {
    if (aspect >= 1.0)
      glOrtho(-aspect,aspect,-1.0,1.0,0.1,100.0);
    else
      glOrtho(-1.0,1.0,-1.0/aspect,1.0/aspect,0.1,100.0);
  }
  else
  {
    gluPerspective(60.0,aspect,0.1,100.0);
  }
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  if (display_mode == DISP_WIREFRAME)
    hypertorus(0.0,2.0*M_PI,0.0,2.0*M_PI,40,40);
  else
    hypertorus(0.0,2.0*M_PI,0.0,2.0*M_PI,60,60);
}


void reshape_hypertorus(ModeInfo * mi, int width, int height)
{
  hypertorusstruct *hp = &hyper[MI_SCREEN(mi)];

  hp->WindW = (GLint)width;
  hp->WindH = (GLint)height;
  glViewport(0,0,width,height);
  aspect = (GLfloat)width/(GLfloat)height;
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
 *    Initialize hypertorus.  Called each time the window changes.
 *-----------------------------------------------------------------------------
 */

void init_hypertorus(ModeInfo * mi)
{
  hypertorusstruct *hp;

  if (hyper == NULL)
  {
    hyper = (hypertorusstruct *)calloc(MI_NUM_SCREENS(mi),
                                       sizeof(hypertorusstruct));
    if (hyper == NULL)
      return;
  }
  hp = &hyper[MI_SCREEN(mi)];

  if ((hp->glx_context = init_GL(mi)) != NULL)
  {
    reshape_hypertorus(mi,MI_WIDTH(mi),MI_HEIGHT(mi));
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
void draw_hypertorus(ModeInfo * mi)
{
  Display          *display = MI_DISPLAY(mi);
  Window           window = MI_WINDOW(mi);
  hypertorusstruct *hp;

  if (hyper == NULL)
    return;
  hp = &hyper[MI_SCREEN(mi)];

  MI_IS_DRAWN(mi) = True;
  if (!hp->glx_context)
    return;

  glXMakeCurrent(display,window,*(hp->glx_context));

  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();

  display_hypertorus();

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

void release_hypertorus(ModeInfo * mi)
{
  if (hyper != NULL)
  {
    int screen;

    for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
    {
      hypertorusstruct *hp = &hyper[screen];

      if (hp->glx_context)
        hp->glx_context = (GLXContext *)NULL;
    }
    (void) free((void *)hyper);
    hyper = (hypertorusstruct *)NULL;
  }
  FreeAllGL(mi);
}

void change_hypertorus(ModeInfo * mi)
{
  hypertorusstruct *hp = &hyper[MI_SCREEN(mi)];

  if (!hp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi),MI_WINDOW(mi),*(hp->glx_context));
  init(mi);
}

#endif /* USE_GL */
