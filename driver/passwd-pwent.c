/* passwd-pwent.c --- verifying typed passwords with the OS.
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

#ifndef NO_LOCKING  /* whole file */

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_CRYPT_H
# include <crypt.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#ifdef __bsdi__
# include <sys/param.h>
# if _BSDI_VERSION >= 199608
#  define BSD_AUTH
# endif
#endif /* __bsdi__ */


#if defined(HAVE_SHADOW_PASSWD)	      /* passwds live in /etc/shadow */

#   include <shadow.h>
#   define PWNAME   "spnam"
#   define PWTYPE   struct spwd *
#   define PWPSLOT  sp_pwdp
#   define GETPW    getspnam

#elif defined(HAVE_ENHANCED_PASSWD)      /* passwds live in /tcb/files/auth/ */
				      /* M.Matsumoto <matsu@yao.sharp.co.jp> */
#   include <sys/security.h>
#   include <prot.h>

#   define PWNAME   "prpwnam"
#   define PWTYPE   struct pr_passwd *
#   define PWPSLOT  ufld.fd_encrypt
#   define GETPW    getprpwnam

#elif defined(HAVE_ADJUNCT_PASSWD)

#   include <sys/label.h>
#   include <sys/audit.h>
#   include <pwdadj.h>

#   define PWNAME   "pwanam"
#   define PWTYPE   struct passwd_adjunct *
#   define PWPSLOT  pwa_passwd
#   define GETPW    getpwanam

#elif defined(HAVE_HPUX_PASSWD)

#   include <hpsecurity.h>
#   include <prot.h>

#   define PWNAME   "spwnam"
#   define PWTYPE   struct s_passwd *
#   define PWPSLOT  pw_passwd
#   define GETPW    getspwnam

#   define HAVE_BIGCRYPT

#elif defined(HAVE_PWNAM_SHADOW_PASSWD)

#   define PWNAME   "pwnam_shadow"
#   define PWTYPE   struct passwd *
#   define PWPSLOT  pw_passwd
#   define GETPW    getpwnam_shadow

#endif

#include "blurb.h"
#include "auth.h"


#ifdef ALLOW_ROOT_PASSWD
static char *encrypted_root_passwd = 0;
#endif
static char *encrypted_user_passwd = 0;

#define ROOT "root"


static char *
user_name (void)
{
  /* I think that just checking $USER here is not the best idea. */

  const char *u = 0;

  /* It has been reported that getlogin() returns the wrong user id on some
     very old SGI systems...  And I've seen it return the string "rlogin"
     sometimes!  Screw it, using getpwuid() should be enough...
   */
/* u = (char *) getlogin ();
 */

  /* getlogin() fails if not attached to a terminal; in that case, use
     getpwuid().  (Note that in this case, we're not doing shadow stuff, since
     all we're interested in is the name, not the password.  So that should
     still work.  Right?) */
  if (!u || !*u)
    {
      struct passwd *p = getpwuid (getuid ());
      u = (p ? p->pw_name : 0);
    }

  return (u ? strdup(u) : 0);
}


static Bool
passwd_known_p (const char *pw)
{
  return (pw &&
	  pw[0] != '*' &&	/* This would be sensible...         */
	  strlen(pw) > 4);	/* ...but this is what Solaris does. */
}


static char *
get_encrypted_passwd (const char *user)
{
  char *result = 0;
  const char *pwtype = "pwnam";

# ifdef PWTYPE
  if (user && *user && !result)
    {					/* First check the shadow passwords. */
      PWTYPE p = GETPW((char *) user);
      if (p && passwd_known_p (p->PWPSLOT))
        {
          result = strdup(p->PWPSLOT);
          pwtype = PWNAME;
        }
    }
# endif /* PWTYPE */

  if (user && *user && !result)
    {					/* Check non-shadow passwords too. */
      struct passwd *p = getpwnam(user);
      if (p && passwd_known_p (p->pw_passwd))
        result = strdup(p->pw_passwd);
    }

  /* The manual for passwd(4) on HPUX 10.10 says:

	  Password aging is put in effect for a particular user if his
	  encrypted password in the password file is followed by a comma and
	  a nonnull string of characters from the above alphabet.  This
	  string defines the "age" needed to implement password aging.

     So this means that passwd->pw_passwd isn't simply a string of cyphertext,
     it might have trailing junk.  So, if there is a comma in the string, and
     that comma is beyond position 13, terminate the string before the comma.
   */
  if (result && strlen(result) > 13)
    {
      char *s = strchr (result+13, ',');
      if (s)
	*s = 0;
    }

  /* We only issue this warning in non-verbose mode if not compiled with
     support for PAM.  If we're using PAM, it's common for pwent passwords
     to be unavailable. */

  if (!result &&
      (verbose_p
# ifdef HAVE_PAM
       || 0
# else
       || 1
# endif
       ))
    fprintf (stderr, "%s: %s: couldn't get password of \"%s\"\n",
	     blurb(), pwtype, (user ? user : "(null)"));

  return result;
}



/* This has to be called before we've changed our effective user ID,
   because it might need privileges to get at the encrypted passwords.
   Returns false if we weren't able to get any passwords, and therefore,
   locking isn't possible.  (It will also have written to stderr.)
 */

Bool
pwent_priv_init (void)
{
  char *u;

#ifdef HAVE_ENHANCED_PASSWD
  set_auth_parameters(argc, argv);
  check_auth_parameters();
#endif /* HAVE_DEC_ENHANCED */

  u = user_name();
  encrypted_user_passwd = get_encrypted_passwd(u);
#ifdef ALLOW_ROOT_PASSWD
  encrypted_root_passwd = get_encrypted_passwd(ROOT);
#endif
  if (u) free (u);

  if (encrypted_user_passwd)
    return True;
  else
    return False;
}


Bool
pwent_lock_init (void)
{
  if (encrypted_user_passwd)
    return True;
  else
    return False;
}



static Bool
passwds_match_p (const char *cleartext, const char *ciphertext)
{
  char *s = 0;  /* note that on some systems, crypt() may return null */

  s = (char *) crypt (cleartext, ciphertext);
  if (s && !strcmp (s, ciphertext))
    return True;

#ifdef HAVE_BIGCRYPT
  /* There seems to be no way to tell at runtime if an HP machine is in
     "trusted" mode, and thereby, which of crypt() or bigcrypt() we should
     be calling to compare passwords.  So call them both, and see which
     one works. */

  s = (char *) bigcrypt (cleartext, ciphertext);
  if (s && !strcmp (s, ciphertext))
    return True;

#endif /* HAVE_BIGCRYPT */
  
  return False;
}



/* This can be called at any time, and says whether the typed password
   belongs to either the logged in user (real uid, not effective); or
   to root.
 */
Bool
pwent_passwd_valid_p (void *closure, const char *typed_passwd)
{
  if (encrypted_user_passwd &&
      passwds_match_p (typed_passwd, encrypted_user_passwd))
    return True;

#ifdef ALLOW_ROOT_PASSWD
  /* do not allow root to have a null password. */
  else if (typed_passwd[0] &&
	   encrypted_root_passwd &&
           passwds_match_p (typed_passwd, encrypted_root_passwd))
    return True;
#endif /* ALLOW_ROOT_PASSWD */

  else
    return False;
}

#else  /* NO_LOCKING */
int _ignore_;
#endif /* NO_LOCKING -- whole file */
