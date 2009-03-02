/* xscreensaver, Copyright (c) 1991-1998 Jamie Zawinski <jwz@netscape.com>
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

saver_info *global_si_kludge = 0;	/* I hate C so much... */

char *progname = 0;
char *progclass = 0;
XrmDatabase db = 0;


static Atom XA_ACTIVATE, XA_DEACTIVATE, XA_CYCLE, XA_NEXT, XA_PREV;
static Atom XA_EXIT, XA_RESTART, XA_DEMO, XA_LOCK;


static XrmOptionDescRec options [] = {
  { "-timeout",		   ".timeout",		XrmoptionSepArg, 0 },
  { "-cycle",		   ".cycle",		XrmoptionSepArg, 0 },
  { "-lock-mode",	   ".lock",		XrmoptionNoArg, "on" },
  { "-no-lock-mode",	   ".lock",		XrmoptionNoArg, "off" },
  { "-lock-timeout",	   ".lockTimeout",	XrmoptionSepArg, 0 },
  { "-visual",		   ".visualID",		XrmoptionSepArg, 0 },
  { "-install",		   ".installColormap",	XrmoptionNoArg, "on" },
  { "-no-install",	   ".installColormap",	XrmoptionNoArg, "off" },
  { "-verbose",		   ".verbose",		XrmoptionNoArg, "on" },
  { "-silent",		   ".verbose",		XrmoptionNoArg, "off" },
  { "-xidle-extension",	   ".xidleExtension",	XrmoptionNoArg, "on" },
  { "-no-xidle-extension", ".xidleExtension",	XrmoptionNoArg, "off" },
  { "-mit-extension",	   ".mitSaverExtension",XrmoptionNoArg, "on" },
  { "-no-mit-extension",   ".mitSaverExtension",XrmoptionNoArg, "off" },
  { "-sgi-extension",	   ".sgiSaverExtension",XrmoptionNoArg, "on" },
  { "-no-sgi-extension",   ".sgiSaverExtension",XrmoptionNoArg, "off" },
  { "-idelay",		   ".initialDelay",	XrmoptionSepArg, 0 },
  { "-nice",		   ".nice",		XrmoptionSepArg, 0 }
};

static char *defaults[] = {
#include "XScreenSaver_ad.h"
 0
};

static void
do_help (saver_info *si)
{
  printf ("\
xscreensaver %s, copyright (c) 1991-1998 by Jamie Zawinski <jwz@netscape.com>\n\
The standard Xt command-line options are accepted; other options include:\n\
\n\
    -timeout <minutes>         When the screensaver should activate.\n\
    -cycle <minutes>           How long to let each hack run.\n\
    -lock-mode                 Require a password before deactivating.\n\
    -no-lock-mode              Don't.\n\
    -lock-timeout <minutes>    Grace period before locking; default 0.\n\
    -visual <id-or-class>      Which X visual to run on.\n\
    -install                   Install a private colormap.\n\
    -no-install                Don't.\n\
    -verbose                   Be loud.\n\
    -silent                    Don't.\n\
    -mit-extension             Use the R6 MIT_SCREEN_SAVER server extension.\n\
    -no-mit-extension          Don't.\n\
    -sgi-extension             Use the SGI SCREEN-SAVER server extension.\n\
    -no-sgi-extension          Don't.\n\
    -xidle-extension           Use the R5 XIdle server extension.\n\
    -no-xidle-extension        Don't.\n\
    -help                      This message.\n\
\n\
The `xscreensaver' program should be left running in the background.\n\
Use the `xscreensaver-command' program to manipulate a running xscreensaver.\n\
\n\
The `*programs' resource controls which graphics demos will be launched by\n\
the screensaver.  See `man xscreensaver' or the web page for more details.\n\
\n\
For updates, check http://people.netscape.com/jwz/xscreensaver/\n\n",
	  si->version);

#ifdef NO_LOCKING
  printf ("Support for locking was not enabled at compile-time.\n");
#endif
#ifdef NO_DEMO_MODE
  printf ("Support for demo mode was not enabled at compile-time.\n");
#endif
#if !defined(HAVE_XIDLE_EXTENSION) && !defined(HAVE_MIT_SAVER_EXTENSION) && !defined(HAVE_SGI_SAVER_EXTENSION)
  printf ("Support for the XIDLE, SCREEN_SAVER, and MIT-SCREEN-SAVER server\
 extensions\nwas not enabled at compile-time.\n");
#endif /* !HAVE_XIDLE_EXTENSION && !HAVE_MIT_SAVER_EXTENSION && !HAVE_SGI_SAVER_EXTENSION */

  fflush (stdout);
  exit (1);
}


static char *
reformat_hack(const char *hack)
{
  int i;
  const char *in = hack;
  int indent = 13;
  char *h2 = (char *) malloc(strlen(in) + indent + 2);
  char *out = h2;

  while (isspace(*in)) in++;		/* skip whitespace */
  while (*in && !isspace(*in) && *in != ':')
    *out++ = *in++;			/* snarf first token */
  while (isspace(*in)) in++;		/* skip whitespace */

  if (*in == ':')
    *out++ = *in++;			/* copy colon */
  else
    {
      in = hack;
      out = h2;				/* reset to beginning */
    }

  *out = 0;

  while (isspace(*in)) in++;		/* skip whitespace */
  for (i = strlen(h2); i < indent; i++)	/* indent */
    *out++ = ' ';

  while (*in) *out++ = *in++;		/* copy rest of line */
  *out = 0;

  return h2;
}


static void
get_screenhacks (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  int i = 0;
  int hacks_size = 60;
  int size;
  char *d;

  d = get_string_resource ("monoPrograms", "MonoPrograms");
  if (d && !*d) { free(d); d = 0; }
  if (!d)
    d = get_string_resource ("colorPrograms", "ColorPrograms");
  if (d && !*d) { free(d); d = 0; }

  if (d)
    {
      fprintf (stderr,
       "%s: the `monoPrograms' and `colorPrograms' resources are obsolete;\n\
	see the manual for details.\n", progname);
      free(d);
    }

  d = get_string_resource ("programs", "Programs");

  size = d ? strlen (d) : 0;
  p->screenhacks = (char **) malloc (sizeof (char *) * hacks_size);
  p->screenhacks_count = 0;

  while (i < size)
    {
      int end, start = i;
      if (d[i] == ' ' || d[i] == '\t' || d[i] == '\n' || d[i] == 0)
	{
	  i++;
	  continue;
	}
      if (hacks_size <= p->screenhacks_count)
	p->screenhacks = (char **) realloc (p->screenhacks,
					    (hacks_size = hacks_size * 2) *
					    sizeof (char *));
      p->screenhacks [p->screenhacks_count++] = d + i;
      while (d[i] != 0 && d[i] != '\n')
	i++;
      end = i;
      while (i > start && (d[i-1] == ' ' || d[i-1] == '\t'))
	i--;
      d[i] = 0;
      i = end + 1;
    }

  /* shrink all whitespace to one space, for the benefit of the "demo"
     mode display.  We only do this when we can easily tell that the
     whitespace is not significant (no shell metachars).
   */
  for (i = 0; i < p->screenhacks_count; i++)
    {
      char *s = p->screenhacks [i];
      char *s2;
      int L = strlen (s);
      int j, k;
      for (j = 0; j < L; j++)
	{
	  switch (s[j])
	    {
	    case '\'': case '"': case '`': case '\\':
	      goto DONE;
	    case '\t':
	      s[j] = ' ';
	    case ' ':
	      k = 0;
	      for (s2 = s+j+1; *s2 == ' ' || *s2 == '\t'; s2++)
		k++;
	      if (k > 0)
		{
		  for (s2 = s+j+1; s2[k]; s2++)
		    *s2 = s2[k];
		  *s2 = 0;
		}
	      break;
	    }
	}
    DONE:
      p->screenhacks[i] = reformat_hack(s);  /* mallocs */
    }

  if (p->screenhacks_count)
    {
      /* Shrink down the screenhacks array to be only as big as it needs to.
	 This doesn't really matter at all. */
      p->screenhacks = (char **)
	realloc (p->screenhacks, ((p->screenhacks_count + 1) *
				  sizeof(char *)));
      p->screenhacks [p->screenhacks_count] = 0;
    }
  else
    {
      free (p->screenhacks);
      p->screenhacks = 0;
    }
}


static void
get_resources (saver_info *si)
{
  char *s;
  saver_preferences *p = &si->prefs;

  p->verbose_p	    = get_boolean_resource ("verbose", "Boolean");
  p->lock_p	    = get_boolean_resource ("lock", "Boolean");
  p->fade_p	    = get_boolean_resource ("fade", "Boolean");
  p->unfade_p	    = get_boolean_resource ("unfade", "Boolean");
  p->fade_seconds   = get_seconds_resource ("fadeSeconds", "Time");
  p->fade_ticks	    = get_integer_resource ("fadeTicks", "Integer");
  p->install_cmap_p = get_boolean_resource ("installColormap", "Boolean");
  p->nice_inferior  = get_integer_resource ("nice", "Nice");

  p->initial_delay  = get_seconds_resource ("initialDelay", "Time");
  p->timeout        = 1000 * get_minutes_resource ("timeout", "Time");
  p->lock_timeout   = 1000 * get_minutes_resource ("lockTimeout", "Time");
  p->cycle          = 1000 * get_minutes_resource ("cycle", "Time");

#ifndef NO_LOCKING
  p->passwd_timeout = 1000 * get_seconds_resource ("passwdTimeout", "Time");
#endif

  p->pointer_timeout = 1000 * get_seconds_resource ("pointerPollTime", "Time");
  p->notice_events_timeout = 1000*get_seconds_resource("windowCreationTimeout",
						       "Time");
  p->shell = get_string_resource ("bourneShell", "BourneShell");


  /* don't set use_xidle_extension unless it is explicitly specified */
  if ((s = get_string_resource ("xidleExtension", "Boolean")))
    p->use_xidle_extension = get_boolean_resource ("xidleExtension","Boolean");
  else
#ifdef HAVE_XIDLE_EXTENSION		/* pick a default */
    p->use_xidle_extension = True;	/* if we have it, use it */
#else  /* !HAVE_XIDLE_EXTENSION */
    p->use_xidle_extension = False;
#endif /* !HAVE_XIDLE_EXTENSION */
  if (s) free (s);

  /* don't set use_mit_extension unless it is explicitly specified */
  if ((s = get_string_resource ("mitSaverExtension", "Boolean")))
    p->use_mit_saver_extension = get_boolean_resource ("mitSaverExtension",
						       "Boolean");
  else
#ifdef HAVE_MIT_SAVER_EXTENSION		/* pick a default */
    p->use_mit_saver_extension = False;	/* Default false, because it sucks */
#else  /* !HAVE_MIT_SAVER_EXTENSION */
    p->use_mit_saver_extension = False;
#endif /* !HAVE_MIT_SAVER_EXTENSION */
  if (s) free (s);


  /* don't set use_mit_extension unless it is explicitly specified */
  if ((s = get_string_resource ("sgiSaverExtension", "Boolean")))
    p->use_sgi_saver_extension = get_boolean_resource ("sgiSaverExtension",
						       "Boolean");
  else
#ifdef HAVE_SGI_SAVER_EXTENSION		/* pick a default */
    p->use_sgi_saver_extension = True;	/* if we have it, use it */
#else  /* !HAVE_SGI_SAVER_EXTENSION */
    p->use_sgi_saver_extension = False;
#endif /* !HAVE_SGI_SAVER_EXTENSION */
  if (s) free (s);


  /* Throttle the various timeouts to reasonable values.
   */
#ifndef NO_LOCKING
  if (p->passwd_timeout == 0) p->passwd_timeout = 30000;	 /* 30 secs */
#endif
  if (p->timeout < 10000) p->timeout = 10000;			 /* 10 secs */
  if (p->cycle != 0 && p->cycle < 2000) p->cycle = 2000;	 /*  2 secs */
  if (p->pointer_timeout == 0) p->pointer_timeout = 5000;	 /*  5 secs */
  if (p->notice_events_timeout == 0)
    p->notice_events_timeout = 10000;				 /* 10 secs */
  if (p->fade_seconds == 0 || p->fade_ticks == 0)
    p->fade_p = False;
  if (! p->fade_p) p->unfade_p = False;

  p->watchdog_timeout = p->cycle;
  if (p->watchdog_timeout < 30000) p->watchdog_timeout = 30000;	  /* 30 secs */
  if (p->watchdog_timeout > 3600000) p->watchdog_timeout = 3600000; /*  1 hr */

#ifdef NO_LOCKING
  si->locking_disabled_p = True;
  si->nolock_reason = "not compiled with locking support";
  if (p->lock_p)
    {
      p->lock_p = False;
      fprintf (stderr, "%s: not compiled with support for locking.\n",
	       progname);
    }
#else  /* ! NO_LOCKING */
  if (p->lock_p && si->locking_disabled_p)
    {
      fprintf (stderr, "%s: locking is disabled (%s).\n", progname,
	       si->nolock_reason);
      p->lock_p = False;
    }
#endif /* ! NO_LOCKING */

  get_screenhacks (si);

  if (p->debug_p)
    {
      XSynchronize(si->dpy, True);
      p->verbose_p = True;
      p->initial_delay = 0;
    }
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

static void initialize (saver_info *si, int argc, char **argv);
static void main_loop (saver_info *si);

int
main (int argc, char **argv)
{
  saver_info si;
  memset(&si, 0, sizeof(si));
  global_si_kludge = &si;	/* I hate C so much... */
  initialize (&si, argc, argv);
  main_loop (&si);		/* doesn't return */
  return 0;
}


int
saver_ehandler (Display *dpy, XErrorEvent *error)
{
  saver_info *si = global_si_kludge;	/* I hate C so much... */

  fprintf (real_stderr, "\nX error in %s:\n", progname);
  if (XmuPrintDefaultErrorMessage (dpy, error, real_stderr))
    saver_exit (si, -1);
  else
    fprintf (real_stderr, " (nonfatal.)\n");
  return 0;
}

static void
initialize_connection (saver_info *si, int argc, char **argv)
{
  int i;
  Widget toplevel_shell = XtAppInitialize (&si->app, progclass,
					   options, XtNumber (options),
					   &argc, argv, defaults, 0, 0);

  si->dpy = XtDisplay (toplevel_shell);
  si->db = XtDatabase (si->dpy);
  XtGetApplicationNameAndClass (si->dpy, &progname, &progclass);

  if(strlen(progname)  > 100) progname [99] = 0;  /* keep it short. */

  db = si->db;	/* resources.c needs this */

  if (argc == 2 && !strcmp (argv[1], "-help"))
    do_help (si);

  else if (argc == 2 && !strcmp (argv[1], "-debug"))
    si->prefs.debug_p = True;  /* no resource for this one, out of paranoia. */

  else if (argc > 1)
    {
      const char *s = argv[1];
      fprintf (stderr, "%s: unknown option \"%s\".  Try \"-help\".\n",
	       progname, s);

      if (s[0] == '-' && s[1] == '-') s++;
      if (!strcmp (s, "-activate") ||
	  !strcmp (s, "-deactivate") ||
	  !strcmp (s, "-cycle") ||
	  !strcmp (s, "-next") ||
	  !strcmp (s, "-prev") ||
	  !strcmp (s, "-exit") ||
	  !strcmp (s, "-restart") ||
	  !strcmp (s, "-demo") ||
	  !strcmp (s, "-lock") ||
	  !strcmp (s, "-version") ||
	  !strcmp (s, "-time"))
	{
	  fprintf (stderr, "\n\
    However, %s is an option to the `xscreensaver-command' program.\n\
    The `xscreensaver' program is a daemon that runs in the background.\n\
    You control a running xscreensaver process by sending it messages\n\
    with `xscreensaver-command'.  See the man pages for details,\n\
    or check the web page: http://people.netscape.com/jwz/xscreensaver/\n\n",
		   s);

	  /* Since version 1.21 renamed the "-lock" option to "-lock-mode",
	     suggest that explicitly. */
	  if (!strcmp (s, "-lock"))
	    fprintf (stderr, "\
    Or perhaps you meant either the \"-lock-mode\" or the\n\
    \"-lock-timeout <minutes>\" options to xscreensaver?\n\n");
	}

      exit (1);
    }
  get_resources (si);
#ifndef NO_SETUID
  hack_uid_warn (si);
#endif /* NO_SETUID */
  XA_VROOT = XInternAtom (si->dpy, "__SWM_VROOT", False);
  XA_SCREENSAVER = XInternAtom (si->dpy, "SCREENSAVER", False);
  XA_SCREENSAVER_VERSION = XInternAtom (si->dpy, "_SCREENSAVER_VERSION",False);
  XA_SCREENSAVER_ID = XInternAtom (si->dpy, "_SCREENSAVER_ID", False);
  XA_SCREENSAVER_TIME = XInternAtom (si->dpy, "_SCREENSAVER_TIME", False);
  XA_XSETROOT_ID = XInternAtom (si->dpy, "_XSETROOT_ID", False);
  XA_ACTIVATE = XInternAtom (si->dpy, "ACTIVATE", False);
  XA_DEACTIVATE = XInternAtom (si->dpy, "DEACTIVATE", False);
  XA_RESTART = XInternAtom (si->dpy, "RESTART", False);
  XA_CYCLE = XInternAtom (si->dpy, "CYCLE", False);
  XA_NEXT = XInternAtom (si->dpy, "NEXT", False);
  XA_PREV = XInternAtom (si->dpy, "PREV", False);
  XA_EXIT = XInternAtom (si->dpy, "EXIT", False);
  XA_DEMO = XInternAtom (si->dpy, "DEMO", False);
  XA_LOCK = XInternAtom (si->dpy, "LOCK", False);

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
	  XtVaAppCreateShell(progname, progclass, applicationShellWidgetClass,
			     si->dpy,
			     XtNscreen, ssi->screen,
			     XtNvisual, ssi->current_visual,
			     XtNdepth,  visual_depth(ssi->screen,
						     ssi->current_visual),
			     0);
    }
}


static void
initialize (saver_info *si, int argc, char **argv)
{
  int i;
  saver_preferences *p = &si->prefs;
  Bool initial_demo_mode_p = False;
  si->version = (char *) malloc (5);
  memcpy (si->version, screensaver_id + 17, 4);
  si->version [4] = 0;
  progname = argv[0]; /* reset later; this is for the benefit of lock_init() */

  if(strlen(progname) > 100) progname[99] = 0;  /* keep it short. */

#ifdef NO_LOCKING
  si->locking_disabled_p = True;
  si->nolock_reason = "not compiled with locking support";
#else  /* !NO_LOCKING */
  si->locking_disabled_p = False;

# ifdef SCO
  set_auth_parameters(argc, argv);
# endif /* SCO */

  if (! lock_init (argc, argv))	/* before hack_uid() for proper permissions */
    {
      si->locking_disabled_p = True;
      si->nolock_reason = "error getting password";
    }
#endif  /* !NO_LOCKING */

#ifndef NO_SETUID
  hack_uid (si);
#endif /* NO_SETUID */

  progclass = "XScreenSaver";

  /* remove -initial-demo-mode switch before saving argv */
  for (i = 1; i < argc; i++)
    while (!strcmp ("-initial-demo-mode", argv [i]))
      {
	int j;
	initial_demo_mode_p = True;
	for (j = i; j < argc; j++)
	  argv [j] = argv [j+1];
	argv [j] = 0;
	argc--;
	if (argc <= i) break;
      }
  save_argv (argc, argv);
  initialize_connection (si, argc, argv);

  if (p->verbose_p)
    printf ("\
%s %s, copyright (c) 1991-1998 by Jamie Zawinski <jwz@netscape.com>\n\
 pid = %d.\n", progname, si->version, (int) getpid ());

  
  for (i = 0; i < si->nscreens; i++)
    if (ensure_no_screensaver_running (si->dpy, si->screens[i].screen))
      exit (1);

  hack_environment (si);

  si->demo_mode_p = initial_demo_mode_p;
  srandom ((int) time ((time_t *) 0));

  if (p->use_sgi_saver_extension)
    {
#ifdef HAVE_SGI_SAVER_EXTENSION
      if (! query_sgi_saver_extension (si))
	{
	  fprintf (stderr,
	 "%s: display %s does not support the SGI SCREEN_SAVER extension.\n",
		   progname, DisplayString (si->dpy));
	  p->use_sgi_saver_extension = False;
	}
      else if (p->use_mit_saver_extension)
	{
	  fprintf (stderr, "%s: SGI SCREEN_SAVER extension used instead\
 of MIT-SCREEN-SAVER extension.\n",
		   progname);
	  p->use_mit_saver_extension = False;
	}
      else if (p->use_xidle_extension)
	{
	  fprintf (stderr,
	 "%s: SGI SCREEN_SAVER extension used instead of XIDLE extension.\n",
		   progname);
	  p->use_xidle_extension = False;
	}
#else  /* !HAVE_MIT_SAVER_EXTENSION */
      fprintf (stderr,
       "%s: not compiled with support for the SGI SCREEN_SAVER extension.\n",
	       progname);
      p->use_sgi_saver_extension = False;
#endif /* !HAVE_SGI_SAVER_EXTENSION */
    }

  if (p->use_mit_saver_extension)
    {
#ifdef HAVE_MIT_SAVER_EXTENSION
      if (! query_mit_saver_extension (si))
	{
	  fprintf (stderr,
	 "%s: display %s does not support the MIT-SCREEN-SAVER extension.\n",
		   progname, DisplayString (si->dpy));
	  p->use_mit_saver_extension = False;
	}
      else if (p->use_xidle_extension)
	{
	  fprintf (stderr,
	 "%s: MIT-SCREEN-SAVER extension used instead of XIDLE extension.\n",
		   progname);
	  p->use_xidle_extension = False;
	}
#else  /* !HAVE_MIT_SAVER_EXTENSION */
      fprintf (stderr,
       "%s: not compiled with support for the MIT-SCREEN-SAVER extension.\n",
	       progname);
      p->use_mit_saver_extension = False;
#endif /* !HAVE_MIT_SAVER_EXTENSION */
    }

  if (p->use_xidle_extension)
    {
#ifdef HAVE_XIDLE_EXTENSION
      int first_event, first_error;
      if (! XidleQueryExtension (si->dpy, &first_event, &first_error))
	{
	  fprintf (stderr,
		   "%s: display %s does not support the XIdle extension.\n",
		   progname, DisplayString (si->dpy));
	  p->use_xidle_extension = False;
	}
#else  /* !HAVE_XIDLE_EXTENSION */
      fprintf (stderr, "%s: not compiled with support for XIdle.\n",
	       progname);
      p->use_xidle_extension = False;
#endif /* !HAVE_XIDLE_EXTENSION */
    }

  /* Call this only after having probed for presence of desired extension. */
  initialize_screensaver_window (si);

  init_sigchld ();

  disable_builtin_screensaver (si, True);

  if (p->verbose_p && p->use_mit_saver_extension)
    fprintf (stderr, "%s: using MIT-SCREEN-SAVER server extension.\n",
	     progname);
  if (p->verbose_p && p->use_sgi_saver_extension)
    fprintf (stderr, "%s: using SGI SCREEN_SAVER server extension.\n",
	     progname);
  if (p->verbose_p && p->use_xidle_extension)
    fprintf (stderr, "%s: using XIdle server extension.\n",
	     progname);

  initialize_stderr (si);
  XSetErrorHandler (saver_ehandler);

  if (initial_demo_mode_p)
    /* If the user wants demo mode, don't wait around before doing it. */
    p->initial_delay = 0;

  if (!p->use_xidle_extension &&
      !p->use_mit_saver_extension &&
      !p->use_sgi_saver_extension)
    {
      if (p->initial_delay)
	{
	  if (p->verbose_p)
	    {
	      printf ("%s: waiting for %d second%s...", progname,
		      (int) p->initial_delay,
		      (p->initial_delay == 1 ? "" : "s"));
	      fflush (stdout);
	    }
	  sleep (p->initial_delay);
	  if (p->verbose_p)
	    printf (" done.\n");
	}
      if (p->verbose_p)
	{
	  printf ("%s: selecting events on extant windows...", progname);
	  fflush (stdout);
	}

      /* Select events on the root windows of every screen.  This also selects
	 for window creation events, so that new subwindows will be noticed.
       */
      for (i = 0; i < si->nscreens; i++)
	start_notice_events_timer (si,
				   RootWindowOfScreen (si->screens[i].screen));

      if (p->verbose_p)
	printf (" done.\n");
    }
}

static void
main_loop (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  while (1)
    {
      if (! si->demo_mode_p)
	sleep_until_idle (si, True);

#ifndef NO_DEMO_MODE
      if (si->demo_mode_p)
	demo_mode (si);
      else
#endif /* !NO_DEMO_MODE */
	{
	  if (p->verbose_p)
	    printf ("%s: user is idle; waking up at %s.\n", progname,
		    timestring());
	  blank_screen (si);
	  spawn_screenhack (si, True);
	  if (p->cycle)
	    si->cycle_id = XtAppAddTimeOut (si->app, p->cycle, cycle_timer,
					    (XtPointer) si);

#ifndef NO_LOCKING
	  if (p->lock_p && p->lock_timeout == 0)
	    si->locked_p = True;
	  if (p->lock_p && !si->locked_p)
	    /* locked_p might be true already because of ClientMessage */
	    si->lock_id = XtAppAddTimeOut (si->app, p->lock_timeout,
					   activate_lock_timer,
					   (XtPointer) si);
#endif /* !NO_LOCKING */

	PASSWD_INVALID:

	  sleep_until_idle (si, False); /* until not idle */

#ifndef NO_LOCKING
	  if (si->locked_p)
	    {
	      Bool val;
	      if (si->locking_disabled_p) abort ();
	      si->dbox_up_p = True;

	      /* We used to ungrab the keyboard here, before calling unlock_p()
		 to pop up the dialog box.  This left the keyboard ungrabbed
		 for a small window, during an insecure state.  Bennett Todd
		 was seeing the bahavior that, when the load was high, he could
		 actually get characters through to a shell under the saver
		 window (he accidentally typed his password there...)

		 So the ungrab has been moved down into pop_passwd_dialog()
		 just after the server is grabbed, closing this window
		 entirely.
	       */
	      /* ungrab_keyboard_and_mouse (si); */

	      {
		saver_screen_info *ssi = si->default_screen;
		suspend_screenhack (si, True);
		XUndefineCursor (si->dpy, ssi->screensaver_window);
		if (p->verbose_p)
		  printf ("%s: prompting for password.\n", progname);
		val = unlock_p (si);
		if (p->verbose_p && val == False)
		  printf ("%s: password incorrect!\n", progname);
		si->dbox_up_p = False;
		XDefineCursor (si->dpy, ssi->screensaver_window, ssi->cursor);
		suspend_screenhack (si, False);

		/* I think this grab is now redundant, but it shouldn't hurt.
		 */
		if (!si->demo_mode_p)
		  grab_keyboard_and_mouse (si, ssi->screensaver_window,
					   ssi->cursor);
	      }

	      if (! val)
		goto PASSWD_INVALID;
	      si->locked_p = False;
	    }
#endif /* !NO_LOCKING */

	  /* Let's kill it before unblanking, to get it to stop drawing as
	     soon as possible... */
	  kill_screenhack (si);
	  unblank_screen (si);

	  if (si->cycle_id)
	    {
	      XtRemoveTimeOut (si->cycle_id);
	      si->cycle_id = 0;
	    }

#ifndef NO_LOCKING
	  if (si->lock_id)
	    {
	      XtRemoveTimeOut (si->lock_id);
	      si->lock_id = 0;
	    }
#endif /* !NO_LOCKING */

	  if (p->verbose_p)
	    printf ("%s: user is active; going to sleep at %s.\n", progname,
		    timestring ());
	}
    }
}



Bool
handle_clientmessage (saver_info *si, XEvent *event, Bool until_idle_p)
{
  saver_preferences *p = &si->prefs;
  Atom type = 0;
  if (event->xclient.message_type != XA_SCREENSAVER)
    {
      char *str;
      str = XGetAtomName (si->dpy, event->xclient.message_type);
      fprintf (stderr, "%s: unrecognised ClientMessage type %s received\n",
	       progname, (str ? str : "(null)"));
      if (str) XFree (str);
      return False;
    }
  if (event->xclient.format != 32)
    {
      fprintf (stderr, "%s: ClientMessage of format %d received, not 32\n",
	       progname, event->xclient.format);
      return False;
    }

  type = event->xclient.data.l[0];
  if (type == XA_ACTIVATE)
    {
      if (until_idle_p)
	{
	  if (p->verbose_p)
	    printf ("%s: ACTIVATE ClientMessage received.\n", progname);
	  if (p->use_mit_saver_extension || p->use_sgi_saver_extension)
	    {
	      XForceScreenSaver (si->dpy, ScreenSaverActive);
	      return False;
	    }
	  else
	    {
	      return True;
	    }
	}
      fprintf (stderr,
	       "%s: ClientMessage ACTIVATE received while already active.\n",
	       progname);
    }
  else if (type == XA_DEACTIVATE)
    {
      if (! until_idle_p)
	{
	  if (p->verbose_p)
	    printf ("%s: DEACTIVATE ClientMessage received.\n", progname);
	  if (p->use_mit_saver_extension || p->use_sgi_saver_extension)
	    {
	      XForceScreenSaver (si->dpy, ScreenSaverReset);
	      return False;
	    }
	  else
	    {
	      return True;
	    }
	}
      fprintf (stderr,
	       "%s: ClientMessage DEACTIVATE received while inactive.\n",
	       progname);
    }
  else if (type == XA_CYCLE)
    {
      if (! until_idle_p)
	{
	  if (p->verbose_p)
	    printf ("%s: CYCLE ClientMessage received.\n", progname);
	  if (si->cycle_id)
	    XtRemoveTimeOut (si->cycle_id);
	  si->cycle_id = 0;
	  cycle_timer ((XtPointer) si, 0);
	  return False;
	}
      fprintf (stderr, "%s: ClientMessage CYCLE received while inactive.\n",
	       progname);
    }
  else if (type == XA_NEXT || type == XA_PREV)
    {
      if (p->verbose_p)
	printf ("%s: %s ClientMessage received.\n", progname,
		(type == XA_NEXT ? "NEXT" : "PREV"));
      si->next_mode_p = 1 + (type == XA_PREV);

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
	  if (p->verbose_p)
	    printf ("%s: EXIT ClientMessage received.\n", progname);
	  if (! until_idle_p)
	    {
	      unblank_screen (si);
	      kill_screenhack (si);
	      XSync (si->dpy, False);
	    }
	  saver_exit (si, 0);
	}
      else
	fprintf (stderr, "%s: EXIT ClientMessage received while locked.\n",
		 progname);
    }
  else if (type == XA_RESTART)
    {
      /* The RESTART message works whether the screensaver is active or not,
	 unless the screen is locked, in which case it doesn't work.
       */
      if (until_idle_p || !si->locked_p)
	{
	  if (p->verbose_p)
	    printf ("%s: RESTART ClientMessage received.\n", progname);
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
	fprintf(stderr, "%s: RESTART ClientMessage received while locked.\n",
		progname);
    }
  else if (type == XA_DEMO)
    {
#ifdef NO_DEMO_MODE
      fprintf (stderr, "%s: not compiled with support for DEMO mode\n",
	       progname);
#else
      if (until_idle_p)
	{
	  if (p->verbose_p)
	    printf ("%s: DEMO ClientMessage received.\n", progname);
	  si->demo_mode_p = True;
	  return True;
	}
      fprintf (stderr,
	       "%s: DEMO ClientMessage received while active.\n", progname);
#endif
    }
  else if (type == XA_LOCK)
    {
#ifdef NO_LOCKING
      fprintf (stderr, "%s: not compiled with support for LOCK mode\n",
	       progname);
#else
      if (si->locking_disabled_p)
	fprintf (stderr,
	       "%s: LOCK ClientMessage received, but locking is disabled.\n",
		 progname);
      else if (si->locked_p)
	fprintf (stderr,
	       "%s: LOCK ClientMessage received while already locked.\n",
		 progname);
      else
	{
	  si->locked_p = True;
	  if (p->verbose_p) 
	    printf ("%s: LOCK ClientMessage received;%s locking.\n",
		    progname, until_idle_p ? " activating and" : "");

	  if (si->lock_id)	/* we're doing it now, so lose the timeout */
	    {
	      XtRemoveTimeOut (si->lock_id);
	      si->lock_id = 0;
	    }

	  if (until_idle_p)
	    {
	      if (p->use_mit_saver_extension || p->use_sgi_saver_extension)
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
#endif
    }
  else
    {
      char *str;
      str = (type ? XGetAtomName(si->dpy, type) : 0);
      if (str)
	fprintf (stderr,
		 "%s: unrecognised screensaver ClientMessage %s received\n",
		 progname, str);
      else
	fprintf (stderr,
		"%s: unrecognised screensaver ClientMessage 0x%x received\n",
		 progname, (unsigned int) event->xclient.data.l[0]);
      if (str) XFree (str);
    }
  return False;
}
