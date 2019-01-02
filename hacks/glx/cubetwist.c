/* cubetwist, Copyright (c) 2016-2017 Jamie Zawinski <jwz@jwz.org>
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

# define release_cube 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "normals.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */


#define DEF_SPIN         "True"
#define DEF_WANDER       "True"
#define DEF_SPEED        "1.0"
#define DEF_FLAT         "True"
#define DEF_THICKNESS    "0.0"
#define DEF_DISPLACEMENT "0.0"

typedef struct cube cube;
struct cube {
  GLfloat size, thickness;
  XYZ pos, rot;
  GLfloat color[4];
  cube *next;
};

typedef struct oscillator oscillator;
struct oscillator {
  double ratio, from, to, speed, *var;
  int remaining;
  oscillator *next;
};

typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;
  cube *cubes;
  oscillator *oscillators;
} cube_configuration;

static cube_configuration *bps = NULL;

static Bool do_flat;
static Bool do_spin;
static GLfloat speed;
static GLfloat thickness;
static GLfloat displacement;
static Bool do_wander;

static XrmOptionDescRec opts[] = {
  { "-spin",   ".spin",   XrmoptionNoArg, "True" },
  { "+spin",   ".spin",   XrmoptionNoArg, "False" },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" },
  { "-flat",   ".flat",   XrmoptionNoArg, "True" },
  { "+flat",   ".flat",   XrmoptionNoArg, "False" },
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
  { "-thickness",  ".thickness",  XrmoptionSepArg, 0 },
  { "-displacement",  ".displacement",  XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&do_flat,   "flat",   "flat",   DEF_FLAT,   t_Bool},
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_Bool},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
  {&thickness, "thickness", "Thickness", DEF_THICKNESS, t_Float},
  {&displacement, "displacement", "Displacement", DEF_DISPLACEMENT, t_Float},
};

ENTRYPOINT ModeSpecOpt cube_opts = {countof(opts), opts, countof(vars), vars, NULL};


static int
draw_strut (ModeInfo *mi, cube *c)
{
  int wire = MI_IS_WIREFRAME(mi);
  int polys = 0;

  glPushMatrix();
  glFrontFace (GL_CW);
  glNormal3f (0, 0, -1);
  glTranslatef (-c->size/2, -c->size/2, -c->size/2);

  glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLE_FAN);
  glVertex3f (0, 0, 0);
  glVertex3f (c->size, 0, 0);
  glVertex3f (c->size - c->thickness, c->thickness, 0);
  glVertex3f (c->thickness, c->thickness, 0);
  glEnd();
  polys += 2;

  glNormal3f (0, 1, 0);
  glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLE_FAN);
  glVertex3f (c->thickness, c->thickness, 0);
  glVertex3f (c->size - c->thickness, c->thickness, 0);
  glVertex3f (c->size - c->thickness, c->thickness, c->thickness);
  glVertex3f (c->thickness, c->thickness, c->thickness);
  glEnd();
  polys += 2;
  glPopMatrix();

  return polys;
}


static int
draw_cubes (ModeInfo *mi, cube *c)
{
  int polys = 0;
  int i, j;

  glColor4fv (c->color);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, c->color);

  glPushMatrix();
  for (j = 0; j < 6; j++)
    {
      for (i = 0; i < 4; i++)
        {
          polys += draw_strut (mi, c);
          glRotatef (90, 0, 0, 1);
        }
      if (j == 3)
        glRotatef (90, 0, 0, 1);
      if (j < 4)
        glRotatef (90, 0, 1, 0);
      else
        glRotatef (180, 1, 0, 0);
    }
  glPopMatrix();

  if (c->next)
    {
      /* This leaves rotations on the prevailing matrix stack, but since
         this is a tail-call, that's fine.  Don't blow the matrix stack. */
      glRotatef (c->rot.x, 1, 0, 0);
      glRotatef (c->rot.y, 0, 1, 0);
      glRotatef (c->rot.z, 0, 0, 1);
      glTranslatef (c->pos.x, c->pos.y, c->pos.z);
      c->next->pos = c->pos;
      c->next->rot = c->rot;
      polys += draw_cubes (mi, c->next);
    }

  check_gl_error("cubetwist");
  return polys;
}


static void
make_cubes (ModeInfo *mi)
{
  cube_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat step = 2 * (thickness + displacement);
  GLfloat size = 1.0;
  cube *tail = 0;
  GLfloat cc[4], cstep;
  int depth = 0;
  cube *c;

  cc[0] = 0.3 + frand(0.7);
  cc[1] = 0.3 + frand(0.7);
  cc[2] = 0.3 + frand(0.7);
  cc[3] = 1;

  if (bp->cubes) abort();
  while (1)
    {
      cube *c = (cube *) calloc (1, sizeof (*c));
      c->size = size;
      c->thickness = thickness;
      if (tail)
        tail->next = c;
      else
        bp->cubes = c;
      tail = c;

      depth++;
      size -= step;
      if (size <= step)
        break;
    }

  cstep = 0.8 / depth;
  for (c = bp->cubes; c; c = c->next)
    {
      memcpy (c->color, cc, sizeof(cc));
      cc[0] -= cstep;
      cc[1] -= cstep;
      cc[2] -= cstep;
    }
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


static void
tick_oscillators (ModeInfo *mi)
{
  cube_configuration *bp = &bps[MI_SCREEN(mi)];
  oscillator *prev = 0;
  oscillator *a = bp->oscillators;
  GLfloat tick = 0.1 / speed;

  while (a)
    {
      oscillator *next = a->next;
      a->ratio += tick * a->speed;
      if (a->ratio > 1)
        a->ratio = 1;

      *a->var = a->from + (a->to - a->from) * ease_ratio (a->ratio);

      if (a->ratio < 1)			/* mid cycle */
        prev = a;
      else if (--a->remaining <= 0)	/* ended, and expired */
        {
          if (prev)
            prev->next = next;
          else
            bp->oscillators = next;
          free (a);
        }
      else				/* keep going the other way */
        {
          GLfloat swap = a->from;
          a->from = a->to;
          a->to = swap;
          a->ratio = 0;
          prev = a;
        }

      a = next;
    }
}


static void
add_oscillator (ModeInfo *mi, double *var, GLfloat speed, GLfloat to,
                int repeat)
{
  cube_configuration *bp = &bps[MI_SCREEN(mi)];
  oscillator *a;

  /* If an oscillator is already running on this variable, don't
     add another. */
  for (a = bp->oscillators; a && a->next; a = a->next)
    if (a->var == var)
      return;

  a = (oscillator *) calloc (1, sizeof (*a));
  if (repeat <= 0) abort();
  a->ratio = 0;
  a->from = *var;
  a->to = to;
  a->speed = speed;
  a->var = var;
  a->remaining = repeat;
  a->next = bp->oscillators;
  bp->oscillators = a;
# if 0
  fprintf (stderr, "%s: %3d %6.2f -> %6.2f %s\n",
           progname, repeat, *var, to,
           (var == &bp->midpoint.z ? "z" :
            var == &bp->tilt ? "tilt" :
            var == &bp->axial_radius ? "r" :
            var == &bp->speed ? "speed" : "?"));
# endif
}


#undef RANDSIGN
#define RANDSIGN() ((random() & 1) ? 1 : -1)

static void
add_random_oscillator (ModeInfo *mi)
{
  cube_configuration *bp = &bps[MI_SCREEN(mi)];
  cube *c = bp->cubes;
  double s1 = speed * 0.07;
  double s2 = speed * 0.3;
  double disp = (thickness + displacement);
  int c1 = 1 + ((random() % 4) ? 0 : (random() % 3));
  int c2 = 2;
  int n = random() % 6;

  switch (n) {
  case 0: add_oscillator (mi, &c->rot.x, s1, 90 * RANDSIGN(), c1); break;
  case 1: add_oscillator (mi, &c->rot.y, s1, 90 * RANDSIGN(), c1); break;
  case 2: add_oscillator (mi, &c->rot.z, s1, 90 * RANDSIGN(), c1); break;
  case 3: add_oscillator (mi, &c->pos.x, s2, disp * RANDSIGN(), c2); break;
  case 4: add_oscillator (mi, &c->pos.y, s2, disp * RANDSIGN(), c2); break;
  case 5: add_oscillator (mi, &c->pos.z, s2, disp * RANDSIGN(), c2); break;
  default: abort(); break;
  }
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_cube (ModeInfo *mi, int width, int height)
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
cube_handle_event (ModeInfo *mi, XEvent *event)
{
  cube_configuration *bp = &bps[MI_SCREEN(mi)];

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
          while (bp->cubes)
            {
              cube *c = bp->cubes->next;
              free (bp->cubes);
              bp->cubes = c;
            }

          while (bp->oscillators)
            {
              oscillator *o = bp->oscillators->next;
              free (bp->oscillators);
              bp->oscillators = o;
            }

          if (random() & 1)
            {
              thickness = 0.03 + frand(0.02);
              displacement = (random() & 1) ? 0 : (thickness / 3);
            }
          else
            {
              thickness = 0.001 + frand(0.02);
              displacement = 0;
            }

          make_cubes (mi);

          return True;
        }
    }

  return False;
}


ENTRYPOINT void 
init_cube (ModeInfo *mi)
{
  cube_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);

  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_cube (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  if (!wire && !do_flat)
    {
      GLfloat color[4] = {1, 1, 1, 1};
      GLfloat cspec[4] = {1, 1, 0, 1};
      static const GLfloat shiny = 30;

      static GLfloat pos0[4] = { 0.5, -1, -0.5, 0};
      static GLfloat pos1[4] = {-0.75, -1, 0, 0};
      static GLfloat amb[4] = {0, 0, 0, 1};
      static GLfloat dif[4] = {1, 1, 1, 1};
      static GLfloat spc[4] = {1, 1, 1, 1};

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_LIGHT1);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_CULL_FACE);

      glLightfv(GL_LIGHT0, GL_POSITION, pos0);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);

      glLightfv(GL_LIGHT1, GL_POSITION, pos1);
      glLightfv(GL_LIGHT1, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT1, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT1, GL_SPECULAR, spc);

      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);
      glMaterialfv (GL_FRONT, GL_SPECULAR,  cspec);
      glMateriali  (GL_FRONT, GL_SHININESS, shiny);
    }

  {
    double spin_speed   = 0.05;
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

  if (thickness > 0.5)
    thickness = 0.5;
  if (displacement > 0.5)
    displacement = 0.5;

  if (thickness <= 0.0001)
    {
      if (random() & 1)
        {
          thickness = 0.03 + frand(0.02);
          displacement = (random() & 1) ? 0 : (thickness / 3);
        }
      else
        {
          thickness = 0.001 + frand(0.02);
          displacement = 0;
        }
    }

  make_cubes (mi);
}


ENTRYPOINT void
draw_cube (ModeInfo *mi)
{
  cube_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  glShadeModel(GL_SMOOTH);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  glScalef(1.1, 1.1, 1.1);

  {
    double x, y, z;
    get_position (bp->rot, &x, &y, &z, !bp->button_down_p);
    glTranslatef((x - 0.5) * 4,
                 (y - 0.5) * 4,
                 (z - 0.5) * 2);

    gltrackball_rotate (bp->trackball);

    get_rotation (bp->rot, &x, &y, &z, !bp->button_down_p);
    glRotatef (x * 360, 1.0, 0.0, 0.0);
    glRotatef (y * 360, 0.0, 1.0, 0.0);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }

  mi->polygon_count = 0;

  glScalef (6, 6, 6);

  mi->polygon_count = draw_cubes (mi, bp->cubes);
  glPopMatrix ();

  if (!bp->button_down_p)
    tick_oscillators (mi);

  if (! bp->oscillators &&
      !bp->button_down_p &&
      !(random() % 60))
    {
      bp->cubes->pos.x = bp->cubes->pos.y = bp->cubes->pos.z = 0;
      bp->cubes->rot.x = bp->cubes->rot.y = bp->cubes->rot.z = 0;
      add_random_oscillator (mi);
    }

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

ENTRYPOINT void
free_cube (ModeInfo *mi)
{
  cube_configuration *bp = &bps[MI_SCREEN(mi)];
  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  while (bp->cubes)
    {
      cube *c = bp->cubes->next;
      free (bp->cubes);
      bp->cubes = c;
    }

  while (bp->oscillators)
    {
      oscillator *o = bp->oscillators->next;
      free (bp->oscillators);
      bp->oscillators = o;
    }
  if (bp->trackball) gltrackball_free (bp->trackball);
  if (bp->rot) free_rotator (bp->rot);
}


XSCREENSAVER_MODULE_2 ("CubeTwist", cubetwist, cube)

#endif /* USE_GL */
