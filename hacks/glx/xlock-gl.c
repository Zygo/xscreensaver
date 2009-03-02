/* xlock-gl.c --- xscreensaver compatibility layer for xlockmore GL modules.
 * xscreensaver, Copyright (c) 1997, 1998, 1999 Jamie Zawinski <jwz@jwz.org>
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

#include <stdio.h>
#include "screenhack.h"
#include "xlockmoreI.h"

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>

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


GLXContext *
init_GL(ModeInfo * mi)
{
  Display *dpy = mi->dpy;
  Window window = mi->window;
  Screen *screen = mi->xgwa.screen;
  Visual *visual = mi->xgwa.visual;
  GLXContext glx_context = 0;
  XVisualInfo vi_in, *vi_out;
  int out_count;

  vi_in.screen = screen_number (screen);
  vi_in.visualid = XVisualIDFromVisual (visual);
  vi_out = XGetVisualInfo (dpy, VisualScreenMask|VisualIDMask,
			   &vi_in, &out_count);
  if (! vi_out) abort ();

  {
    XSync (dpy, False);
    orig_ehandler = XSetErrorHandler (BadValue_ehandler);
    glx_context = glXCreateContext (dpy, vi_out, 0, GL_TRUE);
    XSync (dpy, False);
    XSetErrorHandler (orig_ehandler);
    if (got_error)
      glx_context = 0;
  }

  XFree((char *) vi_out);

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

  /* GLXContext is already a pointer type.
     Why this function returns a pointer to a pointer, I have no idea...
   */
  {
    GLXContext *ptr = (GLXContext *) malloc(sizeof(GLXContext));
    *ptr = glx_context;
    return ptr;
  }
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




/* Frames-per-second statistics */

static int fps_text_x = 10;
static int fps_text_y = 10;
static int fps_sample_frames = 10;
static GLuint font_dlist;

static void
fps_init (ModeInfo *mi)
{
  const char *font = get_string_resource ("fpsFont", "Font");
  XFontStruct *f;
  Font id;
  int first, last;

  if (!font) font = "-*-courier-bold-r-normal-*-180-*";
  f = XLoadQueryFont(mi->dpy, font);
  if (!f) f = XLoadQueryFont(mi->dpy, "fixed");

  id = f->fid;
  first = f->min_char_or_byte2;
  last = f->max_char_or_byte2;
  
  clear_gl_error ();
  font_dlist = glGenLists ((GLuint) last+1);
  check_gl_error ("glGenLists");

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
