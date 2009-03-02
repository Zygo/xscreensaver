/* GLBUBBLES (C) 1998 Richard W.M. Jones. */

#ifndef __bubbles3d_h__
#define __bubbles3d_h__

#include <X11/Intrinsic.h>

#ifdef STANDALONE
# include <math.h>
# include "xlockmoreI.h"	/* from the xscreensaver distribution */
#else /* !STANDALONE */
# include "xlock.h"		/* from the xlockmore distribution */
#endif /* !STANDALONE */

#include <GL/gl.h>
#include <GL/glu.h>

/* Static configuration. */
#define GLB_SLOW_GL          0	/* Set this if you have a slow GL
				   * implementation. If you have an accelerated
				   * graphics card, set this to 0.
				 */
#define GLB_USE_BLENDING     0	/* Use alpha feature to create see-through
				   * bubbles.
				 */
#define GLB_VERTICES_EPSILON 0.0005	/* How close are identical vertices? */

/* Configuration structure. */
struct glb_config {
	int         subdivision_depth;	/* Controls how many triangles are in
					 * each bubble. 2 and 3 are good values.
					 */
	int         nr_nudge_axes;	/* Number of directions in which each
					 * bubble gets stretched. Values between
					 * 3 and 7 seem to produce good results.
					 */
	GLfloat     nudge_angle_factor;		/* Max. amount of rotation in nudge angles.
						 * Controls the amount of `wobble' we see,
						 * and 0.3 seems to work well.
						 */
	GLfloat     nudge_factor;	/* Max. displacement of any single nudge.
					 * Controls the amount of the wobble. Depends
					 * on NR_NUDGE_AXES, and must not exceed
					 * 1 / NR_NUDGE_AXES. 0.1 is good.
					 */
	GLfloat     rotation_factor;	/* Max. amount by which bubbles rotate. */
	int         create_bubbles_every;	/* How often to create new bubbles. */
	int         max_bubbles;	/* Max. number of bubbles to create. */
	double      p_bubble_group[4];	/* Probabilities of creating 1, 2, 3, 4
					 * bubbles in a group. Cumulative.
					 */
	GLfloat     max_size;	/* Max. size. */
	GLfloat     min_size;	/* Min. size of bubbles. */
	GLfloat     max_speed;	/* Max. speed. */
	GLfloat     min_speed;	/* Min. speed of bubbles. */
	GLfloat     scale_factor;	/* Factor by which bubbles scale from bottom
					 * of screen to top. 1.5 - 2.0 are OK.
					 */
	GLfloat     screen_bottom;	/* Bottom of screen. */
	GLfloat     screen_top;	/* Top of screen. */
	GLfloat     bg_colour[4];	/* Background colour. */
	GLfloat     bubble_colour[4];	/* Colour of the bubbles. */
};

extern struct glb_config glb_config;

#define glb_drand() ((double)LRAND() / (double)MAXRAND)

/*-- From glb_sphere.c. --*/
typedef GLfloat glb_vertex[3];
typedef GLuint glb_triangle[3];
extern void glb_sphere_init(void);
extern glb_vertex *glb_sphere_get_vertices(int *nr_vertices);
extern glb_triangle *glb_sphere_get_triangles(int *nr_triangles);
extern void glb_sphere_end(void);

/*-- From glb_bubble.c. --*/
extern void *glb_bubble_new(GLfloat x, GLfloat y, GLfloat z, GLfloat scale,
			    GLfloat y_incr, GLfloat scale_incr);
extern void glb_bubble_delete(void *);
extern void glb_bubble_step(void *);
extern void glb_bubble_draw(void *);
extern GLfloat glb_bubble_get_y(void *);

/*-- From glb_draw.c. --*/
extern void *glb_draw_init(void);
extern void glb_draw_step(void *);
extern void glb_draw_end(void *);

#endif /* __bubbles3d_h__ */
