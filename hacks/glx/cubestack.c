/* cubestack, Copyright (c) 2016 Jamie Zawinski <jwz@jwz.org>
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

# define refresh_cube 0
# define release_cube 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "colors.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */


#define DEF_WANDER    "True"
#define DEF_SPEED     "1.0"
#define DEF_THICKNESS "0.13"
#define DEF_OPACITY   "0.7"

typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;
  GLfloat state;
  GLfloat r;
  int length;
  int ncolors;
  XColor *colors;
  int ccolor;
} cube_configuration;

static cube_configuration *bps = NULL;

static GLfloat speed;
static GLfloat thickness;
static GLfloat opacity;
static Bool do_wander;

static XrmOptionDescRec opts[] = {
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" },
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
  { "-thickness",  ".thickness",  XrmoptionSepArg, 0 },
  { "-opacity",  ".opacity",  XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
  {&thickness, "thickness", "Thickness", DEF_THICKNESS, t_Float},
  {&opacity, "opacity", "Opacity", DEF_OPACITY, t_Float},
};

ENTRYPOINT ModeSpecOpt cube_opts = {countof(opts), opts, countof(vars), vars, NULL};


static int
draw_strut (ModeInfo *mi)
{
  int wire = MI_IS_WIREFRAME(mi);
  int polys = 0;
  GLfloat h;

  glPushMatrix();
  glFrontFace (GL_CCW);
  glNormal3f (0, 0, -1);
  glTranslatef (-0.5, -0.5, 0);
  glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLE_FAN);
  glVertex3f (0, 0, 0);
  glVertex3f (1, 0, 0);
  glVertex3f (1 - thickness, thickness, 0);
  glVertex3f (thickness, thickness, 0);
  glEnd();
  polys += 2;

  h = 0.5 - thickness;
  if (h >= 0.25)
    {
      glBegin (wire ? GL_LINE_LOOP : GL_TRIANGLE_FAN);
      glVertex3f (0.5, 0.5, 0);
      glVertex3f (0.5 - thickness/2, 0.5 - thickness/2, 0);
      glVertex3f (0.5 - thickness/2, 0.5 - h/2, 0);
      glVertex3f (0.5 + thickness/2, 0.5 - h/2, 0);
      glVertex3f (0.5 + thickness/2, 0.5 - thickness/2, 0);
      glEnd();
      polys += 3;
    }

  glPopMatrix();

  return polys;
}


static int
draw_face (ModeInfo *mi)
{
  int i;
  int polys = 0;
  for (i = 0; i < 4; i++)
    {
      polys += draw_strut (mi);
      glRotatef (90, 0, 0, 1);
    }
  return polys;
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


static int
draw_cube_1 (ModeInfo *mi, GLfloat state, GLfloat color[4], Bool bottom_p)
{
  int polys = 0;
  int istate = state;
  GLfloat r = state - istate;
  GLfloat a = color[3];

  r = ease_ratio (r);

# define COLORIZE(R) \
      color[3] = a * R; \
      glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color); \
      glColor4fv (color)

  if (bottom_p)
    {
      GLfloat r2 = (state < 0 ? 1 + state : 1);
      COLORIZE(r2);
      polys += draw_face (mi);		/* Bottom */
    }

  if (state >= 0)			/* Left */
    {
      GLfloat r2 = (istate == 0 ? r : 1);
      COLORIZE(r2);
      glPushMatrix();
      glTranslatef (-0.5, 0.5, 0);
      glRotatef (-r2 * 90, 0, 1, 0);
      glTranslatef (0.5, -0.5, 0);
      polys += draw_face (mi);
      glPopMatrix();
    }

  if (state >= 1)			/* Back */
    {
      GLfloat r2 = (istate == 1 ? r : 1);
      COLORIZE(r2);
      glPushMatrix();
      glTranslatef (-0.5, 0.5, 0);
      glRotatef ( 90, 0, 1, 0);
      glRotatef (-90, 0, 0, 1);
      glRotatef (-r2 * 90, 0, 1, 0);
      glTranslatef (0.5, -0.5, 0);
      polys += draw_face (mi);
      glPopMatrix();
    }

  if (state >= 2)			/* Right */
    {
      GLfloat r2 = (istate == 2 ? r : 1);
      COLORIZE(r2);
      glPushMatrix();
      glTranslatef (0.5, 0.5, 0);
      glRotatef ( 90, 0, 1, 0);
      glRotatef (-90, 0, 0, 1);
      glRotatef (-90, 0, 1, 0);
      glRotatef (-r2 * 90, 0, 1, 0);
      glTranslatef (-0.5, -0.5, 0);
      polys += draw_face (mi);
      glPopMatrix();
    }

  if (state >= 3)			/* Front */
    {
      GLfloat r2 = (istate == 3 ? r : 1);
      COLORIZE(r2);
      glPushMatrix();
      glTranslatef (0.5, 0.5, 0);
      glRotatef ( 90, 0, 1, 0);
      glRotatef (-90, 0, 0, 1);
      glRotatef (-180, 0, 1, 0);
      glTranslatef (-1, 0, 0);
      glRotatef (-r2 * 90, 0, 1, 0);
      glTranslatef (0.5, -0.5, 0);
      polys += draw_face (mi);
      glPopMatrix();
    }

  if (state >= 4)			/* Top */
    {
      GLfloat r2 = (istate == 4 ? r : 1);
      COLORIZE(r2);
      glPushMatrix();
      glTranslatef (0, 0, 1);
      glRotatef (-90, 0, 0, 1);
      glTranslatef (0.5, 0.5, 0);
      glRotatef (-90, 0, 1, 0);
      glRotatef (r2 * 90, 0, 1, 0);
      glTranslatef (-0.5, -0.5, 0);
      polys += draw_face (mi);
      glPopMatrix();
    }

  return polys;
}


static int
draw_cubes (ModeInfo *mi)
{
  cube_configuration *bp = &bps[MI_SCREEN(mi)];
  int polys = 0;
  GLfloat z = bp->state / 6;
  int i;
  GLfloat c[4];
  int c0 = bp->ccolor;
  GLfloat alpha = opacity;

  glPushMatrix();
  glTranslatef (0, 0, -1.5 - z);

  glTranslatef (0, 0, -bp->length);
  for (i = bp->length-1; i >= 0; i--)
    {
      int c1 = c0 - i - 1;
      if (c1 < 0) c1 += bp->ncolors;
      c[0] = bp->colors[c1].red   / 65536.0;
      c[1] = bp->colors[c1].green / 65536.0;
      c[2] = bp->colors[c1].blue  / 65536.0;
      c[3] = alpha;

      glTranslatef (0, 0, 1);
      polys += draw_cube_1 (mi, 5, c, i == bp->length - 1);
    }

  c[0] = bp->colors[c0].red   / 65536.0;
  c[1] = bp->colors[c0].green / 65536.0;
  c[2] = bp->colors[c0].blue  / 65536.0;
  c[3] = alpha;
  glTranslatef (0, 0, 1);
  polys += draw_cube_1 (mi, bp->state, c, bp->length == 0);

  glPopMatrix();

  return polys;
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_cube (ModeInfo *mi, int width, int height)
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
          bp->ncolors = 32;
          make_smooth_colormap (0, 0, 0,
                                bp->colors, &bp->ncolors,
                                False, 0, False);
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

  MI_INIT (mi, bps, NULL);

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_cube (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  if (!wire)
    {
      glDisable (GL_LIGHTING);
      glEnable(GL_DEPTH_TEST);
      glShadeModel (GL_SMOOTH);
      glEnable (GL_NORMALIZE);
      glDisable (GL_CULL_FACE);
      glEnable (GL_BLEND);
      glDisable (GL_DEPTH_TEST);
      glBlendFunc (GL_SRC_ALPHA, GL_ONE);
    }

  {
    double wander_speed = 0.005;
    bp->rot = make_rotator (0, 0, 0, 0,
                            do_wander ? wander_speed : 0,
                            False);
    bp->trackball = gltrackball_init (True);
  }

  if (thickness > 0.5)
    thickness = 0.5;
  if (thickness < 0.001)
    thickness = 0.001;

  bp->ncolors = 32;
  bp->colors = (XColor *) calloc(bp->ncolors, sizeof(XColor));
  make_smooth_colormap (0, 0, 0,
                        bp->colors, &bp->ncolors,
                        False, 0, False);
  bp->state = -1;
}


ENTRYPOINT void
draw_cube (ModeInfo *mi)
{
  cube_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(bp->glx_context));
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  {
    double x, y, z;
    get_position (bp->rot, &x, &y, &z, !bp->button_down_p);
    glTranslatef((x - 0.5) * 4,
                 (y - 0.5) * 4,
                 (z - 0.5) * 2);

    gltrackball_rotate (bp->trackball);
  }

  mi->polygon_count = 0;

  glScalef (6, 6, 6);
  glRotatef (-45, 1, 0, 0);
  glRotatef (20, 0, 0, 1);
  glRotatef (bp->r, 0, 0, 1);

  mi->polygon_count = draw_cubes (mi);
  glPopMatrix ();

  if (!bp->button_down_p)
    {
      int max = 6;
      bp->state += speed * 0.015;
      bp->r += speed * 0.05;
      while (bp->r > 360)
        bp->r -= 360;
      while (bp->state > max)
        {
          bp->state -= max;
          bp->length++;
          bp->ccolor++;
          if (bp->ccolor > bp->ncolors)
            bp->ccolor = 0;
        }

      if (bp->length > 20)
        bp->length = 20;
    }

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

XSCREENSAVER_MODULE_2 ("CubeStack", cubestack, cube)

#endif /* USE_GL */
