/* xscreensaver-command, Copyright © 1991-2023 Jamie Zawinski <jwz@jwz.org>
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
     XA_DEACTIVATE, XA_CYCLE, XA_RESTART,
     XA_NET_WM_PID, XA_NET_WM_STATE, XA_NET_WM_STATE_ABOVE,
     XA_NET_WM_STATE_FULLSCREEN, XA_NET_WM_BYPASS_COMPOSITOR,
     XA_NET_WM_STATE_STAYS_ON_TOP, XA_KDE_NET_WM_WINDOW_TYPE_OVERRIDE,
     XA_NET_WM_WINDOW_TYPE, XA_NET_WM_WINDOW_TYPE_SPLASH,
     XA_NET_WM_WINDOW_TYPE_DIALOG, XA_NET_WM_WINDOW_TYPE_NOTIFICATION,
     XA_NET_WM_WINDOW_TYPE_NORMAL;

void
init_xscreensaver_atoms (Display *dpy)
{
# define A(N) XInternAtom (dpy, (N), False)
  XA_SCREENSAVER          = A("SCREENSAVER");
  XA_SCREENSAVER_ID       = A("_SCREENSAVER_ID");
  XA_SCREENSAVER_VERSION  = A("_SCREENSAVER_VERSION");
  XA_SCREENSAVER_STATUS   = A("_SCREENSAVER_STATUS");
  XA_SCREENSAVER_RESPONSE = A("_SCREENSAVER_RESPONSE");

  XA_ACTIVATE   = A("ACTIVATE");
  XA_DEACTIVATE = A("DEACTIVATE");
  XA_SUSPEND    = A("SUSPEND");
  XA_RESTART    = A("RESTART");
  XA_CYCLE      = A("CYCLE");
  XA_NEXT       = A("NEXT");
  XA_PREV       = A("PREV");
  XA_SELECT     = A("SELECT");
  XA_EXIT       = A("EXIT");
  XA_DEMO       = A("DEMO");
  XA_LOCK       = A("LOCK");
  XA_BLANK      = A("BLANK");

  XA_NET_WM_PID                      = A("_NET_WM_PID");
  XA_NET_WM_STATE                    = A("_NET_WM_STATE");
  XA_NET_WM_STATE_ABOVE              = A("_NET_WM_STATE_ABOVE");
  XA_NET_WM_STATE_FULLSCREEN         = A("_NET_WM_STATE_FULLSCREEN");
  XA_NET_WM_BYPASS_COMPOSITOR        = A("_NET_WM_BYPASS_COMPOSITOR");
  XA_NET_WM_WINDOW_TYPE              = A("_NET_WM_WINDOW_TYPE");
  XA_NET_WM_WINDOW_TYPE_SPLASH       = A("_NET_WM_WINDOW_TYPE_SPLASH");
  XA_NET_WM_WINDOW_TYPE_DIALOG       = A("_NET_WM_WINDOW_TYPE_DIALOG");
  XA_NET_WM_WINDOW_TYPE_NOTIFICATION = A("_NET_WM_WINDOW_TYPE_NOTIFICATION");
  XA_NET_WM_WINDOW_TYPE_NORMAL       = A("_NET_WM_WINDOW_TYPE_NORMAL");
  XA_NET_WM_STATE_STAYS_ON_TOP       = A("_NET_WM_STATE_STAYS_ON_TOP");
  XA_KDE_NET_WM_WINDOW_TYPE_OVERRIDE = A("_KDE_NET_WM_WINDOW_TYPE_OVERRIDE");

# undef A
}
