/* xpm-ximage.c --- converts XPM data to an XImage for use with OpenGL.*/

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)xpm-image.c       5.0 00/11/14 xlockmore";

#endif
/*
 * xscreensaver, Copyright (c) 1998 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Version extracted from xscreensaver 3.26 for xlockmore
 * by Eric Lassauge <lassauge@mail.dotcom.fr> http://lassauge.free.fr/linux.html
 * Parts of the code are from Jamie, the rest dealing with alpha channel is from me.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef STANDALONE
#error "Can't make that standalone !!!"
#else                           /* !STANDALONE */
#include "xlock.h"              /* from the xlockmore distribution */
#include "vis.h"
#endif                          /* !STANDALONE */

#include <X11/Intrinsic.h>


#if defined( USE_XPM ) || defined( USE_XPMINC ) /* whole file */
#if USE_XPMINC
#include <xpm.h>
#else
#include <X11/xpm.h>            /* Normal spot */
#endif /* USE_XPMINC */

#include <X11/Xutil.h>

static Bool
bigendian (void)
{
  union { int i; char c[sizeof(int)]; } u;
  u.i = 1;
  return !u.c[0];
}


/* Returns an XImage structure containing the bits of the given XPM image.
   This XImage will be 32 bits per pixel, 8 each per R, G, and B, with the
   extra byte set to 0xFF or 0x00 dealing with alpha value.

   The Display and Visual arguments are used only for creating the XImage;
   no bits are pushed to the server.

   The Colormap argument is used just for parsing color names; no colors
   are allocated.
 */
XImage *
xpm_to_ximage (Display *dpy, Visual *visual, Colormap cmap, char **xpm_data)
{
  /* All we want to do is get RGB data out of the XPM file built in to this
     program.  This is a pain, because there is no way  (as of XPM version
     4.6, at least) to get libXpm to make an XImage without also allocating
     colors with XAllocColor.  So, instead, we create an XpmImage and parse
     out the RGB values of the pixels ourselves; and construct an XImage
     by hand.  Regardless of the depth of the visual we're using, this
     XImage will have 32 bits per pixel, 8 each per R, G, and B.  We put
     0xFF or 0x00 in the fourth slot, as GL will interpret that as "alpha".
     Alpha is detected using "None" color of XPM file.
   */
  XImage *ximage;
  XpmImage xpm_image;
  XpmInfo xpm_info;
  int result, trans_idx = 0;
  int x, y, i;
  int bpl, wpl;
  XColor colors[256];

  memset (&xpm_image, 0, sizeof(xpm_image));
  memset (&xpm_info, 0, sizeof(xpm_info));
  result = XpmCreateXpmImageFromData (xpm_data, &xpm_image, &xpm_info);
  if (result != XpmSuccess)
  {
      (void) fprintf(stderr, "Unable to parse xpm data (%d).\n",
	      result);
      return (XImage *)NULL;
  }

  if (xpm_image.ncolors > 256)
  {
      (void) fprintf(stderr, "Unable to use xpm datas with too much colors (%d).\n",
	      xpm_image.ncolors);
      return (XImage *)NULL;
  }

  ximage = XCreateImage (dpy, visual, 32, ZPixmap, 0, 0,
			 xpm_image.width, xpm_image.height, 32, 0);

  bpl = ximage->bytes_per_line;
  wpl = bpl/4;

  if ( (ximage->data = (char *) malloc(xpm_image.height * bpl)) == NULL)
  {
        /* free everything up to now */
        XDestroyImage(ximage);
        return (XImage *)NULL;
  }

  /* Parse the colors in the XPM into RGB values. */
  for (i = 0; i < (int) xpm_image.ncolors; i++)
  {
    /* Before throwing an error, check if it is the "None" color */
    if (xpm_image.colorTable[i].c_color && (!strncasecmp(xpm_image.colorTable[i].c_color,"none",4)))
    {
       trans_idx = i;
       colors[trans_idx].red   = 0xFF;
       colors[trans_idx].green = 0xFF;
       colors[trans_idx].blue  = 0xFF;
    }
    else if (!XParseColor(dpy, cmap, xpm_image.colorTable[i].c_color, &colors[i]))
    {
	(void) fprintf(stderr,"Unparsable color: %s\n",
		xpm_image.colorTable[i].c_color);
        /* free everything up to now */
        free(ximage->data);
        XDestroyImage(ximage);
        return (XImage *)NULL;
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

    for (y = 0; y < (int) xpm_image.height; y++)
      {
	int y2 = (xpm_image.height-1-y); /* Texture maps are upside down. */

	unsigned int *oline = (unsigned int *) (ximage->data   + (y  * bpl));
	unsigned int *iline = (unsigned int *) (xpm_image.data + (y2 * wpl));

	for (x = 0; x < (int) xpm_image.width; x++)
	  {
	    XColor *c = &colors[iline[x]];
            /* Again, check for transparent color */
            if ((int) iline[x] == trans_idx)
	       /* pack it as RGBA transparent */
	       oline[x] = (((c->red   >> 8) << rpos) |
			   ((c->green >> 8) << gpos) |
			   ((c->blue  >> 8) << bpos) |
			   (0x00            << apos));
            else
	       /* pack it as RGBA */
	       oline[x] = (((c->red   >> 8) << rpos) |
			   ((c->green >> 8) << gpos) |
			   ((c->blue  >> 8) << bpos) |
			   (0xFF            << apos));
	  }
      }
  }

  /* I sure hope these only free the contents, and not the args. */
  XpmFreeXpmImage (&xpm_image);
  XpmFreeXpmInfo (&xpm_info);

  return ximage;
}


#else  /* !HAVE_XPM */

XImage *
xpm_to_ximage (Display *dpy, Visual *visual, Colormap cmap, char **xpm_data)
{
  (void) fprintf(stderr, "Not compiled with XPM support.\n");
  return (XImage *)NULL;
}

#endif /* !HAVE_XPM */
