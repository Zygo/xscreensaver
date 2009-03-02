/* glschool_gl.h, Copyright (c) 2005-2006 David C. Lambert <dcl@panix.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */
#ifndef __GLSCHOOL_GL_H__
#define __GLSCHOOL_GL_H__

#ifdef HAVE_COCOA
# include "jwxyz.h"
# include <OpenGL/gl.h>
# include <OpenGL/glu.h>
#else
# include <X11/Xlib.h>
# include <GL/gl.h>
# include <GL/glu.h>
#endif

#include "glschool_alg.h"

extern void initFog(void);
extern void initGLEnv(Bool);
extern void initLights(void);
extern void reshape(int, int);
extern void drawGoal(double *, GLuint);
extern void getColorVect(XColor *, int, double *);
extern void drawBoundingBox(BBox *, Bool);
extern void createBBoxList(BBox *, GLuint *, int);
extern void createDrawLists(BBox *, GLuint *, GLuint *, GLuint *, Bool);
extern void drawSchool(XColor *, School *, GLuint, GLuint, GLuint, int, Bool, Bool);

#endif /* __GLSCHOOL_GL_H__ */
