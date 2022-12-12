
#if 0
static const char sccsid[] = "@(#)buildlwo.c	4.02 97/04/20 xlockmore";
#endif

/*-
 * buildlwo.c: Lightwave Object Display List Builder for OpenGL
 *
 * This module can be called by any GL mode wishing to use
 * objects created in NewTek's Lightwave 3D.  The objects must
 * first be converted to C source with my converter "lw2ogl".
 * If other people are interested in this, I will put up a
 * web page for it at http://www.netaxs.com/~emackey/lw2ogl/
 *
 * by Ed Mackey, 4/19/97
 *
 */

#ifndef STANDALONE
# include "xlock.h"
#else
# include "screenhackI.h"
#endif

#ifdef USE_GL

#include "buildlwo.h"

GLuint
BuildLWO(int wireframe, const struct lwo *object)
{
	GLuint      dl_num;
	const GLfloat *pnts, *normals, *grab;
	const unsigned short int *pols;
        GLfloat three[3];
	int         p, num_pnts = 0;

	dl_num = glGenLists(1);
	if (!dl_num)
		return (0);

	pnts = object->pnts;
	normals = object->normals;
	pols = object->pols;

	glNewList(dl_num, GL_COMPILE);

	if (!pols) {
		num_pnts = object->num_pnts;
		glBegin(GL_POINTS);
		for (p = 0; p < num_pnts; ++p) {
			three[0] = *(pnts++);
			three[1] = *(pnts++);
			three[2] = *(pnts++);
			glVertex3fv(three);
		}
		glEnd();
	} else
		for (;;) {
			if (num_pnts <= 0) {
				num_pnts = *pols + 2;
				if (num_pnts < 3)
					break;
				if (num_pnts == 3) {
					glBegin(GL_POINTS);
				} else if (num_pnts == 4) {
					glBegin(GL_LINES);
				} else {
					three[0] = *(normals++);
					three[1] = *(normals++);
					three[2] = *(normals++);
					glNormal3fv(three);
					if (wireframe)
						glBegin(GL_LINE_LOOP);
					else
						glBegin(GL_POLYGON);
				}
			} else if (num_pnts == 1) {
				glEnd();
			} else {
				grab = pnts + ((int) (*pols) * 3);
				three[0] = *(grab++);
				three[1] = *(grab++);
				three[2] = *(grab);
				glVertex3fv(three);
			}
			--num_pnts;
			++pols;
		}

	glEndList();

	return (dl_num);
}

#endif /* USE_GL */

/* End of buildlwo.c */
