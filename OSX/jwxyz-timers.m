/* xscreensaver, Copyright (c) 2006-2014 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This is the OSX implementation of Xt timers, for libjwxyz.
 */

//#define DEBUG_TIMERS
//#define DEBUG_SOURCES

#import <stdlib.h>

#import "jwxyz.h"
#import "jwxyz-timers.h"



#ifdef DEBUG_TIMERS
# define LOGT( str,arg1)      NSLog(str,arg1)
# define LOGT2(str,arg1,arg2) NSLog(str,arg1,arg2)
#else
# define LOGT( str,arg1)
# define LOGT2(str,arg1,arg2)
#endif

#ifdef DEBUG_SOURCES
# define LOGI( str,arg1,arg2)      NSLog(str,arg1,arg2)
# define LOGI2(str,arg1,arg2,arg3) NSLog(str,arg1,arg2,arg3)
#else
# define LOGI( str,arg1,arg2)
# define LOGI2(str,arg1,arg2,arg3)
#endif

#define ASSERT_RET(C,S) do {                    \
    if (!(C)) {                                 \
      jwxyz_abort ("jwxyz-timers: %s",(S));     \
      return;                                   \
 }} while(0)


XtAppContext
XtDisplayToApplicationContext (Display *dpy)
{
  return (XtAppContext) dpy;
}

#define app_to_display(APP) ((Display *) (APP))


struct jwxyz_sources_data {
  int fd_count;
  XtInputId ids[FD_SETSIZE];
  struct jwxyz_XtIntervalId *all_timers;
};

struct jwxyz_XtIntervalId {
  XtAppContext app;
  CFRunLoopTimerRef cftimer;
  int refcount;

  XtTimerCallbackProc cb;
  XtPointer closure;

  struct jwxyz_XtIntervalId *next;
};

struct jwxyz_XtInputId {
  XtAppContext app;
  int refcount;

  XtInputCallbackProc cb;
  XtPointer closure;
  int fd;
};


static const void *
jwxyz_timer_retain (const void *arg)
{
  struct jwxyz_XtIntervalId *data = (struct jwxyz_XtIntervalId *) arg;
  data->refcount++;
  LOGT2(@"timer  0x%08X: retain %d", (unsigned int) data, data->refcount);
  return arg;
}

/* This is called both by the user to manually kill a timer (XtRemoveTimeOut)
   and by the run loop after a timer has fired (CFRunLoopTimerInvalidate).
 */
static void
jwxyz_timer_release (const void *arg)
{
  struct jwxyz_XtIntervalId *data = (struct jwxyz_XtIntervalId *) arg;
  jwxyz_sources_data *td = display_sources_data (app_to_display (data->app));

  data->refcount--;
  LOGT2(@"timer  0x%08X: release %d", (unsigned int) data, data->refcount);
  ASSERT_RET (data->refcount >= 0, "double free");

  if (data->refcount == 0) {

    // Remove it from the list of live timers.
    XtIntervalId prev, timer;
    int hit = 0;
    for (timer = td->all_timers, prev = 0;
         timer;
         prev = timer, timer = timer->next) {
      if (timer == data) {
        ASSERT_RET (!hit, "circular timer list");
        if (prev)
          prev->next = timer->next;
        else
          td->all_timers = timer->next;
        timer->next = 0;
        hit = 1;
      } else {
        ASSERT_RET (timer->refcount > 0, "timer list corrupted");
      }
    }

    free (data);
  }
}

static const void *
jwxyz_source_retain (const void *arg)
{
  struct jwxyz_XtInputId *data = (struct jwxyz_XtInputId *) arg;
  data->refcount++;
  LOGI2(@"source 0x%08X %2d: retain %d", (unsigned int) data, data->fd, 
        data->refcount);
  return arg;
}

static void
jwxyz_source_release (const void *arg)
{
  struct jwxyz_XtInputId *data = (struct jwxyz_XtInputId *) arg;
  data->refcount--;
  LOGI2(@"source 0x%08X %2d: release %d", (unsigned int) data, data->fd,
        data->refcount);
  ASSERT_RET (data->refcount >= 0, "double free");
  if (data->refcount == 0) {
    memset (data, 0xA1, sizeof(*data));
    data->fd = -666;
    free (data);
  }
}


static void
jwxyz_timer_cb (CFRunLoopTimerRef timer, void *arg)
{
  struct jwxyz_XtIntervalId *data = (struct jwxyz_XtIntervalId *) arg;
  LOGT(@"timer  0x%08X: fire", (unsigned int) data);
  data->cb (data->closure, &data);

  // Our caller (__CFRunLoopDoTimer) will now call CFRunLoopTimerInvalidate,
  // which will call jwxyz_timer_release.
}


XtIntervalId
XtAppAddTimeOut (XtAppContext app, unsigned long msecs,
                 XtTimerCallbackProc cb, XtPointer closure)
{
  jwxyz_sources_data *td = display_sources_data (app_to_display (app));
  struct jwxyz_XtIntervalId *data = (struct jwxyz_XtIntervalId *)
    calloc (1, sizeof(*data));
  data->app = app;
  data->cb = cb;
  data->closure = closure;

  LOGT2(@"timer  0x%08X: alloc %lu", (unsigned int) data, msecs);
  
  CFRunLoopTimerContext ctx = { 0, };
  ctx.info    = data;
  ctx.retain  = jwxyz_timer_retain;
  ctx.release = jwxyz_timer_release;

  CFAbsoluteTime time = CFAbsoluteTimeGetCurrent() + (msecs / 1000.0);

  data->cftimer =
    CFRunLoopTimerCreate (NULL, // allocator
                          time, 0, 0, 0, // interval, flags, order
                          jwxyz_timer_cb, &ctx);
  // CFRunLoopTimerCreate called jwxyz_timer_retain.

  data->next = td->all_timers;
  td->all_timers = data;

  CFRunLoopAddTimer (CFRunLoopGetCurrent(), data->cftimer,
                     kCFRunLoopCommonModes);
  return data;
}


void
XtRemoveTimeOut (XtIntervalId id)
{
  LOGT(@"timer  0x%08X: remove", (unsigned int) id);
  ASSERT_RET (id->refcount > 0, "already freed");
  ASSERT_RET (id->cftimer, "timers corrupted");

  CFRunLoopRemoveTimer (CFRunLoopGetCurrent(), id->cftimer,
                        kCFRunLoopCommonModes);
  CFRunLoopTimerInvalidate (id->cftimer);
  // CFRunLoopTimerInvalidate called jwxyz_timer_release.
}


jwxyz_sources_data *
jwxyz_sources_init (XtAppContext app)
{
  jwxyz_sources_data *td = (jwxyz_sources_data *) calloc (1, sizeof (*td));
  return td;
}

static void
jwxyz_source_select (XtInputId id)
{
  jwxyz_sources_data *td = display_sources_data (app_to_display (id->app));
  ASSERT_RET (id->fd > 0 && id->fd < FD_SETSIZE, "fd out of range");
  ASSERT_RET (td->ids[id->fd] == 0, "sources corrupted");
  td->ids[id->fd] = id;
  td->fd_count++;
}

static void
jwxyz_source_deselect (XtInputId id)
{
  jwxyz_sources_data *td = display_sources_data (app_to_display (id->app));
  ASSERT_RET (td->fd_count > 0, "sources corrupted");
  ASSERT_RET (id->fd > 0 && id->fd < FD_SETSIZE, "fd out of range");
  ASSERT_RET (td->ids[id->fd] == id, "sources corrupted");
  td->ids[id->fd] = 0;
  td->fd_count--;
}

void
jwxyz_sources_run (jwxyz_sources_data *td)
{
  if (td->fd_count == 0) return;

  struct timeval tv = { 0, };
  fd_set fds;
  int i;
  int max = 0;

  FD_ZERO (&fds);
  for (i = 0; i < FD_SETSIZE; i++) {
    if (td->ids[i]) {
      FD_SET (i, &fds);
      max = i;
    }
  }

  ASSERT_RET (max > 0, "no fds");

  if (0 < select (max+1, &fds, NULL, NULL, &tv)) {
    for (i = 0; i < FD_SETSIZE; i++) {
      if (FD_ISSET (i, &fds)) {
        XtInputId id = td->ids[i];
        ASSERT_RET (id && id->cb, "sources corrupted");
        ASSERT_RET (id->fd == i, "sources corrupted");
        id->cb (id->closure, &id->fd, &id);
      }
    }
  }
}


XtInputId
XtAppAddInput (XtAppContext app, int fd, XtPointer flags,
               XtInputCallbackProc cb, XtPointer closure)
{
  struct jwxyz_XtInputId *data = (struct jwxyz_XtInputId *)
    calloc (1, sizeof(*data));
  data->cb = cb;
  data->fd = fd;
  data->closure = closure;

  LOGI(@"source 0x%08X %2d: alloc", (unsigned int) data, data->fd);

  data->app = app;
  jwxyz_source_retain (data);
  jwxyz_source_select (data);

  return data;
}

void
XtRemoveInput (XtInputId id)
{
  LOGI(@"source 0x%08X %2d: remove", (unsigned int) id, id->fd);
  ASSERT_RET (id->refcount > 0, "sources corrupted");

  jwxyz_source_deselect (id);
  jwxyz_source_release (id);
}

static void
jwxyz_XtRemoveInput_all (jwxyz_sources_data *td)
{
  int i;
  for (i = 0; i < FD_SETSIZE; i++) {
    XtInputId id = td->ids[i];
    if (id) XtRemoveInput (id);
  }
}


static void
jwxyz_XtRemoveTimeOut_all (jwxyz_sources_data *td)
{
  struct jwxyz_XtIntervalId *timer, *next;
  int count = 0;

  // Iterate the timer list, being careful that XtRemoveTimeOut removes
  // things from that list.
  if (td->all_timers) {
    for (timer = td->all_timers, next = timer->next;
         timer;
         timer = next, next = (timer ? timer->next : 0)) {
      XtRemoveTimeOut (timer);
      count++;
      ASSERT_RET (count < 10000, "way too many timers to free");
    }
    ASSERT_RET (!td->all_timers, "timer list didn't empty");
  }
}


void
jwxyz_sources_free (jwxyz_sources_data *td)
{
  jwxyz_XtRemoveInput_all (td);
  jwxyz_XtRemoveTimeOut_all (td);
  memset (td, 0xA1, sizeof(*td));
  free (td);
}


XtInputMask
XtAppPending (XtAppContext app)
{
  return XtIMAlternateInput;  /* just always say yes */
}

void
XtAppProcessEvent (XtAppContext app, XtInputMask mask)
{
  jwxyz_sources_data *td = display_sources_data (app_to_display (app));
  jwxyz_sources_run (td);
}
