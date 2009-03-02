/* tube, Copyright (c) 2001, 2003 Jamie Zawinski <jwz@jwz.org>
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

#ifndef __TUBE_H__
#define __TUBE_H__

extern void tube (GLfloat x1, GLfloat y1, GLfloat z1,
                  GLfloat x2, GLfloat y2, GLfloat z2,
                  GLfloat diameter, GLfloat cap_size,
                  int faces, int smooth, int caps_p, int wire_p);

extern void cone (GLfloat x1, GLfloat y1, GLfloat z1,
                  GLfloat x2, GLfloat y2, GLfloat z2,
                  GLfloat diameter, GLfloat cap_size,
                  int faces, int smooth, int cap_p,  int wire_p);

#endif /* __TUBE_H__ */
