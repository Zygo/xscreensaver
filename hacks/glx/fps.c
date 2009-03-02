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
  int i;
  /* save the current state */
  /* note: could be expensive! */

  if (y < 0)
    y = mi->xgwa.height + y;

  clear_gl_error ();
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  {
    check_gl_error ("glPushAttrib");

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
      check_gl_error ("glPushMatrix");
      glLoadIdentity();

      /* Each matrix mode has its own stack, so we need to push/pop
         them separately. */
      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      {
        check_gl_error ("glPushMatrix");
        glLoadIdentity();

        gluOrtho2D(0, mi->xgwa.width, 0, mi->xgwa.height);
        check_gl_error ("gluOrtho2D");

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
        for (i = 0; i < strlen(string); i++)
          glCallList (font_dlist + (int)string[i]);

        check_gl_error ("fps_print_string");
      }
      glPopMatrix();
    }
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

  }
  /* clean up after our state changes */
  glPopAttrib();
  check_gl_error ("glPopAttrib");
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
