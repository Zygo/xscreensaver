/* cubestorm, Copyright (c) 2003, 2004 Jamie Zawinski <jwz@jwz.org>
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

#define PROGCLASS	"CubeStorm"
#define HACK_INIT	init_cube
#define HACK_DRAW	draw_cube
#define HACK_RESHAPE	reshape_cube
#define HACK_HANDLE_EVENT cube_handle_event
#define EVENT_MASK      PointerMotionMask
#define sws_opts	xlockmore_opts

#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_SPEED       "1.0"
#define DEF_THICKNESS   "0.06"
#define DEF_COUNT       "4"

#define DEFAULTS	"*delay:	30000       \n" \
			"*count:      " DEF_COUNT   "\n" \
			"*showFPS:      False       \n" \
	               	"*fpsSolid:     True        \n" \
			"*wireframe:    False       \n" \


#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "colors.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#include <GL/glu.h>

typedef struct {
  rotator *rot;
  int ccolor;
} subcube;

typedef struct {
  GLXContext *glx_context;
  trackball_state *trackball;
  Bool button_down_p;
  Bool clear_p;

  GLuint cube_list;

  int ncolors;
  XColor *colors;

  subcube *subcubes;

} cube_configuration;

static cube_configuration *bps = NULL;

static Bool do_spin;
static Bool do_wander;
static GLfloat speed;
static GLfloat thickness;

static XrmOptionDescRec opts[] = {
  { "-spin",   ".spin",   XrmoptionNoArg, "True" },
  { "+spin",   ".spin",   XrmoptionNoArg, "False" },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" },
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
  { "-thickness", ".thickness",  XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_Bool},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
  {&thickness, "thickness", "Thickness",  DEF_THICKNESS,  t_Float},
};

ModeSpecOpt sws_opts = {countof(opts), opts, countof(vars), vars, NULL};


static void
draw_face (ModeInfo *mi)
{
  int wire = MI_IS_WIREFRAME(mi);

  int i;
  GLfloat t = thickness / 2;
  GLfloat a = -0.5;
  GLfloat b =  0.5;

  if (t <= 0) t = 0.001;
  else if (t > 0.5) t = 0.5;

  glPushMatrix();
  glFrontFace(GL_CW);

  for (i = 0; i < 4; i++)
    {
      glNormal3f (0, 0, -1);
      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glVertex3f (a,   a,   a);
      glVertex3f (b,   a,   a);
      glVertex3f (b-t, a+t, a);
      glVertex3f (a+t, a+t, a);
      glEnd();

      glNormal3f (0, 1, 0);
      glBegin (wire ? GL_LINE_LOOP : GL_QUADS);
      glVertex3f (b-t, a+t, a);
      glVertex3f (b-t, a+t, a+t);
      glVertex3f (a+t, a+t, a+t);
      glVertex3f (a+t, a+t, a);
      glEnd();

      glRotatef(90, 0, 0, 1);
    }
  glPopMatrix();
}

static void
draw_faces (ModeInfo *mi)
{
  glPushMatrix();
  draw_face (mi);
  glRotatef (90,  0, 1, 0); draw_face (mi);
  glRotatef (90,  0, 1, 0); draw_face (mi);
  glRotatef (90,  0, 1, 0); draw_face (mi);
  glRotatef (90,  1, 0, 0); draw_face (mi);
  glRotatef (180, 1, 0, 0); draw_face (mi);
  glPopMatrix();
}


static void
new_cube_colors (ModeInfo *mi)
{
  cube_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  bp->ncolors = 128;
  if (bp->colors) free (bp->colors);
  bp->colors = (XColor *) calloc(bp->ncolors, sizeof(XColor));
  make_smooth_colormap (0, 0, 0,
                        bp->colors, &bp->ncolors,
                        False, 0, False);
  for (i = 0; i < MI_COUNT(mi); i++)
    bp->subcubes[i].ccolor = random() % bp->ncolors;
}


/* Window management, etc
 */
void
reshape_cube (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (30.0, 1/h, 1.0, 100.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 0.0, 45.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}


Bool
cube_handle_event (ModeInfo *mi, XEvent *event)
{
  cube_configuration *bp = &bps[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button == Button1)
    {
      bp->button_down_p = True;
      gltrackball_start (bp->trackball,
                         event->xbutton.x, event->xbutton.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
    {
      bp->button_down_p = False;
      return True;
    }
  else if (event->xany.type == ButtonPress &&
           (event->xbutton.button == Button4 ||
            event->xbutton.button == Button5))
    {
      gltrackball_mousewheel (bp->trackball, event->xbutton.button, 10,
                              !!event->xbutton.state);
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
  else if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);
      if (c == ' ')
        {
          glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
          return True;
        }
      else if (c == '\r' || c == '\n')
        {
          new_cube_colors (mi);
          return True;
        }
    }
  return False;
}


void 
init_cube (ModeInfo *mi)
{
  cube_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);
  int i;

  if (!bps) {
    bps = (cube_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (cube_configuration));
    if (!bps) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }

    bp = &bps[MI_SCREEN(mi)];
  }

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  if (MI_COUNT(mi) <= 0) MI_COUNT(mi) = 1;

  bp->trackball = gltrackball_init ();
  bp->subcubes = (subcube *) calloc (MI_COUNT(mi), sizeof(subcube));
  for (i = 0; i < MI_COUNT(mi); i++)
    {
      double wander_speed, spin_speed, spin_accel;

      if (i == 0)
        {
          wander_speed = 0.05 * speed;
          spin_speed   = 10.0 * speed;
          spin_accel   = 4.0  * speed;
        }
      else
        {
          wander_speed = 0;
          spin_speed   = 4.0 * speed;
          spin_accel   = 2.0 * speed;
        }

      bp->subcubes[i].rot = make_rotator (do_spin ? spin_speed : 0,
                                          do_spin ? spin_speed : 0,
                                          do_spin ? spin_speed : 0,
                                          spin_accel,
                                          do_wander ? wander_speed : 0,
                                          True);
    }

  bp->colors = 0;
  new_cube_colors (mi);

  reshape_cube (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

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

  bp->cube_list = glGenLists (1);
  glNewList (bp->cube_list, GL_COMPILE);
  draw_faces (mi);
  glEndList ();

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}


void
draw_cube (ModeInfo *mi)
{
  cube_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  int wire = MI_IS_WIREFRAME(mi);
  int i;
  double x, y, z;

  if (!bp->glx_context)
    return;

  glShadeModel(GL_SMOOTH);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);

  if (bp->clear_p)   /* we're in "no vapor trails" mode */
    {
      glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
      if (! (random() % (int) (25 / speed)))
        bp->clear_p = False;
    }
  else
    {
      if (! (random() % (int) (200 / speed)))
        {
          bp->clear_p = True;
          new_cube_colors (mi);
        }
    }

  glPushMatrix ();

  glScalef(1.1, 1.1, 1.1);

  mi->polygon_count = 0;

  get_position (bp->subcubes[0].rot, &x, &y, &z, !bp->button_down_p);
  glTranslatef((x - 0.5) * 15,
               (y - 0.5) * 15,
               (z - 0.5) * 30);
  gltrackball_rotate (bp->trackball);

  glScalef (4.0, 4.0, 4.0);

  for (i = 0; i < MI_COUNT(mi); i++)
    {
      static GLfloat bcolor[4] = {0.0, 0.0, 0.0, 1.0};
      static GLfloat bspec[4]  = {1.0, 1.0, 1.0, 1.0};
      static GLfloat bshiny    = 128.0;

      glPushMatrix();

      if (i != 0)  /* N+1 cubes rotate relative to cube 0 */
        {
          get_rotation (bp->subcubes[0].rot, &x, &y, &z, False);
          glRotatef (x * 360, 1.0, 0.0, 0.0);
          glRotatef (y * 360, 0.0, 1.0, 0.0);
          glRotatef (z * 360, 0.0, 0.0, 1.0);
        }

      get_rotation (bp->subcubes[i].rot, &x, &y, &z, !bp->button_down_p);
      glRotatef (x * 360, 1.0, 0.0, 0.0);
      glRotatef (y * 360, 0.0, 1.0, 0.0);
      glRotatef (z * 360, 0.0, 0.0, 1.0);

      bcolor[0] = bp->colors[bp->subcubes[i].ccolor].red   / 65536.0;
      bcolor[1] = bp->colors[bp->subcubes[i].ccolor].green / 65536.0;
      bcolor[2] = bp->colors[bp->subcubes[i].ccolor].blue  / 65536.0;
      bp->subcubes[i].ccolor++;
      if (bp->subcubes[i].ccolor >= bp->ncolors)
        bp->subcubes[i].ccolor = 0;

      if (wire)
        glColor3f (bcolor[0], bcolor[1], bcolor[2]);
      else
        {
          glMaterialfv (GL_FRONT, GL_SPECULAR,            bspec);
          glMateriali  (GL_FRONT, GL_SHININESS,           bshiny);
          glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, bcolor);
        }

      glCallList (bp->cube_list);
      mi->polygon_count += (4 * 2 * 6);

      glPopMatrix();
    }

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}

#endif /* USE_GL */
