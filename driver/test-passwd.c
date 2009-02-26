/* xscreensaver, Copyright (c) 1998 Jamie Zawinski <jwz@jwz.org>
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

#define WHICH 0

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <ctype.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>

#include "xscreensaver.h"
#include "resources.h"
#include "version.h"

char *progname = 0;
char *progclass = 0;
XrmDatabase db = 0;
saver_info *global_si_kludge;

FILE *real_stderr, *real_stdout;

void reset_stderr(saver_screen_info *ssi) {}
void clear_stderr(saver_screen_info *ssi) {}
void reset_watchdog_timer(saver_info *si, Bool on_p) {}
void monitor_power_on (saver_info *si) {}
void grab_keyboard_and_mouse (saver_info *si, Window w, Cursor c) {}
void ungrab_keyboard_and_mouse (saver_info *si) {}
Bool select_visual (saver_screen_info *ssi, const char *v) { return False; }
void raise_window (saver_info *si, Bool i, Bool b, Bool d) {}
void restore_real_vroot (saver_info *si) {}
void saver_exit (saver_info *si, int status, const char *core) { exit(status);}
const char * signal_name(int signal) { return "???"; }
Bool monitor_powered_on_p (saver_info *si) { return True; }
void start_notice_events_timer (saver_info *si, Window w) {}
void initialize_screensaver_window (saver_info *si) {}
void blank_screen (saver_info *si) {}
void unblank_screen (saver_info *si) {}
Bool handle_clientmessage (saver_info *si, XEvent *e, Bool u) { return False; }
int BadWindow_ehandler (Display *dpy, XErrorEvent *error) { exit(1); }
int write_init_file (saver_info *si) { return 0;}
Bool window_exists_p (Display *dpy, Window window) {return True;}

const char *blurb(void) { return progname; }
Atom XA_SCREENSAVER, XA_DEMO, XA_PREFS;


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


static char *
reformat_hack (const char *hack)
{
  int i;
  const char *in = hack;
  int indent = 13;
  char *h2 = (char *) malloc(strlen(in) + indent + 2);
  char *out = h2;

  while (isspace(*in)) in++;		/* skip whitespace */
  while (*in && !isspace(*in) && *in != ':')
    *out++ = *in++;			/* snarf first token */
  while (isspace(*in)) in++;		/* skip whitespace */

  if (*in == ':')
    *out++ = *in++;			/* copy colon */
  else
    {
      in = hack;
      out = h2;				/* reset to beginning */
    }

  *out = 0;

  while (isspace(*in)) in++;		/* skip whitespace */
  for (i = strlen(h2); i < indent; i++)	/* indent */
    *out++ = ' ';

  /* copy the rest of the line. */
  while (*in)
    {
      /* shrink all whitespace to one space, for the benefit of the "demo"
	 mode display.  We only do this when we can easily tell that the
	 whitespace is not significant (no shell metachars).
       */
      switch (*in)
	{
	case '\'': case '"': case '`': case '\\':
	  {
	    /* Metachars are scary.  Copy the rest of the line unchanged. */
	    while (*in)
	      *out++ = *in++;
	  }
	  break;
	case ' ': case '\t':
	  {
	    while (*in == ' ' || *in == '\t')
	      in++;
	    *out++ = ' ';
	  }
	  break;
	default:
	  *out++ = *in++;
	  break;
	}
    }
  *out = 0;

  /* strip trailing whitespace. */
  out = out-1;
  while (out > h2 && (*out == ' ' || *out == '\t' || *out == '\n'))
    *out-- = 0;

  return h2;
}


static void
get_screenhacks (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  int i = 0;
  int start = 0;
  int end = 0;
  int size;
  char *d;

  d = get_string_resource ("monoPrograms", "MonoPrograms");
  if (d && !*d) { free(d); d = 0; }
  if (!d)
    d = get_string_resource ("colorPrograms", "ColorPrograms");
  if (d && !*d) { free(d); d = 0; }

  if (d)
    {
      fprintf (stderr,
       "%s: the `monoPrograms' and `colorPrograms' resources are obsolete;\n\
	see the manual for details.\n", blurb());
      free(d);
    }

  d = get_string_resource ("programs", "Programs");

  if (p->screenhacks)
    {
      for (i = 0; i < p->screenhacks_count; i++)
	if (p->screenhacks[i])
	  free (p->screenhacks[i]);
      free(p->screenhacks);
      p->screenhacks = 0;
    }

  if (!d || !*d)
    {
      p->screenhacks_count = 0;
      p->screenhacks = 0;
      return;
    }

  size = strlen (d);


  /* Count up the number of newlines (which will be equal to or larger than
     the number of hacks.)
   */
  i = 0;
  for (i = 0; d[i]; i++)
    if (d[i] == '\n')
      i++;
  i++;

  p->screenhacks = (char **) calloc (sizeof (char *), i+1);

  /* Iterate over the lines in `d' (the string with newlines)
     and make new strings to stuff into the `screenhacks' array.
   */
  p->screenhacks_count = 0;
  while (start < size)
    {
      /* skip forward over whitespace. */
      while (d[start] == ' ' || d[start] == '\t' || d[start] == '\n')
	start++;

      /* skip forward to newline or end of string. */
      end = start;
      while (d[end] != 0 && d[end] != '\n')
	end++;

      /* null terminate. */
      d[end] = 0;

      p->screenhacks[p->screenhacks_count++] = reformat_hack (d + start);
      if (p->screenhacks_count >= i)
	abort();

      start = end+1;
    }

  if (p->screenhacks_count == 0)
    {
      free (p->screenhacks);
      p->screenhacks = 0;
    }
}



static char *fallback[] = {
#include "XScreenSaver_ad.h"
 0
};

int
main (int argc, char **argv)
{
  Widget toplevel_shell;
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

# ifdef SCO
  set_auth_parameters(argc, argv);
# endif /* SCO */

  if (! lock_init (argc, argv))	/* before hack_uid() for proper permissions */
    {
      si->locking_disabled_p = True;
      si->nolock_reason = "error getting password";
    }

  hack_uid (si);

  progclass = "XScreenSaver";

  toplevel_shell = XtAppInitialize (&si->app, progclass, 0, 0,
				    &argc, argv, fallback,
				    0, 0);

  si->dpy = XtDisplay (toplevel_shell);
  si->db = XtDatabase (si->dpy);
  si->default_screen->toplevel_shell = toplevel_shell;
  si->default_screen->screen = XtScreen(toplevel_shell);
  si->default_screen->default_visual =
    si->default_screen->current_visual =
      DefaultVisualOfScreen(si->default_screen->screen);
  si->default_screen->screensaver_window =
    RootWindowOfScreen(si->default_screen->screen);

  db = si->db;
  XtGetApplicationNameAndClass (si->dpy, &progname, &progclass);

  p->debug_p = True;
  p->verbose_p = True;
  p->timestamp_p = True;
  p->lock_p = True;

  p->fade_p	    = get_boolean_resource ("fade", "Boolean");
  p->unfade_p	    = get_boolean_resource ("unfade", "Boolean");
  p->fade_seconds   = 1000 * get_seconds_resource ("fadeSeconds", "Time");
  p->fade_ticks	    = get_integer_resource ("fadeTicks", "Integer");
  p->install_cmap_p = get_boolean_resource ("installColormap", "Boolean");
  p->nice_inferior  = get_integer_resource ("nice", "Nice");

  p->initial_delay   = 1000 * get_seconds_resource ("initialDelay", "Time");
  p->splash_duration = 1000 * get_seconds_resource ("splashDuration", "Time");
  p->timeout         = 1000 * get_minutes_resource ("timeout", "Time");
  p->lock_timeout    = 1000 * get_minutes_resource ("lockTimeout", "Time");
  p->cycle           = 1000 * get_minutes_resource ("cycle", "Time");
  p->passwd_timeout  = 1000 * get_seconds_resource ("passwdTimeout", "Time");
  p->passwd_timeout = 1000 * get_seconds_resource ("passwdTimeout", "Time");
  p->splash_duration = 1000 * get_seconds_resource ("splashDuration", "Time");
  p->shell = get_string_resource ("bourneShell", "BourneShell");
  p->help_url = get_string_resource("helpURL", "URL");
  p->load_url_command = get_string_resource("loadURL", "LoadURL");

  get_screenhacks(si);

  while (1)
    {
#if WHICH == 0
      if (unlock_p (si))
	fprintf (stderr, "%s: password correct\n", progname);
      else
	fprintf (stderr, "%s: password INCORRECT!\n", progname);

      XSync(si->dpy, False);
      sleep (3);
#elif WHICH == 1
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
#else
      si->demo_mode_p = True;
      make_screenhack_dialog (si);
      XtAppMainLoop(si->app);
#endif
    }
}
