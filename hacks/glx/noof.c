/* noof, Copyright (c) 2004 Bill Torzewski <billt@worksitez.com>
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

#define DEFAULTS	"*delay:	10000       \n" \
			"*showFPS:      False       \n" \
			"*fpsSolid:     True        \n" \
			"*doubleBuffer: False       \n" \

# define refresh_noof 0
# define release_noof 0
# define noof_handle_event 0
#include "xlockmore.h"

#ifdef USE_GL /* whole file */

#define N_SHAPES 7

/* For some reason this hack screws up on Cocoa if we try to double-buffer it.
   It looks fine single-buffered, so let's just do that. */
static int dbuf_p = 0;

ENTRYPOINT ModeSpecOpt noof_opts = {0, NULL, 0, NULL, NULL};

typedef struct {
  GLXContext *glx_context;

  float pos[N_SHAPES * 3];
  float dir[N_SHAPES * 3];
  float acc[N_SHAPES * 3];
  float col[N_SHAPES * 3];
  float hsv[N_SHAPES * 3];
  float hpr[N_SHAPES * 3];
  float ang[N_SHAPES];
  float spn[N_SHAPES];
  float sca[N_SHAPES];
  float geep[N_SHAPES];
  float peep[N_SHAPES];
  float speedsq[N_SHAPES];
  int blad[N_SHAPES];

  float ht, wd;

  int tko;

} noof_configuration;

static noof_configuration *bps = NULL;


static void
initshapes(noof_configuration *bp, int i)
{
  int k;
  float f;

  /* random init of pos, dir, color */
  for (k = i * 3; k <= i * 3 + 2; k++) {
    f = random() / (double) RAND_MAX;
    bp->pos[k] = f;
    f = random() / (double) RAND_MAX;
    f = (f - 0.5) * 0.05;
    bp->dir[k] = f;
    f = random() / (double) RAND_MAX;
    f = (f - 0.5) * 0.0002;
    bp->acc[k] = f;
    f = random() / (double) RAND_MAX;
    bp->col[k] = f;
  }

  bp->speedsq[i] = bp->dir[i * 3] * bp->dir[i * 3] + bp->dir[i * 3 + 1] * bp->dir[i * 3 + 1];
  f = random() / (double) RAND_MAX;
  bp->blad[i] = 2 + (int) (f * 17.0);
  f = random() / (double) RAND_MAX;
  bp->ang[i] = f;
  f = random() / (double) RAND_MAX;
  bp->spn[i] = (f - 0.5) * 40.0 / (10 + bp->blad[i]);
  f = random() / (double) RAND_MAX;
  bp->sca[i] = (f * 0.1 + 0.08);
  bp->dir[i * 3] *= bp->sca[i];
  bp->dir[i * 3 + 1] *= bp->sca[i];

  f = random() / (double) RAND_MAX;
  bp->hsv[i * 3] = f * 360.0;

  f = random() / (double) RAND_MAX;
  bp->hsv[i * 3 + 1] = f * 0.6 + 0.4;

  f = random() / (double) RAND_MAX;
  bp->hsv[i * 3 + 2] = f * 0.7 + 0.3;

  f = random() / (double) RAND_MAX;
  bp->hpr[i * 3] = f * 0.005 * 360.0;
  f = random() / (double) RAND_MAX;
  bp->hpr[i * 3 + 1] = f * 0.03;
  f = random() / (double) RAND_MAX;
  bp->hpr[i * 3 + 2] = f * 0.02;

  bp->geep[i] = 0;
  f = random() / (double) RAND_MAX;
  bp->peep[i] = 0.01 + f * 0.2;
}

static const float bladeratio[] =
{
  /* nblades = 2..7 */
  0.0, 0.0, 3.00000, 1.73205, 1.00000, 0.72654, 0.57735, 0.48157,
  /* 8..13 */
  0.41421, 0.36397, 0.19076, 0.29363, 0.26795, 0.24648,
  /* 14..19 */
  0.22824, 0.21256, 0.19891, 0.18693, 0.17633, 0.16687,
};

static int
drawleaf(noof_configuration *bp, int l)
{
  int polys = 0;
  int b, blades;
  float x, y;
  float wobble;

  blades = bp->blad[l];

  y = 0.10 * sin(bp->geep[l] * M_PI / 180.0) + 0.099 * sin(bp->geep[l] * 5.12 * M_PI / 180.0);
  if (y < 0)
    y = -y;
  x = 0.15 * cos(bp->geep[l] * M_PI / 180.0) + 0.149 * cos(bp->geep[l] * 5.12 * M_PI / 180.0);
  if (x < 0.0)
    x = 0.0 - x;
  if (y < 0.001 && x > 0.000002 && ((bp->tko & 0x1) == 0)) {
    initshapes(bp, l);      /* let it become reborn as something
                           else */
    bp->tko++;
    return polys;
  } {
    float w1 = sin(bp->geep[l] * 15.3 * M_PI / 180.0);
    wobble = 3.0 + 2.00 * sin(bp->geep[l] * 0.4 * M_PI / 180.0) + 3.94261 * w1;
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
    glTranslatef(bp->pos[l * 3], bp->pos[l * 3 + 1], bp->pos[l * 3 + 2]);
    glRotatef(bp->ang[l] + b * (360.0 / blades), 0.0, 0.0, 1.0);
    glScalef(wobble * bp->sca[l], wobble * bp->sca[l], wobble * bp->sca[l]);
    /**
    if(tko & 0x40000) glColor3f(col[l*3], col[l*3+1], col[l*3+2]); 
    else
    */
    glColor4ub(0, 0, 0, 0x60);

    /* constrain geep cooridinates here XXX */
    glEnable(GL_BLEND);

    glBegin(GL_TRIANGLE_STRIP);
    glVertex2f(x * bp->sca[l], 0.0);
    glVertex2f(x, y);
    glVertex2f(x, -y);  /* C */
    glVertex2f(0.3, 0.0);  /* D */
    polys += 2;
    glEnd();

    /**
    if(tko++ & 0x40000) glColor3f(0,0,0);
    else
    */
    glColor3f(bp->col[l * 3], bp->col[l * 3 + 1], bp->col[l * 3 + 2]);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x * bp->sca[l], 0.0);
    glVertex2f(x, y);
    glVertex2f(0.3, 0.0);  /* D */
    glVertex2f(x, -y);  /* C */
    polys += 3;
    glEnd();
    glDisable(GL_BLEND);

    glPopMatrix();
  }
  return polys;
}

static void
motionUpdate(noof_configuration *bp, int t)
{
  if (bp->pos[t * 3] < -bp->sca[t] * bp->wd && bp->dir[t * 3] < 0.0) {
    bp->dir[t * 3] = -bp->dir[t * 3];
  /**
  acc[t*3+1] += 0.8*acc[t*3];
  acc[t*3] = -0.8*acc[t*3];
  */
  } else if (bp->pos[t * 3] > (1 + bp->sca[t]) * bp->wd && bp->dir[t * 3] > 0.0) {
    bp->dir[t * 3] = -bp->dir[t * 3];
    /**
    acc[t*3+1] += 0.8*acc[t*3];
    acc[t*3] = -0.8*acc[t*3];
    */
  } else if (bp->pos[t * 3 + 1] < -bp->sca[t] * bp->ht && bp->dir[t * 3 + 1] < 0.0) {
    bp->dir[t * 3 + 1] = -bp->dir[t * 3 + 1];
    /**
    acc[t*3] += 0.8*acc[t*3+1];
    acc[t*3+1] = -0.8*acc[t*3+1];
    */
  } else if (bp->pos[t * 3 + 1] > (1 + bp->sca[t]) * bp->ht && bp->dir[t * 3 + 1] > 0.0) {
    bp->dir[t * 3 + 1] = -bp->dir[t * 3 + 1];
    /**
    acc[t*3] += 0.8*acc[t*3+1];
    acc[t*3+1] = -0.8*acc[t*3+1];
    */
  }

  bp->pos[t * 3] += bp->dir[t * 3];
  bp->pos[t * 3 + 1] += bp->dir[t * 3 + 1];
  /**
  dir[t*3]   += acc[t*3];
  dir[t*3+1] += acc[t*3+1];
  */
  bp->ang[t] += bp->spn[t];
  bp->geep[t] += bp->peep[t];
  if (bp->geep[t] > 360 * 5.0)
    bp->geep[t] -= 360 * 5.0;
  if (bp->ang[t] < 0.0) {
    bp->ang[t] += 360.0;
  }
  if (bp->ang[t] > 360.0) {
    bp->ang[t] -= 360.0;
  }
}

static void
colorUpdate(noof_configuration *bp, int i)
{
  if (bp->hsv[i * 3 + 1] <= 0.5 && bp->hpr[i * 3 + 1] < 0.0)
    bp->hpr[i * 3 + 1] = -bp->hpr[i * 3 + 1];  /* adjust s */
  if (bp->hsv[i * 3 + 1] >= 1.0 && bp->hpr[i * 3 + 1] > 0.0)
    bp->hpr[i * 3 + 1] = -bp->hpr[i * 3 + 1];  /* adjust s */
  if (bp->hsv[i * 3 + 2] <= 0.4 && bp->hpr[i * 3 + 2] < 0.0)
    bp->hpr[i * 3 + 2] = -bp->hpr[i * 3 + 2];  /* adjust s */
  if (bp->hsv[i * 3 + 2] >= 1.0 && bp->hpr[i * 3 + 2] > 0.0)
    bp->hpr[i * 3 + 2] = -bp->hpr[i * 3 + 2];  /* adjust s */

  bp->hsv[i * 3] += bp->hpr[i * 3];
  bp->hsv[i * 3 + 1] += bp->hpr[i * 3 + 1];
  bp->hsv[i * 3 + 2] += bp->hpr[i * 3 + 2];

  /* --- hsv -> rgb --- */
#define H(hhh) hhh[i*3  ]
#define S(hhh) hhh[i*3+1]
#define V(hhh) hhh[i*3+2]

#define R(hhh) hhh[i*3  ]
#define G(hhh) hhh[i*3+1]
#define B(hhh) hhh[i*3+2]

  if (V(bp->hsv) < 0.0)
    V(bp->hsv) = 0.0;
  if (V(bp->hsv) > 1.0)
    V(bp->hsv) = 1.0;
  if (S(bp->hsv) <= 0.0) {
    R(bp->col) = V(bp->hsv);
    G(bp->col) = V(bp->hsv);
    B(bp->col) = V(bp->hsv);
  } else {
    float f, h, p, q, t, v;
    int hi;

    while (H(bp->hsv) < 0.0)
      H(bp->hsv) += 360.0;
    while (H(bp->hsv) >= 360.0)
      H(bp->hsv) -= 360.0;

    if (S(bp->hsv) < 0.0)
      S(bp->hsv) = 0.0;
    if (S(bp->hsv) > 1.0)
      S(bp->hsv) = 1.0;

    h = H(bp->hsv) / 60.0;
    hi = (int) (h);
    f = h - hi;
    v = V(bp->hsv);
    p = V(bp->hsv) * (1 - S(bp->hsv));
    q = V(bp->hsv) * (1 - S(bp->hsv) * f);
    t = V(bp->hsv) * (1 - S(bp->hsv) * (1 - f));

    if (hi <= 0) {
      R(bp->col) = v;
      G(bp->col) = t;
      B(bp->col) = p;
    } else if (hi == 1) {
      R(bp->col) = q;
      G(bp->col) = v;
      B(bp->col) = p;
    } else if (hi == 2) {
      R(bp->col) = p;
      G(bp->col) = v;
      B(bp->col) = t;
    } else if (hi == 3) {
      R(bp->col) = p;
      G(bp->col) = q;
      B(bp->col) = v;
    } else if (hi == 4) {
      R(bp->col) = t;
      G(bp->col) = p;
      B(bp->col) = v;
    } else {
      R(bp->col) = v;
      G(bp->col) = p;
      B(bp->col) = q;
    }
  }
}

static void
gravity(noof_configuration *bp, float fx)
{
  int a, b;

  for (a = 0; a < N_SHAPES; a++) {
    for (b = 0; b < a; b++) {
      float t, d2;

      t = bp->pos[b * 3] - bp->pos[a * 3];
      d2 = t * t;
      t = bp->pos[b * 3 + 1] - bp->pos[a * 3 + 1];
      d2 += t * t;
      if (d2 < 0.000001)
        d2 = 0.00001;
      if (d2 < 0.1) {

        float v0, v1, z;
        v0 = bp->pos[b * 3] - bp->pos[a * 3];
        v1 = bp->pos[b * 3 + 1] - bp->pos[a * 3 + 1];

        z = 0.00000001 * fx / (d2);

        bp->dir[a * 3] += v0 * z * bp->sca[b];
        bp->dir[b * 3] += -v0 * z * bp->sca[a];
        bp->dir[a * 3 + 1] += v1 * z * bp->sca[b];
        bp->dir[b * 3 + 1] += -v1 * z * bp->sca[a];

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

ENTRYPOINT void
draw_noof (ModeInfo *mi)
{
  int i;
  noof_configuration *bp = &bps[MI_SCREEN(mi)];

  if (!bp->glx_context)
    return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(bp->glx_context));
  mi->polygon_count = 0;

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

  gravity(bp, -2.0);
  for (i = 0; i < N_SHAPES; i++) {
    motionUpdate(bp, i);
    colorUpdate(bp, i);
      mi->polygon_count += drawleaf(bp, i);
  }

  if (mi->fps_p) do_fps (mi);
  glFinish();

  if (dbuf_p)
    glXSwapBuffers(MI_DISPLAY(mi), MI_WINDOW(mi));
}


ENTRYPOINT void
reshape_noof(ModeInfo *mi, int w, int h)
{
  noof_configuration *bp = &bps[MI_SCREEN(mi)];
  glViewport(0, 0, w, h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  if (w <= h) {
    bp->wd = 1.0;
    bp->ht = (GLfloat) h / (GLfloat) w;
    glOrtho(0.0, 1.0,
      0.0, 1.0 * (GLfloat) h / (GLfloat) w,
      -16.0, 4.0);
  } else {
    bp->wd = (GLfloat) w / (GLfloat) h;
    bp->ht = 1.0;
    glOrtho(0.0, 1.0 * (GLfloat) w / (GLfloat) h,
      0.0, 1.0,
      -16.0, 4.0);
  }
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

ENTRYPOINT void 
init_noof (ModeInfo *mi)
{
  int i;
  noof_configuration *bp;

#ifdef HAVE_JWZGLES
  dbuf_p = 1;
#endif

  if (!bps) {
    bps = (noof_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (noof_configuration));
    if (!bps) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }
  }

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  glDrawBuffer(dbuf_p ? GL_BACK : GL_FRONT);
  glEnable(GL_LINE_SMOOTH);
  glShadeModel(GL_FLAT);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  for (i = 0; i < N_SHAPES; i++)
    initshapes(bp, i);
  reshape_noof (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
}


XSCREENSAVER_MODULE ("Noof", noof)

#endif /* USE_GL */
