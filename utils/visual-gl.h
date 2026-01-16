/* xscreensaver, Copyright © 2025 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __VISUAL_GL_H__
#define __VISUAL_GL_H__

extern Visual *get_gl_visual (Screen *);
extern void describe_gl_visual (FILE *, Screen *, Visual *, Bool priv_cmap_p);
extern Bool validate_gl_visual (FILE *, Screen *, const char *, Visual *);

#ifdef HAVE_EGL

# ifdef __egl_h_  /* EGL/egl.h included */

extern void get_egl_config (Display *, EGLDisplay *, EGLint vid, EGLConfig *);

typedef struct egl_data {
  EGLDisplay egl_display;
  EGLSurface egl_surface;
  EGLContext egl_context;
  EGLConfig  egl_config;
} egl_data;

egl_data   *openGL_context_for_window (Screen *, Window);
void openGL_destroy_context (Display *, egl_data *);

# endif /* __egl_h_ */

#else /* !HAVE_EGL -- GLX */

GLXContext  openGL_context_for_window (Screen *, Window);
void openGL_destroy_context (Display *, GLXContext);

#endif /* !HAVE_EGL */

#endif /* __VISUAL_GL_H__ */
