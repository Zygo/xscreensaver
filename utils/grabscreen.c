/* xscreensaver, Copyright (c) 1992, 1993 Jamie Zawinski <jwz@lucid.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This file contains code for grabbing an image of the screen to hack its
   bits.  This is a little tricky, since doing this involves the need to tell
   the difference between drawing on the actual root window, and on the fake
   root window used by the screensaver, since at this level the illusion 
   breaks down...
 */

#if __STDC__
#include <stdlib.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xatom.h>

static Bool
MapNotify_event_p (dpy, event, window)
     Display *dpy;
     XEvent *event;
     XPointer window;
{
  return (event->xany.type == MapNotify &&
	  event->xvisibility.window == (Window) window);
}


static Bool
screensaver_window_p (dpy, window)
     Display *dpy;
     Window window;
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

Pixmap
grab_screen_image (dpy, window, root_p)
     Display *dpy;
     Window window;
     int root_p;
{
  Pixmap pixmap = 0;
  XWindowAttributes xgwa;

  XGetWindowAttributes (dpy, window, &xgwa);

  if (screensaver_window_p (dpy, window))
    {
      /* note: this assumes vroot.h didn't encapsulate the XRootWindowOfScreen
	 function, only the RootWindowOfScreen macro... */
      Window real_root = XRootWindowOfScreen (DefaultScreenOfDisplay (dpy));

      XSetWindowBackgroundPixmap (dpy, window, None);

      /* prevent random viewer of the screen saver (locker) from messing
	 with windows.   We don't check whether it succeeded, because what
	 are our options, really... */
      XGrabPointer (dpy, real_root, True, ButtonPressMask|ButtonReleaseMask,
		    GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
      XGrabKeyboard (dpy, real_root, True, GrabModeSync, GrabModeAsync,
		     CurrentTime);

      XUnmapWindow (dpy, window);
      XSync (dpy, True);
      sleep (5);     /* wait for everyone to swap in and handle exposes... */
      XMapRaised (dpy, window);

      XUngrabPointer (dpy, CurrentTime);
      XUngrabKeyboard (dpy, CurrentTime);

      XSync (dpy, True);
    }
  else if (root_p)
    {
      XGCValues gcv;
      GC gc;
      gcv.function = GXcopy;
      gcv.subwindow_mode = IncludeInferiors;
      gc = XCreateGC (dpy, window, GCFunction | GCSubwindowMode, &gcv);
      pixmap = XCreatePixmap(dpy, window, xgwa.width, xgwa.height, xgwa.depth);
      XCopyArea (dpy, RootWindowOfScreen (xgwa.screen), pixmap, gc,
		 xgwa.x, xgwa.y, xgwa.width, xgwa.height, 0, 0);
      XFreeGC (dpy, gc);
      XSetWindowBackgroundPixmap (dpy, window, pixmap);
    }
  else
    {
      XEvent event;
      XSetWindowBackgroundPixmap (dpy, window, None);
      XMapWindow (dpy, window);
      XFlush (dpy);
      XIfEvent (dpy, &event, MapNotify_event_p, (XPointer) window);
      XSync (dpy, True);
    }
  return pixmap;
}
