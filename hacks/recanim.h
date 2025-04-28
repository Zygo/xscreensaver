/* recanim, Copyright (c) 2014-2021 Jamie Zawinski <jwz@jwz.org>
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

typedef struct record_anim_state record_anim_state;

extern record_anim_state *screenhack_record_anim_init (Screen *, Window,
                                                       int frames);
extern void screenhack_record_anim (record_anim_state *);
extern void screenhack_record_anim_free (record_anim_state *);

extern time_t screenhack_record_anim_time (time_t *);
extern void screenhack_record_anim_gettimeofday (struct timeval *
# ifdef GETTIMEOFDAY_TWO_ARGS
                                                 , struct timezone *
# endif
                                                 );
#define time screenhack_record_anim_time
#define gettimeofday screenhack_record_anim_gettimeofday

#endif /* __XSCREENSAVER_RECORD_ANIM_H__ */
