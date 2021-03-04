/* xftwrap.h --- XftDrawStringUtf8 with multi-line strings.
 * xscreensaver, Copyright © 2021 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __XFTWRAP_H__
#define __XFTWRAP_H__

/* Returns a new string word-wrapped to fit in the width in pixels.
 */
extern char *xft_word_wrap (Display *, XftFont *, const char *, int pixels);

/* Like XftTextExtentsUtf8, but handles multi-line strings.
   XGlyphInfo will contain the bounding box that encloses all of the text.
   Return value is the number of lines in the text, >= 1.
 */
extern int XftTextExtentsUtf8_multi (Display *, XftFont *, 
                                     const FcChar8 *, int L, XGlyphInfo *);

/* Like XftDrawStringUtf8, but handles multi-line strings.
   Alignment is 1, 0 or -1 for left, center, right.
 */
extern void XftDrawStringUtf8_multi (XftDraw *, const XftColor *,
                                     XftFont *, int x, int y,
                                     const FcChar8 *, int L,
                                     int alignment);

#endif /* __XFTWRAP_H__ */
