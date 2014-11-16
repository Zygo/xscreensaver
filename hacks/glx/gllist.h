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

#ifndef __GLLIST_H__
#define __GLLIST_H__

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>

#ifdef HAVE_COCOA
#elif defined(HAVE_ANDROID)
# include <GLES/gl.h>
#else /* real X11 */
# include <GL/gl.h>
#endif

#ifdef HAVE_JWZGLES
# include "jwzgles.h"
#endif /* HAVE_JWZGLES */

struct gllist 
{
  GLenum format;
  GLenum primitive;
  int points;
  const void *data;
  struct gllist *next;
};

void renderList (const struct gllist *, int wire_p);
void renderListNormals (const struct gllist *, GLfloat length, int facesp);

#endif /* __GLLIST_H__ */
