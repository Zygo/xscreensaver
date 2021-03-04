/* dpms.c --- syncing the X Display Power Management System values
 * xscreensaver, Copyright © 2001-2021 Jamie Zawinski <jwz@jwz.org>
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
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>

#include "xscreensaver.h"

#ifndef HAVE_DPMS_EXTENSION   /* almost the whole file */

void
sync_server_dpms_settings (Display *dpy, struct saver_preferences *p)
{
  if (p->verbose_p)
    fprintf (stderr, "%s: DPMS not supported at compile time\n", blurb());
}

Bool monitor_powered_on_p (Display *dpy) { return True; }
void monitor_power_on (saver_info *si, Bool on_p) { return; }

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
sync_server_dpms_settings (Display *dpy, struct saver_preferences *p)
{
  int event = 0, error = 0;
  BOOL o_enabled = False;
  CARD16 o_power = 0;
  CARD16 o_standby = 0, o_suspend = 0, o_off = 0;
  Bool bogus_p = False;

  Bool enabled_p       = (p->dpms_enabled_p && p->mode != DONT_BLANK);
  Bool dpms_quickoff_p = p->dpms_quickoff_p;
  int standby_secs     = p->dpms_standby / 1000;
  int suspend_secs     = p->dpms_suspend / 1000;
  int off_secs         = p->dpms_off / 1000;
  Bool verbose_p       = p->verbose_p;
  static Bool warned_p = False;

  /* If the monitor is currently powered off, defer any changes until
     we are next called while it is powered on. */
  if (! monitor_powered_on_p (dpy))
    return;

  XSetScreenSaver (dpy, 0, 0, 0, 0); 
  XForceScreenSaver (dpy, ScreenSaverReset);

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
      if (verbose_p && !warned_p)
        fprintf (stderr, "%s: XDPMS extension not supported\n", blurb());
      warned_p = True;
      return;
    }

  if (! DPMSCapable (dpy))
    {
      if (verbose_p && !warned_p)
        fprintf (stderr, "%s: DPMS not supported\n", blurb());
      warned_p = True;
      return;
    }

  if (! DPMSInfo (dpy, &o_power, &o_enabled))
    {
      if (verbose_p && !warned_p)
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
      else if (verbose_p)
        fprintf (stderr, "%s: turned DPMS %s\n", blurb(),
                 enabled_p ? "on" : "off");
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
      else if (verbose_p)
        fprintf (stderr, "%s: set DPMS timeouts: %d %d %d\n", blurb(),
                 standby_secs, suspend_secs, off_secs);
    }
}

Bool
monitor_powered_on_p (Display *dpy)
{
  Bool result;
  int event_number, error_number;
  BOOL onoff = False;
  CARD16 state;

  if (!DPMSQueryExtension(dpy, &event_number, &error_number))
    /* Server doesn't know -- assume the monitor is on. */
    result = True;

  else if (!DPMSCapable(dpy))
    /* Server says the monitor doesn't do power management -- so it's on. */
    result = True;

  else
    {
      DPMSInfo(dpy, &state, &onoff);
      if (!onoff)
	/* Server says DPMS is disabled -- so the monitor is on. */
	result = True;
      else
	switch (state) {
	case DPMSModeOn:      result = True;  break;  /* really on */
	case DPMSModeStandby: result = False; break;  /* kinda off */
	case DPMSModeSuspend: result = False; break;  /* pretty off */
	case DPMSModeOff:     result = False; break;  /* really off */
	default:	      result = True;  break;  /* protocol error? */
	}
    }

  return result;
}

void
monitor_power_on (saver_info *si, Bool on_p)
{
  if ((!!on_p) != monitor_powered_on_p (si->dpy))
    {
      XErrorHandler old_handler;
      int event_number, error_number;
      static Bool warned_p = False;
      if (!DPMSQueryExtension(si->dpy, &event_number, &error_number) ||
          !DPMSCapable(si->dpy))
        {
          if (si->prefs.verbose_p && !warned_p)
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

      if ((!!on_p) != monitor_powered_on_p (si->dpy))  /* double-check */
	fprintf (stderr,
       "%s: DPMSForceLevel(dpy, %s) did not change monitor power state\n",
		 blurb(),
                 (on_p ? "DPMSModeOn" : "DPMSModeOff"));
    }
}

#endif /* HAVE_DPMS_EXTENSION -- whole file */
