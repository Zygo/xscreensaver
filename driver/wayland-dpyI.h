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

#ifndef __XSCREENSAVER_WAYLAND_DPYI_H__
#define __XSCREENSAVER_WAYLAND_DPYI_H__

#include "wayland-dpy.h"

struct wayland_dpy {
  struct wl_display    *dpy;
  struct wl_event_loop *event_loop;

  void (*atexit_cb) (void *closure);
  void *atexit_closure;
};

#endif /* __XSCREENSAVER_WAYLAND_DPYI_H__ */
