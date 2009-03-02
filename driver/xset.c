/* xset.c --- interacting with server extensions and the builtin screensaver.
 * xscreensaver, Copyright (c) 1991-2005 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xos.h>

/* This file doesn't need the Xt headers, so stub these types out... */
#undef XtPointer
#define XtAppContext void*
#define XrmDatabase  void*
#define XtIntervalId void*
#define XtPointer    void*
#define Widget       void*

#include "xscreensaver.h"

#ifdef _VROOT_H_
ERROR!  You must not include vroot.h in this file.
#endif


/* MIT SCREEN-SAVER server extension hackery.
 */

#ifdef HAVE_MIT_SAVER_EXTENSION

# include <X11/extensions/scrnsaver.h>

Bool
query_mit_saver_extension (saver_info *si)
{
  return XScreenSaverQueryExtension (si->dpy,
				     &si->mit_saver_ext_event_number,
				     &si->mit_saver_ext_error_number);
}

static int
ignore_all_errors_ehandler (Display *dpy, XErrorEvent *error)
{
  return 0;
}

static void
init_mit_saver_extension (saver_info *si)
{
  int i;
  Pixmap *blank_pix = (Pixmap *) calloc (sizeof(Pixmap), si->nscreens);

  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      XID kill_id = 0;
      Atom kill_type = 0;
      Window root = RootWindowOfScreen (ssi->screen);
      blank_pix[i] = XCreatePixmap (si->dpy, root, 1, 1, 1);

      /* Kill off the old MIT-SCREEN-SAVER client if there is one.
	 This tends to generate X errors, though (possibly due to a bug
	 in the server extension itself?) so just ignore errors here. */
      if (XScreenSaverGetRegistered (si->dpy,
				     XScreenNumberOfScreen (ssi->screen),
				     &kill_id, &kill_type)
	  && kill_id != blank_pix[i])
	{
	  XErrorHandler old_handler =
	    XSetErrorHandler (ignore_all_errors_ehandler);
	  XKillClient (si->dpy, kill_id);
	  XSync (si->dpy, False);
	  XSetErrorHandler (old_handler);
	}
      XScreenSaverSelectInput (si->dpy, root, ScreenSaverNotifyMask);
      XScreenSaverRegister (si->dpy,
			    XScreenNumberOfScreen (ssi->screen),
			    (XID) blank_pix[i], XA_PIXMAP);
    }
  free(blank_pix);
}
#endif /* HAVE_MIT_SAVER_EXTENSION */


/* SGI SCREEN_SAVER server extension hackery.
 */

#ifdef HAVE_SGI_SAVER_EXTENSION

# include <X11/extensions/XScreenSaver.h>

Bool
query_sgi_saver_extension (saver_info *si)
{
  return XScreenSaverQueryExtension (si->dpy,
				     &si->sgi_saver_ext_event_number,
				     &si->sgi_saver_ext_error_number);
}

static void
init_sgi_saver_extension (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  int i;
  if (si->screen_blanked_p)
    /* If you mess with this while the server thinks it's active,
       the server crashes. */
    return;

  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      XScreenSaverDisable (si->dpy, XScreenNumberOfScreen(ssi->screen));
      if (! XScreenSaverEnable (si->dpy, XScreenNumberOfScreen(ssi->screen)))
	{
	  fprintf (stderr,
       "%s: SGI SCREEN_SAVER extension exists, but can't be initialized;\n\
		perhaps some other screensaver program is already running?\n",
		   blurb());
	  si->using_sgi_saver_extension = False;
	  return;
	}
    }
}

#endif /* HAVE_SGI_SAVER_EXTENSION */


/* XIDLE server extension hackery.
 */

#ifdef HAVE_XIDLE_EXTENSION

# include <X11/extensions/xidle.h>

Bool
query_xidle_extension (saver_info *si)
{
  int event_number;
  int error_number;
  return XidleQueryExtension (si->dpy, &event_number, &error_number);
}

#endif /* HAVE_XIDLE_EXTENSION */



/* Resize and Rotate server extension hackery.
 */

#ifdef HAVE_RANDR

# include <X11/extensions/Xrandr.h>

Bool
query_randr_extension (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  Bool ok = XRRQueryExtension (si->dpy,
                               &si->randr_event_number,
                               &si->randr_error_number);

  if (ok)
    {
      int nscreens = ScreenCount (si->dpy);  /* number of *real* screens */
      int i;

      if (p->verbose_p)
	fprintf (stderr, "%s: selecting RANDR events\n", blurb());
      for (i = 0; i < nscreens; i++)
# ifdef RRScreenChangeNotifyMask                 /* randr.h 1.5, 2002/09/29 */
        XRRSelectInput (si->dpy, RootWindow (si->dpy, i),
                        RRScreenChangeNotifyMask);
# else  /* !RRScreenChangeNotifyMask */          /* Xrandr.h 1.4, 2001/06/07 */
        XRRScreenChangeSelectInput (si->dpy, RootWindow (si->dpy, i), True);
# endif /* !RRScreenChangeNotifyMask */
    }

  return ok;
}

#endif /* HAVE_RANDR */



/* Figuring out what the appropriate XSetScreenSaver() parameters are
   (one wouldn't expect this to be rocket science.)
 */

void
disable_builtin_screensaver (saver_info *si, Bool unblank_screen_p)
{
  saver_preferences *p = &si->prefs;
  int current_server_timeout, current_server_interval;
  int current_prefer_blank, current_allow_exp;
  int desired_server_timeout, desired_server_interval;
  int desired_prefer_blank, desired_allow_exp;

  XGetScreenSaver (si->dpy, &current_server_timeout, &current_server_interval,
		   &current_prefer_blank, &current_allow_exp);

  desired_server_timeout = current_server_timeout;
  desired_server_interval = current_server_interval;
  desired_prefer_blank = current_prefer_blank;
  desired_allow_exp = current_allow_exp;

  /* On SGIs, if interval is non-zero, it is the number of seconds after
     screen saving starts at which the monitor should be powered down.
     Obviously I don't want that, so set it to 0 (meaning "never".)

     Power saving is disabled if DontPreferBlanking, but in that case,
     we don't get extension events either.  So we can't turn it off that way.

     Note: if you're running Irix 6.3 (O2), you may find that your monitor is
     powering down anyway, regardless of the xset settings.  This is fixed by
     installing SGI patches 2447 and 2537.
   */
  desired_server_interval = 0;

  /* I suspect (but am not sure) that DontAllowExposures might have
     something to do with powering off the monitor as well, at least
     on some systems that don't support XDPMS?  Who knows... */
  desired_allow_exp = AllowExposures;

  if (si->using_mit_saver_extension || si->using_sgi_saver_extension)
    {
      desired_server_timeout = (p->timeout / 1000);

      /* The SGI extension won't give us events unless blanking is on.
	 I think (unsure right now) that the MIT extension is the opposite. */
      if (si->using_sgi_saver_extension)
	desired_prefer_blank = PreferBlanking;
      else
	desired_prefer_blank = DontPreferBlanking;
    }
  else
    {
      /* When we're not using an extension, set the server-side timeout to 0,
	 so that the server never gets involved with screen blanking, and we
	 do it all ourselves.  (However, when we *are* using an extension,
	 we tell the server when to notify us, and rather than blanking the
	 screen, the server will send us an X event telling us to blank.)
       */
      desired_server_timeout = 0;
    }

  /* XSetScreenSaver() generates BadValue if either timeout parameter
     exceeds 15 bits (signed short.)  That is 09:06:07.
   */
  if (desired_server_timeout  > 0x7FFF) desired_server_timeout  = 0x7FFF;
  if (desired_server_interval > 0x7FFF) desired_server_interval = 0x7FFF;

  if (desired_server_timeout != current_server_timeout ||
      desired_server_interval != current_server_interval ||
      desired_prefer_blank != current_prefer_blank ||
      desired_allow_exp != current_allow_exp)
    {
      if (p->verbose_p)
	fprintf (stderr,
                 "%s: disabling server builtin screensaver:\n"
                 "%s:  (xset s %d %d; xset s %s; xset s %s)\n",
                 blurb(), blurb(),
		 desired_server_timeout, desired_server_interval,
		 (desired_prefer_blank ? "blank" : "noblank"),
		 (desired_allow_exp ? "expose" : "noexpose"));

      XSetScreenSaver (si->dpy,
		       desired_server_timeout, desired_server_interval,
		       desired_prefer_blank, desired_allow_exp);
      XSync(si->dpy, False);
    }


#if defined(HAVE_MIT_SAVER_EXTENSION) || defined(HAVE_SGI_SAVER_EXTENSION)
  {
    static Bool extension_initted = False;
    if (!extension_initted)
      {
	extension_initted = True;
# ifdef HAVE_MIT_SAVER_EXTENSION
	if (si->using_mit_saver_extension) init_mit_saver_extension(si);
# endif
# ifdef HAVE_SGI_SAVER_EXTENSION
	if (si->using_sgi_saver_extension) init_sgi_saver_extension(si);
# endif
      }
  }
#endif /* HAVE_MIT_SAVER_EXTENSION || HAVE_SGI_SAVER_EXTENSION */

  if (unblank_screen_p)
    /* Turn off the server builtin saver if it is now running. */
    XForceScreenSaver (si->dpy, ScreenSaverReset);
}
