/* xlock-gl.c --- xscreensaver compatibility layer for xlockmore GL modules.
 * xscreensaver, Copyright © 1997-2025 Jamie Zawinski <jwz@jwz.org>
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

#include "xlockmoreI.h"

#ifdef HAVE_GL  /* whole file */

#include "visual-gl.h"
#include "texfont.h"

#ifndef isupper
# define isupper(c)  ((c) >= 'A' && (c) <= 'Z')
#endif
#ifndef _tolower
# define _tolower(c)  ((c) - 'A' + 'a')
#endif


#ifndef HAVE_JWZGLES
# undef glEnable
void (* glEnable_fn) (GLuint) = glEnable;

# if defined(__linux__) && (defined(__arm__) || defined(__ARM_ARCH))
#  define PI_LIKE  /* Raspberry Pi-adjacent */
static void
glEnable_bad_line_smooth (GLuint cap)
{
  if (cap != GL_LINE_SMOOTH)
    glEnable (cap);
}
# endif /* PI_LIKE */
#endif /* HAVE_JWZGLES */


GLXContext *
init_GL(ModeInfo * mi)
{
  /* The Android version of this function is in android/screenhack-android.c */
  Display *dpy = mi->dpy;
  Window window = mi->window;
  Screen *screen = mi->xgwa.screen;

  if (mi->glx_context) {
    glXMakeCurrent (dpy, window, mi->glx_context);
    return &mi->glx_context;
  }

# ifdef HAVE_JWZGLES
  mi->jwzgles_state = jwzgles_make_state();
  mi->xlmft->jwzgles_make_current = jwzgles_make_current;
  mi->xlmft->jwzgles_free = jwzgles_free_state;
  mi->xlmft->jwzgles_make_current (mi->jwzgles_state);
# endif

  mi->glx_context = openGL_context_for_window (screen, window);

  /* Process the -background argument. */
  {
    char *s = get_string_resource(mi->dpy, "background", "Background");
    XColor c = { 0, };
    if (! XParseColor (dpy, mi->xgwa.colormap, s, &c))
      fprintf (stderr, "%s: can't parse color %s; using black.\n", 
               progname, s);
    if (s) free (s);
    glClearColor (c.red   / 65535.0,
                  c.green / 65535.0,
                  c.blue  / 65535.0,
                  1.0);
  }

  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   /* Sep 2022, Sep 2023: The Raspberry Pi 4b Broadcom driver doesn't do
      GL_LINE_SMOOTH properly: lines show up as barely-visible static noise.
      If we're on ARM, check the GL vendor and maybe disable GL_LINE_SMOOTH.
    */
# ifdef PI_LIKE
  {
    const char *s = (const char *) glGetString (GL_VENDOR);
    if (s && !strcmp (s, "Broadcom"))
      glEnable_fn = glEnable_bad_line_smooth;
  }
# endif

  /* GLXContext is already a pointer type.
     Why this function returns a pointer to a pointer, I have no idea...
   */
  return &mi->glx_context;
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


#ifdef HAVE_EGL

static egl_data *global_egl_kludge = 0;

Bool
glXMakeCurrent (Display *dpy, GLXDrawable window, egl_data *d)
{
  if (!d) abort();
  if (! eglMakeCurrent (d->egl_display, d->egl_surface, d->egl_surface,
                        d->egl_context))
    abort();

  global_egl_kludge = d;

  return True;
}

void
glXSwapBuffers (Display *dpy, GLXDrawable win)
{
  egl_data *d = global_egl_kludge;
  if (!d) abort();
  if (! eglSwapBuffers (d->egl_display, d->egl_surface))
    abort();
}

#endif /* HAVE_EGL */


#endif /* HAVE_GL -- whole file */
