/* splodesic, Copyright (c) 2016 Jamie Zawinski <jwz@jwz.org>
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

# define release_splodesic 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "colors.h"
#include "normals.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */


#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_SPEED       "1.0"
#define DEF_FREQ        "4"

#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

typedef struct { double a, o; } LL;	/* latitude + longitude */

typedef struct triangle triangle;
struct triangle {
  XYZ p[3];
  triangle *next;
  triangle *neighbors[3];
  GLfloat altitude;
  GLfloat velocity;
  GLfloat thrust;
  int thrust_duration;
  int refcount;
};

typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;

  int count;
  triangle *triangles;

  int ncolors;
  XColor *colors;
  int ccolor;

} splodesic_configuration;

static splodesic_configuration *bps = NULL;

static Bool do_spin;
static GLfloat speed;
static int depth_arg;
static Bool do_wander;

static XrmOptionDescRec opts[] = {
  { "-spin",   ".spin",   XrmoptionNoArg, "True" },
  { "+spin",   ".spin",   XrmoptionNoArg, "False" },
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
  { "-depth",  ".freq",   XrmoptionSepArg, 0 },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" }
};

static argtype vars[] = {
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_Bool},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
  {&depth_arg, "freq",   "Depth",  DEF_FREQ,   t_Int},
};

ENTRYPOINT ModeSpecOpt splodesic_opts = {countof(opts), opts, countof(vars), vars, NULL};


/* Creates a triangle specified by 3 polar endpoints.
 */
static void
make_triangle1 (ModeInfo *mi, LL v1, LL v2, LL v3)
{
  splodesic_configuration *bp = &bps[MI_SCREEN(mi)];
  triangle *t = (triangle *) calloc (1, sizeof(*t));

  t->p[0].x = cos (v1.a) * cos (v1.o);
  t->p[0].y = cos (v1.a) * sin (v1.o);
  t->p[0].z = sin (v1.a);

  t->p[1].x = cos (v2.a) * cos (v2.o);
  t->p[1].y = cos (v2.a) * sin (v2.o);
  t->p[1].z = sin (v2.a);

  t->p[2].x = cos (v3.a) * cos (v3.o);
  t->p[2].y = cos (v3.a) * sin (v3.o);
  t->p[2].z = sin (v3.a);

  t->next = bp->triangles;
  bp->triangles = t;
  bp->count++;
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


/* Creates triangular geodesic facets to the given depth.
 */
static void
make_triangle (ModeInfo *mi, LL v1, LL v2, LL v3, int depth)
{
  if (depth <= 0)
    make_triangle1 (mi, v1, v2, v3);
  else
    {
      LL v12, v23, v13;
      XYZ p1, p2, p3, p12, p23, p13;

      midpoint2 (v1, v2, &v12, &p1, &p2, &p12);
      midpoint2 (v2, v3, &v23, &p2, &p3, &p23);
      midpoint2 (v1, v3, &v13, &p1, &p3, &p13);
      depth--;

      make_triangle (mi, v1,  v12, v13, depth);
      make_triangle (mi, v12, v2,  v23, depth);
      make_triangle (mi, v13, v23, v3,  depth);
      make_triangle (mi, v12, v23, v13, depth);
    }
}


/* Creates triangles of a geodesic to the given depth (frequency).
 */
static void
make_geodesic (ModeInfo *mi)
{
  int depth = depth_arg;
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
          make_triangle (mi, v1, v2, vc, depth);
          make_triangle (mi, v2, v1, v3, depth);
        }
      else				/* south */
        {
          v1.a = -v1.a;
          v2.a = -v2.a;
          v3.a = -v3.a;
          vc.a = -vc.a;
          make_triangle (mi, v2, v1, vc, depth);
          make_triangle (mi, v1, v2, v3, depth);
        }
    }
}


/* Add t1 to the neighbor list of t0. */
static void
link_neighbor (int i, int j, triangle *t0, triangle *t1)
{
  int k;
  if (t0 == t1)
    return;
  for (k = 0; k < countof(t0->neighbors); k++)
    {
      if (t0->neighbors[k] == t1 ||
          t0->neighbors[k] == 0)
        {
          t0->neighbors[k] = t1;
          return;
        }
    }
  fprintf (stderr, "%d %d: too many neighbors\n", i, j);
  abort();
}


static int
feq (GLfloat a, GLfloat b)	/* Oh for fuck's sake */
{
  const GLfloat e = 0.00001;
  GLfloat d = a - b;
  return (d > -e && d < e);
}


/* Link each triangle to its three neighbors.
 */
static void
link_neighbors (ModeInfo *mi)
{
  splodesic_configuration *bp = &bps[MI_SCREEN(mi)];
  triangle *t0 = bp->triangles;
  int i;

  /* Triangles are neighbors if they share an edge (exactly 2 points).
     (There must be a faster than N! way to do this...)
   */
  for (i = 0, t0 = bp->triangles; t0; t0 = t0->next, i++)
    {
      triangle *t1;
      int j;

      for (j = i+1, t1 = t0->next; t1; t1 = t1->next, j++)
        {
          int count = 0;
          int ii, jj;
          for (ii = 0; ii < 3; ii++)
            for (jj = 0; jj < 3; jj++)
              if (feq (t0->p[ii].x, t1->p[jj].x) &&
                  feq (t0->p[ii].y, t1->p[jj].y) &&
                  feq (t0->p[ii].z, t1->p[jj].z))
                count++;
          if (count >= 3)
            {
              fprintf (stderr, "%d %d: too many matches: %d\n", i, j, count);
              abort();
            }
          if (count == 2)
            {
              link_neighbor (i, j, t0, t1);
              link_neighbor (j, i, t1, t0);
            }
        }

      if (! (t0->neighbors[0] && t0->neighbors[1] && t0->neighbors[2]))
        {
          fprintf (stderr, "%d: missing neighbors\n", i);
          abort();
        }

      t0->altitude = 60;  /* Fall in from space */
    }
}


/* Add thrust to the triangle, and propagate some of that to its neighbors.
 */
static void
add_thrust (triangle *t, GLfloat thrust)
{
  GLfloat dampen = 0;
  if (t->refcount)
    return;
  t->refcount++;
  t->velocity += thrust;

  /* Eyeballed this to look roughly the same at various depths. Eh. */
  switch (depth_arg) {
  case 0: dampen = 0.5;    break;
  case 1: dampen = 0.7;    break;
  case 2: dampen = 0.9;    break;
  case 3: dampen = 0.98;   break;
  case 4: dampen = 0.985;  break;
  default: dampen = 0.993; break;
  }

  thrust *= dampen;
  if (thrust > 0.1)
    {
      add_thrust (t->neighbors[0], thrust);
      add_thrust (t->neighbors[1], thrust);
      add_thrust (t->neighbors[2], thrust);
    }
}


static void
tick_triangles (ModeInfo *mi)
{
  splodesic_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat gravity = 0.1;
  triangle *t;
  int i;

  /* Compute new velocities. */
  for (i = 0, t = bp->triangles; t; t = t->next, i++)
    {
      if (t->thrust > 0)
        {
          add_thrust (t, t->thrust);
          t->thrust_duration--;
          if (t->thrust_duration <= 0)
            {
              t->thrust_duration = 0;
              t->thrust = 0;
            }
        }
    }

  /* Apply new velocities. */
  for (i = 0, t = bp->triangles; t; t = t->next, i++)
    {
      t->altitude += t->velocity;
      t->velocity -= gravity;
      if (t->altitude < 0)
        {
          t->velocity = 0;
          t->altitude = 0;
        }
      t->refcount = 0;  /* Clear for next time */
    }

  /* Add eruptions. */
  if (frand(1 / speed) < 0.2)
    {
      int n = random() % bp->count;
      for (i = 0, t = bp->triangles; t; t = t->next, i++)
        if (i == n)
          break;
      t->thrust += gravity * 1.5;
      t->thrust_duration = 1 + BELLRAND(16);
    }

  bp->ccolor++;
  if (bp->ccolor >= bp->ncolors)
    bp->ccolor = 0;
}


static void
draw_triangles (ModeInfo *mi)
{
  splodesic_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  triangle *t;
  GLfloat c[4];
  int c0 = bp->ccolor;
  int c1 = (c0 + bp->ncolors / 2) % bp->ncolors;

  c[0] = bp->colors[c0].red    / 65536.0;
  c[1] = bp->colors[c0].green  / 65536.0;
  c[2] = bp->colors[c0].blue   / 65536.0;
  c[3] = 1;

  if (wire)
    glColor4fv (c);
  else
    {
      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, c);

      c[0] = bp->colors[c1].red    / 65536.0;
      c[1] = bp->colors[c1].green  / 65536.0;
      c[2] = bp->colors[c1].blue   / 65536.0;
      c[3] = 1;
      glMaterialfv (GL_BACK, GL_AMBIENT_AND_DIFFUSE, c);
    }

  glFrontFace (GL_CCW);
  for (t = bp->triangles; t; t = t->next)
    {
      GLfloat a = t->altitude * 0.25;
      XYZ c;
      glPushMatrix();

      c.x = t->p[0].x + t->p[1].x + t->p[2].x;
      c.y = t->p[0].y + t->p[1].y + t->p[2].y;
      c.z = t->p[0].z + t->p[1].z + t->p[2].z;
      if (a > 0)
        glTranslatef (a * c.x / 3, a * c.y / 3, a * c.z / 3);
      glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLES);
      glNormal3f (c.x, c.y, c.z);
      glVertex3f (t->p[0].x, t->p[0].y, t->p[0].z);
      glVertex3f (t->p[1].x, t->p[1].y, t->p[1].z);
      glVertex3f (t->p[2].x, t->p[2].y, t->p[2].z);
      glEnd();
      mi->polygon_count++;
      glPopMatrix();
    }
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_splodesic (ModeInfo *mi, int width, int height)
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

# ifdef HAVE_MOBILE	/* Keep it the same relative size when rotated. */
  {
    int o = (int) current_device_rotation();
    if (o != 0 && o != 180 && o != -180)
      glScalef (1/h, 1/h, 1/h);
  }
# endif

  glClear(GL_COLOR_BUFFER_BIT);
}


ENTRYPOINT Bool
splodesic_handle_event (ModeInfo *mi, XEvent *event)
{
  splodesic_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;
  else if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);
      if (c == ' ' || c == '\t')
        {
          bp->ncolors = 1024;
          make_smooth_colormap (0, 0, 0,
                                bp->colors, &bp->ncolors,
                                False, 0, False);
          return True;
        }
    }

  return False;
}


ENTRYPOINT void 
init_splodesic (ModeInfo *mi)
{
  splodesic_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);

  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_splodesic (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  if (!wire)
    {
      GLfloat pos[4] = {4.0, 1.4, 1.1, 0.0};
      GLfloat amb[4] = {0.0, 0.0, 0.0, 1.0};
      GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
      GLfloat spc[4] = {1.0, 0.2, 0.2, 1.0};
      GLfloat cspec[4] = {1, 1, 1, 1};
      static const GLfloat shiny = 10;
      int lightmodel = 1;

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_CULL_FACE);

      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);
      glLightModeliv (GL_LIGHT_MODEL_TWO_SIDE, &lightmodel);
      glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR, cspec);
      glMateriali (GL_FRONT_AND_BACK, GL_SHININESS, shiny);
    }

  {
    double spin_speed   = 0.5;
    double wander_speed = 0.005;
    double spin_accel   = 1.0;

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
  make_smooth_colormap (0, 0, 0,
                        bp->colors, &bp->ncolors,
                        False, 0, False);

# ifdef HAVE_MOBILE
  depth_arg--;
# endif

  if (depth_arg < 0)  depth_arg = 0;
  if (depth_arg > 10) depth_arg = 10;

  make_geodesic (mi);
  link_neighbors (mi);
}


ENTRYPOINT void
draw_splodesic (ModeInfo *mi)
{
  splodesic_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glDisable(GL_CULL_FACE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  {
    double x, y, z;
    get_position (bp->rot, &x, &y, &z, !bp->button_down_p);
    glTranslatef((x - 0.5) * 6,
                 (y - 0.5) * 6,
                 (z - 0.5) * 8);

    gltrackball_rotate (bp->trackball);

    get_rotation (bp->rot, &x, &y, &z, !bp->button_down_p);
    glRotatef (x * 360, 1.0, 0.0, 0.0);
    glRotatef (y * 360, 0.0, 1.0, 0.0);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }

  mi->polygon_count = 0;

# ifdef HAVE_MOBILE
  glScalef (3, 3, 3);
#else
  glScalef (4, 4, 4);
# endif

  if (! bp->button_down_p)
    tick_triangles (mi);
  draw_triangles (mi);

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_splodesic (ModeInfo *mi)
{
  splodesic_configuration *bp = &bps[MI_SCREEN(mi)];

  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  while (bp->triangles)
    {
      triangle *t = bp->triangles->next;
      free (bp->triangles);
      bp->triangles = t;
    }
  if (bp->trackball) gltrackball_free (bp->trackball);
  if (bp->rot) free_rotator (bp->rot);
  if (bp->colors) free (bp->colors);
}

XSCREENSAVER_MODULE ("Splodesic", splodesic)

#endif /* USE_GL */
