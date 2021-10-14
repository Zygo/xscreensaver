/* xscreensaver, Copyright © 1993-2021 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __XSCREENSAVER_PREFS_H__
#define __XSCREENSAVER_PREFS_H__

extern int parse_init_file (const char *name,
                            void (*handler) (int lineno, 
                                             const char *key, const char *val,
                                             void *closure),
                            void *closure);

#endif /* __XSCREENSAVER_PREFS_H__ */
