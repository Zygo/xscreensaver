/* passwd-pwent.c --- verifying typed passwords with the OS.
 * xscreensaver, Copyright (c) 1993-1998 Jamie Zawinski <jwz@jwz.org>
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
#ifndef VMS
# include <pwd.h>
# include <grp.h>
#else /* VMS */
# include "vms-pwd.h"
#endif /* VMS */


#ifdef __bsdi__
# include <sys/param.h>
# if _BSDI_VERSION >= 199608
#  define BSD_AUTH
# endif
#endif /* __bsdi__ */


#if defined(HAVE_SHADOW_PASSWD)	      /* passwds live in /etc/shadow */

#   include <shadow.h>
#   define PWTYPE   struct spwd *
#   define PWPSLOT  sp_pwdp
#   define GETPW    getspnam

#elif defined(HAVE_ENHANCED_PASSWD)      /* passwds live in /tcb/files/auth/ */
				      /* M.Matsumoto <matsu@yao.sharp.co.jp> */
#   include <sys/security.h>
#   include <prot.h>

#   define PWTYPE   struct pr_passwd *
#   define PWPSLOT  ufld.fd_encrypt
#   define GETPW    getprpwnam

#elif defined(HAVE_ADJUNCT_PASSWD)

#   include <sys/label.h>
#   include <sys/audit.h>
#   include <pwdadj.h>

#   define PWTYPE   struct passwd_adjunct *
#   define PWPSLOT  pwa_passwd
#   define GETPW    getpwanam

#elif defined(HAVE_HPUX_PASSWD)

#   include <hpsecurity.h>
#   include <prot.h>

#   define PWTYPE   struct s_passwd *
#   define PWPSLOT  pw_passwd
#   define GETPW    getspwnam

#   define HAVE_BIGCRYPT

#endif


/* blargh */
#undef  Bool
#undef  True
#undef  False
#define Bool  int
#define True  1
#define False 0


extern const char *blurb(void);

static char *encrypted_root_passwd = 0;
static char *encrypted_user_passwd = 0;

#ifdef VMS
# define ROOT "SYSTEM"
#else
# define ROOT "root"
#endif

#ifndef VMS
Bool pwent_priv_init (int argc, char **argv, Bool verbose_p);
Bool pwent_lock_init (int argc, char **argv, Bool verbose_p);
Bool pwent_passwd_valid_p (const char *typed_passwd, Bool verbose_p);
#endif


#ifndef VMS

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

#else  /* VMS */

static char *
user_name (void)
{
  char *u = getenv("USER");
  return (u ? strdup(u) : 0);
}

#endif /* VMS */


static Bool
passwd_known_p (const char *pw)
{
  return (pw &&
	  pw[0] != '*' &&	/* This would be sensible...         */
	  strlen(pw) > 4);	/* ...but this is what Solaris does. */
}


static char *
get_encrypted_passwd(const char *user)
{
  char *result = 0;

#ifdef PWTYPE
  if (user && *user && !result)
    {					/* First check the shadow passwords. */
      PWTYPE p = GETPW((char *) user);
      if (p && passwd_known_p (p->PWPSLOT))
	result = strdup(p->PWPSLOT);
    }
#endif /* PWTYPE */

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

#ifndef HAVE_PAM
  /* We only issue this warning if not compiled with support for PAM.
     If we're using PAM, it's not unheard of that normal pwent passwords
     would be unavailable. */

  if (!result)
    fprintf (stderr, "%s: couldn't get password of \"%s\"\n",
	     blurb(), (user ? user : "(null)"));
#endif /* !HAVE_PAM */

  return result;
}



/* This has to be called before we've changed our effective user ID,
   because it might need privileges to get at the encrypted passwords.
   Returns false if we weren't able to get any passwords, and therefore,
   locking isn't possible.  (It will also have written to stderr.)
 */

#ifndef VMS

Bool
pwent_priv_init (int argc, char **argv, Bool verbose_p)
{
  char *u;

#ifdef HAVE_ENHANCED_PASSWD
  set_auth_parameters(argc, argv);
  check_auth_parameters();
#endif /* HAVE_DEC_ENHANCED */

  u = user_name();
  encrypted_user_passwd = get_encrypted_passwd(u);
  encrypted_root_passwd = get_encrypted_passwd(ROOT);
  if (u) free (u);

  if (encrypted_user_passwd)
    return True;
  else
    return False;
}


Bool
pwent_lock_init (int argc, char **argv, Bool verbose_p)
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
pwent_passwd_valid_p (const char *typed_passwd, Bool verbose_p)
{
  if (encrypted_user_passwd &&
      passwds_match_p (typed_passwd, encrypted_user_passwd))
    return True;

  /* do not allow root to have a null password. */
  else if (typed_passwd[0] &&
	   encrypted_root_passwd &&
           passwds_match_p (typed_passwd, encrypted_root_passwd))
    return True;

  else
    return False;
}

#else  /* VMS */
Bool pwent_lock_init (int argc, char **argv, Bool verbose_p) { return True; }
#endif /* VMS */

#endif /* NO_LOCKING -- whole file */
