/* glsl-utils.c --- support functions for GLSL in OpenGL hacks.
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

#include "screenhackI.h"
#include "glsl-utils.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#ifdef HAVE_GLSL

extern const char *progname;

/* Copy a 4x4 column-major matrix: c = m. */
void glsl_CopyMatrix(GLfloat c[16], GLfloat m[16])
{
  int i, j;

  for (j=0; j<4; j++)
    for (i=0; i<4; i++)
      c[GLSL__LINCOOR(i,j,4)] = m[GLSL__LINCOOR(i,j,4)];
}


/* Create a 4x4 column-major identity matrix. */
void glsl_Identity(GLfloat c[16])
{
  int i, j;

  for (j=0; j<4; j++)
    for (i=0; i<4; i++)
      c[GLSL__LINCOOR(i,j,4)] = (i==j);
}


/* Multiply two 4x4 column-major matrices: c = c*m. */
void glsl_MultMatrix(GLfloat c[16], GLfloat m[16])
{
  int i, j;
  GLfloat t[16];

  /* Copy c to t. */
  glsl_CopyMatrix(t,c);
  /* Compute c = t*m. */
  for (j=0; j<4; j++)
    for (i=0; i<4; i++)
      c[GLSL__LINCOOR(i,j,4)] =
        (t[GLSL__LINCOOR(i,0,4)]*m[GLSL__LINCOOR(0,j,4)]+
         t[GLSL__LINCOOR(i,1,4)]*m[GLSL__LINCOOR(1,j,4)]+
         t[GLSL__LINCOOR(i,2,4)]*m[GLSL__LINCOOR(2,j,4)]+
         t[GLSL__LINCOOR(i,3,4)]*m[GLSL__LINCOOR(3,j,4)]);
}


/* Multiply a 4x4 column-major matrix by a rotation matrix that rotates
   around the axis (x,y,z) by the angle angle: c = c*r(angle,x,y,z). */
void glsl_Rotate(GLfloat c[16], GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
  GLfloat l, t, ct, st, omct, n[3], r[16];

  l = sqrtf(x*x+y*y+z*z);
  n[0] = x/l;
  n[1] = y/l;
  n[2] = z/l;
  t = angle*M_PI/180.0f;
  ct = cosf(t);
  st = sinf(t);
  omct = 1.0f-ct;

  r[GLSL__LINCOOR(0,0,4)] = n[0]*n[0]*omct+ct;
  r[GLSL__LINCOOR(1,0,4)] = n[0]*n[1]*omct+n[2]*st;
  r[GLSL__LINCOOR(2,0,4)] = n[0]*n[2]*omct-n[1]*st;
  r[GLSL__LINCOOR(3,0,4)] = 0.0f;

  r[GLSL__LINCOOR(0,1,4)] = n[0]*n[1]*omct-n[2]*st;
  r[GLSL__LINCOOR(1,1,4)] = n[1]*n[1]*omct+ct;
  r[GLSL__LINCOOR(2,1,4)] = n[1]*n[2]*omct+n[0]*st;
  r[GLSL__LINCOOR(3,1,4)] = 0.0f;

  r[GLSL__LINCOOR(0,2,4)] = n[0]*n[2]*omct+n[1]*st;
  r[GLSL__LINCOOR(1,2,4)] = n[1]*n[2]*omct-n[0]*st;
  r[GLSL__LINCOOR(2,2,4)] = n[2]*n[2]*omct+ct;
  r[GLSL__LINCOOR(3,2,4)] = 0.0f;

  r[GLSL__LINCOOR(0,3,4)] = 0.0f;
  r[GLSL__LINCOOR(1,3,4)] = 0.0f;
  r[GLSL__LINCOOR(2,3,4)] = 0.0f;
  r[GLSL__LINCOOR(3,3,4)] = 1.0f;

  glsl_MultMatrix(c,r);
}


/* Multiply a 4x4 column-major matrix by a matrix that stretches, shrinks,
   or reflects an object along the axes: c = c*m(sx,sy,sz). */
void glsl_Scale(GLfloat c[16], GLfloat sx, GLfloat sy, GLfloat sz)
{
  int i;

  for (i=0; i<4; i++)
  {
    c[GLSL__LINCOOR(i,0,4)] *= sx;
    c[GLSL__LINCOOR(i,1,4)] *= sy;
    c[GLSL__LINCOOR(i,2,4)] *= sz;
  }
}


/* Multiply a 4x4 column-major matrix by a matrix that translates an object
   by a translation vector: c = c*t(tx,ty,tz). */
void glsl_Translate(GLfloat c[16], GLfloat tx, GLfloat ty, GLfloat tz)
{
  int i;

  for (i=0; i<4; i++)
  {
    c[GLSL__LINCOOR(i,3,4)] = (tx*c[GLSL__LINCOOR(i,0,4)]+
                               ty*c[GLSL__LINCOOR(i,1,4)]+
                               tz*c[GLSL__LINCOOR(i,2,4)]+
                               c[GLSL__LINCOOR(i,3,4)]);
  }
}


/* Add a perspective projection to a 4x4 column-major matrix. */
void glsl_Perspective(GLfloat c[16], GLfloat fovy, GLfloat aspect,
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

  glsl_Identity(m);
  m[GLSL__LINCOOR(0,0,4)] = cot/aspect;
  m[GLSL__LINCOOR(1,1,4)] = cot;
  m[GLSL__LINCOOR(2,2,4)] = -(z_far+z_near)/dz;
  m[GLSL__LINCOOR(3,2,4)] = -1.0f;
  m[GLSL__LINCOOR(2,3,4)] = -2.0f*z_near*z_far/dz;
  m[GLSL__LINCOOR(3,3,4)] = 0.0f;
  glsl_MultMatrix(c,m);
}


/* Add an orthographic projection to a 4x4 column-major matrix. */
void glsl_Orthographic(GLfloat c[16], GLfloat left, GLfloat right,
                       GLfloat bottom, GLfloat top,
                       GLfloat nearval, GLfloat farval)
{
  GLfloat m[16];

  if (left == right || bottom == top || nearval == farval)
    return;

  glsl_Identity(m);
  m[GLSL__LINCOOR(0,0,4)] = 2.0f/(right-left);
  m[GLSL__LINCOOR(0,3,4)] = -(right+left)/(right-left);
  m[GLSL__LINCOOR(1,1,4)] = 2.0f/(top-bottom);
  m[GLSL__LINCOOR(1,3,4)] = -(top+bottom)/(top-bottom);
  m[GLSL__LINCOOR(2,2,4)] = -2.0f/(farval-nearval);
  m[GLSL__LINCOOR(2,3,4)] = -(farval+nearval)/(farval-nearval);
  glsl_MultMatrix(c,m);
}


/* Get the OpenGL and GLSL versions. */
GLboolean glsl_GetGlAndGlslVersions(GLint *gl_major, GLint *gl_minor,
                                    GLint *glsl_major, GLint *glsl_minor,
                                    GLboolean *gl_gles3)
{
  const char *gl_version, *glsl_version;
  int n;
  const char *err = 0;

  *gl_major = -1;
  *gl_minor = -1;
  *glsl_major = 1;
  *glsl_minor = -1;
  *gl_gles3 = GL_FALSE;
  gl_version = (const char *)glGetString(GL_VERSION);
  glsl_version = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
  if (gl_version == NULL || glsl_version == NULL)
    {
      err = "GL version unknown";
      goto DONE;
    }
  if (!strncmp(gl_version,"OpenGL ES",9))
  {
    if (!strncmp(glsl_version,"OpenGL ES GLSL ES",17))
      *gl_gles3 = GL_TRUE;
    else
      {
        err = "GLSL not supported";
        goto DONE;
      }
  }
  if (*gl_gles3)
    n = sscanf(&gl_version[9],"%d.%d",gl_major,gl_minor);
  else
    n = sscanf(gl_version,"%d.%d",gl_major,gl_minor);
  if (n != 2)
    {
      err = "GL version number unparsable";
      goto DONE;
    }
  if (*gl_gles3)
    n = sscanf(&glsl_version[17],"%d.%d",glsl_major,glsl_minor);
  else
    n = sscanf(glsl_version,"%d.%d",glsl_major,glsl_minor);
  if (n != 2)
    {
      err = "GLSL version number unparsable";
      goto DONE;
    }

 DONE:

#if 0/*# ifndef __OPTIMIZE__*/
  if (err)
    fprintf (stderr, "%s: GLSL: %s\n", progname, err);
  else
    fprintf (stderr, "%s: GLSL available: GL=%d.%d, GLSL=%d.%d GLES3=%d\n",
             progname,
             *gl_major, *gl_minor, 
             *glsl_major, *glsl_minor,
             *gl_gles3 ? 1 : 0);
# endif

  return (err ? GL_FALSE : GL_TRUE);
}


#define PRINT_ERRORS
/* #undef PRINT_ERRORS */

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
      fprintf(stderr,"%s: %s",progname,log);
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
      fprintf(stderr,"%s: %s",progname,log);
    }
    free(log);
  }
}

#else

#define PRINT_COMPILE_ERROR(shader)
#define PRINT_LINK_ERROR(program)

#endif


/* Compile and link a vertex and a Fragment shader into a GLSL program. */
GLboolean glsl_CompileAndLinkShaders(GLsizei vertex_shader_count,
                                     const GLchar **vertex_shader_source,
                                     GLsizei fragment_shader_count,
                                     const GLchar **fragment_shader_source,
                                     GLuint *shader_program)
{
  GLuint vertex_shader, fragment_shader;
  GLint status;
  const char *err = 0;

  /* Create and compile the vertex shader. */
  vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  if (vertex_shader == 0)
    return GL_FALSE;
  fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  if (fragment_shader == 0)
  {
    glDeleteShader(vertex_shader);
    err = "unable to create fragment shader";
    goto DONE;
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
    err = "vertex shader compilation failed";
    goto DONE;
  }
  glCompileShader(fragment_shader);
  glGetShaderiv(fragment_shader,GL_COMPILE_STATUS,&status);
  if (status == GL_FALSE)
  {
    PRINT_COMPILE_ERROR(fragment_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    err = "fragment shader compilation failed";
    goto DONE;
  }
  *shader_program = glCreateProgram();
  if (*shader_program == 0)
  {
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    err = "shader creation failed";
    goto DONE;
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
    err = "shader attachment failed";
    goto DONE;
  }
  /* Once the shader program has compiled successfully, we can delete the
     vertex and fragment shaders. */
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

 DONE:
  if (err)
    fprintf (stderr, "%s: GLSL: %s\n", progname, err);
#if 0/*# ifndef __OPTIMIZE__*/
  else
    fprintf (stderr, "%s: GLSL: shaders initialized\n", progname);
# endif

  return (err ? GL_FALSE : GL_TRUE);
}


#endif /* HAVE_GLSL */
