/* xscreensaver, Copyright (c) 1991-1996 Jamie Zawinski <jwz@netscape.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xos.h>
#include <X11/Xmu/SysUtil.h>

#include <signal.h>		/* for the signal names */

#include "xscreensaver.h"

#ifdef HAVE_MIT_SAVER_EXTENSION
#include <X11/extensions/scrnsaver.h>
#endif /* HAVE_MIT_SAVER_EXTENSION */

#ifdef HAVE_SGI_SAVER_EXTENSION
#include <X11/extensions/XScreenSaver.h>
#endif /* HAVE_SGI_SAVER_EXTENSION */

extern Bool use_mit_saver_extension;
extern Bool use_sgi_saver_extension;

#if __STDC__
extern int kill (pid_t, int);		/* signal() is in sys/signal.h... */
#endif /* __STDC__ */

extern Time timeout;

extern Bool lock_p, demo_mode_p;

Atom XA_VROOT, XA_XSETROOT_ID;
Atom XA_SCREENSAVER_VERSION, XA_SCREENSAVER_ID;

#if __STDC__
extern void describe_visual (FILE *, Display *, Visual *);
extern void reset_stderr (void);
#endif

Window screensaver_window = 0;
Cursor cursor;
Colormap cmap, cmap2;
Bool install_cmap_p;
Bool fade_p, unfade_p;
int fade_seconds, fade_ticks;

static unsigned long black_pixel;
static Window real_vroot, real_vroot_value;

#ifdef HAVE_MIT_SAVER_EXTENSION
Window server_mit_saver_window = 0;
#endif /* HAVE_MIT_SAVER_EXTENSION */

#define ALL_POINTER_EVENTS \
	(ButtonPressMask | ButtonReleaseMask | EnterWindowMask | \
	 LeaveWindowMask | PointerMotionMask | PointerMotionHintMask | \
	 Button1MotionMask | Button2MotionMask | Button3MotionMask | \
	 Button4MotionMask | Button5MotionMask | ButtonMotionMask)

/* I don't really understand Sync vs Async, but these seem to work... */
#define grab_kbd(win) \
  XGrabKeyboard (dpy, (win), True, GrabModeSync, GrabModeAsync, CurrentTime)
#define grab_mouse(win) \
  XGrabPointer (dpy, (win), True, ALL_POINTER_EVENTS, \
		GrabModeAsync, GrabModeAsync, None, cursor, CurrentTime)

void
grab_keyboard_and_mouse P((void))
{
  Status status;
  XSync (dpy, False);

  if (demo_mode_p) return;

  status = grab_kbd (screensaver_window);
  if (status != GrabSuccess)
    {	/* try again in a second */
      sleep (1);
      status = grab_kbd (screensaver_window);
      if (status != GrabSuccess)
	fprintf (stderr, "%s: %scouldn't grab keyboard!  (%d)\n",
		 progname, (verbose_p ? "## " : ""), status);
    }
  status = grab_mouse (screensaver_window);
  if (status != GrabSuccess)
    {	/* try again in a second */
      sleep (1);
      status = grab_mouse (screensaver_window);
      if (status != GrabSuccess)
	fprintf (stderr, "%s: %scouldn't grab pointer!  (%d)\n",
		 progname, (verbose_p ? "## " : ""), status);
    }
}

void
ungrab_keyboard_and_mouse P((void))
{
  XUngrabPointer (dpy, CurrentTime);
  XUngrabKeyboard (dpy, CurrentTime);
}


void
ensure_no_screensaver_running ()
{
  int i;
  Window root = RootWindowOfScreen (screen);
  Window root2, parent, *kids;
  unsigned int nkids;
  int (*old_handler) ();

  old_handler = XSetErrorHandler (BadWindow_ehandler);

  if (! XQueryTree (dpy, root, &root2, &parent, &kids, &nkids))
    abort ();
  if (root != root2)
    abort ();
  if (parent)
    abort ();
  for (i = 0; i < nkids; i++)
    {
      Atom type;
      int format;
      unsigned long nitems, bytesafter;
      char *version;

      if (XGetWindowProperty (dpy, kids[i], XA_SCREENSAVER_VERSION, 0, 1,
			      False, XA_STRING, &type, &format, &nitems,
			      &bytesafter, (unsigned char **) &version)
	  == Success
	  && type != None)
	{
	  char *id;
	  if (!XGetWindowProperty (dpy, kids[i], XA_SCREENSAVER_ID, 0, 512,
				   False, XA_STRING, &type, &format, &nitems,
				   &bytesafter, (unsigned char **) &id)
	      == Success
	      || type == None)
	    id = "???";

	  fprintf (stderr,
      "%s: %salready running on display %s (window 0x%x)\n from process %s.\n",
		   progname, (verbose_p ? "## " : ""), DisplayString (dpy),
		   (int) kids [i], id);
	  exit (1);
	}
    }

  if (kids) XFree ((char *) kids);
  XSync (dpy, False);
  XSetErrorHandler (old_handler);
}


void
disable_builtin_screensaver ()
{
  int server_timeout, server_interval, prefer_blank, allow_exp;
  /* Turn off the server builtin saver if it is now running. */
  XForceScreenSaver (dpy, ScreenSaverReset);
  XGetScreenSaver (dpy, &server_timeout, &server_interval,
		   &prefer_blank, &allow_exp);

#if defined(HAVE_MIT_SAVER_EXTENSION) || defined(HAVE_SGI_SAVER_EXTENSION)
  if (use_mit_saver_extension || use_sgi_saver_extension)
    {
      /* Override the values specified with "xset" with our own parameters. */
      allow_exp = True;
      server_interval = 0;
      server_timeout = (timeout / 1000);

      /* The SGI extension won't give us events unless blanking is on.
	 I think (unsure right now) that the MIT extension is the opposite. */
      prefer_blank = (use_sgi_saver_extension ? True : False);

      if (verbose_p)
	fprintf (stderr,
		 "%s: configuring server for saver timeout of %d seconds.\n",
		 progname, server_timeout);
      XSetScreenSaver (dpy, server_timeout, server_interval,
		       prefer_blank, allow_exp);
    }
  else
#endif /* HAVE_MIT_SAVER_EXTENSION || HAVE_SGI_SAVER_EXTENSION */
  if (server_timeout != 0)
    {
      server_timeout = 0;
      XSetScreenSaver (dpy, server_timeout, server_interval,
		       prefer_blank, allow_exp);
      printf ("%s%sisabling server builtin screensaver.\n\
	You can re-enable it with \"xset s on\".\n",
	      (verbose_p ? "" : progname), (verbose_p ? "\n\tD" : ": d"));
    }
}


/* Virtual-root hackery */

#ifdef _VROOT_H_
ERROR!  You must not include vroot.h in this file.
#endif

static void
#if __STDC__
store_vroot_property (Window win, Window value)
#else
store_vroot_property (win, value)
     Window win, value;
#endif
{
#if 0
  printf ("%s: storing XA_VROOT = 0x%x (%s) = 0x%x (%s)\n", progname, 
	  win,
	  (win == screensaver_window ? "ScreenSaver" :
	   (win == real_vroot ? "VRoot" :
	    (win == real_vroot_value ? "Vroot_value" : "???"))),
	  value,
	  (value == screensaver_window ? "ScreenSaver" :
	   (value == real_vroot ? "VRoot" :
	    (value == real_vroot_value ? "Vroot_value" : "???"))));
#endif
  XChangeProperty (dpy, win, XA_VROOT, XA_WINDOW, 32, PropModeReplace,
		   (unsigned char *) &value, 1);
}

static void
#if __STDC__
remove_vroot_property (Window win)
#else
remove_vroot_property (win)
     Window win;
#endif
{
#if 0
  printf ("%s: removing XA_VROOT from 0x%x (%s)\n", progname, win, 
	  (win == screensaver_window ? "ScreenSaver" :
	   (win == real_vroot ? "VRoot" :
	    (win == real_vroot_value ? "Vroot_value" : "???"))));
#endif
  XDeleteProperty (dpy, win, XA_VROOT);
}


static void
kill_xsetroot_data P((void))
{
  Atom type;
  int format;
  unsigned long nitems, bytesafter;
  Pixmap *dataP = 0;

  /* If the user has been using xv or xsetroot as a screensaver (to display
     an image on the screensaver window, as a kind of slideshow) then the
     pixmap and its associated color cells have been put in RetainPermanent
     CloseDown mode.  Since we're not destroying the xscreensaver window,
     but merely unmapping it, we need to free these resources or those
     colormap cells will stay allocated while the screensaver is off.  (We
     could just delete the screensaver window and recreate it later, but
     that could cause other problems.)  This code does an atomic read-and-
     delete of the _XSETROOT_ID property, and if it held a pixmap, then we
     cause the RetainPermanent resources of the client which created it
     (and which no longer exists) to be freed.
   */
  if (XGetWindowProperty (dpy, screensaver_window, XA_XSETROOT_ID, 0, 1,
			  True, AnyPropertyType, &type, &format, &nitems, 
			  &bytesafter, (unsigned char **) &dataP)
      == Success
      && type != None)
    {
      if (dataP && *dataP && type == XA_PIXMAP && format == 32 &&
	  nitems == 1 && bytesafter == 0)
	{
	  if (verbose_p)
	    printf ("%s: destroying xsetroot data (0x%lX).\n",
		    progname, *dataP);
	  XKillClient (dpy, *dataP);
	}
      else
	fprintf (stderr, "%s: %sdeleted unrecognised _XSETROOT_ID property: \n\
	%lu, %lu; type: %lu, format: %d, nitems: %lu, bytesafter %ld\n",
		 progname, (verbose_p ? "## " : ""),
		 (unsigned long) dataP, (dataP ? *dataP : 0), type,
		 format, nitems, bytesafter);
    }
}


static void handle_signals P((Bool on_p));

static void
save_real_vroot P((void))
{
  int i;
  Window root = RootWindowOfScreen (screen);
  Window root2, parent, *kids;
  unsigned int nkids;

  real_vroot = 0;
  real_vroot_value = 0;
  if (! XQueryTree (dpy, root, &root2, &parent, &kids, &nkids))
    abort ();
  if (root != root2)
    abort ();
  if (parent)
    abort ();
  for (i = 0; i < nkids; i++)
    {
      Atom type;
      int format;
      unsigned long nitems, bytesafter;
      Window *vrootP = 0;

      if (XGetWindowProperty (dpy, kids[i], XA_VROOT, 0, 1, False, XA_WINDOW,
			      &type, &format, &nitems, &bytesafter,
			      (unsigned char **) &vrootP)
	  != Success)
	continue;
      if (! vrootP)
	continue;
      if (real_vroot)
	{
	  if (*vrootP == screensaver_window) abort ();
	  fprintf (stderr,
	    "%s: %smore than one virtual root window found (0x%x and 0x%x).\n",
		   progname, (verbose_p ? "## " : ""),
		   (int) real_vroot, (int) kids [i]);
	  exit (1);
	}
      real_vroot = kids [i];
      real_vroot_value = *vrootP;
    }

  if (real_vroot)
    {
      handle_signals (True);
      remove_vroot_property (real_vroot);
      XSync (dpy, False);
    }

  XFree ((char *) kids);
}

static Bool
restore_real_vroot_1 P((void))
{
  if (verbose_p && real_vroot)
    printf ("%s: restoring __SWM_VROOT property on the real vroot (0x%lx).\n",
	    progname, (unsigned long) real_vroot);
  remove_vroot_property (screensaver_window);
  if (real_vroot)
    {
      store_vroot_property (real_vroot, real_vroot_value);
      real_vroot = 0;
      real_vroot_value = 0;
      /* make sure the property change gets there before this process
	 terminates!  We might be doing this because we have intercepted
	 SIGTERM or something. */
      XSync (dpy, False);
      return True;
    }
  return False;
}

void
restore_real_vroot ()
{
  if (restore_real_vroot_1 ())
    handle_signals (False);
}


/* Signal hackery to ensure that the vroot doesn't get left in an 
   inconsistent state
 */

static const char *sig_names [255] = { 0 };

static void
restore_real_vroot_handler (sig)
     int sig;
{
  signal (sig, SIG_DFL);
  if (restore_real_vroot_1 ())
    fprintf (stderr, "\n%s: %s%s (%d) intercepted, vroot restored.\n",
	     progname, (verbose_p ? "## " : ""),
	     ((sig < sizeof(sig_names) && sig >= 0 && sig_names [sig])
	      ? sig_names [sig] : "unknown signal"),
	     sig);
  kill (getpid (), sig);
}


static void
#if __STDC__
catch_signal (int sig, char *signame, Bool on_p)
#else
catch_signal (sig, signame, on_p)
     int sig;
     char *signame;
     Bool on_p;
#endif
{
  if (! on_p)
    signal (sig, SIG_DFL);
  else
    {
      sig_names [sig] = signame;
      if (((int) signal (sig, restore_real_vroot_handler)) == -1)
	{
	  char buf [255];
	  sprintf (buf, "%s: %scouldn't catch %s (%d)", progname,
		   (verbose_p ? "## " : ""), signame, sig);
	  perror (buf);
	  restore_real_vroot ();
	  exit (1);
	}
    }
}

static void
handle_signals (on_p)
     Bool on_p;
{
#if 0
  if (on_p) printf ("handling signals\n");
  else printf ("unhandling signals\n");
#endif

  catch_signal (SIGHUP,  "SIGHUP",  on_p);
  catch_signal (SIGINT,  "SIGINT",  on_p);
  catch_signal (SIGQUIT, "SIGQUIT", on_p);
  catch_signal (SIGILL,  "SIGILL",  on_p);
  catch_signal (SIGTRAP, "SIGTRAP", on_p);
  catch_signal (SIGIOT,  "SIGIOT",  on_p);
  catch_signal (SIGABRT, "SIGABRT", on_p);
#ifdef SIGEMT
  catch_signal (SIGEMT,  "SIGEMT",  on_p);
#endif
  catch_signal (SIGFPE,  "SIGFPE",  on_p);
  catch_signal (SIGBUS,  "SIGBUS",  on_p);
  catch_signal (SIGSEGV, "SIGSEGV", on_p);
#ifdef SIGSYS
  catch_signal (SIGSYS,  "SIGSYS",  on_p);
#endif
  catch_signal (SIGTERM, "SIGTERM", on_p);
#ifdef SIGXCPU
  catch_signal (SIGXCPU, "SIGXCPU", on_p);
#endif
#ifdef SIGXFSZ
  catch_signal (SIGXFSZ, "SIGXFSZ", on_p);
#endif
#ifdef SIGDANGER
  catch_signal (SIGDANGER, "SIGDANGER", on_p);
#endif
}


/* Managing the actual screensaver window */

Bool
window_exists_p (dpy, window)
     Display *dpy;
     Window window;
{
  int (*old_handler) ();
  XWindowAttributes xgwa;
  xgwa.screen = 0;
  old_handler = XSetErrorHandler (BadWindow_ehandler);
  XGetWindowAttributes (dpy, window, &xgwa);
  XSync (dpy, False);
  XSetErrorHandler (old_handler);
  return (xgwa.screen != 0);
}

void
initialize_screensaver_window P((void))
{
  /* This resets the screensaver window as fully as possible, since there's
     no way of knowing what some random client may have done to us in the
     meantime.  We could just destroy and recreate the window, but that has
     its own set of problems...
   */
  XColor black;
  XClassHint class_hints;
  XSetWindowAttributes attrs;
  unsigned long attrmask;
  int width = WidthOfScreen (screen);
  int height = HeightOfScreen (screen);
  char id [2048];

  reset_stderr ();

  black.red = black.green = black.blue = 0;

  if (cmap == DefaultColormapOfScreen (screen))
    cmap = 0;

  if (install_cmap_p || visual != DefaultVisualOfScreen (screen))
    {
      if (! cmap)
	{
	  cmap = XCreateColormap (dpy, RootWindowOfScreen (screen),
				  visual, AllocNone);
	  if (! XAllocColor (dpy, cmap, &black)) abort ();
	  black_pixel = black.pixel;
	}
    }
  else
    {
      if (cmap)
	{
	  XFreeColors (dpy, cmap, &black_pixel, 1, 0);
	  XFreeColormap (dpy, cmap);
	}
      cmap = DefaultColormapOfScreen (screen);
      black_pixel = BlackPixelOfScreen (screen);
    }

  if (cmap2)
    {
      XFreeColormap (dpy, cmap2);
      cmap2 = 0;
    }

  if (fade_p)
    {
      cmap2 = copy_colormap (dpy, cmap, 0);
      if (! cmap2)
	fade_p = unfade_p = 0;
    }

  attrmask = (CWOverrideRedirect | CWEventMask | CWBackingStore | CWColormap |
	      CWBackPixel | CWBackingPixel | CWBorderPixel);
  attrs.override_redirect = True;

  /* When use_mit_saver_extension or use_sgi_saver_extension is true, we won't
     actually be reading these events during normal operation; but we still
     need to see Button events for demo-mode to work properly.
   */
  attrs.event_mask = (KeyPressMask | KeyReleaseMask |
		      ButtonPressMask | ButtonReleaseMask |
		      PointerMotionMask);

  attrs.backing_store = NotUseful;
  attrs.colormap = cmap;
  attrs.background_pixel = black_pixel;
  attrs.backing_pixel = black_pixel;
  attrs.border_pixel = black_pixel;

#if 0
  if (demo_mode_p || lock_p) width = width / 2;  /* #### */
#endif

  if (screensaver_window || !verbose_p)
    ;
  else if (visual == DefaultVisualOfScreen (screen))
    {
      fprintf (stderr, "%s: using default visual ", progname);
      describe_visual (stderr, dpy, visual);
    }
  else
    {
      fprintf (stderr, "%s: using visual:   ", progname);
      describe_visual (stderr, dpy, visual);
      fprintf (stderr, "%s: default visual: ", progname);
      describe_visual (stderr, dpy, DefaultVisualOfScreen (screen));
    }

#ifdef HAVE_MIT_SAVER_EXTENSION
  if (use_mit_saver_extension)
    {
      XScreenSaverInfo *info;
      Window root = RootWindowOfScreen (screen);

      /* This call sets the server screensaver timeouts to what we think
	 they should be (based on the resources and args xscreensaver was
	 started with.)  It's important that we do this to sync back up
	 with the server - if we have turned on prematurely, as by an
	 ACTIVATE ClientMessage, then the server may decide to activate
	 the screensaver while it's already active.  That's ok for us,
	 since we would know to ignore that ScreenSaverActivate event,
	 but a side effect of this would be that the server would map its
	 saver window (which we then hide again right away) meaning that
	 the bits currently on the screen get blown away.  Ugly. */
#if 0
      /* #### Ok, that doesn't work - when we tell the server that the
	 screensaver is "off" it sends us a Deactivate event, which is
	 sensible... but causes the saver to never come on.  Hmm. */
      disable_builtin_screensaver ();
#endif /* 0 */

#if 0
      /* #### The MIT-SCREEN-SAVER extension gives us access to the
	 window that the server itself uses for saving the screen.
	 However, using this window in any way, in particular, calling
	 XScreenSaverSetAttributes() as below, tends to make the X server
	 crash.  So fuck it, let's try and get along without using it...

	 It's also inconvenient to use this window because it doesn't
	 always exist (though the ID is constant.)  So to use this
	 window, we'd have to reimplement the ACTIVATE ClientMessage to
	 tell the *server* to tell *us* to turn on, to cause the window
	 to get created at the right time.  Gag.  */
      XScreenSaverSetAttributes (dpy, root,
				 0, 0, width, height, 0,
				 visual_depth, InputOutput, visual,
				 attrmask, &attrs);
      XSync (dpy, False);
#endif /* 0 */

      info = XScreenSaverAllocInfo ();
      XScreenSaverQueryInfo (dpy, root, info);
      server_mit_saver_window = info->window;
      if (! server_mit_saver_window) abort ();
      XFree (info);
    }
#endif /* HAVE_MIT_SAVER_EXTENSION */

  if (screensaver_window)
    {
      XWindowChanges changes;
      unsigned int changesmask = CWX|CWY|CWWidth|CWHeight|CWBorderWidth;
      changes.x = 0;
      changes.y = 0;
      changes.width = width;
      changes.height = height;
      changes.border_width = 0;

      XConfigureWindow (dpy, screensaver_window, changesmask, &changes);
      XChangeWindowAttributes (dpy, screensaver_window, attrmask, &attrs);
    }
  else
    {
      screensaver_window =
	XCreateWindow (dpy, RootWindowOfScreen (screen), 0, 0, width, height,
		       0, visual_depth, InputOutput, visual, attrmask,
		       &attrs);
    }

#ifdef HAVE_MIT_SAVER_EXTENSION
  if (!use_mit_saver_extension ||
      window_exists_p (dpy, screensaver_window))
    /* When using the MIT-SCREEN-SAVER extension, the window pointed to
       by screensaver_window only exists while the saver is active.
       So we must be careful to only try and manipulate it while it
       exists...
     */
#endif /* HAVE_MIT_SAVER_EXTENSION */
    {
      class_hints.res_name = progname;
      class_hints.res_class = progclass;
      XSetClassHint (dpy, screensaver_window, &class_hints);
      XStoreName (dpy, screensaver_window, "screensaver");
      XChangeProperty (dpy, screensaver_window, XA_SCREENSAVER_VERSION,
		       XA_STRING, 8, PropModeReplace,
		       (unsigned char *) screensaver_version,
		       strlen (screensaver_version));

      sprintf (id, "%d on host ", getpid ());
      if (! XmuGetHostname (id + strlen (id), sizeof (id) - strlen (id) - 1))
	strcat (id, "???");
      XChangeProperty (dpy, screensaver_window, XA_SCREENSAVER_ID, XA_STRING,
		       8, PropModeReplace, (unsigned char *) id, strlen (id));

      if (!cursor)
	{
	  Pixmap bit;
	  bit = XCreatePixmapFromBitmapData (dpy, screensaver_window, "\000",
					     1, 1,
					     BlackPixelOfScreen (screen),
					     BlackPixelOfScreen (screen), 1);
	  cursor = XCreatePixmapCursor (dpy, bit, bit, &black, &black, 0, 0);
	  XFreePixmap (dpy, bit);
	}

      XSetWindowBackground (dpy, screensaver_window, black_pixel);
      if (! demo_mode_p)
	XDefineCursor (dpy, screensaver_window, cursor);
      else
	XUndefineCursor (dpy, screensaver_window);
    }
}


void 
raise_window (inhibit_fade, between_hacks_p)
     Bool inhibit_fade, between_hacks_p;
{
  initialize_screensaver_window ();

  if (fade_p && !inhibit_fade && !demo_mode_p)
    {
      int grabbed;
      Colormap current_map = (between_hacks_p
			      ? cmap
			      : DefaultColormapOfScreen (screen));
      copy_colormap (dpy, current_map, cmap2);
      if (verbose_p) fprintf (stderr, "%s: fading... ", progname);
      XGrabServer (dpy);
      /* grab and blacken mouse on the root window (saver not mapped yet) */
      grabbed = grab_mouse (RootWindowOfScreen (screen));
      /* fade what's on the screen to black */
      XInstallColormap (dpy, cmap2);
      fade_colormap (dpy, current_map, cmap2, fade_seconds, fade_ticks,
		     True, True);
      if (verbose_p) fprintf (stderr, "fading done.\n");
      XClearWindow (dpy, screensaver_window);
      XMapRaised (dpy, screensaver_window);

#ifdef HAVE_MIT_SAVER_EXTENSION
      if (server_mit_saver_window &&
	  window_exists_p (dpy, server_mit_saver_window))
	XUnmapWindow (dpy, server_mit_saver_window);
#endif /* HAVE_MIT_SAVER_EXTENSION */

      /* Once the saver window is up, restore the colormap.
	 (The "black" pixels of the two colormaps are compatible.) */
      XInstallColormap (dpy, cmap);
      if (grabbed == GrabSuccess)
	XUngrabPointer (dpy, CurrentTime);
      XUngrabServer (dpy);
    }
  else
    {
      XClearWindow (dpy, screensaver_window);
      XMapRaised (dpy, screensaver_window);
#ifdef HAVE_MIT_SAVER_EXTENSION
      if (server_mit_saver_window &&
	  window_exists_p (dpy, server_mit_saver_window))
	XUnmapWindow (dpy, server_mit_saver_window);
#endif /* HAVE_MIT_SAVER_EXTENSION */
    }

  if (install_cmap_p)
    XInstallColormap (dpy, cmap);
}

#ifdef __hpux
 /* Calls to XHPDisableReset and XHPEnableReset must be balanced,
    or BadAccess errors occur. */
static Bool hp_locked_p = False;
#endif /* __hpux */

void
blank_screen ()
{
  save_real_vroot ();
  store_vroot_property (screensaver_window, screensaver_window);
  raise_window (False, False);
  grab_keyboard_and_mouse ();
#ifdef __hpux
  if (lock_p && !hp_locked_p)
    XHPDisableReset (dpy);	/* turn off C-Sh-Reset */
  hp_locked_p = True;
#endif
}

void
unblank_screen ()
{
  if (unfade_p && !demo_mode_p)
    {
      int grabbed;
      Colormap default_map = DefaultColormapOfScreen (screen);
      blacken_colormap (dpy, cmap2);
      if (verbose_p) fprintf (stderr, "%s: unfading... ", progname);
      XGrabServer (dpy);
      /* grab and blacken mouse on the root window. */
      grabbed = grab_mouse (RootWindowOfScreen (screen));
      XInstallColormap (dpy, cmap2);
      XUnmapWindow (dpy, screensaver_window);
      fade_colormap (dpy, default_map, cmap2, fade_seconds, fade_ticks,
		     False, True);
      XInstallColormap (dpy, default_map);
      if (verbose_p) fprintf (stderr, "unfading done.\n");
      if (grabbed == GrabSuccess)
	XUngrabPointer (dpy, CurrentTime);
      XUngrabServer (dpy);
    }
  else
    {
      if (install_cmap_p)
	{
	  XClearWindow (dpy, screensaver_window); /* avoid technicolor */
	  XInstallColormap (dpy, DefaultColormapOfScreen (screen));
	}
      XUnmapWindow (dpy, screensaver_window);
    }


  /* If the focus window does has a non-default colormap, then install
     that colormap as well.  (On SGIs, this will cause both the root map
     and the focus map to be installed simultaniously.  It'd be nice to
     pick up the other colormaps that had been installed, too; perhaps
     XListInstalledColormaps could be used for that?)
   */
  {
    Window focus = 0;
    int revert_to;
    XGetInputFocus (dpy, &focus, &revert_to);
    if (focus && focus != PointerRoot && focus != None)
      {
	XWindowAttributes xgwa;
	Colormap default_map = DefaultColormapOfScreen (screen);
	xgwa.colormap = 0;
	XGetWindowAttributes (dpy, focus, &xgwa);
	if (xgwa.colormap &&
	    xgwa.colormap != default_map)
	  XInstallColormap (dpy, xgwa.colormap);
      }
  }


  kill_xsetroot_data ();
  ungrab_keyboard_and_mouse ();
  restore_real_vroot ();

#ifdef __hpux
  if (lock_p && hp_locked_p)
    XHPEnableReset (dpy);	/* turn C-Sh-Reset back on */
  hp_locked_p = False;
#endif
}
