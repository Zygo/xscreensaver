/* subprocs.c --- choosing, spawning, and killing screenhacks.
 * xscreensaver, Copyright © 1991-2021 Jamie Zawinski <jwz@jwz.org>
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

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>		/* not used for much... */
#include <X11/Xatom.h>
#include <X11/Intrinsic.h>	/* For XtAppAddSignal */

#ifndef ESRCH
# include <errno.h>
#endif

#include <sys/time.h>		/* sys/resource.h needs this for timeval */
#include <sys/param.h>		/* for PATH_MAX */

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>		/* for waitpid() and associated macros */
#endif

#ifdef HAVE_SETRLIMIT
# include <sys/resource.h>	/* for setrlimit() and RLIMIT_AS */
#endif

#include <signal.h>		/* for the signal names */
#include <time.h>

#ifdef ENABLE_NLS
# include <libintl.h>
# define _(S) gettext(S)
#else
# define _(S) (S)
#endif

#include "xscreensaver.h"
#include "exec.h"
#include "yarandom.h"
#include "visual.h"		/* for id_to_visual() */


enum job_status {
  job_running,	/* the process is still alive */
  job_stopped,	/* we have sent it a STOP signal */
  job_killed,	/* we have sent it a TERM signal */
  job_dead	/* we have wait()ed for it, and it's dead -- this state only
		   occurs so that we can avoid calling free() from a signal
		   handler.  Shortly after going into this state, the list
		   element will be removed. */
};

struct screenhack_job {
  char *name;
  pid_t pid;
  int screen;
  enum job_status status;
  time_t launched, killed;
  struct screenhack_job *next;
};

static struct screenhack_job *jobs = 0;

static void clean_job_list (void);
static void await_dying_children (saver_info *si);
static void describe_dead_child (saver_info *, pid_t, int wait_status,
                                 struct rusage);

static XtSignalId xt_sigterm_id = 0;
static int sigterm_received = 0;

static XtSignalId xt_sigchld_id = 0;
static int sigchld_received = 0;


const char *
signal_name (int signal)
{
  /* sys_signame[], sys_siglist[], strsignal() and sigabbrev_np()
     are an unportable mess. */
  switch (signal) {
  case SIGHUP:	  return "SIGHUP";
  case SIGINT:	  return "SIGINT";
  case SIGQUIT:	  return "SIGQUIT";
  case SIGILL:	  return "SIGILL";
  case SIGTRAP:	  return "SIGTRAP";
#ifdef SIGABRT
  case SIGABRT:	  return "SIGABRT";
#endif
  case SIGFPE:	  return "SIGFPE";
  case SIGKILL:	  return "SIGKILL";
  case SIGBUS:	  return "SIGBUS";
  case SIGSEGV:	  return "SIGSEGV";
  case SIGPIPE:	  return "SIGPIPE";
  case SIGALRM:	  return "SIGALRM";
  case SIGTERM:	  return "SIGTERM";
#ifdef SIGSTOP
  case SIGSTOP:	  return "SIGSTOP";
#endif
#ifdef SIGCONT
  case SIGCONT:	  return "SIGCONT";
#endif
#ifdef SIGUSR1
  case SIGUSR1:	  return "SIGUSR1";
#endif
#ifdef SIGUSR2
  case SIGUSR2:	  return "SIGUSR2";
#endif
#ifdef SIGEMT
  case SIGEMT:	  return "SIGEMT";
#endif
#ifdef SIGSYS
  case SIGSYS:	  return "SIGSYS";
#endif
  case SIGCHLD:	  return "SIGCHLD";
#ifdef SIGPWR
  case SIGPWR:	  return "SIGPWR";
#endif
#ifdef SIGWINCH
  case SIGWINCH:  return "SIGWINCH";
#endif
#ifdef SIGURG
  case SIGURG:	  return "SIGURG";
#endif
#ifdef SIGIO
  case SIGIO:	  return "SIGIO";
#endif
#ifdef SIGVTALRM
  case SIGVTALRM: return "SIGVTALRM";
#endif
#ifdef SIGXCPU
  case SIGXCPU:	  return "SIGXCPU";
#endif
#ifdef SIGXFSZ
  case SIGXFSZ:	  return "SIGXFSZ";
#endif
#ifdef SIGDANGER
  case SIGDANGER: return "SIGDANGER";
#endif
  default:
    {
      static char buf[50];
      sprintf(buf, "signal %d\n", signal);
      return buf;
    }
  }
}


/* Management of child processes, and de-zombification.
 */

static char *
timestring (time_t when)
{
  static char buf[255] = { 0 };
  struct tm tm;
  localtime_r (&when, &tm);
  sprintf (buf, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
  return buf;
}

#ifdef DEBUG
void show_job_list (void);
void
show_job_list (void)
{
  struct screenhack_job *job;
  fprintf(stderr, "%s: job list:\n", blurb());
  for (job = jobs; job; job = job->next)
    {
      fprintf (stderr, "  %5ld: %2d: (%s) %s %s\n",
               (long) job->pid,
               job->screen,
               (job->status == job_running ? "running" :
                job->status == job_stopped ? "stopped" :
                job->status == job_killed  ? " killed" :
                job->status == job_dead    ? "   dead" : "    ???"),
               (job->killed   ? timestring (job->killed) :
                job->launched ? timestring (job->launched) :
                "??:??:??"),
               job->name);
    }
  fprintf (stderr, "\n");
}
#endif /* DEBUG */


static void
make_job (pid_t pid, int screen, const char *cmd)
{
  struct screenhack_job *job = (struct screenhack_job *) malloc (sizeof(*job));

  static char name [1024];
  const char *in = cmd;
  char *out = name;
  int got_eq = 0;

  clean_job_list();

 AGAIN:
  while (*in && isspace(*in)) in++;		/* skip whitespace */
  while (*in && !isspace(*in) && *in != ':') {
    if (*in == '=') got_eq = 1;
    *out++ = *in++;			/* snarf first token */
  }

  if (got_eq)				/* if the first token was FOO=bar */
    {					/* then get the next token instead. */
      got_eq = 0;
      out = name;
      goto AGAIN;
    }

  while (*in && isspace(*in)) in++;		/* skip whitespace */
  *out = 0;

  job->name = strdup(name);
  job->pid = pid;
  job->screen = screen;
  job->status = job_running;
  job->launched = time ((time_t *) 0);
  job->killed = 0;
  job->next = jobs;
  jobs = job;
}


static void
free_job (struct screenhack_job *job)
{
  if (!job)
    return;
  else if (job == jobs)
    jobs = jobs->next;
  else
    {
      struct screenhack_job *job2, *prev;
      for (prev = 0, job2 = jobs;
	   job2;
	   prev = job2, job2 = job2->next)
	if (job2 == job)
	  {
	    prev->next = job->next;
	    break;
	  }
    }
  free(job->name);
  free(job);
}


/* Cleans out dead jobs from the jobs list.
 */
static void
clean_job_list (void)
{
  struct screenhack_job *job, *prev, *next;
  time_t now = time ((time_t *) 0);
  static time_t last_warn = 0;
  Bool warnedp = False;

  for (prev = 0, job = jobs, next = (job ? job->next : 0);
       job;
       prev = job, job = next, next = (job ? job->next : 0))
    {
      if (job->status == job_dead)
	{
	  if (prev)
	    prev->next = next;
	  free_job (job);
	  job = prev;
	}
      else if (job->status == job_killed &&
               now - job->killed > 10 &&
               now - last_warn   > 10)
        {
          fprintf (stderr,
                   "%s: WARNING: pid %ld (%s) sent SIGTERM %ld seconds ago"
                   " and did not die!\n",
                   blurb(),
                   (long) job->pid,
                   job->name,
                   (long) (now - job->killed));
          warnedp = True;
        }
    }
  if (warnedp) last_warn = now;
}


static struct screenhack_job *
find_job (pid_t pid)
{
  struct screenhack_job *job;
  for (job = jobs; job; job = job->next)
    if (job->pid == pid)
      return job;
  return 0;
}


static int
kill_job (saver_info *si, pid_t pid, int signal)
{
  saver_preferences *p = &si->prefs;
  struct screenhack_job *job;
  int status = -1;

  clean_job_list();

  job = find_job (pid);
  if (!job ||
      !job->pid ||
      job->status == job_killed)
    {
      if (p->verbose_p)
	fprintf (stderr, "%s: no child %ld to signal!\n",
		 blurb(), (long) pid);
      goto DONE;
    }

  switch (signal) {
  case SIGTERM:
    job->status = job_killed;
    job->killed = time ((time_t *) 0);
    break;
#ifdef SIGSTOP
  case SIGSTOP: job->status = job_stopped; break;
  case SIGCONT: job->status = job_running; break;
#endif /* SIGSTOP */
  default: abort();
  }

  if (p->verbose_p)
    fprintf (stderr, "%s: %d: %s pid %lu (%s)\n",
             blurb(), job->screen,
             (job->status == job_killed  ? "killing" :
              job->status == job_stopped ? "suspending" : "resuming"),
             (unsigned long) job->pid,
             job->name);

  status = kill (job->pid, signal);

  if (p->verbose_p && status < 0)
    {
      if (errno == ESRCH)
	fprintf (stderr,
                 "%s: %d: child process %lu (%s) was already dead\n",
		 blurb(), job->screen, (unsigned long) job->pid, job->name);
      else
	{
	  char buf [1024];
	  sprintf (buf, "%s: %d: couldn't kill child process %lu (%s)",
		   blurb(), job->screen, (unsigned long) job->pid, job->name);
	  perror (buf);
	}
    }

  await_dying_children (si);

 DONE:
  clean_job_list();
  return status;
}


/* We use Xt-style signal handling.  A Unix signal fires, and we inform Xt of
   that.  Then after we return to the top-level command loop on the main
   stack, Xt runs our callback function for that signal.  Just like Xt timers.
 */
static void
catch_signal (int sig, RETSIGTYPE (*handler) (int))
{
# ifdef HAVE_SIGACTION
  struct sigaction a;
  a.sa_handler = handler;
  sigemptyset (&a.sa_mask);
  a.sa_flags = 0;
  if (sigaction (sig, &a, 0) < 0)
# else  /* !HAVE_SIGACTION */
  if (((long) signal (sig, handler)) == -1L)
# endif /* !HAVE_SIGACTION */
    {
      char buf [255];
      sprintf (buf, "%s: couldn't catch signal %s", blurb(),
               signal_name (sig));
      perror (buf);
      abort();
    }
}


/* Exiting gracefully.

   When xscreensaver sends a SIGTERM signal to xscreensaver-gfx, rather
   than exiting immediately, we want it to do two things:

     - Send a SIGTERM to each running screenhack.  They would *probably*
       die of a BadWindow X error once their window was deleted, but this
       is cleaner and more immediate.

     - Fade the screens in from black.  This might take several seconds.

   Should another signal come in while that is ongoing, we should just
   die immediately.
 */

static RETSIGTYPE
saver_sigterm_handler (int sig)
{
  /* This is the actual signal handler, running on the signal stack.
     After firing once, set this signal back to the default behavior. */
  sigterm_received = sig;
  catch_signal (sig, SIG_DFL);

  /* The first time a signal fires, inform Xt of that so that it will run
     xt_signal_handler().  "XtNoticeSignal is the only Intrinsics function
     that can safely be called from a signal handler". */
  if (xt_sigterm_id)
    XtNoticeSignal (xt_sigterm_id);
}


static void
xt_sigterm_handler (XtPointer data, XtSignalId *id)
{
  /* This runs from the Xt event loop on the main stack, some time after
     the signal fired. */
  saver_info *si = (saver_info *) data;
  saver_preferences *p = &si->prefs;
  static Bool hit_p = False;
  int i;

  if (xt_sigterm_id)
    XtRemoveSignal (xt_sigterm_id);
  xt_sigterm_id = 0;

  if (hit_p)
    fprintf (stderr, "%s: second signal: %s: exiting\n", blurb(), 
             signal_name (sigterm_received));
  else
    {
      hit_p = True;
      if (p->verbose_p)
        fprintf (stderr, "%s: %s: unblanking\n", blurb(), 
                 signal_name (sigterm_received));

      /* Kill before unblanking, to stop drawing as soon as possible. */
      for (i = 0; i < si->nscreens; i++)
        {
          saver_screen_info *ssi = &si->screens[i];
          if (ssi->cycle_id)
            {
              XtRemoveTimeOut (ssi->cycle_id);
              ssi->cycle_id = 0;
            }
          kill_screenhack (ssi);
        }
      unblank_screen (si);

      if (p->verbose_p)
        fprintf (stderr, "%s: %s: exiting\n", blurb(), 
                 signal_name (sigterm_received));
    }

  /* Exit with the original signal received. */
  kill (getpid(), sigterm_received);
  abort();
}


/* SIGCHLD handling.  Basically the same deal as SIGTERM.
 */

static RETSIGTYPE
sigchld_handler (int sig)
{
  /* This is the actual signal handler, running on the signal stack.
     After firing once, set this signal to fire again. */
  sigchld_received = sig;
# ifndef HAVE_SIGACTION
  catch_signal (SIGCHLD, sigchld_handler);
# endif

  if (xt_sigchld_id)
    XtNoticeSignal (xt_sigchld_id);
}


static void
xt_sigchld_handler (XtPointer data, XtSignalId *id)
{
  /* This runs from the Xt event loop on the main stack, some time after
     the signal fired. */
  saver_info *si = (saver_info *) data;

  if (si->prefs.debug_p)
    fprintf(stderr, "%s: got SIGCHLD\n", blurb());

  await_dying_children (si);		/* Their first album was better */
}


static void
await_dying_children (saver_info *si)
{
  while (1)
    {
      int wait_status = 0;
      pid_t kid;
      struct rusage rus;

      errno = 0;
      kid = wait4 (-1, &wait_status, WNOHANG|WUNTRACED, &rus);

      if (si->prefs.debug_p)
	{
	  if (kid < 0 && errno)
            fprintf (stderr, "%s: waitpid(-1) ==> %ld (%d)\n", blurb(),
                     (long) kid, errno);
          else
            fprintf (stderr, "%s: waitpid(-1) ==> %ld\n", blurb(),
                     (long) kid);
	}

      /* 0 means no more children to reap.
	 -1 means error -- except "interrupted system call" isn't a "real"
	 error, so if we get that, we should just try again. */
      if (kid == 0 ||
	  (kid < 0 && errno != EINTR))
	break;

      describe_dead_child (si, kid, wait_status, rus);
    }
}


static void
describe_dead_child (saver_info *si, pid_t kid, int wait_status,
                     struct rusage rus)
{
  int i;
  saver_preferences *p = &si->prefs;
  struct screenhack_job *job = find_job (kid);
  const char *name = job ? job->name : "<unknown>";
  int screen_no = job ? job->screen : 0;
  char msg[1024];
  *msg = 0;

  if (WIFEXITED (wait_status))
    {
      int exit_status = WEXITSTATUS (wait_status);
      /* Treat exit code as a signed 8-bit quantity. */
      if (exit_status & 0x80) exit_status |= ~0xFF;

      sprintf (msg, _("crashed with status %d"), exit_status);
      if (p->verbose_p)
        fprintf (stderr,
                 "%s: %d: child pid %lu (%s) exited abnormally"
                 " with status %d\n",
                 blurb(), screen_no, (unsigned long) kid, name, exit_status);
      if (job)
	job->status = job_dead;
    }
  else if (WIFSIGNALED (wait_status))
    {
      const char *sig = signal_name (WTERMSIG (wait_status));
      if (job &&
	  job->status == job_killed &&
	  WTERMSIG (wait_status) == SIGTERM)
        {
          /* Expected notification after we killed it. */
          sprintf (msg, _("exited normally with %.100s"), sig);
          if (p->verbose_p)
            fprintf (stderr, "%s: %d: child pid %lu (%s)"
                     " exited normally with %s\n",
                     blurb(), screen_no, (unsigned long) kid, name, sig);
        }
      else
        {
          /* Unexpected signal. */
          sprintf (msg, _("crashed with %.100s"), sig);
          if (p->verbose_p)
            fprintf (stderr, "%s: %d: child pid %lu (%s)"
                     " unexpectedly terminated with %s\n",
                     blurb(), screen_no, (unsigned long) kid, name, sig);
        }
      if (job)
	job->status = job_dead;
    }
  else if (WIFSTOPPED (wait_status))
    {
      if (p->verbose_p)
        fprintf (stderr, "%s: child pid %lu (%s) stopped with %s\n",
                 blurb(), (unsigned long) kid, name,
                 signal_name (WSTOPSIG (wait_status)));
      if (job)
	job->status = job_stopped;
    }
  else
    {
      /* Didn't exit, signal or stop; is this even possible? */
      sprintf (msg, _("crashed mysteriously"));
      fprintf (stderr, "%s: child pid %lu (%s) died in a mysterious way!",
	       blurb(), (unsigned long) kid, name);
      if (job)
	job->status = job_dead;
    }

# ifdef LOG_CPU_TIME
  if (p->verbose_p && job && job->status == job_dead)
    {
      long u = rus.ru_utime.tv_usec / 1000 + rus.ru_utime.tv_sec * 1000;
      long s = rus.ru_stime.tv_usec / 1000 + rus.ru_stime.tv_sec * 1000;
      fprintf (stderr, "%s: %d: CPU used: %.1fu, %.1fs\n",
               blurb(), screen_no, u / 1000.0, s / 1000.0);
    }
# endif /* LOG_CPU_TIME */

  /* Clear out the pid so that any_screenhacks_running_p() knows it's dead.
   */
  if (!job || job->status == job_dead)
    {
      for (i = 0; i < si->nscreens; i++)
        {
          saver_screen_info *ssi = &si->screens[i];
          if (kid == ssi->pid)
            {
              ssi->pid = 0;
              if (*msg)
                screenhack_obituary (ssi, name, msg);
            }
        }
    }
}


void
init_sigchld (saver_info *si)
{
  static Bool signals_initialized_p = 0;
  if (signals_initialized_p) return;
  signals_initialized_p = True;

  catch_signal (SIGTERM, saver_sigterm_handler);	/* kill     */
  catch_signal (SIGINT,  saver_sigterm_handler);	/* shell ^C */
  catch_signal (SIGQUIT, saver_sigterm_handler);	/* shell ^| */
  catch_signal (SIGCHLD, sigchld_handler);

  xt_sigchld_id = XtAppAddSignal (si->app, xt_sigchld_handler, si);
  xt_sigterm_id = XtAppAddSignal (si->app, xt_sigterm_handler, si);
}


static void
hack_subproc_environment (Screen *screen, Window saver_window)
{
  /* Store $DISPLAY into the environment, so that the $DISPLAY variable that
     the spawned processes inherit is correct.  First, it must be on the same
     host and display as the value of -display passed in on our command line
     (which is not necessarily the same as what our $DISPLAY variable is.)
     Second, the screen number in the $DISPLAY passed to the subprocess should
     be the screen on which this particular hack is running -- not the display
     specification which the driver itself is using, since the driver ignores
     its screen number and manages all existing screens.

     Likewise, store a window ID in $XSCREENSAVER_WINDOW -- this is necessary
     in a Xinerama or RANDR world where a single X11 'Screen' spans multiple
     monitors, and we want to run a hack on each piece of glass, not spanning
     them.  In that case, multiple hacks have the same $DISPLAY, screen and
     root window.
   */
  Display *dpy = DisplayOfScreen (screen);
  const char *odpy = DisplayString (dpy);
  char *ndpy = (char *) malloc (strlen(odpy) + 20);
  char *nssw = (char *) malloc (40);
  char *s, *c;

  strcpy (ndpy, "DISPLAY=");
  s = ndpy + strlen(ndpy);
  strcpy (s, odpy);

  /* We have to find the last colon since it is the boundary between
     hostname & screen - IPv6 numeric format addresses may have many
     colons before that point, and DECnet addresses always have two colons */
  c = strrchr(s,':');				/* skip to last colon */
  if (c != NULL) s = c+1;
  while (isdigit(*s)) s++;			/* skip over dpy number */
  while (*s == '.') s++;			/* skip over dot */
  if (s[-1] != '.') *s++ = '.';			/* put on a dot */
  sprintf(s, "%d", screen_number (screen));	/* put on screen number */

  sprintf (nssw, "XSCREENSAVER_WINDOW=0x%lX", (unsigned long) saver_window);

  if (putenv (ndpy))
    abort ();
  if (putenv (nssw))
    abort ();

  /* don't free ndpy/nssw -- some implementations of putenv (BSD 4.4,
     glibc 2.0) copy the argument, but some (libc4,5, glibc 2.1.2)
     do not.  So we must leak it (and/or the previous setting). Yay.
   */
}


#ifdef ABORT_TESTER  /* Shoot down processes after a bit, for debugging */
static void
abort_debug_timer (XtPointer closure, XtIntervalId *id)
{
  saver_screen_info *ssi = (saver_screen_info *) closure;
  if (ssi->pid)
    {
      fprintf (stderr, "%s: %d: %ld: born to ill\n", blurb(), ssi->number,
               (unsigned long) ssi->pid);
      kill (ssi->pid, SIGILL);
    }
}
#endif /* ABORT_TESTER */


/* Executes the command in another process.
   Command may be any single command acceptable to /bin/sh.
   It may include wildcards, but no semicolons.
   If successful, the pid of the other process is returned.
   Otherwise, -1 is returned and an error may have been
   printed to stderr.
 */
static pid_t
fork_and_exec (saver_screen_info *ssi, const char *command)
{
  saver_info *si = ssi->global;
  saver_preferences *p = &si->prefs;
  pid_t forked;

  switch ((int) (forked = fork ()))
    {
    case -1:
      {
        char buf [255];
        sprintf (buf, "%s: couldn't fork", blurb());
        perror (buf);
        break;
      }

    case 0:
      close (ConnectionNumber (si->dpy));	/* close display fd */
      if (ssi)
        hack_subproc_environment (ssi->screen, ssi->screensaver_window);

      exec_command (p->shell, command, p->nice_inferior);
      /* If that returned, we were unable to exec the subprocess. */
      exit (1);  /* exits child fork */
      break;

    default:	/* parent */
      make_job (forked, (ssi ? ssi->number : 0), command);
      if (p->verbose_p)
        fprintf (stderr, "%s: %d: forked \"%s\" in pid %lu"
                 " on window 0x%lx\n",
                 blurb(), (ssi ? ssi->number : 0), command,
                 (unsigned long) forked,
                 (unsigned long) ssi->screensaver_window);
      break;
    }

# ifdef ABORT_TESTER
  if (forked)
    XtAppAddTimeOut (si->app, 1000 * (5 + (5 * ssi->number)),
                     abort_debug_timer, (XtPointer) ssi);
# endif

  return forked;
}


static Bool
select_visual_of_hack (saver_screen_info *ssi, screenhack *hack)
{
  saver_info *si = ssi->global;
  saver_preferences *p = &si->prefs;
  Bool selected;

  if (hack->visual && *hack->visual)
    selected = select_visual(ssi, hack->visual);
  else
    selected = select_visual(ssi, 0);

  if (!selected && (p->verbose_p || si->demoing_p))
    fprintf (stderr,
             (si->demoing_p
              ? "%s: warning, no \"%s\" visual for \"%s\"\n"
              : "%s: no \"%s\" visual; skipping \"%s\"\n"),
             blurb(),
             (hack->visual && *hack->visual ? hack->visual : "???"),
             hack->command);

  return selected;
}


void
spawn_screenhack (saver_screen_info *ssi)
{
  saver_info *si = ssi->global;
  saver_preferences *p = &si->prefs;
  XFlush (si->dpy);

  if (!monitor_powered_on_p (si->dpy))
    {
      if (si->prefs.verbose_p)
        fprintf (stderr,
                 "%s: %d: X says monitor has powered down; "
                 "not launching a hack\n", blurb(), ssi->number);
      return;
    }

  if (p->screenhacks_count)
    {
      screenhack *hack;
      pid_t forked;
      char buf [255];
      int new_hack = -1;
      int retry_count = 0;
      Bool force = False;

    AGAIN:

      if (p->screenhacks_count < 1)
        {
          /* No hacks at all */
          new_hack = -1;
        }
      else if (p->screenhacks_count == 1)
        {
          /* Exactly one hack in the list */
          new_hack = 0;
        }
      else if (si->selection_mode == -1)
        {
          /* Select the next hack, wrapping. */
          new_hack = (ssi->current_hack + 1) % p->screenhacks_count;
        }
      else if (si->selection_mode == -2)
        {
          /* Select the previous hack, wrapping. */
          if (ssi->current_hack < 0)
            new_hack = p->screenhacks_count - 1;
          else
            new_hack = ((ssi->current_hack + p->screenhacks_count - 1)
                        % p->screenhacks_count);
        }
      else if (si->selection_mode > 0)
	{
          /* Select a specific hack, by number (via the ACTIVATE command.) */
	  new_hack = ((si->selection_mode - 1) % p->screenhacks_count);
	  force = True;
	}
      else if (p->mode == ONE_HACK &&
               p->selected_hack >= 0)
	{
          /* Select a specific hack, by number (via "One Saver" mode.) */
          new_hack = p->selected_hack;
	  force = True;
	}
      else if (p->mode == BLANK_ONLY || p->mode == DONT_BLANK)
        {
          new_hack = -1;
        }
      else if (p->mode == RANDOM_HACKS_SAME &&
               ssi->number != 0)
	{
	  /* Use the same hack that's running on screen 0.
             (Assumes this function was called on screen 0 first.)
           */
          new_hack = si->screens[0].current_hack;
	}
      else  /* (p->mode == RANDOM_HACKS) */
	{
	  /* Select a random hack (but not the one we just ran.) */
	  while ((new_hack = random () % p->screenhacks_count)
		 == ssi->current_hack)
	    ;
	}

      if (new_hack < 0)   /* don't run a hack */
        {
          ssi->current_hack = -1;
          goto DONE;
        }

      ssi->current_hack = new_hack;
      hack = p->screenhacks[ssi->current_hack];

      /* If the hack is disabled, or there is no visual for this hack,
	 then try again (move forward, or backward, or re-randomize.)
	 Unless this hack was specified explicitly, in which case,
	 use it regardless.
       */
      if (force)
        select_visual_of_hack (ssi, hack);
        
      if (!force &&
	  (!hack->enabled_p ||
	   !on_path_p (hack->command) ||
	   !select_visual_of_hack (ssi, hack)))
	{
	  if (++retry_count > (p->screenhacks_count*4))
	    {
	      /* Uh, oops.  Odds are, there are no suitable visuals,
		 and we're looping.  Give up.  (This is totally lame,
		 what we should do is make a list of suitable hacks at
		 the beginning, then only loop over them.)
	      */
	      if (p->verbose_p)
		fprintf(stderr,
		      "%s: %d: no programs enabled, or no suitable visuals\n",
			blurb(), ssi->number);
	      return;
	    }
	  else
	    goto AGAIN;
	}

      forked = fork_and_exec (ssi, hack->command);
      switch ((int) forked)
	{
	case -1: /* fork failed */
	case 0:  /* child fork (can't happen) */
	  sprintf (buf, "%s: couldn't fork", blurb());
	  perror (buf);
          exit (1);
	  break;

	default:
	  ssi->pid = forked;
	  break;
	}

      XChangeProperty (si->dpy, ssi->screensaver_window, XA_WM_COMMAND,
                       XA_STRING, 8, PropModeReplace,
                       (unsigned char *) hack->command,
                       strlen (hack->command));
    }

 DONE:

  if (ssi->current_hack < 0)
    XDeleteProperty (si->dpy, ssi->screensaver_window, XA_WM_COMMAND);

  store_saver_status (si);  /* store current hack numbers */

  /* Now that the hack has launched, queue a timer to cycle it. */
  if (!si->demoing_p && p->cycle)
    {
      Time how_long = p->cycle;

      /* If we're in "SELECT n" mode, the cycle timer going off will just
         restart this same hack again.  There's not much point in doing this
         every 5 or 10 minutes, but on the other hand, leaving one hack
         running for days is probably not a great idea, since they tend to
         leak and/or crash.  So, restart the thing once an hour.
       */
      if (si->selection_mode > 0 && ssi->pid)
        how_long = 1000 * 60 * 60;

      /* If there are multiple screens, stagger the restart time of subsequent
         screens: they will all change every N minutes, but not at the same
         time.  But don't let that offset be more than about 5 minutes.
       */
      if (ssi->cycle_count == 0 && si->nscreens > 1)
        {
          Time max = 1000 * 60 * 60 * 10;
          int t2 = (how_long > max ? max : how_long);
          int off = t2 * ssi->number / si->nscreens;
          how_long += off;
          if (off && p->verbose_p)
            fprintf (stderr, "%s: %d: offsetting cycle time by %d sec\n",
                     blurb(), ssi->number, off / 1000);
        }

      if (p->debug_p)
        fprintf (stderr, "%s: %d: starting cycle_timer (%ld)\n",
                 blurb(), ssi->number, how_long);

      if (ssi->cycle_id)
        XtRemoveTimeOut (ssi->cycle_id);
      ssi->cycle_id =
        XtAppAddTimeOut (si->app, how_long, cycle_timer, (XtPointer) ssi);

      if (p->verbose_p)
        {
          time_t t = time((time_t *) 0) + how_long/1000;
          fprintf (stderr, "%s: %d: next cycle in %lu sec at %s\n",
                   blurb(), ssi->number, how_long/1000, timestring(t));
        }
    }

  ssi->cycle_count++;
}


void
kill_screenhack (saver_screen_info *ssi)
{
  saver_info *si = ssi->global;
  if (ssi->pid)
    kill_job (si, ssi->pid, SIGTERM);
  ssi->pid = 0;
}


Bool
any_screenhacks_running_p (saver_info *si)
{
  Bool any_running_p = False;
  int i;
  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      if (ssi->pid) any_running_p = True;
      /* Consider it running if an error dialog is posted, so that we
         don't prematurely clear the window. */
      if (ssi->error_dialog) any_running_p = True;
    }
  return any_running_p;
}


/* Fork "xscreensaver-gl-visual" and wait for it to print the IDs of
   the GL visual that should be used on this screen.
 */
Visual *
get_best_gl_visual (saver_info *si, Screen *screen)
{
  saver_preferences *p = &si->prefs;
  pid_t forked;
  int fds [2];
  int in, out;
  int errfds[2];
  int errin = -1, errout = -1;
  char buf[1024];

  char *av[10];
  int ac = 0;

  av[ac++] = "xscreensaver-gl-visual";
  av[ac] = 0;

  if (pipe (fds))
    {
      perror ("error creating pipe:");
      return 0;
    }

  in = fds [0];
  out = fds [1];

  if (!si->prefs.verbose_p)
    {
      if (pipe (errfds))
        {
          perror ("error creating pipe:");
          return 0;
        }

      errin = errfds [0];
      errout = errfds [1];
    }

  switch ((int) (forked = fork ()))
    {
    case -1:
      {
        sprintf (buf, "%s: couldn't fork", blurb());
        perror (buf);
        exit (1);
      }
    case 0:
      {
        close (in);  /* don't need this one */
        close (ConnectionNumber (si->dpy));	/* close display fd */

        if (dup2 (out, STDOUT_FILENO) < 0)	/* pipe stdout */
          {
            perror ("could not dup() a new stdout:");
            return 0;
          }

        if (! si->prefs.verbose_p)
          {
            close(errin);
            if (dup2 (errout, STDERR_FILENO) < 0)
              {
                perror ("could not dup() a new stderr:");
                return 0;
              }
          }

        hack_subproc_environment (screen, 0);	/* set $DISPLAY */

        execvp (av[0], av);			/* shouldn't return. */

        if (errno != ENOENT /* || si->prefs.verbose_p */ )
          {
            /* Ignore "no such file or directory" errors.
               Issue all other exec errors, though. */
            sprintf (buf, "%s: running %s", blurb(), av[0]);
            perror (buf);
          }
        exit (1);                               /* exits fork */
        break;
      }
    default:
      {
        int result = 0;
        int wait_status = 0;
        pid_t pid = -1;
        FILE *f;
        unsigned long v = 0;
        char c;

        make_job (forked, 0, av[0]);  /* Bookkeeping for SIGCHLD */

        if (p->verbose_p)
          fprintf (stderr, "%s: %d: forked \"%s\" in pid %lu\n",
                   blurb(), 0, av[0], (unsigned long) forked);

        f = fdopen (in, "r");
        close (out);  /* don't need this one */

        *buf = 0;
        if (! fgets (buf, sizeof(buf)-1, f))
          *buf = 0;
        fclose (f);

        if (! si->prefs.verbose_p)
          {
            close (errout);
            close (errin);
          }

        /* Wait for the child to die - wait for this pid only, not others. */
        pid = waitpid (forked, &wait_status, 0);
        if (si->prefs.debug_p)
          fprintf (stderr, "%s: waitpid(%ld) => %ld\n", blurb(),
                   (long) forked, (long) pid);

        if (1 == sscanf (buf, "0x%lx %c", &v, &c))
          result = (int) v;

        if (result == 0)
          {
            if (si->prefs.verbose_p)
              {
                int L = strlen(buf);
                fprintf (stderr, "%s: %s did not report a GL visual!\n",
                         blurb(), av[0]);

                if (L && buf[L-1] == '\n')
                  buf[--L] = 0;
                if (*buf)
                  fprintf (stderr, "%s: %s said: \"%s\"\n",
                           blurb(), av[0], buf);
              }
            return 0;
          }
        else
          {
            Visual *v = id_to_visual (screen, result);
            if (si->prefs.verbose_p)
              fprintf (stderr, "%s: %d: %s: GL visual is 0x%X%s\n",
                       blurb(), screen_number (screen),
                       av[0], result,
                       (v == DefaultVisualOfScreen (screen)
                        ? " (default)" : ""));
            return v;
          }
      }
    }

  abort();
}
