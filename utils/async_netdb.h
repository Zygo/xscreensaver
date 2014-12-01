/* vi: set tw=78: */

/* async_netdb.h, Copyright (c) Dave Odell <dmo2118@gmail.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#ifndef ASYNC_NETDB_H
#define ASYNC_NETDB_H

#include "thread_util.h"

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

/*
   Both async_name_from_addr_* and async_addr_from_name_* follow basic pattern
   for io_thread clients as described in thread_util.h:
   1. async_*_from_*_start
   2. async_*_from_*_is_done (repeat as necessary)
   3a. async_*_from_*_finish to retrieve the results, or:
   3b. async_*_from_*_cancel to abort the lookup.

   On systems that can't do asynchronous host lookups, the *_finish functions
   do the actual lookup.
 */

#ifndef NI_MAXHOST
/*
   From the glibc man page for getnameinfo(3):
      Since glibc 2.8, these definitions are exposed only if one of the
      feature test macros _BSD_SOURCE, _SVID_SOURCE, or _GNU_SOURCE is
      defined.
 */
# define NI_MAXHOST 1025
#endif

#if HAVE_PTHREAD && HAVE_GETADDRINFO
/*
   If threads or getaddrinfo() are unavailable, then the older gethostbyname()
   and gethostbyaddr() functions are used, and IPv6 is disabled.
 */
# define ASYNC_NETDB_USE_GAI 1
#endif

#if ASYNC_NETDB_USE_GAI

typedef struct sockaddr_storage async_netdb_sockaddr_storage_t;

int _async_netdb_is_done (struct io_thread *io);

#else

typedef struct sockaddr_in async_netdb_sockaddr_storage_t;

# ifndef EAI_SYSTEM
/* The EAI_* codes are specified specifically as preprocessor macros, so
   the #ifdef here should always work...
   http://pubs.opengroup.org/onlinepubs/009604499/basedefs/netdb.h.html */

#   define ASYNC_NETDB_FAKE_EAI 1

/* Even without addrinfo, the EAI_* error codes are used. The numbers are from
   Linux's netdb.h. */
#   define EAI_NONAME -2
#   define EAI_AGAIN  -3
#   define EAI_FAIL   -4
#   define EAI_MEMORY -10
#   define EAI_SYSTEM -11

const char *_async_netdb_strerror (int errcode);

#   define gai_strerror(errcode) _async_netdb_strerror (errcode)
# endif

# define _async_netdb_is_done(io) 1

#endif

/* In non-threaded mode, _async_name_from_addr_param is used in place of
   async_name_from_addr. */
struct _async_name_from_addr_param
{
  socklen_t addrlen;
  async_netdb_sockaddr_storage_t addr;
};

typedef struct async_name_from_addr
{
  /*
     Stupid memory trick, thwarted: The host string could be at the beginning
     of this structure, and the memory block that contains this struct could
     be resized and returned directly in async_name_from_addr_finish. But...

     There is no aligned_realloc. In fact, aligned_realloc is a bit of a
     problem, mostly because:
     1. realloc() is the only way to resize a heap-allocated memory block.
     2. realloc() moves memory.
     3. The location that realloc() moves memory to won't be aligned.
   */

  struct _async_name_from_addr_param param;
  struct io_thread io;

  char host[NI_MAXHOST];
  int gai_error;
  int errno_error;

} *async_name_from_addr_t;

async_name_from_addr_t async_name_from_addr_start (Display *dpy,
                                                   const struct sockaddr *addr,
                                                   socklen_t addrlen);
/*
   Starts an asynchronous name-from-address lookup.
   dpy:     An X11 Display with a .useThreads resource.
   addr:    An address. Like various socket functions (e.g. bind(2),
            connect(2), sendto(2)), this isn't really a struct sockaddr so
            much as a "subtype" of sockaddr, like sockaddr_in, or
            sockaddr_in6, or whatever.
   addrlen: The (actual) length of *addr.
   Returns NULL if the request couldn't be created (due to low memory).
 */

#define async_name_from_addr_is_done(self) _async_netdb_is_done (&(self)->io)

#if ASYNC_NETDB_USE_GAI
void async_name_from_addr_cancel (async_name_from_addr_t self);
#else
# define async_name_from_addr_cancel(self) (free (self))
#endif

int async_name_from_addr_finish (async_name_from_addr_t self,
                                 char **host, int *errno_error);
/*
   Gets the result of an asynchronous name-from-address lookup. If the lookup
   operation is still in progress, or if the system can't do async lookups,
   this will block. This cleans up the lookup operation; do not use 'self'
   after calling this function.
   self:        The lookup operation.
   host:        If no error, the name of the host. Free this with free(3).
   errno_error: The value of errno if EAI_SYSTEM is returned. Can be NULL.
   Returns 0 on success, otherwise an error from getnameinfo(3).
 */

/* In non-threaded mode, async_addr_from_name contains different stuff. */
typedef struct async_addr_from_name
{
#if ASYNC_NETDB_USE_GAI
  struct io_thread io;

  int gai_error;
  int errno_error;

  struct addrinfo *res;
#else
  char dont_complain_about_empty_structs;
#endif
} *async_addr_from_name_t;

async_addr_from_name_t async_addr_from_name_start (Display *dpy,
                                                   const char *host);
/*
   Starts an asynchronous address-from-name lookup.
   dpy:  An X11 display.
   host: The hostname to look up.
   Returns NULL if the request couldn't be created (due to low memory).
 */

#define async_addr_from_name_is_done(self) _async_netdb_is_done (&(self)->io)

#if ASYNC_NETDB_USE_GAI
void async_addr_from_name_cancel (async_addr_from_name_t self);
#else
# define async_addr_from_name_cancel(self) (thread_free (self));
#endif

/* sockaddr must be sizeof(async_netdb_sockaddr_storage_t) in size. */
int async_addr_from_name_finish (async_addr_from_name_t self, void *addr,
                                 socklen_t *addrlen, int *errno_error);
/*
   Returns the address from an asynchronous address-from-name operation. If
   the lookup is still in progress, or the system can't do an asynchronous
   lookup, this blocks. This cleans up the lookup operation; do not use 'self'
   after calling this function.
   self:        The lookup operation.
   addr:        A sockaddr. This must be as large as or larger than
                sizeof(async_netdb_sockaddr_storage_t). (Hint: just use an
                instance of async_netdb_sockaddr_storage_t itself here.)
   addrlen:     The length of the obtained sockaddr.
   errno_error: The value of errno if EAI_SYSTEM is returned. Can be NULL.
   Returns 0 on success, or an error from getaddrinfo(3).
 */

#endif

/* Local Variables:      */
/* mode: c               */
/* fill-column: 78       */
/* c-file-style: "gnu"   */
/* c-basic-offset: 2     */
/* indent-tabs-mode: nil */
/* End:                  */
