/* xscreensaver, Copyright © 1991-2022 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* For our purposes, here are the salient facts about the XInput2 extension:

   - Available by default as of X11R7 in 2009.

   - We receive events from the XIRawEvent family while other processes
     have the keyboard and mouse grabbed (XI_RawKeyPress, etc.)
     The entire xscreensaver-auth security model hinges on this fact.

   - We cannot receive events from the XIDeviceEvent family (XI_KeyPress,
     etc.) so while XIDeviceEvents contain many fields that we would like
     to have, don't be fooled, those are not available.  XIRawEvents such
     as XI_RawKeyPress cannot be cast to XIDeviceEvent.

   - XI_RawButtonPress, XI_RawButtonRelease and XI_RawMotion contain no
     usable position information.  The non-raw versions do, but we don't
     receive those, so we have to read that info via XQueryPointer.

   - XI_RawKeyPress and XI_RawKeyRelease contain the keycode (in 'detail')
     but no modifier info, so, again, to be able to tell "a" from "A" we
     need to call XQueryPointer.

   - XI_RawKeyPress does not auto-repeat the way Xlib KeyPress does.

   - Raw events have no window, subwindow, etc.

   - Event serial numbers are not unique.

   - If *this* process has the keyboard and mouse grabbed, it receives:

     - KeyPress, KeyRelease
     - ButtonPress, ButtonRelease, MotionNotify
     - XI_RawButtonPress, XI_RawButtonRelease, XI_RawMotion (doubly reported)

   - If this process *does not* have keyboard and mouse grabbed, it receives:

     - XI_RawKeyPress, XI_RawKeyRelease
     - XI_RawButtonPress, XI_RawButtonRelease, XI_RawMotion

   The closest thing to actual documentation on XInput2 seems to be a series
   of blog posts by Peter Hutterer.  There's basically nothing about it on
   www.x.org.  https://who-t.blogspot.com/2009/07/xi2-recipes-part-4.html
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>

#include "blurb.h"
#include "xinput.h"

extern Bool debug_p;

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))


/* Initialize the XInput2 extension. Returns True on success.
 */
Bool
init_xinput (Display *dpy, int *opcode_ret)
{
  XIEventMask evmasks[1];
  unsigned char mask1[(XI_LASTEVENT + 7)/8];
  int major, minor;
  int xi_opcode, ev, err;
  int i;
  int ndevs = 0;
  XIDeviceInfo *devs;

  if (!XQueryExtension (dpy, "XInputExtension", &xi_opcode, &ev, &err))
    {
      fprintf (stderr, "%s: XInput extension missing\n", blurb());
      return False;
    }

  major = 2;	/* Desired version */
  minor = 2;
  if (XIQueryVersion (dpy, &major, &minor) != Success)
    {
      fprintf (stderr, "%s: server only supports XInput %d.%d\n",
               blurb(), major, minor);
      return False;
    }

  if (verbose_p)
    fprintf (stderr, "%s: XInput version %d.%d\n", blurb(), major, minor);

  memset (mask1, 0, sizeof(mask1));

  /* The XIRawEvent family of events are delivered to us while someone else
     holds a grab, but the non-raw versions are not.  In fact, attempting to
     select XI_KeyPress, etc. results in a BadAccess X error.
   */
  XISetMask (mask1, XI_RawMotion);
  XISetMask (mask1, XI_RawKeyPress);
  XISetMask (mask1, XI_RawKeyRelease);
  XISetMask (mask1, XI_RawButtonPress);
  XISetMask (mask1, XI_RawButtonRelease);
  XISetMask (mask1, XI_RawTouchBegin);
  XISetMask (mask1, XI_RawTouchUpdate);
  XISetMask (mask1, XI_RawTouchEnd);

  /* If we use XIAllDevices instead, we get duplicate events. */
  evmasks[0].deviceid = XIAllMasterDevices;
  evmasks[0].mask_len = sizeof(mask1);
  evmasks[0].mask = mask1;

  /* Only select events on screen 0 -- if we select on each screen,
     we get duplicate events. */
  if (XISelectEvents (dpy, RootWindow (dpy, 0), evmasks, countof(evmasks))
      != Success)
    {
      fprintf (stderr, "%s: XISelectEvents failed\n", blurb());
      return False;
    }

  XFlush (dpy);

  devs = XIQueryDevice (dpy, XIAllDevices, &ndevs);
  if (!ndevs)
    {
      fprintf (stderr, "%s: XInput: no devices\n", blurb());
      return False;
    }

  if (verbose_p)
    for (i = 0; i < ndevs; i++)
      {
        XIDeviceInfo *d = &devs[i];
        fprintf (stderr, "%s:   device %2d/%d: %s: %s\n",
                 blurb(), d->deviceid, d->attachment, 
                 (d->use == XIMasterPointer  ? "MP" :
                  d->use == XIMasterKeyboard ? "MK" :
                  d->use == XISlavePointer   ? "SP" :
                  d->use == XISlaveKeyboard  ? "SK" :
                  d->use == XIFloatingSlave  ? "FS" : "??"),
                 d->name);
      }

  XIFreeDeviceInfo (devs);
  *opcode_ret = xi_opcode;
  return True;
}


/* Convert an XInput2 event to corresponding old-school Xlib event.
   Returns true on success.
 */
Bool
xinput_event_to_xlib (int evtype, XIDeviceEvent *in, XEvent *out)
{
  Display *dpy = in->display;
  Bool ok = False;

  int root_x = 0, root_y = 0;
  int win_x = 0,  win_y = 0;
  unsigned int mods = 0;
  Window subw = 0;

  switch (evtype) {
  case XI_RawKeyPress:
  case XI_RawKeyRelease:
  case XI_RawButtonPress:
  case XI_RawButtonRelease:
  case XI_RawMotion:
    {
      Window root_ret;
      int win_x, win_y;
      int i;
      for (i = 0; i < ScreenCount (dpy); i++)   /* query on correct screen */
        if (XQueryPointer (dpy, RootWindow (dpy, i),
                           &root_ret, &subw, &root_x, &root_y,
                           &win_x, &win_y, &mods))
          break;
    }
  default: break;
  }

  switch (evtype) {
  case XI_RawKeyPress:
  case XI_RawKeyRelease:
    out->xkey.type      = (evtype == XI_RawKeyPress ? KeyPress : KeyRelease);
    out->xkey.display   = in->display;
    out->xkey.root      = in->root;
    out->xkey.window    = subw;
    out->xkey.subwindow = subw;
    out->xkey.time      = in->time;
    out->xkey.x         = win_x;
    out->xkey.y         = win_y;
    out->xkey.x_root    = root_x;
    out->xkey.y_root    = root_y;
    out->xkey.state     = mods;
    out->xkey.keycode   = in->detail;
    ok = True;
    break;
  case XI_RawButtonPress:
  case XI_RawButtonRelease:
    out->xbutton.type      = (evtype == XI_RawButtonPress 
                              ? ButtonPress : ButtonRelease);
    out->xbutton.display   = in->display;
    out->xbutton.root      = in->root;
    out->xbutton.window    = subw;
    out->xbutton.subwindow = subw;
    out->xbutton.time      = in->time;
    out->xbutton.x         = win_x;
    out->xbutton.y         = win_y;
    out->xbutton.x_root    = root_x;
    out->xbutton.y_root    = root_y;
    out->xbutton.state     = mods;
    out->xbutton.button    = in->detail;
    ok = True;
    break;
  case XI_RawMotion:
    out->xmotion.type      = MotionNotify;
    out->xmotion.display   = in->display;
    out->xmotion.root      = in->root;
    out->xmotion.window    = subw;
    out->xmotion.subwindow = subw;
    out->xmotion.time      = in->time;
    out->xmotion.x         = win_x;
    out->xmotion.y         = win_y;
    out->xmotion.x_root    = root_x;
    out->xmotion.y_root    = root_y;
    out->xmotion.state     = mods;
    ok = True;
    break;
  case XI_RawTouchBegin:
  case XI_RawTouchEnd:
    out->xbutton.type      = (evtype == XI_RawTouchBegin 
                              ? ButtonPress : ButtonRelease);
    out->xbutton.display   = in->display;
    out->xbutton.root      = in->root;
    out->xbutton.window    = subw;
    out->xbutton.subwindow = subw;
    out->xbutton.time      = in->time;
    out->xbutton.x         = win_x;
    out->xbutton.y         = win_y;
    out->xbutton.x_root    = root_x;
    out->xbutton.y_root    = root_y;
    out->xbutton.state     = mods;
    out->xbutton.button    = in->detail;
    ok = True;
    break;
  default:
    break;
  }

  return ok;
}


static void
print_kbd_event (XKeyEvent *xkey, XComposeStatus *compose, Bool x11_p)
{
  if (debug_p)		/* Passwords show up in plaintext! */
    {
      KeySym keysym = 0;
      char c[100];
      char M[100], *mods = M;
      int n = XLookupString (xkey, c, sizeof(c)-1, &keysym, compose);
      const char *ks = keysym ? XKeysymToString (keysym) : "NULL";
      c[n] = 0;
      if      (*c == '\n') strcpy (c, "\\n");
      else if (*c == '\r') strcpy (c, "\\r");
      else if (*c == '\t') strcpy (c, "\\t");

      *mods = 0;
      if (xkey->state & ShiftMask)   strcat (mods, "-Sh");
      if (xkey->state & LockMask)    strcat (mods, "-Lk");
      if (xkey->state & ControlMask) strcat (mods, "-C");
      if (xkey->state & Mod1Mask)    strcat (mods, "-M1");
      if (xkey->state & Mod2Mask)    strcat (mods, "-M2");
      if (xkey->state & Mod3Mask)    strcat (mods, "-M3");
      if (xkey->state & Mod4Mask)    strcat (mods, "-M4");
      if (xkey->state & Mod5Mask)    strcat (mods, "-M5");
      if (*mods) mods++;
      if (!*mods) strcat (mods, "0");

      fprintf (stderr, "%s: %s    0x%02X %s %s \"%s\"\n", blurb(),
               (x11_p
                ? (xkey->type == KeyPress
                   ? "X11 KeyPress    "
                   : "X11 KeyRelease  ")
                : (xkey->type == KeyPress
                   ? "XI_RawKeyPress  "
                   : "XI_RawKeyRelease")),
               xkey->keycode, mods, ks, c);
    }
  else			/* Log only that the KeyPress happened. */
    {
      fprintf (stderr, "%s: X11 Key%s\n", blurb(),
               (xkey->type == KeyPress ? "Press  " : "Release"));
    }
}


void
print_xinput_event (Display *dpy, XEvent *xev, const char *desc)
{
  XIRawEvent *re;

  switch (xev->xany.type) {
  case KeyPress:
  case KeyRelease:
    {
      static XComposeStatus compose = { 0, };
      print_kbd_event (&xev->xkey, &compose, True);
    }
    break;

  case ButtonPress:
  case ButtonRelease:
    fprintf (stderr, "%s: X11 Button%s   %d %d\n", blurb(),
             (xev->xany.type == ButtonPress ? "Press  " : "Release"),
             xev->xbutton.button, xev->xbutton.state);
    break;

  case MotionNotify:
    fprintf (stderr, "%s: X11 MotionNotify   %4d, %-4d"
             "                   %s\n",
             blurb(), xev->xmotion.x_root, xev->xmotion.y_root,
             (desc ? desc : ""));
    break;
  default:
    break;
  }

  if (xev->xany.type != GenericEvent)
    return;  /* not an XInput event */
  
  if (!xev->xcookie.data)
    XGetEventData (dpy, &xev->xcookie);

  re = xev->xcookie.data;
  if (!re) return; /* Bogus XInput event */

  switch (re->evtype) {
  case XI_RawKeyPress:
  case XI_RawKeyRelease:
    if (debug_p)
      {
        /* Fake up an XKeyEvent in order to call XKeysymToString(). */
        XEvent ev2;
        Bool ok = xinput_event_to_xlib (re->evtype,
                                        (XIDeviceEvent *) re,
                                        &ev2);
        if (!ok)
          fprintf (stderr, "%s: unable to translate XInput2 event\n", blurb());
        else
          {
            static XComposeStatus compose = { 0, };
            print_kbd_event (&ev2.xkey, &compose, False);
          }
        break;
      }
    else
      fprintf (stderr, "%s: XI RawKey%s\n", blurb(),
               (re->evtype == XI_RawKeyPress ? "Press  " : "Release"));
    break;

  case XI_RawButtonPress:
  case XI_RawButtonRelease:
      {
        Window root_ret, child_ret;
        int root_x, root_y;
        int win_x, win_y;
        unsigned int mask;
        XQueryPointer (dpy, DefaultRootWindow (dpy),
                       &root_ret, &child_ret, &root_x, &root_y,
                       &win_x, &win_y, &mask);
        fprintf (stderr, "%s: XI _RawButton%s %4d, %-4d\n", blurb(),
                 (re->evtype == XI_RawButtonPress ? "Press  " : "Release"),
                 root_x, root_y);
      }
    break;

  case XI_RawMotion:
    if (verbose_p > 1)
      {
        Window root_ret, child_ret;
        int root_x, root_y;
        int win_x, win_y;
        unsigned int mask;
        XQueryPointer (dpy, DefaultRootWindow (dpy),
                       &root_ret, &child_ret, &root_x, &root_y,
                       &win_x, &win_y, &mask);
        fprintf (stderr,
                 "%s: XI_RawMotion       %4d, %-4d  %7.02f, %-7.02f%s\n",
                 blurb(), root_x, root_y, re->raw_values[0], re->raw_values[1],
                 (desc ? desc : ""));
      }
    break;

  /* Touch-screens, possibly trackpads or tablets. */
  case XI_RawTouchBegin:
    fprintf (stderr, "%s: XI RawTouchBegin\n", blurb());
    break;
  case XI_RawTouchEnd:
    fprintf (stderr, "%s: XI RawTouchEnd\n", blurb());
    break;
  case XI_RawTouchUpdate:
    if (verbose_p > 1)
      fprintf (stderr, "%s: XI RawTouchUpdate\n", blurb());
    break;

  default:
    fprintf (stderr, "%s: unknown XInput event %d\n", blurb(), re->type);
    break;
  }
}
