/* xscreensaver, Copyright (c) 1991-1999 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/*   ========================================================================
 *   First we wait until the keyboard and mouse become idle for the specified
 *   amount of time.  We do this in one of three different ways: periodically
 *   checking with the XIdle server extension; selecting key and mouse events
 *   on (nearly) all windows; or by waiting for the MIT-SCREEN-SAVER extension
 *   to send us a "you are idle" event.
 *
 *   Then, we map a full screen black window (or, in the case of the 
 *   MIT-SCREEN-SAVER extension, use the one it gave us.)
 *
 *   We place a __SWM_VROOT property on this window, so that newly-started
 *   clients will think that this window is a "virtual root" window.
 *
 *   If there is an existing "virtual root" window (one that already had
 *   an __SWM_VROOT property) then we remove that property from that window.
 *   Otherwise, clients would see that window (the real virtual root) instead
 *   of ours (the impostor.)
 *
 *   Then we pick a random program to run, and start it.  Two assumptions 
 *   are made about this program: that it has been specified with whatever
 *   command-line options are necessary to make it run on the root window;
 *   and that it has been compiled with vroot.h, so that it is able to find
 *   the root window when a virtual-root window manager (or this program) is
 *   running.
 *
 *   Then, we wait for keyboard or mouse events to be generated on the window.
 *   When they are, we kill the inferior process, unmap the window, and restore
 *   the __SWM_VROOT property to the real virtual root window if there was one.
 *
 *   While we are waiting, we also set up timers so that, after a certain 
 *   amount of time has passed, we can start a different screenhack.  We do
 *   this by killing the running child process with SIGTERM, and then starting
 *   a new one in the same way.
 *
 *   If there was a real virtual root, meaning that we removed the __SWM_VROOT
 *   property from it, meaning we must (absolutely must) restore it before we
 *   exit, then we set up signal handlers for most signals (SIGINT, SIGTERM,
 *   etc.) that do this.  Most Xlib and Xt routines are not reentrant, so it
 *   is not generally safe to call them from signal handlers; however, this
 *   program spends most of its time waiting, so the window of opportunity 
 *   when code could be called reentrantly is fairly small; and also, the worst
 *   that could happen is that the call would fail.  If we've gotten one of
 *   these signals, then we're on our way out anyway.  If we didn't restore the
 *   __SWM_VROOT property, that would be very bad, so it's worth a shot.  Note
 *   that this means that, if you're using a virtual-root window manager, you
 *   can really fuck up the world by killing this process with "kill -9".
 *
 *   This program accepts ClientMessages of type SCREENSAVER; these messages
 *   may contain the atom ACTIVATE or DEACTIVATE, meaning to turn the 
 *   screensaver on or off now, regardless of the idleness of the user,
 *   and a few other things.  The included "xscreensaver_command" program
 *   sends these messsages.
 *
 *   If we don't have the XIdle, MIT-SCREEN-SAVER, or SGI SCREEN_SAVER
 *   extensions, then we do the XAutoLock trick: notice every window that
 *   gets created, and wait 30 seconds or so until its creating process has
 *   settled down, and then select KeyPress events on those windows which
 *   already select for KeyPress events.  It's important that we not select
 *   KeyPress on windows which don't select them, because that would
 *   interfere with event propagation.  This will break if any program
 *   changes its event mask to contain KeyRelease or PointerMotion more than
 *   30 seconds after creating the window, but that's probably pretty rare.
 *   
 *   The reason that we can't select KeyPresses on windows that don't have
 *   them already is that, when dispatching a KeyPress event, X finds the
 *   lowest (leafmost) window in the hierarchy on which *any* client selects
 *   for KeyPress, and sends the event to that window.  This means that if a
 *   client had a window with subwindows, and expected to receive KeyPress
 *   events on the parent window instead of the subwindows, then that client
 *   would malfunction if some other client selected KeyPress events on the
 *   subwindows.  It is an incredible misdesign that one client can make
 *   another client malfunction in this way.
 *
 *   To detect mouse motion, we periodically wake up and poll the mouse
 *   position and button/modifier state, and notice when something has
 *   changed.  We make this check every five seconds by default, and since the
 *   screensaver timeout has a granularity of one minute, this makes the
 *   chance of a false positive very small.  We could detect mouse motion in
 *   the same way as keyboard activity, but that would suffer from the same
 *   "client changing event mask" problem that the KeyPress events hack does.
 *   I think polling is more reliable.
 *
 *   None of this crap happens if we're using one of the extensions, so install
 *   one of them if the description above sounds just too flaky to live.  It
 *   is, but those are your choices.
 *
 *   A third idle-detection option could be implemented (but is not): when
 *   running on the console display ($DISPLAY is `localhost`:0) and we're on a
 *   machine where /dev/tty and /dev/mouse have reasonable last-modification
 *   times, we could just stat() those.  But the incremental benefit of
 *   implementing this is really small, so forget I said anything.
 *
 *   Debugging hints:
 *     - Have a second terminal handy.
 *     - Be careful where you set your breakpoints, you don't want this to
 *       stop under the debugger with the keyboard grabbed or the blackout
 *       window exposed.
 *     - If you run your debugger under XEmacs, try M-ESC (x-grab-keyboard)
 *       to keep your emacs window alive even when xscreensaver has grabbed.
 *     - Go read the code related to `debug_p'.
 *     - You probably can't set breakpoints in functions that are called on
 *       the other side of a call to fork() -- if your clients are dying 
 *       with signal 5, Trace/BPT Trap, you're losing in this way.
 *     - If you aren't using a server extension, don't leave this stopped
 *       under the debugger for very long, or the X input buffer will get
 *       huge because of the keypress events it's selecting for.  This can
 *       make your X server wedge with "no more input buffers."
 *       
 * ======================================================================== */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <ctype.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xos.h>
#ifdef HAVE_XMU
# ifndef VMS
#  include <X11/Xmu/Error.h>
# else  /* !VMS */
#  include <Xmu/Error.h>
# endif /* !VMS */
#else  /* !HAVE_XMU */
# include "xmu.h"
#endif /* !HAVE_XMU */

#ifdef HAVE_XIDLE_EXTENSION
#include <X11/extensions/xidle.h>
#endif /* HAVE_XIDLE_EXTENSION */

#include "xscreensaver.h"
#include "version.h"
#include "yarandom.h"
#include "resources.h"
#include "visual.h"
#include "usleep.h"

saver_info *global_si_kludge = 0;	/* I hate C so much... */

char *progname = 0;
char *progclass = 0;
XrmDatabase db = 0;


static Atom XA_SCREENSAVER_RESPONSE;
static Atom XA_ACTIVATE, XA_DEACTIVATE, XA_CYCLE, XA_NEXT, XA_PREV;
static Atom XA_EXIT, XA_RESTART, XA_LOCK, XA_SELECT;
Atom XA_DEMO, XA_PREFS;


static XrmOptionDescRec options [] = {
  { "-timeout",		   ".timeout",		XrmoptionSepArg, 0 },
  { "-cycle",		   ".cycle",		XrmoptionSepArg, 0 },
  { "-lock-mode",	   ".lock",		XrmoptionNoArg, "on" },
  { "-no-lock-mode",	   ".lock",		XrmoptionNoArg, "off" },
  { "-no-lock",		   ".lock",		XrmoptionNoArg, "off" },
  { "-lock-timeout",	   ".lockTimeout",	XrmoptionSepArg, 0 },
  { "-lock-vts",	   ".lockVTs",		XrmoptionNoArg, "on" },
  { "-no-lock-vts",	   ".lockVTs",		XrmoptionNoArg, "off" },
  { "-visual",		   ".visualID",		XrmoptionSepArg, 0 },
  { "-install",		   ".installColormap",	XrmoptionNoArg, "on" },
  { "-no-install",	   ".installColormap",	XrmoptionNoArg, "off" },
  { "-verbose",		   ".verbose",		XrmoptionNoArg, "on" },
  { "-silent",		   ".verbose",		XrmoptionNoArg, "off" },
  { "-timestamp",	   ".timestamp",	XrmoptionNoArg, "on" },
  { "-capture-stderr",	   ".captureStderr",	XrmoptionNoArg, "on" },
  { "-no-capture-stderr",  ".captureStderr",	XrmoptionNoArg, "off" },
  { "-xidle-extension",	   ".xidleExtension",	XrmoptionNoArg, "on" },
  { "-no-xidle-extension", ".xidleExtension",	XrmoptionNoArg, "off" },
  { "-mit-extension",	   ".mitSaverExtension",XrmoptionNoArg, "on" },
  { "-no-mit-extension",   ".mitSaverExtension",XrmoptionNoArg, "off" },
  { "-sgi-extension",	   ".sgiSaverExtension",XrmoptionNoArg, "on" },
  { "-no-sgi-extension",   ".sgiSaverExtension",XrmoptionNoArg, "off" },
  { "-splash",		   ".splash",		XrmoptionNoArg, "on" },
  { "-no-splash",	   ".splash",		XrmoptionNoArg, "off" },
  { "-nosplash",	   ".splash",		XrmoptionNoArg, "off" },
  { "-idelay",		   ".initialDelay",	XrmoptionSepArg, 0 },
  { "-nice",		   ".nice",		XrmoptionSepArg, 0 },

  /* Actually these are built in to Xt, but just to be sure... */
  { "-synchronous",	   ".synchronous",	XrmoptionNoArg, "on" },
  { "-xrm",		   NULL,		XrmoptionResArg, NULL }
};

static char *defaults[] = {
#include "XScreenSaver_ad.h"
 0
};

#ifdef _VROOT_H_
ERROR!  You must not include vroot.h in this file.
#endif

static void
do_help (saver_info *si)
{
  fflush (stdout);
  fflush (stderr);
  fprintf (stdout, "\
xscreensaver %s, copyright (c) 1991-1998 by Jamie Zawinski <jwz@jwz.org>\n\
The standard Xt command-line options are accepted; other options include:\n\
\n\
    -timeout <minutes>       When the screensaver should activate.\n\
    -cycle <minutes>         How long to let each hack run before switching.\n\
    -lock-mode               Require a password before deactivating.\n\
    -lock-timeout <minutes>  Grace period before locking; default 0.\n\
    -visual <id-or-class>    Which X visual to run on.\n\
    -install                 Install a private colormap.\n\
    -verbose                 Be loud.\n\
    -no-splash               Don't display a splash-screen at startup.\n\
    -help                    This message.\n\
\n\
See the manual for other options and X resources.\n\
\n\
The `xscreensaver' program should be left running in the background.\n\
Use the `xscreensaver-demo' and `xscreensaver-command' programs to\n\
manipulate a running xscreensaver.\n\
\n\
The `*programs' resource controls which graphics demos will be launched by\n\
the screensaver.  See `man xscreensaver' or the web page for more details.\n\
\n\
Just getting started?  Try this:\n\
\n\
        xscreensaver &\n\
        xscreensaver-demo\n\
\n\
For updates, check http://www.jwz.org/xscreensaver/\n\
\n",
	  si->version);
  fflush (stdout);
  fflush (stderr);
  exit (1);
}


char *
timestring (void)
{
  time_t now = time ((time_t *) 0);
  char *str = (char *) ctime (&now);
  char *nl = (char *) strchr (str, '\n');
  if (nl) *nl = 0; /* take off that dang newline */
  return str;
}

static Bool blurb_timestamp_p = False;   /* kludge */

const char *
blurb (void)
{
  if (!blurb_timestamp_p)
    return progname;
  else
    {
      static char buf[255];
      char *ct = timestring();
      int n = strlen(progname);
      if (n > 100) n = 99;
      strncpy(buf, progname, n);
      buf[n++] = ':';
      buf[n++] = ' ';
      strncpy(buf+n, ct+11, 8);
      strcpy(buf+n+9, ": ");
      return buf;
    }
}


int
saver_ehandler (Display *dpy, XErrorEvent *error)
{
  saver_info *si = global_si_kludge;	/* I hate C so much... */

  if (!real_stderr) real_stderr = stderr;

  fprintf (real_stderr, "\n"
	   "#######################################"
	   "#######################################\n\n"
	   "%s: X Error!  PLEASE REPORT THIS BUG.\n\n"
	   "#######################################"
	   "#######################################\n\n",
	   blurb());
  if (XmuPrintDefaultErrorMessage (dpy, error, real_stderr))
    {
      fprintf (real_stderr, "\n");
      if (si->prefs.xsync_p)
	{
	  saver_exit (si, -1, "because of synchronous X Error");
	}
      else
	{
          fprintf (real_stderr,
                   "#######################################"
                   "#######################################\n\n");
          fprintf (real_stderr,
   "    If at all possible, please re-run xscreensaver with the command line\n"
   "    arguments `-sync -verbose', and reproduce this bug.  That will cause\n"
   "    xscreensaver to dump a `core' file to the current directory.  Please\n"
   "    include the stack trace from that core file in your bug report.\n"
   "\n"
   "    http://www.jwz.org/xscreensaver/bugs.html explains how to create the\n"
   "    most useful bug reports, and how to examine core files.\n"
   "\n"
   "    The more information you can provide, the better.  But please report\n"
   "    report this bug, regardless!\n"
   "\n");
          fprintf (real_stderr,
                   "#######################################"
                   "#######################################\n\n");

	  saver_exit (si, -1, 0);
	}
    }
  else
    fprintf (real_stderr, " (nonfatal.)\n");
  return 0;
}


/* This error handler is used only while the X connection is being set up;
   after we've got a connection, we don't use this handler again.  The only
   reason for having this is so that we can present a more idiot-proof error
   message than "cannot open display."
 */
static void 
startup_ehandler (String name, String type, String class,
                  String defalt,  /* one can't even spel properly
                                     in this joke of a language */
                  String *av, Cardinal *ac)
{
  char fmt[512];
  String p[10];
  saver_info *si = global_si_kludge;	/* I hate C so much... */
  XrmDatabase *db = XtAppGetErrorDatabase(si->app);
  *fmt = 0;
  XtAppGetErrorDatabaseText(si->app, name, type, class, defalt,
                            fmt, sizeof(fmt)-1, *db);

  fprintf (stderr, "%s: ", blurb());

  memset (p, 0, sizeof(p));
  if (*ac > countof (p)) *ac = countof (p);
  memcpy ((char *) p, (char *) av, (*ac) * sizeof(*av));
  fprintf (stderr, fmt,		/* Did I mention that I hate C? */
           p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9]);
  fprintf (stderr, "\n");

  describe_uids (si, stderr);
  fprintf (stderr, "\n"
           "%s: Errors at startup are usually authorization problems.\n"
           "              Did you read the manual?  Specifically, the parts\n"
           "              that talk about XAUTH, XDM, and root logins?\n"
           "\n"
           "              http://www.jwz.org/xscreensaver/man.html\n"
           "\n",
           blurb());

  fflush (stderr);
  fflush (stdout);
  exit (1);
}


/* The zillions of initializations.
 */

/* Set progname, version, etc.  This is done very early.
 */
static void
set_version_string (saver_info *si, int *argc, char **argv)
{
  progclass = "XScreenSaver";

  /* progname is reset later, after we connect to X. */
  progname = strrchr(argv[0], '/');
  if (progname) progname++;
  else progname = argv[0];

  if (strlen(progname) > 100)	/* keep it short. */
    progname[99] = 0;

  /* The X resource database blows up if argv[0] has a "." in it. */
  {
    char *s = argv[0];
    while ((s = strchr (s, '.')))
      *s = '_';
  }

  si->version = (char *) malloc (5);
  memcpy (si->version, screensaver_id + 17, 4);
  si->version [4] = 0;
}


/* Initializations that potentially take place as a priveleged user:
   If the xscreensaver executable is setuid root, then these initializations
   are run as root, before discarding privileges.
 */
static void
privileged_initialization (saver_info *si, int *argc, char **argv)
{
#ifdef NO_LOCKING
  si->locking_disabled_p = True;
  si->nolock_reason = "not compiled with locking support";
#else /* !NO_LOCKING */
  si->locking_disabled_p = False;
  /* before hack_uid() for proper permissions */
  if (! lock_init (*argc, argv, si->prefs.verbose_p))
    {
      si->locking_disabled_p = True;
      si->nolock_reason = "error getting password";
    }
#endif /* NO_LOCKING */

#ifndef NO_SETUID
  hack_uid (si);
#endif /* NO_SETUID */
}


/* Open the connection to the X server, and intern our Atoms.
 */
static Widget
connect_to_server (saver_info *si, int *argc, char **argv)
{
  Widget toplevel_shell;

  XSetErrorHandler (saver_ehandler);

  XtAppSetErrorMsgHandler (si->app, startup_ehandler);
  toplevel_shell = XtAppInitialize (&si->app, progclass,
				    options, XtNumber (options),
				    argc, argv, defaults, 0, 0);
  XtAppSetErrorMsgHandler (si->app, 0);

  si->dpy = XtDisplay (toplevel_shell);
  si->prefs.db = XtDatabase (si->dpy);
  XtGetApplicationNameAndClass (si->dpy, &progname, &progclass);

  if(strlen(progname) > 100)	/* keep it short. */
    progname [99] = 0;

  db = si->prefs.db;	/* resources.c needs this */

  XA_VROOT = XInternAtom (si->dpy, "__SWM_VROOT", False);
  XA_SCREENSAVER = XInternAtom (si->dpy, "SCREENSAVER", False);
  XA_SCREENSAVER_VERSION = XInternAtom (si->dpy, "_SCREENSAVER_VERSION",False);
  XA_SCREENSAVER_ID = XInternAtom (si->dpy, "_SCREENSAVER_ID", False);
  XA_SCREENSAVER_TIME = XInternAtom (si->dpy, "_SCREENSAVER_TIME", False);
  XA_SCREENSAVER_RESPONSE = XInternAtom (si->dpy, "_SCREENSAVER_RESPONSE",
					 False);
  XA_XSETROOT_ID = XInternAtom (si->dpy, "_XSETROOT_ID", False);
  XA_ACTIVATE = XInternAtom (si->dpy, "ACTIVATE", False);
  XA_DEACTIVATE = XInternAtom (si->dpy, "DEACTIVATE", False);
  XA_RESTART = XInternAtom (si->dpy, "RESTART", False);
  XA_CYCLE = XInternAtom (si->dpy, "CYCLE", False);
  XA_NEXT = XInternAtom (si->dpy, "NEXT", False);
  XA_PREV = XInternAtom (si->dpy, "PREV", False);
  XA_SELECT = XInternAtom (si->dpy, "SELECT", False);
  XA_EXIT = XInternAtom (si->dpy, "EXIT", False);
  XA_DEMO = XInternAtom (si->dpy, "DEMO", False);
  XA_PREFS = XInternAtom (si->dpy, "PREFS", False);
  XA_LOCK = XInternAtom (si->dpy, "LOCK", False);

  return toplevel_shell;
}


/* Handle the command-line arguments that were not handled for us by Xt.
   Issue an error message and exit if there are unknown options.
 */
static void
process_command_line (saver_info *si, int *argc, char **argv)
{
  int i;
  for (i = 1; i < *argc; i++)
    {
      if (!strcmp (argv[i], "-debug"))
	/* no resource for this one, out of paranoia. */
	si->prefs.debug_p = True;

      else if (!strcmp (argv[i], "-h") ||
	       !strcmp (argv[i], "-help") ||
	       !strcmp (argv[i], "--help"))
	do_help (si);

      else
	{
	  const char *s = argv[i];
	  fprintf (stderr, "%s: unknown option \"%s\".  Try \"-help\".\n",
		   blurb(), s);

	  if (s[0] == '-' && s[1] == '-') s++;
	  if (!strcmp (s, "-activate") ||
	      !strcmp (s, "-deactivate") ||
	      !strcmp (s, "-cycle") ||
	      !strcmp (s, "-next") ||
	      !strcmp (s, "-prev") ||
	      !strcmp (s, "-exit") ||
	      !strcmp (s, "-restart") ||
	      !strcmp (s, "-demo") ||
	      !strcmp (s, "-prefs") ||
	      !strcmp (s, "-preferences") ||
	      !strcmp (s, "-lock") ||
	      !strcmp (s, "-version") ||
	      !strcmp (s, "-time"))
	    {

	      if (!strcmp (s, "-demo") || !strcmp (s, "-prefs"))
		fprintf (stderr, "\n\
    Perhaps you meant to run the `xscreensaver-demo' program instead?\n");
	      else
		fprintf (stderr, "\n\
    However, `%s' is an option to the `xscreensaver-command' program.\n", s);

	      fprintf (stderr, "\
    The `xscreensaver' program is a daemon that runs in the background.\n\
    You control a running xscreensaver process by sending it messages\n\
    with `xscreensaver-demo' or `xscreensaver-command'.\n\
.   See the man pages for details, or check the web page:\n\
    http://www.jwz.org/xscreensaver/\n\n");

	      /* Since version 1.21 renamed the "-lock" option to "-lock-mode",
		 suggest that explicitly. */
	      if (!strcmp (s, "-lock"))
		fprintf (stderr, "\
    Or perhaps you meant either the \"-lock-mode\" or the\n\
    \"-lock-timeout <minutes>\" options to xscreensaver?\n\n");
	    }

	  exit (1);
	}
    }
}


/* Print out the xscreensaver banner to the tty if applicable;
   Issue any other warnings that are called for at this point.
 */
static void
print_banner (saver_info *si)
{
  saver_preferences *p = &si->prefs;

  /* This resource gets set some time before the others, so that we know
     whether to print the banner (and so that the banner gets printed before
     any resource-database-related error messages.)
   */
  p->verbose_p = (p->debug_p || get_boolean_resource ("verbose", "Boolean"));

  /* Ditto, for the locking_disabled_p message. */
  p->lock_p = get_boolean_resource ("lock", "Boolean");

  if (p->verbose_p)
    fprintf (stderr,
	     "%s %s, copyright (c) 1991-1998 "
	     "by Jamie Zawinski <jwz@jwz.org>.\n",
	     progname, si->version);

  if (p->debug_p)
    fprintf (stderr, "\n"
	     "%s: Warning: running in DEBUG MODE.  Be afraid.\n"
	     "\n"
	     "\tNote that in debug mode, the xscreensaver window will only\n"
	     "\tcover the left half of the screen.  (The idea is that you\n"
	     "\tcan still see debugging output in a shell, if you position\n"
	     "\tit on the right side of the screen.)\n"
	     "\n"
	     "\tDebug mode is NOT SECURE.  Do not run with -debug in\n"
	     "\tuntrusted environments.\n"
	     "\n",
	     blurb());

  if (p->verbose_p)
    {
      if (!si->uid_message || !*si->uid_message)
	describe_uids (si, stderr);
      else
	{
	  if (si->orig_uid && *si->orig_uid)
	    fprintf (stderr, "%s: initial effective uid/gid was %s.\n",
		     blurb(), si->orig_uid);
	  fprintf (stderr, "%s: %s\n", blurb(), si->uid_message);
	}

      fprintf (stderr, "%s: in process %lu.\n", blurb(),
	       (unsigned long) getpid());
    }

  /* If locking was not able to be initalized for some reason, explain why.
     (This has to be done after we've read the lock_p resource.)
   */
  if (p->lock_p && si->locking_disabled_p)
    {
      p->lock_p = False;
      fprintf (stderr, "%s: locking is disabled (%s).\n", blurb(),
	       si->nolock_reason);
      if (strstr (si->nolock_reason, "passw"))
	fprintf (stderr, "%s: does xscreensaver need to be setuid?  "
		 "consult the manual.\n", blurb());
      else if (strstr (si->nolock_reason, "running as "))
	fprintf (stderr, 
		 "%s: locking only works when xscreensaver is launched\n"
		 "\t by a normal, non-privileged user (e.g., not \"root\".)\n"
		 "\t See the manual for details.\n",
		 blurb());
    }
}


/* Examine all of the display's screens, and populate the `saver_screen_info'
   structures.
 */
static void
initialize_per_screen_info (saver_info *si, Widget toplevel_shell)
{
  Bool found_any_writable_cells = False;
  int i;

  si->nscreens = ScreenCount(si->dpy);
  si->screens = (saver_screen_info *)
    calloc(sizeof(saver_screen_info), si->nscreens);

  si->default_screen = &si->screens[DefaultScreen(si->dpy)];

  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      ssi->global = si;
      ssi->screen = ScreenOfDisplay (si->dpy, i);

      /* Note: we can't use the resource ".visual" because Xt is SO FUCKED. */
      ssi->default_visual =
	get_visual_resource (ssi->screen, "visualID", "VisualID", False);

      ssi->current_visual = ssi->default_visual;
      ssi->current_depth = visual_depth (ssi->screen, ssi->current_visual);

      if (ssi == si->default_screen)
	/* Since this is the default screen, use the one already created. */
	ssi->toplevel_shell = toplevel_shell;
      else
	/* Otherwise, each screen must have its own unmapped root widget. */
	ssi->toplevel_shell =
	  XtVaAppCreateShell (progname, progclass, applicationShellWidgetClass,
			      si->dpy,
			      XtNscreen, ssi->screen,
			      XtNvisual, ssi->current_visual,
			      XtNdepth,  visual_depth (ssi->screen,
						       ssi->current_visual),
			      0);

      if (! found_any_writable_cells)
	{
	  /* Check to see whether fading is ever possible -- if any of the
	     screens on the display has a PseudoColor visual, then fading can
	     work (on at least some screens.)  If no screen has a PseudoColor
	     visual, then don't bother ever trying to fade, because it will
	     just cause a delay without causing any visible effect.
	  */
	  if (has_writable_cells (ssi->screen, ssi->current_visual) ||
	      get_visual (ssi->screen, "PseudoColor", True, False) ||
	      get_visual (ssi->screen, "GrayScale", True, False))
	    found_any_writable_cells = True;
	}
    }

  si->prefs.fading_possible_p = found_any_writable_cells;
}


/* If any server extensions have been requested, try and initialize them.
   Issue warnings if requests can't be honored.
 */
static void
initialize_server_extensions (saver_info *si)
{
  saver_preferences *p = &si->prefs;

  Bool server_has_xidle_extension_p = False;
  Bool server_has_sgi_saver_extension_p = False;
  Bool server_has_mit_saver_extension_p = False;

  si->using_xidle_extension = p->use_xidle_extension;
  si->using_sgi_saver_extension = p->use_sgi_saver_extension;
  si->using_mit_saver_extension = p->use_mit_saver_extension;

#ifdef HAVE_XIDLE_EXTENSION
  server_has_xidle_extension_p = query_xidle_extension (si);
#endif
#ifdef HAVE_SGI_SAVER_EXTENSION
  server_has_sgi_saver_extension_p = query_sgi_saver_extension (si);
#endif
#ifdef HAVE_MIT_SAVER_EXTENSION
  server_has_mit_saver_extension_p = query_mit_saver_extension (si);
#endif

  if (!server_has_xidle_extension_p)
    si->using_xidle_extension = False;
  else if (p->verbose_p)
    {
      if (si->using_xidle_extension)
	fprintf (stderr, "%s: using XIDLE extension.\n", blurb());
      else
	fprintf (stderr, "%s: not using server's XIDLE extension.\n", blurb());
    }

  if (!server_has_sgi_saver_extension_p)
    si->using_sgi_saver_extension = False;
  else if (p->verbose_p)
    {
      if (si->using_sgi_saver_extension)
	fprintf (stderr, "%s: using SGI SCREEN_SAVER extension.\n", blurb());
      else
	fprintf (stderr,
		 "%s: not using server's SGI SCREEN_SAVER extension.\n",
		 blurb());
    }

  if (!server_has_mit_saver_extension_p)
    si->using_mit_saver_extension = False;
  else if (p->verbose_p)
    {
      if (si->using_mit_saver_extension)
	fprintf (stderr, "%s: using lame MIT-SCREEN-SAVER extension.\n",
		 blurb());
      else
	fprintf (stderr,
		 "%s: not using server's lame MIT-SCREEN-SAVER extension.\n",
		 blurb());
    }
}


/* For the case where we aren't using an server extensions, select user events
   on all the existing windows, and launch timers to select events on
   newly-created windows as well.

   If a server extension is being used, this does nothing.
 */
static void
select_events (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  int i;

  if (si->using_xidle_extension ||
      si->using_mit_saver_extension ||
      si->using_sgi_saver_extension)
    return;

  if (p->initial_delay)
    {
      if (p->verbose_p)
	{
	  fprintf (stderr, "%s: waiting for %d second%s...", blurb(),
		   (int) p->initial_delay/1000,
		   (p->initial_delay == 1000 ? "" : "s"));
	  fflush (stderr);
	  fflush (stdout);
	}
      usleep (p->initial_delay);
      if (p->verbose_p)
	fprintf (stderr, " done.\n");
    }

  if (p->verbose_p)
    {
      fprintf (stderr, "%s: selecting events on extant windows...", blurb());
      fflush (stderr);
      fflush (stdout);
    }

  /* Select events on the root windows of every screen.  This also selects
     for window creation events, so that new subwindows will be noticed.
   */
  for (i = 0; i < si->nscreens; i++)
    start_notice_events_timer (si, RootWindowOfScreen (si->screens[i].screen));

  if (p->verbose_p)
    fprintf (stderr, " done.\n");
}


void
maybe_reload_init_file (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  if (init_file_changed_p (p))
    {
      if (p->verbose_p)
	fprintf (stderr, "%s: file \"%s\" has changed, reloading.\n",
		 blurb(), init_file_name());

      load_init_file (p);

      /* If a server extension is in use, and p->timeout has changed,
	 we need to inform the server of the new timeout. */
      disable_builtin_screensaver (si, False);
    }
}


/* Loop forever:

       - wait until the user is idle;
       - blank the screen;
       - wait until the user is active;
       - unblank the screen;
       - repeat.

 */
static void
main_loop (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  Bool ok_to_unblank;

  while (1)
    {
      sleep_until_idle (si, True);

      if (p->verbose_p)
	{
	  if (si->demoing_p)
	    fprintf (stderr, "%s: demoing %d at %s.\n", blurb(),
		     si->selection_mode, timestring());
	  else
	    if (p->verbose_p)
	      fprintf (stderr, "%s: blanking screen at %s.\n", blurb(),
		       timestring());
	}

      maybe_reload_init_file (si);

      blank_screen (si);
      kill_screenhack (si);
      spawn_screenhack (si, True);

      /* Don't start the cycle timer in demo mode. */
      if (!si->demoing_p && p->cycle)
	si->cycle_id = XtAppAddTimeOut (si->app, p->cycle, cycle_timer,
					(XtPointer) si);


#ifndef NO_LOCKING
      if (!si->demoing_p &&		/* if not going into demo mode */
	  p->lock_p &&			/* and locking is enabled */
	  !si->locking_disabled_p &&	/* and locking is possible */
	  p->lock_timeout == 0)		/* and locking is not timer-deferred */
	si->locked_p = True;		/* then lock right now. */

      /* locked_p might be true already because of the above, or because of
	 the LOCK ClientMessage.  But if not, and if we're supposed to lock
	 after some time, set up a timer to do so.
       */
      if (p->lock_p &&
	  !si->locked_p &&
	  p->lock_timeout > 0)
	si->lock_id = XtAppAddTimeOut (si->app, p->lock_timeout,
				       activate_lock_timer,
				       (XtPointer) si);
#endif /* !NO_LOCKING */


      ok_to_unblank = True;
      do {

	sleep_until_idle (si, False);		/* until not idle */
	maybe_reload_init_file (si);

#ifndef NO_LOCKING
	if (si->locked_p)
	  {
	    saver_screen_info *ssi = si->default_screen;
	    if (si->locking_disabled_p) abort ();

	    si->dbox_up_p = True;
	    suspend_screenhack (si, True);
	    XUndefineCursor (si->dpy, ssi->screensaver_window);

	    ok_to_unblank = unlock_p (si);

	    si->dbox_up_p = False;
	    XDefineCursor (si->dpy, ssi->screensaver_window, ssi->cursor);
	    suspend_screenhack (si, False);	/* resume */
	  }
#endif /* !NO_LOCKING */

	} while (!ok_to_unblank);


      if (p->verbose_p)
	fprintf (stderr, "%s: unblanking screen at %s.\n",
		 blurb(), timestring ());

      /* Kill before unblanking, to stop drawing as soon as possible. */
      kill_screenhack (si);
      unblank_screen (si);

      si->locked_p = False;
      si->demoing_p = 0;
      si->selection_mode = 0;

      if (si->cycle_id)
	{
	  XtRemoveTimeOut (si->cycle_id);
	  si->cycle_id = 0;
	}

      if (si->lock_id)
	{
	  XtRemoveTimeOut (si->lock_id);
	  si->lock_id = 0;
	}

      if (p->verbose_p)
	fprintf (stderr, "%s: awaiting idleness.\n", blurb());
    }
}

static void analyze_display (saver_info *si);

int
main (int argc, char **argv)
{
  Widget shell;
  saver_info the_si;
  saver_info *si = &the_si;
  saver_preferences *p = &si->prefs;
  int i;

  memset(si, 0, sizeof(*si));
  global_si_kludge = si;	/* I hate C so much... */

  srandom ((int) time ((time_t *) 0));

  save_argv (argc, argv);
  set_version_string (si, &argc, argv);
  privileged_initialization (si, &argc, argv);
  hack_environment (si);

  shell = connect_to_server (si, &argc, argv);
  process_command_line (si, &argc, argv);
  print_banner (si);

  initialize_per_screen_info (si, shell);  /* also sets p->fading_possible_p */

  for (i = 0; i < si->nscreens; i++)
    if (ensure_no_screensaver_running (si->dpy, si->screens[i].screen))
      exit (1);

  load_init_file (p);

  if (p->xsync_p) XSynchronize (si->dpy, True);
  blurb_timestamp_p = p->timestamp_p;  /* kludge */

  if (p->verbose_p) analyze_display (si);
  initialize_server_extensions (si);
  initialize_screensaver_window (si);
  select_events (si);
  init_sigchld ();
  disable_builtin_screensaver (si, True);
  initialize_stderr (si);

  make_splash_dialog (si);

  main_loop (si);		/* doesn't return */
  return 0;
}


/* Processing ClientMessage events.
 */

static void
clientmessage_response (saver_info *si, Window w, Bool error,
			const char *stderr_msg,
			const char *protocol_msg)
{
  char *proto;
  int L;
  saver_preferences *p = &si->prefs;
  if (error || p->verbose_p)
    fprintf (stderr, "%s: %s\n", blurb(), stderr_msg);

  L = strlen(protocol_msg);
  proto = (char *) malloc (L + 2);
  proto[0] = (error ? '-' : '+');
  strcpy (proto+1, protocol_msg);
  L++;

  XChangeProperty (si->dpy, w, XA_SCREENSAVER_RESPONSE, XA_STRING, 8,
		   PropModeReplace, proto, L);
  XSync (si->dpy, False);
  free (proto);
}

Bool
handle_clientmessage (saver_info *si, XEvent *event, Bool until_idle_p)
{
  Atom type = 0;
  Window window = event->xclient.window;

  /* Preferences might affect our handling of client messages. */
  maybe_reload_init_file (si);

  if (event->xclient.message_type != XA_SCREENSAVER)
    {
      char *str;
      str = XGetAtomName (si->dpy, event->xclient.message_type);
      fprintf (stderr, "%s: unrecognised ClientMessage type %s received\n",
	       blurb(), (str ? str : "(null)"));
      if (str) XFree (str);
      return False;
    }
  if (event->xclient.format != 32)
    {
      fprintf (stderr, "%s: ClientMessage of format %d received, not 32\n",
	       blurb(), event->xclient.format);
      return False;
    }

  type = event->xclient.data.l[0];
  if (type == XA_ACTIVATE)
    {
      if (until_idle_p)
	{
	  clientmessage_response(si, window, False,
				 "ACTIVATE ClientMessage received.",
				 "activating.");
	  si->selection_mode = 0;
	  si->demoing_p = False;
	  if (si->using_mit_saver_extension || si->using_sgi_saver_extension)
	    {
	      XForceScreenSaver (si->dpy, ScreenSaverActive);
	      return False;
	    }
	  else
	    {
	      return True;
	    }
	}
      clientmessage_response(si, window, True,
		       "ClientMessage ACTIVATE received while already active.",
			     "already active.");
    }
  else if (type == XA_DEACTIVATE)
    {
      if (! until_idle_p)
	{
	  clientmessage_response(si, window, False,
				 "DEACTIVATE ClientMessage received.",
				 "deactivating.");
	  if (si->using_mit_saver_extension || si->using_sgi_saver_extension)
	    {
	      XForceScreenSaver (si->dpy, ScreenSaverReset);
	      return False;
	    }
	  else
	    {
	      return True;
	    }
	}
      clientmessage_response(si, window, True,
			   "ClientMessage DEACTIVATE received while inactive.",
			     "not active.");
    }
  else if (type == XA_CYCLE)
    {
      if (! until_idle_p)
	{
	  clientmessage_response(si, window, False,
				 "CYCLE ClientMessage received.",
				 "cycling.");
	  si->selection_mode = 0;	/* 0 means randomize when its time. */
	  si->demoing_p = False;
	  if (si->cycle_id)
	    XtRemoveTimeOut (si->cycle_id);
	  si->cycle_id = 0;
	  cycle_timer ((XtPointer) si, 0);
	  return False;
	}
      clientmessage_response(si, window, True,
			     "ClientMessage CYCLE received while inactive.",
			     "not active.");
    }
  else if (type == XA_NEXT || type == XA_PREV)
    {
      clientmessage_response(si, window, False,
			     (type == XA_NEXT
			      ? "NEXT ClientMessage received."
			      : "PREV ClientMessage received."),
			     "cycling.");
      si->selection_mode = (type == XA_NEXT ? -1 : -2);
      si->demoing_p = False;

      if (! until_idle_p)
	{
	  if (si->cycle_id)
	    XtRemoveTimeOut (si->cycle_id);
	  si->cycle_id = 0;
	  cycle_timer ((XtPointer) si, 0);
	}
      else
	return True;
    }
  else if (type == XA_SELECT)
    {
      char buf [255];
      char buf2 [255];
      long which = event->xclient.data.l[1];

      sprintf (buf, "SELECT %ld ClientMessage received.", which);
      sprintf (buf2, "activating (%ld).", which);
      clientmessage_response (si, window, False, buf, buf2);

      if (which < 0) which = 0;		/* 0 == "random" */
      si->selection_mode = which;
      si->demoing_p = False;

      if (! until_idle_p)
	{
	  if (si->cycle_id)
	    XtRemoveTimeOut (si->cycle_id);
	  si->cycle_id = 0;
	  cycle_timer ((XtPointer) si, 0);
	}
      else
	return True;
    }
  else if (type == XA_EXIT)
    {
      /* Ignore EXIT message if the screen is locked. */
      if (until_idle_p || !si->locked_p)
	{
	  clientmessage_response (si, window, False,
				  "EXIT ClientMessage received.",
				  "exiting.");
	  if (! until_idle_p)
	    {
	      unblank_screen (si);
	      kill_screenhack (si);
	      XSync (si->dpy, False);
	    }
	  saver_exit (si, 0, 0);
	}
      else
	clientmessage_response (si, window, True,
				"EXIT ClientMessage received while locked.",
				"screen is locked.");
    }
  else if (type == XA_RESTART)
    {
      /* The RESTART message works whether the screensaver is active or not,
	 unless the screen is locked, in which case it doesn't work.
       */
      if (until_idle_p || !si->locked_p)
	{
	  clientmessage_response (si, window, False,
				  "RESTART ClientMessage received.",
				  "restarting.");
	  if (! until_idle_p)
	    {
	      unblank_screen (si);
	      kill_screenhack (si);
	      XSync (si->dpy, False);
	    }

	  /* make sure error message shows up before exit. */
	  if (real_stderr && stderr != real_stderr)
	    dup2 (fileno(real_stderr), fileno(stderr));

	  restart_process (si);
	  exit (1);	/* shouldn't get here; but if restarting didn't work,
			   make this command be the same as EXIT. */
	}
      else
	clientmessage_response (si, window, True,
				"RESTART ClientMessage received while locked.",
				"screen is locked.");
    }
  else if (type == XA_DEMO)
    {
      long arg = event->xclient.data.l[1];
      Bool demo_one_hack_p = (arg == 300);

      if (demo_one_hack_p)
	{
	  if (until_idle_p)
	    {
	      long which = event->xclient.data.l[2];
	      char buf [255];
	      char buf2 [255];
	      sprintf (buf, "DEMO %ld ClientMessage received.", which);
	      sprintf (buf2, "demoing (%ld).", which);
	      clientmessage_response (si, window, False, buf, buf2);

	      if (which < 0) which = 0;		/* 0 == "random" */
	      si->selection_mode = which;
	      si->demoing_p = True;

	      return True;
	    }

	  clientmessage_response (si, window, True,
				  "DEMO ClientMessage received while active.",
				  "already active.");
	}
      else
	{
	  clientmessage_response (si, window, True,
				  "obsolete form of DEMO ClientMessage.",
				  "obsolete form of DEMO ClientMessage.");
	}
    }
  else if (type == XA_PREFS)
    {
      clientmessage_response (si, window, True,
			      "the PREFS client-message is obsolete.",
			      "the PREFS client-message is obsolete.");
    }
  else if (type == XA_LOCK)
    {
#ifdef NO_LOCKING
      clientmessage_response (si, window, True,
			      "not compiled with support for locking.",
			      "locking not enabled.");
#else /* !NO_LOCKING */
      if (si->locking_disabled_p)
	clientmessage_response (si, window, True,
		      "LOCK ClientMessage received, but locking is disabled.",
			      "locking not enabled.");
      else if (si->locked_p)
	clientmessage_response (si, window, True,
			   "LOCK ClientMessage received while already locked.",
				"already locked.");
      else
	{
	  char buf [255];
	  char *response = (until_idle_p
			    ? "activating and locking."
			    : "locking.");
	  si->locked_p = True;
	  si->selection_mode = 0;
	  si->demoing_p = False;
	  sprintf (buf, "LOCK ClientMessage received; %s", response);
	  clientmessage_response (si, window, False, buf, response);

	  if (si->lock_id)	/* we're doing it now, so lose the timeout */
	    {
	      XtRemoveTimeOut (si->lock_id);
	      si->lock_id = 0;
	    }

	  if (until_idle_p)
	    {
	      if (si->using_mit_saver_extension ||
                  si->using_sgi_saver_extension)
		{
		  XForceScreenSaver (si->dpy, ScreenSaverActive);
		  return False;
		}
	      else
		{
		  return True;
		}
	    }
	}
#endif /* !NO_LOCKING */
    }
  else
    {
      char buf [1024];
      char *str;
      str = (type ? XGetAtomName(si->dpy, type) : 0);

      if (str)
	{
	  if (strlen (str) > 80)
	    strcpy (str+70, "...");
	  sprintf (buf, "unrecognised screensaver ClientMessage %s received.",
		   str);
	  free (str);
	}
      else
	{
	  sprintf (buf,
		   "unrecognised screensaver ClientMessage 0x%x received.",
		   (unsigned int) event->xclient.data.l[0]);
	}

      clientmessage_response (si, window, True, buf, buf);
    }
  return False;
}


/* Some random diagnostics printed in -verbose mode.
 */

static void
analyze_display (saver_info *si)
{
  int i, j;
  static const char *exts[][2] = {
    { "SCREEN_SAVER",	   "SGI Screen-Saver" },
    { "SCREEN-SAVER",	   "SGI Screen-Saver" },
    { "MIT-SCREEN-SAVER",  "MIT Screen-Saver" },
    { "XIDLE",		   "XIdle" },
    { "SGI-VIDEO-CONTROL", "SGI Video-Control" },
    { "READDISPLAY",	   "SGI Read-Display" },
    { "MIT-SHM",	   "Shared Memory" },
    { "DOUBLE-BUFFER",	   "Double-Buffering" },
    { "DPMS",		   "Power Management" },
    { "GLX",		   "GLX" }
  };

  fprintf (stderr, "%s: running on display \"%s\"\n", blurb(),
	   DisplayString(si->dpy));
  fprintf (stderr, "%s: vendor is %s, %d\n", blurb(),
	   ServerVendor(si->dpy), VendorRelease(si->dpy));

  fprintf (stderr, "%s: useful extensions:\n", blurb());
  for (i = 0; i < countof(exts); i++)
    {
      int op = 0, event = 0, error = 0;
      if (XQueryExtension (si->dpy, exts[i][0], &op, &event, &error))
	fprintf (stderr, "%s:   %s\n", blurb(), exts[i][1]);
    }

  for (i = 0; i < si->nscreens; i++)
    {
      unsigned long colormapped_depths = 0;
      unsigned long non_mapped_depths = 0;
      XVisualInfo vi_in, *vi_out;
      int out_count;
      vi_in.screen = i;
      vi_out = XGetVisualInfo (si->dpy, VisualScreenMask, &vi_in, &out_count);
      if (!vi_out) continue;
      for (j = 0; j < out_count; j++)
	if (vi_out[j].class == PseudoColor)
	  colormapped_depths |= (1 << vi_out[j].depth);
	else
	  non_mapped_depths  |= (1 << vi_out[j].depth);
      XFree ((char *) vi_out);

      if (colormapped_depths)
	{
	  fprintf (stderr, "%s: screen %d colormapped depths:", blurb(), i);
	  for (j = 0; j < 32; j++)
	    if (colormapped_depths & (1 << j))
	      fprintf (stderr, " %d", j);
	  fprintf (stderr, "\n");
	}
      if (non_mapped_depths)
	{
	  fprintf (stderr, "%s: screen %d non-mapped depths:", blurb(), i);
	  for (j = 0; j < 32; j++)
	    if (non_mapped_depths & (1 << j))
	      fprintf (stderr, " %d", j);
	  fprintf (stderr, "\n");
	}
    }
}
