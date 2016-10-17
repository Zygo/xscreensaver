/* xscreensaver, Copyright (c) 1992-2016 Jamie Zawinski <jwz@jwz.org>
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

/* This code is a mess.  There's two decades of history in this file.
   There are several distinct paths through this file depending on what
   platform it's being compiled for:


   X11 execution path:

       load_image_async CB
           load_random_image_x11
               fork_exec_cb
                   "xscreensaver-getimage 0xWINDOW 0xPIXMAP"
                       "xscreensaver-getimage-file --name /DIR"
                       draw_colorbars
                       XPutImage
                   XtAppAddInput xscreensaver_getimage_cb
       ...
       xscreensaver_getimage_cb
           get_name_from_xprops
           get_original_geometry_from_xprops
           CB name, geom, closure


   MacOS execution path:

       load_image_async CB
           load_random_image_cocoa
               osx_grab_desktop_image (grabclient-osx.m, MacOS version)
                   copy_framebuffer_to_ximage
                   XPutImage
               draw_colorbars
               osx_load_image_file_async
                   open_image_name_pipe
                       "xscreensaver-getimage-file --name /DIR"
                 XtAppAddInput xscreensaver_getimage_file_cb
       ...
       xscreensaver_getimage_file_cb
           osx_load_image_file
               CB name, geom, closure


   iOS execution path:

       load_image_async CB
           load_random_image_cocoa
               osx_grab_desktop_image (grabclient-osx.m, iOS version)
                   CGWindowListCreateImage
                   jwxyz_draw_NSImage_or_CGImage
               draw_colorbars
               ios_load_random_image
                   ios_load_random_image_cb
                       jwxyz_draw_NSImage_or_CGImage
                       CB name, geom, closure


   Andrid execution path:

       load_image_async CB
           load_random_image_android
               jwxyz_load_random_image (jwxyz-android.c)
                   XPutImage
               draw_colorbars
               CB name, geom, closure
 */

#include "utils.h"
#include "grabscreen.h"
#include "resources.h"
#include "yarandom.h"

#ifdef HAVE_JWXYZ
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


/* Used for pipe callbacks in X11 or OSX mode.
   X11: this is the xscreensaver_getimage_cb closure,
     when waiting on the fork of "xscreensaver-getimage"
   OSX: this is the xscreensaver_getimage_file_cb closure,
     when waiting on the fork of "xscreensaver-getimage-file"
 */
typedef struct {
  void (*callback) (Screen *, Window, Drawable,
                    const char *name, XRectangle *geom, void *closure);
  Screen *screen;
  Window window;
  Drawable drawable;
  void *closure;
  XtInputId pipe_id;
  FILE *pipe;

# if !defined(USE_IPHONE) && !defined(HAVE_COCOA)  /* Real X11 */
  pid_t pid;
# endif

# if !defined(USE_IPHONE) && defined(HAVE_COCOA)   /* Desktop OSX */
  char *directory;
# endif

} xscreensaver_getimage_data;


#if !defined(HAVE_COCOA) && !defined(HAVE_ANDROID)   /* Real X11 */

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


/* Read the image's original name off of the window's X properties.
   Used only when running "real" X11, not jwxyz.
 */
static char *
get_name_from_xprops (Display *dpy, Window window)
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


/* Read the image's original geometry off of the window's X properties.
   Used only when running "real" X11, not jwxyz.
 */
static Bool
get_original_geometry_from_xprops (Display *dpy, Window window, XRectangle *ret)
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
# ifdef HAVE_PUTENV
  if (putenv (ndpy))
    abort ();
# endif /* HAVE_PUTENV */

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

   Used to execute "xscreensaver-getimage".
   Used only when running "real" X11, not jwxyz.
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


static void xscreensaver_getimage_cb (XtPointer closure,
                                      int *fd, XtIntervalId *id);

/* Spawn a program, and run the callback when it finishes.
   Used to execute "xscreensaver-getimage".
   Used only when running "real" X11, not jwxyz.
 */
static void
fork_exec_cb (const char *command,
              Screen *screen, Window window, Drawable drawable,
              void (*callback) (Screen *, Window, Drawable,
                                const char *name, XRectangle *geom,
                                void *closure),
              void *closure)
{
  XtAppContext app = XtDisplayToApplicationContext (DisplayOfScreen (screen));
  xscreensaver_getimage_data *data;
  char buf [255];
  pid_t forked;
  FILE *wpipe;

  int fds [2];

  if (pipe (fds))
    {
      sprintf (buf, "%s: creating pipe", progname);
      perror (buf);
      exit (1);
    }

  data = (xscreensaver_getimage_data *) calloc (1, sizeof(*data));
  data->callback   = callback;
  data->closure    = closure;
  data->screen     = screen;
  data->window     = window;
  data->drawable   = drawable;
  data->pipe       = fdopen (fds[0], "r");
  wpipe            = fdopen (fds[1], "w");   /* Is this necessary? */

  if (!data->pipe || !wpipe)
    {
      sprintf (buf, "%s: fdopen", progname);
      perror (buf);
      exit (1);
    }

  data->pipe_id =
    XtAppAddInput (app, fileno (data->pipe),
                   (XtPointer) (XtInputReadMask | XtInputExceptMask),
                   xscreensaver_getimage_cb, (XtPointer) data);

  forked = fork ();
  switch ((int) forked)
    {
    case -1:
      sprintf (buf, "%s: couldn't fork", progname);
      perror (buf);
      return;

    case 0:					/* child */

      fclose (data->pipe);
      data->pipe = 0;

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
      fclose (wpipe);
      data->pid = forked;
      break;
    }
}


/* Called in the parent when the forked process dies.
   Runs the caller's callback, and cleans up.
   This runs when "xscreensaver-getimage" exits.
   Used only when running "real" X11, not jwxyz.
 */
static void
xscreensaver_getimage_cb (XtPointer closure, int *fd, XtIntervalId *id)
{
  xscreensaver_getimage_data *data = (xscreensaver_getimage_data *) closure;
  Display *dpy = DisplayOfScreen (data->screen);
  char *name;
  XRectangle geom = { 0, 0, 0, 0 };

  XtRemoveInput (*id);

  name = get_name_from_xprops (dpy, data->window);
  get_original_geometry_from_xprops (dpy, data->window, &geom);

  data->callback (data->screen, data->window, data->drawable,
                  name, &geom, data->closure);
  if (name) free (name);

  fclose (data->pipe);

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
   Used only when running "real" X11, not jwxyz.
 */
static void
load_random_image_x11 (Screen *screen, Window window, Drawable drawable,
                       void (*callback) (Screen *, Window, Drawable,
                                         const char *name, XRectangle *geom,
                                         void *closure),
                       void *closure)
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

  /* Start the image loading in another fork and return immediately.
     Invoke the callback function when done. */
  fork_exec_cb (cmd, screen, window, drawable, callback, closure);

  free (cmd);
  XSync (dpy, True);
}

#elif defined (HAVE_COCOA) /* OSX or iOS */

# ifndef USE_IPHONE   /* HAVE_COCOA && !USE_IPHONE -- desktop OSX */

# define BACKSLASH(c) \
  (! ((c >= 'a' && c <= 'z') || \
      (c >= 'A' && c <= 'Z') || \
      (c >= '0' && c <= '9') || \
      c == '.' || c == '_' || c == '-' || c == '+' || c == '/'))

/* Gets the name of an image file to load by running xscreensaver-getimage-file
   at the end of a pipe.  This can be very slow!
 */
static FILE *
open_image_name_pipe (const char *dir)
{
  char *s;

  /* /bin/sh on OS X 10.10 wipes out the PATH. */
  const char *path = getenv("PATH");
  char *cmd = s = malloc ((strlen(dir) + strlen(path)) * 2 + 100);
  strcpy (s, "/bin/sh -c 'export PATH=");
  s += strlen (s);
  while (*path) {
    char c = *path++;
    if (BACKSLASH(c)) *s++ = '\\';
    *s++ = c;
  }
  strcpy (s, "; ");
  s += strlen (s);

  strcpy (s, "xscreensaver-getimage-file --name ");
  s += strlen (s);
  while (*dir) {
    char c = *dir++;
    if (BACKSLASH(c)) *s++ = '\\';
    *s++ = c;
  }

  strcpy (s, "'");
  s += strlen (s);

  *s = 0;

  FILE *pipe = popen (cmd, "r");
  free (cmd);
  return pipe;
}


static void
xscreensaver_getimage_file_cb (XtPointer closure, int *source, XtInputId *id)
{
  /* This is not called from a signal handler, so doing stuff here is fine.
   */
  xscreensaver_getimage_data *clo2 = (xscreensaver_getimage_data *) closure;
  char buf[10240];
  const char *dir = clo2->directory;
  char *absfile = 0;
  *buf = 0;
  fgets (buf, sizeof(buf)-1, clo2->pipe);
  pclose (clo2->pipe);
  clo2->pipe = 0;
  XtRemoveInput (clo2->pipe_id);
  clo2->pipe_id = 0;

  /* strip trailing newline */
  int L = strlen(buf);
  while (L > 0 && (buf[L-1] == '\r' || buf[L-1] == '\n'))
    buf[--L] = 0;

  Display *dpy = DisplayOfScreen (clo2->screen);
  XRectangle geom;

  if (*buf && *buf != '/')		/* pathname is relative to dir. */
    {
      absfile = malloc (strlen(dir) + strlen(buf) + 10);
      strcpy (absfile, dir);
      if (dir[strlen(dir)-1] != '/')
        strcat (absfile, "/");
      strcat (absfile, buf);
    }

  if (! osx_load_image_file (clo2->screen, clo2->window, clo2->drawable,
                             (absfile ? absfile : buf), &geom)) {
    /* unable to load image - draw colorbars 
     */
    XWindowAttributes xgwa;
    XGetWindowAttributes (dpy, clo2->window, &xgwa);
    Window r;
    int x, y;
    unsigned int w, h, bbw, d;
    struct stat st;

    /* Log something to syslog so we can tell the difference between
       corrupted images and broken symlinks. */
    if (!*buf)
      fprintf (stderr, "%s: no image filename found\n", progname);
    else if (! stat (buf, &st))
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

  /* Take the extension off of the file name. */
  /* Duplicated in driver/xscreensaver-getimage.c. */
  if (*buf)
    {
      char *slash = strrchr (buf, '/');
      char *dot = strrchr ((slash ? slash : buf), '.');
      if (dot) *dot = 0;
      /* Replace slashes with newlines */
      /* while (dot = strchr(buf, '/')) *dot = '\n'; */
      /* Replace slashes with spaces */
      /* while ((dot = strchr(buf, '/'))) *dot = ' '; */
    }

  if (absfile) free (absfile);
  clo2->callback (clo2->screen, clo2->window, clo2->drawable, buf, &geom,
                  clo2->closure);
  clo2->callback = 0;
  free (clo2->directory);
  free (clo2);
}


# else   /* HAVE_COCOA && USE_IPHONE -- iOS */

/* Callback for ios_load_random_image(), called after we have loaded an
   image from the iOS device's Photo Library.  See grabclient-ios.m.
 */
static void
ios_load_random_image_cb (void *uiimage, const char *filename, 
                          int width, int height, void *closure)
{
  xscreensaver_getimage_data *clo2 = (xscreensaver_getimage_data *) closure;
  Display *dpy = DisplayOfScreen (clo2->screen);
  XRectangle geom;
  XWindowAttributes xgwa;
  Window r;
  int x, y;
  unsigned int w, h, bbw, d;
  int rot = 0;

  XGetWindowAttributes (dpy, clo2->window, &xgwa);
  XGetGeometry (dpy, clo2->drawable, &r, &x, &y, &w, &h, &bbw, &d);

  /* If the image is portrait and the window is landscape, or vice versa,
     rotate the image. The idea is to fill up as many pixels as possible,
     and assume the user will just rotate their phone until it looks right.
     This makes "decayscreen", etc. much more easily viewable.
   */
  if (get_boolean_resource (dpy, "rotateImages", "RotateImages")) {
    if ((width > height) != (w > h))
      rot = 5;
  }

  if (uiimage)
    {
      jwxyz_draw_NSImage_or_CGImage (DisplayOfScreen (clo2->screen), 
                                     clo2->drawable,
                                     True, uiimage, &geom,
                                     rot);
    }
  else  /* Probably means no images in the gallery. */
    {
      draw_colorbars (clo2->screen, xgwa.visual, clo2->drawable, xgwa.colormap,
                      0, 0, w, h);
      geom.x = geom.y = 0;
      geom.width = w;
      geom.height = h;
      filename = 0;
    }

  clo2->callback (clo2->screen, clo2->window, clo2->drawable,
                  filename, &geom, clo2->closure);
  clo2->callback = 0;
  free (clo2);
}

# endif /* HAVE_COCOA && USE_IPHONE */


static void
osx_load_image_file_async (Screen *screen, Window xwindow, Drawable drawable,
                           const char *dir,
                           void (*callback) (Screen *, Window, Drawable,
                                             const char *name,
                                             XRectangle *geom,
                                             void *closure),
                       void *closure)
{
  xscreensaver_getimage_data *clo2 =
    (xscreensaver_getimage_data *) calloc (1, sizeof(*clo2));

  clo2->screen = screen;
  clo2->window = xwindow;
  clo2->drawable = drawable;
  clo2->callback = callback;
  clo2->closure = closure;

# ifndef USE_IPHONE   /* Desktop OSX */
  clo2->directory = strdup (dir);
  clo2->pipe = open_image_name_pipe (dir);
  clo2->pipe_id = XtAppAddInput (XtDisplayToApplicationContext (
                            DisplayOfScreen (screen)), 
                            fileno (clo2->pipe),
                            (XtPointer) (XtInputReadMask | XtInputExceptMask),
                            xscreensaver_getimage_file_cb, (XtPointer) clo2);
# else /* USE_IPHONE */
  ios_load_random_image (ios_load_random_image_cb, clo2);
# endif /* USE_IPHONE */
}


/* Loads an image into the Drawable, returning once the image is loaded.
 */
static void
load_random_image_cocoa (Screen *screen, Window window, Drawable drawable,
                         void (*callback) (Screen *, Window, Drawable,
                                           const char *name, XRectangle *geom,
                                           void *closure),
                         void *closure)
{
  Display *dpy = DisplayOfScreen (screen);
  XWindowAttributes xgwa;
  Bool deskp = get_boolean_resource (dpy, "grabDesktopImages",  "Boolean");
  Bool filep = get_boolean_resource (dpy, "chooseRandomImages", "Boolean");
  const char *dir = 0;
  Bool done = False;
  XRectangle geom;
  char *name = 0;
  
  if (!drawable) abort();

  XGetWindowAttributes (dpy, window, &xgwa);
  {
    Window r;
    int x, y;
    unsigned int w, h, bbw, d;
    XGetGeometry (dpy, drawable, &r, &x, &y, &w, &h, &bbw, &d);
    xgwa.width = w;
    xgwa.height = h;
  }

  geom.x = 0;
  geom.y = 0;
  geom.width  = xgwa.width;
  geom.height = xgwa.height;

# ifndef USE_IPHONE
  if (filep)
    dir = get_string_resource (dpy, "imageDirectory", "ImageDirectory");

  if (!dir || !*dir)
    filep = False;
# endif /* ! USE_IPHONE */

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
    if (osx_grab_desktop_image (screen, window, drawable, &geom)) {
      name = strdup ("desktop");
      done = True;
    }
  }

  if (! done)
    draw_colorbars (screen, xgwa.visual, drawable, xgwa.colormap,
                    0, 0, xgwa.width, xgwa.height);

  /* If we got here, we loaded synchronously, so we're done. */
  callback (screen, window, drawable, name, &geom, closure);
  if (name) free (name);
}


#elif defined(HAVE_ANDROID)

/* Loads an image into the Drawable, returning once the image is loaded.
 */
static void
load_random_image_android (Screen *screen, Window window, Drawable drawable,
                           void (*callback) (Screen *, Window, Drawable,
                                             const char *name,
                                             XRectangle *geom, void *closure),
                           void *closure)
{
  Display *dpy = DisplayOfScreen (screen);
  XWindowAttributes xgwa;
  XRectangle geom;
  char *name = 0;
  char *data = 0;
  int width  = 0;
  int height = 0;
  
  if (!drawable) abort();

  XGetWindowAttributes (dpy, window, &xgwa);
  {
    Window r;
    int x, y;
    unsigned int w, h, bbw, d;
    XGetGeometry (dpy, drawable, &r, &x, &y, &w, &h, &bbw, &d);
    xgwa.width = w;
    xgwa.height = h;
  }

  geom.x = 0;
  geom.y = 0;
  geom.width  = xgwa.width;
  geom.height = xgwa.height;

  data = jwxyz_load_random_image (dpy, &width, &height, &name);
  if (! data)
    draw_colorbars (screen, xgwa.visual, drawable, xgwa.colormap,
                    0, 0, xgwa.width, xgwa.height);
  else
    {
      XImage *img = XCreateImage (dpy, xgwa.visual, 32,
                                  ZPixmap, 0, data, width, height, 0, 0);
      XGCValues gcv;
      GC gc;
      gcv.foreground = BlackPixelOfScreen (screen);
      gc = XCreateGC (dpy, drawable, GCForeground, &gcv);
      XFillRectangle (dpy, drawable, gc, 0, 0, xgwa.width, xgwa.height);
      XPutImage (dpy, drawable, gc, img, 0, 0, 
                 (xgwa.width  - width) / 2,
                 (xgwa.height - height) / 2,
                 width, height);
      XDestroyImage (img);
      XFreeGC (dpy, gc);
    }

  callback (screen, window, drawable, name, &geom, closure);
  if (name) free (name);
}

#endif /* HAVE_ANDROID */



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
  w = XTextWidth (f, text, (int) strlen(text));

  gcv.foreground = get_pixel_resource (dpy, xgwa.colormap,
                                       "foreground", "Foreground");
  gcv.background = get_pixel_resource (dpy, xgwa.colormap,
                                       "background", "Background");
  gcv.font = f->fid;
  gc = XCreateGC (dpy, window, GCFont | GCForeground | GCBackground, &gcv);
  XDrawImageString (dpy, window, gc,
                    (xgwa.width - w) / 2,
                    (xgwa.height - (f->ascent + f->descent)) / 2 + f->ascent,
                    text, (int) strlen(text));
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
  if (!callback) abort();
# if defined(HAVE_COCOA)
  load_random_image_cocoa   (screen, window, drawable, callback, closure);
# elif defined(HAVE_ANDROID)
  load_random_image_android (screen, window, drawable, callback, closure);
# else /* real X11 */
  load_random_image_x11     (screen, window, drawable, callback, closure);
# endif
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
