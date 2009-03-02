/* tube, Copyright (c) 2001 Jamie Zawinski <jwz@jwz.org>
 * Utility functions to create tubes and cones in GL.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include "config.h"
#include <stdlib.h>
#include <math.h>
#include <GL/glx.h>
#include "tube.h"

static void
unit_tube (int faces, Bool smooth, Bool wire)
{
  int i;
  GLfloat step = M_PI * 2 / faces;
  GLfloat s2 = step/2;
  GLfloat th;
  GLfloat x, y, x0=0, y0=0;
  int z = 0;

  /* side walls
   */
  glFrontFace(GL_CCW);
  glBegin (wire ? GL_LINES : (smooth ? GL_QUAD_STRIP : GL_QUADS));

  th = 0;
  x = 1;
  y = 0;

  if (!smooth)
    {
      x0 = cos (s2);
      y0 = sin (s2);
    }

  if (smooth) faces++;

  for (i = 0; i < faces; i++)
    {
      if (smooth)
        glNormal3f(x, 0, y);
      else
        glNormal3f(x0, 0, y0);

      glVertex3f(x, 0, y);
      glVertex3f(x, 1, y);

      th += step;
      x  = cos (th);
      y  = sin (th);

      if (!smooth)
        {
          x0 = cos (th + s2);
          y0 = sin (th + s2);

          glVertex3f(x, 1, y);
          glVertex3f(x, 0, y);
        }
    }
  glEnd();

  /* End caps
   */
  for (z = 0; z <= 1; z++)
    {
      glFrontFace(z == 0 ? GL_CCW : GL_CW);
      glNormal3f(0, (z == 0 ? -1 : 1), 0);
      glBegin(wire ? GL_LINE_LOOP : GL_TRIANGLE_FAN);
      if (! wire) glVertex3f(0, z, 0);
      for (i = 0, th = 0; i <= faces; i++)
        {
          GLfloat x = cos (th);
          GLfloat y = sin (th);
          glVertex3f(x, z, y);
          th += step;
        }
      glEnd();
    }
}


static void
unit_cone (int faces, Bool smooth, Bool wire)
{
  int i;
  GLfloat step = M_PI * 2 / faces;
  GLfloat s2 = step/2;
  GLfloat th;
  GLfloat x, y, x0, y0;

  /* side walls
   */
  glFrontFace(GL_CW);
  glBegin(wire ? GL_LINES : GL_TRIANGLES);

  th = 0;
  x = 1;
  y = 0;
  x0 = cos (s2);
  y0 = sin (s2);

  for (i = 0; i < faces; i++)
    {
      glNormal3f(x0, 0, y0);
      glVertex3f(0,  1, 0);

      if (smooth) glNormal3f(x, 0, y);
      glVertex3f(x, 0, y);

      th += step;
      x0 = cos (th + s2);
      y0 = sin (th + s2);
      x  = cos (th);
      y  = sin (th);

      if (smooth) glNormal3f(x, 0, y);
      glVertex3f(x, 0, y);
    }
  glEnd();

  /* End cap
   */
  glFrontFace(GL_CCW);
  glNormal3f(0, -1, 0);
  glBegin(wire ? GL_LINE_LOOP : GL_TRIANGLE_FAN);
  if (! wire) glVertex3f(0, 0, 0);
  for (i = 0, th = 0; i <= faces; i++)
    {
      GLfloat x = cos (th);
      GLfloat y = sin (th);
      glVertex3f(x, 0, y);
      th += step;
    }
  glEnd();
}


static void
tube_1 (GLfloat x1, GLfloat y1, GLfloat z1,
        GLfloat x2, GLfloat y2, GLfloat z2,
        GLfloat diameter, GLfloat cap_size,
        int faces, Bool smooth, Bool wire,
        Bool cone_p)
{
  GLfloat length, X, Y, Z;

  if (diameter <= 0) abort();

  X = (x2 - x1);
  Y = (y2 - y1);
  Z = (z2 - z1);

  if (X == 0 && Y == 0 && Z == 0)
    return;

  length = sqrt (X*X + Y*Y + Z*Z);

  glPushMatrix();

  glTranslatef(x1, y1, z1);
  glRotatef (-atan2 (X, Y)               * (180 / M_PI), 0, 0, 1);
  glRotatef ( atan2 (Z, sqrt(X*X + Y*Y)) * (180 / M_PI), 1, 0, 0);
  glScalef (diameter, length, diameter);

  /* extend the endpoints of the tube by the cap size in both directions */
  if (cap_size != 0)
    {
      GLfloat c = cap_size/length;
      glTranslatef (0, -c, 0);
      glScalef (1, 1+c+c, 1);
    }

  if (cone_p)
    unit_cone (faces, smooth, wire);
  else
    unit_tube (faces, smooth, wire);

  glPopMatrix();
}


void
tube (GLfloat x1, GLfloat y1, GLfloat z1,
      GLfloat x2, GLfloat y2, GLfloat z2,
      GLfloat diameter, GLfloat cap_size,
      int faces, Bool smooth, Bool wire)
{
  tube_1 (x1, y1, z1, x2, y2, z2, diameter, cap_size, faces, smooth, wire,
          False);
}


void
cone (GLfloat x1, GLfloat y1, GLfloat z1,
      GLfloat x2, GLfloat y2, GLfloat z2,
      GLfloat diameter, GLfloat cap_size,
      int faces, Bool smooth, Bool wire)
{
  tube_1 (x1, y1, z1, x2, y2, z2, diameter, cap_size, faces, smooth, wire,
          True);
}
