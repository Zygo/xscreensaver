/* xscreensaver, Copyright (c) 1992-2006 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __SCREENHACK_H__
#define __SCREENHACK_H__

#include "screenhackI.h"

/* In an Xlib world, we define two global symbols here:
   a struct in `MODULENAME_xscreensaver_function_table',
   and a pointer to that in `xscreensaver_function_table'.

   In a Cocoa world, we only define the prefixed symbol;
   the un-prefixed symbol does not exist.
 */
#ifdef HAVE_COCOA
# define XSCREENSAVER_LINK(NAME)
#else
# define XSCREENSAVER_LINK(NAME) \
   struct xscreensaver_function_table *xscreensaver_function_table = &NAME;
#endif


#if defined(HAVE_COCOA) && !defined(__XLOCKMORE_INTERNAL_H__)
  /* this is one enormous kludge... */
# undef ya_rand_init
  static void
  xscreensaver_common_setup(struct xscreensaver_function_table *xsft, void *a)
  { ya_rand_init(0); }
#else
# define xscreensaver_common_setup 0
#endif


#define XSCREENSAVER_MODULE_2(CLASS,NAME,PREFIX)		\
  struct xscreensaver_function_table				\
	 NAME ## _xscreensaver_function_table = {		\
	   CLASS,						\
	   PREFIX ## _defaults,					\
	   PREFIX ## _options,					\
	   xscreensaver_common_setup, 0,			\
	   PREFIX ## _init,					\
	   PREFIX ## _draw,					\
	   PREFIX ## _reshape,					\
	   PREFIX ## _event,					\
	   PREFIX ## _free,					\
           0, 0 };						\
  XSCREENSAVER_LINK (NAME ## _xscreensaver_function_table)

#define XSCREENSAVER_MODULE(CLASS,PREFIX)				\
      XSCREENSAVER_MODULE_2(CLASS,PREFIX,PREFIX)

#endif /* __SCREENHACK_H__ */
