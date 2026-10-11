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

#include "pow2.h"

int
i_log2 (size_t x)
{
  /* -1 works best for to_pow2. */
  if (!x)
    return -1;

  /* GCC 3.4 also has this. */
  /* The preprocessor criteria here must match what's in pow2.h, to prevent
   * infinite recursion.
   */
# if defined __GNUC__ && __GNUC__ >= 4 || defined __clang__
  return i_log2_fast(x);
# else
  {
    unsigned bits = sizeof(x) * CHAR_BIT;
    size_t mask = (size_t)-1;
    unsigned result = bits - 1;

    while (bits) {
      if (!(x & mask)) {
        result -= bits;
        x <<= bits;
      }

      bits >>= 1;
      mask <<= bits;
    }

    return result;
  }
# endif
}

size_t
to_pow2 (size_t x)
{
  return !x ? 1 : 1 << (i_log2(x - 1) + 1);
}
