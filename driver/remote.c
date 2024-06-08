/* xscreensaver-command, Copyright © 1991-2024 Jamie Zawinski <jwz@jwz.org>
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

#ifdef HAVE_DPMS_EXTENSION
# include <X11/extensions/dpms.h>
#endif

#include "blurb.h"
#include "atoms.h"
#include "remote.h"
#include "clientmsg.h"

#ifdef _VROOT_H_
ERROR! you must not include vroot.h in this file
#endif

static Bool error_handler_hit_p = False;
static int
ignore_all_errors_ehandler (Display *dpy, XErrorEvent *error)
{
  error_handler_hit_p = True;
  return 0;
}


/* See comment in xscreensaver.c for why this is here instead of there.
 */
static void
reset_dpms_timer (Display *dpy)
{
# ifdef HAVE_DPMS_EXTENSION

  XErrorHandler old_handler;
  int event_number, error_number;
  BOOL enabled = False;
  CARD16 power = 0;

  XSync (dpy, False);
  error_handler_hit_p = False;
  old_handler = XSetErrorHandler (ignore_all_errors_ehandler);

  if (! DPMSQueryExtension (dpy, &event_number, &error_number))
    goto DONE;
  if (! DPMSCapable (dpy))
    goto DONE;
  if (! DPMSInfo (dpy, &power, &enabled))
    goto DONE;
  if (!enabled)
    goto DONE;

  DPMSForceLevel (dpy, DPMSModeOn);

  /* We want to reset the timer inside the X server.  DPMSForceLevel doesn't
     do that if the monitor was already powered on: it keeps counting down.
     But disabling and re-enabling seems to do the trick. */
  DPMSDisable (dpy);
  DPMSEnable (dpy);

 DONE:
  XSync (dpy, False);
  XSetErrorHandler (old_handler);

# endif /* HAVE_DPMS_EXTENSION */
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
                  fprintf (stderr, "bad status format on root window\n");
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
	      fprintf (stderr, "no saver status on root window\n");
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
	  arg1 = 5000;	/* version number of the XA_DEMO protocol, */
	  arg2 = arg;	/* since it didn't use to take an argument. */
	}

      if (command == XA_DEACTIVATE)
        reset_dpms_timer (dpy);

      event.xany.type = ClientMessage;
      event.xclient.display = dpy;
      event.xclient.window = window;
      event.xclient.message_type = XA_SCREENSAVER;
      event.xclient.format = 32;
      memset (&event.xclient.data, 0, sizeof(event.xclient.data));
      event.xclient.data.l[0] = (long) command;
      event.xclient.data.l[1] = arg1;
      event.xclient.data.l[2] = arg2;

      if (! XSendEvent (dpy, window, False, PropertyChangeMask, &event))
	{
	  sprintf (err, "XSendEvent(dpy, 0x%x ...) failed\n",
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


static Bool
xscreensaver_command_event_p (Display *dpy, XEvent *event, XPointer arg)
{
  return (event->xany.type == PropertyNotify &&
          event->xproperty.state == PropertyNewValue &&
          event->xproperty.atom == XA_SCREENSAVER_RESPONSE);
}


static int
xscreensaver_command_response (Display *dpy, Window window,
                               Bool verbose_p, Bool exiting_p,
                               char **error_ret)
{
  int sleep_count = 0;
  char err[2048];
  XEvent event;
  Bool got_event = False;

  while (!(got_event = XCheckIfEvent(dpy, &event,
				     &xscreensaver_command_event_p, 0)) &&
	 sleep_count++ < 10)
    {
# if defined(HAVE_SELECT)
      /* Wait for an event, but don't wait longer than 1 sec.  Note that we
         might do this multiple times if an event comes in, but it wasn't
         the event we're waiting for.
       */
      int fd = XConnectionNumber(dpy);
      fd_set rset;
      struct timeval tv;
      tv.tv_sec  = 1;
      tv.tv_usec = 0;
      FD_ZERO (&rset);
      FD_SET (fd, &rset);
      select (fd+1, &rset, 0, 0, &tv);
# else  /* !HAVE_SELECT */
      sleep(1);
# endif /* !HAVE_SELECT */
    }

  if (!got_event)
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
      Status st2;
      Atom type;
      int format;
      unsigned long nitems, bytesafter;
      unsigned char *msg = 0;
      XErrorHandler old_handler;

      XSync (dpy, False);
      error_handler_hit_p = False;
      old_handler = XSetErrorHandler (ignore_all_errors_ehandler);
      st2 = XGetWindowProperty (dpy, window,
				XA_SCREENSAVER_RESPONSE,
				0, 1024, True,
				AnyPropertyType,
				&type, &format, &nitems, &bytesafter,
				&msg);
      XSync (dpy, False);
      XSetErrorHandler (old_handler);

      if (error_handler_hit_p)
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

  return -1;  /* warning suppression: not actually reached */
}


/* Wait until xscreensaver says the screen is blanked.
   Catches errors, times out after a few seconds.
 */
static int
xscreensaver_command_wait_for_blank (Display *dpy, 
                                     Bool verbose_p, char **error_ret)
{
  Window w = RootWindow (dpy, 0);  /* always screen 0 */
  time_t start = time((time_t*)0);
  int max = 10;
  char err[2048];
  while (1)
    {
      Atom type;
      int format;
      unsigned long nitems, bytesafter;
      unsigned char *dataP = 0;
      time_t now;
      struct timeval tv;

      /* Wait until the status property on the root window changes to
         BLANK or LOCKED. */
      if (XGetWindowProperty (dpy, w,
                              XA_SCREENSAVER_STATUS,
                              0, 999, False, XA_INTEGER,
                              &type, &format, &nitems, &bytesafter,
                              &dataP)
          == Success
          && type == XA_INTEGER
          && nitems >= 3
          && dataP)
        {
          Atom state = ((Atom *) dataP)[0];

          if (verbose_p > 1)
            {
              PROP32 *status = (PROP32 *) dataP;
              int i;
              fprintf (stderr, "%s: read status property: 0x%lx: %s", progname,
                       (unsigned long) w,
                       (status[0] == XA_LOCK  ? "LOCK" :
                        status[0] == XA_BLANK ? "BLANK" :
                        status[0] == 0 ? "0" : "???"));
              for (i = 1; i < nitems; i++)
                fprintf (stderr, ", %lu", status[i]);
              fprintf (stderr, "\n");
            }

          if (state == XA_BLANK || state == XA_LOCK)
            {
              if (verbose_p > 1)
                fprintf (stderr, "%s: screen blanked\n", progname);
              break;
            }
        }

      now = time ((time_t *) 0);
      if (now >= start + max)
        {
          strcpy (err, "Timed out waiting for screen to blank");
          if (error_ret)
            *error_ret = strdup (err);
          else
            fprintf (stderr, "%s: %s\n", progname, err);
          return -1;
        }
      else if (verbose_p == 1 && now > start + 3)
        {
          fprintf (stderr, "%s: waiting for status change\n", progname);
          verbose_p++;
        }

      tv.tv_sec  = 0;
      tv.tv_usec = 1000000L / 10;
      select (0, 0, 0, 0, &tv);
    }

  return 0;
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

  /* If this command should result in the screen being blank, wait until
     the xscreensaver window is mapped before returning. */
  if (status == 0 &&
      (command == XA_ACTIVATE ||
       command == XA_SUSPEND ||
       command == XA_LOCK ||
       command == XA_NEXT ||
       command == XA_PREV ||
       command == XA_SELECT))
    status = xscreensaver_command_wait_for_blank (dpy, verbose_p, error_ret);

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
      XGetWindowProperty (dpy, window, XA_SCREENSAVER_VERSION, 0, 100,
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
	      if (o) p = strrchr (o, '@');
	      if (p) c = strchr (p, ')');
	      if (c)
		{
		  /* found ID of the form "1234 (user@host)"
                     or the weirder "1234 (user@crap@host)". */
		  user = o+1;
		  host = p+1;
		  *p = 0;
		  *c = 0;
		}
	    }

	}

      if (!user_ret)
        ;
      else if (user && *user && *user != '?')
	*user_ret = strdup (user);
      else
	*user_ret = 0;

      if (!host_ret)
        ;
      else if (host && *host && *host != '?')
	*host_ret = strdup (host);
      else
	*host_ret = 0;

      if (id)
	XFree (id);
    }
}
