/* vi: set tw=78: */

/* async_netdb.c, Copyright (c) Dave Odell <dmo2118@gmail.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * Threaded versions of get(name/addr)info to replace calls to
 * gethostby(name/addr), for Sonar.
 */

#include "async_netdb.h"

#include "thread_util.h"

#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>

/* This is very much system-dependent, but hopefully 64K covers it just about
   everywhere. The threads here shouldn't need much. */
#define ASYNC_NETDB_STACK 65536

#if ASYNC_NETDB_USE_GAI

# define _get_addr_family(addr) ((addr)->x_sockaddr_storage.ss_family)
# define _get_addr_len(addr) ((addr)->x_sockaddr_storage.ss_len)

static int _has_threads;

int _async_netdb_is_done (struct io_thread *io)
{
  if (_has_threads >= 0)
    return io_thread_is_done (io);
  return 1;
}

#else /* ASYNC_NETDB_USE_GAI */

# define _get_addr_family(addr) ((addr)->x_sockaddr_in.sin_family)
# define _get_addr_len(addr) ((addr)->x_sockaddr_in.sin_len)

static const int _has_threads = -1;

# if ASYNC_NETDB_FAKE_EAI

const char *_async_netdb_strerror (int errcode)
{
  /* (h)strerror should return messages in the user's preferred language. */

  /* On Solaris, gethostbyname(3) is in libnsl, but hstrerror(3) is in
     libresolv. Not that it matters, since Solaris has real gai_strerror in
     libsocket. */

  switch (errcode)
    {
    case EAI_NONAME:
      return hstrerror (HOST_NOT_FOUND);
    case EAI_AGAIN:
      return hstrerror (TRY_AGAIN);
    case EAI_FAIL:
      return hstrerror (NO_RECOVERY);
    case EAI_MEMORY:
      return strerror (ENOMEM);
    }
  /* case EAI_SYSTEM: default: */
  return strerror (EINVAL); /* No good errno equivalent here. */
}

# endif /* ASYNC_NETDB_FAKE_EAI */

#endif /* !ASYNC_NETDB_USE_GAI */

static int _translate_h_errno (int error)
{
  switch (error)
    {
    case HOST_NOT_FOUND:
    case NO_DATA:
      return EAI_NONAME;
    case TRY_AGAIN:
      return EAI_AGAIN;
    }

  /* case NO_RECOVERY: default: */
  return EAI_FAIL;
}

/* async_name_from_addr - */

#if ASYNC_NETDB_USE_GAI

static void *
_async_name_from_addr_thread (void *self_raw)
{
  /* The stack is unusually small here. If this crashes, you may need to bump
     the value of ASYNC_NETDB_STACK. */

  async_name_from_addr_t self = (async_name_from_addr_t)self_raw;

  /* getnameinfo() is thread-safe, gethostbyaddr() is not. */
  self->gai_error = getnameinfo ((void *)&self->param.addr,
                                 self->param.addrlen,
                                 self->host, sizeof(self->host), NULL, 0,
                                 NI_NAMEREQD);
  /* Removing NI_MAKEREQD would require a call to inet_ntoa in
     async_name_from_addr_finish. */

  self->errno_error = errno;

  if (io_thread_return (&self->io))
    thread_free(self);

  return NULL;
}

#endif /* ASYNC_NETDB_USE_GAI */

static void
_async_name_from_addr_set_param (struct _async_name_from_addr_param *self,
                                 const struct sockaddr *addr,
                                 socklen_t addrlen)
{
  self->addrlen = addrlen;
  memcpy (&self->addr, addr, addrlen);

#if HAVE_STRUCT_SOCKADDR_SA_LEN
  _get_addr_len (&self->addr) = addrlen; /* The BSDs need this. */
#endif
}

async_name_from_addr_t
async_name_from_addr_start (Display *dpy, const struct sockaddr *addr,
                            socklen_t addrlen)
{
  assert (addrlen);
  assert (addrlen <= sizeof (async_netdb_sockaddr_storage_t));

#if ASYNC_NETDB_USE_GAI
  _has_threads = threads_available (dpy);
  if (_has_threads >= 0)
    {
      async_name_from_addr_t self, result;

      if (thread_malloc ((void **)&self, dpy,
                         sizeof (struct async_name_from_addr)))
        return NULL;

      _async_name_from_addr_set_param (&self->param, addr, addrlen);

      result = io_thread_create (&self->io, self,
                                 _async_name_from_addr_thread, dpy,
                                 ASYNC_NETDB_STACK);
      if (!result)
        thread_free (self);
      return result;
    }
#endif /* ASYNC_NETDB_USE_GAI */

  {
    struct _async_name_from_addr_param *result =
      (struct _async_name_from_addr_param *)
      malloc (sizeof (struct _async_name_from_addr_param));

    if (result)
      _async_name_from_addr_set_param (result, addr, addrlen);

    return (async_name_from_addr_t)result;
  }
}

#if ASYNC_NETDB_USE_GAI
void
async_name_from_addr_cancel (async_name_from_addr_t self)
{
  if (_has_threads >= 0)
    {
      if(io_thread_cancel (&self->io))
        thread_free (self);
    }
  else
    {
      free (self);
    }
}
#endif /* ASYNC_NETDB_USE_GAI */

int
async_name_from_addr_finish (async_name_from_addr_t self_raw,
                             char **host, int *errno_error)
{
#if ASYNC_NETDB_USE_GAI
  if (_has_threads >= 0)
    {
      async_name_from_addr_t self = self_raw;
      int gai_error;

      io_thread_finish (&self->io);

      gai_error = self->gai_error;
      if (gai_error)
        {
          if (errno_error)
            *errno_error = self->errno_error;
          *host = NULL; /* For safety's sake. */
        }
      else
        {
          *host = strdup(self->host);
          if (!*host)
            gai_error = EAI_MEMORY;
        }

      thread_free (self);
      return gai_error;
    }
#endif /* ASYNC_NETDB_USE_GAI */

  {
    struct _async_name_from_addr_param *self =
      (struct _async_name_from_addr_param *)self_raw;

    const struct hostent *he;
    int error;
    const void *raw_addr;
    socklen_t addrlen;

    switch (_get_addr_family (&self->addr))
      {
      case AF_INET:
        raw_addr = &self->addr.x_sockaddr_in.sin_addr;
        addrlen = 4;
        break;
#if ASYNC_NETDB_USE_GAI
      case AF_INET6:
        raw_addr = &self->addr.x_sockaddr_in6.sin6_addr;
        addrlen = 16;
        break;
#endif /* ASYNC_NETDB_USE_GAI */
      default:
        return EAI_NONAME;
      }

    he = gethostbyaddr(raw_addr, addrlen, _get_addr_family (&self->addr));
    error = h_errno;

    free (self);

    if (!he)
    {
      *host = NULL; /* For safety's sake. */
      return _translate_h_errno(error);
    }

    if (!he->h_name)
      return EAI_NONAME;

    *host = strdup (he->h_name);
    if (!*host)
      return EAI_MEMORY;

    return 0;
  }
}

/* async_addr_from_name - */

static char *
_async_addr_from_name_hostname (async_addr_from_name_t self)
{
  return (char *)(self + 1);
}

static void
_async_addr_from_name_free (async_addr_from_name_t self)
{
#if ASYNC_NETDB_USE_GAI
  if (self->res) /* FreeBSD won't do freeaddrinfo (NULL). */
    freeaddrinfo (self->res);
#endif
  thread_free (self);
}

#if ASYNC_NETDB_USE_GAI

static void *
_async_addr_from_name_thread (void *self_raw)
{
  /* The stack is unusually small here. If this crashes, you may need to bump
     the value of ASYNC_NETDB_STACK. */

  async_addr_from_name_t self = (async_addr_from_name_t)self_raw;
  self->gai_error = getaddrinfo (_async_addr_from_name_hostname (self), NULL,
                                 NULL, &self->res);
  self->errno_error = errno;

  if (io_thread_return (&self->io))
    _async_addr_from_name_free (self);

  return NULL;
}

#endif /* ASYNC_NETDB_USE_GAI */

/* getaddrinfo supports more than a hostname, but gethostbyname does not. */
async_addr_from_name_t
async_addr_from_name_start (Display *dpy, const char *hostname)
{
  async_addr_from_name_t self;
  if (thread_malloc ((void **)&self, dpy,
                     sizeof(struct async_addr_from_name) + strlen(hostname) + 1))
    return NULL;

  strcpy (_async_addr_from_name_hostname (self), hostname);

#if ASYNC_NETDB_USE_GAI
  _has_threads = threads_available (dpy);
  self->res = NULL;
  if (_has_threads >= 0)
    {
      async_addr_from_name_t result =
        io_thread_create (&self->io, self, _async_addr_from_name_thread, dpy,
                          ASYNC_NETDB_STACK);

      if (!result)
        thread_free(result);
      self = result;
    }
#endif /* ASYNC_NETDB_USE_GAI */

  return self;
}

#if ASYNC_NETDB_USE_GAI
void
async_addr_from_name_cancel (async_addr_from_name_t self)
{
  if (_has_threads >= 0)
    {
      if (io_thread_cancel (&self->io))
        _async_addr_from_name_free (self);
    }
  else
    {
      thread_free (self);
    }
}
#endif /* ASYNC_NETDB_USE_GAI */

/* async_name_from_addr_finish does sockaddr_in or sockaddr_in6. */

int
async_addr_from_name_finish (async_addr_from_name_t self, void *addr,
                             socklen_t *addrlen, int *errno_error)
{
#if ASYNC_NETDB_USE_GAI
  if (_has_threads >= 0)
    {
      int gai_error;
      io_thread_finish (&self->io);

      gai_error = self->gai_error;
      if (errno_error)
        *errno_error = self->errno_error;

      if (!gai_error)
        {
          struct addrinfo *ai = self->res;
          if (!ai)
            gai_error = EAI_NONAME;
          else
            {
              assert (ai->ai_addrlen <=
                      sizeof (async_netdb_sockaddr_storage_t));
              memcpy (addr, ai->ai_addr, ai->ai_addrlen);
              *addrlen = ai->ai_addrlen;
            }
        }

      _async_addr_from_name_free (self);
      return gai_error;
    }
#endif /* ASYNC_NETDB_USE_GAI */

  {
    struct hostent *he =
      gethostbyname (_async_addr_from_name_hostname (self));
    int error = h_errno;
    void *raw_addr;
    async_netdb_sockaddr_storage_t *addr_storage =
      (async_netdb_sockaddr_storage_t *)addr;

    _async_addr_from_name_free (self);

    if (!he)
      return _translate_h_errno (error);

    switch (he->h_addrtype)
      {
      case AF_INET:
        {
          struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
          addr_in->sin_port = 0;
          raw_addr = &addr_in->sin_addr;
          *addrlen = sizeof(*addr_in);
          assert (he->h_length == 4);
        }
        break;
#if ASYNC_NETDB_USE_GAI
      case AF_INET6:
        {
          struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)addr;
          addr_in6->sin6_port = 0;
          addr_in6->sin6_flowinfo = 0;
          raw_addr = &addr_in6->sin6_addr;
          *addrlen = sizeof(*addr_in6);
          assert (he->h_length == 16);
        }
        break;
#endif /* ASYNC_NETDB_USE_GAI */
      default:
        return EAI_NONAME;
      }

#if HAVE_STRUCT_SOCKADDR_SA_LEN
    _get_addr_len (addr_storage) = *addrlen;
#endif
    _get_addr_family (addr_storage) = he->h_addrtype;

    memcpy (raw_addr, he->h_addr_list[0], he->h_length);
    return 0;
  }
}

/* Local Variables:      */
/* mode: c               */
/* fill-column: 78       */
/* c-file-style: "gnu"   */
/* c-basic-offset: 2     */
/* indent-tabs-mode: nil */
/* End:                  */
