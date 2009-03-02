/* atlantis --- Shows moving 3D sea animals */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)atlantis.c	5.01 2001/04/17 xlockmore";

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
 * My e-mail address is lassauge@mail.dotcom.fr
 *
 * Eric Lassauge  (May-13-1998)
 *
 * REVISION HISTORY:
 *
 * Jamie Zawinski, 2-Apr-01:  - The fishies were inside out!  The back faces
 *                              were being drawn, not the front faces.
 *                            - Added a texture to simulate light from the
 *                              surface, like in the SGI version.
 * 
 * E.Lassauge - 01/03/01 : Added FPS stuff
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
 *        - add a sort of background image
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
#define PROGCLASS "Atlantis"
#define HACK_INIT init_atlantis
#define HACK_DRAW draw_atlantis
#define atlantis_opts xlockmore_opts
#define DEFAULTS "*delay: 18000 \n" \
 "*count: 4 \n" \
 "*cycles: 100 \n" \
 "*size: 6000 \n" \
 "*showFps:    False \n" \
 "*wireframe:  False \n" \
 "*whalespeed: 250\n"
#include "xlockmore.h"		/* from the xscreensaver distribution */
#else /* !STANDALONE */
#include "xlock.h"		/* from the xlockmore distribution */
#include "visgl.h"
#endif /* !STANDALONE */

#ifdef MODE_atlantis

#include "atlantis.h"
#include <GL/glu.h>
#include "xpm-ximage.h"

#ifdef STANDALONE
#include "../images/sea-texture.xpm"
#else /* !STANDALONE */
#include "pixmaps/sea-texture.xpm"
#endif /* !STANDALONE */

#define DEF_WHALESPEED  "250"
#define DEF_TEXTURE     "True"

static int  whalespeed;
static int  do_texture;

static XrmOptionDescRec opts[] =
{
     {(char *) "-whalespeed", (char *) ".atlantis.whalespeed", XrmoptionSepArg, (caddr_t) NULL},
     {(char *) "-texture",    (char *) ".atlantis.texture",    XrmoptionNoArg, (caddr_t)"True"},
     {(char *) "+texture",    (char *) ".atlantis.texture",    XrmoptionNoArg, (caddr_t)"False"},
};

static argtype vars[] =
{
 {(caddr_t *) & whalespeed, (char *) "whalespeed", (char *) "WhaleSpeed", (char *) DEF_WHALESPEED, t_Int},
 {(caddr_t *) & do_texture, (char *) "texture",    (char *) "Texture",    (char *) DEF_TEXTURE,    t_Bool},
};

static OptionStruct desc[] =
{
	{(char *) "-whalespeed num", (char *) "speed of whales and the dolphin"},
	{(char *) "-texture num",    (char *) "whether to introduce water-like distortion"},
};

ModeSpecOpt atlantis_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   atlantis_description =
{"atlantis", "init_atlantis", "draw_atlantis", "release_atlantis",
 "refresh_atlantis", "change_atlantis", NULL, &atlantis_opts,
 18000, NUM_SHARKS, SHARKSPEED, SHARKSIZE, 64, 1.0, "",
 "Shows moving sharks/whales/dolphin", 0, NULL};

#endif

static atlantisstruct *atlantis = (atlantisstruct *) NULL;

static void
parse_image_data(ModeInfo *mi)
{
  atlantisstruct *ap = &atlantis[MI_SCREEN(mi)];
  ap->texture = xpm_to_ximage (MI_DISPLAY(mi),
                               MI_VISUAL(mi),
                               MI_COLORMAP(mi),
                               sea_texture);
}

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
Init(ModeInfo * mi)
{
        atlantisstruct *ap = &atlantis[MI_SCREEN(mi)];

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

	glFrontFace(GL_CCW);

        if (ap->wire)
          {
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glDisable(GL_LIGHTING);
            glDisable(GL_NORMALIZE);
          }
        else
          {
            glDepthFunc(GL_LEQUAL);
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
            glEnable(GL_NORMALIZE);
            glShadeModel(GL_SMOOTH);

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
          }

        if (ap->wire || !do_texture)
          {
            glDisable(GL_TEXTURE_2D);
          }
        else
          {
            GLfloat s_plane[] = { 1, 0, 0, 0 };
            GLfloat t_plane[] = { 0, 0, 1, 0 };
            GLfloat scale = 0.0005;

            if (!ap->texture)
              parse_image_data (mi);

            clear_gl_error();
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                         ap->texture->width, ap->texture->height, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE,
                         ap->texture->data);
            check_gl_error("texture");

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

            glTexGeni (GL_S, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
            glTexGeni (GL_T, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
            glTexGenfv(GL_S, GL_EYE_PLANE, s_plane);
            glTexGenfv(GL_T, GL_EYE_PLANE, t_plane);

            glEnable(GL_TEXTURE_GEN_S);
            glEnable(GL_TEXTURE_GEN_T);
            glEnable(GL_TEXTURE_2D);

            glMatrixMode(GL_TEXTURE);
            glLoadIdentity();
            glScalef(scale, scale, 1);
            glMatrixMode(GL_MODELVIEW);
          }

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
			atlantis = (atlantisstruct *) NULL;
			return;
		}
	}
	ap->sharkspeed = MI_CYCLES(mi);		/* has influence on the "width"
						   of the movement */
	ap->sharksize = MI_SIZE(mi)?MI_SIZE(mi):SHARKSIZE;	/* has influence on the "distance"
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
		Init(mi);
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

	AllDisplay(ap);
	Animate(ap);
	if (MI_IS_FPS(mi)) do_fps (mi);
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
			if (ap->texture)
			{
				XDestroyImage(ap->texture);
			}
		}
		(void) free((void *) atlantis);
		atlantis = (atlantisstruct *) NULL;
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
	Init(mi);
}

#endif /* MODE_atlantis */
