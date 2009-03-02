/* dangerball, Copyright (c) 2001, 2002, 2003 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include <X11/Intrinsic.h>

extern XtAppContext app;

#define PROGCLASS	"DangerBall"
#define HACK_INIT	init_ball
#define HACK_DRAW	draw_ball
#define HACK_RESHAPE	reshape_ball
#define HACK_HANDLE_EVENT ball_handle_event
#define EVENT_MASK      PointerMotionMask
#define sws_opts	xlockmore_opts

#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_SPEED       "0.05"

#define DEFAULTS	"*delay:	30000       \n" \
			"*count:        30          \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \
			"*speed:      " DEF_SPEED " \n" \
			"*spin:       " DEF_SPIN   "\n" \
			"*wander:     " DEF_WANDER "\n" \


#define SPIKE_FACES   12  /* how densely to render spikes */
#define SMOOTH_SPIKES True
#define SPHERE_SLICES 32  /* how densely to render spheres */
#define SPHERE_STACKS 16


#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "colors.h"
#include "sphere.h"
#include "tube.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#include <GL/glu.h>

typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;

  GLuint ball_list;
  GLuint spike_list;

  GLfloat pos;
  int *spikes;

  int ncolors;
  XColor *colors;
  int ccolor;
  int color_shift;

} ball_configuration;

static ball_configuration *bps = NULL;

static Bool do_spin;
static GLfloat speed;
static Bool do_wander;

static XrmOptionDescRec opts[] = {
  { "-spin",   ".spin",   XrmoptionNoArg, "True" },
  { "+spin",   ".spin",   XrmoptionNoArg, "False" },
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" }
};

static argtype vars[] = {
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_Bool},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
};

ModeSpecOpt sws_opts = {countof(opts), opts, countof(vars), vars, NULL};


/* Window management, etc
 */
void
reshape_ball (ModeInfo *mi, int width, int height)
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
randomize_spikes (ModeInfo *mi)
{
  ball_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  bp->pos = 0;
  for (i = 0; i < MI_COUNT(mi); i++)
    {
      bp->spikes[i*2]   = (random() % 360) - 180;
      bp->spikes[i*2+1] = (random() % 180) - 90;
    }

# define ROT_SCALE 22
  for (i = 0; i < MI_COUNT(mi) * 2; i++)
    bp->spikes[i] = (bp->spikes[i] / ROT_SCALE) * ROT_SCALE;

  if ((random() % 3) == 0)
    bp->color_shift = random() % (bp->ncolors / 2);
  else
    bp->color_shift = 0;
}

static void
draw_spikes (ModeInfo *mi)
{
  ball_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat diam = 0.2;
  GLfloat pos = bp->pos;
  int i;

  if (pos < 0) pos = -pos;

  pos = (asin (0.5 + pos/2) - 0.5) * 2;

  for (i = 0; i < MI_COUNT(mi); i++)
    {
      glPushMatrix();
      glRotatef(bp->spikes[i*2],   0, 1, 0);
      glRotatef(bp->spikes[i*2+1], 0, 0, 1);
      glTranslatef(0.7, 0, 0);
      glRotatef(-90, 0, 0, 1);
      glScalef (diam, pos, diam);
      glCallList (bp->spike_list);
      glPopMatrix();

      mi->polygon_count += (SPIKE_FACES + 1);
    }
}


static void
move_spikes (ModeInfo *mi)
{
  ball_configuration *bp = &bps[MI_SCREEN(mi)];

  if (bp->pos >= 0)		/* moving outward */
    {
      bp->pos += speed;
      if (bp->pos >= 1)		/*  reverse gears at apex */
        bp->pos = -1;
    }
  else				/* moving inward */
    {
      bp->pos += speed;
      if (bp->pos >= 0)		/*  stop at end */
        randomize_spikes (mi);
    }
}


Bool
ball_handle_event (ModeInfo *mi, XEvent *event)
{
  ball_configuration *bp = &bps[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button & Button1)
    {
      bp->button_down_p = True;
      gltrackball_start (bp->trackball,
                         event->xbutton.x, event->xbutton.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button & Button1)
    {
      bp->button_down_p = False;
      return True;
    }
  else if (event->xany.type == MotionNotify &&
           bp->button_down_p)
    {
      gltrackball_track (bp->trackball,
                         event->xmotion.x, event->xmotion.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }

  return False;
}


void 
init_ball (ModeInfo *mi)
{
  ball_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);

  if (!bps) {
    bps = (ball_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (ball_configuration));
    if (!bps) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }

    bp = &bps[MI_SCREEN(mi)];
  }

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

  {
    double spin_speed   = 10.0;
    double wander_speed = 0.15;
    double spin_accel   = 2.0;

    bp->rot = make_rotator (do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            spin_accel,
                            do_wander ? wander_speed : 0,
                            True);
    bp->trackball = gltrackball_init ();
  }

  bp->ncolors = 128;
  bp->colors = (XColor *) calloc(bp->ncolors, sizeof(XColor));
  make_smooth_colormap (0, 0, 0,
                        bp->colors, &bp->ncolors,
                        False, 0, False);

  bp->spikes = (int *) calloc(MI_COUNT(mi), sizeof(*bp->spikes) * 2);

  bp->ball_list = glGenLists (1);
  bp->spike_list = glGenLists (1);

  glNewList (bp->ball_list, GL_COMPILE);
  unit_sphere (SPHERE_STACKS, SPHERE_SLICES, wire);
  glEndList ();

  glNewList (bp->spike_list, GL_COMPILE);
  cone (0, 0, 0,
        0, 1, 0,
        1, 0, SPIKE_FACES, SMOOTH_SPIKES, False, wire);
  glEndList ();

  randomize_spikes (mi);
}


void
draw_ball (ModeInfo *mi)
{
  ball_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int c2;

  static GLfloat bcolor[4] = {0.0, 0.0, 0.0, 1.0};
  static GLfloat scolor[4] = {0.0, 0.0, 0.0, 1.0};
  static GLfloat bspec[4]  = {1.0, 1.0, 1.0, 1.0};
  static GLfloat sspec[4]  = {0.0, 0.0, 0.0, 1.0};
  static GLfloat bshiny    = 128.0;
  static GLfloat sshiny    = 0.0;

  if (!bp->glx_context)
    return;

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
    glTranslatef((x - 0.5) * 8,
                 (y - 0.5) * 8,
                 (z - 0.5) * 15);

    gltrackball_rotate (bp->trackball);

    get_rotation (bp->rot, &x, &y, &z, !bp->button_down_p);
    glRotatef (x * 360, 1.0, 0.0, 0.0);
    glRotatef (y * 360, 0.0, 1.0, 0.0);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }

  bcolor[0] = bp->colors[bp->ccolor].red   / 65536.0;
  bcolor[1] = bp->colors[bp->ccolor].green / 65536.0;
  bcolor[2] = bp->colors[bp->ccolor].blue  / 65536.0;

  c2 = (bp->ccolor + bp->color_shift) % bp->ncolors;
  scolor[0] = bp->colors[c2].red   / 65536.0;
  scolor[1] = bp->colors[c2].green / 65536.0;
  scolor[2] = bp->colors[c2].blue  / 65536.0;

  bp->ccolor++;
  if (bp->ccolor >= bp->ncolors) bp->ccolor = 0;

  mi->polygon_count = 0;

  glScalef (2.0, 2.0, 2.0);

  move_spikes (mi);

  glMaterialfv (GL_FRONT, GL_SPECULAR,            bspec);
  glMateriali  (GL_FRONT, GL_SHININESS,           bshiny);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, bcolor);
  glCallList (bp->ball_list);
  mi->polygon_count += (SPHERE_SLICES * SPHERE_STACKS);

  glMaterialfv (GL_FRONT, GL_SPECULAR,            sspec);
  glMaterialf  (GL_FRONT, GL_SHININESS,           sshiny);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, scolor);
  draw_spikes (mi);
  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

#endif /* USE_GL */
