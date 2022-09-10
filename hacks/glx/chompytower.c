/* chompytower, Copyright © 2022 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.

 TODO
 - knotholes:
   - render teeth into color and depth
   - render branches into depth
   - render tubes into depth
   - render branches into color with depth-equal
 */

#define DEFAULTS	"*delay:	30000       \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \
			"*towerColor:   #eE9752"   "\n" \
			"*teethColor:   #FFFF88"   "\n" \
			"*jawColor:     #eE9752"   "\n" \

# define release_chompytower 0

#ifdef USE_GL /* whole file */

#define DEF_SPEED	"1.0"
#define DEF_SPIN	"True"
#define DEF_WANDER	"False"
#define DEF_TILT	"True"
#define DEF_SMOOTH	"True"
#define DEF_RESOLUTION	"1.0"

#include "xlockmore.h"
#include "colors.h"
#include "hsv.h"
#include "sphere.h"
#include "rotator.h"
#include "normals.h"
#include "spline.h"
#include "gltrackball.h"
#include "gllist.h"
#include <ctype.h>

extern const struct gllist
  *teeth_model_jaw_upper_half,   *teeth_model_jaw_lower_half,
  *teeth_model_teeth_upper_half, *teeth_model_teeth_lower_half;

static const struct gllist * const *all_objs[] = {
  &teeth_model_jaw_upper_half,   &teeth_model_jaw_lower_half,
  &teeth_model_teeth_upper_half, &teeth_model_teeth_lower_half,
};

enum { JAW_UPPER_HALF, JAW_LOWER_HALF, TEETH_UPPER_HALF, TEETH_LOWER_HALF };


typedef struct {
  GLfloat x, y;		/* position of midpoint */
  GLfloat dx, dy;	/* velocity and direction */
  double torque;	/* rotational speed */
  double th;		/* angle of rotation */
  GLfloat elasticity;	/* how fast they deform */
  GLfloat max_velocity;	/* speed limit */
  GLfloat min_r, max_r;	/* radius range */
  int npoints;		/* control points */
  GLfloat *r;		/* radii */
  spline *spline;
} blob;

typedef struct {
  enum { DEAD, HIDDEN, EXTENDING, OPENING, OPEN, CLOSING, CLOSED, RETRACTING }
    state;
  GLfloat ratio, speed;
  int pos;
} funhole;

typedef struct {
  int npoints;
  XYZ *points, *fnormals, *vnormals;
  GLfloat r, z;
  GLfloat color[4];
  funhole funhole;
} slice;

typedef struct {
  XYZ pos, orient;
  blob *blob;
  int nslices, max_slices;
  GLfloat slice_height;
  slice **slices;
  GLuint dlist;
  int npolys;

  int ncolors;
  XColor *colors;
  GLfloat ccolor;

} branch;

typedef struct {
  GLXContext *glx_context;
  rotator *rot, *rot2;
  trackball_state *trackball;
  Bool button_down_p;
  int nbranches;
  branch **branches;
  double last_tick;
  GLuint *component_dlists;
  GLfloat component_colors[countof(all_objs)][4];
  GLuint sphere_dlist;
  int sphere_npolys;
} chompytower_configuration;

static chompytower_configuration *bps = NULL;

static GLfloat speed;
static GLfloat resolution_arg;
static Bool do_spin;
static Bool do_wander;
static Bool do_tilt;
static Bool do_smooth;

static XrmOptionDescRec opts[] = {
  { "-speed",   ".speed",  XrmoptionSepArg, 0 },
  { "-spin",    ".spin",   XrmoptionNoArg, "True"  },
  { "+spin",    ".spin",   XrmoptionNoArg, "False" },
  { "-wander",  ".wander", XrmoptionNoArg, "True"  },
  { "+wander",  ".wander", XrmoptionNoArg, "False" },
  { "-tilt",    ".tilt",   XrmoptionNoArg, "True"  },
  { "+tilt",    ".tilt",   XrmoptionNoArg, "False" },
  { "-smooth",  ".smooth", XrmoptionNoArg, "True"  },
  { "+smooth",  ".smooth", XrmoptionNoArg, "False" },
  { "-resolution", ".resolution", XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_Bool},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&do_tilt,   "tilt",   "Tilt",   DEF_TILT,   t_Bool},
  {&do_smooth, "smooth", "Smooth", DEF_SMOOTH, t_Bool},
  {&resolution_arg, "resolution", "Resolution", DEF_RESOLUTION, t_Float},
};

ENTRYPOINT ModeSpecOpt chompytower_opts = {
  countof(opts), opts, countof(vars), vars, NULL };

#define RANDSIGN() ((random() & 1) ? 1 : -1)
#define SPLINE_SCALE 1000
#define FUNHOLE_HEIGHT 0.2

static double
double_time (void)
{
  struct timeval now;
# ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone tzp;
  gettimeofday(&now, &tzp);
# else
  gettimeofday(&now);
# endif

  return (now.tv_sec + ((double) now.tv_usec * 0.000001));
}


static void
parse_color (ModeInfo *mi, char *key, GLfloat color[4])
{
  XColor xcolor;
  char *string = get_string_resource (mi->dpy, key, "Color");
  if (!XParseColor (mi->dpy, mi->xgwa.colormap, string, &xcolor))
    {
      fprintf (stderr, "%s: unparsable color in %s: %s\n", progname,
               key, string);
      exit (1);
    }
  free (string);

  color[0] = xcolor.red   / 65536.0;
  color[1] = xcolor.green / 65536.0;
  color[2] = xcolor.blue  / 65536.0;
  color[3] = 1;
}


static XYZ
normalize (XYZ p)
{
  GLfloat d = sqrt(p.x*p.x + p.y*p.y + p.z*p.z);
  if (d < 0.0000001)
    p.x = p.y = p.z = 0;
  else
    {
      p.x /= d;
      p.y /= d;
      p.z /= d;
    }

  return p;
}


/* From goop.c, which I wrote 25 years ago. We use all parts of the buffalo. */
static blob *
make_blob (void)
{
  blob *b = (blob *) calloc (1, sizeof(*b));
  int i;
  GLfloat mid;
  GLfloat size = 1;

  GLfloat ss = 0.2 / resolution_arg;

  b->max_r = size/2;
  b->min_r = size/10;

  if (b->min_r < 0.1) b->min_r = 0.1;
  mid = ((b->min_r + b->max_r) / 2);

  b->torque       = ss * 0.0075;
  b->elasticity   = ss * 0.09;
  b->max_velocity = ss * 0.5;

  b->x = 0;
  b->y = 0;

  b->dx = frand (b->max_velocity) * RANDSIGN();
  b->dy = frand (b->max_velocity) * RANDSIGN();
  b->th = frand (M_PI+M_PI) * RANDSIGN();
  b->npoints = (random() % 5) + 5;

  b->spline = make_spline (b->npoints);
  b->r = (GLfloat *) calloc (b->npoints, sizeof(*b->r));
  for (i = 0; i < b->npoints; i++)
    b->r[i] = (fmod (random(), mid) + (mid/2)) * RANDSIGN();
  return b;
}


static void
free_blob (blob *blob)
{
  free_spline (blob->spline);
  free (blob->r);
  free (blob);
}


static void 
throb_blob (blob *b)
{
  int i;
  double frac = ((M_PI+M_PI) / b->npoints);

  for (i = 0; i < b->npoints; i++)
    {
      GLfloat r = b->r[i];
      GLfloat ra = (r > 0 ? r : -r);
      GLfloat th = (b->th > 0 ? b->th : -b->th);
      GLfloat x, y;

      /* place control points evenly around perimiter, shifted by theta */
      x = b->x + ra * cos (i * frac + th);
      y = b->y + ra * sin (i * frac + th);

      b->spline->control_x[i] = x * SPLINE_SCALE;
      b->spline->control_y[i] = y * SPLINE_SCALE;

      /* alter the radius by a random amount, in the direction in which
	 it had been going (the sign of the radius indicates direction.) */
      ra += (frand (b->elasticity) * (r > 0 ? 1 : -1));
      r = ra * (r >= 0 ? 1 : -1);

      /* If we've reached the end (too long or too short) reverse direction. */
      if ((ra > b->max_r && r >= 0) ||
	  (ra < b->min_r && r < 0))
	r = -r;
      /* And reverse direction in mid-course once every 50 times. */
      else if (! (random() % 50))
	r = -r;

      b->r[i] = r;
    }

  compute_closed_spline (b->spline);
}


static slice *
make_slice (blob *b, double r)
{
  slice *s = (slice *) calloc (1, sizeof(*s));

  /* Each slice must have the same number of points, or we can't stack them
     properly.  So re-scale the spline output to our resolution.
   */
  int n0 = b->spline->n_points;
  int n1 = 40 * resolution_arg;
  const XPoint *p0 = b->spline->points;
  XYZ *p1 = (XYZ *) calloc (n1, sizeof(*p1));
  GLfloat ss = SPLINE_SCALE;
  int i1;

  s->r = r;
  for (i1 = 0; i1 < n1; i1++)
    {
      double r  = i1 / (double) n1;
      double i0 = r * n0;
      int i0a   = (int) i0;
      int i0b   = (i0a + 1) % n0;
      double r1 = (n0 > n1 ? 0 : fmod (i0, 1));
      p1[i1].x = s->r * (p0[i0a].x + (r1 * (p0[i0b].x - p0[i0a].x))) / ss;
      p1[i1].y = s->r * (p0[i0a].y + (r1 * (p0[i0b].y - p0[i0a].y))) / ss;
      p1[i1].z = 0;
    }

  s->npoints = n1;
  s->points  = p1;
  s->fnormals = (XYZ *) calloc (n1, sizeof(*s->fnormals));
  s->vnormals = (XYZ *) calloc (n1, sizeof(*s->vnormals));
  s->funhole.state = DEAD;

  return s;
}


static void
free_slice (slice *s)
{
  free (s->points);
  free (s->fnormals);
  free (s->vnormals);
  free (s);
}


static branch *
make_branch (ModeInfo *mi)
{
  branch *b = (branch *) calloc (1, sizeof(*b));

  b->blob = make_blob();
  b->max_slices = 500 * resolution_arg;
  b->slice_height = 0.02 / resolution_arg;
  b->dlist = glGenLists (1);

  {
    GLfloat c[4];
    int h1, h2;
    double s1, s2, v1, v2;

    parse_color (mi, "towerColor", c);
    rgb_to_hsv (c[0] * 65536, c[1] * 65536, c[2] * 65536, &h1, &s1, &v1);
    h2 = h1; s2 = s1; v2 = v1;
    v2 *= (v2 > 0.5 ? 0.25 : 2);
    if (v2 > 1) v2 = 1;

    b->ncolors = 64;
    b->colors = (XColor *) calloc (b->ncolors, sizeof(*b->colors));
    make_color_ramp (0, 0, 0, h1, s1, v1, h2, s2, v2, b->colors, &b->ncolors,
                     True, False, 0);
  }

  return b;
}


static void
free_branch (branch *b)
{
  int i;
  free_blob (b->blob);
  for (i = 0; i < b->nslices; i++)
    free_slice (b->slices[i]);
  free (b->slices);
  free (b->colors);
  glDeleteLists (b->dlist, 1);
  free (b);
}


static void
grow_branch (branch *b)
{
  GLfloat step = 0.01 / resolution_arg;
  GLfloat r = 0;
  slice *o = b->nslices ? b->slices[b->nslices-1] : 0;
  slice *s0, *s1, *s2;
  int i;

  b->slices = (slice **)
    (b->slices
     ? realloc (b->slices, (b->nslices + 1) * sizeof (*b->slices))
     : calloc (b->nslices + 1, sizeof (*b->slices)));

  if (b->nslices)
    {
      r = b->slices[b->nslices-1]->r + step;
      if (r > 1) r = 1;
    }

  s1 = b->nslices ? b->slices[b->nslices-1] : 0;

  throb_blob (b->blob);
  s0 = make_slice (b->blob, r);
  s0->z = s1 ? s1->z + b->slice_height : 0;
  b->slices [b->nslices++] = s0;

  /* Compute face normals for this slice, downward. */
  for (i = 0; i < s0->npoints; i++)
    {
      int j = (i+1) % s0->npoints;
      XYZ a = s0->points[i];
      XYZ b = s0->points[j];
      XYZ c = o ? o->points[i] : a;
      XYZ d = o ? o->points[j] : b;
      XYZ n1, n2;

      a.z += s0->z;
      b.z += s0->z;
      c.z += o ? o->z : s0->z;

      n1 = normalize (calc_normal (a, c, b));
      n2 = normalize (calc_normal (c, d, b));

      /* These quads aren't planes: they twist, so average the normals
         of any two component triangles. */
      s0->fnormals[i].x = (n1.x + n2.x) / 2;
      s0->fnormals[i].y = (n1.y + n2.y) / 2;
      s0->fnormals[i].z = (n1.z + n2.z) / 2;
    }

  /* Vertex normals are average of face normals of the 4 quads in which it
     participates.  We have just added layer N.  We can now compute the vertex
     normals for layer N-1, between N-1 and N-2, if those exist.
   */
  s2 = b->nslices > 1 ? b->slices[b->nslices-2] : 0;
  if (s2)
    for (i = 0; i < s0->npoints; i++)
      {
        int j = i < s0->npoints-1 ? i+1 : 0;
        XYZ n1 = s0->fnormals[i];
        XYZ n2 = s0->fnormals[j];
        XYZ n3 = s2->fnormals[i];
        XYZ n4 = s2->fnormals[j];
        s1->vnormals[i].x = (n1.x + n2.x + n3.x + n4.x) / 4;
        s1->vnormals[i].y = (n1.y + n2.y + n3.y + n4.y) / 4;
        s1->vnormals[i].z = (n1.z + n2.z + n3.z + n4.z) / 4;
      }

  s0->color[0] = b->colors[(int) b->ccolor].red   / 65536.0;
  s0->color[1] = b->colors[(int) b->ccolor].green / 65536.0;
  s0->color[2] = b->colors[(int) b->ccolor].blue  / 65536.0;

  s0->color[3] = 1;
  b->ccolor += 1.0 / speed;
  if (b->ccolor >= b->ncolors)
    b->ccolor = 0;

  {
    GLfloat last_funhole_y = 0;

    for (i = 0; i < b->nslices; i++)
      {
        slice *s1 = b->slices[i];
        if (s1->funhole.state != DEAD)
          last_funhole_y = s1->z;
      }
    
    if (s0->z > last_funhole_y + FUNHOLE_HEIGHT * 1.3 &&
        (last_funhole_y == 0 ||
         !(random() % 10)))
      {
        /* Don't place a funhole if this point is too close to the center:
           the tube is too thin right here.
         */
        GLfloat min_dist = 0.3;
        int pos = random() % s0->npoints;
        XYZ p = s0->points[pos];

        GLfloat dist2 = p.x*p.x + p.y*p.y + p.z*p.z;
        if (dist2 >= min_dist * min_dist)  /* No need for sqrt */
          {
            s0->funhole.state = HIDDEN;
            s0->funhole.pos = pos;
            s0->funhole.ratio = 0;
            s0->funhole.speed = 0.04 + frand(0.01);
          }
      }
  }
}


static void
add_branch (ModeInfo *mi)
{
  chompytower_configuration *bp = &bps[MI_SCREEN(mi)];
  bp->branches = (branch **)
    (bp->branches
     ? realloc (bp->branches, (bp->nbranches + 1) * sizeof (*bp->branches))
     : calloc (bp->nbranches + 1, sizeof (*bp->branches)));
  bp->branches [bp->nbranches++] = make_branch (mi);
}


static void
render_branch (ModeInfo *mi, branch *b)
{
  int wire = MI_IS_WIREFRAME(mi);
  GLfloat spec[4] = {0.4, 0.4, 0.4, 1.0};
  GLfloat shiny = 20; /* 0-128 */
  int i, j;

  glNewList (b->dlist, GL_COMPILE);
  b->npolys = 0;

  glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,  spec);
  glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS, shiny);

  for (i = 1; i < b->nslices; i++)
    {
      slice *s1 = b->slices[i];
      slice *s2 = b->slices[i-1];

      if (! wire)
        glBegin (do_smooth ? GL_QUAD_STRIP : GL_QUADS);

# define COL(S) \
      if (! wire) { \
        glColor4fv (S->color); \
        glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, S->color); \
      }

      for (j = 0; j <= s1->npoints; j++)
        {
          int jj = j % s1->npoints;
          int kk = (j+1) % s1->npoints;
          XYZ pa = s1->points[jj];
          XYZ pb = s2->points[jj];
          XYZ pc = s1->points[kk];
          XYZ pd = s2->points[kk];
          XYZ na = do_smooth ? s1->vnormals[jj] : s1->fnormals[jj];
          XYZ nb = do_smooth ? s2->vnormals[jj] : s1->fnormals[jj];
          XYZ nc = do_smooth ? s1->vnormals[kk] : s1->fnormals[jj];
          XYZ nd = do_smooth ? s2->vnormals[kk] : s1->fnormals[jj];

          pa.z += s1->z;
          pb.z += s2->z;
          pc.z += s1->z;
          pd.z += s2->z;

          if (wire) glBegin (GL_LINE_LOOP);

          COL(s1);
          glNormal3f (na.x, na.y, na.z); glVertex3f (pa.x, pa.y, pa.z);
          COL(s2);
          glNormal3f (nb.x, nb.y, nb.z); glVertex3f (pb.x, pb.y, pb.z);
          if (wire || !do_smooth)
            {
              glNormal3f (nd.x, nd.y, nd.z); glVertex3f (pd.x, pd.y, pd.z);
              COL(s1);
              glNormal3f (nc.x, nc.y, nc.z); glVertex3f (pc.x, pc.y, pc.z);
            }
          b->npolys++;

          if (wire) glEnd();

# undef COL

# if 0
          /* Draw point normals */
          if (wire)
            {
              GLfloat ss = 0.05;
              glColor3f (0, 0, 1);
              glBegin (GL_LINES);
              glVertex3f (pa.x, pa.y, pa.z);
              glVertex3f (pa.x + na.x * ss,
                          pa.y + na.y * ss,
                          pa.z + na.z * ss);
              glVertex3f (pb.x, pb.y, pb.z);
              glVertex3f (pb.x + nb.x * ss,
                          pb.y + nb.y * ss,
                          pb.z + nb.z * ss);
              glEnd();
              glColor3f (0, 1, 0);
            }
# endif
# if 0
          /* Draw face normals */
          if (wire)
            {
              GLfloat ss = 0.05;
              XYZ pe;
              XYZ ne = s1->fnormals[jj];
              glColor3f (0, 1, 1);
              pe.x = (pa.x + pb.x + pc.x + pd.x) / 4;
              pe.y = (pa.y + pb.y + pc.y + pd.y) / 4;
              pe.z = (pa.z + pb.z + pc.z + pd.z) / 4;

              glBegin (GL_LINES);
              glVertex3f (pe.x, pe.y, pe.z);
              glVertex3f (pe.x + ne.x * ss,
                          pe.y + ne.y * ss,
                          pe.z + ne.z * ss);
              glEnd();
              glColor3f (0, 1, 0);
            }
# endif
        }
      if (! wire) glEnd();

    }

  glEndList ();
}


static void
tick_funhole (slice *s)
{
  if (s->funhole.state == DEAD)
    return;

  s->funhole.ratio += s->funhole.speed;
  if (s->funhole.ratio <= 1)
    return;

  s->funhole.ratio = 0;
  s->funhole.speed = (0.05 + frand (0.01)) * speed;

  switch (s->funhole.state) {
  case HIDDEN:
    s->funhole.state = EXTENDING;
    s->funhole.speed *= 0.2;
    break;
  case EXTENDING:
    s->funhole.state = CLOSED;
    s->funhole.speed *= 1 + frand(4);
    break;
  case CLOSED:
    if (! (random() % 20))
      {
        s->funhole.state = RETRACTING;
        s->funhole.speed *= 0.2;
      }
    else
      {
        s->funhole.state = OPENING;
        s->funhole.speed *= 3;
      }
    break;
  case OPENING:
    s->funhole.state = OPEN;
    if (! (random() % 6))
      {
        s->funhole.speed *= 0.3;
        s->funhole.speed *= 1 + frand(3);
      }
    else
      s->funhole.speed *= 100;
    break;
  case OPEN:
    s->funhole.state = CLOSING;
    s->funhole.speed *= 3;
    break;
  case CLOSING:
    s->funhole.state = CLOSED;
    if (! (random() % 6))
      {
        s->funhole.speed *= 0.3;
        s->funhole.speed *= 1 + frand(4);
      }
    else
      s->funhole.speed *= 100;
    break;
  case RETRACTING:
    s->funhole.state = HIDDEN;
    s->funhole.speed *= 1 + frand(4);
    break;
  default:
    abort();
    break;
  }
}


static void
tick_chompytower (ModeInfo *mi)
{
  chompytower_configuration *bp = &bps[MI_SCREEN(mi)];
  double step = 0.01 * speed;  /* #### */
  int i;

  for (i = 0; i < bp->nbranches; i++)
    {
      branch *b = bp->branches[i];
      double min_z = -(b->max_slices * b->slice_height * 0.35);  /* #### */
      Bool changed_p = False;
      int j;

      b->pos.z -= step;
      if (b->pos.z < min_z && b->nslices)
        {
          slice *s = b->slices[0];
          free_slice (s);
          for (j = 0; j < b->nslices; j++)
            {
              slice *s2 = j < b->nslices-1 ? b->slices[j+1] : 0;
              b->slices[j] = s2;
              if (s2)
                s2->z -= b->slice_height;
            }
          b->nslices--;
          b->pos.z += b->slice_height;
          changed_p = True;
        }

      if (b->nslices < b->max_slices)
        {
          grow_branch (b);
          changed_p = True;
        }

      if (changed_p)
        render_branch (mi, b);

      for (j = 0; j < b->nslices; j++)
        tick_funhole (b->slices[j]);
    }
}


ENTRYPOINT void
reshape_chompytower (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;
  int y = 0;

  if (width > height * 5) {   /* tiny window: show middle */
    height = width;
    y = -height/2;
    h = height / (GLfloat) width;
  }

  glViewport (0, y, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (30, 1/h, 1, 500);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0, 0, 30,
             0, 0, 0,
             0, 1, 0);

  {
    GLfloat s = (MI_WIDTH(mi) < MI_HEIGHT(mi)
                 ? (MI_WIDTH(mi) / (GLfloat) MI_HEIGHT(mi))
                 : 1);
    glScalef (s, s, s);
  }

  glClear(GL_COLOR_BUFFER_BIT);
}


ENTRYPOINT Bool
chompytower_handle_event (ModeInfo *mi, XEvent *event)
{
  chompytower_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;

  return False;
}


ENTRYPOINT void 
init_chompytower (ModeInfo *mi)
{
  chompytower_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);
  int i;

  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_chompytower (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  {
    double spin_speed   = 0.3;
    double wander_speed = 0.005;
    double tilt_speed   = 0.01;
    double spin_accel   = 0.5;

    bp->rot = make_rotator (0, 0, do_spin ? spin_speed : 0,
                            spin_accel,
                            do_wander ? wander_speed : 0,
                            True);
    bp->rot2 = make_rotator (0, 0, 0, 0,
                             (do_tilt ? tilt_speed : 0),
                             True);
    bp->trackball = gltrackball_init (False);
  }

  bp->component_dlists = (GLuint *)
    calloc (countof(all_objs)+1, sizeof(GLuint));
  for (i = 0; i < countof(all_objs); i++)
    bp->component_dlists[i] = glGenLists (1);

  for (i = 0; i < countof(all_objs); i++)
    {
      const struct gllist *gll = *all_objs[i];
      char *key = 0;
      GLfloat spec[4] = {0.4, 0.4, 0.4, 1.0};
      GLfloat shiny = 80; /* 0-128 */

      glNewList (bp->component_dlists[i], GL_COMPILE);

      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      glMatrixMode(GL_TEXTURE);
      glPushMatrix();
      glMatrixMode(GL_MODELVIEW);

      glRotatef (-90, 1, 0, 0);

      glBindTexture (GL_TEXTURE_2D, 0);

      switch (i) {
      case JAW_UPPER_HALF:
      case JAW_LOWER_HALF:
        key = "jawColor";
        break;
      case TEETH_UPPER_HALF:
      case TEETH_LOWER_HALF:
        key = "teethColor";
        break;
      default:
        abort();
      }

      parse_color (mi, key, bp->component_colors[i]);

      glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,  spec);
      glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS, shiny);
      renderList (gll, wire);

      glMatrixMode(GL_TEXTURE);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);
      glPopMatrix();

      glEndList ();
    }

  bp->sphere_dlist = glGenLists (1);
  glNewList (bp->sphere_dlist, GL_COMPILE);
  bp->sphere_npolys += unit_sphere (16, 32, wire);
  glEndList ();

  bp->nbranches = 0;
  add_branch (mi);
}


static int
draw_component (ModeInfo *mi, int i)
{
  chompytower_configuration *bp = &bps[MI_SCREEN(mi)];
  glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE,
                bp->component_colors[i]);

  glFrontFace (GL_CCW);
  glCallList (bp->component_dlists[i]);

  glPushMatrix();
  glScalef (-1, 1, 1);
  glFrontFace (GL_CW);
  glCallList (bp->component_dlists[i]);
  glPopMatrix();

  return 2 * (*all_objs[i])->points / 3;
}


static GLfloat
ease_fn (GLfloat r)
{
  return cos ((r/2 + 1) * M_PI) + 1; /* Smooth curve up, end at slope 1. */
}


static GLfloat
ease_ratio (GLfloat r)
{
  GLfloat ease = 0.5;
  if      (r <= 0)     return 0;
  else if (r >= 1)     return 1;
  else if (r <= ease)  return     ease * ease_fn (r / ease);
  else if (r > 1-ease) return 1 - ease * ease_fn ((1 - r) / ease);
  else                 return r;
}


static int
draw_funhole (ModeInfo *mi, slice *s, Bool shadow_p)
{
  chompytower_configuration *bp = &bps[MI_SCREEN(mi)];
  int polys = 0;
  XYZ p = s->points[s->funhole.pos];
  GLfloat X, Y, Z;
  GLfloat dist, odist;
  GLfloat tilt = 0;
  GLfloat max_tilt = 20;
  static const XYZ hole_aspect = { 0.5, 0.225, 0.9 };

  if (s->funhole.state == DEAD)
    return 0;

  dist = sqrt (p.x*p.x + p.y*p.y + p.z*p.z);
  odist = dist;

  glPushMatrix();
  glTranslatef (0, 0, p.z + s->z);

  X = p.x;
  Y = p.y;
  Z = p.z;
  glRotatef (-atan2 (X, Y)               * (180 / M_PI), 0, 0, 1);
  glRotatef ( atan2 (Z, sqrt(X*X + Y*Y)) * (180 / M_PI), 1, 0, 0);

  switch (s->funhole.state) {
  case HIDDEN:     dist  = 0; break;
  case EXTENDING:  dist *= ease_ratio (s->funhole.ratio);     break;
  case RETRACTING: dist *= ease_ratio (1 - s->funhole.ratio); break;
  case OPENING:    tilt  = ease_ratio (s->funhole.ratio);     break;
  case CLOSING:    tilt  = ease_ratio (1 - s->funhole.ratio); break;
  case OPEN:       tilt  = 1; break;
  case CLOSED:     tilt  = 0; break;
  default: abort(); break;
  }

  dist *= 0.4;

  if (shadow_p)
    {
      glFrontFace (GL_CCW);
      glPushMatrix();
      glTranslatef (0, odist, 0);
      glScalef (hole_aspect.x * FUNHOLE_HEIGHT,
                hole_aspect.y * FUNHOLE_HEIGHT,
                hole_aspect.z * FUNHOLE_HEIGHT);
      glCallList (bp->sphere_dlist);
      polys += bp->sphere_npolys;
      glPopMatrix();
    }
  else /* if (s->funhole.state != HIDDEN) */
    {
      glTranslatef (0, dist, 0);
      glScalef (FUNHOLE_HEIGHT, FUNHOLE_HEIGHT, FUNHOLE_HEIGHT);

      glRotatef (-90, 1, 0, 0);

      glRotatef (tilt * -max_tilt, 1, 0, 0);
      /* polys += draw_component (mi, JAW_UPPER_HALF); */
      polys += draw_component (mi, TEETH_UPPER_HALF);

      glRotatef (tilt * max_tilt * 2, 1, 0, 0);
      /* polys += draw_component (mi, JAW_LOWER_HALF); */
      polys += draw_component (mi, TEETH_LOWER_HALF);
    }

  glPopMatrix();
  return polys;
}


ENTRYPOINT void
draw_chompytower (ModeInfo *mi)
{
  chompytower_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int wire = MI_IS_WIREFRAME(mi);
  double now = double_time();
  int i;

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glShadeModel (GL_SMOOTH);
  glEnable (GL_DEPTH_TEST);
  glEnable (GL_NORMALIZE);
  glEnable (GL_CULL_FACE);

  if (!wire)
    {
      GLfloat pos[4] = {4.0, 1.4, 1.1, 0.0};
      GLfloat amb[4] = {0.0, 0.0, 0.0, 1.0};
      GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
      GLfloat spc[4] = {0.5, 0.5, 0.5, 1.0};

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);
    }

  mi->polygon_count = 0;

  glPushMatrix ();

  {
    double x, y, z;
    double maxz = 50;

    glRotatef (current_device_rotation(), 0, 0, 1);  /* right side up */

    gltrackball_rotate (bp->trackball);

    if (do_wander)
      {
        get_position (bp->rot, &x, &y, &z, !bp->button_down_p);
        glTranslatef((x - 0.5) * 4,
                     (y - 0.5) * 0.2,
                     (z - 0.5) * 8);
      }

    if (do_tilt)
      {
        get_position (bp->rot2, &x, &y, &z, !bp->button_down_p);
        glRotatef (maxz / 2 - z * maxz, 1, 0, 0);
      }

    if (do_spin)
      {
        get_rotation (bp->rot, &x, &y, &z, !bp->button_down_p);
        glRotatef (z * 360, 0, 1, 0);
      }
  }

  glRotatef (90, 1, 0, 0);

# if 0
  {
    static slice S = { 0, }, *s = &S;
    glScalef (20, 20, 20);
    glRotatef (-90, 0, 0, 1);
    if (s->funhole.state == DEAD)
      {
        s->points = (XYZ *) calloc (100, sizeof(*s->points));
        s->points[0].y = 0.1;
        s->funhole.state = HIDDEN;
        s->funhole.ratio = 0;
        s->funhole.speed = 0.04;
      }
    draw_funhole (mi, s, False);
    tick_funhole (s);
  }
# else
  glTranslatef (0, 0, 20);
  glScalef (15, 15, 15);
  for (i = 0; i < bp->nbranches; i++)
    {
      int j;
      branch *b = bp->branches[i];
      glPushMatrix();
      glTranslatef (b->pos.x, b->pos.y, b->pos.z);

      /* Actually carving a hole into the geometry of the tower would be
         tricky, since OpenGL doesn't actually have any notion of volumetric
         3D geometry, only of 2D projections thereof, so there is no way to
         subtract one object from another.  But we can sorta fake it by
         drawing invisible objects into the depth buffer as masks.  The
         illusion doesn't work perfectly, but in this particular application
         it's kind of close...
       */

      /* Lay down the teeth first. */
      mi->polygon_count += b->npolys;
      for (j = 0; j < b->nslices; j++)
        mi->polygon_count += draw_funhole (mi, b->slices[j], False);

      /* Draw into the depth buffer but not the frame buffer:
         mask out the hole to expose the teeth. */
      if (! wire)
        glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
      for (j = 0; j < b->nslices; j++)
        mi->polygon_count += draw_funhole (mi, b->slices[j], True);

      /* Now draw the trunk into the frame buffer only where there's
         already depth. */
      glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      glFrontFace (GL_CCW);
      glCallList (b->dlist);

      glPopMatrix();
    }
# endif

  glPopMatrix ();

  if (! bp->button_down_p &&
      now > bp->last_tick + (1/30.0) / speed)
    {
      tick_chompytower (mi);
      bp->last_tick = now;
    }

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_chompytower (ModeInfo *mi)
{
  chompytower_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  if (bp->trackball) gltrackball_free (bp->trackball);
  if (bp->rot) free_rotator (bp->rot);

  if (bp->component_dlists)
    {
      for (i = 0; i < countof(all_objs); i++)
        if (glIsList(bp->component_dlists[i]))
          glDeleteLists(bp->component_dlists[i], 1);
      free (bp->component_dlists);
    }

  if (glIsList(bp->sphere_dlist)) glDeleteLists(bp->sphere_dlist, 1);

  for (i = 0; i < bp->nbranches; i++)
    free_branch (bp->branches[i]);
  free (bp->branches);
}

XSCREENSAVER_MODULE ("ChompyTower", chompytower)

#endif /* USE_GL */
