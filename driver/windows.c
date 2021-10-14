/* windows.c --- turning the screen black; dealing with visuals, virtual roots.
 * xscreensaver, Copyright © 1991-2021 Jamie Zawinski <jwz@jwz.org>
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

#ifdef HAVE_UNAME
# include <sys/utsname.h>	/* for uname() */
#endif /* HAVE_UNAME */

#include <stdio.h>
#include <pwd.h>		/* for getpwuid() */
#include <X11/Xlib.h>
#include <X11/Xutil.h>		/* for XSetClassHint() */
#include <X11/Xatom.h>
#include <X11/Intrinsic.h>
#include <time.h>

#ifdef HAVE_XF86VMODE
# include <X11/extensions/xf86vmode.h>
#endif /* HAVE_XF86VMODE */

#ifdef HAVE_XINERAMA
# include <X11/extensions/Xinerama.h>
#endif /* HAVE_XINERAMA */

#include "xscreensaver.h"
#include "atoms.h"
#include "visual.h"
#include "screens.h"
#include "fade.h"
#include "resources.h"
#include "xft.h"
#include "font-retry.h"


extern int kill (pid_t, int);		/* signal() is in sys/signal.h... */

#ifdef _VROOT_H_
ERROR!  You must not include vroot.h in this file.
#endif


static void reset_watchdog_timer (saver_info *si);

void
store_saver_status (saver_info *si)
{
  /* The contents of XA_SCREENSAVER_STATUS has LOCK/BLANK/0 in the first slot,
     the time at which that state began in the second slot, and the ordinal of
     the running hacks on each screen (1-based) in subsequent slots.  Since
     we don't know the blank-versus-lock status here, we leave whatever was
     there before unchanged: it will be updated by "xscreensaver".

     XA_SCREENSAVER_STATUS is stored on the (real) root window of screen 0.

     XA_SCREENSAVER_VERSION and XA_SCREENSAVER_ID are stored on the unmapped
     window created by the "xscreensaver" process.  ClientMessage events are
     sent to that window, and the responses are sent via the
     XA_SCREENSAVER_RESPONSE property on it.

     These properties are not used on the windows created by "xscreensaver-gfx"
     for use by the display hacks.

     See the different version of this function in xscreensaver.c.
   */
  Display *dpy = si->dpy;
  Window w = RootWindow (dpy, 0);  /* always screen 0 */
  Atom type;
  unsigned char *dataP = 0;
  PROP32 *status = 0;
  int format;
  unsigned long nitems, bytesafter;
  int nitems2 = si->nscreens + 2;
  int i;

  /* Read the old property, so we can change just our parts. */
  XGetWindowProperty (dpy, w,
                      XA_SCREENSAVER_STATUS,
                      0, 999, False, XA_INTEGER,
                      &type, &format, &nitems, &bytesafter,
                      &dataP);

  status = (PROP32 *) calloc (nitems2, sizeof(PROP32));

  if (dataP && type == XA_INTEGER && nitems >= 3)
    {
      status[0] = ((PROP32 *) dataP)[0];
      status[1] = ((PROP32 *) dataP)[1];
    }

  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      status[2 + i] = ssi->current_hack + 1;  /* 1-based */
    }

  XChangeProperty (si->dpy, w, XA_SCREENSAVER_STATUS, XA_INTEGER, 32,
                   PropModeReplace, (unsigned char *) status, nitems2);
  XSync (dpy, False);

  if (si->prefs.debug_p && si->prefs.verbose_p)
    {
      int i;
      fprintf (stderr, "%s: wrote status property: 0x%lx: %s", blurb(),
               (unsigned long) w,
               (status[0] == XA_LOCK  ? "LOCK" :
                status[0] == XA_BLANK ? "BLANK" :
                status[0] == 0 ? "0" : "???"));
      for (i = 1; i < nitems; i++)
        fprintf (stderr, ", %lu", status[i]);
      fprintf (stderr, "\n");
    }

  free (status);
  if (dataP)
    XFree (dataP);
}


static void
initialize_screensaver_window_1 (saver_screen_info *ssi)
{
  saver_info *si = ssi->global;
  saver_preferences *p = &si->prefs;
  Bool install_cmap_p = ssi->install_cmap_p;   /* not p->install_cmap_p */

  /* This resets the screensaver window as fully as possible, since there's
     no way of knowing what some random client may have done to us in the
     meantime.  We could just destroy and recreate the window, but that has
     its own set of problems.  (Update: that's exactly what we're doing
     these days.)
   */
  XColor black;
  XSetWindowAttributes attrs;
  unsigned long attrmask;
  static Bool printed_visual_info = False;  /* only print the message once. */
  Window horked_window = 0;

  black.red = black.green = black.blue = 0;

  if (ssi->cmap == DefaultColormapOfScreen (ssi->screen))
    ssi->cmap = 0;

  if (ssi->current_visual != DefaultVisualOfScreen (ssi->screen))
    /* It's not the default visual, so we have no choice but to install. */
    install_cmap_p = True;

  if (install_cmap_p)
    {
      if (! ssi->cmap)
	{
	  ssi->cmap = XCreateColormap (si->dpy,
				       RootWindowOfScreen (ssi->screen),
				      ssi->current_visual, AllocNone);
	  if (! XAllocColor (si->dpy, ssi->cmap, &black)) abort ();
	  ssi->black_pixel = black.pixel;
	}
    }
  else
    {
      Colormap def_cmap = DefaultColormapOfScreen (ssi->screen);
      if (ssi->cmap)
	{
	  XFreeColors (si->dpy, ssi->cmap, &ssi->black_pixel, 1, 0);
	  if (ssi->cmap != def_cmap)
	    XFreeColormap (si->dpy, ssi->cmap);
	}
      ssi->cmap = def_cmap;
      ssi->black_pixel = BlackPixelOfScreen (ssi->screen);
    }

  attrmask = (CWOverrideRedirect | CWEventMask | CWBackingStore | CWColormap |
	      CWBackPixel | CWBackingPixel | CWBorderPixel);
  attrs.override_redirect = True;

  attrs.event_mask = (KeyPressMask | KeyReleaseMask |
		      ButtonPressMask | ButtonReleaseMask |
		      PointerMotionMask);

  attrs.backing_store = Always;
  attrs.colormap = ssi->cmap;
  attrs.background_pixel = ssi->black_pixel;
  attrs.backing_pixel = ssi->black_pixel;
  attrs.border_pixel = ssi->black_pixel;

  printed_visual_info = True;  /* Too noisy */

  if (!p->verbose_p || printed_visual_info)
    ;
  else if (ssi->current_visual == DefaultVisualOfScreen (ssi->screen))
    {
      fprintf (stderr, "%s: %d: visual ", blurb(), ssi->number);
      describe_visual (stderr, ssi->screen, ssi->current_visual,
		       install_cmap_p);
    }
  else
    {
      fprintf (stderr, "%s: using visual:   ", blurb());
      describe_visual (stderr, ssi->screen, ssi->current_visual,
		       install_cmap_p);
      fprintf (stderr, "%s: default visual: ", blurb());
      describe_visual (stderr, ssi->screen,
		       DefaultVisualOfScreen (ssi->screen),
		       ssi->install_cmap_p);
    }
  printed_visual_info = True;

  if (ssi->error_dialog)
    {
      defer_XDestroyWindow (si->app, si->dpy, ssi->error_dialog);
      ssi->error_dialog = 0;
    }

  if (ssi->screensaver_window)
    {
      XWindowChanges changes;
      unsigned int changesmask = CWX|CWY|CWWidth|CWHeight|CWBorderWidth;
      changes.x = ssi->x;
      changes.y = ssi->y;
      changes.width = ssi->width;
      changes.height = ssi->height;
      changes.border_width = 0;

      /* XConfigureWindow and XChangeWindowAttributes can fail if a hack did
         something weird to the window.  In that case, we must destroy and
         re-create it. */
      if (! (XConfigureWindow (si->dpy, ssi->screensaver_window,
                               changesmask, &changes) &&
             XChangeWindowAttributes (si->dpy, ssi->screensaver_window,
                                      attrmask, &attrs)))
        {
          horked_window = ssi->screensaver_window;
          ssi->screensaver_window = 0;
        }
    }

  if (!ssi->screensaver_window)
    {
      ssi->screensaver_window =
	XCreateWindow (si->dpy, RootWindowOfScreen (ssi->screen),
                       ssi->x, ssi->y, ssi->width, ssi->height,
                       0, ssi->current_depth, InputOutput,
		       ssi->current_visual, attrmask, &attrs);
      xscreensaver_set_wm_atoms (si->dpy, ssi->screensaver_window,
                                 ssi->width, ssi->height, 0);

      if (horked_window)
        {
          fprintf (stderr,
            "%s: someone horked our saver window (0x%lx)!  Recreating it...\n",
                   blurb(), (unsigned long) horked_window);
          defer_XDestroyWindow (si->app, si->dpy, horked_window);
        }

      if (p->verbose_p > 1)
	fprintf (stderr, "%s: %d: saver window is 0x%lx\n",
                 blurb(), ssi->number,
                 (unsigned long) ssi->screensaver_window);
    }

  if (!ssi->cursor)
    {
      Pixmap bit =
        XCreatePixmapFromBitmapData (si->dpy, ssi->screensaver_window,
                                     "\000", 1, 1,
                                     BlackPixelOfScreen (ssi->screen),
                                     BlackPixelOfScreen (ssi->screen),
                                     1);
      ssi->cursor = XCreatePixmapCursor (si->dpy, bit, bit, &black, &black,
					 0, 0);
      XFreePixmap (si->dpy, bit);
    }

  XSetWindowBackground (si->dpy, ssi->screensaver_window, ssi->black_pixel);

  if (si->demoing_p)
    XUndefineCursor (si->dpy, ssi->screensaver_window);
  else
    XDefineCursor (si->dpy, ssi->screensaver_window, ssi->cursor);
}


void
initialize_screensaver_window (saver_info *si)
{
  int i;
  for (i = 0; i < si->nscreens; i++)
    initialize_screensaver_window_1 (&si->screens[i]);
}


static void
raise_window (saver_screen_info *ssi)
{
  saver_info *si = ssi->global;
  if (ssi->error_dialog)
    {
      /* Make the error be topmost, and the saver be directly below it. */
      Window windows[2];
      windows[0] = ssi->error_dialog;
      windows[1] = ssi->screensaver_window;
      XMapRaised (si->dpy, windows[0]);
      XRestackWindows (si->dpy, windows, countof(windows));
    }
  else
    XMapRaised (si->dpy, ssi->screensaver_window);

  if (ssi->cmap)
    XInstallColormap (si->dpy, ssi->cmap);
}


/* Called when the RANDR (Resize and Rotate) extension tells us that
   the size of the screen has changed while the screen was blanked.
   Call update_screen_layout() first, then call this to synchronize
   the size of the saver windows to the new sizes of the screens.
 */
void
resize_screensaver_window (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  int i;

  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      XWindowAttributes xgwa;

      /* Make sure a window exists -- it might not if a monitor was just
         added for the first time.
       */
      if (! ssi->screensaver_window)
        {
          initialize_screensaver_window_1 (ssi);
          if (p->verbose_p)
            fprintf (stderr,
                     "%s: %d: newly added window 0x%lx %dx%d+%d+%d\n",
                     blurb(), i, (unsigned long) ssi->screensaver_window,
                     ssi->width, ssi->height, ssi->x, ssi->y);
        }

      /* Make sure the window is the right size -- it might not be if
         the monitor changed resolution, or if a badly-behaved hack
         screwed with it.
       */
      XGetWindowAttributes (si->dpy, ssi->screensaver_window, &xgwa);
      if (xgwa.x      != ssi->x ||
          xgwa.y      != ssi->y ||
          xgwa.width  != ssi->width ||
          xgwa.height != ssi->height)
        {
          XWindowChanges changes;
          unsigned int changesmask = CWX|CWY|CWWidth|CWHeight|CWBorderWidth;
          changes.x      = ssi->x;
          changes.y      = ssi->y;
          changes.width  = ssi->width;
          changes.height = ssi->height;
          changes.border_width = 0;

          if (p->verbose_p)
            fprintf (stderr,
                     "%s: %d: resize 0x%lx from %dx%d+%d+%d to %dx%d+%d+%d\n",
                     blurb(), i, (unsigned long) ssi->screensaver_window,
                     xgwa.width, xgwa.height, xgwa.x, xgwa.y,
                     ssi->width, ssi->height, ssi->x, ssi->y);

          if (! XConfigureWindow (si->dpy, ssi->screensaver_window,
                                  changesmask, &changes))
            fprintf (stderr, "%s: %d: someone horked our saver window"
                     " (0x%lx)!  Unable to resize it!\n",
                     blurb(), i, (unsigned long) ssi->screensaver_window);
        }

      /* Now (if blanked) make sure that it's mapped and running a hack --
         it might not be if we just added it.  (We also might be re-using
         an old window that existed for a previous monitor that was
         removed and re-added.)

         Note that spawn_screenhack() calls select_visual() which may destroy
         and re-create the window via initialize_screensaver_window_1().
       */
      raise_window (ssi);
      XSync (si->dpy, False);
      if (! ssi->pid)
        spawn_screenhack (ssi);
    }

  /* Kill off any savers running on no-longer-extant monitors.
   */
  for (; i < si->ssi_count; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      if (ssi->pid)
        kill_screenhack (ssi);
      if (ssi->screensaver_window)
        {
          XUnmapWindow (si->dpy, ssi->screensaver_window);
        }
    }
}


static void 
raise_windows (saver_info *si)
{
  int i;
  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      raise_window (ssi);
    }
}


/* Called only once, before the main loop begins.
 */
void 
blank_screen (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  Bool user_active_p = False;
  int i;

  initialize_screensaver_window (si);
  sync_server_dpms_settings (si->dpy, p);

  if (p->fade_p &&
      !si->demoing_p &&
      !si->emergency_p)
    {
      Window *current_windows = (Window *)
        malloc (si->nscreens * sizeof(*current_windows));
      if (!current_windows) abort();

      for (i = 0; i < si->nscreens; i++)
	{
	  saver_screen_info *ssi = &si->screens[i];
	  current_windows[i] = ssi->screensaver_window;
	  /* Ensure that the default background of the window is really black,
	     not a pixmap or something.  (This does not clear the window.) */
	  XSetWindowBackground (si->dpy, ssi->screensaver_window,
				ssi->black_pixel);
	}

      if (p->verbose_p) fprintf (stderr, "%s: fading...\n", blurb());

      /* This will take several seconds to complete. */
      user_active_p = fade_screens (si->app, si->dpy,
                                    current_windows, si->nscreens,
                                    p->fade_seconds / 1000.0,
                                    True,  /* out_p */
                                    True,  /* from_desktop_p */
                                    &si->fade_state);
      free (current_windows);

      if (!p->verbose_p)
        ;
      else if (user_active_p)
        fprintf (stderr, "%s: fading aborted\n", blurb());
      else
        fprintf (stderr, "%s: fading done\n", blurb());
    }

  raise_windows (si);
  reset_watchdog_timer (si);

  /* user_active_p means that the user aborted the fade-out -- but that does
     not mean that we are necessarily about to exit.  If we are locking, then
     the user activity will cause the unlock dialog to appear, but
     authentication might not succeed. */

  for (i = 0; i < si->nscreens; i++)
    /* This also queues each screen's cycle_timer. */
    spawn_screenhack (&si->screens[i]);

  /* Turn off "next" and "prev" modes after they have happened once. */
   if (si->selection_mode < 0)
     si->selection_mode = 0;

  /* If we are blanking only, optionally power down monitor right now. */
  if (p->mode == BLANK_ONLY &&
      p->dpms_quickoff_p)
    monitor_power_on (si, False);
}


/* Called only once, upon receipt of SIGTERM, just before exiting.
 */
void
unblank_screen (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  Bool unfade_p = p->unfade_p;
  int i;

  monitor_power_on (si, True);

  if (si->demoing_p)
    unfade_p = False;

  if (unfade_p)
    {
      double seconds = p->fade_seconds / 1000.0;
      double ratio = 1/3.0;
      Window *current_windows = (Window *)
	calloc(sizeof(Window), si->nscreens);
      Bool interrupted_p = False;

      for (i = 0; i < si->nscreens; i++)
	{
	  saver_screen_info *ssi = &si->screens[i];
	  current_windows[i] = ssi->screensaver_window;
	}

      if (p->verbose_p) fprintf (stderr, "%s: unfading...\n", blurb());

      monitor_power_on (si, True);

      /* When we fade in to the desktop, first fade out from the saver to
         black, then fade in from black to the desktop. */
      interrupted_p = fade_screens (si->app, si->dpy,
                                    current_windows, si->nscreens,
                                    seconds * ratio,
                                    True,  /* out_p */
                                    False, /* from_desktop_p */
                                    &si->fade_state);
      if (! interrupted_p)
        interrupted_p = fade_screens (si->app, si->dpy,
                                      current_windows, si->nscreens,
                                      seconds * (1-ratio),
                                      False, /* out_p */
                                      False, /* from_desktop_p */
                                      &si->fade_state);
      free (current_windows);

      if (p->verbose_p)
        fprintf (stderr, "%s: unfading done%s\n", blurb(),
                 (interrupted_p ? " (interrupted)" : ""));
    }
  else
    {
      for (i = 0; i < si->nscreens; i++)
	{
	  saver_screen_info *ssi = &si->screens[i];
	  if (ssi->cmap)
	    {
	      Colormap c = DefaultColormapOfScreen (ssi->screen);
	      /* avoid technicolor */
              XSetWindowBackground (si->dpy, ssi->screensaver_window,
                                    BlackPixelOfScreen (ssi->screen));
	      XClearWindow (si->dpy, ssi->screensaver_window);
	      if (c) XInstallColormap (si->dpy, c);
	    }
	  XUnmapWindow (si->dpy, ssi->screensaver_window);
	}
    }
}


static Visual *
get_screen_gl_visual (saver_info *si, int real_screen_number)
{
  int nscreens = ScreenCount (si->dpy);

  if (! si->best_gl_visuals)
    {
      int i;
      si->best_gl_visuals = (Visual **) 
        calloc (nscreens + 1, sizeof (*si->best_gl_visuals));

      for (i = 0; i < nscreens; i++)
        if (! si->best_gl_visuals[i])
          si->best_gl_visuals[i] = 
            get_best_gl_visual (si, ScreenOfDisplay (si->dpy, i));
    }

  if (real_screen_number < 0 || real_screen_number >= nscreens) abort();
  return si->best_gl_visuals[real_screen_number];
}


Bool
select_visual (saver_screen_info *ssi, const char *visual_name)
{
  XWindowAttributes xgwa;
  saver_info *si = ssi->global;
  saver_preferences *p = &si->prefs;
  Bool install_cmap_p = p->install_cmap_p;
  Bool was_installed_p = (ssi->cmap != DefaultColormapOfScreen(ssi->screen));
  Visual *new_v = 0;
  Bool got_it;

  /* On some systems (most recently, MacOS X) OpenGL programs get confused
     when you kill one and re-start another on the same window.  So maybe
     it's best to just always destroy and recreate the xscreensaver window
     when changing hacks, instead of trying to reuse the old one?
   */
  Bool always_recreate_window_p = True;

  get_screen_gl_visual (si, 0);   /* let's probe all the GL visuals early */

  /* We make sure the existing window is actually on ssi->screen before
     trying to use it, in case things moved around radically when monitors
     were added or deleted.  If we don't do this we could get a BadMatch
     even though the depths match.  I think.
   */
  memset (&xgwa, 0, sizeof(xgwa));
  if (ssi->screensaver_window)
    XGetWindowAttributes (si->dpy, ssi->screensaver_window, &xgwa);

  if (visual_name && *visual_name)
    {
      if (!strcasecmp(visual_name, "default-i"))
	{
	  visual_name = "default";
	  install_cmap_p = True;
	}
      else if (!strcasecmp(visual_name, "default-n"))
	{
	  visual_name = "default";
	  install_cmap_p = False;
	}
      else if (!strcasecmp(visual_name, "GL"))
        {
          new_v = get_screen_gl_visual (si, ssi->real_screen_number);
          if (!new_v && p->verbose_p)
            fprintf (stderr, "%s: no GL visuals\n", blurb());
        }

      if (!new_v)
        new_v = get_visual (ssi->screen, visual_name, True, False);
    }
  else
    {
      new_v = ssi->default_visual;
    }

  got_it = !!new_v;

  if (new_v && new_v != DefaultVisualOfScreen(ssi->screen))
    /* It's not the default visual, so we have no choice but to install. */
    install_cmap_p = True;

  ssi->install_cmap_p = install_cmap_p;

  if ((ssi->screen != xgwa.screen) ||
      (new_v &&
       (always_recreate_window_p ||
        (ssi->current_visual != new_v) ||
        (install_cmap_p != was_installed_p))))
    {
      Colormap old_c = ssi->cmap;
      Window old_w = ssi->screensaver_window;
      if (! new_v) 
        new_v = ssi->current_visual;

      if (p->verbose_p)
	{
#if 0
	  fprintf (stderr, "%s: %d: visual ", blurb(), ssi->number);
	  describe_visual (stderr, ssi->screen, new_v, install_cmap_p);
#endif
#if 0
	  fprintf (stderr, "%s:                  from ", blurb());
	  describe_visual (stderr, ssi->screen, ssi->current_visual,
			   was_installed_p);
#endif
	}

      ssi->current_visual = new_v;
      ssi->current_depth = visual_depth(ssi->screen, new_v);
      ssi->cmap = 0;
      ssi->screensaver_window = 0;

      initialize_screensaver_window_1 (ssi);
      raise_window (ssi);

      /* Now we can destroy the old window without horking our grabs. */
      defer_XDestroyWindow (si->app, si->dpy, old_w);

      if (p->verbose_p > 1)
	fprintf (stderr, "%s: %d: destroyed old saver window 0x%lx\n",
		 blurb(), ssi->number, (unsigned long) old_w);

      if (old_c &&
	  old_c != DefaultColormapOfScreen (ssi->screen))
	XFreeColormap (si->dpy, old_c);
    }

  return got_it;
}


/* Synchronize the contents of si->ssi to the current state of the monitors.
   Doesn't change anything if nothing has changed; otherwise, alters and
   reuses existing saver_screen_info structs as much as possible.
   Returns True if anything changed.
 */
Bool
update_screen_layout (saver_info *si)
{
  monitor **monitors = scan_monitors (si->dpy);
  int count = 0;
  int good_count = 0;
  int i, j;
  int seen_screens[100] = { 0, };

  if (! monitor_layouts_differ_p (monitors, si->monitor_layout))
    {
      free_monitors (monitors);
      return False;
    }

  free_monitors (si->monitor_layout);
  si->monitor_layout = monitors;
  check_monitor_sanity (si->monitor_layout);

  while (monitors[count])
    {
      if (monitors[count]->sanity == S_SANE)
        good_count++;
      count++;
    }

  if (si->ssi_count == 0)
    {
      si->ssi_count = 10;
      si->screens = (saver_screen_info *)
        calloc (sizeof(*si->screens), si->ssi_count);
    }

  if (si->ssi_count < count)
    {
      si->screens = (saver_screen_info *)
        realloc (si->screens, sizeof(*si->screens) * count);
      memset (si->screens + si->ssi_count, 0,
              sizeof(*si->screens) * (count - si->ssi_count));
      si->ssi_count = count;
    }

  if (! si->screens) abort();

  si->nscreens = good_count;

  /* Regenerate the list of GL visuals as needed. */
  if (si->best_gl_visuals)
    free (si->best_gl_visuals);
  si->best_gl_visuals = 0;

  for (i = 0, j = 0; i < count; i++)
    {
      monitor *m = monitors[i];
      saver_screen_info *ssi = &si->screens[j];
      int sn;
      if (monitors[i]->sanity != S_SANE) continue;

      ssi->global = si;
      ssi->number = j;

      sn = screen_number (m->screen);
      ssi->screen = m->screen;
      ssi->real_screen_number = sn;
      ssi->real_screen_p = (seen_screens[sn] == 0);
      seen_screens[sn]++;

      ssi->default_visual =
	get_visual_resource (ssi->screen, "visualID", "VisualID", False);
      ssi->current_visual = ssi->default_visual;
      ssi->current_depth = visual_depth (ssi->screen, ssi->current_visual);

      ssi->x      = m->x;
      ssi->y      = m->y;
      ssi->width  = m->width;
      ssi->height = m->height;

# ifndef DEBUG_MULTISCREEN
      {
        saver_preferences *p = &si->prefs;
        if (p->debug_p
#  ifdef QUAD_MODE
            && !p->quad_p
#  endif
            )
          ssi->width /= 2;
      }
# endif

      j++;
    }

  return True;
}


/* When the screensaver is active, this timer will periodically change
   the running program.  Each screen has its own timer.
 */
void
cycle_timer (XtPointer closure, XtIntervalId *id)
{
  saver_screen_info *ssi = (saver_screen_info *) closure;
  saver_info *si = ssi->global;

  if (ssi->error_dialog)
    {
      defer_XDestroyWindow (si->app, si->dpy, ssi->error_dialog);
      ssi->error_dialog = 0;
    }

  maybe_reload_init_file (si);
  kill_screenhack (ssi);
  raise_window (ssi);

  /* We could do a fade-out of just this screen here; but that would only work
     if the fade method is SHM, not gamma or colormap.  It would also only
     look right if the cycle timers never fire at the same time, which is
     currently the case. */

  XSync (si->dpy, False);
  spawn_screenhack (ssi);  /* This also re-adds the cycle_id timer */
}


/* Called when a screenhack has exited unexpectedly.
   We print a notification on the window, and in a little while, launch
   a new hack (rather than waiting for the cycle timer to fire).
 */
void
screenhack_obituary (saver_screen_info *ssi,
                     const char *name, const char *error)
{
  saver_info *si = ssi->global;
  saver_preferences *p = &si->prefs;
  Time how_long = p->cycle;
  Time max = 1000 * 60;		/* Message stays up no longer than this */
  Window window;
  Visual *visual;
  XSetWindowAttributes attrs;
  XWindowChanges changes;
  unsigned long attrmask;
  XftFont *font;
  XftColor fg;
  XftDraw *xftdraw;
  XGlyphInfo overall;
  XGCValues gcv;
  GC gc;
  char *fn, *cn;
  char buf[255];
  int x, y, pad;
  int bw = 4;
  Colormap cmap;

  /* Restart the cycle timer, to take down the error dialog and launch
     a new hack.
   */
  if (how_long > max)
    how_long = max;
  if (ssi->cycle_id)
    XtRemoveTimeOut (ssi->cycle_id);
  ssi->cycle_id =
    XtAppAddTimeOut (si->app, how_long, cycle_timer, (XtPointer) ssi);
  ssi->cycle_at = time ((time_t *) 0) + how_long / 1000;
  if (p->verbose_p)
    fprintf (stderr, "%s: %d: cycling in %lu sec\n", blurb(), ssi->number,
             how_long / 1000);

  /* Render an error message while we wait.

     We can't just render text on top of ssi->screensaver_window because
     if there was an OpenGL hack running on it, Xlib graphics might not
     show up at all.  Likewise, creating a sub-window doesn't work.
     So it must be a top-level override-redirect window atop the saver.
   */
  cmap = ssi->cmap ? ssi->cmap : DefaultColormapOfScreen (ssi->screen);
  window = ssi->error_dialog;
  if (window) defer_XDestroyWindow (si->app, si->dpy, window);
  attrs.override_redirect = True;
  attrs.background_pixel = ssi->black_pixel;
  attrs.border_pixel = ssi->black_pixel;
  attrs.backing_store = Always;
  attrs.colormap = cmap;
  attrmask = (CWOverrideRedirect | CWBackPixel | CWBorderPixel | 
              CWBackingStore | CWColormap);
  visual = ssi->current_visual;
  window = ssi->error_dialog =
    XCreateWindow (si->dpy, RootWindowOfScreen (ssi->screen),
                   0, 0, 1, 1, 0, ssi->current_depth, InputOutput, visual,
                   attrmask, &attrs);

  fn = get_string_resource (si->dpy, "errorFont", "Font");
  cn = get_string_resource (si->dpy, "errorColor", "Color");
  if (!fn || !*fn) fn = strdup ("monospace bold 16");
  if (!cn || !*cn) cn = strdup ("#FF0000");

  font = load_xft_font_retry (si->dpy, screen_number (ssi->screen), fn);
  XftColorAllocName (si->dpy, visual, cmap, cn, &fg);
  xftdraw = XftDrawCreate (si->dpy, window, visual, cmap);

  gcv.foreground =
    get_pixel_resource (si->dpy, cmap, "errorColor", "Color");
  gcv.line_width = bw;
  gc = XCreateGC (si->dpy, window, GCForeground | GCLineWidth, &gcv);

  if (name && *name)
    sprintf (buf, "\"%.100s\" %.100s", name, error);
  else
    sprintf (buf, "%.100s", error);

  XftTextExtentsUtf8 (si->dpy, font, (FcChar8 *) buf, strlen(buf), &overall);
  x = (ssi->width - overall.width) / 2;
  y = (ssi->height - overall.height) / 2 + font->ascent;
  pad = bw + font->ascent * 2;

  attrmask = CWX | CWY | CWWidth | CWHeight;
  changes.x = ssi->x + x - pad;
  changes.y = ssi->y + y - (font->ascent + pad);
  changes.width = overall.width + pad * 2;
  changes.height = font->ascent + font->descent + pad * 2;
  XConfigureWindow (si->dpy, window, attrmask, &changes);
  xscreensaver_set_wm_atoms (si->dpy, window, changes.width, changes.height,
                             ssi->screensaver_window);
  XMapRaised (si->dpy, window);
  XClearWindow (si->dpy, window);

  XDrawRectangle (si->dpy, window, gc, gcv.line_width/2, gcv.line_width/2,
                  changes.width - gcv.line_width,
                  changes.height - gcv.line_width);

  x = pad;
  y = font->ascent + pad;
  XftDrawStringUtf8 (xftdraw, &fg, font, x, y, (FcChar8 *) buf, strlen (buf));
  XSync (si->dpy, False);

  XFreeGC (si->dpy, gc);
  XftDrawDestroy (xftdraw);
  /* XftColorFree (si->dpy, visual, cmap, &fg); */
  XftFontClose (si->dpy, font);
  free (fn);
  free (cn);
}


/* This timer goes off every few minutes to try and clean up anything that has
   gone wrong.  It raises the windows, in case some other window has been
   mapped on top of them, and re-sets the server's DPMS settings.

   Maybe we should respond to Expose events to detect when another window has
   raised above us and re-raise ourselves sooner.  But that would result in us
   fighting against "xscreensaver-auth" which tries very hard to be on top.
 */
static void
watchdog_timer (XtPointer closure, XtIntervalId *id)
{
  saver_info *si = (saver_info *) closure;
  saver_preferences *p = &si->prefs;
  Bool running_p, on_p;

  /* If the DPMS settings on the server have changed, change them back to
     what ~/.xscreensaver says they should be. */
  sync_server_dpms_settings (si->dpy, p);

  if (si->prefs.debug_p)
    fprintf (stderr, "%s: watchdog timer raising screen\n", blurb());

  raise_windows (si);

  running_p = any_screenhacks_running_p (si);
  on_p = monitor_powered_on_p (si->dpy);
  if (running_p && !on_p)
    {
      int i;
      if (si->prefs.verbose_p)
        fprintf (stderr,
                 "%s: monitor has powered down; killing running hacks\n",
                 blurb());
      for (i = 0; i < si->nscreens; i++)
        kill_screenhack (&si->screens[i]);
      /* Do not clear current_hack here. */
    }
  else if (!running_p && on_p)
    {
      /* If the hack number is set but no hack is running, it is because the
         hack was killed when the monitor powered off, above.  This assumes
         that kill_screenhack() clears pid but not current_hack.  Start the
         hack going again.  The cycle_timer will also do this (unless "cycle"
         is 0) but watchdog_timer runs more frequently.
       */
      if (si->screens[0].current_hack >= 0)
        {
          int i;
          if (si->prefs.verbose_p)
            fprintf (stderr,
                     "%s: monitor has powered back on; re-launching hacks\n",
                     blurb());
          for (i = 0; i < si->nscreens; i++)
            spawn_screenhack (&si->screens[i]);
        }
    }

  /* Re-schedule this timer.  The watchdog timer defaults to a bit less
     than the hack cycle period, but is never longer than one hour.
   */
  si->watchdog_id = 0;
  reset_watchdog_timer (si);
}


static void
reset_watchdog_timer (saver_info *si)
{
  saver_preferences *p = &si->prefs;

  if (si->watchdog_id)
    {
      XtRemoveTimeOut (si->watchdog_id);
      si->watchdog_id = 0;
    }

  if (p->watchdog_timeout <= 0) return;
  si->watchdog_id = XtAppAddTimeOut (si->app, p->watchdog_timeout,
                                     watchdog_timer, (XtPointer) si);
  if (p->debug_p)
    fprintf (stderr, "%s: restarting watchdog_timer (%ld, %ld)\n",
             blurb(), p->watchdog_timeout, si->watchdog_id);
}
