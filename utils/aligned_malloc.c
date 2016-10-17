/* -*- mode: c; tab-width: 4; fill-column: 128 -*- */
/* vi: set ts=4 tw=128: */

/*
aligned_malloc.c, Copyright (c) 2014 Dave Odell <dmo2118@gmail.com>

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.  No representations are made about the suitability of this
software for any purpose.  It is provided "as is" without express or
implied warranty.
*/

#include "aligned_malloc.h"

#include <stddef.h>
#include <stdlib.h>

#if !ALIGNED_MALLOC_HAS_MEMALIGN

#include <assert.h>
#include <errno.h>

int aligned_malloc(void **ptr, unsigned alignment, size_t size)
{
	void *block_start;
	ptrdiff_t align1 = alignment - 1;

	assert(alignment && !(alignment & (alignment - 1))); /* alignment must be a power of two. */

	size += sizeof(void *) + align1;
	block_start = malloc(size);
	if(!block_start)
		return ENOMEM;
	*ptr = (void *)(((ptrdiff_t)block_start + sizeof(void *) + align1) & ~align1);
	((void **)(*ptr))[-1] = block_start;
	return 0;
}

void aligned_free(void *ptr)
{
	free(((void **)(ptr))[-1]);
}

#endif
