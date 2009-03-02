/*
 * engine.c - GL representation of a 4 stroke engine
 *
 * version 1.0
 *
 * Copyright (C) 2001 Ben Buxton (bb@cactii.net)
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */


#include <X11/Intrinsic.h>

#ifdef STANDALONE
# define PROGCLASS                                      "Engine"
# define HACK_INIT                                      init_engine
# define HACK_DRAW                                      draw_engine
# define HACK_RESHAPE                           reshape_engine
# define engine_opts                                     xlockmore_opts
/* insert defaults here */

#define DEFAULTS        "*delay:       10000       \n" \
                        "*showFPS:       False       \n" \
                        "*move:       True       \n" \
                        "*spin:       True       \n" \
                        "*rotatespeed:      1\n" \

# include "xlockmore.h"                         /* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"                                     /* from the xlockmore distribution */
#endif /* !STANDALONE */

/* lifted from lament.c */
#define RAND(n) ((long) ((random() & 0x7fffffff) % ((long) (n))))
#define RANDSIGN() ((random() & 1) ? 1 : -1)


#ifdef USE_GL

#include <GL/glu.h>


static int rotatespeed;
static int move;
static int spin;

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

static XrmOptionDescRec opts[] = {
  {"-rotate-speed", ".engine.rotatespeed", XrmoptionSepArg, "1" },
  {"-move", ".engine.move", XrmoptionNoArg, (caddr_t) "true" },
  {"+move", ".engine.move", XrmoptionNoArg, (caddr_t) "false" },
  {"-spin", ".engine.spin", XrmoptionNoArg, (caddr_t) "true" },
  {"+spin", ".engine.spin", XrmoptionNoArg, (caddr_t) "false" },
};

static argtype vars[] = {
  {(caddr_t *) &rotatespeed, "rotatespeed", "Rotatespeed", "1", t_Int},
  {(caddr_t *) &move, "move", "Move", "True", t_Bool},
  {(caddr_t *) &spin, "spin", "Spin", "True", t_Bool},
};

ModeSpecOpt engine_opts = {countof(opts), opts, countof(vars), vars, NULL};

#ifdef USE_MODULES
ModStruct   engine_description =
{"engine", "init_engine", "draw_engine", "release_engine",
 "draw_engine", "init_engine", NULL, &engine_opts,
 1000, 1, 2, 1, 4, 1.0, "",
 "A four stroke engine", 0, NULL};

#endif


typedef struct {
  GLXContext *glx_context;
  Window window;
  GLfloat x, y, z; /* position */
  GLfloat dx, dy, dz; /* position */
  GLfloat an1, an2, an3; /* internal angle */
  GLfloat nx, ny, nz; /* spin vector */
  GLfloat a; /* spin angle */
  GLfloat da; /* spin speed */
} Engine;

static Engine *engine = NULL;

#include <math.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265
#endif

#define MOVE_MULT 0.05

#define RAND_RANGE(min, max) ((min) + (max - min) * f_rand())

int win_w, win_h;

static GLfloat viewer[] = {0.0, 0.0, 30.0};
static GLfloat lookat[] = {0.0, 0.0, 0.0};
static GLfloat lightpos[] = {7.0, 7.0, 12, 1.0};
GLfloat light_sp[] = {0.8, 0.8, 0.8, 0.5};
static GLfloat red[] = {1.0, 0, 0, 1.0};
static GLfloat green[] = {0.0, 1, 0, 1.0};
static GLfloat blue[] = {0, 0, 1, 1.0};
static GLfloat white[] = {1.0, 1, 1, 1.0};
static GLfloat yellow_t[] = {1, 1, 0, 0.4};

void circle(float, int,int);
GLvoid normal(GLfloat [], GLfloat [], GLfloat [], 
                  GLfloat *, GLfloat *, GLfloat *);

float sin_table[720];
float cos_table[720];
float tan_table[720];

/* we use trig tables to speed things up - 200 calls to sin()
 in one frame can be a bit harsh..
*/

void make_tables(void) {
int i;
float f;

  f = 360 / (M_PI * 2);
  for (i = 0 ; i <= 720 ; i++) {
    sin_table[i] = sin(i/f);
  }
  for (i = 0 ; i <= 720 ; i++) {
    cos_table[i] = cos(i/f);
  }
  for (i = 0 ; i <= 720 ; i++) {
    tan_table[i] = tan(i/f);
  }
}

/* if inner and outer are the same, we draw a cylinder, not a tube */
/* for a tube, endcaps is 0 (none), 1 (left), 2 (right) or 3(both) */
/* angle is how far around the axis to go (up to 360) */

void cylinder (GLfloat x, GLfloat y, GLfloat z, float length, float outer, float inner, int endcaps, int sang,
                 int eang) {
int a; /* current angle around cylinder */
int b = 0; /* previous */
int angle, norm, step, sangle;
float z1, y1, z2, y2, ex=0;
float y3, z3;
float Z1, Y1, Z2, Y2, xl, Y3, Z3;
GLfloat y2c[720], z2c[720];
GLfloat ony, onz; /* previous normals */
int nsegs, tube = 0;

  glPushMatrix();
  nsegs = outer*(MAX(win_w, win_h)/200);
  nsegs = MAX(nsegs, 6);
  nsegs = MAX(nsegs, 40);
  if (nsegs % 2)
     nsegs += 1;
  sangle = sang;
  angle = eang;
  ony = onz = 0;
  z1 = cos_table[sangle]*outer+z; y1 = sin_table[sangle] * outer+y;
  Z1 = cos_table[sangle] * inner+z; Y1 = sin_table[sangle]*inner+y ; 
  Z2 = z;
  Y2 = y;
  xl = x + length;
  if (inner < outer && endcaps < 3) tube = 1;
  step = 360/nsegs;

  glBegin(GL_QUADS);
  for (a = sangle ; a <= angle || b <= angle ; a+= step) {
    y2=outer*(float)sin_table[a]+y;
    z2=outer*(float)cos_table[a]+z;
    y3=outer*(float)sin_table[a+step]+y;
    z3=outer*(float)cos_table[a+step]+z;
    if (endcaps)
       y2c[a] = y2; z2c[a] = z2; /* cache for later */
    if (tube) {
      Y2=inner*(float)sin_table[a]+y;
      Z2=inner*(float)cos_table[a]+z;
      Y3=inner*(float)sin_table[a+step]+y;
      Z3=inner*(float)cos_table[a+step]+z;
    }
    glNormal3f(0, y1, z1);
    glVertex3f(x,y1,z1);
    glVertex3f(xl,y1,z1);
    glNormal3f(0, y2, z2);
    glVertex3f(xl,y2,z2);
    glVertex3f(x,y2,z2);
    if (a == sangle && angle - sangle < 360) {
      if (tube)
        glVertex3f(x, Y1, Z1);
      else
        glVertex3f(x, y, z);
      glVertex3f(x, y1, z1);
      glVertex3f(xl, y1, z1);
      if (tube)
        glVertex3f(xl, Z1, Z1);
      else
        glVertex3f(xl, y, z);
    }
    if (tube) {
      if (endcaps != 1) {
        glNormal3f(-1, 0, 0); /* left end */
        glVertex3f(x, y1, z1);
        glVertex3f(x, y2, z2);
        glVertex3f(x, Y2, Z2);
        glVertex3f(x, Y1, Z1);
      }

      glNormal3f(0, -Y1, -Z1); /* inner surface */
      glVertex3f(x, Y1, Z1);
      glVertex3f(xl, Y1, Z1);
      glNormal3f(0, -Y2, -Z2);
      glVertex3f(xl, Y2, Z2);
      glVertex3f(x, Y2, Z2);

      if (endcaps != 2) {
        glNormal3f(1, 0, 0); /* right end */
        glVertex3f(xl, y1, z1);
        glVertex3f(xl, y2, z2);
        glVertex3f(xl, Y2, Z2);
        glVertex3f(xl, Y1, Z1);
      }
    }

    z1=z2; y1=y2;
    Z1=Z2; Y1=Y2;
    b = a;
  }
  glEnd();

  if (angle - sangle < 360) {
    GLfloat nx, ny, nz;
    GLfloat v1[3], v2[3], v3[3];
    v1[0] = x; v1[1] = y; v1[2] = z;
    v2[0] = x; v2[1] = y1; v2[2] = z1;
    v3[0] = xl; v3[1] = y1; v3[2] = z1;
    normal(&v2[0], &v1[0], &v3[0], &nx, &ny, &nz);
    glBegin(GL_QUADS);
    glNormal3f(nx, ny, nz);
    glVertex3f(x, y, z);
    glVertex3f(x, y1, z1);
    glVertex3f(xl, y1, z1);
    glVertex3f(xl, y, z);
    glEnd();
  }
  if (endcaps) {
    GLfloat end, start;
    if (tube) {
      if (endcaps == 1) {
        end = 0;
        start = 0;
      } else if (endcaps == 2) {
        start = end = length+0.01;
      } else {
        end = length+0.02; start = -0.01;
      }
      norm = (ex == length+0.01) ? -1 : 1;
    } else  {
      end = length;
      start = 0;
      norm = -1;
    }

    for(ex = start ; ex <= end ; ex += length) {
      z1 = outer*cos_table[sangle]+z;
      y1 = y+sin_table[sangle]*outer;
      step = 360/nsegs;
      glBegin(GL_TRIANGLES);
      b = 0;
      for (a = sangle ; a <= angle || b <= angle; a+= step) {
          glNormal3f(norm, 0, 0);
          glVertex3f(x+ex,y, z);
          glVertex3f(x+ex,y1,z1);
          glVertex3f(x+ex,y2c[a],z2c[a]);
        y1 = y2c[a]; z1 = z2c[a];
        b = a;
      }
      if (!tube) norm = 1;
      glEnd();
    }
  }
  glPopMatrix();
}

GLvoid normal(GLfloat v1[], GLfloat v2[], GLfloat v3[], 
                  GLfloat *nx, GLfloat *ny, GLfloat *nz)
{
   GLfloat x, y, z, X, Y, Z;

   x = v2[0]-v1[0];
   y = v2[1]-v1[1];
   z = v2[2]-v1[2];
   X = v3[0]-v1[0];
   Y = v3[1]-v1[1];
   Z = v3[2]-v1[2];

   *nx = Y*z - Z*y;
   *ny = Z*x - X*z;
   *nz = X*y - Y*x;

} 



void circle(float radius, int segments, int half) {
float x1 = 0, x2 = 0;
float y1 = 0, y2 = 0;
int i, t, s;

  if (half) {
    t = 270; s = 90;
    x1 = radius, y1 = 0;
  } else {
    t = 360, s = 0;
  }
  glBegin(GL_TRIANGLES);
  glNormal3f(1, 0, 0);
  for(i=s;i<=t;i+=10)
  {
    float angle=i;
    x2=radius*(float)cos_table[(int)angle];
    y2=radius*(float)sin_table[(int)angle];
    glVertex3f(0,0,0);
    glVertex3f(x1,y1,0);
    glVertex3f(x2,y2,0);
    x1=x2;
    y1=y2;
  }
  glEnd();
}

void ring(GLfloat inner, GLfloat outer, int nsegs) {
GLfloat z1, z2, y1, y2;
GLfloat Z1, Z2, Y1, Y2;
int i;

  z1 = inner; y1 = 0;
  Z1 = outer; Y1 = 0;
  glBegin(GL_QUADS);
  glNormal3f(1, 0, 0);
  for(i=0; i <=360 ; i+= 360/nsegs)
  {
    float angle=i;
    z2=inner*(float)sin_table[(int)angle];
    y2=inner*(float)cos_table[(int)angle];
    Z2=outer*(float)sin_table[(int)angle];
    Y2=outer*(float)cos_table[(int)angle];
    glVertex3f(0, Y1, Z1);
    glVertex3f(0, y1, z1);
    glVertex3f(0, y2, z2);
    glVertex3f(0, Y2, Z2);
    z1=z2;
    y1=y2;
    Z1=Z2;
    Y1=Y2;
  }
  glEnd();
}

void Rect(GLfloat x, GLfloat y, GLfloat z, GLfloat w, GLfloat h,
            GLfloat t) {
GLfloat yh;
GLfloat xw;
GLfloat zt;

  yh = y+h; xw = x+w; zt = z - t;

  glBegin(GL_QUADS); /* front */
    glNormal3f(0, 0, 1);
    glVertex3f(x, y, z);
    glVertex3f(x, yh, z);
    glVertex3f(xw, yh, z);
    glVertex3f(xw, y, z);
  /* back */
    glNormal3f(0, 0, -1);
    glVertex3f(x, y, zt);
    glVertex3f(x, yh, zt);
    glVertex3f(xw, yh, zt);
    glVertex3f(xw, y, zt);
  /* top */
    glNormal3f(0, 1, 0);
    glVertex3f(x, yh, z);
    glVertex3f(x, yh, zt);
    glVertex3f(xw, yh, zt);
    glVertex3f(xw, yh, z);
  /* bottom */
    glNormal3f(0, -1, 0);
    glVertex3f(x, y, z);
    glVertex3f(x, y, zt);
    glVertex3f(xw, y, zt);
    glVertex3f(xw, y, z);
  /* left */
    glNormal3f(-1, 0, 0);
    glVertex3f(x, y, z);
    glVertex3f(x, y, zt);
    glVertex3f(x, yh, zt);
    glVertex3f(x, yh, z);
  /* right */
    glNormal3f(1, 0, 0);
    glVertex3f(xw, y, z);
    glVertex3f(xw, y, zt);
    glVertex3f(xw, yh, zt);
    glVertex3f(xw, yh, z);
  glEnd();
}

void makepiston(void) {
GLfloat colour[] = {0.6, 0.6, 0.6, 1.0};
int i;
  
  i = glGenLists(1);
  glNewList(i, GL_COMPILE);
  glRotatef(90, 0, 0, 1);
  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, colour);
  glMaterialfv(GL_FRONT, GL_SPECULAR, colour);
  glMateriali(GL_FRONT, GL_SHININESS, 20);
  cylinder(0, 0, 0, 2, 1, 0.7, 2, 0, 360); /* body */
  colour[0] = colour[1] = colour[2] = 0.2;
  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, colour);
  cylinder(1.6, 0, 0, 0.1, 1.05, 1.05, 0, 0, 360); /* ring */
  cylinder(1.8, 0, 0, 0.1, 1.05, 1.05, 0, 0, 360); /* ring */
  glEndList();
}

void CrankBit(GLfloat x, GLfloat y, GLfloat z) {
  Rect(x, y, z, 0.2, 1.8, 1);
}

void boom(GLfloat x, GLfloat y, int s) {
static GLfloat red[] = {0, 0, 0, 0.9};
static GLfloat lpos[] = {0, 0, 0, 1};
static GLfloat d = 0, wd;
static int time = 0;

  if (time == 0 && s) {
    red[0] = red[1] = 0;
    d = 0.05;
    time++;
    glEnable(GL_LIGHT1); 
  } else if (time == 0 && !s) {
    return;
  } else if (time >= 8 && time < 16 && !s) {
    time++;
    red[0] -= 0.2; red[1] -= 0.1;
    d-= 0.04;
  } else if (time >= 16) {
    time = 0;
    glDisable(GL_LIGHT1);
    return;
  } else {
    red[0] += 0.2; red[1] += 0.1;
    d+= 0.04;
    time++;
  }
  lpos[0] = x; lpos[1] = y-d;
  glLightfv(GL_LIGHT1, GL_POSITION, lpos);
  glLightfv(GL_LIGHT1, GL_DIFFUSE, red);
  glLightfv(GL_LIGHT1, GL_SPECULAR, red);
  glLighti(GL_LIGHT1, GL_LINEAR_ATTENUATION, 1.3);
  glLighti(GL_LIGHT1, GL_CONSTANT_ATTENUATION, 0);

  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
  glPushMatrix();
  glRotatef(90, 0, 0, 1);
  wd = d*3;
  if (wd > 0.7) wd = 0.7;
  glEnable(GL_BLEND);
  glDepthMask(GL_FALSE);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  cylinder(y-d, -x, 0, d, wd, wd, 1, 0, 360);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glPopMatrix();
}

void display(Engine *e) {

static int a = 0;
GLfloat zb, yb;
static GLfloat ln[730], yp[730], ang[730];
static int ln_init = 0;
static int spark;

  glEnable(GL_LIGHTING);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();
  gluLookAt(viewer[0], viewer[1], viewer[2], lookat[0], lookat[1], lookat[2], 0.0, 1.0, 0.0);
  glPushMatrix();
  glLightfv(GL_LIGHT0, GL_POSITION, lightpos);
  glLightfv(GL_LIGHT0, GL_SPECULAR, light_sp);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, light_sp);

  if (move) {
/* calculate position for the whole object */
    e->x = sin(e->an1)*15;
    e->an1 += e->dx; 
    if (e->an1 >= 2*M_PI) e->an1 -= 2*M_PI;

    e->y = sin(e->an2)*15;
    e->an2 += e->dy; 
    if (e->an2 >= 2*M_PI) e->an2 -= 2*M_PI;

    e->z = sin(e->an3)*10-10;
    e->an3 += e->dz; 
    if (e->an3 >= 2*M_PI) e->an3 -= 2*M_PI;
    glTranslatef(e->x, e->y, e->z);
  }

  if (spin) glRotatef(e->a, e->nx, e->ny, e->nz); 
  glTranslatef(-5, 0, 0); 
  if (spin) e->a += e->da;
  if (spin && (e->a > 360 || e->a < -360)) {
     e->a -= (e->a > 0) ? 360 : -360;
     if ((random() % 5) == 4) {
        e->da = (float)(random() % 1000);
        e->da = e->da/125 - 4;
     }
     if ((random() % 5) == 4) {
        e->nx = (float)(random() % 100) / 100;
        e->ny = (float)(random() % 100) / 100;
        e->nz = (float)(random() % 100) / 100;
     }
  }
/* crankshaft */
  glPushMatrix();
  glRotatef(a, 1, 0, 0);
  glCallList(1);
  glPopMatrix();

  /* init the ln[] matrix for speed */
  if (ln_init == 0) {
    for (ln_init = 0 ; ln_init < 730 ; ln_init++) {
      zb = sin_table[ln_init];
      yb = cos_table[ln_init];
      yp[ln_init] = yb + sqrt(25 - (zb*zb)); /* y ordinate of piston */
      ln[ln_init] = sqrt(zb*zb + (yb-yp[ln_init])*(yb-yp[ln_init])); /* length of rod */
      ang[ln_init] = asin(zb/5)*57; /* angle of connecting rod */
      ang[ln_init] *= -1;
    }
  }

  zb = sin_table[a];
  yb = cos_table[a];

/* pistons */
  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, white);
  glPushMatrix();
  glTranslatef(0, yp[a]-0.3, 0);
  glCallList(2);
  glPopMatrix();
  glPushMatrix();
  glTranslatef(3.2, yp[(a > 180 ? a-180 : a+180)]-0.3, 0);
  glCallList(2);
  glPopMatrix();
  glPushMatrix();
  glTranslatef(6.5, yp[a]-0.3, 0);
  glCallList(2);
  glPopMatrix();
  glPushMatrix();
  glTranslatef(9.8, yp[(a > 180 ? a-180 : a+180)]-0.3, 0);
  glCallList(2);
  glPopMatrix();

/* spark plugs */
  glPushMatrix();
  glRotatef(90, 0, 0, 1);
  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
  cylinder(8.5, 0, 0, 0.5, 0.4, 0.3, 1, 0, 360);
  cylinder(8.5, -3.2, 0, 0.5, 0.4, 0.3, 1, 0, 360);
  cylinder(8.5, -6.5, 0, 0.5, 0.4, 0.3, 1, 0, 360);
  cylinder(8.5, -9.8, 0, 0.5, 0.4, 0.3, 1, 0, 360);

  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, white);
  cylinder(8, 0, 0, 0.5, 0.2, 0.2, 1, 0, 360);
  cylinder(8, -3.2, 0, 0.5, 0.2, 0.2, 1, 0, 360);
  cylinder(8, -6.5, 0, 0.5, 0.2, 0.2, 1, 0, 360);
  cylinder(8, -9.8, 0, 0.5, 0.2, 0.2, 1, 0, 360);

  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, white);
  cylinder(9, 0, 0, 1, 0.15, 0.15, 1, 0, 360);
  cylinder(9, -3.2, 0, 1, 0.15, 0.15, 1, 0, 360);
  cylinder(9, -6.5, 0, 1, 0.15, 0.15, 1, 0, 360);
  cylinder(9, -9.8, 0, 1, 0.15, 0.15, 1, 0, 360);

  glPopMatrix();

  if (a == 0) spark = 1 - spark;

  if (spark == 0) {
    if (a == 0)
      boom(0, 8, 1);
    else if (a == 180) 
      boom(3.2, 8, 1);
    if (a < 180)
      boom(0, 8, 0);
    else if (a < 360)
      boom(3.2, 8, 0);
  } else {
    if (a == 0)
      boom(6.5, 8, 1);
    else if (a == 180) 
      boom(9.8, 8, 1);
    if (a < 180)
      boom(6.5, 8, 0);
    else if (a < 360)
      boom(9.8, 8, 0);
  }



  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue);
  glPushMatrix();
  cylinder(-0.8, yb, zb, 1.6, 0.3, 0.3, 1, 0, 365);
  cylinder(5.7, yb, zb, 1.7, 0.3, 0.3, 1, 0, 365);

  cylinder(2.4, -yb, -zb, 1.7, 0.3, 0.3, 1, 0, 365);
  cylinder(9.0, -yb, -zb, 1.7, 0.3, 0.3, 1, 0, 365);
  glPopMatrix();

 /* rod */
  glPushMatrix();
  glRotatef(90, 0, 0, 1);
  glRotatef(ang[a], 0, -1, 0);
  cylinder(yb, 0, zb, ln[a], 0.2, 0.2, 0, 0, 365);
  cylinder(yb, -6.4, zb, ln[a], 0.2, 0.2, 0, 0, 365);
  glPopMatrix();

  glPushMatrix();
  glRotatef(90, 0, 0, 1);
  glRotatef(ang[a+180], 0, -1, 0);
  cylinder(-yb, -3.2, -zb, ln[a], 0.2, 0.2, 0, 0, 365);
  cylinder(-yb, -9.7, -zb, ln[a], 0.2, 0.2, 0, 0, 365);
  glPopMatrix();


  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, yellow_t);
  glEnable(GL_BLEND);
  glDepthMask(GL_FALSE);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  Rect(-1, 8.3, 1, 12, 0.2, 2);
  Rect(-1.2, -0.5, 1, 0.2, 9, 2);

  Rect(1.4, 3, 1, 0.6, 5.3, 2);
  Rect(4.4, 3, 1, 1, 5.3, 2);
  Rect(7.7, 3, 1, 0.8, 5.3, 2);
  Rect(10.8, -0.5, 1, 0.2, 8.8, 2);

  Rect(-1, 2.8, 1, 12, 0.2, 0.2); 
  Rect(-1, 2.8, -1, 12, 0.2, 0.2); 

  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);


  a+=10; if (a >= 360) a = 0;
  glPopMatrix();
  glFlush();
}

void makeshaft (void) {
int i;
  i = glGenLists(1);
  glNewList(i, GL_COMPILE);
  cylinder(10.4, 0, 0, 2, 0.3, 0.3, 1, 0, 365);
  cylinder(7.1, 0, 0, 2, 0.3, 0.3, 0, 0, 365);
  cylinder(3.8, 0, 0, 2, 0.3, 0.3, 0, 0, 365);
  cylinder(0.5, 0, 0, 2, 0.3, 0.3, 0, 0, 365);
  cylinder(-1.5, 0, 0, 1, 0.3, 0.3, 1, 0, 365);

  cylinder(12.4, 0, 0, 1, 3, 2.5, 0, 0, 365);
  Rect(12.4, -0.3, 2.8, 0.5, 0.6, 5.6);
  Rect(12.4, -2.8, 0.3, 0.5, 5.6, 0.6);

  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, green);
  CrankBit(0.5, -0.4, 0.5);
  cylinder(0.5, 0.5, 0, 0.2, 2, 2, 1, 240, 300);
  CrankBit(-0.7, -0.4, 0.5);
  cylinder(-0.7, 0.5, 0, 0.2, 2, 2, 1, 240, 300);

  CrankBit(2.5, -1.4, 0.5);
  cylinder(2.5, -0.5, 0, 0.2, 2, 2, 1, 60, 120);
  CrankBit(3.8, -1.4, 0.5);
  cylinder(3.8, -0.5, 0, 0.2, 2, 2, 1, 60, 120);

  CrankBit(5.8, -0.4, 0.5);
  cylinder(5.8, 0.5, 0, 0.2, 2, 2, 1, 240, 300);
  CrankBit(7.1, -0.4, 0.5);
  cylinder(7.1, 0.5, 0, 0.2, 2, 2, 1, 240, 300);

  CrankBit(9.1, -1.4, 0.5);
  cylinder(9.1, -0.5, 0, 0.2, 2, 2, 1, 60, 120);
  CrankBit(10.4, -1.4, 0.5);
  cylinder(10.4, -0.5, 0, 0.2, 2, 2, 1, 60, 120);

  glEndList();

}

void reshape_engine(ModeInfo *mi, int width, int height)
{

 glViewport(0,0,(GLint)width, (GLint) height);
 glMatrixMode(GL_PROJECTION);
 glLoadIdentity();
 glFrustum(-1.0,1.0,-1.0,1.0,1.5,70.0);
 glMatrixMode(GL_MODELVIEW);
 win_h = height; win_w = width;
}


void init_engine(ModeInfo *mi)
{
int screen = MI_SCREEN(mi);
Engine *e;

 if (engine == NULL) {
   if ((engine = (Engine *) calloc(MI_NUM_SCREENS(mi),
                                        sizeof(Engine))) == NULL)
          return;
 }
 e = &engine[screen];
 e->window = MI_WINDOW(mi);

 e->x = e->y = e->z = e->a = e->an1 = e->nx = e->ny = e->nz = 
 e->dx = e->dy = e->dz = e->da = 0;

 if (move) {
   e->dx = (float)(random() % 1000)/30000;
   e->dy = (float)(random() % 1000)/30000;
   e->dz = (float)(random() % 1000)/30000;
 } else {
  viewer[0] = 0; viewer[1] = 6; viewer[2] = 15;
  lookat[0] = 0; lookat[1] = 4; lookat[2] = 0; 
 }
 if (spin) {
   e->da = (float)(random() % 1000)/125 - 4;
   e->nx = (float)(random() % 100) / 100;
   e->ny = (float)(random() % 100) / 100;
   e->nz = (float)(random() % 100) / 100;
 }

 if ((e->glx_context = init_GL(mi)) != NULL) {
      reshape_engine(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
 } else {
     MI_CLEARWINDOW(mi);
 }
 glClearColor(0.0,0.0,0.0,0.0);
 glShadeModel(GL_SMOOTH);
 glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
 glEnable(GL_DEPTH_TEST);
 glEnable(GL_LIGHTING);
 glEnable(GL_LIGHT0);
 glEnable(GL_NORMALIZE);
 make_tables();
 makeshaft();
 makepiston();
}

void draw_engine(ModeInfo *mi) {
Engine *e = &engine[MI_SCREEN(mi)];
Window w = MI_WINDOW(mi);
Display *disp = MI_DISPLAY(mi);

  if (!e->glx_context)
      return;

  glXMakeCurrent(disp, w, *(e->glx_context));

  display(e);

  if(mi->fps_p) do_fps(mi);
  glFinish(); 
  glXSwapBuffers(disp, w);
}

void release_engine(ModeInfo *mi) {

  if (engine != NULL) {
   (void) free((void *) engine);
   engine = NULL;
  }
  FreeAllGL(MI);
}

#endif
