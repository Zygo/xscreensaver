/* xscreensaver, Copyright © 2025 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __XSCREENSAVER_WAYLAND_IDLE_H__
#define __XSCREENSAVER_WAYLAND_IDLE_H__

/* Binds to Wayland idle-detection protocols and returns an opaque state
   object on success.  When user activity is detected, the callback will
   be run with the provided object as its argument.
 */
extern wayland_idle *
wayland_idle_init (wayland_dpy *dpy,
                   void (*activity_cb) (void *closure),
                   void *closure);

/* Shut it all down. */
extern void wayland_idle_free (wayland_idle *);

#endif /* __XSCREENSAVER_WAYLAND_IDLE_H__ */
