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
 * Detecting and changing whether monitors are powered on under Wayland.
 * What was ~50 lines of code under X11 (with error checking!) is now 360+.
 * Wayland is incredible (pej., obs.)
 *
 * This code uses either the "wlr-output-power-management-unstable-v1" or
 * "org-kde-kwin-dpms-manager" protocol.  Without one of these, XScreenSaver
 * cannot power down the screen after the configured timeout, and cannot know
 * to stop running hacks on a powered-down screen.
 *
 * These protocols are, of course, not supported by GNOME. As is traditional.
 *
 * Also! Under "wlr", the monitors are not automatically turned back on when
 * there is user activity.  So if this program crashes with the screens off,
 * good luck getting them to turn back on.  KDE does the right thing.
 *
 * GNOME does the following crap, because they have never seen a Wayland
 * protocol that they didn't think should be replaced with DBus instead:
 *
 *     busctl --user set-property \
 *       org.gnome.Mutter.DisplayConfig \
 *       /org/gnome/Mutter/DisplayConfig \
 *       org.gnome.Mutter.DisplayConfig PowerSaveMode \
 *       i \
 *       [ "0" or "1" ]
 *
 * So that's nice.
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
#include <X11/Intrinsic.h>

#include <wayland-client.h>
#include <wayland-server.h>
#include "wayland-protocols/wlr-output-power-management-unstable-v1-client-protocol.h"
#include "wayland-protocols/dpms-client-protocol.h"

#include "wayland-dpyI.h"
#include "wayland-dpms.h"
#include "types.h"
#include "blurb.h"

struct wayland_dpms {
  wayland_dpy *parent;
  struct wl_registry *reg;
  struct zwlr_output_power_manager_v1 *wlr_mgr;
  struct org_kde_kwin_dpms_manager    *kde_mgr;
  struct wl_list outputs;
};

typedef struct dpms_output {
  struct wl_list link;

  struct wl_output *wl_output;
  char *name;
  Bool wl_done;	/* wl_output: all info about this output has been sent */

  struct zwlr_output_power_v1   *wlr_out;
  enum zwlr_output_power_v1_mode wlr_mode;
  Bool wlr_failed; /* I have no idea what this means */

  struct org_kde_kwin_dpms   *kde_out;
  enum org_kde_kwin_dpms_mode kde_mode;
  Bool kde_done;

} dpms_output;


static void
wl_handle_name (void *data, struct wl_output *wl_output, const char *name)
{
  dpms_output *out = (dpms_output *) data;
  if (out->name) free (out->name);
  out->name = strdup (name);
  if (verbose_p > 2)
    fprintf (stderr, "%s: wayland: dpms: output %s\n", blurb(), name);
}

/* Even though we don't use any of these callback functions, they have to
   be included in the listener or it crashes.  How very. */

static void
wl_handle_geometry (void *data, struct wl_output *wl_output,
                    int32_t x, int32_t y,
                    int32_t physical_width, int32_t physical_height,
                    int32_t subpixel, const char *make, const char* model,
                    int32_t transform)
{
}

static void
wl_handle_mode (void *data, struct wl_output *wl_output,
                uint32_t flags, int32_t width, int32_t height, int32_t refresh)
{
}

static void
wl_handle_scale (void *data, struct wl_output *wl_output, int32_t scale)
{
}

static void
wl_handle_description (void *data, struct wl_output *wl_output,
                       const char *desc)
{
}

static void
wl_handle_done (void *data, struct wl_output *wl_output)
{
  dpms_output *out = (dpms_output *) data;
  out->wl_done = True;
}


static void
wlr_handle_mode (void *data,
                 struct zwlr_output_power_v1 *wlr_output_power,
                 enum zwlr_output_power_v1_mode mode)
{
  dpms_output *out = (dpms_output *) data;
  out->wlr_mode = mode;
  if (verbose_p > 2)
    fprintf (stderr, "%s: wayland: dpms: output %s is %s\n", blurb(),
             out->name,
             (out->wlr_mode == ZWLR_OUTPUT_POWER_V1_MODE_ON ? "on" : "off"));
}

static void
wlr_handle_failed (void *data, struct zwlr_output_power_v1 *wlr_output_power)
{
  dpms_output *out = (dpms_output *) data;
  out->wlr_failed = True;
  if (verbose_p > 2)
    fprintf (stderr, "%s: wayland: dpms: output %s is FAILED?\n", blurb(),
             out->name);
}


static void
kde_handle_supported (void *data, struct org_kde_kwin_dpms *dpms,
                      uint32_t supported)
{
}

static void
kde_handle_mode (void *data, struct org_kde_kwin_dpms *dpms, uint32_t mode)
{
  dpms_output *out = (dpms_output *) data;
  out->kde_mode = mode;
  if (verbose_p > 2)
    fprintf (stderr, "%s: wayland: dpms: output %s is %s\n", blurb(),
             out->name,
             (out->kde_mode == ORG_KDE_KWIN_DPMS_MODE_ON      ? "on"      :
              out->kde_mode == ORG_KDE_KWIN_DPMS_MODE_STANDBY ? "standby" :
              out->kde_mode == ORG_KDE_KWIN_DPMS_MODE_SUSPEND ? "suspend" :
              out->kde_mode == ORG_KDE_KWIN_DPMS_MODE_OFF     ? "off"     :
              "???"));
}

static void
kde_handle_done (void *data, struct org_kde_kwin_dpms *dpms)
{
  dpms_output *out = (dpms_output *) data;
  out->kde_done = True;
}


/* This is an iterator for all of the extensions that Wayland provides,
   so we can find the ones we are interested in.
 */
static void
handle_global (void *data, struct wl_registry *reg,
               uint32_t name, const char *iface, uint32_t version)
{
  wayland_dpms *state = (wayland_dpms *) data;

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
      static const struct zwlr_output_power_v1_listener wlr_listener = {
        .mode        = wlr_handle_mode,
        .failed      = wlr_handle_failed,
      };
      static const struct org_kde_kwin_dpms_listener kde_listener = {
        .supported   = kde_handle_supported,
        .mode        = kde_handle_mode,
        .done        = kde_handle_done,
      };

      dpms_output *out = (dpms_output *) calloc (sizeof (*out), 1);
      wl_list_insert (&state->outputs, &out->link);

      if (verbose_p > 2)
        fprintf (stderr, "%s: wayland: dpms: found: %s\n", blurb(), iface);

      /* First listener: properties of the wl_output object. */
      out->wl_output = wl_registry_bind (reg, name, &wl_output_interface,
                                         version);
      wl_output_add_listener (out->wl_output, &wl_listener, out);

      if (state->wlr_mgr)
        {
          /* Second listener: changes to its DPMS state. */
          out->wlr_out =
            zwlr_output_power_manager_v1_get_output_power (state->wlr_mgr,
                                                           out->wl_output);
          zwlr_output_power_v1_add_listener (out->wlr_out, &wlr_listener, out);
        }
      else if (state->kde_mgr)
        {
          /* Second listener: changes to its DPMS state. */
          out->kde_out =
            org_kde_kwin_dpms_manager_get (state->kde_mgr, out->wl_output);
          org_kde_kwin_dpms_add_listener (out->kde_out, &kde_listener, out);
        }
    }
  else if (!strcmp (iface, zwlr_output_power_manager_v1_interface.name))
    {
      if (verbose_p)
        fprintf (stderr, "%s: wayland: dpms: found: %s\n", blurb(), iface);
      state->wlr_mgr =
        wl_registry_bind (reg, name, &zwlr_output_power_manager_v1_interface,
                          version);
    }
  else if (!strcmp (iface, org_kde_kwin_dpms_manager_interface.name))
    {
      if (verbose_p)
        fprintf (stderr, "%s: wayland: dpms: found: %s\n", blurb(), iface);
      state->kde_mgr =
        wl_registry_bind (reg, name, &org_kde_kwin_dpms_manager_interface,
                          version);
    }
}


static void
handle_global_remove (void *data, struct wl_registry *registry, uint32_t name)
{
}


wayland_dpms *
wayland_dpms_init (wayland_dpy *dpy)
{
  static const struct wl_registry_listener listener = {
    .global        = handle_global,
    .global_remove = handle_global_remove,
  };
  wayland_dpms *state;
  if (!dpy) return NULL;

  state = (wayland_dpms *) calloc (sizeof (*state), 1);
  state->parent = dpy;
  wl_list_init (&state->outputs);

  state->reg = wl_display_get_registry (dpy->dpy);
  wl_registry_add_listener (state->reg, &listener, state);

  /* Do the first round trip to load the wl_outputs. */
  wayland_dpy_process_events (dpy, true);

  /* Now do more round trips until we have all the info about them. FFS! */
  while (1)
    {
      Bool all_done = True;
      dpms_output *out;
      wl_list_for_each (out, &state->outputs, link)
        {
          if (!out->wl_done)
            all_done = False;
          if (state->kde_mgr && !out->kde_done)
            all_done = False;
        }
      if (all_done) break;
      wayland_dpy_process_events (dpy, true);
    }

  if (!state->wlr_mgr && !state->kde_mgr)
    {
      fprintf (stderr,
               "%s: wayland: dpms: server doesn't implement \"%s\" or \"%s\"\n",
               blurb(),
               zwlr_output_power_manager_v1_interface.name,
               org_kde_kwin_dpms_manager_interface.name);
      wayland_dpms_free (state);
      return NULL;
    }

  return state;
}


void
wayland_dpms_free (wayland_dpms *state)
{
  dpms_output *out, *tmp;
  wl_list_for_each_safe (out, tmp, &state->outputs, link)
    {
      wl_list_remove (&out->link);
      /* #### out->wl_output ? */
      /* #### out->zwlr_output_power_v1, etc ? */
      free (out);
    }

  /* #### state->reg ? */
  /* #### state->zwlr_output_power_manager_v1, etc ? */
  free (state);
}


/* True if any head is enabled.
 */
Bool
wayland_monitor_powered_on_p (wayland_dpms *state)
{
  Bool on_p = False;
  dpms_output *out;

  if (!state) return on_p;

  wayland_dpy_process_events (state->parent, True);
  wl_list_for_each (out, &state->outputs, link)
    {
      if (out->wlr_out)
        {
          if (out->wlr_mode == ZWLR_OUTPUT_POWER_V1_MODE_ON)
            on_p = True;
        }
      else if (out->kde_out)
        {
          if (out->kde_mode == ORG_KDE_KWIN_DPMS_MODE_ON)
            on_p = True;
        }
      else
        abort();
    }

  return on_p;
}


/* Set all heads to 'enabled' or 'disabled'.
 */
void
wayland_monitor_power_on (wayland_dpms *state, Bool on_p)
{
  enum zwlr_output_power_v1_mode wlr_target = 
    (on_p ? ZWLR_OUTPUT_POWER_V1_MODE_ON : ZWLR_OUTPUT_POWER_V1_MODE_OFF);
  enum org_kde_kwin_dpms_mode    kde_target =
    (on_p ? ORG_KDE_KWIN_DPMS_MODE_ON    : ORG_KDE_KWIN_DPMS_MODE_OFF);
  dpms_output *out;

  if (!state) return;

  wl_list_for_each (out, &state->outputs, link)
    {
      if (out->wlr_out)
        {
          if (out->wlr_mode == wlr_target)
            continue;
          else
            zwlr_output_power_v1_set_mode (out->wlr_out, wlr_target);
        }
      else if (out->kde_out)
        {
          if (out->kde_mode == kde_target)
            continue;
          else
            org_kde_kwin_dpms_set (out->kde_out, kde_target);
        }
      else
        abort();

      if (verbose_p > 2)
        fprintf (stderr, "%s: wayland: dpms: set output %s to %s\n", blurb(),
                 out->name, (on_p ? "on" : "off"));
    }

  wayland_dpy_process_events (state->parent, True);
}

