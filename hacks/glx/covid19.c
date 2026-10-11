/* covid19, Copyright (c) 2020 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Created: Thursday, March 264th, 2020.
 */

#define DEFAULTS	"*delay:	30000       \n" \
			"*count:        60          \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \
			"*membraneColor: #AAFFAA"  "\n" \
			"*spikeColor:    #DD0000"  "\n" \
			"*mpColor:       #8888FF"  "\n" \
			"*epColor:       #FF8888"  "\n" \
			"*hesColor:      #880088"  "\n" \
			"*suppressRotationAnimation: True\n" \

# define release_ball 0

#include "xlockmore.h"
#include "colors.h"
#include "sphere.h"
#include "tube.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_SPEED       "1.0"

#define SPIKE_FACES   12
#define SMOOTH_SPIKES True
#define SPHERE_SLICES 64
#define SPHERE_STACKS 32
#define SPHERE_SLICES_2 16
#define SPHERE_STACKS_2 8

#define SPIKE_FACESb     3
#define SPHERE_SLICESb   10
#define SPHERE_STACKSb   5
#define SPHERE_SLICES_2b 5
#define SPHERE_STACKS_2b 3


typedef struct { GLfloat x, y, z; } XYZ;
typedef enum { MEMBRANE, SPIKE, M_PROTEIN, E_PROTEIN, HES } feature;
typedef enum { IN, DRAW, OUT } draw_mode;

#undef RANDSIGN
#define RANDSIGN() ((random() & 1) ? 1 : -1)

typedef struct {
  XYZ pos;
  GLfloat scale;
  rotator *rot;
  GLuint dlist;
} ball;

typedef struct {
  GLXContext *glx_context;
  trackball_state *trackball;
  Bool button_down_p;
  draw_mode mode;
  GLfloat tick;
  GLuint ball_lists[20];
  int ball_polys;
  int max_balls, count, ball_delta;
  ball *balls;
  GLfloat membrane_color[4];
  GLfloat spike_color[4];
  GLfloat mp_color[4];
  GLfloat ep_color[4];
  GLfloat hes_color[4];
} ball_configuration;

static ball_configuration *bps = NULL;

static Bool do_spin;
static GLfloat speed;
static Bool do_wander;

static XrmOptionDescRec opts[] = {
  { "-spin",   ".spin",   XrmoptionNoArg, "True" },
  { "+spin",   ".spin",   XrmoptionNoArg, "False" },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" },
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_Bool},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
};

ENTRYPOINT ModeSpecOpt ball_opts = {
  countof(opts), opts, countof(vars), vars, NULL};


/* Window management, etc
 */
ENTRYPOINT void
reshape_ball (ModeInfo *mi, int width, int height)
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
ball_handle_event (ModeInfo *mi, XEvent *event)
{
  ball_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      bp->mode = OUT;
      bp->tick = 1;
      return True;
    }

  return False;
}


static int
unit_spike (ModeInfo *mi, Bool lowrez)
{
  ball_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  int polys = 0;
  GLfloat r = 0.2;
  GLfloat s = 0.2;
  int i;
  glPushMatrix();

  glColor4fv (bp->spike_color);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, bp->spike_color);

  glScalef (s, s, s);
  glTranslatef (0, -r, 0);
  if (!lowrez)
    glTranslatef (-r, 0, 0);
  polys += tube (0, 0, 0,
                 0, 1, 0,
                 r, 0,
                 (lowrez ? SPIKE_FACESb : SPIKE_FACES),
                 True, False, wire);
  if (!lowrez)
    glTranslatef (r*2, 0, 0);
  if (! lowrez)
    polys += tube (0, 0, 0,
                   0, 1, 0,
                   r, 0,
                   (lowrez ? SPIKE_FACESb : SPIKE_FACES),
                   True, False, wire);
  if (!lowrez)
    glTranslatef (-r, 0, 0);

  glTranslatef (0, 1, 0);
  r *= 2;
  glScalef (r, r, r);

  for (i = 0; i < (lowrez ? 1 : 3); i++)
    {
      glPushMatrix();
      glRotatef (360.0 / 3 * i, 0, 1, 0);
      if (!lowrez)
        glTranslatef (r, 0, 0);
      polys += unit_sphere ((lowrez ? SPHERE_STACKS_2b : SPHERE_STACKS_2),
                            (lowrez ? SPHERE_SLICES_2b : SPHERE_SLICES_2),
                            wire);
      glPopMatrix();
    }

  glPopMatrix();
  return polys;
}


static int
unit_ball (ModeInfo *mi, Bool lowrez)
{
  ball_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  int polys = 0;
  feature f;

  for (f = 0; f <= HES; f++)
    {
      switch (f) {
      case MEMBRANE:
        glColor4fv (bp->membrane_color);
        glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, bp->membrane_color);
        polys += unit_sphere ((lowrez ? SPHERE_STACKSb : SPHERE_STACKS),
                              (lowrez ? SPHERE_SLICESb : SPHERE_SLICES),
                              wire);
        break;

      case SPIKE:
        {
          GLfloat th0 = atan (0.5);  /* lat division: 26.57 deg */
          GLfloat s = M_PI / 5;	     /* lon division: 72 deg    */
          int i, j;
          int n = (lowrez ? 8 : 10);
          for (j = 0; j < n; j++)
            for (i = 0; i < n; i++)
              {
                GLfloat th1 = s * i;
                GLfloat a = th0;
                GLfloat o = th1;
                GLfloat x, y, z;

                a += (0.2 + frand (0.9)) * RANDSIGN();
                o += (0.2 + frand (0.9)) * RANDSIGN();

                x = cos(a) * cos(o);
                y = cos(a) * sin(o);
                z = sin(a);

                glPushMatrix();

                if (! (i & 1))
                  {
                    glRotatef (180,   0, 1, 0);
                    glRotatef (180/5, 0, 0, 1);
                  }

                glTranslatef (x, y, z);
                glRotatef (-atan2 (x, y)               * (180/M_PI), 0, 0, 1);
                glRotatef ( atan2 (z, sqrt(x*x + y*y)) * (180/M_PI), 1, 0, 0);
                polys += unit_spike (mi, lowrez);
                glPopMatrix();
              }

          glPushMatrix();
          glRotatef (90, 1, 0, 0);
          glTranslatef (0, 1, 0);
          polys += unit_spike (mi, lowrez);

          glTranslatef (0, -2, 0);
          glRotatef (180, 1, 0, 0);
          polys += unit_spike (mi, lowrez);
          glPopMatrix();

        }
        break;

      default:
        {
          GLfloat s = 0.04;
          int n = (lowrez ? 50 : 200);
          int i;
          GLfloat *c;
          switch (f) {
          case M_PROTEIN: c = bp->mp_color; break;
          case E_PROTEIN: c = bp->ep_color; break;
          case HES:       c = bp->hes_color; break;
          default: abort();
          }

          glColor4fv (c);
          glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, c);

          if (f == HES) 
            {
              s *= 1.5;
              n /= 8;
            }

          for (i = 0; i < n; i++)
            {
              glPushMatrix();
              glRotatef (random() % 360, 1, 0, 0);
              glRotatef (random() % 180, 0, 1, 0);
              glTranslatef (1, 0, 0);
              glRotatef (90, 0, 0, 1);
              glScalef (s, s, s);
              polys += unit_dome ((lowrez ? SPHERE_STACKS_2b : SPHERE_STACKS_2),
                                  (lowrez ? SPHERE_SLICES_2b : SPHERE_SLICES_2),
                                  wire);
              glPopMatrix();
            }
        }
        break;
      }
    }

  return polys;
}


static void
parse_color (ModeInfo *mi, char *key, GLfloat color[4])
{
  XColor xcolor;
  char *string = get_string_resource (mi->dpy, key, "RobotColor");
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


static void
make_balls (ModeInfo *mi, int count)
{
  /* Distribute the balls into a rectangular grid that fills the window.
     There may be some empty cells.  N items in a W x H rectangle:
     N = W * H
     N = W * W * R
     N/R = W*W
     W = sqrt(N/R)
  */
  ball_configuration *bp = &bps[MI_SCREEN(mi)];

  GLfloat aspect = MI_WIDTH(mi) / (GLfloat) MI_HEIGHT(mi);
  int nlines = sqrt (count / aspect) + 0.5;
  int *cols = (int *) calloc (nlines, sizeof(*cols));
  int i, x, y, max = 0;
  GLfloat scale, spacing;
  Bool lowrez = (count > 40);

  if (bp->balls) 
    {
      for (i = 0; i < bp->count; i++)
        free_rotator (bp->balls[i].rot);
      free (bp->balls);
    }

  bp->count = count;
  bp->balls = (ball *) calloc (sizeof (*bp->balls), count);

  for (i = 0; i < count; i++)
    {
      cols[i % nlines]++;
      if (cols[i % nlines] > max) max = cols[i % nlines];
    }

  /* That gave us, e.g. 7777666. Redistribute to 6767767. */ 
  for (i = 0; i < nlines / 2; i += 2)
    {
      int j = nlines-i-1;
      int swap = cols[i];
      cols[i] = cols[j];
      cols[j] = swap;
    }

  scale = 1.0 / nlines;       /* Scale for height */
  if (scale * max > aspect)   /* Shrink if overshot width */
    scale *= aspect / (scale * max);

  scale *= 0.9;		  /* Add padding */
  spacing = scale * 4;

  if (count == 1) spacing = 0;

  i = 0;
  for (y = 0; y < nlines; y++)
    for (x = 0; x < cols[y]; x++)
      {
        ball *v = &bp->balls[i];
        double spin_speed   = 1.0 * speed;
        double wander_speed = 0.04 * speed;
        double spin_accel   = 1.0;
        int n = countof (bp->ball_lists) / 2;

        v->scale = scale;
        v->pos.x = spacing * (x - cols[y] / 2.0) + spacing/2;
        v->pos.y = spacing * (y - nlines  / 2.0) + spacing/2;
        v->pos.z = 0;
        v->dlist = bp->ball_lists [(random() % n) + (lowrez ? n : 0)];
        v->rot = make_rotator (do_spin ? spin_speed : 0,
                               do_spin ? spin_speed : 0,
                               do_spin ? spin_speed : 0,
                               spin_accel,
                               do_wander ? wander_speed : 0,
                               True);
        i++;
    }
  free (cols);
}


ENTRYPOINT void 
init_ball (ModeInfo *mi)
{
  ball_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  int i;

  MI_INIT (mi, bps);
  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_ball (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

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
    }

  parse_color (mi, "membraneColor", bp->membrane_color);
  parse_color (mi, "spikeColor",    bp->spike_color);
  parse_color (mi, "mpColor",       bp->mp_color);
  parse_color (mi, "epColor",       bp->ep_color);
  parse_color (mi, "hesColor",      bp->hes_color);

  for (i = 0; i < countof(bp->ball_lists); i++)
    {
      Bool lowrez = (i > countof(bp->ball_lists) / 2);
      bp->ball_lists[i] = glGenLists (1);
      glNewList (bp->ball_lists[i], GL_COMPILE);
      bp->ball_polys = unit_ball (mi, lowrez);
      glEndList ();
    }

  bp->ball_delta = 1;
  bp->max_balls = MI_COUNT(mi);
  if (bp->max_balls < 1) bp->max_balls = 1;
  bp->count = (bp->max_balls > 10 ? 1 + random() % 5 :
               bp->max_balls > 5  ? 1 + random() % 3 : 1);
  make_balls (mi, bp->count);

  bp->trackball = gltrackball_init (True);
}


ENTRYPOINT void
draw_ball (ModeInfo *mi)
{
  ball_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  GLfloat s = 1;
  int i;

  static const GLfloat bspec[4]  = {1.0, 1.0, 1.0, 1.0};
  static const GLfloat bshiny    = 128.0;

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  glScalef (4, 4, 4);

  gltrackball_rotate (bp->trackball);

  mi->polygon_count = 0;

  glMaterialfv (GL_FRONT, GL_SPECULAR,  bspec);
  glMateriali  (GL_FRONT, GL_SHININESS, bshiny);

  switch (bp->mode) {
  case DRAW:
    bp->tick -= 1/30.0/5;  /* No more often than 5 sec */
    if (bp->tick <= 0)
      {
        bp->tick = 1;
        if (! (random() % 20))
        {
          bp->mode = OUT;
          bp->tick = 1;
        }
      }
    s = 1;
    break;
  case IN:
    bp->tick += 1/12.0;
    if (bp->tick >= 1)
      {
        bp->tick = 1;
        bp->mode = DRAW;
      }
    s = bp->tick;
    break;
  case OUT:
    bp->tick -= 1/12.0;
    s = bp->tick;
    if (bp->tick <= 0)
      {
        int c2;
        int n;
        bp->tick = 0;
        bp->mode = IN;

        n = (bp->count < 5  ? 2 :
             bp->count < 20 ? 5 : 20);
        c2 = bp->count + (1 + (random() % n)) * bp->ball_delta;
        if (c2 < 1)
          {
            c2 = 1;
            bp->ball_delta = 1;
          }
        else if (c2 > bp->max_balls)
          {
            c2 = bp->max_balls;
            bp->ball_delta = -1;
          }

        make_balls (mi, c2);
        s = 0;
      }
    break;
  default:
    abort();
  }

  if (s > 0)
    for (i = 0; i < bp->count; i++)
      {
        ball *v = &bp->balls[i];
        double x, y, z;
        glPushMatrix();
        glTranslatef (v->pos.x, v->pos.y, v->pos.z);
        glScalef (v->scale, v->scale, v->scale);

        get_position (v->rot, &x, &y, &z, !bp->button_down_p);
        glTranslatef((x - 0.5) * 2,
                     (y - 0.5) * 2,
                     (z - 0.5) * 8 * (bp->count > 8 ? 3 : 1));
        get_rotation (v->rot, &x, &y, &z, !bp->button_down_p);
        glRotatef (x * 360, 1.0, 0.0, 0.0);
        glRotatef (y * 360, 0.0, 1.0, 0.0);
        glRotatef (z * 360, 0.0, 0.0, 1.0);

        glScalef (s, s, s);
        glCallList (v->dlist);
        mi->polygon_count += bp->ball_polys;
        glPopMatrix ();
      }
  glPopMatrix ();

  mi->recursion_depth = bp->count;

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_ball (ModeInfo *mi)
{
  ball_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  for (i = 0; i < bp->count; i++)
    free_rotator (bp->balls[i].rot);
  free (bp->balls);
  if (bp->trackball) gltrackball_free (bp->trackball);
  for (i = 0; i < countof(bp->ball_lists); i++)
    if (glIsList(bp->ball_lists[i])) glDeleteLists(bp->ball_lists[i], 1);
}

#ifndef HAVE_IPHONE
XSCREENSAVER_MODULE_2 ("COVID19", covid19, ball)
#else
XSCREENSAVER_MODULE_2 ("Co____9", co____9, ball)

  /* App Store Connect Resolution Center: App Review

     Binary Rejected

     Guideline 1.1 - Safety - Objectionable Content We found that your app
     includes content or concepts that some users may find upsetting,

     We found that your app includes content or concepts that some users may
     find upsetting, offensive, or otherwise objectionable.

     Specifically, your entertainment or gaming app inappropriately references
     the C____-__ p__d__ic in the metadata or binary. Entertainment or gaming
     apps that directly or indirectly reference the C____-__ p__d__ic in any
     way are not appropriate for the App Store.

   */
#endif


#endif /* USE_GL */
