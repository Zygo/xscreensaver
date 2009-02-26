/* xscreensaver-command, Copyright (c) 1991-1998
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

#define STANDALONE

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <X11/Xproto.h>		/* for CARD32 */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>		/* for XGetClassHint() */
#include <X11/Xos.h>

#include <X11/Intrinsic.h>	/* only needed to get through xscreensaver.h */

#include "version.h"

#ifdef STANDALONE
  static char *progname;
  static Atom XA_VROOT;
  static Atom XA_SCREENSAVER, XA_SCREENSAVER_VERSION, XA_SCREENSAVER_TIME;
#else  /* !STANDALONE */
# include "xscreensaver.h"
#endif /* !STANDALONE */


#ifdef _VROOT_H_
ERROR! you must not include vroot.h in this file
#endif

static Window
find_screensaver_window (Display *dpy, char **version)
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
			      XA_SCREENSAVER_VERSION,
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


void
xscreensaver_command (Display *dpy, Atom command)
{
  char *v = 0;
  Window window = find_screensaver_window (dpy, &v);

  if (command == XA_SCREENSAVER_TIME ||
      command == XA_SCREENSAVER_VERSION)
    {
      XClassHint hint;
      memset (&hint, 0, sizeof(hint));
      if (!v || !*v)
	{
	  fprintf (stderr, "%s: version property not set on window 0x%x?\n",
		   progname, (unsigned int) window);
	  exit (1);
	}

      XGetClassHint(dpy, window, &hint);
      if (!hint.res_class)
	{
	  fprintf (stderr, "%s: class hints not set on window 0x%x?\n",
		   progname, (unsigned int) window);
	  exit (1);
	}

      fprintf (stdout, "%s %s", hint.res_class, v);

      if (command != XA_SCREENSAVER_TIME)
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

	  if (XGetWindowProperty (dpy, window, XA_VROOT,
				  0, 0, False, XA_WINDOW,
				  &type, &format, &nitems, &bytesafter,
				  &data)
	      == Success
	      && type != None)
	    active_p = True;

	  if (data) free (data);
	  data = 0;

	  if (XGetWindowProperty (dpy, window,
				  XA_SCREENSAVER_TIME,
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
		fprintf (stdout, ": screen blanked since ");
	      else
		/* suggestions for a better way to phrase this are welcome. */
		fprintf (stdout, ": screen non-blanked since ");
	      fprintf (stdout, "%s", ctime(&tt));
	      if (data) free (data);
	    }
	  else
	    {
	      if (data) free (data);
	      fprintf (stdout, "\n");
	      fflush (stdout);
	      fprintf (stderr, "%s: no time on window 0x%x (%s %s).\n",
		       progname, (unsigned int) window,
		       hint.res_class, (v ? v : "???"));
	      exit (1);
	    }
	}
    }
  else
    {
      XEvent event;
      event.xany.type = ClientMessage;
      event.xclient.display = dpy;
      event.xclient.window = window;
      event.xclient.message_type = XA_SCREENSAVER;
      event.xclient.format = 32;
      event.xclient.data.l[0] = (long) command;
      if (! XSendEvent (dpy, window, False, 0L, &event))
	{
	  fprintf (stderr, "%s: XSendEvent(dpy, 0x%x ...) failed.\n",
		   progname, (unsigned int) window);
	  exit (1);
	}
    }
  XSync (dpy, 0);
}



#ifdef STANDALONE
static Atom XA_ACTIVATE, XA_DEACTIVATE, XA_CYCLE, XA_NEXT, XA_PREV;
static Atom XA_EXIT, XA_RESTART, XA_DEMO, XA_LOCK;

static char *progname;
static char *screensaver_version;
static char *usage = "\n\
usage: %s -<switch>\n\
\n\
  This program provides external control of a running xscreensaver process.\n\
  Version %s, copyright (c) 1991-1997 Jamie Zawinski <jwz@netscape.com>.\n\
\n\
  The xscreensaver program is a daemon that runs in the background.\n\
  You control a running xscreensaver process by sending it messages\n\
  with this program, xscreensaver-command.  See the man pages for\n\
  details.  These are the arguments understood by xscreensaver-command:\n\
\n\
  -demo         Ask the xscreensaver process to enter interactive demo mode.\n\
\n\
  -activate     Turn on the screensaver (blank the screen), as if the user\n\
                had been idle for long enough.\n\
\n\
  -deactivate   Turns off the screensaver (un-blank the screen), as if user\n\
                activity had been detected.\n\
\n\
  -cycle        If the screensaver is active (the screen is blanked), then\n\
                stop the current graphics demo and run a new one (chosen\n\
                randomly.)\n\
\n\
  -next         Like either -activate or -cycle, depending on which is more\n\
                appropriate, except that the graphics hack that will be run\n\
                is the next one in the list, instead of a randomly-chosen\n\
                one.  In other words, repeatedly executing -next will cause\n\
                the xscreensaver process to invoke each graphics demo\n\
                sequentially.  (Though using the -demo option is probably\n\
                an easier way to accomplish that.)\n\
\n\
  -prev         Like -next, but goes in the other direction.\n\
\n\
  -exit         Causes the xscreensaver process to exit gracefully.  This is\n\
                roughly the same as killing the process with `kill', but it\n\
                is easier, since you don't need to first figure out the pid.\n\
                (Note that one must *never* kill xscreensaver with -9!)\n\
\n\
  -restart      Causes the screensaver process to exit and then restart with\n\
                the same command line arguments as last time.  Do this after\n\
                you've changed your X resource settings, to cause\n\
                xscreensaver to notice the changes.\n\
\n\
  -lock         Tells the running xscreensaver process to lock the screen\n\
                immediately.  This is like -activate, but forces locking as\n\
                well, even if locking is not the default.\n\
\n\
  -version      Prints the version of xscreensaver that is currently running\n\
                on the display -- that is, the actual version number of the\n\
                running xscreensaver background process, rather than the\n\
                version number of xscreensaver-command.\n\
\n\
  -time         Prints the time at which the screensaver last activated or\n\
                deactivated (roughly, how long the user has been idle or\n\
                non-idle -- but not quite, since it only tells you when the\n\
                screen became blanked or un-blanked.)\n\
\n\
  See the man page for more details.\n\
  For updates, check http://people.netscape.com/jwz/xscreensaver/\n\
\n";

#define USAGE() \
 { fprintf (stderr, usage, progname, screensaver_version); exit (1); }

int
main (int argc, char **argv)
{
  Display *dpy;
  int i;
  char *dpyname = 0;
  Atom *cmd = 0;

  progname = argv[0];
  screensaver_version = (char *) malloc (5);
  memcpy (screensaver_version, screensaver_id + 17, 4);
  screensaver_version [4] = 0;

  for (i = 1; i < argc; i++)
    {
      const char *s = argv [i];
      int L;
      if (s[0] == '-' && s[1] == '-') s++;
      L = strlen (s);
      if (L < 2) USAGE ();
      if (!strncmp (s, "-display", L))		dpyname = argv [++i];
      else if (cmd) USAGE ()
      else if (!strncmp (s, "-activate", L))   cmd = &XA_ACTIVATE;
      else if (!strncmp (s, "-deactivate", L)) cmd = &XA_DEACTIVATE;
      else if (!strncmp (s, "-cycle", L))      cmd = &XA_CYCLE;
      else if (!strncmp (s, "-next", L))       cmd = &XA_NEXT;
      else if (!strncmp (s, "-prev", L))       cmd = &XA_PREV;
      else if (!strncmp (s, "-exit", L))       cmd = &XA_EXIT;
      else if (!strncmp (s, "-restart", L))    cmd = &XA_RESTART;
      else if (!strncmp (s, "-demo", L))       cmd = &XA_DEMO;
      else if (!strncmp (s, "-lock", L))       cmd = &XA_LOCK;
      else if (!strncmp (s, "-version", L))    cmd = &XA_SCREENSAVER_VERSION;
      else if (!strncmp (s, "-time", L))       cmd = &XA_SCREENSAVER_TIME;
      else USAGE ();
    }
  if (!cmd)
    USAGE ();

  if (!dpyname) dpyname = (char *) getenv ("DISPLAY");
  dpy = XOpenDisplay (dpyname);
  if (!dpy)
    {
      fprintf (stderr, "%s: can't open display %s\n", progname,
	       (dpyname ? dpyname : "(null)"));
      exit (1);
    }

  XA_VROOT = XInternAtom (dpy, "__SWM_VROOT", False);
  XA_SCREENSAVER = XInternAtom (dpy, "SCREENSAVER", False);
  XA_SCREENSAVER_VERSION = XInternAtom (dpy, "_SCREENSAVER_VERSION",False);
  XA_SCREENSAVER_TIME = XInternAtom (dpy, "_SCREENSAVER_TIME", False);
  XA_ACTIVATE = XInternAtom (dpy, "ACTIVATE", False);
  XA_DEACTIVATE = XInternAtom (dpy, "DEACTIVATE", False);
  XA_RESTART = XInternAtom (dpy, "RESTART", False);
  XA_CYCLE = XInternAtom (dpy, "CYCLE", False);
  XA_NEXT = XInternAtom (dpy, "NEXT", False);
  XA_PREV = XInternAtom (dpy, "PREV", False);
  XA_EXIT = XInternAtom (dpy, "EXIT", False);
  XA_DEMO = XInternAtom (dpy, "DEMO", False);
  XA_LOCK = XInternAtom (dpy, "LOCK", False);

  xscreensaver_command(dpy, *cmd);

  fflush (stdout);
  fflush (stderr);
  exit (0);
}

#endif /* STANDALONE */
