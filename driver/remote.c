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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>

#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H */

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <X11/Xproto.h>		/* for CARD32 */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>		/* for XGetClassHint() */
#include <X11/Xos.h>

#include "remote.h"

#ifdef _VROOT_H_
ERROR! you must not include vroot.h in this file
#endif

extern char *progname;
extern Atom XA_SCREENSAVER, XA_SCREENSAVER_VERSION, XA_SCREENSAVER_RESPONSE;
extern Atom XA_SCREENSAVER_ID, XA_SCREENSAVER_TIME;
extern Atom XA_VROOT, XA_SELECT, XA_DEMO;


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
send_xscreensaver_command (Display *dpy, Atom command, long arg,
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
      long arg1 = arg;
      long arg2 = 0;

      if (arg < 0)
	abort();
      else if (arg == 0 && command == XA_SELECT)
	abort();
      else if (arg != 0 && command == XA_DEMO)
	{
	  arg1 = 300;	/* version number of the XA_DEMO protocol, */
	  arg2 = arg;	/* since it didn't use to take an argument. */
	}

      event.xany.type = ClientMessage;
      event.xclient.display = dpy;
      event.xclient.window = window;
      event.xclient.message_type = XA_SCREENSAVER;
      event.xclient.format = 32;
      memset (&event.xclient.data, 0, sizeof(event.xclient.data));
      event.xclient.data.l[0] = (long) command;
      event.xclient.data.l[1] = arg1;
      event.xclient.data.l[2] = arg2;
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


static int
xscreensaver_command_response (Display *dpy, Window window, Bool verbose_p)
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
	  if (event.xany.type == PropertyNotify &&
	      event.xproperty.state == PropertyNewValue &&
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
		      if (verbose_p || ret != 0)
			fprintf ((ret < 0 ? stderr : stdout),
				 "%s: %s\n",
				 progname,
				 msg+1);
		      XFree (msg);
		      return ret;
		    }
		}
	    }
	}
    }
}


int
xscreensaver_command (Display *dpy, Atom command, long arg, Bool verbose_p)
{
  Window w = 0;
  int status = send_xscreensaver_command (dpy, command, arg, &w);
  if (status == 0)
    status = xscreensaver_command_response (dpy, w, verbose_p);
  fflush (stdout);
  fflush (stderr);
  return status;
}


void
server_xscreensaver_version (Display *dpy,
			     char **version_ret,
			     char **user_ret,
			     char **host_ret)
{
  Window window = find_screensaver_window (dpy, 0);

  Atom type;
  int format;
  unsigned long nitems, bytesafter;

  if (version_ret)
    *version_ret = 0;
  if (host_ret)
    *host_ret = 0;

  if (!window)
    return;

  if (version_ret)
    {
      char *v = 0;
      XGetWindowProperty (dpy, window, XA_SCREENSAVER_VERSION, 0, 1,
			  False, XA_STRING, &type, &format, &nitems,
			  &bytesafter, (unsigned char **) &v);
      if (v)
	{
	  *version_ret = strdup (v);
	  XFree (v);
	}
    }

  if (user_ret || host_ret)
    {
      char *id = 0;
      const char *user = 0;
      const char *host = 0;

      XGetWindowProperty (dpy, window, XA_SCREENSAVER_ID, 0, 512,
			  False, XA_STRING, &type, &format, &nitems,
			  &bytesafter, (unsigned char **) &id);
      if (id && *id)
	{
	  const char *old_tag = " on host ";
	  const char *s = strstr (id, old_tag);
	  if (s)
	    {
	      /* found ID of the form "1234 on host xyz". */
	      user = 0;
	      host = s + strlen (old_tag);
	    }
	  else
	    {
	      char *o = 0, *p = 0, *c = 0;
	      o = strchr (id, '(');
	      if (o) p = strchr (o, '@');
	      if (p) c = strchr (p, ')');
	      if (c)
		{
		  /* found ID of the form "1234 (user@host)". */
		  user = o+1;
		  host = p+1;
		  *p = 0;
		  *c = 0;
		}
	    }

	}

      if (user && *user && *user != '?')
	*user_ret = strdup (user);
      else
	*user_ret = 0;

      if (host && *host && *host != '?')
	*host_ret = strdup (host);
      else
	*host_ret = 0;

      if (id)
	XFree (id);
    }
}
