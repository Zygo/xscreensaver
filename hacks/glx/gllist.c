/* xscreensaver, Copyright (c) 1998-2014 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include "gllist.h"

void
renderList (const struct gllist *list, int wire_p)
{
  while (list)
    {
      if (!wire_p || list->primitive == GL_LINES)
        {
          glInterleavedArrays (list->format, 0, list->data);
          glDrawArrays (list->primitive, 0, list->points);
        }
      else
        {
          /* For wireframe, do it the hard way: treat every tuple of
             points as its own line loop.
           */
          const GLfloat *p = (GLfloat *) list->data;
          int i, j, tick, skip, stride;

          switch (list->primitive) {
          case GL_QUADS: tick = 4; break;
          case GL_TRIANGLES: tick = 3; break;
          default: abort(); break; /* write me */
          }

          switch (list->format) {
          case GL_C3F_V3F: case GL_N3F_V3F: skip = 3; stride = 6; break;
          default: abort(); break; /* write me */
          }

          glBegin (GL_LINE_LOOP);
          for (i = 0, j = skip;
               i < list->points;
               i++, j += stride)
            {
              if (i && !(i % tick))
                {
                  glEnd();
                  glBegin (GL_LINE_LOOP);
                }
              glVertex3f (p[j], p[j+1], p[j+2]);
            }
          glEnd();
        }
      list = list->next;
    }
}


void
renderListNormals (const struct gllist *list, GLfloat length, int faces_p)
{
  while (list)
    {
      const GLfloat *p = (GLfloat *) list->data;
      int i, j, tick, skip, stride;
      GLfloat v[3], n[3];

      if (list->primitive == GL_LINES) continue;

      if (! faces_p)
        tick = 1;
      else
        switch (list->primitive) {
        case GL_QUADS: tick = 4; break;
        case GL_TRIANGLES: tick = 3; break;
        default: abort(); break; /* write me */
        }

      switch (list->format) {
      case GL_N3F_V3F: skip = 0; stride = 6; break;
      case GL_C3F_V3F: continue; break;
      default: abort(); break; /* write me */
      }

      v[0] = v[1] = v[2] = 0;
      n[0] = n[1] = n[2] = 0;

      for (i = 0, j = skip;
           i <= list->points;
           i++, j += stride)
        {
          if (i && !(i % tick))
            {
              n[0] /= tick;
              n[1] /= tick;
              n[2] /= tick;
              v[0] /= tick;
              v[1] /= tick;
              v[2] /= tick;
              glPushMatrix();
              glTranslatef (v[0], v[1], v[2]);
              glScalef (length, length, length);
              glBegin (GL_LINES);
              glVertex3f (0, 0, 0);
              glVertex3f (n[0], n[1], n[2]);
              glEnd();
              glPopMatrix();
              v[0] = v[1] = v[2] = 0;
              n[0] = n[1] = n[2] = 0;
            }

          if (i == list->points) break;
          n[0] += p[j];
          n[1] += p[j+1];
          n[2] += p[j+2];
          v[0] += p[j+3];
          v[1] += p[j+4];
          v[2] += p[j+5];

        }
      list = list->next;
    }
}
