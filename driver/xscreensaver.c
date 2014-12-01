/* xscreensaver, Copyright (c) 1991-2013 Jamie Zawinski <jwz@jwz.org>
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
 *   Then, we map a full screen black window.
 *
 *   We place a __SWM_VROOT property on this window, so that newly-started
 *   clients will think that this window is a "virtual root" window (as per
 *   the logic in the historical "vroot.h" header.)
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
 *   On multi-screen systems, we do the above on each screen, and start
 *   multiple programs, each with a different value of $DISPLAY.
 *
 *   On Xinerama systems, we do a similar thing, but instead create multiple
 *   windows on the (only) display, and tell the subprocess which one to use
 *   via the $XSCREENSAVER_WINDOW environment variable -- this trick requires
 *   a recent (Aug 2003) revision of vroot.h.
 *
 *   (See comments in screens.c for more details about Xinerama/RANDR stuff.)
 *
 *   While we are waiting for user activity, we also set up timers so that,
 *   after a certain amount of time has passed, we can start a different
 *   screenhack.  We do this by killing the running child process with
 *   SIGTERM, and then starting a new one in the same way.
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
 *   may contain the atoms ACTIVATE, DEACTIVATE, etc, meaning to turn the 
 *   screensaver on or off now, regardless of the idleness of the user,
 *   and a few other things.  The included "xscreensaver-command" program
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
 *   30 seconds after creating the window, but such programs do not seem to
 *   occur in nature (I've never seen it happen in all these years.)
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
 *   On systems with /proc/interrupts (Linux) we poll that file and note when
 *   the interrupt counter numbers on the "keyboard" and "PS/2" lines change.
 *   (There is no reliable way, using /proc/interrupts, to detect non-PS/2
 *   mice, so it doesn't help for serial or USB mice.)
 *
 *   None of this crap happens if we're using one of the extensions.  Sadly,
 *   the XIdle extension hasn't been available for many years; the SGI
 *   extension only exists on SGIs; and the MIT extension, while widely
 *   deployed, is garbage in several ways.
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
 *       the other side of a call to fork() -- if your subprocesses are
 *       dying with signal 5, Trace/BPT Trap, you're losing in this way.
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

#ifdef ENABLE_NLS
# include <locale.h>
# include <libintl.h>
#endif /* ENABLE_NLS */

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
#ifdef HAVE_XMU
# ifndef VMS
#  include <X11/Xmu/Error.h>
# else  /* !VMS */
#  include <Xmu/Error.h>
# endif /* !VMS */
#else  /* !HAVE_XMU */
# include "xmu.h"
#endif /* !HAVE_XMU */

#ifdef HAVE_MIT_SAVER_EXTENSION
#include <X11/extensions/scrnsaver.h>
#endif /* HAVE_MIT_SAVER_EXTENSION */

#ifdef HAVE_XIDLE_EXTENSION
# include <X11/extensions/xidle.h>
#endif /* HAVE_XIDLE_EXTENSION */

#ifdef HAVE_SGI_VC_EXTENSION
# include <X11/extensions/XSGIvc.h>
#endif /* HAVE_SGI_VC_EXTENSION */

#ifdef HAVE_READ_DISPLAY_EXTENSION
# include <X11/extensions/readdisplay.h>
#endif /* HAVE_READ_DISPLAY_EXTENSION */

#ifdef HAVE_XSHM_EXTENSION
# include <X11/extensions/XShm.h>
#endif /* HAVE_XSHM_EXTENSION */

#ifdef HAVE_DPMS_EXTENSION
# include <X11/extensions/dpms.h>
#endif /* HAVE_DPMS_EXTENSION */


#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
# include <X11/extensions/Xdbe.h>
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

#ifdef HAVE_XF86VMODE
# include <X11/extensions/xf86vmode.h>
#endif /* HAVE_XF86VMODE */

#ifdef HAVE_XF86MISCSETGRABKEYSSTATE
# include <X11/extensions/xf86misc.h>
#endif /* HAVE_XF86MISCSETGRABKEYSSTATE */

#ifdef HAVE_XINERAMA
# include <X11/extensions/Xinerama.h>
#endif /* HAVE_XINERAMA */

#ifdef HAVE_RANDR
# include <X11/extensions/Xrandr.h>
#endif /* HAVE_RANDR */


#include "xscreensaver.h"
#include "version.h"
#include "yarandom.h"
#include "resources.h"
#include "visual.h"
#include "usleep.h"
#include "auth.h"

saver_info *global_si_kludge = 0;	/* I hate C so much... */

char *progname = 0;
char *progclass = 0;
XrmDatabase db = 0;


static Atom XA_SCREENSAVER_RESPONSE;
static Atom XA_ACTIVATE, XA_DEACTIVATE, XA_CYCLE, XA_NEXT, XA_PREV;
static Atom XA_RESTART, XA_SELECT;
static Atom XA_THROTTLE, XA_UNTHROTTLE;
Atom XA_DEMO, XA_PREFS, XA_EXIT, XA_LOCK, XA_BLANK;


static XrmOptionDescRec options [] = {

  { "-verbose",		   ".verbose",		XrmoptionNoArg, "on" },
  { "-silent",		   ".verbose",		XrmoptionNoArg, "off" },

  /* xscreensaver-demo uses this one */
  { "-nosplash",	   ".splash",		XrmoptionNoArg, "off" },
  { "-no-splash",	   ".splash",		XrmoptionNoArg, "off" },

  /* useful for debugging */
  { "-no-capture-stderr",  ".captureStderr",	XrmoptionNoArg, "off" },
  { "-log",		   ".logFile",		XrmoptionSepArg, 0 },
};

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

static void
do_help (saver_info *si)
{
  char *s, year[5];
  s = strchr (screensaver_id, '-');
  s = strrchr (s, '-');
  s++;
  strncpy (year, s, 4);
  year[4] = 0;

  fflush (stdout);
  fflush (stderr);
  fprintf (stdout, "\
xscreensaver %s, copyright (c) 1991-%s by Jamie Zawinski <jwz@jwz.org>\n\
\n\
  All xscreensaver configuration is via the `~/.xscreensaver' file.\n\
  Rather than editing that file by hand, just run `xscreensaver-demo':\n\
  that program lets you configure the screen saver graphically,\n\
  including timeouts, locking, and display modes.\n\
\n",
	  si->version, year);
  fprintf (stdout, "\
  Just getting started?  Try this:\n\
\n\
        xscreensaver &\n\
        xscreensaver-demo\n\
\n\
  For updates, online manual, and FAQ, please see the web page:\n\
\n\
       http://www.jwz.org/xscreensaver/\n\
\n");

  fflush (stdout);
  fflush (stderr);
  exit (1);
}


Bool in_signal_handler_p = 0;	/* I hate C so much... */

char *
timestring (void)
{
  if (in_signal_handler_p)
    {
      /* Turns out that ctime() and even localtime_r() call malloc() on Linux!
         So we can't call them from inside SIGCHLD.  WTF.
       */
      static char buf[30];
      strcpy (buf, "... ... ..   signal ....");
      return buf;
    }
  else
    {
      time_t now = time ((time_t *) 0);
      char *str = (char *) ctime (&now);
      char *nl = (char *) strchr (str, '\n');
      if (nl) *nl = 0; /* take off that dang newline */
      return str;
    }
}

static Bool blurb_timestamp_p = True;   /* kludge */

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
  int i;
  Bool fatal_p;

  if (!real_stderr) real_stderr = stderr;

  fprintf (real_stderr, "\n"
	   "#######################################"
	   "#######################################\n\n"
	   "%s: X Error!  PLEASE REPORT THIS BUG.\n",
	   blurb());

  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      fprintf (real_stderr, "%s: screen %d/%d: 0x%x, 0x%x, 0x%x\n",
               blurb(), ssi->real_screen_number, ssi->number,
               (unsigned int) RootWindowOfScreen (si->screens[i].screen),
               (unsigned int) si->screens[i].real_vroot,
               (unsigned int) si->screens[i].screensaver_window);
    }

  fprintf (real_stderr, "\n"
	   "#######################################"
	   "#######################################\n\n");

  fatal_p = XmuPrintDefaultErrorMessage (dpy, error, real_stderr);

  fatal_p = True;  /* The only time I've ever seen a supposedly nonfatal error,
                      it has been BadImplementation / Xlib sequence lost, which
                      are in truth pretty damned fatal.
                    */

  fprintf (real_stderr, "\n");

  if (! fatal_p)
    fprintf (real_stderr, "%s: nonfatal error.\n\n", blurb());
  else
    {
      if (si->prefs.xsync_p)
	{
	  saver_exit (si, -1, "because of synchronous X Error");
	}
      else
	{
#ifdef __GNUC__
  __extension__   /* don't warn about "string length is greater than the
                     length ISO C89 compilers are required to support". */
#endif
          fprintf (real_stderr,
   "#######################################################################\n"
   "\n"
   "    If at all possible, please re-run xscreensaver with the command\n"
   "    line arguments `-sync -verbose -log log.txt', and reproduce this\n"
   "    bug.  That will cause xscreensaver to dump a `core' file to the\n"
   "    current directory.  Please include the stack trace from that core\n"
   "    file in your bug report.  *DO NOT* mail the core file itself!  That\n"
   "    won't work.  A \"log.txt\" file will also be written.  Please *do*\n"
   "    include the complete \"log.txt\" file with your bug report.\n"
   "\n"
   "    http://www.jwz.org/xscreensaver/bugs.html explains how to create\n"
   "    the most useful bug reports, and how to examine core files.\n"
   "\n"
   "    The more information you can provide, the better.  But please\n"
   "    report this bug, regardless!\n"
   "\n"
   "#######################################################################\n"
   "\n"
   "\n");

	  saver_exit (si, -1, 0);
	}
    }

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

  if (si->orig_uid && !strncmp (si->orig_uid, "root/", 5))
    {
      fprintf (stderr, "\n"
          "%s: This is probably because you're logging in as root.  You\n"
"              shouldn't log in as root: you should log in as a normal user,\n"
"              and then `su' as needed.  If you insist on logging in as\n"
"              root, you will have to turn off X's security features before\n"
"              xscreensaver will work.\n"
               "\n"
"              Please read the manual and FAQ for more information:\n",
               blurb());
    }
  else
    {
      fprintf (stderr, "\n"
          "%s: Errors at startup are usually authorization problems.\n"
"              But you're not logging in as root (good!) so something\n"
"              else must be wrong.  Did you read the manual and the FAQ?\n",
           blurb());
    }

  fprintf (stderr, "\n"
          "              http://www.jwz.org/xscreensaver/faq.html\n"
          "              http://www.jwz.org/xscreensaver/man.html\n"
          "\n");

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
#ifndef NO_LOCKING
  /* before hack_uid() for proper permissions */
  lock_priv_init (*argc, argv, si->prefs.verbose_p);
#endif /* NO_LOCKING */

  hack_uid (si);
}


/* Figure out what locking mechanisms are supported.
 */
static void
lock_initialization (saver_info *si, int *argc, char **argv)
{
#ifdef NO_LOCKING
  si->locking_disabled_p = True;
  si->nolock_reason = "not compiled with locking support";
#else /* !NO_LOCKING */

  /* Finish initializing locking, now that we're out of privileged code. */
  if (! lock_init (*argc, argv, si->prefs.verbose_p))
    {
      si->locking_disabled_p = True;
      si->nolock_reason = "error getting password";
    }

  /* If locking is currently enabled, but the environment indicates that
     we have been launched as GDM's "Background" program, then disable
     locking just in case.
   */
  if (!si->locking_disabled_p && getenv ("RUNNING_UNDER_GDM"))
    {
      si->locking_disabled_p = True;
      si->nolock_reason = "running under GDM";
    }

  /* If the server is XDarwin (MacOS X) then disable locking.
     (X grabs only affect X programs, so you can use Command-Tab
     to bring any other Mac program to the front, e.g., Terminal.)
   */
  if (!si->locking_disabled_p)
    {
      int op = 0, event = 0, error = 0;
      Bool macos_p = False;

#ifdef __APPLE__
      /* Disable locking if *running* on Apple hardware, since we have no
         reliable way to determine whether the server is running on MacOS.
         Hopefully __APPLE__ means "MacOS" and not "Linux on Mac hardware"
         but I'm not really sure about that.
       */
      macos_p = True;
#endif

      if (!macos_p)
        /* This extension exists on the Apple X11 server, but not
           on earlier versions of the XDarwin server. */
        macos_p = XQueryExtension (si->dpy, "Apple-DRI", &op, &event, &error);

      if (macos_p)
        {
          si->locking_disabled_p = True;
          si->nolock_reason = "Cannot lock securely on MacOS X";
        }
    }

  if (si->prefs.debug_p)    /* But allow locking anyway in debug mode. */
    si->locking_disabled_p = False;

#endif /* NO_LOCKING */
}


/* Open the connection to the X server, and intern our Atoms.
 */
static Widget
connect_to_server (saver_info *si, int *argc, char **argv)
{
  Widget toplevel_shell;

#ifdef HAVE_PUTENV
  char *d = getenv ("DISPLAY");
  if (!d || !*d)
    {
      char *ndpy = strdup("DISPLAY=:0.0");
      /* if (si->prefs.verbose_p) */      /* sigh, too early to test this... */
        fprintf (stderr,
                 "%s: warning: $DISPLAY is not set: defaulting to \"%s\".\n",
                 blurb(), ndpy+8);
      if (putenv (ndpy))
        abort ();
      /* don't free (ndpy) -- some implementations of putenv (BSD 4.4,
         glibc 2.0) copy the argument, but some (libc4,5, glibc 2.1.2)
         do not.  So we must leak it (and/or the previous setting). Yay.
       */
    }
#endif /* HAVE_PUTENV */

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
  XA_SCREENSAVER_STATUS = XInternAtom (si->dpy, "_SCREENSAVER_STATUS", False);
  XA_SCREENSAVER_RESPONSE = XInternAtom (si->dpy, "_SCREENSAVER_RESPONSE",
					 False);
  XA_XSETROOT_ID = XInternAtom (si->dpy, "_XSETROOT_ID", False);
  XA_ESETROOT_PMAP_ID = XInternAtom (si->dpy, "ESETROOT_PMAP_ID", False);
  XA_XROOTPMAP_ID = XInternAtom (si->dpy, "_XROOTPMAP_ID", False);
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
  XA_BLANK = XInternAtom (si->dpy, "BLANK", False);
  XA_THROTTLE = XInternAtom (si->dpy, "THROTTLE", False);
  XA_UNTHROTTLE = XInternAtom (si->dpy, "UNTHROTTLE", False);

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

  char *s, year[5];
  s = strchr (screensaver_id, '-');
  s = strrchr (s, '-');
  s++;
  strncpy (year, s, 4);
  year[4] = 0;

  /* This resource gets set some time before the others, so that we know
     whether to print the banner (and so that the banner gets printed before
     any resource-database-related error messages.)
   */
  p->verbose_p = (p->debug_p || 
                  get_boolean_resource (si->dpy, "verbose", "Boolean"));

  /* Ditto, for the locking_disabled_p message. */
  p->lock_p = get_boolean_resource (si->dpy, "lock", "Boolean");

  if (p->verbose_p)
    fprintf (stderr,
	     "%s %s, copyright (c) 1991-%s "
	     "by Jamie Zawinski <jwz@jwz.org>.\n",
	     progname, si->version, year);

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

  if (p->verbose_p && senescent_p ())
    fprintf (stderr, "\n"
             "*************************************"
             "**************************************\n"
	     "%s: Warning: this version of xscreensaver is VERY OLD!\n"
	     "%s: Please upgrade!  http://www.jwz.org/xscreensaver/\n"
             "*************************************"
             "**************************************\n"
	     "\n",
             blurb(), blurb());

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
}

static void
print_lock_failure_banner (saver_info *si)
{
  saver_preferences *p = &si->prefs;

  /* If locking was not able to be initalized for some reason, explain why.
     (This has to be done after we've read the lock_p resource.)
   */
  if (si->locking_disabled_p)
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


/* called from screens.c so that all the Xt crud is here. */
void
initialize_screen_root_widget (saver_screen_info *ssi)
{
  saver_info *si = ssi->global;
  if (ssi->toplevel_shell)
    XtDestroyWidget (ssi->toplevel_shell);
  ssi->toplevel_shell =
    XtVaAppCreateShell (progname, progclass, 
                        applicationShellWidgetClass,
                        si->dpy,
                        XtNscreen, ssi->screen,
                        XtNvisual, ssi->current_visual,
                        XtNdepth,  visual_depth (ssi->screen,
                                                 ssi->current_visual),
                        NULL);
}


/* Examine all of the display's screens, and populate the `saver_screen_info'
   structures.  Make sure this is called after hack_environment() sets $PATH.
 */
static void
initialize_per_screen_info (saver_info *si, Widget toplevel_shell)
{
  int i;

  update_screen_layout (si);

  /* Check to see whether fading is ever possible -- if any of the
     screens on the display has a PseudoColor visual, then fading can
     work (on at least some screens.)  If no screen has a PseudoColor
     visual, then don't bother ever trying to fade, because it will
     just cause a delay without causing any visible effect.
   */
  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      if (has_writable_cells (ssi->screen, ssi->current_visual) ||
          get_visual (ssi->screen, "PseudoColor", True, False) ||
          get_visual (ssi->screen, "GrayScale", True, False))
        {
          si->fading_possible_p = True;
          break;
        }
    }

#ifdef HAVE_XF86VMODE_GAMMA
  si->fading_possible_p = True;  /* if we can gamma fade, go for it */
#endif
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
  Bool system_has_proc_interrupts_p = False;
  Bool server_has_xinput_extension_p = False;
  const char *piwhy = 0;

  si->using_xidle_extension = p->use_xidle_extension;
  si->using_sgi_saver_extension = p->use_sgi_saver_extension;
  si->using_mit_saver_extension = p->use_mit_saver_extension;
  si->using_proc_interrupts = p->use_proc_interrupts;
  si->using_xinput_extension = p->use_xinput_extension;

#ifdef HAVE_XIDLE_EXTENSION
  {
    int ev, er;
    server_has_xidle_extension_p = XidleQueryExtension (si->dpy, &ev, &er);
  }
#endif
#ifdef HAVE_SGI_SAVER_EXTENSION
  server_has_sgi_saver_extension_p =
    XScreenSaverQueryExtension (si->dpy,
                                &si->sgi_saver_ext_event_number,
                                &si->sgi_saver_ext_error_number);
#endif
#ifdef HAVE_MIT_SAVER_EXTENSION
  server_has_mit_saver_extension_p =
    XScreenSaverQueryExtension (si->dpy,
                                &si->mit_saver_ext_event_number,
                                &si->mit_saver_ext_error_number);
#endif
#ifdef HAVE_PROC_INTERRUPTS
  system_has_proc_interrupts_p = query_proc_interrupts_available (si, &piwhy);
#endif

#ifdef HAVE_XINPUT
  server_has_xinput_extension_p = query_xinput_extension (si);
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

#ifdef HAVE_XINPUT
  if (!server_has_xinput_extension_p)
    si->using_xinput_extension = False;
  else
    {
      if (si->using_xinput_extension)
        init_xinput_extension(si);

      if (p->verbose_p)
        {
          if (si->using_xinput_extension)
            fprintf (stderr,
                     "%s: selecting events from %d XInputExtension devices.\n",
                     blurb(), si->num_xinput_devices);
          else
            fprintf (stderr,
                     "%s: not using XInputExtension.\n",
                     blurb());
        }
    }
#endif

  if (!system_has_proc_interrupts_p)
    {
      si->using_proc_interrupts = False;
      if (p->verbose_p && piwhy)
	fprintf (stderr, "%s: not using /proc/interrupts: %s.\n", blurb(),
                 piwhy);
    }
  else if (p->verbose_p)
    {
      if (si->using_proc_interrupts)
	fprintf (stderr,
                 "%s: consulting /proc/interrupts for keyboard activity.\n",
		 blurb());
      else
	fprintf (stderr,
                "%s: not consulting /proc/interrupts for keyboard activity.\n",
		 blurb());
    }
}


#ifdef DEBUG_MULTISCREEN
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
          describe_monitor_layout (si);
        }
      resize_screensaver_window (si);
    }
  XtAppAddTimeOut (si->app, 1000*4, debug_multiscreen_timer, (XtPointer) si);
}
#endif /* DEBUG_MULTISCREEN */


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
    {
      saver_screen_info *ssi = &si->screens[i];
      if (ssi->real_screen_p)
        start_notice_events_timer (si,
           RootWindowOfScreen (si->screens[i].screen), False);
    }

  if (p->verbose_p)
    fprintf (stderr, " done.\n");

# ifdef DEBUG_MULTISCREEN
  if (p->debug_p) debug_multiscreen_timer ((XtPointer) si, 0);
# endif
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

      load_init_file (si->dpy, p);

      /* If a server extension is in use, and p->timeout has changed,
	 we need to inform the server of the new timeout. */
      disable_builtin_screensaver (si, False);

      /* If the DPMS settings in the init file have changed,
         change the settings on the server to match. */
      sync_server_dpms_settings (si->dpy,
                                 (p->dpms_enabled_p  &&
                                  p->mode != DONT_BLANK),
                                 p->dpms_quickoff_p,
                                 p->dpms_standby / 1000,
                                 p->dpms_suspend / 1000,
                                 p->dpms_off / 1000,
                                 False);
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
  int i;

  while (1)
    {
      Bool was_locked = False;

      if (p->verbose_p)
	fprintf (stderr, "%s: awaiting idleness.\n", blurb());

      check_for_leaks ("unblanked A");
      sleep_until_idle (si, True);
      check_for_leaks ("unblanked B");

      if (p->verbose_p)
	{
	  if (si->demoing_p)
	    fprintf (stderr, "%s: demoing %d at %s.\n", blurb(),
		     si->selection_mode, timestring());
	  else
            fprintf (stderr, "%s: blanking screen at %s.\n", blurb(),
                     timestring());
	}

      maybe_reload_init_file (si);

      if (p->mode == DONT_BLANK)
        {
          if (p->verbose_p)
            fprintf (stderr, "%s: idle with blanking disabled at %s.\n",
                     blurb(), timestring());

          /* Go around the loop and wait for the next bout of idleness,
             or for the init file to change, or for a remote command to
             come in, or something.

             But, if locked_p is true, go ahead.  This can only happen
             if we're in "disabled" mode but a "lock" clientmessage came
             in: in that case, we should go ahead and blank/lock the screen.
           */
          if (!si->locked_p)
            continue;
        }

      /* Since we're about to blank the screen, kill the de-race timer,
         if any.  It might still be running if we have unblanked and then
         re-blanked in a short period (e.g., when using the "next" button
         in xscreensaver-demo.)
       */
      if (si->de_race_id)
        {
          if (p->verbose_p)
            fprintf (stderr, "%s: stopping de-race timer (%d remaining.)\n",
                     blurb(), si->de_race_ticks);
          XtRemoveTimeOut (si->de_race_id);
          si->de_race_id = 0;
        }


      /* Now, try to blank.
       */

      if (! blank_screen (si))
        {
          /* We were unable to grab either the keyboard or mouse.
             This means we did not (and must not) blank the screen.
             If we were to blank the screen while some other program
             is holding both the mouse and keyboard grabbed, then
             we would never be able to un-blank it!  We would never
             see any events, and the display would be wedged.

             In particular, without that keyboard grab, we will be
             unable to ever read keypresses on the unlock dialog.
             You can't unlock if you can't type your password.

             So, just go around the loop again and wait for the
             next bout of idleness.  (If the user remains idle, we
             will next try to blank the screen again in no more than
             60 seconds.)
          */
          Time retry = 60 * 1000;
          if (p->timeout < retry)
            retry = p->timeout;

          if (p->debug_p)
            {
              fprintf (stderr,
                  "%s: DEBUG MODE: unable to grab -- BLANKING ANYWAY.\n",
                       blurb());
            }
          else
            {
              fprintf (stderr,
                  "%s: unable to grab keyboard or mouse!  Blanking aborted.\n",
                       blurb());

              schedule_wakeup_event (si, retry, p->debug_p);
              continue;
            }
        }

      for (i = 0; i < si->nscreens; i++)
        kill_screenhack (&si->screens[i]);

      raise_window (si, True, True, False);
      if (si->throttled_p)
        fprintf (stderr, "%s: not launching hack (throttled.)\n", blurb());
      else
        for (i = 0; i < si->nscreens; i++)
          spawn_screenhack (&si->screens[i]);

      /* If we are blanking only, optionally power down monitor right now. */
      if (p->mode == BLANK_ONLY &&
          p->dpms_enabled_p && 
          p->dpms_quickoff_p)
        {
          sync_server_dpms_settings (si->dpy, True,
                                     p->dpms_quickoff_p,
                                     p->dpms_standby / 1000,
                                     p->dpms_suspend / 1000,
                                     p->dpms_off / 1000,
                                     False);
          monitor_power_on (si, False);
        }

      /* Don't start the cycle timer in demo mode. */
      if (!si->demoing_p && p->cycle)
	si->cycle_id = XtAppAddTimeOut (si->app,
                                        (si->selection_mode
                                         /* see comment in cycle_timer() */
                                         ? 1000 * 60 * 60
                                         : p->cycle),
                                        cycle_timer,
					(XtPointer) si);


#ifndef NO_LOCKING
      /* Maybe start locking the screen.
       */
      {
        Time lock_timeout = p->lock_timeout;

        if (si->emergency_lock_p && p->lock_p && lock_timeout)
          {
            int secs = p->lock_timeout / 1000;
            if (p->verbose_p)
              fprintf (stderr,
                     "%s: locking now, instead of waiting for %d:%02d:%02d.\n",
                       blurb(),
                       (secs / (60 * 60)), ((secs / 60) % 60), (secs % 60));
            lock_timeout = 0;
          }

        si->emergency_lock_p = False;

        if (!si->demoing_p &&           /* if not going into demo mode */
            p->lock_p &&                /* and locking is enabled */
            !si->locking_disabled_p &&  /* and locking is possible */
            lock_timeout == 0)          /* and locking is not timer-deferred */
          set_locked_p (si, True);      /* then lock right now. */

        /* locked_p might be true already because of the above, or because of
           the LOCK ClientMessage.  But if not, and if we're supposed to lock
           after some time, set up a timer to do so.
        */
        if (p->lock_p &&
            !si->locked_p &&
            lock_timeout > 0)
          si->lock_id = XtAppAddTimeOut (si->app, lock_timeout,
                                         activate_lock_timer,
                                         (XtPointer) si);
      }
#endif /* !NO_LOCKING */


      ok_to_unblank = True;
      do {

        check_for_leaks ("blanked A");
	sleep_until_idle (si, False);		/* until not idle */
        check_for_leaks ("blanked B");

	maybe_reload_init_file (si);

#ifndef NO_LOCKING
        /* Maybe unlock the screen.
         */
	if (si->locked_p)
	  {
	    saver_screen_info *ssi = si->default_screen;
	    if (si->locking_disabled_p) abort ();

            was_locked = True;
	    si->dbox_up_p = True;
            for (i = 0; i < si->nscreens; i++)
              suspend_screenhack (&si->screens[i], True);	  /* suspend */
	    XUndefineCursor (si->dpy, ssi->screensaver_window);

	    ok_to_unblank = unlock_p (si);

	    si->dbox_up_p = False;
	    XDefineCursor (si->dpy, ssi->screensaver_window, ssi->cursor);
            for (i = 0; i < si->nscreens; i++)
              suspend_screenhack (&si->screens[i], False);	   /* resume */

            if (!ok_to_unblank &&
                !screenhack_running_p (si))
              {
                /* If the lock dialog has been dismissed and we're not about to
                   unlock the screen, and there is currently no hack running,
                   then launch one.  (There might be no hack running if DPMS
                   had kicked in.  But DPMS is off now, so bring back the hack)
                 */
                if (si->cycle_id)
                  XtRemoveTimeOut (si->cycle_id);
                si->cycle_id = 0;
                cycle_timer ((XtPointer) si, 0);
              }
	  }
#endif /* !NO_LOCKING */

	} while (!ok_to_unblank);


      if (p->verbose_p)
	fprintf (stderr, "%s: unblanking screen at %s.\n",
		 blurb(), timestring ());

      /* Kill before unblanking, to stop drawing as soon as possible. */
      for (i = 0; i < si->nscreens; i++)
        kill_screenhack (&si->screens[i]);
      unblank_screen (si);

      set_locked_p (si, False);
      si->emergency_lock_p = False;
      si->demoing_p = 0;
      si->selection_mode = 0;

      /* If we're throttled, and the user has explicitly unlocked the screen,
         then unthrottle.  If we weren't locked, then don't unthrottle
         automatically, because someone might have just bumped the desk... */
      if (was_locked)
        {
          if (si->throttled_p && p->verbose_p)
            fprintf (stderr, "%s: unthrottled.\n", blurb());
          si->throttled_p = False;
        }

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

      /* Since we're unblanked now, break race conditions and make
         sure we stay that way (see comment in timers.c.) */
      if (! si->de_race_id)
        de_race_timer ((XtPointer) si, 0);
    }
}

static void analyze_display (saver_info *si);
static void fix_fds (void);

int
main (int argc, char **argv)
{
  Widget shell;
  saver_info the_si;
  saver_info *si = &the_si;
  saver_preferences *p = &si->prefs;
  struct passwd *spasswd;
  int i;

  /* It turns out that if we do setlocale (LC_ALL, "") here, people
     running in Japanese locales get font craziness on the password
     dialog, presumably because it is displaying Japanese characters
     in a non-Japanese font.  However, if we don't call setlocale()
     at all, then XLookupString() never returns multi-byte UTF-8
     characters when people type non-Latin1 characters on the
     keyboard.

     The current theory (and at this point, I'm really guessing!) is
     that using LC_CTYPE instead of LC_ALL will make XLookupString()
     behave usefully, without having the side-effect of screwing up
     the fonts on the unlock dialog.

     See https://bugs.launchpad.net/ubuntu/+source/xscreensaver/+bug/671923
     from comment #20 onward.

       -- jwz, 24-Sep-2011
   */
#ifdef ENABLE_NLS
  if (!setlocale (LC_CTYPE, ""))
    fprintf (stderr, "%s: warning: could not set default locale\n",
             progname);

  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  textdomain (GETTEXT_PACKAGE);
#endif /* ENABLE_NLS */

  memset(si, 0, sizeof(*si));
  global_si_kludge = si;	/* I hate C so much... */

  fix_fds();

# undef ya_rand_init
  ya_rand_init (0);

  save_argv (argc, argv);
  set_version_string (si, &argc, argv);
  privileged_initialization (si, &argc, argv);
  hack_environment (si);

  spasswd = getpwuid(getuid());
  if (!spasswd)
    {
      fprintf(stderr, "Could not figure out who the current user is!\n");
      return 1;
    }

  si->user = strdup(spasswd->pw_name ? spasswd->pw_name : "(unknown)");

# ifndef NO_LOCKING
  si->unlock_cb = gui_auth_conv;
  si->auth_finished_cb = auth_finished_cb;
# endif /* !NO_LOCKING */

  shell = connect_to_server (si, &argc, argv);
  process_command_line (si, &argc, argv);
  stderr_log_file (si);
  print_banner (si);

  load_init_file(si->dpy, p); /* must be before initialize_per_screen_info() */
  blurb_timestamp_p = p->timestamp_p;  /* kludge */
  initialize_per_screen_info (si, shell); /* also sets si->fading_possible_p */

  /* We can only issue this warning now. */
  if (p->verbose_p && !si->fading_possible_p && (p->fade_p || p->unfade_p))
    fprintf (stderr,
             "%s: there are no PseudoColor or GrayScale visuals.\n"
             "%s: ignoring the request for fading/unfading.\n",
             blurb(), blurb());

  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      if (ssi->real_screen_p)
        if (ensure_no_screensaver_running (si->dpy, si->screens[i].screen))
          exit (1);
    }

  lock_initialization (si, &argc, argv);
  print_lock_failure_banner (si);

  if (p->xsync_p) XSynchronize (si->dpy, True);

  if (p->verbose_p) analyze_display (si);
  initialize_server_extensions (si);

  si->blank_time = time ((time_t *) 0); /* must be before ..._window */
  initialize_screensaver_window (si);

  select_events (si);
  init_sigchld ();

  disable_builtin_screensaver (si, True);
  sync_server_dpms_settings (si->dpy,
                             (p->dpms_enabled_p  &&
                              p->mode != DONT_BLANK),
                             p->dpms_quickoff_p,
                             p->dpms_standby / 1000,
                             p->dpms_suspend / 1000,
                             p->dpms_off / 1000,
                             False);

  initialize_stderr (si);
  handle_signals (si);

  make_splash_dialog (si);

  main_loop (si);		/* doesn't return */
  return 0;
}

static void
fix_fds (void)
{
  /* Bad Things Happen if stdin, stdout, and stderr have been closed
     (as by the `sh incantation "xscreensaver >&- 2>&-").  When you do
     that, the X connection gets allocated to one of these fds, and
     then some random library writes to stderr, and random bits get
     stuffed down the X pipe, causing "Xlib: sequence lost" errors.
     So, we cause the first three file descriptors to be open to
     /dev/null if they aren't open to something else already.  This
     must be done before any other files are opened (or the closing
     of that other file will again free up one of the "magic" first
     three FDs.)

     We do this by opening /dev/null three times, and then closing
     those fds, *unless* any of them got allocated as #0, #1, or #2,
     in which case we leave them open.  Gag.

     Really, this crap is technically required of *every* X program,
     if you want it to be robust in the face of "2>&-".
   */
  int fd0 = open ("/dev/null", O_RDWR);
  int fd1 = open ("/dev/null", O_RDWR);
  int fd2 = open ("/dev/null", O_RDWR);
  if (fd0 > 2) close (fd0);
  if (fd1 > 2) close (fd1);
  if (fd2 > 2) close (fd2);
}



/* Processing ClientMessage events.
 */


static Bool error_handler_hit_p = False;

static int
ignore_all_errors_ehandler (Display *dpy, XErrorEvent *error)
{
  error_handler_hit_p = True;
  return 0;
}

/* Sometimes some systems send us ClientMessage events with bogus atoms in
   them.  We only look up the atom names for printing warning messages,
   so don't bomb out when it happens...
 */
static char *
XGetAtomName_safe (Display *dpy, Atom atom)
{
  char *result;
  XErrorHandler old_handler;
  if (!atom) return 0;

  XSync (dpy, False);
  error_handler_hit_p = False;
  old_handler = XSetErrorHandler (ignore_all_errors_ehandler);
  result = XGetAtomName (dpy, atom);
  XSync (dpy, False);
  XSetErrorHandler (old_handler);
  XSync (dpy, False);
  if (error_handler_hit_p) result = 0;

  if (result)
    return result;
  else
    {
      char buf[100];
      sprintf (buf, "<<undefined atom 0x%04X>>", (unsigned int) atom);
      return strdup (buf);
    }
}


static void
clientmessage_response (saver_info *si, Window w, Bool error,
			const char *stderr_msg,
			const char *protocol_msg)
{
  char *proto;
  int L;
  saver_preferences *p = &si->prefs;
  XErrorHandler old_handler;

  if (error || p->verbose_p)
    fprintf (stderr, "%s: %s\n", blurb(), stderr_msg);

  L = strlen(protocol_msg);
  proto = (char *) malloc (L + 2);
  proto[0] = (error ? '-' : '+');
  strcpy (proto+1, protocol_msg);
  L++;

  /* Ignore all X errors while sending a response to a ClientMessage.
     Pretty much the only way we could get an error here is if the
     window we're trying to send the reply on has been deleted, in
     which case, the sender of the ClientMessage won't see our response
     anyway.
   */
  XSync (si->dpy, False);
  error_handler_hit_p = False;
  old_handler = XSetErrorHandler (ignore_all_errors_ehandler);

  XChangeProperty (si->dpy, w, XA_SCREENSAVER_RESPONSE, XA_STRING, 8,
		   PropModeReplace, (unsigned char *) proto, L);

  XSync (si->dpy, False);
  XSetErrorHandler (old_handler);
  XSync (si->dpy, False);

  free (proto);
}


static void
bogus_clientmessage_warning (saver_info *si, XEvent *event)
{
#if 0  /* Oh, fuck it.  GNOME likes to spew random ClientMessages at us
          all the time.  This is presumably indicative of an error in
          the sender of that ClientMessage: if we're getting it and 
          ignoring it, then it's not reaching the intended recipient.
          But people complain to me about this all the time ("waaah!
          xscreensaver is printing to it's stderr and that gets my
          panties all in a bunch!")  And I'm sick of hearing about it.
          So we'll just ignore these messages and let GNOME go right
          ahead and continue to stumble along in its malfunction.
        */

  saver_preferences *p = &si->prefs;
  char *str = XGetAtomName_safe (si->dpy, event->xclient.message_type);
  Window w = event->xclient.window;
  char wdesc[255];
  int screen = 0;
  Bool root_p = False;

  *wdesc = 0;
  for (screen = 0; screen < si->nscreens; screen++)
    if (w == si->screens[screen].screensaver_window)
      {
        strcpy (wdesc, "xscreensaver");
        break;
      }
    else if (w == RootWindow (si->dpy, screen))
      {
        strcpy (wdesc, "root");
        root_p = True;
        break;
      }

  /* If this ClientMessage was sent to the real root window instead of to the
     xscreensaver window, then it might be intended for someone else who is
     listening on the root window (e.g., the window manager).  So only print
     the warning if: we are in debug mode; or if the bogus message was
     actually sent to one of the xscreensaver-created windows.
   */
  if (root_p && !p->debug_p)
    return;

  if (!*wdesc)
    {
      XErrorHandler old_handler;
      XClassHint hint;
      XWindowAttributes xgwa;
      memset (&hint, 0, sizeof(hint));
      memset (&xgwa, 0, sizeof(xgwa));

      XSync (si->dpy, False);
      old_handler = XSetErrorHandler (ignore_all_errors_ehandler);
      XGetClassHint (si->dpy, w, &hint);
      XGetWindowAttributes (si->dpy, w, &xgwa);
      XSync (si->dpy, False);
      XSetErrorHandler (old_handler);
      XSync (si->dpy, False);

      screen = (xgwa.screen ? screen_number (xgwa.screen) : -1);

      sprintf (wdesc, "%.20s / %.20s",
               (hint.res_name  ? hint.res_name  : "(null)"),
               (hint.res_class ? hint.res_class : "(null)"));
      if (hint.res_name)  XFree (hint.res_name);
      if (hint.res_class) XFree (hint.res_class);
    }

  fprintf (stderr, "%s: %d: unrecognised ClientMessage \"%s\" received\n",
           blurb(), screen, (str ? str : "(null)"));
  fprintf (stderr, "%s: %d: for window 0x%lx (%s)\n",
           blurb(), screen, (unsigned long) w, wdesc);
  if (str) XFree (str);

#endif /* 0 */
}


Bool
handle_clientmessage (saver_info *si, XEvent *event, Bool until_idle_p)
{
  saver_preferences *p = &si->prefs;
  Atom type = 0;
  Window window = event->xclient.window;

  /* Preferences might affect our handling of client messages. */
  maybe_reload_init_file (si);

  if (event->xclient.message_type != XA_SCREENSAVER ||
      event->xclient.format != 32)
    {
      bogus_clientmessage_warning (si, event);
      return False;
    }

  type = event->xclient.data.l[0];
  if (type == XA_ACTIVATE)
    {
      if (until_idle_p)
	{
          if (p->mode == DONT_BLANK)
            {
              clientmessage_response(si, window, True,
                         "ACTIVATE ClientMessage received in DONT_BLANK mode.",
                                     "screen blanking is currently disabled.");
              return False;
            }

	  clientmessage_response(si, window, False,
				 "ACTIVATE ClientMessage received.",
				 "activating.");
	  si->selection_mode = 0;
	  si->demoing_p = False;

          if (si->throttled_p && p->verbose_p)
            fprintf (stderr, "%s: unthrottled.\n", blurb());
	  si->throttled_p = False;

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
# if 0
      /* When -deactivate is received while locked, pop up the dialog box
         instead of just ignoring it.  Some people depend on this behavior
         to be able to unlock by using e.g. a fingerprint reader without
         also having to click the mouse first.
       */
      if (si->locked_p) 
        {
          clientmessage_response(si, window, False,
              "DEACTIVATE ClientMessage received while locked: ignored.",
              "screen is locked.");
        }
      else
# endif /* 0 */
        {
          if (! until_idle_p)
            {
              if (si->throttled_p && p->verbose_p)
                fprintf (stderr, "%s: unthrottled.\n", blurb());
              si->throttled_p = False;

              clientmessage_response(si, window, False,
                                     "DEACTIVATE ClientMessage received.",
                                     "deactivating.");
              if (si->using_mit_saver_extension ||
                  si->using_sgi_saver_extension)
                {
                  XForceScreenSaver (si->dpy, ScreenSaverReset);
                  return False;
                }
              else
                {
                  return True;
                }
            }
          clientmessage_response(si, window, False,
                          "ClientMessage DEACTIVATE received while inactive: "
                          "resetting idle timer.",
                                 "not active: idle timer reset.");
          reset_timers (si);
        }
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

          if (si->throttled_p && p->verbose_p)
            fprintf (stderr, "%s: unthrottled.\n", blurb());
	  si->throttled_p = False;

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

      if (si->throttled_p && p->verbose_p)
        fprintf (stderr, "%s: unthrottled.\n", blurb());
      si->throttled_p = False;

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

      if (p->mode == DONT_BLANK)
        {
          clientmessage_response(si, window, True,
                           "SELECT ClientMessage received in DONT_BLANK mode.",
                                 "screen blanking is currently disabled.");
          return False;
        }

      sprintf (buf, "SELECT %ld ClientMessage received.", which);
      sprintf (buf2, "activating (%ld).", which);
      clientmessage_response (si, window, False, buf, buf2);

      if (which < 0) which = 0;		/* 0 == "random" */
      si->selection_mode = which;
      si->demoing_p = False;

      if (si->throttled_p && p->verbose_p)
        fprintf (stderr, "%s: unthrottled.\n", blurb());
      si->throttled_p = False;

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
              int i;
              for (i = 0; i < si->nscreens; i++)
                kill_screenhack (&si->screens[i]);
	      unblank_screen (si);
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
              int i;
              for (i = 0; i < si->nscreens; i++)
                kill_screenhack (&si->screens[i]);
	      unblank_screen (si);
	      XSync (si->dpy, False);
	    }

	  restart_process (si);  /* does not return */
          abort();
	}
      else
	clientmessage_response (si, window, True,
				"RESTART ClientMessage received while locked.",
				"screen is locked.");
    }
  else if (type == XA_DEMO)
    {
      long arg = event->xclient.data.l[1];
      Bool demo_one_hack_p = (arg == 5000);

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

              if (si->throttled_p && p->verbose_p)
                fprintf (stderr, "%s: unthrottled.\n", blurb());
              si->throttled_p = False;

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
	  sprintf (buf, "LOCK ClientMessage received; %s", response);
	  clientmessage_response (si, window, False, buf, response);
	  set_locked_p (si, True);
	  si->selection_mode = 0;
	  si->demoing_p = False;

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
  else if (type == XA_THROTTLE)
    {
      /* The THROTTLE command is deprecated -- it predates the XDPMS
         extension.  Instead of using -throttle, users should instead
         just power off the monitor (e.g., "xset dpms force off".)
         In a few minutes, xscreensaver will notice that the monitor
         is off, and cease running hacks.
       */
      if (si->throttled_p)
	clientmessage_response (si, window, True,
                                "THROTTLE ClientMessage received, but "
                                "already throttled.",
                                "already throttled.");
      else
	{
	  char buf [255];
	  char *response = "throttled.";
	  si->throttled_p = True;
	  si->selection_mode = 0;
	  si->demoing_p = False;
	  sprintf (buf, "THROTTLE ClientMessage received; %s", response);
	  clientmessage_response (si, window, False, buf, response);

          if (! until_idle_p)
            {
              if (si->cycle_id)
                XtRemoveTimeOut (si->cycle_id);
              si->cycle_id = 0;
              cycle_timer ((XtPointer) si, 0);
            }
	}
    }
  else if (type == XA_UNTHROTTLE)
    {
      if (! si->throttled_p)
	clientmessage_response (si, window, True,
                                "UNTHROTTLE ClientMessage received, but "
                                "not throttled.",
                                "not throttled.");
      else
	{
	  char buf [255];
	  char *response = "unthrottled.";
	  si->throttled_p = False;
	  si->selection_mode = 0;
	  si->demoing_p = False;
	  sprintf (buf, "UNTHROTTLE ClientMessage received; %s", response);
	  clientmessage_response (si, window, False, buf, response);

          if (! until_idle_p)
            {
              if (si->cycle_id)
                XtRemoveTimeOut (si->cycle_id);
              si->cycle_id = 0;
              cycle_timer ((XtPointer) si, 0);
            }
	}
    }
  else
    {
      char buf [1024];
      char *str;
      str = XGetAtomName_safe (si->dpy, type);

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
  static struct {
    const char *name; const char *desc; 
    Bool useful_p;
    Status (*version_fn) (Display *, int *majP, int *minP);
  } exts[] = {

   { "SCREEN_SAVER", /* underscore */           "SGI Screen-Saver",
#     ifdef HAVE_SGI_SAVER_EXTENSION
        True,  0
#     else
        False, 0
#     endif
   }, { "SCREEN-SAVER", /* dash */              "SGI Screen-Saver",
#     ifdef HAVE_SGI_SAVER_EXTENSION
        True,  0
#     else
        False, 0
#     endif
   }, { "MIT-SCREEN-SAVER",                     "MIT Screen-Saver",
#     ifdef HAVE_MIT_SAVER_EXTENSION
        True,  XScreenSaverQueryVersion
#     else
        False, 0
#     endif
   }, { "XIDLE",                                "XIdle",           
#     ifdef HAVE_XIDLE_EXTENSION
        True,  0
#     else
        False, 0
#     endif
   }, { "SGI-VIDEO-CONTROL",                    "SGI Video-Control",
#     ifdef HAVE_SGI_VC_EXTENSION
        True,  XSGIvcQueryVersion
#     else
        False, 0
#     endif
   }, { "READDISPLAY",                          "SGI Read-Display",
#     ifdef HAVE_READ_DISPLAY_EXTENSION
        True,  XReadDisplayQueryVersion
#     else
        False, 0
#     endif
   }, { "MIT-SHM",                              "Shared Memory",   
#     ifdef HAVE_XSHM_EXTENSION
        True, (Status (*) (Display*,int*,int*)) XShmQueryVersion /* 4 args */
#     else
        False, 0
#     endif
   }, { "DOUBLE-BUFFER",                        "Double-Buffering",
#     ifdef HAVE_DOUBLE_BUFFER_EXTENSION
        True, XdbeQueryExtension
#     else
        False, 0
#     endif
   }, { "DPMS",                                 "Power Management",
#     ifdef HAVE_DPMS_EXTENSION
        True,  DPMSGetVersion
#     else
        False, 0
#     endif
   }, { "GLX",                                  "GLX",             
#     ifdef HAVE_GL
        True,  0
#     else
        False, 0
#     endif
   }, { "XFree86-VidModeExtension",             "XF86 Video-Mode", 
#     ifdef HAVE_XF86VMODE
        True,  XF86VidModeQueryVersion
#     else
        False, 0
#     endif
   }, { "XC-VidModeExtension",                  "XC Video-Mode", 
#     ifdef HAVE_XF86VMODE
        True,  XF86VidModeQueryVersion
#     else
        False, 0
#     endif
   }, { "XFree86-MISC",                         "XF86 Misc", 
#     ifdef HAVE_XF86MISCSETGRABKEYSSTATE
        True,  XF86MiscQueryVersion
#     else
        False, 0
#     endif
   }, { "XC-MISC",                              "XC Misc", 
#     ifdef HAVE_XF86MISCSETGRABKEYSSTATE
        True,  XF86MiscQueryVersion
#     else
        False, 0
#     endif
   }, { "XINERAMA",                             "Xinerama",
#     ifdef HAVE_XINERAMA
        True,  XineramaQueryVersion
#     else
        False, 0
#     endif
   }, { "RANDR",                                "Resize-and-Rotate",
#     ifdef HAVE_RANDR
        True,  XRRQueryVersion
#     else
        False, 0
#     endif
   }, { "DRI",		                        "DRI",
        True,  0
   }, { "NV-CONTROL",                           "NVidia",
        True,  0
   }, { "NV-GLX",                               "NVidia GLX",
        True,  0
   }, { "Apple-DRI",                            "Apple-DRI (XDarwin)",
        True,  0
   }, { "XInputExtension",                      "XInput",
        True,  0
   },
  };

  fprintf (stderr, "%s: running on display \"%s\"\n", blurb(), 
           DisplayString(si->dpy));
  fprintf (stderr, "%s: vendor is %s, %d.\n", blurb(),
	   ServerVendor(si->dpy), VendorRelease(si->dpy));

  fprintf (stderr, "%s: useful extensions:\n", blurb());
  for (i = 0; i < countof(exts); i++)
    {
      int op = 0, event = 0, error = 0;
      char buf [255];
      int maj = 0, min = 0;
      int dummy1, dummy2, dummy3;
      int j;

      /* Most of the extension version functions take 3 args,
         writing results into args 2 and 3, but some take more.
         We only ever care about the first two results, but we
         pass in three extra pointers just in case.
       */
      Status (*version_fn_2) (Display*,int*,int*,int*,int*,int*) =
        (Status (*) (Display*,int*,int*,int*,int*,int*)) exts[i].version_fn;

      if (!XQueryExtension (si->dpy, exts[i].name, &op, &event, &error))
        continue;
      sprintf (buf, "%s:   ", blurb());
      j = strlen (buf);
      strcat (buf, exts[i].desc);

      if (!version_fn_2)
        ;
      else if (version_fn_2 (si->dpy, &maj, &min, &dummy1, &dummy2, &dummy3))
        sprintf (buf+strlen(buf), " (%d.%d)", maj, min);
      else
        strcat (buf, " (unavailable)");

      if (!exts[i].useful_p)
        strcat (buf, " (disabled at compile time)");
      fprintf (stderr, "%s\n", buf);
    }

  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      unsigned long colormapped_depths = 0;
      unsigned long non_mapped_depths = 0;
      XVisualInfo vi_in, *vi_out;
      int out_count;

      if (!ssi->real_screen_p) continue;

      vi_in.screen = ssi->real_screen_number;
      vi_out = XGetVisualInfo (si->dpy, VisualScreenMask, &vi_in, &out_count);
      if (!vi_out) continue;
      for (j = 0; j < out_count; j++) {
	if (vi_out[j].depth >= 32) continue;
	if (vi_out[j].class == PseudoColor)
	  colormapped_depths |= (1 << vi_out[j].depth);
	else
	  non_mapped_depths  |= (1 << vi_out[j].depth);
	}
      XFree ((char *) vi_out);

      if (colormapped_depths)
	{
	  fprintf (stderr, "%s: screen %d colormapped depths:", blurb(),
                   ssi->real_screen_number);
	  for (j = 0; j < 32; j++)
	    if (colormapped_depths & (1 << j))
	      fprintf (stderr, " %d", j);
	  fprintf (stderr, ".\n");
	}
      if (non_mapped_depths)
	{
	  fprintf (stderr, "%s: screen %d non-colormapped depths:",
                   blurb(), ssi->real_screen_number);
	  for (j = 0; j < 32; j++)
	    if (non_mapped_depths & (1 << j))
	      fprintf (stderr, " %d", j);
	  fprintf (stderr, ".\n");
	}
    }

  describe_monitor_layout (si);
}


Bool
display_is_on_console_p (saver_info *si)
{
  Bool not_on_console = True;
  char *dpystr = DisplayString (si->dpy);
  char *tail = (char *) strchr (dpystr, ':');
  if (! tail || strncmp (tail, ":0", 2))
    not_on_console = True;
  else
    {
      char dpyname[255], localname[255];
      strncpy (dpyname, dpystr, tail-dpystr);
      dpyname [tail-dpystr] = 0;
      if (!*dpyname ||
	  !strcmp(dpyname, "unix") ||
	  !strcmp(dpyname, "localhost"))
	not_on_console = False;
      else if (gethostname (localname, sizeof (localname)))
	not_on_console = True;  /* can't find hostname? */
      else if (!strncmp (dpyname, "/tmp/launch-", 12))  /* MacOS X launchd */
	not_on_console = False;
      else
	{
	  /* We have to call gethostbyname() on the result of gethostname()
	     because the two aren't guarenteed to be the same name for the
	     same host: on some losing systems, one is a FQDN and the other
	     is not.  Here in the wide wonderful world of Unix it's rocket
	     science to obtain the local hostname in a portable fashion.
	     
	     And don't forget, gethostbyname() reuses the structure it
	     returns, so we have to copy the fucker before calling it again.
	     Thank you master, may I have another.
	   */
	  struct hostent *h = gethostbyname (dpyname);
	  if (!h)
	    not_on_console = True;
	  else
	    {
	      char hn [255];
	      struct hostent *l;
	      strcpy (hn, h->h_name);
	      l = gethostbyname (localname);
	      not_on_console = (!l || !!(strcmp (l->h_name, hn)));
	    }
	}
    }
  return !not_on_console;
}


/* Do a little bit of heap introspection...
 */
void
check_for_leaks (const char *where)
{
#if defined(HAVE_SBRK) && defined(LEAK_PARANOIA)
  static unsigned long last_brk = 0;
  int b = (unsigned long) sbrk(0);
  if (last_brk && last_brk < b)
    fprintf (stderr, "%s: %s: brk grew by %luK.\n",
             blurb(), where,
             (((b - last_brk) + 1023) / 1024));
  last_brk = b;
#endif /* HAVE_SBRK */
}
