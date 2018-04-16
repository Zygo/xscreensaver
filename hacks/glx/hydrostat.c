/* hydrostat, Copyright (C) 2012 by Justin Windle
 * Copyright (c) 2016 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Tentacle simulation using inverse kinematics.
 *
 *   http://soulwire.co.uk/experiments/muscular-hydrostats/
 *   https://github.com/soulwire/Muscular-Hydrostats/
 *
 * Ported to C from Javascript by jwz, May 2016
 */

#define DEFAULTS	"*delay:	20000       \n" \
			"*count:	3           \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \
			"*suppressRotationAnimation: True\n" \

# define release_hydrostat 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "colors.h"
#include "sphere.h"
#include "normals.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

/* It looks bad when you rotate it with the trackball, because it reveals
   that the tentacles are moving in a 2d plane.  But it's useful for
   debugging. */
#undef USE_TRACKBALL

#define DEF_SPEED       "1.0"
#define DEF_PULSE       "True"
#define DEF_HEAD_RADIUS "60"
#define DEF_TENTACLES   "35"
#define DEF_THICKNESS   "18"
#define DEF_LENGTH      "55"
#define DEF_GRAVITY     "0.5"
#define DEF_CURRENT     "0.25"
#define DEF_FRICTION    "0.02"
#define DEF_OPACITY     "0.8"


#define TENTACLE_FACES 5

typedef struct {
  XYZ pos, opos, v;
} node;

typedef struct {
  int length;
  GLfloat radius;
  GLfloat spacing;
  GLfloat friction;
  GLfloat th;
  node *nodes;
  GLfloat color[4];
} tentacle;

typedef struct {
  XYZ pos, from, to;
  GLfloat ratio, pulse, rate;
  GLfloat head_radius;
  GLfloat thickness;
  int ntentacles;
  tentacle *tentacles;
  GLfloat color[4];
} squid;

typedef struct {
  GLXContext *glx_context;
  Bool button_down_p;
  int dragging;
  squid **squids;
  GLfloat cos_sin_table[2 * (TENTACLE_FACES + 1)];
  GLuint head;
  int head_polys;
# ifdef USE_TRACKBALL
  trackball_state *trackball;
# endif
} hydrostat_configuration;

static hydrostat_configuration *bps = NULL;

static Bool do_pulse;
static GLfloat speed_arg;
static GLfloat head_radius_arg;
static GLfloat ntentacles_arg;
static GLfloat thickness_arg;
static GLfloat length_arg;
static GLfloat gravity_arg;
static GLfloat current_arg;
static GLfloat friction_arg;
static GLfloat opacity_arg;

static XrmOptionDescRec opts[] = {
  { "-pulse",       ".pulse",      XrmoptionNoArg, "True"  },
  { "+pulse",       ".pulse",      XrmoptionNoArg, "False" },
  { "-speed",       ".speed",      XrmoptionSepArg, 0 },
  { "-head-radius", ".headRadius", XrmoptionSepArg, 0 },
  { "-tentacles",   ".tentacles",  XrmoptionSepArg, 0 },
  { "-thickness",   ".thickness",  XrmoptionSepArg, 0 },
  { "-length",      ".length",     XrmoptionSepArg, 0 },
  { "-gravity",     ".gravity",    XrmoptionSepArg, 0 },
  { "-current",     ".current",    XrmoptionSepArg, 0 },
  { "-friction",    ".friction",   XrmoptionSepArg, 0 },
  { "-opacity",     ".opacity",    XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  { &do_pulse,        "pulse",      "Pulse",      DEF_PULSE,       t_Bool  },
  { &speed_arg,       "speed",      "Speed",      DEF_SPEED,       t_Float },
  { &head_radius_arg, "headRadius", "HeadRadius", DEF_HEAD_RADIUS, t_Float },
  { &ntentacles_arg,  "tentacles",  "Tentacles",  DEF_TENTACLES,   t_Float },
  { &thickness_arg,   "thickness",  "Thickness",  DEF_THICKNESS,   t_Float },
  { &length_arg,      "length",     "Length",     DEF_LENGTH,      t_Float },
  { &gravity_arg,     "gravity",    "Gravity",    DEF_GRAVITY,     t_Float },
  { &current_arg,     "current",    "Current",    DEF_CURRENT,     t_Float },
  { &friction_arg,    "friction",   "Friction",   DEF_FRICTION,    t_Float },
  { &opacity_arg,     "opacity",    "Opacity",    DEF_OPACITY,     t_Float },
};

ENTRYPOINT ModeSpecOpt hydrostat_opts = {countof(opts), opts,
                                         countof(vars), vars, NULL};


static void
move_tentacle (squid *sq, tentacle *t)
{
  int i, j;
  node *prev = &t->nodes[0];
  int rot = (int) current_device_rotation();

  for (i = 1, j = 0; i < t->length; i++, j++)
    {
      XYZ d, p;
      GLfloat da;
      node *n = &t->nodes[i];

      /* Sadly, this is still computing motion in a 2d plane, so the
         tentacles look dumb if the scene is rotated. */

      n->pos.x += n->v.x;
      n->pos.y += n->v.y;
      n->pos.z += n->v.z;

      d.x = prev->pos.x - n->pos.x;
      d.y = prev->pos.y - n->pos.y;
      d.z = prev->pos.z - n->pos.z;
      da = atan2 (d.z, d.x);

      p.x = n->pos.x + cos (da) * t->spacing * t->length;
      p.y = n->pos.y + cos (da) * t->spacing * t->length;
      p.z = n->pos.z + sin (da) * t->spacing * t->length;

      n->pos.x = prev->pos.x - (p.x - n->pos.x);
      n->pos.y = prev->pos.y - (p.y - n->pos.y);
      n->pos.z = prev->pos.z - (p.z - n->pos.z);

      n->v.x = n->pos.x - n->opos.x;
      n->v.y = n->pos.y - n->opos.y;
      n->v.z = n->pos.z - n->opos.z;

      n->v.x *= t->friction * (1 - friction_arg);
      n->v.y *= t->friction * (1 - friction_arg);
      n->v.z *= t->friction * (1 - friction_arg);

      switch (rot) {
      case 90: case -270:
        n->v.x += gravity_arg;
        n->v.y -= current_arg;
        n->v.z -= current_arg;
        break;
      case -90: case 270:
        n->v.x -= gravity_arg;
        n->v.y += current_arg;
        n->v.z += current_arg;
        break;
      case 180: case -180:
        n->v.x -= current_arg;
        n->v.y -= current_arg;
        n->v.z -= gravity_arg;
        break;
      default:
        n->v.x += current_arg;
        n->v.y += current_arg;
        n->v.z += gravity_arg;
        break;
      }

      n->opos.x = n->pos.x;
      n->opos.y = n->pos.y;
      n->opos.z = n->pos.z;

      prev = n;
    }
}


static GLfloat
ease_fn (GLfloat r)
{
  return cos ((r/2 + 1) * M_PI) + 1; /* Smooth curve up, end at slope 1. */
}


/* Squirty motion: fast acceleration, then fade. */
static GLfloat
ease_ratio (GLfloat r)
{
  GLfloat ease = 0.05;
  GLfloat ease2 = 1-ease;
  if      (r <= 0)     r = 0;
  else if (r >= 1)     r = 1;
  else if (r <= ease)  r =     ease  * ease_fn (r / ease);
  else                 r = 1 - ease2 * ease_fn ((1 - r) / ease2);
  return r;
}


static void
move_squid (ModeInfo *mi, squid *sq)
{
  hydrostat_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat step = M_PI * 2 / sq->ntentacles;
  int i;
  GLfloat radius = head_radius_arg;
  GLfloat r;

  /* Move to a new position */

  if (! bp->button_down_p)
    {
      sq->ratio += speed_arg * 0.01;
      if (sq->ratio >= 1)
        {
          sq->ratio = -(frand(2.0) + frand(2.0) + frand(2.0));
          sq->from.x = sq->to.x;
          sq->from.y = sq->to.y;
          sq->from.z = sq->to.z;
          sq->to.x = 250 - frand(500);
          sq->to.y = 250 - frand(500);
          sq->to.z = 250 - frand(500);
        }

      r = sq->ratio > 0 ? ease_ratio (sq->ratio) : 0;
      sq->pos.x = sq->from.x + r * (sq->to.x - sq->from.x);
      sq->pos.y = sq->from.y + r * (sq->to.y - sq->from.y);
      sq->pos.z = sq->from.z + r * (sq->to.z - sq->from.z);
    }

  if (do_pulse)
    {
      GLfloat p = pow (sin (sq->pulse * M_PI), 18);
      sq->head_radius = (head_radius_arg * 0.7 +
                         head_radius_arg * 0.3 * p);
      radius = sq->head_radius * 0.25;
      sq->pulse += sq->rate * speed_arg * 0.02;
      if (sq->pulse > 1) sq->pulse = 0;
    }

  for (i = 0; i < sq->ntentacles; i++)
    {
      tentacle *tt = &sq->tentacles[i];
      GLfloat th = i * step;
      GLfloat px = cos (th) * radius;
      GLfloat py = sin (th) * radius;
      tt->th = th;
      tt->nodes[0].pos.x = sq->pos.x + px;
      tt->nodes[0].pos.y = sq->pos.y + py;
      tt->nodes[0].pos.z = sq->pos.z;
      move_tentacle (sq, tt);
    }
}


/* Find the angle at which the head should be tilted in the XY plane.
 */
static GLfloat
head_angle (ModeInfo *mi, squid *sq)
{
  int i;
  XYZ sum = { 0, };

  for (i = 0; i < sq->ntentacles; i++)
    {
      tentacle *t = &sq->tentacles[i];
      int j = t->length / 3;  /* Pick a node toward the top */
      node *n = &t->nodes[j];
      sum.x += n->pos.x;
      sum.y += n->pos.y;
      sum.z += n->pos.z;
    }

  sum.x /= sq->ntentacles;
  sum.y /= sq->ntentacles;
  sum.z /= sq->ntentacles;

  sum.x -= sq->pos.x;
  sum.y -= sq->pos.y;
  sum.z -= sq->pos.z;

  return (-atan2 (sum.x, sum.z) * (180 / M_PI));
}


static void
draw_head (ModeInfo *mi, squid *sq, GLfloat scale)
{
  hydrostat_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat c2[4];
  GLfloat angle = head_angle (mi, sq);

  scale *= 1.1;

  glPushMatrix();
  glTranslatef (sq->pos.x, sq->pos.y, sq->pos.z);
  glScalef (sq->head_radius, sq->head_radius, sq->head_radius);
  glScalef (scale, scale, scale);
  glRotatef (90, 1, 0, 0);

  memcpy (c2, sq->color, sizeof(c2));
  if (opacity_arg < 1.0 && scale >= 1.0)
    c2[3] *= 0.6;
  glColor4fv (c2);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, c2);

  glTranslatef (0, 0.3, 0);
  glRotatef (angle, 0, 0, 1);

  glCallList (bp->head);
  mi->polygon_count += bp->head_polys;

  glPopMatrix();
}


static void
draw_squid (ModeInfo *mi, squid *sq)
{
  hydrostat_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  int i;
  glPushMatrix();
  glRotatef (90, 1, 0, 0);

  if (opacity_arg < 1.0)
    draw_head (mi, sq, 0.75);

  if (!wire) {
    glFrontFace (GL_CCW);
    glBegin (GL_TRIANGLE_STRIP);
  }

  for (i = 0; i < sq->ntentacles; i++)
    {
      tentacle *t = &sq->tentacles[i];
      int j;

      glColor4fv (t->color);
      glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, t->color);

      if (wire)
        {
          glBegin (GL_LINE_STRIP);
          for (j = 0; j < t->length; j++)
            glVertex3f (t->nodes[j].pos.x,
                        t->nodes[j].pos.y,
                        t->nodes[j].pos.z);
          glEnd();
          mi->polygon_count += t->length;
        }
      else
        {
          GLfloat radius = t->radius * thickness_arg;
          GLfloat rstep = radius / t->length;
          for (j = 0; j < t->length-1; j++)
            {
              int k;
              node *n1 = &t->nodes[j];
              node *n2 = &t->nodes[j+1];
              GLfloat X = (n1->pos.x - n2->pos.x);
              GLfloat Y = (n1->pos.y - n2->pos.y);
              GLfloat Z = (n1->pos.z - n2->pos.z);
              GLfloat L = sqrt (X*X + Y*Y + Z*Z);
              GLfloat r2 = radius - rstep;
              GLfloat L2 = sqrt (X*X + Y*Y);

              for (k = 0; k <= TENTACLE_FACES; k++)
                {
                  GLfloat c = bp->cos_sin_table[2 * k];
                  GLfloat s = bp->cos_sin_table[2 * k + 1];
                  GLfloat x1 = radius * c;
                  GLfloat y1 = radius * s;
                  GLfloat z1 = 0;
                  GLfloat x2 = r2 * c;
                  GLfloat y2 = r2 * s;
                  GLfloat z2 = L;

                  GLfloat x1t = (L2*X*z1-X*Z*y1+L*Y*x1)/(L*L2);
                  GLfloat z1t = (L2*Y*z1-Y*Z*y1-L*X*x1)/(L*L2);
                  GLfloat y1t = (Z*z1+L2*y1)/L;

                  GLfloat x2t = (L2*X*z2-X*Z*y2+L*Y*x2)/(L*L2) + n1->pos.x;
                  GLfloat z2t = (L2*Y*z2-Y*Z*y2-L*X*x2)/(L*L2) + n1->pos.y;
                  GLfloat y2t = (Z*z2+L2*y2)/L + n1->pos.z;

                  glNormal3f (x1t, z1t, y1t);

                  x1t += n1->pos.x;
                  z1t += n1->pos.y;
                  y1t += n1->pos.z;

                  if (k == 0)
                    glVertex3f (x1t, z1t, y1t);
                  glVertex3f (x1t, z1t, y1t);

                  glVertex3f (x2t, z2t, y2t);
                  if (k == TENTACLE_FACES)
                    glVertex3f (x2t, z2t, y2t);

                  mi->polygon_count++;
                }
              radius = r2;
            }
        }
    }

  if (!wire)
    glEnd ();

  draw_head (mi, sq, 1.0);

  glPopMatrix();
}


static squid *
make_squid (ModeInfo *mi, int which)
{
  squid *sq = calloc (1, sizeof(*sq));
  int i;

  sq->head_radius = head_radius_arg;
  sq->thickness   = thickness_arg;
  sq->ntentacles  = ntentacles_arg;

  sq->color[0] = 0.1 + frand(0.7);
  sq->color[1] = 0.5 + frand(0.5);
  sq->color[2] = 0.1 + frand(0.7);
  sq->color[3] = opacity_arg;

  sq->from.x = sq->to.x = sq->pos.x = 200 - frand(400);
  sq->from.y = sq->to.y = sq->pos.y = 200 - frand(400);
  sq->from.z = sq->to.z = sq->pos.z = -frand(200);

  sq->ratio = -frand(3);

  if (which > 0)   /* Start others off screen, and moving in */
    {
      sq->from.x = sq->to.x = sq->pos.x = 800 + frand(500)
        * (random()&1 ? 1 : -1);
      sq->from.y = sq->to.y = sq->pos.y = 800 + frand(500)
        * (random()&1 ? 1 : -1);
      sq->ratio = 0;
    }

  if (do_pulse)
    sq->pulse = frand(1.0);
  sq->rate = 0.8 + frand(0.2);

  sq->tentacles = (tentacle *)
    calloc (sq->ntentacles, sizeof(*sq->tentacles));
  for (i = 0; i < sq->ntentacles; i++)
    {
      int j;
      tentacle *t = &sq->tentacles[i];
      GLfloat shade = 0.75 + frand(0.25);

      t->length   = 2 + length_arg * (0.8 + frand (0.4));
      t->radius   = 0.05 + frand (0.95);
      t->spacing  = 0.02 + frand (0.08);
      t->friction = 0.7  + frand (0.18);
      t->nodes = (node *) calloc (t->length + 1, sizeof (*t->nodes));

      t->color[0] = shade * sq->color[0];
      t->color[1] = shade * sq->color[1];
      t->color[2] = shade * sq->color[2];
      t->color[3] = sq->color[3];

      for (j = 0; j < t->length; j++)
        {
          node *n = &t->nodes[j];
          n->pos.x = sq->pos.x;
          n->pos.y = sq->pos.y;
          n->pos.z = sq->pos.z + j;
        }
    }

  return sq;
}

/* qsort comparator for sorting squid by depth */
static int
cmp_squid (const void *aa, const void *bb)
{
  squid * const *a = aa;
  squid * const *b = bb;
  return ((int) ((*b)->pos.y * 10000) -
          (int) ((*a)->pos.y * 10000));
}


static void
free_squid (squid *sq)
{
  int i;
  for (i = 0; i < sq->ntentacles; i++)
    free (sq->tentacles[i].nodes);
  free (sq->tentacles);
  free (sq);
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_hydrostat (ModeInfo *mi, int width, int height)
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
hydrostat_handle_event (ModeInfo *mi, XEvent *event)
{
  hydrostat_configuration *bp = &bps[MI_SCREEN(mi)];
  int w = MI_WIDTH(mi);
  int h = MI_HEIGHT(mi);
  int x, y;

# ifdef USE_TRACKBALL
  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;
# endif

  switch (event->xany.type) {
  case ButtonPress: case ButtonRelease:
    x = event->xbutton.x;
    y = event->xbutton.y;
    break;
  case MotionNotify:
    x = event->xmotion.x;
    y = event->xmotion.y;
    break;
  default:
    x = y = 0;
  }

  x -= w/2;
  y -= h/2;
  x *= 0.7;
  y *= 0.7;

  if (event->xany.type == ButtonPress)
    {
      int i;
      GLfloat D0 = 999999;

      /* This is pretty halfassed hit detection, but it works ok... */
      for (i = 0; i < MI_COUNT(mi); i++)
        {
          squid *s = bp->squids[i];
          GLfloat X = s->pos.x - x;
          GLfloat Y = s->pos.z - y;
          GLfloat D = sqrt(X*X + Y*Y);
          if (D < D0)
            {
              bp->dragging = i;
              D0 = D;
            }
        }

      if (D0 > 300)	/* Too far away, missed hit */
        {
          bp->dragging = -1;
          return False;
        }

      bp->squids[bp->dragging]->ratio = -3;
      bp->button_down_p = True;

      return True;
    }
  else if (event->xany.type == ButtonRelease && bp->dragging >= 0)
    {
      bp->button_down_p = False;
      bp->dragging = -1;
      return True;
    }
  else if (event->xany.type == MotionNotify && bp->dragging >= 0)
    {
      squid *s = bp->squids[bp->dragging];
      s->from.x = s->to.x = s->pos.x = x;
      s->from.z = s->to.z = s->pos.z = y;
      s->from.y = s->to.y = s->pos.y;
      return True;
    }

  return False;
}


ENTRYPOINT void 
init_hydrostat (ModeInfo *mi)
{
  int wire = MI_IS_WIREFRAME(mi);
  hydrostat_configuration *bp;
  int i;

  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_hydrostat (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  if (!wire)
    {
      GLfloat pos[4] = {1.0, 1.0, 1.0, 0.0};
      GLfloat amb[4] = {0.0, 0.0, 0.0, 1.0};
      GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
      GLfloat spc[4] = {0.0, 1.0, 1.0, 1.0};
      int k;

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_CULL_FACE);

      glLightfv(GL_LIGHT0, GL_POSITION, pos);
      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
      glLightfv(GL_LIGHT0, GL_SPECULAR, spc);

      for (k = 0; k <= TENTACLE_FACES; k++)
        {
          GLfloat th  = k * M_PI * 2 / TENTACLE_FACES;
          bp->cos_sin_table[2 * k] = cos(th);
          bp->cos_sin_table[2 * k + 1] = sin(th);
        }
    }

  glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);

  if (MI_COUNT(mi) <= 0)
    MI_COUNT(mi) = 1;

  if (random() & 1)
    current_arg = -current_arg;

  if (MI_COUNT(mi) == 1 || wire)
    opacity_arg = 1.0;
  if (opacity_arg < 0.1) opacity_arg = 0.1;
  if (opacity_arg > 1.0) opacity_arg = 1.0;

  bp->squids = (squid **) calloc (MI_COUNT(mi), sizeof(*bp->squids));
  for (i = 0; i < MI_COUNT(mi); i++)
    bp->squids[i] = make_squid (mi, i);

  bp->dragging = -1;

  if (opacity_arg < 1.0)
    {
      glEnable (GL_BLEND);
      glBlendFunc (GL_SRC_ALPHA, GL_ONE);
    }

  i = wire ? 4 : 24;
  bp->head = glGenLists (1);
  glNewList (bp->head, GL_COMPILE);
  glScalef (1, 1.1, 1);
  bp->head_polys = unit_dome (wire ? 8 : 16, i, wire);
  glRotatef (180, 0, 0, 1);
  glScalef (1, 0.5, 1);
  bp->head_polys += unit_dome (wire ? 8 : 8, i, wire);
  glEndList ();

# ifdef USE_TRACKBALL
  bp->trackball = gltrackball_init (True);
# endif
}


ENTRYPOINT void
draw_hydrostat (ModeInfo *mi)
{
  hydrostat_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int i;

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(bp->glx_context));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  glScalef (0.03, 0.03, 0.03);

# ifdef USE_TRACKBALL
  gltrackball_rotate (bp->trackball);
# endif

  mi->polygon_count = 0;

  if (opacity_arg < 1.0)
    qsort (bp->squids, MI_COUNT(mi), sizeof(*bp->squids), cmp_squid);

  for (i = 0; i < MI_COUNT(mi); i++)
    {
      squid *sq = bp->squids[i];
      move_squid (mi, sq);
      draw_squid (mi, sq);
      if (opacity_arg < 1.0)
        glClear (GL_DEPTH_BUFFER_BIT);
    }

  if (! (random() % 700))  /* Reverse the flow every now and then */
    current_arg = -current_arg;

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_hydrostat (ModeInfo *mi)
{
  hydrostat_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  if (!bp->squids)
    return;
  for (i = 0; i < MI_COUNT(mi); i++)
    free_squid (bp->squids[i]);
  free (bp->squids);
}

XSCREENSAVER_MODULE ("Hydrostat", hydrostat)

#endif /* USE_GL */
