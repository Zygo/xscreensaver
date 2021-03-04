/* xscreensaver, Copyright © 1991-2021 Jamie Zawinski <jwz@jwz.org>
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
#include <X11/extensions/XInput2.h>

#include "blurb.h"
#include "xinput.h"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))


/* Initialize the XInput2 extension. Returns True on success.
 */
Bool
init_xinput (Display *dpy, int *opcode_ret)
{
  int nscreens = ScreenCount (dpy);
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

  for (i = 0; i < nscreens; i++)
    {
      Window root = RootWindow (dpy, i);
      if (XISelectEvents (dpy, root, evmasks, countof(evmasks)) != Success)
        {
          fprintf (stderr, "%s: XISelectEvents failed\n", blurb());
          return False;
        }
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

     However, I'm seeing random noise in the various XIDeviceEvent.mods fields.
     Nonsensical values like base = 0x6045FB3D.  So, let's poll the actual
     modifiers from XQueryPointer.  This can race: maybe the modifier state
     changed between when the server generated the keyboard event, and when
     we receive it and poll.  However, if an actual human is typing and
     releasing their modifier keys on such a tight timeframe... that's
     probably already not going well.

     I'm also seeing random noise in the event_xy and root_xy fields in
     motion events.  So just always use XQueryPointer.
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
  default:
    break;
  }

  return ok;
}
