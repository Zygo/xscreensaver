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

#ifndef __XSCREENSAVER_WAYLAND_LOCK_H__
#define __XSCREENSAVER_WAYLAND_LOCK_H__

/* Binds to Wayland locking protocols and returns an opaque state object
   on success.
 */
extern wayland_lock *wayland_lock_init (wayland_dpy *);

/* Locks the screen.  Returns true on success.

   The 'get_window' callback must return an X11 Window corresponding to the
   wl_output with the given name (RANDR screen).

   The 'unlocked' callback is run if the server forcibly unlocks us.
 */
extern Bool
wayland_lock_screen (wayland_lock *,
                     Window (*get_window_cb) (const char *name, void *closure),
                     void (*reshape_cb) (const char *name,
                                         unsigned int w, unsigned int h,
                                         void *closure),
                     void (*unlocked_cb) (void *closure),
                     void *closure);

extern void wayland_unlock_screen (wayland_lock *);

/* Shut it all down. If the screen is locked it may remain locked. */
extern void wayland_lock_free (wayland_lock *);

#endif /* __XSCREENSAVER_WAYLAND_LOCK_H__ */
