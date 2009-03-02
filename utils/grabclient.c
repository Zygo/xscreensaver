/* xscreensaver, Copyright (c) 1992-2005 Jamie Zawinski <jwz@jwz.org>
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

#include "vroot.h"
#include <X11/Xatom.h>

#include <X11/Intrinsic.h>   /* for XtInputId, etc */


#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>		/* for waitpid() and associated macros */
#endif


extern char *progname;
extern XtAppContext app;


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
use_subwindow_mode_p(Screen *screen, Window window)
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
    return strdup((char *) name);
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

  switch ((int) (forked = fork ()))
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
  char *grabber = get_string_resource ("desktopGrabber", "DesktopGrabber");
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

  /* In case "cmd" fails, leave some random image on the screen, not just
     black or white, so that it's more obvious what went wrong. */
  checkerboard (screen, drawable);

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


/* Loads an image into the Drawable in the background;
   when the image is fully loaded, runs the callback.
   When grabbing desktop images, the Window will be unmapped first.
 */
void
fork_load_random_image (Screen *screen, Window window, Drawable drawable,
                        void (*callback) (Screen *, Window, Drawable,
                                          const char *name, XRectangle *geom,
                                          void *closure),
                        void *closure)
{
  load_random_image_1 (screen, window, drawable, callback, closure, 0, 0);
}


#ifndef DEBUG

/* Loads an image into the Drawable, returning once the image is loaded.
   When grabbing desktop images, the Window will be unmapped first.
 */
void
load_random_image (Screen *screen, Window window, Drawable drawable,
                   char **name_ret, XRectangle *geom_ret)
{
  load_random_image_1 (screen, window, drawable, 0, 0, name_ret, geom_ret);
}

#else  /* DEBUG */

typedef struct {
  char **name_ret;
  Bool done;
} debug_closure;

static void
debug_cb (Screen *screen, Window window, Drawable drawable,
          const char *name, void *closure)
{
  debug_closure *data = (debug_closure *) closure;
  fprintf (stderr, "%s: GRAB DEBUG: callback\n", progname);
  if (data->name_ret)
    *data->name_ret = (name ? strdup (name) : 0);
  data->done = True;
}

void
load_random_image (Screen *screen, Window window, Drawable drawable,
                   char **name_ret)
{
  debug_closure data;
  data.name_ret = name_ret;
  data.done = False;
  fprintf (stderr, "%s: GRAB DEBUG: forking\n", progname);
  fork_load_random_image (screen, window, drawable, debug_cb, &data);
  while (! data.done)
    {
      fprintf (stderr, "%s: GRAB DEBUG: waiting\n", progname);
      if (XtAppPending (app) & XtIMAlternateInput)
        XtAppProcessEvent (app, XtIMAlternateInput);
      usleep (50000);
    }
  fprintf (stderr, "%s: GRAB DEBUG: done\n", progname);
}

#endif /* DEBUG */
