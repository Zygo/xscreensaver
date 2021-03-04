/* xscreensaver, Copyright © 1991-2021 Jamie Zawinski <jwz@jwz.org>
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
 * This is "xscreensaver-auth" -- When the screen is locked and there is
 * user activity, this program is launched to pop up a dialog, authenticate
 * the user, and eventually exit with a status code indicating success or
 * failure.  See the comment atop xscreensaver.c for details of the division
 * of powers.
 *
 * Flow of control within xscreensaver-auth:
 *
 *   - Privileged password initialization (e.g. read /etc/shadow as root)
 *   - Disavow privileges
 *   - Unprivileged password initialization
 *   - Connect to X server
 *   - xss_authenticate (passwd.c)
 *       - Tries PAM, Kerberos, pwent, shadow passwords (passwd-*.c) until
 *         one of them works.  Non-PAM methods are wrapped to act like PAM.
 *       - pam_conv calls our "conversation" function zero or more times.
 *         That function is expected to present messages to the user and/or
 *         to prompt the user to answer a question, wait for the answer, and
 *         return it.  There might be only one question (the password) or
 *         there might be others, even multiple passwords.
 *           - xscreensaver_auth_conv (dialog.c) is our conversation function.
 *               - First time it is called, it creates the window.
 *               - Subsequent times, it reuses that window.
 *               - Runs an X11 event loop waiting for the user to complete
 *                 or timeout, then returns the entered strings.
 *           - pam_conv takes the user input and returns success/failure.
 *       - xscreensaver_auth_finished is called to pop up a final dialog to
 *         present any error messages.
 *   - Exit with appropriate code.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <ctype.h>
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xos.h>
#include <X11/Xlocale.h>	/* for setlocale */
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>

#ifdef ENABLE_NLS
# include <libintl.h>
#endif

#include "xscreensaver.h"
#include "version.h"
#include "atoms.h"
#include "yarandom.h"
#include "resources.h"
#include "visual.h"
#include "auth.h"

#ifdef __GNUC__
 __extension__     /* shut up about "string length is greater than the length
                      ISO C89 compilers are required to support" when including
                      the .ad file... */
#endif

static char *defaults[] = {
#include "XScreenSaver_ad.h"
 0
};


char *progclass = 0;
Bool debug_p = False;


#ifdef HAVE_PROC_OOM
/* Linux systems have an "out-of-memory killer" that will nuke random procs in
   low-memory situations.  You'd think it would pick the process using the
   most memory, but most of the time it seems to pick the process that would
   be most comically inconvenient, such as your screen locker, or crond.

   Since killing "xscreensaver" unlocks the screen... that would be bad.

   This program, "xscreensaver-auth", is the part of the XScreenSaver daemon
   that might need to be setuid for other reasons, so we handle the OOM killer
   here.  We could instead handle OOM in the "xscreensaver" program, but then
   that program would *also* need to be setuid.

   So instead, "xscreensaver-auth" sets OOM immunity on its *parent* process.
   That means that if you run it by hand, it will apply that immunity to the
   parent shell.  Maybe that's bad?  I think I don't care.

       Linux >= 2.6.11: echo -17   > /proc/$$/oom_adj
       Linux >= 2.6.37: echo -1000 > /proc/$$/oom_score_adj

   "An aircraft company discovered that it was cheaper to fly its planes with
   less fuel on board.  On rare occasions, however, the amount of fuel was
   insufficient, and the plane would crash.  In emergency cases, a passenger
   was selected and thrown out of the plane.  When necessary, the procedure
   was repeated."

       https://lwn.net/Articles/104179/

   The OOM killer preferentially kills processes whose *children* use a lot of
   memory, and processes that are niced.  So if a display mode uses a lot of
   memory, the OOM-killer is more likely to shoot down the XScreenSaver
   *daemon* than just that screenhack!

   To disable the OOM-killer entirely:

       echo 2 > /proc/sys/vm/overcommit_memory
       echo vm.overcommit_memory = 2 >> /etc/sysctl.conf
 */
static void
oom_assassin_immunity (void)
{
# define OOM_VAL "-1000"
  char fn[1024];
  struct stat st;
  FILE *fd;
  pid_t pid = getppid();  /* our parent, not us */

  sprintf (fn, "/proc/%d/oom_score_adj", pid);
  if (stat(fn, &st) != 0)
    {
      if (verbose_p)
        fprintf (stderr, "%s: OOM: %s does not exist\n", blurb(), fn);
      return;
    }
  fd = fopen (fn, "w");
  if (!fd) goto FAIL;
  if (fputs (OOM_VAL "\n", fd) <= 0) goto FAIL;
  if (fclose (fd) != 0) goto FAIL;

  if (verbose_p)
    fprintf (stderr, "%s: OOM: echo " OOM_VAL " > %s\n", blurb(), fn);
  return;

 FAIL:
  {
    char buf[1024];
    const char *b = blurb();
    sprintf (buf, "%.40s: OOM: %.200s", b, fn);
    perror (buf);
    if (getuid() == geteuid())
      fprintf (stderr,
               "%s:   To prevent the kernel from randomly unlocking\n"
               "%s:   your screen via the out-of-memory killer,\n"
               "%s:   \"%s\" must be setuid root.\n",
               b, b, b, progname);
  }
}
#endif /* HAVE_PROC_OOM */


int
main (int argc, char **argv)
{
  Display *dpy;
  XtAppContext app;
  Widget root_widget;
  char *dpy_str = getenv ("DISPLAY");
  Bool xsync_p = False;
  Bool splash_p = False;
  Bool init_p = False;
  int i;

# undef ya_rand_init
  ya_rand_init (0);

  /* For Xt and X resource database purposes, this program is
     "xscreensaver", not "xscreensaver-auth".
   */
  {
    char *s = strrchr(argv[0], '/');
    if (s) s++;
    else s = argv[0];
    if (strlen(s) > 20)	/* keep it short. */
      s[20] = 0;
    progname = s;
  }

  progclass = "XScreenSaver";
  argv[0] = "xscreensaver";

  for (i = 1; i < argc; i++)
    {
      const char *oa = argv[i];
      /* XScreenSaver predates the "--arg" convention. */
      if (argv[i][0] == '-' && argv[i][1] == '-')
        argv[i]++;

      if (!strcmp (argv[i], "-v") || !strcmp (argv[i], "-verbose"))
        verbose_p = True;
      else if (!strcmp (argv[i], "-vv"))   verbose_p += 2;
      else if (!strcmp (argv[i], "-vvv"))  verbose_p += 3;
      else if (!strcmp (argv[i], "-vvvv")) verbose_p += 4;
      else if (!strcmp (argv[i], "-q") || !strcmp (argv[i], "-quiet"))
        verbose_p = False;
      else if (!strcmp (argv[i], "-debug"))
        /* Does nothing else at the moment but warn that "xscreensaver"
           is logging keystrokes to stderr. */
        debug_p = True;
      else if (!strcmp (argv[i], "-d") ||
               !strcmp (argv[i], "-dpy") ||
               !strcmp (argv[i], "-disp") ||
               !strcmp (argv[i], "-display"))
        {
          dpy_str = argv[++i];
          if (!dpy_str) goto HELP;
        }
      else if (!strcmp (argv[i], "-sync") ||
               !strcmp (argv[i], "-synch") ||
               !strcmp (argv[i], "-synchronize") ||
               !strcmp (argv[i], "-synchronise"))
        xsync_p = True;
      else if (!strcmp (argv[i], "-splash"))
        splash_p = True;
      else if (!strcmp (argv[i], "-init"))
        init_p = True;
      else if (!strcmp (argv[i], "-h") || !strcmp (argv[i], "-help"))
        {
        HELP:
          fprintf (stderr,
            "\n"
            "\txscreensaver-auth is launched by the xscreensaver daemon\n"
            "\tto authenticate the user by prompting for a password.\n"
            "\tDo not run this directly.\n"
            "\n"
            "\tOptions:\n"
            "\t\t--dpy host:display.screen\n"
            "\t\t--verbose --sync --splash --init\n"
            "\n"
            "\tRun 'xscreensaver-settings' to configure.\n"
            "\n");
          exit (1);
        }
      else
        {
          fprintf (stderr, "\n%s: unknown option: %s\n", blurb(), oa);
          goto HELP;
        }
    }

  if (!splash_p && init_p)
    {
      const char *v = XSCREENSAVER_VERSION;
      if (strstr (v, "a") || strstr (v, "b"))
        splash_p = True;  /* Not optional for alpha and beta releases */
    }

# ifdef HAVE_PROC_OOM
  if (splash_p || init_p)
    oom_assassin_immunity ();
# endif

  if (!splash_p && !init_p)
    lock_priv_init ();

  if (!splash_p && init_p)
    exit (0);

  disavow_privileges ();

  if (!splash_p)
    lock_init ();

  /* Setting the locale is necessary for XLookupString to return multi-byte
     characters, enabling their use in passwords.
   */
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


  /* Copy the -dpy arg to $DISPLAY for subprocesses. */
  {
    char *s = (char *) malloc (strlen(dpy_str) + 20);
    sprintf (s, "DISPLAY=%s", dpy_str);
    putenv (s);
    /* free (s); */  /* some versions of putenv do not copy */
  }

  /* Open the display */
  {
    XrmOptionDescRec options;
    argc = 1;   /* Xt does not receive any of our command-line options. */
    root_widget = XtAppInitialize (&app, progclass, &options, 0,
                                   &argc, argv, defaults, 0, 0);
  }

  dpy = XtDisplay (root_widget);
  if (xsync_p) XSynchronize (dpy, True);

  if (splash_p)
    {
      xscreensaver_splash (root_widget);
      exit (0);
    }
  else if (xscreensaver_auth ((void *) root_widget,
                              xscreensaver_auth_conv,
                              xscreensaver_auth_finished))
    {
      if (verbose_p)
        fprintf (stderr, "%s: authentication succeeded\n", blurb());
      exit (200);
    }
  else
    {
      if (verbose_p)
        fprintf (stderr, "%s: authentication failed\n", blurb());
      exit (-1);
    }

  /* On timeout, xscreensaver_auth did exit(0) */
}
