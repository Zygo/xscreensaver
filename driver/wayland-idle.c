/* xscreensaver, Copyright © 2025 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Detecting idleness via a connection to the Wayland server and the
 * "idle-notify-v1" protocol: https://wayland.app/protocols/ext-idle-notify-v1
 * If the Wayland server does not implement this protocol, XScreenSaver cannot
 * function.  KDE implements it, but GNOME does not.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <wayland-client.h>
#include <wayland-server.h>

#include "blurb.h"
#include "wayland-dpyI.h"
#include "wayland-idle.h"
#include "wayland-protocols/ext-idle-notify-v1-client-protocol.h"
#include "wayland-protocols/idle-client-protocol.h"


struct wayland_idle {
  wayland_dpy          *parent;
  struct wl_display    *dpy;
  struct wl_registry   *reg;
  struct wl_event_loop *event_loop;

  void (*activity_cb) (void *closure);
  void *closure;

  struct wl_seat *seat;
  char           *seat_name;
  uint32_t        seat_caps;

  struct ext_idle_notifier_v1      *ext_idle_notifier;
  struct ext_idle_notification_v1  *ext_idle_notification;

  struct org_kde_kwin_idle         *kde_idle_manager;
  struct org_kde_kwin_idle_timeout *kde_idle_timer;
};


static void
seat_handle_capabilities (void *data, struct wl_seat *seat, uint32_t caps)
{
  wayland_idle *state = (wayland_idle *) data;
  state->seat_caps = caps;
  if (verbose_p > 2)
    fprintf (stderr, "%s: wayland: idle: seat caps: 0x%02lX\n", blurb(),
             (unsigned long) caps);
}

static void
seat_handle_name (void *data, struct wl_seat *seat, const char *name)
{
  wayland_idle *state = (wayland_idle *) data;
  state->seat_name = strdup (name);
  if (verbose_p > 2)
    fprintf (stderr, "%s: wayland: idle: seat name: \"%s\"\n", blurb(), name);
}


/* This is an iterator for all of the extensions that Wayland provides,
   so we can find the ones we are interested in.
 */
static void
handle_global (void *data, struct wl_registry *reg,
               uint32_t name, const char *iface, uint32_t version)
{
  wayland_idle *state = (wayland_idle *) data;

  if (!strcmp (iface, wl_seat_interface.name))
    {
      static const struct wl_seat_listener seat_listener = {
	.name         = seat_handle_name,
	.capabilities = seat_handle_capabilities,
      };
      if (verbose_p > 2)
        fprintf (stderr, "%s: wayland: idle: found: %s\n", blurb(), iface);
      state->seat = wl_registry_bind (reg, name, &wl_seat_interface, version);
      wl_seat_add_listener (state->seat, &seat_listener, state);
    }
  else if (!strcmp (iface, ext_idle_notifier_v1_interface.name))
    {
      if (verbose_p)
        fprintf (stderr, "%s: wayland: idle: found: %s\n", blurb(), iface);
      state->ext_idle_notifier =
        wl_registry_bind (reg, name, &ext_idle_notifier_v1_interface, 1);
    }
  else if (!strcmp (iface, org_kde_kwin_idle_interface.name))
    {
      if (verbose_p)
        fprintf (stderr, "%s: wayland: idle: found: %s\n", blurb(), iface);
      state->kde_idle_manager =
        wl_registry_bind (reg, name, &org_kde_kwin_idle_interface, 1);
    }
}

static void
handle_global_remove (void *data, struct wl_registry *registry, uint32_t name)
{
}


/* Called when the user has been idle longer than the timeout. */
static void
ext_handle_idled (void *data, struct ext_idle_notification_v1 *note)
{
  /* wayland_idle *state = (wayland_idle *) data; */
  if (verbose_p > 2)
    fprintf (stderr, "%s: wayland: idle: ext_idle notification\n", blurb());
}

/* Called when the user is no longer idle. */
static void
ext_handle_resumed (void *data, struct ext_idle_notification_v1 *note)
{
  wayland_idle *state = (wayland_idle *) data;
  if (verbose_p > 2)
    fprintf (stderr, "%s: wayland: idle: ext_idle active notification\n",
             blurb());
  (*state->activity_cb) (state->closure);
}

static void
kde_handle_idle (void *data, struct org_kde_kwin_idle_timeout *timer)
{
  /* wayland_idle *state = (wayland_idle *) data; */
  if (verbose_p > 2)
    fprintf (stderr, "%s: wayland: idle: kde_idle notification\n", blurb());
}

static void
kde_handle_resumed (void *data, struct org_kde_kwin_idle_timeout *timer)
{
  wayland_idle *state = (wayland_idle *) data;
  if (verbose_p > 2)
    fprintf (stderr, "%s: wayland: idle: kde_idle active notification\n",
             blurb());
  (*state->activity_cb) (state->closure);
}


static void
destroy_timer (wayland_idle *state)
{
  if (state->ext_idle_notification)
    {
      ext_idle_notification_v1_destroy (state->ext_idle_notification);
      state->ext_idle_notification = 0;
      if (verbose_p > 2)
        fprintf (stderr, "%s: wayland: idle: destroyed kde_idle timer\n",
                 blurb());
    }
  if (state->kde_idle_timer)
    {
      org_kde_kwin_idle_timeout_release (state->kde_idle_timer);
      state->kde_idle_timer = 0;
      if (verbose_p > 2)
        fprintf (stderr, "%s: wayland: idle: destroyed ext_idle timer\n",
                 blurb());
    }
}

static void
register_timer (wayland_idle *state, int timeout)
{
  destroy_timer (state);
  if (state->ext_idle_notifier)
    {
      static const struct ext_idle_notification_v1_listener
        ext_idle_notification_listener = {
          .idled   = ext_handle_idled,
          .resumed = ext_handle_resumed,
      };

      state->ext_idle_notification =
        ext_idle_notifier_v1_get_idle_notification (state->ext_idle_notifier,
                                                    timeout,
                                                    state->seat);
      ext_idle_notification_v1_add_listener (state->ext_idle_notification,
                                             &ext_idle_notification_listener,
                                             state);
      if (verbose_p > 2)
        fprintf (stderr, "%s: wayland: idle: registered ext_idle timer\n",
                 blurb());
    }
  else
    {
      static const struct org_kde_kwin_idle_timeout_listener
        kde_idle_timer_listener = {
	  .idle    = kde_handle_idle,
          .resumed = kde_handle_resumed,
      };

      state->kde_idle_timer =
        org_kde_kwin_idle_get_idle_timeout (state->kde_idle_manager,
                                            state->seat,
                                            timeout);
      org_kde_kwin_idle_timeout_add_listener (state->kde_idle_timer,
                                              &kde_idle_timer_listener,
                                              state);
      if (verbose_p > 2)
        fprintf (stderr, "%s: wayland: idle: registered kde_idle timer\n",
                 blurb());
    }
}


/* Binds to Wayland idle-detection protocols and returns an opaque state
   object on success.  When user activity is detected, the callback will
   be run with the provided object as its argument.
 */
wayland_idle *
wayland_idle_init (wayland_dpy *dpy,
                   void (*activity_cb) (void *closure),
                   void *closure)
{
  wayland_idle *state;
  static const struct wl_registry_listener listener = {
    .global        = handle_global,
    .global_remove = handle_global_remove,
  };

  if (!dpy) return NULL;

  state = (wayland_idle *) calloc (sizeof (*state), 1);
  state->parent      = dpy;
  state->activity_cb = activity_cb;
  state->closure     = closure;

  state->reg = wl_display_get_registry (dpy->dpy);
  wl_registry_add_listener (state->reg, &listener, state);

  /* It takes two round trips: first to register seats, then timers. */
  wayland_dpy_process_events (dpy, true);

  if (! state->seat)
    {
      wayland_idle_free (state);
      fprintf (stderr, "%s: wayland: idle: server has no seats\n", blurb());
      return NULL;
    }

  if (! (state->kde_idle_manager ||
         state->ext_idle_notifier))
    {
      fprintf (stderr,
               "%s: wayland: idle: server doesn't implement \"%s\" or \"%s\"\n",
               blurb(),
               ext_idle_notifier_v1_interface.name,
               org_kde_kwin_idle_interface.name);
      wayland_idle_free (state);
      return NULL;
    }

  register_timer (state, 1000);  /* milliseconds */

  return state;
}

void
wayland_idle_free (wayland_idle *state)
{
  if (state->seat_name)
    free (state->seat_name);
  /* #### state->reg ? */
  /* #### state->seat ? */
  /* #### state->ext_idle_notifier, etc ? */
  free (state);
}
