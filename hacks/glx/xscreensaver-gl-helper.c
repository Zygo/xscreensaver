/* xscreensaver, Copyright (c) 2000 by Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* xscreensaver-gl-helper -- prints the ID of the best visual to use
   for GL programs on stdout.
 */

#include "utils.h"
#include "visual.h"

#include <GL/gl.h>
#include <GL/glx.h>

char *progname = 0;
char *progclass = "XScreenSaver";
void *db = 0; /* hack hack -- no need for Xt in this program */

int
main (int argc, char **argv)
{
  Display *dpy;
  Screen *screen;
  Visual *visual;
  char *d = getenv ("DISPLAY");
  int i;

  progname = argv[0];
  for (i = 1; i < argc; i++)
    {
      if (argv[i][0] == '-' && argv[i][1] == '-') argv[i]++;
      if (strlen(argv[i]) >= 2 &&
          !strncmp ("-display", argv[i], strlen(argv[i])))
        {
          if (i == argc-1) goto LOSE;
          d = argv[i+1];
          i++;
        }
      else
        {
         LOSE:
          fprintf (stderr, "usage: %s [ -display host:dpy.screen ]\n",
                   progname);
          fprintf (stderr,
                   "This program prints out the ID of the best "
                   "X visual for GL programs to use.\n");
          exit (1);
        }
    }

  dpy = XOpenDisplay (d);
  if (!dpy)
    {
      fprintf (stderr, "%s: couldn't open display %s\n", progname,
               (d ? d : "(null)"));
      exit (1);
    }

  screen = DefaultScreenOfDisplay (dpy);
  visual = get_gl_visual (screen);

  if (visual)
    printf ("0x%x\n", XVisualIDFromVisual (visual));
  else
    printf ("none\n");

  exit (0);
}
