/* fps, Copyright (c) 2001-2014 Jamie Zawinski <jwz@jwz.org>
 * Draw a frames-per-second display (Xlib and OpenGL).
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_COCOA
# include "jwxyz.h"
#elif defined(HAVE_ANDROID)
# include <GLES/gl.h>
#else /* real Xlib */
# include <GL/glx.h>
# include <GL/glu.h>
#endif /* !HAVE_COCOA */

#ifdef HAVE_JWZGLES
# include "jwzgles.h"
#endif /* HAVE_JWZGLES */

#include "xlockmoreI.h"
#include "fpsI.h"
#include "texfont.h"

/* These are in xlock-gl.c */
extern void clear_gl_error (void);
extern void check_gl_error (const char *type);

typedef struct {
  texture_font_data *texfont;
  int line_height;
} gl_fps_data;


static void
xlockmore_gl_fps_init (fps_state *st)
{
  gl_fps_data *data = (gl_fps_data *) calloc (1, sizeof(*data));
  data->texfont = load_texture_font (st->dpy, "fpsFont");
  texture_string_width (data->texfont, "M", &data->line_height);
  st->gl_fps_data = data;
}


/* Callback in xscreensaver_function_table, via xlockmore.c.
 */
void
xlockmore_gl_compute_fps (Display *dpy, Window w, fps_state *fpst, 
                          void *closure)
{
  ModeInfo *mi = (ModeInfo *) closure;
  if (! mi->fpst)
    {
      mi->fpst = fpst;
      xlockmore_gl_fps_init (fpst);
    }

  fps_compute (fpst, mi->polygon_count, mi->recursion_depth);
}


/* Called directly from GL programs (as `do_fps') before swapping buffers.
 */
void
xlockmore_gl_draw_fps (ModeInfo *mi)
{
  fps_state *st = mi->fpst;
  if (st)   /* might be too early */
    {
      gl_fps_data *data = (gl_fps_data *) st->gl_fps_data;
      XWindowAttributes xgwa;
      int lines = 1;
      const char *s;
      int y = st->y;

      XGetWindowAttributes (st->dpy, st->window, &xgwa);
      for (s = st->string; *s; s++) 
        if (*s == '\n') lines++;

      if (y < 0)
        y = xgwa.height + y - (lines * data->line_height);
      y += lines * data->line_height;

      glColor3f (1, 1, 1);
      print_texture_label (st->dpy, data->texfont,
                           xgwa.width, xgwa.height,
                           2, st->string);
    }
}
