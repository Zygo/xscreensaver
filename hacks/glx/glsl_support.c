/* glsl_support.c --- support functions for GLSL in OpenGL hacks.
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


#include "glsl_support.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>


#ifdef HAVE_GLSL


/* Column-major linearized matrix coordinate */
#define CMLINCOOR(r,c,h) ((c)*(h)+(r))


/* Copy a 4x4 column-major matrix: c = m. */
void glslsCopyMatrix(GLfloat c[16], GLfloat m[16])
{
  int i, j;

  for (j=0; j<4; j++)
    for (i=0; i<4; i++)
      c[CMLINCOOR(i,j,4)] = m[CMLINCOOR(i,j,4)];
}


/* Create a 4x4 column-major identity matrix. */
void glslsIdentity(GLfloat c[16])
{
  int i, j;

  for (j=0; j<4; j++)
    for (i=0; i<4; i++)
      c[CMLINCOOR(i,j,4)] = (i==j);
}


/* Multiply two 4x4 column-major matrices: c = c*m. */
void glslsMultMatrix(GLfloat c[16], GLfloat m[16])
{
  int i, j;
  GLfloat t[16];

  /* Copy c to t. */
  glslsCopyMatrix(t,c);
  /* Compute c = t*m. */
  for (j=0; j<4; j++)
    for (i=0; i<4; i++)
      c[CMLINCOOR(i,j,4)] = (t[CMLINCOOR(i,0,4)]*m[CMLINCOOR(0,j,4)]+
                             t[CMLINCOOR(i,1,4)]*m[CMLINCOOR(1,j,4)]+
                             t[CMLINCOOR(i,2,4)]*m[CMLINCOOR(2,j,4)]+
                             t[CMLINCOOR(i,3,4)]*m[CMLINCOOR(3,j,4)]);
}


/* Add a perspective projection to a 4x4 column-major matrix. */
void glslsPerspective(GLfloat c[16], GLfloat fovy, GLfloat aspect,
                      GLfloat z_near, GLfloat z_far)
{
  GLfloat m[16];
  double s, cot, dz;
  double rad;

  rad = fovy*(0.5f*(float)M_PI/180.0f);
  dz = z_far-z_near;
  s = sinf(rad);
  if (dz == 0.0f || s == 0.0f || aspect == 0.0f)
    return;
  cot = cosf(rad)/s;

  glslsIdentity(m);
  m[CMLINCOOR(0,0,4)] = cot/aspect;
  m[CMLINCOOR(1,1,4)] = cot;
  m[CMLINCOOR(2,2,4)] = -(z_far+z_near)/dz;
  m[CMLINCOOR(3,2,4)] = -1.0f;
  m[CMLINCOOR(2,3,4)] = -2.0f*z_near*z_far/dz;
  m[CMLINCOOR(3,3,4)] = 0.0f;
  glslsMultMatrix(c,m);
}


/* Add an orthographic projection to a 4x4 column-major matrix. */
void glslsOrthographic(GLfloat c[16], GLfloat left, GLfloat right,
                       GLfloat bottom, GLfloat top,
                       GLfloat nearval, GLfloat farval)
{
  GLfloat m[16];

  if (left == right || bottom == top || nearval == farval)
    return;

  glslsIdentity(m);
  m[CMLINCOOR(0,0,4)] = 2.0f/(right-left);
  m[CMLINCOOR(0,3,4)] = -(right+left)/(right-left);
  m[CMLINCOOR(1,1,4)] = 2.0f/(top-bottom);
  m[CMLINCOOR(1,3,4)] = -(top+bottom)/(top-bottom);
  m[CMLINCOOR(2,2,4)] = -2.0f/(farval-nearval);
  m[CMLINCOOR(2,3,4)] = -(farval+nearval)/(farval-nearval);
  glslsMultMatrix(c,m);
}


/* Get the OpenGL and GLSL versions. */
GLboolean glslsGetGlAndGlslVersions(GLint *gl_major, GLint *gl_minor,
                                    GLint *glsl_major, GLint *glsl_minor)
{
  const char *gl_version, *glsl_version;
  int n;

  *gl_major = -1;
  *gl_minor = -1;
  *glsl_major = 1;
  *glsl_minor = -1;
  gl_version = (const char *)glGetString(GL_VERSION);
  if (gl_version == NULL)
    return GL_FALSE;
  n = sscanf(gl_version,"%d.%d",gl_major,gl_minor);
  if (n != 2)
    return GL_FALSE;
  glsl_version = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
  if (glsl_version == NULL)
    return GL_FALSE;
  n = sscanf(glsl_version,"%d.%d",glsl_major,glsl_minor);
  if (n != 2)
    return GL_FALSE;
  return GL_TRUE;
}


/* #define PRINT_ERRORS */
#undef PRINT_ERRORS

#ifdef PRINT_ERRORS

#define PRINT_COMPILE_ERROR(shader) print_compile_error(shader)
#define PRINT_LINK_ERROR(program) print_link_error(program)

static void print_compile_error(GLuint shader)
{
  GLint length_log;
  GLsizei length;
  GLchar *log;

  glGetShaderiv(shader,GL_INFO_LOG_LENGTH,&length_log);
  if (length_log > 0)
  {
    log = malloc(length_log*sizeof(*log));
    if (log != NULL)
    {
      glGetShaderInfoLog(shader,length_log,&length,log);
      fprintf(stderr,"%s",log);
    }
    free(log);
  }
}


static void print_link_error(GLuint program)
{
  GLint length_log;
  GLsizei length;
  GLchar *log;

  glGetProgramiv(program,GL_INFO_LOG_LENGTH,&length_log);
  if (length_log > 0)
  {
    log = malloc(length_log*sizeof(*log));
    if (log != NULL)
    {
      glGetProgramInfoLog(program,length_log,&length,log);
      fprintf(stderr,"%s",log);
    }
    free(log);
  }
}

#else

#define PRINT_COMPILE_ERROR(shader)
#define PRINT_LINK_ERROR(program)

#endif


/* Compile and link a vertex and a Fragment shader into a GLSL program. */
GLboolean glslsCompileAndLinkShaders(GLsizei vertex_shader_count,
                                     const GLchar **vertex_shader_source,
                                     GLsizei fragment_shader_count,
                                     const GLchar **fragment_shader_source,
                                     GLuint *shader_program)
{
  GLuint vertex_shader, fragment_shader;
  GLint status;

  /* Create and compile the vertex shader. */
  vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  if (vertex_shader == 0)
    return GL_FALSE;
  fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  if (fragment_shader == 0)
  {
    glDeleteShader(vertex_shader);
    return GL_FALSE;
  }
  glShaderSource(vertex_shader,vertex_shader_count,vertex_shader_source,
                 NULL);
  glShaderSource(fragment_shader,fragment_shader_count,fragment_shader_source,
                 NULL);
  glCompileShader(vertex_shader);
  glGetShaderiv(vertex_shader,GL_COMPILE_STATUS,&status);
  if (status == GL_FALSE)
  {
    PRINT_COMPILE_ERROR(vertex_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return GL_FALSE;
  }
  glCompileShader(fragment_shader);
  glGetShaderiv(fragment_shader,GL_COMPILE_STATUS,&status);
  if (status == GL_FALSE)
  {
    PRINT_COMPILE_ERROR(fragment_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return GL_FALSE;
  }
  *shader_program = glCreateProgram();
  if (*shader_program == 0)
  {
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return GL_FALSE;
  }
  glAttachShader(*shader_program,vertex_shader);
  glAttachShader(*shader_program,fragment_shader);
  glLinkProgram(*shader_program);
  glGetProgramiv(*shader_program,GL_LINK_STATUS,&status);
  if (status == GL_FALSE)
  {
    PRINT_LINK_ERROR(*shader_program);
    glDeleteProgram(*shader_program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return GL_FALSE;
  }
  /* Once the shader program has compiled successfully, we can delete the
     vertex and fragment shaders. */
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  return GL_TRUE;
}


#endif /* HAVE_GLSL */
