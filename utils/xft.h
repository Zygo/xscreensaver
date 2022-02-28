/* xscreensaver, Copyright (c) 2014-2015 Jamie Zawinski <jwz@jwz.org>
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

/* The XGlyphInfo field names and values are, of course, arbitrarily
   different from XCharStruct for no sensible reason.  These macros
   translate between them.
 */

# define XGlyphInfo_to_XCharStruct(G,C) do {		\
    (C).lbearing  =  -(G).x;				\
    (C).rbearing  =   (G).width - (G).x;		\
    (C).ascent    =   (G).y;				\
    (C).descent   =   (G).height - (G).y;		\
    (C).width     =   (G).xOff;				\
} while (0)

# define XCharStruct_to_XGlyphInfo(C,G) do {		\
    (G).x         =  -(C).lbearing;			\
    (G).y         =   (C).ascent;			\
    (G).xOff      =   (C).width;			\
    (G).yOff      =   0;				\
    (G).width     =   (C).rbearing - (C).lbearing;	\
    (G).height    =   (C).ascent   + (C).descent;	\
} while (0)

/* Xutf8TextExtents returns a bounding box in an XRectangle, which
   conveniently interprets everything in the opposite direction
   from XGlyphInfo!
 */
# define XCharStruct_to_XmbRectangle(C,R) do {		\
    (R).x         =   (C).lbearing;			\
    (R).y         =  -(C).ascent;			\
    (R).width     =   (C).rbearing - (C).lbearing;	\
    (R).height    =   (C).ascent   + (C).descent;	\
} while (0)

# define XmbRectangle_to_XCharStruct(R,C,ADV) do {	\
    (C).lbearing  =   (R).x;				\
    (C).rbearing  =   (R).width + (R).x;		\
    (C).ascent    =  -(R).y;				\
    (C).descent   =   (R).height + (R).y;		\
    (C).width     =   (ADV);				\
} while (0)


# ifdef HAVE_XFT

#  include <X11/Xft/Xft.h>

# else  /* !HAVE_XFT -- the rest of the file */

# ifdef HAVE_COCOA
#  include "jwxyz.h"
# elif defined(HAVE_ANDROID)
#  include "jwxyz.h"
# else
#  include <X11/Xlib.h>
# endif

/* This doesn't seem to work right under X11.  See comment in xft.c. */
# ifndef HAVE_COCOA
#  undef HAVE_XUTF8DRAWSTRING
# endif


# ifndef _Xconst
#  define _Xconst const
# endif

typedef struct _XGlyphInfo {
  unsigned short width, height;     /* bounding box of the ink */
  short x, y;		/* distance from upper left of bbox to glyph origin. */
  short xOff, yOff;	/* distance from glyph origin to next origin. */
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
XftFont *XftFontOpenName (Display *dpy, int screen, _Xconst char *name);

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
Display *XftDrawDisplay (XftDraw *);
Bool XftDrawSetClipRectangles (XftDraw *, int x, int y, 
                               _Xconst XRectangle *rects, int n);
Bool XftDrawSetClip (XftDraw *draw, Region region);
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
