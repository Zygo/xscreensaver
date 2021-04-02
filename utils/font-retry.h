/* xscreensaver, Copyright © 2018-2021 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __FONT_RETRY_H__
#define __FONT_RETRY_H__

/* Like XLoadQueryFont, XftFontOpenXlfd or XftFontOpenName, except that the
   argument can be a comma-separated list of font names.  Each name can be
   either an XLFD or Xft pattern.  Returns the first exact match it finds, or
   if there is no exact match, applies heuristics to the last font in the list
   until it finds a substitution.
 */
extern XFontStruct *load_font_retry (Display *, const char *font_list);

# ifdef __XSCREENSAVER_XFT_H__  /* if xft.h has been included */
extern XftFont *load_xft_font_retry (Display *, int screen,
                                     const char *font_list);
# endif

#endif /* __FONT_RETRY_H__ */
