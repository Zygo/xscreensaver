/*
 * engine.c - GL representation of a 4 stroke engine
 *
 * version 2.00
 *
 * Copyright (C) 2001 Ben Buxton (bb@cactii.net)
 * modified by Ed Beroset (beroset@mindspring.com)
 *  new to 2.0 version is:
 *  - command line argument to specify number of cylinders
 *  - command line argument to specify included angle of engine
 *  - removed broken command line argument to specify rotation speed
 *  - included crankshaft shapes and firing orders for real engines
 *    verified using the Bosch _Automotive Handbook_, 5th edition, pp 402,403
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
# define HACK_HANDLE_EVENT				engine_handle_event
# define HACK_RESHAPE                                   reshape_engine
# define EVENT_MASK					PointerMotionMask
# define engine_opts                                    xlockmore_opts
/* insert defaults here */

#define DEF_ENGINE "(none)"
#define DEF_TITLES "False"
#define DEF_SPIN   "True"
#define DEF_WANDER "True"

#define DEFAULTS        "*delay:           10000        \n" \
                        "*showFPS:         False        \n" \
                        "*move:            True         \n" \
                        "*spin:            True         \n" \
                        "*engine:        " DEF_ENGINE  "\n" \
			"*titles:        " DEF_TITLES  "\n" \
			"*titleFont:  -*-times-bold-r-normal-*-180-*\n" \

# include "xlockmore.h"              /* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"                  /* from the xlockmore distribution */
#endif /* !STANDALONE */

#include "rotator.h"
#include "gltrackball.h"

/* lifted from lament.c */
#define RAND(n) ((long) ((random() & 0x7fffffff) % ((long) (n))))
#define RANDSIGN() ((random() & 1) ? 1 : -1)


#ifdef USE_GL

#include <GL/glu.h>




#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

static int engineType;
static char *which_engine;
static int move;
static int movepaused = 0;
static int spin;
static Bool do_titles;

static XrmOptionDescRec opts[] = {
  {"-engine",  ".engine.engine", XrmoptionSepArg, DEF_ENGINE },
  {"-move",    ".engine.move",   XrmoptionNoArg, (caddr_t) "True"  },
  {"+move",    ".engine.move",   XrmoptionNoArg, (caddr_t) "False" },
  {"-spin",    ".engine.spin",   XrmoptionNoArg, (caddr_t) "True"  },
  {"+spin",    ".engine.spin",   XrmoptionNoArg, (caddr_t) "False" },
  { "-titles", ".engine.titles", XrmoptionNoArg, (caddr_t) "True"  },
  { "+titles", ".engine.titles", XrmoptionNoArg, (caddr_t) "False" },
};

static argtype vars[] = {
  {(caddr_t *) &which_engine, "engine", "Engine", DEF_ENGINE, t_String},
  {(caddr_t *) &move,         "move",   "Move",   DEF_WANDER, t_Bool},
  {(caddr_t *) &spin,         "spin",   "Spin",   DEF_SPIN,   t_Bool},
  {(caddr_t *) &do_titles,    "titles", "Titles", DEF_TITLES, t_Bool},
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
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;
  XFontStruct *xfont;
  GLuint font_dlist;
  char *engine_name;
} Engine;

static Engine *engine = NULL;

#include <math.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265
#endif

/* these defines are used to provide symbolic means
 * by which to refer to various portions or multiples
 * of a cyle in degrees
 */
#define HALFREV 180
#define ONEREV 360
#define TWOREV 720

#define MOVE_MULT 0.05

#define RAND_RANGE(min, max) ((min) + (max - min) * f_rand())

float crankOffset;
float crankWidth = 1.5;

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

GLvoid normal(GLfloat [], GLfloat [], GLfloat [], 
                  GLfloat *, GLfloat *, GLfloat *);

float sin_table[TWOREV];
float cos_table[TWOREV];
float tan_table[TWOREV];

/* 
 * this table represents both the firing order and included angle of engine.
 * To simplify things, we always number from 0 starting at the flywheel and 
 * moving down the crankshaft toward the back of the engine.  This doesn't
 * always match manufacturer's schemes.  For example, the Porsche 911 engine
 * is a flat six with the following configuration (Porsche's numbering):
 *
 *    3  2  1
 *  |=            firing order is 1-6-2-4-3-5 in this diagram
 *     6  5  4
 *
 * We renumber these using our scheme but preserve the effective firing order:
 *
 *   0  2  4
 * |=             firing order is 4-1-2-5-0-3 in this diagram
 *    1  3  5
 *
 * To avoid going completely insane, we also reorder these so the newly 
 * renumbered cylinder 0 is always first: 0-3-4-1-2-5
 *   
 * For a flat 6, the included angle is 180 degrees (0 would be a inline
 * engine).  Because these are all four-stroke engines, each piston goes
 * through 720 degrees of rotation for each time the spark plug sparks,
 * so in this case, we would use the following angles:
 *
 * cylinder     firing order     angle
 * --------     ------------     -----
 *    0               0             0
 *    1               3           360
 *    2               4           240
 *    3               1           600
 *    4               2           480
 *    5               5           120
 *
 */

typedef struct 
{
    int cylinders;
    int includedAngle;
    int pistonAngle[12];  /* twelve cylinders should suffice... */
    int speed;		/*  step size in degrees for engine speed */
    const char *engineName;  /* currently unused */
} engine_type;

engine_type engines[] = {
    { 3,   0, { 0, 240, 480,   0,   0,   0,
                0,   0,   0,   0,   0,   0 }, 12,
     "Honda Insight" },
    { 4,   0, { 0, 180, 540, 360,   0,   0,
                0,   0,   0,   0,   0,   0 }, 12,
     "BMW M3" },
    { 4, 180, { 0, 360, 180, 540,   0,   0,
                0,   0,   0,   0,   0,   0 }, 12,
     "VW Beetle" },
    { 5,   0, { 0, 576, 144, 432, 288,   0,
                0,   0,   0,   0,   0,   0 }, 12,
     "Audi Quattro" },
    { 6,   0, { 0, 240, 480, 120, 600, 360,
                0,   0,   0,   0,   0,   0 }, 12,
     "BMW M5" },
    { 6,  90, { 0, 360, 480, 120, 240, 600,
                0,   0,   0,   0,   0,   0 }, 12,
     "Subaru XT" },
    { 6, 180, { 0, 360, 240, 600, 480, 120,
                0,   0,   0,   0,   0,   0 }, 12,
     "Porsche 911" },
    { 8,  90, { 0, 450,  90, 180, 270, 360,
              540, 630,   0,   0,   0,   0 }, 15,
     "Corvette Z06" },
    {10,  90, { 0,  72, 432, 504, 288, 360,
              144, 216, 576, 648,   0,   0 }, 12,
     "Dodge Viper" },
    {12,  60, { 0, 300, 240, 540, 480,  60,
              120, 420, 600, 180, 360, 660 }, 12,
     "Jaguar XKE" },
};

/* this define is just a little shorter way of referring to members of the 
 * table above
 */
#define ENG engines[engineType]

/* given a number of cylinders and an included angle, finds matching engine */
int find_engine(const char *name)
{
  int i;

  if (!name || !*name || !strcasecmp (name, "(none)"))
    return (random() % countof(engines));

  for (i = 0; i < countof(engines); i++) {
    if (!strcasecmp(name, engines[i].engineName))
      return i;
  }

  fprintf (stderr, "%s: unknown engine type \"%s\"\n", progname, name);
  fprintf (stderr, "%s: available models are:\n", progname);
  for (i = 0; i < countof(engines); i++) {
    fprintf (stderr, "\t %-13s (%d cylinders",
             engines[i].engineName, engines[i].cylinders);
    if (engines[i].includedAngle == 0)
      fprintf (stderr, ")\n");
    else if (engines[i].includedAngle == 180)
      fprintf (stderr, ", flat)\n");
    else
      fprintf (stderr, ", V)\n");
  }
  exit(1);
}

/* we use trig tables to speed things up - 200 calls to sin()
 in one frame can be a bit harsh..
*/

void make_tables(void)
{
  int i;
  float f;

  f = ONEREV / (M_PI * 2);
  for (i = 0 ; i <= TWOREV ; i++) {
    sin_table[i] = sin(i/f);
  }
  for (i = 0 ; i <= TWOREV ; i++) {
    cos_table[i] = cos(i/f);
  }
  for (i = 0 ; i <= TWOREV ; i++) {
    tan_table[i] = tan(i/f);
  }
}

/* if inner and outer are the same, we draw a cylinder, not a tube */
/* for a tube, endcaps is 0 (none), 1 (left), 2 (right) or 3(both) */
/* angle is how far around the axis to go (up to 360) */

void cylinder (GLfloat x, GLfloat y, GLfloat z, 
    float length, float outer, float inner, int endcaps, int sang, int eang)
{
  int a; /* current angle around cylinder */
  int b = 0; /* previous */
  int angle, norm, step, sangle;
  float z1, y1, z2, y2, ex=0;
  float y3, z3;
  float Z1, Y1, Z2, Y2, xl, Y3, Z3;
  GLfloat y2c[TWOREV], z2c[TWOREV];
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
  step = ONEREV/nsegs;

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
    if (a == sangle && angle - sangle < ONEREV) {
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

  if (angle - sangle < ONEREV) {
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
      step = ONEREV/nsegs;
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

/* this is just a convenience function to make a solid rod */
void rod (GLfloat x, GLfloat y, GLfloat z, float length, float diameter)
{
    cylinder(x, y, z, length, diameter, diameter, 3, 0, ONEREV);
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



void Rect(GLfloat x, GLfloat y, GLfloat z, GLfloat w, GLfloat h,
            GLfloat t)
{
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

void makepiston(void)
{
  GLfloat colour[] = {0.6, 0.6, 0.6, 1.0};
  int i;
  
  i = glGenLists(1);
  glNewList(i, GL_COMPILE);
  glRotatef(90, 0, 0, 1);
  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, colour);
  glMaterialfv(GL_FRONT, GL_SPECULAR, colour);
  glMateriali(GL_FRONT, GL_SHININESS, 20);
  cylinder(0, 0, 0, 2, 1, 0.7, 2, 0, ONEREV); /* body */
  colour[0] = colour[1] = colour[2] = 0.2;
  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, colour);
  cylinder(1.6, 0, 0, 0.1, 1.05, 1.05, 0, 0, ONEREV); /* ring */
  cylinder(1.8, 0, 0, 0.1, 1.05, 1.05, 0, 0, ONEREV); /* ring */
  glEndList();
}

void CrankBit(GLfloat x)
{
  Rect(x, -1.4, 0.5, 0.2, 1.8, 1);
  cylinder(x, -0.5, 0, 0.2, 2, 2, 1, 60, 120);
}

void boom(GLfloat x, GLfloat y, int s)
{
  static GLfloat red[] = {0, 0, 0, 0.9};
  static GLfloat lpos[] = {0, 0, 0, 1};
  static GLfloat d = 0, wd;
  int flameOut = 720/ENG.speed/ENG.cylinders;
  static int time = 0;

  if (time == 0 && s) {
    red[0] = red[1] = 0;
    d = 0.05;
    time++;
    glEnable(GL_LIGHT1); 
  } else if (time == 0 && !s) {
    return;
  } else if (time >= 8 && time < flameOut && !s) {
    time++;
    red[0] -= 0.2; red[1] -= 0.1;
    d-= 0.04;
  } else if (time >= flameOut) {
    time = 0;
    glDisable(GL_LIGHT1);
    return;
  } else {
    red[0] += 0.2; red[1] += 0.1;
    d+= 0.04;
    time++;
  }
  lpos[0] = x-d; lpos[1] = y;
  glLightfv(GL_LIGHT1, GL_POSITION, lpos);
  glLightfv(GL_LIGHT1, GL_DIFFUSE, red);
  glLightfv(GL_LIGHT1, GL_SPECULAR, red);
  glLighti(GL_LIGHT1, GL_LINEAR_ATTENUATION, 1.3);
  glLighti(GL_LIGHT1, GL_CONSTANT_ATTENUATION, 0);

  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
  wd = d*3;
  if (wd > 0.7) wd = 0.7;
  glEnable(GL_BLEND);
  glDepthMask(GL_FALSE);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  rod(x, y, 0, d, wd);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}

void display(Engine *e)
{
  static int a = 0;
  GLfloat zb, yb;
  static GLfloat ln[730], yp[730], ang[730];
  static int ln_init = 0;
  static int lastPlug = 0;
  int half;
  int sides;
  int j, b;
  static float rightSide;

  glEnable(GL_LIGHTING);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();
  gluLookAt(viewer[0], viewer[1], viewer[2], lookat[0], lookat[1], lookat[2], 
	0.0, 1.0, 0.0);
  glPushMatrix();
  glLightfv(GL_LIGHT0, GL_POSITION, lightpos);
  glLightfv(GL_LIGHT0, GL_SPECULAR, light_sp);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, light_sp);

  if (move) {
    double x, y, z;
    get_position (e->rot, &x, &y, &z, !e->button_down_p);
    glTranslatef(x*16-9, y*14-7, z*16-10);
  }
  if (spin) {
    double x, y, z;
    gltrackball_rotate (e->trackball);
    get_rotation(e->rot, &x, &y, &z, !e->button_down_p);
    glRotatef(x*ONEREV, 1.0, 0.0, 0.0);
    glRotatef(y*ONEREV, 0.0, 1.0, 0.0);
    glRotatef(x*ONEREV, 0.0, 0.0, 1.0);
  }

/* So the rotation appears around the centre of the engine */
  glTranslatef(-5, 0, 0); 

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
      /* y ordinate of piston */
      yp[ln_init] = yb + sqrt(25 - (zb*zb)); 
      /* length of rod */
      ln[ln_init] = sqrt(zb*zb + (yb-yp[ln_init])*(yb-yp[ln_init])); 
      /* angle of connecting rod */
      ang[ln_init] = asin(zb/5)*57; 
      ang[ln_init] *= -1;
    }
  }

  glPushMatrix();
  sides = (ENG.includedAngle == 0) ? 1 : 2;
  for (half = 0; half < sides; half++, glRotatef(ENG.includedAngle,1,0,0))
  {
    /* pistons */
      /* glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, white); */
      for (j = 0; j < ENG.cylinders; j += sides)
      {
	b = (a + ENG.pistonAngle[j+half]) % ONEREV;
	glPushMatrix();
	glTranslatef(crankWidth/2 + crankOffset*(j+half), yp[b]-0.3, 0);
	glCallList(2);
	glPopMatrix();
      }
    /* spark plugs */
      glPushMatrix();
      glRotatef(90, 0, 0, 1); 
      glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
      for (j = 0; j < ENG.cylinders; j += sides) 
      {
	cylinder(8.5, -crankWidth/2-crankOffset*(j+half), 0, 
	    0.5, 0.4, 0.3, 1, 0, ONEREV); 
      }
      glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, white);
      for (j = 0; j < ENG.cylinders; j += sides)
      {
	rod(8, -crankWidth/2-crankOffset*(j+half), 0, 0.5, 0.2); 
	rod(9, -crankWidth/2-crankOffset*(j+half), 0, 1, 0.15); 
      }   

     /* rod */
      glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue);
      for (j = 0; j < ENG.cylinders; j += sides)
      {
	  b = (a+HALFREV+ENG.pistonAngle[j+half]) % TWOREV; 
	  glPushMatrix();
	  glRotatef(ang[b], 0, 1, 0);
	  rod(-cos_table[b],
              -crankWidth/2-crankOffset*(j+half),
              -sin_table[b], 
              ln[b], 0.2);
	  glPopMatrix();
      }
      glPopMatrix();

	/* engine block */
      glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, yellow_t);
      glEnable(GL_BLEND);
      glDepthMask(GL_FALSE);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      rightSide = (sides > 1) ? 0 : 1.6;
    /* left plate */
      Rect(-crankWidth/2, -0.5,  1, 0.2, 9, 2);
    /* right plate */
      Rect(0.3+crankOffset*ENG.cylinders-rightSide, -0.5, 1, 0.2, 9, 2);
    /* head plate */
      Rect(-crankWidth/2+0.2, 8.3, 1, 
	    crankWidth/2+0.1+crankOffset*ENG.cylinders-rightSide, 0.2, 2);
    /* front rail */
      Rect(-crankWidth/2+0.2, 3, 1, 
	    crankWidth/2+0.1+crankOffset*ENG.cylinders-rightSide, 0.2, 0.2);
    /* back rail */
      Rect(-crankWidth/2+0.2, 3, -1+0.2, 
	    crankWidth/2+0.1+crankOffset*ENG.cylinders-rightSide, 0.2, 0.2);
    /* plates between cylinders */
      for (j=0; j < ENG.cylinders - (sides == 1); j += sides)
	Rect(0.4+crankWidth+crankOffset*(j-half), 3, 1, 1, 5.3, 2);
      glDepthMask(GL_TRUE);
  }
  glPopMatrix();

	/* see which of our plugs should fire now, if any */
  for (j = 0; j < ENG.cylinders; j++)
  {
    if (0 == ((a + ENG.pistonAngle[j]) % TWOREV))
    {
	glPushMatrix();
	if (j & 1) 
	    glRotatef(ENG.includedAngle,1,0,0);
	glRotatef(90, 0, 0, 1); 
	boom(8, -crankWidth/2-crankOffset*j, 1); 
	lastPlug = j;
	glPopMatrix();
    }
  }

  if (lastPlug != j)
  {
    /* this code causes the last plug explosion to dim gradually */
    if (lastPlug & 1) 
      glRotatef(ENG.includedAngle, 1, 0, 0);
    glRotatef(90, 0, 0, 1); 
    boom(8, -crankWidth/2-crankOffset*lastPlug, 0); 
  }
  glDisable(GL_BLEND);

  a += ENG.speed; 
  if (a >= TWOREV) 
    a = 0;
  glPopMatrix();
  glFlush();
}

void makeshaft (void)
{
  int i;
  int j;
  const static float crankThick = 0.2;
  const static float crankDiam = 0.3;

  i = glGenLists(1);
  glNewList(i, GL_COMPILE);

  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue);
  /* draw the flywheel */
  cylinder(-2.5, 0, 0, 1, 3, 2.5, 0, 0, ONEREV);
  Rect(-2, -0.3, 2.8, 0.5, 0.6, 5.6);
  Rect(-2, -2.8, 0.3, 0.5, 5.6, 0.6);

  /* now make each of the shaft bits between the cranks, 
   * starting from the flywheel end which is at X-coord 0. 
   * the first cranskhaft bit is always 2 units long 
   */
  rod(-2, 0, 0, 2, crankDiam);

  /* Each crank is crankWidth units wide and the total width of a
   * cylinder assembly is 3.3 units. For inline engines, there is just
   * a single crank per cylinder width.  For other engine
   * configurations, there is a crank between each pair of adjacent
   * cylinders on one side of the engine, so the crankOffset length is
   * halved.
   */
  crankOffset = 3.3;
  if (ENG.includedAngle != 0)
    crankOffset /= 2;
  for (j = 0; j < ENG.cylinders - 1; j++)
    rod(crankWidth - crankThick + crankOffset*j, 0, 0, 
	crankOffset - crankWidth + 2 * crankThick, crankDiam);
  /* the last bit connects to the engine wall on the non-flywheel end */
  rod(crankWidth - crankThick + crankOffset*j, 0, 0, 0.9, crankDiam);


  for (j = 0; j < ENG.cylinders; j++)
  {
    glPushMatrix();
    if (j & 1)
        glRotatef(HALFREV+ENG.pistonAngle[j]+ENG.includedAngle,1,0,0);
    else
        glRotatef(HALFREV+ENG.pistonAngle[j],1,0,0);
    /* draw wrist pin */
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue);
    rod(crankOffset*j, -1.0, 0.0, crankWidth, crankDiam);
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, green);
    /* draw right part of crank */
    CrankBit(crankOffset*j); 
    /* draw left part of crank */
    CrankBit(crankWidth-crankThick+crankOffset*j);
    glPopMatrix();
  }
  glEndList();
}

static void
load_font (ModeInfo *mi, char *res, XFontStruct **fontP, GLuint *dlistP)
{
  const char *font = get_string_resource (res, "Font");
  XFontStruct *f;
  Font id;
  int first, last;

  if (!font) font = "-*-times-bold-r-normal-*-180-*";

  f = XLoadQueryFont(mi->dpy, font);
  if (!f) f = XLoadQueryFont(mi->dpy, "fixed");

  id = f->fid;
  first = f->min_char_or_byte2;
  last = f->max_char_or_byte2;
  
  clear_gl_error ();
  *dlistP = glGenLists ((GLuint) last+1);
  check_gl_error ("glGenLists");
  glXUseXFont(id, first, last-first+1, *dlistP + first);
  check_gl_error ("glXUseXFont");

  *fontP = f;
}


static void
print_title_string (ModeInfo *mi, const char *string, GLfloat x, GLfloat y)
{
  Engine *e = &engine[MI_SCREEN(mi)];
  XFontStruct *font = e->xfont;
  GLfloat line_height = font->ascent + font->descent;

  y -= line_height;

  glPushAttrib (GL_TRANSFORM_BIT |  /* for matrix contents */
                GL_ENABLE_BIT);     /* for various glDisable calls */
  glDisable (GL_LIGHTING);
  glDisable (GL_DEPTH_TEST);
  {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    {
      glLoadIdentity();

      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      {
        int i;
        int x2 = x;
        glLoadIdentity();

        gluOrtho2D (0, mi->xgwa.width, 0, mi->xgwa.height);

        glRasterPos2f (x, y);
        for (i = 0; i < strlen(string); i++)
          {
            char c = string[i];
            if (c == '\n')
              {
                glRasterPos2f (x, (y -= line_height));
                x2 = x;
              }
            else
              {
                glCallList (e->font_dlist + (int)(c));
                x2 += (font->per_char
                       ? font->per_char[c - font->min_char_or_byte2].width
                       : font->min_bounds.width);
              }
          }
      }
      glPopMatrix();
    }
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
  }
  glPopAttrib();

  glMatrixMode(GL_MODELVIEW);
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
  viewer[0] = 0; viewer[1] = 2; viewer[2] = 18;
  lookat[0] = 0; lookat[1] = 0; lookat[2] = 0; 

 }
 if (spin) {
   e->da = (float)(random() % 1000)/125 - 4;
   e->nx = (float)(random() % 100) / 100;
   e->ny = (float)(random() % 100) / 100;
   e->nz = (float)(random() % 100) / 100;
 }

 {
   double spin_speed = 1.0;
   double wander_speed = 0.03;

 e->rot = make_rotator (spin ? spin_speed : 0,
                        spin ? spin_speed : 0,
                        spin ? spin_speed : 0,
                        1.0,
                        move ? wander_speed : 0,
                        True);

    e->trackball = gltrackball_init ();
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
 engineType = find_engine(which_engine);

 e->engine_name = malloc(200);
 sprintf (e->engine_name,
          "%s\n%s%d%s",
          engines[engineType].engineName,
          (engines[engineType].includedAngle == 0 ? "" :
           engines[engineType].includedAngle == 180 ? "Flat " : "V"),
          engines[engineType].cylinders,
          (engines[engineType].includedAngle == 0 ? " Cylinder" : "")
          );

 makeshaft();
 makepiston();
 load_font (mi, "titleFont", &e->xfont, &e->font_dlist);
}

Bool engine_handle_event (ModeInfo *mi, XEvent *event)
{
   Engine *e = &engine[MI_SCREEN(mi)];

   if (event->xany.type == ButtonPress &&
       event->xbutton.button & Button1)
   {
       e->button_down_p = True;
       gltrackball_start (e->trackball,
		          event->xbutton.x, event->xbutton.y,
			  MI_WIDTH (mi), MI_HEIGHT (mi));
       movepaused = 1;
       return True;
   }
   else if (event->xany.type == ButtonRelease &&
            event->xbutton.button & Button1) {
       e->button_down_p = False;
       movepaused = 0;
       return True;
   }
   else if (event->xany.type == MotionNotify &&
            e->button_down_p) {
      gltrackball_track (e->trackball,
		         event->xmotion.x, event->xmotion.y,
			 MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
   }
  return False;
}

void draw_engine(ModeInfo *mi)
{
  Engine *e = &engine[MI_SCREEN(mi)];
  Window w = MI_WINDOW(mi);
  Display *disp = MI_DISPLAY(mi);

  if (!e->glx_context)
    return;

  glXMakeCurrent(disp, w, *(e->glx_context));


  display(e);

  if (do_titles)
    print_title_string (mi, e->engine_name,
                        10, mi->xgwa.height - 10);

  if(mi->fps_p) do_fps(mi);
  glFinish(); 
  glXSwapBuffers(disp, w);
}

void release_engine(ModeInfo *mi) 
{
  if (engine != NULL) {
   (void) free((void *) engine);
   engine = NULL;
  }
  FreeAllGL(MI);
}

#endif
