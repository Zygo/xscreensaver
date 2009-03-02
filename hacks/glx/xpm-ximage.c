/* xpm-ximage.c --- converts XPM data to an XImage for use with OpenGL.
 * xscreensaver, Copyright (c) 1998 Jamie Zawinski <jwz@jwz.org>
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

#ifdef HAVE_XPM		/* whole file */

#include <stdlib.h>
#include <stdio.h>
#include <X11/Intrinsic.h>
#include <X11/Xutil.h>
#include <X11/xpm.h>

extern char *progname;

static Bool
bigendian (void)
{
  union { int i; char c[sizeof(int)]; } u;
  u.i = 1;
  return !u.c[0];
}


/* Returns an XImage structure containing the bits of the given XPM image.
   This XImage will be 32 bits per pixel, 8 each per R, G, and B, with the
   extra byte set to 0xFF.

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
     0xFF in the fourth slot, as GL will interpret that as "alpha".
   */
  XImage *ximage = 0;
  XpmImage xpm_image;
  XpmInfo xpm_info;
  int result;
  int x, y, i;
  int bpl, wpl;
  XColor colors[255];

  result = XpmCreateXpmImageFromData (xpm_data, &xpm_image, &xpm_info);
  if (result != XpmSuccess)
    {
      fprintf(stderr, "%s: unable to parse xpm data (%d).\n", progname,
	      result);
      exit (1);
    }

  ximage = XCreateImage (dpy, visual, 32, ZPixmap, 0, 0,
			 xpm_image.width, xpm_image.height, 32, 0);

  bpl = ximage->bytes_per_line;
  wpl = bpl/4;

  ximage->data = (char *) malloc(xpm_image.height * bpl);

  /* Parse the colors in the XPM into RGB values. */
  for (i = 0; i < xpm_image.ncolors; i++)
    if (!XParseColor(dpy, cmap, xpm_image.colorTable[i].c_color, &colors[i]))
      {
	fprintf(stderr, "%s: unparsable color: %s\n", progname,
		xpm_image.colorTable[i].c_color);
	exit(1);
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
	    /* pack it as RGBA */
	    oline[x] = (((c->red   >> 8) << rpos) |
			((c->green >> 8) << gpos) |
			((c->blue  >> 8) << bpos) |
			(0xFF            << apos));
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


#else  /* !HAVE_XPM */

static XImage *
xpm_to_ximage (char **xpm_data)
{
  fprintf(stderr, "%s: not compiled with XPM support.\n", progname);
  exit (1);
}

#endif /* !HAVE_XPM */
