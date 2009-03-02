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

#include "prefs.h"

extern char *progname;
extern char *progclass;

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))



/* =======================================================================
   server extensions and virtual roots
   ======================================================================= */

extern Bool restore_real_vroot (saver_info *si);
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
#ifdef HAVE_RANDR
extern Bool query_randr_extension (saver_info *);
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

extern Bool update_screen_layout (saver_info *si);
extern void initialize_screensaver_window (saver_info *si);
extern void initialize_screen_root_widget (saver_screen_info *ssi);

extern void raise_window (saver_info *si,
			    Bool inhibit_fade, Bool between_hacks_p,
			    Bool dont_clear);
extern Bool blank_screen (saver_info *si);
extern void unblank_screen (saver_info *si);
extern void resize_screensaver_window (saver_info *si);

extern void get_screen_viewport (saver_screen_info *ssi,
                                 int *x_ret, int *y_ret,
                                 int *w_ret, int *h_ret,
                                 int target_x, int target_y,
                                 Bool verbose_p);


/* =======================================================================
   locking
   ======================================================================= */

#ifndef NO_LOCKING
extern Bool unlock_p (saver_info *si);
extern Bool lock_priv_init (int argc, char **argv, Bool verbose_p);
extern Bool lock_init (int argc, char **argv, Bool verbose_p);
extern Bool passwd_valid_p (const char *typed_passwd, Bool verbose_p);
#endif /* NO_LOCKING */

extern void set_locked_p (saver_info *si, Bool locked_p);
extern int move_mouse_grab (saver_info *si, Window to, Cursor cursor,
                            int to_screen_no);
extern int mouse_screen (saver_info *si);


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


/* =======================================================================
   timers
   ======================================================================= */

extern void start_notice_events_timer (saver_info *, Window, Bool verbose_p);
extern void cycle_timer (XtPointer si, XtIntervalId *id);
extern void activate_lock_timer (XtPointer si, XtIntervalId *id);
extern void reset_watchdog_timer (saver_info *si, Bool on_p);
extern void idle_timer (XtPointer si, XtIntervalId *id);
extern void de_race_timer (XtPointer si, XtIntervalId *id);
extern void sleep_until_idle (saver_info *si, Bool until_idle_p);
extern void reset_timers (saver_info *si);
extern void schedule_wakeup_event (saver_info *si, Time when, Bool verbose_p);


/* =======================================================================
   remote control
   ======================================================================= */

extern Bool handle_clientmessage (saver_info *, XEvent *, Bool);
extern void maybe_reload_init_file (saver_info *);

/* =======================================================================
   subprocs
   ======================================================================= */

extern void handle_signals (saver_info *si);
#ifdef HAVE_SIGACTION
 extern sigset_t block_sigchld (void);
#else  /* !HAVE_SIGACTION */
 extern int block_sigchld (void);
#endif /* !HAVE_SIGACTION */
extern void unblock_sigchld (void);
extern void hack_environment (saver_info *si);
extern void hack_subproc_environment (Screen *, Window saver_window);
extern void init_sigchld (void);
extern void spawn_screenhack (saver_screen_info *ssi);
extern pid_t fork_and_exec (saver_screen_info *ssi, const char *command);
extern void kill_screenhack (saver_screen_info *ssi);
extern void suspend_screenhack (saver_screen_info *ssi, Bool suspend_p);
extern Bool screenhack_running_p (saver_info *si);
extern void emergency_kill_subproc (saver_info *si);
extern Bool select_visual (saver_screen_info *ssi, const char *visual_name);
extern void store_saver_status (saver_info *si);
extern const char *signal_name (int signal);

/* =======================================================================
   subprocs diagnostics
   ======================================================================= */

extern FILE *real_stderr;
extern FILE *real_stdout;
extern void stderr_log_file (saver_info *si);
extern void initialize_stderr (saver_info *si);
extern void reset_stderr (saver_screen_info *ssi);
extern void clear_stderr (saver_screen_info *ssi);
extern void shutdown_stderr (saver_info *si);


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
extern Visual *get_best_gl_visual (saver_info *si, Screen *screen);
extern void check_for_leaks (const char *where);
extern void describe_monitor_layout (saver_info *si);

#ifdef HAVE_XF86VMODE
Bool safe_XF86VidModeGetViewPort (Display *, int, int *, int *);
#endif /* HAVE_XF86VMODE */

extern Atom XA_VROOT, XA_XSETROOT_ID, XA_ESETROOT_PMAP_ID, XA_XROOTPMAP_ID;
extern Atom XA_SCREENSAVER, XA_SCREENSAVER_VERSION, XA_SCREENSAVER_ID;
extern Atom XA_SCREENSAVER_STATUS, XA_LOCK, XA_BLANK;
extern Atom XA_DEMO, XA_PREFS;

#endif /* __XSCREENSAVER_H__ */
