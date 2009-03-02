/* xscreensaver, Copyright (c) 1992, 1993, 1994, 1997, 1998, 2001, 2003
 *  Jamie Zawinski <jwz@jwz.org>
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

extern char *progname;


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
  char *version;
  if (XGetWindowProperty (dpy, window,
			  XInternAtom (dpy, "_SCREENSAVER_VERSION", False),
			  0, 1, False, XA_STRING,
			  &type, &format, &nitems, &bytesafter,
			  (unsigned char **) &version)
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
  int x, y;
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


/* Loads an image into the Drawable.
   When grabbing desktop images, the Window will be unmapped first.
 */
void
load_random_image (Screen *screen, Window window, Drawable drawable)
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
  system (cmd);
  free (cmd);
  XSync (dpy, True);
}
