/* setuid.c --- management of runtime priveleges.
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

#ifndef NO_SETUID /* whole file */

#include <X11/Xlib.h>		/* not used for much... */

/* This file doesn't need the Xt headers, so stub these types out... */
#undef XtPointer
#define XtAppContext void*
#define XrmDatabase  void*
#define XtIntervalId void*
#define XtPointer    void*
#define Widget       void*

#include "xscreensaver.h"

#ifndef EPERM
#include <errno.h>
#endif

#include <pwd.h>		/* for getpwnam() and struct passwd */
#include <grp.h>		/* for getgrgid() and struct group */


static const char *
uid_gid_string(uid_t uid, gid_t gid)
{
  static char buf[255];
  struct passwd *p = 0;
  struct group *g = 0;
  p = getpwuid (uid);
  g = getgrgid (gid);
  sprintf (buf, "%s/%s (%ld/%ld)",
	   (p && p->pw_name ? p->pw_name : "???"),
	   (g && g->gr_name ? g->gr_name : "???"),
	   (long) uid, (long) gid);
  return buf;
}


void
describe_uids (saver_info *si, FILE *out)
{
  uid_t uid = getuid();
  gid_t gid = getgid();
  uid_t euid = geteuid();
  gid_t egid = getegid();
  char *s1 = strdup (uid_gid_string (uid, gid));
  char *s2 = strdup (uid_gid_string (euid, egid));

  if (si->orig_uid && *si->orig_uid)
    fprintf (out, "%s: initial effective uid/gid was %s\n", blurb(),
	     si->orig_uid);
  fprintf (out, "%s: running as %s", blurb(), s1);
  if (uid != euid || gid != egid)
    fprintf (out, "; effectively %s", s2);
  fprintf(out, "\n");
  free(s1);
  free(s2);
}


/* If we've been run as setuid or setgid to someone else (most likely root)
   turn off the extra permissions so that random user-specified programs
   don't get special privileges.  (On some systems it is necessary to install
   this program as setuid root in order to read the passwd file to implement
   lock-mode.)
 */
void
hack_uid (saver_info *si)
{
  si->orig_uid = strdup (uid_gid_string (geteuid(), getegid()));

  setgid (getgid ());
  setuid (getuid ());

  /* If we're being run as root (as from xdm) then switch the user id
     to something safe. */
  if (getuid () == 0)
    {
      struct passwd *p = 0;
      struct group *g = 0;
      int uid_errno = 0;
      int gid_errno = 0;

      /* Locking can't work when running as root, because we have no way of
	 knowing what the user id of the logged in user is (so we don't know
	 whose password to prompt for.)
       */
      si->locking_disabled_p = True;
      si->nolock_reason = "running as root";
      p = getpwnam ("nobody");
      if (! p) p = getpwnam ("noaccess");
      if (! p) p = getpwnam ("daemon");
      if (! p)
	{
	  fprintf (stderr,
		   "%s: running as root, and couldn't find a safer uid.\n",
		   blurb());
	  saver_exit(si, 1, 0);
	}

      g = getgrgid (p->pw_gid);

      /* Rumor has it that some implementations of of setuid() do nothing
	 when called with -1; therefore, if the "nobody" user has a uid of
	 -1, then that would be Really Bad.  Rumor further has it that such
	 systems really ought to be using -2 for "nobody", since that works.
	 So, if we get a uid (or gid, for good measure) of -1, switch to -2
	 instead.
       */

      if (p->pw_gid == -1) p->pw_gid = -2;
      if (p->pw_uid == -1) p->pw_uid = -2;


      /* Change the gid to be a safe one, then change the uid to be a safe
	 one (must do it in this order, because root privs vanish when uid
	 is changed, and after that, gid can't be changed.)
       */
      if (setgid (p->pw_gid) != 0)
	gid_errno = errno ? errno : -1;
      if (setuid (p->pw_uid) != 0)
	uid_errno = errno ? errno : -1;

      if (uid_errno == 0 && gid_errno == 0)
	{
	  static char buf [1024];
	  sprintf (buf, "changed uid/gid to %s/%s (%ld/%ld).",
		   p->pw_name, (g ? g->gr_name : "???"),
		   (long) p->pw_uid, (long) p->pw_gid);
	  si->uid_message = buf;
	}
      else
	{
	  char buf [1024];
	  if (gid_errno)
	    {
	      sprintf (buf, "%s: couldn't set gid to %s (%ld)",
		       blurb(),
		       (g ? g->gr_name : "???"),
		       (long) p->pw_gid);
	      if (gid_errno == -1)
		fprintf(stderr, "%s: unknown error\n", buf);
	      else
		perror(buf);
	    }

	  if (uid_errno)
	    {
	      sprintf (buf, "%s: couldn't set uid to %s (%ld)",
		       blurb(),
		       (p ? p->pw_name : "???"),
		       (long) p->pw_uid);
	      if (uid_errno == -1)
		fprintf(stderr, "%s: unknown error\n", buf);
	      else
		perror(buf);
	    }
	}

      if (uid_errno != 0)
	{
	  /* We'd better exit rather than continue running as root.
	     But if we switched uid but not gid, continue running,
	     since that doesn't really matter.  (Right?)
	   */
	  saver_exit (si, -1, 0);
	}
    }
# ifndef NO_LOCKING
 else	/* disable locking if already being run as "someone else" */
   {
     struct passwd *p = getpwuid (getuid ());
     if (!p ||
	 !strcmp (p->pw_name, "root") ||
	 !strcmp (p->pw_name, "nobody") ||
	 !strcmp (p->pw_name, "noaccess") ||
	 !strcmp (p->pw_name, "operator") ||
	 !strcmp (p->pw_name, "daemon") ||
	 !strcmp (p->pw_name, "bin") ||
	 !strcmp (p->pw_name, "adm") ||
	 !strcmp (p->pw_name, "sys") ||
	 !strcmp (p->pw_name, "games"))
       {
	 static char buf [1024];
	 sprintf (buf, "running as %s", p->pw_name);
	 si->nolock_reason = buf;
	 si->locking_disabled_p = True;
       }
   }
# endif /* !NO_LOCKING */
}

#else  /* !NO_SETUID */

void hack_uid (saver_info *si) { }
void hack_uid_warn (saver_info *si) { }
void describe_uids (saver_info *si) { }

#endif /* NO_SETUID */
