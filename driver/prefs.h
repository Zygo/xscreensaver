/* xscreensaver, Copyright (c) 1993-2015 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __XSCREENSAVER_PREFS_H__
#define __XSCREENSAVER_PREFS_H__

#include "types.h"

extern void load_init_file (Display *, saver_preferences *);
extern Bool init_file_changed_p (saver_preferences *);
extern int write_init_file (Display *,
                            saver_preferences *, const char *version_string,
                            Bool verbose_p);
const char *init_file_name (void);
extern Bool senescent_p (void);

extern screenhack *parse_screenhack (const char *line);
extern void free_screenhack (screenhack *);
extern char *format_hack (Display *, screenhack *, Bool wrap_p);
char *make_hack_name (Display *, const char *shell_command);

/* From dpms.c */
extern void sync_server_dpms_settings (Display *, Bool enabled_p,
                                       Bool dpms_quickoff_p,
                                       int standby_secs, int suspend_secs,
                                       int off_secs,
                                       Bool verbose_p);

#endif /* __XSCREENSAVER_PREFS_H__ */
