/* xscreensaver, Copyright (c) 1997 Jamie Zawinski <jwz@netscape.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This is a kludge that lets xscreensaver work with SGI demos that expect
   to be run from `haven'.  It runs the program given on the command line,
   then waits for an X window to be created whose name is that of the 
   program.  Then, it resizes that window to fill the screen.  Run it
   like this:

       xscreensaver-sgigl /usr/demos/bin/ep -S
       xscreensaver-sgigl -n ant /usr/demos/General_Demos/ant/RUN
       xscreensaver-sgigl -n atlantis /usr/demos/General_Demos/atlantis/RUN
       xscreensaver-sgigl -n /usr/demos/General_Demos/powerflip/powerflip \
          /usr/demos/General_Demos/powerflip/RUN

   Except that doesn't really work.  You have to do this instead:

       xscreensaver-sgigl -n ant ant.sh

   where ant.sh contains

       #!/bin/sh
       cd /usr/demos/General_Demos/ant
       exec ./ant -S

   There's no way to make this work with powerflip at all, since it doesn't
   take a -S option to run in the foreground.
 */

/* #### Another way to do this would be:
   instead of exec'ing the hack, fork it; then wait for that fork to die.
   If it dies, but the window ID is still valid, then that means the 
   sub-process has forked itself (as those fuckwits at SGI are wont to do.)
   In that case, this process should go to sleep, and set up a signal handler
   that will destroy the X window when it is killed.  That way, the caller
   is given a foreground pid which, when killed, will cause the hack to die
   (by a roundabout mechanism.)

   This would all be so much simpler if those assholes would just:

   1: get out of the habit of writing programs that magically background
      themselves, and

   2: give the fucking programs arguments which control the window size
      instead of always making 100x100 windows!

   I won't even dream of having a "-root" option that understood virtual-roots;
   that would just be too outlandish to even dream about.
 */

static char *progname;

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/Xmu/Error.h>

#include "vroot.h"
#undef RootWindowOfScreen
#undef RootWindow
#undef DefaultRootWindow


static int
BadWindow_ehandler (Display *dpy, XErrorEvent *error)
{
  if (error->error_code == BadWindow ||
      error->error_code == BadMatch ||
      error->error_code == BadDrawable)
    return 0;
  else
    {
      fprintf (stderr, "\nX error in %s:\n", progname);
      if (XmuPrintDefaultErrorMessage (dpy, error, stderr))
	exit(1);
      else
	fprintf (stderr, " (nonfatal.)\n");
    }
  return 0;
}


void
main(int ac, char **av)
{
  char buf [512];
  pid_t parent, forked;
  Display *dpy;
  Screen *screen;
  char *s;
  char *n1 = 0;
  char *n2 = 0;
  Bool verbose = False;
  Window root, vroot;

  progname = av[0];

  s = strrchr(progname, '/');
  if (s) progname = s+1;

  if (ac < 1)
    {
      fprintf(stderr,
	      "usage: %s [ -v ] [ -n window-name ] program arguments...\n",
	      progname);
      exit(1);
    }

  if (ac > 2 && !strcmp(av[1], "-v"))
    {
      verbose = True;
      av++;
      ac--;
    }

  if (ac > 2 && !strcmp(av[1], "-n"))
    {
      n2 = av[2];
      av += 2;
      ac -= 2;
    }

  n1 = strrchr(av[1], '/');
  if (n1) n1++;
  else n1 = av[1];


  dpy = XOpenDisplay(0);
  if (!dpy)
    {
      fprintf(stderr, "%s: couldn't open display\n", progname);
      exit(1);
    }

  screen = DefaultScreenOfDisplay(dpy);
  root   = XRootWindowOfScreen (screen);
  vroot  = VirtualRootWindowOfScreen (screen);

  XSelectInput (dpy, root, SubstructureNotifyMask);
  if (root != vroot)
    XSelectInput (dpy, vroot, SubstructureNotifyMask);

  XSetErrorHandler (BadWindow_ehandler);

  if (verbose)
    fprintf(stderr, "%s: selected SubstructureNotifyMask on 0x%x / 0x%x\n",
	    progname, root, vroot);

  parent = getpid();

  if (verbose)
    fprintf(stderr, "%s: pid is %d\n", progname, parent);

  switch ((int) (forked = fork ()))
    {
    case -1:
      {
	sprintf (buf, "%s: couldn't fork", progname);
	perror (buf);
	exit (1);
	break;
      }
    case 0:	/* forked */
      {
	time_t start = time((time_t) 0);
	XEvent event;

	if (verbose)
	  fprintf(stderr, "%s: forked pid is %d\n", progname, getpid());

	while (1)
	  {
	    XNextEvent(dpy, &event);

	    if (event.xany.type == CreateNotify)
	      {
		char *name = 0;
		Window w = event.xcreatewindow.window;
		XSync(dpy, False);

		XFetchName(dpy, w, &name);
		if (!name)
		  {
		    /* Try again to see if the name has been set later... */
		    XSync(dpy, False);
		    sleep(1);
		    XFetchName(dpy, w, &name);
		  }

		if (name &&
		    ((n1 && !strcmp(name, n1)) ||
		     (n2 && !strcmp(name, n2))))
		  {
		    if (verbose)
		      fprintf(stderr, "%s: resizing 0x%x\n", progname, w);

		    XMoveResizeWindow(dpy, w, 0, 0,
				      WidthOfScreen(screen),
				      HeightOfScreen(screen));

#if 0
		    if (vroot && vroot != root &&
			event.xcreatewindow.parent == root)
		      {
			if (verbose)
			  fprintf(stderr,
				  "%s: reparenting 0x%x from 0x%x to 0x%x\n",
				  progname, w, root, vroot);
			XReparentWindow(dpy, w, vroot, 0, 0);
		      }
#endif

		    XSync(dpy, False);
		    fflush(stdout);
		    fflush(stderr);
		    exit(0);	/* Note that this only exits a child fork.  */
		  }
	      }

	    if (start + 5 < time((time_t) 0))
	      {
		fprintf(stderr,
		    "%s: timed out: no window named \"%s\" has been created\n",
			progname, (n2 ? n2 : n1));
		fflush(stdout);
		fflush(stderr);
		kill(parent, SIGTERM);
		exit(1);
	      }
	  }
	break;
      }
    default:	/* foreground */
      {
	close (ConnectionNumber (dpy));		/* close display fd */
	execvp (av[1], av+1);			/* shouldn't return. */
	sprintf (buf, "%s: execvp(\"%s\") failed", progname, av[1]);
	perror (buf);
	fflush(stderr);
	fflush(stdout);
	exit (1);
	break;
      }
    }
}
