/* passwd-pam.c --- verifying typed passwords with PAM
 * (Pluggable Authentication Modules.)
 * written by Bill Nottingham <notting@redhat.com> (and jwz) for
 * xscreensaver, Copyright (c) 1993-2017 Jamie Zawinski <jwz@jwz.org>
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
 *    http://www.us.kernel.org/pub/linux/libs/pam/Linux-PAM-html/Linux-PAM_ADG.html
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
#include <signal.h>
#include <errno.h>
#include <X11/Intrinsic.h>

#include <sys/stat.h>

#include "auth.h"

extern sigset_t block_sigchld (void);
extern void unblock_sigchld (void);

/* blargh */
#undef  Bool
#undef  True
#undef  False
#define Bool  int
#define True  1
#define False 0

#undef countof
#define countof(x) (sizeof((x))/sizeof(*(x)))

/* Some time between Red Hat 4.2 and 7.0, the words were transposed 
   in the various PAM_x_CRED macro names.  Yay!
 */
#if !defined(PAM_REFRESH_CRED) && defined(PAM_CRED_REFRESH)
# define PAM_REFRESH_CRED PAM_CRED_REFRESH
#endif
#if !defined(PAM_REINITIALIZE_CRED) && defined(PAM_CRED_REINITIALIZE)
# define PAM_REINITIALIZE_CRED PAM_CRED_REINITIALIZE
#endif

static int pam_conversation (int nmsgs,
                             const struct pam_message **msg,
                             struct pam_response **resp,
                             void *closure);

void pam_try_unlock(saver_info *si, Bool verbose_p,
	       Bool (*valid_p)(const char *typed_passwd, Bool verbose_p));

Bool pam_priv_init (int argc, char **argv, Bool verbose_p);

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
   *unlock* the screen.  (With the non-PAM password code, the analogous
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
   (I've also been told that it's fixed in Solaris 2.6 by patch 106257-05.)
 */
static void *suns_pam_implementation_blows = 0;


/**
 * This function is the PAM conversation driver. It conducts a full
 * authentication round by invoking the GUI with various prompts.
 */
void
pam_try_unlock(saver_info *si, Bool verbose_p,
	       Bool (*valid_p)(const char *typed_passwd, Bool verbose_p))
{
  const char *service = PAM_SERVICE_NAME;
  pam_handle_t *pamh = 0;
  int status = -1;
  struct pam_conv pc;
# ifdef HAVE_SIGTIMEDWAIT
  sigset_t set;
  struct timespec timeout;
# endif /* HAVE_SIGTIMEDWAIT */

  pc.conv = &pam_conversation;
  pc.appdata_ptr = (void *) si;

  /* On SunOS 5.6, the `appdata_ptr' slot seems to be ignored, and the
     `closure' argument to pc.conv always comes in as random garbage. */
  suns_pam_implementation_blows = (void *) si;


  /* Initialize PAM.
   */
  status = pam_start (service, si->user, &pc, &pamh);
  if (verbose_p)
    fprintf (stderr, "%s: pam_start (\"%s\", \"%s\", ...) ==> %d (%s)\n",
             blurb(), service, si->user,
             status, PAM_STRERROR (pamh, status));
  if (status != PAM_SUCCESS) goto DONE;

  /* #### We should set PAM_TTY to the display we're using, but we
     don't have that handy from here.  So set it to :0.0, which is a
     good guess (and has the bonus of counting as a "secure tty" as
     far as PAM is concerned...)
   */
  {
    char *tty = strdup (":0.0");
    status = pam_set_item (pamh, PAM_TTY, tty);
    if (verbose_p)
      fprintf (stderr, "%s:   pam_set_item (p, PAM_TTY, \"%s\") ==> %d (%s)\n",
               blurb(), tty, status, PAM_STRERROR(pamh, status));
    free (tty);
  }

  /* Try to authenticate as the current user.
     We must turn off our SIGCHLD handler for the duration of the call to
     pam_authenticate(), because in some cases, the underlying PAM code
     will do this:

        1: fork a setuid subprocess to do some dirty work;
        2: read a response from that subprocess;
        3: waitpid(pid, ...) on that subprocess.

    If we (the ignorant parent process) have a SIGCHLD handler, then there's
    a race condition between steps 2 and 3: if the subprocess exits before
    waitpid() was called, then our SIGCHLD handler fires, and gets notified
    of the subprocess death; then PAM's call to waitpid() fails, because the
    process has already been reaped.

    I consider this a bug in PAM, since the caller should be able to have
    whatever signal handlers it wants -- the PAM documentation doesn't say
    "oh by the way, if you use PAM, you can't use SIGCHLD."
   */

  PAM_NO_DELAY(pamh);

  if (verbose_p)
    fprintf (stderr, "%s:   pam_authenticate (...) ...\n", blurb());

# ifdef HAVE_SIGTIMEDWAIT
  timeout.tv_sec = 0;
  timeout.tv_nsec = 1;
  set =
# endif /* HAVE_SIGTIMEDWAIT */
    block_sigchld();
  status = pam_authenticate (pamh, 0);
# ifdef HAVE_SIGTIMEDWAIT
  sigtimedwait (&set, NULL, &timeout);
  /* #### What is the portable thing to do if we don't have it? */
# endif /* HAVE_SIGTIMEDWAIT */
  unblock_sigchld();

  if (verbose_p)
    fprintf (stderr, "%s:   pam_authenticate (...) ==> %d (%s)\n",
             blurb(), status, PAM_STRERROR(pamh, status));

  if (status == PAM_SUCCESS)  /* Win! */
    {
      int status2;

      /* On most systems, it doesn't matter whether the account modules
         are run, or whether they fail or succeed.

         On some systems, the account modules fail, because they were
         never configured properly, but it's necessary to run them anyway
         because certain PAM modules depend on side effects of the account
         modules having been run.

         And on still other systems, the account modules are actually
         used, and failures in them should be considered to be true!

         So:
         - We run the account modules on all systems.
         - Whether we ignore them is a configure option.

         It's all kind of a mess.
       */
      status2 = pam_acct_mgmt (pamh, 0);

      if (verbose_p)
        fprintf (stderr, "%s:   pam_acct_mgmt (...) ==> %d (%s)\n",
                 blurb(), status2, PAM_STRERROR(pamh, status2));

      /* HPUX for some reason likes to make PAM defines different from
       * everyone else's. */
#ifdef PAM_AUTHTOKEN_REQD
      if (status2 == PAM_AUTHTOKEN_REQD)
#else
      if (status2 == PAM_NEW_AUTHTOK_REQD)
#endif
        {
          status2 = pam_chauthtok (pamh, PAM_CHANGE_EXPIRED_AUTHTOK);
          if (verbose_p)
            fprintf (stderr, "%s: pam_chauthtok (...) ==> %d (%s)\n",
                     blurb(), status2, PAM_STRERROR(pamh, status2));
        }

      /* If 'configure' requested that we believe the results of PAM
         account module failures, then obey that status code.
         Otherwise ignore it.
       */
#ifdef PAM_CHECK_ACCOUNT_TYPE
       status = status2;
#endif

      /* Each time we successfully authenticate, refresh credentials,
         for Kerberos/AFS/DCE/etc.  If this fails, just ignore that
         failure and blunder along; it shouldn't matter.
       */

#if defined(__linux__)
      /* Apparently the Linux PAM library ignores PAM_REFRESH_CRED and only
         refreshes credentials when using PAM_REINITIALIZE_CRED. */
      status2 = pam_setcred (pamh, PAM_REINITIALIZE_CRED);
#else
      /* But Solaris requires PAM_REFRESH_CRED or extra prompts appear. */
      status2 = pam_setcred (pamh, PAM_REFRESH_CRED);
#endif

      if (verbose_p)
        fprintf (stderr, "%s:   pam_setcred (...) ==> %d (%s)\n",
                 blurb(), status2, PAM_STRERROR(pamh, status2));
    }

 DONE:
  if (pamh)
    {
      int status2 = pam_end (pamh, status);
      pamh = 0;
      if (verbose_p)
        fprintf (stderr, "%s: pam_end (...) ==> %d (%s)\n",
                 blurb(), status2,
                 (status2 == PAM_SUCCESS ? "Success" : "Failure"));
    }

  if (status == PAM_SUCCESS)
    si->unlock_state = ul_success;	     /* yay */
  else if (si->unlock_state == ul_cancel ||
           si->unlock_state == ul_time)
    ;					     /* more specific failures ok */
  else
    si->unlock_state = ul_fail;		     /* generic failure */
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

# ifndef S_ISDIR
#  define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
# endif

  if (stat (dir, &st) == 0 && S_ISDIR(st.st_mode))
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


static int
pam_conversation (int nmsgs,
		  const struct pam_message **msg,
		  struct pam_response **resp,
		  void *vsaver_info)
{
  int i, ret = -1;
  struct auth_message *messages = 0;
  struct auth_response *authresp = 0;
  struct pam_response *pam_responses;
  saver_info *si = (saver_info *) vsaver_info;
  Bool verbose_p;

  /* On SunOS 5.6, the `closure' argument always comes in as random garbage. */
  si = (saver_info *) suns_pam_implementation_blows;

  verbose_p = si->prefs.verbose_p;

  /* Converting the PAM prompts into the XScreenSaver native format.
   * It was a design goal to collapse (INFO,PROMPT) pairs from PAM
   * into a single call to the unlock_cb function. The unlock_cb function
   * does that, but only if it is passed several prompts at a time. Most PAM
   * modules only send a single prompt at a time, but because there is no way
   * of telling whether there will be more prompts to follow, we can only ever
   * pass along whatever was passed in here.
   */

  messages = calloc(nmsgs, sizeof(struct auth_message));
  pam_responses = calloc(nmsgs, sizeof(*pam_responses));
  
  if (!pam_responses || !messages)
    goto end;

  if (verbose_p)
    fprintf (stderr, "%s:     pam_conversation (", blurb());

  for (i = 0; i < nmsgs; ++i)
    {
      if (verbose_p && i > 0) fprintf (stderr, ", ");

      messages[i].msg = msg[i]->msg;

      switch (msg[i]->msg_style) {
      case PAM_PROMPT_ECHO_OFF: messages[i].type = AUTH_MSGTYPE_PROMPT_NOECHO;
        if (verbose_p) fprintf (stderr, "ECHO_OFF");
        break;
      case PAM_PROMPT_ECHO_ON:  messages[i].type = AUTH_MSGTYPE_PROMPT_ECHO;
        if (verbose_p) fprintf (stderr, "ECHO_ON");
        break;
      case PAM_ERROR_MSG:       messages[i].type = AUTH_MSGTYPE_ERROR;
        if (verbose_p) fprintf (stderr, "ERROR_MSG");
        break;
      case PAM_TEXT_INFO:       messages[i].type = AUTH_MSGTYPE_INFO;
        if (verbose_p) fprintf (stderr, "TEXT_INFO");
        break;
      default:                  messages[i].type = AUTH_MSGTYPE_PROMPT_ECHO;
        if (verbose_p) fprintf (stderr, "PROMPT_ECHO");
        break;
      }

      if (verbose_p) 
        fprintf (stderr, "=\"%s\"", msg[i]->msg ? msg[i]->msg : "(null)");
    }

  if (verbose_p)
    fprintf (stderr, ") ...\n");

  ret = si->unlock_cb(nmsgs, messages, &authresp, si);

  /* #### If the user times out, or hits ESC or Cancel, we return PAM_CONV_ERR,
          and PAM logs this as an authentication failure.  It would be nice if
          there was some way to indicate that this was a "cancel" rather than
          a "fail", so that it wouldn't show up in syslog, but I think the
          only options are PAM_SUCCESS and PAM_CONV_ERR.  (I think that
          PAM_ABORT means "internal error", not "cancel".)  Bleh.
   */

  if (ret == 0)
    {
      for (i = 0; i < nmsgs; ++i)
	pam_responses[i].resp = authresp[i].response;
    }

end:
  if (messages)
    free(messages);

  if (authresp)
    free(authresp);

  if (verbose_p)
    fprintf (stderr, "%s:     pam_conversation (...) ==> %s\n", blurb(),
             (ret == 0 ? "PAM_SUCCESS" : "PAM_CONV_ERR"));

  if (ret == 0)
    {
      *resp = pam_responses;
      return PAM_SUCCESS;
    }

  /* Failure only */
    if (pam_responses)
      free(pam_responses);

    return PAM_CONV_ERR;
}

#endif /* NO_LOCKING -- whole file */
