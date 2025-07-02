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

typedef struct wayland_state wayland_state;

/* Connects to Wayland and returns an opaque state object on success.
   When user activity is detected, the callback will be run with the
   provided object as its argument.  On failure, returns NULL and
   an error message.
 */
extern wayland_state *
wayland_idle_init (void (*activity_cb) (void *closure),
                   void *closure,
                   const char **error_ret);

/* Returns the file descriptor of the Wayland display connection.
   You may select on this to see if it needs attention. */
extern int wayland_idle_get_fd (wayland_state *);

/* Handle any notifications from the Wayland server and run callbacks. */
extern void wayland_idle_process_events (wayland_state *);

/* Shut it all down. */
extern void wayland_idle_free (wayland_state *);

#endif /* __XSCREENSAVER_WAYLAND_H__ */
