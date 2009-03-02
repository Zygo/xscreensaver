/* xlock-gl.c --- xscreensaver compatibility layer for xlockmore GL modules.
 * xscreensaver, Copyright (c) 1997, 1998, 1999 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * This file, along with xlockmore.h, make it possible to compile an xlockmore
 * GL module into a standalone program, and thus use it with xscreensaver.
 * By Jamie Zawinski <jwz@jwz.org> on 31-May-97.
 */

#include <stdio.h>
#include "screenhack.h"
#include "xlockmoreI.h"

#include <GL/gl.h>
#include <GL/glx.h>

/* Gag -- we use this to turn X errors from glXCreateContext() into
   something that will actually make sense to the user.
 */
static XErrorHandler orig_ehandler = 0;
static Bool got_error = 0;

static int
BadValue_ehandler (Display *dpy, XErrorEvent *error)
{
  if (error->error_code == BadValue)
    {
      got_error = True;
      return 0;
    }
  else
    return orig_ehandler (dpy, error);
}


GLXContext *
init_GL(ModeInfo * mi)
{
  Display *dpy = mi->dpy;
  Window window = mi->window;
  Screen *screen = mi->xgwa.screen;
  Visual *visual = mi->xgwa.visual;
  GLXContext glx_context = 0;
  XVisualInfo vi_in, *vi_out;
  int out_count;

  vi_in.screen = screen_number (screen);
  vi_in.visualid = XVisualIDFromVisual (visual);
  vi_out = XGetVisualInfo (dpy, VisualScreenMask|VisualIDMask,
			   &vi_in, &out_count);
  if (! vi_out) abort ();

  {
    XSync (dpy, False);
    orig_ehandler = XSetErrorHandler (BadValue_ehandler);
    glx_context = glXCreateContext (dpy, vi_out, 0, GL_TRUE);
    XSync (dpy, False);
    XSetErrorHandler (orig_ehandler);
    if (got_error)
      glx_context = 0;
  }

  XFree((char *) vi_out);

  if (!glx_context)
    {
      fprintf(stderr, "%s: couldn't create GL context for visual 0x%x.\n",
	      progname, (unsigned int) XVisualIDFromVisual (visual));
      exit(1);
    }

  glXMakeCurrent (dpy, window, glx_context);

  {
    GLboolean rgba_mode = 0;
    glGetBooleanv(GL_RGBA_MODE, &rgba_mode);
    if (!rgba_mode)
      {
	glIndexi (WhitePixelOfScreen (screen));
	glClearIndex (BlackPixelOfScreen (screen));
      }
  }

  /* GLXContext is already a pointer type.
     Why this function returns a pointer to a pointer, I have no idea...
   */
  {
    GLXContext *ptr = (GLXContext *) malloc(sizeof(GLXContext));
    *ptr = glx_context;
    return ptr;
  }
}
