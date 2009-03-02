/* xscreensaver, Copyright (c) 2006 Jamie Zawinski <jwz@jwz.org>
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

#import <stdlib.h>
#import <Cocoa/Cocoa.h>
#import "jwxyz.h"
#import "jwxyz-timers.h"

//#define DEBUG_TIMERS
//#define DEBUG_SOURCES

/* If this is defined, we implement timers in terms of CFRunLoopTimerCreate.
   But I couldn't get that to work right: when the left window was accepting
   input, the right window would starve.  So I implemented them in terms of
   select() instead.  This is more efficient anyway, since instead of the
   event loop telling us that input is ready constantly, we only check once
   just before the draw method is called.
 */
#undef USE_COCOA_SOURCES


#ifdef DEBUG_TIMERS
# define LOGT(str,arg1,arg2) NSLog(str,arg1,arg2)
#else
# define LOGT(str,arg1,arg2)
#endif

#ifdef DEBUG_SOURCES
# define LOGI(str,arg1,arg2,arg3) NSLog(str,arg1,arg2,arg3)
#else
# define LOGI(str,arg1,arg2,arg3)
#endif


XtAppContext
XtDisplayToApplicationContext (Display *dpy)
{
  return (XtAppContext) dpy;
}

#define app_to_display(APP) ((Display *) (APP))


struct jwxyz_XtIntervalId {
  CFRunLoopTimerRef cftimer;
  int refcount;

  XtTimerCallbackProc cb;
  XtPointer closure;
};

struct jwxyz_XtInputId {
# ifdef USE_COCOA_SOURCES
  CFRunLoopSourceRef cfsource;
  CFSocketRef socket;
# else
  XtAppContext app;
# endif
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
  LOGT(@"timer  0x%08X: retain %d", (unsigned int) data, data->refcount);
  return arg;
}

static void
jwxyz_timer_release (const void *arg)
{
  struct jwxyz_XtIntervalId *data = (struct jwxyz_XtIntervalId *) arg;
  data->refcount--;
  LOGT(@"timer  0x%08X: release %d", (unsigned int) data, data->refcount);
  if (data->refcount < 0) abort();
  if (data->refcount == 0) free (data);
}

static const void *
jwxyz_source_retain (const void *arg)
{
  struct jwxyz_XtInputId *data = (struct jwxyz_XtInputId *) arg;
  data->refcount++;
  LOGI(@"source 0x%08X %2d: retain %d", (unsigned int) data, data->fd, 
       data->refcount);
  return arg;
}

static void
jwxyz_source_release (const void *arg)
{
  struct jwxyz_XtInputId *data = (struct jwxyz_XtInputId *) arg;
  data->refcount--;
  LOGI(@"source 0x%08X %2d: release %d", (unsigned int) data, data->fd,
       data->refcount);
  if (data->refcount < 0) abort();
  if (data->refcount == 0) {
# ifdef USE_COCOA_SOURCES
    if (data->socket)
      CFRelease (data->socket);
    if (data->cfsource)
      CFRelease (data->cfsource);
# endif /* USE_COCOA_SOURCES */
    memset (data, 0xA1, sizeof(*data));
    data->fd = -666;
    free (data);
  }
}


static void
jwxyz_timer_cb (CFRunLoopTimerRef timer, void *arg)
{
  struct jwxyz_XtIntervalId *data = (struct jwxyz_XtIntervalId *) arg;
  LOGT(@"timer  0x%08X: fire", (unsigned int) data, 0);
  data->cb (data->closure, &data);

  // Our caller (__CFRunLoopDoTimer) will now call CFRunLoopTimerInvalidate,
  // which will call jwxyz_timer_release.
}


#ifdef USE_COCOA_SOURCES

/* whether there is data available to be read on the file descriptor
 */
static int
input_available_p (int fd)
{
  struct timeval tv = { 0, };
  fd_set fds;
  FD_ZERO (&fds);
  FD_SET (fd, &fds);
  return select (fd+1, &fds, NULL, NULL, &tv);
}


static void
jwxyz_source_cb (CFSocketRef s, CFSocketCallBackType type,
                 CFDataRef address, const void *call_data, void *info)
{
  struct jwxyz_XtInputId *data = (struct jwxyz_XtInputId *) info;

  if (type != kCFSocketReadCallBack) abort();
  if (call_data != 0) abort();  // not used for kCFSocketRead

  // We are sometimes called when there is not, in fact, data available!
  // So don't call data->cb if we're being fed a pack of lies.
  //
  if (! input_available_p (data->fd)) {
    LOGI(@"source 0x%08X %2d: false alarm!", (unsigned int) data, data->fd, 0);
    return;
  }

  LOGI(@"source 0x%08X %2d: fire", (unsigned int) data, data->fd, 0);

  data->cb (data->closure, &data->fd, &data);
}

#endif /* USE_COCOA_SOURCES */


XtIntervalId
XtAppAddTimeOut (XtAppContext app, unsigned long msecs,
                 XtTimerCallbackProc cb, XtPointer closure)
{
  struct jwxyz_XtIntervalId *data = (struct jwxyz_XtIntervalId *)
    calloc (1, sizeof(*data));
  data->cb = cb;
  data->closure = closure;

  LOGT(@"timer  0x%08X: alloc %d", (unsigned int) data, msecs);
  
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

  CFRunLoopAddTimer (CFRunLoopGetCurrent(), data->cftimer,
                     kCFRunLoopCommonModes);
  return data;
}


void
XtRemoveTimeOut (XtIntervalId id)
{
  LOGT(@"timer  0x%08X: remove", (unsigned int) id, 0);
  if (id->refcount <= 0) abort();
  if (!id->cftimer) abort();

  CFRunLoopRemoveTimer (CFRunLoopGetCurrent(), id->cftimer,
                        kCFRunLoopCommonModes);
  CFRunLoopTimerInvalidate (id->cftimer);
  // CFRunLoopTimerInvalidate called jwxyz_timer_release.
}


#ifndef USE_COCOA_SOURCES

struct jwxyz_sources_data {
  int count;
  XtInputId ids[FD_SETSIZE];
};

jwxyz_sources_data *
jwxyz_sources_init (XtAppContext app)
{
  jwxyz_sources_data *td = (jwxyz_sources_data *) calloc (1, sizeof (*td));
  return td;
}

void
jwxyz_sources_free (jwxyz_sources_data *td)
{
  free (td);
}


static void
jwxyz_source_select (XtInputId id)
{
  jwxyz_sources_data *td = display_sources_data (app_to_display (id->app));
  if (id->fd <= 0 || id->fd >= FD_SETSIZE) abort();
  if (td->ids[id->fd]) abort();
  td->ids[id->fd] = id;
  td->count++;
}

static void
jwxyz_source_deselect (XtInputId id)
{
  jwxyz_sources_data *td = display_sources_data (app_to_display (id->app));
  if (td->count <= 0) abort();
  if (id->fd <= 0 || id->fd >= FD_SETSIZE) abort();
  if (td->ids[id->fd] != id) abort();
  td->ids[id->fd] = 0;
  td->count--;
}

void
jwxyz_sources_run (jwxyz_sources_data *td)
{
  if (td->count == 0) return;

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

  if (!max) abort();

  if (0 < select (max+1, &fds, NULL, NULL, &tv)) {
    for (i = 0; i < FD_SETSIZE; i++) {
      if (FD_ISSET (i, &fds)) {
        XtInputId id = td->ids[i];
        if (!id || !id->cb) abort();
        if (id->fd != i) abort();
        id->cb (id->closure, &id->fd, &id);
      }
    }
  }
}

#endif /* !USE_COCOA_SOURCES */


XtInputId
XtAppAddInput (XtAppContext app, int fd, XtPointer flags,
               XtInputCallbackProc cb, XtPointer closure)
{
  struct jwxyz_XtInputId *data = (struct jwxyz_XtInputId *)
    calloc (1, sizeof(*data));
  data->cb = cb;
  data->fd = fd;
  data->closure = closure;

  LOGI(@"source 0x%08X %2d: alloc", (unsigned int) data, data->fd, 0);

# ifdef USE_COCOA_SOURCES

  CFSocketContext ctx = { 0, };
  ctx.info    = data;
  ctx.retain  = jwxyz_source_retain;
  ctx.release = jwxyz_source_release;

  data->socket = CFSocketCreateWithNative (NULL, fd, kCFSocketReadCallBack,
                                           jwxyz_source_cb, &ctx);
  // CFSocketCreateWithNative called jwxyz_source_retain.
  
  CFSocketSetSocketFlags (data->socket,
                          kCFSocketAutomaticallyReenableReadCallBack
                          );
  // not kCFSocketCloseOnInvalidate.

  data->cfsource = CFSocketCreateRunLoopSource (NULL, data->socket, 0);
  
  CFRunLoopAddSource (CFRunLoopGetCurrent(), data->cfsource,
                     kCFRunLoopCommonModes);
  
# else  /* !USE_COCOA_SOURCES */

  data->app = app;
  jwxyz_source_retain (data);
  jwxyz_source_select (data);

# endif /* !USE_COCOA_SOURCES */

  return data;
}

void
XtRemoveInput (XtInputId id)
{
  LOGI(@"source 0x%08X %2d: remove", (unsigned int) id, id->fd, 0);
  if (id->refcount <= 0) abort();
# ifdef USE_COCOA_SOURCES
  if (! id->cfsource) abort();
  if (! id->socket) abort();

  CFRunLoopRemoveSource (CFRunLoopGetCurrent(), id->cfsource,
                         kCFRunLoopCommonModes);
  CFSocketInvalidate (id->socket);
  // CFSocketInvalidate called jwxyz_source_release.

# else  /* !USE_COCOA_SOURCES */

  jwxyz_source_deselect (id);
  jwxyz_source_release (id);

# endif /* !USE_COCOA_SOURCES */
}

void
jwxyz_XtRemoveInput_all (Display *dpy)
{
# ifdef USE_COCOA_SOURCES
  abort();
# else  /* !USE_COCOA_SOURCES */

  jwxyz_sources_data *td = display_sources_data (dpy);
  int i;
  for (i = 0; i < FD_SETSIZE; i++) {
    XtInputId id = td->ids[i];
    if (id) XtRemoveInput (id);
  }

# endif /* !USE_COCOA_SOURCES */
}
