/* noof, Copyright (c) 2004 Mark Kilgard <mjk@nvidia.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Originally a demo included with GLUT;
 * (Apparently this was called "diatoms" on Irix.)
 * ported to raw GL and xscreensaver by jwz, 12-Feb-2004.
 */

#include <X11/Intrinsic.h>

extern XtAppContext app;

#define PROGCLASS	"Noof"
#define HACK_INIT	init_noof
#define HACK_DRAW	draw_noof
#define HACK_RESHAPE	reshape_noof
#define noof_opts	xlockmore_opts

#define DEFAULTS	"*delay:	10000       \n" \
			"*showFPS:      False       \n" \
			"*fpsSolid:     True        \n" \

#include "xlockmore.h"

#ifdef USE_GL /* whole file */

typedef struct {
  GLXContext *glx_context;
} noof_configuration;

static noof_configuration *bps = NULL;
ModeSpecOpt noof_opts = {0, NULL, 0, NULL, NULL};


/* --- shape parameters def'n --- */
#define N_SHAPES 7
static float pos[N_SHAPES * 3];
static float dir[N_SHAPES * 3];
static float acc[N_SHAPES * 3];
static float col[N_SHAPES * 3];
static float hsv[N_SHAPES * 3];
static float hpr[N_SHAPES * 3];
static float ang[N_SHAPES];
static float spn[N_SHAPES];
static float sca[N_SHAPES];
static float geep[N_SHAPES];
static float peep[N_SHAPES];
static float speedsq[N_SHAPES];
static int blad[N_SHAPES];

static float ht, wd;

static void
initshapes(int i)
{
  int k;
  float f;

  /* random init of pos, dir, color */
  for (k = i * 3; k <= i * 3 + 2; k++) {
    f = random() / (double) RAND_MAX;
    pos[k] = f;
    f = random() / (double) RAND_MAX;
    f = (f - 0.5) * 0.05;
    dir[k] = f;
    f = random() / (double) RAND_MAX;
    f = (f - 0.5) * 0.0002;
    acc[k] = f;
    f = random() / (double) RAND_MAX;
    col[k] = f;
  }

  speedsq[i] = dir[i * 3] * dir[i * 3] + dir[i * 3 + 1] * dir[i * 3 + 1];
  f = random() / (double) RAND_MAX;
  blad[i] = 2 + (int) (f * 17.0);
  f = random() / (double) RAND_MAX;
  ang[i] = f;
  f = random() / (double) RAND_MAX;
  spn[i] = (f - 0.5) * 40.0 / (10 + blad[i]);
  f = random() / (double) RAND_MAX;
  sca[i] = (f * 0.1 + 0.08);
  dir[i * 3] *= sca[i];
  dir[i * 3 + 1] *= sca[i];

  f = random() / (double) RAND_MAX;
  hsv[i * 3] = f * 360.0;

  f = random() / (double) RAND_MAX;
  hsv[i * 3 + 1] = f * 0.6 + 0.4;

  f = random() / (double) RAND_MAX;
  hsv[i * 3 + 2] = f * 0.7 + 0.3;

  f = random() / (double) RAND_MAX;
  hpr[i * 3] = f * 0.005 * 360.0;
  f = random() / (double) RAND_MAX;
  hpr[i * 3 + 1] = f * 0.03;
  f = random() / (double) RAND_MAX;
  hpr[i * 3 + 2] = f * 0.02;

  geep[i] = 0;
  f = random() / (double) RAND_MAX;
  peep[i] = 0.01 + f * 0.2;
}

static int tko = 0;

static float bladeratio[] =
{
  /* nblades = 2..7 */
  0.0, 0.0, 3.00000, 1.73205, 1.00000, 0.72654, 0.57735, 0.48157,
  /* 8..13 */
  0.41421, 0.36397, 0.19076, 0.29363, 0.26795, 0.24648,
  /* 14..19 */
  0.22824, 0.21256, 0.19891, 0.18693, 0.17633, 0.16687,
};

static void
drawleaf(int l)
{

  int b, blades;
  float x, y;
  float wobble;

  blades = blad[l];

  y = 0.10 * sin(geep[l] * M_PI / 180.0) + 0.099 * sin(geep[l] * 5.12 * M_PI / 180.0);
  if (y < 0)
    y = -y;
  x = 0.15 * cos(geep[l] * M_PI / 180.0) + 0.149 * cos(geep[l] * 5.12 * M_PI / 180.0);
  if (x < 0.0)
    x = 0.0 - x;
  if (y < 0.001 && x > 0.000002 && ((tko & 0x1) == 0)) {
    initshapes(l);      /* let it become reborn as something
                           else */
    tko++;
    return;
  } {
    float w1 = sin(geep[l] * 15.3 * M_PI / 180.0);
    wobble = 3.0 + 2.00 * sin(geep[l] * 0.4 * M_PI / 180.0) + 3.94261 * w1;
  }

  /**
  if(blades == 2) if (y > 3.000*x) y = x*3.000;
  if(blades == 3) if (y > 1.732*x) y = x*1.732;
  if(blades == 4) if (y >       x) y = x;
  if(blades == 5) if (y > 0.726*x) y = x*0.726;
  if(blades == 6) if (y > 0.577*x) y = x*0.577;
  if(blades == 7) if (y > 0.481*x) y = x*0.481;
  if(blades == 8) if (y > 0.414*x) y = x*0.414;
  */
  if (y > x * bladeratio[blades])
    y = x * bladeratio[blades];

  for (b = 0; b < blades; b++) {
    glPushMatrix();
    glTranslatef(pos[l * 3], pos[l * 3 + 1], pos[l * 3 + 2]);
    glRotatef(ang[l] + b * (360.0 / blades), 0.0, 0.0, 1.0);
    glScalef(wobble * sca[l], wobble * sca[l], wobble * sca[l]);
    /**
    if(tko & 0x40000) glColor3f(col[l*3], col[l*3+1], col[l*3+2]); 
    else
    */
    glColor4ub(0, 0, 0, 0x60);

    /* constrain geep cooridinates here XXX */
    glEnable(GL_BLEND);

    glBegin(GL_TRIANGLE_STRIP);
    glVertex2f(x * sca[l], 0.0);
    glVertex2f(x, y);
    glVertex2f(x, -y);  /* C */
    glVertex2f(0.3, 0.0);  /* D */
    glEnd();

    /**
    if(tko++ & 0x40000) glColor3f(0,0,0);
    else
    */
    glColor3f(col[l * 3], col[l * 3 + 1], col[l * 3 + 2]);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x * sca[l], 0.0);
    glVertex2f(x, y);
    glVertex2f(0.3, 0.0);  /* D */
    glVertex2f(x, -y);  /* C */
    glEnd();
    glDisable(GL_BLEND);

    glPopMatrix();
  }
}

static void
motionUpdate(int t)
{
  if (pos[t * 3] < -sca[t] * wd && dir[t * 3] < 0.0) {
    dir[t * 3] = -dir[t * 3];
  /**
  acc[t*3+1] += 0.8*acc[t*3];
  acc[t*3] = -0.8*acc[t*3];
  */
  } else if (pos[t * 3] > (1 + sca[t]) * wd && dir[t * 3] > 0.0) {
    dir[t * 3] = -dir[t * 3];
    /**
    acc[t*3+1] += 0.8*acc[t*3];
    acc[t*3] = -0.8*acc[t*3];
    */
  } else if (pos[t * 3 + 1] < -sca[t] * ht && dir[t * 3 + 1] < 0.0) {
    dir[t * 3 + 1] = -dir[t * 3 + 1];
    /**
    acc[t*3] += 0.8*acc[t*3+1];
    acc[t*3+1] = -0.8*acc[t*3+1];
    */
  } else if (pos[t * 3 + 1] > (1 + sca[t]) * ht && dir[t * 3 + 1] > 0.0) {
    dir[t * 3 + 1] = -dir[t * 3 + 1];
    /**
    acc[t*3] += 0.8*acc[t*3+1];
    acc[t*3+1] = -0.8*acc[t*3+1];
    */
  }

  pos[t * 3] += dir[t * 3];
  pos[t * 3 + 1] += dir[t * 3 + 1];
  /**
  dir[t*3]   += acc[t*3];
  dir[t*3+1] += acc[t*3+1];
  */
  ang[t] += spn[t];
  geep[t] += peep[t];
  if (geep[t] > 360 * 5.0)
    geep[t] -= 360 * 5.0;
  if (ang[t] < 0.0) {
    ang[t] += 360.0;
  }
  if (ang[t] > 360.0) {
    ang[t] -= 360.0;
  }
}

static void
colorUpdate(int i)
{
  if (hsv[i * 3 + 1] <= 0.5 && hpr[i * 3 + 1] < 0.0)
    hpr[i * 3 + 1] = -hpr[i * 3 + 1];  /* adjust s */
  if (hsv[i * 3 + 1] >= 1.0 && hpr[i * 3 + 1] > 0.0)
    hpr[i * 3 + 1] = -hpr[i * 3 + 1];  /* adjust s */
  if (hsv[i * 3 + 2] <= 0.4 && hpr[i * 3 + 2] < 0.0)
    hpr[i * 3 + 2] = -hpr[i * 3 + 2];  /* adjust s */
  if (hsv[i * 3 + 2] >= 1.0 && hpr[i * 3 + 2] > 0.0)
    hpr[i * 3 + 2] = -hpr[i * 3 + 2];  /* adjust s */

  hsv[i * 3] += hpr[i * 3];
  hsv[i * 3 + 1] += hpr[i * 3 + 1];
  hsv[i * 3 + 2] += hpr[i * 3 + 2];

  /* --- hsv -> rgb --- */
#define H(hhh) hhh[i*3  ]
#define S(hhh) hhh[i*3+1]
#define V(hhh) hhh[i*3+2]

#define R(hhh) hhh[i*3  ]
#define G(hhh) hhh[i*3+1]
#define B(hhh) hhh[i*3+2]

  if (V(hsv) < 0.0)
    V(hsv) = 0.0;
  if (V(hsv) > 1.0)
    V(hsv) = 1.0;
  if (S(hsv) <= 0.0) {
    R(col) = V(hsv);
    G(col) = V(hsv);
    B(col) = V(hsv);
  } else {
    float f, h, p, q, t, v;
    int hi;

    while (H(hsv) < 0.0)
      H(hsv) += 360.0;
    while (H(hsv) >= 360.0)
      H(hsv) -= 360.0;

    if (S(hsv) < 0.0)
      S(hsv) = 0.0;
    if (S(hsv) > 1.0)
      S(hsv) = 1.0;

    h = H(hsv) / 60.0;
    hi = (int) (h);
    f = h - hi;
    v = V(hsv);
    p = V(hsv) * (1 - S(hsv));
    q = V(hsv) * (1 - S(hsv) * f);
    t = V(hsv) * (1 - S(hsv) * (1 - f));

    if (hi <= 0) {
      R(col) = v;
      G(col) = t;
      B(col) = p;
    } else if (hi == 1) {
      R(col) = q;
      G(col) = v;
      B(col) = p;
    } else if (hi == 2) {
      R(col) = p;
      G(col) = v;
      B(col) = t;
    } else if (hi == 3) {
      R(col) = p;
      G(col) = q;
      B(col) = v;
    } else if (hi == 4) {
      R(col) = t;
      G(col) = p;
      B(col) = v;
    } else {
      R(col) = v;
      G(col) = p;
      B(col) = q;
    }
  }
}

static void
gravity(float fx)
{
  int a, b;

  for (a = 0; a < N_SHAPES; a++) {
    for (b = 0; b < a; b++) {
      float t, d2;

      t = pos[b * 3] - pos[a * 3];
      d2 = t * t;
      t = pos[b * 3 + 1] - pos[a * 3 + 1];
      d2 += t * t;
      if (d2 < 0.000001)
        d2 = 0.00001;
      if (d2 < 0.1) {

        float v0, v1, z;
        v0 = pos[b * 3] - pos[a * 3];
        v1 = pos[b * 3 + 1] - pos[a * 3 + 1];

        z = 0.00000001 * fx / (d2);

        dir[a * 3] += v0 * z * sca[b];
        dir[b * 3] += -v0 * z * sca[a];
        dir[a * 3 + 1] += v1 * z * sca[b];
        dir[b * 3 + 1] += -v1 * z * sca[a];

      }
    }
    /** apply brakes
    if(dir[a*3]*dir[a*3] + dir[a*3+1]*dir[a*3+1]
      > 0.0001) {
      dir[a*3] *= 0.9;
      dir[a*3+1] *= 0.9;
    }
    */
  }
}

void
draw_noof (ModeInfo *mi)
{
  int i;

  /**
  if((random() & 0xff) == 0x34){
    glClear(GL_COLOR_BUFFER_BIT);
  }

  if((tko & 0x1f) == 0x1f){
    glEnable(GL_BLEND);
    glColor4f(0.0, 0.0, 0.0, 0.09);
    glRectf(0.0, 0.0, wd, ht);
    glDisable(GL_BLEND);
#ifdef __sgi
    sginap(0);
#endif
  }
  */

  gravity(-2.0);
  for (i = 0; i < N_SHAPES; i++) {
    motionUpdate(i);
    colorUpdate(i);
    drawleaf(i);
  }

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(MI_DISPLAY(mi), MI_WINDOW(mi));
}


void
reshape_noof(ModeInfo *mi, int w, int h)
{
  glViewport(0, 0, w, h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  if (w <= h) {
    wd = 1.0;
    ht = (GLfloat) h / (GLfloat) w;
    glOrtho(0.0, 1.0,
      0.0, 1.0 * (GLfloat) h / (GLfloat) w,
      -16.0, 4.0);
  } else {
    wd = (GLfloat) w / (GLfloat) h;
    ht = 1.0;
    glOrtho(0.0, 1.0 * (GLfloat) w / (GLfloat) h,
      0.0, 1.0,
      -16.0, 4.0);
  }
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void 
init_noof (ModeInfo *mi)
{
  int i;
  noof_configuration *bp;

  if (!bps) {
    bps = (noof_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (noof_configuration));
    if (!bps) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }
    bp = &bps[MI_SCREEN(mi)];
  }

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  glClearColor(0.0, 0.0, 0.0, 1.0);
  glEnable(GL_LINE_SMOOTH);
  glShadeModel(GL_FLAT);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  for (i = 0; i < N_SHAPES; i++)
    initshapes(i);
  reshape_noof (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
}


#endif /* USE_GL */
