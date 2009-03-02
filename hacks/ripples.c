/* ripples, Copyright (c) 1999 Ian McConnell <ian@emit.demon.co.uk>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

/*
 * "Water" ripples that can cross and interfere with each other.
 *
 * I can't remember where I got this idea from, but it's been around for a
 * while in various demos. Some inspiration from
 *      water.txt by Tom Hammersley,tomh@globalnet.co.uk
 *
 * Options
 * -delay	usleep every iteration
 * -rate	Add one drop every "rate" iterations
 * -box         Add big square splash every "box" iters (not very good)
 * -water	Ripples on a grabbed background image
 * -foreground  Interpolate ripples between these two colors
 * -background
 * -oily	Psychedelic colours like man
 * -stir	Add a regular pattern of drops
 * -fluidity	Between 0 and 16. 16 = big drops
 * -light	Hack to add lighting effect
 *
 * Code mainly hacked from xflame and decayscreen.
 */

/* Version history:
 * 13 Oct 1999: Initial hack
 * 30 Oct 1999: Speeded up graphics with dirty buffer. Returned to using
 *              putpixel for greater portability
 *              Added a variety of methods for splashing screen.
 * 31 Oct 1999: Added in lighting hack
 * 13 Nov 1999: Speed up tweaks
 *              Adjust "light" for different bits per colour (-water only)
 *
 */


#include <math.h>
#include "screenhack.h"
#include <X11/Xutil.h>

typedef enum {ripple_drop, ripple_blob, ripple_box, ripple_stir} ripple_mode;

#ifdef HAVE_XSHM_EXTENSION
#include "xshm.h"
static Bool use_shm;
static XShmSegmentInfo shm_info;
#endif /* HAVE_XSHM_EXTENSION */

static Window window;
static Display *display;
static GC gc;
static Visual *visual;

static XImage *orig_map, *buffer_map;
static int ctab[256];
static Colormap colormap;
static int ncolors;
static int light;

static int width, height; /* ripple size */
static int bigwidth, bigheight; /* screen size */

static Bool transparent;
static short *bufferA, *bufferB, *temp;
static char *dirty_buffer;

#define TABLE 256
static double cos_tab[TABLE];

/* Distribution of drops: many little ones and a few big ones. */
static double drop_dist[] =
{0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.2, 0.6};

static void (*draw_transparent)(short *src);

/* How hard to hit the water */
#define SPLASH 512
#undef  MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#undef  MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#undef  DIRTY
#define DIRTY 3 /* dirty >= 2, 1 = restore original pixel, 0 = leave alone */

/* From fortune(6) */
/* -- really weird C code to count the number of bits in a word */
#define BITCOUNT(x)	(((BX_(x)+(BX_(x)>>4)) & 0x0F0F0F0F) % 255)
#define BX_(x)		((x) - (((x)>>1)&0x77777777) \
                             - (((x)>>2)&0x33333333) \
                             - (((x)>>3)&0x11111111))


/*      -------------------------------------------             */


static int
map_color(int grey)
{
  /* Clip it */
  grey = ncolors * abs(grey) / (SPLASH/4);
  if (grey > ncolors)
    grey = ncolors;

  /* Display it */
  return ctab[grey];
}


static void
draw_ripple(short *src)
{
  int across, down;
  char *dirty = dirty_buffer;

  for (down = 0; down < height - 1; down++, src += 1, dirty += 1)
    for (across = 0; across < width - 1; across++, src++, dirty++) {
      int v1, v2, v3, v4;
      v1 = (int)*src;
      v2 = (int)*(src + 1);
      v3 = (int)*(src + width);
      v4 = (int)*(src + width + 1);
      if ((v1 == 0 && v2 == 0 && v3 == 0 && v4 == 0)) {
        if (*dirty > 0)
          (*dirty)--;
      } else
        *dirty = DIRTY;

      if (*dirty > 0) {
        int dx;
        if (light > 0) {
          dx = ((v3 - v1) + (v4 - v2)) << light; /* light from top */
        } else
          dx = 0;
        XPutPixel(buffer_map,(across<<1),  (down<<1),  map_color(dx + v1));
        XPutPixel(buffer_map,(across<<1)+1,(down<<1),  map_color(dx + ((v1 + v2) >> 1)));
        XPutPixel(buffer_map,(across<<1),  (down<<1)+1,map_color(dx + ((v1 + v3) >> 1)));
        XPutPixel(buffer_map,(across<<1)+1,(down<<1)+1,map_color(dx + ((v1 + v4) >> 1)));
      }
    }
}


/*      -------------------------------------------             */


/* Uses the horizontal gradient as an offset to create a warp effect  */
static void
draw_transparent_vanilla(short *src)
{
  int across, down, pixel;
  char *dirty = dirty_buffer;

  pixel = 0;
  for (down = 0; down < height - 2; down++, pixel += 2)
    for (across = 0; across < width-2; across++, pixel++) {
      int gradx, grady, gradx1, grady1;
      int x0, x1, x2, y1, y2;

      x0 = src[pixel];
      x1 = src[pixel + 1];
      x2 = src[pixel + 2];
      y1 = src[pixel + width];
      y2 = src[pixel + 2*width];

      gradx = (x1 - x0);
      grady = (y1 - x0);
      gradx1= (x2 - x1);
      grady1= (y2 - y1);
      gradx1 = 1 + (gradx + gradx1) / 2;
      grady1 = 1 + (grady + grady1) / 2;

      if ((2*across+MIN(gradx,gradx1) < 0) ||
          (2*across+MAX(gradx,gradx1) >= bigwidth)) {
        gradx = 0;
        gradx1= 1;
      }
      if ((2*down+MIN(grady,grady1) < 0) ||
          (2*down+MAX(grady,grady1) >= bigheight)) {
        grady = 0;
        grady1 = 1;
      }

      if ((gradx == 0 && gradx1 == 1 && grady == 0 && grady1 == 1)) {
        if (dirty[pixel] > 0)
          dirty[pixel]--;
      } else
        dirty[pixel] = DIRTY;

      if (dirty[pixel] > 0) {
        XPutPixel(buffer_map, (across<<1),  (down<<1),
                  XGetPixel(orig_map, (across<<1) + gradx, (down<<1) + grady));
        XPutPixel(buffer_map, (across<<1)+1,(down<<1),
                  XGetPixel(orig_map, (across<<1) + gradx1,(down<<1) + grady));
        XPutPixel(buffer_map, (across<<1),  (down<<1)+1,
                  XGetPixel(orig_map, (across<<1) + gradx, (down<<1) + grady1));
        XPutPixel(buffer_map, (across<<1)+1,(down<<1)+1,
                  XGetPixel(orig_map, (across<<1) + gradx1,(down<<1) + grady1));
      }
    }
}


/*      -------------------------------------------             */


/* This builds on the above warp effect by adding in a lighting effect: */
/* brighten pixels by an amount corresponding to the vertical gradient */
static unsigned long rmask;
static unsigned long gmask;
static unsigned long bmask;
static int rshift;
static int gshift;
static int bshift;


static void
set_mask(unsigned long color, unsigned long *mask, int *shift)
{
  *shift = 0;
  while (color != 0 && (color & 1) == 0) {
    (*shift)++;
    color >>= 1;
  }
  *mask = color;
}


static unsigned long
cadd(unsigned long color, int dx, unsigned long mask, int shift)
{
  int x;
  color >>= shift;
  x = (color & mask);
  x += dx;
  if (x < 0) x = 0;
  else if (x > (int)mask) x = mask;
  color = x;
  return color << shift;
}


static unsigned long
bright(int dx, unsigned long color)
{
  return (cadd(color, dx, rmask, rshift) |
          cadd(color, dx, gmask, gshift) |
          cadd(color, dx, bmask, bshift));
}


static void
draw_transparent_light(short *src)
{
  int across, down, pixel;
  char *dirty = dirty_buffer;

  pixel = 0;
  for (down = 0; down < height - 2; down++, pixel += 2)
    for (across = 0; across < width-2; across++, pixel++) {
      int gradx, grady, gradx1, grady1;
      int x0, x1, x2, y1, y2;

      x0 = src[pixel];
      x1 = src[pixel + 1];
      x2 = src[pixel + 2];
      y1 = src[pixel + width];
      y2 = src[pixel + 2*width];

      gradx = (x1 - x0);
      grady = (y1 - x0);
      gradx1= (x2 - x1);
      grady1= (y2 - y1);
      gradx1 = 1 + (gradx + gradx1) / 2;
      grady1 = 1 + (grady + grady1) / 2;

      if ((2*across+MIN(gradx,gradx1) < 0) ||
          (2*across+MAX(gradx,gradx1) >= bigwidth)) {
        gradx = 0;
        gradx1= 1;
      }
      if ((2*down+MIN(grady,grady1) < 0) ||
          (2*down+MAX(grady,grady1) >= bigheight)) {
        grady = 0;
        grady1 = 1;
      }

      if ((gradx == 0 && gradx1 == 1 && grady == 0 && grady1 == 1)) {
        if (dirty[pixel] > 0)
          dirty[pixel]--;
      } else
        dirty[pixel] = DIRTY;

      if (dirty[pixel] > 0) {
        int dx;

        /* light from top */
        if (4-light >= 0)
          dx = (grady + (src[pixel+width+1]-x1)) >> (4-light);
        else
          dx = (grady + (src[pixel+width+1]-x1)) << (light-4);

        if (dx != 0) {
          XPutPixel(buffer_map, (across<<1),  (down<<1),
                    bright(dx, XGetPixel(orig_map, (across<<1) + gradx, (down<<1) + grady)));
          XPutPixel(buffer_map, (across<<1)+1,(down<<1),
                    bright(dx, XGetPixel(orig_map, (across<<1) + gradx1,(down<<1) + grady)));
          XPutPixel(buffer_map, (across<<1),  (down<<1)+1,
                    bright(dx, XGetPixel(orig_map, (across<<1) + gradx, (down<<1) + grady1)));
          XPutPixel(buffer_map, (across<<1)+1,(down<<1)+1,
                    bright(dx, XGetPixel(orig_map, (across<<1) + gradx1,(down<<1) + grady1)));
        } else {
          /* Could use XCopyArea, but XPutPixel is faster */
          XPutPixel(buffer_map, (across<<1),  (down<<1),
                    XGetPixel(orig_map, (across<<1) + gradx, (down<<1) + grady));
          XPutPixel(buffer_map, (across<<1)+1,(down<<1),
                    XGetPixel(orig_map, (across<<1) + gradx1,(down<<1) + grady));
          XPutPixel(buffer_map, (across<<1),  (down<<1)+1,
                    XGetPixel(orig_map, (across<<1) + gradx, (down<<1) + grady1));
          XPutPixel(buffer_map, (across<<1)+1,(down<<1)+1,
                    XGetPixel(orig_map, (across<<1) + gradx1,(down<<1) + grady1));
        }
      }
    }
}


/*      -------------------------------------------             */


#if 0
/* Doesn't go any faster and doesn't work at all colour depths */
static void
draw_transparent16l(short *src)
{
  int across, down, bigpix, pixel;
  char *dirty = dirty_buffer;
  unsigned short *buffer, *orig;

  buffer = (unsigned short *) buffer_map->data;
  orig = (unsigned short *) orig_map->data;

  for (pixel = bigpix = down = 0;
       down < height - 2;
       down++, pixel += 2, bigpix += bigwidth+4)
    for (across = 0; across < width-2; across++, pixel++, bigpix+=2) {
      int gradx, grady, gradx1, grady1;
      int x0, x1, x2, y1, y2;

      x0 = src[pixel];
      x1 = src[pixel + 1];
      x2 = src[pixel + 2];
      y1 = src[pixel + width];
      y2 = src[pixel + 2*width];

      gradx = (x1 - x0);
      grady = (y1 - x0);
      gradx1= (x2 - x1);
      grady1= (y2 - y1);
      gradx1 = 1 + (gradx + gradx1) / 2;
      grady1 = 1 + (grady + grady1) / 2;

      if ((2*across+MIN(gradx,gradx1) < 0) ||
          (2*across+MAX(gradx,gradx1) >= bigwidth)) {
        gradx = 0;
        gradx1= 1;
      }
      if ((2*down+MIN(grady,grady1) < 0) ||
          (2*down+MAX(grady,grady1) >= bigheight)) {
        grady = 0;
        grady1 = 1;
      }

      if ((gradx == 0 && gradx1 == 1 && grady == 0 && grady1 == 1)) {
        if (dirty[pixel] > 0)
          dirty[pixel]--;
      } else
        dirty[pixel] = DIRTY;

      if (dirty[pixel] > 0) {
        unsigned short *dest = buffer + bigpix;
        unsigned short *image = orig + bigpix;
        int dx;

        /* light from top */
        if (4-light >= 0)
          dx = (grady + (src[pixel+width+1]-x1)) >> (4-light);
        else
          dx = (grady + (src[pixel+width+1]-x1)) << (light-4);

        grady *= bigwidth;
        grady1*= bigwidth;

        if (dx != 0) {
          *dest++ = dobright(dx, *(image + gradx  + grady));
          *dest   = dobright(dx, *(image + gradx1 + grady));
          dest   += bigwidth - 1;
          *dest++ = dobright(dx, *(image + gradx  + grady1));
          *dest   = dobright(dx, *(image + gradx1 + grady1));
        } else {
          *dest++ = *(image + gradx  + grady);
          *dest   = *(image + gradx1 + grady);
          dest   += bigwidth - 1;
          *dest++ = *(image + gradx  + grady1);
          *dest   = *(image + gradx1 + grady1);
        }
      }
    }
}
#endif


/*      -------------------------------------------             */


static void
setup_X(Display * disp, Window win)
{
  XWindowAttributes xwa;
  int depth;

  XGetWindowAttributes(disp, win, &xwa);
  window = win;
  display = disp;
  depth = xwa.depth;
  colormap = xwa.colormap;
  bigwidth = xwa.width;
  bigheight = xwa.height;
  visual = xwa.visual;


  /* This causes buffer_map to be 1 pixel taller and wider than orig_map,
     which can cause the two XImages to have different bytes-per-line,
     which causes stair-stepping.  So this better not be necessary...
     -jwz, 23-Nov-01
   */
#if 0 /* I'm not entirely sure if I need this */
  if (bigwidth % 2)
    bigwidth++;
  if (bigheight % 2)
    bigheight++;
#endif


  width = bigwidth / 2;
  height = bigheight / 2;

  if (transparent) {
    XGCValues gcv;
    long gcflags;

    gcv.function = GXcopy;
    gcv.subwindow_mode = IncludeInferiors;

    gcflags = GCForeground | GCFunction;
    if (use_subwindow_mode_p(xwa.screen, window))	/* see grabscreen.c */
      gcflags |= GCSubwindowMode;

    gc = XCreateGC(display, window, gcflags, &gcv);

    load_random_image (xwa.screen, window, window, NULL);

    orig_map = XGetImage(display, window, 0, 0, xwa.width, xwa.height,
			 ~0L, ZPixmap);
  } else {
    XGCValues gcv;

    gc = XCreateGC(display, window, 0, &gcv);
    orig_map = 0;
  }

  if (!gc) {
    fprintf(stderr, "XCreateGC failed\n");
    exit(1);
  }

  buffer_map = 0;

#ifdef HAVE_XSHM_EXTENSION
  if (use_shm) {
    buffer_map = create_xshm_image(display, xwa.visual, depth,
				   ZPixmap, 0, &shm_info, bigwidth, bigheight);
    if (!buffer_map) {
      use_shm = False;
      fprintf(stderr, "create_xshm_image failed\n");
    }
  }
#endif /* HAVE_XSHM_EXTENSION */

  if (!buffer_map) {
    buffer_map = XCreateImage(display, xwa.visual,
			      depth, ZPixmap, 0, 0,
			      bigwidth, bigheight, 8, 0);
    buffer_map->data = (char *)
      calloc(buffer_map->height, buffer_map->bytes_per_line);
  }
}


static void
DisplayImage(void)
{
#ifdef HAVE_XSHM_EXTENSION
  if (use_shm)
    XShmPutImage(display, window, gc, buffer_map, 0, 0, 0, 0,
		 bigwidth, bigheight, False);
  else
#endif /* HAVE_XSHM_EXTENSION */
    XPutImage(display, window, gc, buffer_map, 0, 0, 0, 0,
	      bigwidth, bigheight);

  XSync(display,False);
  screenhack_handle_events(display);
}


/*      -------------------------------------------             */


static int
cinterp(double a, int bg, int fg)
{
  int result;
  result = (int)((1-a) * bg + a * fg + 0.5);
  if (result < 0) result = 0;
  if (result > 255) result = 255;
  return result;
}


/* Interpolate the ripple colours between the background colour and
   foreground colour */
static void
init_linear_colors(void)
{
  int i, j, red, green, blue, bred, bgreen, bblue;
  XColor fg, bg;

  if (ncolors < 2 || mono_p)
    ncolors = 2;
  if (ncolors <= 2)
    mono_p = True;

  /* Make it possible to set the color of the ripples,
     Based on work by Raymond Medeiros <ray@stommel.marine.usf.edu> and jwz.
   */
  fg.pixel = get_pixel_resource("foreground", "Foreground",
				display, colormap);
  XQueryColor(display, colormap, &fg);
  red = (fg.red >> 8);
  green = (fg.green >> 8);
  blue = (fg.blue >> 8);

  bg.pixel = get_pixel_resource("background", "Background",
				display, colormap);
  XQueryColor(display, colormap, &bg);
  bred = (bg.red >> 8);
  bgreen = (bg.green >> 8);
  bblue = (bg.blue >> 8);

  j = 0;
  for (i = 0; i < ncolors+1; i++) {
    XColor xcl;
    double a = (double)i / ncolors;
    int r = cinterp(a, bred, red);
    int g = cinterp(a, bgreen, green);
    int b = cinterp(a, bblue, blue);

    xcl.red = (unsigned short) ((r << 8) | r);
    xcl.green = (unsigned short) ((g << 8) | g);
    xcl.blue = (unsigned short) ((b << 8) | b);
    xcl.flags = DoRed | DoGreen | DoBlue;

    XAllocColor(display, colormap, &xcl);

    ctab[j++] = (int) xcl.pixel;
  }
}


static void
init_oily_colors(void)
{
  XColor *colors = NULL;

  if (ncolors < 2 || mono_p)
    ncolors = 2;
  if (ncolors <= 2)
    mono_p = True;
  colors = 0;

  if (!mono_p) {
    colors = (XColor *)malloc(sizeof(*colors) * (ncolors+1));
    make_smooth_colormap(display, visual, colormap, colors, &ncolors,
                         True, /* allocate */
                         False, /* not writable */
                         True); /* verbose (complain about failure) */
    if (ncolors <= 2) {
      if (colors)
        free (colors);
      colors = 0;
      mono_p = True;
    }
  }
  if (!mono_p) {
    int i, j = 0;
    for (i = 0; i < ncolors+1; i++) {
      XAllocColor(display, colormap, colors+i);
      ctab[j++] = (int) colors[i].pixel;
    }
    free (colors);
  } else {
    ncolors = 2;
    ctab[1] = get_pixel_resource("foreground", "Foreground", display, colormap);
    ctab[0] = get_pixel_resource("background", "Background", display, colormap);
  }
}


/*      -------------------------------------------             */


static void
init_cos_tab(void)
{
  int i;
  for (i = 0; i < TABLE; i++)
    cos_tab[i] = cos(i * M_PI/2 / TABLE);
}


/* Shape of drop to add */
static double
sinc(double x)
{
#if 1
  /* cosine hump */
  int i;
  i = (int)(x * TABLE + 0.5);
  if (i >= TABLE) i = (TABLE-1) - (i-(TABLE-1));
  return cos_tab[i];
#elif 0
  return cos(x * M_PI/2);
#else
  if (fabs(x) < 0.1)
    return 1 - x*x;
  else
    return sin(x) / x;
#endif
}


static void
add_circle_drop(int x, int y, int radius, int dheight)
{
  int i, r2, cx, cy;
  short *buf = (random()&1) ? bufferA : bufferB;

  i = y * width + x;
  r2 = radius * radius;

  for (cy = -radius; cy <= radius; cy++)
    for (cx = -radius; cx <= radius; cx++) {
      int r = cx*cx + cy*cy;
      if (r <= r2) {
        buf[i + cx + cy*width] =
          (short)(dheight * sinc(sqrt(r)/radius));
      }
    }
}


static void
add_drop(ripple_mode mode, int drop)
{
  int newx, newy, dheight;
  int radius = MIN(width, height) / 50;
  /* Don't put drops too near the edge of the screen or they get stuck */
  int border = 8;

  switch (mode) {
  default:
  case ripple_drop: {
    int x;

    dheight = 1 + (random() % drop);
    newx = border + (random() % (width - 2*border));
    newy = border + (random() % (height - 2*border));
    x = newy * width + newx;
    bufferA[x + 1] = bufferA[x] = bufferA[x + width] = bufferA[x + width + 1] =
      bufferB[x + 1] = bufferB[x] = bufferB[x + width] = bufferB[x + width + 1] =
      dheight;
  }
  break;
  case ripple_blob: {
    double power;

    power = drop_dist[random() % (sizeof(drop_dist)/sizeof(drop_dist[0]))]; /* clumsy */
    dheight = (int)(drop * (power + 0.01));
    newx = radius + border + (random() % (int)(width - 2*border - 2*radius*power));
    newy = radius + border + (random() % (int)(height - 2*border - 2*radius*power));
    add_circle_drop(newx, newy, radius, dheight);
  }
  break;
  /* Adding too many boxes too quickly (-box 1) doesn't give the waves time
     to disperse and the waves build up (and overflow) */
  case ripple_box: {
    int x;
    int cx, cy;
    short *buf = (random()&1) ? bufferA : bufferB;

    radius = (1 + (random() % 5)) * (1 + (random() % 5));
    dheight = drop / 128;
    if (random() & 1) dheight = -dheight;
    newx = radius + border + (random() % (width - 2*border - 2*radius));
    newy = radius + border + (random() % (height - 2*border - 2*radius));
    x = newy * width + newx;
    for (cy = -radius; cy <= radius; cy++)
      for (cx = -radius; cx <= radius; cx++)
        buf[x + cx + cy*width] = (short)(dheight);
  }
  break;
  case ripple_stir: {
    static double ang = 0;
    border += radius;
    newx = border + (int)((width-2*border) * (1+cos(3*ang)) / 2);
    newy = border + (int)((height-2*border) * (1+sin(2*ang)) / 2);
    add_circle_drop(newx, newy, radius, drop / 10);
    ang += 0.02;
    if (ang > 12*M_PI) ang = 0;
  }
  break;
  }
}


static void
init_ripples(int ndrops, int splash)
{
  int i;

  bufferA = (short *)calloc(width * height, sizeof(*bufferA));
  bufferB = (short *)calloc(width * height, sizeof(*bufferB));
  temp = (short *)calloc(width * height, sizeof(*temp));

  dirty_buffer = (char *)calloc(width * height, sizeof(*dirty_buffer));

  for (i = 0; i < ndrops; i++)
    add_drop(ripple_blob, splash);

  if (transparent) {
    /* There's got to be a better way of doing this  XCopyArea? */
    memcpy(buffer_map->data, orig_map->data,
           bigheight * buffer_map->bytes_per_line);
  } else {
    int across, down, color;

    color = map_color(0); /* background colour */
    for (down = 0; down < bigheight; down++)
      for (across = 0; across < bigwidth; across++)
        XPutPixel(buffer_map,across,  down,  color);
  }

  DisplayImage();
}


/*
 Explanation from hq_water.zip (Arturo R Montesinos (ARM) arami@fi.upm.es)

 Differential equation is:  u  = a ( u  + u  )
                             tt       xx   yy

 Where a = tension * gravity / surface_density.

 Approximating second derivatives by central differences:

  [ u(t+1)-2u(t)+u(t-1) ] / dt = a [ u(x+1)+u(x-1)+u(y+1)+u(y-1)-4u ] / h

 where dt = time step squared, h = dx*dy = mesh resolution squared.

 From where u(t+1) may be calculated as:

            dt  |   1   |                   dt
 u(t+1) = a --  | 1 0 1 |u - u(t-1) + (2-4a --)u
            h   |   1   |                    h

 When a*dt/h = 1/2 last term vanishes, giving:

                 1 |   1   |
        u(t+1) = - | 1 0 1 |u - u(t-1)
                 2 |   1   |

 (note that u(t-1,x,y) is only used to calculate u(t+1,x,y) so
  we can use the same array for both t-1 and t+1, needing only
  two arrays, U[0] and U[1])

 Dampening is simulated by subtracting 1/2^n of result.
 n=4 gives best-looking result
 n<4 (eg 2 or 3) thicker consistency, waves die out immediately
 n>4 (eg 8 or 12) more fluid, waves die out slowly
 */

static void
ripple(int fluidity)
{
  int across, down, pixel;
  static int toggle;
  static int count;
  short *src, *dest;

  if (toggle == 0) {
    src = bufferA;
    dest = bufferB;
    toggle = 1;
  } else {
    src = bufferB;
    dest = bufferA;
    toggle = 0;
  }

  switch (count) {
  case 0: case 1:
    pixel = 1 * width + 1;
    for (down = 1; down < height - 1; down++, pixel += 2 * 1)
      for (across = 1; across < width - 1; across++, pixel++) {
        temp[pixel] =
          (((src[pixel - 1] + src[pixel + 1] +
             src[pixel - width] + src[pixel + width]) / 2)) - dest[pixel];
      }

    /* Smooth the output */
    pixel = 1 * width + 1;
    for (down = 1; down < height - 1; down++, pixel += 2 * 1)
      for (across = 1; across < width - 1; across++, pixel++) {
        if (temp[pixel] != 0) { /* Close enough for government work */
          int damp =
            (temp[pixel - 1] + temp[pixel + 1] +
             temp[pixel - width] + temp[pixel + width] +
             temp[pixel - width - 1] + temp[pixel - width + 1] +
             temp[pixel + width - 1] + temp[pixel + width + 1] +
             temp[pixel]) / 9;
          dest[pixel] = damp - (damp >> fluidity);
        } else
          dest[pixel] = 0;
      }
    break;
  case 2: case 3:
    pixel = 1 * width + 1;
    for (down = 1; down < height - 1; down++, pixel += 2 * 1)
      for (across = 1; across < width - 1; across++, pixel++) {
        int damp =
          (((src[pixel - 1] + src[pixel + 1] +
             src[pixel - width] + src[pixel + width]) / 2)) - dest[pixel];
        dest[pixel] = damp - (damp >> fluidity);
      }
    break;
  }
  if (++count > 3) count = 0;

  if (transparent)
    draw_transparent(dest);
  else
    draw_ripple(dest);
}


/*      -------------------------------------------             */


char *progclass = "Ripples";

char *defaults[] =
{
  ".background:		black",
  ".foreground:		#FFAF5F",
  "*colors:		200",
  "*dontClearRoot:	True",
  "*delay:		50000",
  "*rate:	 	5",
  "*box:	 	0",
  "*water: 		False",
  "*oily: 		False",
  "*stir: 		False",
  "*fluidity: 		6",
  "*light: 		0",
#ifdef HAVE_XSHM_EXTENSION
  "*useSHM: True",
#endif				/* HAVE_XSHM_EXTENSION */
  0
};

XrmOptionDescRec options[] =
{
  { "-colors",	".colors",	XrmoptionSepArg, 0},
  { "-colours",	".colors",	XrmoptionSepArg, 0},
  {"-delay",	".delay",	XrmoptionSepArg, 0},
  {"-rate",	".rate",	XrmoptionSepArg, 0},
  {"-box",	".box",		XrmoptionSepArg, 0},
  {"-water",	".water",	XrmoptionNoArg, "True"},
  {"-oily",	".oily",	XrmoptionNoArg, "True"},
  {"-stir",	".stir",	XrmoptionNoArg, "True"},
  {"-fluidity",	".fluidity",	XrmoptionSepArg, 0},
  {"-light",	".light",	XrmoptionSepArg, 0},
#ifdef HAVE_XSHM_EXTENSION
  {"-shm",	".useSHM",	XrmoptionNoArg, "True"},
  {"-no-shm",	".useSHM",	XrmoptionNoArg, "False"},
#endif				/* HAVE_XSHM_EXTENSION */
  {0, 0, 0, 0}
};


void screenhack(Display *disp, Window win)
{
  int iterations = 0;
  int delay = get_integer_resource("delay", "Integer");
  int rate = get_integer_resource("rate", "Integer");
  int box = get_integer_resource("box", "Integer");
  int oily = get_boolean_resource("oily", "Boolean");
  int stir = get_boolean_resource("stir", "Boolean");
  int fluidity = get_integer_resource("fluidity", "Integer");
  transparent = get_boolean_resource("water", "Boolean");
#ifdef HAVE_XSHM_EXTENSION
  use_shm = get_boolean_resource("useSHM", "Boolean");
#endif /* HAVE_XSHM_EXTENSION */
  light = get_integer_resource("light", "Integer");

  if (fluidity <= 1) fluidity = 1;
  if (fluidity > 16) fluidity = 16; /* 16 = sizeof(short) */
  if (light < 0) light = 0;

  init_cos_tab();
  setup_X(disp, win);

  ncolors = get_integer_resource ("colors", "Colors");
  if (0 == ncolors)		/* English spelling? */
    ncolors = get_integer_resource ("colours", "Colors");

  if (ncolors > sizeof(ctab)/sizeof(*ctab))
    ncolors = sizeof(ctab)/sizeof(*ctab);

  if (oily)
    init_oily_colors();
  else
    init_linear_colors();

  if (transparent && light > 0) {
    int maxbits;
    draw_transparent = draw_transparent_light;
    set_mask(visual->red_mask,   &rmask, &rshift);
    set_mask(visual->green_mask, &gmask, &gshift);
    set_mask(visual->blue_mask,  &bmask, &bshift);
    if (rmask == 0) draw_transparent = draw_transparent_vanilla;

    /* Adjust the shift value "light" when we don't have 8 bits per colour */
    maxbits = MIN(MIN(BITCOUNT(rmask), BITCOUNT(gmask)), BITCOUNT(bmask));
    light -= 8-maxbits;
    if (light < 0) light = 0;
  } else
    draw_transparent = draw_transparent_vanilla;

  init_ripples(0, -SPLASH); /* Start off without any drops */

  /* Main drawing loop */
  while (1) {
    if (rate > 0 && (iterations % rate) == 0)
      add_drop(ripple_blob, -SPLASH);
    if (stir)
      add_drop(ripple_stir, -SPLASH);
    if (box > 0 && (random() % box) == 0)
      add_drop(ripple_box, -SPLASH);

    ripple(fluidity);
    DisplayImage();

    if (delay)
      usleep(delay);

    iterations++;
  }
}
