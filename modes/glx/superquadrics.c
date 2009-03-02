/* -*- Mode: C; tab-width: 4 -*- */
/* superquadrics --- 3D mathematical shapes */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)superquadrics.c	4.07 97/11/24 xlockmore";

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
 * Superquadrics were invented by Dr. Alan Barr of Caltech University.
 * They were first published in "Computer Graphics and Applications",
 * volume 1, number 1, 1981, in the article "Superquadrics and Angle-
 * Preserving Transformations."  Dr. Barr based the Superquadrics on
 * Piet Hein's "super ellipses."  Super ellipses are like 2D ellipses,
 * except that the formula includes an exponent, raising its X and Y
 * values to a (fractional) power, and allowing them to gradually
 * change from round to square edges.  Superquadrics extend this
 * idea into 3 dimensions, using two exponents to modify a
 * quadric surface in a similar fashion.
 *
 * Revision History:
 * 30-Mar-97: Turned into a module for xlockmore 4.02 alpha.  The code
 *    is almost unrecognizable now from the first revision, except for
 *    a few remaining two-letter variable names.  I still don't have
 *    the normal vectors working right (I wrote the buggy normal vector
 *    code myself, can you tell?)
 * 07-Jan-97: A legend reborn; Superquadrics make an appearance as a
 *    real OpenGL program written in C.  I can even render them with
 *    proper lighting and specular highlights.  Gee, they look almost
 *    as good now as the original color plates of them that my uncle
 *    showed me as a child in 1981.  I don't know what computer hardware
 *    he was using at the time, but it's taken a couple decades for the
 *    PC clone hardware to catch up to it.
 * 05-Jan-97: After almost a decade, Superquadrics had almost faded away
 *    into the myths and folklore of all the things my brother and I played
 *    with on computers when we were kids with too much time on our hands.
 *    I had since gotten involved in Unix, OpenGL, and other things.
 *    A sudden flash of inspiration caused me to dig out the old Pascal
 *    source code, run it through p2c, and start ripping away the old
 *    wireframe rendering code, to be replaced by OpenGL.
 * Late 1989 or early 1990:  Around this time I did the Turbo Pascal
 *    port of the Superquadrics.  Unfortunately, many of the original variable
 *    names remained the same from the C= 64 original.  This was unfortunate
 *    because BASIC on the c64 only allowed 2-letter, global variable names.
 *    But the speed improvement over BASIC was very impressive at the time.
 * Thanksgiving, 1987: Written.  My uncle Al, who invented Superquadrics some
 *    years earlier, came to visit us.  I was a high school kid at the time,
 *    with nothing more than a Commodore 64.  Somehow we wrote this program,
 *    (he did the math obviously, I just coded it into BASIC for the c64).
 *    Yeah, 320x200 resolution, colorless white wireframe, and half an hour
 *    rendering time per superquadric.  PLOT x,y.  THOSE were the days.
 *    In the following years I would port Superquadrics to AppleBASIC,
 *    AmigaBASIC, and then Turbo Pascal for IBM clones.  5 minutes on a 286!
 *    Talk about fast rendering!  But these days, when my Pentium 166 runs
 *    the same program, the superquadric will already be waiting on the
 *    screen before my monitor can change frequency from text to graphics
 *    mode.  Can't time the number of minutes that way!  Darn ;)
 *
 * Ed Mackey
 */

/*-
 * due to a Bug/feature in VMS X11/Intrinsic.h has to be placed before xlock.
 * otherwise caddr_t is not defined correctly
 */

#include <X11/Intrinsic.h>

#ifdef STANDALONE
#define PROGCLASS "Superquadrics"
#define HACK_INIT init_superquadrics
#define HACK_DRAW draw_superquadrics
#define superquadrics_opts xlockmore_opts
#define DEFAULTS "*delay: 100 \n" \
 "*count: 25 \n" \
 "*cycles: 40 \n" \
 "*wireframe: False \n"
#include "xlockmore.h"		/* from the xscreensaver distribution */
#else /* !STANDALONE */
#include "xlock.h"		/* from the xlockmore distribution */
#include "vis.h"
#endif /* !STANDALONE */

#ifdef MODE_superquadrics

/*-
 * Note for low-CPU-speed machines:  If your frame rate is so low that
 * attempts at animation appear futile, try using "-cycles 1", which puts
 * Superquadrics into kind of a slide-show mode.  It will still use up
 * all of your CPU power, but it may look nicer.
 */

#define DEF_SPINSPEED  "5.0"

static float spinspeed;

static XrmOptionDescRec opts[] =
{
  {"-spinspeed", ".superquadrics.spinspeed", XrmoptionSepArg, (caddr_t) NULL}
};
static argtype vars[] =
{
  {(caddr_t *) & spinspeed, "spinspeed", "Spinspeed", DEF_SPINSPEED, t_Float}
};
static OptionStruct desc[] =
{
	{"-spinspeed num", "speed of rotation, in degrees per frame"}
};

ModeSpecOpt superquadrics_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   superquadrics_description =
{"superquadrics", "init_superquadrics", "draw_superquadrics", "release_superquadrics",
 "refresh_superquadrics", "init_superquadrics", NULL, &superquadrics_opts,
 1000, 25, 40, 1, 4, 1.0, "",
 "Shows 3D mathematical shapes", 0, NULL};

#endif

#include <GL/glu.h>

#define MaxRes          50
#define MinRes          5

typedef double dimi[MaxRes + 1];

typedef struct {
	double      xExponent, yExponent;
	GLfloat     r[4], g[4], b[4];
	long        Mode;
	int         rotx, rotz;
} state;

typedef struct {
	GLXContext *glx_context;
	int         dist, wireframe, flatshade, shownorms, maxcount, maxwait;
	int         counter, viewcount, viewwait, mono;
	GLfloat     curmat[4][4], rotx, roty, rotz, spinspeed;
	/* In dimi: the first letter stands for cosine/sine, the second
	 *          stands for North, South, East, or West.  I think.
	 */
	dimi        cs, se, sw, sn, ss, ce, cw, cn, Prevxx, Prevyy, Prevzz,
	            Prevxn, Prevyn, Prevzn;
	double      xExponent, yExponent, Mode;
	int         resolution;
	state       now, later;
} superquadricsstruct;

static superquadricsstruct *superquadrics = NULL;

#define CLIP_NORMALS 10000.0

static void ReshapeSuperquadrics(int w, int h);

static int
myrand(int range)
{
	return ((int) (((float) range) * LRAND() / (MAXRAND)));
}

static float
myrandreal(void)
{
	return (LRAND() / (MAXRAND));
}

/* Some old, old, OLD code follows.  Ahh this takes me back..... */

/* Output from p2c, the Pascal-to-C translator */
/* From input file "squad.pas" */

static double
XtoY(double x, double y)
{
	double      z, a;

	/* This is NOT your typical raise-X-to-the-Y-power function.  Do not attempt
	 * to replace this with a standard exponent function.  If you must, just
	 * replace the "a = exp(y * log(z));" line with something faster.
	 */

	z = fabs(x);
	if (z < 1e-20) {
		a = 0.0;
		return a;
	}
	a = exp(y * log(z));
	if (a > CLIP_NORMALS)
		a = CLIP_NORMALS;
	if (x < 0)
		a = -a;
	return a;
}


static double
Sine(double x, double e)
{
	/* This is just the sine wave raised to the exponent.  BUT, you can't
	 * raise negative numbers to fractional exponents.  So we have a special
	 * XtoY routune which handles it in a way useful to superquadrics.
	 */

	return (XtoY(sin(x), e));
}


static double
Cosine(double x, double e)
{
	return (XtoY(cos(x), e));
}


static void
MakeUpStuff(int allstuff, superquadricsstruct * sp)
{
	static int  pats[4][4] =
	{
		{0, 0, 0, 0},
		{0, 1, 0, 1},
		{0, 0, 1, 1},
		{0, 1, 1, 0}
	};

	int         dostuff;
	int         t, pat;
	GLfloat     r, g, b, r2, g2, b2;

	/* randomize it. */

	if (sp->maxcount < 2)
		allstuff = 1;
	dostuff = allstuff * 15;
	if (!dostuff) {
		dostuff = myrand(3) + 1;
		if (myrand(2) || (dostuff & 1))
			dostuff |= 4;
		if (myrand(2))
			dostuff |= 8;
	}
	if (dostuff & 1) {
		sp->later.xExponent = (((long) floor(myrandreal() * 250 + 0.5)) / 100.0) + 0.1;
		sp->later.yExponent = (((long) floor(myrandreal() * 250 + 0.5)) / 100.0) + 0.1;

		/* Increase the 2.0 .. 2.5 range to 2.0 .. 3.0 */
		if (sp->later.xExponent > 2.0)
			sp->later.xExponent = (sp->later.xExponent * 2.0) - 2.0;
		if (sp->later.yExponent > 2.0)
			sp->later.yExponent = (sp->later.yExponent * 2.0) - 2.0;
	}
	if (dostuff & 2) {
		do {
			sp->later.Mode = myrand(3L) + 1;
		} while (!allstuff && (sp->later.Mode == sp->now.Mode));
		/* On init: make sure it can stay in mode 1 if it feels like it. */
	}
	if (dostuff & 4) {
		if (sp->mono) {
			if (sp->wireframe) {
				b = g = r = 1.0;
				b2 = g2 = r2 = 1.0;
			} else {
				b = g = r = (GLfloat) (140 + myrand(100)) / 255.0;
				b2 = g2 = r2 = ((r > 0.69) ? (1.0 - r) : r);
			}
		} else {
			r = (GLfloat) (40 + myrand(200)) / 255.0;
			g = (GLfloat) (40 + myrand(200)) / 255.0;
			b = (GLfloat) (40 + myrand(200)) / 255.0;

			r2 = ((myrand(4) && ((r < 0.31) || (r > 0.69))) ? (1.0 - r) : r);
			g2 = ((myrand(4) && ((g < 0.31) || (g > 0.69))) ? (1.0 - g) : g);
			b2 = ((myrand(4) && ((b < 0.31) || (b > 0.69))) ? (1.0 - b) : b);
		}

		pat = myrand(4);
		for (t = 0; t < 4; ++t) {
			sp->later.r[t] = pats[pat][t] ? r : r2;
			sp->later.g[t] = pats[pat][t] ? g : g2;
			sp->later.b[t] = pats[pat][t] ? b : b2;
		}
	}
	if (dostuff & 8) {
		sp->later.rotx = myrand(360) - 180;
		sp->later.rotz = myrand(160) - 80;
	}
}

static void
inputs(superquadricsstruct * sp)
{
	int         iv;
	double      u, v, mode3, cn3, inverter2, flatu, flatv;

	if (sp->Mode < 1.000001) {
		mode3 = 1.0;
		cn3 = 0.0;
		inverter2 = 1.0;
	} else if (sp->Mode < 2.000001) {
		mode3 = 1.0;
		cn3 = (sp->Mode - 1.0) * 1.5;
		inverter2 = (sp->Mode - 1.0) * -2.0 + 1.0;
	} else {
		mode3 = (sp->Mode - 1.0);
		cn3 = (sp->Mode - 2.0) / 2.0 + 1.5;
		inverter2 = -1.0;
	}

	if (sp->flatshade) {
		flatu = M_PI / (sp->resolution - 1);
		flatv = mode3 * M_PI / ((sp->resolution - 1) * 2);
	} else {
		flatu = flatv = 0.0;
	}

	/* (void) printf("Calculating....\n"); */
	for (iv = 1; iv <= sp->resolution; iv++) {

		/* u ranges from PI down to -PI */
		u = (1 - iv) * 2 * M_PI / (sp->resolution - 1) + M_PI;

		/* v ranges from PI/2 down to -PI/2 */
		v = (1 - iv) * mode3 * M_PI / (sp->resolution - 1) + M_PI * (mode3 / 2.0);

		/* Use of xExponent */
		sp->se[iv] = Sine(u, sp->xExponent);
		sp->ce[iv] = Cosine(u, sp->xExponent);
		sp->sn[iv] = Sine(v, sp->yExponent);
		sp->cn[iv] = Cosine(v, sp->yExponent) * inverter2 + cn3;

		/* Normal vector computations only */
		sp->sw[iv] = Sine(u + flatu, 2 - sp->xExponent);
		sp->cw[iv] = Cosine(u + flatu, 2 - sp->xExponent);
		sp->ss[iv] = Sine(v + flatv, 2 - sp->yExponent) * inverter2;
		sp->cs[iv] = Cosine(v + flatv, 2 - sp->yExponent);
	}			/* next */

	/* Now fix up the endpoints */
	sp->se[sp->resolution] = sp->se[1];
	sp->ce[sp->resolution] = sp->ce[1];

	if (sp->Mode > 2.999999) {
		sp->sn[sp->resolution] = sp->sn[1];
		sp->cn[sp->resolution] = sp->cn[1];
	}
}


static void
DoneScale(superquadricsstruct * sp)
{
	double      xx, yy, zz, xp = 0, yp = 0, zp = 0, xn, yn, zn, xnp = 0,
	            ynp = 0, znp = 0;
	int         ih, iv;

	/* Hey don't knock my 2-letter variable names.  Simon's BASIC rules, man! ;-> */
	/* Just kidding..... */
	int         toggle = 0;

	for (ih = 1; ih <= sp->resolution; ih++) {
		toggle ^= 2;
		for (iv = 1; iv <= sp->resolution; iv++) {
			toggle ^= 1;
			if (sp->wireframe)
				glColor3f(sp->curmat[toggle][0], sp->curmat[toggle][1], sp->curmat[toggle][2]);
			else
				glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, sp->curmat[toggle]);

			xx = sp->cn[iv] * sp->ce[ih];
			zz = sp->cn[iv] * sp->se[ih];
			yy = sp->sn[iv];

			if (sp->wireframe) {
				if ((ih > 1) || (iv > 1)) {
					glBegin(GL_LINES);
					if (ih > 1) {
						glVertex3f(xx, yy, zz);
						glVertex3f(sp->Prevxx[iv], sp->Prevyy[iv], sp->Prevzz[iv]);
					}
					if (iv > 1) {
						glVertex3f(xx, yy, zz);
						glVertex3f(sp->Prevxx[iv - 1], sp->Prevyy[iv - 1], sp->Prevzz[iv - 1]);
					}
/* PURIFY 4.0.1 reports an unitialized memory read on the next line when using
   * MesaGL 2.2 and -mono.  This has been fixed in MesaGL 2.3 and later. */
					glEnd();
				}
			} else {
				if ((sp->cs[iv] > 1e+10) || (sp->cs[iv] < -1e+10)) {
					xn = sp->cs[iv];
					zn = sp->cs[iv];
					yn = sp->ss[iv];
				} else {
					xn = sp->cs[iv] * sp->cw[ih];
					zn = sp->cs[iv] * sp->sw[ih];
					yn = sp->ss[iv];
				}
				if ((ih > 1) && (iv > 1)) {
					glNormal3f(xn, yn, zn);
					glBegin(GL_POLYGON);
					glVertex3f(xx, yy, zz);
					if (!sp->flatshade)
						glNormal3f(sp->Prevxn[iv], sp->Prevyn[iv], sp->Prevzn[iv]);
					glVertex3f(sp->Prevxx[iv], sp->Prevyy[iv], sp->Prevzz[iv]);
					if (!sp->flatshade)
						glNormal3f(xnp, ynp, znp);
					glVertex3f(xp, yp, zp);
					if (!sp->flatshade)
						glNormal3f(sp->Prevxn[iv - 1], sp->Prevyn[iv - 1], sp->Prevzn[iv - 1]);
					glVertex3f(sp->Prevxx[iv - 1], sp->Prevyy[iv - 1], sp->Prevzz[iv - 1]);
					glEnd();
				}
				if (sp->shownorms) {
					if (!sp->flatshade)
						glShadeModel(GL_FLAT);
					glDisable(GL_LIGHTING);
					glBegin(GL_LINES);
					glVertex3f(xx, yy, zz);
					glVertex3f(xx + xn, yy + yn, zz + zn);
					glEnd();
					if (!sp->flatshade)
						glShadeModel(GL_SMOOTH);
					glEnable(GL_LIGHTING);
				}
				xnp = sp->Prevxn[iv];
				ynp = sp->Prevyn[iv];
				znp = sp->Prevzn[iv];
				sp->Prevxn[iv] = xn;
				sp->Prevyn[iv] = yn;
				sp->Prevzn[iv] = zn;
			}

			xp = sp->Prevxx[iv];
			yp = sp->Prevyy[iv];
			zp = sp->Prevzz[iv];
			sp->Prevxx[iv] = xx;
			sp->Prevyy[iv] = yy;
			sp->Prevzz[iv] = zz;

		}		/* next */
	}			/* next */
}

/**** End of really old code ****/

static void
SetCull(int init, superquadricsstruct * sp)
{
	static int  cullmode;

	if (init) {
		cullmode = 0;
		return;
	}
	if (sp->Mode < 1.0001) {
		if (cullmode != 1) {
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
			cullmode = 1;
		}
	} else if (sp->Mode > 2.9999) {
		if (cullmode != 2) {
			glEnable(GL_CULL_FACE);
			glCullFace(GL_FRONT);
			cullmode = 2;
		}
	} else {
		if (cullmode) {
			glDisable(GL_CULL_FACE);
			cullmode = 0;
		}
	}
}

static void
SetCurrentShape(superquadricsstruct * sp)
{
	int         t;

	sp->xExponent = sp->now.xExponent = sp->later.xExponent;
	sp->yExponent = sp->now.yExponent = sp->later.yExponent;

	for (t = 0; t < 4; ++t) {
		sp->curmat[t][0] = sp->now.r[t] = sp->later.r[t];
		sp->curmat[t][1] = sp->now.g[t] = sp->later.g[t];
		sp->curmat[t][2] = sp->now.b[t] = sp->later.b[t];
	}

	sp->Mode = (double) (sp->now.Mode = sp->later.Mode);
	sp->rotx = sp->now.rotx = sp->later.rotx;
	sp->rotz = sp->now.rotz = sp->later.rotz;

	sp->counter = -sp->maxwait;

	inputs(sp);
}

static void
NextSuperquadric(superquadricsstruct * sp)
{
	double      fnow, flater;
	int         t;

	sp->roty -= sp->spinspeed;
	while (sp->roty >= 360.0)
		sp->roty -= 360.0;
	while (sp->roty < 0.0)
		sp->roty += 360.0;

	--sp->viewcount;

	if (sp->counter > 0) {
		if (--sp->counter == 0) {
			SetCurrentShape(sp);
			if (sp->counter == 0) {		/* Happens if sp->maxwait == 0 */
				MakeUpStuff(0, sp);
				sp->counter = sp->maxcount;
			}
		} else {
			fnow = (double) sp->counter / (double) sp->maxcount;
			flater = (double) (sp->maxcount - sp->counter) / (double) sp->maxcount;
			sp->xExponent = sp->now.xExponent * fnow + sp->later.xExponent * flater;
			sp->yExponent = sp->now.yExponent * fnow + sp->later.yExponent * flater;

			for (t = 0; t < 4; ++t) {
				sp->curmat[t][0] = sp->now.r[t] * fnow + sp->later.r[t] * flater;
				sp->curmat[t][1] = sp->now.g[t] * fnow + sp->later.g[t] * flater;
				sp->curmat[t][2] = sp->now.b[t] * fnow + sp->later.b[t] * flater;
			}

			sp->Mode = (double) sp->now.Mode * fnow + (double) sp->later.Mode * flater;
			sp->rotx = (double) sp->now.rotx * fnow + (double) sp->later.rotx * flater;
			sp->rotz = (double) sp->now.rotz * fnow + (double) sp->later.rotz * flater;

			inputs(sp);
		}
	} else {
		if (++sp->counter >= 0) {
			MakeUpStuff(0, sp);
			sp->counter = sp->maxcount;
		}
	}
}

static void
DisplaySuperquadrics(superquadricsstruct * sp)
{
	glDrawBuffer(GL_BACK);
	if (sp->wireframe)
		glClear(GL_COLOR_BUFFER_BIT);
	else
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (sp->viewcount < 1) {
		sp->viewcount = sp->viewwait;
		ReshapeSuperquadrics(-1, -1);
	}
	glPushMatrix();
	glTranslatef(0.0, 0.0, -((GLfloat) (sp->dist) / 16.0) - (sp->Mode * 3.0 - 1.0));	/* viewing transform  */
	glRotatef(sp->rotx, 1.0, 0.0, 0.0);	/* pitch */
	glRotatef(sp->rotz, 0.0, 0.0, 1.0);	/* bank */
	glRotatef(sp->roty, 0.0, 1.0, 0.0);	/* "spin", like heading but comes after P & B */

	SetCull(0, sp);

	DoneScale(sp);

	glPopMatrix();

	/* Remember to flush & swap the buffers after calling this function! */
}

static void
NextSuperquadricDisplay(superquadricsstruct * sp)
{
	NextSuperquadric(sp);
	DisplaySuperquadrics(sp);
}

#define MINSIZE 200
static void
ReshapeSuperquadrics(int w, int h)
{
	static int  last_w = 0, last_h = 0;
	int         maxsize, cursize;

	if (w < 0) {
		w = last_w;
		h = last_h;
	} else {
		last_w = w;
		last_h = h;
	}
	maxsize = (w < h) ? w : h;
	if (maxsize <= MINSIZE) {
		cursize = maxsize;
	} else {
		cursize = myrand(maxsize - MINSIZE) + MINSIZE;
	}
	if ((w > cursize) && (h > cursize)) {
		glViewport(myrand(w - cursize), myrand(h - cursize), cursize, cursize);
		w = h = cursize;
	} else {
		glViewport(0, 0, w, h);
	}
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(30.0, (GLfloat) w / (GLfloat) h, 0.1, 200.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

static void
InitSuperquadrics(int wfmode, int snorm, int res, int count, float speed, superquadricsstruct * sp)
{
	GLfloat     ambient[] =
	{0.4, 0.4, 0.4, 1.0};
	GLfloat     position[] =
	{10.0, 1.0, 1.0, 10.0};
	GLfloat     mat_diffuse[] =
	{1.0, 0.5, 0.5, 1.0};
	GLfloat     mat_specular[] =
	{0.8, 0.8, 0.8, 1.0};
	GLfloat     mat_shininess[] =
	{50.0};

	int         t;

	for (t = 0; t < 4; ++t)
		sp->curmat[t][3] = 1.0;

	sp->rotx = 35.0;
	sp->roty = 0.0;
	sp->rotz = 0.0;
	sp->dist = (16 << 3);
	sp->wireframe = sp->flatshade = sp->shownorms = 0;
	sp->maxcount = count;
	if (sp->maxcount < 1)
		sp->maxcount = 1;
	sp->maxwait = sp->maxcount >> 1;
	SetCull(1, sp);

	sp->spinspeed = speed;
	sp->viewcount = sp->viewwait = (sp->maxcount < 2) ? 1 : (sp->maxcount << 3);

	if (res < MinRes)
		res = MinRes;
	if (res > MaxRes)
		res = MaxRes;
	sp->resolution = res;

	if (wfmode == 2)
		sp->flatshade = 1;
	else if (wfmode)
		sp->wireframe = 1;

	if (snorm)
		sp->shownorms = 1;

	if (sp->wireframe) {
		glShadeModel(GL_FLAT);
		glDisable(GL_LIGHTING);
		glColor3f(mat_diffuse[0], mat_diffuse[1], mat_diffuse[2]);
	} else {
		if (sp->flatshade) {
			glShadeModel(GL_FLAT);
			position[0] = 1.0;
			position[3] = 0.0;
		}
		glEnable(GL_LIGHTING);
		glEnable(GL_LIGHT0);
		glDepthFunc(GL_LEQUAL);
		glEnable(GL_DEPTH_TEST);

		glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
		glLightfv(GL_LIGHT0, GL_POSITION, position);

		/*glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, mat_diffuse); */
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, mat_shininess);

		glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

		glFrontFace(GL_CW);
		glEnable(GL_NORMALIZE);
	}

	MakeUpStuff(1, sp);
	SetCurrentShape(sp);
	MakeUpStuff(1, sp);	/* Initialize it */
	sp->counter = sp->maxcount;
}

/* End of superquadrics main functions */

void
init_superquadrics(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         screen = MI_SCREEN(mi);

	superquadricsstruct *sp;

	if (superquadrics == NULL) {
		if ((superquadrics = (superquadricsstruct *) calloc(MI_NUM_SCREENS(mi),
				      sizeof (superquadricsstruct))) == NULL)
			return;
	}
	sp = &superquadrics[screen];
	sp->mono = (MI_IS_MONO(mi) ? 1 : 0);

	if ((sp->glx_context = init_GL(mi)) != NULL) {

		InitSuperquadrics(MI_IS_WIREFRAME(mi), 0,
				  MI_COUNT(mi), MI_CYCLES(mi), spinspeed, sp);
		ReshapeSuperquadrics(MI_WIDTH(mi), MI_HEIGHT(mi));

		DisplaySuperquadrics(sp);
		glFinish();
		glXSwapBuffers(display, window);
	} else {
		MI_CLEARWINDOW(mi);
	}
}

void
draw_superquadrics(ModeInfo * mi)
{
	superquadricsstruct *sp = &superquadrics[MI_SCREEN(mi)];
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);

	MI_IS_DRAWN(mi) = True;

	if (!sp->glx_context)
		return;

	glXMakeCurrent(display, window, *(sp->glx_context));

	NextSuperquadricDisplay(sp);

	glFinish();
	glXSwapBuffers(display, window);
}

void
refresh_superquadrics(ModeInfo * mi)
{
	/* Nothing happens here */
}

void
release_superquadrics(ModeInfo * mi)
{
	if (superquadrics != NULL) {
		(void) free((void *) superquadrics);
		superquadrics = NULL;
	}
	FreeAllGL(mi);
}


#endif

/* End of superquadrics.c */
