/* pow2, Copyright (c) 2016 Dave Odell <dmo2118@gmail.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#ifndef POW2_H
#define POW2_H

#include <stdlib.h>

extern int i_log2(size_t x);
extern size_t to_pow2 (size_t x); /* return the next larger power of 2. */

#endif /* POW2_H */
