/* xscreensaver, Copyright (c) 1993-2008 Jamie Zawinski <jwz@jwz.org>
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

typedef enum {
  ul_read,		/* reading input or ready to do so */
  ul_success,		/* auth success, unlock */
  ul_fail,		/* auth fail */
  ul_cancel,		/* user cancelled auth (pw_cancel or pw_null) */
  ul_time,		/* timed out */
  ul_finished		/* user pressed enter */
} unlock_state;

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

struct auth_message;
struct auth_response;

typedef int (*auth_conv_cb_t) (
  int num_msg,
  const struct auth_message *msg,
  struct auth_response **resp,
  saver_info *si);

typedef struct saver_preferences saver_preferences;
typedef struct saver_screen_info saver_screen_info;
typedef struct passwd_dialog_data passwd_dialog_data;
typedef struct splash_dialog_data splash_dialog_data;
typedef struct _monitor monitor;


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
  Bool ignore_uninstalled_p;	/* whether to avoid displaying or complaining
                                   about hacks that are not on $PATH */
  Bool debug_p;			/* pay no mind to the man behind the curtain */
  Bool xsync_p;			/* whether XSynchronize has been called */

  Bool lock_p;			/* whether to lock as well as save */

  Bool fade_p;			/* whether to fade to black, if possible */
  Bool unfade_p;		/* whether to fade from black, if possible */
  Time fade_seconds;		/* how long that should take */
  int fade_ticks;		/* how many ticks should be used */
  Bool splash_p;		/* whether to do a splash screen at startup */

  Bool install_cmap_p;		/* whether we should use our own colormap
				   when using the screen's default visual. */

# ifdef QUAD_MODE
  Bool quad_p;			/* whether to run four savers per monitor */
# endif

  screenhack **screenhacks;	/* the programs to run */
  int screenhacks_count;

  saver_mode mode;		/* hack-selection mode */
  int selected_hack;		/* in one_hack mode, this is the one */

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
  int pointer_hysteresis;	/* mouse motions less than N/sec are ignored */

  Bool dpms_enabled_p;		/* Whether to power down the monitor */
  Time dpms_standby;		/* how long until monitor goes black */
  Time dpms_suspend;		/* how long until monitor power-saves */
  Time dpms_off;		/* how long until monitor powers down */

  Bool grab_desktop_p;		/* These are not used by "xscreensaver" */
  Bool grab_video_p;		/*  itself: they are used by the external */
  Bool random_image_p;		/*  "xscreensaver-getimage" program, and set */
  char *image_directory;	/*  by the "xscreensaver-demo" configurator. */

  text_mode tmode;		/* How we generate text to display. */
  char *text_literal;		/* used when tmode is TEXT_LITERAL. */
  char *text_file;		/* used when tmode is TEXT_FILE.    */
  char *text_program;		/* used when tmode is TEXT_PROGRAM. */
  char *text_url;		/* used when tmode is TEXT_URL.     */

  Bool use_xidle_extension;	/* which extension to use, if possible */
  Bool use_mit_saver_extension;
  Bool use_sgi_saver_extension;
  Bool use_proc_interrupts;

  Bool getviewport_full_of_lies_p; /* XFree86 bug #421 */

  char *shell;			/* where to find /bin/sh */

  char *demo_command;		/* How to enter demo mode. */
  char *prefs_command;		/* How to edit preferences. */
  char *help_url;		/* Where the help document resides. */
  char *load_url_command;	/* How one loads URLs. */
  char *new_login_command;	/* Command for the "New Login" button. */
};

/* This structure holds all the data that applies to the program as a whole,
   or to the non-screen-specific parts of the display connection.

   The saver_preferences structure (prefs.h) holds all the user-specified
   parameters, read from the command line, the resource database, or entered
   through a dialog box.
 */
struct saver_info {
  char *version;
  saver_preferences prefs;

  int nscreens;
  int ssi_count;
  saver_screen_info *screens;
  saver_screen_info *default_screen;	/* ...on which dialogs will appear. */
  monitor **monitor_layout;		/* private to screens.c */
  Visual **best_gl_visuals;		/* visuals for GL hacks on screen N */

  /* =======================================================================
     global connection info
     ======================================================================= */

  XtAppContext app;
  Display *dpy;

  /* =======================================================================
     server extension info
     ======================================================================= */

  Bool using_xidle_extension;	   /* which extension is being used.         */
  Bool using_mit_saver_extension;  /* Note that `p->use_*' is the *request*, */
  Bool using_sgi_saver_extension;  /* and `si->using_*' is the *reality*.    */
  Bool using_proc_interrupts;

# ifdef HAVE_MIT_SAVER_EXTENSION
  int mit_saver_ext_event_number;
  int mit_saver_ext_error_number;
# endif
# ifdef HAVE_SGI_SAVER_EXTENSION
  int sgi_saver_ext_event_number;
  int sgi_saver_ext_error_number;
# endif
# ifdef HAVE_RANDR
  int randr_event_number;
  int randr_error_number;
# endif


  /* =======================================================================
     blanking
     ======================================================================= */

  Bool screen_blanked_p;	/* Whether the saver is currently active. */
  Window mouse_grab_window;	/* Window holding our mouse grab */
  Window keyboard_grab_window;	/* Window holding our keyboard grab */
  int mouse_grab_screen;	/* The screen number the mouse grab is on */
  int keyboard_grab_screen;	/* The screen number the keyboard grab is on */
  Bool fading_possible_p;	/* Whether fading to/from black is possible. */
  Bool throttled_p;             /* Whether we should temporarily just blank
                                   the screen, not run hacks. (Deprecated:
                                   users should use "xset dpms force off"
                                   instead.) */
  time_t blank_time;		/* The time at which the screen was blanked
                                   (if currently blanked) or unblanked (if
                                   not blanked.) */


  /* =======================================================================
     locking and runtime privileges
     ======================================================================= */

  Bool locked_p;		/* Whether the screen is currently locked. */
  Bool dbox_up_p;		/* Whether the demo-mode or passwd dialogs
				   are currently visible */

  Bool locking_disabled_p;	/* Sometimes locking is impossible. */
  char *nolock_reason;		/* This is why. */

  char *orig_uid;		/* What uid/gid we had at startup, before
				   discarding privileges. */
  char *uid_message;		/* Any diagnostics from our attempt to
				   discard privileges (printed only in
				   -verbose mode.) */
  Bool dangerous_uid_p;		/* Set to true if we're running as a user id
				   which is known to not be a normal, non-
				   privileged user. */

  Window passwd_dialog;		/* The password dialog, if it's up. */
  passwd_dialog_data *pw_data;	/* Other info necessary to draw it. */

  int unlock_failures;		/* Counts failed login attempts while the
				   screen is locked. */

  char *unlock_typeahead;	/* If the screen is locked, and the user types
                                   a character, we assume that it is the first
                                   character of the password.  It's stored here
                                   for the password dialog to use to populate
                                   itself. */

  char *user;			/* The user whose session is locked. */
  char *cached_passwd;		/* Cached password, used to avoid multiple
				   prompts for password-only auth mechanisms.*/
  unlock_state unlock_state;

  auth_conv_cb_t unlock_cb;	/* The function used to prompt for creds. */
  void (*auth_finished_cb) (saver_info *si);
				/* Called when authentication has finished,
				   regardless of success or failure.
				   May be NULL. */


  /* =======================================================================
     demoing
     ======================================================================= */

  Bool demoing_p;		/* Whether we are demoing a single hack
				   (without UI.) */

  Window splash_dialog;		/* The splash dialog, if its up. */
  splash_dialog_data *sp_data;	/* Other info necessary to draw it. */


  /* =======================================================================
     timers
     ======================================================================= */

  XtIntervalId lock_id;		/* Timer to implement `prefs.lock_timeout' */
  XtIntervalId cycle_id;	/* Timer to implement `prefs.cycle' */
  XtIntervalId timer_id;	/* Timer to implement `prefs.timeout' */
  XtIntervalId watchdog_id;	/* Timer to implement `prefs.watchdog */
  XtIntervalId check_pointer_timer_id;	/* `prefs.pointer_timeout' */

  XtIntervalId de_race_id;	/* Timer to make sure screen un-blanks */
  int de_race_ticks;

  time_t last_activity_time;		   /* Used only when no server exts. */
  time_t last_wall_clock_time;             /* Used to detect laptop suspend. */
  saver_screen_info *last_activity_screen;

  Bool emergency_lock_p;        /* Set when the wall clock has jumped
                                   (presumably due to laptop suspend) and we
                                   need to lock down right away instead of
                                   waiting for the lock timer to go off. */


  /* =======================================================================
     remote control
     ======================================================================= */

  int selection_mode;		/* Set to -1 if the NEXT ClientMessage has just
				   been received; set to -2 if PREV has just
				   been received; set to N if SELECT or DEMO N
				   has been received.  (This is kind of nasty.)
				 */

  /* =======================================================================
     subprocs
     ======================================================================= */

  XtIntervalId stderr_popup_timer;

};

/* This structure holds all the data that applies to the screen-specific parts
   of the display connection; if the display has multiple screens, there will
   be one of these for each screen.
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

  Widget toplevel_shell;

  /* =======================================================================
     blanking
     ======================================================================= */

  Window screensaver_window;	/* The window that will impersonate the root,
				   when the screensaver activates.  Note that
				   the window stored here may change, as we
				   destroy and recreate it on different
				   visuals. */
  Colormap cmap;		/* The colormap that goes with the window. */
  Bool install_cmap_p;		/* Whether this screen should have its own
                                   colormap installed, for whichever of several
                                   reasons.  This is definitive (even a false
                                   value here overrides prefs->install_cmap_p.)
                                 */
  Visual *current_visual;	/* The visual of the window. */
  int current_depth;		/* How deep the visual (and the window) are. */

  Visual *default_visual;	/* visual to use when none other specified */

  Window real_vroot;		/* The original virtual-root window. */
  Window real_vroot_value;	/* What was in the __SWM_VROOT property. */

  Cursor cursor;		/* A blank cursor that goes with the
				   real root window. */
  unsigned long black_pixel;	/* Black, allocated from `cmap'. */

  int blank_vp_x, blank_vp_y;   /* Where the virtual-scrolling viewport was
                                   when the screen went blank.  We need to
                                   prevent the X server from letting the mouse
                                   bump the edges to scroll while the screen
                                   is locked, so we reset to this when it has
                                   moved, and the lock dialog is up... */

# ifdef HAVE_MIT_SAVER_EXTENSION
  Window server_mit_saver_window;
# endif


  /* =======================================================================
     demoing
     ======================================================================= */

  Colormap demo_cmap;		/* The colormap that goes with the dialogs:
				   this might be the same as `cmap' so care
				   must be taken not to free it while it's
				   still in use. */

  /* =======================================================================
     timers
     ======================================================================= */

  int poll_mouse_last_root_x;		/* Used only when no server exts. */
  int poll_mouse_last_root_y;
  Window poll_mouse_last_child;
  unsigned int poll_mouse_last_mask;
  time_t poll_mouse_last_time;


  /* =======================================================================
     subprocs
     ======================================================================= */

  int current_hack;		/* Index into `prefs.screenhacks' */
  pid_t pid;

  int stderr_text_x;
  int stderr_text_y;
  int stderr_line_height;
  XFontStruct *stderr_font;
  GC stderr_gc;
  Window stderr_overlay_window;    /* Used if the server has overlay planes */
  Colormap stderr_cmap;
};


#endif /* __XSCREENSAVER_TYPES_H__ */
