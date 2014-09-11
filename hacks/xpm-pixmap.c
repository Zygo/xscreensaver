/* xpm-pixmap.c --- converts XPM data to a Pixmap.
 * xscreensaver, Copyright (c) 1998-2006 Jamie Zawinski <jwz@jwz.org>
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

#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_COCOA
# include "jwxyz.h"
#else
# include <X11/Xlib.h>
# include <X11/Xutil.h>
# include "visual.h"  /* for screen_number() */
#endif

#include "xpm-pixmap.h"

extern char *progname;

#if defined(HAVE_GDK_PIXBUF)

# include <gdk-pixbuf/gdk-pixbuf.h>

# ifdef HAVE_GTK2
#  include <gdk-pixbuf-xlib/gdk-pixbuf-xlib.h>
# else  /* !HAVE_GTK2 */
#  include <gdk-pixbuf/gdk-pixbuf-xlib.h>
# endif /* !HAVE_GTK2 */


/* Returns a Pixmap structure containing the bits of the given XPM image.
   This is the gdk_pixbuf version of this function.
 */
static Pixmap
xpm_to_pixmap_1 (Display *dpy, Window window,
                 int *width_ret, int *height_ret,
                 Pixmap *mask_ret,
                 const char *filename,
                 /*const*/ char * const *xpm_data)
{
  GdkPixbuf *pb;
  static int initted = 0;
  XWindowAttributes xgwa;
#ifdef HAVE_GTK2
  GError *gerr = NULL;
#endif /* HAVE_GTK2 */
  XGetWindowAttributes (dpy, window, &xgwa);

  if (!initted)
    {
#ifdef HAVE_GTK2
#if !GLIB_CHECK_VERSION(2, 36 ,0)
      g_type_init ();
#endif
#endif /* HAVE_GTK2 */
      gdk_pixbuf_xlib_init (dpy, screen_number (xgwa.screen));
      xlib_rgb_init (dpy, xgwa.screen);
      initted = 1;
    }

  pb = (filename
#ifdef HAVE_GTK2
        ? gdk_pixbuf_new_from_file (filename, &gerr)
#else  /* !HAVE_GTK2 */
	? gdk_pixbuf_new_from_file (filename)
#endif /* !HAVE_GTK2 */
        : gdk_pixbuf_new_from_xpm_data ((const char **) xpm_data));
  if (pb)
    {
      int w = gdk_pixbuf_get_width (pb);
      int h = gdk_pixbuf_get_height (pb);
      Pixmap pixmap = 0;

      /* #### Note that this always uses the default colormap!  Morons!
              Owen says that in Gnome 2.0, I should try using
              gdk_pixbuf_render_pixmap_and_mask_for_colormap() instead.
              But I don't have Gnome 2.0 yet.
       */
      gdk_pixbuf_xlib_render_pixmap_and_mask (pb, &pixmap, mask_ret, 128);

      if (!pixmap)
        {
          fprintf (stderr, "%s: out of memory (%d x %d)\n", progname, w, h);
          exit (-1);
        }
      /* gdk_pixbuf_unref (pb);  -- #### does doing this free colors? */

      if (width_ret)  *width_ret  = w;
      if (height_ret) *height_ret = h;

      return pixmap;
    }
  else if (filename)
    {
#ifdef HAVE_GTK2
      fprintf (stderr, "%s: %s\n", progname, gerr->message);
      g_error_free (gerr);
#else /* !HAVE_GTK2 */
      fprintf (stderr, "%s: unable to load %s\n", progname, filename);
#endif /* !HAVE_GTK2 */
      exit (-1);
    }
  else
    {
      fprintf (stderr, "%s: unable to initialize built-in images\n", progname);
      exit (-1);
    }
}


#elif defined(HAVE_XPM)

#include <X11/Intrinsic.h>
#include <X11/xpm.h>

#ifdef HAVE_XMU
# ifndef VMS
#  include <X11/Xmu/Drawing.h>
#else  /* VMS */
#  include <Xmu/Drawing.h>
# endif /* VMS */
#endif

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))


static int
handle_xpm_error (const char *filename, int status)
{
  if (!filename) filename = "builtin";
  switch (status)
    {
    case XpmSuccess:
      return 0;
      break;
    case XpmColorError:
      fprintf (stderr, "%s: %s: warning: color substitution performed\n",
               progname, filename);
      return 0;
      break;
    case XpmFileInvalid:
      return 1;
      break;
    case XpmOpenFailed:
      fprintf (stderr, "%s: %s: no such file\n", progname, filename);
      break;
    case XpmNoMemory:
      fprintf (stderr, "%s: %s: out of memory\n", progname, filename);
      break;
    case XpmColorFailed:
      fprintf (stderr, "%s: %s: color allocation failed\n",
               progname, filename);
      break;
    default:
      fprintf (stderr, "%s: %s: unknown XPM error %d\n", progname,
               filename, status);
      break;
    }
  exit (-1);
}


/* The libxpm version of this function...
 */
static Pixmap
xpm_to_pixmap_1 (Display *dpy, Window window,
                 int *width_ret, int *height_ret,
                 Pixmap *mask_ret,
                 const char *filename,
                 /*const*/ char * const *xpm_data)
{
  Pixmap pixmap = 0;
  XpmAttributes xpmattrs;
  XpmImage xpm_image;
  XpmInfo xpm_info;
  int status;
  XWindowAttributes xgwa;
  XGetWindowAttributes (dpy, window, &xgwa);

  memset (&xpm_image, 0, sizeof(xpm_image));
  memset (&xpm_info, 0, sizeof(xpm_info));

  if (filename)
    {
      xpm_data = 0;
      status = XpmReadFileToData ((char *) filename, &xpm_data);
      if (handle_xpm_error (filename, status))
        goto TRY_XBM;
    }

  xpmattrs.valuemask = 0;

# ifdef XpmCloseness
  xpmattrs.valuemask |= XpmCloseness;
  xpmattrs.closeness = 40000;
# endif
# ifdef XpmVisual
  xpmattrs.valuemask |= XpmVisual;
  xpmattrs.visual = xgwa.visual;
# endif
# ifdef XpmDepth
  xpmattrs.valuemask |= XpmDepth;
  xpmattrs.depth = xgwa.depth;
# endif
# ifdef XpmColormap
  xpmattrs.valuemask |= XpmColormap;
  xpmattrs.colormap = xgwa.colormap;
# endif

  status = XpmCreatePixmapFromData (dpy, window, xpm_data,
                                    &pixmap, mask_ret, &xpmattrs);
  if (handle_xpm_error (filename, status))
    pixmap = 0;
  else
    {
      if (width_ret)  *width_ret  = xpmattrs.width;
      if (height_ret) *height_ret = xpmattrs.height;
    }

 TRY_XBM:

#ifdef HAVE_XMU
  if (! pixmap)
    {
      unsigned long fg = BlackPixelOfScreen (xgwa.screen);
      unsigned long bg = WhitePixelOfScreen (xgwa.screen);
      int xh, yh;
      Pixmap b2;
      pixmap = XmuLocateBitmapFile (xgwa.screen, filename,
				    0, 0, width_ret, height_ret, &xh, &yh);
      if (! pixmap)
	{
	  fprintf (stderr, "%s: couldn't find XBM %s\n", progname,
		   filename);
	  exit (-1);
	}
      b2 = XmuCreatePixmapFromBitmap (dpy, window, pixmap,
                                      *width_ret, *height_ret,
				      xgwa.depth, fg, bg);
      XFreePixmap (dpy, pixmap);
      pixmap = b2;

      if (!pixmap)
        {
	  fprintf (stderr, "%s: couldn't load XBM %s\n", progname, filename);
	  exit (-1);
        }
    }
#else  /* !XMU */
    {
      fprintf (stderr,
	       "%s: your vendor doesn't ship the standard Xmu library.\n",
	       progname);
      fprintf (stderr, "\tWe can't load XBM files without it.\n");
      exit (-1);
    }
#endif /* !XMU */

  return pixmap;
}

#else  /* !HAVE_XPM && !HAVE_GDK_PIXBUF */

/* If we don't have libXPM or Pixbuf, then use "minixpm".
   This can read XPM data from memory, but can't read files.
 */

#include "minixpm.h"

static Pixmap
xpm_to_pixmap_1 (Display *dpy, Window window,
                 int *width_ret, int *height_ret,
                 Pixmap *mask_ret,
                 const char *filename,
                 /*const*/ char * const *xpm_data)
{
  XWindowAttributes xgwa;
  XImage *ximage;
  Pixmap pixmap, p2 = 0;
  int iw, ih, npixels;
  unsigned long *pixels = 0;
  unsigned char *mask = 0;
  XGCValues gcv;
  GC gc;

  if (filename)
    {
      fprintf(stderr, 
              "%s: no files: not compiled with XPM or Pixbuf support.\n", 
              progname);
      exit (1);
    }

  if (! xpm_data) abort();

  XGetWindowAttributes (dpy, window, &xgwa);
  ximage = minixpm_to_ximage (dpy, xgwa.visual, xgwa.colormap, xgwa.depth, 
                              BlackPixelOfScreen (xgwa.screen),
                              (const char * const *)
                              xpm_data, &iw, &ih, &pixels, &npixels, 
                              (mask_ret ? &mask : 0));
  if (pixels) free (pixels);

  pixmap = XCreatePixmap (dpy, window, iw, ih, xgwa.depth);
  gc = XCreateGC (dpy, pixmap, 0, &gcv);
  XPutImage (dpy, pixmap, gc, ximage, 0, 0, 0, 0, iw, ih);
  XFreeGC (dpy, gc);
  XDestroyImage (ximage);

  if (mask)
    {
      p2 = XCreatePixmap (dpy, window, iw, ih, 1);
      gcv.foreground = 1;
      gcv.background = 0;
      gc = XCreateGC (dpy, p2, GCForeground|GCBackground, &gcv);
      ximage = XCreateImage (dpy, xgwa.visual, 1, XYBitmap, 0, (char *) mask,
                             iw, ih, 8, 0);
      ximage->byte_order = ximage->bitmap_bit_order = LSBFirst;
      if (!ximage) abort();
      XPutImage (dpy, p2, gc, ximage, 0, 0, 0, 0, iw, ih);
      XFreeGC (dpy, gc);
      XDestroyImage (ximage);
    }

  if (width_ret)  *width_ret  = iw;
  if (height_ret) *height_ret = ih;
  if (mask_ret)   *mask_ret   = p2;
  return pixmap;
}

#endif /* minixpm */


Pixmap
xpm_data_to_pixmap (Display *dpy, Window window, 
                    /*const*/ char * const *xpm_data,
                    int *width_ret, int *height_ret,
                    Pixmap *mask_ret)
{
  return xpm_to_pixmap_1 (dpy, window, width_ret, height_ret, mask_ret,
                          0, xpm_data);
}


Pixmap
xpm_file_to_pixmap (Display *dpy, Window window, const char *filename,
                    int *width_ret, int *height_ret,
                    Pixmap *mask_ret)
{
  return xpm_to_pixmap_1 (dpy, window, width_ret, height_ret, mask_ret,
                          filename, 0);
}
