/* windows.c --- turning the screen black; dealing with visuals, virtual roots.
 * xscreensaver, Copyright (c) 1991-2006 Jamie Zawinski <jwz@jwz.org>
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

#ifdef VMS
# include <unixlib.h>		/* for getpid() */
# include "vms-gtod.h"		/* for gettimeofday() */
#endif /* VMS */

#ifndef VMS
# include <pwd.h>		/* for getpwuid() */
#else /* VMS */
# include "vms-pwd.h"
#endif /* VMS */

#ifdef HAVE_UNAME
# include <sys/utsname.h>	/* for uname() */
#endif /* HAVE_UNAME */

#include <stdio.h>
/* #include <X11/Xproto.h>	/ * for CARD32 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>		/* for XSetClassHint() */
#include <X11/Xatom.h>
#include <X11/Xos.h>		/* for time() */
#include <signal.h>		/* for the signal names */
#include <time.h>
#include <sys/time.h>

/* You might think that to store an array of 32-bit quantities onto a
   server-side property, you would pass an array of 32-bit data quantities
   into XChangeProperty().  You would be wrong.  You have to use an array
   of longs, even if long is 64 bits (using 32 of each 64.)
 */
typedef long PROP32;

#ifdef HAVE_MIT_SAVER_EXTENSION
# include <X11/extensions/scrnsaver.h>
#endif /* HAVE_MIT_SAVER_EXTENSION */

#ifdef HAVE_XF86VMODE
# include <X11/extensions/xf86vmode.h>
#endif /* HAVE_XF86VMODE */

#ifdef HAVE_XINERAMA
# include <X11/extensions/Xinerama.h>
#endif /* HAVE_XINERAMA */

/* This file doesn't need the Xt headers, so stub these types out... */
#undef XtPointer
#define XtAppContext void*
#define XrmDatabase  void*
#define XtIntervalId void*
#define XtPointer    void*
#define Widget       void*

#include "xscreensaver.h"
#include "visual.h"
#include "fade.h"


extern int kill (pid_t, int);		/* signal() is in sys/signal.h... */

Atom XA_VROOT, XA_XSETROOT_ID, XA_ESETROOT_PMAP_ID, XA_XROOTPMAP_ID;
Atom XA_SCREENSAVER, XA_SCREENSAVER_VERSION, XA_SCREENSAVER_ID;
Atom XA_SCREENSAVER_STATUS;


extern saver_info *global_si_kludge;	/* I hate C so much... */

static void maybe_transfer_grabs (saver_screen_info *ssi,
                                  Window old_w, Window new_w, int new_screen);

#define ALL_POINTER_EVENTS \
	(ButtonPressMask | ButtonReleaseMask | EnterWindowMask | \
	 LeaveWindowMask | PointerMotionMask | PointerMotionHintMask | \
	 Button1MotionMask | Button2MotionMask | Button3MotionMask | \
	 Button4MotionMask | Button5MotionMask | ButtonMotionMask)


static const char *
grab_string(int status)
{
  switch (status)
    {
    case GrabSuccess:     return "GrabSuccess";
    case AlreadyGrabbed:  return "AlreadyGrabbed";
    case GrabInvalidTime: return "GrabInvalidTime";
    case GrabNotViewable: return "GrabNotViewable";
    case GrabFrozen:      return "GrabFrozen";
    default:
      {
	static char foo[255];
	sprintf(foo, "unknown status: %d", status);
	return foo;
      }
    }
}

static int
grab_kbd(saver_info *si, Window w, int screen_no)
{
  saver_preferences *p = &si->prefs;
  int status = XGrabKeyboard (si->dpy, w, True,
			      /* I don't really understand Sync vs Async,
				 but these seem to work... */
			      GrabModeSync, GrabModeAsync,
			      CurrentTime);
  if (status == GrabSuccess)
    {
      si->keyboard_grab_window = w;
      si->keyboard_grab_screen = screen_no;
    }

  if (p->verbose_p)
    fprintf(stderr, "%s: %d: grabbing keyboard on 0x%lx... %s.\n",
	    blurb(), screen_no, (unsigned long) w, grab_string(status));
  return status;
}


static int
grab_mouse (saver_info *si, Window w, Cursor cursor, int screen_no)
{
  saver_preferences *p = &si->prefs;
  int status = XGrabPointer (si->dpy, w, True, ALL_POINTER_EVENTS,
			     GrabModeAsync, GrabModeAsync, w,
			     cursor, CurrentTime);
  if (status == GrabSuccess)
    {
      si->mouse_grab_window = w;
      si->mouse_grab_screen = screen_no;
    }

  if (p->verbose_p)
    fprintf(stderr, "%s: %d: grabbing mouse on 0x%lx... %s.\n",
	    blurb(), screen_no, (unsigned long) w, grab_string(status));
  return status;
}


static void
ungrab_kbd(saver_info *si)
{
  saver_preferences *p = &si->prefs;
  XUngrabKeyboard(si->dpy, CurrentTime);
  if (p->verbose_p)
    fprintf(stderr, "%s: %d: ungrabbing keyboard (was 0x%lx).\n",
            blurb(), si->keyboard_grab_screen,
            (unsigned long) si->keyboard_grab_window);
  si->keyboard_grab_window = 0;
}


static void
ungrab_mouse(saver_info *si)
{
  saver_preferences *p = &si->prefs;
  XUngrabPointer(si->dpy, CurrentTime);
  if (p->verbose_p)
    fprintf(stderr, "%s: %d: ungrabbing mouse (was 0x%lx).\n",
            blurb(), si->mouse_grab_screen,
            (unsigned long) si->mouse_grab_window);
  si->mouse_grab_window = 0;
}


/* Apparently there is this program called "rdesktop" which is a windows
   terminal server client for Unix.  It would seem that this program holds
   the keyboard GRABBED the whole time it has focus!  This is, of course,
   completely idiotic: the whole point of grabbing is to get events when
   you do *not* have focus, so grabbing *only when* you have focus is
   completely redundant -- unless your goal is to make xscreensaver not
   able to ever lock the screen when your program is running.

   If xscreensaver blanks while rdesktop still has a keyboard grab, then
   when we try to prompt for the password, we won't get the characters:
   they'll be typed into rdesktop.

   Perhaps rdesktop will release its keyboard grab if it loses focus?
   What the hell, let's give it a try.  If we fail to grab the keyboard
   four times in a row, we forcibly set focus to "None" and try four
   more times.  (We don't touch focus unless we're already having a hard
   time getting a grab.)
 */
static void
nuke_focus (saver_info *si, int screen_no)
{
  saver_preferences *p = &si->prefs;
  Window focus = 0;
  int rev = 0;

  XGetInputFocus (si->dpy, &focus, &rev);

  if (p->verbose_p)
    {
      char w[255], r[255];

      if      (focus == PointerRoot) strcpy (w, "PointerRoot");
      else if (focus == None)        strcpy (w, "None");
      else    sprintf (w, "0x%lx", (unsigned long) focus);

      if      (rev == RevertToParent)      strcpy (r, "RevertToParent");
      else if (rev == RevertToPointerRoot) strcpy (r, "RevertToPointerRoot");
      else if (rev == RevertToNone)        strcpy (r, "RevertToNone");
      else    sprintf (r, "0x%x", rev);

      fprintf (stderr, "%s: %d: removing focus from %s / %s.\n",
               blurb(), screen_no, w, r);
    }

  XSetInputFocus (si->dpy, None, RevertToNone, CurrentTime);
  XSync (si->dpy, False);
}


static Bool
grab_keyboard_and_mouse (saver_info *si, Window window, Cursor cursor,
                         int screen_no)
{
  Status mstatus = 0, kstatus = 0;
  int i;
  int retries = 4;
  Bool focus_fuckus = False;

 AGAIN:

  for (i = 0; i < retries; i++)
    {
      XSync (si->dpy, False);
      kstatus = grab_kbd (si, window, screen_no);
      if (kstatus == GrabSuccess)
        break;

      /* else, wait a second and try to grab again. */
      sleep (1);
    }

  if (kstatus != GrabSuccess)
    {
      fprintf (stderr, "%s: couldn't grab keyboard!  (%s)\n",
               blurb(), grab_string(kstatus));

      if (! focus_fuckus)
        {
          focus_fuckus = True;
          nuke_focus (si, screen_no);
          goto AGAIN;
        }
    }

  for (i = 0; i < retries; i++)
    {
      XSync (si->dpy, False);
      mstatus = grab_mouse (si, window, cursor, screen_no);
      if (mstatus == GrabSuccess)
        break;

      /* else, wait a second and try to grab again. */
      sleep (1);
    }

  if (mstatus != GrabSuccess)
    fprintf (stderr, "%s: couldn't grab pointer!  (%s)\n",
             blurb(), grab_string(mstatus));


  /* When should we allow blanking to proceed?  The current theory
     is that a keyboard grab is manditory; a mouse grab is optional.

     - If we don't have a keyboard grab, then we won't be able to
       read a password to unlock, so the kbd grab is manditory.
       (We can't conditionalize this on locked_p, because someone
       might run "xscreensaver-command -lock" at any time.)

     - If we don't have a mouse grab, then we might not see mouse
       clicks as a signal to unblank -- but we will still see kbd
       activity, so that's not a disaster.
   */

  if (kstatus != GrabSuccess)	/* Do not blank without a kbd grab.   */
    return False;

  return True;			/* Grab is good, go ahead and blank.  */
}

static void
ungrab_keyboard_and_mouse (saver_info *si)
{
  ungrab_mouse (si);
  ungrab_kbd (si);
}


int
move_mouse_grab (saver_info *si, Window to, Cursor cursor, int to_screen_no)
{
  Window old = si->mouse_grab_window;

  if (old == 0)
    return grab_mouse (si, to, cursor, to_screen_no);
  else
    {
      saver_preferences *p = &si->prefs;
      int status;

      XSync (si->dpy, False);
      XGrabServer (si->dpy);			/* ############ DANGER! */
      XSync (si->dpy, False);

      if (p->verbose_p)
        fprintf(stderr, "%s: grabbing server...\n", blurb());

      ungrab_mouse (si);
      status = grab_mouse (si, to, cursor, to_screen_no);

      if (status != GrabSuccess)   /* Augh! */
        {
          sleep (1);               /* Note dramatic evil of sleeping
                                      with server grabbed. */
          XSync (si->dpy, False);
          status = grab_mouse (si, to, cursor, to_screen_no);
        }

      if (status != GrabSuccess)   /* Augh!  Try to get the old one back... */
        grab_mouse (si, old, cursor, to_screen_no);

      XUngrabServer (si->dpy);
      XSync (si->dpy, False);			/* ###### (danger over) */

      if (p->verbose_p)
        fprintf(stderr, "%s: ungrabbing server.\n", blurb());

      return status;
    }
}


/* Prints an error message to stderr and returns True if there is another
   xscreensaver running already.  Silently returns False otherwise. */
Bool
ensure_no_screensaver_running (Display *dpy, Screen *screen)
{
  Bool status = 0;
  int i;
  Window root = RootWindowOfScreen (screen);
  Window root2, parent, *kids;
  unsigned int nkids;
  XErrorHandler old_handler = XSetErrorHandler (BadWindow_ehandler);

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
      unsigned char *version;

      if (XGetWindowProperty (dpy, kids[i], XA_SCREENSAVER_VERSION, 0, 1,
			      False, XA_STRING, &type, &format, &nitems,
			      &bytesafter, &version)
	  == Success
	  && type != None)
	{
	  unsigned char *id;
	  if (!XGetWindowProperty (dpy, kids[i], XA_SCREENSAVER_ID, 0, 512,
				   False, XA_STRING, &type, &format, &nitems,
				   &bytesafter, &id)
	      == Success
	      || type == None)
	    id = (unsigned char *) "???";

	  fprintf (stderr,
      "%s: already running on display %s (window 0x%x)\n from process %s.\n",
		   blurb(), DisplayString (dpy), (int) kids [i],
                   (char *) id);
	  status = True;
	}
    }

  if (kids) XFree ((char *) kids);
  XSync (dpy, False);
  XSetErrorHandler (old_handler);
  return status;
}



/* Virtual-root hackery */

#ifdef _VROOT_H_
ERROR!  You must not include vroot.h in this file.
#endif

static void
store_vroot_property (Display *dpy, Window win, Window value)
{
#if 0
  if (p->verbose_p)
    fprintf (stderr,
	     "%s: storing XA_VROOT = 0x%x (%s) = 0x%x (%s)\n", blurb(), 
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
remove_vroot_property (Display *dpy, Window win)
{
#if 0
  if (p->verbose_p)
    fprintf (stderr, "%s: removing XA_VROOT from 0x%x (%s)\n", blurb(), win, 
	     (win == screensaver_window ? "ScreenSaver" :
	      (win == real_vroot ? "VRoot" :
	       (win == real_vroot_value ? "Vroot_value" : "???"))));
#endif
  XDeleteProperty (dpy, win, XA_VROOT);
}


static Bool safe_XKillClient (Display *dpy, XID id);

#ifdef HAVE_XF86VMODE
static Bool safe_XF86VidModeGetViewPort (Display *, int, int *, int *);
#endif /* HAVE_XF86VMODE */


static void
kill_xsetroot_data_1 (Display *dpy, Window window,
                      Atom prop, const char *atom_name,
                      Bool verbose_p)
{
  Atom type;
  int format;
  unsigned long nitems, bytesafter;
  unsigned char *dataP = 0;

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

     Update: it seems that Gnome and KDE do this same trick, but with the
     properties "ESETROOT_PMAP_ID" and/or "_XROOTPMAP_ID" instead of
     "_XSETROOT_ID".  So, we'll kill those too.
   */
  if (XGetWindowProperty (dpy, window, prop, 0, 1,
			  True, AnyPropertyType, &type, &format, &nitems, 
			  &bytesafter, &dataP)
      == Success
      && type != None)
    {
      Pixmap *pixP = (Pixmap *) dataP;
      if (pixP && *pixP && type == XA_PIXMAP && format == 32 &&
	  nitems == 1 && bytesafter == 0)
	{
	  if (verbose_p)
	    fprintf (stderr, "%s: destroying %s data (0x%lX).\n",
		     blurb(), atom_name, *pixP);
	  safe_XKillClient (dpy, *pixP);
	}
      else
	fprintf (stderr,
                 "%s: deleted unrecognised %s property: \n"
                 "\t%lu, %lu; type: %lu, format: %d, "
                 "nitems: %lu, bytesafter %ld\n",
		 blurb(), atom_name,
                 (unsigned long) pixP, (pixP ? *pixP : 0), type,
		 format, nitems, bytesafter);
    }
}


static void
kill_xsetroot_data (Display *dpy, Window w, Bool verbose_p)
{
  kill_xsetroot_data_1 (dpy, w, XA_XSETROOT_ID, "_XSETROOT_ID", verbose_p);
  kill_xsetroot_data_1 (dpy, w, XA_ESETROOT_PMAP_ID, "ESETROOT_PMAP_ID",
                        verbose_p);
  kill_xsetroot_data_1 (dpy, w, XA_XROOTPMAP_ID, "_XROOTPMAP_ID", verbose_p);
}


static void
save_real_vroot (saver_screen_info *ssi)
{
  saver_info *si = ssi->global;
  Display *dpy = si->dpy;
  Screen *screen = ssi->screen;
  int i;
  Window root = RootWindowOfScreen (screen);
  Window root2, parent, *kids;
  unsigned int nkids;
  XErrorHandler old_handler;

  /* It's possible that a window might be deleted between our call to
     XQueryTree() and our call to XGetWindowProperty().  Don't die if
     that happens (but just ignore that window, it's not the one we're
     interested in anyway.)
   */
  XSync (dpy, False);
  old_handler = XSetErrorHandler (BadWindow_ehandler);
  XSync (dpy, False);

  ssi->real_vroot = 0;
  ssi->real_vroot_value = 0;
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
      unsigned char *dataP = 0;
      Window *vrootP;
      int j;

      /* Skip this window if it is the xscreensaver window of any other
         screen (this can happen in the Xinerama case.)
       */
      for (j = 0; j < si->nscreens; j++)
        {
          saver_screen_info *ssi2 = &si->screens[j];
          if (kids[i] == ssi2->screensaver_window)
            goto SKIP;
        }

      if (XGetWindowProperty (dpy, kids[i], XA_VROOT, 0, 1, False, XA_WINDOW,
			      &type, &format, &nitems, &bytesafter,
			      &dataP)
	  != Success)
	continue;
      if (! dataP)
	continue;

      vrootP = (Window *) dataP;
      if (ssi->real_vroot)
	{
	  if (*vrootP == ssi->screensaver_window) abort ();
	  fprintf (stderr,
	    "%s: more than one virtual root window found (0x%x and 0x%x).\n",
		   blurb(), (int) ssi->real_vroot, (int) kids [i]);
	  exit (1);
	}
      ssi->real_vroot = kids [i];
      ssi->real_vroot_value = *vrootP;
    SKIP:
      ;
    }

  XSync (dpy, False);
  XSetErrorHandler (old_handler);
  XSync (dpy, False);

  if (ssi->real_vroot)
    {
      remove_vroot_property (si->dpy, ssi->real_vroot);
      XSync (dpy, False);
    }

  XFree ((char *) kids);
}


static Bool
restore_real_vroot_1 (saver_screen_info *ssi)
{
  saver_info *si = ssi->global;
  saver_preferences *p = &si->prefs;
  if (p->verbose_p && ssi->real_vroot)
    fprintf (stderr,
	     "%s: restoring __SWM_VROOT property on the real vroot (0x%lx).\n",
	     blurb(), (unsigned long) ssi->real_vroot);
  remove_vroot_property (si->dpy, ssi->screensaver_window);
  if (ssi->real_vroot)
    {
      store_vroot_property (si->dpy, ssi->real_vroot, ssi->real_vroot_value);
      ssi->real_vroot = 0;
      ssi->real_vroot_value = 0;
      /* make sure the property change gets there before this process
	 terminates!  We might be doing this because we have intercepted
	 SIGTERM or something. */
      XSync (si->dpy, False);
      return True;
    }
  return False;
}

Bool
restore_real_vroot (saver_info *si)
{
  int i;
  Bool did_any = False;
  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      if (restore_real_vroot_1 (ssi))
	did_any = True;
    }
  return did_any;
}


/* Signal hackery to ensure that the vroot doesn't get left in an 
   inconsistent state
 */

const char *
signal_name(int signal)
{
  switch (signal) {
  case SIGHUP:	  return "SIGHUP";
  case SIGINT:	  return "SIGINT";
  case SIGQUIT:	  return "SIGQUIT";
  case SIGILL:	  return "SIGILL";
  case SIGTRAP:	  return "SIGTRAP";
#ifdef SIGABRT
  case SIGABRT:	  return "SIGABRT";
#endif
  case SIGFPE:	  return "SIGFPE";
  case SIGKILL:	  return "SIGKILL";
  case SIGBUS:	  return "SIGBUS";
  case SIGSEGV:	  return "SIGSEGV";
  case SIGPIPE:	  return "SIGPIPE";
  case SIGALRM:	  return "SIGALRM";
  case SIGTERM:	  return "SIGTERM";
#ifdef SIGSTOP
  case SIGSTOP:	  return "SIGSTOP";
#endif
#ifdef SIGCONT
  case SIGCONT:	  return "SIGCONT";
#endif
#ifdef SIGUSR1
  case SIGUSR1:	  return "SIGUSR1";
#endif
#ifdef SIGUSR2
  case SIGUSR2:	  return "SIGUSR2";
#endif
#ifdef SIGEMT
  case SIGEMT:	  return "SIGEMT";
#endif
#ifdef SIGSYS
  case SIGSYS:	  return "SIGSYS";
#endif
#ifdef SIGCHLD
  case SIGCHLD:	  return "SIGCHLD";
#endif
#ifdef SIGPWR
  case SIGPWR:	  return "SIGPWR";
#endif
#ifdef SIGWINCH
  case SIGWINCH:  return "SIGWINCH";
#endif
#ifdef SIGURG
  case SIGURG:	  return "SIGURG";
#endif
#ifdef SIGIO
  case SIGIO:	  return "SIGIO";
#endif
#ifdef SIGVTALRM
  case SIGVTALRM: return "SIGVTALRM";
#endif
#ifdef SIGXCPU
  case SIGXCPU:	  return "SIGXCPU";
#endif
#ifdef SIGXFSZ
  case SIGXFSZ:	  return "SIGXFSZ";
#endif
#ifdef SIGDANGER
  case SIGDANGER: return "SIGDANGER";
#endif
  default:
    {
      static char buf[50];
      sprintf(buf, "signal %d\n", signal);
      return buf;
    }
  }
}



static RETSIGTYPE
restore_real_vroot_handler (int sig)
{
  saver_info *si = global_si_kludge;	/* I hate C so much... */

  signal (sig, SIG_DFL);
  if (restore_real_vroot (si))
    fprintf (real_stderr, "\n%s: %s intercepted, vroot restored.\n",
	     blurb(), signal_name(sig));
  kill (getpid (), sig);
}

static void
catch_signal (saver_info *si, int sig, RETSIGTYPE (*handler) (int))
{
# ifdef HAVE_SIGACTION

  struct sigaction a;
  a.sa_handler = handler;
  sigemptyset (&a.sa_mask);
  a.sa_flags = 0;

  /* On Linux 2.4.9 (at least) we need to tell the kernel to not mask delivery
     of this signal from inside its handler, or else when we execvp() the
     process again, it starts up with SIGHUP blocked, meaning that killing
     it with -HUP only works *once*.  You'd think that execvp() would reset
     all the signal masks, but it doesn't.
   */
#  if defined(SA_NOMASK)
  a.sa_flags |= SA_NOMASK;
#  elif defined(SA_NODEFER)
  a.sa_flags |= SA_NODEFER;
#  endif

  if (sigaction (sig, &a, 0) < 0)
# else  /* !HAVE_SIGACTION */
  if (((long) signal (sig, handler)) == -1L)
# endif /* !HAVE_SIGACTION */
    {
      char buf [255];
      sprintf (buf, "%s: couldn't catch %s", blurb(), signal_name(sig));
      perror (buf);
      saver_exit (si, 1, 0);
    }
}

static RETSIGTYPE saver_sighup_handler (int sig);

void
handle_signals (saver_info *si)
{
  catch_signal (si, SIGHUP, saver_sighup_handler);

  catch_signal (si, SIGINT,  restore_real_vroot_handler);
  catch_signal (si, SIGQUIT, restore_real_vroot_handler);
  catch_signal (si, SIGILL,  restore_real_vroot_handler);
  catch_signal (si, SIGTRAP, restore_real_vroot_handler);
#ifdef SIGIOT
  catch_signal (si, SIGIOT,  restore_real_vroot_handler);
#endif
  catch_signal (si, SIGABRT, restore_real_vroot_handler);
#ifdef SIGEMT
  catch_signal (si, SIGEMT,  restore_real_vroot_handler);
#endif
  catch_signal (si, SIGFPE,  restore_real_vroot_handler);
  catch_signal (si, SIGBUS,  restore_real_vroot_handler);
  catch_signal (si, SIGSEGV, restore_real_vroot_handler);
#ifdef SIGSYS
  catch_signal (si, SIGSYS,  restore_real_vroot_handler);
#endif
  catch_signal (si, SIGTERM, restore_real_vroot_handler);
#ifdef SIGXCPU
  catch_signal (si, SIGXCPU, restore_real_vroot_handler);
#endif
#ifdef SIGXFSZ
  catch_signal (si, SIGXFSZ, restore_real_vroot_handler);
#endif
#ifdef SIGDANGER
  catch_signal (si, SIGDANGER, restore_real_vroot_handler);
#endif
}


static RETSIGTYPE
saver_sighup_handler (int sig)
{
  saver_info *si = global_si_kludge;	/* I hate C so much... */

  /* Re-establish SIGHUP handler */
  catch_signal (si, SIGHUP, saver_sighup_handler);

  fprintf (stderr, "%s: %s received: restarting...\n",
           blurb(), signal_name(sig));

  if (si->screen_blanked_p)
    {
      unblank_screen (si);
      kill_screenhack (si);
      XSync (si->dpy, False);
    }

  restart_process (si);   /* Does not return */
  abort ();
}



void
saver_exit (saver_info *si, int status, const char *dump_core_reason)
{
  saver_preferences *p = &si->prefs;
  static Bool exiting = False;
  Bool bugp;
  Bool vrs;

  if (exiting)
    exit(status);

  exiting = True;
  
  vrs = restore_real_vroot (si);
  emergency_kill_subproc (si);
  shutdown_stderr (si);

  if (p->verbose_p && vrs)
    fprintf (real_stderr, "%s: old vroot restored.\n", blurb());

  fflush(real_stdout);

#ifdef VMS	/* on VMS, 1 is the "normal" exit code instead of 0. */
  if (status == 0) status = 1;
  else if (status == 1) status = -1;
#endif

  bugp = !!dump_core_reason;

  if (si->prefs.debug_p && !dump_core_reason)
    dump_core_reason = "because of -debug";

  if (dump_core_reason)
    {
      /* Note that the Linux man page for setuid() says If uid is
	 different from the old effective uid, the process will be
	 forbidden from leaving core dumps.
      */
      char cwd[4096]; /* should really be PATH_MAX, but who cares. */
      cwd[0] = 0;
      fprintf(real_stderr, "%s: dumping core (%s)\n", blurb(),
	      dump_core_reason);

      if (bugp)
	fprintf(real_stderr,
		"%s: see http://www.jwz.org/xscreensaver/bugs.html\n"
		"\t\t\tfor bug reporting information.\n\n",
		blurb());

# if defined(HAVE_GETCWD)
      if (!getcwd (cwd, sizeof(cwd)))
# elif defined(HAVE_GETWD)
      if (!getwd (cwd))
# endif
        strcpy(cwd, "unknown.");

      fprintf (real_stderr, "%s: current directory is %s\n", blurb(), cwd);
      describe_uids (si, real_stderr);

      /* Do this to drop a core file, so that we can get a stack trace. */
      abort();
    }

  exit (status);
}


/* Managing the actual screensaver window */

Bool
window_exists_p (Display *dpy, Window window)
{
  XErrorHandler old_handler;
  XWindowAttributes xgwa;
  xgwa.screen = 0;
  old_handler = XSetErrorHandler (BadWindow_ehandler);
  XGetWindowAttributes (dpy, window, &xgwa);
  XSync (dpy, False);
  XSetErrorHandler (old_handler);
  return (xgwa.screen != 0);
}

static void
store_saver_id (saver_screen_info *ssi)
{
  XClassHint class_hints;
  saver_info *si = ssi->global;
  unsigned long pid = (unsigned long) getpid ();
  char buf[20];
  struct passwd *p = getpwuid (getuid ());
  const char *name, *host;
  char *id;

  /* First store the name and class on the window.
   */
  class_hints.res_name = progname;
  class_hints.res_class = progclass;
  XSetClassHint (si->dpy, ssi->screensaver_window, &class_hints);
  XStoreName (si->dpy, ssi->screensaver_window, "screensaver");

  /* Then store the xscreensaver version number.
   */
  XChangeProperty (si->dpy, ssi->screensaver_window,
		   XA_SCREENSAVER_VERSION,
		   XA_STRING, 8, PropModeReplace,
		   (unsigned char *) si->version,
		   strlen (si->version));

  /* Now store the XSCREENSAVER_ID property, that says what user and host
     xscreensaver is running as.
   */

  if (p && p->pw_name && *p->pw_name)
    name = p->pw_name;
  else if (p)
    {
      sprintf (buf, "%lu", (unsigned long) p->pw_uid);
      name = buf;
    }
  else
    name = "???";

# if defined(HAVE_UNAME)
  {
    struct utsname uts;
    if (uname (&uts) < 0)
      host = "???";
    else
      host = uts.nodename;
  }
# elif defined(VMS)
  host = getenv("SYS$NODE");
# else  /* !HAVE_UNAME && !VMS */
  host = "???";
# endif /* !HAVE_UNAME && !VMS */

  id = (char *) malloc (strlen(name) + strlen(host) + 50);
  sprintf (id, "%lu (%s@%s)", pid, name, host);

  XChangeProperty (si->dpy, ssi->screensaver_window,
		   XA_SCREENSAVER_ID, XA_STRING,
		   8, PropModeReplace,
		   (unsigned char *) id, strlen (id));
  free (id);
}


void
store_saver_status (saver_info *si)
{
  PROP32 *status;
  int size = si->nscreens + 2;
  int i;

  status = (PROP32 *) calloc (size, sizeof(PROP32));

  status[0] = (PROP32) (si->screen_blanked_p
                        ? (si->locked_p ? XA_LOCK : XA_BLANK)
                        : 0);
  status[1] = (PROP32) si->blank_time;

  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      status [2 + i] = ssi->current_hack + 1;
    }

  XChangeProperty (si->dpy,
                   RootWindow (si->dpy, 0),  /* always screen #0 */
                   XA_SCREENSAVER_STATUS,
                   XA_INTEGER, 32, PropModeReplace,
                   (unsigned char *) status, size);
  free (status);
}



/* Returns the area of the screen which the xscreensaver window should cover.
   Normally this is the whole screen, but if the X server's root window is
   actually larger than the monitor's displayable area, then we want to
   operate in the currently-visible portion of the desktop instead.
 */
void
get_screen_viewport (saver_screen_info *ssi,
                     int *x_ret, int *y_ret,
                     int *w_ret, int *h_ret,
                     int target_x, int target_y,
                     Bool verbose_p)
{
  int w = WidthOfScreen (ssi->screen);
  int h = HeightOfScreen (ssi->screen);

# ifdef HAVE_XF86VMODE
  saver_info *si = ssi->global;
  saver_preferences *p = &si->prefs;
  int event, error;
  int dot;
  XF86VidModeModeLine ml;
  int x, y;
  Bool xinerama_p = si->xinerama_p;

#  ifndef HAVE_XINERAMA
  /* Even if we don't have the client-side Xinerama lib, check to see if
     the server supports Xinerama, so that we know to ignore the VidMode
     extension -- otherwise a server crash could result.  Yay. */
  xinerama_p = XQueryExtension (si->dpy, "XINERAMA", &error, &event, &error);
#  endif /* !HAVE_XINERAMA */

#  ifdef HAVE_XINERAMA
  if (xinerama_p)
    {
      int mouse_p = (target_x != -1 && target_y != -1);
      int which = -1;
      int i;

      /* If a mouse position wasn't passed in, assume we're talking about
         this screen. */
      if (!mouse_p)
        {
          target_x = ssi->x;
          target_y = ssi->y;
        }

      /* Find the Xinerama rectangle that contains the mouse position. */
      for (i = 0; i < si->nscreens; i++)
        {
          if (target_x >= si->screens[i].x &&
              target_y >= si->screens[i].y &&
              target_x <  si->screens[i].x + si->screens[i].width &&
              target_y <  si->screens[i].y + si->screens[i].height)
            which = i;
        }
      if (which == -1) which = 0;  /* didn't find it?  Use the first. */
      *x_ret = si->screens[which].x;
      *y_ret = si->screens[which].y;
      *w_ret = si->screens[which].width;
      *h_ret = si->screens[which].height;

      if (verbose_p)
        {
          fprintf (stderr, "%s: %d: xinerama vp: %dx%d+%d+%d",
                   blurb(), which,
                   si->screens[which].width, si->screens[which].height,
                   si->screens[which].x, si->screens[which].y);
          if (mouse_p)
            fprintf (stderr, "; mouse at %d,%d", target_x, target_y);
          fprintf (stderr, ".\n");
        }

      return;
    }
#  endif /* HAVE_XINERAMA */

  if (!xinerama_p &&  /* Xinerama + VidMode = broken. */
      XF86VidModeQueryExtension (si->dpy, &event, &error) &&
      safe_XF86VidModeGetViewPort (si->dpy, ssi->number, &x, &y) &&
      XF86VidModeGetModeLine (si->dpy, ssi->number, &dot, &ml))
    {
      char msg[512];
      *x_ret = x;
      *y_ret = y;
      *w_ret = ml.hdisplay;
      *h_ret = ml.vdisplay;

      if (*x_ret == 0 && *y_ret == 0 && *w_ret == w && *h_ret == h)
        /* There is no viewport -- the screen does not scroll. */
        return;


      /* Apparently some versions of XFree86 return nonsense here!
         I've had reports of 1024x768 viewports at -1936862040, -1953705044.
         So, sanity-check the values and give up if they are out of range.
       */
      if (*x_ret <  0 || *x_ret >= w ||
          *y_ret <  0 || *y_ret >= h ||
          *w_ret <= 0 || *w_ret >  w ||
          *h_ret <= 0 || *h_ret >  h)
        {
          static int warned_once = 0;
          if (!warned_once)
            {
              fprintf (stderr, "\n"
                  "%s: X SERVER BUG: %dx%d viewport at %d,%d is impossible.\n"
                  "%s: The XVidMode server extension is returning nonsense.\n"
                  "%s: Please report this bug to your X server vendor.\n\n",
                       blurb(), *w_ret, *h_ret, *x_ret, *y_ret,
                       blurb(), blurb());
              warned_once = 1;
            }
          *x_ret = 0;
          *y_ret = 0;
          *w_ret = w;
          *h_ret = h;
          return;
        }

      sprintf (msg, "%s: %d: vp is %dx%d+%d+%d",
               blurb(), ssi->number,
               *w_ret, *h_ret, *x_ret, *y_ret);


      if (p->getviewport_full_of_lies_p)
        {
          /* XF86VidModeGetViewPort() tends to be full of lies on laptops
             that have a docking station or external monitor that runs in
             a different resolution than the laptop's screen:

                 http://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=81593
                 http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=208417
                 http://bugs.xfree86.org/show_bug.cgi?id=421

             The XFree86 developers have closed the bug. As far as I can
             tell, their reason for this was, "this is an X server bug,
             but it's pretty hard to fix. Therefore, we are closing it."

             So, now there's a preference item for those unfortunate users to
             tell us not to trust a word that XF86VidModeGetViewPort() says.
           */
          static int warned_once = 0;
          if (!warned_once && verbose_p)
            {
              warned_once = 1;
              fprintf (stderr,
                  "%s: %d: XF86VidModeGetViewPort() says vp is %dx%d+%d+%d;\n"
                  "%s: %d:     assuming that is a pack of lies;\n"
                  "%s: %d:     using %dx%d+0+0 instead.\n",
                       blurb(), ssi->number,
                       *w_ret, *h_ret, *x_ret, *y_ret,
                       blurb(), ssi->number,
                       blurb(), ssi->number, w, h);
            }

          *x_ret = 0;
          *y_ret = 0;
          *w_ret = w;
          *h_ret = h;
          return;
        }


      /* Apparently, though the server stores the X position in increments of
         1 pixel, it will only make changes to the *display* in some other
         increment.  With XF86_SVGA on a Thinkpad, the display only updates
         in multiples of 8 pixels when in 8-bit mode, and in multiples of 4
         pixels in 16-bit mode.  I don't know what it does in 24- and 32-bit
         mode, because I don't have enough video memory to find out.

         I consider it a bug that XF86VidModeGetViewPort() is telling me the
         server's *target* scroll position rather than the server's *actual*
         scroll position.  David Dawes agrees, and says they may fix this in
         XFree86 4.0, but it's notrivial.

         He also confirms that this behavior is server-dependent, so the
         actual scroll position cannot be reliably determined by the client.
         So... that means the only solution is to provide a ``sandbox''
         around the blackout window -- we make the window be up to N pixels
         larger than the viewport on both the left and right sides.  That
         means some part of the outer edges of each hack might not be
         visible, but screw it.

         I'm going to guess that 16 pixels is enough, and that the Y dimension
         doesn't have this problem.

         The drawback of doing this, of course, is that some of the screenhacks
         will still look pretty stupid -- for example, "slidescreen" will cut
         off the left and right edges of the grid, etc.
      */
#  define FUDGE 16
      if (x > 0 && x < w - ml.hdisplay)  /* not at left edge or right edge */
        {
          /* Round X position down to next lower multiple of FUDGE.
             Increase width by 2*FUDGE in case some server rounds up.
           */
          *x_ret = ((x - 1) / FUDGE) * FUDGE;
          *w_ret += (FUDGE * 2);
        }
#  undef FUDGE

      if (*x_ret != x ||
          *y_ret != y ||
          *w_ret != ml.hdisplay ||
          *h_ret != ml.vdisplay)
        sprintf (msg + strlen(msg), "; fudged to %dx%d+%d+%d",
                 *w_ret, *h_ret, *x_ret, *y_ret);

      if (verbose_p)
        fprintf (stderr, "%s.\n", msg);

      return;
    }

# endif /* HAVE_XF86VMODE */

  *x_ret = 0;
  *y_ret = 0;
  *w_ret = w;
  *h_ret = h;
}


static Bool error_handler_hit_p = False;

static int
ignore_all_errors_ehandler (Display *dpy, XErrorEvent *error)
{
  error_handler_hit_p = True;
  return 0;
}


/* Returns True if successful, False if an X error occurred.
   We need this because other programs might have done things to
   our window that will cause XChangeWindowAttributes() to fail:
   if that happens, we give up, destroy the window, and re-create
   it.
 */
static Bool
safe_XChangeWindowAttributes (Display *dpy, Window window,
                              unsigned long mask,
                              XSetWindowAttributes *attrs)
{
  XErrorHandler old_handler;
  XSync (dpy, False);
  error_handler_hit_p = False;
  old_handler = XSetErrorHandler (ignore_all_errors_ehandler);

  XChangeWindowAttributes (dpy, window, mask, attrs);

  XSync (dpy, False);
  XSetErrorHandler (old_handler);
  XSync (dpy, False);

  return (!error_handler_hit_p);
}


/* This might not be necessary, but just in case. */
static Bool
safe_XConfigureWindow (Display *dpy, Window window,
                       unsigned long mask, XWindowChanges *changes)
{
  XErrorHandler old_handler;
  XSync (dpy, False);
  error_handler_hit_p = False;
  old_handler = XSetErrorHandler (ignore_all_errors_ehandler);

  XConfigureWindow (dpy, window, mask, changes);

  XSync (dpy, False);
  XSetErrorHandler (old_handler);
  XSync (dpy, False);

  return (!error_handler_hit_p);
}

/* This might not be necessary, but just in case. */
static Bool
safe_XDestroyWindow (Display *dpy, Window window)
{
  XErrorHandler old_handler;
  XSync (dpy, False);
  error_handler_hit_p = False;
  old_handler = XSetErrorHandler (ignore_all_errors_ehandler);

  XDestroyWindow (dpy, window);

  XSync (dpy, False);
  XSetErrorHandler (old_handler);
  XSync (dpy, False);

  return (!error_handler_hit_p);
}


static Bool
safe_XKillClient (Display *dpy, XID id)
{
  XErrorHandler old_handler;
  XSync (dpy, False);
  error_handler_hit_p = False;
  old_handler = XSetErrorHandler (ignore_all_errors_ehandler);

  XKillClient (dpy, id);

  XSync (dpy, False);
  XSetErrorHandler (old_handler);
  XSync (dpy, False);

  return (!error_handler_hit_p);
}


#ifdef HAVE_XF86VMODE
static Bool
safe_XF86VidModeGetViewPort (Display *dpy, int screen, int *xP, int *yP)
{
  Bool result;
  XErrorHandler old_handler;
  XSync (dpy, False);
  error_handler_hit_p = False;
  old_handler = XSetErrorHandler (ignore_all_errors_ehandler);

  result = XF86VidModeGetViewPort (dpy, screen, xP, yP);

  XSync (dpy, False);
  XSetErrorHandler (old_handler);
  XSync (dpy, False);

  return (error_handler_hit_p
          ? False
          : result);
}

/* There is no "safe_XF86VidModeGetModeLine" because it fails with an
   untrappable I/O error instead of an X error -- so one must call
   safe_XF86VidModeGetViewPort first, and assume that both have the
   same error condition.  Thank you XFree, may I have another.
 */

#endif /* HAVE_XF86VMODE */


static void
initialize_screensaver_window_1 (saver_screen_info *ssi)
{
  saver_info *si = ssi->global;
  saver_preferences *p = &si->prefs;
  Bool install_cmap_p = ssi->install_cmap_p;   /* not p->install_cmap_p */

  /* This resets the screensaver window as fully as possible, since there's
     no way of knowing what some random client may have done to us in the
     meantime.  We could just destroy and recreate the window, but that has
     its own set of problems...
   */
  XColor black;
  XSetWindowAttributes attrs;
  unsigned long attrmask;
  int x, y, width, height;
  static Bool printed_visual_info = False;  /* only print the message once. */
  Window horked_window = 0;

  get_screen_viewport (ssi, &x, &y, &width, &height, -1, -1,
                       (p->verbose_p && !si->screen_blanked_p));

  black.red = black.green = black.blue = 0;

  if (ssi->cmap == DefaultColormapOfScreen (ssi->screen))
    ssi->cmap = 0;

  if (ssi->current_visual != DefaultVisualOfScreen (ssi->screen))
    /* It's not the default visual, so we have no choice but to install. */
    install_cmap_p = True;

  if (install_cmap_p)
    {
      if (! ssi->cmap)
	{
	  ssi->cmap = XCreateColormap (si->dpy,
				       RootWindowOfScreen (ssi->screen),
				      ssi->current_visual, AllocNone);
	  if (! XAllocColor (si->dpy, ssi->cmap, &black)) abort ();
	  ssi->black_pixel = black.pixel;
	}
    }
  else
    {
      Colormap def_cmap = DefaultColormapOfScreen (ssi->screen);
      if (ssi->cmap)
	{
	  XFreeColors (si->dpy, ssi->cmap, &ssi->black_pixel, 1, 0);
	  if (ssi->cmap != ssi->demo_cmap &&
	      ssi->cmap != def_cmap)
	    XFreeColormap (si->dpy, ssi->cmap);
	}
      ssi->cmap = def_cmap;
      ssi->black_pixel = BlackPixelOfScreen (ssi->screen);
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
  attrs.colormap = ssi->cmap;
  attrs.background_pixel = ssi->black_pixel;
  attrs.backing_pixel = ssi->black_pixel;
  attrs.border_pixel = ssi->black_pixel;

  if (p->debug_p
# ifdef QUAD_MODE
      && !p->quad_p
# endif
      )
    width = width / 2;

  if (!p->verbose_p || printed_visual_info)
    ;
  else if (ssi->current_visual == DefaultVisualOfScreen (ssi->screen))
    {
      fprintf (stderr, "%s: %d: visual ", blurb(), ssi->number);
      describe_visual (stderr, ssi->screen, ssi->current_visual,
		       install_cmap_p);
    }
  else
    {
      fprintf (stderr, "%s: using visual:   ", blurb());
      describe_visual (stderr, ssi->screen, ssi->current_visual,
		       install_cmap_p);
      fprintf (stderr, "%s: default visual: ", blurb());
      describe_visual (stderr, ssi->screen,
		       DefaultVisualOfScreen (ssi->screen),
		       ssi->install_cmap_p);
    }
  printed_visual_info = True;

#ifdef HAVE_MIT_SAVER_EXTENSION
  if (si->using_mit_saver_extension)
    {
      XScreenSaverInfo *info;
      Window root = RootWindowOfScreen (ssi->screen);

#if 0
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

      /* #### Ok, that doesn't work - when we tell the server that the
	 screensaver is "off" it sends us a Deactivate event, which is
	 sensible... but causes the saver to never come on.  Hmm. */
      disable_builtin_screensaver (si, True);
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
      XScreenSaverSetAttributes (si->dpy, root,
				 0, 0, width, height, 0,
				 current_depth, InputOutput, visual,
				 attrmask, &attrs);
      XSync (si->dpy, False);
#endif /* 0 */

      info = XScreenSaverAllocInfo ();
      XScreenSaverQueryInfo (si->dpy, root, info);
      ssi->server_mit_saver_window = info->window;
      if (! ssi->server_mit_saver_window) abort ();
      XFree (info);
    }
#endif /* HAVE_MIT_SAVER_EXTENSION */

  if (ssi->screensaver_window)
    {
      XWindowChanges changes;
      unsigned int changesmask = CWX|CWY|CWWidth|CWHeight|CWBorderWidth;
      changes.x = x;
      changes.y = y;
      changes.width = width;
      changes.height = height;
      changes.border_width = 0;

      if (! (safe_XConfigureWindow (si->dpy, ssi->screensaver_window,
                                  changesmask, &changes) &&
             safe_XChangeWindowAttributes (si->dpy, ssi->screensaver_window,
                                           attrmask, &attrs)))
        {
          horked_window = ssi->screensaver_window;
          ssi->screensaver_window = 0;
        }
    }

  if (!ssi->screensaver_window)
    {
      ssi->screensaver_window =
	XCreateWindow (si->dpy, RootWindowOfScreen (ssi->screen),
                       x, y, width, height,
                       0, ssi->current_depth, InputOutput,
		       ssi->current_visual, attrmask, &attrs);

      reset_stderr (ssi);

      if (horked_window)
        {
          fprintf (stderr,
            "%s: someone horked our saver window (0x%lx)!  Recreating it...\n",
                   blurb(), (unsigned long) horked_window);
          maybe_transfer_grabs (ssi, horked_window, ssi->screensaver_window,
                                ssi->number);
          safe_XDestroyWindow (si->dpy, horked_window);
          horked_window = 0;
        }

      if (p->verbose_p)
	fprintf (stderr, "%s: %d: saver window is 0x%lx.\n",
                 blurb(), ssi->number,
                 (unsigned long) ssi->screensaver_window);
    }

  store_saver_id (ssi);       /* store window name and IDs */

  if (!ssi->cursor)
    {
      Pixmap bit;
      bit = XCreatePixmapFromBitmapData (si->dpy, ssi->screensaver_window,
					 "\000", 1, 1,
					 BlackPixelOfScreen (ssi->screen),
					 BlackPixelOfScreen (ssi->screen),
					 1);
      ssi->cursor = XCreatePixmapCursor (si->dpy, bit, bit, &black, &black,
					 0, 0);
      XFreePixmap (si->dpy, bit);
    }

  XSetWindowBackground (si->dpy, ssi->screensaver_window, ssi->black_pixel);

  if (si->demoing_p)
    XUndefineCursor (si->dpy, ssi->screensaver_window);
  else
    XDefineCursor (si->dpy, ssi->screensaver_window, ssi->cursor);
}

void
initialize_screensaver_window (saver_info *si)
{
  int i;
  for (i = 0; i < si->nscreens; i++)
    initialize_screensaver_window_1 (&si->screens[i]);
}


/* Called when the RANDR (Resize and Rotate) extension tells us that the
   size of the screen has changed while the screen was blanked.  If we
   don't do this, then the screen saver will no longer fully fill the
   screen, and some of the underlying desktop may be visible.
 */
void
resize_screensaver_window (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  int i;

  /* First update the size info in the saver_screen_info structs.
   */

# ifdef HAVE_XINERAMA
  if (si->xinerama_p)
    {
      /* As of XFree86 4.3.0, the RANDR and XINERAMA extensions cannot coexist.
         However, maybe they will someday, so I'm guessing that the right thing
         to do in that case will be to re-query the Xinerama rectangles after
         a RANDR size change is received: presumably, if the resolution of one
         or more of the monitors has changed, then the Xinerama rectangle
         corresponding to that monitor will also have been updated.
       */
      int nscreens;
      XineramaScreenInfo *xsi = XineramaQueryScreens (si->dpy, &nscreens);
      if (nscreens != si->nscreens) abort();
      if (!xsi) abort();
      for (i = 0; i < si->nscreens; i++)
        {
          saver_screen_info *ssi = &si->screens[i];
          if (p->verbose_p &&
              (ssi->x      != xsi[i].x_org ||
               ssi->y      != xsi[i].y_org ||
               ssi->width  != xsi[i].width ||
               ssi->height != xsi[i].height))
            fprintf (stderr,
                   "%s: %d: resize xinerama from %dx%d+%d+%d to %dx%d+%d+%d\n",
                     blurb(), i,
                     ssi->width,   ssi->height,   ssi->x,       ssi->y,
                     xsi[i].width, xsi[i].height, xsi[i].x_org, xsi[i].y_org);

          ssi->x      = xsi[i].x_org;
          ssi->y      = xsi[i].y_org;
          ssi->width  = xsi[i].width;
          ssi->height = xsi[i].height;
        }
      XFree (xsi);
    }
  else
# endif /* HAVE_XINERAMA */
    {
      /* Not Xinerama -- get the real sizes of the root windows. */
      for (i = 0; i < si->nscreens; i++)
        {
          saver_screen_info *ssi = &si->screens[i];
          XWindowAttributes xgwa;
          XGetWindowAttributes (si->dpy, RootWindowOfScreen (ssi->screen),
                                &xgwa);

          if (p->verbose_p &&
              (ssi->x      != xgwa.x ||
               ssi->y      != xgwa.y ||
               ssi->width  != xgwa.width ||
               ssi->height != xgwa.height))
            fprintf (stderr,
                     "%s: %d: resize screen from %dx%d+%d+%d to %dx%d+%d+%d\n",
                     blurb(), i,
                     ssi->width, ssi->height, ssi->x, ssi->y,
                     xgwa.width, xgwa.height, xgwa.x, xgwa.y);

          ssi->x      = xgwa.x;
          ssi->y      = xgwa.y;
          ssi->width  = xgwa.width;
          ssi->height = xgwa.height;
        }
    }

  /* Next, ensure that the screensaver windows are the right size, taking
     into account both the new size of the screen in question's root window,
     and any viewport within that.
   */

  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      XWindowAttributes xgwa;
      XWindowChanges changes;
      int x, y, width, height;
      unsigned int changesmask = CWX|CWY|CWWidth|CWHeight|CWBorderWidth;

      XGetWindowAttributes (si->dpy, ssi->screensaver_window, &xgwa);
      get_screen_viewport (ssi, &x, &y, &width, &height, -1, -1,
                           (p->verbose_p && !si->screen_blanked_p));
      if (xgwa.x == x &&
          xgwa.y == y &&
          xgwa.width  == width &&
          xgwa.height == height)
        continue;  /* no change! */

      changes.x = x;
      changes.y = y;
      changes.width  = width;
      changes.height = height;
      changes.border_width = 0;

      if (p->debug_p
# ifdef QUAD_MODE
          && !p->quad_p
# endif
          ) 
        changes.width = changes.width / 2;

      if (p->verbose_p)
        fprintf (stderr,
                 "%s: %d: resize 0x%lx from %dx%d+%d+%d to %dx%d+%d+%d\n",
                 blurb(), i, (unsigned long) ssi->screensaver_window,
                 xgwa.width, xgwa.height, xgwa.x, xgwa.y,
                 width, height, x, y);
      if (! safe_XConfigureWindow (si->dpy, ssi->screensaver_window,
                                   changesmask, &changes))
        {
          fprintf (stderr,
    "%s: %d: someone horked our saver window (0x%lx)!  Unable to resize it!\n",
                   blurb(), i, (unsigned long) ssi->screensaver_window);
        }
    }
}


void 
raise_window (saver_info *si,
	      Bool inhibit_fade, Bool between_hacks_p, Bool dont_clear)
{
  saver_preferences *p = &si->prefs;
  int i;

  if (si->demoing_p)
    inhibit_fade = True;

  if (si->emergency_lock_p)
    inhibit_fade = True;

  if (!dont_clear)
    initialize_screensaver_window (si);

  reset_watchdog_timer (si, True);

  if (p->fade_p && si->fading_possible_p && !inhibit_fade)
    {
      Window *current_windows = (Window *)
	calloc(sizeof(Window), si->nscreens);
      Colormap *current_maps = (Colormap *)
	calloc(sizeof(Colormap), si->nscreens);

      for (i = 0; i < si->nscreens; i++)
	{
	  saver_screen_info *ssi = &si->screens[i];
	  current_windows[i] = ssi->screensaver_window;
	  current_maps[i] = (between_hacks_p
			     ? ssi->cmap
			     : DefaultColormapOfScreen (ssi->screen));
	  /* Ensure that the default background of the window is really black,
	     not a pixmap or something.  (This does not clear the window.) */
	  XSetWindowBackground (si->dpy, ssi->screensaver_window,
				ssi->black_pixel);
	}

      if (p->verbose_p) fprintf (stderr, "%s: fading...\n", blurb());

      XGrabServer (si->dpy);			/* ############ DANGER! */

      /* Clear the stderr layer on each screen.
       */
      if (!dont_clear)
	for (i = 0; i < si->nscreens; i++)
	  {
	    saver_screen_info *ssi = &si->screens[i];
	    if (ssi->stderr_overlay_window)
	      /* Do this before the fade, since the stderr cmap won't fade
		 even if we uninstall it (beats me...) */
	      clear_stderr (ssi);
	  }

      /* Note!  The server is grabbed, and this will take several seconds
	 to complete! */
      fade_screens (si->dpy, current_maps,
                    current_windows, si->nscreens,
		    p->fade_seconds/1000, p->fade_ticks, True, !dont_clear);

      free(current_maps);
      free(current_windows);
      current_maps = 0;
      current_windows = 0;

      if (p->verbose_p) fprintf (stderr, "%s: fading done.\n", blurb());

#ifdef HAVE_MIT_SAVER_EXTENSION
      for (i = 0; i < si->nscreens; i++)
	{
	  saver_screen_info *ssi = &si->screens[i];
	  if (ssi->server_mit_saver_window &&
	      window_exists_p (si->dpy, ssi->server_mit_saver_window))
	    XUnmapWindow (si->dpy, ssi->server_mit_saver_window);
	}
#endif /* HAVE_MIT_SAVER_EXTENSION */

      XUngrabServer (si->dpy);
      XSync (si->dpy, False);			/* ###### (danger over) */
    }
  else
    {
      for (i = 0; i < si->nscreens; i++)
	{
	  saver_screen_info *ssi = &si->screens[i];
	  if (!dont_clear)
	    XClearWindow (si->dpy, ssi->screensaver_window);
	  if (!dont_clear || ssi->stderr_overlay_window)
	    clear_stderr (ssi);
	  XMapRaised (si->dpy, ssi->screensaver_window);
#ifdef HAVE_MIT_SAVER_EXTENSION
	  if (ssi->server_mit_saver_window &&
	      window_exists_p (si->dpy, ssi->server_mit_saver_window))
	    XUnmapWindow (si->dpy, ssi->server_mit_saver_window);
#endif /* HAVE_MIT_SAVER_EXTENSION */
	}
    }

  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      if (ssi->cmap)
	XInstallColormap (si->dpy, ssi->cmap);
    }
}


int
mouse_screen (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  Window pointer_root, pointer_child;
  int root_x, root_y, win_x, win_y;
  unsigned int mask;
  int i;

  if (si->nscreens == 1)
    return 0;

  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      if (XQueryPointer (si->dpy, RootWindowOfScreen (ssi->screen),
                         &pointer_root, &pointer_child,
                         &root_x, &root_y, &win_x, &win_y, &mask) &&
          root_x >= ssi->x &&
          root_y >= ssi->y &&
          root_x <  ssi->x + ssi->width &&
          root_y <  ssi->y + ssi->height)
        {
          if (p->verbose_p)
            fprintf (stderr, "%s: mouse is on screen %d of %d\n",
                     blurb(), i, si->nscreens);
          return i;
        }
    }

  /* couldn't figure out where the mouse is?  Oh well. */
  return 0;
}


Bool
blank_screen (saver_info *si)
{
  int i;
  Bool ok;
  Window w;
  int mscreen;

  /* Note: we do our grabs on the root window, not on the screensaver window.
     If we grabbed on the saver window, then the demo mode and lock dialog
     boxes wouldn't get any events.

     By "the root window", we mean "the root window that contains the mouse."
     We use to always grab the mouse on screen 0, but that has the effect of
     moving the mouse to screen 0 from whichever screen it was on, on
     multi-head systems.
   */
  mscreen = mouse_screen (si);
  w = RootWindowOfScreen(si->screens[mscreen].screen);
  ok = grab_keyboard_and_mouse (si, w,
                                (si->demoing_p ? 0 : si->screens[0].cursor),
                                mscreen);


# if 0
  if (si->using_mit_saver_extension || si->using_sgi_saver_extension)
    /* If we're using a server extension, then failure to get a grab is
       not a big deal -- even without the grab, we will still be able
       to un-blank when there is user activity, since the server will
       tell us. */
    /* #### No, that's not true: if we don't have a keyboard grab,
            then we can't read passwords to unlock.
     */
    ok = True;
# endif /* 0 */

  if (!ok)
    return False;

  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      if (ssi->real_screen_p)
        save_real_vroot (ssi);
      store_vroot_property (si->dpy,
			    ssi->screensaver_window,
			    ssi->screensaver_window);

#ifdef HAVE_XF86VMODE
      {
        int ev, er;
        if (!XF86VidModeQueryExtension (si->dpy, &ev, &er) ||
            !safe_XF86VidModeGetViewPort (si->dpy, i,
                                          &ssi->blank_vp_x,
                                          &ssi->blank_vp_y))
          ssi->blank_vp_x = ssi->blank_vp_y = -1;
      }
#endif /* HAVE_XF86VMODE */
    }

  raise_window (si, False, False, False);

  si->screen_blanked_p = True;
  si->blank_time = time ((time_t) 0);
  si->last_wall_clock_time = 0;

  store_saver_status (si);  /* store blank time */

  return True;
}


void
unblank_screen (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  Bool unfade_p = (si->fading_possible_p && p->unfade_p);
  int i;

  monitor_power_on (si);
  reset_watchdog_timer (si, False);

  if (si->demoing_p)
    unfade_p = False;

  if (unfade_p)
    {
      Window *current_windows = (Window *)
	calloc(sizeof(Window), si->nscreens);

      for (i = 0; i < si->nscreens; i++)
	{
	  saver_screen_info *ssi = &si->screens[i];
	  current_windows[i] = ssi->screensaver_window;
	  /* Ensure that the default background of the window is really black,
	     not a pixmap or something.  (This does not clear the window.) */
	  XSetWindowBackground (si->dpy, ssi->screensaver_window,
				ssi->black_pixel);
	}

      if (p->verbose_p) fprintf (stderr, "%s: unfading...\n", blurb());


      XSync (si->dpy, False);
      XGrabServer (si->dpy);			/* ############ DANGER! */
      XSync (si->dpy, False);

      /* Clear the stderr layer on each screen.
       */
      for (i = 0; i < si->nscreens; i++)
	{
	  saver_screen_info *ssi = &si->screens[i];
	  clear_stderr (ssi);
	}

      XUngrabServer (si->dpy);
      XSync (si->dpy, False);			/* ###### (danger over) */

      fade_screens (si->dpy, 0,
                    current_windows, si->nscreens,
		    p->fade_seconds/1000, p->fade_ticks,
		    False, False);

      free(current_windows);
      current_windows = 0;

      if (p->verbose_p) fprintf (stderr, "%s: unfading done.\n", blurb());
    }
  else
    {
      for (i = 0; i < si->nscreens; i++)
	{
	  saver_screen_info *ssi = &si->screens[i];
	  if (ssi->cmap)
	    {
	      Colormap c = DefaultColormapOfScreen (ssi->screen);
	      /* avoid technicolor */
	      XClearWindow (si->dpy, ssi->screensaver_window);
	      if (c) XInstallColormap (si->dpy, c);
	    }
	  XUnmapWindow (si->dpy, ssi->screensaver_window);
	}
    }


  /* If the focus window does has a non-default colormap, then install
     that colormap as well.  (On SGIs, this will cause both the root map
     and the focus map to be installed simultaneously.  It'd be nice to
     pick up the other colormaps that had been installed, too; perhaps
     XListInstalledColormaps could be used for that?)
   */
  {
    Window focus = 0;
    int revert_to;
    XGetInputFocus (si->dpy, &focus, &revert_to);
    if (focus && focus != PointerRoot && focus != None)
      {
	XWindowAttributes xgwa;
	xgwa.colormap = 0;
	XGetWindowAttributes (si->dpy, focus, &xgwa);
	if (xgwa.colormap &&
	    xgwa.colormap != DefaultColormapOfScreen (xgwa.screen))
	  XInstallColormap (si->dpy, xgwa.colormap);
      }
  }


  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      kill_xsetroot_data (si->dpy, ssi->screensaver_window, p->verbose_p);
    }

  store_saver_status (si);  /* store unblank time */
  ungrab_keyboard_and_mouse (si);
  restore_real_vroot (si);

  /* Unmap the windows a second time, dammit -- just to avoid a race
     with the screen-grabbing hacks.  (I'm not sure if this is really
     necessary; I'm stabbing in the dark now.)
  */
  for (i = 0; i < si->nscreens; i++)
    XUnmapWindow (si->dpy, si->screens[i].screensaver_window);

  si->screen_blanked_p = False;
  si->blank_time = time ((time_t) 0);
  si->last_wall_clock_time = 0;

  store_saver_status (si);  /* store unblank time */
}


/* Transfer any grabs from the old window to the new.
   Actually I think none of this is necessary, since we always
   hold our grabs on the root window, but I wrote this before
   re-discovering that...
 */
static void
maybe_transfer_grabs (saver_screen_info *ssi,
                      Window old_w, Window new_w,
                      int new_screen_no)
{
  saver_info *si = ssi->global;

  /* If the old window held our mouse grab, transfer the grab to the new
     window.  (Grab the server while so doing, to avoid a race condition.)
   */
  if (old_w == si->mouse_grab_window)
    {
      XGrabServer (si->dpy);		/* ############ DANGER! */
      ungrab_mouse (si);
      grab_mouse (si, ssi->screensaver_window,
                  (si->demoing_p ? 0 : ssi->cursor),
                  new_screen_no);
      XUngrabServer (si->dpy);
      XSync (si->dpy, False);		/* ###### (danger over) */
    }

  /* If the old window held our keyboard grab, transfer the grab to the new
     window.  (Grab the server while so doing, to avoid a race condition.)
   */
  if (old_w == si->keyboard_grab_window)
    {
      XGrabServer (si->dpy);		/* ############ DANGER! */
      ungrab_kbd(si);
      grab_kbd(si, ssi->screensaver_window, ssi->number);
      XUngrabServer (si->dpy);
      XSync (si->dpy, False);		/* ###### (danger over) */
    }
}



Bool
select_visual (saver_screen_info *ssi, const char *visual_name)
{
  saver_info *si = ssi->global;
  saver_preferences *p = &si->prefs;
  Bool install_cmap_p = p->install_cmap_p;
  Bool was_installed_p = (ssi->cmap != DefaultColormapOfScreen(ssi->screen));
  Visual *new_v = 0;
  Bool got_it;

  /* On some systems (most recently, MacOS X) OpenGL programs get confused
     when you kill one and re-start another on the same window.  So maybe
     it's best to just always destroy and recreate the xscreensaver window
     when changing hacks, instead of trying to reuse the old one?
   */
  Bool always_recreate_window_p = True;

  if (visual_name && *visual_name)
    {
      if (!strcmp(visual_name, "default-i") ||
          !strcmp(visual_name, "Default-i") ||
          !strcmp(visual_name, "Default-I")
          )
	{
	  visual_name = "default";
	  install_cmap_p = True;
	}
      else if (!strcmp(visual_name, "default-n") ||
               !strcmp(visual_name, "Default-n") ||
               !strcmp(visual_name, "Default-N"))
	{
	  visual_name = "default";
	  install_cmap_p = False;
	}
      else if (!strcmp(visual_name, "gl") ||
               !strcmp(visual_name, "Gl") ||
               !strcmp(visual_name, "GL"))
        {
          new_v = ssi->best_gl_visual;
          if (!new_v && p->verbose_p)
            fprintf (stderr, "%s: no GL visuals.\n", progname);
        }

      if (!new_v)
        new_v = get_visual (ssi->screen, visual_name, True, False);
    }
  else
    {
      new_v = ssi->default_visual;
    }

  got_it = !!new_v;

  if (new_v && new_v != DefaultVisualOfScreen(ssi->screen))
    /* It's not the default visual, so we have no choice but to install. */
    install_cmap_p = True;

  ssi->install_cmap_p = install_cmap_p;

  if (new_v &&
      (always_recreate_window_p ||
       (ssi->current_visual != new_v) ||
       (install_cmap_p != was_installed_p)))
    {
      Colormap old_c = ssi->cmap;
      Window old_w = ssi->screensaver_window;

      if (p->verbose_p)
	{
	  fprintf (stderr, "%s: %d: visual ", blurb(), ssi->number);
	  describe_visual (stderr, ssi->screen, new_v, install_cmap_p);
#if 0
	  fprintf (stderr, "%s:                  from ", blurb());
	  describe_visual (stderr, ssi->screen, ssi->current_visual,
			   was_installed_p);
#endif
	}

      reset_stderr (ssi);
      ssi->current_visual = new_v;
      ssi->current_depth = visual_depth(ssi->screen, new_v);
      ssi->cmap = 0;
      ssi->screensaver_window = 0;

      initialize_screensaver_window_1 (ssi);

      /* stderr_overlay_window is a child of screensaver_window, so we need
	 to destroy that as well (actually, we just need to invalidate and
	 drop our pointers to it, but this will destroy it, which is ok so
	 long as it happens before old_w itself is destroyed.) */
      reset_stderr (ssi);

      raise_window (si, True, True, False);
      store_vroot_property (si->dpy,
			    ssi->screensaver_window, ssi->screensaver_window);

      /* Transfer any grabs from the old window to the new. */
      maybe_transfer_grabs (ssi, old_w, ssi->screensaver_window, ssi->number);

      /* Now we can destroy the old window without horking our grabs. */
      XDestroyWindow (si->dpy, old_w);

      if (p->verbose_p)
	fprintf (stderr, "%s: %d: destroyed old saver window 0x%lx.\n",
		 blurb(), ssi->number, (unsigned long) old_w);

      if (old_c &&
	  old_c != DefaultColormapOfScreen (ssi->screen) &&
	  old_c != ssi->demo_cmap)
	XFreeColormap (si->dpy, old_c);
    }

  return got_it;
}
