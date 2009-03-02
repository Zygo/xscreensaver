#if 0
static const char sccsid[] = "@(#)b_sphere.c  4.11 98/06/16 xlockmore";
#endif

/*-
 * BUBBLE3D (C) 1998 Richard W.M. Jones.
 * b_sphere.c: Create a list of vertices and triangles in a
 * normalized sphere, which is then later used as the basic shape
 * for all bubbles. This code is run once when the program starts
 * up.
 */

#include "bubble3d.h"

/* The list of vertices created. */
typedef glb_vertex vertex;
static vertex *vertices = 0;
static int  nr_vertices = 0, nr_vertices_allocated = 0;

/* The list of triangles created. */
typedef glb_triangle triangle;
static triangle *triangles = 0;
static int  nr_triangles = 0, nr_triangles_allocated = 0;

/* Have we initialized the lists yet? */
static int  initialized = 0;

#define EPSILON GLB_VERTICES_EPSILON

/* Should be taken care of already... but just in case */
#if !defined( __GNUC__ ) && !defined(__cplusplus) && !defined(c_plusplus)
#undef inline
#define inline			/* */
#endif
static inline int
close_enough(GLfloat * v1, GLfloat * v2)
{
	return fabs((double) (v1[0] - v2[0])) <= EPSILON &&
		fabs((double) (v1[1] - v2[1])) <= EPSILON &&
		fabs((double) (v1[2] - v2[2])) <= EPSILON;
}

#define INCR(n) ((n == 0) ? (n = 1) : (n *= 2))
#define INCR_ALLOCATION(a, n, t) (a = (t *) realloc (a, INCR (n) * sizeof (t)))

static inline GLuint
save_vertex(GLfloat * v)
{
	int         i;

	/* Inefficient, but we only do this a few times. Check to see if there's
	 * an existing vertex which is `close enough' to this one.
	 */
	for (i = 0; i < nr_vertices; ++i)
		if (close_enough(v, vertices[i]))
			return i;

	if (nr_vertices_allocated <= nr_vertices) {
		if (vertices == 0) {
			vertices = (vertex *) malloc(INCR(nr_vertices_allocated) * sizeof (vertex));
		} else {
			INCR_ALLOCATION(vertices, nr_vertices_allocated, vertex);
		}
	}
	vertices[nr_vertices][0] = v[0];
	vertices[nr_vertices][1] = v[1];
	vertices[nr_vertices][2] = v[2];
	return nr_vertices++;
}

static inline GLuint
save_triangle(GLuint v1, GLuint v2, GLuint v3)
{
	if (nr_triangles_allocated <= nr_triangles) {
		if (triangles == 0) {
			triangles = (triangle *) malloc(INCR(nr_triangles_allocated) * sizeof (triangle));
		} else {
			INCR_ALLOCATION(triangles, nr_triangles_allocated, triangle);
		}
	}
	triangles[nr_triangles][0] = v1;
	triangles[nr_triangles][1] = v2;
	triangles[nr_triangles][2] = v3;
	return nr_triangles++;
}

static inline void
normalize(GLfloat v[3])
{
	GLfloat     d = (GLfloat) sqrt((double) (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]));

	if (d != 0) {
		v[0] /= d;
		v[1] /= d;
		v[2] /= d;
	} else {
		v[0] = v[1] = v[2] = 0;
	}
}

static void
subdivide(GLfloat * v1, GLuint vi1,
	  GLfloat * v2, GLuint vi2,
	  GLfloat * v3, GLuint vi3,
	  int depth)
{
	int         i;

	if (depth == 0) {
		save_triangle(vi1, vi2, vi3);
	} else {
		GLuint      vi12, vi23, vi31;
		GLfloat     v12[3], v23[3], v31[3];

		for (i = 0; i < 3; ++i) {
			v12[i] = v1[i] + v2[i];
			v23[i] = v2[i] + v3[i];
			v31[i] = v3[i] + v1[i];
		}
		normalize(v12);
		vi12 = save_vertex(v12);
		normalize(v23);
		vi23 = save_vertex(v23);
		normalize(v31);
		vi31 = save_vertex(v31);
		subdivide(v1, vi1, v12, vi12, v31, vi31, depth - 1);
		subdivide(v2, vi2, v23, vi23, v12, vi12, depth - 1);
		subdivide(v3, vi3, v31, vi31, v23, vi23, depth - 1);
		subdivide(v12, vi12, v23, vi23, v31, vi31, depth - 1);
	}
}

#define ICO_X 0.525731112119133606
#define ICO_Z 0.850650808352039932

static GLfloat vdata[12][3] =
{
	{-ICO_X, 0, ICO_Z},
	{ICO_X, 0, ICO_Z},
	{-ICO_X, 0, -ICO_Z},
	{ICO_X, 0, -ICO_Z},
	{0, ICO_Z, ICO_X},
	{0, ICO_Z, -ICO_X},
	{0, -ICO_Z, ICO_X},
	{0, -ICO_Z, -ICO_X},
	{ICO_Z, ICO_X, 0},
	{-ICO_Z, ICO_X, 0},
	{ICO_Z, -ICO_X, 0},
	{-ICO_Z, -ICO_X, 0}
};

static GLuint tindices[20][3] =
{
	{0, 4, 1},
	{0, 9, 4},
	{9, 5, 4},
	{4, 5, 8},
	{4, 8, 1},
	{8, 10, 1},
	{8, 3, 10},
	{5, 3, 8},
	{5, 2, 3},
	{2, 7, 3},
	{7, 10, 3},
	{7, 6, 10},
	{7, 11, 6},
	{11, 0, 6},
	{0, 1, 6},
	{6, 1, 10},
	{9, 0, 11},
	{9, 11, 2},
	{9, 2, 5},
	{7, 2, 11}
};

/* Public interface: Create the sphere. */
void
glb_sphere_init(void)
{
	int         i;

	if (initialized)
		return;

	for (i = 0; i < 20; ++i) {
		subdivide(vdata[tindices[i][0]], save_vertex(vdata[tindices[i][0]]),
		   vdata[tindices[i][1]], save_vertex(vdata[tindices[i][1]]),
		   vdata[tindices[i][2]], save_vertex(vdata[tindices[i][2]]),
			  glb_config.subdivision_depth);
	}

	initialized = 1;
}

/* Return the vertices list. */
glb_vertex *
glb_sphere_get_vertices(int *nr_vertices_ptr)
{
	glb_sphere_init();
	*nr_vertices_ptr = nr_vertices;
	return vertices;
}

/* Return the triangles list. */
glb_triangle *
glb_sphere_get_triangles(int *nr_triangles_ptr)
{
	glb_sphere_init();
	*nr_triangles_ptr = nr_triangles;
	return triangles;
}

/* Free up memory. */
#if 0
void
glb_sphere_end(void)
{
	initialized = 0;

	(void) free((void *) vertices);
	(void) free((void *) triangles);

	vertices = 0;
	nr_vertices = nr_vertices_allocated = 0;

	triangles = 0;
	nr_triangles = nr_triangles_allocated = 0;
}

#endif
