/* normals, Copyright (c) 2002-2012 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Compute normal vectors for arbitrary triangles.
 */

#ifndef __NORMALS_H__
#define __NORMALS_H__

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#ifndef HAVE_COCOA
# include <GL/gl.h>
#endif

#ifdef HAVE_JWZGLES
# include "jwzgles.h"
#endif /* HAVE_JWZGLES */

typedef struct {
  double x,y,z;
} XYZ;

/* Calculate the unit normal at p given two other points p1,p2 on the
   surface. The normal points in the direction of p1 crossproduct p2
 */
extern XYZ calc_normal (XYZ p, XYZ p1, XYZ p2);

/* Call glNormal3f() with a normal of the indicated triangle.
 */
extern void do_normal (GLfloat x1, GLfloat y1, GLfloat z1,
                       GLfloat x2, GLfloat y2, GLfloat z2,
                       GLfloat x3, GLfloat y3, GLfloat z3);

#endif /* __NORMALS_H__ */
