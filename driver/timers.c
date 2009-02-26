/* timers.c --- detecting when the user is idle, and other timer-related tasks.
 * xscreensaver, Copyright (c) 1991-1997, 1998
 *  Jamie Zawinski <jwz@netscape.com>
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

/* #define DEBUG_TIMERS */

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/Xos.h>
#ifdef HAVE_XMU
# ifndef VMS
#  include <X11/Xmu/Error.h>
# else /* VMS */
#  include <Xmu/Error.h>
# endif /* VMS */
# else /* !HAVE_XMU */
# include "xmu.h"
#endif /* !HAVE_XMU */

#ifdef HAVE_XIDLE_EXTENSION
#include <X11/extensions/xidle.h>
#endif /* HAVE_XIDLE_EXTENSION */

#ifdef HAVE_MIT_SAVER_EXTENSION
#include <X11/extensions/scrnsaver.h>
#endif /* HAVE_MIT_SAVER_EXTENSION */

#ifdef HAVE_SGI_SAVER_EXTENSION
#include <X11/extensions/XScreenSaver.h>
#endif /* HAVE_SGI_SAVER_EXTENSION */

#ifdef HAVE_XHPDISABLERESET
# include <X11/XHPlib.h>
  extern Bool hp_locked_p;	/* from windows.c */
#endif /* HAVE_XHPDISABLERESET */

#include "xscreensaver.h"


void
idle_timer (XtPointer closure, XtIntervalId *id)
{
  saver_info *si = (saver_info *) closure;

  /* What an amazingly shitty design.  Not only does Xt execute timeout
     events from XtAppNextEvent() instead of from XtDispatchEvent(), but
     there is no way to tell Xt to block until there is an X event OR a
     timeout happens.  Once your timeout proc is called, XtAppNextEvent()
     still won't return until a "real" X event comes in.

     So this function pushes a stupid, gratuitous, unnecessary event back
     on the event queue to force XtAppNextEvent to return Right Fucking Now.
     When the code in sleep_until_idle() sees an event of type XAnyEvent,
     which the server never generates, it knows that a timeout has occurred.
   */
  XEvent fake_event;
  fake_event.type = 0;	/* XAnyEvent type, ignored. */
  fake_event.xany.display = si->dpy;
  fake_event.xany.window  = 0;
  XPutBackEvent (si->dpy, &fake_event);
}


static void
notice_events (saver_info *si, Window window, Bool top_p)
{
  saver_preferences *p = &si->prefs;
  XWindowAttributes attrs;
  unsigned long events;
  Window root, parent, *kids;
  unsigned int nkids;

  if (XtWindowToWidget (si->dpy, window))
    /* If it's one of ours, don't mess up its event mask. */
    return;

  if (!XQueryTree (si->dpy, window, &root, &parent, &kids, &nkids))
    return;
  if (window == root)
    top_p = False;

  XGetWindowAttributes (si->dpy, window, &attrs);
  events = ((attrs.all_event_masks | attrs.do_not_propagate_mask)
	    & KeyPressMask);

  /* Select for SubstructureNotify on all windows.
     Select for KeyPress on all windows that already have it selected.
     Do we need to select for ButtonRelease?  I don't think so.
   */
  XSelectInput (si->dpy, window, SubstructureNotifyMask | events);

  if (top_p && p->verbose_p && (events & KeyPressMask))
    {
      /* Only mention one window per tree (hack hack). */
      printf ("%s: selected KeyPress on 0x%lX\n", progname,
	      (unsigned long) window);
      top_p = False;
    }

  if (kids)
    {
      while (nkids)
	notice_events (si, kids [--nkids], top_p);
      XFree ((char *) kids);
    }
}


int
BadWindow_ehandler (Display *dpy, XErrorEvent *error)
{
  /* When we notice a window being created, we spawn a timer that waits
     30 seconds or so, and then selects events on that window.  This error
     handler is used so that we can cope with the fact that the window
     may have been destroyed <30 seconds after it was created.
   */
  if (error->error_code == BadWindow ||
      error->error_code == BadMatch ||
      error->error_code == BadDrawable)
    return 0;
  else
    return saver_ehandler (dpy, error);
}


struct notice_events_timer_arg {
  saver_info *si;
  Window w;
};

static void
notice_events_timer (XtPointer closure, XtIntervalId *id)
{
  struct notice_events_timer_arg *arg =
    (struct notice_events_timer_arg *) closure;

  XErrorHandler old_handler = XSetErrorHandler (BadWindow_ehandler);

  saver_info *si = arg->si;
  Window window = arg->w;

  free(arg);
  notice_events (si, window, True);
  XSync (si->dpy, False);
  XSetErrorHandler (old_handler);
}

void
start_notice_events_timer (saver_info *si, Window w)
{
  saver_preferences *p = &si->prefs;
  struct notice_events_timer_arg *arg =
    (struct notice_events_timer_arg *) malloc(sizeof(*arg));
  arg->si = si;
  arg->w = w;
  XtAppAddTimeOut (si->app, p->notice_events_timeout, notice_events_timer,
		   (XtPointer) arg);
}


/* When the screensaver is active, this timer will periodically change
   the running program.
 */
void
cycle_timer (XtPointer closure, XtIntervalId *id)
{
  saver_info *si = (saver_info *) closure;
  saver_preferences *p = &si->prefs;
  Time how_long = p->cycle;
  if (si->dbox_up_p || si->question_up_p)
    {
      if (p->verbose_p)
	printf ("%s: dialog box up; delaying hack change.\n", progname);
      how_long = 30000; /* 30 secs */
    }
  else
    {
      if (p->verbose_p)
	printf ("%s: changing graphics hacks.\n", progname);
      kill_screenhack (si);
      spawn_screenhack (si, False);
    }
  si->cycle_id = XtAppAddTimeOut (si->app, how_long, cycle_timer,
				  (XtPointer) si);

#ifdef DEBUG_TIMERS
  if (p->verbose_p)
    printf ("%s: starting cycle_timer (%ld, %ld)\n",
	    progname, how_long, si->cycle_id);
#endif /* DEBUG_TIMERS */
}


void
activate_lock_timer (XtPointer closure, XtIntervalId *id)
{
  saver_info *si = (saver_info *) closure;
  saver_preferences *p = &si->prefs;

  if (p->verbose_p)
    printf ("%s: timed out; activating lock\n", progname);
  si->locked_p = True;

#ifdef HAVE_XHPDISABLERESET
  if (!hp_locked_p)
    {
      XHPDisableReset (si->dpy);	/* turn off C-Sh-Reset */
      hp_locked_p = True;
    }
#endif
}


/* Call this when user activity (or "simulated" activity) has been noticed.
 */
static void
reset_timers (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  if (p->use_mit_saver_extension || p->use_sgi_saver_extension)
    return;

#ifdef DEBUG_TIMERS
  if (p->verbose_p)
    printf ("%s:   killing idle_timer    (%ld, %ld)\n",
	    progname, p->timeout, si->timer_id);
#endif /* DEBUG_TIMERS */

  XtRemoveTimeOut (si->timer_id);
  si->timer_id = XtAppAddTimeOut (si->app, p->timeout, idle_timer,
				  (XtPointer) si);
  if (si->cycle_id) abort ();

#ifdef DEBUG_TIMERS
  if (p->verbose_p)
    printf ("%s:   restarting idle_timer (%ld, %ld)\n",
	    progname, p->timeout, si->timer_id);
#endif /* DEBUG_TIMERS */

  si->last_activity_time = time ((time_t *) 0);
}

/* When we aren't using a server extension, this timer is used to periodically
   wake up and poll the mouse position, which is possibly more reliable than
   selecting motion events on every window.
 */
static void
check_pointer_timer (XtPointer closure, XtIntervalId *id)
{
  int i;
  saver_info *si = (saver_info *) closure;
  saver_preferences *p = &si->prefs;
  Bool active_p = False;

  if (p->use_xidle_extension ||
      p->use_mit_saver_extension ||
      p->use_sgi_saver_extension)
    abort ();

  si->check_pointer_timer_id =
    XtAppAddTimeOut (si->app, p->pointer_timeout, check_pointer_timer,
		     (XtPointer) si);

  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      Window root, child;
      int root_x, root_y, x, y;
      unsigned int mask;

      XQueryPointer (si->dpy, ssi->screensaver_window, &root, &child,
		     &root_x, &root_y, &x, &y, &mask);

      if (root_x == ssi->poll_mouse_last_root_x &&
	  root_y == ssi->poll_mouse_last_root_y &&
	  child  == ssi->poll_mouse_last_child &&
	  mask   == ssi->poll_mouse_last_mask)
	continue;

      active_p = True;

#ifdef DEBUG_TIMERS
      if (p->verbose_p)
	if (root_x == ssi->poll_mouse_last_root_x &&
	    root_y == ssi->poll_mouse_last_root_y &&
	    child  == ssi->poll_mouse_last_child)
	  printf ("%s: modifiers changed at %s on screen %d.\n",
		  progname, timestring(), i);
	else
	  printf ("%s: pointer moved at %s on screen %d.\n",
		  progname, timestring(), i);
#endif /* DEBUG_TIMERS */

      si->last_activity_screen    = ssi;
      ssi->poll_mouse_last_root_x = root_x;
      ssi->poll_mouse_last_root_y = root_y;
      ssi->poll_mouse_last_child  = child;
      ssi->poll_mouse_last_mask   = mask;
    }

  if (active_p)
    reset_timers (si);
}


void
sleep_until_idle (saver_info *si, Bool until_idle_p)
{
  saver_preferences *p = &si->prefs;
  XEvent event;

  if (until_idle_p)
    {
      if (!p->use_mit_saver_extension && !p->use_sgi_saver_extension)
	{
	  /* Wake up periodically to ask the server if we are idle. */
	  si->timer_id = XtAppAddTimeOut (si->app, p->timeout, idle_timer,
					  (XtPointer) si);

#ifdef DEBUG_TIMERS
	  if (p->verbose_p)
	    printf ("%s: starting idle_timer (%ld, %ld)\n",
		    progname, p->timeout, si->timer_id);
#endif /* DEBUG_TIMERS */
	}

      if (!p->use_xidle_extension &&
	  !p->use_mit_saver_extension &&
	  !p->use_sgi_saver_extension)
	/* start polling the mouse position */
	check_pointer_timer ((XtPointer) si, 0);
    }

  while (1)
    {
      XtAppNextEvent (si->app, &event);

      switch (event.xany.type) {
      case 0:		/* our synthetic "timeout" event has been signalled */
	if (until_idle_p)
	  {
	    Time idle;
#ifdef HAVE_XIDLE_EXTENSION
	    if (p->use_xidle_extension)
	      {
		if (! XGetIdleTime (si->dpy, &idle))
		  {
		    fprintf (stderr, "%s: XGetIdleTime() failed.\n", progname);
		    saver_exit (si, 1);
		  }
	      }
	    else
#endif /* HAVE_XIDLE_EXTENSION */
#ifdef HAVE_MIT_SAVER_EXTENSION
	      if (p->use_mit_saver_extension)
		{
		  /* We don't need to do anything in this case - the synthetic
		     event isn't necessary, as we get sent specific events
		     to wake us up. */
		  idle = 0;
		}
	    else
#endif /* HAVE_MIT_SAVER_EXTENSION */
#ifdef HAVE_SGI_SAVER_EXTENSION
	      if (p->use_sgi_saver_extension)
		{
		  /* We don't need to do anything in this case - the synthetic
		     event isn't necessary, as we get sent specific events
		     to wake us up. */
		  idle = 0;
		}
	    else
#endif /* HAVE_SGI_SAVER_EXTENSION */
	      {
		idle = 1000 * (si->last_activity_time - time ((time_t *) 0));
	      }
	    
	    if (idle >= p->timeout)
	      goto DONE;
	    else if (!p->use_mit_saver_extension &&
		     !p->use_sgi_saver_extension)
	      {
		si->timer_id = XtAppAddTimeOut (si->app, p->timeout - idle,
						idle_timer, (XtPointer) si);
#ifdef DEBUG_TIMERS
		if (p->verbose_p)
		  printf ("%s: starting idle_timer (%ld, %ld)\n",
			  progname, p->timeout - idle, si->timer_id);
#endif /* DEBUG_TIMERS */
	      }
	  }
	break;

      case ClientMessage:
	if (handle_clientmessage (si, &event, until_idle_p))
	  goto DONE;
	break;

      case CreateNotify:
	if (!p->use_xidle_extension &&
	    !p->use_mit_saver_extension &&
	    !p->use_sgi_saver_extension)
	  {
	    start_notice_events_timer (si, event.xcreatewindow.window);
#ifdef DEBUG_TIMERS
	    if (p->verbose_p)
	      printf ("%s: starting notice_events_timer for 0x%X (%lu)\n",
		      progname,
		      (unsigned int) event.xcreatewindow.window,
		      p->notice_events_timeout);
#endif /* DEBUG_TIMERS */
	  }
	break;

      case KeyPress:
      case KeyRelease:
      case ButtonPress:
      case ButtonRelease:
      case MotionNotify:

#ifdef DEBUG_TIMERS
	if (p->verbose_p)
	  {
	    if (event.xany.type == MotionNotify)
	      printf ("%s: MotionNotify at %s\n", progname, timestring ());
	    else if (event.xany.type == KeyPress)
	      printf ("%s: KeyPress seen on 0x%X at %s\n", progname,
		      (unsigned int) event.xkey.window, timestring ());
	  }
#endif /* DEBUG_TIMERS */

	/* We got a user event */
	if (!until_idle_p)
	  goto DONE;
	else
	  reset_timers (si);
	break;

      default:

#ifdef HAVE_MIT_SAVER_EXTENSION
	if (event.type == si->mit_saver_ext_event_number)
	  {
	    XScreenSaverNotifyEvent *sevent =
	      (XScreenSaverNotifyEvent *) &event;
	    if (sevent->state == ScreenSaverOn)
	      {
# ifdef DEBUG_TIMERS
		if (p->verbose_p)
		  printf ("%s: ScreenSaverOn event received at %s\n",
			  progname, timestring ());
# endif /* DEBUG_TIMERS */

		/* Get the "real" server window(s) out of the way as soon
		   as possible. */
		int i = 0;
		for (i = 0; i < si->nscreens; i++)
		  {
		    saver_screen_info *ssi = &si->screens[i];
		    if (ssi->server_mit_saver_window &&
			window_exists_p (si->dpy,
					 ssi->server_mit_saver_window))
		      XUnmapWindow (si->dpy, ssi->server_mit_saver_window);
		  }

		if (sevent->kind != ScreenSaverExternal)
		  {
# ifdef DEBUG_TIMERS
		    fprintf (stderr,
			 "%s: ScreenSaverOn event wasn't of type External!\n",
			     progname);
# endif /* DEBUG_TIMERS */
		  }

		if (until_idle_p)
		  goto DONE;
	      }
	    else if (sevent->state == ScreenSaverOff)
	      {
# ifdef DEBUG_TIMERS
		if (p->verbose_p)
		  printf ("%s: ScreenSaverOff event received at %s\n",
			  progname, timestring ());
# endif /* DEBUG_TIMERS */
		if (!until_idle_p)
		  goto DONE;
	      }
# ifdef DEBUG_TIMERS
	    else if (p->verbose_p)
	      printf ("%s: unknown MIT-SCREEN-SAVER event received at %s\n",
		      progname, timestring ());
# endif /* DEBUG_TIMERS */
	  }
	else

#endif /* HAVE_MIT_SAVER_EXTENSION */


#ifdef HAVE_SGI_SAVER_EXTENSION
	if (event.type == (si->sgi_saver_ext_event_number + ScreenSaverStart))
	  {
# ifdef DEBUG_TIMERS
	    if (p->verbose_p)
	      printf ("%s: ScreenSaverStart event received at %s\n",
		      progname, timestring ());
# endif /* DEBUG_TIMERS */

	    if (until_idle_p)
	      goto DONE;
	  }
	else if (event.type == (si->sgi_saver_ext_event_number +
				ScreenSaverEnd))
	  {
# ifdef DEBUG_TIMERS
	    if (p->verbose_p)
	      printf ("%s: ScreenSaverEnd event received at %s\n",
		      progname, timestring ());
# endif /* DEBUG_TIMERS */
	    if (!until_idle_p)
	      goto DONE;
	  }
	else
#endif /* HAVE_SGI_SAVER_EXTENSION */

	  XtDispatchEvent (&event);
      }
    }
 DONE:


  /* If there's a user event on the queue, swallow it.
     If we're using a server extension, and the user becomes active, we
     get the extension event before the user event -- so the keypress or
     motion or whatever is still on the queue.  This makes "unfade" not
     work, because it sees that event, and bugs out.  (This problem
     doesn't exhibit itself without an extension, because in that case,
     there's only one event generated by user activity, not two.)
   */
  XCheckMaskEvent (si->dpy, (KeyPressMask|ButtonPressMask|PointerMotionMask),
		   &event);


  if (si->check_pointer_timer_id)
    {
      XtRemoveTimeOut (si->check_pointer_timer_id);
      si->check_pointer_timer_id = 0;
    }
  if (si->timer_id)
    {
      XtRemoveTimeOut (si->timer_id);
      si->timer_id = 0;
    }

  if (until_idle_p && si->cycle_id)
    abort ();

  return;
}


/* This timer goes off every few minutes, whether the user is idle or not,
   to try and clean up anything that has gone wrong.

   It calls disable_builtin_screensaver() so that if xset has been used,
   or some other program (like xlock) has messed with the XSetScreenSaver()
   settings, they will be set back to sensible values (if a server extension
   is in use, messing with xlock can cause xscreensaver to never get a wakeup
   event, and could cause monitor power-saving to occur, and all manner of
   heinousness.)

   If the screen is currently blanked, it raises the window, in case some
   other window has been mapped on top of it.

   If the screen is currently blanked, and there is no hack running, it
   clears the window, in case there is an error message printed on it (we
   don't want the error message to burn in.)
 */

static void
watchdog_timer (XtPointer closure, XtIntervalId *id)
{
  saver_info *si = (saver_info *) closure;
  if (!si->demo_mode_p)
    {
      disable_builtin_screensaver (si, False);
      if (si->screen_blanked_p)
	{
	  Bool running_p = screenhack_running_p(si);

#ifdef DEBUG_TIMERS
	  if (si->prefs.verbose_p)
	    printf ("%s: watchdog timer raising %sscreen.\n",
		    progname, (running_p ? "" : "and clearing "));
#endif /* DEBUG_TIMERS */

	  raise_window (si, True, True, running_p);

	  if (!monitor_powered_on_p (si))
	    {
	      if (si->prefs.verbose_p)
		printf ("%s: server reports that monitor has powered down; "
			"killing running hacks.\n", progname);
	      kill_screenhack (si);
	    }
	}
    }
}


void
reset_watchdog_timer (saver_info *si, Bool on_p)
{
  saver_preferences *p = &si->prefs;

  if (si->watchdog_id)
    {
      XtRemoveTimeOut (si->watchdog_id);
      si->watchdog_id = 0;
    }

  if (on_p && p->watchdog_timeout)
    {
      si->watchdog_id = XtAppAddTimeOut (si->app, p->watchdog_timeout,
					 watchdog_timer, (XtPointer) si);

#ifdef DEBUG_TIMERS
      if (p->verbose_p)
	printf ("%s: restarting watchdog_timer (%ld, %ld)\n",
		progname, p->watchdog_timeout, si->watchdog_id);
#endif /* DEBUG_TIMERS */

    }
}
