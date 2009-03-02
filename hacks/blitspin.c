/* xscreensaver, Copyright (c) 1992-1997, 2003 Jamie Zawinski <jwz@jwz.org>
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
#include "xpm-pixmap.h"
#include <stdio.h>

#include "images/som.xbm"

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

static void display (Pixmap);

#define copy_all_to(from, xoff, yoff, to, gc)		\
  XCopyArea (dpy, (from), (to), (gc), 0, 0,		\
	     size-(xoff), size-(yoff), (xoff), (yoff))

#define copy_all_from(to, xoff, yoff, from, gc)		\
  XCopyArea (dpy, (from), (to), (gc), (xoff), (yoff),	\
	     size-(xoff), size-(yoff), 0, 0)

static void
rotate (void)
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

static Pixmap
read_screen (Display *dpy, Window window, int *widthP, int *heightP)
{
  Pixmap p;
  XWindowAttributes xgwa;
  XGCValues gcv;
  GC gc;
  XGetWindowAttributes (dpy, window, &xgwa);
  *widthP = xgwa.width;
  *heightP = xgwa.height;

  p = XCreatePixmap(dpy, window, *widthP, *heightP, xgwa.depth);
  gcv.function = GXcopy;
  gc = XCreateGC (dpy, window, GCFunction, &gcv);

  load_random_image (xgwa.screen, window, p);

  /* Reset the window's background color... */
  XSetWindowBackground (dpy, window,
			get_pixel_resource ("background", "Background",
					    dpy, xgwa.colormap));
  XCopyArea (dpy, p, window, gc, 0, 0, *widthP, *heightP, 0, 0);
  XFreeGC (dpy, gc);

  return p;
}


static int 
to_pow2(int n, Bool up)
{
  /* sizeof(Dimension) == 2. */
  int powers_of_2[] = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024,
			2048, 4096, 8192, 16384, 32768, 65536 };
  int i = 0;
  if (n > 65536) size = 65536;
  while (n >= powers_of_2[i]) i++;
  if (n == powers_of_2[i-1])
    return n;
  else
    return powers_of_2[up ? i : i-1];
}

static void
init (void)
{
  XWindowAttributes xgwa;
  Colormap cmap;
  XGCValues gcv;
  int width, height;
  char *bitmap_name;
  Bool scale_up;

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
      width = som_width;
      height = som_height;
      bitmap = XCreatePixmapFromBitmapData (dpy, window, (char *) som_bits,
					    width, height, fg, bg, depth);
      scale_up = True; /* definitely. */
    }
  else if (!strcmp (bitmap_name, "(screen)"))
    {
      bitmap = read_screen (dpy, window, &width, &height);
      scale_up = True; /* maybe? */
    }
  else
    {
      bitmap = xpm_file_to_pixmap (dpy, window, bitmap_name,
                                   &width, &height, 0);
      scale_up = True; /* probably? */
    }

  size = (width < height) ? height : width;	/* make it square */
  size = to_pow2(size, scale_up);		/* round up to power of 2 */
  {						/* don't exceed screen size */
    int s = XScreenNumberOfScreen(xgwa.screen);
    int w = to_pow2(XDisplayWidth(dpy, s), False);
    int h = to_pow2(XDisplayHeight(dpy, s), False);
    if (size > w) size = w;
    if (size > h) size = h;
  }

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
  XFreePixmap(dpy, bitmap);

  display (self);
  XSync(dpy, False);
  screenhack_handle_events (dpy);
}

static void
display (Pixmap pixmap)
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
  if (depth != 1)
    XCopyArea (dpy, pixmap, window, gc, 0, 0, size, size,
	       (xgwa.width-size)>>1, (xgwa.height-size)>>1);
  else
    XCopyPlane (dpy, pixmap, window, gc, 0, 0, size, size,
		(xgwa.width-size)>>1, (xgwa.height-size)>>1, 1);
/*
  XDrawRectangle (dpy, window, gc,
		  ((xgwa.width-size)>>1)-1, ((xgwa.height-size)>>1)-1,
		  size+2, size+2);
*/
  XSync (dpy, False);
  screenhack_handle_events (dpy);
}


char *progclass = "BlitSpin";

char *defaults [] = {
  ".background:	black",
  ".foreground:	white",
  "*delay:	500000",
  "*delay2:	500000",
  "*bitmap:	(default)",
  "*geometry:	512x512",
  0
};

XrmOptionDescRec options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-delay2",		".delay2",	XrmoptionSepArg, 0 },
  { "-bitmap",		".bitmap",	XrmoptionSepArg, 0 },
  { "-grab-screen",	".bitmap",	XrmoptionNoArg, "(screen)" },
  { 0, 0, 0, 0 }
};

void
screenhack (Display *d, Window w)
{
  dpy = d;
  window = w;
  init ();
  if (delay2) usleep (delay2 * 2);
  while (1)
    {
      rotate ();
      screenhack_handle_events (d);
      if (delay2) usleep (delay2);
    }
}
