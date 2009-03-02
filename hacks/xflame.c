/* xflame, Copyright (c) 1996-2002 Carsten Haitzler <raster@redhat.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* Version history as near as I (jwz) can piece together:

   * Carsten Haitzler <raster@redhat.com> wrote the first version in 1996.

   * Rahul Jain <rahul@rice.edu> added support for TrueColor displays.

   * Someone did a rough port of it to use the xscreensaver utility routines
     instead of creating its own window by hand.

   * Someone (probably Raster) came up with a subsequent version that had
     a Red Hat logo hardcoded into it.

   * Daniel Zahn <stumpy@religions.com> found that version in 1998, and 
     hacked it to be able to load a different logo from a PGM (P5) file, 
     with a single hardcoded pathname.

   * Jamie Zawinski <jwz@jwz.org> found several versions of xflame in
     March 1999, and pieced them together.  Changes:

       - Correct and fault-tolerant use of the Shared Memory extension;
         previous versions of xflame did not work when $DISPLAY was remote.

       - Replaced PGM-reading code with code that can read arbitrary XBM
         and XPM files (color ones will be converted to grayscale.)

       - Command-line options all around -- no hardcoded pathnames or
         behavioral constants.

       - General cleanup and portability tweaks.

   * 4-Oct-99, jwz: added support for packed-24bpp (versus 32bpp.)
   * 16-Jan-2002, jwz: added gdk_pixbuf support.

 */


/* portions by Daniel Zahn <stumpy@religions.com> */


#include "screenhack.h"
#include "xpm-pixmap.h"
#include <X11/Xutil.h>
#include <limits.h>

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#ifdef HAVE_XSHM_EXTENSION
# include "xshm.h"
#endif /* HAVE_XSHM_EXTENSION */

#include "images/bob.xbm"

#define MAX_VAL             255

static Display         *display;
static Window          window;
static int             depth;
static int             width;
static int             height;
static Colormap        colormap;
static Visual          *visual;
static Screen          *screen;
static Bool            shared;
static Bool            bloom;
static XImage          *xim;
#ifdef HAVE_XSHM_EXTENSION
static XShmSegmentInfo shminfo;
#endif /* HAVE_XSHM_EXTENSION */
static GC              gc;
static int             ctab[256];

static unsigned char  *flame;
static unsigned char  *theim;
static int             fwidth;
static int             fheight;
static int             top;
static int             hspread;
static int             vspread;
static int             residual;

static int ihspread;
static int ivspread;
static int iresidual;
static int variance;
static int vartrend;

static void
GetXInfo(Display *disp, Window win)
{
  XWindowAttributes xwa;
   
  XGetWindowAttributes(disp,win,&xwa);

  window   = win;
  display  = disp;
  colormap = xwa.colormap;
  depth    = xwa.depth;
  visual   = xwa.visual;
  screen   = xwa.screen;
  width    = xwa.width;
  height   = xwa.height;

  if (width%2)
    width++;
  if (height%2)
    height++;
}

static void
MakeImage(void)
{
  XGCValues gcv;

#ifdef HAVE_XSHM_EXTENSION
  shared = True;
  xim = create_xshm_image (display, visual, depth, ZPixmap, NULL,
                           &shminfo, width, height);
#else  /* !HAVE_XSHM_EXTENSION */
  xim = 0;
#endif /* !HAVE_XSHM_EXTENSION */

  if (!xim)
    {
      shared = False;
      xim = XCreateImage (display, visual, depth, ZPixmap, 0, NULL,
                          width, height, 32, 0);
      if (xim)
        xim->data = (char *) calloc(xim->height, xim->bytes_per_line);
      if (!xim || !xim->data)
        {
          fprintf(stderr,"%s: out of memory.\n", progname);
          exit(1);
        }
    }

  gc = XCreateGC(display,window,0,&gcv);
  if (!gc) exit (1);
}


static void
InitColors(void)
{
  int i = 0, j = 0, red = 0, green = 0, blue = 0;
  XColor fg;

  /* Make it possible to set the color of the flames, 
     by Raymond Medeiros <ray@stommel.marine.usf.edu> and jwz.
  */
  fg.pixel = get_pixel_resource ("foreground", "Foreground",
                                 display, colormap);
  XQueryColor (display, colormap, &fg);

  red   = 255 - (fg.red   >> 8);
  green = 255 - (fg.green >> 8);
  blue  = 255 - (fg.blue  >> 8);


  for (i = 0; i < 256 * 2; i += 2)
    {
      XColor xcl;
      int r = (i - red)   * 3;
      int g = (i - green) * 3;
      int b = (i - blue)  * 3;
    
      if (r < 0)   r = 0;
      if (r > 255) r = 255;
      if (g < 0)   g = 0;
      if (g > 255) g = 255;
      if (b < 0)   b = 0;
      if (b > 255) b = 255;

      xcl.red   = (unsigned short)((r << 8) | r);
      xcl.green = (unsigned short)((g << 8) | g);
      xcl.blue  = (unsigned short)((b << 8) | b);
      xcl.flags = DoRed | DoGreen | DoBlue;

      XAllocColor(display,colormap,&xcl);

      ctab[j++] = (int)xcl.pixel;
    }
}


static void
DisplayImage(void)
{
#ifdef HAVE_XSHM_EXTENSION
  if (shared)
    XShmPutImage(display, window, gc, xim, 0,(top - 1) << 1, 0,
                 (top - 1) << 1, width, height - ((top - 1) << 1), False);
  else
#endif /* HAVE_XSHM_EXTENSION */
    XPutImage(display, window, gc, xim, 0, (top - 1) << 1, 0,
              (top - 1) << 1, width, height - ((top - 1) << 1));
}


static void
InitFlame(void)
{
  fwidth  = width / 2;
  fheight = height / 2;
  flame   = (unsigned char *) malloc((fwidth + 2) * (fheight + 2)
                                     * sizeof(unsigned char));

  if (!flame)
    {
      fprintf(stderr,"%s: out of memory\n", progname);
      exit(1);
    }

  top      = 1;
  ihspread  = get_integer_resource("hspread", "Integer");
  ivspread  = get_integer_resource("vspread", "Integer");
  iresidual = get_integer_resource("residual", "Integer");
  variance  = get_integer_resource("variance", "Integer");
  vartrend  = get_integer_resource("vartrend", "Integer");
  bloom     = get_boolean_resource("bloom",    "Boolean");

# define THROTTLE(VAR,NAME) \
  if (VAR < 0 || VAR > 255) { \
    fprintf(stderr, "%s: %s must be in the range 0-255 (not %d).\n", \
            progname, NAME, VAR); \
    exit(1); }
  THROTTLE (ihspread, "hspread");
  THROTTLE (ivspread, "vspread");
  THROTTLE (iresidual,"residual");
  THROTTLE (variance, "variance");
  THROTTLE (vartrend, "vartrend");
# undef THROTTLE



  hspread = ihspread;
  vspread = ivspread;
  residual = iresidual;
}


static void
Flame2Image16(void)
{
  int x,y;
  unsigned short *ptr;
  unsigned char *ptr1;
  int v1,v2,v3,v4;

  ptr  = (unsigned short *)xim->data;
  ptr += (top << 1) * width;
  ptr1 = flame + 1 + (top * (fwidth + 2));

  for(y = top; y < fheight; y++)
    {
      for( x = 0; x < fwidth; x++)
        {
          v1 = (int)*ptr1;
          v2 = (int)*(ptr1 + 1);
          v3 = (int)*(ptr1 + fwidth + 2);
          v4 = (int)*(ptr1 + fwidth + 2 + 1);
          ptr1++;
          *ptr++ = (unsigned short)ctab[v1];
          *ptr   = (unsigned short)ctab[(v1 + v2) >> 1];
          ptr   += width - 1;
          *ptr++ = (unsigned short)ctab[(v1 + v3) >> 1];
          *ptr   = (unsigned short)ctab[(v1 + v4) >> 1];
          ptr   -= width - 1;
        }
      ptr  += width;
      ptr1 += 2;
    }
}

static void
Flame2Image32(void)
{
  int x,y;
  unsigned int *ptr;
  unsigned char *ptr1;
  int v1,v2,v3,v4;

  ptr  = (unsigned int *)xim->data;
  ptr += (top << 1) * width;
  ptr1 = flame + 1 + (top * (fwidth + 2));

  for( y = top; y < fheight; y++)
    {
      for( x = 0; x < fwidth; x++)
        {
          v1 = (int)*ptr1;
          v2 = (int)*(ptr1 + 1);
          v3 = (int)*(ptr1 + fwidth + 2);
          v4 = (int)*(ptr1 + fwidth + 2 + 1);
          ptr1++;
          *ptr++ = (unsigned int)ctab[v1];
          *ptr   = (unsigned int)ctab[(v1 + v2) >> 1];
          ptr   += width - 1;
          *ptr++ = (unsigned int)ctab[(v1 + v3) >> 1];
          *ptr   = (unsigned int)ctab[(v1 + v4) >> 1];
          ptr   -= width - 1;
        }
      ptr  += width;
      ptr1 += 2;
    }
}

static void
Flame2Image24(void)
{
  int x,y;
  unsigned char *ptr;
  unsigned char *ptr1;
  int v1,v2,v3,v4;

  ptr  = (unsigned char *)xim->data;
  ptr += (top << 1) * xim->bytes_per_line;
  ptr1 = flame + 1 + (top * (fwidth + 2));

  for( y = top; y < fheight; y++)
    {
      unsigned char *last_ptr = ptr;
      for( x = 0; x < fwidth; x++)
        {
          v1 = (int)*ptr1;
          v2 = (int)*(ptr1 + 1);
          v3 = (int)*(ptr1 + fwidth + 2);
          v4 = (int)*(ptr1 + fwidth + 2 + 1);
          ptr1++;

          ptr[2] = ((unsigned int)ctab[v1] & 0x00FF0000) >> 16;
          ptr[1] = ((unsigned int)ctab[v1] & 0x0000FF00) >> 8;
          ptr[0] = ((unsigned int)ctab[v1] & 0x000000FF);
          ptr += 3;

          ptr[2] = ((unsigned int)ctab[(v1 + v2) >> 1] & 0x00FF0000) >> 16;
          ptr[1] = ((unsigned int)ctab[(v1 + v2) >> 1] & 0x0000FF00) >> 8;
          ptr[0] = ((unsigned int)ctab[(v1 + v2) >> 1] & 0x000000FF);
          ptr += ((width - 1) * 3);

          ptr[2] = ((unsigned int)ctab[(v1 + v3) >> 1] & 0x00FF0000) >> 16;
          ptr[1] = ((unsigned int)ctab[(v1 + v3) >> 1] & 0x0000FF00) >> 8;
          ptr[0] = ((unsigned int)ctab[(v1 + v3) >> 1] & 0x000000FF);
          ptr += 3;

          ptr[2] = ((unsigned int)ctab[(v1 + v4) >> 1] & 0x00FF0000) >> 16;
          ptr[1] = ((unsigned int)ctab[(v1 + v4) >> 1] & 0x0000FF00) >> 8;
          ptr[0] = ((unsigned int)ctab[(v1 + v4) >> 1] & 0x000000FF);
          ptr -= ((width - 1) * 3);
        }

      ptr = last_ptr + (xim->bytes_per_line << 1);
      ptr1 += 2;
    }
}

static void
Flame2Image8(void)
{
  int x,y;
  unsigned char *ptr;
  unsigned char *ptr1;
  int v1,v2,v3,v4;

  ptr  = (unsigned char *)xim->data;
  ptr += (top << 1) * width;
  ptr1 = flame + 1 + (top * (fwidth + 2));

  for(y=top;y<fheight;y++)
    {
      for(x=0;x<fwidth;x++)
        {
          v1 = (int)*ptr1;
          v2 = (int)*(ptr1 + 1);
          v3 = (int)*(ptr1 + fwidth + 2);
          v4 = (int)*(ptr1 + fwidth + 2 + 1);
          ptr1++;
          *ptr++ = (unsigned char)ctab[v1];
          *ptr   = (unsigned char)ctab[(v1 + v2) >> 1];
          ptr   += width - 1;
          *ptr++ = (unsigned char)ctab[(v1 + v3) >> 1];
          *ptr   = (unsigned char)ctab[(v1 + v4) >> 1];
          ptr   -= width - 1;
        }
      ptr  += width;
      ptr1 += 2;
    }
}

static void
Flame2Image1234567(void)
{
  int x,y;
  unsigned char *ptr1;
  int v1,v2,v3,v4;

  ptr1 = flame + 1 + (top * (fwidth + 2));

  for( y = top; y < fheight; y++)
    {
      for( x = 0; x < fwidth; x++)
        {
          v1 = (int)*ptr1;
          v2 = (int)*(ptr1 + 1);
          v3 = (int)*(ptr1 + fwidth + 2);
          v4 = (int)*(ptr1 + fwidth + 2 + 1);
          ptr1++;
          XPutPixel(xim,(x << 1),    (y << 1),    ctab[v1]);
          XPutPixel(xim,(x << 1) + 1,(y << 1),    ctab[(v1 + v2) >> 1]);
          XPutPixel(xim,(x << 1),    (y << 1) + 1,ctab[(v1 + v3) >> 1]);
          XPutPixel(xim,(x << 1) + 1,(y << 1) + 1,ctab[(v1 + v4) >> 1]);
        }
    }
}

static void
Flame2Image(void)
{
  switch (xim->bits_per_pixel)
    {
    case 32: Flame2Image32(); break;
    case 24: Flame2Image24(); break;
    case 16: Flame2Image16(); break;
    case 8:  Flame2Image8();  break;
    default:
      if (xim->bits_per_pixel <= 7)
        Flame2Image1234567();
      else
        abort();
      break;
    }
}


static void
FlameActive(void)
{
  int x,v1;
  unsigned char *ptr1;
   
  ptr1 = flame + ((fheight + 1) * (fwidth + 2));

  for (x = 0; x < fwidth + 2; x++)
    {
      v1      = *ptr1;
      v1     += ((random() % variance) - vartrend);
      *ptr1++ = v1 % 255;
    }

  if (bloom)
    {
      v1= (random() % 100);
      if (v1 == 10)
	residual += (random()%10);
      else if (v1 == 20)
	hspread += (random()%15);
      else if (v1 == 30)
	vspread += (random()%20);
    }

  residual = ((iresidual* 10) + (residual *90)) / 100;
  hspread  = ((ihspread * 10) + (hspread  *90)) / 100;
  vspread  = ((ivspread * 10) + (vspread  *90)) / 100;
}


static void
FlameAdvance(void)
{
  int x,y;
  unsigned char *ptr2;
  int newtop = top;

  for (y = fheight + 1; y >= top; y--)
    {
      int used = 0;
      unsigned char *ptr1 = flame + 1 + (y * (fwidth + 2));
      for (x = 0; x < fwidth; x++)
        {
          int v1 = (int)*ptr1;
          int v2, v3;
          if (v1 > 0)
            {
              used = 1;
              ptr2 = ptr1 - fwidth - 2;
              v3   = (v1 * vspread) >> 8;
              v2   = (int)*(ptr2);
              v2  += v3;
              if (v2 > MAX_VAL) 
                v2 = MAX_VAL;

              *(ptr2) = (unsigned char)v2;
              v3  = (v1 * hspread) >> 8;
              v2  = (int)*(ptr2 + 1);
              v2 += v3;
              if (v2 > MAX_VAL) 
                v2 = MAX_VAL;
          
              *(ptr2 + 1) = (unsigned char)v2;
              v2          = (int)*(ptr2 - 1);
              v2         += v3;
              if (v2 > MAX_VAL) 
                v2 = MAX_VAL;
          
              *(ptr2 - 1) = (unsigned char)v2;
        
              if (y < fheight + 1)
                {
                  v1    = (v1 * residual) >> 8;
                  *ptr1 = (unsigned char)v1;
                }
            }
          ptr1++;
          if (used) 
            newtop = y - 1;
        }
 
      /* clean up the right gutter */
      {
        int v1 = (int)*ptr1;
        v1 = (v1 * residual) >> 8;
        *ptr1 = (unsigned char)v1;
      }
    }

  top = newtop - 1;

  if (top < 1)
    top = 1;
}


static void
FlameFill(int val)
{
  int x, y;
  for (y = 0; y < fheight + 1; y++)
    {
      unsigned char *ptr1 = flame + 1 + (y * (fwidth + 2));
      for (x = 0; x < fwidth; x++)
        {
          *ptr1 = val;
          ptr1++;
        }
    }
}


static void
FlamePasteData(unsigned char *d, int xx, int yy, int w, int h)
{
  unsigned char *ptr1,*ptr2;
  ptr2 = d;

  if (xx < 0) xx = 0;
  if (yy < 0) yy = 0;

  if ((xx >= 0) &&
      (yy >= 0) &&
      (xx + w <= fwidth) &&
      (yy + h <= fheight))
    {
      int x, y;
      for (y = 0; y < h; y++)
        {
          ptr1 = flame + 1 + xx + ((yy + y) * (fwidth + 2));
          for (x = 0; x < w; x++)
            {
              if (*ptr2 / 24)
                *ptr1 += random() % (*ptr2 / 24);

              ptr1++;
              ptr2++;
            }
        }
    }
  else
    {
      static Bool warned = False;
      if (!warned)
        {
          fprintf (stderr, "%s: window is %dx%d; image must be "
                   "smaller than %dx%d (not %dx%d).\n",
                   progname, width, height, fwidth, fheight, w, h);
          warned = True;
        }
    }
}


static unsigned char *
loadBitmap(int *w, int *h)
{
  char *bitmap_name = get_string_resource ("bitmap", "Bitmap");

  if (!bitmap_name ||
      !*bitmap_name ||
      !strcmp(bitmap_name, "none"))
    ;
  else if (!strcmp(bitmap_name, "(default)"))   /* use the builtin */
    {
      XImage *ximage;
      unsigned char *result, *o;
      char *bits = (char *) malloc (sizeof(bob_bits));
      int x, y;
      int scale = ((width > bob_width * 11) ? 2 : 1);
 
      memcpy (bits, bob_bits, sizeof(bob_bits));
      ximage = XCreateImage (display, visual, 1, XYBitmap, 0, bits,
                             bob_width, bob_height, 8, 0);
      ximage->byte_order = LSBFirst;
      ximage->bitmap_bit_order = LSBFirst;
      *w = ximage->width * scale;
      *h = ximage->height * scale;
      o = result = (unsigned char *) malloc ((*w * scale) * (*h * scale));
      for (y = 0; y < *h; y++)
        for (x = 0; x < *w; x++)
          *o++ = (XGetPixel(ximage, x/scale, y/scale) ? 255 : 0);
       
      return result;
    }
  else  /* load a bitmap file */
    {
      Pixmap pixmap =
        xpm_file_to_pixmap (display, window, bitmap_name, &width, &height, 0);
      XImage *image;
      int x, y;
      unsigned char *result, *o;
      XColor colors[256];
      Bool cmap_p = has_writable_cells (screen, visual);

      if (cmap_p)
        {
          int i;
          for (i = 0; i < countof (colors); i++)
            colors[i].pixel = i;
          XQueryColors (display, colormap, colors, countof (colors));
        }

      image = XGetImage (display, pixmap, 0, 0, width, height, ~0L, ZPixmap);
      XFreePixmap(display, pixmap);

      result = (unsigned char *) malloc (width * height);
      o = result;
      for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
          {
            int rgba = XGetPixel (image, x, y);
            int gray;
            if (cmap_p)
              gray = ((200 - ((((colors[rgba].red   >> 8) & 0xFF) +
                              ((colors[rgba].green >> 8) & 0xFF) +
                              ((colors[rgba].blue  >> 8) & 0xFF))
                             >> 1))
                      & 0xFF);
            else
              /* This is *so* not handling all the cases... */
              gray = (image->depth > 16
                      ? ((((rgba >> 24) & 0xFF) +
                          ((rgba >> 16) & 0xFF) +
                          ((rgba >>  8) & 0xFF) +
                          ((rgba      ) & 0xFF)) >> 2)
                      : ((((rgba >> 12) & 0x0F) +
                          ((rgba >>  8) & 0x0F) +
                          ((rgba >>  4) & 0x0F) +
                          ((rgba      ) & 0x0F)) >> 1));

            *o++ = 255 - gray;
          }

      XFree (image->data);
      image->data = 0;
      XDestroyImage (image);

      *w = width;
      *h = height;
      return result;
    }

  *w = 0;
  *h = 0;
  return 0;

}



char *progclass = "XFlame";

char *defaults [] = {
  ".background:     black",
  ".foreground:     #FFAF5F",
  "*bitmap:         (default)",
  "*bitmapBaseline: 20",
  "*delay:          10000",
  "*hspread:        30",
  "*vspread:        97",
  "*residual:       99",
  "*variance:       50",
  "*vartrend:       20",
  "*bloom:          True",   

#ifdef HAVE_XSHM_EXTENSION
  "*useSHM: False",   /* xshm turns out not to help. */
#endif /* HAVE_XSHM_EXTENSION */
   0
};

XrmOptionDescRec options [] = {
  { "-delay",     ".delay",          XrmoptionSepArg, 0 },
  { "-bitmap",    ".bitmap",         XrmoptionSepArg, 0 },
  { "-baseline",  ".bitmapBaseline", XrmoptionSepArg, 0 },
  { "-hspread",   ".hspread",        XrmoptionSepArg, 0 },
  { "-vspread",   ".vspread",        XrmoptionSepArg, 0 },
  { "-residual",  ".residual",       XrmoptionSepArg, 0 },
  { "-variance",  ".variance",       XrmoptionSepArg, 0 },
  { "-vartrend",  ".vartrend",       XrmoptionSepArg, 0 },
  { "-bloom",     ".bloom",          XrmoptionNoArg, "True" },
  { "-no-bloom",  ".bloom",          XrmoptionNoArg, "False" },
#ifdef HAVE_XSHM_EXTENSION
  { "-shm",       ".useSHM",         XrmoptionNoArg, "True" },
  { "-no-shm",    ".useSHM",         XrmoptionNoArg, "False" },
#endif /* HAVE_XSHM_EXTENSION */
  { 0, 0, 0, 0 }
};

void
screenhack (Display *disp, Window win)
{
  int theimx = 0, theimy = 0;
  int baseline = get_integer_resource ("bitmapBaseline", "Integer");
  int delay = get_integer_resource ("delay", "Integer");
  xim      = NULL;
  top      = 1;
  flame    = NULL;

  GetXInfo(disp,win);
  InitColors();
  theim = loadBitmap(&theimx, &theimy);

  /* utils/xshm.c doesn't provide a way to free the shared-memory image, which
     makes it hard for us to react to window resizing.  So, punt for now.  The
     size of the window at startup is the size it will stay.
  */
  GetXInfo(disp,win);

  MakeImage();
  InitFlame();
  FlameFill(0);

  while (1)
    {
      FlameActive();

      if (theim)
        FlamePasteData(theim, (fwidth - theimx) / 2,
                       fheight - theimy - baseline, theimx, theimy);

      FlameAdvance();
      Flame2Image();
      DisplayImage();

      XSync(display,False);
      screenhack_handle_events(display);
      if (delay)
        usleep (delay);
    }
}
