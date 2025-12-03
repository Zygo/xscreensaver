/* xscreensaver, Copyright © 1999-2025 Jamie Zawinski <jwz@jwz.org>
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
#include "resources.h"

#ifdef HAVE_GL
# include <GL/gl.h>
# ifdef HAVE_EGL
#  include <EGL/egl.h>
#  include <EGL/eglext.h>
# else
#  include <GL/glx.h>
# endif
#endif /* HAVE_GL */

#include "visual.h"
#include "visual-gl.h"		/* after EGL/egl.h */

#undef countof
#define countof(x) (sizeof(x)/sizeof(*(x)))


extern char *progname;


Visual *
get_gl_visual (Screen *screen)
{
#ifdef HAVE_GL

  Display *dpy = DisplayOfScreen (screen);

# ifdef HAVE_EGL

  Visual *v;
  EGLint vid;
  EGLDisplay *egl_display;
  EGLConfig egl_config = 0;
  int egl_major = -1, egl_minor = -1;

  /* If you pass in a non-NULL EGLAttrib list to eglGetPlatformDisplay --
     even one with no elements, that is, the single terminator { EGL_NONE } --
     you get the error, "no matching EGL config for X11 visual 0x21".
     So it is impossible for us to inform EGL which X11 screen number
     is in use, for any weirdos still using Zaphod multi-head.
   */
# if 1
  EGLAttrib *av = NULL;
# else
  EGLAttrib av[10];
  int ac = 0;
#  ifdef EGL_PLATFORM_X11_SCREEN_KHR
  av[ac++] = EGL_PLATFORM_X11_SCREEN_KHR;
  av[ac++] = (EGLAttrib) screen_number (screen);
#  endif
  av[ac] = EGL_NONE;
#endif

  /* This is re-used, no need to close it. */
  egl_display = eglGetPlatformDisplay (EGL_PLATFORM_X11_KHR, (void *) dpy, av);

  if (!egl_display)
    {
      fprintf (stderr, "%s: eglGetPlatformDisplay failed\n", progname);
      return 0;
    }

  if (! eglInitialize (egl_display, &egl_major, &egl_minor))
    {
      /* The library already printed this, but without progname:
         "libEGL warning: DRI2: failed to create any config" */
      /* fprintf (stderr, "%s: eglInitialize failed\n", progname); */
      return 0;  /* We get here if running Xephyr in 8-bit pseudocolor */
    }

  /* Get the best EGL config on any visual, then see what visual it used. */
  get_egl_config (dpy, egl_display, -1, &egl_config);
  if (!egl_config) abort();

  if (!eglGetConfigAttrib (egl_display, egl_config,
                           EGL_NATIVE_VISUAL_ID, &vid))
    {
      fprintf (stderr, "%s: EGL: no native visual ID\n", progname);
      abort();
    }
        
  v = id_to_visual (screen, vid);
  if (!v)
    {
      fprintf (stderr, "%s: EGL: no X11 visual 0x%x\n", progname, vid);
      abort();
    }

    return v;

# else  /* !HAVE_EGL -- GLX */

  int screen_num = screen_number (screen);

# define R GLX_RED_SIZE
# define G GLX_GREEN_SIZE
# define B GLX_BLUE_SIZE
# define A GLX_ALPHA_SIZE
# define D GLX_DEPTH_SIZE
# define I GLX_BUFFER_SIZE
# define DB GLX_DOUBLEBUFFER
# define ST GLX_STENCIL_SIZE

# if defined(GLX_SAMPLE_BUFFERS) /* Needs to come before GL_SAMPLE_BUFFERS */
#  define SB GLX_SAMPLE_BUFFERS
#  define SM GLX_SAMPLES
# elif defined(GLX_SAMPLE_BUFFERS_ARB)
#  define SB GLX_SAMPLE_BUFFERS_ARB
#  define SM GLX_SAMPLES_ARB
# elif defined(GLX_SAMPLE_BUFFERS_SGIS)
#  define SB GLX_SAMPLE_BUFFERS_SGIS
#  define SM GLX_SAMPLES_SGIS
# elif defined(GL_SAMPLE_BUFFERS)
#  define SB GL_SAMPLE_BUFFERS
#  define SM GL_SAMPLES
# endif

  int attrs[][40] = {
# ifdef SB				  /* rgba double stencil multisample */
   { GLX_RGBA, R,8, G,8, B,8, A,8, D,8, DB, ST,1, SB,1, SM,8, 0 },
   { GLX_RGBA, R,8, G,8, B,8, A,8, D,8, DB, ST,1, SB,1, SM,6, 0 },
   { GLX_RGBA, R,8, G,8, B,8, A,8, D,8, DB, ST,1, SB,1, SM,4, 0 },
   { GLX_RGBA, R,8, G,8, B,8, A,8, D,8, DB, ST,1, SB,1, SM,2, 0 },
#  define SB_COUNT 4 /* #### Kludgey count of preceeding lines! */
# endif
   { GLX_RGBA, R,8, G,8, B,8, A,8, D,8, DB, ST,1, 0 }, /* rgba double stencil */
   { GLX_RGBA, R,8, G,8, B,8,      D,8, DB, ST,1, 0 }, /* rgb  double stencil */
   { GLX_RGBA, R,4, G,4, B,4,      D,4, DB, ST,1, 0 },
   { GLX_RGBA, R,2, G,2, B,2,      D,2, DB, ST,1, 0 },
   { GLX_RGBA, R,8, G,8, B,8, A,8, D,8, DB,       0 }, /* rgba double */
   { GLX_RGBA, R,8, G,8, B,8,      D,8, DB,       0 }, /* rgb  double */
   { GLX_RGBA, R,4, G,4, B,4,      D,4, DB,       0 },
   { GLX_RGBA, R,2, G,2, B,2,      D,2, DB,       0 },
   { GLX_RGBA, R,8, G,8, B,8, A,8, D,8,           0 }, /* rgba single */
   { GLX_RGBA, R,8, G,8, B,8,      D,8,           0 }, /* rgb  single */
   { GLX_RGBA, R,4, G,4, B,4,      D,4,           0 },
   { GLX_RGBA, R,2, G,2, B,2,      D,2,           0 },
   { I, 8,                         D,8, DB,       0 }, /* cmap double */
   { I, 4,                         D,4, DB,       0 },
   { I, 8,                         D,8,           0 }, /* cmap single */
   { I, 4,                         D,4,           0 },
   { GLX_RGBA, R,1, G,1, B,1,      D,1,           0 }  /* monochrome */
  };

  int i = 0;

# ifdef SB
  if (! get_boolean_resource (dpy, "multiSample", "MultiSample"))
    i = SB_COUNT;  /* skip over the multibuffer entries in 'attrs' */
# endif /* SB */

  for (; i < sizeof(attrs)/sizeof(*attrs); i++)
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

# endif  /* !HAVE_EGL -- GLX */
#endif /* !HAVE_GL */

  return 0;
}


#ifdef HAVE_EGL
/* An X11 "visual" is an object that encapsulates the available display
   formats: color mapping, depth and so on.  So is a GLE "config".
   So why isn't there a one-to-one mapping between visuals and configs?
   Once again, I can only assume the answer is that the people responsible
   for every post-2002 version of the OpenGL specification are incompetent.

   Under GLX, there will be multiple X11 visuals that appear (and behave)
   identically to X11 programs, code but that have different GL parameters.
   Two RGB visuals might have different alpha, depth, or stencil sizes,
   for example, that would matter to a GL program but not to an X11 program.

   But EGL has dozens of 'configs' for each visual, so we can't just pass
   around a Window and a Visual to describe the display parameters: we need
   to pass around the 'config' as well.  Or, do what we do here, have both
   sides use the same code to convert a visual to the best 'config'.
   (It goes down a list of parameter settings, from higher quality to lower,
   and takes the first config that matches.)

   SGI solved this problem in like 1988, and like so many things, GLES went
   and fucked it all up again.
 */
void
get_egl_config (Display *dpy, EGLDisplay *egl_display,
                EGLint x11_visual_id, EGLConfig *egl_config)
{
# define COMMON \
    EGL_SURFACE_TYPE,      EGL_WINDOW_BIT, \
    EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER
# define R EGL_RED_SIZE
# define G EGL_GREEN_SIZE
# define B EGL_BLUE_SIZE
# define A EGL_ALPHA_SIZE
# define D EGL_DEPTH_SIZE
# define I EGL_BUFFER_SIZE
# define ST EGL_STENCIL_SIZE
# define SB EGL_SAMPLE_BUFFERS
# define SM EGL_SAMPLES

  const EGLint templates[][40] = {
# ifdef SB
   { COMMON, R,8, G,8, B,8, A,8, D,8, ST,1, SB,1, SM,8, EGL_NONE },
   { COMMON, R,8, G,8, B,8, A,8, D,8, ST,1, SB,1, SM,6, EGL_NONE },
   { COMMON, R,8, G,8, B,8, A,8, D,8, ST,1, SB,1, SM,4, EGL_NONE },
   { COMMON, R,8, G,8, B,8, A,8, D,8, ST,1, SB,1, SM,2, EGL_NONE },
#  define SB_COUNT 4 /* #### Kludgey count of preceeding lines! */
# endif
   { COMMON, R,8, G,8, B,8, A,8, D,8, ST,1, EGL_NONE }, /* rgba stencil */
   { COMMON, R,8, G,8, B,8,      D,8, ST,1, EGL_NONE }, /* rgb  stencil */
   { COMMON, R,4, G,4, B,4,      D,4, ST,1, EGL_NONE },
   { COMMON, R,2, G,2, B,2,      D,2, ST,1, EGL_NONE },
   { COMMON, R,8, G,8, B,8, A,8, D,8,       EGL_NONE }, /* rgba */
   { COMMON, R,8, G,8, B,8,      D,8,       EGL_NONE }, /* rgb  */
   { COMMON, R,4, G,4, B,4,      D,4,       EGL_NONE },
   { COMMON, R,2, G,2, B,2,      D,2,       EGL_NONE },
   { COMMON, R,1, G,1, B,1,      D,1,       EGL_NONE }  /* monochrome */
  };
  EGLint attrs[40];
  EGLint nconfig;
  int i, i_start, j, k, iter, pass;
  Bool glslp;

  i_start = 0;
# ifdef SB
  if (! get_boolean_resource (dpy, "multiSample", "MultiSample"))
    i_start = SB_COUNT;  /* skip over the multibuffer entries in 'attrs' */
# endif /* SB */

  glslp = get_boolean_resource (dpy, "prefersGLSL", "PrefersGLSL");
  iter = (glslp ? 2 : 1);

  *egl_config = 0;
  for (pass = 0; pass < iter; pass++)
    {
      for (i = i_start ; i < countof(templates); i++)
        {
          for (j = 0, k = 0; templates[i][j] != EGL_NONE; j += 2)
            {
              attrs[k++] = templates[i][j];
              attrs[k++] = templates[i][j+1];
            }

          attrs[k++] = EGL_RENDERABLE_TYPE;
# ifdef HAVE_GLES3
          if (glslp && pass == 0)
            attrs[k++] = EGL_OPENGL_ES3_BIT;
          else
            attrs[k++] = EGL_OPENGL_ES_BIT;
# elif defined(HAVE_JWZGLES)
          attrs[k++] = EGL_OPENGL_ES_BIT;
# else
          attrs[k++] = EGL_OPENGL_BIT;
# endif

          if (x11_visual_id != -1)
            {
              attrs[k++] = EGL_NATIVE_VISUAL_ID;
              attrs[k++] = x11_visual_id;
            }

          attrs[k++] = EGL_NONE;

          nconfig = -1;
          if (eglChooseConfig (egl_display, attrs, egl_config, 1, &nconfig)
              && nconfig == 1)
            break;
        }
      if (i < countof(templates))
        break;
    }

  if (! *egl_config)
    {
      fprintf (stderr, "%s: no matching EGL config for X11 visual 0x%lx\n",
               progname, (unsigned long) x11_visual_id);
      return;
    }
}
#endif /* HAVE_EGL */


void
describe_gl_visual (FILE *f, Screen *screen, Visual *visual,
                    Bool private_cmap_p)
{
  describe_visual (f, screen, visual, private_cmap_p);

#ifdef HAVE_GL
  {
# ifdef HAVE_EGL
    EGLDisplay *egl_display;
    EGLConfig config = 0;
# else  /* !HAVE_EGL -- GLX */
    int status;
    int value = False;
# endif /* !HAVE_EGL -- GLX */

    Display *dpy = DisplayOfScreen (screen);
    XVisualInfo vi_in, *vi_out;
    int out_count;

    vi_in.screen = screen_number (screen);
    vi_in.visualid = XVisualIDFromVisual (visual);
    vi_out = XGetVisualInfo (dpy, (VisualScreenMask | VisualIDMask),
                             &vi_in, &out_count);
    if (! vi_out) abort ();

# ifdef HAVE_EGL

    /* This is re-used, no need to close it. */
    egl_display = eglGetPlatformDisplay (EGL_PLATFORM_X11_KHR,
                                         (void *) dpy, NULL);
    if (!egl_display)
      {
        fprintf (stderr, "%s: eglGetPlatformDisplay failed\n", progname);
        return;
      }

    get_egl_config (dpy, egl_display, vi_out->visualid, &config);
    if (!config)
      {
        fprintf (stderr, "%s: no matching EGL config for X11 visual 0x%lx\n",
                 progname, vi_out->visualid);
        abort();
      }

    {
      int i;
      const struct { int hexp; EGLint i; const char *s; } fields[] = {
        { 1, EGL_CONFIG_ID,		"config ID:"	 },
        { 1, EGL_CONFIG_CAVEAT,		"caveat:"	 },
        { 1, EGL_CONFORMANT,		"conformant:"	 },
        { 0, EGL_COLOR_BUFFER_TYPE,	"buffer type:"	 },
        { 0, EGL_RED_SIZE,		"color size:"	 },
        { 0, EGL_TRANSPARENT_RED_VALUE,	"transparency:"	 },
        { 0, EGL_BUFFER_SIZE,		"buffer size:"	 },
        { 0, EGL_DEPTH_SIZE,		"depth size:"	 },
        { 0, EGL_LUMINANCE_SIZE,	"lum size:"	 },
        { 0, EGL_STENCIL_SIZE,		"stencil size:"	 },
        { 0, EGL_ALPHA_MASK_SIZE,	"alpha mask:"	 },
        { 0, EGL_LEVEL,			"level:"	 },
        { 0, EGL_SAMPLES,		"samples:"	 },
        { 0, EGL_SAMPLE_BUFFERS,	"sample bufs:"	 },
        { 0, EGL_NATIVE_RENDERABLE,	"native render:" },
        { 1, EGL_NATIVE_VISUAL_TYPE,	"native type:"	 },
        { 1, EGL_RENDERABLE_TYPE,	"render type:"	 },
        { 0, EGL_SURFACE_TYPE,		"surface type:"	 },
        { 0, EGL_BIND_TO_TEXTURE_RGB,	"bind RGB:"	 },
        { 0, EGL_BIND_TO_TEXTURE_RGBA,	"bind RGBA:"	 },
        { 0, EGL_MAX_PBUFFER_WIDTH,	"buffer width:"	 },
        { 0, EGL_MAX_PBUFFER_HEIGHT,	"buffer height:" },
        { 0, EGL_MAX_PBUFFER_PIXELS,	"buffer pixels:" },
        { 0, EGL_MAX_SWAP_INTERVAL,	"max swap:"	 },
        { 0, EGL_MIN_SWAP_INTERVAL,	"min swap:"	 },
        };
      EGLint r=0, g=0, b=0, a=0, tt=0, tr=0, tg=0, tb=0;
      eglGetConfigAttrib (egl_display, config, EGL_RED_SIZE,   &r);
      eglGetConfigAttrib (egl_display, config, EGL_GREEN_SIZE, &g);
      eglGetConfigAttrib (egl_display, config, EGL_BLUE_SIZE,  &b);
      eglGetConfigAttrib (egl_display, config, EGL_ALPHA_SIZE, &a);
      eglGetConfigAttrib (egl_display, config, EGL_TRANSPARENT_TYPE, &tt);
      eglGetConfigAttrib (egl_display, config, EGL_TRANSPARENT_RED_VALUE,  &tr);
      eglGetConfigAttrib (egl_display, config, EGL_TRANSPARENT_GREEN_VALUE,&tg);
      eglGetConfigAttrib (egl_display, config, EGL_TRANSPARENT_BLUE_VALUE, &tb);
      for (i = 0; i < countof(fields); i++)
        {
          EGLint v = 0;
          char s[100];
          eglGetConfigAttrib (egl_display, config, fields[i].i, &v);
          if (fields[i].i == EGL_RED_SIZE)
            sprintf (s, "%d, %d, %d, %d", r, g, b, a);
          else if (fields[i].i == EGL_TRANSPARENT_RED_VALUE && tt != EGL_NONE)
            sprintf (s, "%d, %d, %d", tr, tg, tb);
          else if (fields[i].i == EGL_CONFIG_CAVEAT)
            {
              const char *s2 = (v == EGL_NONE ? "none" :
                                v == EGL_SLOW_CONFIG ? "slow" :
# ifdef EGL_NON_CONFORMANT
                                v == EGL_NON_CONFORMANT ? "non-conformant" :
# endif
                                "???");
              strcpy (s, s2);
            }
          else if (fields[i].i == EGL_COLOR_BUFFER_TYPE)
            strcpy (s, (v == EGL_RGB_BUFFER ? "RGB" :
                        v == EGL_LUMINANCE_BUFFER ? "luminance" :
                        "???"));
          else if (fields[i].i == EGL_CONFORMANT ||
                   fields[i].i == EGL_RENDERABLE_TYPE)
            {
              sprintf (s, "0x%02x", v);
              if (v & EGL_OPENGL_BIT)     strcat (s, " OpenGL");
              if (v & EGL_OPENGL_ES_BIT)  strcat (s, " GLES-1.x");
              if (v & EGL_OPENGL_ES2_BIT) strcat (s, " GLES-2.0");
# ifdef EGL_OPENGL_ES3_BIT
              if (v & EGL_OPENGL_ES3_BIT) strcat (s, " GLES-3.0");
# endif
              if (v & EGL_OPENVG_BIT)     strcat (s, " OpenVG");
            }
          else if (fields[i].hexp)
            sprintf (s, "0x%02x", v);
          else if (v)
            sprintf (s, "%d", v);
          else
            *s = 0;

          if (*s) fprintf (f, "    EGL %-14s %s\n", fields[i].s, s);
        }
    }

# else /* !HAVE_EGL -- GLX */

    status = glXGetConfig (dpy, vi_out, GLX_USE_GL, &value);

    if (status == GLX_NO_EXTENSION)
      /* dpy does not support the GLX extension. */
      return;

    if (status == GLX_BAD_VISUAL || value == False)
      /* this visual does not support GLX. */
      return;
    
    if (!glXGetConfig (dpy, vi_out, GLX_LEVEL, &value) &&
        value != 0)
      fprintf (f, "    GLX level:         %d\n", value);

    if (!glXGetConfig (dpy, vi_out, GLX_RGBA, &value) && value)
      {
        int r=0, g=0, b=0, a=0;
        glXGetConfig (dpy, vi_out, GLX_RED_SIZE,   &r);
        glXGetConfig (dpy, vi_out, GLX_GREEN_SIZE, &g);
        glXGetConfig (dpy, vi_out, GLX_BLUE_SIZE,  &b);
        glXGetConfig (dpy, vi_out, GLX_ALPHA_SIZE, &a);
        fprintf (f, "    GLX type:          RGBA (%2d, %2d, %2d, %2d)\n",
                 r, g, b, a);

        r=0, g=0, b=0, a=0;
        glXGetConfig (dpy, vi_out, GLX_ACCUM_RED_SIZE,   &r);
        glXGetConfig (dpy, vi_out, GLX_ACCUM_GREEN_SIZE, &g);
        glXGetConfig (dpy, vi_out, GLX_ACCUM_BLUE_SIZE,  &b);
        glXGetConfig (dpy, vi_out, GLX_ACCUM_ALPHA_SIZE, &a);
        fprintf (f, "    GLX accum:         RGBA (%2d, %2d, %2d, %2d)\n",
                 r, g, b, a);
      }
    else
      {
        value = 0;
        glXGetConfig (dpy, vi_out, GLX_BUFFER_SIZE, &value);
        fprintf (f, "    GLX type:          indexed (%d)\n", value);
      }

# ifndef  GLX_NONE_EXT       /* Hooray for gratuitious name changes. */
#  define GLX_NONE_EXT                    GLX_NONE
#  define GLX_TRANSPARENT_TYPE_EXT        GLX_TRANSPARENT_TYPE
#  define GLX_TRANSPARENT_INDEX_EXT       GLX_TRANSPARENT_INDEX
#  define GLX_TRANSPARENT_INDEX_VALUE_EXT GLX_TRANSPARENT_INDEX_VALUE
#  define GLX_TRANSPARENT_RGB_EXT         GLX_TRANSPARENT_RGB
#  define GLX_TRANSPARENT_RED_VALUE_EXT   GLX_TRANSPARENT_RED_VALUE
#  define GLX_TRANSPARENT_GREEN_VALUE_EXT GLX_TRANSPARENT_GREEN_VALUE
#  define GLX_TRANSPARENT_BLUE_VALUE_EXT  GLX_TRANSPARENT_BLUE_VALUE
#  define GLX_TRANSPARENT_ALPHA_VALUE_EXT GLX_TRANSPARENT_ALPHA_VALUE
# endif

# ifdef GLX_VISUAL_CAVEAT_EXT
    if (!glXGetConfig (dpy, vi_out, GLX_VISUAL_CAVEAT_EXT, &value) &&
        value != GLX_NONE_EXT)
#   ifdef GLX_NON_CONFORMANT_EXT
      fprintf (f, "    GLX rating:        %s\n",
               (value == GLX_NONE_EXT ? "none" :
                value == GLX_SLOW_VISUAL_EXT ? "slow" :
                value == GLX_NON_CONFORMANT_EXT ? "non-conformant" :
                "???"));
#   else      
      fprintf (f, "    GLX rating:        %s\n",
               (value == GLX_NONE_EXT ? "none" :
                value == GLX_SLOW_VISUAL_EXT ? "slow" :
                "???"));
#   endif /* GLX_NON_CONFORMANT_EXT */
# endif /* GLX_VISUAL_CAVEAT_EXT */

    if (!glXGetConfig (dpy, vi_out, GLX_DOUBLEBUFFER, &value))
      fprintf (f, "    GLX double-buffer: %s\n", (value ? "yes" : "no"));

    if (!glXGetConfig (dpy, vi_out, GLX_STEREO, &value) &&
        value)
      fprintf (f, "    GLX stereo:        %s\n", (value ? "yes" : "no"));

    if (!glXGetConfig (dpy, vi_out, GLX_AUX_BUFFERS, &value) &&
        value != 0)
      fprintf (f, "    GLX aux buffers:   %d\n", value);

    if (!glXGetConfig (dpy, vi_out, GLX_DEPTH_SIZE, &value))
      fprintf (f, "    GLX depth size:    %d\n", value);

    if (!glXGetConfig (dpy, vi_out, GLX_STENCIL_SIZE, &value) &&
        value != 0)
      fprintf (f, "    GLX stencil size:  %d\n", value);

# ifdef SB  /* GL_SAMPLE_BUFFERS || GLX_SAMPLE_BUFFERS_* */
    if (!glXGetConfig (dpy, vi_out, SB, &value) &&
        value != 0)
      {
        int bufs = value;
        if (!glXGetConfig (dpy, vi_out, SM, &value))
          fprintf (f, "    GLX multisample:   %d, %d\n", bufs, value);
      }
# endif  /* GL_SAMPLE_BUFFERS || GLX_SAMPLE_BUFFERS_* */

    if (!glXGetConfig (dpy, vi_out, GLX_TRANSPARENT_TYPE_EXT, &value) &&
        value != GLX_NONE_EXT)
      {
        if (value == GLX_NONE_EXT)
          fprintf (f, "    GLX transparency:  none\n");
        else if (value == GLX_TRANSPARENT_INDEX_EXT)
          {
            if (!glXGetConfig (dpy, vi_out, GLX_TRANSPARENT_INDEX_VALUE_EXT,
                               &value))
              fprintf (f, "    GLX transparency:  indexed (%d)\n", value);
          }
        else if (value == GLX_TRANSPARENT_RGB_EXT)
          {
            int r=0, g=0, b=0, a=0;
            glXGetConfig (dpy, vi_out, GLX_TRANSPARENT_RED_VALUE_EXT,   &r);
            glXGetConfig (dpy, vi_out, GLX_TRANSPARENT_GREEN_VALUE_EXT, &g);
            glXGetConfig (dpy, vi_out, GLX_TRANSPARENT_BLUE_VALUE_EXT,  &b);
            glXGetConfig (dpy, vi_out, GLX_TRANSPARENT_ALPHA_VALUE_EXT, &a);
            fprintf (f, "    GLX transparency:  RGBA (%2d, %2d, %2d, %2d)\n",
                     r, g, b, a);
          }
      }
# endif /* !HAVE_EGL */
  }
#endif  /* HAVE_GL */
}


Bool
validate_gl_visual (FILE *out, Screen *screen, const char *window_desc,
                    Visual *visual)
{
#ifdef HAVE_GL
# ifndef HAVE_EGL
  int status;
  int value = False;
  unsigned int id;
# endif

  Display *dpy = DisplayOfScreen (screen);
  XVisualInfo vi_in, *vi_out;
  int out_count;

  vi_in.screen = screen_number (screen);
  vi_in.visualid = XVisualIDFromVisual (visual);
  vi_out = XGetVisualInfo (dpy, (VisualScreenMask | VisualIDMask),
                             &vi_in, &out_count);
  if (! vi_out) abort ();

# ifndef HAVE_EGL
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
# endif /* !HAVE_EGL */
    {
      return True;
    }

#else  /* !HAVE_GL */

  fprintf (out, "%s: GL support was not compiled in to this program.\n",
           progname);
  return False;

#endif  /* !HAVE_GL */
}


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


/* Initializes OpenGL and returns a GLXContext or egl_data for the window.
   The window is assumed to have been created with the proper visual.
 */
#ifdef HAVE_EGL
egl_data  *openGL_context_for_window (Screen *screen, Window window)
#else
GLXContext openGL_context_for_window (Screen *screen, Window window)
#endif
{
  Display *dpy = DisplayOfScreen (screen);
  XWindowAttributes xgwa;
  Visual *visual;
  XVisualInfo vi_in, *vi_out;
  int out_count;

# ifdef HAVE_EGL
  egl_data *egl_data_ret = NULL;
# else
  GLXContext glx_context = NULL;
# endif

  XGetWindowAttributes (dpy, window, &xgwa);
  visual = xgwa.visual;

  vi_in.screen = screen_number (screen);
  vi_in.visualid = XVisualIDFromVisual (visual);
  vi_out = XGetVisualInfo (dpy, VisualScreenMask|VisualIDMask,
			   &vi_in, &out_count);
  if (! vi_out) abort ();

# ifdef HAVE_EGL
  {
    egl_data *d = (egl_data *) calloc (1, sizeof(*d));

    /* The correct EGL config has been selected by calling get_egl_config()
       from get_gl_visual and returning the corresponding X11 Visual.
       That visual is the one that was used to create the window. We will
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
                                            (void *) dpy, NULL);
    if (!d->egl_display)
      {
        fprintf (stderr, "%s: eglGetPlatformDisplay failed\n", progname);
        abort();
      }

    get_egl_config (dpy, d->egl_display, vid, &d->egl_config);
    if (!d->egl_config)
      {
        /* get_egl_config already printed this:
        fprintf (stderr, "%s: no matching EGL config for X11 visual 0x%lx\n",
                 progname, vi_out->visualid); */
        /* returning 0 might be reasonable, but makes all the GL hacks
           simply draw nothing in a loop. */
        exit (1);
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

    if (! eglMakeCurrent (d->egl_display, d->egl_surface, d->egl_surface,
                          d->egl_context))
      abort();

    egl_data_ret = d;
  }
# else /* GLX */
  {
    XSync (dpy, False);
    orig_ehandler = XSetErrorHandler (BadValue_ehandler);
    glx_context = glXCreateContext (dpy, vi_out, 0, GL_TRUE);
    XSync (dpy, False);
    XSetErrorHandler (orig_ehandler);
    if (got_error)
      glx_context = 0;
  }

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
  while (glGetError() != GL_NO_ERROR)
    ;

# ifdef HAVE_EGL
  return egl_data_ret;
# else
  return glx_context;
# endif
}

#ifdef HAVE_EGL

void
openGL_destroy_context (Display *dpy, egl_data *data)
{
  eglDestroyContext (data->egl_display, data->egl_context);
  eglDestroySurface (data->egl_display, data->egl_surface);
  eglTerminate (data->egl_display);
  free (data);
}

#else /* HAVE_EGL */

void
openGL_destroy_context (Display *dpy, GLXContext ctx)
{
  glXDestroyContext (dpy, ctx);
}

#endif /* HAVE_EGL */
