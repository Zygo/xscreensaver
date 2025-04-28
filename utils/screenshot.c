/* xscreensaver-command, Copyright © 2022 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Grab images of the desktop.  See the comments at the top of
 * ../hacks/xscreensaver-getimage.c for the how and why of it.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <X11/Xproto.h>		/* for CARD32 */
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "screenshot.h"
#include "visual.h"
#include "../driver/blurb.h"

static Atom XA_SCREENSAVER_SCREENSHOT = 0;

static void
screenshot_init (Display *dpy)
{
  if (!XA_SCREENSAVER_SCREENSHOT)
    XA_SCREENSAVER_SCREENSHOT =
      XInternAtom (dpy, "_SCREENSAVER_SCREENSHOT", False);
}


/* Returns the absolute position of the window on its root window.
 */
void
window_root_offset (Display *dpy, Window window, int *x, int *y)
{
  *x = 0;
  *y = 0;
  while (1)
    {
      Window root, parent, *kids = 0;
      XWindowAttributes xgwa;
      unsigned int nkids = 0;
      XGetWindowAttributes (dpy, window, &xgwa);
      *x += xgwa.x;
      *y += xgwa.y;

      if (! XQueryTree (dpy, window, &root, &parent, &kids, &nkids))
        abort ();
      if (kids) XFree ((char *) kids);
      if (!parent || parent == root) break;
      window = parent;
    }
}


static Bool error_handler_hit_p = False;

static int
ignore_all_errors_ehandler (Display *dpy, XErrorEvent *error)
{
  error_handler_hit_p = True;
  return 0;
}


/* Grab a screenshot and return it.
   It will be the size and extent of the given window,
   or the full screen, as requested.
   Might be None if we failed.
 */
Pixmap
screenshot_grab (Display *dpy, Window window, Bool full_screen_p,
                 Bool verbose_p)
{
  Window root_window;
  XWindowAttributes root_xgwa, win_xgwa, owin_xgwa;
  Pixmap pixmap;
  XGCValues gcv;
  GC gc;
  struct { int x, y, x2, y2; } root, win;
  XErrorHandler old_handler;

  XGetWindowAttributes (dpy, window, &win_xgwa);
  root_window = XRootWindowOfScreen (win_xgwa.screen);
  XGetWindowAttributes (dpy, root_window, &root_xgwa);

  if (visual_class (win_xgwa.screen, win_xgwa.visual) != TrueColor)
    {
      if (verbose_p)
        fprintf (stderr, "%s: no screenshot, not TrueColor\n", blurb());
      return None;
    }

# ifdef __APPLE__   /* Can't XCopyArea root window under rootless XQuartz. */
  if (verbose_p)
    fprintf (stderr, "%s: no screenshot under XQuartz\n", blurb());
  return None;
# endif

  root.x  = 0;
  root.y  = 0;
  root.x2 = root_xgwa.width;
  root.y2 = root_xgwa.height;

  owin_xgwa = win_xgwa;
  if (full_screen_p)
    {
      win_xgwa = root_xgwa;
      win = root;
    }
  else
    {
      window_root_offset (dpy, window, &win.x, &win.y);
      owin_xgwa.x = win.x;
      owin_xgwa.y = win.y;
      win.x2 = win.x + win_xgwa.width;
      win.y2 = win.y + win_xgwa.height;
    }

  /* Clip win to within root, in case it is partially off screen. */
  if (win.x < 0) win.x = 0;
  if (win.y < 0) win.y = 0;
  if (win.x2 > root.x2) win.x2 = root.x2;
  if (win.y2 > root.y2) win.y2 = root.y2;

  gcv.function = GXcopy;
  gcv.subwindow_mode = IncludeInferiors;
  pixmap = XCreatePixmap (dpy, root_window,
                          win_xgwa.width, win_xgwa.height,
                          win_xgwa.depth);
  gc = XCreateGC (dpy, pixmap, GCFunction | GCSubwindowMode, &gcv);

  /* I do not understand why some systems get a BadMatch on this XCopyArea.
     (Sep 2022, Debian 11.5 x86 under VirtualBox.) */
  XSync (dpy, False);
  error_handler_hit_p = False;
  old_handler = XSetErrorHandler (ignore_all_errors_ehandler);

  XCopyArea (dpy, root_window, pixmap, gc,
             win.x, win.y,
             win.x2 - win.x,
             win.y2 - win.y,
             0, 0);

  XSync (dpy, False);
  XSetErrorHandler (old_handler);
  if (error_handler_hit_p)
    {
      XFreePixmap (dpy, pixmap);
      pixmap = 0;
    }

  XFreeGC (dpy, gc);

  if (verbose_p || !pixmap)
    fprintf (stderr, "%s: %s screenshot 0x%lx %dx%d"
             " for window 0x%lx %dx%d+%d+%d\n", blurb(),
             (pixmap ? "saved" : "failed to save"),
             (unsigned long) pixmap, win_xgwa.width, win_xgwa.height,
             (unsigned long) window,
             owin_xgwa.width, owin_xgwa.height, owin_xgwa.x, owin_xgwa.y);
  return pixmap;
}


/* Store the screenshot pixmap on a property on the window. */
void
screenshot_save (Display *dpy, Window window, Pixmap pixmap)
{
  screenshot_init (dpy);
  XChangeProperty (dpy, window, XA_SCREENSAVER_SCREENSHOT, XA_PIXMAP, 32,
                   PropModeReplace, (unsigned char *) &pixmap, 1);
}


/* Returns a screenshot pixmap from a property on the window. */
static Pixmap
screenshot_load_prop (Display *dpy, Window window)
{
  Atom type;
  int format;
  unsigned long nitems, bytesafter;
  unsigned char *ret = 0;
  int status;
  Pixmap pixmap = None;

  screenshot_init (dpy);
  status = XGetWindowProperty (dpy, window, XA_SCREENSAVER_SCREENSHOT,
                               0, 2, False, XA_PIXMAP,
                               &type, &format, &nitems, &bytesafter,
                               &ret);
  if (status == Success &&
      type == XA_PIXMAP &&
      nitems == 1 &&
      bytesafter == 0)
    pixmap = ((Pixmap *) ret)[0];

  if (ret) XFree (ret);
  return pixmap;
}


/* Loads the screenshot from screenshot_save() and returns a new pixmap
   that is the same size as the window.
 */
Pixmap
screenshot_load (Display *dpy, Window window, Bool verbose_p)
{
  /* The screenshot pixmap might be bigger than the window: an image
     of the whole screen, and we want the section at our origin and
     size.  It also might be smaller than the window if the screenshot
     was taken and then later the size of our window changed
     (e.g. because of XRANDR).
   */
  XWindowAttributes win_xgwa;
  struct { int x, y, x2, y2, w, h; } root, win, from, to;
  Pixmap screenshot, p2;
  XGCValues gcv;
  GC gc;

  screenshot = screenshot_load_prop (dpy, window);
  if (!screenshot) return 0;

  XGetWindowAttributes (dpy, window, &win_xgwa);
  window_root_offset (dpy, window, &win.x, &win.y);
  win.w  = win_xgwa.width;
  win.h  = win_xgwa.height;
  win.x2 = win.x + win.w;
  win.y2 = win.y + win.h;

  {
    Window r;
    int x, y;
    unsigned int w, h, bw, d;
    XGetGeometry (dpy, screenshot, &r, &x, &y, &w, &h, &bw, &d);
    root.x  = 0;
    root.y  = 0;
    root.w  = w;
    root.h  = h;
    root.x2 = w;
    root.y2 = h;
  }

  /* If win is larger than root, copy root to center of win.
     Else copy the portion of root that is overlapped by win.
   */
  if (win.w >= root.w)
    {
      from.x = 0;
      to.x   = (win.w - root.w) / 2;
      to.w   = root.w;
      from.w = to.w;
    }
  else
    {
      from.x  = win.x;
      from.x2 = win.x2;
      from.w  = from.x2 - from.x;

      to.x    = 0;
      to.x2   = win.w;
      to.w    = to.x2 - to.x;

      if (from.x  < root.x)  from.x  = root.x;
      if (from.x2 > root.x2) from.x2 = root.x2;
      if (to.x  < 0)     to.x = 0;
      if (to.x2 > win.w) to.x2 = win.w;

      from.w = from.x2 - from.x;
      to.w   = to.x2   - to.x;
    }

  if (win.h >= root.h)
    {
      from.y = 0;
      to.y   = (win.h - root.h) / 2;
      to.h   = root.h;
      from.h = to.h;
    }
  else
    {
      from.y  = win.y;
      from.y2 = win.y2;
      from.h  = from.y2 - from.y;

      to.y    = 0;
      to.y2   = win.h;
      to.h    = to.y2 - to.y;

      if (from.y  < root.y)  from.y  = root.y;
      if (from.y2 > root.y2) from.y2 = root.y2;
      if (to.y    < 0)     to.y  = 0;
      if (to.y2   > win.h) to.y2 = win.h;

      from.h = from.y2 - from.y;
      to.h   = to.y2   - to.y;
    }

  /* Create target pixmap the same size as our window. */
  p2 = XCreatePixmap (dpy, window, win_xgwa.width, win_xgwa.height,
                      win_xgwa.depth);

  /* Blank it, in case the copied bits do not completely cover it. */
  gcv.function = GXcopy;
  gcv.foreground = BlackPixelOfScreen (win_xgwa.screen);
  gc = XCreateGC (dpy, window, GCFunction | GCForeground, &gcv);
  XFillRectangle (dpy, p2, gc, 0, 0, win_xgwa.width, win_xgwa.height);
  XCopyArea (dpy, screenshot, p2, gc,
             from.x, from.y, from.w, from.h, to.x, to.y);
  XFreeGC (dpy, gc);

  if (verbose_p)
    fprintf (stderr, "%s: loaded screenshot 0x%lx"
             " for window 0x%lx %dx%d+%d+%d"
             " from saved 0x%lx %dx%d\n", blurb(),
             (unsigned long) p2,
             (unsigned long) window,
             win_xgwa.width, win_xgwa.height, win_xgwa.x, win_xgwa.y,
             (unsigned long) screenshot, root.w, root.h);
  return p2;
}
