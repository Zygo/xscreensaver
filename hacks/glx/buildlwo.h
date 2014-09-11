/*-
 * buildlwo.h: Header file for Lightwave Object Display List Builder
 * for OpenGL
 *
 * by Ed Mackey, 4/19/97
 *
 */

#ifndef __BUILD_LWO_H__
#define __BUILD_LWO_H__

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef STANDALONE
# ifndef HAVE_COCOA
#  include <GL/gl.h>
# endif
#endif

#ifdef HAVE_JWZGLES
# include "jwzgles.h"
#endif /* HAVE_JWZGLES */

struct lwo {
	int             num_pnts;
	const GLfloat  *pnts;
	const GLfloat  *normals;
	const unsigned  short int *pols;
	const GLfloat  *smoothnormals;
};

GLuint      BuildLWO(int wireframe, const struct lwo *object);

#endif

/* End of buildlwo.h */
