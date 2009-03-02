/* xpm-ximage.c --- converts XPM data to an XImage for use with OpenGL.
 * xscreensaver, Copyright (c) 1998, 2001, 2002 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Alpha channel support by Eric Lassauge <lassauge@mail.dotcom.fr>.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>

extern char *progname;


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

static Bool
bigendian (void)
{
  union { int i; char c[sizeof(int)]; } u;
  u.i = 1;
  return !u.c[0];
}


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
      if (! XpmReadFileToData ((char *) filename, &xpm_data))
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
    if (bigendian())
      rpos = 24, gpos = 16, bpos =  8, apos =  0;
    else
      rpos =  0, gpos =  8, bpos = 16, apos = 24;

    for (y = 0; y < xpm_image.height; y++)
      {
	int y2 = (xpm_image.height-1-y); /* Texture maps are upside down. */

	unsigned int *oline = (unsigned int *) (ximage->data   + (y  * bpl));
	unsigned int *iline = (unsigned int *) (xpm_image.data + (y2 * wpl));

	for (x = 0; x < xpm_image.width; x++)
	  {
	    XColor *c = &colors[iline[x]];
            int alpha = ((iline[x] == transparent_color_index) ? 0x00 : 0xFF);
            oline[x] = (((c->red   >> 8) << rpos) |
                        ((c->green >> 8) << gpos) |
                        ((c->blue  >> 8) << bpos) |
                        (alpha           << apos));
	  }
      }
  }

  /* I sure hope these only free the contents, and not the args. */
#if 0  /* Apparently not?  Gotta love those well-documented APIs! */
  XpmFreeXpmImage (&xpm_image);
  XpmFreeXpmInfo (&xpm_info);
#endif

  return ximage;
}


#else  /* !HAVE_XPM && !HAVE_GDK_PIXBUF */

static XImage *
xpm_to_ximage_1 (Display *dpy, Visual *visual, Colormap cmap,
                 const char *filename, char **xpm_data)
{
  fprintf(stderr, "%s: not compiled with XPM or Pixbuf support.\n", progname);
  exit (1);
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
xpm_to_ximage (Display *dpy, Visual *visual, Colormap cmap, char **xpm_data)
{
  return xpm_to_ximage_1 (dpy, visual, cmap, 0, xpm_data);
}


XImage *
xpm_file_to_ximage (Display *dpy, Visual *visual, Colormap cmap,
                    const char *filename)
{
  return xpm_to_ximage_1 (dpy, visual, cmap, filename, 0);
}
