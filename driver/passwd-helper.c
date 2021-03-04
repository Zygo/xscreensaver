/* passwd-helper.c --- verifying typed passwords with external helper program
 * xscreensaver, Copyright © 1993-2021 Jamie Zawinski <jwz@jwz.org>
 * written by Olaf Kirch <okir@suse.de>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */


/*****************************************************************************

    I strongly suspect that this code has not been used in decades, and I
    am considering removing it.  These details should be hidden behind PAM.
    If you are using this code, email me and tell me why.  -- jwz, Feb 2021

 *****************************************************************************/

#error "email jwz@jwz.org about passwd-helper.c"


/* The idea here is to be able to run xscreensaver without any setuid bits.
 * Password verification happens through an external program that you feed
 * your password to on stdin.  The external command is invoked with a user
 * name argument.
 *
 * The external helper does whatever authentication is necessary.  Currently,
 * SuSE uses "unix2_chkpwd", which is a variation of "unix_chkpwd" from the
 * PAM distribution.
 *
 * Normally, the password helper should just authenticate the calling user
 * (i.e. based on the caller's real uid).  This is in order to prevent
 * brute-forcing passwords in a shadow environment.  A less restrictive
 * approach would be to allow verifying other passwords as well, but always
 * with a 2 second delay or so.  (Not sure what SuSE's "unix2_chkpwd"
 * currently does.)
 *                         -- Olaf Kirch <okir@suse.de>, 16-Dec-2003
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef NO_LOCKING  /* whole file */

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>
#include <sys/wait.h>

#include "blurb.h"
#include "auth.h"


static int
ext_run (const char *user, const char *typed_passwd)
{
  int pfd[2], status;
  pid_t pid;

  if (pipe(pfd) < 0)
    return 0;

  if (verbose_p)
    fprintf (stderr, "%s: EXT: %s\n", blurb(), PASSWD_HELPER_PROGRAM);

  if ((pid = fork()) < 0) {
    close(pfd[0]);
    close(pfd[1]);
    return 0;
  }

  if (pid == 0) {
    close(pfd[1]);
    if (pfd[0] != 0)
      dup2(pfd[0], 0);

    /* Helper is invoked as helper service-name [user] */
    execlp(PASSWD_HELPER_PROGRAM, PASSWD_HELPER_PROGRAM, "xscreensaver", user, NULL);
    if (verbose_p)
      fprintf(stderr, "%s: EXT: %s\n", PASSWD_HELPER_PROGRAM,
      		strerror(errno));
    exit(1);
  }

  close(pfd[0]);

  /* Write out password to helper process */
  if (!typed_passwd)
    typed_passwd = "";
  write(pfd[1], typed_passwd, strlen(typed_passwd));
  close(pfd[1]);

  while (waitpid(pid, &status, 0) < 0) {
    if (errno == EINTR)
      continue;
    if (verbose_p)
      fprintf(stderr, "%s: EXT: waitpid failed: %s\n",
		blurb(), strerror(errno));
    return 0;
  }

  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    return 0;
  return 1;
}



/* This can be called at any time, and says whether the typed password
   belongs to either the logged in user (real uid, not effective); or
   to root.
 */
int
ext_passwd_valid_p (void *closure, const char *typed_passwd)
{
  struct passwd *pw;
  int res = 0;

  if ((pw = getpwuid(getuid())) != NULL)
    res = ext_run (pw->pw_name, typed_passwd);
  endpwent();

#ifdef ALLOW_ROOT_PASSWD
  if (!res)
    res = ext_run ("root", typed_passwd);
#endif /* ALLOW_ROOT_PASSWD */

  return res;
}


Bool
ext_priv_init (void)
{
  /* Make sure the passwd helper exists */
  if (access(PASSWD_HELPER_PROGRAM, X_OK) < 0) {
    fprintf(stderr,
    		"%s: EXT: warning: %s does not exist.\n"
		"%s: EXT password authentication will not work.\n",
		blurb(), PASSWD_HELPER_PROGRAM, blurb());
    return False;
  }
  return True;
}

#endif /* NO_LOCKING -- whole file */
