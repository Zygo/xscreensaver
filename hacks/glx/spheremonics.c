/* xscreensaver, Copyright (c) 2002 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Algorithm by Paul Bourke <pbourke@swin.edu.au>
 * http://astronomy.swin.edu.au/~pbourke/geometry/sphericalh/
 * Screensaver veneer and parameter selection by jwz.
 *
 *  Paul says:
 *
 * These closed objects are commonly called spherical harmonics,
 * although they are only remotely related to the mathematical
 * definition found in the solution to certain wave functions, most
 * notable the eigenfunctions of angular momentum operators.
 *
 * The formula is quite simple: the form used here is based upon
 * spherical (polar) coordinates (radius, theta, phi).
 *
 *    r = sin(m0 phi)   ^ m1 + 
 *        cos(m2 phi)   ^ m3 + 
 *        sin(m4 theta) ^ m5 + 
 *        cos(m6 theta) ^ m7 
 *
 * Where phi ranges from 0 to pi (lines of latitude), and theta ranges
 * from 0 to 2 pi (lines of longitude), and r is the radius.  The
 * parameters m0, m1, m2, m3, m4, m5, m6, and m7 are all integers
 * greater than or equal to 0.
 *
 * As the degree increases, the objects become increasingly "pointed"
 * and a large number of polygons are required to represent the surface
 * faithfully.
 *
 * jwz adds:
 * 
 * The eight parameters live in the `cc->m' array.
 * Each time we permute the image, we alter *one* of those eight parameters.
 * Each time we alter a parameter, we move it in the same direction (either
 * toward larger or smaller values) in the range [0, 3].
 *
 * By altering only one parameter at a time, and only by small amounts,
 * we tend to produce successive objects that are pretty similar to each
 * other, so you can see a progression.
 *
 * It'd be nice if they were even closer together, so that it looked more
 * like a morph, but, well, that's not how it works.
 *
 * There tends to be a dark stripe in the colormaps.  I don't know why.
 * Perhaps utils/colors.c is at fault?
 *
 * Note that this equation sometimes generates faces that are inside out:
 *     -parameters 01210111
 * To make this work, we need to render back-faces with two-sided lighting:
 * figuring out how to correct the winding and normals on those inside out
 * surfaces would be too hard.
 */

#include <X11/Intrinsic.h>

extern XtAppContext app;

#define PROGCLASS	"Spheremonics"
#define HACK_INIT	init_spheremonics
#define HACK_DRAW	draw_spheremonics
#define HACK_RESHAPE	reshape_spheremonics
#define HACK_HANDLE_EVENT spheremonics_handle_event
#define EVENT_MASK      PointerMotionMask
#define ccs_opts	xlockmore_opts

#define DEF_DURATION    "100"
#define DEF_SPIN        "XYZ"
#define DEF_WANDER      "False"
#define DEF_RESOLUTION  "64"
#define DEF_BBOX        "False"
#define DEF_GRID        "True"
#define DEF_SMOOTH      "True"
#define DEF_PARMS       "(default)"

#define DEFAULTS	"*delay:	30000       \n" \
			"*resolution: " DEF_RESOLUTION "\n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \
			"*duration:   " DEF_DURATION "\n" \
			"*spin:       " DEF_SPIN   "\n" \
			"*wander:     " DEF_WANDER "\n" \
			"*bbox:       " DEF_BBOX   "\n" \
			"*grid:       " DEF_GRID   "\n" \
                        "*smooth:     " DEF_SMOOTH "\n" \
                        "*parameters: " DEF_PARMS  "\n" \

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "colors.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#include <GL/glu.h>

typedef struct {
   double x,y,z;
} XYZ;

typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;

  GLuint dlist, dlist2;
  GLfloat scale;
  XYZ bbox[2];

  int resolution;
  int ncolors;
  XColor *colors;

  int m[8];
  int dm[8];
  int m_max;

  int tracer;
  int mesher;
  int polys1, polys2;  /* polygon counts */

  XFontStruct *font;
  GLuint font_list;

} spheremonics_configuration;

static spheremonics_configuration *ccs = NULL;

static char *do_spin;
static Bool do_wander;
static Bool do_bbox;
static Bool do_grid;
static int smooth_p;
static char *static_parms;
static int res;
static int duration;

static XrmOptionDescRec opts[] = {
  { "-spin",   ".spin",   XrmoptionSepArg, 0 },
  { "+spin",   ".spin",   XrmoptionNoArg, "" },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" },
  { "-resolution", ".resolution", XrmoptionSepArg, 0 },
  { "-duration",   ".duration",   XrmoptionSepArg, 0 },
  { "-bbox",   ".bbox",  XrmoptionNoArg, "True" },
  { "+bbox",   ".bbox",  XrmoptionNoArg, "False" },
  { "-grid",   ".grid",  XrmoptionNoArg, "True" },
  { "+grid",   ".grid",  XrmoptionNoArg, "False" },
  {"-smooth",  ".smooth", XrmoptionNoArg, "True" },
  {"+smooth",  ".smooth", XrmoptionNoArg, "False" },
  { "-parameters", ".parameters", XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {(caddr_t *) &do_spin,   "spin",   "Spin",   DEF_SPIN,   t_String},
  {(caddr_t *) &do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {(caddr_t *) &res,       "resolution", "Resolution", DEF_RESOLUTION, t_Int},
  {(caddr_t *) &duration,  "duration",   "Duration",   DEF_DURATION,   t_Int},
  {(caddr_t *) &do_bbox,   "bbox",   "BBox",   DEF_BBOX,   t_Bool},
  {(caddr_t *) &do_grid,   "grid",   "Grid",   DEF_GRID,   t_Bool},
  {(caddr_t *) &smooth_p,  "smooth", "Smooth", DEF_SMOOTH, t_Bool},
  {(caddr_t *) &static_parms, "parameters", "Parameters", DEF_PARMS, t_String},
};

ModeSpecOpt ccs_opts = {countof(opts), opts, countof(vars), vars, NULL};


/* Window management, etc
 */
void
reshape_spheremonics (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (30.0, 1/h, 1.0, 100.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 0.0, 30.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);

  glClear(GL_COLOR_BUFFER_BIT);
}


static void
gl_init (ModeInfo *mi)
{
/*  spheremonics_configuration *cc = &ccs[MI_SCREEN(mi)]; */
  int wire = MI_IS_WIREFRAME(mi);

  static GLfloat pos[4] = {5.0, 5.0, 10.0, 1.0};

  glEnable(GL_NORMALIZE);

  if (!wire)
    {
      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_DEPTH_TEST);

      /* With objects that have proper winding and normals set up on all
         their faces, one can cull back-faces; however, these equations
         generate objects that are sometimes "inside out", and determining
         whether a facet has been inverted like that is really hard.
         So we render both front and back faces, at a probable performance
         penalty on non-accelerated systems.

         When rendering back faces, we also need to do two-sided lighting,
         or the fact that the normals are flipped gives us too-dark surfaces
         on the inside-out surfaces.

         This isn't generally something you'd want, because you end up
         with half the lighting dynamic range (kind of.)  So if you had
         a sphere with correctly pointing normals, and a single light
         source, it would be illuminated from two sides.  In this case,
         though, it saves us from a difficult and time consuming
         inside/outside test.  And we don't really care about a precise
         lighting effect.
       */
      glDisable(GL_CULL_FACE);
      glLightModeli (GL_LIGHT_MODEL_TWO_SIDE, TRUE);
    }
}



/* generate the object */

static XYZ
sphere_eval (double theta, double phi, int *m)
{
  double r = 0;
  XYZ p;

  r += pow (sin(m[0] * phi),  (double)m[1]);
  r += pow (cos(m[2] * phi),  (double)m[3]);
  r += pow (sin(m[4] * theta),(double)m[5]);
  r += pow (cos(m[6] * theta),(double)m[7]);

  p.x = r * sin(phi) * cos(theta);
  p.y = r * cos(phi);
  p.z = r * sin(phi) * sin(theta);

  return (p);
}


/* Normalise a vector */
static void
normalize (XYZ *p)
{
  double length;
  length = sqrt(p->x * p->x + p->y * p->y + p->z * p->z);
  if (length != 0) {
    p->x /= length;
    p->y /= length;
    p->z /= length;
  } else {
    p->x = 0;
    p->y = 0;
    p->z = 0;
  }       
}

/*-------------------------------------------------------------------------
        Calculate the unit normal at p given two other points 
        p1,p2 on the surface. The normal points in the direction 
        of p1 crossproduct p2
 */
static XYZ
calc_normal (XYZ p, XYZ p1, XYZ p2)
{
  XYZ n, pa, pb;
  pa.x = p1.x - p.x;
  pa.y = p1.y - p.y;
  pa.z = p1.z - p.z;
  pb.x = p2.x - p.x;
  pb.y = p2.y - p.y;
  pb.z = p2.z - p.z;
  n.x = pa.y * pb.z - pa.z * pb.y;
  n.y = pa.z * pb.x - pa.x * pb.z;
  n.z = pa.x * pb.y - pa.y * pb.x;
  normalize (&n);
  return (n);
}


static void
do_color (int i, XColor *colors)
{
  GLfloat c[4];
  c[0] = colors[i].red   / 65535.0;
  c[1] = colors[i].green / 65535.0;
  c[2] = colors[i].blue  / 65535.0;
  c[3] = 1.0;
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, c);
  glColor3f (c[0], c[1], c[2]);
}


static void
draw_circle (ModeInfo *mi, Bool teeth_p)
{
  GLfloat th;
  int tick = 0;
  GLfloat x, y;
  GLfloat step = (M_PI / 180);

  glBegin(GL_LINE_LOOP);
  for (th = 0; th < M_PI*2; th += step*5)
    {
      GLfloat r1 = 0.5;
      x = cos (th);
      y = sin (th);
      glVertex3f(x*r1, y*r1,  0);
    }
  glEnd();

  if (!teeth_p) return;

  glBegin(GL_LINES);
  for (th = 0; th < M_PI*2; th += step)
    {
      GLfloat r1 = 0.5;
      GLfloat r2 = r1 - 0.01;
      if (! (tick % 10))
        r2 -= 0.02;
      else if (! (tick % 5))
        r2 -= 0.01;
      tick++;

      x = cos (th);
      y = sin (th);
      glVertex3f(x*r1, y*r1,  0);
      glVertex3f(x*r2, y*r2,  0);
    }
  glEnd();
}


static void
draw_bounding_box (ModeInfo *mi)
{
  spheremonics_configuration *cc = &ccs[MI_SCREEN(mi)];

  static GLfloat c1[4] = { 0.2, 0.2, 0.6, 1.0 };
  static GLfloat c2[4] = { 1.0, 0.0, 0.0, 1.0 };
  int wire = MI_IS_WIREFRAME(mi);

  GLfloat x1 = cc->bbox[0].x;
  GLfloat y1 = cc->bbox[0].y;
  GLfloat z1 = cc->bbox[0].z;
  GLfloat x2 = cc->bbox[1].x;
  GLfloat y2 = cc->bbox[1].y;
  GLfloat z2 = cc->bbox[1].z;

#if 1
  x1 = y1 = z1 = -0.5;
  x2 = y2 = z2 =  0.5;
#endif

  if (do_bbox && !wire)
    {
      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, c1);
      glFrontFace(GL_CCW);

      glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
      glNormal3f(0, 1, 0);
      glVertex3f(x1, y1, z1); glVertex3f(x1, y1, z2);
      glVertex3f(x2, y1, z2); glVertex3f(x2, y1, z1);
      glEnd();
      glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
      glNormal3f(0, -1, 0);
      glVertex3f(x2, y2, z1); glVertex3f(x2, y2, z2);
      glVertex3f(x1, y2, z2); glVertex3f(x1, y2, z1);
      glEnd();
      glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
      glNormal3f(0, 0, 1);
      glVertex3f(x1, y1, z1); glVertex3f(x2, y1, z1);
      glVertex3f(x2, y2, z1); glVertex3f(x1, y2, z1);
      glEnd();
      glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
      glNormal3f(0, 0, -1);
      glVertex3f(x1, y2, z2); glVertex3f(x2, y2, z2);
      glVertex3f(x2, y1, z2); glVertex3f(x1, y1, z2);
      glEnd();
      glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
      glNormal3f(1, 0, 0);
      glVertex3f(x1, y2, z1); glVertex3f(x1, y2, z2);
      glVertex3f(x1, y1, z2); glVertex3f(x1, y1, z1);
      glEnd();
      glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
      glNormal3f(-1, 0, 0);
      glVertex3f(x2, y1, z1); glVertex3f(x2, y1, z2);
      glVertex3f(x2, y2, z2); glVertex3f(x2, y2, z1);
      glEnd();
    }

  glPushAttrib (GL_LIGHTING);
  glDisable (GL_LIGHTING);

  glColor3f (c2[0], c2[1], c2[2]);

  if (do_grid)
    {
      glPushMatrix();
      glBegin(GL_LINES);
      glVertex3f(0, -0.66, 0);
      glVertex3f(0,  0.66, 0); 
      glEnd();
      draw_circle (mi, True);
      glRotatef(90, 1, 0, 0);
      draw_circle (mi, True);
      glRotatef(90, 0, 1, 0);
      draw_circle (mi, True);
      glPopMatrix();
    }
  else
    {
      glBegin(GL_LINES);
      if (x1 > 0) x1 = 0; if (x2 < 0) x2 = 0;
      if (y1 > 0) y1 = 0; if (y2 < 0) y2 = 0;
      if (z1 > 0) z1 = 0; if (z2 < 0) z2 = 0;
      glVertex3f(x1, 0,  0);  glVertex3f(x2, 0,  0); 
      glVertex3f(0 , y1, 0);  glVertex3f(0,  y2, 0); 
      glVertex3f(0,  0,  z1); glVertex3f(0,  0,  z2); 
      glEnd();
    }

  glPopAttrib();
}


static void
do_tracer (ModeInfo *mi)
{
  spheremonics_configuration *cc = &ccs[MI_SCREEN(mi)];

  if (cc->tracer == -1 &&
      cc->mesher == -1 &&
      !(random() % (duration * 4)))
    {
      if (random() & 1)
        cc->tracer = ((random() & 1) ? 0 : 180);
      else
        cc->mesher = ((random() % ((duration / 3) + 1)) +
                      (random() % ((duration / 3) + 1)));
    }

  if (cc->tracer >= 0)
    {
      int d = (90 - cc->tracer);
      GLfloat th = d * (M_PI / 180);
      GLfloat x = cos (th);
      GLfloat y = sin (th);
      GLfloat s = 1.5 / cc->scale;

      if (s > 0.001)
        {
          static GLfloat c[4] = { 0.6, 0.5, 1.0, 1.0 };

          glPushAttrib (GL_LIGHTING);
          glDisable (GL_LIGHTING);

          glPushMatrix();
          glRotatef (90, 1, 0, 0);
          glTranslatef (0, 0, y*s/2);
          s *= x;
          glScalef(s, s, s);
          glColor3f (c[0], c[1], c[2]);
          draw_circle (mi, False);
          glPopMatrix();

          glPopAttrib();
        }

      cc->tracer += 5;
      if (cc->tracer == 180 || cc->tracer == 360)
        cc->tracer = -1;
    }
}


static int
unit_spheremonics (ModeInfo *mi,
                   int resolution, Bool wire, int *m, XColor *colors)
{
  spheremonics_configuration *cc = &ccs[MI_SCREEN(mi)];
  int polys = 0;
  int i, j;
  double du, dv;
  XYZ q[4];
  XYZ n[4];
  int res = (wire == 2
             ? resolution / 2
             : resolution);

  cc->bbox[0].x = cc->bbox[0].y = cc->bbox[0].z = 0;
  cc->bbox[1].x = cc->bbox[1].y = cc->bbox[1].z = 0;

  du = (M_PI+M_PI) / (double)res; /* Theta */
  dv = M_PI        / (double)res; /* Phi   */

  if (wire)
    glColor3f (1, 1, 1);

  glBegin (wire ? GL_LINE_LOOP : GL_QUADS);

  for (i = 0; i < res; i++) {
    double u = i * du;
    for (j = 0; j < res; j++) {
      double v = j * dv;
      q[0] = sphere_eval (u, v, m);
      n[0] = calc_normal(q[0],
                         sphere_eval (u+du/10, v, m),
                         sphere_eval (u, v+dv/10, m));
      glNormal3f(n[0].x,n[0].y,n[0].z);
      if (!wire) do_color (i, colors);
      glVertex3f(q[0].x,q[0].y,q[0].z);

      q[1] = sphere_eval (u+du, v, m);
      n[1] = calc_normal(q[1],
                         sphere_eval (u+du+du/10, v, m),
                         sphere_eval (u+du, v+dv/10, m));
      glNormal3f(n[1].x,n[1].y,n[1].z);
      if (!wire) do_color ((i+1)%res, colors);
      glVertex3f(q[1].x,q[1].y,q[1].z);

      q[2] = sphere_eval (u+du, v+dv, m);
      n[2] = calc_normal(q[2],
                         sphere_eval (u+du+du/10, v+dv, m),
                         sphere_eval (u+du, v+dv+dv/10, m));
      glNormal3f(n[2].x,n[2].y,n[2].z);
      if (!wire) do_color ((i+1)%res, colors);
      glVertex3f(q[2].x,q[2].y,q[2].z);

      q[3] = sphere_eval (u,v+dv, m);
      n[3] = calc_normal(q[3],
                         sphere_eval (u+du/10, v+dv, m),
                         sphere_eval (u, v+dv+dv/10, m));
      glNormal3f(n[3].x,n[3].y,n[3].z);
      if (!wire) do_color (i, colors);
      glVertex3f(q[3].x,q[3].y,q[3].z);

      polys++;

# define CHECK_BBOX(N) \
         if (q[(N)].x < cc->bbox[0].x) cc->bbox[0].x = q[(N)].x; \
         if (q[(N)].y < cc->bbox[0].y) cc->bbox[0].y = q[(N)].y; \
         if (q[(N)].z < cc->bbox[0].z) cc->bbox[0].z = q[(N)].z; \
         if (q[(N)].x > cc->bbox[1].x) cc->bbox[1].x = q[(N)].x; \
         if (q[(N)].y > cc->bbox[1].y) cc->bbox[1].y = q[(N)].y; \
         if (q[(N)].z > cc->bbox[1].z) cc->bbox[1].z = q[(N)].z

      CHECK_BBOX(0);
      CHECK_BBOX(1);
      CHECK_BBOX(2);
      CHECK_BBOX(3);
# undef CHECK_BBOX
    }
  }
  glEnd();

  {
    GLfloat w = cc->bbox[1].x - cc->bbox[0].x;
    GLfloat h = cc->bbox[1].y - cc->bbox[0].y;
    GLfloat d = cc->bbox[1].z - cc->bbox[0].z;
    GLfloat wh = (w > h ? w : h);
    GLfloat hd = (h > d ? h : d);
    GLfloat scale = (wh > hd ? wh : hd);

    cc->scale = 1/scale;

    if (wire < 2 && (do_bbox || do_grid))
      {
        GLfloat s = scale * 1.5;
        glPushMatrix();
        glScalef(s, s, s);
        draw_bounding_box (mi);
        glPopMatrix();
      }
  }
  return polys;
}


static void
init_colors (ModeInfo *mi)
{
  spheremonics_configuration *cc = &ccs[MI_SCREEN(mi)];
  int i;
  cc->ncolors = cc->resolution;
  cc->colors = (XColor *) calloc(cc->ncolors, sizeof(XColor));
  make_smooth_colormap (0, 0, 0,
                        cc->colors, &cc->ncolors, 
                        False, 0, False);

  /* brighter colors, please... */
  for (i = 0; i < cc->ncolors; i++)
    {
      cc->colors[i].red   = (cc->colors[i].red   / 2) + 32767;
      cc->colors[i].green = (cc->colors[i].green / 2) + 32767;
      cc->colors[i].blue  = (cc->colors[i].blue  / 2) + 32767;
    }
}


/* Pick one of the parameters to the function and tweak it up or down.
 */
static void
tweak_parameters (ModeInfo *mi)
{
  spheremonics_configuration *cc = &ccs[MI_SCREEN(mi)];

  /* If the -parameters command line option was specified, just use that
     all the time.
   */
  if (static_parms &&
      *static_parms &&
      !!strcasecmp (static_parms, "(default)"))
    {
      unsigned long n;
      char dummy;
      if (8 == sscanf (static_parms, "%d %d %d %d %d %d %d %d %c",
                       &cc->m[0], &cc->m[1], &cc->m[2], &cc->m[3],
                       &cc->m[4], &cc->m[5], &cc->m[6], &cc->m[7],
                       &dummy))
        return;
      else if (strlen (static_parms) == 8 &&
               1 == sscanf (static_parms, "%lu %c", &n, &dummy))
        {
          const char *s = static_parms;
          int i = 0;
          while (*s)
            cc->m[i++] = (*s++)-'0';
          return;
        }
      fprintf (stderr,
               "%s: -parameters must be a string of 8 ints (not \"%s\")\n",
               progname, static_parms);
      exit (1);
    }

  static_parms = 0;


# define SHIFT(N) do { \
    int n = (N); \
    cc->m[n] += cc->dm[n]; \
    if (cc->m[n] <= 0) \
      cc->m[n] = 0, cc->dm[n] = -cc->dm[n]; \
    else if (cc->m[n] >= cc->m_max) \
      cc->m[n] = cc->m_max, cc->dm[n] = -cc->dm[n]; \
  } while(0)

/*    else if (cc->m[n] >= cc->m_max/2 && (! (random() % 3))) \
      cc->m[n] = cc->m_max/2, cc->dm[n] = -cc->dm[n]; \
*/

  switch(random() % 8)
    {
    case 0: SHIFT(0); break;
    case 1: SHIFT(1); break;
    case 2: SHIFT(2); break;
    case 3: SHIFT(3); break;
    case 4: SHIFT(4); break;
    case 5: SHIFT(5); break;
    case 6: SHIFT(6); break;
    case 7: SHIFT(7); break;
    default: abort(); break;
    }
# undef SHIFT

#if 0
    printf ("%s: state: %d %d %d %d %d %d %d %d\n",
            progname,
            cc->m[0], cc->m[1], cc->m[2], cc->m[3],
            cc->m[4], cc->m[5], cc->m[6], cc->m[7]);
#endif

}


static void
generate_spheremonics (ModeInfo *mi)
{
  spheremonics_configuration *cc = &ccs[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);

  tweak_parameters (mi);

  {
    static Bool done = False;
    if (!done || (0 == (random() % 20)))
      {
        init_colors (mi);
        done = True;
      }
  }

  {
    glNewList(cc->dlist, GL_COMPILE);
    cc->polys1 = unit_spheremonics (mi, cc->resolution, wire,cc->m,cc->colors);
    glEndList();

    glNewList(cc->dlist2, GL_COMPILE);
    glPushAttrib (GL_LIGHTING);
    glDisable (GL_LIGHTING);
    glPushMatrix();
    glScalef (1.05, 1.05, 1.05);
    cc->polys2 = unit_spheremonics (mi, cc->resolution, 2, cc->m, cc->colors);
    glPopMatrix();
    glPopAttrib();
    glEndList();
  }
}




static void
load_font (ModeInfo *mi, char *res, XFontStruct **fontP, GLuint *dlistP)
{
  const char *font = get_string_resource (res, "Font");
  XFontStruct *f;
  Font id;
  int first, last;

  if (!font) font = "-*-times-bold-r-normal-*-140-*";

  f = XLoadQueryFont(mi->dpy, font);
  if (!f) f = XLoadQueryFont(mi->dpy, "fixed");

  id = f->fid;
  first = f->min_char_or_byte2;
  last = f->max_char_or_byte2;
  
  clear_gl_error ();
  *dlistP = glGenLists ((GLuint) last+1);
  check_gl_error ("glGenLists");
  glXUseXFont(id, first, last-first+1, *dlistP + first);
  check_gl_error ("glXUseXFont");

  *fontP = f;
}

static void
draw_label (ModeInfo *mi, const char *s)
{
  spheremonics_configuration *cc = &ccs[MI_SCREEN(mi)];
  unsigned int i;
  
  glPushAttrib(GL_TRANSFORM_BIT | GL_ENABLE_BIT);
  glDisable(GL_LIGHTING);
  glDisable(GL_DEPTH_TEST);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  gluOrtho2D(0, mi->xgwa.width, 0, mi->xgwa.height);
  glColor3f(1.0, 1.0, 0.0);

  glRasterPos2f (10,
                 (mi->xgwa.height
                  - 10
                  - (cc->font->ascent + cc->font->descent)));
  for (i = 0; i < strlen(s); i++)
    glCallList (cc->font_list + (int)s[i]);

  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glPopAttrib();
}




void 
init_spheremonics (ModeInfo *mi)
{
  spheremonics_configuration *cc;

  if (!ccs) {
    ccs = (spheremonics_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (spheremonics_configuration));
    if (!ccs) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }

    cc = &ccs[MI_SCREEN(mi)];
  }

  cc = &ccs[MI_SCREEN(mi)];

  if ((cc->glx_context = init_GL(mi)) != NULL) {
    gl_init(mi);
    reshape_spheremonics (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  }

  if (smooth_p) 
    {
      glEnable (GL_LINE_SMOOTH);
      glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);
      glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
      glEnable (GL_BLEND);
    }

  {
    Bool spinx=False, spiny=False, spinz=False;
    double spin_speed   = 1.0;
    double wander_speed = 0.03;

    char *s = do_spin;
    while (*s)
      {
        if      (*s == 'x' || *s == 'X') spinx = True;
        else if (*s == 'y' || *s == 'Y') spiny = True;
        else if (*s == 'z' || *s == 'Z') spinz = True;
        else
          {
            fprintf (stderr,
         "%s: spin must contain only the characters X, Y, or Z (not \"%s\")\n",
                     progname, do_spin);
            exit (1);
          }
        s++;
      }

    cc->rot = make_rotator (spinx ? spin_speed : 0,
                            spinz ? spin_speed : 0,
                            spiny ? spin_speed : 0,
                            1.0,
                            do_wander ? wander_speed : 0,
                            (spinx && spiny && spinz));
    cc->trackball = gltrackball_init ();
  }

  cc->tracer = -1;
  cc->mesher = -1;

  cc->resolution = res;

  load_font (mi, "labelfont", &cc->font, &cc->font_list);

  cc->dlist = glGenLists(1);
  cc->dlist2 = glGenLists(1);

  cc->m_max = 4; /* 9? */
  {
    unsigned int i;
    for (i = 0; i < countof(cc->dm); i++)
      cc->dm[i] = 1;  /* going up! */

    /* Generate a few more times so we don't always start off with a sphere */
    for (i = 0; i < 5; i++)
      tweak_parameters (mi);
  }

  generate_spheremonics(mi);
}


Bool
spheremonics_handle_event (ModeInfo *mi, XEvent *event)
{
  spheremonics_configuration *cc = &ccs[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button & Button1)
    {
      cc->button_down_p = True;
      gltrackball_start (cc->trackball,
                         event->xbutton.x, event->xbutton.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button & Button1)
    {
      cc->button_down_p = False;
      return True;
    }
  else if (event->xany.type == MotionNotify &&
           cc->button_down_p)
    {
      gltrackball_track (cc->trackball,
                         event->xmotion.x, event->xmotion.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }

  return False;
}


void
draw_spheremonics (ModeInfo *mi)
{
  spheremonics_configuration *cc = &ccs[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  if (!cc->glx_context)
    return;

  glShadeModel(GL_SMOOTH);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  glScalef(1.1, 1.1, 1.1);

  {
    double x, y, z;
    get_position (cc->rot, &x, &y, &z, !cc->button_down_p);
    glTranslatef((x - 0.5) * 8,
                 (y - 0.5) * 6,
                 (z - 0.5) * 8);

    gltrackball_rotate (cc->trackball);

    get_rotation (cc->rot, &x, &y, &z, !cc->button_down_p);
    glRotatef (x * 360, 1.0, 0.0, 0.0);
    glRotatef (y * 360, 0.0, 1.0, 0.0);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }

  glScalef(7,7,7);

  mi->polygon_count = 0;

  glScalef (cc->scale, cc->scale, cc->scale);
  glCallList (cc->dlist);
  mi->polygon_count += cc->polys1;

  if (cc->mesher >= 0 /* || mouse_p */)
    {
      glCallList (cc->dlist2);
      mi->polygon_count += cc->polys2;
      if (cc->mesher >= 0)
        cc->mesher--;
    }
  do_tracer(mi);


  if (cc->button_down_p)
    {
      char buf[200];
      sprintf (buf,
               ((cc->m[0]<10 && cc->m[1]<10 && cc->m[2]<10 && cc->m[3]<10 &&
                 cc->m[4]<10 && cc->m[5]<10 && cc->m[6]<10 && cc->m[7]<10)
                ? "%d%d%d%d%d%d%d%d"
                : "%d %d %d %d %d %d %d %d"),
               cc->m[0], cc->m[1], cc->m[2], cc->m[3],
               cc->m[4], cc->m[5], cc->m[6], cc->m[7]);
      draw_label (mi, buf);
    }

  if (!static_parms)
    {
      static int tick = 0;
      if (tick++ >= duration && !cc->button_down_p)
        {
          generate_spheremonics(mi);
          tick = 0;
          cc->mesher = -1;  /* turn off the mesh when switching objects */
        }
    }

  glPopMatrix();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

#endif /* USE_GL */
