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

#ifndef __XSCREENSAVER_UTF8WC_H__
#define __XSCREENSAVER_UTF8WC_H__

/* Utilities for converting between UTF8 and XChar2b. */

/* Converts a null-terminated UTF8 string to a null-terminated XChar2b array.
   This only handles characters that can be represented in 16 bits, the
   Basic Multilingual Plane. (No hieroglyphics, Elvish, Klingon or Emoji.)
 */
extern XChar2b * utf8_to_XChar2b (const char *, int *length_ret);

/* Converts a null-terminated XChar2b array to a null-terminated UTF8 string.
 */
extern char *    XChar2b_to_utf8 (const XChar2b *, int *length_ret);

/* Split a UTF8 string into an array of strings, one per character.
   The sub-strings will be null terminated and may be multiple bytes.
 */
extern char ** utf8_split (const char *string, int *length_ret);

/* Converts a UTF8 string to the closest Latin1 or ASCII equivalent.
 */
extern char *utf8_to_latin1 (const char *string, Bool ascii_p);

#endif /* __XSCREENSAVER_UTF8WC_H__ */
