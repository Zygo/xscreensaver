/* xscreensaver, Copyright (c) 1992 Jamie Zawinski <jwz@lucid.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * And remember: X Windows is to graphics hacking as roman numerals are to
 * the square root of pi.
 */

/* This file contains simple code to open a window or draw on the root.
   The idea being that, when writing a graphics hack, you can just link
   with this .o to get all of the uninteresting junk out of the way.

   -  create a procedure `screenhack(dpy, window)'

   -  create a variable `char *progclass' which names this program's
      resource class.

   -  create a variable `char defaults []' for the default resources.

   -  create a variable `XrmOptionDescRec options []' for the command-line,
      and `int options_size' which is `XtNumber (options)'.

   And that's it...
 */

#include "version.h"

#include <stdio.h>
#include <X11/Intrinsic.h>
#include <X11/IntrinsicP.h>
#include <X11/CoreP.h>
#include <X11/Xmu/Error.h>
#include "screenhack.h"

char *progname;
XrmDatabase db;
Bool mono_p;


static XrmOptionDescRec default_options [] = {
  { "-root",	".root",		XrmoptionNoArg, "True" },
  { "-window",	".root",		XrmoptionNoArg, "False" },
  { "-mono",	".mono",		XrmoptionNoArg, "True" },
  { "-install",	".installColormap",	XrmoptionNoArg, "True" }
};

static char *default_defaults[] = {
  "*root:		false",
  "*geometry:		500x500", /* this should be .geometry, but nooooo... */
  "*mono:		false",
  "*installColormap:	false",
  0
};

static XrmOptionDescRec *merged_options;
static int merged_options_size;
static char **merged_defaults;

static void
merge_options ()
{
  int options_sizeof = options_size * sizeof (options[0]);
  int defaults_size;
  merged_options_size = XtNumber (default_options) + options_size;
  merged_options = (XrmOptionDescRec *)
    malloc (sizeof (default_options) + options_sizeof);
  memcpy (merged_options, options, options_sizeof);
  memcpy (merged_options + options_size, default_options,
	  sizeof (default_options));

  for (defaults_size = 0; defaults [defaults_size]; defaults_size++);
  merged_defaults = (char **)
    malloc (sizeof (default_defaults) + (defaults_size * sizeof (char *)));
  memcpy (merged_defaults, default_defaults, sizeof (default_defaults));
  memcpy ((merged_defaults - 1 +
	   (sizeof (default_defaults) / sizeof (default_defaults[0]))),
	  defaults, ((defaults_size + 1) * sizeof (defaults[0])));
}


/* Make the X errors print out the name of this program, so we have some
   clue which one has a bug when they die under the screensaver.
 */

static int
screenhack_ehandler (dpy, error)
     Display *dpy;
     XErrorEvent *error;
{
  fprintf (stderr, "\nX error in %s:\n", progname);
  if (XmuPrintDefaultErrorMessage (dpy, error, stderr))
    exit (-1);
  else
    fprintf (stderr, " (nonfatal.)\n");
  return 0;
}

static Bool
MapNotify_event_p (dpy, event, window)
     Display *dpy;
     XEvent *event;
     XPointer window;
{
  return (event->xany.type == MapNotify &&
	  event->xvisibility.window == (Window) window);
}


void
main (argc, argv)
     int argc;
     char **argv;
{
  XtAppContext app;
  Widget toplevel;
  Display *dpy;
  Window window;
  Colormap cmap;
  Bool root_p;
  XEvent event;

  merge_options ();
  toplevel = XtAppInitialize (&app, progclass, merged_options,
			      merged_options_size, &argc, argv,
			      merged_defaults, 0, 0);
  dpy = XtDisplay (toplevel);
  db = XtDatabase (dpy);
  XtGetApplicationNameAndClass (dpy, &progname, &progclass);
  XSetErrorHandler (screenhack_ehandler);
  if (argc > 1)
    {
      int i;
      int x = 18;
      int end = 78;
      fprintf (stderr, "%s: unrecognised option \"%s\"\n", progname, argv[1]);
      fprintf (stderr, "Options include: ");
      for (i = 0; i < merged_options_size; i++)
	{
	  char *sw = merged_options [i].option;
	  Bool argp = (merged_options [i].argKind == XrmoptionSepArg);
	  int size = strlen (sw) + (argp ? 6 : 0) + 2;
	  if (x + size >= end)
	    {
	      fprintf (stderr, "\n\t\t ");
	      x = 18;
	    }
	  x += size;
	  fprintf (stderr, "%s", sw);
	  if (argp) fprintf (stderr, " <arg>");
	  if (i != merged_options_size - 1) fprintf (stderr, ", ");
	}
      fprintf (stderr, ".\n");
      exit (1);
    }

  mono_p = get_boolean_resource ("mono", "Boolean");
  if (CellsOfScreen (DefaultScreenOfDisplay (dpy)) <= 2)
    mono_p = True;

  root_p = get_boolean_resource ("root", "Boolean");
  if (root_p)
    {
      XWindowAttributes xgwa;
      window = RootWindowOfScreen (XtScreen (toplevel));
      XtDestroyWidget (toplevel);
      XGetWindowAttributes (dpy, window, &xgwa);
      cmap = xgwa.colormap;
    }
  else
    {
      XtRealizeWidget (toplevel);
      window = XtWindow (toplevel);
      if (get_boolean_resource ("installColormap", "InstallColormap"))
	{
	  cmap = XCreateColormap (dpy, window,
				  DefaultVisualOfScreen (XtScreen (toplevel)),
				  AllocNone);
	  XSetWindowColormap (dpy, window, cmap);
	}
      else
	cmap = DefaultColormap (dpy, DefaultScreen (dpy));
    }
  if (!get_boolean_resource ("dontClearWindow", "Boolean")) /* kludge-o-rama */
    {
      XSetWindowBackground (dpy, window,
			    get_pixel_resource ("background", "Background",
						dpy, cmap));
      XClearWindow (dpy, window);
    }

  if (!root_p && toplevel->core.mapped_when_managed)
    /* wait for it to be mapped */
    XIfEvent (dpy, &event, MapNotify_event_p, (XPointer) window);

  XSync (dpy, False);
  srandom ((int) time ((time_t *) 0));
  screenhack (dpy, window);
}
