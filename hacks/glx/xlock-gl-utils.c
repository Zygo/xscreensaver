/* xlock-gl.c --- xscreensaver compatibility layer for xlockmore GL modules.
 * xscreensaver, Copyright (c) 1997-2015 Jamie Zawinski <jwz@jwz.org>
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
#include "xlockmoreI.h"
#include "texfont.h"

#ifndef isupper
# define isupper(c)  ((c) >= 'A' && (c) <= 'Z')
#endif
#ifndef _tolower
# define _tolower(c)  ((c) - 'A' + 'a')
#endif


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

# ifdef HAVE_JWZGLES
  jwzgles_reset();
# endif

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


  /* jwz: the doc for glDrawBuffer says "The initial value is GL_FRONT
     for single-buffered contexts, and GL_BACK for double-buffered
     contexts."  However, I find that this is not always the case,
     at least with Mesa 3.4.2 -- sometimes the default seems to be
     GL_FRONT even when glGet(GL_DOUBLEBUFFER) is true.  So, let's
     make sure.

     Oh, hmm -- maybe this only happens when we are re-using the
     xscreensaver window, and the previous GL hack happened to die with
     the other buffer selected?  I'm not sure.  Anyway, this fixes it.
   */
  {
    GLboolean d = False;
    glGetBooleanv (GL_DOUBLEBUFFER, &d);
    if (d)
      glDrawBuffer (GL_BACK);
    else
      glDrawBuffer (GL_FRONT);
  }

  /* Sometimes glDrawBuffer() throws "invalid op". Dunno why. Ignore. */
  clear_gl_error ();

  /* Process the -background argument. */
  {
    char *s = get_string_resource(mi->dpy, "background", "Background");
    XColor c = { 0, };
    if (! XParseColor (dpy, mi->xgwa.colormap, s, &c))
      fprintf (stderr, "%s: can't parse color %s; using black.\n", 
               progname, s);
    glClearColor (c.red   / 65535.0,
                  c.green / 65535.0,
                  c.blue  / 65535.0,
                  1.0);
  }

  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  /* GLXContext is already a pointer type.
     Why this function returns a pointer to a pointer, I have no idea...
   */
  {
    GLXContext *ptr = (GLXContext *) malloc(sizeof(GLXContext));
    *ptr = glx_context;
    return ptr;
  }
}



/* clear away any lingering error codes */
void
clear_gl_error (void)
{
  while (glGetError() != GL_NO_ERROR)
    ;
}

/* report a GL error. */
void
check_gl_error (const char *type)
{
  char buf[100];
  GLenum i;
  const char *e;
  switch ((i = glGetError())) {
  case GL_NO_ERROR: return;
  case GL_INVALID_ENUM:          e = "invalid enum";      break;
  case GL_INVALID_VALUE:         e = "invalid value";     break;
  case GL_INVALID_OPERATION:     e = "invalid operation"; break;
  case GL_STACK_OVERFLOW:        e = "stack overflow";    break;
  case GL_STACK_UNDERFLOW:       e = "stack underflow";   break;
  case GL_OUT_OF_MEMORY:         e = "out of memory";     break;
#ifdef GL_INVALID_FRAMEBUFFER_OPERATION
  case GL_INVALID_FRAMEBUFFER_OPERATION:
    e = "invalid framebuffer operation";
    break;
#endif
#ifdef GL_TABLE_TOO_LARGE_EXT
  case GL_TABLE_TOO_LARGE_EXT:   e = "table too large";   break;
#endif
#ifdef GL_TEXTURE_TOO_LARGE_EXT
  case GL_TEXTURE_TOO_LARGE_EXT: e = "texture too large"; break;
#endif
  default:
    e = buf; sprintf (buf, "unknown error %d", (int) i); break;
  }
  fprintf (stderr, "%s: %s error: %s\n", progname, type, e);
  exit (1);
}


/* Callback in xscreensaver_function_table, via xlockmore.c.
 */
Visual *
xlockmore_pick_gl_visual (Screen *screen)
{
  /* pick the "best" visual by interrogating the GL library instead of
     by asking Xlib.  GL knows better.
   */
  Visual *v = 0;
  Display *dpy = DisplayOfScreen (screen);
  char *string = get_string_resource (dpy, "visualID", "VisualID");
  char *s;

  if (string)
    for (s = string; *s; s++)
      if (isupper (*s)) *s = _tolower (*s);

  if (!string || !*string ||
      !strcmp (string, "gl") ||
      !strcmp (string, "best") ||
      !strcmp (string, "color") ||
      !strcmp (string, "default"))
    v = get_gl_visual (screen);		/* from ../utils/visual-gl.c */

  if (string)
    free (string);

  return v;
}


/* Callback in xscreensaver_function_table, via xlockmore.c.
 */
Bool
xlockmore_validate_gl_visual (Screen *screen, const char *name, Visual *visual)
{
  return validate_gl_visual (stderr, screen, name, visual);
}
