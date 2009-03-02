/* xscreensaver, Copyright (c) 2001 by Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* xscreensaver-getimage -- helper program that puts an image
   (e.g., a snapshot of the desktop) onto the given window.
 */

#include "utils.h"

#include <X11/Intrinsic.h>
#include <errno.h>

#ifdef HAVE_XMU
# ifndef VMS
#  include <X11/Xmu/Error.h>
# else /* VMS */
#  include <Xmu/Error.h>
# endif
#else
# include "xmu.h"
#endif

#include "yarandom.h"
#include "grabscreen.h"
#include "resources.h"
#include "colorbars.h"
#include "prefs.h"
#include "vroot.h"


static char *defaults[] = {
#include "../driver/XScreenSaver_ad.h"
 0
};



char *progname = 0;
char *progclass = "XScreenSaver";
XrmDatabase db;
XtAppContext app;

extern void grabscreen_verbose (void);


#define GETIMAGE_VIDEO_PROGRAM "xscreensaver-getimage-video"
#define GETIMAGE_FILE_PROGRAM  "xscreensaver-getimage-file"


const char *
blurb (void)
{
  return progname;
}


static void
exec_error (char **av)
{
  char buf [512];
  char *token;

  sprintf (buf, "%s: could not execute \"%s\"", progname, av[0]);
  perror (buf);

  if (errno == ENOENT &&
      (token = getenv("PATH")))
    {
# ifndef PATH_MAX
#  ifdef MAXPATHLEN
#   define PATH_MAX MAXPATHLEN
#  else
#   define PATH_MAX 2048
#  endif
# endif
      char path[PATH_MAX];
      fprintf (stderr, "\n");
      *path = 0;
# if defined(HAVE_GETCWD)
      getcwd (path, sizeof(path));
# elif defined(HAVE_GETWD)
      getwd (path);
# endif
      if (*path)
        fprintf (stderr, "    Current directory is: %s\n", path);
      fprintf (stderr, "    PATH is:\n");
      token = strtok (strdup(token), ":");
      while (token)
        {
          fprintf (stderr, "        %s\n", token);
          token = strtok(0, ":");
        }
      fprintf (stderr, "\n");
    }

  exit (1);
}

static int
x_ehandler (Display *dpy, XErrorEvent *error)
{
  fprintf (stderr, "\nX error in %s:\n", progname);
  if (XmuPrintDefaultErrorMessage (dpy, error, stderr))
    exit (-1);
  else
    fprintf (stderr, " (nonfatal.)\n");
  return 0;
}



static void
get_image (Screen *screen, Window window, Bool verbose_p)
{
  Display *dpy = DisplayOfScreen (screen);
  Bool desk_p  = get_boolean_resource ("grabDesktopImages",  "Boolean");
  Bool video_p = get_boolean_resource ("grabVideoFrames",    "Boolean");
  Bool image_p = get_boolean_resource ("chooseRandomImages", "Boolean");
  char *dir    = get_string_resource ("imageDirectory", "ImageDirectory");

  enum { do_desk, do_video, do_image, do_bars } which = do_bars;
  int count = 0;

  if (verbose_p)
    {
      fprintf (stderr, "%s: grabDesktopImages:  %s\n",
               progname, desk_p ? "True" : "False");
      fprintf (stderr, "%s: grabVideoFrames:    %s\n",
               progname, video_p ? "True" : "False");
      fprintf (stderr, "%s: chooseRandomImages: %s\n",
               progname, image_p ? "True" : "False");
      fprintf (stderr, "%s: imageDirectory:     %s\n",
               progname, (dir ? dir : ""));
    }

  if (!dir || !*dir)
    {
      if (verbose_p && image_p)
        fprintf (stderr,
                 "%s: no imageDirectory: turning off chooseRandomImages.\n",
                 progname);
      image_p = False;
    }

# ifndef _VROOT_H_
#  error Error!  This file definitely needs vroot.h!
# endif

  /* If the window is not the root window (real or virtual!) then the hack
     that called this program is running in "-window" mode instead of in
     "-root" mode.

     If the window is not the root window, then it's not possible to grab
     video or images onto it (the contract with those programs is to draw on
     the root.)  So turn off those options in that case, and turn on desktop
     grabbing.  (Since we're running in a window on the desktop already, we
     know it's not a security problem to expose desktop bits.)
   */

  if ((desk_p || video_p || image_p) &&
      !top_level_window_p (screen, window))
    {
      desk_p  = False;
      video_p = False;
      image_p = False;
      if (verbose_p)
        fprintf (stderr, "%s: not a top-level window: using colorbars.\n",
                 progname);
    }
  else if (window != VirtualRootWindowOfScreen (screen))
    {
      Bool changed_p = False;
      if (!desk_p) desk_p  = True,  changed_p = True;
      if (video_p) video_p = False, changed_p = True;
      if (image_p) image_p = False, changed_p = True;
      if (changed_p && verbose_p)
        fprintf (stderr,
                 "%s: not running on root window: grabbing desktop.\n",
                 progname);
    }

  count = 0;
  if (desk_p)  count++;
  if (video_p) count++;
  if (image_p) count++;

  if (count == 0)
    which = do_bars;
  else
    while (1)  /* loop until we get one that's permitted */
      {
        which = (random() % 3);
        if (which == do_desk  && desk_p)  break;
        if (which == do_video && video_p) break;
        if (which == do_image && image_p) break;
      }

  if (which == do_desk)
    {
      if (verbose_p)
        {
          fprintf (stderr, "%s: grabbing desktop image\n", progname);
          grabscreen_verbose();
        }
      grab_screen_image (screen, window);
      XSync (dpy, False);
    }
  else if (which == do_bars)
    {
      XWindowAttributes xgwa;
      XGetWindowAttributes (dpy, window, &xgwa);
      if (verbose_p)
        fprintf (stderr, "%s: drawing colorbars\n", progname);
      draw_colorbars (dpy, window, 0, 0, xgwa.width, xgwa.height);
      XSync (dpy, False);
    }
  else
    {
      char *av[10];
      memset (av, 0, sizeof(av));
      switch (which)
        {
        case do_video:
          if (verbose_p)
            fprintf (stderr, "%s: grabbing video\n", progname);
          av[0] = GETIMAGE_VIDEO_PROGRAM;
          break;
        case do_image:
          if (verbose_p)
            fprintf (stderr, "%s: loading random image file\n", progname);
          av[0] = GETIMAGE_FILE_PROGRAM;
          av[1] = dir;
          break;
        default:
          abort();
          break;
        }

      if (verbose_p)
        {
          int i;
          for (i = (sizeof(av)/sizeof(*av))-1; i > 1; i--)
            av[i] = av[i-1];
          av[1] = strdup ("--verbose");
        }

      if (verbose_p)
        {
          int i = 0;
          fprintf (stderr, "%s: executing \"", progname);
          while (av[i])
            {
              fprintf (stderr, "%s", av[i]);
              if (av[++i]) fprintf (stderr, " ");
            }
          fprintf (stderr, "\"\n");
        }

# ifdef HAVE_PUTENV
      /* Store our "-display" argument into the $DISPLAY variable,
         so that the subprocess gets the right display if the
         prevailing $DISPLAY is different. */
      {
        const char *odpy = DisplayString (dpy);
        char *ndpy = (char *) malloc(strlen(odpy) + 20);
        strcpy (ndpy, "DISPLAY=");
        strcat (ndpy, odpy);
        if (putenv (ndpy))
          abort ();
      }
# endif /* HAVE_PUTENV */

      close (ConnectionNumber (dpy));	/* close display fd */

      execvp (av[0], av);		/* shouldn't return */
      exec_error (av);
    }
}


#if 0
static Bool
mapper (XrmDatabase *db, XrmBindingList bindings, XrmQuarkList quarks,
	XrmRepresentation *type, XrmValue *value, XPointer closure)
{
  int i;
  for (i = 0; quarks[i]; i++)
    {
      if (bindings[i] == XrmBindTightly)
	fprintf (stderr, (i == 0 ? "" : "."));
      else if (bindings[i] == XrmBindLoosely)
	fprintf (stderr, "*");
      else
	fprintf (stderr, " ??? ");
      fprintf(stderr, "%s", XrmQuarkToString (quarks[i]));
    }

  fprintf (stderr, ": %s\n", (char *) value->addr);

  return False;
}
#endif


int
main (int argc, char **argv)
{
  saver_preferences P;
  Widget toplevel;
  Display *dpy;
  Screen *screen;
  Window window = (Window) 0;
  Bool verbose_p = False;
  char *s;
  int i;

  progname = argv[0];
  s = strrchr (progname, '/');
  if (s) progname = s+1;

  /* We must read exactly the same resources as xscreensaver.
     That means we must have both the same progclass *and* progname,
     at least as far as the resource database is concerned.  So,
     put "xscreensaver" in argv[0] while initializing Xt.
   */
  argv[0] = "xscreensaver";
  toplevel = XtAppInitialize (&app, progclass, 0, 0, &argc, argv,
			      defaults, 0, 0);
  argv[0] = progname;
  dpy = XtDisplay (toplevel);
  screen = XtScreen (toplevel);
  db = XtDatabase (dpy);

  XtGetApplicationNameAndClass (dpy, &s, &progclass);
  XSetErrorHandler (x_ehandler);
  XSync (dpy, False);

  /* half-assed way of avoiding buffer-overrun attacks. */
  if (strlen (progname) >= 100) progname[100] = 0;

  for (i = 1; i < argc; i++)
    {
      if (argv[i][0] == '-' && argv[i][1] == '-') argv[i]++;
      if (!strcmp (argv[i], "-v") ||
          !strcmp (argv[i], "-verbose"))
        verbose_p = True;
      else if (window == 0)
        {
          unsigned long w;
          char dummy;

          if (!strcmp (argv[i], "root") ||
              !strcmp (argv[i], "-root") ||
              !strcmp (argv[i], "--root"))
            window = RootWindowOfScreen (screen);

          else if ((1 == sscanf (argv[i], " 0x%x %c", &w, &dummy) ||
                    1 == sscanf (argv[i], " %d %c",   &w, &dummy)) &&
                   w != 0)
            window = (Window) w;
          else
            goto LOSE;
        }
      else
        {
         LOSE:
          fprintf (stderr,
            "usage: %s [ -display host:dpy.screen ] [ -v ] window-id\n",
                   progname);
          fprintf (stderr, "\n"
	"\tThis program puts an image of the desktop on the given window.\n"
	"\tIt is used by those xscreensaver demos that operate on images.\n"
	"\n");
          exit (1);
        }
    }

  if (window == 0) goto LOSE;

  /* Randomize -- only need to do this here because this program
     doesn't use the `screenhack.h' or `lockmore.h' APIs. */
# undef ya_rand_init
  ya_rand_init (0);

  memset (&P, 0, sizeof(P));
  P.db = db;
  load_init_file (&P);

  if (P.verbose_p)
    verbose_p = True;

#if 0
  /* Print out all the resources we read. */
  {
    XrmName name = { 0 };
    XrmClass class = { 0 };
    int count = 0;
    XrmEnumerateDatabase (db, &name, &class, XrmEnumAllLevels, mapper,
			  (XtPointer) &count);
  }
#endif

  get_image (screen, window, verbose_p);
  exit (0);
}
