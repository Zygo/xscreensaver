/* xscreensaver, Copyright (c) 1998, 1999 by Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* The XDBE (Double Buffering) extension is pretty tricky to use, since you
   can get X errors at inconvenient times during initialization.  This file
   contains a utility routine to make it easier to deal with.
 */

#include "utils.h"
#include "xdbe.h"
#include "resources.h"		/* for get_string_resource() */

/* #define DEBUG */

#ifdef DEBUG
# include <X11/Xmu/Error.h>
#endif

extern char *progname;

#ifdef HAVE_DOUBLE_BUFFER_EXTENSION	/* whole file */

static Bool xdbe_got_x_error = False;
static int
xdbe_ehandler (Display *dpy, XErrorEvent *error)
{
  xdbe_got_x_error = True;

#ifdef DEBUG
  fprintf (stderr, "\n%s: ignoring X error from DOUBLE-BUFFER:\n", progname);
  XmuPrintDefaultErrorMessage (dpy, error, stderr);
  fprintf (stderr, "\n");
#endif

  return 0;
}


XdbeBackBuffer 
xdbe_get_backbuffer (Display *dpy, Window window,
                     XdbeSwapAction action)
{
  XdbeBackBuffer b;
  XErrorHandler old_handler;
  int maj, min;

  if (!get_boolean_resource("useDBE", "Boolean"))
    return 0;

  if (!XdbeQueryExtension (dpy, &maj, &min))
    return 0;

  XSync (dpy, False);
  xdbe_got_x_error = False;
  old_handler = XSetErrorHandler (xdbe_ehandler);
  b = XdbeAllocateBackBufferName(dpy, window, XdbeUndefined);
  XSync (dpy, False);
  XSetErrorHandler (old_handler);

  if (xdbe_got_x_error)
    return 0;

  return b;
}

#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */
