/* xscreensaver, Copyright (c) 1993-2001 Jamie Zawinski <jwz@jwz.org>
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

typedef struct screenhack screenhack;
struct screenhack {
  Bool enabled_p;
  char *visual;
  char *name;
  char *command;
};

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
  Bool capture_stderr_p;	/* whether to redirect stdout/stderr  */
  Bool debug_p;			/* pay no mind to the man behind the curtain */
  Bool xsync_p;			/* whether XSynchronize has been called */

  Bool lock_p;			/* whether to lock as well as save */
  Bool lock_vt_p;		/* whether to lock VTs too, if possible */

  Bool fade_p;			/* whether to fade to black, if possible */
  Bool unfade_p;		/* whether to fade from black, if possible */
  Time fade_seconds;		/* how long that should take */
  int fade_ticks;		/* how many ticks should be used */
  Bool splash_p;		/* whether to do a splash screen at startup */

  Bool install_cmap_p;		/* whether we should use our own colormap
				   when using the screen's default visual. */

  screenhack **screenhacks;	/* the programs to run */
  int screenhacks_count;

  int nice_inferior;		/* nice value for subprocs */
  int inferior_memory_limit;	/* setrlimit(LIMIT_AS) value for subprocs */

  Time initial_delay;		/* how long to sleep after launch */
  Time splash_duration;		/* how long the splash screen stays up */
  Time timeout;			/* how much idle time before activation */
  Time lock_timeout;		/* how long after activation locking starts */
  Time cycle;			/* how long each hack should run */
  Time passwd_timeout;		/* how much time before pw dialog goes down */
  Time pointer_timeout;		/* how often to check mouse position */
  Time notice_events_timeout;	/* how long after window creation to select */
  Time watchdog_timeout;	/* how often to re-raise and re-blank screen */

  Bool dpms_enabled_p;		/* Whether to power down the monitor */
  Time dpms_standby;		/* how long until monitor goes black */
  Time dpms_suspend;		/* how long until monitor power-saves */
  Time dpms_off;		/* how long until monitor powers down */

  Bool grab_desktop_p;		/* These are not used by "xscreensaver" */
  Bool grab_video_p;		/*  itself: they are used by the external */
  Bool random_image_p;		/*  "xscreensaver-getimage" program, and set */
  char *image_directory;	/*  by the "xscreensaver-demo" configurator. */

  Bool use_xidle_extension;	/* which extension to use, if possible */
  Bool use_mit_saver_extension;
  Bool use_sgi_saver_extension;
  Bool use_proc_interrupts;

  char *shell;			/* where to find /bin/sh */

  char *demo_command;		/* How to enter demo mode. */
  char *prefs_command;		/* How to edit preferences. */
  char *help_url;		/* Where the help document resides. */
  char *load_url_command;	/* How one loads URLs. */
};


extern void load_init_file (saver_preferences *p);
extern Bool init_file_changed_p (saver_preferences *p);
extern int write_init_file (saver_preferences *p, const char *version_string,
                            Bool verbose_p);
const char *init_file_name (void);

extern screenhack *parse_screenhack (const char *line);
extern void free_screenhack (screenhack *hack);
extern char *format_hack (screenhack *hack, Bool wrap_p);
char *make_hack_name (const char *shell_command);

/* From dpms.c */
extern void sync_server_dpms_settings (Display *dpy, Bool enabled_p,
                                       int standby_secs, int suspend_secs,
                                       int off_secs,
                                       Bool verbose_p);

#endif /* __XSCREENSAVER_PREFS_H__ */
