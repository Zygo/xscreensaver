/* test-xinput.c --- playing with the XInput2 extension.
 * xscreensaver, Copyright © 2021 Jamie Zawinski <jwz@jwz.org>
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
#include <X11/extensions/XInput2.h>

#include "blurb.h"
#include "xinput.h"

char *progclass = "XScreenSaver";
Bool debug_p = True;

static void
ungrab_timer (XtPointer closure, XtIntervalId *id)
{
  Display *dpy = (Display *) closure;
  fprintf (stderr, "\n%s: ungrabbing\n\n", blurb());
  XUngrabKeyboard (dpy, CurrentTime);
  XUngrabPointer (dpy, CurrentTime);
}


static const char *
grab_string (int status)
{
  switch (status) {
  case GrabSuccess:     return "GrabSuccess";
  case AlreadyGrabbed:  return "AlreadyGrabbed";
  case GrabInvalidTime: return "GrabInvalidTime";
  case GrabNotViewable: return "GrabNotViewable";
  case GrabFrozen:      return "GrabFrozen";
  default:
    {
      static char buf[255];
      sprintf(buf, "unknown status: %d", status);
      return buf;
    }
  }
}


int
main (int argc, char **argv)
{
  XtAppContext app;
  Widget toplevel_shell;
  Display *dpy;
  int xi_opcode;
  Bool grab_kbd_p   = False;
  Bool grab_mouse_p = False;
  Bool mouse_sync_p = True;
  Bool kbd_sync_p   = True;
  int i;

  progname = argv[0];

  for (i = 1; i < argc; i++)
    {
      const char *oa = argv[i];
      if (argv[i][0] == '-' && argv[i][1] == '-')
        argv[i]++;
      if (!strcmp (argv[i], "-grab"))
        grab_kbd_p = grab_mouse_p = True;
      else if (!strcmp (argv[i], "-grab-kbd") ||
               !strcmp (argv[i], "-grab-keyboard"))
        grab_kbd_p = True;
      else if (!strcmp (argv[i], "-grab-mouse") ||
               !strcmp (argv[i], "-grab-pointer"))
        grab_mouse_p = True;
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
      else
        {
          fprintf (stderr, "%s: unknown option: %s\n", blurb(), oa);
          exit (1);
        }
    }

  toplevel_shell = XtAppInitialize (&app, progclass, 0, 0, 
                                    &argc, argv, 0, 0, 0);
  dpy = XtDisplay (toplevel_shell);
  if (!dpy) exit(1);

  if (! init_xinput (dpy, &xi_opcode))
    exit (1);

  if (grab_kbd_p || grab_mouse_p)
    {
      int timeout = 15;
      Window w = RootWindow (dpy, 0);
      int status;
      XColor black = { 0, };
      Pixmap bit = XCreateBitmapFromData (dpy, w, "\000", 1, 1);
      Cursor cursor = XCreatePixmapCursor (dpy, bit, bit, &black, &black, 0, 0);

      if (grab_kbd_p)
        {
          status = XGrabKeyboard (dpy, w, True,
                                  (mouse_sync_p ? GrabModeSync : GrabModeAsync),
                                  (kbd_sync_p   ? GrabModeSync : GrabModeAsync),
                                  CurrentTime);
          if (status == GrabSuccess)
            fprintf (stderr, "%s: grabbed keyboard (%s, %s)\n", blurb(),
                     (mouse_sync_p ? "sync" : "async"),
                     (kbd_sync_p   ? "sync" : "async"));
          else
            {
              fprintf (stderr, "%s: failed to grab keyboard (%s, %s): %s\n",
                       blurb(),
                       (mouse_sync_p ? "sync" : "async"),
                       (kbd_sync_p   ? "sync" : "async"),
                       grab_string (status));
              exit(1);
            }
        }

      if (grab_mouse_p)
        {
          status = XGrabPointer (dpy, w, True, 
                                 (ButtonPressMask   | ButtonReleaseMask |
                                  EnterWindowMask   | LeaveWindowMask |
                                  PointerMotionMask | PointerMotionHintMask |
                                  Button1MotionMask | Button2MotionMask |
                                  Button3MotionMask | Button4MotionMask |
                                  Button5MotionMask | ButtonMotionMask),
                                 (mouse_sync_p ? GrabModeSync : GrabModeAsync),
                                 (kbd_sync_p   ? GrabModeSync : GrabModeAsync),
                                 w, cursor, CurrentTime);
          if (status == GrabSuccess)
            fprintf (stderr, "%s: grabbed mouse (%s, %s)\n", blurb(),
                     (mouse_sync_p ? "sync" : "async"),
                     (kbd_sync_p   ? "sync" : "async"));
          else
            {
              fprintf (stderr, "%s: failed to grab mouse (%s, %s): %s\n",
                       blurb(),
                       (mouse_sync_p ? "sync" : "async"),
                       (kbd_sync_p   ? "sync" : "async"),
                       grab_string (status));
              exit(1);
            }
        }

      fprintf (stderr, "%s: ungrabbing in %d seconds\n", blurb(), timeout);
      XtAppAddTimeOut (app, 1000 * timeout, ungrab_timer, (XtPointer) dpy);
    }

  while (1)
    {
      XEvent xev;
      XIRawEvent *re;

      XtAppNextEvent (app, &xev);
      XtDispatchEvent (&xev);

      switch (xev.xany.type) {
      case KeyPress:
      case KeyRelease:
        {
          static XComposeStatus compose = { 0, };
          KeySym keysym = 0;
          char c[100];
          int n;
          n = XLookupString (&xev.xkey, c, sizeof(c)-1, &keysym, &compose);
          c[n] = 0;
          fprintf (stderr, "%s: X11 Key%s      %02x %02x %s \"%s\"\n", blurb(),
                   (xev.xkey.type == KeyPress ? "Press  " : "Release"),
                   xev.xkey.keycode, xev.xkey.state,
                   XKeysymToString (keysym), c);
        }
        break;
      case ButtonPress:
      case ButtonRelease:
        fprintf (stderr, "%s: X11 Button%s   %d %d\n", blurb(),
                 (xev.xany.type == ButtonPress ? "Press  " : "Release"),
                 xev.xbutton.button, xev.xbutton.state);
        break;
      case MotionNotify:
        fprintf (stderr, "%s: X11 MotionNotify   %4d, %-4d\n",
                 blurb(), xev.xmotion.x_root, xev.xmotion.y_root);
        break;
      case GenericEvent:
        break;
      case EnterNotify:
      case LeaveNotify:
        break;
      default:
        fprintf (stderr, "%s: X11 event %d on 0x%lx\n",
                 blurb(), xev.xany.type, xev.xany.window);
        break;
      }

      if (xev.xcookie.type != GenericEvent ||
          xev.xcookie.extension != xi_opcode)
        continue;  /* not an XInput event */
      if (!xev.xcookie.data)
        XGetEventData (dpy, &xev.xcookie);
      if (!xev.xcookie.data)
        continue;  /* Bogus XInput event */

      re = xev.xcookie.data;
      switch (xev.xcookie.evtype) {
      case XI_RawKeyPress:
      case XI_RawKeyRelease:
        {
          /* Fake up an XKeyEvent in order to call XKeysymToString(). */
          XEvent ev2;
          Bool ok = xinput_event_to_xlib (xev.xcookie.evtype,
                                          (XIDeviceEvent *) re,
                                          &ev2);
          if (!ok)
            fprintf (stderr, "%s: unable to translate XInput2 event\n",
                     blurb());
          else
            {
              static XComposeStatus compose = { 0, };
              KeySym keysym = 0;
              char c[100];
              int n;
              n = XLookupString (&ev2.xkey, c, sizeof(c)-1, &keysym, &compose);
              c[n] = 0;
              fprintf (stderr, "%s: XI_RawKey%s    %02x %02x %s \"%s\"\n",
                       blurb(),
                       (ev2.xkey.type == KeyPress ? "Press  " : "Release"),
                       ev2.xkey.keycode, ev2.xkey.state,
                       XKeysymToString (keysym), c);
            }
        }
        break;
      case XI_RawButtonPress:
      case XI_RawButtonRelease:
        fprintf (stderr, "%s: XI_RawButton%s %d\n", blurb(),
                 (re->evtype == XI_RawButtonPress ? "Press  " : "Release"),
                 re->detail);
        break;
      case XI_RawMotion:
        {
          Window root_ret, child_ret;
          int root_x, root_y;
          int win_x, win_y;
          unsigned int mask;
          XQueryPointer (dpy, DefaultRootWindow (dpy),
                         &root_ret, &child_ret, &root_x, &root_y,
                         &win_x, &win_y, &mask);
          fprintf (stderr, "%s: XI_RawMotion       %4d, %-4d  %7.02f, %-7.02f\n",
                   blurb(),
                   root_x, root_y,
                   re->raw_values[0], re->raw_values[1]);
        }
        break;
      case XI_RawTouchBegin:
        fprintf (stderr, "%s: XI_RawTouchBegin\n", blurb());
        break;
      case XI_RawTouchEnd:
        fprintf (stderr, "%s: XI_RawTouchEnd", blurb());
        break;
      case XI_RawTouchUpdate:
        fprintf (stderr, "%s: XI_RawTouchUpdate", blurb());
        break;

      default:
        fprintf (stderr, "%s: XInput unknown event %d\n", blurb(),
                 xev.xcookie.evtype);
        break;
      }

      XFreeEventData (dpy, &xev.xcookie);
    }

  exit (0);
}
