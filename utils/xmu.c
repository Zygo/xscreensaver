/* xscreensaver, Copyright © 1991-2021 Jamie Zawinski <jwz@jwz.org>
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

#include <stdio.h>
#include <X11/Xlib.h>
#include "xmu.h"

extern const char *progname;

int
XmuPrintDefaultErrorMessage (Display *dpy, XErrorEvent *event, FILE *fp)
{
  /* The real libXmu version of this does localization and uses private,
     undocumented fields inside the Display struct to print things about
     errors from server-extension events.  But the following is sufficient.
   */
  char xerr[1024], opname[1024], num[20];
  const char *b = progname;

  /* Convert the error code to its name, e.g. "BadWindow" */
  XGetErrorText (dpy, event->error_code, xerr, sizeof(xerr)-1);

  /* Convert the request's ID number to its name, e.g., "XGetProperty" */
  sprintf (num, "%d", event->request_code);
  XGetErrorDatabaseText (dpy, "XRequest", num, "", opname, sizeof(opname)-1);

  fprintf (fp, "%s:   Failed request: %s\n", b, xerr);
  fprintf (fp, "%s:   Major opcode:   %d (%s)\n", b, event->request_code,
           opname);
  fprintf (fp, "%s:   Resource id:    0x%lx\n", b, event->resourceid);
  fprintf (fp, "%s:   Serial number:  %ld / %ld\n\n", b,
           event->serial, NextRequest(dpy)-1);
  return 1;
}
