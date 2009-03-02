/* jigglypuff - a most, most, unfortunate screensaver.
 *
 * Copyright (c) 2003 Keith Macleod (kmacleod@primus.ca)
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Draws all varieties of obscene, spastic, puffy balls
 * orbiting lazily about the screen. More of an accident
 * than anything else.
 *
 * Apologies to anyone who thought they were getting a Pokemon
 * out of this.
 *
 * Of course, if you modify it to do something interesting and/or
 * funny, I'd appreciate receiving a copy.
 *
 * 04/06/2003 - Oops, figured out what was wrong with the sphere
 *              mapping. I had assumed it was done in model space,
 *              but of course I was totally wrong... Eye space you
 *              say? Yup. km
 *
 * 03/31/2003 - Added chrome to the color options. The mapping
 *              is anything but 'correct', but it's a pretty good
 *              effect anyways, as long as the surface is jiggling
 *              enough that you can't tell. Sure, it seems  kind of odd
 *              that it's reflecting a sky that's obviously not there,
 *              but who has time to worry about silly details like
 *              that? Not me, ah rekkin'. km
 *
 */

#ifdef STANDALONE
# define DEFAULTS           "*delay: 20000\n" \
                            "*showFPS: False\n" \
                            "*wireframe: False\n" \

# define refresh_jigglypuff 0
# define release_jigglypuff 0
# include "xlockmore.h"
#else
# include "xlock.h"
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "xpm-ximage.h"
#include "gltrackball.h"
#include "../images/jigglymap.xpm"

#ifdef USE_GL


#define DEF_COLOR           "cycle"
#define DEF_SHININESS       "100"
#define DEF_COMPLEXITY      "2"
#define DEF_SPEED           "500"
#define DEF_DISTANCE        "100"
#define DEF_HOLD            "800"
#define DEF_SPHERISM        "75"
#define DEF_DAMPING         "500"

#ifndef max
#define max(a,b) (((a)>(b))?(a):(b))
#define min(a,b) (((a)<(b))?(a):(b))
#endif

/* Why isn't RAND_MAX correct in the first place? */
#define REAL_RAND_MAX (2.0*(float)RAND_MAX)

static int spherism;
static int hold;
static int distance;
static int damping;

static int complexity;
static int speed;

static int do_tetrahedron;
static int spooky;
static char *color;
static int shininess;

static int random_parms;

typedef struct solid solid;

typedef struct {
    float stable_distance;
    float hold_strength;
    float spherify_strength;
    float damping_velocity;
    float damping_factor;

    int do_wireframe;
    int spooky;
    int color_style;
    GLint shininess;
    GLfloat jiggly_color[4];
    GLfloat color_dir[3];

    solid *shape;

    trackball_state *trackball;
    int button_down;

    float angle;
    float axis;
    float speed;

    GLXContext *glx_context;
} jigglystruct;

static jigglystruct *jss = NULL;

static XrmOptionDescRec opts[] = {
    {"-random", ".Jigglypuff.random", XrmoptionNoArg, "true"},
    {"+random", ".Jigglypuff.random", XrmoptionNoArg, "false"},
    {"-tetra", ".Jigglypuff.tetra", XrmoptionNoArg, "true"},
    {"+tetra", ".Jigglypuff.tetra", XrmoptionNoArg, "false"},
    {"-spooky", ".Jigglypuff.spooky", XrmoptionSepArg, "0"},
    {"-color", ".Jigglypuff.color", XrmoptionSepArg, DEF_COLOR},
    {"-shininess", ".Jigglypuff.shininess", XrmoptionSepArg, DEF_SHININESS},
    {"-complexity", ".Jigglypuff.complexity", XrmoptionSepArg, DEF_COMPLEXITY},
    {"-speed", ".Jigglypuff.speed", XrmoptionSepArg, DEF_SPEED},
    {"-spherism", ".Jigglypuff.spherism", XrmoptionSepArg, DEF_SPHERISM},
    {"-hold", ".Jigglypuff.hold", XrmoptionSepArg, DEF_HOLD},
    {"-distance", "Jigglypuff.distance", XrmoptionSepArg, DEF_DISTANCE},
    {"-damping", "Jigglypuff.damping", XrmoptionSepArg, DEF_DAMPING}
};

static argtype vars[] = {
    {&random_parms, "random", "Random", "True", t_Bool},
    {&do_tetrahedron, "tetra", "Tetra", "False", t_Bool},
    {&spooky, "spooky", "Spooky", "0", t_Int},
    {&color, "color", "Color", DEF_COLOR, t_String},
    {&shininess, "shininess", "Shininess", DEF_SHININESS, t_Int},
    {&complexity, "complexity", "Complexity", DEF_COMPLEXITY, t_Int},
    {&speed, "speed", "Speed", DEF_SPEED, t_Int},
    {&spherism, "spherism", "Spherism", DEF_SPHERISM, t_Int},
    {&hold, "hold", "Hold", DEF_HOLD, t_Int},
    {&distance, "distance", "Distance", DEF_DISTANCE, t_Int},
    {&damping, "damping", "Damping", DEF_DAMPING, t_Int}
};

#undef countof
#define countof(x) ((int)(sizeof(x)/sizeof(*(x))))

ENTRYPOINT ModeSpecOpt jigglypuff_opts = {countof(opts), opts, countof(vars), vars, NULL};

#define COLOR_STYLE_NORMAL    0
#define COLOR_STYLE_CYCLE     1
#define COLOR_STYLE_CLOWNBARF 2
#define COLOR_STYLE_FLOWERBOX 3
#define COLOR_STYLE_CHROME    4

#define CLOWNBARF_NCOLORS 5

static const GLfloat clownbarf_colors[CLOWNBARF_NCOLORS][4] = {
    {0.7, 0.7, 0.0, 1.0},
    {0.8, 0.1, 0.1, 1.0},
    {0.1, 0.1, 0.8, 1.0},
    {0.9, 0.9, 0.9, 1.0},
    {0.0, 0.0, 0.0, 1.0}
};

static const GLfloat flowerbox_colors[4][4] = {
    {0.7, 0.7, 0.0, 1.0},
    {0.9, 0.0, 0.0, 1.0},
    {0.0, 0.9, 0.0, 1.0},
    {0.0, 0.0, 0.9, 1.0},
};

# if 0 /* I am not even going to *try* and make this bullshit compile
          without warning under gcc -std=c89 -pedantic.  -jwz. */
#ifdef DEBUG
# ifdef __GNUC__ /* GCC style */
#define _DEBUG(msg, args...) do { \
    fprintf(stderr, "%s : %d : " msg ,__FILE__,__LINE__ ,##args); \
} while(0)
# else /* C99 standard style */
#define _DEBUG(msg, ...) do { \
    fprintf(stderr, "%s : %d : " msg ,__FILE__,__LINE__,__VA_ARGS__); \
} while(0)
# endif
#else
# ifdef __GNUC__ /* GCC style */
#define _DEBUG(msg, args...)
# else /* C99 standard style */
#define _DEBUG(msg, ...)
# endif
#endif
#endif /* 0 */

/* This is all the half-edge b-rep code (as well as basic geometry) */
typedef struct face face;
typedef struct edge edge;
typedef struct hedge hedge;
typedef struct vertex vertex;
typedef GLfloat coord;
typedef coord vector[3];

struct solid {
    face *faces;
    edge *edges;
    vertex *vertices;
};

struct face {
    solid *s;
    hedge *start;
    const GLfloat *color;
    face *next;
};

struct edge {
    solid *s;
    hedge *left;
    hedge *right;
    edge *next;
};

struct hedge {
    face *f;
    edge *e;
    vertex *vtx;
    hedge *next;
    hedge *prev;
};

struct vertex {
    solid *s;
    hedge *h;
    vector v;
    vector n;
    vector f;
    vector vel;
    vertex *next;
};

static inline void vector_init(vector v, coord x, coord y, coord z)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
}    

static inline void vector_copy(vector d, vector s)
{
    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
}

static inline void vector_add(vector v1, vector v2, vector v)
{
    vector_init(v, v1[0]+v2[0], v1[1]+v2[1], v1[2]+v2[2]);
}

static inline void vector_add_to(vector v1, vector v2)
{
    v1[0] += v2[0];
    v1[1] += v2[1];
    v1[2] += v2[2];
}

static inline void vector_sub(vector v1, vector v2, vector v)
{
    vector_init(v, v1[0]-v2[0], v1[1]-v2[1], v1[2]-v2[2]);
}

static inline void vector_scale(vector v, coord s)
{
    v[0] *= s;
    v[1] *= s;
    v[2] *= s;
}

static inline coord dot(vector v1, vector v2)
{
    return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}

static inline void cross(vector v1, vector v2, vector v)
{
    vector_init(v,
		v1[1]*v2[2] - v2[1]*v1[2],
		v1[2]*v2[0] - v2[2]*v1[0],
		v1[0]*v2[1] - v2[0]*v1[1]);
}

static inline coord magnitude2(vector v)
{
    return v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
}

static inline coord magnitude(vector v)
{
    return sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static inline void normalize(vector v)
{
    coord mag = 1.0/sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);

    v[0] *= mag;
    v[1] *= mag;
    v[2] *= mag;
}

static inline void normalize_to(vector v, coord m)
{
    coord mag = 1.0/sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2])/m;

    v[0] *= mag;
    v[1] *= mag;
    v[2] *= mag;
}

static inline void midpoint(vector v1, vector v2, vector v)
{
    vector_init(v,
		v1[0] + 0.5 * (v2[0] - v1[0]),
		v1[1] + 0.5 * (v2[1] - v1[1]),
		v1[2] + 0.5 * (v2[2] - v1[2]));
}

static inline hedge *partner(hedge *h) {
    if(!h->e)
	return NULL;
    if(h == h->e->left) {
	return h->e->right;
    }
    else if(h == h->e->right) {
	return h->e->left;
    }
    else {
/*	_DEBUG("Inconsistent edge detected. Presumably, this is a bug. Exiting.\n", NULL); */
	exit(-1);
    }
}

static vertex *vertex_new(solid *s, vector v)
{
    vertex *vtx = (vertex*)malloc(sizeof(vertex));

    if(!vtx)
	return NULL;
    vtx->s = s;
    vtx->next = s->vertices;
    s->vertices = vtx;
    vector_copy(vtx->v, v);
    vector_init(vtx->f, 0, 0, 0);
    vector_init(vtx->vel, 0, 0, 0);
    return vtx;
}

/* insert a new halfedge after hafter. this is low-level,
 * i.e. it is a helper for the split_* functions, which
 * maintain the consistency of the solid.
 */
static hedge *hedge_new(hedge *hafter, vertex *vtx)
{
    hedge *h = (hedge*)malloc(sizeof(hedge));
    
    if(!h) {
/*	_DEBUG("Out of memory in hedge_new()\n",NULL); */
	return NULL;
    }
    h->f = hafter->f;
    h->vtx = vtx;
    h->e = NULL;
    h->prev = hafter;
    h->next = hafter->next;
    hafter->next = h;
    h->next->prev = h;
    return h;
}

static edge *edge_new(solid *s)
{
    edge *e = (edge*)malloc(sizeof(edge));
    if(!e) {
/*	_DEBUG("Out of memory in edge_new()\n",NULL);*/
	exit(-1);
    }
    e->next = s->edges;
    s->edges = e;
    e-> s = s;
    e->left = e->right = NULL;
    return e;
}

static face *face_new(solid *s, hedge *h)
{
    face *f = (face*)malloc(sizeof(face));
    if(!f) {
/*	_DEBUG("Out of memory in face_new()",NULL);*/
	exit(-1);
    }
    f->s = s;
    f->start = h;
    f->next = s->faces;
    s->faces = f;
    return f;
}

/* split vertex vtx, creating a new edge after v on f
 * that goes to a new vertex at v, adjoining whatever
 * face is on the other side of the halfedge attached to
 * v on f. 
 * Assumptions:
 *    there are at least 2 faces.
 *    partner(h)->next->vtx == vtx
 * Post-assumptions:
 *    the new halfedge will be inserted AFTER the indicated
 *    halfedge. This means that f->start is guaranteed not to
 *    change.
 *    the vertex returned will have h==<the new halfedge>.
 */

static vertex *vertex_split(hedge *h, vector v)
{
    hedge *h2, *hn1, *hn2;
    vertex *vtxn;
    edge *en;
    face *f1, *f2;

    f1 = h->f;
    h2 = partner(h);
    f2 = h2->f;
    
    vtxn = vertex_new(f1->s, v);
    hn1 = hedge_new(h, vtxn);
    vtxn->h = hn1;
    hn2 = hedge_new(h2, vtxn);
    hn2->e = h->e;

    if(h2->e->left == h2)
	h2->e->left = hn2;
    else
	h2->e->right = hn2;

    en = edge_new(f1->s);
    en->left = hn1;
    en->right = h2;
    hn1->e = en;
    h2->e = en;
    return vtxn;
}

static face *face_split(face *f, hedge *h1, hedge *h2)
{
    hedge *hn1, *hn2, *tmp;
    edge *en;
    face *fn;

    if(h1->f != f || h2->f != f) {
/*	_DEBUG("Whoah, cap'n, yer usin' a bad halfedge!\n",NULL);*/
	exit(-1);
    }
    if(h1 == h2) {
/*	_DEBUG("Trying to split a face at a single vertex\n",NULL);*/
	exit(-1);
    }
    /* close the loops */
    h1->prev->next = h2;
    h2->prev->next = h1;
    tmp = h1->prev;
    h1->prev = h2->prev;
    h2->prev = tmp;
    /* insert halfedges & create edge */
    hn1 = hedge_new(h2->prev, h1->vtx);
    hn2 = hedge_new(h1->prev, h2->vtx);
    en = edge_new(f->s);
    en->left = hn1;
    en->right = hn2;
    hn1->e = en;
    hn2->e = en;

    /* make the new face, first find out which hedge is contained
    * in the original face, then start the new face at the other */
    tmp = f->start;
    while(tmp != h1 && tmp != h2)
	tmp = tmp->next;
    tmp = (tmp == h1) ? h2 : h1 ;
    fn = face_new(f->s, tmp);
    do {
	tmp->f = fn;
	tmp = tmp->next;
    } while(tmp != fn->start);
    fn->color = f->color;
    return fn;
}

static solid *solid_new(vector where) 
{
    solid *s = (solid*)malloc(sizeof(solid));
    face *f1, *f2;
    edge *e;
    vertex *vtx;
    hedge *h1,*h2;

    s->faces = NULL;
    s->edges = NULL;
    s->vertices = NULL;

    h1 = (hedge*)malloc(sizeof(hedge));
    h2 = (hedge*)malloc(sizeof(hedge));
    h1->next = h1->prev = h1;
    h2->next = h2->prev = h2;

    vtx = vertex_new(s, where);
    vtx->h = h1;
    h1->vtx = vtx;
    h2->vtx = vtx;

    e = edge_new(s);
    e->left = h1;
    e->right = h2;
    h1->e = e;
    h2->e = e;

    f1 = face_new(s, h1);
    f2 = face_new(s, h2);
    h1->f = f1;
    h2->f = f2;

    return s;
}

/* This is all the code directly related to constructing the jigglypuff */
static void face_tessel2(face *f)
{
    hedge *h1=f->start->prev, *h2=f->start->next;
    
    if(h1->next == h1)
	return;
    while(h2 != h1 && h2->next != h1) {
	f = face_split(f, h1, h2);
	h1 = f->start;
	h2 = f->start->next->next;
    }
}

/* This will only work with solids composed entirely of
 * triangular faces. It first add a vertex to the middle
 * of each edge, then walks the faces, connecting the
 * dots.
 * I'm abusing the fact that new faces and edges are always
 * added at the head of the list. If that ever changes,
 * this is borked. 
 */
static void solid_tesselate(solid *s) 
{
    edge *e = s->edges;
    face *f = s->faces;
    
    while(e) {
	vector v;
	midpoint(e->left->vtx->v, e->right->vtx->v, v);
	vertex_split(e->left, v);
	e = e->next;
    }
    while(f) {
	face_tessel2(f);
	f=f->next;
    }
}
		
static void solid_spherify(solid * s, coord size) 
{
    vertex *vtx = s->vertices;

    while(vtx) {
	normalize_to(vtx->v, size);
	vtx = vtx->next;
    }
}

static solid *tetrahedron(jigglystruct *js) 
{
    solid *s;
    vertex *vtx;
    vector v;
    hedge *h;
    face *f;
    int i;

    vector_init(v, 1, 1, 1);
    s = solid_new(v);
    vector_init(v, -1, -1, 1);
    h = s->faces->start;
    vtx = vertex_split(h, v);
    vector_init(v, -1, 1, -1);
    vtx = vertex_split(vtx->h, v);
    h = vtx->h;
    f = face_split(s->faces, h, h->prev);
    vector_init(v, 1, -1, -1);
    vertex_split(f->start, v);
    f = s->faces->next->next;
    h = f->start;
    face_split(f, h, h->next->next);

    if(js->color_style == COLOR_STYLE_FLOWERBOX) {
	f = s->faces;
	for(i=0; i<4; i++) {
	    f->color = flowerbox_colors[i];
	    f = f->next;
	}
    }	    

    return s;
}

static solid *tesselated_tetrahedron(coord size, int iter, jigglystruct *js) {
    solid *s = tetrahedron(js);
    int i;

    for(i=0; i<iter; i++) {
	solid_tesselate(s);
    }
    return s;
}

static void clownbarf_colorize(solid *s) {
    face *f = s->faces;
    while(f) {
	f->color = clownbarf_colors[random() % CLOWNBARF_NCOLORS];
	f = f->next;
    }
}

/* Here be the rendering code */

static inline void vertex_calcnormal(vertex *vtx, jigglystruct *js)
{
    hedge *start = vtx->h, *h=start;
    
    vector_init(vtx->n, 0, 0, 0);
    do {
	vector u, v, norm;
	vector_sub(h->prev->vtx->v, vtx->v, u);
	vector_sub(h->next->vtx->v, vtx->v, v);
	cross(u, v, norm);
	vector_add_to(vtx->n, norm);
	h = partner(h)->next;
    } while(h != start);
    if(!js->spooky)
	normalize(vtx->n);
    else
	vector_scale(vtx->n, js->spooky);
}

static inline void vertex_render(vertex *vtx, jigglystruct *js)
{
    glNormal3fv(vtx->n);
    glVertex3fv(vtx->v);
}

/* This can be optimized somewhat due to the fact that all
 * the faces are triangles. I haven't actually tested to
 * see what the cost is of calling glBegin/glEnd for each
 * triangle.
 */
static inline void face_render(face *f, jigglystruct *js)
{
    hedge *h1, *h2, *hend;

    h1 = f->start;
    hend = h1->prev;
    h2 = h1->next;
    
    if(js->color_style == COLOR_STYLE_FLOWERBOX ||
	js->color_style == COLOR_STYLE_CLOWNBARF)
	glColor4fv(f->color);
    glBegin(GL_TRIANGLES);
    while(h1 != hend && h2 !=hend) {
	vertex_render(h1->vtx, js);
	vertex_render(h2->vtx, js);
	vertex_render(hend->vtx, js);
	h1 = h2;
	h2 = h1->next;
    }
    glEnd();
}

static void jigglypuff_render(jigglystruct *js) 
{
    face *f = js->shape->faces;
    vertex *vtx = js->shape->vertices;

    while(vtx) {
	vertex_calcnormal(vtx, js);
	vtx = vtx->next;
    }
    while(f) {
	face_render(f, js);
	f=f->next;
    }
}

/* This is the jiggling code */

/* stable distance when subdivs == 4 */
#define STABLE_DISTANCE 0.088388347648

static void update_shape(jigglystruct *js)
{
    vertex *vtx = js->shape->vertices;
    edge *e = js->shape->edges;
    vector zero;

    vector_init(zero, 0, 0, 0);

    /* sum all the vertex-vertex forces */
    while(e) {
	vector f;
	coord mag;
	vector_sub(e->left->vtx->v, e->right->vtx->v, f);
	mag = js->stable_distance - magnitude(f);
	vector_scale(f, mag);
	vector_add_to(e->left->vtx->f, f);
	vector_sub(zero, f, f);
	vector_add_to(e->right->vtx->f, f);
	e = e->next;
    }

    /* scale back the v-v force and add the spherical force
     * then add the result to the vertex velocity, damping
     * if necessary. Finally, move the vertex */
    while(vtx) {
	coord mag;
	vector to_sphere;
	vector_scale(vtx->f, js->hold_strength);
	vector_copy(to_sphere, vtx->v);
	mag = 1 - magnitude(to_sphere);
	vector_scale(to_sphere, mag * js->spherify_strength);
	vector_add_to(vtx->f, to_sphere);
	vector_add_to(vtx->vel, vtx->f);
	vector_init(vtx->f, 0, 0, 0);
	mag = magnitude2(vtx->vel);
	if(mag > js->damping_velocity)
	    vector_scale(vtx->vel, js->damping_factor);
	vector_add_to(vtx->v, vtx->vel);
	vtx = vtx->next;
    }
}

/* These are the various initialization routines */

static void init_texture(ModeInfo *mi)
{
    XImage *img = xpm_to_ximage(mi->dpy, mi->xgwa.visual,
			       mi->xgwa.colormap, jigglymap_xpm);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
		 img->width, img->height, 0, GL_RGBA,
		 GL_UNSIGNED_BYTE, img->data);

    XDestroyImage(img);
}

static void setup_opengl(ModeInfo *mi, jigglystruct *js)
{
    const GLfloat lpos0[4] = {-12, 8, 12, 0};
    const GLfloat lpos1[4] = {7, -5, 0, 0};
    const GLfloat lcol0[4] = {0.7f, 0.7f, 0.65f, 1};
    const GLfloat lcol1[4] = {0.3f, 0.2f, 0.1f, 1};
    const GLfloat scolor[4]= {0.9f, 0.9f, 0.9f, 0.5f};

    glDrawBuffer(GL_BACK);
    glClearColor(0, 0, 0, 0);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_DEPTH_TEST);

    if(js->do_wireframe) {
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    else {
	glCullFace(GL_BACK);
	glFrontFace(GL_CW);
	glEnable(GL_CULL_FACE);
    }

    if(js->color_style != COLOR_STYLE_CHROME) {
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHT1);
	
	glLightfv(GL_LIGHT0, GL_POSITION, lpos0);
	glLightfv(GL_LIGHT1, GL_POSITION, lpos1);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, lcol0);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, lcol1);

	glEnable(GL_COLOR_MATERIAL);
	glColor4fv(js->jiggly_color);

	glMaterialfv(GL_FRONT, GL_SPECULAR, scolor);
	glMateriali(GL_FRONT, GL_SHININESS, js->shininess);
    }
    else { /* chrome */
	init_texture(mi);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
}

static int parse_color(jigglystruct *js)
{
    unsigned int r, g, b;
    if(!strcmp(color, "clownbarf")) {
	js->color_style = COLOR_STYLE_CLOWNBARF;
	return 1;
    }
    else if(!strcmp(color, "flowerbox")) {
	js->color_style = COLOR_STYLE_FLOWERBOX;
	return 1;
    }
    else if(!strcmp(color, "chrome")) {
	js->color_style = COLOR_STYLE_CHROME;
	return 1;
    }
    else if(!strcmp(color, "cycle")) {
	js->color_style = COLOR_STYLE_CYCLE;
	js->jiggly_color[0] = ((float)random()) / REAL_RAND_MAX * 0.7 + 0.3;
	js->jiggly_color[1] = ((float)random()) / REAL_RAND_MAX * 0.7 + 0.3;
	js->jiggly_color[2] = ((float)random()) / REAL_RAND_MAX * 0.7 + 0.3;
	js->jiggly_color[3] = 1.0f;
	js->color_dir[0] = ((float)random()) / REAL_RAND_MAX / 100.0;
	js->color_dir[1] = ((float)random()) / REAL_RAND_MAX / 100.0;
	js->color_dir[2] = ((float)random()) / REAL_RAND_MAX / 100.0;
	return 1;
    }
    else
	js->color_style = 0;
    if(strlen(color) != 7)
	return 0;
    if(sscanf(color,"#%02x%02x%02x", &r, &g, &b) != 3) {
	return 0;
    }
    js->jiggly_color[0] = ((float)r)/255;
    js->jiggly_color[1] = ((float)g)/255;
    js->jiggly_color[2] = ((float)b)/255;
    js->jiggly_color[3] = 1.0f;

    return 1;
}

static void randomize_parameters(jigglystruct *js) {
    do_tetrahedron = random() & 1;
    js->do_wireframe = !(random() & 3);
    js->color_style = random() % 5;
    if(js->color_style == COLOR_STYLE_NORMAL
	|| js->color_style == COLOR_STYLE_CYCLE) {
	js->jiggly_color[0] = ((float)random()) / REAL_RAND_MAX * 0.5 + 0.5;
	js->jiggly_color[1] = ((float)random()) / REAL_RAND_MAX * 0.5 + 0.5;
	js->jiggly_color[2] = ((float)random()) / REAL_RAND_MAX * 0.5 + 0.5;
	js->jiggly_color[3] = 1.0f;
	if(js->color_style == COLOR_STYLE_CYCLE) {
	    js->color_dir[0] = ((float)random()) / REAL_RAND_MAX / 100.0;
	    js->color_dir[1] = ((float)random()) / REAL_RAND_MAX / 100.0;
	    js->color_dir[2] = ((float)random()) / REAL_RAND_MAX / 100.0;
	}
    }
    if((js->color_style != COLOR_STYLE_CHROME) && (random() & 1))
	js->spooky = (random() % 6) + 4;
    else 
	js->spooky = 0;
    js->shininess = random() % 200;
    speed = (random() % 700) + 50;
    /* It' kind of dull if this is too high when it starts as a sphere */
    spherism = do_tetrahedron ? (random() % 500) + 20 : (random() % 100) + 10;
    hold = (random() % 800) + 100;
    distance = (random() % 500) + 100;
    damping = (random() % 800) + 50;
}    

static void calculate_parameters(jigglystruct *js, int subdivs) {
    /* try to compensate for the inherent instability at
     * low complexity. */
    float dist_factor = (subdivs == 6) ? 2 : (subdivs == 5) ? 1 : 0.5;

    js->stable_distance = ((float)distance / 500.0) 
	* (STABLE_DISTANCE / dist_factor);
    js->hold_strength = (float)hold / 10000.0;
    js->spherify_strength = (float)spherism / 10000.0;
    js->damping_velocity = (float)damping / 100000.0;
    js->damping_factor = 
	0.001/max(js->hold_strength, js->spherify_strength);

    js->speed = (float)speed / 1000.0;
}

/* The screenhack related functions begin here */

ENTRYPOINT Bool jigglypuff_handle_event(ModeInfo *mi, XEvent *event)
{
    jigglystruct *js = &jss[MI_SCREEN(mi)];
    
    if(event->xany.type == ButtonPress &&
       event->xbutton.button == Button1) {
	js->button_down = 1;
	gltrackball_start(js->trackball, event->xbutton.x, event->xbutton.y,
			  MI_WIDTH(mi), MI_HEIGHT(mi));
	return True;
    }
    else if(event->xany.type == ButtonRelease &&
	    event->xbutton.button == Button1) {
	js->button_down = 0;
	return True;
    }
  else if (event->xany.type == ButtonPress &&
           (event->xbutton.button == Button4 ||
            event->xbutton.button == Button5 ||
            event->xbutton.button == Button6 ||
            event->xbutton.button == Button7))
    {
      gltrackball_mousewheel (js->trackball, event->xbutton.button, 10,
                              !!event->xbutton.state);
      return True;
    }
    else if(event->xany.type == MotionNotify && js->button_down) {
	gltrackball_track(js->trackball, event->xmotion.x, event->xmotion.y,
			  MI_WIDTH(mi), MI_HEIGHT(mi));
	return True;
    }
    return False;
}

ENTRYPOINT void reshape_jigglypuff(ModeInfo *mi, int width, int height)
{
    GLfloat aspect = (GLfloat)width / (GLfloat)height;

    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-0.5*aspect, 0.5*aspect, -0.5, 0.5, 1, 20);
/*    glTranslatef(0, 0, -10);*/
}

ENTRYPOINT void draw_jigglypuff(ModeInfo *mi)
{
    jigglystruct *js = &jss[MI_SCREEN(mi)];
    
    glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(js->glx_context));
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0,0,-10);

    glRotatef(js->angle, sin(js->axis), cos(js->axis), -sin(js->axis));
    glTranslatef(0, 0, 5);
    if(!(js->button_down)) {
	if((js->angle += js->speed) >= 360.0f ) {
	    js->angle -= 360.0f;
	}
	if((js->axis+=0.01f) >= 2*M_PI ) {
	    js->axis -= 2*M_PI;
	}
    }
    gltrackball_rotate(js->trackball);

    if(js->color_style == COLOR_STYLE_CYCLE) {
	int i;
	vector_add(js->jiggly_color, js->color_dir, js->jiggly_color);
	
	for(i=0; i<3; i++) {
	    if(js->jiggly_color[i] > 1.0 || js->jiggly_color[i] < 0.3) {
		js->color_dir[i] = (-js->color_dir[i]);
		js->jiggly_color[i] += js->color_dir[i];
	    }
	}
	glColor4fv(js->jiggly_color);
    }
    
    jigglypuff_render(js);
    if(MI_IS_FPS(mi))
	do_fps(mi);
    glFinish();
    update_shape(js);
    glXSwapBuffers(MI_DISPLAY(mi), MI_WINDOW(mi));
}

ENTRYPOINT void init_jigglypuff(ModeInfo *mi)
{
    jigglystruct *js;
    int subdivs;

    if(!jss) {
	jss = (jigglystruct*)
	    calloc(MI_NUM_SCREENS(mi), sizeof(jigglystruct));
	if(!jss) {
	    fprintf(stderr, "%s: No..memory...must...abort..\n", progname);
	    exit(1);
	}
    }

    js = &jss[MI_SCREEN(mi)];

    js->do_wireframe = MI_IS_WIREFRAME(mi);

    js->shininess = shininess;

    subdivs = (complexity==1) ? 4 : (complexity==2) ? 5
	: (complexity==3) ? 6 : 5;

    js->spooky = spooky << (subdivs-3);

    if(!parse_color(js)) {
	fprintf(stderr, "%s: Bad color specification: '%s'.\n", progname, color);
	exit(-1);
    }
    
    if(random_parms)
	randomize_parameters(js);

    js->angle = frand(180);
    js->axis  = frand(M_PI);

    js->shape = tesselated_tetrahedron(1, subdivs, js);

    if(!do_tetrahedron)
	solid_spherify(js->shape, 1);

    if(js->color_style == COLOR_STYLE_CLOWNBARF)
	clownbarf_colorize(js->shape);

    calculate_parameters(js, subdivs);

    if((js->glx_context = init_GL(mi)) != NULL) {
	glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(js->glx_context));
	setup_opengl(mi, js);
	reshape_jigglypuff(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
    }
    else {
	MI_CLEARWINDOW(mi);
    }
    js->trackball = gltrackball_init();
/*    _DEBUG("distance : %f\nhold : %f\nspherify : %f\ndamping : %f\ndfact : %f\n",
	   js->stable_distance, js->hold_strength, js->spherify_strength,
	   js->damping_velocity, js->damping_factor);
    _DEBUG("wire : %d\nspooky : %d\nstyle : %d\nshininess : %d\n",
	   js->do_wireframe, js->spooky, js->color_style, js->shininess);*/
}

XSCREENSAVER_MODULE ("Jigglypuff", jigglypuff)

#endif /* USE_GL */

/* This is the end of the file */
