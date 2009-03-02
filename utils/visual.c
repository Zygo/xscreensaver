/* xscreensaver, Copyright (c) 1993, 1994, 1995 Jamie Zawinski <jwz@mcom.com>
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
   (where "best" is biased in the direction of high color counts...)
 */

#if __STDC__
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#endif

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#if __STDC__
# define P(x)x
#else
#define P(x)()
#endif

#ifndef isupper
# define isupper(c)  ((c) >= 'A' && (c) <= 'Z')
#endif
#ifndef _tolower
# define _tolower(c)  ((c) - 'A' + 'a')
#endif

extern char *progname;
extern char *get_string_resource P((char *, char *));

static Visual *pick_best_visual P ((Screen *));
static Visual *pick_best_visual_of_class P((Screen *, int));
static Visual *id_to_visual P((Screen *, int));
static int screen_number P((Screen *));
static Visual *id_to_visual P((Screen *screen, int id));
int get_visual_depth P((Display *dpy, Visual *visual));


#define DEFAULT_VISUAL	-1
#define BEST_VISUAL	-2
#define SPECIFIC_VISUAL	-3

Visual *
get_visual_resource (dpy, name, class)
     Display *dpy;
     char *name, *class;
{
  Screen *screen = DefaultScreenOfDisplay (dpy);
  char c, *v = get_string_resource (name, class);
  char *tmp;
  int vclass;
  unsigned long id;

  if (v)
    for (tmp = v; *tmp; tmp++)
      if (isupper (*tmp)) *tmp = _tolower (*tmp);

  if (!v)					  vclass = BEST_VISUAL;
  else if (!strcmp (v, "default"))		  vclass = DEFAULT_VISUAL;
  else if (!strcmp (v, "best")) 		  vclass = BEST_VISUAL;
  else if (!strcmp (v, "staticgray"))	 	  vclass = StaticGray;
  else if (!strcmp (v, "staticcolor"))		  vclass = StaticColor;
  else if (!strcmp (v, "truecolor"))		  vclass = TrueColor;
  else if (!strcmp (v, "grayscale"))		  vclass = GrayScale;
  else if (!strcmp (v, "pseudocolor"))		  vclass = PseudoColor;
  else if (!strcmp (v, "directcolor"))		  vclass = DirectColor;
  else if (1 == sscanf (v, " %ld %c", &id, &c))	  vclass = SPECIFIC_VISUAL;
  else if (1 == sscanf (v, " 0x%lx %c", &id, &c)) vclass = SPECIFIC_VISUAL;
  else
    {
      fprintf (stderr, "%s: unrecognized visual \"%s\".\n", progname, v);
      vclass = DEFAULT_VISUAL;
    }
  if (v) free (v);

  if (vclass == DEFAULT_VISUAL)
    return DefaultVisualOfScreen (screen);
  else if (vclass == BEST_VISUAL)
    return pick_best_visual (screen);
  else if (vclass == SPECIFIC_VISUAL)
    {
      Visual *visual = id_to_visual (screen, id);
      if (visual) return visual;
      fprintf (stderr, "%s: no visual with id 0x%x.\n", progname,
	       (unsigned int) id);
      return DefaultVisualOfScreen (screen);
    }
  else
    {
      Visual *visual = pick_best_visual_of_class (screen, vclass);
      if (visual) return visual;
      fprintf (stderr, "%s: no visual of class %s.\n", progname, v);
      return DefaultVisualOfScreen (screen);
    }
}

static Visual *
pick_best_visual (screen)
	Screen *screen;
{
  /* The "best" visual is the one on which we can allocate the largest
     range and number of colors.

     Therefore, a TrueColor visual which is at least 16 bits deep is best.
     (The assumption here being that a TrueColor of less than 16 bits is
     really just a PseudoColor visual with a pre-allocated color cube.)

     The next best thing is a PseudoColor visual of any type.  After that
     come the non-colormappable visuals, and non-color visuals.
   */
  Display *dpy = DisplayOfScreen (screen);
  Visual *visual;
  if ((visual = pick_best_visual_of_class (screen, TrueColor)) &&
      get_visual_depth (dpy, visual) >= 16)
    return visual;
  if ((visual = pick_best_visual_of_class (screen, PseudoColor)))
    return visual;
  if ((visual = pick_best_visual_of_class (screen, TrueColor)))
    return visual;
  if ((visual = pick_best_visual_of_class (screen, DirectColor)))
    return visual;
  if ((visual = pick_best_visual_of_class (screen, GrayScale)))
    return visual;
  if ((visual = pick_best_visual_of_class (screen, StaticGray)))
    return visual;
  return DefaultVisualOfScreen (screen);
}

static Visual *
pick_best_visual_of_class (screen, visual_class)
     Screen *screen;
     int visual_class;
{
  /* The best visual of a class is the one which on which we can allocate
     the largest range and number of colors, which means the one with the
     greatest depth and number of cells.
   */
  Display *dpy = DisplayOfScreen (screen);
  XVisualInfo vi_in, *vi_out;
  int out_count;

  vi_in.class = visual_class;
  vi_in.screen = screen_number (screen);
  vi_out = XGetVisualInfo (dpy, (VisualClassMask | VisualScreenMask),
			   &vi_in, &out_count);
  if (vi_out)
    {
      /* choose the 'best' one, if multiple */
      int i, best;
      Visual *visual;
      for (i = 0, best = 0; i < out_count; i++)
	/* It's better if it's deeper, or if it's the same depth with
	   more cells (does that ever happen?  Well, it could...) */
	if ((vi_out [i].depth > vi_out [best].depth) ||
	    ((vi_out [i].depth == vi_out [best].depth) &&
	     (vi_out [i].colormap_size > vi_out [best].colormap_size)))
	  best = i;
      visual = vi_out [best].visual;
      XFree ((char *) vi_out);
      return visual;
    }
  else
    return 0;
}

static Visual *
id_to_visual (screen, id)
     Screen *screen;
     int id;
{
  Display *dpy = DisplayOfScreen (screen);
  XVisualInfo vi_in, *vi_out;
  int out_count;
  vi_in.screen = screen_number (screen);
  vi_in.visualid = id;
  vi_out = XGetVisualInfo (dpy, (VisualScreenMask | VisualIDMask),
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

void
describe_visual (f, dpy, visual)
     FILE *f;
     Display *dpy;
     Visual *visual;
{
  Screen *screen = DefaultScreenOfDisplay (dpy);
  XVisualInfo vi_in, *vi_out;
  int out_count;
  vi_in.screen = screen_number (screen);
  vi_in.visualid = XVisualIDFromVisual (visual);
  vi_out = XGetVisualInfo (dpy, (VisualScreenMask | VisualIDMask),
			   &vi_in, &out_count);
  if (! vi_out) abort ();
  fprintf (f, "0x%02x (%s depth: %2d, cmap: %3d)\n",
	   (unsigned int) vi_out->visualid,
	   (vi_out->class == StaticGray  ? "StaticGray, " :
	    vi_out->class == StaticColor ? "StaticColor," :
	    vi_out->class == TrueColor   ? "TrueColor,  " :
	    vi_out->class == GrayScale   ? "GrayScale,  " :
	    vi_out->class == PseudoColor ? "PseudoColor," :
	    vi_out->class == DirectColor ? "DirectColor," :
					   "UNKNOWN:    "),
	   vi_out->depth, vi_out->colormap_size /*, vi_out->bits_per_rgb*/);
  XFree ((char *) vi_out);
}

static int
screen_number (screen)
	Screen *screen;
{
  Display *dpy = DisplayOfScreen (screen);
  int i;
  for (i = 0; i < ScreenCount (dpy); i++)
    if (ScreenOfDisplay (dpy, i) == screen)
      return i;
  abort ();
}

int
visual_cells (dpy, visual)
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
  c = vi_out [0].colormap_size;
  XFree ((char *) vi_out);
  return c;
}
