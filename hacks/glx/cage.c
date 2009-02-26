/* -*- Mode: C; tab-width: 4 -*- */
/* cage --- the Impossible Cage, an Escher like scene. */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)cage.c	4.07 98/01/04 xlockmore";

#endif

#undef DEBUG_LISTS

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
 * The RotateAroundU() routine was adapted from the book
 *    "Computer Graphics Principles and Practice 
 *     Foley - vanDam - Feiner - Hughes
 *     Second Edition" Pag. 227, exercise 5.15.
 * 
 * This mode shows some interesting scenes that are impossible OR very
 * wierd to build in the real universe. Much of the scenes are inspirated
 * on Mauritz Cornelis Escher's works which derivated the mode's name.
 * M.C. Escher (1898-1972) was a dutch artist and many people prefer to
 * say he was a mathematician.
 *
 * Thanks goes to Brian Paul for making it possible and inexpensive to use 
 * OpenGL at home.
 *
 * Since I'm not a native English speaker, my apologies for any grammatical
 * mistake.
 *
 * My e-mail address is
 * m-vianna@usa.net
 *
 * Marcelo F. Vianna (Jun-01-1997)
 *
 * Revision History:
 * 01-Jan-98: Mode separated from escher and renamed
 * 08-Jun-97: New scene implemented: "Impossible Cage" based in a M.C. Escher's
 *            painting with the same name (quite similar). The first GL mode
 *            to use texture mapping.
 *            The "Impossible Cage" scene doesn't use DEPTH BUFFER, the 
 *            wood planks are drawn consistently using GL_CULL_FACE, and
 *            the painter's algorithm is used to sort the planks.
 *            Marcelo F. Vianna.
 * 07-Jun-97: Speed ups in Moebius Strip using GL_CULL_FACE.
 *            Marcelo F. Vianna.
 * 03-Jun-97: Initial Release (Only one scene: "Moebius Strip")
 *            The Moebius Strip scene was inspirated in a M.C. Escher's
 *            painting named Moebius Strip II in wich ants walk across a
 *            Moebius Strip path, sometimes meeting each other and sometimes
 *            being in "opposite faces" (note that the moebius strip has
 *            only one face and one edge).
 *            Marcelo F. Vianna.
 *
 */

/*-
 * Texture mapping is only available on RGBA contexts, Mono and color index
 * visuals DO NOT support texture mapping in OpenGL.
 *
 * BUT Mesa do implements RGBA contexts in pseudo color visuals, so texture
 * mapping shuld work on PseudoColor, DirectColor, TrueColor using Mesa. Mono
 * is not officially supported for both OpenGL and Mesa, but seems to not crash
 * Mesa.
 *
 * In real OpenGL, PseudoColor DO NOT support texture map (as far as I know).
 */

#include <X11/Intrinsic.h>

#ifdef STANDALONE
# define PROGCLASS			"Cage"
# define HACK_INIT			init_cage
# define HACK_DRAW			draw_cage
# define cage_opts			xlockmore_opts
# define DEFAULTS			"*cycles:		1       \n"			\
							"*delay:		1000    \n"			\
							"*wireframe:	False	\n"
# include "xlockmore.h"		/* from the xscreensaver distribution */
#else /* !STANDALONE */
# include "xlock.h"		/* from the xlockmore distribution */

#endif /* !STANDALONE */

#ifdef USE_GL


#include <GL/glu.h>
#include "e_textures.h"

ModeSpecOpt cage_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   cage_description =
{"cage", "init_cage", "draw_cage", "release_cage",
 "draw_cage", "change_cage", NULL, &cage_opts,
 1000, 1, 1, 1, 1.0, 4, "",
 "Shows the Impossible Cage, an Escher-like GL scene", 0, NULL};

#endif

#define Scale4Window               0.3
#define Scale4Iconic               0.4

#define sqr(A)                     ((A)*(A))

#ifndef Pi
#define Pi                         M_PI
#endif

/*************************************************************************/

typedef struct {
	GLint       WindH, WindW;
	GLfloat     step;
	int         AreObjectsDefined[1];
	GLXContext *glx_context;
} cagestruct;

static float front_shininess[] =
{60.0};
static float front_specular[] =
{0.7, 0.7, 0.7, 1.0};
static float ambient[] =
{0.0, 0.0, 0.0, 1.0};
static float diffuse[] =
{1.0, 1.0, 1.0, 1.0};
static float position0[] =
{1.0, 1.0, 1.0, 0.0};
static float position1[] =
{-1.0, -1.0, 1.0, 0.0};
static float lmodel_ambient[] =
{0.5, 0.5, 0.5, 1.0};
static float lmodel_twoside[] =
{GL_TRUE};

static float MaterialWhite[] =
{0.7, 0.7, 0.7, 1.0};

static cagestruct *cage = NULL;
static GLuint objects;

#define ObjWoodPlank    0

#define PlankWidth      3.0
#define PlankHeight     0.35
#define PlankThickness  0.15

static void
draw_woodplank(cagestruct * cp)
{
	if (!cp->AreObjectsDefined[ObjWoodPlank]) {
		glNewList(objects + ObjWoodPlank, GL_COMPILE_AND_EXECUTE);
		glBegin(GL_QUADS);
		glNormal3f(0, 0, 1);
		glTexCoord2f(0, 0);
		glVertex3f(-PlankWidth, -PlankHeight, PlankThickness);
		glTexCoord2f(1, 0);
		glVertex3f(PlankWidth, -PlankHeight, PlankThickness);
		glTexCoord2f(1, 1);
		glVertex3f(PlankWidth, PlankHeight, PlankThickness);
		glTexCoord2f(0, 1);
		glVertex3f(-PlankWidth, PlankHeight, PlankThickness);
		glNormal3f(0, 0, -1);
		glTexCoord2f(0, 0);
		glVertex3f(-PlankWidth, PlankHeight, -PlankThickness);
		glTexCoord2f(1, 0);
		glVertex3f(PlankWidth, PlankHeight, -PlankThickness);
		glTexCoord2f(1, 1);
		glVertex3f(PlankWidth, -PlankHeight, -PlankThickness);
		glTexCoord2f(0, 1);
		glVertex3f(-PlankWidth, -PlankHeight, -PlankThickness);
		glNormal3f(0, 1, 0);
		glTexCoord2f(0, 0);
		glVertex3f(-PlankWidth, PlankHeight, PlankThickness);
		glTexCoord2f(1, 0);
		glVertex3f(PlankWidth, PlankHeight, PlankThickness);
		glTexCoord2f(1, 1);
		glVertex3f(PlankWidth, PlankHeight, -PlankThickness);
		glTexCoord2f(0, 1);
		glVertex3f(-PlankWidth, PlankHeight, -PlankThickness);
		glNormal3f(0, -1, 0);
		glTexCoord2f(0, 0);
		glVertex3f(-PlankWidth, -PlankHeight, -PlankThickness);
		glTexCoord2f(1, 0);
		glVertex3f(PlankWidth, -PlankHeight, -PlankThickness);
		glTexCoord2f(1, 1);
		glVertex3f(PlankWidth, -PlankHeight, PlankThickness);
		glTexCoord2f(0, 1);
		glVertex3f(-PlankWidth, -PlankHeight, PlankThickness);
		glNormal3f(1, 0, 0);
		glTexCoord2f(0, 0);
		glVertex3f(PlankWidth, -PlankHeight, PlankThickness);
		glTexCoord2f(1, 0);
		glVertex3f(PlankWidth, -PlankHeight, -PlankThickness);
		glTexCoord2f(1, 1);
		glVertex3f(PlankWidth, PlankHeight, -PlankThickness);
		glTexCoord2f(0, 1);
		glVertex3f(PlankWidth, PlankHeight, PlankThickness);
		glNormal3f(-1, 0, 0);
		glTexCoord2f(0, 0);
		glVertex3f(-PlankWidth, PlankHeight, PlankThickness);
		glTexCoord2f(1, 0);
		glVertex3f(-PlankWidth, PlankHeight, -PlankThickness);
		glTexCoord2f(1, 1);
		glVertex3f(-PlankWidth, -PlankHeight, -PlankThickness);
		glTexCoord2f(0, 1);
		glVertex3f(-PlankWidth, -PlankHeight, PlankThickness);
		glEnd();
		glEndList();
		cp->AreObjectsDefined[ObjWoodPlank] = 1;
#ifdef DEBUG_LISTS
		(void) printf("WoodPlank drawn SLOWLY\n");
#endif
	} else {
		glCallList(objects + ObjWoodPlank);
#ifdef DEBUG_LISTS
		(void) printf("WoodPlank drawn quickly\n");
#endif
	}
}

static void
draw_impossiblecage(cagestruct * cp)
{
	glPushMatrix();
	glRotatef(90, 0, 1, 0);
	glTranslatef(0.0, PlankHeight - PlankWidth, -PlankThickness - PlankWidth);
	draw_woodplank(cp);
	glPopMatrix();
	glPushMatrix();
	glRotatef(90, 0, 0, 1);
	glTranslatef(0.0, PlankHeight - PlankWidth, PlankWidth - PlankThickness);
	draw_woodplank(cp);
	glPopMatrix();
	glPushMatrix();
	glRotatef(90, 0, 1, 0);
	glTranslatef(0.0, PlankWidth - PlankHeight, -PlankThickness - PlankWidth);
	draw_woodplank(cp);
	glPopMatrix();
	glPushMatrix();
	glTranslatef(0.0, PlankWidth - PlankHeight, 3 * PlankThickness - PlankWidth);
	draw_woodplank(cp);
	glPopMatrix();
	glPushMatrix();
	glRotatef(90, 0, 0, 1);
	glTranslatef(0.0, PlankWidth - PlankHeight, PlankWidth - PlankThickness);
	draw_woodplank(cp);
	glPopMatrix();
	glPushMatrix();
	glTranslatef(0.0, PlankWidth - PlankHeight, PlankWidth - 3 * PlankThickness);
	draw_woodplank(cp);
	glPopMatrix();
	glPushMatrix();
	glTranslatef(0.0, PlankHeight - PlankWidth, 3 * PlankThickness - PlankWidth);
	draw_woodplank(cp);
	glPopMatrix();
	glPushMatrix();
	glRotatef(90, 0, 0, 1);
	glTranslatef(0.0, PlankHeight - PlankWidth, PlankThickness - PlankWidth);
	draw_woodplank(cp);
	glPopMatrix();
	glPushMatrix();
	glTranslatef(0.0, PlankHeight - PlankWidth, PlankWidth - 3 * PlankThickness);
	draw_woodplank(cp);
	glPopMatrix();
	glPushMatrix();
	glRotatef(90, 0, 1, 0);
	glTranslatef(0.0, PlankHeight - PlankWidth, PlankWidth + PlankThickness);
	draw_woodplank(cp);
	glPopMatrix();
	glPushMatrix();
	glRotatef(90, 0, 0, 1);
	glTranslatef(0.0, PlankWidth - PlankHeight, PlankThickness - PlankWidth);
	draw_woodplank(cp);
	glPopMatrix();
	glPushMatrix();
	glRotatef(90, 0, 1, 0);
	glTranslatef(0.0, PlankWidth - PlankHeight, PlankWidth + PlankThickness);
	draw_woodplank(cp);
	glPopMatrix();
}

static void
reshape(ModeInfo * mi, int width, int height)
{
	cagestruct *cp = &cage[MI_SCREEN(mi)];

	glViewport(0, 0, cp->WindW = (GLint) width, cp->WindH = (GLint) height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(-1.0, 1.0, -1.0, 1.0, 5.0, 15.0);
	glMatrixMode(GL_MODELVIEW);
	if (width >= 1024) {
		glLineWidth(3);
		glPointSize(3);
	} else if (width >= 512) {
		glLineWidth(2);
		glPointSize(2);
	} else {
		glLineWidth(1);
		glPointSize(1);
	}
	cp->AreObjectsDefined[ObjWoodPlank] = 0;
}

static void
pinit(void)
{
	glClearDepth(1.0);
	glClearColor(0.0, 0.0, 0.0, 1.0);

	glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
	glLightfv(GL_LIGHT0, GL_POSITION, position0);
	glLightfv(GL_LIGHT1, GL_AMBIENT, ambient);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuse);
	glLightfv(GL_LIGHT1, GL_POSITION, position1);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);
	glLightModelfv(GL_LIGHT_MODEL_TWO_SIDE, lmodel_twoside);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHT1);
	glEnable(GL_NORMALIZE);
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);

	/* cage */
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialWhite);
	glShadeModel(GL_FLAT);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_CULL_FACE);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	gluBuild2DMipmaps(GL_TEXTURE_2D, 3, WoodTextureWidth, WoodTextureHeight,
			  GL_RGB, GL_UNSIGNED_BYTE, WoodTextureData);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, front_shininess);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, front_specular);
}

void
init_cage(ModeInfo * mi)
{
	int         screen = MI_SCREEN(mi);
	cagestruct *cp;

	if (cage == NULL) {
		if ((cage = (cagestruct *) calloc(MI_NUM_SCREENS(mi),
					       sizeof (cagestruct))) == NULL)
			return;
	}
	cp = &cage[screen];
	cp->step = NRAND(90);

	if ((cp->glx_context = init_GL(mi)) != NULL) {

		reshape(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
		glDrawBuffer(GL_BACK);
		if (!glIsList(objects))
			objects = glGenLists(1);
		pinit();
	} else {
		MI_CLEARWINDOW(mi);
	}
}

void
draw_cage(ModeInfo * mi)
{
	cagestruct *cp = &cage[MI_SCREEN(mi)];

	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);

	MI_IS_DRAWN(mi) = True;

	if (!cp->glx_context)
		return;

	glXMakeCurrent(display, window, *(cp->glx_context));

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glPushMatrix();

	glTranslatef(0.0, 0.0, -10.0);

	if (!MI_IS_ICONIC(mi)) {
		glScalef(Scale4Window * cp->WindH / cp->WindW, Scale4Window, Scale4Window);
	} else {
		glScalef(Scale4Iconic * cp->WindH / cp->WindW, Scale4Iconic, Scale4Iconic);
	}

	/* cage */
	glRotatef(cp->step * 100, 0, 0, 1);
	glRotatef(25 + cos(cp->step * 5) * 6, 1, 0, 0);
	glRotatef(204.5 - sin(cp->step * 5) * 8, 0, 1, 0);
	draw_impossiblecage(cp);

	glPopMatrix();

	glFlush();

	glXSwapBuffers(display, window);

	cp->step += 0.025;
}

void
change_cage(ModeInfo * mi)
{
	cagestruct *cp = &cage[MI_SCREEN(mi)];

	if (!cp->glx_context)
		return;

	glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(cp->glx_context));
	pinit();
}

void
release_cage(ModeInfo * mi)
{
	if (cage != NULL) {
		(void) free((void *) cage);
		cage = NULL;
	}
	if (glIsList(objects)) {
		glDeleteLists(objects, 1);
	}
	FreeAllGL(mi);
}

#endif
