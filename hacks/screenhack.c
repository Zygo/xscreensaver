/* xscreensaver, Copyright (c) 1992, 1995, 1997, 1998
 *  Jamie Zawinski <jwz@jwz.org>
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

   -  create a variable `char defaults []' for the default resources, and
      null-terminate it.

   -  create a variable `XrmOptionDescRec options[]' for the command-line,
      and null-terminate it.

   And that's it...
 */

#include <stdio.h>
#include <X11/Intrinsic.h>
#include <X11/IntrinsicP.h>
#include <X11/CoreP.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xutil.h>

#ifdef __sgi
# include <X11/SGIScheme.h>	/* for SgiUseSchemes() */
#endif /* __sgi */

#ifdef HAVE_XMU
# ifndef VMS
#  include <X11/Xmu/Error.h>
# else /* VMS */
#  include <Xmu/Error.h>
# endif
#else
# include "xmu.h"
#endif
#include "screenhack.h"
#include "version.h"
#include "vroot.h"

char *progname;
XrmDatabase db;
Bool mono_p;

static XrmOptionDescRec default_options [] = {
  { "-root",	".root",		XrmoptionNoArg, "True" },
  { "-window",	".root",		XrmoptionNoArg, "False" },
  { "-mono",	".mono",		XrmoptionNoArg, "True" },
  { "-install",	".installColormap",	XrmoptionNoArg, "True" },
  { "-noinstall",".installColormap",	XrmoptionNoArg, "False" },
  { "-visual",	".visualID",		XrmoptionSepArg, 0 },
  { "-window-id", ".windowID",		XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

static char *default_defaults[] = {
  ".root:		false",
  "*geometry:		600x480", /* this should be .geometry, but nooooo... */
  "*mono:		false",
  "*installColormap:	false",
  "*visualID:		default",
  "*windowID:		",
  0
};

static XrmOptionDescRec *merged_options;
static int merged_options_size;
static char **merged_defaults;

static void
merge_options (void)
{
  int def_opts_size, opts_size;
  int def_defaults_size, defaults_size;

  for (def_opts_size = 0; default_options[def_opts_size].option;
       def_opts_size++)
    ;
  for (opts_size = 0; options[opts_size].option; opts_size++)
    ;

  merged_options_size = def_opts_size + opts_size;
  merged_options = (XrmOptionDescRec *)
    malloc ((merged_options_size + 1) * sizeof(*default_options));
  memcpy (merged_options, default_options,
	  (def_opts_size * sizeof(*default_options)));
  memcpy (merged_options + def_opts_size, options,
	  ((opts_size + 1) * sizeof(*default_options)));

  for (def_defaults_size = 0; default_defaults[def_defaults_size];
       def_defaults_size++)
    ;
  for (defaults_size = 0; defaults[defaults_size]; defaults_size++)
    ;
  merged_defaults = (char **)
    malloc ((def_defaults_size + defaults_size + 1) * sizeof (*defaults));;
  memcpy (merged_defaults, default_defaults,
	  def_defaults_size * sizeof(*defaults));
  memcpy (merged_defaults + def_defaults_size, defaults,
	  (defaults_size + 1) * sizeof(*defaults));

  /* This totally sucks.  Xt should behave like this by default.
     If the string in `defaults' looks like ".foo", change that
     to "Progclass.foo".
   */
  {
    char **s;
    for (s = merged_defaults; *s; s++)
      if (**s == '.')
	{
	  const char *oldr = *s;
	  char *newr = (char *) malloc(strlen(oldr) + strlen(progclass) + 3);
	  strcpy (newr, progclass);
	  strcat (newr, oldr);
	  *s = newr;
	}
  }
}


/* Make the X errors print out the name of this program, so we have some
   clue which one has a bug when they die under the screensaver.
 */

static int
screenhack_ehandler (Display *dpy, XErrorEvent *error)
{
  fprintf (stderr, "\nX error in %s:\n", progname);
  if (XmuPrintDefaultErrorMessage (dpy, error, stderr))
    exit (-1);
  else
    fprintf (stderr, " (nonfatal.)\n");
  return 0;
}

static Bool
MapNotify_event_p (Display *dpy, XEvent *event, XPointer window)
{
  return (event->xany.type == MapNotify &&
	  event->xvisibility.window == (Window) window);
}


#ifdef USE_GL
extern Visual *get_gl_visual (Screen *, const char *, const char *);
#endif

#ifdef XLOCKMORE
extern void pre_merge_options (void);
#endif


static Atom XA_WM_PROTOCOLS, XA_WM_DELETE_WINDOW;

/* Dead-trivial event handling: exits if "q" or "ESC" are typed.
   Exit if the WM_PROTOCOLS WM_DELETE_WINDOW ClientMessage is received.
 */
void
screenhack_handle_event (Display *dpy, XEvent *event)
{
  switch (event->xany.type)
    {
    case KeyPress:
      {
        KeySym keysym;
        char c = 0;
        XLookupString (&event->xkey, &c, 1, &keysym, 0);
        if (c == 'q' ||
            c == 'Q' ||
            c == 3 ||	/* ^C */
            c == 27)	/* ESC */
          exit (0);
      }
    case ButtonPress:
      XBell (dpy, 0);
      break;
    case ClientMessage:
      {
        if (event->xclient.message_type != XA_WM_PROTOCOLS)
          {
            char *s = XGetAtomName(dpy, event->xclient.message_type);
            if (!s) s = "(null)";
            fprintf (stderr, "%s: unknown ClientMessage %s received!\n",
                     progname, s);
          }
        else if (event->xclient.data.l[0] != XA_WM_DELETE_WINDOW)
          {
            char *s1 = XGetAtomName(dpy, event->xclient.message_type);
            char *s2 = XGetAtomName(dpy, event->xclient.data.l[0]);
            if (!s1) s1 = "(null)";
            if (!s2) s2 = "(null)";
            fprintf (stderr, "%s: unknown ClientMessage %s[%s] received!\n",
                     progname, s1, s2);
          }
        else
          {
            exit (0);
          }
      }
      break;
    }
}


void
screenhack_handle_events (Display *dpy)
{
  while (XPending (dpy))
    {
      XEvent event;
      XNextEvent (dpy, &event);
      screenhack_handle_event (dpy, &event);
    }
}



int
main (int argc, char **argv)
{
  XtAppContext app;
  Widget toplevel;
  Display *dpy;
  Window window;
  Visual *visual;
  Colormap cmap;
  Bool root_p;
  Window on_window = 0;
  XEvent event;
  Boolean dont_clear /*, dont_map */;
  char version[255];

#ifdef XLOCKMORE
  pre_merge_options ();
#endif
  merge_options ();

#ifdef __sgi
  /* We have to do this on SGI to prevent the background color from being
     overridden by the current desktop color scheme (we'd like our backgrounds
     to be black, thanks.)  This should be the same as setting the
     "*useSchemes: none" resource, but it's not -- if that resource is
     present in the `default_defaults' above, it doesn't work, though it
     does work when passed as an -xrm arg on the command line.  So screw it,
     turn them off from C instead.
   */
  SgiUseSchemes ("none"); 
#endif /* __sgi */

  toplevel = XtAppInitialize (&app, progclass, merged_options,
			      merged_options_size, &argc, argv,
			      merged_defaults, 0, 0);
  dpy = XtDisplay (toplevel);
  db = XtDatabase (dpy);
  XtGetApplicationNameAndClass (dpy, &progname, &progclass);
  XSetErrorHandler (screenhack_ehandler);

  XA_WM_PROTOCOLS = XInternAtom (dpy, "WM_PROTOCOLS", False);
  XA_WM_DELETE_WINDOW = XInternAtom (dpy, "WM_DELETE_WINDOW", False);

  {
    char *v = (char *) strdup(strchr(screensaver_id, ' '));
    char *s1, *s2, *s3, *s4;
    s1 = (char *) strchr(v,  ' '); s1++;
    s2 = (char *) strchr(s1, ' ');
    s3 = (char *) strchr(v,  '('); s3++;
    s4 = (char *) strchr(s3, ')');
    *s2 = 0;
    *s4 = 0;
    sprintf (version, "%s: from the XScreenSaver %s distribution (%s.)",
	     progclass, s1, s3);
    free(v);
  }

  if (argc > 1)
    {
      const char *s;
      int i;
      int x = 18;
      int end = 78;
      Bool help_p = !strcmp(argv[1], "-help");
      fprintf (stderr, "%s\n", version);
      for (s = progclass; *s; s++) fprintf(stderr, " ");
      fprintf (stderr, "  http://www.jwz.org/xscreensaver/\n\n");

      if (!help_p)
	fprintf(stderr, "Unrecognised option: %s\n", argv[1]);
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
      exit (help_p ? 0 : 1);
    }

  dont_clear = get_boolean_resource ("dontClearRoot", "Boolean");
/*dont_map = get_boolean_resource ("dontMapWindow", "Boolean"); */
  mono_p = get_boolean_resource ("mono", "Boolean");
  if (CellsOfScreen (DefaultScreenOfDisplay (dpy)) <= 2)
    mono_p = True;

  root_p = get_boolean_resource ("root", "Boolean");

  {
    char *s = get_string_resource ("windowID", "WindowID");
    if (s && *s)
      on_window = get_integer_resource ("windowID", "WindowID");
    if (s) free (s);
  }

  if (on_window)
    {
      XWindowAttributes xgwa;
      window = (Window) on_window;
      XtDestroyWidget (toplevel);
      XGetWindowAttributes (dpy, window, &xgwa);
      cmap = xgwa.colormap;
      visual = xgwa.visual;
    }
  else if (root_p)
    {
      XWindowAttributes xgwa;
      window = RootWindowOfScreen (XtScreen (toplevel));
      XtDestroyWidget (toplevel);
      XGetWindowAttributes (dpy, window, &xgwa);
      cmap = xgwa.colormap;
      visual = xgwa.visual;
    }
  else
    {
      Boolean def_visual_p;
      Screen *screen = XtScreen (toplevel);

#ifdef USE_GL
      visual = get_gl_visual (screen, "visualID", "VisualID");
#else
      visual = get_visual_resource (screen, "visualID", "VisualID", False);
#endif

      if (toplevel->core.width <= 0)
	toplevel->core.width = 600;
      if (toplevel->core.height <= 0)
	toplevel->core.height = 480;

      def_visual_p = (visual == DefaultVisualOfScreen (screen));

      if (!def_visual_p)
	{
	  unsigned int bg, bd;
	  Widget new;

	  cmap = XCreateColormap (dpy, RootWindowOfScreen(screen),
				  visual, AllocNone);
	  bg = get_pixel_resource ("background", "Background", dpy, cmap);
	  bd = get_pixel_resource ("borderColor", "Foreground", dpy, cmap);

	  new = XtVaAppCreateShell (progname, progclass,
				    topLevelShellWidgetClass, dpy,
				    XtNmappedWhenManaged, False,
				    XtNvisual, visual,
				    XtNdepth, visual_depth (screen, visual),
				    XtNwidth, toplevel->core.width,
				    XtNheight, toplevel->core.height,
				    XtNcolormap, cmap,
				    XtNbackground, (Pixel) bg,
				    XtNborderColor, (Pixel) bd,
				    0);
	  XtDestroyWidget (toplevel);
	  toplevel = new;
	  XtRealizeWidget (toplevel);
	  window = XtWindow (toplevel);
	}
      else
	{
	  XtVaSetValues (toplevel, XtNmappedWhenManaged, False, 0);
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
	    {
	      cmap = DefaultColormap (dpy, DefaultScreen (dpy));
	    }
	}

/*
      if (dont_map)
	{
	  XtVaSetValues (toplevel, XtNmappedWhenManaged, False, 0);
	  XtRealizeWidget (toplevel);
	}
      else
*/
	{
	  XtPopup (toplevel, XtGrabNone);
	}

      XtVaSetValues(toplevel, XtNtitle, version, 0);

      /* For screenhack_handle_events(): select KeyPress, and
         announce that we accept WM_DELETE_WINDOW. */
      {
        XWindowAttributes xgwa;
        XGetWindowAttributes (dpy, window, &xgwa);
        XSelectInput (dpy, window,
                      xgwa.your_event_mask | KeyPressMask | ButtonPressMask);
        XChangeProperty (dpy, window, XA_WM_PROTOCOLS, XA_ATOM, 32,
                         PropModeReplace,
                         (unsigned char *) &XA_WM_DELETE_WINDOW, 1);
      }
    }

  if (!dont_clear)
    {
      XSetWindowBackground (dpy, window,
			    get_pixel_resource ("background", "Background",
						dpy, cmap));
      XClearWindow (dpy, window);
    }

  if (!root_p && !on_window)
    /* wait for it to be mapped */
    XIfEvent (dpy, &event, MapNotify_event_p, (XPointer) window);

  XSync (dpy, False);
  srandom ((int) time ((time_t *) 0));
  screenhack (dpy, window); /* doesn't return */
  return 0;
}
