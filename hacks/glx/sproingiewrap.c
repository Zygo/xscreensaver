/* -*- Mode: C; tab-width: 4 -*- */
/* sproingiewrap.c - sproingies wrapper */

#if 0
static const char sccsid[] = "@(#)sproingiewrap.c	4.07 97/11/24 xlockmore";
#endif

/*-
 * sproingiewrap.c - Copyright 1996 Sproingie Technologies Incorporated.
 *
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
 *    Programming:  Ed Mackey, http://www.netaxs.com/~emackey/
 *                  Gordon Wrigley, gdw33@student.canterbury.ac.nz
 *                  Sergio Gutiérrez "Sergut", sergut@gmail.com
 *    Sproingie 3D objects modeled by:  Al Mackey, al@iam.com
 *       (using MetaNURBS in NewTek's Lightwave 3D v5).
 *
 * Revision History:
 * 01-Sep-06: Make the sproingies select a new direction after each hop
 *              (6 frames); if they fall towards the edge, they explode.
 *              (TODO: let them fall for a time before they explode or
 *               disappear) [sergut]
 * 13-Dec-02: Changed triangle normals into vertex normals to give a smooth
 *              apperance and moved the sproingies from Display Lists to
 *              Vertex Arrays, still need to do this for the TopsSides.
 *              [gordon]
 * 26-Apr-97: Added glPointSize() calls around explosions, plus other fixes.
 * 28-Mar-97: Added size support.
 * 22-Mar-97: Updated to use glX interface instead of xmesa one.
 *              Also, support for multiscreens added.
 * 20-Mar-97: Updated for xlockmore v4.02alpha7 and higher, using
 *              xlockmore's built-in Mesa/OpenGL support instead of
 *              my own.  Submitted for inclusion in xlockmore.
 * 09-Dec-96: Written.
 */

#ifdef STANDALONE
# define DEFAULTS	"*delay:		30000   \n"			\
					"*count:		8       \n"			\
					"*size:			0       \n"			\
					"*showFPS:      False   \n"			\
					"*fpsTop:       True    \n"			\
					"*wireframe:	False	\n"

# define release_sproingies 0
# define sproingies_handle_event 0
# include "xlockmore.h"				/* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"					/* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL

#define DEF_SMART_SPROINGIES "True" /* Smart sproingies do not fall down the edge */

#include "sproingies.h"

static XrmOptionDescRec opts[] = {
    {"-fall",     ".smartSproingies",  XrmoptionNoArg,  "False"},
    {"-no-fall",  ".smartSproingies",  XrmoptionNoArg,  "True"},
};

static int smrt_spr;

static argtype vars[] = {
    {&smrt_spr,  "smartSproingies",  "Boolean",  DEF_SMART_SPROINGIES,  t_Bool},
};

ENTRYPOINT ModeSpecOpt sproingies_opts =
{countof(opts), opts, countof(vars), vars, NULL};

#ifdef USE_MODULES
ModStruct   sproingies_description =
{"sproingies", "init_sproingies", "draw_sproingies", NULL,
 "refresh_sproingies", "init_sproingies", "free_sproingies", &sproingies_opts,
 1000, 5, 0, 400, 4, 1.0, "",
 "Shows Sproingies!  Nontoxic.  Safe for pets and small children", 0, NULL};

#endif

#define MINSIZE 32

#include <time.h>

typedef struct {
	GLfloat     view_rotx, view_roty, view_rotz;
	GLint       gear1, gear2, gear3;
	GLfloat     angle;
	GLuint      limit;
	GLuint      count;
	GLXContext *glx_context;
	int         mono;
	Window      window;
	sp_instance si;
} sproingiesstruct;

static sproingiesstruct *sproingies = NULL;


ENTRYPOINT void
init_sproingies (ModeInfo * mi)
{
	Window      window = MI_WINDOW(mi);
	int         screen = MI_SCREEN(mi);

	int         count = MI_COUNT(mi);
	int         size = MI_SIZE(mi);

	sproingiesstruct *sp;
	int         wfmode = 0, grnd = 0, mspr, w, h;

	MI_INIT (mi, sproingies);
	sp = &sproingies[screen];

	sp->mono = (MI_IS_MONO(mi) ? 1 : 0);
	sp->window = window;
	if ((sp->glx_context = init_GL(mi)) != NULL) {

		if (MI_IS_WIREFRAME(mi))
			wfmode = 1;

		if (grnd > 2)
			grnd = 2;

		mspr = count;
		if (mspr > 100)
			mspr = 100;

		/* wireframe, ground, maxsproingies */
		InitSproingies(&sp->si, wfmode, grnd, mspr, smrt_spr, sp->mono);

		/* Viewport is specified size if size >= MINSIZE && size < screensize */
		if (size == 0) {
			w = MI_WIDTH(mi);
			h = MI_HEIGHT(mi);
		} else if (size < MINSIZE) {
			w = MINSIZE;
			h = MINSIZE;
		} else {
			w = (size > MI_WIDTH(mi)) ? MI_WIDTH(mi) : size;
			h = (size > MI_HEIGHT(mi)) ? MI_HEIGHT(mi) : size;
		}

		glViewport((MI_WIDTH(mi) - w) / 2, (MI_HEIGHT(mi) - h) / 2, w, h);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(65.0, (GLfloat) w / (GLfloat) h, 0.1, 2000.0);	/* was 200000.0 */
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		DisplaySproingies(&sp->si);

	} else {
		MI_CLEARWINDOW(mi);
	}
}

/* ARGSUSED */
ENTRYPOINT void
draw_sproingies (ModeInfo * mi)
{
	sproingiesstruct *sp = &sproingies[MI_SCREEN(mi)];
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);

	if (!sp->glx_context)
		return;

	glDrawBuffer(GL_BACK);
	glXMakeCurrent(display, window, *sp->glx_context);

    glPushMatrix();
    glRotatef(current_device_rotation(), 0, 0, 1);
	NextSproingieDisplay(&sp->si);	/* It will swap. */
    glPopMatrix();

    if (mi->fps_p) do_fps (mi);
    glFinish();
    glXSwapBuffers(MI_DISPLAY(mi), MI_WINDOW(mi));
}

#ifndef STANDALONE
ENTRYPOINT void
refresh_sproingies(ModeInfo * mi)
{
	/* No need to do anything here... The whole screen is updated
	 * every frame anyway.  Otherwise this would be just like
	 * draw_sproingies, above, but replace NextSproingieDisplay(...)
	 * with DisplaySproingies(...).
	 */
}
#endif /* !STANDALONE */

ENTRYPOINT void
reshape_sproingies (ModeInfo *mi, int w, int h)
{
  ReshapeSproingies(w, h);
}


ENTRYPOINT void
free_sproingies (ModeInfo * mi)
{
	sproingiesstruct *sp = &sproingies[MI_SCREEN(mi)];
    if (!sp->glx_context) return;
	glXMakeCurrent (MI_DISPLAY(mi), MI_WINDOW(mi), *sp->glx_context);
    CleanupSproingies(&sp->si);
}

XSCREENSAVER_MODULE ("Sproingies", sproingies)

#endif /* USE_GL */

/* End of sproingiewrap.c */
