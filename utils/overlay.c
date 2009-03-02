/* xscreensaver, Copyright (c) 1997 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* If the server's root window contains a SERVER_OVERLAY_VISUALS property,
   then that identifies the visuals which correspond to the video hardware's
   overlay planes.  Windows created in these kinds of visuals have the
   property that one particular pixel value is transparent.

   On my Indy, there are two transparent visuals, one of which is at layer 1,
   and one of which is at layer 2.  This is apparently the ordering in which
   they are overlayed (1 being topmost.)  The other difference between them
   is that the topmost one only has 2 planes, while the next one has 8.

   Rumor has it that SGI, HP, DEC, and IBM all use the same mechanism.

   This code selects the topmost one, regardless of depth.  Maybe that's not
   the right thing.  Well, in XScreenSaver, we only need to allocate two
   colors from it (it's only used to display the stderr output, so that the
   text can overlay the graphics without being obliterated by it.)
 */


#include "utils.h"

#include <X11/Xutil.h>
#include <X11/Xproto.h>

#include "visual.h"


struct overlay_data
{
  CARD32 visual_id;
  CARD32 transparency;	/* 0: none; 1: pixel; 2: mask (?) */
  CARD32 value;		/* the transparent pixel */
  CARD32 layer;		/* -1: underlay; 0: normal; 1: popup; 2: overlay */
};

static int
get_overlay_prop (Screen *screen, struct overlay_data **data_ret)
{
  int result;
  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  struct overlay_data *data = 0;
  Display *dpy = DisplayOfScreen(screen);
  Window window = RootWindowOfScreen(screen);
  Atom XA_SERVER_OVERLAY_VISUALS =
    XInternAtom (dpy, "SERVER_OVERLAY_VISUALS", False);

  *data_ret = 0;
  result = XGetWindowProperty (dpy, window, XA_SERVER_OVERLAY_VISUALS,
			       0, (65536 / sizeof (long)), False, 
			       XA_SERVER_OVERLAY_VISUALS,
			       &actual_type, &actual_format,
			       &nitems, &bytes_after,
			       (unsigned char **) &data);
  if (result != Success ||
      actual_type != XA_SERVER_OVERLAY_VISUALS ||
      actual_format != 32 ||
      nitems < 1)
    {
      if (data) XFree(data);
      return 0;
    }
  else
    {
      *data_ret = data;
      return nitems / (sizeof(*data) / sizeof(CARD32));
    }
}


Visual *
get_overlay_visual (Screen *screen, unsigned long *transparent_pixel_ret)
{
  struct overlay_data *data = 0;
  int n_visuals = get_overlay_prop (screen, &data);
  Visual *visual = 0;
  int depth = 0;
  unsigned long pixel = 0;
  unsigned int layer = 0;
  int i;

  if (data)
    for (i = 0; i < n_visuals; i++)

      /* Only accept ones that have a transparent pixel. */
      if (data[i].transparency == 1)
	{
	  XVisualInfo vi_in, *vi_out;
	  int out_count;
	  vi_in.visualid = data[i].visual_id;
	  vi_out = XGetVisualInfo (DisplayOfScreen(screen), VisualIDMask,
				   &vi_in, &out_count);
	  if (vi_out)
	    {
	      /* Prefer the one at the topmost layer; after that, prefer
		 the one with the greatest depth (most colors.) */
	      if (layer < data[i].layer ||
		  (layer == data[i].layer &&
		   depth < vi_out[0].depth))
		{
		  visual = vi_out[0].visual;
		  depth  = vi_out[0].depth;
		  layer  = data[i].layer;
		  pixel  = data[i].value;
		}
	      XFree(vi_out);
	    }
	}

  if (data) XFree(data);
  if (visual && transparent_pixel_ret)
    *transparent_pixel_ret = pixel;
  return visual;
}
