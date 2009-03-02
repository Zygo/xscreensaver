/* -*- Mode: C; tab-width: 4 -*- */
/* sproingies.c - 3D sproingies */

#if 0
static const char sccsid[] = "@(#)sproingies.c	4.04 97/07/28 xlockmore";
#endif

/*-
 *  sproingies.c - Copyright 1996 by Ed Mackey, freely distributable.
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
 * Revision History:
 * See sproingiewrap.c
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef STANDALONE
# include "xlockmoreI.h"		/* from the xscreensaver distribution */
#else /* !STANDALONE */
# include "xlock.h"            /* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL

#ifdef HAVE_COCOA
# include <OpenGL/glu.h>
#else
# include <GL/glu.h>
#endif

#include "gllist.h"
#include "sproingies.h"

#define MAXSPROING 100
#define T_COUNT 40
#define BOOM_FRAME 50

struct sPosColor {
	int         x, y, z, frame, life;
	GLfloat     r, g, b;
};

typedef struct {
	int         rotx, roty, dist, wireframe, flatshade, groundlevel,
	            maxsproingies, mono;
	int         sframe, target_rx, target_ry, target_dist, target_count;
	const struct gllist *sproingies[6];
	const struct gllist *SproingieBoom;
	GLuint TopsSides;
	struct sPosColor *positions;
} sp_instance;

static sp_instance *si_list = NULL;
static int  active_screens = 0;

extern const struct gllist *s1_1;
extern const struct gllist *s1_2;
extern const struct gllist *s1_3;
extern const struct gllist *s1_4;
extern const struct gllist *s1_5;
extern const struct gllist *s1_6;
extern const struct gllist *s1_b;

static int
myrand(int range)
{
	return ((int) (((float) range) * LRAND() / (MAXRAND)));
}

static      GLuint
build_TopsSides(int wireframe)
{
	GLuint      dl_num;
	GLfloat     mat_color[4] =
	{0.0, 0.0, 0.0, 1.0};

	dl_num = glGenLists(2);
	if (!dl_num)
		return (0);	/* 0 means out of display lists. */

	/* Surface: Tops */
	glNewList(dl_num, GL_COMPILE);
	mat_color[0] = 0.392157;
	mat_color[1] = 0.784314;
	mat_color[2] = 0.941176;
	if (wireframe)
		glColor3fv(mat_color);
	else {
		glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, mat_color);
	}
	glEndList();

	/* Surface: Sides */
	glNewList(dl_num + 1, GL_COMPILE);
	if (wireframe)
		glColor3fv(mat_color);
	else {
      /* jwz: in wireframe mode, color tops and sides the same. */
      mat_color[0] = 0.156863;
      mat_color[1] = 0.156863;
      mat_color[2] = 0.392157;
		glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, mat_color);
	}
	glEndList();
	return (dl_num);
}

static void
LayGround(int sx, int sy, int sz, int width, int height, sp_instance * si)
{
	int         x, y, z, w, h;
	GLenum      begin_polygon;

	if (si->wireframe)
		begin_polygon = GL_LINE_LOOP;
	else
		begin_polygon = GL_POLYGON;

	if (!si->wireframe) {
		if (!si->mono)
			glCallList(si->TopsSides);	/* Render the tops */
		glNormal3f(0.0, 1.0, 0.0);

		for (h = 0; h < height; ++h) {
			x = sx + h;
			y = sy - (h << 1);
			z = sz + h;
			for (w = 0; w < width; ++w) {
				glBegin(begin_polygon);
				glVertex3i(x, y, z);
				glVertex3i(x, y, z - 1);
				glVertex3i(x + 1, y, z - 1);
				glVertex3i(x + 1, y, z);
				glEnd();
				glBegin(begin_polygon);
				glVertex3i(x + 1, y - 1, z);
				glVertex3i(x + 1, y - 1, z - 1);
				glVertex3i(x + 2, y - 1, z - 1);
				glVertex3i(x + 2, y - 1, z);
				glEnd();
				++x;
				--z;
			}
		}
	}
	if (!si->mono)
		glCallList(si->TopsSides + 1);	/* Render the sides */
	if (!si->wireframe)
		glNormal3f(0.0, 0.0, 1.0);

	for (h = 0; h < height; ++h) {
		x = sx + h;
		y = sy - (h << 1);
		z = sz + h;
		for (w = 0; w < width; ++w) {
			glBegin(begin_polygon);
			glVertex3i(x, y, z);
			glVertex3i(x + 1, y, z);
			glVertex3i(x + 1, y - 1, z);
			glVertex3i(x, y - 1, z);
			glEnd();
			glBegin(begin_polygon);
			glVertex3i(x + 1, y - 1, z);
			glVertex3i(x + 2, y - 1, z);
			glVertex3i(x + 2, y - 2, z);
			glVertex3i(x + 1, y - 2, z);
/*-
 * PURIFY 4.0.1 reports an unitialized memory read on the next line when using
 * MesaGL 2.2 and -mono.  This has been fixed in MesaGL 2.3 and later. */
			glEnd();
			++x;
			--z;
		}
	}

	/* Render the other sides */
	if (!si->wireframe)
		glNormal3f(1.0, 0.0, 0.0);

	for (h = 0; h < height; ++h) {
		x = sx + h;
		y = sy - (h << 1);
		z = sz + h;
		for (w = 0; w < width; ++w) {
			glBegin(begin_polygon);
			glVertex3i(x + 1, y, z);
			glVertex3i(x + 1, y, z - 1);
			glVertex3i(x + 1, y - 1, z - 1);
			glVertex3i(x + 1, y - 1, z);
			glEnd();
			glBegin(begin_polygon);
			glVertex3i(x + 2, y - 1, z);
			glVertex3i(x + 2, y - 1, z - 1);
			glVertex3i(x + 2, y - 2, z - 1);
			glVertex3i(x + 2, y - 2, z);
			glEnd();
			++x;
			--z;
		}
	}

	if (si->wireframe) {
		if (!si->mono)
			glCallList(si->TopsSides);	/* Render the tops */

		for (h = 0; h < height; ++h) {
			x = sx + h;
			y = sy - (h << 1);
			z = sz + h;
			for (w = 0; w < width; ++w) {
				glBegin(begin_polygon);
				glVertex3i(x, y, z);
				glVertex3i(x, y, z - 1);
				glVertex3i(x + 1, y, z - 1);
				glVertex3i(x + 1, y, z);
				glEnd();
				glBegin(begin_polygon);
				glVertex3i(x + 1, y - 1, z);
				glVertex3i(x + 1, y - 1, z - 1);
				glVertex3i(x + 2, y - 1, z - 1);
				glVertex3i(x + 2, y - 1, z);
				glEnd();
				++x;
				--z;
			}
		}
	}
}

#define RESET_SPROINGIE (-30 + myrand(28))

static void
AdvanceSproingie(int t, sp_instance * si)
{
	int         g_higher, g_back, t2;
	struct sPosColor *thisSproingie = &(si->positions[t]);
	struct sPosColor *S2 = &(si->positions[0]);

	if (thisSproingie->life > 0) {
		if ((++(thisSproingie->frame)) > 11) {
			if (thisSproingie->frame >= BOOM_FRAME) {
				if ((thisSproingie->r -= 0.08) < 0.0)
					thisSproingie->r = 0.0;
				if ((thisSproingie->g -= 0.08) < 0.0)
					thisSproingie->g = 0.0;
				if ((thisSproingie->b -= 0.08) < 0.0)
					thisSproingie->b = 0.0;
				if ((--(thisSproingie->life)) < 1) {
					thisSproingie->life = RESET_SPROINGIE;
				}
				return;
			}
			thisSproingie->x += 1;
			thisSproingie->y -= 2;
			thisSproingie->z += 1;
			thisSproingie->frame = 0;

			for (t2 = 0; t2 < si->maxsproingies; ++t2) {
				if ((t2 != t) && (thisSproingie->x == S2->x) &&
				    (thisSproingie->y == S2->y) && (thisSproingie->z == S2->z) &&
				    (S2->life > 10) && (S2->frame < 6)) {
#if 0
					if (thisSproingie->life > S2->life) {
						S2->life = 10;
					} else {
#endif
						if (thisSproingie->life > 10) {
							thisSproingie->life = 10;
							thisSproingie->frame = BOOM_FRAME;
							if ((thisSproingie->r += 0.5) > 1.0)
								thisSproingie->r = 1.0;
							if ((thisSproingie->g += 0.5) > 1.0)
								thisSproingie->g = 1.0;
							if ((thisSproingie->b += 0.5) > 1.0)
								thisSproingie->b = 1.0;
						}
#if 0
					}
#endif
				}
				++S2;
			}
		}
		if (!((thisSproingie->life == 10) &&
		      (thisSproingie->frame > 0) &&
		      (thisSproingie->frame < BOOM_FRAME))) {
			if ((--(thisSproingie->life)) < 1) {
				thisSproingie->life = RESET_SPROINGIE;
			} else if (thisSproingie->life < 9) {
				thisSproingie->frame -= 2;
			}
		}		/* else wait here for frame 0 to come about. */
	} else if (++(thisSproingie->life) >= 0) {
		if (t > 1) {
			g_higher = -3 + myrand(5);
			g_back = -2 + myrand(5);
		} else if (t == 1) {
			g_higher = -2 + myrand(3);
			g_back = -1 + myrand(3);
		} else {
			g_higher = -1;
			g_back = 0;
		}

		thisSproingie->x = (-g_higher - g_back);
		thisSproingie->y = (g_higher << 1);
		thisSproingie->z = (g_back - g_higher);
		thisSproingie->life = 40 + myrand(200);
		thisSproingie->frame = -10;
		thisSproingie->r = (GLfloat) (40 + myrand(200)) / 255.0;
		thisSproingie->g = (GLfloat) (40 + myrand(200)) / 255.0;
		thisSproingie->b = (GLfloat) (40 + myrand(200)) / 255.0;

		for (t2 = 0; t2 < si->maxsproingies; ++t2) {
			if ((t2 != t) && (thisSproingie->x == S2->x) &&
			    (thisSproingie->y == S2->y) && (thisSproingie->z == S2->z) &&
			    (S2->life > 10) && (S2->frame < 0)) {
				/* If one is already being born, just wait. */
				thisSproingie->life = -1;
			}
			++S2;
		}
	}
}

static void
NextSproingie(int screen)
{
	sp_instance *si = &si_list[screen];
	int         ddx, t;
	struct sPosColor *thisSproingie = &(si->positions[0]);

	if (++si->sframe > 11) {
		si->sframe = 0;
		for (t = 0; t < si->maxsproingies; ++t) {
			thisSproingie->x -= 1;
			thisSproingie->y += 2;
			thisSproingie->z -= 1;
			++thisSproingie;
		}
	}
	for (t = 0; t < si->maxsproingies; ++t) {
		AdvanceSproingie(t, si);
	}

	if (si->target_count < 0) {	/* track to current target */
		if (si->target_rx < si->rotx)
			--si->rotx;
		else if (si->target_rx > si->rotx)
			++si->rotx;

		if (si->target_ry < si->roty)
			--si->roty;
		else if (si->target_ry > si->roty)
			++si->roty;

		ddx = (si->target_dist - si->dist) / 8;
		if (ddx)
			si->dist += ddx;
		else if (si->target_dist < si->dist)
			--si->dist;
		else if (si->target_dist > si->dist)
			++si->dist;

		if ((si->target_rx == si->rotx) && (si->target_ry == si->roty) &&
		    (si->target_dist == si->dist)) {
			si->target_count = T_COUNT;
			if (si->target_dist <= 32)
				si->target_count >>= 2;
		}
	} else if (--si->target_count < 0) {	/* make up new target */
		si->target_rx = myrand(100) - 35;
		si->target_ry = -myrand(90);
		si->target_dist = 32 << myrand(2);	/* could be 32, 64, or 128, (previously or 256) */

		if (si->target_dist >= si->dist)	/* no duplicate distances */
			si->target_dist <<= 1;
	}
	/* Otherwise just hang loose for a while here */
}

#ifdef __AUXFUNCS__
void
PrintEm(void)
{
	int         t, count = 0;

	for (t = 0; t < maxsproingies; ++t) {
		if (positions[t].life > 0)
			++count;
	}
	(void) printf("RotX: %d, RotY: %d, Dist: %d.  Targets: X %d, Y %d, D %d.  Visible: %d\n",
		 rotx, roty, dist, target_rx, target_ry, target_dist, count);
}

void
ResetEm(void)
{
	int         t;

	for (t = 0; t < maxsproingies; ++t) {
		positions[t].x = 0;
		positions[t].y = 0;
		positions[t].z = 0;
		positions[t].life = -2;
		positions[t].frame = 0;
	}
}

void
distAdd(void)
{
	if (dist < (1 << 16 << 4))
		dist <<= 1;
}

void
distSubtract(void)
{
	if (dist > 1)
		dist >>= 1;
}

void
rotxAdd(void)
{
	rotx = (rotx + 5) % 360;
}

void
rotxSubtract(void)
{
	rotx = (rotx - 5) % 360;
}

void
rotyAdd(void)
{
	roty = (roty + 5) % 360;
}

void
rotySubtract(void)
{
	roty = (roty - 5) % 360;
}

void
rotxBAdd(void)
{
	rotx = (rotx + 45) % 360;
}

void
rotxBSubtract(void)
{
	rotx = (rotx - 45) % 360;
}

void
rotyBAdd(void)
{
	roty = (roty + 45) % 360;
}

void
rotyBSubtract(void)
{
	roty = (roty - 45) % 360;
}

#endif

static void
RenderSproingie(int t, sp_instance * si)
{
	GLfloat     scale, pointsize, mat_color[4] =
	{0.0, 0.0, 0.0, 1.0};
	GLdouble    clipplane[4] =
	{0.0, 1.0, 0.0, 0.0};
	struct sPosColor *thisSproingie = &(si->positions[t]);

	if (thisSproingie->life < 1)
		return;

	glPushMatrix();

	if (!si->mono) {
		mat_color[0] = thisSproingie->r;
		mat_color[1] = thisSproingie->g;
		mat_color[2] = thisSproingie->b;
		if (si->wireframe)
			glColor3fv(mat_color);
		else {
			glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, mat_color);
		}
	}
	if (thisSproingie->frame < 0) {
		glEnable(GL_CLIP_PLANE0);
		glTranslatef((GLfloat) (thisSproingie->x),
			     (GLfloat) (thisSproingie->y) +
			     ((GLfloat) (thisSproingie->frame) / 9.0),
			     (GLfloat) (thisSproingie->z));
		clipplane[3] = ((GLdouble) (thisSproingie->frame) / 9.0) +
			(si->wireframe ? 0.0 : 0.1);
		glClipPlane(GL_CLIP_PLANE0, clipplane);
/**		glCallList(si->sproingies[0]);*/
/**/	renderList(si->sproingies[0], si->wireframe);
		glDisable(GL_CLIP_PLANE0);
	} else if (thisSproingie->frame >= BOOM_FRAME) {
		glTranslatef((GLfloat) (thisSproingie->x) + 0.5,
			     (GLfloat) (thisSproingie->y) + 0.5,
			     (GLfloat) (thisSproingie->z) - 0.5);
		scale = (GLfloat) (1 << (thisSproingie->frame - BOOM_FRAME));
		glScalef(scale, scale, scale);
		if (!si->wireframe) {
			if (!si->mono)
				glColor3fv(mat_color);
			glDisable(GL_LIGHTING);
		}
		pointsize = (GLfloat) ((BOOM_FRAME + 8) - thisSproingie->frame) -
			(si->dist / 64.0);
		glPointSize((pointsize < 1.0) ? 1.0 : pointsize);
/*-
 * PURIFY 4.0.1 reports an unitialized memory read on the next line when using
 * MesaGL 2.2.  This has been tracked to MesaGL 2.2 src/points.c line 313. */
/**		glCallList(si->SproingieBoom);*/
/**/	renderList(si->SproingieBoom, si->wireframe);
		glPointSize(1.0);
		if (!si->wireframe) {
			glEnable(GL_LIGHTING);
		}
	} else if (thisSproingie->frame > 5) {
		glTranslatef((GLfloat) (thisSproingie->x + 1),
			     (GLfloat) (thisSproingie->y - 1), (GLfloat) (thisSproingie->z - 1));
		glRotatef((GLfloat) - 90.0, 0.0, 1.0, 0.0);
/**		glCallList(si->sproingies[thisSproingie->frame - 6]);*/
/**/	renderList(si->sproingies[thisSproingie->frame - 6], si->wireframe);
	} else {
		glTranslatef((GLfloat) (thisSproingie->x), (GLfloat) (thisSproingie->y),
			     (GLfloat) (thisSproingie->z));
/**		glCallList(si->sproingies[thisSproingie->frame]);*/
/**/	renderList(si->sproingies[thisSproingie->frame], si->wireframe);
	}

	glPopMatrix();

}

static void
ComputeGround(sp_instance * si)
{
	int         g_higher, g_back, g_width, g_height;

	/* higher: x-1, y+2, z-1 */
	/* back: x-1, y, z+1 */

	if (si->groundlevel == 0) {
		g_back = 2;
		g_width = 5;
	} else if (si->groundlevel == 1) {
		g_back = 4;
		g_width = 8;
	} else {
		g_back = 8;
		g_width = 16;
	}

	if ((g_higher = si->dist >> 3) < 4)
		g_higher = 4;
	if (g_higher > 16)
		g_higher = 16;
	g_height = g_higher << 1;

	if (si->rotx < -10)
		g_higher += (g_higher >> 2);
	else if (si->rotx > 10)
		g_higher -= (g_higher >> 2);

#if 0
	if (si->dist > 128) {
		++g_higher;
		++g_back;
		g_back <<= 1;
	} else if (si->dist > 64) {
		++g_higher;
		++g_back;
	} else if (si->dist > 32) {
		/* nothing special */
	} else {
		if (g_higher > 2) {
			g_higher = g_back = 4;
		}
	}
#endif

	/* startx, starty, startz, width, height */
	LayGround((-g_higher - g_back), (g_higher << 1), (g_back - g_higher),
		  (g_width), (g_height), si);
}

void
DisplaySproingies(int screen,int pause)
{
	sp_instance *si = &si_list[screen];
	int         t;
	GLfloat     position[] =
	{8.0, 5.0, -2.0, 0.1};

	if (si->wireframe)
		glClear(GL_COLOR_BUFFER_BIT);
	else
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glPushMatrix();
	glTranslatef(0.0, 0.0, -(GLfloat) (si->dist) / 16.0);	/* viewing transform  */
	glRotatef((GLfloat) si->rotx, 1.0, 0.0, 0.0);
	glRotatef((GLfloat) si->roty, 0.0, 1.0, 0.0);

	if (!si->wireframe)
		glLightfv(GL_LIGHT0, GL_POSITION, position);

#if 0				/* Show light pos */
	glPushMatrix();
	glTranslatef(position[0], position[1], position[2]);
	glColor3f(1.0, 1.0, 1.0);
	if (!si->wireframe) {
		glDisable(GL_LIGHTING);
	}
	glCallList(si->SproingieBoom);
	if (!si->wireframe) {
		glEnable(GL_LIGHTING);
	}
	glPopMatrix();
#endif

	glTranslatef((GLfloat) si->sframe * (-1.0 / 12.0) - 0.75,
		     (GLfloat) si->sframe * (2.0 / 12.0) - 0.5,
		     (GLfloat) si->sframe * (-1.0 / 12.0) + 0.75);

	if (si->wireframe)
		ComputeGround(si);

	for (t = 0; t < si->maxsproingies; ++t) {
		RenderSproingie(t, si);
	}

	if (!si->wireframe)
		ComputeGround(si);

	glPopMatrix();
	glFlush();
}

void
NextSproingieDisplay(int screen,int pause)
{
	NextSproingie(screen);
/*        if (pause) usleep(pause);  don't do this! -jwz */
	DisplaySproingies(screen,pause);
}

void
ReshapeSproingies(int w, int h)
{
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(65.0, (GLfloat) w / (GLfloat) h, 0.1, 2000.0);	/* was 200000.0 */
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void
CleanupSproingies(int screen)
{
	sp_instance *si = &si_list[screen];
/*
	int         t;
	if (si->SproingieBoom) {
		for (t = 0; t < 6; ++t)
			glDeleteLists(si->sproingies[t], 1);

		glDeleteLists(si->TopsSides, 2);
		glDeleteLists(si->SproingieBoom, 1);

		--active_screens;
		si->SproingieBoom = 0;
	}
*/
	if (si->TopsSides) {
		glDeleteLists(si->TopsSides, 2);
	}
	if (si->positions) {
		(void) free((void *) (si->positions));
		si->positions = NULL;
	}
	if ((active_screens == 0) && si_list) {
		(void) free((void *) (si_list));
		si_list = NULL;
	}
}

void
InitSproingies(int wfmode, int grnd, int mspr, int screen, int numscreens,
	       int mono)
{
	GLfloat     ambient[] =
	{0.2, 0.2, 0.2, 1.0};
	GLfloat     position[] =
	{10.0, 1.0, 1.0, 10.0};
	GLfloat     mat_diffuse[] =
	{0.6, 0.6, 0.6, 1.0};
	GLfloat     mat_specular[] =
	{0.8, 0.8, 0.8, 1.0};
	GLfloat     mat_shininess[] =
	{50.0};

	sp_instance *si;
	int         t;

	if (si_list == NULL) {
		if ((si_list = (sp_instance *) calloc(numscreens,
					      sizeof (sp_instance))) == NULL)
			return;
	}
	si = &si_list[screen];

	active_screens++;
	CleanupSproingies(screen);

	if (mspr < 0)
		mspr = 0;
	if (mspr >= MAXSPROING)
		mspr = MAXSPROING - 1;

	si->rotx = 0;
	si->roty = -45;
	si->dist = (16 << 2);
	si->sframe = 0;
	si->target_count = 0;
	si->mono = mono;

	si->wireframe = si->flatshade = 0;

	if (wfmode == 2)
		si->flatshade = 1;
	else if (wfmode)
		si->wireframe = 1;

	si->groundlevel = grnd;
	si->maxsproingies = mspr;

	if (si->maxsproingies) {
		si->positions = (struct sPosColor *) calloc(si->maxsproingies,
						  sizeof (struct sPosColor));

		if (!(si->positions))
			si->maxsproingies = 0;
	}
	for (t = 0; t < si->maxsproingies; ++t) {
		si->positions[t].x = 0;
		si->positions[t].y = 0;
		si->positions[t].z = 0;
		si->positions[t].life = (-t * ((si->maxsproingies > 19) ? 1 : 4)) - 2;
		si->positions[t].frame = 0;
	}

#if 0				/* Test boom */
	si->positions[0].x = 0;
	si->positions[0].y = 0;
	si->positions[0].z = 0;
	si->positions[0].life = 10;
	si->positions[0].frame = BOOM_FRAME;
	si->positions[0].r = 0.656863;
	si->positions[0].g = 1.0;
	si->positions[0].b = 0.656863;
#endif

	if (!(si->TopsSides = build_TopsSides(si->wireframe)))
		(void) fprintf(stderr, "build_TopsSides\n");
/*
	if (!(si->sproingies[0] = BuildLWO(si->wireframe, &LWO_s1_1)))
		(void) fprintf(stderr, "BuildLWO - 1\n");
	if (!(si->sproingies[1] = BuildLWO(si->wireframe, &LWO_s1_2)))
		(void) fprintf(stderr, "BuildLWO - 2\n");
	if (!(si->sproingies[2] = BuildLWO(si->wireframe, &LWO_s1_3)))
		(void) fprintf(stderr, "BuildLWO - 3\n");
	if (!(si->sproingies[3] = BuildLWO(si->wireframe, &LWO_s1_4)))
		(void) fprintf(stderr, "BuildLWO - 4\n");
	if (!(si->sproingies[4] = BuildLWO(si->wireframe, &LWO_s1_5)))
		(void) fprintf(stderr, "BuildLWO - 5\n");
	if (!(si->sproingies[5] = BuildLWO(si->wireframe, &LWO_s1_6)))
		(void) fprintf(stderr, "BuildLWO - 6\n");

	if (!(si->SproingieBoom = BuildLWO(si->wireframe, &LWO_s1_b)))
		(void) fprintf(stderr, "BuildLWO - b\n");
*/
	si->sproingies[0]=s1_1;
	si->sproingies[1]=s1_2;
	si->sproingies[2]=s1_3;
	si->sproingies[3]=s1_4;
	si->sproingies[4]=s1_5;
	si->sproingies[5]=s1_6;
	si->SproingieBoom=s1_b;

	if (si->wireframe) {
		glShadeModel(GL_FLAT);
		glDisable(GL_LIGHTING);
	} else {
		if (si->flatshade) {
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

		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diffuse);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, mat_shininess);

		/* glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE); */
		glCullFace(GL_BACK);
		glEnable(GL_CULL_FACE);

		glFrontFace(GL_CW);
		/* glEnable(GL_NORMALIZE); */
	}
}

#endif

/* End of sproingies.c */
