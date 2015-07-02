/* recanim, Copyright (c) 2014-2015 Jamie Zawinski <jwz@jwz.org>
 * Record animation frames of the running screenhack.
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

#ifdef USE_GL
# ifdef HAVE_COCOA
#  include "jwxyz.h"
# else /* !HAVE_COCOA -- real Xlib */
#  include <GL/glx.h>
#  include <GL/glu.h>
# endif /* !HAVE_COCOA */
# ifdef HAVE_JWZGLES
#  include "jwzgles.h"
# endif /* HAVE_JWZGLES */
#endif /* USE_GL */

#ifdef HAVE_GDK_PIXBUF
# ifdef HAVE_GTK2
#  include <gdk-pixbuf-xlib/gdk-pixbuf-xlib.h>
# else  /* !HAVE_GTK2 */
#  include <gdk-pixbuf/gdk-pixbuf-xlib.h>
# endif /* !HAVE_GTK2 */
#endif /* HAVE_GDK_PIXBUF */

#include <sys/stat.h>
#include <sys/types.h>

#include "screenhackI.h"
#include "recanim.h"

struct record_anim_state {
  Screen *screen;
  Window window;
  int frame_count;
  int target_frames;
  XWindowAttributes xgwa;
  char *title;
  int pct;
  int fade_frames;
# ifdef USE_GL
  char *data, *data2;
# else  /* !USE_GL */
  XImage *img;
  Pixmap p;
  GC gc;
# endif /* !USE_GL */
};

record_anim_state *
screenhack_record_anim_init (Screen *screen, Window window, int target_frames)
{
  Display *dpy = DisplayOfScreen (screen);
  record_anim_state *st;

# ifndef USE_GL
  XGCValues gcv;
# endif /* !USE_GL */

  if (target_frames <= 0) return 0;

  st = (record_anim_state *) calloc (1, sizeof(*st));

  st->screen = screen;
  st->window = window;
  st->target_frames = target_frames;
  st->frame_count = 0;
  st->fade_frames = 30 * 1.5;

  if (st->fade_frames >= (st->target_frames / 2) - 30)
    st->fade_frames = 0;

# ifdef HAVE_GDK_PIXBUF
  {
    Window root;
    int x, y;
    unsigned int w, h, d, bw;
    XGetGeometry (dpy, window, &root, &x, &y, &w, &h, &bw, &d);
    gdk_pixbuf_xlib_init_with_depth (dpy, screen_number (screen), d);

#  ifdef HAVE_GTK2
#   if !GLIB_CHECK_VERSION(2, 36 ,0)
    g_type_init();
#   endif
#  else  /* !HAVE_GTK2 */
    xlib_rgb_init (dpy, screen);
#  endif /* !HAVE_GTK2 */
  }
# else  /* !HAVE_GDK_PIXBUF */
#  error GDK_PIXBUF is required
# endif /* !HAVE_GDK_PIXBUF */

  XGetWindowAttributes (dpy, st->window, &st->xgwa);

# ifdef USE_GL

  st->data  = (char *) calloc (st->xgwa.width, st->xgwa.height * 3);
  st->data2 = (char *) calloc (st->xgwa.width, st->xgwa.height * 3);

# else    /* !USE_GL */

  st->gc = XCreateGC (dpy, st->window, 0, &gcv);
  st->p = XCreatePixmap (dpy, st->window,
                         st->xgwa.width, st->xgwa.height, st->xgwa.depth);
  st->img = XCreateImage (dpy, st->xgwa.visual, st->xgwa.depth, ZPixmap,
                          0, 0, st->xgwa.width, st->xgwa.height, 8, 0);
  st->img->data = (char *) calloc (st->img->height, st->img->bytes_per_line);
# endif /* !USE_GL */


# ifndef HAVE_COCOA
  XFetchName (dpy, st->window, &st->title);
# endif /* !HAVE_COCOA */

  return st;
}


/* Fade to black. Assumes data is 3-byte packed.
 */
static void
fade_frame (record_anim_state *st, unsigned char *data, double ratio)
{
  int x, y, i;
  int w = st->xgwa.width;
  int h = st->xgwa.height;
  unsigned char *s = data;
  for (y = 0; y < h; y++)
    for (x = 0; x < w; x++)
      for (i = 0; i < 3; i++)
        *s++ *= ratio;
}


void
screenhack_record_anim (record_anim_state *st)
{
  int bytes_per_line = st->xgwa.width * 3;

# ifndef USE_GL

  Display *dpy = DisplayOfScreen (st->screen);
  char *data = (char *) st->img->data;

  /* Under XQuartz we can't just do XGetImage on the Window, we have to
     go through an intermediate Pixmap first.  I don't understand why.
     Also, the fucking resize handle shows up as black.  God dammit.
     A workaround for that is to temporarily remove /opt/X11/bin/quartz-wm
   */
  XCopyArea (dpy, st->window, st->p, st->gc, 0, 0,
             st->xgwa.width, st->xgwa.height, 0, 0);
  XGetSubImage (dpy, st->p, 0, 0, st->xgwa.width, st->xgwa.height,
                ~0L, ZPixmap, st->img, 0, 0);

  /* Convert BGRA to RGB */
  {
    const char *in = st->img->data;
    char *out = st->img->data;
    int x, y;
    int w = st->img->width;
    int h = st->img->height;
    for (y = 0; y < h; y++)
      {
        const char *in2 = in;
        for (x = 0; x < w; x++)
          {
            *out++ = in2[2];
            *out++ = in2[1];
            *out++ = in2[0];
            in2 += 4;
          }
        in += st->img->bytes_per_line;
      }
  }
# else  /* USE_GL */

  char *data = st->data2;
  int y;

# ifdef HAVE_JWZGLES
#  undef glReadPixels /* Kludge -- unimplemented in the GLES compat layer */
# endif

  /* First OpenGL frame tends to be random data like a shot of my desktop,
     since it is the front buffer when we were drawing in the back buffer.
     Leave it black. */
  /* glDrawBuffer (GL_BACK); */
  if (st->frame_count != 0)
    glReadPixels (0, 0, st->xgwa.width, st->xgwa.height,
                  GL_RGB, GL_UNSIGNED_BYTE, st->data);

  /* Flip vertically */
  for (y = 0; y < st->xgwa.height; y++)
    memcpy (data      + bytes_per_line * y,
            st->data  + bytes_per_line * (st->xgwa.height - y - 1),
            bytes_per_line);

# endif /* USE_GL */


  if (st->frame_count < st->fade_frames)
    fade_frame (st, (unsigned char *) data,
                (double) st->frame_count / st->fade_frames);
  else if (st->frame_count >= st->target_frames - st->fade_frames)
    fade_frame (st, (unsigned char *) data,
                (double) (st->target_frames - st->frame_count - 1) /
                st->fade_frames);

# ifdef HAVE_GDK_PIXBUF
  {
    const char *type = "png";
    char fn[1024];
    GError *error = 0;
    GdkPixbuf *pixbuf;

    pixbuf = gdk_pixbuf_new_from_data ((guchar *) data,
                                       GDK_COLORSPACE_RGB, False, /* alpha */
                                       8, /* bits per sample */
                                       st->xgwa.width, st->xgwa.height,
                                       bytes_per_line,
                                       0, 0);

    sprintf (fn, "%s-%06d.%s", progname, st->frame_count, type);
    gdk_pixbuf_save (pixbuf, fn, type, &error, NULL);

    if (error)
      {
        fprintf (stderr, "%s: %s: %s\n", progname, fn, error->message);
        exit (1);
      }

    g_object_unref (pixbuf);
  }
# else  /* !HAVE_GDK_PIXBUF */
#  error GDK_PIXBUF is required
# endif /* !HAVE_GDK_PIXBUF */

# ifndef HAVE_COCOA
  {  /* Put percent done in window title */
    int pct = 100 * (st->frame_count + 1) / st->target_frames;
    if (pct != st->pct && st->title)
      {
        Display *dpy = DisplayOfScreen (st->screen);
        char *t2 = (char *) malloc (strlen(st->title) + 20);
        sprintf (t2, "%s: %d%%", st->title, pct);
        XStoreName (dpy, st->window, t2);
        XSync (dpy, 0);
        free (t2);
        st->pct = pct;
      }
  }
# endif /* !HAVE_COCOA */

  if (++st->frame_count >= st->target_frames)
    screenhack_record_anim_free (st);
}


void
screenhack_record_anim_free (record_anim_state *st)
{
# ifndef USE_GL
  Display *dpy = DisplayOfScreen (st->screen);
# endif /* !USE_GL */

  struct stat s;
  int i;
  const char *type = "png";
  char cmd[1024];
  char fn[1024];
  const char *soundtrack = 0;

  fprintf (stderr, "%s: wrote %d frames\n", progname, st->frame_count);

# ifdef USE_GL
  free (st->data);
  free (st->data2);
# else  /* !USE_GL */
  free (st->img->data);
  st->img->data = 0;
  XDestroyImage (st->img);
  XFreeGC (dpy, st->gc);
  XFreePixmap (dpy, st->p);
# endif /* !USE_GL */

  sprintf (fn, "%s.%s", progname, "mp4");
  unlink (fn);

# define ST "images/drives-200.mp3"
  soundtrack = ST;
  if (stat (soundtrack, &s)) soundtrack = 0;
  if (! soundtrack) soundtrack = "../" ST;
  if (stat (soundtrack, &s)) soundtrack = 0;
  if (! soundtrack) soundtrack = "../../" ST;
  if (stat (soundtrack, &s)) soundtrack = 0;

  sprintf (cmd,
           "ffmpeg"
           " -framerate 30"	/* rate of input: must be before -i */
           " -i '%s-%%06d.%s'"
           " -r 30",		/* rate of output: must be after -i */
           progname, type);
  if (soundtrack)
    sprintf (cmd + strlen(cmd),
             " -i '%s' -map 0:v:0 -map 1:a:0 -acodec libfaac",
             soundtrack);
  sprintf (cmd + strlen(cmd),
           " -c:v libx264"
           " -profile:v high"
           " -crf 18"
           " -pix_fmt yuv420p"
           " '%s'"
           " 2>&-",
           fn);
  fprintf (stderr, "%s: exec: %s\n", progname, cmd);
  system (cmd);

  if (stat (fn, &s))
    {
      fprintf (stderr, "%s: %s was not created\n", progname, fn);
      exit (1);
    }

  fprintf (stderr, "%s: wrote %s (%.1f MB)\n", progname, fn,
           s.st_size / (float) (1024 * 1024));

  for (i = 0; i < st->frame_count; i++)
    {
      sprintf (fn, "%s-%06d.%s", progname, i, type);
      unlink (fn);
    }

  if (st->title)
    free (st->title);
  free (st);
  exit (0);
}
