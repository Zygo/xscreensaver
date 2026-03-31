/* xscreensaver, Copyright © 2014-2026 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __XSCREENSAVER_UTF8WC_H__
#define __XSCREENSAVER_UTF8WC_H__

/* Converts a UTF8 string to the closest Latin1 or ASCII equivalent.
 */
extern char *utf8_to_latin1 (const char *string, int ascii_p);

/* Converts a Unicode character to a multi-byte UTF8 sequence.
   Returns the number of bytes written.
 */
extern int utf8_encode (unsigned long uc, char *out, long length);

/* Parse the first UTF8 character at the front of the string.
   Return the Unicode character, and the number of bytes read.
 */
extern long utf8_decode (const unsigned char *in, long length,
                         unsigned long *unicode_ret);

/* Like utf8_decode() except that if the following character is a
   Combining Diacritical or Zero Width Joiner, the appropriate number
   of following characters will be swallowed as well.
   Return the *first* Unicode character, and the number of bytes read.
 */
extern long utf8_decode_combining (const unsigned char *in, long length,
                                   unsigned long *unicode_ret);

/* Whether the Unicode character is whitespace, for word-wrapping purposes. */
extern int uc_isspace (unsigned long);

/* Whether the Unicode character is punctuation, for word-wrapping purposes. */
extern int uc_ispunct (unsigned long);

/* Whether the Unicode character should be combined with the preceding
   character (Combining Diacritical or Zero Width Joiner). */
extern int uc_is_combining (unsigned long);


# if defined(_X11_XLIB_H_) ||	/* X11/Xlib.h has been included */ \
     defined(__JWXYZ_H__)	/* jwxyz/jwxyz.h has been included */

/* Utilities for converting between UTF8 and XChar2b. */

/* Converts a null-terminated UTF8 string to a null-terminated XChar2b array.
   This only handles characters that can be represented in 16 bits, the
   Basic Multilingual Plane. (No hieroglyphics, Elvish, Klingon or Emoji.)
 */
extern XChar2b * utf8_to_XChar2b (const char *, int *length_ret);

/* Converts a null-terminated XChar2b array to a null-terminated UTF8 string.
 */
extern char *    XChar2b_to_utf8 (const XChar2b *, int *length_ret);

# endif /* Xlib */

#endif /* __XSCREENSAVER_UTF8WC_H__ */
