/* xscreensaver, Copyright (c) 1992-1996 Jamie Zawinski <jwz@netscape.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#if __STDC__
# include <stdlib.h>
#endif

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xos.h>

#if __STDC__
# define P(x)x
#else
# define P(x)()
#endif

extern int get_visual_class P((Display *, Visual *));
extern void screenhack_usleep P((unsigned long));
#define usleep screenhack_usleep

#define MAX_COLORS 4096
static XColor orig_colors [MAX_COLORS];
static XColor current_colors [MAX_COLORS];
static int ncolors;

Colormap
copy_colormap (dpy, cmap, into_cmap)
     Display *dpy;
     Colormap cmap, into_cmap;
{
  int i;
  Screen *screen = DefaultScreenOfDisplay (dpy);
  Visual *visual = DefaultVisualOfScreen (screen);
  Window window = RootWindowOfScreen (screen);
  int vclass = get_visual_class (dpy, visual);

  ncolors = CellsOfScreen (screen);

  /* If this is a colormap on a mono visual, or one with insanely many
     color cells, bug out. */
  if (ncolors <= 2 || ncolors > MAX_COLORS)
    return 0;
  /* If this is a non-writable visual, bug out. */
  if (vclass == StaticGray || vclass == StaticColor || vclass == TrueColor)
    return 0;

  if (! into_cmap)
    into_cmap = XCreateColormap (dpy, window, visual, AllocAll);
  if (! cmap)
    cmap = DefaultColormap (dpy, DefaultScreen (dpy));
  for (i = 0; i < ncolors; i++)
    orig_colors [i].pixel = i;
  XQueryColors (dpy, cmap, orig_colors, ncolors);
  XStoreColors (dpy, into_cmap, orig_colors, ncolors);
  return into_cmap;
}

void
blacken_colormap (dpy, cmap)
     Display *dpy;
     Colormap cmap;
{
  int i;
  for (i = 0; i < ncolors; i++)
    {
      current_colors [i].pixel = i;
      current_colors [i].red = current_colors [i].green =
	current_colors [i].blue = 0;
    }
  XStoreColors (dpy, cmap, current_colors, ncolors);
}


/* The business with `install_p' and `extra_cmaps' is to fake out the SGI
   8-bit video hardware, which is capable of installing multiple (4) colormaps
   simultaniously.  We have to install multiple copies of the same set of
   colors in order to fill up all the available slots in the hardware color
   lookup table.
 */

void
fade_colormap (dpy, cmap, cmap2, seconds, ticks, out_p, install_p)
     Display *dpy;
     Colormap cmap, cmap2;
     int seconds, ticks;
     Bool out_p;
     Bool install_p;
{
  int i;
  int steps = seconds * ticks;
  XEvent dummy_event;

  Screen *screen = DefaultScreenOfDisplay (dpy);
  Visual *visual = DefaultVisualOfScreen (screen);
  Window window = RootWindowOfScreen (screen);
  static Colormap extra_cmaps[4] = { 0, };
  int n_extra_cmaps = sizeof(extra_cmaps)/sizeof(*extra_cmaps);

  if (! cmap2)
    return;

  for (i = 0; i < ncolors; i++)
    orig_colors [i].pixel = i;
  XQueryColors (dpy, cmap, orig_colors, ncolors);
  memcpy (current_colors, orig_colors, ncolors * sizeof (XColor));

  if (install_p)
    for (i=0; i < n_extra_cmaps; i++)
      if (!extra_cmaps[i])
	extra_cmaps[i] = XCreateColormap (dpy, window, visual, AllocAll);

  for (i = (out_p ? steps : 0);
       (out_p ? i > 0 : i < steps);
       (out_p ? i-- : i++))
    {
      int j;
      for (j = 0; j < ncolors; j++)
	{
	  current_colors[j].red   = orig_colors[j].red   * i / steps;
	  current_colors[j].green = orig_colors[j].green * i / steps;
	  current_colors[j].blue  = orig_colors[j].blue  * i / steps;
	}
      XStoreColors (dpy, cmap2, current_colors, ncolors);

      if (install_p)
	{
	  for (j=0; j < n_extra_cmaps; j++)
	    if (extra_cmaps[j])
	      XStoreColors (dpy, extra_cmaps[j], current_colors, ncolors);

	  for (j=0; j < n_extra_cmaps; j++)
	    if (extra_cmaps[j])
	      XInstallColormap (dpy, extra_cmaps[j]);
	  XInstallColormap (dpy, cmap2);
	}

      XSync (dpy, False);

      /* If there is user activity, bug out.
	 We put the event back so that the calling code can notice it too.
	 It would be better to not remove it at all, but that's harder
	 because Xlib has such a non-design for this kind of crap, and
	 in this application it doesn't matter if the events end up out
	 of order, so in the grand unix tradition we say "fuck it" and
	 do something that mostly works for the time being.
       */
      if (XCheckMaskEvent (dpy, (KeyPressMask | KeyReleaseMask |
				 ButtonPressMask | ButtonReleaseMask |
				 PointerMotionMask),
			   &dummy_event))
	{
	  XPutBackEvent (dpy, &dummy_event);
	  goto DONE;
	}

      usleep (1000000 / (ticks * 2)); /* the 2 is a hack... */
    }

DONE:

  if (install_p)
    {
      XInstallColormap (dpy, cmap2);
/*      for (i=0; i < n_extra_cmaps; i++)
	if (extra_cmaps[i])
	  XFreeColormap (dpy, extra_cmaps[i]);
 */
    }
}


#if 0
#include "../hacks/screenhack.h"
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
  Colormap cmap = DefaultColormap (dpy, DefaultScreen (dpy));
  Colormap cmap2 = copy_colormap (dpy, cmap, 0);

  int seconds = 1;
  int ticks = 30 * seconds;
  int delay = 1;

  XSynchronize (dpy, True);

  while (1)
    {
      XSync (dpy, False);
/*      XGrabServer (dpy); */
      fprintf(stderr,"out..."); fflush(stderr);
      XInstallColormap (dpy, cmap2);
      fade_colormap (dpy, cmap, cmap2, seconds, ticks, True, True);
      fprintf(stderr, "done.\n"); fflush(stderr);
      if (delay) sleep (delay);
      fprintf(stderr,"in..."); fflush(stderr);
      fade_colormap (dpy, cmap, cmap2, seconds, ticks, False, True);
      XInstallColormap (dpy, cmap);
      fprintf(stderr, "done.\n"); fflush(stderr);
      XUngrabServer (dpy);
      XSync (dpy, False);
      if (delay) sleep (delay);
    }
}

#endif
