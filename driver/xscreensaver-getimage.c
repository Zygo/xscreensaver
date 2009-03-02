/* xscreensaver, Copyright (c) 2001, 2002 by Jamie Zawinski <jwz@jwz.org>
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
#include <ctype.h>
#include <errno.h>

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>		/* for waitpid() and associated macros */
#endif

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
#include "visual.h"
#include "prefs.h"
#include "vroot.h"

#ifdef HAVE_GDK_PIXBUF

# ifdef HAVE_GTK2
#  include <gdk-pixbuf-xlib/gdk-pixbuf-xlib.h>
# else  /* !HAVE_GTK2 */
#  include <gdk-pixbuf/gdk-pixbuf-xlib.h>
# endif /* !HAVE_GTK2 */

# define HAVE_BUILTIN_IMAGE_LOADER
#endif /* HAVE_GDK_PIXBUF */


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

  exit (-1);
}

static int
x_ehandler (Display *dpy, XErrorEvent *error)
{
  fprintf (stderr, "\nX error in %s:\n", progname);
  XmuPrintDefaultErrorMessage (dpy, error, stderr);
  exit (-1);
  return 0;
}



#ifdef HAVE_BUILTIN_IMAGE_LOADER
static void load_image_internal (Screen *screen, Window window,
                                 int win_width, int win_height,
                                 Bool verbose_p,
                                 int ac, char **av);
#endif /* HAVE_BUILTIN_IMAGE_LOADER */


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

  XWindowAttributes xgwa;
  XGetWindowAttributes (dpy, window, &xgwa);
  screen = xgwa.screen;

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
      Bool changed_p = False;
      if (desk_p)  desk_p  = False, changed_p = True;
      if (video_p) video_p = False, changed_p = True;
# ifndef HAVE_BUILTIN_IMAGE_LOADER
      if (image_p) image_p = False, changed_p = True;
      if (changed_p && verbose_p)
        fprintf (stderr, "%s: not a top-level window: using colorbars.\n",
                 progname);
# endif /* !HAVE_BUILTIN_IMAGE_LOADER */
    }
  else if (window != VirtualRootWindowOfScreen (screen))
    {
      Bool changed_p = False;
      if (video_p) video_p = False, changed_p = True;
# ifndef HAVE_BUILTIN_IMAGE_LOADER
      if (!desk_p) desk_p  = True,  changed_p = True;
      if (image_p) image_p = False, changed_p = True;
      if (changed_p && verbose_p)
        fprintf (stderr,
                 "%s: not running on root window: grabbing desktop.\n",
                 progname);
# endif /* !HAVE_BUILTIN_IMAGE_LOADER */
    }

  count = 0;
  if (desk_p)  count++;
  if (video_p) count++;
  if (image_p) count++;

  if (count == 0)
    which = do_bars;
  else
    {
      int i = 0;
      while (1)  /* loop until we get one that's permitted */
        {
          which = (random() % 3);
          if (which == do_desk  && desk_p)  break;
          if (which == do_video && video_p) break;
          if (which == do_image && image_p) break;
          if (++i > 200) abort();
        }
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
      if (verbose_p)
        fprintf (stderr, "%s: drawing colorbars\n", progname);
      draw_colorbars (dpy, window, 0, 0, xgwa.width, xgwa.height);
      XSync (dpy, False);
    }
  else
    {
      char *av[10];
      int ac = 0;
      memset (av, 0, sizeof(av));
      switch (which)
        {
        case do_video:
          if (verbose_p)
            fprintf (stderr, "%s: grabbing video\n", progname);
          av[ac++] = GETIMAGE_VIDEO_PROGRAM;
          break;
        case do_image:
          if (verbose_p)
            fprintf (stderr, "%s: loading random image file\n", progname);
          av[ac++] = GETIMAGE_FILE_PROGRAM;

# ifdef HAVE_BUILTIN_IMAGE_LOADER
          av[ac++] = "--name";
# endif /* !HAVE_BUILTIN_IMAGE_LOADER */
          av[ac++] = dir;
          break;
        default:
          abort();
          break;
        }

      if (verbose_p)
        {
          int i;
          for (i = ac; i > 1; i--)
            av[i] = av[i-1];
          av[1] = strdup ("--verbose");
          ac++;
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
        char *s;
        int screen_no = screen_number (screen);  /* might not be default now */

        strcpy (ndpy, "DISPLAY=");
        s = ndpy + strlen(ndpy);
        strcpy (s, odpy);

        while (*s && *s != ':') s++;		/* skip to colon */
        while (*s == ':') s++;			/* skip over colons */
        while (isdigit(*s)) s++;		/* skip over dpy number */
        while (*s == '.') s++;			/* skip over dot */
        if (s[-1] != '.') *s++ = '.';		/* put on a dot */
        sprintf(s, "%d", screen_no);		/* put on screen number */

        if (putenv (ndpy))
          abort ();

        /* don't free (ndpy) -- some implementations of putenv (BSD
           4.4, glibc 2.0) copy the argument, but some (libc4,5, glibc
           2.1.2) do not.  So we must leak it (and/or the previous
           setting).  Yay.
         */
      }
# endif /* HAVE_PUTENV */

# ifdef HAVE_BUILTIN_IMAGE_LOADER
      if (which == do_image)
        {
          load_image_internal (screen, window, xgwa.width, xgwa.height,
                               verbose_p, ac, av);
          return;
        }
# endif /* HAVE_BUILTIN_IMAGE_LOADER */


      close (ConnectionNumber (dpy));	/* close display fd */

      execvp (av[0], av);		/* shouldn't return */
      exec_error (av);
    }
}


#ifdef HAVE_BUILTIN_IMAGE_LOADER

/* Reads a filename from "GETIMAGE_FILE_PROGRAM --name /DIR"
 */
static char *
get_filename (Display *dpy, int ac, char **av)
{
  pid_t forked;
  int fds [2];
  int in, out;
  char buf[1024];

  if (pipe (fds))
    {
      sprintf (buf, "%s: error creating pipe", progname);
      perror (buf);
      return 0;
    }

  in = fds [0];
  out = fds [1];

  switch ((int) (forked = fork ()))
    {
    case -1:
      {
        sprintf (buf, "%s: couldn't fork", progname);
        perror (buf);
        return 0;
      }
    case 0:
      {
        int stdout_fd = 1;

        close (in);  /* don't need this one */
        close (ConnectionNumber (dpy));		/* close display fd */

        if (dup2 (out, stdout_fd) < 0)		/* pipe stdout */
          {
            sprintf (buf, "%s: could not dup() a new stdout", progname);
            exit (-1);                          /* exits fork */
          }

        execvp (av[0], av);			/* shouldn't return. */
        exit (-1);                              /* exits fork */
        break;
      }
    default:
      {
        int wait_status = 0;
        FILE *f = fdopen (in, "r");
        int L;

        close (out);  /* don't need this one */
        *buf = 0;
        fgets (buf, sizeof(buf)-1, f);
        fclose (f);

        /* Wait for the child to die. */
        waitpid (-1, &wait_status, 0);

        L = strlen (buf);
        while (L && buf[L-1] == '\n')
          buf[--L] = 0;
          
        return strdup (buf);
      }
    }

  abort();
}



static void
load_image_internal (Screen *screen, Window window,
                     int win_width, int win_height,
                     Bool verbose_p,
                     int ac, char **av)
{
  GdkPixbuf *pb;
  Display *dpy = DisplayOfScreen (screen);
  char *filename = get_filename (dpy, ac, av);

  if (!filename)
    {
      fprintf (stderr, "%s: no file name returned by %s\n",
               progname, av[0]);
      goto FAIL;
    }
  else if (verbose_p)
    fprintf (stderr, "%s: loading \"%s\"\n", progname, filename);

  gdk_pixbuf_xlib_init (dpy, screen_number (screen));
  xlib_rgb_init (dpy, screen);

  pb = gdk_pixbuf_new_from_file (filename
#ifdef HAVE_GTK2
				 , NULL
#endif /* HAVE_GTK2 */
	  );

  if (pb)
    {
      int w = gdk_pixbuf_get_width (pb);
      int h = gdk_pixbuf_get_height (pb);
      int srcx, srcy, destx, desty;

      Bool exact_fit_p = ((w == win_width  && h <= win_height) ||
                          (h == win_height && w <= win_width));

      if (!exact_fit_p)  /* scale the image up or down */
        {
          float rw = (float) win_width  / w;
          float rh = (float) win_height / h;
          float r = (rw < rh ? rw : rh);
          int tw = w * r;
          int th = h * r;
          int pct = (r * 100);

          if (pct < 95 || pct > 105)  /* don't scale if it's close */
            {
              GdkPixbuf *pb2;
              if (verbose_p)
                fprintf (stderr,
                         "%s: scaling image by %d%% (%dx%d -> %dx%d)\n",
                         progname, pct, w, h, tw, th);

              pb2 = gdk_pixbuf_scale_simple (pb, tw, th, GDK_INTERP_BILINEAR);
              if (pb2)
                {
                  gdk_pixbuf_unref (pb);
                  pb = pb2;
                  w = tw;
                  h = th;
                }
              else
                fprintf (stderr, "%s: out of memory when scaling?\n",
                         progname);
            }
        }

      /* Center the image on the window. */
      srcx = 0;
      srcy = 0;
      destx = (win_width  - w) / 2;
      desty = (win_height - h) / 2;
      if (destx < 0) srcx = -destx, destx = 0;
      if (desty < 0) srcy = -desty, desty = 0;

      if (win_width  < w) w = win_width;
      if (win_height < h) h = win_height;

      /* #### Note that this always uses the default colormap!  Morons!
              Owen says that in Gnome 2.0, I should try using
              gdk_pixbuf_render_pixmap_and_mask_for_colormap() instead.
              But I don't have Gnome 2.0 yet.
       */
      XClearWindow (dpy, window);
      gdk_pixbuf_xlib_render_to_drawable_alpha (pb, window,
                                                srcx, srcy, destx, desty, w, h,
                                                GDK_PIXBUF_ALPHA_FULL, 127,
                                                XLIB_RGB_DITHER_NORMAL, 0, 0);
      XSync (dpy, False);

      if (verbose_p)
        fprintf (stderr, "%s: displayed %dx%d image at %d,%d.\n",
                 progname, w, h, destx, desty);
    }
  else if (filename)
    {
      fprintf (stderr, "%s: unable to load %s\n", progname, filename);
      goto FAIL;
    }
  else
    {
      fprintf (stderr, "%s: unable to initialize built-in images\n", progname);
      goto FAIL;
    }

  return;

 FAIL:
  if (verbose_p)
    fprintf (stderr, "%s: drawing colorbars\n", progname);
  draw_colorbars (dpy, window, 0, 0, win_width, win_height);
  XSync (dpy, False);
}

#endif /* HAVE_BUILTIN_IMAGE_LOADER */



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
