/*-
 * buildlwo.h: Header file for Lightwave Object Display List Builder
 * for OpenGL
 *
 * by Ed Mackey, 4/19/97
 *
 */

#ifndef __BUILD_LWO_H__
#define __BUILD_LWO_H__

struct lwo {
	int         num_pnts;
	GLfloat    *pnts;
	GLfloat    *normals;
	unsigned short int *pols;
	GLfloat    *smoothnormals;
};

GLuint      BuildLWO(int wireframe, struct lwo *object);

#endif

/* End of buildlwo.h */
