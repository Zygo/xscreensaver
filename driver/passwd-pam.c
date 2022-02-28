/* passwd-pam.c --- verifying typed passwords with PAM
 * xscreensaver, Copyright © 1993-2022 Jamie Zawinski <jwz@jwz.org>
 * By Bill Nottingham <notting@redhat.com> and jwz.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* PAM sucks in that there is no way to tell whether a particular service is
   configured at all.  That is, there is no way to tell the difference between
   "authentication of the FOO service is not allowed" and "the user typed the
   wrong password."

   On RedHat 5.1 systems, if a service name is not known, it defaults to being
   not allowed (because the fallback service, /etc/pam.d/other, is set to
   `pam_deny'.)

   On Solaris 2.6 systems, unknown services default to authenticating normally.

   So, we require that an "xscreensaver" PAM service exist.  This has a bad
   failure mode, however: if that service doesn't exist, then XScreenSaver
   will lock the screen, but is unable to unlock.  (With the non-PAM password
   code, XScreenSaver can refuse to lock, because it is able to determine up
   front that it was unable to retrieve the password info.)

   At startup, we check for the existence of /etc/pam.d/xscreensaver or
   /etc/pam.conf, and if those don't exist, we print a warning that PAM is
   probably not configured properly.  This isn't *necessarily* correct, since
   those files are not a part of PAM's C API, but it's how real-world systems
   actually work.

   Also note that FreeBSD's PAM configuration requires the calling process to
   be running as root during the *entire* interactive PAM conversation: it
   can't ever disavow privileges.  Whereas Linux's PAM configurations use a
   setuid helper within the PAM stack so that a non-root process can still
   authenticate, as is right and proper.  Consequently, PAM does not work with
   XScreenSaver on FreeBSD by default.  Dear FreeBSD, get your shit together.

   I am told that only *some* of the FreeBSD PAM modules have this bug, e.g.,
   "pam_unix" fails, but "pam_winbind" works.  So this problem is demonstrably
   fixable entirely within the PAM stack, and that's where it should be fixed.
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
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <security/pam_appl.h>

#include "blurb.h"
#include "auth.h"


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

typedef struct {
  Bool (*conv_fn) (void *closure,
                   int nmsgs,
                   const auth_message *msg,
                   auth_response **resp);
  void *conv_fn_closure;
} pam_conv_closure;


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


/* On SunOS 5.6, the `pam_conv.appdata_ptr' slot seems to be ignored, and
   the `closure' argument to pc.conv always comes in as random garbage.
   So we get around this by using a global variable instead.  Shoot me!

   (I've been told this is bug 4092227, and is fixed in Solaris 7.)
   (I've also been told that it's fixed in Solaris 2.6 by patch 106257-05.)
 */
static void *suns_pam_implementation_blows = 0;


/* This function invokes the PAM conversation.  It conducts a full
   authentication round by presenting the GUI with various prompts.
 */
Bool
pam_try_unlock (void *closure,
                Bool (*conv_fn) (void *closure,
                                 int nmsgs,
                                 const auth_message *msg,
                                 auth_response **resp))
{
  const char *service = PAM_SERVICE_NAME;
  pam_handle_t *pamh = 0;
  int status = -1;
  struct pam_conv pc;
  struct passwd *p = getpwuid (getuid());
  const char *user = (p && p->pw_name && *p->pw_name ? p->pw_name : 0);
  pam_conv_closure conv_closure;

  if (!user)
    {
      fprintf (stderr, "%s: PAM: unable to get current user\n", blurb());
      return False;
    }

  conv_closure.conv_fn = conv_fn;
  conv_closure.conv_fn_closure = closure;
  pc.appdata_ptr = &conv_closure;
  pc.conv = &pam_conversation;

  /* On SunOS 5.6, the `appdata_ptr' slot seems to be ignored, and the
     `closure' argument to pc.conv always comes in as random garbage. */
  suns_pam_implementation_blows = pc.appdata_ptr;


  /* Initialize PAM.
   */
  status = pam_start (service, user, &pc, &pamh);
  if (verbose_p)
    fprintf (stderr, "%s: PAM: pam_start (\"%s\", \"%s\", ...) ==> %d (%s)\n",
             blurb(), service, user,
             status, PAM_STRERROR (pamh, status));
  if (status != PAM_SUCCESS) goto DONE;

  {
    char *tty = getenv ("DISPLAY");
    if (!tty || !*tty) tty = ":0.0";
    status = pam_set_item (pamh, PAM_TTY, tty);
    if (verbose_p)
      fprintf (stderr, "%s:   pam_set_item (p, PAM_TTY, \"%s\") ==> %d (%s)\n",
               blurb(), tty, status, PAM_STRERROR(pamh, status));
  }

  PAM_NO_DELAY(pamh);

  if (verbose_p)
    fprintf (stderr, "%s:   pam_authenticate (...) ...\n", blurb());

  status = pam_authenticate (pamh, 0);

  if (verbose_p)
    fprintf (stderr, "%s:   pam_authenticate (...) ==> %d (%s)\n",
             blurb(), status, PAM_STRERROR(pamh, status));

  if (status == PAM_SUCCESS)  /* So far so good... */
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

  return (status == PAM_SUCCESS);
}


Bool 
pam_priv_init (void)
{
  /* We have nothing to do at init-time.
     However, we might as well do some error checking.
     If "/etc/pam.d" exists and is a directory, but "/etc/pam.d/xscreensaver"
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
                 "%s: PAM: warning: %s does not exist.\n"
                 "%s: PAM: password authentication is unlikely to work.\n",
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
                  "%s: PAM: warning: %s does not list the `%s' service.\n"
                  "%s: PAM: password authentication is unlikely to work.\n",
                       blurb(), file2, PAM_SERVICE_NAME, blurb());
            }
        }
      /* else warn about file2 existing but being unreadable? */
    }
  else
    {
      fprintf (stderr,
               "%s: PAM: warning: neither %s nor %s exist.\n"
               "%s: PAM: password authentication is unlikely to work.\n",
               blurb(), file2, file, blurb());
    }

  /* Return true anyway, just in case. */
  return True;
}


/* This is pam_conv->conv */
static int
pam_conversation (int nmsgs,
		  const struct pam_message **msg,
		  struct pam_response **resp,
		  void *closure)
{
  int i;
  auth_message *messages = 0;
  auth_response *authresp = 0;
  struct pam_response *pam_responses;
  pam_conv_closure *conv_closure;

  /* On SunOS 5.6, the `closure' argument always comes in as random garbage. */
  closure = suns_pam_implementation_blows;
  conv_closure = closure;

  /* Converting the PAM prompts into the XScreenSaver native format.
   * It was a design goal to collapse (INFO,PROMPT) pairs from PAM
   * into a single call to the unlock_cb function. The unlock_cb function
   * does that, but only if it is passed several prompts at a time. Most PAM
   * modules only send a single prompt at a time, but because there is no way
   * of telling whether there will be more prompts to follow, we can only ever
   * pass along whatever was passed in here.
   */

  messages = calloc (nmsgs, sizeof(*messages));
  pam_responses = calloc (nmsgs, sizeof(*pam_responses));
  if (!pam_responses || !messages) abort();

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

  /* This opens the dialog box and runs the X11 event loop. 
     It only returns if the user entered a password.
     If they hit cancel, or timed out, it exited.
  */
  conv_closure->conv_fn (conv_closure->conv_fn_closure, nmsgs, messages,
                         &authresp);

  for (i = 0; i < nmsgs; ++i)
    pam_responses[i].resp = authresp[i].response;

  if (messages) free (messages);
  if (authresp) free (authresp);

  if (verbose_p)
    fprintf (stderr, "%s:     pam_conversation (...) ==> PAM_SUCCESS\n",
             blurb());

  *resp = pam_responses;
  return PAM_SUCCESS;
}

#endif /* NO_LOCKING -- whole file */
