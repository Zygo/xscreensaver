/* passwd.c --- verifying typed passwords with the OS.
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
#   define crypt    bigcrypt

#endif


/* blargh */
#undef  Bool
#undef  True
#undef  False
#define Bool  int
#define True  1
#define False 0


extern char *progname;

static char *encrypted_root_passwd = 0;
static char *encrypted_user_passwd = 0;

#ifdef VMS
# define ROOT "SYSTEM"
#else
# define ROOT "root"
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
  if (user && *user)
    {
#ifdef PWTYPE
      {					/* First check the shadow passwords. */
	PWTYPE p = GETPW((char *) user);
	if (p && passwd_known_p (p->PWPSLOT))
	  return strdup(p->PWPSLOT);
      }
#endif
      {					/* Check non-shadow passwords too. */
	struct passwd *p = getpwnam(user);
	if (p && passwd_known_p (p->pw_passwd))
	  return strdup(p->pw_passwd);
      }
    }

  fprintf (stderr, "%s: couldn't get password of \"%s\"\n",
	   blurb(), (user ? user : "(null)"));

  return 0;
}



/* This has to be called before we've changed our effective user ID,
   because it might need priveleges to get at the encrypted passwords.
   Returns false if we weren't able to get any passwords, and therefore,
   locking isn't possible.  (It will also have written to stderr.)
 */

#ifndef VMS

Bool
lock_init (int argc, char **argv)
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


/* This can be called at any time, and says whether the typed password
   belongs to either the logged in user (real uid, not effective); or
   to root.
 */
Bool
passwd_valid_p (const char *typed_passwd)
{
  char *s = 0;  /* note that on some systems, crypt() may return null */

  if (encrypted_user_passwd &&
      (s = (char *) crypt (typed_passwd, encrypted_user_passwd)) &&
      !strcmp (s, encrypted_user_passwd))
    return True;

  /* do not allow root to have a null password. */
  else if (typed_passwd[0] &&
	   encrypted_root_passwd &&
	   (s = (char *) crypt (typed_passwd, encrypted_root_passwd)) &&
	   !strcmp (s, encrypted_root_passwd))
    return True;

  else
    return False;
}

#else  /* VMS */
Bool lock_init (int argc, char **argv) { return True; }
#endif /* VMS */

#endif /* NO_LOCKING -- whole file */
