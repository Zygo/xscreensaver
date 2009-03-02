/* StonerView: An eccentric visual toy.
   Copyright 1998-2001 by Andrew Plotkin (erkyrath@eblong.com)
   http://www.eblong.com/zarf/stonerview.html
 
   Permission to use, copy, modify, distribute, and sell this software and its
   documentation for any purpose is hereby granted without fee, provided that
   the above copyright notice appear in all copies and that both that
   copyright notice and this permission notice appear in supporting
   documentation.  No representations are made about the suitability of this
   software for any purpose.  It is provided "as is" without express or 
   implied warranty.
*/

/* Ported away from GLUT (so that it can do `-root' and work with xscreensaver)
   by Jamie Zawinski <jwz@jwz.org>, 22-Jan-2001.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>

#include "vroot.h"	/* handle virtual root windows --- very important */

#include "version.h"	/* get the xscreensaver package's version number */
#include "yarandom.h"	/* use xscreensaver's RNG */

#include "stonerview-osc.h"
#include "stonerview-move.h"

static char *progclass = "StonerView";
static char *progname = 0;

#define FRAMERATE (50) /* milliseconds per frame */

static GLfloat view_rotx = -45.0, view_roty = 0.0, view_rotz = 0.0;
static GLfloat view_scale = 4.0;
/* static GLint prevtime = 0; / * for timing */

static void setup_window(void);

void win_draw(void);
static void win_reshape(int width, int height);
/* static void win_visible(int vis); */
/* static void win_idle(void); */
static void handle_events(void);

static Display *dpy;
static Window window;
static int wireframe = 0;

static Atom XA_WM_PROTOCOLS, XA_WM_DELETE_WINDOW;


static void
usage (void)
{
  fprintf (stderr, "usage: %s [--wire] [--geom G | --root | --window-id ID]\n",
           progname);
  exit (1);
}


/* mostly lifted from xscreensaver/utils/visual.c... */
static int
visual_depth (Display *dpy, int screen, Visual *visual)
{
  XVisualInfo vi_in, *vi_out;
  int out_count, d;
  vi_in.screen = screen;
  vi_in.visualid = XVisualIDFromVisual (visual);
  vi_out = XGetVisualInfo (dpy, VisualScreenMask|VisualIDMask,
			   &vi_in, &out_count);
  if (! vi_out) abort ();
  d = vi_out [0].depth;
  XFree ((char *) vi_out);
  return d;
}


/* mostly lifted from xscreensaver/hacks/screenhack.c... */
static char *make_title_string (void)
{
  char version[255];
  char *v = (char *) strdup(strchr(screensaver_id, ' '));
  char *s1, *s2, *s3, *s4;
  s1 = (char *) strchr(v,  ' '); s1++;
  s2 = (char *) strchr(s1, ' ');
  s3 = (char *) strchr(v,  '('); s3++;
  s4 = (char *) strchr(s3, ')');
  *s2 = 0;
  *s4 = 0;
  sprintf (version, "%s: from the XScreenSaver %s distribution (%s.)",
           progclass, s1, s3);
  return (strdup(version));
}


int init_view(int *argc, char *argv[])
{
  int ix;
  int fullscreen = 0;
  int on_root = 0;
  Window on_window = 0;

  int undef = -65536;
  int x = undef, y = undef;
  int w = 500, h = 500;
  char *dpystr = (char *) getenv ("DISPLAY");
  char *geom = 0;
  int screen;
  Visual *visual = 0;
  XWindowAttributes xgwa;
  XSetWindowAttributes xswa;
  unsigned long xswa_mask = 0;
  XSizeHints hints;
  GLXContext glx_context = 0;

  progname = argv[0];

  memset (&hints, 0, sizeof(hints));

  for (ix=1; ix<*argc; ix++)
    {
      if (argv[ix][0] == '-' && argv[ix][1] == '-')
        argv[ix]++;
      if (!strcmp(argv[ix], "-geometry") ||
          !strcmp(argv[ix], "-geom"))
        {
          if (on_root || fullscreen) usage();
          geom = argv[++ix];
        }
      else if (!strcmp(argv[ix], "-display") ||
               !strcmp(argv[ix], "-disp") ||
               !strcmp(argv[ix], "-dpy") ||
               !strcmp(argv[ix], "-d"))
        dpystr = argv[++ix];
      else if (!strcmp(argv[ix], "-root"))
        {
          if (geom || fullscreen) usage();
          on_root = 1;
        }
      else if (!strcmp(argv[ix], "-window-id") &&
               *argc > ix+1)
        {
          unsigned long id;
          char c;
          if (1 != sscanf (argv[ix+1], "%lu %c", &id, &c) &&
              1 != sscanf (argv[ix+1], "0x%lx %c", &id, &c))
            usage();
          ix++;
          on_window = (Window) id;
        }
      else if (!strcmp(argv[ix], "-fullscreen") ||
               !strcmp(argv[ix], "-full"))
        {
          if (on_root || geom) usage();
          fullscreen = 1;
        }
      else if (!strcmp(argv[ix], "-wireframe") ||
               !strcmp(argv[ix], "-wire"))
        {
          wireframe = 1;
        }
      else
        {
          usage();
        }
    }

  dpy = XOpenDisplay (dpystr);
  if (!dpy) exit (1);

  screen = DefaultScreen (dpy);

  XA_WM_PROTOCOLS = XInternAtom (dpy, "WM_PROTOCOLS", False);
  XA_WM_DELETE_WINDOW = XInternAtom (dpy, "WM_DELETE_WINDOW", False);

  if (on_root || on_window)
    {
      window = (on_window ? on_window : RootWindow (dpy, screen));
      XGetWindowAttributes (dpy, window, &xgwa);
      visual = xgwa.visual;
      w = xgwa.width;
      h = xgwa.height;
    }
  else
    {
      int ww = WidthOfScreen (DefaultScreenOfDisplay (dpy));
      int hh = HeightOfScreen (DefaultScreenOfDisplay (dpy));

      if (fullscreen)
        {
          w = ww;
          h = hh;
        }
      else if (geom)
        {
          /* Since we're not linking against Xt or GLUT, we get to parse
             the `-geometry' argument ourselves.  YAY.
           */
          char c;
          if      (4 == sscanf (geom, "=%dx%d+%d+%d%c", &w, &h, &x, &y, &c))
            ;
          else if (4 == sscanf (geom, "=%dx%d-%d+%d%c", &w, &h, &x, &y, &c))
            x = ww-w-x;
          else if (4 == sscanf (geom, "=%dx%d+%d-%d%c", &w, &h, &x, &y, &c))
            y = hh-h-y;
          else if (4 == sscanf (geom, "=%dx%d-%d-%d%c", &w, &h, &x, &y, &c))
            x = ww-w-x, y = hh-h-y;
          else if (2 == sscanf (geom, "=%dx%d%c", &w, &h, &c))
            ;
          else if (2 == sscanf (geom, "+%d+%d%c", &x, &y, &c))
            ;
          else if (2 == sscanf (geom, "-%d+%d%c", &x, &y, &c))
            x = ww-w-x;
          else if (2 == sscanf (geom, "+%d-%d%c", &x, &y, &c))
            y = hh-h-y;
          else if (2 == sscanf (geom, "-%d-%d%c", &x, &y, &c))
            x = ww-w-x, y = hh-h-y;
          else
            {
              fprintf (stderr, "%s: unparsable geometry: %s\n",
                       progname, geom);
              exit (1);
            }

          hints.flags = USSize;
          hints.width = w;
          hints.height = h;
          if (x != undef && y != undef)
            {
              hints.flags |= USPosition;
              hints.x = x;
              hints.y = y;
            }
        }

      /* Pick a good GL visual */
      {
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
            XVisualInfo *vi = glXChooseVisual (dpy, screen, attrs[i]);
            if (vi)
              {
                visual = vi->visual;
                XFree (vi);
                break;
              }
          }
        if (!visual)
          {
            fprintf (stderr, "%s: unable to find a GL visual\n", progname);
            exit (1);
          }
      }


      if (x == undef) x = 0;
      if (y == undef) y = 0;

      xswa_mask = (CWEventMask | CWColormap |
                   CWBackPixel | CWBackingPixel | CWBorderPixel );
      xswa.colormap = XCreateColormap (dpy, RootWindow (dpy, screen),
                                       visual, AllocNone);
      xswa.background_pixel = BlackPixel (dpy, screen);
      xswa.backing_pixel = xswa.background_pixel;
      xswa.border_pixel = xswa.background_pixel;
      xswa.event_mask = (KeyPressMask | ButtonPressMask | StructureNotifyMask);

      window = XCreateWindow (dpy, RootWindow (dpy, screen),
                              x, y, w, h, 0,
                              visual_depth (dpy, screen, visual),
                              InputOutput, visual,
                              xswa_mask, &xswa);

      {
        XWMHints wmhints;

        XTextProperty tp1, tp2;
        char *v = make_title_string ();
        XStringListToTextProperty (&v, 1, &tp1);
        XStringListToTextProperty (&progclass, 1, &tp2);
        wmhints.flags = InputHint;
        wmhints.input = True;
        XSetWMProperties (dpy, window, &tp1, &tp2, argv, *argc, &hints,
                          &wmhints, 0);
        free (v);
      }

      XChangeProperty (dpy, window, XA_WM_PROTOCOLS, XA_ATOM, 32,
                       PropModeReplace,
                       (unsigned char *) &XA_WM_DELETE_WINDOW, 1);

      XMapRaised (dpy, window);
      XSync (dpy, False);
    }


  /* Now hook up to GLX */
  {
    XVisualInfo vi_in, *vi_out;
    int out_count;
    vi_in.screen = screen;
    vi_in.visualid = XVisualIDFromVisual (visual);
      vi_out = XGetVisualInfo (dpy, VisualScreenMask|VisualIDMask,
                               &vi_in, &out_count);
      if (! vi_out) abort ();

      glx_context = glXCreateContext (dpy, vi_out, 0, GL_TRUE);
      XFree((char *) vi_out);

      if (!glx_context)
        {
          fprintf(stderr, "%s: couldn't create GL context for root window.\n",
                  progname);
          exit(1);
        }

      glXMakeCurrent (dpy, window, glx_context);
  }

  setup_window();
  win_reshape(w, h);

  return 1;
}

static void setup_window(void)
{
  glEnable(GL_CULL_FACE);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_DEPTH_TEST);

  glEnable(GL_NORMALIZE);
}

/* callback: draw everything */
void win_draw(void)
{
  int ix;
  static GLfloat white[] = { 1.0, 1.0, 1.0, 1.0 };
  static GLfloat gray[] =  { 0.6, 0.6, 0.6, 1.0 };

  glDrawBuffer(GL_BACK);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix();
  glScalef(view_scale, view_scale, view_scale);
  glRotatef(view_rotx, 1.0, 0.0, 0.0);
  glRotatef(view_roty, 0.0, 1.0, 0.0);
  glRotatef(view_rotz, 0.0, 0.0, 1.0);

  glShadeModel(GL_FLAT);

  for (ix=0; ix<NUM_ELS; ix++) {
    elem_t *el = &elist[ix];

    glNormal3f(0.0, 0.0, 1.0);

    /* outline the square */
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, (wireframe ? white : gray));
    glBegin(GL_LINE_LOOP);
    glVertex3f(el->pos[0]-el->vervec[0], el->pos[1]-el->vervec[1], el->pos[2]);
    glVertex3f(el->pos[0]+el->vervec[1], el->pos[1]-el->vervec[0], el->pos[2]);
    glVertex3f(el->pos[0]+el->vervec[0], el->pos[1]+el->vervec[1], el->pos[2]);
    glVertex3f(el->pos[0]-el->vervec[1], el->pos[1]+el->vervec[0], el->pos[2]);
    glEnd();

    if (wireframe) continue;

    /* fill the square */
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, el->col);
    glBegin(GL_QUADS);
    glVertex3f(el->pos[0]-el->vervec[0], el->pos[1]-el->vervec[1], el->pos[2]);
    glVertex3f(el->pos[0]+el->vervec[1], el->pos[1]-el->vervec[0], el->pos[2]);
    glVertex3f(el->pos[0]+el->vervec[0], el->pos[1]+el->vervec[1], el->pos[2]);
    glVertex3f(el->pos[0]-el->vervec[1], el->pos[1]+el->vervec[0], el->pos[2]);
    glEnd();
  }

  glPopMatrix();

  glFinish();
  glXSwapBuffers(dpy, window);

  handle_events();
}


/* callback: new window size or exposure */
static void win_reshape(int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport(0, 0, (GLint) width, (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-1.0, 1.0, -h, h, 5.0, 60.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.0, 0.0, -40.0);
}


static void
handle_events (void)
{
  while (XPending (dpy))
    {
      XEvent E;
      XEvent *event = &E;
      XNextEvent (dpy, event);
      switch (event->xany.type)
        {
        case ConfigureNotify:
          {
            XWindowAttributes xgwa;
            XGetWindowAttributes (dpy, window, &xgwa);
            win_reshape (xgwa.width, xgwa.height);
            break;
          }
        case KeyPress:
          {
            KeySym keysym;
            char c = 0;
            XLookupString (&event->xkey, &c, 1, &keysym, 0);
            if (c == 'q' ||
                c == 'Q' ||
                c == 3 ||	/* ^C */
                c == 27)	/* ESC */
              exit (0);
            else if (! (keysym >= XK_Shift_L && keysym <= XK_Hyper_R))
              XBell (dpy, 0);  /* beep for non-chord keys */
          }
          break;
        case ButtonPress:
          XBell (dpy, 0);
          break;
        case ClientMessage:
          {
            if (event->xclient.message_type != XA_WM_PROTOCOLS)
              {
                char *s = XGetAtomName(dpy, event->xclient.message_type);
                if (!s) s = "(null)";
                fprintf (stderr, "%s: unknown ClientMessage %s received!\n",
                         progname, s);
              }
            else if (event->xclient.data.l[0] != XA_WM_DELETE_WINDOW)
              {
                char *s1 = XGetAtomName(dpy, event->xclient.message_type);
                char *s2 = XGetAtomName(dpy, event->xclient.data.l[0]);
                if (!s1) s1 = "(null)";
                if (!s2) s2 = "(null)";
                fprintf (stderr,"%s: unknown ClientMessage %s[%s] received!\n",
                         progname, s1, s2);
              }
            else
              {
                exit (0);
              }
          }
          break;
        }
    }
}
