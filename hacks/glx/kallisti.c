/* kallisti, Copyright © 2023-2024 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * The golden apple of discord: https://www.jwz.org/blog/2023/09/ti-kallisti/
 */

#define DEFAULTS	"*delay:	30000       \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \

# define release_chao 0
#define DEF_SPIN        "Y"
#define DEF_WANDER      "FALSE"
#define DEF_SPEED       "1.0"

#include "xlockmore.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#include "gllist.h"

extern struct gllist *kallisti_model;


typedef struct {
  GLXContext *glx_context;
  trackball_state *trackball;
  rotator *rot;
  Bool button_down_p;
  GLuint chao_list;
  GLuint texture;
  enum { FADE_IN, DRAW } state;
  GLfloat tick;
  GLfloat th;
} chao_configuration;

static chao_configuration *bps = NULL;

static char *do_spin;
static GLfloat speed;
static Bool do_wander;

static XrmOptionDescRec opts[] = {
  { "-spin",   ".spin",   XrmoptionSepArg, 0 },
  { "+spin",   ".spin",   XrmoptionNoArg, "" },
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" }
};

static argtype vars[] = {
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_String},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
};

ENTRYPOINT ModeSpecOpt chao_opts = {
  countof(opts), opts, countof(vars), vars, NULL};


ENTRYPOINT void
reshape_chao (ModeInfo *mi, int width, int height)
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
  gluPerspective (30.0, 1/h, 1.0, 100);

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
chao_handle_event (ModeInfo *mi, XEvent *event)
{
  chao_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;

  return False;
}


ENTRYPOINT void
init_chao (ModeInfo *mi)
{
  chao_configuration *bp;
  int wire = MI_IS_WIREFRAME(mi);

  MI_INIT (mi, bps);
  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_chao (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  {
    Bool spinx=False, spiny=False, spinz=False;
    double spin_speed   = 0.6 * speed;
    double spin_accel   = 0.3;
    double wander_speed = 0.01 * speed;

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
                            False);
    bp->trackball = gltrackball_init (True);
  }

  bp->chao_list = glGenLists (1);
  glNewList (bp->chao_list, GL_COMPILE);
  renderList (kallisti_model, wire);
  glEndList ();

  bp->state = FADE_IN;
  bp->tick = 0;
}


ENTRYPOINT void
draw_chao (ModeInfo *mi)
{
  chao_configuration *bp = &bps[MI_SCREEN(mi)];
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

  switch (bp->state) {
  case FADE_IN:
    bp->tick += 0.01 * speed;
    if (bp->tick >= 1.0)
      {
        bp->tick = 1;
        bp->state = DRAW;
      }
    break;
  case DRAW:
    break;
  default:
    abort();
  }

  if (! MI_IS_WIREFRAME(mi))
    {
      GLfloat pos0[]  = { -30.0,  15.0,   3.0, 1.0 };
      GLfloat pos1[]  = {  24.0, -12.0, -12.0, 1.0 };
      GLfloat pos2[]  = {   0.0,   0.0,   0.0, 1.0 };

      GLfloat amb0[]  = { 0.4, 0.4, 0.4, 1.0 };
      GLfloat amb1[]  = { 0.0, 0.0, 0.0, 1.0 };
      GLfloat amb2[]  = { 0.0, 0.0, 0.0, 1.0 };
      GLfloat dif0[]  = { 1.0, 1.0, 1.0, 1.0 };
      GLfloat dif1[]  = { 0.7, 0.1, 0.1, 1.0 };
      GLfloat dif2[]  = { 0.6, 0.6, 0.0, 1.0 };

      amb0[0] *= bp->tick; amb0[1] *= bp->tick; amb0[2] *= bp->tick;
      dif0[0] *= bp->tick; dif0[1] *= bp->tick; dif0[2] *= bp->tick;
      amb1[0] *= bp->tick; amb1[1] *= bp->tick; amb1[2] *= bp->tick;
      dif1[0] *= bp->tick; dif1[1] *= bp->tick; dif1[2] *= bp->tick;
      amb2[0] *= bp->tick; amb2[1] *= bp->tick; amb2[2] *= bp->tick;
      dif2[0] *= bp->tick; dif2[1] *= bp->tick; dif2[2] *= bp->tick;

      glLightfv(GL_LIGHT0, GL_POSITION, pos0);
      glLightfv(GL_LIGHT1, GL_POSITION, pos1);

      glLightfv(GL_LIGHT0, GL_AMBIENT,  amb0);
      glLightfv(GL_LIGHT1, GL_AMBIENT,  amb1);
      glLightfv(GL_LIGHT2, GL_AMBIENT,  amb2);
      glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif0);
      glLightfv(GL_LIGHT1, GL_DIFFUSE,  dif1);
      glLightfv(GL_LIGHT2, GL_DIFFUSE,  dif2);

      glEnable(GL_LIGHTING);
      glEnable(GL_LIGHT0);
      glEnable(GL_LIGHT1);
      glEnable(GL_LIGHT2);

      glEnable(GL_DEPTH_TEST);
      glEnable(GL_TEXTURE_2D);
      glEnable(GL_NORMALIZE);
      glEnable(GL_CULL_FACE);

      { /* Spinny light */
        GLfloat s = 10;
        GLfloat th2 = M_PI * 0.65;

        bp->th += 0.02 * speed;
        while (bp->th > M_PI * 2)
          bp->th -= M_PI * 2;

        pos2[0] = s * cos (th2) * cos (-bp->th);
        pos2[1] = s * sin (-bp->th);
        pos2[2] = s * sin (th2) * cos (-bp->th);
        glLightfv (GL_LIGHT2, GL_POSITION, pos2);

# if 0
        glDisable (GL_LIGHTING);
        glBegin (GL_LINES);
        glVertex3f (0, 0, 0);
        glVertex3fv (pos2);
        glEnd();
        glEnable (GL_LIGHTING);
# endif
      }

    }

  glPushMatrix ();
  glRotatef (current_device_rotation(), 0, 0, 1);
  glScalef (10, 10, 10);

  glRotatef (15, 1, 0, 0);
  glTranslatef (0, -0.3, 0);

  {
    double x, y, z;
    get_position (bp->rot, &x, &y, &z, !bp->button_down_p);
    glTranslatef((x - 0.5) * 1,
                 (y - 0.5) * 0.8,
                 (z - 1) * 2);

    gltrackball_rotate (bp->trackball);

    get_rotation (bp->rot, &x, &y, &z, !bp->button_down_p);
    glRotatef (x * 360, 1.0, 0.0, 0.0);
    glRotatef (y * 360, 0.0, 1.0, 0.0);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }

  glRotatef (-90, 1, 0, 0);


  mi->polygon_count = 0;

  {
    GLfloat amb[] = { 0.33, 0.22, 0.03, 1.00 };
    GLfloat spc[] = { 0.78, 0.57, 0.11, 1.00 };
    GLfloat dif[] = { 0.99, 0.91, 0.81, 1.00 };
    GLfloat shiny = 27.80;

    amb[0] *= bp->tick; amb[1] *= bp->tick; amb[2] *= bp->tick;
    spc[0] *= bp->tick; spc[1] *= bp->tick; spc[2] *= bp->tick;
    dif[0] *= bp->tick; dif[1] *= bp->tick; dif[2] *= bp->tick;

    glMaterialfv(GL_FRONT, GL_AMBIENT,   amb);
    glMaterialfv(GL_FRONT, GL_DIFFUSE,   spc);
    glMaterialfv(GL_FRONT, GL_SPECULAR,  dif);
    glMaterialf (GL_FRONT, GL_SHININESS, shiny);
  }

# if 0
  glDisable (GL_LIGHTING);
  glBegin (GL_LINES);
  glVertex3f (0, 0, -100); glVertex3f (0, 0, 100);
  glVertex3f (0, -100, 0); glVertex3f (0, 100, 0);
  glVertex3f (-100, 0, 0); glVertex3f (100, 0, 0);
  glEnd();
  glEnable (GL_LIGHTING);
# endif

  /* Align axis with core of apple, since the model's bbox isn't centered. */
  glTranslatef (0.0534, 0.0394, -0.03);

  glCallList (bp->chao_list);
  mi->polygon_count += kallisti_model->points / 3;

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_chao (ModeInfo *mi)
{
  chao_configuration *bp = &bps[MI_SCREEN(mi)];
  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  free_rotator (bp->rot);
  if (glIsList(bp->chao_list)) glDeleteLists(bp->chao_list, 1);
  if (bp->trackball) gltrackball_free (bp->trackball);
}

XSCREENSAVER_MODULE_2 ("Kallisti", kallisti, chao)

#endif /* USE_GL */
