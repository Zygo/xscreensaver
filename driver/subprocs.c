/* subprocs.c --- choosing, spawning, and killing screenhacks.
 * xscreensaver, Copyright (c) 1991, 1992, 1993, 1995, 1997, 1998
 *  Jamie Zawinski <jwz@jwz.org>
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
# include <errno.h>
#endif

#include <sys/time.h>		/* sys/resource.h needs this for timeval */

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>		/* for waitpid() and associated macros */
#endif

#if defined(HAVE_SETPRIORITY) && defined(PRIO_PROCESS)
# include <sys/resource.h>	/* for setpriority() and PRIO_PROCESS */
#endif

#ifdef VMS
# include <processes.h>
# include <unixio.h>		/* for close */
# include <unixlib.h>		/* for getpid */
# define pid_t int
# define fork  vfork
#endif /* VMS */

#include <signal.h>		/* for the signal names */

#if !defined(SIGCHLD) && defined(SIGCLD)
# define SIGCHLD SIGCLD
#endif

#if 0 /* putenv() is declared in stdlib.h on modern linux systems. */
#ifdef HAVE_PUTENV
extern int putenv (/* const char * */);	/* getenv() is in stdlib.h... */
#endif
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
	sprintf (buf, "%s: nice(%d) failed", blurb(), n);
	perror (buf);
    }
  }
#elif defined(HAVE_SETPRIORITY) && defined(PRIO_PROCESS)
  if (setpriority (PRIO_PROCESS, getpid(), nice_level) != 0)
    {
      char buf [512];
      sprintf (buf, "%s: setpriority(PRIO_PROCESS, %lu, %d) failed",
	       blurb(), (unsigned long) getpid(), nice_level);
      perror (buf);
    }
#else
  fprintf (stderr,
	   "%s: don't know how to change process priority on this system.\n",
	   blurb());

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
    sprintf (buf, "%s: could not execute \"%s\"", blurb(), av[0]);
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
  char *command2 = (char *) malloc (strlen (command) + 10);
  const char *s;
  int got_eq = 0;
  const char *after_vars;

  /* Skip leading whitespace.
   */
  while (*command == ' ' || *command == '\t')
    command++;

  /* If the string has a series of tokens with "=" in them at them, set
     `after_vars' to point into the string after those tokens and any
     trailing whitespace.  Otherwise, after_vars == command.
   */
  after_vars = command;
  for (s = command; *s; s++)
    {
      if (*s == '=') got_eq = 1;
      else if (*s == ' ')
        {
          if (got_eq)
            {
              while (*s == ' ' || *s == '\t')
                s++;
              after_vars = s;
              got_eq = 0;
            }
          else
            break;
        }
    }

  *command2 = 0;
  strncat (command2, command, after_vars - command);
  strcat (command2, "exec ");
  strcat (command2, after_vars);

  /* We have now done these transformations:
     "foo -x -y"               ==>  "exec foo -x -y"
     "BLAT=foop      foo -x"   ==>  "BLAT=foop      exec foo -x"
     "BLAT=foop A=b  foo -x"   ==>  "BLAT=foop A=b  exec foo -x"
   */


  /* Invoke the shell as "/bin/sh -c 'exec prog -arg -arg ...'" */
  av [ac++] = (char *) shell;
  av [ac++] = "-c";
  av [ac++] = command2;
  av [ac]   = 0;

  execvp (av[0], av);			/* shouldn't return. */

  {
    char buf [512];
    sprintf (buf, "%s: execvp(\"%s\") failed", blurb(), av[0]);
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
  Bool hairy_p = !!strpbrk (command, "*?$&!<>[];`'\\\"=");
  /* note: = is in the above because of the sh syntax "FOO=bar cmd". */

  if (getuid() == (uid_t) 0 || geteuid() == (uid_t) 0)
    {
      /* If you're thinking of commenting this out, think again.
         If you do so, you will open a security hole.  Mail jwz
         so that he may enlighten you as to the error of your ways.
       */
      fprintf (stderr, "%s: we're still running as root!  Disaster!\n",
               blurb());
      saver_exit (si, 1, 0);
    }

  if (p->verbose_p)
    fprintf (stderr, "%s: spawning \"%s\" in pid %lu%s.\n",
	     blurb(), command, (unsigned long) getpid (),
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
    fprintf (stderr, "%s: spawning \"%s\" in pid %lu.\n",
	     blurb(), command, getpid());
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
  fprintf(stderr, "%s: job list:\n", blurb());
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
  int got_eq = 0;
  int first = 1;

  clean_job_list();

 AGAIN:
  while (isspace(*in)) in++;		/* skip whitespace */
  while (!isspace(*in) && *in != ':') {
    if (*in == '=') got_eq = 1;
    *out++ = *in++;			/* snarf first token */
  }

  if (got_eq)				/* if the first token was FOO=bar */
    {					/* then get the next token instead. */
      got_eq = 0;
      out = name;
      first = 0;
      goto AGAIN;
    }

  while (isspace(*in)) in++;		/* skip whitespace */
  if (first && *in == ':')		/* token was a visual name; skip it. */
    {
      out = name;
      first = 0;
      goto AGAIN;
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
		 blurb(), (long) pid);
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
    fprintf (stderr, "%s: %s pid %lu.\n", blurb(),
	     (signal == SIGTERM ? "killing" :
	      signal == SIGSTOP ? "suspending" :
	      signal == SIGCONT ? "resuming" : "signalling"),
	     (unsigned long) job->pid);
#else  /* !SIGSTOP */
  if (p->verbose_p)
    fprintf (stderr, "%s: %s pid %lu.\n", blurb(), "killing",
	     (unsigned long) job->pid);
#endif /* !SIGSTOP */

  status = kill (job->pid, signal);

  if (p->verbose_p && status < 0)
    {
      if (errno == ESRCH)
	fprintf (stderr, "%s: child process %lu (%s) was already dead.\n",
		 blurb(), job->pid, job->name);
      else
	{
	  char buf [1024];
	  sprintf (buf, "%s: couldn't kill child process %lu (%s)",
		   blurb(), job->pid, job->name);
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
    fprintf(stderr, "%s: got SIGCHLD%s\n", blurb(),
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
		 blurb(), (unsigned long) kid, name, exit_status);
      else if (p->verbose_p)
	fprintf (stderr, "%s: child pid %lu (%s) exited normally.\n",
		 blurb(), (unsigned long) kid, name);

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
		 blurb(), (unsigned long) kid, name,
		 signal_name (WTERMSIG(wait_status)));

      if (job)
	job->status = job_dead;
    }
  else if (WIFSTOPPED (wait_status))
    {
      if (p->verbose_p)
	fprintf (stderr, "%s: child pid %lu (%s) stopped with %s.\n",
		 blurb(), (unsigned long) kid, name,
		 signal_name (WSTOPSIG (wait_status)));

      if (job)
	job->status = job_stopped;
    }
  else
    {
      fprintf (stderr, "%s: child pid %lu (%s) died in a mysterious way!",
	       blurb(), (unsigned long) kid, name);
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
	  sprintf (buf, "%s: couldn't catch SIGCHLD", blurb());
	  perror (buf);
	}
      sigchld_initialized_p = True;
    }

# else  /* !HAVE_SIGACTION */

  if (((long) signal (SIGCHLD, sigchld_handler)) == -1L)
    {
      char buf [255];
      sprintf (buf, "%s: couldn't catch SIGCHLD", blurb());
      perror (buf);
    }
# endif /* !HAVE_SIGACTION */
#endif /* SIGCHLD */
}





static Bool
select_visual_of_hack (saver_screen_info *ssi, screenhack *hack)
{
  saver_info *si = ssi->global;
  saver_preferences *p = &si->prefs;
  Bool selected;

  if (hack->visual && *hack->visual == ':')
    selected = select_visual(ssi, hack->visual);
  else
    selected = select_visual(ssi, 0);

  if (!selected && (p->verbose_p || si->demoing_p))
    fprintf (stderr,
             (si->demoing_p
              ? "%s: warning, no \"%s\" visual for \"%s\".\n"
              : "%s: no \"%s\" visual; skipping \"%s\".\n"),
             blurb(),
             (hack->visual && *hack->visual ? hack->visual : "???"),
             hack->command);

  return selected;
}


static void
spawn_screenhack_1 (saver_screen_info *ssi, Bool first_time_p)
{
  saver_info *si = ssi->global;
  saver_preferences *p = &si->prefs;
  raise_window (si, first_time_p, True, False);
  XFlush (si->dpy);

  if (p->screenhacks_count)
    {
      screenhack *hack;
      pid_t forked;
      char buf [255];
      int new_hack;
      int retry_count = 0;
      Bool force = False;

    AGAIN:

      if (p->screenhacks_count == 1)
	/* If there is only one hack in the list, there is no choice. */
	new_hack = 0;

      else if (si->selection_mode == -1)
	/* Select the next hack, wrapping. */
	new_hack = (ssi->current_hack + 1) % p->screenhacks_count;

      else if (si->selection_mode == -2)
	/* Select the previous hack, wrapping. */
	new_hack = ((ssi->current_hack + p->screenhacks_count - 1)
		    % p->screenhacks_count);

      else if (si->selection_mode > 0)
	/* Select a specific hack, by number.  No negotiation. */
	{
	  new_hack = ((si->selection_mode - 1) % p->screenhacks_count);
	  force = True;
	}
      else
	{
	  /* Select a random hack (but not the one we just ran.) */
	  while ((new_hack = random () % p->screenhacks_count)
		 == ssi->current_hack)
	    ;
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
			"%s: no suitable visuals for these programs.\n",
			blurb());
	      return;
	    }
	  else
	    goto AGAIN;
	}

      /* Turn off "next" and "prev" modes now, but "demo" mode is only
	 turned off by explicit action.
       */
      if (si->selection_mode < 0)
	si->selection_mode = 0;

      switch ((int) (forked = fork ()))
	{
	case -1:
	  sprintf (buf, "%s: couldn't fork", blurb());
	  perror (buf);
	  restore_real_vroot (si);
	  saver_exit (si, 1, 0);

	case 0:
	  close (ConnectionNumber (si->dpy));	/* close display fd */
	  nice_subproc (p->nice_inferior);	/* change process priority */
	  hack_subproc_environment (ssi);	/* set $DISPLAY */
	  exec_screenhack (si, hack->command);	/* this does not return */
	  abort();
	  break;

	default:
	  ssi->pid = forked;
	  (void) make_job (forked, hack->command);
	  break;
	}
    }
}


void
spawn_screenhack (saver_info *si, Bool first_time_p)
{
  if (monitor_powered_on_p (si))
    {
      int i;
      for (i = 0; i < si->nscreens; i++)
        {
          saver_screen_info *ssi = &si->screens[i];
          spawn_screenhack_1 (ssi, first_time_p);
        }
    }
  else if (si->prefs.verbose_p)
    fprintf (stderr,
             "%s: server reports that monitor has powered down; "
             "not launching a new hack.\n", blurb());

  store_saver_status (si);  /* store current hack numbers */
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


/* Environment variables. */


/* Modifies $PATH in the current environment, so that if DEFAULT_PATH_PREFIX
   is defined, the xscreensaver daemon will search that directory for hacks.
 */
void
hack_environment (saver_info *si)
{
#if defined(HAVE_PUTENV) && defined(DEFAULT_PATH_PREFIX)
  static const char *def_path = DEFAULT_PATH_PREFIX;
  if (def_path && *def_path)
    {
      const char *opath = getenv("PATH");
      char *npath = (char *) malloc(strlen(def_path) + strlen(opath) + 20);
      strcpy (npath, "PATH=");
      strcat (npath, def_path);
      strcat (npath, ":");
      strcat (npath, opath);

      if (putenv (npath))
	abort ();
    }
#endif /* HAVE_PUTENV && DEFAULT_PATH_PREFIX */
}


void
hack_subproc_environment (saver_screen_info *ssi)
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


/* Restarting the xscreensaver process from scratch. */

static char **saved_argv;

void
save_argv (int argc, char **argv)
{
  saved_argv = (char **) calloc (argc+2, sizeof (char *));
  saved_argv [argc] = 0;
  while (argc--)
    {
      int i = strlen (argv [argc]) + 1;
      saved_argv [argc] = (char *) malloc (i);
      memcpy (saved_argv [argc], argv [argc], i);
    }
}


/* Re-execs the process with the arguments in saved_argv.
   Does not return unless there was an error.
 */
void
restart_process (saver_info *si)
{
  if (si->prefs.verbose_p)
    {
      int i;
      fprintf (real_stderr, "%s: re-executing", blurb());
      for (i = 0; saved_argv[i]; i++)
	fprintf (real_stderr, " %s", saved_argv[i]);
      fprintf (real_stderr, "\n");
    }
  describe_uids (si, real_stderr);
  fprintf (real_stderr, "\n");

  fflush (real_stdout);
  fflush (real_stderr);
  execvp (saved_argv [0], saved_argv);	/* shouldn't return */
  {
    char buf [512];
    sprintf (buf, "%s: could not restart process", blurb());
    perror(buf);
    fflush(stderr);
  }
  XBell(si->dpy, 0);
}
