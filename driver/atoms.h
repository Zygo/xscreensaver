/* xscreensaver-command, Copyright © 1991-2025 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef _XSCREENSAVER_ATOMS_H_
#define _XSCREENSAVER_ATOMS_H_

extern Atom XA_SCREENSAVER, XA_SCREENSAVER_VERSION, XA_SCREENSAVER_RESPONSE,
     XA_SCREENSAVER_ID, XA_SCREENSAVER_STATUS, XA_SELECT, XA_DEMO, XA_EXIT,
     XA_BLANK, XA_LOCK, XA_ACTIVATE, XA_SUSPEND, XA_NEXT, XA_PREV,
     XA_DEACTIVATE, XA_CYCLE, XA_RESTART, XA_AUTH,
     XA_NET_WM_PID, XA_NET_WM_STATE, XA_NET_WM_STATE_ABOVE,
     XA_NET_WM_STATE_FULLSCREEN, XA_NET_WM_BYPASS_COMPOSITOR,
     XA_NET_WM_STATE_STAYS_ON_TOP, XA_KDE_NET_WM_WINDOW_TYPE_OVERRIDE,
     XA_NET_WM_WINDOW_TYPE, XA_NET_WM_WINDOW_TYPE_SPLASH,
     XA_NET_WM_WINDOW_TYPE_DIALOG, XA_NET_WM_WINDOW_TYPE_NOTIFICATION,
     XA_NET_WM_WINDOW_TYPE_NORMAL;

extern void init_xscreensaver_atoms (Display *dpy);
extern void xscreensaver_set_wm_atoms (Display *, Window,
                                       int width, int height,
                                       Window for_window);

/* You might think that to store an array of 32-bit quantities onto a
   server-side property, you would pass an array of 32-bit data quantities
   into XChangeProperty().  You would be wrong.  You have to use an array
   of longs, even if long is 64 bits (using 32 of each 64.)
 */
typedef long PROP32;

#endif /* _XSCREENSAVER_ATOMS_H_ */
