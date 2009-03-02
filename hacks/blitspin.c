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

/* Rotate a bitmap using using bitblts.
   The bitmap must be square, and must be a power of 2 in size.
   This was translated from SmallTalk code which appeared in the
   August 1981 issue of Byte magazine.

   The input bitmap may be non-square, it is padded and centered
   with the background color.  Another way would be to subdivide
   the bitmap into square components and rotate them independently
   (and preferably in parallel), but I don't think that would be as
   interesting looking.

   It's too bad almost nothing uses blitter hardware these days,
   or this might actually win.
 */

#include "screenhack.h"
#include <stdio.h>

#ifdef HAVE_XPM
# include <X11/xpm.h>
# ifndef PIXEL_ALREADY_TYPEDEFED
#  define PIXEL_ALREADY_TYPEDEFED /* Sigh, Xmu/Drawing.h needs this... */
# endif
#endif

#include <X11/Xmu/Drawing.h>

#include "default.xbm"

static Display *dpy;
static Window window;
static unsigned int size;
static Pixmap self, temp, mask;
static GC SET, CLR, CPY, IOR, AND, XOR;
static GC gc;
static int delay, delay2;
static Pixmap bitmap;
static int depth;
static unsigned int fg, bg;

static void rotate(), init (), display ();

#define copy_all_to(from, xoff, yoff, to, gc)		\
  XCopyArea (dpy, (from), (to), (gc), 0, 0,		\
	     size-(xoff), size-(yoff), (xoff), (yoff))

#define copy_all_from(to, xoff, yoff, from, gc)		\
  XCopyArea (dpy, (from), (to), (gc), (xoff), (yoff),	\
	     size-(xoff), size-(yoff), 0, 0)

static void
rotate ()
{
  int qwad; /* fuckin' C, man... who needs namespaces? */
  XFillRectangle (dpy, mask, CLR, 0, 0, size, size);
  XFillRectangle (dpy, mask, SET, 0, 0, size>>1, size>>1);
  for (qwad = size>>1; qwad > 0; qwad>>=1)
    {
      if (delay) usleep (delay);
      copy_all_to   (mask,       0,       0, temp, CPY);  /* 1 */
      copy_all_to   (mask,       0,    qwad, temp, IOR);  /* 2 */
      copy_all_to   (self,       0,       0, temp, AND);  /* 3 */
      copy_all_to   (temp,       0,       0, self, XOR);  /* 4 */
      copy_all_from (temp,    qwad,       0, self, XOR);  /* 5 */
      copy_all_from (self,    qwad,       0, self, IOR);  /* 6 */
      copy_all_to   (temp,    qwad,       0, self, XOR);  /* 7 */
      copy_all_to   (self,       0,       0, temp, CPY);  /* 8 */
      copy_all_from (temp,    qwad,    qwad, self, XOR);  /* 9 */
      copy_all_to   (mask,       0,       0, temp, AND);  /* A */
      copy_all_to   (temp,       0,       0, self, XOR);  /* B */
      copy_all_to   (temp,    qwad,    qwad, self, XOR);  /* C */
      copy_all_from (mask, qwad>>1, qwad>>1, mask, AND);  /* D */
      copy_all_to   (mask,    qwad,       0, mask, IOR);  /* E */
      copy_all_to   (mask,       0,    qwad, mask, IOR);  /* F */
      display (self);
    }
}

static void
read_bitmap (bitmap_name, widthP, heightP)
     char *bitmap_name;
     int *widthP, *heightP;
{
#ifdef HAVE_XPM
  XpmAttributes xpmattrs;
  int result;
  xpmattrs.valuemask = 0;
  bitmap = 0;
  result = XpmReadFileToPixmap (dpy, window, bitmap_name, &bitmap, 0,
				&xpmattrs);
  switch (result)
    {
    case XpmColorError:
      fprintf (stderr, "%s: warning: xpm color substitution performed\n",
	       progname);
      /* fall through */
    case XpmSuccess:
      *widthP = xpmattrs.width;
      *heightP = xpmattrs.height;
      break;
    case XpmFileInvalid:
    case XpmOpenFailed:
      bitmap = 0;
      break;
    case XpmColorFailed:
      fprintf (stderr, "%s: xpm: color allocation failed\n", progname);
      exit (-1);
    case XpmNoMemory:
      fprintf (stderr, "%s: xpm: out of memory\n", progname);
      exit (-1);
    default:
      fprintf (stderr, "%s: xpm: unknown error code %d\n", progname, result);
      exit (-1);
    }
  if (! bitmap)
#endif
    {
      int xh, yh;
      Pixmap b2;
      bitmap = XmuLocateBitmapFile (DefaultScreenOfDisplay (dpy), bitmap_name,
				    0, 0, widthP, heightP, &xh, &yh);
      if (! bitmap)
	{
	  fprintf (stderr, "%s: couldn't find bitmap %s\n", progname,
		   bitmap_name);
	  exit (1);
	}
      b2 = XmuCreatePixmapFromBitmap (dpy, window, bitmap, *widthP, *heightP,
				      depth, fg, bg);
      XFreePixmap (dpy, bitmap);
      bitmap = b2;
    }
}

static void
init ()
{
  XWindowAttributes xgwa;
  Colormap cmap;
  XGCValues gcv;
  int width, height;
  unsigned int real_size;
  char *bitmap_name;
  int i;

  XGetWindowAttributes (dpy, window, &xgwa);
  cmap = xgwa.colormap;
  depth = xgwa.depth;

  fg = get_pixel_resource ("foreground", "Foreground", dpy, cmap);
  bg = get_pixel_resource ("background", "Background", dpy, cmap);
  delay = get_integer_resource ("delay", "Integer");
  delay2 = get_integer_resource ("delay2", "Integer");
  if (delay < 0) delay = 0;
  if (delay2 < 0) delay2 = 0;
  bitmap_name = get_string_resource ("bitmap", "Bitmap");
  if (! bitmap_name || !*bitmap_name)
    bitmap_name = "(default)";

  if (!strcmp (bitmap_name, "(default)"))
    {
      width = logo_width;
      height = logo_height;
      bitmap = XCreatePixmapFromBitmapData (dpy, window, (char *) logo_bits,
					    width, height, fg, bg, depth);
    }
  else
    {
      read_bitmap (bitmap_name, &width, &height);
    }

  real_size = (width > height) ? width : height;

  size = real_size;
  /* semi-sleazy way of doing (setq size (expt 2 (ceiling (log size 2)))). */
  for (i = 31; i > 0; i--)
    if (size & (1<<i)) break;
  if (size & (~(1<<i)))
    size = (size>>i)<<(i+1);
  self = XCreatePixmap (dpy, window, size, size, depth);
  temp = XCreatePixmap (dpy, window, size, size, depth);
  mask = XCreatePixmap (dpy, window, size, size, depth);
  gcv.foreground = (depth == 1 ? 1 : (~0));
  gcv.function=GXset;  SET = XCreateGC(dpy,self,GCFunction|GCForeground,&gcv);
  gcv.function=GXclear;CLR = XCreateGC(dpy,self,GCFunction|GCForeground,&gcv);
  gcv.function=GXcopy; CPY = XCreateGC(dpy,self,GCFunction|GCForeground,&gcv);
  gcv.function=GXor;   IOR = XCreateGC(dpy,self,GCFunction|GCForeground,&gcv);
  gcv.function=GXand;  AND = XCreateGC(dpy,self,GCFunction|GCForeground,&gcv);
  gcv.function=GXxor;  XOR = XCreateGC(dpy,self,GCFunction|GCForeground,&gcv);

  gcv.foreground = gcv.background = bg;
  gc = XCreateGC (dpy, window, GCForeground|GCBackground, &gcv);
  /* Clear self to the background color (not to 0, which CLR does.) */
  XFillRectangle (dpy, self, gc, 0, 0, size, size);
  XSetForeground (dpy, gc, fg);

  XCopyArea (dpy, bitmap, self, CPY, 0, 0, width, height,
	     (size - width)>>1, (size - height)>>1);
}

static void
display (pixmap)
     Pixmap pixmap;
{
  XWindowAttributes xgwa;
  static int last_w = 0, last_h = 0;
  XGetWindowAttributes (dpy, window, &xgwa);
  if (xgwa.width != last_w || xgwa.height != last_h)
    {
      XClearWindow (dpy, window);
      last_w = xgwa.width;
      last_h = xgwa.height;
    }
#ifdef HAVE_XPM
  if (depth != 1)
    XCopyArea (dpy, pixmap, window, gc, 0, 0, size, size,
	       (xgwa.width-size)>>1, (xgwa.height-size)>>1);
  else
#endif
    XCopyPlane (dpy, pixmap, window, gc, 0, 0, size, size,
		(xgwa.width-size)>>1, (xgwa.height-size)>>1, 1);
/*
  XDrawRectangle (dpy, window, gc,
		  ((xgwa.width-size)>>1)-1, ((xgwa.height-size)>>1)-1,
		  size+2, size+2);
*/
  XSync (dpy, True);
}


char *progclass = "BlitSpin";

char *defaults [] = {
  "*background:	black",
  "*foreground:	white",
  "*delay:	500000",
  "*delay2:	500000",
  "*bitmap:	(default)",
  0
};

XrmOptionDescRec options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-delay2",		".delay2",	XrmoptionSepArg, 0 },
  { "-bitmap",		".bitmap",	XrmoptionSepArg, 0 }
};
int options_size = (sizeof (options) / sizeof (options[0]));

void
screenhack (d, w)
     Display *d;
     Window w;
{
  dpy = d;
  window = w;
  init ();
  while (1)
    {
      rotate ();
      if (delay2) usleep (delay2);
    }
}
