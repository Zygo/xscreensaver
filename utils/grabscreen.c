/* xscreensaver, Copyright (c) 1992, 1993, 1994, 1997, 1998, 2003
 *  Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This file contains code for grabbing an image of the screen to hack its
   bits.  This is a little tricky, since doing this involves the need to tell
   the difference between drawing on the actual root window, and on the fake
   root window used by the screensaver, since at this level the illusion 
   breaks down...
 */

#include "utils.h"
#include "yarandom.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>

#ifdef HAVE_XMU
# ifndef VMS
#  include <X11/Xmu/WinUtil.h>
# else  /* VMS */
#  include <Xmu/WinUtil.h>
# endif /* VMS */
#endif

#include "usleep.h"
#include "colors.h"
#include "grabscreen.h"
#include "visual.h"
#include "resources.h"

#include "vroot.h"
#undef RootWindowOfScreen
#undef RootWindow
#undef DefaultRootWindow


#ifdef HAVE_READ_DISPLAY_EXTENSION
# include <X11/extensions/readdisplay.h>
  static Bool read_display (Screen *, Window, Pixmap, Bool);
#endif /* HAVE_READ_DISPLAY_EXTENSION */


static void copy_default_colormap_contents (Screen *, Colormap, Visual *);

#ifdef HAVE_READ_DISPLAY_EXTENSION
static void allocate_cubic_colormap (Screen *, Window, Visual *);
void remap_image (Screen *, Window, Colormap, XImage *);
#endif


static Bool
MapNotify_event_p (Display *dpy, XEvent *event, XPointer window)
{
  return (event->xany.type == MapNotify &&
	  event->xvisibility.window == (Window) window);
}

extern char *progname;
Bool grab_verbose_p = False;

void
grabscreen_verbose(void)
{
  grab_verbose_p = True;
}


static void
raise_window(Display *dpy, Window window, Bool dont_wait)
{
  if (grab_verbose_p)
    fprintf(stderr, "%s: raising window 0x%08lX (%s)\n",
            progname, (unsigned long) window,
            (dont_wait ? "not waiting" : "waiting"));

  if (! dont_wait)
    {
      XWindowAttributes xgwa;
      XSizeHints hints;
      long supplied = 0;
      memset(&hints, 0, sizeof(hints));
      XGetWMNormalHints(dpy, window, &hints, &supplied);
      XGetWindowAttributes (dpy, window, &xgwa);
      hints.x = xgwa.x;
      hints.y = xgwa.y;
      hints.width = xgwa.width;
      hints.height = xgwa.height;
      hints.flags |= (PPosition|USPosition|PSize|USSize);
      XSetWMNormalHints(dpy, window, &hints);

      XSelectInput (dpy, window, (xgwa.your_event_mask | StructureNotifyMask));
    }

  XMapRaised(dpy, window);

  if (! dont_wait)
    {
      XEvent event;
      XIfEvent (dpy, &event, MapNotify_event_p, (XPointer) window);
      XSync (dpy, True);
    }
}


static Bool
xscreensaver_window_p (Display *dpy, Window window)
{
  Atom type;
  int format;
  unsigned long nitems, bytesafter;
  char *version;
  if (XGetWindowProperty (dpy, window,
			  XInternAtom (dpy, "_SCREENSAVER_VERSION", False),
			  0, 1, False, XA_STRING,
			  &type, &format, &nitems, &bytesafter,
			  (unsigned char **) &version)
      == Success
      && type != None)
    return True;
  return False;
}



/* Whether the given window is:
   - the real root window;
   - a direct child of the root window;
   - a direct child of the window manager's decorations.
 */
Bool
top_level_window_p (Screen *screen, Window window)
{
  Display *dpy = DisplayOfScreen (screen);
  Window root, parent, *kids;
  unsigned int nkids;

  if (!XQueryTree (dpy, window, &root, &parent, &kids, &nkids))
    return False;

  if (window == root)
    return True;

  /* If our direct parent is the real root window, then yes. */
  if (parent == root)
    return True;
  else
    {
      Atom type = None;
      int format;
      unsigned long nitems, bytesafter;
      unsigned char *data;

      /* If our direct parent has the WM_STATE property, then it is a
         window manager decoration -- yes.
      */
      if (XGetWindowProperty (dpy, window,
                              XInternAtom (dpy, "WM_STATE", True),
                              0, 0, False, AnyPropertyType,
                              &type, &format, &nitems, &bytesafter,
                              (unsigned char **) &data)
          == Success
          && type != None)
        return True;
    }

  /* Else, no.  We're deep in a tree somewhere.
   */
  return False;
}


static Bool error_handler_hit_p = False;
static XErrorHandler old_ehandler = 0;
static int
BadWindow_ehandler (Display *dpy, XErrorEvent *error)
{
  error_handler_hit_p = True;
  if (error->error_code == BadWindow || error->error_code == BadDrawable)
    return 0;
  else if (!old_ehandler)
    {
      abort();
      return 0;
    }
  else
    return (*old_ehandler) (dpy, error);
}


/* XCopyArea seems not to work right on SGI O2s if you draw in SubwindowMode
   on a window whose depth is not the maximal depth of the screen?  Or
   something.  Anyway, things don't work unless we: use SubwindowMode for
   the real root window (or a legitimate virtual root window); but do not
   use SubwindowMode for the xscreensaver window.  I make no attempt to
   explain.
 */
Bool
use_subwindow_mode_p(Screen *screen, Window window)
{
  if (window != VirtualRootWindowOfScreen(screen))
    return False;
  else if (xscreensaver_window_p(DisplayOfScreen(screen), window))
    return False;
  else
    return True;
}


/* Install the colormaps of all visible windows, deepest first.
   This should leave the colormaps of the topmost windows installed
   (if only N colormaps can be installed at a time, then only the
   topmost N windows will be shown in the right colors.)
 */
static void
install_screen_colormaps (Screen *screen)
{
  int i;
  Display *dpy = DisplayOfScreen (screen);
  Window real_root;
  Window parent, *kids = 0;
  unsigned int nkids = 0;

  XSync (dpy, False);
  old_ehandler = XSetErrorHandler (BadWindow_ehandler);
  error_handler_hit_p = False;

  real_root = XRootWindowOfScreen (screen);  /* not vroot */
  if (XQueryTree (dpy, real_root, &real_root, &parent, &kids, &nkids))
    for (i = 0; i < nkids; i++)
      {
	XWindowAttributes xgwa;
	Window client;
#ifdef HAVE_XMU
	/* #### need to put XmuClientWindow() in xmu.c, sigh... */
	if (! (client = XmuClientWindow (dpy, kids[i])))
#endif
	  client = kids[i];
	xgwa.colormap = 0;
	XGetWindowAttributes (dpy, client, &xgwa);
	if (xgwa.colormap && xgwa.map_state == IsViewable)
	  XInstallColormap (dpy, xgwa.colormap);
      }
  XInstallColormap (dpy, DefaultColormapOfScreen (screen));
  XSync (dpy, False);
  XSetErrorHandler (old_ehandler);
  XSync (dpy, False);

  if (kids)
    XFree ((char *) kids);
}


void
grab_screen_image (Screen *screen, Window window)
{
  Display *dpy = DisplayOfScreen (screen);
  XWindowAttributes xgwa;
  Window real_root;
  Bool root_p;
  Bool saver_p;
  Bool grab_mouse_p = False;
  int unmap_time = 0;

  real_root = XRootWindowOfScreen (screen);  /* not vroot */
  root_p = (window == real_root);
  saver_p = xscreensaver_window_p (dpy, window);

  XGetWindowAttributes (dpy, window, &xgwa);
  screen = xgwa.screen;

  if (saver_p)
    /* I think this is redundant, but just to be safe... */
    root_p = False;

  if (saver_p)
    /* The only time grabbing the mouse is important is if this program
       is being run while the saver is locking the screen. */
    grab_mouse_p = True;

  if (!root_p)
    {
      double unmap = 0;
      if (saver_p)
	{
	  unmap = get_float_resource("grabRootDelay", "Seconds");
	  if (unmap <= 0.00001 || unmap > 20) unmap = 2.5;
	}
      else
	{
	  unmap = get_float_resource("grabWindowDelay", "Seconds");
	  if (unmap <= 0.00001 || unmap > 20) unmap = 0.66;
	}
      unmap_time = unmap * 100000;
    }

  if (grab_verbose_p)
    {
      fprintf(stderr,
              "\n%s: window 0x%08lX root: %d saver: %d grab: %d wait: %.1f\n",
              progname, (unsigned long) window,
              root_p, saver_p, grab_mouse_p, ((double)unmap_time)/1000000.0);

      fprintf(stderr, "%s: ", progname);
      describe_visual(stderr, screen, xgwa.visual, False);
      fprintf (stderr, "\n");
    }


  if (!root_p && !top_level_window_p (screen, window))
    {
      if (grab_verbose_p)
        fprintf (stderr, "%s: not a top-level window: 0x%08lX: not grabbing\n",
                 progname, (unsigned long) window);
      return;
    }


  if (!root_p)
    XSetWindowBackgroundPixmap (dpy, window, None);

  if (grab_mouse_p)
    {
      /* prevent random viewer of the screen saver (locker) from messing
	 with windows.   We don't check whether it succeeded, because what
	 are our options, really... */
      XGrabPointer (dpy, real_root, True, ButtonPressMask|ButtonReleaseMask,
		    GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
      XGrabKeyboard (dpy, real_root, True, GrabModeSync, GrabModeAsync,
		     CurrentTime);
    }

  if (unmap_time > 0)
    {
      XUnmapWindow (dpy, window);
      install_screen_colormaps (screen);
      XSync (dpy, True);
      usleep(unmap_time); /* wait for everyone to swap in and handle exposes */
    }

  if (!root_p)
    {
#ifdef HAVE_READ_DISPLAY_EXTENSION
      if (! read_display(screen, window, 0, saver_p))
#endif /* HAVE_READ_DISPLAY_EXTENSION */
	{
#ifdef HAVE_READ_DISPLAY_EXTENSION
          if (grab_verbose_p)
            fprintf(stderr, "%s: read_display() failed\n", progname);
#endif /* HAVE_READ_DISPLAY_EXTENSION */

	  copy_default_colormap_contents (screen, xgwa.colormap, xgwa.visual);
	  raise_window(dpy, window, saver_p);

          /* Generally it's bad news to call XInstallColormap() explicitly,
             but this file does a lot of sleazy stuff already...  This is to
             make sure that the window's colormap is installed, even in the
             case where the window is OverrideRedirect. */
          if (xgwa.colormap) XInstallColormap (dpy, xgwa.colormap);
          XSync (dpy, False);
	}
    }
  else  /* root_p */
    {
      Pixmap pixmap;
      pixmap = XCreatePixmap(dpy, window, xgwa.width, xgwa.height, xgwa.depth);

#ifdef HAVE_READ_DISPLAY_EXTENSION
      if (! read_display(screen, window, pixmap, True))
#endif
	{
	  Window real_root = XRootWindowOfScreen (screen); /* not vroot */
	  XGCValues gcv;
	  GC gc;

#ifdef HAVE_READ_DISPLAY_EXTENSION
          if  (grab_verbose_p)
            fprintf(stderr, "%s: read_display() failed\n", progname);
#endif /* HAVE_READ_DISPLAY_EXTENSION */

	  copy_default_colormap_contents (screen, xgwa.colormap, xgwa.visual);

	  gcv.function = GXcopy;
	  gcv.subwindow_mode = IncludeInferiors;
	  gc = XCreateGC (dpy, window, GCFunction | GCSubwindowMode, &gcv);
	  XCopyArea (dpy, real_root, pixmap, gc,
		     xgwa.x, xgwa.y, xgwa.width, xgwa.height, 0, 0);
	  XFreeGC (dpy, gc);
	}
      XSetWindowBackgroundPixmap (dpy, window, pixmap);
      XFreePixmap (dpy, pixmap);
    }

  if (grab_verbose_p)
    fprintf (stderr, "%s: grabbed %d bit screen image to %swindow.\n",
             progname, xgwa.depth,
             (root_p ? "real root " : ""));

  if (grab_mouse_p)
    {
      XUngrabPointer (dpy, CurrentTime);
      XUngrabKeyboard (dpy, CurrentTime);
    }

  XSync (dpy, True);
}


/* When we are grabbing and manipulating a screen image, it's important that
   we use the same colormap it originally had.  So, if the screensaver was
   started with -install, we need to copy the contents of the default colormap
   into the screensaver's colormap.
 */
static void
copy_default_colormap_contents (Screen *screen,
				Colormap to_cmap,
				Visual *to_visual)
{
  Display *dpy = DisplayOfScreen (screen);
  Visual *from_visual = DefaultVisualOfScreen (screen);
  Colormap from_cmap = XDefaultColormapOfScreen (screen);

  XColor *old_colors, *new_colors;
  unsigned long *pixels;
  XVisualInfo vi_in, *vi_out;
  int out_count;
  int from_cells, to_cells, max_cells, got_cells;
  int i;

  if (from_cmap == to_cmap)
    return;

  vi_in.screen = XScreenNumberOfScreen (screen);
  vi_in.visualid = XVisualIDFromVisual (from_visual);
  vi_out = XGetVisualInfo (dpy, VisualScreenMask|VisualIDMask,
			   &vi_in, &out_count);
  if (! vi_out) abort ();
  from_cells = vi_out [0].colormap_size;
  XFree ((char *) vi_out);

  vi_in.screen = XScreenNumberOfScreen (screen);
  vi_in.visualid = XVisualIDFromVisual (to_visual);
  vi_out = XGetVisualInfo (dpy, VisualScreenMask|VisualIDMask,
			   &vi_in, &out_count);
  if (! vi_out) abort ();
  to_cells = vi_out [0].colormap_size;
  XFree ((char *) vi_out);

  max_cells = (from_cells > to_cells ? to_cells : from_cells);

  old_colors = (XColor *) calloc (sizeof (XColor), max_cells);
  new_colors = (XColor *) calloc (sizeof (XColor), max_cells);
  pixels = (unsigned long *) calloc (sizeof (unsigned long), max_cells);
  for (i = 0; i < max_cells; i++)
    old_colors[i].pixel = i;
  XQueryColors (dpy, from_cmap, old_colors, max_cells);

  got_cells = max_cells;
  allocate_writable_colors (dpy, to_cmap, pixels, &got_cells);

  if (grab_verbose_p && got_cells != max_cells)
    fprintf(stderr, "%s: got only %d of %d cells\n", progname,
	    got_cells, max_cells);

  if (got_cells <= 0)					 /* we're screwed */
    ;
  else if (got_cells == max_cells &&			 /* we're golden */
	   from_cells == to_cells)
    XStoreColors (dpy, to_cmap, old_colors, got_cells);
  else							 /* try to cope... */
    {
      for (i = 0; i < got_cells; i++)
	{
	  XColor *c = old_colors + i;
	  int j;
	  for (j = 0; j < got_cells; j++)
	    if (pixels[j] == c->pixel)
	      {
		/* only store this color value if this is one of the pixels
		   we were able to allocate. */
		XStoreColors (dpy, to_cmap, c, 1);
		break;
	      }
	}
    }


  if (grab_verbose_p)
    fprintf(stderr, "%s: installing copy of default colormap\n", progname);

  free (old_colors);
  free (new_colors);
  free (pixels);
}



/* The SGI ReadDisplay extension.
   This extension lets you get back a 24-bit image of the screen, taking into
   account the colors with which all windows are *currently* displayed, even
   if those windows have different visuals.  Without this extension, presence
   of windows with different visuals or colormaps will result in technicolor
   when one tries to grab the screen image.
 */

#ifdef HAVE_READ_DISPLAY_EXTENSION

static Bool
read_display (Screen *screen, Window window, Pixmap into_pixmap,
	      Bool dont_wait)
{
  Display *dpy = DisplayOfScreen (screen);
  XWindowAttributes xgwa;
  int rd_event_base = 0;
  int rd_error_base = 0;
  unsigned long hints = 0;
  XImage *image = 0;
  XGCValues gcv;
  int class;
  GC gc;
  Bool remap_p = False;

  /* Check to see if the server supports the extension, and bug out if not.
   */
  if (! XReadDisplayQueryExtension (dpy, &rd_event_base, &rd_error_base))
    {
      if (grab_verbose_p)
        fprintf(stderr, "%s: no XReadDisplay extension\n", progname);
      return False;
    }

  /* If this isn't a visual we know how to handle, bug out.  We handle:
      = TrueColor in depths 8, 12, 15, 16, and 32;
      = PseudoColor and DirectColor in depths 8 and 12.
   */
  XGetWindowAttributes(dpy, window, &xgwa);
  class = visual_class (screen, xgwa.visual);
  if (class == TrueColor)
    {
      if (xgwa.depth != 8  && xgwa.depth != 12 && xgwa.depth != 15 &&
          xgwa.depth != 16 && xgwa.depth != 24 && xgwa.depth != 32)
        {
          if (grab_verbose_p)
            fprintf(stderr, "%s: TrueColor depth %d unsupported\n",
                    progname, xgwa.depth);
          return False;
        }
    }
  else if (class == PseudoColor || class == DirectColor)
    {
      if (xgwa.depth != 8 && xgwa.depth != 12)
        {
          if (grab_verbose_p)
            fprintf(stderr, "%s: Pseudo/DirectColor depth %d unsupported\n",
                    progname, xgwa.depth);
          return False;
        }
      else
	/* Allocate a TrueColor-like spread of colors for the image. */
	remap_p = True;
    }


  /* Try and read the screen.
   */
  hints = (XRD_TRANSPARENT | XRD_READ_POINTER);
  image = XReadDisplay (dpy, window, xgwa.x, xgwa.y, xgwa.width, xgwa.height,
			hints, &hints);
  if (!image)
    {
      if (grab_verbose_p)
        fprintf(stderr, "%s: XReadDisplay() failed\n", progname);
      return False;
    }
  if (!image->data)
    {
      if (grab_verbose_p)
        fprintf(stderr, "%s: XReadDisplay() returned no data\n", progname);
      XDestroyImage(image);
      return False;
    }

  /* XReadDisplay tends to LIE about the depth of the image it read.
     It is returning an XImage which has `depth' and `bits_per_pixel'
     confused!

     That is, on a 24-bit display, where all visuals claim depth 24, and
     where XGetImage would return an XImage with depth 24, and where
     XPutImage will get a BadMatch with images that are not depth 24,
     XReadDisplay is returning images with depth 32!  Fuckwits!

     So if the visual is of depth 24, but the image came back as depth 32,
     hack it to be 24 lest we get a BadMatch from XPutImage.

     I wonder what happens on an 8-bit SGI...  Probably it still returns
     an image claiming depth 32?  Certainly it can't be 8.  So, let's just
     smash it to 32...
   */
  if (image->depth == 32 /* && xgwa.depth == 24 */ )
    image->depth = 24;

  /* If the visual of the window/pixmap into which we're going to draw is
     less deep than the screen itself, then we need to convert the grabbed bits
     to match the depth by clipping off the less significant bit-planes of each
     color component.
   */
  if (image->depth > xgwa.depth)
    {
      int x, y;
      /* We use the same image->data in both images -- that's ok, because
	 since we're reading from B and writing to A, and B uses more bytes
	 per pixel than A, the write pointer won't overrun the read pointer.
       */
      XImage *image2 = XCreateImage (dpy, xgwa.visual, xgwa.depth,
				     ZPixmap, 0, image->data,
				     xgwa.width, xgwa.height,
				     8, 0);
      if (!image2)
        {
          if (grab_verbose_p)
            fprintf(stderr, "%s: out of memory?\n", progname);
          return False;
        }

      if (grab_verbose_p)
        fprintf(stderr, "%s: converting from depth %d to depth %d\n",
                progname, image->depth, xgwa.depth);

      for (y = 0; y < image->height; y++)
	for (x = 0; x < image->width; x++)
	  {
	    /* #### really these shift values should be determined from the
	       mask values -- but that's a pain in the ass, and anyway,
	       this is an SGI-specific extension so hardcoding assumptions
	       about the SGI server's behavior isn't *too* heinous... */
	    unsigned long pixel = XGetPixel(image, x, y);
	    unsigned int r = (pixel & image->red_mask);
	    unsigned int g = (pixel & image->green_mask) >> 8;
	    unsigned int b = (pixel & image->blue_mask) >> 16;

	    if (xgwa.depth == 8)
	      pixel = ((r >> 5) | ((g >> 5) << 3) | ((b >> 6) << 6));
	    else if (xgwa.depth == 12)
	      pixel = ((r >> 4) | ((g >> 4) << 4) | ((b >> 4) << 8));
	    else if (xgwa.depth == 16 || xgwa.depth == 15)
              /* Gah! I don't understand why these are in the other order. */
	      pixel = (((r >> 3) << 10) | ((g >> 3) << 5) | ((b >> 3)));
	    else
	      abort();

	    XPutPixel(image2, x, y, pixel);
	  }
      image->data = 0;
      XDestroyImage(image);
      image = image2;
    }

  if (remap_p)
    {
      allocate_cubic_colormap (screen, window, xgwa.visual);
      remap_image (screen, window, xgwa.colormap, image);
    }

  /* Now actually put the bits into the window or pixmap -- note the design
     bogosity of this extension, where we've been forced to take 24 bit data
     from the server to the client, and then push it back from the client to
     the server, *without alteration*.  We should have just been able to tell
     the server, "put a screen image in this drawable", instead of having to
     go through the intermediate step of converting it to an Image.  Geez.
     (Assuming that the window is of screen depth; we happen to handle less
     deep windows, but that's beside the point.)
   */
  gcv.function = GXcopy;
  gc = XCreateGC (dpy, window, GCFunction, &gcv);

  if (into_pixmap)
    {
      gcv.function = GXcopy;
      gc = XCreateGC (dpy, into_pixmap, GCFunction, &gcv);
      XPutImage (dpy, into_pixmap, gc, image, 0, 0, 0, 0,
		 xgwa.width, xgwa.height);
    }
  else
    {
      gcv.function = GXcopy;
      gc = XCreateGC (dpy, window, GCFunction, &gcv);

      /* Ok, now we'll be needing that window on the screen... */
      raise_window(dpy, window, dont_wait);

      /* Plop down the bits... */
      XPutImage (dpy, window, gc, image, 0, 0, 0, 0, xgwa.width, xgwa.height);
    }
  XFreeGC (dpy, gc);

  if (image->data)
    {
      free(image->data);
      image->data = 0;
    }
  XDestroyImage(image);

  return True;
}
#endif /* HAVE_READ_DISPLAY_EXTENSION */


#ifdef HAVE_READ_DISPLAY_EXTENSION

/* Makes and installs a colormap that makes a PseudoColor or DirectColor
   visual behave like a TrueColor visual of the same depth.
 */
static void
allocate_cubic_colormap (Screen *screen, Window window, Visual *visual)
{
  Display *dpy = DisplayOfScreen (screen);
  XWindowAttributes xgwa;
  Colormap cmap;
  int nr, ng, nb, cells;
  int r, g, b;
  int depth;
  XColor colors[4097];
  int i;

  XGetWindowAttributes (dpy, window, &xgwa);
  cmap = xgwa.colormap;
  depth = visual_depth(screen, visual);

  switch (depth)
    {
    case 8:  nr = 3; ng = 3; nb = 2; cells = 256;  break;
    case 12: nr = 4; ng = 4; nb = 4; cells = 4096; break;
    default: abort(); break;
    }

  memset(colors, 0, sizeof(colors));
  for (r = 0; r < (1 << nr); r++)
    for (g = 0; g < (1 << ng); g++)
      for (b = 0; b < (1 << nb); b++)
	{
	  i = (r | (g << nr) | (b << (nr + ng)));
	  colors[i].pixel = i;
          colors[i].flags = DoRed|DoGreen|DoBlue;
	  if (depth == 8)
	    {
	      colors[i].red   = ((r << 13) | (r << 10) | (r << 7) |
				 (r <<  4) | (r <<  1));
	      colors[i].green = ((g << 13) | (g << 10) | (g << 7) |
				 (g <<  4) | (g <<  1));
	      colors[i].blue  = ((b << 14) | (b << 12) | (b << 10) |
				 (b <<  8) | (b <<  6) | (b <<  4) |
				 (b <<  2) | b);
	    }
	  else
	    {
	      colors[i].red   = (r << 12) | (r << 8) | (r << 4) | r;
	      colors[i].green = (g << 12) | (g << 8) | (g << 4) | g;
	      colors[i].blue  = (b << 12) | (b << 8) | (b << 4) | b;
	    }
	}

  {
    int j;
    int allocated = 0;
    int interleave = cells / 8;  /* skip around, rather than allocating in
                                    order, so that we get better coverage if
                                    we can't allocated all of them. */
    for (j = 0; j < interleave; j++)
      for (i = 0; i < cells; i += interleave)
        if (XAllocColor (dpy, cmap, &colors[i + j]))
          allocated++;

    if (grab_verbose_p)
      fprintf (stderr, "%s: allocated %d of %d colors for cubic map\n",
               progname, allocated, cells);
  }
}

static unsigned long
find_closest_pixel (XColor *colors, int ncolors,
                    unsigned long r, unsigned long g, unsigned long b)
{
  unsigned long distance = ~0;
  int i, found = 0;

  if (ncolors == 0)
    abort();
  for (i = 0; i < ncolors; i++)
    {
      unsigned long d;
      int rd, gd, bd;

      rd = r - colors[i].red;
      gd = g - colors[i].green;
      bd = b - colors[i].blue;
      if (rd < 0) rd = -rd;
      if (gd < 0) gd = -gd;
      if (bd < 0) bd = -bd;
      d = (rd << 1) + (gd << 2) + bd;
      
      if (d < distance)
	{
	  distance = d;
	  found = i;
          if (distance == 0)
              break;
	}
    }

  return found;
}


void
remap_image (Screen *screen, Window window, Colormap cmap, XImage *image)
{
  Display *dpy = DisplayOfScreen (screen);
  unsigned long map[4097];
  int x, y, i;
  int cells;
  XColor colors[4097];

  if (image->depth == 8)
    cells = 256;
  else if (image->depth == 12)
    cells = 4096;
  else
    abort();

  memset(map,    -1, sizeof(*map));
  memset(colors, -1, sizeof(*colors));

  for (i = 0; i < cells; i++)
    colors[i].pixel = i;
  XQueryColors (dpy, cmap, colors, cells);

  if (grab_verbose_p)
    fprintf(stderr, "%s: building table for %d bit image\n",
            progname, image->depth);

  for (i = 0; i < cells; i++)
    {
      unsigned short r, g, b;

      if (cells == 256)
        {
          /* "RRR GGG BB" In an 8 bit map.  Convert that to
             "RRR RRR RR" "GGG GGG GG" "BB BB BB BB" to give
             an even spread. */
          r = (i & 0x07);
          g = (i & 0x38) >> 3;
          b = (i & 0xC0) >> 6;

          r = ((r << 13) | (r << 10) | (r << 7) | (r <<  4) | (r <<  1));
          g = ((g << 13) | (g << 10) | (g << 7) | (g <<  4) | (g <<  1));
          b = ((b << 14) | (b << 12) | (b << 10) | (b <<  8) |
               (b <<  6) | (b <<  4) | (b <<  2) | b);
        }
      else
        {
          /* "RRRR GGGG BBBB" In a 12 bit map.  Convert that to
             "RRRR RRRR" "GGGG GGGG" "BBBB BBBB" to give an even
             spread. */
          r = (i & 0x00F);
          g = (i & 0x0F0) >> 4;
          b = (i & 0xF00) >> 8;

          r = (r << 12) | (r << 8) | (r << 4) | r;
          g = (g << 12) | (g << 8) | (g << 4) | g;
          b = (b << 12) | (b << 8) | (b << 4) | b;
        }

      map[i] = find_closest_pixel (colors, cells, r, g, b);
    }

  if (grab_verbose_p)
    fprintf(stderr, "%s: remapping colors in %d bit image\n",
            progname, image->depth);

  for (y = 0; y < image->height; y++)
    for (x = 0; x < image->width; x++)
      {
        unsigned long pixel = XGetPixel(image, x, y);
        if (pixel >= cells) abort();
        XPutPixel(image, x, y, map[pixel]);
      }
}


#endif /* HAVE_READ_DISPLAY_EXTENSION */
