/* xscreensaver, Copyright (c) 2014 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* Compatibility layer using XDrawString, XDrawString16() or Xutf8DrawString().
   This layer is used by X11 systems without Xft, and by MacOS / iOS.
 */

#ifndef __XSCREENSAVER_XFT_H__
#define __XSCREENSAVER_XFT_H__

# ifdef HAVE_XFT

#  include <X11/Xft/Xft.h>

# else  /* !HAVE_XFT -- whole file */

# ifdef HAVE_COCOA
#  include "jwxyz.h"
#elif defined(HAVE_ANDROID)
#  include "jwxyz.h"
# else
#  include <X11/Xlib.h>
# endif

/* This doesn't seem to work right.  See comment in xft.c. */
# undef HAVE_XUTF8DRAWSTRING


# ifndef _Xconst
#  define _Xconst const
# endif

typedef struct _XGlyphInfo {
  unsigned short width, height;     /* bounding box of the ink */
  short x, y;		/* distance from upper left of bbox to glyph origin. */
  short xOff, yOff;	/* distance from glyph origin to next origin. */

  /* These field names and values are, of course, arbitrarily different
     from XCharStruct for no sensible reason.  Here's a translation key
     between the two:

     XGlyphInfo from XCharStruct:

       g.x         =  -c.lbearing;
       g.y         =   c.ascent;
       g.xOff      =   c.width;
       g.yOff      =   0;
       g.width     =   c.rbearing - c.lbearing;
       g.height    =   c.ascent   + c.descent;
     
     XCharStruct from XGlyphInfo:

       c.lbearing  =  -g.x;
       c.rbearing  =   g.width - g.x;
       c.ascent    =   g.y;
       c.descent   =   g.height - g.y;
       c.width     =   g.xOff;
   */

} XGlyphInfo;


typedef struct _XftFont {
  XFontStruct *xfont;
# ifdef HAVE_XUTF8DRAWSTRING
  XFontSet fontset;
# endif
  char *name;
  int ascent;
  int descent;
  int height;
} XftFont;

typedef struct {
  unsigned short   red;
  unsigned short   green;
  unsigned short   blue;
  unsigned short   alpha;
} XRenderColor;

typedef struct _XftColor {
  unsigned long pixel;
  XRenderColor color;
} XftColor;

typedef struct _XftDraw XftDraw;

typedef unsigned char FcChar8;


XftFont *XftFontOpenXlfd (Display *dpy, int screen, _Xconst char *xlfd);

void XftFontClose (Display *dpy, XftFont *font);

Bool XftColorAllocName (Display  *dpy,
                        _Xconst Visual *visual,
                        Colormap cmap,
                        _Xconst char *name,
                        XftColor *result);

Bool XftColorAllocValue (Display *dpy,
                         _Xconst Visual *visual,
                         Colormap cmap,
                         _Xconst XRenderColor *color,
                         XftColor *result);

void XftColorFree (Display *dpy,
                   Visual *visual,
                   Colormap cmap,
                   XftColor *color);

XftDraw *XftDrawCreate (Display   *dpy,
                        Drawable  drawable,
                        Visual    *visual,
                        Colormap  colormap);

void XftDrawDestroy (XftDraw *draw);

void
XftTextExtentsUtf8 (Display	    *dpy,
		    XftFont	    *pub,
		    _Xconst FcChar8 *string,
		    int		    len,
		    XGlyphInfo	    *extents);

void
XftDrawStringUtf8 (XftDraw	    *draw,
		   _Xconst XftColor *color,
		   XftFont	    *pub,
		   int		    x,
		   int		    y,
		   _Xconst FcChar8  *string,
		   int		    len);

# endif /* !HAVE_XFT */

#endif /* __XSCREENSAVER_XFT_H__ */
