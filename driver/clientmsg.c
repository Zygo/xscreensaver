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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "blurb.h"
#include "atoms.h"
#include "clientmsg.h"

extern Bool verbose_p;

static int
error_handler (Display *dpy, XErrorEvent *error)
{
  return 0;
}


Window
find_screensaver_window (Display *dpy, char **version)
{
  int i;
  Window root = RootWindowOfScreen (DefaultScreenOfDisplay (dpy));
  Window root2, parent, *kids;
  unsigned int nkids;
  XErrorHandler old_handler;
  Window ret = 0;

  XSync (dpy, False);
  old_handler = XSetErrorHandler (error_handler);

  if (version) *version = 0;

  if (! XQueryTree (dpy, root, &root2, &parent, &kids, &nkids))
    abort ();
  if (root != root2)
    abort ();
  if (parent)
    abort ();
  if (! (kids && nkids))
    goto DONE;
  for (i = 0; i < nkids; i++)
    {
      Atom type;
      int format;
      unsigned long nitems, bytesafter;
      unsigned char *v = 0;
      int status;

      /* We're walking the list of root-level windows and trying to find
         the one that has a particular property on it.  We need to trap
         BadWindows errors while doing this, because it's possible that
         some random window might get deleted in the meantime.  (That
         window won't have been the one we're looking for.)
       */
      status = XGetWindowProperty (dpy, kids[i],
                                   XA_SCREENSAVER_VERSION,
                                   0, 200, False, XA_STRING,
                                   &type, &format, &nitems, &bytesafter,
                                   &v);
      if (status == Success && type != None)
	{
          ret = kids[i];
	  if (version)
	    *version = (char *) v;
          else
            XFree (v);
          goto DONE;
	}
      if (v) XFree (v);
    }

 DONE:
  if (kids) XFree (kids);
  XSetErrorHandler (old_handler);
  return ret;
}


void
clientmessage_response (Display *dpy, XEvent *xev, Bool ok, const char *msg)
{
  char *proto;
  int L = 0;

  if (verbose_p || !ok)
    {
      Atom cmd = xev->xclient.data.l[0];
      char *name = XGetAtomName (dpy, cmd);
      fprintf (stderr, "%s: ClientMessage %s: %s\n", blurb(), 
               (name ? name : "???"), msg);
    }

  L = strlen (msg);
  proto = (char *) malloc (L + 2);
  if (!proto) return;
  proto[0] = (ok ? '+' : '-');
  memcpy (proto+1, msg, L);
  L++;
  proto[L] = 0;

  XChangeProperty (dpy, xev->xclient.window,
                   XA_SCREENSAVER_RESPONSE, XA_STRING, 8,
		   PropModeReplace, (unsigned char *) proto, L);
  free (proto);
}
