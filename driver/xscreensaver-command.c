/* xscreensaver-command, Copyright © 1991-2021 Jamie Zawinski <jwz@jwz.org>
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

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

/* #include <X11/Xproto.h>	/ * for CARD32 */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>		/* for XGetClassHint() */
#include <X11/Xos.h>

#include <X11/Intrinsic.h>	/* only needed to get through xscreensaver.h */

#include "blurb.h"
#include "remote.h"
#include "version.h"
#include "atoms.h"

#ifdef _VROOT_H_
ERROR! you must not include vroot.h in this file
#endif

static char *screensaver_version;
# ifdef __GNUC__
  __extension__   /* don't warn about "string length is greater than the
                     length ISO C89 compilers are required to support" in the
                     usage string... */
# endif
static char *usage = "\n\
usage: %s -<option>\n\
\n\
  This program provides external control of a running xscreensaver process.\n\
  Version %s, copyright (c) 1991-%s Jamie Zawinski <jwz@jwz.org>.\n\
\n\
  The xscreensaver program is a daemon that runs in the background.\n\
  You control a running xscreensaver process by sending it messages\n\
  with this program, xscreensaver-command.  See the man pages for\n\
  details.  These are the arguments understood by xscreensaver-command:\n\
\n\
  -quiet        Only print output if an error occurs.\n\
  -verbose      Opposite of -quiet. Default.\n\
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
                sequentially.\n\
\n\
  -prev         Like -next, but goes in the other direction.\n\
\n\
  -select <N>   Like -activate, but runs the Nth element in the list of\n\
                hacks.  By knowing what is in the `programs' list, and in\n\
                what order, you can use this to activate the screensaver\n\
                with a particular graphics demo.  (The first element in the\n\
                list is numbered 1, not 0.)\n\
\n\
  -lock         Tells the running xscreensaver process to lock the screen\n\
                immediately.  This is like -activate, but forces locking as\n\
                well, even if locking is not the default.  If the saver is\n\
                already active, this causes it to be locked as well.\n\
\n\
  -suspend      Like -activate, but ignores the lock-timeout, if any, and\n\
                immediately powers off the screen without fading out.\n\
                This is intended to be run just after your laptop's lid\n\
                is closed, and just before the CPU halts.\n\
\n\
  -exit         Causes the xscreensaver process to exit gracefully.\n\
                This does nothing if the display is currently locked.\n\
                (Note that one must *never* kill xscreensaver with -9!)\n\
\n\
  -restart      Causes the screensaver process to exit and then restart with\n\
                the same command line arguments as last time.  You shouldn't\n\
                really need to do this, since xscreensaver notices when the\n\
                .xscreensaver file has changed and re-reads it as needed.\n\
\n\
  -time         Prints the time at which the screensaver last activated or\n\
                deactivated (roughly, how long the user has been idle or\n\
                non-idle -- but not quite, since it only tells you when the\n\
                screen became blanked or un-blanked.)\n\
\n\
  -watch        Prints a line each time the screensaver changes state: when\n\
                the screen blanks, locks, unblanks, or when the running hack\n\
                is changed.  This option never returns; it is intended for\n\
                use by shell scripts that want to react to the screensaver\n\
                in some way.\n\
\n\
  -version      Prints the version of xscreensaver that is currently running\n\
                on the display -- that is, the actual version number of the\n\
                running xscreensaver background process, rather than the\n\
                version number of xscreensaver-command.\n\
\n\
  See the man page for more details.\n\
  For updates, check https://www.jwz.org/xscreensaver/\n\
\n";

#define USAGE() do { \
 fprintf (stderr, usage, progname, screensaver_version, year); exit (1); \
 } while(0)

static int watch (Display *);

int
main (int argc, char **argv)
{
  Display *dpy;
  int i;
  char *dpyname = 0;
  Atom *cmd = 0;
  long arg = 0L;
  char *s;
  Atom XA_WATCH = 0;  /* kludge: not really an atom */
  char year[5];
  Bool verbose_p = TRUE;

  progname = argv[0];
  s = strrchr (progname, '/');
  if (s) progname = s+1;

  screensaver_version = strdup (screensaver_id + 17);
  s = strchr (screensaver_version, ' ');
  *s = 0;

  s = strchr (screensaver_id, '-');
  s = strrchr (s, '-');
  s++;
  strncpy (year, s, 4);
  year[4] = 0;

  for (i = 1; i < argc; i++)
    {
      const char *s = argv [i];
      int L;
      if (s[0] == '-' && s[1] == '-') s++;
      L = strlen (s);
      if (L < 2) USAGE ();
      if (!strncmp (s, "-display", L))	       dpyname = argv [++i];
      else if (!strncmp (s, "-dpy", L))        dpyname = argv [++i];
      else if (!strncmp (s, "-quiet", L))      verbose_p = FALSE;
      else if (!strcmp  (s, "-verbose"))       verbose_p = TRUE;
      else if (!strcmp  (s, "-v"))             verbose_p = TRUE;
      else if (!strcmp  (s, "-vv"))            verbose_p = 2;
      else if (!strcmp  (s, "-vvv"))           verbose_p = 3;
      else if (!strcmp  (s, "-vvvv"))          verbose_p = 4;
      else if (cmd) USAGE();
      else if (!strncmp (s, "-activate", L))   cmd = &XA_ACTIVATE;
      else if (!strncmp (s, "-deactivate", L)) cmd = &XA_DEACTIVATE;
      else if (!strncmp (s, "-suspend", L))    cmd = &XA_SUSPEND;
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
      else if (!strncmp (s, "-time", L))       cmd = &XA_SCREENSAVER_STATUS;
      else if (!strncmp (s, "-watch", L))      cmd = &XA_WATCH;
      else USAGE ();

      if (cmd == &XA_SELECT || cmd == &XA_DEMO)
	{
	  long a;
	  char c;
	  if (i+1 < argc && (1 == sscanf(argv[i+1], " %ld %c", &a, &c)))
	    {
	      arg = a;
	      i++;
	    }
	}
    }

  if (!cmd)
    USAGE ();

  if (arg < 0)
    /* no command may have a negative argument. */
    USAGE();
  else if (arg == 0)
    {
      /* SELECT must have a non-zero argument. */
      if (cmd == &XA_SELECT)
	USAGE();
    }
  else /* arg > 0 */
    {
      /* no command other than SELECT and DEMO may have a non-zero argument. */
      if (cmd != &XA_DEMO && cmd != &XA_SELECT)
	USAGE();
    }



  /* -demo with no arguments launches xscreensaver-settings.
     -demo N sends a message to the daemon demo that hack number.
   */
  if (cmd == &XA_PREFS ||
      (cmd == &XA_DEMO && arg == 0))
    {
      char buf [512];
      char *new_argv[] = { "xscreensaver-settings", 0, 0, 0, 0, 0 };
      int ac = 1;

      if (dpyname)
	{
	  new_argv[ac++] = "-display";
	  new_argv[ac++] = dpyname;
	}

      if (cmd == &XA_PREFS)
	new_argv[ac++] = "-prefs";

      fflush(stdout);
      fflush(stderr);
      execvp (new_argv[0], new_argv);	/* shouldn't return */

      sprintf (buf, "%s: could not exec %s", progname, new_argv[0]);
      perror(buf);
      fflush(stdout);
      fflush(stderr);
      exit (-1);
    }



  if (!dpyname) dpyname = (char *) getenv ("DISPLAY");

  if (!dpyname)
    {
      dpyname = ":0.0";
      fprintf (stderr,
               "%s: warning: $DISPLAY is not set: defaulting to \"%s\"\n",
               progname, dpyname);
    }

  dpy = XOpenDisplay (dpyname);
  if (!dpy)
    {
      fprintf (stderr, "%s: can't open display %s\n", progname,
	       (dpyname ? dpyname : "(null)"));
      exit (1);
    }

  init_xscreensaver_atoms (dpy);

  if (cmd == &XA_WATCH)
    {
      i = watch (dpy);
      exit (i);
    }

  if (*cmd == XA_ACTIVATE || *cmd == XA_LOCK || *cmd == XA_SUSPEND || 
      *cmd == XA_NEXT || *cmd == XA_PREV || *cmd == XA_SELECT)
    /* People never guess that KeyRelease deactivates the screen saver too,
       so if we're issuing an activation command, wait a second.
       No need to do this if stdin is not a tty, meaning we're not being
       run from the command line.
     */
    if (isatty(0))
      sleep (1);

  i = xscreensaver_command (dpy, *cmd, arg, verbose_p, NULL);
  if (i < 0) exit (i);
  else exit (0);
}


static int
watch (Display *dpy)
{
  char *v = 0;
  Window window = RootWindow (dpy, 0);
  XWindowAttributes xgwa;
  XEvent event;
  PROP32 *last = 0;

  if (v) free (v);
  XGetWindowAttributes (dpy, window, &xgwa);
  XSelectInput (dpy, window, xgwa.your_event_mask | PropertyChangeMask);

  while (1)
    {
      XNextEvent (dpy, &event);
      if (event.xany.type == PropertyNotify &&
          event.xproperty.state == PropertyNewValue &&
          event.xproperty.atom == XA_SCREENSAVER_STATUS)
        {
	  Atom type;
	  int format;
	  unsigned long nitems, bytesafter;
          unsigned char *dataP = 0;

	  if (XGetWindowProperty (dpy,
                                  RootWindow (dpy, 0),  /* always screen #0 */
				  XA_SCREENSAVER_STATUS,
				  0, 999, False, XA_INTEGER,
				  &type, &format, &nitems, &bytesafter,
				  &dataP)
	      == Success
	      && type
	      && dataP)
	    {
              time_t tt;
              char *s;
              Bool changed = False;
              Bool running = False;
              PROP32 *data = (PROP32 *) dataP;

              if (type != XA_INTEGER || nitems < 3)
                {
                STATUS_LOSE:
                  if (last) XFree (last);
                  if (data) XFree (data);
                  fprintf (stderr, "%s: bad status format on root window\n",
                           progname);
                  return -1;
                }
                  
              tt = (time_t) data[1];
              if (tt <= (time_t) 666000000L) /* early 1991 */
                goto STATUS_LOSE;

              s = ctime(&tt);
              if (s[strlen(s)-1] == '\n')
                s[strlen(s)-1] = 0;

              if (!last || data[0] != last[0])
                {
                  /* State changed. */
                  if (data[0] == XA_BLANK)
                    printf ("BLANK %s\n", s);
                  else if (data[0] == XA_LOCK)
                    printf ("LOCK %s\n", s);
                  else if (data[0] == 0)
                    printf ("UNBLANK %s\n", s);
                  else
                    goto STATUS_LOSE;
                }

              if (!last)
                changed = True;
              else
                {
                  int i;
                  for (i = 2; i < nitems; i++)
                    {
                      if (data[i] != last[i])
                        changed = True;
                      if (data[i])
                        running = True;
                    }
                }

              if (running && changed)
                {
                  int i;
                  fprintf (stdout, "RUN");
                  for (i = 2; i < nitems; i++)
                    fprintf (stdout, " %d", (int) data[i]);
                  fprintf (stdout, "\n");
                }

              fflush (stdout);

              if (last) XFree (last);
              last = data;
	    }
	  else
	    {
	      if (last) XFree (last);
	      if (dataP) XFree (dataP);
	      fprintf (stderr, "%s: no saver status on root window\n",
		       progname);
	      return -1;
	    }
        }
    }
}
