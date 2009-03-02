/* dpms.c --- syncing the X Display Power Management values
 * xscreensaver, Copyright (c) 2001 Jamie Zawinski <jwz@jwz.org>
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

#ifdef HAVE_DPMS_EXTENSION

# include <X11/Xproto.h>
# include <X11/extensions/dpms.h>
# include <X11/extensions/dpmsstr.h>

  /* Why this crap is not in a header file somewhere, I have no idea.  Losers!
   */
  extern Bool DPMSQueryExtension (Display *dpy, int *event_ret, int *err_ret);
  extern Bool DPMSCapable (Display *dpy);
  extern Status DPMSInfo (Display *dpy, CARD16 *power_level, BOOL *state);
  extern Status DPMSSetTimeouts (Display *dpy,
                                 CARD16 standby, CARD16 suspend, CARD16 off);
  extern Bool DPMSGetTimeouts (Display *dpy,
                               CARD16 *standby, CARD16 *suspend, CARD16 *off);
  extern Status DPMSEnable (Display *dpy);
  extern Status DPMSDisable (Display *dpy);

#endif /* HAVE_DPMS_EXTENSION */


/* This file doesn't need the Xt headers, so stub these types out... */
#undef XtPointer
#define XtAppContext void*
#define XrmDatabase  void*
#define XtIntervalId void*
#define XtPointer    void*
#define Widget       void*

#include "xscreensaver.h"

void
sync_server_dpms_settings (Display *dpy, Bool enabled_p,
                           int standby_secs, int suspend_secs, int off_secs,
                           Bool verbose_p)
{
# ifdef HAVE_DPMS_EXTENSION

  int event = 0, error = 0;
  BOOL o_enabled = False;
  CARD16 o_power = 0;
  CARD16 o_standby = 0, o_suspend = 0, o_off = 0;

  Bool bogus_p = (standby_secs == 0 || suspend_secs == 0 || off_secs == 0);

  if (bogus_p) enabled_p = False;

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

# else  /* !HAVE_DPMS_EXTENSION */

  if (verbose_p)
    fprintf (stderr, "%s: DPMS support not compiled in.\n", blurb());

# endif /* HAVE_DPMS_EXTENSION */
}
