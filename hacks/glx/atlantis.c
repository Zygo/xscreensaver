/* atlantis --- Shows moving 3D sea animals */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)atlantis.c	1.3 98/06/18 xlockmore";

#endif

/* Copyright (c) E. Lassauge, 1998. */

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
 * The original code for this mode was written by Mark J. Kilgard
 * as a demo for openGL programming.
 * 
 * Porting it to xlock  was possible by comparing the original Mesa's morph3d 
 * demo with it's ported version to xlock, so thanks for Marcelo F. Vianna 
 * (look at morph3d.c) for his indirect help.
 *
 * Thanks goes also to Brian Paul for making it possible and inexpensive
 * to use OpenGL at home.
 *
 * My e-mail address is lassauge@sagem.fr
 *
 * Eric Lassauge  (May-13-1998)
 *
 * REVISION HISTORY:
 * 
 * David A. Bagley - 98/06/17 : Add whalespeed option. Global options to
 *                              initialize local variables are now:
 *                              XLock.atlantis.cycles: 100      ! SharkSpeed
 *                              XLock.atlantis.batchcount: 4    ! SharkNum
 *                              XLock.atlantis.whalespeed: 250  ! WhaleSpeed
 *                              XLock.atlantis.size: 6000       ! SharkSize
 *                              Add random direction for whales/dolphins
 * 
 * E.Lassauge - 98/06/16: Use the following global options to initialize
 *                        local variables :
 *                              XLock.atlantis.delay: 100       ! SharkSpeed
 *                              XLock.atlantis.batchcount: 4    ! SharkNum
 *                              XLock.atlantis.cycles: 250      ! WhaleSpeed
 *                              XLock.atlantis.size: 6000       ! SharkSize
 *                        Add support for -/+ wireframe (t'was so easy to do!)
 *
 * TODO : 
 *        - add a sort of background image or random bg color
 *        - better handling of sizes and speeds
 *        - test standalone and module modes
 *        - purify it (!)
 */

/* Copyright (c) Mark J. Kilgard, 1994. */

/**
 * (c) Copyright 1993, 1994, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * Permission to use, copy, modify, and distribute this software for
 * any purpose and without fee is hereby granted, provided that the above
 * copyright notice appear in all copies and that both the copyright notice
 * and this permission notice appear in supporting documentation, and that
 * the name of Silicon Graphics, Inc. not be used in advertising
 * or publicity pertaining to distribution of the software without specific,
 * written prior permission.
 *
 * THE MATERIAL EMBODIED ON THIS SOFTWARE IS PROVIDED TO YOU "AS-IS"
 * AND WITHOUT WARRANTY OF ANY KIND, EXPRESS, IMPLIED OR OTHERWISE,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY OR
 * FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL SILICON
 * GRAPHICS, INC.  BE LIABLE TO YOU OR ANYONE ELSE FOR ANY DIRECT,
 * SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY
 * KIND, OR ANY DAMAGES WHATSOEVER, INCLUDING WITHOUT LIMITATION,
 * LOSS OF PROFIT, LOSS OF USE, SAVINGS OR REVENUE, OR THE CLAIMS OF
 * THIRD PARTIES, WHETHER OR NOT SILICON GRAPHICS, INC.  HAS BEEN
 * ADVISED OF THE POSSIBILITY OF SUCH LOSS, HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE
 * POSSESSION, USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * US Government Users Restricted Rights
 * Use, duplication, or disclosure by the Government is subject to
 * restrictions set forth in FAR 52.227.19(c)(2) or subparagraph
 * (c)(1)(ii) of the Rights in Technical Data and Computer Software
 * clause at DFARS 252.227-7013 and/or in similar or successor
 * clauses in the FAR or the DOD or NASA FAR Supplement.
 * Unpublished-- rights reserved under the copyright laws of the
 * United States.  Contractor/manufacturer is Silicon Graphics,
 * Inc., 2011 N.  Shoreline Blvd., Mountain View, CA 94039-7311.
 *
 * OpenGL(TM) is a trademark of Silicon Graphics, Inc.
 */

#ifdef STANDALONE
# define PROGCLASS	"Atlantis"
# define HACK_INIT	init_atlantis
# define HACK_DRAW	draw_atlantis
# define atlantis_opts	xlockmore_opts
# define DEFAULTS	"*delay:	1000 \n" \
			 "*count:          4 \n" \
			 "*cycles:       100 \n" \
			 "*size:        6000 \n" \
			 "*whalespeed:   250 \n"
# include "xlockmore.h"		/* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"		/* from the xlockmore distribution */
#include "vis.h"
#endif /* !STANDALONE */

#ifdef USE_GL

#include "atlantis.h"
#include <GL/glu.h>


#define DEF_WHALESPEED  "250"
static int  whalespeed;
static XrmOptionDescRec opts[] =
{
     {"-whalespeed", ".atlantis.whalespeed", XrmoptionSepArg, (caddr_t) NULL}
};

static argtype vars[] =
{
{(caddr_t *) & whalespeed, "whalespeed", "WhaleSpeed", DEF_WHALESPEED, t_Int}
};

static OptionStruct desc[] =
{
	{"-whalespeed num", "speed of whales and the dolphin"}
};

ModeSpecOpt atlantis_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   atlantis_description =
{"atlantis", "init_atlantis", "draw_atlantis", "release_atlantis",
 "refresh_atlantis", "change_atlantis", NULL, &atlantis_opts,
 1000, NUM_SHARKS, SHARKSPEED, SHARKSIZE, 64, 1.0, "",
 "Shows moving sharks/whales/dolphin", 0, NULL};

#endif

static atlantisstruct *atlantis = NULL;

static void
InitFishs(atlantisstruct * ap)
{
	int         i;

	for (i = 0; i < ap->num_sharks; i++) {
		ap->sharks[i].x = 70000.0 + NRAND(ap->sharksize);
		ap->sharks[i].y = NRAND(ap->sharksize);
		ap->sharks[i].z = NRAND(ap->sharksize);
		ap->sharks[i].psi = NRAND(360) - 180.0;
		ap->sharks[i].v = 1.0;
	}

	/* Random whae direction */
	ap->whaledir = LRAND() & 1;

	ap->dolph.x = 30000.0;
	ap->dolph.y = 0.0;
	ap->dolph.z = (float) (ap->sharksize);
	ap->dolph.psi = (ap->whaledir) ? 90.0 : -90.0;
	ap->dolph.theta = 0.0;
	ap->dolph.v = 6.0;

	ap->momWhale.x = 70000.0;
	ap->momWhale.y = 0.0;
	ap->momWhale.z = 0.0;
	ap->momWhale.psi = (ap->whaledir) ? 90.0 : -90.0;
	ap->momWhale.theta = 0.0;
	ap->momWhale.v = 3.0;

	ap->babyWhale.x = 60000.0;
	ap->babyWhale.y = -2000.0;
	ap->babyWhale.z = -2000.0;
	ap->babyWhale.psi = (ap->whaledir) ? 90.0 : -90.0;
	ap->babyWhale.theta = 0.0;
	ap->babyWhale.v = 3.0;
}

static void
Init(atlantisstruct * ap)
{
	static float ambient[] =
	{0.1, 0.1, 0.1, 1.0};
	static float diffuse[] =
	{1.0, 1.0, 1.0, 1.0};
	static float position[] =
	{0.0, 1.0, 0.0, 0.0};
	static float mat_shininess[] =
	{90.0};
	static float mat_specular[] =
	{0.8, 0.8, 0.8, 1.0};
	static float mat_diffuse[] =
	{0.46, 0.66, 0.795, 1.0};
	static float mat_ambient[] =
	{0.0, 0.1, 0.2, 1.0};
	static float lmodel_ambient[] =
	{0.4, 0.4, 0.4, 1.0};
	static float lmodel_localviewer[] =
	{0.0};
	float       fblue = 0.0, fgreen;

	glFrontFace(GL_CW);

	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);

	glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
	glLightfv(GL_LIGHT0, GL_POSITION, position);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);
	glLightModelfv(GL_LIGHT_MODEL_LOCAL_VIEWER, lmodel_localviewer);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, mat_shininess);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diffuse);
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat_ambient);

	InitFishs(ap);

	/* Add a little randomness */
	fblue = ((float) (NRAND(50)) / 100.0) + 0.50;
	fgreen = fblue * 0.56;
	glClearColor(0.0, fgreen, fblue, 0.0);
}

static void
Reshape(ModeInfo * mi, int width, int height)
{
	atlantisstruct *ap = &atlantis[MI_SCREEN(mi)];

	glViewport(0, 0, ap->WinW = (GLint) width, ap->WinH = (GLint) height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(400.0, (GLdouble) width / (GLdouble) height, 1.0, 2000000.0);
	glMatrixMode(GL_MODELVIEW);
}

static void
Animate(atlantisstruct * ap)
{
	int         i;

	for (i = 0; i < ap->num_sharks; i++) {
		SharkPilot(&(ap->sharks[i]), ap->sharkspeed);
		SharkMiss(ap, i);
	}
	WhalePilot(&(ap->dolph), ap->whalespeed, ap->whaledir);
	ap->dolph.phi++;
	WhalePilot(&(ap->momWhale), ap->whalespeed, ap->whaledir);
	ap->momWhale.phi++;
	WhalePilot(&(ap->babyWhale), ap->whalespeed, ap->whaledir);
	ap->babyWhale.phi++;
}

static void
AllDisplay(atlantisstruct * ap)
{
	int         i;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	for (i = 0; i < ap->num_sharks; i++) {
		glPushMatrix();
		FishTransform(&(ap->sharks[i]));
		DrawShark(&(ap->sharks[i]), ap->wire);
		glPopMatrix();
	}

	glPushMatrix();
	FishTransform(&(ap->dolph));
	DrawDolphin(&(ap->dolph), ap->wire);
	glPopMatrix();

	glPushMatrix();
	FishTransform(&(ap->momWhale));
	DrawWhale(&(ap->momWhale), ap->wire);
	glPopMatrix();

	glPushMatrix();
	FishTransform(&(ap->babyWhale));
	glScalef(0.45, 0.45, 0.3);
	DrawWhale(&(ap->babyWhale), ap->wire);
	glPopMatrix();
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
 *    Initialize atlantis.  Called each time the window changes.
 *-----------------------------------------------------------------------------
 */

void
init_atlantis(ModeInfo * mi)
{
	int         screen = MI_SCREEN(mi);
	atlantisstruct *ap;
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);

	if (atlantis == NULL) {
		if ((atlantis = (atlantisstruct *) calloc(MI_NUM_SCREENS(mi),
					   sizeof (atlantisstruct))) == NULL)
			return;
	}
	ap = &atlantis[screen];
	ap->num_sharks = MI_COUNT(mi);
	if (ap->sharks == NULL) {
		if ((ap->sharks = (fishRec *) calloc(ap->num_sharks,
						sizeof (fishRec))) == NULL) {
			/* free everything up to now */
			(void) free((void *) atlantis);
			atlantis = NULL;
			return;
		}
	}
	ap->sharkspeed = MI_CYCLES(mi);		/* has influence on the "width"
						   of the movement */
	ap->sharksize = MI_SIZE(mi);	/* has influence on the "distance"
					   of the sharks */
	ap->whalespeed = whalespeed;
	ap->wire = MI_IS_WIREFRAME(mi);

	if (MI_IS_DEBUG(mi)) {
		(void) fprintf(stderr,
			       "%s:\n\tnum_sharks=%d\n\tsharkspeed=%.1f\n\tsharksize=%d\n\twhalespeed=%.1f\n\twireframe=%s\n",
			       MI_NAME(mi),
			       ap->num_sharks,
			       ap->sharkspeed,
			       ap->sharksize,
			       ap->whalespeed,
			       ap->wire ? "yes" : "no"
			);
	}
	if ((ap->glx_context = init_GL(mi)) != NULL) {

		Reshape(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
		glDrawBuffer(GL_BACK);
		Init(ap);
		AllDisplay(ap);
		glXSwapBuffers(display, window);

	} else {
		MI_CLEARWINDOW(mi);
	}
}

/*
 *-----------------------------------------------------------------------------
 *    Called by the mainline code periodically to update the display.
 *-----------------------------------------------------------------------------
 */
void
draw_atlantis(ModeInfo * mi)
{
	atlantisstruct *ap = &atlantis[MI_SCREEN(mi)];

	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);

	MI_IS_DRAWN(mi) = True;

	if (!ap->glx_context)
		return;

	glXMakeCurrent(display, window, *(ap->glx_context));

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glPushMatrix();

	AllDisplay(ap);
	Animate(ap);

	glXSwapBuffers(display, window);
}


/*
 *-----------------------------------------------------------------------------
 *    The display is being taken away from us.  Free up malloc'ed 
 *      memory and X resources that we've alloc'ed.  Only called
 *      once, we must zap everything for every screen.
 *-----------------------------------------------------------------------------
 */

void
release_atlantis(ModeInfo * mi)
{
	int         screen;

	if (atlantis != NULL) {
		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			atlantisstruct *ap = &atlantis[screen];

			if (ap->sharks)
				(void) free((void *) ap->sharks);
		}
		(void) free((void *) atlantis);
		atlantis = NULL;
	}
	FreeAllGL(mi);
}

void
refresh_atlantis(ModeInfo * mi)
{
}

void
change_atlantis(ModeInfo * mi)
{
	atlantisstruct *ap = &atlantis[MI_SCREEN(mi)];

	if (!ap->glx_context)
		return;

	glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(ap->glx_context));
	Init(ap);
}

#endif /* USE_GL */
