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

#ifndef __XSCREENSAVER_WAYLAND_DPY_H__
#define __XSCREENSAVER_WAYLAND_DPY_H__

typedef struct wayland_dpy  wayland_dpy;
typedef struct wayland_idle wayland_idle;
typedef struct wayland_dpms wayland_dpms;
typedef struct wayland_lock wayland_lock;

/* Connects to Wayland and returns a state object on success. */
extern wayland_dpy *wayland_dpy_connect (void);

/* Returns the file descriptor of the Wayland display connection.
   You may select on this to see if it needs attention. */
extern int wayland_dpy_get_fd (wayland_dpy *);

/* Handle any notifications from the Wayland server and run callbacks.
   If sync_p, blocks until all outstanding events have been processed.
 */
extern void wayland_dpy_process_events (wayland_dpy *, int sync_p);

/* Run this callback when the Wayland server drops the connection. */
extern void wayland_dpy_atexit (wayland_dpy *,
                                void (*cb) (void *closure),
                                void *closure);

/* Shut it all down. */
extern void wayland_dpy_close (wayland_dpy *);

#endif /* __XSCREENSAVER_WAYLAND_DPY_H__ */
