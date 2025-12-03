/* xscreensaver, Copyright © 1992-2025 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* There are several different mechanisms here for fading the desktop to
   black, and then fading it back in again.

   - Colormaps: This only works on 8-bit displays, which basically haven't
     existed since the 90s.  It takes the current colormap, makes a writable
     copy of it, and then animates the color cells to fade and unfade.

   - XF86 Gamma or RANDR Gamma: These do the fade by altering the brightness
     settings of the screen.  This works on any system that has the "XF86
     Video-Mode" extension (which is every modern system) AND ALSO has gamma
     support in the video driver.  But it turns out that as of 2022, the
     Raspberry Pi HDMI video driver still does not support gamma.  And there's
     no way to determine that the video driver lacks gamma support even though
     the extension exists.  Since the Pi is probably the single most popular
     platform for running X11 on the desktop these days, that makes this
     method pretty much useless now.

   - SGI VC: Same as the above, but only works on SGI.

   - XSHM: This works by taking a screenshot and hacking the bits by hand.
     It's slow.  Also, in order to fade in from black to the desktop (possibly
     hours after it faded out) it has to retain that first screenshot of the
     desktop to fade back to.  But if the desktop had changed in the meantime,
     there will be a glitch at the end as it snaps from the screenshot to the
     new current reality.  And under Wayland, it has to get that screenshot
     by running "xscreensaver-getimage" which in turn runs "grim", if it is
     installed, which it might not be.

   - OpenGL: This is like XSHM but uses OpenGL for rendering.  Faster, but
     same screenshot- and Wayland-related downsides.

   In summary, everything is terrible because X11 doesn't support alpha.


   The fade process goes like this:

     Screen saver activates:
  
       - Desktop is visible
       - Save screenshot for later
       - Fade out:
         - Map invisible temp windows
         - Fade from desktop to black
         - Erase saver windows to black and raise them
         - Destroy temp windows
  
     Screen saver deactivates:
  
       - Saver graphics are visible
       - Fade out:
         - Get a screenshot of the current screenhack
         - Map invisible temp windows
         - Fade from screenhack screenshot to black
       - Fade in:
         - Fade from black to saved screenshot of desktop
         - Unmap obscured saver windows
         - Destroy temp windows
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>		/* for waitpid() and associated macros */
#endif

#ifdef HAVE_JWXYZ
# include "jwxyz.h"
#else /* real X11 */
# include <X11/Xlib.h>
# include <X11/Xatom.h>
# include <X11/Xproto.h>
# include <X11/Intrinsic.h>
#endif /* !HAVE_JWXYZ */

#ifdef USE_GL
# include <GL/gl.h>
# include <GL/glu.h>
# ifdef HAVE_EGL
#  include <EGL/egl.h>
#  include <EGL/eglext.h>
# else
#  include <GL/glx.h>
# endif
#endif /* USE_GL */

#include "blurb.h"
#include "visual.h"
#ifdef USE_GL
#include "visual-gl.h"
#endif
#include "usleep.h"
#include "fade.h"
#include "xshm.h"
#include "atoms.h"
#include "clientmsg.h"
#include "xmu.h"
#include "pow2.h"
#include "doubletime.h"
#include "screenshot.h"

/* Since gamma fading doesn't work on the Raspberry Pi, probably the single
   most popular desktop Linux system these days, let's not use this fade
   method even if the extension exists (which it does).
 */
#undef HAVE_XF86VMODE_GAMMA

/* I'm not sure that the RANDR fade method brings anything to the party
   that the XF86 method does  See below.
 */
#undef HAVE_RANDR_12

#ifndef HAVE_XINPUT
# error The XInput2 extension is required
#endif

#include <X11/extensions/XInput2.h>
#include "xinput.h"

#if defined(__APPLE__) && !defined(HAVE_COCOA)
# define HAVE_MACOS_X11
#elif !defined(HAVE_JWXYZ) /* Real X11, possibly Wayland */
# define HAVE_REAL_X11
#endif


typedef struct {
  int nscreens;
  Pixmap *screenshots;
} fade_state;


#ifdef HAVE_SGI_VC_EXTENSION
static int sgi_gamma_fade (XtAppContext, Display *, Window *wins, int count,
                           double secs, Bool out_p);
#endif
#ifdef HAVE_XF86VMODE_GAMMA
static int xf86_gamma_fade (XtAppContext, Display *, Window *wins, int count,
                            double secs, Bool out_p);
#endif
#ifdef HAVE_RANDR_12
static int randr_gamma_fade (XtAppContext, Display *, Window *wins, int count,
                             double secs, Bool out_p);
#endif
static int colormap_fade (XtAppContext, Display *, Window *wins, int count, 
                          double secs, Bool out_p, Bool from_desktop_p);
static int xshm_fade (XtAppContext, Display *,
                      Window *wins, int count, double secs,
                      Bool out_p, Bool from_desktop_p, fade_state *);


#ifdef HAVE_XINPUT
static int xi_opcode = -1;
#endif

/* Closure arg points to a Bool saying whether motion events count.
   Motion aborts fade-out, but only clicks and keys abort fade-in.
 */
static Bool
user_event_p (Display *dpy, XEvent *event, XPointer arg)
{
  Bool motion_p = *((Bool *) arg);

  switch (event->xany.type) {
  case KeyPress: case ButtonPress:
    return True;
    break;
  case MotionNotify:
    if (motion_p) return True;
    break;
# ifdef HAVE_XINPUT
  case GenericEvent:
    {
      XIRawEvent *re;
      if (event->xcookie.extension != xi_opcode)
        return False;
      if (! event->xcookie.data)
        XGetEventData (dpy, &event->xcookie);
      if (! event->xcookie.data)
        return False;

      re = event->xcookie.data;
      switch (re->evtype) {
      case XI_RawKeyPress:
      case XI_RawButtonPress:
      case XI_RawTouchBegin:
      case XI_KeyPress:
      case XI_ButtonPress:
      case XI_TouchBegin:
        return True;
        break;
      case XI_RawMotion:
      case XI_RawTouchUpdate:
      case XI_Motion:
      case XI_TouchUpdate:
        if (motion_p) return True;
        break;
      default:
        break;
      }

      /* Calling XFreeEventData here is bad news */
    }
    break;
# endif /* HAVE_XINPUT */
  default: break;
  }

  return False;
}


static Bool
user_active_p (XtAppContext app, Display *dpy, Bool fade_out_p)
{
  XEvent event;
  XtInputMask m;
  Bool motion_p = fade_out_p;   /* Motion aborts fade-out, not fade-in. */
  motion_p = False;		/* Naah, never abort on motion only */

# ifdef HAVE_XINPUT
  if (xi_opcode == -1)
    {
      Bool ov = verbose_p;
      xi_opcode = 0; /* only init once */
      verbose_p = False;  /* xscreensaver already printed this */
      init_xinput (dpy, &xi_opcode);
      verbose_p = ov;
    }
# endif

  m = XtAppPending (app);
  if (m & ~XtIMXEvent)
    {
      /* Process timers and signals only, don't block. */
      if (verbose_p > 1)
        fprintf (stderr, "%s: Xt pending %ld\n", blurb(), m);
      XtAppProcessEvent (app, m);
    }

  /* If there is user activity, bug out.  (Bug out on keypresses or
     mouse presses, but not motion, and not release events.  Bugging
     out on motion made the unfade hack be totally useless, I think.)
   */
  if (XCheckIfEvent (dpy, &event, &user_event_p, (XPointer) &motion_p))
    {
      if (verbose_p > 1)
        {
          XIRawEvent *re = 0;
          if (event.xany.type == GenericEvent && !event.xcookie.data)
            {
              XGetEventData (dpy, &event.xcookie);
              re = event.xcookie.data;
            }
          fprintf (stderr, "%s: user input %d %d\n", blurb(),
                   event.xany.type,
                   (re ? re->evtype : -1));
        }
      XPutBackEvent (dpy, &event);
      return True;
    }

  return False;
}


static void
flush_user_input (Display *dpy)
{
  XEvent event;
  Bool motion_p = True;
  while (XCheckIfEvent (dpy, &event, &user_event_p, (XPointer) &motion_p))
    if (verbose_p > 1)
      {
        XIRawEvent *re = 0;
        if (event.xany.type == GenericEvent && !event.xcookie.data)
          {
            XGetEventData (dpy, &event.xcookie);
            re = event.xcookie.data;
          }
        fprintf (stderr, "%s: flushed user event %d %d\n", blurb(),
                 event.xany.type,
                 (re ? re->evtype : -1));
      }
}


/* This bullshit is needed because the VidMode and SHM extensions don't work
   on remote displays. */
static Bool error_handler_hit_p = False;

static int
ignore_all_errors_ehandler (Display *dpy, XErrorEvent *error)
{
  if (verbose_p > 1)
    XmuPrintDefaultErrorMessage (dpy, error, stderr);
  error_handler_hit_p = True;
  return 0;
}


/* Like XDestroyWindow, but destroys the window later, on a timer.  This is
   necessary to work around a KDE 5 compositor bug.  Without this, destroying
   an old window causes the desktop to briefly become visible, even though a
   new window has already been mapped that is obscuring both of them!
 */
typedef struct {
  XtAppContext app;
  Display *dpy;
  Window window;
} defer_destroy_closure;

static void
defer_destroy_handler (XtPointer closure, XtIntervalId *id)
{
  defer_destroy_closure *c = (defer_destroy_closure *) closure;
  XErrorHandler old_handler;
  XSync (c->dpy, False);
  error_handler_hit_p = False;
  old_handler = XSetErrorHandler (ignore_all_errors_ehandler);
  XDestroyWindow (c->dpy, c->window);
  XSync (c->dpy, False);
  XSetErrorHandler (old_handler);
  if (verbose_p > 1 && !error_handler_hit_p)
    fprintf (stderr, "%s: destroyed old window 0x%lx\n",
             blurb(), (unsigned long) c->window);
  free (c);
}

/* Used here and in windows.c */
void
defer_XDestroyWindow (XtAppContext app, Display *dpy, Window w)
{
  defer_destroy_closure *c = (defer_destroy_closure *) malloc (sizeof (*c));
  c->app = app;
  c->dpy = dpy;
  c->window = w;
  XtAppAddTimeOut (app, 5 * 1000, defer_destroy_handler, (XtPointer) c);
}


/* Returns true if canceled by user activity. */
Bool
fade_screens (XtAppContext app, Display *dpy,
              Window *saver_windows, int nwindows,
              double seconds, Bool out_p, Bool from_desktop_p,
              void **closureP)
{
  int status = False;
  fade_state *state = 0;

  if (nwindows <= 0) return False;
  if (!saver_windows) return False;

  if (!closureP) abort();
  state = (fade_state *) *closureP;
  if (!state)
    {
      state = (fade_state *) calloc (1, sizeof (*state));
      *closureP = state;
    }

  if (from_desktop_p && !out_p)
    abort();  /* Fading in from desktop makes no sense */

  if (out_p)
    flush_user_input (dpy);    /* Flush at start of cycle */

# ifdef HAVE_SGI_VC_EXTENSION
  /* First try to do it by fading the gamma in an SGI-specific way... */
  status = sgi_gamma_fade (app, dpy, saver_windows, nwindows, seconds, out_p);
  if (status == 0 || status == 1)
    return status;  /* faded, possibly canceled */
# endif

# ifdef HAVE_RANDR_12
  /* Then try to do it by fading the gamma in an RANDR-specific way... */
  status = randr_gamma_fade (app, dpy, saver_windows, nwindows, seconds, out_p);
  if (status == 0 || status == 1)
    return status;  /* faded, possibly canceled */
# endif

# ifdef HAVE_XF86VMODE_GAMMA
  /* Then try to do it by fading the gamma in an XFree86-specific way... */
  status = xf86_gamma_fade(app, dpy, saver_windows, nwindows, seconds, out_p);
  if (status == 0 || status == 1)
    return status;  /* faded, possibly canceled */
# endif

  if (has_writable_cells (DefaultScreenOfDisplay (dpy),
                          DefaultVisual (dpy, 0)))
    {
      /* Do it the old-fashioned way, which only really worked on
         8-bit displays. */
      status = colormap_fade (app, dpy, saver_windows, nwindows, seconds,
                              out_p, from_desktop_p);
      if (status == 0 || status == 1)
        return status;  /* faded, possibly canceled */
    }

  /* Else do it the hard way, by hacking a screenshot. */
  status = xshm_fade (app, dpy, saver_windows, nwindows, seconds, out_p,
                      from_desktop_p, state);
  status = (status ? True : False);

  return status;
}

/****************************************************************************

    Colormap fading

 ****************************************************************************/


/* The business with `cmaps_per_screen' is to fake out the SGI 8-bit video
   hardware, which is capable of installing multiple (4) colormaps
   simultaneously.  We have to install multiple copies of the same set of
   colors in order to fill up all the available slots in the hardware color
   lookup table, so we install an extra N colormaps per screen to make sure
   that all screens really go black.

   I'm told that this trick also works with XInside's AcceleratedX when using
   the Matrox Millennium card (which also allows multiple PseudoColor and
   TrueColor visuals to co-exist and display properly at the same time.)  

   This trick works ok on the 24-bit Indy video hardware, but doesn't work at
   all on the O2 24-bit hardware.  I guess the higher-end hardware is too
   "good" for this to work (dammit.)  So... I figured out the "right" way to
   do this on SGIs, which is to ramp the monitor's gamma down to 0.  That's
   what is implemented in sgi_gamma_fade(), so we use that if we can.

   Returns:
   0: faded normally
   1: canceled by user activity
  -1: unable to fade because the extension isn't supported.
 */
static int
colormap_fade (XtAppContext app, Display *dpy,
               Window *saver_windows, int nwindows,
               double seconds, Bool out_p, Bool from_desktop_p)
{
  int status = -1;
  Colormap *window_cmaps = 0;
  int i, j, k;
  unsigned int cmaps_per_screen = 5;
  unsigned int nscreens = ScreenCount(dpy);
  unsigned int ncmaps = nscreens * cmaps_per_screen;
  Colormap *fade_cmaps = 0;
  Bool installed = False;
  unsigned int total_ncolors;
  XColor *orig_colors, *current_colors, *screen_colors, *orig_screen_colors;
  unsigned int screen;

  window_cmaps = (Colormap *) calloc(sizeof(Colormap), nwindows);
  if (!window_cmaps) abort();
  for (screen = 0; screen < nwindows; screen++)
    {
      XWindowAttributes xgwa;
      XGetWindowAttributes (dpy, saver_windows[screen], &xgwa);
      window_cmaps[screen] = xgwa.colormap;
    }

  error_handler_hit_p = False;

  if (verbose_p > 1)
    fprintf (stderr, "%s: colormap fade %s\n",
             blurb(), (out_p ? "out" : "in"));

  total_ncolors = 0;
  for (i = 0; i < nscreens; i++)
    total_ncolors += CellsOfScreen (ScreenOfDisplay(dpy, i));

  orig_colors    = (XColor *) calloc(sizeof(XColor), total_ncolors);
  current_colors = (XColor *) calloc(sizeof(XColor), total_ncolors);

  /* Get the contents of the colormap we are fading from or to. */
  screen_colors = orig_colors;
  for (i = 0; i < nscreens; i++)
    {
      Screen *sc = ScreenOfDisplay (dpy, i);
      int ncolors = CellsOfScreen (sc);
      Colormap cmap = (from_desktop_p || !out_p
                       ? DefaultColormapOfScreen(sc)
                       : window_cmaps[i]);
      for (j = 0; j < ncolors; j++)
	screen_colors[j].pixel = j;
      XQueryColors (dpy, cmap, screen_colors, ncolors);

      screen_colors += ncolors;
    }

  memcpy (current_colors, orig_colors, total_ncolors * sizeof (XColor));


  /* Make the writable colormaps (we keep these around and reuse them.) */
  if (!fade_cmaps)
    {
      fade_cmaps = (Colormap *) calloc(sizeof(Colormap), ncmaps);
      for (i = 0; i < nscreens; i++)
	{
	  Visual *v = DefaultVisual(dpy, i);
	  Screen *s = ScreenOfDisplay(dpy, i);
	  if (has_writable_cells (s, v))
	    for (j = 0; j < cmaps_per_screen; j++)
	      fade_cmaps[(i * cmaps_per_screen) + j] =
		XCreateColormap (dpy, RootWindowOfScreen (s), v, AllocAll);
	}
    }

  /* Run the animation at the maximum frame rate in the time allotted. */
  {
    double start_time = double_time();
    double end_time = start_time + seconds;
    double prev = 0;
    double now;
    int frames = 0;
    double max = 1/60.0;  /* max FPS */
    while ((now = double_time()) < end_time)
      {
        double ratio = (end_time - now) / seconds;
        if (!out_p) ratio = 1-ratio;

        /* For each screen, compute the current value of each color...
         */
        orig_screen_colors = orig_colors;
        screen_colors = current_colors;
        for (j = 0; j < nscreens; j++)
          {
            int ncolors = CellsOfScreen (ScreenOfDisplay (dpy, j));
            for (k = 0; k < ncolors; k++)
              {
                /* This doesn't take into account the relative luminance of the
                   RGB components (0.299, 0.587, and 0.114 at gamma 2.2) but
                   the difference is imperceptible for this application... */
                screen_colors[k].red   = orig_screen_colors[k].red   * ratio;
                screen_colors[k].green = orig_screen_colors[k].green * ratio;
                screen_colors[k].blue  = orig_screen_colors[k].blue  * ratio;
              }
            screen_colors      += ncolors;
            orig_screen_colors += ncolors;
          }

        /* Put the colors into the maps...
         */
        screen_colors = current_colors;
        for (j = 0; j < nscreens; j++)
          {
            int ncolors = CellsOfScreen (ScreenOfDisplay (dpy, j));
            for (k = 0; k < cmaps_per_screen; k++)
              {
                Colormap c = fade_cmaps[j * cmaps_per_screen + k];
                if (c)
                  XStoreColors (dpy, c, screen_colors, ncolors);
              }
            screen_colors += ncolors;
          }

        /* Put the maps on the screens, and then take the windows off the
           screen.  (only need to do this the first time through the loop.)
         */
        if (!installed)
          {
            for (j = 0; j < ncmaps; j++)
              if (fade_cmaps[j])
                XInstallColormap (dpy, fade_cmaps[j]);
            installed = True;

            if (!out_p)
              for (j = 0; j < nwindows; j++)
                {
                  XUnmapWindow (dpy, saver_windows[j]);
                  XClearWindow (dpy, saver_windows[j]);
                }
          }

        XSync (dpy, False);

        if (error_handler_hit_p)
          goto DONE;
        if (user_active_p (app, dpy, out_p))
          {
            status = 1;   /* user activity status code */
            goto DONE;
          }
        frames++;

        if (now < prev + max)
          usleep (1000000 * (prev + max - now));
        prev = now;
      }

    if (verbose_p > 1)
      fprintf (stderr, "%s: %.0f FPS\n", blurb(), frames / (now - start_time));
  }

  status = 0;   /* completed fade with no user activity */

 DONE:

  if (orig_colors)    free (orig_colors);
  if (current_colors) free (current_colors);

  /* If we've been given windows to raise after blackout, raise them before
     releasing the colormaps.
   */
  if (out_p)
    {
      for (i = 0; i < nwindows; i++)
	{
          XClearWindow (dpy, saver_windows[i]);
	  XMapRaised (dpy, saver_windows[i]);
	}
      XSync(dpy, False);
    }

  /* Now put the target maps back.
     If we're fading out, use the given cmap (or the default cmap, if none.)
     If we're fading in, always use the default cmap.
   */
  for (i = 0; i < nscreens; i++)
    {
      Colormap cmap = window_cmaps[i];
      if (!cmap || !out_p)
	cmap = DefaultColormap(dpy, i);
      XInstallColormap (dpy, cmap);
    }

  /* The fade (in or out) is complete, so we don't need the black maps on
     stage any more.
   */
  for (i = 0; i < ncmaps; i++)
    if (fade_cmaps[i])
      {
	XUninstallColormap(dpy, fade_cmaps[i]);
	XFreeColormap(dpy, fade_cmaps[i]);
	fade_cmaps[i] = 0;
      }
  free (window_cmaps);
  free(fade_cmaps);
  fade_cmaps = 0;

  if (error_handler_hit_p) status = -1;
  return status;
}


/****************************************************************************

    SGI gamma fading

 ****************************************************************************/

#ifdef HAVE_SGI_VC_EXTENSION

# include <X11/extensions/XSGIvc.h>

struct screen_sgi_gamma_info {
  int gamma_map;  /* ??? always using 0 */
  int nred, ngreen, nblue;
  unsigned short *red1, *green1, *blue1;
  unsigned short *red2, *green2, *blue2;
  int gamma_size;
  int gamma_precision;
  Bool alpha_p;
};


static void sgi_whack_gamma(Display *dpy, int screen,
                            struct screen_sgi_gamma_info *info, float ratio);

/* Returns:
   0: faded normally
   1: canceled by user activity
  -1: unable to fade because the extension isn't supported.
 */
static int
sgi_gamma_fade (XtAppContext app, Display *dpy,
		Window *saver_windows, int nwindows,
                double seconds, Bool out_p)
{
  int nscreens = ScreenCount(dpy);
  struct timeval then, now;
  int i, screen;
  int status = -1;
  struct screen_sgi_gamma_info *info = (struct screen_sgi_gamma_info *)
    calloc(nscreens, sizeof(*info));

  if (verbose_p > 1)
    fprintf (stderr, "%s: sgi fade %s\n",
             blurb(), (out_p ? "out" : "in"));

  /* Get the current gamma maps for all screens.
     Bug out and return -1 if we can't get them for some screen.
   */
  for (screen = 0; screen < nscreens; screen++)
    {
      if (!XSGIvcQueryGammaMap(dpy, screen, info[screen].gamma_map,
			       &info[screen].gamma_size,
			       &info[screen].gamma_precision,
			       &info[screen].alpha_p))
	goto FAIL;

      if (!XSGIvcQueryGammaColors(dpy, screen, info[screen].gamma_map,
				  XSGIVC_COMPONENT_RED,
				  &info[screen].nred, &info[screen].red1))
	goto FAIL;
      if (! XSGIvcQueryGammaColors(dpy, screen, info[screen].gamma_map,
				   XSGIVC_COMPONENT_GREEN,
				   &info[screen].ngreen, &info[screen].green1))
	goto FAIL;
      if (!XSGIvcQueryGammaColors(dpy, screen, info[screen].gamma_map,
				  XSGIVC_COMPONENT_BLUE,
				  &info[screen].nblue, &info[screen].blue1))
	goto FAIL;

      if (info[screen].gamma_precision == 8)    /* Scale it up to 16 bits. */
	{
	  int j;
	  for(j = 0; j < info[screen].nred; j++)
	    info[screen].red1[j]   =
	      ((info[screen].red1[j]   << 8) | info[screen].red1[j]);
	  for(j = 0; j < info[screen].ngreen; j++)
	    info[screen].green1[j] =
	      ((info[screen].green1[j] << 8) | info[screen].green1[j]);
	  for(j = 0; j < info[screen].nblue; j++)
	    info[screen].blue1[j]  =
	      ((info[screen].blue1[j]  << 8) | info[screen].blue1[j]);
	}

      info[screen].red2   = (unsigned short *)
	malloc(sizeof(*info[screen].red2)   * (info[screen].nred+1));
      info[screen].green2 = (unsigned short *)
	malloc(sizeof(*info[screen].green2) * (info[screen].ngreen+1));
      info[screen].blue2  = (unsigned short *)
	malloc(sizeof(*info[screen].blue2)  * (info[screen].nblue+1));
    }

#ifdef GETTIMEOFDAY_TWO_ARGS
  gettimeofday(&then, &tzp);
#else
  gettimeofday(&then);
#endif

  /* If we're fading in (from black), then first crank the gamma all the
     way down to 0, then take the windows off the screen.
   */
  if (!out_p)
    {
      for (screen = 0; screen < nscreens; screen++)
	sgi_whack_gamma(dpy, screen, &info[screen], 0.0);
      
      for (screen = 0; screen < nwindows; screen++)
        {
          XUnmapWindow (dpy, saver_windows[screen]);
          XClearWindow (dpy, saver_windows[screen]);
          XSync(dpy, False);
        }
    }

  /* Run the animation at the maximum frame rate in the time allotted. */
  {
    double start_time = double_time();
    double end_time = start_time + seconds;
    double prev = 0;
    double now;
    int frames = 0;
    double max = 1/60.0;  /* max FPS */
    while ((now = double_time()) < end_time)
      {
        double ratio = (end_time - now) / seconds;
        if (!out_p) ratio = 1-ratio;

        for (screen = 0; screen < nwindows; screen++)
	  sgi_whack_gamma (dpy, screen, &info[screen], ratio);

        if (error_handler_hit_p)
          goto FAIL;
        if (user_active_p (app, dpy, out_p))
          {
            status = 1;   /* user activity status code */
            goto DONE;
          }
        frames++;

        if (now < prev + max)
          usleep (1000000 * (prev + max - now));
        prev = now;
      }

    if (verbose_p > 1)
      fprintf (stderr, "%s: %.0f FPS\n", blurb(), frames / (now - start_time));
  }

  status = 0;   /* completed fade with no user activity */

 DONE:

  if (out_p)
    {
      for (screen = 0; screen < nwindows; screen++)
	{
          XClearWindow (dpy, saver_windows[screen]);
	  XMapRaised (dpy, saver_windows[screen]);
	}
      XSync(dpy, False);
    }

  /* I can't explain this; without this delay, we get a flicker.
     I suppose there's some lossage with stale bits being in the
     hardware frame buffer or something, and this delay gives it
     time to flush out.  This sucks! */
  usleep(100000);  /* 1/10th second */

  for (screen = 0; screen < nscreens; screen++)
    sgi_whack_gamma(dpy, screen, &info[screen], 1.0);
  XSync(dpy, False);

 FAIL:
  for (screen = 0; screen < nscreens; screen++)
    {
      if (info[screen].red1)   free (info[screen].red1);
      if (info[screen].green1) free (info[screen].green1);
      if (info[screen].blue1)  free (info[screen].blue1);
      if (info[screen].red2)   free (info[screen].red2);
      if (info[screen].green2) free (info[screen].green2);
      if (info[screen].blue2)  free (info[screen].blue2);
    }
  free(info);

  if (verbose_p > 1 && status)
    fprintf (stderr, "%s: SGI fade %s failed\n",
             blurb(), (out_p ? "out" : "in"));

  if (error_handler_hit_p) status = -1;
  return status;
}

static void
sgi_whack_gamma (Display *dpy, int screen, struct screen_sgi_gamma_info *info,
                 float ratio)
{
  int k;

  if (ratio < 0) ratio = 0;
  if (ratio > 1) ratio = 1;
  for (k = 0; k < info->gamma_size; k++)
    {
      info->red2[k]   = info->red1[k]   * ratio;
      info->green2[k] = info->green1[k] * ratio;
      info->blue2[k]  = info->blue1[k]  * ratio;
    }

  XSGIvcStoreGammaColors16(dpy, screen, info->gamma_map, info->nred,
			   XSGIVC_MComponentRed, info->red2);
  XSGIvcStoreGammaColors16(dpy, screen, info->gamma_map, info->ngreen,
			   XSGIVC_MComponentGreen, info->green2);
  XSGIvcStoreGammaColors16(dpy, screen, info->gamma_map, info->nblue,
			   XSGIVC_MComponentBlue, info->blue2);
  XSync(dpy, False);
}

#endif /* HAVE_SGI_VC_EXTENSION */


/****************************************************************************

    XFree86 gamma fading

 ****************************************************************************/

#ifdef HAVE_XF86VMODE_GAMMA

#include <X11/extensions/xf86vmode.h>

typedef struct {
  XF86VidModeGamma vmg;
  int size;
  unsigned short *r, *g, *b;
} xf86_gamma_info;

static int xf86_check_gamma_extension (Display *dpy);
static Bool xf86_whack_gamma (Display *dpy, int screen,
                              xf86_gamma_info *ginfo, float ratio);

/* Returns:
   0: faded normally
   1: canceled by user activity
  -1: unable to fade because the extension isn't supported.
 */
static int
xf86_gamma_fade (XtAppContext app, Display *dpy,
                 Window *saver_windows, int nwindows,
                 double seconds, Bool out_p)
{
  int nscreens = ScreenCount(dpy);
  int screen;
  int status = -1;
  xf86_gamma_info *info = 0;

  static int ext_ok = -1;

  if (verbose_p > 1)
    fprintf (stderr, "%s: xf86 fade %s\n",
             blurb(), (out_p ? "out" : "in"));

  /* Only probe the extension once: the answer isn't going to change. */
  if (ext_ok == -1)
    ext_ok = xf86_check_gamma_extension (dpy);

  /* If this server doesn't have the gamma extension, bug out. */
  if (ext_ok == 0)
    goto FAIL;

# ifndef HAVE_XF86VMODE_GAMMA_RAMP
  if (ext_ok == 2) ext_ok = 1;  /* server is newer than client! */
# endif

  info = (xf86_gamma_info *) calloc(nscreens, sizeof(*info));

  /* Get the current gamma maps for all screens.
     Bug out and return -1 if we can't get them for some screen.
   */
  for (screen = 0; screen < nscreens; screen++)
    {
      if (ext_ok == 1)  /* only have gamma parameter, not ramps. */
        {
          if (!XF86VidModeGetGamma(dpy, screen, &info[screen].vmg))
            goto FAIL;
        }
# ifdef HAVE_XF86VMODE_GAMMA_RAMP
      else if (ext_ok == 2)  /* have ramps */
        {
          if (!XF86VidModeGetGammaRampSize(dpy, screen, &info[screen].size))
            goto FAIL;
          if (info[screen].size <= 0)
            goto FAIL;

          info[screen].r = (unsigned short *)
            calloc(info[screen].size, sizeof(unsigned short));
          info[screen].g = (unsigned short *)
            calloc(info[screen].size, sizeof(unsigned short));
          info[screen].b = (unsigned short *)
            calloc(info[screen].size, sizeof(unsigned short));

          if (!(info[screen].r && info[screen].g && info[screen].b))
            goto FAIL;

# if 0
          if (verbose_p > 1 && out_p)
            {
              int i;
              fprintf (stderr, "%s: initial gamma ramps, size %d:\n",
                       blurb(), info[screen].size);
              fprintf (stderr, "%s:   R:", blurb());
              for (i = 0; i < info[screen].size; i++)
                fprintf (stderr, " %d", info[screen].r[i]);
              fprintf (stderr, "\n%s:   G:", blurb());
              for (i = 0; i < info[screen].size; i++)
                fprintf (stderr, " %d", info[screen].g[i]);
              fprintf (stderr, "\n%s:   B:", blurb());
              for (i = 0; i < info[screen].size; i++)
                fprintf (stderr, " %d", info[screen].b[i]);
              fprintf (stderr, "\n");
            }
# endif /* 0 */

          if (!XF86VidModeGetGammaRamp(dpy, screen, info[screen].size,
                                       info[screen].r,
                                       info[screen].g,
                                       info[screen].b))
            goto FAIL;
        }
# endif /* HAVE_XF86VMODE_GAMMA_RAMP */
      else
        abort();
    }

  /* If we're fading in (from black), then first crank the gamma all the
     way down to 0, then take the windows off the screen.
   */
  if (!out_p)
    {
      for (screen = 0; screen < nscreens; screen++)
	xf86_whack_gamma(dpy, screen, &info[screen], 0.0);
      for (screen = 0; screen < nwindows; screen++)
        {
          XUnmapWindow (dpy, saver_windows[screen]);
          XClearWindow (dpy, saver_windows[screen]);
          XSync(dpy, False);
        }
    }

  /* Run the animation at the maximum frame rate in the time allotted. */
  {
    double start_time = double_time();
    double end_time = start_time + seconds;
    double prev = 0;
    double now;
    int frames = 0;
    double max = 1/60.0;  /* max FPS */
    while ((now = double_time()) < end_time)
      {
        double ratio = (end_time - now) / seconds;
        if (!out_p) ratio = 1-ratio;

        for (screen = 0; screen < nscreens; screen++)
          xf86_whack_gamma (dpy, screen, &info[screen], ratio);

        if (error_handler_hit_p)
          goto FAIL;
        if (user_active_p (app, dpy, out_p))
          {
            status = 1;   /* user activity status code */
            goto DONE;
          }
        frames++;

        if (now < prev + max)
          usleep (1000000 * (prev + max - now));
        prev = now;
      }

    if (verbose_p > 1)
      fprintf (stderr, "%s: %.0f FPS\n", blurb(), frames / (now - start_time));
  }

  status = 0;   /* completed fade with no user activity */

 DONE:

  if (out_p)
    {
      for (screen = 0; screen < nwindows; screen++)
	{
          XClearWindow (dpy, saver_windows[screen]);
	  XMapRaised (dpy, saver_windows[screen]);
	}
      XSync(dpy, False);
    }

  /* I can't explain this; without this delay, we get a flicker.
     I suppose there's some lossage with stale bits being in the
     hardware frame buffer or something, and this delay gives it
     time to flush out.  This sucks! */
  usleep(100000);  /* 1/10th second */

  for (screen = 0; screen < nscreens; screen++)
    xf86_whack_gamma(dpy, screen, &info[screen], 1.0);
  XSync(dpy, False);

 FAIL:
  if (info)
    {
      for (screen = 0; screen < nscreens; screen++)
        {
          if (info[screen].r) free(info[screen].r);
          if (info[screen].g) free(info[screen].g);
          if (info[screen].b) free(info[screen].b);
        }
      free(info);
    }

  if (verbose_p > 1 && status)
    fprintf (stderr, "%s: xf86 fade %s failed\n",
             blurb(), (out_p ? "out" : "in"));

  if (error_handler_hit_p) status = -1;
  return status;
}


static Bool
safe_XF86VidModeQueryVersion (Display *dpy, int *majP, int *minP)
{
  Bool result;
  XErrorHandler old_handler;
  XSync (dpy, False);
  error_handler_hit_p = False;
  old_handler = XSetErrorHandler (ignore_all_errors_ehandler);

  result = XF86VidModeQueryVersion (dpy, majP, minP);

  XSync (dpy, False);
  XSetErrorHandler (old_handler);
  XSync (dpy, False);

  return (error_handler_hit_p
          ? False
          : result);
}



/* VidModeExtension version 2.0 or better is needed to do gamma.
   2.0 added gamma values; 2.1 added gamma ramps.
 */
# define XF86_VIDMODE_GAMMA_MIN_MAJOR 2
# define XF86_VIDMODE_GAMMA_MIN_MINOR 0
# define XF86_VIDMODE_GAMMA_RAMP_MIN_MAJOR 2
# define XF86_VIDMODE_GAMMA_RAMP_MIN_MINOR 1



/* Returns 0 if gamma fading not available; 1 if only gamma value setting
   is available; 2 if gamma ramps are available.
 */
static int
xf86_check_gamma_extension (Display *dpy)
{
  int event, error, major, minor;

  if (!XF86VidModeQueryExtension (dpy, &event, &error))
    return 0;  /* display doesn't have the extension. */

  if (!safe_XF86VidModeQueryVersion (dpy, &major, &minor))
    return 0;  /* unable to get version number? */

  if (major < XF86_VIDMODE_GAMMA_MIN_MAJOR || 
      (major == XF86_VIDMODE_GAMMA_MIN_MAJOR &&
       minor < XF86_VIDMODE_GAMMA_MIN_MINOR))
    return 0;  /* extension is too old for gamma. */

  if (major < XF86_VIDMODE_GAMMA_RAMP_MIN_MAJOR || 
      (major == XF86_VIDMODE_GAMMA_RAMP_MIN_MAJOR &&
       minor < XF86_VIDMODE_GAMMA_RAMP_MIN_MINOR))
    return 1;  /* extension is too old for gamma ramps. */

  /* Copacetic */
  return 2;
}


/* XFree doesn't let you set gamma to a value smaller than this.
   Apparently they didn't anticipate the trick I'm doing here...
 */
#define XF86_MIN_GAMMA  0.1


static Bool
xf86_whack_gamma(Display *dpy, int screen, xf86_gamma_info *info,
                 float ratio)
{
  XErrorHandler old_handler;
  XSync (dpy, False);
  error_handler_hit_p = False;
  old_handler = XSetErrorHandler (ignore_all_errors_ehandler);

  if (ratio < 0) ratio = 0;
  if (ratio > 1) ratio = 1;

  if (info->size == 0)    /* we only have a gamma number, not a ramp. */
    {
      XF86VidModeGamma g2;

      g2.red   = info->vmg.red   * ratio;
      g2.green = info->vmg.green * ratio;
      g2.blue  = info->vmg.blue  * ratio;

# ifdef XF86_MIN_GAMMA
      if (g2.red   < XF86_MIN_GAMMA) g2.red   = XF86_MIN_GAMMA;
      if (g2.green < XF86_MIN_GAMMA) g2.green = XF86_MIN_GAMMA;
      if (g2.blue  < XF86_MIN_GAMMA) g2.blue  = XF86_MIN_GAMMA;
# endif

      if (! XF86VidModeSetGamma (dpy, screen, &g2))
        return -1;
    }
  else
    {
# ifdef HAVE_XF86VMODE_GAMMA_RAMP

      unsigned short *r, *g, *b;
      int i;
      r = (unsigned short *) malloc(info->size * sizeof(unsigned short));
      g = (unsigned short *) malloc(info->size * sizeof(unsigned short));
      b = (unsigned short *) malloc(info->size * sizeof(unsigned short));

      for (i = 0; i < info->size; i++)
        {
          r[i] = info->r[i] * ratio;
          g[i] = info->g[i] * ratio;
          b[i] = info->b[i] * ratio;
        }

      if (! XF86VidModeSetGammaRamp(dpy, screen, info->size, r, g, b))
        return -1;

      free (r);
      free (g);
      free (b);

# else  /* !HAVE_XF86VMODE_GAMMA_RAMP */
      abort();
# endif /* !HAVE_XF86VMODE_GAMMA_RAMP */
    }

  XSync (dpy, False);
  XSetErrorHandler (old_handler);
  XSync (dpy, False);

  return status;
}

#endif /* HAVE_XF86VMODE_GAMMA */


/****************************************************************************

    RANDR gamma fading

 ****************************************************************************


   Dec 2020: I noticed that gamma fading was not working on a Raspberry Pi
   with Raspbian 10.6, and wrote this under the hypothesis that the XF86
   gamma fade code was failing and maybe the RANDR version would work better.
   Then I discovered that gamma simply isn't supported by the Raspberry Pi
   HDMI driver:

       https://github.com/raspberrypi/firmware/issues/1274

   I should have tried this first and seen it not work:

       xrandr --output HDMI-1 --brightness .1

   Since I still don't have an answer to the question of whether the XF86
   gamma fading method works on modern Linux systems that also have RANDR,
   I'm leaving this new code turned off for now, as it is largely untested.
   The new code would be useful if:

     A) The XF86 way doesn't work but the RANDR way does, or
     B) There exist systems that have RANDR but do not have XF86.

   But until Raspberry Pi supports gamma, both gamma methods fail to work
   for far too many users for them to be used in XScreenSaver.
 */
#ifdef HAVE_RANDR_12

# include <X11/extensions/Xrandr.h>

typedef struct {
  RRCrtc crtc;
  Bool enabled_p;
  XRRCrtcGamma *gamma;
} randr_gamma_info;


static int
randr_check_gamma_extension (Display *dpy)
{
  int event, error, major, minor;
  if (! XRRQueryExtension (dpy, &event, &error))
    return 0;
  
  if (! XRRQueryVersion (dpy, &major, &minor)) {
    if (verbose_p > 1) fprintf (stderr, "%s: no randr ext\n", blurb());
    return 0;
  }

  /* Reject if < 1.5. It's possible that 1.2 - 1.4 work, but untested. */
  if (major < 1 || (major == 1 && minor < 5)) {
    if (verbose_p > 1) fprintf (stderr, "%s: randr ext only version %d.%d\n",
                               blurb(), major, minor);
    return 0;
  }

  return 1;
}


static void randr_whack_gamma (Display *dpy, int screen,
                               randr_gamma_info *ginfo, float ratio);

/* Returns:
   0: faded normally
   1: canceled by user activity
  -1: unable to fade because the extension isn't supported.
 */
static int
randr_gamma_fade (XtAppContext app, Display *dpy,
                   Window *saver_windows, int nwindows,
                   double seconds, Bool out_p)
{
  int xsc = ScreenCount (dpy);
  int nscreens = 0;
  int j, screen;
  int status = -1;
  randr_gamma_info *info = 0;

  static int ext_ok = -1;

  if (verbose_p > 1)
    fprintf (stderr, "%s: randr fade %s\n",
             blurb(), (out_p ? "out" : "in"));

  /* Only probe the extension once: the answer isn't going to change. */
  if (ext_ok == -1)
    ext_ok = randr_check_gamma_extension (dpy);

  /* If this server doesn't have the RANDR extension, bug out. */
  if (ext_ok == 0)
    goto FAIL;

  /* Add up the virtual screens on each X screen. */
  for (screen = 0; screen < xsc; screen++)
    {
      XRRScreenResources *res = 
        XRRGetScreenResources (dpy, RootWindow (dpy, screen));
      nscreens += res->noutput;
      XRRFreeScreenResources (res);
    }

  if (nscreens <= 0)
    goto FAIL;

  info = (randr_gamma_info *) calloc(nscreens, sizeof(*info));

  /* Get the current gamma maps for all screens.
     Bug out and return -1 if we can't get them for some screen.
  */
  for (screen = 0, j = 0; screen < xsc; screen++)
    {
      XRRScreenResources *res =
        XRRGetScreenResources (dpy, RootWindow (dpy, screen));
      int k;
      for (k = 0; k < res->noutput; k++, j++)
        {
          XRROutputInfo *rroi = XRRGetOutputInfo (dpy, res, res->outputs[j]);
          RRCrtc crtc = (rroi->crtc  ? rroi->crtc :
                         rroi->ncrtc ? rroi->crtcs[0] : 0);

          info[j].crtc = crtc;
          info[j].gamma = XRRGetCrtcGamma (dpy, crtc);

          /* #### is this test sufficient? */
          info[j].enabled_p = (rroi->connection != RR_Disconnected);

# if 0
          if (verbose_p > 1 && out_p)
            {
              int m;
              fprintf (stderr, "%s: initial gamma ramps, size %d:\n",
                       blurb(), info[j].gamma->size);
              fprintf (stderr, "%s:   R:", blurb());
              for (m = 0; m < info[j].gamma->size; m++)
                fprintf (stderr, " %d", info[j].gamma->red[m]);
              fprintf (stderr, "\n%s:   G:", blurb());
              for (m = 0; m < info[j].gamma->size; m++)
                fprintf (stderr, " %d", info[j].gamma->green[m]);
              fprintf (stderr, "\n%s:   B:", blurb());
              for (m = 0; m < info[j].gamma->size; m++)
                fprintf (stderr, " %d", info[j].gamma->blue[m]);
              fprintf (stderr, "\n");
            }
# endif /* 0 */

          XRRFreeOutputInfo (rroi);
        }
      XRRFreeScreenResources (res);
    }

  /* If we're fading in (from black), then first crank the gamma all the
     way down to 0, then take the windows off the screen.
  */
  if (!out_p)
    {
      for (screen = 0; screen < nscreens; screen++)
	randr_whack_gamma(dpy, screen, &info[screen], 0.0);
      for (screen = 0; screen < nwindows; screen++)
        {
          XUnmapWindow (dpy, saver_windows[screen]);
          XClearWindow (dpy, saver_windows[screen]);
          XSync(dpy, False);
        }
    }

  /* Run the animation at the maximum frame rate in the time allotted. */
  {
    double start_time = double_time();
    double end_time = start_time + seconds;
    double prev = 0;
    double now;
    int frames = 0;
    double max = 1/60.0;  /* max FPS */
    while ((now = double_time()) < end_time)
      {
        double ratio = (end_time - now) / seconds;
        if (!out_p) ratio = 1-ratio;

        for (screen = 0; screen < nwindows; screen++)
          {
            if (!info[screen].enabled_p)
              continue;

            randr_whack_gamma (dpy, screen, &info[screen], ratio);
          }

        if (error_handler_hit_p)
          goto FAIL;
        if (user_active_p (app, dpy, out_p))
          {
            status = 1;   /* user activity status code */
            goto DONE;
          }
        frames++;

        if (now < prev + max)
          usleep (1000000 * (prev + max - now));
        prev = now;
      }

    if (verbose_p > 1)
      fprintf (stderr, "%s: %.0f FPS\n", blurb(), frames / (now - start_time));
  }

  status = 0;   /* completed fade with no user activity */

 DONE:

  if (out_p)
    {
      for (screen = 0; screen < nwindows; screen++)
	{
          XClearWindow (dpy, saver_windows[screen]);
	  XMapRaised (dpy, saver_windows[screen]);
	}
      XSync(dpy, False);
    }

  /* I can't explain this; without this delay, we get a flicker.
     I suppose there's some lossage with stale bits being in the
     hardware frame buffer or something, and this delay gives it
     time to flush out.  This sucks! */
  /* #### That comment was about XF86, not verified with randr. */
  usleep(100000);  /* 1/10th second */

  for (screen = 0; screen < nscreens; screen++)
    randr_whack_gamma (dpy, screen, &info[screen], 1.0);
  XSync(dpy, False);

 FAIL:
  if (info)
    {
      for (screen = 0; screen < nscreens; screen++)
        {
          if (info[screen].gamma) XRRFreeGamma (info[screen].gamma);
        }
      free(info);
    }

  if (verbose_p > 1 && status)
    fprintf (stderr, "%s: randr fade %s failed\n",
             blurb(), (out_p ? "out" : "in"));

  return status;
}


static void
randr_whack_gamma (Display *dpy, int screen, randr_gamma_info *info,
                   float ratio)
{
  XErrorHandler old_handler;
  XRRCrtcGamma *g2;
  int i;

  XSync (dpy, False);
  error_handler_hit_p = False;
  old_handler = XSetErrorHandler (ignore_all_errors_ehandler);

  if (ratio < 0) ratio = 0;
  if (ratio > 1) ratio = 1;

  g2 = XRRAllocGamma (info->gamma->size);
  for (i = 0; i < info->gamma->size; i++)
    {
      g2->red[i]   = ratio * info->gamma->red[i];
      g2->green[i] = ratio * info->gamma->green[i];
      g2->blue[i]  = ratio * info->gamma->blue[i];
    }

  XRRSetCrtcGamma (dpy, info->crtc, g2);
  XRRFreeGamma (g2);

  XSync (dpy, False);
  XSetErrorHandler (old_handler);
  XSync (dpy, False);

  return 0;
}

#endif /* HAVE_RANDR_12 */


/****************************************************************************

    XSHM or OpenGL screenshot fading
    This module does one or the other, depending on USE_GL

 ****************************************************************************/

typedef struct {
  Window window;
  XImage *src;
  Pixmap screenshot;
# ifndef USE_GL
  XImage *intermediate;
  GC gc;
# else /* USE_GL */
  GLuint texid;
  GLfloat texw, texh;
#  ifdef HAVE_EGL
  egl_data   *glx_context;
#  else
  GLXContext glx_context;
#  endif
# endif /* USE_GL */
} xshm_fade_info;


#ifdef USE_GL
static int opengl_whack (Display *, xshm_fade_info *, float ratio);
#else
static int xshm_whack (Display *, XShmSegmentInfo *,
                       xshm_fade_info *, float ratio);
#endif

/* Grab a screenshot and return it.
   It will be the size and extent of the given window.
   Or None if we failed.
   Somewhat duplicated in screenshot.c:screenshot_grab().
 */
static Pixmap
xshm_screenshot_grab (Display *dpy, Window window,
                      Bool from_desktop_p, Bool verbose_p)
{
  XWindowAttributes xgwa;
  Pixmap pixmap = 0;
  Bool external_p = False;
  double start = double_time();

# ifdef HAVE_MACOS_X11
  external_p = True;
# else  /* X11, possibly Wayland */
  external_p = getenv ("WAYLAND_DISPLAY") || getenv ("WAYLAND_SOCKET");
# endif

  if (from_desktop_p)
    {
      Pixmap p = screenshot_load (dpy, window, verbose_p);
      if (p) return p;
    }

  XGetWindowAttributes (dpy, window, &xgwa);
  pixmap = XCreatePixmap (dpy, window, xgwa.width, xgwa.height, xgwa.depth);
  if (!pixmap) return None;

  XSync (dpy, False);  /* So that the pixmap exists before we exec. */

  /* Run xscreensaver-getimage to get a screenshot.  It will, in turn,
     run "screencapture" or "grim" to write the screenshot to a PNG file,
     then will load that file onto our Pixmap.
   */
  if (external_p)
    {
      pid_t forked;
      char *av[20];
      int ac = 0;
      char buf[1024];
      char rootstr[20], pixstr[20];

      sprintf (rootstr, "0x%0lx", (unsigned long) window);
      sprintf (pixstr,  "0x%0lx", (unsigned long) pixmap);
      av[ac++] = "xscreensaver-getimage";
      if (verbose_p)
        av[ac++] = "--verbose";
      av[ac++] = "--desktop";
      if (!from_desktop_p)
        av[ac++] = "--no-cache";
      av[ac++] = "--no-images";
      av[ac++] = "--no-video";
      av[ac++] = rootstr;
      av[ac++] = pixstr;
      av[ac] = 0;

      if (verbose_p)
        {
          int i;
          fprintf (stderr, "%s: fade: executing:", blurb());
          for (i = 0; i < ac; i++)
            fprintf (stderr, " %s", av[i]);
          fprintf (stderr, "\n");
        }

      switch ((int) (forked = fork ())) {
      case -1:
        {
          sprintf (buf, "%s: couldn't fork", blurb());
          perror (buf);
          return 0;
        }
      case 0:
        {
          close (ConnectionNumber (dpy));	/* close display fd */
          execvp (av[0], av);			/* shouldn't return. */
          exit (-1);				/* exits fork */
          break;
        }
      default:
        {
          int wait_status = 0, exit_status = 0;
          /* Wait for the child to die. */
          waitpid (forked, &wait_status, 0);
          exit_status = WEXITSTATUS (wait_status);
          /* Treat exit code as a signed 8-bit quantity. */
          if (exit_status & 0x80) exit_status |= ~0xFF;
          if (exit_status != 0)
            {
              fprintf (stderr, "%s: fade: %s exited with %d\n",
                       blurb(), av[0], exit_status);
              if (pixmap) XFreePixmap (dpy, pixmap);
              return None;
            }
        }
      }
    }
# ifndef HAVE_MACOS_X11
  else
    {
      /* Grab a screenshot using XCopyArea. */

      XGCValues gcv;
      GC gc;
      gcv.function = GXcopy;
      gcv.subwindow_mode = IncludeInferiors;
      gc = XCreateGC (dpy, pixmap, GCFunction | GCSubwindowMode, &gcv);
      XCopyArea (dpy, xgwa.root, pixmap, gc,
                 xgwa.x, xgwa.y, xgwa.width, xgwa.height, 0, 0);
      XFreeGC (dpy, gc);
    }
# endif /* HAVE_MACOS_X11 */

  {
    double elapsed = double_time() - start;
    char late[100];
    if (elapsed >= 0.75)
      sprintf (late, " -- took %.1f seconds", elapsed);
    else
      *late = 0;

    if (verbose_p || !pixmap || *late)
      fprintf (stderr, "%s: %s screenshot 0x%lx %dx%d"
               " for window 0x%lx%s\n", blurb(),
               (pixmap ? "saved" : "failed to save"),
               (unsigned long) pixmap, xgwa.width, xgwa.height,
               (unsigned long) window,
               late);
  }

  return pixmap;
}


#ifdef USE_GL
static void
opengl_make_current (Display *dpy, xshm_fade_info *info)
{
# ifdef HAVE_EGL
  egl_data *d = info->glx_context;
  if (! eglMakeCurrent (d->egl_display, d->egl_surface, d->egl_surface,
                        d->egl_context))
    abort();
# else /* !HAVE_EGL */
  glXMakeCurrent (dpy, info->window, info->glx_context);
# endif /* !HAVE_EGL */

}

/* report a GL error. */
static Bool
check_gl_error (const char *type)
{
  char buf[100];
  GLenum i;
  const char *e;
  switch ((i = glGetError())) {
  case GL_NO_ERROR: return False;
  case GL_INVALID_ENUM:          e = "invalid enum";      break;
  case GL_INVALID_VALUE:         e = "invalid value";     break;
  case GL_INVALID_OPERATION:     e = "invalid operation"; break;
  case GL_STACK_OVERFLOW:        e = "stack overflow";    break;
  case GL_STACK_UNDERFLOW:       e = "stack underflow";   break;
  case GL_OUT_OF_MEMORY:         e = "out of memory";     break;
#ifdef GL_INVALID_FRAMEBUFFER_OPERATION
  case GL_INVALID_FRAMEBUFFER_OPERATION:
    e = "invalid framebuffer operation";
    break;
#endif
#ifdef GL_TABLE_TOO_LARGE_EXT
  case GL_TABLE_TOO_LARGE_EXT:   e = "table too large";   break;
#endif
#ifdef GL_TEXTURE_TOO_LARGE_EXT
  case GL_TEXTURE_TOO_LARGE_EXT: e = "texture too large"; break;
#endif
  default:
    e = buf; sprintf (buf, "unknown error %d", (int) i); break;
  }
  fprintf (stderr, "%s: %s error: %s\n", progname, type, e);
  return True;
}
#endif /* USE_GL */


/* Returns:
   0: faded normally
   1: canceled by user activity
  -1: unknown error
 */
static int
xshm_fade (XtAppContext app, Display *dpy,
           Window *saver_windows, int nwindows, double seconds, 
           Bool out_p, Bool from_desktop_p, fade_state *state)
{
  int screen;
  int status = -1;
  xshm_fade_info *info = 0;
# ifndef USE_GL
  XShmSegmentInfo shm_info;
# endif
  Window saver_window = 0;
  XErrorHandler old_handler = 0;

  XSync (dpy, False);
  old_handler = XSetErrorHandler (ignore_all_errors_ehandler);
  error_handler_hit_p = False;  

# ifdef USE_GL
  if (verbose_p > 1)
    fprintf (stderr, "%s: GL fade %s\n", blurb(), (out_p ? "out" : "in"));
# else /* !USE_GL */
  if (verbose_p > 1)
    fprintf (stderr, "%s: SHM fade %s\n", blurb(), (out_p ? "out" : "in"));
# endif /* !USE_GL */

  info = (xshm_fade_info *) calloc(nwindows, sizeof(*info));
  if (!info) goto FAIL;

  saver_window = find_screensaver_window (dpy, 0);
  if (!saver_window) goto FAIL;

  /* Retrieve a screenshot of the area covered by each window.
     Windows might not be mapped.
     Bug out and return -1 if we can't get one for some screen.
  */

  for (screen = 0; screen < nwindows; screen++)
    {
      XWindowAttributes xgwa;
      Window root;
      Visual *visual;
      XSetWindowAttributes attrs;
      unsigned long attrmask = 0;
# ifndef USE_GL
      XGCValues gcv;
# endif /* USE_GL */

      XGetWindowAttributes (dpy, saver_windows[screen], &xgwa);
      root = RootWindowOfScreen (xgwa.screen);
# ifdef USE_GL
      visual = get_gl_visual (xgwa.screen);
# else /* !USE_GL */
      visual = xgwa.visual;
# endif /* !USE_GL */

# ifdef USE_GL
      info[screen].src =
        XCreateImage (dpy, visual, 32, ZPixmap, 0, NULL,
                      xgwa.width, xgwa.height, 32, 0);
      if (!info[screen].src) goto FAIL;
      info[screen].src->data = (char *)
        malloc (info[screen].src->height * info[screen].src->bytes_per_line);
      info[screen].src->bitmap_bit_order =
        info[screen].src->byte_order = MSBFirst;

      while (glGetError() != GL_NO_ERROR)
        ;  /* Flush */

# else /* !USE_GL */
      info[screen].src =
        create_xshm_image (dpy, visual, xgwa.depth,
                           ZPixmap, &shm_info, xgwa.width, xgwa.height);
      if (!info[screen].src) goto FAIL;

      info[screen].intermediate =
        create_xshm_image (dpy, visual, xgwa.depth,
                           ZPixmap, &shm_info, xgwa.width, xgwa.height);
      if (!info[screen].intermediate) goto FAIL;
# endif /* !USE_GL */

      if (!out_p)
        {
          /* If we are fading in, retrieve the saved screenshot from
             before we faded out. */
          if (state->nscreens <= screen) goto FAIL;
          info[screen].screenshot = state->screenshots[screen];
        }
      else
        {
          info[screen].screenshot =
            xshm_screenshot_grab (dpy, saver_windows[screen], from_desktop_p,
                                  verbose_p);
          if (!info[screen].screenshot) goto FAIL;
        }

      /* Create the fader window for the animation. */
      attrmask = CWOverrideRedirect;
      attrs.override_redirect = True;
      info[screen].window = 
        XCreateWindow (dpy, root, xgwa.x, xgwa.y,
                       xgwa.width, xgwa.height, xgwa.border_width, xgwa.depth,
                       InputOutput, visual,
                       attrmask, &attrs);
      if (!info[screen].window) goto FAIL;
      /* XSelectInput (dpy, info[screen].window,
                       KeyPressMask | ButtonPressMask); */

# ifdef USE_GL
      /* Copy the screenshot pixmap to the texture XImage */
      XGetSubImage (dpy, info[screen].screenshot,
                    0, 0, xgwa.width, xgwa.height,
                    ~0L, ZPixmap, info[screen].src, 0, 0);

      /* Convert 0RGB to RGBA */
      {
        XImage *ximage = info[screen].src;
        int x, y;
        for (y = 0; y < ximage->height; y++)
          for (x = 0; x < ximage->width; x++)
            {
              unsigned long p = XGetPixel (ximage, x, y);
              unsigned long a = 0xFF;
           /* unsigned long a = (p >> 24) & 0xFF; */
              unsigned long r = (p >> 16) & 0xFF;
              unsigned long g = (p >>  8) & 0xFF;
              unsigned long b = (p >>  0) & 0xFF;
              p = (r << 24) | (g << 16) | (b << 8) | (a << 0);
              XPutPixel (ximage, x, y, p);
            }
      }

      /* Connect the window to an OpenGL context */
      info[screen].glx_context =
        openGL_context_for_window (xgwa.screen, info[screen].window);
      opengl_make_current (dpy, &info[screen]);
      if (check_gl_error ("connect")) goto FAIL;

      glEnable (GL_TEXTURE_2D);
      glEnable (GL_NORMALIZE);
      glGenTextures (1, &info[screen].texid);
      glBindTexture (GL_TEXTURE_2D, info[screen].texid);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
      if (check_gl_error ("bind texture")) goto FAIL;

      {
        int tex_width  = (GLsizei) to_pow2 (info[screen].src->width);
        int tex_height = (GLsizei) to_pow2 (info[screen].src->height);
        /* Create power of 2 empty texture */
        glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, tex_width, tex_height, 0,
                      GL_RGBA, GL_UNSIGNED_BYTE, 0);
        if (check_gl_error ("glTexImage2D")) goto FAIL;
        /* Load our non-power-of-2 image data into it. */
        glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0,
                         info[screen].src->width, info[screen].src->height,
                           GL_RGBA, GL_UNSIGNED_BYTE, info[screen].src->data);
        if (check_gl_error ("glTexSubImage2D")) goto FAIL;
        info[screen].texw = info[screen].src->width  / (GLfloat) tex_width;
        info[screen].texh = info[screen].src->height / (GLfloat) tex_height;
      }

      glViewport (0, 0, xgwa.width, xgwa.height);
      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      glOrtho (0, 1, 1, 0, -1, 1);
      glMatrixMode (GL_MODELVIEW);
      glLoadIdentity();
      glClearColor (0, 0, 0, 1);
      glClear (GL_COLOR_BUFFER_BIT);
      glFrontFace (GL_CCW);
      if (check_gl_error ("GL setup")) goto FAIL;

# else /* !USE_GL */
      /* Copy the screenshot pixmap to the source image */
      if (! get_xshm_image (dpy, info[screen].screenshot, info[screen].src,
                            0, 0, ~0L, &shm_info))
        goto FAIL;

      gcv.function = GXcopy;
      info[screen].gc = XCreateGC (dpy, info[screen].window, GCFunction, &gcv);
# endif /* !USE_GL */
    }

  /* If we're fading out from the desktop, save our screen shots for later use.
     But not if we're fading out from the savers to black.  In that case we
     don't want to overwrite the desktop screenshot with the current screenshot
     which is of the final frames of the just-killed graphics hacks. */
  if (from_desktop_p)
    {
      if (!out_p) abort();
      for (screen = 0; screen < state->nscreens; screen++)
        if (state->screenshots[screen])
          XFreePixmap (dpy, state->screenshots[screen]);
      if (state->screenshots)
        free (state->screenshots);
      state->nscreens = nwindows;
      state->screenshots = calloc (nwindows, sizeof(*state->screenshots));
      if (!state->screenshots)
        state->nscreens = 0;
      for (screen = 0; screen < state->nscreens; screen++)
        state->screenshots[screen] = info[screen].screenshot;
    }

  for (screen = 0; screen < nwindows; screen++)
    {
# ifndef USE_GL
      if (out_p)
        /* Copy the screenshot to the fader window */
        XSetWindowBackgroundPixmap (dpy, info[screen].window,
                                    info[screen].screenshot);
      else
        {
          XSetWindowBackgroundPixmap (dpy, info[screen].window, None);
          XSetWindowBackground (dpy, info[screen].window, BlackPixel (dpy, 0));
        }
# endif /* USE_GL */

      XMapRaised (dpy, info[screen].window);

      /* Now that we have mapped the screenshot on the fader windows,
         take the saver windows off the screen. */
# if 0
      /* Under macOS X11 and Wayland, this causes a single frame of flicker.
         I don't see how that is possible, since we just did XMapRaised,
         above. */
      if (out_p)
        {
          XUnmapWindow (dpy, saver_windows[screen]);
          XClearWindow (dpy, saver_windows[screen]);
        }
# endif
    }

  /* Run the animation at the maximum frame rate in the time allotted. */
  {
    double start_time = double_time();
    double end_time = start_time + seconds;
    double prev = 0;
    double now;
    int frames = 0;
    double max = 1/60.0;  /* max FPS */
    while ((now = double_time()) < end_time)
      {
        double ratio = (end_time - now) / seconds;
        if (!out_p) ratio = 1-ratio;

        for (screen = 0; screen < nwindows; screen++)
# ifdef USE_GL
          if (opengl_whack (dpy, &info[screen], ratio))
            goto FAIL;
# else /* !USE_GL */
          if (xshm_whack (dpy, &shm_info, &info[screen], ratio))
            goto FAIL;
# endif /* !USE_GL */

        if (error_handler_hit_p)
          goto FAIL;
        if (user_active_p (app, dpy, out_p))
          {
            status = 1;   /* user activity status code */
            goto DONE;
          }
        frames++;

        if (now < prev + max)
          usleep (1000000 * (prev + max - now));
        prev = now;
      }

    if (verbose_p > 1)
      fprintf (stderr, "%s: %.0f FPS\n", blurb(), frames / (now - start_time));
  }

  status = 0;   /* completed fade with no user activity */

 DONE:

  /* If we're fading out, we have completed the transition from what was
     on the screen to black, using our fader windows.  Now raise the saver
     windows and take the fader windows off the screen.  Since they're both
     black, that will be imperceptible.   
   */
  if (out_p)
    {
      for (screen = 0; screen < nwindows; screen++)
	{
          XClearWindow (dpy, saver_windows[screen]);
	  XMapRaised (dpy, saver_windows[screen]);
          /* Doing this here triggers the same KDE 5 compositor bug that
             defer_XDestroyWindow is to work around. */
          /* if (info[screen].window)
            XUnmapWindow (dpy, info[screen].window); */
	}
    }

  XSync (dpy, False);

 FAIL:

  /* After fading in, take the saver windows off the screen before
     destroying the occluding screenshot windows. */
  if (!out_p)
    {
      for (screen = 0; screen < nwindows; screen++)
	{
          XUnmapWindow (dpy, saver_windows[screen]);
          XClearWindow (dpy, saver_windows[screen]);
	}
    }

  if (info)
    {
      for (screen = 0; screen < nwindows; screen++)
        {
# ifdef USE_GL
          if (info[screen].src)
            XDestroyImage (info[screen].src);
          if (info[screen].texid)
            glDeleteTextures (1, &info[screen].texid);
          openGL_destroy_context (dpy, info[screen].glx_context);
# else /* !USE_GL */
          if (info[screen].src)
            destroy_xshm_image (dpy, info[screen].src, &shm_info);
          if (info[screen].intermediate)
            destroy_xshm_image (dpy, info[screen].intermediate, &shm_info);
          if (info[screen].gc)
            XFreeGC (dpy, info[screen].gc);
# endif /* !USE_GL */

          if (info[screen].window)
            defer_XDestroyWindow (app, dpy, info[screen].window);
        }
      free (info);
    }

  /* If fading in, delete the screenshot pixmaps, and the list of them. */
  if (!out_p && saver_window)
    {
      for (screen = 0; screen < state->nscreens; screen++)
        if (state->screenshots[screen])
          XFreePixmap (dpy, state->screenshots[screen]);
      if (state->screenshots)
        free (state->screenshots);
      state->nscreens = 0;
      state->screenshots = 0;
    }

  XSync (dpy, False);
  XSetErrorHandler (old_handler);

  if (error_handler_hit_p) status = -1;
  if (verbose_p > 1 && status)
# ifdef HAVE_GL
    fprintf (stderr, "%s: GL fade %s failed\n",
             blurb(), (out_p ? "out" : "in"));
# else /* !HAVE_GL */
    fprintf (stderr, "%s: SHM fade %s failed\n",
             blurb(), (out_p ? "out" : "in"));
# endif /* !HAVE_GL */

  return status;
}


#ifdef USE_GL

static int
opengl_whack (Display *dpy, xshm_fade_info *info, float ratio)
{
  GLfloat w = info->texw;
  GLfloat h = info->texh;

  opengl_make_current (dpy, info);
  glBindTexture (GL_TEXTURE_2D, info->texid);
  glClear (GL_COLOR_BUFFER_BIT);

  if (ratio < 0) ratio = 0;
  if (ratio > 1) ratio = 1;

  glColor3f (ratio, ratio, ratio);

  glBegin (GL_QUADS);
  glTexCoord2f (0, 0); glVertex3f (0, 0, 0);
  glTexCoord2f (0, h); glVertex3f (0, 1, 0);
  glTexCoord2f (w, h); glVertex3f (1, 1, 0);
  glTexCoord2f (w, 0); glVertex3f (1, 0, 0);
  glEnd();
  glFinish();

# ifdef HAVE_EGL
  if (! eglSwapBuffers (info->glx_context->egl_display,
                        info->glx_context->egl_surface))
    return True;
# else
  glXSwapBuffers (dpy, info->window);
# endif

  if (check_gl_error ("gl whack")) return True;
  return False;
}


#else /* !USE_GL */

static int
xshm_whack (Display *dpy, XShmSegmentInfo *shm_info,
            xshm_fade_info *info, float ratio)
{
  unsigned char *inbits  = (unsigned char *) info->src->data;
  unsigned char *outbits = (unsigned char *) info->intermediate->data;
  unsigned char *end = (outbits + 
                        info->intermediate->bytes_per_line *
                        info->intermediate->height);
  unsigned char ramp[256];
  int i;

  XSync (dpy, False);

  if (ratio < 0) ratio = 0;
  if (ratio > 1) ratio = 1;

  for (i = 0; i < sizeof(ramp); i++)
    ramp[i] = i * ratio;
  while (outbits < end)
    *outbits++ = ramp[*inbits++];

  put_xshm_image (dpy, info->window, info->gc, info->intermediate, 0, 0, 0, 0,
                  info->intermediate->width, info->intermediate->height,
                  shm_info);
  XSync (dpy, False);
  return 0;
}

#endif /* !USE_GL */

