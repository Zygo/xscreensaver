/* StonerView: An eccentric visual toy.
   Copyright 1998-2001 by Andrew Plotkin (erkyrath@eblong.com)

   For the latest version, source code, and links to more of my stuff, see:
   http://www.eblong.com/zarf/stonerview.html

   Permission to use, copy, modify, distribute, and sell this software and its
   documentation for any purpose is hereby granted without fee, provided that
   the above copyright notice appear in all copies and that both that
   copyright notice and this permission notice appear in supporting
   documentation.  No representations are made about the suitability of this
   software for any purpose.  It is provided "as is" without express or 
   implied warranty.
*/

/* Ported away from GLUT (so that it can do `-root' and work with xscreensaver)
   by Jamie Zawinski <jwz@jwz.org>, 22-Jan-2001.

   Revamped to work in the xlockmore framework so that it will also work
   with the MacOS X port of xscreensaver by jwz, 21-Feb-2006.
 */

#define DEFAULTS	"*delay:	20000       \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n"

# define refresh_stonerview 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"

#ifdef USE_GL /* whole file */

#include "stonerview.h"
#include "gltrackball.h"

typedef struct {
  GLXContext *glx_context;
  stonerview_state *st;
  trackball_state *trackball;
  Bool button_down_p;
} stonerview_configuration;

static stonerview_configuration *bps = NULL;

ENTRYPOINT ModeSpecOpt stonerview_opts = {0, NULL, 0, NULL, NULL};


ENTRYPOINT void
reshape_stonerview (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport(0, 0, (GLint) width, (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-1.0, 1.0, -h, h, 5.0, 60.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.0, 0.0, -40.0);
}


ENTRYPOINT void 
init_stonerview (ModeInfo *mi)
{
  stonerview_configuration *bp;

  if (!bps) {
    bps = (stonerview_configuration *)
      calloc (MI_NUM_SCREENS(mi), sizeof (stonerview_configuration));
    if (!bps) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }

    bp = &bps[MI_SCREEN(mi)];
  }

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  bp->trackball = gltrackball_init ();
  bp->st = init_view(MI_IS_WIREFRAME(mi));
  init_move(bp->st);

  reshape_stonerview (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
}


ENTRYPOINT void
draw_stonerview (ModeInfo *mi)
{
  stonerview_configuration *bp = &bps[MI_SCREEN(mi)];

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(bp->glx_context));

  glPushMatrix ();
  gltrackball_rotate (bp->trackball);
  win_draw(bp->st);
  if (! bp->button_down_p)
    move_increment(bp->st);
  glPopMatrix ();

  mi->polygon_count = NUM_ELS;
  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(MI_DISPLAY (mi), MI_WINDOW(mi));
}

ENTRYPOINT void
release_stonerview (ModeInfo *mi)
{
  if (bps) {
    int screen;
    for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
      stonerview_configuration *bp = &bps[screen];
      if (bp->st)
        win_release (bp->st);
    }
    free (bps);
    bps = 0;
  }
  FreeAllGL(mi);
}

ENTRYPOINT Bool
stonerview_handle_event (ModeInfo *mi, XEvent *event)
{
  stonerview_configuration *bp = &bps[MI_SCREEN(mi)];

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

  return False;
}

XSCREENSAVER_MODULE ("StonerView", stonerview)

#endif /* USE_GL */
