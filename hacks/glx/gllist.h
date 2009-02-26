#ifndef __GLLIST_H__
#define __GLLIST_H__
#include <GL/gl.h>
#include <stdlib.h>

struct gllist{
	GLenum format;
	GLenum primitive;
	int points;
	void *data;
	struct gllist *next;
};

void renderList(struct gllist *list);

#endif
