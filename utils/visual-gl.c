/* xscreensaver, Copyright (c) 1999, 2000 by Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This file contains code for picking the best visual for GL programs by
   actually asking the GL library to figure it out for us.  The code in
   visual.c might do a good job of this on most systems, but not on most
   high end 3D cards (e.g., Silicon Graphics or nVidia.)

   There exists information about visuals which is available to GL, but
   which is not available via Xlib calls.  So the only way to know
   which visual to use (other than impirically) is to actually call
   glXChooseVisual().
 */

#include "utils.h"
#include "visual.h"

#ifdef HAVE_GL
# include <GL/gl.h>
# include <GL/glx.h>
#endif /* HAVE_GL */

extern char *progname;

Visual *
get_gl_visual (Screen *screen)
{
#ifdef HAVE_GL
  Display *dpy = DisplayOfScreen (screen);
  int screen_num = screen_number (screen);

# define R GLX_RED_SIZE
# define G GLX_GREEN_SIZE
# define B GLX_BLUE_SIZE
# define D GLX_DEPTH_SIZE
# define I GLX_BUFFER_SIZE
# define DB GLX_DOUBLEBUFFER

  int attrs[][20] = {
    { GLX_RGBA, R, 8, G, 8, B, 8, D, 8, DB, 0 }, /* rgb double */
    { GLX_RGBA, R, 4, G, 4, B, 4, D, 4, DB, 0 },
    { GLX_RGBA, R, 2, G, 2, B, 2, D, 2, DB, 0 },
    { GLX_RGBA, R, 8, G, 8, B, 8, D, 8,     0 }, /* rgb single */
    { GLX_RGBA, R, 4, G, 4, B, 4, D, 4,     0 },
    { GLX_RGBA, R, 2, G, 2, B, 2, D, 2,     0 },
    { I, 8,                       D, 8, DB, 0 }, /* cmap double */
    { I, 4,                       D, 4, DB, 0 },
    { I, 8,                       D, 8,     0 }, /* cmap single */
    { I, 4,                       D, 4,     0 },
    { GLX_RGBA, R, 1, G, 1, B, 1, D, 1,     0 }  /* monochrome */
  };

  int i;
  for (i = 0; i < sizeof(attrs)/sizeof(*attrs); i++)
    {
      XVisualInfo *vi = glXChooseVisual (dpy, screen_num, attrs[i]);
      if (vi)
        {
          Visual *v = vi->visual;
          XFree (vi);
          /* describe_gl_visual (stderr, screen, v, False); */
          return v;
        }
    }
#endif /* !HAVE_GL */

  return 0;
}


void
describe_gl_visual (FILE *f, Screen *screen, Visual *visual,
                    Bool private_cmap_p)
{
  describe_visual (f, screen, visual, private_cmap_p);

#ifdef HAVE_GL
  {
    int status;
    int value = False;

    Display *dpy = DisplayOfScreen (screen);
    XVisualInfo vi_in, *vi_out;
    int out_count;

    vi_in.screen = screen_number (screen);
    vi_in.visualid = XVisualIDFromVisual (visual);
    vi_out = XGetVisualInfo (dpy, (VisualScreenMask | VisualIDMask),
                             &vi_in, &out_count);
    if (! vi_out) abort ();

    status = glXGetConfig (dpy, vi_out, GLX_USE_GL, &value);

    if (status == GLX_NO_EXTENSION)
      /* dpy does not support the GLX extension. */
      return;

    if (status == GLX_BAD_VISUAL || value == False)
      /* this visual does not support GLX. */
      return;
    
    if (!glXGetConfig (dpy, vi_out, GLX_LEVEL, &value) &&
        value != 0)
      printf ("    GLX level:         %d\n", value);

    if (!glXGetConfig (dpy, vi_out, GLX_RGBA, &value) && value)
      {
        int r=0, g=0, b=0, a=0;
        glXGetConfig (dpy, vi_out, GLX_RED_SIZE,   &r);
        glXGetConfig (dpy, vi_out, GLX_GREEN_SIZE, &g);
        glXGetConfig (dpy, vi_out, GLX_BLUE_SIZE,  &b);
        glXGetConfig (dpy, vi_out, GLX_ALPHA_SIZE, &a);
        printf ("    GLX type:          RGBA (%2d, %2d, %2d, %2d)\n",
                r, g, b, a);

        r=0, g=0, b=0, a=0;
        glXGetConfig (dpy, vi_out, GLX_ACCUM_RED_SIZE,   &r);
        glXGetConfig (dpy, vi_out, GLX_ACCUM_GREEN_SIZE, &g);
        glXGetConfig (dpy, vi_out, GLX_ACCUM_BLUE_SIZE,  &b);
        glXGetConfig (dpy, vi_out, GLX_ACCUM_ALPHA_SIZE, &a);
        printf ("    GLX accum:         RGBA (%2d, %2d, %2d, %2d)\n",
                r, g, b, a);
      }
    else
      {
        value = 0;
        glXGetConfig (dpy, vi_out, GLX_BUFFER_SIZE, &value);
        printf ("    GLX type:          indexed (%d)\n", value);
      }

# ifdef GLX_VISUAL_CAVEAT_EXT
    if (!glXGetConfig (dpy, vi_out, GLX_VISUAL_CAVEAT_EXT, &value) &&
        value != GLX_NONE_EXT)
#   ifdef GLX_NON_CONFORMANT_EXT
      printf ("    GLX rating:        %s\n",
              (value == GLX_NONE_EXT ? "none" :
               value == GLX_SLOW_VISUAL_EXT ? "slow" :
               value == GLX_NON_CONFORMANT_EXT ? "non-conformant" :
               "???"));
#   else      
      printf ("    GLX rating:        %s\n",
              (value == GLX_NONE_EXT ? "none" :
               value == GLX_SLOW_VISUAL_EXT ? "slow" :
               "???"));
#   endif /* GLX_NON_CONFORMANT_EXT */
# endif /* GLX_VISUAL_CAVEAT_EXT */

    if (!glXGetConfig (dpy, vi_out, GLX_DOUBLEBUFFER, &value))
      printf ("    GLX double-buffer: %s\n", (value ? "yes" : "no"));

    if (!glXGetConfig (dpy, vi_out, GLX_STEREO, &value) &&
        value)
      printf ("    GLX stereo:        %s\n", (value ? "yes" : "no"));

    if (!glXGetConfig (dpy, vi_out, GLX_AUX_BUFFERS, &value) &&
        value != 0)
      printf ("    GLX aux buffers:   %d\n", value);

    if (!glXGetConfig (dpy, vi_out, GLX_DEPTH_SIZE, &value))
      printf ("    GLX depth size:    %d\n", value);

    if (!glXGetConfig (dpy, vi_out, GLX_STENCIL_SIZE, &value) &&
        value != 0)
      printf ("    GLX stencil size:  %d\n", value);

# ifdef GLX_SAMPLE_BUFFERS_SGIS
    if (!glXGetConfig (dpy, vi_out, GLX_SAMPLE_BUFFERS_SGIS, &value) &&
        value != 0)
      {
        int bufs = value;
        if (!glXGetConfig (dpy, vi_out, GLX_SAMPLES_SGIS, &value))
          printf ("    GLX multisamplers: %d (%d)\n", bufs, value);
      }
# endif /* GLX_SAMPLE_BUFFERS_SGIS */

    if (!glXGetConfig (dpy, vi_out, GLX_TRANSPARENT_TYPE_EXT, &value) &&
        value != GLX_NONE_EXT)
      {
        if (value == GLX_NONE_EXT)
          printf ("    GLX transparency:  none\n");
        else if (value == GLX_TRANSPARENT_INDEX_EXT)
          {
            if (!glXGetConfig (dpy, vi_out, GLX_TRANSPARENT_INDEX_VALUE_EXT,
                               &value))
              printf ("    GLX transparency:  indexed (%d)\n", value);
          }
        else if (value == GLX_TRANSPARENT_RGB_EXT)
          {
            int r=0, g=0, b=0, a=0;
            glXGetConfig (dpy, vi_out, GLX_TRANSPARENT_RED_VALUE_EXT,   &r);
            glXGetConfig (dpy, vi_out, GLX_TRANSPARENT_GREEN_VALUE_EXT, &g);
            glXGetConfig (dpy, vi_out, GLX_TRANSPARENT_BLUE_VALUE_EXT,  &b);
            glXGetConfig (dpy, vi_out, GLX_TRANSPARENT_ALPHA_VALUE_EXT, &a);
            printf ("    GLX transparency:  RGBA (%2d, %2d, %2d, %2d)\n",
                    r, g, b, a);
          }
      }
  }
#endif  /* HAVE_GL */
}


Bool
validate_gl_visual (FILE *out, Screen *screen, const char *window_desc,
                    Visual *visual)
{
#ifdef HAVE_GL
  int status;
  int value = False;

  Display *dpy = DisplayOfScreen (screen);
  XVisualInfo vi_in, *vi_out;
  int out_count;
  unsigned int id;

  vi_in.screen = screen_number (screen);
  vi_in.visualid = XVisualIDFromVisual (visual);
  vi_out = XGetVisualInfo (dpy, (VisualScreenMask | VisualIDMask),
                             &vi_in, &out_count);
  if (! vi_out) abort ();

  status = glXGetConfig (dpy, vi_out, GLX_USE_GL, &value);

  id = (unsigned int) vi_out->visualid;
  XFree ((char *) vi_out);

  if (status == GLX_NO_EXTENSION)
    {
      fprintf (out, "%s: display \"%s\" does not support the GLX extension.\n",
               progname, DisplayString (dpy));
      return False;
    }
  else if (status == GLX_BAD_VISUAL || value == False)
    {
      fprintf (out,
               "%s: %s's visual 0x%x does not support the GLX extension.\n",
               progname, window_desc, id);
      return False;
    }
  else
    {
      return True;
    }

#else  /* !HAVE_GL */

  fprintf (out, "%s: GL support was not compiled in to this program.\n",
           progname);
  return False;

#endif  /* !HAVE_GL */
}
