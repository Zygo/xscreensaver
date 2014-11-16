/* geodesic, Copyright (c) 2013-2014 Jamie Zawinski <jwz@jwz.org>
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
			"*count:        4           \n" \
			"*showFPS:      False       \n"

# define refresh_geodesic 0
# define release_geodesic 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "colors.h"
#include "normals.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#include "gllist.h"

#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_SPEED       "1.0"
#define DEF_MODE        "mesh"

typedef struct { double a, o; } LL;	/* latitude + longitude */

typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;
  int ncolors;
  XColor *colors;
  int ccolor, ccolor2;
  GLfloat color1[4], color2[4];

  GLfloat depth;
  GLfloat delta;

  GLfloat thickness;
  GLfloat thickdelta;

  GLfloat morph_ratio;

  Bool random_p;
  enum { WIRE, MESH, SOLID, STELLATED, STELLATED2 } mode;

} geodesic_configuration;

static geodesic_configuration *bps = NULL;

static Bool do_spin;
static GLfloat speed;
static Bool do_wander;
static char *mode_str;

static XrmOptionDescRec opts[] = {
  { "-spin",      ".spin",   XrmoptionNoArg, "True"  },
  { "+spin",      ".spin",   XrmoptionNoArg, "False" },
  { "-speed",     ".speed",  XrmoptionSepArg, 0      },
  { "-wander",    ".wander", XrmoptionNoArg, "True"  },
  { "+wander",    ".wander", XrmoptionNoArg, "False" },
  { "-mode",      ".mode",   XrmoptionSepArg, 0      },
  { "-wireframe", ".mode",   XrmoptionNoArg, "wire"  },
};

static argtype vars[] = {
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_Bool},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
  {&mode_str,   "mode",  "Mode",   DEF_MODE,   t_String},
};

ENTRYPOINT ModeSpecOpt geodesic_opts = {
  countof(opts), opts, countof(vars), vars, NULL};


/* Renders a triangle specified by 3 cartesian endpoints.
 */
static void
triangle0 (ModeInfo *mi, XYZ p1, XYZ p2, XYZ p3)
{
  geodesic_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  GLfloat r = bp->thickness;

  if (bp->mode == SOLID || bp->mode == STELLATED || bp->mode == STELLATED2)
    r = 1;

  if (r <= 0.001) r = 0.001;

  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, bp->color1);

  if (wire) r = 1;

  if (r <= 0)
    ;
  else if (r >= 1)	/* solid triangular face */
    {
      glFrontFace (GL_CCW);
      glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLES);
      do_normal (p1.x, p1.y, p1.z,
                 p2.x, p2.y, p2.z,
                 p3.x, p3.y, p3.z);
      glVertex3f (p1.x, p1.y, p1.z);
      glVertex3f (p2.x, p2.y, p2.z);
      glVertex3f (p3.x, p3.y, p3.z);
      glEnd();
      mi->polygon_count++;
    }
  else			/* mesh: triangular face with a triangular hole */
    {
      XYZ p1b, p2b, p3b, c;
      GLfloat d = 0.98;

      c.x = (p1.x + p2.x + p3.x) / 3;
      c.y = (p1.y + p2.y + p3.y) / 3;
      c.z = (p1.z + p2.z + p3.z) / 3;

      p1b.x = p1.x + (r * (c.x - p1.x));
      p1b.y = p1.y + (r * (c.y - p1.y));
      p1b.z = p1.z + (r * (c.z - p1.z));

      p2b.x = p2.x + (r * (c.x - p2.x));
      p2b.y = p2.y + (r * (c.y - p2.y));
      p2b.z = p2.z + (r * (c.z - p2.z));

      p3b.x = p3.x + (r * (c.x - p3.x));
      p3b.y = p3.y + (r * (c.y - p3.y));
      p3b.z = p3.z + (r * (c.z - p3.z));

      /* Outside faces */

      do_normal (p1.x, p1.y, p1.z,
                 p2.x, p2.y, p2.z,
                 p3.x, p3.y, p3.z);

      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glVertex3f (p1.x,  p1.y,  p1.z);
      glVertex3f (p1b.x, p1b.y, p1b.z);
      glVertex3f (p3b.x, p3b.y, p3b.z);
      glVertex3f (p3.x,  p3.y,  p3.z);
      mi->polygon_count++;

      glVertex3f (p1.x,  p1.y,  p1.z);
      glVertex3f (p2.x,  p2.y,  p2.z);
      glVertex3f (p2b.x, p2b.y, p2b.z);
      glVertex3f (p1b.x, p1b.y, p1b.z);
      mi->polygon_count++;

      glVertex3f (p2.x,  p2.y,  p2.z);
      glVertex3f (p3.x,  p3.y,  p3.z);
      glVertex3f (p3b.x, p3b.y, p3b.z);
      glVertex3f (p2b.x, p2b.y, p2b.z);
      mi->polygon_count++;
      glEnd();

      /* Inside faces */

      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, bp->color2);

      do_normal (p3.x,  p3.y,  p3.z,
                 p3b.x, p3b.y, p3b.z,
                 p1b.x, p1b.y, p1b.z);

      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glVertex3f (d * p3.x,  d * p3.y,  d * p3.z);
      glVertex3f (d * p3b.x, d * p3b.y, d * p3b.z);
      glVertex3f (d * p1b.x, d * p1b.y, d * p1b.z);
      glVertex3f (d * p1.x,  d * p1.y,  d * p1.z);
      mi->polygon_count++;

      glVertex3f (d * p1b.x, d * p1b.y, d * p1b.z);
      glVertex3f (d * p2b.x, d * p2b.y, d * p2b.z);
      glVertex3f (d * p2.x,  d * p2.y,  d * p2.z);
      glVertex3f (d * p1.x,  d * p1.y,  d * p1.z);
      mi->polygon_count++;

      glVertex3f (d * p2b.x, d * p2b.y, d * p2b.z);
      glVertex3f (d * p3b.x, d * p3b.y, d * p3b.z);
      glVertex3f (d * p3.x,  d * p3.y,  d * p3.z);
      glVertex3f (d * p2.x,  d * p2.y,  d * p2.z);
      mi->polygon_count++;
      glEnd();


      /* Connecting edges */

      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, bp->color1);

      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);

      do_normal (p1b.x, p1b.y, p1b.z,
                 p2b.x, p2b.y, p2b.z,
                 p2b.x * d, p2b.y * d, p2b.z * d);
      glVertex3f (p1b.x, p1b.y, p1b.z);
      glVertex3f (p2b.x, p2b.y, p2b.z);
      glVertex3f (p2b.x * d, p2b.y * d, p2b.z * d);
      glVertex3f (p1b.x * d, p1b.y * d, p1b.z * d);
      mi->polygon_count++;
      
      do_normal (p2b.x, p2b.y, p2b.z,
                 p3b.x, p3b.y, p3b.z,
                 p3b.x * d, p3b.y * d, p3b.z * d);
      glVertex3f (p2b.x, p2b.y, p2b.z);
      glVertex3f (p3b.x, p3b.y, p3b.z);
      glVertex3f (p3b.x * d, p3b.y * d, p3b.z * d);
      glVertex3f (p2b.x * d, p2b.y * d, p2b.z * d);
      mi->polygon_count++;

      do_normal (p3b.x, p3b.y, p3b.z,
                 p1b.x, p1b.y, p1b.z,
                 p1b.x * d, p1b.y * d, p1b.z * d);
      glVertex3f (p3b.x, p3b.y, p3b.z);
      glVertex3f (p1b.x, p1b.y, p1b.z);
      glVertex3f (p1b.x * d, p1b.y * d, p1b.z * d);
      glVertex3f (p3b.x * d, p3b.y * d, p3b.z * d);
      mi->polygon_count++;
      glEnd();
    }
}


/* Renders a triangle specified by 3 polar endpoints.
 */
static void
triangle1 (ModeInfo *mi, LL v1, LL v2, LL v3)
{
  XYZ p1, p2, p3;

  p1.x = cos (v1.a) * cos (v1.o);
  p1.y = cos (v1.a) * sin (v1.o);
  p1.z = sin (v1.a);

  p2.x = cos (v2.a) * cos (v2.o);
  p2.y = cos (v2.a) * sin (v2.o);
  p2.z = sin (v2.a);

  p3.x = cos (v3.a) * cos (v3.o);
  p3.y = cos (v3.a) * sin (v3.o);
  p3.z = sin (v3.a);

  triangle0 (mi, p1, p2, p3);
}


/* Computes the midpoint of a line between two polar coords.
 */
static void
midpoint2 (LL v1, LL v2, LL *vm_ret,
           XYZ *p1_ret, XYZ *p2_ret, XYZ *pm_ret)
{
  XYZ p1, p2, pm;
  LL vm;
  GLfloat hyp;

  p1.x = cos (v1.a) * cos (v1.o);
  p1.y = cos (v1.a) * sin (v1.o);
  p1.z = sin (v1.a);

  p2.x = cos (v2.a) * cos (v2.o);
  p2.y = cos (v2.a) * sin (v2.o);
  p2.z = sin (v2.a);

  pm.x = (p1.x + p2.x) / 2;
  pm.y = (p1.y + p2.y) / 2;
  pm.z = (p1.z + p2.z) / 2;

  vm.o = atan2 (pm.y, pm.x);
  hyp = sqrt (pm.x * pm.x + pm.y * pm.y);
  vm.a = atan2 (pm.z, hyp);

  *p1_ret = p1;
  *p2_ret = p2;
  *pm_ret = pm;
  *vm_ret = vm;
}


/* Computes the midpoint of a triangle specified in polar coords.
 */
static void
midpoint3 (LL v1, LL v2, LL v3, LL *vm_ret,
           XYZ *p1_ret, XYZ *p2_ret, XYZ *p3_ret, XYZ *pm_ret)
{
  XYZ p1, p2, p3, pm;
  LL vm;
  GLfloat hyp;

  p1.x = cos (v1.a) * cos (v1.o);
  p1.y = cos (v1.a) * sin (v1.o);
  p1.z = sin (v1.a);

  p2.x = cos (v2.a) * cos (v2.o);
  p2.y = cos (v2.a) * sin (v2.o);
  p2.z = sin (v2.a);

  p3.x = cos (v3.a) * cos (v3.o);
  p3.y = cos (v3.a) * sin (v3.o);
  p3.z = sin (v3.a);

  pm.x = (p1.x + p2.x + p3.x) / 3;
  pm.y = (p1.y + p2.y + p3.y) / 3;
  pm.z = (p1.z + p2.z + p3.z) / 3;

  vm.o = atan2 (pm.y, pm.x);
  hyp = sqrt (pm.x * pm.x + pm.y * pm.y);
  vm.a = atan2 (pm.z, hyp);

  *p1_ret = p1;
  *p2_ret = p2;
  *p3_ret = p3;
  *pm_ret = pm;
  *vm_ret = vm;
}


/* Renders a triangular geodesic facet to the given depth.
 */
static void
triangle (ModeInfo *mi, LL v1, LL v2, LL v3, int depth)
{
  geodesic_configuration *bp = &bps[MI_SCREEN(mi)];

  if (depth <= 0)
    triangle1 (mi, v1, v2, v3);
  else
    {
      LL v12, v23, v13;
      XYZ p1, p2, p3, p12, p23, p13;
      GLfloat r = bp->morph_ratio;

      midpoint2 (v1, v2, &v12, &p1, &p2, &p12);
      midpoint2 (v2, v3, &v23, &p2, &p3, &p23);
      midpoint2 (v1, v3, &v13, &p1, &p3, &p13);
      depth--;

      if (depth == 0 && 
          r != 0 &&
          (bp->mode == STELLATED || bp->mode == STELLATED2))
        {			/* morph between flat and stellated faces */
          XYZ pc, pc2;
          LL vc;
          midpoint3 (v1, v2, v3, &vc, &p1, &p2, &p3, &pc);

          pc2.x = cos (vc.a) * cos (vc.o);
          pc2.y = cos (vc.a) * sin (vc.o);
          pc2.z = sin (vc.a);

          pc.x = pc.x + r * (pc2.x - pc.x);
          pc.y = pc.y + r * (pc2.y - pc.y);
          pc.z = pc.z + r * (pc2.z - pc.z);

          triangle0 (mi, p1, p2, pc);
          triangle0 (mi, p2, p3, pc);
          triangle0 (mi, p3, p1, pc);
        }
      else if (depth == 0 && r < 1)
        {			/* morph between flat and sphere-oid faces */
          XYZ p12b, p23b, p13b;

          p12b.x = cos (v12.a) * cos (v12.o);
          p12b.y = cos (v12.a) * sin (v12.o);
          p12b.z = sin (v12.a);

          p23b.x = cos (v23.a) * cos (v23.o);
          p23b.y = cos (v23.a) * sin (v23.o);
          p23b.z = sin (v23.a);

          p13b.x = cos (v13.a) * cos (v13.o);
          p13b.y = cos (v13.a) * sin (v13.o);
          p13b.z = sin (v13.a);

          p12.x = p12.x + r * (p12b.x - p12.x);
          p12.y = p12.y + r * (p12b.y - p12.y);
          p12.z = p12.z + r * (p12b.z - p12.z);

          p23.x = p23.x + r * (p23b.x - p23.x);
          p23.y = p23.y + r * (p23b.y - p23.y);
          p23.z = p23.z + r * (p23b.z - p23.z);

          p13.x = p13.x + r * (p13b.x - p13.x);
          p13.y = p13.y + r * (p13b.y - p13.y);
          p13.z = p13.z + r * (p13b.z - p13.z);

          triangle0 (mi, p1,  p12, p13);
          triangle0 (mi, p12, p2,  p23);
          triangle0 (mi, p13, p23, p3);
          triangle0 (mi, p12, p23, p13);
        }
      else
        {
          triangle (mi, v1,  v12, v13, depth);
          triangle (mi, v12, v2,  v23, depth);
          triangle (mi, v13, v23, v3,  depth);
          triangle (mi, v12, v23, v13, depth);
        }
    }
}


/* Renders a geodesic sphere to the given depth (frequency).
 */
static void
make_geodesic (ModeInfo *mi, int depth)
{
  GLfloat th0 = atan (0.5);  /* lat division: 26.57 deg */
  GLfloat s = M_PI / 5;	     /* lon division: 72 deg    */
  int i;

  for (i = 0; i < 10; i++)
    {
      GLfloat th1 = s * i;
      GLfloat th2 = s * (i+1);
      GLfloat th3 = s * (i+2);
      LL v1, v2, v3, vc;
      v1.a = th0;    v1.o = th1;
      v2.a = th0;    v2.o = th3;
      v3.a = -th0;   v3.o = th2;
      vc.a = M_PI/2; vc.o = th2;

      if (i & 1)			/* north */
        {
          triangle (mi, v1, v2, vc, depth);
          triangle (mi, v2, v1, v3, depth);
        }
      else				/* south */
        {
          v1.a = -v1.a;
          v2.a = -v2.a;
          v3.a = -v3.a;
          vc.a = -vc.a;
          triangle (mi, v2, v1, vc, depth);
          triangle (mi, v1, v2, v3, depth);
        }
    }
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_geodesic (ModeInfo *mi, int width, int height)
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



ENTRYPOINT void 
init_geodesic (ModeInfo *mi)
{
  geodesic_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);

  if (!bps) {
    bps = (geodesic_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (geodesic_configuration));
    if (!bps) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }
  }

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_geodesic (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  /* This comes first because it sets wire. */
  if (!mode_str || !*mode_str)
    mode_str = DEF_MODE;
  if (!strcasecmp(mode_str, "random")) {
    bp->random_p = 1;
    bp->mode = MESH + (random() % (STELLATED2 - MESH + 1));
  } else if (!strcasecmp(mode_str, "mesh")) {
    bp->mode = MESH;
  } else if (!strcasecmp(mode_str, "solid")) {
    bp->mode = SOLID;
  } else if (!strcasecmp(mode_str, "stellated")) {
    bp->mode = STELLATED;
  } else if (!strcasecmp(mode_str, "stellated2")) {
    bp->mode = STELLATED2;
  } else if (!strcasecmp(mode_str, "wire")) {
    bp->mode = WIRE;
    MI_IS_WIREFRAME(mi) = wire = 1;
  } else {
    fprintf (stderr, "%s: unknown mode: %s\n", progname, mode_str);
    exit (1);
  }


  {
    static GLfloat cspec[4] = {1.0, 1.0, 1.0, 1.0};
    static const GLfloat shiny = 128.0;

    static GLfloat pos[4] = {1.0, 1.0, 1.0, 0.0};
    static GLfloat amb[4] = {0.0, 0.0, 0.0, 1.0};
    static GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
    static GLfloat spc[4] = {0.0, 1.0, 1.0, 1.0};

    glLightfv(GL_LIGHT0, GL_POSITION, pos);
    glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
    glLightfv(GL_LIGHT0, GL_SPECULAR, spc);

    glMaterialfv (GL_FRONT, GL_SPECULAR,  cspec);
    glMateriali  (GL_FRONT, GL_SHININESS, shiny);

    glLineWidth (3);
  }

  if (! wire)
    {
      glEnable (GL_DEPTH_TEST);
      glEnable (GL_BLEND);
      glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

  /* Actually this looks pretty good in -wire with lighting! */
  glEnable (GL_LIGHTING);
  glEnable (GL_LIGHT0);

  if (! bp->rot)
  {
    double spin_speed   = 0.25  * speed;
    double wander_speed = 0.01 * speed;
    double spin_accel   = 0.2;

    bp->rot = make_rotator (do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            spin_accel,
                            do_wander ? wander_speed : 0,
                            True);
    bp->trackball = gltrackball_init (True);
  }

  if (MI_COUNT(mi) < 1) MI_COUNT(mi) = 1;

  bp->ncolors = 1024;
  if (! bp->colors)
    bp->colors = (XColor *) calloc(bp->ncolors, sizeof(XColor));
  make_smooth_colormap (0, 0, 0,
                        bp->colors, &bp->ncolors,
                        False, 0, False);
  bp->ccolor = 0;
  bp->depth = 1;	/* start 1 up from the icosahedron */
  bp->delta = 0.003;

#if 0
  bp->thickness = 1;
  bp->thickdelta = 0.0007;
#else
  bp->thickness = 0.1;
  bp->thickdelta = 0;
#endif
}


ENTRYPOINT Bool
geodesic_handle_event (ModeInfo *mi, XEvent *event)
{
  geodesic_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      mode_str = "random";
      init_geodesic (mi);
      return True;
    }

  return False;
}


ENTRYPOINT void
draw_geodesic (ModeInfo *mi)
{
  int wire = MI_IS_WIREFRAME(mi);
  geodesic_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(bp->glx_context));

  if (! wire)
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

  bp->color1[0] = bp->colors[bp->ccolor].red    / 65536.0;
  bp->color1[1] = bp->colors[bp->ccolor].green  / 65536.0;
  bp->color1[2] = bp->colors[bp->ccolor].blue   / 65536.0;
  bp->color1[3] = 1;

  bp->color2[0] = bp->colors[bp->ccolor2].red   / 65536.0;
  bp->color2[1] = bp->colors[bp->ccolor2].green / 65536.0;
  bp->color2[2] = bp->colors[bp->ccolor2].blue  / 65536.0;
  bp->color2[3] = 1;

  bp->ccolor  = (bp->ccolor + 1) % bp->ncolors;
  bp->ccolor2 = (bp->ccolor + bp->ncolors / 2) % bp->ncolors;

  mi->polygon_count = 0;

  glScalef (10, 10, 10);

  {
    GLfloat r = bp->depth - floor(bp->depth);
    GLfloat alpha, morph1, morph2;
    int d1, d2;

    /* Two ranges: first for fading in the new segments.
       Second for morphing the segments into position.
     */
    GLfloat range = 0.15;
    GLfloat min1 = (0.5 - range) / 2;
    GLfloat max1 = 0.5 - min1;
    GLfloat min2 = 0.5 + min1;
    GLfloat max2 = 0.5 + max1;

    if (r < min1)			/* old alone */
      {
        d1 = d2 = floor (bp->depth);
        morph1 = morph2 = 1;
        alpha = 1;
      }
    else if (r < max1 &&		/* fade to new flat */
             (bp->mode == MESH ||
              bp->mode == STELLATED ||
              bp->mode == STELLATED2))
      {
        d1 = floor (bp->depth);
        d2 = ceil (bp->depth);
        morph1 = 1;
        morph2 = 0;
        alpha = (r - min1) / (max1 - min1);

        if (bp->mode == STELLATED || bp->mode == STELLATED2)
          {
            morph1 = 1 - alpha;		   /* de-stellate while fading out */
            morph1 = 2 * (morph1 - 0.5);   /* do it faster */
            if (morph1 < 0) morph1 = 0;
          }
      }
    else if (r < min2)			/* new flat */
      {
        d1 = d2 = ceil (bp->depth);
        morph1 = morph2 = 0;
        alpha = 1;
      }
    else if (r < max2)			/* morph */
      {
        d1 = d2 = ceil (bp->depth);
        morph1 = morph2 = (r - min2) / (max2 - min2);
        alpha = 1;
      }
    else				/* new alone */
      {
        d1 = d2 = ceil (bp->depth);
        morph1 = morph2 = 1;
        alpha = 1;
      }

    mi->recursion_depth = d2 + r;

    if (bp->mode == STELLATED2)
      {
        morph1 = -morph1;
        morph2 = -morph2;
      }

    if (d1 != d2)
      {
        if (alpha > 0.5)   /* always draw the more transparent one first */
          {
            int s1; GLfloat s2;
            s1 = d1; d1 = d2; d2 = s1;
            s2 = morph1; morph1 = morph2; morph2 = s2;
            alpha = 1 - alpha;
          }
        bp->color1[3] = 1 - alpha;
        bp->color2[3] = 1 - alpha;

        if (! wire)
          glDisable (GL_POLYGON_OFFSET_FILL);

        bp->morph_ratio = morph1;
        make_geodesic (mi, d1);

        /* Make the less-transparent object take precedence */
        if (!wire)
          {
            glEnable (GL_POLYGON_OFFSET_FILL);
            glPolygonOffset (1.0, 1.0);
          }
      }

    bp->color1[3] = alpha;
    bp->color2[3] = alpha;

    bp->morph_ratio = morph2;
    make_geodesic (mi, d2);
  }

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);


  if (! bp->button_down_p)
    {
      bp->depth += speed * bp->delta;
      bp->thickness += speed * bp->thickdelta;

      if (bp->depth > MI_COUNT(mi)-1)
        {
          bp->depth = MI_COUNT(mi)-1;
          bp->delta = -fabs (bp->delta);
        }
      else if (bp->depth < 0)
        {
          bp->depth = 0;
          bp->delta = fabs (bp->delta);

          /* Randomize the mode again when we hit the bottom state.
             #### I wish this did a fade instead of a jump-cut.
           */
          if (bp->random_p)
            bp->mode = MESH + (random() % (STELLATED2 - MESH + 1));
        }

      if (bp->thickness > 1)
        {
          bp->thickness = 1;
          bp->thickdelta = -fabs (bp->thickdelta);
        }
      else if (bp->thickness < 0)
        {
          bp->thickness = 0;
          bp->thickdelta = fabs (bp->thickdelta);
        }
    }
}

XSCREENSAVER_MODULE ("Geodesic", geodesic)

#endif /* USE_GL */
