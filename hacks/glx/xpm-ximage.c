/* xpm-ximage.c --- converts XPM data to an XImage for use with OpenGL.
 * xscreensaver, Copyright (c) 1998-2008 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Alpha channel support by Eric Lassauge <lassauge@users.sourceforge.net>
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
#endif

#include "xpm-ximage.h"

extern char *progname;


#if defined(HAVE_GDK_PIXBUF) || defined(HAVE_XPM)
static Bool
bigendian (void)
{
  union { int i; char c[sizeof(int)]; } u;
  u.i = 1;
  return !u.c[0];
}
#endif /* HAVE_GDK_PIXBUF || HAVE_XPM */


#if defined(HAVE_GDK_PIXBUF)

# include <gdk-pixbuf/gdk-pixbuf.h>

# ifdef HAVE_GTK2
#  include <gdk-pixbuf-xlib/gdk-pixbuf-xlib.h>
# else  /* !HAVE_GTK2 */
#  include <gdk-pixbuf/gdk-pixbuf-xlib.h>
# endif /* !HAVE_GTK2 */


/* Returns an XImage structure containing the bits of the given XPM image.
   This XImage will be 32 bits per pixel, 8 each per R, G, and B, with the
   extra byte set to either 0xFF or 0x00 (based on the XPM file's mask.)

   The Display and Visual arguments are used only for creating the XImage;
   no bits are pushed to the server.

   The Colormap argument is used just for parsing color names; no colors
   are allocated.

   This is the gdk_pixbuf version of this function.
 */
static XImage *
xpm_to_ximage_1 (Display *dpy, Visual *visual, Colormap cmap,
                 const char *filename,
                 char **xpm_data)
{
  GdkPixbuf *pb;
  static int initted = 0;
#ifdef HAVE_GTK2
  GError *gerr = NULL;
#endif

  if (!initted)
    {
#ifdef HAVE_GTK2
      g_type_init ();
#endif
      gdk_pixbuf_xlib_init (dpy, DefaultScreen (dpy));
      xlib_rgb_init (dpy, DefaultScreenOfDisplay (dpy));
      initted = 1;
    }

  pb = (filename
#ifdef HAVE_GTK2
	? gdk_pixbuf_new_from_file (filename, &gerr)
#else
        ? gdk_pixbuf_new_from_file (filename)
#endif /* HAVE_GTK2 */
        : gdk_pixbuf_new_from_xpm_data ((const char **) xpm_data));
  if (pb)
    {
      XImage *image;
      int w = gdk_pixbuf_get_width (pb);
      int h = gdk_pixbuf_get_height (pb);
      guchar *row = gdk_pixbuf_get_pixels (pb);
      int stride = gdk_pixbuf_get_rowstride (pb);
      int chan = gdk_pixbuf_get_n_channels (pb);
      int x, y;

      image = XCreateImage (dpy, visual, 32, ZPixmap, 0, 0, w, h, 32, 0);
      image->data = (char *) malloc(h * image->bytes_per_line);

      /* Set the bit order in the XImage structure to whatever the
         local host's native bit order is.
       */
      image->bitmap_bit_order =
        image->byte_order =
          (bigendian() ? MSBFirst : LSBFirst);


      if (!image->data)
        {
          fprintf (stderr, "%s: out of memory (%d x %d)\n", progname, w, h);
          exit (1);
        }

      for (y = 0; y < h; y++)
        {
          int y2 = (h-1-y); /* Texture maps are upside down. */

          guchar *i = row;
          for (x = 0; x < w; x++)
            {
              unsigned long rgba = 0;
              switch (chan) {
              case 1:
                rgba = ((0xFF << 24) |
                        (*i << 16) |
                        (*i << 8) |
                         *i);
                i++;
                break;
              case 3:
                rgba = ((0xFF << 24) |
                        (i[2] << 16) |
                        (i[1] << 8) |
                         i[0]);
                i += 3;
                break;
              case 4:
                rgba = ((i[3] << 24) |
                        (i[2] << 16) |
                        (i[1] << 8) |
                         i[0]);
                i += 4;
                break;
              default:
                abort();
                break;
              }
              XPutPixel (image, x, y2, rgba);
            }
          row += stride;
        }
      gdk_pixbuf_unref (pb); /* #### does doing this free colors? */

      return image;
    }
  else if (filename)
    {
#ifdef HAVE_GTK2
      fprintf (stderr, "%s: %s\n", progname, gerr->message);
      g_error_free (gerr);
#else
      fprintf (stderr, "%s: unable to load %s\n", progname, filename);
#endif /* HAVE_GTK2 */
      exit (1);
    }
  else
    {
      fprintf (stderr, "%s: unable to initialize builtin texture\n", progname);
      exit (1);
    }
}


#elif defined(HAVE_XPM)


#include <stdlib.h>
#include <stdio.h>
#include <X11/Intrinsic.h>

#include <X11/Xutil.h>
#include <X11/xpm.h>

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))



/* The libxpm version of this function...
 */
static XImage *
xpm_to_ximage_1 (Display *dpy, Visual *visual, Colormap cmap,
                 const char *filename, char **xpm_data)
{
  /* All we want to do is get RGB data out of the XPM file built in to this
     program.  This is a pain, because there is no way  (as of XPM version
     4.6, at least) to get libXpm to make an XImage without also allocating
     colors with XAllocColor.  So, instead, we create an XpmImage and parse
     out the RGB values of the pixels ourselves; and construct an XImage
     by hand.  Regardless of the depth of the visual we're using, this
     XImage will have 32 bits per pixel, 8 each per R, G, and B.  We put
     0xFF or 0x00 in the fourth (alpha) slot, depending on the file's mask.
   */
  XImage *ximage = 0;
  XpmImage xpm_image;
  XpmInfo xpm_info;
  int result;
  int transparent_color_index = -1;
  int x, y, i;
  int bpl, wpl;
  XColor colors[256];

  memset (&xpm_image, 0, sizeof(xpm_image));
  memset (&xpm_info, 0, sizeof(xpm_info));

  if (filename)
    {
      xpm_data = 0;
      if (XpmSuccess != XpmReadFileToData ((char *) filename, &xpm_data))
        {
          fprintf (stderr, "%s: unable to read XPM file %s\n",
                   progname, filename);
          exit (1);
        }
    }

  result = XpmCreateXpmImageFromData (xpm_data, &xpm_image, &xpm_info);
  if (result != XpmSuccess)
    {
      fprintf(stderr, "%s: unable to parse xpm data (%d).\n", progname,
	      result);
      exit (1);
    }

  if (xpm_image.ncolors > countof(colors))
    {
      fprintf (stderr, "%s: too many colors (%d) in XPM.\n",
               progname, xpm_image.ncolors);
      exit (1);
    }

  ximage = XCreateImage (dpy, visual, 32, ZPixmap, 0, 0,
			 xpm_image.width, xpm_image.height, 32, 0);

  bpl = ximage->bytes_per_line;
  wpl = bpl/4;

  ximage->data = (char *) malloc(xpm_image.height * bpl);

  /* Parse the colors in the XPM into RGB values. */
  for (i = 0; i < xpm_image.ncolors; i++)
    {
      const char *c = xpm_image.colorTable[i].c_color;
      if (!c)
        {
          fprintf(stderr, "%s: bogus color table?  (%d)\n", progname, i);
          exit (1);
        }
      else if (!strncasecmp (c, "None", 4))
        {
          transparent_color_index = i;
          colors[transparent_color_index].red   = 0xFF;
          colors[transparent_color_index].green = 0xFF;
          colors[transparent_color_index].blue  = 0xFF;
        }
      else if (!XParseColor (dpy, cmap, c, &colors[i]))
        {
          fprintf(stderr, "%s: unparsable color: %s\n", progname, c);
          exit (1);
        }
    }

  /* Translate the XpmImage to an RGB XImage. */
  {
    int rpos, gpos, bpos, apos;  /* bitfield positions */

    /* Note that unlike X, which is endianness-agnostic (since any XImage
       can have its own specific bit ordering, with the server reversing
       things as necessary) OpenGL pretends everything is client-side, so
       we need to pack things in the right order for the client machine.
     */

    ximage->bitmap_bit_order =
      ximage->byte_order =
        (bigendian() ? MSBFirst : LSBFirst);

#if 0
    /* #### Cherub says that the little-endian case must be taken on MacOSX,
            or else the colors/alpha are the wrong way around.  How can
            that be the case?
     */
    if (bigendian())
      rpos = 24, gpos = 16, bpos =  8, apos =  0;
    else
#endif
      rpos =  0, gpos =  8, bpos = 16, apos = 24;

  }

  /* I sure hope these only free the contents, and not the args. */
#if 0  /* Apparently not?  Gotta love those well-documented APIs! */
  XpmFreeXpmImage (&xpm_image);
  XpmFreeXpmInfo (&xpm_info);
#endif

  return ximage;
}


#else  /* !HAVE_XPM && !HAVE_GDK_PIXBUF */

/* If we don't have libXPM or Pixbuf, then use "minixpm".
   This can read XPM data from memory, but can't read files.
 */

#include <stdlib.h>
#include <stdio.h>
#include "minixpm.h"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))


/* Given a bitmask, returns the position and width of the field.
 */
static void
decode_mask (unsigned int mask, unsigned int *pos_ret, unsigned int *size_ret)
{
  int i;
  for (i = 0; i < 32; i++)
    if (mask & (1L << i))
      {
        int j = 0;
        *pos_ret = i;
        for (; i < 32; i++, j++)
          if (! (mask & (1L << i)))
            break;
        *size_ret = j;
        return;
      }
}


/* The minixpm version of this function...
 */
static XImage *
xpm_to_ximage_1 (Display *dpy, Visual *visual, Colormap cmap,
                 const char *filename, char **xpm_data)
{
  int iw, ih, w8, x, y;
  XImage *ximage;
  char *data;
  unsigned char *mask = 0;
  int depth = 32;
  unsigned long background_color =
    BlackPixelOfScreen (DefaultScreenOfDisplay (dpy));
  unsigned long *pixels = 0;
  int npixels = 0;
  int bpl;

  unsigned int rpos=0, gpos=0, bpos=0, apos=0;
  unsigned int rmsk=0, gmsk=0, bmsk=0, amsk=0;
  unsigned int rsiz=0, gsiz=0, bsiz=0, asiz=0;

  if (filename)
    {
      fprintf(stderr, 
              "%s: no files: not compiled with XPM or Pixbuf support.\n", 
              progname);
      exit (1);
    }

  if (! xpm_data) abort();
  ximage = minixpm_to_ximage (dpy, visual, cmap, depth, background_color,
                              (const char * const *) xpm_data,
                              &iw, &ih, &pixels, &npixels, &mask);
  if (pixels) free (pixels);
  
  bpl = ximage->bytes_per_line;
  data = ximage->data;
  ximage->data = malloc (ximage->height * bpl);
  
  /* Flip image upside down, for texture maps; 
     process the mask; and re-arrange the color components for GL.
   */
  w8 = (ximage->width + 7) / 8;

  rmsk = ximage->red_mask;
  gmsk = ximage->green_mask;
  bmsk = ximage->blue_mask;
  amsk = ~(rmsk|gmsk|bmsk);

  decode_mask (rmsk, &rpos, &rsiz);
  decode_mask (gmsk, &gpos, &gsiz);
  decode_mask (bmsk, &bpos, &bsiz);
  decode_mask (amsk, &apos, &asiz);

  for (y = 0; y < ximage->height; y++)
    {
      int y2 = (ximage->height-1-y);
    
      unsigned int *oline = (unsigned int *) (ximage->data + (y  * bpl));
      unsigned int *iline = (unsigned int *) (data         + (y2 * bpl));
    
      for (x = 0; x < ximage->width; x++)
        {
          unsigned long pixel = iline[x];
          unsigned char r = (pixel & rmsk) >> rpos;
          unsigned char g = (pixel & gmsk) >> gpos;
          unsigned char b = (pixel & bmsk) >> bpos;
          unsigned char a = (mask
                             ? ((mask [(y2 * w8) + (x >> 3)] & (1 << (x % 8)))
                                ? 0xFF : 0)
                             : 0xFF);
# if 0
          pixel = ((r << rpos) | (g << gpos) | (b << bpos) | (a << apos));
# else
          pixel = ((a << 24) | (b << 16) | (g <<  8) | r);
# endif
          oline[x] = pixel;
        }
    }
  free (data);

  return ximage;
}

#endif /* !HAVE_XPM */


/* Returns an XImage structure containing the bits of the given XPM image.
   This XImage will be 32 bits per pixel, 8 each per R, G, and B, with the
   extra byte set to either 0xFF or 0x00 (based on the XPM file's mask.)

   The Display and Visual arguments are used only for creating the XImage;
   no bits are pushed to the server.

   The Colormap argument is used just for parsing color names; no colors
   are allocated.
 */
XImage *
xpm_to_ximage (Display *dpy, Visual *visual, Colormap cmap, 
               char **xpm_data)
{
  return xpm_to_ximage_1 (dpy, visual, cmap, 0, xpm_data);
}


XImage *
xpm_file_to_ximage (Display *dpy, Visual *visual, Colormap cmap,
                    const char *filename)
{
  return xpm_to_ximage_1 (dpy, visual, cmap, filename, 0);
}
