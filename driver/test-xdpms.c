/* test-xdpms.c --- playing with the XDPMS extension.
 * xscreensaver, Copyright © 1998-2021 Jamie Zawinski <jwz@jwz.org>
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
#include <time.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Intrinsic.h>

#include <X11/Xproto.h>
#include <X11/extensions/dpms.h>
/*#include <X11/extensions/dpmsstr.h>*/

#include "blurb.h"
char *progclass = "XScreenSaver";

static Bool error_handler_hit_p = False;

static int
ignore_all_errors_ehandler (Display *dpy, XErrorEvent *error)
{
  error_handler_hit_p = True;
  return 0;
}


int
main (int argc, char **argv)
{
  int delay = 10;

  int event_number, error_number;
  int major, minor;
  CARD16 standby, suspend, off;
  CARD16 state;
  BOOL onoff;

  XtAppContext app;
  Widget toplevel_shell = XtAppInitialize (&app, progclass, 0, 0,
					   &argc, argv, 0, 0, 0);
  Display *dpy = XtDisplay (toplevel_shell);

  if (!DPMSQueryExtension(dpy, &event_number, &error_number))
    {
      fprintf(stderr, "%s: DPMSQueryExtension(dpy, ...) ==> False\n",
	      blurb());
      fprintf(stderr, "%s: server does not support the XDPMS extension\n",
	      blurb());
      exit(1);
    }
  else
    fprintf(stderr, "%s: DPMSQueryExtension(dpy, ...) ==> %d, %d\n", blurb(),
	    event_number, error_number);

  if (!DPMSCapable(dpy))
    {
      fprintf(stderr, "%s: DPMSCapable(dpy) ==> False\n", blurb());
      fprintf(stderr, "%s: server says hardware doesn't support DPMS\n",
	      blurb());
      exit(1);
    }
  else
    fprintf(stderr, "%s: DPMSCapable(dpy) ==> True\n", blurb());

  if (!DPMSGetVersion(dpy, &major, &minor))
    {
      fprintf(stderr, "%s: DPMSGetVersion(dpy, ...) ==> False\n", blurb());
      fprintf(stderr, "%s: server didn't report XDPMS version numbers?\n",
	      blurb());
    }
  else
    fprintf(stderr, "%s: DPMSGetVersion(dpy, ...) ==> %d, %d\n", blurb(),
	    major, minor);

  if (!DPMSGetTimeouts(dpy, &standby, &suspend, &off))
    {
      fprintf(stderr, "%s: DPMSGetTimeouts(dpy, ...) ==> False\n", blurb());
      fprintf(stderr, "%s: server didn't report DPMS timeouts?\n", blurb());
    }
  else
    fprintf(stderr,
	    "%s: DPMSGetTimeouts(dpy, ...)\n"
	    "\t ==> standby = %d, suspend = %d, off = %d\n",
	    blurb(), standby, suspend, off);

  while (1)
    {
      if (!DPMSInfo(dpy, &state, &onoff))
	{
	  fprintf(stderr, "%s: DPMSInfo(dpy, ...) ==> False\n", blurb());
	  fprintf(stderr, "%s: couldn't read DPMS state?\n", blurb());
	  onoff = 0;
	  state = -1;
	}
      else
	{
	  fprintf(stderr, "%s: DPMSInfo(dpy, ...) ==> %s, %s\n", blurb(),
		  (state == DPMSModeOn ? "DPMSModeOn" :
		   state == DPMSModeStandby ? "DPMSModeStandby" :
		   state == DPMSModeSuspend ? "DPMSModeSuspend" :
		   state == DPMSModeOff ? "DPMSModeOff" : "???"),
		  (onoff == 1 ? "On" : onoff == 0 ? "Off" : "???"));
	}

      if (state == DPMSModeStandby ||
	  state == DPMSModeSuspend ||
	  state == DPMSModeOff)
	{
	  int st;
	  fprintf(stderr, "%s: monitor is off; turning it on\n", blurb());

          XSync (dpy, False);
          error_handler_hit_p = False;
          XSetErrorHandler (ignore_all_errors_ehandler);
          XSync (dpy, False);
	  st = DPMSForceLevel (dpy, DPMSModeOn);
          XSync (dpy, False);
          if (error_handler_hit_p) st = -666;

	  fprintf (stderr, "%s: DPMSForceLevel (dpy, DPMSModeOn) ==> %s\n",
		   blurb(), (st == -666 ? "X Error" : st ? "Ok" : "Error"));
	}

      sleep (delay);
    }
}
