/* hilbert, Copyright (c) 2011-2014 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * 2D and 3D Hilbert space-filling curves.
 *
 * Inspired by "Visualizing Hilbert Curves" by Nelson Max, 1998:
 * https://e-reports-ext.llnl.gov/pdf/234149.pdf
 */

#define DEFAULTS	"*delay:	30000       \n" \
			"*count:        30          \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \
			"*geometry:	800x800\n" \
			"*suppressRotationAnimation: True\n" \

# define release_hilbert 0

#include "xlockmore.h"
#include "colors.h"
#include "sphere.h"
#include "tube.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */


#define DEF_SPIN        "True"
#define DEF_WANDER      "False"
#define DEF_SPEED       "1.0"
#define DEF_MODE        "random"
#define DEF_ENDS        "random"
#define DEF_MAX_DEPTH   "5"
#define DEF_THICKNESS   "0.25"

#define PAUSE_TICKS 180

typedef struct {
  double x,y,z;
} XYZ;

typedef struct {
  unsigned short x,y,z;
} XYZb;

typedef struct {
  int size;
  XYZb *values;		/* assume max depth of 20 (2^16 on each side) */
} hilbert_cache;


static int dlist_faces[] = { 20, 10, 8, 4, 3 };


typedef struct {
  GLXContext *glx_context;
  rotator *rot, *rot2;
  trackball_state *trackball;
  Bool button_down_p;
  Bool twodee_p;
  Bool closed_p;
  int ncolors;
  XColor *colors;

  int depth;
  int depth_tick;

  GLfloat path_start, path_end;
  int path_tick;
  int pause;
  GLfloat diam;

  hilbert_cache **caches;	/* filled lazily */

  GLuint dlists   [40][2];	/* we don't actually alloc all of these */
  int dlist_polys [40][2];

} hilbert_configuration;

static hilbert_configuration *bps = NULL;

static Bool do_spin;
static GLfloat speed;
static Bool do_wander;
static char *mode_str;
static char *ends_str;
static int max_depth;
static GLfloat thickness;

static XrmOptionDescRec opts[] = {
  { "-spin",      ".spin",     XrmoptionNoArg, "True"   },
  { "+spin",      ".spin",     XrmoptionNoArg, "False"  },
  { "-speed",     ".speed",    XrmoptionSepArg, 0       },
  { "-wander",    ".wander",   XrmoptionNoArg, "True"   },
  { "+wander",    ".wander",   XrmoptionNoArg, "False"  },
  { "-mode",      ".mode",     XrmoptionSepArg, 0       },
  { "-2d",        ".mode",     XrmoptionNoArg, "2D"     },
  { "-3d",        ".mode",     XrmoptionNoArg, "3D"     },
  { "-ends",      ".ends",     XrmoptionSepArg, 0       },
  { "-closed",    ".ends",     XrmoptionNoArg, "closed" },
  { "-open",      ".ends",     XrmoptionNoArg, "open"   },
  { "-max-depth", ".maxDepth", XrmoptionSepArg, 0       },
  { "-thickness", ".thickness",XrmoptionSepArg, 0       },
};

static argtype vars[] = {
  {&do_spin,   "spin",     "Spin",     DEF_SPIN,      t_Bool},
  {&do_wander, "wander",   "Wander",   DEF_WANDER,    t_Bool},
  {&speed,     "speed",    "Speed",    DEF_SPEED,     t_Float},
  {&mode_str,  "mode",     "Mode",     DEF_MODE,      t_String},
  {&ends_str,  "ends",     "Ends",     DEF_ENDS,      t_String},
  {&max_depth, "maxDepth", "MaxDepth", DEF_MAX_DEPTH, t_Int},
  {&thickness, "thickness","Thickness",DEF_THICKNESS, t_Float},
};

ENTRYPOINT ModeSpecOpt hilbert_opts = {countof(opts), opts, countof(vars), vars, NULL};


/* 2D Hilbert, and closed-loop variant.
 */

/* These arrays specify which bits of the T index contribute to the
   X, Y and Z values.  Higher order bits are defined recursively.
 */
static const int xbit2d[4] = { 0, 0, 1, 1 };
static const int ybit2d[4] = { 0, 1, 1, 0 };

static const int xbit3d[8] = { 0,0,0,0,1,1,1,1 };
static const int ybit3d[8] = { 0,1,1,0,0,1,1,0 };
static const int zbit3d[8] = { 0,0,1,1,1,1,0,0 };

/* These matrixes encapsulate the necessary reflection and translation
   of each 4 sub-squares or 8 sub-cubes.  The r2d and r3d arrays are
   the normal Hilbert descent, and the s2d and s3d arrays are the 
   modified curve used for only level 0 of a closed-form path.
 */

static int r2d[4][2][2] = {
  /*      _    _
         | |..| |
         :_    _:
          _|  |_

  */       
  {{ 0, 1},
   { 1, 0}},
  {{ 1, 0},
   { 0, 1}},
  {{ 1, 0},
   { 0, 1}},
  {{ 0,-1},
   {-1, 0}},
};

static int s2d[4][2][2] = {
  /*      __    __
         |  |..|  |	Used for outermost iteration only, in "closed" mode
         :   ..   :
         |__|  |__|

  */       
  {{-1, 0},
   { 0,-1}},
  {{ 1, 0},
   { 0, 1}},
  {{ 1, 0},
   { 0, 1}},
  {{-1, 0},
   { 0,-1}},
};


static int r3d[8][3][3] = {
  /*
          /|      /|
         / |     / |
        /__|____/  |
           |       |
          /       /
         /       /
  */       
 {{ 0, 1, 0},
  { 1, 0, 0},
  { 0, 0, 1}},
 {{ 0, 0, 1},
  { 0, 1, 0},
  { 1, 0, 0}},
 {{ 1, 0, 0},
  { 0, 1, 0},
  { 0, 0, 1}},
 {{ 0, 0,-1},
  {-1, 0, 0},
  { 0, 1, 0}},
 {{ 0, 0, 1},
  { 1, 0, 0},
  { 0, 1, 0}},
 {{ 1, 0, 0},
  { 0, 1, 0},
  { 0, 0, 1}},
 {{ 0, 0,-1},
  { 0, 1, 0},
  {-1, 0, 0}},
 {{ 0,-1, 0},
  {-1, 0, 0},
  { 0, 0, 1}},
};


static int s3d[8][3][3] = {
  /*
          /|      /|	Used for outermost iteration only, in "closed" mode
         / |     / |
        /__|____/  |
           |       |
          /       /
         /_______/
  */       
 {{-1, 0, 0},
  { 0, 0,-1},
  { 0, 1, 0}},
 {{ 0, 0, 1},
  { 0, 1, 0},
  { 1, 0, 0}},
 {{ 1, 0, 0},
  { 0, 1, 0},
  { 0, 0, 1}},
 {{ 0, 0,-1},
  {-1, 0, 0},
  { 0, 1, 0}},
 {{ 0, 0, 1},
  { 1, 0, 0},
  { 0, 1, 0}},
 {{ 1, 0, 0},
  { 0, 1, 0},
  { 0, 0, 1}},
 {{ 0, 0,-1},
  { 0, 1, 0},
  {-1, 0, 0}},

 {{-1, 0, 0},
  { 0, 0,-1},
  { 0, 1, 0}},
};



/* Matrix utilities
 */

static void 
matrix_times_vector2d (int m[2][2], int v[2], int dest[2])
{
  dest[0] = m[0][0]*v[0] + m[0][1]*v[1];
  dest[1] = m[1][0]*v[0] + m[1][1]*v[1];
}

static void
matrix_times_vector3d (int m[3][3], int v[3], int dest[3])
{
  dest[0] = m[0][0]*v[0] + m[0][1]*v[1] + m[0][2]*v[2];
  dest[1] = m[1][0]*v[0] + m[1][1]*v[1] + m[1][2]*v[2];
  dest[2] = m[2][0]*v[0] + m[2][1]*v[1] + m[2][2]*v[2];
}


static void
matrix_multiply2d (int m1[2][2], int m2[2][2], int dest[2][2])
{
  dest[0][0] = m1[0][0] * m2[0][0] + m1[0][1] * m2[1][0];
  dest[0][1] = m1[0][0] * m2[0][1] + m1[0][1] * m2[1][1];
  dest[1][0] = m1[1][0] * m2[0][0] + m1[1][1] * m2[1][0];
  dest[1][1] = m1[1][0] * m2[0][1] + m1[1][1] * m2[1][1];
}

static void
matrix_multiply3d (int m1[3][3], int m2[3][3], int dest[3][3])
{
  int i, j, k;
  for (i = 0; i < 3; i++) 
    for (j = 0; j < 3; j++)
      {
        dest[i][j] = 0;
        for (k = 0; k < 3; k++)
          dest[i][j] += m1[i][k] * m2[k][j];
      }
}

static void
identity_matrix2d (int m[2][2])
{
  m[0][0] = m[1][1] = 1;
  m[0][1] = m[1][0] = 0;
}

static void
identity_matrix3d (int m[3][3])
{
  m[0][0] = m[1][1] = m[2][2] = 1;
  m[0][1] = m[0][2] = m[1][0] = m[1][2] = m[2][0] = m[2][1] = 0;
}


static void
matrix_copy2d (int src[2][2], int dest[2][2])
{
  int i, j;
  for (i = 0; i < 2; i++) 
    for (j = 0; j < 2; j++)
      dest[i][j] = src[i][j];
}


static void
matrix_copy3d (int src[3][3], int dest[3][3])
{
  int i, j;
  for (i = 0; i < 3; i++) 
    for (j = 0; j < 3; j++)
      dest[i][j] = src[i][j];
}


/* 2D and 3D Hilbert:
   Given an index T along the curve, return the XY or XYZ coordinates.
   N is depth of the curve.
 */

static void
t_to_xy (int n, int t, int *x, int *y, Bool closed_p)
{
  int k, rt[2][2], rq[2][2], va[2], vb[2];
  identity_matrix2d(rt);
  *x = *y = 0;

  for (k = n-1; k >= 0; k--)
    {
      int j = 3 & (t >> (2*k));
      va[0] = 2 * xbit2d[j] - 1;
      va[1] = 2 * ybit2d[j] - 1;
      matrix_times_vector2d (rt, va, vb);
      *x += ((vb[0] + 1) / 2) << k;
      *y += ((vb[1] + 1) / 2) << k;
      if (k > 0)
        {
          matrix_copy2d (rt, rq);
          if (k == n-1 && closed_p)
            matrix_multiply2d (rq, s2d[j], rt);
          else
            matrix_multiply2d (rq, r2d[j], rt);
        }
    }
}


static void
t_to_xyz (int n, int t, int *x, int *y, int *z, Bool closed_p)
{
  int k, rt[3][3], rq[3][3], va[3], vb[3];
  identity_matrix3d(rt);
  *x = *y = *z = 0; 

  for (k = n-1; k >= 0; k--)
    {
      int j = 7 & (t >> (3*k));
      va[0] = 2 * xbit3d[j] - 1;
      va[1] = 2 * ybit3d[j] - 1;
      va[2] = 2 * zbit3d[j] - 1;
      matrix_times_vector3d (rt, va, vb);
      *x += ((vb[0] + 1) / 2) << k;
      *y += ((vb[1] + 1) / 2) << k;
      *z += ((vb[2] + 1) / 2) << k;
      if (k > 0)
        {
          matrix_copy3d (rt, rq);
          if (k == n-1 && closed_p)
            matrix_multiply3d (rq, s3d[j], rt);
          else
            matrix_multiply3d (rq, r3d[j], rt);
        }
    }
}


/* Rendering the curve
 */


/* Draw a sphere at the intersection of two tubes, to round off
   the connection between them.  Use one of our cache display lists
   of unit spheres in various sizes.
 */
static int
draw_joint (ModeInfo *mi, XYZ p, GLfloat diam, int faces, int wire)
{
  hilbert_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  diam *= 0.99;  /* try to clean up the edges a bit */

  if (faces <= 4) return 0;  /* too small to see */

  glPushMatrix();
  glTranslatef (p.x, p.y, p.z);
  glScalef (diam, diam, diam);

  /*  i = unit_sphere (faces, faces, wire);*/

  /* if (!bp->dlists[faces][0]) abort(); */
  /* if (!bp->dlist_polys[faces][0]) abort(); */

  glCallList (bp->dlists[faces][0]);
  i = bp->dlist_polys[faces][0];
  glPopMatrix();
  return i;
}


/* Draw a tube, and associated end caps.  Use one of our cache display lists
   of unit tubes in various sizes.  Pick the resolution of the tube based
   on how large it's being drawn.  It's ok to get chunkier if the thing is
   only a few pixels wide on the screen.
 */
static Bool
draw_segment (ModeInfo *mi,
              XYZ p0, XYZ p1,		/* from, to */
              int t, int end_t,		/* value and range */
              GLfloat path_start, GLfloat path_end,	/* clipping */
              Bool head_cap_p,
              int *last_colorP)
{
  hilbert_configuration *bp = &bps[MI_SCREEN(mi)];

  double t0 = (double) (t-1) / end_t;	/* ratio of p[01] along curve */
  double t1 = (double) t / end_t;

  int wire = MI_IS_WIREFRAME(mi);
  int owire = wire;
  GLfloat dd = bp->diam;
  int faces;

  if (path_start >= t1)  /* whole segment is before clipping region */
    return False;
  if (path_end < t0)     /* whole segment is after clipping region */
    return False;


  if (bp->twodee_p) dd *= 2;   /* more polys in 2D mode */

  faces = (dd > 0.040 ? dlist_faces[0] :
           dd > 0.020 ? dlist_faces[1] :
           dd > 0.010 ? dlist_faces[2] :
           dd > 0.005 ? dlist_faces[3] :
           dd > 0.002 ? dlist_faces[4] :
           1);

  /* In 2d mode, we can drop into wireframe mode and it still looks ok... */
  if (faces == 1)
    {
      if (bp->twodee_p)
        wire = True;
      else
        faces = 3;
    }

  if (wire && !owire)
    {
      glDisable (GL_DEPTH_TEST);
      glDisable (GL_CULL_FACE);
      glDisable (GL_LIGHTING);
    }

  if (t0 < path_start)
    {
      XYZ p;
      GLfloat seg_range = t1 - t0;
      GLfloat clip_range = path_start - t0;
      GLfloat ratio = clip_range / seg_range;
      p.x = p0.x + ((p1.x - p0.x) * ratio);
      p.y = p0.y + ((p1.y - p0.y) * ratio);
      p.z = p0.z + ((p1.z - p0.z) * ratio);
      p0 = p;
    }

  if (t1 > path_end)
    {
      XYZ p;
      GLfloat seg_range = t1 - t0;
      GLfloat clip_range = path_end - t0;
      GLfloat ratio = clip_range / seg_range;
      p.x = p0.x + ((p1.x - p0.x) * ratio);
      p.y = p0.y + ((p1.y - p0.y) * ratio);
      p.z = p0.z + ((p1.z - p0.z) * ratio);
      p1 = p;
    }

  if (p0.x == p1.x &&
      p0.y == p1.y &&
      p0.z == p1.z)
    return False;

  {
    int segs = bp->ncolors * (t1 - t0);
    int i;
    XYZ p0b, p1b;

    if (segs < 1) segs = 1;

    for (i = 0; i < segs; i++)
      {
        int j = i + 1;
        GLfloat color[4] = {0.0, 0.0, 0.0, 1.0};
        GLfloat t0b;
        int c;

        p0b.x = p0.x + i * ((p1.x - p0.x) / segs);
        p0b.y = p0.y + i * ((p1.y - p0.y) / segs);
        p0b.z = p0.z + i * ((p1.z - p0.z) / segs);

        p1b.x = p0.x + j * ((p1.x - p0.x) / segs);
        p1b.y = p0.y + j * ((p1.y - p0.y) / segs);
        p1b.z = p0.z + j * ((p1.z - p0.z) / segs);



        /* #### this isn't quite right */
        t0b = t0 + i * (t1 - t0) / segs;

        c = bp->ncolors * t0b;
        if (c >= bp->ncolors) c = bp->ncolors-1;

        /* Above depth 6, was spending 5% of the time in glMateralfv,
           so only set the color if it's different. */

        if (c != *last_colorP)
          {
            color[0] = bp->colors[c].red   / 65536.0;
            color[1] = bp->colors[c].green / 65536.0;
            color[2] = bp->colors[c].blue  / 65536.0;
            if (wire)
              glColor3fv (color);
            else
              glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);
            *last_colorP = c;
          }


        if (wire)
          {
            glBegin (GL_LINES);
            glVertex3f (p0b.x, p0b.y, p0b.z);
            glVertex3f (p1b.x, p1b.y, p1b.z);
            glEnd ();
            mi->polygon_count++;
          }
        else
          {
            /* mi->polygon_count += tube (p0b.x, p0b.y, p0b.z,
                                          p1b.x, p1b.y, p1b.z,
                                          bp->diam, 0, faces, True,
                                          0, wire);
             */

            /* Render a rotated and scaled prefab unit tube.

               There's probably a slightly more concise way to do this,
               but since they're all at right angles at least we don't
               have to use atan().
             */
            GLfloat s;
            glPushMatrix();
            glTranslatef (p0b.x, p0b.y, p0b.z);

            if (p1b.x > p0b.x)
              {
                s = p1b.x - p0b.x;
              }
            else if (p1b.x < p0b.x)
              {
                glRotatef (180, 0, 0, 1);
                s = p0b.x - p1b.x;
              }
            else if (p1b.y > p0b.y) {
                glRotatef (90, 0, 0, 1);
                s = p1b.y - p0b.y;
              }
            else if (p1b.y < p0b.y)
              {
                glRotatef (-90, 0, 0, 1);
                s = p0b.y - p1b.y;
              }
            else if (p1b.z > p0b.z)
              {
                glRotatef (-90, 0, 1, 0);
                s = p1b.z - p0b.z;
              }
            else /* (p1b.z < p0b.z) */
              {
                glRotatef (90, 0, 1, 0);
                s = p0b.z - p1b.z;
              }

            glScalef (s, bp->diam, bp->diam);
            glCallList (bp->dlists[faces][1]);
            mi->polygon_count += bp->dlist_polys[faces][1];
            glPopMatrix();


            /* If this is the bleeding edge, cap it too. */
            if (head_cap_p) {
              mi->polygon_count += draw_joint (mi, p0b, bp->diam, faces, wire);
              head_cap_p = False;
            }
          }
      }

    /* If the path is closed, close it.  This segment doesn't animate
       like the others, but, oh well.
     */
    if (! wire)
      mi->polygon_count += draw_joint (mi, p1b, bp->diam, faces, wire);
  }

  return True;
}


static void
mem(void)
{
  fprintf (stderr, "%s: out of memory\n", progname);
  exit (1);
}


/* Render the whole thing, but omit segments that lie outside of
   the path_start and path_end ratios.
 */
static void
hilbert (ModeInfo *mi, int size, GLfloat path_start, GLfloat path_end)
{
  hilbert_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);

  GLfloat w = pow(2, size);
  int end_t = (bp->twodee_p ? w * w : w * w * w);

  XYZ prev = { 0, };
  XYZ first = { 0, };
  Bool first_p = False;
  Bool any_p = False;
  int t;
  Bool fill_cache_p = False;
  hilbert_cache *cc;
  int last_color = -1;

  /* Do this here since at higher resolutions, we turn wireframe on
     and off. */

  if (! wire)
    {
      glEnable (GL_NORMALIZE);
      glEnable (GL_DEPTH_TEST);
      glEnable (GL_CULL_FACE);
      glEnable (GL_LIGHTING);
      glEnable (GL_POLYGON_OFFSET_FILL);
    }


  if (!bp->caches)
    {
      bp->caches = (hilbert_cache **)
        calloc (max_depth + 2, sizeof (*bp->caches));
      if (!bp->caches) mem();
      fill_cache_p = True;
    }

  cc = bp->caches[size];
  if (! cc)
    {
      cc = (hilbert_cache *) calloc (1, sizeof (*cc));
      cc->values = (XYZb *) calloc (end_t + 1, sizeof (*cc->values));
      if (!cc->values) mem();
      cc->size = end_t;
      bp->caches[size] = cc;
      fill_cache_p = True;
    }

  for (t = 0; t < end_t; t++)
    {
      int x, y, z;
      XYZ c;
      XYZb *cb;

      if (!fill_cache_p)
        {
          cb = &cc->values[t];
          x = cb->x;
          y = cb->y;
          z = cb->z;
        }
      else
        {
          if (!bp->twodee_p)
            t_to_xyz (size, t, &x, &y, &z, bp->closed_p);
          else
            {
              t_to_xy (size, t, &x, &y, bp->closed_p);
              z = w/2;
            }
          cb = &cc->values[t];
          cb->x = x;
          cb->y = y;
          cb->z = z;
        }

      c.x = (GLfloat) x / w - 0.5;
      c.y = (GLfloat) y / w - 0.5;
      c.z = (GLfloat) z / w - 0.5;

      /* #### We could save some polygons by not drawing the spheres
         between colinear segments. */

      if (t > 0) {
        if (draw_segment (mi, prev, c, t, end_t, path_start, path_end, !any_p,
                          &last_color))
          any_p = True;
      }
      prev = c;
      if (! first_p) {
        first = c;
        first_p = True;
      }
    }

  if (bp->closed_p && path_end >= 1.0) {
    draw_segment (mi, prev, first, t, end_t, path_start, path_end, !any_p,
                  &last_color);
  }
}



/* Window management, etc
 */
ENTRYPOINT void
reshape_hilbert (ModeInfo *mi, int width, int height)
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
hilbert_handle_event (ModeInfo *mi, XEvent *event)
{
  hilbert_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;
  else if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);
      if (c == '+' || c == '=' ||
          keysym == XK_Up || keysym == XK_Right || keysym == XK_Next)
        {
          bp->depth++;
          if (bp->depth > max_depth) bp->depth = max_depth;
          return True;
        }
      else if (c == '-' || c == '_' ||
               keysym == XK_Down || keysym == XK_Left || keysym == XK_Prior)
        {
          bp->depth--;
          if (bp->depth < 1) bp->depth = 1;
          return True;
        }
      else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
        {
          bp->depth += bp->depth_tick;
          if (bp->depth > max_depth-1)
            {
              bp->depth = max_depth;
              bp->depth_tick = -1;
            }
          else if (bp->depth <= 1)
            {
              bp->depth = 1;
              bp->depth_tick = 1;
            }
          return True;
        }
    }

  return False;
}


ENTRYPOINT void 
init_hilbert (ModeInfo *mi)
{
  hilbert_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);
  int i;

  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];

  bp->depth      = 2;
  bp->depth_tick = 1;
  bp->path_start = 0.0;
  bp->path_end   = 0.0;
  bp->path_tick  = 1;

  if (thickness < 0.01) thickness = 0.01;
  if (thickness > 1.0) thickness = 1.0;

  if (speed <= 0) speed = 0.0001;
  if (max_depth < 2) max_depth = 2;
  if (max_depth > 20) max_depth = 20;  /* hard limit due to hilbert_cache */

  if (bp->depth > max_depth-1) bp->depth = max_depth-1;

  bp->glx_context = init_GL(mi);

  reshape_hilbert (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  if (!wire)
    {
      GLfloat pos[4] = {1.0, 1.0, 1.0, 0.0};
      GLfloat amb[4] = {0.0, 0.0, 0.0, 1.0};
      GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
      GLfloat spc[4] = {0.0, 1.0, 1.0, 1.0};

      glEnable (GL_LIGHTING);
      glEnable (GL_LIGHT0);

      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);
    }

  {
    double spin_speed   = 0.04;
    double tilt_speed   = spin_speed / 10;
    double wander_speed = 0.008;
    double spin_accel   = 0.01;

    bp->rot = make_rotator (do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            spin_accel,
                            do_wander ? wander_speed : 0,
                            do_spin);
    bp->rot2 = make_rotator (0, 0, 0, 0, tilt_speed, True);
    bp->trackball = gltrackball_init (True);
  }

  if (mode_str && !strcasecmp(mode_str, "2d"))
    bp->twodee_p = True;
  else if (mode_str && (!strcasecmp(mode_str, "3d")))
    bp->twodee_p = False;
  else
    {
      if (! (mode_str && !strcasecmp(mode_str, "random")))
        fprintf (stderr, "%s: 'mode' must be '2d', '3d', or 'random'\n",
                 progname);
      bp->twodee_p = ((random() % 3) == 0);
    }


  if (ends_str && (!strcasecmp(ends_str, "closed")))
    bp->closed_p = True;
  else if (ends_str && (!strcasecmp(ends_str, "open")))
    bp->closed_p = False;
  else
    {
      if (! (ends_str && !strcasecmp(ends_str, "random")))
        fprintf (stderr, "%s: 'ends' must be 'open', 'closed', or 'random'\n",
                 progname);
      bp->closed_p = ((random() % 3) != 0);
    }


  /* More colors results in more polygons (more tube segments) so keeping
     this small is worthwhile. */
  bp->ncolors = (bp->twodee_p ? 1024 : 128);

  if (bp->closed_p)
    {
      /* Since the path is closed, colors must also be a closed loop */
      bp->colors = (XColor *) calloc(bp->ncolors, sizeof(XColor));
      make_smooth_colormap (0, 0, 0,
                            bp->colors, &bp->ncolors,
                            False, 0, False);
    }
  else
    {
      /* Since the path is open, first and last colors should differ. */
      bp->ncolors *= 2;
      bp->colors = (XColor *) calloc(bp->ncolors, sizeof(XColor));
      make_uniform_colormap (0, 0, 0,
                             bp->colors, &bp->ncolors,
                             False, 0, False);
      bp->ncolors /= 2;
    }

  /* Generate display lists for several different resolutions of
     a unit tube and a unit sphere.
   */
  for (i = 0; i < countof(dlist_faces); i++)
    {
      int faces = dlist_faces[i];
      bp->dlists[faces][0] = glGenLists (1);

      glNewList (bp->dlists[faces][0], GL_COMPILE);
      bp->dlist_polys[faces][0] = unit_sphere (faces, faces, wire);
      glEndList ();

      bp->dlists[faces][1] = glGenLists (1);

      glNewList (bp->dlists[faces][1], GL_COMPILE);
      bp->dlist_polys[faces][1] =
        tube (0, 0, 0, 1, 0, 0,
              1, 0, faces, True, 0, wire);
      glEndList ();
    }
}


ENTRYPOINT void
draw_hilbert (ModeInfo *mi)
{
  hilbert_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int wire = MI_IS_WIREFRAME(mi);

  static const GLfloat bspec[4]  = {1.0, 1.0, 1.0, 1.0};
  static const GLfloat bshiny    = 128.0;
  GLfloat bcolor[4] = {1.0, 1.0, 1.0, 1.0};

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  glShadeModel(GL_SMOOTH);

  if (! wire)
    {
      glEnable (GL_NORMALIZE);
      glEnable (GL_DEPTH_TEST);
      glEnable (GL_CULL_FACE);
      glEnable (GL_LIGHTING);
      glEnable (GL_LIGHT0);
      glEnable (GL_POLYGON_OFFSET_FILL);
    }

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  glScalef(1.1, 1.1, 1.1);

  {
    double x, y, z, z2;
    get_position (bp->rot, &x, &y, &z, !bp->button_down_p);
    glTranslatef((x - 0.5) * 8,
                 (y - 0.5) * 8,
                 (z - 0.5) * 15);

    gltrackball_rotate (bp->trackball);

    get_rotation (bp->rot, &x, &y, &z, !bp->button_down_p);

    if (bp->twodee_p && do_spin)    /* face front */
      {
        double max = 70;
        get_position (bp->rot2, &x, &y, &z2, !bp->button_down_p);
        glRotatef (max/2 - x*max, 1, 0, 0);
        glRotatef (max/2 - y*max, 0, 1, 0);
        glRotatef (z * 360, 0, 0, 1);  /* but upside down is ok */
      }
    else
      {
        glRotatef (x * 360, 1, 0, 0);
        glRotatef (y * 360, 0, 1, 0);
        glRotatef (z * 360, 0, 0, 1);
      }
  }

  mi->polygon_count = 0;

  glMaterialfv (GL_FRONT, GL_SPECULAR,            bspec);
  glMateriali  (GL_FRONT, GL_SHININESS,           bshiny);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, bcolor);

  glScalef (8,8,8);
  glTranslatef (0.1, 0.1, 0);

  if (!do_spin && !bp->twodee_p)
    {
      /* tilt the cube a little, and make the start and end visible */
      glTranslatef (-0.1, 0, 0);
      glRotatef (140, 0, 1, 0);
      glRotatef (30, 1, 0, 0);
    }

  if (wire)
    glLineWidth (bp->depth > 4 ? 1.0 :
                 bp->depth > 3 ? 2.0 : 
                 3.0);

  if (bp->path_tick > 0)	/* advancing the end point, [0 - N) */
    {				/* drawing 1 partial path, 1st time only. */

      if (!bp->button_down_p)
        bp->path_end += 0.01 * speed;

      if (bp->path_end >= 1.0)
        {
          bp->path_start = 0.0;
          bp->path_end   = 1.0;
          bp->path_tick = -1;
          bp->pause = PAUSE_TICKS;
        }

      bp->diam = thickness / pow (2, bp->depth);
      hilbert (mi, bp->depth, bp->path_start, bp->path_end);
      mi->recursion_depth = bp->depth + bp->path_start;
    }

  else				/* advancing the start point, (N - 1] */
    {				/* drawing 2 paths at different rez. */
      if (bp->pause)
        {
          bp->pause--;
        }
      else
        {
          if (!bp->button_down_p)
            bp->path_start += 0.01 * speed;

          if (bp->path_start > 1.0)
            {
              bp->path_start = 0.0;
              bp->depth += bp->depth_tick;
              bp->pause = PAUSE_TICKS;
              if (bp->depth > max_depth-1) 
                {
                  bp->depth = max_depth;
                  bp->depth_tick = -1;
                }
              else if (bp->depth <= 1)
                {
                  bp->depth = 1;
                  bp->depth_tick = 1;
                }
            }
        }

      {
        GLfloat d1 = thickness / pow (2, bp->depth);
        GLfloat d2 = thickness / pow (2, bp->depth + bp->depth_tick);
        bp->diam = (d1 * (1 - bp->path_start) +
                    d2 * bp->path_start);
      }

      /* First time around, start is 0, and end animates forward.
         Then, to display the next higher resolution, we render
         depth=N while increasing start and leaving end=1.0;
         and simultaneously animationg depth=N+1 with start=0 and
         end increasing by the same amount.  

         The two paths aren't actually connected together at the
         resolution-change interface, and sometimes they overlap,
         but things go fast enough that it's hard to spot those
         glitches, so it looks ok.
       */
      glPolygonOffset (0, 0);
      hilbert (mi, bp->depth,   bp->path_start, bp->path_end);

      glPolygonOffset (1.0, 1.0);
      hilbert (mi, bp->depth + bp->depth_tick, 0.0, bp->path_start);

      mi->recursion_depth = bp->depth + (bp->depth_tick * bp->path_start);
    }

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_hilbert (ModeInfo *mi)
{
  hilbert_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;

  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  if (bp->trackball) gltrackball_free (bp->trackball);
  if (bp->rot) free_rotator (bp->rot);
  if (bp->rot2) free_rotator (bp->rot2);
  if (bp->colors) free (bp->colors);
  if (bp->caches)
    for (i = 0; i < max_depth + 2; i++)
      if (bp->caches[i])
        {
          free (bp->caches[i]->values);
          free (bp->caches[i]);
        }
  /* #### free dlists */
}

XSCREENSAVER_MODULE ("Hilbert", hilbert)

#endif /* USE_GL */
