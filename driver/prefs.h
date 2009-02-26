/* xscreensaver, Copyright (c) 1993-1998 Jamie Zawinski <jwz@jwz.org>
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

typedef struct saver_preferences saver_preferences;


/* This structure holds all the user-specified parameters, read from the
   command line, the resource database, or entered through a dialog box.
 */
struct saver_preferences {

  XrmDatabase db;		/* The resource database into which the
				   init file is merged, and out of which the
				   preferences are parsed. */

  time_t init_file_date;	/* The date (from stat()) of the .xscreensaver
				   file the last time this process read or
				   wrote it. */

  Bool verbose_p;		/* whether to print out lots of status info */
  Bool timestamp_p;		/* whether to mark messages with a timestamp */
  Bool debug_p;			/* pay no mind to the man behind the curtain */
  Bool xsync_p;			/* whether XSynchronize has been called */

  Bool lock_p;			/* whether to lock as well as save */
  Bool lock_vt_p;		/* whether to lock VTs too, if possible */

  Bool fade_p;			/* whether to fade to black, if possible */
  Bool unfade_p;		/* whether to fade from black, if possible */
  int fade_seconds;		/* how long that should take */
  int fade_ticks;		/* how many ticks should be used */
  Bool fading_possible_p;	/* Whether fading to/from black is possible.
				   (This isn't strictly a preference, as it
				   can only be known by querying the display;
				   the caller of the prefs code may fill this
				   in if it knows/cares, and warnings will be
				   issued.) */

  Bool install_cmap_p;		/* whether we should use our own colormap
				   when using the screen's default visual. */

  char **screenhacks;		/* the programs to run */
  int screenhacks_count;

  int nice_inferior;		/* nice value for subprocs */

  Time initial_delay;		/* how long to sleep after launch */
  Time splash_duration;		/* how long the splash screen stays up */
  Time timeout;			/* how much idle time before activation */
  Time lock_timeout;		/* how long after activation locking starts */
  Time cycle;			/* how long each hack should run */
  Time passwd_timeout;		/* how much time before pw dialog goes down */
  Time pointer_timeout;		/* how often to check mouse position */
  Time notice_events_timeout;	/* how long after window creation to select */
  Time watchdog_timeout;	/* how often to re-raise and re-blank screen */

  Bool use_xidle_extension;	/* which extension to use, if possible */
  Bool use_mit_saver_extension;
  Bool use_sgi_saver_extension;

  char *shell;			/* where to find /bin/sh */

  char *demo_command;		/* How to enter demo mode. */
  char *prefs_command;		/* How to edit preferences. */
  char *help_url;		/* Where the help document resides. */
  char *load_url_command;	/* How one loads URLs. */
};


extern void load_init_file (saver_preferences *p);
extern Bool init_file_changed_p (saver_preferences *p);
extern void write_init_file (saver_preferences *p, const char *version_string);
const char *init_file_name (void);

#endif /* __XSCREENSAVER_PREFS_H__ */
