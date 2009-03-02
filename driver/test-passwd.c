/* xscreensaver, Copyright (c) 1998, 2001 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This is a kludgy test harness for debugging the password dialog box.
   It's somewhat easier to debug it here than in the xscreensaver executable
   itself.
 */

#define WHICH_PASS   100
#define WHICH_SPLASH 101
#define WHICH_TTY    102

#define WHICH        WHICH_PASS

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <ctype.h>
#include <pwd.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>

#include "xscreensaver.h"
#include "resources.h"
#include "version.h"
#include "visual.h"

char *progname = 0;
char *progclass = 0;
XrmDatabase db = 0;
saver_info *global_si_kludge;

FILE *real_stderr, *real_stdout;

void monitor_power_on (saver_info *si) {}
Bool monitor_powered_on_p (saver_info *si) { return True; }
void initialize_screensaver_window (saver_info *si) {}
void raise_window (saver_info *si, Bool i, Bool b, Bool d) {}
Bool blank_screen (saver_info *si) {return False;}
void unblank_screen (saver_info *si) {}
Bool select_visual (saver_screen_info *ssi, const char *v) { return False; }
Bool window_exists_p (Display *dpy, Window window) {return True;}
void start_notice_events_timer (saver_info *si, Window w, Bool b) {}
Bool handle_clientmessage (saver_info *si, XEvent *e, Bool u) { return False; }
int BadWindow_ehandler (Display *dpy, XErrorEvent *error) { exit(1); }
const char *signal_name(int signal) { return "???"; }
void restore_real_vroot (saver_info *si) {}
void store_saver_status (saver_info *si) {}
void saver_exit (saver_info *si, int status, const char *core) { exit(status);}
int move_mouse_grab (saver_info *si, Window to, Cursor cursor) { return 0; }

const char *blurb(void) { return progname; }
Atom XA_SCREENSAVER, XA_DEMO, XA_PREFS;

void
get_screen_viewport (saver_screen_info *ssi,
                     int *x_ret, int *y_ret,
                     int *w_ret, int *h_ret,
                     Bool verbose_p)
{
  *x_ret = 0;
  *y_ret = 0;
  *w_ret = WidthOfScreen (ssi->screen);
  *h_ret = HeightOfScreen (ssi->screen);
}


void
idle_timer (XtPointer closure, XtIntervalId *id)
{
  saver_info *si = (saver_info *) closure;
  XEvent fake_event;
  fake_event.type = 0;	/* XAnyEvent type, ignored. */
  fake_event.xany.display = si->dpy;
  fake_event.xany.window  = 0;
  XPutBackEvent (si->dpy, &fake_event);
}


static char *fallback[] = {
#include "XScreenSaver_ad.h"
 0
};

int
main (int argc, char **argv)
{
  Widget toplevel_shell = 0;
  saver_screen_info ssip;
  saver_info sip;
  saver_info *si = &sip;
  saver_preferences *p = &si->prefs;

  memset(&sip, 0, sizeof(sip));
  memset(&ssip, 0, sizeof(ssip));

  si->nscreens = 1;
  si->screens = si->default_screen = &ssip;
  ssip.global = si;

  global_si_kludge = si;
  real_stderr = stderr;
  real_stdout = stdout;

  si->version = (char *) malloc (5);
  memcpy (si->version, screensaver_id + 17, 4);
  progname = argv[0];
  {
    char *s = strrchr(progname, '/');
    if (*s) strcpy (progname, s+1);
  }

  /* before hack_uid() for proper permissions */
  lock_priv_init (argc, argv, True);

  hack_uid (si);

  if (! lock_init (argc, argv, si->prefs.verbose_p))
    {
      si->locking_disabled_p = True;
      si->nolock_reason = "error getting password";
    }

  progclass = "XScreenSaver";

#if (WHICH != WHICH_TTY)
  toplevel_shell = XtAppInitialize (&si->app, progclass, 0, 0,
				    &argc, argv, fallback,
				    0, 0);

  si->dpy = XtDisplay (toplevel_shell);
  p->db = XtDatabase (si->dpy);
  si->default_screen->toplevel_shell = toplevel_shell;
  si->default_screen->screen = XtScreen(toplevel_shell);
  si->default_screen->default_visual =
    si->default_screen->current_visual =
      DefaultVisualOfScreen(si->default_screen->screen);
  si->default_screen->screensaver_window =
    RootWindowOfScreen(si->default_screen->screen);
  si->default_screen->current_depth =
    visual_depth(si->default_screen->screen,
                 si->default_screen->current_visual);

  db = p->db;
  XtGetApplicationNameAndClass (si->dpy, &progname, &progclass);

  load_init_file (&si->prefs);

#endif /* (WHICH != 2) */

  p->verbose_p = True;

  while (1)
    {
#if WHICH == WHICH_PASS
      if (unlock_p (si))
	fprintf (stderr, "%s: password correct\n", progname);
      else
	fprintf (stderr, "%s: password INCORRECT!\n", progname);

      XSync(si->dpy, False);
      sleep (3);
#elif WHICH == WHICH_SPLASH
      {
	XEvent event;
	make_splash_dialog (si);
	XtAppAddTimeOut (si->app, p->splash_duration + 1000,
			 idle_timer, (XtPointer) si);
	while (si->splash_dialog)
	  {
	    XtAppNextEvent (si->app, &event);
	    if (event.xany.window == si->splash_dialog)
	      handle_splash_event (si, &event);
	    XtDispatchEvent (&event);
	  }
	XSync (si->dpy, False);
	sleep (1);
      }
#elif WHICH == WHICH_TTY
      {
        char *pass;
        char buf[255];
        struct passwd *p = getpwuid (getuid ());
        printf ("\n%s: %s's password: ", progname, p->pw_name);

        pass = fgets (buf, sizeof(buf)-1, stdin);
        if (!pass || !*pass)
          exit (0);
        if (pass[strlen(pass)-1] == '\n')
          pass[strlen(pass)-1] = 0;

        if (passwd_valid_p (pass, True))
          printf ("%s: Ok!\n", progname);
        else
          printf ("%s: Wrong!\n", progname);
      }
#else
# error bogus WHICH value!
#endif
    }
}
