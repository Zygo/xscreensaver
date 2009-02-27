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

#if __STDC__
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#endif

#include <stdio.h>

#include <X11/Xlib.h>		/* not used for much... */

#ifndef ESRCH
#include <errno.h>
#endif

#include <sys/time.h>		/* sys/resource.h needs this for timeval */
#include <sys/resource.h>	/* for setpriority() and PRIO_PROCESS */
#include <sys/wait.h>		/* for waitpid() and associated macros */
#include <signal.h>		/* for the signal names */

extern char **environ;		/* why isn't this in some header file? */

#ifndef NO_SETUID
#include <pwd.h>		/* for getpwnam() and struct passwd */
#include <grp.h>		/* for getgrgid() and struct group */
#endif /* NO_SETUID */

#if !defined(SIGCHLD) && defined(SIGCLD)
#define SIGCHLD SIGCLD
#endif

#if __STDC__
extern int putenv (const char *);	/* getenv() is in stdlib.h... */
extern int kill (pid_t, int);		/* signal() is in sys/signal.h... */
#endif

# if defined(SVR4) || defined(SYSV)
#  define random() rand()
# else /* !totally-losing-SYSV */
extern long random();			/* rand() is in stdlib.h... */
# endif /* !totally-losing-SYSV */


#include "xscreensaver.h"

/* this must be `sh', not whatever $SHELL happens to be. */
char *shell;
static pid_t pid = 0;
char **screenhacks;
int screenhacks_count;
int current_hack = -1;
char *demo_hack;
int next_mode_p = 0;
Bool locking_disabled_p = False;
int nice_inferior = 0;

extern Bool demo_mode_p;

static void
exec_screenhack (command)
     char *command;
{
  char *tmp;
  char buf [512];
  char *av [5];
  int ac = 0;

  /* Close this fork's version of the display's fd.  It will open its own. */
  close (ConnectionNumber (dpy));
  
  /* I don't believe what a sorry excuse for an operating system UNIX is!

     - I want to spawn a process.
     - I want to know it's pid so that I can kill it.
     - I would like to receive a message when it dies of natural causes.
     - I want the spawned process to have user-specified arguments.

     The *only way* to parse arguments the way the shells do is to run a
     shell (or duplicate what they do, which would be a *lot* of code.)

     The *only way* to know the pid of the process is to fork() and exec()
     it in the spawned side of the fork.

     But if you're running a shell to parse your arguments, this gives you
     the pid of the SHELL, not the pid of the PROCESS that you're actually
     interested in, which is an *inferior* of the shell.  This also means
     that the SIGCHLD you get applies to the shell, not its inferior.

     So, the only solution other than implementing an argument parser here
     is to force the shell to exec() its inferior.  What a fucking hack!
     We prepend "exec " to the command string.
   */
  tmp = command;
  command = (char *) malloc (strlen (tmp) + 6);
  memcpy (command, "exec ", 5);
  memcpy (command + 5, tmp, strlen (tmp) + 1);

  /* Invoke the shell as "/bin/sh -c 'exec prog -arg -arg ...'" */
  av [ac++] = shell;
  av [ac++] = "-c";
  av [ac++] = command;
  av [ac++] = 0;
  
  if (verbose_p)
    printf ("%s: spawning \"%s\" in pid %d.\n", progname, command, getpid ());

#if defined(SYSV) || defined(__hpux)
  {
    int old_nice = nice (0);
    int n = nice_inferior - old_nice;
    errno = 0;
    if (nice (n) == -1 && errno != 0)
      {
	sprintf (buf, "%s: %snice(%d) failed", progname,
		 (verbose_p ? "## " : ""), n);
	perror (buf);
      }
  }
#else /* !SYSV */
#ifdef PRIO_PROCESS
  if (setpriority (PRIO_PROCESS, getpid(), nice_inferior) != 0)
    {
      sprintf (buf, "%s: %ssetpriority(PRIO_PROCESS, %d, %d) failed",
	       progname, (verbose_p ? "## " : ""), getpid(), nice_inferior);
      perror (buf);
    }
#else /* !PRIO_PROCESS */
  if (nice_inferior != 0)
    fprintf (stderr,
	   "%s: %sdon't know how to change process priority on this system.\n",
	     progname, (verbose_p ? "## " : ""));
#endif /* !PRIO_PROCESS */
#endif /* !SYSV */

  /* Now overlay the current process with /bin/sh running the command.
     If this returns, it's an error.
   */
  execve (av [0], av, environ);

  sprintf (buf, "%s: %sexecve() failed", progname, (verbose_p ? "## " : ""));
  perror (buf);
  exit (1);	/* Note this this only exits a child fork.  */
}

/* to avoid a race between the main thread and the SIGCHLD handler */
static int killing = 0;
static Bool suspending = False;

static char *current_hack_name P((void));

static void
await_child_death (killed)
     Bool killed;
{
  Bool suspended_p = False;
  int status;
  pid_t kid;
  killing = 1;
  if (! pid)
    return;

  do
    {
      kid = waitpid (pid, &status, WUNTRACED);
    }
  while (kid == -1 && errno == EINTR);

  if (kid == pid)
    {
      if (WIFEXITED (status))
	{
	  int exit_status = WEXITSTATUS (status);
	  if (exit_status & 0x80)
	    exit_status |= ~0xFF;
	  if (exit_status != 0 && verbose_p)
	    printf ("%s: child pid %d (%s) exited abnormally (code %d).\n",
		    progname, pid, current_hack_name (), exit_status);
	  else if (verbose_p)
	    printf ("%s: child pid %d (%s) exited normally.\n",
		    progname, pid, current_hack_name ());
	}
      else if (WIFSIGNALED (status))
	{
	  if (!killed || WTERMSIG (status) != SIGTERM)
	    fprintf (stderr,
		     "%s: %schild pid %d (%s) terminated with signal %d!\n",
		     progname, (verbose_p ? "## " : ""),
		     pid, current_hack_name (), WTERMSIG (status));
	  else if (verbose_p)
	    printf ("%s: child pid %d (%s) terminated with SIGTERM.\n",
		    progname, pid, current_hack_name ());
	}
      else if (suspending)
	{
	  suspended_p = True;
	  suspending = False; /* complain if it happens twice */
	}
      else if (WIFSTOPPED (status))
	{
	  suspended_p = True;
	  fprintf (stderr, "%s: %schild pid %d (%s) stopped with signal %d!\n",
		   progname, (verbose_p ? "## " : ""), pid,
		   current_hack_name (), WSTOPSIG (status));
	}
      else
	fprintf (stderr, "%s: %schild pid %d (%s) died in a mysterious way!",
		 progname, (verbose_p ? "## " : ""), pid, current_hack_name());
    }
  else if (kid <= 0)
    fprintf (stderr, "%s: %swaitpid(%d, ...) says there are no kids?  (%d)\n",
	     progname, (verbose_p ? "## " : ""), pid, kid);
  else
    fprintf (stderr, "%s: %swaitpid(%d, ...) says proc %d died, not %d?\n",
	     progname, (verbose_p ? "## " : ""), pid, kid, pid);
  killing = 0;
  if (suspended_p != True)
    pid = 0;
}

static char *
current_hack_name ()
{
  static char chn [1024];
  char *hack = (demo_mode_p ? demo_hack : screenhacks [current_hack]);
  int i;
  for (i = 0; hack [i] != 0 && hack [i] != ' ' && hack [i] != '\t'; i++)
    chn [i] = hack [i];
  chn [i] = 0;
  return chn;
}

#ifdef SIGCHLD
static void
sigchld_handler (sig)
     int sig;
{
  if (killing)
    return;
  if (! pid)
    abort ();
  await_child_death (False);
}
#endif


void
init_sigchld ()
{
#ifdef SIGCHLD
  if (((int) signal (SIGCHLD, sigchld_handler)) == -1)
    {
      char buf [255];
      sprintf (buf, "%s: %scouldn't catch SIGCHLD", progname,
	       (verbose_p ? "## " : ""));
      perror (buf);
    }
#endif
}


extern void raise_window P((Bool inhibit_fade, Bool between_hacks_p));

void
spawn_screenhack (first_time_p)
     Bool first_time_p;
{
  raise_window (first_time_p, True);
  XFlush (dpy);

  if (screenhacks_count || demo_mode_p)
    {
      char *hack;
      pid_t forked;
      char buf [255];
      int new_hack;
      if (demo_mode_p)
	{
	  hack = demo_hack;
	}
      else
	{
	  if (screenhacks_count == 1)
	    new_hack = 0;
	  else if (next_mode_p == 1)
	    new_hack = (current_hack + 1) % screenhacks_count,
	    next_mode_p = 0;
	  else if (next_mode_p == 2)
	    {
	      new_hack = ((current_hack + screenhacks_count - 1)
			  % screenhacks_count);
	      next_mode_p = 0;
	    }
	  else
	    while ((new_hack = random () % screenhacks_count) == current_hack)
	      ;
	  current_hack = new_hack;
	  hack = screenhacks[current_hack];
	}

      switch (forked = fork ())
	{
	case -1:
	  sprintf (buf, "%s: %scouldn't fork",
		   progname, (verbose_p ? "## " : ""));
	  perror (buf);
	  restore_real_vroot ();
	  exit (1);
	case 0:
	  exec_screenhack (hack); /* this does not return */
	  break;
	default:
	  pid = forked;
	  break;
	}
    }
}

void
kill_screenhack ()
{
  killing = 1;
  if (! pid)
    return;
  if (kill (pid, SIGTERM) < 0)
    {
      if (errno == ESRCH)
	{
	  /* Sometimes we don't get a SIGCHLD at all!  WTF?
	     It's a race condition.  It looks to me like what's happening is
	     something like: a subprocess dies of natural causes.  There is a
	     small window between when the process dies and when the SIGCHLD
	     is (would have been) delivered.  If we happen to try to kill()
	     the process during that time, the kill() fails, because the
	     process is already dead.  But! no SIGCHLD is delivered (perhaps
	     because the failed kill() has reset some state in the kernel?)
	     Anyway, if kill() says "No such process", then we have to wait()
	     for it anyway, because the process has already become a zombie.
	     I love Unix.
	   */
	  await_child_death (False);
	}
      else
	{
	  char buf [255];
	  sprintf (buf, "%s: %scouldn't kill child process %d", progname,
		   (verbose_p ? "## " : ""), pid);
	  perror (buf);
	}
    }
  else
    {
      if (verbose_p)
	printf ("%s: killing pid %d.\n", progname, pid);
      await_child_death (True);
    }
}


void
suspend_screenhack (suspend_p)
     Bool suspend_p;
{
  
  suspending = suspend_p;
  if (! pid)
    ;
  else if (kill (pid, (suspend_p ? SIGSTOP : SIGCONT)) < 0)
    {
      char buf [255];
      sprintf (buf, "%s: %scouldn't %s child process %d", progname,
	       (verbose_p ? "## " : ""),
	       (suspend_p ? "suspend" : "resume"),
	       pid);
      perror (buf);
    }
  else if (verbose_p)
    printf ("%s: %s pid %d.\n", progname,
	    (suspend_p ? "suspending" : "resuming"), pid);
}


/* Restarting the xscreensaver process from scratch. */

static char **saved_argv;

void
save_argv (argc, argv)
     int argc;
     char **argv;
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
restart_process ()
{
  XCloseDisplay (dpy);
  fflush (stdout);
  fflush (stderr);
  execvp (saved_argv [0], saved_argv);
  fprintf (stderr, "%s: %scould not restart process: %s (%d)\n",
	   progname, (verbose_p ? "## " : ""),
	   (errno == E2BIG ? "arglist too big" :
	    errno == EACCES ? "could not execute" :
	    errno == EFAULT ? "memory fault" :
	    errno == EIO ? "I/O error" :
	    errno == ENAMETOOLONG ? "name too long" :
	    errno == ELOOP ? "too many symbolic links" :
	    errno == ENOENT ? "file no longer exists" :
	    errno == ENOTDIR ? "directory no longer exists" :
	    errno == ENOEXEC ? "bad executable file" :
	    errno == ENOMEM ? "out of memory" :
	    "execvp() returned unknown error code"),
	   errno);
  exit (1);
}

void
demo_mode_restart_process ()
{
  int i;
  for (i = 0; saved_argv [i]; i++);
  /* add the -demo switch; save_argv() left room for this. */
  saved_argv [i] = "-demo";
  saved_argv [i+1] = 0;
  restart_process ();
}

void
hack_environment ()
{
  /* Store $DISPLAY into the environment, so that the $DISPLAY variable that
     the spawned processes inherit is the same as the value of -display passed
     in on our command line (which is not necessarily the same as what our
     $DISPLAY variable is.)
   */
  char *s, buf [2048];
  int i;
  sprintf (buf, "DISPLAY=%s", DisplayString (dpy));
  i = strlen (buf);
  s = (char *) malloc (i+1);
  strncpy (s, buf, i+1);
  if (putenv (s))
    abort ();
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
hack_uid ()
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
      struct passwd *p = getpwnam ("nobody");
      locking_disabled_p = True;
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
	  sprintf (hack_uid_error, "changing uid/gid to %s/%s (%d/%d).",
		   p->pw_name, (g ? g->gr_name : "???"), p->pw_uid, p->pw_gid);

	  /* Change the gid to be a safe one.  If we can't do that, then
	     print a warning.  We change the gid before the uid so that we
	     change the gid while still root. */
	  if (setgid (p->pw_gid) != 0)
	    {
	      hack_uid_errno = errno;
	      sprintf (hack_uid_error, "couldn't set gid to %s (%d)",
		       (g ? g->gr_name : "???"), p->pw_gid);
	    }

	  /* Now change the uid to be a safe one. */
	  if (setuid (p->pw_uid) != 0)
	    {
	      hack_uid_errno = errno;
	      sprintf (hack_uid_error, "couldn't set uid to %s (%d)",
		       p->pw_name, p->pw_uid);
	    }
	}
    }
#ifndef NO_LOCKING
 else	/* disable locking if already being run as "someone else" */
   {
     struct passwd *p = getpwuid (getuid ());
     if (!p ||
	 !strcmp (p->pw_name, "root") ||
	 !strcmp (p->pw_name, "nobody") ||
	 !strcmp (p->pw_name, "daemon") ||
	 !strcmp (p->pw_name, "bin") ||
	 !strcmp (p->pw_name, "sys"))
       locking_disabled_p = True;
   }
#endif /* NO_LOCKING */
}

void
hack_uid_warn ()
{
  if (! hack_uid_error)
    ;
  else if (hack_uid_errno == 0)
    {
      if (verbose_p)
	printf ("%s: %s\n", progname, hack_uid_error);
    }
  else
    {
      char buf [255];
      sprintf (buf, "%s: %s%s", progname, (verbose_p ? "## " : ""),
	       hack_uid_error);
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
