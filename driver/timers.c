/* timers.c --- detecting when the user is idle, and other timer-related tasks.
 * xscreensaver, Copyright (c) 1991-2008 Jamie Zawinski <jwz@jwz.org>
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

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/Xos.h>
#include <time.h>
#include <sys/time.h>
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

#ifdef HAVE_RANDR
#include <X11/extensions/Xrandr.h>
#endif /* HAVE_RANDR */

#include "xscreensaver.h"

#undef ABS
#define ABS(x)((x)<0?-(x):(x))

#undef MAX
#define MAX(x,y)((x)>(y)?(x):(y))


#ifdef HAVE_PROC_INTERRUPTS
static Bool proc_interrupts_activity_p (saver_info *si);
#endif /* HAVE_PROC_INTERRUPTS */

static void check_for_clock_skew (saver_info *si);


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

  /* If we are the timer that just went off, clear the pointer to the id. */
  if (id)
    {
      if (si->timer_id && *id != si->timer_id)
        abort();  /* oops, scheduled timer twice?? */
      si->timer_id = 0;
    }
}


void
schedule_wakeup_event (saver_info *si, Time when, Bool verbose_p)
{
  if (si->timer_id)
    {
      if (verbose_p)
        fprintf (stderr, "%s: idle_timer already running\n", blurb());
      return;
    }

  /* Wake up periodically to ask the server if we are idle. */
  si->timer_id = XtAppAddTimeOut (si->app, when, idle_timer,
                                  (XtPointer) si);

  if (verbose_p)
    fprintf (stderr, "%s: starting idle_timer (%ld, %ld)\n",
             blurb(), when, si->timer_id);
}


static void
notice_events (saver_info *si, Window window, Bool top_p)
{
  saver_preferences *p = &si->prefs;
  XWindowAttributes attrs;
  unsigned long events;
  Window root, parent, *kids;
  unsigned int nkids;
  int screen_no;

  if (XtWindowToWidget (si->dpy, window))
    /* If it's one of ours, don't mess up its event mask. */
    return;

  if (!XQueryTree (si->dpy, window, &root, &parent, &kids, &nkids))
    return;
  if (window == root)
    top_p = False;

  /* Figure out which screen this window is on, for the diagnostics. */
  for (screen_no = 0; screen_no < si->nscreens; screen_no++)
    if (root == RootWindowOfScreen (si->screens[screen_no].screen))
      break;

  XGetWindowAttributes (si->dpy, window, &attrs);
  events = ((attrs.all_event_masks | attrs.do_not_propagate_mask)
	    & KeyPressMask);

  /* Select for SubstructureNotify on all windows.
     Select for KeyPress on all windows that already have it selected.

     Note that we can't select for ButtonPress, because of X braindamage:
     only one client at a time may select for ButtonPress on a given
     window, though any number can select for KeyPress.  Someone explain
     *that* to me.

     So, if the user spends a while clicking the mouse without ever moving
     the mouse or touching the keyboard, we won't know that they've been
     active, and the screensaver will come on.  That sucks, but I don't
     know how to get around it.

     Since X presents mouse wheels as clicks, this applies to those, too:
     scrolling through a document using only the mouse wheel doesn't
     count as activity...  Fortunately, /proc/interrupts helps, on
     systems that have it.  Oh, if it's a PS/2 mouse, not serial or USB.
     This sucks!
   */
  XSelectInput (si->dpy, window, SubstructureNotifyMask | events);

  if (top_p && p->debug_p && (events & KeyPressMask))
    {
      /* Only mention one window per tree (hack hack). */
      fprintf (stderr, "%s: %d: selected KeyPress on 0x%lX\n",
               blurb(), screen_no, (unsigned long) window);
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
start_notice_events_timer (saver_info *si, Window w, Bool verbose_p)
{
  saver_preferences *p = &si->prefs;
  struct notice_events_timer_arg *arg =
    (struct notice_events_timer_arg *) malloc(sizeof(*arg));
  arg->si = si;
  arg->w = w;
  XtAppAddTimeOut (si->app, p->notice_events_timeout, notice_events_timer,
		   (XtPointer) arg);

  if (verbose_p)
    fprintf (stderr, "%s: starting notice_events_timer for 0x%X (%lu)\n",
             blurb(), (unsigned int) w, p->notice_events_timeout);
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

  if (si->selection_mode > 0 &&
      screenhack_running_p (si))
    /* If we're in "SELECT n" mode, the cycle timer going off will just
       restart this same hack again.  There's not much point in doing this
       every 5 or 10 minutes, but on the other hand, leaving one hack running
       for days is probably not a great idea, since they tend to leak and/or
       crash.  So, restart the thing once an hour. */
    how_long = 1000 * 60 * 60;

  if (si->dbox_up_p)
    {
      if (p->verbose_p)
	fprintf (stderr, "%s: dialog box up; delaying hack change.\n",
		 blurb());
      how_long = 30000; /* 30 secs */
    }
  else
    {
      int i;
      maybe_reload_init_file (si);
      for (i = 0; i < si->nscreens; i++)
        kill_screenhack (&si->screens[i]);

      raise_window (si, True, True, False);

      if (!si->throttled_p)
        for (i = 0; i < si->nscreens; i++)
          spawn_screenhack (&si->screens[i]);
      else
        {
          if (p->verbose_p)
            fprintf (stderr, "%s: not launching new hack (throttled.)\n",
                     blurb());
        }
    }

  if (how_long > 0)
    {
      si->cycle_id = XtAppAddTimeOut (si->app, how_long, cycle_timer,
                                      (XtPointer) si);

      if (p->debug_p)
        fprintf (stderr, "%s: starting cycle_timer (%ld, %ld)\n",
                 blurb(), how_long, si->cycle_id);
    }
  else
    {
      if (p->debug_p)
        fprintf (stderr, "%s: not starting cycle_timer: how_long == %ld\n",
                 blurb(), (unsigned long) how_long);
    }
}


void
activate_lock_timer (XtPointer closure, XtIntervalId *id)
{
  saver_info *si = (saver_info *) closure;
  saver_preferences *p = &si->prefs;

  if (p->verbose_p)
    fprintf (stderr, "%s: timed out; activating lock.\n", blurb());
  set_locked_p (si, True);
}


/* Call this when user activity (or "simulated" activity) has been noticed.
 */
void
reset_timers (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  if (si->using_mit_saver_extension || si->using_sgi_saver_extension)
    return;

  if (si->timer_id)
    {
      if (p->debug_p)
        fprintf (stderr, "%s: killing idle_timer  (%ld, %ld)\n",
                 blurb(), p->timeout, si->timer_id);
      XtRemoveTimeOut (si->timer_id);
      si->timer_id = 0;
    }

  schedule_wakeup_event (si, p->timeout, p->debug_p); /* sets si->timer_id */

  if (si->cycle_id) abort ();	/* no cycle timer when inactive */

  si->last_activity_time = time ((time_t *) 0);

  /* This will (hopefully, supposedly) tell the server to re-set its
     DPMS timer.  Without this, the -deactivate clientmessage would
     prevent xscreensaver from blanking, but would not prevent the
     monitor from powering down. */
#if 0
  /* #### With some servers, this causes the screen to flicker every
     time a key is pressed!  Ok, I surrender.  I give up on ever
     having DPMS work properly.
   */
  XForceScreenSaver (si->dpy, ScreenSaverReset);

  /* And if the monitor is already powered off, turn it on.
     You'd think the above would do that, but apparently not? */
  monitor_power_on (si);
#endif

}


/* Returns true if the mouse has moved since the last time we checked.
   Small motions (of less than "hysteresis" pixels/second) are ignored.
 */
static Bool
pointer_moved_p (saver_screen_info *ssi, Bool mods_p)
{
  saver_info *si = ssi->global;
  saver_preferences *p = &si->prefs;

  Window root, child;
  int root_x, root_y, x, y;
  unsigned int mask;
  time_t now = time ((time_t *) 0);
  unsigned int distance, dps;
  unsigned long seconds = 0;
  Bool moved_p = False;

  /* don't check xinerama pseudo-screens. */
  if (!ssi->real_screen_p) return False;

  if (!XQueryPointer (si->dpy, ssi->screensaver_window, &root, &child,
                      &root_x, &root_y, &x, &y, &mask))
    {
      /* If XQueryPointer() returns false, the mouse is not on this screen.
       */
      x = root_x = -1;
      y = root_y = -1;
      root = child = 0;
      mask = 0;
    }

  distance = MAX (ABS (ssi->poll_mouse_last_root_x - root_x),
                  ABS (ssi->poll_mouse_last_root_y - root_y));
  seconds = (now - ssi->poll_mouse_last_time);


  /* When the screen is blanked, we get MotionNotify events, but when not
     blanked, we poll only every 5 seconds, and that's not enough resolution
     to do hysteresis based on a 1 second interval.  So, assume that any
     motion we've seen during the 5 seconds when our eyes were closed happened
     in the last 1 second instead.
   */
  if (seconds > 1) seconds = 1;

  dps = (seconds <= 0 ? distance : (distance / seconds));

  /* Motion only counts if the rate is more than N pixels per second.
   */
  if (dps >= p->pointer_hysteresis &&
      distance > 0)
    moved_p = True;

  /* If the mouse is not on this screen but used to be, that's motion.
     If the mouse was not on this screen, but is now, that's motion.
   */
  {
    Bool on_screen_p  = (root_x != -1 && root_y != -1);
    Bool was_on_screen_p = (ssi->poll_mouse_last_root_x != -1 &&
                            ssi->poll_mouse_last_root_y != -1);

    if (on_screen_p != was_on_screen_p)
      moved_p = True;
  }

  if (p->debug_p && (distance != 0 || moved_p))
    {
      fprintf (stderr, "%s: %d: pointer %s", blurb(), ssi->number,
               (moved_p ? "moved:  " : "ignored:"));
      if (ssi->poll_mouse_last_root_x == -1)
        fprintf (stderr, "off screen");
      else
        fprintf (stderr, "%d,%d",
                 ssi->poll_mouse_last_root_x,
                 ssi->poll_mouse_last_root_y);
      fprintf (stderr, " -> ");
      if (root_x == -1)
        fprintf (stderr, "off screen");
      else
        fprintf (stderr, "%d,%d", root_x, root_y);
      if (ssi->poll_mouse_last_root_x != -1 && root_x != -1)
        fprintf (stderr, " (%d,%d; %d/%lu=%d)",
                 ABS(ssi->poll_mouse_last_root_x - root_x),
                 ABS(ssi->poll_mouse_last_root_y - root_y),
                 distance, seconds, dps);

      fprintf (stderr, ".\n");
    }

  if (!moved_p &&
      mods_p &&
      mask != ssi->poll_mouse_last_mask)
    {
      moved_p = True;

      if (p->debug_p)
        fprintf (stderr, "%s: %d: modifiers changed: 0x%04x -> 0x%04x.\n",
                 blurb(), ssi->number, ssi->poll_mouse_last_mask, mask);
    }

  si->last_activity_screen   = ssi;
  ssi->poll_mouse_last_child = child;
  ssi->poll_mouse_last_mask  = mask;

  if (moved_p || seconds > 0)
    {
      ssi->poll_mouse_last_time   = now;
      ssi->poll_mouse_last_root_x = root_x;
      ssi->poll_mouse_last_root_y = root_y;
    }

  return moved_p;
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

  if (!si->using_proc_interrupts &&
      (si->using_xidle_extension ||
       si->using_mit_saver_extension ||
       si->using_sgi_saver_extension))
    /* If an extension is in use, we should not be polling the mouse.
       Unless we're also checking /proc/interrupts, in which case, we should.
     */
    abort ();

  if (id && *id == si->check_pointer_timer_id)  /* this is us - it's expired */
    si->check_pointer_timer_id = 0;

  if (si->check_pointer_timer_id)		/* only queue one at a time */
    XtRemoveTimeOut (si->check_pointer_timer_id);

  si->check_pointer_timer_id =			/* now re-queue */
    XtAppAddTimeOut (si->app, p->pointer_timeout, check_pointer_timer,
		     (XtPointer) si);

  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      if (pointer_moved_p (ssi, True))
        active_p = True;
    }

#ifdef HAVE_PROC_INTERRUPTS
  if (!active_p &&
      si->using_proc_interrupts &&
      proc_interrupts_activity_p (si))
    {
      active_p = True;
    }
#endif /* HAVE_PROC_INTERRUPTS */

  if (active_p)
    reset_timers (si);

  check_for_clock_skew (si);
}


/* An unfortunate situation is this: the saver is not active, because the
   user has been typing.  The machine is a laptop.  The user closes the lid
   and suspends it.  The CPU halts.  Some hours later, the user opens the
   lid.  At this point, Xt's timers will fire, and xscreensaver will blank
   the screen.

   So far so good -- well, not really, but it's the best that we can do,
   since the OS doesn't send us a signal *before* shutdown -- but if the
   user had delayed locking (lockTimeout > 0) then we should start off
   in the locked state, rather than only locking N minutes from when the
   lid was opened.  Also, eschewing fading is probably a good idea, to
   clamp down as soon as possible.

   We only do this when we'd be polling the mouse position anyway.
   This amounts to an assumption that machines with APM support also
   have /proc/interrupts.
 */
static void
check_for_clock_skew (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  time_t now = time ((time_t *) 0);
  long shift = now - si->last_wall_clock_time;

  if (p->debug_p)
    {
      int i = (si->last_wall_clock_time == 0 ? 0 : shift);
      fprintf (stderr,
               "%s: checking wall clock for hibernation (%d:%02d:%02d).\n",
               blurb(),
               (i / (60 * 60)), ((i / 60) % 60), (i % 60));
    }

  if (si->last_wall_clock_time != 0 &&
      shift > (p->timeout / 1000))
    {
      if (p->verbose_p)
        fprintf (stderr, "%s: wall clock has jumped by %ld:%02ld:%02ld!\n",
                 blurb(),
                 (shift / (60 * 60)), ((shift / 60) % 60), (shift % 60));

      si->emergency_lock_p = True;
      idle_timer ((XtPointer) si, 0);
    }

  si->last_wall_clock_time = now;
}



static void
dispatch_event (saver_info *si, XEvent *event)
{
  /* If this is for the splash dialog, pass it along.
     Note that the password dialog is handled with its own event loop,
     so events for that window will never come through here.
   */
  if (si->splash_dialog && event->xany.window == si->splash_dialog)
    handle_splash_event (si, event);

  XtDispatchEvent (event);
}


static void
swallow_unlock_typeahead_events (saver_info *si, XEvent *e)
{
  XEvent event;
  char buf [100];
  int i = 0;

  memset (buf, 0, sizeof(buf));

  event = *e;

  do
    {
      if (event.xany.type == KeyPress)
        {
          char s[2];
          int size = XLookupString ((XKeyEvent *) &event, s, 1, 0, 0);
          if (size != 1) continue;
          switch (*s)
            {
            case '\010': case '\177':			/* Backspace */
              if (i > 0) i--;
              break;
            case '\025': case '\030':			/* Erase line */
            case '\012': case '\015':			/* Enter */
            case '\033':				/* ESC */
              i = 0;
              break;
            case '\040':				/* Space */
              if (i == 0)
                break;  /* ignore space at beginning of line */
              /* else, fall through */
            default:
              buf [i++] = *s;
              break;
            }
        }

    } while (i < sizeof(buf)-1 &&
             XCheckMaskEvent (si->dpy, KeyPressMask, &event));

  buf[i] = 0;

  if (si->unlock_typeahead)
    {
      memset (si->unlock_typeahead, 0, strlen(si->unlock_typeahead));
      free (si->unlock_typeahead);
    }

  if (i > 0)
    si->unlock_typeahead = strdup (buf);
  else
    si->unlock_typeahead = 0;

  memset (buf, 0, sizeof(buf));
}


/* methods of detecting idleness:

      explicitly informed by SGI SCREEN_SAVER server event;
      explicitly informed by MIT-SCREEN-SAVER server event;
      poll server idle time with XIDLE extension;
      select events on all windows, and note absence of recent events;
      note that /proc/interrupts has not changed in a while;
      activated by clientmessage.

   methods of detecting non-idleness:

      read events on the xscreensaver window;
      explicitly informed by SGI SCREEN_SAVER server event;
      explicitly informed by MIT-SCREEN-SAVER server event;
      select events on all windows, and note events on any of them;
      note that /proc/interrupts has changed;
      deactivated by clientmessage.

   I trust that explains why this function is a big hairy mess.
 */
void
sleep_until_idle (saver_info *si, Bool until_idle_p)
{
  saver_preferences *p = &si->prefs;
  XEvent event;

  /* We need to select events on all windows if we're not using any extensions.
     Otherwise, we don't need to. */
  Bool scanning_all_windows = !(si->using_xidle_extension ||
                                si->using_mit_saver_extension ||
                                si->using_sgi_saver_extension);

  /* We need to periodically wake up and check for idleness if we're not using
     any extensions, or if we're using the XIDLE extension.  The other two
     extensions explicitly deliver events when we go idle/non-idle, so we
     don't need to poll. */
  Bool polling_for_idleness = !(si->using_mit_saver_extension ||
                                si->using_sgi_saver_extension);

  /* Whether we need to periodically wake up and check to see if the mouse has
     moved.  We only need to do this when not using any extensions.  The reason
     this isn't the same as `polling_for_idleness' is that the "idleness" poll
     can happen (for example) 5 minutes from now, whereas the mouse-position
     poll should happen with low periodicity.  We don't need to poll the mouse
     position with the XIDLE extension, but we do need to periodically wake up
     and query the server with that extension.  For our purposes, polling
     /proc/interrupts is just like polling the mouse position.  It has to
     happen on the same kind of schedule. */
  Bool polling_mouse_position = (si->using_proc_interrupts ||
                                 !(si->using_xidle_extension ||
                                   si->using_mit_saver_extension ||
                                   si->using_sgi_saver_extension));

  if (until_idle_p)
    {
      if (polling_for_idleness)
        /* This causes a no-op event to be delivered to us in a while, so that
           we come back around through the event loop again.  */
        schedule_wakeup_event (si, p->timeout, p->debug_p);

      if (polling_mouse_position)
        /* Check to see if the mouse has moved, and set up a repeating timer
           to do so periodically (typically, every 5 seconds.) */
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

            /* We may be idle; check one last time to see if the mouse has
               moved, just in case the idle-timer went off within the 5 second
               window between mouse polling.  If the mouse has moved, then
               check_pointer_timer() will reset last_activity_time.
             */
            if (polling_mouse_position)
              check_pointer_timer ((XtPointer) si, 0);

#ifdef HAVE_XIDLE_EXTENSION
	    if (si->using_xidle_extension)
	      {
                /* The XIDLE extension uses the synthetic event to prod us into
                   re-asking the server how long the user has been idle. */
		if (! XGetIdleTime (si->dpy, &idle))
		  {
		    fprintf (stderr, "%s: XGetIdleTime() failed.\n", blurb());
		    saver_exit (si, 1, 0);
		  }
	      }
	    else
#endif /* HAVE_XIDLE_EXTENSION */
#ifdef HAVE_MIT_SAVER_EXTENSION
	      if (si->using_mit_saver_extension)
		{
		  /* We don't need to do anything in this case - the synthetic
		     event isn't necessary, as we get sent specific events
		     to wake us up.  In fact, this event generally shouldn't
                     be being delivered when the MIT extension is in use. */
		  idle = 0;
		}
	    else
#endif /* HAVE_MIT_SAVER_EXTENSION */
#ifdef HAVE_SGI_SAVER_EXTENSION
	      if (si->using_sgi_saver_extension)
		{
		  /* We don't need to do anything in this case - the synthetic
		     event isn't necessary, as we get sent specific events
		     to wake us up.  In fact, this event generally shouldn't
                     be being delivered when the SGI extension is in use. */
		  idle = 0;
		}
	    else
#endif /* HAVE_SGI_SAVER_EXTENSION */
	      {
                /* Otherwise, no server extension is in use.  The synthetic
                   event was to tell us to wake up and see if the user is now
                   idle.  Compute the amount of idle time by comparing the
                   `last_activity_time' to the wall clock.  The l_a_t was set
                   by calling `reset_timers()', which is called only in only
                   two situations: when polling the mouse position has revealed
                   the the mouse has moved (user activity) or when we have read
                   an event (again, user activity.)
                 */
		idle = 1000 * (si->last_activity_time - time ((time_t *) 0));
	      }

	    if (idle >= p->timeout)
              {
                /* Look, we've been idle long enough.  We're done. */
                goto DONE;
              }
            else if (si->emergency_lock_p)
              {
                /* Oops, the wall clock has jumped far into the future, so
                   we need to lock down in a hurry! */
                goto DONE;
              }
            else
              {
                /* The event went off, but it turns out that the user has not
                   yet been idle for long enough.  So re-signal the event.
                   Be economical: if we should blank after 5 minutes, and the
                   user has been idle for 2 minutes, then set this timer to
                   go off in 3 minutes.
                 */
                if (polling_for_idleness)
                  schedule_wakeup_event (si, p->timeout - idle, p->debug_p);
              }
	  }
	break;

      case ClientMessage:
	if (handle_clientmessage (si, &event, until_idle_p))
	  goto DONE;
	break;

      case CreateNotify:
        /* A window has been created on the screen somewhere.  If we're
           supposed to scan all windows for events, prepare this window. */
	if (scanning_all_windows)
	  {
            Window w = event.xcreatewindow.window;
	    start_notice_events_timer (si, w, p->debug_p);
	  }
	break;

      case KeyPress:
      case ButtonPress:
      /* Ignore release events so that hitting ESC at the password dialog
         doesn't result in the password dialog coming right back again when
         the fucking release key is seen! */
      /* case KeyRelease:*/
      /* case ButtonRelease:*/
      case MotionNotify:

	if (p->debug_p)
	  {
            Window root=0, window=0;
            int x=-1, y=-1;
            const char *type = 0;
	    if (event.xany.type == MotionNotify)
              {
                /*type = "MotionNotify";*/
                root = event.xmotion.root;
                window = event.xmotion.window;
                x = event.xmotion.x_root;
                y = event.xmotion.y_root;
              }
	    else if (event.xany.type == KeyPress)
              {
                type = "KeyPress";
                root = event.xkey.root;
                window = event.xkey.window;
                x = y = -1;
              }
	    else if (event.xany.type == ButtonPress)
              {
                type = "ButtonPress";
                root = event.xkey.root;
                window = event.xkey.window;
                x = event.xmotion.x_root;
                y = event.xmotion.y_root;
              }

            if (type)
              {
                int i;
                for (i = 0; i < si->nscreens; i++)
                  if (root == RootWindowOfScreen (si->screens[i].screen))
                    break;
                fprintf (stderr,"%s: %d: %s on 0x%lx",
                         blurb(), i, type, (unsigned long) window);

                /* Be careful never to do this unless in -debug mode, as
                   this could expose characters from the unlock password. */
                if (p->debug_p && event.xany.type == KeyPress)
                  {
                    KeySym keysym;
                    char c = 0;
                    XLookupString (&event.xkey, &c, 1, &keysym, 0);
                    fprintf (stderr, " (%s%s)",
                             (event.xkey.send_event ? "synthetic " : ""),
                             XKeysymToString (keysym));
                  }

                if (x == -1)
                  fprintf (stderr, "\n");
                else
                  fprintf (stderr, " at %d,%d.\n", x, y);
              }
	  }

	/* If any widgets want to handle this event, let them. */
	dispatch_event (si, &event);

        
        /* If we got a MotionNotify event, figure out what screen it
           was on and poll the mouse there: if the mouse hasn't moved
           far enough to count as "real" motion, then ignore this
           event.
         */
        if (event.xany.type == MotionNotify)
          {
            int i;
            for (i = 0; i < si->nscreens; i++)
              if (event.xmotion.root ==
                  RootWindowOfScreen (si->screens[i].screen))
                break;
            if (i < si->nscreens)
              {
                if (!pointer_moved_p (&si->screens[i], False))
                  continue;
              }
          }


	/* We got a user event.
	   If we're waiting for the user to become active, this is it.
	   If we're waiting until the user becomes idle, reset the timers
	   (since now we have longer to wait.)
	 */
	if (!until_idle_p)
	  {
	    if (si->demoing_p &&
		(event.xany.type == MotionNotify ||
		 event.xany.type == KeyRelease))
	      /* When we're demoing a single hack, mouse motion doesn't
		 cause deactivation.  Only clicks and keypresses do. */
	      ;
	    else
	      /* If we're not demoing, then any activity causes deactivation.
	       */
	      goto DONE;
	  }
	else
	  reset_timers (si);

	break;

      default:

#ifdef HAVE_MIT_SAVER_EXTENSION
	if (event.type == si->mit_saver_ext_event_number)
	  {
            /* This event's number is that of the MIT-SCREEN-SAVER server
               extension.  This extension has one event number, and the event
               itself contains sub-codes that say what kind of event it was
               (an "idle" or "not-idle" event.)
             */
	    XScreenSaverNotifyEvent *sevent =
	      (XScreenSaverNotifyEvent *) &event;
	    if (sevent->state == ScreenSaverOn)
	      {
		int i = 0;
		if (p->verbose_p)
		  fprintf (stderr, "%s: MIT ScreenSaverOn event received.\n",
			   blurb());

		/* Get the "real" server window(s) out of the way as soon
		   as possible. */
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
		    fprintf (stderr,
			 "%s: ScreenSaverOn event wasn't of type External!\n",
			     blurb());
		  }

		if (until_idle_p)
		  goto DONE;
	      }
	    else if (sevent->state == ScreenSaverOff)
	      {
		if (p->verbose_p)
		  fprintf (stderr, "%s: MIT ScreenSaverOff event received.\n",
			   blurb());
		if (!until_idle_p)
		  goto DONE;
	      }
	    else
	      fprintf (stderr,
		       "%s: unknown MIT-SCREEN-SAVER event %d received!\n",
		       blurb(), sevent->state);
	  }
	else

#endif /* HAVE_MIT_SAVER_EXTENSION */


#ifdef HAVE_SGI_SAVER_EXTENSION
	if (event.type == (si->sgi_saver_ext_event_number + ScreenSaverStart))
	  {
            /* The SGI SCREEN_SAVER server extension has two event numbers,
               and this event matches the "idle" event. */
	    if (p->verbose_p)
	      fprintf (stderr, "%s: SGI ScreenSaverStart event received.\n",
		       blurb());

	    if (until_idle_p)
	      goto DONE;
	  }
	else if (event.type == (si->sgi_saver_ext_event_number +
				ScreenSaverEnd))
	  {
            /* The SGI SCREEN_SAVER server extension has two event numbers,
               and this event matches the "idle" event. */
	    if (p->verbose_p)
	      fprintf (stderr, "%s: SGI ScreenSaverEnd event received.\n",
		       blurb());
	    if (!until_idle_p)
	      goto DONE;
	  }
	else
#endif /* HAVE_SGI_SAVER_EXTENSION */

#ifdef HAVE_RANDR
        if (event.type == (si->randr_event_number + RRScreenChangeNotify))
          {
            /* The Resize and Rotate extension sends an event when the
               size, rotation, or refresh rate of any screen has changed.
             */
            XRRScreenChangeNotifyEvent *xrr_event =
              (XRRScreenChangeNotifyEvent *) &event;

            if (p->verbose_p)
              {
                /* XRRRootToScreen is in Xrandr.h 1.4, 2001/06/07 */
                int screen = XRRRootToScreen (si->dpy, xrr_event->window);
                fprintf (stderr, "%s: %d: screen change event received\n",
                         blurb(), screen);
              }

# ifdef RRScreenChangeNotifyMask
            /* Inform Xlib that it's ok to update its data structures. */
            XRRUpdateConfiguration (&event); /* Xrandr.h 1.9, 2002/09/29 */
# endif /* RRScreenChangeNotifyMask */

            /* Resize the existing xscreensaver windows and cached ssi data. */
            if (update_screen_layout (si))
              {
                if (p->verbose_p)
                  {
                    fprintf (stderr, "%s: new layout:\n", blurb());
                    describe_monitor_layout (si);
                  }
                resize_screensaver_window (si);
              }
          }
        else
#endif /* HAVE_RANDR */

          /* Just some random event.  Let the Widgets handle it, if desired. */
	  dispatch_event (si, &event);
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
  if (!until_idle_p && si->locked_p)
    swallow_unlock_typeahead_events (si, &event);
  else
    while (XCheckMaskEvent (si->dpy,
                            (KeyPressMask|ButtonPressMask|PointerMotionMask),
                     &event))
      ;


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

  if (until_idle_p && si->cycle_id)	/* no cycle timer when inactive */
    abort ();
}



/* Some crap for dealing with /proc/interrupts.

   On Linux systems, it's possible to see the hardware interrupt count
   associated with the keyboard.  We can therefore use that as another method
   of detecting idleness.

   Why is it a good idea to do this?  Because it lets us detect keyboard
   activity that is not associated with X events.  For example, if the user
   has switched to another virtual console, it's good for xscreensaver to not
   be running graphics hacks on the (non-visible) X display.  The common
   complaint that checking /proc/interrupts addresses is that the user is
   playing Quake on a non-X console, and the GL hacks are perceptibly slowing
   the game...

   This is tricky for a number of reasons.

     * First, we must be sure to only do this when running on an X server that
       is on the local machine (because otherwise, we'd be reacting to the
       wrong keyboard.)  The way we do this is by noting that the $DISPLAY is
       pointing to display 0 on the local machine.  It *could* be that display
       1 is also on the local machine (e.g., two X servers, each on a different
       virtual-terminal) but it's also possible that screen 1 is an X terminal,
       using this machine as the host.  So we can't take that chance.

     * Second, one can only access these interrupt numbers in a completely
       and utterly brain-damaged way.  You would think that one would use an
       ioctl for this.  But no.  The ONLY way to get this information is to
       open the pseudo-file /proc/interrupts AS A FILE, and read the numbers
       out of it TEXTUALLY.  Because this is Unix, and all the world's a file,
       and the only real data type is the short-line sequence of ASCII bytes.

       Now it's all well and good that the /proc/interrupts pseudo-file
       exists; that's a clever idea, and a useful API for things that are
       already textually oriented, like shell scripts, and users doing
       interactive debugging sessions.  But to make a *C PROGRAM* open a file
       and parse the textual representation of integers out of it is just
       insane.

     * Third, you can't just hold the file open, and fseek() back to the
       beginning to get updated data!  If you do that, the data never changes.
       And I don't want to call open() every five seconds, because I don't want
       to risk going to disk for any inodes.  It turns out that if you dup()
       it early, then each copy gets fresh data, so we can get around that in
       this way (but for how many releases, one might wonder?)

     * Fourth, the format of the output of the /proc/interrupts file is
       undocumented, and has changed several times already!  In Linux 2.0.33,
       even on a multiprocessor machine, it looks like this:

          0:  309453991   timer
          1:    4771729   keyboard
   
       but in Linux 2.2 and 2.4 kernels with MP machines, it looks like this:

                   CPU0       CPU1
          0:    1671450    1672618    IO-APIC-edge  timer
          1:      13037      13495    IO-APIC-edge  keyboard

       and in Linux 2.6, it's gotten even goofier: now there are two lines
       labelled "i8042".  One of them is the keyboard, and one of them is
       the PS/2 mouse -- and of course, you can't tell them apart, except
       by wiggling the mouse and noting which one changes:

                   CPU0       CPU1
          1:      32051      30864    IO-APIC-edge  i8042
         12:     476577     479913    IO-APIC-edge  i8042

       Joy!  So how are we expected to parse that?  Well, this code doesn't
       parse it: it saves the first line with the string "keyboard" (or
       "i8042") in it, and does a string-comparison to note when it has
       changed.  If there are two "i8042" lines, we assume the first is
       the keyboard and the second is the mouse (doesn't matter which is
       which, really, as long as we don't compare them against each other.)

   Thanks to Nat Friedman <nat@nat.org> for figuring out most of this crap.

   Note that if you have a serial or USB mouse, or a USB keyboard, it won't
   detect it.  That's because there's no way to tell the difference between a
   serial mouse and a general serial port, and all USB devices look the same
   from here.  It would be somewhat unfortunate to have the screensaver turn
   off when the modem on COM1 burped, or when a USB disk was accessed.
 */


#ifdef HAVE_PROC_INTERRUPTS

#define PROC_INTERRUPTS "/proc/interrupts"

Bool
query_proc_interrupts_available (saver_info *si, const char **why)
{
  /* We can use /proc/interrupts if $DISPLAY points to :0, and if the
     "/proc/interrupts" file exists and is readable.
   */
  FILE *f;
  if (why) *why = 0;

  if (!display_is_on_console_p (si))
    {
      if (why) *why = "not on primary console";
      return False;
    }

  f = fopen (PROC_INTERRUPTS, "r");
  if (!f)
    {
      if (why) *why = "does not exist";
      return False;
    }

  fclose (f);
  return True;
}


static Bool
proc_interrupts_activity_p (saver_info *si)
{
  static FILE *f0 = 0;
  FILE *f1 = 0;
  int fd;
  static char last_kbd_line[255] = { 0, };
  static char last_ptr_line[255] = { 0, };
  char new_line[sizeof(last_kbd_line)];
  Bool checked_kbd = False, kbd_changed = False;
  Bool checked_ptr = False, ptr_changed = False;
  int i8042_count = 0;

  if (!f0)
    {
      /* First time -- open the file. */
      f0 = fopen (PROC_INTERRUPTS, "r");
      if (!f0)
        {
          char buf[255];
          sprintf(buf, "%s: error opening %s", blurb(), PROC_INTERRUPTS);
          perror (buf);
          goto FAIL;
        }
    }

  if (f0 == (FILE *) -1)	    /* means we got an error initializing. */
    return False;

  fd = dup (fileno (f0));
  if (fd < 0)
    {
      char buf[255];
      sprintf(buf, "%s: could not dup() the %s fd", blurb(), PROC_INTERRUPTS);
      perror (buf);
      goto FAIL;
    }

  f1 = fdopen (fd, "r");
  if (!f1)
    {
      char buf[255];
      sprintf(buf, "%s: could not fdopen() the %s fd", blurb(),
              PROC_INTERRUPTS);
      perror (buf);
      goto FAIL;
    }

  /* Actually, I'm unclear on why this fseek() is necessary, given the timing
     of the dup() above, but it is. */
  if (fseek (f1, 0, SEEK_SET) != 0)
    {
      char buf[255];
      sprintf(buf, "%s: error rewinding %s", blurb(), PROC_INTERRUPTS);
      perror (buf);
      goto FAIL;
    }

  /* Now read through the pseudo-file until we find the "keyboard",
     "PS/2 mouse", or "i8042" lines. */

  while (fgets (new_line, sizeof(new_line)-1, f1))
    {
      Bool i8042_p = !!strstr (new_line, "i8042");
      if (i8042_p) i8042_count++;

      if (strchr (new_line, ','))
        {
          /* Ignore any line that has a comma on it: this is because
             a setup like this:

                 12:     930935          XT-PIC  usb-uhci, PS/2 Mouse

             is really bad news.  It *looks* like we can note mouse
             activity from that line, but really, that interrupt gets
             fired any time any USB device has activity!  So we have
             to ignore any shared IRQs.
           */
        }
      else if (!checked_kbd &&
               (strstr (new_line, "keyboard") ||
                (i8042_p && i8042_count == 1)))
        {
          /* Assume the keyboard interrupt is the line that says "keyboard",
             or the *first* line that says "i8042".
           */
          kbd_changed = (*last_kbd_line && !!strcmp (new_line, last_kbd_line));
          strcpy (last_kbd_line, new_line);
          checked_kbd = True;
        }
      else if (!checked_ptr &&
               (strstr (new_line, "PS/2 Mouse") ||
                (i8042_p && i8042_count == 2)))
        {
          /* Assume the mouse interrupt is the line that says "PS/2 mouse",
             or the *second* line that says "i8042".
           */
          ptr_changed = (*last_ptr_line && !!strcmp (new_line, last_ptr_line));
          strcpy (last_ptr_line, new_line);
          checked_ptr = True;
        }

      if (checked_kbd && checked_ptr)
        break;
    }

  if (checked_kbd || checked_ptr)
    {
      fclose (f1);

      if (si->prefs.debug_p && (kbd_changed || ptr_changed))
        fprintf (stderr, "%s: /proc/interrupts activity: %s\n",
                 blurb(),
                 ((kbd_changed && ptr_changed) ? "mouse and kbd" :
                  kbd_changed ? "kbd" :
                  ptr_changed ? "mouse" : "ERR"));

      return (kbd_changed || ptr_changed);
    }


  /* If we got here, we didn't find either a "keyboard" or a "PS/2 Mouse"
     line in the file at all. */
  fprintf (stderr, "%s: no keyboard or mouse data in %s?\n",
           blurb(), PROC_INTERRUPTS);

 FAIL:
  if (f1)
    fclose (f1);

  if (f0 && f0 != (FILE *) -1)
    fclose (f0);

  f0 = (FILE *) -1;
  return False;
}

#endif /* HAVE_PROC_INTERRUPTS */


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
  saver_preferences *p = &si->prefs;

  disable_builtin_screensaver (si, False);

  /* If the DPMS settings on the server have changed, change them back to
     what ~/.xscreensaver says they should be. */
  sync_server_dpms_settings (si->dpy,
                             (p->dpms_enabled_p  &&
                              p->mode != DONT_BLANK),
                             p->dpms_standby / 1000,
                             p->dpms_suspend / 1000,
                             p->dpms_off / 1000,
                             False);

  if (si->screen_blanked_p)
    {
      Bool running_p = screenhack_running_p (si);

      if (si->dbox_up_p)
        {
          if (si->prefs.debug_p)
            fprintf (stderr, "%s: dialog box is up: not raising screen.\n",
                     blurb());
        }
      else
        {
          if (si->prefs.debug_p)
            fprintf (stderr, "%s: watchdog timer raising %sscreen.\n",
                     blurb(), (running_p ? "" : "and clearing "));

          raise_window (si, True, True, running_p);
        }

      if (screenhack_running_p (si) &&
          !monitor_powered_on_p (si))
	{
          int i;
	  if (si->prefs.verbose_p)
	    fprintf (stderr,
		     "%s: X says monitor has powered down; "
		     "killing running hacks.\n", blurb());
          for (i = 0; i < si->nscreens; i++)
            kill_screenhack (&si->screens[i]);
	}

      /* Re-schedule this timer.  The watchdog timer defaults to a bit less
         than the hack cycle period, but is never longer than one hour.
       */
      si->watchdog_id = 0;
      reset_watchdog_timer (si, True);
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

      if (p->debug_p)
	fprintf (stderr, "%s: restarting watchdog_timer (%ld, %ld)\n",
		 blurb(), p->watchdog_timeout, si->watchdog_id);
    }
}


/* It's possible that a race condition could have led to the saver
   window being unexpectedly still mapped.  This can happen like so:

    - screen is blanked
    - hack is launched
    - that hack tries to grab a screen image (it does this by
      first unmapping the saver window, then remapping it.)
    - hack unmaps window
    - hack waits
    - user becomes active
    - hack re-maps window (*)
    - driver kills subprocess
    - driver unmaps window (**)

   The race is that (*) might have been sent to the server before
   the client process was killed, but, due to scheduling randomness,
   might not have been received by the server until after (**).
   In other words, (*) and (**) might happen out of order, meaning
   the driver will unmap the window, and then after that, the
   recently-dead client will re-map it.  This leaves the user
   locked out (it looks like a desktop, but it's not!)

   To avoid this: after un-blanking the screen, we launch a timer
   that wakes up once a second for ten seconds, and makes damned
   sure that the window is still unmapped.
 */

void
de_race_timer (XtPointer closure, XtIntervalId *id)
{
  saver_info *si = (saver_info *) closure;
  saver_preferences *p = &si->prefs;
  int secs = 1;

  if (id == 0)  /* if id is 0, this is the initialization call. */
    {
      si->de_race_ticks = 10;
      if (p->verbose_p)
        fprintf (stderr, "%s: starting de-race timer (%d seconds.)\n",
                 blurb(), si->de_race_ticks);
    }
  else
    {
      int i;
      XSync (si->dpy, False);
      for (i = 0; i < si->nscreens; i++)
        {
          saver_screen_info *ssi = &si->screens[i];
          Window w = ssi->screensaver_window;
          XWindowAttributes xgwa;
          XGetWindowAttributes (si->dpy, w, &xgwa);
          if (xgwa.map_state != IsUnmapped)
            {
              if (p->verbose_p)
                fprintf (stderr,
                         "%s: %d: client race! emergency unmap 0x%lx.\n",
                         blurb(), i, (unsigned long) w);
              XUnmapWindow (si->dpy, w);
            }
          else if (p->debug_p)
            fprintf (stderr, "%s: %d: (de-race of 0x%lx is cool.)\n",
                     blurb(), i, (unsigned long) w);
        }
      XSync (si->dpy, False);

      si->de_race_ticks--;
    }

  if (id && *id == si->de_race_id)
    si->de_race_id = 0;

  if (si->de_race_id) abort();

  if (si->de_race_ticks <= 0)
    {
      si->de_race_id = 0;
      if (p->verbose_p)
        fprintf (stderr, "%s: de-race completed.\n", blurb());
    }
  else
    {
      si->de_race_id = XtAppAddTimeOut (si->app, secs * 1000,
                                        de_race_timer, closure);
    }
}
