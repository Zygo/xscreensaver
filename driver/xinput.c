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

  XISetMask (mask1, XI_RawMotion);
  XISetMask (mask1, XI_RawKeyPress);
  XISetMask (mask1, XI_RawKeyRelease);
  XISetMask (mask1, XI_RawButtonPress);
  XISetMask (mask1, XI_RawButtonRelease);
  XISetMask (mask1, XI_RawTouchBegin);
  XISetMask (mask1, XI_RawTouchUpdate);
  XISetMask (mask1, XI_RawTouchEnd);

  /* If we use XIAllDevices instead, we get double events. */
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
  unsigned int mods = 0;

  /* The closest thing to actual documentation on XInput2 seems to be a series
     of blog posts by Peter Hutterer.  There's basically nothing about it on
     www.x.org.  In http://who-t.blogspot.com/2009/07/xi2-recipes-part-4.html
     he says: 

       "XIDeviceEvent [...] contains the state of the modifier keys [...]
       The base modifiers are the ones currently pressed, latched the ones
       pressed until a key is pressed that's configured to unlatch it (e.g.
       some shift-capslock interactions have this behaviour) and finally
       locked modifiers are the ones permanently active until unlocked
       (default capslock behaviour in the US layout). The effective modifiers
       are a bitwise OR of the three above - which is essentially equivalent
       to the modifiers state supplied in the core protocol events."

     However, it appears that the XInput2 library is returning uninitialized
     data for *many* of the components of its XIDeviceEvent responses.  In
     particular, the XIModifierState fields have ludicrously large and
     incorrect values like 0x6045FB3D; and the root_x/y and event_x/y values
     on XI_RawButton* and XI_RawMotion events are always nonsense.

     This is very worrying, as it suggests that the XInput2 extension is
     returning uninitialized or freed memory from the X11 server process.
     Build and run the "driver/test-xinput" program for an easy way to see
     this bug in action.

     To work around this, we just always get the current modifier state and
     mouse position by calling XQueryPointer instead of reading the fields
     from the event structures.  (This can technically race: maybe the
     modifier state changed between when the server generated the keyboard
     event, and when we receive it and poll.  But we're talking nanoseconds
     here: the window for that race is way smaller than the precision of human
     finger muscles.)
   */
  switch (evtype) {
  case XI_RawKeyPress:
  case XI_RawKeyRelease:
  case XI_RawButtonPress:
  case XI_RawButtonRelease:
  case XI_RawMotion:
    {
      Window root_ret, child_ret;
      int win_x, win_y;
      int i;
      for (i = 0; i < ScreenCount (dpy); i++)   /* query on correct screen */
        if (XQueryPointer (dpy, RootWindow (dpy, i),
                           &root_ret, &child_ret, &root_x, &root_y,
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
    out->xkey.window    = in->event;
    out->xkey.root      = in->root;
    out->xkey.subwindow = in->child;
    out->xkey.time      = in->time;
    out->xkey.x         = root_x;
    out->xkey.y         = root_y;
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
    out->xbutton.window    = in->event;
    out->xbutton.root      = in->root;
    out->xbutton.subwindow = in->child;
    out->xbutton.time      = in->time;
    out->xbutton.x         = root_x;
    out->xbutton.y         = root_y;
    out->xbutton.x_root    = root_x;
    out->xbutton.y_root    = root_y;
    out->xbutton.state     = mods;
    out->xbutton.button    = in->detail;
    ok = True;
    break;
  case XI_RawMotion:
    out->xmotion.type      = MotionNotify;
    out->xmotion.display   = in->display;
    out->xmotion.window    = in->event;
    out->xmotion.root      = in->root;
    out->xmotion.subwindow = in->child;
    out->xmotion.time      = in->time;
    out->xmotion.x         = root_x;
    out->xmotion.y         = root_y;
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
    out->xbutton.window    = in->event;
    out->xbutton.root      = in->root;
    out->xbutton.subwindow = in->child;
    out->xbutton.time      = in->time;
    out->xbutton.x         = root_x;
    out->xbutton.y         = root_y;
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
print_kbd_event (XEvent *xev, XComposeStatus *compose, Bool x11_p)
{
  if (debug_p)		/* Passwords show up in plaintext! */
    {
      KeySym keysym = 0;
      char c[100];
      char M[100], *mods = M;
      int n = XLookupString (&xev->xkey, c, sizeof(c)-1, &keysym, compose);
      const char *ks = keysym ? XKeysymToString (keysym) : "NULL";
      c[n] = 0;
      if      (*c == '\n') strcpy (c, "\\n");
      else if (*c == '\r') strcpy (c, "\\r");
      else if (*c == '\t') strcpy (c, "\\t");

      *mods = 0;
      if (xev->xkey.state & ShiftMask)   strcat (mods, "-Sh");
      if (xev->xkey.state & LockMask)    strcat (mods, "-Lk");
      if (xev->xkey.state & ControlMask) strcat (mods, "-C");
      if (xev->xkey.state & Mod1Mask)    strcat (mods, "-M1");
      if (xev->xkey.state & Mod2Mask)    strcat (mods, "-M2");
      if (xev->xkey.state & Mod3Mask)    strcat (mods, "-M3");
      if (xev->xkey.state & Mod4Mask)    strcat (mods, "-M4");
      if (xev->xkey.state & Mod5Mask)    strcat (mods, "-M5");
      if (*mods) mods++;
      if (!*mods) strcat (mods, "0");

      fprintf (stderr, "%s: %s    0x%02X %s %s \"%s\"\n", blurb(),
               (x11_p
                ? (xev->xkey.type == KeyPress
                   ? "X11 KeyPress    "
                   : "X11 KeyRelease  ")
                : (xev->xkey.type == KeyPress
                   ? "XI_RawKeyPress  "
                   : "XI_RawKeyRelease")),
               xev->xkey.keycode, mods, ks, c);
    }
  else			/* Log only that the KeyPress happened. */
    {
      fprintf (stderr, "%s: X11 Key%s\n", blurb(),
               (xev->xkey.type == KeyPress ? "Press  " : "Release"));
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
      print_kbd_event (xev, &compose, True);
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
            print_kbd_event (&ev2, &compose, False);
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
        fprintf (stderr, "%s: XI _RawButton%s %4d, %-4d  %7.02f, %-7.02f\n",
                 blurb(),
                 (re->evtype == XI_RawButtonPress ? "Press  " : "Release"),
                 root_x, root_y,
                 re->raw_values[0], re->raw_values[1]);
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
