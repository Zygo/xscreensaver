/* xlock-gl.c --- xscreensaver compatibility layer for xlockmore GL modules.
 * xscreensaver, Copyright (c) 1997-2021 Jamie Zawinski <jwz@jwz.org>
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

#include "texfont.h"

#ifndef isupper
# define isupper(c)  ((c) >= 'A' && (c) <= 'Z')
#endif
#ifndef _tolower
# define _tolower(c)  ((c) - 'A' + 'a')
#endif


# ifndef HAVE_EGL
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
#endif /* !HAVE_EGL */


GLXContext *
init_GL(ModeInfo * mi)
{
  /* The Android version of this function is in android/screenhack-android.c */
  Display *dpy = mi->dpy;
  Window window = mi->window;
  Screen *screen = mi->xgwa.screen;
  Visual *visual = mi->xgwa.visual;
  XVisualInfo vi_in, *vi_out;
  int out_count;

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

  vi_in.screen = screen_number (screen);
  vi_in.visualid = XVisualIDFromVisual (visual);
  vi_out = XGetVisualInfo (dpy, VisualScreenMask|VisualIDMask,
			   &vi_in, &out_count);
  if (! vi_out) abort ();

# ifdef HAVE_EGL
  {
    egl_data *d = (egl_data *) calloc (1, sizeof(*d));

    /* The correct EGL config has been selected by utils/visual-gl.c
       (via hacks/glx/xscreensaver-gl-visual) by calling get_egl_config()
       from get_gl_visual and returning the corresponding X11 Visual.
       That visual is the one that was used to create our window. We will
       pass the corresponding visual ID to get_egl_config() to obtain the
       same configuration here. */
    unsigned int vid = XVisualIDFromVisual (visual);

    const EGLint ctxattr1[] = {
# ifdef HAVE_JWZGLES
      EGL_CONTEXT_MAJOR_VERSION, 1,	/* Request an OpenGL ES 1.1 context. */
      EGL_CONTEXT_MINOR_VERSION, 1,
# else
      EGL_CONTEXT_MAJOR_VERSION, 1,	/* Request an OpenGL 1.3 context. */
      EGL_CONTEXT_MINOR_VERSION, 3,
# endif
      EGL_NONE
    };
    const EGLint *ctxattr = ctxattr1;

# ifdef HAVE_GLES3
    const EGLint ctxattr3[] = {
      EGL_CONTEXT_MAJOR_VERSION, 3,	/* Request an OpenGL ES 3.0 context. */
      EGL_CONTEXT_MINOR_VERSION, 0,
      EGL_NONE
    };

    if (get_boolean_resource (dpy, "prefersGLSL", "PrefersGLSL"))
      ctxattr = ctxattr3;
# endif /* HAVE_GLES3 */

    /* This is re-used, no need to close it. */
    d->egl_display = eglGetPlatformDisplay (EGL_PLATFORM_X11_KHR,
                                            (EGLNativeDisplayType) dpy, NULL);
    if (!d->egl_display)
      {
        fprintf (stderr, "%s: eglGetPlatformDisplay failed\n", progname);
        abort();
      }

    get_egl_config (dpy, d->egl_display, vid, &d->egl_config);
    if (!d->egl_config)
      {
        fprintf (stderr, "%s: no matching EGL config for X11 visual 0x%lx\n",
                 progname, vi_out->visualid);
        abort();
      }

    d->egl_surface = eglCreatePlatformWindowSurface (d->egl_display,
                                                     d->egl_config,
                                                     &window, NULL);
    if (! d->egl_surface)
      {
        fprintf (stderr, "%s: eglCreatePlatformWindowSurface failed:"
                 " window 0x%lx visual 0x%x\n", progname, window, vid);
        abort();
      }

#ifdef HAVE_JWZGLES
    /* This call is not strictly necessary to get an OpenGL ES context
       since the default API is EGL_OPENGL_ES_API, but it  makes our
       intention clear.
     */
    if (!eglBindAPI (EGL_OPENGL_ES_API))
      {
        fprintf (stderr, "%s: eglBindAPI failed\n", progname);
      }
#else /* !HAVE_JWZGLES */
    /* This is necessary to get a OpenGL context instead of an OpenGLES
       context.
     */
    if (!eglBindAPI (EGL_OPENGL_API))
      {
        fprintf (stderr, "%s: eglBindAPI failed\n", progname);
      }
#endif /* !HAVE_JWZGLES */

    d->egl_context = eglCreateContext (d->egl_display, d->egl_config,
                                       EGL_NO_CONTEXT, ctxattr);

# ifdef HAVE_GLES3
    /* If creation of a GLES 3.0 context failed, fall back to GLES 1.x. */
    if (!d->egl_context && ctxattr != ctxattr1)
      {
        /* fprintf (stderr, "%s: eglCreateContext 3.0 failed\n", progname); */
        d->egl_context = eglCreateContext (d->egl_display, d->egl_config,
                                           EGL_NO_CONTEXT, ctxattr1);
      }
# endif /* HAVE_GLES3 */

    if (!d->egl_context)
      {
        fprintf (stderr, "%s: eglCreateContext failed\n", progname);
        abort();
      }

    /* describe_gl_visual (stderr, screen, visual, False); */

    mi->glx_context = d;  /* #### leaked */

    glXMakeCurrent (dpy, window, mi->glx_context);
  }
# else /* GLX */
  {
    XSync (dpy, False);
    orig_ehandler = XSetErrorHandler (BadValue_ehandler);
    mi->glx_context = glXCreateContext (dpy, vi_out, 0, GL_TRUE);
    XSync (dpy, False);
    XSetErrorHandler (orig_ehandler);
    if (got_error)
      mi->glx_context = 0;
  }

  if (!mi->glx_context)
    {
      fprintf(stderr, "%s: couldn't create GL context for visual 0x%x.\n",
	      progname, (unsigned int) XVisualIDFromVisual (visual));
      exit(1);
    }

  glXMakeCurrent (dpy, window, mi->glx_context);

  {
    GLboolean rgba_mode = 0;
    glGetBooleanv(GL_RGBA_MODE, &rgba_mode);
    if (!rgba_mode)
      {
	glIndexi (WhitePixelOfScreen (screen));
	glClearIndex (BlackPixelOfScreen (screen));
      }
  }
# endif /* GLX */

  XFree((char *) vi_out);


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
    if (s) free (s);
    glClearColor (c.red   / 65535.0,
                  c.green / 65535.0,
                  c.blue  / 65535.0,
                  1.0);
  }

  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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
