/* test-xkb.c --- playing with the X Keyboard extension.
 * xscreensaver, Copyright © 2021 Jamie Zawinski <jwz@jwz.org>
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
#include <X11/XKBlib.h>

#include "blurb.h"

char *progclass = "XScreenSaver";

int
main (int argc, char **argv)
{
  XtAppContext app;
  Widget toplevel_shell;
  Display *dpy;
  int i;

  progname = argv[0];

  for (i = 1; i < argc; i++)
    {
      const char *oa = argv[i];
      if (argv[i][0] == '-' && argv[i][1] == '-')
        argv[i]++;
      if (0) ;
      else
        {
          fprintf (stderr, "%s: unknown option: %s\n", blurb(), oa);
          exit (1);
        }
    }

  toplevel_shell = XtAppInitialize (&app, progclass, 0, 0, 
                                    &argc, argv, 0, 0, 0);
  dpy = XtDisplay (toplevel_shell);
  if (!dpy) exit(1);

  {
    XkbStateRec state;
    XkbDescPtr desc;
    char *group;
    Atom name;

    if (XkbGetState (dpy, XkbUseCoreKbd, &state))
      {
        fprintf (stderr, "%s: XkbGetState failed\n", progname);
        exit (1);
      }
    desc = XkbGetKeyboard (dpy, XkbAllComponentsMask, XkbUseCoreKbd);
    if (!desc || !desc->names)
      {
        fprintf (stderr, "%s: XkbGetKeyboard failed\n", progname);
        exit (1);
      }

    name = desc->names->groups[state.group];
    group = (name ? XGetAtomName (dpy, name) : strdup("NULL"));
    fprintf (stderr, "%s: kbd name: %s\n", progname, group);
    free (group);
  }

  exit (0);
}
