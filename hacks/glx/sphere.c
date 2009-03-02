/* sphere, Copyright (c) 1998 David Konerding <dek@cgl.ucsf.edu>
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
 *  8-Oct-98: dek           Released initial version of "glplanet"
 * 21-Mar-01: jwz@jwz.org   Broke sphere routine out into its own file.
 */

#include "config.h"
#include <stdlib.h>
#include <math.h>
#include <GL/glx.h>
#include "tube.h"

/* Function for determining points on the surface of the sphere */
static void
parametric_sphere (float theta, float rho, GLfloat *vector)
{
  vector[0] = -sin(theta) * sin(rho);
  vector[1] = cos(theta) * sin(rho);
  vector[2] = cos(rho);
}


void
unit_sphere (int stacks, int slices, Bool wire)
{
  int i, j;
  float drho, dtheta;
  float rho, theta;
  GLfloat vector[3];
  GLfloat ds, dt, t, s;

  if (!wire)
    glShadeModel(GL_SMOOTH);

  /* Generate a sphere with quadrilaterals.
   * Quad vertices are determined using a parametric sphere function.
   * For fun, you could generate practically any parameteric surface and
   * map an image onto it. 
   */
  drho = M_PI / stacks;
  dtheta = 2.0 * M_PI / slices;
  ds = 1.0 / slices;
  dt = 1.0 / stacks;

  glFrontFace(GL_CCW);
  glBegin (wire ? GL_LINE_LOOP : GL_QUADS);

  t = 0.0;
  for (i=0; i < stacks; i++) {
    rho = i * drho;
    s = 0.0;
    for (j=0; j < slices; j++) {
      theta = j * dtheta;

      glTexCoord2f (s,t);
      parametric_sphere (theta, rho, vector);
      glNormal3fv (vector);
      parametric_sphere (theta, rho, vector);
      glVertex3f (vector[0], vector[1], vector[2]);

      glTexCoord2f (s,t+dt);
      parametric_sphere (theta, rho+drho, vector);
      glNormal3fv (vector);
      parametric_sphere (theta, rho+drho, vector);
      glVertex3f (vector[0], vector[1], vector[2]);

      glTexCoord2f (s+ds,t+dt);
      parametric_sphere (theta + dtheta, rho+drho, vector);
      glNormal3fv (vector);
      parametric_sphere (theta + dtheta, rho+drho, vector);
      glVertex3f (vector[0], vector[1], vector[2]);

      glTexCoord2f (s+ds, t);
      parametric_sphere (theta + dtheta, rho, vector);
      glNormal3fv (vector);
      parametric_sphere (theta + dtheta, rho, vector);
      glVertex3f (vector[0], vector[1], vector[2]);

      s = s + ds;
    }
    t = t + dt;
  }
  glEnd();
}
