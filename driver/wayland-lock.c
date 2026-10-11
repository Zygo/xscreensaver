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
 * Locking the screen via the "ext-session-lock-v1" protocol:
 * https://wayland.app/protocols/ext-session-lock-v1
 * If the Wayland server does not implement this protocol, XScreenSaver
 * cannot lock the screen.  It is not implemented by KDE or GNOME.
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

#include <X11/Xlib.h>

#include <wayland-client.h>
#include <wayland-server.h>

#include "blurb.h"
#include "wayland-dpyI.h"
#include "wayland-lock.h"
#include "wayland-protocols/ext-session-lock-v1-client-protocol.h"

struct wayland_lock {
  wayland_dpy          *parent;
  struct wl_display    *dpy;
  struct wl_registry   *reg;
  struct wl_event_loop *event_loop;
  struct wl_list        outputs;

  Window (*get_window_cb) (const char *name, void *closure);
  void (*reshape_cb) (const char *name, unsigned int w, unsigned int h,
                      void *closure);
  void (*unlocked_cb) (void *closure);
  void *closure;

  struct ext_session_lock_manager_v1 *session_lock_mgr;
  struct ext_session_lock_v1         *session_lock;

  enum { WAITING, LOCKED, FAILED } lock_state;
};

typedef struct lock_output {
  struct wl_list link;
  wayland_lock *state;

  struct wl_output *wl_output;
  char *name;
  int32_t x, y, physical_width, physical_height, subpixel, transform;
  int32_t width, height, refresh, scale;

  Bool wl_done;	/* wl_output: all info about this output has been sent */

  struct wl_surface *lock_surface;
  struct ext_session_lock_surface_v1 *session_lock_surface;

} lock_output;


static void
wl_handle_name (void *data, struct wl_output *wl_output, const char *name)
{
  lock_output *out = (lock_output *) data;
  if (out->name) free (out->name);
  out->name = strdup (name);
  if (verbose_p > 2)
    fprintf (stderr, "%s: wayland: lock: output %s\n", blurb(), name);
}

static void
wl_handle_geometry (void *data, struct wl_output *wl_output,
                    int32_t x, int32_t y,
                    int32_t physical_width, int32_t physical_height,
                    int32_t subpixel, const char *make, const char* model,
                    int32_t transform)
{
  lock_output *out = (lock_output *) data;
  out->x = x;
  out->y = y;
  out->physical_width  = physical_width;
  out->physical_height = physical_height;
  out->subpixel  = subpixel;
  out->transform = transform;
}

static void
wl_handle_mode (void *data, struct wl_output *wl_output,
                uint32_t flags, int32_t width, int32_t height, int32_t refresh)
{
  lock_output *out = (lock_output *) data;
  out->width   = width;
  out->height  = height;
  out->refresh = refresh;
}

static void
wl_handle_scale (void *data, struct wl_output *wl_output, int32_t scale)
{
  lock_output *out = (lock_output *) data;
  out->scale = scale;
}

static void
wl_handle_description (void *data, struct wl_output *wl_output,
                       const char *desc)
{
}

static void
handle_configure (void *data, struct ext_session_lock_surface_v1 *surface,
                  uint32_t serial, uint32_t width, uint32_t height)
{
  lock_output *out = (lock_output *) data;
  wayland_lock *state = out->state;
  out->width = width;
  out->height = height;
  if (verbose_p > 2)
    fprintf (stderr, "%s: wayland: lock: configure \"%s\" %u x %u\n",
             blurb(), out->name, width, height);
  ext_session_lock_surface_v1_ack_configure (surface, serial);
  state->reshape_cb (out->name, width, height, state->closure);
}


/* Inform the server of the surfaces we will be using on the locked screens.
 */
static void
get_lock_surface (lock_output *out)
{
  wayland_lock *state = out->state;
  Window xwin;

  if (! state->session_lock)
    abort();
  if (out->lock_surface)
    abort();

  xwin = state->get_window_cb (out->name, state->closure);
  if (!xwin)
    {
      fprintf (stderr, "%s: no X window for \"%s\"\n", blurb(), out->name);
      /* #### abort(); */
    }

  /* #### Here's where the magic happens.

          I don't know how to get the wl_surface of an X11 Window,
          and without that, none of this can be made to work.

          Xwayland sends a ClientMessage to the internal "XWM" window
          manager with the WL_SURFACE_ID in it, but it does so using a
          secret, internal API that does not allow anyone else, even the
          creator of the X11 Window, to see that event.  Sweet.

          But even if we got past that hurdle somehow, this still wouldn't
          work because 'get_lock_surface' insists that the surface not have
          a role or buffer already attached, and Xwayland surfaces do.

          Great job, everybody.  Perfect, no notes.

     out->lock_surface = ...
   */

  out->session_lock_surface =
    ext_session_lock_v1_get_lock_surface (state->session_lock,
                                          out->lock_surface,
                                          out->wl_output);
  {
    static const struct ext_session_lock_surface_v1_listener listener = {
      .configure = handle_configure,
    };
    ext_session_lock_surface_v1_add_listener (out->session_lock_surface,
                                              &listener, out);
  }

  fprintf (stderr, "%s: wayland: lock: output added: %s\n", blurb(),
           out->name);
}


static void
wl_handle_done (void *data, struct wl_output *wl_output)
{
  lock_output *out = (lock_output *) data;
  wayland_lock *state = out->state;
  out->wl_done = True;
  if (state->session_lock)  /* Output appeared after locking began */
    get_lock_surface (out);
}


/* This is an iterator for all of the extensions that Wayland provides,
   so we can find the ones we are interested in.
 */
static void
handle_global (void *data, struct wl_registry *reg,
               uint32_t name, const char *iface, uint32_t version)
{
  wayland_lock *state = (wayland_lock *) data;

  if (!strcmp (iface, wl_output_interface.name))
    {
      static const struct wl_output_listener wl_listener = {
	.name        = wl_handle_name,
	.geometry    = wl_handle_geometry,
	.mode        = wl_handle_mode,
	.scale       = wl_handle_scale,
	.description = wl_handle_description,
	.done        = wl_handle_done,
      };

      lock_output *out = (lock_output *) calloc (sizeof (*out), 1);
      out->state = state;
      wl_list_insert (&state->outputs, &out->link);

      if (verbose_p > 2)
        fprintf (stderr, "%s: wayland: lock: found: %s\n", blurb(), iface);

      /* First listener: properties of the wl_output object. */
      out->wl_output = wl_registry_bind (reg, name, &wl_output_interface,
                                         version);
      wl_output_add_listener (out->wl_output, &wl_listener, out);
    }
  else if (! strcmp (iface, ext_session_lock_manager_v1_interface.name))
    {
      if (verbose_p)
        fprintf (stderr, "%s: wayland: lock: found: %s\n", blurb(), iface);
      state->session_lock_mgr =
        wl_registry_bind (reg, name, &ext_session_lock_manager_v1_interface,
                          version);
    }
}

static void
handle_global_remove (void *data, struct wl_registry *registry, uint32_t name)
{
}


/* Called if our request to lock the screen was accepted. */
static void
handle_locked (void *data, struct ext_session_lock_v1 *lock)
{
  wayland_lock *state = (wayland_lock *) data;
  state->lock_state = LOCKED;
}

/* Called if our request to lock the screen was rejected, or if the server
   decided to forcibly unlock us for some other reason. */
static void
handle_finished (void *data, struct ext_session_lock_v1 *lock)
{
  wayland_lock *state = (wayland_lock *) data;
  if (state->lock_state == LOCKED)
    state->unlocked_cb (state);
  state->lock_state = FAILED;
}


/* Binds to Wayland locking protocols and returns an opaque state object
   on success.
 */
wayland_lock *
wayland_lock_init (wayland_dpy *dpy)
{
  wayland_lock *state;
  static const struct wl_registry_listener listener = {
    .global        = handle_global,
    .global_remove = handle_global_remove,
  };

  if (!dpy) return NULL;

  state = (wayland_lock *) calloc (sizeof (*state), 1);
  state->parent     = dpy;
  state->lock_state = WAITING;

  wl_list_init (&state->outputs);

  state->reg = wl_display_get_registry (dpy->dpy);
  wl_registry_add_listener (state->reg, &listener, state);

  /* Do the first round trip to load the wl_outputs. */
  wayland_dpy_process_events (dpy, true);

  /* Now do more round trips until we have all the info about them. FFS! */
  while (1)
    {
      Bool all_done = True;
      lock_output *out;
      wl_list_for_each (out, &state->outputs, link)
        {
          if (!out->wl_done)
            all_done = False;
        }
      if (all_done) break;
      wayland_dpy_process_events (dpy, true);
    }

  if (! state->session_lock_mgr)
    {
      fprintf (stderr,
               "%s: wayland: lock: server doesn't implement \"%s\"\n",
               blurb(),
               ext_session_lock_v1_interface.name);
      wayland_lock_free (state);
      return NULL;
    }

  return state;
}


/* Locks the screen.  Returns true on success.
 */
Bool
wayland_lock_screen (wayland_lock *state,
                     Window (*get_window_cb) (const char *name, void *closure),
                     void (*reshape_cb) (const char *name,
                                         unsigned int w, unsigned int h,
                                         void *closure),
                     void (*unlocked_cb) (void *closure),
                     void *closure)
{
  if (!get_window_cb) abort();
  if (!reshape_cb) abort();
  if (!unlocked_cb) abort();

  state->get_window_cb = get_window_cb;
  state->unlocked_cb   = unlocked_cb;
  state->reshape_cb    = reshape_cb;
  state->closure       = closure;

  /* Request that the screen be locked.  Either the 'locked' or 'finished'
     callback will be run in response. */
  {
    static const struct ext_session_lock_v1_listener listener = {
      .locked   = handle_locked,
      .finished = handle_finished,
    };
    state->session_lock =
      ext_session_lock_manager_v1_lock (state->session_lock_mgr);
    ext_session_lock_v1_add_listener (state->session_lock, &listener, state);
  }

  /* Wait for either 'locked' or 'finished' to run. */
  while (state->lock_state == WAITING)
    wayland_dpy_process_events (state->parent, true);

  switch (state->lock_state) {
  case LOCKED:
    if (verbose_p)
      fprintf (stderr, "%s: wayland: lock: locked\n", blurb());
    break;
  case FAILED:
    fprintf (stderr, "%s: wayland: lock: failed to lock\n", blurb());
    return False;
    break;
  default:
    abort();
    break;
  }

  /* Now that we are locked, get a surface for each currently-existing
     output. */
  {
    lock_output *out;
    wl_list_for_each (out, &state->outputs, link)
      {
        get_lock_surface (out);
      }
  }

  return True;
}


void
wayland_unlock_screen (wayland_lock *state)
{
  if (! state->session_lock) return;

  ext_session_lock_v1_unlock_and_destroy (state->session_lock);
  state->session_lock  = NULL;
  state->get_window_cb = NULL;
  state->unlocked_cb   = NULL;
  state->reshape_cb    = NULL;
  state->closure       = NULL;
  state->lock_state    = WAITING;
  wayland_dpy_process_events (state->parent, true);
}


void
wayland_lock_free (wayland_lock *state)
{
  lock_output *out, *tmp;

  if (state->session_lock)
    ext_session_lock_v1_destroy (state->session_lock);

  if (state->session_lock_mgr)
    ext_session_lock_manager_v1_destroy (state->session_lock_mgr);

  wl_list_for_each_safe (out, tmp, &state->outputs, link)
    {
      wl_list_remove (&out->link);
      if (out->session_lock_surface)
        ext_session_lock_surface_v1_destroy (out->session_lock_surface);
      if (out->wl_output)
        wl_output_destroy (out->wl_output);
      if (out->name)
        free (out->name);
      free (out);
    }

  if (state->reg)
    wl_proxy_destroy ((struct wl_proxy *) state->reg);

  free (state);
}
