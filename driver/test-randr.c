/* test-randr.c --- playing with the Resize And Rotate extension.
 * xscreensaver, Copyright © 2004-2022 Jamie Zawinski <jwz@jwz.org>
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

#include "blurb.h"
char *progclass = "XScreenSaver";

static Bool error_handler_hit_p = False;

static int
ignore_all_errors_ehandler (Display *dpy, XErrorEvent *error)
{
  error_handler_hit_p = True;
  return 0;
}


static void
query_outputs (Display *dpy, int screen)
{
# ifdef HAVE_RANDR_12
  int major = -1, minor = -1;

  if (!XRRQueryVersion(dpy, &major, &minor))
    abort();

  if (major > 1 || (major == 1 && minor >= 2))
    {
      int j;
      XRRScreenResources *res = 
        XRRGetScreenResources (dpy, RootWindow (dpy, screen));
      fprintf (stderr, "\n");
      for (j = 0; j < res->noutput; j++)
        {
          int k;
          XRROutputInfo *rroi = XRRGetOutputInfo (dpy, res, res->outputs[j]);
          fprintf (stderr, "%s:   Output %d: %s: %s (%d)\n", blurb(), j,
                   rroi->name,
                   (rroi->connection == RR_Connected         ? "connected" :
                    rroi->connection == RR_Disconnected      ? "disconnected" :
                    rroi->connection == RR_UnknownConnection ? "unknown" :
                    "ERROR"),
                   (int) rroi->crtc);
          for (k = 0; k < rroi->ncrtc; k++)
            {
              XRRCrtcInfo *crtci = XRRGetCrtcInfo (dpy, res, rroi->crtcs[k]);
              fprintf(stderr, "%s:   %c CRTC %d (%d): %dx%d+%d+%d\n", 
                      blurb(),
                      (rroi->crtc == rroi->crtcs[k] ? '+' : ' '),
                      k, (int) rroi->crtcs[k],
                      crtci->width, crtci->height, crtci->x, crtci->y);
              XRRFreeCrtcInfo (crtci);
            }
          XRRFreeOutputInfo (rroi);
          fprintf (stderr, "\n");
        }
      XRRFreeScreenResources (res);
    }
# endif /* HAVE_RANDR_12 */
}


int
main (int argc, char **argv)
{
  int event_number = -1, error_number = -1;
  int major = -1, minor = -1;
  int nscreens = 0;
  int i;

  XtAppContext app;
  Widget toplevel_shell = XtAppInitialize (&app, progclass, 0, 0,
					   &argc, argv, 0, 0, 0);
  Display *dpy = XtDisplay (toplevel_shell);

  progname = argv[0];
  nscreens = ScreenCount(dpy);

  if (!XRRQueryExtension(dpy, &event_number, &error_number))
    {
      fprintf(stderr, "%s: XRRQueryExtension(dpy, ...) ==> False\n",
	      blurb());
      fprintf(stderr, "%s: server does not support the RANDR extension\n",
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
      XRRScreenConfiguration *rrc;
      XErrorHandler old_handler;

      XSync (dpy, False);
      error_handler_hit_p = False;
      old_handler = XSetErrorHandler (ignore_all_errors_ehandler);

      rrc = (major >= 0 ? XRRGetScreenInfo (dpy, RootWindow (dpy, i)) : 0);

      XSync (dpy, False);
      XSetErrorHandler (old_handler);
      XSync (dpy, False);

      if (error_handler_hit_p)
        {
          fprintf(stderr, "%s:   XRRGetScreenInfo(dpy, %d) ==> X error:\n",
                  blurb(), i);
          /* do it again without the error handler to print the error */
          rrc = XRRGetScreenInfo (dpy, RootWindow (dpy, i));
        }
      else if (rrc)
        {
          SizeID current_size = -1;
          Rotation current_rotation = ~0;

          fprintf (stderr, "\n%s: Screen %d\n", blurb(), i);

          current_size =
            XRRConfigCurrentConfiguration (rrc, &current_rotation);

          /* Times */
# if 0    /* #### This is wrong -- I don't understand what these two
                  timestamp numbers represent, or how they correlate
                  to the wall clock or to each other. */
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
# endif

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

      query_outputs (dpy, i);
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

      fprintf (stderr, "\n%s: awaiting events...\n\n"
          "\t(If you resize the screen or add/remove monitors, this should\n"
          "\tnotice that and print stuff.  Otherwise, hit ^C.)\n\n",
               blurb());
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
                       blurb(), screen);

              fprintf (stderr, "%s: screen %d: old size: \t%d x %d\n",
                       blurb(), screen,
                       DisplayWidth (dpy, screen),
                       DisplayHeight (dpy, screen));
              fprintf (stderr, "%s: screen %d: old root: \t%d x %d\t0x%lx\n",
                       blurb(), screen,
                       xgwa[screen].width, xgwa[screen].height,
                       (unsigned long) w[screen]);

              XRRUpdateConfiguration (&event);
              XSync (dpy, False);

              fprintf (stderr, "%s: screen %d: new size: \t%d x %d\n",
                       blurb(), screen,
                       DisplayWidth (dpy, screen),
                       DisplayHeight (dpy, screen));

              w[screen] = RootWindow (dpy, screen);
              XGetWindowAttributes (dpy, w[screen], &xgwa[screen]);
              fprintf (stderr, "%s: screen %d: new root:\t%d x %d\t0x%lx\n",
                       blurb(), screen,
                       xgwa[screen].width, xgwa[screen].height,
                       (unsigned long) w[screen]);

              for (i = 0; i < nscreens; i++)
                query_outputs (dpy, i);
            }
          else
            {
              fprintf (stderr, "%s: event %d\n", blurb(), event.type);
            }
        }
    }

  XSync (dpy, False);
  exit (0);
}
