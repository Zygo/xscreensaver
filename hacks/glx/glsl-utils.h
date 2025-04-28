/* glsl-utils.h --- support functions for GLSL in OpenGL hacks.
 * Copyright (c) 2020-2021 Carsten Steger <carsten@mirsanmir.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */


#ifndef __GLSL_UTILS_H__
#define __GLSL_UTILS_H__

#ifdef HAVE_GLSL


/* Column-major linearized matrix coordinate */
#define GLSL__LINCOOR(r,c,h) ((c)*(h)+(r))


/* Copy a 4x4 column-major matrix: c = m. */
extern void glsl_CopyMatrix(GLfloat c[16], GLfloat m[16]);

/* Create a 4x4 column-major identity matrix. */
extern void glsl_Identity(GLfloat c[16]);

/* Multiply two 4x4 column-major matrices: c = c*m. */
extern void glsl_MultMatrix(GLfloat c[16], GLfloat m[16]);

/* Multiply a 4×4 column-major matrix with a vector: o = m * v. */
extern void glsl_MultMatrixVector(GLfloat o[4], GLfloat m[16], GLfloat v[4]);

/* Multiply a 4x4 column-major matrix by a rotation matrix that rotates
   around the axis (x,y,z) by the angle angle: c = c*r(angle,x,y,z). */
extern void glsl_Rotate(GLfloat c[16], GLfloat angle, GLfloat x, GLfloat y,
                        GLfloat z);

/* Multiply a 4x4 column-major matrix by a matrix that translates an object
   by a translation vector: c = c * t(tx,ty,tz). */
extern void glsl_Translate(GLfloat c[16], GLfloat tx, GLfloat ty, GLfloat tz);

/* Multiply a 4x4 column-major matrix by a matrix that stretches, shrinks,
   or reflects an object along the axes: c = c*s(sx,sy,sz). */
extern void glsl_Scale(GLfloat c[16], GLfloat sx, GLfloat sy, GLfloat sz);

/* Add a look-at viewing matrix to a 4×4 column-major matrix. */
extern void glsl_LookAt(GLfloat c[16],
                        GLfloat eyex, GLfloat eyey, GLfloat eyez,
                        GLfloat centerx, GLfloat centery, GLfloat centerz,
                        GLfloat upx, GLfloat upy, GLfloat upz);

/* Add a perspective projection to a 4x4 column-major matrix. */
extern void glsl_Perspective(GLfloat c[16], GLfloat fovy, GLfloat aspect,
                             GLfloat z_near, GLfloat z_far);

/* Add an orthographic projection to a 4x4 column-major matrix. */
extern void glsl_Orthographic(GLfloat c[16], GLfloat left, GLfloat right,
                              GLfloat bottom, GLfloat top,
                              GLfloat nearval, GLfloat farval);

/* Get the OpenGL and GLSL versions. */
extern GLboolean glsl_GetGlAndGlslVersions(GLint *gl_major,
                                           GLint *gl_minor,
                                           GLint *glsl_major,
                                           GLint *glsl_minor,
                                           GLboolean *gl_gles3);

/* Compile and link a vertex and a Fragment shader into a GLSL program. */
extern GLboolean glsl_CompileAndLinkShaders(GLsizei vertex_shader_count,
                                        const GLchar **vertex_shader_source,
                                        GLsizei fragment_shader_count,
                                        const GLchar **fragment_shader_source,
                                        GLuint *shader_program);


#endif /* HAVE_GLSL */


#endif /* __GLSL_UTILS_H__ */
