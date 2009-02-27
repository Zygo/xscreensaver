/* xscreensaver, Copyright (c) 1993 Jamie Zawinski <jwz@lucid.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This file contains some code for intelligently picking the best visual
   (where "best" is somewhat biased in the direction of writable cells...)
 */

#if __STDC__
#include <stdlib.h>
#endif

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifndef isupper
# define isupper(c)  ((c) >= 'A' && (c) <= 'Z')
#endif
#ifndef _tolower
# define _tolower(c)  ((c) - 'A' + 'a')
#endif

extern char *progname;
extern char *get_string_resource ();

static Visual *
pick_best_visual_of_class (display, visual_class)
     Display *display;
     int visual_class;
{
  XVisualInfo vi_in, *vi_out;
  int out_count;

  vi_in.class = visual_class;
  vi_in.screen = DefaultScreen (display);
  vi_out = XGetVisualInfo (display, VisualClassMask|VisualScreenMask,
			   &vi_in, &out_count);
  if (vi_out)
    {       /* choose the 'best' one, if multiple */
      int i, best;
      Visual *visual;
      for (i = 0, best = 0; i < out_count; i++)
	if (vi_out [i].depth > vi_out [best].depth)
	  best = i;
      visual = vi_out [best].visual;
      XFree ((char *) vi_out);
      return visual;
    }
  else
    return 0;
}

static Visual *
pick_best_visual (display)
     Display *display;
{
  Visual *visual;
  if (visual = pick_best_visual_of_class (display, PseudoColor))
    return visual;
  if (visual = pick_best_visual_of_class (display, DirectColor))
    return visual;
  if (visual = pick_best_visual_of_class (display, GrayScale))
    return visual;
  if (visual = pick_best_visual_of_class (display, StaticGray))
    return visual;
  return DefaultVisual (display, DefaultScreen (display));
}


static Visual *
id_to_visual (dpy, id)
     Display *dpy;
     int id;
{
  XVisualInfo vi_in, *vi_out;
  int out_count;
  vi_in.screen = DefaultScreen (dpy);
  vi_in.visualid = id;
  vi_out = XGetVisualInfo (dpy, VisualScreenMask|VisualIDMask,
			   &vi_in, &out_count);
  if (vi_out)
    {
      Visual *v = vi_out[0].visual;
      XFree ((char *) vi_out);
      return v;
    }
  return 0;
}

int
get_visual_depth (dpy, visual)
     Display *dpy;
     Visual *visual;
{
  XVisualInfo vi_in, *vi_out;
  int out_count, d;
  vi_in.screen = DefaultScreen (dpy);
  vi_in.visualid = XVisualIDFromVisual (visual);
  vi_out = XGetVisualInfo (dpy, VisualScreenMask|VisualIDMask,
			   &vi_in, &out_count);
  if (! vi_out) abort ();
  d = vi_out [0].depth;
  XFree ((char *) vi_out);
  return d;
}


int
get_visual_class (dpy, visual)
     Display *dpy;
     Visual *visual;
{
  XVisualInfo vi_in, *vi_out;
  int out_count, c;
  vi_in.screen = DefaultScreen (dpy);
  vi_in.visualid = XVisualIDFromVisual (visual);
  vi_out = XGetVisualInfo (dpy, VisualScreenMask|VisualIDMask,
			   &vi_in, &out_count);
  if (! vi_out) abort ();
  c = vi_out [0].class;
  XFree ((char *) vi_out);
  return c;
}


Visual *
get_visual_resource (dpy, name, class)
     Display *dpy;
     char *name, *class;
{
  char c, *s = get_string_resource (name, class);
  char *tmp;
  int vclass;
  int id;

  if (s)
    for (tmp = s; *tmp; tmp++)
      if (isupper (*tmp)) *tmp = _tolower (*tmp);

  if      (!s || !strcmp (s, "best"))  vclass = -1;
  else if (!strcmp (s, "default"))     vclass = -2;
  else if (!strcmp (s, "staticgray"))  vclass = StaticGray;
  else if (!strcmp (s, "staticcolor")) vclass = StaticColor;
  else if (!strcmp (s, "truecolor"))   vclass = TrueColor;
  else if (!strcmp (s, "grayscale"))   vclass = GrayScale;
  else if (!strcmp (s, "pseudocolor")) vclass = PseudoColor;
  else if (!strcmp (s, "directcolor")) vclass = DirectColor;
  else if (1 == sscanf (s, " %ld %c", &id, &c))   vclass = -3;
  else if (1 == sscanf (s, " 0x%lx %c", &id, &c)) vclass = -3;
  else
    {
      fprintf (stderr, "%s: unrecognized visual \"%s\".\n", progname, s);
      vclass = -1;
    }
  if (s) free (s);

  if (vclass == -1)
    return pick_best_visual (dpy);
  else if (vclass == -2)
    return DefaultVisual (dpy, DefaultScreen (dpy));
  else if (vclass == -3)
    {
      Visual *v = id_to_visual (dpy, id);
      if (v) return v;
      fprintf (stderr, "%s: no visual with id 0x%x.\n", progname, id);
      return pick_best_visual (dpy);
    }
  else
    return pick_best_visual_of_class (dpy, vclass);
}

void
describe_visual (f, dpy, visual)
     FILE *f;
     Display *dpy;
     Visual *visual;
{
  XVisualInfo vi_in, *vi_out;
  int out_count, d;
  vi_in.screen = DefaultScreen (dpy);
  vi_in.visualid = XVisualIDFromVisual (visual);
  vi_out = XGetVisualInfo (dpy, VisualScreenMask|VisualIDMask,
			   &vi_in, &out_count);
  if (! vi_out) abort ();
  fprintf (f, "0x%02x (%s depth: %2d, cmap: %3d @ %d)\n", vi_out->visualid,
	   (vi_out->class == StaticGray  ? "StaticGray, " :
	    vi_out->class == StaticColor ? "StaticColor," :
	    vi_out->class == TrueColor   ? "TrueColor,  " :
	    vi_out->class == GrayScale   ? "GrayScale,  " :
	    vi_out->class == PseudoColor ? "PseudoColor," :
	    vi_out->class == DirectColor ? "DirectColor," :
	    "???"),
	   vi_out->depth, vi_out->colormap_size, vi_out->bits_per_rgb);
  XFree ((char *) vi_out);
}
