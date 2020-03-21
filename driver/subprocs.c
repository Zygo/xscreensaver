/* subprocs.c --- choosing, spawning, and killing screenhacks.
 * xscreensaver, Copyright (c) 1991-2019 Jamie Zawinski <jwz@jwz.org>
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
#include <sys/param.h>		/* for PATH_MAX */

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>		/* for waitpid() and associated macros */
#endif

#ifdef HAVE_SETRLIMIT
# include <sys/resource.h>	/* for setrlimit() and RLIMIT_AS */
#endif

#ifdef VMS
# include <processes.h>
# include <unixio.h>		/* for close */
# include <unixlib.h>		/* for getpid */
# define pid_t int
# define fork  vfork
#endif /* VMS */

#include <signal.h>		/* for the signal names */
#include <time.h>

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
#include "exec.h"
#include "yarandom.h"
#include "visual.h"    /* for id_to_visual() */

extern saver_info *global_si_kludge;	/* I hate C so much... */


/* Used when printing error/debugging messages from signal handlers.
 */
static const char *
no_malloc_number_to_string (long num)
{
  static char string[128] = "";
  int num_digits;
  Bool negative_p = False;

  num_digits = 0;

  if (num == 0)
    return "0";

  if (num < 0)
    {
      negative_p = True;
      num = -num;
    }

  while ((num > 0) && (num_digits < sizeof(string) - 1))
    {
      int digit;
      digit = (int) num % 10;
      num_digits++;
      string[sizeof(string) - 1 - num_digits] = digit + '0';
      num /= 10;
    }

  if (negative_p)
    {
      num_digits++;
      string[sizeof(string) - 1 - num_digits] = '-';
    }

  return string + sizeof(string) - 1 - num_digits;
}

/* Like write(), but runs strlen() on the arg to get the length. */
static int
write_string (int fd, const char *str)
{
  return write (fd, str, strlen (str));
}

static int
write_long (int fd, long n)
{
  const char *str = no_malloc_number_to_string (n);
  return write_string (fd, str);
}


/* RLIMIT_AS (called RLIMIT_VMEM on some systems) controls the maximum size
   of a process's address space, i.e., the maximal brk(2) and mmap(2) values.
   Setting this lets you put a cap on how much memory a process can allocate.

   Except the "and mmap()" part kinda makes this useless, since many GL
   implementations end up using mmap() to pull the whole frame buffer into
   memory (or something along those lines) making it appear processes are
   using hundreds of megabytes when in fact they're using very little, and
   we end up capping their mallocs prematurely.  YAY!
 */
#if defined(RLIMIT_VMEM) && !defined(RLIMIT_AS)
# define RLIMIT_AS RLIMIT_VMEM
#endif

static void
limit_subproc_memory (int address_space_limit, Bool verbose_p)
{

/* This has caused way more problems than it has solved...
   Let's just completely ignore the "memoryLimit" option now.
 */
#undef HAVE_SETRLIMIT

#if defined(HAVE_SETRLIMIT) && defined(RLIMIT_AS)
  struct rlimit r;

  if (address_space_limit < 10 * 1024)  /* let's not be crazy */
    return;

  if (getrlimit (RLIMIT_AS, &r) != 0)
    {
      char buf [512];
      sprintf (buf, "%s: getrlimit(RLIMIT_AS) failed", blurb());
      perror (buf);
      return;
    }

  r.rlim_cur = address_space_limit;

  if (setrlimit (RLIMIT_AS, &r) != 0)
    {
      char buf [512];
      sprintf (buf, "%s: setrlimit(RLIMIT_AS, {%lu, %lu}) failed",
               blurb(), r.rlim_cur, r.rlim_max);
      perror (buf);
      return;
    }

  if (verbose_p)
    {
      int i = address_space_limit;
      char buf[100];
      if      (i >= (1<<30) && i == ((i >> 30) << 30))
        sprintf(buf, "%dG", i >> 30);
      else if (i >= (1<<20) && i == ((i >> 20) << 20))
        sprintf(buf, "%dM", i >> 20);
      else if (i >= (1<<10) && i == ((i >> 10) << 10))
        sprintf(buf, "%dK", i >> 10);
      else
        sprintf(buf, "%d bytes", i);

      fprintf (stderr, "%s: limited pid %lu address space to %s.\n",
               blurb(), (unsigned long) getpid (), buf);
    }

#endif /* HAVE_SETRLIMIT && RLIMIT_AS */
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
  int screen;
  enum job_status status;
  time_t launched, killed;
  struct screenhack_job *next;
};

static struct screenhack_job *jobs = 0;

/* for debugging -- nothing calls this, but it's useful to invoke from gdb.
 */
void show_job_list (void);

void
show_job_list (void)
{
  struct screenhack_job *job;
  fprintf(stderr, "%s: job list:\n", blurb());
  for (job = jobs; job; job = job->next)
    {
      char b[] = "           ??:??:??     ";
      char *t = (job->killed   ? timestring (job->killed) :
                 job->launched ? timestring (job->launched) : b);
      t += 11;
      t[8] = 0;
        fprintf (stderr, "  %5ld: %2d: (%s) %s %s\n",
                 (long) job->pid,
                 job->screen,
                 (job->status == job_running ? "running" :
                  job->status == job_stopped ? "stopped" :
                  job->status == job_killed  ? " killed" :
                  job->status == job_dead    ? "   dead" : "    ???"),
                 t, job->name);
    }
  fprintf (stderr, "\n");
}


static void clean_job_list (void);

static struct screenhack_job *
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

static void await_dying_children (saver_info *si);
#ifndef VMS
static void describe_dead_child (saver_info *, pid_t, int wait_status);
#endif


/* Semaphore to temporarily turn the SIGCHLD handler into a no-op.
   Don't alter this directly -- use block_sigchld() / unblock_sigchld().
 */
static int block_sigchld_handler = 0;


#ifdef HAVE_SIGACTION
 sigset_t
#else  /* !HAVE_SIGACTION */
 int
#endif /* !HAVE_SIGACTION */
block_sigchld (void)
{
#ifdef HAVE_SIGACTION
  struct sigaction sa;
  sigset_t child_set;

  memset (&sa, 0, sizeof (sa));
  sa.sa_handler = SIG_IGN;
  sigaction (SIGPIPE, &sa, NULL);

  sigemptyset (&child_set);
  sigaddset (&child_set, SIGCHLD);
  sigprocmask (SIG_BLOCK, &child_set, 0);

#else  /* !HAVE_SIGACTION */
  signal (SIGPIPE, SIG_IGN);
#endif /* !HAVE_SIGACTION */

  block_sigchld_handler++;

#ifdef HAVE_SIGACTION
  return child_set;
#else  /* !HAVE_SIGACTION */
  return 0;
#endif /* !HAVE_SIGACTION */
}

void
unblock_sigchld (void)
{
  if (block_sigchld_handler <= 0)
    abort();

  if (block_sigchld_handler <= 1)  /* only unblock if count going to 0 */
    {
#ifdef HAVE_SIGACTION
  struct sigaction sa;
  sigset_t child_set;

  memset(&sa, 0, sizeof (sa));
  sa.sa_handler = SIG_DFL;
  sigaction(SIGPIPE, &sa, NULL);

  sigemptyset(&child_set);
  sigaddset(&child_set, SIGCHLD);
  sigprocmask(SIG_UNBLOCK, &child_set, 0);

#else /* !HAVE_SIGACTION */
  signal(SIGPIPE, SIG_DFL);
#endif /* !HAVE_SIGACTION */
    }

  block_sigchld_handler--;
}

int
kill_job (saver_info *si, pid_t pid, int signal)
{
  saver_preferences *p = &si->prefs;
  struct screenhack_job *job;
  int status = -1;

  clean_job_list();

  if (in_signal_handler_p)
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
  case SIGTERM:
    job->status = job_killed;
    job->killed = time ((time_t *) 0);
    break;
#ifdef SIGSTOP
    /* #### there must be a way to do this on VMS... */
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
                 "%s: %d: child process %lu (%s) was already dead.\n",
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
  in_signal_handler_p++;

  if (si->prefs.debug_p)
    {
      /* Don't call fprintf() from signal handlers, as it might malloc.
      fprintf(stderr, "%s: got SIGCHLD%s\n", blurb(),
	    (block_sigchld_handler ? " (blocked)" : ""));
      */
      write_string (STDERR_FILENO, blurb());
      write_string (STDERR_FILENO, ": got SIGCHLD");

      if (block_sigchld_handler)
        write_string (STDERR_FILENO, " (blocked)\n");
      else
        write_string (STDERR_FILENO, "\n");
    }

  if (block_sigchld_handler < 0)
    abort();
  else if (block_sigchld_handler == 0)
    {
      block_sigchld();
      await_dying_children (si);
      unblock_sigchld();
    }

  init_sigchld();
  in_signal_handler_p--;
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
            {
              /* Don't call fprintf() from signal handlers, as it might malloc.
	      fprintf (stderr, "%s: waitpid(-1) ==> %ld (%d)\n", blurb(),
		       (long) kid, errno);
               */
              write_string (STDERR_FILENO, blurb());
              write_string (STDERR_FILENO, ": waitpid(-1) ==> ");
              write_long   (STDERR_FILENO, (long) kid);
              write_string (STDERR_FILENO, " (");
              write_long   (STDERR_FILENO, (long) errno);
              write_string (STDERR_FILENO, ")\n");
            }
          else
            {
              /* Don't call fprintf() from signal handlers, as it might malloc.
              fprintf (stderr, "%s: waitpid(-1) ==> %ld\n", blurb(),
                       (long) kid);
               */
              write_string (STDERR_FILENO, blurb());
              write_string (STDERR_FILENO, ": waitpid(-1) ==> ");
              write_long   (STDERR_FILENO, (long) kid);
              write_string (STDERR_FILENO, "\n");
            }
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
  int screen_no = job ? job->screen : 0;

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
        {
          /* Don't call fprintf() from signal handlers, as it might malloc.
	  fprintf (stderr,
		   "%s: %d: child pid %lu (%s) exited abnormally (code %d).\n",
		   blurb(), screen_no, (unsigned long) kid, name, exit_status);
           */
          write_string (STDERR_FILENO, blurb());
          write_string (STDERR_FILENO, ": ");
          write_long   (STDERR_FILENO, (long) screen_no);
          write_string (STDERR_FILENO, ": child pid ");
          write_long   (STDERR_FILENO, (long) kid);
          write_string (STDERR_FILENO, " (");
          write_string (STDERR_FILENO, name);
          write_string (STDERR_FILENO, ") exited abnormally (code ");
          write_long   (STDERR_FILENO, (long) exit_status);
          write_string (STDERR_FILENO, ").\n"); 
        }
      else if (p->verbose_p)
        {
          /* Don't call fprintf() from signal handlers, as it might malloc.
	  fprintf (stderr, "%s: %d: child pid %lu (%s) exited normally.\n",
		   blurb(), screen_no, (unsigned long) kid, name);
           */
          write_string (STDERR_FILENO, blurb());
          write_string (STDERR_FILENO, ": ");
          write_long   (STDERR_FILENO, (long) screen_no);
          write_string (STDERR_FILENO, ": child pid ");
          write_long   (STDERR_FILENO, (long) kid);
          write_string (STDERR_FILENO, " (");
          write_string (STDERR_FILENO, name);
          write_string (STDERR_FILENO, ") exited normally.\n");
        }

      if (job)
	job->status = job_dead;
    }
  else if (WIFSIGNALED (wait_status))
    {
      if (p->verbose_p ||
	  !job ||
	  job->status != job_killed ||
	  WTERMSIG (wait_status) != SIGTERM)
        {
          /* Don't call fprintf() from signal handlers, as it might malloc.
	  fprintf (stderr, "%s: %d: child pid %lu (%s) terminated with %s.\n",
		   blurb(), screen_no, (unsigned long) kid, name,
		   signal_name (WTERMSIG(wait_status)));
           */
          write_string (STDERR_FILENO, blurb());
          write_string (STDERR_FILENO, ": ");
          write_long   (STDERR_FILENO, (long) screen_no);
          write_string (STDERR_FILENO, ": child pid ");
          write_long   (STDERR_FILENO, (long) kid);
          write_string (STDERR_FILENO, " (");
          write_string (STDERR_FILENO, name);
          write_string (STDERR_FILENO, ") terminated with signal ");
          write_long   (STDERR_FILENO, WTERMSIG(wait_status));
          write_string (STDERR_FILENO, ".\n");
        }

      if (job)
	job->status = job_dead;
    }
  else if (WIFSTOPPED (wait_status))
    {
      if (p->verbose_p)
        {
          /* Don't call fprintf() from signal handlers, as it might malloc.
	  fprintf (stderr, "%s: child pid %lu (%s) stopped with %s.\n",
		   blurb(), (unsigned long) kid, name,
		   signal_name (WSTOPSIG (wait_status)));
           */
          write_string (STDERR_FILENO, blurb());
          write_string (STDERR_FILENO, ": ");
          write_long   (STDERR_FILENO, (long) screen_no);
          write_string (STDERR_FILENO, ": child pid ");
          write_long   (STDERR_FILENO, (long) kid);
          write_string (STDERR_FILENO, " (");
          write_string (STDERR_FILENO, name);
          write_string (STDERR_FILENO, ") stopped with signal ");
          write_long   (STDERR_FILENO, WSTOPSIG(wait_status));
          write_string (STDERR_FILENO, ".\n");
        }

      if (job)
	job->status = job_stopped;
    }
  else
    {
      /* Don't call fprintf() from signal handlers, as it might malloc.
      fprintf (stderr, "%s: child pid %lu (%s) died in a mysterious way!",
	       blurb(), (unsigned long) kid, name);
       */
      write_string (STDERR_FILENO, blurb());
      write_string (STDERR_FILENO, ": ");
      write_long   (STDERR_FILENO, (long) screen_no);
      write_string (STDERR_FILENO, ": child pid ");
      write_long   (STDERR_FILENO, (long) kid);
      write_string (STDERR_FILENO, " (");
      write_string (STDERR_FILENO, name);
      write_string (STDERR_FILENO, ") died in a mysterious way!");
      if (job)
	job->status = job_dead;
    }

  /* Clear out the pid so that screenhack_running_p() knows it's dead.
   */
  if (!job || job->status == job_dead)
    {
    for (i = 0; i < si->nscreens; i++)
      {
	saver_screen_info *ssi = &si->screens[i];
	if (kid == ssi->pid)
	  ssi->pid = 0;
      }
# ifdef HAVE_LIBSYSTEMD
    if (kid == si->systemd_pid)
      si->systemd_pid = 0;
# endif
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

  if (hack->visual && *hack->visual)
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
print_path_error (const char *program)
{
  char buf [512];
  char *cmd = strdup (program);
  char *token = strchr (cmd, ' ');

  if (token) *token = 0;
  sprintf (buf, "%s: could not execute \"%.100s\"", blurb(), cmd);
  free (cmd);
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
      if (! getcwd (path, sizeof(path)))
        *path = 0;
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


/* Executes the command in another process.
   Command may be any single command acceptable to /bin/sh.
   It may include wildcards, but no semicolons.
   If successful, the pid of the other process is returned.
   Otherwise, -1 is returned and an error may have been
   printed to stderr.
 */
pid_t
fork_and_exec (saver_screen_info *ssi, const char *command)
{
  return fork_and_exec_1 (ssi->global, ssi, command);
}

pid_t
fork_and_exec_1 (saver_info *si, saver_screen_info *ssi, const char *command)
{
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
      limit_subproc_memory (p->inferior_memory_limit, p->verbose_p);
      if (ssi)
        hack_subproc_environment (ssi->screen, ssi->screensaver_window);

      if (p->verbose_p)
        fprintf (stderr, "%s: %d: spawning \"%s\" in pid %lu.\n",
                 blurb(), (ssi ? ssi->number : 0), command,
                 (unsigned long) getpid ());

      exec_command (p->shell, command, p->nice_inferior);

      /* If that returned, we were unable to exec the subprocess.
         Print an error message, if desired.
       */
      if (! p->ignore_uninstalled_p)
        print_path_error (command);

      exit (1);  /* exits child fork */
      break;

    default:	/* parent */
      (void) make_job (forked, (ssi ? ssi->number : 0), command);
      break;
    }

  return forked;
}


void
spawn_screenhack (saver_screen_info *ssi)
{
  saver_info *si = ssi->global;
  saver_preferences *p = &si->prefs;
  XFlush (si->dpy);

  if (!monitor_powered_on_p (si))
    {
      if (si->prefs.verbose_p)
        fprintf (stderr,
                 "%s: %d: X says monitor has powered down; "
                 "not launching a hack.\n", blurb(), ssi->number);
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
          if (si->selection_mode < 0)
            si->selection_mode = 0;
          return;
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
		      "%s: %d: no programs enabled, or no suitable visuals.\n",
			blurb(), ssi->number);
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

      forked = fork_and_exec (ssi, hack->command);
      switch ((int) forked)
	{
	case -1: /* fork failed */
	case 0:  /* child fork (can't happen) */
	  sprintf (buf, "%s: couldn't fork", blurb());
	  perror (buf);
	  restore_real_vroot (si);
	  saver_exit (si, 1, "couldn't fork");
	  break;

	default:
	  ssi->pid = forked;
	  break;
	}
    }

  store_saver_status (si);  /* store current hack number */
}


void
kill_screenhack (saver_screen_info *ssi)
{
  saver_info *si = ssi->global;
  if (ssi->pid)
    kill_job (si, ssi->pid, SIGTERM);
  ssi->pid = 0;
}


void
suspend_screenhack (saver_screen_info *ssi, Bool suspend_p)
{
#ifdef SIGSTOP	/* older VMS doesn't have it... */
  saver_info *si = ssi->global;
  if (ssi->pid)
    kill_job (si, ssi->pid, (suspend_p ? SIGSTOP : SIGCONT));
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
  Bool any_running_p = False;
  int i;
  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      if (ssi->pid) any_running_p = True;
    }
  return any_running_p;
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
      char *npath;
      if (! opath) opath = "/bin:/usr/bin";  /* WTF */
      npath = (char *) malloc(strlen(def_path) + strlen(opath) + 20);
      strcpy (npath, "PATH=");
      strcat (npath, def_path);
      strcat (npath, ":");
      strcat (npath, opath);

      if (putenv (npath))
	abort ();

      /* don't free (npath) -- some implementations of putenv (BSD 4.4,
         glibc 2.0) copy the argument, but some (libc4,5, glibc 2.1.2)
         do not.  So we must leak it (and/or the previous setting). Yay.
       */
    }
#endif /* HAVE_PUTENV && DEFAULT_PATH_PREFIX */
}


void
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

     Likewise, store a window ID in $XSCREENSAVER_WINDOW -- this will allow
     us to (eventually) run multiple hacks in Xinerama mode, where each hack
     has the same $DISPLAY but a different piece of glass.
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

  /* Allegedly, BSD 4.3 didn't have putenv(), but nobody runs such systems
     any more, right?  It's not Posix, but everyone seems to have it. */
#ifdef HAVE_PUTENV
  if (putenv (ndpy))
    abort ();
  if (putenv (nssw))
    abort ();

  /* don't free ndpy/nssw -- some implementations of putenv (BSD 4.4,
     glibc 2.0) copy the argument, but some (libc4,5, glibc 2.1.2)
     do not.  So we must leak it (and/or the previous setting). Yay.
   */
#endif /* HAVE_PUTENV */
}


/* GL crap */

Visual *
get_best_gl_visual (saver_info *si, Screen *screen)
{
  pid_t forked;
  int fds [2];
  int in, out;
  int errfds[2];
  int errin = -1, errout = -1;
  char buf[1024];

  char *av[10];
  int ac = 0;

  av[ac++] = "xscreensaver-gl-helper";
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

  block_sigchld();   /* This blocks it in the parent and child, to avoid
                        racing.  It is never unblocked in the child before
                        the child exits, but that doesn't matter.
                      */

  switch ((int) (forked = fork ()))
    {
    case -1:
      {
        sprintf (buf, "%s: couldn't fork", blurb());
        perror (buf);
        saver_exit (si, 1, 0);
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

        FILE *f = fdopen (in, "r");
        unsigned long v = 0;
        char c;

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
          {
            write_string (STDERR_FILENO, blurb());
            write_string (STDERR_FILENO, ": waitpid(");
            write_long   (STDERR_FILENO, (long) forked);
            write_string (STDERR_FILENO, ") ==> ");
            write_long   (STDERR_FILENO, (long) pid);
            write_string (STDERR_FILENO, "\n");
          }

        unblock_sigchld();   /* child is dead and waited, unblock now. */

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
              fprintf (stderr, "%s: %d: %s: GL visual is 0x%X%s.\n",
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


/* Re-execs the process with the arguments in saved_argv.  Does not return.
 */
void
restart_process (saver_info *si)
{
  fflush (stdout);
  fflush (stderr);
  shutdown_stderr (si);
  if (si->prefs.verbose_p)
    {
      int i;
      fprintf (stderr, "%s: re-executing", blurb());
      for (i = 0; saved_argv[i]; i++)
	fprintf (stderr, " %s", saved_argv[i]);
      fprintf (stderr, "\n");
    }
  describe_uids (si, stderr);
  fprintf (stderr, "\n");

  fflush (stdout);
  fflush (stderr);
  execvp (saved_argv [0], saved_argv);	/* shouldn't return */
  {
    char buf [512];
    sprintf (buf, "%s: could not restart process", blurb());
    perror(buf);
    fflush(stderr);
    abort();
  }
}
