/* tube, Copyright (c) 2001 Jamie Zawinski <jwz@jwz.org>
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

#include "config.h"
#include <stdlib.h>
#include <stdio.h>

#include "screenhack.h"
#include "xlockmoreI.h"

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>

#undef DEBUG  /* Defining this causes check_gl_error() to be called inside
                 time-critical sections, which could slow things down (since
                 it might result in a round-trip, and stall of the pipeline.)
               */

extern void clear_gl_error (void);
extern void check_gl_error (const char *type);

static int fps_text_x = 10;
static int fps_text_y = 10;
static int fps_ascent, fps_descent;
static int fps_sample_frames = 10;
static GLuint font_dlist;
static Bool fps_clear_p = False;

static void
fps_init (ModeInfo *mi)
{
  const char *font = get_string_resource ("fpsFont", "Font");
  XFontStruct *f;
  Font id;
  int first, last;

  fps_clear_p = get_boolean_resource ("fpsSolid", "FPSSolid");

  if (!font) font = "-*-courier-bold-r-normal-*-180-*";
  f = XLoadQueryFont(mi->dpy, font);
  if (!f) f = XLoadQueryFont(mi->dpy, "fixed");

  id = f->fid;
  first = f->min_char_or_byte2;
  last = f->max_char_or_byte2;
  
  clear_gl_error ();
  font_dlist = glGenLists ((GLuint) last+1);
  check_gl_error ("glGenLists");

  fps_ascent = f->ascent;
  fps_descent = f->descent;

  if (get_boolean_resource ("fpsTop", "FPSTop"))
    fps_text_y = - (f->ascent + 10);

  glXUseXFont(id, first, last-first+1, font_dlist + first);
  check_gl_error ("glXUseXFont");
}


static void
fps_print_string (ModeInfo *mi, GLfloat x, GLfloat y, const char *string)
{
  /* save the current state */
  /* note: could be expensive! */

  if (y < 0)
    y = mi->xgwa.height + y;

  clear_gl_error ();

  /* Sadly, this causes a stall of the graphics pipeline (as would the
     equivalent calls to glGet*.)  But there's no way around this, short
     of having each caller set up the specific display matrix we need
     here, which would kind of defeat the purpose of centralizing this
     code in one file.
   */
  glPushAttrib(GL_TRANSFORM_BIT |  /* for matrix contents */
               GL_ENABLE_BIT |     /* for various glDisable calls */
               GL_CURRENT_BIT |    /* for glColor3f() */
               GL_LIST_BIT);       /* for glListBase() */
  {
# ifdef DEBUG
    check_gl_error ("glPushAttrib");
# endif

    /* disable lighting and texturing when drawing bitmaps!
       (glPopAttrib() restores these, I believe.)
     */
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

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
        if (fps_clear_p)
          {
            glColor3f (0, 0, 0);
            glRecti (x, y - fps_descent,
                     mi->xgwa.width - x,
                     y + fps_ascent + fps_descent);
          }

        /* draw the text */
        glColor3f (1, 1, 1);
        glRasterPos2f (x, y);
        glListBase (font_dlist);
        glCallLists (strlen(string), GL_UNSIGNED_BYTE, string);

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


void
do_fps (ModeInfo *mi)
{
  /* every N frames, get the time and use it to get the frames per second */
  static int frame_counter = -1;
  static double oldtime = 0; /* time in usecs, as a double */
  static double newtime = 0;

  static char msg [1024] = { 0, };

  if (frame_counter == -1)
    {
      fps_init (mi);
      frame_counter = fps_sample_frames;
    }

  if (frame_counter++ == fps_sample_frames)
    {
      double fps;
      struct timeval now;
# ifdef GETTIMEOFDAY_TWO_ARGS
      struct timezone tzp;
      gettimeofday(&now, &tzp);
# else
      gettimeofday(&now);
# endif

      oldtime = newtime;
      newtime = now.tv_sec + ((double) now.tv_usec * 0.000001);

      fps = fps_sample_frames / (newtime - oldtime);

      if (fps < 0.0001)
        {
          strcpy(msg, "FPS: (accumulating...)");
        }
      else
        {
          sprintf(msg, "FPS: %.02f", fps);

          if (mi->pause != 0)
            {
              char buf[40];
              sprintf(buf, "%f", mi->pause / 1000000.0); /* FTSO C */
              while(*buf && buf[strlen(buf)-1] == '0')
                buf[strlen(buf)-1] = 0;
              if (buf[strlen(buf)-1] == '.')
                buf[strlen(buf)-1] = 0;
              sprintf(msg + strlen(msg), " (including %s sec/frame delay)",
                      buf);
            }
        }

      frame_counter = 0;
    }

  fps_print_string (mi, fps_text_x, fps_text_y, msg);
}
