/* setuid.c --- management of runtime privileges.
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
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <string.h>
#include <pwd.h>		/* for getpwnam() and struct passwd */
#include <grp.h>		/* for getgrgid() and struct group */
#include <errno.h>

#include "blurb.h"
#include "auth.h"


static const char *
uid_gid_string (uid_t uid, gid_t gid)
{
  static char buf[255];
  struct passwd *p = 0;
  struct group *g = 0;
  p = getpwuid (uid);
  g = getgrgid (gid);
  sprintf (buf, "%.100s/%.100s (%ld/%ld)",
	   (p && p->pw_name ? p->pw_name : "???"),
	   (g && g->gr_name ? g->gr_name : "???"),
	   (long) uid, (long) gid);
  return buf;
}


/* Returns true if we need to call setgroups().

   Without calling setgroups(), the process will retain any supplementary
   gids associated with the uid, e.g.:

       % groups root
       root : root bin daemon sys adm disk wheel

   However, setgroups() can only be called by root, and returns EPERM
   for other users even if the call would be a no-op (e.g., setting the
   group list to the current list.)  So, to avoid that spurious error,
   before calling setgroups() we first check whether the current list
   of groups contains only one element, our target group.  If so, we
   don't need to call setgroups().
 */
static int
setgroups_needed_p (uid_t target_group)
{
  gid_t groups[1024];
  int n, size;
  size = sizeof(groups) / sizeof(gid_t);
  n = getgroups (size - 1, groups);
  if (n < 0)
    {
      char buf [1024];
      sprintf (buf, "%s: getgroups(%ld, ...)", blurb(), (long int)(size - 1));
      perror (buf);
      return 1;
    }
  else if (n == 0)            /* an empty list means only egid is in effect. */
    return 0;
  else if (n == 1 && groups[0] == target_group)   /* one element, the target */
    return 0;
  else                        /* more than one, or the wrong one. */
    return 1;
}


static int
set_ids_by_number (uid_t uid, gid_t gid)
{
  int uid_errno = 0;
  int gid_errno = 0;
  int sgs_errno = 0;
  struct passwd *p = getpwuid (uid);
  struct group  *g = getgrgid (gid);

  /* Rumor has it that some implementations of of setuid() do nothing
     when called with -1; therefore, if the "nobody" user has a uid of
     -1, then that would be Really Bad.  Rumor further has it that such
     systems really ought to be using -2 for "nobody", since that works.
     So, if we get a uid (or gid, for good measure) of -1, switch to -2
     instead.  Note that this must be done after we've looked up the
     user/group names with getpwuid(-1) and/or getgrgid(-1).
   */
  if (gid == (gid_t) -1) gid = (gid_t) -2;
  if (uid == (uid_t) -1) uid = (uid_t) -2;

  errno = 0;
  if (setgroups_needed_p (gid) &&
      setgroups (1, &gid) < 0)
    sgs_errno = errno ? errno : -1;

  errno = 0;
  if (setgid (gid) != 0)
    gid_errno = errno ? errno : -1;

  errno = 0;
  if (setuid (uid) != 0)
    uid_errno = errno ? errno : -1;

  if (uid_errno == 0 && gid_errno == 0 && sgs_errno == 0)
    {
      if (verbose_p)
        fprintf (stderr, "%s: changed uid/gid to %.100s/%.100s (%ld/%ld)\n",
                 blurb(),
                 (p && p->pw_name ? p->pw_name : "???"),
                 (g && g->gr_name ? g->gr_name : "???"),
                 (long) uid, (long) gid);
      return 0;
    }
  else
    {
      char buf [1024];
      gid_t groups[1024];
      int n, size;

      if (sgs_errno)
	{
	  sprintf (buf, "%s: couldn't setgroups to %.100s (%ld)",
		   blurb(),
		   (g && g->gr_name ? g->gr_name : "???"),
		   (long) gid);
	  if (sgs_errno == -1)
	    fprintf(stderr, "%s: unknown error\n", buf);
	  else
            {
              errno = sgs_errno;
              perror (buf);
            }

	  fprintf (stderr, "%s: effective group list: ", blurb());
	  size = sizeof(groups) / sizeof(gid_t);
          n = getgroups (size - 1, groups);
          if (n < 0)
            fprintf (stderr, "unknown!\n");
          else
            {
              int i;
              fprintf (stderr, "[");
              for (i = 0; i < n; i++)
                {
                  g = getgrgid (groups[i]);
                  if (i > 0) fprintf (stderr, ", ");
                  if (g && g->gr_name) fprintf (stderr, "%s", g->gr_name);
                  else fprintf (stderr, "%ld", (long) groups[i]);
                }
              fprintf (stderr, "]\n");
            }
        }

      if (gid_errno)
	{
	  sprintf (buf, "%s: couldn't set gid to %.100s (%ld)",
		   blurb(),
		   (g && g->gr_name ? g->gr_name : "???"),
		   (long) gid);
	  if (gid_errno == -1)
	    fprintf (stderr, "%s: unknown error\n", buf);
	  else
            {
              errno = gid_errno;
              perror (buf);
            }
	}

      if (uid_errno)
	{
	  sprintf (buf, "%s: couldn't set uid to %.100s (%ld)",
		   blurb(),
		   (p && p->pw_name ? p->pw_name : "???"),
		   (long) uid);
	  if (uid_errno == -1)
	    fprintf(stderr, "%s: unknown error\n", buf);
	  else
            {
              errno = uid_errno;
              perror(buf);
            }
	}

      return -1;
    }
}


/* If we've been run as setuid or setgid to someone else (most likely root)
   turn off the extra permissions so that random user-specified programs
   don't get special privileges.  (On some systems it is necessary to install
   this program as setuid root in order to read the passwd file to implement
   lock-mode.)

     *** WARNING: DO NOT DISABLE ANY OF THE FOLLOWING CODE!
         If you do so, you will open a security hole.
         Do not log in to an X11 desktop as root.
         Log in as a normal users and 'sudo' as needed.
 */
void
disavow_privileges (void)
{
  uid_t euid = geteuid();
  gid_t egid = getegid();
  uid_t uid = getuid();
  gid_t gid = getgid();
  struct passwd *p;

  if (uid != euid || gid != egid)
    {
      if (verbose_p)
        fprintf (stderr, "%s: initial effective uid/gid was %s\n", blurb(),
                 uid_gid_string (euid, egid));
      if (set_ids_by_number (uid, gid) != 0)
        exit (1);  /* already printed error */
    }

  /* If we're still running as root, or if the user we are running at seems
     to be in any way hinky, exit and do not allow password authentication
     to continue.
   */
  uid = getuid ();	/* get it again */
  p = getpwuid (uid);

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
      fprintf (stderr,
               "%s: running as user \"%s\" -- authentication disallowed\n",
               blurb(),
               (p && p->pw_name && *p->pw_name
                ? p->pw_name
                : "<unknown>"));
      exit (1);
    }

  if (verbose_p)
    fprintf (stderr, "%s: running as user \"%s\"\n", blurb(), p->pw_name);
}
