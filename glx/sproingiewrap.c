
#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)sproingiewrap.c	4.2 97/04/26 xlockmore";

#endif

/*-
 * sproingiewrap.c - Copyright 1996 Sproingie Technologies Incorporated.
 *                   Source and binary freely distributable under the
 *                   terms in xlock.c
 *
 *    Programming:  Ed Mackey, http://www.early.com/~emackey/
 *    Sproingie 3D objects modeled by:  Al Mackey, al@iam.com
 *       (using MetaNURBS in NewTek's Lightwave 3D v5).
 *
 * Revision History:
 * 26-Apr-97: Added glPointSize() calls around explosions, plus other fixes.
 * 28-Mar-97: Added size support.
 * 22-Mar-97: Updated to use glX interface instead of xmesa one.
 *              Also, support for multiscreens added.
 * 20-Mar-97: Updated for xlockmore v4.02alpha7 and higher, using
 *              xlockmore's built-in Mesa/OpenGL support instead of
 *              my own.  Submitted for inclusion in xlockmore.
 * 09-Dec-96: Written.
 *
 * Ed Mackey
 */

/*-
The sproingies have six "real" 
frames, (s1_1 to s1_6) that show a sproingie jumping off a block, headed
down and to the right.  But the program thinks of sproingies as having
twelve "virtual" frames, with the latter six being copies of the first,
only lowered and rotated by 90 degrees (jumping to the left).  So after
going through 12 frames, a sproingie has gone down two rows but not moved
horizontally. 

To have the sproingies randomly choose left/right jumps at each block, the
program should go back to thinking of only 6 frames, and jumping down
only one row when it is done.  Then it can pick a direction for the next
row.

 (Falling off the end might not be so bad either.  :) )  
*/


#include "xlock.h"

#ifdef USE_GL

#define MINSIZE 32

#include <GL/glu.h>
#include <time.h>

ModeSpecOpt sproingies_opts =
{0, NULL, 0, NULL, NULL};

void        NextSproingie(int screen);
void        NextSproingieDisplay(int screen);
void        DisplaySproingies(int screen);
void        ReshapeSproingies(int w, int h);
void        CleanupSproingies(int screen);
void        InitSproingies(int wfmode, int grnd, int mspr, int screen, int numscreens, int mono);

typedef struct {
	GLfloat     view_rotx, view_roty, view_rotz;
	GLint       gear1, gear2, gear3;
	GLfloat     angle;
	GLuint      limit;
	GLuint      count;
	GLXContext  glx_context;
	int         mono;
} sproingiesstruct;

static sproingiesstruct *sproingies = NULL;

static Display *swap_display;
static Window swap_window;

void
SproingieSwap(void)
{
	glFinish();
	glXSwapBuffers(swap_display, swap_window);
}


/* ARGSUSED */
void
draw_sproingies(ModeInfo * mi)
{
	sproingiesstruct *sp = &sproingies[MI_SCREEN(mi)];
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);

	glDrawBuffer(GL_BACK);
	glXMakeCurrent(display, window, sp->glx_context);

	swap_display = display;
	swap_window = window;

	NextSproingieDisplay(MI_SCREEN(mi));	/* It will swap. */
}

void
refresh_sproingies(ModeInfo * mi)
{
	/* No need to do anything here... The whole screen is updated
	 * every frame anyway.  Otherwise this would be just like
	 * draw_sproingies, above, but replace NextSproingieDisplay(...)
	 * with DisplaySproingies(...).
	 */
}

void
release_sproingies(ModeInfo * mi)
{
	if (sproingies != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			sproingiesstruct *sp = &sproingies[screen];

			glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), sp->glx_context);
			CleanupSproingies(MI_SCREEN(mi));
		}

		/* Don't destroy the glXContext.  init_GL does that. */

		(void) free((void *) sproingies);
		sproingies = NULL;
	}
}

void
init_sproingies(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         screen = MI_SCREEN(mi);

	int         cycles = MI_CYCLES(mi);
	int         batchcount = MI_BATCHCOUNT(mi);
	int         size = MI_SIZE(mi);

	sproingiesstruct *sp;
	int         wfmode = 0, grnd, mspr, w, h;

	if (sproingies == NULL) {
		if ((sproingies = (sproingiesstruct *) calloc(MI_NUM_SCREENS(mi),
					 sizeof (sproingiesstruct))) == NULL)
			return;
	}
	sp = &sproingies[screen];

	sp->mono = (MI_WIN_IS_MONO(mi) ? 1 : 0);

	sp->glx_context = init_GL(mi);

	if ((cycles & 1) || MI_WIN_IS_WIREFRAME(mi) || sp->mono)
		wfmode = 1;
	grnd = (cycles >> 1);
	if (grnd > 2)
		grnd = 2;

	mspr = batchcount;
	if (mspr > 100)
		mspr = 100;

	/* wireframe, ground, maxsproingies */
	InitSproingies(wfmode, grnd, mspr, MI_SCREEN(mi), MI_NUM_SCREENS(mi), sp->mono);

	/* Viewport is specified size if size >= MINSIZE && size < screensize */
	if (size == 0) {
		w = MI_WIN_WIDTH(mi);
		h = MI_WIN_HEIGHT(mi);
	} else if (size < MINSIZE) {
		w = MINSIZE;
		h = MINSIZE;
	} else {
		w = (size > MI_WIN_WIDTH(mi)) ? MI_WIN_WIDTH(mi) : size;
		h = (size > MI_WIN_HEIGHT(mi)) ? MI_WIN_HEIGHT(mi) : size;
	}

	glViewport((MI_WIN_WIDTH(mi) - w) / 2, (MI_WIN_HEIGHT(mi) - h) / 2, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(65.0, (GLfloat) w / (GLfloat) h, 0.1, 2000.0);	/* was 200000.0 */
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	swap_display = display;
	swap_window = window;
	DisplaySproingies(MI_SCREEN(mi));
}

#endif

/* End of sproingiewrap.c */
