/* dropshadow.h, Copyright (c) 2009 Jens Kilian <jjk@acm.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#ifndef __DROPSHADOW_H__
#define __DROPSHADOW_H__

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_COCOA
# ifndef HAVE_JWZGLES
#  include <OpenGL/glu.h>
# endif
#else /* !HAVE_COCOA */
# include <GL/gl.h>
# include <GL/glu.h>
#endif /* !HAVE_COCOA */

#ifdef HAVE_JWZGLES
# include "jwzgles.h"
#endif /* HAVE_JWZGLES */

/* Initialize drop shadow texture, return a texture ID.
 */
GLuint
init_drop_shadow(void);

/* Draw a drop shadow around a rectangle.

   t                Texture ID (as returned by init_drop_shadow()).
   x, y, z; w, h    Position (left bottom), depth and size of rectangle.
   r                Radius of drop shadow.

   The shadow will be drawn using the current color.
 */

void
draw_drop_shadow (GLuint t,
                  GLfloat x, GLfloat y, GLfloat z, GLfloat w, GLfloat h,
                  GLfloat r);

#endif /* __DROPSHADOW_H__ */
