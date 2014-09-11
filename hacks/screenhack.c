/* xscreensaver, Copyright (c) 1992-2014 Jamie Zawinski <jwz@jwz.org>
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

   Create a few static global procedures and variables:

      static void *YOURNAME_init (Display *, Window);

          Return an opaque structure representing your drawing state.

      static unsigned long YOURNAME_draw (Display *, Window, void *closure);

          Draw one frame.
          The `closure' arg is your drawing state, that you created in `init'.
          Return the number of microseconds to wait until the next frame.

          This should return in some small fraction of a second. 
          Do not call `usleep' or loop excessively.  For long loops, use a
          finite state machine.

      static void YOURNAME_reshape (Display *, Window, void *closure,
				    unsigned int width, unsigned int height);

          Called when the size of the window changes with the new size.

      static Bool YOURNAME_event (Display *, Window, void *closure,
				  XEvent *event);

          Called when a keyboard or mouse event arrives.
          Return True if you handle it in some way, False otherwise.

      static void YOURNAME_free (Display *, Window, void *closure);

           Called when you are done: free everything you've allocated,
           including your private `state' structure.  

           NOTE: this is called in windowed-mode when the user typed
           'q' or clicks on the window's close box; but when
           xscreensaver terminates this screenhack, it does so by
           killing the process with SIGSTOP.  So this callback is
           mostly useless.

      static char YOURNAME_defaults [] = { "...", "...", ... , 0 };

           This variable is an array of strings, your default resources.
           Null-terminate the list.

      static XrmOptionDescRec YOURNAME_options[] = { { ... }, ... { 0,0,0,0 } }

           This variable describes your command-line options.
           Null-terminate the list.

      Finally , invoke the XSCREENSAVER_MODULE() macro to tie it all together.

   Additional caveats:

      - Make sure that all functions in your module are static (check this
        by running "nm -g" on the .o file).

      - Do not use global variables: all such info must be stored in the
        private `state' structure.

      - Do not use static function-local variables, either.  Put it in `state'.

        Assume that there are N independent runs of this code going in the
        same address space at the same time: they must not affect each other.

      - Don't forget to write an XML file to describe the user interface
        of your screen saver module.  See .../hacks/config/README for details.
 */

#define DEBUG_PAIR

#include <stdio.h>
#include <X11/Intrinsic.h>
#include <X11/IntrinsicP.h>
#include <X11/CoreP.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/keysym.h>

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

#include "screenhackI.h"
#include "version.h"
#include "vroot.h"
#include "fps.h"

#ifndef _XSCREENSAVER_VROOT_H_
# error Error!  You have an old version of vroot.h!  Check -I args.
#endif /* _XSCREENSAVER_VROOT_H_ */

#ifndef isupper
# define isupper(c)  ((c) >= 'A' && (c) <= 'Z')
#endif
#ifndef _tolower
# define _tolower(c)  ((c) - 'A' + 'a')
#endif


/* This is defined by the SCREENHACK_MAIN() macro via screenhack.h.
 */
extern struct xscreensaver_function_table *xscreensaver_function_table;


const char *progname;   /* used by hacks in error messages */
const char *progclass;  /* used by ../utils/resources.c */
Bool mono_p;		/* used by hacks */


static XrmOptionDescRec default_options [] = {
  { "-root",	".root",		XrmoptionNoArg, "True" },
  { "-window",	".root",		XrmoptionNoArg, "False" },
  { "-mono",	".mono",		XrmoptionNoArg, "True" },
  { "-install",	".installColormap",	XrmoptionNoArg, "True" },
  { "-noinstall",".installColormap",	XrmoptionNoArg, "False" },
  { "-visual",	".visualID",		XrmoptionSepArg, 0 },
  { "-window-id", ".windowID",		XrmoptionSepArg, 0 },
  { "-fps",	".doFPS",		XrmoptionNoArg, "True" },
  { "-no-fps",  ".doFPS",		XrmoptionNoArg, "False" },

# ifdef DEBUG_PAIR
  { "-pair",	".pair",		XrmoptionNoArg, "True" },
# endif /* DEBUG_PAIR */
  { 0, 0, 0, 0 }
};

static char *default_defaults[] = {
  ".root:		false",
  "*geometry:		1280x720", /* this should be .geometry, but noooo... */
  "*mono:		false",
  "*installColormap:	false",
  "*doFPS:		false",
  "*multiSample:	false",
  "*visualID:		default",
  "*windowID:		",
  "*desktopGrabber:	xscreensaver-getimage %s",
  0
};

static XrmOptionDescRec *merged_options;
static int merged_options_size;
static char **merged_defaults;


static void
merge_options (void)
{
  struct xscreensaver_function_table *ft = xscreensaver_function_table;

  const XrmOptionDescRec *options = ft->options;
  const char * const *defaults    = ft->defaults;
  const char *progclass           = ft->progclass;

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
      else
        *s = strdup (*s);
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


static Atom XA_WM_PROTOCOLS, XA_WM_DELETE_WINDOW;

/* Dead-trivial event handling: exits if "q" or "ESC" are typed.
   Exit if the WM_PROTOCOLS WM_DELETE_WINDOW ClientMessage is received.
   Returns False if the screen saver should now terminate.
 */
static Bool
screenhack_handle_event_1 (Display *dpy, XEvent *event)
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
          return False;  /* exit */
        else if (! (keysym >= XK_Shift_L && keysym <= XK_Hyper_R))
          XBell (dpy, 0);  /* beep for non-chord keys */
      }
      break;
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
            return False;  /* exit */
          }
      }
      break;
    }
  return True;
}


static Visual *
pick_visual (Screen *screen)
{
  struct xscreensaver_function_table *ft = xscreensaver_function_table;

  if (ft->pick_visual_hook)
    {
      Visual *v = ft->pick_visual_hook (screen);
      if (v) return v;
    }

  return get_visual_resource (screen, "visualID", "VisualID", False);
}


/* Notice when the user has requested a different visual or colormap
   on a pre-existing window (e.g., "-root -visual truecolor" or
   "-window-id 0x2c00001 -install") and complain, since when drawing
   on an existing window, we have no choice about these things.
 */
static void
visual_warning (Screen *screen, Window window, Visual *visual, Colormap cmap,
                Bool window_p)
{
  struct xscreensaver_function_table *ft = xscreensaver_function_table;

  char *visual_string = get_string_resource (DisplayOfScreen (screen),
                                             "visualID", "VisualID");
  Visual *desired_visual = pick_visual (screen);
  char win[100];
  char why[100];

  if (window == RootWindowOfScreen (screen))
    strcpy (win, "root window");
  else
    sprintf (win, "window 0x%lx", (unsigned long) window);

  if (window_p)
    sprintf (why, "-window-id 0x%lx", (unsigned long) window);
  else
    strcpy (why, "-root");

  if (visual_string && *visual_string)
    {
      char *s;
      for (s = visual_string; *s; s++)
        if (isupper (*s)) *s = _tolower (*s);

      if (!strcmp (visual_string, "default") ||
          !strcmp (visual_string, "default") ||
          !strcmp (visual_string, "best"))
        /* don't warn about these, just silently DWIM. */
        ;
      else if (visual != desired_visual)
        {
          fprintf (stderr, "%s: ignoring `-visual %s' because of `%s'.\n",
                   progname, visual_string, why);
          fprintf (stderr, "%s: using %s's visual 0x%lx.\n",
                   progname, win, XVisualIDFromVisual (visual));
        }
      free (visual_string);
    }

  if (visual == DefaultVisualOfScreen (screen) &&
      has_writable_cells (screen, visual) &&
      get_boolean_resource (DisplayOfScreen (screen),
                            "installColormap", "InstallColormap"))
    {
      fprintf (stderr, "%s: ignoring `-install' because of `%s'.\n",
               progname, why);
      fprintf (stderr, "%s: using %s's colormap 0x%lx.\n",
               progname, win, (unsigned long) cmap);
    }

  if (ft->validate_visual_hook)
    {
      if (! ft->validate_visual_hook (screen, win, visual))
        exit (1);
    }
}


static void
fix_fds (void)
{
  /* Bad Things Happen if stdin, stdout, and stderr have been closed
     (as by the `sh incantation "attraction >&- 2>&-").  When you do
     that, the X connection gets allocated to one of these fds, and
     then some random library writes to stderr, and random bits get
     stuffed down the X pipe, causing "Xlib: sequence lost" errors.
     So, we cause the first three file descriptors to be open to
     /dev/null if they aren't open to something else already.  This
     must be done before any other files are opened (or the closing
     of that other file will again free up one of the "magic" first
     three FDs.)

     We do this by opening /dev/null three times, and then closing
     those fds, *unless* any of them got allocated as #0, #1, or #2,
     in which case we leave them open.  Gag.

     Really, this crap is technically required of *every* X program,
     if you want it to be robust in the face of "2>&-".
   */
  int fd0 = open ("/dev/null", O_RDWR);
  int fd1 = open ("/dev/null", O_RDWR);
  int fd2 = open ("/dev/null", O_RDWR);
  if (fd0 > 2) close (fd0);
  if (fd1 > 2) close (fd1);
  if (fd2 > 2) close (fd2);
}


static Boolean
screenhack_table_handle_events (Display *dpy,
                                const struct xscreensaver_function_table *ft,
                                Window window, void *closure
#ifdef DEBUG_PAIR
                                , Window window2, void *closure2
#endif
                                )
{
  XtAppContext app = XtDisplayToApplicationContext (dpy);

  if (XtAppPending (app) & (XtIMTimer|XtIMAlternateInput))
    XtAppProcessEvent (app, XtIMTimer|XtIMAlternateInput);

  while (XPending (dpy))
    {
      XEvent event;
      XNextEvent (dpy, &event);

      if (event.xany.type == ConfigureNotify)
        {
          if (event.xany.window == window)
            ft->reshape_cb (dpy, window, closure,
                            event.xconfigure.width, event.xconfigure.height);
#ifdef DEBUG_PAIR
          if (window2 && event.xany.window == window2)
            ft->reshape_cb (dpy, window2, closure2,
                            event.xconfigure.width, event.xconfigure.height);
#endif
        }
      else if (event.xany.type == ClientMessage ||
               (! (event.xany.window == window
                   ? ft->event_cb (dpy, window, closure, &event)
#ifdef DEBUG_PAIR
                   : (window2 && event.xany.window == window2)
                   ? ft->event_cb (dpy, window2, closure2, &event)
#endif
                   : 0)))
        if (! screenhack_handle_event_1 (dpy, &event))
          return False;

      if (XtAppPending (app) & (XtIMTimer|XtIMAlternateInput))
        XtAppProcessEvent (app, XtIMTimer|XtIMAlternateInput);
    }
  return True;
}


static Boolean
usleep_and_process_events (Display *dpy,
                           const struct xscreensaver_function_table *ft,
                           Window window, fps_state *fpst, void *closure,
                           unsigned long delay
#ifdef DEBUG_PAIR
                         , Window window2, fps_state *fpst2, void *closure2,
                           unsigned long delay2
#endif
                           )
{
  do {
    unsigned long quantum = 100000;  /* 1/10th second */
    if (quantum > delay) 
      quantum = delay;
    delay -= quantum;

    XSync (dpy, False);
    if (quantum > 0)
      {
        usleep (quantum);
        if (fpst) fps_slept (fpst, quantum);
#ifdef DEBUG_PAIR
        if (fpst2) fps_slept (fpst2, quantum);
#endif
      }

    if (! screenhack_table_handle_events (dpy, ft, window, closure
#ifdef DEBUG_PAIR
                                          , window2, closure2
#endif
                                          ))
      return False;
  } while (delay > 0);

  return True;
}


static void
screenhack_do_fps (Display *dpy, Window w, fps_state *fpst, void *closure)
{
  fps_compute (fpst, 0, -1);
  fps_draw (fpst);
}


static void
run_screenhack_table (Display *dpy, 
                      Window window,
# ifdef DEBUG_PAIR
                      Window window2,
# endif
                      const struct xscreensaver_function_table *ft)
{

  /* Kludge: even though the init_cb functions are declared to take 2 args,
     actually call them with 3, for the benefit of xlockmore_init() and
     xlockmore_setup().
   */
  void *(*init_cb) (Display *, Window, void *) = 
    (void *(*) (Display *, Window, void *)) ft->init_cb;

  void (*fps_cb) (Display *, Window, fps_state *, void *) = ft->fps_cb;

  void *closure = init_cb (dpy, window, ft->setup_arg);
  fps_state *fpst = fps_init (dpy, window);

#ifdef DEBUG_PAIR
  void *closure2 = 0;
  fps_state *fpst2 = 0;
  if (window2) closure2 = init_cb (dpy, window2, ft->setup_arg);
  if (window2) fpst2 = fps_init (dpy, window2);
#endif

  if (! closure)  /* if it returns nothing, it can't possibly be re-entrant. */
    abort();

  if (! fps_cb) fps_cb = screenhack_do_fps;

  while (1)
    {
      unsigned long delay = ft->draw_cb (dpy, window, closure);
#ifdef DEBUG_PAIR
      unsigned long delay2 = 0;
      if (window2) delay2 = ft->draw_cb (dpy, window2, closure2);
#endif

      if (fpst) fps_cb (dpy, window, fpst, closure);
#ifdef DEBUG_PAIR
      if (fpst2) fps_cb (dpy, window, fpst2, closure);
#endif

      if (! usleep_and_process_events (dpy, ft,
                                       window, fpst, closure, delay
#ifdef DEBUG_PAIR
                                       , window2, fpst2, closure2, delay2
#endif
                                       ))
        break;
    }

  ft->free_cb (dpy, window, closure);
  if (fpst) fps_free (fpst);

#ifdef DEBUG_PAIR
  if (window2) ft->free_cb (dpy, window2, closure2);
  if (fpst2) fps_free (fpst2);
#endif
}


static Widget
make_shell (Screen *screen, Widget toplevel, int width, int height)
{
  Display *dpy = DisplayOfScreen (screen);
  Visual *visual = pick_visual (screen);
  Boolean def_visual_p = (toplevel && 
                          visual == DefaultVisualOfScreen (screen));

  if (width  <= 0) width  = 600;
  if (height <= 0) height = 480;

  if (def_visual_p)
    {
      Window window;
      XtVaSetValues (toplevel,
                     XtNmappedWhenManaged, False,
                     XtNwidth, width,
                     XtNheight, height,
                     XtNinput, True,  /* for WM_HINTS */
                     NULL);
      XtRealizeWidget (toplevel);
      window = XtWindow (toplevel);

      if (get_boolean_resource (dpy, "installColormap", "InstallColormap"))
        {
          Colormap cmap = 
            XCreateColormap (dpy, window, DefaultVisualOfScreen (screen),
                             AllocNone);
          XSetWindowColormap (dpy, window, cmap);
        }
    }
  else
    {
      unsigned int bg, bd;
      Widget new;
      Colormap cmap = XCreateColormap (dpy, VirtualRootWindowOfScreen(screen),
                                       visual, AllocNone);
      bg = get_pixel_resource (dpy, cmap, "background", "Background");
      bd = get_pixel_resource (dpy, cmap, "borderColor", "Foreground");

      new = XtVaAppCreateShell (progname, progclass,
                                topLevelShellWidgetClass, dpy,
                                XtNmappedWhenManaged, False,
                                XtNvisual, visual,
                                XtNdepth, visual_depth (screen, visual),
                                XtNwidth, width,
                                XtNheight, height,
                                XtNcolormap, cmap,
                                XtNbackground, (Pixel) bg,
                                XtNborderColor, (Pixel) bd,
                                XtNinput, True,  /* for WM_HINTS */
                                NULL);

      if (!toplevel)  /* kludge for the second window in -pair mode... */
        XtVaSetValues (new, XtNx, 0, XtNy, 550, NULL);

      XtRealizeWidget (new);
      toplevel = new;
    }

  return toplevel;
}

static void
init_window (Display *dpy, Widget toplevel, const char *title)
{
  Window window;
  XWindowAttributes xgwa;
  XtPopup (toplevel, XtGrabNone);
  XtVaSetValues (toplevel, XtNtitle, title, NULL);

  /* Select KeyPress, and announce that we accept WM_DELETE_WINDOW.
   */
  window = XtWindow (toplevel);
  XGetWindowAttributes (dpy, window, &xgwa);
  XSelectInput (dpy, window,
                (xgwa.your_event_mask | KeyPressMask | KeyReleaseMask |
                 ButtonPressMask | ButtonReleaseMask));
  XChangeProperty (dpy, window, XA_WM_PROTOCOLS, XA_ATOM, 32,
                   PropModeReplace,
                   (unsigned char *) &XA_WM_DELETE_WINDOW, 1);
}


int
main (int argc, char **argv)
{
  struct xscreensaver_function_table *ft = xscreensaver_function_table;

  XWindowAttributes xgwa;
  Widget toplevel;
  Display *dpy;
  Window window;
# ifdef DEBUG_PAIR
  Window window2 = 0;
  Widget toplevel2 = 0;
# endif
  XtAppContext app;
  Bool root_p;
  Window on_window = 0;
  XEvent event;
  Boolean dont_clear;
  char version[255];

  fix_fds();

  progname = argv[0];   /* reset later */
  progclass = ft->progclass;

  if (ft->setup_cb)
    ft->setup_cb (ft, ft->setup_arg);

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

  XtGetApplicationNameAndClass (dpy,
                                (char **) &progname,
                                (char **) &progclass);

  /* half-assed way of avoiding buffer-overrun attacks. */
  if (strlen (progname) >= 100) ((char *) progname)[100] = 0;

  XSetErrorHandler (screenhack_ehandler);

  XA_WM_PROTOCOLS = XInternAtom (dpy, "WM_PROTOCOLS", False);
  XA_WM_DELETE_WINDOW = XInternAtom (dpy, "WM_DELETE_WINDOW", False);

  {
    char *v = (char *) strdup(strchr(screensaver_id, ' '));
    char *s1, *s2, *s3, *s4;
    const char *ot = get_string_resource (dpy, "title", "Title");
    s1 = (char *) strchr(v,  ' '); s1++;
    s2 = (char *) strchr(s1, ' ');
    s3 = (char *) strchr(v,  '('); s3++;
    s4 = (char *) strchr(s3, ')');
    *s2 = 0;
    *s4 = 0;
    if (ot && !*ot) ot = 0;
    sprintf (version, "%.50s%s%s: from the XScreenSaver %s distribution (%s)",
             (ot ? ot : ""),
             (ot ? ": " : ""),
	     progclass, s1, s3);
    free(v);
  }

  if (argc > 1)
    {
      const char *s;
      int i;
      int x = 18;
      int end = 78;
      Bool help_p = (!strcmp(argv[1], "-help") ||
                     !strcmp(argv[1], "--help"));
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

#if 0
      if (help_p)
        {
          fprintf (stderr, "\nResources:\n\n");
          for (i = 0; i < merged_options_size; i++)
            {
              const char *opt = merged_options [i].option;
              const char *res = merged_options [i].specifier + 1;
              const char *val = merged_options [i].value;
              char *s = get_string_resource (dpy, (char *) res, (char *) res);

              if (s)
                {
                  int L = strlen(s);
                while (L > 0 && (s[L-1] == ' ' || s[L-1] == '\t'))
                  s[--L] = 0;
                }

              fprintf (stderr, "    %-16s %-18s ", opt, res);
              if (merged_options [i].argKind == XrmoptionSepArg)
                {
                  fprintf (stderr, "[%s]", (s ? s : "?"));
                }
              else
                {
                  fprintf (stderr, "%s", (val ? val : "(null)"));
                  if (val && s && !strcasecmp (val, s))
                    fprintf (stderr, " [default]");
                }
              fprintf (stderr, "\n");
            }
          fprintf (stderr, "\n");
        }
#endif

      exit (help_p ? 0 : 1);
    }

  {
    char **s;
    for (s = merged_defaults; *s; s++)
      free(*s);
  }

  free (merged_options);
  free (merged_defaults);
  merged_options = 0;
  merged_defaults = 0;

  dont_clear = get_boolean_resource (dpy, "dontClearRoot", "Boolean");
  mono_p = get_boolean_resource (dpy, "mono", "Boolean");
  if (CellsOfScreen (DefaultScreenOfDisplay (dpy)) <= 2)
    mono_p = True;

  root_p = get_boolean_resource (dpy, "root", "Boolean");

  {
    char *s = get_string_resource (dpy, "windowID", "WindowID");
    if (s && *s)
      on_window = get_integer_resource (dpy, "windowID", "WindowID");
    if (s) free (s);
  }

  if (on_window)
    {
      window = (Window) on_window;
      XtDestroyWidget (toplevel);
      XGetWindowAttributes (dpy, window, &xgwa);
      visual_warning (xgwa.screen, window, xgwa.visual, xgwa.colormap, True);

      /* Select KeyPress and resize events on the external window.
       */
      xgwa.your_event_mask |= KeyPressMask | StructureNotifyMask;
      XSelectInput (dpy, window, xgwa.your_event_mask);

      /* Select ButtonPress and ButtonRelease events on the external window,
         if no other app has already selected them (only one app can select
         ButtonPress at a time: BadAccess results.)
       */
      if (! (xgwa.all_event_masks & (ButtonPressMask | ButtonReleaseMask)))
        XSelectInput (dpy, window,
                      (xgwa.your_event_mask |
                       ButtonPressMask | ButtonReleaseMask));
    }
  else if (root_p)
    {
      window = VirtualRootWindowOfScreen (XtScreen (toplevel));
      XtDestroyWidget (toplevel);
      XGetWindowAttributes (dpy, window, &xgwa);
      /* With RANDR, the root window can resize! */
      XSelectInput (dpy, window, xgwa.your_event_mask | StructureNotifyMask);
      visual_warning (xgwa.screen, window, xgwa.visual, xgwa.colormap, False);
    }
  else
    {
      Widget new = make_shell (XtScreen (toplevel), toplevel,
                               toplevel->core.width,
                               toplevel->core.height);
      if (new != toplevel)
        {
          XtDestroyWidget (toplevel);
          toplevel = new;
        }

      init_window (dpy, toplevel, version);
      window = XtWindow (toplevel);
      XGetWindowAttributes (dpy, window, &xgwa);

# ifdef DEBUG_PAIR
      if (get_boolean_resource (dpy, "pair", "Boolean"))
        {
          toplevel2 = make_shell (xgwa.screen, 0,
                                  toplevel->core.width,
                                  toplevel->core.height);
          init_window (dpy, toplevel2, version);
          window2 = XtWindow (toplevel2);
        }
# endif /* DEBUG_PAIR */
    }

  if (!dont_clear)
    {
      unsigned int bg = get_pixel_resource (dpy, xgwa.colormap,
                                            "background", "Background");
      XSetWindowBackground (dpy, window, bg);
      XClearWindow (dpy, window);
# ifdef DEBUG_PAIR
      if (window2)
        {
          XSetWindowBackground (dpy, window2, bg);
          XClearWindow (dpy, window2);
        }
# endif
    }

  if (!root_p && !on_window)
    /* wait for it to be mapped */
    XIfEvent (dpy, &event, MapNotify_event_p, (XPointer) window);

  XSync (dpy, False);

  /* This is the one and only place that the random-number generator is
     seeded in any screenhack.  You do not need to seed the RNG again,
     it is done for you before your code is invoked. */
# undef ya_rand_init
  ya_rand_init (0);

  run_screenhack_table (dpy, window, 
# ifdef DEBUG_PAIR
                        window2,
# endif
                        ft);

  XtDestroyWidget (toplevel);
  XtDestroyApplicationContext (app);

  return 0;
}
