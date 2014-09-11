/* -*- mode: c; tab-width: 4; fill-column: 128 -*- */
/* vi: set ts=4 tw=128: */

/*
aligned_malloc.h, Copyright (c) 2014 Dave Odell <dmo2118@gmail.com>

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.  No representations are made about the suitability of this
software for any purpose.  It is provided "as is" without express or
implied warranty.
*/

#ifndef __ALIGNED_MALLOC_H__
#define __ALIGNED_MALLOC_H__

#include <stdlib.h>

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#if _POSIX_VERSION >= 200112L || _XOPEN_VERSION >= 600

# define aligned_malloc posix_memalign
# define aligned_free   free

#else /* old POSIX */

 /* This can't simply be named posix_memalign, since the real thing uses
    free(), but this one can't. */
 int aligned_malloc(void **ptr, unsigned alignment, size_t size);
 void aligned_free(void *);

#endif /* old POSIX */

#endif /* __ALIGNED_MALLOC_H__ */
