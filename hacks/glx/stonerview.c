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

# define release_stonerview 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"

#ifdef USE_GL /* whole file */

#include "stonerview.h"
#include "gltrackball.h"

#define DEF_TRANSPARENT "True"

typedef struct {
  GLXContext *glx_context;
  stonerview_state *st;
  trackball_state *trackball;
  Bool button_down_p;
} stonerview_configuration;

static stonerview_configuration *bps = NULL;

static Bool transparent_p;


static XrmOptionDescRec opts[] = {
  { "-transparent",   ".transparent",   XrmoptionNoArg, "True" },
  { "+transparent",   ".transparent",   XrmoptionNoArg, "False" },
};

static argtype vars[] = {
  {&transparent_p, "transparent", "Transparent", DEF_TRANSPARENT, t_Bool},
};

ENTRYPOINT ModeSpecOpt stonerview_opts = {countof(opts), opts, countof(vars), vars, NULL};


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

  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  bp->trackball = gltrackball_init (False);
  bp->st = stonerview_init_view(MI_IS_WIREFRAME(mi), transparent_p);
  stonerview_init_move(bp->st);

  reshape_stonerview (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  clear_gl_error(); /* WTF? sometimes "invalid op" from glViewport! */
}


ENTRYPOINT void
draw_stonerview (ModeInfo *mi)
{
  stonerview_configuration *bp = &bps[MI_SCREEN(mi)];

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  glPushMatrix ();
  glRotatef( current_device_rotation(), 0, 0, 1);
  gltrackball_rotate (bp->trackball);

# ifdef HAVE_MOBILE	/* Keep it the same relative size when rotated. */
  {
    GLfloat h = MI_HEIGHT(mi) / (GLfloat) MI_WIDTH(mi);
    int o = (int) current_device_rotation();
    if (o != 0 && o != 180 && o != -180)
      glScalef (h, h, h);
  }
# endif

  stonerview_win_draw(bp->st);
  if (! bp->button_down_p)
    stonerview_move_increment(bp->st);
  glPopMatrix ();

  mi->polygon_count = NUM_ELS;
  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(MI_DISPLAY (mi), MI_WINDOW(mi));
}

ENTRYPOINT void
free_stonerview (ModeInfo *mi)
{
  stonerview_configuration *bp = &bps[MI_SCREEN(mi)];
  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  if (bp->trackball) gltrackball_free (bp->trackball);
  if (bp->st)
    stonerview_win_release (bp->st);
}

ENTRYPOINT Bool
stonerview_handle_event (ModeInfo *mi, XEvent *event)
{
  stonerview_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;

  return False;
}

XSCREENSAVER_MODULE ("StonerView", stonerview)

#endif /* USE_GL */
