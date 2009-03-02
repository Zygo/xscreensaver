/* tube, Copyright (c) 2001, 2006 Jamie Zawinski <jwz@jwz.org>
 * Utility function to draw a frames-per-second display.
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

#include "xlockmoreI.h"

#ifdef HAVE_COCOA
# include <OpenGL/gl.h>
# include <OpenGL/glu.h>
# include <AGL/agl.h>
#else /* !HAVE_COCOA -- real Xlib */
# include <GL/gl.h>
# include <GL/glu.h>
# include <GL/glx.h>
#endif /* !HAVE_COCOA */

#undef DEBUG  /* Defining this causes check_gl_error() to be called inside
                 time-critical sections, which could slow things down (since
                 it might result in a round-trip, and stall of the pipeline.)
               */

extern void clear_gl_error (void);
extern void check_gl_error (const char *type);

extern GLfloat fps_1 (ModeInfo *mi);
extern void    fps_2 (ModeInfo *mi);
extern void    do_fps (ModeInfo *mi);
extern void    fps_free (ModeInfo *mi);

struct fps_state {
  int x, y;
  int ascent, descent;
  GLuint font_dlist;
  Bool clear_p;
  char string[1024];

  int last_ifps;
  GLfloat last_fps;
  int frame_count;
  struct timeval prev, now;
};


static void
fps_init (ModeInfo *mi)
{
  struct fps_state *st = mi->fps_state;

  if (st) free (st);
  st = (struct fps_state *) calloc (1, sizeof(*st));
  mi->fps_state = st;

  st->clear_p = get_boolean_resource (mi->dpy, "fpsSolid", "FPSSolid");

# ifndef HAVE_COCOA /* Xlib version */
  {
    const char *font;
    XFontStruct *f;
    Font id;
    int first, last;

    font = get_string_resource (mi->dpy, "fpsFont", "Font");

    if (!font) font = "-*-courier-bold-r-normal-*-180-*";
    f = XLoadQueryFont (mi->dpy, font);
    if (!f) f = XLoadQueryFont (mi->dpy, "fixed");

    id = f->fid;
    first = f->min_char_or_byte2;
    last = f->max_char_or_byte2;
  
    clear_gl_error ();
    st->font_dlist = glGenLists ((GLuint) last+1);
    check_gl_error ("glGenLists");

    st->ascent = f->ascent;
    st->descent = f->descent;

    glXUseXFont (id, first, last-first+1, st->font_dlist + first);
    check_gl_error ("glXUseXFont");
  }

# else  /* HAVE_COCOA */

  {
    AGLContext ctx = aglGetCurrentContext();
    GLint id    = 0;   /* 0 = system font; 1 = application font */
    Style face  = 1;   /* 0 = plain; 1=B; 2=I; 3=BI; 4=U; 5=UB; etc. */
    GLint size  = 24;
    GLint first = 32;
    GLint last  = 255;

    st->ascent  = size * 0.9;
    st->descent = size - st->ascent;

    clear_gl_error ();
    st->font_dlist = glGenLists ((GLuint) last+1);
    check_gl_error ("glGenLists");

    if (! aglUseFont (ctx, id, face, size, 
                      first, last-first+1, st->font_dlist + first)) {
      check_gl_error ("aglUseFont");
      abort();
    }
  }

# endif  /* HAVE_COCOA */

  st->x = 10;
  st->y = 10;
  if (get_boolean_resource (mi->dpy, "fpsTop", "FPSTop"))
    st->y = - (st->ascent + 10);
}

void
fps_free (ModeInfo *mi)
{
  if (mi->fps_state)
    free (mi->fps_state);
  mi->fps_state = 0;
}

static void
fps_print_string (ModeInfo *mi, GLfloat x, GLfloat y, const char *string)
{
  struct fps_state *st = mi->fps_state;
  const char *L2 = strchr (string, '\n');

  if (y < 0)
    {
      y = mi->xgwa.height + y;
      if (L2)
        y -= (st->ascent + st->descent);
    }

# ifdef DEBUG
  clear_gl_error ();
# endif

  /* Sadly, this causes a stall of the graphics pipeline (as would the
     equivalent calls to glGet*.)  But there's no way around this, short
     of having each caller set up the specific display matrix we need
     here, which would kind of defeat the purpose of centralizing this
     code in one file.
   */
  glPushAttrib (GL_TRANSFORM_BIT |  /* for matrix contents */
                GL_ENABLE_BIT |     /* for various glDisable calls */
                GL_CURRENT_BIT |    /* for glColor3f() */
                GL_LIST_BIT);       /* for glListBase() */
  {
# ifdef DEBUG
    check_gl_error ("glPushAttrib");
# endif

    /* disable lighting and texturing when drawing bitmaps!
       (glPopAttrib() restores these.)
     */
    glDisable (GL_TEXTURE_2D);
    glDisable (GL_LIGHTING);
    glDisable (GL_BLEND);
    glDisable (GL_DEPTH_TEST);
    glDisable (GL_CULL_FACE);

    /* glPopAttrib() does not restore matrix changes, so we must
       push/pop the matrix stacks to be non-intrusive there.
     */
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    {
# ifdef DEBUG
      check_gl_error ("glPushMatrix");
# endif
      glLoadIdentity();

      /* Each matrix mode has its own stack, so we need to push/pop
         them separately. */
      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      {
# ifdef DEBUG
        check_gl_error ("glPushMatrix");
# endif
        glLoadIdentity();

        gluOrtho2D(0, mi->xgwa.width, 0, mi->xgwa.height);
# ifdef DEBUG
        check_gl_error ("gluOrtho2D");
# endif

        /* clear the background */
        if (st->clear_p)
          {
            int lines = L2 ? 2 : 1;
            glColor3f (0, 0, 0);
            glRecti (x / 2, y - st->descent,
                     mi->xgwa.width - x,
                     y + lines * (st->ascent + st->descent));
          }

        /* draw the text */
        glColor3f (1, 1, 1);
        glRasterPos2f (x, y);
        glListBase (st->font_dlist);

        if (L2)
          {
            L2++;
            glCallLists (strlen(L2), GL_UNSIGNED_BYTE, L2);
            glRasterPos2f (x, y + (st->ascent + st->descent));
            glCallLists (L2 - string - 1, GL_UNSIGNED_BYTE, string);
          }
        else
          {
            glCallLists (strlen(string), GL_UNSIGNED_BYTE, string);
          }

# ifdef DEBUG
        check_gl_error ("fps_print_string");
# endif
      }
      glPopMatrix();
    }
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

  }
  /* clean up after our state changes */
  glPopAttrib();
# ifdef DEBUG
  check_gl_error ("glPopAttrib");
# endif
}


GLfloat
fps_1 (ModeInfo *mi)
{
  struct fps_state *st = mi->fps_state;
  if (!st)
    {
      fps_init (mi);
      st = mi->fps_state;
      strcpy (st->string, "FPS: (accumulating...)");
    }

  /* Every N frames (where N is approximately one second's worth of frames)
     check the wall clock.  We do this because checking the wall clock is
     a slow operation.
   */
  if (st->frame_count++ >= st->last_ifps)
    {
# ifdef GETTIMEOFDAY_TWO_ARGS
      struct timezone tzp;
      gettimeofday(&st->now, &tzp);
# else
      gettimeofday(&st->now);
# endif

      if (st->prev.tv_sec == 0)
        st->prev = st->now;
    }

  /* If we've probed the wall-clock time, regenerate the string.
   */
  if (st->now.tv_sec != st->prev.tv_sec)
    {
      double uprev = st->prev.tv_sec + ((double) st->prev.tv_usec * 0.000001);
      double unow  =  st->now.tv_sec + ((double)  st->now.tv_usec * 0.000001);
      double fps   = st->frame_count / (unow - uprev);

      st->prev = st->now;
      st->frame_count = 0;
      st->last_ifps = fps;
      st->last_fps  = fps;

      sprintf (st->string, "FPS: %.02f", fps);

#ifndef HAVE_COCOA
      /* since there's no "-delay 0" in the Cocoa version,
         don't bother mentioning the inter-frame delay. */
      if (mi->pause != 0)
        {
          char buf[40];
          sprintf(buf, "%f", mi->pause / 1000000.0); /* FTSO C */
          while(*buf && buf[strlen(buf)-1] == '0')
            buf[strlen(buf)-1] = 0;
          if (buf[strlen(buf)-1] == '.')
            buf[strlen(buf)-1] = 0;
          sprintf(st->string + strlen(st->string),
                  " (including %s sec/frame delay)",
                  buf);
        }
#endif /* HAVE_COCOA */

      if (mi->polygon_count > 0)
        {
          unsigned long p = mi->polygon_count;
          const char *s = "";
# if 0
          if      (p >= (1024 * 1024)) p >>= 20, s = "M";
          else if (p >= 2048)          p >>= 10, s = "K";
# endif

          strcat (st->string, "\nPolys: ");
          if (p >= 1000000)
            sprintf (st->string + strlen(st->string), "%lu,%03lu,%03lu%s",
                     (p / 1000000), ((p / 1000) % 1000), (p % 1000), s);
          else if (p >= 1000)
            sprintf (st->string + strlen(st->string), "%lu,%03lu%s",
                     (p / 1000), (p % 1000), s);
          else
            sprintf (st->string + strlen(st->string), "%lu%s", p, s);
        }
    }

  return st->last_fps;
}

void
fps_2 (ModeInfo *mi)
{
  struct fps_state *st = mi->fps_state;
  fps_print_string (mi, st->x, st->y, st->string);
}


void
do_fps (ModeInfo *mi)
{
  fps_1 (mi);   /* Lazily compute current FPS value, about once a second. */
  fps_2 (mi);   /* Print the string every frame (else nothing shows up.) */
}
