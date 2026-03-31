/* xscreensaver-command, Copyright © 1991-2021 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xproto.h>		/* for CARD32 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "atoms.h"

#ifdef HAVE_UNAME
# include <sys/utsname.h>	/* for uname() */
#endif /* HAVE_UNAME */


/* Set some properties to hopefully tell the window manager to leave us alone.
   This is used by xscreensaver-gfx and xscreensaver-auth but not xscreensaver.
 */
void
xscreensaver_set_wm_atoms (Display *dpy, Window window, int width, int height, 
                           Window for_window)
{
  XClassHint class_hints;
  XSizeHints size_hints;
# ifdef HAVE_UNAME
  struct utsname uts;
# endif
  Atom va[10];
  long vl[10];
  int i;
  class_hints.res_name  = "xscreensaver";  /* not progname */
  class_hints.res_class = "XScreenSaver";
  size_hints.flags      = PMinSize | PMaxSize;
  size_hints.min_width  = size_hints.max_width  = width;  /* non-resizable */
  size_hints.min_height = size_hints.max_height = height;
  XStoreName (dpy, window, "XScreenSaver");
  XSetClassHint (dpy, window, &class_hints);
  XSetWMNormalHints (dpy, window, &size_hints);

  /* XA_WM_COMMAND and _NET_WM_PID are later updated by spawn_screenhack.  */
  XChangeProperty (dpy, window, XA_WM_COMMAND,
                   XA_STRING, 8, PropModeReplace,
                   (unsigned char *) class_hints.res_name,
                   strlen (class_hints.res_name));

# ifdef HAVE_UNAME
  if (! uname (&uts))
    XChangeProperty (dpy, window,
                     XA_WM_CLIENT_MACHINE, XA_STRING, 8,
                     PropModeReplace, (unsigned char *) uts.nodename,
                     strlen (uts.nodename));
# endif

  /* In the olden days, OverrideRedirect meant that the window manager did
     not see or touch our window, but these days, compositing WMs like to
     get up to all sorts of shenanigans.  I don't know whether setting these
     properties has any effect, but they *might* tell the WM to keep its
     grubby paws off of our windows.
   */

  i = 0;
  vl[i++] = 1;  /* _NET_WM_BYPASS_COMPOSITOR = 1 */
  XChangeProperty (dpy, window, XA_NET_WM_BYPASS_COMPOSITOR, XA_CARDINAL, 32,
                   PropModeReplace, (unsigned char *) vl, i);

  /* _NET_WM_STATE = [ _NET_WM_STATE_ABOVE, _NET_WM_STATE_FULLSCREEN ] */
  i = 0;
  va[i++] = XA_NET_WM_STATE_ABOVE;
  va[i++] = XA_NET_WM_STATE_FULLSCREEN;
  va[i++] = XA_NET_WM_STATE_STAYS_ON_TOP;  /* Does this do anything? */
  XChangeProperty (dpy, window, XA_NET_WM_STATE, XA_ATOM, 32,
                   PropModeReplace, (unsigned char *) va, i);

  /* As there is no _NET_WM_WINDOW_TYPE_SCREENSAVER, which property is
     most likely to effectively communicate "on top always" to the WM?
     _NET_WM_WINDOW_TYPE = NORMAL, SPLASH, DIALOG or NOTIFICATION? */
  i = 0;
  va[i++] = XA_NET_WM_WINDOW_TYPE_NOTIFICATION;
  va[i++] = XA_KDE_NET_WM_WINDOW_TYPE_OVERRIDE;  /* Does this do anything? */
  XChangeProperty (dpy, window, XA_NET_WM_WINDOW_TYPE, XA_ATOM, 32,
                   PropModeReplace, (unsigned char *) va, i);

  if (for_window)  /* This is the error dialog for a saver window */
    {
      va[0] = for_window;
      /* _WM_TRANSIENT_FOR = screensaver_window */
      XChangeProperty (dpy, window,
                       XA_WM_TRANSIENT_FOR, XA_WINDOW, 32,
                       PropModeReplace, (unsigned char *) va, 1);
    }
}
