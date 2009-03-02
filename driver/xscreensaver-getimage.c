/* xscreensaver, Copyright (c) 2001, 2002, 2003 by Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* xscreensaver-getimage -- helper program that puts a random image
   onto the given window or pixmap.  That image is either a screen-grab,
   a file loaded from disk, or a frame grabbed from the system's video
   input.
 */

#include "utils.h"

#include <X11/Intrinsic.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>		/* for waitpid() and associated macros */
#endif

#ifdef HAVE_XMU
# ifndef VMS
#  include <X11/Xmu/Error.h>
# else /* VMS */
#  include <Xmu/Error.h>
# endif
#else
# include "xmu.h"
#endif

#include "yarandom.h"
#include "grabscreen.h"
#include "resources.h"
#include "colorbars.h"
#include "visual.h"
#include "prefs.h"
#include "version.h"
#include "vroot.h"

#ifdef HAVE_GDK_PIXBUF
# undef HAVE_JPEGLIB
# ifdef HAVE_GTK2
#  include <gdk-pixbuf-xlib/gdk-pixbuf-xlib.h>
# else  /* !HAVE_GTK2 */
#  include <gdk-pixbuf/gdk-pixbuf-xlib.h>
# endif /* !HAVE_GTK2 */
#endif /* HAVE_GDK_PIXBUF */

#ifdef HAVE_JPEGLIB
# undef HAVE_GDK_PIXBUF
# include <jpeglib.h>
#endif


#ifdef __GNUC__
 __extension__     /* shut up about "string length is greater than the length
                      ISO C89 compilers are required to support" when including
                      the .ad file... */
#endif

static char *defaults[] = {
#include "../driver/XScreenSaver_ad.h"
 0
};



char *progname = 0;
char *progclass = "XScreenSaver";
XrmDatabase db;
XtAppContext app;

extern void grabscreen_verbose (void);


#define GETIMAGE_VIDEO_PROGRAM "xscreensaver-getimage-video"
#define GETIMAGE_FILE_PROGRAM  "xscreensaver-getimage-file"

const char *
blurb (void)
{
  return progname;
}


static int
x_ehandler (Display *dpy, XErrorEvent *error)
{
  fprintf (stderr, "\nX error in %s:\n", progname);
  XmuPrintDefaultErrorMessage (dpy, error, stderr);
  exit (-1);
  return 0;
}


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


/* Clear the window or pixmap to black, or its background color.
 */
static void
clear_drawable (Screen *screen, Drawable drawable)
{
  Display *dpy = DisplayOfScreen (screen);
  XGCValues gcv;
  GC gc;
  Window root;
  int x, y;
  unsigned int w, h, bw, d;
  XGetGeometry (dpy, drawable, &root, &x, &y, &w, &h, &bw, &d);

  /* The window might have no-op background of None, so to clear it,
     draw a black rectangle first, then do XClearWindow (in case the
     actual background color is non-black...) */

  /* #### really we should allocate "black" instead, but I'm lazy... */
  gcv.foreground = BlackPixelOfScreen (screen);
  gc = XCreateGC (dpy, drawable, GCForeground, &gcv);
  XFillRectangle (dpy, drawable, gc, 0, 0, w, h);
  XFreeGC (dpy, gc);
  if (drawable_window_p (dpy, drawable))
    XClearWindow (dpy, (Window) drawable);
  XFlush (dpy);
}


/* Figure out what kind of scaling/positioning we ought to do to display
   a src-sized image in a dest-sized window/pixmap.  Returns the width
   and height to which the image should be scaled, and the position where
   it should be displayed to center it.
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
      int tw = src_w * r;
      int th = src_h * r;
      int pct = (r * 100);

#if 0
      /* this optimization breaks things */
      if (pct < 95 || pct > 105)  /* don't scale if it's close */
#endif
        {
          if (verbose_p)
            fprintf (stderr, "%s: scaling image by %d%% (%dx%d -> %dx%d)\n",
                     progname, pct, src_w, src_h, tw, th);
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

  if (dest_w < src_w) src_w = dest_w;
  if (dest_h < src_h) src_h = dest_h;

  *scaled_w_ret = src_w;
  *scaled_h_ret = src_h;
  *scaled_from_x_ret = srcx;
  *scaled_from_y_ret = srcy;
  *scaled_to_x_ret = destx;
  *scaled_to_y_ret = desty;

  if (verbose_p)
    fprintf (stderr, "%s: displaying %dx%d image at %d,%d.\n",
             progname, src_w, src_h, destx, desty);
}


#ifdef HAVE_GDK_PIXBUF

/* Reads the given image file and renders it on the Drawable, using GDK.
   Returns False if it fails.
 */
static Bool
read_file_gdk (Screen *screen, Window window, Drawable drawable,
               const char *filename, Bool verbose_p)
{
  GdkPixbuf *pb;
  Display *dpy = DisplayOfScreen (screen);
  unsigned int win_width, win_height;
# ifdef HAVE_GTK2
  GError *gerr = 0;
# endif /* HAVE_GTK2 */

  {
    Window root;
    int x, y;
    unsigned int bw, d;
    XWindowAttributes xgwa;
    XGetWindowAttributes (dpy, window, &xgwa);
    screen = xgwa.screen;
    XGetGeometry (dpy, drawable,
                  &root, &x, &y, &win_width, &win_height, &bw, &d);
  }

  gdk_pixbuf_xlib_init (dpy, screen_number (screen));
# ifdef HAVE_GTK2
  g_type_init();
# else  /* !HAVE_GTK2 */
  xlib_rgb_init (dpy, screen);
# endif /* !HAVE_GTK2 */

  pb = gdk_pixbuf_new_from_file (filename
# ifdef HAVE_GTK2
                                 , &gerr
# endif /* HAVE_GTK2 */
                                 );

  if (!pb)
    {
      fprintf (stderr, "%s: unable to load \"%s\"\n", progname, filename);
#  ifdef HAVE_GTK2
      if (gerr && gerr->message && *gerr->message)
        fprintf (stderr, "%s: reason: %s\n", progname, gerr->message);
#  endif /* HAVE_GTK2 */
      return False;
    }
  else
    {
      int w = gdk_pixbuf_get_width (pb);
      int h = gdk_pixbuf_get_height (pb);
      int srcx, srcy, destx, desty, w2, h2;

      compute_image_scaling (w, h, win_width, win_height, verbose_p,
                             &srcx, &srcy, &destx, &desty, &w2, &h2);
      if (w != w2 || h != h2)
        {
          GdkPixbuf *pb2 = gdk_pixbuf_scale_simple (pb, w2, h2,
                                                    GDK_INTERP_BILINEAR);
          if (pb2)
            {
              gdk_pixbuf_unref (pb);
              pb = pb2;
              w = w2;
              h = h2;
            }
          else
            fprintf (stderr, "%s: out of memory when scaling?\n", progname);
        }

      clear_drawable (screen, drawable);

      /* #### Note that this always uses the default colormap!  Morons!
         Owen says that in Gnome 2.0, I should try using
         gdk_pixbuf_render_pixmap_and_mask_for_colormap() instead.
         But I haven't tried.
       */
      gdk_pixbuf_xlib_render_to_drawable_alpha (pb, drawable,
                                                srcx, srcy, destx, desty,
                                                w, h,
                                                GDK_PIXBUF_ALPHA_FULL, 127,
                                                XLIB_RGB_DITHER_NORMAL,
                                                0, 0);
      XSync (dpy, False);
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
               progname, allocated, cells);
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
            progname, image->depth);

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
            progname, image->depth);

  for (y = 0; y < image->height; y++)
    for (x = 0; x < image->width; x++)
      {
        unsigned long pixel = XGetPixel(image, x, y);
        if (pixel >= cells) abort();
        XPutPixel(image, x, y, map[pixel]);
      }
}


/* If the file has a PPM (P6) on it, read it and return an XImage.
   Otherwise, rewind the fd back to the beginning, and return 0.
 */
static XImage *
maybe_read_ppm (Screen *screen, Visual *visual,
                const char *filename, FILE *in, Bool verbose_p)
{
  Display *dpy = DisplayOfScreen (screen);
  int depth = visual_depth (screen, visual);
  struct stat st;
  char *buf = 0;
  int bufsiz = 0;
  char *s, dummy;
  int i, j;
  int x, y, w, h, maxval;
  XImage *ximage = 0;

  if (fstat (fileno (in), &st))
    goto FAIL;

  bufsiz = st.st_size;
  buf = (char *) malloc (bufsiz + 1);
  if (!buf)
    {
      fprintf (stderr, "%s: out of memory loading %d byte PPM file %s\n",
               progname, bufsiz, filename);
      goto FAIL;
    }

  if (! (s = fgets (buf, bufsiz, in)))   /* line 1 */
    goto FAIL;

  if (!strncmp (buf, "\107\111", 2))
    {
      fprintf (stderr, "%s: %s: sorry, GIF files not supported"
               " when compiled with JPEGlib instead of GDK_Pixbuf.\n",
               progname, filename);
      goto FAIL;
    }
  else if (!strncmp (buf, "\211\120", 2))
    {
      fprintf (stderr, "%s: %s: sorry, PNG files not supported"
               " when compiled with JPEGlib instead of GDK_Pixbuf.\n",
               progname, filename);
      goto FAIL;
    }

  if (strncmp (s, "P6", 2))
    goto FAIL;

  if (! (s = fgets (buf, bufsiz, in)))   /* line 2 */
    goto FAIL;
  if (2 != sscanf (s, " %d %d %c", &w, &h, &dummy))
    {
      fprintf (stderr, "%s: %s: invalid PPM (line 2)\n", progname, filename);
      goto FAIL;
    }

  if (! (s = fgets (buf, bufsiz, in)))   /* line 3 */
    goto FAIL;
  if (1 != sscanf (s, " %d %c", &maxval, &dummy))
    {
      fprintf (stderr, "%s: %s: invalid PPM (line 3)\n", progname, filename);
      goto FAIL;
    }
  if (maxval != 255)
    {
      fprintf (stderr, "%s: %s: unparsable PPM: maxval is %d\n",
               progname, filename, maxval);
      goto FAIL;
    }

  ximage = XCreateImage (dpy, visual, depth, ZPixmap, 0, 0,
                         w, h, 8, 0);
  if (ximage)
    ximage->data = (char *) calloc (ximage->height, ximage->bytes_per_line);
  if (!ximage || !ximage->data)
    {
      fprintf (stderr, "%s: out of memory loading %dx%d PPM file %s\n",
               progname, ximage->width, ximage->height, filename);
      goto FAIL;
    }

  s = buf;
  j = bufsiz;
  while ((i = fread (s, 1, j, in)) > 0)
    s += i, j -= i;

  i = 0;
  for (y = 0; y < ximage->height; y++)
    for (x = 0; x < ximage->width; x++)
      {
        unsigned char r = buf[i++];
        unsigned char g = buf[i++];
        unsigned char b = buf[i++];
        unsigned long pixel;

        if (depth > 16)
          pixel = (r << 16) | (g << 8) | b;
        else if (depth == 8)
          pixel = ((r >> 5) | ((g >> 5) << 3) | ((b >> 6) << 6));
        else if (depth == 12)
          pixel = ((r >> 4) | ((g >> 4) << 4) | ((b >> 4) << 8));
        else if (depth == 16 || depth == 15)
          pixel = (((r >> 3) << 10) | ((g >> 3) << 5) | ((b >> 3)));
        else
          abort();

        XPutPixel (ximage, x, y, pixel);
      }

  free (buf);
  return ximage;

 FAIL:
  if (buf) free (buf);
  if (ximage && ximage->data)
    {
      free (ximage->data);
      ximage->data = 0;
    }
  if (ximage) XDestroyImage (ximage);
  fseek (in, 0, SEEK_SET);
  return 0;
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
  fprintf (stderr, "%s: %s: %s\n", progname, err->filename, buf);
}


static void
jpg_error_exit (j_common_ptr cinfo)
{
  getimg_jpg_error_mgr *err = (getimg_jpg_error_mgr *) cinfo->err;
  cinfo->err->output_message (cinfo);
  draw_colorbars (err->screen, err->visual, err->drawable, err->cmap,
                  0, 0, 0, 0);
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
      fprintf (stderr, "%s: unsupported depth: %d\n", progname, depth);
      goto FAIL;
    }

  in = fopen (filename, "rb");
  if (!in)
    {
      fprintf (stderr, "%s: %s: unreadable\n", progname, filename);
      goto FAIL;
    }

  /* Check to see if it's a PPM, and if so, read that instead of using
     the JPEG library.  Yeah, this is all modular and stuff.
   */
  if ((ximage = maybe_read_ppm (screen, visual, filename, in, verbose_p)))
    {
      fclose (in);
      return ximage;
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
               progname, ximage->width, ximage->height, filename);
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
              int j = x * cinfo.num_components;
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
              else if (depth == 16 || depth == 15)
                /* Gah! I don't understand why these are in the other
                   order. */
                pixel = (((r >> 3) << 10) | ((g >> 3) << 5) | ((b >> 3)));
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


/* Scales an XImage, modifying it in place.
   If out of memory, returns False, and the XImage will have been
   destroyed and freed.
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
               progname,
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


/* Reads the given image file and renders it on the Drawable, using JPEG lib.
   Returns False if it fails.
 */
static Bool
read_file_jpeglib (Screen *screen, Window window, Drawable drawable,
                   const char *filename, Bool verbose_p)
{
  Display *dpy = DisplayOfScreen (screen);
  XImage *ximage;
  Visual *visual;
  int class, depth;
  Colormap cmap;
  unsigned int win_width, win_height, win_depth;
  int srcx, srcy, destx, desty, w2, h2;

  {
    Window root;
    int x, y;
    unsigned int bw;
    XWindowAttributes xgwa;

    XGetWindowAttributes (dpy, window, &xgwa);
    screen = xgwa.screen;
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
               progname, depth);
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

  /* Finally, put the resized image on the window.
   */
  clear_drawable (screen, drawable);
  {
    GC gc;
    XGCValues gcv;
    gc = XCreateGC (dpy, drawable, 0, &gcv);
    XPutImage (dpy, drawable, gc, ximage,
               srcx, srcy, destx, desty, ximage->width, ximage->height);
    XFreeGC (dpy, gc);
  }

  free (ximage->data);
  ximage->data = 0;
  XDestroyImage (ximage);
  XSync (dpy, False);
  return True;
}

#endif /* HAVE_JPEGLIB */


/* Reads the given image file and renders it on the Drawable.
   Returns False if it fails.
 */
static Bool
display_file (Screen *screen, Window window, Drawable drawable,
              const char *filename, Bool verbose_p)
{
  if (verbose_p)
    fprintf (stderr, "%s: loading \"%s\"\n", progname, filename);

# if defined(HAVE_GDK_PIXBUF)
  if (read_file_gdk (screen, window, drawable, filename, verbose_p))
    return True;
# elif defined(HAVE_JPEGLIB)
  if (read_file_jpeglib (screen, window, drawable, filename, verbose_p))
    return True;
# else  /* !(HAVE_GDK_PIXBUF || HAVE_JPEGLIB) */
  /* shouldn't get here if we have no image-loading methods available. */
  abort();
# endif /* !(HAVE_GDK_PIXBUF || HAVE_JPEGLIB) */

  return False;
}


/* Invokes a sub-process and returns its output (presumably, a file to
   load.)  Free the string when done.  video_p controls which program
   to run.
 */
static char *
get_filename_1 (Screen *screen, const char *directory, Bool video_p,
                Bool verbose_p)
{
  Display *dpy = DisplayOfScreen (screen);
  pid_t forked;
  int fds [2];
  int in, out;
  char buf[1024];
  char *av[20];
  int ac = 0;

  if (!video_p)
    {
      av[ac++] = GETIMAGE_FILE_PROGRAM;
      if (verbose_p)
        av[ac++] = "--verbose";
      av[ac++] = "--name";
      av[ac++] = (char *) directory;
    }
  else
    {
      av[ac++] = GETIMAGE_VIDEO_PROGRAM;
      if (verbose_p)
        av[ac++] = "--verbose";
      av[ac++] = "--name";
    }
  av[ac] = 0;

  if (verbose_p)
    {
      int i;
      fprintf (stderr, "%s: executing:", progname);
      for (i = 0; i < ac; i++)
        fprintf (stderr, " %s", av[i]);
      fprintf (stderr, "\n");
    }

  if (pipe (fds))
    {
      sprintf (buf, "%s: error creating pipe", progname);
      perror (buf);
      return 0;
    }

  in = fds [0];
  out = fds [1];

  switch ((int) (forked = fork ()))
    {
    case -1:
      {
        sprintf (buf, "%s: couldn't fork", progname);
        perror (buf);
        return 0;
      }
    case 0:
      {
        int stdout_fd = 1;

        close (in);  /* don't need this one */
        close (ConnectionNumber (dpy));		/* close display fd */

        if (dup2 (out, stdout_fd) < 0)		/* pipe stdout */
          {
            sprintf (buf, "%s: could not dup() a new stdout", progname);
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

        close (out);  /* don't need this one */
        *buf = 0;
        fgets (buf, sizeof(buf)-1, f);
        fclose (f);

        /* Wait for the child to die. */
        waitpid (-1, &wait_status, 0);

        L = strlen (buf);
        while (L && buf[L-1] == '\n')
          buf[--L] = 0;
          
        if (!*buf)
          return 0;
        if (stat(buf, &st))
          {
            fprintf (stderr, "%s: file does not exist: \"%s\"\n",
                     progname, buf);
            return 0;
          }
        else
          return strdup (buf);
      }
    }

  abort();
}


/* Returns a pathname to an image file.  Free the string when you're done.
 */
static char *
get_filename (Screen *screen, const char *directory, Bool verbose_p)
{
  return get_filename_1 (screen, directory, False, verbose_p);
}


/* Grabs a video frame to a file, and returns a pathname to that file.
   Delete that file when you are done with it (and free the string.)
 */
static char *
get_video_filename (Screen *screen, Bool verbose_p)
{
  return get_filename_1 (screen, 0, True, verbose_p);
}


/* Grabs a video frame, and renders it on the Drawable.
   Returns False if it fails;
 */
static Bool
display_video (Screen *screen, Window window, Drawable drawable,
               Bool verbose_p)
{
  char *filename = get_video_filename (screen, verbose_p);
  Bool status;

  if (!filename)
    {
      if (verbose_p)
        fprintf (stderr, "%s: video grab failed.\n", progname);
      return False;
    }

  status = display_file (screen, window, drawable, filename, verbose_p);

  if (unlink (filename))
    {
      char buf[512];
      sprintf (buf, "%s: rm %.100s", progname, filename);
      perror (buf);
    }
  else if (verbose_p)
    fprintf (stderr, "%s: rm %s\n", progname, filename);

  if (filename) free (filename);
  return status;
}


/* Grabs an image (from a file, video, or the desktop) and renders it on
   the Drawable.  If `file' is specified, always use that file.  Otherwise,
   select randomly, based on the other arguments.
 */
static void
get_image (Screen *screen,
           Window window, Drawable drawable,
           Bool verbose_p,
           Bool desk_p,
           Bool video_p,
           Bool image_p,
           const char *dir,
           const char *file)
{
  Display *dpy = DisplayOfScreen (screen);
  enum { do_desk, do_video, do_image, do_bars } which = do_bars;
  int count = 0;
  struct stat st;

  if (! drawable_window_p (dpy, window))
    {
      fprintf (stderr, "%s: 0x%lx is a pixmap, not a window!\n",
               progname, (unsigned long) window);
      exit (1);
    }

  if (file && stat (file, &st))
    {
      fprintf (stderr, "%s: file \"%s\" does not exist\n", progname, file);
      file = 0;
    }

  if (verbose_p)
    {
      fprintf (stderr, "%s: grabDesktopImages:  %s\n",
               progname, desk_p ? "True" : "False");
      fprintf (stderr, "%s: grabVideoFrames:    %s\n",
               progname, video_p ? "True" : "False");
      fprintf (stderr, "%s: chooseRandomImages: %s\n",
               progname, image_p ? "True" : "False");
      fprintf (stderr, "%s: imageDirectory:     %s\n",
               progname, (file ? file : dir ? dir : ""));
    }

# if !(defined(HAVE_GDK_PIXBUF) || defined(HAVE_JPEGLIB))
  image_p = False;    /* can't load images from files... */
  if (file)
    {
      fprintf (stderr,
               "%s: image file loading not available at compile-time\n",
               progname);
      fprintf (stderr, "%s: can't load \"%s\"\n", progname, file);
      file = 0;
    }
# endif /* !(HAVE_GDK_PIXBUF || HAVE_JPEGLIB) */

  if (file)
    {
      desk_p = False;
      video_p = False;
      image_p = True;
    }

  if (!dir || !*dir)
    {
      if (verbose_p && image_p)
        fprintf (stderr,
                 "%s: no imageDirectory: turning off chooseRandomImages.\n",
                 progname);
      image_p = False;
    }


# ifndef _VROOT_H_
#  error Error!  This file definitely needs vroot.h!
# endif

  /* We can grab desktop images if:
       - the window is the real root window;
       - the window is the virtal root window;
       - the window is a toplevel window.
     We cannot grab desktop images if:
       - the window is a non-top-level window.
   */
  if (desk_p)
    {
      if (!top_level_window_p (screen, window))
        {
          desk_p = False;
          if (verbose_p)
            fprintf (stderr,
                    "%s: 0x%x not top-level: turning off grabDesktopImages.\n",
                     progname, (unsigned int) window);
        }
    }

  count = 0;
  if (desk_p)  count++;
  if (video_p) count++;
  if (image_p) count++;

  if (count == 0)
    which = do_bars;
  else
    {
      int i = 0;
      while (1)  /* loop until we get one that's permitted */
        {
          which = (random() % 3);
          if (which == do_desk  && desk_p)  break;
          if (which == do_video && video_p) break;
          if (which == do_image && image_p) break;
          if (++i > 200) abort();
        }
    }


  /* If we're to search a directory to find an image file, do so now.
   */
  if (which == do_image && !file)
    {
      file = get_filename (screen, dir, verbose_p);
      if (!file)
        {
          which = do_bars;
          if (verbose_p)
            fprintf (stderr, "%s: no image files found.\n", progname);
        }
    }

  /* Now actually render something.
   */
  if (which == do_bars)
    {
      XWindowAttributes xgwa;
    COLORBARS:
      if (verbose_p)
        fprintf (stderr, "%s: drawing colorbars.\n", progname);
      XGetWindowAttributes (dpy, window, &xgwa);
      draw_colorbars (screen, xgwa.visual, drawable, xgwa.colormap,
                      0, 0, 0, 0);
      XSync (dpy, False);
    }
  else if (which == do_desk)
    {
      GC gc;
      XGCValues gcv;
      XWindowAttributes xgwa;

      if (verbose_p)
        {
          fprintf (stderr, "%s: grabbing desktop image\n", progname);
          grabscreen_verbose();
        }
      gc = XCreateGC (dpy, drawable, 0, &gcv);
      XGetWindowAttributes (dpy, window, &xgwa);
      grab_screen_image (screen, window);
      XCopyArea (dpy, window, drawable, gc,
                 0, 0, xgwa.width, xgwa.height, 0, 0);
      XFreeGC (dpy, gc);
      XSync (dpy, False);
    }
  else if (which == do_image)
    {
      if (! display_file (screen, window, drawable, file, verbose_p))
        goto COLORBARS;
    }
  else if (which == do_video)
    {
      if (! display_video (screen, window, drawable, verbose_p))
        goto COLORBARS;
    }
  else
    abort();
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
   "      -display host:dpy.screen    which display to use\n"		      \
   "      -root                       draw to the root window\n"	      \
   "      -verbose                    print diagnostics\n"		      \
   "      -images  / -no-images       whether to allow image file loading\n"  \
   "      -video   / -no-video        whether to allow video grabs\n"	      \
   "      -desktop / -no-desktop      whether to allow desktop screen grabs\n"\
   "      -directory <path>           where to find image files to load\n"    \
   "      -file <filename>            load this image file\n"                 \
   "\n"									      \
   "    The XScreenSaver Control Panel (xscreensaver-demo) lets you set the\n"\
   "    defaults for these options in your ~/.xscreensaver file.\n"           \
   "\n"

int
main (int argc, char **argv)
{
  saver_preferences P;
  Widget toplevel;
  Display *dpy;
  Screen *screen;
  char *oprogname = progname;
  char *file = 0;
  char version[255];

  Window window = (Window) 0;
  Drawable drawable = (Drawable) 0;
  const char *window_str = 0;
  const char *drawable_str = 0;
  char *s;
  int i;

  progname = argv[0];
  s = strrchr (progname, '/');
  if (s) progname = s+1;
  oprogname = progname;

  /* half-assed way of avoiding buffer-overrun attacks. */
  if (strlen (progname) >= 100) progname[100] = 0;

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

  toplevel = XtAppInitialize (&app, progclass, 0, 0, &argc, argv,
                              defaults, 0, 0);
  dpy = XtDisplay (toplevel);
  screen = XtScreen (toplevel);
  db = XtDatabase (dpy);
  XtGetApplicationNameAndClass (dpy, &s, &progclass);
  XSetErrorHandler (x_ehandler);
  XSync (dpy, False);

  /* Randomize -- only need to do this here because this program
     doesn't use the `screenhack.h' or `lockmore.h' APIs. */
# undef ya_rand_init
  ya_rand_init (0);

  memset (&P, 0, sizeof(P));
  P.db = db;
  load_init_file (&P);

  progname = argv[0] = oprogname;

  for (i = 1; i < argc; i++)
    {
      unsigned long w;
      char dummy;

      /* Have to re-process these, or else the .xscreensaver file
         has priority over the command line...
       */
      if (!strcmp (argv[i], "-v") || !strcmp (argv[i], "-verbose"))
        P.verbose_p = True;
      else if (!strcmp (argv[i], "-desktop"))    P.grab_desktop_p = True;
      else if (!strcmp (argv[i], "-no-desktop")) P.grab_desktop_p = False;
      else if (!strcmp (argv[i], "-video"))      P.grab_video_p = True;
      else if (!strcmp (argv[i], "-no-video"))   P.grab_video_p = False;
      else if (!strcmp (argv[i], "-images"))     P.random_image_p = True;
      else if (!strcmp (argv[i], "-no-images"))  P.random_image_p = False;
      else if (!strcmp (argv[i], "-file"))       file = argv[++i];
      else if (!strcmp (argv[i], "-directory") || !strcmp (argv[i], "-dir"))
        P.image_directory = argv[++i];
      else if (!strcmp (argv[i], "-root") || !strcmp (argv[i], "root"))
        {
          if (window)
            {
              fprintf (stderr, "%s: both %s and %s specified?\n",
                       progname, argv[i], window_str);
              goto LOSE;
            }
          window_str = argv[i];
          window = (Window) RootWindowOfScreen (screen);
        }
      else if ((1 == sscanf (argv[i], " 0x%lx %c", &w, &dummy) ||
                1 == sscanf (argv[i], " %lu %c",   &w, &dummy)) &&
               w != 0)
        {
          if (drawable)
            {
              fprintf (stderr, "%s: both %s and %s specified?\n",
                       progname, drawable_str, argv[i]);
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
      else
        {
          if (argv[i][0] == '-')
            fprintf (stderr, "\n%s: unknown option \"%s\"\n",
                     progname, argv[i]);
          else
            fprintf (stderr, "\n%s: unparsable window/pixmap ID: \"%s\"\n",
                     progname, argv[i]);
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
      fprintf (stderr, "\n%s: no window ID specified!\n", progname);
      goto LOSE;
    }


#ifdef DEBUG
  if (P.verbose_p)       /* Print out all the resources we can see. */
    {
      XrmName name = { 0 };
      XrmClass class = { 0 };
      int count = 0;
      XrmEnumerateDatabase (db, &name, &class, XrmEnumAllLevels, mapper,
                            (XtPointer) &count);
    }
#endif /* DEBUG */

  if (!window) abort();
  if (!drawable) drawable = window;

  get_image (screen, window, drawable, P.verbose_p,
             P.grab_desktop_p, P.grab_video_p, P.random_image_p,
             P.image_directory, file);
  exit (0);
}
