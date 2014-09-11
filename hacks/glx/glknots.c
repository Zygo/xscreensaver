/* glknots, Copyright (c) 2003-2014 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Generates some 3D knots (closed loops).
 * Inspired by Paul Bourke <pbourke@swin.edu.au> at
 * http://astronomy.swin.edu.au/~pbourke/curves/knot/
 */

#define DEFAULTS	"*delay:	30000       \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \

# define refresh_knot 0
# define release_knot 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "colors.h"
#include "tube.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#define DEF_SPIN        "XYZ"
#define DEF_WANDER      "True"
#define DEF_SPEED       "1.0"
#define DEF_THICKNESS   "0.3"
#define DEF_SEGMENTS    "800"
#define DEF_DURATION    "8"

typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;

  GLuint knot_list;

  int ncolors;
  XColor *colors;
  int ccolor;

  int mode;  /* 0 = normal, 1 = out, 2 = in */
  int mode_tick;
  Bool clear_p;

  time_t last_time;
  int draw_tick;

} knot_configuration;

static knot_configuration *bps = NULL;

static char *do_spin;
static GLfloat speed;
static Bool do_wander;
static GLfloat thickness;
static unsigned int segments;
static int duration;

static XrmOptionDescRec opts[] = {
  { "-spin",   ".spin",   XrmoptionSepArg, 0 },
  { "+spin",   ".spin",   XrmoptionNoArg, "" },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" },
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
  { "-thickness", ".thickness",  XrmoptionSepArg, 0 },
  { "-segments",  ".segments",   XrmoptionSepArg, 0 },
  { "-duration",  ".duration",   XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_String},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
  {&thickness, "thickness", "Thickness",  DEF_THICKNESS, t_Float},
  {&segments,  "segments",  "Segments",   DEF_SEGMENTS,  t_Int},
  {&duration,  "duration",  "Duration",   DEF_DURATION,  t_Int},
};

ENTRYPOINT ModeSpecOpt knot_opts = {countof(opts), opts, countof(vars), vars, NULL};


static void
make_knot (ModeInfo *mi)
{
  int wire = MI_IS_WIREFRAME(mi);

  GLfloat diam = (4 * thickness);
  int faces = (wire ? 3 : 6);

  unsigned int i;
  double x, y, z, ox=0, oy=0, oz=0;
  double mu;

  double p[9];

  Bool blobby_p = (0 == (random() % 5));
  Bool type = (random() % 2);

  for (i = 0; i < countof(p); i++)
    {
      p[i] = 1 + (random() % 4);
      if (! (random() % 3))
        p[i] += (random() % 5);
    }

  if (type == 1)
    {
      p[0] += 4;
      p[1] *= ((p[0] + p[0]) / 10);
      blobby_p = False;
    }

  mi->polygon_count = 0;

  for (i = 0; i <= segments; i++)
    {
      if (type == 0)
        {
          mu = i * (M_PI * 2) / segments;
          x = 10 * (cos(mu) + cos(p[0]*mu)) + cos(p[1]*mu) + cos(p[2]*mu);
          y = 6 * sin(mu) + 10 * sin(p[3]*mu);
          z = 16 * sin(p[4]*mu) * sin(p[5]*mu/2) + p[6]*sin(p[7]*mu) -
            2 * sin(p[8]*mu);
        }
      else if (type == 1)
        {
          mu = i * (M_PI * 2) * p[0] / (double) segments;
          x = 10 * cos(mu) * (1 + cos(p[1] * mu/ p[0]) / 2.0);
          y = 25 * sin(p[1] * mu / p[0]) / 2.0;
          z = 10 * sin(mu) * (1 + cos(p[1] * mu/ p[0]) / 2.0);
        }
      else
        abort();

      if (i != 0)
        {
          GLfloat dist = sqrt ((x-ox)*(x-ox) +
                               (y-oy)*(y-oy) +
                               (z-oz)*(z-oz));
          GLfloat di;
          if (!blobby_p)
            di = diam;
          else
            {
              di = dist * (segments / 500.0);
              di = (di * di * 3);
            }

          mi->polygon_count += tube (ox, oy, oz,
                                     x, y, z,
                                     di, dist/3,
                                     faces, True, wire, wire);
        }

      ox = x;
      oy = y;
      oz = z;
   }
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_knot (ModeInfo *mi, int width, int height)
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
new_knot (ModeInfo *mi)
{
  knot_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;

  bp->clear_p = !!(random() % 15);

  bp->ncolors = 128;
  bp->colors = (XColor *) calloc(bp->ncolors, sizeof(XColor));
  make_smooth_colormap (0, 0, 0,
                        bp->colors, &bp->ncolors,
                        False, 0, False);

  for (i = 0; i < bp->ncolors; i++)
    {
      /* make colors twice as bright */
      bp->colors[i].red   = (bp->colors[i].red   >> 2) + 0x7FFF;
      bp->colors[i].green = (bp->colors[i].green >> 2) + 0x7FFF;
      bp->colors[i].blue  = (bp->colors[i].blue  >> 2) + 0x7FFF;
    }

  glNewList (bp->knot_list, GL_COMPILE);
  make_knot (mi);
  glEndList ();
}


ENTRYPOINT Bool
knot_handle_event (ModeInfo *mi, XEvent *event)
{
  knot_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      bp->last_time = 1;
      return True;
    }

  return False;
}



ENTRYPOINT void 
init_knot (ModeInfo *mi)
{
  knot_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);

  if (!bps) {
    bps = (knot_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (knot_configuration));
    if (!bps) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }
  }

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  if (thickness <= 0) thickness = 0.001;
  else if (thickness > 1) thickness = 1;

  if (segments < 10) segments = 10;

  reshape_knot (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

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
    Bool spinx=False, spiny=False, spinz=False;
    double spin_speed   = 2.0;
    double wander_speed = 0.05;
    double spin_accel   = 0.2;

    char *s = do_spin;
    while (*s)
      {
        if      (*s == 'x' || *s == 'X') spinx = True;
        else if (*s == 'y' || *s == 'Y') spiny = True;
        else if (*s == 'z' || *s == 'Z') spinz = True;
        else if (*s == '0') ;
        else
          {
            fprintf (stderr,
         "%s: spin must contain only the characters X, Y, or Z (not \"%s\")\n",
                     progname, do_spin);
            exit (1);
          }
        s++;
      }

    bp->rot = make_rotator (spinx ? spin_speed : 0,
                            spiny ? spin_speed : 0,
                            spinz ? spin_speed : 0,
                            spin_accel,
                            do_wander ? wander_speed : 0,
                            (spinx && spiny && spinz));
    bp->trackball = gltrackball_init (True);
  }

  bp->knot_list = glGenLists (1);
  new_knot(mi);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}


ENTRYPOINT void
draw_knot (ModeInfo *mi)
{
  knot_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  GLfloat bcolor[4] = {0.0, 0.0, 0.0, 1.0};
  GLfloat bspec[4]  = {1.0, 1.0, 1.0, 1.0};
  GLfloat bshiny    = 128.0;

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(bp->glx_context));

  if (bp->mode == 0)
    {
      if (bp->draw_tick++ > 10)
        {
          time_t now = time((time_t *) 0);
          if (bp->last_time == 0) bp->last_time = now;
          bp->draw_tick = 0;
          if (!bp->button_down_p &&
              now - bp->last_time >= duration)
            {
              bp->mode = 1;    /* go out */
              bp->mode_tick = 10 * speed;
              bp->last_time = now;
            }
        }
    }
  else if (bp->mode == 1)   /* out */
    {
      if (--bp->mode_tick <= 0)
        {
          new_knot (mi);
          bp->mode_tick = 10 * speed;
          bp->mode = 2;  /* go in */
        }
    }
  else if (bp->mode == 2)   /* in */
    {
      if (--bp->mode_tick <= 0)
        bp->mode = 0;  /* normal */
    }
  else
    abort();

  glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  if (bp->clear_p)
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

  bcolor[0] = bp->colors[bp->ccolor].red   / 65536.0;
  bcolor[1] = bp->colors[bp->ccolor].green / 65536.0;
  bcolor[2] = bp->colors[bp->ccolor].blue  / 65536.0;
  bp->ccolor++;
  if (bp->ccolor >= bp->ncolors) bp->ccolor = 0;

  glMaterialfv (GL_FRONT, GL_SPECULAR,            bspec);
  glMateriali  (GL_FRONT, GL_SHININESS,           bshiny);
  glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, bcolor);

  glScalef(0.25, 0.25, 0.25);

  if (bp->mode != 0)
    {
      GLfloat s = (bp->mode == 1
                   ? bp->mode_tick / (10 * speed)
                   : ((10 * speed) - bp->mode_tick + 1) / (10 * speed));
      glScalef (s, s, s);
    }

  glCallList (bp->knot_list);

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

XSCREENSAVER_MODULE_2 ("GLKnots", glknots, knot)

#endif /* USE_GL */
