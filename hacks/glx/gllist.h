#ifndef __GLLIST_H__
#define __GLLIST_H__

#include <stdlib.h>

#ifdef HAVE_COCOA
# include <OpenGL/gl.h>
#else
# include <GL/gl.h>
#endif

struct gllist{
	GLenum format;
	GLenum primitive;
	int points;
	const void *data;
	struct gllist *next;
};

void renderList(const struct gllist *list, int wire_p);

#endif
