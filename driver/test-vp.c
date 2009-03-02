/* test-xinerama.c --- playing with XF86VidModeGetViewPort
 * xscreensaver, Copyright (c) 2004 Jamie Zawinski <jwz@jwz.org>
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
#include <X11/extensions/xf86vmode.h>
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


static Bool error_handler_hit_p = False;

static int
ignore_all_errors_ehandler (Display *dpy, XErrorEvent *error)
{
  error_handler_hit_p = True;
  return 0;
}


static int
screen_count (Display *dpy)
{
  int n = ScreenCount(dpy);
  int xn = 0;
  int event_number, error_number;

  if (!XineramaQueryExtension (dpy, &event_number, &error_number))
    {
      fprintf(stderr, "%s: XineramaQueryExtension(dpy, ...)    ==> False\n",
              blurb());
      goto DONE;
    }
  else
    fprintf(stderr,   "%s: XineramaQueryExtension(dpy, ...)    ==> %d, %d\n",
            blurb(), event_number, error_number);

  if (!XineramaIsActive(dpy))
    {
      fprintf(stderr, "%s: XineramaIsActive(dpy)               ==> False\n",
              blurb());
      goto DONE;
    }
  else
    {
      int major, minor;
      XineramaScreenInfo *xsi;
      fprintf(stderr, "%s: XineramaIsActive(dpy)               ==> True\n",
              blurb());
      if (!XineramaQueryVersion(dpy, &major, &minor))
        {
          fprintf(stderr,
                  "%s: XineramaQueryVersion(dpy, ...)      ==> False\n",
                  blurb());
          goto DONE;
        }
      else
        fprintf(stderr,
                "%s: XineramaQueryVersion(dpy, ...)      ==> %d, %d\n",
                blurb(), major, minor);

      xsi = XineramaQueryScreens (dpy, &xn);
      if (xsi) XFree (xsi);
    }

 DONE:
  fprintf (stderr, "\n");
  fprintf (stderr, "%s: X client screens: %d\n", blurb(), n);
  fprintf (stderr, "%s: Xinerama screens: %d\n", blurb(), xn);
  fprintf (stderr, "\n");

  if (xn > n) return xn;
  else return n;
}


int
main (int argc, char **argv)
{
  int event_number, error_number;
  int major, minor;
  int nscreens = 0;
  int i;

  XtAppContext app;
  Widget toplevel_shell = XtAppInitialize (&app, progclass, 0, 0,
					   &argc, argv, 0, 0, 0);
  Display *dpy = XtDisplay (toplevel_shell);
  XtGetApplicationNameAndClass (dpy, &progname, &progclass);

  if (!XF86VidModeQueryExtension(dpy, &event_number, &error_number))
    {
      fprintf(stderr, "%s: XF86VidModeQueryExtension(dpy, ...) ==> False\n",
	      blurb());
      fprintf(stderr,
              "%s: server does not support the XF86VidMode extension.\n",
	      blurb());
      exit(1);
    }
  else
    fprintf(stderr, "%s: XF86VidModeQueryExtension(dpy, ...) ==> %d, %d\n",
            blurb(), event_number, error_number);

  if (!XF86VidModeQueryVersion(dpy, &major, &minor))
    {
      fprintf(stderr, "%s: XF86VidModeQueryVersion(dpy, ...) ==> False\n",
              blurb());
      fprintf(stderr,
              "%s: server didn't report XF86VidMode version numbers?\n",
	      blurb());
    }
  else
    fprintf(stderr, "%s: XF86VidModeQueryVersion(dpy, ...)   ==> %d, %d\n",
            blurb(), major, minor);

  nscreens = screen_count (dpy);

  for (i = 0; i < nscreens; i++)
    {
      int result = 0;
      int x = 0, y = 0, dot = 0;
      XF86VidModeModeLine ml = { 0, };
      XErrorHandler old_handler;

      XSync (dpy, False);
      error_handler_hit_p = False;
      old_handler = XSetErrorHandler (ignore_all_errors_ehandler);

      result = XF86VidModeGetViewPort (dpy, i, &x, &y);

      XSync (dpy, False);
      XSetErrorHandler (old_handler);
      XSync (dpy, False);

      if (error_handler_hit_p)
        {
          fprintf(stderr,
                  "%s: XF86VidModeGetViewPort(dpy, %d, ...) ==> X error\n",
                  blurb(), i);
          continue;
        }

      if (! result)
        fprintf(stderr, "%s: XF86VidModeGetViewPort(dpy, %d, ...) ==> %d\n",
                blurb(), i, result);

      result = XF86VidModeGetModeLine (dpy, i, &dot, &ml);
      if (! result)
        fprintf(stderr, "%s: XF86VidModeGetModeLine(dpy, %d, ...) ==> %d\n",
                blurb(), i, result);

      fprintf (stderr, "%s:   screen %d: %dx%d; viewport: %dx%d+%d+%d\n",
               blurb(), i,
               DisplayWidth (dpy, i), DisplayHeight (dpy, i),
               ml.hdisplay, ml.vdisplay, x, y
               );

      fprintf (stderr,
               "%s:       hsync start %d; end %d; total %d; skew  %d;\n",
               blurb(),
               ml.hsyncstart, ml.hsyncend, ml.htotal, ml.hskew);
      fprintf (stderr,
               "%s:       vsync start %d; end %d; total %d; flags 0x%04x;\n",
               blurb(),
               ml.vsyncstart, ml.vsyncend, ml.vtotal, ml.flags);
      fprintf (stderr, "\n");
    }
  XSync (dpy, False);
  exit (0);
}
