/* Author: Mauro Persano (mauro_persano@yahoo.com)  27 Nov 2005
 *
 * Todo:
 *   - support for 4-bit / packed 24-bit visuals.
 *
 * Revision history:
 * 06-Dec-2005: fixed stupid off-by-one buf; (preliminar) monochrome support.
 * 05-Dec-2005: more speed ups.
 * 27-Nov-2005: speed ups.
 * 21-Nov-2005: initial version
 */
#ifdef STANDALONE
#define MODE_fzort
#define PROGCLASS "Fzort"
#define HACK_INIT init_fzort
#define HACK_DRAW draw_fzort
#define fzort_opts xlockmode_opts
#define DEFAULTS "*delay: 0 \n"
#define UNIFORM_COLORS
#include "xlockmore.h" /* xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h" /* xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_fzort

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

ModeSpecOpt fzort_opts =
{0, (XrmOptionDescRec *)NULL, 0, (argtype *)NULL, (OptionStruct *)NULL };

#ifdef USE_MODULES
ModStruct fzort_description =
{ "fzort", "init_fzort", "draw_fzort", "release_fzort",
  "refresh_fzort", "init_fzort", (char *)NULL, &fzort_opts,
  10000, 1, 1, 1, 64, 1.0, "",
  "Shows a metallic-looking fzort.", 0, NULL };
#endif

#define MESH_DENSITY 22
/* #define MESH_DENSITY 44 */

#define MAX_WIDTH 800
#define MAX_HEIGHT 800
#define TEXTURE_SIZE 256 /* use a power of 2 preferably */

extern int XShmGetEventBase(Display *);

#define MAX_POLY_VTX 20

struct vector {
	float x, y, z;
};

struct matrix {
	float m11, m12, m13, m14;
	float m21, m22, m23, m24;
	float m31, m32, m33, m34;
};

struct vertex {
	struct vector p;
	struct vector normal;
};

struct pvertex {
	struct vector p;
	int x_scr, y_scr;
	long u_txt, v_txt;
};

struct polygon {
	int nvtx;
	int vtx_index[MAX_POLY_VTX];
	struct vector normal;
};

struct mesh {
	int npoly, nvtx;
	struct vertex *vtx;
	struct polygon *poly;
};

struct box {
	int x_min, y_min, x_max, y_max;
};

struct clip_limits {
	int x_min;
	int x_max;
	int y_min;
	int y_max;
};

struct raster {
	int width;
	int height;
	int pitch;
	int bpp;
	void *bits;
};

struct render_order {
	float z;
	unsigned zi;
	struct polygon *poly;
};

typedef struct fzort_ctx {
	Bool initialized;

	int palette[256];
	int using_shm;

	XShmSegmentInfo shm_info;
	XImage *image;

	struct clip_limits clip;
	struct raster raster;
	struct mesh *mesh;
	void *texture;

	/* render ctx */
	float lx, ly, cx, cy;
	void(*fill_triangle_fn)(struct fzort_ctx *, struct pvertex *,
	  struct pvertex *, struct pvertex *);
	struct pvertex *pvtx;
	struct render_order *order_in, *order_out;

	long start_ticks;

	struct box cur_box, prev_box;
	int first_frame;
} fzort_ctx;

static fzort_ctx *fzorts = (fzort_ctx *) NULL;
static int shm_completion_event;
static unsigned char texture8[TEXTURE_SIZE*TEXTURE_SIZE];
static int texture_initialized = False;

/* ordered dithering matrix */

static unsigned int dither6[8][8] = {
{     1, 59, 15, 55,  2, 56, 12, 52, },
{    33, 17, 47, 31, 34, 18, 44, 28, },
{     9, 49,  5, 63, 10, 50,  6, 60, },
{    41, 25, 37, 21, 42, 26, 38, 22, },
{     3, 57, 13, 53,  0, 58, 14, 54, },
{    35, 19, 45, 29, 32, 16, 46, 30, },
{    11, 51,  7, 61,  8, 48,  4, 62, },
{    43, 27, 39, 23, 40, 24, 36, 20 } };

/*
 *	f i x p o i n t
 */

#if defined(__GNUC__) && defined(__i386__)
#define FP_SHIFT				16
#else
#define FP_SHIFT				10
#endif

#define FP_SHIFT_COMPL		(32 - FP_SHIFT)
#define FP_MULTIPLIER		(1L << FP_SHIFT)
#define FP_ONE				(1L << FP_SHIFT)
#define FP_HALF				(1L << (FP_SHIFT - 1))
#define INT_TO_FP(a)		((a) << FP_SHIFT)
#define FP_TO_INT(f)		((f) >> FP_SHIFT)
#define FLOAT_TO_FP(f)		((long)((f) * (float)FP_MULTIPLIER))
#define FP_TO_FLOAT(f)		((float)(f) / (float)FP_MULTIPLIER)
#define FRAC_PART(f)		((f) & (FP_MULTIPLIER - 1))
#define FP_CEIL(f)			(((f) + FP_MULTIPLIER - 1) & \
				  			  ~(FP_MULTIPLIER - 1))
#define FP_FLOOR(f)			((f) & ~(FP_MULTIPLIER - 1))
#define FP_ICEIL(f)			(((f) + FP_ONE - 1) >> FP_SHIFT)
#define FP_IFLOOR(f)		((f) >> FP_SHIFT)

/* a / b */
#if defined(__GNUC__) && defined(__i386__)
static inline long
fp_div(long a, long b)
{
	long c;
	asm volatile("movl %%eax,%%edx; shll %3,%%eax; sarl %4,%%edx; idivl %2"
		: "=a"(c)
		: "0"(a), "m"(b), "i"(FP_SHIFT), "i"(32 - FP_SHIFT)
		: "edx");
	return c;
}
#else
static inline long
fp_div(long a, long b)
{
	return (a << FP_SHIFT)/b;
}
#endif

/* a * b */
#if defined(__GNUC__) && defined(__i386__)
static inline long
fp_mul(long a, long b)
{
	long c;
    asm volatile("imull %2; shrdl %3,%%edx,%%eax"
		: "=a"(c)
		: "0"(a), "m"(b), "i"(FP_SHIFT)
		: "edx"); 
	return c;
}
#else
static inline long
fp_mul(long a, long b)
{
	return (a*b) >> FP_SHIFT;
}
#endif

/* s + a * b */
#if defined(__GNUC__) && defined(__i386__)
static inline long
fp_mul_add(long s, long a, long b)
{
	long c;
    asm volatile("imull %2; shrdl %3,%%edx,%%eax; addl %4,%%eax"
		: "=a"(c)
		: "0"(a), "m"(b), "i"(FP_SHIFT), "m"(s)
		: "edx"); 
	return c;
}
#else
static inline long
fp_mul_add(long s, long a, long b)
{
	return s + ((a*b) >> FP_SHIFT);
}
#endif


/*
 *  v e c t o r
 */

static inline void
vec_copy(struct vector *r, struct vector *a)
{
	memcpy(r, a, sizeof *r);
}

static inline void
vec_set(struct vector *r, float x, float y, float z)
{
	r->x = x;
	r->y = y;
	r->z = z;
}

static inline void
vec_neg_copy(struct vector *r, struct vector *a)
{
	r->x = -a->x;
	r->y = -a->y;
	r->z = -a->z;
}

static inline void
vec_neg(struct vector *r)
{
	vec_neg_copy(r, r);
}

static inline void
vec_add(struct vector *r, struct vector *a, struct vector *b)
{
	r->x = a->x + b->x;
	r->y = a->y + b->y;
	r->z = a->z + b->z;
}

static inline void
vec_add_to(struct vector *a, struct vector *b)
{
	a->x += b->x;
	a->y += b->y;
	a->z += b->z;
}

static inline void
vec_sub(struct vector *r, struct vector *a, struct vector *b)
{
	r->x = a->x - b->x;
	r->y = a->y - b->y;
	r->z = a->z - b->z;
}

static inline void
vec_sub_from(struct vector *a, struct vector *b)
{
	a->x -= b->x;
	a->y -= b->y;
	a->z -= b->z;
}

static inline float
vec_dot_product(struct vector *a, struct vector *b)
{
	return a->x*b->x + a->y*b->y + a->z*b->z;
}

static inline float
vec_length_squared(struct vector *a)
{
	return vec_dot_product(a, a);
}

static inline void
vec_cross_product(struct vector *r, struct vector *a, struct vector *b)
{
	r->x = a->y*b->z - a->z*b->y;
	r->y = a->z*b->x - a->x*b->z;
	r->z = a->x*b->y - a->y*b->x;
}

static inline void
vec_scalar_mul(struct vector *a, float s)
{
	a->x *= s;
	a->y *= s;
	a->z *= s;
}

static inline void
vec_scalar_mul_copy(struct vector *r, struct vector *a, float s)
{
	r->x = a->x*s;
	r->y = a->y*s;
	r->z = a->z*s;
}

static float
vec_length(struct vector *a)
{
	return sqrt(vec_length_squared(a));
}

static void
vec_normalize(struct vector *a)
{
	float l;

	l = vec_length(a);

	if (l == 0.f)
		a->x = 1.f, a->y = 0.f, a->z = 0.f;
	else
		vec_scalar_mul(a, 1.f/l);
}


/*
 *  m a t r i x
 */

static inline void
mat_copy(struct matrix *r, struct matrix *m)
{
	memcpy(r, m, sizeof *r);
}

static const struct matrix identity = {
	1.f, 0.f, 0.f, 0.f,
	0.f, 1.f, 0.f, 0.f,
	0.f, 0.f, 1.f, 0.f
};

static void
mat_make_identity(struct matrix *m)
{
	memcpy(m, &identity, sizeof *m);
}

static void
mat_make_rotation_around_x(struct matrix *m, float ang)
{
	float c, s;

	c = cos(ang);
	s = sin(ang);

	mat_make_identity(m);

	m->m22 = c; m->m23 = -s;
	m->m32 = s; m->m33 = c; 
}

static void
mat_make_rotation_around_y(struct matrix *m, float ang)
{
	float c, s;

	c = cos(ang);
	s = sin(ang);

	mat_make_identity(m);

	m->m11 = c; m->m13 = -s;
	m->m31 = s; m->m33 = c;
}

static void
mat_make_rotation_around_z(struct matrix *m, float ang)
{
	float c, s;

	c = cos(ang);
	s = sin(ang);

	mat_make_identity(m);

	m->m11 = c; m->m12 = -s;
	m->m21 = s; m->m22 = c;
}

static void
mat_make_translation(struct matrix *m, float x, float y, float z)
{
	mat_make_identity(m);

	m->m14 = x;
	m->m24 = y;
	m->m34 = z;
}

static void
mat_mul_copy(struct matrix *r, struct matrix *a, struct matrix *b)
{
	float l11, l12, l13, l14;
	float l21, l22, l23, l24;
	float l31, l32, l33, l34;

	l11 = a->m11*b->m11 + a->m12*b->m21 + a->m13*b->m31;
	l12 = a->m11*b->m12 + a->m12*b->m22 + a->m13*b->m32;
	l13 = a->m11*b->m13 + a->m12*b->m23 + a->m13*b->m33;
	l14 = a->m11*b->m14 + a->m12*b->m24 + a->m13*b->m34 + a->m14;

	l21 = a->m21*b->m11 + a->m22*b->m21 + a->m23*b->m31;
	l22 = a->m21*b->m12 + a->m22*b->m22 + a->m23*b->m32;
	l23 = a->m21*b->m13 + a->m22*b->m23 + a->m23*b->m33;
	l24 = a->m21*b->m14 + a->m22*b->m24 + a->m23*b->m34 + a->m24;

	l31 = a->m31*b->m11 + a->m32*b->m21 + a->m33*b->m31;
	l32 = a->m31*b->m12 + a->m32*b->m22 + a->m33*b->m32;
	l33 = a->m31*b->m13 + a->m32*b->m23 + a->m33*b->m33;
	l34 = a->m31*b->m14 + a->m32*b->m24 + a->m33*b->m34 + a->m34;

	r->m11 = l11; r->m12 = l12; r->m13 = l13; r->m14 = l14;
	r->m21 = l21; r->m22 = l22; r->m23 = l23; r->m24 = l24;
	r->m31 = l31; r->m32 = l32; r->m33 = l33; r->m34 = l34;
}

static void
mat_mul(struct matrix *a, struct matrix *b)
{
	mat_mul_copy(a, a, b);
}

static void
mat_transform_copy(struct vector *r, struct matrix *m,
  struct vector *v)
{
	float x, y, z;

	x = m->m11*v->x + m->m12*v->y + m->m13*v->z + m->m14;
	y = m->m21*v->x + m->m22*v->y + m->m23*v->z + m->m24;
	z = m->m31*v->x + m->m32*v->y + m->m33*v->z + m->m34;

	r->x = x;
	r->y = y;
	r->z = z;
}

static inline void
mat_rotate_copy(struct vector *r, struct matrix *m, struct vector *v)
{
	float x, y, z;

	x = m->m11*v->x + m->m12*v->y + m->m13*v->z;
	y = m->m21*v->x + m->m22*v->y + m->m23*v->z;
	z = m->m31*v->x + m->m32*v->y + m->m33*v->z;

	r->x = x;
	r->y = y;
	r->z = z;
}

static inline void
mat_rotate(struct vector *v, struct matrix *m)
{
	mat_rotate_copy(v, m, v);
}


/*
 *	t r i a n g l e
 */

struct edge {
	struct pvertex *v0, *v1;
	int dx, dy;
	int du, dv;
	int sy;
	long sx;
	long dxdy;
	int lines;
	int adjy;
};

static inline void
init_edge(fzort_ctx *ctx, struct edge *edge,
  struct pvertex *v0, struct pvertex *v1)
{
	float y_max;

	edge->v0 = v0;
	edge->v1 = v1;
	edge->dx = v1->x_scr - v0->x_scr;
	edge->dy = v1->y_scr - v0->y_scr;
	edge->du = v1->u_txt - v0->u_txt;
	edge->dv = v1->v_txt - v0->v_txt;

	edge->sy = v0->y_scr;
	if (edge->sy < ctx->clip.y_min) {
		edge->adjy = ctx->clip.y_min - edge->sy;
		edge->sy = ctx->clip.y_min;
	} else {
		edge->adjy = 0;
	}

	y_max = v1->y_scr;
	if (y_max > ctx->clip.y_max)
		y_max = ctx->clip.y_max;

	if (y_max <= edge->sy) {
		edge->lines = 0;
	} else {
		edge->lines = y_max - edge->sy;
		edge->dxdy = fp_div(edge->dx, edge->dy);
		edge->sx = INT_TO_FP(v0->x_scr) + edge->adjy*edge->dxdy;
	} 
}

#define CONCAT(a, b) a ## b

#define NAME fill_triangle_1bpp_lsb_to_msb
#define PIXEL_TYPE unsigned char
#define LSB_TO_MSB
#define MONOCHROME
#include "fz_filler.h"

#define NAME fill_triangle_1bpp_msb_to_lsb
#define PIXEL_TYPE unsigned char
#define MONOCHROME
#include "fz_filler.h"

#define NAME fill_triangle_8bpp
#define PIXEL_TYPE unsigned char
#include "fz_filler.h"

#define NAME fill_triangle_16bpp
#define PIXEL_TYPE unsigned short
#include "fz_filler.h"

#define NAME fill_triangle_32bpp
#define PIXEL_TYPE unsigned int
#include "fz_filler.h"

/*
 *	r e n d e r
 */

static void
radix_sort(struct render_order *out, struct render_order *in, int rshift,
  int n)
{
	static int count[256], index[256];
	int i, b;

	memset(count, 0, sizeof count);

	for (i = 0; i < n; i++) {
		b = (in[i].zi >> rshift) & 0xff;
		count[b]++;
	}

	index[0] = 0;

	for (i = 1; i < 256; i++)
		index[i] = index[i - 1] + count[i - 1];

	for (i = 0; i < n; i++) {
		b = (in[i].zi >> rshift) & 0xff;
		out[index[b]] = in[i];
		index[b]++;
	}
}

static void
render_process_mesh(fzort_ctx *ctx, struct matrix *m)
{
	struct polygon *pin, *pend;
	struct vertex *v, *vend;
	struct pvertex *pv;
	int i, j, npoly, x_scr_min, x_scr_max, y_scr_min, y_scr_max;

	/* vertices */

	ctx->cur_box.x_min = ctx->cur_box.y_min =
	ctx->cur_box.x_max = ctx->cur_box.y_max = 0;

	x_scr_min = ctx->clip.x_max + 1;
	x_scr_max = ctx->clip.x_min - 1;
	y_scr_min = ctx->clip.y_max + 1;
	y_scr_max = ctx->clip.y_min - 1;

	vend = ctx->mesh->vtx + ctx->mesh->nvtx;
	pv = ctx->pvtx;

	for (v = ctx->mesh->vtx; v != vend; v++) {
		struct vector normal;
		float w;
		
		mat_transform_copy(&pv->p, m, &v->p);
		mat_rotate_copy(&normal, m, &v->normal);
		vec_normalize(&normal);

		pv->u_txt = FLOAT_TO_FP((TEXTURE_SIZE/2) +
		  (TEXTURE_SIZE/2)*normal.y + .5);
		pv->v_txt = FLOAT_TO_FP((TEXTURE_SIZE/2) +
		  (TEXTURE_SIZE/2)*normal.x + .5);
	
		w = ctx->lx/pv->p.z;
		pv->x_scr = ctx->cx + w*pv->p.x + .5;
		pv->y_scr = ctx->cy + w*pv->p.y + .5;

		if (pv->x_scr < x_scr_min)
			x_scr_min = pv->x_scr; 

		if (pv->x_scr > x_scr_max)
			x_scr_max = pv->x_scr; 

		if (pv->y_scr < y_scr_min)
			y_scr_min = pv->y_scr; 

		if (pv->y_scr > y_scr_max)
			y_scr_max = pv->y_scr; 

		pv++;
	}

	if (x_scr_min > ctx->clip.x_max)
		return;

	ctx->cur_box.x_min =
	  x_scr_min < ctx->clip.x_min ? ctx->clip.x_min : x_scr_min;

	if (x_scr_max < ctx->clip.x_min)
		return;

	ctx->cur_box.x_max =
	  x_scr_max > ctx->clip.x_max ? ctx->clip.x_max : x_scr_max;

	if (y_scr_min > ctx->clip.y_max)
		return;

	ctx->cur_box.y_min =
	  y_scr_min < ctx->clip.y_min ? ctx->clip.y_min : y_scr_min;

	if (y_scr_max < ctx->clip.y_min)
		return;

	ctx->cur_box.y_max =
	  y_scr_max > ctx->clip.y_max ? ctx->clip.y_max : y_scr_max;

	/* polygons */

	npoly = 0;
	pend = ctx->mesh->poly + ctx->mesh->npoly;

	for (pin = ctx->mesh->poly; pin != pend; pin++) {
		int nclip_x_min, nclip_x_max, nclip_y_min, nclip_y_max;
		int *idx, *iend;

		nclip_x_min = nclip_x_max = nclip_y_min = nclip_y_max = 0;

		iend = &pin->vtx_index[pin->nvtx];

		for (idx = &pin->vtx_index[0]; idx != iend; idx++) {
			pv = &ctx->pvtx[*idx];
	
			nclip_x_min += (pv->x_scr < ctx->clip.x_min);
			nclip_x_max += (pv->x_scr > ctx->clip.x_max);
			nclip_y_min += (pv->x_scr < ctx->clip.y_min);
			nclip_y_max += (pv->y_scr > ctx->clip.y_max);
		}
	
		if (nclip_x_min != pin->nvtx && nclip_x_max != pin->nvtx &&
		  nclip_y_min != pin->nvtx && nclip_y_max != pin->nvtx) {
			struct pvertex *p0, *p1, *p2, *p3;
			int ok;

			p0 = &ctx->pvtx[pin->vtx_index[0]];
			p1 = &ctx->pvtx[pin->vtx_index[1]];
			p2 = &ctx->pvtx[pin->vtx_index[2]];

			ok = (p2->x_scr - p0->x_scr)*(p1->y_scr - p0->y_scr) -
			  (p1->x_scr - p0->x_scr)*(p2->y_scr - p0->y_scr) >= 0;

			if (pin->nvtx == 4) {
				p3 = &ctx->pvtx[pin->vtx_index[3]];

				ok |= (p3->x_scr - p0->x_scr)*(p2->y_scr - p0->y_scr) -
				  (p2->x_scr - p0->x_scr)*(p3->y_scr - p0->y_scr) >= 0;
			}

			if (ok) {
				ctx->order_in[npoly].poly = pin;
				ctx->order_in[npoly].z = p0->p.z;
				npoly++;
			}
		}
	}

	/* draw */

	if (npoly) {
		float z_min, z_max, z_scale, z;
		struct polygon *poly;

		z_min = z_max = 0.f;
	
		for (i = 0; i < npoly; i++) {
			z = ctx->order_in[i].z;
	
			if (i == 0 || z < z_min)
				z_min = z;
	
			if (i == 0 || z > z_max)
				z_max = z;
		}
	
		z_scale = 255.f/(z_max - z_min);
	
		for (i = 0; i < npoly; i++)
			ctx->order_in[i].zi = z_scale*(ctx->order_in[i].z - z_min);
	
		radix_sort(ctx->order_out, ctx->order_in, 0, npoly);
	
		for (i = npoly - 1; i >= 0; i--) {
			poly = ctx->order_out[i].poly;
	
			for (j = 1; j < poly->nvtx - 1; j++) {
				ctx->fill_triangle_fn(ctx,
				  &ctx->pvtx[poly->vtx_index[0]],
				  &ctx->pvtx[poly->vtx_index[j]],
				  &ctx->pvtx[poly->vtx_index[j + 1]]);
			}
		}
	}
}

/*
 *	m e s h
 */

static void
mesh_free(struct mesh *m)
{
	if (m->vtx != NULL)
		free(m->vtx);

	if (m->poly != NULL)
		free(m->poly);

	free(m);
}

static inline float
radius_offset(float phi, float theta, float phase, float amp)
{
	return (amp + 4.*amp*sin(3.*phase + theta))*sin(phase + 5.*phi) + 
	  (amp + 4.*amp*sin(2.*phase + 2.*phi))*sin(phase + 3.*theta); 
}

static inline void
calc_coord(struct vector *v, float radius, float phi, float theta,
  float phase, float amp)
{
	float mr, r;

	/* coordinates */
	r = radius + radius_offset(phi, theta, phase, amp);
	v->z = -r*cos(phi);
	mr = r*sin(phi);
	v->y = -mr*sin(theta);
	v->x = -mr*cos(theta);
}

static void
calc_mesh_vertices(struct mesh *mesh, int density, float radius,
  float phase, float amp)
{
	struct vertex *vtx;
	double phi, theta, dphi, dtheta;
	int u, v, a, b, c;
	struct vector v0, v1;
	float mr, r;
	float cos_theta, sin_theta, cos_phi, sin_phi;
	float cos_3theta, sin_3theta;
	float sin_phase, cos_phase, sin_2phase, cos_2phase;
	float sin_3phase, cos_3phase;
	float cos_dphi, sin_dphi, cos_dtheta, sin_dtheta, st, ct;
	float offs;
	float k0, k1;

	vtx = mesh->vtx;

	/* vertices */

	calc_coord(&vtx->p, radius, 0., M_PI, phase, amp);

	vtx++;

	sin_phase = sin(phase);
	cos_phase = cos(phase);

	sin_2phase = 2.*sin_phase*cos_phase;
	cos_2phase = 1. - 2.*sin_phase*sin_phase;

	sin_3phase = 3.*sin_phase - 4.*sin_phase*sin_phase*sin_phase;
	cos_3phase = 4.*cos_phase*cos_phase*cos_phase - 3.*cos_phase;

	dtheta = 2.*M_PI/density;
	dphi = M_PI/(density + 1);

	sin_dphi = sin(dphi);
	cos_dphi = cos(dphi);

	sin_dtheta = sin(dtheta);
	cos_dtheta = cos(dtheta);

	cos_phi = cos_dphi;
	sin_phi = sin_dphi;

	for (phi = dphi, u = 0; u < density; u++, phi += dphi) {
		/* XXX: work these out */
		k0 = sin(phase + 5.*phi);
		k1 = amp + 4.*amp*sin(2.*phase + 2.*phi);

		cos_theta = 1.;
		sin_theta = 0.;

		for (theta = 0., v = 0; v < density; v++, theta += dtheta) {
			sin_3theta = 3*sin_theta -
			  4.*sin_theta*sin_theta*sin_theta;
			cos_3theta = 4*cos_theta*cos_theta*cos_theta -
			  3.*cos_theta;

			/*
				(amp + 4.*amp*sin(3.*phase + theta))*
				  sin(phase + 5.*phi) + 
				(amp + 4.*amp*sin(2.*phase + 2.*phi))*
				  sin(phase + 3.*theta); 
	  		*/

			offs = (amp + 4.*amp*
			  (sin_3phase*cos_theta + sin_theta*cos_3phase))*k0 +
			  k1*(sin_phase*cos_3theta + cos_phase*sin_3theta);

			r = radius + offs;

			vtx->p.z = -r*cos_phi;
			mr = r*sin_phi;
			vtx->p.y = -mr*sin_theta;
			vtx->p.x = -mr*cos_theta;
			vtx++;

			st = sin_theta*cos_dtheta + cos_theta*sin_dtheta;
			ct = cos_theta*cos_dtheta - sin_theta*sin_dtheta;

			sin_theta = st;
			cos_theta = ct;
		}

		st = sin_phi*cos_dphi + cos_phi*sin_dphi;
		ct = cos_phi*cos_dphi - sin_phi*sin_dphi;

		sin_phi = st;
		cos_phi = ct;
	}

	calc_coord(&vtx->p, radius, M_PI, M_PI, phase, amp);

	/* normals */

	vec_set(&mesh->vtx[0].normal, 0., 0., -1.);
	vec_set(&mesh->vtx[1 + density*density].normal, 0., 0., 1.);

	for (u = 0; u < density; u++) {
		for (v = 0; v < density; v++) {
			a = u*density + v + 1;
			b = u*density + (v + 1)%density + 1;
			c = (u + 1)*density + 1;
			if (u < density - 1)
				c += v;

			vec_sub(&v0, &mesh->vtx[b].p, &mesh->vtx[a].p);
			vec_sub(&v1, &mesh->vtx[c].p, &mesh->vtx[a].p);
			vec_cross_product(&mesh->vtx[a].normal, &v0, &v1);
		}
	}
}

static void
set_triangle(struct polygon *poly, int a, int b, int c)
{
	poly->nvtx = 3;
	poly->vtx_index[0] = a;
	poly->vtx_index[1] = b;
	poly->vtx_index[2] = c;
}

static void
set_quad(struct polygon *poly, int a, int b, int c, int d)
{
	poly->nvtx = 4;
	poly->vtx_index[0] = a;
	poly->vtx_index[1] = b;
	poly->vtx_index[2] = c;
	poly->vtx_index[3] = d;
}

static struct mesh *
make_sphere(int density)
{
	int u, v, a, b, c, d;
	struct polygon *poly;
	struct mesh *mesh;

	if ((mesh = (struct mesh *) malloc(sizeof *mesh)) == NULL) {
		return NULL;
	}

	mesh->npoly = mesh->nvtx = 0;

	/* vertices */

	mesh->nvtx = 2 + density*density;
	mesh->vtx = malloc(mesh->nvtx * sizeof *mesh->vtx);

	/* polygons */

	mesh->npoly = (density + 1)*density;
	poly = mesh->poly = malloc(mesh->npoly * sizeof *mesh->poly);

	for (u = 0; u < density; u++) {
		for (v = 0; v < density; v++) {
			a = u*density + v + 1;
			b = u*density + (v + 1)%density + 1;

			if (u == 0) {
				set_triangle(poly++, a, 0, b);
			} else {
				c = (u - 1)*density + v + 1;
				d = (u - 1)*density + (v + 1)%density + 1;

				set_quad(poly++, a, c, d, b);
			}
		}
	}

	for (v = 0; v < density; v++) {
		a = density*density + 1;
		d = (density - 1)*density + ((v + 1) % density) + 1;
		c = (density - 1)*density + v + 1;

		set_triangle(poly++, a, c, d);
	}

	return mesh;
}

/*
 *	t e x t u r e   g e n e r a t i o n
 */

static int
getpix(unsigned char *dest, int x, int y)
{
	return dest[(y&(TEXTURE_SIZE-1))*TEXTURE_SIZE + (x&(TEXTURE_SIZE - 1))];
}

static void
putpix(unsigned char *dest, int x, int y, int v)
{
	dest[(y&(TEXTURE_SIZE-1))*TEXTURE_SIZE + (x&(TEXTURE_SIZE-1))] = v;
}

static int
randnum(int range)
{
    return LRAND() % (2*(range + 1)) - range;
}

static int
plasmaavg(unsigned char *dest, int x1, int y1, int x2, int y2)
{
    int average, distance;

    average = (getpix(dest, x1, y1) + getpix(dest, x2, y2)) / 2;
    distance = abs(x1 - x2) + abs(y1 - y2);

    if (distance > 2)
        distance -= 2;

    return average + randnum(distance);
}

static void
rplasma(unsigned char *dest, int left, int top, int right, int bottom)
{
    int x, y;

    if (abs(top - bottom) < 2 && abs(right - left) < 2)
        return;

    x = left;
    y = (top + bottom) / 2;
    if (!getpix(dest, x, y))
		putpix(dest, x, y, plasmaavg(dest, left, top, left, bottom));

    x = right;
    if (!getpix(dest, x, y))
        putpix(dest, x, y, plasmaavg(dest, right, top, right, bottom));

    x = (left + right) / 2;
    y = top;
    if (!getpix(dest, x, y))
		putpix(dest, x, y, plasmaavg(dest, left, top, right, top));

    y = bottom;
    if (!getpix(dest, x, y))
	putpix(dest, x, y, plasmaavg(dest, left, bottom, right, bottom));

    y = (top + bottom) / 2;
    if (!getpix(dest, x, y))
        putpix(dest, x, y, (plasmaavg(dest, x, top, x, bottom) +
	  plasmaavg(dest, left, y, right, y)) / 2 + randnum(right - left));

    rplasma(dest, left, top, x, y);
    rplasma(dest, x, top, right, y);
    rplasma(dest, x, y, right, bottom);
    rplasma(dest, left, y, x, bottom);
}

static void
flare(unsigned char *dest)
{
	int i, j, r;
	float a;
	unsigned s;

	struct vector n, l;

	vec_set(&l, -.4, .4, 1.);
	vec_normalize(&l);

	for (i = 0; i < TEXTURE_SIZE; i++) {
		for (j = 0; j < TEXTURE_SIZE; j++) {
			n.x = j - TEXTURE_SIZE/2;
			n.y = i - TEXTURE_SIZE/2;
			n.z = -TEXTURE_SIZE/2;

			vec_normalize(&n);

			a = vec_dot_product(&n, &l);
			a *= a; a *= a;
			a *= a; a *= a;
			a *= a; a *= a;

			r = a*255;
			s = (dest[i*TEXTURE_SIZE + j]>>1) + r;
			s = s > 255 ? 255 : s;
			dest[i*TEXTURE_SIZE + j] = s;
		}
	}
}

static void *
init_texture(int *palette, int depth)
{
	int i, j;
	unsigned int v, pixel_size;
	void *texture;

	switch (depth) {
		case 1: case 8: pixel_size = sizeof(unsigned char); break;
		case 16: pixel_size = sizeof(unsigned short); break;
		case 24: case 32: pixel_size = sizeof(unsigned int); break;
		default: return NULL; /* ugh */
	}

	if ((texture = malloc(TEXTURE_SIZE*TEXTURE_SIZE*pixel_size)) == NULL) {
		return NULL;
	}

	if (!texture_initialized) {
		memset(texture8, 0, sizeof texture8);
		rplasma(texture8, 0, 0, TEXTURE_SIZE, TEXTURE_SIZE);
		flare(texture8);
		texture_initialized = True;
	}

	switch (depth) {
		case 1:
			memcpy(texture, texture8, TEXTURE_SIZE*TEXTURE_SIZE);
			break;

		case 8:
			for (i = 0; i < TEXTURE_SIZE; i++) {
				for (j = 0; j < TEXTURE_SIZE; j++) {
					v = texture8[i*TEXTURE_SIZE + j];
					((unsigned char *)texture)[i*TEXTURE_SIZE + j] = palette[v];
				}
			}
			break;

		case 16:
			for (i = 0; i < TEXTURE_SIZE; i++) {
				for (j = 0; j < TEXTURE_SIZE; j++) {
					v = texture8[i*TEXTURE_SIZE + j];
					((unsigned short *)texture)[i*TEXTURE_SIZE + j]
					  = palette[v];
				}
			}
			break;

		case 24:
		case 32:
			for (i = 0; i < TEXTURE_SIZE; i++) {
				for (j = 0; j < TEXTURE_SIZE; j++) {
					v = texture8[i*TEXTURE_SIZE + j];
					((unsigned int *)texture)[i*TEXTURE_SIZE + j] = palette[v];
				}
			}
			break;
	}

	return texture;
}


/*
 *	x i m a g e
 */

static int caught_x_error;

static int
x_error_handler(Display *display, XErrorEvent *error)
{
	caught_x_error = 1;

	return 0;
}

static XImage *
create_xshmimage(ModeInfo *mi, XShmSegmentInfo *si, int width, int height)
{
	XImage *img;
	XErrorHandler prev_handler;
	int depth, format;

	if (MI_DEPTH(mi) == 1 || MI_NPIXELS(mi) > 2) {
		depth = MI_DEPTH(mi);
		format = ZPixmap;
	} else {
		depth = 1;
		format = XYBitmap;
	}

	XSync(MI_DISPLAY(mi), False);
	prev_handler = XSetErrorHandler(x_error_handler);

	caught_x_error = 0;
	img = XShmCreateImage(MI_DISPLAY(mi), MI_VISUAL(mi), depth,
	  format, NULL, si, width, height);

	if (img == NULL || caught_x_error)
		goto failed_0;

	if ((si->shmid = shmget(IPC_PRIVATE, img->bytes_per_line*img->height,
	  IPC_CREAT|0777)) == -1)
		goto failed_1;

	si->readOnly = False;
	img->data = si->shmaddr = shmat(si->shmid, 0, 0);

	caught_x_error = 0;
	if (XShmAttach(MI_DISPLAY(mi), si) == False || caught_x_error)
		goto failed_2;

	XSetErrorHandler(prev_handler);

	XSync(MI_DISPLAY(mi), False);

	shmctl(si->shmid, IPC_RMID, 0);

	return img;

failed_2:
	shmdt(si->shmaddr);
failed_1:
	XDestroyImage(img);
	XSync(MI_DISPLAY(mi), False);
failed_0:
	XSetErrorHandler(prev_handler);
	return NULL;
}

static void
destroy_xshmimage(ModeInfo *mi, XImage *img, XShmSegmentInfo *si)
{
	XErrorHandler prev_handler;

	XSync(MI_DISPLAY(mi), False);

	prev_handler = XSetErrorHandler(x_error_handler);
	XShmDetach(MI_DISPLAY(mi), si);
	XSetErrorHandler(prev_handler);

	XDestroyImage(img);
	XSync(MI_DISPLAY(mi), False);

	shmdt(si->shmaddr);

	XSync(MI_DISPLAY(mi), False);
}

static XImage *
create_ximage(ModeInfo *mi, int width, int height)
{
	XImage *img;
	int depth, format;

	if (MI_DEPTH(mi) == 1 || MI_NPIXELS(mi) > 2) {
		depth = MI_DEPTH(mi);
		format = ZPixmap;
	} else {
		depth = 1;
		format = XYBitmap;
	}

	if ((img = XCreateImage(MI_DISPLAY(mi), MI_VISUAL(mi), depth,
	  format, 0, 0, width, height, 8, 0)) == NULL) {
		return NULL;
	}

	if ((img->data = calloc(img->height, img->bytes_per_line)) == NULL) {
	  	XDestroyImage(img);
		XSync(MI_DISPLAY(mi), False);
		return NULL;
	}

	return img;
}

static int
make_image(ModeInfo *mi, struct fzort_ctx *fz)
{
	int img_width, img_height;
	
	img_width = MIN(MI_WIDTH(mi), MI_HEIGHT(mi));

	if (img_width > MAX_WIDTH)
		img_width = MAX_WIDTH;

	img_height = img_width;

	if (XShmQueryExtension(MI_DISPLAY(mi))) {
		fz->image = create_xshmimage(mi, &fz->shm_info, img_width,
		  img_height);
		fz->using_shm = fz->image != NULL;
	}

	if (fz->image == NULL) {
		if ((fz->image = create_ximage(mi, img_width, img_height)) == NULL) {
			return -1;
		}
	}

	return 0;
}

static void
free_image(ModeInfo *mi, struct fzort_ctx *fz)
{
		if (fz->using_shm) {
			destroy_xshmimage(mi, fz->image, &fz->shm_info);
		} else {
			free(fz->image->data);
			fz->image->data = NULL;
			XDestroyImage(fz->image);
			XSync(MI_DISPLAY(mi), False);
		}
		fz->image = NULL;
}


/*
 *	f z o r t
 */

static void
release_fzort_ctx(ModeInfo *mi, fzort_ctx *fz)
{
	if (fz->initialized) {
		free(fz->pvtx);
		free(fz->order_in);
		free(fz->order_out);
		free(fz->texture);
		mesh_free(fz->mesh);
		free_image(mi, fz);
		fz->initialized = False;
	}
}

static void
init_color_map(ModeInfo *mi, fzort_ctx *fz, float r, float g, float b,
  float kd, float ks, float n)
{
	float red, green, blue, ang;
	unsigned ib, ig, ir;
	float ka, kb;
	int i;
	XColor xc;

	for (i = 0; i < 256; i++) {
		ang = (255 - i)*(M_PI/2.)/256.;

		ka = kd * cos(ang);
		kb = ks * pow(cos(ang), n);

		red = r*ka + kb;
		green = g*ka + kb;
		blue = b*ka + kb;

		ib = blue > 1. ? 255 : 255*blue;
		ig = green > 1. ? 255 : 255*green;
		ir = red > 1. ? 255 : 255*red;

		xc.red = ir<<8;
		xc.green = ig<<8;
		xc.blue = ib<<8;
		xc.flags = DoRed|DoGreen|DoBlue;

		XAllocColor(MI_DISPLAY(mi), MI_COLORMAP(mi), &xc);

		fz->palette[i] = xc.pixel;
	}
}

static void
init_fzort_ctx(ModeInfo *mi, fzort_ctx *fz)
{
	struct timeval tv;

	if (fz->initialized) {
		release_fzort_ctx(mi, fz);
	}

	/* 0. create image for double-buffering */

	if (make_image(mi, fz) < 0) {
		goto failed_0;
	}

	fz->clip.x_min = 0;
	fz->clip.y_min = 0;
	fz->clip.x_max = fz->image->width - 1;
	fz->clip.y_max = fz->image->height - 1;

	fz->raster.width = fz->image->width;
	fz->raster.height = fz->image->height;
	fz->raster.pitch = fz->image->bytes_per_line;
	fz->raster.bits = fz->image->data;
	fz->raster.bpp = fz->image->bits_per_pixel;

	fz->lx = fz->raster.width;
	fz->ly = fz->raster.height;
	fz->cx = fz->raster.width/2;
	fz->cy = fz->raster.height/2;

	switch (fz->raster.bpp) {
		case 1:
			/* check if the leftmost pixel is represented by the least
			 * significant bit or vice-versa  */
			fz->image->data[0] = 0;
			XPutPixel(fz->image, 0, 0, 1);
			if (fz->image->data[0] == 1)
				fz->fill_triangle_fn = fill_triangle_1bpp_lsb_to_msb;
			else
				fz->fill_triangle_fn = fill_triangle_1bpp_msb_to_lsb;
			break;
		case 8: fz->fill_triangle_fn = fill_triangle_8bpp; break;
		case 16: fz->fill_triangle_fn = fill_triangle_16bpp; break;
		case 24: case 32:
		  fz->fill_triangle_fn = fill_triangle_32bpp; break;
	}

	/* 1. create mesh */

	if ((fz->mesh = make_sphere(MESH_DENSITY)) == NULL) {
		goto failed_1;
	}

	/* 2. initialize palette */

	if (fz->image->depth >= 8) {
		init_color_map(mi, fz, .2, .2, 1., .9, .5, 4.);
	}

	/* 3. create texture */

	if ((fz->texture = init_texture(fz->palette, fz->image->bits_per_pixel))
	  == NULL) {
		goto failed_2;
	}

	/* 4. allocate memory for vertices and polygons */

	if ((fz->pvtx = malloc(fz->mesh->nvtx * sizeof *fz->pvtx)) == NULL) {
		goto failed_3;
	}

	if ((fz->order_in = malloc(fz->mesh->npoly * sizeof *fz->order_in))
	  == NULL) {
		goto failed_4;
	}

	if ((fz->order_out = malloc(fz->mesh->npoly * sizeof *fz->order_out))
	  == NULL) {
		goto failed_5;
	}

	gettimeofday(&tv, NULL);
	fz->start_ticks = tv.tv_sec*1000 + tv.tv_usec/1000;

	fz->prev_box.x_min = 0;
	fz->prev_box.x_max = fz->clip.x_max;
	fz->prev_box.y_min = 0;
	fz->prev_box.y_max = fz->clip.y_max;

	fz->initialized = True;

	MI_CLEARWINDOW(mi);

	return;

failed_5:
	free(fz->order_in);
failed_4:
	free(fz->pvtx);
failed_3:
	free(fz->texture);
failed_2:
	mesh_free(fz->mesh);
failed_1:
	free_image(mi, fz);
failed_0:
	return;
}

static void
render(fzort_ctx *fz, long ticks)
{
	struct matrix t, v;
	float a, b, c;
	struct matrix rmatrix, ry, rx;

	a = (float)ticks/2000.;

	calc_mesh_vertices(fz->mesh, MESH_DENSITY, 120.,
	  /* 4.*a, 12. + 5.*sin(2.*a)); */
	  4.*a, 7. + 7.*sin(2.*a));
	  /* 0., 0.); */

	b = 1.5*a;
	c = 2.2*a;

	mat_make_rotation_around_y(&ry, 1.5*a);
	mat_make_rotation_around_x(&rx, 2.2*a);
	mat_mul_copy(&rmatrix, &ry, &rx);
	mat_make_translation(&t, 220.*sin(b), 220.*cos(a),
	  900. + 80.*cos(b)*sin(a));
	mat_mul_copy(&v, &t, &rmatrix);
	render_process_mesh(fz, &v);
}

/*
 *   P u b l i c   i n t e r f a c e
 */

void
init_fzort(ModeInfo *mi)
{
	fzort_ctx *fz;

	if (fzorts == NULL) {
		if ((fzorts = (fzort_ctx *) calloc(MI_NUM_SCREENS(mi),
		  sizeof (fzort_ctx))) == NULL)
			return;
	}

	fz = &fzorts[MI_SCREEN(mi)];

	init_fzort_ctx(mi, fz);

	shm_completion_event = XShmGetEventBase(MI_DISPLAY(mi)) + ShmCompletion;
}

void
draw_fzort(ModeInfo *mi)
{
	fzort_ctx *fz;
	int x0, y0, src_x_min, src_y_min, src_x_max, src_y_max;
	int img_width, img_height;
	int lines, bytes_per_pixel, bytes_per_block;
	char *ptr;
	long cur_ticks;
	struct timeval tv;

	if (fzorts == NULL)
		return;

	fz = &fzorts[MI_SCREEN(mi)];
#if 0
	printf("Dies after here on Solaris/gcc\n");
	if (!fz->initialized)
		return;
	printf("Dies before here on Solaris/gcc\n");
#else
	if (!fz->initialized)
		return;
#endif

	/* clear what was used by previous iteration */

	if (fz->image->bits_per_pixel < 8) {
		/* monochrome pixmaps use so little memory that it's probably cheaper
		 * to do a single memset  */
		memset(fz->image->data, 0, fz->image->height*fz->image->bytes_per_line);
	} else {
		bytes_per_pixel = fz->image->bits_per_pixel/8;
		lines = fz->prev_box.y_max - fz->prev_box.y_min + 1;
		ptr = fz->image->data +
		  fz->prev_box.y_min*fz->image->bytes_per_line +
		  fz->prev_box.x_min*bytes_per_pixel;
		bytes_per_block= (fz->prev_box.x_max - fz->prev_box.x_min + 1)*
		  bytes_per_pixel;

		while (lines--) {
			memset(ptr, 0, bytes_per_block);
			ptr += fz->image->bytes_per_line;
		}
	}

	gettimeofday(&tv, NULL);
	cur_ticks = tv.tv_sec*1000 + tv.tv_usec/1000;

	/* render */

	render(fz, cur_ticks - fz->start_ticks);

	/* display image */

	x0 = (MI_WIDTH(mi) - fz->image->width)/2;
	y0 = (MI_HEIGHT(mi) - fz->image->height)/2;

	if (fz->image->width <= 200) {
		src_x_min = src_y_min = 0;
		src_x_max = fz->image->width - 1;
		src_y_max = fz->image->height - 1;
	} else {
		src_x_min = MIN(fz->prev_box.x_min, fz->cur_box.x_min);
		src_y_min = MIN(fz->prev_box.y_min, fz->cur_box.y_min);

		src_x_max = MAX(fz->prev_box.x_max, fz->cur_box.x_max);
		src_y_max = MAX(fz->prev_box.y_max, fz->cur_box.y_max);
	}

	XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_WHITE_PIXEL(mi));
	XSetBackground(MI_DISPLAY(mi), MI_GC(mi), MI_BLACK_PIXEL(mi));

	if (fz->using_shm) {
		XEvent event;

		XShmPutImage(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi),
		  fz->image,
		  src_x_min, src_y_min, x0 + src_x_min, y0 + src_y_min,
		  src_x_max - src_x_min + 1, src_y_max - src_y_min + 1,
		  1);

		while (XCheckTypedEvent(MI_DISPLAY(mi), shm_completion_event,
		  &event) == False)
		        ;
	} else {
		XPutImage(MI_DISPLAY(mi), MI_WINDOW(mi), MI_GC(mi),
		  fz->image, 
		  src_x_min, src_y_min, x0 + src_x_min, y0 + src_y_min,
		  src_x_max - src_x_min + 1, src_y_max - src_y_min + 1);
	}

	fz->prev_box = fz->cur_box;
}

void
release_fzort(ModeInfo *mi)
{
	int i;

	if (fzorts != NULL) {
		for (i = 0; i < MI_NUM_SCREENS(mi); i++) {
			release_fzort_ctx(mi, &fzorts[i]);
		}
	}

	free(fzorts);

	fzorts = (fzort_ctx *) NULL;
}

void
refresh_fzort(ModeInfo *mi)
{
	MI_CLEARWINDOW(mi);
}

#endif /* MODE_fzort */
