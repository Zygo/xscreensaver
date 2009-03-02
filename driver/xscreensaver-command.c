/* xscreensaver-command, Copyright (c) 1991-1997
 *  by Jamie Zawinski <jwz@netscape.com>
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
#include <X11/Xproto.h>		/* for CARD32 */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>		/* for XGetClassHint() */
#include <X11/Xos.h>
#include <stdlib.h>

#include "version.h"

#ifdef _VROOT_H_
ERROR! you must not include vroot.h in this file
#endif

static char *screensaver_version;
static char *usage = "usage: %s -<switch>\n\
\n\
  This program provides external control of a running xscreensaver process.\n\
  Version %s, copyright (c) 1991-1997 Jamie Zawinski <jwz@netscape.com>.\n\
\n\
  -demo         Enter interactive demo mode.\n\
  -deactivate   Turns off the screensaver if it is on, as user input would.\n\
  -activate     Turns it on as if the user had been idle for long enough.\n\
  -cycle        Stops the current graphics hack and runs a new one.\n\
  -next         Like either -activate or -cycle, depending on which is more\n\
                appropriate, except that the screenhack that will be run is\n\
                the next one in the list of hacks, instead of a randomly-\n\
                chosen one.  This option could be used for looking at a demo\n\
                of each of the configured hacks.\n\
  -prev         Like -next, but goes in the other direction.\n\
  -exit         Causes the screensaver process to exit.  This is the same as\n\
                killing the process with `kill', but it's easier, since you\n\
                don't need to first figure out the pid.  (Note that one\n\
                must *never* kill xscreensaver with -9!)\n\
  -restart      Causes the screensaver process to exit and then restart with\n\
                the same command line arguments.  Do this after you've\n\
                changed the resource database, to cause the screensaver to\n\
                notice the changes.\n\
  -lock         Same as -activate, but with immediate locking.\n\
  -version      Prints the version of XScreenSaver that is currently running\n\
                on the display.\n\
  -time         Prints the time at which the screensaver last activated or\n\
                deactivated (roughly, how long the user has been idle or\n\
                non-idle.)\n\
\n\
  See the man page for more details.\n\
  For updates, check http://people.netscape.com/jwz/xscreensaver/\n\
\n";

static Window
find_screensaver_window (Display *dpy, char *progname, char **version)
{
  int i;
  Window root = RootWindowOfScreen (DefaultScreenOfDisplay (dpy));
  Window root2, parent, *kids;
  unsigned int nkids;

  if (version) *version = 0;

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
      char *v;

      if (XGetWindowProperty (dpy, kids[i],
			      XInternAtom (dpy, "_SCREENSAVER_VERSION", False),
			      0, 200, False, XA_STRING,
			      &type, &format, &nitems, &bytesafter,
			      (unsigned char **) &v)
	  == Success
	  && type != None)
	{
	  if (version)
	    *version = v;
	  return kids[i];
	}
    }
  fprintf (stderr, "%s: no screensaver is running on display %s\n", progname,
	   DisplayString (dpy));
  exit (1);
}


#define USAGE() \
 { fprintf (stderr, usage, argv[0], screensaver_version); exit (1); }

int
main (int argc, char **argv)
{
  Display *dpy;
  Window window;
  int i;
  int query = 0;
#define Q_version 1
#define Q_time    2
  char *message = 0, *dpyname = 0;
  char *v = 0;

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
      else if (!strncmp (s, "-version", L))	query   = Q_version;
      else if (!strncmp (s, "-time", L))	query   = Q_time;
      else USAGE ();
    }
  if (!message && !query) USAGE ();
  if (!dpyname) dpyname = (char *) getenv ("DISPLAY");
  dpy = XOpenDisplay (dpyname);
  if (!dpy)
    {
      fprintf (stderr, "%s: can't open display %s\n", argv[0],
	       (dpyname ? dpyname : "(null)"));
      exit (1);
    }
  window = find_screensaver_window (dpy, argv[0], &v);

  if (message)
    {
      XEvent event;
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
    }
  else if (query)
    {
      XClassHint hint;
      memset (&hint, 0, sizeof(hint));
      if (!v || !*v)
	{
	  fprintf (stderr, "%s: version property not set on window 0x%x?\n",
		   argv [0], (unsigned int) window);
	  exit (1);
	}

      XGetClassHint(dpy, window, &hint);
      if (!hint.res_class)
	{
	  fprintf (stderr, "%s: class hints not set on window 0x%x?\n",
		   argv [0], (unsigned int) window);
	  exit (1);
	}

      fprintf (stdout, "%s %s", hint.res_class, v);

      if (query != Q_time)
	{
	  fprintf (stdout, "\n");
	}
      else
	{
	  Atom type;
	  int format;
	  unsigned long nitems, bytesafter;
	  unsigned char *data = 0;
	  XWindowAttributes xgwa;
	  Bool active_p = False;

	  xgwa.map_state = IsViewable;
	  XGetWindowAttributes (dpy, window, &xgwa);

	  if (XGetWindowProperty (dpy, window,
				  XInternAtom (dpy, "__SWM_VROOT", False),
				  0, 0, False, XA_WINDOW,
				  &type, &format, &nitems, &bytesafter,
				  &data)
	      == Success
	      && type != None)
	    active_p = True;

	  if (data) free (data);
	  data = 0;

	  if (XGetWindowProperty (dpy, window,
				  XInternAtom (dpy, "_SCREENSAVER_TIME",False),
				  0, 1, False, XA_INTEGER,
				  &type, &format, &nitems, &bytesafter,
				  &data)
	      == Success
	      && type == XA_INTEGER
	      && data)
	    {
	      CARD32 time32 = *((CARD32 *)data);
	      time_t tt = (time_t) time32;

	      if (active_p)
		fprintf (stdout, ": active since ");
	      else
		fprintf (stdout, ": inactive since ");
	      fprintf (stdout, "%s", ctime(&tt));
	      if (data) free (data);
	    }
	  else
	    {
	      if (data) free (data);
	      fprintf (stdout, "\n");
	      fflush (stdout);
	      fprintf (stderr, "%s: no time on window 0x%x (%s %s).\n",
		       argv[0], (unsigned int) window,
		       hint.res_class, (v ? v : "???"));
	      exit (1);
	    }
	}
    }

  XSync (dpy, 0);
  fflush (stdout);
  fflush (stderr);
  exit (0);
}
