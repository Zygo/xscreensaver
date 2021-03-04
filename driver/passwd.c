/* passwd.c --- verifying typed passwords with the OS.
 * xscreensaver, Copyright © 1993-2021 Jamie Zawinski <jwz@jwz.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <pwd.h>		/* for getpwuid() */

#ifdef HAVE_SYSLOG
# include <syslog.h>
#endif /* HAVE_SYSLOG */

#include <X11/Intrinsic.h>

#include "xscreensaver.h"
#include "auth.h"


#ifdef NO_LOCKING

Bool lock_init (void) { return 0; }
Bool lock_priv_init (void) { return 0; }
Bool xscreensaver_auth (void *closure,
                        Bool (*conv_fn) (void *closure,
                                         int nmsgs,
                                         const auth_message *msg,
                                         auth_response **resp),
                        void (*finished_fn) (void *closure, Bool status))
{
  return False;
}

#else  /* NO_LOCKING -- whole file */

extern const char *blurb(void);


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
  Bool (*init) (void);
  Bool (*priv_init) (void);
  Bool (*valid_p) (void *closure, const char *plaintext);
  Bool (*try_unlock) (void *closure,
                      Bool (*conv_fn) (void *closure,
                                       int nmsgs,
                                       const auth_message *msg,
                                       auth_response **resp));
  Bool initted_p;
  Bool priv_initted_p;
};



/* The authorization methods to try, in order of preference.
   The first that initializes successfully is used and others are ignored.
 */
struct auth_methods methods[] = {
# ifdef HAVE_PAM
  { "PAM",   0, pam_priv_init, 0, pam_try_unlock, 0, },
# endif
# ifdef HAVE_KERBEROS
  { "KRB",   kerberos_lock_init, 0, kerberos_passwd_valid_p, 0, },
# endif
# ifdef PASSWD_HELPER_PROGRAM
  { "EXT",   0, ext_priv_init, ext_passwd_valid_p, 0, },
# endif
  { "pwnam", pwent_lock_init, pwent_priv_init, pwent_passwd_valid_p, 0, }
};


Bool
lock_priv_init (void)
{
  int i;
  Bool any_ok = False;
  for (i = 0; i < countof(methods); i++)
    {
      if (!methods[i].priv_init)
        methods[i].priv_initted_p = True;
      else
        methods[i].priv_initted_p = methods[i].priv_init();

      if (methods[i].priv_initted_p)
        any_ok = True;
    }
  return any_ok;
}


Bool
lock_init (void)
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
        methods[i].initted_p = methods[i].init();

      if (methods[i].initted_p)
        any_ok = True;
      else if (verbose_p)
        fprintf (stderr, "%s: %s: passwords initialization failed\n",
                 blurb(), methods[i].name);
    }
  return any_ok;
}


/* For those auth methods that have a 'valid_p' function instead of a
   'try_unlock' function, this does a PAM-like conversation that first
   prompts for a password and then tests it with the 'valid_p' function.
 */
static Bool
try_valid_p (void *closure,
             const char *name,
             Bool (*valid_p) (void *closure, const char *typed_passwd),
             Bool (*conv_fn) (void *closure,
                              int nmsgs,
                              const auth_message *msg,
                              auth_response **resp))
{
  auth_message message;
  auth_response *response = NULL;
  Bool ok = False;

  memset (&message, 0, sizeof(message));

  if (verbose_p)
    fprintf (stderr, "%s: %s: non-PAM password auth\n", blurb(), name);

  /* Call the auth_conv function with "Password:", then feed the result
     into valid_p() */
  message.type = AUTH_MSGTYPE_PROMPT_NOECHO;
  message.msg = "Password:";

  ok = conv_fn (closure, 1, &message, &response);
  if (!response || !response->response)
    ok = False;

  if (ok)
    ok = valid_p (closure, response->response);

  if (response)
    {
      if (response->response)
        free (response->response);
      free (response);
    }

  return ok;
}


/* Write a password failure to the system log.
 */
static void
do_syslog (void)
{
# ifdef HAVE_SYSLOG
  struct passwd *pw = getpwuid (getuid ());
  char *d = getenv ("DISPLAY");
  char *u = (pw && pw->pw_name ? pw->pw_name : "???");
  int opt = 0;
  int fac = 0;
  int pri = LOG_NOTICE;

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

  openlog (progname, opt, fac);
  syslog (pri, "Failed login on display \"%s\" for \"%s\"", d, u);
  closelog ();

# endif /* HAVE_SYSLOG */
}


/* Runs through each authentication driver calling its try_unlock function.
 */
Bool
xscreensaver_auth (void *closure,
                   Bool (*conv_fn) (void *closure,
                                    int nmsgs,
                                    const auth_message *msg,
                                    auth_response **resp),
                   void (*finished_fn) (void *closure, Bool status))
{
  int i;
  Bool ok = False;

  for (i = 0; i < countof(methods); i++)
    {
      if (!methods[i].initted_p)
        continue;

      if (methods[i].try_unlock)
        ok = methods[i].try_unlock (closure, conv_fn);
      else if (methods[i].valid_p)
        ok = try_valid_p (closure, methods[i].name, methods[i].valid_p,
                          conv_fn);
      else
        abort();  /* method must have one or the other function */

      /* Only try the first method that initialized properly.  That means that
         if PAM initialized correctly, we will never try pwent or Kerberos.
         If we did, then typing an incorrect password at PAM would a second
         password prompt that would only go to pwent.  There's no easy way to
         re-use the password typed the first time. */
      break;
    }

  if (!ok)
    do_syslog ();

  if (finished_fn)
    finished_fn (closure, ok);

  return ok;
}

#endif /* NO_LOCKING -- whole file */
