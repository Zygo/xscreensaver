/* glsl_support.h --- support functions for GLSL in OpenGL hacks.
 * Copyright (c) 2020-2020 Carsten Steger <carsten@mirsanmir.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */


#ifndef __GLSL_SUPPORT_H__
#define __GLSL_SUPPORT_H__

/* GL/glext.h must be included before the rest of the header files are
   included so we can define GL_GLEXT_PROTOTYPES.  Therefore, we must
   include config.h here to have HAVE_GLSL initialized and then include
   GL/glext.h before it is included indirectly via the other includes. */
#ifdef HAVE_CONFIG_H
# include <config.h>
# ifdef HAVE_GLSL
#  define GL_GLEXT_PROTOTYPES
#  include <GL/gl.h>
#  include <GL/glext.h>
# endif /* HAVE_GLSL */
#endif /* HAVE_CONFIG_H */


#ifdef HAVE_GLSL


/* Copy a 4x4 column-major matrix: c = m. */
extern void glslsCopyMatrix(GLfloat c[16], GLfloat m[16]);

/* Create a 4x4 column-major identity matrix. */
extern void glslsIdentity(GLfloat c[16]);

/* Multiply two 4x4 column-major matrices: c = c*m. */
extern void glslsMultMatrix(GLfloat c[16], GLfloat m[16]);

/* Add a perspective projection to a 4x4 column-major matrix. */
extern void glslsPerspective(GLfloat c[16], GLfloat fovy, GLfloat aspect,
                             GLfloat z_near, GLfloat z_far);

/* Add an orthographic projection to a 4x4 column-major matrix. */
extern void glslsOrthographic(GLfloat c[16], GLfloat left, GLfloat right,
                              GLfloat bottom, GLfloat top,
                              GLfloat nearval, GLfloat farval);

/* Get the OpenGL and GLSL versions. */
extern GLboolean glslsGetGlAndGlslVersions(GLint *gl_major,
                                           GLint *gl_minor,
                                           GLint *glsl_major,
                                           GLint *glsl_minor);

/* Compile and link a vertex and a Fragment shader into a GLSL program. */
extern GLboolean glslsCompileAndLinkShaders(GLsizei vertex_shader_count,
                                        const GLchar **vertex_shader_source,
                                        GLsizei fragment_shader_count,
                                        const GLchar **fragment_shader_source,
                                        GLuint *shader_program);


#endif /* HAVE_GLSL */


#endif /* __GLSL_SUPPORT_H__ */
