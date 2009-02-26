/* xscreensaver, Copyright (c) 1992-1997 Jamie Zawinski <jwz@netscape.com>
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

extern Colormap copy_colormap (Screen *, Visual *, Colormap from, Colormap to);
extern void blacken_colormap (Screen *, Colormap cmap);
extern void fade_screens (Display *dpy,
			  Colormap *cmaps, Window *black_windows,
			  int seconds, int ticks,
			  Bool out_p, Bool clear_windows);
#endif /* __FADE_H__ */
