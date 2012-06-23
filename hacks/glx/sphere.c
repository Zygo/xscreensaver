/* sphere, Copyright (c) 2002 Paul Bourke <pbourke@swin.edu.au>,
 *         Copyright (c) 2010 Jamie Zawinski <jwz@jwz.org>
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
 * 21-Aug-10  jwz@jwz.org  Converted to use glDrawArrays, for OpenGL ES.
 */

#include <math.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef HAVE_COCOA
# include <GL/gl.h>
#endif

#ifdef HAVE_JWZGLES
# include "jwzgles.h"
#endif /* HAVE_JWZGLES */

#include "sphere.h"

typedef struct { GLfloat x, y, z; } XYZ;

int
unit_sphere (int stacks, int slices, int wire_p)
{
  int polys = 0;
  int i,j;
  double theta1, theta2, theta3;
  XYZ p, n;
  XYZ la = { 0, -1, 0 }, lb = { 0, -1, 0 };
  XYZ c = {0, 0, 0};  /* center */
  double r = 1.0;     /* radius */
  int stacks2 = stacks * 2;

  int mode = (wire_p ? GL_LINE_STRIP : GL_TRIANGLE_STRIP);

  int arraysize, out;
  struct { XYZ p; XYZ n; GLfloat s, t; } *array;

  if (r < 0)
    r = -r;
  if (slices < 0)
    slices = -slices;

  arraysize = (stacks+1) * (slices+1) * (wire_p ? 4 : 2);
  array = (void *) calloc (arraysize, sizeof(*array));
  if (! array) abort();
  out = 0;

  if (slices < 4 || stacks < 2 || r <= 0)
    {
      mode = GL_POINTS;
      array[out++].p = c;
      goto END;
    }

  for (j = 0; j < stacks; j++)
    {
      theta1 = j       * (M_PI+M_PI) / stacks2 - M_PI_2;
      theta2 = (j + 1) * (M_PI+M_PI) / stacks2 - M_PI_2;

      for (i = slices; i >= 0; i--)
        {
          theta3 = i * (M_PI+M_PI) / slices;

          if (wire_p)
            {
              array[out++].p = lb;				/* vertex */
              array[out++].p = la;				/* vertex */
            }

          n.x = cos (theta2) * cos(theta3);
          n.y = sin (theta2);
          n.z = cos (theta2) * sin(theta3);
          p.x = c.x + r * n.x;
          p.y = c.y + r * n.y;
          p.z = c.z + r * n.z;

          array[out].p = p;					/* vertex */
          array[out].n = n;					/* normal */
          array[out].s = i       / (GLfloat) slices;		/* texture */
          array[out].t = 2*(j+1) / (GLfloat) stacks2;
          out++;

          if (wire_p) la = p;

          n.x = cos(theta1) * cos(theta3);
          n.y = sin(theta1);
          n.z = cos(theta1) * sin(theta3);
          p.x = c.x + r * n.x;
          p.y = c.y + r * n.y;
          p.z = c.z + r * n.z;

          array[out].p = p;					/* vertex */
          array[out].n = n;					/* normal */
          array[out].s = i   / (GLfloat) slices;		/* texture */
          array[out].t = 2*j / (GLfloat) stacks2;
          out++;

          if (out >= arraysize) abort();

          if (wire_p) lb = p;
          polys++;
        }
    }

 END:

  glEnableClientState (GL_VERTEX_ARRAY);
  glEnableClientState (GL_NORMAL_ARRAY);
  glEnableClientState (GL_TEXTURE_COORD_ARRAY);

  glVertexPointer   (3, GL_FLOAT, sizeof(*array), &array[0].p);
  glNormalPointer   (   GL_FLOAT, sizeof(*array), &array[0].n);
  glTexCoordPointer (2, GL_FLOAT, sizeof(*array), &array[0].s);

  glDrawArrays (mode, 0, out);

  free (array);

  return polys;
}
