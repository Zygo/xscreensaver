/* exec.c --- executes a program in *this* pid, without an intervening process.
 * xscreensaver, Copyright (c) 1991-2008 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */


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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>

#ifndef ESRCH
# include <errno.h>
#endif

#if defined(HAVE_SETPRIORITY) && defined(PRIO_PROCESS)
# include <sys/resource.h>	/* for setpriority() and PRIO_PROCESS */
				/* and also setrlimit() and RLIMIT_AS */
#endif

#ifdef VMS
# include <processes.h>
# include <unixio.h>		/* for close */
# include <unixlib.h>		/* for getpid */
# define pid_t int
# define fork  vfork
#endif /* VMS */

#include "exec.h"

extern const char *blurb (void);

static void nice_process (int nice_level);


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

  execvp (av[0], av);	/* shouldn't return. */
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

  execvp (av[0], av);	/* shouldn't return. */
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


void
exec_command (const char *shell, const char *command, int nice_level)
{
  int hairy_p;

#ifndef VMS
  nice_process (nice_level);

  hairy_p = !!strpbrk (command, "*?$&!<>[];`'\\\"=");
  /* note: = is in the above because of the sh syntax "FOO=bar cmd". */

  if (getuid() == (uid_t) 0 || geteuid() == (uid_t) 0)
    {
      /* If you're thinking of commenting this out, think again.
         If you do so, you will open a security hole.  Mail jwz
         so that he may enlighten you as to the error of your ways.
       */
      fprintf (stderr, "%s: we're still running as root!  Disaster!\n",
               blurb());
      exit (-1);
    }

  if (hairy_p)
    /* If it contains any shell metacharacters, do it the hard way,
       and fork a shell to parse the arguments for us. */
    exec_complex_command (shell, command);
  else
    /* Otherwise, we can just exec the program directly. */
    exec_simple_command (command);

#else  /* VMS */
  exec_vms_command (command);
#endif /* VMS */
}


/* Setting process priority
 */

static void
nice_process (int nice_level)
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


/* Whether the given command exists on $PATH.
   (Anything before the first space is considered to be the program name.)
 */
int
on_path_p (const char *program)
{
  int result = 0;
  struct stat st;
  char *cmd = strdup (program);
  char *token = strchr (cmd, ' ');
  char *path = 0;
  int L;

  if (token) *token = 0;
  token = 0;

  if (strchr (cmd, '/'))
    {
      result = (0 == stat (cmd, &st));
      goto DONE;
    }

  path = getenv("PATH");
  if (!path || !*path)
    goto DONE;

  L = strlen (cmd);
  path = strdup (path);
  token = strtok (path, ":");

  while (token)
    {
      char *p2 = (char *) malloc (strlen (token) + L + 3);
      strcpy (p2, token);
      strcat (p2, "/");
      strcat (p2, cmd);
      result = (0 == stat (p2, &st));
      if (result)
        goto DONE;
      token = strtok (0, ":");
    }

 DONE:
  free (cmd);
  if (path) free (path);
  return result;
}

