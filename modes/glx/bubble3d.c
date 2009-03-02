/* -*- Mode: C; tab-width: 4 -*- */
/* bubble3d.c - 3D bubbles  */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)bubble3d.c  4.11 98/06/16 xlockmore";

#endif

/*-
 * BUBBLE3D (C) 1998 Richard W.M. Jones.
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * Revision History:
 * 16-Jun-98: Written.
 *
 * bubble.c: This code is responsible for creating and managing
 * bubbles over their lifetime.
 * The bubbles may be drawn inside out.
 */

#include "bubble3d.h"

typedef struct bubble {
	GLfloat    *contributions;	/* List of contributions from each
					 * nudge to each vertex. This list has
					 * length nr_vertices * nr_nudge_axes.
					 */
	GLfloat     x, y, z;	/* (x,y,z) location of the bubble. */
	GLfloat     scale;	/* Scaling factor applied to bubble. */
	GLfloat     y_incr, scale_incr;		/* Change in y and scale each frame. */
	GLfloat     rotx, roty, rotz;	/* Current rotation. */
	GLfloat     rotx_incr, roty_incr, rotz_incr;	/* Amount by which we increase
							 * rotation each step.
							 */
	GLfloat    *nudge_angle;	/* Current angle (radians) of each
					 * nudge. This list has length nr_nudge_axes.
					 */
	GLfloat    *nudge_angle_incr;	/* Amount by which we increase each nudge
					 * angle in each frame.
					 */
} bubble;

/* Should be taken care of already... but just in case */
#if !defined( __GNUC__ ) && !defined(__cplusplus) && !defined(c_plusplus)
#undef inline
#define inline			/* */
#endif
static inline void
normalize(GLfloat v[3])
{
	GLfloat     d = (GLfloat) sqrt((double) (v[0] * v[0] + v[1] * v[1] +
						 v[2] * v[2]));

	if (d != 0) {
		v[0] /= d;
		v[1] /= d;
		v[2] /= d;
	} else {
		v[0] = v[1] = v[2] = 0;
	}
}

static inline GLfloat
dotprod(GLfloat * v1, GLfloat * v2)
{
	return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

static inline GLfloat
max(GLfloat a, GLfloat b)
{
	return a > b ? a : b;
}

/* Create a new bubble. */
void       *
glb_bubble_new(GLfloat x, GLfloat y, GLfloat z, GLfloat scale,
	       GLfloat y_incr, GLfloat scale_incr)
{
	int         i, j;

	/* GLfloat axes [glb_config.nr_nudge_axes][3]; */
	GLfloat     axes[5][3];	/* HARD CODED for SunCC */
	int         nr_vertices;
	glb_vertex *vertices = glb_sphere_get_vertices(&nr_vertices);

	bubble     *b = (bubble *) malloc(sizeof *b);

	if (b == 0)
		return 0;

	b->contributions = (GLfloat *) malloc(sizeof (GLfloat) * nr_vertices *
					      glb_config.nr_nudge_axes);
	if (b->contributions == 0) {
		(void) free((void *) b);
		return 0;
	}
	b->nudge_angle = (GLfloat *) malloc(sizeof (GLfloat) * glb_config.nr_nudge_axes);
	if (b->nudge_angle == 0) {
		(void) free((void *) b->contributions);
		(void) free((void *) b);
		return 0;
	}
	b->nudge_angle_incr = (GLfloat *) malloc(sizeof (GLfloat) * glb_config.nr_nudge_axes);
	if (b->nudge_angle_incr == 0) {
		(void) free((void *) b->nudge_angle);
		(void) free((void *) b->contributions);
		(void) free((void *) b);
		return 0;
	}
	/* Initialize primitive elements. */
	b->x = x;
	b->y = y;
	b->z = z;
	b->scale = scale;
	b->y_incr = y_incr;
	b->scale_incr = scale_incr;
	b->rotx = b->roty = b->rotz = 0;
	b->rotx_incr = glb_drand() * glb_config.rotation_factor * 2
		- glb_config.rotation_factor;
	b->roty_incr = glb_drand() * glb_config.rotation_factor * 2
		- glb_config.rotation_factor;
	b->rotz_incr = glb_drand() * glb_config.rotation_factor * 2
		- glb_config.rotation_factor;

	/* Initialize the nudge angle arrays. */
	for (i = 0; i < glb_config.nr_nudge_axes; ++i) {
		b->nudge_angle[i] = 0;
		b->nudge_angle_incr[i] = glb_drand() * glb_config.nudge_angle_factor;
	}

	/* Choose some random nudge axes. */
	for (i = 0; i < glb_config.nr_nudge_axes; ++i) {
		axes[i][0] = glb_drand() * 2 - 1;
		axes[i][1] = glb_drand() * 2 - 1;
		axes[i][2] = glb_drand() * 2 - 1;
		normalize(axes[i]);
	}

	/* Calculate the contribution that each nudge axis has on each vertex. */
	for (i = 0; i < nr_vertices; ++i)
		for (j = 0; j < glb_config.nr_nudge_axes; ++j)
			b->contributions[i * glb_config.nr_nudge_axes + j]
				= max(0.0, dotprod(vertices[i], axes[j]));

	return (void *) b;
}

/* Delete a bubble and free up all memory. */
void
glb_bubble_delete(void *bb)
{
	bubble     *b = (bubble *) bb;

	if (b != NULL) {
		if (b->nudge_angle_incr) {
			(void) free((void *) b->nudge_angle_incr);
		}
		if (b->nudge_angle) {
			(void) free((void *) b->nudge_angle);
		}
		if (b->contributions) {
			(void) free((void *) b->contributions);
		}
		(void) free((void *) b);
		b = (bubble *) NULL;
	}
}

/* Rotate and wobble a bubble by a single step. */
void
glb_bubble_step(void *bb)
{
	int         i;
	bubble     *b = (bubble *) bb;

	/* Update the rotation. */
	b->rotx += b->rotx_incr;
	b->roty += b->roty_incr;
	b->rotz += b->rotz_incr;

	/* Update the nudge angles. */
	for (i = 0; i < glb_config.nr_nudge_axes; ++i)
		b->nudge_angle[i] += b->nudge_angle_incr[i];

	/* Move it upwards & outwards. */
	b->y += b->y_incr;
	b->scale += b->scale_incr;
}

/* Draw a bubble. */
void
glb_bubble_draw(void *bb)
{
	int         i, j;
	bubble     *b = (bubble *) bb;
	int         nr_vertices;
	glb_vertex *vertices = glb_sphere_get_vertices(&nr_vertices);
	int         nr_triangles;
	glb_triangle *triangles = glb_sphere_get_triangles(&nr_triangles);
	glb_vertex *new_vertices;

	new_vertices = (glb_vertex *) malloc(sizeof (glb_vertex) * nr_vertices);
	/* Calculate the vertices of this bubble, factoring in each nudge axis. */
	for (i = 0; i < nr_vertices; ++i) {
		GLfloat     s = 0;

		for (j = 0; j < glb_config.nr_nudge_axes; ++j)
			s += ((GLfloat) cos((double) (b->nudge_angle[j])) *
			      glb_config.nudge_factor - glb_config.nudge_factor / 2) *
				b->contributions[i * glb_config.nr_nudge_axes + j];

		new_vertices[i][0] = vertices[i][0] * (s + 1);
		new_vertices[i][1] = vertices[i][1] * (s + 1);
		new_vertices[i][2] = vertices[i][2] * (s + 1);
	}

	glPushMatrix();

	/* Apply translation, rotation and scalings. */
	glTranslatef(b->x, b->y, b->z);

	glRotatef(b->rotx, 1, 0, 0);
	glRotatef(b->roty, 0, 1, 0);
	glRotatef(b->rotz, 0, 0, 1);

	glScalef(b->scale, b->scale, b->scale);

	/* Draw the bubble. */
	glBegin(GL_TRIANGLES);
	for (i = 0; i < nr_triangles; ++i) {
		glNormal3fv(new_vertices[triangles[i][0]]);
		glVertex3fv(new_vertices[triangles[i][0]]);
		glNormal3fv(new_vertices[triangles[i][1]]);
		glVertex3fv(new_vertices[triangles[i][1]]);
		glNormal3fv(new_vertices[triangles[i][2]]);
		glVertex3fv(new_vertices[triangles[i][2]]);
	}
	glEnd();
	glPopMatrix();
	(void) free((void *) new_vertices);
}

/* Return y value. */
GLfloat
glb_bubble_get_y(void *bb)
{
	bubble     *b = (bubble *) bb;

	return b->y;
}
