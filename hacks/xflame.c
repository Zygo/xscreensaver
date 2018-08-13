/* xflame, Copyright (c) 1996-2018 Carsten Haitzler <raster@redhat.com>
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
   * 9-Oct-2016, Dave Odell <dmo2118@gmail.com>: Updated for new xshm.c.

 */

/* portions by Daniel Zahn <stumpy@religions.com> */


#include "screenhack.h"
#include "ximage-loader.h"
#include <limits.h>

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

# undef MAX
# undef MIN
# define MAX(A,B) ((A)>(B)?(A):(B))
# define MIN(A,B) ((A)<(B)?(A):(B))

#include "xshm.h"

#include "images/gen/bob_png.h"

#define MAX_VAL             255

struct state {
  Display *dpy;
  Window window;
  int             depth;
  int             width;
  int             height;
  Colormap        colormap;
  Visual          *visual;
  Screen          *screen;
  Bool            bloom;
  XImage          *xim;
  XShmSegmentInfo shminfo;
  GC              gc;
  int             ctab[256];

  unsigned char  *flame;
  unsigned char  *theim;
  int             fwidth;
  int             fheight;
  int             top;
  int             hspread;
  int             vspread;
  int             residual;

  int ihspread;
  int ivspread;
  int iresidual;
  int variance;
  int vartrend;

  int delay;
  int baseline;
  int theimx, theimy;
};

static void
GetXInfo(struct state *st)
{
  XWindowAttributes xwa;
   
  XGetWindowAttributes(st->dpy,st->window,&xwa);

  st->colormap = xwa.colormap;
  st->depth    = xwa.depth;
  st->visual   = xwa.visual;
  st->screen   = xwa.screen;
  st->width    = xwa.width;
  st->height   = xwa.height;

  if (st->width%2)
    st->width++;
  if (st->height%2)
    st->height++;
}

static void
MakeImage(struct state *st)
{
  XGCValues gcv;

  if (st->xim)
    destroy_xshm_image (st->dpy, st->xim, &st->shminfo);

  st->xim = create_xshm_image (st->dpy, st->visual, st->depth, ZPixmap,
                               &st->shminfo, st->width, st->height);
  if (!st->xim)
    {
      fprintf(stderr,"%s: out of memory.\n", progname);
      exit(1);
    }

  if (! st->gc)
    st->gc = XCreateGC(st->dpy,st->window,0,&gcv);
}


static void
InitColors(struct state *st)
{
  int i = 0, j = 0, red = 0, green = 0, blue = 0;
  XColor fg;

  /* Make it possible to set the color of the flames, 
     by Raymond Medeiros <ray@stommel.marine.usf.edu> and jwz.
  */
  fg.pixel = get_pixel_resource (st->dpy, st->colormap,
                                 "foreground", "Foreground");
  XQueryColor (st->dpy, st->colormap, &fg);

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

      XAllocColor(st->dpy,st->colormap,&xcl);

      st->ctab[j++] = (int)xcl.pixel;
    }
}


static void
DisplayImage(struct state *st)
{
  put_xshm_image(st->dpy, st->window, st->gc, st->xim, 0,(st->top - 1) << 1, 0,
                 (st->top - 1) << 1, st->width, st->height - ((st->top - 1) << 1),
                 &st->shminfo);
}


static void
InitFlame(struct state *st)
{
  st->fwidth  = st->width / 2;
  st->fheight = st->height / 2;

  if (st->flame) free (st->flame);
  st->flame   = (unsigned char *) malloc((st->fwidth + 2) * (st->fheight + 2)
                                     * sizeof(unsigned char));

  if (!st->flame)
    {
      fprintf(stderr,"%s: out of memory\n", progname);
      exit(1);
    }

  st->top      = 1;
  st->ihspread  = get_integer_resource(st->dpy, "hspread", "Integer");
  st->ivspread  = get_integer_resource(st->dpy, "vspread", "Integer");
  st->iresidual = get_integer_resource(st->dpy, "residual", "Integer");
  st->variance  = get_integer_resource(st->dpy, "variance", "Integer");
  st->vartrend  = get_integer_resource(st->dpy, "vartrend", "Integer");
  st->bloom     = get_boolean_resource(st->dpy, "bloom",    "Boolean");

# define THROTTLE(VAR,NAME) \
  if (VAR < 0 || VAR > 255) { \
    fprintf(stderr, "%s: %s must be in the range 0-255 (not %d).\n", \
            progname, NAME, VAR); \
    exit(1); }
  THROTTLE (st->ihspread, "hspread");
  THROTTLE (st->ivspread, "vspread");
  THROTTLE (st->iresidual,"residual");
  THROTTLE (st->variance, "variance");
  THROTTLE (st->vartrend, "vartrend");
# undef THROTTLE

#if 0
  if (st->width > 2560)  /* Retina displays */
    {
      /* #### One of these knobs must mean "make the fire be twice as tall"
         but I can't figure out how. Changing any of the default values
         of any of these seems to just make it all go crappy. */
      st->ivspread = MAX (0, MIN (255, st->ivspread + 1));
    }
#endif

  st->hspread = st->ihspread;
  st->vspread = st->ivspread;
  st->residual = st->iresidual;
}


static void
Flame2Image16(struct state *st)
{
  int x,y;
  unsigned short *ptr;
  unsigned char *ptr1;
  int v1,v2,v3,v4;

  ptr  = (unsigned short *)st->xim->data;
  ptr += (st->top << 1) * st->width;
  ptr1 = st->flame + 1 + (st->top * (st->fwidth + 2));

  for(y = st->top; y < st->fheight; y++)
    {
      for( x = 0; x < st->fwidth; x++)
        {
          v1 = (int)*ptr1;
          v2 = (int)*(ptr1 + 1);
          v3 = (int)*(ptr1 + st->fwidth + 2);
          v4 = (int)*(ptr1 + st->fwidth + 2 + 1);
          ptr1++;
          *ptr++ = (unsigned short)st->ctab[v1];
          *ptr   = (unsigned short)st->ctab[(v1 + v2) >> 1];
          ptr   += st->width - 1;
          *ptr++ = (unsigned short)st->ctab[(v1 + v3) >> 1];
          *ptr   = (unsigned short)st->ctab[(v1 + v4) >> 1];
          ptr   -= st->width - 1;
        }
      ptr  += st->width;
      ptr1 += 2;
    }
}

static void
Flame2Image32(struct state *st)
{
  int x,y;
  unsigned int *ptr;
  unsigned char *ptr1;
  int v1,v2,v3,v4;

  ptr  = (unsigned int *)st->xim->data;
  ptr += (st->top << 1) * st->width;
  ptr1 = st->flame + 1 + (st->top * (st->fwidth + 2));

  for( y = st->top; y < st->fheight; y++)
    {
      for( x = 0; x < st->fwidth; x++)
        {
          v1 = (int)*ptr1;
          v2 = (int)*(ptr1 + 1);
          v3 = (int)*(ptr1 + st->fwidth + 2);
          v4 = (int)*(ptr1 + st->fwidth + 2 + 1);
          ptr1++;
          *ptr++ = (unsigned int)st->ctab[v1];
          *ptr   = (unsigned int)st->ctab[(v1 + v2) >> 1];
          ptr   += st->width - 1;
          *ptr++ = (unsigned int)st->ctab[(v1 + v3) >> 1];
          *ptr   = (unsigned int)st->ctab[(v1 + v4) >> 1];
          ptr   -= st->width - 1;
        }
      ptr  += st->width;
      ptr1 += 2;
    }
}

static void
Flame2Image24(struct state *st)
{
  int x,y;
  unsigned char *ptr;
  unsigned char *ptr1;
  int v1,v2,v3,v4;

  ptr  = (unsigned char *)st->xim->data;
  ptr += (st->top << 1) * st->xim->bytes_per_line;
  ptr1 = st->flame + 1 + (st->top * (st->fwidth + 2));

  for( y = st->top; y < st->fheight; y++)
    {
      unsigned char *last_ptr = ptr;
      for( x = 0; x < st->fwidth; x++)
        {
          v1 = (int)*ptr1;
          v2 = (int)*(ptr1 + 1);
          v3 = (int)*(ptr1 + st->fwidth + 2);
          v4 = (int)*(ptr1 + st->fwidth + 2 + 1);
          ptr1++;

          ptr[2] = ((unsigned int)st->ctab[v1] & 0x00FF0000) >> 16;
          ptr[1] = ((unsigned int)st->ctab[v1] & 0x0000FF00) >> 8;
          ptr[0] = ((unsigned int)st->ctab[v1] & 0x000000FF);
          ptr += 3;

          ptr[2] = ((unsigned int)st->ctab[(v1 + v2) >> 1] & 0x00FF0000) >> 16;
          ptr[1] = ((unsigned int)st->ctab[(v1 + v2) >> 1] & 0x0000FF00) >> 8;
          ptr[0] = ((unsigned int)st->ctab[(v1 + v2) >> 1] & 0x000000FF);
          ptr += ((st->width - 1) * 3);

          ptr[2] = ((unsigned int)st->ctab[(v1 + v3) >> 1] & 0x00FF0000) >> 16;
          ptr[1] = ((unsigned int)st->ctab[(v1 + v3) >> 1] & 0x0000FF00) >> 8;
          ptr[0] = ((unsigned int)st->ctab[(v1 + v3) >> 1] & 0x000000FF);
          ptr += 3;

          ptr[2] = ((unsigned int)st->ctab[(v1 + v4) >> 1] & 0x00FF0000) >> 16;
          ptr[1] = ((unsigned int)st->ctab[(v1 + v4) >> 1] & 0x0000FF00) >> 8;
          ptr[0] = ((unsigned int)st->ctab[(v1 + v4) >> 1] & 0x000000FF);
          ptr -= ((st->width - 1) * 3);
        }

      ptr = last_ptr + (st->xim->bytes_per_line << 1);
      ptr1 += 2;
    }
}

static void
Flame2Image8(struct state *st)
{
  int x,y;
  unsigned char *ptr;
  unsigned char *ptr1;
  int v1,v2,v3,v4;

  ptr  = (unsigned char *)st->xim->data;
  ptr += (st->top << 1) * st->width;
  ptr1 = st->flame + 1 + (st->top * (st->fwidth + 2));

  for(y=st->top;y<st->fheight;y++)
    {
      for(x=0;x<st->fwidth;x++)
        {
          v1 = (int)*ptr1;
          v2 = (int)*(ptr1 + 1);
          v3 = (int)*(ptr1 + st->fwidth + 2);
          v4 = (int)*(ptr1 + st->fwidth + 2 + 1);
          ptr1++;
          *ptr++ = (unsigned char)st->ctab[v1];
          *ptr   = (unsigned char)st->ctab[(v1 + v2) >> 1];
          ptr   += st->width - 1;
          *ptr++ = (unsigned char)st->ctab[(v1 + v3) >> 1];
          *ptr   = (unsigned char)st->ctab[(v1 + v4) >> 1];
          ptr   -= st->width - 1;
        }
      ptr  += st->width;
      ptr1 += 2;
    }
}

static void
Flame2Image1234567(struct state *st)
{
  int x,y;
  unsigned char *ptr1;
  int v1,v2,v3,v4;

  ptr1 = st->flame + 1 + (st->top * (st->fwidth + 2));

  for( y = st->top; y < st->fheight; y++)
    {
      for( x = 0; x < st->fwidth; x++)
        {
          v1 = (int)*ptr1;
          v2 = (int)*(ptr1 + 1);
          v3 = (int)*(ptr1 + st->fwidth + 2);
          v4 = (int)*(ptr1 + st->fwidth + 2 + 1);
          ptr1++;
          XPutPixel(st->xim,(x << 1),    (y << 1),    st->ctab[v1]);
          XPutPixel(st->xim,(x << 1) + 1,(y << 1),    st->ctab[(v1 + v2) >> 1]);
          XPutPixel(st->xim,(x << 1),    (y << 1) + 1,st->ctab[(v1 + v3) >> 1]);
          XPutPixel(st->xim,(x << 1) + 1,(y << 1) + 1,st->ctab[(v1 + v4) >> 1]);
        }
    }
}

static void
Flame2Image(struct state *st)
{
  switch (st->xim->bits_per_pixel)
    {
    case 32: Flame2Image32(st); break;
    case 24: Flame2Image24(st); break;
    case 16: Flame2Image16(st); break;
    case 8:  Flame2Image8(st);  break;
    default:
      if (st->xim->bits_per_pixel <= 7)
        Flame2Image1234567(st);
      else
        abort();
      break;
    }
}


static void
FlameActive(struct state *st)
{
  int x,v1;
  unsigned char *ptr1;
   
  ptr1 = st->flame + ((st->fheight + 1) * (st->fwidth + 2));

  for (x = 0; x < st->fwidth + 2; x++)
    {
      v1      = *ptr1;
      v1     += ((random() % st->variance) - st->vartrend);
      *ptr1++ = v1 % 255;
    }

  if (st->bloom)
    {
      v1= (random() % 100);
      if (v1 == 10)
	st->residual += (random()%10);
      else if (v1 == 20)
	st->hspread += (random()%15);
      else if (v1 == 30)
	st->vspread += (random()%20);
    }

  st->residual = ((st->iresidual* 10) + (st->residual *90)) / 100;
  st->hspread  = ((st->ihspread * 10) + (st->hspread  *90)) / 100;
  st->vspread  = ((st->ivspread * 10) + (st->vspread  *90)) / 100;
}


static void
FlameAdvance(struct state *st)
{
  int x,y;
  unsigned char *ptr2;
  int newtop = st->top;

  for (y = st->fheight + 1; y >= st->top; y--)
    {
      int used = 0;
      unsigned char *ptr1 = st->flame + 1 + (y * (st->fwidth + 2));
      for (x = 0; x < st->fwidth; x++)
        {
          int v1 = (int)*ptr1;
          int v2, v3;
          if (v1 > 0)
            {
              used = 1;
              ptr2 = ptr1 - st->fwidth - 2;
              v3   = (v1 * st->vspread) >> 8;
              v2   = (int)*(ptr2);
              v2  += v3;
              if (v2 > MAX_VAL) 
                v2 = MAX_VAL;

              *(ptr2) = (unsigned char)v2;
              v3  = (v1 * st->hspread) >> 8;
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
        
              if (y < st->fheight + 1)
                {
                  v1    = (v1 * st->residual) >> 8;
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
        v1 = (v1 * st->residual) >> 8;
        *ptr1 = (unsigned char)v1;
      }
    }

  st->top = newtop - 1;

  if (st->top < 1)
    st->top = 1;
}


static void
FlameFill(struct state *st, int val)
{
  int x, y;
  for (y = 0; y < st->fheight + 1; y++)
    {
      unsigned char *ptr1 = st->flame + 1 + (y * (st->fwidth + 2));
      for (x = 0; x < st->fwidth; x++)
        {
          *ptr1 = val;
          ptr1++;
        }
    }
}


static void
FlamePasteData(struct state *st,
               unsigned char *d, int xx, int yy, int w, int h)
{
  unsigned char *ptr1,*ptr2;
  ptr2 = d;

  if (xx < 0) xx = 0;
  if (yy < 0) yy = 0;

  if ((xx >= 0) &&
      (yy >= 0) &&
      (xx + w <= st->fwidth) &&
      (yy + h <= st->fheight))
    {
      int x, y;
      for (y = 0; y < h; y++)
        {
          ptr1 = st->flame + 1 + xx + ((yy + y) * (st->fwidth + 2));
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
          fprintf (stderr, "%s: st->window is %dx%d; image must be "
                   "smaller than %dx%d (not %dx%d).\n",
                   progname, st->width, st->height, st->fwidth, st->fheight, w, h);
          warned = True;
        }
    }
}


static XImage *
double_ximage (Display *dpy, Visual *visual, XImage *image)
{
  int x, y;
  XImage *out = XCreateImage (dpy, visual, image->depth, ZPixmap, 0, 0,
                              image->width * 2, image->height * 2, 8, 0);
  out->data = (char *) malloc (out->height * out->bytes_per_line);
  for (y = 0; y < image->width; y++)
    for (x = 0; x < image->height; x++)
      {
	unsigned long p = XGetPixel (image, x, y);
	XPutPixel (out, x*2,   y*2,   p);
	XPutPixel (out, x*2+1, y*2,   p);
	XPutPixel (out, x*2,   y*2+1, p);
	XPutPixel (out, x*2+1, y*2+1, p);
      }
  XDestroyImage (image);
  return out;
}


static unsigned char *
gaussian_blur (unsigned char *in, int w, int h, double r)
{
  unsigned char *out = malloc(w * h);
  int rs = (int) ((r * 2.57) + 0.5);
  int i, j;

  for (i = 0; i < h; i ++)
    for (j = 0; j < w; j++) 
      {
        double val = 0, wsum = 0;
        int ix, iy;
        for (iy = i-rs; iy<i+rs+1; iy++)
          for (ix = j-rs; ix<j+rs+1; ix++)
            {
              int x = MIN(w-1, MAX(0, ix));
              int y = MIN(h-1, MAX(0, iy));
              int dsq = (ix-j)*(ix-j)+(iy-i)*(iy-i);
              double wght = exp (-dsq / (2*r*r)) / (M_PI*2*r*r);
              val += in[y*w+x] * wght;
              wsum += wght;
            }
        out[i*w+j] = val/wsum;            
      }

  free (in);
  return out;
}


static unsigned char *
loadBitmap (struct state *st)
{
  int x, y;
  unsigned char *result, *o;
  int blur = 0;

# ifdef HAVE_JWXYZ
  const char *bitmap_name = "(default)"; /* #### always use builtin */
# else
  char *bitmap_name = get_string_resource (st->dpy, "bitmap", "Bitmap");
# endif
  XImage *image = 0;
  if (!bitmap_name ||
      !*bitmap_name ||
      !strcmp(bitmap_name, "none"))
    ;
  else if (!strcmp(bitmap_name, "(default)"))   /* use the builtin */
    image = image_data_to_ximage (st->dpy, st->visual,
                                  bob_png, sizeof(bob_png));
  else
    image = file_to_ximage (st->dpy, st->visual, bitmap_name);

  if (! image) return 0;

  while (image->width  < st->width  / 10 &&
         image->height < st->height / 10)
    {
      image = double_ximage (st->dpy, st->visual, image);
      blur++;
    }

  result = (unsigned char *) malloc (image->width * image->height);
  o = result;
  for (y = 0; y < image->height; y++)
    for (x = 0; x < image->width; x++)
      {
        unsigned long agbr = XGetPixel (image, x, image->height - y - 1);
        unsigned long a    = (agbr >> 24) & 0xFF;
        unsigned long gray = (a == 0
                              ? 0xFF
                              : ((((agbr >> 16) & 0xFF) +
                                  ((agbr >>  8) & 0xFF) +
                                  ((agbr >>  0) & 0xFF))
                                 / 3));
        if (gray < 96) gray /= 2;  /* a little more contrast */
        *o++ = 255 - gray;
      }

  /* If we enlarged the image, file off the sharp edges. */
  if (blur > 0)
    result = gaussian_blur (result, image->width, image->height, blur * 1.7);

  st->theimx = image->width;
  st->theimy = image->height;
  XDestroyImage (image);
  return result;
}


static void *
xflame_init (Display *dpy, Window win)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  st->dpy = dpy;
  st->window = win;
  st->baseline = get_integer_resource (dpy, "bitmapBaseline", "Integer");
  st->delay = get_integer_resource (dpy, "delay", "Integer");
  st->xim      = NULL;
  st->top      = 1;
  st->flame    = NULL;

  GetXInfo(st);
  InitColors(st);
  st->theim = loadBitmap(st);

  MakeImage(st);
  InitFlame(st);
  FlameFill(st,0);

  return st;
}

static unsigned long
xflame_draw (Display *dpy, Window win, void *closure)
{
  struct state *st = (struct state *) closure;
  FlameActive(st);

  if (st->theim)
    FlamePasteData(st, st->theim, (st->fwidth - st->theimx) / 2,
                   st->fheight - st->theimy - st->baseline, st->theimx, st->theimy);

  FlameAdvance(st);
  Flame2Image(st);
  DisplayImage(st);

  return st->delay;
}

static void
xflame_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  GetXInfo(st);
  MakeImage(st);
  InitFlame(st);
  FlameFill(st,0);
  XClearWindow (dpy, window);
}

static Bool
xflame_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
xflame_free (Display *dpy, Window window, void *closure)
{
}




static const char *xflame_defaults [] = {
  ".background:     black",
  ".foreground:     #FFAF5F",
  "*fpsTop:	    true",
  "*fpsSolid:       true",
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

static XrmOptionDescRec xflame_options [] = {
  { "-foreground",".foreground",     XrmoptionSepArg, 0 },
  { "-fg",        ".foreground",     XrmoptionSepArg, 0 },
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


XSCREENSAVER_MODULE ("XFlame", xflame)
