/* subprocs.c --- choosing, spawning, and killing screenhacks.
 * xscreensaver, Copyright (c) 1991, 1992, 1993, 1995, 1997, 1998
 *  Jamie Zawinski <jwz@netscape.com>
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

#ifndef ESRCH
#include <errno.h>
#endif

#include <sys/time.h>		/* sys/resource.h needs this for timeval */

#ifndef VMS

# include <sys/resource.h>	/* for setpriority() and PRIO_PROCESS */
# include <sys/wait.h>		/* for waitpid() and associated macros */

#else  /* VMS */

# if __DECC_VER >= 50200000
#  include <sys/wait.h>
# endif

# include <processes.h>
# include <unixio.h>		/* for close */
# include <unixlib.h>		/* for getpid */
# define pid_t    int
# define fork     vfork

#endif /* VMS */

#include <signal.h>		/* for the signal names */

#ifndef NO_SETUID
#include <pwd.h>		/* for getpwnam() and struct passwd */
#include <grp.h>		/* for getgrgid() and struct group */
#endif /* NO_SETUID */

#if !defined(SIGCHLD) && defined(SIGCLD)
#define SIGCHLD SIGCLD
#endif

#ifdef HAVE_PUTENV
extern int putenv (/* const char * */);	/* getenv() is in stdlib.h... */
#endif
extern int kill (pid_t, int);		/* signal() is in sys/signal.h... */

/* This file doesn't need the Xt headers, so stub these types out... */
#undef XtPointer
#define XtAppContext void*
#define XrmDatabase  void*
#define XtIntervalId void*
#define XtPointer    void*
#define Widget       void*

#include "xscreensaver.h"
#include "yarandom.h"


extern saver_info *global_si_kludge;	/* I hate C so much... */

static void hack_environment (saver_screen_info *ssi);


static void
nice_subproc (int nice_level)
{
  if (nice_level == 0)
    return;

#if defined(HAVE_NICE)
  {
    int old_nice = nice (0);
    int n = nice_level - old_nice;
    errno = 0;
    if (nice (n) == -1 && errno != 0)
      {
	char buf [512];
	sprintf (buf, "%s: nice(%d) failed", progname, n);
	perror (buf);
    }
  }
#elif defined(HAVE_SETPRIORITY) && defined(PRIO_PROCESS)
  if (setpriority (PRIO_PROCESS, getpid(), nice_level) != 0)
    {
      char buf [512];
      sprintf (buf, "%s: setpriority(PRIO_PROCESS, %lu, %d) failed",
	       progname, (unsigned long) getpid(), nice_level);
      perror (buf);
    }
#else
  fprintf (stderr,
	   "%s: don't know how to change process priority on this system.\n",
	   progname);

#endif
}


#ifndef VMS

static void
exec_simple_command (const char *command)
{
  char *av[1024];
  int ac = 0;
  char *token = strtok (strdup(command), " \t");
  while (token)
    {
      av[ac++] = token;
      token = strtok(0, " \t");
    }
  av[ac] = 0;

  execvp (av[0], av);			/* shouldn't return. */

  {
    char buf [512];
    sprintf (buf, "%s: could not execute \"%s\"", progname, av[0]);
    perror (buf);

    if (errno == ENOENT &&
	(token = getenv("PATH")))
      {
# ifndef PATH_MAX
#  ifdef MAXPATHLEN
#   define PATH_MAX MAXPATHLEN
#  else
#   define PATH_MAX 2048
#  endif
# endif
	char path[PATH_MAX];
	fprintf (stderr, "\n");
	*path = 0;
# if defined(HAVE_GETCWD)
	getcwd (path, sizeof(path));
# elif defined(HAVE_GETWD)
	getwd (path);
# endif
	if (*path)
	  fprintf (stderr, "    Current directory is: %s\n", path);
	fprintf (stderr, "    PATH is:\n");
	token = strtok (strdup(token), ":");
	while (token)
	  {
	    fprintf (stderr, "        %s\n", token);
	    token = strtok(0, ":");
	  }
	fprintf (stderr, "\n");
      }
  }
  fflush(stderr);
  fflush(stdout);
  exit (1);	/* Note that this only exits a child fork.  */
}


static void
exec_complex_command (const char *shell, const char *command)
{
  char *av[5];
  int ac = 0;
  char *command2 = (char *) malloc (strlen (command) + 6);
  memcpy (command2, "exec ", 5);
  memcpy (command2 + 5, command, strlen (command) + 1);

  /* Invoke the shell as "/bin/sh -c 'exec prog -arg -arg ...'" */
  av [ac++] = (char *) shell;
  av [ac++] = "-c";
  av [ac++] = command2;
  av [ac]   = 0;

  execvp (av[0], av);			/* shouldn't return. */

  {
    char buf [512];
    sprintf (buf, "%s: execvp(\"%s\") failed", progname, av[0]);
    perror (buf);
    fflush(stderr);
    fflush(stdout);
    exit (1);	/* Note that this only exits a child fork.  */
  }
}

#else  /* VMS */

static void
exec_vms_command (const char *command)
{
  system (command);
  fflush (stderr);
  fflush (stdout);
  exit (1);	/* Note that this only exits a child fork.  */
}

#endif /* !VMS */


static void
exec_screenhack (saver_info *si, const char *command)
{
  /* I don't believe what a sorry excuse for an operating system UNIX is!

     - I want to spawn a process.
     - I want to know it's pid so that I can kill it.
     - I would like to receive a message when it dies of natural causes.
     - I want the spawned process to have user-specified arguments.

     If shell metacharacters are present (wildcards, backquotes, etc), the
     only way to parse those arguments is to run a shell to do the parsing
     for you.

     And the only way to know the pid of the process is to fork() and exec()
     it in the spawned side of the fork.

     But if you're running a shell to parse your arguments, this gives you
     the pid of the *shell*, not the pid of the *process* that you're
     actually interested in, which is an *inferior* of the shell.  This also
     means that the SIGCHLD you get applies to the shell, not its inferior.
     (Why isn't that sufficient?  I don't remember any more, but it turns
     out that it isn't.)

     So, the only solution, when metacharacters are present, is to force the
     shell to exec() its inferior.  What a fucking hack!  We prepend "exec "
     to the command string, and hope it doesn't contain unquoted semicolons
     or ampersands (we don't search for them, because we don't want to
     prohibit their use in quoted strings (messages, for example) and parsing
     out the various quote characters is too much of a pain.)

     (Actually, Clint Wong <clint@jts.com> points out that process groups
     might be used to take care of this problem; this may be worth considering
     some day, except that, 1: this code works now, so why fix it, and 2: from
     what I've seen in Emacs, dealing with process groups isn't especially
     portable.)
   */
  saver_preferences *p = &si->prefs;

#ifndef VMS
  Bool hairy_p = !!strpbrk (command, "*?$&!<>[];`'\\\"");

  if (p->verbose_p)
    printf ("%s: spawning \"%s\" in pid %lu%s.\n",
	    progname, command, (unsigned long) getpid (),
	    (hairy_p ? " (via shell)" : ""));

  if (hairy_p)
    /* If it contains any shell metacharacters, do it the hard way,
       and fork a shell to parse the arguments for us. */
    exec_complex_command (p->shell, command);
  else
    /* Otherwise, we can just exec the program directly. */
    exec_simple_command (command);

#else /* VMS */
  if (p->verbose_p)
    printf ("%s: spawning \"%s\" in pid %lu.\n", progname, command, getpid());
  exec_vms_command (command);
#endif /* VMS */

  abort();	/* that shouldn't have returned. */
}



/* Management of child processes, and de-zombification.
 */

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
  enum job_status status;
  struct screenhack_job *next;
};

static struct screenhack_job *jobs = 0;

/* for debugging -- nothing calls this, but it's useful to invoke from gdb. */
void
show_job_list (void)
{
  struct screenhack_job *job;
  fprintf(stderr, "%s: job list:\n", progname);
  for (job = jobs; job; job = job->next)
    fprintf (stderr, "  %5ld: (%s) %s\n",
	     (long) job->pid,
	     (job->status == job_running ? "running" :
	      job->status == job_stopped ? "stopped" :
	      job->status == job_killed  ? " killed" :
	      job->status == job_dead    ? "   dead" : "    ???"),
	     job->name);
  fprintf (stderr, "\n");
}


static void clean_job_list (void);

static struct screenhack_job *
make_job (pid_t pid, const char *cmd)
{
  struct screenhack_job *job = (struct screenhack_job *) malloc (sizeof(*job));

  static char name [1024];
  const char *in = cmd;
  char *out = name;

  clean_job_list();

  while (isspace(*in)) in++;		/* skip whitespace */
  while (!isspace(*in) && *in != ':')
    *out++ = *in++;			/* snarf first token */
  while (isspace(*in)) in++;		/* skip whitespace */
  if (*in == ':')			/* token was a visual name; skip it. */
    {
      in++;
      out = name;
      while (isspace(*in)) in++;		/* skip whitespace */
      while (!isspace(*in)) *out++ = *in++;	/* snarf first token */
    }
  *out = 0;

  job->name = strdup(name);
  job->pid = pid;
  job->status = job_running;
  job->next = jobs;
  jobs = job;

  return jobs;
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


/* Cleans out dead jobs from the jobs list -- this must only be called
   from the main thread, not from a signal handler. 
 */
static void
clean_job_list (void)
{
  struct screenhack_job *job, *prev, *next;
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
    }
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

static void await_dying_children (saver_info *si);
#ifndef VMS
static void describe_dead_child (saver_info *, pid_t, int wait_status);
#endif


/* Semaphore to temporarily turn the SIGCHLD handler into a no-op.
   Don't alter this directly -- use block_sigchld() / unblock_sigchld().
 */
static int block_sigchld_handler = 0;


static void
block_sigchld (void)
{
#ifdef HAVE_SIGACTION
  sigset_t child_set;
  sigemptyset (&child_set);
  sigaddset (&child_set, SIGCHLD);
  sigprocmask (SIG_BLOCK, &child_set, 0);
#endif /* HAVE_SIGACTION */

  block_sigchld_handler++;
}

static void
unblock_sigchld (void)
{
#ifdef HAVE_SIGACTION
  sigset_t child_set;
  sigemptyset(&child_set);
  sigaddset(&child_set, SIGCHLD);
  sigprocmask(SIG_UNBLOCK, &child_set, 0);
#endif /* HAVE_SIGACTION */

  block_sigchld_handler--;
}

static int
kill_job (saver_info *si, pid_t pid, int signal)
{
  saver_preferences *p = &si->prefs;
  struct screenhack_job *job;
  int status = -1;

  clean_job_list();

  if (block_sigchld_handler)
    /* This function should not be called from the signal handler. */
    abort();

  block_sigchld();			/* we control the horizontal... */

  job = find_job (pid);
  if (!job ||
      !job->pid ||
      job->status == job_killed)
    {
      if (p->verbose_p)
	fprintf (stderr, "%s: no child %ld to signal!\n",
		 progname, (long) pid);
      goto DONE;
    }

  switch (signal) {
  case SIGTERM: job->status = job_killed;  break;
#ifdef SIGSTOP
    /* #### there must be a way to do this on VMS... */
  case SIGSTOP: job->status = job_stopped; break;
  case SIGCONT: job->status = job_running; break;
#endif /* SIGSTOP */
  default: abort();
  }

#ifdef SIGSTOP
  if (p->verbose_p)
    fprintf (stderr, "%s: %s pid %lu.\n", progname,
	     (signal == SIGTERM ? "killing" :
	      signal == SIGSTOP ? "suspending" :
	      signal == SIGCONT ? "resuming" : "signalling"),
	     (unsigned long) job->pid);
#else  /* !SIGSTOP */
  if (p->verbose_p)
    fprintf (stderr, "%s: %s pid %lu.\n", progname, "killing",
	     (unsigned long) job->pid);
#endif /* !SIGSTOP */

  status = kill (job->pid, signal);

  if (p->verbose_p && status < 0)
    {
      if (errno == ESRCH)
	fprintf (stderr, "%s: child process %lu (%s) was already dead.\n",
		 progname, job->pid, job->name);
      else
	{
	  char buf [1024];
	  sprintf (buf, "%s: couldn't kill child process %lu (%s)",
		   progname, job->pid, job->name);
	  perror (buf);
	}
    }

  await_dying_children (si);

 DONE:
  unblock_sigchld();
  if (block_sigchld_handler < 0)
    abort();

  clean_job_list();
  return status;
}


#ifdef SIGCHLD
static RETSIGTYPE
sigchld_handler (int sig)
{
  saver_info *si = global_si_kludge;	/* I hate C so much... */

  if (si->prefs.debug_p)
    fprintf(stderr, "%s: got SIGCHLD%s\n", progname,
	    (block_sigchld_handler ? " (blocked)" : ""));

  if (block_sigchld_handler < 0)
    abort();
  else if (block_sigchld_handler == 0)
    {
      block_sigchld();
      await_dying_children (si);
      unblock_sigchld();
    }

  init_sigchld();
}
#endif /* SIGCHLD */


#ifndef VMS
static void
await_dying_children (saver_info *si)
{
  while (1)
    {
      int wait_status = 0;
      pid_t kid;

      errno = 0;
      kid = waitpid (-1, &wait_status, WNOHANG|WUNTRACED);

      if (si->prefs.debug_p)
	{
	  if (kid < 0 && errno)
	    fprintf (stderr, "%s: waitpid(-1) ==> %ld (%d)\n", progname,
		     (long) kid, errno);
	  else
	    fprintf (stderr, "%s: waitpid(-1) ==> %ld\n", progname,
		     (long) kid);
	}

      /* 0 means no more children to reap.
	 -1 means error -- except "interrupted system call" isn't a "real"
	 error, so if we get that, we should just try again. */
      if (kid == 0 ||
	  (kid < 0 && errno != EINTR))
	break;

      describe_dead_child (si, kid, wait_status);
    }
}


static void
describe_dead_child (saver_info *si, pid_t kid, int wait_status)
{
  int i;
  saver_preferences *p = &si->prefs;
  struct screenhack_job *job = find_job (kid);
  const char *name = job ? job->name : "<unknown>";

  if (WIFEXITED (wait_status))
    {
      int exit_status = WEXITSTATUS (wait_status);

      /* Treat exit code as a signed 8-bit quantity. */
      if (exit_status & 0x80) exit_status |= ~0xFF;

      /* One might assume that exiting with non-0 means something went wrong.
	 But that loser xswarm exits with the code that it was killed with, so
	 it *always* exits abnormally.  Treat abnormal exits as "normal" (don't
	 mention them) if we've just killed the subprocess.  But mention them
	 if they happen on their own.
       */
      if (!job ||
	  (exit_status != 0 &&
	   (p->verbose_p || job->status != job_killed)))
	fprintf (stderr,
		 "%s: child pid %lu (%s) exited abnormally (code %d).\n",
		 progname, (unsigned long) kid, name, exit_status);
      else if (p->verbose_p)
	printf ("%s: child pid %lu (%s) exited normally.\n",
		progname, (unsigned long) kid, name);

      if (job)
	job->status = job_dead;
    }
  else if (WIFSIGNALED (wait_status))
    {
      if (p->verbose_p ||
	  !job ||
	  job->status != job_killed ||
	  WTERMSIG (wait_status) != SIGTERM)
	fprintf (stderr, "%s: child pid %lu (%s) terminated with %s.\n",
		 progname, (unsigned long) kid, name,
		 signal_name (WTERMSIG(wait_status)));

      if (job)
	job->status = job_dead;
    }
  else if (WIFSTOPPED (wait_status))
    {
      if (p->verbose_p)
	fprintf (stderr, "%s: child pid %lu (%s) stopped with %s.\n",
		 progname, (unsigned long) kid, name,
		 signal_name (WSTOPSIG (wait_status)));

      if (job)
	job->status = job_stopped;
    }
  else
    {
      fprintf (stderr, "%s: child pid %lu (%s) died in a mysterious way!",
	       progname, (unsigned long) kid, name);
      if (job)
	job->status = job_dead;
    }

  /* Clear out the pid so that screenhack_running_p() knows it's dead.
   */
  if (!job || job->status == job_dead)
    for (i = 0; i < si->nscreens; i++)
      {
	saver_screen_info *ssi = &si->screens[i];
	if (kid == ssi->pid)
	  ssi->pid = 0;
      }
}

#else  /* VMS */
static void await_dying_children (saver_info *si) { return; }
#endif /* VMS */


void
init_sigchld (void)
{
#ifdef SIGCHLD

# ifdef HAVE_SIGACTION	/* Thanks to Tom Kelly <tom@ancilla.toronto.on.ca> */

  static Bool sigchld_initialized_p = 0;
  if (!sigchld_initialized_p)
    {
      struct sigaction action, old;

      action.sa_handler = sigchld_handler;
      sigemptyset(&action.sa_mask);
      action.sa_flags = 0;

      if (sigaction(SIGCHLD, &action, &old) < 0)
	{
	  char buf [255];
	  sprintf (buf, "%s: couldn't catch SIGCHLD", progname);
	  perror (buf);
	}
      sigchld_initialized_p = True;
    }

# else  /* !HAVE_SIGACTION */

  if (((long) signal (SIGCHLD, sigchld_handler)) == -1L)
    {
      char buf [255];
      sprintf (buf, "%s: couldn't catch SIGCHLD", progname);
      perror (buf);
    }
# endif /* !HAVE_SIGACTION */
#endif /* SIGCHLD */
}





static Bool
select_visual_of_hack (saver_screen_info *ssi, const char *hack)
{
  saver_info *si = ssi->global;
  saver_preferences *p = &si->prefs;
  Bool selected;
  static char vis [1024];
  const char *in = hack;
  char *out = vis;
  while (isspace(*in)) in++;		/* skip whitespace */
  while (!isspace(*in) && *in != ':')
    *out++ = *in++;			/* snarf first token */
  while (isspace(*in)) in++;		/* skip whitespace */
  *out = 0;

  if (*in == ':')
    selected = select_visual(ssi, vis);
  else
    selected = select_visual(ssi, 0);

  if (!selected && (p->verbose_p || si->demo_mode_p))
    {
      if (*in == ':') in++;
      while (isspace(*in)) in++;
      fprintf (stderr,
	       (si->demo_mode_p
		? "%s: warning, no \"%s\" visual for \"%s\".\n"
		: "%s: no \"%s\" visual; skipping \"%s\".\n"),
	       progname, (vis ? vis : "???"), in);
    }

  return selected;
}


static void
spawn_screenhack_1 (saver_screen_info *ssi, Bool first_time_p)
{
  saver_info *si = ssi->global;
  saver_preferences *p = &si->prefs;
  raise_window (si, first_time_p, True, False);
  XFlush (si->dpy);

  if (p->screenhacks_count || si->demo_mode_p)
    {
      char *hack;
      pid_t forked;
      char buf [255];
      int new_hack;

      if (si->demo_mode_p)
	{
	  hack = si->demo_hack;

	  /* Ignore visual-selection failure if in demo mode. */
	  (void) select_visual_of_hack (ssi, hack);
	}
      else
	{
	  int retry_count = 0;

	AGAIN:
	  if (p->screenhacks_count == 1)
	    new_hack = 0;
	  else if (si->next_mode_p == 1)
	    new_hack = (ssi->current_hack + 1) % p->screenhacks_count;
	  else if (si->next_mode_p == 2)
	    new_hack = ((ssi->current_hack + p->screenhacks_count - 1)
			% p->screenhacks_count);
	  else
	    while ((new_hack = random () % p->screenhacks_count)
		   == ssi->current_hack)
	      ;
	  ssi->current_hack = new_hack;
	  hack = p->screenhacks[ssi->current_hack];

	  if (!select_visual_of_hack (ssi, hack))
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
			    "%s: no suitable visuals for these programs.\n",
			    progname);
		  return;
		}
	      else
		goto AGAIN;
	    }
	}
      si->next_mode_p = 0;


      /* If there's a visual description on the front of the command, nuke it.
       */
      {
	char *in = hack;
	while (isspace(*in)) in++;			/* skip whitespace */
	hack = in;
	while (!isspace(*in) && *in != ':') in++;	/* snarf first token */
	while (isspace(*in)) in++;			/* skip whitespace */
	if (*in == ':')
	  {
	    in++;
	    while (isspace(*in)) in++;
	    hack = in;
	  }
      }

      switch ((int) (forked = fork ()))
	{
	case -1:
	  sprintf (buf, "%s: couldn't fork", progname);
	  perror (buf);
	  restore_real_vroot (si);
	  saver_exit (si, 1);

	case 0:
	  close (ConnectionNumber (si->dpy));	/* close display fd */
	  nice_subproc (p->nice_inferior);	/* change process priority */
	  hack_environment (ssi);		/* set $DISPLAY */
	  exec_screenhack (si, hack);		/* this does not return */
	  abort();
	  break;

	default:
	  ssi->pid = forked;
	  (void) make_job (forked, hack);
	  break;
	}
    }
}


void
spawn_screenhack (saver_info *si, Bool first_time_p)
{
  int i;
  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      spawn_screenhack_1 (ssi, first_time_p);
    }
}


void
kill_screenhack (saver_info *si)
{
  int i;
  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      if (ssi->pid)
	kill_job (si, ssi->pid, SIGTERM);
      ssi->pid = 0;
    }
}


void
suspend_screenhack (saver_info *si, Bool suspend_p)
{
#ifdef SIGSTOP	/* older VMS doesn't have it... */
  int i;
  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      if (ssi->pid)
	kill_job (si, ssi->pid, (suspend_p ? SIGSTOP : SIGCONT));
    }
#endif /* SIGSTOP */
}


/* Called when we're exiting abnormally, to kill off the subproc. */
void
emergency_kill_subproc (saver_info *si)
{
  int i;
#ifdef SIGCHLD
  signal (SIGCHLD, SIG_IGN);
#endif /* SIGCHLD */

  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      if (ssi->pid)
	{
	  kill_job (si, ssi->pid, SIGTERM);
	  ssi->pid = 0;
	}
    }
}

Bool
screenhack_running_p (saver_info *si)
{
  Bool result = True;
  int i;
  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      if (!ssi->pid)
	result = False;
    }
  return result;
}


/* Restarting the xscreensaver process from scratch. */

static char **saved_argv;

void
save_argv (int argc, char **argv)
{
  saved_argv = (char **) malloc ((argc + 2) * sizeof (char *));
  saved_argv [argc] = 0;
  while (argc--)
    {
      int i = strlen (argv [argc]) + 1;
      saved_argv [argc] = (char *) malloc (i);
      memcpy (saved_argv [argc], argv [argc], i);
    }
}

void
restart_process (saver_info *si)
{
  fflush (real_stdout);
  fflush (real_stderr);
  execvp (saved_argv [0], saved_argv);	/* shouldn't return */
  {
    char buf [512];
    sprintf (buf, "%s: could not restart process", progname);
    perror(buf);
    fflush(stderr);
  }
}

/* Like restart_process(), but ensures that when it restarts,
   it comes up in demo-mode. */
void
demo_mode_restart_process (saver_info *si)
{
  int i;
  for (i = 0; saved_argv [i]; i++);
  /* add the -initial-demo-mode switch; save_argv() left room for this. */
  saved_argv [i] = "-initial-demo-mode";
  saved_argv [i+1] = 0;
  restart_process (si);		/* shouldn't return */
  saved_argv [i] = 0;
  XBell(si->dpy, 0);
}

static void
hack_environment (saver_screen_info *ssi)
{
  /* Store $DISPLAY into the environment, so that the $DISPLAY variable that
     the spawned processes inherit is correct.  First, it must be on the same
     host and display as the value of -display passed in on our command line
     (which is not necessarily the same as what our $DISPLAY variable is.)
     Second, the screen number in the $DISPLAY passed to the subprocess should
     be the screen on which this particular hack is running -- not the display
     specification which the driver itself is using, since the driver ignores
     its screen number and manages all existing screens.
   */
  saver_info *si = ssi->global;
  const char *odpy = DisplayString (si->dpy);
  char *ndpy = (char *) malloc(strlen(odpy) + 20);
  int screen_number;
  char *s;

  for (screen_number = 0; screen_number < si->nscreens; screen_number++)
    if (ssi == &si->screens[screen_number])
      break;
  if (screen_number >= si->nscreens) abort();

  strcpy (ndpy, "DISPLAY=");
  s = ndpy + strlen(ndpy);
  strcpy (s, odpy);

  while (*s && *s != ':') s++;			/* skip to colon */
  while (*s == ':') s++;			/* skip over colons */
  while (isdigit(*s)) s++;			/* skip over dpy number */
  while (*s == '.') s++;			/* skip over dot */
  if (s[-1] != '.') *s++ = '.';			/* put on a dot */
  sprintf(s, "%d", screen_number);		/* put on screen number */

  /* Allegedly, BSD 4.3 didn't have putenv(), but nobody runs such systems
     any more, right?  It's not Posix, but everyone seems to have it. */
#ifdef HAVE_PUTENV
  if (putenv (ndpy))
    abort ();
#endif /* HAVE_PUTENV */
}


/* Change the uid/gid of the screensaver process, so that it is safe for it
   to run setuid root (which it needs to do on some systems to read the 
   encrypted passwords from the passwd file.)

   hack_uid() is run before opening the X connection, so that XAuth works.
   hack_uid_warn() is called after the connection is opened and the command
   line arguments are parsed, so that the messages from hack_uid() get 
   printed after we know whether we're in `verbose' mode.
 */

#ifndef NO_SETUID

static int hack_uid_errno;
static char hack_uid_buf [255], *hack_uid_error;

void
hack_uid (saver_info *si)
{

  /* If we've been run as setuid or setgid to someone else (most likely root)
     turn off the extra permissions so that random user-specified programs
     don't get special privileges.  (On some systems it might be necessary
     to install this as setuid root in order to read the passwd file to
     implement lock-mode...)
  */
  setgid (getgid ());
  setuid (getuid ());

  hack_uid_errno = 0;
  hack_uid_error = 0;

  /* If we're being run as root (as from xdm) then switch the user id
     to something safe. */
  if (getuid () == 0)
    {
      struct passwd *p;
      /* Locking can't work when running as root, because we have no way of
	 knowing what the user id of the logged in user is (so we don't know
	 whose password to prompt for.)
       */
      si->locking_disabled_p = True;
      si->nolock_reason = "running as root";
      p = getpwnam ("nobody");
      if (! p) p = getpwnam ("noaccess");
      if (! p) p = getpwnam ("daemon");
      if (! p) p = getpwnam ("bin");
      if (! p) p = getpwnam ("sys");
      if (! p)
	{
	  hack_uid_error = "couldn't find safe uid; running as root.";
	  hack_uid_errno = -1;
	}
      else
	{
	  struct group *g = getgrgid (p->pw_gid);
	  hack_uid_error = hack_uid_buf;
	  sprintf (hack_uid_error, "changing uid/gid to %s/%s (%ld/%ld).",
		   p->pw_name, (g ? g->gr_name : "???"),
		   (long) p->pw_uid, (long) p->pw_gid);

	  /* Change the gid to be a safe one.  If we can't do that, then
	     print a warning.  We change the gid before the uid so that we
	     change the gid while still root. */
	  if (setgid (p->pw_gid) != 0)
	    {
	      hack_uid_errno = errno;
	      sprintf (hack_uid_error, "couldn't set gid to %s (%ld)",
		       (g ? g->gr_name : "???"), (long) p->pw_gid);
	    }

	  /* Now change the uid to be a safe one. */
	  if (setuid (p->pw_uid) != 0)
	    {
	      hack_uid_errno = errno;
	      sprintf (hack_uid_error, "couldn't set uid to %s (%ld)",
		       p->pw_name, (long) p->pw_uid);
	    }
	}
    }
# ifndef NO_LOCKING
 else	/* disable locking if already being run as "someone else" */
   {
     struct passwd *p = getpwuid (getuid ());
     if (!p ||
	 !strcmp (p->pw_name, "root") ||
	 !strcmp (p->pw_name, "nobody") ||
	 !strcmp (p->pw_name, "noaccess") ||
	 !strcmp (p->pw_name, "daemon") ||
	 !strcmp (p->pw_name, "bin") ||
	 !strcmp (p->pw_name, "sys"))
       {
	 si->locking_disabled_p = True;
	 si->nolock_reason = hack_uid_buf;
	 sprintf (si->nolock_reason, "running as %s", p->pw_name);
       }
   }
# endif /* !NO_LOCKING */
}

void
hack_uid_warn (saver_info *si)
{
  saver_preferences *p = &si->prefs;

  if (! hack_uid_error)
    ;
  else if (hack_uid_errno == 0)
    {
      if (p->verbose_p)
	printf ("%s: %s\n", progname, hack_uid_error);
    }
  else
    {
      char buf [255];
      sprintf (buf, "%s: %s", progname, hack_uid_error);
      if (hack_uid_errno == -1)
	fprintf (stderr, "%s\n", buf);
      else
	{
	  errno = hack_uid_errno;
	  perror (buf);
	}
    }
}

#endif /* !NO_SETUID */
