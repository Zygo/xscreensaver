/* glschool_gl.c, Copyright (c) 2005-2012 David C. Lambert <dcl@panix.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include "sphere.h"
#include "glschool_gl.h"
#include "sphere.h"
#include "tube.h"

void
glschool_drawGoal(double *goal, GLuint goalList)
{
	glColor3f(1.0, 0.0, 0.0);
	glPushMatrix();
	{
		glTranslatef(goal[0], goal[1], goal[2]);
		glColor3f(1.0, 0.0, 0.0);
		glCallList(goalList);
	}
	glPopMatrix();
}

int
glschool_drawBoundingBox(BBox *bbox, Bool wire)
{
  int polys = 0;
	double		xMin = BBOX_XMIN(bbox);
	double		yMin = BBOX_YMIN(bbox);
	double		zMin = BBOX_ZMIN(bbox);

	double		xMax = BBOX_XMAX(bbox);
	double		yMax = BBOX_YMAX(bbox);
	double		zMax = BBOX_ZMAX(bbox);

	glFrontFace(GL_CCW);
	if (wire) glLineWidth(5.0);

	/*  back */
	glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
	glColor3f(0.0, 0.0, .15);
	glVertex3f(xMin, yMin, zMin);
	glVertex3f(xMax, yMin, zMin);
	glVertex3f(xMax, yMax, zMin);
	glVertex3f(xMin, yMax, zMin);
        polys++;
	glEnd();

	/* left */
	glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
	glColor3f(0.0, 0.0, .2);
	glVertex3f(xMin, yMin, zMax);
	glVertex3f(xMin, yMin, zMin);
	glVertex3f(xMin, yMax, zMin);
	glVertex3f(xMin, yMax, zMax);
        polys++;
	glEnd();

	/* right */
	glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
	glColor3f(0.0, 0.0, .2);
	glVertex3f(xMax, yMin, zMin);
	glVertex3f(xMax, yMin, zMax);
	glVertex3f(xMax, yMax, zMax);
	glVertex3f(xMax, yMax, zMin);
        polys++;
	glEnd();

	/* top */
	glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
	glColor3f(0.0, 0.0, .1);
	glVertex3f(xMax, yMax, zMax);
	glVertex3f(xMin, yMax, zMax);
	glVertex3f(xMin, yMax, zMin);
	glVertex3f(xMax, yMax, zMin);
        polys++;
	glEnd();

	/* bottom */
	glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
	glColor3f(0.0, 0.0, .3);
	glVertex3f(xMin, yMin, zMax);
	glVertex3f(xMax, yMin, zMax);
	glVertex3f(xMax, yMin, zMin);
	glVertex3f(xMin, yMin, zMin);
        polys++;
	glEnd();

	if (wire) glLineWidth(1.0);

        return polys;
}

int
glschool_createBBoxList(BBox *bbox, GLuint *bboxList, int wire)
{
  int polys = 0;
	*bboxList = glGenLists(1);
	glNewList(*bboxList, GL_COMPILE);
	polys = glschool_drawBoundingBox(bbox, wire);
	glEndList();
        return polys;
}

void
glschool_createDrawLists(BBox *bbox, GLuint *bboxList, GLuint *goalList, GLuint *fishList, int *fish_polys, int *box_polys, int wire)
{

        int faces = 16;

        *box_polys = 0;
        *fish_polys = 0;

        *box_polys += glschool_createBBoxList(bbox, bboxList, wire);

        *box_polys = 0;
        *fish_polys = 0;

	*goalList = glGenLists(1);
	glNewList(*goalList, GL_COMPILE);
        glScalef (5, 5, 5);
        *box_polys += unit_sphere (10, 10, wire);
	glEndList();

	*fishList = glGenLists(1);
	glNewList(*fishList, GL_COMPILE);
        *fish_polys += cone (0, 0, 0,
                             0, 0, 10,
                             2, 0,
                             faces, True, (faces <= 3), /* cap */
                             wire);
        glTranslatef (0, 0, -0.3);
        glScalef (2, 2, 2);
        glRotatef (90, 1, 0, 0);
        if (faces > 3)
          *fish_polys += unit_sphere (faces, faces, wire);
	glEndList();
}


void
glschool_initLights(void)
{
	GLfloat		amb[4] = {0.1, 0.1, 0.1, 1.0};
	GLfloat		dif[4] = {1.0, 1.0, 1.0, 1.0};
	GLfloat		spc[4] = {1.0, 1.0, 1.0, 1.0};
	GLfloat		pos[4] = {0.0, 50.0, -50.0, 1.0};

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glLightfv(GL_LIGHT0, GL_POSITION, pos);
	glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
	glLightfv(GL_LIGHT0, GL_SPECULAR, spc);

	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHTING);
}

void
glschool_initFog(void)
{
	GLfloat		fog[4] = {0.0, 0.0, 0.15, 1.0};

	glEnable(GL_FOG);
	glFogi(GL_FOG_MODE, GL_EXP2);
	glFogfv(GL_FOG_COLOR, fog);
	glFogf(GL_FOG_DENSITY, .0025);
	glFogf(GL_FOG_START, -100);
}

void
glschool_initGLEnv(Bool doFog)
{
	GLfloat spc[4] = {1.0, 1.0, 1.0, 1.0};

	glClearDepth(1.0);
	glDepthFunc(GL_LESS);

	glEnable(GL_COLOR_MATERIAL);
	glMateriali(GL_FRONT, GL_SHININESS, 128);
	glMaterialfv(GL_FRONT, GL_SPECULAR, spc);
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, spc);

	glEnable(GL_NORMALIZE);
	glEnable(GL_DEPTH_TEST);
	glShadeModel(GL_SMOOTH);
	glEnable(GL_CULL_FACE);

	glschool_initLights();
	if (doFog) glschool_initFog();
}

void
glschool_reshape(int width, int height)
{
	GLfloat h = (GLfloat) width / (GLfloat) height;

	glViewport (0, 0, (GLint) width, (GLint) height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60.0, h, 0.1, 451.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void
glschool_getColorVect(XColor *colors, int index, double *colorVect)
{
	colorVect[0] = colors[index].red / 65535.0;
	colorVect[1] = colors[index].green / 65535.0;
	colorVect[2] = colors[index].blue / 65535.0;
}

void
glschool_drawSchool(XColor *colors, School *s,
		   GLuint bboxList, GLuint goalList, GLuint fishList,
		   int rotCounter, Bool drawGoal_p, Bool drawBBox_p,
           int fish_polys, int box_polys, unsigned long *polys)
{
	double			xVect[3];
	double			colorVect[3];
	int				i = 0;
	double			rotTheta = 0.0;
	double			colTheta = 0.0;
	Fish			*f = (Fish *)0;
	int				nFish = SCHOOL_NFISH(s);
	Fish			*theFishes = SCHOOL_FISHES(s);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	if (drawBBox_p) {
		glDisable(GL_LIGHTING);
		glCallList(bboxList);
		glEnable(GL_LIGHTING);
                *polys += box_polys;
	}

	if (drawGoal_p)	glschool_drawGoal(SCHOOL_GOAL(s), goalList);

	for(i = 0, f = theFishes; i < nFish; i++, f++) {
		colTheta = glschool_computeNormalAndThetaToPlusZ(FISH_AVGVEL(f), xVect);
		rotTheta = glschool_computeNormalAndThetaToPlusZ(FISH_VEL(f), xVect);

		if (FISH_IAVGVEL(f,2) < 0.0) colTheta = 180.0 - colTheta;
		if (FISH_VZ(f) < 0.0) rotTheta = 180.0 - rotTheta;

		glschool_getColorVect(colors, (int)(colTheta+240)%360, colorVect);
		glColor3f(colorVect[0], colorVect[1], colorVect[2]);

		glPushMatrix();
		{
			glTranslatef(FISH_X(f), FISH_Y(f), FISH_Z(f));
			glRotatef(180.0+rotTheta, xVect[0], xVect[1], xVect[2]);
			glCallList(fishList);
                        *polys += fish_polys;
		}
		glPopMatrix();
	}

	glFinish();
}
