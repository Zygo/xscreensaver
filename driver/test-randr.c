/* test-randr.c --- playing with the Resize And Rotate extension.
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
#include <X11/extensions/Xrandr.h>

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
  int i;

  XtAppContext app;
  Widget toplevel_shell = XtAppInitialize (&app, progclass, 0, 0,
					   &argc, argv, 0, 0, 0);
  Display *dpy = XtDisplay (toplevel_shell);
  XtGetApplicationNameAndClass (dpy, &progname, &progclass);

  nscreens = ScreenCount(dpy);

  if (!XRRQueryExtension(dpy, &event_number, &error_number))
    {
      fprintf(stderr, "%s: XRRQueryExtension(dpy, ...) ==> False\n",
	      blurb());
      fprintf(stderr, "%s: server does not support the RANDR extension.\n",
	      blurb());
      major = -1;
    }
  else
    {
      fprintf(stderr, "%s: XRRQueryExtension(dpy, ...) ==> %d, %d\n",
              blurb(), event_number, error_number);

      if (!XRRQueryVersion(dpy, &major, &minor))
        {
          fprintf(stderr, "%s: XRRQueryVersion(dpy, ...) ==> False\n",
                  blurb());
          fprintf(stderr, "%s: server didn't report RANDR version numbers?\n",
                  blurb());
        }
      else
        fprintf(stderr, "%s: XRRQueryVersion(dpy, ...) ==> %d, %d\n", blurb(),
                major, minor);
    }

  for (i = 0; i < nscreens; i++)
    {
      XRRScreenConfiguration *rrc =
        (major >= 0 ? XRRGetScreenInfo (dpy, RootWindow (dpy, i)) : 0);

      if (rrc)
        {
          SizeID current_size = -1;
          Rotation current_rotation = ~0;

          fprintf (stderr, "\n%s: Screen %d\n", blurb(), i);

          current_size =
            XRRConfigCurrentConfiguration (rrc, &current_rotation);

          /* Times */
          {
            Time server_time, config_time;
            server_time = XRRConfigTimes (rrc, &config_time);
            if (config_time == 0 || server_time == 0)
              fprintf (stderr, "%s:   config has never been changed\n",
                       blurb());
            else
              fprintf (stderr, "%s:   config changed %lu seconds ago\n",
                       blurb(), (unsigned long) (server_time - config_time));
          }

          /* Rotations */
          {
            Rotation available, current;
            available = XRRConfigRotations (rrc, &current);

            fprintf (stderr, "%s:   Available Rotations:\t", blurb());
            if (available & RR_Rotate_0)   fprintf (stderr,   " 0");
            if (available & RR_Rotate_90)  fprintf (stderr,  " 90");
            if (available & RR_Rotate_180) fprintf (stderr, " 180");
            if (available & RR_Rotate_270) fprintf (stderr, " 270");
            if (! (available & (RR_Rotate_0   | RR_Rotate_90 |
                                RR_Rotate_180 | RR_Rotate_270)))
              fprintf (stderr, " none");
            fprintf (stderr, "\n");

            if (current_rotation != current)
              fprintf (stderr,
                       "%s:   WARNING: rotation inconsistency: 0x%X vs 0x%X\n",
                       blurb(), current_rotation, current);

            fprintf (stderr, "%s:   Current Rotation:\t", blurb());
            if (current & RR_Rotate_0)   fprintf (stderr,   " 0");
            if (current & RR_Rotate_90)  fprintf (stderr,  " 90");
            if (current & RR_Rotate_180) fprintf (stderr, " 180");
            if (current & RR_Rotate_270) fprintf (stderr, " 270");
            if (! (current & (RR_Rotate_0   | RR_Rotate_90 |
                              RR_Rotate_180 | RR_Rotate_270)))
              fprintf (stderr, " none");
            fprintf (stderr, "\n");

            fprintf (stderr, "%s:   Available Reflections:\t", blurb());
            if (available & RR_Reflect_X) fprintf (stderr,   " X");
            if (available & RR_Reflect_Y) fprintf (stderr,   " Y");
            if (! (available & (RR_Reflect_X | RR_Reflect_Y)))
              fprintf (stderr, " none");
            fprintf (stderr, "\n");

            fprintf (stderr, "%s:   Current Reflections:\t", blurb());
            if (current & RR_Reflect_X) fprintf (stderr,   " X");
            if (current & RR_Reflect_Y) fprintf (stderr,   " Y");
            if (! (current & (RR_Reflect_X | RR_Reflect_Y)))
              fprintf (stderr, " none");
            fprintf (stderr, "\n");
          }

          /* Sizes */
          {
            int nsizes, j;
            XRRScreenSize *rrsizes;

            rrsizes = XRRConfigSizes (rrc, &nsizes);
            if (nsizes <= 0)
                fprintf (stderr, "%s:   sizes:\t none\n", blurb());
            else
              for (j = 0; j < nsizes; j++)
                {
                  short *rates;
                  int nrates, k;
                  fprintf (stderr,
                           "%s:   %c size %d: %d x %d\t rates:",
                           blurb(), 
                           (j == current_size ? '+' : ' '),
                           j,
                           rrsizes[j].width, rrsizes[j].height);

                  rates = XRRConfigRates (rrc, j, &nrates);
                  if (nrates == 0)
                    fprintf (stderr, " none?");
                  else
                    for (k = 0; k < nrates; k++)
                      fprintf (stderr, " %d", rates[k]);
                  fprintf (stderr, "\n");
                  /* don't free 'rates' */
                }
            /* don't free 'rrsizes' */
          }

          XRRFreeScreenConfigInfo (rrc);
        }
      else if (major >= 0)
        {
          fprintf(stderr, "%s:   XRRGetScreenInfo(dpy, %d) ==> NULL\n",
                  blurb(), i);
        }
    }

  if (major > 0)
    {
      Window w[20];
      XWindowAttributes xgwa[20];

      for (i = 0; i < nscreens; i++)
        {
          XRRSelectInput (dpy, RootWindow (dpy, i), RRScreenChangeNotifyMask);
          w[i] = RootWindow (dpy, i);
          XGetWindowAttributes (dpy, w[i], &xgwa[i]);
        }

      XSync (dpy, False);
      fprintf (stderr, "\n%s: awaiting events...\n", progname);
      while (1)
        {
	  XEvent event;
	  XNextEvent (dpy, &event);

          if (event.type == event_number + RRScreenChangeNotify)
            {
              XRRScreenChangeNotifyEvent *xrr_event =
                (XRRScreenChangeNotifyEvent *) &event;
              int screen = XRRRootToScreen (dpy, xrr_event->window);

              fprintf (stderr, "%s: screen %d: RRScreenChangeNotify event\n",
                       progname, screen);

              fprintf (stderr, "%s: screen %d: old size: \t%d x %d\n",
                       progname, screen,
                       DisplayWidth (dpy, screen),
                       DisplayHeight (dpy, screen));
              fprintf (stderr, "%s: screen %d: old root 0x%lx:\t%d x %d\n",
                       progname, screen, (unsigned long) w[screen],
                       xgwa[screen].width, xgwa[screen].height);

              XRRUpdateConfiguration (&event);
              XSync (dpy, False);

              fprintf (stderr, "%s: screen %d: new size: \t%d x %d\n",
                       progname, screen,
                       DisplayWidth (dpy, screen),
                       DisplayHeight (dpy, screen));

              w[screen] = RootWindow (dpy, screen);
              XGetWindowAttributes (dpy, w[screen], &xgwa[screen]);
              fprintf (stderr, "%s: screen %d: new root 0x%lx:\t%d x %d\n",
                       progname, screen, (unsigned long) w[screen],
                       xgwa[screen].width, xgwa[screen].height);
              fprintf (stderr, "\n");
            }
          else
            {
              fprintf (stderr, "%s: event %d\n", progname, event.type);
            }
        }
    }

  XSync (dpy, False);
  exit (0);
}
