/* xscreensaver, Copyright © 2018-2026 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifdef __XSCREENSAVER_XFT_H__	/* Only if xft.h has also been included */

#ifndef __FONT_RETRY_H__
#define __FONT_RETRY_H__

/* Like XftFontOpenName, except that the argument can be a comma-separated
   list of font names.  Each name can be either an XScreenSaver-style font
   name or an Xft-style pattern.  Returns the first exact match it finds,
   or if there is no exact match, applies heuristics to the last font in
   the list until it finds a substitution.
 */
extern XftFont *load_xft_font_retry (Display *, int screen,
                                     const char *font_list);

#endif /* __FONT_RETRY_H__ */
#endif
