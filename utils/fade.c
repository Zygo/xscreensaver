/* xscreensaver, Copyright (c) 1992 Jamie Zawinski <jwz@lucid.com>
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
# define bcopy(from,to,size) memcpy((to),(from),(size))
# define bzero(addr,size) memset((addr),0,(size))
extern void screenhack_usleep (unsigned long);
#else
extern void screenhack_usleep ();
#endif

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
  ncolors = CellsOfScreen (DefaultScreenOfDisplay (dpy));
  if (ncolors <= 2 || ncolors > MAX_COLORS)
    return 0;
  if (! into_cmap)
    into_cmap = XCreateColormap (dpy, RootWindow (dpy, DefaultScreen (dpy)),
				 DefaultVisual (dpy, DefaultScreen (dpy)),
				 AllocAll);
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


void
fade_colormap (dpy, cmap, cmap2, seconds, ticks, out_p)
     Display *dpy;
     Colormap cmap, cmap2;
     int seconds, ticks;
     Bool out_p;
{
  int i;
  int steps = seconds * ticks;
  XEvent dummy_event;

  if (! cmap2)
    return;

  for (i = 0; i < ncolors; i++)
    orig_colors [i].pixel = i;
  XQueryColors (dpy, cmap, orig_colors, ncolors);
  bcopy (orig_colors, current_colors, ncolors * sizeof (XColor));

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
	  return;
	}

      usleep (1000000 / (ticks * 2)); /* the 2 is a hack... */
    }
}

#if 0


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

  while (1)
    {
      XSync (dpy, False);
      XGrabServer (dpy);
      XInstallColormap (dpy, cmap2);
      fade_colormap (dpy, cmap, cmap2, seconds, ticks, True);
      if (delay) sleep (delay);
      fade_colormap (dpy, cmap, cmap2, seconds, ticks, False);
      XInstallColormap (dpy, cmap);
      XUngrabServer (dpy);
      XSync (dpy, False);
      if (delay) sleep (delay);
    }
}

#endif
