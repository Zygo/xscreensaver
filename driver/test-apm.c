/* test-apm.c --- playing with the APM library.
 * xscreensaver, Copyright (c) 1999 Jamie Zawinski <jwz@jwz.org>
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

#include <stdio.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>

#include <apm.h>

#define countof(x) (sizeof((x))/sizeof(*(x)))


char *progname = 0;
char *progclass = "XScreenSaver";

static const char *
blurb (void)
{
  static char buf[255];
  time_t now = time ((time_t *) 0);
  char *ct = (char *) ctime (&now);
  int n = strlen(progname);
  if (n > 100) n = 99;
  strncpy(buf, progname, n);
  buf[n++] = ':';
  buf[n++] = ' ';
  strncpy(buf+n, ct+11, 8);
  strcpy(buf+n+9, ": ");
  return buf;
}

static void
apm_cb (XtPointer closure, int *fd, XtInputId *id)
{
  apm_event_t events[100];
  int n, i;
  while ((n = apm_get_events (*fd, 0, events, countof(events)))
         > 0)
    for (i = 0; i < n; i++)
      {
        fprintf (stderr, "%s: APM event 0x%x: %s.\n", blurb(),
                 events[i], apm_event_name (events[i]));
#if 0
        switch (events[i])
          {
          case APM_SYS_STANDBY:
          case APM_USER_STANDBY:
          case APM_SYS_SUSPEND:
          case APM_USER_SUSPEND:
          case APM_CRITICAL_SUSPEND:
            break;
          }
#endif
      }
}

int
main (int argc, char **argv)
{
  XtAppContext app;
  Widget toplevel_shell = XtAppInitialize (&app, progclass, 0, 0,
					   &argc, argv, 0, 0, 0);
  Display *dpy = XtDisplay (toplevel_shell);
  int fd;
  XtInputId id;
  XtGetApplicationNameAndClass (dpy, &progname, &progclass);

  fd = apm_open ();
  if (fd <= 0)
    {
      fprintf (stderr, "%s: couldn't initialize APM.\n", blurb());
      exit (1);
    }

  id = XtAppAddInput(app, fd,
                     (XtPointer) (XtInputReadMask | XtInputWriteMask),
                     apm_cb, 0);
  XtAppMainLoop (app);
  exit (0);
}
