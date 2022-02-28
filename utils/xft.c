/* xscreensaver, Copyright © 2014-2021 Jamie Zawinski <jwz@jwz.org>
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


Display *
XftDrawDisplay (XftDraw *draw)
{
  return draw->dpy;
}


Bool
XftDrawSetClipRectangles (XftDraw *draw, int x, int y,
                          _Xconst XRectangle *rects, int n)
{
  if (!n)
    return XSetClipMask (draw->dpy, draw->gc, 0);
# if 0
  else
    /* #### unimplemented in jwxyz, so clipping is a no-op */
    return XSetClipRectangles (draw->dpy, draw->gc, x, y,
                               (XRectangle *) rects,  /* discard const */
                               n, Unsorted);
# else
  return False;
# endif
}


Bool
XftDrawSetClip (XftDraw *draw, Region region)
{
  if (!region)
    return XSetClipMask (draw->dpy, draw->gc, 0);
# if 0
  else
    /* #### unimplemented in jwxyz, so clipping is a no-op */
    {
      XRectangle rect;		/* #### untested */
      XClipBox (region, &rect); /* #### unimplemented in jwxyz */
      return XftDrawSetClipRectangles (draw, 0, 0, &rect, 1);
    }
# else
  return False;
# endif
}


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
    unsigned i;

    /* In the event of -*-random-* (under JWXYZ), get the actual XLFD,
       otherwise we'll get another random font that doesn't match ff->xfont.
     */
    char *xlfd_resolved = NULL;

    char **missing_charset_list_return;
    int missing_charset_count_return;
    char *def_string_return;

    char *ss;

    for (i = 0; i != ff->xfont->n_properties; ++i) {
      if (ff->xfont->properties[i].name == XA_FONT) {
        xlfd_resolved = XGetAtomName (dpy, ff->xfont->properties[i].card32);
        if (xlfd_resolved)
          xlfd = xlfd_resolved;
        break;
      }
    }

    ss = (char *) malloc (strlen(xlfd) + 10);
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
    free (xlfd_resolved);
  }
# endif

  return ff;
}


/* Very approximately convert an XFT-style name to an XLFD style name
   and then call XftFontOpenXlfd on that.
 */
XftFont *
XftFontOpenName (Display *dpy, int screen, _Xconst char *xft_name)
{
  XftFont *font;
  char *name = strdup (xft_name);
  char *xlfd = 0;
  char *s, *b, *i, *o, *c;
  int size;
  char dummy;

  /* Ignore any ":keywords=" */
  c = strchr (name, ':');
  if (c) *c = 0;

  /* Downcase ASCII */
  for (s = name; *s; s++)
    if (*s >= 'A' && *s <= 'Z')
      *s += 'a'-'A';

  /* "Family-NN" */
  s = strrchr (name, '-');
  if (!s) goto FAIL;
  if (1 != sscanf (s+1, " %d %c", &size, &dummy)) goto FAIL;
  if (size <= 0) goto FAIL;
  *s = 0;

  /* "Family Bold", etc. */
  b = strstr (name, " bold");
  i = strstr (name, " italic");
  o = strstr (name, " oblique");
  if (b) *b = 0;
  if (i) *i = 0;
  if (o) *o = 0;

  xlfd = (char *) malloc (strlen(name) + 80);
  sprintf (xlfd, "-*-%s-%s-%s-*-*-*-%d-*-*-*-*-*-*",
           name,
           (b ? "bold" : "medium"),
           (i ? "i" : o ? "o" : "r"),
           size * 10);
  font = XftFontOpenXlfd (dpy, screen, xlfd);
  free (name);
  free (xlfd);
  return font;

 FAIL:
  fprintf (stderr, "%s: XFT: unparsable: \"%s\"\n", progname, xft_name);
  if (name) free (name);
  if (xlfd) free (xlfd);
  return 0;
}


void
XftFontClose (Display *dpy, XftFont *font)
{
  if (!dpy || !font) return;   /* Fuck it, just leak */
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
  if (!dpy || !visual || !result) abort();
  if (!name || !*name) name = "#FFFFFF";

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
      int red_shift   = maskbase (visual->red_mask);
      int red_len     = masklen  (visual->red_mask);
      int green_shift = maskbase (visual->green_mask);
      int green_len   = masklen (visual->green_mask);
      int blue_shift  = maskbase (visual->blue_mask);
      int blue_len    = masklen (visual->blue_mask);
      result->pixel = (((color->red   >> (16 - red_len))   << red_shift)   |
                       ((color->green >> (16 - green_len)) << green_shift) |
                       ((color->blue  >> (16 - blue_len))  << blue_shift));
# ifdef HAVE_JWXYZ
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
  if (!dpy || !visual || !color) return;   /* Fuck it, just leak */
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
  if (!draw) return;   /* Fuck it, just leak */
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
  XCharStruct overall;

  if (!dpy || !font || !string || !extents) abort();

# ifdef HAVE_XUTF8DRAWSTRING
  {
    XRectangle ink;
    int advancement =
      Xutf8TextExtents (font->fontset, (const char *) string, len, &ink, 0);
    XmbRectangle_to_XCharStruct (ink, overall, advancement);
  }
# else  /* !HAVE_XUTF8DRAWSTRING */
  {
    char *s2 = (char *) malloc (len + 1);
    int direction, ascent, descent;
    XChar2b *s16;
    int s16_len = 0;
    strncpy (s2, (char *) string, len);
    s2[len] = 0;
    s16 = utf8_to_XChar2b (s2, &s16_len);
    XTextExtents16 (font->xfont, s16, s16_len,
                    &direction, &ascent, &descent, &overall);
    free (s2);
    free (s16);
  }
# endif /* !HAVE_XUTF8DRAWSTRING */

  XCharStruct_to_XGlyphInfo (overall, *extents);
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

  /* #### I guess I don't really understand how FontSet works, because when
          using the real X11 implementation of Xutf8DrawString, this seems
          to just truncate the text at the first non-ASCII character.

          The XDrawString16() path works, however, at the expense of losing
          everything above Basic Multilingual.  However, that path is only
          taken on X11 systems that are old enough to not have libXft,
          which means that the chance of Unicode working was already slim.
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
