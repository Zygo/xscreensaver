/* test-uid.c --- playing with grabs.
 * xscreensaver, Copyright (c) 1999 Jamie Zawinski <jwz@jwz.org>
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
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>

char *progname = 0;
char *progclass = "XScreenSaver";

#define ALL_POINTER_EVENTS \
	(ButtonPressMask | ButtonReleaseMask | EnterWindowMask | \
	 LeaveWindowMask | PointerMotionMask | PointerMotionHintMask | \
	 Button1MotionMask | Button2MotionMask | Button3MotionMask | \
	 Button4MotionMask | Button5MotionMask | ButtonMotionMask)

int
main (int argc, char **argv)
{
  XtAppContext app;
  int kstatus, mstatus;
  Cursor cursor = 0;
  int delay = 60 * 15;
  Widget toplevel_shell = XtAppInitialize (&app, progclass, 0, 0,
					   &argc, argv, 0, 0, 0);
  Display *dpy = XtDisplay (toplevel_shell);
  Window w = RootWindow (dpy, DefaultScreen(dpy));
  XtGetApplicationNameAndClass (dpy, &progname, &progclass);

  kstatus = XGrabKeyboard (dpy, w, True,
                           GrabModeSync, GrabModeAsync,
                           CurrentTime);
  fprintf (stderr, "%s: grabbing keyboard on 0x%x... %s.\n",
           progname, (unsigned long) w,
           (kstatus == GrabSuccess ? "GrabSuccess" :
            kstatus == AlreadyGrabbed ? "AlreadyGrabbed" :
            kstatus == GrabInvalidTime ? "GrabInvalidTime" :
            kstatus == GrabNotViewable ? "GrabNotViewable" :
            kstatus == GrabFrozen ? "GrabFrozen" :
            "???"));

  mstatus = XGrabPointer (dpy, w, True, ALL_POINTER_EVENTS,
                          GrabModeAsync, GrabModeAsync, None,
                          cursor, CurrentTime);
  fprintf (stderr, "%s: grabbing mouse on 0x%x... %s.\n",
           progname, (unsigned long) w,
           (mstatus == GrabSuccess ? "GrabSuccess" :
            mstatus == AlreadyGrabbed ? "AlreadyGrabbed" :
            mstatus == GrabInvalidTime ? "GrabInvalidTime" :
            mstatus == GrabNotViewable ? "GrabNotViewable" :
            mstatus == GrabFrozen ? "GrabFrozen" :
            "???"));

  XSync(dpy, False);

  if (kstatus == GrabSuccess || mstatus == GrabSuccess)
    {
      fprintf (stderr, "%s: sleeping for %d:%02d:%02d...\n",
               progname,
               delay / (60 * 60),
               (delay % (60 * 60)) / 60,
               delay % 60);
      fflush(stderr);
      sleep (delay);
      XSync(dpy, False);
    }

  exit (0);
}
