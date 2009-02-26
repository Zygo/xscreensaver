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

#ifndef __XSCREENSAVER_H__
#define __XSCREENSAVER_H__

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <string.h>
#include <stdio.h>

#include "prefs.h"

extern char *progname;
extern char *progclass;

typedef struct saver_info saver_info;
typedef struct saver_screen_info saver_screen_info;
typedef struct passwd_dialog_data passwd_dialog_data;
typedef struct splash_dialog_data splash_dialog_data;

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))



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
  saver_screen_info *screens;
  saver_screen_info *default_screen;	/* ...on which dialogs will appear. */


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


  /* =======================================================================
     blanking
     ======================================================================= */

  Bool screen_blanked_p;	/* Whether the saver is currently active. */
  Window mouse_grab_window;	/* Window holding our mouse grab */
  Window keyboard_grab_window;	/* Window holding our keyboard grab */


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

  Window passwd_dialog;		/* The password dialog, if its up. */
  passwd_dialog_data *pw_data;	/* Other info necessary to draw it. */

  int unlock_failures;		/* Counts failed login attempts while the
				   screen is locked. */


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

  time_t last_activity_time;		   /* Used only when no server exts. */
  saver_screen_info *last_activity_screen;


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

  Screen *screen;
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
  Visual *default_visual;	/* visual to use when none other specified */
  int current_depth;		/* How deep the visual (and the window) are. */

  Window real_vroot;		/* The original virtual-root window. */
  Window real_vroot_value;	/* What was in the __SWM_VROOT property. */

  Cursor cursor;		/* A blank cursor that goes with the
				   real root window. */
  unsigned long black_pixel;	/* Black, allocated from `cmap'. */

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




/* =======================================================================
   server extensions and virtual roots
   ======================================================================= */

extern void restore_real_vroot (saver_info *si);
extern void disable_builtin_screensaver (saver_info *, Bool unblank_screen_p);
extern Bool ensure_no_screensaver_running (Display *, Screen *);

#ifdef HAVE_MIT_SAVER_EXTENSION
extern Bool query_mit_saver_extension (saver_info *);
#endif
#ifdef HAVE_SGI_SAVER_EXTENSION
extern Bool query_sgi_saver_extension (saver_info *);
#endif
#ifdef HAVE_XIDLE_EXTENSION
extern Bool query_xidle_extension (saver_info *);
#endif
#ifdef HAVE_PROC_INTERRUPTS
extern Bool query_proc_interrupts_available (saver_info *, const char **why);
#endif

/* Display Power Management System (DPMS) interface. */
extern Bool monitor_powered_on_p (saver_info *si);
extern void monitor_power_on (saver_info *si);


/* =======================================================================
   blanking
   ======================================================================= */

extern void initialize_screensaver_window (saver_info *si);
extern void raise_window (saver_info *si,
			    Bool inhibit_fade, Bool between_hacks_p,
			    Bool dont_clear);
extern void blank_screen (saver_info *si);
extern void unblank_screen (saver_info *si);
extern Bool grab_keyboard_and_mouse (saver_info *si, Window, Cursor);
extern void ungrab_keyboard_and_mouse (saver_info *si);

/* =======================================================================
   locking
   ======================================================================= */

#ifndef NO_LOCKING
extern Bool unlock_p (saver_info *si);
extern Bool lock_init (int argc, char **argv, Bool verbose_p);
extern Bool passwd_valid_p (const char *typed_passwd, Bool verbose_p);

extern void make_passwd_window (saver_info *si);
extern void draw_passwd_window (saver_info *si);
extern void update_passwd_window (saver_info *si, const char *printed_passwd,
				  float ratio);
extern void destroy_passwd_window (saver_info *si);

#endif /* NO_LOCKING */

/* =======================================================================
   runtime privileges
   ======================================================================= */

extern void hack_uid (saver_info *si);
extern void describe_uids (saver_info *si, FILE *out);

/* =======================================================================
   demoing
   ======================================================================= */

extern void draw_shaded_rectangle (Display *dpy, Window window,
				   int x, int y,
				   int width, int height,
				   int thickness,
				   unsigned long top_color,
				   unsigned long bottom_color);
extern int string_width (XFontStruct *font, char *s);

extern void make_splash_dialog (saver_info *si);
extern void handle_splash_event (saver_info *si, XEvent *e);
extern void skull (Display *, Window, GC, GC, int, int, int, int);


/* =======================================================================
   timers
   ======================================================================= */

extern void start_notice_events_timer (saver_info *, Window, Bool verbose_p);
extern void cycle_timer (XtPointer si, XtIntervalId *id);
extern void activate_lock_timer (XtPointer si, XtIntervalId *id);
extern void reset_watchdog_timer (saver_info *si, Bool on_p);
extern void idle_timer (XtPointer si, XtIntervalId *id);
extern void sleep_until_idle (saver_info *si, Bool until_idle_p);

/* =======================================================================
   remote control
   ======================================================================= */

extern Bool handle_clientmessage (saver_info *, XEvent *, Bool);
extern void maybe_reload_init_file (saver_info *);

/* =======================================================================
   subprocs
   ======================================================================= */

extern void hack_environment (saver_info *si);
extern void hack_subproc_environment (saver_screen_info *ssi);
extern void init_sigchld (void);
extern void spawn_screenhack (saver_info *si, Bool first_time_p);
extern void kill_screenhack (saver_info *si);
extern void suspend_screenhack (saver_info *si, Bool suspend_p);
extern Bool screenhack_running_p (saver_info *si);
extern void emergency_kill_subproc (saver_info *si);
extern Bool select_visual (saver_screen_info *ssi, const char *visual_name);
extern const char *signal_name (int signal);

/* =======================================================================
   subprocs diagnostics
   ======================================================================= */

extern FILE *real_stderr;
extern FILE *real_stdout;
extern void initialize_stderr (saver_info *si);
extern void reset_stderr (saver_screen_info *ssi);
extern void clear_stderr (saver_screen_info *ssi);


/* =======================================================================
   misc
   ======================================================================= */

extern const char *blurb (void);
extern void save_argv (int argc, char **argv);
extern void saver_exit (saver_info *si, int status, const char *core_reason);
extern void restart_process (saver_info *si);

extern int saver_ehandler (Display *dpy, XErrorEvent *error);
extern int BadWindow_ehandler (Display *dpy, XErrorEvent *error);
extern Bool window_exists_p (Display *dpy, Window window);
extern char *timestring (void);
extern Bool display_is_on_console_p (saver_info *si);

extern Atom XA_VROOT, XA_XSETROOT_ID;
extern Atom XA_SCREENSAVER, XA_SCREENSAVER_VERSION, XA_SCREENSAVER_ID;
extern Atom XA_SCREENSAVER_TIME;
extern Atom XA_DEMO, XA_PREFS;

#endif /* __XSCREENSAVER_H__ */
