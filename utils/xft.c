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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef HAVE_XFT

# include "utils.h"
# include "resources.h"
# include "xft.h"
# include "utf8wc.h"

extern const char *progname;

struct _XftDraw {
  Display   *dpy;
  Drawable  drawable;
  GC gc;
  unsigned long pixel;
  Font fid;
  Visual *visual;
  Colormap  colormap;
};


XftFont *
XftFontOpenXlfd (Display *dpy, int screen, _Xconst char *xlfd)
{
  XftFont *ff = (XftFont *) calloc (1, sizeof(*ff));

  if (!dpy || !xlfd) abort();
  if (!ff) return 0;
  ff->xfont = XLoadQueryFont (dpy, xlfd);
  if (!ff->xfont)
    {
      free (ff);
      return 0;
    }

  ff->name = strdup (xlfd);
  ff->ascent  = ff->xfont->ascent;
  ff->descent = ff->xfont->descent;
  ff->height = ff->ascent + ff->descent;

# ifdef HAVE_XUTF8DRAWSTRING
  {
    char **missing_charset_list_return;
    int missing_charset_count_return;
    char *def_string_return;

    char *ss = (char *) malloc (strlen(xlfd) + 10);
    strcpy (ss, xlfd);
    strcat (ss, ",*");
    ff->fontset = XCreateFontSet (dpy, ss,
                                  &missing_charset_list_return,
                                  &missing_charset_count_return,
                                  &def_string_return);

# if 0
    {
      int i;
      for (i = 0; i < missing_charset_count_return; i++)
        fprintf (stderr, "%s: missing charset: %s\n",
                 ss, missing_charset_list_return[i]);
    }
# endif

    /* Apparently this is not to be freed. */
    /* if (def_string_return) XFree (def_string_return); */

    if (missing_charset_list_return)
      XFreeStringList (missing_charset_list_return);

    free (ss);
  }
# endif

  return ff;
}


void
XftFontClose (Display *dpy, XftFont *font)
{
  if (!dpy || !font) abort();
  free (font->name);
  XFreeFont (dpy, font->xfont);
# ifdef HAVE_XUTF8DRAWSTRING
  XFreeFontSet (dpy, font->fontset);
# endif
  free (font);
}


Bool
XftColorAllocName (Display  *dpy,
		   _Xconst Visual *visual,
		   Colormap cmap,
		   _Xconst char *name,
		   XftColor *result)
{
  XColor color;
  if (!dpy || !visual || !name || !result) abort();

  if (! XParseColor (dpy, cmap, name, &color))
    {
      fprintf (stderr, "%s: can't parse color %s", progname, name);
      return False;
    }
  else if (! XAllocColor (dpy, cmap, &color))
    {
      fprintf (stderr, "%s: couldn't allocate color %s", progname, name);
      return False;
    }
  else
    {
      XRenderColor color2;
      color2.red    = color.red;
      color2.green  = color.green;
      color2.blue   = color.blue;
      color2.alpha  = 0xFFFF;
      XftColorAllocValue (dpy, visual, cmap, &color2, result);
      result->pixel = color.pixel;
      return True;
    }
}


static short
maskbase (unsigned long m)
{
  short i;
  if (!m)
    return 0;
  i = 0;
  while (! (m&1))
    {
      m >>= 1;
      i++;
    }
  return i;
}


static short
masklen (unsigned long m)
{
  unsigned long y;
  y = (m >> 1) & 033333333333;
  y = m - y - ((y >>1) & 033333333333);
  return (short) (((y + (y >> 3)) & 030707070707) % 077);
}


Bool
XftColorAllocValue (Display *dpy,
		    _Xconst Visual *visual,
		    Colormap cmap,
		    _Xconst XRenderColor *color,
		    XftColor *result)
{
  if (!dpy || !visual || !color || !result) abort();
  if (visual->class == TrueColor)
    {
      int red_shift, red_len;
      int green_shift, green_len;
      int blue_shift, blue_len;

      red_shift   = maskbase (visual->red_mask);
      red_len     = masklen  (visual->red_mask);
      green_shift = maskbase (visual->green_mask);
      green_len   = masklen (visual->green_mask);
      blue_shift  = maskbase (visual->blue_mask);
      blue_len    = masklen (visual->blue_mask);
      result->pixel = (((color->red   >> (16 - red_len))   << red_shift)   |
                       ((color->green >> (16 - green_len)) << green_shift) |
                       ((color->blue  >> (16 - blue_len))  << blue_shift));
# ifdef HAVE_COCOA
      result->pixel |= BlackPixel(dpy, 0);  /* alpha */
# endif
    }
  else
    {
      XColor xcolor;
      xcolor.red   = color->red;
      xcolor.green = color->green;
      xcolor.blue  = color->blue;
      if (!XAllocColor (dpy, cmap, &xcolor))
        return False;
      result->pixel = xcolor.pixel;
    }
  result->color.red   = color->red;
  result->color.green = color->green;
  result->color.blue  = color->blue;
  result->color.alpha = color->alpha;
  return True;
}


void
XftColorFree (Display *dpy,
              Visual *visual,
              Colormap cmap,
              XftColor *color)
{
  if (!dpy || !visual || !color) abort();
  if (visual->class != TrueColor)
    XFreeColors (dpy, cmap, &color->pixel, 1, 0);
}


XftDraw *
XftDrawCreate (Display   *dpy,
	       Drawable  drawable,
	       Visual    *visual,
	       Colormap  colormap)
{
  XftDraw *dd = (XftDraw *) calloc (1, sizeof(*dd));
  if (!dpy || !drawable || !visual) abort();
  if (!dd) return 0;

  dd->dpy = dpy;
  dd->drawable = drawable;
  dd->visual = visual;
  dd->colormap = colormap;
  dd->gc = XCreateGC (dpy, drawable, 0, 0);
  return dd;
}


void
XftDrawDestroy (XftDraw	*draw)
{
  if (!draw) abort();
  XFreeGC (draw->dpy, draw->gc);
  free (draw);
}


void
XftTextExtentsUtf8 (Display	    *dpy,
		    XftFont	    *font,
		    _Xconst FcChar8 *string,
		    int		    len,
		    XGlyphInfo	    *extents)
{
  int direction, ascent, descent;
  XCharStruct overall;
  XChar2b *s16;
  int s16_len = 0;
  char *s2 = (char *) malloc (len + 1);

  if (!dpy || !font || !string || !extents || !s2) abort();

  strncpy (s2, (char *) string, len);
  s2[len] = 0;
  s16 = utf8_to_XChar2b (s2, &s16_len);
  free (s2);
  XTextExtents16 (font->xfont, s16, s16_len,
                  &direction, &ascent, &descent, &overall);
  free (s16);

  extents->x      = -overall.lbearing;
  extents->y      =  overall.ascent;
  extents->xOff   =  overall.width;
  extents->yOff   =  0;
  extents->width  =  overall.rbearing - overall.lbearing;
  extents->height =  overall.ascent + overall.descent;
}


void
XftDrawStringUtf8 (XftDraw	    *draw,
		   _Xconst XftColor *color,
		   XftFont	    *font,
		   int		    x,
		   int		    y,
		   _Xconst FcChar8  *string,
		   int		    len)
{
  if (!draw || !color || !font || !string) abort();

  if (color->pixel != draw->pixel)
    {
      XSetForeground (draw->dpy, draw->gc, color->pixel);
      draw->pixel = color->pixel;
    }
  if (font->xfont->fid != draw->fid)
    {
      XSetFont (draw->dpy, draw->gc, font->xfont->fid);
      draw->fid = font->xfont->fid;
    }

# ifdef HAVE_XUTF8DRAWSTRING
  /* If we have Xutf8DrawString, use it instead of XDrawString16 because
     there is some chance it will handle characters of more than 16 bits
     (beyond the Basic Multilingual Plane).
   */

  /* #### I guess I don't really understand how FontSet works, because this
          seems to just truncate the text at the first non-ASCII character.
          The XDrawString16() path works, however.
   */
  Xutf8DrawString (draw->dpy, draw->drawable, font->fontset, draw->gc, x, y, 
                   (const char *) string, len);
# else
  {
    int s16_len = 0;
    char *s2 = (char *) malloc (len + 1);
    XChar2b *s16;
    strncpy (s2, (char *) string, len);
    s2[len] = 0;
    s16 = utf8_to_XChar2b (s2, &s16_len);
    free (s2);
    XDrawString16 (draw->dpy, draw->drawable, draw->gc, x, y, s16, s16_len);
    free (s16);
  }
# endif
}

#else  /* HAVE_XFT */

const int Wempty_translation_unit_is_a_dumb_warning = 0;

#endif /* HAVE_XFT */
