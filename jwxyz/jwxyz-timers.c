/* xscreensaver, Copyright (c) 2006-2016 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This is the portable implementation of Xt timers and inputs, for libjwxyz.
 */

#include "config.h"

#ifdef HAVE_JWXYZ /* whole file */


#undef DEBUG_TIMERS
#undef DEBUG_SOURCES

#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>
#include "jwxyz.h"
#include "jwxyz-timers.h"

#ifdef HAVE_ANDROID
extern void Log(const char *format, ...);
#endif

#ifdef HAVE_COCOA
# define Log(S, ...) fprintf(stderr, "xscreensaver: " S "\n", __VA_ARGS__)
#endif

#ifdef DEBUG_TIMERS
# define LOGT(...) Log(__VA_ARGS__)
#else
# define LOGT(...)
#endif

#ifdef DEBUG_SOURCES
# define LOGI(...) Log(__VA_ARGS__)
#else
# define LOGI(...)
#endif

#define ASSERT_RET(C,S) do {                    \
    if (!(C)) {                                 \
      jwxyz_abort ("jwxyz-timers: %s",(S));     \
      return;                                   \
 }} while(0)

#define ASSERT_RET0(C,S) do {                   \
    if (!(C)) {                                 \
      jwxyz_abort ("jwxyz-timers: %s",(S));     \
      return 0;                                 \
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
  XtIntervalId all_timers;
};

struct jwxyz_XtIntervalId {
  XtAppContext app;
  int refcount;

  double run_at;
  XtTimerCallbackProc cb;
  XtPointer closure;

  XtIntervalId next;
};

struct jwxyz_XtInputId {
  XtAppContext app;
  int refcount;

  XtInputCallbackProc cb;
  XtPointer closure;
  int fd;
};


static double
double_time (void)
{
  struct timeval now;
# ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone tzp;
  gettimeofday(&now, &tzp);
# else
  gettimeofday(&now);
# endif

  return (now.tv_sec + ((double) now.tv_usec * 0.000001));
}


jwxyz_sources_data *
jwxyz_sources_init (XtAppContext app)
{
  jwxyz_sources_data *td = (jwxyz_sources_data *) calloc (1, sizeof (*td));
  return td;
}

XtIntervalId
XtAppAddTimeOut (XtAppContext app, unsigned long msecs,
                 XtTimerCallbackProc cb, XtPointer closure)
{
  jwxyz_sources_data *td = display_sources_data (app_to_display (app));
  XtIntervalId data = (XtIntervalId) calloc (1, sizeof(*data));
  double now = double_time();
  data->app = app;
  data->cb = cb;
  data->closure = closure;
  data->refcount++;
  data->run_at = now + (msecs / 1000.0);

  data->next = td->all_timers;
  td->all_timers = data;

  LOGT("timer  0x%08lX: alloc %lu %.2f", (unsigned long) data, msecs,
       data->run_at - now);

  return data;
}


/* This is called both by the user to manually kill a timer,
   and by the run loop after a timer has fired.
 */
void
XtRemoveTimeOut (XtIntervalId data)
{
  jwxyz_sources_data *td = display_sources_data (app_to_display (data->app));

  LOGT("timer  0x%08lX: remove", (unsigned long) data);
  ASSERT_RET (data->refcount > 0, "already freed");

  data->refcount--;
  LOGT("timer  0x%08lX: release %d", (unsigned long) data, data->refcount);
  ASSERT_RET (data->refcount >= 0, "double free");

  if (data->refcount == 0) {

    /* Remove it from the list of live timers. */
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


XtInputId
XtAppAddInput (XtAppContext app, int fd, XtPointer flags,
               XtInputCallbackProc cb, XtPointer closure)
{
  jwxyz_sources_data *td = display_sources_data (app_to_display (app));
  XtInputId data = (XtInputId) calloc (1, sizeof(*data));
  data->cb = cb;
  data->fd = fd;
  data->closure = closure;
  data->app = app;
  data->refcount++;

  LOGI("source 0x%08lX %2d: alloc", (unsigned long) data, data->fd);

  ASSERT_RET0 (fd > 0 && fd < FD_SETSIZE, "fd out of range");
  ASSERT_RET0 (td->ids[fd] == 0, "sources corrupted");
  td->ids[fd] = data;
  td->fd_count++;

  return data;
}


void
XtRemoveInput (XtInputId id)
{
  jwxyz_sources_data *td = display_sources_data (app_to_display (id->app));

  LOGI("source 0x%08lX %2d: remove", (unsigned long) id, id->fd);
  ASSERT_RET (id->refcount > 0, "sources corrupted");
  ASSERT_RET (td->fd_count > 0, "sources corrupted");
  ASSERT_RET (id->fd > 0 && id->fd < FD_SETSIZE, "fd out of range");
  ASSERT_RET (td->ids[id->fd] == id, "sources corrupted");

  td->ids[id->fd] = 0;
  td->fd_count--;
  id->refcount--;

  LOGI("source 0x%08lX %2d: release %d", (unsigned long) id, id->fd,
       id->refcount);
  ASSERT_RET (id->refcount >= 0, "double free");
  if (id->refcount == 0) {
    memset (id, 0xA1, sizeof(*id));
    id->fd = -666;
    free (id);
  }
}


static void
jwxyz_timers_run (jwxyz_sources_data *td)
{
  /* Iterate the timer list, being careful because XtRemoveTimeOut removes
     the current item from that list. */
  if (td->all_timers) {
    XtIntervalId timer, next;
    double now = double_time();
    int count = 0;

    for (timer = td->all_timers, next = timer->next;
         timer;
         timer = next, next = (timer ? timer->next : 0)) {
      if (timer->run_at <= now) {
        LOGT("timer  0x%08lX: fire %.02f", (unsigned long) timer,
             now - timer->run_at);
        timer->cb (timer->closure, &timer);
        XtRemoveTimeOut (timer);
        count++;
        ASSERT_RET (count < 10000, "way too many timers to run");
      }
    }
  }
}


static void
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
  XtIntervalId timer, next;
  int count = 0;

  /* Iterate the timer list, being careful because XtRemoveTimeOut removes
     the current item from that list. */
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
  if (mask & XtIMAlternateInput)
    jwxyz_sources_run (td);
  if (mask & XtIMTimer)
    jwxyz_timers_run (td);
}

#endif /* HAVE_JWXYZ */
