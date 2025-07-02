/* xscreensaver, Copyright © 1991-2025 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * XScreenSaver Daemon, version 6.
 *
 * This is "xscreensaver-gfx" -- When the time comes for the screen to blank,
 * this process is launched by "xscreensaver" to fade out, black out every
 * monitor on the system, launch graphics demos to render on those blanked
 * screens, and cycle them from time to time.  See the comment atop
 * xscreensaver.c for details of the division of powers.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <ctype.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xatom.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xos.h>
#include <time.h>
#include <sys/time.h>
#include <netdb.h>	/* for gethostbyname() */
#include <sys/types.h>
#include <pwd.h>

#ifndef HAVE_XINPUT
# error The XInput2 extension is required
#endif

#include <X11/extensions/XInput2.h>

#ifdef HAVE_RANDR
# include <X11/extensions/Xrandr.h>
#endif /* HAVE_RANDR */

#ifdef ENABLE_NLS
# include <locale.h>
# include <libintl.h>
#endif

#include "xscreensaver.h"
#include "version.h"
#include "atoms.h"
#include "yarandom.h"
#include "resources.h"
#include "visual.h"
#include "screens.h"
#include "clientmsg.h"
#include "xmu.h"

saver_info *global_si_kludge = 0;	/* I hate C so much... */

char *progclass = 0;
XrmDatabase db = 0;
Bool debug_p = False;


#ifdef __GNUC__
 __extension__     /* shut up about "string length is greater than the length
                      ISO C89 compilers are required to support" when including
                      the .ad file... */
#endif

static char *defaults[] = {
#include "XScreenSaver_ad.h"
 0
};

#ifdef _VROOT_H_
ERROR!  You must not include vroot.h in this file.
#endif


void
maybe_reload_init_file (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  if (init_file_changed_p (p))
    {
      Bool ov = p->verbose_p;
      if (p->verbose_p)
	fprintf (stderr, "%s: file \"%s\" has changed, reloading\n",
		 blurb(), init_file_name());

      load_init_file (si->dpy, p);

      if (ov)
        p->verbose_p = True;

      /* If the DPMS settings in the init file have changed, change the
         settings on the server to match.  This also would have happened at
         the watchdog timer. */
      sync_server_dpms_settings (si->dpy, p);
    }
}


static int
saver_ehandler (Display *dpy, XErrorEvent *error)
{
  saver_info *si = global_si_kludge;	/* I hate C so much... */
  int i;

  fprintf (stderr, "\n"
	   "#######################################"
	   "#######################################\n\n"
	   "%s: X Error!  PLEASE REPORT THIS BUG.\n",
	   blurb());

  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      fprintf (stderr, "%s: screen %d/%d: 0x%x, 0x%x\n",
               blurb(), ssi->real_screen_number, ssi->number,
               (unsigned int) RootWindowOfScreen (si->screens[i].screen),
               (unsigned int) si->screens[i].screensaver_window);
    }

  fprintf (stderr, "\n"
	   "#######################################"
	   "#######################################\n\n");

  XmuPrintDefaultErrorMessage (dpy, error, stderr);
  exit (1);
}


static void
connect_to_server (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  Widget toplevel_shell;
  Window daemon_window;
  XrmOptionDescRec options;
  char *name;
  int ac = 1;
  char *av[] = { "xscreensaver" };    /* For Xt and Xrm purposes */

  /* Make sure the X11 socket doesn't get allocated to stderr: >&- 2>&-.
     Parent "xscreensaver" already did this, but let's make sure. */
  {
    int fd0 = open ("/dev/null", O_RDWR);
    int fd1 = open ("/dev/null", O_RDWR);
    int fd2 = open ("/dev/null", O_RDWR);
    if (fd0 > 2) close (fd0);
    if (fd1 > 2) close (fd1);
    if (fd2 > 2) close (fd2);
  }

  XSetErrorHandler (saver_ehandler);

  toplevel_shell = XtAppInitialize (&si->app, progclass, &options, 0,
				    &ac, av, defaults, 0, 0);

  si->dpy = XtDisplay (toplevel_shell);
  si->prefs.db = XtDatabase (si->dpy);
  XtGetApplicationNameAndClass (si->dpy, &name, &progclass);

  db = si->prefs.db;	/* resources.c needs this */

  init_xscreensaver_atoms (si->dpy);

  /* Select property changes on the window created by our parent xscreensaver
     process: this is where ClientMessage events are sent, and response
     properties are written.
   */
  daemon_window = find_screensaver_window (si->dpy, 0);
  if (daemon_window)
    {
      Window root = RootWindow (si->dpy, 0);  /* always screen 0 */
      XWindowAttributes xgwa;
      XGetWindowAttributes (si->dpy, daemon_window, &xgwa);
      XSelectInput (si->dpy, daemon_window,
                    xgwa.your_event_mask | PropertyChangeMask);

      /* For tracking changes to XA_SCREENSAVER_STATUS on the root. */
      XGetWindowAttributes (si->dpy, root, &xgwa);
      XSelectInput (si->dpy, root,
                    xgwa.your_event_mask | PropertyChangeMask);
    }
  else
    {
      if (p->verbose_p ||
          !(getenv ("WAYLAND_DISPLAY") || getenv ("WAYLAND_SOCKET")))
        fprintf (stderr, "%s: xscreensaver does not seem to be running!\n",
                 blurb());

      /* Under normal circumstances, that window should have been created
         by the "xscreensaver" process.  But if for some reason someone
         has run "xscreensaver-gfx" directly (Wayland?) we need this window
         to exist for ClientMessages to be receivable.  So let's make one.
       */
      XClassHint class_hints;
      XSetWindowAttributes attrs;
      unsigned long attrmask = 0;
      pid_t pid = getpid();
      char *id;
      const char *version_number = XSCREENSAVER_VERSION;

      class_hints.res_name  = (char *) progname;  /* not const? */
      class_hints.res_class = "XScreenSaver";
      id = (char *) malloc (20);
      sprintf (id, "%lu", (unsigned long) pid);
    
      attrmask = CWOverrideRedirect | CWEventMask;
      attrs.override_redirect = True;
      attrs.event_mask = PropertyChangeMask;

      daemon_window = XCreateWindow (si->dpy, RootWindow (si->dpy, 0),
                                     0, 0, 1, 1, 0,
                                     DefaultDepth (si->dpy, 0), InputOutput,
                                     DefaultVisual (si->dpy, 0), attrmask, &attrs);
      XStoreName (si->dpy, daemon_window, "XScreenSaver GFX");
      XSetClassHint (si->dpy, daemon_window, &class_hints);
      XChangeProperty (si->dpy, daemon_window, XA_WM_COMMAND, XA_STRING,
                       8, PropModeReplace, (unsigned char *) progname,
                       strlen (progname));
      XChangeProperty (si->dpy, daemon_window, XA_SCREENSAVER_VERSION, XA_STRING,
                       8, PropModeReplace, (unsigned char *) version_number,
                       strlen (version_number));
      XChangeProperty (si->dpy, daemon_window, XA_SCREENSAVER_ID, XA_STRING,
                       8, PropModeReplace, (unsigned char *) id, strlen (id));
      free (id);

      si->all_clientmessages_p = True;
    }
}


static void
initialize_randr (saver_info *si)
{
  saver_preferences *p = &si->prefs;

#ifdef HAVE_RANDR
  if (XRRQueryExtension (si->dpy,
                         &si->randr_event_number, &si->randr_error_number))
    {
      int nscreens = ScreenCount (si->dpy);  /* number of *real* screens */
      int i;

      si->using_randr_extension = TRUE;

      if (p->verbose_p)
	fprintf (stderr, "%s: selecting RANDR events\n", blurb());
      for (i = 0; i < nscreens; i++)
#  ifdef RRScreenChangeNotifyMask                 /* randr.h 1.5, 2002/09/29 */
        XRRSelectInput (si->dpy, RootWindow (si->dpy, i),
                        RRScreenChangeNotifyMask);
#  else  /* !RRScreenChangeNotifyMask */          /* Xrandr.h 1.4, 2001/06/07 */
        XRRScreenChangeSelectInput (si->dpy, RootWindow (si->dpy, i), True);
#  endif /* !RRScreenChangeNotifyMask */
    }
# endif /* HAVE_RANDR */
}


/* Read the XA_SCREENSAVER_STATUS propery from the root window of screen 0
   to see what hacks were selected the last time we ran, so that -next and
   -prev can work.
 */
static void
read_status_prop (saver_info *si)
{
  Window w = RootWindow (si->dpy, 0);  /* always screen 0 */
  Atom type;
  unsigned char *dataP = 0;
  int format, i;
  unsigned long nitems, bytesafter;

  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      ssi->current_hack = -1;
    }

  /* XA_SCREENSAVER_STATUS format documented in windows.c. */
  if (XGetWindowProperty (si->dpy, w,
                          XA_SCREENSAVER_STATUS,
                          0, 999, False, XA_INTEGER,
                          &type, &format, &nitems, &bytesafter,
                          &dataP)
      == Success
      && type == XA_INTEGER
      && nitems >= 3
      && dataP)
    {
      PROP32 *data = (PROP32 *) dataP;
      int off = 3;
      for (i = off; i < nitems; i++)
        {
          int j = i - off;
          if (j < si->nscreens)
            {
              saver_screen_info *ssi = &si->screens[j];
              int n = data[i];
              ssi->current_hack = n-1;  /* 1-based */
            }
        }
    }

  if (dataP)
    XFree (dataP);
}


/* When we get a PropertyChange event on XA_SCREENSAVER_STATUS, note
   whether the auth dialog is currently posted.
 */
static void
status_prop_changed (saver_info *si)
{
  Window w = RootWindow (si->dpy, 0);  /* always screen 0 */
  Atom type;
  unsigned char *dataP = 0;
  int format;
  unsigned long nitems, bytesafter;
  Bool oauth_p = si->auth_p;

  if (XGetWindowProperty (si->dpy, w,
                          XA_SCREENSAVER_STATUS,
                          0, 999, False, XA_INTEGER,
                          &type, &format, &nitems, &bytesafter,
                          &dataP)
      == Success
      && type == XA_INTEGER
      && nitems >= 3
      && dataP)
    {
      PROP32 *data = (PROP32 *) dataP;
      si->auth_p = (data[0] == XA_AUTH);
      if (si->auth_p != oauth_p)  /* Faster watchdog while authenticating */
        reset_watchdog_timer (si);
    }
  else
    si->auth_p = False;
}


/* Processing ClientMessage events.
   Both xscreensaver and xscreensaver-gfx handle these; some are handled
   exclusively by one program or another, and a couple (next, prev) are
   handled by xscreensaver only if xscreensaver-gfx is not already running.
 */
static void
handle_clientmessage (saver_info *si, XEvent *xev)
{
  Display *dpy = si->dpy;
  saver_preferences *p = &si->prefs;
  Atom type = 0;

  if (xev->xclient.message_type != XA_SCREENSAVER ||
      xev->xclient.format != 32)
    return;

  /* Preferences might affect our handling of client messages. */
  maybe_reload_init_file (si);

  type = xev->xclient.data.l[0];

  if (type == XA_CYCLE)
    {
      int i;
      clientmessage_response (dpy, xev, True, "cycling");
      si->selection_mode = 0;	/* 0 means randomize when its time. */
    CYCLE:
      si->demoing_p = False;
      monitor_power_on (si, True);

      for (i = 0; i < si->nscreens; i++)
        {
          saver_screen_info *ssi = &si->screens[i];
          cycle_timer ((XtPointer) ssi, 0);
        }
    }
  else if (type == XA_NEXT || type == XA_PREV)
    {
      /* If xscreensaver-gfx was not running, these were handled by
         xscreensaver.  If xscreensaver-gfx is running, then xscreensaver
         ignored these and we reply instead.
       */
      clientmessage_response (dpy, xev, True, "cycling");
      si->selection_mode = (type == XA_NEXT ? -1 : -2);
      goto CYCLE;
    }
  else if (type == XA_SELECT)
    {
      /* Same xscreensaver/xscreensaver-gfx division of labor as next/prev. */
      long which = xev->xclient.data.l[1];

      if (p->mode == DONT_BLANK)
        {
          clientmessage_response (dpy, xev, False, "blanking disabled");
          return;
        }

      clientmessage_response (dpy, xev, True, "selecting");

      if (which < 0) which = 0;		/* 0 == "random" */
      si->selection_mode = which;
      goto CYCLE;
    }
  else if (si->all_clientmessages_p)
    {
      /* The xscreensaver daemon is not running, so return an error response
         for unhandled ClientMessages so that xscreensaver-command doesn't
         have to time out waiting for a response that will not arrive. */
      clientmessage_response (dpy, xev, False,
                              "xscreensaver daemon not running");
    }
  else
    {
      /* All other ClientMessages are handled by xscreensaver rather than
         xscreensaver-gfx, and it will return an error message for unknown
         ones. */
    }
}


#ifdef DEBUG_MULTISCREEN
/* For monitor hot-swap stress-testing.  See screens.c. */
static void
debug_multiscreen_timer (XtPointer closure, XtIntervalId *id)
{
  saver_info *si = (saver_info *) closure;
  saver_preferences *p = &si->prefs;
  if (update_screen_layout (si))
    {
      if (p->verbose_p)
        {
          fprintf (stderr, "%s: new layout:\n", blurb());
          describe_monitor_layout (si->monitor_layout);
        }
      resize_screensaver_window (si);
    }
  XtAppAddTimeOut (si->app, 1000*4, debug_multiscreen_timer, (XtPointer) si);
}
#endif /* DEBUG_MULTISCREEN */


static void
main_loop (saver_info *si, Bool init_p)
{
  saver_preferences *p = &si->prefs;

  if (init_p && p->verbose_p)
    print_available_extensions (si);

  initialize_randr (si);
  update_screen_layout (si);

  if (p->verbose_p)
    describe_monitor_layout (si->monitor_layout);

  /* The screen blanked (will blank) at xscreensaver-gfx's launch time.
     The last user activity time is presumed to be 'timeout' before that.
     Unless the command line included --next, --prev, --demo, etc.
   */
  si->blank_time = time ((time_t *) 0);
  si->activity_time = (si->selection_mode == 0
                       ? si->blank_time - p->timeout / 1000
                       : si->blank_time);

  initialize_screensaver_window (si);
  init_sigchld (si);
  read_status_prop (si);

  if (! p->verbose_p)
    ;
  else if (si->demoing_p)
    fprintf (stderr, "%s: demoing %d\n", blurb(), si->selection_mode);
  else
    fprintf (stderr, "%s: blanking\n", blurb());

  blank_screen (si);

# ifdef DEBUG_MULTISCREEN
  if (p->debug_p)
    debug_multiscreen_timer ((XtPointer) si, 0);
# endif

  while (1)
    {
      /* We have to go through this union bullshit because gcc-4.4.0 has
         stricter struct-aliasing rules.  Without this, the optimizer
         can fuck things up. */
      union {
        XEvent x_event;
# ifdef HAVE_RANDR
        XRRScreenChangeNotifyEvent xrr_event;
# endif /* HAVE_RANDR */
      } event;

      /* Block waiting for the next X event, while running timers. */
      XtAppNextEvent (si->app, &event.x_event);

      if (event.x_event.xany.type == ClientMessage)
        handle_clientmessage (si, &event.x_event);

      else if (event.x_event.xany.type == PropertyNotify &&
               event.x_event.xproperty.state == PropertyNewValue &&
               event.x_event.xproperty.atom == XA_SCREENSAVER_STATUS)
        status_prop_changed (si);

# ifdef HAVE_RANDR
      else if (si->using_randr_extension &&
               (event.x_event.type == 
                (si->randr_event_number + RRScreenChangeNotify)))
        {
          /* The Resize and Rotate extension sends an event when the
             size, rotation, or refresh rate of any screen has changed.
           */
          Bool changed_p;

          /* Inform Xlib that it's ok to update its data structures. */
          XRRUpdateConfiguration (&event.x_event);

          /* Resize the existing xscreensaver windows and cached ssi data. */
          changed_p = update_screen_layout (si);

          if (p->verbose_p)
            {
              int screen = XRRRootToScreen (si->dpy, event.xrr_event.window);
              fprintf (stderr, "%s: %d: screen change event: %s\n",
                       blurb(), screen,
                       (changed_p ? "new layout:" : "layout unchanged"));
              if (changed_p)
                describe_monitor_layout (si->monitor_layout);
            }

          if (changed_p)
            resize_screensaver_window (si);
        }
# endif /* HAVE_RANDR */

      XtDispatchEvent (&event.x_event);
    }
}


int
main (int argc, char **argv)
{
  char *dpy_str = getenv ("DISPLAY");
  char *s;
  saver_info the_si;
  saver_info *si = &the_si;
  saver_preferences *p = &si->prefs;
  Bool init_p = False;
  Bool cmdline_verbose_val = False, cmdline_verbose_p = False;
  int i;

  memset(si, 0, sizeof(*si));
  global_si_kludge = si;	/* I hate C so much... */

# undef ya_rand_init
  ya_rand_init (0);

  /* For Xt and X resource database purposes, this program is
     "xscreensaver", not "xscreensaver-gfx".
   */
  s = strrchr(argv[0], '/');
  if (s) s++;
  else s = argv[0];
  if (strlen(s) > 20)	/* keep it short. */
    s[20] = 0;
  progname = s;

  progclass = "XScreenSaver";

  si->version = strdup (screensaver_id + 17);
  s = strchr (si->version, ' ');
  *s = 0;

  for (i = 1; i < argc; i++)
    {
      const char *oa = argv[i];
      /* XScreenSaver predates the "--arg" convention. */
      if (argv[i][0] == '-' && argv[i][1] == '-')
        argv[i]++;

      if (!strcmp (argv[i], "-debug"))
        p->debug_p = True;
      else if (!strcmp (argv[i], "-v") || !strcmp (argv[i], "-verbose"))
        {
          p->verbose_p++;
          cmdline_verbose_p = True, cmdline_verbose_val = p->verbose_p;
        }
      else if (!strcmp (argv[i], "-vv"))
        {
          p->verbose_p += 2;
          cmdline_verbose_p = True, cmdline_verbose_val = p->verbose_p;
        }
      else if (!strcmp (argv[i], "-vvv"))
        {
          p->verbose_p += 3;
          cmdline_verbose_p = True, cmdline_verbose_val = p->verbose_p;
        }
      else if (!strcmp (argv[i], "-vvvv"))
        {
          p->verbose_p += 4;
          cmdline_verbose_p = True, cmdline_verbose_val = p->verbose_p;
        }
      else if (!strcmp (argv[i], "-q") || !strcmp (argv[i], "-quiet"))
        {
          p->verbose_p = False;
          cmdline_verbose_p = True, cmdline_verbose_val = p->verbose_p;
        }
      else if (!strcmp (argv[i], "-init"))
        init_p = True;
      else if (!strcmp (argv[i], "-d") ||
               !strcmp (argv[i], "-dpy") ||
               !strcmp (argv[i], "-disp") ||
               !strcmp (argv[i], "-display"))
        {
          dpy_str = argv[++i];
          if (!dpy_str) goto HELP;
        }
      else if (!strcmp (argv[i], "-ver") ||
               !strcmp (argv[i], "-vers") ||
               !strcmp (argv[i], "-version"))
        {
          fprintf (stderr, "%s\n", screensaver_id+4);
          exit (1);
        }
      else if (!strcmp (argv[i], "-sync") ||
               !strcmp (argv[i], "-synch") ||
               !strcmp (argv[i], "-synchronize") ||
               !strcmp (argv[i], "-synchronise"))
        p->xsync_p = True;
      else if (!strcmp (argv[i], "-h") || !strcmp (argv[i], "-help"))
        {
        HELP:
          fprintf (stderr,
            "\n"
            "\txscreensaver-gfx is launched by the xscreensaver daemon\n"
            "\tto manage the graphical display modes as subprocesses.\n"
            "\tDo not run this directly.\n"
            "\n"
            "\tOptions:\n"
            "\t\t--dpy host:display.screen\n"
            "\t\t--verbose --debug --sync\n"
            "\t\t--init --emergency\n"
            "\t\t--next --prev --select N --demo N\n"
            "\n"
            "\tRun 'xscreensaver-settings' to configure.\n"
            "\n");
          exit (1);
        }
      else if (!strcmp (argv[i], "-emergency"))
        {
          /* This means we should start up without a fade-out, blanking the
             screen as quickly as possible.  This is used when suspending,
             and also if xscreensaver-gfx has crashed and is being restarted.
           */
          si->emergency_p = True;
        }
      else if (!strcmp (argv[i], "-next"))
        {
          /* "xscreensaver-command -next" sent a ClientMessage to the daemon
             while it was idle, so we should start with this hack. */
          si->selection_mode = -1;  /* -1 means next */
        }
      else if (!strcmp (argv[i], "-prev"))
        {
          si->selection_mode = -2;  /* -2 means prev */
        }
      else if (!strcmp (argv[i], "-select") ||
               !strcmp (argv[i], "-demo"))
        {
          /* "xscreensaver-command -select N" sent a ClientMessage to the
             daemon while it was idle, so we should start with this hack. */
          int n;
          char c;
          if (!strcmp (argv[i], "-demo"))
            si->demoing_p = True;	/* Just means "don't auto-cycle" */

          if (! argv[++i]) goto FAIL;
          if (1 != sscanf (argv[i], " %d %c", &n, &c)) goto HELP;
          if (n <= 0) goto FAIL;
          si->selection_mode = n;  /* hack number is 1-based */
        }
      else
        {
        FAIL:
          fprintf (stderr, "\n%s: unknown option: %s\n", blurb(), oa);
          goto HELP;
        }
    }

  /* Copy the -dpy arg to $DISPLAY for subprocesses. */
  s = (char *) malloc (strlen(dpy_str) + 20);
  sprintf (s, "DISPLAY=%s", dpy_str);
  putenv (s);
  /* free (s); */  /* some versions of putenv do not copy */

# ifdef ENABLE_NLS
  {
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    textdomain (GETTEXT_PACKAGE);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    if (!setlocale (LC_ALL, ""))
      fprintf (stderr, "%s: warning: could not set default locale\n",
               progname);
  }
# endif /* ENABLE_NLS */

  connect_to_server (si);

  if (p->xsync_p)
    XSynchronize (si->dpy, True);

  load_init_file (si->dpy, p);

  /* Command line overrides init file */
  if (cmdline_verbose_p)
    p->verbose_p = cmdline_verbose_val;
  verbose_p = p->verbose_p;
  debug_p   = p->debug_p;

  main_loop (si, init_p);		/* doesn't return */
  exit (-1);
}
