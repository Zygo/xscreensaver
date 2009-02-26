/* atlantis --- Shows moving 3D sea animals */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)atlantis.c	1.1 98/05/13 xlockmore";

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
 * TODO : - use def_xxx options to initialize local variables (number of
 *          sharks, etc...)
 *        - test standalone mode
 *	  - better colormap handling (problems somewhere ?)
 *	  - purify it (!)
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
# define DEFAULTS	"*delay:	1000 \n"
# include "xlockmore.h"		/* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"		/* from the xlockmore distribution */

#endif /* !STANDALONE */

#ifdef USE_GL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "atlantis.h"
#include <GL/glu.h>


ModeSpecOpt atlantis_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   atlantis_description =
{"atlantis", "init_atlantis", "draw_atlantis", "release_atlantis",
 "refresh_atlantis", "change_atlantis", NULL, &atlantis_opts,
 1000, 2, 5, 500, 64, 1.0, "",
 "Shows moving sharks/whales/dolphin", 0, NULL};

#endif

static atlantisstruct *atlantis = NULL;

static void
InitFishs(atlantisstruct *ap)
{
    int i;

    for (i = 0; i < NUM_SHARKS; i++) {
        ap->sharks[i].x = 70000.0 + NRAND(6000);
        ap->sharks[i].y = NRAND(6000);
        ap->sharks[i].z = NRAND(6000);
        ap->sharks[i].psi = NRAND(360) - 180.0;
        ap->sharks[i].v = 1.0;
    }

		/* i = LRAND() & 1; */ /* Works the first time but they do not return */
    ap->dolph.x = 30000.0;
    ap->dolph.y = 0.0;
    ap->dolph.z = 6000.0;
    ap->dolph.psi = /* (i) ? 90.0 : - */ 90.0;
    ap->dolph.theta = 0.0;
    ap->dolph.v = 3.0;

    ap->momWhale.x = 70000.0;
    ap->momWhale.y = 0.0;
    ap->momWhale.z = 0.0;
    ap->momWhale.psi = /* (i) ? 90.0 : - */ 90.0;
    ap->momWhale.theta = 0.0;
    ap->momWhale.v = 3.0;

    ap->babyWhale.x = 60000.0;
    ap->babyWhale.y = -2000.0;
    ap->babyWhale.z = -2000.0;
    ap->babyWhale.psi = /* (i) ? 90.0 : - */ 90.0;
    ap->babyWhale.theta = 0.0;
    ap->babyWhale.v = 3.0;
}

static void
Init(atlantisstruct *ap)
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
#if 0
    GLfloat map1[4] =
    {0.0, 0.0, 0.0, 0.0};
    GLfloat map2[4] =
    {0.0, 0.0, 0.0, 0.0};
#endif

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

    glClearColor(0.0, 0.5, 0.9, 0.0);
}

static void
Reshape(ModeInfo *mi,int width, int height)
{
    atlantisstruct *ap = &atlantis[MI_SCREEN(mi)];

    glViewport(0, 0, ap->WinW= (GLint) width, ap->WinH= (GLint) height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(400.0, (GLdouble)width/(GLdouble)height, 1.0, 2000000.0);
    glMatrixMode(GL_MODELVIEW);
}

static void
Animate(atlantisstruct *ap)
{
    int i;

    for (i = 0; i < NUM_SHARKS; i++) {
        SharkPilot(&(ap->sharks[i]));
        SharkMiss(ap,i);
    }
    WhalePilot(&(ap->dolph));
    ap->dolph.phi++;
    WhalePilot(&(ap->momWhale));
    ap->momWhale.phi++;
    WhalePilot(&(ap->babyWhale));
    ap->babyWhale.phi++;
}

static void
AllDisplay(atlantisstruct *ap)
{
    int i;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (i = 0; i < NUM_SHARKS; i++) {
        glPushMatrix();
        FishTransform(&(ap->sharks[i]));
        DrawShark(&(ap->sharks[i]));
        glPopMatrix();
    }

    glPushMatrix();
    FishTransform(&(ap->dolph));
    DrawDolphin(&(ap->dolph));
    glPopMatrix();

    glPushMatrix();
    FishTransform(&(ap->momWhale));
    DrawWhale(&(ap->momWhale));
    glPopMatrix();

    glPushMatrix();
    FishTransform(&(ap->babyWhale));
    glScalef(0.45, 0.45, 0.3);
    DrawWhale(&(ap->babyWhale));
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
	if (atlantis != NULL) {
		(void) free((void *) atlantis);
		atlantis = NULL;
	}
	FreeAllGL(mi);
}

/*
 *-----------------------------------------------------------------------------
 *    Called when the mainline xlock code notices possible window
 *      damage.  This hook should take steps to repaint the entire
 *      window (no specific damage area information is provided).
 *-----------------------------------------------------------------------------
 */

void
refresh_atlantis(ModeInfo * mi)
{
    MI_CLEARWINDOW(mi);
    Reshape(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
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

#endif	/* USE_GL */
