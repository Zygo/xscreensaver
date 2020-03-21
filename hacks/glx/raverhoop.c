/* raverhoop, Copyright (c) 2016 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Simulates an LED hula hoop in a dark room. Oontz oontz oontz.
 */

#define DEFAULTS	"*delay:	20000       \n" \
			"*ncolors:      12          \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \

# define release_hoop 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "colors.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */


#define DEF_SPIN        "False"
#define DEF_WANDER      "False"
#define DEF_LIGHTS      "200"
#define DEF_SPEED       "1.0"
#define DEF_LIGHT_SPEED "1.0"
#define DEF_SUSTAIN     "1.0"

typedef struct {
  GLfloat x,y,z;
} XYZ;

typedef struct afterimage afterimage;
struct afterimage {
  GLfloat color[4];
  XYZ position;
  afterimage *next;
};

typedef struct {
  GLfloat color[4];
  int duty_cycle[10];   /* off, on, off, on... values add to 100 */
  GLfloat ratio;
  Bool on;
} light;


typedef struct oscillator oscillator;
struct oscillator {
  GLfloat ratio, from, to, speed, *var;
  int remaining;
  oscillator *next;
};


typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;

  int nlights;
  light *lights;
  GLfloat radius;
  GLfloat axial_radius;
  XYZ midpoint;
  GLfloat tilt;
  GLfloat spin;
  GLfloat th;
  GLfloat speed;
  afterimage *trail;
  oscillator *oscillators;

} hoop_configuration;

static hoop_configuration *bps = NULL;

static Bool do_spin;
static Bool do_wander;
static int nlights;
static GLfloat speed, light_speed, sustain;

static XrmOptionDescRec opts[] = {
  { "-spin",   ".spin",   XrmoptionNoArg, "True" },
  { "+spin",   ".spin",   XrmoptionNoArg, "False" },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" },
  { "-lights", ".lights", XrmoptionSepArg, 0 },
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
  { "-light-speed", ".lightSpeed",  XrmoptionSepArg, 0 },
  { "-sustain", ".sustain", XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_Bool},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&nlights,   "lights", "Lights", DEF_LIGHTS, t_Int},
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
  {&light_speed, "lightSpeed", "Speed", DEF_LIGHT_SPEED, t_Float},
  {&sustain,   "sustain", "Sustain",  DEF_SUSTAIN, t_Float},
};

ENTRYPOINT ModeSpecOpt hoop_opts = {countof(opts), opts, countof(vars), vars, NULL};


static void
decay_afterimages (ModeInfo *mi)
{
  hoop_configuration *bp = &bps[MI_SCREEN(mi)];
  afterimage *prev = 0;
  afterimage *a = bp->trail;
  GLfloat tick = 0.05 / sustain;

  while (a)
    {
      afterimage *next = a->next;
      a->color[3] -= tick;
      if (a->color[3] < 0)
        {
          if (prev)
            prev->next = next;
          else
            bp->trail = next;
          free (a);
        }
      else
        prev = a;
      a = next;
    }
}

static void
add_afterimage (ModeInfo *mi, GLfloat x, GLfloat y, GLfloat z,
                GLfloat color[4])
{
  hoop_configuration *bp = &bps[MI_SCREEN(mi)];
  afterimage *a = (afterimage *) calloc (1, sizeof (*a));
  afterimage *b;
  a->position.x = x;
  a->position.y = y;
  a->position.z = z;
  memcpy (a->color, color, sizeof(a->color));

  /* Put it at the end so that the older, dimmer ones are laid down on
     the frame buffer before the newer, brighter ones. */
  if (!bp->trail)
    bp->trail = a;
  else
    {
      for (b = bp->trail; b->next; b = b->next)
        ;
      b->next = a;
    }
}


static void
tick_light (light *L)
{
  int i;
  int n = 0;

  L->ratio += 0.05 * light_speed;
  while (L->ratio > 1)
    L->ratio -= 1;

  for (i = 0; i < countof(L->duty_cycle); i++)
    {
      n += L->duty_cycle[i];
      if (n > 100) abort();
      if (n / 100.0 > L->ratio)
        {
          L->on = (i & 1);
          break;
        }
    }
}



static void
tick_hoop (ModeInfo *mi)
{
  hoop_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat m0[16];
  int i;

  glGetFloatv (GL_MODELVIEW_MATRIX, m0);

  for (i = 0; i < bp->nlights; i++)
    {
      light *L = &bp->lights[i];
      GLfloat m1[16];
      GLfloat th = M_PI * 2 * i / bp->nlights;
      GLfloat x = cos (th);
      GLfloat y = sin (th);
      GLfloat z;
      
      tick_light (L);
      if (! L->on)
        continue;

      glPushMatrix();

      glTranslatef (bp->midpoint.x, bp->midpoint.y, bp->midpoint.z);
      glRotatef (bp->th * 180 / M_PI, 0, 0, 1);
      glRotatef (bp->tilt, 0, 1, 0);
      glRotatef (bp->spin, 1, 0, 0);
      glTranslatef (x * bp->radius, y * bp->radius, 0);
      glGetFloatv (GL_MODELVIEW_MATRIX, m1);
      glPopMatrix();

      /* After all of our translations and rotations, figure out where
         it actually ended up.
       */
      x = m1[12] - m0[12];
      y = m1[13] - m0[13];
      z = m1[14] - m0[14];
      add_afterimage (mi, x, y, z, L->color);
    }
}


static void
draw_lights (ModeInfo *mi)
{
  hoop_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  afterimage *a;
  GLfloat m[4][4];

  if (wire)
    {
      int n = 360;
      int i;
      glPushMatrix();

      glBegin (GL_LINES);
      glVertex3f (0, 0, -bp->radius);
      glVertex3f (0, 0,  bp->radius);
      glEnd();

      glTranslatef (bp->midpoint.x, bp->midpoint.y, bp->midpoint.z);
      glRotatef (bp->th * 180 / M_PI, 0, 0, 1);
      glRotatef (bp->tilt, 0, 1, 0);
      glRotatef (bp->spin, 1, 0, 0);

      glBegin (GL_LINE_LOOP);
      glVertex3f (0, 0, 0);
      for (i = 0; i <= n; i++)
        {
          GLfloat th = i * M_PI * 2 / n;
          glVertex3f (bp->radius * -cos(th),
                      bp->radius * -sin(th), 0);
        }
      for (i = 0; i <= n; i++)
        {
          GLfloat th = i * M_PI * 2 / n;
          glVertex3f (bp->axial_radius * -cos(th),
                      bp->axial_radius * -sin(th), 0);
        }
      glEnd();
      glPopMatrix();
    }

  /* Billboard the lights to always face the camera */
  glGetFloatv (GL_MODELVIEW_MATRIX, &m[0][0]);
  m[0][0] = 1; m[1][0] = 0; m[2][0] = 0;
  m[0][1] = 0; m[1][1] = 1; m[2][1] = 0;
  m[0][2] = 0; m[1][2] = 0; m[2][2] = 1;
  glLoadIdentity();
  glMultMatrixf (&m[0][0]);

  for (a = bp->trail; a; a = a->next)
    {
      glPushMatrix();

      glTranslatef (a->position.x, a->position.y, a->position.z);

      if (wire)
        {
          GLfloat c[3];
          c[0] = a->color[0] * a->color[3];
          c[1] = a->color[1] * a->color[3];
          c[2] = a->color[2] * a->color[3];
          glColor3fv (c);
        }
      else
        glColor4fv (a->color);

      glRotatef (45, 0, 0, 1);
      glScalef (0.15, 0.15, 0.15);
      glBegin (GL_QUADS);
      glTexCoord2f (0, 0); glVertex3f (-1, -1, 0);
      glTexCoord2f (1, 0); glVertex3f ( 1, -1, 0);
      glTexCoord2f (1, 1); glVertex3f ( 1,  1, 0);
      glTexCoord2f (0, 1); glVertex3f (-1,  1, 0);
      glEnd();
      mi->polygon_count++;

      glPopMatrix();
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
  GLfloat ease = 0.35;
  if      (r <= 0)     return 0;
  else if (r >= 1)     return 1;
  else if (r <= ease)  return     ease * ease_fn (r / ease);
  else if (r > 1-ease) return 1 - ease * ease_fn ((1 - r) / ease);
  else                 return r;
}


static void
tick_oscillators (ModeInfo *mi)
{
  hoop_configuration *bp = &bps[MI_SCREEN(mi)];
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
calm_oscillators (ModeInfo *mi)
{
  hoop_configuration *bp = &bps[MI_SCREEN(mi)];
  oscillator *a;
  for (a = bp->oscillators; a && a->next; a = a->next)
    a->remaining = 1;
}


static void
add_oscillator (ModeInfo *mi, GLfloat *var, GLfloat speed, GLfloat to,
                int repeat)
{
  hoop_configuration *bp = &bps[MI_SCREEN(mi)];
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


static void
add_random_oscillator (ModeInfo *mi)
{
  hoop_configuration *bp = &bps[MI_SCREEN(mi)];
  int n = random() % 12;
  switch (n) {
  case 0: case 1: case 2:
    add_oscillator (mi, &bp->midpoint.z, 1,
                    bp->radius * (0.8 + frand(0.2))
                    * (random() & 1 ? 1 : -1),
                    (3 + (random() % 10)));
    break;
  case 3: case 4: case 5:
    add_oscillator (mi, &bp->tilt, 1,
                    -(GLfloat) (random() % 15),
                    3 + (random() % 20));
    break;
  case 6: case 7: case 8:
    add_oscillator (mi, &bp->axial_radius, 1,
                    0.1 + bp->radius * frand(1.4),
                    1 + (random() % 4));
    break;
  case 9: case 10:
    add_oscillator (mi, &bp->speed, 3,
                    (0.7 + frand(0.9)) * (random() & 1 ? 1 : -1),
                    ((random() % 5)
                     ? 1
                     : 2 + (random() % 5)));
    break;
  case 11: 
    add_oscillator (mi, &bp->spin, 0.1,
                    180 * (1 + (random() % 2)),
                    2 * (1 + (random() % 5)));
    break;
  default:
    abort();
    break;
  }
}


static void
randomize_colors (ModeInfo *mi)
{
  hoop_configuration *bp = &bps[MI_SCREEN(mi)];
  int ncolors = MI_NCOLORS(mi);
  GLfloat *colors;
  int ncycles;
  int i;

  if (ncolors < 1)
    ncolors = 1;
  if (ncolors > bp->nlights)
    ncolors = bp->nlights;

  if (! (random() % 10))
    ncolors = 1;

  colors = calloc (ncolors, 4 * sizeof(*colors));
  for (i = 0; i < ncolors; i++)
    {
      GLfloat *c = &colors[i * 4];
# define QUANTIZE() (((random() % 16) << 4) | 0xF) / 255.0
      c[0] = QUANTIZE();
      c[1] = QUANTIZE();
      c[2] = QUANTIZE();
      c[3] = 1;
    }

  switch (random() % 5) {
  case 0:  ncycles = 1; break;
  case 2:  ncycles = ncolors * (1 + (random() % 5)); break;
  default: ncycles = ncolors; break;
  }

  for (i = 0; i < bp->nlights; i++)
    {
      light *L = &bp->lights[i];
      int n = i * ncolors / bp->nlights;
      int m = i * ncycles / bp->nlights;
      if (n >= ncolors) abort();
      if (m >= ncycles) abort();
      memcpy (L->color, &colors[n], sizeof (L->color));

      if (ncycles <= 1)
        {
          L->duty_cycle[0] = 0;
          L->duty_cycle[1] = 100;
        }
      else if (m & 1)
        {
          L->duty_cycle[0] = 50;
          L->duty_cycle[1] = 50;
        }
      else
        {
          L->duty_cycle[0] = 0;
          L->duty_cycle[1] = 50;
          L->duty_cycle[2] = 50;
        }
    }
  free (colors);
}


static void
move_hoop (ModeInfo *mi)
{
  hoop_configuration *bp = &bps[MI_SCREEN(mi)];

  bp->th += 0.2 * speed * bp->speed;
  while (bp->th > M_PI*2)
    bp->th -= M_PI*2;
  while (bp->th < 0)
    bp->th += M_PI*2;

  bp->midpoint.x = bp->axial_radius * cos (bp->th);
  bp->midpoint.y = bp->axial_radius * sin (bp->th);

  tick_oscillators (mi);

  if (! (random() % 80))
    add_random_oscillator (mi);

  if (! (random() % 120))
    randomize_colors (mi);
}


static void
build_texture (ModeInfo *mi)
{
  int x, y;
  int size = 128;
  int s2 = size / 2;
  int bpl = size * 2;
  unsigned char *data = malloc (bpl * size);

  for (y = 0; y < size; y++)
    {
      for (x = 0; x < size; x++)
        {
          double dist = (sqrt (((s2 - x) * (s2 - x)) +
                               ((s2 - y) * (s2 - y)))
                         / s2);
          unsigned char *c = &data [y * bpl + x * 2];
          c[0] = 0xFF;
          c[1] = 0xFF * sin (dist > 1 ? 0 : (1 - dist));
        }
    }

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
  check_gl_error ("texture param");

  glTexImage2D (GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, size, size, 0,
                GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, data);
  check_gl_error ("light texture");
  free (data);
}



/* Window management, etc
 */
ENTRYPOINT void
reshape_hoop (ModeInfo *mi, int width, int height)
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
hoop_handle_event (ModeInfo *mi, XEvent *event)
{
  hoop_configuration *bp = &bps[MI_SCREEN(mi)];

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
          randomize_colors (mi);
          calm_oscillators (mi);
          add_random_oscillator (mi);
          return True;
        }
    }

  return False;
}


ENTRYPOINT void 
init_hoop (ModeInfo *mi)
{
  hoop_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);

  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_hoop (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  glDisable (GL_LIGHTING);
  glDisable (GL_DEPTH_TEST);
  glEnable (GL_CULL_FACE);
  glEnable (GL_NORMALIZE);
  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE);
  glPolygonMode (GL_FRONT, GL_FILL);
  glShadeModel (GL_FLAT);

  if (! wire)
    {
      build_texture (mi);
      glEnable (GL_TEXTURE_2D);
    }

  {
    double spin_speed   = 0.3;
    double wander_speed = 0.005;
    double spin_accel   = 2.0;

    bp->rot = make_rotator (do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            spin_accel,
                            do_wander ? wander_speed : 0,
                            False);
    bp->trackball = gltrackball_init (True);
  }

  bp->radius = 30;
  bp->axial_radius = bp->radius * 0.3;
  bp->tilt = - (GLfloat) (5 + (random() % 12));
  bp->speed = (random() % 1 ? 1 : -1);
  bp->nlights = nlights;
  bp->lights = (light *) calloc (bp->nlights, sizeof (*bp->lights));
  randomize_colors (mi);
  move_hoop (mi);
  add_random_oscillator (mi);
}


ENTRYPOINT void
draw_hoop (ModeInfo *mi)
{
  hoop_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  {
    double x, y, z;
    get_position (bp->rot, &x, &y, &z, !bp->button_down_p);
    glTranslatef((x - 0.5) * 7,
                 (y - 0.5) * 0.5,
                 (z - 0.5) * 15);

    gltrackball_rotate (bp->trackball);
    glRotatef (current_device_rotation(), 0, 0, 1);

    get_rotation (bp->rot, &x, &y, &z, !bp->button_down_p);
    glRotatef (x * 360, 1.0, 0.0, 0.0);
    glRotatef (y * 360, 0.0, 1.0, 0.0);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }

  mi->polygon_count = 0;

  glScalef (0.2, 0.2, 0.2);

# ifdef HAVE_MOBILE
  glScalef (0.7, 0.7, 0.7);
# endif

  glRotatef (70, 1, 0, 0);

  if (! bp->button_down_p)
    move_hoop (mi);

  decay_afterimages (mi);
  tick_hoop (mi);
  draw_lights (mi);

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_hoop (ModeInfo *mi)
{
  hoop_configuration *bp = &bps[MI_SCREEN(mi)];

  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  if (bp->trackball) gltrackball_free (bp->trackball);
  if (bp->rot) free_rotator (bp->rot);
  if (bp->lights) free (bp->lights);
  while (bp->trail) {
    afterimage *n = bp->trail->next;
    free (bp->trail);
    bp->trail = n;
  }
  while (bp->oscillators) {
    oscillator *n = bp->oscillators->next;
    free (bp->oscillators);
    bp->oscillators = n;
  }
  
}

XSCREENSAVER_MODULE_2 ("RaverHoop", raverhoop, hoop)

#endif /* USE_GL */
