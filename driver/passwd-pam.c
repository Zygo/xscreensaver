/* passwd-pam.c --- verifying typed passwords with PAM
 * (Pluggable Authentication Modules.)
 * written by Bill Nottingham <notting@redhat.com> (and jwz) for
 * xscreensaver, Copyright (c) 1993-1998, 2000 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Some PAM resources:
 *
 *    PAM home page:
 *    http://www.us.kernel.org/pub/linux/libs/pam/
 *
 *    PAM FAQ:
 *    http://www.us.kernel.org/pub/linux/libs/pam/FAQ
 *
 *    PAM Application Developers' Guide:
 *    http://www.us.kernel.org/pub/linux/libs/pam/Linux-PAM-html/pam_appl.html
 *
 *    PAM Mailing list archives:
 *    http://www.linuxhq.com/lnxlists/linux-pam/
 *
 *    Compatibility notes, especially between Linux and Solaris:
 *    http://www.contrib.andrew.cmu.edu/u/shadow/pam.html
 *
 *    The Open Group's PAM API documentation:
 *    http://www.opengroup.org/onlinepubs/8329799/pam_start.htm
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef NO_LOCKING  /* whole file */

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

extern char *blurb(void);


#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <security/pam_appl.h>

#include <sys/stat.h>


/* blargh */
#undef  Bool
#undef  True
#undef  False
#define Bool  int
#define True  1
#define False 0

#undef countof
#define countof(x) (sizeof((x))/sizeof(*(x)))

static int pam_conversation (int nmsgs,
                             const struct pam_message **msg,
                             struct pam_response **resp,
                             void *closure);

struct pam_closure {
  const char *user;
  const char *typed_passwd;
  Bool verbose_p;
};


#ifdef HAVE_PAM_FAIL_DELAY
   /* We handle delays ourself.*/
   /* Don't set this to 0 (Linux bug workaround.) */
# define PAM_NO_DELAY(pamh) pam_fail_delay ((pamh), 1)
#else  /* !HAVE_PAM_FAIL_DELAY */
# define PAM_NO_DELAY(pamh) /* */
#endif /* !HAVE_PAM_FAIL_DELAY */


/* On SunOS 5.6, and on Linux with PAM 0.64, pam_strerror() takes two args.
   On some other Linux systems with some other version of PAM (e.g.,
   whichever Debian release comes with a 2.2.5 kernel) it takes one arg.
   I can't tell which is more "recent" or "correct" behavior, so configure
   figures out which is in use for us.  Shoot me!
 */
#ifdef PAM_STRERROR_TWO_ARGS
# define PAM_STRERROR(pamh, status) pam_strerror((pamh), (status))
#else  /* !PAM_STRERROR_TWO_ARGS */
# define PAM_STRERROR(pamh, status) pam_strerror((status))
#endif /* !PAM_STRERROR_TWO_ARGS */


/* PAM sucks in that there is no way to tell whether a particular service
   is configured at all.  That is, there is no way to tell the difference
   between "authentication of the FOO service is not allowed" and "the
   user typed the wrong password."

   On RedHat 5.1 systems, if a service name is not known, it defaults to
   being not allowed (because the fallback service, /etc/pam.d/other, is
   set to `pam_deny'.)

   On Solaris 2.6 systems, unknown services default to authenticating normally.

   So, we could simply require that the person who installs xscreensaver
   set up an "xscreensaver" PAM service.  However, if we went that route,
   it would have a really awful failure mode: the failure mode would be that
   xscreensaver was willing to *lock* the screen, but would be unwilling to
   *unlock* the screen.  (With the non-PAM password code, the analagous
   situation -- security not being configured properly, for example do to the
   executable not being installed as setuid root -- the failure mode is much
   more palettable, in that xscreensaver will refuse to *lock* the screen,
   because it can know up front that there is no password that will work.)

   Another route would be to have the service name to consult be computed at
   compile-time (perhaps with a configure option.)  However, that doesn't
   really solve the problem, because it means that the same executable might
   work fine on one machine, but refuse to unlock when run on another
   machine.

   Another alternative would be to look in /etc/pam.conf or /etc/pam.d/ at
   runtime to see what services actually exist.  But I think that's no good,
   because who is to say that the PAM info is actually specified in those
   files?  Opening and reading those files is not a part of the PAM client
   API, so it's not guarenteed to work on any given system.

   An alternative I tried was to specify a list of services to try, and to
   try them all in turn ("xscreensaver", "xlock", "xdm", and "login").
   This worked, but it was slow (and I also had to do some contortions to
   work around bugs in Linux PAM 0.64-3.)

   So what we do today is, try PAM once, and if that fails, try the usual
   getpwent() method.  So if PAM doesn't work, it will at least make an
   attempt at looking up passwords in /etc/passwd or /etc/shadow instead.

   This all kind of blows.  I'm not sure what else to do.
 */


/* On SunOS 5.6, the `pam_conv.appdata_ptr' slot seems to be ignored, and
   the `closure' argument to pc.conv always comes in as random garbage.
   So we get around this by using a global variable instead.  Shoot me!

   (I've been told this is bug 4092227, and is fixed in Solaris 7.)
 */
static void *suns_pam_implementation_blows = 0;


/* This can be called at any time, and says whether the typed password
   belongs to either the logged in user (real uid, not effective); or
   to root.
 */
Bool
pam_passwd_valid_p (const char *typed_passwd, Bool verbose_p)
{
  const char *service = PAM_SERVICE_NAME;
  pam_handle_t *pamh = 0;
  int status = -1;
  struct pam_conv pc;
  struct pam_closure c;
  char *user = 0;

  struct passwd *p = getpwuid (getuid ());
  if (!p) return False;

  user = strdup (p->pw_name);

  c.user = user;
  c.typed_passwd = typed_passwd;
  c.verbose_p = verbose_p;

  pc.conv = &pam_conversation;
  pc.appdata_ptr = (void *) &c;

  /* On SunOS 5.6, the `appdata_ptr' slot seems to be ignored, and the
     `closure' argument to pc.conv always comes in as random garbage. */
  suns_pam_implementation_blows = (void *) &c;


  /* Initialize PAM.
   */
  status = pam_start (service, c.user, &pc, &pamh);
  if (verbose_p)
    fprintf (stderr, "%s: pam_start (\"%s\", \"%s\", ...) ==> %d (%s)\n",
             blurb(), service, c.user,
             status, PAM_STRERROR (pamh, status));
  if (status != PAM_SUCCESS) goto DONE;

  /* #### We should set PAM_TTY to the display we're using, but we
     don't have that handy from here.  So set it to :0.0, which is a
     good guess (and has the bonus of counting as a "secure tty" as
     far as PAM is concerned...)
   */
  {
    const char *tty = ":0.0";
    status = pam_set_item (pamh, PAM_TTY, strdup(tty));
    if (verbose_p)
      fprintf (stderr, "%s:   pam_set_item (p, PAM_TTY, \"%s\") ==> %d (%s)\n",
               blurb(), tty, status, PAM_STRERROR(pamh, status));
  }

  /* Try to authenticate as the current user.
   */
  PAM_NO_DELAY(pamh);
  status = pam_authenticate (pamh, 0);
  if (verbose_p)
    fprintf (stderr, "%s:   pam_authenticate (...) ==> %d (%s)\n",
             blurb(), status, PAM_STRERROR(pamh, status));
  if (status == PAM_SUCCESS)  /* Win! */
    {
      /* Each time we successfully authenticate, refresh credentials,
         for Kerberos/AFS/DCE/etc.  If this fails, just ignore that
         failure and blunder along; it shouldn't matter.
       */
      int status2 = pam_setcred (pamh, PAM_REFRESH_CRED);
      if (verbose_p)
        fprintf (stderr, "%s:   pam_setcred (...) ==> %d (%s)\n",
                 blurb(), status2, PAM_STRERROR(pamh, status2));
      goto DONE;
    }

  /* If that didn't work, set the user to root, and try to authenticate again.
   */
  c.user = "root";
  status = pam_set_item (pamh, PAM_USER, strdup(c.user));
  if (verbose_p)
    fprintf (stderr, "%s:   pam_set_item(p, PAM_USER, \"%s\") ==> %d (%s)\n",
             blurb(), c.user, status, PAM_STRERROR(pamh, status));
  if (status != PAM_SUCCESS) goto DONE;

  PAM_NO_DELAY(pamh);
  status = pam_authenticate (pamh, 0);
  if (verbose_p)
    fprintf (stderr, "%s:   pam_authenticate (...) ==> %d (%s)\n",
             blurb(), status, PAM_STRERROR(pamh, status));

 DONE:
  if (user) free (user);
  if (pamh)
    {
      int status2 = pam_end (pamh, status);
      pamh = 0;
      if (verbose_p)
        fprintf (stderr, "%s: pam_end (...) ==> %d (%s)\n",
                 blurb(), status2,
                 (status2 == PAM_SUCCESS ? "Success" : "Failure"));
    }
  return (status == PAM_SUCCESS ? True : False);
}


Bool 
pam_priv_init (int argc, char **argv, Bool verbose_p)
{
  /* We have nothing to do at init-time.
     However, we might as well do some error checking.
     If "/etc/pam.d" exists and is a directory, but "/etc/pam.d/xlock"
     does not exist, warn that PAM probably isn't going to work.

     This is a priv-init instead of a non-priv init in case the directory
     is unreadable or something (don't know if that actually happens.)
   */
  const char   dir[] = "/etc/pam.d";
  const char  file[] = "/etc/pam.d/" PAM_SERVICE_NAME;
  const char file2[] = "/etc/pam.conf";
  struct stat st;

  if (stat (dir, &st) == 0 && st.st_mode & S_IFDIR)
    {
      if (stat (file, &st) != 0)
        fprintf (stderr,
                 "%s: warning: %s does not exist.\n"
                 "%s: password authentication via PAM is unlikely to work.\n",
                 blurb(), file, blurb());
    }
  else if (stat (file2, &st) == 0)
    {
      FILE *f = fopen (file2, "r");
      if (f)
        {
          Bool ok = False;
          char buf[255];
          while (fgets (buf, sizeof(buf), f))
            if (strstr (buf, PAM_SERVICE_NAME))
              {
                ok = True;
                break;
              }
          fclose (f);
          if (!ok)
            {
              fprintf (stderr,
                  "%s: warning: %s does not list the `%s' service.\n"
                  "%s: password authentication via PAM is unlikely to work.\n",
                       blurb(), file2, PAM_SERVICE_NAME, blurb());
            }
        }
      /* else warn about file2 existing but being unreadable? */
    }
  else
    {
      fprintf (stderr,
               "%s: warning: neither %s nor %s exist.\n"
               "%s: password authentication via PAM is unlikely to work.\n",
               blurb(), file2, file, blurb());
    }

  /* Return true anyway, just in case. */
  return True;
}


/* This is the function PAM calls to have a conversation with the user.
   Really, this function should be the thing that pops up dialog boxes
   as needed, and prompts for various strings.

   But, for now, xscreensaver uses its normal password-prompting dialog
   first, and then this function simply returns the result that has been
   typed.

   This means that if PAM was using a retina scanner for auth, xscreensaver
   would prompt for a password; then pam_conversation() would be called
   with a string like "Please look into the retina scanner".  The user
   would never see this string, and the prompted-for password would be
   ignored.
 */
static int
pam_conversation (int nmsgs,
                  const struct pam_message **msg,
                  struct pam_response **resp,
                  void *closure)
{
  int replies = 0;
  struct pam_response *reply = 0;
  struct pam_closure *c = (struct pam_closure *) closure;

  /* On SunOS 5.6, the `closure' argument always comes in as random garbage. */
  c = (struct pam_closure *) suns_pam_implementation_blows;


  reply = (struct pam_response *) calloc (nmsgs, sizeof (*reply));
  if (!reply) return PAM_CONV_ERR;
	
  for (replies = 0; replies < nmsgs; replies++)
    {
      switch (msg[replies]->msg_style)
        {
        case PAM_PROMPT_ECHO_ON:
          reply[replies].resp_retcode = PAM_SUCCESS;
          reply[replies].resp = strdup (c->user);	   /* freed by PAM */
          if (c->verbose_p)
            fprintf (stderr, "%s:     PAM ECHO_ON(\"%s\") ==> \"%s\"\n",
                     blurb(), msg[replies]->msg,
                     reply[replies].resp);
          break;
        case PAM_PROMPT_ECHO_OFF:
          reply[replies].resp_retcode = PAM_SUCCESS;
          reply[replies].resp = strdup (c->typed_passwd);   /* freed by PAM */
          if (c->verbose_p)
            fprintf (stderr, "%s:     PAM ECHO_OFF(\"%s\") ==> password\n",
                     blurb(), msg[replies]->msg);
          break;
        case PAM_TEXT_INFO:
          /* ignore it... */
          reply[replies].resp_retcode = PAM_SUCCESS;
          reply[replies].resp = 0;
          if (c->verbose_p)
            fprintf (stderr, "%s:     PAM TEXT_INFO(\"%s\") ==> ignored\n",
                     blurb(), msg[replies]->msg);
          break;
        case PAM_ERROR_MSG:
          /* ignore it... */
          reply[replies].resp_retcode = PAM_SUCCESS;
          reply[replies].resp = 0;
          if (c->verbose_p)
            fprintf (stderr, "%s:     PAM ERROR_MSG(\"%s\") ==> ignored\n",
                     blurb(), msg[replies]->msg);
          break;
        default:
          /* Must be an error of some sort... */
          free (reply);
          if (c->verbose_p)
            fprintf (stderr, "%s:     PAM unknown %d(\"%s\") ==> ignored\n",
                     blurb(), msg[replies]->msg_style, msg[replies]->msg);
          return PAM_CONV_ERR;
        }
    }
  *resp = reply;
  return PAM_SUCCESS;
}

#endif /* NO_LOCKING -- whole file */
