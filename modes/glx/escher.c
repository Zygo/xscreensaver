/* -*- Mode: C; tab-width: 4 -*- */
/* escher --- Escher like scenes. */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)escher.c	4.04 97/07/28 xlockmore";

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
 * My e-mail addresses are
 * vianna@cat.cbpf.br 
 *         and
 * marcelo@venus.rdc.puc-rio.br
 *
 * Marcelo F. Vianna (Jun-01-1997)
 *
 * Revision History:
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
 *            The Moebious Strip scene was inspirated in a M.C. Escher's
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
#define PROGCLASS "Escher"
#define HACK_INIT init_escher
#define HACK_DRAW draw_escher
#define escher_opts xlockmore_opts
#define DEFAULTS "*delay: 1000 \n" \
 "*count: 0 \n"
#include "xlockmore.h"		/* from the xscreensaver distribution */
#else /* !STANDALONE */
#include "xlock.h"		/* from the xlockmore distribution */

#endif /* !STANDALONE */

#ifdef USE_GL


#include <GL/glu.h>
#include "e_textures.h"

#define DEF_SOLIDMOEBIUS  "False"
#define DEF_NOANTS  "False"

static int  solidmoebius;
static int  noants;

static XrmOptionDescRec opts[] =
{
   {"-solidmoebius", ".escher.solidmoebius", XrmoptionNoArg, (caddr_t) "on"},
  {"+solidmoebius", ".escher.solidmoebius", XrmoptionNoArg, (caddr_t) "off"},
	{"-noants", ".escher.noants", XrmoptionNoArg, (caddr_t) "on"},
	{"+noants", ".escher.noants", XrmoptionNoArg, (caddr_t) "off"}
};
static argtype vars[] =
{
	{(caddr_t *) & solidmoebius, "solidmoebius", "Solidmoebius", DEF_SOLIDMOEBIUS, t_Bool},
	{(caddr_t *) & noants, "noants", "Noants", DEF_NOANTS, t_Bool}
};
static OptionStruct desc[] =
{
	{"-/+solidmoebius", "select between a SOLID or a NET Moebius Strip"},
	{"-/+noants", "turn on/off walking ants"}
};

ModeSpecOpt escher_opts =
{4, opts, 2, vars, desc};

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
	GLfloat     ant_position;
	int         scene;
	int         AreObjectsDefined[3];
	GLXContext  glx_context;
} escherstruct;

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

static float MaterialRed[] =
{0.7, 0.0, 0.0, 1.0};
static float MaterialGreen[] =
{0.1, 0.5, 0.2, 1.0};
static float MaterialBlue[] =
{0.0, 0.0, 0.7, 1.0};
static float MaterialCyan[] =
{0.2, 0.5, 0.7, 1.0};
static float MaterialYellow[] =
{0.7, 0.7, 0.0, 1.0};
static float MaterialMagenta[] =
{0.6, 0.2, 0.5, 1.0};
static float MaterialWhite[] =
{0.7, 0.7, 0.7, 1.0};
static float MaterialGray[] =
{0.2, 0.2, 0.2, 1.0};
static float MaterialGray5[] =
{0.5, 0.5, 0.5, 1.0};
static float MaterialGray6[] =
{0.6, 0.6, 0.6, 1.0};
static float MaterialGray8[] =
{0.8, 0.8, 0.8, 1.0};

static escherstruct *escher = NULL;
static GLuint objects;

#define NUM_SCENES      2

#define ObjMoebiusStrip 0
#define ObjAntBody      1
#define ObjWoodPlank    2

#define PlankWidth      3.0
#define PlankHeight     0.35
#define PlankThickness  0.15

static void
mySphere(float radius)
{
	GLUquadricObj *quadObj;

	quadObj = gluNewQuadric();
	gluQuadricDrawStyle(quadObj, (GLenum) GLU_FILL);
	gluSphere(quadObj, radius, 16, 16);
	gluDeleteQuadric(quadObj);
}

static void
myCone(float radius)
{
	GLUquadricObj *quadObj;

	quadObj = gluNewQuadric();
	gluQuadricDrawStyle(quadObj, (GLenum) GLU_FILL);
	gluCylinder(quadObj, radius, 0, radius * 3, 8, 1);
	gluDeleteQuadric(quadObj);
}

static void
draw_escher_ant(escherstruct * ep, float *Material, int mono)
{
	static float ant_step = 0;
	float       cos1 = cos(ant_step);
	float       cos2 = cos(ant_step + 2 * Pi / 3);
	float       cos3 = cos(ant_step + 4 * Pi / 3);
	float       sin1 = sin(ant_step);
	float       sin2 = sin(ant_step + 2 * Pi / 3);
	float       sin3 = sin(ant_step + 4 * Pi / 3);

	if (mono)
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGray5);
	else
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, Material);
	if (!ep->AreObjectsDefined[ObjAntBody]) {
		glNewList(objects + ObjAntBody, GL_COMPILE_AND_EXECUTE);
		glEnable(GL_CULL_FACE);
		glPushMatrix();
		glScalef(1, 1.3, 1);
		mySphere(0.18);
		glScalef(1, 1 / 1.3, 1);
		glTranslatef(0.00, 0.30, 0.00);
		mySphere(0.2);

		glTranslatef(-0.05, 0.17, 0.05);
		glRotatef(-90, 1, 0, 0);
		glRotatef(-25, 0, 1, 0);
		myCone(0.05);
		glTranslatef(0.00, 0.10, 0.00);
		myCone(0.05);
		glRotatef(25, 0, 1, 0);
		glRotatef(90, 1, 0, 0);

		glScalef(1, 1.3, 1);
		glTranslatef(0.15, -0.65, 0.05);
		mySphere(0.25);
		glScalef(1, 1 / 1.3, 1);
		glPopMatrix();
		glDisable(GL_CULL_FACE);
		glEndList();
		ep->AreObjectsDefined[ObjAntBody] = 1;
#ifdef DEBUG_LISTS
		(void) printf("Ant drawn SLOWLY\n");
#endif
	} else {
		glCallList(objects + ObjAntBody);
#ifdef DEBUG_LISTS
		(void) printf("Ant drawn quickly\n");
#endif
	}

	glDisable(GL_LIGHTING);
	/* ANTENNAS */
	glBegin(GL_LINES);
	if (mono)
		glColor3fv(MaterialGray5);
	else
		glColor3fv(Material);
	glVertex3f(0.00, 0.30, 0.00);
	glColor3fv(MaterialGray);
	glVertex3f(0.40, 0.70, 0.40);
	if (mono)
		glColor3fv(MaterialGray5);
	else
		glColor3fv(Material);
	glVertex3f(0.00, 0.30, 0.00);
	glColor3fv(MaterialGray);
	glVertex3f(0.40, 0.70, -0.40);
	glEnd();
	glBegin(GL_POINTS);
	if (mono)
		glColor3fv(MaterialGray6);
	else
		glColor3fv(MaterialRed);
	glVertex3f(0.40, 0.70, 0.40);
	glVertex3f(0.40, 0.70, -0.40);
	glEnd();

	/* LEFT-FRONT ARM */
	glBegin(GL_LINE_STRIP);
	if (mono)
		glColor3fv(MaterialGray5);
	else
		glColor3fv(Material);
	glVertex3f(0.00, 0.05, 0.18);
	glVertex3f(0.35 + 0.05 * cos1, 0.15, 0.25);
	glColor3fv(MaterialGray);
	glVertex3f(-0.20 + 0.05 * cos1, 0.25 + 0.1 * sin1, 0.45);
	glEnd();

	/* LEFT-CENTER ARM */
	glBegin(GL_LINE_STRIP);
	if (mono)
		glColor3fv(MaterialGray5);
	else
		glColor3fv(Material);
	glVertex3f(0.00, 0.00, 0.18);
	glVertex3f(0.35 + 0.05 * cos2, 0.00, 0.25);
	glColor3fv(MaterialGray);
	glVertex3f(-0.20 + 0.05 * cos2, 0.00 + 0.1 * sin2, 0.45);
	glEnd();

	/* LEFT-BACK ARM */
	glBegin(GL_LINE_STRIP);
	if (mono)
		glColor3fv(MaterialGray5);
	else
		glColor3fv(Material);
	glVertex3f(0.00, -0.05, 0.18);
	glVertex3f(0.35 + 0.05 * cos3, -0.15, 0.25);
	glColor3fv(MaterialGray);
	glVertex3f(-0.20 + 0.05 * cos3, -0.25 + 0.1 * sin3, 0.45);
	glEnd();

	/* RIGHT-FRONT ARM */
	glBegin(GL_LINE_STRIP);
	if (mono)
		glColor3fv(MaterialGray5);
	else
		glColor3fv(Material);
	glVertex3f(0.00, 0.05, -0.18);
	glVertex3f(0.35 - 0.05 * sin1, 0.15, -0.25);
	glColor3fv(MaterialGray);
	glVertex3f(-0.20 - 0.05 * sin1, 0.25 + 0.1 * cos1, -0.45);
	glEnd();

	/* RIGHT-CENTER ARM */
	glBegin(GL_LINE_STRIP);
	if (mono)
		glColor3fv(MaterialGray5);
	else
		glColor3fv(Material);
	glVertex3f(0.00, 0.00, -0.18);
	glVertex3f(0.35 - 0.05 * sin2, 0.00, -0.25);
	glColor3fv(MaterialGray);
	glVertex3f(-0.20 - 0.05 * sin2, 0.00 + 0.1 * cos2, -0.45);
	glEnd();

	/* RIGHT-BACK ARM */
	glBegin(GL_LINE_STRIP);
	if (mono)
		glColor3fv(MaterialGray5);
	else
		glColor3fv(Material);
	glVertex3f(0.00, -0.05, -0.18);
	glVertex3f(0.35 - 0.05 * sin3, -0.15, -0.25);
	glColor3fv(MaterialGray);
	glVertex3f(-0.20 - 0.05 * sin3, -0.25 + 0.1 * cos3, -0.45);
	glEnd();

	glBegin(GL_POINTS);
	if (mono)
		glColor3fv(MaterialGray8);
	else
		glColor3fv(MaterialMagenta);
	glVertex3f(-0.20 + 0.05 * cos1, 0.25 + 0.1 * sin1, 0.45);
	glVertex3f(-0.20 + 0.05 * cos2, 0.00 + 0.1 * sin2, 0.45);
	glVertex3f(-0.20 + 0.05 * cos3, -0.25 + 0.1 * sin3, 0.45);
	glVertex3f(-0.20 - 0.05 * sin1, 0.25 + 0.1 * cos1, -0.45);
	glVertex3f(-0.20 - 0.05 * sin2, 0.00 + 0.1 * cos2, -0.45);
	glVertex3f(-0.20 - 0.05 * sin3, -0.25 + 0.1 * cos3, -0.45);
	glEnd();

	glEnable(GL_LIGHTING);

	ant_step += 0.3;
}

static void
RotateAaroundU(float Ax, float Ay, float Az,
	       float Ux, float Uy, float Uz,
	       float *Cx, float *Cy, float *Cz,
	       float Theta)
{
	float       cosO = cos(Theta);
	float       sinO = sin(Theta);
	float       one_cosO = 1 - cosO;
	float       Ux2 = sqr(Ux);
	float       Uy2 = sqr(Uy);
	float       Uz2 = sqr(Uz);
	float       UxUy = Ux * Uy;
	float       UxUz = Ux * Uz;
	float       UyUz = Uy * Uz;

	*Cx = (Ux2 + cosO * (1 - Ux2)) * Ax + (UxUy * one_cosO - Uz * sinO) * Ay + (UxUz * one_cosO + Uy * sinO) * Az;
	*Cy = (UxUy * one_cosO + Uz * sinO) * Ax + (Uy2 + cosO * (1 - Uy2)) * Ay + (UyUz * one_cosO - Ux * sinO) * Az;
	*Cz = (UxUz * one_cosO - Uy * sinO) * Ax + (UyUz * one_cosO + Ux * sinO) * Ay + (Uz2 + cosO * (1 - Uz2)) * Az;
}

#define MoebiusDivisions 40
#define MoebiusTransversals 4
static void
draw_moebius(ModeInfo * mi)
{
	GLfloat     Phi, Theta;
	GLfloat     cPhi, sPhi;
	escherstruct *ep = &escher[MI_SCREEN(mi)];
	int         i, j;
	int         mono = MI_WIN_IS_MONO(mi);

	float       Cx, Cy, Cz;

	if (!ep->AreObjectsDefined[ObjMoebiusStrip]) {
		glNewList(objects + ObjMoebiusStrip, GL_COMPILE_AND_EXECUTE);

		if (solidmoebius) {
			glBegin(GL_QUAD_STRIP);
			Phi = 0;
			i = 0;
			while (i < (MoebiusDivisions * 2 + 1)) {
				Theta = Phi / 2;
				cPhi = cos(Phi);
				sPhi = sin(Phi);

				i++;
				if (mono)
					glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialWhite);
				else if (i % 2)
					glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialRed);
				else
					glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGray);

				RotateAaroundU(cPhi, sPhi, 0, -sPhi, cPhi, 0, &Cx, &Cy, &Cz, Theta);
				glNormal3f(Cx, Cy, Cz);
				RotateAaroundU(0, 0, 1, -sPhi, cPhi, 0, &Cx, &Cy, &Cz, Theta);
				glVertex3f(cPhi * 3 + Cx, sPhi * 3 + Cy, +Cz);
				glVertex3f(cPhi * 3 - Cx, sPhi * 3 - Cy, -Cz);

				Phi += Pi / MoebiusDivisions;
			}
			glEnd();
		} else {
			for (j = -MoebiusTransversals; j < MoebiusTransversals; j++) {
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				glBegin(GL_QUAD_STRIP);
				Phi = 0;
				i = 0;
				while (i < (MoebiusDivisions * 2 + 1)) {
					Theta = Phi / 2;
					cPhi = cos(Phi);
					sPhi = sin(Phi);

					RotateAaroundU(cPhi, sPhi, 0, -sPhi, cPhi, 0, &Cx, &Cy, &Cz, Theta);
					glNormal3f(Cx, Cy, Cz);
					RotateAaroundU(0, 0, 1, -sPhi, cPhi, 0, &Cx, &Cy, &Cz, Theta);
					j++;
					if (j == MoebiusTransversals || mono)
						glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialWhite);
					else if (i % 2)
						glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialRed);
					else
						glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGray);
					glVertex3f(cPhi * 3 + Cx / MoebiusTransversals * j, sPhi * 3 + Cy / MoebiusTransversals * j, +Cz / MoebiusTransversals * j);
					j--;
					if (j == -MoebiusTransversals || mono)
						glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialWhite);
					else if (i % 2)
						glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialRed);
					else
						glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGray);
					glVertex3f(cPhi * 3 + Cx / MoebiusTransversals * j, sPhi * 3 + Cy / MoebiusTransversals * j, +Cz / MoebiusTransversals * j);

					Phi += Pi / MoebiusDivisions;
					i++;
				}
				glEnd();
			}
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}

		glEndList();
		ep->AreObjectsDefined[ObjMoebiusStrip] = 1;
#ifdef DEBUG_LISTS
		(void) printf("Strip drawn SLOWLY\n");
#endif
	} else {
		glCallList(objects + ObjMoebiusStrip);
#ifdef DEBUG_LISTS
		(void) printf("Strip drawn quickly\n");
#endif
	}

	if (!noants) {
		/* DRAW BLUE ANT */
		glPushMatrix();
		glRotatef(ep->ant_position + 180, 0, 0, 1);
		glTranslatef(3, 0, 0);
		glRotatef(ep->ant_position / 2 + 90, 0, 1, 0);
		glTranslatef(0.28, 0, -0.45);
		draw_escher_ant(ep, MaterialYellow, mono);
		glPopMatrix();

		/* DRAW YELLOW ANT */
		glPushMatrix();
		glRotatef(ep->ant_position, 0, 0, 1);
		glTranslatef(3, 0, 0);
		glRotatef(ep->ant_position / 2, 0, 1, 0);
		glTranslatef(0.28, 0, -0.45);
		draw_escher_ant(ep, MaterialBlue, mono);
		glPopMatrix();

		/* DRAW GREEN ANT */
		glPushMatrix();
		glRotatef(-ep->ant_position, 0, 0, 1);
		glTranslatef(3, 0, 0);
		glRotatef(-ep->ant_position / 2, 0, 1, 0);
		glTranslatef(0.28, 0, 0.45);
		glRotatef(180, 1, 0, 0);
		draw_escher_ant(ep, MaterialGreen, mono);
		glPopMatrix();

		/* DRAW CYAN ANT */
		glPushMatrix();
		glRotatef(-ep->ant_position + 180, 0, 0, 1);
		glTranslatef(3, 0, 0);
		glRotatef(-ep->ant_position / 2 + 90, 0, 1, 0);
		glTranslatef(0.28, 0, 0.45);
		glRotatef(180, 1, 0, 0);
		draw_escher_ant(ep, MaterialCyan, mono);
		glPopMatrix();
	}
	ep->ant_position += 1;
}
#undef MoebiusDivisions
#undef MoebiusTransversals

static void
draw_woodplank(escherstruct * ep)
{
	if (!ep->AreObjectsDefined[ObjWoodPlank]) {
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
		ep->AreObjectsDefined[ObjWoodPlank] = 1;
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
draw_impossiblecage(escherstruct * ep)
{
	glPushMatrix();
	glRotatef(90, 0, 1, 0);
	glTranslatef(0.0, PlankHeight - PlankWidth, -PlankThickness - PlankWidth);
	draw_woodplank(ep);
	glPopMatrix();
	glPushMatrix();
	glRotatef(90, 0, 0, 1);
	glTranslatef(0.0, PlankHeight - PlankWidth, PlankWidth - PlankThickness);
	draw_woodplank(ep);
	glPopMatrix();
	glPushMatrix();
	glRotatef(90, 0, 1, 0);
	glTranslatef(0.0, PlankWidth - PlankHeight, -PlankThickness - PlankWidth);
	draw_woodplank(ep);
	glPopMatrix();
	glPushMatrix();
	glTranslatef(0.0, PlankWidth - PlankHeight, 3 * PlankThickness - PlankWidth);
	draw_woodplank(ep);
	glPopMatrix();
	glPushMatrix();
	glRotatef(90, 0, 0, 1);
	glTranslatef(0.0, PlankWidth - PlankHeight, PlankWidth - PlankThickness);
	draw_woodplank(ep);
	glPopMatrix();
	glPushMatrix();
	glTranslatef(0.0, PlankWidth - PlankHeight, PlankWidth - 3 * PlankThickness);
	draw_woodplank(ep);
	glPopMatrix();
	glPushMatrix();
	glTranslatef(0.0, PlankHeight - PlankWidth, 3 * PlankThickness - PlankWidth);
	draw_woodplank(ep);
	glPopMatrix();
	glPushMatrix();
	glRotatef(90, 0, 0, 1);
	glTranslatef(0.0, PlankHeight - PlankWidth, PlankThickness - PlankWidth);
	draw_woodplank(ep);
	glPopMatrix();
	glPushMatrix();
	glTranslatef(0.0, PlankHeight - PlankWidth, PlankWidth - 3 * PlankThickness);
	draw_woodplank(ep);
	glPopMatrix();
	glPushMatrix();
	glRotatef(90, 0, 1, 0);
	glTranslatef(0.0, PlankHeight - PlankWidth, PlankWidth + PlankThickness);
	draw_woodplank(ep);
	glPopMatrix();
	glPushMatrix();
	glRotatef(90, 0, 0, 1);
	glTranslatef(0.0, PlankWidth - PlankHeight, PlankThickness - PlankWidth);
	draw_woodplank(ep);
	glPopMatrix();
	glPushMatrix();
	glRotatef(90, 0, 1, 0);
	glTranslatef(0.0, PlankWidth - PlankHeight, PlankWidth + PlankThickness);
	draw_woodplank(ep);
	glPopMatrix();
}

void
draw_escher(ModeInfo * mi)
{
	escherstruct *ep = &escher[MI_SCREEN(mi)];

	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);

	glXMakeCurrent(display, window, ep->glx_context);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glPushMatrix();

	glTranslatef(0.0, 0.0, -10.0);

	if (!MI_WIN_IS_ICONIC(mi)) {
		glScalef(Scale4Window * ep->WindH / ep->WindW, Scale4Window, Scale4Window);
	} else {
		glScalef(Scale4Iconic * ep->WindH / ep->WindW, Scale4Iconic, Scale4Iconic);
	}


	switch (ep->scene) {
		case 1:
			glRotatef(ep->step * 100, 1, 0, 0);
			glRotatef(ep->step * 95, 0, 1, 0);
			glRotatef(ep->step * 90, 0, 0, 1);
			draw_moebius(mi);
			break;
		case 2:	/* 196 - 213 */
			glRotatef(ep->step * 100, 0, 0, 1);
			glRotatef(25 + cos(ep->step * 5) * 6, 1, 0, 0);
			glRotatef(204.5 - sin(ep->step * 5) * 8, 0, 1, 0);
			draw_impossiblecage(ep);
			break;
	}

	glPopMatrix();

	glFlush();

	glXSwapBuffers(display, window);

	ep->step += 0.025;
}

static void
reshape(ModeInfo * mi, int width, int height)
{
	escherstruct *ep = &escher[MI_SCREEN(mi)];

	glViewport(0, 0, ep->WindW = (GLint) width, ep->WindH = (GLint) height);
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
	ep->AreObjectsDefined[ObjMoebiusStrip] = 0;
	ep->AreObjectsDefined[ObjAntBody] = 0;
	ep->AreObjectsDefined[ObjWoodPlank] = 0;
}

static void
pinit(ModeInfo * mi)
{
	escherstruct *ep = &escher[MI_SCREEN(mi)];

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

	switch (ep->scene) {
		case 1:
			glShadeModel(GL_SMOOTH);
			glEnable(GL_DEPTH_TEST);
			glDisable(GL_TEXTURE_2D);
			glDisable(GL_CULL_FACE);
			break;
		case 2:
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialWhite);
			glShadeModel(GL_FLAT);
			glDisable(GL_DEPTH_TEST);
			glEnable(GL_TEXTURE_2D);
			glEnable(GL_CULL_FACE);
			break;
	}

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
init_escher(ModeInfo * mi)
{
	int         screen = MI_SCREEN(mi);
	escherstruct *ep;

	if (escher == NULL) {
		if ((escher = (escherstruct *) calloc(MI_NUM_SCREENS(mi),
					     sizeof (escherstruct))) == NULL)
			return;
	}
	ep = &escher[screen];
	ep->step = NRAND(90);
	ep->ant_position = NRAND(90);

	ep->glx_context = init_GL(mi);

	reshape(mi, MI_WIN_WIDTH(mi), MI_WIN_HEIGHT(mi));
	ep->scene = MI_BATCHCOUNT(mi);
	if (ep->scene <= 0 || ep->scene > NUM_SCENES)
		ep->scene = NRAND(NUM_SCENES) + 1;
	glDrawBuffer(GL_BACK);
	objects = glGenLists(3);
	pinit(mi);

}

void
change_escher(ModeInfo * mi)
{
	escherstruct *ep = &escher[MI_SCREEN(mi)];

	ep->scene = (ep->scene) % NUM_SCENES + 1;
	glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), ep->glx_context);
	pinit(mi);
}

void
release_escher(ModeInfo * mi)
{
	if (escher != NULL) {
		(void) free((void *) escher);
		escher = NULL;
	}
	glDeleteLists(objects, 3);
}

#endif
