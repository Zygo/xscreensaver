/* -*- Mode: C; tab-width: 4 -*- */
/* invert --- shere inversion */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)invert.c	5.01 2001/03/01 xlockmore";

#endif


/*-
 * invert.c - Sphere inversion
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 01-Nov-2000: Allocation checks
 * 199?: Written
 *
 * Tim Rowley (code from the Geometry Center <URL:http://www.geom.umn.edu/>
 *
 * This is a sphere eversion of William P. Thurston which was the subject
 * of the Geometry Center film "Outside In".  The code is based on the
 * software which was used to create the RIB files for the film.
 * Trying to figure it out from the full eversion is difficult.  If you get
 * a chance to look at the original film, it leads up the eversion nicely.
 * There is more information about the eversion, including the script from
 * the film, at: http://www.geom.umn.edu/docs/outreach/oi/
 *
 * Demonstration of turning a sphere inside out without creating
 * any kinks (two surfaces can occupy the same space at the same time).
 *
 * Revision History:
 * 01-Mar-2001: Added FPS stuff E.Lassauge <lassauge AT users.sourceforge.net>
 */

#ifdef VMS
/*-
 * due to a Bug/feature in VMS X11/Intrinsic.h has to be placed before xlock.
 * otherwise caddr_t is not defined correctly
 */
#include <X11/Intrinsic.h>
#endif
#include "xlock.h"
#include "visgl.h"

#ifdef MODE_invert

#include "i_linkage.h"
#define STEPS 75

ModeSpecOpt invert_opts =
{0, (XrmOptionDescRec *) NULL, 0, (argtype *) NULL, (OptionStruct *) NULL};

#ifdef USE_MODULES
ModStruct invert_description =
{(char *) "invert", (char *) "init_invert",
 (char *) "draw_invert", (char *) "release_invert",
 (char *) "draw_invert", (char *) "init_invert",
 (char *) NULL, &invert_opts,
 80000, 1, 1, 1, 64, 1.0, (char *) "",
 (char *) "Shows a sphere inverted without wrinkles", 0, NULL};

#endif


static spherestruct *spheres = (spherestruct *) NULL;

/* new window size or exposure */
static void
reshape(int width, int height)
{
  GLfloat     h = (GLfloat) height / (GLfloat) width;

  glViewport(0, 0, (GLint) width, (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-1.0, 1.0, -h, h, 5.0, 60.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.0, 0.0, -10.0);

  /* The depth buffer will be cleared, if needed, before the
   * next frame.  Right now we just want to black the screen.
   */
  glClear(GL_COLOR_BUFFER_BIT);
}


static void
pinit(void)
{
/*
  GLfloat front_mat[] = {.8, .7, .4, 1.0};
  GLfloat back_mat[] = {.508, .333, .774, 1.0};
  */

  GLfloat front_ambient[] = {.16, .14, .08, 1.0};
  GLfloat front_diffuse[] = {.56, .49, .28, 1.0};
  GLfloat front_specular[] = {1, 1, 0.8, 1.0};

  GLfloat back_ambient[] = {.1016, .0666, .1548, 1.0};
  GLfloat back_diffuse[] = {.254, .166, .387, 1.0};
  GLfloat back_specular[] = {.4, .2, .5, 1.0};

  /* spherestruct *gp = &spheres[MI_SCREEN(mi)]; */
  static GLfloat pos[4] =
  {5.0, 5.0, 10.0, 0.0};

  glLightfv(GL_LIGHT0, GL_POSITION, pos);
  glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_DEPTH_TEST);
  glShadeModel(GL_SMOOTH);

  glMaterialfv(GL_FRONT, GL_AMBIENT, front_ambient);
  glMaterialfv(GL_FRONT, GL_DIFFUSE, front_diffuse);
  glMaterialfv(GL_FRONT, GL_SPECULAR, front_specular);
  glMaterialf(GL_FRONT, GL_SHININESS, 32.0);

  glMaterialfv(GL_BACK, GL_AMBIENT, back_ambient);
  glMaterialfv(GL_BACK, GL_DIFFUSE, back_diffuse);
  glMaterialfv(GL_BACK, GL_SPECULAR, back_specular);
  glMaterialf(GL_BACK, GL_SHININESS, 38.0);
}

static void
free_invert(Display *display, spherestruct *gp)
{
  if (gp->glx_context) {
    /* Display lists MUST be freed while their glXContext is current. */
    glXMakeCurrent(display, gp->window, *(gp->glx_context));
    if (glIsList(gp->frames)) {
      glDeleteLists(gp->frames, STEPS);
      gp->frames = 0;
      /* Don't destroy the glXContext.  init_GL does that. */
    }
  }
  if (gp->partlist != NULL) {
    free(gp->partlist);
    gp->partlist = (char *) NULL;
  }
}

void
init_invert(ModeInfo * mi)
{
  spherestruct *gp;

  if (spheres == NULL) {
    if ((spheres = (spherestruct *) calloc(MI_NUM_SCREENS(mi),
					   sizeof (spherestruct))) == NULL)
      return;
  }
  gp = &spheres[MI_SCREEN(mi)];
  gp->window = MI_WINDOW(mi);

  gp->time = 0;
  gp->construction = 1;
  gp->partlist = (char *) NULL;
  gp->numsteps = STEPS;
  gp->view_rotx = NRAND(360);
  gp->view_roty = NRAND(360);
  gp->view_rotz = NRAND(360);
  if ((gp->glx_context = init_GL(mi)) != NULL) {
    if ((gp->frames = glGenLists(STEPS)) == 0) {
      free_invert(MI_DISPLAY(mi), gp);
      return;
    }
    reshape(MI_WIDTH(mi), MI_HEIGHT(mi));
    pinit();
  } else {
    MI_CLEARWINDOW(mi);
  }
}

void
draw_invert(ModeInfo * mi)
{
  Display    *display = MI_DISPLAY(mi);
  Window      window = MI_WINDOW(mi);
  /* int         angle_incr = MI_CYCLES(mi) ? MI_CYCLES(mi) : 2; */
  int         rot_incr = MI_COUNT(mi) ? MI_COUNT(mi) : 1;
  spherestruct *gp;

  if (spheres == NULL)
 	return;
  gp = &spheres[MI_SCREEN(mi)];

  if (!gp->glx_context)
    return;

  MI_IS_DRAWN(mi) = True;

  glDrawBuffer(GL_BACK);

  glXMakeCurrent(display, window, *(gp->glx_context));
  if (!invert_draw(gp)) {
    free_invert(display, gp);
    return;
  }

  /* let's do something so we don't get bored */
  if (gp->time == STEPS-1)
    gp->construction = 0;
  if (gp->time == STEPS-1)
    gp->forwards = 0;
  if (gp->time == 0)
    gp->forwards = 1;
  if (gp->forwards)
    gp->time++;
  else
    gp->time--;
  gp->view_rotx = (int) (gp->view_rotx + rot_incr) % 360;
  gp->view_roty = (int) (gp->view_roty + rot_incr) % 360;
  gp->view_rotz = (int) (gp->view_rotz + rot_incr) % 360;

  glFinish();
  if (MI_IS_FPS(mi)) do_fps (mi);
  glXSwapBuffers(display, window);
}

void
release_invert(ModeInfo * mi)
{
  if (spheres != NULL) {
    int         screen;

    for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
       free_invert(MI_DISPLAY(mi), &spheres[screen]);
    free(spheres);
    spheres = (spherestruct *) NULL;
  }
  FreeAllGL(mi);
}


/*********************************************************/

#endif /* MODE_invert */
