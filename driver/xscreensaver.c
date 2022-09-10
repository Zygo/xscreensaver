/* xscreensaver, Copyright © 1991-2022 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * ==========================================================================
 * XScreenSaver Daemon, version 6.
 * ==========================================================================
 *
 * Having started its life in 1991, XScreenSaver acquired a lot of optional
 * features to allow it to detect user activity, lock the screen and
 * authenticate on systems as varied as VMS, SunOS, HPUX, SGI, and of course
 * that young upstart, Linux.  In such a heterogeneous environment, many
 * features that would have simplified things were not universally available.
 * Though I made an effort to follow a principle of minimal library usage in
 * the XScreenSaver daemon, as described in my 2004 article "On Toolkits"
 * <https://www.jwz.org/xscreensaver/toolkits.html>, there was still quite a
 * lot of code in the daemon, more than is strictly necessary when we consider
 * only modern, still-maintained operating systems.
 *
 * This 2021 refactor had one goal: to minimize the amount of code in which a
 * crash will cause the screen to unlock.  In service of that goal, the process
 * which holds the keyboard an mouse grabbed should:
 *
 *   -  Be as small as technically possible, for ease of auditing;
 *
 *   -  Link against as few libraries as possible, to bring in as little
 *      third-party code as possible;
 *
 *   -  Delegate other tasks to helper programs running in other processes,
 *      in such a way that if those other processes should crash, the grabs
 *      remain intact and the screen remains locked.
 *
 * In the XScreenSaver 6 design, the division of labor is as follows:
 *
 *  A)  "xscreensaver" -- This is the main screen saver daemon.  If this
 *      process crashes, the screen unlocks, so this is the one where
 *      minimizing the attack surface is critical.
 *
 *      -  It reads events using the XInput extension version 2, which is
 *         required.  This means that X11R7 is also required, which was
 *         released in 2009, and is ubiquitous as of 2021.
 *
 *      -  It links against only libX11 and libXi.
 *
 *      -  It maps no windows and renders no graphics or text.  It only
 *         acquires keyboard and mouse grabs, handles timer logic, and
 *         launches and manages several sub-processes.
 *
 *  B)  "xscreensaver-gfx" -- When the time comes for the screen to blank,
 *      this process is launched to fade out, black out every monitor on
 *      the system, launch graphics demos to render on those blanked screens,
 *      and cycle them from time to time.
 *
 *      -  If this program crashes, the screen does not unlock.  The keyboard
 *         and mouse remain grabbed by the "xscreensaver" process, which will
 *         re-launch this process as necessary.
 *
 *      -  If it does crash, the logged in user's desktop may be momentarily
 *         visible before it re-launches, but it will be impossible to interact
 *         with it.
 *
 *      -  This process must react to hot-swapping of monitors in order to
 *         keep the screen blanked and the desktop obscured, and manages the
 *         life cycle of the graphics demos, each running in their own
 *         sub-process of "xscreensaver-gfx".
 *
 *  C)  The graphics demos themselves.  Launched by "xscreensaver-gfx" to run
 *      on the windows provided, there is one of these processes for each
 *      screen.  These can use X11 or OpenGL, and have no impact on security;
 *      if it breaks, you can keep both pieces.
 *
 *  D)  "xscreensaver-auth" -- When the time comes to prompt the user for their
 *      password to unlock the screen, the "xscreensaver" daemon launches this
 *      process.  If it exits with a "success" error code, the screen unlocks,
 *      otherwise it does not.  This means that if it crashes, the screen
 *      remains locked.
 *
 *      -  It turns out that programs using the XInput 2 extension are able to
 *         snoop the keyboard even when the keyboard is grabbed by another
 *         process!  So that's how this program reads user input, even while
 *         the "xscreensaver" process has the keyboard grabbed.
 *
 *      -  This program runs PAM, or whatever other authorization mechanisms
 *         are configured.  On some systems, it may need to be setuid in order
 *         to read /etc/shadow, meaning it must disavow privileges after
 *         initialization but before connecting to the X server.
 *
 *      -  It gets to use libXft, so the fonts don't suck.
 *
 *      -  In theory, this program could be implemented using any GUI toolkit,
 *         and thus take advantage of input methods, on-screen keyboards,
 *         screen readers, etc.  Again, this is permissible because this
 *         program fails SAFE: if it crashes, the screen remains locked.
 *
 *  E)  "xscreensaver-systemd" -- This runs in the background and monitors
 *      requests on the systemd dbus.  This is how closing the laptop lid
 *      causes the screen to lock, and how video players request that blanking
 *      be inhibited.  This program invokes "xscreensaver-command" as needed
 *      to pass those requests along to "xscreensaver" via ClientMessages.
 *
 *
 * ==========================================================================
 * Ancient History
 * ==========================================================================
 *
 * As mentioned above, XScreenSaver version 6 (released in 2021) requires
 * the XInput 2 server extension for idle detection and password input.
 * In XScreenSaver 5 and earlier (1991 through 2020), idle-detection was
 * far more convoluted.  Basically, we selected input events on every window,
 * and noticed when new windows were created so that we could track them too.
 * Over the decades, there have also been three optional X11 server extensions
 * that were applicable to screen saving:
 *
 *  - XIdle
 *
 *    This extension provided a function to poll the user's idle time.
 *    It was simple and direct and worked great.  Therefore, it was
 *    removed from the X11 distribution in 1994, with the release of
 *    X11R6.  https://bugs.freedesktop.org/show_bug.cgi?id=1419
 *
 *  - SGI SCREEN_SAVER
 *
 *    This extension sent two new events: "user is idle", and "user is no
 *    longer idle". It was simple and direct and worked great.  But as
 *    the name implies, it only ever worked on Silicon Graphics machines.
 *    SGI became irrelevant around 1998 and went out of business in 2009.
 *
 *  - MIT-SCREEN-SAVER
 *
 *    This extension still exists, but it is useless to us.  Since it still
 *    exists, people sometimes ask why XScreenSaver no longer makes use of it.
 *    The MIT-SCREEN-SAVER extension took the following approach:
 *
 *      - When the user is idle, immediately map full screen black windows
 *        on each screen.
 *
 *      - Inform the screen saver client that the screen is now black.
 *
 *      - When user activity occurs, unmap the windows and then inform the
 *        screen saver client.
 *
 *    The screen saver client was able to specify a few parameters of that
 *    window, like visual and depth, but that was all.
 *
 *    The extension was designed with the assumption that a screen saver would
 *    render onto the provided window.  However:
 *
 *      - OpenGL programs may require different visuals than 2D X11 programs,
 *        and you can't change the visual of a window after it has been
 *        created.
 *
 *      - The extension mapped one window per X11 "Screen", which, in this
 *        modern world, tend to span the entire virtual desktop; whereas
 *        XScreenSaver runs savers full screen on each *monitor* instead.
 *        In other words, it was incompatible with Xinerama / RANDR.
 *
 *      - Since this extension mapped its own full-screen black windows and
 *        informed us of that after the fact, it precluded "fade" and "unfade"
 *        animations from being implemented.
 *
 *      - Since it only told us when the user was idle or non-idle, it
 *        precluded the "hysteresis" option from being implemented (ignoring
 *        tiny mouse motions).
 *
 *    In summary, it created its windows too early, removed them too late,
 *    created windows of the wrong quantity and wrong shape, could not create
 *    them with the proper visuals, and delivered too little information about
 *    what caused the user activity.
 *
 *    Also, the MIT-SCREEN-SAVER extension was flaky, and using it at all led
 *    to frequent server crashes.  Worse, those server crashes were highly
 *    dependent on your particular video driver.
 *
 *     So that's why, even if the server supports the MIT-SCREEN-SAVER
 *     extension, we don't use it.
 *
 *     Some video players attempt to inhibit blanking during playback by
 *     calling XResetScreenSaver and/or XScreenSaverSuspend, which affect
 *     only the X server's *built-in* screen saver, and the window created
 *     by the MIT-SCREEN-SAVER extension.  To rely upon those calls alone
 *     and then call it a day would betray a willful ignorance of why the
 *     MIT-SCREEN-SAVER extension is a useless foundation upon which to
 *     build a screen saver or screen locker.
 *
 *     Fortunately, every modern video player and web browser also makes
 *     use of systemd / logind / dbus to request blanking inhibition, and
 *     we receive those requests via our systemd integration in
 *     xscreensaver-systemd (which see).
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "version.h"

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>

#include <pwd.h>		/* for getpwuid() */
#include <grp.h>		/* for getgrgid() */

#ifdef HAVE_UNAME
# include <sys/utsname.h>	/* for uname() */
#endif /* HAVE_UNAME */

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>		/* for waitpid() and associated macros */
#endif

#ifndef HAVE_XINPUT
# error The XInput2 extension is required
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/Xos.h>
#include <X11/extensions/XInput2.h>

#include "xmu.h"
#include "blurb.h"
#include "atoms.h"
#include "clientmsg.h"
#include "xinput.h"
#include "prefs.h"


#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#undef ABS
#define ABS(x)((x)<0?-(x):(x))

#undef MAX
#define MAX(x,y)((x)>(y)?(x):(y))


/* Globals used in this file.
 */
Bool debug_p = False;
static Bool splash_p = True;
static const char *version_number = 0;

/* Preferences. */
static Bool lock_p = False;
static Bool locking_disabled_p = False;
static Bool blanking_disabled_p = False;
static unsigned int blank_timeout = 0;
static unsigned int lock_timeout = 0;
static unsigned int pointer_hysteresis = 0;

/* Subprocesses. */
#define SAVER_GFX_PROGRAM     "xscreensaver-gfx"
#define SAVER_AUTH_PROGRAM    "xscreensaver-auth"
#define SAVER_SYSTEMD_PROGRAM "xscreensaver-systemd"
static pid_t saver_gfx_pid     = 0;
static pid_t saver_auth_pid    = 0;
static pid_t saver_systemd_pid = 0;
static int sighup_received  = 0;
static int sigterm_received = 0;
static int sigchld_received = 0;
static Bool gfx_stopped_p = False;

Window daemon_window = 0;
Cursor blank_cursor = None;
Cursor auth_cursor  = None;


/* for the --restart command.
 */
static char **saved_argv;

static void
save_argv (int ac, char **av)
{
  saved_argv = (char **) calloc (ac+2, sizeof (char *));
  saved_argv [ac] = 0;
  while (ac--)
    {
      int i = strlen (av[ac]) + 1;
      saved_argv[ac] = (char *) malloc (i);
      memcpy (saved_argv[ac], av[ac], i);
    }
}


static void
kill_all_subprocs (void)
{
  if (saver_gfx_pid)
    {
      if (verbose_p)
        fprintf (stderr, "%s: pid %lu: killing " SAVER_GFX_PROGRAM "\n",
                 blurb(), (unsigned long) saver_gfx_pid);
      kill (saver_gfx_pid, SIGTERM);

      if (gfx_stopped_p)	/* SIGCONT to allow SIGTERM to proceed */
        {
          if (verbose_p)
            fprintf (stderr, "%s: pid %lu: sending " SAVER_GFX_PROGRAM
                     " SIGCONT\n",
                     blurb(), (unsigned long) saver_gfx_pid);
          gfx_stopped_p = False;
          kill (-saver_gfx_pid, SIGCONT);  /* send to process group */
        }
    }

  if (saver_auth_pid)
    {
      if (verbose_p)
        fprintf (stderr, "%s: pid %lu: killing " SAVER_AUTH_PROGRAM "\n",
                 blurb(), (unsigned long) saver_auth_pid);
      kill (saver_auth_pid, SIGTERM);
    }

  if (saver_systemd_pid)
    {
      if (verbose_p)
        fprintf (stderr, "%s: pid %lu: killing " SAVER_SYSTEMD_PROGRAM "\n",
                 blurb(), (unsigned long) saver_systemd_pid);
      kill (saver_systemd_pid, SIGTERM);
    }
}


static void
saver_exit (int status)
{
  kill_all_subprocs();
  exit (status);
}


static void
catch_signal (int sig, RETSIGTYPE (*handler) (int))
{
# ifdef HAVE_SIGACTION

  struct sigaction a;
  a.sa_handler = handler;
  sigemptyset (&a.sa_mask);
  a.sa_flags = 0;

  /* On Linux 2.4.9 (at least) we need to tell the kernel to not mask delivery
     of this signal from inside its handler, or else when we execvp() the
     process again, it starts up with SIGHUP blocked, meaning that killing
     it with -HUP only works *once*.  You'd think that execvp() would reset
     all the signal masks, but it doesn't.
   */
#  if defined(SA_NOMASK)
  a.sa_flags |= SA_NOMASK;
#  elif defined(SA_NODEFER)
  a.sa_flags |= SA_NODEFER;
#  endif

  if (sigaction (sig, &a, 0) < 0)
# else  /* !HAVE_SIGACTION */
  if (((long) signal (sig, handler)) == -1L)
# endif /* !HAVE_SIGACTION */
    {
      char buf [255];
      sprintf (buf, "%s: couldn't catch signal %d", blurb(), sig);
      perror (buf);
      saver_exit (1);
    }
}


/* Re-execs the process with the arguments in saved_argv.  Does not return.
   Do not call this while the screen is locked: user must unlock first.
 */
static void
restart_process (void)
{
  kill_all_subprocs();
  execvp (saved_argv [0], saved_argv);	/* shouldn't return */
  {
    char buf [512];
    sprintf (buf, "%s: could not restart process", blurb());
    perror(buf);
    fflush(stderr);
    abort();
  }
}

static RETSIGTYPE saver_sighup_handler  (int sig) { sighup_received  = sig; }
static RETSIGTYPE saver_sigchld_handler (int sig) { sigchld_received = sig; }
static RETSIGTYPE saver_sigterm_handler (int sig) { sigterm_received = sig; }

static void
handle_signals (void)
{
  catch_signal (SIGHUP,  saver_sighup_handler);
  catch_signal (SIGCHLD, saver_sigchld_handler);
  catch_signal (SIGTERM, saver_sigterm_handler);	/* kill     */
  catch_signal (SIGINT,  saver_sigterm_handler);	/* shell ^C */
  catch_signal (SIGQUIT, saver_sigterm_handler);	/* shell ^| */
}


static pid_t
fork_and_exec (Display *dpy, int argc, char **argv)
{
  char buf [255];
  pid_t forked = fork();
  switch ((int) forked) {
  case -1:
    sprintf (buf, "%s: couldn't fork", blurb());
    perror (buf);
    break;

  case 0:
    close (ConnectionNumber (dpy));	/* close display fd */
    execvp (argv[0], argv);		/* shouldn't return. */

    sprintf (buf, "%s: pid %lu: couldn't exec %s", blurb(),
             (unsigned long) getpid(), argv[0]);
    perror (buf);
    exit (1);				/* exits child fork */
    break;

  default:				/* parent fork */
    if (verbose_p)
      {
        int i;
        fprintf (stderr, "%s: pid %lu: launched",
                 blurb(), (unsigned long) forked);
        for (i = 0; i < argc; i++)
          fprintf (stderr, " %s", argv[i]);
        fprintf (stderr, "\n");
      }

    /* Put each launched process in its own process group so that
       SIGSTOP will affect all of its inferior processes as well.
     */
    if (setpgid (forked, 0))
      {
        char buf [255];
        sprintf (buf, "%s: setpgid %d", blurb(), forked);
        perror (buf);
      }
    break;
  }

  return forked;
}


static int respawn_thrashing_count = 0;

/* Called from the main loop some time after the SIGCHLD signal has fired.
   Returns true if:
     - the process that died was "xscreensaver-auth", and
     - it exited with a "success" status meaning "user is authenticated".
 */
static Bool
handle_sigchld (Display *dpy, Bool blanked_p)
{
  Bool authenticated_p = False;

  sigchld_received = 0;
  if (debug_p)
    fprintf (stderr, "%s: SIGCHLD received\n", blurb());

  /* Reap every now-dead inferior without blocking. */
  while (1)
    {
      int wait_status = 0;
      pid_t kid = waitpid (-1, &wait_status, WNOHANG | WUNTRACED);
      char how[100];

      /* 0 means no more children to reap.
         -1 means error -- except "interrupted system call"
         isn't a "real" error, so if we get that, try again.
       */
      if (kid == 0 ||
          (kid < 0 && errno != EINTR))
        break;

      /* We get SIGCHLD after sending SIGSTOP, but no action is required. */
      if (WIFSTOPPED (wait_status))
        {
          if (verbose_p)
            fprintf (stderr, "%s: pid %lu: %s stopped\n", blurb(),
                     (unsigned long) kid,
                     (kid == saver_gfx_pid     ? SAVER_GFX_PROGRAM :
                      kid == saver_auth_pid    ? SAVER_AUTH_PROGRAM :
                      kid == saver_systemd_pid ? SAVER_SYSTEMD_PROGRAM :
                      "unknown"));
          continue;
        }

      if (WIFSIGNALED (wait_status))
        {
          if (WTERMSIG (wait_status) == SIGTERM)
            strcpy (how, "with SIGTERM");
          else
            sprintf (how, "with signal %d", WTERMSIG (wait_status));
        }
      else if (WIFEXITED (wait_status))
        {
          int exit_status = WEXITSTATUS (wait_status);
          /* Treat exit code as a signed 8-bit quantity. */
          if (exit_status & 0x80) exit_status |= ~0xFF;
          if (exit_status)
            sprintf (how, "with status %d", exit_status);
          else
            strcpy (how, "normally");
        }
      else
        sprintf (how, "for unknown reason %d", wait_status);

      if (kid == saver_gfx_pid)
        {
          saver_gfx_pid = 0;
          gfx_stopped_p = False;
          if (blanked_p)
            {
              if (respawn_thrashing_count > 5)
                {
                  /* If we have tried to re-launch this pid N times in a row
                     without unblanking, give up instead of forking it in a
                     tight loop.  Screen will remain grabbed, but desktop will
                     be visible.
                   */
                  fprintf (stderr, "%s: pid %lu: " SAVER_GFX_PROGRAM
                           " won't re-launch!\n",
                           blurb(), (unsigned long) kid);
                }
              else
                {
                  char *av[10];
                  int ac = 0;
                  av[ac++] = SAVER_GFX_PROGRAM;
                  av[ac++] = "--emergency";
                  if (verbose_p)     av[ac++] = "--verbose";
                  if (verbose_p > 1) av[ac++] = "--verbose";
                  if (verbose_p > 2) av[ac++] = "--verbose";
                  if (debug_p)       av[ac++] = "--debug";
                  av[ac] = 0;
                  fprintf (stderr, "%s: pid %lu: " SAVER_GFX_PROGRAM
                           " exited unexpectedly %s: re-launching\n",
                           blurb(), (unsigned long) kid, how);
                  gfx_stopped_p = False;
                  saver_gfx_pid = fork_and_exec (dpy, ac, av);
                  respawn_thrashing_count++;
                }
            }
          else
            {
              /* Should not have been running anyway. */
              if (verbose_p)
                fprintf (stderr, "%s: pid %lu: " SAVER_GFX_PROGRAM
                         " exited %s\n", blurb(), (unsigned long) kid, how);
            }
        }
      else if (kid == saver_systemd_pid)
        {
          /* xscreensaver-systemd might fail if systemd isn't running, or if
             xscreensaver-systemd was already running, or if some other
             program has bound to our targets.  Or if it doesn't exist because
             this system doesn't use systemd.  So don't re-launch it if it
             failed to start, or dies.
           */
          saver_systemd_pid = 0;
          fprintf (stderr, "%s: pid %lu: " SAVER_SYSTEMD_PROGRAM
                   " exited unexpectedly %s\n",
                   blurb(), (unsigned long) kid, how);
        }
      else if (kid == saver_auth_pid)
        {
          saver_auth_pid = 0;

          /* xscreensaver-auth exits with status 200 to mean "ok to unlock".
             Any other exit code, or dying with a signal, means "nope".
           */
          if (!WIFSIGNALED (wait_status) &&
              WIFEXITED (wait_status))
            {
              unsigned int status = (unsigned int) WEXITSTATUS (wait_status);
              if (status == 200)
                {
                  authenticated_p = True;
                  strcpy (how, "and authenticated");
                }
              else if (status == 0 && !blanked_p)
                strcpy (how, "normally");   /* This was the splash dialog */
              else if (status == 0)
                strcpy (how, "and authentication canceled");
              else if (status == 255)  /* which is -1 */
                strcpy (how, "and authentication failed");
            }

          if (verbose_p)
            fprintf (stderr, "%s: pid %lu: " SAVER_AUTH_PROGRAM " exited %s\n",
                     blurb(), (unsigned long) kid, how);
        }
      else if (verbose_p)
        {
          fprintf (stderr, "%s: pid %lu: unknown child"
                   " exited unexpectedly %s\n",
                   blurb(), (unsigned long) kid, how);
        }
    }

  return authenticated_p;
}


/* Add DEFAULT_PATH_PREFIX to the front of $PATH.
   Typically "/usr/libexec/xscreensaver".
 */
static void
hack_environment (void)
{
  static const char *def_path = DEFAULT_PATH_PREFIX;
  const char *opath = getenv("PATH");
  char *npath;
  if (! opath) opath = "/bin:/usr/bin";  /* WTF */
  npath = (char *) malloc(strlen(def_path) + strlen(opath) + 20);
  strcpy (npath, "PATH=");
  strcat (npath, def_path);
  strcat (npath, ":");
  strcat (npath, opath);

  /* Can fail if out of memory, I guess. Ignore errors. */
  putenv (npath);

  /* don't free (npath) -- some implementations of putenv (BSD 4.4,
     glibc 2.0) copy the argument, but some (libc4,5, glibc 2.1.2)
     do not.  So we must leak it (and/or the previous setting). Yay.
   */
}


static void
print_banner(void)
{
  const time_t rel = XSCREENSAVER_RELEASED;
  struct tm *tm = localtime (&rel);
  char buf[100];
  int months = (time ((time_t *) 0) - XSCREENSAVER_RELEASED) / (60*60*24*30);
  int years = months / 12.0 + 0.7;

  strftime (buf, sizeof(buf), "%b %Y", tm);

  if (months > 18)
    sprintf (buf + strlen(buf),  " -- %d years ago", years);
  else if (months > 1)
    sprintf (buf + strlen(buf), " -- %d months ago", months);

  if (verbose_p || debug_p)
    {
      fprintf (stderr,
               "\tXScreenSaver " XSCREENSAVER_VERSION ", released %s\n"
               "\tCopyright \302\251 1991-%d by"
               " Jamie Zawinski <jwz@jwz.org>\n\n",
               buf, tm->tm_year + 1900);
      if (months > 18)
        fprintf (stderr,
                 /* Hey jerks, the only time someone will see this particular
                    message is if they are running xscreensaver with '-log' in
                    order to send me a bug report, and they had damned well
                    better try the latest release before they do that. */
                 "\t   ###################################################\n"
                 "\t   ###                                             ###\n"
                 "\t   ###  THIS VERSION IS VERY OLD! PLEASE UPGRADE!  ###\n"
                 "\t   ###                                             ###\n"
                 "\t   ###################################################\n"
                 "\n");
    }

  if (debug_p)
      fprintf (stderr,
               "#####################################"
               "#####################################\n"
               "\n"
               "\t\t\t DEBUG MODE IS NOT SECURE\n"
               "\n"
               "\tThe XScreenSaver window will only cover the left half of\n"
               "\tthe screen.  Position your terminal window on the right.\n"
               "\tWARNING: stderr and the log file will include every\n"
               "\tcharacter that you type, including passwords.\n"
               "\n"
               "#####################################"
               "#####################################\n"
               "\n");

  version_number = XSCREENSAVER_VERSION;
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


/* Mostly duplicated in resources.c.
 */
static int
parse_time (const char *string)
{
  unsigned int h, m, s;
  char c;
  if (3 == sscanf (string,   " %u : %2u : %2u %c", &h, &m, &s, &c))
    ;
  else if (2 == sscanf (string, " : %2u : %2u %c", &m, &s, &c) ||
	   2 == sscanf (string,    " %u : %2u %c", &m, &s, &c))
    h = 0;
  else if (1 == sscanf (string,       " : %2u %c", &s, &c))
    h = m = 0;
  else if (1 == sscanf (string,          " %u %c", &m, &c))
    h = s = 0;
  else
    {
      fprintf (stderr, "%s: unparsable duration \"%s\"\n", blurb(), string);
      return -1;
    }
  if (s >= 60 && (h != 0 || m != 0))
    {
      fprintf (stderr, "%s: seconds > 59 in \"%s\"\n", blurb(), string);
      return -1;
    }
  if (m >= 60 && h > 0)
    {
      fprintf (stderr, "%s: minutes > 59 in \"%s\"\n", blurb(), string);
      return -1;
    }
  return ((h * 60 * 60) + (m * 60) + s);
}


/* This program only needs a very few options from the init file, so it
   just reads the .ad file and the .xscreensaver file directly rather
   than going through Xt and Xrm.
 */
static void init_line_handler (int lineno, 
                               const char *key, const char *val,
                               void *closure)
{
  if      (!strcmp (key, "verbose")) verbose_p = !strcasecmp (val, "true");
  else if (!strcmp (key, "splash"))  splash_p  = !strcasecmp (val, "true");
  else if (!strcmp (key, "lock"))    lock_p    = !strcasecmp (val, "true");
  else if (!strcmp (key, "mode"))    blanking_disabled_p =
                                       !strcasecmp (val, "off");
  else if (!strcmp (key, "timeout"))
    {
      int t = parse_time (val);
      if (t > 0) blank_timeout = t;
    }
  else if (!strcmp (key, "lockTimeout"))
    {
      int t = parse_time (val);
      if (t >= 0) lock_timeout = t;
    }
  else if (!strcmp (key, "pointerHysteresis"))
    {
      int i = atoi (val);
      if (i >= 0)
        pointer_hysteresis = i;
    }
}

static void
read_init_file_simple (const char *filename)
{
  if (debug_p)
    fprintf (stderr, "%s: reading %s\n", blurb(), filename);
  parse_init_file (filename, init_line_handler, 0);
}


static time_t init_file_time = 0;

static void
read_init_files (Bool both_p)
{
  static const char *home = 0;
  char *fn;
  struct stat st;
  Bool read_p = False;

  if (!home)
    {
      home = getenv("HOME");
      if (!home) home = "";
    }

  if (both_p)
    {
      read_init_file_simple (AD_DIR "/XScreenSaver");
      read_p = True;
    }

  fn = (char *) malloc (strlen(home) + 40);
  sprintf (fn, "%s/.xscreensaver", home);

  if (!stat (fn, &st))
    {
      if (both_p || st.st_mtime != init_file_time)
        {
          Bool ov = verbose_p;
          init_file_time = st.st_mtime;
          read_init_file_simple (fn);
          read_p = True;

          /* Changes to verbose in .xscreenaver after startup are ignored; else
             running xscreensaver-settings would turn off cmd line -verbose. */
          if (!both_p) verbose_p = ov;
        }
    }

  if (blank_timeout < 5)
    blank_timeout = 60 * 60 * 10;

  free (fn);

  if (read_p && verbose_p)
    {
      fprintf (stderr, "%s: blank after: %d\n", blurb(), blank_timeout);
      if (lock_p)
        fprintf (stderr, "%s: lock after:  %d\n", blurb(), lock_timeout);
    }
}




/* We trap and ignore ALL protocol errors that happen after initialization.
   By default, Xlib would exit.  Ignoring an X error is less bad than
   crashing and unlocking.
 */
static Bool ignore_x11_error_p  = False;
static Bool error_handler_hit_p = False;
static Bool print_x11_error_p   = True;

static int
error_handler (Display *dpy, XErrorEvent *event)
{
  error_handler_hit_p = True;

  if (print_x11_error_p)
    {
      const char *b = blurb();
      const char *p = progname;
      fprintf (stderr, "\n%s: X ERROR! PLEASE REPORT THIS BUG!\n\n", b);
      progname = b;
      XmuPrintDefaultErrorMessage (dpy, event, stderr);
      progname = p;

# ifdef __GNUC__
  __extension__   /* don't warn about "string length is greater than the
                     length ISO C89 compilers are required to support". */
# endif
    fprintf (stderr, "\n"
   "#######################################################################\n"
   "\n"
   "    If at all possible, please re-run xscreensaver with the command\n"
   "    line arguments \"-sync -log log.txt\", and reproduce this bug.\n"
   "    Please include the complete \"log.txt\" file with your bug report.\n"
   "\n"
   "    https://www.jwz.org/xscreensaver/bugs.html explains how to create\n"
   "    the most useful bug reports.\n"
   "\n"
   "    The more information you can provide, the better.  But please\n"
   "    report this bug, regardless!\n"
   "\n"
   "#######################################################################\n"
   "\n"
   "\n");

      fflush (stderr);

      if (! ignore_x11_error_p)
        abort();
    }

  return 0;
}


/* Check for other running instances of XScreenSaver, gnome-screensaver, etc.
 */
static void
ensure_no_screensaver_running (Display *dpy)
{
  int screen, nscreens = ScreenCount (dpy);

  /* Silently ignore BadWindow race conditions. */
  Bool op = print_x11_error_p;
  print_x11_error_p = False;

  for (screen = 0; screen < nscreens; screen++)
    {
      int i;
      Window root = RootWindow (dpy, screen);
      Window root2 = 0, parent = 0, *kids = 0;
      unsigned int nkids = 0;

      if (! XQueryTree (dpy, root, &root2, &parent, &kids, &nkids))
        continue;
      if (root != root2)
        continue;
      if (parent)
        continue;
      for (i = 0; i < nkids; i++)
        {
          Atom type;
          int format;
          unsigned long nitems, bytesafter;
          unsigned char *version;

          if (XGetWindowProperty (dpy, kids[i], XA_SCREENSAVER_VERSION, 0, 1,
                                  False, XA_STRING, &type, &format, &nitems,
                                  &bytesafter, &version)
              == Success
              && type != None)
            {
              unsigned char *id;
              if (XGetWindowProperty (dpy, kids[i], XA_SCREENSAVER_ID, 0, 512,
                                      False, XA_STRING, &type, &format,
                                      &nitems, &bytesafter, &id)
                  != Success
                  || type == None)
                id = (unsigned char *) "???";

              fprintf (stderr,
                       "%s: already running on display %s"
                       " (window 0x%x)\n from process %s\n",
                       blurb(), DisplayString (dpy), (int) kids [i],
                       (char *) id);
              saver_exit (1);
            }
          else if (XGetWindowProperty (dpy, kids[i], XA_WM_COMMAND, 0, 128,
                                       False, XA_STRING, &type, &format,
                                       &nitems, &bytesafter, &version)
                   == Success
                   && type != None
                   && (!strcmp ((char *) version, "gnome-screensaver") ||
                       !strcmp ((char *) version, "mate-screensaver") ||
                       !strcmp ((char *) version, "cinnamon-screensaver") ||
                       !strcmp ((char *) version, "xfce4-screensaver") ||
                       !strcmp ((char *) version, "light-locker")))
            {
              fprintf (stderr,
                       "%s: \"%s\" is already running on display %s"
                       " (window 0x%x)\n",
                       blurb(), (char *) version,
                       DisplayString (dpy), (int) kids [i]);
              saver_exit (1);
            }
        }
      if (kids) XFree ((char *) kids);
    }

  print_x11_error_p = op;
}


/* Store a property on the root window indicating that xscreensaver is
   running, and whether it is blanked or locked.  This property is read
   by "xscreensaver-command" and by ensure_no_screensaver_running().
   This property is also overwritten by "xscreensaver-gfx" to indicate
   which screenhacks are running.
 */
static void
store_saver_status (Display *dpy,
                    Bool blanked_p, Bool locked_p, time_t blank_time)
{
  /* The contents of XA_SCREENSAVER_STATUS has LOCK/BLANK/0 in the first slot,
     the time at which that state began in the second slot, and the ordinal of
     the running hacks on each screen (1-based) in subsequent slots.  Since
     we don't know the hacks here (or even how many monitors are attached) we
     leave whatever was there before unchanged: it will be updated by
     "xscreensaver-gfx".

     XA_SCREENSAVER_STATUS is stored on the (real) root window of screen 0.

     XA_SCREENSAVER_VERSION and XA_SCREENSAVER_ID are stored on the unmapped
     window created by the "xscreensaver" process.  ClientMessage events are
     sent to that window, and the responses are sent via the
     XA_SCREENSAVER_RESPONSE property on it.

     These properties are not used on the windows created by "xscreensaver-gfx"
     for use by the display hacks.

     See the different version of this function in windows.c.
   */
  Window w = RootWindow (dpy, 0);  /* always screen 0 */
  Atom type;
  unsigned char *dataP = 0;
  PROP32 *status = 0;
  int format;
  unsigned long nitems, bytesafter;

  /* Read the old property, so we can change just parts. */
  if (XGetWindowProperty (dpy, w,
                          XA_SCREENSAVER_STATUS,
                          0, 999, False, XA_INTEGER,
                          &type, &format, &nitems, &bytesafter,
                          &dataP)
      == Success
      && type == XA_INTEGER
      && nitems >= 3
      && dataP)
    status = (PROP32 *) dataP;

  if (!status)	/* There was no existing property */
    {
      nitems = 3;
      status = (PROP32 *) malloc (nitems * sizeof(*status));
    }

  status[0] = (PROP32) (locked_p  ? XA_LOCK  : blanked_p ? XA_BLANK : 0);
  status[1] = (PROP32) blank_time;  /* Y2038 bug: unsigned 32 bit time_t */
  XChangeProperty (dpy, w, XA_SCREENSAVER_STATUS, XA_INTEGER, 32,
                   PropModeReplace, (unsigned char *) status, nitems);
  XSync (dpy, False);

# if 0
  if (debug_p && verbose_p)
    {
      int i;
      fprintf (stderr, "%s: wrote status property: 0x%lx: %s", blurb(),
               (unsigned long) w,
               (status[0] == XA_LOCK  ? "LOCK" :
                status[0] == XA_BLANK ? "BLANK" :
                status[0] == 0 ? "0" : "???"));
      for (i = 1; i < nitems; i++)
        fprintf (stderr, ", %lu", status[i]);
      fprintf (stderr, "\n");
      if (system ("xprop -root _SCREENSAVER_STATUS") <= 0)
        fprintf (stderr, "%s: xprop exec failed\n", blurb());
    }
# endif /* 0 */

  if (status != (PROP32 *) dataP)
    free (status);
  if (dataP)
    XFree (dataP);
}


/* This process does not map any windows on the screen.  However, it creates
   one hidden window on screen 0, which is the rendezvous point for
   communication with xscreensaver-command: that window is how it can tell
   that XScreenSaver is running, what the version number is, and it is where
   bidirectional ClientMessage communication takes place.  Since there are
   no "blanking" windows around at all when xscreensaver-gfx is not running,
   this window is needed.  We could have instead re-tooled xscreensaver-command
   to do all of its communication through the root window instead, but this 
   seemed easier.
 */
static void
create_daemon_window (Display *dpy)
{
  XClassHint class_hints;
  XSetWindowAttributes attrs;
  unsigned long attrmask = 0;
  const char *name = "???";
  const char *host = "???";
  char buf[20];
  pid_t pid = getpid();
  struct passwd *p = getpwuid (getuid ());
  time_t now = time ((time_t *) 0);
  char *id;
# ifdef HAVE_UNAME
  struct utsname uts;
# endif

  if (p && p->pw_name && *p->pw_name)
    name = p->pw_name;
  else if (p)
    {
      sprintf (buf, "%lu", (unsigned long) p->pw_uid);
      name = buf;
    }
  else
    name = "???";

# ifdef HAVE_UNAME
  {
    if (! uname (&uts))
      host = uts.nodename;
  }
# endif

  class_hints.res_name  = (char *) progname;  /* not const? */
  class_hints.res_class = "XScreenSaver";
  id = (char *) malloc (strlen(name) + strlen(host) + 50);
  sprintf (id, "%lu (%s@%s)", (unsigned long) pid, name, host);

  attrmask = CWOverrideRedirect | CWEventMask;
  attrs.override_redirect = True;
  attrs.event_mask = PropertyChangeMask;

  daemon_window = XCreateWindow (dpy, RootWindow (dpy, 0),
                                 0, 0, 1, 1, 0,
                                 DefaultDepth (dpy, 0), InputOutput,
                                 DefaultVisual (dpy, 0), attrmask, &attrs);
  XStoreName (dpy, daemon_window, "XScreenSaver Daemon");
  XSetClassHint (dpy, daemon_window, &class_hints);
  XChangeProperty (dpy, daemon_window, XA_WM_COMMAND, XA_STRING,
		   8, PropModeReplace, (unsigned char *) progname,
                   strlen (progname));
  XChangeProperty (dpy, daemon_window, XA_SCREENSAVER_VERSION, XA_STRING,
                   8, PropModeReplace, (unsigned char *) version_number,
		   strlen (version_number));
  XChangeProperty (dpy, daemon_window, XA_SCREENSAVER_ID, XA_STRING,
		   8, PropModeReplace, (unsigned char *) id, strlen (id));

  store_saver_status (dpy, False, False, now);
  free (id);
}


static const char *
grab_string (int status)
{
  switch (status) {
  case GrabSuccess:     return "GrabSuccess";
  case AlreadyGrabbed:  return "AlreadyGrabbed";
  case GrabInvalidTime: return "GrabInvalidTime";
  case GrabNotViewable: return "GrabNotViewable";
  case GrabFrozen:      return "GrabFrozen";
  default:
    {
      static char buf[255];
      sprintf(buf, "unknown status: %d", status);
      return buf;
    }
  }
}

static int
grab_kbd (Screen *screen)
{
  Display *dpy = DisplayOfScreen (screen);
  Window w = RootWindowOfScreen (screen);
  int status = XGrabKeyboard (dpy, w, True, GrabModeAsync, GrabModeAsync,
                              CurrentTime);
  if (verbose_p)
    fprintf (stderr, "%s: grabbing keyboard on 0x%lx: %s\n",
             blurb(), (unsigned long) w, grab_string (status));
  return status;
}


static int
grab_mouse (Screen *screen, Cursor cursor)
{
  Display *dpy = DisplayOfScreen (screen);
  Window w = RootWindowOfScreen (screen);
  int status = XGrabPointer (dpy, w, True, 
                             (ButtonPressMask   | ButtonReleaseMask |
                              EnterWindowMask   | LeaveWindowMask |
                              PointerMotionMask | PointerMotionHintMask |
                              Button1MotionMask | Button2MotionMask |
                              Button3MotionMask | Button4MotionMask |
                              Button5MotionMask | ButtonMotionMask),
                             GrabModeAsync, GrabModeAsync, w,
                             cursor, CurrentTime);
  if (verbose_p)
    fprintf (stderr, "%s: grabbing mouse on 0x%lx... %s\n",
             blurb(), (unsigned long) w, grab_string (status));
  return status;
}


static void
ungrab_kbd (Display *dpy)
{
  if (verbose_p)
    fprintf (stderr, "%s: ungrabbing keyboard\n", blurb());
  XUngrabKeyboard (dpy, CurrentTime);
}


static void
ungrab_mouse (Display *dpy)
{
  if (verbose_p)
    fprintf (stderr, "%s: ungrabbing mouse\n", blurb());
  XUngrabPointer (dpy, CurrentTime);
}


/* Some remote desktop clients (e.g., "rdesktop") hold the keyboard GRABBED the
   whole time they have focus!  This is idiotic because the whole point of
   grabbing is to get events when you do *not* have focus, so grabbing only
   when* you have focus is redundant.  Anyway, that prevents us from getting a
   keyboard grab.  It turns out that for some of these apps, de-focusing them
   forces them to release their grab.

   So if we fail to grab the keyboard four times in a row, we forcibly set
   focus to "None" and try four more times.  We don't touch focus unless we're
   already having a hard time getting a grab.
 */
static void
nuke_focus (Screen *screen)
{
  Display *dpy = DisplayOfScreen (screen);
  Window focus = 0;
  int rev = 0;

  XGetInputFocus (dpy, &focus, &rev);

  if (verbose_p)
    {
      char w[255], r[255];

      if      (focus == PointerRoot) strcpy (w, "PointerRoot");
      else if (focus == None)        strcpy (w, "None");
      else    sprintf (w, "0x%lx", (unsigned long) focus);

      if      (rev == RevertToParent)      strcpy (r, "RevertToParent");
      else if (rev == RevertToPointerRoot) strcpy (r, "RevertToPointerRoot");
      else if (rev == RevertToNone)        strcpy (r, "RevertToNone");
      else    sprintf (r, "0x%x", rev);

      fprintf (stderr, "%s: removing focus from %s / %s\n",
               blurb(), w, r);
    }

  XSetInputFocus (dpy, None, RevertToNone, CurrentTime);
}


static void
ungrab_keyboard_and_mouse (Display *dpy)
{
  ungrab_mouse (dpy);
  ungrab_kbd (dpy);
}


/* Returns true if it succeeds.
 */
static Bool
grab_keyboard_and_mouse (Screen *screen)
{
  Display *dpy = DisplayOfScreen (screen);
  Status mstatus = 0, kstatus = 0;
  int i;
  int retries = 4;
  Bool focus_fuckus = False;

 AGAIN:

  for (i = 0; i < retries; i++)
    {
      XSync (dpy, False);
      kstatus = grab_kbd (screen);
      if (kstatus == GrabSuccess)
        break;

      /* else, wait a second and try to grab again. */
      sleep (1);
    }

  if (kstatus != GrabSuccess)
    {
      fprintf (stderr, "%s: couldn't grab keyboard: %s\n",
               blurb(), grab_string (kstatus));

      if (! focus_fuckus)
        {
          focus_fuckus = True;
          nuke_focus (screen);
          goto AGAIN;
        }
    }

  for (i = 0; i < retries; i++)
    {
      XSync (dpy, False);
      mstatus = grab_mouse (screen, blank_cursor);
      if (mstatus == GrabSuccess)
        break;

      /* else, wait a second and try to grab again. */
      sleep (1);
    }

  if (mstatus != GrabSuccess)
    fprintf (stderr, "%s: couldn't grab pointer: %s\n",
             blurb(), grab_string (mstatus));


  /* When should we allow blanking to proceed?  The current theory
     is that a keyboard grab is mandatory; a mouse grab is optional.

     - If we don't have a keyboard grab, then we won't be able to
       read a password to unlock, so the kbd grab is mandatory.
       (We can't conditionalize this on locked_p, because someone
       might run "xscreensaver-command -lock" at any time.)

     - If we don't have a mouse grab, then we might not see mouse
       clicks as a signal to unblank -- but we will still see kbd
       activity, so that's not a disaster.

     If the mouse grab failed with AlreadyGrabbed, then I *think*
     that means that we will still see the mouse events via XInput2.
     But if it failed with GrabFrozen, that means that the grabber
     used GrabModeSync, and we will only receive those mouse events
     as a replay after they release the grab, which doesn't help us.

     If the keyboard grab failed with AlreadyGrabbed rather than
     GrabFrozen then we may still get those keypresses -- but so will
     the program holding the grab, so that's unacceptable for our
     purpose of reading passwords.

     It has been suggested that we should allow blanking if locking
     is disabled, and we have a mouse grab but no keyboard grab.
     That would allow screen blanking (but not locking) while the gdm
     login screen had the keyboard grabbed, but one would have to use
     the mouse to unblank.  Keyboard characters would go to the gdm
     login field without unblanking.  I have not made this change
     because I'm not completely convinced it is a safe thing to do.
   */

  if (kstatus != GrabSuccess)	/* Do not blank without a kbd grab.   */
    {
      /* If we didn't get both grabs, release the one we did get. */
      ungrab_keyboard_and_mouse (dpy);
      return False;
    }

  return True;			/* Grab is good, go ahead and blank.  */
}


/* Which screen is the mouse on?
 */
static Screen *
mouse_screen (Display *dpy)
{
  int i, nscreens = ScreenCount (dpy);
  if (nscreens > 1)
    for (i = 0; i < nscreens; i++)
      {
        Window pointer_root, pointer_child;
        int root_x, root_y, win_x, win_y;
        unsigned int mask;
        int status = XQueryPointer (dpy, RootWindow (dpy, i),
                                    &pointer_root, &pointer_child,
                                    &root_x, &root_y, &win_x, &win_y, &mask);
        if (status != None)
          {
            if (verbose_p)
              fprintf (stderr, "%s: mouse is on screen %d of %d\n",
                       blurb(), i, nscreens);
            return ScreenOfDisplay (dpy, i);
          }
      }

  return ScreenOfDisplay (dpy, 0);
}


static void
maybe_disable_locking (Display *dpy)
{
  const char *why = 0;
  Bool wayland_p = (getenv ("WAYLAND_DISPLAY") ||
                    getenv ("WAYLAND_SOCKET"));

# ifdef NO_LOCKING
  why = "locking disabled at compile time";
# endif

  if (!why)
    {
      uid_t u = getuid();
      if (u == 0 || u == (uid_t) -1 || u == (uid_t) -2)
        why = "cannot lock when running as root";
    }

  if (!why && getenv ("RUNNING_UNDER_GDM"))
    /* Launched as GDM's "Background" program */
    why = "cannot lock when running under GDM";

  /* X11 grabs don't work under Wayland's embedded X11 server.  The Wayland
     window manager lives at a higher level than the X11 emulation layer. */
  if (!why && wayland_p)
    why = "cannot lock securely under Wayland";

  if (!why)
    {
      /* Grabs under the macOS XDarwin server only affect other X11 programs:
         you can Cmd-Tab to "Terminal".  These extensions exist on later
         releases XQuartz. */
      int op = 0, event = 0, error = 0;
      if (XQueryExtension (dpy, "Apple-DRI", &op, &event, &error) ||
          XQueryExtension (dpy, "Apple-WM",  &op, &event, &error))
        why = "cannot lock securely under macOS X11";
    }

  if (why)
    {
      if (debug_p)
        {
          fprintf (stderr, "%s: %s\n", blurb(), why);
          fprintf (stderr, "%s: DEBUG MODE: allowing locking anyway!\n",
                   blurb());
        }
      else if (wayland_p)
        {
          const char *s = blurb();
          locking_disabled_p = True;

          /* Maybe we should just refuse to launch instead?  We can operate
             properly only if the user uses only X11 programs, and doesn't
             want to lock the screen.
           */
          fprintf (stderr, "\n"
              "%s: WARNING: Wayland is not supported.\n"
              "\n"
              "%s:     Under Wayland, idle-detection fails when non-X11\n"
              "%s:     programs are selected, meaning the screen may\n"
              "%s:     blank prematurely.  Also, locking is impossible.\n"
              "%s:     See the manual for instructions on configuring\n"
              "%s:     your system to use X11 instead of Wayland.\n\n",
                   s, s, s, s, s, s);
        }
      else
        {
          locking_disabled_p = True;
          if (lock_p || verbose_p)
            fprintf (stderr, "%s: locking disabled: %s\n", blurb(), why);
        }
    }
}


static void
main_loop (Display *dpy)
{
  int xi_opcode;
  time_t now = time ((time_t *) 0);
  time_t active_at = now;
  time_t blanked_at = 0;
  time_t ignore_activity_before = now;
  time_t last_checked_init_file = now;
  Bool authenticated_p = False;
  Bool ignore_motion_p = False;

  enum { UNBLANKED, BLANKED, LOCKED, AUTH } current_state = UNBLANKED;

  struct { time_t time; int x, y; } last_mouse = { 0, 0, 0 };

  maybe_disable_locking (dpy);
  init_xscreensaver_atoms (dpy);
  ensure_no_screensaver_running (dpy);

  if (! init_xinput (dpy, &xi_opcode))
    saver_exit (1);

  /* Disable server built-in screen saver. */
  XSetScreenSaver (dpy, 0, 0, 0, 0);
  XForceScreenSaver (dpy, ScreenSaverReset);

  /* It would be nice to sync the server's DPMS settings here to what is
     specified in the .xscreensaver file, but xscreensaver-gfx handles that,
     so that won't happen until the first time the screen blanks. */

  create_daemon_window (dpy);

  handle_signals();

  blank_cursor = None;   /* Cursor of window under mouse (which is blank). */
  auth_cursor = XCreateFontCursor (dpy, XC_top_left_arrow);

  if (strchr (version_number, 'a') || strchr (version_number, 'b'))
    splash_p = True;  /* alpha and beta releases */

  /* Launch "xscreensaver-auth" once at startup with either --splash or --init,
     The latter to handle the OOM-killer while setuid. */
  {
    char *av[10];
    int ac = 0;
    av[ac++] = SAVER_AUTH_PROGRAM;
    av[ac++] = (splash_p ? "--splash" : "--init");
    if (verbose_p)     av[ac++] = "--verbose";
    if (verbose_p > 1) av[ac++] = "--verbose";
    if (verbose_p > 2) av[ac++] = "--verbose";
    if (debug_p)       av[ac++] = "--debug";
    av[ac] = 0;
    saver_auth_pid = fork_and_exec (dpy, ac, av);
  }

# if defined(HAVE_LIBSYSTEMD) || defined(HAVE_LIBELOGIND) 
  /* Launch xscreensaver-systemd at startup. */
  {
    char *av[10];
    int ac = 0;
    av[ac++] = SAVER_SYSTEMD_PROGRAM;
    if (verbose_p || debug_p)
      av[ac++] = "--verbose";
    av[ac] = 0;
    saver_systemd_pid = fork_and_exec (dpy, ac, av);
  }
# endif /* HAVE_LIBSYSTEMD || HAVE_LIBELOGIND */


  /* X11 errors during startup initialization were fatal.
     Once we enter the main loop, they are printed but ignored.
   */
  XSync (dpy, False);
  ignore_x11_error_p = True;

  /************************************************************************
   Main loop
  ************************************************************************/

  while (1)
    {
      Bool force_blank_p = False;
      Bool force_lock_p = False;
      Atom blank_mode = 0;
      char blank_mode_arg[20] = { 0 };

      /* Wait until an event comes in, or a timeout. */
      {
        int xfd = ConnectionNumber (dpy);
        fd_set in_fds;
        struct timeval tv;
        time_t until;
        switch (current_state) {
        case UNBLANKED: until = active_at + blank_timeout; break;
        case BLANKED:   until = blanked_at + lock_timeout; break;
        default:        until = 0;
        }

        tv.tv_sec = 0;
        tv.tv_usec = 0;
        if (until >= now)
          tv.tv_sec = until - now;
          
        if (verbose_p > 3)
          {
            if (!tv.tv_sec)
              fprintf (stderr, "%s: block until input\n", blurb());
            else
              {
                struct tm tm;
                time_t t = now + tv.tv_sec;
                localtime_r (&t, &tm);
                fprintf (stderr,
                         "%s: block for %ld sec until %02d:%02d:%02d\n", 
                         blurb(), tv.tv_sec, tm.tm_hour, tm.tm_min, tm.tm_sec);
              }
          }

        FD_ZERO (&in_fds);
        FD_SET (xfd, &in_fds);
        select (xfd + 1, &in_fds, NULL, NULL, (tv.tv_sec ? &tv : NULL));
      }

      now = time ((time_t *) 0);


      /********************************************************************
       Signal handlers
       ********************************************************************/


      /* If a signal handler fired, it set a flag, and that caused select()
         to return.  Now that we are back on the program stack, handle
         those signals. */

      /* SIGHUP is the same as "xscreensaver-command -restart". */
      if (sighup_received)
        {
          sighup_received = 0;
          if (current_state == LOCKED)
            {
              fprintf (stderr,
                       "%s: SIGHUP received while locked: ignoring\n",
                       blurb());
            }
          else
            {
              fprintf (stderr, "%s: SIGHUP received: restarting\n", blurb());
              restart_process();   /* Does not return */
              fprintf (stderr, "%s: SIGHUP RESTART FAILED!\n", blurb());
            }
        }

      /* Upon SIGTERM, SIGINT or SIGQUIT we must kill our subprocesses
         before exiting.
       */
      if (sigterm_received)
        {
          int sig = sigterm_received;  /* Same handler for all 3 signals */
          const char *sn = (sig == SIGINT  ? "SIGINT" :
                            sig == SIGQUIT ? "SIGQUIT" : "SIGTERM");
          sigterm_received = 0;
          fprintf (stderr, "%s: %s received%s: exiting\n", blurb(), sn,
                   (current_state == LOCKED ? " while locked" : ""));

          /* Rather than calling saver_exit(), set our SIGTERM handler back to
             the default and re-signal it so that this process actually dies
             with a signal. */
          signal (sig, SIG_DFL);
          kill_all_subprocs();
          kill (getpid(), sig);
          abort();
        }

      /* SIGCHLD is fired any time one of our subprocesses dies.
         When "xscreensaver-auth" dies, it analyzes its exit code.
       */
      if (sigchld_received)
        authenticated_p = handle_sigchld (dpy, current_state != UNBLANKED);

      /* Now process any outstanding X11 events on the queue: user activity
         from XInput, and ClientMessages from xscreensaver-command.
       */
      while (XPending (dpy))
        {
          XEvent xev;
          XNextEvent (dpy, &xev);
          now = time ((time_t *) 0);

          /****************************************************************
           Client Messages
           Both xscreensaver and xscreensaver-gfx handle these; some are
           handled exclusively by one program or another, and some of them
           (select, next, prev) are handled by xscreensaver only if
           xscreensaver-gfx is not already running.
           ****************************************************************/

          switch (xev.xany.type) {
          case ClientMessage:
            {
              Atom msg = xev.xclient.data.l[0];

              /* Re-load the .xscreensaver file before processing the results
                 of any ClientMessage, in case they just changed the timeouts.
               */
              last_checked_init_file = 0;

              if (! (xev.xclient.message_type == XA_SCREENSAVER &&
                     xev.xclient.format == 32 &&
                     xev.xclient.data.l[0]))
                {
                  goto BAD_CM;
                }
              else if (msg == XA_ACTIVATE ||
                       msg == XA_SELECT ||
                       msg == XA_DEMO ||
                       msg == XA_NEXT ||
                       msg == XA_PREV)
                {
                  /* The others are the same as -activate except that they
                     cause some extra args to be added to the xscreensaver-gfx
                     command line.
                   */
                  if (msg != XA_ACTIVATE)
                    blank_mode = msg;
                  if (msg == XA_SELECT || msg == XA_DEMO)
                    { /* see remote.c */
                      unsigned long  n = xev.xclient.data.l[1];
                      if (n == 5000) n = xev.xclient.data.l[2];
                      sprintf (blank_mode_arg, "%lu", n);
                    }

                  if (msg == XA_DEMO) ignore_motion_p = True;

                  if (current_state == UNBLANKED)
                    {
                      force_blank_p = True;
                      ignore_activity_before = now + 2;
                      clientmessage_response (dpy, &xev, True, "blanking");
                    }
                  else if (msg == XA_SELECT ||
                           msg == XA_NEXT ||
                           msg == XA_PREV)
                    {
                      /* When active, these are handled by xscreensaver-gfx
                         instead of xscreensaver, so silently ignore them,
                         and allow xscreensaver-gfx to reply instead. */
                    }
                  else
                    clientmessage_response (dpy, &xev, False,
                                            "already active");
                }
              else if (msg == XA_CYCLE)
                {
                  if (current_state == UNBLANKED)
                    /* Only allowed when screen already blanked */
                    clientmessage_response (dpy, &xev, False, "not blanked");
                  /* else xscreensaver-gfx will respond to this. */
                }
              else if (msg == XA_DEACTIVATE)
                {
                  if (current_state == UNBLANKED)
                    {
                      clientmessage_response (dpy, &xev, True,
                                 "already inactive, resetting activity time");
                      active_at = now;
                      ignore_activity_before = now;
                    }
                  else
                    {
                      /* This behaves just like user input: if state is
                         LOCKED, it will advance to AUTH. */
                      active_at = now;
                      ignore_activity_before = now;
                      clientmessage_response (dpy, &xev, True, "deactivating");
                    }

                  /* DEACTIVATE while inactive also needs to reset the
                     server's DPMS time, but doing that here would mean
                     linking with additional libraries, doing additional X
                     protocol, and also some finicky error handling, since
                     the DPMS extension is a pain in the ass.  So instead,
                     I made xscreensaver-command do that instead.  This
                     somewhat breaks the abstraction of ClientMessage
                     handling, but it's more robust. */
                }
              else if (msg == XA_LOCK)
                {
                  if (locking_disabled_p)
                    clientmessage_response (dpy, &xev, False,
                                            "locking disabled");
                  else if (current_state == UNBLANKED ||
                           current_state == BLANKED)
                    {
                      force_lock_p = True;
                      ignore_activity_before = now + 2;
                      clientmessage_response (dpy, &xev, True, "locking");
                    }
                  else
                    clientmessage_response (dpy, &xev, False,
                                            "already locked");
                }
              else if (msg == XA_SUSPEND)
                {
                  force_blank_p = True;
                  if (lock_p) force_lock_p = True;
                  ignore_activity_before = now + 2;
                  blank_mode = msg;
                  clientmessage_response (dpy, &xev, True, "suspending");
                }
              else if (msg == XA_EXIT)
                {
                  if (current_state == UNBLANKED ||
                      current_state == BLANKED)
                    {
                      clientmessage_response (dpy, &xev, True, "exiting");
                      XSync (dpy, False);
                      saver_exit (0);
                    }
                  else
                    clientmessage_response (dpy, &xev, False,
                                            "screen is locked");
                }
              else if (msg == XA_RESTART)
                {
                  if (current_state == UNBLANKED ||
                      current_state == BLANKED)
                    {
                      clientmessage_response (dpy, &xev, True, "restarting");
                      XSync (dpy, False);
                      restart_process();   /* Does not return */
                      fprintf (stderr, "%s: RESTART FAILED!\n", blurb());
                    }
                  else
                    clientmessage_response (dpy, &xev, False,
                                            "screen is locked");
                }
              else
                {
                BAD_CM:
                  if (verbose_p)
                    {
                      Atom type;
                      char *tstr, *name;
                      Bool op = print_x11_error_p;
                      print_x11_error_p = False;     /* Ignore BadAtom */
                      type = xev.xclient.message_type;
                      tstr = type ? XGetAtomName (dpy, type) : 0;
                      name = msg  ? XGetAtomName (dpy, msg)  : 0;
                      fprintf (stderr,
                               "%s: unrecognized ClientMessage %s %s\n",
                               blurb(),
                               (tstr ? tstr : "???"),
                               (name ? name : "???"));
                      if (tstr) XFree (tstr);
                      if (name) XFree (name);
                      print_x11_error_p = op;
                    }
                }
              continue;
            }


          /****************************************************************
           Normal X11 keyboard and mouse events
           ****************************************************************/

          /* XInput2 "raw" events bypass X11 grabs, but grabs take priority.
             If this process has the keyboard and mouse grabbed, it receives
             the following events:

               - X11 KeyPress, KeyRelease
               - X11 ButtonPress, ButtonRelease
               - X11 MotionNotify
               - XInput XI_RawButtonPress, XI_RawButtonRelease
               - XInput XI_RawMotion

             Note that button and motion events are doubly reported, but
             keyboard events are not.

             If this process does *not* have the keyboard and mouse grabbed,
             it receives the following events, regardless of the window in
             which they occur:

               - XInput XI_RawKeyPress, XI_RawKeyRelease
               - XInput XI_RawButtonPress, XI_RawButtonRelease
               - XInput XI_RawMotion

             But here's an irritating kink: though XInput2 generally allows
             snooping of everything, it respects GrabModeSync.  What this
             means is that if some other process has the keyboard grabbed with
             "Sync" instead of "Async", then this process will not see any of
             the events until that process releases its grab, and then the
             events come in late, all at once.  Verify this by running:

               test-xinput &
                  Note that keyboard and mouse events are detected.
               test-grab --mouse --mouse-async --kbd-async
                  Same.
               test-grab --mouse --mouse-sync --kbd-async
                  Keyboard events are detected.
                  No motion or button events until "test-grab" is killed.
               test-grab --mouse --mouse-async --kbd-sync
                  Vice versa.
           */

          case KeyPress:
          case KeyRelease:
            if (current_state != AUTH &&  /* logged by xscreensaver-auth */
                (verbose_p > 1 ||
                 (verbose_p && now - active_at > 1)))
              print_xinput_event (dpy, &xev, "");
            active_at = now;
            continue;
            break;
          case ButtonPress:
          case ButtonRelease:
            active_at = now;
            if (verbose_p)
              print_xinput_event (dpy, &xev, "");
            continue;
            break;
          case MotionNotify:
            /* Since we always get XI_RawMotion, but only get MotionNotify
               when grabbed, we can just ignore MotionNotify and let the
               XI_RawMotion clause handle hysteresis. */
            if (verbose_p > 1)
              print_xinput_event (dpy, &xev, "ignored");
            continue;
            break;
          default:
            break;
          }


          /****************************************************************
           XInput keyboard and mouse events
           ****************************************************************/

          if (xev.xcookie.type != GenericEvent ||
              xev.xcookie.extension != xi_opcode)
            continue;  /* not an XInput event */

          if (!xev.xcookie.data)
            XGetEventData (dpy, &xev.xcookie);
          if (!xev.xcookie.data)
            continue;  /* Bogus XInput event */

          switch (xev.xcookie.evtype) {
          case XI_RawKeyPress:
          case XI_RawKeyRelease:
          case XI_RawButtonPress:
          case XI_RawButtonRelease:
          case XI_RawTouchBegin:
          case XI_RawTouchEnd:
          case XI_RawTouchUpdate:
            if (current_state != AUTH &&  /* logged by xscreensaver-auth */
                (verbose_p > 1 ||
                 (verbose_p && now - active_at > 1)))
              print_xinput_event (dpy, &xev, "");
            active_at = now;
            break;

          case XI_RawMotion:
            {
              /* Mouse wheel scrolling sends Button4 and Button5 events as well
                 as motion, so we handled those above, in XI_RawButtonPress.
                 The raw_values in the motion event for a mouse wheel reflect
                 the position of the wheel sensor.

                 On a trackpad where two-finger-swipe is a scroll gesture, I
                 saw behavior identical to a mouse wheel -- it does not send
                 RawTouch events.
               */

              /* Don't poll the mouse position more frequently than once
                 a second.  The motion only counts as activity if it has
                 moved farther than N pixels per second.
               */
              int secs = now - last_mouse.time;
              if (secs >= 1)
                {
                  Window root_ret, child_ret;
                  int root_x, root_y;
                  int win_x, win_y;
                  unsigned int mask;
                  int dist;
                  Bool ignored_p = False;

                  XQueryPointer (dpy, DefaultRootWindow (dpy),
                                 &root_ret, &child_ret, &root_x, &root_y,
                                 &win_x, &win_y, &mask);
                  dist = MAX (ABS (last_mouse.x - root_x),
                              ABS (last_mouse.y - root_y));

                  ignored_p = (ignore_motion_p || dist < pointer_hysteresis);

                  if (! ignored_p)
                    {
                      active_at = now;
                      last_mouse.time = now;
                      last_mouse.x = root_x;
                      last_mouse.y = root_y;
                    }

                  if (verbose_p > 1 ||
                      (verbose_p && now - active_at > 5))
                    print_xinput_event (dpy, &xev, 
                                        (ignored_p ? " ignored" : ""));
                }
            }
            break;

          default:
            if (verbose_p)
              print_xinput_event (dpy, &xev, "");
            break;
          }

          XFreeEventData (dpy, &xev.xcookie);
        }


      /********************************************************************
       Advancing the state machine
       ********************************************************************/

      /* If it's time, see if the .xscreensaver file has changed, since that
         might change the blank and lock timeouts.
       */
      if (now >= last_checked_init_file + 60)
        {
          last_checked_init_file = now;
          if (verbose_p)
            fprintf(stderr,"%s: checking init file\n", blurb());
          read_init_files (False);
        }

      /* Now that events have been processed, see if the state should change,
         based on any events received and the current time.
       */
      switch (current_state) {
      case UNBLANKED:
        if (!locking_disabled_p &&
            (force_lock_p ||
             (lock_p && 
              now >= active_at + blank_timeout + lock_timeout)))
          {
            if (verbose_p)
              fprintf (stderr, "%s: locking\n", blurb());
            if (grab_keyboard_and_mouse (mouse_screen (dpy)))
              {
                current_state = LOCKED;
                blanked_at = now;
                authenticated_p = False;
                store_saver_status (dpy, True, True, now);
              }
            else
              fprintf (stderr, "%s: unable to grab -- locking aborted!\n",
                       blurb());

            force_lock_p = False;   /* Single shot */
          }
        else if (force_blank_p ||
                 now >= active_at + blank_timeout)
          {
            if (blanking_disabled_p && !force_blank_p)
              {
                if (verbose_p)
                  fprintf (stderr, "%s: not blanking: disabled\n", blurb());
              }
            else
              {
                if (verbose_p)
                  fprintf (stderr, "%s: blanking\n", blurb());
                if (grab_keyboard_and_mouse (mouse_screen (dpy)))
                  {
                    current_state = BLANKED;
                    blanked_at = now;
                    store_saver_status (dpy, True, False, now);
                  }
                else
                  fprintf (stderr, "%s: unable to grab -- blanking aborted!\n",
                           blurb());
              }
          }

        if (current_state == BLANKED || current_state == LOCKED)
          {
            /* Grab succeeded and state changed: launch graphics. */
            if (! saver_gfx_pid)
              {
                static Bool first_time_p = True;
                char *av[20];
                int ac = 0;
                av[ac++] = SAVER_GFX_PROGRAM;
                if (first_time_p)  av[ac++] = "--init";
                if (verbose_p)     av[ac++] = "--verbose";
                if (verbose_p > 1) av[ac++] = "--verbose";
                if (verbose_p > 2) av[ac++] = "--verbose";
                if (debug_p)       av[ac++] = "--debug";

                if (blank_mode == XA_NEXT)
                  av[ac++] = "--next";
                else if (blank_mode == XA_PREV)
                  av[ac++] = "--prev";
                else if (blank_mode == XA_SELECT)
                  av[ac++] = "--select";
                else if (blank_mode == XA_DEMO)
                  av[ac++] = "--demo";
                else if (blank_mode == XA_SUSPEND)
                  av[ac++] = "--emergency";
                  
                if (blank_mode == XA_SELECT || blank_mode == XA_DEMO)
                  av[ac++] = blank_mode_arg;

                av[ac] = 0;
                gfx_stopped_p = False;
                saver_gfx_pid = fork_and_exec (dpy, ac, av);
                respawn_thrashing_count = 0;
                first_time_p = False;
              }
          }
        break;

      case BLANKED:
        if (!locking_disabled_p &&
            (force_lock_p ||
             (lock_p && 
              now >= blanked_at + lock_timeout)))
          {
            if (verbose_p)
              fprintf (stderr, "%s: locking%s\n", blurb(),
                       (force_lock_p ? "" : " after timeout"));
            current_state = LOCKED;
            authenticated_p = False;
            store_saver_status (dpy, True, True, now);
            force_lock_p = False;   /* Single shot */
          }
        else if (active_at >= now &&
                 active_at >= ignore_activity_before)
          {
          UNBLANK:
            if (verbose_p)
              fprintf (stderr, "%s: unblanking\n", blurb());
            current_state = UNBLANKED;
            ignore_motion_p = False;
            store_saver_status (dpy, False, False, now);

            if (saver_gfx_pid)
              {
                if (verbose_p)
                  fprintf (stderr,
                           "%s: pid %lu: killing " SAVER_GFX_PROGRAM "\n",
                           blurb(), (unsigned long) saver_gfx_pid);
                kill (saver_gfx_pid, SIGTERM);
                respawn_thrashing_count = 0;

                if (gfx_stopped_p)  /* SIGCONT to allow SIGTERM to proceed */
                  {
                    if (verbose_p)
                      fprintf (stderr, "%s: pid %lu: sending "
                               SAVER_GFX_PROGRAM " SIGCONT\n",
                               blurb(), (unsigned long) saver_gfx_pid);
                    gfx_stopped_p = False;
                    kill (-saver_gfx_pid, SIGCONT);  /* send to process group */
                  }
              }

            ungrab_keyboard_and_mouse (dpy);
          }
        break;

      case LOCKED:
        if (active_at >= now &&
            active_at >= ignore_activity_before)
          {
            char *av[10];
            int ac = 0;

            if (saver_gfx_pid)
              {
                /* To suspend or not suspend?  If we don't suspend, then a
                   misbehaving or heavily-loaded screenhack might make it more
                   difficult to type in the password.  Though that seems pretty
                   unlikely.

                   But if we do suspend, then attaching a new monitor while
                   the unlock dialog is up will cause a new desktop to be
                   visible, since "xscreensaver-gfx" is the process that
                   handles RANDR.  You can't interact with that new desktop,
                   but you can see it, possibly including (depending on the
                   window manager) the names of file icons on the desktop, or
                   other things. */
# if 0
                if (verbose_p)
                  fprintf (stderr, "%s: pid %lu: sending " SAVER_GFX_PROGRAM
                           " SIGSTOP\n", blurb(),
                           (unsigned long) saver_gfx_pid);
                gfx_stopped_p = True;
                kill (-saver_gfx_pid, SIGSTOP);  /* send to process group */
# endif
              }

            if (verbose_p)
              fprintf (stderr, "%s: authorizing\n", blurb());
            current_state = AUTH;

            /* We already hold the mouse grab, but try to re-grab it with
               a different mouse pointer, so that the pointer shows up while
               the auth dialog is raised.  We can ignore failures here. */
            grab_mouse (mouse_screen (dpy), auth_cursor);

            av[ac++] = SAVER_AUTH_PROGRAM;
            if (verbose_p)     av[ac++] = "--verbose";
            if (verbose_p > 1) av[ac++] = "--verbose";
            if (verbose_p > 2) av[ac++] = "--verbose";
            if (debug_p)       av[ac++] = "--debug";
            av[ac] = 0;
            saver_auth_pid = fork_and_exec (dpy, ac, av);
          }
        break;

      case AUTH:
        if (saver_auth_pid)
          {
            /* xscreensaver-auth still running -- wait for it to exit. */
          }
        else if (authenticated_p)
          {
            /* xscreensaver-auth exited with "success" status */
            if (verbose_p)
              fprintf (stderr, "%s: unlocking\n", blurb());
            authenticated_p = False;
            goto UNBLANK;
          }
        else
          {
            /* xscreensaver-auth exited with non-success, or with signal. */
            if (verbose_p)
              fprintf (stderr, "%s: authorization failed\n", blurb());
            current_state = LOCKED;
            authenticated_p = False;

            /* We already hold the mouse grab, but try to re-grab it with
               a different mouse pointer, to hide the pointer again now that
               the auth dialog is gone.  We can ignore failures here. */
            grab_mouse (mouse_screen (dpy), blank_cursor);

            /* When the unlock dialog is dismissed, ignore any input for a
               second to give the user time to take their hands off of the
               keyboard and mouse, so that it doesn't pop up again
               immediately. */
            ignore_activity_before = now + 1;

            if (gfx_stopped_p)	/* SIGCONT to resume savers */
              {
                if (verbose_p)
                  fprintf (stderr, "%s: pid %lu: sending " SAVER_GFX_PROGRAM
                           " SIGCONT\n",
                           blurb(), (unsigned long) saver_gfx_pid);
                gfx_stopped_p = False;
                kill (-saver_gfx_pid, SIGCONT);  /* send to process group */
              }
          }
        break;

      default:
        /* abort(); */
        break;
      }
    }
}


/* There is no good reason for this executable to be setuid, but in case
   it is, turn that off.
 */
static const char *
disavow_privileges (void)
{
  static char msg[255];
  uid_t euid = geteuid();
  gid_t egid = getegid();
  uid_t uid = getuid();
  gid_t gid = getgid();

  if (uid == euid && gid == egid)
    return 0;

  /* Without setgroups(), the process will retain any supplementary groups
     associated with the uid, e.g. the default groups of the "root" user.
     But setgroups() can only be called by root, and returns EPERM for other
     users even if the call would be a no-op.  So ignore its error status.
   */
  setgroups (1, &gid);

  if (gid != egid && setgid (gid) != 0)
    {
      fprintf (stderr, "%s: setgid %d -> %d failed\n", blurb(), egid, gid);
      exit (1);
    }

  if (uid != euid && setgid (gid) != 0)
    {
      fprintf (stderr, "%s: setuid %d -> %d failed\n", blurb(), euid, uid);
      exit (1);
    }

  /* Return the message since this is before verbose_p is set or log opened. */
  sprintf (msg, "setuid %d:%d -> %d:%d", euid, egid, uid, gid);
  return msg;
}


int
main (int argc, char **argv)
{
  Display *dpy = 0;
  char *dpy_str = getenv ("DISPLAY");
  char *logfile = 0;
  Bool sync_p = False;
  Bool cmdline_verbose_val = False, cmdline_verbose_p = False;
  Bool cmdline_splash_val  = False, cmdline_splash_p  = False;
  const char *s;
  const char *pmsg;
  int i;

  progname = argv[0];
  s = strrchr (progname, '/');
  if (s) progname = s+1;

  pmsg = disavow_privileges();

  fclose (stdin);
  fix_fds();

  for (i = 1; i < argc; i++)
    {
      const char *oa = argv[i];
      /* XScreenSaver predates the "--arg" convention. */
      if (argv[i][0] == '-' && argv[i][1] == '-')
        argv[i]++;

      if (!strcmp (argv[i], "-debug"))
        debug_p = True;
      else if (!strcmp (argv[i], "-v") || !strcmp (argv[i], "-verbose"))
        {
          verbose_p++;
          cmdline_verbose_p = True, cmdline_verbose_val = verbose_p;
        }
      else if (!strcmp (argv[i], "-vv"))
        {
          verbose_p += 2;
          cmdline_verbose_p = True, cmdline_verbose_val = verbose_p;
        }
      else if (!strcmp (argv[i], "-vvv"))
        {
          verbose_p += 3;
          cmdline_verbose_p = True, cmdline_verbose_val = verbose_p;
        }
      else if (!strcmp (argv[i], "-vvvv"))
        {
          verbose_p += 4;
          cmdline_verbose_p = True, cmdline_verbose_val = verbose_p;
        }
      else if (!strcmp (argv[i], "-q") || !strcmp (argv[i], "-quiet"))
        {
          verbose_p = 0;
          cmdline_verbose_p = True, cmdline_verbose_val = verbose_p;
        }
      else if (!strcmp (argv[i], "-splash"))
        {
          splash_p = True;
          cmdline_splash_p = True, cmdline_splash_val = splash_p;
        }
      else if (!strcmp (argv[i], "-no-splash") ||
               !strcmp (argv[i], "-nosplash"))
        {
          splash_p = False;
          cmdline_splash_p = True, cmdline_splash_val = splash_p;
        }
      else if (!strcmp (argv[i], "-log"))
        {
          logfile = argv[++i];
          if (!logfile) goto HELP;
          if (! verbose_p)  /* might already be -vv */
            verbose_p = cmdline_verbose_p = cmdline_verbose_val = True;
        }
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
        sync_p = True;
      else if (!strcmp (argv[i], "-h") || !strcmp (argv[i], "-help"))
        {
        HELP:
          print_banner();
          fprintf (stderr, 
                   "\t\thttps://www.jwz.org/xscreensaver/\n"
                   "\n"
                   "\tOptions:\n\n"
                   "\t\t--dpy host:display.screen\n"
                   "\t\t--verbose\n"
                   "\t\t--no-splash\n"
                   "\t\t--log logfile\n"
                   "\n"
                   "\tRun 'xscreensaver-settings' to configure.\n"
                   "\n");
          saver_exit (1);
        }
      else
        {
          fprintf (stderr, "\n%s: unknown option: %s\n\n", blurb(), oa);
          goto HELP;
        }
    }

  if (logfile)
    {
      int stdout_fd = 1;
      int stderr_fd = 2;
      int fd = open (logfile, O_WRONLY | O_APPEND | O_CREAT, 0666);
      if (fd < 0)
        {
          char buf[255];
        FAIL:
          sprintf (buf, "%.100s: %.100s", blurb(), logfile);
          perror (buf);
          fflush (stderr);
          fflush (stdout);
          saver_exit (1);
        }

      fprintf (stderr, "%s: logging to file %s\n", blurb(), logfile);

      if (dup2 (fd, stdout_fd) < 0) goto FAIL;
      if (dup2 (fd, stderr_fd) < 0) goto FAIL;

      fprintf (stderr, "\n\n"
               "#####################################"
               "#####################################\n"
               "%s: logging to \"%s\"\n"
               "#####################################"
               "#####################################\n"
               "\n",
               blurb(), logfile);

      if (!verbose_p)
        verbose_p = True;
      cmdline_verbose_val = verbose_p;
    }

  save_argv (argc, argv);
  hack_environment();
  print_banner();
  read_init_files (True);

  /* Command line overrides init file */
  if (cmdline_verbose_p) verbose_p = cmdline_verbose_val;
  if (cmdline_splash_p)  splash_p  = cmdline_splash_val;

  if (verbose_p)
    fprintf (stderr, "%s: running in process %lu\n", blurb(),
             (unsigned long) getpid());

  if (verbose_p && pmsg)
    fprintf (stderr, "%s: %s\n", blurb(), pmsg);

  if (! dpy_str)
    {
      dpy_str = ":0.0";
      fprintf (stderr,
               "%s: warning: $DISPLAY is not set: defaulting to \"%s\"\n",
               blurb(), dpy_str);
    }

  /* Copy the -dpy arg to $DISPLAY for subprocesses. */
  {
    char *s = (char *) malloc (strlen(dpy_str) + 20);
    sprintf (s, "DISPLAY=%s", dpy_str);
    putenv (s);
    /* free (s); */  /* some versions of putenv do not copy */
  }

  dpy = XOpenDisplay (dpy_str);
  if (!dpy) saver_exit (1);

  if (sync_p)
    {
      XSynchronize (dpy, True);
      XSync (dpy, False);
    }

  XSetErrorHandler (error_handler);

  main_loop (dpy);
  saver_exit (0);
  return (1);
}
