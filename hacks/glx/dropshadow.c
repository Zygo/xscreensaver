/* dropshadow.c, Copyright (c) 2009 Jens Kilian <jjk@acm.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>

#include "dropshadow.h"

/* (Alpha) texture data for drop shadow.
 */
static int drop_shadow_width = 32;
static int drop_shadow_height = 32;
static unsigned char drop_shadow_data[] =
{
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 1, 1, 1, 3, 3, 4, 4, 4, 4, 4, 4,
  4, 4, 4, 4, 4, 4, 3, 3, 1, 1, 1, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 1, 1, 3, 4, 6, 7, 9, 10, 10, 10, 10, 10,
  10, 10, 10, 10, 10, 9, 7, 6, 4, 3, 1, 1, 0, 0, 0, 0,
  0, 0, 0, 1, 1, 3, 5, 9, 13, 16, 19, 19, 21, 21, 22, 22,
  22, 22, 21, 21, 19, 19, 16, 13, 9, 5, 3, 1, 1, 0, 0, 0,
  0, 0, 1, 1, 3, 5, 10, 16, 22, 28, 32, 35, 37, 37, 38, 38,
  38, 38, 37, 37, 35, 32, 28, 22, 16, 10, 5, 3, 1, 1, 0, 0,
  0, 0, 1, 1, 4, 9, 16, 25, 34, 43, 50, 55, 58, 59, 60, 60,
  60, 60, 59, 58, 55, 50, 43, 34, 25, 16, 9, 4, 1, 1, 0, 0,
  0, 0, 1, 3, 6, 13, 22, 34, 48, 61, 70, 77, 80, 82, 83, 84,
  84, 83, 82, 80, 77, 70, 61, 48, 34, 22, 13, 6, 3, 1, 0, 0,
  0, 0, 1, 3, 7, 16, 28, 43, 61, 76, 88, 97, 102, 103, 104, 104,
  104, 104, 103, 102, 97, 88, 76, 61, 43, 28, 16, 7, 3, 1, 0, 0,
  0, 1, 1, 4, 9, 19, 32, 51, 70, 88, 103, 112, 117, 120, 121, 121,
  121, 121, 120, 117, 112, 103, 88, 70, 51, 32, 19, 9, 4, 1, 1, 0,
  0, 1, 1, 4, 10, 20, 35, 55, 77, 97, 112, 122, 128, 130, 132, 133,
  133, 132, 130, 128, 122, 112, 97, 77, 55, 35, 20, 10, 4, 1, 1, 0,
  0, 1, 1, 4, 10, 21, 37, 58, 80, 101, 117, 128, 134, 137, 138, 139,
  139, 138, 137, 134, 128, 117, 101, 80, 58, 37, 21, 10, 4, 1, 0, 0,
  0, 0, 1, 4, 10, 21, 38, 59, 82, 103, 119, 130, 137, 139, 141, 142,
  142, 141, 139, 137, 130, 119, 103, 82, 59, 38, 21, 10, 4, 1, 0, 0,
  0, 0, 1, 4, 10, 22, 38, 59, 83, 104, 121, 132, 139, 141, 142, 142,
  142, 142, 141, 139, 132, 121, 104, 83, 59, 38, 22, 10, 4, 1, 0, 0,
  0, 0, 1, 4, 10, 22, 38, 60, 84, 104, 121, 133, 139, 142, 142, 142,
  142, 142, 142, 139, 133, 121, 104, 84, 60, 38, 22, 10, 4, 1, 0, 0,
  0, 0, 1, 4, 10, 22, 38, 60, 84, 104, 121, 133, 139, 142, 142, 142,
  142, 142, 142, 139, 133, 121, 104, 84, 60, 38, 22, 10, 4, 1, 0, 0,
  0, 0, 1, 4, 10, 22, 38, 59, 83, 104, 121, 132, 139, 141, 142, 142,
  142, 142, 141, 139, 132, 121, 104, 83, 59, 38, 22, 10, 4, 1, 0, 0,
  0, 0, 1, 4, 10, 21, 38, 59, 82, 103, 119, 130, 137, 139, 141, 142,
  142, 141, 139, 137, 130, 119, 103, 82, 59, 38, 21, 10, 4, 1, 0, 0,
  0, 1, 1, 4, 10, 21, 37, 58, 80, 101, 118, 128, 134, 137, 139, 139,
  139, 139, 137, 134, 128, 117, 102, 80, 58, 37, 21, 10, 4, 1, 0, 0,
  0, 1, 1, 4, 10, 20, 35, 55, 77, 97, 112, 122, 128, 130, 132, 133,
  133, 132, 130, 128, 122, 112, 97, 77, 55, 35, 20, 10, 4, 1, 1, 0,
  0, 1, 1, 4, 9, 19, 32, 51, 70, 88, 103, 112, 117, 120, 121, 121,
  121, 121, 120, 117, 112, 103, 88, 70, 51, 32, 19, 9, 4, 1, 1, 0,
  0, 0, 1, 3, 7, 16, 28, 43, 61, 76, 88, 97, 102, 103, 104, 104,
  104, 104, 103, 102, 97, 88, 76, 61, 43, 28, 16, 7, 3, 1, 0, 0,
  0, 0, 1, 3, 6, 13, 22, 34, 48, 61, 70, 77, 80, 82, 83, 84,
  84, 83, 82, 80, 77, 70, 61, 48, 34, 22, 13, 6, 3, 1, 0, 0,
  0, 0, 1, 1, 4, 9, 16, 25, 34, 43, 50, 55, 58, 59, 60, 60,
  60, 60, 59, 58, 55, 50, 43, 34, 25, 16, 9, 4, 1, 1, 0, 0,
  0, 0, 1, 1, 3, 5, 10, 16, 22, 28, 32, 35, 37, 37, 38, 38,
  38, 38, 37, 37, 35, 32, 28, 22, 16, 10, 5, 3, 1, 1, 0, 0,
  0, 0, 0, 1, 1, 3, 5, 9, 13, 16, 19, 19, 21, 21, 22, 22,
  22, 22, 21, 21, 19, 19, 16, 13, 9, 5, 3, 1, 1, 0, 0, 0,
  0, 0, 0, 0, 1, 1, 3, 4, 6, 7, 9, 10, 10, 10, 10, 10,
  10, 10, 10, 10, 10, 9, 7, 6, 4, 3, 1, 1, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 1, 1, 1, 3, 3, 4, 4, 4, 4, 4, 4,
  4, 4, 4, 4, 4, 4, 3, 3, 1, 1, 1, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

GLuint
init_drop_shadow(void)
{
    GLuint t;

    glGenTextures (1, &t);
    if (t <= 0) abort();

    glBindTexture (GL_TEXTURE_2D, t);
#if 0
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    gluBuild2DMipmaps (GL_TEXTURE_2D, GL_ALPHA,
                       drop_shadow_width, drop_shadow_height,
                       GL_ALPHA, GL_UNSIGNED_BYTE,
                       drop_shadow_data);
#else
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_ALPHA, 
                  drop_shadow_width, drop_shadow_height, 0,
                  GL_ALPHA, GL_UNSIGNED_BYTE,
                  drop_shadow_data);
#endif

    return t;
}

void
draw_drop_shadow (GLuint t,
                  GLfloat x, GLfloat y, GLfloat z, GLfloat w, GLfloat h,
                  GLfloat r)
{
  /* Inner and outer boundaries of shadow. */
  const GLfloat li = x,     lo = li - r;
  const GLfloat ri = x + w, ro = ri + r;
  const GLfloat bi = y,     bo = bi - r;
  const GLfloat ti = y + h, to = ti + r;

  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glEnable (GL_TEXTURE_2D);
  glBindTexture (GL_TEXTURE_2D, t);

  glBegin (GL_QUADS);

  /* There's likely a better way to do this... */
  glTexCoord2f (0.0, 0.0); glVertex3f (lo, bo, z);
  glTexCoord2f (0.5, 0.0); glVertex3f (li, bo, z);
  glTexCoord2f (0.5, 0.5); glVertex3f (li, bi, z);
  glTexCoord2f (0.0, 0.5); glVertex3f (lo, bi, z);

  glTexCoord2f (0.5, 0.0); glVertex3f (li, bo, z);
  glTexCoord2f (0.5, 0.0); glVertex3f (ri, bo, z);
  glTexCoord2f (0.5, 0.5); glVertex3f (ri, bi, z);
  glTexCoord2f (0.5, 0.5); glVertex3f (li, bi, z);

  glTexCoord2f (0.5, 0.0); glVertex3f (ri, bo, z);
  glTexCoord2f (1.0, 0.0); glVertex3f (ro, bo, z);
  glTexCoord2f (1.0, 0.5); glVertex3f (ro, bi, z);
  glTexCoord2f (0.5, 0.5); glVertex3f (ri, bi, z);

  glTexCoord2f (0.5, 0.5); glVertex3f (ri, bi, z);
  glTexCoord2f (1.0, 0.5); glVertex3f (ro, bi, z);
  glTexCoord2f (1.0, 0.5); glVertex3f (ro, ti, z);
  glTexCoord2f (0.5, 0.5); glVertex3f (ri, ti, z);

  glTexCoord2f (0.5, 0.5); glVertex3f (ri, ti, z);
  glTexCoord2f (1.0, 0.5); glVertex3f (ro, ti, z);
  glTexCoord2f (1.0, 1.0); glVertex3f (ro, to, z);
  glTexCoord2f (0.5, 1.0); glVertex3f (ri, to, z);

  glTexCoord2f (0.5, 0.5); glVertex3f (li, ti, z);
  glTexCoord2f (0.5, 0.5); glVertex3f (ri, ti, z);
  glTexCoord2f (0.5, 1.0); glVertex3f (ri, to, z);
  glTexCoord2f (0.5, 1.0); glVertex3f (li, to, z);

  glTexCoord2f (0.0, 0.5); glVertex3f (lo, ti, z);
  glTexCoord2f (0.5, 0.5); glVertex3f (li, ti, z);
  glTexCoord2f (0.5, 1.0); glVertex3f (li, to, z);
  glTexCoord2f (0.0, 1.0); glVertex3f (lo, to, z);

  glTexCoord2f (0.0, 0.5); glVertex3f (lo, bi, z);
  glTexCoord2f (0.5, 0.5); glVertex3f (li, bi, z);
  glTexCoord2f (0.5, 0.5); glVertex3f (li, ti, z);
  glTexCoord2f (0.0, 0.5); glVertex3f (lo, ti, z);

  glEnd();
}
