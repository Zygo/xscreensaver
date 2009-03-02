/* normals, Copyright (c) 2002-2004 Jamie Zawinski <jwz@jwz.org>
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

#include "config.h"
#include <GL/gl.h>
#include "normals.h"

/* Calculate the unit normal at p given two other points p1,p2 on the
   surface. The normal points in the direction of p1 crossproduct p2
 */
XYZ
calc_normal (XYZ p, XYZ p1, XYZ p2)
{
  XYZ n, pa, pb;
  pa.x = p1.x - p.x;
  pa.y = p1.y - p.y;
  pa.z = p1.z - p.z;
  pb.x = p2.x - p.x;
  pb.y = p2.y - p.y;
  pb.z = p2.z - p.z;
  n.x = pa.y * pb.z - pa.z * pb.y;
  n.y = pa.z * pb.x - pa.x * pb.z;
  n.z = pa.x * pb.y - pa.y * pb.x;
  return (n);
}

/* Call glNormal3f() with a normal of the indicated triangle.
 */
void
do_normal(GLfloat x1, GLfloat y1, GLfloat z1,
          GLfloat x2, GLfloat y2, GLfloat z2,
          GLfloat x3, GLfloat y3, GLfloat z3)
{
  XYZ p1, p2, p3, p;
  p1.x = x1; p1.y = y1; p1.z = z1;
  p2.x = x2; p2.y = y2; p2.z = z2;
  p3.x = x3; p3.y = y3; p3.z = z3;
  p = calc_normal (p1, p2, p3);
  glNormal3f (p.x, p.y, p.z);
}
