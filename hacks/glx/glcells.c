/* -*- Mode: C; tab-width: 2 -*- */
/* glcells --- Cells growing on your screen */

/*-
 * Cells growing on your screen
 *
 * Copyright (c) 2007 by Matthias Toussaint
 *
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
 * 2007: Written by Matthias Toussaint
 * 0.1 Initial version
 * 0.2 Bugfixes (threading) and code cleanup by Jamie Zawinski
 *     Window scaling bug + performance bug in tick()
 */
 
#include <sys/time.h> /* gettimeofday */

#include "xlockmore.h"
#include <math.h>

/**********************************
  DEFINES
 **********************************/

#define INDEX_OFFSET 100000
#define NUM_CELL_SHAPES 10

#define refresh_glcells 0
#define release_glcells 0
#define glcells_handle_event 0

#define DEF_DELAY     "20000"
#define DEF_MAXCELLS  "800"
#define DEF_RADIUS    "40"
#define DEF_SEEDS     "1"
#define DEF_QUALITY   "3"
#define DEF_KEEPOLD   "False"
#define DEF_MINFOOD   "5"
#define DEF_MAXFOOD   "20"
#define DEF_DIVIDEAGE "20"
#define DEF_MINDIST   "1.4"
#define DEF_PAUSE     "50"

#define DEFAULTS	"*delay:	30000            \n" \
			"*showFPS:      False            \n" \
			"*wireframe:    False            \n" \
			"*suppressRotationAnimation: True\n" \

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#ifndef HAVE_JWZGLES /* glDrawElements unimplemented... */
# define USE_VERTEX_ARRAY
#endif

#define TEX_SIZE 64

/**********************************
  TYPEDEFS
 **********************************/
 
typedef struct    /* a 3-D vector */
{
  double x, y, z;   /* 3-D coordinates (we don't need w here) */
} Vector;

typedef struct    /* a triangle (indexes of vertexes in some list) */
{
  int i[3];         /* the three indexes for the triangle corners */
} Triangle;

typedef struct
{
  float *vertex;
  float *normal;
  unsigned *index;
  int num_index;
} VertexArray;

typedef struct    /* an 3-D object without normal vectors */
{ 
  Vector *vertex;       /* the vertexes */
  Triangle *triangle;   /* triangle list */
  int num_vertex;       /* number of vertexes */
  int num_triangle;     /* number of triangles */
} Object;

typedef struct    /* an 3-D object with smooth normal vectors */
{
  Vector *vertex;       /* the vertexes */
  Vector *normal;       /* the vertex normal vectors */
  Triangle *triangle;   /* triangle list */
  int num_vertex;       /* number of vertexes */
  int num_triangle;     /* number of triangles */
} ObjectSmooth;

typedef struct    /* Cell */
{
  double x, y;      /* position */
  double vx, vy;    /* movement vector */
  int age;          /* cells age */
  double min_dist;  /* minimum distance to other cells */
  int energy;       /* health */
  double rotation;  /* random rot, so they don't look all the same */
  double radius;    /* current size of cell */
  double growth;    /* current growth rate. might be <1.0 while dividing,
                       >1.0 when finished dividing and food is available
                       and 1.0 when grown up */
} Cell;

typedef struct    /* hacks state */
{
  GLXContext *glx_context;
  int width, height;    /* current size of viewport */
  double screen_scale;  /* we scale content with window size */
  int num_cells;        /* current number of cell in list */
  Cell *cell;           /* array of cells */
  int cell_polys;
  GLfloat color[4];     /* current cell color */
  double radius;        /* cell radius */
  int move_dist;        /* min distance from neighbours for forking */
  int max_cells;        /* maximum number of cells */
  int num_seeds;        /* number of initial seeds */
  int keep_old_cells;   /* draw dead cells? */
  int divide_age;       /* min age for division */
                        /* display lists for the cell stages */
  int cell_list[NUM_CELL_SHAPES];    
  int nucleus_list;
  int minfood;          /* minimum amount of food per area unit */
  int maxfood;          /* maximum amount of food per area unit */
  int pause;            /* pause at end (all cells dead) */
  int pause_counter;
  int wire;             /* draw wireframe? */
  Object *sphere;       /* the raw undisturbed sphere */
  double *disturbance;  /* disturbance values for the vertexes */
  int *food;            /* our petri dish (e.g. screen) */
  GLubyte *texture;     /* texture data for nucleus */
  GLuint texture_name;  /* texture name for binding */
} State;

/**********************************
  STATIC STUFF
 **********************************/
 
static State *sstate = NULL;

static XrmOptionDescRec opts[] = {
  { "-maxcells",   ".maxcells",   XrmoptionSepArg, 0 },
  { "-radius",     ".radius",     XrmoptionSepArg, 0 },
  { "-seeds",      ".seeds",      XrmoptionSepArg, 0 },
  { "-quality",    ".quality",    XrmoptionSepArg, 0 },
  { "-minfood",    ".minfood",    XrmoptionSepArg, 0 },
  { "-maxfood",    ".maxfood",    XrmoptionSepArg, 0 },
  { "-divideage",  ".divideage",  XrmoptionSepArg, 0 },
  { "-mindist",    ".mindist",    XrmoptionSepArg, 0 },
  { "-pause",      ".pause",      XrmoptionSepArg, 0 },
  { "-keepold",    ".keepold",    XrmoptionNoArg, "True" }
};

static int  s_maxcells;
static int  s_radius;
static int  s_seeds;
static int  s_quality;
static int  s_minfood;
static int  s_maxfood;
static int  s_divideage;
static int  s_pause;
static float s_min_dist;
static Bool s_keepold;

static argtype vars[] = {
  {&s_maxcells,  "maxcells",  "Max Cells",      DEF_MAXCELLS, t_Int},
  {&s_radius,    "radius",    "Radius",         DEF_RADIUS,   t_Int},
  {&s_seeds,     "seeds",     "Seeds",          DEF_SEEDS,    t_Int},
  {&s_quality,   "quality",   "Quality",        DEF_QUALITY,  t_Int},
  {&s_minfood,   "minfood",   "Min Food",       DEF_MINFOOD,  t_Int},
  {&s_maxfood,   "maxfood",   "Max Food",       DEF_MAXFOOD,  t_Int},
  {&s_pause,   "pause",     "Pause at end",   DEF_PAUSE,  t_Int},
  {&s_divideage, "divideage", "Age for duplication (Ticks)",       DEF_DIVIDEAGE,  t_Int},
  {&s_min_dist,  "mindist",   "Minimum preferred distance to other cells",       DEF_MINDIST,  t_Float},
  {&s_keepold,   "keepold",   "Keep old cells", DEF_KEEPOLD,  t_Bool}
};

/**********************************
  PROTOTYPES
 **********************************/
 
/* render scene */
static int render( State *st );
/* create initial cells and fill petri dish with food */
static void create_cells( State * );
/* do one animation step */
static void tick( State *st );
/* draw a single cell */
static void draw_cell( State *st, int shape );
/* draw cells nucleus */
static void draw_nucleus( State *st );
/* return randum number in the interval min-max */
static int random_interval( int min, int max );
/* retunr random number in the interval 0-max */
static int random_max( int max );
/* create display list for given disturbance weighting factor */
static int create_list( State *st, double fac );
/* return length of vector */
static double vector_length( Vector * );
/* normalize vector */
static void vector_normalize( Vector * );
/* a += b */
static void vector_add( Vector *a, Vector *b );
/* a -= b */
static void vector_sub( Vector *a, Vector *b );
/* a *= fac */
static void vector_mul( Vector *a, double fac );
/* a.x = a.y = a.z = 0 */
static void vector_clear( Vector *a );
/* return crossproduct a*b in out */
static void vector_crossprod( Vector *a, Vector *b, Vector *out );
/* return 1 if vectors are equal (epsilon compare) otherwise 0 */
static int vector_compare( Vector *a, Vector *b );
/* compute normal vector of given triangle and return in out */
static void triangle_normal( Vector *a, Vector *b, Vector *c, Vector *out );
/* take an Object and create an ObjectSmooth out of it */
static ObjectSmooth *create_ObjectSmooth( Object * );
/* Subdivide the Object once (assuming it's supposed to be a shpere */
static Object *subdivide( Object *obj );
/* free an Object */
static void free_Object( Object * );
/* free an ObjectSmooth */
static void free_ObjectSmooth( ObjectSmooth * );
/* scale an Object. return pointer to the object */
/*static Object *scale_Object( Object *obj, double scale );*/
/* create a perfect sphere refining with divisions */
static Object *create_sphere( State *st, int divisions );
/* make a copy of the given Object */
static Object *clone_Object( Object * );
/* return 1 if cell is capable to divide */
static int can_divide( State *st, Cell *cell );
#ifdef USE_VERTEX_ARRAY
static VertexArray *array_from_ObjectSmooth( ObjectSmooth * );
#endif
static void create_nucleus_texture( State *st );

ENTRYPOINT ModeSpecOpt glcells_opts = { countof(opts), opts,                                                   countof(vars), vars, 
                                        NULL };
                                        

/**********************************
  INLINE FUNCTIONS
 **********************************/
/* create random numbers
*/
static inline int random_interval( int min, int max )
{
  int n = max - min;
  if (n == 0) n = 1;
  return min+(random()%n);
}

static inline int random_max( int max )
{
  return random()%max;
}

/* Vector stuff
*/

/* a += b */
static inline void vector_add( Vector *a, Vector *b )
{
  a->x += b->x;
  a->y += b->y;
  a->z += b->z;
}

/* a -= b */
static inline void vector_sub( Vector *a, Vector *b )
{
  a->x -= b->x;
  a->y -= b->y;
  a->z -= b->z;
}

/* a *= v */
static inline void vector_mul( Vector *a, double v )
{
  a->x *= v;
  a->y *= v;
  a->z *= v;
}

/* set to 0 */
static inline void vector_clear( Vector *vec )
{
  vec->x = vec->y = vec->z = 0;
}

/* return vector length */
static inline double vector_length( Vector *vec )
{
  return sqrt( vec->x*vec->x + vec->y*vec->y + vec->z*vec->z );
}

/* normalize vector */
static inline void vector_normalize( Vector *vec )
{
  double len = vector_length( vec );
  
  if (len != 0.0) {
    vector_mul( vec, 1.0 / len );
  }
}

/* crossproduct */
static inline void vector_crossprod( Vector *a, Vector *b, Vector *out )
{
  out->x = a->y*b->z - a->z*b->y;
  out->y = a->z*b->x - a->x*b->z;
  out->z = a->x*b->y - a->y*b->x;
}

/* epsilon compare of two vectors */
static inline int vector_compare( Vector *a, Vector *b )
{
  const double epsilon = 0.0000001;
  Vector delta = *a;
  
  vector_sub( &delta, b );
  if (fabs(delta.x) < epsilon && 
      fabs(delta.y) < epsilon &&
      fabs(delta.z) < epsilon) {
    return 1;
  }
  
  return 0;
}

/* check if given cell is capable of dividing 
   needs space, must be old enough, grown up and healthy
*/
static inline int can_divide( State *st, Cell *cell )
{
  if (cell->min_dist > st->move_dist &&
      cell->age >= st->divide_age &&
      cell->radius > 0.99 * st->radius &&
      cell->energy > 0) {
    return 1;
  }
  
  return 0;
}

/**********************************
  FUNCTIONS
 **********************************/

/* compute normal vector of given 
   triangle spanned by the points a, b, c 
*/
static void triangle_normal( Vector *a, Vector *b, Vector *c, Vector *out )
{
  Vector v1 = *a;
  Vector v2 = *a;
  
  vector_sub( &v1, b );
  vector_sub( &v2, c );
  vector_crossprod( &v1, &v2, out );
}

/* free */
static void free_Object( Object *obj )
{
  free( obj->vertex );
  free( obj->triangle );
  free( obj );
}

static void free_ObjectSmooth( ObjectSmooth *obj )
{
  free( obj->vertex );
  free( obj->triangle );
  free( obj->normal );
  free( obj );
}

/* scale the given Object */
#if 0
static Object *scale_Object( Object *obj, double scale )
{
  int v;
  
  for (v=0; v<obj->num_vertex; ++v) {
    vector_mul( &obj->vertex[v], scale );
  }
  
  return obj;
}
#endif

/* create a copy of the given Object */
static Object *clone_Object( Object *obj )
{
  /* alloc */
  Object *ret = (Object *) malloc( sizeof( Object ) );
  
  ret->vertex = 
      (Vector *) malloc( obj->num_vertex*sizeof(Vector) );
  ret->triangle = 
      (Triangle *) malloc( obj->num_triangle*sizeof(Triangle) );
  ret->num_vertex = obj->num_vertex;
  ret->num_triangle = obj->num_triangle;
  /* copy */
  memcpy( ret->vertex, obj->vertex, 
          obj->num_vertex*sizeof(Vector) );
  memcpy( ret->triangle, obj->triangle,
          obj->num_triangle*sizeof(Triangle) );
  
  return ret;
}

#ifdef USE_VERTEX_ARRAY
static VertexArray *array_from_ObjectSmooth( ObjectSmooth *obj )
{
  int i, j;
  VertexArray *array = (VertexArray *) malloc( sizeof( VertexArray ) );
  
  array->vertex = (float *) malloc( 3*sizeof(float)*obj->num_vertex );
  array->normal = (float *) malloc( 3*sizeof(float)*obj->num_vertex );
  array->index = (unsigned *) malloc( 3*sizeof(unsigned)*obj->num_triangle );
  array->num_index = obj->num_triangle*3;
  
  for (i=0, j=0; i<obj->num_vertex; ++i) {
    array->vertex[j]   = obj->vertex[i].x;
    array->normal[j++] = obj->normal[i].x;
    array->vertex[j]   = obj->vertex[i].y;
    array->normal[j++] = obj->normal[i].y;
    array->vertex[j]   = obj->vertex[i].z;
    array->normal[j++] = obj->normal[i].z;
  }

  for (i=0, j=0; i<obj->num_triangle; ++i) {
    array->index[j++] = obj->triangle[i].i[0];
    array->index[j++] = obj->triangle[i].i[1];
    array->index[j++] = obj->triangle[i].i[2];
  }
  
  return array;
}
#endif /* USE_VERTEX_ARRAY */


/* create a smoothed version of the given Object
   by computing average normal vectors for the vertexes 
*/
static ObjectSmooth *create_ObjectSmooth( Object *obj )
{
  int t, v, i;
  Vector *t_normal = 
      (Vector *) malloc( obj->num_triangle*sizeof(Vector) );
  ObjectSmooth *ret = 
      (ObjectSmooth *) malloc( sizeof( ObjectSmooth ) );
  
  /* fill in vertexes and triangles */
  ret->num_vertex = obj->num_vertex;
  ret->num_triangle = obj->num_triangle;
  ret->vertex = 
      (Vector *) malloc( obj->num_vertex * sizeof( Vector ) );
  ret->normal = 
      (Vector *) malloc( obj->num_vertex * sizeof( Vector ) );
  ret->triangle = 
      (Triangle *) malloc( obj->num_triangle * sizeof( Triangle ) );
  
  for (v=0; v<obj->num_vertex; ++v) {
    ret->vertex[v] = obj->vertex[v];
  }
  
  for (t=0; t<obj->num_triangle; ++t) {
    ret->triangle[t] = obj->triangle[t];
  }
  
  /* create normals (triangles) */
  for (t=0; t<ret->num_triangle; ++t) {
    triangle_normal( &ret->vertex[ret->triangle[t].i[0]],
                     &ret->vertex[ret->triangle[t].i[1]],
                     &ret->vertex[ret->triangle[t].i[2]],
                     &t_normal[t] );
  }
  
  /* create normals (vertex) by averaging triangle 
     normals at vertex
  */
  for (v=0; v<ret->num_vertex; ++v) {
    vector_clear( &ret->normal[v] );
    for (t=0; t<ret->num_triangle; ++t) {
      for (i=0; i<3; ++i) {
        if (ret->triangle[t].i[i] == v) {
          vector_add( &ret->normal[v], &t_normal[t] );
        }
      }
    }
    /* as we have only a half sphere we force the
       normals at the bortder to be perpendicular to z.
       the simple algorithm above makes an error here.
    */
    if (fabs(ret->vertex[v].z) < 0.0001) {
      ret->normal[v].z = 0.0;
    }  
    
    vector_normalize( &ret->normal[v] );
  }
  
  free( t_normal );
  
  return ret;
}

/* subdivide the triangles of the object once
   The order of this algorithm is probably something like O(n^42) :)
   but I can't think of something smarter at the moment 
*/
static Object *subdivide( Object *obj )
{
  /* create for worst case (which I dont't know) */
  int start, t, i, v;
  int index_list[1000];
  int index_cnt, index_found;
  Object *tmp = (Object *)malloc( sizeof(Object) );
  Object *ret = (Object *)malloc( sizeof(Object) );
  Object *c_ret;
  
  tmp->vertex = 
      (Vector *)malloc( 100*obj->num_vertex*sizeof( Vector ) );
  tmp->triangle = 
      (Triangle *)malloc( 4*obj->num_triangle*sizeof( Triangle ) );
  tmp->num_vertex = 0;
  tmp->num_triangle = 0;
  ret->vertex = 
      (Vector *)malloc( 100*obj->num_vertex*sizeof( Vector ) );
  ret->triangle = 
      (Triangle *)malloc( 4*obj->num_triangle*sizeof( Triangle ) );
  ret->num_vertex = 0;
  ret->num_triangle = 0;
#ifdef PRINT_STAT
  fprintf( stderr, "in v=%d t=%d\n", 
           obj->num_vertex, obj->num_triangle );
#endif
  /* for each triangle create 3 new vertexes and the 4 
     corresponding triangles 
  */
  for (t=0; t<obj->num_triangle; ++t) {
    /* copy the three original vertexes */
    for (i=0; i<3; ++i) {
      tmp->vertex[tmp->num_vertex++] =
        obj->vertex[obj->triangle[t].i[i]];
    }
    
    /* create 3 new */
    tmp->vertex[tmp->num_vertex] = 
        obj->vertex[obj->triangle[t].i[0]];
    vector_add( &tmp->vertex[tmp->num_vertex],
                 &obj->vertex[obj->triangle[t].i[1]] );
    vector_mul( &tmp->vertex[tmp->num_vertex++], 0.5 ); 
    
    tmp->vertex[tmp->num_vertex] = 
        obj->vertex[obj->triangle[t].i[1]];
    vector_add( &tmp->vertex[tmp->num_vertex],
                 &obj->vertex[obj->triangle[t].i[2]] );
    vector_mul( &tmp->vertex[tmp->num_vertex++], 0.5 ); 
    
    tmp->vertex[tmp->num_vertex] = 
        obj->vertex[obj->triangle[t].i[2]];
    vector_add( &tmp->vertex[tmp->num_vertex],
                 &obj->vertex[obj->triangle[t].i[0]] );
    vector_mul( &tmp->vertex[tmp->num_vertex++], 0.5 ); 

    /* create triangles */
    start = tmp->num_vertex-6;
    
    tmp->triangle[tmp->num_triangle].i[0] = start;
    tmp->triangle[tmp->num_triangle].i[1] = start+3;
    tmp->triangle[tmp->num_triangle++].i[2] = start+5;
      
    tmp->triangle[tmp->num_triangle].i[0] = start+3;
    tmp->triangle[tmp->num_triangle].i[1] = start+1;
    tmp->triangle[tmp->num_triangle++].i[2] = start+4;
      
    tmp->triangle[tmp->num_triangle].i[0] = start+5;
    tmp->triangle[tmp->num_triangle].i[1] = start+4;
    tmp->triangle[tmp->num_triangle++].i[2] = start+2;
      
    tmp->triangle[tmp->num_triangle].i[0] = start+3;
    tmp->triangle[tmp->num_triangle].i[1] = start+4;
    tmp->triangle[tmp->num_triangle++].i[2] = start+5;
  }
  
  /* compress object eliminating double vertexes 
     (welcome to the not so smart section)
  */
  /* copy original triangle list */
  for (t=0; t<tmp->num_triangle; ++t) {
    ret->triangle[t] = tmp->triangle[t];
  }
  ret->num_triangle = tmp->num_triangle;
  
  /* copy unique vertexes and correct triangle list */
  for (v=0; v<tmp->num_vertex; ++v) {
    /* create list of vertexes that are the same */
    index_cnt = 0;
    for (i=0; i<tmp->num_vertex; ++i) {
      /* check if i and v are the same
         first in the list is the smallest index
      */
      if (vector_compare( &tmp->vertex[v], &tmp->vertex[i] )) {
        index_list[index_cnt++] = i;
      }
    }
    
    /* check if vertex unknown so far */
    index_found = 0;
    for (i=0; i<ret->num_vertex; ++i) {
      if (vector_compare( &ret->vertex[i],
          &tmp->vertex[index_list[0]] )) {
        index_found = 1;
        break;
      }
    }
    
    if (!index_found) {
      ret->vertex[ret->num_vertex] = tmp->vertex[index_list[0]];
      
      /* correct triangles 
         (we add an offset to the index, so we can tell them apart)
      */
      for (t=0; t<ret->num_triangle; ++t) {
        for (i=0; i<index_cnt; ++i) {
          if (ret->triangle[t].i[0] == index_list[i]) {
            ret->triangle[t].i[0] = ret->num_vertex+INDEX_OFFSET;
          }
          if (ret->triangle[t].i[1] == index_list[i]) {
            ret->triangle[t].i[1] = ret->num_vertex+INDEX_OFFSET;
          }
          if (ret->triangle[t].i[2] == index_list[i]) {
            ret->triangle[t].i[2] = ret->num_vertex+INDEX_OFFSET;
          }
        }
      }
      ret->num_vertex++;
    }
  }
  
  free_Object( tmp );
  
  /* correct index offset */
  for (t=0; t<ret->num_triangle; ++t) {
    ret->triangle[t].i[0] -= INDEX_OFFSET;
    ret->triangle[t].i[1] -= INDEX_OFFSET;
    ret->triangle[t].i[2] -= INDEX_OFFSET;
  }
  
  /* normalize vertexes */
  for (v=0; v<ret->num_vertex; ++v) {
    vector_normalize( &ret->vertex[v] );
  }
#ifdef PRINT_STAT
  fprintf( stderr, "out v=%d t=%d\n", 
           ret->num_vertex, ret->num_triangle );
#endif
  /* shrink the arrays by cloning */
  c_ret = clone_Object( ret );
  free_Object( ret );
  
  return c_ret;
}

static int render( State *st )
{
#ifdef PRINT_STAT
  struct timeval tv1, tv2;
  int usec;
#endif
  GLfloat LightAmbient[]= { 0.1f, 0.1f, 0.1f, 1.0f };
  GLfloat LightPosition[]= { -20.0f, -10.0f, -100.0f, 0.0f };
  int b;
  int num_paint = 0;
  
  if (0 == st->food) return 0;
#ifdef PRINT_STAT
  gettimeofday( &tv1, NULL );
#endif
  /* life goes on... */
  tick( st );
#ifdef PRINT_STAT
  gettimeofday( &tv2, NULL );
  usec = (tv2.tv_sec-tv1.tv_sec)*1000000+(tv2.tv_usec-tv1.tv_usec);
  fprintf( stderr, "tick %d\n", usec );
  gettimeofday( &tv1, NULL );
#endif

  glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
  glDepthFunc(GL_LESS);
  glEnable(GL_DEPTH_TEST);
  glLightfv( GL_LIGHT0, GL_AMBIENT, LightAmbient );
  glLightfv( GL_LIGHT0, GL_DIFFUSE, st->color );
  glLightfv( GL_LIGHT0, GL_POSITION, LightPosition );
  
  /* prepare lighting vs. wireframe */
  if (!st->wire) {
    glEnable( GL_LIGHT0 );
    glEnable( GL_LIGHTING );
    glEnable( GL_NORMALIZE );
    glPolygonMode( GL_FRONT, GL_FILL );
  } else {
# ifndef HAVE_JWZGLES /* #### glPolygonMode other than GL_FILL unimplemented */
    glPolygonMode( GL_FRONT, GL_LINE );
# endif
  }
  
# if 0
  if (st->wire) {
    glDisable(GL_DEPTH_TEST);
    glColor3f (1, 1, 1);
    glBegin(GL_LINE_LOOP);
    glVertex3f(0, 0, 0); glVertex3f(st->width, 0, 0);
    glVertex3f(st->width, st->height, 0); glVertex3f(0, st->height, 0);
    glVertex3f(0, 0, 0); glVertex3f(st->width/4, 0, 0);
    glVertex3f(st->width/4, st->height/4, 0); glVertex3f(0, st->height/4, 0);
    glEnd();
  }
# endif

  /* draw the dead cells if choosen */
  if (st->keep_old_cells) {
    for (b=0; b<st->num_cells; ++b) {
      if (st->cell[b].energy <= 0) {
        num_paint++;
        glPushMatrix();
        glTranslatef( st->cell[b].x, st->cell[b].y, 0.0 );
        glRotatef( st->cell[b].rotation, 0.0, 0.0, 1.0 );
        glScalef( st->cell[b].radius, st->cell[b].radius, st->cell[b].radius );
        draw_cell( st, 9 );
        glPopMatrix();
      }
    }
  }
  
  /* draw the living cells */
  for (b=0; b<st->num_cells; ++b) {
     if (st->cell[b].energy >0) { 
      double fac = (double)st->cell[b].energy / 50.0; 
      int shape;
      if (fac < 0.0) fac = 0.0;
      if (fac > 1.0) fac = 1.0;
      
      shape = (int)(9.0*fac);
      num_paint++;
      /*glColor3f( fac, fac, fac );*/
      
# if 0
      if (st->wire) {
        glBegin(GL_LINES);
        glVertex3f(0, 0, 0);
        glVertex3f(st->cell[b].x, st->cell[b].y, 0);
        glEnd();
      }
# endif

      glPushMatrix();
      glTranslatef( st->cell[b].x, st->cell[b].y, 0.0 );
      glRotatef( st->cell[b].rotation, 0.0, 0.0, 1.0 );
      glScalef( st->cell[b].radius, st->cell[b].radius, st->cell[b].radius );
      draw_cell( st, 9-shape );
      glPopMatrix();
    }
  }
  
  /* draw cell nuclei */
  if (!st->wire)
  {
    glDisable( GL_LIGHT0 );
    glDisable( GL_LIGHTING );
    
    glEnable( GL_BLEND );
    glDisable( GL_DEPTH_TEST );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );    
    glEnable( GL_TEXTURE_2D );
    glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
    glBindTexture( GL_TEXTURE_2D, st->texture_name );
    
    for (b=0; b<st->num_cells; ++b) {
      if (st->cell[b].energy>0 || st->keep_old_cells) {
        glPushMatrix();
        glTranslatef( st->cell[b].x, st->cell[b].y, 0.0 );
        glScalef( st->cell[b].radius, st->cell[b].radius, st->cell[b].radius );
        draw_nucleus( st );
        glPopMatrix();
      }
    }
    
    glDisable( GL_TEXTURE_2D );
    glDisable( GL_BLEND );
  }
  
#ifdef PRINT_STAT
  gettimeofday( &tv2, NULL );
  usec = (tv2.tv_sec-tv1.tv_sec)*1000000+(tv2.tv_usec-tv1.tv_usec);
  fprintf( stderr, "OpenGL %d\n", usec );
#endif
  return num_paint * st->cell_polys;
}

/* this creates the initial subdivided half-dodecaedron */
static Object *create_sphere( State *st, int divisions )
{
  int num_vertex = 9;
  int num_triangle = 10;  
  int i, v, t;
  double a, aStep = (double)M_PI / 3.0;
  double e;
  int vi[30] = { 0, 7, 1, 1, 7, 2, 2, 8, 3, 3, 8, 4, 4, 6, 5,
                 5, 6, 0, 0, 6, 7, 2, 7, 8, 4, 8, 6, 6, 8, 7 };
  Object *obj = (Object *)malloc( sizeof( Object ) );
  
  obj->vertex = (Vector *)malloc( num_vertex*sizeof( Vector ) );
  obj->triangle = 
      (Triangle *)malloc( num_triangle*sizeof( Triangle ) );
  obj->num_vertex = num_vertex;
  obj->num_triangle = num_triangle;
                
  /* create vertexes for dodecaedron */
  a = 0.0;
  for (v=0; v<6; ++v) {
    obj->vertex[v].x = sin( a );
    obj->vertex[v].y = -cos( a );
    obj->vertex[v].z = 0.0;
  
    a += aStep;
  }

  a = -60.0/180.0*(double)M_PI;
  e = 58.2825/180.0 * (double)M_PI;
  for (;v<9; ++v) {
    obj->vertex[v].x = sin( a )*cos( e );
    obj->vertex[v].y = -cos( a )*cos( e );
    obj->vertex[v].z = -sin( e );
  
    a += 2.0*aStep;
  }

  /* create triangles */
  for (t=0; t<obj->num_triangle; ++t) {
    obj->triangle[t].i[0] = vi[3*t];
    obj->triangle[t].i[1] = vi[3*t+1];
    obj->triangle[t].i[2] = vi[3*t+2];
  }

  /* subdivide as specified */
  for (i=0; i<divisions; ++i) {
    Object *newObj = subdivide( obj );
    free_Object( obj );
    obj = newObj;
  }
  
  st->cell_polys = obj->num_triangle;
  
  return obj;
}

static int create_list( State *st, double fac )
{
  int v;
  Object *obj = clone_Object( st->sphere );
  ObjectSmooth *smooth;
#ifdef USE_VERTEX_ARRAY
  VertexArray *vertex_array;
#else
  int t, i;
#endif
  int list = glGenLists(1);  
  
  /* apply wrinckle factor */
  for (v=0; v<obj->num_vertex; ++v) {
    vector_mul( &obj->vertex[v], 1.0+fac*st->disturbance[v] );
  }
  
  /* compute normals */
  smooth = create_ObjectSmooth( obj );
  free_Object( obj );
  
  /* Create display list */
  glNewList( list, GL_COMPILE );
#ifdef USE_VERTEX_ARRAY
  vertex_array = array_from_ObjectSmooth( smooth );
  glEnableClientState( GL_VERTEX_ARRAY );
  glEnableClientState( GL_NORMAL_ARRAY );
  glVertexPointer( 3, GL_FLOAT, 0, vertex_array->vertex );
  glNormalPointer( GL_FLOAT, 0, vertex_array->normal );
  glDrawElements( GL_TRIANGLES, vertex_array->num_index, 
                  GL_UNSIGNED_INT, vertex_array->index );
  free( vertex_array );
#else
  glBegin( GL_TRIANGLES );
  
  for (t=0; t<smooth->num_triangle; ++t) {
    for (i=0; i<3; ++i) {
      glNormal3f( smooth->normal[smooth->triangle[t].i[i]].x, 
                  smooth->normal[smooth->triangle[t].i[i]].y, 
                  smooth->normal[smooth->triangle[t].i[i]].z );
      glVertex3f( smooth->vertex[smooth->triangle[t].i[i]].x, 
                  smooth->vertex[smooth->triangle[t].i[i]].y, 
                  smooth->vertex[smooth->triangle[t].i[i]].z );
    }
  }    
  
  glEnd();
#endif
  glEndList();
  
  free_ObjectSmooth( smooth );
  
  return list;
}

static void draw_cell( State *st, int shape )
{
# ifdef HAVE_JWZGLES /* #### glPolygonMode other than GL_FILL unimplemented */
  if (st->wire) {
    glDisable(GL_DEPTH_TEST);
    glColor3f (1, 1, 1);
    glPushMatrix();
    glScalef (0.33, 0.33, 1);
    glBegin (GL_LINE_LOOP);
    glVertex3f (-1, -1, 0); glVertex3f (-1,  1, 0);
    glVertex3f ( 1,  1, 0); glVertex3f ( 1, -1, 0);
    glEnd();
    if (shape == 9) {
      glBegin (GL_LINES);
      glVertex3f (-1, -1, 0); glVertex3f (1,  1, 0);
      glVertex3f (-1,  1, 0); glVertex3f (1, -1, 0);
      glEnd();
    }
    glPopMatrix();
    return;
  }
# endif

  if (-1 == st->cell_list[shape]) {
    st->cell_list[shape] = create_list( st, (double)shape/10.0 );
  }
  
  glCallList( st->cell_list[shape] );
}

static void create_nucleus_texture( State *st )
{
  int x, y;
  int w2 = TEX_SIZE/2;
  float s = w2*w2/4.0;
  
  st->texture = (GLubyte *) malloc( 4*TEX_SIZE*TEX_SIZE );
  
  for (y=0; y<TEX_SIZE; ++y) {
    for (x=0; x<TEX_SIZE; ++x) {
      float r2 = ((x-w2)*(x-w2)+(y-w2)*(y-w2));
      float v = 120.0 * expf( -(r2) / s );
      st->texture[4*(x+y*TEX_SIZE)]   = (GLubyte)0;
      st->texture[4*(x+y*TEX_SIZE)+1] = (GLubyte)0;
      st->texture[4*(x+y*TEX_SIZE)+2] = (GLubyte)0;
      st->texture[4*(x+y*TEX_SIZE)+3] = (GLubyte)v;
    }
  }
  
  glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
  glGenTextures( 1, &st->texture_name );
  glBindTexture( GL_TEXTURE_2D, st->texture_name );
  
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
  glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, TEX_SIZE, TEX_SIZE, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, st->texture );
}

static void draw_nucleus( State *st )
{
  if (-1 == st->nucleus_list) {
    float z = -1.2f;
    float r=1.0/2.0f;
    st->nucleus_list = glGenLists( 1 );
    glNewList( st->nucleus_list, GL_COMPILE );
    glBegin( GL_QUADS );
    glTexCoord2f( 0.0f, 0.0f ); glVertex3f( -r, -r, z );
    glTexCoord2f( 0.0f, 1.0f ); glVertex3f( -r, r, z );
    glTexCoord2f( 1.0f, 1.0f ); glVertex3f( r, r, z );
    glTexCoord2f( 1.0f, 0.0f ); glVertex3f( r, -r, z );    
    glEnd();
    glEndList();
  }
  
  glCallList( st->nucleus_list );
}

static void create_cells( State *st )
{
  int border = (int)(200.0 * st->screen_scale);
  int i, foodcnt;
  int w = st->width-2*border;
  int h = st->height-2*border;
  
  st->color[0] = 0.5 + random_max( 1000 ) * 0.0005;
  st->color[1] = 0.5 + random_max( 1000 ) * 0.0005;
  st->color[2] = 0.5 + random_max( 1000 ) * 0.0005;
  st->color[3] = 1.0f;
  
  /* allocate if startup */
  if (!st->cell) {
    st->cell = (Cell *) malloc( st->max_cells * sizeof(Cell));
  }
  
  /* fill the screen with random food for our little critters */
  foodcnt = (st->width*st->height)/16;  
  for (i=0; i<foodcnt; ++i) {
    st->food[i] = random_interval( st->minfood, st->maxfood );
  }
    
  /* create the requested seed-cells */
  st->num_cells = st->num_seeds;

  for (i=0; i<st->num_cells; ++i) {
    st->cell[i].x        = border + random_max( w );
    st->cell[i].y        = border + random_max( h );
    st->cell[i].vx       = 0.0;
    st->cell[i].vy       = 0.0;
    st->cell[i].age      = random_max( 0x0f );
    st->cell[i].min_dist = 500.0;
    st->cell[i].energy   = random_interval( 5, 5+0x3f );
    st->cell[i].rotation = ((double)random()/(double)RAND_MAX)*360.0;
    st->cell[i].radius   = st->radius;
    st->cell[i].growth   = 1.0;
  }
}

/* all this is rather expensive :( */
static void tick( State *st )
{
  int new_num_cells, num_cells=0;
  int b, j;
  int x, y, w4=st->width/4, h4=st->height/4, offset;
  double min_dist;
  int min_index;
  int num_living = 0;
  const double check_dist = 0.75*st->move_dist;
  const double grow_dist = 0.75*st->radius;
  const double adult_radius = st->radius;
  
  /* find number of cells capable of division 
     and count living cells
  */
  for (b=0; b<st->num_cells; ++b) {
    if (st->cell[b].energy > 0) num_living++;
    if (can_divide( st, &st->cell[b] )) num_cells++;
  }
  new_num_cells = st->num_cells + num_cells;
  
  /* end of simulation ? */
  if (0 == num_living || new_num_cells >= st->max_cells) {
    if (st->pause_counter > 0) st->pause_counter--;
    if (st->pause_counter > 0) return;
    create_cells( st );
    st->pause_counter = st->pause;
  } else if (num_cells) { /* any fertile candidates ? */
    for (b=0, j=st->num_cells; b<st->num_cells; ++b) {
      if (can_divide( st, &st->cell[b] )) {
        st->cell[b].vx      = random_interval( -50, 50 ) * 0.01;
        st->cell[b].vy      = random_interval( -50, 50 ) * 0.01;
        st->cell[b].age     = random_max( 0x0f );
        /* half energy for both plus some bonus for forking */
        st->cell[b].energy  = 
            st->cell[b].energy/2 + random_max( 0x0f );
        /* forking makes me shrink */
        st->cell[b].growth  = 0.995;
        
        /* this one initially goes into the oposite direction */
        st->cell[j].vx       = -st->cell[b].vx;
        st->cell[j].vy       = -st->cell[b].vy;
        /* same center */
        st->cell[j].x        = st->cell[b].x;
        st->cell[j].y        = st->cell[b].y;
        st->cell[j].age      = random_max( 0x0f );
        st->cell[j].energy   = (st->cell[b].energy);
        st->cell[j].rotation =
            ((double)random()/(double)RAND_MAX)*360.0;
        st->cell[j].growth   = st->cell[b].growth;
        st->cell[j].radius   = st->cell[b].radius;
        ++j;
      } else {
        st->cell[b].vx = 0.0;
        st->cell[b].vy = 0.0;
      }
    }
    
    st->num_cells = new_num_cells;
  }

  /* for each find a direction to escape */
  if (st->num_cells > 1) {
    for (b=0; b<st->num_cells; ++b) {
      if (st->cell[b].energy > 0) {
        double vx;
        double vy;
        double len;
        
        /* grow or shrink */
        st->cell[b].radius *= st->cell[b].growth;
        /* find closest neighbour */
        min_dist = 100000.0;
        min_index = 0;
        for (j=0; j<st->num_cells; ++j) {
          if (j!=b) {
            const double dx = st->cell[b].x - st->cell[j].x;
            const double dy = st->cell[b].y - st->cell[j].y;
            
            if (fabs(dx) < check_dist || fabs(dy) < check_dist) {
              const double dist = dx*dx+dy*dy;
              /*const double dist = sqrt( dx*dx+dy*dy );*/
              if (dist<min_dist) {
                min_dist = dist;
                min_index = j;
              }
            }
          }
        }
        /* escape step is away from closest normalized with distance */
        vx = st->cell[b].x - st->cell[min_index].x;
        vy = st->cell[b].y - st->cell[min_index].y;
        len = sqrt( vx*vx + vy*vy );
        if (len > 0.0001) {
          st->cell[b].vx = vx/len;
          st->cell[b].vy = vy/len;
        }
        st->cell[b].min_dist = len;
        /* if not adult (radius too small) */
        if (st->cell[b].radius < adult_radius) {
          /* if too small 60% stop shrinking */
          if (st->cell[b].radius < adult_radius * 0.6) {
            st->cell[b].growth = 1.0;
          }
          /* at safe distance we start growing again */
          if (len > grow_dist) {
            if (st->cell[b].energy > 30) {
              st->cell[b].growth = 1.005;
            } 
          }
        } else {  /* else keep size */
          st->cell[b].growth = 1.0;
        }
      }
    }
  } else {
    st->cell[0].min_dist = 2*st->move_dist;
  }
    
  /* now move em, snack and burn energy */
  for (b=0; b<st->num_cells; ++b) {
    /* if still alive */
    if (st->cell[b].energy > 0) {
      /* agility depends on amount of energy */
      double fac = (double)st->cell[b].energy / 50.0;
      if (fac < 0.0) fac = 0.0;
      if (fac > 1.0) fac = 1.0;
      
      st->cell[b].x += fac*(2.0 - 
          (4.0*(double)random() / (double)RAND_MAX) + 
          st->cell[b].vx);
      st->cell[b].y += fac*(2.0 - 
          (4.0*(double)random() / (double)RAND_MAX) + 
          st->cell[b].vy);
      
      /* get older and burn energy */
      if (st->cell[b].energy > 0) {
        st->cell[b].age++;
        st->cell[b].energy--;
      }
      
      /* have a snack */
      x = ((int)st->cell[b].x)/4;
      if (x<0) x=0;
      if (x>=w4) x = w4-1;
      y = ((int)st->cell[b].y)/4;
      if (y<0) y=0;
      if (y>=h4) y = h4-1;
    
      offset = x+y*w4;
    
      /* don't eat if already satisfied */
      if (st->cell[b].energy < 100 &&
          st->food[offset] > 0) {
        st->food[offset]--;
        st->cell[b].energy++;
        /* if you are hungry, eat more */
        if (st->cell[b].energy < 50 && 
            st->food[offset] > 0) {
          st->food[offset]--;
          st->cell[b].energy++;
        }
      }
    }
  }
}

ENTRYPOINT void 
reshape_glcells( ModeInfo *mi, int width, int height )
{
  State *st  = &sstate[MI_SCREEN(mi)];
# ifdef HAVE_MOBILE
  int rot = current_device_rotation();
# endif
  st->height = height;
  st->width  = width;
# ifdef HAVE_MOBILE
  st->screen_scale = (double)(width < height ? width : height) / 1600.0;
# else
  st->screen_scale = (double)width / 1600.0;
# endif

  st->radius = s_radius;
  if (st->radius < 5) st->radius = 5;
  if (st->radius > 200) st->radius = 200;
  st->radius *= st->screen_scale;
       
  st->move_dist = s_min_dist;
  if (st->move_dist < 1.0) st->move_dist = 1.0;
  if (st->move_dist > 3.0) st->move_dist = 3.0;
  st->move_dist *= st->radius;

  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho( 0, width, height, 0, 200, 0 );
# ifdef HAVE_MOBILE
  glRotatef (rot, 0, 0, 1);
# endif
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  if (st->food) free( st->food );
  st->food = (int *)malloc( ((width*height)/16)*sizeof(int) );
  /* create_cells( st );*/

# ifdef HAVE_MOBILE
  glTranslatef (st->width/2, st->height/2, 0);
  if (rot == 90 || rot == -90 || rot == 270 || rot == -270)
    st->width = height, st->height = width;
  glRotatef (rot, 0, 0, 1);
  if (st->wire) glScalef(0.8, 0.8, 1);
  glTranslatef (-st->width/2, -st->height/2, 0);
# endif
}

static void free_glcells( ModeInfo *mi );

ENTRYPOINT void 
init_glcells( ModeInfo *mi )
{
  int i, divisions;
  State *st=0;
  
  MI_INIT(mi, sstate, free_glcells);
  st = &sstate[MI_SCREEN(mi)];
  
  st->glx_context = init_GL(mi);
  st->cell = 0;
  st->num_cells = 0;
  st->wire = MI_IS_WIREFRAME(mi);
  
  /* get settings */
  st->max_cells = s_maxcells;;
  if (st->max_cells < 50) st->max_cells = 50;
  if (st->max_cells > 10000) st->max_cells = 10000;
  
  st->pause = s_pause;
  if (st->pause < 0) st->pause = 0;
  if (st->pause > 400) st->pause = 400;
  st->pause_counter = st->pause;
  
  st->radius = s_radius;
  if (st->radius < 5) st->radius = 5;
  if (st->radius > 200) st->radius = 200;
  
  divisions = s_quality;
  if (divisions < 0) divisions = 0;
  if (divisions > 5) divisions = 5;
  
  st->num_seeds = s_seeds;
  if (st->num_seeds < 1) st->num_seeds = 1;
  if (st->num_seeds > 16) st->num_seeds = 16;
  
  st->minfood = s_minfood;
  if (st->minfood < 0) st->minfood = 0;
  if (st->minfood > 1000) st->minfood = 1000;
  
  st->maxfood = s_maxfood;
  if (st->maxfood < 0) st->maxfood = 0;
  if (st->maxfood > 1000) st->maxfood = 1000;
  
  if (st->maxfood < st->minfood) st->maxfood = st->minfood+1;
  
  st->keep_old_cells = s_keepold;
  
  st->divide_age = s_divideage;
  if (st->divide_age < 1) st->divide_age = 1;
  if (st->divide_age > 1000) st->divide_age = 1000;
     
  st->move_dist = s_min_dist;
  if (st->move_dist < 1.0) st->move_dist = 1.0;
  if (st->move_dist > 3.0) st->move_dist = 3.0;
  st->move_dist *= st->radius;
  
  for (i=0; i<NUM_CELL_SHAPES; ++i) st->cell_list[i] = -1;
  st->nucleus_list = -1;
  st->food = 0;
  
  st->sphere = create_sphere( st, divisions );
  st->disturbance = 
      (double *) malloc( st->sphere->num_vertex*sizeof(double) );
  for (i=0; i<st->sphere->num_vertex; ++i) {
    st->disturbance[i] = 
        0.05-((double)random()/(double)RAND_MAX*0.1);
  }
  
  create_nucleus_texture( st );

  reshape_glcells (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
}

ENTRYPOINT void
draw_glcells( ModeInfo *mi )
{
  State *st = &sstate[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  
  if (!st->glx_context) return;

  glXMakeCurrent( MI_DISPLAY(mi), MI_WINDOW(mi), 
                  *(st->glx_context) );
  
  mi->polygon_count = render( st );

  if (mi->fps_p) do_fps (mi);
  
  glFinish();
  glXSwapBuffers( dpy, window );
}

static void
free_glcells( ModeInfo *mi )
{
  int i;
  State *st  = &sstate[MI_SCREEN(mi)];

  if (st->glx_context) {
    glXMakeCurrent( MI_DISPLAY(mi), MI_WINDOW(mi),
                    *(st->glx_context) );
  
    /* nuke everything before exit */
    if (st->sphere) free_Object( st->sphere );
    if (st->food)   free( st->food );
    for (i=0; i<NUM_CELL_SHAPES; ++i) {
      if (st->cell_list[i] != -1) {
        glDeleteLists( st->cell_list[i], 1 );
      }
    }
    if (st->cell) free( st->cell );
    free( st->disturbance );
    glDeleteTextures( 1, &st->texture_name );
    free( st->texture );
  }
}

XSCREENSAVER_MODULE( "GLCells", glcells )
