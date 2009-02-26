/* xscreensaver, Copyright (c) 1993-1998 Jamie Zawinski <jwz@netscape.com>
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

extern char *progname;
extern char *progclass;

typedef struct saver_preferences saver_preferences;
typedef struct saver_info saver_info;
typedef struct saver_screen_info saver_screen_info;


#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))



/* This structure holds all the user-specified parameters, read from the
   command line, the resource database, or entered through a dialog box.
 */
struct saver_preferences {
  Bool verbose_p;
  Bool lock_p;			/* whether to lock as well as save */

  Bool debug_p;
  Bool fade_p;			/* whether to fade to black */
  Bool unfade_p;		/* whether to fade from black */
  int fade_seconds;		/* how long that should take */
  int fade_ticks;		/* how many ticks should be used */

  Bool install_cmap_p;		/* whether we should use our own colormap
				   when using the screen's default visual. */

  char **screenhacks;		/* the programs to run */
  int screenhacks_count;

  int nice_inferior;		/* nice value for subprocs */

  int initial_delay;		/* how long to sleep after launch */
  Time timeout;			/* how much idle time before activation */
  Time lock_timeout;		/* how long after activation locking starts */
  Time cycle;			/* how long each hack should run */
#ifndef NO_LOCKING
  Time passwd_timeout;		/* how much time before pw dialog goes down */
#endif
  Time pointer_timeout;		/* how often to check mouse position */
  Time notice_events_timeout;	/* how long after window creation to select */
  Time watchdog_timeout;	/* how often to re-raise and re-blank screen */

  Bool use_xidle_extension;	/* which extension to use, if possible */
  Bool use_mit_saver_extension;
  Bool use_sgi_saver_extension;

  char *shell;			/* where to find /bin/sh */

};


/* This structure holds all the data that applies to the program as a whole,
   or to the non-screen-specific parts of the display connection.
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
  XrmDatabase db;

  /* =======================================================================
     server extension info
     ======================================================================= */

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
     locking
     ======================================================================= */

  Bool locking_disabled_p;	/* Sometimes locking is impossible. */
  char *nolock_reason;		/* This is why. */
  Bool locked_p;		/* Whether the screen is currently locked. */
  Bool dbox_up_p;		/* Whether the demo-mode or passwd dialogs
				   are currently visible */

  /* =======================================================================
     demoing
     ======================================================================= */

  Bool demo_mode_p;		/* Whether demo-mode is active */
  char *demo_hack;		/* The hack that has been selected from the
				   dialog box, which should be run next. */


  /* =======================================================================
     asking questions
     ======================================================================= */

  Bool question_up_p;		/* Whether the question dialog is currently
				   visible. */
  Widget question_dialog;	/* The question dialog, if any. */


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

  int next_mode_p;		/* Set to 1 if the NEXT ClientMessage has just
				   been received; set to 2 if PREV has just
				   been received.  (#### This is nasty.) */

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
  Bool install_cmap_p;		/* whether we should use our own colormap.
				   This can be overridden on a per-hack basis.
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
extern void disable_builtin_screensaver (saver_info *si, Bool turn_off_p);
extern Bool ensure_no_screensaver_running (Display *, Screen *);

#ifdef HAVE_MIT_SAVER_EXTENSION
extern Bool query_mit_saver_extension (saver_info *);
#endif
#ifdef HAVE_SGI_SAVER_EXTENSION
extern Bool query_sgi_saver_extension (saver_info *);
#endif


/* =======================================================================
   blanking
   ======================================================================= */

extern void initialize_screensaver_window (saver_info *si);
extern void raise_window (saver_info *si,
			    Bool inhibit_fade, Bool between_hacks_p,
			    Bool dont_clear);
extern void blank_screen (saver_info *si);
extern void unblank_screen (saver_info *si);
extern void grab_keyboard_and_mouse (saver_info *si, Window, Cursor);
extern void ungrab_keyboard_and_mouse (saver_info *si);

/* =======================================================================
   locking
   ======================================================================= */

#ifndef NO_LOCKING
extern Bool unlock_p (saver_info *si);
extern create_passwd_dialog (Widget, Visual *, Colormap);
extern Bool lock_init (int argc, char **argv);
extern Bool passwd_valid_p (const char *typed_passwd);
#endif

/* =======================================================================
   demoing
   ======================================================================= */

#ifndef NO_DEMO_MODE
extern void demo_mode (saver_info *si);
extern void demo_mode_restart_process (saver_info *si);
extern create_demo_dialog (Widget, Visual *, Colormap);
extern create_resources_dialog (Widget, Visual *, Colormap);
#endif

#if !defined(NO_LOCKING) || !defined(NO_DEMO_MODE)
extern void pop_up_dialog_box (Widget dialog, Widget form, int where);
extern void format_into_label (Widget label, const char *arg);
extern void steal_focus_and_colormap (Widget dialog);
#endif

#ifdef HAVE_MOTIF
extern void disable_motif_drag_and_drop(Widget w);
#endif


/* =======================================================================
   timers
   ======================================================================= */

extern void start_notice_events_timer (saver_info *, Window);
extern void cycle_timer (XtPointer si, XtIntervalId *id);
extern void activate_lock_timer (XtPointer si, XtIntervalId *id);
extern void reset_watchdog_timer (saver_info *si, Bool on_p);
extern void idle_timer (XtPointer si, XtIntervalId *id);
extern void sleep_until_idle (saver_info *si, Bool until_idle_p);

/* =======================================================================
   remote control
   ======================================================================= */

extern Bool handle_clientmessage (saver_info *, XEvent *, Bool);

/* =======================================================================
   subprocs
   ======================================================================= */

extern void init_sigchld (void);
extern void spawn_screenhack (saver_info *si, Bool first_time_p);
extern void kill_screenhack (saver_info *si);
extern void suspend_screenhack (saver_info *si, Bool suspend_p);
extern Bool screenhack_running_p (saver_info *si);
extern void emergency_kill_subproc (saver_info *si);
extern Bool select_visual (saver_screen_info *ssi, const char *visual_name);
extern const char *signal_name (int signal);

/* =======================================================================
   subprocs security
   ======================================================================= */

#ifdef NO_SETUID
# define hack_uid()
# define hack_uid_warn()
#else /* !NO_SETUID */
 extern void hack_uid (saver_info *si);
 extern void hack_uid_warn (saver_info *si);
#endif /* NO_SETUID */

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

extern void save_argv (int argc, char **argv);
extern void saver_exit (saver_info *si, int status);
extern void restart_process (saver_info *si);

extern int saver_ehandler (Display *dpy, XErrorEvent *error);
extern int BadWindow_ehandler (Display *dpy, XErrorEvent *error);
extern Bool window_exists_p (Display *dpy, Window window);
extern char *timestring (void);

extern Atom XA_VROOT, XA_XSETROOT_ID;
extern Atom XA_SCREENSAVER, XA_SCREENSAVER_VERSION, XA_SCREENSAVER_ID;
extern Atom XA_SCREENSAVER_TIME;

#endif /* __XSCREENSAVER_H__ */
