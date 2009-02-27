/* xscreensaver, Copyright (c) 1991-1993 Jamie Zawinski <jwz@lucid.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include "version.h"

/*   ========================================================================
 *   First we wait until the keyboard and mouse become idle for the specified
 *   amount of time.  We do this by periodically checking with the XIdle
 *   server extension.
 *
 *   Then, we map a full screen black window.  
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
 *   If we don't have the XIdle extension, then we do the XAutoLock trick:
 *   notice every window that gets created, and wait 30 seconds or so until
 *   its creating process has settled down, and then select KeyPress events on
 *   those windows which already select for KeyPress events.  It's important
 *   that we not select KeyPress on windows which don't select them, because
 *   that would interfere with event propagation.  This will break if any
 *   program changes its event mask to contain KeyRelease or PointerMotion
 *   more than 30 seconds after creating the window, but that's probably
 *   pretty rare.
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
 *   None of this crap happens if we're using the XIdle extension, so install
 *   it if the description above sounds just too flaky to live.  It is, but
 *   those are your choices.
 *
 *   A third idle-detection option could be implement (but is not): when running
 *   on the console display ($DISPLAY is `localhost`:0) and we're on a machine
 *   where /dev/tty and /dev/mouse have reasonable last-modification times, we
 *   could just stat those.  But the incremental benefit of implementing this
 *   is really small, so forget I said anything.
 *
 *   Debugging hints:
 *     - Have a second terminal handy.
 *     - Be careful where you set your breakpoints, you don't want this to
 *       stop under the debugger with the keyboard grabbed or the blackout
 *       window exposed.
 *     - you probably can't set breakpoints in functions that are called on
 *       the other side of a call to fork() -- if your clients are dying 
 *       with signal 5, Trace/BPT Trap, you're losing in this way.
 *     - If you aren't using XIdle, don't leave this stopped under the
 *       debugger for very long, or the X input buffer will get huge because
 *       of the keypress events it's selecting for.  This can make your X
 *       server wedge with "no more input buffers."
 *       
 *   ========================================================================
 */

#if __STDC__
#include <stdlib.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Intrinsic.h>
#include <X11/Xos.h>

#ifdef HAVE_XIDLE
#include <X11/extensions/xidle.h>
#endif

#include "xscreensaver.h"

#if defined(SVR4) || defined(SYSV)
# define srandom(i) srand((unsigned int)(i))
#else
extern void srandom P((int));		/* srand() is in stdlib.h... */
#endif

extern char *get_string_resource P((char *, char *));
extern Bool get_boolean_resource P((char *, char *));
extern int get_integer_resource P((char *, char *));
extern unsigned int get_minutes_resource P((char *, char *));
extern unsigned int get_seconds_resource P((char *, char *));

extern Visual *get_visual_resource P((Display *, char *, char *));
extern int get_visual_depth P((Display *, Visual *));

extern void notice_events_timer P((XtPointer closure, void *timer));
extern void cycle_timer P((void *junk1, XtPointer junk2));
extern void activate_lock_timer P((void *junk1, XtPointer junk2));
extern void sleep_until_idle P((Bool until_idle_p));

extern void ensure_no_screensaver_running P((void));
extern void initialize_screensaver_window P((void));
extern void disable_builtin_screensaver P((void));

extern void hack_environment P((void));
extern void grab_keyboard_and_mouse P((void));
extern void ungrab_keyboard_and_mouse P((void));

extern void save_argv P((int argc, char **argv));


char *screensaver_version;
char *progname;
char *progclass;
XrmDatabase db;

XtAppContext app;

Display *dpy;
Screen *screen;
Visual *visual;
int visual_depth;

Widget toplevel_shell;

Time lock_timeout;

extern Time timeout;
extern Time cycle;
#ifndef NO_LOCKING
extern Time passwd_timeout;
#endif
extern Time pointer_timeout;
extern Time notice_events_timeout;
extern XtIntervalId lock_id, cycle_id;

Bool use_xidle;
Bool verbose_p;
Bool lock_p, locked_p;

extern char **screenhacks;
extern int screenhacks_count;
extern char *shell;
extern int nice_inferior;
extern Window screensaver_window;
extern Cursor cursor;
extern Colormap cmap, cmap2;
extern Bool fade_p, unfade_p;
extern int fade_seconds, fade_ticks;
extern Bool install_cmap_p;
extern Bool locking_disabled_p;
extern char *nolock_reason;
extern Bool demo_mode_p;
extern Bool dbox_up_p;
extern int next_mode_p;

static time_t initial_delay;

extern Atom XA_VROOT, XA_XSETROOT_ID;
extern Atom XA_SCREENSAVER_VERSION, XA_SCREENSAVER_ID;

static Atom XA_SCREENSAVER;
static Atom XA_ACTIVATE, XA_DEACTIVATE, XA_CYCLE, XA_NEXT, XA_PREV;
static Atom XA_EXIT, XA_RESTART, XA_DEMO, XA_LOCK;

#ifdef NO_MOTIF /* kludge */
Bool demo_mode_p = 0;
Bool dbox_up_p = 0;
#ifndef NO_LOCKING
Time passwd_timeout = 0;
#endif
#endif


#ifdef NO_DEMO_MODE
# define demo_mode() abort()
#else
extern void demo_mode P((void));
#endif

static XrmOptionDescRec options [] = {
  { "-timeout",		".timeout",	XrmoptionSepArg, 0 },
  { "-idelay",		".initialDelay",XrmoptionSepArg, 0 },
  { "-cycle",		".cycle",	XrmoptionSepArg, 0 },
  { "-visual",		".visualID",	XrmoptionSepArg, 0 },
  { "-lock-timeout",	".lockTimeout",	XrmoptionSepArg, 0 },
  { "-verbose",		".verbose",	XrmoptionNoArg, "on" },
  { "-silent",		".verbose",	XrmoptionNoArg, "off" },
  { "-xidle",		".xidle",	XrmoptionNoArg, "on" },
  { "-no-xidle",	".xidle",	XrmoptionNoArg, "off" },
  { "-lock",		".lock",	XrmoptionNoArg, "on" },
  { "-no-lock",		".lock",	XrmoptionNoArg, "off" }
};

static char *defaults[] = {
#include "XScreenSaver.ad.h"
 0
};

static void
do_help ()
{
  printf ("\
xscreensaver %s, copyright (c) 1991-1993 by Jamie Zawinski <jwz@lucid.com>.\n\
The standard Xt command-line options are accepted; other options include:\n\
\n\
    -timeout <minutes>		when the screensaver should activate\n\
    -cycle <minutes>		how long to let each hack run\n\
    -idelay <seconds>		how long to sleep before startup\n\
    -demo			enter interactive demo mode on startup\n\
    -verbose			be loud\n\
    -silent			don't\n\
    -xidle			use the XIdle server extension\n\
    -no-xidle			don't\n\
    -lock			require a password before deactivating\n\
    -no-lock			don't\n\
    -lock-timeout <minutes>	grace period before locking; default 0\n\
    -help			this message\n\
\n\
Use the `xscreensaver-command' program to control a running screensaver.\n\
\n\
The *programs, *colorPrograms, and *monoPrograms resources control which\n\
graphics demos will be launched by the screensaver.  See the man page for\n\
more details.\n\n",
	  screensaver_version);

#ifdef NO_LOCKING
  printf("Support for locking was not enabled at compile-time.\n");
#endif
#ifdef NO_DEMO_MODE
  printf("Support for demo mode was not enabled at compile-time.\n");
#endif
#ifndef HAVE_XIDLE
  printf("Support for the XIdle extension was not enabled at compile-time.\n");
#endif

  fflush (stdout);
  exit (1);
}


static void
get_screenhacks ()
{
  char *data[3];
  int i, hacks_size = 10;

  data[0] = get_string_resource ("programs", "Programs");
  data[1] = ((CellsOfScreen (screen) <= 2)
	     ? get_string_resource ("monoPrograms", "MonoPrograms")
	     : get_string_resource ("colorPrograms", "ColorPrograms"));
  data[2] = 0;
  if (! data[0]) data[0] = data[1], data[1] = 0;

  screenhacks = (char **) malloc (sizeof (char *) * hacks_size);
  screenhacks_count = 0;

  for (i = 0; data[i]; i++)
    {
      int j = 0;
      char *d = data [i];
      int size = strlen (d);
      while (j < size)
	{
	  int end, start = j;
	  if (d[j] == ' ' || d[j] == '\t' || d[j] == '\n' || d[j] == 0)
	    {
	      j++;
	      continue;
	    }
	  if (hacks_size <= screenhacks_count)
	    screenhacks = (char **) realloc (screenhacks,
					     (hacks_size = hacks_size * 2) *
					     sizeof (char *));
	  screenhacks [screenhacks_count++] = d + j;
	  while (d[j] != 0 && d[j] != '\n')
	    j++;
	  end = j;
	  while (j > start && (d[j-1] == ' ' || d[j-1] == '\t'))
	    j--;
	  d[j] = 0;
	  j = end + 1;
	}
    }

  /* shrink all whitespace to one space, for the benefit of the "demo"
     mode display.  We only do this when we can easily tell that the
     whitespace is not significant (no shell metachars).
   */
  for (i = 0; i < screenhacks_count; i++)
    {
      char *s = screenhacks [i];
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
		for (s2 = s + j + 1; *s2; s2++)
		  s2 [0] = s2 [k];
	      break;
	    }
	}
    DONE:
      ;
    }

  if (screenhacks_count)
    {
      /* Shrink down the screenhacks array to be only as big as it needs to.
	 This doesn't really matter at all. */
      screenhacks = (char **)
	realloc (screenhacks, ((screenhacks_count + 1) * sizeof(char *)));
      screenhacks [screenhacks_count] = 0;
    }
  else
    {
      free (screenhacks);
      screenhacks = 0;
    }
}


static void
get_resources ()
{
  /* Note: we can't use the resource ".visual" because Xt is SO FUCKED. */
  visual	  = get_visual_resource (dpy, "visualID", "VisualID");
  timeout         = 1000 * get_minutes_resource ("timeout", "Time");
  cycle           = 1000 * get_minutes_resource ("cycle",   "Time");
  lock_timeout	  = 1000 * get_minutes_resource ("lockTimeout", "Time");
  nice_inferior   = get_integer_resource ("nice", "Nice");
  verbose_p       = get_boolean_resource ("verbose", "Boolean");
  lock_p          = get_boolean_resource ("lock", "Boolean");
  install_cmap_p  = get_boolean_resource ("installColormap", "Boolean");
  fade_p	  = get_boolean_resource ("fade", "Boolean");
  unfade_p	  = get_boolean_resource ("unfade", "Boolean");
  fade_seconds	  = get_seconds_resource ("fadeSeconds", "Time");
  fade_ticks	  = get_integer_resource ("fadeTicks", "Integer");
  shell           = get_string_resource ("bourneShell", "BourneShell");
  initial_delay   = get_seconds_resource ("initialDelay", "Time");
  pointer_timeout = 1000 * get_seconds_resource ("pointerPollTime", "Time");
  notice_events_timeout = 1000 * get_seconds_resource ("windowCreationTimeout",
						       "Time");
#ifndef NO_LOCKING
  passwd_timeout  = 1000 * get_seconds_resource ("passwdTimeout", "Time");
  if (passwd_timeout == 0) passwd_timeout = 30000;
#endif
  if (timeout < 10000) timeout = 10000;
  if (cycle < 2000) cycle = 2000;
  if (pointer_timeout == 0) pointer_timeout = 5000;
  if (notice_events_timeout == 0) notice_events_timeout = 10000;
  if (fade_seconds == 0 || fade_ticks == 0) fade_p = False;
  if (! fade_p) unfade_p = False;

  visual_depth = get_visual_depth (dpy, visual);

  if (visual_depth <= 1 || CellsOfScreen (screen) <= 2)
    install_cmap_p = False;

#ifdef NO_LOCKING
  locking_disabled_p = True;
  nolock_reason = "not compiled with locking support";
  if (lock_p)
    {
      lock_p = False;
      fprintf (stderr, "%s: %snot compiled with support for locking.\n",
	       progname, (verbose_p ? "## " : ""));
    }
#else  /* ! NO_LOCKING */
  if (lock_p && locking_disabled_p)
    {
      fprintf (stderr, "%s: %slocking is disabled (%s).\n", progname,
	       (verbose_p ? "## " : ""), nolock_reason);
      lock_p = False;
    }
#endif /* ! NO_LOCKING */

  /* don't set use_xidle unless it is explicitly specified */
  if (get_string_resource ("xidle", "Boolean"))
    use_xidle = get_boolean_resource ("xidle", "Boolean");
  else
#ifdef HAVE_XIDLE	/* pick a default */
    use_xidle = True;
#else
    use_xidle = False;
#endif
  get_screenhacks ();
}

char *
timestring ()
{
  long now = time ((time_t *) 0);
  char *str = (char *) ctime (&now);
  char *nl = (char *) strchr (str, '\n');
  if (nl) *nl = 0; /* take off that dang newline */
  return str;
}

#ifdef NO_SETUID
# define hack_uid()
# define hack_uid_warn()
#else /* !NO_SETUID */
extern void hack_uid P((void));
extern void hack_uid_warn P((void));
#endif /* NO_SETUID */


#ifndef NO_LOCKING
extern Bool unlock_p P((Widget));
extern Bool lock_init P((void));
#endif

static void initialize ();
static void main_loop ();
static void initialize ();

void
main (argc, argv)
     int argc;
     char **argv;
{
  initialize (argc, argv);
  main_loop ();
}


static void
initialize_connection (argc, argv)
     int argc;
     char **argv;
{
  toplevel_shell = XtAppInitialize (&app, progclass,
				    options, XtNumber (options),
				    &argc, argv, defaults, 0, 0);

  dpy = XtDisplay (toplevel_shell);
  screen = XtScreen (toplevel_shell);
  db = XtDatabase (dpy);
  XtGetApplicationNameAndClass (dpy, &progname, &progclass);

  if (argc == 2 && !strcmp (argv[1], "-help"))
    do_help ();
  else if (argc > 1)
    {
      fprintf (stderr, "%s: unknown option %s\n", progname, argv [1]);
      exit (1);
    }
  get_resources ();
  hack_uid_warn ();
  hack_environment ();
  XA_VROOT = XInternAtom (dpy, "__SWM_VROOT", False);
  XA_SCREENSAVER = XInternAtom (dpy, "SCREENSAVER", False);
  XA_SCREENSAVER_VERSION = XInternAtom (dpy, "_SCREENSAVER_VERSION", False);
  XA_SCREENSAVER_ID = XInternAtom (dpy, "_SCREENSAVER_ID", False);
  XA_XSETROOT_ID = XInternAtom (dpy, "_XSETROOT_ID", False);
  XA_ACTIVATE = XInternAtom (dpy, "ACTIVATE", False);
  XA_DEACTIVATE = XInternAtom (dpy, "DEACTIVATE", False);
  XA_RESTART = XInternAtom (dpy, "RESTART", False);
  XA_CYCLE = XInternAtom (dpy, "CYCLE", False);
  XA_NEXT = XInternAtom (dpy, "NEXT", False);
  XA_PREV = XInternAtom (dpy, "PREV", False);
  XA_EXIT = XInternAtom (dpy, "EXIT", False);
  XA_DEMO = XInternAtom (dpy, "DEMO", False);
  XA_LOCK = XInternAtom (dpy, "LOCK", False);
}

extern void init_sigchld P((void));

static void
initialize (argc, argv)
     int argc;
     char **argv;
{
  Bool initial_demo_mode_p = False;
  screensaver_version = (char *) malloc (5);
  memcpy (screensaver_version, screensaver_id + 17, 4);
  screensaver_version [4] = 0;
  progname = argv[0]; /* reset later; this is for the benefit of lock_init() */

#ifdef NO_LOCKING
  locking_disabled_p = True;
  nolock_reason = "not compiled with locking support";
#else
  locking_disabled_p = False;
  if (! lock_init ())	/* before hack_uid() for proper permissions */
    {
      locking_disabled_p = True;
      nolock_reason = "error getting password";
    }
#endif

  hack_uid ();
  progclass = "XScreenSaver";

  /* remove -demo switch before saving argv */
  {
    int i;
    for (i = 1; i < argc; i++)
      while (!strcmp ("-demo", argv [i]))
	{
	  int j;
	  initial_demo_mode_p = True;
	  for (j = i; j < argc; j++)
	    argv [j] = argv [j+1];
	  argv [j] = 0;
	  argc--;
	  if (argc <= i) break;
	}
  }
  save_argv (argc, argv);
  initialize_connection (argc, argv);
  ensure_no_screensaver_running ();

  if (verbose_p)
    printf ("\
%s %s, copyright (c) 1991-1993 by Jamie Zawinski <jwz@lucid.com>.\n\
 pid = %d.\n", progname, screensaver_version, getpid ());
  ensure_no_screensaver_running ();

  demo_mode_p = initial_demo_mode_p;
  screensaver_window = 0;
  cursor = 0;
  initialize_screensaver_window ();
  srandom ((int) time ((time_t *) 0));
  cycle_id = 0;
  lock_id = 0;
  locked_p = False;

  if (use_xidle)
    {
#ifdef HAVE_XIDLE
      int first_event, first_error;
      if (! XidleQueryExtension (dpy, &first_event, &first_error))
	{
	  fprintf (stderr,
		   "%s: display %s does not support the XIdle extension.\n",
		   progname, DisplayString (dpy));
	  use_xidle = 0;
	}
#else
      fprintf (stderr, "%s: not compiled with support for XIdle.\n",
	       progname);
      use_xidle = 0;
#endif
    }

  init_sigchld ();

  disable_builtin_screensaver ();

  if (initial_demo_mode_p)
    /* If the user wants demo mode, don't wait around before doing it. */
    initial_delay = 0;

#ifdef HAVE_XIDLE
  if (! use_xidle)
#endif
    {
      if (initial_delay)
	{
	  if (verbose_p)
	    {
	      printf ("%s: waiting for %d second%s...", progname,
		      initial_delay, (initial_delay == 1 ? "" : "s"));
	      fflush (stdout);
	    }
	  sleep (initial_delay);
	  if (verbose_p)
	    printf (" done.\n");
	}
      if (verbose_p)
	{
	  printf ("%s: selecting events on extant windows...", progname);
	  fflush (stdout);
	}
      notice_events_timer ((XtPointer)
			   RootWindowOfScreen (XtScreen (toplevel_shell)),
			   0);
      if (verbose_p)
	printf (" done.\n");
    }
}


extern void suspend_screenhack P((Bool suspend_p));

static void
main_loop ()
{
  while (1)
    {
      if (! demo_mode_p)
	sleep_until_idle (True);

      if (demo_mode_p)
	demo_mode ();
      else
	{
	  if (verbose_p)
	    printf ("%s: user is idle; waking up at %s.\n", progname,
		    timestring());
	  blank_screen ();
	  spawn_screenhack (True);
	  if (cycle)
	    cycle_id = XtAppAddTimeOut (app, cycle, cycle_timer, 0);

#ifndef NO_LOCKING
	  if (lock_p && lock_timeout == 0)
	    locked_p = True;
	  if (lock_p && !locked_p)
	    /* locked_p might be true already because of ClientMessage */
	    lock_id = XtAppAddTimeOut (app,lock_timeout,activate_lock_timer,0);
#endif

	PASSWD_INVALID:

	  sleep_until_idle (False); /* until not idle */

#ifndef NO_LOCKING
	  if (locked_p)
	    {
	      Bool val;
	      if (locking_disabled_p) abort ();
	      dbox_up_p = True;
	      ungrab_keyboard_and_mouse ();
	      suspend_screenhack (True);
	      XUndefineCursor (dpy, screensaver_window);
	      if (verbose_p)
		printf ("%s: prompting for password.\n", progname);
	      val = unlock_p (toplevel_shell);
	      if (verbose_p && val == False)
		printf ("%s: password incorrect!\n", progname);
	      dbox_up_p = False;
	      XDefineCursor (dpy, screensaver_window, cursor);
	      suspend_screenhack (False);
	      grab_keyboard_and_mouse ();
	      if (! val)
		goto PASSWD_INVALID;
	      locked_p = False;
	    }
#endif
	  unblank_screen ();
	  kill_screenhack ();
	  if (cycle_id)
	    {
	      XtRemoveTimeOut (cycle_id);
	      cycle_id = 0;
	    }
#ifndef NO_LOCKING
	  if (lock_id)
	    {
	      XtRemoveTimeOut (lock_id);
	      lock_id = 0;
	    }
#endif
	  if (verbose_p)
	    printf ("%s: user is active; going to sleep at %s.\n", progname,
		    timestring ());
	}
    }
}



Bool
handle_clientmessage (event, until_idle_p)
     XEvent *event;
     Bool until_idle_p;
{
  Atom type;
  if (event->xclient.message_type != XA_SCREENSAVER)
    {
      char *str;
      str = XGetAtomName (dpy, event->xclient.message_type);
      fprintf (stderr, "%s: %sunrecognised ClientMessage type %s received\n",
	       progname, (verbose_p ? "## " : ""),
	       (str ? str : "(null)"));
      if (str) XFree (str);
    }
  if (event->xclient.format != 32)
    {
      fprintf (stderr, "%s: %sClientMessage of format %d received, not 32\n",
	       progname, (verbose_p ? "## " : ""), event->xclient.format);
    }
  type = event->xclient.data.l[0];
  if (type == XA_ACTIVATE)
    {
      if (until_idle_p)
	{
	  if (verbose_p)
	    printf ("%s: ACTIVATE ClientMessage received.\n", progname);
	  return True;
	}
      fprintf (stderr,
	       "%s: %sClientMessage ACTIVATE received while already active.\n",
	       progname, (verbose_p ? "## " : ""));
    }
  else if (type == XA_DEACTIVATE)
    {
      if (! until_idle_p)
	{
	  if (verbose_p)
	    printf ("%s: DEACTIVATE ClientMessage received.\n", progname);
	  return True;
	}
      fprintf (stderr,
	       "%s: %sClientMessage DEACTIVATE received while inactive.\n",
	       progname, (verbose_p ? "## " : ""));
    }
  else if (type == XA_CYCLE)
    {
      if (! until_idle_p)
	{
	  if (verbose_p)
	    printf ("%s: CYCLE ClientMessage received.\n", progname);
	  if (cycle_id)
	    XtRemoveTimeOut (cycle_id);
	  cycle_id = 0;
	  cycle_timer (0, 0);
	  return False;
	}
      fprintf (stderr,
	       "%s: %sClientMessage CYCLE received while inactive.\n",
	       progname, (verbose_p ? "## " : ""));
    }
  else if (type == XA_NEXT || type == XA_PREV)
    {
      if (verbose_p)
	printf ("%s: %s ClientMessage received.\n", progname,
		(type == XA_NEXT ? "NEXT" : "PREV"));
      next_mode_p = 1 + (type == XA_PREV);

      if (! until_idle_p)
	{
	  if (cycle_id)
	    XtRemoveTimeOut (cycle_id);
	  cycle_id = 0;
	  cycle_timer (0, 0);
	}
      else
	return True;
    }
  else if (type == XA_EXIT)
    {
      /* Ignore EXIT message if the screen is locked. */
      if (until_idle_p || !locked_p)
	{
	  if (verbose_p)
	    printf ("%s: EXIT ClientMessage received.\n", progname);
	  if (! until_idle_p)
	    {
	      unblank_screen ();
	      kill_screenhack ();
	      XSync (dpy, False);
	    }
	  exit (0);
	}
      else
	fprintf (stderr, "%s: %sEXIT ClientMessage received while locked.\n",
		 progname, (verbose_p ? "## " : ""));
    }
  else if (type == XA_RESTART)
    {
      /* The RESTART message works whether the screensaver is active or not,
	 unless the screen is locked, in which case it doesn't work.
       */
      if (until_idle_p || !locked_p)
	{
	  if (verbose_p)
	    printf ("%s: RESTART ClientMessage received.\n", progname);
	  if (! until_idle_p)
	    {
	      unblank_screen ();
	      kill_screenhack ();
	      XSync (dpy, False);
	    }
	  restart_process ();
	}
      else
	fprintf(stderr, "%s: %sRESTART ClientMessage received while locked.\n",
		progname, (verbose_p ? "## " : ""));
    }
  else if (type == XA_DEMO)
    {
#ifdef NO_DEMO_MODE
      fprintf (stderr,
	       "%s: %snot compiled with support for DEMO mode\n",
	       progname, (verbose_p ? "## " : ""));
#else
      if (until_idle_p)
	{
	  if (verbose_p)
	    printf ("%s: DEMO ClientMessage received.\n", progname);
	  demo_mode_p = True;
	  return True;
	}
      fprintf (stderr,
	       "%s: %sDEMO ClientMessage received while active.\n",
	       progname, (verbose_p ? "## " : ""));
#endif
    }
  else if (type == XA_LOCK)
    {
#ifdef NO_LOCKING
      fprintf (stderr, "%s: %snot compiled with support for LOCK mode\n",
	       progname, (verbose_p ? "## " : ""));
#else
      if (locking_disabled_p)
	fprintf (stderr,
	       "%s: %sLOCK ClientMessage received, but locking is disabled.\n",
		 progname, (verbose_p ? "## " : ""));
      else if (locked_p)
	fprintf (stderr,
	       "%s: %sLOCK ClientMessage received while already locked.\n",
		 progname, (verbose_p ? "## " : ""));
      else
	{
	  locked_p = True;
	  if (verbose_p) 
	    printf ("%s: LOCK ClientMessage received;%s locking.\n",
		    progname, until_idle_p ? " activating and" : "");

	  if (lock_id)	/* we're doing it now, so lose the timeout */
	    {
	      XtRemoveTimeOut (lock_id);
	      lock_id = 0;
	    }

	  if (until_idle_p)
	    return True;
	}
#endif
    }
  else
    {
      char *str;
      str = XGetAtomName (dpy, type);
      if (str)
	fprintf (stderr,
		 "%s: %sunrecognised screensaver ClientMessage %s received\n",
		 progname, (verbose_p ? "## " : ""), str);
      else
	fprintf (stderr,
		 "%s: %sunrecognised screensaver ClientMessage 0x%x received\n",
		 progname, (verbose_p ? "## " : ""),
		 event->xclient.data.l[0]);
      if (str) XFree (str);
    }
  return False;
}
