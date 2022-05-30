/* test-uid.c --- playing with grabs.
 * xscreensaver, Copyright © 1999-2022 Jamie Zawinski <jwz@jwz.org>
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
#include <X11/cursorfont.h>

#include "blurb.h"
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
  int kstatus = AlreadyGrabbed, mstatus = AlreadyGrabbed;
  Cursor cursor1, cursor2;
  int delay1 = 15;
  int delay2 = 60 * 15 - delay1;
  Widget toplevel_shell = XtAppInitialize (&app, progclass, 0, 0,
					   &argc, argv, 0, 0, 0);
  Display *dpy = XtDisplay (toplevel_shell);
  Window w = RootWindow (dpy, DefaultScreen(dpy));
  int i;

  Bool grab_kbd_p   = True;
  Bool grab_mouse_p = True;
  Bool mouse_sync_p = True;
  Bool kbd_sync_p   = True;

  progname = argv[0];

  cursor1 = XCreateFontCursor (dpy, XC_hand1);
  cursor2 = XCreateFontCursor (dpy, XC_gumby);

  for (i = 1; i < argc; i++)
    {
      const char *oa = argv[i];
      if (argv[i][0] == '-' && argv[i][1] == '-')
        argv[i]++;
      if (!strcmp (argv[i], "-kbd") || !strcmp (argv[i], "-keyboard"))
        grab_mouse_p = False;
      else if (!strcmp (argv[i], "-mouse") || !strcmp (argv[i], "-pointer"))
        grab_kbd_p = False;
      else if (!strcmp (argv[i], "-kbd-sync") ||
               !strcmp (argv[i], "-keyboard-sync"))
        kbd_sync_p = True;
      else if (!strcmp (argv[i], "-kbd-async") ||
               !strcmp (argv[i], "-keyboard-async"))
        kbd_sync_p = False;
      else if (!strcmp (argv[i], "-mouse-sync") ||
               !strcmp (argv[i], "-pointer-sync"))
        mouse_sync_p = True;
      else if (!strcmp (argv[i], "-mouse-async") ||
               !strcmp (argv[i], "-pointer-async"))
        mouse_sync_p = False;
      else if (!strcmp (argv[i], "-sync"))
        kbd_sync_p = mouse_sync_p = True;
      else if (!strcmp (argv[i], "-async"))
        kbd_sync_p = mouse_sync_p = False;
      else
        {
          fprintf (stderr, "%s: unknown option: %s\n", blurb(), oa);
          fprintf (stderr, "usage: %s "
                            "[--sync | --async]"
                   "\n\t\t   [--kbd]   [--kbd-sync   | --kbd-async]"
                   "\n\t\t   [--mouse] [--mouse-sync | --mouse-async]\n",
                   progname);
          exit (1);
        }
    }

  if (grab_kbd_p)
    {
      kstatus = XGrabKeyboard (dpy, w, True,
                               (mouse_sync_p ? GrabModeSync : GrabModeAsync),
                               (kbd_sync_p   ? GrabModeSync : GrabModeAsync),
                               CurrentTime);
      fprintf (stderr, "%s: grabbing keyboard on 0x%lx (%s, %s)... %s\n",
               progname, (unsigned long) w,
               (mouse_sync_p ? "sync" : "async"),
               (kbd_sync_p   ? "sync" : "async"),
               (kstatus == GrabSuccess ? "GrabSuccess" :
                kstatus == AlreadyGrabbed ? "AlreadyGrabbed" :
                kstatus == GrabInvalidTime ? "GrabInvalidTime" :
                kstatus == GrabNotViewable ? "GrabNotViewable" :
                kstatus == GrabFrozen ? "GrabFrozen" :
                "???"));
    }

  if (grab_mouse_p)
    {
      mstatus = XGrabPointer (dpy, w, True, ALL_POINTER_EVENTS,
                               (mouse_sync_p ? GrabModeSync : GrabModeAsync),
                               (kbd_sync_p   ? GrabModeSync : GrabModeAsync),
                              None, cursor1, CurrentTime);
      fprintf (stderr, "%s: grabbing mouse on 0x%lx (%s, %s)... %s\n",
               progname, (unsigned long) w,
               (mouse_sync_p ? "sync" : "async"),
               (kbd_sync_p   ? "sync" : "async"),
               (mstatus == GrabSuccess ? "GrabSuccess" :
                mstatus == AlreadyGrabbed ? "AlreadyGrabbed" :
                mstatus == GrabInvalidTime ? "GrabInvalidTime" :
                mstatus == GrabNotViewable ? "GrabNotViewable" :
                mstatus == GrabFrozen ? "GrabFrozen" :
                "???"));
    }

  XSync(dpy, False);

  if (kstatus == GrabSuccess || mstatus == GrabSuccess)
    {
      fprintf (stderr, "%s: sleeping for %d:%02d:%02d...\n",
               progname,
               delay1 / (60 * 60),
               (delay1 % (60 * 60)) / 60,
               delay1 % 60);
      sleep (delay1);
      fprintf (stderr, "%s: changing mouse cursor...\n", progname);

      mstatus = XGrabPointer (dpy, w, True, ALL_POINTER_EVENTS,
                              (mouse_sync_p ? GrabModeSync : GrabModeAsync),
                               (kbd_sync_p   ? GrabModeSync : GrabModeAsync),
                              None, cursor2, CurrentTime);
      XSync (dpy, False);
      if (mstatus != GrabSuccess)
        fprintf (stderr, "%s: failed: %s\n", progname,
                 (mstatus == GrabSuccess ? "GrabSuccess" :
                  mstatus == AlreadyGrabbed ? "AlreadyGrabbed" :
                  mstatus == GrabInvalidTime ? "GrabInvalidTime" :
                  mstatus == GrabNotViewable ? "GrabNotViewable" :
                  mstatus == GrabFrozen ? "GrabFrozen" :
                  "???"));

      fprintf (stderr, "%s: sleeping for %d:%02d:%02d...\n",
               progname,
               delay2 / (60 * 60),
               (delay2 % (60 * 60)) / 60,
               delay2 % 60);
      fflush (stderr);
      sleep (delay2);
      XSync (dpy, False);
    }

  exit (0);
}
