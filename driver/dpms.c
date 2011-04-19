/* dpms.c --- syncing the X Display Power Management values
 * xscreensaver, Copyright (c) 2001-2009 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* Display Power Management System (DPMS.)

   On XFree86 systems, "man xset" reports:

       -dpms    The -dpms option disables DPMS (Energy Star) features.
       +dpms    The +dpms option enables DPMS (Energy Star) features.

       dpms flags...
                The dpms option allows the DPMS (Energy Star)
                parameters to be set.  The option can take up to three
                numerical values, or the `force' flag followed by a
                DPMS state.  The `force' flags forces the server to
                immediately switch to the DPMS state specified.  The
                DPMS state can be one of `standby', `suspend', or
                `off'.  When numerical values are given, they set the
                inactivity period before the three modes are activated.
                The first value given is for the `standby' mode, the
                second is for the `suspend' mode, and the third is for
                the `off' mode.  Setting these values implicitly
                enables the DPMS features.  A value of zero disables a
                particular mode.

   However, note that the implementation is more than a little bogus,
   in that there is code in /usr/X11R6/lib/libXdpms.a to implement all
   the usual server-extension-querying utilities -- but there are no
   prototypes in any header file!  Thus, the prototypes here.  (The
   stuff in X11/extensions/dpms.h and X11/extensions/dpmsstr.h define
   the raw X protcol, they don't define the API to libXdpms.a.)

   Some documentation:
   Library:  ftp://ftp.x.org/pub/R6.4/xc/doc/specs/Xext/DPMSLib.ms
   Protocol: ftp://ftp.x.org/pub/R6.4/xc/doc/specs/Xext/DPMS.ms
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <X11/Xlib.h>

#ifdef HAVE_DPMS_EXTENSION   /* almost the whole file */

# include <X11/Xproto.h>
# include <X11/extensions/dpms.h>
/*# include <X11/extensions/dpmsstr.h>*/

  /* Why this crap is not in a header file somewhere, I have no idea.  Losers!
   */
  extern Bool   DPMSQueryExtension (Display *, int *event_ret, int *err_ret);
  extern Status DPMSGetVersion (Display *, int *major_ret, int *minor_ret);
  extern Bool   DPMSCapable (Display *);
  extern Status DPMSInfo (Display *, CARD16 *power_level, BOOL *state);
  extern Status DPMSEnable (Display *dpy);
  extern Status DPMSDisable (Display *dpy);
  extern Status DPMSForceLevel (Display *, CARD16 level);
  extern Status DPMSSetTimeouts (Display *, CARD16 standby, CARD16 suspend,
                                 CARD16 off);
  extern Bool   DPMSGetTimeouts (Display *, CARD16 *standby,
                                 CARD16 *suspend, CARD16 *off);

#endif /* HAVE_DPMS_EXTENSION */


/* This file doesn't need the Xt headers, so stub these types out... */
#undef XtPointer
#define XtAppContext void*
#define XrmDatabase  void*
#define XtIntervalId void*
#define XtPointer    void*
#define Widget       void*

#include "xscreensaver.h"

#ifdef HAVE_DPMS_EXTENSION

void
sync_server_dpms_settings (Display *dpy, Bool enabled_p,
                           int standby_secs, int suspend_secs, int off_secs,
                           Bool verbose_p)
{
  int event = 0, error = 0;
  BOOL o_enabled = False;
  CARD16 o_power = 0;
  CARD16 o_standby = 0, o_suspend = 0, o_off = 0;
  Bool bogus_p = False;

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
      if (verbose_p)
        fprintf (stderr, "%s: XDPMS extension not supported.\n", blurb());
      return;
    }

  if (! DPMSCapable (dpy))
    {
      if (verbose_p)
        fprintf (stderr, "%s: DPMS not supported.\n", blurb());
      return;
    }

  if (! DPMSInfo (dpy, &o_power, &o_enabled))
    {
      if (verbose_p)
        fprintf (stderr, "%s: unable to get DPMS state.\n", blurb());
      return;
    }

  if (o_enabled != enabled_p)
    {
      if (! (enabled_p ? DPMSEnable (dpy) : DPMSDisable (dpy)))
        {
          if (verbose_p)
            fprintf (stderr, "%s: unable to set DPMS state.\n", blurb());
          return;
        }
      else if (verbose_p)
        fprintf (stderr, "%s: turned DPMS %s.\n", blurb(),
                 enabled_p ? "on" : "off");
    }

  if (bogus_p)
    {
      if (verbose_p)
        fprintf (stderr, "%s: not setting bogus DPMS timeouts: %d %d %d.\n",
                 blurb(), standby_secs, suspend_secs, off_secs);
      return;
    }

  if (!DPMSGetTimeouts (dpy, &o_standby, &o_suspend, &o_off))
    {
      if (verbose_p)
        fprintf (stderr, "%s: unable to get DPMS timeouts.\n", blurb());
      return;
    }

  if (o_standby != standby_secs ||
      o_suspend != suspend_secs ||
      o_off != off_secs)
    {
      if (!DPMSSetTimeouts (dpy, standby_secs, suspend_secs, off_secs))
        {
          if (verbose_p)
            fprintf (stderr, "%s: unable to set DPMS timeouts.\n", blurb());
          return;
        }
      else if (verbose_p)
        fprintf (stderr, "%s: set DPMS timeouts: %d %d %d.\n", blurb(),
                 standby_secs, suspend_secs, off_secs);
    }
}

Bool
monitor_powered_on_p (saver_info *si)
{
  Bool result;
  int event_number, error_number;
  BOOL onoff = False;
  CARD16 state;

  if (!DPMSQueryExtension(si->dpy, &event_number, &error_number))
    /* Server doesn't know -- assume the monitor is on. */
    result = True;

  else if (!DPMSCapable(si->dpy))
    /* Server says the monitor doesn't do power management -- so it's on. */
    result = True;

  else
    {
      DPMSInfo(si->dpy, &state, &onoff);
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
monitor_power_on (saver_info *si)
{
  if (!monitor_powered_on_p (si))
    {
      DPMSForceLevel(si->dpy, DPMSModeOn);
      XSync(si->dpy, False);
      if (!monitor_powered_on_p (si))
	fprintf (stderr,
       "%s: DPMSForceLevel(dpy, DPMSModeOn) did not power the monitor on?\n",
		 blurb());
    }
}

#else  /* !HAVE_DPMS_EXTENSION */

void
sync_server_dpms_settings (Display *dpy, Bool enabled_p,
                           int standby_secs, int suspend_secs, int off_secs,
                           Bool verbose_p)
{
  if (verbose_p)
    fprintf (stderr, "%s: DPMS support not compiled in.\n", blurb());
}

Bool
monitor_powered_on_p (saver_info *si) 
{
  return True; 
}

void
monitor_power_on (saver_info *si)
{
  return; 
}

#endif /* !HAVE_DPMS_EXTENSION */
