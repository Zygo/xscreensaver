/*
 * circuit - Random electronic components floating around
 *
 * version 1.4
 *
 * Since version 1.1: added to-220 transistor, added fuse
 * Since version 1.2: random display digits, LED improvements (flickering)
 * Since version 1.3: ICs look better, font textures, improved normals to
 *                    eliminate segmenting on curved surfaces, speedups
 * Since version 1.4: Added RCA connector, 3.5mm connector, slide switch,
 *                    surface mount, to-92 markings. Fixed ~5min crash.
 *                    Better LED illumination. Other minor changes.
 *
 * Copyright (C) 2001,2002 Ben Buxton (bb@cactii.net)
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
 * This hack uses lookup tables for sin, cos and tan - it can do a lot
 */

#ifdef STANDALONE
#define DEFAULTS        "*delay:   20000 \n" \
                        "*showFPS: False \n"

# define refresh_circuit 0
# define circuit_handle_event 0
# include "xlockmore.h"                         /* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"                                     /* from the xlockmore distribution */
#endif /* !STANDALONE */

#define DEF_SPIN        "True"
#define DEF_SEVEN       "False"
#define DEF_PARTS       "10"
#define DEF_ROTATESPEED "1"
#define DEF_LIGHT       "True"

/* lifted from lament.c */
#define RAND(n) ((long) ((random() & 0x7fffffff) % ((long) (n))))
#define RANDSIGN() ((random() & 1) ? 1 : -1)


#ifdef USE_GL

#include "font-ximage.h"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

static int maxparts;
static char *font;
static int rotatespeed;
static int spin;
static int uselight;
static int seven;

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

static XrmOptionDescRec opts[] = {
  {"-parts", ".circuit.parts", XrmoptionSepArg, 0 },
  {"-font", ".circuit.font", XrmoptionSepArg, 0 },
  {"-rotate-speed", ".circuit.rotatespeed", XrmoptionSepArg, 0 },
  {"+spin", ".circuit.spin", XrmoptionNoArg, "false" },
  {"-spin", ".circuit.spin", XrmoptionNoArg, "true" },
  {"+light", ".circuit.light", XrmoptionNoArg, "false" },
  {"-light", ".circuit.light", XrmoptionNoArg, "true" },
  {"+seven", ".circuit.seven", XrmoptionNoArg, "false" },
  {"-seven", ".circuit.seven", XrmoptionNoArg, "true" },
};

static argtype vars[] = {
  {&maxparts, "parts", "Parts", DEF_PARTS, t_Int},
  {&font, "font", "Font", "fixed", t_String},
  {&rotatespeed, "rotatespeed", "Rotatespeed", DEF_ROTATESPEED, t_Int},
  {&spin, "spin", "Spin", DEF_SPIN, t_Bool},
  {&uselight, "light", "Light", DEF_LIGHT, t_Bool},
  {&seven, "seven", "Seven", DEF_SEVEN, t_Bool},
};

ENTRYPOINT ModeSpecOpt circuit_opts = {countof(opts), opts, countof(vars), vars, NULL};

#ifdef USE_MODULES
ModStruct   circuit_description =
{"circuit", "init_circuit", "draw_circuit", "release_circuit",
 "draw_circuit", "init_circuit", NULL, &circuit_opts,
 1000, 1, 2, 1, 4, 1.0, "",
 "Flying electronic components", 0, NULL};

#endif

#define MAX_COMPONENTS 400
#define MOVE_MULT 0.02

static float f_rand(void)
{
   return ((float)RAND(10000)/(float)10000);
}

#define RAND_RANGE(min, max) ((min) + (max - min) * f_rand())

/* used for allocating font textures */
typedef struct {
  int num; /* index number */
  int w;   /* width */
  int h;   /* height */
} TexNum;

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

static const char * const transistortypes[] = {
  "TIP2955",
  "TIP32C",
  "LM 350T",
  "IRF730",
  "ULN2577",
  "7805T",
  "7912T",
  "TIP120",
  "2N6401",
  "BD239",
  "2SC1590",
  "MRF485",
  "SC141D"
};

static const char * const to92types[] = {
  "C\n548",
  "C\n848",
  "74\nL05",
  "C\n858",
  "BC\n212L",
  "BC\n640",
  "BC\n337",
  "BC\n338",
  "S817",
  "78\nL12",
  "TL\n431",
  "LM\n35DZ",
};

static const char * const smctypes[] = {
  "1M-",
  "1K",
  "1F",
  "B10",
  "S14",
  "Q3",
  "4A"
};

typedef struct {
  int type; /* package type. 0 = to-92, 1 = to-220 */
  GLfloat tw, th; /* texture dimensions */
  GLuint tnum; /* texture binding */
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

/* 3.5 mm plug */
typedef struct {
  int blah;
} ThreeFive;

/* slide switch */
typedef struct {
  int position;
} Switch;

typedef struct {
  int pins;
  const char *val;
} ICTypes;

static const ICTypes ictypes[] = {
  {8, "NE 555"},
  {8, "LM 386N"},
  {8, "ADC0831"},
  {8, "LM 383T"},
  {8, "TL071"},
  {8, "LM 311"},
  {8, "LM393"},
  {8, "LM 3909"},

  {14, "LM 380N"},
  {14, "NE 556"},
  {14, "TL074"},
  {14, "LM324"},
  {14, "LM339"},
  {14, "MC1488"},
  {14, "MC1489"},
  {14, "LM1877-9"},
  {14, "4011"},
  {14, "4017"},
  {14, "4013"},
  {14, "4024"},
  {14, "4066"},

  {16, "4076"},
  {16, "4049"},
  {16, "4094"},
  {16, "4043"},
  {16, "4510"},
  {16, "4511"},
  {16, "4035"},
  {16, "RS232"},
  {16, "MC1800"},
  {16, "ULN2081"},
  {16, "UDN2953"},

  {24, "ISD1416P"},
  {24, "4515"},
  {24, "TMS6264L"},
  {24, "MC146818"}
};

typedef struct {
  int type; /* 0 = DIL, 1 = flat square */
  int pins; 
  float tw, th; /* texture dimensions for markings */
  int tnum; /* texture number */
} IC;

/* 7 segment display */

typedef struct {
  int value; /* displayed number */
} Disp;

typedef struct {
  GLfloat l, w;
} Fuse;

typedef struct {
  GLfloat l, w;
  int col;
} RCA;

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

/* standard colour codes */

static const GLfloat colorcodes [12][3] = {
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
static const int values [9][2] = {
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

typedef struct {
  GLXContext *glx_context;
  Window window;

  int XMAX, YMAX;
  int win_w, win_h;

  /* one lucky led gets to be a light source , unless -no-light*/
  int light;
  int lighton;

  /* stores refs to textures */
  int s_refs[50];

  GLfloat viewer[3];
  GLfloat lightpos[4];

  float sin_table[720];
  float cos_table[720];
  float tan_table[720];

  Component *components[MAX_COMPONENTS];
  int band_list[12];
  int band_list_polys[12];

  GLfloat grid_col[3], grid_col2[3];

  int display_i;
  GLfloat rotate_angle;

  char *font_strings[50]; /* max of 40 textures */
  int font_w[50], font_h[50];
  int font_init;

  GLfloat draw_sx, draw_sy; /* bright spot co-ords */
  int draw_sdir; /* 0 = left-right, 1 = right-left, 2 = up->dn, 3 = dn->up */
  int draw_s; /* if spot is enabled */
  float draw_ds; /* speed of spot */

} Circuit;

static Circuit *circuit = NULL;


static int DrawResistor(Circuit *, Resistor *);
static int DrawDiode(Circuit *, Diode *);
static int DrawTransistor(Circuit *, Transistor *);
static int DrawLED(Circuit *, LED *);
static int DrawIC(Circuit *, IC *);
static int DrawCapacitor(Circuit *, Capacitor *);
static int DrawDisp(Circuit *, Disp *);
static int DrawFuse(Circuit *, Fuse *);
static int DrawRCA(Circuit *, RCA *);
static int DrawThreeFive(Circuit *, ThreeFive *);
static int DrawSwitch(Circuit *, Switch *);

static void freetexture(Circuit *, GLuint);
static void reorder(Component *[]);
static int circle(Circuit *, float, int,int);
static int bandedCylinder(Circuit *, 
                           float, float , GLfloat, GLfloat , GLfloat,  
                           Band **, int);
static TexNum *fonttexturealloc(ModeInfo *, const char *, float *, float *);
static int Rect(GLfloat , GLfloat , GLfloat, GLfloat , GLfloat ,GLfloat);
static int ICLeg(GLfloat, GLfloat, GLfloat, int);
static int HoledRectangle(Circuit *ci, 
                           GLfloat, GLfloat, GLfloat, GLfloat, int);
static Resistor *NewResistor(void);
static Diode *NewDiode(void);
static Transistor *NewTransistor(ModeInfo *);
static LED * NewLED(Circuit *);
static Capacitor *NewCapacitor(Circuit *);
static IC* NewIC(ModeInfo *);
static Disp* NewDisp(Circuit *);
static Fuse *NewFuse(Circuit *);
static RCA *NewRCA(Circuit *);
static ThreeFive *NewThreeFive(Circuit *);
static Switch *NewSwitch(Circuit *);

/* we use trig tables to speed things up - 200 calls to sin()
 in one frame can be a bit harsh..
*/

static void make_tables(Circuit *ci) 
{
int i;
float f;

  f = 360 / (M_PI * 2);
  for (i = 0 ; i < 720 ; i++) {
    ci->sin_table[i] = sin(i/f);
  }
  for (i = 0 ; i < 720 ; i++) {
    ci->cos_table[i] = cos(i/f);
  }
  for (i = 0 ; i < 720 ; i++) {
    ci->tan_table[i] = tan(i/f);
  }
}


static int createCylinder (Circuit *ci,
                            float length, float radius, int endcaps, int half) 
{
  int polys = 0;
  int a; /* current angle around cylinder */
  int angle, norm;
  float z1, y1, z2, y2,ex;
  int nsegs;

  glPushMatrix();
  nsegs = radius*MAX(ci->win_w, ci->win_h)/20;
  nsegs = MAX(nsegs, 4);
  if (nsegs % 2)
     nsegs += 1;
  angle = (half) ? (180 - 90/nsegs) : 374;
  z1 = radius; y1 = 0;
  glBegin(GL_QUADS);
  for (a = 0 ; a <= angle ; a+= angle/nsegs) {
    y2=radius*(float)ci->sin_table[(int)a];
    z2=radius*(float)ci->cos_table[(int)a];
      glNormal3f(0, y1, z1);
      glVertex3f(0,y1,z1);
      glVertex3f(length,y1,z1);
      glNormal3f(0, y2, z2);
      glVertex3f(length,y2,z2);
      glVertex3f(0,y2,z2);
      polys++;
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
      polys++;
    glEnd();
  }
  if (endcaps) {
    for(ex = 0 ; ex <= length ; ex += length) {
      z1 = radius; y1 = 0;
      norm = (ex == length) ? 1 : -1;
      glBegin(GL_TRIANGLES);
      glNormal3f(norm, 0, 0);
      for (a = 0 ; a <= angle ; a+= angle/nsegs) {
        y2=radius*(float)ci->sin_table[(int)a];
        z2=radius*(float)ci->cos_table[(int)a];
          glVertex3f(ex,0, 0);
          glVertex3f(ex,y1,z1);
          glVertex3f(ex,y2,z2);
          polys++;
        z1=z2;
        y1=y2;
      }
      glEnd();
    }
  }
  glPopMatrix();
  return polys;
}

static int circle(Circuit *ci, float radius, int segments, int half)
{
  int polys = 0;
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
    x2=radius*(float)ci->cos_table[(int)angle];
    y2=radius*(float)ci->sin_table[(int)angle];
    glVertex3f(0,0,0);
    glVertex3f(0,y1,x1);
    glVertex3f(0,y2,x2);
    polys++;
    x1=x2;
    y1=y2;
  }
  glEnd();
  return polys;
}

static int wire(Circuit *ci, float len)
{
  int polys = 0;
  GLfloat col[] = {0.3, 0.3, 0.3, 1.0};
  GLfloat spec[] = {0.9, 0.9, 0.9, 1.0};
  GLfloat nospec[] = {0.4, 0.4, 0.4, 1.0};
  GLfloat shin = 30;
  int n;

  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, col);
  glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
  glMaterialfv(GL_FRONT, GL_SHININESS, &shin);
  n = glIsEnabled(GL_NORMALIZE);
  if (!n) glEnable(GL_NORMALIZE);
  polys += createCylinder(ci, len, 0.05, 1, 0);
  if (!n) glDisable(GL_NORMALIZE);
  glMaterialfv(GL_FRONT, GL_SPECULAR, nospec);
  return polys;
}

#if 0
static int ring(GLfloat inner, GLfloat outer, int nsegs)
{
  int polys = 0;
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
    z2=inner*(float)ci->sin_table[(int)angle];
    y2=inner*(float)ci->cos_table[(int)angle];
    Z2=outer*(float)ci->sin_table[(int)angle];
    Y2=outer*(float)ci->cos_table[(int)angle];
    glVertex3f(0, Y1, Z1);
    glVertex3f(0, y1, z1);
    glVertex3f(0, y2, z2);
    glVertex3f(0, Y2, Z2);
    polys++;
    z1=z2;
    y1=y2;
    Z1=Z2;
    Y1=Y2;
  }
  glEnd();
  return polys;
}
#endif

static int sphere(Circuit *ci, GLfloat r, float stacks, float slices,
             int startstack, int endstack, int startslice,
             int endslice)
{
  int polys = 0;
  GLfloat d, d1, dr, dr1, Dr, Dr1, D, D1, z1, z2, y1, y2, Y1, Z1, Y2, Z2;
  int a, a1, b, b1, c0, c1;
  GLfloat step, sstep;

  step = 180/stacks;
  sstep = 360/slices;
  a1 = startstack * step;
  b1 = startslice * sstep;
  y1 = z1 = Y1 = Z1 = 0;
  c0 = (endslice / slices) * 360;
  c1 = (endstack/stacks)*180;
  glBegin(GL_QUADS);
  for (a = startstack * step ; a <= c1 ; a+= step) {
    d=ci->sin_table[a];
    d1=ci->sin_table[a1];
    D=ci->cos_table[a];
    D1=ci->cos_table[a1];
    dr = d * r;
    dr1 = d1 * r;
    Dr = D * r;
    Dr1 = D1 * r;
    for (b = b1 ; b <= c0 ; b+= sstep) {
      y2=dr*ci->sin_table[b];
      z2=dr*ci->cos_table[b];
      Y2=dr1*ci->sin_table[b];
      Z2=dr1*ci->cos_table[b];
        glNormal3f(Dr, y1, z1);
        glVertex3f(Dr,y1,z1);
        glNormal3f(Dr, y2, z2);
        glVertex3f(Dr,y2,z2);
        glNormal3f(Dr1, Y2, Z2);
        glVertex3f(Dr1,Y2,Z2);
        glNormal3f(Dr1, Y1, Z1);
        glVertex3f(Dr1,Y1,Z1);
        polys++;
      z1=z2;
      y1=y2;
      Z1=Z2;
      Y1=Y2;
    }
    a1 = a;
  }
  glEnd();
  return polys;
}

static int DrawComponent(Circuit *ci, Component *c, unsigned long *polysP)
{
  int polys = *polysP;
  int ret = 0; /* return 1 if component is freed */

   glPushMatrix();
   glTranslatef(c->x, c->y, c->z);
     if (c->angle > 0) {
        glRotatef(c->angle, c->rotx, c->roty, c->rotz);
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
     polys += DrawResistor(ci, c->c);
   } else if (c->type == 1) {
     polys += DrawDiode(ci, c->c);
   } else if (c->type == 2) {
     polys += DrawTransistor(ci, c->c);
   } else if (c->type == 3) {
     if (((LED *)c->c)->light && ci->light) {
       GLfloat lp[] = {0.1, 0, 0, 1};
       glEnable(GL_LIGHT1);
       glLightfv(GL_LIGHT1, GL_POSITION, lp);
     }
     polys += DrawLED(ci, c->c);
   } else if (c->type == 4) {
     polys += DrawCapacitor(ci, c->c);
   } else if (c->type == 5) {
     polys += DrawIC(ci, c->c);
   } else if (c->type == 6) {
     polys += DrawDisp(ci, c->c);
   } else if (c->type == 7) {
     polys += DrawFuse(ci, c->c);
   } else if (c->type == 8) {
     polys += DrawRCA(ci, c->c);
   } else if (c->type == 9) {
     polys += DrawThreeFive(ci, c->c);
   } else if (c->type == 10) {
     polys += DrawSwitch(ci, c->c);
   }
   c->x += c->dx * MOVE_MULT;
   c->y += c->dy * MOVE_MULT;
   if (c->x > ci->XMAX/2 || c->x < 0 - ci->XMAX/2 ||
       c->y > ci->YMAX/2 || c->y < 0 - ci->YMAX/2) {
        if (c->type == 3 && ((LED *)c->c)->light && ci->light) {
          glDisable(GL_LIGHT1);
          ci->light = 0; ci->lighton = 0;
        }
	if (c->type == 5) {
          if (((IC *)c->c)->tnum)
            freetexture(ci, ((IC *)c->c)->tnum);
	}
	if (c->type == 2) {
          if (((Transistor *)c->c)->tnum)
            freetexture(ci, ((Transistor *)c->c)->tnum);
	}
        if (c->type == 1)
          free(((Diode *)c->c)->band); /* remember to free diode band */
        free(c->c);
        ret = 1;
   }

   glPopMatrix();
   glDisable(GL_NORMALIZE);
   *polysP = polys;
   return ret;
}

/* draw a resistor */

static int DrawResistor(Circuit *ci, Resistor *r)
{
  int polys = 0;
  int i;
  GLfloat col[] = {0.74, 0.62, 0.46, 1.0};
  GLfloat spec[] = {0.8, 0.8, 0.8, 1.0};
  GLfloat shine = 30;

   glTranslatef(-4, 0, 0);
   polys += wire(ci, 3);
   glTranslatef(3, 0, 0);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, col);
   glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
   glMaterialfv(GL_FRONT, GL_SHININESS, &shine);
   polys += createCylinder(ci, 1.8, 0.4, 1, 0);
   glPushMatrix();
   for (i = 0 ; i < 4 ; i++) {
     glTranslatef(0.35, 0, 0);
     glCallList(ci->band_list[r->b[i]]);
     polys += ci->band_list_polys[r->b[i]];
   }
   glPopMatrix();
   glTranslatef(1.8, 0, 0);
   polys += wire(ci, 3);
   return polys;
}

static int DrawRCA(Circuit *ci, RCA *rca)
{
  int polys = 0;
  GLfloat col[] = {0.6, 0.6, 0.6, 1.0}; /* metal */
  GLfloat red[] = {1.0, 0.0, 0.0, 1.0}; /* red */
  GLfloat white[] = {1.0, 1.0, 1.0, 1.0}; /* white */
  GLfloat spec[] = {1, 1, 1, 1}; /* glass */

   glPushMatrix();
   glTranslatef(0.3, 0, 0);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, col);
   glMateriali(GL_FRONT, GL_SHININESS, 40);
   glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
   polys += createCylinder(ci, 0.7, 0.45, 0, 0);
   glTranslatef(0.4, 0, 0);
   polys += createCylinder(ci, 0.9, 0.15, 1, 0);
   glTranslatef(-1.9, 0, 0);
   glMateriali(GL_FRONT, GL_SHININESS, 20);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, rca->col ? white : red);
   polys += createCylinder(ci, 1.5, 0.6, 1, 0);
   glTranslatef(-0.9, 0, 0);
   polys += createCylinder(ci, 0.9, 0.25, 0, 0);
   glTranslatef(0.1, 0, 0);
   polys += createCylinder(ci, 0.2, 0.3, 0, 0);
   glTranslatef(0.3, 0, 0);
   polys += createCylinder(ci, 0.2, 0.3, 1, 0);
   glTranslatef(0.3, 0, 0);
   polys += createCylinder(ci, 0.2, 0.3, 1, 0);
   glPopMatrix();
   return polys;
}

static int DrawSwitch(Circuit *ci, Switch *f)
{
  int polys = 0;
  GLfloat col[] = {0.6, 0.6, 0.6, 0}; /* metal */
  GLfloat dark[] = {0.1, 0.1, 0.1, 1.0}; /* dark */
  GLfloat brown[] = {0.69, 0.32, 0, 1.0}; /* brown */
  GLfloat spec[] = {0.9, 0.9, 0.9, 1}; /* shiny */

   glPushMatrix();
   glMaterialfv(GL_FRONT, GL_DIFFUSE, col);
   glMaterialfv(GL_FRONT, GL_AMBIENT, dark);
   glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
   glMateriali(GL_FRONT, GL_SHININESS, 90);
   polys += Rect(-0.25, 0, 0, 1.5, 0.5, 0.75);
/* polys += Rect(-0.5, 0.5, 0, 2, 0.1, 0.75); */
   glPushMatrix();
   glRotatef(90, 1, 0, 0);
   glTranslatef(-0.5, -0.4, -0.4);
   polys += HoledRectangle(ci, 0.5, 0.75, 0.1, 0.15, 8);
   glTranslatef(2, 0, 0);
   polys += HoledRectangle(ci, 0.5, 0.75, 0.1, 0.15, 8);
   glPopMatrix();
   polys += Rect(0.1, -0.4, -0.25, 0.1, 0.4, 0.05);
   polys += Rect(0.5, -0.4, -0.25, 0.1, 0.4, 0.05);
   polys += Rect(0.9, -0.4, -0.25, 0.1, 0.4, 0.05);
   polys += Rect(0.1, -0.4, -0.5, 0.1, 0.4, 0.05);
   polys += Rect(0.5, -0.4, -0.5, 0.1, 0.4, 0.05);
   polys += Rect(0.9, -0.4, -0.5, 0.1, 0.4, 0.05);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, dark);
   glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
   polys += Rect(0, 0.5, -0.1, 1, 0.05, 0.5);
   polys += Rect(0, 0.6, -0.1, 0.5, 0.6, 0.5);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, brown);
   polys += Rect(-0.2, -0.01, -0.1, 1.4, 0.1, 0.55);
   glPopMatrix();
   return polys;
}


static int DrawFuse(Circuit *ci, Fuse *f)
{
  int polys = 0;
  GLfloat col[] = {0.5, 0.5, 0.5, 1.0}; /* endcaps */
  GLfloat glass[] = {0.4, 0.4, 0.4, 0.3}; /* glass */
  GLfloat spec[] = {1, 1, 1, 1}; /* glass */

   glPushMatrix();
   glTranslatef(-1.8, 0, 0);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, col);
   glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
   glMateriali(GL_FRONT, GL_SHININESS, 40);
   polys += createCylinder(ci, 0.8, 0.45, 1, 0);
   glTranslatef(0.8, 0, 0);
   glEnable(GL_BLEND);
   glDepthMask(GL_FALSE);
   glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, glass);
   glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, 40);
   polys += createCylinder(ci, 2, 0.4, 0, 0);
   polys += createCylinder(ci, 2, 0.3, 0, 0);
   glDisable(GL_BLEND);
   glDepthMask(GL_TRUE);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, col);
   glMateriali(GL_FRONT, GL_SHININESS, 40);
   glBegin(GL_LINES);
   glVertex3f(0, 0, 0);
   glVertex3f(2, 0. ,0);
   glEnd();
   glTranslatef(2, 0, 0);
   polys += createCylinder(ci, 0.8, 0.45, 1, 0);
   glPopMatrix();
   return polys;
}


static int DrawCapacitor(Circuit *ci, Capacitor *c)
{
  int polys = 0;
  GLfloat col[] = {0, 0, 0, 0};
  GLfloat spec[] = {0.8, 0.8, 0.8, 0};
  GLfloat brown[] = {0.84, 0.5, 0};
  GLfloat shine = 40;

  glPushMatrix();
  if (c->type) {
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, brown);
    polys += sphere(ci, c->width, 15, 15, 0, 4 ,0, 15);
    glTranslatef(1.35*c->width, 0, 0);
    polys += sphere(ci, c->width, 15, 15, 11, 15, 0, 15);
    glRotatef(90, 0, 0, 1);
    glTranslatef(0, 0.7*c->width, 0.3*c->width);
    polys += wire(ci, 3*c->width);
    glTranslatef(0, 0, -0.6*c->width);
    polys += wire(ci, 3*c->width);
  } else {
    glTranslatef(0-c->length*2, 0, 0);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, col);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, &shine);
    glBegin(GL_POLYGON);
     glVertex3f(0, 0.82*c->width, -0.1);
     glVertex3f(3*c->length, 0.82*c->width, -0.1);
     glVertex3f(3*c->length, 0.82*c->width, 0.1);
     glVertex3f(0, 0.82*c->width, 0.1);
    glEnd();
    col[0] = 0.0;
    col[1] = 0.2;
    col[2] = 0.9;
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, col);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0, 1.0);
    polys += createCylinder(ci, 3.0*c->length, 0.8*c->width, 1, 0);
    glDisable(GL_POLYGON_OFFSET_FILL);
    col[0] = 0.7;
    col[1] = 0.7;
    col[2] = 0.7;
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, col);
    polys += circle(ci, 0.6*c->width, 30, 0);
    col[0] = 0;
    col[1] = 0;
    col[2] = 0;
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, col);
    glTranslatef(3.0*c->length, 0.0, 0);
    polys += circle(ci, 0.6*c->width, 30, 0);
    glTranslatef(0, 0.4*c->width, 0);
    polys += wire(ci, 3*c->length);
    glTranslatef(0.0, -0.8*c->width, 0);
    polys += wire(ci, 3.3*c->length);
  }
  glPopMatrix();
  return polys;
}

static int DrawLED(Circuit *ci, LED *l)
{
  int polys = 0;
  GLfloat col[] = {0, 0, 0, 0.6};
  GLfloat black[] = {0, 0, 0, 0.6};

  col[0] = l->r; col[1] = l->g; col[2] = l->b;
  if (l->light && ci->light) {
    GLfloat dir[] = {-1, 0, 0};
    glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, dir);
    if (!ci->lighton) {
      glLightfv(GL_LIGHT1, GL_SPECULAR, col);
      glLightfv(GL_LIGHT1, GL_AMBIENT, black);
      col[0] /= 1.5; col[1] /= 1.5; col[2] /= 1.5;
      glLightfv(GL_LIGHT1, GL_DIFFUSE, col);
      glLighti(GL_LIGHT1, GL_SPOT_CUTOFF, (GLint) 90);
      glLighti(GL_LIGHT1, GL_CONSTANT_ATTENUATION, (GLfloat)1); 
      glLighti(GL_LIGHT1, GL_LINEAR_ATTENUATION, (GLfloat)0); 
      glLighti(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, (GLfloat)0); 
      glLighti(GL_LIGHT1, GL_SPOT_EXPONENT, (GLint) 20);
      ci->lighton = 1;
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
  glTranslatef(-0.9, 0, 0);
  polys += createCylinder(ci, 1.2, 0.3, 0, 0);
  if (l->light && ci->light) {
    glDisable(GL_LIGHTING);
    glColor3fv(col);
  }
  polys += sphere(ci, 0.3, 7, 7, 3, 7, 0, 7);
  if (l->light && ci->light) {
    glEnable(GL_LIGHTING);
  } else {
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
  }

  glTranslatef(1.2, 0, 0);
  polys += createCylinder(ci, 0.1, 0.38, 1, 0);
  glTranslatef(-0.3, 0.15, 0);
  polys += wire(ci, 3);
  glTranslatef(0, -0.3, 0);
  polys += wire(ci, 3.3);
  if (random() % 50 == 25) {
    if (l->light) {
      l->light = 0; ci->light = 0; ci->lighton = 0;
      glDisable(GL_LIGHT1);
    } else if (!ci->light) {
      l->light = 1; 
      ci->light = 1;
    }
  }
  return polys;
}


static int DrawThreeFive(Circuit *ci, ThreeFive *d)
{
  int polys = 0;
  GLfloat shine = 40;
  GLfloat const dark[] = {0.3, 0.3, 0.3, 0};
  GLfloat const light[] = {0.6, 0.6, 0.6, 0};
  GLfloat const cream[] = {0.8, 0.8, 0.6, 0};
  GLfloat const spec[] = {0.7, 0.7, 0.7, 0};

   glPushMatrix();
   glMaterialfv(GL_FRONT, GL_SHININESS, &shine);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, cream);
   glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
   
   glTranslatef(-2.0, 0, 0);
   polys += createCylinder(ci, 0.7, 0.2, 0, 0);
   glTranslatef(0.7, 0, 0);
   polys += createCylinder(ci, 1.3, 0.4, 1, 0);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, light);
   glTranslatef(1.3, 0, 0);
   polys += createCylinder(ci, 1.3, 0.2, 0, 0);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, dark);
   glTranslatef(0.65, 0, 0);
   polys += createCylinder(ci, 0.15, 0.21, 0, 0);
   glTranslatef(0.3, 0, 0);
   polys += createCylinder(ci, 0.15, 0.21, 0, 0);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, light);
   glTranslatef(0.4, 0, 0);
   polys += sphere(ci, 0.23, 7, 7, 0, 5, 0, 7);

   glPopMatrix();
   return polys;
}

static int DrawDiode(Circuit *ci, Diode *d)
{
  int polys = 0;
  GLfloat shine = 40;
  GLfloat col[] = {0.3, 0.3, 0.3, 0};
  GLfloat spec[] = {0.7, 0.7, 0.7, 0};

   glPushMatrix();
   glMaterialfv(GL_FRONT, GL_SHININESS, &shine);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, col);
   glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
   glTranslatef(-4, 0, 0);
   polys += wire(ci, 3);
   glTranslatef(3, 0, 0);
   polys += bandedCylinder(ci, 0.3, 1.5, d->r, d->g, d->b, &(d->band), 1);
   glTranslatef(1.5, 0, 0);
   polys += wire(ci, 3);
   glPopMatrix();
   return polys;
}

static int Rect(GLfloat x, GLfloat y, GLfloat z, GLfloat w, GLfloat h,
            GLfloat t)
{
  int polys = 0;
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
    polys++;
  /* back */
    glNormal3f(0, 0, -1);
    glVertex3f(x, y, zt);
    glVertex3f(x, yh, zt);
    glVertex3f(xw, yh, zt);
    glVertex3f(xw, y, zt);
    polys++;
  /* top */
    glNormal3f(0, 1, 0);
    glVertex3f(x, yh, z);
    glVertex3f(x, yh, zt);
    glVertex3f(xw, yh, zt);
    glVertex3f(xw, yh, z);
    polys++;
  /* bottom */
    glNormal3f(0, -1, 0);
    glVertex3f(x, y, z);
    glVertex3f(x, y, zt);
    glVertex3f(xw, y, zt);
    glVertex3f(xw, y, z);
    polys++;
  /* left */
    glNormal3f(-1, 0, 0);
    glVertex3f(x, y, z);
    glVertex3f(x, y, zt);
    glVertex3f(x, yh, zt);
    glVertex3f(x, yh, z);
    polys++;
  /* right */
    glNormal3f(1, 0, 0);
    glVertex3f(xw, y, z);
    glVertex3f(xw, y, zt);
    glVertex3f(xw, yh, zt);
    glVertex3f(xw, yh, z);
    polys++;
  glEnd();
  return polys;
}

/* IC pins */

static int ICLeg(GLfloat x, GLfloat y, GLfloat z, int dir)
{
  int polys = 0;
  if (dir) {
    polys += Rect(x-0.1, y, z, 0.1, 0.1, 0.02);
    polys += Rect(x-0.1, y, z, 0.02, 0.1, 0.1);
    polys += Rect(x-0.1, y+0.03, z-0.1, 0.02, 0.05, 0.3);
  } else {
    polys += Rect(x, y, z, 0.1, 0.1, 0.02);
    polys += Rect(x+0.8*0.1, y, z, 0.02, 0.1, 0.1);
    polys += Rect(x+0.8*0.1, y+0.03, z-0.1, 0.02, 0.05, 0.3);
  }
  return polys;
}


static int DrawIC(Circuit *ci, IC *c)
{
  int polys = 0;
  GLfloat w, h, d;
  int z;
  GLfloat col[] = {0.1, 0.1, 0.1, 0};
  GLfloat col2[] = {0.2, 0.2, 0.2, 0};
  GLfloat spec[] = {0.6, 0.6, 0.6, 0};
  GLfloat shine = 40;
  GLfloat lspec[] = {0.6, 0.6, 0.6, 0};
  GLfloat lcol[] = {0.4, 0.4, 0.4, 0};
  GLfloat lshine = 40;
  float mult, th, size;

  glPushMatrix();
  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, col);
  glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0, 1.0);
    glBegin(GL_QUADS);
      glNormal3f(0, 0, 1);
      glVertex3f(w, h, 0.1);
      glVertex3f(w, -h, 0.1);
      glVertex3f(-w, -h, 0.1);
      glVertex3f(-w, h, 0.1);
      polys++;
      glNormal3f(0, 0, -1);
      glVertex3f(w, h, -0.1);
      glVertex3f(w, -h, -0.1);
      glVertex3f(-w, -h, -0.1);
      glVertex3f(-w, h, -0.1);
      polys++;
      glNormal3f(1, 0, 0);
      glVertex3f(w, h, -0.1);
      glVertex3f(w, -h, -0.1);
      glVertex3f(w, -h, 0.1);
      glVertex3f(w, h, 0.1);
      polys++;
      glNormal3f(0, -1, 0);
      glVertex3f(w, -h, -0.1);
      glVertex3f(w, -h, 0.1);
      glVertex3f(-w, -h, 0.1);
      glVertex3f(-w, -h, -0.1);
      polys++;
      glNormal3f(-1, 0, 0);
      glVertex3f(-w, h, -0.1);
      glVertex3f(-w, h, 0.1);
      glVertex3f(-w, -h, 0.1);
      glVertex3f(-w, -h, -0.1);
      polys++;
      glNormal3f(0, -1, 0);
      glVertex3f(-w, h, -0.1);
      glVertex3f(w, h, -0.1);
      glVertex3f(w, h, 0.1);
      glVertex3f(-w, h, 0.1);
      polys++;
    glEnd();
    glDisable(GL_POLYGON_OFFSET_FILL);
    if (c->tnum) glBindTexture(GL_TEXTURE_2D, c->tnum);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    if (c->pins == 8)
      size = 0.4;
    else
      size = 0.6;
    th = size*2/3;
    mult = size*c->tw / c->th;
    mult /= 2;
    glBegin(GL_QUADS); /* text markings */
     glNormal3f(0, 0, 1);
     glTexCoord2f(0, 1);
     glVertex3f(th, mult, 0.1);
     glTexCoord2f(1, 1);
     glVertex3f(th, -mult, 0.1);
     glTexCoord2f(1, 0);
     glVertex3f(-th, -mult, 0.1);
     glTexCoord2f(0, 0);
     glVertex3f(-th, mult, 0.1);
      polys++;
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    d = (h*2-0.1) / c->pins;
    d*=2;
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, lcol);
    glMaterialfv(GL_FRONT, GL_SPECULAR, lspec);
    glMaterialfv(GL_FRONT, GL_SHININESS, &lshine);
    for (z = 0 ; z < c->pins/2 ; z++) {
      polys += ICLeg(w, -h + z*d + d/2, 0, 0);
    }
    for (z = 0 ; z < c->pins/2 ; z++) {
      polys += ICLeg(-w, -h + z*d + d/2, 0, 1);
    }
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, col2);
    glTranslatef(-w+0.3, h-0.3, 0.1);
    glRotatef(90, 0, 1, 0);
    polys += circle(ci, 0.1, 7, 0);
    glPopMatrix();
    return polys;
}

static int DrawDisp(Circuit *ci, Disp *d)
{
  int polys = 0;
  GLfloat col[] = {0.8, 0.8, 0.8, 1.0}; /* body colour */
  GLfloat front[] = {0.2, 0.2, 0.2, 1.0}; /* front colour */
  GLfloat on[] = {0.9, 0, 0, 1}; /* 'on' segment */
  GLfloat off[] = {0.3, 0, 0, 1}; /* 'off' segment */
  int i, j, k;
  GLfloat x, y; /* for the pins */
  GLfloat spec[] = {0.6, 0.6, 0.6, 0};
  GLfloat lcol[] = {0.4, 0.4, 0.4, 0};
  GLfloat shine = 40;
  static const GLfloat vdata_h[6][2] = {
    {0, 0},
    {0.1, 0.1},
    {0.9, 0.1},
    {1, 0},
    {0.9, -0.1},
    {0.1, -0.1}
  };
  static const GLfloat vdata_v[6][2] = {
    {0.27, 0},
    {0.35, -0.1},
    {0.2, -0.9},
    {0.1, -1},
    {0, -0.9},
    {0.15, -0.15}
  };

  static const GLfloat seg_start[7][2] = {
    {0.55, 2.26},
    {1.35, 2.26},
    {1.2, 1.27},
    {0.25, 0.25},
    {0.06, 1.25},
    {0.25, 2.25},
    {0.39, 1.24}
  };

  static const int nums[10][7] = {
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

   glTranslatef(-0.9, -1.8, 0);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, col);
   polys += Rect(0, 0, -0.01, 1.8, 2.6, 0.7);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, front);
   glBegin(GL_QUADS);
     glVertex2f(-0.05, -0.05);
     glVertex2f(-0.05, 2.65);
     glVertex2f(1.85, 2.65);
     glVertex2f(1.85, -0.05);
     polys++;
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
     polys++;
     glEnd();
   }
   glColor3fv(on);
   glPointSize(4);
   glBegin(GL_POINTS);
    glVertex3f(1.5, 0.2, 0.01);
    polys++;
   glEnd();
   glEnable(GL_LIGHTING);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, lcol);
   glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
   glMaterialfv(GL_FRONT, GL_SHININESS, &shine);
   for (x = 0.35 ; x <= 1.5 ; x+= 1.15) {
     for ( y = 0.2 ; y <= 2.4 ; y += 0.3) {
       polys += ICLeg(x, y, -0.7, 1);
     }
   }
   return polys;
}

static int HoledRectangle(Circuit *ci, 
                           GLfloat w, GLfloat h, GLfloat d, GLfloat radius,
                           int p)
{
  int polys = 0;
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
    y2=radius*(float)ci->sin_table[(int)a];
    x2=radius*(float)ci->cos_table[(int)a];

    if (a < 45 || a > 315) {
      xr = side;
      yr = side1 * ci->tan_table[a];
      nx = 1; ny = 0;
    } else if (a <= 135 || a >= 225) {
      xr = side/ci->tan_table[a];
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
      yr = -side1 * ci->tan_table[a];
      nx = -1; ny = 0;
    }

      glNormal3f(-x1, -y1, 0); /* cylinder */
      glVertex3f(x1,y1,0);
      glVertex3f(x1,y1,-d);
      glVertex3f(x2,y2,-d);
      glVertex3f(x2,y2,0);
      polys++;

      glNormal3f(0, 0, 1); /* front face */
      glVertex3f(x1,y1,0);
      glVertex3f(xr1, yr1, 0);
      glVertex3f(xr, yr, 0);
      glVertex3f(x2, y2, 0);
      polys++;

      glNormal3f(nx, ny, 0); /* side */
      glVertex3f(xr, yr, 0);
      glVertex3f(xr, yr, -d);
      glVertex3f(xr1, yr1, -d);
      glVertex3f(xr1, yr1, 0);
      polys++;

      glNormal3f(0, 0, -1); /* back */
      glVertex3f(xr, yr, -d);
      glVertex3f(x2, y2, -d);
      glVertex3f(x1, y1, -d);
      glVertex3f(xr1, yr1, -d);
      polys++;

    x1=x2;
    y1=y2;
    xr1 = xr; yr1 = yr;
  }
  glEnd();
  return polys;
}

static int DrawTransistor(Circuit *ci, Transistor *t)
{
  int polys = 0;
  GLfloat col[] = {0.3, 0.3, 0.3, 1.0};
  GLfloat spec[] = {0.9, 0.9, 0.9, 1.0};
  GLfloat nospec[] = {0.4, 0.4, 0.4, 1.0};
  GLfloat shin = 30;

  glPushMatrix();
  glMaterialfv(GL_FRONT, GL_SHININESS, &shin);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, col);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  if (t->type == 1) { /* TO-92 style */
    float mult, y1, y2;
    mult = 1.5*t->th/t->tw;
    y1 = 0.2+mult/2;
    y2 = 0.8-mult/2;
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, col);
    glRotatef(90, 0, 1, 0);
    glRotatef(90, 0, 0, 1);
    polys += createCylinder(ci, 1.0, 0.4, 1, 1);
    polys += Rect(0, -0.2, 0.4, 1, 0.2, 0.8);
/* Draw the markings */
    glEnable(GL_TEXTURE_2D);
    if (t->tnum) glBindTexture(GL_TEXTURE_2D, t->tnum);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glBegin (GL_QUADS);
     glNormal3f(0, 0, 1);
     glTexCoord2f(0, 1);
     glVertex3f(y1, -0.21, 0.3);
     glTexCoord2f(1, 1);
     glVertex3f(y1, -0.21, -0.3);
     glTexCoord2f(1, 0);
     glVertex3f(y2, -0.21, -0.3);
     glTexCoord2f(0, 0);
     glVertex3f(y2, -0.21, 0.3);
     polys++;
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glTranslatef(-2, 0, -0.2);
    polys += wire(ci, 2);
    glTranslatef(0, 0, 0.2);
    polys += wire(ci, 2);
    glTranslatef(0, 0, 0.2);
    polys += wire(ci, 2);
  } else if (t->type == 0) { /* TO-220 Style */
    float mult, y1, y2;
    mult = 1.5*t->th/t->tw;
    y1 = 0.75+mult/2;
    y2 = 0.75-mult/2;
    polys += Rect(0, 0, 0, 1.5, 1.5, 0.5);
    glEnable(GL_TEXTURE_2D);
    if (t->tnum) glBindTexture(GL_TEXTURE_2D, t->tnum);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glBegin (GL_QUADS);
     glNormal3f(0, 0, 1);
     glTexCoord2f(0, 1);
     glVertex3f(0, y1, 0.01);
     glTexCoord2f(1, 1);
     glVertex3f(1.5, y1, 0.01);
     glTexCoord2f(1, 0);
     glVertex3f(1.5, y2, 0.01);
     glTexCoord2f(0, 0);
     glVertex3f(0, y2, 0.01);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, col);
    glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
    glMaterialfv(GL_FRONT, GL_SHININESS, &shin);
    polys += Rect(0, 0, -0.5, 1.5, 1.5, 0.30);
    if (!glIsEnabled(GL_NORMALIZE)) glEnable(GL_NORMALIZE);
    glTranslatef(0.75, 1.875, -0.55);
    polys += HoledRectangle(ci, 1.5, 0.75, 0.25, 0.2, 8);
    glMaterialfv(GL_FRONT, GL_SPECULAR, nospec);
    glTranslatef(-0.375, -1.875, 0);
    glRotatef(90, 0, 0, -1);
    polys += wire(ci, 2);
    glTranslatef(0, 0.375, 0);
    polys += wire(ci, 2);
    glTranslatef(0, 0.375, 0);
    polys += wire(ci, 2);
  } else {              /* SMC transistor */
/* Draw the body */
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, col);
    glTranslatef(-0.5, -0.25, 0.1);
    polys += Rect(0, 0, 0, 1, 0.5, 0.2);
/* Draw the markings */
    glEnable(GL_TEXTURE_2D);
    if (t->tnum) glBindTexture(GL_TEXTURE_2D, t->tnum);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glBegin (GL_QUADS);
     glNormal3f(0, 0, 1);
     glTexCoord2f(0, 1);
     glVertex3f(0.2, 0, 0.01);
     glTexCoord2f(1, 1);
     glVertex3f(0.8, 0, 0.01);
     glTexCoord2f(1, 0);
     glVertex3f(0.8, 0.5, 0.01);
     glTexCoord2f(0, 0);
     glVertex3f(0.2, 0.5, 0.01);
     polys++;
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
/* Now draw the legs */
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, col);
    glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
    glMaterialfv(GL_FRONT, GL_SHININESS, &shin);
    polys += Rect(0.25, -0.1, -0.05, 0.1, 0.1, 0.2);
    polys += Rect(0.75, -0.1, -0.05, 0.1, 0.1, 0.2);
    polys += Rect(0.5, 0.5, -0.05, 0.1, 0.1, 0.2);
    polys += Rect(0.25, -0.2, -0.2, 0.1, 0.15, 0.1);
    polys += Rect(0.75, -0.2, -0.2, 0.1, 0.15, 0.1);
    polys += Rect(0.5, 0.5, -0.2, 0.1, 0.15, 0.1);
  }
  glPopMatrix();
  return polys;
}

static Component * NewComponent(ModeInfo *mi)
{
  Circuit *ci = &circuit[MI_SCREEN(mi)];
  Component *c;
  float rnd;

  c = malloc(sizeof(Component));
  c->angle = RAND_RANGE(0,360);
  rnd = f_rand();
  if (rnd < 0.25) { /* come from the top */
     c->y = ci->YMAX/2;
     c->x = RAND_RANGE(0, ci->XMAX) - ci->XMAX/2;
     if (c->x > 0)
       c->dx = 0 - RAND_RANGE(0.5, 2);
     else 
       c->dx = RAND_RANGE(0.5, 2);
     c->dy = 0 - RAND_RANGE(0.5, 2);
  } else if (rnd < 0.5) { /* come from the bottom */
     c->y = 0 - ci->YMAX/2;
     c->x = RAND_RANGE(0, ci->XMAX) - ci->XMAX/2;
     if (c->x > 0)
       c->dx = 0 - RAND_RANGE(0.5, 2);
     else 
       c->dx = RAND_RANGE(0.5, 2);
     c->dy = RAND_RANGE(0.5, 2);
  } else if (rnd < 0.75) { /* come from the left */
     c->x = 0 - ci->XMAX/2;
     c->y = RAND_RANGE(0, ci->YMAX) - ci->YMAX/2;
     c->dx = RAND_RANGE(0.5, 2);
     if (c->y > 0)
       c->dy = 0 - RAND_RANGE(0.5, 2);
     else 
       c->dy = RAND_RANGE(0.5, 2);
  } else { /* come from the right */
     c->x = ci->XMAX/2;
     c->y = RAND_RANGE(0, ci->YMAX) - ci->YMAX/2;
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
  c->drot = f_rand() * 3;
  c->rdeg = 0;
  c->dz = f_rand()*2 - 1;
  c->norm = 0;
  c->alpha = 0; /* explicitly set to 1 later */
  rnd = random() % 11;
  if (rnd < 1) {
    c->c = NewResistor();
    c->type = 0;
    if (f_rand() < 0.4)
      c->norm = 1; /* some resistors shine */
  } else if (rnd < 2) {
    c->c = NewDiode();
    if (f_rand() < 0.4)
      c->norm = 1; /* some diodes shine */
    c->type = 1;
  } else if (rnd < 3) {
    c->c = NewTransistor(mi);
    c->norm = 1;
    c->type = 2;
  } else if (rnd < 4) {
    c->c = NewCapacitor(ci);
    c->norm = 1;
    c->type = 4;
  } else if (rnd < 5) {
    c->c = NewIC(mi);
    c->type = 5;
    c->norm = 1;
  } else if (rnd < 6) {
    c->c = NewLED(ci);
    c->type = 3;
    c->norm = 1;
    c->alpha = 1;
  } else if (rnd < 7) {
    c->c = NewFuse(ci);
    c->norm = 1;
    c->type = 7;
    c->alpha = 1;
  } else if (rnd < 8) {
    c->c = NewRCA(ci);
    c->norm = 1;
    c->type = 8;
  } else if (rnd < 9) {
    c->c = NewThreeFive(ci);
    c->norm = 1;
    c->type = 9;
  } else if (rnd < 10) {
    c->c = NewSwitch(ci);
    c->norm = 1;
    c->type = 10;
  } else {
    c->c = NewDisp(ci);
    c->type = 6;
  }
  return c;
}

static Transistor *NewTransistor(ModeInfo *mi)
{
  Transistor *t;
  float texfg[] = {0.7, 0.7, 0.7, 1.0};
  float texbg[] = {0.3, 0.3, 0.3, 0.1};
  TexNum *tn;
  const char *val;

  t = malloc(sizeof(Transistor));
  t->type = (random() % 3);
  if (t->type == 0) {
    val = transistortypes[random() % countof(transistortypes)];
    tn = fonttexturealloc(mi, val, texfg, texbg);
    if (tn == NULL) {
      fprintf(stderr, "Error getting a texture for a string!\n");
      t->tnum = 0;
    } else {
      t->tnum = tn->num;
      t->tw = tn->w; t->th = tn->h;
      free(tn);
    }
  } else if (t->type == 2) {
    val = smctypes[random() % countof(smctypes)];
    tn = fonttexturealloc(mi, val, texfg, texbg);
    if (tn == NULL) {
      fprintf(stderr, "Error getting a texture for a string!\n");
      t->tnum = 0;
    } else {
      t->tnum = tn->num;
      t->tw = tn->w; t->th = tn->h;
      free(tn);
    }
  } else if (t->type == 1) {
    val = to92types[random() % countof(to92types)];
    tn = fonttexturealloc(mi, val, texfg, texbg);
    if (tn == NULL) {
      fprintf(stderr, "Error getting a texture for a string!\n");
      t->tnum = 0;
    } else {
      t->tnum = tn->num;
      t->tw = tn->w; t->th = tn->h;
      free(tn);
    }
  }

  return t;
}

static Capacitor *NewCapacitor(Circuit *ci)
{
  Capacitor *c;

  c = malloc(sizeof(Capacitor));
  c->type = (f_rand() < 0.5);
  if (!c->type) {
    c->length = RAND_RANGE(0.5, 1);
    c->width = RAND_RANGE(0.5, 1);
  } else {
    c->width = RAND_RANGE(0.3, 1);
  }
  return c;
}

/* 7 segment display */

static Disp *NewDisp(Circuit *ci)
{
  Disp *d;

  d = malloc(sizeof(Disp));
  if (seven)
    d->value = 7;
  else
    d->value = RAND_RANGE(0, 10);
  return d;
}


static IC *NewIC(ModeInfo *mi)
{
  IC *c;
  int pins;
  TexNum *tn;
  float texfg[] = {0.7, 0.7, 0.7, 1.0};
  float texbg[] = {0.1, 0.1, 0.1, 0};
  const char *val;
  char *str;
  int types[countof(ictypes)], i, n = 0;

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
  for (i = 0 ; i < countof(ictypes) ; i++) {
    if (ictypes[i].pins == pins) {
       types[n] = i;
       n++;
    }
  }

  if (n > countof(types)) abort();
  val = ictypes[types[random() % n]].val;
  str = malloc(strlen(val) + 1 + 4 + 1); /* add space for production date */
  sprintf(str, "%s\n%02d%02d", val, (int)RAND_RANGE(80, 100), (int)RAND_RANGE(1,53));
  tn = fonttexturealloc(mi, str, texfg, texbg);
  free(str);
  if (tn == NULL) {
    fprintf(stderr, "Error allocating font texture for '%s'\n", val);
    c->tnum = 0;
  } else {
    c->tw = tn->w; c->th = tn->h;
    c->tnum = tn->num;
    free(tn);
  }
  c->pins = pins;
  return c;
}

static LED *NewLED(Circuit *ci)
{
  LED *l;
  float r;

  l = malloc(sizeof(LED));
  r = f_rand();
  l->light = 0;
  if (!ci->light && (f_rand() < 0.4)) {
     ci->light = 1;
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

static Fuse *NewFuse(Circuit *ci)
{
  Fuse *f;

  f = malloc(sizeof(Fuse));
  return f;
}

static RCA *NewRCA(Circuit *ci)
{
  RCA *r;

  r = malloc(sizeof(RCA));
  r->col = (random() % 10 < 5);
  return r;
}

static ThreeFive *NewThreeFive(Circuit *ci)
{
  ThreeFive *r;

  r = malloc(sizeof(ThreeFive));
  return r;
}

static Switch *NewSwitch(Circuit *ci)
{
  Switch *s;

  s = malloc(sizeof(Switch));
  s->position = 0;
  return s;
}

static Diode *NewDiode(void)
{
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


static Resistor  * NewResistor(void)
{
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

static void makebandlist(Circuit *ci)
{
  int i;
  GLfloat col[] = {0,0,0,0};
  GLfloat spec[] = {0.8,0.8,0.8,0};
  GLfloat shine = 40;

   for (i = 0 ; i < 12 ; i++) {
     ci->band_list[i] = glGenLists(1);
     glNewList(ci->band_list[i], GL_COMPILE);
     col[0] = colorcodes[i][0];
     col[1] = colorcodes[i][1];
     col[2] = colorcodes[i][2];
     glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, col);
     glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
     glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, &shine);
     ci->band_list_polys[i] = createCylinder(ci, 0.1, 0.42, 0, 0);
     glEndList();
  }
}
  

static int bandedCylinder(Circuit *ci, 
                           float radius, float l, 
                           GLfloat r, GLfloat g, GLfloat bl, 
                           Band **b, int nbands)
{
  int polys = 0;
  int n; /* band number */
  GLfloat col[] = {0,0,0,0};

   col[0] = r; col[1] = g; col[2] = bl;
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, col);
   polys += createCylinder(ci, l, radius, 1, 0); /* body */
   for (n = 0 ; n < nbands ; n++) {
     glPushMatrix();
     glTranslatef(b[n]->pos*l, 0, 0);
     col[0] = b[n]->r; col[1] = b[n]->g; col[2] = b[n]->b;
     glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, col);
     polys += createCylinder(ci, b[n]->len*l, radius*1.05, 0, 0); /* band */
     glPopMatrix();
   }
   return polys;
}

static int drawgrid(Circuit *ci)
{
  int polys = 0;
  GLfloat x, y;
  GLfloat col3[] = {0, 0.8, 0};

  if (!ci->draw_s) {
     if (f_rand() < ((rotatespeed > 0) ? 0.05 : 0.01)) {
       ci->draw_sdir = RAND_RANGE(0, 4);
       ci->draw_ds = RAND_RANGE(0.4, 0.8);
       switch (ci->draw_sdir) {
          case 0:
           ci->draw_sx = -ci->XMAX/2;
           ci->draw_sy = ((int)RAND_RANGE(0, ci->YMAX/2))*2 - ci->YMAX/2;
           break;
          case 1:
           ci->draw_sx = ci->XMAX/2;
           ci->draw_sy = ((int)RAND_RANGE(0, ci->YMAX/2))*2 - ci->YMAX/2;
           break;
          case 2:
           ci->draw_sy = ci->YMAX/2;
           ci->draw_sx = ((int)RAND_RANGE(0, ci->XMAX/2))*2 - ci->XMAX/2;
           break;
          case 3:
           ci->draw_sy = -ci->YMAX/2;
           ci->draw_sx = ((int)RAND_RANGE(0, ci->XMAX/2))*2 - ci->XMAX/2;
           break;
        }
        ci->draw_s = 1;
      }
  } else if (rotatespeed <= 0) {
    if (ci->grid_col[1] < 0.25) {
      ci->grid_col[1] += 0.025; ci->grid_col[2] += 0.005;
      ci->grid_col2[1] += 0.015 ; ci->grid_col2[2] += 0.005;
    }
  }

  glDisable(GL_LIGHTING);
  if (ci->draw_s) {
    glColor3fv(col3);
    glPushMatrix();
    glTranslatef(ci->draw_sx, ci->draw_sy, -10);
    polys += sphere(ci, 0.1, 10, 10, 0, 10, 0, 10);
    if (ci->draw_sdir == 0) 
      glTranslatef(-ci->draw_ds, 0, 0);
    if (ci->draw_sdir == 1) 
      glTranslatef(ci->draw_ds, 0, 0);
    if (ci->draw_sdir == 2) 
      glTranslatef(0, ci->draw_ds, 0);
    if (ci->draw_sdir == 3) 
      glTranslatef(0, -ci->draw_ds, 0);
    polys += sphere(ci, 0.05, 10, 10, 0, 10, 0, 10);
    glPopMatrix();
    if (ci->draw_sdir == 0) {
       ci->draw_sx += ci->draw_ds;
       if (ci->draw_sx > ci->XMAX/2)
         ci->draw_s = 0;
    }
    if (ci->draw_sdir == 1) {
       ci->draw_sx -= ci->draw_ds;
       if (ci->draw_sx < -ci->XMAX/2)
         ci->draw_s = 0;
    }
    if (ci->draw_sdir == 2) {
       ci->draw_sy -= ci->draw_ds;
       if (ci->draw_sy < ci->YMAX/2)
         ci->draw_s = 0;
    }
    if (ci->draw_sdir == 3) {
       ci->draw_sy += ci->draw_ds;
       if (ci->draw_sy > ci->YMAX/2)
         ci->draw_s = 0;
    }
  } else if (rotatespeed <= 0) {
    if (ci->grid_col[1] > 0) {
      ci->grid_col[1] -= 0.0025; ci->grid_col[2] -= 0.0005;
      ci->grid_col2[1] -= 0.0015 ; ci->grid_col2[2] -= 0.0005;
    }
  }
  for (x = -ci->XMAX/2 ; x <= ci->XMAX/2 ; x+= 2) {
    glColor3fv(ci->grid_col);
    glBegin(GL_LINES);
     glVertex3f(x, ci->YMAX/2, -10);
     glVertex3f(x, -ci->YMAX/2, -10);
     glColor3fv(ci->grid_col2);
     glVertex3f(x-0.02, ci->YMAX/2, -10);
     glVertex3f(x-0.02, -ci->YMAX/2, -10);
     glVertex3f(x+0.02, ci->YMAX/2, -10);
     glVertex3f(x+0.02, -ci->YMAX/2, -10);
    glEnd();
  }
  for (y = -ci->YMAX/2 ; y <= ci->YMAX/2 ; y+= 2) {
    glColor3fv(ci->grid_col);
    glBegin(GL_LINES);
     glVertex3f(-ci->XMAX/2, y, -10);
     glVertex3f(ci->XMAX/2, y, -10);
     glColor3fv(ci->grid_col2);
     glVertex3f(-ci->XMAX/2, y-0.02, -10);
     glVertex3f(ci->XMAX/2, y-0.02, -10);
     glVertex3f(-ci->XMAX/2, y+0.02, -10);
     glVertex3f(ci->XMAX/2, y+0.02, -10);
    glEnd();
  }
  glEnable(GL_LIGHTING);
  return polys;
}

static void display(ModeInfo *mi)
{
  Circuit *ci = &circuit[MI_SCREEN(mi)];
  GLfloat light_sp[] = {0.8, 0.8, 0.8, 1.0};
  GLfloat black[] = {0, 0, 0, 1.0};
  int j;

  mi->polygon_count = 0;

  if (ci->display_i == 0) {
    for (ci->display_i = 0 ; ci->display_i < maxparts ; ci->display_i++) {
      ci->components[ci->display_i] = NULL;
    }
  } 
  glEnable(GL_LIGHTING);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();
  gluLookAt(ci->viewer[0], ci->viewer[1], ci->viewer[2], 
            0.0, 0.0, 0.0, 
            0.0, 1.0, 0.0);
  glPushMatrix();
  glRotatef(ci->rotate_angle, 0, 0, 1);
  ci->rotate_angle += 0.01 * (float)rotatespeed;
  if (ci->rotate_angle >= 360) ci->rotate_angle = 0;

  glLightfv(GL_LIGHT0, GL_POSITION, ci->lightpos);
  glLightfv(GL_LIGHT0, GL_SPECULAR, light_sp);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, light_sp);
  glLighti(GL_LIGHT0, GL_CONSTANT_ATTENUATION, (GLfloat)1); 
  glLighti(GL_LIGHT0, GL_LINEAR_ATTENUATION, (GLfloat)0.5); 
  glLighti(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, (GLfloat)0); 
  mi->polygon_count += drawgrid(ci);
  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, light_sp);
  if (f_rand() < 0.05) {
    for (j = 0 ; j < maxparts ; j++) {
      if (ci->components[j] == NULL) {
        ci->components[j] = NewComponent(mi);
        j = maxparts;
      }
    }
    reorder(&ci->components[0]);
  }
  for (j = 0 ; j < maxparts ; j++) {
     glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, black);
     glMaterialfv(GL_FRONT, GL_EMISSION, black);
     glMaterialfv(GL_FRONT, GL_SPECULAR, black);
     if (ci->components[j] != NULL) {
       if (DrawComponent(ci, ci->components[j], &mi->polygon_count)) {
        free(ci->components[j]); ci->components[j] = NULL;
       }
     }
  }
  glPopMatrix();
  glFlush();
}

static void freetexture (Circuit *ci, GLuint texture) 
{
  ci->s_refs[texture]--;
  if (ci->s_refs[texture] < 1) {
    glDeleteTextures(1, &texture);
  }
}

static TexNum * fonttexturealloc (ModeInfo *mi,
                                  const char *str, float *fg, float *bg)
{
  Circuit *ci = &circuit[MI_SCREEN(mi)];
  int i, status;
  XImage *ximage;
  char *c;
  TexNum *t;

  if (ci->font_init == 0) {
    for (i = 0 ; i < 50 ; i++) {
      ci->font_strings[i] = NULL;
      ci->s_refs[i] = 0;
      ci->font_w[i] = 0; ci->font_h[i] = 0;
    }
    ci->font_init++;
  }
  for (i = 0 ; i < 50 ; i++) {
    if (!ci->s_refs[i] && ci->font_strings[i]) {
      free (ci->font_strings[i]);
      ci->font_strings[i] = NULL;
    }
    if (ci->font_strings[i] && !strcmp(str, ci->font_strings[i])) { /* if one matches */
      t = malloc(sizeof(TexNum));
      t->w = ci->font_w[i]; t->h = ci->font_h[i];
      t->num = i;
      ci->s_refs[i]++;
      return t;
    }
  }

  /* at this point we need to make the new texture */
  ximage = text_to_ximage (mi->xgwa.screen,
                           mi->xgwa.visual,
                           font, str,
                           fg, bg);
  for (i = 0 ; ci->font_strings[i] != NULL ; i++) { /* set i to the next unused value */
     if (i > 49) {
        fprintf(stderr, "Texture cache full!\n");
        free(ximage->data);
        ximage->data = 0;
        XFree (ximage);
        return NULL;
     }
  }

  glBindTexture(GL_TEXTURE_2D, i);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  clear_gl_error();
  status = gluBuild2DMipmaps(GL_TEXTURE_2D, 4, ximage->width, ximage->height,
                             GL_RGBA, GL_UNSIGNED_BYTE, ximage->data);
  if (status)
    {
      const char *s = (char *) gluErrorString (status);
      fprintf (stderr, "%s: error mipmapping %dx%d texture: %s\n",
               progname, ximage->width, ximage->height,
               (s ? s : "(unknown)"));
      exit (1);
    }
  check_gl_error("mipmapping");

  t = malloc(sizeof(TexNum));
  t->w = ximage->width;
  t->h = ximage->height;
  ci->font_w[i] = t->w; ci->font_h[i] = t->h;
  free(ximage->data);
  ximage->data = 0;
  XFree (ximage);
  c = malloc(strlen(str)+1);
  strncpy(c, str, strlen(str)+1);
  ci->font_strings[i] = c;
  ci->s_refs[i]++;
  t->num = i;
  return t;
}

/* ensure transparent components are at the end */
static void reorder(Component *c[])
{
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

ENTRYPOINT void reshape_circuit(ModeInfo *mi, int width, int height)
{
 Circuit *ci = &circuit[MI_SCREEN(mi)];
 GLfloat h = (GLfloat) height / (GLfloat) width;
 glViewport(0,0,(GLint)width, (GLint) height);
 glMatrixMode(GL_PROJECTION);
 glLoadIdentity();
 glFrustum(-1.0,1.0,-h,h,1.5,35.0);
 glMatrixMode(GL_MODELVIEW);
 ci->win_h = height; 
 ci->win_w = width;
 ci->YMAX = ci->XMAX * h;
}


ENTRYPOINT void init_circuit(ModeInfo *mi)
{
int screen = MI_SCREEN(mi);
Circuit *ci;

 if (circuit == NULL) {
   if ((circuit = (Circuit *) calloc(MI_NUM_SCREENS(mi),
                                        sizeof(Circuit))) == NULL)
          return;
 }
 ci = &circuit[screen];
 ci->window = MI_WINDOW(mi);

 ci->XMAX = ci->YMAX = 30;
 ci->viewer[2] = 14;
 ci->lightpos[0] = 7;
 ci->lightpos[1] = 7;
 ci->lightpos[2] = 15;
 ci->lightpos[3] = 1;

 ci->grid_col[1] = 0.25;
 ci->grid_col[2] = 0.05;
 ci->grid_col2[1] = 0.125;
 ci->grid_col2[2] = 0.05;

 if (maxparts >= MAX_COMPONENTS)
   maxparts = MAX_COMPONENTS-1;

 if ((ci->glx_context = init_GL(mi)) != NULL) {
      reshape_circuit(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
 } else {
     MI_CLEARWINDOW(mi);
 }
 if (uselight == 0)
    ci->light = 1;
 glShadeModel(GL_SMOOTH);
 glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
 glEnable(GL_DEPTH_TEST);
 glEnable(GL_LIGHTING);
 glEnable(GL_LIGHT0);
 glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
 make_tables(ci);
 makebandlist(ci);
 
}

ENTRYPOINT void draw_circuit(ModeInfo *mi)
{
  Circuit *ci = &circuit[MI_SCREEN(mi)];
  Window w = MI_WINDOW(mi);
  Display *disp = MI_DISPLAY(mi);

  if (!ci->glx_context)
      return;

 glXMakeCurrent(disp, w, *(ci->glx_context));

  display(mi);

  if(mi->fps_p) do_fps(mi);
  glFinish(); 
  glXSwapBuffers(disp, w);
}

ENTRYPOINT void release_circuit(ModeInfo *mi)
{
  if (circuit != NULL) {
   (void) free((void *) circuit);
   circuit = NULL;
  }
  FreeAllGL(mi);
}

XSCREENSAVER_MODULE ("Circuit", circuit)

#endif
