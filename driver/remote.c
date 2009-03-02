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
#include <time.h>
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
extern Atom XA_SCREENSAVER_ID, XA_SCREENSAVER_STATUS, XA_EXIT;
extern Atom XA_VROOT, XA_SELECT, XA_DEMO, XA_BLANK, XA_LOCK;


static XErrorHandler old_handler = 0;
static Bool got_badwindow = False;
static int
BadWindow_ehandler (Display *dpy, XErrorEvent *error)
{
  if (error->error_code == BadWindow)
    {
      got_badwindow = True;
      return 0;
    }
  else
    {
      fprintf (stderr, "%s: ", progname);
      if (!old_handler) abort();
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
    return 0;
  for (i = 0; i < nkids; i++)
    {
      Atom type;
      int format;
      unsigned long nitems, bytesafter;
      unsigned char *v;
      int status;

      /* We're walking the list of root-level windows and trying to find
         the one that has a particular property on it.  We need to trap
         BadWindows errors while doing this, because it's possible that
         some random window might get deleted in the meantime.  (That
         window won't have been the one we're looking for.)
       */
      XSync (dpy, False);
      if (old_handler) abort();
      got_badwindow = False;
      old_handler = XSetErrorHandler (BadWindow_ehandler);
      status = XGetWindowProperty (dpy, kids[i],
                                   XA_SCREENSAVER_VERSION,
                                   0, 200, False, XA_STRING,
                                   &type, &format, &nitems, &bytesafter,
                                   &v);
      XSync (dpy, False);
      XSetErrorHandler (old_handler);
      old_handler = 0;

      if (got_badwindow)
        {
          status = BadWindow;
          got_badwindow = False;
        }

      if (status == Success && type != None)
	{
          Window ret = kids[i];
	  if (version)
	    *version = (char *) v;
          XFree (kids);
	  return ret;
	}
    }

  if (kids) XFree (kids);
  return 0;
}


static int
send_xscreensaver_command (Display *dpy, Atom command, long arg,
			   Window *window_ret, char **error_ret)
{
  int status = -1;
  char *v = 0;
  Window window = find_screensaver_window (dpy, &v);
  XWindowAttributes xgwa;
  char err[2048];

  if (window_ret)
    *window_ret = window;

  if (!window)
    {
      sprintf (err, "no screensaver is running on display %s",
               DisplayString (dpy));

      if (error_ret)
        {
          *error_ret = strdup (err);
          status = -1;
          goto DONE;
        }

      if (command == XA_EXIT)
        {
          /* Don't print an error if xscreensaver is already dead. */
          status = 1;
          goto DONE;
        }

      fprintf (stderr, "%s: %s\n", progname, err);
      status = -1;
      goto DONE;
    }

  /* Select for property change events, so that we can read the response. */
  XGetWindowAttributes (dpy, window, &xgwa);
  XSelectInput (dpy, window, xgwa.your_event_mask | PropertyChangeMask);

  if (command == XA_SCREENSAVER_STATUS ||
      command == XA_SCREENSAVER_VERSION)
    {
      XClassHint hint;
      memset (&hint, 0, sizeof(hint));
      if (!v || !*v)
	{
	  sprintf (err, "version property not set on window 0x%x?",
		   (unsigned int) window);
          if (error_ret)
            *error_ret = strdup (err);
          else
            fprintf (stderr, "%s: %s\n", progname, err);

          status = -1;
          goto DONE;
	}

      XGetClassHint(dpy, window, &hint);
      if (!hint.res_class)
	{
	  sprintf (err, "class hints not set on window 0x%x?",
		   (unsigned int) window);
          if (error_ret)
            *error_ret = strdup (err);
          else
            fprintf (stderr, "%s: %s\n", progname, err);

          if (v) free (v);
          status = -1;
          goto DONE;
	}

      fprintf (stdout, "%s %s", hint.res_class, v);

      if (command != XA_SCREENSAVER_STATUS)
	{
	  fprintf (stdout, "\n");
	}
      else
	{
	  Atom type;
	  int format;
	  unsigned long nitems, bytesafter;
	  unsigned char *dataP = 0;

	  if (XGetWindowProperty (dpy,
                                  RootWindow (dpy, 0),
				  XA_SCREENSAVER_STATUS,
				  0, 999, False, XA_INTEGER,
				  &type, &format, &nitems, &bytesafter,
				  &dataP)
	      == Success
	      && type
	      && dataP)
	    {
              Atom blanked;
              time_t tt;
              char *s;
              Atom *data = (Atom *) dataP;

              if (type != XA_INTEGER || nitems < 3)
                {
                STATUS_LOSE:
                  if (data) free (data);
                  fprintf (stdout, "\n");
                  fflush (stdout);
                  fprintf (stderr, "bad status format on root window.\n");
                  status = -1;
                  goto DONE;
                }
                  
              blanked = (Atom) data[0];
              tt = (time_t) data[1];

              if (tt <= (time_t) 666000000L) /* early 1991 */
                goto STATUS_LOSE;

	      if (blanked == XA_BLANK)
		fputs (": screen blanked since ", stdout);
	      else if (blanked == XA_LOCK)
		fputs (": screen locked since ", stdout);
	      else if (blanked == 0)
		/* suggestions for a better way to phrase this are welcome. */
		fputs (": screen non-blanked since ", stdout);
              else
                /* `blanked' has an unknown value - fail. */
                goto STATUS_LOSE;

              s = ctime(&tt);
              if (s[strlen(s)-1] == '\n')
                s[strlen(s)-1] = 0;
              fputs (s, stdout);

              {
                int nhacks = nitems - 2;
                Bool any = False;
                int i;
                for (i = 0; i < nhacks; i++)
                  if (data[i + 2] > 0)
                    {
                      any = True;
                      break;
                    }

                if (any && nhacks == 1)
                  fprintf (stdout, " (hack #%d)\n", (int) data[2]);
                else if (any)
                  {
                    fprintf (stdout, " (hacks: ");
                    for (i = 0; i < nhacks; i++)
                      {
                        fprintf (stdout, "#%d", (int) data[2 + i]);
                        if (i != nhacks-1)
                          fputs (", ", stdout);
                      }
                    fputs (")\n", stdout);
                  }
                else
                  fputs ("\n", stdout);
              }

	      if (data) free (data);
	    }
	  else
	    {
	      if (dataP) XFree (dataP);
	      fprintf (stdout, "\n");
	      fflush (stdout);
	      fprintf (stderr, "no saver status on root window.\n");
              status = -1;
              goto DONE;
	    }
	}

      /* No need to read a response for these commands. */
      status = 1;
      goto DONE;
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
	  sprintf (err, "XSendEvent(dpy, 0x%x ...) failed.\n",
		   (unsigned int) window);
          if (error_ret)
            *error_ret = strdup (err);
          else
            fprintf (stderr, "%s: %s\n", progname, err);
          status = -1;
          goto DONE;
	}
    }

  status = 0;

 DONE:
  if (v) free (v);
  XSync (dpy, 0);
  return status;
}


static int
xscreensaver_command_response (Display *dpy, Window window,
                               Bool verbose_p, Bool exiting_p,
                               char **error_ret)
{
  int fd = ConnectionNumber (dpy);
  int timeout = 10;
  int status;
  fd_set fds;
  struct timeval tv;
  char err[2048];

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
          if (error_ret)
            {
              sprintf (buf, "error waiting for reply");
              *error_ret = strdup (buf);
            }
          else
            {
              sprintf (buf, "%s: error waiting for reply", progname);
              perror (buf);
            }
	  return status;
	}
      else if (status == 0)
	{
	  sprintf (err, "no response to command.");
          if (error_ret)
            *error_ret = strdup (err);
          else
            fprintf (stderr, "%s: %s\n", progname, err);
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
	      unsigned char *msg = 0;

	      XSync (dpy, False);
              if (old_handler) abort();
	      old_handler = XSetErrorHandler (BadWindow_ehandler);
	      st2 = XGetWindowProperty (dpy, window,
					XA_SCREENSAVER_RESPONSE,
					0, 1024, True,
					AnyPropertyType,
					&type, &format, &nitems, &bytesafter,
					&msg);
	      XSync (dpy, False);
              XSetErrorHandler (old_handler);
              old_handler = 0;

	      if (got_badwindow)
		{
                  if (exiting_p)
                    return 0;

                  sprintf (err, "xscreensaver window unexpectedly deleted.");

                  if (error_ret)
                    *error_ret = strdup (err);
                  else
                    fprintf (stderr, "%s: %s\n", progname, err);

		  return -1;
		}

	      if (st2 == Success && type != None)
		{
		  if (type != XA_STRING || format != 8)
		    {
		      sprintf (err, "unrecognized response property.");

                      if (error_ret)
                        *error_ret = strdup (err);
                      else
                        fprintf (stderr, "%s: %s\n", progname, err);

		      if (msg) XFree (msg);
		      return -1;
		    }
		  else if (!msg || (msg[0] != '+' && msg[0] != '-'))
		    {
		      sprintf (err, "unrecognized response message.");

                      if (error_ret)
                        *error_ret = strdup (err);
                      else  
                        fprintf (stderr, "%s: %s\n", progname, err);

		      if (msg) XFree (msg);
		      return -1;
		    }
		  else
		    {
		      int ret = (msg[0] == '+' ? 0 : -1);
                      sprintf (err, "%s: %s\n", progname, (char *) msg+1);

                      if (error_ret)
                        *error_ret = strdup (err);
                      else if (verbose_p || ret != 0)
			fprintf ((ret < 0 ? stderr : stdout), "%s\n", err);

		      XFree (msg);
		      return ret;
		    }
		}
	    }
	}
    }
}


int
xscreensaver_command (Display *dpy, Atom command, long arg, Bool verbose_p,
                      char **error_ret)
{
  Window w = 0;
  int status = send_xscreensaver_command (dpy, command, arg, &w, error_ret);
  if (status == 0)
    status = xscreensaver_command_response (dpy, w, verbose_p,
                                            (command == XA_EXIT),
                                            error_ret);

  fflush (stdout);
  fflush (stderr);
  return (status < 0 ? status : 0);
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
  if (user_ret)
    *user_ret = 0;
  if (host_ret)
    *host_ret = 0;

  if (!window)
    return;

  if (version_ret)
    {
      unsigned char *v = 0;
      XGetWindowProperty (dpy, window, XA_SCREENSAVER_VERSION, 0, 1,
			  False, XA_STRING, &type, &format, &nitems,
			  &bytesafter, &v);
      if (v)
	{
	  *version_ret = strdup ((char *) v);
	  XFree (v);
	}
    }

  if (user_ret || host_ret)
    {
      unsigned char *id = 0;
      const char *user = 0;
      const char *host = 0;

      XGetWindowProperty (dpy, window, XA_SCREENSAVER_ID, 0, 512,
			  False, XA_STRING, &type, &format, &nitems,
			  &bytesafter, &id);
      if (id && *id)
	{
	  const char *old_tag = " on host ";
	  const char *s = strstr ((char *) id, old_tag);
	  if (s)
	    {
	      /* found ID of the form "1234 on host xyz". */
	      user = 0;
	      host = s + strlen (old_tag);
	    }
	  else
	    {
	      char *o = 0, *p = 0, *c = 0;
	      o = strchr ((char *) id, '(');
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
