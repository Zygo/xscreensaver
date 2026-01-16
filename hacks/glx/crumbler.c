/* crumbler, Copyright © 2018-2025 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#define DEFAULTS	"*delay:	30000       \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \
			"*suppressRotationAnimation: True\n" \

# define release_crumbler 0

#include "xlockmore.h"
#include "colors.h"
#include "rotator.h"
#include "quickhull.h"
#include "gltrackball.h"
#include "easing.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */


#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_SPEED       "1.0"
#define DEF_DENSITY     "1.0"
#define DEF_FRACTURE    "0"

#undef RANDSIGN
#define RANDSIGN() ((random() & 1) ? 1 : -1)

typedef struct {
  qh_vertex_t *verts;		/* interior point cloud */
  int nverts, onverts;
  qh_vertex_t min, max;		/* enclosing box */
  qh_vertex_t mid, vec;
  int polygon_count;
  GLuint dlist;
  int color;
  int color_shift;
} chunk;

typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  trackball_state *trackball;
  enum { IDLE, SPLIT, PAUSE, FLEE, ZOOM } state;
  GLfloat tick;
  Bool button_down_p;
  int nchunks;
  chunk **chunks;
  chunk *ghost;

  int ncolors;
  XColor *colors;
} crumbler_configuration;

static crumbler_configuration *bps = NULL;

static Bool do_spin;
static GLfloat speed;
static GLfloat density;
static int fracture;
static Bool do_wander;

static XrmOptionDescRec opts[] = {
  { "-spin",    ".spin",    XrmoptionNoArg, "True" },
  { "+spin",    ".spin",    XrmoptionNoArg, "False" },
  { "-speed",   ".speed",   XrmoptionSepArg, 0 },
  { "-density", ".density", XrmoptionSepArg, 0 },
  { "-fracture",".fracture",XrmoptionSepArg, 0 },
  { "-wander",  ".wander",  XrmoptionNoArg, "True" },
  { "+wander",  ".wander",  XrmoptionNoArg, "False" }
};

static argtype vars[] = {
  {&do_spin,   "spin",    "Spin",    DEF_SPIN,    t_Bool},
  {&do_wander, "wander",  "Wander",  DEF_WANDER,  t_Bool},
  {&speed,     "speed",   "Speed",   DEF_SPEED,   t_Float},
  {&density,   "density", "Density", DEF_DENSITY, t_Float},
  {&fracture,  "fracture","Fracture",DEF_FRACTURE,t_Int},
};

ENTRYPOINT ModeSpecOpt crumbler_opts = {countof(opts), opts, countof(vars), vars, NULL};


/* Create a roughly spherical cloud of N random points.
 */
static void
make_point_cloud (qh_vertex_t *verts, int nverts)
{
  int i = 0;
  while (i < nverts)
    {
      verts[i].x = (0.5 - frand(1.0));
      verts[i].y = (0.5 - frand(1.0));
      verts[i].z = (0.5 - frand(1.0));
      if ((verts[i].x * verts[i].x +
           verts[i].y * verts[i].y +
           verts[i].z * verts[i].z)
          < 0.25)
        i++;
    }
}

static void crumbler_oom (void)
{
# ifdef HAVE_JWXYZ
  jwxyz_abort ("%s: out of memory, try reducing 'density'", progname);
# else
  fprintf (stderr, "%s: out of memory, try reducing 'density'\n", progname);
  exit (1);
# endif
}


static chunk *
make_chunk (void)
{
  chunk *c = (chunk *) calloc (1, sizeof(*c));
  if (!c) crumbler_oom();
  c->dlist = glGenLists (1);
  c->color_shift = 1 + (random() % 3) * RANDSIGN();
  return c;
}

/* Return False if out of memory */
static Bool
render_chunk (ModeInfo *mi, chunk *c)
{
  int wire = MI_IS_WIREFRAME(mi);
  int i, j;
  qh_mesh_t m;
  GLfloat d;

  if (c->nverts <= 3)
    {
      fprintf (stderr, "%s: nverts %d\n", progname, c->nverts);
      abort();
    }

  c->polygon_count = 0;
  c->min.x = c->min.y = c->min.z =  999999;
  c->max.x = c->max.y = c->max.z = -999999;

  for (i = 0; i < c->nverts; i++)
    {
      if (c->verts[i].x < c->min.x) c->min.x = c->verts[i].x;
      if (c->verts[i].y < c->min.y) c->min.y = c->verts[i].y;
      if (c->verts[i].z < c->min.z) c->min.z = c->verts[i].z;
      if (c->verts[i].x > c->max.x) c->max.x = c->verts[i].x;
      if (c->verts[i].y > c->max.y) c->max.y = c->verts[i].y;
      if (c->verts[i].z > c->max.z) c->max.z = c->verts[i].z;
    }

  c->mid.x = (c->max.x + c->min.x) / 2;
  c->mid.y = (c->max.y + c->min.y) / 2;
  c->mid.z = (c->max.z + c->min.z) / 2;

  /* midpoint as normalized vector from origin */
  d = sqrt (c->mid.x * c->mid.x +
            c->mid.y * c->mid.y +
            c->mid.z * c->mid.z);
  c->vec.x = c->mid.x / d;
  c->vec.y = c->mid.y / d;
  c->vec.z = c->mid.z / d;

  if (c->nverts <= 3)
    {
      fprintf (stderr, "%s: nverts %d\n", progname, c->nverts);
      abort();
    }

  m = qh_quickhull3d (c->verts, c->nverts);

  if (!m.vertices)  /* out of memory */
    {
      qh_free_mesh (m);
      return False;
    }

  glNewList (c->dlist, GL_COMPILE);
  if (! wire) glBegin (GL_TRIANGLES);
  for (i = 0, j = 0; i < m.nindices; i += 3, j++)
    {
      qh_vertex_t *v0 = &m.vertices[m.indices[i+0]];
      qh_vertex_t *v1 = &m.vertices[m.indices[i+1]];
      qh_vertex_t *v2 = &m.vertices[m.indices[i+2]];
      qh_vec3_t   *n  = &m.normals[m.normalindices[j]];

      if (i+2 >= m.nindices) abort();
      if (j >= m.nnormals) abort();

      glNormal3f (n->x, n->y, n->z);
      if (wire) glBegin(GL_LINE_LOOP);
      glVertex3f (v0->x, v0->y, v0->z);
      glVertex3f (v1->x, v1->y, v1->z);
      glVertex3f (v2->x, v2->y, v2->z);
      if (wire) glEnd();
      c->polygon_count++;
    }
  if (! wire) glEnd();

  if (wire)
    {
      glPointSize (1);
      glColor3f (0, 1, 0);
      glBegin (GL_POINTS);
      for (i = 0; i < c->nverts; i++)
        {
          if (i > 0 && i == c->onverts)
            {
              glEnd();
              glColor3f (1, 0, 0);
              glBegin (GL_POINTS);
            }
          glVertex3f (c->verts[i].x, c->verts[i].y, c->verts[i].z);
        }
      glEnd();
    }

  glEndList();

  qh_free_mesh (m);
  return True;
}


static void
free_chunk (chunk *c)
{
  if (c->dlist)
    glDeleteLists (c->dlist, 1);
  free (c->verts);
  free (c);
}


/* Make sure the chunk contains at least N points.
   As we subdivide, the number of points is reduced.
   This adds new points to the interior that do not
   affect the shape of the outer hull.
 */
static void
pad_chunk (chunk *c, int min)
{
  /* Allocate a new array of size N
     Copy the old points into it
     while size < N
       pick two random points
       add a point that is somewhere along the line between them
       (that point will still be inside the old hull)
   */
  qh_vertex_t *verts;
  int i;
  if (c->nverts >= min) return;
  if (c->nverts <= 3) abort();
  verts = (qh_vertex_t *) calloc (min, sizeof(*verts));
  if (!verts) crumbler_oom();
  memcpy (verts, c->verts, c->nverts * sizeof(*verts));
  i = c->nverts;
  while (i < min)
    {
      qh_vertex_t v;
      int j0, j1;
      GLfloat r;
      j0 = random() % c->nverts;
      do {
        j1 = random() % c->nverts;
      } while (j0 == j1);

      r = 0.2 + frand(0.6);
# undef R
# define R(F) v.F = c->verts[j0].F + \
                    r * (fabs (c->verts[j1].F - c->verts[j0].F)) \
                    * (c->verts[j0].F > c->verts[j1].F ? -1 : 1)
      R(x);
      R(y);
      R(z);
# undef R

      /* Sometimes quickhull.c is giving us concave and un-closed polygons.
         Maybe it gets confused if there are duplicate points?  So reject
         this point if it is within epsilon of any earlier point.
       */
# if 0 /* Nope, that's not it. */
      {
        Bool ok = True;
        int j;
        for (j = 0; j < i; j++)
          {
            
            double X = c->verts[j].x - v.x;
            double Y = c->verts[j].y - v.y;
            double Z = c->verts[j].z - v.z;
            double d2 = X*X + Y*Y + Z*Z;
            if (d2 < 0.0001)
              {
                /* fprintf (stderr, "## REJ %f\n",d2); */
                ok = False;
                break;
              }
          }
        if (! ok) continue;
      }
# endif

      verts[i++] = v;
    }

#if 0
  fprintf (stdout, " int n = %d;\n", min);
  fprintf (stdout, " qh_vertex_t v[] = {");
  for (i = 0; i < min; i++)
    fprintf(stdout,"{%f,%f,%f},", verts[i].x, verts[i].y, verts[i].z);
  fprintf (stdout, "};\n\n");
#endif

  free (c->verts);
  c->verts = verts;
  c->onverts = c->nverts;
  c->nverts = min;

#if 0
  qh_vertex_t *verts2 = (qh_vertex_t *) calloc (n, sizeof(*verts2));
  if (!verts2) crumbler_oom();
  memcpy (verts2, v, n * sizeof(*verts2));
  free (c->verts);
  c->verts = verts2;
  c->onverts = 0;
  c->nverts = n;
#endif
}


/* Returns a list of N new chunks.
 */
static chunk **
split_chunk (ModeInfo *mi, chunk *c, int nchunks)
{
  /* Pick N key-points from the cloud.
     Create N new chunks.
     For each old point:
       It goes in chunk N if it is closest to key-point N.
     Free old chunk.
     for each new chunk
       render_chunk
   */
  crumbler_configuration *bp = &bps[MI_SCREEN(mi)];
  chunk **chunks;
  int *keys;
  int i, j;
  int retries = 0;

 RETRY:
  chunks = (chunk **) calloc (nchunks, sizeof(*chunks));
  if (!chunks) crumbler_oom();
  keys = (int *) calloc (nchunks, sizeof(*keys));
  if (!keys) crumbler_oom();

  for (i = 0; i < nchunks; i++)
    {
      /* Fill keys with random numbers that are not duplicates. */
      Bool ok = True;
      chunk *c2 = 0;
      if (nchunks >= c->nverts)
        {
          fprintf (stderr, "%s: nverts %d nchunks %d\n", progname,
                   c->nverts, nchunks);
          abort();
        }
      do {
        keys[i] = random() % c->nverts;
        ok = True;
        for (j = 0; j < i; j++)
          if (keys[i] == keys[j])
            {
              ok = False;
              break;
            }
      } while (!ok);

      c2 = make_chunk();
      chunks[i] = c2;
      chunks[i]->nverts = 0;
      c2->verts = (qh_vertex_t *) calloc (c->nverts, sizeof(*c2->verts));
      if (!c2->verts) crumbler_oom();
      c2->color = (c->color + (random() % (1 + (bp->ncolors / 3))))
                   % bp->ncolors;
    }

  /* Add the verts to the approprate chunks
   */
  for (i = 0; i < c->nverts; i++)
    {
      qh_vertex_t *v0 = &c->verts[i];
      int target_chunk = -1;
      double target_d2 = 9999999;
      chunk *c2 = 0;

      for (j = 0; j < nchunks; j++)
        {
          qh_vertex_t *v1 = &c->verts[keys[j]];
          double X = v1->x - v0->x;
          double Y = v1->y - v0->y;
          double Z = v1->z - v0->z;
          double d2 = X*X + Y*Y + Z*Z;
          if (d2 < target_d2)
            {
              target_d2 = d2;
              target_chunk = j;
            }
        }
      if (target_chunk == -1) abort();

      c2 = chunks[target_chunk];
      c2->verts[c2->nverts++] = *v0;
      if (c2->nverts > c->nverts) abort();
    }

  free (keys);
  keys = 0;

  for (i = 0; i < nchunks; i++)
    {
      chunk *c2 = chunks[i];

      /* It is possible that the keys we have chosen have resulted in one or
         more cells that have 3 or fewer points in them. If that's the case,
         re-randomize.
       */
      if (c2->nverts <= 3)
        {
          for (j = 0; j < nchunks; j++)
            free_chunk (chunks[j]);
          free (chunks);
          chunks = 0;
          if (retries++ > 100)
            {
              fprintf(stderr, "%s: unsplittable\n", progname);
              abort();
            }
          goto RETRY;
        }

      if (i == 0)  /* The one we're gonna keep */
        pad_chunk (c2, c->nverts);
      if (! render_chunk (mi, c2))
        crumbler_oom();   /* We are too far in to recover from this */
    }

  return chunks;
}


static void
tick_crumbler (ModeInfo *mi)
{
  crumbler_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat ts;

  if (bp->button_down_p) return;

  switch (bp->state) {
  case IDLE:  ts = 0.02;  break;
  case SPLIT: ts = 0.01;  break;
  case PAUSE: ts = 0.008; break;
  case FLEE:  ts = 0.005; break;
  case ZOOM:  ts = 0.03;  break;
  default:    abort();    break;
  }

  bp->tick += ts * speed;

  if (bp->tick < 1) return;

  bp->tick = 0;
  bp->state = (bp->state + 1) % (ZOOM + 1);

  switch (bp->state) {
  case IDLE:
    {
      chunk *c = bp->chunks[0];
      int i;

      /* We already animated it zooming to full size. Now make it real. */
      GLfloat X = (c->max.x - c->min.x);
      GLfloat Y = (c->max.y - c->min.y);
      GLfloat Z = (c->max.z - c->min.z);
      GLfloat s = 1 / MAX(X, MAX(Y, Z));

      for (i = 0; i < c->nverts; i++)
        {
          c->verts[i].x *= s;
          c->verts[i].y *= s;
          c->verts[i].z *= s;
        }

      /* Re-render it to move the verts in the display list too.
         This also recomputes min, max and mid.
       */
      if (! render_chunk (mi, c))
        crumbler_oom();   /* We are too far in to recover from this */
      break;
    }

  case SPLIT:
    {
      chunk *c = bp->chunks[0];
      int frac = (fracture >= 2 ? fracture : 2 + (2 * (random() % 5)));
      chunk **chunks = split_chunk (mi, c, frac);
      if (bp->nchunks != 1) abort();
      if (bp->ghost) abort();
      bp->ghost = c;
      free (bp->chunks);
      bp->chunks = chunks;
      bp->nchunks = frac;
      break;
    }

  case PAUSE:
    break;

  case FLEE:
    if (bp->ghost) free_chunk (bp->ghost);
    bp->ghost = 0;
    break;

  case ZOOM:
    {
      chunk *c = bp->chunks[0];
      int i;
      for (i = 1; i < bp->nchunks; i++)
        free_chunk (bp->chunks[i]);
      bp->nchunks = 1;

      /* We already animated the remaining chunk moving toward the origin.
         Make it real.
      */
      for (i = 0; i < c->nverts; i++)
        {
          c->verts[i].x -= c->mid.x;
          c->verts[i].y -= c->mid.y;
          c->verts[i].z -= c->mid.z;
        }

      /* Re-render it to move the verts in the display list too.
         This also recomputes min, max and mid (now 0).
       */
      if (! render_chunk (mi, c))
        crumbler_oom();   /* We are too far in to recover from this */
      break;
    }

  default: abort(); break;
  }
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_crumbler (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;
  int y = 0;

  if (width > height * 5) {   /* tiny window: show middle */
    height = width * 9/16;
    y = -height/2;
    h = height / (GLfloat) width;
  }

  glViewport (0, y, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (30.0, 1/h, 1.0, 100.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 0.0, 30.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);

  {
    GLfloat s = (MI_WIDTH(mi) < MI_HEIGHT(mi)
                 ? (MI_WIDTH(mi) / (GLfloat) MI_HEIGHT(mi))
                 : 1);
    glScalef (s, s, s);
  }

  glClear(GL_COLOR_BUFFER_BIT);
}


ENTRYPOINT Bool
crumbler_handle_event (ModeInfo *mi, XEvent *event)
{
  crumbler_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;

  return False;
}


ENTRYPOINT void 
init_crumbler (ModeInfo *mi)
{
  crumbler_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);
  int i;

  MI_INIT (mi, bps);
  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_crumbler (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  if (!wire)
    {
      GLfloat pos[4] = {1.0, 1.0, 1.0, 0.0};
      GLfloat amb[4] = {0.0, 0.0, 0.0, 1.0};
      GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
      GLfloat spc[4] = {0.0, 1.0, 1.0, 1.0};

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_CULL_FACE);

      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);

      glEnable (GL_BLEND);
      glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

  {
    double spin_speed   = 0.5 * speed;
    double spin_accel   = 0.3;
    double wander_speed = 0.01 * speed;

    bp->rot = make_rotator (do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            spin_accel,
                            do_wander ? wander_speed : 0,
                            True);
    bp->trackball = gltrackball_init (True);
  }

  bp->ncolors = 1024;
  bp->colors = (XColor *) calloc(bp->ncolors, sizeof(XColor));
  if (!bp->colors) crumbler_oom();
  make_smooth_colormap (0, 0, 0,
                        bp->colors, &bp->ncolors,
                        False, 0, False);

  /* brighter colors, please... */
  for (i = 0; i < bp->ncolors; i++)
    {
# undef R
# define R(F) F = 65535 * (0.3 + 0.7 * ((F) / 65535.0))
      R (bp->colors[i].red);
      R (bp->colors[i].green);
      R (bp->colors[i].blue);
# undef R
    }

  {
    double d2 = density;
    chunk *c;

    bp->nchunks = 1;
    bp->chunks = (chunk **) calloc (bp->nchunks, sizeof(*bp->chunks));
    if (! bp->chunks) crumbler_oom();

    c = make_chunk();
    if (! c) crumbler_oom();

    bp->chunks[0] = c;

    while (1)
      {
        c->nverts = 4500 * d2;
        c->verts = (qh_vertex_t *) calloc (c->nverts, sizeof(*c->verts));
        if (c->verts)
          {
            make_point_cloud (c->verts, c->nverts);

            /* Let's shrink it to a point then zoom in. */
            bp->state = ZOOM;
            bp->tick = 0;
            for (i = 0; i < c->nverts; i++)
              {
                c->verts[i].x /= 500;
                c->verts[i].y /= 500;
                c->verts[i].z /= 500;
              }

            if (! render_chunk (mi, c))
              {
                free (c->verts);
                c->verts = 0;
              }
          }

        if (c->verts)
          break;

        if (d2 < 0.1)
          crumbler_oom();
        d2 *= 0.9;
      }

    if (density != d2)
      fprintf (stderr,
               "%s: out of memory: reduced density from %.01f to %0.1f\n",
               progname, density, d2);
  }
}


static void
draw_chunk (ModeInfo *mi, chunk *c, GLfloat alpha)
{
  crumbler_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat color[4];

  color[0] = bp->colors[c->color].red   / 65536.0;
  color[1] = bp->colors[c->color].green / 65536.0;
  color[2] = bp->colors[c->color].blue  / 65536.0;
  color[3] = alpha;
  glColor4fv (color);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);

  c->color += c->color_shift;
  while (c->color < 0) c->color += bp->ncolors;
  while (c->color >= bp->ncolors) c->color -= bp->ncolors;

  glCallList (c->dlist);
  mi->polygon_count += c->polygon_count;
}


ENTRYPOINT void
draw_crumbler (ModeInfo *mi)
{
  int wire = MI_IS_WIREFRAME(mi);
  crumbler_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  GLfloat alpha = 1;
  int i;

  static const GLfloat bspec[4]  = {1.0, 1.0, 1.0, 1.0};
  static const GLfloat bshiny    = 128.0;

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  tick_crumbler (mi);

  glShadeModel(GL_SMOOTH);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  {
    double x, y, z;
    get_position (bp->rot, &x, &y, &z, !bp->button_down_p);
    glTranslatef((x - 0.5) * 8,
                 (y - 0.5) * 8,
                 (z - 0.5) * 15);

    gltrackball_rotate (bp->trackball);

    get_rotation (bp->rot, &x, &y, &z, !bp->button_down_p);
    glRotatef (x * 360, 1.0, 0.0, 0.0);
    glRotatef (y * 360, 0.0, 1.0, 0.0);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }

  mi->polygon_count = 0;

  glMaterialfv (GL_FRONT, GL_SPECULAR, bspec);
  glMateriali  (GL_FRONT, GL_SHININESS, bshiny);

  if (do_wander)
    glScalef (10, 10, 10);
  else
    glScalef (13, 13, 13);

  alpha = 1;
  for (i = 0; i < bp->nchunks; i++)
    {
      chunk *c = bp->chunks[i];

      glPushMatrix();

      switch (bp->state) {
        case FLEE:
          {
            GLfloat r = ease (EASE_IN_OUT_SINE, bp->tick);
            /* Move everybody toward the origin, so that chunk #0 ends up
               centered there. */
            glTranslatef (-r * c->mid.x,
                          -r * c->mid.y,
                          -r * c->mid.z);
            if (i != 0)
              {
                /* Move this chunk away from the center, along a vector from
                   the origin to its midpoint. */
                GLfloat d2 = r * 6;
                glTranslatef (c->vec.x * d2, c->vec.y * d2, c->vec.z * d2);
                alpha = 1 - r;
              }
          }
          break;

      case ZOOM:
        {
          chunk *c = bp->chunks[0];
          GLfloat X = (c->max.x - c->min.x);
          GLfloat Y = (c->max.y - c->min.y);
          GLfloat Z = (c->max.z - c->min.z);
          GLfloat size0 = MAX(X, MAX(Y, Z));
          GLfloat size1 = 1.0;
          GLfloat r = 1 - ease (EASE_IN_OUT_SINE, bp->tick);
          GLfloat s = 1 / (size0 + r * (size1 - size0));
          glScalef (s, s, s);
        }
        break;

      default:
        break;
      }

      draw_chunk (mi, c, alpha);
      glPopMatrix();
    }

  /* Draw the old one, fading out. */
  if (!wire && bp->state == SPLIT && bp->ghost)
    {
      GLfloat s;
      /* alpha = 1 - bp->tick; */
      alpha = 1;
      /* s = 0.7 + (0.3 * ease (EASE_IN_OUT_SINE, 1-bp->tick)); */
      s = 2 * ease (EASE_IN_OUT_SINE, (1-bp->tick) / 2);
      s *= 1.01;
      glScalef (s, s, s);
      draw_chunk (mi, bp->ghost, alpha);
    }

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_crumbler (ModeInfo *mi)
{
  crumbler_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  if (bp->trackball) gltrackball_free (bp->trackball);
  if (bp->rot) free_rotator (bp->rot);
  if (bp->colors) free (bp->colors);
  for (i = 0; i < bp->nchunks; i++)
    free_chunk (bp->chunks[i]);
  if (bp->chunks) free (bp->chunks);
}


XSCREENSAVER_MODULE ("Crumbler", crumbler)

#endif /* USE_GL */
