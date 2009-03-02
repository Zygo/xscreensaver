/* jigglypuff - the least colorful screensaver you'll ever see
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
 * This could either use more options, or less options and
 * more randomness.
 *
 * Apologies to anyone who thought they were getting a Pokemon
 * out of this.
 *
 * Of course, if you modify it to do something interesting and/or
 * funny, I'd appreciate receiving a copy.
 */

#include <X11/Intrinsic.h>

#ifdef STANDALONE
# define PROGCLASS        "Jigglypuff"
# define HACK_INIT        init_jigglypuff
# define HACK_DRAW        draw_jigglypuff
# define HACK_RESHAPE     reshape_jigglypuff
# define jigglypuff_opts  xlockmore_opts
# define DEFAULTS         "*random: True\n" \
                          "*delay: 20000\n" \
                          "*showFPS: False\n" \
                          "wireframe: False\n"

# include "xlockmore.h"
#else
# include "xlock.h"
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef USE_GL
#include <GL/gl.h>

#ifndef max
#define max(a,b) (((a)>(b))?(a):(b))
#define min(a,b) (((a)<(b))?(a):(b))
#endif

static int spherism;
static int hold;
static int distance;
static int damping;

static int do_wireframe;
static int do_tetrahedron;
static int do_spooky;
static int random_parms;

typedef struct solid solid;

typedef struct {
    float stable_distance;
    float hold_strength;
    float spherify_strength;
    float damping_velocity;
    float damping_factor;

    int do_wireframe;
    int do_spooky;

    solid *shape;
    float angle;
    float axis;

    GLXContext *glx_context;
}jigglystruct;

static jigglystruct *jss = NULL;

static XrmOptionDescRec opts[] = {
    {"-random", ".Jigglypuff.random", XrmoptionNoArg, (caddr_t)"true"},
    {"+random", ".Jigglypuff.random", XrmoptionNoArg, (caddr_t)"false"},
    {"-tetra", ".Jigglypuff.tetra", XrmoptionNoArg, (caddr_t)"true"},
    {"+tetra", ".Jigglypuff.tetra", XrmoptionNoArg, (caddr_t)"false"},
    {"-spooky", ".Jigglypuff.spooky", XrmoptionNoArg, (caddr_t)"true"},
    {"+spooky", ".Jigglypuff.spooky", XrmoptionNoArg, (caddr_t)"false"},
    {"-spherism", ".Jigglypuff.spherism", XrmoptionSepArg, "500"},
    {"-hold", ".Jigglypuff.hold", XrmoptionSepArg, "500"},
    {"-distance", "Jigglypuff.distance", XrmoptionSepArg, "500"},
    {"-damping", "Jigglypuff.damping", XrmoptionSepArg, "50"}
};

static argtype vars[] = {
    {(caddr_t*)&random_parms, "random", "Random", "False", t_Bool},
    {(caddr_t*)&do_tetrahedron, "tetra", "Tetra", "True", t_Bool},
    {(caddr_t*)&do_spooky, "spooky", "Spooky", "False", t_Bool},
    {(caddr_t*)&spherism, "spherism", "Spherism", "100", t_Int},
    {(caddr_t*)&hold, "hold", "Hold", "600", t_Int},
    {(caddr_t*)&distance, "distance", "Distance", "500", t_Int},
    {(caddr_t*)&damping, "damping", "Damping", "50", t_Int}
};

#undef countof
#define countof(x) ((int)(sizeof(x)/sizeof(*(x))))

ModeSpecOpt jigglypuff_opts = {countof(opts), opts, countof(vars), vars, NULL};

#ifdef DEBUG
#define _DEBUG(msg, args...) do { \
    fprintf(stderr, "%s : %d : " msg ,__FILE__,__LINE__ ,##args); \
} while(0)
#else
#define _DEBUG(msg, args...)
#endif

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

static void vector_init(vector v, coord x, coord y, coord z)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
}    

static void vector_copy(vector d, vector s)
{
    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
}

#if 0
static void vector_add(vector v1, vector v2, vector v)
{
    vector_init(v, v1[0]+v2[0], v1[1]+v2[1], v1[2]+v2[2]);
}
#endif

static void vector_add_to(vector v1, vector v2)
{
    v1[0] += v2[0];
    v1[1] += v2[1];
    v1[2] += v2[2];
}

static void vector_sub(vector v1, vector v2, vector v)
{
    vector_init(v, v1[0]-v2[0], v1[1]-v2[1], v1[2]-v2[2]);
}

static void vector_scale(vector v, coord s)
{
    v[0] *= s;
    v[1] *= s;
    v[2] *= s;
}

#if 0
static coord dot(vector v1, vector v2)
{
    return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}
#endif

static void cross(vector v1, vector v2, vector v)
{
    vector_init(v,
		v1[1]*v2[2] - v2[1]*v1[2],
		v1[2]*v2[0] - v2[2]*v1[0],
		v1[0]*v2[1] - v2[0]*v1[1]);
}

static coord magnitude2(vector v)
{
    return v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
}

static coord magnitude(vector v)
{
    return sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static void normalize(vector v)
{
    coord mag = 1.0/sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);

    v[0] *= mag;
    v[1] *= mag;
    v[2] *= mag;
}

static void normalize_to(vector v, coord m)
{
    coord mag = 1.0/sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2])/m;

    v[0] *= mag;
    v[1] *= mag;
    v[2] *= mag;
}

static void midpoint(vector v1, vector v2, vector v)
{
    vector_init(v,
		v1[0] + 0.5 * (v2[0] - v1[0]),
		v1[1] + 0.5 * (v2[1] - v1[1]),
		v1[2] + 0.5 * (v2[2] - v1[2]));
}

static hedge *partner(hedge *h) {
    if(!h->e)
	return NULL;
    if(h == h->e->left) {
	return h->e->right;
    }
    else if(h == h->e->right) {
	return h->e->left;
    }
    else {
	_DEBUG("Holy shit Batman! this edge is fucked up!\n");
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
	_DEBUG("Out of memory in hedge_new()\n");
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
	_DEBUG("Out of memory in edge_new()\n");
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
	_DEBUG("Out of memory in face_new()");
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
 *    the new halfedge will be inserted _before_ the
 *    halfedge owning vtx in f.
 *    THIS IS WRONG. FIX ME!!!
 * New Deal - the Invariants
 *    the new halfedge will be inserted AFTER the indicated
 *    halfedge. This means that f->start is guaranteed not to
 *    change.
 * Also, the vertex returned will have h==<the new halfedge>.
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
	_DEBUG("Whoah, cap'n, yer usin' a bad halfedge!\n");
	exit(-1);
    }
    if(h1 == h2) {
	_DEBUG("Trying to split a face at a single vertex\n");
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

static solid *tetra(void) 
{
    solid *s;
    vertex *vtx;
    vector v;
    hedge *h;
    face *f;

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

    return s;
}

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

static solid *tesselated_tetrahedron(coord size, int iter) 
{
    solid *s = tetra();
    int i;

    for(i=0; i<iter; i++) {
	solid_tesselate(s);
    }
    return s;
}

static void vertex_calcnormal(vertex *vtx, int spooky)
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
    if(!spooky)
	normalize(vtx->n);
    else
	vector_scale(vtx->n, 15);
}

static void vertex_render(vertex *vtx)
{
    glNormal3fv(vtx->n);
    glVertex3fv(vtx->v);
}

static void face_render(face *f)
{
    hedge *h1, *h2, *hend;

    h1 = f->start;
    hend = h1->prev;
    h2 = h1->next;
    
    glBegin(GL_TRIANGLES);
    while(h1 != hend && h2 !=hend) {
	vertex_render(h1->vtx);
	vertex_render(h2->vtx);
	vertex_render(hend->vtx);
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
	vertex_calcnormal(vtx, js->do_spooky);
	vtx = vtx->next;
    }
    while(f) {
	face_render(f);
	f=f->next;
    }
}

void calculate_parameters(jigglystruct *js) {
    js->stable_distance = (float)distance / 10000.0;
    js->hold_strength = (float)hold / 1000.0;
    js->spherify_strength = (float)spherism / 10000.0;
    js->damping_velocity = (float)damping / 100000.0;
    js->damping_factor = 
	min(0.1, 1.0/max(js->hold_strength, js->spherify_strength));
}

void randomize_parameters(void) {
    do_tetrahedron = !(random() & 1);
    do_spooky = !(random() & 3);
    do_wireframe = !(random() & 3);
    spherism = random() % 1000;
    hold = random() % 1000;
    distance = random() % 1000;
    damping = random() % 1000;
}    

void update_shape(jigglystruct *js) {
    vertex *vtx = js->shape->vertices;
    edge *e = js->shape->edges;
    vector zero;

    vector_init(zero, 0, 0, 0);

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

void draw_jigglypuff(ModeInfo *mi)
{
    jigglystruct *js = &jss[MI_SCREEN(mi)];

    glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(js->glx_context));
    
    glDrawBuffer(GL_BACK);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glRotatef(js->angle, sin(js->axis), cos(js->axis), -sin(js->axis));
    glTranslatef(0,0,5);

    if((js->angle+=0.1) >= 360.0f ) {
	js->angle -= 360.0f;
    }
    if((js->axis+=0.01f) >= 2*M_PI ) {
	js->axis -= 2*M_PI;
    }
    jigglypuff_render(js);
    if(mi->fps_p)
	do_fps(mi);
    glFinish();
    update_shape(js);
    glXSwapBuffers(MI_DISPLAY(mi), MI_WINDOW(mi));
}

void reshape_jigglypuff(ModeInfo *mi, int width, int height)
{
    GLfloat aspect = (GLfloat)width / (GLfloat)height;

    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-0.5*aspect, 0.5*aspect, -0.5, 0.5, 1, 20);
    glTranslatef(0,0,-10);
}

static void setup_opengl(ModeInfo *mi, jigglystruct *js)
{
    const GLfloat lpos0[4] = {-12, 8, 12, 0};
    const GLfloat lpos1[4] = {7, -5, 0, 0};
    const GLfloat lcol0[4] = {0.7, 0.7, 0.65, 1};
    const GLfloat lcol1[4] = {0.3, 0.2, 0.1, 1};
    const GLfloat color1[4]={1, 1, 1, 0.5};
    const GLfloat color2[4]={0.9, 0.9, 0.9, 0.5};

    glShadeModel(GL_SMOOTH);

    if(js->do_wireframe) {
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    else {
	glCullFace(GL_BACK);
	glFrontFace(GL_CW);
	glEnable(GL_CULL_FACE);
    }

    glEnable(GL_DEPTH_TEST);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);

    glLightfv(GL_LIGHT0, GL_POSITION, lpos0);
    glLightfv(GL_LIGHT1, GL_POSITION, lpos1);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lcol0);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, lcol1);

    glClearColor(0, 0, 0, 0);

    glMaterialfv(GL_FRONT, GL_DIFFUSE, color1);
    glMaterialfv(GL_FRONT, GL_SPECULAR, color2);
    glMateriali(GL_FRONT, GL_SHININESS, 100);
}

void init_jigglypuff(ModeInfo *mi)
{
    jigglystruct *js;

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
    
    if(random_parms)
	randomize_parameters();
    if((js->glx_context = init_GL(mi)) != NULL) {
	glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(js->glx_context));
	setup_opengl(mi, js);
    }
    js->shape = tesselated_tetrahedron(1, 5);
    if(!do_tetrahedron)
	solid_spherify(js->shape, 1);
    calculate_parameters(js);
    js->do_spooky = do_spooky;
}

#endif /* USE_GL */
