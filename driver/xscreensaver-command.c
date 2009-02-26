/* xscreensaver-command, Copyright (c) 1991-1998
 *  by Jamie Zawinski <jwz@jwz.org>
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
#include <sys/time.h>
#include <sys/types.h>

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
  static Atom XA_SCREENSAVER, XA_SCREENSAVER_RESPONSE, XA_SCREENSAVER_VERSION;
  static Atom XA_SCREENSAVER_TIME, XA_SELECT;
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
  return 0;
}


static int
send_xscreensaver_command (Display *dpy, Atom command, long argument,
			   Window *window_ret)
{
  char *v = 0;
  Window window = find_screensaver_window (dpy, &v);
  XWindowAttributes xgwa;

  if (window_ret)
    *window_ret = window;

  if (!window)
    return -1;

  /* Select for property change events, so that we can read the response. */
  XGetWindowAttributes (dpy, window, &xgwa);
  XSelectInput (dpy, window, xgwa.your_event_mask | PropertyChangeMask);

  if (command == XA_SCREENSAVER_TIME ||
      command == XA_SCREENSAVER_VERSION)
    {
      XClassHint hint;
      memset (&hint, 0, sizeof(hint));
      if (!v || !*v)
	{
	  fprintf (stderr, "%s: version property not set on window 0x%x?\n",
		   progname, (unsigned int) window);
	  return -1;
	}

      XGetClassHint(dpy, window, &hint);
      if (!hint.res_class)
	{
	  fprintf (stderr, "%s: class hints not set on window 0x%x?\n",
		   progname, (unsigned int) window);
	  return -1;
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
	  Bool active_p = False;

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
	      return -1;
	    }
	}

      /* No need to read a response for these commands. */
      return 1;
    }
  else
    {
      XEvent event;
      long arg1 = (command == XA_SELECT ? argument : 0L);
      event.xany.type = ClientMessage;
      event.xclient.display = dpy;
      event.xclient.window = window;
      event.xclient.message_type = XA_SCREENSAVER;
      event.xclient.format = 32;
      memset (&event.xclient.data, 0, sizeof(event.xclient.data));
      event.xclient.data.l[0] = (long) command;
      event.xclient.data.l[1] = arg1;
      if (! XSendEvent (dpy, window, False, 0L, &event))
	{
	  fprintf (stderr, "%s: XSendEvent(dpy, 0x%x ...) failed.\n",
		   progname, (unsigned int) window);
	  return -1;
	}
    }
  XSync (dpy, 0);
  return 0;
}


static XErrorHandler old_handler = 0;
static Bool got_badwindow = False;
static int
BadWindow_ehandler (Display *dpy, XErrorEvent *error)
{
  /* When we notice a window being created, we spawn a timer that waits
     30 seconds or so, and then selects events on that window.  This error
     handler is used so that we can cope with the fact that the window
     may have been destroyed <30 seconds after it was created.
   */
  if (error->error_code == BadWindow)
    {
      got_badwindow = True;
      return 0;
    }
  else
    {
      fprintf (stderr, "%s: ", progname);
      return (*old_handler) (dpy, error);
    }
}


static int
xscreensaver_command_response (Display *dpy, Window window)
{
  int fd = ConnectionNumber (dpy);
  int timeout = 10;
  int status;
  fd_set fds;
  struct timeval tv;

  while (1)
    {
      FD_ZERO(&fds);
      FD_SET(fd, &fds);
      memset(&tv, 0, sizeof(tv));
      tv.tv_sec = timeout;
      status = select (fd+1, &fds, 0, &fds, &tv);

      if (status < 0)
	{
	  char buf[1024];
	  sprintf (buf, "%s: waiting for reply", progname);
	  perror (buf);
	  return status;
	}
      else if (status == 0)
	{
	  fprintf (stderr, "%s: no response to command.\n", progname);
	  return -1;
	}
      else
	{
	  XEvent event;
	  XNextEvent (dpy, &event);
	  switch (event.xany.type) {
	  case PropertyNotify:
	    if (event.xproperty.state == PropertyNewValue &&
		event.xproperty.atom == XA_SCREENSAVER_RESPONSE)
	      {
		Status st2;
		Atom type;
		int format;
		unsigned long nitems, bytesafter;
		char *msg = 0;

		old_handler = XSetErrorHandler (BadWindow_ehandler);
		XSync (dpy, False);

		st2 = XGetWindowProperty (dpy, window,
					  XA_SCREENSAVER_RESPONSE,
					  0, 1024, True,
					  AnyPropertyType,
					  &type, &format, &nitems, &bytesafter,
					  (unsigned char **) &msg);

		if (got_badwindow)
		  {
		    fprintf (stdout,
			     "%s: xscreensaver window has been deleted.\n",
			     progname);
		    return 0;
		  }

		if (st2 == Success && type != None)
		  {
		    if (type != XA_STRING || format != 8)
		      {
			fprintf (stderr,
				 "%s: unrecognized response property.\n",
				 progname);
			if (msg) XFree (msg);
			return -1;
		      }
		    else if (!msg || (msg[0] != '+' && msg[0] != '-'))
		      {
			fprintf (stderr,
				 "%s: unrecognized response message.\n",
				 progname);
			if (msg) XFree (msg);
			return -1;
		      }
		    else
		      {
			int ret = (msg[0] == '+' ? 0 : -1);
			fprintf ((ret < 0 ? stderr : stdout),
				 "%s: %s\n",
				 progname,
				 msg+1);
			XFree (msg);
			return ret;
		      }
		  }
	      }
	    break;

	  default:
	    fprintf (stderr, "%s: got unexpected response event %d.\n",
		     progname, event.xany.type);
	    return -1;
	    break;
	  }
	}
    }
}


int
xscreensaver_command (Display *dpy, Atom command, long argument)
{
  Window w = 0;
  int status = send_xscreensaver_command (dpy, command, argument, &w);
  if (status == 0)
    status = xscreensaver_command_response (dpy, w);
  fflush (stdout);
  fflush (stderr);
  return status;
}


#ifdef STANDALONE
static Atom XA_ACTIVATE, XA_DEACTIVATE, XA_CYCLE, XA_NEXT, XA_PREV;
static Atom XA_EXIT, XA_RESTART, XA_DEMO, XA_PREFS, XA_LOCK;

static char *progname;
static char *screensaver_version;
static char *usage = "\n\
usage: %s -<option>\n\
\n\
  This program provides external control of a running xscreensaver process.\n\
  Version %s, copyright (c) 1991-1998 Jamie Zawinski <jwz@jwz.org>.\n\
\n\
  The xscreensaver program is a daemon that runs in the background.\n\
  You control a running xscreensaver process by sending it messages\n\
  with this program, xscreensaver-command.  See the man pages for\n\
  details.  These are the arguments understood by xscreensaver-command:\n\
\n\
  -demo         Ask the xscreensaver process to enter interactive demo mode.\n\
\n\
  -prefs        Ask the xscreensaver process to bring up the preferences\n\
                panel.\n\
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
  -select <N>   Like -activate, but runs the Nth element in the list of\n\
                hacks.  By knowing what is in the `programs' list, and in\n\
                what order, you can use this to activate the screensaver\n\
                with a particular graphics demo.  (The first element in the\n\
                list is numbered 1, not 0.)\n\
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
  For updates, check http://www.jwz.org/xscreensaver/\n\
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
  long arg = 0L;

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
      else if (!strncmp (s, "-select", L))     cmd = &XA_SELECT;
      else if (!strncmp (s, "-exit", L))       cmd = &XA_EXIT;
      else if (!strncmp (s, "-restart", L))    cmd = &XA_RESTART;
      else if (!strncmp (s, "-demo", L))       cmd = &XA_DEMO;
      else if (!strncmp (s, "-preferences",L)) cmd = &XA_PREFS;
      else if (!strncmp (s, "-prefs",L))       cmd = &XA_PREFS;
      else if (!strncmp (s, "-lock", L))       cmd = &XA_LOCK;
      else if (!strncmp (s, "-version", L))    cmd = &XA_SCREENSAVER_VERSION;
      else if (!strncmp (s, "-time", L))       cmd = &XA_SCREENSAVER_TIME;
      else USAGE ();

      if (cmd == &XA_SELECT)
	{
	  char junk;
	  i++;
	  if (i >= argc ||
	      (1 != sscanf(argv[i], " %ld %c", &arg, &junk)))
	    USAGE ();
	}
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
  XA_SCREENSAVER_RESPONSE = XInternAtom (dpy, "_SCREENSAVER_RESPONSE", False);
  XA_ACTIVATE = XInternAtom (dpy, "ACTIVATE", False);
  XA_DEACTIVATE = XInternAtom (dpy, "DEACTIVATE", False);
  XA_RESTART = XInternAtom (dpy, "RESTART", False);
  XA_CYCLE = XInternAtom (dpy, "CYCLE", False);
  XA_NEXT = XInternAtom (dpy, "NEXT", False);
  XA_PREV = XInternAtom (dpy, "PREV", False);
  XA_SELECT = XInternAtom (dpy, "SELECT", False);
  XA_EXIT = XInternAtom (dpy, "EXIT", False);
  XA_DEMO = XInternAtom (dpy, "DEMO", False);
  XA_PREFS = XInternAtom (dpy, "PREFS", False);
  XA_LOCK = XInternAtom (dpy, "LOCK", False);

  XSync (dpy, 0);

  if (*cmd == XA_ACTIVATE || *cmd == XA_LOCK ||
      *cmd == XA_NEXT || *cmd == XA_PREV || *cmd == XA_SELECT)
    /* People never guess that KeyRelease deactivates the screen saver too,
       so if we're issuing an activation command, wait a second. */
    sleep (1);

  i = xscreensaver_command (dpy, *cmd, arg);
  if (i < 0) exit (i);
  else exit (0);
}

#endif /* STANDALONE */
