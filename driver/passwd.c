/* passwd.c --- verifying typed passwords with the OS.
 * xscreensaver, Copyright (c) 1993-2002 Jamie Zawinski <jwz@jwz.org>
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
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

extern char *blurb(void);
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
  Bool initted_p;
  Bool priv_initted_p;
};


#ifdef HAVE_KERBEROS
extern Bool kerberos_lock_init (int argc, char **argv, Bool verbose_p);
extern Bool kerberos_passwd_valid_p (const char *typed_passwd, Bool verbose_p);
#endif
#ifdef HAVE_PAM
extern Bool pam_priv_init (int argc, char **argv, Bool verbose_p);
extern Bool pam_passwd_valid_p (const char *typed_passwd, Bool verbose_p);
#endif
extern Bool pwent_lock_init (int argc, char **argv, Bool verbose_p);
extern Bool pwent_priv_init (int argc, char **argv, Bool verbose_p);
extern Bool pwent_passwd_valid_p (const char *typed_passwd, Bool verbose_p);


/* The authorization methods to try, in order.
   Note that the last one (the pwent version) is actually two auth methods,
   since that code tries shadow passwords, and then non-shadow passwords.
   (It's all in the same file since the APIs are randomly nearly-identical.)
 */
struct auth_methods methods[] = {
# ifdef HAVE_KERBEROS
  { "Kerberos",         kerberos_lock_init, 0, kerberos_passwd_valid_p,
                        False, False },
# endif
# ifdef HAVE_PAM
  { "PAM",              0, pam_priv_init, pam_passwd_valid_p, 
                        False, False },
# endif
  { "normal",           pwent_lock_init, pwent_priv_init, pwent_passwd_valid_p,
                        False, False }
};


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


Bool 
passwd_valid_p (const char *typed_passwd, Bool verbose_p)
{
  int i, j;
  for (i = 0; i < countof(methods); i++)
    {
      int ok_p = (methods[i].initted_p &&
                  methods[i].valid_p (typed_passwd, verbose_p));

      check_for_leaks (methods[i].name);

      if (ok_p)
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
                           "%s: authentication via %s passwords failed.\n",
                           blurb(), methods[j].name);
              fprintf (stderr,
                       "%s: but authentication via %s passwords succeeded.\n",
                       blurb(), methods[i].name);
            }

          return True;		/* Successfully authenticated! */
        }
    }

  return False;			/* Authentication failure. */
}

#endif /* NO_LOCKING -- whole file */
