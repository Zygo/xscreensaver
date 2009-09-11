/* xscreensaver, Copyright (c) 1992-2008 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include "utils.h"

#include <sys/time.h> /* for gettimeofday() */

#ifdef VMS
# include "vms-gtod.h"
#endif /* VMS */

#include "visual.h"
#include "usleep.h"
#include "fade.h"

Colormap
copy_colormap (Screen *screen, Visual *visual,
	       Colormap cmap, Colormap into_cmap)
{
  int i;
  Display *dpy = DisplayOfScreen (screen);
  Window window = RootWindowOfScreen (screen);
  int ncolors = CellsOfScreen (screen);
  XColor *colors = 0;

  /* If this is a colormap on a mono visual, or one with insanely many
     color cells, bug out. */
  if (ncolors <= 2 || ncolors > 4096)
    return 0;
  /* If this is a non-writable visual, bug out. */
  if (!has_writable_cells (screen, visual))
    return 0;

  if (! into_cmap)
    into_cmap = XCreateColormap (dpy, window, visual, AllocAll);
  if (! cmap)
    cmap = DefaultColormapOfScreen (screen);

  colors = (XColor *) calloc(sizeof(XColor), ncolors);
  for (i = 0; i < ncolors; i++)
    colors [i].pixel = i;
  XQueryColors (dpy, cmap, colors, ncolors);
  XStoreColors (dpy, into_cmap, colors, ncolors);
  free (colors);
  return into_cmap;
}


void
blacken_colormap (Screen *screen, Colormap cmap)
{
  Display *dpy = DisplayOfScreen (screen);
  int ncolors = CellsOfScreen (screen);
  XColor *colors;
  int i;
  if (ncolors > 4096)
    return;
  colors = (XColor *) calloc(sizeof(XColor), ncolors);
  for (i = 0; i < ncolors; i++)
    colors[i].pixel = i;
  XStoreColors (dpy, cmap, colors, ncolors);
  free (colors);
}



static void fade_screens_1 (Display *dpy, Colormap *cmaps,
			    Window *black_windows, int nwindows,
                            int seconds, int ticks,
			    Bool out_p, Bool clear_windows);

#ifdef HAVE_SGI_VC_EXTENSION
static int sgi_gamma_fade (Display *dpy,
			   Window *black_windows, int nwindows,
                           int seconds, int ticks,
			   Bool out_p, Bool clear_windows);
#endif /* HAVE_SGI_VC_EXTENSION */

#ifdef HAVE_XF86VMODE_GAMMA
static int xf86_gamma_fade (Display *dpy,
                            Window *black_windows, int nwindows,
                            int seconds, int ticks,
                            Bool out_p, Bool clear_windows);
#endif /* HAVE_XF86VMODE_GAMMA */


void
fade_screens (Display *dpy, Colormap *cmaps,
              Window *black_windows, int nwindows,
	      int seconds, int ticks,
	      Bool out_p, Bool clear_windows)
{
  int oseconds = seconds;
  Bool was_in_p = !out_p;

  /* When we're asked to fade in, first fade out, then fade in.
     That way all the transitions are smooth -- from what's on the
     screen, to black, to the desktop.
   */
  if (was_in_p)
    {
      clear_windows = True;
      out_p = True;
      seconds /= 3;
      if (seconds == 0)
	seconds = 1;
    }

 AGAIN:

/* #### printf("\n\nfade_screens %d %d %d\n", seconds, ticks, out_p); */

#ifdef HAVE_SGI_VC_EXTENSION
  /* First try to do it by fading the gamma in an SGI-specific way... */
  if (0 == sgi_gamma_fade(dpy, black_windows, nwindows,
                          seconds, ticks, out_p,
			  clear_windows))
    ;
  else
#endif /* HAVE_SGI_VC_EXTENSION */

#ifdef HAVE_XF86VMODE_GAMMA
  /* Then try to do it by fading the gamma in an XFree86-specific way... */
  if (0 == xf86_gamma_fade(dpy, black_windows, nwindows,
                           seconds, ticks, out_p,
                           clear_windows))
    ;
  else
#endif /* HAVE_XF86VMODE_GAMMA */

    /* Else, do it the old-fashioned way, which (somewhat) loses if
       there are TrueColor windows visible. */
    fade_screens_1 (dpy, cmaps, black_windows, nwindows,
                    seconds, ticks,
		    out_p, clear_windows);

  /* If we were supposed to be fading in, do so now (we just faded out,
     so now fade back in.)
   */
  if (was_in_p)
    {
      was_in_p = False;
      out_p = False;
      seconds = oseconds * 2 / 3;
      if (seconds == 0)
	seconds = 1;
      goto AGAIN;
    }
}


static void
sleep_from (struct timeval *now, struct timeval *then, long usecs_per_step)
{
  /* If several seconds have passed, the machine must have been asleep
     or thrashing or something.  Don't sleep in that case, to avoid
     overflowing and sleeping for an unconscionably long time.  This
     function should only be sleeping for very short periods.
   */
  if (now->tv_sec - then->tv_sec < 5)
    {
      long diff = (((now->tv_sec - then->tv_sec) * 1000000) +
                   now->tv_usec - then->tv_usec);
      if (usecs_per_step > diff)
        usleep (usecs_per_step - diff);
    }

  then->tv_sec  = now->tv_sec;
  then->tv_usec = now->tv_usec;
}



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
 */
static void
fade_screens_1 (Display *dpy, Colormap *cmaps,
                Window *black_windows, int nwindows,
		int seconds, int ticks,
		Bool out_p, Bool clear_windows)
{
  int i, j, k;
  int steps = seconds * ticks;
  long usecs_per_step = (long)(seconds * 1000000) / (long)steps;
  XEvent dummy_event;
  int cmaps_per_screen = 5;
  int nscreens = ScreenCount(dpy);
  int ncmaps = nscreens * cmaps_per_screen;
  Colormap *fade_cmaps = 0;
  Bool installed = False;
  int total_ncolors;
  XColor *orig_colors, *current_colors, *screen_colors, *orig_screen_colors;
  struct timeval then, now;
#ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone tzp;
#endif

  total_ncolors = 0;
  for (i = 0; i < nscreens; i++)
    total_ncolors += CellsOfScreen (ScreenOfDisplay(dpy, i));

  orig_colors    = (XColor *) calloc(sizeof(XColor), total_ncolors);
  current_colors = (XColor *) calloc(sizeof(XColor), total_ncolors);

  /* Get the contents of the colormap we are fading from or to. */
  screen_colors = orig_colors;
  for (i = 0; i < nscreens; i++)
    {
      int ncolors = CellsOfScreen (ScreenOfDisplay (dpy, i));
      Colormap cmap = (cmaps ? cmaps[i] : 0);
      if (!cmap) cmap = DefaultColormap(dpy, i);

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

#ifdef GETTIMEOFDAY_TWO_ARGS
  gettimeofday(&then, &tzp);
#else
  gettimeofday(&then);
#endif

  /* Iterate by steps of the animation... */
  for (i = (out_p ? steps : 0);
       (out_p ? i > 0 : i < steps);
       (out_p ? i-- : i++))
    {

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
	      screen_colors[k].red   = orig_screen_colors[k].red   * i / steps;
	      screen_colors[k].green = orig_screen_colors[k].green * i / steps;
	      screen_colors[k].blue  = orig_screen_colors[k].blue  * i / steps;
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

      /* Put the maps on the screens, and then take the windows off the screen.
	 (only need to do this the first time through the loop.)
       */
      if (!installed)
	{
	  for (j = 0; j < ncmaps; j++)
	    if (fade_cmaps[j])
	      XInstallColormap (dpy, fade_cmaps[j]);
	  installed = True;

	  if (black_windows && !out_p)
	    for (j = 0; j < nwindows; j++)
	      if (black_windows[j])
		{
		  XUnmapWindow (dpy, black_windows[j]);
		  XClearWindow (dpy, black_windows[j]);
		}
	}

      XSync (dpy, False);

      /* If there is user activity, bug out.  (Bug out on keypresses or
	 mouse presses, but not motion, and not release events.  Bugging
	 out on motion made the unfade hack be totally useless, I think.)

	 We put the event back so that the calling code can notice it too.
	 It would be better to not remove it at all, but that's harder
	 because Xlib has such a non-design for this kind of crap, and
	 in this application it doesn't matter if the events end up out
	 of order, so in the grand unix tradition we say "fuck it" and
	 do something that mostly works for the time being.
       */
      if (XCheckMaskEvent (dpy, (KeyPressMask|ButtonPressMask), &dummy_event))
	{
	  XPutBackEvent (dpy, &dummy_event);
	  goto DONE;
	}

#ifdef GETTIMEOFDAY_TWO_ARGS
      gettimeofday(&now, &tzp);
#else
      gettimeofday(&now);
#endif

      /* If we haven't already used up our alotted time, sleep to avoid
	 changing the colormap too fast. */
      sleep_from (&now, &then, usecs_per_step);
    }

 DONE:

  if (orig_colors)    free (orig_colors);
  if (current_colors) free (current_colors);

  /* If we've been given windows to raise after blackout, raise them before
     releasing the colormaps.
   */
  if (out_p && black_windows)
    {
      for (i = 0; i < nwindows; i++)
	{
	  if (clear_windows)
	    XClearWindow (dpy, black_windows[i]);
	  XMapRaised (dpy, black_windows[i]);
	}
      XSync(dpy, False);
    }

  /* Now put the target maps back.
     If we're fading out, use the given cmap (or the default cmap, if none.)
     If we're fading in, always use the default cmap.
   */
  for (i = 0; i < nscreens; i++)
    {
      Colormap cmap = (cmaps ? cmaps[i] : 0);
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
  free(fade_cmaps);
  fade_cmaps = 0;
}



/* SGI Gamma fading */

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

static int
sgi_gamma_fade (Display *dpy,
		Window *black_windows, int nwindows,
                int seconds, int ticks,
		Bool out_p, Bool clear_windows)
{
  int steps = seconds * ticks;
  long usecs_per_step = (long)(seconds * 1000000) / (long)steps;
  XEvent dummy_event;
  int nscreens = ScreenCount(dpy);
  struct timeval then, now;
#ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone tzp;
#endif
  int i, screen;
  int status = -1;
  struct screen_sgi_gamma_info *info = (struct screen_sgi_gamma_info *)
    calloc(nscreens, sizeof(*info));

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
	if (black_windows && black_windows[screen])
	  {
	    XUnmapWindow (dpy, black_windows[screen]);
	    XClearWindow (dpy, black_windows[screen]);
	    XSync(dpy, False);
	  }
    }

  /* Iterate by steps of the animation... */
  for (i = (out_p ? steps : 0);
       (out_p ? i > 0 : i < steps);
       (out_p ? i-- : i++))
    {
      for (screen = 0; screen < nscreens; screen++)
	{
	  sgi_whack_gamma(dpy, screen, &info[screen],
                          (((float)i) / ((float)steps)));

	  /* If there is user activity, bug out.  (Bug out on keypresses or
	     mouse presses, but not motion, and not release events.  Bugging
	     out on motion made the unfade hack be totally useless, I think.)

	     We put the event back so that the calling code can notice it too.
	     It would be better to not remove it at all, but that's harder
	     because Xlib has such a non-design for this kind of crap, and
	     in this application it doesn't matter if the events end up out
	     of order, so in the grand unix tradition we say "fuck it" and
	     do something that mostly works for the time being.
	   */
	  if (XCheckMaskEvent (dpy, (KeyPressMask|ButtonPressMask),
			       &dummy_event))
	    {
	      XPutBackEvent (dpy, &dummy_event);
	      goto DONE;
	    }

#ifdef GETTIMEOFDAY_TWO_ARGS
	  gettimeofday(&now, &tzp);
#else
	  gettimeofday(&now);
#endif

	  /* If we haven't already used up our alotted time, sleep to avoid
	     changing the colormap too fast. */
          sleep_from (&now, &then, usecs_per_step);
	}
    }
  

 DONE:

  if (out_p && black_windows)
    {
      for (screen = 0; screen < nwindows; screen++)
	{
	  if (clear_windows)
	    XClearWindow (dpy, black_windows[screen]);
	  XMapRaised (dpy, black_windows[screen]);
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

  status = 0;

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

  return status;
}

static void
sgi_whack_gamma(Display *dpy, int screen, struct screen_sgi_gamma_info *info,
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



/* XFree86 4.x+ Gamma fading */

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

static int
xf86_gamma_fade (Display *dpy,
                 Window *black_windows, int nwindows,
                 int seconds, int ticks,
                 Bool out_p, Bool clear_windows)
{
  int steps = seconds * ticks;
  long usecs_per_step = (long)(seconds * 1000000) / (long)steps;
  XEvent dummy_event;
  int nscreens = ScreenCount(dpy);
  struct timeval then, now;
#ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone tzp;
#endif
  int i, screen;
  int status = -1;
  xf86_gamma_info *info = 0;

  static int ext_ok = -1;

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
	xf86_whack_gamma(dpy, screen, &info[screen], 0.0);
      for (screen = 0; screen < nwindows; screen++)
	if (black_windows && black_windows[screen])
	  {
	    XUnmapWindow (dpy, black_windows[screen]);
	    XClearWindow (dpy, black_windows[screen]);
	    XSync(dpy, False);
	  }
    }

  /* Iterate by steps of the animation... */
  for (i = (out_p ? steps : 0);
       (out_p ? i > 0 : i < steps);
       (out_p ? i-- : i++))
    {
      for (screen = 0; screen < nscreens; screen++)
	{
	  xf86_whack_gamma(dpy, screen, &info[screen],
                           (((float)i) / ((float)steps)));

	  /* If there is user activity, bug out.  (Bug out on keypresses or
	     mouse presses, but not motion, and not release events.  Bugging
	     out on motion made the unfade hack be totally useless, I think.)

	     We put the event back so that the calling code can notice it too.
	     It would be better to not remove it at all, but that's harder
	     because Xlib has such a non-design for this kind of crap, and
	     in this application it doesn't matter if the events end up out
	     of order, so in the grand unix tradition we say "fuck it" and
	     do something that mostly works for the time being.
	   */
	  if (XCheckMaskEvent (dpy, (KeyPressMask|ButtonPressMask),
			       &dummy_event))
	    {
	      XPutBackEvent (dpy, &dummy_event);
	      goto DONE;
	    }

#ifdef GETTIMEOFDAY_TWO_ARGS
	  gettimeofday(&now, &tzp);
#else
	  gettimeofday(&now);
#endif

	  /* If we haven't already used up our alotted time, sleep to avoid
	     changing the colormap too fast. */
          sleep_from (&now, &then, usecs_per_step);
	}
    }
  

 DONE:

  if (out_p && black_windows)
    {
      for (screen = 0; screen < nwindows; screen++)
	{
	  if (clear_windows)
	    XClearWindow (dpy, black_windows[screen]);
	  XMapRaised (dpy, black_windows[screen]);
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

  status = 0;

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

  return status;
}


/* This bullshit is needed because the VidMode extension doesn't work
   on remote displays -- but if the remote display has the extension
   at all, XF86VidModeQueryExtension returns true, and then
   XF86VidModeQueryVersion dies with an X error.  Thank you XFree,
   may I have another.
 */

static Bool error_handler_hit_p = False;

static int
ignore_all_errors_ehandler (Display *dpy, XErrorEvent *error)
{
  error_handler_hit_p = True;
  return 0;
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
  Bool status;

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

      status = XF86VidModeSetGamma (dpy, screen, &g2);
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

      status = XF86VidModeSetGammaRamp(dpy, screen, info->size, r, g, b);

      free (r);
      free (g);
      free (b);

# else  /* !HAVE_XF86VMODE_GAMMA_RAMP */
      abort();
# endif /* !HAVE_XF86VMODE_GAMMA_RAMP */
    }

  XSync(dpy, False);
  return status;
}

#endif /* HAVE_XF86VMODE_GAMMA */
