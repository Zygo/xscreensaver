/*-
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * Cubic Grid - a 3D lattice. The observer is located in the centre of 
 * a spinning finite lattice. As it rotates, various view-throughs appear and 
 * evolve. A simple idea with interesting results.
 * 
 * Vasek Potocek (Dec-28-2007)
 * vasek.potocek@post.cz
 */

#define DEFAULTS   "*delay:         20000         \n" \
                   "*showFPS:       False         \n" \
                   "*wireframe:     False         \n"

# define refresh_cubicgrid 0
#include "xlockmore.h"

#ifdef USE_GL

#define DEF_SPEED   "1.0"
#define DEF_DIV     "30"
#define DEF_ZOOM    "20"
#define DEF_BIGDOTS "True"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "rotator.h"
#include "gltrackball.h"

/*************************************************************************/

static int ticks;
static float size;
static float speed;
static Bool bigdots;

static argtype vars[] = {
  { &speed,   "speed",   "Speed",   DEF_SPEED,   t_Float },
  { &size,    "zoom",    "Zoom",    DEF_ZOOM,    t_Float },
  { &ticks,   "ticks",   "Ticks",   DEF_DIV,     t_Int },
  { &bigdots, "bigdots", "BigDots", DEF_BIGDOTS, t_Bool },
};

static XrmOptionDescRec opts[] = {
  { "-speed",   ".speed",   XrmoptionSepArg, 0 },
  { "-zoom",    ".zoom",    XrmoptionSepArg, 0 },
  { "-ticks",   ".ticks",   XrmoptionSepArg, 0 },
  { "-bigdots", ".bigdots", XrmoptionNoArg,  "True" },
  { "+bigdots", ".bigdots", XrmoptionNoArg,  "False" },
};

ENTRYPOINT ModeSpecOpt cubicgrid_opts = {countof(opts), opts, countof(vars), vars, NULL};

#ifdef USE_MODULES
ModStruct   cubicgrid_description =
{ "cubicgrid", "init_cubicgrid", "draw_cubicgrid", "release_cubicgrid",
  "draw_cubicgrid", "change_cubicgrid", NULL, &cubicgrid_opts,
  25000, 1, 1, 1, 1.0, 4, "",
  "Shows a rotating 3D lattice from inside", 0, NULL
};
#endif

typedef struct {
  GLXContext    *glx_context;
  GLfloat       ratio;
  GLint         list;

  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;
  int npoints;
} cubicgrid_conf;

static cubicgrid_conf *cubicgrid = NULL;

static const GLfloat zpos = -18.0;

/*************************************************************************/

ENTRYPOINT Bool
cubicgrid_handle_event (ModeInfo *mi, XEvent *event)
{
  cubicgrid_conf *cp = &cubicgrid[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button == Button1)
    {
      cp->button_down_p = True;
      gltrackball_start (cp->trackball,
                         event->xbutton.x, event->xbutton.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
    {
      cp->button_down_p = False;
      return True;
    }
  else if (event->xany.type == ButtonPress &&
           (event->xbutton.button == Button4 ||
            event->xbutton.button == Button5 ||
            event->xbutton.button == Button6 ||
            event->xbutton.button == Button7))
    {
      gltrackball_mousewheel (cp->trackball, event->xbutton.button, 2,
                              !!event->xbutton.state);
      return True;
    }
  else if (event->xany.type == MotionNotify &&
           cp->button_down_p)
    {
      gltrackball_track (cp->trackball,
                         event->xmotion.x, event->xmotion.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }

  return False;
}


static Bool draw_main(cubicgrid_conf *cp) 
{
  double x, y, z;

  glClear(GL_COLOR_BUFFER_BIT);
  glLoadIdentity();
  glTranslatef(0, 0, zpos);

  glScalef(size/ticks, size/ticks, size/ticks);

  /* Do it twice because we don't track the device's orientation. */
  glRotatef( current_device_rotation(), 0, 0, 1);
  gltrackball_rotate (cp->trackball);
  glRotatef(-current_device_rotation(), 0, 0, 1);

  get_rotation (cp->rot, &x, &y, &z, !cp->button_down_p);
  glRotatef (x * 360, 1.0, 0.0, 0.0);
  glRotatef (y * 360, 0.0, 1.0, 0.0);
  glRotatef (z * 360, 0.0, 0.0, 1.0);

  glTranslatef(-ticks/2.0, -ticks/2.0, -ticks/2.0);
  glCallList(cp->list);
  return True;
}

static void init_gl(ModeInfo *mi) 
{
  cubicgrid_conf *cp = &cubicgrid[MI_SCREEN(mi)];
  int x, y, z;
  float tf = ticks;

  glDrawBuffer(GL_BACK);
  if(bigdots) {
    glPointSize(2.0);
  }
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glShadeModel(GL_FLAT);

  cp->list = glGenLists(1);
  glNewList(cp->list, GL_COMPILE);
  if(MI_IS_MONO(mi)) {
    glColor3f(1.0, 1.0, 1.0);
    glBegin(GL_POINTS);
    for(x = 0; x < ticks; x++) {
      for(y = 0; y < ticks; y++) {
        for(z = 0; z < ticks; z++) {
          glVertex3f(x, y, z);
          cp->npoints++;
        }
      }
    }
    glEnd();
  }
  else
  {
    glBegin(GL_POINTS);
    for(x = 0; x < ticks; x++) {
      for(y = 0; y < ticks; y++) {
        for(z = 0; z < ticks; z++) {
          glColor3f(x/tf, y/tf, z/tf);
          glVertex3f(x, y, z);
          cp->npoints++;
        }
      }
    }
    glEnd();
  }
  glEndList();
}

/*************************************************************************/

ENTRYPOINT void reshape_cubicgrid(ModeInfo *mi, int width, int height) 
{
  cubicgrid_conf *cp = &cubicgrid[MI_SCREEN(mi)];
  if(!height) height = 1;
  cp->ratio = (GLfloat)width/(GLfloat)height;
  glViewport(0, 0, (GLint) width, (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(30.0, cp->ratio, 1.0, 100.0);
  glMatrixMode(GL_MODELVIEW);
  glClear(GL_COLOR_BUFFER_BIT);
}

ENTRYPOINT void release_cubicgrid(ModeInfo *mi) 
{
  if (cubicgrid != NULL) {
    int screen;
    for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
      cubicgrid_conf *cp = &cubicgrid[screen];
      if (cp->glx_context) {
        cp->glx_context = NULL;
      }
    }
    free((void *)cubicgrid);
    cubicgrid = NULL;
  }
  FreeAllGL(mi);
}

ENTRYPOINT void init_cubicgrid(ModeInfo *mi) 
{
  cubicgrid_conf *cp;
  if(!cubicgrid) {
    cubicgrid = (cubicgrid_conf *)calloc(MI_NUM_SCREENS(mi), sizeof(cubicgrid_conf));
    if(!cubicgrid) return;
  }
  cp = &cubicgrid[MI_SCREEN(mi)];

  if ((cp->glx_context = init_GL(mi)) != NULL) {
    init_gl(mi);
    reshape_cubicgrid(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  } else {
    MI_CLEARWINDOW(mi);
  }

  {
    double spin_speed = 0.045 * speed;
    double spin_accel = 0.005 * speed;

    cp->rot = make_rotator (spin_speed, spin_speed, spin_speed,
                            spin_accel, 0, True);
    cp->trackball = gltrackball_init ();
  }
}

ENTRYPOINT void draw_cubicgrid(ModeInfo * mi) 
{
  Display *display = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  cubicgrid_conf *cp;
  if (!cubicgrid) return;
  cp = &cubicgrid[MI_SCREEN(mi)];
  MI_IS_DRAWN(mi) = True;
  if (!cp->glx_context) return;
  glXMakeCurrent(display, window, *(cp->glx_context));
  if (!draw_main(cp)) {
    release_cubicgrid(mi);
    return;
  }
  mi->polygon_count = cp->npoints;
  if (MI_IS_FPS(mi)) do_fps (mi);
  glFlush();
  glXSwapBuffers(display, window);
}

#ifndef STANDALONE
ENTRYPOINT void change_cubicgrid(ModeInfo * mi) 
{
  cubicgrid_conf *cp = &cubicgrid[MI_SCREEN(mi)];
  if (!cp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(cp->glx_context));
  init_gl(mi);
}
#endif /* !STANDALONE */


XSCREENSAVER_MODULE ("CubicGrid", cubicgrid)

#endif
