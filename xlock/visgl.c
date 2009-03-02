#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)visgl.c	5.01 01/02/19 xlockmore";

#endif

/*-
 * visgl.c - separated out of vis.c for lots of good reasons !
 *         - parts grabbed from xscreensaver
 *
 * Copyright (c) 2001 by Eric Lassauge
 *
 * Revision History:
 *
 * E.Lassauge - 09-Mar-2001:
 *	- Use showfps vs erroneous fpsTop
 *      - use MI_DELAY instead of MI_PAUSE for accumulating FPS value
 */


#include "xlock.h"
#include "visgl.h"

#ifdef USE_GL
#include <GL/glu.h>

static GLXContext *glXContext = (GLXContext *) NULL;
Bool *glOK = (Bool *) NULL;

/* fps stuff */
static XFontStruct *font_struct = (XFontStruct *) NULL;
static GLuint   font_dlist;
extern Bool     fpsTop;
extern char 	*fpsfontname;

/*-
 * NOTE WELL:  We _MUST_ destroy the glXContext between each mode
 * in random mode, otherwise OpenGL settings and paramaters from one
 * mode will affect the default initial state for the next mode.
 * BUT, we are going to keep the visual returned by glXChooseVisual,
 * because it will still be good (and because Mesa must keep track
 * of each one, even after XFree(), causing a small memory leak).
 */

XVisualInfo *
getGLVisual(Display * display, int screen, XVisualInfo * wantVis, int monochrome)
{
	if (wantVis) {
		/* Use glXGetConfig() to see if wantVis has what we need already. */
		int         depthBits, doubleBuffer;

		/* glXGetConfig(display, wantVis, GLX_RGBA, &rgbaMode); */
		glXGetConfig(display, wantVis, GLX_DEPTH_SIZE, &depthBits);
		glXGetConfig(display, wantVis, GLX_DOUBLEBUFFER, &doubleBuffer);

		if ((depthBits > 0) && doubleBuffer) {
			return wantVis;
		} else {
			XFree((char *) wantVis);	/* Free it up since its useless now. */
			wantVis = (XVisualInfo *) NULL;
		}
	}
	/* If wantVis is useless, try glXChooseVisual() */
	if (monochrome) {
		/* Monochrome display - use color index mode */
		int         attribList[] =
		{GLX_DOUBLEBUFFER, None};

		return glXChooseVisual(display, screen, attribList);
	} else {
		int         attribList[] =
#if 1
		{GLX_RGBA, GLX_RED_SIZE, 1, GLX_GREEN_SIZE, 1, GLX_BLUE_SIZE, 1,
		 GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 1, None};

#else
		{GLX_RGBA, GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 1, None};

#endif
		return glXChooseVisual(display, screen, attribList);
	}
}

/*-
 * The following function should be called on startup of any GL mode.
 * It returns a GLXContext for the calling mode to use with
 * glXMakeCurrent().
 */
GLXContext *
init_GL(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         screen = MI_SCREEN(mi);
	XVisualInfo xvi_in, *xvi_out;
	int         num_vis;
	GLboolean   rgbaMode;

#ifdef FX
	extern void resetSize(ModeInfo * mi, Bool setGL);

#endif

	if (!glXContext) {
		glXContext = (GLXContext *) calloc(MI_NUM_SCREENS(mi),
						   sizeof (GLXContext));
		if (!glXContext)
			return (GLXContext *) NULL;
	}
	if (glXContext[screen]) {
		glXDestroyContext(display, glXContext[screen]);
		glXContext[screen] = (GLXContext) NULL;
	}
	xvi_in.screen = screen;
	xvi_in.visualid = XVisualIDFromVisual(MI_VISUAL(mi));
	xvi_out = XGetVisualInfo(display, VisualScreenMask | VisualIDMask,
				 &xvi_in, &num_vis);
	if (!xvi_out) {
		(void) fprintf(stderr, "Could not get XVisualInfo\n");
		return (GLXContext *) NULL;
	}
/*-
 * PURIFY 4.0.1 on SunOS4 and on Solaris 2 reports a 104 byte memory leak on
 * the next line each time that a GL mode is run in random mode when using
 * MesaGL 2.2.  This cumulative leak can cause xlock to eventually crash if
 * available memory is depleted.  This bug is fixed in MesaGL 2.3. */
	if (glOK && glOK[screen])
		glXContext[screen] = glXCreateContext(display, xvi_out, 0, GL_TRUE);

	XFree((char *) xvi_out);

	if (!glXContext[screen]) {
		if (MI_IS_VERBOSE(mi))
			(void) fprintf(stderr,
				       "GL could not create rendering context on screen %d.\n", screen);
		return (GLXContext *) NULL;
	}
/*-
 * PURIFY 4.0.1 on Solaris2 reports an uninitialized memory read on the next
 * line. PURIFY 4.0.1 on SunOS4 does not report this error. */
	if (!glXMakeCurrent(display, window, glXContext[screen])) {
		if (MI_IS_DEBUG(mi)) {
			(void) fprintf(stderr, "GLX error\n");
			(void) fprintf(stderr, "If using MesaGL, XGetWindowAttributes is\n");
			(void) fprintf(stderr, "probably returning a null colormap in\n");
			(void) fprintf(stderr, "XMesaCreateWindowBuffer in xmesa1.c .\n");
		}
		return (GLXContext *) NULL;
	}
	/* True Color junk */
	glGetBooleanv(GL_RGBA_MODE, &rgbaMode);
	if (!rgbaMode) {
		glIndexi(MI_WHITE_PIXEL(mi));
		glClearIndex((float) MI_BLACK_PIXEL(mi));
	}
#ifdef FX
	resetSize(mi, True);
#endif
	return (&(glXContext[screen]));
}

void
FreeAllGL(ModeInfo * mi)
{
	int         scr;

#ifdef FX
	extern void resetSize(ModeInfo * mi, Bool setGL);

#endif

	if (glXContext) {
		/* To release the current context */
		glXMakeCurrent(MI_DISPLAY(mi), None, (GLXContext) NULL);
		/* then destroy all contexts */
		for (scr = 0; scr < MI_NUM_SCREENS(mi); scr++) {
			if (glXContext[scr]) {
				glXDestroyContext(MI_DISPLAY(mi), glXContext[scr]);
				glXContext[scr] = (GLXContext) NULL;
			}
		}
		free(glXContext);
		glXContext = (GLXContext *) NULL;
	}
#ifdef FX
	resetSize(mi, False);
#endif

#if 0
	if (glOK) {
		free(glOK);
		glOK = NULL;
	}
#endif
	if (MI_IS_FPS(mi))
	{
		/* Free font stuff */
		if (font_struct)
		{
  			int last;

  			last = font_struct->max_char_or_byte2;
  			clear_gl_error ();
			if (glIsList(font_dlist)) {
	  			glDeleteLists (font_dlist,(GLuint) last+1);
  				(void) check_gl_error ("glDeleteLists");
				font_dlist = 0;
			}
			XFreeFont(MI_DISPLAY(mi),font_struct);
			/* font_struct should be freed now */
			font_struct = (XFontStruct *) NULL;
		}
		/* else oops ? */
	}
}

/* clear away any lingering error codes */
void
clear_gl_error (void)
{
  while (glGetError() != GL_NO_ERROR);
}

/* report a GL error. */
Bool
check_gl_error (const char *type)
{
  char buf[100];
  GLenum i;
  const char *e;
  switch ((i = glGetError())) {
  case GL_NO_ERROR: return False;
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
    e = buf; (void) sprintf (buf, "unknown error %d", (int) i); break;
  }
  (void) fprintf (stderr, "XLock: %s error: %s\n", type, e);
  return True;
}


/* Frames-per-second statistics */

static int fps_text_x = 10;
static int fps_text_y = 10;
static int fps_sample_frames = 10;

static Bool
fps_init (ModeInfo *mi)
{
  const char *font = fpsfontname;
  XFontStruct *f;
  Font id;
  int first, last;

  if (!font) font = "-*-courier-bold-r-normal-*-180-*";
  f = XLoadQueryFont(MI_DISPLAY(mi), font);
  if (!f) f = XLoadQueryFont(MI_DISPLAY(mi), "fixed");

  font_struct = f; /* save font struct info */
  id = f->fid;
  first = f->min_char_or_byte2;
  last = f->max_char_or_byte2;

  clear_gl_error ();
  font_dlist = glGenLists ((GLuint) last+1);
  if (check_gl_error ("glGenLists"))
 	return False;

  if (fpsTop) /* Draw string on top of screen */
    fps_text_y = - (f->ascent + 10);

  glXUseXFont(id, first, last-first+1, font_dlist + first);
  return (!check_gl_error ("glXUseXFont"));
}

static Bool
fps_print_string (ModeInfo *mi, GLfloat x, GLfloat y, const char *string)
{
  static GLfloat red = 0.0, green = 0.0, blue = 0.0;
  int i;
  /* save the current state */
  /* note: could be expensive! */

  while (red == 0.0 && green == 0.0 && blue == 0.0) {
     red = NRAND(2) ? 1.0 : 0.0;
     green = NRAND(2) ? 1.0 : 0.0;
     blue = NRAND(2) ? 1.0 : 0.0;
  }
  if (y < 0)
    y = MI_HEIGHT(mi) + y;

  clear_gl_error ();
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  {
    if (check_gl_error ("glPushAttrib"))
	return False;
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
      if (check_gl_error ("glPushMatrix"))
	return False;
      glLoadIdentity();

      /* Each matrix mode has its own stack, so we need to push/pop
         them separately. */
      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      {
        if (check_gl_error ("glPushMatrix"))
	  return False;
        glLoadIdentity();

        gluOrtho2D(0.0, (GLfloat) MI_WIDTH(mi), 0.0, (GLfloat) MI_HEIGHT(mi));
        if (check_gl_error ("gluOrtho2D"))
	  return False;
        /* draw the text */
        glColor3f (red, green, blue);
        glRasterPos2f (x, y);
        for (i = 0; i < (int) strlen(string); i++)
          glCallList (font_dlist + (int)string[i]);

        if (check_gl_error ("fps_print_string"))
	  return False;
      }
      glPopMatrix();
    }
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

  }
  /* clean up after our state changes */
  glPopAttrib();
  return (!check_gl_error ("glPopAttrib"));
}


void
do_fps (ModeInfo *mi)
{
  /* every N frames, get the time and use it to get the frames per second */
  static int frame_counter = -1;
  static double oldtime = 0; /* time in usecs, as a double */
  static double newtime = 0;

  static char msg [1024] = { 0, };

  if (MI_IS_ICONIC(mi))
  {
	/* don't do it in iconic state */
	return;
  }
  if (frame_counter == -1)
    {
      if (!fps_init (mi))
	return;
      frame_counter = fps_sample_frames;
    }

  if (frame_counter++ == fps_sample_frames)
    {
      double fps;
      struct timeval now;
# ifdef GETTIMEOFDAY_TWO_ARGS
#ifdef VMS
	(void) gettimeofday(&now, NULL);
#else
	struct timezone tzp;
	(void) gettimeofday(&now, &tzp);
#endif
# else
      (void) gettimeofday(&now);
# endif

      oldtime = newtime;
      newtime = now.tv_sec + ((double) now.tv_usec * 0.000001);

      fps = fps_sample_frames / (newtime - oldtime);

      if (fps < 0.0001)
        {
          (void) strcpy(msg, "FPS: (accumulating...)");
        }
      else
        {
          (void) sprintf(msg, "FPS: %.02f", fps);

          if (MI_DELAY(mi) != 0 && !fpsTop)
            { /* Include delay information in FPS display only if not on top */
              char buf[40];
              (void) sprintf(buf, "%f", MI_DELAY(mi) / 1000000.0); /* FTSO C */
              while(*buf && buf[strlen(buf)-1] == '0')
                buf[strlen(buf)-1] = 0;
              if (buf[strlen(buf)-1] == '.')
                buf[strlen(buf)-1] = 0;
              (void) sprintf(msg + strlen(msg), " (including %s sec/frame delay)",
                      buf);
            }
        }

      frame_counter = 0;
    }
    (void) fps_print_string (mi, (float) fps_text_x, (float) -fps_text_y, msg);
}
#endif
