/* test-xinerama.c --- playing with the Xinerama extension.
 * xscreensaver, Copyright (c) 2003 Jamie Zawinski <jwz@jwz.org>
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
#include <time.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Intrinsic.h>

#include <X11/Xproto.h>
#include <X11/extensions/Xinerama.h>

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


int
main (int argc, char **argv)
{
  int event_number, error_number;
  int major, minor;
  int nscreens = 0;
  XineramaScreenInfo *xsi;
  int i;

  XtAppContext app;
  Widget toplevel_shell = XtAppInitialize (&app, progclass, 0, 0,
					   &argc, argv, 0, 0, 0);
  Display *dpy = XtDisplay (toplevel_shell);
  XtGetApplicationNameAndClass (dpy, &progname, &progclass);

  if (!XineramaQueryExtension(dpy, &event_number, &error_number))
    {
      fprintf(stderr, "%s: XineramaQueryExtension(dpy, ...) ==> False\n",
	      blurb());
      fprintf(stderr, "%s: server does not support the Xinerama extension.\n",
	      blurb());
      exit(1);
    }
  else
    fprintf(stderr, "%s: XineramaQueryExtension(dpy, ...) ==> %d, %d\n",
            blurb(), event_number, error_number);

  if (!XineramaIsActive(dpy))
    {
      fprintf(stderr, "%s: XineramaIsActive(dpy) ==> False\n", blurb());
      fprintf(stderr, "%s: server says Xinerama is turned off.\n", blurb());
      exit(1);
    }
  else
    fprintf(stderr, "%s: XineramaIsActive(dpy) ==> True\n", blurb());

  if (!XineramaQueryVersion(dpy, &major, &minor))
    {
      fprintf(stderr, "%s: XineramaQueryVersion(dpy, ...) ==> False\n",
              blurb());
      fprintf(stderr, "%s: server didn't report Xinerama version numbers?\n",
	      blurb());
    }
  else
    fprintf(stderr, "%s: XineramaQueryVersion(dpy, ...) ==> %d, %d\n", blurb(),
	    major, minor);

  xsi = XineramaQueryScreens (dpy, &nscreens);
  fprintf(stderr, "%s: %d Xinerama screens\n", blurb(), nscreens);
  
  for (i = 0; i < nscreens; i++)
    fprintf (stderr, "%s:   screen %d: %dx%d+%d+%d\n",
             blurb(),
             xsi[i].screen_number,
             xsi[i].width, xsi[i].height,
             xsi[i].x_org, xsi[i].y_org);
  XFree (xsi);
  XSync (dpy, False);
  exit (0);
}
