/* xscreensaver, Copyright (c) 2006-2012 Jamie Zawinski <jwz@jwz.org>
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

#ifndef __JWXYZ_TIMERS_H__
#define __JWXYZ_TIMERS_H__

#include "jwxyz.h"

typedef struct jwxyz_sources_data jwxyz_sources_data;

extern jwxyz_sources_data *jwxyz_sources_init (XtAppContext);
extern void jwxyz_sources_free (jwxyz_sources_data *);
extern void jwxyz_sources_run (jwxyz_sources_data *);

extern void jwxyz_XtRemoveInput_all (Display *);
// extern void jwxyz_XtRemoveTimeOut_all (Display *);

#endif /* __JWXYZ_TIMERS_H__ */
