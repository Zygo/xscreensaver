/* biof --- 3D Bioform */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)biof.c	5.15 2005/02/15 xlockmore";
#endif

/* Copyright (c) E. Lassauge, 2005. */

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
 * The original code is from the really slick screensaver series
 * (see below the original header)
 *
 * Eric Lassauge  (Feb-15-2005) <lassauge AT users.sourceforge.net>
 *
 * REVISION HISTORY:
 *
 * - Eric Lassauge  (Feb-15-2005) <lassauge AT users.sourceforge.net>
 *   Ported the rss_glx version to xlockmore (added random color + wireframe + fps)
 */

/*
 * Copyright (C) 2002  <hk@dgmr.nl>
 * Ported to Linux by Tugrul Galatali <tugrul@galatali.com>
 *
 * BioF is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation.
 *
 * BioF is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef STANDALONE
# define MODE_biof
# define PROGCLASS      "BioF"
# define HACK_INIT      init_biof
# define HACK_DRAW      draw_biof
# define HACK_RESHAPE   reshape_biof
# define biof_opts  xlockmore_opts
# define DEFAULTS       "*delay:       25000 \n" \
                         "*count:          4 \n" \
                         "*showFPS:    False \n" \
                         "*cycles:       100 \n" \
                         "*size:        6000 \n" \
                         "*preset:         5 \n" 

# include "xlockmore.h"         /* from the xscreensaver distribution */
# include <stdio.h>
# include <stdlib.h>
# include <math.h>
#else /* !STANDALONE */
# include "xlock.h"             /* from the xlockmore distribution */
# include "visgl.h"
#endif /* !STANDALONE */

#ifdef MODE_biof
// BioF screen saver

#include <stdlib.h>
#include <GL/gl.h>
#include <GL/glu.h>

#define TRIANGLES 1
#define SPHERES 2
#define BIGSPHERES 3
#define POINTS 4
#define LIGHTMAP 5

//#define COLOR	0.9, 0.75, 0.5
//#define COLOR_RAND(i)	((float) (NRAND(i)) / 100.0)
// Try to keep the same 'weight' on the components:
#define COLOR_RAND(i)	((float) ( (2*i - 100) + NRAND(100 - i) ) / 100.0)
static float C_RED;
static float C_GREEN;
static float C_BLUE;
#define COLOR	C_RED, C_GREEN, C_BLUE

static void handle_opts (ModeInfo * mi);

static XrmOptionDescRec opts[] =
{
        {(char *) "-preset",    (char *) ".biof.preset",    XrmoptionSepArg, (caddr_t)NULL},
        {(char *) "-triangles",    (char *) ".biof.preset",    XrmoptionNoArg, (caddr_t)"1"},
        {(char *) "-spheres",    (char *) ".biof.preset",    XrmoptionNoArg, (caddr_t)"2"},
        {(char *) "-bigspheres",    (char *) ".biof.preset",    XrmoptionNoArg, (caddr_t)"3"},
        {(char *) "-lightmap",    (char *) ".biof.preset",    XrmoptionNoArg, (caddr_t)"5"},
        {(char *) "-lines",    (char *) ".biof.lines",    XrmoptionSepArg, (caddr_t)NULL},
        {(char *) "-points",    (char *) ".biof.points",    XrmoptionSepArg, (caddr_t)NULL},
        {(char *) "-offangle",    (char *) ".biof.offangle",    XrmoptionNoArg, (caddr_t)"True"},
        {(char *) "+offangle",    (char *) ".biof.offangle",    XrmoptionNoArg, (caddr_t)"False"},
};

static int var_preset;
static int var_offangle;
static int var_lines;
static int var_points;

static argtype vars[] =
{
        {(void *) & var_preset, (char *) "preset",    (char *) "Preset",    (char *) "0",    t_Int},
        {(void *) & var_lines, (char *) "lines",    (char *) "Lines",    (char *) "0",    t_Int},
        {(void *) & var_points, (char *) "points",    (char *) "Points",    (char *) "0",    t_Int},
        {(void *) & var_offangle, (char *) "offangle",    (char *) "OffAngle",    (char *) "False",    t_Bool},
};

static OptionStruct desc[] =
{
        {(char *) "-preset num",        (char *) "preset to use [1-5]"},
        {(char *) "-triangles",         (char *) "preset 1"},
        {(char *) "-spheres",           (char *) "preset 2"},
        {(char *) "-bigspheres",        (char *) "preset 3"},
        {(char *) "-lightmap",          (char *) "preset 5"},
        {(char *) "-lines num",         (char *) "use num lines"},
        {(char *) "-points num",        (char *) "use num points"},
        {(char *) "+/-offangle",        (char *) "whether to use OffAngle"},
};

ModeSpecOpt biof_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   cage_description =
{"cage", "init_biof", "draw_biof", "release_biof",
 "draw_biof", "init_biof", (char *) NULL, &cage_opts,
 10000, 800, 1, 0, 1.0, "",
 "Shows the Impossible Cage, an Escher-like GL scene", 0, NULL};

#endif

typedef struct {
        GLint       WinH, WinW;
        GLXContext *glx_context;
	int	    wire;

	int NRPOINTS, NRLINES;
	int dGeometry;
	int dOffAngle;

	GLUquadricObj *shapes;
} biofstruct;

static biofstruct *biof = (biofstruct *) NULL;

static void Horn (ModeInfo * mi, int ribs, double bend, double stack, double twist, double twisttrans, double grow)
{
	biofstruct *bp = &biof[MI_SCREEN(mi)];
	int i;

	for (i = 0; i < ribs; i++) {
		glPushMatrix ();
		glRotated (i * bend / ribs, 1, 0, 0);
		glTranslated (0, 0, i * stack / ribs);
		glTranslated (twisttrans, 0, 0);
		glRotated (i * twist / ribs, 0, 0, 1);
		glTranslated (-twisttrans, 0, 0);

		switch (bp->dGeometry) {
		case TRIANGLES:
			glBegin (bp->wire?GL_LINE_LOOP:GL_TRIANGLES);
			glColor4d (COLOR, 1.0);
			glVertex3d (2.0, 2.0, 0.0);
			glColor4d (COLOR, 0.0);
			glVertex3d (0.0, 2.0, 0.0);
			glVertex3d (2.0, 0.0, 0.0);
			glEnd ();

			break;

		case BIGSPHERES:
			if (!bp->wire)
			{
				gluQuadricDrawStyle (bp->shapes, GLU_FILL);
				gluQuadricNormals (bp->shapes, GLU_SMOOTH);
			}
			else
			{
				gluQuadricDrawStyle (bp->shapes, GLU_LINE);
				gluQuadricNormals (bp->shapes, GLU_NONE);
			}
			gluSphere (bp->shapes, 7 * exp (i / ribs * log (grow)), 20, 10);

			break;

		case SPHERES:
			if (!bp->wire)
			{
				gluQuadricDrawStyle (bp->shapes, GLU_FILL);
				gluQuadricNormals (bp->shapes, GLU_SMOOTH);
			}
			else
			{
				gluQuadricDrawStyle (bp->shapes, GLU_LINE);
				gluQuadricNormals (bp->shapes, GLU_NONE);
			}
			//gluSphere(bp->shapes, power(grow,i/ribs), 5, 5);
			gluSphere (bp->shapes, exp (i / ribs * log (grow)), 6, 4);

			break;

		case LIGHTMAP:
		case POINTS:
			glBegin (GL_POINTS);
			glVertex3d (0, 0, 0);
			glEnd ();

			break;
		}

		glPopMatrix ();
	}
}

void reshape_biof (ModeInfo * mi, int width, int height)
{
	biofstruct *bp = &biof[MI_SCREEN(mi)];

	glViewport (0, 0, bp->WinW = (GLint) width, bp->WinH = (GLint) height);
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();
	gluPerspective (30, (GLdouble) width / (GLdouble) height, 100, 300);
}

void init_biof (ModeInfo * mi)
{
	float glfLightPosition[4] = { 100.0, 100.0, 100.0, 0.0 };
	float glfFog[4] = { 0.0, 0.0, 0.3, 1.0 };
	//float DiffuseLightColor[4] = { 1, 0.8, 0.4, 1.0 };
	float DiffuseLightColor[4] = { 1.0, 1.0, 1.0, 1.0 };
	int i, j;
	double x, y, r, d;
	unsigned char texbuf[64][64];
	int        screen = MI_SCREEN(mi);
        biofstruct *bp;

        if (biof == NULL) {
                if ((biof = (biofstruct *) calloc(MI_NUM_SCREENS(mi),
                                           sizeof (biofstruct))) == NULL)
                        return;
        }
        bp = &biof[screen];
	bp->wire = MI_IS_WIREFRAME(mi);
	C_RED   = COLOR_RAND(90);
	C_GREEN = COLOR_RAND(75);
	C_BLUE  = COLOR_RAND(50);
	DiffuseLightColor[1] = COLOR_RAND(80);
	DiffuseLightColor[2] = COLOR_RAND(40);

	handle_opts(mi);
	if (MI_IS_DEBUG(mi)) {
                (void) fprintf(stderr,
                               "%s:\n\tpreset=%d\n\toffangle=%d\n\tlines=%d\n\tpoints=%d\n\twireframe=%s\n",
                               MI_NAME(mi),
                               bp->dGeometry,
                               bp->dOffAngle,
                               bp->NRLINES,
                               bp->NRPOINTS,
                               bp->wire ? "yes" : "no"
                        );
        }

        if ((bp->glx_context = init_GL(mi)) != NULL) {

		glEnable (GL_DEPTH_TEST);

		if (bp->wire)
		{
			glDisable (GL_BLEND);
		}
		else
		{
			glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glEnable (GL_BLEND);
		}

		if ((bp->dGeometry == SPHERES) || (bp->dGeometry == BIGSPHERES)) {
			if (bp->wire)
			{
				glDisable (GL_CULL_FACE);
				glDisable (GL_LIGHTING);
				glDisable (GL_LIGHT0);
			}
			else
			{
				glEnable (GL_CULL_FACE);
				glCullFace (GL_BACK);

				glLightfv (GL_LIGHT0, GL_POSITION, glfLightPosition);
				glEnable (GL_LIGHTING);
				glEnable (GL_LIGHT0);

				glLightfv (GL_LIGHT0, GL_DIFFUSE, DiffuseLightColor);
			}

			bp->shapes = gluNewQuadric ();
		} else if (bp->dGeometry == POINTS) {
			if (bp->wire)
			{
				glDisable (GL_POINT_SMOOTH);
				glPointSize (3.0);
			}
			else
			{
				glHint (GL_POINT_SMOOTH_HINT, GL_NICEST);
				glPointSize (3.0);
				glEnable (GL_POINT_SMOOTH);

				glFogi (GL_FOG_MODE, GL_LINEAR);
				glHint (GL_FOG_HINT, GL_NICEST);
				glFogfv (GL_FOG_COLOR, glfFog);
				glFogf (GL_FOG_START, 150);
				glFogf (GL_FOG_END, 250);
				glEnable (GL_FOG);
			}
		} else if (bp->dGeometry == LIGHTMAP) { /* Can't be wired ! */
			r = 32;

			memset ((void *)&texbuf, 0, 4096);

			for (i = 0; i < 64; i++) {
				for (j = 0; j < 64; j++) {
					x = abs (i - 32);
					y = abs (j - 32);
					d = sqrt (x * x + y * y);

					if (d < r) {
						d = 1 - (d / r);
						texbuf[i][j] = (char)(255.0f * d * d);
					}
				}
			}

			gluBuild2DMipmaps (GL_TEXTURE_2D, 1, 64, 64, GL_LUMINANCE, GL_UNSIGNED_BYTE, texbuf);

			glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
			glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}

		reshape_biof (mi,MI_WIDTH(mi), MI_HEIGHT(mi));
        } else {
                MI_CLEARWINDOW(mi);
        }

}

void release_biof (ModeInfo * mi)
{
	int         screen;

        if (biof != NULL) {
                for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
                        biofstruct *bp = &biof[screen];
			if ((bp->dGeometry == SPHERES) || (bp->dGeometry == BIGSPHERES))
				gluDeleteQuadric(bp->shapes);
                }
                (void)free((void *)biof);
                biof = (biofstruct *) NULL;
        }
        FreeAllGL(mi);
}

void draw_biof (ModeInfo * mi)
{
	struct timeval now;
	double dt;
	biofstruct *bp = &biof[MI_SCREEN(mi)];
	Display    *display = MI_DISPLAY(mi);
        Window      window = MI_WINDOW(mi);

        MI_IS_DRAWN(mi) = True;

        if (!bp->glx_context)
                return;

        glXMakeCurrent(display, window, *(bp->glx_context));

	GETTIMEOFDAY (&now);

	dt = (now.tv_sec * 1000000 + now.tv_usec) * 0.000001;

	if (bp->dGeometry == LIGHTMAP) {
		int i, j, offset;
		float *fb_buffer;
		double x, y, w;

		if ((fb_buffer = (float *) malloc(bp->NRPOINTS *
				bp->NRLINES * 3 * sizeof (float))) == NULL) {
			return;
		}
		glDisable (GL_DEPTH_TEST);
		glFeedbackBuffer (bp->NRPOINTS * bp->NRLINES * 3, GL_2D, fb_buffer);
		glRenderMode (GL_FEEDBACK);

		glMatrixMode (GL_MODELVIEW);
		glPushMatrix ();
		glLoadIdentity ();

		gluLookAt (200, 0, 0, 0, 0, 0, 0, 0, 1);

		glRotated (90, 0, 1, 0);
		glRotated (10 * dt, 0, 0, 1);

		glNewList (1, GL_COMPILE);
		Horn (mi, bp->NRPOINTS, 315 * sin (dt * 0.237), 45 /* + 20 * sin(dt * 0.133) */ , 1500 * sin (dt * 0.213), 7.5 + 2.5 * sin (dt * 0.173), 1);
		glEndList ();

		if (bp->dOffAngle)
			glRotated (45, 1, 1, 0);

		for (i = 0; i < bp->NRLINES; i++) {
			glPushMatrix ();

			glRotated (360 * i / bp->NRLINES, 0, 0, 1);
			glRotated (45, 1, 0, 0);
			glTranslated (0, 0, 2);

			glCallList (1);

			glPopMatrix ();
		}

		glPopMatrix ();

		glRenderMode (GL_RENDER);

		glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		glMatrixMode (GL_PROJECTION);
		glPushMatrix ();
		glLoadIdentity ();

		glMatrixMode (GL_MODELVIEW);
		glPushMatrix ();
		glLoadIdentity ();

		glScalef (2.0 / bp->WinW, 2.0 / bp->WinH, 1.0);
		glTranslatef (-0.5 * bp->WinW, -0.5 * bp->WinH, 0.0);

		glClear (GL_COLOR_BUFFER_BIT);

		glEnable (GL_TEXTURE_2D);
		glBlendFunc (GL_ONE, GL_ONE);
		glEnable (GL_BLEND);

		glBegin (GL_QUADS);

		w = bp->WinW >> 7;

		for (j = 0; j < bp->NRPOINTS; j++) {
			glColor3f (0.25 - 0.15 * j / bp->NRPOINTS, 0.15 + 0.10 * j / bp->NRPOINTS, 0.06 + 0.14 * j / bp->NRPOINTS);
			for (i = 0; i < bp->NRLINES; i++) {
				offset = 3 * (i * bp->NRPOINTS + j);
				x = fb_buffer[offset + 1];
				y = fb_buffer[offset + 2];

				glTexCoord2f (0.0, 0.0);
				glVertex2f (x - w, y - w);

				glTexCoord2f (1.0, 0.0);
				glVertex2f (x + w, y - w);

				glTexCoord2f (1.0, 1.0);
				glVertex2f (x + w, y + w);

				glTexCoord2f (0.0, 1.0);
				glVertex2f (x - w, y + w);
			}
		}
		if (fb_buffer != NULL)
			free(fb_buffer);
		glEnd ();

		glMatrixMode (GL_MODELVIEW);
		glPopMatrix ();
		glMatrixMode (GL_PROJECTION);
		glPopMatrix ();
	} else {
		int i;

		glClear (GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

		glMatrixMode (GL_MODELVIEW);
		glLoadIdentity ();
		gluLookAt (200, 0, 0, 0, 0, 0, 0, 0, 1);

		glPushMatrix ();
		glRotated (90, 0, 1, 0);
		glRotated (10 * dt, 0, 0, 1);

		glColor3d (COLOR);

		glNewList (1, GL_COMPILE);
		Horn (mi, bp->NRPOINTS, 315 * sin (dt * 0.237), 50 + 20 * sin (dt * 0.133), 1500 * sin (dt * 0.213), 7.5 + 2.5 * sin (dt * 0.173), 0.6 + 0.1 * sin (dt * 0.317));
		glEndList ();

		if (bp->dOffAngle)
			glRotated (45, 1, 1, 0);

		for (i = 0; i < bp->NRLINES; i++) {
			glPushMatrix ();
			glRotated (360 * i / bp->NRLINES, 0, 0, 1);
			glRotated (45, 1, 0, 0);
			glTranslated (0, 0, 2);
			glCallList (1);
			glPopMatrix ();
		}

		glPopMatrix ();
	}

        if (MI_IS_FPS(mi)) do_fps (mi);
        glXSwapBuffers(display, window);
}

static void setDefaults (ModeInfo *mi, int use_preset)
{
	biofstruct *bp = &biof[MI_SCREEN(mi)];
	bp->dGeometry = use_preset;

	switch (use_preset) {
	case TRIANGLES:
		bp->NRPOINTS = 80;
		bp->NRLINES = 32;

		break;
	case SPHERES:
		bp->NRPOINTS = 80;
		bp->NRLINES = 30;

		break;
	case BIGSPHERES:
		bp->NRPOINTS = 20;
		bp->NRLINES = 5;

		break;
	case POINTS:
		bp->NRPOINTS = 250;
		bp->NRLINES = 30;

		break;
	case LIGHTMAP:
		bp->NRPOINTS = 150;
		bp->NRLINES = 20;

		break;
	}
}

static void handle_opts (ModeInfo * mi)
{
	biofstruct *bp = &biof[MI_SCREEN(mi)];
	int change_flag = 0;

	setDefaults(mi,LIGHTMAP);
	bp->dOffAngle = 0;

	if ( (var_preset > 0) && (var_preset < 6))
	{
		change_flag = 1;
		setDefaults (mi, var_preset);
	}
	if (var_lines > 0)
	{
		change_flag = 1;
		bp->NRLINES = var_lines;
	}
	if (var_points > 0)
	{
		change_flag = 1;
		bp->NRPOINTS = var_points;
	}
	if (var_offangle)
	{
		change_flag = 1;
		bp->dOffAngle = 1;
	}

	if (!change_flag) {
		bp->dGeometry = NRAND (5) + 1;
		bp->dOffAngle = NRAND (2);

		switch (bp->dGeometry) {
		case TRIANGLES:
		case SPHERES:
			bp->NRLINES = NRAND (7) + 2;
			bp->NRPOINTS = NRAND (96) + 32;
			break;

		case BIGSPHERES:
			bp->NRLINES = NRAND (7) + 4;
			bp->NRPOINTS = NRAND (32) + 4;
			break;

		case POINTS:
		case LIGHTMAP:
			bp->NRLINES = NRAND (32) + 4;
			bp->NRPOINTS = NRAND (64) + 64;
		}
	}
}
#endif
