/* hypnowheel, Copyright (c) 2008 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Draws overlapping spirals, where the tightness of the spirals changes.
 * Nice settings:
 *
 * -layers 7 -wander
 * -count 3 -layers 50
 * -twistiness 0.2 -layers 20
 * -count 3 -layers 2 -speed 20 -twist 10 -wander
 */

#define DEFAULTS	"*delay:	20000       \n" \
			"*count:        13          \n" \
			"*showFPS:      False       \n" \
			"*fpsSolid:     True        \n" \
			"*wireframe:    False       \n" \
			"*suppressRotationAnimation: True\n" \

# define refresh_hypnowheel 0
# define release_hypnowheel 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "colors.h"
#include "rotator.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */


#define DEF_WANDER      "False"
#define DEF_SYMMETRIC   "False"
#define DEF_SPEED       "1.0"
#define DEF_TWISTINESS  "4.0"
#define DEF_LAYERS      "4"

typedef struct {
  int color;
  GLfloat twist;
  GLfloat alpha;
  rotator *rot;
} disc;

typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  disc *discs;
  int ncolors;
  XColor *colors;

} hypnowheel_configuration;

static hypnowheel_configuration *bps = NULL;

static GLfloat speed;
static GLfloat twistiness;
static GLint nlayers;
static Bool do_wander;
static Bool do_symmetric;

static XrmOptionDescRec opts[] = {
  { "-speed",      ".speed",      XrmoptionSepArg, 0 },
  { "-twistiness", ".twistiness", XrmoptionSepArg, 0 },
  { "-layers",     ".layers",     XrmoptionSepArg, 0 },
  { "-wander",     ".wander",     XrmoptionNoArg, "True" },
  { "+wander",     ".wander",     XrmoptionNoArg, "False" },
  { "-symmetric",  ".symmetric",  XrmoptionNoArg, "True" },
  { "+symmetric",  ".symmetric",  XrmoptionNoArg, "False" },
};

static argtype vars[] = {
  {&do_wander,	  "wander",     "Wander",     DEF_WANDER,     t_Bool},
  {&do_symmetric, "symmetric",  "Symmetric",  DEF_SYMMETRIC,  t_Bool},
  {&speed,	  "speed",      "Speed",      DEF_SPEED,      t_Float},
  {&twistiness,	  "twistiness", "Twistiness", DEF_TWISTINESS, t_Float},
  {&nlayers,	  "layers",     "Layers",     DEF_LAYERS,     t_Int},
};

ENTRYPOINT ModeSpecOpt hypnowheel_opts = {
  countof(opts), opts, countof(vars), vars, NULL};


static void
draw_spiral (ModeInfo *mi, disc *d)
{
  int wire = MI_IS_WIREFRAME(mi);
  hypnowheel_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat rr = 0.5;
  int n = MI_COUNT(mi);
  int steps = n * (wire ? 3 : (n < 5 ? 60 : n < 10 ? 20 : 10));
  GLfloat dth = M_PI*2 / n;
  GLfloat dr = rr / steps;
  GLfloat th;
  GLfloat twist = d->twist;
  GLfloat dtwist = M_PI * 2 * twist / steps;
  double cscale = 65536.0;

  if (nlayers > 3 && !wire)
    cscale *= (nlayers-2);   /* don't wash out to white */

  glColor4f (bp->colors[d->color].red   / cscale,
             bp->colors[d->color].green / cscale, 
             bp->colors[d->color].blue  / cscale,
             d->alpha);
  for (th = 0; th < M_PI*2; th += dth)
    {
      GLfloat th1 = th;
      GLfloat r;
      glBegin (wire ? GL_LINE_STRIP : GL_QUAD_STRIP);
      for (r = 0; r <= rr; r += dr)
        {
          GLfloat th2 = th1 + dth/2 + dtwist;
          th1 += dtwist;
          glVertex3f (r * cos(th1), r * sin(th1), 0);
          if (! wire)
            glVertex3f (r * cos(th2), r * sin(th2), 0);
          mi->polygon_count++;
        }
      glEnd();
    }
}



/* Window management, etc
 */
ENTRYPOINT void
reshape_hypnowheel (ModeInfo *mi, int width, int height)
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


ENTRYPOINT void 
init_hypnowheel (ModeInfo *mi)
{
  hypnowheel_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);
  int i;

  if (!bps) {
    bps = (hypnowheel_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (hypnowheel_configuration));
    if (!bps) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }
  }

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_hypnowheel (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  if (! bp->rot)
    bp->rot = make_rotator (0, 0, 0, 0, speed * 0.0025, False);

  bp->ncolors = 1024;
  if (!bp->colors)
    bp->colors = (XColor *) calloc(bp->ncolors, sizeof(XColor));
  make_smooth_colormap (0, 0, 0,
                        bp->colors, &bp->ncolors,
                        False, 0, False);

  if (MI_COUNT(mi) < 2) MI_COUNT(mi) = 2;
  if (nlayers < 1) nlayers = 1;
  if (!bp->discs)
    bp->discs = (disc *) calloc (nlayers, sizeof (disc));

  for (i = 0; i < nlayers; i++)
    {
      double spin_speed   = speed * 0.2;
      double wander_speed = speed * 0.0012;
      double spin_accel   = 0.2;

      bp->discs[i].twist = 0;
      bp->discs[i].alpha = 1;
      bp->discs[i].color = i * bp->ncolors / nlayers;

      spin_speed   += frand (spin_speed   / 5);
      wander_speed += frand (wander_speed * 3);

      if (!bp->discs[i].rot)
      bp->discs[i].rot = make_rotator (spin_speed, spin_speed, spin_speed,
                                       spin_accel,
                                       (do_wander ? wander_speed : 0),
                                       True);
    }

  glDisable (GL_LIGHTING);
  glDisable (GL_DEPTH_TEST);
  glDepthMask (GL_FALSE);
  glDisable (GL_CULL_FACE);

  if (! wire)
    {
      glEnable (GL_BLEND);
      glBlendFunc (GL_ONE, GL_ONE);
    }
}


ENTRYPOINT void
draw_hypnowheel (ModeInfo *mi)
{
  hypnowheel_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int i;

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(bp->glx_context));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  {
    double x, y, z;
    get_position (bp->rot, &x, &y, &z, True);
    glTranslatef((x - 0.5) * 8,
                 (y - 0.5) * 8,
                 0);

    get_rotation (bp->rot, &x, &y, &z, True);
    glRotatef (x * 360, 1.0, 0.0, 0.0);
    glRotatef (y * 360, 0.0, 1.0, 0.0);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }

  mi->polygon_count = 0;

  glScalef (45, 45, 45);

  for (i = 0; i < nlayers; i++)
    {
      disc *d = &bp->discs[i];
      double x, y, z;
      rotator *rot = (do_symmetric ? bp->discs[(i & ~0x1)].rot : d->rot);
      Bool tick = (!do_symmetric || i == 0);

      glPushMatrix();

      d->color++;
      if (d->color >= bp->ncolors)
        d->color = 0;

      get_position (rot, &x, &y, &z, tick);
      x -= 0.5;
      y -= 0.5;
      x *= 0.1;
      y *= 0.1;

      glTranslatef (x, y, 0);
      d->twist = (z * twistiness *
                  ((i & 1) ? 1 : -1));

      get_rotation (rot, &x, &y, &z, tick);

      glRotatef (360 * z, 0, 0, 1);  /* rotation of this disc */

      draw_spiral (mi, &bp->discs[i]);
      glPopMatrix();
    }

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

ENTRYPOINT Bool
hypnowheel_handle_event (ModeInfo *mi, XEvent *event)
{
  if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      init_hypnowheel (mi);
      return True;
    }
  return False;
}


XSCREENSAVER_MODULE ("Hypnowheel", hypnowheel)

#endif /* USE_GL */
