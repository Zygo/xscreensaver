/* tangram, Copyright (c) 2005 Jeremy English <jhe@jeremyenglish.org>
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

#ifndef HAVE_COCOA
# include <GL/gl.h>
#endif

#ifdef HAVE_JWZGLES
# include "jwzgles.h"
#endif /* HAVE_JWZGLES */

#include "tangram_shapes.h"

#define small_scale  ( 1 )
#define large_scale  ( 2 )
#define medium_scale ( 1.414213562 )

#define alpha (0.05)

static void tri_45_90(int wire)
{
    GLfloat vertices[][3] = {
        {0, alpha, 0},
        {0, alpha, 1},
        {1, alpha, 0},
        {0, -alpha, 0},
        {0, -alpha, 1},
        {1, -alpha, 0}
    };

    glBegin((wire) ? GL_LINE_LOOP : GL_TRIANGLES);

    glNormal3f(0, 1, 0);
    glVertex3fv(vertices[0]);
    glNormal3f(0, 1, 0);
    glVertex3fv(vertices[2]);
    glNormal3f(0, 1, 0);
    glVertex3fv(vertices[1]);

    glNormal3f(0, -1, 0);
    glVertex3fv(vertices[3]);
    glNormal3f(0, -1, 0);
    glVertex3fv(vertices[4]);
    glNormal3f(0, -1, 0);
    glVertex3fv(vertices[5]);
    glEnd();

    glBegin((wire) ? GL_LINE_LOOP : GL_QUADS);
    glNormal3f(1, 0, 1);
    glVertex3fv(vertices[2]);
    glNormal3f(1, 0, 1);
    glVertex3fv(vertices[5]);
    glNormal3f(1, 0, 1);
    glVertex3fv(vertices[4]);
    glNormal3f(1, 0, 1);
    glVertex3fv(vertices[1]);

    glNormal3f(-1, 0, 0);
    glVertex3fv(vertices[0]);
    glNormal3f(-1, 0, 0);
    glVertex3fv(vertices[1]);
    glNormal3f(-1, 0, 0);
    glVertex3fv(vertices[4]);
    glNormal3f(-1, 0, 0);
    glVertex3fv(vertices[3]);

    glNormal3f(0, 0, -1);
    glVertex3fv(vertices[0]);
    glNormal3f(0, 0, -1);
    glVertex3fv(vertices[3]);
    glNormal3f(0, 0, -1);
    glVertex3fv(vertices[5]);
    glNormal3f(0, 0, -1);
    glVertex3fv(vertices[2]);
    glEnd();
}

static
void unit_cube(int wire)
{
    glBegin((wire) ? GL_LINE_LOOP : GL_QUADS);


    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.0f, alpha, 0.0f);
    glVertex3f(0.0f, alpha, 1.0f);
    glVertex3f(1.0f, alpha, 1.0f);
    glVertex3f(1.0f, alpha, 0.0f);

    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0.0f, -alpha, 1.0f);
    glVertex3f(1.0f, -alpha, 1.0f);
    glVertex3f(1.0f, alpha, 1.0f);
    glVertex3f(0.0f, alpha, 1.0f);

    glNormal3f(0.0f, 0.0f, -1.0f);
    glVertex3f(0.0f, -alpha, 0.0f);
    glVertex3f(0.0f, alpha, 0.0f);
    glVertex3f(1.0f, alpha, 0.0f);
    glVertex3f(1.0f, -alpha, 0.0f);

    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f(1.0f, -alpha, 0.0f);
    glVertex3f(1.0f, alpha, 0.0f);
    glVertex3f(1.0f, alpha, 1.0f);
    glVertex3f(1.0f, -alpha, 1.0f);

    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, -alpha, 0.0f);
    glVertex3f(0.0f, -alpha, 1.0f);
    glVertex3f(0.0f, alpha, 1.0f);
    glVertex3f(0.0f, alpha, 0.0f);

    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(0.0f, -alpha, 0.0f);
    glVertex3f(1.0f, -alpha, 0.0f);
    glVertex3f(1.0f, -alpha, 1.0f);
    glVertex3f(0.0f, -alpha, 1.0f);

    glEnd();
}

static
void unit_rhomboid(int wire)
{
    glBegin((wire) ? GL_LINE_LOOP : GL_QUADS);

    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0, alpha, 0);
    glVertex3f(1, alpha, 1);
    glVertex3f(1, alpha, 2);
    glVertex3f(0, alpha, 1);

    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(0, -alpha, 0);
    glVertex3f(0, -alpha, 1);
    glVertex3f(1, -alpha, 2);
    glVertex3f(1, -alpha, 1);

    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(0, alpha, 0);
    glVertex3f(0, alpha, 1);
    glVertex3f(0, -alpha, 1);
    glVertex3f(0, -alpha, 0);


    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0, alpha, 1);
    glVertex3f(1, alpha, 2);
    glVertex3f(1, -alpha, 2);
    glVertex3f(0, -alpha, 1);

    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f(1, alpha, 1);
    glVertex3f(1, -alpha, 1);
    glVertex3f(1, -alpha, 2);
    glVertex3f(1, alpha, 2);

    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0, alpha, 0);
    glVertex3f(0, -alpha, 0);
    glVertex3f(1, -alpha, 1);
    glVertex3f(1, alpha, 1);

    glEnd();
}

/* All of the pieces have the same thickness so all of the Y values are the same */

GLuint get_sm_tri_dl(int wire)
{
    GLuint triangle = glGenLists(1);
    glNewList(triangle, GL_COMPILE);
    glScalef(small_scale, small_scale, small_scale);
    tri_45_90(wire);
    glEndList();
    return triangle;
}

GLuint get_lg_tri_dl(int wire)
{
    GLuint triangle = glGenLists(1);
    glNewList(triangle, GL_COMPILE);
    glScalef(large_scale, small_scale, large_scale);
    tri_45_90(wire);
    glEndList();
    return triangle;
}

GLuint get_md_tri_dl(int wire)
{
    GLuint triangle = glGenLists(1);
    glNewList(triangle, GL_COMPILE);
    glScalef(medium_scale, small_scale, medium_scale);
    tri_45_90(wire);
    glEndList();
    return triangle;
}

GLuint get_square_dl(int wire)
{
    GLuint square = glGenLists(1);
    glNewList(square, GL_COMPILE);
    glScalef(small_scale, small_scale, small_scale);
    unit_cube(wire);
    glEndList();
    return square;
}

GLuint get_rhomboid_dl(int wire)
{
    GLuint rhomboid = glGenLists(1);
    glNewList(rhomboid, GL_COMPILE);
    glScalef(small_scale, small_scale, small_scale);
    unit_rhomboid(wire);
    glEndList();
    return rhomboid;
}
