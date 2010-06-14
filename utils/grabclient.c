/* xscreensaver, Copyright (c) 1992-2010 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This file contains code for running an external program to grab an image
   onto the given window.  The external program in question must take a
   window ID as its argument, e.g., the "xscreensaver-getimage" program
   in the hacks/ directory.

   That program links against utils/grabimage.c, which happens to export the
   same API as this program (utils/grabclient.c).
 */

#include "utils.h"
#include "grabscreen.h"
#include "resources.h"

#ifdef HAVE_COCOA
# include "jwxyz.h"
# include "colorbars.h"
#else /* !HAVE_COCOA -- real Xlib */
# include "vroot.h"
# include <X11/Xatom.h>
# include <X11/Intrinsic.h>   /* for XtInputId, etc */
#endif /* !HAVE_COCOA */

#include <sys/stat.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>		/* for waitpid() and associated macros */
#endif


extern char *progname;

static void print_loading_msg (Screen *, Window);

#ifndef HAVE_COCOA

static Bool error_handler_hit_p = False;

static int
ignore_all_errors_ehandler (Display *dpy, XErrorEvent *error)
{
  error_handler_hit_p = True;
  return 0;
}


/* Returns True if the given Drawable is a Window; False if it's a Pixmap.
 */
static Bool
drawable_window_p (Display *dpy, Drawable d)
{
  XErrorHandler old_handler;
  XWindowAttributes xgwa;

  XSync (dpy, False);
  old_handler = XSetErrorHandler (ignore_all_errors_ehandler);
  error_handler_hit_p = False;
  XGetWindowAttributes (dpy, d, &xgwa);
  XSync (dpy, False);
  XSetErrorHandler (old_handler);
  XSync (dpy, False);

  if (!error_handler_hit_p)
    return True;   /* It's a Window. */
  else
    return False;  /* It's a Pixmap, or an invalid ID. */
}


static Bool
xscreensaver_window_p (Display *dpy, Window window)
{
  Atom type;
  int format;
  unsigned long nitems, bytesafter;
  unsigned char *version;
  if (XGetWindowProperty (dpy, window,
			  XInternAtom (dpy, "_SCREENSAVER_VERSION", False),
			  0, 1, False, XA_STRING,
			  &type, &format, &nitems, &bytesafter,
			  &version)
      == Success
      && type != None)
    return True;
  return False;
}


/* XCopyArea seems not to work right on SGI O2s if you draw in SubwindowMode
   on a window whose depth is not the maximal depth of the screen?  Or
   something.  Anyway, things don't work unless we: use SubwindowMode for
   the real root window (or a legitimate virtual root window); but do not
   use SubwindowMode for the xscreensaver window.  I make no attempt to
   explain.
 */
Bool
use_subwindow_mode_p (Screen *screen, Window window)
{
  if (window != VirtualRootWindowOfScreen(screen))
    return False;
  else if (xscreensaver_window_p(DisplayOfScreen(screen), window))
    return False;
  else
    return True;
}


static void
checkerboard (Screen *screen, Drawable drawable)
{
  Display *dpy = DisplayOfScreen (screen);
  unsigned int x, y;
  int size = 24;
  XColor fg, bg;
  XGCValues gcv;
  GC gc = XCreateGC (dpy, drawable, 0, &gcv);
  Colormap cmap;
  unsigned int win_width, win_height;

  fg.flags = bg.flags = DoRed|DoGreen|DoBlue;
  fg.red = fg.green = fg.blue = 0x0000;
  bg.red = bg.green = bg.blue = 0x4444;
  fg.pixel = 0;
  bg.pixel = 1;

  if (drawable_window_p (dpy, drawable))
    {
      XWindowAttributes xgwa;
      XGetWindowAttributes (dpy, drawable, &xgwa);
      win_width = xgwa.width;
      win_height = xgwa.height;
      cmap = xgwa.colormap;
      screen = xgwa.screen;
    }
  else  /* it's a pixmap */
    {
      XWindowAttributes xgwa;
      Window root;
      int x, y;
      unsigned int bw, d;
      XGetWindowAttributes (dpy, RootWindowOfScreen (screen), &xgwa);
      cmap = xgwa.colormap;
      XGetGeometry (dpy, drawable,
                    &root, &x, &y, &win_width, &win_height, &bw, &d);
    }

  /* Allocate black and gray, but don't hold them locked. */
  if (XAllocColor (dpy, cmap, &fg))
    XFreeColors (dpy, cmap, &fg.pixel, 1, 0);
  if (XAllocColor (dpy, cmap, &bg))
    XFreeColors (dpy, cmap, &bg.pixel, 1, 0);

  XSetForeground (dpy, gc, bg.pixel);
  XFillRectangle (dpy, drawable, gc, 0, 0, win_width, win_height);
  XSetForeground (dpy, gc, fg.pixel);
  for (y = 0; y < win_height; y += size+size)
    for (x = 0; x < win_width; x += size+size)
      {
        XFillRectangle (dpy, drawable, gc, x,      y,      size, size);
        XFillRectangle (dpy, drawable, gc, x+size, y+size, size, size);
      }
  XFreeGC (dpy, gc);
}


static char *
get_name (Display *dpy, Window window)
{
  Atom type;
  int format;
  unsigned long nitems, bytesafter;
  unsigned char *name = 0;
  Atom atom = XInternAtom (dpy, XA_XSCREENSAVER_IMAGE_FILENAME, False);
  if (XGetWindowProperty (dpy, window, atom,
                          0, 1024, False, XA_STRING,
                          &type, &format, &nitems, &bytesafter,
                          &name)
      == Success
      && type != None)
    return (char *) name;
  else
    return 0;
}


static Bool
get_geometry (Display *dpy, Window window, XRectangle *ret)
{
  Atom type;
  int format;
  unsigned long nitems, bytesafter;
  unsigned char *name = 0;
  Atom atom = XInternAtom (dpy, XA_XSCREENSAVER_IMAGE_GEOMETRY, False);
  int x, y;
  unsigned int w, h;
  if (XGetWindowProperty (dpy, window, atom,
                          0, 1024, False, XA_STRING,
                          &type, &format, &nitems, &bytesafter,
                          &name)
      == Success
      && type != None)
    {
      int flags = XParseGeometry ((char *) name, &x, &y, &w, &h);
      free (name);
      /* Require all four, and don't allow negative positions. */
      if (flags == (XValue|YValue|WidthValue|HeightValue))
        {
          ret->x = x;
          ret->y = y;
          ret->width  = w;
          ret->height = h;
          return True;
        }
      else
        return False;
    }
  else
    return False;
}


static void
hack_subproc_environment (Display *dpy)
{
  /* Store $DISPLAY into the environment, so that the $DISPLAY variable that
     the spawned processes inherit is what we are actually using.
   */
  const char *odpy = DisplayString (dpy);
  char *ndpy = (char *) malloc(strlen(odpy) + 20);
  strcpy (ndpy, "DISPLAY=");
  strcat (ndpy, odpy);

  /* Allegedly, BSD 4.3 didn't have putenv(), but nobody runs such systems
     any more, right?  It's not Posix, but everyone seems to have it. */
#ifdef HAVE_PUTENV
  if (putenv (ndpy))
    abort ();
#endif /* HAVE_PUTENV */

  /* don't free (ndpy) -- some implementations of putenv (BSD 4.4,
     glibc 2.0) copy the argument, but some (libc4,5, glibc 2.1.2, MacOS)
     do not.  So we must leak it (and/or the previous setting). Yay.
   */
}


/* Spawn a program, and wait for it to finish.
   If we just use system() for this, then sometimes the subprocess
   doesn't die when *this* process is sent a TERM signal.  Perhaps
   this is due to the intermediate /bin/sh that system() uses to
   parse arguments?  I'm not sure.  But using fork() and execvp()
   here seems to close the race.
 */
static void
exec_simple_command (const char *command)
{
  char *av[1024];
  int ac = 0;
  char *token = strtok (strdup(command), " \t");
  while (token)
    {
      av[ac++] = token;
      token = strtok(0, " \t");
    }
  av[ac] = 0;

  execvp (av[0], av);	/* shouldn't return. */
}


static void
fork_exec_wait (const char *command)
{
  char buf [255];
  pid_t forked;
  int status;

  switch ((int) (forked = fork ()))
    {
    case -1:
      sprintf (buf, "%s: couldn't fork", progname);
      perror (buf);
      return;

    case 0:
      exec_simple_command (command);
      exit (1);  /* exits child fork */
      break;

    default:
      waitpid (forked, &status, 0);
      break;
    }
}


typedef struct {
  void (*callback) (Screen *, Window, Drawable,
                    const char *name, XRectangle *geom, void *closure);
  Screen *screen;
  Window window;
  Drawable drawable;
  void *closure;
  FILE *read_pipe;
  FILE *write_pipe;
  XtInputId pipe_id;
  pid_t pid;
} grabclient_data;


static void finalize_cb (XtPointer closure, int *fd, XtIntervalId *id);

static void
fork_exec_cb (const char *command,
              Screen *screen, Window window, Drawable drawable,
              void (*callback) (Screen *, Window, Drawable,
                                const char *name, XRectangle *geom,
                                void *closure),
              void *closure)
{
  XtAppContext app = XtDisplayToApplicationContext (DisplayOfScreen (screen));
  grabclient_data *data;
  char buf [255];
  pid_t forked;

  int fds [2];

  if (pipe (fds))
    {
      sprintf (buf, "%s: creating pipe", progname);
      perror (buf);
      exit (1);
    }

  data = (grabclient_data *) calloc (1, sizeof(*data));
  data->callback   = callback;
  data->closure    = closure;
  data->screen     = screen;
  data->window     = window;
  data->drawable   = drawable;
  data->read_pipe  = fdopen (fds[0], "r");
  data->write_pipe = fdopen (fds[1], "w");

  if (!data->read_pipe || !data->write_pipe)
    {
      sprintf (buf, "%s: fdopen", progname);
      perror (buf);
      exit (1);
    }

  data->pipe_id =
    XtAppAddInput (app, fileno (data->read_pipe),
                   (XtPointer) (XtInputReadMask | XtInputExceptMask),
                   finalize_cb, (XtPointer) data);

  forked = fork ();
  switch ((int) forked)
    {
    case -1:
      sprintf (buf, "%s: couldn't fork", progname);
      perror (buf);
      return;

    case 0:					/* child */

      fclose (data->read_pipe);
      data->read_pipe = 0;

      /* clone the write pipe onto stdout so that it gets closed
         when the fork exits.  This will shut down the pipe and
         signal the parent.
       */
      close (fileno (stdout));
      dup2 (fds[1], fileno (stdout));
      close (fds[1]);

      close (fileno (stdin)); /* for kicks */

      exec_simple_command (command);
      exit (1);  /* exits child fork */
      break;

    default:					/* parent */
      fclose (data->write_pipe);
      data->write_pipe = 0;
      data->pid = forked;
      break;
    }
}


/* Called in the parent when the forked process dies.
   Runs the caller's callback, and cleans up.
 */
static void
finalize_cb (XtPointer closure, int *fd, XtIntervalId *id)
{
  grabclient_data *data = (grabclient_data *) closure;
  Display *dpy = DisplayOfScreen (data->screen);
  char *name;
  XRectangle geom = { 0, 0, 0, 0 };

  XtRemoveInput (*id);

  name = get_name (dpy, data->window);
  get_geometry (dpy, data->window, &geom);

  data->callback (data->screen, data->window, data->drawable,
                  name, &geom, data->closure);
  if (name) free (name);

  fclose (data->read_pipe);

  if (data->pid)	/* reap zombies */
    {
      int status;
      waitpid (data->pid, &status, 0);
      data->pid = 0;
    }

  memset (data, 0, sizeof (*data));
  free (data);
}


/* Loads an image into the Drawable.
   When grabbing desktop images, the Window will be unmapped first.
 */
static void
load_random_image_1 (Screen *screen, Window window, Drawable drawable,
                     void (*callback) (Screen *, Window, Drawable,
                                       const char *name, XRectangle *geom,
                                       void *closure),
                     void *closure,
                     char **name_ret,
                     XRectangle *geom_ret)
{
  Display *dpy = DisplayOfScreen (screen);
  char *grabber = get_string_resource(dpy, "desktopGrabber", "DesktopGrabber");
  char *cmd;
  char id[200];

  if (!grabber || !*grabber)
    {
      fprintf (stderr,
         "%s: resources installed incorrectly: \"desktopGrabber\" is unset!\n",
               progname);
      exit (1);
    }

  sprintf (id, "0x%lx 0x%lx",
           (unsigned long) window,
           (unsigned long) drawable);
  cmd = (char *) malloc (strlen(grabber) + strlen(id) + 1);

  /* Needn't worry about buffer overflows here, because the buffer is
     longer than the length of the format string, and the length of what
     we're putting into it.  So the only way to crash would be if the
     format string itself was corrupted, but that comes from the
     resource database, and if hostile forces have access to that,
     then the game is already over.
   */
  sprintf (cmd, grabber, id);
  free (grabber);
  grabber = 0;

  /* In case "cmd" fails, leave some random image on the screen, not just
     black or white, so that it's more obvious what went wrong. */
  checkerboard (screen, drawable);
  if (window == drawable)
    print_loading_msg (screen, window);

  XSync (dpy, True);
  hack_subproc_environment (dpy);

  if (callback)
    {
      /* Start the image loading in another fork and return immediately.
         Invoke the callback function when done.
       */
      if (name_ret) abort();
      fork_exec_cb (cmd, screen, window, drawable, callback, closure);
    }
  else
    {
      /* Wait for the image to load, and return it immediately.
       */
      fork_exec_wait (cmd);
      if (name_ret)
        *name_ret = get_name (dpy, window);
      if (geom_ret)
        get_geometry (dpy, window, geom_ret);
    }

  free (cmd);
  XSync (dpy, True);
}

#else  /* HAVE_COCOA */

/* Gets the name of an image file to load by running xscreensaver-getimage-file
   at the end of a pipe.  This can be very slow!
 */
static FILE *
open_image_name_pipe (const char *dir)
{
  char *cmd = malloc (strlen(dir) * 2 + 100);
  char *s;
  strcpy (cmd, "xscreensaver-getimage-file --name ");
  s = cmd + strlen (cmd);
  while (*dir) {
    char c = *dir++;
    /* put a backslash in front of any character that might confuse sh. */
    if (! ((c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           c == '.' || c == '_' || c == '-' || c == '+' || c == '/'))
      *s++ = '\\';
    *s++ = c;
  }
  *s = 0;

  FILE *pipe = popen (cmd, "r");
  free (cmd);
  return pipe;
}


struct pipe_closure {
  FILE *pipe;
  XtInputId id;
  Screen *screen;
  Window xwindow;
  Drawable drawable;
  void (*callback) (Screen *, Window, Drawable,
                    const char *name, XRectangle *geom,
                    void *closure);
  void *closure;
};


static void
pipe_cb (XtPointer closure, int *source, XtInputId *id)
{
  /* This is not called from a signal handler, so doing stuff here is fine.
   */
  struct pipe_closure *clo2 = (struct pipe_closure *) closure;
  char buf[10240];
  fgets (buf, sizeof(buf)-1, clo2->pipe);
  pclose (clo2->pipe);
  clo2->pipe = 0;
  XtRemoveInput (clo2->id);
  clo2->id = 0;

  /* strip trailing newline */
  int L = strlen(buf);
  while (L > 0 && (buf[L-1] == '\r' || buf[L-1] == '\n'))
    buf[--L] = 0;

  Display *dpy = DisplayOfScreen (clo2->screen);
  XRectangle geom;

  if (! osx_load_image_file (clo2->screen, clo2->xwindow, clo2->drawable,
                             buf, &geom)) {
    /* unable to load image - draw colorbars 
     */
    XWindowAttributes xgwa;
    XGetWindowAttributes (dpy, clo2->xwindow, &xgwa);
    Window r;
    int x, y;
    unsigned int w, h, bbw, d;
    struct stat *st;

    /* Log something to syslog so we can tell the difference between
       corrupted images and broken symlinks. */
    if (!*buf)
      fprintf (stderr, "%s: no image filename found\n", progname);
    else if (! stat (buf, st))
      fprintf (stderr, "%s: %s: unparsable\n", progname, buf);
    else
      {
        char buf2[2048];
        sprintf (buf2, "%.255s: %.1024s", progname, buf);
        perror (buf2);
      }

    XGetGeometry (dpy, clo2->drawable, &r, &x, &y, &w, &h, &bbw, &d);
    draw_colorbars (clo2->screen, xgwa.visual, clo2->drawable, xgwa.colormap,
                    0, 0, w, h);
    geom.x = geom.y = 0;
    geom.width = w;
    geom.height = h;
  }

  clo2->callback (clo2->screen, clo2->xwindow, clo2->drawable, buf, &geom,
                  clo2->closure);
  clo2->callback = 0;
  free (clo2);
}


static void
osx_load_image_file_async (Screen *screen, Window xwindow, Drawable drawable,
                           const char *dir,
                           void (*callback) (Screen *, Window, Drawable,
                                             const char *name,
                                             XRectangle *geom,
                                             void *closure),
                       void *closure)
{
#if 0	/* do it synchronously */

  FILE *pipe = open_image_name_pipe (dir);
  char buf[10240];
  *buf = 0;
  fgets (buf, sizeof(buf)-1, pipe);
  pclose (pipe);

  /* strip trailing newline */
  int L = strlen(buf);
  while (L > 0 && (buf[L-1] == '\r' || buf[L-1] == '\n'))
    buf[--L] = 0;

  XRectangle geom;
  if (! osx_load_image_file (screen, xwindow, drawable, buf, &geom)) {
    /* draw colorbars */
    abort();
  }
  callback (screen, xwindow, drawable, buf, &geom, closure);

#else	/* do it asynchronously */

  Display *dpy = DisplayOfScreen (screen);
  struct pipe_closure *clo2 = (struct pipe_closure *) calloc (1, sizeof(*clo2));
  clo2->pipe = open_image_name_pipe (dir);
  clo2->id = XtAppAddInput (XtDisplayToApplicationContext (dpy), 
                            fileno (clo2->pipe),
                            (XtPointer) (XtInputReadMask | XtInputExceptMask),
                            pipe_cb, (XtPointer) clo2);
  clo2->screen = screen;
  clo2->xwindow = xwindow;
  clo2->drawable = drawable;
  clo2->callback = callback;
  clo2->closure = closure;
#endif
}


/* Loads an image into the Drawable, returning once the image is loaded.
 */
static void
load_random_image_1 (Screen *screen, Window window, Drawable drawable,
                     void (*callback) (Screen *, Window, Drawable,
                                       const char *name, XRectangle *geom,
                                       void *closure),
                     void *closure,
                     char **name_ret,
                     XRectangle *geom_ret)
{
  Display *dpy = DisplayOfScreen (screen);
  XWindowAttributes xgwa;
  Bool deskp = get_boolean_resource (dpy, "grabDesktopImages",  "Boolean");
  Bool filep = get_boolean_resource (dpy, "chooseRandomImages", "Boolean");
  const char *dir = 0;
  Bool done = False;
  XRectangle geom_ret_2;
  char *name_ret_2 = 0;

  if (!drawable) abort();

  if (callback) {
    geom_ret = &geom_ret_2;
    name_ret = &name_ret_2;
  }

  XGetWindowAttributes (dpy, window, &xgwa);
  {
    Window r;
    int x, y;
    unsigned int w, h, bbw, d;
    XGetGeometry (dpy, drawable, &r, &x, &y, &w, &h, &bbw, &d);
    xgwa.width = w;
    xgwa.height = h;
  }

  if (name_ret)
    *name_ret = 0;

  if (geom_ret) {
    geom_ret->x = 0;
    geom_ret->y = 0;
    geom_ret->width  = xgwa.width;
    geom_ret->height = xgwa.height;
  }

  if (filep)
    dir = get_string_resource (dpy, "imageDirectory", "ImageDirectory");

  if (!dir || !*dir)
    filep = False;

  if (deskp && filep) {
    deskp = !(random() & 5);    /* if both, desktop 1/5th of the time */
    filep = !deskp;
  }

  if (filep && !done) {
    osx_load_image_file_async (screen, window, drawable, dir, 
                               callback, closure);
    return;
  }

  if (deskp && !done) {
    osx_grab_desktop_image (screen, window, drawable);
    if (name_ret)
      *name_ret = strdup ("desktop");
    done = True;
  }

  if (! done) {
    draw_colorbars (screen, xgwa.visual, drawable, xgwa.colormap,
                    0, 0, xgwa.width, xgwa.height);
    done = True;
  }

  if (callback) {
    /* If we got here, we loaded synchronously even though they wanted async.
     */
    callback (screen, window, drawable, name_ret_2, &geom_ret_2, closure);
  }
}

#endif /* HAVE_COCOA */


/* Writes the string "Loading..." in the middle of the screen.
   This will presumably get blown away when the image finally loads,
   minutes or hours later...

   This is called by load_image_async_simple() but not by load_image_async(),
   since it is assumed that hacks that are loading more than one image
   *at one time* will be doing something more clever than just blocking
   with a blank screen.
 */
static void
print_loading_msg (Screen *screen, Window window)
{
  Display *dpy = DisplayOfScreen (screen);
  XWindowAttributes xgwa;
  XGCValues gcv;
  XFontStruct *f = 0;
  GC gc;
  char *fn = get_string_resource (dpy, "labelFont", "Font");
  const char *text = "Loading...";
  int w;

  if (!fn) fn = get_string_resource (dpy, "titleFont", "Font");
  if (!fn) fn = get_string_resource (dpy, "font", "Font");
  if (!fn) fn = strdup ("-*-times-bold-r-normal-*-180-*");
  f = XLoadQueryFont (dpy, fn);
  if (!f) f = XLoadQueryFont (dpy, "fixed");
  if (!f) abort();
  free (fn);
  fn = 0;

  XGetWindowAttributes (dpy, window, &xgwa);
  w = XTextWidth (f, text, strlen(text));

  gcv.foreground = get_pixel_resource (dpy, xgwa.colormap,
                                       "foreground", "Foreground");
  gcv.background = get_pixel_resource (dpy, xgwa.colormap,
                                       "background", "Background");
  gcv.font = f->fid;
  gc = XCreateGC (dpy, window, GCFont | GCForeground | GCBackground, &gcv);
  XDrawImageString (dpy, window, gc,
                    (xgwa.width - w) / 2,
                    (xgwa.height - (f->ascent + f->descent)) / 2 + f->ascent,
                    text, strlen(text));
  XFreeFont (dpy, f);
  XFreeGC (dpy, gc);
  XSync (dpy, False);
}


/* Loads an image into the Drawable in the background;
   when the image is fully loaded, runs the callback.
   When grabbing desktop images, the Window will be unmapped first.
 */
void
load_image_async (Screen *screen, Window window, Drawable drawable,
                  void (*callback) (Screen *, Window, Drawable,
                                    const char *name, XRectangle *geom,
                                    void *closure),
                  void *closure)
{
  load_random_image_1 (screen, window, drawable, callback, closure, 0, 0);
}

struct async_load_state {
  Bool done_p;
  char *filename;
  XRectangle geom;
};

static void
load_image_async_simple_cb (Screen *screen, Window window, Drawable drawable,
                            const char *name, XRectangle *geom, void *closure)
{
  async_load_state *state = (async_load_state *) closure;
  state->done_p = True;
  state->filename = (name ? strdup (name) : 0);
  state->geom = *geom;
}

async_load_state *
load_image_async_simple (async_load_state *state,
                         Screen *screen,
                         Window window,
                         Drawable drawable, 
                         char **filename_ret,
                         XRectangle *geometry_ret)
{
  if (state && state->done_p)		/* done! */
    {
      if (filename_ret)
        *filename_ret = state->filename;
      else if (state->filename)
        free (state->filename);

      if (geometry_ret)
        *geometry_ret = state->geom;

      free (state);
      return 0;
    }
  else if (! state)			/* first time */
    {
      state = (async_load_state *) calloc (1, sizeof(*state));
      state->done_p = False;
      print_loading_msg (screen, window);
      load_image_async (screen, window, drawable, 
                        load_image_async_simple_cb,
                        state);
      return state;
    }
  else					/* still waiting */
    return state;
}
