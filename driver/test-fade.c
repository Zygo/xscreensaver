/* test-fade.c --- playing with colormap and/or gamma fading.
 * xscreensaver, Copyright (c) 2001 Jamie Zawinski <jwz@jwz.org>
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

#include <X11/Intrinsic.h>
#include "xscreensaver.h"
#include "fade.h"

#ifdef HAVE_SGI_VC_EXTENSION
# include <X11/extensions/XSGIvc.h>
#endif
#ifdef HAVE_XF86VMODE_GAMMA
# include <X11/extensions/xf86vmode.h>
#endif

XrmDatabase db = 0;
char *progname = 0;
char *progclass = "XScreenSaver";

#define SGI_VC_NAME "SGI-VIDEO-CONTROL"
#define XF86_VIDMODE_NAME "XFree86-VidModeExtension"

int
main (int argc, char **argv)
{
  int seconds = 3;
  int ticks = 20;
  int delay = 1;

  int op, event, error, major, minor;

  XtAppContext app;
  Widget toplevel_shell = XtAppInitialize (&app, progclass, 0, 0,
					   &argc, argv, 0, 0, 0);
  Display *dpy = XtDisplay (toplevel_shell);
  Colormap *current_maps;
  int i;

  XtGetApplicationNameAndClass (dpy, &progname, &progclass);
  db = XtDatabase (dpy);

  current_maps = (Colormap *) calloc(sizeof(Colormap), ScreenCount(dpy));
  for (i = 0; i < ScreenCount(dpy); i++)
    current_maps[i] = DefaultColormap (dpy, i);

  if (!XQueryExtension (dpy, SGI_VC_NAME, &op, &event, &error))
    fprintf(stderr, "%s: no " SGI_VC_NAME " extension\n", progname);
  else
    {
# ifdef HAVE_SGI_VC_EXTENSION
      if (!XSGIvcQueryVersion (dpy, &major, &minor))
        fprintf(stderr, "%s: unable to get " SGI_VC_NAME " version\n",
                progname);
      else
        fprintf(stderr, "%s: " SGI_VC_NAME " version %d.%d\n",
                progname, major, minor);
# else /* !HAVE_SGI_VC_EXTENSION */
      fprintf(stderr, "%s: no support for display's " SGI_VC_NAME
              " extension\n", progname);
# endif /* !HAVE_SGI_VC_EXTENSION */
    }


  if (!XQueryExtension (dpy, XF86_VIDMODE_NAME, &op, &event, &error))
    fprintf(stderr, "%s: no " XF86_VIDMODE_NAME " extension\n", progname);
  else
    {
# ifdef HAVE_XF86VMODE_GAMMA
      if (!XF86VidModeQueryVersion (dpy, &major, &minor))
        fprintf(stderr, "%s: unable to get " XF86_VIDMODE_NAME " version\n",
                progname);
      else
        fprintf(stderr, "%s: " XF86_VIDMODE_NAME " version %d.%d\n",
                progname, major, minor);
# else /* !HAVE_XF86VMODE_GAMMA */
      fprintf(stderr, "%s: no support for display's " XF86_VIDMODE_NAME
              " extension\n", progname);
# endif /* !HAVE_XF86VMODE_GAMMA */
    }

  fprintf (stderr, "%s: fading %d screen%s\n",
           progname, ScreenCount(dpy), ScreenCount(dpy) == 1 ? "" : "s");

  while (1)
    {
      XSync (dpy, False);

      fprintf(stderr, "%s: out...", progname);
      fflush(stderr);
      fade_screens (dpy, current_maps, 0, seconds, ticks, True, False);
      fprintf(stderr, "done.\n");
      fflush(stderr);

      if (delay) sleep (delay);

      fprintf(stderr,"%s: in...", progname);
      fflush(stderr);
      fade_screens (dpy, current_maps, 0, seconds, ticks, False, False);
      fprintf(stderr, "done.\n");
      fflush(stderr);

      if (delay) sleep (delay);
    }
}
