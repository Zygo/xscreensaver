/* passwd.c --- verifying typed passwords with the OS.
 * xscreensaver, Copyright (c) 1993-2019 Jamie Zawinski <jwz@jwz.org>
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

#ifndef NO_LOCKING  /* whole file */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>

#ifndef VMS
# include <pwd.h>		/* for getpwuid() */
#else /* VMS */
# include "vms-pwd.h"
#endif /* VMS */

#ifdef HAVE_SYSLOG
# include <syslog.h>
#endif /* HAVE_SYSLOG */

#include <X11/Intrinsic.h>

#include "xscreensaver.h"
#include "auth.h"

extern const char *blurb(void);
extern void check_for_leaks (const char *where);


/* blargh */
#undef  Bool
#undef  True
#undef  False
#define Bool  int
#define True  1
#define False 0

#undef countof
#define countof(x) (sizeof((x))/sizeof(*(x)))

struct auth_methods {
  const char *name;
  Bool (*init) (int argc, char **argv, Bool verbose_p);
  Bool (*priv_init) (int argc, char **argv, Bool verbose_p);
  Bool (*valid_p) (const char *typed_passwd, Bool verbose_p);
  void (*try_unlock) (saver_info *si, Bool verbose_p,
		      Bool (*valid_p)(const char *typed_passwd, Bool verbose_p));
  Bool initted_p;
  Bool priv_initted_p;
};


#ifdef HAVE_KERBEROS
extern Bool kerberos_lock_init (int argc, char **argv, Bool verbose_p);
extern Bool kerberos_passwd_valid_p (const char *typed_passwd, Bool verbose_p);
#endif
#ifdef HAVE_PAM
extern Bool pam_priv_init (int argc, char **argv, Bool verbose_p);
extern void pam_try_unlock (saver_info *si, Bool verbose_p,
			    Bool (*valid_p)(const char *typed_passwd, Bool verbose_p));
#endif
#ifdef PASSWD_HELPER_PROGRAM
extern Bool ext_priv_init (int argc, char **argv, Bool verbose_p);
extern Bool ext_passwd_valid_p (const char *typed_passwd, Bool verbose_p);
#endif
extern Bool pwent_lock_init (int argc, char **argv, Bool verbose_p);
extern Bool pwent_priv_init (int argc, char **argv, Bool verbose_p);
extern Bool pwent_passwd_valid_p (const char *typed_passwd, Bool verbose_p);

Bool lock_priv_init (int argc, char **argv, Bool verbose_p);
Bool lock_init (int argc, char **argv, Bool verbose_p);
Bool passwd_valid_p (const char *typed_passwd, Bool verbose_p);

/* The authorization methods to try, in order.
   Note that the last one (the pwent version) is actually two auth methods,
   since that code tries shadow passwords, and then non-shadow passwords.
   (It's all in the same file since the APIs are randomly nearly-identical.)
 */
struct auth_methods methods[] = {
# ifdef HAVE_PAM
  { "PAM",              0, pam_priv_init, 0, pam_try_unlock,
                        False, False },
# endif
# ifdef HAVE_KERBEROS
  { "Kerberos",         kerberos_lock_init, 0, kerberos_passwd_valid_p, 0,
                        False, False },
# endif
# ifdef PASSWD_HELPER_PROGRAM
  { "external",		0, ext_priv_init, ext_passwd_valid_p, 0,
  			False, False },
# endif
  { "normal",           pwent_lock_init, pwent_priv_init, pwent_passwd_valid_p, 0,
                        False, False }
};


# ifdef HAVE_PROC_OOM
/* On some recent Linux systems you can tell the kernel's OOM-killer to
   consider the possibility of maybe sometimes not killing you in low-memory
   situations. Because that would unlock the screen. And that would be bad.

   Linux >= 2.6.11: echo -17   > /proc/$$/oom_adj	  <-- ignoring this.
   Linux >= 2.6.37: echo -1000 > /proc/$$/oom_score_adj	  <-- trying this.
 */
static void
oom_assassin_immunity (Bool verbose_p)
{
  char fn[1024];
  struct stat st;
  FILE *out;
  sprintf (fn, "/proc/%d/oom_score_adj", getpid());
  if (stat(fn, &st) != 0)
    {
      if (verbose_p)
        fprintf (stderr, "%s: OOM: %s does not exist\n", blurb(), fn);
      return;
    }
  out = fopen (fn, "w");
  if (!out)
    {
      if (verbose_p)
        {
          char b[2048];
          sprintf (b, "%s: OOM: unable to write %s\n", blurb(), fn);
          perror(b);
        }
      return;
    }
  fputs ("-1000\n", out);
  fclose (out);
}
# endif /* HAVE_PROC_OOM */


Bool
lock_priv_init (int argc, char **argv, Bool verbose_p)
{
  int i;
  Bool any_ok = False;
  for (i = 0; i < countof(methods); i++)
    {
      if (!methods[i].priv_init)
        methods[i].priv_initted_p = True;
      else
        methods[i].priv_initted_p = methods[i].priv_init (argc, argv,
                                                          verbose_p);

      if (methods[i].priv_initted_p)
        any_ok = True;
      else if (verbose_p)
        fprintf (stderr, "%s: initialization of %s passwords failed.\n",
                 blurb(), methods[i].name);
    }

# ifdef HAVE_PROC_OOM
  oom_assassin_immunity (verbose_p);
# endif

  return any_ok;
}


Bool
lock_init (int argc, char **argv, Bool verbose_p)
{
  int i;
  Bool any_ok = False;
  for (i = 0; i < countof(methods); i++)
    {
      if (!methods[i].priv_initted_p)	/* Bail if lock_priv_init failed. */
        continue;

      if (!methods[i].init)
        methods[i].initted_p = True;
      else
        methods[i].initted_p = methods[i].init (argc, argv, verbose_p);

      if (methods[i].initted_p)
        any_ok = True;
      else if (verbose_p)
        fprintf (stderr, "%s: initialization of %s passwords failed.\n",
                 blurb(), methods[i].name);
    }
  return any_ok;
}


/* A basic auth driver that simply prompts for a password then runs it through
 * valid_p to determine whether the password is correct.
 */
static void
try_unlock_password(saver_info *si,
		   Bool verbose_p,
		   Bool (*valid_p)(const char *typed_passwd, Bool verbose_p))
{
  struct auth_message message;
  struct auth_response *response = NULL;

  memset(&message, 0, sizeof(message));

  if (verbose_p)
    fprintf(stderr, "%s: non-PAM password auth.\n", blurb());

  /* Call the auth_conv function with "Password:", then feed
   * the result into valid_p()
   */
  message.type = AUTH_MSGTYPE_PROMPT_NOECHO;
  message.msg = "Password:";

  si->unlock_cb(1, &message, &response, si);

  if (!response)
    return;

  if (valid_p (response->response, verbose_p))
    si->unlock_state = ul_success;	       /* yay */
  else if (si->unlock_state == ul_cancel ||
           si->unlock_state == ul_time)
    ;					       /* more specific failures ok */
  else
    si->unlock_state = ul_fail;		       /* generic failure */

  if (response->response)
    free(response->response);
  free(response);
}


/* Write a password failure to the system log.
 */
static void
do_syslog (saver_info *si, Bool verbose_p)
{
# ifdef HAVE_SYSLOG
  struct passwd *pw = getpwuid (getuid ());
  char *d = (si->dpy ? DisplayString (si->dpy) : 0);
  char *u = (pw && pw->pw_name ? pw->pw_name : "???");
  int opt = 0;
  int fac = 0;

#  ifdef LOG_PID
  opt = LOG_PID;
#  endif

#  if defined(LOG_AUTHPRIV)
  fac = LOG_AUTHPRIV;
#  elif defined(LOG_AUTH)
  fac = LOG_AUTH;
#  else
  fac = LOG_DAEMON;
#  endif

  if (!d) d = "";

#  undef FMT
#  define FMT "FAILED LOGIN %d ON DISPLAY \"%s\", FOR \"%s\""

  if (verbose_p)
    fprintf (stderr, "%s: syslog: " FMT "\n", blurb(), 
             si->unlock_failures, d, u);

  openlog (progname, opt, fac);
  syslog (LOG_NOTICE, FMT, si->unlock_failures, d, u);
  closelog ();

# endif /* HAVE_SYSLOG */
}



/**
 * Runs through each authentication driver calling its try_unlock function.
 * Called xss_authenticate() because AIX beat us to the name authenticate().
 */
void
xss_authenticate(saver_info *si, Bool verbose_p)
{
  int i, j;

  si->unlock_state = ul_read;

  for (i = 0; i < countof(methods); i++)
    {
      if (!methods[i].initted_p)
        continue;

      if (si->cached_passwd != NULL && methods[i].valid_p)
	si->unlock_state = (methods[i].valid_p(si->cached_passwd, verbose_p) == True)
	                   ? ul_success : ul_fail;
      else if (methods[i].try_unlock != NULL)
        methods[i].try_unlock(si, verbose_p, methods[i].valid_p);
      else if (methods[i].valid_p)
        try_unlock_password(si, verbose_p, methods[i].valid_p);
      else /* Ze goggles, zey do nozing! */
        fprintf(stderr, "%s: authentication method %s does nothing.\n",
                blurb(), methods[i].name);

      check_for_leaks (methods[i].name);

      /* If password authentication failed, but the password was NULL
         (meaning the user just hit RET) then treat that as "cancel".
         This means that if the password is literally NULL, it will
         work; but if not, then NULL passwords are treated as cancel.
       */
      if (si->unlock_state == ul_fail &&
          si->cached_passwd &&
          !*si->cached_passwd)
        {
          if (verbose_p)
            fprintf (stderr, "%s: assuming null password means cancel.\n",
                     blurb());
          si->unlock_state = ul_cancel;
        }

      if (si->unlock_state == ul_success)
        {
          /* If we successfully authenticated by method N, but attempting
             to authenticate by method N-1 failed, mention that (since if
             an earlier authentication method fails and a later one succeeds,
             something screwy is probably going on.)
           */
          if (verbose_p && i > 0)
            {
              for (j = 0; j < i; j++)
                if (methods[j].initted_p)
                    fprintf (stderr,
                             "%s: authentication via %s failed.\n",
                             blurb(), methods[j].name);
              fprintf (stderr,
                       "%s: authentication via %s succeeded.\n",
                       blurb(), methods[i].name);
            }
          goto DONE;		/* Successfully authenticated! */
        }
      else if (si->unlock_state == ul_cancel ||
               si->unlock_state == ul_time)
        {
          /* If any auth method gets a cancel or timeout, don't try the
             next auth method!  We're done! */
          if (verbose_p)
            fprintf (stderr, "%s: authentication via %s %s.\n",
                     blurb(), methods[i].name,
                     (si->unlock_state == ul_cancel
                      ? "cancelled" : "timed out"));
          goto DONE;
        }
    }

  if (verbose_p)
    fprintf(stderr, "%s: All authentication mechanisms failed.\n", blurb());

  if (si->unlock_state == ul_fail)
    {
      /* Note the time of the first failure */
      if (si->unlock_failures == 0)
        si->unlock_failure_time = time((time_t *) 0);
      si->unlock_failures++;
      do_syslog (si, verbose_p);
    }

DONE:
  if (si->auth_finished_cb)
    si->auth_finished_cb (si);
}

#endif /* NO_LOCKING -- whole file */
