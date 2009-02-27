/* xscreensaver, Copyright (c) 1992-1997 Jamie Zawinski <jwz@netscape.com>
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


/* The business with `cmaps_per_screen' is to fake out the SGI 8-bit video
   hardware, which is capable of installing multiple (4) colormaps
   simultaniously.  We have to install multiple copies of the same set of
   colors in order to fill up all the available slots in the hardware color
   lookup table, so we install an extra N colormaps per screen to make sure
   that all screens really go black.  */

void
fade_screens (Display *dpy, Colormap *cmaps,
	      int seconds, int ticks,
	      Bool out_p)
{
  int i, j, k;
  int steps = seconds * ticks;
  long usecs_per_step = (long)(seconds * 1000000) / (long)steps;
  XEvent dummy_event;
  int cmaps_per_screen = 5;
  int nscreens = ScreenCount(dpy);
  int ncmaps = nscreens * cmaps_per_screen;
  static Colormap *fade_cmaps = 0;
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

      /* Put the maps on the screens...
	 (only need to do this the first time through the loop.)
       */
      if (!installed)
	{
	  for (j = 0; j < ncmaps; j++)
	    if (fade_cmaps[j])
	      XInstallColormap (dpy, fade_cmaps[j]);
	  installed = True;
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
      {
	long diff = (((now.tv_sec - then.tv_sec) * 1000000) +
		     now.tv_usec - then.tv_usec);
	then.tv_sec = now.tv_sec;
	then.tv_usec = now.tv_usec;
	if (usecs_per_step > diff)
	  usleep (usecs_per_step - diff);
      }
    }

 DONE:

  if (orig_colors)    free (orig_colors);
  if (current_colors) free (current_colors);

  /* Now put the original maps back, if we want to end up with them.
   */
  if (!out_p)
    {
      for (i = 0; i < nscreens; i++)
	{
	  Colormap cmap = (cmaps ? cmaps[i] : 0);
	  if (!cmap) cmap = DefaultColormap(dpy, i);
	  XInstallColormap (dpy, cmap);
	}

      /* We've faded to the default cmaps, so we don't need the black maps
	 on stage any more.  (We can't uninstall these maps yet if we've
	 faded to black, because that would lead to flicker between when
	 we uninstalled them and when the caller raised its black window.)
       */
      for (i = 0; i < ncmaps; i++)
	if (fade_cmaps[i])
	  {
	    XFreeColormap(dpy, fade_cmaps[i]);
	    fade_cmaps[i] = 0;
	  }
      free(fade_cmaps);
      fade_cmaps = 0;
    }
}


#if 0
#include "screenhack.h"

char *progclass = "foo";
char *defaults [] = {
  0
};

XrmOptionDescRec options [] = {0};
int options_size = 0;

void
screenhack (dpy, w)
     Display *dpy;
     Window w;
{
  int seconds = 3;
  int ticks = 20;
  int delay = 1;

  while (1)
    {
      XSync (dpy, False);

      fprintf(stderr,"out..."); fflush(stderr);
      fade_screens (dpy, 0, seconds, ticks, True);
      fprintf(stderr, "done.\n"); fflush(stderr);

      if (delay) sleep (delay);

      fprintf(stderr,"in..."); fflush(stderr);
      fade_screens (dpy, 0, seconds, ticks, False);
      fprintf(stderr, "done.\n"); fflush(stderr);

      if (delay) sleep (delay);
    }
}

#endif
