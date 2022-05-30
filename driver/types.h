/* xscreensaver, Copyright © 1993-2022 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __XSCREENSAVER_TYPES_H__
#define __XSCREENSAVER_TYPES_H__

typedef struct saver_info saver_info;

typedef struct screenhack screenhack;
struct screenhack {
  Bool enabled_p;
  char *visual;
  char *name;
  char *command;
};

typedef enum {
  RANDOM_HACKS, ONE_HACK, BLANK_ONLY, DONT_BLANK, RANDOM_HACKS_SAME
} saver_mode;

typedef enum {
  TEXT_DATE, TEXT_LITERAL, TEXT_FILE, TEXT_PROGRAM, TEXT_URL
} text_mode;

typedef struct saver_preferences saver_preferences;
typedef struct saver_screen_info saver_screen_info;


/* This structure holds all the user-specified parameters, read from the
   command line, the resource database, or entered through a dialog box.
   It is used by xscreensaver-gfx and xscreensaver-settings;
 */
struct saver_preferences {

  XrmDatabase db;		/* The resource database into which the
				   init file is merged, and out of which the
				   preferences are parsed. */

  time_t init_file_date;	/* The date (from stat()) of the .xscreensaver
				   file the last time this process read or
				   wrote it. */

  Bool verbose_p;		/* whether to print out lots of status info */
  Bool ignore_uninstalled_p;	/* whether to avoid displaying or complaining
                                   about hacks that are not on $PATH */
  Bool debug_p;			/* pay no mind to the man behind the curtain */
  Bool xsync_p;			/* whether to XSynchronize */

  Bool lock_p;			/* whether to lock as well as save */

  Bool fade_p;			/* whether to fade to black, if possible */
  Bool unfade_p;		/* whether to fade from black, if possible */
  Time fade_seconds;		/* how long that should take */
  Bool splash_p;		/* whether to do a splash screen at startup */

  Bool install_cmap_p;		/* whether we should use our own colormap
				   when using the screen's default visual */

  screenhack **screenhacks;	/* the programs to run */
  int screenhacks_count;

  saver_mode mode;		/* hack-selection mode */
  int selected_hack;		/* in one_hack mode, this is the one */

  int nice_inferior;		/* nice value for subprocs */

  Time splash_duration;		/* how long the splash screen stays up */
  Time timeout;			/* how much idle time before activation */
  Time lock_timeout;		/* how long after activation locking starts */
  Time cycle;			/* how long each hack should run */
  Time passwd_timeout;		/* how long before pw dialog goes down */
  Time watchdog_timeout;	/* how often to re-raise and re-blank screen */
  int pointer_hysteresis;	/* mouse motions less than N/sec are ignored */

  Bool dpms_enabled_p;		/* whether to power down the monitor */
  Bool dpms_quickoff_p;		/* whether to power down monitor immediately
				   in "Blank Only" mode */
  Time dpms_standby;		/* how long until monitor goes black */
  Time dpms_suspend;		/* how long until monitor power-saves */
  Time dpms_off;		/* how long until monitor powers down */

  Bool grab_desktop_p;		/* These are not used by "xscreensaver" */
  Bool grab_video_p;		/*  itself: they are used by the external */
  Bool random_image_p;		/*  "xscreensaver-getimage" program, and set */
  char *image_directory;	/*  by "xscreensaver-settings". */

  text_mode tmode;		/* How we generate text to display. */
  char *text_literal;		/* used when tmode is TEXT_LITERAL. */
  char *text_file;		/* used when tmode is TEXT_FILE.    */
  char *text_program;		/* used when tmode is TEXT_PROGRAM. */
  char *text_url;		/* used when tmode is TEXT_URL.     */

  char *shell;			/* where to find /bin/sh */

  char *demo_command;		/* How to enter demo mode. */
  char *help_url;		/* Where the help document resides. */
  char *load_url_command;	/* How one loads URLs. */
  char *new_login_command;	/* Command for the "New Login" button. */
  char *dialog_theme;		/* Color scheme on the unlock dialog */

  int auth_warning_slack;	/* Don't warn about login failures if they
                                   all happen within this many seconds of
                                   a successful login. */
};

/* This structure holds all the data that applies to the program as a whole,
   or to the non-screen-specific parts of the display connection.
   It is used only by xscreensaver-gfx.
 */
struct saver_info {

  XtAppContext app;
  Display *dpy;

  char *version;
  saver_preferences prefs;

  int nscreens;
  int ssi_count;
  saver_screen_info *screens;
  struct _monitor **monitor_layout;	/* private to screens.c */
  Visual **best_gl_visuals;		/* visuals for GL hacks on screen N */
  void *fade_state;			/* fade.c private data */

# ifdef HAVE_RANDR
  int randr_event_number;
  int randr_error_number;
  Bool using_randr_extension;
# endif

  Bool demoing_p;		/* Whether we are demoing a single hack
				   (without UI.) */
  Bool emergency_p;		/* Restarted because of a crash */
  XtIntervalId watchdog_id;	/* Timer to implement `prefs.watchdog */

  int selection_mode;		/* Set to -1 if the NEXT ClientMessage has just
				   been received; set to -2 if PREV has just
				   been received; set to N if SELECT or DEMO N
				   has been received.  (This is kind of nasty.)
				 */
};


/* This structure holds all the data that applies to the screen-specific parts
   of the display connection; if the display has multiple screens, there will
   be one of these for each screen.  It is used only by xscreensaver-gfx.
*/
struct saver_screen_info {
  saver_info *global;

  int number;			/* The internal ordinal of this screen,
                                   counting Xinerama rectangles as separate
                                   screens. */
  int real_screen_number;	/* The number of the underlying X screen on
                                   which this rectangle lies. */
  Screen *screen;		/* The X screen in question. */

  int x, y, width, height;	/* The size and position of this rectangle
                                   on its underlying X screen. */

  Bool real_screen_p;		/* This will be true of exactly one ssi per
                                   X screen. */


  Window screensaver_window;	/* The window on which hacks are drawn.  This
                                   window might be destroyed and re-created as
                                   hacks cycle. */
  Colormap cmap;		/* The colormap that goes with the window. */
  Bool install_cmap_p;		/* Whether this screen should have its own
                                   colormap installed, for whichever of several
                                   reasons.  This is definitive (even a false
                                   value here overrides prefs->install_cmap_p.)
                                 */
  Visual *current_visual;	/* The visual of the window. */
  int current_depth;		/* How deep the visual (and the window) are. */

  Visual *default_visual;	/* visual to use when none other specified */

  Cursor cursor;		/* A blank cursor that goes with the
				   real root window. */
  unsigned long black_pixel;	/* Black, allocated from `cmap'. */
  Window error_dialog;		/* Error message about crashed savers */


  XtIntervalId cycle_id;	/* Timer to implement `prefs.cycle' */
  time_t cycle_at;		/* When cycle_id will fire */
  int current_hack;		/* Index into `prefs.screenhacks' */
  pid_t pid;
};


/* From dpms.c */
extern void sync_server_dpms_settings (Display *, struct saver_preferences *);


const char *init_file_name (void);
extern Bool init_file_changed_p (saver_preferences *);
extern void load_init_file (Display *, saver_preferences *);
extern int write_init_file (Display *,
                            saver_preferences *, const char *version_string,
                            Bool verbose_p);
extern Bool senescent_p (void);

char *make_hack_name (Display *, const char *shell_command);

#endif /* __XSCREENSAVER_TYPES_H__ */
