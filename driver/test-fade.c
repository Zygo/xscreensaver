/* test-fade.c --- playing with colormap and/or gamma fading.
 * xscreensaver, Copyright © 2001-2025 Jamie Zawinski <jwz@jwz.org>
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

#include <X11/Xatom.h>
#include <X11/Intrinsic.h>

#include "xscreensaver.h"
#include "resources.h"
#include "screens.h"
#include "screenshot.h"
#include "fade.h"
#include "atoms.h"

#ifdef HAVE_XF86VMODE_GAMMA
# include <X11/extensions/xf86vmode.h>
#endif
#ifdef HAVE_RANDR
# include <X11/extensions/Xrandr.h>
#endif

XrmDatabase db = 0;
char *progclass = "XScreenSaver";
Bool debug_p = True;

#define XF86_VIDMODE_NAME "XFree86-VidModeExtension"
#define RANDR_NAME "RANDR"

int
main (int argc, char **argv)
{
  double seconds = 3;
  double ratio = 1/3.0;
  int delay = 2;

  int op, event, error, major, minor;

  XtAppContext app;
  Widget toplevel_shell = XtAppInitialize (&app, progclass, 0, 0,
					   &argc, argv, 0, 0, 0);
  Display *dpy = XtDisplay (toplevel_shell);
  Screen *screen = ScreenOfDisplay (dpy, 0);
  int nwindows, i;
  Window windows[100];

  int x, y;
  unsigned int bw, d;
  Window root = RootWindow (dpy, 0);
  Visual *visual = DefaultVisual (dpy, 0);
  Pixmap logo, logo_clipmask;
  Pixmap screenshot;
  int logo_npixels;
  unsigned long *logo_pixels;
  unsigned int logo_width, logo_height;
  XSetWindowAttributes attrs;
  unsigned long attrmask = 0;
  void *state = 0;

  verbose_p += 2;

  progname = argv[0];
  progclass = "XScreenSaver";
  db = XtDatabase (dpy);

  init_xscreensaver_atoms (dpy);

  fprintf (stderr, "\n%s:\n"
           "\n"
           "\tYou should see the following, with no flickering:\n"
           "\n"
           "\t- Plain desktop;\n"
           "\t- Four black rectangles slowly fade in (about 4 seconds);\n"
           "\t- Once they are solid black, XScreenSaver logos flash in;\n"
           "\t- Those logos fade out to solid black (about 2 seconds);\n"
           "\t- Those black squares fade to desktop (about 2 seconds);\n"
           "\t- Plain desktop (about 1 second);\n"
           "\t- Repeat.\n"
           "\n",
           blurb());

  {
    const char * version_number = "test-fade";
    Window daemon_window =
      XCreateWindow (dpy, RootWindow (dpy, 0),
                     0, 0, 1, 1, 0,
                     DefaultDepth (dpy, 0), InputOutput,
                     DefaultVisual (dpy, 0), attrmask, &attrs);
    XChangeProperty (dpy, daemon_window, XA_SCREENSAVER_VERSION, XA_STRING,
                     8, PropModeReplace, (unsigned char *) version_number,
                     strlen (version_number));
  }



  if (!XQueryExtension (dpy, XF86_VIDMODE_NAME, &op, &event, &error))
    fprintf(stderr, "%s: no " XF86_VIDMODE_NAME " extension\n", blurb());
  else
    {
# ifdef HAVE_XF86VMODE_GAMMA
      if (!XF86VidModeQueryVersion (dpy, &major, &minor))
        fprintf(stderr, "%s: unable to get " XF86_VIDMODE_NAME " version\n",
                blurb());
      else
        fprintf(stderr, "%s: " XF86_VIDMODE_NAME " version %d.%d\n",
                blurb(), major, minor);
# else /* !HAVE_XF86VMODE_GAMMA */
      fprintf(stderr, "%s: no support for display's " XF86_VIDMODE_NAME
              " extension\n", blurb());
# endif /* !HAVE_XF86VMODE_GAMMA */
    }

  if (!XQueryExtension (dpy, RANDR_NAME, &op, &event, &error))
    fprintf(stderr, "%s: no " RANDR_NAME " extension\n", blurb());
  else
    {
# ifdef HAVE_RANDR
      if (!XRRQueryVersion (dpy, &major, &minor))
        fprintf(stderr, "%s: unable to get " RANDR_NAME " version\n",
                blurb());
      else
        fprintf(stderr, "%s: " RANDR_NAME " version %d.%d\n",
                blurb(), major, minor);
# else /* !HAVE_RANDR */
      fprintf(stderr, "%s: no support for display's " RANDR_NAME
              " extension\n", blurb());
# endif /* !HAVE_RANDR */
    }

  logo = xscreensaver_logo (screen, visual, root, DefaultColormap (dpy, 0),
                            WhitePixel (dpy, 0), 
                            &logo_pixels, &logo_npixels,
                            &logo_clipmask, True);
  XGetGeometry (dpy, logo, &root, &x, &y, &logo_width, &logo_height, &bw, &d);

  fprintf (stderr, "\n%s: grabbing shared screenshot\n", blurb());
  screenshot = screenshot_grab (dpy, root, True, True);
  fprintf (stderr, "\n");

  nwindows = 0;
  {
    int x, y;
    for (y = 0; y < 2; y++)
      for (x = 0; x < 2; x++)
        {
          int win_width = 250;
          int win_height = 200;

          attrmask = CWOverrideRedirect;
          attrs.override_redirect = True;
          windows[nwindows] =
            XCreateWindow (dpy, root,
                           200 + x * win_width * 1.5,
                           200 + y * win_height * 1.5,
                           win_width, win_height, 0, DefaultDepth (dpy, 0),
                           InputOutput, visual,
                           attrmask, &attrs);
          XSetWindowBackground (dpy, windows[nwindows], BlackPixel (dpy, 0));
          XClearWindow (dpy, windows[nwindows]);
          if (screenshot)
            {
              fprintf (stderr, "%s: saving screenshot 0x%0lX on 0x%lX\n",
                       blurb(), (unsigned long) screenshot, windows[nwindows]);
              screenshot_save (dpy, windows[nwindows], screenshot);
            }
          nwindows++;
        }
  }

  fprintf (stderr, "\n%s: fading %d screen%s\n\n",
           blurb(), ScreenCount(dpy), ScreenCount(dpy) == 1 ? "" : "s");

  while (1)
    {
      XSync (dpy, False);

      fprintf(stderr, "%s: fading out\n\n", blurb());
      fflush(stderr);
      fade_screens (app, dpy, windows, nwindows, seconds,
                    True,  /* out_p */
                    True,  /* from_desktop_p */
                    &state);
      for (i = 0; i < nwindows; i++)
        XMapRaised (dpy, windows[i]);
      XSync (dpy, False);
      fprintf(stderr, "%s: out done\n\n", blurb());
      fflush(stderr);

      for (i = 0; i < nwindows; i++)
        {
          XSetWindowBackgroundPixmap (dpy, windows[i], logo);
          XClearWindow (dpy, windows[i]);
          XSetWindowBackground (dpy, windows[i], BlackPixel (dpy, 0));
        }
      XSync (dpy, False);

      if (delay) sleep (delay);

      fprintf(stderr, "%s: fading in\n\n", blurb());
      fflush(stderr);
      fade_screens (app, dpy, windows, nwindows,
                    seconds * ratio,
                    True,  /* out_p */
                    False, /* from_desktop_p */
                    &state);
      fade_screens (app, dpy, windows, nwindows, 
                    seconds * ratio,
                    False, /* out_p */
                    False, /* from_desktop_p */
                    &state);
      XSync (dpy, False);
      fprintf(stderr, "%s: in done\n\n", blurb());
      fflush(stderr);

      if (delay) sleep (delay);
    }
}
