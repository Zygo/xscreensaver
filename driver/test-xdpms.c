/* test-xdpms.c --- playing with the XDPMS extension.
 * xscreensaver, Copyright (c) 1998 Jamie Zawinski <jwz@jwz.org>
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
#include <X11/extensions/dpmsstr.h>

extern Bool DPMSQueryExtension (Display *dpy, int *event_ret, int *error_ret);
extern Bool DPMSCapable (Display *dpy);
extern Status DPMSForceLevel (Display *dpy, CARD16 level);
extern Status DPMSInfo (Display *dpy, CARD16 *power_level, BOOL *state);

extern Status DPMSGetVersion (Display *dpy, int *major_ret, int *minor_ret);
extern Status DPMSSetTimeouts (Display *dpy,
			       CARD16 standby, CARD16 suspend, CARD16 off);
extern Bool DPMSGetTimeouts (Display *dpy,
			     CARD16 *standby, CARD16 *suspend, CARD16 *off);
extern Status DPMSEnable (Display *dpy);
extern Status DPMSDisable (Display *dpy);


char *progname = 0;
char *progclass = "XScreenSaver";

static const char *
blurb (void)
{
  static char buf[255];
  time_t now = time ((time_t *) 0);
  char *ct = (char *) ctime (&now);
  int n = strlen(progname);
  if (n > 100) n = 99;
  strncpy(buf, progname, n);
  buf[n++] = ':';
  buf[n++] = ' ';
  strncpy(buf+n, ct+11, 8);
  strcpy(buf+n+9, ": ");
  return buf;
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
  XtGetApplicationNameAndClass (dpy, &progname, &progclass);

  if (!DPMSQueryExtension(dpy, &event_number, &error_number))
    {
      fprintf(stderr, "%s: DPMSQueryExtension(dpy, ...) ==> False\n",
	      blurb());
      fprintf(stderr, "%s: server does not support the XDPMS extension.\n",
	      blurb());
      exit(1);
    }
  else
    fprintf(stderr, "%s: DPMSQueryExtension(dpy, ...) ==> %d, %d\n", blurb(),
	    event_number, error_number);

  if (!DPMSCapable(dpy))
    {
      fprintf(stderr, "%s: DPMSCapable(dpy) ==> False\n", blurb());
      fprintf(stderr, "%s: server says hardware doesn't support DPMS.\n",
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
	  Status st;
	  fprintf(stderr, "%s: monitor is off; turning it on.\n", blurb());
	  st = DPMSForceLevel (dpy, DPMSModeOn);
	  fprintf (stderr, "%s: DPMSForceLevel (dpy, DPMSModeOn) ==> %s\n",
		   blurb(), (st ? "Ok" : "Error"));
	}

      sleep (delay);
    }
}
