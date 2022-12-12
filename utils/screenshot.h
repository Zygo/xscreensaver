/* xscreensaver, Copyright © 2022 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef _XSCREENSAVER_SCREENSHOT_H_
#define _XSCREENSAVER_SCREENSHOT_H_

/* Grab a screenshot and return it.
   It will be the size and extent of the given window,
   or the full screen, as requested.
   Might be None if we failed.
 */
extern Pixmap screenshot_grab (Display *, Window, Bool full_screen_p,
                               Bool verbose_p);

/* Store the screenshot pixmap on a property on the window. */
extern void screenshot_save (Display *, Window, Pixmap);

/* Loads the screenshot from screenshot_save() and returns a new pixmap
   that is the same size as the winow.
 */
extern Pixmap screenshot_load (Display *, Window, Bool verbose_p);

/* Returns the absolute position of the window on its root window. */
void window_root_offset (Display *, Window, int *x, int *y);

#endif /* _XSCREENSAVER_SCREENSHOT_H_ */
