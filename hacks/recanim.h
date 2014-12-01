/* recanim, Copyright (c) 2014 Jamie Zawinski <jwz@jwz.org>
 * Record animation frames of the running screenhack.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __XSCREENSAVER_RECORD_ANIM_H__
# define __XSCREENSAVER_RECORD_ANIM_H__

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

typedef struct record_anim_state record_anim_state;

extern record_anim_state *screenhack_record_anim_init (Screen *, Window,
                                                       int frames);
extern void screenhack_record_anim (record_anim_state *);
extern void screenhack_record_anim_free (record_anim_state *);

#endif /* __XSCREENSAVER_RECORD_ANIM_H__ */
