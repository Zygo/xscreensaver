/* xscreensaver, Copyright (c) 1991-1993 Jamie Zawinski <jwz@mcom.com>
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
#include <X11/Intrinsic.h>
#include <X11/Xos.h>
#ifndef VMS
#include <X11/Xmu/Error.h>
#else
#include "sys$common:[decw$include.xmu]Error.h"
#endif

#ifdef HAVE_XIDLE
#include <X11/extensions/xidle.h>
#endif

#include "xscreensaver.h"

#if __STDC__
# define P(x)x
#else
#define P(x)()
#endif

extern XtAppContext app;

Time cycle;
Time timeout;
Time pointer_timeout;
Time notice_events_timeout;

extern Bool use_xidle;
extern Bool dbox_up_p;
extern Bool locked_p;
extern Window screensaver_window;

extern Bool handle_clientmessage P((XEvent *, Bool));

static time_t last_activity_time; /* for non-XIdle mode */
static XtIntervalId timer_id = 0;
static XtIntervalId check_pointer_timer_id = 0;
XtIntervalId cycle_id = 0;
XtIntervalId lock_id = 0;

void
idle_timer (junk1, junk2)
     void *junk1;
     XtPointer junk2;
{
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
  fake_event.xany.display = dpy;
  fake_event.xany.window  = 0;
  XPutBackEvent (dpy, &fake_event);
}


static void
#if __STDC__
notice_events (Window window, Bool top_p)
#else
notice_events (window, top_p)
     Window window;
     Bool top_p;
#endif
{
  XWindowAttributes attrs;
  unsigned long events;
  Window root, parent, *kids;
  unsigned int nkids;

  if (XtWindowToWidget (dpy, window))
    /* If it's one of ours, don't mess up its event mask. */
    return;

  if (!XQueryTree (dpy, window, &root, &parent, &kids, &nkids))
    return;
  if (window == root)
    top_p = False;

  XGetWindowAttributes (dpy, window, &attrs);
  events = ((attrs.all_event_masks | attrs.do_not_propagate_mask)
	    & KeyPressMask);

  /* Select for SubstructureNotify on all windows.
     Select for KeyPress on all windows that already have it selected.
     Do we need to select for ButtonRelease?  I don't think so.
   */
  XSelectInput (dpy, window, SubstructureNotifyMask | events);

  if (top_p && verbose_p && (events & KeyPressMask))
    {
      /* Only mention one window per tree (hack hack). */
      printf ("%s: selected KeyPress on 0x%X\n", progname, window);
      top_p = False;
    }

  if (kids)
    {
      while (nkids)
	notice_events (kids [--nkids], top_p);
      XFree ((char *) kids);
    }
}


int
BadWindow_ehandler (dpy, error)
     Display *dpy;
     XErrorEvent *error;
{
  /* When we notice a window being created, we spawn a timer that waits
     30 seconds or so, and then selects events on that window.  This error
     handler is used so that we can cope with the fact that the window
     may have been destroyed <30 seconds after it was created.
   */
  if (error->error_code == BadWindow ||
      error->error_code == BadDrawable)
    return 0;
  XmuPrintDefaultErrorMessage (dpy, error, stderr);
  exit (1);
}

void
notice_events_timer (closure, timer)
     XtPointer closure;
     void *timer;
{
  Window window = (Window) closure;
  int (*old_handler) ();
  old_handler = XSetErrorHandler (BadWindow_ehandler);
  notice_events (window, True);
  XSync (dpy, False);
  XSetErrorHandler (old_handler);
}


/* When the screensaver is active, this timer will periodically change
   the running program.
 */
void
cycle_timer (junk1, junk2)
     void *junk1;
     XtPointer junk2;
{
  Time how_long = cycle;
  if (dbox_up_p)
    {
      if (verbose_p)
	printf ("%s: dbox up; delaying hack change.\n", progname);
      how_long = 30000; /* 30 secs */
    }
  else
    {
      if (verbose_p)
	printf ("%s: changing graphics hacks.\n", progname);
      kill_screenhack ();
      spawn_screenhack (False);
    }
  cycle_id = XtAppAddTimeOut (app, how_long, cycle_timer, 0);
}


void
activate_lock_timer (junk1, junk2)
     void *junk1;
     XtPointer junk2;
{
  if (verbose_p)
    printf ("%s: timed out; activating lock\n", progname);
  locked_p = True;
}


/* Call this when user activity (or "simulated" activity) has been noticed.
 */
static void
reset_timers P((void))
{
#ifdef DEBUG_TIMERS
  if (verbose_p)
    printf ("%s: restarting idle_timer (%d, %d)\n",
	    progname, timeout, timer_id);
#endif
  XtRemoveTimeOut (timer_id);
  timer_id = XtAppAddTimeOut (app, timeout, idle_timer, 0);
  if (cycle_id) abort ();

  last_activity_time = time ((time_t *) 0);
}

/* When we aren't using XIdle, this timer is used to periodically wake up
   and poll the mouse position, which is possibly more reliable than
   selecting motion events on every window.
 */
static void
check_pointer_timer (closure, this_timer)
     void *closure;
     XtPointer this_timer;
{
  static int last_root_x = -1;
  static int last_root_y = -1;
  static Window last_child = (Window) -1;
  static unsigned int last_mask = 0;
  Window root, child;
  int root_x, root_y, x, y;
  unsigned int mask;
  XtIntervalId *timerP = (XtIntervalId *) closure;
#ifdef HAVE_XIDLE
  if (use_xidle)
    abort ();
#endif

  *timerP = XtAppAddTimeOut (app, pointer_timeout, check_pointer_timer,
			     closure);

  XQueryPointer (dpy, screensaver_window, &root, &child,
		 &root_x, &root_y, &x, &y, &mask);
  if (root_x == last_root_x && root_y == last_root_y &&
      child == last_child && mask == last_mask)
    return;

#ifdef DEBUG_TIMERS
  if (verbose_p && this_timer)
    if (root_x == last_root_x && root_y == last_root_y && child == last_child)
      printf ("%s: modifiers changed at %s.\n", progname, timestring ());
    else
      printf ("%s: pointer moved at %s.\n", progname, timestring ());
#endif

  last_root_x = root_x;
  last_root_y = root_y;
  last_child = child;
  last_mask = mask;

  reset_timers ();
}


void
sleep_until_idle (until_idle_p)
     Bool until_idle_p;
{
  XEvent event;

  if (until_idle_p)
    {
      timer_id = XtAppAddTimeOut (app, timeout, idle_timer, 0);
#ifdef HAVE_XIDLE
      if (! use_xidle)
#endif
	/* start polling the mouse position */
	check_pointer_timer (&check_pointer_timer_id, 0);
    }

  while (1)
    {
      XtAppNextEvent (app, &event);

      switch (event.xany.type) {
      case 0:		/* our synthetic "timeout" event has been signalled */
	if (until_idle_p)
	  {
	    Time idle;
#ifdef HAVE_XIDLE
	    if (use_xidle)
	      {
		if (! XGetIdleTime (dpy, &idle))
		  {
		    fprintf (stderr, "%s: %sXGetIdleTime() failed.\n",
			     progname, (verbose_p ? "## " : ""));
		    exit (1);
		  }
	      }
	    else
#endif /* HAVE_XIDLE */
	      idle = 1000 * (last_activity_time - time ((time_t *) 0));
	    
	    if (idle >= timeout)
	      goto DONE;
	    else
	      timer_id = XtAppAddTimeOut (app, timeout - idle,
					  idle_timer, 0);
	  }
	break;

      case ClientMessage:
	if (handle_clientmessage (&event, until_idle_p))
	  goto DONE;
	break;

      case CreateNotify:
#ifdef HAVE_XIDLE
	if (! use_xidle)
#endif
	  XtAppAddTimeOut (app, notice_events_timeout, notice_events_timer,
			   (XtPointer) event.xcreatewindow.window);
	break;

      case KeyPress:
      case KeyRelease:
      case ButtonPress:
      case ButtonRelease:
      case MotionNotify:

#ifdef DEBUG_TIMERS
	if (verbose_p)
	  {
	    if (event.xany.type == MotionNotify)
	      printf ("%s: MotionNotify at %s\n", progname, timestring ());
	    else if (event.xany.type == KeyPress)
	      printf ("%s: KeyPress seen on 0x%X at %s\n", progname,
		      event.xkey.window, timestring ());
	  }
#endif

	/* We got a user event */
	if (!until_idle_p)
	  goto DONE;
	else
	  reset_timers ();
	break;

      default:
	XtDispatchEvent (&event);
      }
    }
 DONE:

  if (check_pointer_timer_id)
    {
      XtRemoveTimeOut (check_pointer_timer_id);
      check_pointer_timer_id = 0;
    }
  if (timer_id)
    {
      XtRemoveTimeOut (timer_id);
      timer_id = 0;
    }

  if (until_idle_p && cycle_id)
    abort ();

  return;
}
