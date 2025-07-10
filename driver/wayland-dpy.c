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
 * Connecting to a Wayland server, and a few connection-related utilities.
 * The various other wayland-*.c modules (idle, dpms, etc.) build on this.
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
#include "wayland-dpyI.h"
#include "blurb.h"

/* Dispatch any events that have been read. */
static int
handle_event (int fd, uint32_t mask, void *data)
{
  wayland_dpy *state = (wayland_dpy *) data;
  int count = 0;

  if ((mask & WL_EVENT_HANGUP) || (mask & WL_EVENT_ERROR))
    {
      fprintf (stderr, "%s: Wayland connection closed\n", blurb());
      if (state->atexit_cb)
        (*state->atexit_cb) (state->atexit_closure);
      exit (1);
    }

  if (mask & WL_EVENT_READABLE)
    count = wl_display_dispatch (state->dpy);

  if (mask & WL_EVENT_WRITABLE)
    wl_display_flush (state->dpy);

  if (mask == 0)
    {
      count = wl_display_dispatch_pending (state->dpy);
      wl_display_flush (state->dpy);
    }

  return count;
}


/* Connects to Wayland and returns an opaque state object on success. */
wayland_dpy *
wayland_dpy_connect (void)
{
  wayland_dpy *state = (wayland_dpy *) calloc (sizeof (*state), 1);
  state->dpy = wl_display_connect (NULL);
  if (!state->dpy)
    {
      free (state);
      return NULL;
    }

  if (verbose_p)
    fprintf (stderr, "%s: wayland: server connected\n", blurb());

  state->event_loop = wl_event_loop_create();

  { /* Tell the event loop to select() on the display connection. */
    struct wl_event_source *source =
      wl_event_loop_add_fd (state->event_loop,
                            wl_display_get_fd (state->dpy),
                            WL_EVENT_READABLE, handle_event, state);
    wl_event_source_check (source);
    /* #### Do I need to free 'source'? */
  }

  wayland_dpy_process_events (state, 0);
  return state;
}

int
wayland_dpy_get_fd (wayland_dpy *state)
{
  return wl_display_get_fd (state->dpy);
}


static void
sync_cb (void *data, struct wl_callback *cb, uint32_t serial)
{
  int *done = data;
  *done = 1;
  wl_callback_destroy (cb);
}

/* Handle any notifications from the Wayland server and run callbacks.
   If sync_p, blocks until all outstanding events have been processed.
 */
void
wayland_dpy_process_events (wayland_dpy *state, int sync_p)
{
  struct wl_callback *cb;
  int done = 0, ret = 0;

  /* Process all outstanding events without blocking. */
  wl_event_loop_dispatch (state->event_loop, 0);

  /* Block until we get our own ACK, meaning queue is flushed. */
  if (sync_p)
    {
      static const struct wl_callback_listener listener = { sync_cb };
      cb = wl_display_sync (state->dpy);
      wl_callback_add_listener (cb, &listener, &done);
      while (ret != -1 && !done)
        ret = wl_display_dispatch (state->dpy);
      if (!done)
        wl_callback_destroy (cb);
    }
}

void
wayland_dpy_close (wayland_dpy *state)
{
  wl_display_disconnect (state->dpy);
  if (state->event_loop)
    wl_event_loop_destroy (state->event_loop);
  free (state);
  if (verbose_p)
    fprintf (stderr, "%s: wayland: disconnected\n", blurb());
}

void
wayland_dpy_atexit (wayland_dpy *dpy,
                    void (*cb) (void *closure),
                    void *closure)
{
  dpy->atexit_cb = cb;
  dpy->atexit_closure = closure;
}
