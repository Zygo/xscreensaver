/* xscreensaver-command, Copyright © 1991-2021 Jamie Zawinski <jwz@jwz.org>
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

#include <X11/Xproto.h>		/* for CARD32 */
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "atoms.h"

Atom XA_SCREENSAVER, XA_SCREENSAVER_VERSION, XA_SCREENSAVER_RESPONSE,
     XA_SCREENSAVER_ID, XA_SCREENSAVER_STATUS, XA_SELECT, XA_DEMO, XA_EXIT,
     XA_BLANK, XA_LOCK, XA_ACTIVATE, XA_SUSPEND, XA_NEXT, XA_PREV,
     XA_DEACTIVATE, XA_CYCLE, XA_RESTART, XA_PREFS;

void
init_xscreensaver_atoms (Display *dpy)
{
  XA_SCREENSAVER = XInternAtom (dpy, "SCREENSAVER", False);
  XA_SCREENSAVER_ID = XInternAtom (dpy, "_SCREENSAVER_ID", False);
  XA_SCREENSAVER_VERSION = XInternAtom (dpy, "_SCREENSAVER_VERSION",False);
  XA_SCREENSAVER_STATUS = XInternAtom (dpy, "_SCREENSAVER_STATUS", False);
  XA_SCREENSAVER_RESPONSE = XInternAtom (dpy, "_SCREENSAVER_RESPONSE", False);
  XA_ACTIVATE = XInternAtom (dpy, "ACTIVATE", False);
  XA_DEACTIVATE = XInternAtom (dpy, "DEACTIVATE", False);
  XA_SUSPEND = XInternAtom (dpy, "SUSPEND", False);
  XA_RESTART = XInternAtom (dpy, "RESTART", False);
  XA_CYCLE = XInternAtom (dpy, "CYCLE", False);
  XA_NEXT = XInternAtom (dpy, "NEXT", False);
  XA_PREV = XInternAtom (dpy, "PREV", False);
  XA_SELECT = XInternAtom (dpy, "SELECT", False);
  XA_EXIT = XInternAtom (dpy, "EXIT", False);
  XA_DEMO = XInternAtom (dpy, "DEMO", False);
  XA_PREFS = XInternAtom (dpy, "PREFS", False);
  XA_LOCK = XInternAtom (dpy, "LOCK", False);
  XA_BLANK = XInternAtom (dpy, "BLANK", False);
}

