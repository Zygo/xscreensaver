/* -*- Mode: C; tab-width: 4 -*- */
/* noof --- SGI Diatoms */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)noof.c	5.04 2002/02/10 xlockmore";

#endif

/*-
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
 * Revision History:
 * 10-Feb-2001: From the GLUT demo distribution by Mark Kilgard
 *              <mjk@nvidia.com> ported by Dave Riesz <driesz@cpl.net>
 */

#ifdef VMS
#include <X11/Intrinsic.h>
#endif

#ifdef STANDALONE
# define MODE_noof
# define PROGCLASS					"Noof"
# define HACK_INIT					init_noof
# define HACK_DRAW					draw_noof
# define HACK_RESHAPE					reshape_noof
# define noof_opts					xlockmore_opts
# define DEFAULTS			"*delay:		1000   \n"
# include "xlockmore.h"				/* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"				/* from the xlockmore distribution */
# include "visgl.h"
#endif /* !STANDALONE */

#ifdef MODE_noof

#define MINSIZE         32      /* minimal viewport size */

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

ModeSpecOpt noof_opts =
{0, (XrmOptionDescRec *) NULL, 0, (argtype *) NULL, (OptionStruct *) NULL};

#ifdef USE_MODULES
ModStruct   noof_description =
{"noof", "init_noof", "draw_noof", "release_noof",
 "draw_noof", "init_noof", (char *) NULL, &noof_opts,
 1000, 1, 1, 1, 64, 1.0, "",
 "Shows SGI Diatoms", 0, NULL};

#endif

#define N_SHAPES 7

typedef struct {
	GLXContext *glx_context;
	Window      window;
	float ht, wd;
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
	int tko;
} noofstruct;

static noofstruct *noof = (noofstruct *) NULL;

/* For portability... */
#undef fcos
#undef fsin
#define fcos  cos
#define fsin  sin

/* function declarations */
static void oneFrame(noofstruct *np);
static void motionUpdate(noofstruct *np, int t);
static void colorUpdate(noofstruct *np, int i);
static void gravity(noofstruct *np, float fx);
static void drawleaf(noofstruct *np, int l);
static void initshapes(noofstruct *np, int i);

/* new window size or exposure */
static void
reshape_noof(ModeInfo *mi, int width, int height)
{
	noofstruct *np;

	if(noof == NULL) return;
	np = &noof[MI_SCREEN(mi)];

	if(!np->glx_context) return;

	glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(np->glx_context));

/* from myinit */
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glEnable(GL_LINE_SMOOTH);
	glShadeModel(GL_FLAT);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	if (width <= height) {
		np->wd = 1.0;
		np->ht = (GLfloat) height / (GLfloat) width;
		glOrtho(0.0, 1.0,
			0.0, 1.0 * (GLfloat) height / (GLfloat) width,
			-16.0, 4.0);
	} else {
		np->wd = (GLfloat) width / (GLfloat) height;
		np->ht = 1.0;
		glOrtho(0.0, 1.0 * (GLfloat) width / (GLfloat) height,
			0.0, 1.0,
			-16.0, 4.0);
	}
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glClear(GL_COLOR_BUFFER_BIT);
	glXSwapBuffers(MI_DISPLAY(mi), MI_WINDOW(mi));
}

static void
pinit(ModeInfo * mi)
{
	int i;
	noofstruct *np;

	if(noof == NULL) return;
	np = &noof[MI_SCREEN(mi)];

	for (i = 0; i < N_SHAPES; i++)
		initshapes(np, i);
	np->tko = 0;
}

void
init_noof(ModeInfo * mi)
{
	noofstruct *np;

	if (noof == NULL) {
		if ((noof = (noofstruct *) calloc(MI_NUM_SCREENS(mi),
					       sizeof (noofstruct))) == NULL)
			return;
	}
	np = &noof[MI_SCREEN(mi)];

	if ((np->glx_context = init_GL(mi)) != NULL)
	{
		reshape_noof(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
		glDrawBuffer(GL_FRONT);
		pinit(mi);
	}
	else
	{
		MI_CLEARWINDOW(mi);
	}


}

void
draw_noof(ModeInfo * mi)
{
	noofstruct *np;

	if(noof == NULL) return;
	np = &noof[MI_SCREEN(mi)];

	MI_IS_DRAWN(mi) = True;
	if(!np->glx_context) return;

	glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(np->glx_context));

	oneFrame(np);
}

void
release_noof(ModeInfo * mi)
{
	if (noof != NULL) {
		int screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			noofstruct *np = &noof[screen];

			if (np->glx_context) {
				np->glx_context = (GLXContext *) NULL;
			}
		}
		free(noof);
		noof = (noofstruct *) NULL;
	}
	FreeAllGL(mi);
}

/******************************************************************************/

/* --- shape parameters def'n --- */

static void
initshapes(noofstruct *np, int i)
{
  int k;
  float f;

  /* random init of pos, dir, color */
  for (k = i * 3; k <= i * 3 + 2; k++) {
    f = ((float) LRAND() / (float) MAXRAND);
    np->pos[k] = f;
    f = ((float) LRAND() / (float) MAXRAND);
    f = (f - 0.5) * 0.05;
    np->dir[k] = f;
    f = ((float) LRAND() / (float) MAXRAND);
    f = (f - 0.5) * 0.0002;
    np->acc[k] = f;
    f = ((float) LRAND() / (float) MAXRAND);
    np->col[k] = f;
  }

  np->speedsq[i] = np->dir[i * 3] * np->dir[i * 3] + np->dir[i * 3 + 1] * np->dir[i * 3 + 1];
  f = ((float) LRAND() / (float) MAXRAND);
  np->blad[i] = 2 + (int) (f * 17.0);
  f = ((float) LRAND() / (float) MAXRAND);
  np->ang[i] = f;
  f = ((float) LRAND() / (float) MAXRAND);
  np->spn[i] = (f - 0.5) * 40.0 / (10 + np->blad[i]);
  f = ((float) LRAND() / (float) MAXRAND);
  np->sca[i] = (f * 0.1 + 0.08);
  np->dir[i * 3] *= np->sca[i];
  np->dir[i * 3 + 1] *= np->sca[i];

  f = ((float) LRAND() / (float) MAXRAND);
  np->hsv[i * 3] = f * 360.0;

  f = ((float) LRAND() / (float) MAXRAND);
  np->hsv[i * 3 + 1] = f * 0.6 + 0.4;

  f = ((float) LRAND() / (float) MAXRAND);
  np->hsv[i * 3 + 2] = f * 0.7 + 0.3;

  f = ((float) LRAND() / (float) MAXRAND);
  np->hpr[i * 3] = f * 0.005 * 360.0;
  f = ((float) LRAND() / (float) MAXRAND);
  np->hpr[i * 3 + 1] = f * 0.03;
  f = ((float) LRAND() / (float) MAXRAND);
  np->hpr[i * 3 + 2] = f * 0.02;

  np->geep[i] = 0;
  f = ((float) LRAND() / (float) MAXRAND);
  np->peep[i] = 0.01 + f * 0.2;
}

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
drawleaf(noofstruct *np, int l)
{

  int b, blades;
  float x, y;
  float wobble;

  blades = np->blad[l];

  y = 0.10 * fsin(np->geep[l] * M_PI / 180.0) + 0.099 * fsin(np->geep[l] * 5.12 * M_PI / 180.0);
  if (y < 0)
    y = -y;
  x = 0.15 * fcos(np->geep[l] * M_PI / 180.0) + 0.149 * fcos(np->geep[l] * 5.12 * M_PI / 180.0);
  if (x < 0.0)
    x = 0.0 - x;
  if (y < 0.001 && x > 0.000002 && ((np->tko & 0x1) == 0)) {
    initshapes(np, l);      /* let it become reborn as something
                           else */
    np->tko++;
    return;
  } {
    float w1 = fsin(np->geep[l] * 15.3 * M_PI / 180.0);
    wobble = 3.0 + 2.00 * fsin(np->geep[l] * 0.4 * M_PI / 180.0) + 3.94261 * w1;
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
    glTranslatef(np->pos[l * 3], np->pos[l * 3 + 1], np->pos[l * 3 + 2]);
    glRotatef(np->ang[l] + b * (360.0 / blades), 0.0, 0.0, 1.0);
    glScalef(wobble * np->sca[l], wobble * np->sca[l], wobble * np->sca[l]);
    /**
    if(np->tko & 0x40000) glColor3f(np->col[l*3], np->col[l*3+1], np->col[l*3+2]);
    else
    */
    glColor4ub(0, 0, 0, 0x60);

    /* constrain geep cooridinates here XXX */
    glEnable(GL_BLEND);

    glBegin(GL_TRIANGLE_STRIP);
    glVertex2f(x * np->sca[l], 0.0);
    glVertex2f(x, y);
    glVertex2f(x, -y);  /* C */
    glVertex2f(0.3, 0.0);  /* D */
    glEnd();

    /**
    if(np->tko++ & 0x40000) glColor3f(0,0,0);
    else
    */
    glColor3f(np->col[l * 3], np->col[l * 3 + 1], np->col[l * 3 + 2]);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x * np->sca[l], 0.0);
    glVertex2f(x, y);
    glVertex2f(0.3, 0.0);  /* D */
    glVertex2f(x, -y);  /* C */
    glEnd();
    glDisable(GL_BLEND);

    glPopMatrix();
  }
}

static void
motionUpdate(noofstruct *np, int t)
{
  if (np->pos[t * 3] < -np->sca[t] * np->wd && np->dir[t * 3] < 0.0) {
    np->dir[t * 3] = -np->dir[t * 3];
  /**
  np->acc[t*3+1] += 0.8*np->acc[t*3];
  np->acc[t*3] = -0.8*np->acc[t*3];
  */
  } else if (np->pos[t * 3] > (1 + np->sca[t]) * np->wd && np->dir[t * 3] > 0.0) {
    np->dir[t * 3] = -np->dir[t * 3];
    /**
    np->acc[t*3+1] += 0.8*np->acc[t*3];
    np->acc[t*3] = -0.8*np->acc[t*3];
    */
  } else if (np->pos[t * 3 + 1] < -np->sca[t] * np->ht && np->dir[t * 3 + 1] < 0.0) {
    np->dir[t * 3 + 1] = -np->dir[t * 3 + 1];
    /**
    np->acc[t*3] += 0.8*np->acc[t*3+1];
    np->acc[t*3+1] = -0.8*np->acc[t*3+1];
    */
  } else if (np->pos[t * 3 + 1] > (1 + np->sca[t]) * np->ht && np->dir[t * 3 + 1] > 0.0) {
    np->dir[t * 3 + 1] = -np->dir[t * 3 + 1];
    /**
    np->acc[t*3] += 0.8*np->acc[t*3+1];
    np->acc[t*3+1] = -0.8*np->acc[t*3+1];
    */
  }

  np->pos[t * 3] += np->dir[t * 3];
  np->pos[t * 3 + 1] += np->dir[t * 3 + 1];
  /**
  np->dir[t*3]   += np->acc[t*3];
  np->dir[t*3+1] += np->acc[t*3+1];
  */
  np->ang[t] += np->spn[t];
  np->geep[t] += np->peep[t];
  if (np->geep[t] > 360 * 5.0)
    np->geep[t] -= 360 * 5.0;
  if (np->ang[t] < 0.0) {
    np->ang[t] += 360.0;
  }
  if (np->ang[t] > 360.0) {
    np->ang[t] -= 360.0;
  }
}

static void
colorUpdate(noofstruct *np, int i)
{
  if (np->hsv[i * 3 + 1] <= 0.5 && np->hpr[i * 3 + 1] < 0.0)
    np->hpr[i * 3 + 1] = -np->hpr[i * 3 + 1];  /* adjust s */
  if (np->hsv[i * 3 + 1] >= 1.0 && np->hpr[i * 3 + 1] > 0.0)
    np->hpr[i * 3 + 1] = -np->hpr[i * 3 + 1];  /* adjust s */
  if (np->hsv[i * 3 + 2] <= 0.4 && np->hpr[i * 3 + 2] < 0.0)
    np->hpr[i * 3 + 2] = -np->hpr[i * 3 + 2];  /* adjust s */
  if (np->hsv[i * 3 + 2] >= 1.0 && np->hpr[i * 3 + 2] > 0.0)
    np->hpr[i * 3 + 2] = -np->hpr[i * 3 + 2];  /* adjust s */

  np->hsv[i * 3] += np->hpr[i * 3];
  np->hsv[i * 3 + 1] += np->hpr[i * 3 + 1];
  np->hsv[i * 3 + 2] += np->hpr[i * 3 + 2];

  /* --- hsv -> rgb --- */
#define H(hhh) hhh[i*3  ]
#define S(hhh) hhh[i*3+1]
#define V(hhh) hhh[i*3+2]

#define R(hhh) hhh[i*3  ]
#define G(hhh) hhh[i*3+1]
#define B(hhh) hhh[i*3+2]

  if (V(np->hsv) < 0.0)
    V(np->hsv) = 0.0;
  if (V(np->hsv) > 1.0)
    V(np->hsv) = 1.0;
  if (S(np->hsv) <= 0.0) {
    R(np->col) = V(np->hsv);
    G(np->col) = V(np->hsv);
    B(np->col) = V(np->hsv);
  } else {
    float f, h, p, q, t, v;
    int hi;

    while (H(np->hsv) < 0.0)
      H(np->hsv) += 360.0;
    while (H(np->hsv) >= 360.0)
      H(np->hsv) -= 360.0;

    if (S(np->hsv) < 0.0)
      S(np->hsv) = 0.0;
    if (S(np->hsv) > 1.0)
      S(np->hsv) = 1.0;

    h = H(np->hsv) / 60.0;
    hi = (int) (h);
    f = h - hi;
    v = V(np->hsv);
    p = V(np->hsv) * (1 - S(np->hsv));
    q = V(np->hsv) * (1 - S(np->hsv) * f);
    t = V(np->hsv) * (1 - S(np->hsv) * (1 - f));

    if (hi <= 0) {
      R(np->col) = v;
      G(np->col) = t;
      B(np->col) = p;
    } else if (hi == 1) {
      R(np->col) = q;
      G(np->col) = v;
      B(np->col) = p;
    } else if (hi == 2) {
      R(np->col) = p;
      G(np->col) = v;
      B(np->col) = t;
    } else if (hi == 3) {
      R(np->col) = p;
      G(np->col) = q;
      B(np->col) = v;
    } else if (hi == 4) {
      R(np->col) = t;
      G(np->col) = p;
      B(np->col) = v;
    } else {
      R(np->col) = v;
      G(np->col) = p;
      B(np->col) = q;
    }
  }
}

static void
gravity(noofstruct *np, float fx)
{
  int a, b;

  for (a = 0; a < N_SHAPES; a++) {
    for (b = 0; b < a; b++) {
      float t, d2;

      t = np->pos[b * 3] - np->pos[a * 3];
      d2 = t * t;
      t = np->pos[b * 3 + 1] - np->pos[a * 3 + 1];
      d2 += t * t;
      if (d2 < 0.000001)
        d2 = 0.00001;
      if (d2 < 0.1) {

        float v0, v1, z;
        v0 = np->pos[b * 3] - np->pos[a * 3];
        v1 = np->pos[b * 3 + 1] - np->pos[a * 3 + 1];

        z = 0.00000001 * fx / (d2);

        np->dir[a * 3] += v0 * z * np->sca[b];
        np->dir[b * 3] += -v0 * z * np->sca[a];
        np->dir[a * 3 + 1] += v1 * z * np->sca[b];
        np->dir[b * 3 + 1] += -v1 * z * np->sca[a];

      }
    }
    /** apply brakes
    if(np->dir[a*3]*np->dir[a*3] + np->dir[a*3+1]*np->dir[a*3+1]
      > 0.0001) {
      np->dir[a*3] *= 0.9;
      np->dir[a*3+1] *= 0.9;
    }
    */
  }
}

static void
oneFrame(noofstruct *np)
{
  int i;

  /**
  if((LRAND() & 0xff) == 0x34){
    glClear(GL_COLOR_BUFFER_BIT);
  }

  if((np->tko & 0x1f) == 0x1f){
    glEnable(GL_BLEND);
    glColor4f(0.0, 0.0, 0.0, 0.09);
    glRectf(0.0, 0.0, np->wd, np->ht);
    glDisable(GL_BLEND);
#ifdef __sgi
    sginap(0);
#endif
  }
  */
  gravity(np, -2.0);
  for (i = 0; i < N_SHAPES; i++) {
    motionUpdate(np, i);
#ifdef __sgi
    sginap(0);
#endif
    colorUpdate(np, i);
#ifdef __sgi
    sginap(0);
#endif
    drawleaf(np, i);
#ifdef __sgi
    sginap(0);
#endif

  }
  glFlush();
}

#endif
