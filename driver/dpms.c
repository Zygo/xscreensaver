/* dpms.c --- syncing the X Display Power Management System values
 * xscreensaver, Copyright © 2001-2025 Jamie Zawinski <jwz@jwz.org>
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

#include <stdio.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>

#include "xscreensaver.h"

#ifdef HAVE_WAYLAND
# include "wayland-dpms.h"
#endif

/* Disable the X11 built-in screen saver. This is not directly related
   to DPMS, but it does need to be prevented from fighting with us.
  */
static Bool
disable_builtin_saver (Display *dpy)
{
  int otimeout   = -1;
  int ointerval  = -1;
  int oblanking  = -1;
  int oexposures = -1;
  XGetScreenSaver (dpy, &otimeout, &ointerval, &oblanking, &oexposures);
  if (otimeout == 0 && ointerval == 0 && oblanking == 0 && oexposures == 0)
    {
      if (verbose_p > 1)
        fprintf (stderr, "%s: builtin saver already disabled\n", blurb());
      return False;
    }
  else
    {
      if (verbose_p)
        fprintf (stderr, "%s: disabling server's builtin saver\n", blurb());
      XSetScreenSaver (dpy, 0, 0, 0, 0);
      XForceScreenSaver (dpy, ScreenSaverReset);
      return True;
    }
}


#ifndef HAVE_DPMS_EXTENSION   /* almost the whole file */

void
sync_server_dpms_settings (saver_info *si)
{
  sync_server_dpms_settings_1 (si->dpy, &si->prefs);
}

void
sync_server_dpms_settings_1 (Display *dpy, struct saver_preferences *p)
{
  disable_builtin_saver (dpy);
  if (p->verbose_p)
    fprintf (stderr, "%s: DPMS not supported at compile time\n", blurb());
}

Bool monitor_powered_on_p (saver_info *si)
{
# ifdef HAVE_WAYLAND
  if (si->wayland_dpms)
    return wayland_monitor_powered_on_p (si->wayland_dpms);
# endif
  if (verbose_p > 1)
    fprintf (stderr,
             "%s: DPMS disabled at compile time, assuming monitor on\n",
             blurb());
  return True;
}

void monitor_power_on (saver_info *si, Bool on_p)
{
# ifdef HAVE_WAYLAND
  if (si->wayland_dpms)
    wayland_monitor_power_on (si->wayland_dpms, on_p);
  else
# endif
  if (verbose_p > 1)
    fprintf (stderr,
             "%s: DPMS disabled at compile time, not turning monitor %s\n",
             blurb(), (on ? "on" : "off"));
  return;
}

#else /* HAVE_DPMS_EXTENSION -- whole file */

# include <X11/Xproto.h>
# include <X11/extensions/dpms.h>


static Bool error_handler_hit_p = False;

static int
ignore_all_errors_ehandler (Display *dpy, XErrorEvent *error)
{
  error_handler_hit_p = True;
  return 0;
}


void
sync_server_dpms_settings (saver_info *si)
{
# ifdef HAVE_WAYLAND	/* The XDPMS extension is definitely not available */
  if (si->wayland_dpy)
    {
      disable_builtin_saver (si->dpy);
      return;
    }
# endif /* HAVE_WAYLAND */

  /* If the monitor is currently powered off, defer any changes until we are
     next called while it is powered on.  Making changes to the DPMS settings
     can have the side-effect of powering the monitor back on.
    */
  if (! monitor_powered_on_p (si))
    {
      if (verbose_p > 1)
        fprintf (stderr, "%s: DPMS: monitor off, skipping sync\n", blurb());
      return;
    }

  sync_server_dpms_settings_1 (si->dpy, &si->prefs);
}

void
sync_server_dpms_settings_1 (Display *dpy, struct saver_preferences *p)
{
  int event = 0, error = 0;
  BOOL o_enabled = False;
  CARD16 o_power = 0;
  CARD16 o_standby = 0, o_suspend = 0, o_off = 0;
  Bool bogus_p = False;
  Bool changed_p = False;
  static int change_count = 0;

  Bool enabled_p       = (p->dpms_enabled_p && p->mode != DONT_BLANK);
  Bool dpms_quickoff_p = p->dpms_quickoff_p;
  int standby_secs     = p->dpms_standby / 1000;
  int suspend_secs     = p->dpms_suspend / 1000;
  int off_secs         = p->dpms_off / 1000;
  Bool verbose_p       = p->verbose_p;
  static Bool warned_p = False;

  changed_p = disable_builtin_saver (dpy);

  if (dpms_quickoff_p && !off_secs)
    {
      /* To do this, we might need to temporarily re-enable DPMS first. */
      off_secs = 0xFFFF;
    }

  if (standby_secs == 0 && suspend_secs == 0 && off_secs == 0)
    /* all zero implies "DPMS disabled" */
    enabled_p = False;

  else if ((standby_secs != 0 && standby_secs < 10) ||
           (suspend_secs != 0 && suspend_secs < 10) ||
           (off_secs     != 0 && off_secs     < 10))
    /* any negative, or any positive-and-less-than-10-seconds, is crazy. */
    bogus_p = True;

  if (bogus_p) enabled_p = False;

  /* X protocol sends these values in a CARD16, so truncate them to 16 bits.
     This means that the maximum timeout is 18:12:15.
   */
  if (standby_secs > 0xFFFF) standby_secs = 0xFFFF;
  if (suspend_secs > 0xFFFF) suspend_secs = 0xFFFF;
  if (off_secs     > 0xFFFF) off_secs     = 0xFFFF;

  if (! DPMSQueryExtension (dpy, &event, &error))
    {
      if (verbose_p > 1 || (verbose_p && !warned_p))
        fprintf (stderr, "%s: XDPMS extension not supported\n", blurb());
      warned_p = True;
      return;
    }

  if (! DPMSCapable (dpy))
    {
      if (verbose_p > 1 || (verbose_p && !warned_p))
        fprintf (stderr, "%s: DPMS not supported\n", blurb());
      warned_p = True;
      return;
    }

  if (! DPMSInfo (dpy, &o_power, &o_enabled))
    {
      if (verbose_p > 1 || (verbose_p && !warned_p))
        fprintf (stderr, "%s: unable to get DPMS state\n", blurb());
      warned_p = True;
      return;
    }

  if (o_enabled != enabled_p)
    {
      if (! (enabled_p ? DPMSEnable (dpy) : DPMSDisable (dpy)))
        {
          if (verbose_p && !warned_p)
            fprintf (stderr, "%s: unable to set DPMS state\n", blurb());
          warned_p = True;
          return;
        }
      else
        {
          if (verbose_p)
            fprintf (stderr, "%s: turned DPMS %s\n", blurb(),
                     enabled_p ? "on" : "off");
          changed_p = True;
        }
    }

  if (bogus_p)
    {
      if (verbose_p)
        fprintf (stderr, "%s: not setting bogus DPMS timeouts: %d %d %d\n",
                 blurb(), standby_secs, suspend_secs, off_secs);
      return;
    }

  if (!DPMSGetTimeouts (dpy, &o_standby, &o_suspend, &o_off))
    {
      if (verbose_p)
        fprintf (stderr, "%s: unable to get DPMS timeouts\n", blurb());
      return;
    }

  if (o_standby != standby_secs ||
      o_suspend != suspend_secs ||
      o_off != off_secs)
    {
      if (!DPMSSetTimeouts (dpy, standby_secs, suspend_secs, off_secs))
        {
          if (verbose_p)
            fprintf (stderr, "%s: unable to set DPMS timeouts\n", blurb());
          return;
        }
      else
        {
          if (verbose_p)
            fprintf (stderr, "%s: set DPMS timeouts: %d %d %d\n", blurb(),
                     standby_secs, suspend_secs, off_secs);
          changed_p = True;
        }
    }
  else if (verbose_p > 1)
    fprintf (stderr, "%s: DPMS timeouts already %d %d %d\n", blurb(),
             o_standby, o_suspend, o_off);

  if (changed_p)
    change_count++;

  if (change_count > 3)
    {
      fprintf (stderr, "%s: WARNING: some other program keeps changing"
               " the DPMS settings. That's bad.\n", blurb());
      change_count = 0;
    }
}

Bool
monitor_powered_on_p (saver_info *si)
{
  Bool result;
  int event_number, error_number;
  BOOL onoff = False;
  CARD16 state;

# ifdef HAVE_WAYLAND

  if (si->wayland_dpms)
    return wayland_monitor_powered_on_p (si->wayland_dpms);
  else if (si->wayland_dpy)
    return True;  /* Wayland server does not support DPMS */

# endif /* HAVE_WAYLAND */

  if (!DPMSQueryExtension(si->dpy, &event_number, &error_number))
    {
      /* Server doesn't know -- assume the monitor is on. */
      if (verbose_p > 1)
        fprintf (stderr, "%s: DPMSQueryExtension failed, assuming monitor on\n",
                 blurb());
      result = True;
    }

  else if (!DPMSCapable(si->dpy))
    {
      /* Server says the monitor doesn't do power management -- so it's on. */
      if (verbose_p > 1)
        fprintf (stderr, "%s: DPMSCapable false; assuming monitor on\n",
                 blurb());
      result = True;
    }

  else
    {
      DPMSInfo(si->dpy, &state, &onoff);
      if (!onoff)
        {
          /* Server says DPMS is disabled -- so the monitor is on. */
          if (verbose_p > 1)
            fprintf (stderr, "%s: DPMSInfo disabled; assuming monitor on\n",
                     blurb());
          result = True;
        }
      else
	switch (state) {
	case DPMSModeOn:      result = True;  break;  /* really on */
	case DPMSModeStandby: result = False; break;  /* kinda off */
	case DPMSModeSuspend: result = False; break;  /* pretty off */
	case DPMSModeOff:     result = False; break;  /* really off */
	default:	      result = True;  break;  /* protocol error? */
	}
      if (verbose_p > 1)
        fprintf (stderr, "%s: DPMSInfo = %s %s\n", blurb(),
                 (state == DPMSModeOn      ? "DPMSModeOn" :
                  state == DPMSModeStandby ? "DPMSModeStandby" :
                  state == DPMSModeSuspend ? "DPMSModeSuspend" :
                  state == DPMSModeOff     ? "DPMSModeOff" : "???"),
                 (result ? "True" : "False"));
    }

  return result;
}

void
monitor_power_on (saver_info *si, Bool on_p)
{
  Bool verbose_p = si->prefs.verbose_p;
  if ((!!on_p) != monitor_powered_on_p (si))
    {
      XErrorHandler old_handler;
      int event_number, error_number;
      static Bool warned_p = False;

# ifdef HAVE_WAYLAND
      if (si->wayland_dpms)
        {
          wayland_monitor_power_on (si->wayland_dpms, on_p);
          return;
        }
      else if (si->wayland_dpy)
        {
          if (verbose_p > 1 || (verbose_p && !warned_p))
            fprintf (stderr,
                     "%s: wayland: unable to power %s monitor\n",
                     blurb(), (on_p ? "on" : "off"));
          warned_p = True;
          return;
        }
# endif /* HAVE_WAYLAND */

      if (!DPMSQueryExtension(si->dpy, &event_number, &error_number) ||
          !DPMSCapable(si->dpy))
        {
          if (verbose_p > 1 || (verbose_p && !warned_p))
            fprintf (stderr,
                     "%s: unable to power %s monitor: no DPMS extension\n",
                     blurb(), (on_p ? "on" : "off"));
          warned_p = True;
          return;
        }

      /* The manual for DPMSForceLevel() says that it throws BadMatch if
         "DPMS is disabled on the specified display."

         The manual for DPMSCapable() says that it "returns True if the X
         server is capable of DPMS."

         Apparently they consider "capable of DPMS" and "DPMS is enabled"
         to be different things, and so even if DPMSCapable() returns
         True, DPMSForceLevel() *might* throw an X Error.  Isn't that
         just fucking special.
       */
      XSync (si->dpy, False);
      error_handler_hit_p = False;
      old_handler = XSetErrorHandler (ignore_all_errors_ehandler);
      XSync (si->dpy, False);
      DPMSForceLevel(si->dpy, (on_p ? DPMSModeOn : DPMSModeOff));
      XSync (si->dpy, False);
      XSetErrorHandler (old_handler);
      /* Ignore error_handler_hit_p, just probe monitor instead */

      if (verbose_p > 1 && error_handler_hit_p)
        fprintf (stderr, "%s: DPMSForceLevel got an X11 error\n", blurb());

      if ((!!on_p) != monitor_powered_on_p (si))  /* double-check */
	fprintf (stderr,
       "%s: DPMSForceLevel(dpy, %s) did not change monitor power state\n",
		 blurb(),
                 (on_p ? "DPMSModeOn" : "DPMSModeOff"));
    }
  else if (verbose_p > 1)
    fprintf (stderr, "%s: monitor is already %s\n", blurb(),
             on_p ? "on" : "off");
}


/* Force the server DPMS state to be what the wall clock says it should be.
 */
void
brute_force_dpms (saver_info *si, time_t activity_time)
{
  Display *dpy = si->dpy;
  saver_preferences *p = &si->prefs;
  XErrorHandler old_handler;
  int event_number, error_number;
  BOOL onoff = False;
  CARD16 state;
  CARD16 target;
  time_t age = time ((time_t *) 0) - activity_time;
  Bool enabled_p = (p->dpms_enabled_p && p->mode != DONT_BLANK);

  if (activity_time == 0)		/* Auth dialog is visible */
    target = DPMSModeOn;
  else if (p->mode == BLANK_ONLY && p->dpms_quickoff_p)
    target = DPMSModeOff;
  else if (enabled_p && p->dpms_off     && age >= (p->dpms_off     / 1000))
    target = DPMSModeOff;
  else if (enabled_p && p->dpms_suspend && age >= (p->dpms_suspend / 1000))
    target = DPMSModeSuspend;
  else if (enabled_p && p->dpms_standby && age >= (p->dpms_standby / 1000))
    target = DPMSModeStandby;
  else
    /* We have no opinion about the desired DPMS state, so if it is off,
       leave it off. It may have been powered down manually with xset. */
    return;

# ifdef HAVE_WAYLAND
  {
    Bool have = monitor_powered_on_p (si);
    Bool want = (target == DPMSModeOn);

    if (have == want)
      ;
    else if (si->wayland_dpms)
      {
        wayland_monitor_power_on (si->wayland_dpms, want);
        if (p->verbose_p)
          fprintf (stderr, "%s: powered %s monitor\n", blurb(),
                   (want ? "on" : "off"));
        return;
      }
    else if (si->wayland_dpy)
      {
        if (verbose_p)
          fprintf (stderr, "%s: wayland: unable to power %s monitor\n",
                   blurb(), (want ? "on" : "off"));
        return;
      }
  }
# endif /* HAVE_WAYLAND */

  if (!DPMSQueryExtension (dpy, &event_number, &error_number) ||
      !DPMSCapable (dpy))
    return;

  DPMSInfo (dpy, &state, &onoff);
  if (!onoff)
    return;

  if (state == target)
    return;

  XSync (dpy, False);
  error_handler_hit_p = False;
  old_handler = XSetErrorHandler (ignore_all_errors_ehandler);
  XSync (dpy, False);

  DPMSForceLevel (dpy, target);

  XSync (dpy, False);
  XSetErrorHandler (old_handler);

  if (p->verbose_p > 1 && error_handler_hit_p)
    {
      fprintf (stderr, "%s: DPMSForceLevel got an X11 error\n", blurb());
      return;
    }

  DPMSInfo (dpy, &state, &onoff);
  {
    const char *s = (target == DPMSModeOff     ? "Off"     :
                     target == DPMSModeSuspend ? "Suspend" :
                     target == DPMSModeStandby ? "Standby" :
                     target == DPMSModeOn      ? "On" : "???");
    if (state != target)
      fprintf (stderr,
          "%s: DPMSForceLevel(dpy, %s) did not change monitor power state\n",
               blurb(), s);
    else if (p->verbose_p)
      fprintf (stderr, "%s: set monitor power to %s\n", blurb(), s);
  }
}

#endif /* HAVE_DPMS_EXTENSION -- whole file */
