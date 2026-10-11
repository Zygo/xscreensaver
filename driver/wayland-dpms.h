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

#ifndef __XSCREENSAVER_WAYLAND_DPMS_H__
#define __XSCREENSAVER_WAYLAND_DPMS_H__

/* Binds to Wayland DPMS-detection protocols and returns an opaque state
   object on success. */
extern wayland_dpms *wayland_dpms_init (wayland_dpy *dpy);
Bool wayland_monitor_powered_on_p (wayland_dpms *);
void wayland_monitor_power_on (wayland_dpms *, Bool on_p);
extern void wayland_dpms_free (wayland_dpms *);

#endif /* __XSCREENSAVER_WAYLAND_DPMS_H__ */
