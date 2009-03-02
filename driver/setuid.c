/* setuid.c --- management of runtime privileges.
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
uid_gid_string (uid_t uid, gid_t gid)
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

  if (si->orig_uid && *si->orig_uid &&
      (!!strcmp (si->orig_uid, s1) ||
       !!strcmp (si->orig_uid, s2)))
    fprintf (out, "%s: initial effective uid/gid was %s\n", blurb(),
	     si->orig_uid);

  fprintf (out, "%s: running as %s", blurb(), s1);
  if (uid != euid || gid != egid)
    fprintf (out, "; effectively %s", s2);
  fprintf(out, "\n");
  free(s1);
  free(s2);
}


static int
set_ids_by_name (struct passwd *p, struct group *g, char **message_ret)
{
  int uid_errno = 0;
  int gid_errno = 0;
  uid_t uid = p->pw_uid;
  gid_t gid = g->gr_gid;

  if (message_ret)
    *message_ret = 0;

  /* Rumor has it that some implementations of of setuid() do nothing
     when called with -1; therefore, if the "nobody" user has a uid of
     -1, then that would be Really Bad.  Rumor further has it that such
     systems really ought to be using -2 for "nobody", since that works.
     So, if we get a uid (or gid, for good measure) of -1, switch to -2
     instead.
   */
  if (gid == (gid_t) -1) gid = (gid_t) -2;
  if (uid == (uid_t) -1) uid = (uid_t) -2;

  errno = 0;
  if (setgid (gid) != 0)
    gid_errno = errno ? errno : -1;

  errno = 0;
  if (setuid (uid) != 0)
    uid_errno = errno ? errno : -1;

  if (uid_errno == 0 && gid_errno == 0)
    {
      static char buf [1024];
      sprintf (buf, "changed uid/gid to %s/%s (%ld/%ld).",
	       p->pw_name, (g ? g->gr_name : "???"),
	       (long) uid, (long) gid);
      if (message_ret)
	*message_ret = buf;
      return 0;
    }
  else
    {
      char buf [1024];
      if (gid_errno)
	{
	  sprintf (buf, "%s: couldn't set gid to %s (%ld)",
		   blurb(),
		   (g ? g->gr_name : "???"),
		   (long) gid);
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
		   (long) uid);
	  if (uid_errno == -1)
	    fprintf(stderr, "%s: unknown error\n", buf);
	  else
	    perror(buf);
	}

      return -1;
    }
}

static int
set_ids_by_number (uid_t uid, gid_t gid, char **message_ret)
{
  struct passwd *p;
  struct group *g;

  errno = 0;
  p = getpwuid (uid);
  if (!p)
    {
      char buf [1024];
      sprintf (buf, "%s: error looking up name of user %d", blurb(),
	       (long) uid);
      if (errno)
	perror (buf);
      else
	fprintf (stderr, "%s: unknown error.\n", buf);
      return -1;
    }

  errno = 0;
  g = getgrgid (gid);
  if (!g)
    {
      char buf [1024];
      sprintf (buf, "%s: error looking up name of group %d", blurb(),
	       (long) gid);
      if (errno)
	perror (buf);
      else
	fprintf (stderr, "%s: unknown error.\n", buf);
      return -1;
    }

  return set_ids_by_name (p, g, message_ret);
}


/* If we've been run as setuid or setgid to someone else (most likely root)
   turn off the extra permissions so that random user-specified programs
   don't get special privileges.  (On some systems it is necessary to install
   this program as setuid root in order to read the passwd file to implement
   lock-mode.)

     *** WARNING: DO NOT DISABLE ANY OF THE FOLLOWING CODE!
         If you do so, you will open a security hole.  See the sections
         of the xscreensaver manual titled "LOCKING AND ROOT LOGINS", 
         and "USING XDM".
 */
void
hack_uid (saver_info *si)
{

  /* Discard privileges, and set the effective user/group ids to the
     real user/group ids.  That is, give up our "chmod +s" rights.
   */
  {
    uid_t euid = geteuid();
    gid_t egid = getegid();
    uid_t uid = getuid();
    gid_t gid = getgid();

    si->orig_uid = strdup (uid_gid_string (euid, egid));

    if (uid != euid || gid != egid)
      if (set_ids_by_number (uid, gid, &si->uid_message) != 0)
	saver_exit (si, 1, 0);
  }


  /* Locking can't work when running as root, because we have no way of
     knowing what the user id of the logged in user is (so we don't know
     whose password to prompt for.)

     *** WARNING: DO NOT DISABLE THIS CODE!
         If you do so, you will open a security hole.  See the sections
         of the xscreensaver manual titled "LOCKING AND ROOT LOGINS",
         and "USING XDM".
   */
  if (getuid() == (uid_t) 0)
    {
      si->locking_disabled_p = True;
      si->nolock_reason = "running as root";
    }


  /* If we're running as root, switch to a safer user.  This is above and
     beyond the fact that we've disabling locking, above -- the theory is
     that running graphics demos as root is just always a stupid thing
     to do, since they have probably never been security reviewed and are
     more likely to be buggy than just about any other kind of program.
     (And that assumes non-malicious code.  There are also attacks here.)

     *** WARNING: DO NOT DISABLE THIS CODE!
         If you do so, you will open a security hole.  See the sections
         of the xscreensaver manual titled "LOCKING AND ROOT LOGINS", 
         and "USING XDM".
   */
  if (getuid() == (uid_t) 0)
    {
      struct passwd *p;

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

      if (set_ids_by_number (p->pw_uid, p->pw_gid, &si->uid_message) != 0)
	saver_exit (si, -1, 0);
    }


  /* If there's anything even remotely funny looking about the passwd struct,
     or if we're running as some other user from the list below (a
     non-comprehensive selection of users known to be privileged in some way,
     and not normal end-users) then disable locking.  If it was possible,
     switching to "nobody" would be the thing to do, but only root itself has
     the privs to do that.

     *** WARNING: DO NOT DISABLE THIS CODE!
         If you do so, you will open a security hole.  See the sections
         of the xscreensaver manual titled "LOCKING AND ROOT LOGINS",
         and "USING XDM".
   */
  {
    uid_t uid = getuid ();		/* get it again */
    struct passwd *p = getpwuid (uid);	/* get it again */

    if (!p ||
	uid == (uid_t)  0 ||
	uid == (uid_t) -1 ||
	uid == (uid_t) -2 ||
	p->pw_uid == (uid_t)  0 ||
	p->pw_uid == (uid_t) -1 ||
	p->pw_uid == (uid_t) -2 ||
	!p->pw_name ||
	!*p->pw_name ||
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
	sprintf (buf, "running as %s",
		 (p && p->pw_name && *p->pw_name
		  ? p->pw_name : "<unknown>"));
	si->nolock_reason = buf;
	si->locking_disabled_p = True;
	si->dangerous_uid_p = True;
      }
  }
}
