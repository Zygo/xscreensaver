/* test-uid.c --- playing with setuid.
 * xscreensaver, Copyright (c) 1998, 2005 Jamie Zawinski <jwz@jwz.org>
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

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>

static void
print(void)
{
  int uid = getuid();
  int gid = getgid();
  int euid = geteuid();
  int egid = getegid();
  struct passwd *p = 0;
  struct group *g = 0;
  gid_t groups[1024];
  int n, size;

  p = getpwuid (uid);
  g = getgrgid (gid);
  fprintf(stderr, "real user/group: %ld/%ld (%s/%s)\n", (long) uid, (long) gid,
	  (p && p->pw_name ? p->pw_name : "???"),
	  (g && g->gr_name ? g->gr_name : "???"));

  p = getpwuid (euid);
  g = getgrgid (egid);
  fprintf(stderr, "eff. user/group: %ld/%ld (%s/%s)\n", (long)euid, (long)egid,
	  (p && p->pw_name ? p->pw_name : "???"),
	  (g && g->gr_name ? g->gr_name : "???"));

  size = sizeof(groups) / sizeof(gid_t);
  n = getgroups(size - 1, groups);
  if (n < 0)
    perror("getgroups failed");
  else
    {
      int i;
      fprintf (stderr, "eff. group list: [");
      for (i = 0; i < n; i++)
        {
          g = getgrgid (groups[i]);
          fprintf(stderr, "%s%s=%ld", (i == 0 ? "" : ", "),
                  (g->gr_name ? g->gr_name : "???"),
                  (long) groups[i]);
        }
      fprintf (stderr, "]\n");
    }
}

int
main (int argc, char **argv)
{
  int i;
  struct passwd *p = 0;
  struct group *g = 0;

  if (argc <= 1)
    {
      fprintf(stderr,
	      "usage: %s [ user/group ... ]\n"
	      "\tEach argument may be a user name, or user/group.\n"
	      "\tThis program will attempt to setuid/setgid to each\n"
	      "\tin turn, and report the results.  The user and group\n"
	      "\tnames may be strings, or numeric.\n",
	      argv[0]);
      exit(1);
    }

  print();
  for (i = 1; i < argc; i++)
    {
      char *user = argv[i];
      char *group = strchr(user, '/');
      if (!group)
	group = strchr(user, '.');
      if (group)
	*group++ = 0;

      if (group && *group)
	{
	  long gid = 0;
	  int was_numeric = 0;

	  g = 0;
	  if (*group == '-' || (*group >= '0' && *group <= '9'))
	    if (1 == sscanf(group, "%ld", &gid))
	      {
		g = getgrgid (gid);
		was_numeric = 1;
	      }

	  if (!g)
	    g = getgrnam(group);

	  if (g)
	    {
	      gid = g->gr_gid;
	      group = g->gr_name;
	    }
	  else
	    {
	      if (was_numeric)
		{
		  fprintf(stderr, "no group numbered %s.\n", group);
		  group = "";
		}
	      else
		{
		  fprintf(stderr, "no group named %s.\n", group);
		  goto NOGROUP;
		}
	    }

	  fprintf(stderr, "setgroups(1, [%ld]) \"%s\"", gid, group);
          {
            gid_t g2 = gid;
            if (setgroups(1, &g2) == 0)
              fprintf(stderr, " succeeded.\n");
            else
              perror(" failed");
          }

	  fprintf(stderr, "setgid(%ld) \"%s\"", gid, group);
	  if (setgid(gid) == 0)
	    fprintf(stderr, " succeeded.\n");
	  else
	    perror(" failed");

	NOGROUP: ;
	}

      if (user && *user)
	{
	  long uid = 0;
	  int was_numeric = 0;

	  p = 0;
	  if (*user == '-' || (*user >= '0' && *user <= '9'))
	    if (1 == sscanf(user, "%ld", &uid))
	      {
		p = getpwuid (uid);
		was_numeric = 1;
	      }

	  if (!p)
	    p = getpwnam(user);

	  if (p)
	    {
	      uid = p->pw_uid;
	      user = p->pw_name;
	    }
	  else
	    {
	      if (was_numeric)
		{
		  fprintf(stderr, "no user numbered \"%s\".\n", user);
		  user = "";
		}
	      else
		{
		  fprintf(stderr, "no user named %s.\n", user);
		  goto NOUSER;
		}
	    }

	  fprintf(stderr, "setuid(%ld) \"%s\"", uid, user);
	  if (setuid(uid) == 0)
	    fprintf(stderr, " succeeded.\n");
	  else
	    perror(" failed");
	NOUSER: ;
	}
      print();
    }

  fprintf(stderr,
	  "running \"whoami\" and \"groups\" in a sub-process reports:\n");
  fflush(stdout);
  fflush(stderr);
  system ("/bin/sh -c 'echo \"`whoami` / `groups`\"'");

  fflush(stdout);
  fflush(stderr);
  exit(0);
}
