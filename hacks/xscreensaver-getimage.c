/* xscreensaver, Copyright © 2001-2025 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* xscreensaver-getimage -- helper program that puts a random image onto
   the given window or pixmap.  That image is either a screen-grab, a
   file loaded from disk, or a frame grabbed from the system's camera.

   On X11 systems, the hacks themselves use "utils/grabclient.c" to
   invoke this program, "xscreensaver-getimage", as a sub-process.
   Loading files is straightforward, and the camera thing is file-like.
   File names are produced by "xscreensaver-getimage-file" and/or
   "xscreensaver-getimage-video".

   On macOS, iOS or Android systems, each saver's "utils/grabclient.c"
   instead links against "OSX/grabclient-osx.m", "OSX/grabclient-ios.m"
   or "jwxyz/jwxyz-android.c" to load images and grab screenshots
   directly without invoking a sub-process to do it.  On those systems,
   "xscreensaver-getimage" is not used.

   See the comment at the top of "utils/grabclient.c' for a more
   detailed explanation of the incredibly comvoluted flows of control
   used by the various platforms.

   --------------------------------------------------------------------

   Grabbing screen images works in a few different ways:

   A: If the hack was invoked by XScreenSaver, then before it blanked the
      screen, "xscreensaver-gfx" (or "xscreensaver-settings", in preview mode)
      saved a screenshot of the whole desktop as a pixmap on a property on the
      saver window.  This code loads that pixmap, and crops or scales it as
      needed.  This means that the screenshot used will always be what the
      desktop looked like at the time that the screen blanked, not what it
      might look like now if the screen happened to be un-blanked.

   B: Under plain-old X11, if the pre-saved pixmap isn't there, then we do it
      the hard way: un-map our window to expose what is under it; wait an
      arbitrary amount of time for all other processes to re-paint their
      windows; copy a screen image; put our window back; and then return that
      image.  This method is slow and unreliable, as there is no way to know
      how long we have to wait for the re-paint, and if you don't wait long
      enough, you get all black.  E.g. on 2022 Raspbian 11.5/Pi4b/LXDE, it
      takes nearly *five seconds* for the frame buffer to update, which is
      truly awful.  Also, the unmapping and remapping causes ugly flicker,
      especially with the OpenGL hacks.  (Prior to XScreenSaver 6.06, we
      always did it this way.)

   C: On MacOS systems running X11 (which nobody does any more), rootless
      XQuartz doesn't let you make screenshots by copying the X11 root
      window, so this instead runs "/usr/sbin/screencapture" to get the
      Mac desktop image as a file.

   D: On Wayland: We try to use "/usr/bin/grim" to get the desktop image
      as a file.  That program is not installed by default on all Wayland
      systems; and it doesn't work under GNOME or KDE.

      "grim: compositor doesn't support wlr-screencopy-unstable-v1".

      There is no way for XScreenSaver to get a screen image under GNOME
      or KDE.  They invented their own, incompatible APIs, which always
      play a camera noise and flash the screen white, and might also
      pop up a confirmation dialog.

      ---------------------------------------------------------------------

      Further notes on the Wayland screen-grabbing situation...
      Which is absolute bugfuck insanity, I might add...

      Rather than exec'ing "grim", which may not be installed, we could
      instead connect to the Wayland server directly and do the thing that
      grim does, but that is an *enormous* amount of code, requiring four
      or five nonstandard "staging" protocols:

        - "ext-image-capture-source-v1" and its required companion
          "ext-image-copy-capture-v1" (and why they aren't the same thing,
          I could not tell you);
        - And "ext-foreign-toplevel-list-v1" which they also depend on;
        - And "zxdg-output-manager-v1", because without that, a "wl_output"
          will tell you its size but not its position. It's
          Heisenbergisterical!

      Once you've done that, you have to figure out what rectangle you want
      from the source outputs. Easy right? Except that they might be rotated,
      mirrored or scaled arbitrarily (e.g. "retina"), and you have to
      recapitulate that in the grabbed image. Look at the madness in grim's
      render.c.

      So pretty much the only part you can leave out is the "write a PNG
      file" bit. The rest of grim's ~1,800 lines of code is insanely
      complicated and sadly necessary.

      It would be easier for me to just include grim with XScreenSaver.
  

      Grim does not support GNOME or KDE, which provide four (4) different
      interfaces for us to also not support:

        - GNOME has a DBus endpoint, "org.gnome.Shell.Screenshot", but it's
          not clear to me whether it is possible to use it without an
          interactive dialog popping up, a shutter sound, and a screen flash:
          https://gitlab.gnome.org/GNOME/gnome-shell/-/blob/main/data/dbus-interfaces/org.gnome.Shell.Screenshot.xml

        - KDE also has a DBus endpoint, "org.kde.KWin.ScreenShot2", which
          says: "The application that requests the screenshot must have the
          org.kde.KWin.ScreenShot2 interface listed in the
          X-KDE-DBUS-Restricted-Interfaces desktop file entry." This may also
          do the mandatory "ask for permission, shutter sound, flash" thing
          that GNOME does, I can't tell:
          https://invent.kde.org/plasma/kwin/-/blob/master/src/plugins/screenshot/org.kde.KWin.ScreenShot2.xml

        - Supposedly KDE and GNOME also support the DBus endpoint
          "org.freedesktop.portal.Screenshot", but I imagine that's just a
          wrapper around their native versions with all the same downsides:
          https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Screenshot.html

        - Back in the Wayland world, there is "kde-zkde-screencast-unstable"
          https://wayland.app/protocols/kde-zkde-screencast-unstable-v1
          Does this one require the user to click OK first? Probably, but who
          can tell?

      More discussion and links in this thread:
      https://www.jwz.org/blog/2025/06/wayland-screenshots/#comment-260369
 */

#include "utils.h"

#include <X11/Intrinsic.h>	/* for XrmDatabase */
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>		/* for waitpid() and associated macros */
#endif

#ifdef HAVE_GETXATTR
# include <sys/xattr.h>
#endif

#include "version.h"
#include "../driver/blurb.h"
#include "yarandom.h"
#include "grabclient.h"
#include "screenshot.h"
#include "resources.h"
#include "colors.h"
#include "colorbars.h"
#include "visual.h"
#include "xmu.h"
#include "vroot.h"
#include "../driver/prefs.h"
#include "../driver/blurb.c"

#ifndef _XSCREENSAVER_VROOT_H_
# error Error!  You have an old version of vroot.h!  Check -I args.
#endif /* _XSCREENSAVER_VROOT_H_ */

#ifdef HAVE_GDK_PIXBUF
# undef HAVE_JPEGLIB

# if (__GNUC__ >= 4)	/* Ignore useless warnings generated by GTK headers */
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wlong-long"
#  pragma GCC diagnostic ignored "-Wvariadic-macros"
#  pragma GCC diagnostic ignored "-Wpedantic"
# endif

# include <gdk-pixbuf/gdk-pixbuf.h>

# ifdef HAVE_GDK_PIXBUF_XLIB
#  include <gdk-pixbuf-xlib/gdk-pixbuf-xlib.h>
# endif

# if (__GNUC__ >= 4)
#  pragma GCC diagnostic pop
# endif

#endif /* HAVE_GDK_PIXBUF */

#ifdef HAVE_JPEGLIB
# undef HAVE_GDK_PIXBUF
# include <jpeglib.h>
#endif


const char *progclass = "XScreenSaver";
XrmDatabase db;
XtAppContext app;

typedef enum {
  GRAB_DESK, GRAB_VIDEO, GRAB_FILE, GRAB_BARS
} grab_type;


#if defined(__APPLE__) && !defined(HAVE_COCOA)
# define HAVE_MACOS_X11
#elif !defined(HAVE_JWXYZ) /* Real X11, possibly Wayland */
# define HAVE_REAL_X11
#endif

#define GETIMAGE_VIDEO_PROGRAM "xscreensaver-getimage-video"
#define GETIMAGE_FILE_PROGRAM  "xscreensaver-getimage-file"

#ifdef HAVE_MACOS_X11
  /* On macOS under X11, the usual X11 mechanism of getting a screen shot
     doesn't work, and we need to use an external program.  This is only
     used when running under X11 on macOS.  If it's a Cocoa build, this
     path is not taken, and OSX/grabclient-osx.m is used instead.
   */
# define USE_EXTERNAL_SCREEN_GRABBER
# define GETIMAGE_SCREEN_PROGRAM "screencapture"

#elif defined(HAVE_REAL_X11)
  /* Under real X11, we can grab the screen directly.  Under XWayland,
     we call out to an external program. */
# define USE_EXTERNAL_SCREEN_GRABBER
# define GETIMAGE_SCREEN_PROGRAM "grim"
#endif


static int
x_ehandler (Display *dpy, XErrorEvent *error)
{
  if (error->error_code == BadWindow || error->error_code == BadDrawable)
    {
      fprintf (stderr, "%s: target %s 0x%lx unexpectedly deleted\n", blurb(),
               (error->error_code == BadWindow ? "window" : "pixmap"),
               (unsigned long) error->resourceid);
    }
  else
    {
      fprintf (stderr, "\nX error in %s:\n", blurb());
      XmuPrintDefaultErrorMessage (dpy, error, stderr);
    }
  exit (-1);
  return 0;
}


static Bool error_handler_hit_p = False;

static int
ignore_badwindow_ehandler (Display *dpy, XErrorEvent *error)
{
  error_handler_hit_p = True;
  if (error->error_code == BadWindow || error->error_code == BadDrawable)
    return 0;
  else
    return x_ehandler (dpy, error);
}


/* Whether the given window is:
   - the real root window;
   - a direct child of the root window;
   - a direct child of the window manager's decorations.
 */
static Bool
top_level_window_p (Screen *screen, Window window)
{
  Display *dpy = DisplayOfScreen (screen);
  Window root, parent, *kids;
  unsigned int nkids;

  if (!XQueryTree (dpy, window, &root, &parent, &kids, &nkids))
    return False;

  if (window == root)
    return True;

  /* If our direct parent is the real root window, then yes. */
  if (parent == root)
    return True;
  else
    {
      Atom type = None;
      int format;
      unsigned long nitems, bytesafter;
      unsigned char *data;

      /* If our direct parent has the WM_STATE property, then it is a
         window manager decoration -- yes.
      */
      if (XGetWindowProperty (dpy, window,
                              XInternAtom (dpy, "WM_STATE", True),
                              0, 0, False, AnyPropertyType,
                              &type, &format, &nitems, &bytesafter,
                              (unsigned char **) &data)
          == Success
          && type != None)
        return True;
    }

  /* Else, no.  We're deep in a tree somewhere.
   */
  return False;
}


static Bool
xscreensaver_window_p (Display *dpy, Window window)
{
  XClassHint h;
  Bool ret = False;
  /* Prior to 6.x, XScreenSaver windows were detectable with the
     XA_SCREENSAVER_VERSION property, but now we just use the WM
     class hint. */
  if (XGetClassHint (dpy, window, &h))
    {
      if (h.res_class && !strcmp (h.res_class, "XScreenSaver"))
        ret = True;
      if (h.res_name) XFree (h.res_name);
      if (h.res_class) XFree (h.res_class);
    }
  return ret;
}


/* Figure out what kind of scaling/positioning we ought to do to display
   a src-sized image in a dest-sized window/pixmap.  Returns the width
   and height to which the image should be scaled, and the position where
   it should be displayed to center it.  The result may be letterboxed
   or pillarboxed to center it, as well as being scaled.
 */
static void
compute_image_scaling (int src_w, int src_h,
                       int dest_w, int dest_h,
                       Bool verbose_p,
                       int *scaled_from_x_ret, int *scaled_from_y_ret,
                       int *scaled_to_x_ret, int *scaled_to_y_ret,
                       int *scaled_w_ret, int *scaled_h_ret)
{
  int srcx, srcy, destx, desty;

  Bool exact_fit_p = ((src_w == dest_w && src_h <= dest_h) ||
                      (src_h == dest_h && src_w <= dest_w));

  if (!exact_fit_p)  /* scale the image up or down */
    {
      float rw = (float) dest_w  / src_w;
      float rh = (float) dest_h / src_h;
      float r = (rw < rh ? rw : rh);
      int tw, th, pct;

      /* If the window is a goofy aspect ratio, take a middle slice of
         the image instead. */
      if (dest_w > dest_h * 5 || dest_h > dest_w * 5)
        {
          double r2 = (dest_w > dest_h
                       ? dest_w / (double) dest_h
                       : dest_h / (double) dest_w);
          r *= r2;
          if (verbose_p)
            fprintf (stderr, "%s: weird aspect: scaling by %.1f\n",
                     blurb(), r2);
        }

      tw = src_w * r;
      th = src_h * r;
      pct = (r * 100);

#if 0
      /* this optimization breaks things */
      if (pct < 95 || pct > 105)  /* don't scale if it's close */
#endif
        {
          if (verbose_p)
            fprintf (stderr, "%s: scaling image by %d%% (%dx%d -> %dx%d)\n",
                     blurb(), pct, src_w, src_h, tw, th);
          src_w = tw;
          src_h = th;
        }
    }

  /* Center the image on the window/pixmap. */
  srcx = 0;
  srcy = 0;
  destx = (dest_w - src_w) / 2;
  desty = (dest_h - src_h) / 2;
  if (destx < 0) srcx = -destx, destx = 0;
  if (desty < 0) srcy = -desty, desty = 0;

  /* if (dest_w < src_w) src_w = dest_w;
     if (dest_h < src_h) src_h = dest_h; */

  *scaled_w_ret = src_w;
  *scaled_h_ret = src_h;
  *scaled_from_x_ret = srcx;
  *scaled_from_y_ret = srcy;
  *scaled_to_x_ret = destx;
  *scaled_to_y_ret = desty;

  if (verbose_p)
    fprintf (stderr, "%s: displaying %dx%d+%d+%d at %dx%d+%d+%d\n",
             blurb(), src_w, src_h, srcx, srcy, dest_w, dest_h, destx, desty);
}


static void
colorbars (Screen *screen, Visual *visual, Drawable drawable, Colormap cmap,
           int logo_size)
{
  Pixmap mask = 0;
  unsigned long *pixels; /* ignored - unfreed */
  int npixels;
  Pixmap logo = xscreensaver_logo (screen, visual, drawable, cmap,
                                   BlackPixelOfScreen (screen),
                                   &pixels, &npixels, &mask,
                                   logo_size);
  draw_colorbars (screen, visual, drawable, cmap, 0, 0, 0, 0, logo, mask);
  XFreePixmap (DisplayOfScreen (screen), logo);
  XFreePixmap (DisplayOfScreen (screen), mask);
}


/* Scales an XImage, modifying it in place.
   This doesn't do dithering or smoothing, so it might have artifacts.
   If out of memory, returns False, and the XImage will have been
   destroyed and freed.
   #### Maybe convert the XImage to a GdkImage and use GDK's scaler.
 */
static Bool
scale_ximage (Screen *screen, Visual *visual,
              XImage *ximage, int new_width, int new_height)
{
  Display *dpy = DisplayOfScreen (screen);
  int depth = visual_depth (screen, visual);
  int x, y;
  double xscale, yscale;

  XImage *ximage2 = XCreateImage (dpy, visual, depth,
                                  ZPixmap, 0, 0,
                                  new_width, new_height, 8, 0);
  ximage2->data = (char *) calloc (ximage2->height, ximage2->bytes_per_line);

  if (!ximage2->data)
    {
      fprintf (stderr, "%s: out of memory scaling %dx%d image to %dx%d\n",
               blurb(),
               ximage->width, ximage->height,
               ximage2->width, ximage2->height);
      if (ximage->data) free (ximage->data);
      if (ximage2->data) free (ximage2->data);
      ximage->data = 0;
      ximage2->data = 0;
      XDestroyImage (ximage);
      XDestroyImage (ximage2);
      return False;
    }

  /* Brute force scaling... */
  xscale = (double) ximage->width  / ximage2->width;
  yscale = (double) ximage->height / ximage2->height;
  for (y = 0; y < ximage2->height; y++)
    for (x = 0; x < ximage2->width; x++)
      XPutPixel (ximage2, x, y,
                 XGetPixel (ximage, x * xscale, y * yscale));

  free (ximage->data);
  ximage->data = 0;

  (*ximage) = (*ximage2);

  ximage2->data = 0;
  XDestroyImage (ximage2);

  return True;
}


#ifdef HAVE_GDK_PIXBUF

/* Reads the given image file and renders it on the Drawable, using GDK.
   Returns False if it fails.
 */
static Bool
read_file_gdk (Screen *screen, Window window, Drawable drawable,
               const char *filename, Bool verbose_p,
               XRectangle *geom_ret)
{
  GdkPixbuf *pb;
  Display *dpy = DisplayOfScreen (screen);
  unsigned int win_width, win_height, win_depth;
  XGCValues gcv;
  GC gc;
  GError *gerr = 0;

  /* Find the size of the Drawable. */
  {
    Window root;
    int x, y;
    unsigned int bw;
    XGetGeometry (dpy, drawable,
                  &root, &x, &y, &win_width, &win_height, &bw, &win_depth);
  }

# ifdef HAVE_GDK_PIXBUF_XLIB
  /* Aug 2022: nothing seems to go wrong if we don't do this at all?
     gtk-2.24.33, gdk-pixbuf 2.42.8. */
  gdk_pixbuf_xlib_init_with_depth (dpy, screen_number (screen), win_depth);
  xlib_rgb_init (dpy, screen_number (screen));
# endif

# if !GLIB_CHECK_VERSION(2, 36 ,0)
  g_type_init();
# endif

  pb = gdk_pixbuf_new_from_file (filename, &gerr);

  if (!pb)
    {
      fprintf (stderr, "%s: unable to load \"%s\"\n", blurb(), filename);
      if (gerr && gerr->message && *gerr->message)
        fprintf (stderr, "%s: reason: %s\n", blurb(), gerr->message);
      return False;
    }
  else
    {
      int w = gdk_pixbuf_get_width (pb);
      int h = gdk_pixbuf_get_height (pb);
      int srcx, srcy, destx, desty, w2, h2;

# ifdef HAVE_GDK_PIXBUF_APPLY_EMBEDDED_ORIENTATION
      {
        int ow = w, oh = h;
        GdkPixbuf *opb = pb;
        pb = gdk_pixbuf_apply_embedded_orientation (opb);
        g_object_unref (opb);
        w = gdk_pixbuf_get_width (pb);
        h = gdk_pixbuf_get_height (pb);
        if (verbose_p && (w != ow || h != oh))
          fprintf (stderr, "%s: rotated %dx%d to %dx%d\n",
                   blurb(), ow, oh, w, h);
      }
# endif

      compute_image_scaling (w, h, win_width, win_height, verbose_p,
                             &srcx, &srcy, &destx, &desty, &w2, &h2);
      if (w != w2 || h != h2)
        {
          GdkPixbuf *pb2 = gdk_pixbuf_scale_simple (pb, w2, h2,
                                                    GDK_INTERP_BILINEAR);
          if (pb2)
            {
              g_object_unref (pb);
              pb = pb2;
              w = w2;
              h = h2;
            }
          else
            fprintf (stderr, "%s: out of memory when scaling?\n", blurb());
        }

      if (srcx > 0) w -= srcx;
      if (srcy > 0) h -= srcy;

      gcv.foreground = BlackPixelOfScreen (screen);
      gc = XCreateGC (dpy, drawable, GCForeground, &gcv);

# ifdef HAVE_GDK_PIXBUF_XLIB
      if (w != win_width || h != win_height)
        XFillRectangle (dpy, drawable, gc, 0, 0, win_width, win_height);

      /* Note that this always uses the default colormap.  Oh my, so sad. */
      gdk_pixbuf_xlib_render_to_drawable_alpha (pb, drawable,
                                                srcx, srcy, destx, desty,
                                                w, h,
                                                GDK_PIXBUF_ALPHA_FULL, 127,
                                                XLIB_RGB_DITHER_NORMAL,
                                                0, 0);
# else /* !HAVE_GDK_PIXBUF_XLIB */
      {
        /* Get the bits from GDK and render them out by hand.
           This only handles 24 or 32-bit RGB TrueColor visuals.
           Suck it, PseudoColor!
         */
        XWindowAttributes xgwa;
        int w = gdk_pixbuf_get_width (pb);
        int h = gdk_pixbuf_get_height (pb);
        guchar *row = gdk_pixbuf_get_pixels (pb);
        int stride = gdk_pixbuf_get_rowstride (pb);
        int chan = gdk_pixbuf_get_n_channels (pb);
        int x, y;
        XImage *image;

        XGetWindowAttributes (dpy, window, &xgwa);
        image = XCreateImage (dpy, xgwa.visual, xgwa.depth, ZPixmap,
                              0, 0, w, h, 8, 0);
        image->data = (char *) malloc (h * image->bytes_per_line);
        gc = XCreateGC (dpy, drawable, 0, &gcv);

        if (!image->data)
          {
            fprintf (stderr, "%s: out of memory (%d x %d)\n", blurb(), w, h);
            return False;
          }

        for (y = 0; y < h; y++)
          {
            guchar *i = row;
            for (x = 0; x < w; x++)
              {
                unsigned long rgba = 0;
                switch (chan) {
                case 1:
                  rgba = ((*i << 16) |
                          (*i <<  8) |
                          (*i <<  0));
                  break;
                case 3:
                case 4:
                  rgba = ((i[0] << 16) |
                          (i[1] <<  8) |
                          (i[2] <<  0));
                  break;
                default:
                  abort();
                  break;
                }
                i += chan;
                XPutPixel (image, x, y, rgba);
              }
            row += stride;
          }

        /* Do this as late as possible to minimize flicker */
        if (w != win_width || h != win_height)
          XFillRectangle (dpy, drawable, gc, 0, 0, win_width, win_height);

        XPutImage (dpy, drawable, gc, image, srcx, srcy, destx, desty, w, h);
        XDestroyImage (image);
      }
# endif /* !HAVE_GDK_PIXBUF_XLIB */

      XFreeGC (dpy, gc);

      if (geom_ret)
        {
          geom_ret->x = destx;
          geom_ret->y = desty;
          geom_ret->width  = w;
          geom_ret->height = h;
        }
    }

  return True;
}

#endif /* HAVE_GDK_PIXBUF */



#ifdef HAVE_JPEGLIB

/* Allocates a colormap that makes a PseudoColor or DirectColor
   visual behave like a TrueColor visual of the same depth.
 */
static void
allocate_cubic_colormap (Screen *screen, Visual *visual, Colormap cmap,
                         Bool verbose_p)
{
  Display *dpy = DisplayOfScreen (screen);
  int nr, ng, nb, cells;
  int r, g, b;
  int depth;
  XColor colors[4097];
  int i;

  depth = visual_depth (screen, visual);

  switch (depth)
    {
    case 8:  nr = 3; ng = 3; nb = 2; cells = 256;  break;
    case 12: nr = 4; ng = 4; nb = 4; cells = 4096; break;
    default: abort(); break;
    }

  memset(colors, 0, sizeof(colors));
  for (r = 0; r < (1 << nr); r++)
    for (g = 0; g < (1 << ng); g++)
      for (b = 0; b < (1 << nb); b++)
	{
	  i = (r | (g << nr) | (b << (nr + ng)));
	  colors[i].pixel = i;
          colors[i].flags = DoRed|DoGreen|DoBlue;
	  if (depth == 8)
	    {
	      colors[i].red   = ((r << 13) | (r << 10) | (r << 7) |
				 (r <<  4) | (r <<  1));
	      colors[i].green = ((g << 13) | (g << 10) | (g << 7) |
				 (g <<  4) | (g <<  1));
	      colors[i].blue  = ((b << 14) | (b << 12) | (b << 10) |
				 (b <<  8) | (b <<  6) | (b <<  4) |
				 (b <<  2) | b);
	    }
	  else
	    {
	      colors[i].red   = (r << 12) | (r << 8) | (r << 4) | r;
	      colors[i].green = (g << 12) | (g << 8) | (g << 4) | g;
	      colors[i].blue  = (b << 12) | (b << 8) | (b << 4) | b;
	    }
	}

  {
    int j;
    int allocated = 0;
    int interleave = cells / 8;  /* skip around, rather than allocating in
                                    order, so that we get better coverage if
                                    we can't allocated all of them. */
    for (j = 0; j < interleave; j++)
      for (i = 0; i < cells; i += interleave)
        if (XAllocColor (dpy, cmap, &colors[i + j]))
          allocated++;

    if (verbose_p)
      fprintf (stderr, "%s: allocated %d of %d colors for cubic map\n",
               blurb(), allocated, cells);
  }
}

/* Find the pixel index that is closest to the given color
   (using linear distance in RGB space -- which is far from the best way.)
 */
static unsigned long
find_closest_pixel (XColor *colors, int ncolors,
                    unsigned long r, unsigned long g, unsigned long b)
{
  unsigned long distance = ~0;
  int i, found = 0;

  if (ncolors == 0)
    abort();
  for (i = 0; i < ncolors; i++)
    {
      unsigned long d;
      int rd, gd, bd;

      rd = r - colors[i].red;
      gd = g - colors[i].green;
      bd = b - colors[i].blue;
      if (rd < 0) rd = -rd;
      if (gd < 0) gd = -gd;
      if (bd < 0) bd = -bd;
      d = (rd << 1) + (gd << 2) + bd;
      
      if (d < distance)
	{
	  distance = d;
	  found = i;
          if (distance == 0)
              break;
	}
    }

  return found;
}


/* Given an XImage with 8-bit or 12-bit RGB data, convert it to be 
   displayable with the given X colormap.  The farther from a perfect
   color cube the contents of the colormap are, the lossier the 
   transformation will be.  No dithering is done.
 */
static void
remap_image (Screen *screen, Colormap cmap, XImage *image, Bool verbose_p)
{
  Display *dpy = DisplayOfScreen (screen);
  unsigned long map[4097];
  int x, y, i;
  int cells;
  XColor colors[4097];

  if (image->depth == 8)
    cells = 256;
  else if (image->depth == 12)
    cells = 4096;
  else
    abort();

  memset(map,    -1, sizeof(*map));
  memset(colors, -1, sizeof(*colors));

  for (i = 0; i < cells; i++)
    colors[i].pixel = i;
  XQueryColors (dpy, cmap, colors, cells);

  if (verbose_p)
    fprintf(stderr, "%s: building color cube for %d bit image\n",
            blurb(), image->depth);

  for (i = 0; i < cells; i++)
    {
      unsigned short r, g, b;

      if (cells == 256)
        {
          /* "RRR GGG BB" In an 8 bit map.  Convert that to
             "RRR RRR RR" "GGG GGG GG" "BB BB BB BB" to give
             an even spread. */
          r = (i & 0x07);
          g = (i & 0x38) >> 3;
          b = (i & 0xC0) >> 6;

          r = ((r << 13) | (r << 10) | (r << 7) | (r <<  4) | (r <<  1));
          g = ((g << 13) | (g << 10) | (g << 7) | (g <<  4) | (g <<  1));
          b = ((b << 14) | (b << 12) | (b << 10) | (b <<  8) |
               (b <<  6) | (b <<  4) | (b <<  2) | b);
        }
      else
        {
          /* "RRRR GGGG BBBB" In a 12 bit map.  Convert that to
             "RRRR RRRR" "GGGG GGGG" "BBBB BBBB" to give an even
             spread. */
          r = (i & 0x00F);
          g = (i & 0x0F0) >> 4;
          b = (i & 0xF00) >> 8;

          r = (r << 12) | (r << 8) | (r << 4) | r;
          g = (g << 12) | (g << 8) | (g << 4) | g;
          b = (b << 12) | (b << 8) | (b << 4) | b;
        }

      map[i] = find_closest_pixel (colors, cells, r, g, b);
    }

  if (verbose_p)
    fprintf(stderr, "%s: remapping colors in %d bit image\n",
            blurb(), image->depth);

  for (y = 0; y < image->height; y++)
    for (x = 0; x < image->width; x++)
      {
        unsigned long pixel = XGetPixel(image, x, y);
        if (pixel >= cells) abort();
        XPutPixel(image, x, y, map[pixel]);
      }
}


typedef struct {
  struct jpeg_error_mgr pub;   /* this is what passes for subclassing in C */
  const char *filename;
  Screen *screen;
  Visual *visual;
  Drawable drawable;
  Colormap cmap;
} getimg_jpg_error_mgr;


static void
jpg_output_message (j_common_ptr cinfo)
{
  getimg_jpg_error_mgr *err = (getimg_jpg_error_mgr *) cinfo->err;
  char buf[JMSG_LENGTH_MAX];
  cinfo->err->format_message (cinfo, buf);
  fprintf (stderr, "%s: %s: %s\n", blurb(), err->filename, buf);
}


static void
jpg_error_exit (j_common_ptr cinfo)
{
  getimg_jpg_error_mgr *err = (getimg_jpg_error_mgr *) cinfo->err;
  cinfo->err->output_message (cinfo);
  colorbars (err->screen, err->visual, err->drawable, err->cmap, 1);
  XSync (DisplayOfScreen (err->screen), False);
  exit (1);
}


/* Reads a JPEG file, returns an RGB XImage of it.
 */
static XImage *
read_jpeg_ximage (Screen *screen, Visual *visual, Drawable drawable,
                  Colormap cmap, const char *filename, Bool verbose_p)
{
  Display *dpy = DisplayOfScreen (screen);
  int depth = visual_depth (screen, visual);

  FILE *in = 0;
  XImage *ximage = 0;
  struct jpeg_decompress_struct cinfo;
  getimg_jpg_error_mgr jerr;
  JSAMPARRAY scanbuf = 0;
  int y;

  jerr.filename = filename;
  jerr.screen = screen;
  jerr.visual = visual;
  jerr.drawable = drawable;
  jerr.cmap = cmap;

  if (! (depth >= 15 || depth == 12 || depth == 8))
    {
      fprintf (stderr, "%s: unsupported depth: %d\n", blurb(), depth);
      goto FAIL;
    }

  in = fopen (filename, "rb");
  if (!in)
    {
      fprintf (stderr, "%s: %s: unreadable\n", blurb(), filename);
      goto FAIL;
    }

  cinfo.err = jpeg_std_error (&jerr.pub);
  jerr.pub.output_message = jpg_output_message;
  jerr.pub.error_exit = jpg_error_exit;

  jpeg_create_decompress (&cinfo);
  jpeg_stdio_src (&cinfo, in);
  jpeg_read_header (&cinfo, TRUE);

  /* set some decode parameters */
  cinfo.out_color_space = JCS_RGB;
  cinfo.quantize_colors = FALSE;

  jpeg_start_decompress (&cinfo);

  ximage = XCreateImage (dpy, visual, depth, ZPixmap, 0, 0,
                         cinfo.output_width, cinfo.output_height,
                         8, 0);
  if (ximage)
    ximage->data = (char *) calloc (ximage->height, ximage->bytes_per_line);

  if (ximage && ximage->data)
    scanbuf = (*cinfo.mem->alloc_sarray) ((j_common_ptr) &cinfo, JPOOL_IMAGE,
                                          cinfo.rec_outbuf_height *
                                          cinfo.output_width *
                                          cinfo.output_components,
                                          1);
  if (!ximage || !ximage->data || !scanbuf)
    {
      fprintf (stderr, "%s: out of memory loading %dx%d file %s\n",
               blurb(), ximage->width, ximage->height, filename);
      goto FAIL;
    }

  y = 0;
  while (cinfo.output_scanline < cinfo.output_height)
    {
      int n = jpeg_read_scanlines (&cinfo, scanbuf, 1);
      int i;
      for (i = 0; i < n; i++)
        {
          int x;
          for (x = 0; x < ximage->width; x++)
            {
              int j = x * cinfo.output_components;
              unsigned char r = scanbuf[i][j];
              unsigned char g = scanbuf[i][j+1];
              unsigned char b = scanbuf[i][j+2];
              unsigned long pixel;

              if (depth > 16)
                pixel = (r << 16) | (g << 8) | b;
              else if (depth == 8)
                pixel = ((r >> 5) | ((g >> 5) << 3) | ((b >> 6) << 6));
              else if (depth == 12)
                pixel = ((r >> 4) | ((g >> 4) << 4) | ((b >> 4) << 8));
              else if (depth == 15)
                /* Gah! I don't understand why these are in the other
                   order. */
                pixel = (((r >> 3) << 10) | ((g >> 3) << 5) | ((b >> 3)));
              else if (depth == 16)
                pixel = (((r >> 3) << 11) | ((g >> 2) << 5) | ((b >> 3)));
              else
                abort();

              XPutPixel (ximage, x, y, pixel);
            }
          y++;
        }
    }

  if (cinfo.output_scanline < cinfo.output_height)
    /* don't goto FAIL -- we might have viewable partial data. */
    jpeg_abort_decompress (&cinfo);
  else
    jpeg_finish_decompress (&cinfo);

  jpeg_destroy_decompress (&cinfo);
  fclose (in);
  in = 0;

  return ximage;

 FAIL:
  if (in) fclose (in);
  if (ximage && ximage->data)
    {
      free (ximage->data);
      ximage->data = 0;
    }
  if (ximage) XDestroyImage (ximage);
  if (scanbuf) free (scanbuf);
  return 0;
}


/* Reads the given image file and renders it on the Drawable, using JPEG lib.
   Returns False if it fails.
 */
static Bool
read_file_jpeglib (Screen *screen, Window window, Drawable drawable,
                   const char *filename, Bool verbose_p,
                   XRectangle *geom_ret)
{
  Display *dpy = DisplayOfScreen (screen);
  XImage *ximage;
  Visual *visual;
  int class, depth;
  Colormap cmap;
  unsigned int win_width, win_height, win_depth;
  int srcx, srcy, destx, desty, w2, h2;
  GC gc;
  XGCValues gcv;

  /* Find the size of the Drawable, and the Visual/Colormap of the Window. */
  {
    Window root;
    int x, y;
    unsigned int bw;
    XWindowAttributes xgwa;
    XGetWindowAttributes (dpy, window, &xgwa);
    visual = xgwa.visual;
    cmap = xgwa.colormap;
    XGetGeometry (dpy, drawable,
                  &root, &x, &y, &win_width, &win_height, &bw, &win_depth);
  }

  /* Make sure we're not on some weirdo visual...
   */
  class = visual_class (screen, visual);
  depth = visual_depth (screen, visual);
  if ((class == PseudoColor || class == DirectColor) &&
      (depth != 8 && depth != 12))
    {
      fprintf (stderr, "%s: Pseudo/DirectColor depth %d unsupported\n",
               blurb(), depth);
      return False;
    }

  /* Read the file...
   */
  ximage = read_jpeg_ximage (screen, visual, drawable, cmap,
                             filename, verbose_p);
  if (!ximage) return False;

  /* Scale it, if necessary...
   */
  compute_image_scaling (ximage->width, ximage->height,
                         win_width, win_height, verbose_p,
                         &srcx, &srcy, &destx, &desty, &w2, &h2);
  if (ximage->width != w2 || ximage->height != h2)
    if (! scale_ximage (screen, visual, ximage, w2, h2))
      return False;

  /* Allocate a colormap, if we need to...
   */
  if (class == PseudoColor || class == DirectColor)
    {
      allocate_cubic_colormap (screen, visual, cmap, verbose_p);
      remap_image (screen, cmap, ximage, verbose_p);
    }

  /* Finally, put the resized image on the drawable.
   */
  gcv.foreground = BlackPixelOfScreen (screen);
  gc = XCreateGC (dpy, drawable, GCForeground, &gcv);
  if (ximage->width != win_width || ximage->height != win_height)
    XFillRectangle (dpy, drawable, gc, 0, 0, win_width, win_height);
  XPutImage (dpy, drawable, gc, ximage,
             srcx, srcy, destx, desty, ximage->width, ximage->height);
  XFreeGC (dpy, gc);

  if (geom_ret)
    {
      geom_ret->x = destx;
      geom_ret->y = desty;
      geom_ret->width  = ximage->width;
      geom_ret->height = ximage->height;
    }

  free (ximage->data);
  ximage->data = 0;
  XDestroyImage (ximage);
  return True;
}

#endif /* HAVE_JPEGLIB */


/* Reads the given image file and renders it on the Drawable.
   Returns False if it fails.
 */
static Bool
display_file (Screen *screen, Window window, Drawable drawable,
              const char *filename, Bool verbose_p,
              XRectangle *geom_ret)
{
  if (verbose_p)
    fprintf (stderr, "%s: loading \"%s\"\n", blurb(), filename);

# if defined(HAVE_GDK_PIXBUF)
  if (read_file_gdk (screen, window, drawable, filename, verbose_p, geom_ret))
    return True;
# elif defined(HAVE_JPEGLIB)
  if (read_file_jpeglib (screen, window, drawable, filename, verbose_p,
                         geom_ret))
    return True;
# else  /* !(HAVE_GDK_PIXBUF || HAVE_JPEGLIB) */
  /* shouldn't get here if we have no image-loading methods available. */
  abort();
# endif /* !(HAVE_GDK_PIXBUF || HAVE_JPEGLIB) */

  return False;
}


/* Invokes a sub-process and returns its output (presumably, a file to
   load.)  Free the string when done.  'grab_type' controls which program
   to run.  Returned pathname may be relative to 'directory', or absolute.
 */
static char *
get_filename_1 (Screen *screen, Window window,
                const char *directory, grab_type type,
                Bool verbose_p)
{
  Display *dpy = DisplayOfScreen (screen);
  pid_t forked;
  int fds [2];
  int in, out;
  char buf[1024];
  char *av[20];
  int ac = 0;
  char *outfile = 0;

  switch (type)
    {
    case GRAB_FILE:
      av[ac++] = GETIMAGE_FILE_PROGRAM;
      if (verbose_p)
        av[ac++] = "--verbose";
      av[ac++] = "--name";
      av[ac++] = (char *) directory;
      break;

    case GRAB_VIDEO:
      av[ac++] = GETIMAGE_VIDEO_PROGRAM;
      if (verbose_p)
        av[ac++] = "--verbose";
      av[ac++] = "--name";
      break;

# ifdef USE_EXTERNAL_SCREEN_GRABBER
    case GRAB_DESK:
      {
	XWindowAttributes xgwa;
        const char *tmpdir = getenv("TMPDIR");
        static char rect[100];
        if (!tmpdir) tmpdir = "/tmp";

	XGetWindowAttributes (dpy, window, &xgwa);
        window_root_offset (dpy, window, &xgwa.x, &xgwa.y);

        outfile = (char *) malloc (strlen(tmpdir) + 100);
        sprintf (outfile, "%s/xscreensaver.%08x.png",
                 tmpdir, random() % 0xFFFFFFFF);

#if  defined(HAVE_MACOS_X11)

        sprintf (rect, "%d,%d,%d,%d", xgwa.x, xgwa.y, xgwa.width, xgwa.height);

        av[ac++] = GETIMAGE_SCREEN_PROGRAM;	/* "screencapture" */
        av[ac++] = "-x";			/* no sound */
        av[ac++] = "-R";			/* rect */
        av[ac++] = rect;
        av[ac++] = "-t";			/* file type */
        av[ac++] = "png";
        av[ac++] = outfile;

#  elif defined(HAVE_REAL_X11)

        sprintf (rect, "%d,%d %dx%d", xgwa.x, xgwa.y, xgwa.width, xgwa.height);

        av[ac++] = GETIMAGE_SCREEN_PROGRAM;	/* "grim" */
        av[ac++] = "-g";			/* geometry */
        av[ac++] = rect;
        av[ac++] = "-t";			/* file type */
        av[ac++] = "png";
        av[ac++] = outfile;
#  else
#   error USE_EXTERNAL_SCREEN_GRABBER is wrong
#  endif
      }
      break;
# endif /* USE_EXTERNAL_SCREEN_GRABBER */

    default:
      abort();
    }
  av[ac] = 0;

  if (verbose_p)
    {
      int i;
      fprintf (stderr, "%s: executing:", blurb());
      for (i = 0; i < ac; i++)
        fprintf (stderr, " %s", av[i]);
      fprintf (stderr, "\n");
    }

  if (pipe (fds))
    {
      sprintf (buf, "%s: error creating pipe", blurb());
      perror (buf);
      return 0;
    }

  in = fds [0];
  out = fds [1];

  switch ((int) (forked = fork ()))
    {
    case -1:
      {
        sprintf (buf, "%s: couldn't fork", blurb());
        perror (buf);
        if (outfile) free (outfile);
        return 0;
      }
    case 0:
      {
        int stdout_fd = 1;

        close (in);  /* don't need this one */
        close (ConnectionNumber (dpy));		/* close display fd */

        if (dup2 (out, stdout_fd) < 0)		/* pipe stdout */
          {
            sprintf (buf, "%s: could not dup() a new stdout", blurb());
            exit (-1);                          /* exits fork */
          }

        execvp (av[0], av);			/* shouldn't return. */
        exit (-1);                              /* exits fork */
        break;
      }
    default:
      {
        struct stat st;
        int wait_status = 0;
        FILE *f = fdopen (in, "r");
        int L;
        char *outfile_full;
        int i = 0;

        close (out);  /* don't need this one */

        *buf = 0;
        do {
          errno = 0;
          if (! fgets (buf, sizeof(buf)-1, f))
            *buf = 0;
        } while (errno == EINTR &&	/* fgets might fail due to SIGCHLD. */
                 i++ < 1000);		/* And just in case. */

        fclose (f);

        /* Wait for the child to die. */
        waitpid (forked, &wait_status, 0);

        L = strlen (buf);
        while (L && buf[L-1] == '\n')
          buf[--L] = 0;

        /* In GRAB_DESK mode, outfile is already set.
           Otherwise, is must be printed by the subprocess. */
        if (! outfile)
          {
            if (!*buf) return 0;
            outfile = strdup (buf);
          }

        outfile_full = outfile;
        if (*outfile != '/')
          {
            /* Program returned path relative to directory.  Prepend dir
               to buf so that we can properly stat it. */
            outfile_full = (char *)
              malloc (strlen(buf) + strlen(directory) + 20);
            strcpy (outfile_full, directory);
            if (directory[strlen(directory)-1] != '/')
              strcat (outfile_full, "/");
            strcat (outfile_full, outfile);
          }

        if (stat (outfile_full, &st))
          {
            if (verbose_p)
              fprintf (stderr, "%s: file does not exist: \"%s\"\n",
                       blurb(), outfile_full);
            if (outfile_full != outfile)
              free (outfile);
            outfile = 0;
          }

        if (outfile_full && outfile_full != outfile)
          free (outfile_full);
        return outfile;
      }
    }

  abort();
}


/* Returns a pathname to an image file.  Free the string when you're done.
 */
static char *
get_filename (Screen *screen, Window window, const char *directory,
              Bool verbose_p)
{
  return get_filename_1 (screen, window, directory, GRAB_FILE, verbose_p);
}


/* Grabs a video frame to a file, and returns a pathname to that file.
   Delete that file when you are done with it (and free the string.)
 */
static char *
get_video_filename (Screen *screen, Window window, Bool verbose_p)
{
  return get_filename_1 (screen, window, 0, GRAB_VIDEO, verbose_p);
}

/* Grabs a screenshot to a file, and returns a pathname to that file.
   It will be the size of the provided window.
   Delete that file when you are done with it (and free the string.)
 */
# ifdef USE_EXTERNAL_SCREEN_GRABBER
static char *
get_desktop_filename (Screen *screen, Window window, Bool verbose_p)
{
  return get_filename_1 (screen, window, 0, GRAB_DESK, verbose_p);
}
#endif /* USE_EXTERNAL_SCREEN_GRABBER */


/* Grabs a video frame, and renders it on the Drawable.
   Returns False if it fails.
 */
static Bool
display_video (Screen *screen, Window window, Drawable drawable,
               Bool verbose_p, XRectangle *geom_ret)
{
  char *filename = get_video_filename (screen, window, verbose_p);
  Bool status;

  if (!filename)
    {
      if (verbose_p)
        fprintf (stderr, "%s: video grab failed\n", blurb());
      return False;
    }

  status = display_file (screen, window, drawable, filename, verbose_p,
                         geom_ret);

  if (unlink (filename))
    {
      char buf[512];
      sprintf (buf, "%s: rm %.100s", blurb(), filename);
      perror (buf);
    }
  else if (verbose_p)
    fprintf (stderr, "%s: rm %s\n", blurb(), filename);

  if (filename) free (filename);
  return status;
}


/* When we are grabbing and manipulating a screen image, it's important that
   we use the same colormap it originally had.  So, if the screensaver was
   started with -install, we need to copy the contents of the default colormap
   into the screensaver's colormap.
 */
# ifndef HAVE_MACOS_X11
static void
copy_default_colormap_contents (Screen *screen,
				Colormap to_cmap,
				Visual *to_visual,
                                Bool verbose_p)
{
  Display *dpy = DisplayOfScreen (screen);
  Visual *from_visual = DefaultVisualOfScreen (screen);
  Colormap from_cmap = XDefaultColormapOfScreen (screen);

  XColor *old_colors, *new_colors;
  unsigned long *pixels;
  XVisualInfo vi_in, *vi_out;
  int out_count;
  int from_cells, to_cells, max_cells, got_cells;
  int i;

  if (from_cmap == to_cmap)
    return;

  vi_in.screen = XScreenNumberOfScreen (screen);
  vi_in.visualid = XVisualIDFromVisual (from_visual);
  vi_out = XGetVisualInfo (dpy, VisualScreenMask|VisualIDMask,
			   &vi_in, &out_count);
  if (! vi_out) abort ();
  from_cells = vi_out [0].colormap_size;
  XFree ((char *) vi_out);

  vi_in.screen = XScreenNumberOfScreen (screen);
  vi_in.visualid = XVisualIDFromVisual (to_visual);
  vi_out = XGetVisualInfo (dpy, VisualScreenMask|VisualIDMask,
			   &vi_in, &out_count);
  if (! vi_out) abort ();
  to_cells = vi_out [0].colormap_size;
  XFree ((char *) vi_out);

  max_cells = (from_cells > to_cells ? to_cells : from_cells);

  old_colors = (XColor *) calloc (sizeof (XColor), max_cells);
  new_colors = (XColor *) calloc (sizeof (XColor), max_cells);
  pixels = (unsigned long *) calloc (sizeof (unsigned long), max_cells);
  for (i = 0; i < max_cells; i++)
    old_colors[i].pixel = i;
  XQueryColors (dpy, from_cmap, old_colors, max_cells);

  got_cells = max_cells;
  allocate_writable_colors (screen, to_cmap, pixels, &got_cells);

  if (verbose_p && got_cells != max_cells)
    fprintf(stderr, "%s: got only %d of %d cells\n", blurb(),
	    got_cells, max_cells);

  if (got_cells <= 0)					 /* we're screwed */
    ;
  else if (got_cells == max_cells &&			 /* we're golden */
	   from_cells == to_cells)
    XStoreColors (dpy, to_cmap, old_colors, got_cells);
  else							 /* try to cope... */
    {
      for (i = 0; i < got_cells; i++)
	{
	  XColor *c = old_colors + i;
	  int j;
	  for (j = 0; j < got_cells; j++)
	    if (pixels[j] == c->pixel)
	      {
		/* only store this color value if this is one of the pixels
		   we were able to allocate. */
		XStoreColors (dpy, to_cmap, c, 1);
		break;
	      }
	}
    }


  if (verbose_p)
    fprintf(stderr, "%s: installing copy of default colormap\n", blurb());

  free (old_colors);
  free (new_colors);
  free (pixels);
}
#endif /* !HAVE_MACOS_X11 */


/* Install the colormaps of all visible windows, deepest first.
   This should leave the colormaps of the topmost windows installed
   (if only N colormaps can be installed at a time, then only the
   topmost N windows will be shown in the right colors.)
 */
static void
install_screen_colormaps (Screen *screen)
{
  unsigned int i;
  Display *dpy = DisplayOfScreen (screen);
  Window real_root;
  Window parent, *kids = 0;
  unsigned int nkids = 0;
  XErrorHandler old_handler;

  XSync (dpy, False);
  old_handler = XSetErrorHandler (ignore_badwindow_ehandler);
  error_handler_hit_p = False;

  real_root = XRootWindowOfScreen (screen);  /* not vroot */
  if (XQueryTree (dpy, real_root, &real_root, &parent, &kids, &nkids))
    for (i = 0; i < nkids; i++)
      {
	XWindowAttributes xgwa;
	Window client;
	/* if (! (client = XmuClientWindow (dpy, kids[i]))) */
	  client = kids[i];
	xgwa.colormap = 0;
	XGetWindowAttributes (dpy, client, &xgwa);
	if (xgwa.colormap && xgwa.map_state == IsViewable)
	  XInstallColormap (dpy, xgwa.colormap);
      }
  XInstallColormap (dpy, DefaultColormapOfScreen (screen));
  XSync (dpy, False);
  XSetErrorHandler (old_handler);
  XSync (dpy, False);

  if (kids)
    XFree ((char *) kids);
}


static Bool
MapNotify_event_p (Display *dpy, XEvent *event, XPointer window)
{
  return (event->xany.type == MapNotify &&
	  event->xvisibility.window == (Window) window);
}


static void
raise_window (Display *dpy, Window window, Bool dont_wait, Bool verbose_p)
{
  if (verbose_p)
    fprintf(stderr, "%s: raising window 0x%0lX (%s)\n",
            blurb(), (unsigned long) window,
            (dont_wait ? "not waiting" : "waiting"));

  if (! dont_wait)
    {
      XWindowAttributes xgwa;
      XSizeHints hints;
      long supplied = 0;
      memset(&hints, 0, sizeof(hints));
      XGetWMNormalHints(dpy, window, &hints, &supplied);
      XGetWindowAttributes (dpy, window, &xgwa);
      hints.x = xgwa.x;
      hints.y = xgwa.y;
      hints.width = xgwa.width;
      hints.height = xgwa.height;
      hints.flags |= (PPosition|USPosition|PSize|USSize);
      XSetWMNormalHints(dpy, window, &hints);

      XSelectInput (dpy, window, (xgwa.your_event_mask | StructureNotifyMask));
    }

  XMapRaised (dpy, window);

  if (! dont_wait)
    {
      XEvent event;
      XIfEvent (dpy, &event, MapNotify_event_p, (XPointer) window);
      XSync (dpy, True);
    }
}


/* Returns a pixmap of a screenshot, that is the size of the window
   and covers that window's extent.

   This unmaps the window, waits an arbitrary amount of time, grabs bits,
   then re-maps the window.  Grabbing bits happens either with XCopyArea
   (on real X11) or by running an external program (macOS or Wayland).
 */
static Pixmap
grab_screen_image (Screen *screen, Window window, Bool cache_p, Bool verbose_p)
{
  Display *dpy = DisplayOfScreen (screen);
  XWindowAttributes xgwa;
  Window real_root;
  Bool root_p;
  Bool saver_p;
  Bool mapped_p;
  double unmap_time = 0;
  char *filename = 0;
  Pixmap screenshot = None;
  XRectangle geom;

  real_root = XRootWindowOfScreen (screen);  /* not vroot */
  root_p = (window == real_root);
  saver_p = xscreensaver_window_p (dpy, window);

  XGetWindowAttributes (dpy, window, &xgwa);
  screen = xgwa.screen;
  mapped_p = (xgwa.map_state == IsViewable);

  if (!cache_p)
    {
      /* This is maybe overloading the meaning of --cache, but for fading
         to work, we need to be able to grab a screenshot that includes
         the window itself.  This only comes into effect when using an
         external screen grabber. */
      mapped_p   = 0;
      unmap_time = 0;
    }

  if (root_p)
    unmap_time = 0;  /* real root window needs no delay */
  else if (!mapped_p)
    unmap_time = 0;  /* not currently on screen */
  else
    {
      unmap_time = get_float_resource (dpy, "grabRootDelay", "Seconds");
      if (unmap_time <= 0.00001 || unmap_time > 20)
        unmap_time = 0.33;

      /* 2022, Raspbian 11.5: after we call XUnmapWindow, it takes nearly
         *five seconds* for the framebuffer to update!  If we delay for less
         than that, the window grabs an image of itself, which usually means
         black.  One would normally suspect the compositor of being
         responsible for these sorts of shenanigans, but this is under
         LXDE...
  
         Oddly, running Debian 11.4 under VirtualBox (meaning slowwwwww)
         does not have this problem, and a delay of 0.33 is plenty.
  
         This problem does not afflict driver/fade.c as it just uses
         XCopyArea directly without needing to wait for other processes
         to react to the XUnmapWindow and re-paint.
  
         This nonsense is what led me to write screenshot.c and used a
         cached screenshot image instead.
       */
      if (saver_p && unmap_time < 5.0)
        unmap_time = 5.0;
    }

  if (verbose_p)
    {
      fprintf (stderr, "%s: window 0x%0lX"
               " root: %d saver: %d map: %d wait: %.1f\n",
               blurb(), (unsigned long) window,
               root_p, saver_p, mapped_p, unmap_time);

      if (xgwa.visual->class != TrueColor)
        {
          fprintf(stderr, "%s: ", blurb());
          describe_visual (stderr, screen, xgwa.visual, False);
          fprintf (stderr, "\n");
        }
    }


  if (!root_p &&
      mapped_p &&
      !top_level_window_p (screen, window))
    {
      if (verbose_p)
        fprintf (stderr, "%s: not a top-level window: 0x%0lX: not grabbing\n",
                 blurb(), (unsigned long) window);
      return None;
    }

  if (unmap_time > 0)
    {
      if (verbose_p)
        fprintf (stderr, "%s: unmapping 0x%0lX\n", blurb(),
                 (unsigned long) window);
      XUnmapWindow (dpy, window);
      if (xgwa.visual->class != TrueColor)
        install_screen_colormaps (screen);
      XSync (dpy, True);
      if (verbose_p)
        fprintf (stderr, "%s: sleeping %.02f\n", blurb(), unmap_time);
      /* wait for everyone to swap in and handle exposes */
      usleep ((unsigned long) (unmap_time * 1000000));
    }

  /* Now that the window is off the screen and we have delayed, grab bits,
     then put the window back.
   */
# ifdef USE_EXTERNAL_SCREEN_GRABBER

#  if defined(HAVE_REAL_X11)
  if (getenv ("WAYLAND_DISPLAY") || getenv ("WAYLAND_SOCKET"))
#  endif

    {
      /* Wayland or macOS: Grab screen via external program. */

      if (verbose_p)
        fprintf (stderr, "%s: grabbing via \"%s\"\n", blurb(),
                 GETIMAGE_SCREEN_PROGRAM);
      filename = get_desktop_filename (screen, window, verbose_p);

      /* We can raise the window before parsing the file. */
      if (unmap_time > 0)
        raise_window (dpy, window, saver_p, verbose_p);
      unmap_time = 0;

      if (!filename)
        {
          /* if (verbose_p) */
          fprintf (stderr, "%s: screenshot via \"%s\" failed\n", blurb(),
                   GETIMAGE_SCREEN_PROGRAM);
          return None;
        }

      /* Read the file into a pixmap the size of the destination drawable. */
      screenshot = XCreatePixmap (dpy, window, xgwa.width, xgwa.height,
                                  xgwa.depth);
      if (!screenshot)
        {
          unlink (filename);
          return None;
        }

#  if defined(HAVE_GDK_PIXBUF)
      if (! read_file_gdk (screen, window, screenshot, filename,
                           verbose_p, &geom))
        {
          unlink (filename);
          XFreePixmap (dpy, screenshot);
          return None;
        }
#  elif defined(HAVE_JPEGLIB)
      if (! read_file_jpeglib (screen, window, screenshot, filename,
                               verbose_p, &geom))
        {
          unlink (filename);
          XFreePixmap (dpy, screenshot);
          return None;
        }
#  else  /* !(HAVE_GDK_PIXBUF || HAVE_JPEGLIB) */
      /* shouldn't get here if we have no image-loading methods available. */
      abort();
# endif /* !(HAVE_GDK_PIXBUF || HAVE_JPEGLIB) */

      if (unlink (filename))
        {
          char buf[512];
          sprintf (buf, "%s: rm %.100s", blurb(), filename);
          perror (buf);
        }
      else if (verbose_p)
        fprintf (stderr, "%s: rm %s\n", blurb(), filename);

      if (verbose_p)
        fprintf (stderr, "%s: screenshot %dx%d+%d+%d via \"%s\"\n", blurb(),
                 xgwa.width, xgwa.height, xgwa.x, xgwa.y, 
                 GETIMAGE_SCREEN_PROGRAM);
    }
# endif /* USE_EXTERNAL_SCREEN_GRABBER */
# ifndef HAVE_MACOS_X11
  else
    {
      /* Real X11: Grab screen via XCopyArea. */

      XGCValues gcv;
      GC gc;

      if (verbose_p)
        fprintf (stderr, "%s: grabbing via XCopyArea\n", blurb());

      screenshot = XCreatePixmap (dpy, window, xgwa.width, xgwa.height,
                                  xgwa.depth);

      if (!root_p && xgwa.visual->class != TrueColor)
        copy_default_colormap_contents (screen, xgwa.colormap, xgwa.visual,
                                        verbose_p);

      gcv.function = GXcopy;
      gcv.subwindow_mode = IncludeInferiors;
      gc = XCreateGC (dpy, window, GCFunction | GCSubwindowMode, &gcv);
      window_root_offset (dpy, window, &xgwa.x, &xgwa.y);
      XCopyArea (dpy, real_root, screenshot, gc,
                 xgwa.x, xgwa.y, xgwa.width, xgwa.height, 0, 0);
      XFreeGC (dpy, gc);

      if (!root_p && xgwa.visual->class != TrueColor)
        copy_default_colormap_contents (screen, xgwa.colormap, xgwa.visual,
                                        verbose_p);
    }
# endif /* !HAVE_MACOS_X11 */

  if (unmap_time > 0)
    raise_window (dpy, window, saver_p, verbose_p);

  /* Generally it's bad news to call XInstallColormap() explicitly,
     but this file does a lot of sleazy stuff already...  This is to
     make sure that the window's colormap is installed, even in the
     case where the window is OverrideRedirect. */
  if (xgwa.colormap && xgwa.visual->class != TrueColor)
    XInstallColormap (dpy, xgwa.colormap);

  if (verbose_p)
    fprintf (stderr, "%s: grabbed screenshot to %s window\n", blurb(), 
             (root_p ? "real root" : saver_p ? "saver" : "top-level"));

  return screenshot;
}


/* Grabs a desktop screen shot that is the size and position of the
   Window (the bits under that window), and renders it onto the Drawable.  
   If the window and drawable are not the same size, the image in the
   drawable is scaled.  Returns False if it fails.
 */
static Bool
display_desktop (Screen *screen, Window window, Drawable drawable,
                 Bool cache_p, Bool verbose_p, XRectangle *geom_ret)
{
  Display *dpy = DisplayOfScreen (screen);
  Window root;
  Pixmap screenshot = None;
  int srcx, srcy, dstx, dsty, w2, h2;
  unsigned int srcw, srch, dstw, dsth, bw, d;
  GC gc;
  XGCValues gcv;

  if (verbose_p)
    fprintf (stderr, "%s: grabbing desktop image\n", blurb());

  root = XRootWindowOfScreen (screen);  /* not vroot */

  /* See if we already have a saved screenshot of the whole desktop;
     if so, grab a sub-rectangle from that, of *this* window. */
  if (cache_p)
    screenshot = screenshot_load (dpy, window, verbose_p);

  /* Otherwise, grab a new one. */
  if (! screenshot)
    {
      if (verbose_p)
        fprintf (stderr, "%s: no saved screenshot on 0x%lx\n", blurb(),
                 window);
      screenshot = grab_screen_image (screen, window, cache_p, verbose_p);
    }

  if (!screenshot)
    return False;

  XGetGeometry (dpy, screenshot, &root, &srcx, &srcy, &srcw, &srch, &bw, &d);
  XGetGeometry (dpy, drawable,   &root, &dstx, &dsty, &dstw, &dsth, &bw, &d);

  compute_image_scaling (srcw, srch,
                         dstw, dsth, verbose_p,
                         &srcx, &srcy, &dstx, &dsty, &w2, &h2);

  if (srcw != w2 || srch != h2)  /* scale screenshot to fit drawable */
    {
      XImage *ximage = 0;
      XWindowAttributes xgwa;
      Pixmap scaled;

      XGetWindowAttributes (dpy, window, &xgwa);

      ximage = XGetImage (dpy, screenshot, 0, 0, srcw, srch, ~0L, ZPixmap);
      if (!ximage ||
          !scale_ximage (xgwa.screen, xgwa.visual, ximage, w2, h2))
        return False;

      scaled = XCreatePixmap (dpy, window, w2, h2, xgwa.depth);
      if (! scaled) return False;

      gc = XCreateGC (dpy, scaled, 0, &gcv);
      XPutImage (dpy, scaled, gc, ximage, srcx, srcy, dstx, dsty, w2, h2);
      XDestroyImage (ximage);
      XFreeGC (dpy, gc);
      XFreePixmap (dpy, screenshot);
      screenshot = scaled;
    }

  gc = XCreateGC (dpy, drawable, 0, &gcv);
  XCopyArea (dpy, screenshot, drawable, gc, 0, 0, w2, h2, 0, 0);
  XFreeGC (dpy, gc);

  if (geom_ret)
    {
      geom_ret->x = 0;
      geom_ret->y = 0;
      geom_ret->width  = w2;
      geom_ret->height = h2;
    }

  return True;
}


/* Whether the given Drawable is unreasonably small.
 */
static Bool
drawable_miniscule_p (Display *dpy, Drawable drawable)
{
  Window root;
  int xx, yy;
  unsigned int bw, d, w = 0, h = 0;
  XGetGeometry (dpy, drawable, &root, &xx, &yy, &w, &h, &bw, &d);
  return (w < 32 || h < 30);
}


/* Returns the URL and title of the given file, if present in xattrs.
   Duplicated in grabclient.c.
 */
static void
get_file_xattrs (const char *file, char **urlP, char **titleP)
{
# ifdef HAVE_GETXATTR

#  ifdef XATTR_ADDITIONAL_OPTIONS
#   define GETXATTR(F,K,B,S) getxattr (F, K, B, S, 0, 0)
#  else
#   define GETXATTR(F,K,B,S) getxattr (F, K, B, S)
#  endif

  char url[1024], title[1024];
  ssize_t s;

  s = GETXATTR (file, "user.xdg.origin.url", url, sizeof(url)-1);
  if (s > 0)
    {
      url[s] = 0;
      *urlP = strdup (url);
    }

  s = GETXATTR (file, "user.dublincore.title", title, sizeof(title)-1);
  if (s > 0)
    {
      title[s] = 0;
      *titleP = strdup (title);
    }
# endif /* HAVE_GETXATTR */
}


/* Grabs an image (from a file, video, or the desktop) and renders it on
   the Drawable.  If 'file' is specified, always use that file.  Otherwise,
   select randomly, based on the other arguments.
 */
static void
get_image (Screen *screen,
           Window window, Drawable drawable,
           Bool verbose_p,
           Bool desk_p,
           Bool cache_p,
           Bool video_p,
           Bool image_p,
           const char *dir,
           const char *file)
{
  Display *dpy = DisplayOfScreen (screen);
  grab_type which = GRAB_BARS;
  struct stat st;
  const char *file_prop = 0;
  char *xattr_url   = 0;
  char *xattr_title = 0;
  char *absfile = 0;
  XRectangle geom = { 0, 0, 0, 0 };

  /* Make sure the Screen and the Window correspond. */
  {
    XWindowAttributes xgwa;
    XGetWindowAttributes (dpy, window, &xgwa);
    screen = xgwa.screen;
  }

  if (file && stat (file, &st))
    {
      fprintf (stderr, "%s: file \"%s\" does not exist\n", blurb(), file);
      file = 0;
    }

  if (verbose_p)
    {
      fprintf (stderr, "%s: grabDesktopImages:  %s\n",
               blurb(), desk_p ? "True" : "False");
      fprintf (stderr, "%s: grabVideoFrames:    %s\n",
               blurb(), video_p ? "True" : "False");
      fprintf (stderr, "%s: chooseRandomImages: %s\n",
               blurb(), image_p ? "True" : "False");
      fprintf (stderr, "%s: imageDirectory:     %s\n",
               blurb(), (file ? file : dir ? dir : ""));
    }

# if !(defined(HAVE_GDK_PIXBUF) || defined(HAVE_JPEGLIB))
  image_p = False;    /* can't load images from files... */
#  ifdef USE_EXTERNAL_SCREEN_GRABBER
  desk_p = False;     /* ...or from desktops grabbed to files. */
#  endif

  if (file)
    {
      fprintf (stderr,
               "%s: image file loading not available at compile-time\n",
               blurb());
      fprintf (stderr, "%s: can't load \"%s\"\n", blurb(), file);
      file = 0;
    }
# endif /* !(HAVE_GDK_PIXBUF || HAVE_JPEGLIB) */

  if (file)
    {
      desk_p  = False;
      video_p = False;
      image_p = True;
    }
  else if (!dir || !*dir)
    {
      if (verbose_p && image_p)
        fprintf (stderr,
                 "%s: no imageDirectory: turning off chooseRandomImages\n",
                 blurb());
      image_p = False;
    }

  /* If the target drawable is really small, no good can come of that.
     Always do colorbars in that case.
   */
  if (drawable_miniscule_p (dpy, drawable))
    {
      desk_p  = False;
      video_p = False;
      image_p = False;
    }


# ifndef _VROOT_H_
#  error Error!  This file definitely needs vroot.h!
# endif

  if (! (desk_p || video_p || image_p))
    which = GRAB_BARS;
  else
    {
      int i = 0;
      int n;
      /* Loop until we get one that's permitted.
         If files or video are permitted, do them more often
         than desktop.

             D+V+I: 10% + 45% + 45%.
             V+I:   50% + 50%
             D+V:   18% + 82%
             D+I:   18% + 82%
       */
    AGAIN:
      n = (random() % 100);
      if (++i > 300) abort();
      else if (desk_p  && n < 10) which = GRAB_DESK;   /* 10% */
      else if (video_p && n < 55) which = GRAB_VIDEO;  /* 45% */
      else if (image_p)           which = GRAB_FILE;   /* 45% */
      else goto AGAIN;
    }


  /* If we're to search a directory to find an image file, do so now.
   */
  if (which == GRAB_FILE && !file)
    {
      file = get_filename (screen, window, dir, verbose_p);
      if (!file)
        {
          which = GRAB_BARS;
          if (verbose_p)
            fprintf (stderr, "%s: no image files found\n", blurb());
        }
    }

  /* Now actually render something.
   */
  switch (which)
    {
    case GRAB_BARS:
      {
        XWindowAttributes xgwa;
        Window root;
        int xx, yy;
        unsigned int bw, d, w = 0, h = 0;

      COLORBARS:
        if (verbose_p)
          fprintf (stderr, "%s: drawing colorbars\n", blurb());
        XGetWindowAttributes (dpy, window, &xgwa);
        XGetGeometry (dpy, drawable, &root, &xx, &yy, &w, &h, &bw, &d);
        colorbars (screen, xgwa.visual, drawable, xgwa.colormap,
                   (h >= 600 ? 2 : h >= 300 ? 1 : 0));
        if (! file_prop) file_prop = "";
        geom.x = 0;
        geom.y = 0;
        geom.width = w;
        geom.height = h;
      }
      break;

    case GRAB_DESK:
      if (! display_desktop (screen, window, drawable, cache_p, 
                             verbose_p, &geom))
        goto COLORBARS;
      file_prop = "desktop";
      break;

    case GRAB_FILE:
      if (*file && *file != '/')	/* pathname is relative to dir. */
        {
          if (absfile) free (absfile);
          absfile = malloc (strlen(dir) + strlen(file) + 10);
          strcpy (absfile, dir);
          if (dir[strlen(dir)-1] != '/')
            strcat (absfile, "/");
          strcat (absfile, file);
        }
      if (! display_file (screen, window, drawable, 
                          (absfile ? absfile : file),
                          verbose_p, &geom))
        goto COLORBARS;
      file_prop = file;
      get_file_xattrs ((absfile ? absfile : file), &xattr_url, &xattr_title);
      break;

    case GRAB_VIDEO:
      if (! display_video (screen, window, drawable, verbose_p, &geom))
        goto COLORBARS;
      file_prop = "video";
      break;

    default:
      abort();
      break;
    }

  {
    Atom a = XInternAtom (dpy, XA_XSCREENSAVER_IMAGE_FILENAME, False);
    if (xattr_url)
      XChangeProperty (dpy, window, a, XA_STRING, 8, PropModeReplace, 
                       (unsigned char *) xattr_url, strlen(xattr_url));
    else if (file_prop && *file_prop)
      {
        char *f2 = strdup (file_prop);

        /* Take the extension off of the file name. */
        /* Duplicated in utils/grabclient.c. */
        char *slash = strrchr (f2, '/');
        char *dot = strrchr ((slash ? slash : f2), '.');
        if (dot) *dot = 0;
        XChangeProperty (dpy, window, a, XA_STRING, 8, PropModeReplace, 
                         (unsigned char *) f2, strlen(f2));
        free (f2);
      }
    else
      XDeleteProperty (dpy, window, a);

    a = XInternAtom (dpy, XA_XSCREENSAVER_IMAGE_TITLE, False);
    if (xattr_title)
      XChangeProperty (dpy, window, a, XA_STRING, 8, PropModeReplace, 
                       (unsigned char *) xattr_title, strlen(xattr_title));
    else
      XDeleteProperty (dpy, window, a);

    a = XInternAtom (dpy, XA_XSCREENSAVER_IMAGE_GEOMETRY, False);
    if (geom.width > 0)
      {
        char gstr[30];
        sprintf (gstr, "%dx%d+%d+%d", geom.width, geom.height, geom.x, geom.y);
        XChangeProperty (dpy, window, a, XA_STRING, 8, PropModeReplace, 
                         (unsigned char *) gstr, strlen (gstr));
      }
    else
      XDeleteProperty (dpy, window, a);
  }

  if (absfile) free (absfile);

  XSync (dpy, False);

  if (xattr_url)   free (xattr_url);
  if (xattr_title) free (xattr_title);
}


/* Read a few entries from ~/.xscreensaver and insert them into Xrm.
   Without this we'd only see the values from the .ad file.
 */
static void init_line_handler (int lineno, 
                               const char *key, const char *val,
                               void *closure)
{
  Display *dpy = (Display *) closure;
  XrmDatabase db = XtDatabase (dpy);

  if (!strcmp (key, "verbose") ||
      !strcmp (key, "grabDesktopImages") ||
      !strcmp (key, "grabVideoFrames") ||
      !strcmp (key, "chooseRandomImages") ||
      !strcmp (key, "imageDirectory"))
    {
      char *key2 = (char *) malloc (strlen(key) + strlen(progname) + 10);
      sprintf (key2, "%s.%s", progname, key);
      /* fprintf (stderr, "%s: XRM: %s = %s\n", progname, key2, val); */
      XrmPutStringResource (&db, key2, val);
      free (key2);
    }
}

static void
load_init_file (Display *dpy)
{
  const char *home = getenv("HOME");
  char *fn;
  if (!home) home = "";
  fn = (char *) malloc (strlen(home) + 40);
  sprintf (fn, "%s/.xscreensaver", home);
  parse_init_file (fn, init_line_handler, dpy);
  free (fn);
}


#ifdef DEBUG
static Bool
mapper (XrmDatabase *db, XrmBindingList bindings, XrmQuarkList quarks,
	XrmRepresentation *type, XrmValue *value, XPointer closure)
{
  int i;
  for (i = 0; quarks[i]; i++)
    {
      if (bindings[i] == XrmBindTightly)
	fprintf (stderr, (i == 0 ? "" : "."));
      else if (bindings[i] == XrmBindLoosely)
	fprintf (stderr, "*");
      else
	fprintf (stderr, " ??? ");
      fprintf(stderr, "%s", XrmQuarkToString (quarks[i]));
    }

  fprintf (stderr, ": %s\n", (char *) value->addr);

  return False;
}
#endif /* DEBUG */


#define USAGE "usage: %s [ -options... ] window-id [pixmap-id]\n"	      \
   "\n"									      \
   "    %s\n"								      \
   "\n"									      \
   "    %s puts an image on the given window or pixmap.\n"		      \
   "\n"									      \
   "    It is used by those xscreensaver demos that operate on images.\n"     \
   "    The image may be a file loaded from disk, a frame grabbed from\n"     \
   "    the system's video camera, or a screenshot of the desktop,\n"         \
   "    depending on command-line options or the ~/.xscreensaver file.\n"     \
   "\n"									      \
   "    Options include:\n"						      \
   "\n"									      \
   "      --display host:dpy.screen   which display to use\n"		      \
   "      --root                      draw to the root window\n"	      \
   "      --verbose                   print diagnostics\n"		      \
   "      --images  / --no-images     whether to allow image file loading\n"  \
   "      --video   / --no-video      whether to allow video grabs\n"	      \
   "      --desktop / --no-desktop    whether to allow desktop screen grabs\n"\
   "      --cache   / --no-cache      whether to cache the desktop image\n"   \
   "      --directory <path>          where to find image files to load\n"    \
   "      --file <filename>           load this image file\n"                 \
   "\n"									      \
   "    The xscreensaver-settings program lets you set the defaults for\n"    \
   "    these options in your ~/.xscreensaver file.\n"		              \
   "\n"

int
main (int argc, char **argv)
{
  Widget toplevel;
  Display *dpy;
  Screen *screen;
  const char *oprogname = progname;
  char *file = 0;
  char version[255];

  Window window = (Window) 0;
  Drawable drawable = (Drawable) 0;
  const char *window_str = 0;
  const char *drawable_str = 0;
  char *s;
  int i;

  Bool verbose_p, grab_desktop_p, grab_video_p, random_image_p;
  Bool cache_desktop_p;
  char *image_directory;

  progname = argv[0];
  s = strrchr (progname, '/');
  if (s) progname = s+1;
  oprogname = progname;

# ifndef _VROOT_H_
#  error Error!  This file definitely needs vroot.h!
# endif

  /* Get the version number, for error messages. */
  {
    char *v = (char *) strdup(strchr(screensaver_id, ' '));
    char *s1, *s2, *s3, *s4;
    s1 = (char *) strchr(v,  ' '); s1++;
    s2 = (char *) strchr(s1, ' ');
    s3 = (char *) strchr(v,  '('); s3++;
    s4 = (char *) strchr(s3, ')');
    *s2 = 0;
    *s4 = 0;
    sprintf (version, "Part of XScreenSaver %s -- %s.", s1, s3);
    free(v);
  }

  /* We must read exactly the same resources as xscreensaver.
     That means we must have both the same progclass *and* progname,
     at least as far as the resource database is concerned.  So,
     put "xscreensaver" in argv[0] while initializing Xt.
   */
  progname = argv[0] = "xscreensaver";

  /* allow one dash or two. */
  for (i = 1; i < argc; i++)
    if (argv[i][0] == '-' && argv[i][1] == '-') argv[i]++;

  toplevel = XtAppInitialize (&app, progclass, 0, 0, &argc, argv, 0, 0, 0);
  dpy = XtDisplay (toplevel);
  screen = XtScreen (toplevel);
  XSetErrorHandler (x_ehandler);
  XSync (dpy, False);

  /* Randomize -- only need to do this here because this program
     doesn't use the `screenhack.h' or `lockmore.h' APIs. */
# undef ya_rand_init
  ya_rand_init (0);

  load_init_file (dpy);

  verbose_p       = get_boolean_resource(dpy, "verbose", "Boolean");
  grab_desktop_p  = get_boolean_resource(dpy, "grabDesktopImages", "Boolean");
  grab_video_p    = get_boolean_resource(dpy, "grabVideoFrames", "Boolean");
  random_image_p  = get_boolean_resource(dpy, "chooseRandomImages", "Boolean");
  image_directory = get_string_resource (dpy, "imageDirectory", "String");
  cache_desktop_p = True;
  if (!image_directory) image_directory = "";

  if (!strncmp (image_directory, "~/", 2))
    {
      const char *home = getenv("HOME");
      if (home && *home)
        {
          char *s2 = (char *)
            malloc (strlen(image_directory) + strlen(home) + 10);
          strcpy (s2, home);
          strcat (s2, image_directory + 1);
          free (image_directory);
          image_directory = s2;
        }
    }

  progname = oprogname;
  argv[0] = (char *) oprogname;

  for (i = 1; i < argc; i++)
    {
      unsigned long w;
      char dummy;

      /* Have to re-process these, or else the .xscreensaver file
         has priority over the command line...
       */
      if (!strcmp (argv[i], "-v") ||
          !strcmp (argv[i], "-vv") ||
          !strcmp (argv[i], "-vvv") ||
          !strcmp (argv[i], "-vvvv") ||
          !strcmp (argv[i], "-vvvvv") ||
          !strcmp (argv[i], "-vvvvvv") ||
          !strcmp (argv[i], "-verbose"))
        verbose_p = True;
      else if (!strcmp (argv[i], "-desktop"))    grab_desktop_p  = True;
      else if (!strcmp (argv[i], "-no-desktop")) grab_desktop_p  = False;
      else if (!strcmp (argv[i], "-cache"))      cache_desktop_p = True;
      else if (!strcmp (argv[i], "-no-cache"))   cache_desktop_p = False;
      else if (!strcmp (argv[i], "-video"))      grab_video_p    = True;
      else if (!strcmp (argv[i], "-no-video"))   grab_video_p    = False;
      else if (!strcmp (argv[i], "-images"))     random_image_p  = True;
      else if (!strcmp (argv[i], "-no-images"))  random_image_p  = False;
      else if (!strcmp (argv[i], "-file"))       file = argv[++i];
      else if (!strcmp (argv[i], "-directory") || !strcmp (argv[i], "-dir"))
        image_directory = argv[++i];
      else if (!strcmp (argv[i], "-root") || !strcmp (argv[i], "root"))
        {
          if (window)
            {
              fprintf (stderr, "%s: both %s and %s specified?\n",
                       blurb(), argv[i], window_str);
              goto LOSE;
            }
          window_str = argv[i];
          window = VirtualRootWindowOfScreen (screen);
        }
      else if ((1 == sscanf (argv[i], " 0x%lx %c", &w, &dummy) ||
                1 == sscanf (argv[i], " %lu %c",   &w, &dummy)) &&
               w != 0)
        {
        WID:
          if (drawable)
            {
              fprintf (stderr, "%s: both %s and %s specified?\n",
                       blurb(), drawable_str, argv[i]);
              goto LOSE;
            }
          else if (window)
            {
              drawable_str = argv[i];
              drawable = (Drawable) w;
            }
          else
            {
              window_str = argv[i];
              window = (Window) w;
            }
        }
      else if (!strcmp (argv[i], "-window-id") && argv[i+1] &&
               (1 == sscanf (argv[i+1], " 0x%lx %c", &w, &dummy)))
        {
          /* Allow "--window-id 0xNN" as well as plain "0xNN" for use by
             xscreensaver-settings, even though that doesn't really work:
             savers are expected to continue running, not exit immediately. */
          i++;
          goto WID;
        }
      else
        {
          if (argv[i][0] == '-')
            fprintf (stderr, "\n%s: unknown option \"%s\"\n",
                     blurb(), argv[i]);
          else
            fprintf (stderr, "\n%s: unparsable window/pixmap ID: \"%s\"\n",
                     blurb(), argv[i]);
        LOSE:
# ifdef __GNUC__
          __extension__   /* don't warn about "string length is greater than
                             the length ISO C89 compilers are required to
                             support" in the usage string... */
# endif
          fprintf (stderr, USAGE, progname, version, progname);
          exit (1);
        }
    }

  if (window == 0)
    {
      fprintf (stderr, "\n%s: no window ID specified!\n", blurb());
      goto LOSE;
    }


#ifdef DEBUG
  if (verbose_p)       /* Print out all the resources we can see. */
    {
      XrmName name = { 0 };
      XrmClass class = { 0 };
      int count = 0;
      XrmEnumerateDatabase (XtDatabase (dpy),
                            &name, &class, XrmEnumAllLevels, mapper,
                            (XtPointer) &count);
    }
#endif /* DEBUG */

  if (!window) abort();
  if (!drawable) drawable = window;

  get_image (screen, window, drawable, verbose_p,
             grab_desktop_p, cache_desktop_p, grab_video_p, random_image_p,
             image_directory, file);
  XSync (dpy, False);
  exit (0);
}
