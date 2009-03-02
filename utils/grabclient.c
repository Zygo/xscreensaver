/* xscreensaver, Copyright (c) 1992, 1993, 1994, 1997, 1998, 2001
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
checkerboard (Screen *screen, Window window)
{
  Display *dpy = DisplayOfScreen (screen);
  int x, y;
  int size = 24;
  XColor fg, bg;
  XGCValues gcv;
  GC gc = XCreateGC (dpy, window, 0, &gcv);
  XWindowAttributes xgwa;
  XGetWindowAttributes (dpy, window, &xgwa);
  fg.flags = bg.flags = DoRed|DoGreen|DoBlue;
  fg.red = fg.green = fg.blue = 0x0000;
  bg.red = bg.green = bg.blue = 0x4444;
  fg.pixel = 0;
  bg.pixel = 1;

  /* Allocate black and gray, but don't hold them locked. */
  if (XAllocColor (dpy, xgwa.colormap, &fg))
    XFreeColors (dpy, xgwa.colormap, &fg.pixel, 1, 0);
  if (XAllocColor (dpy, xgwa.colormap, &bg))
    XFreeColors (dpy, xgwa.colormap, &bg.pixel, 1, 0);

  XSetForeground (dpy, gc, bg.pixel);
  XFillRectangle (dpy, window, gc, 0, 0, xgwa.width, xgwa.height);
  XSetForeground (dpy, gc, fg.pixel);
  for (y = 0; y < xgwa.height; y += size+size)
    for (x = 0; x < xgwa.width; x += size+size)
      {
        XFillRectangle (dpy, window, gc, x,      y,      size, size);
        XFillRectangle (dpy, window, gc, x+size, y+size, size, size);
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


void
grab_screen_image (Screen *screen, Window window)
{
  Display *dpy = DisplayOfScreen (screen);
  char *grabber = get_string_resource ("desktopGrabber", "DesktopGrabber");
  char *cmd;
  char id[20];

  if (!grabber || !*grabber)
    {
      fprintf (stderr,
         "%s: resources installed incorrectly: \"desktopGrabber\" is unset!\n",
               progname);
      exit (1);
    }

  sprintf (id, "0x%lx", (unsigned long) window);
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
  checkerboard (screen, window);

  XSync (dpy, True);
  hack_subproc_environment (dpy);
  system (cmd);
  free (cmd);
  XSync (dpy, True);
}
