/* sphere, Copyright (c) 2002 Paul Bourke <pbourke@swin.edu.au>
 * Utility function to create a unit sphere in GL.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 *  8-Oct-98: dek          Released initial version of "glplanet"
 * 21-Mar-01: jwz@jwz.org  Broke sphere routine out into its own file.
 * 28-Feb-02: jwz@jwz.org  New implementation from Paul Bourke:
 *                         http://astronomy.swin.edu.au/~pbourke/opengl/sphere/
 */

#include "config.h"
#include <stdlib.h>
#include <math.h>
#include <GL/glx.h>


typedef struct { GLfloat x, y, z; } XYZ;

void
unit_sphere (int stacks, int slices, Bool wire)
{
  int i,j;
  double theta1, theta2, theta3;
  XYZ e, p;
  XYZ la, lb;
  XYZ c = {0, 0, 0};  /* center */
  double r = 1.0;     /* radius */
  int stacks2 = stacks * 2;

  if (r < 0)
    r = -r;
  if (slices < 0)
    slices = -slices;

  if (slices < 4 || stacks < 2 || r <= 0)
    {
      glBegin (GL_POINTS);
      glVertex3f (c.x, c.y, c.z);
      glEnd();
      return;
    }

  glFrontFace(GL_CW);

  for (j = 0; j < stacks; j++)
    {
      theta1 = j       * (M_PI+M_PI) / stacks2 - M_PI_2;
      theta2 = (j + 1) * (M_PI+M_PI) / stacks2 - M_PI_2;

      glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLE_STRIP);
      for (i = 0; i <= slices; i++)
        {
          theta3 = i * (M_PI+M_PI) / slices;

          if (wire && i != 0)
            {
              glVertex3f (lb.x, lb.y, lb.z);
              glVertex3f (la.x, la.y, la.z);
            }

          e.x = cos (theta2) * cos(theta3);
          e.y = sin (theta2);
          e.z = cos (theta2) * sin(theta3);
          p.x = c.x + r * e.x;
          p.y = c.y + r * e.y;
          p.z = c.z + r * e.z;

          glNormal3f (e.x, e.y, e.z);
          glTexCoord2f (i       / (double)slices,
                        2*(j+1) / (double)stacks2);
          glVertex3f (p.x, p.y, p.z);
          if (wire) la = p;

          e.x = cos(theta1) * cos(theta3);
          e.y = sin(theta1);
          e.z = cos(theta1) * sin(theta3);
          p.x = c.x + r * e.x;
          p.y = c.y + r * e.y;
          p.z = c.z + r * e.z;

          glNormal3f (e.x, e.y, e.z);
          glTexCoord2f (i   / (double)slices,
                        2*j / (double)stacks2);
          glVertex3f (p.x, p.y, p.z);
          if (wire) lb = p;
        }
      glEnd();
    }
}
