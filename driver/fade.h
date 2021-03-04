/* xscreensaver, Copyright © 1992-2021 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __FADE_H__
#define __FADE_H__

/* Returns true if canceled by user activity. */
extern Bool fade_screens (XtAppContext app, Display *dpy,
                          Window *black_windows, int nwindows,
			  double seconds, Bool out_p, Bool from_desktop_p,
                          void **closureP);
#endif /* __FADE_H__ */
