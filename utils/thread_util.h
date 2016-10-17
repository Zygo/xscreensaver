/* -*- mode: c; tab-width: 4; fill-column: 78 -*- */
/* vi: set ts=4 tw=78: */

/*
thread_util.h, Copyright (c) 2014 Dave Odell <dmo2118@gmail.com>

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.  No representations are made about the suitability of this
software for any purpose.  It is provided "as is" without express or
implied warranty.
*/

#ifndef THREAD_UTIL_H
#define THREAD_UTIL_H

/* thread_util.h because C11 took threads.h. */

/* And POSIX threads because there aren't too many systems that support C11
   threads that don't already support POSIX threads.
   ...Not that it would be too hard to convert from the one to the other.
   Or to have both.
 */

/* Beware!
   Multithreading is a great way to add insidious and catastrophic bugs to
   a program. Make sure you understand the risks.

   You may wish to become familiar with race conditions, deadlocks, mutexes,
   condition variables, and, in lock-free code, memory ordering, cache
   hierarchies, etc., before working with threads.

   On the other hand, if a screenhack locks up or crashes, it's not the
   end of the world: XScreenSaver won't unlock the screen if that happens.
*/

/*
   The basic stragegy for applying threads to a CPU-hungry screenhack:

   1. Find the CPU-hungry part of the hack.

   2. Change that part so the workload can be divided into N equal-sized
      loads, where N is the number of CPU cores in the machine.
      (For example: with two cores, one core could render even scan lines,
      and the other odd scan lines.)

   2a. Keeping in mind that two threads should not write to the same memory
       at the same time. Specifically, they should not be writing to the
       same cache line at the same time -- so align memory allocation and
       memory accesses to the system cache line size as necessary.

   3. On screenhack_init, create a threadpool object. This creates N worker
      threads, and each thread creates and owns a user-defined struct.
      After creation, the threads are idle.

   4. On screenhack_frame, call threadpool_run(). Each thread simultaneously
      wakes up, calls a function that does one of the equal-sized loads,
      then goes back to sleep. The main thread then calls threadpool_wait(),
      which returns once all the worker threads have finished.

      Using this to implement SMP won't necessarily increase performance by
      a factor of N (again, N is CPU cores.). Both X11 and Cocoa on OS X can
      impose a not-insignificant amount of overhead even when simply blitting
      full-screen XImages @ 30 FPS.

      On systems with simultaneous multithreading (a.k.a. Hyper-threading),
      performance gains may be slim to non-existant.
 */

#include "aligned_malloc.h"

#if HAVE_CONFIG_H
/* For HAVE_PTHREAD. */
#	include "config.h"
#endif

#include <stddef.h>

#if HAVE_UNISTD_H
/* For _POSIX_THREADS. */
#	include <unistd.h>
#endif

#if defined HAVE_JWXYZ
#	include "jwxyz.h"
#else
#	include <X11/Xlib.h>
#endif

#if HAVE_PTHREAD
int threads_available(Display *dpy);
#else
#	define threads_available(dpy) (-1)
#endif
/* > 0: Threads are available. This is normally _POSIX_VERSION.
    -1: Threads are not available.
*/

unsigned hardware_concurrency(Display *dpy);
/* This is supposed to return the number of available CPU cores. This number
   isn't necessarily constant: a system administrator can hotplug or
   enable/disable CPUs on certain systems, or the system can deactivate a
   malfunctioning core -- but these are rare.

   If threads are unavailable, this function will return 1.

   This function isn't fast; the result should be cached.
*/

unsigned thread_memory_alignment(Display *dpy);

/* Returns the proper alignment for memory allocated by a thread that is
   shared with other threads.

   A typical CPU accesses the system RAM through a cache, and this cache is
   divided up into cache lines - aligned chunks of memory typically 32 or 64
   bytes in size. Cache faults cause cache lines to be populated from
   memory. And, in a multiprocessing environment, two CPU cores can access the
   same cache line. The consequences of this depend on the CPU model:

   - x86 implements the MESI protocol [1] to maintain cache coherency between
     CPU cores, with a serious performance penalty on both Intel [1] and AMD
     [2].  Intel uses the term "false sharing" to describe two CPU cores
     accessing different memory in the same cache line.

   - ARM allows CPU caches to become inconsistent in this case [3]. Memory
     fences are needed to prevent horrible non-deterministic bugs from
     occurring.  Other CPU architectures have similar behavior to one of the
     above, depending on whether they are "strongly-orderered" (like x86), or
     "weakly-ordered" (like ARM).

   Aligning multithreaded memory accesses according to the cache line size
   neatly sidesteps both issues.

   One complication is that CPU caches are divided up into separate levels,
   and occasionally different levels can have different cache line sizes, so
   to be safe this function returns the largest cache line size among all
   levels.

   If multithreading is not in effect, this returns sizeof(void *), because
   posix_memalign(3) will error out if the alignment is set to be smaller than
   that.

   [1] Intel(R) 64 and IA-32 Architectures Optimization Reference Manual
      (Order Number: 248966-026): 2.1.5 Cache Hierarchy
   [2] Software Optimization Guide for AMD Family 10h Processors (Publication
       #40546): 11.3.4 Data Sharing between Caches
   [3] http://wanderingcoder.net/2011/04/01/arm-memory-ordering/
*/

/*
   Note: aligned_malloc uses posix_memalign(3) when available, or malloc(3)
   otherwise. As of SUSv2 (1997), and *probably* earlier, these are guaranteed
   to be thread-safe. C89 does not discuss threads, or thread safety;
   non-POSIX systems, watch out!
   http://pubs.opengroup.org/onlinepubs/7908799/xsh/threads.html
   http://pubs.opengroup.org/onlinepubs/009695399/functions/xsh_chap02_09.html
*/

/* int thread_malloc(void **ptr, Display *dpy, unsigned size); */
#define thread_malloc(ptr, dpy, size) \
  (aligned_malloc((ptr), thread_memory_alignment(dpy), (size)))

/*
   This simply does a malloc aligned to thread_memory_alignment(). See
   above. On failure, an errno is returned, usually ENOMEM.

   It's possible for two malloc()'d blocks to at least partially share the
   same cache line. When a different thread is writing to each block, then bad
   things can happen (see thread_memory_alignment). Better malloc()
   implementations will divide memory into pools belonging to one thread or
   another, causing memory blocks belonging to different threads to typically
   be located on different memory pages (see getpagesize(2)), mitigating the
   problem in question...but there's nothing stopping threads from passing
   memory to each other. And it's not practical for the system to align each
   block to 64 or 128 byte boundaries -- it's not uncommon to need lots and
   lots of 8-32 byte allocations, and the waste could become a bit excessive.

   Some rules of thumb to take away from this:

   1. Use thread_alloc for memory that might be written to by a thread that
   didn't originally allocate the object.

   2. Use thread_alloc for memory that will be handed from one thread to
   another.

   3. Use malloc if a single thread allocates, reads from, writes to, and
   frees the block of memory.

   Oddly, I (Dave) have not seen this problem described anywhere else.
*/

#define thread_free(ptr) aligned_free(ptr)

#if HAVE_PTHREAD
#	if defined _POSIX_THREADS && _POSIX_THREADS >= 0
/*
   See The Open Group Base Specifications Issue 7, <unistd.h>, Constants for
   Options and Option Groups
   http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/unistd.h.html#tag_13_77_03_02
*/

#		include <pthread.h>

/* Most PThread synchronization functions only fail when they are misused. */
#		if defined NDEBUG
#			define PTHREAD_VERIFY(expr) (void)(expr)
#		else
#			include <assert.h>
#			define PTHREAD_VERIFY(expr) assert(!(expr))
#		endif

extern const pthread_mutex_t mutex_initializer;
extern const pthread_cond_t cond_initializer;

#	else
		/* Whatever caused HAVE_PTHREAD to be defined (configure script,
           usually) made a mistake if this is reached. */
		/* Maybe this should be a warning. */
#		error HAVE_PTHREAD is defined, but _POSIX_THREADS is not.
		/* #undef HAVE_PTHREAD */
#	endif
#endif

struct threadpool
{
/*	This is always the same as the count parameter fed to threadpool_create().
	Here's a neat trick: if the threadpool is zeroed out with a memset, and
	threadpool_create() is never called to create 0 threads, then
	threadpool::count can be used to determine if the threadpool object was
	ever initialized. */
	unsigned count;

	/* Copied from threadpool_class. No need for thread_create here, though. */
	size_t thread_size;
	void (*thread_run)(void *self);
	void (*thread_destroy)(void *self);

	void *serial_threads;

#if HAVE_PTHREAD
	pthread_mutex_t mutex;
	pthread_cond_t cond;

	/* Number of threads waiting for the startup signal. */
	unsigned parallel_pending;

	/* Number of threads still running. During startup, this is the index of the thread currently being initialized. */
	unsigned parallel_unfinished;

	pthread_t *parallel_threads;
#endif
};

/*
   The threadpool_* functions manage a group of threads (naturally).  Each
   thread owns an object described by a threadpool_class. When
   threadpool_run() is called, the specified func parameter is called on each
   thread in parallel. Sometime after calling threadpool_run(), call
   threadpool_wait(), which waits for each thread to return from
   threadpool_class::run().

   Note that thread 0 runs on the thread from which threadpool_run is called
   from, so if each thread has an equal workload, then when threadpool_run
   returns, the other threads will be finished or almost finished. Adding code
   between threadpool_run and threadpool_wait increases the odds that
   threadpool_wait won't actually have to wait at all -- which is nice.

   If the system does not provide threads, then these functions will fake it:
   everything will appear to work normally from the perspective of the caller,
   but when threadpool_run() is called, the "threads" are run synchronously;
   threadpool_wait() does nothing.
*/

struct threadpool_class
{
	/* Size of the thread private object. */
	size_t size;

/*	Create the thread private object. Called in sequence for each thread
	(effectively) from threadpool_create.
    self: A pointer to size bytes of memory, allocated to hold the thread
          object.
    pool: The threadpool object that owns all the threads. If the threadpool
          is nested in another struct, try GET_PARENT_OBJ.
    id:   The ID for the thread; numbering starts at zero and goes up by one
          for each thread.
    Return 0 on success. On failure, return a value from errno.h; this will
    be returned from threadpool_create. */
	int (*create)(void *self, struct threadpool *pool, unsigned id);

/*	Destroys the thread private object. Called in sequence (though not always
	the same sequence as create).  Warning: During shutdown, it is possible
	for destroy() to be called while other threads are still in
	threadpool_run(). */
	void (*destroy)(void *self);
};

/* Returns 0 on success, on failure can return ENOMEM, or any error code from
   threadpool_class.create. */
int threadpool_create(struct threadpool *self, const struct threadpool_class *cls, Display *dpy, unsigned count);
void threadpool_destroy(struct threadpool *self);

void threadpool_run(struct threadpool *self, void (*func)(void *));
void threadpool_wait(struct threadpool *self);

/*
   io_thread is meant to wrap blocking I/O operations in a one-shot worker
   thread, with cancel semantics.

   Unlike threadpool_*, io_thread will not 'fake it'; it is up to the caller
   to figure out what to do if the system doesn't have threads. In
   particular, the start_routine passed to io_thread_create will never be
   called.

   Clients of io_thread implement four functions:
   - state *process_start(...);
     Starts the worker thread.
   - bool process_is_done(state *);
     Returns true if the I/O operation is complete.
   - void process_cancel(state *);
     "Cancels" the I/O operation. The thread will continue to run, but it
     will detach, and clean itself up upon completion.
   - int process_finish(state *, ...)
     Waits for the I/O operation to complete, returns results, and cleans up.

   Or:        /---\
             \/   |    /--> cancel
   start -> is_done --+
                       \--> finish

   These functions follow a basic pattern:
   - start:
     1. Allocate a thread state object with thread_alloc. This state object
        contains an io_thread member.
     2. Save parameters from the start parameters to the state object.
     3. Start the thread with _io_thread_create. The thread receives the state
        object as its parameter.
   - On the worker thread:
     1. Do the I/O.
     2. Call io_thread_return.
       2a. If the result != 0, free the state object.
   - is_done:
     1. Just call _io_thread_is_done.
   - cancel:
     1. Call io_thread_cancel.
       1a. If the result != 0, free the state object.
   - finish:
     1. Call io_thread_finish.
     2. Copy results out of the state object as needed.
     3. Free the state object...or return it to the caller.

   Incidentally, there may sometimes be asynchronous versions of blocking I/O
   functions (struct aiocb and friends, for example); these should be
   preferred over io_thread when performance is a concern.
 */

enum _io_thread_status
{
	_io_thread_working, _io_thread_done, _io_thread_cancelled
};

struct io_thread
{
#if HAVE_PTHREAD
	/* Common misconception: "volatile" should be applied to atomic variables,
	   such as 'status', below. This is false, see
	   <http://stackoverflow.com/q/2484980>. */
	enum _io_thread_status status;
	pthread_t thread;
#else
	char gcc_emits_a_warning_when_the_struct_has_no_members;
#endif
};

#if HAVE_PTHREAD

void *io_thread_create(struct io_thread *self, void *parent, void *(*start_routine)(void *), Display *dpy, unsigned stacksize);
/*
   Create the thread, returns NULL on failure.  Failure is usually due to
   ENOMEM, or the system doesn't support threads.
   self:          The io_thread object to be initialized.
   parent:        The parameter to start_routine.  The io_thread should be
                  contained within or be reachable from this.
   start_routine: The start routine for the worker thread.
   dpy:           The X11 Display, so that '*useThreads' is honored.
   stacksize:     The stack size for the thread. Set to 0 for the system
                  default.
   A note about stacksize: Linux, for example, uses a default of 2 MB of
   stack per thread. Now, this memory is usually committed on the first
   write, so lots of threads won't waste RAM, but it does mean that on a
   32-bit system, there's a limit of just under 1024 threads with the 2 MB
   default due to typical address space limitations of 2 GB for userspace
   processes.  And 1024 threads might not always be enough...
 */

int io_thread_return(struct io_thread *self);
/* Called at the end of start_routine, from above. Returns non-zero if the
   thread has been cancelled, and cleanup needs to take place. */

int io_thread_is_done(struct io_thread *self);
/* Call from the main thread. Returns non-zero if the thread finished. */

int io_thread_cancel(struct io_thread *self);
/* Call from the main thread if the results from the worker thread are not
   needed. This cleans up the io_thread. Returns non-zero if cleanup needs
   to take place. */

void io_thread_finish(struct io_thread *self);
/* Call from the main thread to wait for the worker thread to finish. This
   cleans up the io_thread. */

#else

#define IO_THREAD_STACK_MIN 0

#define io_thread_create(self, parent, start_routine, dpy, stacksize) NULL
#define io_thread_return(self) 0
#define io_thread_is_done(self) 1
#define io_thread_cancel(self) 0
#define io_thread_finish(self)

#endif

#if HAVE_PTHREAD
#	define THREAD_DEFAULTS       "*useThreads: True",
#	define THREAD_DEFAULTS_XLOCK "*useThreads: True\n"
#	define THREAD_OPTIONS \
	{"-threads",    ".useThreads", XrmoptionNoArg, "True"}, \
	{"-no-threads", ".useThreads", XrmoptionNoArg, "False"},
#else
#	define THREAD_DEFAULTS
#	define THREAD_DEFAULTS_XLOCK
#	define THREAD_OPTIONS
#endif

/*
   If a variable 'member' is known to be a member (named 'member_name') of a
   struct (named 'struct_name'), then this can find a pointer to the struct
   that contains it.
*/
#define GET_PARENT_OBJ(struct_name, member_name, member) (struct_name *)((char *)member - offsetof(struct_name, member_name));

#endif
