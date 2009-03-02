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
 * With the hairier objects, some of the faces are inside out.  E.g.,
 *     -parameters 01210111
 * If we turn off GL_CULL_FACE, that object renders more solidly
 * (indicating wrong winding) and the altered surfaces are too dark
 * (indicating wrong normals.)
 */

#include <X11/Intrinsic.h>

extern XtAppContext app;

#define PROGCLASS	"Spheremonics"
#define HACK_INIT	init_spheremonics
#define HACK_DRAW	draw_spheremonics
#define HACK_RESHAPE	reshape_spheremonics
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
#include <ctype.h>

#ifdef USE_GL /* whole file */

#include <GL/glu.h>

typedef struct {
   double x,y,z;
} XYZ;

typedef struct {
  GLXContext *glx_context;

  GLfloat rotx, roty, rotz;	   /* current object rotation */
  GLfloat dx, dy, dz;		   /* current rotational velocity */
  GLfloat ddx, ddy, ddz;	   /* current rotational acceleration */
  GLfloat d_max;		   /* max velocity */
  Bool spin_x, spin_y, spin_z;

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

  gluPerspective( 30.0, 1/h, 1.0, 100.0 );
  gluLookAt( 0.0, 0.0, 15.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.0, 0.0, -15.0);

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
      glEnable(GL_CULL_FACE);
      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_DEPTH_TEST);
    }
}


/* lifted from lament.c */
#define RAND(n) ((long) ((random() & 0x7fffffff) % ((long) (n))))
#define RANDSIGN() ((random() & 1) ? 1 : -1)

static void
rotate(GLfloat *pos, GLfloat *v, GLfloat *dv, GLfloat max_v)
{
  double ppos = *pos;

  /* tick position */
  if (ppos < 0)
    ppos = -(ppos + *v);
  else
    ppos += *v;

  if (ppos > 1.0)
    ppos -= 1.0;
  else if (ppos < 0)
    ppos += 1.0;

  if (ppos < 0) abort();
  if (ppos > 1.0) abort();
  *pos = (*pos > 0 ? ppos : -ppos);

  /* accelerate */
  *v += *dv;

  /* clamp velocity */
  if (*v > max_v || *v < -max_v)
    {
      *dv = -*dv;
    }
  /* If it stops, start it going in the other direction. */
  else if (*v < 0)
    {
      if (random() % 4)
	{
	  *v = 0;

	  /* keep going in the same direction */
	  if (random() % 2)
	    *dv = 0;
	  else if (*dv < 0)
	    *dv = -*dv;
	}
      else
	{
	  /* reverse gears */
	  *v = -*v;
	  *dv = -*dv;
	  *pos = -*pos;
	}
    }

  /* Alter direction of rotational acceleration randomly. */
  if (! (random() % 120))
    *dv = -*dv;

  /* Change acceleration very occasionally. */
  if (! (random() % 200))
    {
      if (*dv == 0)
	*dv = 0.00001;
      else if (random() & 1)
	*dv *= 1.2;
      else
	*dv *= 0.8;
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


static void
unit_spheremonics (ModeInfo *mi,
                   int resolution, Bool wire, int *m, XColor *colors)
{
  spheremonics_configuration *cc = &ccs[MI_SCREEN(mi)];

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

/*  mi->polygon_count = 0; */

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

/*      mi->polygon_count++; */

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
    unit_spheremonics (mi, cc->resolution, wire, cc->m, cc->colors);
    glEndList();

    glNewList(cc->dlist2, GL_COMPILE);
    glPushAttrib (GL_LIGHTING);
    glDisable (GL_LIGHTING);
    glPushMatrix();
    glScalef (1.05, 1.05, 1.05);
    unit_spheremonics (mi, cc->resolution, 2, cc->m, cc->colors);
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
  int i;
  
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
    char *s = do_spin;
    while (*s)
      {
        if      (*s == 'x' || *s == 'X') cc->spin_x = 1;
        else if (*s == 'y' || *s == 'Y') cc->spin_y = 1;
        else if (*s == 'z' || *s == 'Z') cc->spin_z = 1;
        else
          {
            fprintf (stderr,
         "%s: spin must contain only the characters X, Y, or Z (not \"%s\")\n",
                     progname, do_spin);
            exit (1);
          }
        s++;
      }
  }

  cc->rotx = frand(1.0) * RANDSIGN();
  cc->roty = frand(1.0) * RANDSIGN();
  cc->rotz = frand(1.0) * RANDSIGN();

  /* bell curve from 0-6 degrees, avg 3 */
  cc->dx = (frand(0.4) + frand(0.4) + frand(0.4)) / (360/2);
  cc->dy = (frand(0.4) + frand(0.4) + frand(0.4)) / (360/2);
  cc->dz = (frand(0.4) + frand(0.4) + frand(0.4)) / (360/2);

  cc->d_max = cc->dx * 2;

  cc->ddx = 0.00006 + frand(0.00003);
  cc->ddy = 0.00006 + frand(0.00003);
  cc->ddz = 0.00006 + frand(0.00003);
  cc->tracer = -1;
  cc->mesher = -1;

  cc->resolution = res;

  load_font (mi, "labelfont", &cc->font, &cc->font_list);

  cc->dlist = glGenLists(1);
  cc->dlist2 = glGenLists(1);

  cc->m_max = 4; /* 9? */
  {
    int i;
    for (i = 0; i < countof(cc->dm); i++)
      cc->dm[i] = 1;  /* going up! */

    /* Generate a few more times so we don't always start off with a sphere */
    for (i = 0; i < 5; i++)
      tweak_parameters (mi);
  }

  generate_spheremonics(mi);
}


static Bool
mouse_down_p (ModeInfo *mi)
{
  Window root, child;
  int rx, ry, wx, wy;
  unsigned int mask;
  if (!XQueryPointer (MI_DISPLAY(mi), MI_WINDOW(mi),
                      &root, &child, &rx, &ry, &wx, &wy, &mask))
    return False;
  if (! (mask & Button1Mask))
    return False;
  return True;
}


void
draw_spheremonics (ModeInfo *mi)
{
  spheremonics_configuration *cc = &ccs[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  Bool mouse_p;

  if (!cc->glx_context)
    return;

  mouse_p = mouse_down_p (mi);

  glShadeModel(GL_SMOOTH);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  glScalef(1.1, 1.1, 1.1);

  {
    GLfloat x, y, z;

    if (do_wander)
      {
        static int frame = 0;

#       define SINOID(SCALE,SIZE) \
        ((((1 + sin((frame * (SCALE)) / 2 * M_PI)) / 2.0) * (SIZE)) - (SIZE)/2)

        x = SINOID(0.0071, 8.0);
        y = SINOID(0.0053, 6.0);
        z = SINOID(0.0037, 15.0);
        frame++;
        glTranslatef(x, y, z);
      }

    if (cc->spin_x || cc->spin_y || cc->spin_z)
      {
        x = cc->rotx;
        y = cc->roty;
        z = cc->rotz;
        if (x < 0) x = 1 - (x + 1);
        if (y < 0) y = 1 - (y + 1);
        if (z < 0) z = 1 - (z + 1);

        if (cc->spin_x) glRotatef(x * 360, 1.0, 0.0, 0.0);
        if (cc->spin_y) glRotatef(y * 360, 0.0, 1.0, 0.0);
        if (cc->spin_z) glRotatef(z * 360, 0.0, 0.0, 1.0);

        rotate(&cc->rotx, &cc->dx, &cc->ddx, cc->d_max);
        rotate(&cc->roty, &cc->dy, &cc->ddy, cc->d_max);
        rotate(&cc->rotz, &cc->dz, &cc->ddz, cc->d_max);
      }
  }

  glScalef(7,7,7);

  glScalef (cc->scale, cc->scale, cc->scale);
  glCallList (cc->dlist);
  if (cc->mesher >= 0 /* || mouse_p */)
    {
      glCallList (cc->dlist2);
      if (cc->mesher >= 0)
        cc->mesher--;
    }
  do_tracer(mi);


  if (mouse_p)
    {
      char buf[200];
      sprintf (buf, "%d %d %d %d %d %d %d %d",
               cc->m[0], cc->m[1], cc->m[2], cc->m[3],
               cc->m[4], cc->m[5], cc->m[6], cc->m[7]);
      draw_label (mi, buf);
    }

  if (!static_parms)
    {
      static int tick = 0;
      if (tick++ == duration)
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
