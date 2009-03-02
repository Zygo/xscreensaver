/*
 * circuit - Random electronic components floating around
 *
 * version 1.3
 *
 * Since version 1.1: added to-220 transistor, added fuse
 * Since version 1.2: random display digits, LED improvements (flickering)
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

/* Written over a few days in a (successful) bid to learn GL coding 
 *
 * -seven option is dedicated to all the Slarkeners
 *
 * try "-rotate -rotate-speed 0"
 *
 * This hack uses lookup tables for sin, cos and tan - it can do a lot
 */


#include <X11/Intrinsic.h>

#ifdef STANDALONE
# define PROGCLASS                                      "Circuit"
# define HACK_INIT                                      init_circuit
# define HACK_DRAW                                      draw_circuit
# define HACK_RESHAPE                           reshape_circuit
# define circuit_opts                                     xlockmore_opts
/* insert defaults here */

#define DEF_SPIN        "True"
#define DEF_SEVEN       "False"
#define DEF_PARTS       "10"


#define DEFAULTS        "*parts:      " DEF_PARTS " \n" \
                        "*spin:       " DEF_SPIN   "\n" \
                        "*delay:       20000       \n" \
                        "*showFPS:       False       \n" \
                        "*seven:      " DEF_SEVEN  "\n" \
                        "*light:      True  \n" \
                        "*rotate:      False\n" \
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


static int maxparts;
static int spin;
static int seven;
static int rotate;
static int rotatespeed;
static int uselight;
int def_parts = 10;

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

static XrmOptionDescRec opts[] = {
  {"-parts", ".circuit.parts", XrmoptionSepArg, "10" },
  {"-rotate-speed", ".circuit.rotatespeed", XrmoptionSepArg, "1" },
  {"+spin", ".circuit.spin", XrmoptionNoArg, (caddr_t) "false" },
  {"-spin", ".circuit.spin", XrmoptionNoArg, (caddr_t) "true" },
  {"+light", ".circuit.light", XrmoptionNoArg, (caddr_t) "false" },
  {"-light", ".circuit.light", XrmoptionNoArg, (caddr_t) "true" },
  {"+seven", ".circuit.seven", XrmoptionNoArg, (caddr_t) "false" },
  {"-seven", ".circuit.seven", XrmoptionNoArg, (caddr_t) "true" },
  {"+rotate", ".circuit.rotate", XrmoptionNoArg, (caddr_t) "false" },
  {"-rotate", ".circuit.rotate", XrmoptionNoArg, (caddr_t) "true" },
};

static argtype vars[] = {
  {(caddr_t *) &maxparts, "parts", "Parts", DEF_PARTS, t_Int},
  {(caddr_t *) &rotatespeed, "rotatespeed", "Rotatespeed", "1", t_Int},
  {(caddr_t *) &spin, "spin", "Spin", DEF_SPIN, t_Bool},
  {(caddr_t *) &rotate, "rotate", "Rotate", "False", t_Bool},
  {(caddr_t *) &uselight, "light", "Light", "True", t_Bool},
  {(caddr_t *) &seven, "seven", "Seven", DEF_SEVEN, t_Bool},
};

ModeSpecOpt circuit_opts = {countof(opts), opts, countof(vars), vars, NULL};

#ifdef USE_MODULES
ModStruct   circuit_description =
{"circuit", "init_circuit", "draw_circuit", "release_circuit",
 "draw_circuit", "init_circuit", NULL, &circuit_opts,
 1000, 1, 2, 1, 4, 1.0, "",
 "Flying electronic components", 0, NULL};

#endif


typedef struct {
  GLXContext *glx_context;
  Window window;
} Circuit;

static Circuit *circuit = NULL;

#include <math.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265
#endif

/* window width, height */
int win_w, win_h;

/* width and height of viewport */

#define XMAX 30
#define YMAX 30

#define MAX_COMPONENTS 30

#define MOVE_MULT 0.05

static float f_rand(void) {
   return ((float)RAND(10000)/(float)10000);
}

#define RAND_RANGE(min, max) ((min) + (max - min) * f_rand())

/* one lucky led gets to be a light source , unless -no-light*/
int light = 0;
int lighton = 0;

static GLfloat viewer[] = {0.0, 0.0, 14.0};
static GLfloat lightpos[] = {7.0, 7.0, 15, 1.0};

float sin_table[720];
float cos_table[720];
float tan_table[720];


/* Represents a band on a resistor/diode/etc */
typedef struct {
  float pos;     /* relative position from start/previous band */
  GLfloat r, g, b; /* colour of the band */
  float len;     /* length as a fraction of total length */
} Band;

typedef struct {
  Band *b1, *b2, *b3, *b4; /* bands */
  int b[4];
} Resistor;

typedef struct {
  Band *band;
  GLfloat r, g, b; /* body colour */
} Diode;

typedef struct {
  int type; /* package type. 0 = to-92, 1 = to-220 */
} Transistor;

typedef struct {
  GLfloat r,g,b; /* LED colour */
  int light; /* are we the light source? */
} LED;

typedef struct {
  int type; /* 0 = electro, 1 = ceramic */
  float width; /* width of an electro/ceramic */
  float length; /* length of an electro */
} Capacitor;

typedef struct {
  int type; /* 0 = DIL, 1 = flat square */
  int pins; 
} IC;

/* 7 segment display */

typedef struct {
  int value; /* displayed number */
} Disp;

typedef struct {
  GLfloat l, w;
} Fuse;

typedef struct {
  GLfloat x, y, z; /* current co-ordinates */
  GLfloat dx, dy, dz; /* current direction */
  GLfloat rotx, roty, rotz; /* rotation vector */
  GLfloat drot; /* rotation velocity (degrees per frame) */
  int norm; /* Normalize this component (for shine) */
  int rdeg; /* current rotation degrees */
  int angle; /* angle about the z axis */
  int alpha; /* 0 if not a transparent component */
  int type;  /* 0 = resistor, 1 = diode, 2 = transistor, 3 = LED, 4 = cap, 5=IC,
                6 = 7 seg disp */
  void * c; /* pointer to the component */
} Component;

static int band_list[12];
  
/* standard colour codes */

static GLfloat colorcodes [12][3] = {
  {0.0,0.0,0.0},    /* black  0 */
  {0.49,0.25,0.08}, /* brown  1 */
  {1.0,0.0,0.0},    /* red    2 */
  {1.0,0.5,0.0},    /* orange 3 */
  {1.0,1.0,0.0},    /* yellow 4 */
  {0.0,1.0,0.0},    /* green  5 */
  {0.0,0.5,1.0},    /* blue   6 */
  {0.7,0.2,1.0},    /* violet 7 */
  {0.5,0.5,0.5},    /* grey   8 */
  {1.0,1.0,1.0},    /* white  9 */
  {0.66,0.56,0.2},    /* gold  10 */
  {0.8,0.8,0.8},    /* silver 11 */
};

/* base values for components - we can multiply by 0 - 1M */
static int values [9][2] = {
  {1,0},
  {2,2},
  {3,3},
  {4,7},
  {5,6},
  {6,8},
  {7,5},
  {8,2},
  {9,1}
};

void DrawResistor(Resistor *);
void DrawDiode(Diode *);
void DrawTransistor(Transistor *);
void DrawLED(LED *);
void DrawIC(IC *);
void DrawCapacitor(Capacitor *);
void DrawDisp(Disp *);
void DrawFuse(Fuse *);

void reorder(Component *[]);
void circle(float, int,int);
void bandedCylinder(float, float , GLfloat, GLfloat , GLfloat,  Band **, int);
Resistor *NewResistor(void);
Diode *NewDiode(void);
Transistor *NewTransistor(void);
LED * NewLED(void);
Capacitor *NewCapacitor(void);
IC* NewIC(void);
Disp* NewDisp(void);
Fuse *NewFuse(void);

/* we use trig tables to speed things up - 200 calls to sin()
 in one frame can be a bit harsh..
*/

void make_tables(void) {
int i;
float f;

  f = 360 / (M_PI * 2);
  for (i = 0 ; i < 720 ; i++) {
    sin_table[i] = sin(i/f);
  }
  for (i = 0 ; i < 720 ; i++) {
    cos_table[i] = cos(i/f);
  }
  for (i = 0 ; i < 720 ; i++) {
    tan_table[i] = tan(i/f);
  }
}


void createCylinder (float length, float radius, int endcaps, int half) {
int a; /* current angle around cylinder */
int angle, norm;
float z1, y1, z2, y2, ex;
int nsegs;

  glPushMatrix();
  nsegs = radius*MAX(win_w, win_h)/10;
  nsegs = MAX(nsegs, 6);
  if (nsegs % 2)
     nsegs += 1;
  angle = (half) ? (180 - 90/nsegs) : 374;
  z1 = radius; y1 = 0;
  glBegin(GL_QUADS);
  for (a = 0 ; a <= angle ; a+= angle/nsegs) {
    y2=radius*(float)sin_table[(int)a];
    z2=radius*(float)cos_table[(int)a];
      glNormal3f(0, y1, z1);
      glVertex3f(0,y1,z1);
      glVertex3f(length,y1,z1);
      glVertex3f(length,y2,z2);
      glVertex3f(0,y2,z2);
    z1=z2;
    y1=y2;
  }
  glEnd();
  if (half) {
    glBegin(GL_POLYGON);
      glNormal3f(0, 1, 0);
      glVertex3f(0, 0, radius);
      glVertex3f(length, 0, radius);
      glVertex3f(length, 0, 0 - radius);
      glVertex3f(0, 0, 0 - radius);
    glEnd();
  }
  if (endcaps) {
    for(ex = 0 ; ex <= length ; ex += length) {
      z1 = radius; y1 = 0;
      norm = (ex == length) ? 1 : -1;
      for (a = 0 ; a <= angle ; a+= angle/nsegs) {
        y2=radius*(float)sin_table[(int)a];
        z2=radius*(float)cos_table[(int)a];
        glBegin(GL_TRIANGLES);
          glNormal3f(norm, 0, 0);
          glVertex3f(ex,0, 0);
          glVertex3f(ex,y1,z1);
          glVertex3f(ex,y2,z2);
        glEnd();
        z1=z2;
        y1=y2;
      }
    }
  }
  glPopMatrix();
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
    glVertex3f(0,y1,x1);
    glVertex3f(0,y2,x2);
    x1=x2;
    y1=y2;
  }
  glEnd();
}

void wire(float len) {
static GLfloat col[] = {0.3, 0.3, 0.3, 1.0};
static GLfloat spec[] = {0.9, 0.9, 0.9, 1.0};
static GLfloat nospec[] = {0.4, 0.4, 0.4, 1.0};
GLfloat shin = 30;
int n;

  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, col);
  glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
  glMaterialfv(GL_FRONT, GL_SHININESS, &shin);
  n = glIsEnabled(GL_NORMALIZE);
  if (!n) glEnable(GL_NORMALIZE);
  createCylinder(len, 0.05, 1, 0);
  if (!n) glDisable(GL_NORMALIZE);
  glMaterialfv(GL_FRONT, GL_SPECULAR, nospec);
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

void sphere(GLfloat r, float stacks, float slices,
             int startstack, int endstack, int startslice,
             int endslice) {
GLfloat d, d1, dr, dr1, Dr, Dr1, D, D1, z1, z2, y1, y2, Y1, Z1, Y2, Z2;
int a, a1, b, b1, c, c1;
GLfloat step, sstep;

      step = 180/stacks;
      sstep = 360/slices;
      a1 = startstack * step;
      b1 = startslice * sstep;
      y1 = z1 = Y1 = Z1 = 0;
      c = (endslice / slices) * 360;
      c1 = (endstack/stacks)*180;
      glBegin(GL_QUADS);
      for (a = startstack * step ; a <= c1 ; a+= step) {
        d=sin_table[a];
        d1=sin_table[a1];
        D=cos_table[a];
        D1=cos_table[a1];
        dr = d * r;
        dr1 = d1 * r;
        Dr = D * r;
        Dr1 = D1 * r;
        for (b = b1 ; b <= c ; b+= sstep) {
          y2=dr*sin_table[b];
          z2=dr*cos_table[b];
          Y2=dr1*sin_table[b];
          Z2=dr1*cos_table[b];
            glNormal3f(D, y1, z1);
            glVertex3f(Dr,y1,z1);
            glVertex3f(Dr,y2,z2);
            glVertex3f(Dr1,Y2,Z2);
            glVertex3f(Dr1,Y1,Z1);
          z1=z2;
          y1=y2;
          Z1=Z2;
          Y1=Y2;
        }
        a1 = a;
     }
     glEnd();
}

int DrawComponent(Component *c) {
int ret = 0; /* return 1 if component is freed */

   glPushMatrix();
   glTranslatef(c->x, c->y, c->z);
     if (c->angle > 0) {
        glRotatef(c->angle, 0, 0, 1);
     }
   if (spin) {
     glRotatef(c->rdeg, c->rotx, c->roty, c->rotz);
     c->rdeg += c->drot;
   }

   if (c->norm)
     glEnable(GL_NORMALIZE);
   else
     glDisable(GL_NORMALIZE);

 /* call object draw routine here */
   if (c->type == 0) {
     DrawResistor(c->c);
   } else if (c->type == 1) {
     DrawDiode(c->c);
   } else if (c->type == 2) {
     DrawTransistor(c->c);
   } else if (c->type == 3) {
     if (((LED *)c->c)->light && light) {
       GLfloat lp[] = {0.1, 0, 0, 1};
       glEnable(GL_LIGHT1);
       glLightfv(GL_LIGHT1, GL_POSITION, lp);
     }
     DrawLED(c->c);
   } else if (c->type == 4) {
     DrawCapacitor(c->c);
   } else if (c->type == 5) {
     DrawIC(c->c);
   } else if (c->type == 6) {
     DrawDisp(c->c);
   } else if (c->type == 7) {
     DrawFuse(c->c);
   }
   c->x += c->dx * MOVE_MULT;
   c->y += c->dy * MOVE_MULT;
   if (c->x > XMAX/2 || c->x < 0 - XMAX/2 ||
       c->y > YMAX/2 || c->y < 0 - YMAX/2) {
        if (c->type == 3 && ((LED *)c->c)->light && light) {
          glDisable(GL_LIGHT1);
          light = 0; lighton = 0;
        }
        free(c->c);
        ret = 1;
   }

   glPopMatrix();
   glDisable(GL_NORMALIZE);
   return ret;
}
/* draw a resistor */

void DrawResistor(Resistor *r) {
int i;
GLfloat col[] = {0.74, 0.62, 0.46, 1.0};
GLfloat spec[] = {0.8, 0.8, 0.8, 1.0};
GLfloat shine = 30;

   glTranslatef(-4, 0, 0);
   wire(3);
   glTranslatef(3, 0, 0);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, col);
   glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
   glMaterialfv(GL_FRONT, GL_SHININESS, &shine);
   createCylinder(1.8, 0.4, 1, 0);
   glPushMatrix();
   for (i = 0 ; i < 4 ; i++) {
     glTranslatef(0.35, 0, 0);
     glCallList(band_list[r->b[i]]);
   }
   glPopMatrix();
   glTranslatef(1.8, 0, 0);
   wire(3);
}

void DrawFuse(Fuse *f) {
static GLfloat col[] = {0.5, 0.5, 0.5, 1.0}; /* endcaps */
static GLfloat glass[] = {0.4, 0.4, 0.4, 0.3}; /* glass */
static GLfloat spec[] = {1, 1, 1, 1}; /* glass */

   glPushMatrix();
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, col);
   glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
   glMateriali(GL_FRONT, GL_SHININESS, 40);
   createCylinder(0.8, 0.45, 1, 0);
   glTranslatef(0.8, 0, 0);
   glEnable(GL_BLEND);
   glDepthMask(GL_FALSE);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, glass);
   glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, 40);
   createCylinder(2, 0.4, 0, 0);
   createCylinder(2, 0.3, 0, 0);
   glDisable(GL_BLEND);
   glDepthMask(GL_TRUE);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, col);
   glMateriali(GL_FRONT, GL_SHININESS, 40);
   glBegin(GL_LINES);
   glVertex3f(0, 0, 0);
   glVertex3f(2, 0. ,0);
   glEnd();
   glTranslatef(2, 0, 0);
   createCylinder(0.8, 0.45, 1, 0);
   glPopMatrix();
}


void DrawCapacitor(Capacitor *c) {
static GLfloat col[] = {0, 0, 0, 0};
static GLfloat spec[] = {0.8, 0.8, 0.8, 0};
GLfloat brown[] = {0.84, 0.5, 0};
static GLfloat shine = 40;

  glPushMatrix();
  if (c->type) {
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, brown);
    sphere(c->width, 20, 20, 0, 5 ,0, 20);
    glTranslatef(1.45*c->width, 0, 0);
    sphere(c->width, 20, 20, 15, 20, 0, 20);
    glRotatef(90, 0, 0, 1);
    glTranslatef(0, 0.7*c->width, 0.3*c->width);
    wire(3*c->width);
    glTranslatef(0, 0, -0.6*c->width);
    wire(3*c->width);
  } else {
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, col);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, &shine);
    glBegin(GL_POLYGON);
     glVertex3f(0, 0.82*c->width, -0.1);
     glVertex3f(3*c->length, 0.82*c->width, -0.1);
     glVertex3f(3*c->length, 0.82*c->width, 0.1);
     glVertex3f(0, 0.82*c->width, 0.1);
    glEnd();
    col[0] = 0.7;
    col[1] = 0.7;
    col[2] = 0.7;
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, col);
    circle(0.6*c->width, 30, 0);
    col[0] = 0.0;
    col[1] = 0.2;
    col[2] = 0.9;
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, col);
    ring(0.6*c->width, 0.8*c->width, 30);
    glTranslatef(0.01, 0.0, 0);
    createCylinder(3.0*c->length, 0.8*c->width, 1, 0);
    col[0] = 0;
    col[1] = 0;
    col[2] = 0;
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, col);
    glTranslatef(3.01*c->length, 0.0, 0);
    circle(0.6*c->width, 30, 0);
    glTranslatef(0, 0.4*c->width, 0);
    wire(3*c->length);
    glTranslatef(0.0, -0.8*c->width, 0);
    wire(3.3*c->length);
  }
  glPopMatrix();
}

void DrawLED(LED *l) {
GLfloat col[] = {0, 0, 0, 0.6};

  col[0] = l->r; col[1] = l->g; col[2] = l->b;
  if (l->light && light) {
    GLfloat dir[] = {-1, 0, 0};
    glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, dir);
    if (!lighton) {
      glLightfv(GL_LIGHT1, GL_SPECULAR, col);
      glLightfv(GL_LIGHT1, GL_AMBIENT, col);
      glLightfv(GL_LIGHT1, GL_DIFFUSE, col);
      glLighti(GL_LIGHT1, GL_SPOT_CUTOFF, (GLint) 90);
      glLighti(GL_LIGHT1, GL_CONSTANT_ATTENUATION, (GLfloat)1); 
      glLighti(GL_LIGHT1, GL_LINEAR_ATTENUATION, (GLfloat)0); 
      glLighti(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, (GLfloat)0); 
      glLighti(GL_LIGHT1, GL_SPOT_EXPONENT, (GLint) 20);
      lighton = 1;
    }
  }
  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, col);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, col);
  /* no transparency when LED is lit */
  if (!l->light) { 
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);
  }
  createCylinder(1.2, 0.3, 0, 0);
  if (l->light && light) {
    glDisable(GL_LIGHTING);
    glColor3fv(col);
  }
  sphere(0.3, 10, 10, 5, 10, 0, 10);
  if (l->light && light) {
    glEnable(GL_LIGHTING);
  } else {
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
  }

  glTranslatef(1.2, 0, 0);
  createCylinder(0.1, 0.38, 1, 0);
  glTranslatef(-0.3, 0.15, 0);
  wire(3);
  glTranslatef(0, -0.3, 0);
  wire(3.3);
  if (random() % 50 == 25) {
    if (l->light) {
      l->light = 0; light = 0; lighton = 0;
      glDisable(GL_LIGHT1);
    } else if (!light) {
      l->light = 1; light = 1;
    }
  }
}
   


void DrawDiode(Diode *d) {
GLfloat shine = 40;
GLfloat col[] = {0.3, 0.3, 0.3, 0};
GLfloat spec[] = {0.7, 0.7, 0.7, 0};

   glPushMatrix();
   glMaterialfv(GL_FRONT, GL_SHININESS, &shine);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, col);
   glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
   glTranslatef(-4, 0, 0);
   wire(3);
   glTranslatef(3, 0, 0);
   bandedCylinder(0.3, 1.5, d->r, d->g, d->b, &(d->band), 1);
   glTranslatef(1.5, 0, 0);
   wire(3);
   glPopMatrix();
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

/* IC pins */

void ICLeg(GLfloat x, GLfloat y, GLfloat z, int dir) {


  if (dir) {
    Rect(x-0.1, y, z, 0.1, 0.1, 0.02);
    Rect(x-0.1, y, z, 0.02, 0.1, 0.1);
    Rect(x-0.1, y+0.03, z-0.1, 0.02, 0.05, 0.3);
  } else {
    Rect(x, y, z, 0.1, 0.1, 0.02);
    Rect(x+0.8*0.1, y, z, 0.02, 0.1, 0.1);
    Rect(x+0.8*0.1, y+0.03, z-0.1, 0.02, 0.05, 0.3);
  }

}


void DrawIC(IC *c) {
GLfloat w, h, d;
int z;
GLfloat col[] = {0.1, 0.1, 0.1, 0};
GLfloat spec[] = {0.6, 0.6, 0.6, 0};
GLfloat shine = 40;
GLfloat lspec[] = {0.6, 0.6, 0.6, 0};
GLfloat lcol[] = {0.4, 0.4, 0.4, 0};
GLfloat lshine = 40;

  glPushMatrix();
  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, col);
  glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
  glMaterialfv(GL_FRONT, GL_SHININESS, &shine);
  switch(c->pins) {
    case 8:
      w = 1.0; h = 1.5;
      break;
    case 14:
      w = 1.0; h = 3;
      break;
    case 16:
      w = 1.0; h = 3;
      break;
    case 24:
    default:
      w = 1.5; h = 3.5;
      break;
  }
  w = w/2; h = h/2;
    glBegin(GL_QUADS);
      glNormal3f(0, 0, 1);
      glVertex3f(w, h, 0.1);
      glVertex3f(w, -h, 0.1);
      glVertex3f(-w, -h, 0.1);
      glVertex3f(-w, h, 0.1);
      glNormal3f(0, 0, -1);
      glVertex3f(w, h, -0.1);
      glVertex3f(w, -h, -0.1);
      glVertex3f(-w, -h, -0.1);
      glVertex3f(-w, h, -0.1);
      glNormal3f(1, 0, 0);
      glVertex3f(w, h, -0.1);
      glVertex3f(w, -h, -0.1);
      glVertex3f(w, -h, 0.1);
      glVertex3f(w, h, 0.1);
      glNormal3f(0, -1, 0);
      glVertex3f(w, -h, -0.1);
      glVertex3f(w, -h, 0.1);
      glVertex3f(-w, -h, 0.1);
      glVertex3f(-w, -h, -0.1);
      glNormal3f(-1, 0, 0);
      glVertex3f(-w, h, -0.1);
      glVertex3f(-w, h, 0.1);
      glVertex3f(-w, -h, 0.1);
      glVertex3f(-w, -h, -0.1);
      glNormal3f(0, -1, 0);
      glVertex3f(-w, h, -0.1);
      glVertex3f(w, h, -0.1);
      glVertex3f(w, h, 0.1);
      glVertex3f(-w, h, 0.1);
    glEnd();
    d = (h*2-0.1) / c->pins;
    d*=2;
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, lcol);
    glMaterialfv(GL_FRONT, GL_SPECULAR, lspec);
    glMaterialfv(GL_FRONT, GL_SHININESS, &lshine);
    for (z = 0 ; z < c->pins/2 ; z++) {
      ICLeg(w, -h + z*d + d/2, 0, 0);
    }
    for (z = 0 ; z < c->pins/2 ; z++) {
      ICLeg(-w, -h + z*d + d/2, 0, 1);
    }
    glPopMatrix();
}

void DrawDisp(Disp *d) {
GLfloat col[] = {0.8, 0.8, 0.8, 1.0}; /* body colour */
GLfloat front[] = {0.2, 0.2, 0.2, 1.0}; /* front colour */
GLfloat on[] = {0.9, 0, 0, 1}; /* 'on' segment */
GLfloat off[] = {0.3, 0, 0, 1}; /* 'off' segment */
int i, j, k;
GLfloat x, y; /* for the pins */
GLfloat spec[] = {0.6, 0.6, 0.6, 0};
GLfloat lcol[] = {0.4, 0.4, 0.4, 0};
GLfloat shine = 40;
static GLfloat vdata_h[6][2] = 
{
  {0, 0},
  {0.1, 0.1},
  {0.9, 0.1},
  {1, 0},
  {0.9, -0.1},
  {0.1, -0.1}
};
static GLfloat vdata_v[6][2] =
{
  {0.27, 0},
  {0.35, -0.1},
  {0.2, -0.9},
  {0.1, -1},
  {0, -0.9},
  {0.15, -0.15}
};

static GLfloat seg_start[7][2] = 
{

  {0.55, 2.26},
  {1.35, 2.26},
  {1.2, 1.27},
  {0.25, 0.25},
  {0.06, 1.25},
  {0.25, 2.25},
  {0.39, 1.24}
};

static int nums[10][7] =
{
  {1, 1, 1, 1, 1, 1, 0}, /* 0 */
  {0, 1, 1, 0, 0, 0, 0}, /* 1 */
  {1, 1, 0, 1, 1, 0, 1}, /* 2 */
  {1, 1, 1, 1, 0, 0, 1}, /* 3 */
  {0, 1, 1, 0, 0, 1, 1}, /* 4 */
  {1, 0, 1, 1, 0, 1, 1}, /* 5 */
  {1, 0, 1, 1, 1, 1, 1}, /* 6 */
  {1, 1, 1, 0, 0, 0, 0}, /* 7 */
  {1, 1, 1, 1, 1, 1, 1}, /* 8 */
  {1, 1, 1, 0, 0, 1, 1}  /* 9 */
};
  
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, col);
   Rect(0, 0, -0.01, 1.8, 2.6, 0.7);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, front);
   glBegin(GL_QUADS);
     glVertex2f(-0.05, -0.05);
     glVertex2f(-0.05, 2.65);
     glVertex2f(1.85, 2.65);
     glVertex2f(1.85, -0.05);
   glEnd();
   glDisable(GL_LIGHTING); /* lit segments dont need light */
   if (!seven && (random() % 30) == 19) { /* randomly change value */
     d->value = random() % 10;
   }
   for (j = 0 ; j < 7 ; j++) { /* draw the segments */
     GLfloat xx[6], yy[6];
     if (nums[d->value][j])
       glColor3fv(on);
     else
       glColor3fv(off);
     for (k = 0 ; k < 6 ; k++) {
       if (j == 0 || j == 3 || j == 6) {
         xx[k] = seg_start[j][0] + vdata_h[k][0];
         yy[k] = seg_start[j][1] + vdata_h[k][1];
       } else {
         xx[k] = seg_start[j][0] + vdata_v[k][0];
         yy[k] = seg_start[j][1] + vdata_v[k][1];
       }
     }
     glBegin(GL_POLYGON);
     for(i = 0 ; i < 6 ; i++) {
       glVertex3f(xx[i], yy[i], 0.01);
     }
     glEnd();
   }
   glColor3fv(on);
   glPointSize(4);
   glBegin(GL_POINTS);
    glVertex3f(1.5, 0.2, 0.01);
   glEnd();
   glEnable(GL_LIGHTING);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, lcol);
   glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
   glMaterialfv(GL_FRONT, GL_SHININESS, &shine);
   for (x = 0.35 ; x <= 1.5 ; x+= 1.15) {
     for ( y = 0.2 ; y <= 2.4 ; y += 0.3) {
       ICLeg(x, y, -0.7, 1);
     }
   }
}

void HoledRectangle(GLfloat w, GLfloat h, GLfloat d, GLfloat radius, int p) {
int step, a;
GLfloat x1, y1, x2, y2;
GLfloat yr, yr1, xr, xr1, side, side1;
GLfloat nx, ny;

  step = 360 / p;
  x1 = radius; y1 = 0;
  xr1 = w/2; yr1 = 0;
  side = w/2;
  side1 = h/2;
  glBegin(GL_QUADS);
  for (a = 0 ; a <= 360 ; a+= step) {
    y2=radius*(float)sin_table[(int)a];
    x2=radius*(float)cos_table[(int)a];

    if (a < 45 || a > 315) {
      xr = side;
      yr = side1 * tan_table[a];
      nx = 1; ny = 0;
    } else if (a <= 135 || a >= 225) {
      xr = side/tan_table[a];
      if (a >= 225) {
       yr = -side1;
       xr = 0 - xr;
       nx = 0; ny = -1;
      } else {
       yr = side1;
       nx = 0; ny = 1;
     }
    } else {
      xr = -side;
      yr = -side1 * tan_table[a];
      nx = -1; ny = 0;
    }

      glNormal3f(-x1, -y1, 0); /* cylinder */
      glVertex3f(x1,y1,0);
      glVertex3f(x1,y1,-d);
      glVertex3f(x2,y2,-d);
      glVertex3f(x2,y2,0);

      glNormal3f(0, 0, 1); /* front face */
      glVertex3f(x1,y1,0);
      glVertex3f(xr1, yr1, 0);
      glVertex3f(xr, yr, 0);
      glVertex3f(x2, y2, 0);

      glNormal3f(nx, ny, 0); /* side */
      glVertex3f(xr, yr, 0);
      glVertex3f(xr, yr, -d);
      glVertex3f(xr1, yr1, -d);
      glVertex3f(xr1, yr1, 0);

      glNormal3f(0, 0, -1); /* back */
      glVertex3f(xr, yr, -d);
      glVertex3f(x2, y2, -d);
      glVertex3f(x1, y1, -d);
      glVertex3f(xr1, yr1, -d);

    x1=x2;
    y1=y2;
    xr1 = xr; yr1 = yr;
  }
  glEnd();
}

void DrawTransistor(Transistor *t) {
static GLfloat col[] = {0.3, 0.3, 0.3, 1.0};
static GLfloat spec[] = {0.9, 0.9, 0.9, 1.0};
static GLfloat nospec[] = {0.4, 0.4, 0.4, 1.0};
GLfloat shin = 30;

  glPushMatrix();
  glMaterialfv(GL_FRONT, GL_SHININESS, &shin);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, col);
  if (t->type == 1) {
    glRotatef(90, 0, 1, 0);
    glRotatef(90, 0, 0, 1);
    createCylinder(1.0, 0.4, 1, 1);
    Rect(0, -0.2, 0.4, 1, 0.2, 0.8);
    glTranslatef(-2, 0, -0.2);
    wire(2);
    glTranslatef(0, 0, 0.2);
    wire(2);
    glTranslatef(0, 0, 0.2);
    wire(2);
  } else {
    Rect(0, 0, 0, 1.5, 1.5, 0.75);
    glTranslatef(0.75, 1.875, -0.5);
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, col);
    glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
    glMaterialfv(GL_FRONT, GL_SHININESS, &shin);
    if (glIsEnabled(GL_NORMALIZE)) glEnable(GL_NORMALIZE);
    HoledRectangle(1.5, 0.75, 0.25, 0.2, 8);
    glMaterialfv(GL_FRONT, GL_SPECULAR, nospec);
    glTranslatef(-0.375, -1.875, 0);
    glRotatef(90, 0, 0, -1);
    wire(2);
    glTranslatef(0, 0.375, 0);
    wire(2);
    glTranslatef(0, 0.375, 0);
    wire(2);
  }
  glPopMatrix();
}

Component * NewComponent(void) {
Component *c;
float rnd;

  c = malloc(sizeof(Component));
  c->angle = RAND_RANGE(0,360);
  rnd = f_rand();
  if (rnd < 0.25) { /* come from the top */
     c->y = YMAX/2;
     c->x = RAND_RANGE(0, XMAX) - XMAX/2;
     if (c->x > 0)
       c->dx = 0 - RAND_RANGE(0.5, 2);
     else 
       c->dx = RAND_RANGE(0.5, 2);
     c->dy = 0 - RAND_RANGE(0.5, 2);
  } else if (rnd < 0.5) { /* come from the bottom */
     c->y = 0 - YMAX/2;
     c->x = RAND_RANGE(0, XMAX) - XMAX/2;
     if (c->x > 0)
       c->dx = 0 - RAND_RANGE(0.5, 2);
     else 
       c->dx = RAND_RANGE(0.5, 2);
     c->dy = RAND_RANGE(0.5, 2);
  } else if (rnd < 0.75) { /* come from the left */
     c->x = 0 - XMAX/2;
     c->y = RAND_RANGE(0, YMAX) - YMAX/2;
     c->dx = RAND_RANGE(0.5, 2);
     if (c->y > 0)
       c->dy = 0 - RAND_RANGE(0.5, 2);
     else 
       c->dy = RAND_RANGE(0.5, 2);
  } else { /* come from the right */
     c->x = XMAX/2;
     c->y = RAND_RANGE(0, YMAX) - YMAX/2;
     c->dx =  0 - RAND_RANGE(0.5, 2);
     if (c->y > 0)
       c->dy = 0 - RAND_RANGE(0.5, 2);
     else 
       c->dy = RAND_RANGE(0.5, 2);
  }
  c->z = RAND_RANGE(0, 7) - 9;
  c->rotx = f_rand();
  c->roty = f_rand();
  c->rotz = f_rand();
  c->drot = f_rand() * 7;
  c->rdeg = 0;
  c->dz = f_rand()*2 - 1;
  c->norm = 0;
  c->alpha = 0; /* explicitly set to 1 later */
  rnd = f_rand();
  if (rnd < 0.1) {
    c->c = NewResistor();
    c->type = 0;
    if (f_rand() < 0.4)
      c->norm = 1; /* some resistors shine */
  } else if (rnd < 0.2) {
    c->c = NewDiode();
    if (f_rand() < 0.4)
      c->norm = 1; /* some diodes shine */
    c->type = 1;
  } else if (rnd < 0.3) {
    c->c = NewTransistor();
    c->norm = 1;
    c->type = 2;
  } else if (rnd < 0.4) {
    c->c = NewCapacitor();
    if (f_rand() < 0.4)
      c->norm = 1; /* some capacitors shine */
    c->type = 4;
  } else if (rnd < 0.6) {
    c->c = NewIC();
    c->type = 5;
  } else if (rnd < 0.7) {
    c->c = NewLED();
    c->type = 3;
    c->norm = 1;
    c->alpha = 1;
  } else if (rnd < 0.8) {
    c->c = NewFuse();
    c->norm = 1;
    c->type = 7;
    c->alpha = 1;
  } else {
    c->c = NewDisp();
    c->type = 6;
  }
  return c;
}

Transistor *NewTransistor(void) {
Transistor *t;

  t = malloc(sizeof(Transistor));
  t->type = (f_rand() < 0.5);
  return t;
}

Capacitor *NewCapacitor(void) {
Capacitor *c;

  c = malloc(sizeof(Capacitor));
  c->type = (f_rand() < 0.5);
  if (!c->type) {
    c->length = RAND_RANGE(0.5, 1);
    c->width = RAND_RANGE(0.5, 1);
  } else {
    c->width = RAND_RANGE(1, 2);
  }
  return c;
}

/* 7 segment display */

Disp *NewDisp(void) {
Disp *d;

  d = malloc(sizeof(Disp));
  if (seven)
    d->value = 7;
  else
    d->value = RAND_RANGE(0, 10);
  return d;
}


IC *NewIC(void) {
IC *c;
int pins;

  c = malloc(sizeof(IC));
  c->type = 0;
  switch((int)RAND_RANGE(0,4)) {
    case 0:
      pins = 8;
      break;
    case 1:
      pins = 14;
      break;
    case 2:
      pins = 16;
      break;
    case 3:
    default:
      pins = 24;
      break;
  }
  c->pins = pins;
  return c;
}

LED *NewLED(void) {
LED *l;
float r;

  l = malloc(sizeof(LED));
  r = f_rand();
  l->light = 0;
  if (!light && (f_rand() < 0.4)) {
     light = 1;
     l->light = 1;
  }
  if (r < 0.2) {
    l->r = 0.9; l->g = 0; l->b = 0;
  } else if (r < 0.4) {
    l->r = 0.3; l->g = 0.9; l->b = 0;
  } else if (r < 0.6) {
    l->r = 0.8; l->g = 0.9; l->b = 0;
  } else if (r < 0.8) {
    l->r = 0.0; l->g = 0.2; l->b = 0.8;
  } else {
    l->r = 0.9, l->g = 0.55, l->b = 0;
  }
  return l;
}

Fuse *NewFuse(void) {
Fuse *f;

  f = malloc(sizeof(Fuse));
  return f;
}

Diode *NewDiode(void) {
Band *b;
Diode *ret;

  ret = malloc(sizeof(Diode));
  b = malloc(sizeof(Band));
  b->pos = 0.8;
  b->len = 0.1;
  if (f_rand() < 0.5) {
    b->r = 1;
    b->g = 1;
    b->b = 1;
    ret->r = 0.7; ret->g = 0.1 ; ret->b = 0.1;
  } else {
    b->r = 1;
    b->g = 1;
    b->b = 1;
    ret->r = 0.2; ret->g = 0.2 ; ret->b = 0.2;
  }
  ret->band = b;
  return ret;
}


Resistor  * NewResistor(void) {
int v, m, t; /* value, multiplier, tolerance */
Resistor *ret;

  v = RAND(9);
  m = RAND(5);
  t = (RAND(10) < 5) ? 10 : 11; 
  ret = malloc(sizeof(Resistor));

  if (seven) {
    ret->b[0] = ret->b[1] = ret->b[2] = 7;
  } else {
    ret->b[0] = values[v][0];
    ret->b[1] = values[v][1];
    ret->b[2] = m;
  }
  ret->b[3] = t;

  return ret;
}

void makebandlist(void) {
int i;
GLfloat col[] = {0,0,0,0};
GLfloat spec[] = {0.8,0.8,0.8,0};
GLfloat shine = 40;

   for (i = 0 ; i < 12 ; i++) {
     band_list[i] = glGenLists(i);
     glNewList(band_list[i], GL_COMPILE);
     col[0] = colorcodes[i][0];
     col[1] = colorcodes[i][1];
     col[2] = colorcodes[i][2];
     glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, col);
     glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
     glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, &shine);
     createCylinder(0.1, 0.42, 0, 0);
     glEndList();
  }
}
  

void bandedCylinder(float radius, float l, GLfloat r, GLfloat g, GLfloat bl, 
                        Band **b, int nbands) {
int n; /* band number */
int p = 0; /* prev number + 1; */
GLfloat col[] = {0,0,0,0};

   col[0] = r; col[1] = g; col[2] = bl;
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, col);
   createCylinder(l, radius, 1, 0); /* body */
   for (n = 0 ; n < nbands ; n++) {
     glPushMatrix();
     glTranslatef(b[n]->pos*l, 0, 0);
     col[0] = b[n]->r; col[1] = b[n]->g; col[2] = b[n]->b;
     glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, col);
     createCylinder(b[n]->len*l, radius*1.05, 0, 0); /* band */
     glPopMatrix();
     p = n+1;
   }
}

void drawgrid(void) {
GLfloat x, y;
static GLfloat col[] = {0, 0.25, 0.05};
static GLfloat col2[] = {0, 0.125, 0.05};
GLfloat col3[] = {0, 0.8, 0};
static GLfloat sx, sy; /* bright spot co-ords */
static int sdir; /* 0 = left-right, 1 = right-left, 2 = up->dn, 3 = dn->up */
static int s = 0; /* if spot is enabled */
static float ds; /* speed of spot */

  if (!s) {
     if (f_rand() < ((rotate) ? 0.05 : 0.01)) {
       sdir = RAND_RANGE(0, 4);
       ds = RAND_RANGE(0.4, 0.8);
       switch (sdir) {
          case 0:
           sx = -XMAX/2;
           sy = ((int)RAND_RANGE(0, YMAX/2))*2 - YMAX/2;
           break;
          case 1:
           sx = XMAX/2;
           sy = ((int)RAND_RANGE(0, YMAX/2))*2 - YMAX/2;
           break;
          case 2:
           sy = YMAX/2;
           sx = ((int)RAND_RANGE(0, XMAX/2))*2 - XMAX/2;
           break;
          case 3:
           sy = -YMAX/2;
           sx = ((int)RAND_RANGE(0, XMAX/2))*2 - XMAX/2;
           break;
        }
        s = 1;
      }
  } else if (!rotate) {
    if (col[1] < 0.25) {
      col[1] += 0.025; col[2] += 0.005;
      col2[1] += 0.015 ; col2[2] += 0.005;
    }
  }

  glDisable(GL_LIGHTING);
  if (s) {
    glColor3fv(col3);
    glPushMatrix();
    glTranslatef(sx, sy, -10);
    sphere(0.1, 10, 10, 0, 10, 0, 10);
    if (sdir == 0) 
      glTranslatef(-ds, 0, 0);
    if (sdir == 1) 
      glTranslatef(ds, 0, 0);
    if (sdir == 2) 
      glTranslatef(0, ds, 0);
    if (sdir == 3) 
      glTranslatef(0, -ds, 0);
    sphere(0.05, 10, 10, 0, 10, 0, 10);
    glPopMatrix();
    if (sdir == 0) {
       sx += ds;
       if (sx > XMAX/2)
         s = 0;
    }
    if (sdir == 1) {
       sx -= ds;
       if (sx < -XMAX/2)
         s = 0;
    }
    if (sdir == 2) {
       sy -= ds;
       if (sy < YMAX/2)
         s = 0;
    }
    if (sdir == 3) {
       sy += ds;
       if (sy > YMAX/2)
         s = 0;
    }
  } else if (!rotate) {
    if (col[1] > 0) {
      col[1] -= 0.0025; col[2] -= 0.0005;
      col2[1] -= 0.0015 ; col2[2] -= 0.0005;
    }
  }
  for (x = -XMAX/2 ; x <= XMAX/2 ; x+= 2) {
    glColor3fv(col);
    glBegin(GL_LINES);
     glVertex3f(x, YMAX/2, -10);
     glVertex3f(x, -YMAX/2, -10);
     glColor3fv(col2);
     glVertex3f(x-0.02, YMAX/2, -10);
     glVertex3f(x-0.02, -YMAX/2, -10);
     glVertex3f(x+0.02, YMAX/2, -10);
     glVertex3f(x+0.02, -YMAX/2, -10);
    glEnd();
  }
  for (y = -YMAX/2 ; y <= YMAX/2 ; y+= 2) {
    glColor3fv(col);
    glBegin(GL_LINES);
     glVertex3f(-XMAX/2, y, -10);
     glVertex3f(XMAX/2, y, -10);
     glColor3fv(col2);
     glVertex3f(-XMAX/2, y-0.02, -10);
     glVertex3f(XMAX/2, y-0.02, -10);
     glVertex3f(-XMAX/2, y+0.02, -10);
     glVertex3f(XMAX/2, y+0.02, -10);
    glEnd();
  }
  glEnable(GL_LIGHTING);
}

void display(void) {
static Component *c[MAX_COMPONENTS];
static int i = 0;
GLfloat light_sp[] = {0.8, 0.8, 0.8, 1.0};
GLfloat black[] = {0, 0, 0, 1.0};
static GLfloat rotate_angle = 0; /*  when 'rotate' is enabled */
int j;

  if (i == 0) {
    for (i = 0 ; i < maxparts ; i++) {
      c[i] = NULL;
    }
  } 
  glEnable(GL_LIGHTING);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();
  gluLookAt(viewer[0], viewer[1], viewer[2], 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
  glPushMatrix();
  if (rotate) {
     glRotatef(rotate_angle, 0, 0, 1);
     rotate_angle += 0.01 * (float)rotatespeed;
     if (rotate_angle >= 360) rotate_angle = 0;
  }
  glLightfv(GL_LIGHT0, GL_POSITION, lightpos);
  glLightfv(GL_LIGHT0, GL_SPECULAR, light_sp);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, light_sp);
  glLighti(GL_LIGHT0, GL_CONSTANT_ATTENUATION, (GLfloat)1); 
  glLighti(GL_LIGHT0, GL_LINEAR_ATTENUATION, (GLfloat)0.5); 
  glLighti(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, (GLfloat)0); 
  drawgrid();
  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, light_sp);
  if (f_rand() < 0.05) {
    for (j = 0 ; j < maxparts ; j++) {
      if (c[j] == NULL) {
        c[j] = NewComponent();
        j = maxparts;
      }
    }
    reorder(&c[0]);
  }
  for (j = 0 ; j < maxparts ; j++) {
     glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, black);
     glMaterialfv(GL_FRONT, GL_EMISSION, black);
     glMaterialfv(GL_FRONT, GL_SPECULAR, black);
     if (c[j] != NULL) {
       if (DrawComponent(c[j])) {
        free(c[j]); c[j] = NULL;
       }
     }
  }
  glPopMatrix();
  glFlush();
}

/* ensure transparent components are at the end */
void reorder(Component *c[]) {
int i, j, k;
Component *c1[MAX_COMPONENTS];
Component *c2[MAX_COMPONENTS];

  j = 0;
  for (i = 0 ; i < maxparts ; i++) { /* clear old matrix */
    c1[i] = NULL;
    c2[i] = NULL;
  }
  for (i = 0 ; i < maxparts ; i++) {
    if (c[i] == NULL) continue;
    if (c[i]->alpha) { /* transparent parts go to c1 */
      c1[j] = c[i];
      j++;
    } else { /* opaque parts go to c2 */
      c2[i] = c[i];
    }
  }
  for (i = 0 ; i < maxparts ; i++) { /* clear old matrix */
    c[i] = NULL;
  }
  k = 0;
  for (i = 0 ; i < maxparts ; i++) { /* insert opaque part */
    if (c2[i] != NULL) {
      c[k] = c2[i];
      k++;
    }
  }
  for (i = 0 ; i < j ; i++) { /* insert transparent parts */
    c[k] = c1[i];
    k++;
  }
}

void reshape_circuit(ModeInfo *mi, int width, int height)
{

 glViewport(0,0,(GLint)width, (GLint) height);
 glMatrixMode(GL_PROJECTION);
 glLoadIdentity();
 glFrustum(-1.0,1.0,-1.0,1.0,1.5,35.0);
 glMatrixMode(GL_MODELVIEW);
 win_h = height; win_w = width;
}


void init_circuit(ModeInfo *mi)
{
int screen = MI_SCREEN(mi);
Circuit *c;

 if (circuit == NULL) {
   if ((circuit = (Circuit *) calloc(MI_NUM_SCREENS(mi),
                                        sizeof(Circuit))) == NULL)
          return;
 }
 c = &circuit[screen];
 c->window = MI_WINDOW(mi);


 if ((c->glx_context = init_GL(mi)) != NULL) {
      reshape_circuit(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
 } else {
     MI_CLEARWINDOW(mi);
 }
 if (uselight == 0)
    light = 1;
 glClearColor(0.0,0.0,0.0,0.0);
 glShadeModel(GL_SMOOTH);
 glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
 glEnable(GL_DEPTH_TEST);
 glEnable(GL_LIGHTING);
 glEnable(GL_LIGHT0);
 make_tables();
 makebandlist();
 
}

void draw_circuit(ModeInfo *mi) {
Circuit *c = &circuit[MI_SCREEN(mi)];
Window w = MI_WINDOW(mi);
Display *disp = MI_DISPLAY(mi);

  if (!c->glx_context)
      return;

 glXMakeCurrent(disp, w, *(c->glx_context));

  display();

  if(mi->fps_p) do_fps(mi);
  glFinish(); 
  glXSwapBuffers(disp, w);
}

void release_circuit(ModeInfo *mi) {

  if (circuit != NULL) {
   (void) free((void *) circuit);
   circuit = NULL;
  }
  FreeAllGL(MI);
}

#endif
