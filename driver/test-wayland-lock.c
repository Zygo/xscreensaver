/* test-wayland-lock.c --- test harness for ext-session-lock-v1
 * xscreensaver, Copyright © 2025 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <stdio.h>

#include <X11/Xatom.h>
#include <X11/Intrinsic.h>

#ifndef HAVE_WAYLAND
# error Wayland is required
#endif

#include "wayland-dpy.h"
#include "wayland-lock.h"

#include "xscreensaver.h"
#include "resources.h"
#include "screens.h"
#include "atoms.h"

XrmDatabase db = 0;
char *progclass = "XScreenSaver";
Bool debug_p = True;

typedef struct {
  XtAppContext app;
  wayland_dpy  *wdpy;
  wayland_lock *wlock;
  int duration;
} wayland_lock_state;


static Window
get_window_cb (const char *name, void *closure)
{
  /* wayland_lock_state *state = (wayland_lock_state *) closure; */
  fprintf (stderr, "%s: get_window_cb unimplemented\n", blurb());
  return 0;
}

static void
reshape_cb (const char *name, unsigned int w, unsigned int h, void *closure)
{
  /* wayland_lock_state *state = (wayland_lock_state *) closure; */
  fprintf (stderr, "%s: reshape_cb unimplemented\n", blurb());
  abort();
}

static void
unlocked_cb (void *closure)
{
  /* wayland_lock_state *state = (wayland_lock_state *) closure; */
  fprintf (stderr, "%s: server unlocked us!\n", blurb());
  exit (1);
}



static void
wayland_dpy_cb (XtPointer closure, int *source, XtInputId *id)
{
  wayland_lock_state *state = (wayland_lock_state *) closure;
  wayland_dpy_process_events (state->wdpy, False);
}


static void
unlock_timer (XtPointer closure, XtIntervalId *id)
{
  wayland_lock_state *state = (wayland_lock_state *) closure;
  fprintf (stderr, "%s: unlocking...\n", blurb());
  wayland_unlock_screen (state->wlock);
}


static void
lock_timer (XtPointer closure, XtIntervalId *id)
{
  wayland_lock_state *state = (wayland_lock_state *) closure;

  fprintf (stderr, "%s: locking...\n", blurb());
  if (! wayland_lock_screen (state->wlock,
                             get_window_cb,
                             reshape_cb,
                             unlocked_cb,
                             state->wlock))
    {
      fprintf (stderr, "%s: FAILED\n", blurb());
      exit (1);
    }

  XtAppAddTimeOut (state->app, 1000 * state->duration, unlock_timer, state);
}


int
main (int argc, char **argv)
{
  XtAppContext app;
  Widget toplevel_shell = XtAppInitialize (&app, progclass, 0, 0,
					   &argc, argv, 0, 0, 0);
  Display *dpy = XtDisplay (toplevel_shell);
  wayland_dpy *wdpy = wayland_dpy_connect();
  wayland_lock *wlock = wayland_lock_init (wdpy);
  wayland_lock_state S, *state = &S;

  progname = argv[0];
  progclass = "XScreenSaver";
  db = XtDatabase (dpy);

  if (!wdpy)
    {
      fprintf (stderr, "%s: wayland: connection failed.\n", blurb());
      exit (1);
    }
  if (!wlock)
    {
      fprintf (stderr, "%s: wayland: locking unsupported.\n", blurb());
      exit (1);
    }

  verbose_p += 2;

  state->duration = 3;
  state->app      = app;
  state->wdpy     = wdpy;
  state->wlock    = wlock;

  XtAppAddInput (app, wayland_dpy_get_fd (wdpy),
                 (XtPointer) (XtInputReadMask | XtInputExceptMask),
                 wayland_dpy_cb, (XtPointer) state);

  XtAppAddTimeOut (app, 1000 * state->duration, lock_timer, state);

  fprintf (stderr, "%s: locking in %d sec...\n", blurb(), state->duration);
  while (1)
    {
      XEvent event;
      XtAppNextEvent (app, &event);
      XtDispatchEvent (&event);
    }
}
