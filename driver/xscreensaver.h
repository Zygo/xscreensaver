/* xscreensaver, Copyright © 1993-2021 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __XSCREENSAVER_H__
#define __XSCREENSAVER_H__

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <string.h>
#include <stdio.h>

#ifdef HAVE_SIGACTION
# include <signal.h>    /* for sigset_t */
#endif

#include "blurb.h"
#include "types.h"

extern char *progclass;

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

/* =======================================================================
   server extensions and virtual roots
   ======================================================================= */

extern Bool ensure_no_screensaver_running (Display *, Screen *);

/* Display Power Management System (DPMS) interface. */
extern Bool monitor_powered_on_p (Display *);
extern void monitor_power_on (saver_info *si, Bool on_p);


/* =======================================================================
   blanking
   ======================================================================= */

extern Bool update_screen_layout (saver_info *si);
extern void initialize_screensaver_window (saver_info *si);

extern void blank_screen (saver_info *si);
extern void unblank_screen (saver_info *si);
extern void resize_screensaver_window (saver_info *si);

extern void get_screen_viewport (saver_screen_info *ssi,
                                 int *x_ret, int *y_ret,
                                 int *w_ret, int *h_ret,
                                 int target_x, int target_y,
                                 Bool verbose_p);


/* =======================================================================
   timers
   ======================================================================= */

extern void cycle_timer (XtPointer si, XtIntervalId *id);
extern void sleep_until_idle (saver_info *si, Bool until_idle_p);


/* =======================================================================
   subprocs
   ======================================================================= */

#ifdef HAVE_SIGACTION
 extern sigset_t block_sigchld (void);
#else  /* !HAVE_SIGACTION */
 extern int block_sigchld (void);
#endif /* !HAVE_SIGACTION */
extern void unblock_sigchld (void);
extern void hack_environment (saver_info *si);
extern void init_sigchld (saver_info *si);
extern void spawn_screenhack (saver_screen_info *ssi);
extern void kill_screenhack (saver_screen_info *ssi);
extern Bool any_screenhacks_running_p (saver_info *si);
extern Bool select_visual (saver_screen_info *ssi, const char *visual_name);
extern void store_saver_status (saver_info *si);
extern const char *signal_name (int signal);
extern void screenhack_obituary (saver_screen_info *,
                                 const char *name, const char *error);

/* =======================================================================
   misc
   ======================================================================= */

extern Visual *get_best_gl_visual (saver_info *si, Screen *screen);
extern void maybe_reload_init_file (saver_info *);
void print_available_extensions (saver_info *);

#endif /* __XSCREENSAVER_H__ */
