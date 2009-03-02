/* xscreensaver-command, Copyright (c) 1991-1995 Jamie Zawinski <jwz@mcom.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include "version.h"
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xos.h>
#if __STDC__
# include <stdlib.h>
#endif

#ifdef _VROOT_H_
ERROR! you must not include vroot.h in this file
#endif

static char *screensaver_version;
static char *usage = "usage: %s -<switch>\n\
\n\
  This program provides external control of a running xscreensaver process.\n\
  Version %s, copyright (c) 1991-1994 Jamie Zawinski <jwz@mcom.com>.\n\
\n\
  -demo		Enter interactive demo mode.\n\
  -deactivate	Turns off the screensaver if it is on, as user input would.\n\
  -activate	Turns it on as if the user had been idle for long enough.\n\
  -cycle	Stops the current hack and runs a new one.\n\
  -next		Like either -activate or -cycle, depending on which is more\n\
		appropriate, except that the screenhack that will be run is\n\
		the next one in the list of hacks, instead of a randomly-\n\
		chosen one.  This option is good for looking at a demo of\n\
		each of the hacks in place.\n\
  -prev		Like -next, but goes in the other direction.\n\
  -exit		Causes the screensaver process to exit.  It should be ok to\n\
		just kill the process (NOT with -9!) but this is a slightly\n\
		easier way.\n\
  -restart	Causes the screensaver process to exit and then restart with\n\
		the same command line arguments.  This is a good way of \n\
		causing the screensaver to re-read the resource database.\n\
  -lock         Same as -activate, but with immediate locking.\n\
\n\
  See the man page for more details.\n\n";

static Window
find_screensaver_window (dpy, progname)
     Display *dpy;
     char *progname;
{
  int i;
  Window root = RootWindowOfScreen (DefaultScreenOfDisplay (dpy));
  Window root2, parent, *kids;
  unsigned int nkids;

  if (! XQueryTree (dpy, root, &root2, &parent, &kids, &nkids))
    abort ();
  if (root != root2)
    abort ();
  if (parent)
    abort ();
  if (! (kids && nkids))
    abort ();
  for (i = 0; i < nkids; i++)
    {
      Atom type;
      int format;
      unsigned long nitems, bytesafter;
      char *version;

      if (XGetWindowProperty (dpy, kids[i],
			      XInternAtom (dpy, "_SCREENSAVER_VERSION", False),
			      0, 1, False, XA_STRING,
			      &type, &format, &nitems, &bytesafter,
			      (unsigned char **) &version)
	  == Success
	  && type != None)
	return kids[i];
    }
  fprintf (stderr, "%s: no screensaver is running on display %s\n", progname,
	   DisplayString (dpy));
  exit (1);
}


#define USAGE() \
 { fprintf (stderr, usage, argv[0], screensaver_version); exit (1); }

void
main (argc, argv)
     int argc;
     char **argv;
{
  Display *dpy;
  Window window;
  XEvent event;
  int i;
  char *message = 0, *dpyname = 0;
  screensaver_version = (char *) malloc (5);
  memcpy (screensaver_version, screensaver_id + 17, 4);
  screensaver_version [4] = 0;
  for (i = 1; i < argc; i++)
    {
      char *s = argv [i];
      int L = strlen (s);
      if (L < 2) USAGE ();
      if (!strncmp (s, "-display", L))		dpyname = argv [++i];
      else if (message) USAGE ()
      else if (!strncmp (s, "-activate", L))	message = "ACTIVATE";
      else if (!strncmp (s, "-deactivate", L))	message = "DEACTIVATE";
      else if (!strncmp (s, "-cycle", L))	message = "CYCLE";
      else if (!strncmp (s, "-next", L))	message = "NEXT";
      else if (!strncmp (s, "-prev", L))	message = "PREV";
      else if (!strncmp (s, "-exit", L))	message = "EXIT";
      else if (!strncmp (s, "-restart", L))	message = "RESTART";
      else if (!strncmp (s, "-demo", L))	message = "DEMO";
      else if (!strncmp (s, "-lock", L))	message = "LOCK";
      else USAGE ();
    }
  if (! message) USAGE ();
  if (!dpyname) dpyname = (char *) getenv ("DISPLAY");
  dpy = XOpenDisplay (dpyname);
  if (!dpy)
    {
      fprintf (stderr, "%s: can't open display %s\n", argv[0],
	       (dpyname ? dpyname : "(null)"));
      exit (1);
    }
  window = find_screensaver_window (dpy, argv[0]);

  event.xany.type = ClientMessage;
  event.xclient.display = dpy;
  event.xclient.window = window;
  event.xclient.message_type = XInternAtom (dpy, "SCREENSAVER", False);
  event.xclient.format = 32;
  event.xclient.data.l[0] = (long) XInternAtom (dpy, message, False);
  if (! XSendEvent (dpy, window, False, 0L, &event))
    {
      fprintf (stderr, "%s: XSendEvent(dpy, 0x%x ...) failed.\n", argv [0],
	       (unsigned int) window);
      exit (1);
    }
  XSync (dpy, 0);
  exit (0);
}
